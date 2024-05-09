#pragma once

#include <zest.h>
#define IMGUI_DEFINE_MATH_OPERATORS
#include "imgui/imgui.h"
#include <imgui/misc/freetype/imgui_freetype.h>
#include <imgui/backends/imgui_impl_glfw.h>

//This struct must be filled and attached to the draw routine that implements imgui as user data
struct zest_im3d_layer_info {
	zest_layer mesh_layer;
	zest_pipeline pipeline;
};

struct zest_im3d_vertex {
	zest_vec4 position_size;
	zest_vec4 color;
};

void zest_im3d_DrawLayer(zest_draw_routine_t *draw_routine, VkCommandBuffer command_buffer);
void zest_im3d_UpdateBuffers(zest_layer im3d_layer);
void zest_im3d_Initialise(zest_im3d_layer_info *im3d_layer_info);
void zest_im3d_CreateLayer(zest_im3d_layer_info *im3d_layer_info);
