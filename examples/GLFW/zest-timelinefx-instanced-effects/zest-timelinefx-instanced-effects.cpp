#include "zest-timelinefx-instanced-effects.h"

/*

*/

using namespace tfx;
/*
Step 1:	Register a Computer with RegisterComputer.
Step 2: Create any buffers that we need for the compute shader.
Step 3:	Use ComputeBuilder to set up all the things we need for a compute shader:
Step 4: Use MakeCompute with the ComputeBuilder we setup to finish the compute setup
*/

#define MAX_INSTANCES 1024
#define MAX_SPRITES	1000000

#define BufferCast(struct_name, buffer_name) static_cast<struct_name*>(buffer_name.mapped)

//Before you load an effects file, you will need to define a ShapeLoader function that passes the following parameters:
//const char* filename			- this will be the filename of the image being loaded from the library. You don't have to do anything with this if you don't need to.
//ImageData	&image_data			- A struct containing data about the image. You will have to set image_data.ptr to point to the texture in your renderer for later use in the Render function that you will create to render the particles
//void *raw_image_data			- The raw data of the image which you can use to load the image into graphics memory
//int image_memory_size			- The size in bytes of the raw_image_data
//void *custom_data				- This allows you to pass through an object you can use to access whatever is necessary to load the image into graphics memory, depending on the renderer that you're using
void ShapeLoader(const char* filename, tfx_image_data_t *image_data, void *raw_image_data, int image_memory_size, void *custom_data) {
	//Cast your custom data, this can be anything you want
	ComputeExample *game = static_cast<ComputeExample*>(custom_data);

	//This shape loader example uses the STB image library to load the raw bitmap (png usually) data
	zest_bitmap_t bitmap = zest_NewBitmap();
	zest_LoadBitmapImageMemory(&bitmap, (unsigned char*)raw_image_data, image_memory_size, 0);
	//Convert the bitmap to RGBA which is necessary for this particular renderer
	zest_ConvertBitmapToRGBA(&bitmap, 255);
	//The editor has the option to convert an image to an alpha map. I will probably change this so that it gets baked into the saved effect so you won't need to apply the filter here.
	//Alpha map is where all color channels are set to 255
	if (image_data->import_filter) {
		zest_ConvertBitmapToAlpha(&bitmap);
	}
	zest_SetText(&bitmap.name, filename);

	//You'll probably need to load the bitmap in such a way depending on whether or not it's an animation or not
	if (image_data->animation_frames > 1) {
		float max_radius;
		//Add the spritesheet to the texture in our renderer
		zest_image anim = zest_AddTextureAnimationBitmap(game->particle_texture, &bitmap, (tfxU32)image_data->image_size.x, (tfxU32)image_data->image_size.y, (tfxU32)image_data->animation_frames, &max_radius, 1);
		//Important step: you need to point the ImageData.ptr to the appropriate handle in the renderer to point to the texture of the particle shape
		//You'll need to use this in your render function to tell your renderer which texture to use to draw the particle
		image_data->ptr = anim;
	}
	else {
		//Add the bitmap to the texture in our renderer
		zest_image image = zest_AddTextureImageBitmap(game->particle_texture, &bitmap);
		//Important step: you need to point the ImageData.ptr to the appropriate handle in the renderer to point to the texture of the particle shape
		//You'll need to use this in your render function to tell your renderer which texture to use to draw the particle
		image_data->ptr = image;
	}
}

/*In order to build the image data that contains things like image uv coords, you will need a callback function like below
that will get the relevant uvs for each sprite needed in the tfx_library_t. This will be dependent on the renderer that you're
using. In this renderer we can pack the uv coords into 2 packed uint snorms so we load those into the uv_xy and uv_zw fields
The callback requires the following parameters:
* @param ptr			//void* pointer to your image data in the renderer that contains what you need to get at the uv data
* @param image_data		//tfx_gpu_image_data_t: a struct in TimelineFX that you can load extra data into such as a texture array index if needed.
* @param offset			//offset which you can use if you have animated shapes and all the shapes are stored in an array
* returns tfx_vec4_t		//Must return a tfx_vec4_t containing the uv coords. These will be added to the tfx_gpu_image_data_t
*/
tfx_vec4_t GetUV(void *ptr, tfx_gpu_image_data_t *image_data, int offset) {
	zest_image image = (static_cast<zest_image>(ptr) + offset);
	image_data->texture_array_index = image->layer;
	image_data->uv_xy = image->uv_xy;
	image_data->uv_zw = image->uv_zw;
	return tfx_vec4_t(image->uv.x, image->uv.y, image->uv.z, image->uv.w);
}

