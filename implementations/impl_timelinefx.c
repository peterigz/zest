#include "impl_timelinefx.h"
#include "stb_image.h"

//tfx keeps the pointer the shape loader hands it for as long as the shape is in the library, so a record
//is allocated on its own and never moves. Only the pointer list grows, which it is free to do.
ZEST_PRIVATE zest_tfx_shape_image_t *zest__tfx_add_shape_record(tfx_library_render_resources_t *resources) {
	if (resources->shape_image_count == resources->shape_image_capacity) {
		zest_uint capacity = resources->shape_image_capacity ? resources->shape_image_capacity * 2 : 64;
		zest_tfx_shape_image_t **images = (zest_tfx_shape_image_t **)ZEST_UTILITIES_MALLOC(sizeof(zest_tfx_shape_image_t *) * capacity);
		if (!images) {
			return NULL;
		}
		if (resources->shape_images) {
			memcpy(images, resources->shape_images, sizeof(zest_tfx_shape_image_t *) * resources->shape_image_count);
			ZEST_UTILITIES_FREE(resources->shape_images);
		}
		resources->shape_images = images;
		resources->shape_image_capacity = capacity;
	}
	zest_tfx_shape_image_t *record = (zest_tfx_shape_image_t *)ZEST_UTILITIES_MALLOC(sizeof(zest_tfx_shape_image_t));
	if (!record) {
		return NULL;
	}
	*record = ZEST__ZERO_INIT(zest_tfx_shape_image_t);
	resources->shape_images[resources->shape_image_count++] = record;
	return record;
}

//The sheet comes from stb_image so it goes back the same way rather than through zest_FreeBitmap. When the
//decoder could not read it the sheet is tfx's own buffer and is not ours to release.
ZEST_PRIVATE void zest__tfx_free_shape_pixels(zest_tfx_shape_image_t *record) {
	if (record->pixels_owned && record->pixels.data) {
		free(record->pixels.data);
	}
	record->pixels.data = NULL;
	record->pixels_owned = ZEST_FALSE;
}

ZEST_PRIVATE void zest__tfx_free_shape_record(tfx_library_render_resources_t *resources, zest_uint index) {
	zest_tfx_shape_image_t *record = resources->shape_images[index];
	//zest_FreeImage releases the image's bindless index as part of the deferred cleanup, which is the right
	//time: the descriptor can still be read by frames in flight until then. Releasing it here as well would
	//hand the index out again while the cleanup still means to free it.
	if (record->image.value) {
		zest_FreeImage(record->image);
	}
	zest__tfx_free_shape_pixels(record);
	ZEST_UTILITIES_FREE(record);
	resources->shape_images[index] = resources->shape_images[--resources->shape_image_count];
}

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
	bool pixels_not_loaded = !pixels ? true : false;
	if (pixels_not_loaded) {
		pixels = (stbi_uc *)raw_image_data;
		width = tfx_GetImageWidth(image_data);
		height = tfx_GetImageHeight(image_data);
	}

	zest_tfx_shape_image_t *record = zest__tfx_add_shape_record(resources);
	if (!record) {
		if (!pixels_not_loaded) {
			free(pixels);
		}
		return;
	}

	//The sheet is kept whole and its frames are cut straight into the staging buffer when the image is made,
	//so an animation costs one allocation here rather than one per frame.
	record->pixels.data = (zest_byte *)pixels;
	record->pixels.meta.width = width;
	record->pixels.meta.height = height;
	record->pixels.meta.channels = 4;
	record->pixels.meta.bytes_per_pixel = 4;
	record->pixels.meta.stride = width * 4;
	record->pixels.meta.size = (zest_size)width * height * 4;
	record->pixels.meta.format = zest_format_r8g8b8a8_unorm;
	record->pixels.is_imported = ZEST_TRUE;
	record->pixels_owned = pixels_not_loaded ? ZEST_FALSE : ZEST_TRUE;

	zest_uint frames = (zest_uint)tfx_GetImageFrameCount(image_data);
	record->frames = frames > 1 ? frames : 1;
	record->frame_width = record->frames > 1 ? (zest_uint)tfx_GetImageWidth(image_data) : (zest_uint)width;
	record->frame_height = record->frames > 1 ? (zest_uint)tfx_GetImageHeight(image_data) : (zest_uint)height;
	resources->pending_count++;

	//Important step: the record is what the uv lookup is handed for this shape, which is how a particle
	//finds the image made for it below.
	tfx_SetImagePointer(image_data, record);
}

