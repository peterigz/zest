#pragma once

#include <zest.h>
#include "implementations/impl_imgui.h"
#include "implementations/impl_sdl2.h"
#include "implementations/impl_imgui_sdl2.h"
#include "imgui/imgui.h"
#include <imgui/misc/freetype/imgui_freetype.h>
#include <imgui/backends/imgui_impl_sdl2.h>

struct RenderCacheInfo {
	bool draw_imgui;
};

struct ImGuiApp {
	zest_imgui_t imgui_layer_info;
	zest_index imgui_draw_routine_index;
	zest_texture imgui_font_texture;
	zest_texture test_texture;
	zest_atlas_region test_image;
	zest_timer timer;
	RenderCacheInfo cache_info;
	bool reset;
	bool request_graph_print;
};

void InitImGuiApp(ImGuiApp *app);