//---Render specific code---
//The following code will depend on the renderer that you're using

//Update the uniform buffer used to transform vertices in the vertex buffer
void UpdateUniform3d(ComputeExample *game) {
	zest_uniform_buffer_data_t *ubo_ptr = static_cast<zest_uniform_buffer_data_t*>(zest_GetUniformBufferData(ZestRenderer->standard_uniform_buffer));
	ubo_ptr->view = zest_LookAt(game->camera.position, zest_AddVec3(game->camera.position, game->camera.front), game->camera.up);
	ubo_ptr->proj = zest_Perspective(game->camera.fov, zest_ScreenWidthf() / zest_ScreenHeightf(), 0.1f, 10000.f);
	ubo_ptr->proj.v[1].y *= -1.f;
	ubo_ptr->screen_size.x = zest_ScreenWidthf();
	ubo_ptr->screen_size.y = zest_ScreenHeightf();
	ubo_ptr->millisecs = 0;
}

//Send the command to the GPU to draw all of the sprites for all of the animation instances that are currently in play
void DrawComputeSprites(zest_draw_routine routine, VkCommandBuffer command_buffer) {
	ComputeExample &game = *static_cast<ComputeExample*>(routine->user_data);

	//Bind the buffer that contains the sprite instances to draw. These are updated by the compute shader on the GPU
	zest_BindVertexBuffer(game.sprite_buffer->buffer[0]);

	//Draw all the sprites in the buffer
	if (game.effect_is_3d) {
		zest_pipeline pipeline = zest_Pipeline("pipeline_billboard");
		zest_BindPipeline(pipeline, zest_CurrentTextureDescriptorSet(game.particle_texture));
		zest_SendPushConstants(pipeline, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, sizeof(zest_push_constants_t), &game.push_contants);
		zest_Draw(6, GetTotalSpritesThatNeedDrawing(&game.animation_manager_3d), 0, 0);
	}
	else {
		zest_pipeline pipeline = zest_Pipeline("pipeline_2d_sprites");
		zest_BindPipeline(pipeline, zest_CurrentTextureDescriptorSet(game.particle_texture));
		zest_SendPushConstants(pipeline, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, sizeof(AnimationComputeConstants), &game.push_contants);
		zest_Draw(6, game.animation_manager_2d.buffer_metrics.total_sprites_to_draw, 0, 0);
	}
}

//A callback for when the window size is changed so we can update the layer push constants that contain the current screen size
void UpdateSpriteResolution(zest_draw_routine routine) {
	ComputeExample &game = *static_cast<ComputeExample*>(routine->user_data);
	game.push_contants.parameters1 = zest_Vec4Set(2.0f / (float)zest_GetSwapChainExtent().width, 2.0f / (float)zest_GetSwapChainExtent().height, -1.f, -1.f);
	game.push_contants.model = zest_M4(1.f);
}