//Create one layered image per particle shape. The frames of an animated shape become the array layers of
//that one image, so an animation costs a single descriptor like any other shape. Only the shapes the loader
//has just queued are processed, so a refresh only uploads what it added.
static void zest__tfx_upload_pending_shapes(zest_context context, tfx_library_render_resources_t *resources) {
	if (!resources->pending_count) {
		return;
	}
	zest_device device = zest_GetContextDevice(context);
	zest_uint first_pending = resources->shape_image_count - resources->pending_count;

	zest_size total_size = 0;
	for (zest_uint i = first_pending; i != resources->shape_image_count; ++i) {
		zest_tfx_shape_image_t *record = resources->shape_images[i];
		total_size += (zest_size)record->frames * record->frame_width * record->frame_height * 4;
	}

	zest_buffer staging_buffer = zest_CreateDedicatedStagingBuffer(device, total_size, 0);
	if (!staging_buffer) {
		return;
	}

	//A shape's frames go into the staging buffer back to back so that they upload as one region. A shape
	//with a single frame is the same copy with nothing to step over.
	zest_byte *staging_data = (zest_byte *)zest_BufferData(staging_buffer);
	//A sheet whose grid does not hold every frame it claims leaves the frames past the end of the grid
	//uncopied below, so the buffer starts blank rather than uploading whatever was in host memory.
	memset(staging_data, 0, total_size);
	zest_size staging_offset = 0;
	for (zest_uint i = first_pending; i != resources->shape_image_count; ++i) {
		zest_tfx_shape_image_t *record = resources->shape_images[i];
		zest_uint columns = record->frame_width ? record->pixels.meta.width / record->frame_width : 1;
		if (!columns) {
			columns = 1;
		}
		for (zest_uint f = 0; f != record->frames; ++f) {
			zest_bitmap_t frame = ZEST__ZERO_INIT(zest_bitmap_t);
			frame.meta = record->pixels.meta;
			frame.meta.width = record->frame_width;
			frame.meta.height = record->frame_height;
			frame.meta.stride = record->frame_width * 4;
			frame.meta.size = (zest_size)frame.meta.stride * record->frame_height;
			frame.data = staging_data + staging_offset;
			frame.is_imported = ZEST_TRUE;
			zest_CopyBitmap(&record->pixels, (f % columns) * record->frame_width, (f / columns) * record->frame_height,
				record->frame_width, record->frame_height, &frame, 0, 0);
			staging_offset += frame.meta.size;
		}
	}

	zest_queue queue = zest_imm_BeginCommandBuffer(device, zest_queue_graphics);
	zest_size buffer_offset = 0;
	for (zest_uint i = first_pending; i != resources->shape_image_count; ++i) {
		zest_tfx_shape_image_t *record = resources->shape_images[i];
		zest_size shape_size = (zest_size)record->frames * record->frame_width * record->frame_height * 4;

		zest_image_info_t image_info = zest_CreateImageInfo(record->frame_width, record->frame_height);
		image_info.format = zest_format_r8g8b8a8_unorm;
		image_info.layer_count = record->frames;
		//Force the array view so that single frame shapes still sample as a texture2DArray in the shader
		image_info.flags = zest_image_preset_texture_mipmaps | zest_image_flag_force_image_array;
		record->image = zest_CreateImage(device, &image_info);
		if (!record->image.value) {
			//Nothing can be done for the shape now, tfx already has the record. Its bindless index is still
			//the zero it was created with, which addresses the device's default image.
			ZEST_PRINT("Unable to create an image for a particle shape. It will render as the default image.");
			buffer_offset += shape_size;
			continue;
		}
		zest_image image = zest_GetImage(record->image);
		zest_uint mip_levels = zest_ImageInfo(image)->mip_levels;

		zest_buffer_image_copy_t copy_region = ZEST__ZERO_INIT(zest_buffer_image_copy_t);
		copy_region.buffer_offset = buffer_offset;
		copy_region.image_aspect = zest_image_aspect_color_bit;
		copy_region.layer_count = record->frames;
		copy_region.image_extent.width = record->frame_width;
		copy_region.image_extent.height = record->frame_height;
		copy_region.image_extent.depth = 1;

		zest_imm_TransitionImage(queue, image, zest_resource_state_copy_dst, 0, mip_levels, 0, record->frames);
		zest_imm_CopyBufferRegionsToImage(queue, &copy_region, 1, staging_buffer, image);
		if (mip_levels > 1) {
			zest_imm_GenerateMipMaps(queue, image);
		} else {
			zest_imm_TransitionImage(queue, image, zest_resource_state_shader_read, 0, mip_levels, 0, record->frames);
		}
		buffer_offset += shape_size;
	}
	zest_imm_EndCommandBuffer(queue);
	zest_FreeBufferNow(staging_buffer);

	//Descriptor indexes can only be acquired once an image is in a layout valid for sampling, so the shapes
	//get walked a second time now that the uploads have run. The sheets have done their job by this point.
	for (zest_uint i = first_pending; i != resources->shape_image_count; ++i) {
		zest_tfx_shape_image_t *record = resources->shape_images[i];
		if (record->image.value) {
			record->bindless_index = zest_AcquireSampledImageIndex(device, zest_GetImage(record->image), zest_texture_array_binding);
		}
		zest__tfx_free_shape_pixels(record);
	}
	resources->pending_count = 0;
}

//tfx reports a removed shape by hash only and does not hand back the pointer the shape loader set, so the
//live set is read out of the gpu shape rebuild instead: it calls the uv lookup once for every shape still
//in the library, and the lookup marks the record it is given. Clear the marks, rebuild, then sweep.
static void zest__tfx_clear_shape_marks(tfx_library_render_resources_t *resources) {
	for (zest_uint i = 0; i != resources->shape_image_count; ++i) {
		resources->shape_images[i]->live = ZEST_FALSE;
	}
}

static zest_uint zest__tfx_sweep_shape_images(tfx_library_render_resources_t *resources) {
	zest_uint removed = 0;
	zest_uint i = 0;
	while (i < resources->shape_image_count) {
		if (resources->shape_images[i]->live) {
			++i;
			continue;
		}
		zest__tfx_free_shape_record(resources, i);
		removed++;
	}
	return removed;
}

