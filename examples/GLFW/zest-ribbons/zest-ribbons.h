#pragma once

#include <vector>
#include <zest.h>
#include "implementations/impl_imgui.h"
#include "implementations/impl_glfw.h"
#include "implementations/impl_imgui_glfw.h"
#include "imgui/imgui.h"
#include <imgui/misc/freetype/imgui_freetype.h>
#include <imgui/backends/imgui_impl_glfw.h>

struct ribbon_segment {
	zest_vec4 position_and_width;
	zest_vec4 parameters;
};

struct ribbon_vertex {
	zest_vec4 position;
	zest_vec4 uv;
};

struct RibbonBufferInfo {
	uint32_t verticesPerSegment;      // Number of vertices per segment (4 * tessellation)
	uint32_t trianglesPerSegment;     // Number of triangles per segment
	uint32_t indicesPerSegment;       // Number of indices per segment
	uint32_t totalIndices;            // Total number of indices in the buffer
	uint32_t vertexStrideMultiplier;  // How many vertices to advance per segment
};

struct camera_push_constant {
    zest_vec4 position;
	zest_matrix4 view;
	float uv_scale;
	float uv_offset;
	float width_scale_multiplier;
	zest_uint segment_count;
	zest_uint tessellation;  // Added tessellation factor
};

#define SEGMENT_COUNT 256 

struct Ribbons {
	zest_imgui_layer_info_t imgui_layer_info;
	zest_index imgui_draw_routine_index;
	zest_texture imgui_font_texture;
	bool sync_refresh;

	zest_timer timer;
	zest_camera_t camera;

	zest_pipeline ribbon_pipeline;
	zest_index compute_pipeline_index;
	zest_shader ribbon_vert_shader;
	zest_shader ribbon_frag_shader;
	zest_shader_resources ribbon_shader_resources;
	zest_shader ribbon_comp_shader;
	zest_compute ribbon_compute;
	zest_draw_routine ribbon_draw_routine;
	zest_layer ribbon_layer;
	zest_descriptor_buffer ribbon_buffer;
	zest_descriptor_buffer ribbon_vertex_buffer;
	zest_descriptor_buffer ribbon_index_buffer;
	zest_buffer ribbon_staging_buffer[ZEST_MAX_FIF];
	camera_push_constant camera_push;
	zest_descriptor_set_layout ribbon_descriptor_layout;
	zest_texture ribbon_texture;
	zest_image ribbon_image;

	zest_layer line_layer;
	zest_pipeline line_pipeline;

	ribbon_segment ribbon[SEGMENT_COUNT];
	std::vector<uint32_t> ribbon_indices;
	RibbonBufferInfo ribbon_buffer_info;
	int current_ribbon_length;
	int ribbon_index;
	zest_uint ribbon_built;
};

RibbonBufferInfo GenerateRibbonIndices(Ribbons *app, uint32_t tessellation, uint32_t maxSegments);
void InitImGuiApp(Ribbons *app);
void BuildUI(Ribbons *app);
void UpdateUniform3d(Ribbons *app);
void RecordComputeRibbons(zest_work_queue_t *queue, void *data);
int DrawComputeRibbonsCondition(zest_draw_routine draw_routine);
void UploadBuffers(Ribbons *app);
void RibbonComputeFunction(zest_command_queue_compute compute_routine);
