#pragma once

#include <SDL.h>
#include <zest.h>
#include "implementations/impl_imgui.h"
#include "imgui/imgui.h"
#include <imgui/misc/freetype/imgui_freetype.h>
#include <imgui/backends/imgui_impl_sdl2.h>

struct RenderCacheInfo {
	bool draw_imgui;
	zest_image_handle test_texture;
};

struct ImGuiApp {
	zest_device device;
	zest_context context;
	zest_imgui_viewport_t *main_viewport;
	RenderCacheInfo cache_info;
	zest_imgui_t imgui;
	zest_timer_t timer;
};

void InitImGui(ImGuiApp *app, zest_context context, zest_imgui_t &imgui);
void ImGuiSpriteDrawCallback(const ImDrawList* parent_list, const ImDrawCmd* cmd);
