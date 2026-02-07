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
    float timer_lerp;
    float update_time;
} tfx_uniform_buffer_data_t;

typedef struct tfx_render_resources_s {
	zest_image_collection_t particle_images;
	zest_image_collection_t color_ramps_collection;
	zest_image_handle particle_texture;
	zest_image_handle color_ramps_texture;
	zest_layer_handle layer;
	zest_sampler_handle sampler;
	zest_uint layer_index;
	zest_uint color_ramps_index;
	zest_uint particle_texture_index;
	zest_uint image_data_index;
	zest_uint sampler_index;
	zest_uniform_buffer_handle uniform_buffer;
	zest_buffer image_data;
	zest_pipeline_template pipeline;
	zest_shader_handle fragment_shader;
	zest_shader_handle vertex_shader;
	zest_camera_t camera;
	zest_timer_t timer;
	zest_execution_timeline timeline;
} tfx_render_resources_t;

#ifdef __cplusplus
extern "C" {
#endif

void zest_tfx_UpdateUniformBuffer(zest_context context, tfx_render_resources_t *resources);
void zest_tfx_RenderParticles(tfx_effect_manager pm, tfx_render_resources_t *resources);
void zest_tfx_RenderParticlesByEffect(tfx_effect_manager pm, tfx_render_resources_t *resources);
void zest_tfx_DrawParticleLayer(const zest_command_list command_list, void *user_data);
void zest_tfx_UpdateTimelineFXImageData(zest_context context, tfx_render_resources_t *tfx_rendering, tfx_library library);
void zest_tfx_InitTimelineFXRenderResources(zest_device device, zest_context context, tfx_render_resources_t *render_resources, const char *library_path);
void zest_tfx_GetUV(void *ptr, tfx_gpu_image_data_t *image_data, int offset);
void zest_tfx_ShapeLoader(const char *filename, tfx_image_data_t *image_data, void *raw_image_data, int image_memory_size, void *custom_data);

#ifdef __cplusplus
}
#endif