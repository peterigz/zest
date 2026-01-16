#define ZEST_IMPLEMENTATION
#define ZEST_VULKAN_IMPLEMENTATION
#define ZEST_ALL_UTILITIES_IMPLEMENTATION
#include "zest-timelinefx-prerecorded-effects.h"
#include "zest.h"
#include "imgui_internal.h"

/*
Step 1:	Register a Computer with RegisterComputer.
Step 2: Create any buffers that we need for the compute shader.
Step 3:	Use ComputeBuilder to set up all the things we need for a compute shader:
Step 4: Use MakeCompute with the ComputeBuilder we setup to finish the compute setup
*/

#define MAX_INSTANCES 1024
#define MAX_SPRITES	250000

#define BufferCast(struct_name, buffer_name) static_cast<struct_name*>(buffer_name.mapped)

//---Render specific functions---
//The following code will depend on the renderer that you're using

//Send the command to the GPU to draw all of the sprites for all of the animation instances that are currently in play
void RecordComputeSprites(zest_command_list command_list, void *user_data) {
	tfxPrerecordedExample *example = static_cast<tfxPrerecordedExample *>(user_data);

	zest_buffer sprite_buffer = zest_GetPassOutputBuffer(command_list, "sprite buffer");

	zest_cmd_SetScreenSizedViewport(command_list, 0, 1);

	//Bind the buffer that contains the sprite instances to draw. These are updated by the compute shader on the GPU
	zest_cmd_BindVertexBuffer(command_list, 0, 1, sprite_buffer);

	zest_pipeline pipeline = zest_GetPipeline(example->tfx_rendering.pipeline, command_list);

	//Draw all the sprites in the buffer that is built by the compute shader
	zest_cmd_BindPipeline(command_list, pipeline);
	zest_cmd_Draw(command_list, 6, tfx_GetTotalSpritesThatNeedDrawing(example->animation_manager_3d), 0, 0);
}

//Upload the buffers to the GPU that the compute shader will need to read from in order to build the sprite buffer each frame. These buffers
//only need to be uploaded once once you've added all the sprite data for all the animations that you want to use.
void UploadBuffers(tfxPrerecordedExample *example) {

	tfx_animation_manager animation_manager = example->animation_manager_3d;

	zest_BeginImmediateCommandBuffer(example->context);

	//Upload the sprite data. This contains all pre-recorded sprites for all animations that you might want to play.
	zest_buffer staging_buffer = zest_CreateStagingBuffer(example->context, tfx_GetSpriteDataSizeInBytes(animation_manager), tfx_GetSpriteDataBufferPointer(animation_manager));
	zest_imm_CopyBuffer(example->context, staging_buffer, example->sprite_data_buffer, tfx_GetSpriteDataSizeInBytes(animation_manager));
	zest_FreeBuffer(staging_buffer);

	//Upload the emitter properties for each animation you're using. This contains the sprite handle and any flags that might be relavent.
	staging_buffer = zest_CreateStagingBuffer(example->context, tfx_GetAnimationEmitterPropertySizeInBytes(animation_manager), tfx_GetAnimationEmitterPropertiesBufferPointer(animation_manager));
	zest_imm_CopyBuffer(example->context, staging_buffer, example->emitter_properties_buffer, tfx_GetAnimationEmitterPropertySizeInBytes(animation_manager));
	zest_FreeBuffer(staging_buffer);

	//Upload the image data containing all the UV coords and texture array index that the compute shader will use to build the sprite
	//buffer each frame
	staging_buffer = zest_CreateStagingBuffer(example->context, tfx_GetGPUShapesSizeInBytes(example->gpu_image_data), tfx_GetGPUShapesArray(example->gpu_image_data));
	zest_imm_CopyBuffer(example->context, staging_buffer, example->image_data_buffer, tfx_GetGPUShapesSizeInBytes(example->gpu_image_data));
	zest_FreeBuffer(staging_buffer);

	zest_EndImmediateCommandBuffer(example->context);
}

