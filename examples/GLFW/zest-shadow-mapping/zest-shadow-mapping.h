
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
	zest_matrix4 projection;
	zest_matrix4 view;
	zest_matrix4 model;
	zest_matrix4 light_space;
	zest_vec4 light_pos;
	float z_near;
	float z_far;
};

struct uniform_data_offscreen_t {
	zest_matrix4 depth_mvp;
};

struct scene_push_constants_t {
	zest_uint sampler_index;
	zest_uint shadow_index;
	zest_uint view_index;
	zest_uint offscreen_index;
	bool enable_pcf;
};

struct ShadowMappingExample {
	zest_context context;
	zest_device device;
	zest_imgui_t imgui;
	zest_uint imgui_draw_routine_index;
	zest_timer_t timer;
	zest_camera_t camera;

	zest_pipeline_template mesh_pipeline;
	zest_pipeline_template shadow_pipeline;
	scene_push_constants_t scene_push;

	zest_uniform_buffer_handle view_buffer;
	zest_uniform_buffer_handle offscreen_uniform_buffer;

	zest_layer_handle mesh_layer;

	zest_uint vulkanscene_index;

	RenderCacheInfo cache_info;

	zest_sampler_handle sampler_2d;

	zest_vec3 old_camera_position;
	zest_vec3 new_camera_position;
	zest_vec3 light_position;
	float light_fov;
	float z_near = 1.0f;
	float z_far = 96.0f;

	double mouse_x, mouse_y;
	double mouse_delta_x, mouse_delta_y;
	zest_uint fps;

	float frame_timer;
	bool sync_refresh;
};

void InitShadowMappingExample(ShadowMappingExample *app);
void UpdateUniform3d(ShadowMappingExample *app);
void UpdateLights(ShadowMappingExample *app, float timer);
void UpdateMouse(ShadowMappingExample *app);
void UpdateCameraPosition(ShadowMappingExample *app);
void UpdateImGui(ShadowMappingExample *app);
void UploadMeshData(const zest_command_list context, void *user_data);
void SetupBRDFLUT(ShadowMappingExample *app);
void SetupIrradianceCube(ShadowMappingExample *app);
void SetupPrefilteredCube(ShadowMappingExample *app);
void MainLoop(ShadowMappingExample *app);