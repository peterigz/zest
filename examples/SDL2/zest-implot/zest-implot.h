#pragma once

#include <SDL.h>
#include <zest.h>
#include "implementations/impl_imgui.h"
#include "imgui/imgui.h"
#include <imgui/misc/freetype/imgui_freetype.h>
#include <imgui/backends/imgui_impl_sdl2.h>
#include "implot.h"

struct ImGuiApp {
	zest_context context;
	zest_device device;
	zest_timer_t timer;
	bool sync_refresh;
	zest_imgui_t imgui;
};

void InitImGuiApp(ImGuiApp *app);