//Basic function for updating the uniform buffer
void zest_tfx_UpdateUniformBuffer(zest_context context, tfx_library_render_resources_t *resources) {
	zest_uniform_buffer buffer = zest_GetUniformBuffer(resources->uniform_buffer);
	tfx_uniform_buffer_data_t *uniform_buffer = (tfx_uniform_buffer_data_t*)zest_GetUniformBufferData(buffer);
	tfx_uniform_buffer_data_t data;
	data.view = zest_LookAt(resources->camera.position, zest_AddVec3(resources->camera.position, resources->camera.front), resources->camera.up);
	data.proj = zest_Perspective(resources->camera.fov, zest_ScreenWidthf(context) / zest_ScreenHeightf(context), 0.1f, 10000.f);
	data.proj.v[1].y *= -1.f;
	data.screen_size.x = zest_ScreenWidthf(context);
	data.screen_size.y = zest_ScreenHeightf(context);
	data.millisecs = 0;
	data.timer_lerp = (float)zest_TimerLerp(&resources->timer);
	data.update_time = (float)zest_TimerUpdateTime(&resources->timer);
	zest_CalculateFrustumPlanes(&data.view, &data.proj, resources->planes);
	*uniform_buffer = data;
}

void zest_tfx_GetUV(void *ptr, tfx_gpu_image_data_t *image_data, int offset) {
	zest_tfx_shape_image_t *shape = (zest_tfx_shape_image_t *)ptr;
	//The rebuild this runs inside visits every shape still in the library, so it is also where a shape
	//proves it is still there. See zest__tfx_sweep_shape_images.
	shape->live = ZEST_TRUE;
	//Each shape owns its image so the whole 0..1 rect is the shape and the descriptor index travels with
	//the particle instead of arriving in the push constants. The frame is the array layer of that image.
	image_data->uv.x = 0.f;
	image_data->uv.y = 0.f;
	image_data->uv.z = 1.f;
	image_data->uv.w = 1.f;
	image_data->uv_packed = zest_Pack16bit4SNorm(0.f, 0.f, 1.f, 1.f);
	image_data->texture_array_index = (shape->bindless_index << 16) | ((zest_uint)offset & 0xFFFF);
}

tfx_library zest_tfx_LoadLibrary(zest_context context, tfx_library_render_resources_t *resources, const char *library_path) {
	resources->device = zest_GetContextDevice(context);
	tfx_library library = tfx_LoadEffectLibrary(library_path, zest_tfx_ShapeLoader, zest_tfx_GetUV, resources);
	zest__tfx_upload_pending_shapes(context, resources);
	return library;
}

//The library hands out colour ramp indexes by position in its bitmap list, and a refresh rebuilds that
//list from scratch, so the texture has to be built again from the current bitmaps whenever it changes.
static void zest__tfx_build_color_ramps_texture(zest_context context, tfx_library_render_resources_t *resources, tfx_library library) {
	zest_device device = zest_GetContextDevice(context);
	tfxU32 bitmap_count = tfx_GetColorRampBitmapCount(library);
	resources->color_ramps_collection = zest_CreateImageAtlasCollection(zest_format_r16g16b16a16_sfloat, bitmap_count);
	for (tfxU32 i = 0; i != bitmap_count; ++i) {
		tfx_bitmap_t *bitmap = tfx_GetColorRampBitmap(library, i);
		zest_AddImageAtlasPixels(&resources->color_ramps_collection, tfx_GetBitmapData(bitmap), tfx_GetBitmapSize(bitmap), tfx_GetBitmapWidth(bitmap), tfx_GetBitmapHeight(bitmap), zest_format_r16g16b16a16_sfloat);
	}
	resources->color_ramps_texture = zest_CreateImageAtlas(context, &resources->color_ramps_collection, 256, 256, zest_image_preset_texture);
	zest_image color_ramps_image = zest_GetImage(resources->color_ramps_texture);
	resources->color_ramps_index = zest_AcquireSampledImageIndex(device, color_ramps_image, zest_texture_array_binding);
}

void zest_tfx_FinaliseLibrary(zest_context context, tfx_library_render_resources_t *resources, tfx_library library) {
	zest__tfx_build_color_ramps_texture(context, resources, library);

	tfx_UpdateLibraryGPUImageData(library);
	zest_tfx_UpdateTimelineFXImageData(context, resources, tfx_GetLibraryGPUShapes(library));
	zest_tfx_UpdateTimelineFXParticleProperties(context, resources, library);
}

tfxErrorFlags zest_tfx_LoadSpriteData(zest_context context, tfx_library_render_resources_t *resources, const char *path, tfx_animation_manager animation_manager) {
	resources->device = zest_GetContextDevice(context);

	tfxErrorFlags result = tfx_LoadSpriteData(path, animation_manager, zest_tfx_ShapeLoader, resources);
	if (result != 0) return result;

	zest__tfx_upload_pending_shapes(context, resources);
	return result;
}

