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
    zest_matrix4 inv_view_proj;  // For reconstructing world position from depth
    zest_vec2 screen_size;
    float timer_lerp;
    float update_time;
} uniform_buffer_data_t;

struct deferred_mesh_instance_t {
	zest_vec3 pos;                                 //3d position
	zest_color_t color;                              //packed color
	zest_vec3 rotation;
	float roughness;                          //pbr roughness
	zest_vec3 scale;
	float metallic;                          //pbr metallic
};

struct  composite_push_constant_t{
	zest_uint sampler_index;
	zest_uint gTarget_index;
};

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
	zest_vec3 color;
	zest_uint irradiance_index;
	zest_uint brd_lookup_index;
	zest_uint pre_filtered_index;
	zest_uint sampler_index;
	zest_uint view_buffer_index;
	zest_uint lights_buffer_index;
};

// G-buffer pass push constants (vec3 aligned to 16 bytes in GLSL)
struct gbuffer_push_t {
	zest_vec3 color;              // Albedo tint (12 bytes)
	zest_uint _padding;           // Padding to align to 16 bytes
	zest_uint view_buffer_index;
};

struct mouse_t {
	double mouse_x, mouse_y;
	double mouse_delta_x, mouse_delta_y;
};

// Deferred lighting pass push constants
struct deferred_lighting_push_t {
	zest_vec4 camera;
	zest_uint gDepth_index;      // Depth buffer for position reconstruction
	zest_uint gNormal_index;
	zest_uint gAlbedo_index;
	zest_uint gPBR_index;
	zest_uint irradiance_index;
	zest_uint brd_lookup_index;
	zest_uint pre_filtered_index;
	zest_uint sampler_index;
	zest_uint view_index;
	zest_uint lights_index;
	zest_vec3 color;
};

struct SimplePBRExample {
	zest_context context;
	zest_device device;
	zest_imgui_t imgui;
	zest_uint imgui_draw_routine_index;
	zest_timer_t timer;
	zest_camera_t camera;

	zest_layer_handle mesh_layer;
	zest_layer_handle skybox_layer;

	zest_uint teapot_index;
	zest_uint torus_index;
	zest_uint venus_index;
	zest_uint cube_index;
	zest_uint sphere_index;
	zest_uint skybox_index;

	zest_pipeline_template skybox_pipeline;

	// Deferred rendering pipelines
	zest_pipeline_template gbuffer_pipeline;
	zest_pipeline_template lighting_pipeline;
	zest_pipeline_template composite_pipeline;

	zest_uniform_buffer_handle view_buffer;
	zest_uniform_buffer_handle lights_buffer;

	RenderCacheInfo cache_info;

	pbr_consts_t material_push;
	irr_push_constant_t irr_push_constant;
	prefiltered_push_constant_t prefiltered_push_constant;
	composite_push_constant_t composite_push;

	// Deferred rendering push constants
	gbuffer_push_t gbuffer_push;
	deferred_lighting_push_t lighting_push;

	// Deferred rendering shaders
	zest_shader_handle gbuffer_vert;
	zest_shader_handle gbuffer_frag;
	zest_shader_handle lighting_vert;
	zest_shader_handle lighting_frag;
	zest_shader_handle composite_frag;

	zest_image_handle imgui_font_texture;
	zest_image_handle skybox_texture;
	zest_image_handle brd_texture;
	zest_image_handle irr_texture;
	zest_image_handle prefiltered_texture;

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

	mouse_t mouse;
	zest_uint fps;

	zest_atlas_region_t light;
	float ellapsed_time;
	bool sync_refresh;
	int request_graph_print;
	bool reset;
};

void InitSimplePBRExample(SimplePBRExample *app);
void UpdateUniform3d(SimplePBRExample *app);
void UpdateLights(SimplePBRExample *app, float timer);
void UpdateImGui(SimplePBRExample *app);
void UploadMeshData(const zest_command_list context, void *user_data);
void SetupBRDFLUT(SimplePBRExample *app);
void SetupIrradianceCube(SimplePBRExample *app);
void SetupPrefilteredCube(SimplePBRExample *app);
void MainLoop(SimplePBRExample *app);

// Deferred rendering callbacks
void DrawGBufferPass(const zest_command_list command_list, void *user_data);
void DrawLightingPass(const zest_command_list command_list, void *user_data);
void DrawComposite(const zest_command_list command_list, void *user_data);