//Prepare the compute shader that will be used to playback the effects in the animation manager
void PrepareComputeForEffectPlayback(tfxPrerecordedExample *example) {
	//Register a new compute shader
	example->playback_shader = zest_CreateShaderFromSPVFile(example->device, "examples/assets/spv/sprite_data_playback.comp.spv", zest_compute_shader);

	//Utilize a ComputeBuilder to make setting up the compute shader a lot easier
	zest_compute_builder_t builder = zest_BeginComputeBuilder(example->device);
	//Specify the shader that we want to use for the compute shader
	zest_AddComputeShader(&builder, example->playback_shader);
	//Finalise the compute set up by calling MakeCompute which creates all the boiler plate code to create the
	//compute pipeline in Vulkan
	example->compute_pipeline_3d = zest_FinishCompute(&builder, "TFX Playback");
}

//Every frame the compute shader needs to be dispatched which means that all the commands for the compute shader
//need to be added to the command buffer
void SpriteComputeFunction(zest_command_list command_list, void *user_data) {

	tfxPrerecordedExample *example = static_cast<tfxPrerecordedExample *>(user_data);
	//The compute queue item can contain more then one compute shader to be dispatched but in this case there's only
	//one which is the effect playback compute shader
	zest_compute compute = zest_GetCompute(example->compute_pipeline_3d);
	//Grab our tfxPrerecordedExample struct out of the user data

	//We'll need the animation metrics to tell the compute shader how many animation instances we're rendering this frame
	auto metrics = tfx_GetAnimationBufferMetrics(example->animation_manager_3d);
	if (metrics.total_sprites_to_draw == 0) {
		return;
	}

	zest_context context = zest_GetContext(command_list);
	zest_uint fif = zest_CurrentFIF(context);

	zest_buffer offsets_buffer = zest_GetPassOutputBuffer(command_list, "offsets buffer");
	zest_buffer animation_instances_buffer = zest_GetPassOutputBuffer(command_list, "offsets buffer");

	//Bind the compute shader pipeline
	zest_cmd_BindComputePipeline(command_list, compute);
	//Some graphics cards don't support direct writing to the GPU buffer so we have to copy to a staging buffer first, then
	//from there we copy to the GPU.
	zest_cmd_CopyBuffer(command_list, example->offsets_staging_buffer[fif], offsets_buffer, tfx_GetOffsetsSizeInBytes(example->animation_manager_3d));
	zest_cmd_CopyBuffer(command_list, example->animation_instances_staging_buffer[fif], animation_instances_buffer, tfx_GetAnimationInstancesSizeInBytes(example->animation_manager_3d));

	ComputePushConstants compute_constants;
	//Update the push constants with some metrics. These are referenced in the compute shader.
	//The total number of animation instances that need to be drawn
	compute_constants.instances_size = (float)metrics.instances_size;
	//If any animation instance contains animated shapes then set to 1. The compute shader can use this to avoid some unecessary
	//computation if all particle shapes are not animated
	compute_constants.has_animated_shapes = float(example->animation_manager_3d->flags & tfxAnimationManagerFlags_has_animated_shapes);
	//Set the total number of sprites that need to be processed by the shader
	compute_constants.total_sprites_to_draw = (float)metrics.total_sprites_to_draw;
	//Set the offset into the sprite data that the sprites start at
	compute_constants.sprite_data_offset = (float)example->animation_manager_3d->render_queue[0].offset_into_sprite_data;

	//Send the push constants in the compute object to the shader
	zest_cmd_SendPushConstants(command_list, &compute_constants, sizeof(ComputePushConstants));

	//The 128 here refers to the local_size_x in the shader and is how many elements each group will work on
	//For example if there are 1024 sprites, if we divide by 128 there will be 8 groups working on 128 sprites each in parallel
	zest_cmd_DispatchCompute(command_list, (metrics.total_sprites_to_draw / 128) + 1, 1, 1);

}
//----End of render specific functions-----

