#include "impl_timelinefx.h"
#include "stb_image.h"

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
	int width, height, channels;
	stbi_uc *pixels = stbi_load_from_memory(raw_image_data, image_memory_size, &width, &height, &channels, 4);
	//Convert the image to RGBA which is necessary for this particular renderer
	zest_size pixels_size = width * height * 4;
	//The editor has the option to convert an bitmap to an alpha map. I will probably change this so that it gets baked into the saved effect so you won't need to apply the filter here.
	//Alpha map is where all color channels are set to 255
	zest_bitmap_t bitmap = { 0 };
	bitmap.data = pixels;
	bitmap.meta.size = pixels_size;
	bitmap.meta.channels = 4;
	bitmap.meta.width = width;
	bitmap.meta.height = height;
	bitmap.meta.format = zest_format_r8g8b8a8_unorm;
	if (image_data->import_filter) {
		zest_ConvertBitmapToAlpha(&bitmap);
	}

	//Get the texture where we're storing all the particle shapes
	//You'll probably need to load the image in such a way depending on whether or not it's an animation or not
	if (image_data->animation_frames > 1) {
		//Add the spritesheet to the texture in our renderer
		float max_radius = 0;
		image_data->ptr = zest_AddImageAtlasAnimationPixels(&resources->particle_images, pixels, pixels_size, width, height, (tfxU32)image_data->image_size.x, (tfxU32)image_data->image_size.y, (tfxU32)image_data->animation_frames, zest_format_r8g8b8a8_unorm);
		//Important step: you need to point the ImageData.ptr to the appropriate handle in the renderer to point to the texture of the particle shape
		//You'll need to use this in your render function to tell your renderer which texture to use to draw the particle
	} else {
		//Add the image to the texture in our renderer
		image_data->ptr = zest_AddImageAtlasPixels(&resources->particle_images, pixels, pixels_size, width, height, zest_format_r8g8b8a8_unorm);
		//Important step: you need to point the ImageData.ptr to the appropriate handle in the renderer to point to the texture of the particle shape
		//You'll need to use this in your render function to tell your renderer which texture to use to draw the particle
	}
	free(pixels);
}

//Basic function for updating the uniform buffer
void zest_tfx_UpdateUniformBuffer(zest_context context, tfx_render_resources_t *resources) {
	zest_uniform_buffer buffer = zest_GetUniformBuffer(resources->uniform_buffer);
	tfx_uniform_buffer_data_t *uniform_buffer = (tfx_uniform_buffer_data_t*)zest_GetUniformBufferData(buffer);
	uniform_buffer->view = zest_LookAt(resources->camera.position, zest_AddVec3(resources->camera.position, resources->camera.front), resources->camera.up);
	uniform_buffer->proj = zest_Perspective(resources->camera.fov, zest_ScreenWidthf(context) / zest_ScreenHeightf(context), 0.1f, 10000.f);
	uniform_buffer->proj.v[1].y *= -1.f;
	uniform_buffer->screen_size.x = zest_ScreenWidthf(context);
	uniform_buffer->screen_size.y = zest_ScreenHeightf(context);
	uniform_buffer->timer_lerp = (float)zest_TimerLerp(&resources->timer);
	uniform_buffer->update_time = (float)zest_TimerUpdateTime(&resources->timer);
}

void zest_tfx_GetUV(void *ptr, tfx_gpu_image_data_t *image_data, int offset) {
	zest_atlas_region_t *region = (zest_atlas_region_t*)(ptr) + offset;
	image_data->uv.x = region->uv.x;
	image_data->uv.y = region->uv.y;
	image_data->uv.z = region->uv.z;
	image_data->uv.w = region->uv.w;
	image_data->texture_array_index = region->layer_index;
	image_data->uv_packed = region->uv_packed;
}

zest_image_collection_t zest_tfx_CreateImageCollection(zest_uint shape_count) {
	return zest_CreateImageAtlasCollection(zest_format_r8g8b8a8_unorm, shape_count);
}

