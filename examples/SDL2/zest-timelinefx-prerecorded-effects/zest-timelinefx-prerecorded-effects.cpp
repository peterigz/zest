#define ZEST_IMPLEMENTATION
#define ZEST_VULKAN_IMPLEMENTATION
#define ZEST_ALL_UTILITIES_IMPLEMENTATION
#include <SDL.h>
#include "zest-timelinefx-prerecorded-effects.h"
#include "zest.h"
#include "imgui_internal.h"
#include "imgui_impl_sdl2.h"
#include "examples/Common/sdl_controls.cpp"
#include "examples/Common/sdl_events.cpp"

#define MAX_INSTANCES 1024
#define MAX_SPRITES	250000

#define BufferCast(struct_name, buffer_name) static_cast<struct_name*>(buffer_name.mapped)

//---Render specific functions---
//The following code will depend on the renderer that you're using

//Send the command to the GPU to draw all of the sprites for all of the animation instances that are currently in play
void RecordComputeSprites(zest_command_list command_list, void *user_data) {
	tfxPrerecordedExample *example = static_cast<tfxPrerecordedExample *>(user_data);

	zest_buffer billboard_buffer = zest_GetPassInputBuffer(command_list, "Billboard Instances");
	//Bind the buffer that contains the sprite instances to draw. These are updated by the compute shader on the GPU
	zest_cmd_BindVertexBuffer(command_list, 0, 1, billboard_buffer);

	//Draw all the instance_data in the buffer
	tfx_sprite_sheet_push_t push{};
	push.color_ramp_texture_index = example->tfx_rendering.color_ramps_index;
	push.particle_texture_index = example->tfx_rendering.particle_texture_index;
	push.sampler_index = example->tfx_rendering.sampler_index;
	push.image_data_index = example->image_data_index;
	push.prev_billboards_index = 0;
	zest_uniform_buffer uniform_buffer = zest_GetUniformBuffer(example->tfx_rendering.uniform_buffer);
	push.uniform_index = zest_GetUniformBufferDescriptorIndex(uniform_buffer);

	zest_layer layer = zest_GetLayer(example->tfx_rendering.layer);
	zest_cmd_LayerViewport(command_list, layer);
	
	zest_pipeline pipeline = zest_GetPipeline(example->tfx_rendering.pipeline, command_list);
	zest_cmd_BindPipeline(command_list, pipeline);
	zest_cmd_SendPushConstants(command_list, &push, sizeof(tfx_sprite_sheet_push_t));
	tfxU32 total_billboards = tfx_GetTotalSpritesThatNeedDrawing(example->animation_manager_3d);
	zest_cmd_Draw(command_list, 6, total_billboards, 0, 0);
}