void UpdateTimelineFXImageData(tfx_render_resources_t &tfx_rendering, tfx_gpu_shapes gpu_shapes) {
	//Upload the timelinefx image data to the image data buffer created
	zest_buffer staging_buffer = zest_CreateStagingBuffer(tfx_GetGPUShapesSizeInBytes(gpu_shapes), tfx_GetGPUShapesArray(gpu_shapes));
	zest_cmd_CopyBuffer(staging_buffer, tfx_rendering.image_data, tfx_GetGPUShapesSizeInBytes(gpu_shapes));
	zest_FreeBuffer(staging_buffer);
}

//Initialise the example and create all the necessary buffers and objects for the compute shaders and load in the effects library
//and prepare the effect we'll use as an example.
void InitExample(tfxPrerecordedExample *example) {
	zest_tfx_InitTimelineFXRenderResources(example->device, example->context, &example->tfx_rendering, "examples/assets/instanced_effects.tfx");

	zest_sampler_info_t sampler = zest_CreateSamplerInfo();
	example->sampler = zest_CreateSampler(example->context, &sampler);
	example->sampler_index = zest_AcquireSamplerIndex(example->device, zest_GetSampler(example->sampler));

	int width, height, channels;
	stbi_uc *pixels = stbi_load("examples/assets/checker.png", &width, &height, &channels, 4);
	int size = width * height * 4;
	zest_image_info_t image_info = zest_CreateImageInfo(width, height);
	image_info.format = zest_format_r8g8b8a8_unorm;
	image_info.flags = zest_image_preset_texture_mipmaps;
	example->floor_texture = zest_CreateImageWithPixels(example->context, pixels, size, &image_info);
	STBI_FREE(pixels);
	zest_image image = zest_GetImage(example->floor_texture);
	example->floor_image = zest_NewAtlasRegion();
	example->floor_image.width = width;
	example->floor_image.height = height;
	zest_AcquireSampledImageIndex(example->device, image, zest_texture_2d_binding);
	zest_BindAtlasRegionToImage(&example->floor_image, example->sampler_index, image, zest_texture_2d_binding);

	//Render specific - set up a camera
	example->camera = zest_CreateCamera();
	zest_CameraPosition(&example->camera, { 0.f, 0.f, 0.f });
	zest_CameraSetFoV(&example->camera, 60.f);
	zest_CameraSetYaw(&example->camera, zest_Radians(-90.f));
	zest_CameraSetPitch(&example->camera, zest_Radians(0.f));
	zest_CameraUpdateFront(&example->camera);

	//When we record an effect we can specify the camera position which only really makes sense if you're recording an effect
	//that has depth ordering, otherwise it won't matter much
	tfx_vec3_t camera_position_for_recording = tfx_vec3_t(example->camera.front.x, example->camera.front.y, example->camera.front.z);

	//Initialise an animation manager. An animation manager maintains our animation instances for us and provides all the necessary metrics we'll
	//need to upload the buffers we need to both to initialise the compute shader and when we upload the offsets and instances buffer each frame.
	//Specify the maximum number of animation instances that you might want to play each frame
	example->animation_manager_3d = tfx_CreateAnimationManager(MAX_INSTANCES, MAX_SPRITES);
	//Here we set the callback that will be used to decide if an animation should be drawn or not. We use the bounding box to check if it's inside
	//the view frustum and cull it if it's not.
	example->animation_manager_3d->maybe_render_instance_callback = CullAnimationInstancesCallback;
	//Set the user data to the tfxPrerecordedExample which we can use in the callback function
	tfx_SetAnimationManagerUserData(example->animation_manager_3d, example);

	//Create the handle where we will store gpu shapes for uploading to the gpu
	example->gpu_image_data = tfx_CreateGPUShapesList();

	//Record the effects we want and calculate the bounding boxes
	example->record_time = zest_Millisecs();		//See how long it takes to record.

	//Load the effects library and pass our shape loader callback function which loads the particle shape images into our texture
	tfxErrorFlags result = tfx_LoadSpriteData("examples/assets/prerecorded_effects.tfxsd", example->animation_manager_3d, zest_tfx_ShapeLoader, example);
	//ListEffectNames(&example->library);
	assert(result == tfxErrorCode_success);		//Unable to load the effects library!

	//Add the color ramps from the library to the color ramps texture. Color ramps in the library are stored in rgba format and can be
	//simply copied to a bitmap for uploading to the texture
	example->tfx_rendering.color_ramps_collection = zest_CreateImageAtlasCollection(zest_format_r8g8b8a8_unorm, example->animation_manager_3d->color_ramps.color_ramp_bitmaps.size());
	for (tfx_bitmap_t &bitmap : example->animation_manager_3d->color_ramps.color_ramp_bitmaps) {
		zest_AddImageAtlasPixels(&example->tfx_rendering.color_ramps_collection, bitmap.data, bitmap.size, bitmap.width, bitmap.height, zest_format_r8g8b8a8_unorm);
	}
	//Process the color ramp texture to upload it all to the gpu
	example->tfx_rendering.color_ramps_texture = zest_CreateImageAtlas(example->context, &example->tfx_rendering.color_ramps_collection, 256, 256, zest_image_preset_texture);
	zest_image color_ramps_image = zest_GetImage(example->tfx_rendering.color_ramps_texture);
	example->tfx_rendering.color_ramps_index = zest_AcquireSampledImageIndex(example->device, color_ramps_image, zest_texture_array_binding);
	//Build the gpu image data. Pass the GetUV function which you will have to create based on your renderer.
	tfx_BuildAnimationManagerGPUShapeData(example->animation_manager_3d, example->gpu_image_data, GetUV);
	//Update the image data to the gpu
	UpdateTimelineFXImageData(example->tfx_rendering, example->gpu_image_data);

	//Now upload the image data to the GPU and set up the shader resources ready for rendering
	zest_tfx_UpdateTimelineFXImageData(example->context, &example->tfx_rendering, library);

	zest_buffer_info_t buffer_info = zest_CreateBufferInfo(zest_buffer_type_storage, zest_memory_usage_gpu_only);

	//Render Specific - Create the 6 buffers needed for the compute shader. Create these after you've added all the effect animations you want to use
	//to the animation manager.
	/*
	1)	Sprite data buffer for all sprites in all frames of the animation. Use tfx_GetTotalSpriteDataCount() and/or tfx_GetSpriteDataSizeInBytes() to get the
		metrics you need to create the buffer. Note that we've already recorded the sprite data so we know the size of the buffer we need, otherwise
		you might have to resize the buffer later and update the descriptor sets
	*/
	example->sprite_data_buffer = zest_CreateBuffer(example->context, sizeof(tfx_sprite_data3d_t) * tfx_GetTotalSpriteDataCount(example->animation_manager_3d), &buffer_info);

	/*
	5)	Image data buffer contains data about all of the shapes being used in the library. Use GetComputeShapeCount and GetComputeShapeSizeInBytes to get the metrics you need
		to create the buffer.
	*/
	example->image_data_buffer = zest_CreateBuffer(example->context, sizeof(tfx_gpu_image_data_t) * 1000, &buffer_info);

	/*
	6)	Emitter properties contains data about each emitter used to create the animation that the compute shader uses to build the sprite buffer.
		Use GetAnimationEmitterPropertyCount and tfx_GetAnimationEmitterPropertySizeInBytes to get the metrics you need to create the buffer.
	*/
	example->emitter_properties_buffer = zest_CreateBuffer(example->context, sizeof(tfx_animation_emitter_properties_t) * 100, &buffer_info);

	example->bounding_boxes = zest_CreateBuffer(example->context, sizeof(tfx_bounding_box_t) * 200, &buffer_info);

	//Prepare the compute shaders for effect playback and bounding box calculation
	PrepareComputeForEffectPlayback(example);

	//Now that the sprite data is prepared we can Upload the buffers to the GPU. These are the buffers that we only need to upload once before we do anything
	//They will containt the sprite, image and emitter data that the compute shader will index into to build each frame
	UploadBuffers(example);

	example->record_time = zest_Millisecs() - example->record_time;	//note how long the above took

	//Initialise Dear ImGui
	zest_imgui_Initialise(example->context, &example->imgui, zest_implglfw_DestroyWindow);

	//Render specific - Set up the callback for updating the uniform buffers containing the model and view matrices
	zest_tfx_UpdateUniformBuffer(example->context, &example->tfx_rendering);

	//Set up a timer
	example->timer = zest_CreateTimer(60);
	example->random = tfx_NewRandom(zest_Millisecs());
}

