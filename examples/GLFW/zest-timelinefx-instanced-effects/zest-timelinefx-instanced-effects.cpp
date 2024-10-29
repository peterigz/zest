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
#define MAX_SPRITES	250000

#define BufferCast(struct_name, buffer_name) static_cast<struct_name*>(buffer_name.mapped)

//Before you load an effects file, you will need to define a ShapeLoader function that passes the following parameters:
//const char* filename			- this will be the filename of the image being loaded from the library. You don't have to do anything with this if you don't need to.
//ImageData	&image_data			- A struct containing data about the image. You will have to set image_data.ptr to point to the texture in your renderer for later use in the Render function that you will create to render the particles
//void *raw_image_data			- The raw data of the image which you can use to load the image into graphics memory
//int image_memory_size			- The size in bytes of the raw_image_data
//void *custom_data				- This allows you to pass through an object you can use to access whatever is necessary to load the image into graphics memory, depending on the renderer that you're using
void ShapeLoader(const char* filename, tfx_image_data_t *image_data, void *raw_image_data, int image_memory_size, void *custom_data) {
	//Cast your custom data, this can be anything you want
	ComputeExample *example = static_cast<ComputeExample*>(custom_data);

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
		zest_image anim = zest_AddTextureAnimationBitmap(example->tfx_rendering.particle_texture, &bitmap, (tfxU32)image_data->image_size.x, (tfxU32)image_data->image_size.y, (tfxU32)image_data->animation_frames, &max_radius, 1);
		//Important step: you need to point the ImageData.ptr to the appropriate handle in the renderer to point to the texture of the particle shape
		//You'll need to use this in your render function to tell your renderer which texture to use to draw the particle
		image_data->ptr = anim;
	}
	else {
		//Add the bitmap to the texture in our renderer
		zest_image image = zest_AddTextureImageBitmap(example->tfx_rendering.particle_texture, &bitmap);
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
void GetUV(void *ptr, tfx_gpu_image_data_t *image_data, int offset) {
	zest_image image = (static_cast<zest_image>(ptr) + offset);
	image_data->uv = { image->uv.x, image->uv.y, image->uv.z, image->uv.w };
	image_data->texture_array_index = image->layer;
	image_data->max_radius = image->max_radius;
}

//---Render specific functions---
//The following code will depend on the renderer that you're using

//Update the uniform buffer used to transform vertices in the vertex buffer
void UpdateUniform3d(ComputeExample *example) {
	zest_uniform_buffer_data_t *ubo_ptr = static_cast<zest_uniform_buffer_data_t*>(zest_GetUniformBufferData(example->tfx_rendering.uniform_buffer_3d));
	ubo_ptr->view = zest_LookAt(example->camera.position, zest_AddVec3(example->camera.position, example->camera.front), example->camera.up);
	ubo_ptr->proj = zest_Perspective(example->camera.fov, zest_ScreenWidthf() / zest_ScreenHeightf(), 0.1f, 10000.f);
	ubo_ptr->proj.v[1].y *= -1.f;
	ubo_ptr->screen_size.x = zest_ScreenWidthf();
	ubo_ptr->screen_size.y = zest_ScreenHeightf();
	ubo_ptr->millisecs = 0;
	//Calculate the frustum planes
	zest_CalculateFrustumPlanes(&ubo_ptr->view, &ubo_ptr->proj, example->planes);
}

//Send the command to the GPU to draw all of the sprites for all of the animation instances that are currently in play
int RecordComputeSprites(zest_draw_routine routine, VkCommandBuffer unused) {
	ComputeExample &example = *static_cast<ComputeExample*>(routine->user_data);
	VkCommandBuffer command_buffer = zest_BeginRecording(routine->recorder, routine->draw_commands->render_pass, ZEST_FIF);

	zest_SetViewport(command_buffer, routine);

	//Bind the buffer that contains the sprite instances to draw. These are updated by the compute shader on the GPU
	zest_BindVertexBuffer(command_buffer, example.sprite_buffer->buffer[0]);

	//Draw all the sprites in the buffer that is built by the compute shader
	zest_BindPipelineShaderResource(command_buffer, example.tfx_rendering.pipeline, example.tfx_rendering.shader_resource, ZEST_FIF);
	zest_SendPushConstants(command_buffer, example.tfx_rendering.pipeline, VK_SHADER_STAGE_VERTEX_BIT, sizeof(zest_push_constants_t), &example.push_contants);
	zest_Draw(command_buffer, 6, GetTotalSpritesThatNeedDrawing(&example.animation_manager_3d), 0, 0);
	
	zest_EndRecording(routine->recorder, ZEST_FIF);
	return 1;
}

//A callback for when the window size is changed so we can update the layer push constants that contain the current screen size
void UpdateSpriteResolution(zest_draw_routine routine) {
	ComputeExample &example = *static_cast<ComputeExample*>(routine->user_data);
	example.push_contants.parameters1 = zest_Vec4Set(2.0f / (float)zest_GetSwapChainExtent().width, 2.0f / (float)zest_GetSwapChainExtent().height, -1.f, -1.f);
	example.push_contants.model = zest_M4(1.f);
}

//Upload the buffers to the GPU that the compute shader will need to read from in order to build the sprite buffer each frame. These buffers
//only need to be uploaded once once you've added all the sprite data for all the animations that you want to use.
void UploadBuffers(ComputeExample *example) {

	tfx_animation_manager_t *animation_manager = nullptr;
	animation_manager = &example->animation_manager_3d;

	//Upload the sprite data. This contains all pre-recorded sprites for all animations that you might want to play.
	zest_buffer staging_buffer = zest_CreateStagingBuffer(GetSpriteDataSizeInBytes(animation_manager), GetSpriteDataBufferPointer(animation_manager));
	zest_CopyBuffer(staging_buffer, example->sprite_data_buffer->buffer[0], GetSpriteDataSizeInBytes(animation_manager));
	zest_FreeBuffer(staging_buffer);

	//Upload the emitter properties for each animation you're using. This contains the sprite handle and any flags that might be relavent.
	zest_buffer emitter_properties_buffer = zest_GetBufferFromDescriptorBuffer(example->emitter_properties_buffer);
	staging_buffer = zest_CreateStagingBuffer(GetAnimationEmitterPropertySizeInBytes(animation_manager), GetAnimationEmitterPropertiesBufferPointer(animation_manager));
	zest_CopyBuffer(staging_buffer, emitter_properties_buffer, GetAnimationEmitterPropertySizeInBytes(animation_manager));
	zest_FreeBuffer(staging_buffer);

	//Upload the image data containing all the UV coords and texture array index that the compute shader will use to build the sprite
	//buffer each frame
	staging_buffer = zest_CreateStagingBuffer(GetGPUShapesSizeInBytes(&example->gpu_image_data), GetGPUShapesPointer(&example->gpu_image_data));
	zest_CopyBuffer(staging_buffer, example->image_data_buffer->buffer[0], GetGPUShapesSizeInBytes(&example->gpu_image_data));
	zest_FreeBuffer(staging_buffer);
}

//Prepare the compute shader that will be used to playback the effects in the animation manager
void PrepareComputeForEffectPlayback(ComputeExample *example) {
	//Register a new compute shader
	example->compute = zest_CreateCompute("Sprite data compute");

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
	zest_AddComputeBufferForBinding(&builder, example->sprite_data_buffer);					//Binding 0
	zest_AddComputeBufferForBinding(&builder, example->offsets_buffer);						//Binding 1
	zest_AddComputeBufferForBinding(&builder, example->animation_instances_buffer);			//Binding 2
	zest_AddComputeBufferForBinding(&builder, example->image_data_buffer);					//Binding 3
	zest_AddComputeBufferForBinding(&builder, example->emitter_properties_buffer);			//Binding 4
	zest_AddComputeBufferForBinding(&builder, example->sprite_buffer);						//Binding 5
	//Specify the shader that we want to use for the compute shader
	example->compute_pipeline_3d = zest_AddComputeShader(&builder, "sprite_data_playback.comp.spv", "examples/assets/spv/");
	//Specify the size of the push constant struct if we're using it
	zest_SetComputePushConstantSize(&builder, sizeof(zest_compute_push_constant_t));
	//Set the user data that can be passed to the callbacks
	zest_SetComputeUserData(&builder, example);
	zest_SetComputeDescriptorUpdateCallback(&builder, zest_StandardComputeDescriptorUpdate);
	//Finalise the compute set up by calling MakeCompute which creates all the boiler plate code to create the
	//compute pipeline in Vulkan
	zest_MakeCompute(&builder, example->compute);
}

//Prepare the compute shader that will calculate the bounding boxes of each frame of animation. This is only necessary
//if you are recording the effects. Otherwise you can export the animations from TimelineFX and the bounding boxes will
//already be calculated in the tfxsd file.
void PrepareComputeForBoundingBoxCalculation(ComputeExample *example) {
	//Register a new compute shader
	example->bounding_box_compute = zest_CreateCompute("Sprite data bb compute");

	//Utilize a ComputeBuilder to make setting up the compute shader a lot easier
	zest_compute_builder_t builder = zest_NewComputeBuilder();
	//Add layout binding for the buffers that we'll need to use in the compute shader
	zest_AddComputeLayoutBinding(&builder, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 0);
	zest_AddComputeLayoutBinding(&builder, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1);
	zest_AddComputeLayoutBinding(&builder, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 2);
	zest_AddComputeLayoutBinding(&builder, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 3);
	zest_AddComputeLayoutBinding(&builder, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 4);
	//Add the bindings for the buffers we've created
	zest_AddComputeBufferForBinding(&builder, example->sprite_data_buffer);					//Binding 0
	zest_AddComputeBufferForBinding(&builder, example->offsets_buffer);						//Binding 1
	zest_AddComputeBufferForBinding(&builder, example->animation_instances_buffer);			//Binding 2
	zest_AddComputeBufferForBinding(&builder, example->emitter_properties_buffer);			//Binding 3
	zest_AddComputeBufferForBinding(&builder, example->bounding_boxes);						//Binding 4
	//Specify the shader that we want to use for the compute shader
	example->bb_compute_pipeline_3d = zest_AddComputeShader(&builder, "sprite_data_bounding_box.comp.spv", "examples/assets/spv/");
	//Specify the size of the push constant struct if we're using it
	zest_SetComputePushConstantSize(&builder, sizeof(zest_compute_push_constant_t));
	//Set the user data that can be passed to the callbacks
	zest_SetComputeUserData(&builder, example);
	zest_SetComputeDescriptorUpdateCallback(&builder, zest_StandardComputeDescriptorUpdate);
	zest_SetComputeCommandBufferUpdateCallback(&builder, BoundingBoxComputeFunction);
	//Finalise the compute set up by calling MakeCompute which creates all the boiler plate code to create the
	//compute pipeline in Vulkan
	zest_MakeCompute(&builder, example->bounding_box_compute);
}

//Every frame the compute shader needs to be dispatched which means that all the commands for the compute shader
//need to be added to the command buffer
void SpriteComputeFunction(zest_command_queue_compute compute_routine) {

	//The compute queue item can contain more then one compute shader to be dispatched but in this case there's only
	//one which is the effect playback compute shader
	zest_compute compute;
	while (compute = zest_NextComputeRoutine(compute_routine)) {
		//Grab our ComputeExample struct out of the user data
		ComputeExample *example = static_cast<ComputeExample*>(compute->user_data);

		//We'll need the animation metrics to tell the compute shader how many animation instances we're rendering this frame
		auto metrics = GetAnimationBufferMetrics(&example->animation_manager_3d);
		if (metrics.total_sprites_to_draw == 0) {
			continue;
		}

		//Bind the compute shader pipeline
		zest_BindComputePipeline(compute, example->compute_pipeline_3d);
		//Some graphics cards don't support direct writing to the GPU buffer so we have to copy to a staging buffer first, then
		//from there we copy to the GPU.
		zest_CopyBufferCB(zest_CurrentCommandBuffer(), example->offsets_staging_buffer[ZEST_FIF], example->offsets_buffer->buffer[ZEST_FIF], GetOffsetsSizeInBytes(&example->animation_manager_3d), 1);
		zest_CopyBufferCB(zest_CurrentCommandBuffer(), example->animation_instances_staging_buffer[ZEST_FIF], example->animation_instances_buffer->buffer[ZEST_FIF], GetAnimationInstancesSizeInBytes(&example->animation_manager_3d), 1);

		//Update the push constants with some metrics. These are referenced in the compute shader.
		//The total number of animation instances that need to be drawn
		compute->push_constants.parameters.x = (float)metrics.instances_size;
		//If any animation instance contains animated shapes then set to 1. The compute shader can use this to avoid some unecessary
		//computation if all particle shapes are not animated
		compute->push_constants.parameters.y = float(example->animation_manager_3d.flags & tfxAnimationManagerFlags_has_animated_shapes);
		//Set the total number of sprites that need to be processed by the shader
		compute->push_constants.parameters.z = (float)metrics.total_sprites_to_draw;
		//Set the offset into the sprite data that the sprites start at
		compute->push_constants.parameters.w = (float)example->animation_manager_3d.render_queue[0].offset_into_sprite_data;

		//Send the push constants in the compute object to the shader
		zest_SendComputePushConstants(ZestRenderer->current_command_buffer, compute);

		//The 128 here refers to the local_size_x in the shader and is how many elements each group will work on
		//For example if there are 1024 sprites, if we divide by 128 there will be 8 groups working on 128 sprites each in parallel
		zest_DispatchCompute(compute, (metrics.total_sprites_to_draw / 128) + 1, 1, 1);
	}

	//We want the compute shader to finish before the vertex shader is run so we put a barrier here.
	//zest_ComputeToVertexBarrier();
}

//This function is the callback function that will be used when we call zest_RunCompute. This is used when we want to run a compute
//shader just the once to calculate something, in this case the bounding box calculation for every frame of animation in each effect
//that we're using.
void BoundingBoxComputeFunction(zest_compute compute, VkCommandBuffer command_buffer) {
	//The compute queue item can contain more then one compute shader to be dispatched
	//There's only one in this editor though which we created in the PrepareCompute function
	ComputeExample *example = static_cast<ComputeExample*>(compute->user_data);

	//Bind the pipeline and specify the command buffer because this will be a one off call of the compute shader, rather than
	//a compute shader that is run every frame in a rendering pipeline
	zest_BindComputePipelineCB(command_buffer, compute, example->bb_compute_pipeline_3d);

	//We'll need the animation metrics to tell the compute shader how many animation instances we're rendering this frame
	auto metrics = GetAnimationBufferMetrics(&example->animation_manager_3d);

	//Update the push constants with some metrics. These are referenced in the compute shader.
	//The total number of animation instances that need to be drawn
	compute->push_constants.parameters.x = (float)metrics.instances_size;
	//If any animation instance contains animated shapes then set to 1. the compute shader can use this to avoid some unecessary
	//computation if all particle shapes are not animated
	compute->push_constants.parameters.y = float(example->animation_manager_3d.flags & tfxAnimationManagerFlags_has_animated_shapes);
	//Set the total number of sprites that need to be processed by the shader
	compute->push_constants.parameters.z = (float)metrics.total_sprites_to_draw;

	//Send the push constants in the compute object to the shader
	zest_SendComputePushConstants(command_buffer, compute);

	//Note that the compute shader group size is 128, but because of the parallel reduction that we're doing in the compute shader we
	//actually make the group count half what it would normally be because we perform the first comparison on the first and second 128
	//sprites. So effectively each group will actually be working on 256 sprites each. Look in the compute shader for more info on this.
	tfxU32 frame_group_count = (metrics.total_sprites_to_draw / 256) + 1;
	zest_DispatchComputeCB(command_buffer, compute, frame_group_count, 1, 1);
}

void InitTimelineFXRenderResources(tfx_render_resources_t &render_resources, const char *library_path) {
	render_resources.uniform_buffer_3d = zest_CreateUniformBuffer("3d uniform", sizeof(zest_uniform_buffer_data_t));
	render_resources.uniform_buffer_descriptor_set = zest_CreateUniformDescriptorSet(render_resources.uniform_buffer_3d);

	int shape_count = GetShapeCountInLibrary(library_path);
	render_resources.particle_texture = zest_CreateTexture("Particle Texture", zest_texture_storage_type_packed, zest_texture_flag_use_filtering, zest_texture_format_rgba, shape_count);
	render_resources.color_ramps_texture = zest_CreateTextureBank("Particle Color Ramps", zest_texture_format_rgba);
	zest_SetTextureUseFiltering(render_resources.color_ramps_texture, false);

	//Compile the shaders we will use to render the particles
	shaderc_compiler_t compiler = shaderc_compiler_initialize();
	render_resources.fragment_shader = zest_CreateShaderFromFile("examples/assets/shaders/timelinefxstatic.frag", "tfx_frag.spv", shaderc_fragment_shader, false, compiler);
	render_resources.vertex_shader = zest_CreateShaderFromFile("examples/assets/shaders/timelinefx3dstatic.vert", "tfx_vertex3d.spv", shaderc_vertex_shader, false, compiler);
	shaderc_compiler_release(compiler);

	//To render the particles we setup a pipeline with the vertex attributes and shaders to render the particles.
	//First create a descriptor set layout, we need 2 samplers, one to sample the particle texture and another to sample the color ramps
	//We also need 2 storage buffers, one to access the image data in the vertex shader and the other to access the previous frame particles
	//so that they can be interpolated in between updates
	render_resources.descriptor_layout = zest_AddDescriptorLayout("2 samplers 1 storage", zest_CreateDescriptorSetLayout(0, 1, 2));

	zest_pipeline_template_create_info_t instance_create_info = zest_CreatePipelineTemplateCreateInfo();
	instance_create_info.viewport.extent = zest_GetSwapChainExtent();
	//Set up the vertex attributes that will take in all of the billboard data stored in tfx_billboard_instance_t objects
	zest_AddVertexInputBindingDescription(&instance_create_info, 0, sizeof(tfx_billboard_instance_t), VK_VERTEX_INPUT_RATE_INSTANCE);
	zest_AddVertexInputDescription(&instance_create_info.attributeDescriptions, zest_CreateVertexInputDescription(0, 0, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(tfx_billboard_instance_t, position)));	            // Location 0: Postion and stretch in w
	zest_AddVertexInputDescription(&instance_create_info.attributeDescriptions, zest_CreateVertexInputDescription(0, 1, VK_FORMAT_R32G32B32_SFLOAT, offsetof(tfx_billboard_instance_t, rotations)));	            // Location 1: Rotations
	zest_AddVertexInputDescription(&instance_create_info.attributeDescriptions, zest_CreateVertexInputDescription(0, 2, VK_FORMAT_R8G8B8_SNORM, offsetof(tfx_billboard_instance_t, alignment)));					// Location 2: Alignment
	zest_AddVertexInputDescription(&instance_create_info.attributeDescriptions, zest_CreateVertexInputDescription(0, 3, VK_FORMAT_R16G16B16A16_SSCALED, offsetof(tfx_billboard_instance_t, size_handle)));		    // Location 3: Size and handle of the sprite
	zest_AddVertexInputDescription(&instance_create_info.attributeDescriptions, zest_CreateVertexInputDescription(0, 4, VK_FORMAT_R16G16_SSCALED, offsetof(tfx_billboard_instance_t, intensity_life)));    		    // Location 4: 2 intensities for each color
	zest_AddVertexInputDescription(&instance_create_info.attributeDescriptions, zest_CreateVertexInputDescription(0, 5, VK_FORMAT_R16G16_SNORM, offsetof(tfx_billboard_instance_t, curved_alpha)));               	// Location 5: Sharpness and mix lerp value
	zest_AddVertexInputDescription(&instance_create_info.attributeDescriptions, zest_CreateVertexInputDescription(0, 6, VK_FORMAT_R32_UINT, offsetof(tfx_billboard_instance_t, indexes)));							// Location 6: texture indexes to sample the correct image and color ramp
	zest_AddVertexInputDescription(&instance_create_info.attributeDescriptions, zest_CreateVertexInputDescription(0, 7, VK_FORMAT_R32_UINT, offsetof(tfx_billboard_instance_t, captured_index)));   				// Location 7: index of the sprite in the previous buffer when double buffering
	//Set the shaders to our custom timelinefx shaders
	zest_SetPipelineTemplateVertShader(&instance_create_info, "tfx_vertex3d.spv", 0);
	zest_SetPipelineTemplateFragShader(&instance_create_info, "tfx_frag.spv", 0);
	zest_SetPipelineTemplatePushConstant(&instance_create_info, sizeof(zest_push_constants_t), 0, VK_SHADER_STAGE_VERTEX_BIT);
	zest_AddPipelineTemplateDescriptorLayout(&instance_create_info, render_resources.descriptor_layout->vk_layout);
	render_resources.pipeline = zest_AddPipeline("tfx_billboard_pipeline");
	zest_MakePipelineTemplate(render_resources.pipeline, zest_GetStandardRenderPass(), &instance_create_info);
	render_resources.pipeline->pipeline_template.colorBlendAttachment = zest_PreMultiplyBlendState();
	render_resources.pipeline->pipeline_template.depthStencil.depthWriteEnable = VK_FALSE;
	render_resources.pipeline->pipeline_template.depthStencil.depthTestEnable = true;
	zest_BuildPipeline(render_resources.pipeline);

	//Set up the draw layers we need in the renderer
	zest_SetDrawCommandsClsColor(zest_GetCommandQueueDrawCommands("Default Draw Commands"), 0.f, 0.f, .2f, 1.f);

	//Create a buffer to store the image data on the gpu. Note that we don't need this buffer to have multiple frames in flight
	render_resources.image_data = zest_CreateStorageDescriptorBuffer(sizeof(tfx_gpu_image_data_t) * 1000, false);

	//End of render specific code
}

void UpdateTimelineFXImageData(tfx_render_resources_t &tfx_rendering, tfx_library_t &library) {
	//Upload the timelinefx image data to the image data buffer created
	zest_buffer image_data_buffer = zest_GetBufferFromDescriptorBuffer(tfx_rendering.image_data);
	zest_buffer staging_buffer = zest_CreateStagingBuffer(GetGPUShapesSizeInBytes(&library.gpu_shapes), GetGPUShapesPointer(&library.gpu_shapes));
	zest_CopyBuffer(staging_buffer, zest_GetBufferFromDescriptorBuffer(tfx_rendering.image_data), GetGPUShapesSizeInBytes(&library.gpu_shapes));
	zest_FreeBuffer(staging_buffer);
}

void CreateTimelineFXShaderResources(tfx_render_resources_t &tfx_rendering) {
	//We need a descriptor set for the shader resources that we will use in the pipeline we created above
	zest_descriptor_set_builder_t set_builder = zest_NewDescriptorSetBuilder();
	zest_AddBuilderDescriptorWriteStorageBuffer(&set_builder, tfx_rendering.image_data, 0);
	zest_AddBuilderDescriptorWriteImage(&set_builder, zest_GetTextureDescriptorImageInfo(tfx_rendering.particle_texture), 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
	zest_AddBuilderDescriptorWriteImage(&set_builder, zest_GetTextureDescriptorImageInfo(tfx_rendering.color_ramps_texture), 2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
	tfx_rendering.descriptor_set = zest_BuildDescriptorSet(&set_builder, tfx_rendering.descriptor_layout, zest_descriptor_type_dynamic);

	//Finally, set up a shader resource to be used when sending the draw calls to the gpu in our render function
	//This will have the uniform buffer in set 0 and the texures and storage buffers in set 1
	tfx_rendering.shader_resource = zest_CreateShaderResources();
	zest_AddDescriptorSetToResources(tfx_rendering.shader_resource, tfx_rendering.uniform_buffer_descriptor_set);
	zest_AddDescriptorSetToResources(tfx_rendering.shader_resource, &tfx_rendering.descriptor_set);
}
//----End of render specific functions-----

//Initialise the example and create all the necessary buffers and objects for the compute shaders and load in the effects library
//and prepare the effect we'll use as an example.
void InitExample(ComputeExample *example) {
	InitTimelineFXRenderResources(example->tfx_rendering, "examples/assets/instanced_effects.tfx");

	//Create a new texture for storing a floor texture, use storage type bank so we can use texture wrapping.
	example->floor_texture = zest_CreateTexture("Floor texture", zest_texture_storage_type_bank, zest_texture_flag_use_filtering, zest_texture_format_rgba, 10);
	//Load in the floor image from file and add it to the texture
	example->floor_image = zest_AddTextureImageFile(example->floor_texture, "examples/assets/checker.png");
	//Process the texture which will create the resources on the GPU for sampling from the floor image texture
	zest_ProcessTextureImages(example->floor_texture);
	//Store the handle to the builtin mesh pipeline for convenience
	example->mesh_pipeline = zest_Pipeline("pipeline_mesh");
	example->floor_resources = zest_CombineUniformAndTextureSampler(example->tfx_rendering.uniform_buffer_descriptor_set, example->floor_texture);

	//Load the effects library and pass our shape loader callback function which loads the particle shape images into our texture
	auto result = LoadEffectLibrary("examples/assets/instanced_effects.tfx", &example->library, ShapeLoader, GetUV, example);
	assert(result == tfxErrorCode_success);		//Unable to load the effects library!

	//Process the texture with all the particle shapes that we just added to
	zest_ProcessTextureImages(example->tfx_rendering.particle_texture);

	//Add the color ramps from the library to the color ramps texture. Color ramps in the library are stored in rgba format and can be
	//simply copied to a bitmap for uploading to the texture
	for (tfx_bitmap_t &bitmap : example->library.color_ramp_bitmaps) {
		zest_bitmap_t temp_bitmap = zest_CreateBitmapFromRawBuffer("", bitmap.data, (int)bitmap.size, bitmap.width, bitmap.height, bitmap.channels);
		zest_AddTextureImageBitmap(example->tfx_rendering.color_ramps_texture, &temp_bitmap);
	}
	//Process the color ramp texture to upload it all to the gpu
	zest_ProcessTextureImages(example->tfx_rendering.color_ramps_texture);
	//Now that the particle shapes have been setup in the renderer, we can call this function to update the shape data in the library
	//with the correct uv texture coords ready to upload to gpu. This buffer will be accessed in the vertex shader when rendering.
	UpdateLibraryGPUImageData(&example->library);

	//Now upload the image data to the GPU and set up the shader resources ready for rendering
	UpdateTimelineFXImageData(example->tfx_rendering, example->library);
	CreateTimelineFXShaderResources(example->tfx_rendering);

	//Get some effects from the library which we will record as a pre-baked particle animation. The settings for each effect
	//such as how much compression to use is saved in the TimelineFX editor.
	//tfx_effect_emitter_t *effect1 = GetLibraryEffect(&example->library, "Test 1");
	//tfx_effect_emitter_t *effect2 = GetLibraryEffect(&example->library, "Test 2");
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
	//that has depth ordering, otherwise it won't matter much
	tfx_vec3_t camera_position_for_recording = tfx_vec3_t(example->camera.front.x, example->camera.front.y, example->camera.front.z);

	//Initialise the particle manager for 3d effects. this is the particle manager that we'll use to record our animation for us.
	//Note: when we call RecordSpriteData3d the particle manager is reconfigured based on the effect we're using, meaning: reconfigure
	//for ordereded/unordered, 3d etc.
	tfx_particle_manager_info_t pm_info = CreateParticleManagerInfo(tfxParticleManagerSetup_3d_unordered);
	InitializeParticleManager(&example->pm, &example->library, pm_info);
	//Set up the camera in the particle manager
	tfx::SetPMCamera(&example->pm, &example->camera.front.x, &example->camera.position.x);

	//Initialise an animation manager. An animation manager maintains our animation instances for us and provides all the necessary metrics we'll
	//need to upload the buffers we need to both to initialise the compute shader and when we upload the offsets and instances buffer each frame.
	//Specify the maximum number of animation instances that you might want to play each frame
	tfx::InitialiseAnimationManagerFor3d(&example->animation_manager_3d, MAX_INSTANCES, MAX_SPRITES);
	//Here we set the callback that will be used to decide if an animation should be drawn or not. We use the bounding box to check if it's inside
	//the view frustum and cull it if it's not.
	example->animation_manager_3d.maybe_render_instance_callback = CullAnimationInstancesCallback;
	//Set the user data to the ComputeExample which we can use in the callback function
	SetAnimationManagerUserData(&example->animation_manager_3d, example);

	//Record the effects we want and calculate the bounding boxes
	example->record_time = zest_Millisecs();		//See how long it takes to record.

	//You MUST build the compute shape data before Adding sprite data. Pass the GetUV function which you will have to create based on your renderer.
	BuildGPUShapeData(GetParticleShapes(&example->library), &example->gpu_image_data, GetUV);
	//Add all the sprite data of the pre-recorded effect that we made when we called RecordSpriteData3d earlier. You must call RecordSpriteData3d
	//for each effect animation that you want to add to the animation manager.
	tfx_sprite_data_settings_t &anim = example->library.sprite_data_settings[GetEffectInfo(effect2)->sprite_data_settings_index];
	tfx::AddSpriteData(&example->animation_manager_3d, effect2, &example->pm, camera_position_for_recording);
	tfx::AddSpriteData(&example->animation_manager_3d, effect1, &example->pm, camera_position_for_recording);
	tfx::AddSpriteData(&example->animation_manager_3d, effect3, &example->pm, camera_position_for_recording);
	tfx::AddSpriteData(&example->animation_manager_3d, effect4, &example->pm, camera_position_for_recording);

	//Render Specific - Create the 6 buffers needed for the compute shader. Create these after you've added all the effect animations you want to use
	//to the animation manager.
	/*
	1)	Sprite data buffer for all sprites in all frames of the animation. Use GetTotalSpriteDataCount() and/or GetSpriteDataSizeInBytes() to get the
		metrics you need to create the buffer. Note that we've already recorded the sprite data so we know the size of the buffer we need, otherwise
		you might have to resize the buffer later and update the descriptor sets
	*/
	example->sprite_data_buffer = zest_CreateStorageDescriptorBuffer(sizeof(tfx_sprite_data3d_t) * GetTotalSpriteDataCount(&example->animation_manager_3d), false);
	/*
	2)	Vertex buffer to contain all of the sprite instances that will be drawn each frame. This is the only buffer that the compute shader writes to
		each frame. Note that it needs to be accessible to both the compute shader and the vertex shader
	*/
	example->sprite_buffer = zest_CreateComputeVertexDescriptorBuffer(sizeof(zest_billboard_instance_t) * MAX_SPRITES, false);

	/*
	3)	Create the 2 buffers that need writing to from the CPU, this is the offset's buffer containing the index offset for each instance into the sprite data and
		the actually instances data containing position, scale and so on.
	*/
	//If there's no direct to GPU writing then create staging buffers as well to copy to and then upload to the GPU buffers.
	example->offsets_buffer = zest_CreateStorageDescriptorBuffer(sizeof(tfxU32) * MAX_INSTANCES, true);
	example->animation_instances_buffer = zest_CreateStorageDescriptorBuffer(sizeof(tfx_animation_instance_t) * MAX_INSTANCES, true);
	for (ZEST_EACH_FIF_i) {
		example->offsets_staging_buffer[i] = zest_CreateStagingBuffer(sizeof(tfxU32) * MAX_INSTANCES, 0);
		example->animation_instances_staging_buffer[i] = zest_CreateStagingBuffer(sizeof(tfx_animation_instance_t) * MAX_INSTANCES, 0);
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

	/*
	7)	Finally, the optional buffer for calculating the bounding boxes of each frame of the effects so that we can do frustum culling
	*/
	zest_buffer_info_t bb_buffer_info = zest_CreateStorageBufferInfo();

	// We need to be able to copy from the buffer on the gpu to a list on the CPU side so it needs to be marked as a src buffer
	bb_buffer_info.usage_flags |= VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

	// There won't be a default buffer pool size in zest for this type of buffer so we can set one here.
	zest_SetDeviceBufferPoolSize("Src bit storage buffer pool", bb_buffer_info.usage_flags, bb_buffer_info.property_flags, zloc__KILOBYTE(1), zloc__MEGABYTE(4));
	example->bounding_boxes = zest_CreateDescriptorBuffer(&bb_buffer_info, sizeof(tfx_bounding_box_t) * 200, false);

	//Prepare the compute shaders for effect playback and bounding box calculation
	PrepareComputeForEffectPlayback(example);
	PrepareComputeForBoundingBoxCalculation(example);

	//Create a custom draw routine for drawing the sprites that are written by the effect playback compute shader
	example->tfx_rendering.draw_routine = zest_CreateDrawRoutine("timelinefx draw routine");
	example->tfx_rendering.draw_routine->record_callback = RecordComputeSprites;
	example->tfx_rendering.draw_routine->user_data = example;
	UpdateSpriteResolution(example->tfx_rendering.draw_routine);

	//Now that the sprite data is prepared we can Upload the buffers to the GPU. These are the buffers that we only need to upload once before we do anything
	//They will containt the sprite, image and emitter data that the compute shader will index into to build each frame
	UploadBuffers(example);

	//Calculate the bounding boxes for each effect so that we can do frustum culling
	CalculateBoundingBoxes(example, &example->animation_manager_3d, effect1);
	CalculateBoundingBoxes(example, &example->animation_manager_3d, effect2);
	CalculateBoundingBoxes(example, &example->animation_manager_3d, effect3);
	CalculateBoundingBoxes(example, &example->animation_manager_3d, effect4);

	example->record_time = zest_Millisecs() - example->record_time;	//note how long the above took

	//Initialise Dear ImGui
	zest_imgui_Initialise(&example->imgui_layer_info);

	//Render specific -
	//In order to add our custom draw routine to the existing render queue, along with the compute shader we want to run, we can use zest_ModifyCommandQueue like so.
	zest_ModifyCommandQueue(ZestApp->default_command_queue);
	{
		//Set up the compute shader in the command queue and specify the callback function that will be called each frame to build
		//the command buffer for the compute shader
		zest_NewComputeSetup("Compute sprites", example->compute, SpriteComputeFunction);
		zest_ModifyDrawCommands(ZestApp->default_draw_commands);
		{
			//Create a new mesh layer in the command queue to draw the floor plane
			example->mesh_layer = zest_NewBuiltinLayerSetup("Meshes", zest_builtin_layer_mesh);

			//Add the custom draw routine that will draw the compute sprites
			zest_AddDrawRoutine(example->tfx_rendering.draw_routine);

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

//Function to calculate the bouding boxes for an effect by running the compute shader
void CalculateBoundingBoxes(ComputeExample *example, tfx_animation_manager_t *animation_manager, tfx_effect_emitter_t *effect) {
	//Force the current frame in flight to 0
	zest_SetFrameInFlight(0);

	//Manually set up an animation instance
	tfx_animation_instance_t instance;
	tfx_sprite_data_settings_t &anim = effect->library->sprite_data_settings[GetEffectInfo(effect)->sprite_data_settings_index];
	instance.info_index = animation_manager->effect_animation_info.GetIndex(effect->path_hash);
	instance.current_time = 0.f;
	instance.position = { 0.f, 0.f, 0.f };
	instance.scale = 1.f;
	instance.flags = 0;
	instance.animation_length_in_time = anim.animation_length_in_time;
	instance.tween = 0.f;
	instance.frame_count = anim.frames_after_compression;

	tfxU32 highest_sprite_count = 0;
	//See what the most sprites are for a frame so that we can calculate the group count and size for the staging buffer we'll
	//need to download the bounding boxes for each frame.
	for (int i = 0; i != anim.frames_after_compression; ++i) {
		tfx_sprite_data_metrics_t &metrics = animation_manager->effect_animation_info.data[instance.info_index];
		highest_sprite_count = tfx__Max(metrics.frame_meta[i].total_sprites, highest_sprite_count);
	}

	tfxU32 group_count = highest_sprite_count / 256 + 1;
	zest_buffer bounding_boxes = zest_CreateStagingBuffer(sizeof(tfx_bounding_box_t) * group_count, 0);

	//This loop is very similar to the UpdateAnimationManager function in terms of updating the instance
	//each frame
	for (int i = 0; i != anim.frames_after_compression; ++i) {
		tfx_sprite_data_metrics_t &metrics = animation_manager->effect_animation_info.At(effect->path_hash);
		instance.sprite_count = metrics.frame_meta[i].total_sprites;
		instance.offset_into_sprite_data = metrics.frame_meta[i].index_offset[0];
		tfx_sprite_data3d_t &first_sprite = animation_manager->sprite_data_3d[instance.offset_into_sprite_data];
		animation_manager->render_queue.clear();
		animation_manager->offsets.clear();
		animation_manager->offsets.push_back(instance.sprite_count);
		animation_manager->render_queue.push_back(instance);
		UpdateAnimationManagerBufferMetrics(animation_manager);
		animation_manager->buffer_metrics.total_sprites_to_draw = instance.sprite_count;
		tfxU32 frame_group_count = instance.sprite_count / 256 + 1;
		//Maybe grow the bounding box buffer if it's too small
		zest_GrowDescriptorBuffer(example->bounding_boxes, sizeof(tfx_bounding_box_t), sizeof(tfx_bounding_box_t) * frame_group_count);
		zest_UpdateComputeDescriptors(example->bounding_box_compute);
		//Update the offset and instance buffers either.
		memcpy(example->offsets_staging_buffer[ZEST_FIF]->data, animation_manager->offsets.data, GetOffsetsSizeInBytes(animation_manager));
		memcpy(example->animation_instances_staging_buffer[ZEST_FIF]->data, animation_manager->render_queue.data, GetAnimationInstancesSizeInBytes(animation_manager));

		//Run the compute shader and wait for it to finish
		zest_RunCompute(example->bounding_box_compute);
		zest_WaitForCompute(example->bounding_box_compute);

		//There will be a bounding box for each group run on the compute shader, so we just need to copy them from the GPU
		//and then loop through them and set the final bounding box which will contain the min/max values of the box.
		zest_CopyBuffer(example->bounding_boxes->buffer[0], bounding_boxes, sizeof(tfx_bounding_box_t) * frame_group_count);
		metrics.frame_meta[i].min_corner = { FLT_MAX,  FLT_MAX,  FLT_MAX };
		metrics.frame_meta[i].max_corner = { -FLT_MAX, -FLT_MAX, -FLT_MAX };
		for (int group = 0; group != frame_group_count; ++group) {
			tfx_bounding_box_t *bb = (tfx_bounding_box_t*)bounding_boxes->data + group;
			tfx_vec3_t &min_corner = metrics.frame_meta[i].min_corner;
			tfx_vec3_t &max_corner = metrics.frame_meta[i].max_corner;
			metrics.frame_meta[i].min_corner = { tfx__Min(metrics.frame_meta[i].min_corner.x, bb->min_corner.x), tfx__Min(metrics.frame_meta[i].min_corner.y, bb->min_corner.y), tfx__Min(metrics.frame_meta[i].min_corner.z, bb->min_corner.z) };
			metrics.frame_meta[i].max_corner = { tfx__Max(metrics.frame_meta[i].max_corner.x, bb->max_corner.x), tfx__Max(metrics.frame_meta[i].max_corner.y, bb->max_corner.y), tfx__Max(metrics.frame_meta[i].max_corner.z, bb->max_corner.z) };
		}
		//Use the bounding box to calculate the center point and the radius of the box. We can use the radius for a faster way to 
		//cull out of view instances, albeit a little bit less accurate.
		tfx_vec3_t half_extents = (metrics.frame_meta[i].max_corner - metrics.frame_meta[i].min_corner) * 0.5f;
		metrics.frame_meta[i].radius = LengthVec(&half_extents);
		metrics.frame_meta[i].bb_center_point = (metrics.frame_meta[i].max_corner + metrics.frame_meta[i].min_corner) * 0.5f;
		//Advanc the current time of the instance
		instance.current_time += 1000.f / anim.recording_frame_rate;
	}

	//Free the temporary staging buffer
	zest_FreeBuffer(bounding_boxes);
	//Set the frame in flight index back to it's original value.
	zest_RestoreFrameInFlight();
}

//Fuction to cast a ray from screen to world space.
tfx_vec3_t ScreenRay(float x, float y, float depth_offset, zest_vec3 &camera_position) {
	zest_uniform_buffer_data_t *ubo_ptr = static_cast<zest_uniform_buffer_data_t*>(zest_GetUniformBufferData(ZestRenderer->standard_uniform_buffer));
	zest_vec3 camera_last_ray = zest_ScreenRay(x, y, zest_ScreenWidthf(), zest_ScreenHeightf(), &ubo_ptr->proj, &ubo_ptr->view);
	zest_vec3 pos = zest_AddVec3(zest_ScaleVec3(camera_last_ray, depth_offset), camera_position);
	return { pos.x, pos.y, pos.z };
}

void BuildUI(ComputeExample *example) {
	//Must call the imgui GLFW implementation function
	ImGui_ImplGlfw_NewFrame();
	//Draw our imgui stuff
	ImGui::NewFrame();
	ImGui::Begin("Instanced Effects");
	ImGui::Text("FPS %i", ZestApp->last_fps);
	ImGui::Text("Record Time: %zims", example->record_time);
	ImGui::Text("Culled Instances: %i", GetTotalInstancesBeingUpdated(&example->animation_manager_3d) - example->animation_manager_3d.render_queue.size());
	ImGui::Text("Instances In View: %i", example->animation_manager_3d.render_queue.size());
	ImGui::Text("Sprites Drawn: %i", example->animation_manager_3d.buffer_metrics.total_sprites_to_draw);
	ImGui::Text("Total Memory For Drawn Sprites: %ikb", (example->animation_manager_3d.buffer_metrics.total_sprites_to_draw * 64) / (1024));
	ImGui::Text("Total Memory For Sprite Data: %ikb", example->animation_manager_3d.sprite_data_3d.size_in_bytes() / (1024));
	ImGui::End();
	ImGui::Render();
	//Let the layer know that it needs to reupload the imgui mesh data to the GPU
	zest_ResetLayer(example->imgui_layer_info.mesh_layer);
	//Load the imgui mesh data into the layer staging buffers. When the command queue is recorded, it will then upload that data to the GPU buffers for rendering
	zest_imgui_UpdateBuffers(example->imgui_layer_info.mesh_layer);
}

//The callback function called for each instance that is being updated.
bool CullAnimationInstancesCallback(tfx_animation_manager_t *animation_manager, tfx_animation_instance_t *instance, tfx_frame_meta_t *frame_meta, void *user_data) {
	//Grab the ComputeExample struct from the user data.
	ComputeExample *example = static_cast<ComputeExample*>(user_data);
	//The the uniform buffer containing the view and projection matrices so that we can get the view frustum

	//Get the position of the instance offset by the center point of the bounding box
	tfx_vec3_t bb_position = frame_meta->bb_center_point + instance->position;
	//Check if the radius of the bounding box is in view. Note that the planes are calculated in the UpdateUniform3d function.
	return (bool)zest_IsSphereInFrustum(example->planes, &bb_position.x, frame_meta->radius);
}

//Our main update function that's run every frame.
void Update(zest_microsecs elapsed, void *data) {
	ComputeExample *example = static_cast<ComputeExample*>(data);
	//Render specific - set the command queue we're using to record the command buffer and update the uniform buffer.
	zest_SetActiveCommandQueue(ZestApp->default_command_queue);
	UpdateUniform3d(example);

	int r = RandomRange(&example->pm.random, 0, 3);
	if (!example->left_mouse_clicked && ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
		//If the left mouse is clicked then spawn a random firework and raycasting it into the scene.
		tfx_vec3_t position = ScreenRay(zest_MouseXf(), zest_MouseYf(), 12.f, example->camera.position);
		tfxAnimationID anim_id = tfxINVALID;
		if (r == 0) {
			anim_id = AddAnimationInstance(&example->animation_manager_3d, "Big Explosion", 0);
		}
		else if (r == 1) {
			anim_id = AddAnimationInstance(&example->animation_manager_3d, "Star Burst Flash", 0);
		}
		else if (r == 2) {
			anim_id = AddAnimationInstance(&example->animation_manager_3d, "EmissionSingleShot", 0);
		}
		else if (r == 3) {
			anim_id = AddAnimationInstance(&example->animation_manager_3d, "Firework", 0);
		}
		if (anim_id != tfxINVALID) {
			//As long as we get a valie anim id, set it's position and random scale.
			tfx_vec3_t position = tfx_vec3_t(RandomRange(&example->pm.random, -10.f, 10.f), RandomRange(&example->pm.random, 8.f, 15.f), RandomRange(&example->pm.random, -10.f, 10.f));
			SetAnimationPosition(&example->animation_manager_3d, anim_id, &position.x);
			SetAnimationScale(&example->animation_manager_3d, anim_id, RandomRange(&example->pm.random, 0.5f, 1.5f));
		}
		example->left_mouse_clicked = true;
	}
	else if(!ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
		example->left_mouse_clicked = false;
	}

	//Make a firework spawn randomly in the scene every 25 millisecs
	example->trigger_effect += elapsed;
	if (example->trigger_effect >= ZEST_MILLISECONDS_IN_MICROSECONDS(25)) {
		tfxAnimationID anim_id = tfxINVALID;
		if (r == 0) {
			anim_id = AddAnimationInstance(&example->animation_manager_3d, "Big Explosion", 0);
		} else if (r == 1) {
			anim_id = AddAnimationInstance(&example->animation_manager_3d, "Star Burst Flash", 0);
		} else if (r == 2) {
			anim_id = AddAnimationInstance(&example->animation_manager_3d, "EmissionSingleShot", 0);
		} else if (r == 3) {
			anim_id = AddAnimationInstance(&example->animation_manager_3d, "Firework", 0);
		}
		if (anim_id != tfxINVALID) {
			tfx_vec3_t position = tfx_vec3_t(RandomRange(&example->pm.random, -10.f, 10.f), RandomRange(&example->pm.random, 8.f, 15.f), RandomRange(&example->pm.random, -10.f, 10.f));
			SetAnimationPosition(&example->animation_manager_3d, anim_id, &position.x);
			SetAnimationScale(&example->animation_manager_3d, anim_id, RandomRange(&example->pm.random, 0.75f, 1.5f));
		}
		example->trigger_effect = 0;
	}

	if (ImGui::IsKeyDown(ImGuiKey_Space)) {
		ClearAllAnimationInstances(&example->animation_manager_3d);
	}
 
	//First control the camera with the mosue if the right mouse is clicked
	bool camera_free_look = false;
	if (ImGui::IsMouseDown(ImGuiMouseButton_Right)) {
		camera_free_look = true;
		if (glfwRawMouseMotionSupported()) {
			glfwSetInputMode((GLFWwindow*)zest_Window(), GLFW_RAW_MOUSE_MOTION, GLFW_TRUE);
		}
		ZEST__FLAG(ImGui::GetIO().ConfigFlags, ImGuiConfigFlags_NoMouse);
		double x_mouse_speed;
		double y_mouse_speed;
		zest_GetMouseSpeed(&x_mouse_speed, &y_mouse_speed);
		zest_TurnCamera(&example->camera, (float)ZestApp->mouse_delta_x, (float)ZestApp->mouse_delta_y, .05f);
	}
	else if (glfwRawMouseMotionSupported()) {
		camera_free_look = false;
		ZEST__UNFLAG(ImGui::GetIO().ConfigFlags, ImGuiConfigFlags_NoMouse);
		glfwSetInputMode((GLFWwindow*)zest_Window(), GLFW_CURSOR, GLFW_CURSOR_NORMAL);
	}
	else {
		camera_free_look = false;
		ZEST__UNFLAG(ImGui::GetIO().ConfigFlags, ImGuiConfigFlags_NoMouse);
	}

	//An example fixed rate update loop
	zest_TimerAccumulate(example->timer);
	while (zest_TimerDoUpdate(example->timer)) {
		//example->pm.Update();
		BuildUI(example);

		float speed  = 5.f * (float)example->timer->update_time;
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
			glfwSetInputMode((GLFWwindow*)zest_Window(), GLFW_CURSOR, GLFW_CURSOR_DISABLED);
		}
		else {
			glfwSetInputMode((GLFWwindow*)zest_Window(), GLFW_CURSOR, GLFW_CURSOR_NORMAL);
		}

		zest_TimerUnAccumulate(example->timer);
	}

	//Update the animation manager each frame. This will advance the time of each animation instance that's currently playing
	//Pass the amount of time that has elapsed since the last time the function was called
	//This could also be placed in the fixed rate update loop with an elapsed time of the update frequency
	for (auto i : example->animation_manager_3d.instances_in_use[example->animation_manager_3d.current_in_use_buffer]) {
		tfx_animation_instance_t &instance = example->animation_manager_3d.instances[i];
		tfx_sprite_data_metrics_t &metrics = example->animation_manager_3d.effect_animation_info.data[instance.info_index];
	}
	UpdateAnimationManager(&example->animation_manager_3d, elapsed / 1000.f);

	//Now set the mesh drawing so that we can draw a textured plane
	zest_SetMeshDrawing(example->mesh_layer, example->floor_resources, example->mesh_pipeline);
	zest_DrawTexturedPlane(example->mesh_layer, example->floor_image, -500.f, -5.f, -500.f, 1000.f, 1000.f, 50.f, 50.f, 0.f, 0.f);

	//Copy the offsets and animation instances either to the staging buffers. The staging buffers will then be uploaded in the render pipeline.
	memcpy(example->offsets_staging_buffer[ZEST_FIF]->data, example->animation_manager_3d.offsets.data, GetOffsetsSizeInBytes(&example->animation_manager_3d));
	memcpy(example->animation_instances_staging_buffer[ZEST_FIF]->data, example->animation_manager_3d.render_queue.data, GetAnimationInstancesSizeInBytes(&example->animation_manager_3d));

	zest_TimerSet(example->timer);
}

#if defined(_WIN32)
//int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR pCmdLine, int nCmdShow) {
int main() {
	//Render specific
	//When initialising a qulkan app, you can pass a QulkanCreateInfo which you can use to configure some of the base settings of the app
	//Create new config struct for Zest
	zest_create_info_t create_info = zest_CreateInfo();
	//Don't enable vsync so we can see how much FPS we get
	ZEST__UNFLAG(create_info.flags, zest_init_flag_enable_vsync);
	ZEST__FLAG(create_info.flags, zest_init_flag_log_validation_errors_to_console);
	create_info.log_path = "./";
	//Implement GLFW for window creation
	zest_implglfw_SetCallbacks(&create_info);

	//Initialise TimelineFX. Must be run before using any timeline fx functionality.
	InitialiseTimelineFX(std::thread::hardware_concurrency(), tfxMegabyte(128));
	//InitialiseTimelineFX(0, tfxMegabyte(128));

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
#else
int main() {
	//Render specific
	//When initialising a qulkan app, you can pass a QulkanCreateInfo which you can use to configure some of the base settings of the app
	//Create new config struct for Zest
	zest_create_info_t create_info = zest_CreateInfo();
	//Don't enable vsync so we can see how much FPS we get
	ZEST__UNFLAG(create_info.flags, zest_init_flag_enable_vsync);

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
#endif
