
#pragma once

#include <SDL.h>
#include <zest.h>
#include "implementations/impl_imgui.h"
#include "imgui/imgui.h"
#include <imgui/misc/freetype/imgui_freetype.h>
#include <imgui/backends/imgui_impl_sdl2.h>

struct RenderCacheInfo {
	bool draw_imgui;
};

struct uniform_buffer_data_t {
    zest_matrix4 proj;
    zest_matrix4 view;
	zest_vec4 planes[6];
	zest_vec4 light_pos;
	float locSpeed;
	float globSpeed;
	zest_uint visible;
};

struct instance_push_t {
	zest_uint sampler_index;
	zest_uint planet_index;
	zest_uint rock_index;
	zest_uint skybox_index;
	zest_uint ubo_index;
};

struct cull_push_t {
	zest_uint all_instances_index;
	zest_uint visible_instances_index;
	zest_uint indirect_cmd_index;
	zest_uint ubo_index;
	zest_uint total_count;
	zest_uint index_count;
	zest_uint first_index;
	int vertex_offset;
	zest_uint cull_enabled;
	float contract_frustum;
};

struct mesh_instance_t {
	zest_vec3 pos;                                 //3d position
	zest_color_t color;                              //packed color
	zest_vec3 rotation;
	zest_vec3 scale;
	zest_uint texture_layer_index;
};

struct mesh_t {
	zest_buffer index_buffer;
	zest_buffer vertex_buffer;
	zest_uint index_count;
	zest_uint vertex_count;
};

struct mouse_t {
	double mouse_x, mouse_y;
	double mouse_delta_x, mouse_delta_y;
};

struct IndirectDrawExample {
	zest_context context;
	zest_device device;
	zest_imgui_t imgui;
	zest_uint imgui_draw_routine_index;
	zest_timer_t timer;
	zest_camera_t camera;

	zest_pipeline_template rock_pipeline;
	zest_pipeline_template planet_pipeline;
	zest_pipeline_template skybox_pipeline;

	mesh_t planet_mesh;
	zest_layer_handle rock_layer;
	zest_layer_handle skybox_layer;
	zest_uint rock_mesh_index;
	zest_uint skybox_mesh_index;

	instance_push_t push;
	cull_push_t cull_push;

	zest_image_handle lavaplanet_texture;
	zest_image_handle rock_textures;
	zest_image_handle skybox_texture;
	zest_uint lavaplanet_texture_index;
	zest_uint rock_textures_index;

	zest_uniform_buffer_handle view_buffer;

	// GPU culling buffers
	zest_buffer all_instances_ssbo;
	zest_buffer visible_instances;
	zest_buffer indirect_draw_buffer;
	zest_uint all_instances_ssbo_index;
	zest_uint visible_instances_index;
	zest_uint indirect_cmd_index;

	// Compute
	zest_compute_handle cull_compute;

	RenderCacheInfo cache_info;

	zest_sampler_handle sampler_2d;

	zest_vec3 old_camera_position;
	zest_vec3 new_camera_position;

	mouse_t mouse;
	zest_uint fps;

	float ellapsed_time;
	float frame_timer;
	float loc_speed;
	float glob_speed;
	float contract_frustum;
	bool sync_refresh;
	bool culling_enabled;
};

void InitIndirectDrawExample(IndirectDrawExample *app);
void PrepareInstanceData(IndirectDrawExample *app);
void CullCompute(zest_command_list command_list, void *user_data);
void RenderGeometry(zest_command_list command_list, void *user_data);
void UpdateUniform3d(IndirectDrawExample *app);
void UpdateLights(IndirectDrawExample *app, float timer);
void UpdateMouse(IndirectDrawExample *app);
void UpdateImGui(IndirectDrawExample *app);
void UploadMeshData(const zest_command_list context, void *user_data);
void MainLoop(IndirectDrawExample *app);
