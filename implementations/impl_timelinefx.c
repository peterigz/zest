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
	tfx_library_render_resources_t *resources = (tfx_library_render_resources_t *)custom_data;

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
void zest_tfx_UpdateUniformBuffer(zest_context context, tfx_library_render_resources_t *resources) {
	zest_uniform_buffer buffer = zest_GetUniformBuffer(resources->uniform_buffer);
	tfx_uniform_buffer_data_t *uniform_buffer = (tfx_uniform_buffer_data_t*)zest_GetUniformBufferData(buffer);
	uniform_buffer->view = zest_LookAt(resources->camera.position, zest_AddVec3(resources->camera.position, resources->camera.front), resources->camera.up);
	uniform_buffer->proj = zest_Perspective(resources->camera.fov, zest_ScreenWidthf(context) / zest_ScreenHeightf(context), 0.1f, 10000.f);
	uniform_buffer->proj.v[1].y *= -1.f;
	uniform_buffer->screen_size.x = zest_ScreenWidthf(context);
	uniform_buffer->screen_size.y = zest_ScreenHeightf(context);
	uniform_buffer->timer_lerp = (float)zest_TimerLerp(&resources->timer);
	uniform_buffer->update_time = (float)zest_TimerUpdateTime(&resources->timer);
	zest_CalculateFrustumPlanes(&uniform_buffer->view, &uniform_buffer->proj, resources->planes);
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

void zest_tfx_InitTimelineFXRenderResources(zest_context context, tfx_library_render_resources_t *resources, const char *vert_shader, const char *frag_shader) {
	zest_device device = zest_GetContextDevice(context);
	resources->uniform_buffer = zest_CreateUniformBuffer(context, "tfx uniform", sizeof(tfx_uniform_buffer_data_t));

	resources->timer = zest_CreateTimer(60);

	resources->camera = zest_CreateCamera();
	zest_CameraSetFoV(&resources->camera, 60.f);
	//float cam_pos[3] = {0.f, 3.f, 7.5};
	//zest_CameraPosition(&resources->camera, cam_pos);
	zest_CameraUpdateFront(&resources->camera);

	zest_tfx_UpdateUniformBuffer(context, resources);

	//Compile the shaders we will use to render the particles and ribbons
	resources->particles.frag_shader = zest_CreateShaderFromFile(device, frag_shader, "tfx_frag.spv", zest_fragment_shader, NULL, true);
	resources->particles.vert_shader = zest_CreateShaderFromFile(device, vert_shader, "tfx_vertex.spv", zest_vertex_shader, NULL, true);
	resources->ribbons.frag_shader = zest_CreateShaderFromFile(device, "examples/assets/shaders/ribbon.frag", "tfx_ribbon_frag.spv", zest_fragment_shader, NULL, true);
	resources->ribbons.vert_shader = zest_CreateShaderFromFile(device, "examples/assets/shaders/ribbon_3d.vert", "tfx_ribbon_vert.spv", zest_vertex_shader, NULL, true);
	resources->ribbons.comp_shader = zest_CreateShaderFromFile(device, "examples/assets/shaders/ribbons.comp", "tfx_ribbon_comp.spv", zest_compute_shader, NULL, true);

	//To render the particles we setup a pipeline with the vertex attributes and shaders to render the particles.
	//First create a descriptor set layout, we need 2 samplers, one to sample the particle texture and another to sample the color ramps
	//We also need 2 storage buffers, one to access the image data in the vertex shader and the other to access the previous frame particles
	//so that they can be interpolated in between updates

	zest_uniform_buffer uniform_buffer = zest_GetUniformBuffer(resources->uniform_buffer);

	resources->particles.pipeline = zest_CreatePipelineTemplate(device, "Timelinefx pipeline");
	//Set up the vertex attributes that will take in all of the billboard data stored in tfx_instance_t objects
	zest_AddVertexInputBindingDescription(resources->particles.pipeline, 0, sizeof(tfx_instance_t), zest_input_rate_instance);
	zest_AddVertexAttribute(resources->particles.pipeline, 0, 0, zest_format_r32g32b32a32_sfloat, offsetof(tfx_instance_t, position));	            // Location 0: Postion and stretch in w
	zest_AddVertexAttribute(resources->particles.pipeline, 0, 1, zest_format_r16g16b16a16_snorm, offsetof(tfx_instance_t, quaternion));	                // Location 1: Quaternion (packed)
	zest_AddVertexAttribute(resources->particles.pipeline, 0, 2, zest_format_r16g16b16a16_sscaled, offsetof(tfx_instance_t, size_handle));		    // Location 3: Size and handle of the sprite
	zest_AddVertexAttribute(resources->particles.pipeline, 0, 3, zest_format_r8g8b8_snorm, offsetof(tfx_instance_t, alignment));					    // Location 2: Alignment
	zest_AddVertexAttribute(resources->particles.pipeline, 0, 4, zest_format_r16g16_sscaled, offsetof(tfx_instance_t, intensity_gradient_map));       // Location 4: 2 intensities for each color
	zest_AddVertexAttribute(resources->particles.pipeline, 0, 5, zest_format_r8g8b8_unorm, offsetof(tfx_instance_t, curved_alpha_life));          	// Location 5: Sharpness and mix lerp value
	zest_AddVertexAttribute(resources->particles.pipeline, 0, 6, zest_format_r32_uint, offsetof(tfx_instance_t, indexes));							// Location 6: texture indexes to sample the correct image and color ramp
	zest_AddVertexAttribute(resources->particles.pipeline, 0, 7, zest_format_r32_uint, offsetof(tfx_instance_t, captured_index));   				    // Location 7: index of the sprite in the previous buffer when double buffering
	//Set the shaders to our custom timelinefx shaders
	zest_SetPipelineShaders(resources->particles.pipeline, resources->particles.vert_shader, resources->particles.frag_shader);
	zest_SetPipelineDepthTest(resources->particles.pipeline, true, false);
	zest_SetPipelineBlend(resources->particles.pipeline, zest_PreMultiplyBlendState());

	//We want to be able to manually change the current frame in flight in the layer that we use to draw all the billboards.
	//This means that we are able to only change the current frame in flight if we actually updated the particle manager in the current
	//frame allowing us to dictate when to upload the instance buffer to the gpu as there's no need to do it every frame, only when 
	//the particle manager is actually updated.
	resources->layer = zest_CreateFIFInstanceLayer(context, "TimelineFX Layer", sizeof(tfx_instance_t), 50000);
	zest_AcquireInstanceLayerBufferIndex(device, zest_GetLayer(resources->layer));

	zest_sampler_info_t sampler_info = zest_CreateSamplerInfo();
	resources->sampler = zest_CreateSampler(device, &sampler_info);
	zest_sampler sampler = zest_GetSampler(resources->sampler);
	resources->sampler_index = zest_AcquireSamplerIndex(device, sampler);

	//Create a buffer to store the image data on the gpu.
	zest_buffer_info_t image_data_buffer_info = zest_CreateBufferInfo(zest_buffer_type_storage, zest_memory_usage_gpu_only);
	resources->image_data = zest_CreateBuffer(device, sizeof(tfx_gpu_image_data_t) * 1000, &image_data_buffer_info);
	resources->image_data_index = zest_AcquireStorageBufferIndex(device, resources->image_data);

	resources->timeline = zest_CreateExecutionTimeline(device);

	//Configure Ribbons

    //Set up the compute shader
    //Create a new empty compute shader in the renderer

    resources->ribbons.ribbon_compute = zest_CreateCompute(device, "ribbon_emitters", resources->ribbons.comp_shader);

    zest_pipeline_template ribbon_pipeline  = zest_CreatePipelineTemplate(device, "ribbon render pipeline");
    //Set up the vertex attributes that will take in all of the billboard data stored in tfx_instance_t objects
    zest_AddVertexInputBindingDescription(ribbon_pipeline, 0, sizeof(tfx_ribbon_vertex_t), zest_input_rate_vertex);
	zest_AddVertexAttribute(ribbon_pipeline, 0, 0, zest_format_r32g32b32_sfloat, offsetof(tfx_ribbon_vertex_t, position));
    zest_AddVertexAttribute(ribbon_pipeline, 0, 1, zest_format_r32_uint, offsetof(tfx_ribbon_vertex_t, segment_index));
    zest_AddVertexAttribute(ribbon_pipeline, 0, 2, zest_format_r32g32_sfloat, offsetof(tfx_ribbon_vertex_t, uv_offset_scale));
    zest_AddVertexAttribute(ribbon_pipeline, 0, 3, zest_format_r32_uint, offsetof(tfx_ribbon_vertex_t, ribbon_index));
    zest_AddVertexAttribute(ribbon_pipeline, 0, 4, zest_format_r32_uint, offsetof(tfx_ribbon_vertex_t, clipped));
    //Set the shaders to our custom timelinefx shaders
    zest_SetPipelineVertShader(ribbon_pipeline, resources->ribbons.vert_shader);
    zest_SetPipelineFragShader(ribbon_pipeline, resources->ribbons.frag_shader);
	zest_SetPipelineBlend(ribbon_pipeline, zest_PreMultiplyBlendState());
	zest_SetPipelineDepthTest(ribbon_pipeline, true, false);
	resources->ribbons.pipeline = ribbon_pipeline;

}

void zest_tfx_CreateBuffersForEffects(zest_context context, tfx_effect_manager pm, tfx_library_render_resources_t *resources) {
    resources->ribbons.ribbon_buffer_info = tfx_GenerateRibbonBufferInfo(1);
	zest_buffer_info_t storage_buffer_info = zest_CreateBufferInfo(zest_buffer_type_storage, zest_memory_usage_gpu_only);
	zest_device device = zest_GetContextDevice(context);
    zest_ForEachFrameInFlight(i) {
		resources->ribbons.ribbon_lookup_buffer[i] = zest_CreateBuffer(device, sizeof(tfx_gpu_graph_data_t) * 1000, &storage_buffer_info);   //If this has to be resized then we need to rebind the buffer to the compute shader
		resources->ribbons.ribbon_lookup_index[i] = zest_AcquireStorageBufferIndex(device, resources->ribbons.ribbon_lookup_buffer[i]);
        resources->ribbons.ribbon_staging_buffer[i] = zest_CreateStagingBuffer(device, tfx_GetSegmentBufferMaxSizeInBytes(pm), 0);
        resources->ribbons.ribbon_instance_staging_buffer[i] = zest_CreateStagingBuffer(device, tfx_GetRibbonBufferMaxSizeInBytes(pm, 1000), 0);
        resources->ribbons.emitter_staging_buffer[i] = zest_CreateStagingBuffer(device, tfx_GetEmitterBufferMaxSizeInBytes(pm), 0);
		resources->ribbons.lookup_table_dirty[i] = true;
    }
}

void zest_tfx_FreeLibraryImages(tfx_library_render_resources_t *resources) {
	zest_FreeImageCollection(&resources->particle_images);
	zest_FreeImageCollection(&resources->color_ramps_collection);
}

void zest_tfx_UpdateTimelineFXImageData(zest_context context, tfx_library_render_resources_t *tfx_rendering, tfx_gpu_shapes shapes) {
	//Upload the timelinefx image data to the image data buffer created
	zest_buffer image_data_buffer = tfx_rendering->image_data;
	zest_device device = zest_GetContextDevice(context);
	zest_buffer staging_buffer = zest_CreateStagingBuffer(device, tfx_GetGPUShapesSizeInBytes(shapes), tfx_GetGPUShapesArray(shapes));
	zest_queue queue = zest_imm_BeginCommandBuffer(device, zest_queue_transfer);
	zest_imm_CopyBuffer(queue, staging_buffer, image_data_buffer, tfx_GetGPUShapesSizeInBytes(shapes));
	zest_imm_EndCommandBuffer(queue);
	zest_FreeBuffer(staging_buffer);
}

void zest_tfx_DrawParticleLayer(const zest_command_list command_list, void *user_data) {
	tfx_library_render_resources_t *tfx_resources = (tfx_library_render_resources_t *)user_data;
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
void zest_tfx_RenderParticles(tfx_effect_manager pm, tfx_library_render_resources_t *resources) {
	zest_layer layer = zest_GetLayer(resources->layer);
	//Let our renderer know that we want to draw to the timelinefx layer.
	zest_StartInstanceDrawing(layer, resources->particles.pipeline);

	tfx_instance_t *billboards = tfx_GetInstanceBuffer(pm);
	int instance_count = tfx_GetInstanceCount(pm);
	zest_draw_buffer_result result = zest_DrawInstanceBuffer(layer, billboards, tfx_GetInstanceCount(pm));
}

void zest_tfx_RenderParticlesByEffect(tfx_effect_manager pm, tfx_library_render_resources_t *resources) {
	zest_layer layer = zest_GetLayer(resources->layer);
	//Let our renderer know that we want to draw to the timelinefx layer.
	zest_StartInstanceDrawing(layer, resources->particles.pipeline);

	tfx_instance_t *billboards = NULL;
	tfx_effect_instance_data_t *instance_data;
	tfxU32 instance_count = 0;
	//Loop over the effects to get each instance buffer to render
	while (tfx_GetNextInstanceBuffer(pm, &billboards, &instance_data, &instance_count)) {
		zest_draw_buffer_result result = zest_DrawInstanceBuffer(layer, billboards, instance_count);
	}
	tfx_ResetInstanceBufferLoopIndex(pm);
}

void zest_tfx_UploadRibbonData(const zest_command_list command_list, void *user_data) {
	tfx_library_render_resources_t *resources = (tfx_library_render_resources_t*)(user_data);

	zest_context context = zest_GetContext(command_list);
	zest_uint fif = zest_CurrentFIF(context);

	// Get the transient buffers declared in the frame graph by name
	zest_buffer segment_buffer = zest_GetPassOutputBuffer(command_list, "Ribbon Segment Buffer");
	zest_buffer ribbon_instance_buffer = zest_GetPassOutputBuffer(command_list, "Ribbon Instance Buffer");
	zest_buffer emitter_buffer = zest_GetPassOutputBuffer(command_list, "Emitter Buffer");

	tfx_ribbon_buffer_requirements_t ribbon_buffer_requirements = tfx_GetRibbonBufferRequirements();
	if (segment_buffer) {
		zest_cmd_CopyBuffer(command_list, resources->ribbons.ribbon_staging_buffer[fif], segment_buffer, ribbon_buffer_requirements.segment_buffer_size_in_bytes);
	}
	if (ribbon_instance_buffer) {
		zest_cmd_CopyBuffer(command_list, resources->ribbons.ribbon_instance_staging_buffer[fif], ribbon_instance_buffer, ribbon_buffer_requirements.ribbon_buffer_size_in_bytes);
	}
	if (emitter_buffer) {
		zest_cmd_CopyBuffer(command_list, resources->ribbons.emitter_staging_buffer[fif], emitter_buffer, ribbon_buffer_requirements.emitter_buffer_size_in_bytes);
	}
	if (resources->ribbons.lookup_table_dirty[fif]) {
		zest_cmd_CopyBuffer(command_list, resources->ribbons.lookup_tables_staging, resources->ribbons.ribbon_lookup_buffer[fif], tfx_GetGPUGraphLookupsBufferSizeInBytes());
		resources->ribbons.lookup_table_dirty[fif] = false;
	}
}

void zest_tfx_UploadRibbonLookupData(zest_context context, tfx_library_render_resources_t *resources) {
	zest_uint fif = zest_CurrentFIF(context);
	zest_ForEachFrameInFlight(fif) {
		zest_device device = zest_GetContextDevice(context);
		zest_buffer staging = zest_CreateStagingBuffer(device, tfx_GetGPUGraphLookupsBufferSizeInBytes(), tfx_GetGPUGraphLookupsBuffer());
		if (zest_ResizeBuffer(&resources->ribbons.ribbon_lookup_buffer[fif], tfx_GetGPUGraphLookupsBufferSizeInBytes())) {
			zest_ReleaseStorageBufferIndex(device, resources->ribbons.ribbon_lookup_index[fif]);
			resources->ribbons.ribbon_lookup_index[fif] = zest_AcquireStorageBufferIndex(device, resources->ribbons.ribbon_lookup_buffer[fif]);
		}
		zest_queue queue = zest_imm_BeginCommandBuffer(device, zest_queue_transfer);
		zest_imm_CopyBuffer(queue, staging, resources->ribbons.ribbon_lookup_buffer[fif], zest_BufferSize(staging));
		zest_imm_EndCommandBuffer(queue);
		zest_FreeBuffer(staging);
		resources->ribbons.lookup_table_dirty[fif] = false;
	}
}

void zest_tfx_RibbonComputeFunction(const zest_command_list command_list, void *user_data) {
	tfx_library_render_resources_t *resources = (tfx_library_render_resources_t*)(user_data);

	zest_context context = zest_GetContext(command_list);
	zest_uint fif = zest_CurrentFIF(context);
	zest_resource_node ribbon_segments = zest_GetPassInputResource(command_list, "Ribbon Segment Buffer");
	zest_resource_node ribbon_instances = zest_GetPassInputResource(command_list, "Ribbon Instance Buffer");
	zest_resource_node emitters = zest_GetPassInputResource(command_list, "Emitter Buffer");
	zest_resource_node vertexes = zest_GetPassOutputResource(command_list, "Ribbon Vertex Buffer");
	zest_resource_node indexes = zest_GetPassOutputResource(command_list, "Ribbon Index Buffer");

	zest_cmd_BindComputePipeline(command_list, zest_GetCompute(resources->ribbons.ribbon_compute));

	tfx_ribbon_dispatch_t ribbon_dispatch = tfx_CreateRibbonDispatch();
	while (tfx_NextRibbonDispatch(&ribbon_dispatch)) {
		tfx_ribbon_bucket_globals_t *push = &ribbon_dispatch.ribbon_data->globals;
		push->lerp = (float)zest_TimerLerp(&resources->timer);
		zest_uniform_buffer uniform_buffer = zest_GetUniformBuffer(resources->uniform_buffer);
		push->uniform_index = zest_GetUniformBufferDescriptorIndex(uniform_buffer);
		push->graphs_index = resources->ribbons.ribbon_lookup_index[fif];
		push->emitters_index = zest_GetTransientBufferBindlessIndex(command_list, emitters);
		push->ribbon_segments_index = zest_GetTransientBufferBindlessIndex(command_list, ribbon_segments);
		push->ribbons_index = zest_GetTransientBufferBindlessIndex(command_list, ribbon_instances);
		push->vertexes_index = zest_GetTransientBufferBindlessIndex(command_list, vertexes);
		push->indexes_index = zest_GetTransientBufferBindlessIndex(command_list, indexes);
		zest_cmd_SendPushConstants(command_list, push, sizeof(tfx_ribbon_bucket_globals_t));

		zest_cmd_DispatchCompute(command_list, (ribbon_dispatch.total_segments / 1024) + 1, 1, 1);
	}
}

void zest_tfx_RenderRibbons(const zest_command_list command_list, void *user_data) {
	tfx_library_render_resources_t *resources = (tfx_library_render_resources_t*)(user_data);

	zest_buffer vertex_buffer = zest_GetPassInputBuffer(command_list, "Ribbon Vertex Buffer");
	zest_buffer index_buffer = zest_GetPassInputBuffer(command_list, "Ribbon Index Buffer");

	//Bind the buffer that contains the sprite instances to draw. These are updated by the compute shader on the GPU
	zest_cmd_BindVertexBuffer(command_list, 0, 1, vertex_buffer);
	zest_cmd_BindIndexBuffer(command_list, index_buffer);

	//Draw all the sprites in the buffer that is built by the compute shader
	zest_pipeline pipeline = zest_GetPipeline(resources->ribbons.pipeline, command_list);
	zest_cmd_BindPipeline(command_list, pipeline);

	zest_layer layer = zest_GetLayer(resources->layer);
	zest_cmd_LayerViewport(command_list, layer);

	tfx_ribbon_dispatch_t ribbon_dispatch = tfx_CreateRibbonDispatch();
	while (tfx_NextRibbonDispatch(&ribbon_dispatch)) {
		tfx_ribbon_bucket_globals_t *push = &ribbon_dispatch.ribbon_data->globals;
		push->particle_texture_index = resources->particle_texture_index;
		push->sampler_index = resources->sampler_index;
		push->color_ramp_texture_index = resources->color_ramps_index;
		push->image_data_index = resources->image_data_index;
		zest_cmd_SendPushConstants(command_list, &ribbon_dispatch.ribbon_data->globals, sizeof(tfx_ribbon_bucket_globals_t));
		zest_cmd_DrawIndexed(command_list, ribbon_dispatch.index_count, 1, ribbon_dispatch.index_offset, 0, 0);
	}
}

void zest_tfx_AddRibbonsToFrameGraph(tfx_effect_manager pm, tfx_library_render_resources_t *resources, zest_resource_node output_resource) {
	zest_buffer_resource_info_t segment_buffer_info = { 0, tfx_GetSegmentBufferMaxSizeInBytes(pm) };
	zest_buffer_resource_info_t instance_buffer_info = { 0, tfx_GetRibbonBufferMaxSizeInBytes(pm, 1000) };
	zest_buffer_resource_info_t vertex_buffer_info = { zest_resource_usage_hint_vertex_buffer, tfx_GetSegmentVertexBufferMaxSizeInBytes(pm, 0) };
	zest_buffer_resource_info_t index_buffer_info = { zest_resource_usage_hint_index_buffer, tfx_GetSegmentIndexBufferMaxSizeInBytes(pm) };
	zest_buffer_resource_info_t emitter_buffer_info = { 0, tfx_GetEmitterBufferMaxSizeInBytes(pm) };
	zest_resource_node ribbon_segment_buffer = zest_AddTransientBufferResource("Ribbon Segment Buffer", &segment_buffer_info);
	zest_resource_node ribbon_instance_buffer = zest_AddTransientBufferResource("Ribbon Instance Buffer", &instance_buffer_info);
	zest_resource_node emitter_buffer = zest_AddTransientBufferResource("Emitter Buffer", &emitter_buffer_info);
	zest_resource_node ribbon_vertex_buffer = zest_AddTransientBufferResource("Ribbon Vertex Buffer", &vertex_buffer_info);
	zest_resource_node ribbon_index_buffer = zest_AddTransientBufferResource("Ribbon Index Buffer", &index_buffer_info);

	zest_BeginTransferPass("Transfer Ribbon Data"); {
		zest_ConnectOutput(ribbon_segment_buffer);
		zest_ConnectOutput(ribbon_instance_buffer);
		zest_ConnectOutput(emitter_buffer);
		zest_SetPassTask(zest_tfx_UploadRibbonData, resources);
		zest_EndPass();
	}

	zest_BeginComputePass("Compute Ribbons"); {
		zest_ConnectInput(ribbon_segment_buffer);
		zest_ConnectInput(ribbon_instance_buffer);
		zest_ConnectInput(emitter_buffer);
		zest_ConnectOutput(ribbon_vertex_buffer);
		zest_ConnectOutput(ribbon_index_buffer);
		zest_SetPassTask(zest_tfx_RibbonComputeFunction, resources);
		zest_EndPass();
	}

	zest_BeginRenderPass("Draw Ribbons Pass"); {
		zest_ConnectInput(ribbon_vertex_buffer);
		zest_ConnectInput(ribbon_index_buffer);
		if (output_resource) {
			zest_ConnectOutput(output_resource);
		} else {
			zest_ConnectSwapChainOutput();
		}
		zest_SetPassTask(zest_tfx_RenderRibbons, resources);
		zest_EndPass();
	}
}
