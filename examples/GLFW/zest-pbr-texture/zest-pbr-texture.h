#pragma once

#include <GLFW/glfw3.h>
#include <zest.h>
#include "implementations/impl_imgui.h"
#include "imgui/imgui.h"
#include <imgui/misc/freetype/imgui_freetype.h>
#include <imgui/backends/imgui_impl_glfw.h>

struct RenderCacheInfo {
	bool draw_imgui;
	int brd_layout;
	int irradiance_layout;
	int prefiltered_layout;
};

struct UniformLights {
	zest_vec4 lights[4];
	float exposure;
	float gamma;
	zest_uint texture_index;
	zest_uint sampler_index;
};

typedef struct uniform_buffer_data_t {
    zest_matrix4 view;
    zest_matrix4 proj;
    zest_vec2 screen_size;
    float timer_lerp;
    float update_time;
} uniform_buffer_data_t;

struct irr_push_constant_t {
	zest_uint source_env_index;
	zest_uint irr_index;
	zest_uint sampler_index;
	float delta_phi;
	float delta_theta;
};

struct prefiltered_push_constant_t {
	zest_uint source_env_index;
	zest_uint prefiltered_index;
	zest_uint sampler_index;
	float roughness;
	zest_uint num_samples;
};

struct pbr_consts_t {
	zest_vec4 camera;
	zest_uint irradiance_index;
	zest_uint brd_lookup_index;
	zest_uint pre_filtered_index;
	zest_uint sampler_index;
	zest_uint view_buffer_index;
	zest_uint lights_buffer_index;
	zest_uint albedo_index;
	zest_uint normal_index;
	zest_uint ao_index;
	zest_uint metallic_index;
	zest_uint roughness_index;
};

struct PBRTextureExample {
	zest_context context;
	zest_device device;
	zest_imgui_t imgui;
	zest_uint imgui_draw_routine_index;
	zest_timer_t timer;
	zest_camera_t camera;

	zest_layer_handle mesh_layer;
	zest_layer_handle skybox_layer;

	zest_uint gun_index;
	zest_uint skybox_index;

	zest_pipeline_template pbr_pipeline;
	zest_pipeline_template skybox_pipeline;

	zest_uniform_buffer_handle view_buffer;
	zest_uniform_buffer_handle lights_buffer;

	RenderCacheInfo cache_info;

	pbr_consts_t material_push;
	irr_push_constant_t irr_push_constant;
	prefiltered_push_constant_t prefiltered_push_constant;

	zest_image_handle imgui_font_texture;
	zest_image_handle skybox_texture;
	zest_image_handle brd_texture;
	zest_image_handle irr_texture;
	zest_image_handle prefiltered_texture;

	zest_image_handle albedo_texture;
	zest_image_handle normal_texture;
	zest_image_handle ao_texture;
	zest_image_handle metallic_texture;
	zest_image_handle roughness_texture;

	zest_image_view_array_handle prefiltered_view_array;

	zest_sampler_handle sampler_2d;
	zest_uint sampler_2d_index;

	zest_image_view_t *brd_view;

	zest_compute_handle brd_compute;
	zest_compute_handle irr_compute;
	zest_compute_handle prefiltered_compute;

	zest_uint skybox_bindless_texture_index;
	zest_uint brd_bindless_texture_index;
	zest_uint irr_bindless_texture_index;
	zest_uint prefiltered_bindless_texture_index;

	zest_uint *prefiltered_mip_indexes;

	zest_vec3 old_camera_position;
	zest_vec3 new_camera_position;

	double mouse_x, mouse_y;
	double mouse_delta_x, mouse_delta_y;
	float gamma;
	float exposure;
	zest_uint fps;

	zest_atlas_region_t light;
	float ellapsed_time;
	bool sync_refresh;
	int request_graph_print;
	bool reset;
};

void InitPBRTextureExample(PBRTextureExample *app);
void UpdateUniform3d(PBRTextureExample *app);
void UpdateLights(PBRTextureExample *app, float timer);
void UpdateMouse(PBRTextureExample *app);
void UpdateCameraPosition(PBRTextureExample *app);
void UpdateImGui(PBRTextureExample *app);
void UploadMeshData(const zest_command_list context, void *user_data);
void SetupBRDFLUT(PBRTextureExample *app);
void SetupIrradianceCube(PBRTextureExample *app);
void SetupPrefilteredCube(PBRTextureExample *app);
void MainLoop(PBRTextureExample *app);

