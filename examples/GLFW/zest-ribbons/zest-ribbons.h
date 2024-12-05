#pragma once

#include <zest.h>
#include "implementations/impl_imgui.h"
#include "implementations/impl_glfw.h"
#include "implementations/impl_imgui_glfw.h"
#include "imgui/imgui.h"
#include <imgui/misc/freetype/imgui_freetype.h>
#include <imgui/backends/imgui_impl_glfw.h>

struct ribbon_segment {
	zest_vec4 position_and_width;
};

struct Ribbons {
	zest_imgui_layer_info_t imgui_layer_info;
	zest_index imgui_draw_routine_index;
	zest_texture imgui_font_texture;
	bool sync_refresh;

	zest_timer timer;
	zest_camera_t camera;

	zest_pipeline ribbon_pipeline;
	zest_shader ribbon_vert_shader;
	zest_shader ribbon_frag_shader;
	zest_shader ribbon_comp_shader;
	zest_compute ribbon_compute;
	zest_draw_routine ribbon_draw_routine;
	zest_layer ribbon_layer;
	zest_descriptor_buffer ribbon_buffer;

	zest_layer line_layer;
	zest_pipeline line_pipeline;

	ribbon_segment ribbon[100];
	int ribbon_index;
};

void InitImGuiApp(Ribbons *app);
void BuildUI(Ribbons *app);
void UpdateUniform3d(Ribbons *app);
