
#pragma once

#include <GLFW/glfw3.h>
#include <zest.h>
#include "implementations/impl_imgui.h"
#include "imgui/imgui.h"
#include <imgui/misc/freetype/imgui_freetype.h>
#include <imgui/backends/imgui_impl_glfw.h>

#define SHADOW_MAP_CASCADE_COUNT 4
#define SHADOW_MAP_TEXTURE_SIZE 4096

struct RenderCacheInfo {
	bool draw_imgui;
	bool draw_depth_image;
};

typedef struct zest_instance_t {
	zest_vec3 pos;                                 //3d position
	zest_color_t color;                              //packed color
	zest_vec3 rotation;
	zest_uint texture_index;                 
	zest_vec3 scale;
} zest_instance_t;

struct uniform_cascade_data_t {
	zest_matrix4 cascade[SHADOW_MAP_CASCADE_COUNT];
};

struct uniform_buffer_data_t {
	zest_matrix4 proj;
	zest_matrix4 view;
	zest_matrix4 model;
	zest_vec3 light_direction;
};	

struct uniform_fragment_data_t {
	float cascadeSplits[4];
	zest_matrix4 inverseViewMat;
	zest_vec3 light_direction;
	float padding;
	int color_cascades;
};

struct scene_push_constants_t {
	zest_uint sampler_index;
	zest_uint shadow_sampler_index;
	zest_uint shadow_index;
	zest_uint vert_index;
	zest_uint frag_index;
	zest_uint cascade_index;
	zest_uint texture_index;
	int display_cascade_index;
	bool enable_pcf;
};

struct cascade_t {
	float splitDepth;
	zest_matrix4 viewProjMatrix;
};

struct mouse_t {
	double mouse_x, mouse_y;
	double mouse_delta_x, mouse_delta_y;
};

struct CascadingShadowsExample {
	zest_context context;
	zest_device device;
	zest_imgui_t imgui;
	zest_uint imgui_draw_routine_index;
	zest_timer_t timer;
	zest_camera_t camera;

	zest_pipeline_template debug_pipeline;
	zest_pipeline_template mesh_pipeline;
	zest_pipeline_template shadow_pipeline;
	scene_push_constants_t scene_push;

	float cascade_split_lambda;
	zest_uniform_buffer_handle vert_buffer;
	zest_uniform_buffer_handle cascade_buffer;
	zest_uniform_buffer_handle fragment_buffer;

	zest_layer_handle mesh_layer;

	zest_uint tree_index;
	zest_uint terrain_index;

	RenderCacheInfo cache_info;

	zest_sampler_handle sampler_2d;
	zest_sampler_handle shadow_sampler_2d;

	zest_vec3 old_camera_position;
	zest_vec3 new_camera_position;
	zest_vec3 light_position;
	float light_fov;
	float z_near;
	float z_far;
	cascade_t cascades[SHADOW_MAP_CASCADE_COUNT];
	bool color_cascades;
	bool debug_draw;

	mouse_t mouse;
	zest_uint fps;

	float frame_timer;
	bool sync_refresh;
};

void InitCascadingShadowsExample(CascadingShadowsExample *app);
void UpdateUniform3d(CascadingShadowsExample *app);
void UpdateLights(CascadingShadowsExample *app, float timer);
void UpdateImGui(CascadingShadowsExample *app);
void UploadMeshData(const zest_command_list context, void *user_data);
void SetupBRDFLUT(CascadingShadowsExample *app);
void SetupIrradianceCube(CascadingShadowsExample *app);
void SetupPrefilteredCube(CascadingShadowsExample *app);
void MainLoop(CascadingShadowsExample *app);