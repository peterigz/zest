#pragma once

#include <GLFW/glfw3.h>
#include <zest.h>
#include "implementations/impl_imgui.h"
#include "imgui/imgui.h"
#include <imgui/misc/freetype/imgui_freetype.h>
#include <imgui/backends/imgui_impl_glfw.h>
#include <vector>

#define SEGMENT_COUNT 128 
#define RIBBON_COUNT 10

struct ribbon_segment {
	zest_vec4 position_and_width;
	zest_color_t color;
	zest_uint padding[3];
};

struct ribbon {
	zest_uint length;
	zest_uint ribbon_index;
	zest_uint start_index;
};

struct ribbon_instance {
	float width_scale;
	zest_uint start_index;
};

struct ribbon_vertex {
	zest_vec4 position;
	zest_vec4 uv;
	zest_color_t color;
	zest_uint padding[3];
};

struct RibbonBufferInfo {
	uint32_t verticesPerSegment;      // Number of vertices per segment (4 * tessellation)
	uint32_t trianglesPerSegment;     // Number of triangles per segment
	uint32_t indicesPerSegment;       // Number of indices per segment
};

struct camera_push_constant {
    zest_vec4 position;
	float uv_scale;
	float uv_offset;
	float width_scale_multiplier;
	zest_uint segment_count;
	zest_uint tessellation;  // Added tessellation factor
	zest_uint index_offset;
	zest_uint ribbon_count;
	zest_uint segment_buffer_index;
	zest_uint instance_buffer_index;
	zest_uint vertex_buffer_index;
	zest_uint index_buffer_index;
};

struct ribbon_drawing_push_constants {
	zest_uint sampler_index;
	zest_uint texture_index;
	zest_uint uniform_index;
};

struct RibbonUniform {
	zest_matrix4 view;
	zest_matrix4 proj;
};

struct Ribbons {
	zest_imgui_t imgui;
	zest_context context;
	zest_device device;
	bool sync_refresh;

	zest_timer_t timer;
	zest_camera_t camera;
	zest_uint last_fps;
	
	zest_pipeline_template ribbon_pipeline;
	zest_uint compute_pipeline_index;
	zest_shader_handle ribbon_vert_shader;
	zest_shader_handle ribbon_frag_shader;
	zest_shader_handle ribbon_comp_shader;
	zest_compute_handle ribbon_compute;
	zest_draw_batch ribbon_layer;
	zest_uniform_buffer_handle uniform_buffer;
	zest_buffer ribbon_segment_staging_buffer[ZEST_MAX_FIF];
	zest_buffer ribbon_instance_staging_buffer[ZEST_MAX_FIF];
	camera_push_constant camera_push;
	ribbon_drawing_push_constants ribbon_push_constants;
	zest_image_handle ribbon_texture;
	zest_atlas_region_t ribbon_image;
	zest_sampler_handle sampler;
	zest_uint sampler_index;
	zest_uint index_count;
	float seconds_passed;

	zest_buffer_resource_info_t segment_buffer_info;
	zest_buffer_resource_info_t instance_buffer_info;
	zest_buffer_resource_info_t vertex_buffer_info;
	zest_buffer_resource_info_t index_buffer_info;

	ribbon ribbons[RIBBON_COUNT];
	ribbon_instance ribbon_instances[RIBBON_COUNT];
	ribbon_segment ribbon_segments[SEGMENT_COUNT * RIBBON_COUNT];
	int ribbon_count;
	std::vector<uint32_t> ribbon_indices;
	RibbonBufferInfo ribbon_buffer_info;
	zest_uint ribbon_built;

	double mouse_x, mouse_y;
	double mouse_delta_x, mouse_delta_y;
};

RibbonBufferInfo GenerateRibbonInfo(uint32_t tessellation, uint32_t maxSegments, uint32_t max_ribbons);
void InitImGuiApp(Ribbons *app);
void BuildUI(Ribbons *app);
void UpdateUniform3d(Ribbons *app);
void RecordComputeCommands(zest_command_list command_list, void *user_data);
void RecordRibbonDrawing(zest_command_list command_list, void *user_data);
void UploadRibbonData(zest_command_list command_list, void *user_data);
zest_uint CountSegments(Ribbons *app);
void UpdateRibbons(Ribbons *app);
