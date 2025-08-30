#pragma once

#include <zest.h>
#include "implementations/impl_imgui.h"
#include "implementations/impl_glfw.h"
#include "implementations/impl_imgui_glfw.h"
#include "imgui/imgui.h"
#include <imgui/misc/freetype/imgui_freetype.h>
#include <imgui/backends/imgui_impl_glfw.h>
#include "implot.h"

struct ImGuiApp {
	zest_texture imgui_font_texture;
	zest_texture test_texture;
	zest_atlas_region test_image;
	zest_timer timer;
	bool sync_refresh;
};

void InitImGuiApp(ImGuiApp *app);
