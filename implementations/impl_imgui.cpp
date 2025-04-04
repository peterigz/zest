#include "impl_imgui.h"
#include "imgui_internal.h"

//Dear ImGui helper functions
void zest_imgui_RebuildFontTexture(zest_imgui_layer_info_t *imgui_layer_info, zest_uint width, zest_uint height, unsigned char *pixels) {
    zest_WaitForIdleDevice();
    int upload_size = width * height * 4 * sizeof(char);
    zest_bitmap_t font_bitmap = zest_CreateBitmapFromRawBuffer("font_bitmap", pixels, upload_size, width, height, 4);
    zest_ResetTexture(imgui_layer_info->font_texture);
    zest_image font_image = zest_AddTextureImageBitmap(imgui_layer_info->font_texture, &font_bitmap);
    zest_ProcessTextureImages(imgui_layer_info->font_texture);
    zest_RefreshTextureDescriptors(imgui_layer_info->font_texture);
    ImGuiIO &io = ImGui::GetIO();
    io.Fonts->SetTexID((ImTextureID)font_image);
}

void zest_imgui_CreateLayer(zest_imgui_layer_info_t *imgui_layer_info) {
    imgui_layer_info->mesh_layer = zest_NewMeshLayer("imgui mesh layer", sizeof(ImDrawVert));
    imgui_layer_info->pipeline = zest_Pipeline("pipeline_imgui");
    zest_ContextDrawRoutine()->record_callback = zest_imgui_DrawLayer;
    zest_ContextDrawRoutine()->condition_callback = zest_imgui_RecordCondition;
    zest_ContextDrawRoutine()->user_data = imgui_layer_info;
    zest_SetLayerToManualFIF(imgui_layer_info->mesh_layer);
}