//Upload the buffers to the GPU that the compute shader will need to read from in order to build the sprite buffer each frame. These buffers
//only need to be uploaded once once you've added all the sprite data for all the animations that you want to use.
void UploadBuffers(tfxPrerecordedExample *example) {

	tfx_animation_manager animation_manager = example->animation_manager_3d;

	//Upload the sprite data. This contains all pre-recorded instance_data for all animations that you might want to play.
	zest_queue queue = zest_imm_BeginCommandBuffer(example->device, zest_queue_transfer);
	zest_buffer sprite_staging_buffer =  zest_CreateStagingBuffer(example->device, tfx_GetSpriteDataSizeInBytes(animation_manager), tfx_GetSpriteDataBufferPointer(animation_manager));
	zest_imm_CopyBuffer(queue, sprite_staging_buffer, example->sprite_data_buffer, tfx_GetSpriteDataSizeInBytes(animation_manager));

	//Upload the emitter properties for each animation you're using. This contains the sprite handle and any flags that might be relavent.
	zest_buffer emitter_properties_buffer = example->emitter_properties_buffer;
	zest_buffer emitter_staging_buffer = zest_CreateStagingBuffer(example->device, tfx_GetAnimationEmitterPropertySizeInBytes(animation_manager), tfx_GetAnimationEmitterPropertiesBufferPointer(animation_manager));
	zest_imm_CopyBuffer(queue, emitter_staging_buffer, emitter_properties_buffer, tfx_GetAnimationEmitterPropertySizeInBytes(animation_manager));

	//Upload the image data containing all the UV coords and texture array index that the compute shader will use to build the sprite
	//buffer each frame
	zest_buffer image_staging_buffer = zest_CreateStagingBuffer(example->device, tfx_GetGPUShapesSizeInBytes(example->gpu_image_data), tfx_GetGPUShapesArray(example->gpu_image_data));
	zest_imm_CopyBuffer(queue, image_staging_buffer, example->image_data_buffer, tfx_GetGPUShapesSizeInBytes(example->gpu_image_data));
	zest_imm_EndCommandBuffer(queue);

	zest_FreeBuffer(sprite_staging_buffer);
	zest_FreeBuffer(emitter_staging_buffer);
	zest_FreeBuffer(image_staging_buffer);
	example->image_data_index = zest_AcquireStorageBufferIndex(example->device, example->image_data_buffer);
	example->sprite_data_index = zest_AcquireStorageBufferIndex(example->device, example->sprite_data_buffer);
	example->emitter_properties_index = zest_AcquireStorageBufferIndex(example->device, example->emitter_properties_buffer);
}

//Prepare the compute shader that will be used to playback the effects in the animation manager
void PrepareComputeForEffectPlayback(tfxPrerecordedExample *example) {
	//Register a new compute shader
	example->playback_shader = zest_CreateShaderFromFile(example->device, "examples/assets/shaders/sprite_data_playback.comp", "sprite_data_playback.comp.spv", zest_compute_shader, true);

	//Finalise the compute set up by calling MakeCompute which creates all the boiler plate code to create the
	//compute pipeline in Vulkan
	example->compute_pipeline_3d = zest_CreateCompute(example->device, "TFX Playback", example->playback_shader);
}

//Every frame the compute shader needs to be dispatched which means that all the commands for the compute shader
//need to be added to the command buffer
void SpriteComputeFunction(zest_command_list command_list, void *user_data) {

	tfxPrerecordedExample *example = static_cast<tfxPrerecordedExample*>(user_data);

	zest_resource_node offsets_node = zest_GetPassInputResource(command_list, "Buffer Offsets");
	zest_resource_node animation_instances_node = zest_GetPassInputResource(command_list, "Animation Instances");
	zest_resource_node billboard_instances_node = zest_GetPassOutputResource(command_list, "Billboard Instances");

	tfx_animation_manager animation_manager = nullptr;
	animation_manager = example->animation_manager_3d;
	zest_compute compute = zest_GetCompute(example->compute_pipeline_3d);
	zest_cmd_BindComputePipeline(command_list, compute);

	//We'll need the animation metrics to tell the compute shader how many animation instances we're rendering this frame
	tfx_animation_buffer_metrics_t metrics = tfx_GetAnimationBufferMetrics(animation_manager);

	//Update the push constants with some metrics. These are referenced in the compute shader.
	//The total number of animation instances that need to be drawn
	tfx_sprite_data_push_t push;
	push.animation_instances_total = metrics.instances_size;
	//If any animation instance contains animated shapes then set to 1. the compute shader can use this to avoid some unecessary
	//computation if all particle shapes are not animated
	push.animated_shapes = (animation_manager->flags & tfxAnimationManagerFlags_has_animated_shapes);
	//Set the total number of instance_data that need to be processed by the shader
	push.billboards_total = metrics.total_sprites_to_draw;

	push.offsets_index = zest_GetTransientBufferBindlessIndex(command_list, offsets_node);
	push.animation_instances_index = zest_GetTransientBufferBindlessIndex(command_list, animation_instances_node);
	push.billboards_index = zest_GetTransientBufferBindlessIndex(command_list, billboard_instances_node);
	push.image_data_index = example->image_data_index;
	push.sprite_data_index = example->sprite_data_index;
	push.emitter_properties_index = example->emitter_properties_index;

	//Send the push constants in the compute object to the shader
	zest_cmd_SendPushConstants(command_list, &push, sizeof(tfx_sprite_data_push_t));

	//The 128 here refers to the local_size_x in the shader and is how many elements each group will work on
	//For example if there are 1024 instance_data, if we divide by 128 there will be 8 groups working on 128 instance_data each in parallel
	zest_cmd_DispatchCompute(command_list, (metrics.total_sprites_to_draw / 128) + 1, 1, 1);
}
//----End of render specific functions-----

