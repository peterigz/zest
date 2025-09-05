#pragma once

#include <zest.h>
#include "implementations/impl_imgui.h"
#include "implementations/impl_glfw.h"
#include "implementations/impl_imgui_glfw.h"
#include "imgui/imgui.h"
#include <imgui/misc/freetype/imgui_freetype.h>
#include <imgui/backends/imgui_impl_glfw.h>

struct RenderCacheInfo {
	bool draw_imgui;
	VkImageLayout brd_layout;
	VkImageLayout irradiance_layout;
	VkImageLayout prefiltered_layout;
};

struct UniformLights {
	zest_vec4 lights[4];
	float exposure;
	float gamma;
	zest_uint texture_index;
};

typedef struct uniform_buffer_data_t {
    zest_matrix4 view;
    zest_matrix4 proj;
    zest_vec2 screen_size;
    float timer_lerp;
    float update_time;
} uniform_buffer_data_t;

struct billboard_push_constant_t {
	zest_uint texture_index;
};

struct irr_push_constant_t {
	zest_uint source_env_index;
	zest_uint irr_index;
	float delta_phi;
	float delta_theta;
};

struct prefiltered_push_constant_t {
	zest_uint source_env_index;
	zest_uint prefiltered_index;
	float roughness;
	zest_uint num_samples;
};

struct ImGuiApp {
	zest_index imgui_draw_routine_index;
	zest_timer_handle timer;
	zest_camera_t camera;

	zest_layer_handle cube_layer;
	zest_layer_handle skybox_layer;
	zest_layer_handle billboard_layer;

	zest_pipeline_template pbr_pipeline;
	zest_pipeline_template skybox_pipeline;
	zest_pipeline_template billboard_pipeline;

	zest_uniform_buffer_handle view_buffer;
	zest_uniform_buffer_handle lights_buffer;

	RenderCacheInfo cache_info;

	zest_push_constants_t material_push;
	billboard_push_constant_t billboard_push;
	irr_push_constant_t irr_push_constant;
	prefiltered_push_constant_t prefiltered_push_constant;

	zest_shader_resources_handle sprite_resources;
	zest_shader_resources_handle pbr_shader_resources;
	zest_shader_resources_handle skybox_shader_resources;

	zest_shader_handle brd_shader;
	zest_shader_handle irr_shader;
	zest_shader_handle prefiltered_shader;

	zest_image_handle imgui_font_texture;
	zest_image_handle skybox_texture;
	zest_image_handle brd_texture;
	zest_image_handle irr_texture;
	zest_image_handle prefiltered_texture;

	zest_image_view_t *brd_view;

	zest_compute_handle brd_compute;
	zest_compute_handle irr_compute;
	zest_compute_handle prefiltered_compute;

	zest_uint skybox_bindless_index;
	zest_uint brd_bindless_index;
	zest_uint irr_bindless_index;
	zest_uint prefiltered_bindless_index;
	
	zest_uint *prefiltered_mip_indexes;

	zest_vec3 old_camera_position;
	zest_vec3 new_camera_position;

	zest_atlas_region light;
	float ellapsed_time;
	bool sync_refresh;
	bool request_graph_print;
	bool reset;
};

void InitImGuiApp(ImGuiApp *app);
void UpdateLights(ImGuiApp *app, float timer);
void SetupBillboards(ImGuiApp *app);
void SetupBRDFLUT(ImGuiApp *app);
void SetupIrradianceCube(ImGuiApp *app);
void SetupPrefilteredCube(ImGuiApp *app);