void zest_tfx_FinaliseSpriteData(zest_context context, tfx_library_render_resources_t *resources, tfx_animation_manager animation_manager, tfx_gpu_shapes gpu_image_data) {
	zest_device device = zest_GetContextDevice(context);

	tfxU32 bitmap_count = tfx_GetAnimationColorRampBitmapCount(animation_manager);
	resources->color_ramps_collection = zest_CreateImageAtlasCollection(zest_format_r16g16b16a16_sfloat, bitmap_count);
	for (tfxU32 i = 0; i != bitmap_count; ++i) {
		tfx_bitmap_t *bitmap = tfx_GetAnimationColorRampBitmap(animation_manager, i);
		zest_AddImageAtlasPixels(&resources->color_ramps_collection, tfx_GetBitmapData(bitmap), tfx_GetBitmapSize(bitmap), tfx_GetBitmapWidth(bitmap), tfx_GetBitmapHeight(bitmap), zest_format_r16g16b16a16_sfloat);
	}
	resources->color_ramps_texture = zest_CreateImageAtlas(context, &resources->color_ramps_collection, 256, 256, zest_image_preset_texture);
	zest_image color_ramps_image = zest_GetImage(resources->color_ramps_texture);
	resources->color_ramps_index = zest_AcquireSampledImageIndex(device, color_ramps_image, zest_texture_array_binding);

	tfx_BuildAnimationManagerGPUShapeData(animation_manager, gpu_image_data, zest_tfx_GetUV);
	zest_tfx_UpdateTimelineFXImageData(context, resources, gpu_image_data);

	//Create GPU storage buffers for sprite data and emitter properties
	zest_buffer_info_t storage_buffer_info = zest_CreateBufferInfo(zest_buffer_type_storage, zest_memory_usage_gpu_only);
	resources->sprite_data_buffer = zest_CreateBuffer(device, tfx_GetSpriteDataSizeInBytes(animation_manager), &storage_buffer_info);
	resources->emitter_properties_buffer = zest_CreateBuffer(device, tfx_GetAnimationEmitterPropertySizeInBytes(animation_manager), &storage_buffer_info);

	//Upload sprite data and emitter properties to the GPU
	zest_queue queue = zest_imm_BeginCommandBuffer(device, zest_queue_transfer);
	zest_buffer sprite_staging = zest_CreateDedicatedStagingBuffer(device, tfx_GetSpriteDataSizeInBytes(animation_manager), tfx_GetSpriteDataBufferPointer(animation_manager));
	zest_imm_CopyBuffer(queue, sprite_staging, resources->sprite_data_buffer, tfx_GetSpriteDataSizeInBytes(animation_manager));
	zest_buffer emitter_staging = zest_CreateDedicatedStagingBuffer(device, tfx_GetAnimationEmitterPropertySizeInBytes(animation_manager), tfx_GetAnimationEmitterPropertiesBufferPointer(animation_manager));
	zest_imm_CopyBuffer(queue, emitter_staging, resources->emitter_properties_buffer, tfx_GetAnimationEmitterPropertySizeInBytes(animation_manager));
	zest_imm_EndCommandBuffer(queue);
	zest_FreeBufferNow(sprite_staging);
	zest_FreeBufferNow(emitter_staging);

	resources->sprite_data_index = zest_AcquireStorageBufferIndex(device, resources->sprite_data_buffer);
	resources->emitter_properties_index = zest_AcquireStorageBufferIndex(device, resources->emitter_properties_buffer);
}

void zest_tfx_InitTimelineFXRenderResources(zest_context context, tfx_library_render_resources_t *resources, zest_shader_handle vert_shader, zest_shader_handle frag_shader, zest_shader_handle ribbon_vert, zest_shader_handle ribbon_frag, zest_shader_handle ribbon_comp) {
	zest_device device = zest_GetContextDevice(context);
	resources->device = device;
	resources->uniform_buffer = zest_CreateUniformBuffer(context, "tfx uniform", sizeof(tfx_uniform_buffer_data_t));

	resources->timer = zest_CreateTimer(60);

	resources->camera = zest_CreateCamera();
	zest_CameraSetFoV(&resources->camera, 60.f);
	//float cam_pos[3] = {0.f, 3.f, 7.5};
	//zest_CameraPosition(&resources->camera, cam_pos);
	zest_CameraUpdateFront(&resources->camera);

	zest_tfx_UpdateUniformBuffer(context, resources);

	//Compile the shaders we will use to render the particles and ribbons
	resources->particles.frag_shader = frag_shader;
	resources->particles.vert_shader = vert_shader;
	resources->ribbon_rendering.frag_shader = ribbon_frag;
	resources->ribbon_rendering.vert_shader = ribbon_vert;
	resources->ribbon_rendering.comp_shader = ribbon_comp;

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
	zest_AddVertexAttribute(resources->particles.pipeline, 0, 2, zest_format_r16g16_sscaled, offsetof(tfx_instance_t, size));		    // Location 3: Size and handle of the sprite
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
	zest_buffer_info_t storage_buffer_info = zest_CreateBufferInfo(zest_buffer_type_storage, zest_memory_usage_gpu_only);
	resources->image_data = zest_CreateBuffer(device, sizeof(tfx_gpu_image_data_t) * 1000, &storage_buffer_info);
	resources->image_data_index = zest_AcquireStorageBufferIndex(device, resources->image_data);

	resources->particle_properties = zest_CreateBuffer(device, sizeof(tfx_gpu_particle_properties_t) * 1000, &storage_buffer_info);
	resources->particle_properties_index = zest_AcquireStorageBufferIndex(device, resources->particle_properties);

	//Don't actually need this, can remove?
	resources->timeline = zest_CreateExecutionTimeline(device);

	//Configure Ribbons

    //Set up the compute shader
    //Create a new empty compute shader in the renderer

    resources->ribbon_rendering.ribbon_compute = zest_CreateCompute(device, "ribbon_emitters", resources->ribbon_rendering.comp_shader);

    zest_pipeline_template ribbon_pipeline  = zest_CreatePipelineTemplate(device, "ribbon render pipeline");
    //Set up the vertex attributes that will take in all of the billboard data stored in tfx_instance_t objects
    zest_AddVertexInputBindingDescription(ribbon_pipeline, 0, sizeof(tfx_ribbon_vertex_t), zest_input_rate_vertex);
	zest_AddVertexAttribute(ribbon_pipeline, 0, 0, zest_format_r32g32b32_sfloat, offsetof(tfx_ribbon_vertex_t, position));
    zest_AddVertexAttribute(ribbon_pipeline, 0, 1, zest_format_r32_uint, offsetof(tfx_ribbon_vertex_t, segment_index));
    zest_AddVertexAttribute(ribbon_pipeline, 0, 2, zest_format_r32g32_sfloat, offsetof(tfx_ribbon_vertex_t, uv_offset_scale));
    zest_AddVertexAttribute(ribbon_pipeline, 0, 3, zest_format_r32_uint, offsetof(tfx_ribbon_vertex_t, ribbon_index));
    //No attribute for tfx_ribbon_vertex_t::clipped - the vertex shader stopped reading it when clipping
    //moved to the graph lookups. The binding stride stays the full struct size, the compute shader still writes it.
    //Set the shaders to our custom timelinefx shaders
    zest_SetPipelineVertShader(ribbon_pipeline, resources->ribbon_rendering.vert_shader);
    zest_SetPipelineFragShader(ribbon_pipeline, resources->ribbon_rendering.frag_shader);
	zest_SetPipelineBlend(ribbon_pipeline, zest_PreMultiplyBlendState());
	zest_SetPipelineDepthTest(ribbon_pipeline, true, false);
	resources->ribbon_rendering.pipeline = ribbon_pipeline;

}

