#pragma once

#include <zest.h>
#include "imgui/imgui.h"
#include <imgui/misc/freetype/imgui_freetype.h>
#include <imgui/backends/imgui_impl_glfw.h>

//This struct must be filled and attached to the draw routine that implements imgui as user data
struct zest_imgui_layer_info {
	zest_index mesh_layer_index;
	zest_index pipeline_index;
};

void zest_imgui_DrawLayer(zest_draw_routine *draw_routine, VkCommandBuffer command_buffer);
void zest_imgui_CopyBuffers(zest_index mesh_layer_index);
void zest_imgui_DrawImage(zest_image *image, float width, float height);
void zest_imgui_DrawImage2(zest_image *image, float width, float height);