//Fuction to cast a ray from screen to world space.
tfx_vec3_t ScreenRay(tfxPrerecordedExample *example, float x, float y, float depth_offset, zest_vec3 &camera_position) {
	zest_uniform_buffer uniform_buffer = zest_GetUniformBuffer(example->tfx_rendering.uniform_buffer);
	zest_uniform_buffer_data_t *ubo_ptr = static_cast<zest_uniform_buffer_data_t *>(zest_GetUniformBufferData(uniform_buffer));
	zest_vec3 camera_last_ray = zest_ScreenRay(x, y, zest_ScreenWidthf(context), zest_ScreenHeightf(context), &ubo_ptr->proj, &ubo_ptr->view);
	zest_vec3 pos = zest_AddVec3(zest_ScaleVec3(camera_last_ray, depth_offset), camera_position);
	return { pos.x, pos.y, pos.z };
}

void BuildUI(tfxPrerecordedExample *example) {
	//Must call the imgui GLFW implementation function
	ImGui_ImplGlfw_NewFrame();
	//Draw our imgui stuff
	ImGui::NewFrame();
	ImGui::Begin("Instanced Effects");
	ImGui::Text("FPS %i", ZestApp->last_fps);
	ImGui::Text("Record Time: %zims", example->record_time);
	ImGui::Text("Culled Instances: %i", tfx_GetTotalInstancesBeingUpdated(example->animation_manager_3d) - example->animation_manager_3d->render_queue.size());
	ImGui::Text("Instances In View: %i", example->animation_manager_3d->render_queue.size());
	ImGui::Text("Sprites Drawn: %i", example->animation_manager_3d->buffer_metrics.total_sprites_to_draw);
	ImGui::Text("Total Memory For Drawn Sprites: %ikb", (example->animation_manager_3d->buffer_metrics.total_sprites_to_draw * 64) / (1024));
	ImGui::Text("Total Memory For Sprite Data: %ikb", example->animation_manager_3d->sprite_data_3d.size_in_bytes() / (1024));
	ImGui::End();
	ImGui::Render();
	//Let the layer know that it needs to reupload the imgui mesh data to the GPU
	zest_ResetLayer(example->imgui_layer_info.mesh_layer);
}

