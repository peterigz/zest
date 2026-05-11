#pragma once

#include <zest.h>
#include <timelinefx.h>

typedef struct tfx_push_constants_s {
	tfxU32 particle_texture_index;
	tfxU32 color_ramp_texture_index;
	tfxU32 image_data_index;
	tfxU32 particle_properties_index;
	tfxU32 sampler_index;
	tfxU32 prev_billboards_index;
	tfxU32 index_offset;
	tfxU32 uniform_index;
} tfx_push_constants_t;

typedef struct tfx_uniform_buffer_data_t {
    zest_matrix4 view;
    zest_matrix4 proj;
    zest_vec2 screen_size;
	zest_uint millisecs;
    float timer_lerp;
    float update_time;
} tfx_uniform_buffer_data_t;

typedef struct tfx_ribbon_buffers_t {
	zest_buffer ribbon_staging_buffer[ZEST_MAX_FIF];
	zest_buffer ribbon_instance_staging_buffer[ZEST_MAX_FIF];
	zest_buffer emitter_staging_buffer[ZEST_MAX_FIF];
	tfx_ribbon_buffer_info_t ribbon_buffer_info;
	tfx_ribbon_bucket_globals_t globals;
} tfx_ribbon_buffers_t;

typedef struct tfx_ribbon_rendering_t {
	zest_pipeline_template pipeline;
	zest_shader_handle vert_shader;
	zest_shader_handle frag_shader;
	zest_shader_handle comp_shader;
	zest_compute_handle ribbon_compute;
} tfx_ribbon_rendering_t;

typedef struct tfx_particle_rendering_t {
	zest_shader_handle frag_shader;
	zest_shader_handle vert_shader;
	zest_pipeline_template pipeline;
} tfx_particle_rendering_t;

typedef struct tfx_global_library_buffers_t {
    zest_buffer lookup_tables_staging;
	zest_buffer lookup_buffer[ZEST_MAX_FIF];
	zest_uint lookup_index[ZEST_MAX_FIF];
    bool lookup_table_dirty[ZEST_MAX_FIF];
} tfx_global_library_buffers_t;

typedef struct tfx_library_render_resources_s {
	zest_layer_handle layer;
	tfx_particle_rendering_t particles;
	tfx_ribbon_rendering_t ribbon_rendering;
	zest_image_collection_t particle_images;
	zest_image_collection_t color_ramps_collection;
	zest_image_handle particle_texture;
	zest_image_handle color_ramps_texture;
	zest_sampler_handle sampler;
	zest_instruction_id layer_ids[tfxLAYERS];
	zest_uint color_ramps_index;
	zest_uint particle_texture_index;
	zest_uint image_data_index;
	zest_uint sampler_index;
	zest_uint particle_properties_index;
	zest_uniform_buffer_handle uniform_buffer;
	zest_buffer particle_properties;
	zest_buffer image_data;
	zest_camera_t camera;
	zest_timer_t timer;
	zest_execution_timeline timeline;
	zest_vec4 planes[6];
	//Prerecorded sprite data buffers (used by animation manager workflow)
	zest_buffer sprite_data_buffer;
	zest_buffer emitter_properties_buffer;
	zest_uint sprite_data_index;
	zest_uint emitter_properties_index;
} tfx_library_render_resources_t;

typedef struct tfx_ribbon_render_dispatch_t {
	//This struct will be used in cached frame graphs so this data must not change unless
	//you rebuild/cache the frame graph
	tfx_effect_manager effect_manager;
	tfx_ribbon_buffers_t *buffers;
	tfx_library_render_resources_t *render_resources;
	tfx_global_library_buffers_t *global_buffers;
	zest_resource_node segment_buffer;
	zest_resource_node ribbon_instance_buffer;
	zest_resource_node emitter_buffer;
	zest_resource_node vertex_buffer;
	zest_resource_node index_buffer;
} tfx_ribbon_render_dispatch_t;