void zest_imgui_RecordLayer(zest_imgui_layer_info_t *layer_info, zest_uint fif) {
    zest_layer_t *imgui_layer = layer_info->mesh_layer;

    VkCommandBuffer command_buffer = zest_BeginRecording(imgui_layer->draw_routine->recorder, imgui_layer->draw_routine->draw_commands->render_pass, ZEST_FIF);
    ImDrawData *imgui_draw_data = ImGui::GetDrawData();

    zest_BindMeshVertexBuffer(command_buffer, imgui_layer);
    zest_BindMeshIndexBuffer(command_buffer, imgui_layer);

    zest_pipeline last_pipeline = ZEST_NULL;
    VkDescriptorSet last_descriptor_set = VK_NULL_HANDLE;

    VkViewport view = zest_CreateViewport(0.f, 0.f, zest_SwapChainWidthf(), zest_SwapChainHeightf(), 0.f, 1.f);
    vkCmdSetViewport(command_buffer, 0, 1, &view);

    if (imgui_draw_data && imgui_draw_data->CmdListsCount > 0) {

        int32_t vertex_offset = 0;
        int32_t index_offset = 0;

        ImVec2 clip_off = imgui_draw_data->DisplayPos;
        ImVec2 clip_scale = imgui_draw_data->FramebufferScale;

        for (int32_t i = 0; i < imgui_draw_data->CmdListsCount; i++)
        {
            const ImDrawList *cmd_list = imgui_draw_data->CmdLists[i];
            for (int32_t j = 0; j < cmd_list->CmdBuffer.Size; j++)
            {
                const ImDrawCmd *pcmd = &cmd_list->CmdBuffer[j];

                zest_image_t *current_image = (zest_image_t *)pcmd->TextureId;
                if (!current_image) {
                    //This means we're trying to draw a render target
                    assert(pcmd->UserCallbackData);
                    zest_render_target render_target = static_cast<zest_render_target>(pcmd->UserCallbackData);
                    current_image = zest_GetRenderTargetImage(render_target);
                }

                zest_push_constants_t *push_constants = &imgui_layer->push_constants;

                switch (current_image->struct_type) {
                case zest_struct_type_image:
                    if (last_pipeline != layer_info->pipeline || last_descriptor_set != zest_GetTextureDescriptorSetVK(current_image->texture)) {
                        last_descriptor_set = zest_GetTextureDescriptorSetVK(current_image->texture);
                        zest_BindPipeline(command_buffer, layer_info->pipeline, &last_descriptor_set, 1);
                        last_pipeline = layer_info->pipeline;
                    }
                    push_constants->parameters2.x = (float)current_image->layer;
                    break;
                case zest_struct_type_imgui_image:
                {
                    zest_imgui_image_t *imgui_image = (zest_imgui_image_t *)pcmd->TextureId;
                    //The imgui image must have its image, pipeline and shader resources defined
                    ZEST_ASSERT(imgui_image->image);
                    zest_ClearLayerDrawSets(imgui_layer);
                    zest_uint set_count = zest_GetDescriptorSetsForBinding(imgui_image->shader_resources, &imgui_layer->draw_sets, imgui_layer->fif);
                    zest_BindPipeline(command_buffer, imgui_image->pipeline, imgui_layer->draw_sets, set_count);
                    last_descriptor_set = VK_NULL_HANDLE;
                    last_pipeline = imgui_image->pipeline;
                    push_constants = &imgui_image->push_constants;
                    push_constants->parameters2.x = (float)imgui_image->image->layer;
                }
                break;
                default:
                    //Invalid image
                    ZEST_PRINT_WARNING("%s", "Invalid image found when trying to draw an imgui image. This is usually caused when a texture is changed in another thread before drawing is complete causing the image handle to become invalid due to it being freed.");
                    continue;
                }

                vkCmdPushConstants(command_buffer, layer_info->pipeline->pipeline_layout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(zest_push_constants_t), push_constants);

                ImVec2 clip_min((pcmd->ClipRect.x - clip_off.x) * clip_scale.x, (pcmd->ClipRect.y - clip_off.y) * clip_scale.y);
                ImVec2 clip_max((pcmd->ClipRect.z - clip_off.x) * clip_scale.x, (pcmd->ClipRect.w - clip_off.y) * clip_scale.y);

                // Clamp to viewport as vkCmdSetScissor() won't accept values that are off bounds
                if (clip_max.x > zest_SwapChainWidth()) { clip_max.x = zest_SwapChainWidthf(); }
                if (clip_max.y > zest_SwapChainHeight()) { clip_max.y = zest_SwapChainHeightf(); }
                if (clip_max.x <= clip_min.x || clip_max.y <= clip_min.y) {
                    continue;
                }

                VkRect2D scissor_rect;
                scissor_rect.offset.x = ZEST__MAX((int32_t)(clip_min.x), 0);
                scissor_rect.offset.y = ZEST__MAX((int32_t)(clip_min.y), 0);
                scissor_rect.extent.width = (zest_uint)(clip_max.x - clip_min.x);
                scissor_rect.extent.height = (zest_uint)(clip_max.y - clip_min.y);

                vkCmdSetScissor(command_buffer, 0, 1, &scissor_rect);
                vkCmdDrawIndexed(command_buffer, pcmd->ElemCount, 1, index_offset + pcmd->IdxOffset, vertex_offset, 0);
            }
            index_offset += cmd_list->IdxBuffer.Size;
            vertex_offset += cmd_list->VtxBuffer.Size;
        }
    }
    VkRect2D scissor = { { 0, 0 }, { zest_SwapChainWidth(), zest_SwapChainHeight() } };
    vkCmdSetScissor(command_buffer, 0, 1, &scissor);
    zest_EndRecording(imgui_layer->draw_routine->recorder, ZEST_FIF);
}

void zest_imgui_DrawLayer(struct zest_work_queue_t *queue, void *data) {
    zest_draw_routine draw_routine = (zest_draw_routine)data;
    ImDrawData *imgui_draw_data = ImGui::GetDrawData();

    zest_imgui_layer_info_t *layer_info = (zest_imgui_layer_info_t *)draw_routine->user_data;
    assert(layer_info);	//Must set user data as layer info is referenced later
   
    if (draw_routine->recorder->outdated[ZEST_FIF] != 0) {
        zest_imgui_RecordLayer(layer_info, ZEST_FIF);
    }
}