//The callback function called for each instance that is being updated.
bool CullAnimationInstancesCallback(tfx_animation_manager animation_manager, tfx_animation_instance_t *instance, tfx_frame_meta_t *frame_meta, void *user_data) {
	//Grab the tfxPrerecordedExample struct from the user data.
	tfxPrerecordedExample *example = static_cast<tfxPrerecordedExample *>(user_data);
	//The the uniform buffer containing the view and projection matrices so that we can get the view frustum

	//Get the position of the instance offset by the center point of the bounding box
	tfx_vec3_t bb_position = frame_meta->bb_center_point + instance->position;
	//Check if the radius of the bounding box is in view. Note that the planes are calculated in the UpdateUniform3d function.
	return (bool)zest_IsSphereInFrustum(example->planes, &bb_position.x, frame_meta->radius);
}

void UpdateMouse(tfxPrerecordedExample *app) {
	double mouse_x, mouse_y;
	GLFWwindow *handle = (GLFWwindow *)zest_Window(app->context);
	glfwGetCursorPos(handle, &mouse_x, &mouse_y);
	double last_mouse_x = app->mouse_x;
	double last_mouse_y = app->mouse_y;
	app->mouse_x = mouse_x;
	app->mouse_y = mouse_y;
	app->mouse_delta_x = last_mouse_x - app->mouse_x;
	app->mouse_delta_y = last_mouse_y - app->mouse_y;
}