#ifdef __cplusplus
extern "C" {
#endif

//-- High-level API (recommended) --

//Initialise the render resources needed for TimelineFX rendering including pipelines, shaders, layer and uniform buffer.
void zest_tfx_InitTimelineFXRenderResources(zest_context context, tfx_library_render_resources_t *render_resources, zest_shader_handle vert_shader, zest_shader_handle frag_shader, zest_shader_handle ribbon_vert, zest_shader_handle ribbon_frag, zest_shader_handle ribbon_comp);

//Load a TimelineFX library and create the particle image atlas. Returns the library handle so you can create effect templates.
//Call zest_tfx_FinaliseLibrary after creating all effect templates.
tfx_library zest_tfx_LoadLibrary(zest_context context, tfx_library_render_resources_t *resources, const char *library_path, int atlas_width, int atlas_height);

//Finalise the library after creating all effect templates. This uploads color ramps and GPU image data.
void zest_tfx_FinaliseLibrary(zest_context context, tfx_library_render_resources_t *resources, tfx_library library);

//Load prerecorded sprite data and create the particle image atlas.
//shape_count is the maximum number of particle shapes to allocate space for in the image collection.
//Returns the error flags from tfx_LoadSpriteData.
tfxErrorFlags zest_tfx_LoadSpriteData(zest_context context, tfx_library_render_resources_t *resources, const char *path, tfx_animation_manager animation_manager, int shape_count, int atlas_width, int atlas_height);

//Finalise prerecorded sprite data - uploads color ramps from the animation manager and GPU image data.
void zest_tfx_FinaliseSpriteData(zest_context context, tfx_library_render_resources_t *resources, tfx_animation_manager animation_manager, tfx_gpu_shapes gpu_image_data);

//Create a single shared set of ribbon buffers sized across all registered effect managers.
//Call this after all effect managers have been created.
void zest_tfx_CreateRibbonBuffers(zest_context context, tfx_ribbon_buffers_t *buffers);
void zest_tfx_CreateGlobalBuffers(zest_context context, tfx_global_library_buffers_t *buffers);

//Copy ribbon data from all effect managers to staging buffers for GPU upload.
//Handles the camera setup and HasRibbonsToDraw check internally.
void zest_tfx_UpdateRibbonStagingBuffers(zest_context context, tfx_ribbon_buffers_t *buffers, tfx_effect_manager pm);

zest_image_collection_t zest_tfx_CreateImageCollection(zest_uint shape_count);
void zest_tfx_SetRibbonRenderDispatch(tfx_ribbon_render_dispatch_t *render_dispatch, tfx_effect_manager effect_manager, tfx_ribbon_buffers_t *buffers, tfx_library_render_resources_t *resources, tfx_global_library_buffers_t *global_buffers);
//-- Uniform buffer and rendering --

void zest_tfx_UpdateUniformBuffer(zest_context context, tfx_library_render_resources_t *resources);
void zest_tfx_RenderParticles(tfx_effect_manager pm, tfx_library_render_resources_t *resources);
void zest_tfx_RenderParticlesByEffect(tfx_effect_manager pm, tfx_library_render_resources_t *resources);
void zest_tfx_FreeLibraryImages(tfx_library_render_resources_t *resources);

//-- Frame graph pass callbacks --

void zest_tfx_DrawParticleLayer(const zest_command_list command_list, void *user_data);
void zest_tfx_UploadRibbonData(const zest_command_list command_list, void *user_data);
void zest_tfx_UploadGraphData(const zest_command_list command_list, void *user_data);
void zest_tfx_RibbonComputeFunction(const zest_command_list command_list, void *user_data);
void zest_tfx_RenderRibbons(const zest_command_list command_list, void *user_data);
void zest_tfx_AddRibbonsToFrameGraph(tfx_ribbon_render_dispatch_t *render_dispatch, zest_resource_node output_resource);

//-- Low-level functions (for custom renderer integration) --

void zest_tfx_UpdateTimelineFXImageData(zest_context context, tfx_library_render_resources_t *tfx_rendering, tfx_gpu_shapes shapes);
void zest_tfx_UpdateTimelineFXParticleProperties(zest_context context, tfx_library_render_resources_t *tfx_rendering, tfx_library library);
void zest_tfx_InitialiseGlobalData(zest_context context, tfx_global_library_buffers_t *buffers);
void zest_tfx_GetUV(void *ptr, tfx_gpu_image_data_t *image_data, int offset);
void zest_tfx_ShapeLoader(const char *filename, tfx_image_data_t *image_data, void *raw_image_data, int image_memory_size, void *custom_data);

#ifdef __cplusplus
}
#endif