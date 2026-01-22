
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

typedef struct uniform_buffer_data_t {
    zest_matrix4 view;
    zest_matrix4 proj;
    zest_vec2 screen_size;
    float timer_lerp;
    float update_time;
} uniform_buffer_data_t;

struct ShadowMappingExample {
	zest_context context;
	zest_device device;
	zest_imgui_t imgui;
	zest_uint imgui_draw_routine_index;
	zest_timer_t timer;
	zest_camera_t camera;

	zest_pipeline_template mesh_pipeline;

	zest_uniform_buffer_handle view_buffer;

	RenderCacheInfo cache_info;

	zest_sampler_handle sampler_2d;

	zest_uint sampler_2d_index;

	zest_vec3 old_camera_position;
	zest_vec3 new_camera_position;

	double mouse_x, mouse_y;
	double mouse_delta_x, mouse_delta_y;
	zest_uint fps;

	float ellapsed_time;
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