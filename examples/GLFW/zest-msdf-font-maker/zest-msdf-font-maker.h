#pragma once

#include <msdf-atlas-gen/msdf-atlas-gen.h>
using namespace msdf_atlas;
#define ZEST_ENABLE_VALIDATION_LAYER 1
#include <zest.h>
#include "implementations/impl_glfw.h"
#include "implementations/impl_imgui.h"
#include "imgui/imgui.h"
#include <imgui/misc/freetype/imgui_freetype.h>
#include <imgui/backends/imgui_impl_glfw.h>
#include <imgui_file_dialog/ImGuiFileDialog.h>

struct zest_font_configuration {
	std::string font_file;
	std::string font_path;
	std::string font_save_file;
	std::string font_folder;
	std::string zft_file;
	int width, height;
	int thread_count;
	int padding = 8;
	double px_range = 6;
	double miter_limit = 0;
	double minimum_scale = MSDF_ATLAS_DEFAULT_EM_SIZE;
};

struct ImGuiApp {
	zest_imgui_layer_info imgui_layer_info;
	zest_texture imgui_font_texture;
	zest_font_configuration config;

	zest_bool load_next_font;
	zest_font font;
	zest_layer font_layer;
	float preview_text_x;
	float preview_text_y;
	char preview_text[100];
	float preview_color[4];
	float background_color[4];
};

struct custom_stbi_mem_context{
	int last_pos;
	void *context;
};

static void custom_stbi_write_mem(void *context, void *data, int size) {
	custom_stbi_mem_context *c = (custom_stbi_mem_context*)context;
	std::vector<char> *buffer = static_cast<std::vector<char>*>(c->context);
	char *src = (char *)data;
	int cur_pos = c->last_pos;
	for (int i = 0; i < size; i++) {
		buffer->push_back(src[i]);
	}
	c->last_pos = cur_pos;
}

void InitImGuiApp(ImGuiApp *app);
bool GenerateAtlas(const char *fontFilename, const char *save_to, zest_font_configuration *config);