void zest_tfx_CreateRibbonBuffers(zest_context context, tfx_ribbon_buffers_t *buffers) {
    buffers->ribbon_buffer_info = tfx_GenerateRibbonBufferInfo(1);
	zest_buffer_info_t storage_buffer_info = zest_CreateBufferInfo(zest_buffer_type_storage, zest_memory_usage_gpu_only);
	zest_device device = zest_GetContextDevice(context);
    zest_ForEachFrameInFlight(i) {
        buffers->ribbon_staging_buffer[i] = zest_CreateStagingBuffer(device, tfx_GetTotalSegmentBufferMaxSizeInBytes(), 0);
        buffers->ribbon_instance_staging_buffer[i] = zest_CreateStagingBuffer(device, tfx_GetTotalRibbonBufferMaxSizeInBytes(), 0);
        buffers->emitter_staging_buffer[i] = zest_CreateStagingBuffer(device, tfx_GetTotalEmitterBufferMaxSizeInBytes(), 0);
    }
}

void zest_tfx_CreateGlobalBuffers(zest_context context, tfx_global_library_buffers_t *buffers) {
	zest_buffer_info_t storage_buffer_info = zest_CreateBufferInfo(zest_buffer_type_storage, zest_memory_usage_gpu_only);
	zest_device device = zest_GetContextDevice(context);
    zest_ForEachFrameInFlight(i) {
		buffers->lookup_buffer[i] = zest_CreateBuffer(device, sizeof(tfx_gpu_graph_data_t) * 1000, &storage_buffer_info);
		buffers->lookup_index[i] = zest_AcquireStorageBufferIndex(device, buffers->lookup_buffer[i]);
		buffers->lookup_table_dirty[i] = true;
    }
}

zest_bool zest_tfx_RefreshLibrary(zest_context context, tfx_library_render_resources_t *resources, tfx_library library, tfx_global_library_buffers_t *global_buffers, tfx_library_refresh_t *refresh) {
	zest_device device = zest_GetContextDevice(context);
	memset(refresh, 0, sizeof(tfx_library_refresh_t));

	zest_uint images_before = resources->shape_image_count;

	tfx_RefreshLibrary(library, zest_tfx_ShapeLoader, zest_tfx_GetUV, resources, &refresh->result);

	tfxRefreshFlags applied = tfxRefreshFlags_shapes_changed | tfxRefreshFlags_merged
		| tfxRefreshFlags_effects_added | tfxRefreshFlags_effects_removed;
	if ((refresh->result.flags & applied) == 0) {
		//Either the file has not moved or the change needs a full reload, and nothing on the gpu has to move
		return ZEST_FALSE;
	}

	//What follows frees and overwrites resources that frames still in flight are reading
	zest_WaitForIdleDevice(device);

	zest_bool shapes_changed = (refresh->result.flags & tfxRefreshFlags_shapes_changed) != 0;
	if (shapes_changed) {
		//Only the shapes the refresh just added get uploaded, everything already resident is left alone
		zest__tfx_upload_pending_shapes(context, resources);
		refresh->images_added = resources->shape_image_count - images_before;
	}

	//The refresh built the gpu shape list while the new shapes still had no image, so it gets rebuilt now
	//that the uv lookup can answer for all of them. That also rebuilds each emitter's start_frame_index,
	//which is why the properties buffer goes up with it. The rebuild marks every shape the library still
	//has, so it is also what the sweep below reads to find the images of shapes that have gone.
	zest__tfx_clear_shape_marks(resources);
	tfx_UpdateLibraryGPUImageData(library);
	if (shapes_changed) {
		refresh->images_removed = zest__tfx_sweep_shape_images(resources);
	}

	//The refresh rebuilds the library's colour ramp bitmaps from scratch, which reassigns every ramp index
	//even for effects it did not touch, so the texture those indexes address has to be rebuilt with them.
	zest_FreeImage(resources->color_ramps_texture);
	zest_FreeImageCollection(&resources->color_ramps_collection);
	zest__tfx_build_color_ramps_texture(context, resources, library);

	zest_tfx_UpdateTimelineFXImageData(context, resources, tfx_GetLibraryGPUShapes(library));
	zest_tfx_UpdateTimelineFXParticleProperties(context, resources, library);

	//The refresh also rebuilds the global graph lookup table, moving every emitter's lookup offset, and the
	//ribbon compute shader indexes that table by offset. Nothing marks the buffer dirty on its own, so the
	//table is uploaded again here or the ribbons read curves belonging to some other emitter.
	if (global_buffers) {
		zest_tfx_InitialiseGlobalData(context, global_buffers);
	}

	return ZEST_TRUE;
}

