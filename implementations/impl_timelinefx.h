#pragma once

#include <zest.h>
#include <timelinefxlib/timelinefx.h>

typedef struct tfx_push_constants_s {
	tfxU32 particle_texture_index;
	tfxU32 color_ramp_texture_index;
	tfxU32 image_data_index;
	tfxU32 prev_billboards_index;
	tfxU32 index_offset;
} tfx_push_constants_t;

typedef struct tfx_uniform_buffer_data_t {
    zest_matrix4 view;
    zest_matrix4 proj;
    zest_vec2 screen_size;
    float timer_lerp;
    float update_time;
} tfx_uniform_buffer_data_t;

typedef struct tfx_render_resources_s {
	zest_texture particle_texture;
	zest_texture color_ramps_texture;
	zest_layer layer;
	zest_uniform_buffer uniform_buffer;
	zest_buffer image_data;
	zest_pipeline_template pipeline;
	zest_shader fragment_shader;
	zest_shader vertex_shader;
	zest_shader_resources shader_resource;
	zest_camera_t camera;
	zest_timer timer;
} tfx_render_resources_t;

#ifdef __cplusplus
extern "C" {
#endif

void zest_tfx_UpdateUniformBuffer(tfx_render_resources_t *resources);
void zest_tfx_RenderParticles(tfx_effect_manager pm, tfx_render_resources_t *resources);
void zest_tfx_RenderParticlesByEffect(tfx_effect_manager pm, tfx_render_resources_t *resources);
void zest_tfx_AddPassTask(zest_pass_node pass, tfx_render_resources_t *resources);
void zest_tfx_DrawParticleLayer(VkCommandBuffer command_buffer, const zest_render_graph_context_t *context, void *user_data);
void zest_tfx_CreateTimelineFXShaderResources(tfx_render_resources_t *tfx_rendering);
void zest_tfx_UpdateTimelineFXImageData(tfx_render_resources_t *tfx_rendering, tfx_library library);
void zest_tfx_InitTimelineFXRenderResources(tfx_render_resources_t *render_resources, const char *library_path);
void zest_tfx_GetUV(void *ptr, tfx_gpu_image_data_t *image_data, int offset);
void zest_tfx_ShapeLoader(const char *filename, tfx_image_data_t *image_data, void *raw_image_data, int image_memory_size, void *custom_data);

#ifdef __cplusplus
}
#endif