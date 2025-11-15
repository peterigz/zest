#pragma once

#include <GLFW/glfw3.h>
#include <zest.h>
#include "implementations/impl_imgui.h"
#include "imgui/imgui.h"
#include <imgui/misc/freetype/imgui_freetype.h>
#include <imgui/backends/imgui_impl_glfw.h>

struct RenderCacheInfo {
	bool draw_imgui;
	zest_image_handle test_texture;
};

struct ImGuiApp {
	zest_index imgui_draw_routine_index;
	zest_image_handle imgui_font_texture;
	zest_image_handle test_texture;
	zest_timer_t timer;
	zest_pipeline_template imgui_sprite_pipeline;
	zest_shader_handle imgui_sprite_shader;
	zest_context context;
	zest_device device;
	zest_atlas_region_t *wabbit_sprite;
	zest_imgui_t imgui;
	RenderCacheInfo cache_info;
	zest_uint atlas_binding_index;
	zest_uint atlas_sampler_binding_index;
	bool sync_refresh;
	bool request_graph_print;
	bool reset;
};

void InitImGuiApp(ImGuiApp *app);
void ImGuiSpriteDrawCallback(const ImDrawList* parent_list, const ImDrawCmd* cmd);

//----------------------
//Imgui fragment shader for 2 channel images
//----------------------
static const char *zest_shader_imgui_r8g8_frag = ZEST_GLSL(450 core,

layout(location = 0) in vec4 in_color;
layout(location = 1) in vec3 in_uv;

layout(location = 0) out vec4 out_color;
layout(set = 0, binding = 0) uniform sampler samplers[];
layout(set = 0, binding = 1) uniform texture2D textures[];

//Not used by default by can be used in custom imgui image shaders
layout(push_constant) uniform imgui_push
{
	vec4 transform;
	uint texture_index;
	uint sampler_index;
	uint image_layer;
} pc;

void main()
{
	out_color = in_color * texture(sampler2D(textures[pc.texture_index], samplers[pc.sampler_index]), in_uv.xy);
	out_color = vec4(out_color.r, out_color.r, out_color.r, out_color.g);
}

);