void zest_tfx_FreeLibraryImages(tfx_library_render_resources_t *resources) {
	while (resources->shape_image_count) {
		zest__tfx_free_shape_record(resources, resources->shape_image_count - 1);
	}
	if (resources->shape_images) {
		ZEST_UTILITIES_FREE(resources->shape_images);
		resources->shape_images = 0;
		resources->shape_image_capacity = 0;
	}
	resources->pending_count = 0;
	zest_FreeImageCollection(&resources->color_ramps_collection);
}

void zest_tfx_UpdateTimelineFXImageData(zest_context context, tfx_library_render_resources_t *tfx_rendering, tfx_gpu_shapes shapes) {
	//Upload the timelinefx image data to the image data buffer created
	zest_buffer image_data_buffer = tfx_rendering->image_data;
	zest_device device = zest_GetContextDevice(context);
	zest_buffer staging_buffer = zest_CreateDedicatedStagingBuffer(device, tfx_GetGPUShapesSizeInBytes(shapes), tfx_GetGPUShapesArray(shapes));
	zest_queue queue = zest_imm_BeginCommandBuffer(device, zest_queue_transfer);
	zest_imm_CopyBuffer(queue, staging_buffer, image_data_buffer, tfx_GetGPUShapesSizeInBytes(shapes));
	zest_imm_EndCommandBuffer(queue);
	zest_FreeBufferNow(staging_buffer);
}

void zest_tfx_UpdateTimelineFXParticleProperties(zest_context context, tfx_library_render_resources_t *tfx_rendering, tfx_library library) {
	//Upload the timelinefx image data to the image data buffer created
	zest_buffer property_buffer = tfx_rendering->particle_properties;
	zest_device device = zest_GetContextDevice(context);
	zest_buffer staging_buffer = zest_CreateDedicatedStagingBuffer(device, tfx_GetParticlePropertiesBufferSizeInBytes(library), tfx_GetParticlePropertiesBuffer(library));
	zest_queue queue = zest_imm_BeginCommandBuffer(device, zest_queue_transfer);
	zest_imm_CopyBuffer(queue, staging_buffer, property_buffer, tfx_GetParticlePropertiesBufferSizeInBytes(library));
	zest_imm_EndCommandBuffer(queue);
	zest_FreeBufferNow(staging_buffer);
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
		push_constants->sampler_index = tfx_resources->sampler_index;
		push_constants->image_data_index = tfx_resources->image_data_index;
		push_constants->particle_properties_index = tfx_resources->particle_properties_index;
		push_constants->prev_billboards_index = zest_GetLayerVertexDescriptorIndex(layer, true);
		push_constants->uniform_index = zest_GetUniformBufferDescriptorIndex(uniform_buffer);

		zest_cmd_SendPushConstants(command_list, push_constants, sizeof(tfx_push_constants_t));

		zest_cmd_DrawLayerInstruction(command_list, 6, current);

		current = zest_NextLayerInstruction(layer);
	}
}

//A simple example to render the particles. This is for when the particle manager has one single list of sprites rather than grouped by effect
void zest_tfx_RenderParticles(tfx_stage pm, tfx_library_render_resources_t *resources) {
	zest_layer layer = zest_GetLayer(resources->layer);
	//Let our renderer know that we want to draw to the timelinefx layer.
	zest_StartInstanceDrawing(layer, resources->particles.pipeline);

	tfx_instance_t *billboards = tfx_GetInstanceBuffer(pm);
	int instance_count = tfx_GetInstanceCount(pm);
	zest_draw_buffer_result result = zest_DrawInstanceBuffer(layer, billboards, tfx_GetInstanceCount(pm));
}