//Our main update function that's run every frame.
void MainLoop(tfxPrerecordedExample *example) {
	zest_microsecs running_time = zest_Microsecs();
	zest_microsecs frame_time = 0;
	zest_uint frame_count = 0;
	zest_uint fps = 0;

	while (!glfwWindowShouldClose((GLFWwindow*)zest_Window(app->context))) {
		zest_microsecs current_frame_time = zest_Microsecs() - running_time;
		running_time = zest_Microsecs();
		frame_time += current_frame_time;
		frame_count += 1;
		if (frame_time >= ZEST_MICROSECS_SECOND) {
			frame_time -= ZEST_MICROSECS_SECOND;
			fps = frame_count;
			frame_count = 0;
		}

		glfwPollEvents();

		zest_UpdateDevice(app->device);

		UpdateMouse(app);

		if (zest_BeginFrame(context)) {
			float elapsed = (float)current_frame_time;
			zest_tfx_UpdateUniformBuffer(example->context, &example->tfx_rendering);

			int r = tfx_RandomRangeFromToInt(&example->random, 0, 3);
			if (!example->left_mouse_clicked && ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
				//If the left mouse is clicked then spawn a random firework and raycasting it into the scene.
				tfx_vec3_t position = ScreenRay(example, zest_MouseXf(), zest_MouseYf(), 12.f, example->camera.position);
				tfxAnimationID anim_id = tfxINVALID;
				if (r == 0) {
					anim_id = tfx_AddAnimationInstance(example->animation_manager_3d, "Big Explosion", 0);
				} else if (r == 1) {
					anim_id = tfx_AddAnimationInstance(example->animation_manager_3d, "Star Burst Flash", 0);
				} else if (r == 2) {
					anim_id = tfx_AddAnimationInstance(example->animation_manager_3d, "EmissionSingleShot", 0);
				} else if (r == 3) {
					anim_id = tfx_AddAnimationInstance(example->animation_manager_3d, "Firework", 0);
				}
				if (anim_id != tfxINVALID) {
					//As long as we get a valie anim id, set it's position and random scale.
					tfx_vec3_t position = tfx_vec3_t(tfx_RandomRangeFromTo(&example->random, -10.f, 10.f), tfx_RandomRangeFromTo(&example->random, 8.f, 15.f), tfx_RandomRangeFromTo(&example->random, -10.f, 10.f));
					tfx_SetAnimationPosition(example->animation_manager_3d, anim_id, &position.x);
					tfx_SetAnimationScale(example->animation_manager_3d, anim_id, tfx_RandomRangeFromTo(&example->random, 0.5f, 1.5f));
				}
				example->left_mouse_clicked = true;
			} else if (!ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
				example->left_mouse_clicked = false;
			}

			//Make a firework spawn randomly in the scene every 25 millisecs
			example->trigger_effect += elapsed;
			if (example->trigger_effect >= ZEST_MILLISECONDS_IN_MICROSECONDS(25)) {
				tfxAnimationID anim_id = tfxINVALID;
				if (r == 0) {
					anim_id = tfx_AddAnimationInstance(example->animation_manager_3d, "Big Explosion", 0);
				} else if (r == 1) {
					anim_id = tfx_AddAnimationInstance(example->animation_manager_3d, "Star Burst Flash", 0);
				} else if (r == 2) {
					anim_id = tfx_AddAnimationInstance(example->animation_manager_3d, "EmissionSingleShot", 0);
				} else if (r == 3) {
					anim_id = tfx_AddAnimationInstance(example->animation_manager_3d, "Firework", 0);
				}
				if (anim_id != tfxINVALID) {
					tfx_vec3_t position = tfx_vec3_t(tfx_RandomRangeFromTo(&example->random, -10.f, 10.f), tfx_RandomRangeFromTo(&example->random, 8.f, 15.f), tfx_RandomRangeFromTo(&example->random, -10.f, 10.f));
					tfx_SetAnimationPosition(example->animation_manager_3d, anim_id, &position.x);
					tfx_SetAnimationScale(example->animation_manager_3d, anim_id, tfx_RandomRangeFromTo(&example->random, 0.75f, 1.5f));
				}
				example->trigger_effect = 0;
			}

			if (ImGui::IsKeyDown(ImGuiKey_Space)) {
				tfx_ClearAllAnimationInstances(example->animation_manager_3d);
			}

			//First control the camera with the mosue if the right mouse is clicked
			bool camera_free_look = false;
			if (ImGui::IsMouseDown(ImGuiMouseButton_Right)) {
				camera_free_look = true;
				if (glfwRawMouseMotionSupported()) {
					glfwSetInputMode((GLFWwindow *)zest_Window(), GLFW_RAW_MOUSE_MOTION, GLFW_TRUE);
				}
				ZEST__FLAG(ImGui::GetIO().ConfigFlags, ImGuiConfigFlags_NoMouse);
				double x_mouse_speed;
				double y_mouse_speed;
				zest_GetMouseSpeed(&x_mouse_speed, &y_mouse_speed);
				zest_TurnCamera(&example->camera, (float)context->window->mouse_delta_x, (float)context->window->mouse_delta_y, .05f);
			} else if (glfwRawMouseMotionSupported()) {
				camera_free_look = false;
				ZEST__UNFLAG(ImGui::GetIO().ConfigFlags, ImGuiConfigFlags_NoMouse);
				glfwSetInputMode((GLFWwindow *)zest_Window(), GLFW_CURSOR, GLFW_CURSOR_NORMAL);
			} else {
				camera_free_look = false;
				ZEST__UNFLAG(ImGui::GetIO().ConfigFlags, ImGuiConfigFlags_NoMouse);
			}

			//An example fixed rate update loop
			zest_StartTimerLoop(app->timer) {
				//example->pm.Update();
				BuildUI(example);

				float speed = 5.f * (float)example->timer->update_time;
				if (ImGui::IsMouseDown(ImGuiMouseButton_Right)) {
					ImGui::SetWindowFocus(nullptr);

					if (ImGui::IsKeyDown(ImGuiKey_W)) {
						zest_CameraMoveForward(&example->camera, speed);
					}
					if (ImGui::IsKeyDown(ImGuiKey_S)) {
						zest_CameraMoveBackward(&example->camera, speed);
					}
					if (ImGui::IsKeyDown(ImGuiKey_UpArrow)) {
						zest_CameraMoveUp(&example->camera, speed);
					}
					if (ImGui::IsKeyDown(ImGuiKey_DownArrow)) {
						zest_CameraMoveDown(&example->camera, speed);
					}
					if (ImGui::IsKeyDown(ImGuiKey_A)) {
						zest_CameraStrafLeft(&example->camera, speed);
					}
					if (ImGui::IsKeyDown(ImGuiKey_D)) {
						zest_CameraStrafRight(&example->camera, speed);
					}
				}

				//Restore the mouse when right mouse isn't held down
				if (camera_free_look) {
					glfwSetInputMode((GLFWwindow *)zest_Window(), GLFW_CURSOR, GLFW_CURSOR_DISABLED);
				} else {
					glfwSetInputMode((GLFWwindow *)zest_Window(), GLFW_CURSOR, GLFW_CURSOR_NORMAL);
				}

				zest_TimerUnAccumulate(example->timer);
			} zest_EndTimerLoop(app->timer);

			//Update the animation manager each frame. This will advance the time of each animation instance that's currently playing
			//Pass the amount of time that has elapsed since the last time the function was called
			//This could also be placed in the fixed rate update loop with an elapsed time of the update frequency
			for (auto i : example->animation_manager_3d->instances_in_use[example->animation_manager_3d->current_in_use_buffer]) {
				tfx_animation_instance_t &instance = example->animation_manager_3d->instances[i];
				tfx_sprite_data_metrics_t &metrics = example->animation_manager_3d->effect_animation_info.data[instance.info_index];
			}
			tfx_UpdateAnimationManager(example->animation_manager_3d, elapsed / 1000.f);

			//Now set the mesh drawing so that we can draw a textured plane
			zest_SetMeshDrawing(example->mesh_layer, example->floor_resources, example->mesh_pipeline);
			zest_DrawTexturedPlane(example->mesh_layer, example->floor_image, -500.f, -5.f, -500.f, 1000.f, 1000.f, 50.f, 50.f, 0.f, 0.f);

			//Copy the offsets and animation instances either to the staging buffers. The staging buffers will then be uploaded in the render pipeline.
			memcpy(example->offsets_staging_buffer[context->current_fif]->data, example->animation_manager_3d->offsets.data, tfx_GetOffsetsSizeInBytes(example->animation_manager_3d));
			memcpy(example->animation_instances_staging_buffer[context->current_fif]->data, example->animation_manager_3d->render_queue.data, tfx_GetAnimationInstancesSizeInBytes(example->animation_manager_3d));

			zest_TimerSet(example->timer);

			zest_frame_graph_cache_key_t cache_key = {};
			cache_key = zest_InitialiseCacheKey(app->context, &app->cache_info, sizeof(RenderCacheInfo));

			zest_frame_graph frame_graph = zest_GetCachedFrameGraph(context, &cache_key);
			if (!frame_graph) {
				if (zest_BeginFrameGraph(context, "TimelineFX Render Graph", &cache_key)) {
					zest_BeginRenderPass("Blank"); {
						zest_SetPassTask(zest_EmptyRenderPass, 0);
						zest_EndPass();
					}
				}
			}
			zest_QueueFrameGraphForExecution(app->context, frame_graph);
			zest_EndFrame(app->context);
		}
	}
}

