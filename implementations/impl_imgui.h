#pragma once

#include <zest.h>
#define IMGUI_DEFINE_MATH_OPERATORS
#include "imgui/imgui.h"
#include <imgui/misc/freetype/imgui_freetype.h>

void zest_imgui_RecordLayer(const zest_render_graph_context_t *context, zest_buffer vertex_buffer, zest_buffer index_buffer);
zest_resource_node zest_imgui_ImportVertexResources(const char *name);
zest_resource_node zest_imgui_ImportIndexResources(const char *name);
void zest_imgui_UpdateBuffers();
int zest_imgui_RecordCondition(zest_draw_routine draw_routine);
void zest_imgui_DrawImage(zest_image image, VkDescriptorSet set, float width, float height);
void zest_imgui_DrawImage2(zest_image image, float width, float height);
void zest_imgui_DrawTexturedRect(zest_image image, float width, float height, bool tile, float scale_x, float scale_y, float offset_x, float offset_y);
void zest_imgui_DrawTexturedRectRT(zest_render_target render_target, float width, float height, bool tile, float scale_x, float scale_y, float offset_x, float offset_y);
bool zest_imgui_DrawButton(zest_image image, const char* user_texture_id, float width, float height, int frame_padding);
void zest_imgui_RebuildFontTexture(zest_uint width, zest_uint height, unsigned char* pixels);
bool zest_imgui_AddToRenderGraph(zest_pass_node render_pass);
void zest_imgui_DrawImGuiRenderPass(VkCommandBuffer command_buffer, const zest_render_graph_context_t *context, void *user_data);
void zest_imgui_UploadImGuiPass(VkCommandBuffer command_buffer, const zest_render_graph_context_t *context, void *user_data);
void zest_imgui_DarkStyle();