//Upload the buffers to the GPU that the compute shader will need to read from in order to build the sprite buffer each frame. These buffers
//only need to be uploaded once once you've added all the sprite data for all the animations that you want to use.
void UploadBuffers(ComputeExample *game) {

	tfx_animation_manager_t *animation_manager = nullptr;
	if (game->effect_is_3d) {
		animation_manager = &game->animation_manager_3d;
	}
	else {
		animation_manager = &game->animation_manager_2d;
	}

	//Upload the sprite data. This contains all pre-recorded sprites for all animations that you might want to play.
	zest_buffer staging_buffer = zest_CreateStagingBuffer(GetSpriteDataSizeInBytes(animation_manager), GetSpriteDataBufferPointer(animation_manager));
	zest_CopyBuffer(staging_buffer, game->sprite_data_buffer->buffer[0], GetSpriteDataSizeInBytes(animation_manager));
	zest_FreeBuffer(staging_buffer);

	//Upload the emitter properties for each animation you're using. This contains the sprite handle and any flags that might be relavent.
	zest_buffer emitter_properties_buffer = zest_GetBufferFromDescriptorBuffer(game->emitter_properties_buffer);
	staging_buffer = zest_CreateStagingBuffer(GetAnimationEmitterPropertySizeInBytes(animation_manager), GetAnimationEmitterPropertiesBufferPointer(animation_manager));
	zest_CopyBuffer(staging_buffer, emitter_properties_buffer, GetAnimationEmitterPropertySizeInBytes(animation_manager));
	zest_FreeBuffer(staging_buffer);

	//Upload the image data containing all the UV coords and texture array index that the compute shader will use to build the sprite
	//buffer each frame
	staging_buffer = zest_CreateStagingBuffer(GetGPUShapesSizeInBytes(&game->gpu_image_data), GetGPUShapesPointer(&game->gpu_image_data));
	zest_CopyBuffer(staging_buffer, game->image_data_buffer->buffer[0], GetGPUShapesSizeInBytes(&game->gpu_image_data));
	zest_FreeBuffer(staging_buffer);
}

//Prepare the compute shader.
void PrepareCompute(ComputeExample *game) {
	//Register a new compute shader
	game->compute = zest_CreateCompute("Sprite data compute");

	//Utilize a ComputeBuilder to make setting up the compute shader a lot easier
	zest_compute_builder_t builder = zest_NewComputeBuilder();
	//Add layout binding for the buffers that we'll need to use in the compute shader
	zest_AddComputeLayoutBinding(&builder, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 0);
	zest_AddComputeLayoutBinding(&builder, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1);
	zest_AddComputeLayoutBinding(&builder, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 2);
	zest_AddComputeLayoutBinding(&builder, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 3);
	zest_AddComputeLayoutBinding(&builder, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 4);
	zest_AddComputeLayoutBinding(&builder, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 5);
	//Add the bindings for the buffers we've created
	zest_AddComputeBufferForBinding(&builder, game->sprite_data_buffer);				//Binding 0
	zest_AddComputeBufferForBinding(&builder, game->offsets_buffer);					//Binding 1
	zest_AddComputeBufferForBinding(&builder, game->animation_instances_buffer);		//Binding 2
	zest_AddComputeBufferForBinding(&builder, game->image_data_buffer);					//Binding 3
	zest_AddComputeBufferForBinding(&builder, game->emitter_properties_buffer);			//Binding 4
	zest_AddComputeBufferForBinding(&builder, game->sprite_buffer);						//Binding 5
	//Specify the shader that we want to use for the compute shader
	game->compute_pipeline_2d = zest_AddComputeShader(&builder, "examples/assets/spv/sprite_data_playback_2d.comp.spv");
	game->compute_pipeline_3d = zest_AddComputeShader(&builder, "examples/assets/spv/sprite_data_playback.comp.spv");
	//Specify the size of the push constant struct if we're using it
	zest_SetComputePushConstantSize(&builder, sizeof(zest_compute_push_constant_t));
	//Set the user data that can be passed to the callbacks
	zest_SetComputeUserData(&builder, game);
	zest_SetComputeDescriptorUpdateCallback(&builder, zest_StandardComputeDescriptorUpdate);
	//Finalise the compute set up by calling MakeCompute which creates all the boiler plate code to create the
	//compute pipeline in Vulkan
	zest_MakeCompute(&builder, game->compute);
}