//Initialise the example and create all the necessary buffers and objects for the compute shaders and load in the effects library
//and prepare the effect we'll use as an example.
void InitExample(tfxPrerecordedExample *example) {
	zest_tfx_InitTimelineFXRenderResources(example->context, &example->tfx_rendering, "examples/assets/shaders/timelinefx3dstatic.vert", "examples/assets/shaders/timelinefxstatic.frag");
	example->tfx_rendering.particle_images = zest_tfx_CreateImageCollection(32);

	zest_sampler_info_t sampler = zest_CreateSamplerInfo();
	example->sampler = zest_CreateSampler(example->context, &sampler);
	example->sampler_index = zest_AcquireSamplerIndex(example->device, zest_GetSampler(example->sampler));

	int width, height, channels;
	stbi_uc *pixels = stbi_load("examples/assets/checker.png", &width, &height, &channels, 4);
	int size = width * height * 4;
	zest_image_info_t image_info = zest_CreateImageInfo(width, height);
	image_info.format = zest_format_r8g8b8a8_unorm;
	image_info.flags = zest_image_preset_texture_mipmaps;
	STBI_FREE(pixels);

	//Initialise an animation manager. An animation manager maintains our animation instances for us and provides all the necessary metrics we'll
	//need to upload the buffers we need to both to initialise the compute shader and when we upload the offsets and instances buffer each frame.
	//Specify the maximum number of animation instances that you might want to play each frame
	example->animation_manager_3d = tfx_CreateAnimationManager(MAX_INSTANCES, MAX_SPRITES);
	//Here we set the callback that will be used to decide if an animation should be drawn or not. We use the bounding box to check if it's inside
	//the view frustum and cull it if it's not.
	//example->animation_manager_3d->maybe_render_instance_callback = CullAnimationInstancesCallback;
	//Set the user data to the tfxPrerecordedExample which we can use in the callback function
	tfx_SetAnimationManagerUserData(example->animation_manager_3d, example);

	//Create the handle where we will store gpu shapes for uploading to the gpu
	example->gpu_image_data = tfx_CreateGPUShapesList();

	//Record the effects we want and calculate the bounding boxes
	example->record_time = zest_Millisecs();		//See how long it takes to record.

	//Load the effects library and pass our shape loader callback function which loads the particle shape images into our texture
	tfxErrorFlags result = tfx_LoadSpriteData("examples/assets/prerecorded_effects.tfxsd", example->animation_manager_3d, zest_tfx_ShapeLoader, &example->tfx_rendering);
	//ListEffectNames(&example->library);
	assert(result == tfxErrorCode_success);		//Unable to load the effects library!

	example->tfx_rendering.particle_texture = zest_CreateImageAtlas(example->context, &example->tfx_rendering.particle_images, 1024, 1024, 0);
	zest_image particle_image = zest_GetImage(example->tfx_rendering.particle_texture);
	example->tfx_rendering.particle_texture_index = zest_AcquireSampledImageIndex(example->device, particle_image, zest_texture_array_binding);

	//Add the color ramps from the library to the color ramps texture. Color ramps in the library are stored in rgba format and can be
	//simply copied to a bitmap for uploading to the texture
	example->tfx_rendering.color_ramps_collection = zest_CreateImageAtlasCollection(zest_format_r8g8b8a8_unorm, example->animation_manager_3d->color_ramps.color_ramp_bitmaps.size());
	for (tfx_bitmap_t &bitmap : example->animation_manager_3d->color_ramps.color_ramp_bitmaps) {
		zest_AddImageAtlasPixels(&example->tfx_rendering.color_ramps_collection, bitmap.data, bitmap.size, bitmap.width, bitmap.height, zest_format_r8g8b8a8_unorm);	//Create the texture for the color ramps
	}
	example->tfx_rendering.color_ramps_texture = zest_CreateImageAtlas(example->context, &example->tfx_rendering.color_ramps_collection, 256, 256, zest_image_preset_texture);
	zest_image color_ramps_image = zest_GetImage(example->tfx_rendering.color_ramps_texture);
	example->tfx_rendering.color_ramps_index = zest_AcquireSampledImageIndex(example->device, color_ramps_image, zest_texture_array_binding);
	//Build the gpu image data. Pass the GetUV function which you will have to create based on your renderer.
	tfx_BuildAnimationManagerGPUShapeData(example->animation_manager_3d, example->gpu_image_data, zest_tfx_GetUV);
	//Update the image data to the gpu
	zest_tfx_UpdateTimelineFXImageData(example->context, &example->tfx_rendering, example->gpu_image_data);

	zest_buffer_info_t buffer_info = zest_CreateBufferInfo(zest_buffer_type_storage, zest_memory_usage_gpu_only);

	//Render Specific - Create the 6 buffers needed for the compute shader. Create these after you've added all the effect animations you want to use
	//to the animation manager.
	/*
	1)	Sprite data buffer for all sprites in all frames of the animation. Use tfx_GetTotalSpriteDataCount() and/or tfx_GetSpriteDataSizeInBytes() to get the
		metrics you need to create the buffer. Note that we've already recorded the sprite data so we know the size of the buffer we need, otherwise
		you might have to resize the buffer later and update the descriptor sets
	*/
	example->sprite_data_buffer = zest_CreateBuffer(example->device, sizeof(tfx_sprite_data_t) * tfx_GetTotalSpriteDataCount(example->animation_manager_3d), &buffer_info);

	/*
	5)	Image data buffer contains data about all of the shapes being used in the library. Use GetComputeShapeCount and GetComputeShapeSizeInBytes to get the metrics you need
		to create the buffer.
	*/
	example->image_data_buffer = zest_CreateBuffer(example->device, sizeof(tfx_gpu_image_data_t) * 1000, &buffer_info);

	/*
	6)	Emitter properties contains data about each emitter used to create the animation that the compute shader uses to build the sprite buffer.
		Use GetAnimationEmitterPropertyCount and tfx_GetAnimationEmitterPropertySizeInBytes to get the metrics you need to create the buffer.
	*/
	example->emitter_properties_buffer = zest_CreateBuffer(example->device, sizeof(tfx_animation_emitter_properties_t) * 100, &buffer_info);

	example->bounding_boxes = zest_CreateBuffer(example->device, sizeof(tfx_bounding_box_t) * 200, &buffer_info);

	//Prepare the compute shaders for effect playback and bounding box calculation
	PrepareComputeForEffectPlayback(example);

	//Now that the sprite data is prepared we can Upload the buffers to the GPU. These are the buffers that we only need to upload once before we do anything
	//They will containt the sprite, image and emitter data that the compute shader will index into to build each frame
	UploadBuffers(example);

	//Create the staging buffers that will stage the per frame instance and offset buffers
	zest_ForEachFrameInFlight(fif) {
		example->animation_instances_staging_buffer[fif] = zest_CreateStagingBuffer(example->device, MAX_INSTANCES * sizeof(tfx_animation_instance_t), 0);
		example->offsets_staging_buffer[fif] = zest_CreateStagingBuffer(example->device, MAX_INSTANCES * sizeof(tfxU32), 0);
	}

	example->record_time = zest_Millisecs() - example->record_time;	//note how long the above took

	//Initialise Dear ImGui
	zest_imgui_Initialise(example->context, &example->imgui, zest_implsdl2_DestroyWindow);
    ImGui_ImplSDL2_InitForVulkan((SDL_Window *)zest_Window(example->context));

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
	zest_vec3 camera_last_ray = zest_ScreenRay(x, y, zest_ScreenWidthf(example->context), zest_ScreenHeightf(example->context), &ubo_ptr->proj, &ubo_ptr->view);
	zest_vec3 pos = zest_AddVec3(zest_ScaleVec3(camera_last_ray, depth_offset), camera_position);
	return { pos.x, pos.y, pos.z };
}

