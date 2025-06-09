#pragma once

#include <zest.h>
#include "implementations/impl_imgui.h"
#include "implementations/impl_glfw.h"
#include "implementations/impl_imgui_glfw.h"
#include "imgui/imgui.h"
#include <imgui/misc/freetype/imgui_freetype.h>
#include <imgui/backends/imgui_impl_glfw.h>

struct ImGuiApp {
	zest_index imgui_draw_routine_index;
	zest_texture imgui_font_texture;
	zest_texture test_texture;
	zest_image test_image;
	zest_timer timer;
	zest_command_queue command_queue;
	zest_command_queue_draw_commands draw_commands;
	bool sync_refresh;
	bool request_graph_print;
};

void InitImGuiApp(ImGuiApp *app);