//Every frame the compute shader needs to be dispatched which means that all the commands for the compute shader
//need to be added to the command buffer
void SpriteComputeFunction(zest_command_queue_compute compute_routine) {

	//The compute queue item can contain more then one compute shader to be dispatched
	//There's only one in this editor though which we created in the PrepareCompute function
	zest_compute compute;
	while (compute = zest_NextComputeRoutine(compute_routine)) {
		//Grab our ComputeExample struct out of the user data
		ComputeExample *game = static_cast<ComputeExample*>(compute->user_data);

		tfx_animation_manager_t *animation_manager = nullptr;
		if (game->effect_is_3d) {
			animation_manager = &game->animation_manager_3d;
			zest_BindComputePipeline(compute, game->compute_pipeline_3d);
			if (game->using_staging_buffers) {
				zest_CopyBufferCB(zest_CurrentCommandBuffer(), game->offsets_staging_buffer[ZEST_FIF], game->offsets_buffer->buffer[ZEST_FIF], GetOffsetsSizeInBytes(animation_manager));
				zest_CopyBufferCB(zest_CurrentCommandBuffer(), game->animation_instances_staging_buffer[ZEST_FIF], game->animation_instances_buffer->buffer[ZEST_FIF], GetAnimationInstancesSizeInBytes(animation_manager));
			}
		}
		else {
			animation_manager = &game->animation_manager_2d;
			zest_BindComputePipeline(compute, game->compute_pipeline_2d);
			if (game->using_staging_buffers) {
				zest_CopyBufferCB(zest_CurrentCommandBuffer(), game->offsets_staging_buffer[ZEST_FIF], game->offsets_buffer->buffer[ZEST_FIF], GetOffsetsSizeInBytes(animation_manager));
				zest_CopyBufferCB(zest_CurrentCommandBuffer(), game->animation_instances_staging_buffer[ZEST_FIF], game->animation_instances_buffer->buffer[ZEST_FIF], GetAnimationInstancesSizeInBytes(animation_manager));
			}
		}

		//We'll need the animation metrics to tell the compute shader how many animation instances we're rendering this frame
		auto metrics = GetAnimationBufferMetrics(animation_manager);

		//Update the push constants with some metrics. These are referenced in the compute shader.
		//The total number of animation instances that need to be drawn
		compute->push_constants.parameters.x = (float)metrics.instances_size;
		//If any animation instance contains animated shapes then set to 1. the compute shader can use this to avoid some unecessary
		//computation if all particle shapes are not animated
		compute->push_constants.parameters.y = float(animation_manager->flags & tfxAnimationManagerFlags_has_animated_shapes);
		//Set the total number of sprites that need to be processed by the shader
		compute->push_constants.parameters.z = (float)metrics.total_sprites_to_draw;
		compute->push_constants.parameters.w = (float)animation_manager->render_queue[0].offset_into_sprite_data;

		//Send the push constants in the compute object to the shader
		zest_SendComputePushConstants(compute);

		//The 128 here refers to the local_size_x in the shader and is how many elements each group will work on
		//For example if there are 1024 sprites, if we divide by 128 there will be 8 groups working on 128 sprites each in parallel
		zest_DispatchCompute(compute, (metrics.total_sprites_to_draw / 128) + 1, 1, 1);
	}

	//We want the compute shader to finish before the vertex shader is run so we put a barrier here.
	//zest_ComputeToVertexBarrier();
}
//----End of render specific code-----