#if defined(_WIN32)
int main() {

	tfx_InitialiseTimelineFX(tfx_GetDefaultThreadCount(), tfxMegabyte(128));

	if (!glfwInit()) {
		return 0;
	}

	zest_uint count;
	const char **glfw_extensions = glfwGetRequiredInstanceExtensions(&count);

	tfxPrerecordedExample app = {};

	//Create the device that serves all vulkan based contexts
	zest_device_builder device_builder = zest_BeginVulkanDeviceBuilder();
	zest_AddDeviceBuilderExtensions(device_builder, glfw_extensions, count);
	zest_AddDeviceBuilderValidation(device_builder);
	zest_DeviceBuilderLogToConsole(device_builder);
	game.device = zest_EndDeviceBuilder(device_builder);

	//Create a window using GLFW
	zest_window_data_t window_handles = zest_implglfw_CreateWindow(50, 50, 1280, 768, 0, "PBR Simple Example");
	//Initialise Zest
	app.context = zest_CreateContext(app.device, &window_handles, &create_info);

	app.Init();
	MainLoop(&app);
	ImGui_ImplGlfw_Shutdown();
	zest_imgui_Destroy(&app.imgui);
	zest_DestroyDevice(app.device);
	tfx_EndTimelineFX();

	return EXIT_SUCCESS;
}
#else
int main() {
	//Render specific
	//When initialising a qulkan app, you can pass a QulkanCreateInfo which you can use to configure some of the base settings of the app
	//Create new config struct for Zest
	zest_create_context_info_t create_info = zest_CreateContextInfo();
	//Don't enable vsync so we can see how much FPS we get
	ZEST__UNFLAG(create_info.flags, zest_init_flag_enable_vsync);

	//Initialise TimelineFX. Must be run before using any timeline fx functionality.
	InitialiseTimelineFX(std::thread::hardware_concurrency());

	tfxPrerecordedExample example;

	//Initialise Zest
	zest_CreateContext(&create_info);
	//Set the Zest use data
	zest_SetUserData(&example);
	//Set the udpate callback to be called every frame
	zest_SetUserUpdateCallback(Update);
	//Initialise our example
	InitExample(&example);

	//Start the main loop
	zest_Start();

	return EXIT_SUCCESS;
}
#endif
