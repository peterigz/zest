#pragma once

#include <zest.h>
#include "imgui/imgui.h"
#include <imgui/misc/freetype/imgui_freetype.h>
#include <imgui/backends/imgui_impl_glfw.h>

//This struct must be filled and attached to the draw routine that implements imgui as user data
struct zest_imgui_layer_info {
	zest_layer mesh_layer;
	zest_pipeline pipeline;
};

void zest_imgui_DrawLayer(zest_draw_routine_t *draw_routine, VkCommandBuffer command_buffer);
void zest_imgui_CopyBuffers(zest_layer imgui_layer);
void zest_imgui_DrawImage(zest_image image, float width, float height);
void zest_imgui_DrawImage2(zest_image image, float width, float height);
void zest_imgui_Initialise();
void zest_imgui_CreateLayer(zest_imgui_layer_info *imgui_layer_info);