//Initialise the example and create all the necessary buffers and objects for the compute shaders and load in the effects library
//and prepare the effect we'll use as an example.
void InitExample(ComputeExample *example) {
	//Create a texture in our renderer where we can store the particle shapes in the library
	example->particle_texture = zest_CreateTexturePacked("particle_shapes", zest_texture_format_rgba);
	//Reserve the space for our images so that the image pointers don't get lost (render specific)
	zest_SetTexturePackedBorder(example->particle_texture, 8);
	//Load the effects library and pass our shape loader callback function which loads the particle shape images into our texture
	auto result = LoadEffectLibraryPackage("examples/assets/instanced_effects.tfx", &example->library, ShapeLoader, example);
	//ListEffectNames(&example->library);
	assert(result == tfxErrorCode_success);		//Unable to load the effects library!
	//Render specific - process the images onto a texture and upload to the gpu
	zest_ProcessTextureImages(example->particle_texture);

	//Get an effect from the library to use as an example.
	tfx_effect_emitter_t *effect1 = GetLibraryEffect(&example->library, "Big Explosion");
	tfx_effect_emitter_t *effect2 = GetLibraryEffect(&example->library, "Star Burst Flash");
	tfx_effect_emitter_t *effect3 = GetLibraryEffect(&example->library, "EmissionSingleShot");
	tfx_effect_emitter_t *effect4 = GetLibraryEffect(&example->library, "Firework");

	//Render specific - set up a camera
	example->camera = zest_CreateCamera();
	zest_CameraPosition(&example->camera, {0.f, 0.f, 0.f});
	zest_CameraSetFoV(&example->camera, 60.f);
	zest_CameraSetYaw(&example->camera, zest_Radians(-90.f));
	zest_CameraSetPitch(&example->camera, zest_Radians(0.f));
	zest_CameraUpdateFront(&example->camera);

	//When we record an effect we can specify the camera position which only really makes sense if you're recording an effect
	//that has depth ordering.
	tfx_vec3_t camera_position_for_recording = tfx_vec3_t(example->camera.front.x, example->camera.front.y, example->camera.front.z);

	//Set up the maximum number of particles per layer which we pass into the InitParticleManager function
	tfxU32 layer_max_values[tfxLAYERS];
	layer_max_values[0] = 5000;
	layer_max_values[1] = 2500;
	layer_max_values[2] = 2500;
	layer_max_values[3] = 2500;
	//Initialise the particle manager for 3d effects. this is the particle manager that we'll use to record our animation for us.
	//Note: when we call RecordSpriteData3d the particle manager is reconfigured based on the effect we're using, meaning: reconfigure
	//for ordereded/unordered, 3d etc.
	tfx::InitParticleManagerFor3d(&example->pm, &example->library, layer_max_values, 1000, tfxParticleManagerMode_unordered, true, false, 2048);
	//Set up the camera in the particle manager
	tfx::SetPMCamera(&example->pm, &example->camera.front.x, &example->camera.position.x);

	//Initialise an animation manager. An animation manager maintains our animation instances for us and provides all the necessary metrics we'll
	//need to upload the buffers we need to both to initialise the compute shader and when we upload the offsets and instances buffer each frame.
	//Specify the maximum number of animation instances that you might want to play each frame
	tfx::InitialiseAnimationManagerFor3d(&example->animation_manager_3d, MAX_SPRITES);
	example->animation_manager_3d.maybe_render_instance_callback = CullAnimationInstancesCallback;

	//You must build the compute shape data before Adding sprite data. Pass the GetUV function which you will have to create based on your renderer.
	example->gpu_image_data = BuildGPUShapeData(GetParticleShapes(&example->library), GetUV);
	//Add all the sprite data of the pre-recorded effect that we made when we called RecordSpriteData3d earlier. You must call RecordSpriteData3d
	//for each effect animation that you want to add to the animation manager.
	tfx::AddSpriteData(&example->animation_manager_3d, effect1, &example->pm, camera_position_for_recording * -6.f);
	tfx::AddSpriteData(&example->animation_manager_3d, effect2, &example->pm, camera_position_for_recording * -6.f);
	tfx::AddSpriteData(&example->animation_manager_3d, effect3, &example->pm, camera_position_for_recording * -6.f);
	tfx::AddSpriteData(&example->animation_manager_3d, effect4, &example->pm, camera_position_for_recording * -6.f);

	//Render Specific - Create the 6 buffers needed for the compute shader. Create these after you've added all the effect animations you want to use
	//to the animation manager.
	/*
	1)	Sprite data buffer for all sprites in all frames of the animation. Use GetTotalSpriteDataCount() and/or GetSpriteDataSizeInBytes() to get the
		metrics you need to create the buffer
	*/
	example->sprite_data_buffer = zest_CreateStorageDescriptorBuffer(sizeof(tfx_sprite_data3d_t) * GetTotalSpriteDataCount(&example->animation_manager_3d), false);
	/*
	2)	Vertex buffer to contain all of the sprite instances that will be drawn each frame. This is the only buffer that the compute shader writes to
		each frame. Note that it need to be accessible to both the compute shader and the vertex shader
	*/
	example->sprite_buffer = zest_CreateComputeVertexDescriptorBuffer(sizeof(zest_billboard_instance_t) * MAX_SPRITES, false);;
	if (zest_GPUHasDeviceLocalHostVisible()) {
		example->offsets_buffer = zest_CreateCPUVisibleStorageDescriptorBuffer(sizeof(tfxU32) * MAX_INSTANCES, true);
		example->animation_instances_buffer = zest_CreateCPUVisibleStorageDescriptorBuffer(sizeof(tfx_animation_instance_t) * MAX_INSTANCES, true);
		example->using_staging_buffers = false;
	}
	else {
		example->offsets_buffer = zest_CreateStorageDescriptorBuffer(sizeof(tfxU32) * MAX_INSTANCES, true);
		example->animation_instances_buffer = zest_CreateStorageDescriptorBuffer(sizeof(tfx_animation_instance_t) * MAX_INSTANCES, true);
		for (ZEST_EACH_FIF_i) {
			example->offsets_staging_buffer[i] = zest_CreateStagingBuffer(sizeof(tfxU32) * MAX_INSTANCES, 0);
			example->animation_instances_staging_buffer[i] = zest_CreateStagingBuffer(sizeof(tfx_animation_instance_t) * MAX_INSTANCES, 0);
		}
		example->using_staging_buffers = true;
	}
	/*
	5)	Image data buffer contains data about all of the shapes being used in the library. Use GetComputeShapeCount and GetComputeShapeSizeInBytes to get the metrics you need
		to create the buffer.
	*/
	example->image_data_buffer = zest_CreateStorageDescriptorBuffer(sizeof(tfx_gpu_image_data_t) * 1000, false);
	/*
	6)	Emitter properties contains data about each emitter used to create the animation that the compute shader uses to build the sprite buffer.
		Use GetAnimationEmitterPropertyCount and GetAnimationEmitterPropertySizeInBytes to get the metrics you need to create the buffer.
	*/
	example->emitter_properties_buffer = zest_CreateStorageDescriptorBuffer(sizeof(tfx_animation_emitter_properties_t) * 100, false);

	example->draw_routine = zest_CreateDrawRoutine("Sprite drawing");
	example->draw_routine->draw_callback = DrawComputeSprites;
	example->draw_routine->user_data = example;
	UpdateSpriteResolution(example->draw_routine);

	//Upload the buffers to the GPU. These are the buffers that we only need to upload once before we do anything
	//They will containt the sprite, image and emitter data that the compute shader will index into to build each frame
	UploadBuffers(example);
	//Prepare the compute shader
	PrepareCompute(example);

	//Initialise Dear ImGui
	zest_imgui_Initialise(&example->imgui_layer_info);

	//Render specific -
	//In order to add our custom draw routine to the existing render queue, along with the compute shader we want to run, we can use AppendCommandQueue like so.
	zest_ModifyCommandQueue(ZestApp->default_command_queue);
	{
		//Set up the compute shader in the command queue and specify the callback function that will be called each frame to build
		//the command buffer for the compute shader
		zest_NewComputeSetup("Compute sprites", example->compute, SpriteComputeFunction);
		zest_ModifyDrawCommands(ZestApp->default_draw_commands);
		{
			zest_AddDrawRoutine(example->draw_routine);

			//Create a Dear ImGui layer
			zest_imgui_CreateLayer(&example->imgui_layer_info);
		}
		zest_FinishQueueSetup();
	}

	//Render specific - Set up the callback for updating the uniform buffers containing the model and view matrices
	UpdateUniform3d(example);

	//Set up a timer
	example->timer = zest_CreateTimer(60);

}

