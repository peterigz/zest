#pragma once

#include <zest.h>
#define IMGUI_DEFINE_MATH_OPERATORS
#include "imgui/imgui.h"
#include <imgui/misc/freetype/imgui_freetype.h>

void zest_imgui_RecordLayer(zest_imgui_layer_info_t *layer_info, zest_uint fif);
void zest_imgui_DrawLayer(struct zest_work_queue_t *queue, void *data);
void zest_imgui_UpdateBuffers(zest_layer imgui_layer);
int zest_imgui_RecordCondition(zest_draw_routine draw_routine);
void zest_imgui_DrawImage(zest_image image, float width, float height);
void zest_imgui_DrawImage2(zest_image image, float width, float height);
void zest_imgui_DrawTexturedRect(zest_image image, float width, float height, bool tile, float scale_x, float scale_y, float offset_x, float offset_y);
void zest_imgui_DrawTexturedRectRT(zest_render_target render_target, float width, float height, bool tile, float scale_x, float scale_y, float offset_x, float offset_y);
bool zest_imgui_DrawButton(zest_image image, const char* user_texture_id, float width, float height, int frame_padding);
void zest_imgui_RebuildFontTexture(zest_imgui_layer_info_t *imgui_layer_info, zest_uint width, zest_uint height, unsigned char* pixels);
void zest_imgui_CreateLayer(zest_imgui_layer_info_t *imgui_layer_info);
void zest_imgui_DarkStyle();

