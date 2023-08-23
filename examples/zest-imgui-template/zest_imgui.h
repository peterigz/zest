#pragma once

#include <zest.h>
#include <GLFW/glfw3.h>
#include "imgui/imgui.h"
#include <imgui/misc/freetype/imgui_freetype.h>
#include <imgui/backends/imgui_impl_glfw.h>

struct ImGuiApp {
	zest_index imgui_layer_index;
	zest_index imgui_draw_routine_index;
	ImDrawData *imgui_draw_data;
	zest_index imgui_pipeline;
	zest_index imgui_font_texture_index;
};

void InitImGuiApp(ImGuiApp *app);
void DrawImGuiLayer(zest_draw_routine *draw_routine, VkCommandBuffer command_buffer);
