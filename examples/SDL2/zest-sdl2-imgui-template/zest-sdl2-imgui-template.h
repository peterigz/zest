#pragma once

#include <SDL.h>
#include <zest.h>
#include <thread>
#include <atomic>
#include "implementations/impl_imgui.h"
#include "imgui/imgui.h"
#include <imgui/misc/freetype/imgui_freetype.h>
#include <imgui/backends/imgui_impl_sdl2.h>

struct RenderCacheInfo {
	bool draw_imgui;
	zest_image_handle test_texture;
};

struct SpriteState {
	zest_image_handle staging_image_handle;
	zest_atlas_region_t staging_sprite;
	zest_image_handle active_image_handle;
	zest_atlas_region_t active_sprite;
	std::atomic_bool update_ready;
};

struct ImGuiApp {
	zest_index imgui_draw_routine_index;
	zest_image_handle imgui_font_texture;
	zest_image_handle test_texture;
	zest_timer_t timer;
	zest_pipeline_template imgui_sprite_pipeline;
	zest_shader_handle imgui_sprite_shader;
	SpriteState sprite_state;
	zest_context context;
	zest_device device;
	zest_imgui_t imgui;
	RenderCacheInfo cache_info;
	zest_uint sampler_index;
	std::thread loader_thread;
	bool sync_refresh;
	bool request_graph_print;
	bool reset;
	int load_image_index;
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
		out_color = in_color * texture(sampler2D(textures[nonuniformEXT(pc.texture_index)], samplers[nonuniformEXT(pc.sampler_index)]), in_uv.xy);
		out_color = vec4(out_color.r, out_color.g, out_color.b, out_color.a);
	}

);