#pragma once

#include <zest.h>
#include "implementations/impl_glfw.h"
#include "implementations/impl_imgui.h"
#include "imgui/imgui.h"
#include <imgui/misc/freetype/imgui_freetype.h>
#include <imgui/backends/imgui_impl_glfw.h>
#include "stb_truetype.h"

struct ImGuiApp {
	zest_imgui_layer_info imgui_layer_info;
	zest_index imgui_draw_routine_index;
	zest_texture imgui_font_texture;

	zest_texture glyph_texture;
	zest_image glyph_image;
	unsigned char *font_buffer;

	zest_pipeline sdf_pipeline;
};

void RasteriseFont(const char *name, ImGuiApp *app);
void InitImGuiApp(ImGuiApp *app);