void zest_tfx_RenderParticlesByEffect(tfx_stage pm, tfx_library_render_resources_t *resources) {
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
	tfx_ribbon_render_dispatch_t *render_dispatch = (tfx_ribbon_render_dispatch_t*)(user_data);

	zest_context context = zest_GetContext(command_list);
	zest_uint fif = zest_CurrentFIF(context);
	
	tfx_ribbon_buffer_requirements_t ribbon_buffer_requirements = tfx_GetRibbonBufferRequirements();
	if (render_dispatch->segment_buffer) {
		zest_cmd_CopyBuffer(command_list, render_dispatch->buffers->ribbon_staging_buffer[fif], zest_GetResourceBuffer(render_dispatch->segment_buffer), ribbon_buffer_requirements.segment_buffer_size_in_bytes);
	}
	if (render_dispatch->ribbon_instance_buffer) {
		zest_cmd_CopyBuffer(command_list, render_dispatch->buffers->ribbon_instance_staging_buffer[fif], zest_GetResourceBuffer(render_dispatch->ribbon_instance_buffer), ribbon_buffer_requirements.ribbon_buffer_size_in_bytes);
	}
	if (render_dispatch->emitter_buffer) {
		zest_cmd_CopyBuffer(command_list, render_dispatch->buffers->emitter_staging_buffer[fif], zest_GetResourceBuffer(render_dispatch->emitter_buffer), ribbon_buffer_requirements.emitter_buffer_size_in_bytes);
	}
}

void zest_tfx_UploadGraphData(const zest_command_list command_list, void *user_data) {
	tfx_global_library_buffers_t *buffers = (tfx_global_library_buffers_t*)(user_data);

	zest_context context = zest_GetContext(command_list);
	zest_uint fif = zest_CurrentFIF(context);
	
	if (buffers->lookup_table_dirty[fif]) {
		zest_cmd_CopyBuffer(command_list, buffers->lookup_tables_staging, buffers->lookup_buffer[fif], tfx_GetGPUGraphLookupsBufferSizeInBytes());
		buffers->lookup_table_dirty[fif] = false;
	}
}

void zest_tfx_InitialiseGlobalData(zest_context context, tfx_global_library_buffers_t *buffers) {
	zest_uint fif = zest_CurrentFIF(context);
	zest_ForEachFrameInFlight(fif) {
		zest_device device = zest_GetContextDevice(context);
		zest_buffer staging = zest_CreateDedicatedStagingBuffer(device, tfx_GetGPUGraphLookupsBufferSizeInBytes(), tfx_GetGPUGraphLookupsBuffer());
		if (zest_ResizeBuffer(&buffers->lookup_buffer[fif], tfx_GetGPUGraphLookupsBufferSizeInBytes())) {
			zest_ReleaseStorageBufferIndex(device, buffers->lookup_index[fif]);
			buffers->lookup_index[fif] = zest_AcquireStorageBufferIndex(device, buffers->lookup_buffer[fif]);
		}
		zest_queue queue = zest_imm_BeginCommandBuffer(device, zest_queue_transfer);
		zest_imm_CopyBuffer(queue, staging, buffers->lookup_buffer[fif], zest_BufferSize(staging));
		zest_imm_EndCommandBuffer(queue);
		zest_FreeBufferNow(staging);
		buffers->lookup_table_dirty[fif] = false;
	}
}

void zest_tfx_UpdateRibbonStagingBuffers(zest_context context, tfx_ribbon_buffers_t *buffers, tfx_stage pm) {
	if (tfx_HasRibbonsToDraw(pm)) {
		zest_uint fif = zest_CurrentFIF(context);
		tfx_CopyRibbonDataToStagingBuffers(pm,
			zest_BufferData(buffers->ribbon_staging_buffer[fif]),
			zest_BufferData(buffers->ribbon_instance_staging_buffer[fif]),
			zest_BufferData(buffers->emitter_staging_buffer[fif]));
	}
}

void zest_tfx_SetRibbonRenderDispatch(tfx_ribbon_render_dispatch_t *render_dispatch, tfx_stage stage, tfx_ribbon_buffers_t *buffers, tfx_library_render_resources_t *resources, tfx_global_library_buffers_t *global_buffers) {
	render_dispatch->render_resources = resources;
	render_dispatch->buffers = buffers;
	render_dispatch->stage = stage;
	render_dispatch->global_buffers = global_buffers;
}

void zest_tfx_RibbonComputeFunction(const zest_command_list command_list, void *user_data) {
	tfx_ribbon_render_dispatch_t *render_dispatch = (tfx_ribbon_render_dispatch_t*)(user_data);

	zest_context context = zest_GetContext(command_list);
	zest_uint fif = zest_CurrentFIF(context);

	zest_cmd_BindComputePipeline(command_list, zest_GetCompute(render_dispatch->render_resources->ribbon_rendering.ribbon_compute));

	tfx_ribbon_dispatch_t ribbon_dispatch = tfx_CreateRibbonDispatch();
	while (tfx_NextRibbonDispatch(render_dispatch->stage, &ribbon_dispatch)) {
		tfx_ribbon_bucket_globals_t *push = tfx_GetRibbonDispatchGlobals(&ribbon_dispatch);
		push->lerp = (float)zest_TimerLerp(&render_dispatch->render_resources->timer);
		push->time = (float)render_dispatch->render_resources->timer.seconds_passed;
		zest_uniform_buffer uniform_buffer = zest_GetUniformBuffer(render_dispatch->render_resources->uniform_buffer);
		push->uniform_index = zest_GetUniformBufferDescriptorIndex(uniform_buffer);
		push->graphs_index = render_dispatch->global_buffers->lookup_index[fif];
		push->emitters_index = zest_GetTransientBufferBindlessIndex(command_list, render_dispatch->emitter_buffer);
		push->ribbon_segments_index = zest_GetTransientBufferBindlessIndex(command_list, render_dispatch->segment_buffer);
		push->ribbons_index = zest_GetTransientBufferBindlessIndex(command_list, render_dispatch->ribbon_instance_buffer);
		push->vertexes_index = zest_GetTransientBufferBindlessIndex(command_list, render_dispatch->vertex_buffer);
		push->indexes_index = zest_GetTransientBufferBindlessIndex(command_list, render_dispatch->index_buffer);
		zest_cmd_SendPushConstants(command_list, push, sizeof(tfx_ribbon_bucket_globals_t));

		zest_cmd_DispatchCompute(command_list, (ribbon_dispatch.total_segments / 1024) + 1, 1, 1);
	}
}