void BuildUI(tfxPrerecordedExample *example, zest_uint fps) {
	//Must call the imgui SDL2 implementation function
	ImGui_ImplSDL2_NewFrame();
	//Draw our imgui stuff
	ImGui::NewFrame();
	ImGui::Begin("Instanced Effects");
	ImGui::Text("FPS %i", fps);
	ImGui::Text("Record Time: %zims", example->record_time);
	ImGui::Text("Culled Instances: %i", tfx_GetTotalInstancesBeingUpdated(example->animation_manager_3d) - example->animation_manager_3d->render_queue.size());
	ImGui::Text("Instances In View: %i", example->animation_manager_3d->render_queue.size());
	ImGui::Text("Sprites Drawn: %i", example->animation_manager_3d->buffer_metrics.total_sprites_to_draw);
	ImGui::Text("Total Memory For Drawn Sprites: %ikb", (example->animation_manager_3d->buffer_metrics.total_sprites_to_draw * 64) / (1024));
	ImGui::Text("Total Memory For Sprite Data: %ikb", example->animation_manager_3d->sprite_data.size_in_bytes() / (1024));
	ImGui::End();
	ImGui::Render();
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

void UploadAnimationData(const zest_command_list command_list, void *user_data) {
	tfxPrerecordedExample *example = static_cast<tfxPrerecordedExample *>(user_data);

	zest_resource_node offsets_node = zest_GetPassOutputResource(command_list, "Buffer Offsets");
	zest_resource_node animation_instances_node = zest_GetPassOutputResource(command_list, "Animation Instances");

	zest_buffer offsets_buffer = zest_GetResourceBuffer(offsets_node);
	zest_buffer animation_instances_buffer = zest_GetResourceBuffer(animation_instances_node);

	zest_uint fif = zest_CurrentFIF(zest_GetContext(command_list));

	zest_cmd_CopyBuffer(command_list, example->offsets_staging_buffer[fif], offsets_buffer, zest_BufferSize(offsets_buffer));
	zest_cmd_CopyBuffer(command_list, example->animation_instances_staging_buffer[fif], animation_instances_buffer, zest_BufferSize(animation_instances_buffer));
}

zest_buffer GetOffsetsBufferSize(zest_context context, zest_resource_node node) {
	tfxPrerecordedExample *example = static_cast<tfxPrerecordedExample *>(zest_GetResourceUserData(node));
	zest_SetResourceBufferSize(node, tfx_GetOffsetsSizeInBytes(example->animation_manager_3d));
    return NULL;
}

zest_buffer GetAnimationInstanceBufferSize(zest_context context, zest_resource_node node) {
	tfxPrerecordedExample *example = static_cast<tfxPrerecordedExample *>(zest_GetResourceUserData(node));
	zest_SetResourceBufferSize(node, tfx_GetAnimationInstancesSizeInBytes(example->animation_manager_3d));
    return NULL;
}

zest_buffer GetBillboardInstanceBufferSize(zest_context context, zest_resource_node node) {
	tfxPrerecordedExample *example = static_cast<tfxPrerecordedExample *>(zest_GetResourceUserData(node));
	zest_SetResourceBufferSize(node, tfx_GetTotalSpritesThatNeedDrawing(example->animation_manager_3d) * sizeof(tfx_instance_t));
    return NULL;
}

//Our main update function that's run every frame.
void MainLoop(tfxPrerecordedExample *example) {

	zest_microsecs running_time = zest_Microsecs();
	zest_microsecs frame_time = 0;
	zest_uint frame_count = 0;
	zest_uint fps = 0;
	int running = 1;
	SDL_Event event;

	while (running) {
		running = PollSDLEvents(example->context, &event);
		zest_microsecs current_frame_time = zest_Microsecs() - running_time;
		running_time = zest_Microsecs();
		frame_time += current_frame_time;
		frame_count += 1;
		if (frame_time >= ZEST_MICROSECS_SECOND) {
			frame_time -= ZEST_MICROSECS_SECOND;
			fps = frame_count;
			frame_count = 0;
		}
		zest_UpdateDevice(example->device);

		if (zest_BeginFrame(example->context)) {
			UpdateMouse(example->context, &example->mouse, &example->tfx_rendering.camera);

			zest_tfx_UpdateUniformBuffer(example->context, &example->tfx_rendering);

			zest_uint fif = zest_CurrentFIF(example->context);

			int r = tfx_RandomRangeFromToInt(&example->random, 0, 3);
			if (!example->left_mouse_clicked && ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
				//If the left mouse is clicked then spawn a random firework and raycasting it into the scene.
				tfx_vec3_t position = ScreenRay(example, (float)example->mouse.mouse_x, (float)example->mouse.mouse_y, 12.f, example->tfx_rendering.camera.position);
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
			example->trigger_effect += current_frame_time;
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

			//An example fixed rate update loop
			zest_StartTimerLoop(example->timer) {
				//example->pm.Update();
				BuildUI(example, fps);

				UpdateCameraPosition(&example->timer, &example->new_camera_position, &example->old_camera_position, &example->tfx_rendering.camera, 5.f);
			} zest_EndTimerLoop(example->timer);

			example->tfx_rendering.camera.position = zest_LerpVec3(&example->old_camera_position, &example->new_camera_position, (float)zest_TimerLerp(&example->timer));

			//Update the animation manager each frame. This will advance the time of each animation instance that's currently playing
			//Pass the amount of time that has elapsed since the last time the function was called
			//This could also be placed in the fixed rate update loop with an elapsed time of the update frequency
			for (auto i : example->animation_manager_3d->instances_in_use[example->animation_manager_3d->current_in_use_buffer]) {
				tfx_animation_instance_t &instance = example->animation_manager_3d->instances[i];
				tfx_sprite_data_metrics_t &metrics = example->animation_manager_3d->effect_animation_info.data[instance.info_index];
			}
			tfx_UpdateAnimationManager(example->animation_manager_3d, (float)current_frame_time / 1000.f);

			//Copy the offsets and animation instances either to the staging buffers. The staging buffers will then be uploaded in the render pipeline.
			memcpy(zest_BufferData(example->offsets_staging_buffer[fif]), example->animation_manager_3d->offsets.data , tfx_GetOffsetsSizeInBytes(example->animation_manager_3d));
			memcpy(zest_BufferData(example->animation_instances_staging_buffer[fif]), example->animation_manager_3d->render_queue.data, tfx_GetAnimationInstancesSizeInBytes(example->animation_manager_3d));

			RenderCacheInfo cache_info{};
			cache_info.draw_imgui = zest_imgui_HasGuiToDraw(&example->imgui);
			cache_info.draw_timeline_fx = true;
			zest_frame_graph_cache_key_t cache_key = {};
			cache_key = zest_InitialiseCacheKey(example->context, &cache_info, sizeof(RenderCacheInfo));

			zest_frame_graph frame_graph = zest_GetCachedFrameGraph(example->context, &cache_key);
			if (!frame_graph) {
				if (zest_BeginFrameGraph(example->context, "TimelineFX Render Graph", 0)) {
					zest_ImportSwapchainResource();
					zest_buffer_resource_info_t offsets_buffer_info = { zest_resource_usage_hint_copy_dst, tfx_GetOffsetsSizeInBytes(example->animation_manager_3d) };
					zest_buffer_resource_info_t animation_instances_buffer_info = { zest_resource_usage_hint_copy_dst, tfx_GetAnimationInstancesSizeInBytes(example->animation_manager_3d) };
					zest_buffer_resource_info_t billboard_instance_buffer_info = { zest_resource_usage_hint_none, tfx_GetTotalSpritesThatNeedDrawing(example->animation_manager_3d) * sizeof(tfx_instance_t) };

					zest_resource_node offsets_buffer = zest_AddTransientBufferResource("Buffer Offsets", &offsets_buffer_info);
					zest_resource_node animation_instances = zest_AddTransientBufferResource("Animation Instances", &animation_instances_buffer_info);
					zest_resource_node billboard_instances = zest_AddTransientBufferResource("Billboard Instances", &billboard_instance_buffer_info);
					zest_SetResourceBufferProvider(offsets_buffer, GetOffsetsBufferSize);
					zest_SetResourceBufferProvider(animation_instances, GetAnimationInstanceBufferSize);
					zest_SetResourceBufferProvider(billboard_instances, GetBillboardInstanceBufferSize);
					zest_SetResourceUserData(offsets_buffer, example);
					zest_SetResourceUserData(animation_instances, example);
					zest_SetResourceUserData(billboard_instances, example);

					//------------------------ Sprite Data Tab -----------------------------------------------------------
					zest_BeginTransferPass("Transfer Animation Data"); {
						zest_ConnectOutput(offsets_buffer);
						zest_ConnectOutput(animation_instances);
						zest_SetPassTask(UploadAnimationData, example);
						zest_EndPass();
					}

					zest_BeginComputePass("Compute Sprite Data"); {
						zest_ConnectInput(offsets_buffer);
						zest_ConnectInput(animation_instances);
						zest_ConnectOutput(billboard_instances);
						zest_SetPassTask(SpriteComputeFunction, example);
						zest_EndPass();
					}

					zest_BeginRenderPass("Sprite Data Pass"); {
						zest_ConnectInput(billboard_instances);
						zest_ConnectSwapChainOutput();
						zest_SetPassTask(RecordComputeSprites, example);
						zest_EndPass();
					}

					zest_pass_node imgui_pass = zest_imgui_BeginPass(&example->imgui, example->imgui.main_viewport); {
						if (imgui_pass) {
							zest_ConnectSwapChainOutput();
							zest_EndPass();
						}
					}
				}
				frame_graph = zest_EndFrameGraph();
			}
			zest_EndFrame(example->context, frame_graph);
		}
	}
}

int main(int argc, char *argv[]) {
	zest_create_context_info_t create_info = zest_CreateContextInfo();
	ZEST__UNFLAG(create_info.flags, zest_context_init_flag_enable_vsync);

	tfxPrerecordedExample example{};
	//Initialise TimelineFX with however many threads you want. Each emitter is updated in it's own thread.
	tfx_InitialiseTimelineFX(tfx_GetDefaultThreadCount(), tfxMegabyte(128));

	//Create a window using SDL2. We must do this before setting up the device as it's needed to get
	//the extensions info.
	zest_window_data_t window_data = zest_implsdl2_CreateWindow(50, 50, 1280, 768, 0, "TimelineFX Example");

	//Create the device that serves all vulkan based contexts
	example.device = zest_implsdl2_CreateVulkanDevice(&window_data, false);

	//Initialise Zest
	example.context = zest_CreateContext(example.device, &window_data, &create_info);

	InitExample(&example);

	MainLoop(&example);

	ImGui_ImplSDL2_Shutdown();
	zest_imgui_Destroy(&example.imgui);
	zest_DestroyDevice(example.device);
	tfx_EndTimelineFX();

	return 0;
}