void zest_tfx_InitTimelineFXRenderResources(zest_context context, tfx_render_resources_t *resources, const char *vert_shader, const char *frag_shader) {
	zest_device device = zest_GetContextDevice(context);
	resources->uniform_buffer = zest_CreateUniformBuffer(context, "tfx uniform", sizeof(tfx_uniform_buffer_data_t));

	resources->timer = zest_CreateTimer(60);

	resources->camera = zest_CreateCamera();
	zest_CameraSetFoV(&resources->camera, 60.f);
	//float cam_pos[3] = {0.f, 3.f, 7.5};
	//zest_CameraPosition(&resources->camera, cam_pos);
	zest_CameraUpdateFront(&resources->camera);

	zest_tfx_UpdateUniformBuffer(context, resources);

	//Compile the shaders we will use to render the particles
	resources->fragment_shader = zest_CreateShaderFromFile(device, frag_shader, "tfx_frag.spv", zest_fragment_shader, true);
	resources->vertex_shader = zest_CreateShaderFromFile(device, vert_shader, "tfx_vertex.spv", zest_vertex_shader, true);

	//To render the particles we setup a pipeline with the vertex attributes and shaders to render the particles.
	//First create a descriptor set layout, we need 2 samplers, one to sample the particle texture and another to sample the color ramps
	//We also need 2 storage buffers, one to access the image data in the vertex shader and the other to access the previous frame particles
	//so that they can be interpolated in between updates

	zest_uniform_buffer uniform_buffer = zest_GetUniformBuffer(resources->uniform_buffer);

	resources->pipeline = zest_CreatePipelineTemplate(device, "Timelinefx pipeline");
	//Set up the vertex attributes that will take in all of the billboard data stored in tfx_instance_t objects
	zest_AddVertexInputBindingDescription(resources->pipeline, 0, sizeof(tfx_instance_t), zest_input_rate_instance);
	zest_AddVertexAttribute(resources->pipeline, 0, 0, zest_format_r32g32b32a32_sfloat, offsetof(tfx_instance_t, position));	            // Location 0: Postion and stretch in w
	zest_AddVertexAttribute(resources->pipeline, 0, 1, zest_format_r8g8b8a8_snorm, offsetof(tfx_instance_t, quaternion));	                // Location 1: Quaternion (packed)
	zest_AddVertexAttribute(resources->pipeline, 0, 2, zest_format_r8g8b8_snorm, offsetof(tfx_instance_t, alignment));					    // Location 2: Alignment
	zest_AddVertexAttribute(resources->pipeline, 0, 3, zest_format_r16g16b16a16_sscaled, offsetof(tfx_instance_t, size_handle));		    // Location 3: Size and handle of the sprite
	zest_AddVertexAttribute(resources->pipeline, 0, 4, zest_format_r16g16_sscaled, offsetof(tfx_instance_t, intensity_gradient_map));       // Location 4: 2 intensities for each color
	zest_AddVertexAttribute(resources->pipeline, 0, 5, zest_format_r8g8b8_unorm, offsetof(tfx_instance_t, curved_alpha_life));          	// Location 5: Sharpness and mix lerp value
	zest_AddVertexAttribute(resources->pipeline, 0, 6, zest_format_r32_uint, offsetof(tfx_instance_t, indexes));							// Location 6: texture indexes to sample the correct image and color ramp
	zest_AddVertexAttribute(resources->pipeline, 0, 7, zest_format_r32_uint, offsetof(tfx_instance_t, captured_index));   				    // Location 7: index of the sprite in the previous buffer when double buffering
	//Set the shaders to our custom timelinefx shaders
	zest_SetPipelineShaders(resources->pipeline, resources->vertex_shader, resources->fragment_shader);
	zest_SetPipelineDepthTest(resources->pipeline, true, false);
	zest_SetPipelineBlend(resources->pipeline, zest_PreMultiplyBlendState());

	//We want to be able to manually change the current frame in flight in the layer that we use to draw all the billboards.
	//This means that we are able to only change the current frame in flight if we actually updated the particle manager in the current
	//frame allowing us to dictate when to upload the instance buffer to the gpu as there's no need to do it every frame, only when 
	//the particle manager is actually updated.
	resources->layer = zest_CreateFIFInstanceLayer(context, "TimelineFX Layer", sizeof(tfx_instance_t), 50000);
	zest_AcquireInstanceLayerBufferIndex(device, zest_GetLayer(resources->layer));

	zest_sampler_info_t sampler_info = zest_CreateSamplerInfo();
	resources->sampler = zest_CreateSampler(context, &sampler_info);
	zest_sampler sampler = zest_GetSampler(resources->sampler);
	resources->sampler_index = zest_AcquireSamplerIndex(device, sampler);

	//Create a buffer to store the image data on the gpu.
	zest_buffer_info_t image_data_buffer_info = zest_CreateBufferInfo(zest_buffer_type_storage, zest_memory_usage_gpu_only);
	resources->image_data = zest_CreateBuffer(device, sizeof(tfx_gpu_image_data_t) * 1000, &image_data_buffer_info);
	resources->image_data_index = zest_AcquireStorageBufferIndex(device, resources->image_data);

	resources->timeline = zest_CreateExecutionTimeline(device);
	//End of render specific code
}

