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

struct ImGuiApp {
	zest_index imgui_draw_routine_index;
	zest_texture imgui_font_texture;
	zest_timer timer;
	zest_camera_t camera;
	RenderCacheInfo cache_info;
	bool sync_refresh;
	bool request_graph_print;
	bool reset;
};

void InitImGuiApp(ImGuiApp *app);
