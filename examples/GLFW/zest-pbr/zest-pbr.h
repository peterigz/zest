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
};

struct UniformLights {
	zest_vec4 lights[4];
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

struct ImGuiApp {
	zest_index imgui_draw_routine_index;
	zest_texture imgui_font_texture;
	zest_timer timer;
	zest_camera_t camera;
	zest_layer cube_layer;
	zest_pipeline_template pbr_pipeline;
	zest_shader_resources pbr_shader_resources;
	zest_uniform_buffer view_buffer;
	zest_uniform_buffer lights_buffer;
	RenderCacheInfo cache_info;
	zest_push_constants_t material_push;
	billboard_push_constant_t billboard_push;
	zest_layer billboard_layer;
	zest_shader_resources sprite_resources;
	zest_texture sprites_texture;
	zest_pipeline_template billboard_pipeline;
	zest_image light;
	float ellapsed_time;
	bool sync_refresh;
	bool request_graph_print;
	bool reset;
};

void InitImGuiApp(ImGuiApp *app);
void UpdateLights(ImGuiApp *app);
