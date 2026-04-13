#pragma once

#include <zest.h>
#include <timelinefx.h>

typedef struct tfx_push_constants_s {
	tfxU32 particle_texture_index;
	tfxU32 color_ramp_texture_index;
	tfxU32 image_data_index;
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

typedef struct tfx_ribbon_rendering_t {
	zest_pipeline_template pipeline;
	zest_shader_handle vert_shader;
	zest_shader_handle frag_shader;
	zest_shader_handle comp_shader;
	zest_compute_handle ribbon_compute;
	zest_buffer ribbon_lookup_buffer[ZEST_MAX_FIF];
	zest_uint ribbon_lookup_index[ZEST_MAX_FIF];
	zest_buffer ribbon_staging_buffer[ZEST_MAX_FIF];
	zest_buffer ribbon_instance_staging_buffer[ZEST_MAX_FIF];
	zest_buffer emitter_staging_buffer[ZEST_MAX_FIF];
    zest_buffer lookup_tables_staging;
	tfx_ribbon_buffer_info_t ribbon_buffer_info;
	tfx_ribbon_bucket_globals_t globals;
    bool lookup_table_dirty[ZEST_MAX_FIF];
	bool draw_ribbons;
} tfx_ribbon_rendering_t;

typedef struct tfx_particle_rendering_t {
	zest_shader_handle frag_shader;
	zest_shader_handle vert_shader;
	zest_pipeline_template pipeline;
} tfx_particle_rendering_t;

typedef struct tfx_library_render_resources_s {
	zest_layer_handle layer;
	tfx_particle_rendering_t particles;
	tfx_ribbon_rendering_t ribbons;
	zest_image_collection_t particle_images;
	zest_image_collection_t color_ramps_collection;
	zest_image_handle particle_texture;
	zest_image_handle color_ramps_texture;
	zest_sampler_handle sampler;
	zest_uint color_ramps_index;
	zest_uint particle_texture_index;
	zest_uint image_data_index;
	zest_uint sampler_index;
	zest_uniform_buffer_handle uniform_buffer;
	zest_buffer image_data;
	zest_camera_t camera;
	zest_timer_t timer;
	zest_execution_timeline timeline;
	zest_vec4 planes[6];
} tfx_library_render_resources_t;

#ifdef __cplusplus
extern "C" {
#endif

void zest_tfx_UpdateUniformBuffer(zest_context context, tfx_library_render_resources_t *resources);
void zest_tfx_RenderParticles(tfx_effect_manager pm, tfx_library_render_resources_t *resources);
void zest_tfx_RenderParticlesByEffect(tfx_effect_manager pm, tfx_library_render_resources_t *resources);
void zest_tfx_DrawParticleLayer(const zest_command_list command_list, void *user_data);
void zest_tfx_UpdateTimelineFXImageData(zest_context context, tfx_library_render_resources_t *tfx_rendering, tfx_gpu_shapes shapes);
zest_image_collection_t zest_tfx_CreateImageCollection(zest_uint shape_count);
void zest_tfx_InitTimelineFXRenderResources(zest_context context, tfx_library_render_resources_t *render_resources, const char *vert_shader, const char *frag_shader);
void zest_tfx_CreateBuffersForEffects(zest_context context, tfx_effect_manager pm, tfx_library_render_resources_t *render_resources);
void zest_tfx_FreeLibraryImages(tfx_library_render_resources_t *resources);
void zest_tfx_GetUV(void *ptr, tfx_gpu_image_data_t *image_data, int offset);
void zest_tfx_ShapeLoader(const char *filename, tfx_image_data_t *image_data, void *raw_image_data, int image_memory_size, void *custom_data);
void zest_tfx_UploadRibbonData(const zest_command_list command_list, void *user_data);
void zest_tfx_UploadRibbonLookupData(zest_context context, tfx_library_render_resources_t *resources);
void zest_tfx_RibbonComputeFunction(const zest_command_list command_list, void *user_data);
void zest_tfx_RenderRibbons(const zest_command_list command_list, void *user_data);
void zest_tfx_AddRibbonsToFrameGraph(tfx_effect_manager pm, tfx_library_render_resources_t *resources, zest_resource_node output_resource);
void zest_tfx_CreateBuffersForEffects(zest_context context, tfx_effect_manager pm, tfx_library_render_resources_t *render_resources);

#ifdef __cplusplus
}
#endif