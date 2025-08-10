#pragma once

#include <zest.h>
#define IMGUI_DEFINE_MATH_OPERATORS
#include "imgui/imgui.h"
#include <imgui/misc/freetype/imgui_freetype.h>

void zest_imgui_RecordLayer(const zest_render_graph_context_t *context, zest_buffer vertex_buffer, zest_buffer index_buffer);
zest_resource_node zest_imgui_ImportVertexResources(const char *name);
zest_resource_node zest_imgui_ImportIndexResources(const char *name);
void zest_imgui_UpdateBuffers();
void zest_imgui_DrawImage(zest_image image, VkDescriptorSet set, float width, float height);
void zest_imgui_DrawImage2(zest_image image, float width, float height);
void zest_imgui_DrawTexturedRect(zest_image image, float width, float height, bool tile, float scale_x, float scale_y, float offset_x, float offset_y);
bool zest_imgui_DrawButton(zest_image image, const char* user_texture_id, float width, float height, int frame_padding);
void zest_imgui_RebuildFontTexture(zest_uint width, zest_uint height, unsigned char* pixels);
zest_pass_node zest_imgui_AddToRenderGraph();
zest_buffer zest_imgui_VertexBufferProvider(zest_resource_node resource);
zest_buffer zest_imgui_IndexBufferProvider(zest_resource_node resource);
void zest_imgui_DrawImGuiRenderPass(VkCommandBuffer command_buffer, const zest_render_graph_context_t *context, void *user_data);
void zest_imgui_UploadImGuiPass(VkCommandBuffer command_buffer, const zest_render_graph_context_t *context, void *user_data);
void zest_imgui_DarkStyle();
bool zest_imgui_HasGuiToDraw();

