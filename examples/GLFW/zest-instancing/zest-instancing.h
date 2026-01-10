
#pragma once

#include <GLFW/glfw3.h>
#include <zest.h>
#include "implementations/impl_imgui.h"
#include "imgui/imgui.h"
#include <imgui/misc/freetype/imgui_freetype.h>
#include <imgui/backends/imgui_impl_glfw.h>

struct RenderCacheInfo {
	bool draw_imgui;
};

struct uniform_buffer_data_t {
    zest_matrix4 proj;
    zest_matrix4 view;
	zest_vec4 light_pos;
	float locSpeed;
	float globSpeed;
};

struct instance_push_t {
	zest_uint sampler_index;
	zest_uint planet_index;
	zest_uint rock_index;
	zest_uint skybox_index;
	zest_uint ubo_index;
};

struct mesh_t {
	zest_buffer index_buffer;
	zest_buffer vertex_buffer;
	zest_uint index_count;
	zest_uint vertex_count;
};

struct InstancingExample {
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

	zest_image_handle lavaplanet_texture;
	zest_image_handle rock_textures;
	zest_image_handle skybox_texture;
	zest_uint lavaplanet_texture_index;
	zest_uint rock_textures_index;

	zest_uniform_buffer_handle view_buffer;
	zest_buffer rock_instances_buffer;

	RenderCacheInfo cache_info;

	zest_sampler_handle sampler_2d;

	zest_vec3 old_camera_position;
	zest_vec3 new_camera_position;

	double mouse_x, mouse_y;
	double mouse_delta_x, mouse_delta_y;
	zest_uint fps;

	float ellapsed_time;
	float frame_timer;
	float loc_speed;
	float glob_speed;
	bool sync_refresh;
};

void InitInstancingExample(InstancingExample *app);
void PrepareInstanceData(InstancingExample *app);
void RenderGeometry(zest_command_list command_list, void *user_data);
void UpdateUniform3d(InstancingExample *app);
void UpdateLights(InstancingExample *app, float timer);
void UpdateMouse(InstancingExample *app);
void UpdateCameraPosition(InstancingExample *app);
void UpdateImGui(InstancingExample *app);
void UploadMeshData(const zest_command_list context, void *user_data);
void MainLoop(InstancingExample *app);