void zest_imgui_UpdateBuffers(zest_layer imgui_layer) {
    ImDrawData *imgui_draw_data = ImGui::GetDrawData();

    zest_buffer vertex_buffer = zest_GetVertexWriteBuffer(imgui_layer);
    zest_buffer index_buffer = zest_GetIndexWriteBuffer(imgui_layer);

    if (imgui_draw_data) {
        index_buffer->memory_in_use = imgui_draw_data->TotalIdxCount * sizeof(ImDrawIdx);
        vertex_buffer->memory_in_use = imgui_draw_data->TotalVtxCount * sizeof(ImDrawVert);
    } else {
        index_buffer->memory_in_use = 0;
        vertex_buffer->memory_in_use = 0;
    }

    imgui_layer->push_constants.parameters1 = zest_Vec4Set(2.0f / zest_ScreenWidthf(), 2.0f / zest_ScreenHeightf(), -1.f, -1.f);
    imgui_layer->push_constants.parameters2 = zest_Vec4Set1(0.f);

    zest_buffer device_vertex_buffer = zest_GetVertexDeviceBuffer(imgui_layer);
    zest_buffer device_index_buffer = zest_GetIndexDeviceBuffer(imgui_layer);

    if (imgui_draw_data) {
        if (index_buffer->memory_in_use > index_buffer->size) {
            zest_GrowMeshIndexBuffers(imgui_layer);
            index_buffer = zest_GetIndexWriteBuffer(imgui_layer);
        }
        if (vertex_buffer->memory_in_use > vertex_buffer->size) {
            zest_GrowMeshVertexBuffers(imgui_layer);
            vertex_buffer = zest_GetVertexWriteBuffer(imgui_layer);
        }
        ImDrawIdx *idxDst = (ImDrawIdx *)index_buffer->data;
        ImDrawVert *vtxDst = (ImDrawVert *)vertex_buffer->data;

        for (int n = 0; n < imgui_draw_data->CmdListsCount; n++) {
            const ImDrawList *cmd_list = imgui_draw_data->CmdLists[n];
            memcpy(vtxDst, cmd_list->VtxBuffer.Data, cmd_list->VtxBuffer.Size * sizeof(ImDrawVert));
            memcpy(idxDst, cmd_list->IdxBuffer.Data, cmd_list->IdxBuffer.Size * sizeof(ImDrawIdx));
            vtxDst += cmd_list->VtxBuffer.Size;
            idxDst += cmd_list->IdxBuffer.Size;
        }
    }
}

int zest_imgui_RecordCondition(zest_draw_routine draw_routine) {
    ImDrawData *imgui_draw_data = ImGui::GetDrawData();
    if (imgui_draw_data && imgui_draw_data->CmdListsCount > 0) {
        return 1;
    }
    return 0;
}

void zest_imgui_DrawImage(zest_image image, float width, float height) {
    using namespace ImGui;

    ImVec2 image_size((float)image->width, (float)image->height);
    float ratio = image_size.x / image_size.y;
    image_size.x = ratio > 1 ? width : width * ratio;
    image_size.y = ratio > 1 ? height / ratio : height;
    ImVec2 image_offset((width - image_size.x) * .5f, (height - image_size.y) * .5f);
    ImGuiWindow *window = GetCurrentWindow();
    const ImRect image_bb(window->DC.CursorPos + image_offset, window->DC.CursorPos + image_offset + image_size);
    ImVec4 tint_col(1.f, 1.f, 1.f, 1.f);
    window->DrawList->AddImage((ImTextureID)image, image_bb.Min, image_bb.Max, ImVec2(image->uv.x, image->uv.y), ImVec2(image->uv.z, image->uv.w), GetColorU32(tint_col));
}

void zest_imgui_DrawImage2(zest_image image, float width, float height) {
    ImGui::Image((ImTextureID)image, ImVec2(width, height), ImVec2(image->uv.x, image->uv.y), ImVec2(image->uv.z, image->uv.w));
}

void zest_imgui_DrawTexturedRect(zest_image image, float width, float height, bool tile, float scale_x, float scale_y, float offset_x, float offset_y) {

    ImVec2 zw(image->uv.z, image->uv.w);
    zest_vec4 uv = image->uv;
    if (zest_TextureCanTile(image->texture)) {
        if (tile) {
            if (offset_x || offset_y) {
                offset_x = offset_x / float(image->width);
                offset_y = offset_y / float(image->height);
                offset_x *= scale_x;
                offset_y *= scale_y;
            }
            scale_x *= width / float(image->width);
            scale_y *= height / float(image->height);
            zw.x = (uv.z * scale_x) - offset_x;
            zw.y = (uv.w * scale_y) - offset_y;
            uv.x -= offset_x;
            uv.y -= offset_y;
        }
    }

    ImGui::Image((ImTextureID)image, ImVec2(width, height), ImVec2(uv.x, uv.y), zw);
}

