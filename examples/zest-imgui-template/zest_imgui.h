#pragma once

#include <zest.h>
#include <GLFW/glfw3.h>
#include "imgui/imgui.h"
#include <imgui/misc/freetype/imgui_freetype.h>
#include <imgui/backends/imgui_impl_glfw.h>

struct ImGuiApp {
	zest_index imgui_layer_index;
	zest_index imgui_draw_routine_index;
	zest_index imgui_pipeline;
	zest_index imgui_font_texture_index;
	zest_index test_texture_index;
	zest_index test_image_index;
};

void InitImGuiApp(ImGuiApp *app);
void DrawImGuiLayer(zest_draw_routine *draw_routine, VkCommandBuffer command_buffer);
void CopyImGuiBuffers(ImGuiApp *app);

zest_window *zest_implglfw_CreateWindowCallback(int x, int y, int width, int height, zest_bool maximised, const char* title);
void zest_implglfw_CreateWindowSurfaceCallback(zest_window* window);
void zest_implglfw_PollEventsCallback(void);
void zest_implglfw_AddPlatformExtensionsCallback(void);
void zest_implglfw_GetWindowSizeCallback(void *user_data, int *width, int *height);
void zest_implglfw_DestroyWindowCallback(void *user_data);

void zest_DrawImGuiLayer(zest_draw_routine *draw_routine, VkCommandBuffer command_buffer);
void zest_CopyImGuiBuffers(zest_index mesh_layer_index);
void zest_DrawImGuiImage(zest_image *image, float width, float height);
void zest_DrawImGuiImage2(zest_image *image, float width, float height);
