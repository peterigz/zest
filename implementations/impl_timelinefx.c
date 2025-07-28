#include "impl_timelinefx.h"

//Before you load an effects file, you will need to define a ShapeLoader function that passes the following parameters:
//const char* filename			- this will be the filename of the image being loaded from the library. You don't have to do anything with this if you don't need to.
//ImageData	&image_data			- A struct containing data about the image. You will have to set image_data.ptr to point to the texture in your renderer for later use in the Render function that you will create to render the particles
//void *raw_image_data			- The raw data of the image which you can use to load the image into graphics memory
//int image_memory_size			- The size in bytes of the raw_image_data
//void *custom_data				- This allows you to pass through an object you can use to access whatever is necessary to load the image into graphics memory, depending on the renderer that you're using
void zest_tfx_ShapeLoader(const char *filename, tfx_image_data_t *image_data, void *raw_image_data, int image_memory_size, void *custom_data) {
	//Cast your custom data, this can be anything you want
	tfx_render_resources_t *resources = (tfx_render_resources_t *)custom_data;

	//This shape loader example uses the STB image library to load the raw bitmap (png usually) data
	zest_bitmap bitmap = zest_NewBitmap();
	zest_LoadBitmapImageMemory(bitmap, (unsigned char *)raw_image_data, image_memory_size, 0);
	//Convert the image to RGBA which is necessary for this particular renderer
	zest_ConvertBitmapToRGBA(bitmap, 255);
	//The editor has the option to convert an bitmap to an alpha map. I will probably change this so that it gets baked into the saved effect so you won't need to apply the filter here.
	//Alpha map is where all color channels are set to 255
	if (image_data->import_filter)
		zest_ConvertBitmapToAlpha(bitmap);

	//Get the texture where we're storing all the particle shapes
	//You'll probably need to load the image in such a way depending on whether or not it's an animation or not
	if (image_data->animation_frames > 1) {
		//Add the spritesheet to the texture in our renderer
		float max_radius = 0;
		image_data->ptr = zest_AddTextureAnimationBitmap(resources->particle_texture, bitmap, (tfxU32)image_data->image_size.x, (tfxU32)image_data->image_size.y, (tfxU32)image_data->animation_frames, &max_radius, 1);
		//Important step: you need to point the ImageData.ptr to the appropriate handle in the renderer to point to the texture of the particle shape
		//You'll need to use this in your render function to tell your renderer which texture to use to draw the particle
	} else {
		//Add the image to the texture in our renderer
		image_data->ptr = zest_AddTextureImageBitmap(resources->particle_texture, bitmap);
		//Important step: you need to point the ImageData.ptr to the appropriate handle in the renderer to point to the texture of the particle shape
		//You'll need to use this in your render function to tell your renderer which texture to use to draw the particle
	}
	zest_FreeBitmap(bitmap);
}

//Basic function for updating the uniform buffer
void zest_tfx_UpdateUniformBuffer(tfx_render_resources_t *resources) {
	tfx_uniform_buffer_data_t *uniform_buffer = (tfx_uniform_buffer_data_t*)zest_GetUniformBufferData(resources->uniform_buffer);
	uniform_buffer->view = zest_LookAt(resources->camera.position, zest_AddVec3(resources->camera.position, resources->camera.front), resources->camera.up);
	uniform_buffer->proj = zest_Perspective(resources->camera.fov, zest_ScreenWidthf() / zest_ScreenHeightf(), 0.1f, 10000.f);
	uniform_buffer->proj.v[1].y *= -1.f;
	uniform_buffer->screen_size.x = zest_ScreenWidthf();
	uniform_buffer->screen_size.y = zest_ScreenHeightf();
	uniform_buffer->timer_lerp = (float)zest_TimerLerp(resources->timer);
	uniform_buffer->update_time = (float)zest_TimerUpdateTime(resources->timer);
}

void zest_tfx_GetUV(void *ptr, tfx_gpu_image_data_t *image_data, int offset) {
	zest_image image = (zest_image)(ptr)+offset;
	image_data->uv.x = image->uv.x;
	image_data->uv.y = image->uv.y;
	image_data->uv.z = image->uv.z;
	image_data->uv.w = image->uv.w;
	image_data->texture_array_index = image->layer;
	image_data->uv_packed = image->uv_packed;
}

