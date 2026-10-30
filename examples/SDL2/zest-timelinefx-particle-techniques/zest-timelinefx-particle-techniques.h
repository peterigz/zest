#pragma once

#include <SDL.h>
#include <zest.h>
#include "implementations/impl_imgui.h"
#include "imgui/imgui.h"
#include <imgui/misc/freetype/imgui_freetype.h>
#include "impl_timelinefx.h"
#include "examples/Common/timelinefx_shaders.h"
#include <imgui/backends/imgui_impl_glfw.h>

struct AnimationComputeConstants {
	tfxU32 total_sprites;
	tfxU32 total_animation_instances;
	tfxU32 flags;
};

//Mirrors tfx_baked_billboard_push in timelinefx.slang
struct tfx_sprite_sheet_push_t {
	tfxU32 color_ramp_texture_index;
	tfxU32 image_data_index;
	tfxU32 emitter_properties_index;
	tfxU32 sampler_index;
	tfxU32 prev_billboards_index;
	tfxU32 index_offset;
	tfxU32 uniform_index;
	float ndc_offset_x;
	float ndc_offset_y;
};


struct ComputePushConstants {
	zest_uint instances_size;
	zest_uint has_animated_shapes;
	zest_uint total_sprites_to_draw;
	zest_uint sprite_data_offset;
};

struct mouse_t {
	double mouse_x, mouse_y;
	double mouse_delta_x, mouse_delta_y;
};

struct RenderCacheInfo {
	bool draw_imgui;
	bool draw_timeline_fx;
	bool use_stage;
};

#define EFFECT_COUNT 4

enum EffectMode {
	EffectMode_baked,
	EffectMode_stage_cpu,
	EffectMode_stage_gpu,
	EffectMode_count
};

struct tfxParticleTechniquesExample {
	zest_context context;
	zest_device device;
	zest_compute_handle compute;
	zest_compute_handle bounding_box_compute;
	bool left_mouse_clicked;
	bool right_mouse_clicked;

	//For GPU drivers that don't have the capability to write directly to the GPU buffer you will need
	//staging buffers to write to first before uploading those to the GPU device buffers. We only need to upload
	//the instances of each animation and the offsets array which are very small so not much overhead at all.
	zest_buffer animation_instances_staging_buffer[ZEST_MAX_FIF];
	zest_buffer offsets_staging_buffer[ZEST_MAX_FIF];
	//Compiles the TimelineFX shaders from timelinefx.slang
	zest_slang_session shader_session;
	//The compute shader that interpolates all the sprite data
	zest_shader_handle playback_shader;

	tfx_library_render_resources_t tfx_rendering;

	zest_imgui_t imgui;
	tfx_gpu_shapes gpu_image_data;
	zest_timer_t timer;

	zest_layer_handle mesh_layer;			//To draw the floor plain
	zest_pipeline_template mesh_pipeline;
	zest_atlas_region_t floor_image;
	zest_sampler_handle sampler;
	zest_uint sampler_index;
	tfx_random_t random;

	mouse_t mouse;
	zest_vec3 old_camera_position;
	zest_vec3 new_camera_position;

	zest_compute_handle compute_pipeline_3d;
	zest_compute_handle bb_compute_pipeline_3d;

	tfx_animation_manager animation_manager_3d;
	AnimationComputeConstants animation_manager_push_constants;
	zest_millisecs record_time;
	bool effect_is_3d;

	zest_microsecs trigger_effect;

	//The same effects simulated live by a stage, where the Run on GPU emitters are simulated by a compute shader.
	//The stage has its own render resources so its color ramps, shapes and pipeline don't clash with the baked data.
	int effect_mode;
	int requested_effect_mode;
	tfx_library library;
	tfx_stage stage;
	tfx_effect_template effect_templates[EFFECT_COUNT];
	tfxEffectID effect_ids[EFFECT_COUNT];
	tfx_library_render_resources_t stage_rendering;
	tfx_global_library_buffers_t global_buffers;
	tfx_gpu_particle_renderer_t gpu_particles;
};

void SpriteComputeFunction(zest_command_list command_list, void *user_data);
void RecordComputeSprites(zest_command_list command_list, void *user_data);
void InitExample(tfxParticleTechniquesExample *example);
void InitStage(tfxParticleTechniquesExample *example, zest_shader_handle particles_frag_shader, zest_shader_handle ribbon_vert_shader, zest_shader_handle ribbon_frag_shader, zest_shader_handle ribbon_comp_shader);
void UpdateStage(tfxParticleTechniquesExample *example);
void PrepareComputeForEffectPlayback(tfxParticleTechniquesExample *example);
void UpdateUniform3d(tfxParticleTechniquesExample *game);
void Update(zest_microsecs elapsed, void *data);
void BuildUI(tfxParticleTechniquesExample *example, zest_uint fps);
bool CullAnimationInstancesCallback(tfx_animation_manager animation_manager, tfx_float32x3_t position, float radius, void *user_data);
zest_vec3 ScreenRay(tfxParticleTechniquesExample *example, float x, float y, float depth_offset, zest_vec3 &camera_position);