void zest_tfx_FreeLibraryImages(tfx_render_resources_t *resources) {
	zest_FreeImageCollection(&resources->particle_images);
	zest_FreeImageCollection(&resources->color_ramps_collection);
}

void zest_tfx_UpdateTimelineFXImageData(zest_context context, tfx_render_resources_t *tfx_rendering, tfx_library library) {
	//Upload the timelinefx image data to the image data buffer created
	zest_buffer image_data_buffer = tfx_rendering->image_data;
	tfx_gpu_shapes shapes = tfx_GetLibraryGPUShapes(library);
	zest_device device = zest_GetContextDevice(context);
	zest_buffer staging_buffer = zest_CreateStagingBuffer(device, tfx_GetGPUShapesSizeInBytes(shapes), tfx_GetGPUShapesArray(shapes));
	zest_queue queue = zest_imm_BeginCommandBuffer(device, zest_queue_transfer);
	zest_imm_CopyBuffer(queue, staging_buffer, image_data_buffer, tfx_GetGPUShapesSizeInBytes(shapes));
	zest_imm_EndCommandBuffer(queue);
	zest_FreeBuffer(staging_buffer);
}

void zest_tfx_DrawParticleLayer(const zest_command_list command_list, void *user_data) {
	tfx_render_resources_t *tfx_resources = (tfx_render_resources_t *)user_data;
	zest_layer layer = zest_GetLayer(tfx_resources->layer);

	zest_buffer device_buffer = zest_GetLayerVertexBuffer(layer);
	zest_cmd_BindVertexBuffer(command_list, 0, 1, device_buffer);

	zest_pipeline current_pipeline = 0;
	zest_uniform_buffer uniform_buffer = zest_GetUniformBuffer(tfx_resources->uniform_buffer);

	zest_layer_instruction_t *current = zest_NextLayerInstruction(layer);
	while(current) {

		zest_cmd_LayerViewport(command_list, layer);

		zest_pipeline pipeline = zest_GetPipeline(current->pipeline_template, command_list);
		if (pipeline && current_pipeline != pipeline) {
			current_pipeline = pipeline;
			zest_cmd_BindPipeline(command_list, pipeline);
		} else if(!pipeline) {
			current = zest_NextLayerInstruction(layer);
			continue;
		}

		tfx_push_constants_t *push_constants = (tfx_push_constants_t *)current->push_constant;
		push_constants->color_ramp_texture_index = tfx_resources->color_ramps_index;
		push_constants->particle_texture_index = tfx_resources->particle_texture_index;
		push_constants->sampler_index = tfx_resources->sampler_index;
		push_constants->image_data_index = tfx_resources->image_data_index;
		push_constants->prev_billboards_index = zest_GetLayerVertexDescriptorIndex(layer, true);
		push_constants->uniform_index = zest_GetUniformBufferDescriptorIndex(uniform_buffer);

		zest_cmd_SendPushConstants(command_list, push_constants, sizeof(tfx_push_constants_t));

		zest_cmd_DrawLayerInstruction(command_list, 6, current);

		current = zest_NextLayerInstruction(layer);
	}
}

//A simple example to render the particles. This is for when the particle manager has one single list of sprites rather than grouped by effect
void zest_tfx_RenderParticles(tfx_effect_manager pm, tfx_render_resources_t *resources) {
	zest_layer layer = zest_GetLayer(resources->layer);
	//Let our renderer know that we want to draw to the timelinefx layer.
	zest_StartInstanceDrawing(layer, resources->pipeline);

	tfx_instance_t *billboards = tfx_GetInstanceBuffer(pm);
	int instance_count = tfx_GetInstanceCount(pm);
	zest_draw_buffer_result result = zest_DrawInstanceBuffer(layer, billboards, tfx_GetInstanceCount(pm));
	/*
	if (context->renderer->total_frame_count >= 200 && context->renderer->total_frame_count <= 220) {
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
	zest_layer layer = zest_GetLayer(resources->layer);
	//Let our renderer know that we want to draw to the timelinefx layer.
	zest_StartInstanceDrawing(layer, resources->pipeline);

	tfx_instance_t *billboards = NULL;
	tfx_effect_instance_data_t *instance_data;
	tfxU32 instance_count = 0;
	//Loop over the effects to get each instance buffer to render
	while (tfx_GetNextInstanceBuffer(pm, &billboards, &instance_data, &instance_count)) {
		zest_draw_buffer_result result = zest_DrawInstanceBuffer(layer, billboards, instance_count);
	}
	tfx_ResetInstanceBufferLoopIndex(pm);
}