void zest_tfx_InitTimelineFXRenderResources(tfx_render_resources_t *resources, const char *library_path) {
	resources->uniform_buffer = zest_CreateUniformBuffer("tfx uniform", sizeof(tfx_uniform_buffer_data_t));

	resources->timer = zest_CreateTimer("TimelineFX Loop Timer", 60);

	resources->camera = zest_CreateCamera();
	zest_CameraSetFoV(&resources->camera, 60.f);

	zest_tfx_UpdateUniformBuffer(resources);

	int shape_count = tfx_GetShapeCountInLibrary(library_path);
	resources->particle_texture = zest_CreateTexture("Particle Texture", zest_texture_storage_type_packed, zest_texture_flag_use_filtering, zest_texture_format_rgba_unorm, shape_count);
	resources->color_ramps_texture = zest_CreateTextureBank("Particle Color Ramps", zest_texture_format_rgba_unorm);
	zest_SetTextureUseFiltering(resources->color_ramps_texture, false);

	//Compile the shaders we will use to render the particles
	shaderc_compiler_t compiler = shaderc_compiler_initialize();
	resources->fragment_shader = zest_CreateShaderFromFile("examples/assets/shaders/timelinefx.frag", "tfx_frag.spv", shaderc_fragment_shader, true, compiler, 0);
	resources->vertex_shader = zest_CreateShaderFromFile("examples/assets/shaders/timelinefx3d.vert", "tfx_vertex.spv", shaderc_vertex_shader, true, compiler, 0);
	shaderc_compiler_release(compiler);

	//To render the particles we setup a pipeline with the vertex attributes and shaders to render the particles.
	//First create a descriptor set layout, we need 2 samplers, one to sample the particle texture and another to sample the color ramps
	//We also need 2 storage buffers, one to access the image data in the vertex shader and the other to access the previous frame particles
	//so that they can be interpolated in between updates

	resources->pipeline = zest_BeginPipelineTemplate("Timelinefx pipeline");
	//Set up the vertex attributes that will take in all of the billboard data stored in tfx_instance_t objects
	zest_AddVertexInputBindingDescription(resources->pipeline, 0, sizeof(tfx_instance_t), VK_VERTEX_INPUT_RATE_INSTANCE);
	zest_AddVertexAttribute(resources->pipeline, 0, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(tfx_instance_t, position));	            // Location 0: Postion and stretch in w
	zest_AddVertexAttribute(resources->pipeline, 1, VK_FORMAT_R32G32B32_SFLOAT, offsetof(tfx_instance_t, rotations));	                // Location 1: Rotations
	zest_AddVertexAttribute(resources->pipeline, 2, VK_FORMAT_R8G8B8_SNORM, offsetof(tfx_instance_t, alignment));					    // Location 2: Alignment
	zest_AddVertexAttribute(resources->pipeline, 3, VK_FORMAT_R16G16B16A16_SSCALED, offsetof(tfx_instance_t, size_handle));		    // Location 3: Size and handle of the sprite
	zest_AddVertexAttribute(resources->pipeline, 4, VK_FORMAT_R16G16_SSCALED, offsetof(tfx_instance_t, intensity_gradient_map));       // Location 4: 2 intensities for each color
	zest_AddVertexAttribute(resources->pipeline, 5, VK_FORMAT_R8G8B8_UNORM, offsetof(tfx_instance_t, curved_alpha_life));          	// Location 5: Sharpness and mix lerp value
	zest_AddVertexAttribute(resources->pipeline, 6, VK_FORMAT_R32_UINT, offsetof(tfx_instance_t, indexes));							// Location 6: texture indexes to sample the correct image and color ramp
	zest_AddVertexAttribute(resources->pipeline, 7, VK_FORMAT_R32_UINT, offsetof(tfx_instance_t, captured_index));   				    // Location 7: index of the sprite in the previous buffer when double buffering
	//Set the shaders to our custom timelinefx shaders
	zest_SetPipelineShaders(resources->pipeline, "tfx_vertex.spv", "tfx_frag.spv", 0);
	zest_SetPipelinePushConstantRange(resources->pipeline, sizeof(tfx_push_constants_t), zest_shader_render_stages);
	zest_AddPipelineDescriptorLayout(resources->pipeline, zest_vk_GetUniformBufferLayout(resources->uniform_buffer));
	zest_AddPipelineDescriptorLayout(resources->pipeline, zest_vk_GetGlobalBindlessLayout());
	zest_SetPipelineDepthTest(resources->pipeline, false, true);
	zest_SetPipelineBlend(resources->pipeline, zest_PreMultiplyBlendState());
	zest_EndPipelineTemplate(resources->pipeline);

	//We want to be able to manually change the current frame in flight in the layer that we use to draw all the billboards.
	//This means that we are able to only change the current frame in flight if we actually updated the particle manager in the current
	//frame allowing us to dictate when to upload the instance buffer to the gpu as there's no need to do it every frame, only when 
	//the particle manager is actually updated.
	resources->layer = zest_CreateFIFInstanceLayer("TimelineFX Layer", sizeof(tfx_instance_t), ZEST_FIXED_LOOP_BUFFER);
	zest_AcquireGlobalInstanceLayerBufferIndex(resources->layer);

	//Create a buffer to store the image data on the gpu. 
	resources->image_data = zest_CreateStorageBuffer(sizeof(tfx_gpu_image_data_t) * 1000, 0);
	zest_AcquireGlobalStorageBufferIndex(resources->image_data);

	resources->timeline = zest_CreateExecutionTimeline();
	//End of render specific code
}