tfx_vec3_t ScreenRay(float x, float y, float depth_offset, zest_vec3 &camera_position) {
	zest_uniform_buffer_data_t *ubo_ptr = static_cast<zest_uniform_buffer_data_t*>(zest_GetUniformBufferData(ZestRenderer->standard_uniform_buffer));
	zest_vec3 camera_last_ray = zest_ScreenRay(x, y, zest_ScreenWidthf(), zest_ScreenHeightf(), &ubo_ptr->proj, &ubo_ptr->view);
	zest_vec3 pos = zest_AddVec3(zest_ScaleVec3(&camera_last_ray, depth_offset), camera_position);
	return { pos.x, pos.y, pos.z };
}

void BuildUI(ComputeExample *example) {
	//Must call the imgui GLFW implementation function
	ImGui_ImplGlfw_NewFrame();
	//Draw our imgui stuff
	ImGui::NewFrame();
	ImGui::Begin("Instanced Effects");
	ImGui::Text("FPS %i", ZestApp->last_fps);
	ImGui::Text("Active Instances: %i", example->animation_manager_3d.render_queue.size());
	ImGui::Text("Sprites Drawn: %i", example->animation_manager_3d.buffer_metrics.total_sprites_to_draw);
	ImGui::End();
	ImGui::Render();
	//Let the layer know that it needs to reupload the imgui mesh data to the GPU
	zest_SetLayerDirty(example->imgui_layer_info.mesh_layer);
	//Load the imgui mesh data into the layer staging buffers. When the command queue is recorded, it will then upload that data to the GPU buffers for rendering
	zest_imgui_UpdateBuffers(example->imgui_layer_info.mesh_layer);
}