void zest_imgui_DrawTexturedRectRT(zest_render_target render_target, float width, float height, bool tile, float scale_x, float scale_y, float offset_x, float offset_y) {
    zest_vec4 uv = { 0.f, 0.f, 1.f, 1.f };
    ImVec2 zw(1.f, 1.f);
    if (tile) {
        if (offset_x || offset_y) {
            offset_x = offset_x / float(render_target->viewport.extent.width);
            offset_y = offset_y / float(render_target->viewport.extent.height);
            offset_x *= scale_x;
            offset_y *= scale_y;
        }
        scale_x *= width / float(render_target->viewport.extent.width);
        scale_y *= height / float(render_target->viewport.extent.height);
        zw.x = (uv.z * scale_x) - offset_x;
        zw.y = (uv.w * scale_y) - offset_y;
        uv.x -= offset_x;
        uv.y -= offset_y;
    }
    ImGuiWindow *window = ImGui::GetCurrentWindow();
    if (window->SkipItems)
        return;

    window->DrawList->PushTextureID(0);
    ImRect bb(window->DC.CursorPos, window->DC.CursorPos + ImVec2(width, height));
    ImGui::ItemSize(bb);
    if (!ImGui::ItemAdd(bb, 0))
        return;

    window->DrawList->PrimReserve(6, 4);
    window->DrawList->PrimRectUV(bb.Min, bb.Max, ImVec2(uv.x, uv.y), zw, IM_COL32_WHITE);
    IM_ASSERT_PARANOID(window->DrawList->CmdBuffer.Size > 0);
    ImDrawCmd *curr_cmd = &window->DrawList->CmdBuffer.Data[window->DrawList->CmdBuffer.Size - 1];
    curr_cmd->UserCallbackData = render_target;

    window->DrawList->PopTextureID();
}

bool zest_imgui_DrawButton(zest_image image, const char *user_texture_id, float width, float height, int frame_padding) {
    using namespace ImGui;

    ImVec2 size(width, height);
    ImVec2 image_size((float)image->width, (float)image->height);
    float ratio = image_size.x / image_size.y;
    image_size.x = ratio > 1 ? width : width * ratio;
    image_size.y = ratio > 1 ? height / ratio : height;
    ImVec2 image_offset((width - image_size.x) * .5f, (height - image_size.y) * .5f);
    ImVec4 bg_col(0.f, 0.f, 0.f, 0.f);
    ImVec4 tint_col(1.f, 1.f, 1.f, 1.f);
    ImVec2 uv0(image->uv.x, image->uv.y);
    ImVec2 uv1(image->uv.z, image->uv.w);

    ImGuiWindow *window = GetCurrentWindow();
    if (window->SkipItems)
        return false;

    ImGuiContext &g = *GImGui;
    const ImGuiStyle &style = g.Style;

    // Default to using texture ID as ID. User can still push string/integer prefixes.
    // We could hash the size/uv to create a unique ID but that would prevent the user from animating UV.
    PushID(user_texture_id);
    const ImGuiID id = window->GetID("#image");
    PopID();

    const ImVec2 padding = (frame_padding >= 0) ? ImVec2((float)frame_padding, (float)frame_padding) : style.FramePadding;
    const ImRect bb(window->DC.CursorPos, window->DC.CursorPos + size + padding * 2);
    const ImRect image_bb(window->DC.CursorPos + padding + image_offset, window->DC.CursorPos + padding + image_offset + image_size);
    ItemSize(bb);
    if (!ItemAdd(bb, id))
        return false;

    bool hovered, held;
    bool pressed = ButtonBehavior(bb, id, &hovered, &held);

    // Render
    const ImU32 col = GetColorU32((held && hovered) ? ImGuiCol_ButtonActive : hovered ? ImGuiCol_ButtonHovered : ImGuiCol_Button);
    RenderNavCursor(bb, id);
    RenderFrame(bb.Min, bb.Max, col, true, ImClamp((float)ImMin(padding.x, padding.y), 0.0f, style.FrameRounding));
    if (bg_col.w > 0.0f)
        window->DrawList->AddRectFilled(image_bb.Min, image_bb.Max, GetColorU32(bg_col));
    window->DrawList->AddImage((ImTextureID)image, image_bb.Min, image_bb.Max, uv0, uv1, GetColorU32(tint_col));

    return pressed;
}