void zest_tfx_UpdateTimelineFXImageData(tfx_render_resources_t *tfx_rendering, tfx_library library) {
	//Upload the timelinefx image data to the image data buffer created
	zest_buffer image_data_buffer = tfx_rendering->image_data;
	tfx_gpu_shapes shapes = tfx_GetLibraryGPUShapes(library);
	zest_buffer staging_buffer = zest_CreateStagingBuffer(tfx_GetGPUShapesSizeInBytes(shapes), tfx_GetGPUShapesArray(shapes));
	zest_CopyBufferOneTime(staging_buffer, image_data_buffer, tfx_GetGPUShapesSizeInBytes(shapes));
	zest_FreeBuffer(staging_buffer);
}

void zest_tfx_CreateTimelineFXShaderResources(tfx_render_resources_t *tfx_rendering) {
	tfx_rendering->shader_resource = zest_CreateShaderResources("TimelineFX");
	zest_ForEachFrameInFlight(fif) {
		zest_AddDescriptorSetToResources(tfx_rendering->shader_resource, zest_GetFIFUniformBufferSet(tfx_rendering->uniform_buffer, fif), fif);
		zest_AddDescriptorSetToResources(tfx_rendering->shader_resource, zest_GetGlobalBindlessSet(), fif);
	}
}

void zest_tfx_AddPassTask(zest_pass_node pass, tfx_render_resources_t *resources) {
	zest_SetPassTask(pass, zest_tfx_DrawParticleLayer, resources);
}

void zest_tfx_DrawParticleLayer(VkCommandBuffer command_buffer, const zest_render_graph_context_t *context, void *user_data) {
	tfx_render_resources_t *tfx_resources = (tfx_render_resources_t *)user_data;
	zest_layer layer = tfx_resources->layer;
	ZEST_CHECK_HANDLE(layer);	//Not a valid handle! Make sure you pass in the zest_layer in the user data

	zest_buffer device_buffer = layer->memory_refs[layer->fif].device_vertex_data;
	zest_buffer prev_buffer = layer->memory_refs[layer->prev_fif].device_vertex_data;
	zest_BindVertexBuffer(command_buffer, device_buffer);

	zest_vec_foreach(i, layer->draw_instructions[layer->fif]) {
		zest_layer_instruction_t *current = &layer->draw_instructions[layer->fif][i];

		zest_SetScreenSizedViewport(context, 0.f, 1.f);

		zest_pipeline pipeline = zest_PipelineWithTemplate(current->pipeline_template, context->render_pass);
		if (pipeline && ZEST_VALID_HANDLE(current->shader_resources)) {
			zest_BindPipelineShaderResource(command_buffer, pipeline, current->shader_resources);
		} else {
			continue;
		}

		tfx_push_constants_t *push_constants = (tfx_push_constants_t *)current->push_constant;
		push_constants->color_ramp_texture_index = zest_GetTextureDescriptorIndex(tfx_resources->color_ramps_texture);
		push_constants->particle_texture_index = zest_GetTextureDescriptorIndex(tfx_resources->particle_texture);
		push_constants->image_data_index = zest_GetBufferDescriptorIndex(tfx_resources->image_data);
		push_constants->prev_billboards_index = zest_GetLayerVertexDescriptorIndex(layer, true);

		zest_SendPushConstants(command_buffer, pipeline, push_constants);

		zest_DrawLayerInstruction(command_buffer, 6, current);
	}
}

//A simple example to render the particles. This is for when the particle manager has one single list of sprites rather than grouped by effect
void zest_tfx_RenderParticles(tfx_effect_manager pm, tfx_render_resources_t *resources) {
	//Let our renderer know that we want to draw to the timelinefx layer.
	zest_SetInstanceDrawing(resources->layer, resources->shader_resource, resources->pipeline);

	tfx_instance_t *billboards = tfx_GetInstanceBuffer(pm);
	int instance_count = tfx_GetInstanceCount(pm);
	zest_draw_buffer_result result = zest_DrawInstanceBuffer(resources->layer, billboards, tfx_GetInstanceCount(pm));
	/*
	if (ZestRenderer->total_frame_count >= 200 && ZestRenderer->total_frame_count <= 220) {
		for (int i = instance_count - 10; i != instance_count; ++i) {
			tfx_vec4_t position = billboards[i].position;
			zest_uint captured_index = billboards[i].captured_index & 0x0FFFFFFF;
			ZEST_PRINT("%i) %f, %f, %f, %u", i, position.x, position.y, position.z, captured_index);
		}
		ZEST_PRINT("---------------------------");
	}
	*/
}

void zest_tfx_RenderParticlesByEffect(tfx_effect_manager pm, tfx_render_resources_t *resources) {
	//Let our renderer know that we want to draw to the timelinefx layer.
	zest_SetInstanceDrawing(resources->layer, resources->shader_resource, resources->pipeline);

	tfx_instance_t *billboards = NULL;
	tfx_effect_instance_data_t *instance_data;
	tfxU32 instance_count = 0;
	//Loop over the effects to get each instance buffer to render
	while (tfx_GetNextInstanceBuffer(pm, &billboards, &instance_data, &instance_count)) {
		zest_draw_buffer_result result = zest_DrawInstanceBuffer(resources->layer, billboards, instance_count);
	}
	tfx_ResetInstanceBufferLoopIndex(pm);
}