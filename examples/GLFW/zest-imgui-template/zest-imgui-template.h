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
	zest_image_handle test_texture;
};

struct ImGuiApp {
	zest_index imgui_draw_routine_index;
	zest_image_handle imgui_font_texture;
	zest_image_handle test_texture;
	zest_atlas_region test_image;
	zest_timer_handle timer;
	zest_context context;
	zest_atlas_region wabbit_sprite;
	RenderCacheInfo cache_info;
	bool sync_refresh;
	bool request_graph_print;
	bool reset;
};

void InitImGuiApp(ImGuiApp *app);