void zest_imgui_DarkStyle() {
    ImGui::GetStyle().FrameRounding = 4.0f;
    ImGui::GetStyle().GrabRounding = 4.0f;
    ImVec4 *colors = ImGui::GetStyle().Colors;
    colors[ImGuiCol_Text] = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
    colors[ImGuiCol_TextDisabled] = ImVec4(1.00f, 1.00f, 1.00f, 0.26f);
    colors[ImGuiCol_WindowBg] = ImVec4(0.15f, 0.15f, 0.15f, 0.95f);
    colors[ImGuiCol_ChildBg] = ImVec4(0.18f, 0.18f, 0.18f, 1.00f);
    colors[ImGuiCol_PopupBg] = ImVec4(0.14f, 0.14f, 0.14f, 1.00f);
    colors[ImGuiCol_Border] = ImVec4(1.00f, 1.00f, 1.00f, 0.07f);
    colors[ImGuiCol_BorderShadow] = ImVec4(0.00f, 0.00f, 0.00f, 0.32f);
    colors[ImGuiCol_FrameBg] = ImVec4(0.11f, 0.11f, 0.11f, 1.00f);
    colors[ImGuiCol_FrameBgHovered] = ImVec4(0.25f, 0.25f, 0.25f, 1.00f);
    colors[ImGuiCol_FrameBgActive] = ImVec4(0.25f, 0.25f, 0.25f, 1.00f);
    colors[ImGuiCol_TitleBg] = ImVec4(0.10f, 0.10f, 0.10f, 1.00f);
    colors[ImGuiCol_TitleBgActive] = ImVec4(0.10f, 0.10f, 0.10f, 1.00f);
    colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.10f, 0.10f, 0.10f, 1.00f);
    colors[ImGuiCol_MenuBarBg] = ImVec4(0.15f, 0.15f, 0.15f, 0.95f);
    colors[ImGuiCol_ScrollbarBg] = ImVec4(0.02f, 0.02f, 0.02f, 0.34f);
    colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.22f, 0.22f, 0.22f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.31f, 0.31f, 0.31f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.86f, 0.31f, 0.02f, 1.00f);
    colors[ImGuiCol_CheckMark] = ImVec4(0.91f, 0.35f, 0.05f, 1.00f);
    colors[ImGuiCol_SliderGrab] = ImVec4(0.86f, 0.31f, 0.02f, 0.53f);
    colors[ImGuiCol_SliderGrabActive] = ImVec4(0.86f, 0.31f, 0.02f, 1.00f);
    colors[ImGuiCol_Button] = ImVec4(0.5f, 0.5f, 0.5f, .25f);
    colors[ImGuiCol_ButtonHovered] = ImVec4(0.86f, 0.31f, 0.02f, 0.53f);
    colors[ImGuiCol_ButtonActive] = ImVec4(0.86f, 0.31f, 0.02f, 1.00f);
    colors[ImGuiCol_Header] = ImVec4(0.00f, 0.00f, 0.00f, 0.36f);
    colors[ImGuiCol_HeaderHovered] = ImVec4(0.86f, 0.31f, 0.02f, 0.53f);
    colors[ImGuiCol_HeaderActive] = ImVec4(0.86f, 0.31f, 0.02f, 0.53f);
    colors[ImGuiCol_Separator] = ImVec4(0.00f, 0.00f, 0.00f, 0.22f);
    colors[ImGuiCol_SeparatorHovered] = ImVec4(0.86f, 0.31f, 0.02f, 0.53f);
    colors[ImGuiCol_SeparatorActive] = ImVec4(0.86f, 0.31f, 0.02f, 1.00f);
    colors[ImGuiCol_ResizeGrip] = ImVec4(0.86f, 0.31f, 0.02f, 0.53f);
    colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.86f, 0.31f, 0.02f, 0.53f);
    colors[ImGuiCol_ResizeGripActive] = ImVec4(0.86f, 0.31f, 0.02f, 1.00f);
    colors[ImGuiCol_Tab] = ImVec4(0.17f, 0.17f, 0.17f, 0.00f);
    colors[ImGuiCol_TabHovered] = ImVec4(0.86f, 0.31f, 0.02f, 0.80f);
    colors[ImGuiCol_TabSelected] = ImVec4(0.86f, 0.31f, 0.02f, 1.00f);
    colors[ImGuiCol_TabDimmed] = ImVec4(0.86f, 0.31f, 0.02f, 0.00f);
    colors[ImGuiCol_TabDimmedSelected] = ImVec4(0.86f, 0.31f, 0.02f, 0.68f);
    colors[ImGuiCol_DockingPreview] = ImVec4(0.86f, 0.31f, 0.02f, 1.00f);
    colors[ImGuiCol_DockingEmptyBg] = ImVec4(0.0f, 0.0f, 0.0f, 0.00f);
    colors[ImGuiCol_PlotLines] = ImVec4(0.61f, 0.61f, 0.61f, 1.00f);
    colors[ImGuiCol_PlotLinesHovered] = ImVec4(1.00f, 0.43f, 0.35f, 1.00f);
    colors[ImGuiCol_PlotHistogram] = ImVec4(0.90f, 0.70f, 0.00f, 1.00f);
    colors[ImGuiCol_PlotHistogramHovered] = ImVec4(1.00f, 0.60f, 0.00f, 1.00f);
    colors[ImGuiCol_TableHeaderBg] = ImVec4(0.08f, 0.08f, 0.08f, 0.59f);
    colors[ImGuiCol_TableBorderStrong] = ImVec4(0.00f, 0.00f, 0.00f, 0.2f);
    colors[ImGuiCol_TableBorderLight] = ImVec4(0.00f, 0.00f, 0.00f, 0.1f);
    colors[ImGuiCol_TableRowBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    colors[ImGuiCol_TableRowBgAlt] = ImVec4(0.00f, 0.00f, 0.00f, 0.11f);
    colors[ImGuiCol_TextSelectedBg] = ImVec4(0.26f, 0.59f, 0.98f, 0.35f);
    colors[ImGuiCol_DragDropTarget] = ImVec4(0.86f, 0.31f, 0.02f, 1.00f);
    colors[ImGuiCol_NavCursor] = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
    colors[ImGuiCol_NavWindowingHighlight] = ImVec4(1.00f, 1.00f, 1.00f, 0.70f);
    colors[ImGuiCol_NavWindowingDimBg] = ImVec4(0.80f, 0.80f, 0.80f, 0.20f);
    colors[ImGuiCol_ModalWindowDimBg] = ImVec4(0.80f, 0.80f, 0.80f, 0.35f);

    ImGuiStyle &style = ImGui::GetStyle();
    style.WindowPadding.x = 10.f;
    style.WindowPadding.y = 10.f;
    style.FramePadding.x = 6.f;
    style.FramePadding.y = 4.f;
    style.ItemSpacing.x = 10.f;
    style.ItemSpacing.y = 6.f;
    style.ItemInnerSpacing.x = 5.f;
    style.ItemInnerSpacing.y = 5.f;
    style.WindowRounding = 0.f;
    style.FrameRounding = 4.f;
    style.ScrollbarRounding = 1.f;
    style.GrabRounding = 1.f;
    style.TabRounding = 4.f;
    style.IndentSpacing = 20.f;
    style.FrameBorderSize = 1.0f;

    style.ChildRounding = 0.f;
    style.PopupRounding = 0.f;
    style.CellPadding.x = 4.f;
    style.CellPadding.y = 2.f;
    style.TouchExtraPadding.x = 0.f;
    style.TouchExtraPadding.y = 0.f;
    style.ColumnsMinSpacing = 6.f;
    style.ScrollbarSize = 14.f;
    style.ScrollbarRounding = 1.f;
    style.GrabMinSize = 10.f;
    //style.TabMinWidthForCloseButton = 0.f;
    style.DisplayWindowPadding.x = 19.f;
    style.DisplayWindowPadding.y = 19.f;
    style.DisplaySafeAreaPadding.x = 3.f;
    style.DisplaySafeAreaPadding.y = 3.f;
    style.MouseCursorScale = 1.f;
    style.WindowMinSize.x = 32.f;
    style.WindowMinSize.y = 32.f;
    style.ChildBorderSize = 1.f;
}
//--End Dear ImGui helper functions