void zest_tfx_RenderRibbons(const zest_command_list command_list, void *user_data) {
	tfx_ribbon_render_dispatch_t *render_dispatch = (tfx_ribbon_render_dispatch_t*)(user_data);

	//Bind the buffer that contains the sprite instances to draw. These are updated by the compute shader on the GPU
	zest_cmd_BindVertexBuffer(command_list, 0, 1, zest_GetResourceBuffer(render_dispatch->vertex_buffer));
	zest_cmd_BindIndexBuffer(command_list, zest_GetResourceBuffer(render_dispatch->index_buffer));

	//Draw all the sprites in the buffer that is built by the compute shader
	zest_pipeline pipeline = zest_GetPipeline(render_dispatch->render_resources->ribbon_rendering.pipeline, command_list);
	zest_cmd_BindPipeline(command_list, pipeline);

	zest_layer layer = zest_GetLayer(render_dispatch->render_resources->layer);
	zest_cmd_LayerViewport(command_list, layer);

	tfx_ribbon_dispatch_t ribbon_dispatch = tfx_CreateRibbonDispatch();
	while (tfx_NextRibbonDispatch(render_dispatch->stage, &ribbon_dispatch)) {
		tfx_ribbon_bucket_globals_t *push = tfx_GetRibbonDispatchGlobals(&ribbon_dispatch);
		push->sampler_index = render_dispatch->render_resources->sampler_index;
		push->color_ramp_texture_index = render_dispatch->render_resources->color_ramps_index;
		push->image_data_index = render_dispatch->render_resources->image_data_index;
		zest_cmd_SendPushConstants(command_list, push, sizeof(tfx_ribbon_bucket_globals_t));
		zest_cmd_DrawIndexed(command_list, ribbon_dispatch.index_count, 1, ribbon_dispatch.index_offset, 0, 0);
	}
}

void zest_tfx_AddRibbonsToFrameGraph(tfx_ribbon_render_dispatch_t *render_dispatch, zest_resource_node output_resource) {
	zest_buffer_resource_info_t segment_buffer_info = { 0, tfx_GetTotalSegmentBufferMaxSizeInBytes() };
	zest_buffer_resource_info_t instance_buffer_info = { 0, tfx_GetTotalRibbonBufferMaxSizeInBytes() };
	zest_buffer_resource_info_t vertex_buffer_info = { zest_resource_usage_hint_vertex_buffer, tfx_GetTotalSegmentVertexBufferMaxSizeInBytes(0) };
	zest_buffer_resource_info_t index_buffer_info = { zest_resource_usage_hint_index_buffer, tfx_GetTotalSegmentIndexBufferMaxSizeInBytes() };
	zest_buffer_resource_info_t emitter_buffer_info = { 0, tfx_GetTotalEmitterBufferMaxSizeInBytes() };
	render_dispatch->segment_buffer = zest_AddTransientBufferResource("Ribbon Segment Buffer", &segment_buffer_info);
	render_dispatch->ribbon_instance_buffer = zest_AddTransientBufferResource("Ribbon Instance Buffer", &instance_buffer_info);
	render_dispatch->emitter_buffer = zest_AddTransientBufferResource("Emitter Buffer", &emitter_buffer_info);
	render_dispatch->vertex_buffer = zest_AddTransientBufferResource("Ribbon Vertex Buffer", &vertex_buffer_info);
	render_dispatch->index_buffer = zest_AddTransientBufferResource("Ribbon Index Buffer", &index_buffer_info);

	zest_BeginTransferPass("Transfer Ribbon Data"); {
		zest_ConnectOutput(render_dispatch->segment_buffer);
		zest_ConnectOutput(render_dispatch->ribbon_instance_buffer);
		zest_ConnectOutput(render_dispatch->emitter_buffer);
		zest_SetPassTask(zest_tfx_UploadRibbonData, render_dispatch);
		zest_EndPass();
	}

	zest_BeginComputePass("Compute Ribbons"); {
		zest_ConnectInput(render_dispatch->segment_buffer);
		zest_ConnectInput(render_dispatch->ribbon_instance_buffer);
		zest_ConnectInput(render_dispatch->emitter_buffer);
		zest_ConnectOutput(render_dispatch->vertex_buffer);
		zest_ConnectOutput(render_dispatch->index_buffer);
		zest_SetPassTask(zest_tfx_RibbonComputeFunction, render_dispatch);
		zest_EndPass();
	}

	zest_BeginRenderPass("Draw Ribbons Pass"); {
		zest_ConnectInput(render_dispatch->vertex_buffer);
		zest_ConnectInput(render_dispatch->index_buffer);
		if (output_resource) {
			zest_ConnectOutput(output_resource);
		} else {
			zest_ConnectSwapChainOutput();
		}
		zest_SetPassTask(zest_tfx_RenderRibbons, render_dispatch);
		zest_EndPass();
	}
}