bool CullAnimationInstancesCallback(tfx_animation_manager_t *animation_manager, tfx_animation_instance_t *instance, tfx_frame_meta_t *frame_meta, void *user_data) {
	return true;
}

//Our main update function that's run every frame.
void Update(zest_microsecs elapsed, void *data) {
	ComputeExample *example = static_cast<ComputeExample*>(data);
	zest_SetActiveCommandQueue(ZestApp->default_command_queue);
	UpdateUniform3d(example);

	if (!example->left_mouse_clicked && ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
		//tfxEffectID id = AddEffectToParticleManager(&example->pm, example->starburst);
		tfx_vec3_t position = ScreenRay(zest_MouseXf(), zest_MouseYf(), 6.f, example->camera.position);
		//SetEffectPosition(&example->pm, id, { position.x, position.y, position.z });
		tfxAnimationID anim_id = AddAnimationInstance(&example->animation_manager_3d, "Big Explosion");
		if (anim_id != tfxINVALID) {
			SetAnimationPosition(&example->animation_manager_3d, anim_id, &position.x);
			SetAnimationScale(&example->animation_manager_3d, anim_id, RandomRange(&example->pm.random, 0.5f, 1.5f));
		}
		example->left_mouse_clicked = true;
	}
	else if(!ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
		example->left_mouse_clicked = false;
	}

	if (!example->right_mouse_clicked && ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
		//tfxEffectID id = AddEffectToParticleManager(&example->pm, example->starburst);
		tfx_vec3_t position = ScreenRay(zest_MouseXf(), zest_MouseYf(), 6.f, example->camera.position);
		//SetEffectPosition(&example->pm, id, { position.x, position.y, position.z });
		tfxAnimationID anim_id = AddAnimationInstance(&example->animation_manager_3d, "Smokey Explosion");
		if (anim_id != tfxINVALID) {
			SetAnimationPosition(&example->animation_manager_3d, anim_id, &position.x);
			SetAnimationScale(&example->animation_manager_3d, anim_id, 1.f);
		}
		example->right_mouse_clicked = true;
	}
	else if(!ImGui::IsMouseDown(ImGuiMouseButton_Right)) {
		example->right_mouse_clicked = false;
	}

	if (ImGui::IsKeyDown(ImGuiKey_Space)) {
		ClearAllAnimationInstances(&example->animation_manager_3d);
	}

	example->trigger_effect += elapsed;
	if (example->trigger_effect >= ZEST_MILLISECONDS_IN_MICROSECONDS(25)) {
		tfxAnimationID anim_id = tfxINVALID;
		int r = RandomRange(&example->pm.random, 0, 3);
		if (r == 0) {
			anim_id = AddAnimationInstance(&example->animation_manager_3d, "Big Explosion");
		} else if (r == 1) {
			anim_id = AddAnimationInstance(&example->animation_manager_3d, "Star Burst Flash");
		} else if (r == 2) {
			anim_id = AddAnimationInstance(&example->animation_manager_3d, "EmissionSingleShot");
		} else if (r == 3) {
			anim_id = AddAnimationInstance(&example->animation_manager_3d, "Firework");
		}
		if (anim_id != tfxINVALID) {
			tfx_vec3_t position = ScreenRay(RandomRange(&example->pm.random, 0.f, zest_ScreenWidthf()), RandomRange(&example->pm.random, 0.f, zest_ScreenHeightf()), 12.f, example->camera.position);
			SetAnimationPosition(&example->animation_manager_3d, anim_id, &position.x);
			SetAnimationScale(&example->animation_manager_3d, anim_id, RandomRange(&example->pm.random, 0.75f, 1.5f));
		}
		example->trigger_effect = 0;
	}


	/* For debugging:
	float current_time = fMouseX() / fScreenWidth() * example->anim_test.animation_length_in_time;
	for (auto index : example->animation_manager_3d.instances_in_use[example->animation_manager_3d.current_in_use_buffer]) {
		auto &instance = example->animation_manager_3d.instances[index];
		//instance.current_time = 591.744751f;
		//instance.current_time = 805.677063f;
		//instance.current_time = current_time;
	}
	*/

	zest_TimerAccumulate(example->timer);
	while (zest_TimerDoUpdate(example->timer)) {
		//example->pm.Update();
		BuildUI(example);

		zest_TimerUnAccumulate(example->timer);
	}

	//Update the animation manager each frame. This will advance the time of each animation instance that's currently playing
	//Pass the amount of time that has elapsed since the last frame
	UpdateAnimationManager(&example->animation_manager_3d, elapsed / 1000.f);

	//Copy the offsets and animation instances to the staging buffers. These will then be uploaded to the GPU just before the compute
	//shader is dispatched.

    if (example->using_staging_buffers) {
        memcpy(example->offsets_staging_buffer[ZEST_FIF]->data, example->animation_manager_3d.offsets.data, GetOffsetsSizeInBytes(&example->animation_manager_3d));
        memcpy(example->animation_instances_staging_buffer[ZEST_FIF]->data, example->animation_manager_3d.render_queue.data, GetAnimationInstancesSizeInBytes(&example->animation_manager_3d));
    }
    else {
        memcpy(example->offsets_buffer->buffer[ZEST_FIF]->data, example->animation_manager_3d.offsets.data, GetOffsetsSizeInBytes(&example->animation_manager_3d));
        memcpy(example->animation_instances_buffer->buffer[ZEST_FIF]->data, example->animation_manager_3d.render_queue.data, GetAnimationInstancesSizeInBytes(&example->animation_manager_3d));
    }
	zest_TimerSet(example->timer);

	//RenderSpriteDataFrame3d(&example->animation_manager_3d, example, &GetLayer());
}

//int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR pCmdLine, int nCmdShow) {
int main() {
	//Render specific
	//When initialising a qulkan app, you can pass a QulkanCreateInfo which you can use to configure some of the base settings of the app
	//Create new config struct for Zest
	zest_create_info_t create_info = zest_CreateInfo();
	//Don't enable vsync so we can see the FPS go higher then the refresh rate
	ZEST__UNFLAG(create_info.flags, zest_init_flag_enable_vsync);
	//Implement GLFW for window creation
	zest_implglfw_SetCallbacks(&create_info);

	//Initialise TimelineFX. Must be run before using any timeline fx functionality.
	InitialiseTimelineFX(std::thread::hardware_concurrency());

	ComputeExample example;

	//Initialise Zest
	zest_Initialise(&create_info);
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
