#include "impl_imgui.h"
#include "imgui_internal.h"

zest_imgui_t *ZestImGui = 0;

zest_imgui_t *zest_imgui_Initialise(zest_context context) {
    ZestImGui = (zest_imgui_t*)zest_AllocateMemory(context, sizeof(zest_imgui_t));
    memset(ZestImGui, 0, sizeof(zest_imgui_t));
	ZestImGui->context = context;
    ZestImGui->magic = zest_INIT_MAGIC(zest_struct_type_imgui);
    ImGui::CreateContext();
    ImGuiIO &io = ImGui::GetIO();
    io.DisplaySize = ImVec2(zest_ScreenWidthf(context), zest_ScreenHeightf(context));
    io.DisplayFramebufferScale = ImVec2(context->dpi_scale, context->dpi_scale);
    unsigned char *pixels;
    int width, height;
    io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
    int upload_size = width * height * 4 * sizeof(char);

    zest_bitmap font_bitmap = zest_CreateBitmapFromRawBuffer(context, "font_bitmap", pixels, upload_size, width, height, 4);
	zest_image_info_t image_info = zest_CreateImageInfo(width, height);
    image_info.flags = zest_image_preset_texture;
    ZestImGui->font_texture = zest_CreateImage(context, &image_info);
    ZestImGui->font_region = zest_CreateAtlasRegion(ZestImGui->font_texture);
    zest_cmd_CopyBitmapToImage(font_bitmap, ZestImGui->font_texture, 0, 0, 0, 0, width, height);
    zest_FreeBitmap(font_bitmap);

    zest_sampler_info_t sampler_info = zest_CreateSamplerInfo();
    ZestImGui->font_sampler = zest_CreateSampler(context, &sampler_info);
    ZestImGui->font_texture_binding_index = zest_AcquireGlobalSampledImageIndex(ZestImGui->font_texture, zest_texture_2d_binding);
    ZestImGui->font_sampler_binding_index = zest_AcquireGlobalSamplerIndex(ZestImGui->font_sampler, zest_sampler_binding);

    //ZestImGui->vertex_shader = zest_CreateShaderSPVMemory(zest_imgui_vert_spv, zest_imgui_vert_spv_len, "imgui_vert.spv", shaderc_vertex_shader);
    //ZestImGui->fragment_shader = zest_CreateShaderSPVMemory(zest_imgui_frag_spv, zest_imgui_frag_spv_len, "imgui_frag.spv", shaderc_fragment_shader);

	shaderc_compiler_t compiler = shaderc_compiler_initialize();
	ZestImGui->vertex_shader = zest_CreateShader(context, zest_shader_imgui_vert, shaderc_vertex_shader, "imgui_vert", ZEST_FALSE, compiler, 0);
	ZestImGui->fragment_shader = zest_CreateShader(context, zest_shader_imgui_frag, shaderc_fragment_shader, "imgui_frag", ZEST_FALSE, compiler, 0);
	shaderc_compiler_release(compiler);

    ZestImGui->font_resources = zest_CreateShaderResources(context);
    zest_AddGlobalBindlessSetToResources(ZestImGui->font_resources);

    //ImGuiPipeline
    zest_pipeline_template imgui_pipeline = zest_BeginPipelineTemplate(context, "pipeline_imgui");
    zest_SetPipelinePushConstantRange(imgui_pipeline, sizeof(zest_imgui_push_t), zest_shader_render_stages);
    zest_AddVertexInputBindingDescription(imgui_pipeline, 0, sizeof(zest_ImDrawVert_t), zest_input_rate_vertex);
    zest_AddVertexAttribute(imgui_pipeline, 0, 0, zest_format_r32g32_sfloat, offsetof(zest_ImDrawVert_t, pos));    // Location 0: Position
    zest_AddVertexAttribute(imgui_pipeline, 0, 1, zest_format_r32g32_sfloat, offsetof(zest_ImDrawVert_t, uv));    // Location 1: UV
    zest_AddVertexAttribute(imgui_pipeline, 0, 2, zest_format_r8g8b8a8_unorm, offsetof(zest_ImDrawVert_t, col));    // Location 2: Color

    zest_SetPipelineShaders(imgui_pipeline, ZestImGui->vertex_shader, ZestImGui->fragment_shader);

    imgui_pipeline->flags |= zest_pipeline_set_flag_match_swapchain_view_extent_on_rebuild;
    zest_ClearPipelineDescriptorLayouts(imgui_pipeline);
    zest_AddPipelineDescriptorLayout(imgui_pipeline, zest_GetGlobalBindlessLayout(context));

    imgui_pipeline->rasterization.polygon_mode = zest_polygon_mode_fill;
    imgui_pipeline->rasterization.cull_mode = zest_cull_mode_none;
    imgui_pipeline->rasterization.front_face = zest_front_face_counter_clockwise;
    zest_SetPipelineTopology(imgui_pipeline, zest_topology_triangle_list);

    imgui_pipeline->color_blend_attachment = zest_ImGuiBlendState();
    zest_SetPipelineDepthTest(imgui_pipeline, ZEST_FALSE, ZEST_FALSE);
    ZEST_APPEND_LOG(context->device->log_path.str, "ImGui pipeline");

    io.Fonts->SetTexID((ImTextureID)ZestImGui->font_region);

    ZestImGui->pipeline = imgui_pipeline;

	zest_buffer_info_t vertex_info = zest_CreateBufferInfo(zest_buffer_type_vertex, zest_memory_usage_gpu_only);
	zest_buffer_info_t index_info = zest_CreateBufferInfo(zest_buffer_type_index, zest_memory_usage_gpu_only);
	vertex_info.unique_id = 0xDEA41;
	index_info.unique_id = 0xDEA41;

    zest_ForEachFrameInFlight(fif) {
		vertex_info.frame_in_flight = fif;
		index_info.frame_in_flight = fif;
        ZestImGui->vertex_device_buffer[fif] = zest_CreateBuffer(context, 1024 * 1024, &vertex_info);
        ZestImGui->index_device_buffer[fif] = zest_CreateBuffer(context, 1024 * 1024, &index_info);
    }

    return ZestImGui;
}

//Dear ImGui helper functions
void zest_imgui_RebuildFontTexture(zest_uint width, zest_uint height, unsigned char *pixels) {
    zest_WaitForIdleDevice(ZestImGui->context);
    int upload_size = width * height * 4 * sizeof(char);
    zest_bitmap font_bitmap = zest_CreateBitmapFromRawBuffer(ZestImGui->context, "font_bitmap", pixels, upload_size, width, height, 4);
    zest_FreeImage(ZestImGui->font_texture);
	zest_image_info_t image_info = zest_CreateImageInfo(width, height);
    image_info.flags = zest_image_preset_texture;
    ZestImGui->font_texture = zest_CreateImage(ZestImGui->context, &image_info);
    zest_FreeAtlasRegion(ZestImGui->font_region);
    ZestImGui->font_region = zest_CreateAtlasRegion(ZestImGui->font_texture);
    zest_cmd_CopyBitmapToImage(font_bitmap, ZestImGui->font_texture, 0, 0, 0, 0, width, height);
    ZestImGui->font_texture_binding_index = zest_AcquireGlobalSampledImageIndex(ZestImGui->font_texture, zest_texture_2d_binding);
    zest_FreeBitmap(font_bitmap);
    
    ImGuiIO &io = ImGui::GetIO();
    io.Fonts->SetTexID((ImTextureID)ZestImGui->font_region);
}

bool zest_imgui_HasGuiToDraw() {
    ImDrawData *imgui_draw_data = ImGui::GetDrawData();
    return (imgui_draw_data && imgui_draw_data->TotalVtxCount > 0 && imgui_draw_data->TotalIdxCount > 0);
}

zest_pass_node zest_imgui_BeginPass() {
    ImDrawData *imgui_draw_data = ImGui::GetDrawData();
    
    if (imgui_draw_data && imgui_draw_data->TotalVtxCount > 0 && imgui_draw_data->TotalIdxCount > 0) {
        //Declare resources
        zest_resource_node imgui_font_texture = zest_ImportImageResource("Imgui Font", ZestImGui->font_texture, 0);
		zest_resource_node imgui_vertex_buffer = zest_imgui_ImportVertexResources("Imgui Vertex Buffer");
		zest_resource_node imgui_index_buffer = zest_imgui_ImportIndexResources("Imgui Index Buffer");
        //Transfer Pass
        zest_BeginTransferPass("Upload ImGui"); {
            zest_ConnectOutput(imgui_vertex_buffer);
            zest_ConnectOutput(imgui_index_buffer);
            //task
            zest_SetPassTask(zest_imgui_UploadImGuiPass, ZestImGui);
            zest_EndPass();
        }
        //Graphics Pass for ImGui outputting to the output passed in to this function
		zest_pass_node imgui_pass = zest_BeginRenderPass("Dear ImGui Pass");
        //inputs
		zest_ConnectInput(imgui_font_texture);
        zest_ConnectInput(imgui_vertex_buffer);
		zest_ConnectInput(imgui_index_buffer);
        //Task
		zest_SetPassTask(zest_imgui_DrawImGuiRenderPass, NULL);
        return imgui_pass;
    }
    return 0;
}

// This is the function that will be called for your pass.
void zest_imgui_DrawImGuiRenderPass(const zest_command_list command_list, void *user_data) {
    zest_buffer vertex_buffer = zest_GetPassInputBuffer(command_list, "Imgui Vertex Buffer");
    zest_buffer index_buffer = zest_GetPassInputBuffer(command_list, "Imgui Index Buffer");
    zest_imgui_RecordLayer(command_list, vertex_buffer, index_buffer);
}

// This is the function that will be called for your pass.
void zest_imgui_UploadImGuiPass(const zest_command_list command_list, void *user_data) {

    if (!ZestImGui->dirty[ZestImGui->fif]) {
        return;
    }

    zest_buffer staging_vertex = ZestImGui->vertex_staging_buffer[ZestImGui->fif];
    zest_buffer staging_index = ZestImGui->index_staging_buffer[ZestImGui->fif];
    zest_buffer vertex_buffer = zest_GetPassOutputBuffer(command_list, "Imgui Vertex Buffer");
    zest_buffer index_buffer = zest_GetPassOutputBuffer(command_list, "Imgui Index Buffer");

    zest_buffer_uploader_t index_upload = { 0, staging_index, index_buffer, 0 };
    zest_buffer_uploader_t vertex_upload = { 0, staging_vertex, vertex_buffer, 0 };

    if (zest_BufferMemoryInUse(staging_vertex) && zest_BufferMemoryInUse(staging_index)) {
        zest_AddCopyCommand(&vertex_upload, staging_vertex, vertex_buffer, 0);
        zest_AddCopyCommand(&index_upload, staging_index, index_buffer, 0);
    }

    zest_uint vertex_size = zest_vec_size(vertex_upload.buffer_copies);

    zest_cmd_UploadBuffer(command_list, &vertex_upload);
    zest_cmd_UploadBuffer(command_list, &index_upload);

    ZestImGui->dirty[ZestImGui->fif] = 0;
}

void zest_imgui_RecordLayer(const zest_command_list command_list, zest_buffer vertex_buffer, zest_buffer index_buffer) {
    ImDrawData *imgui_draw_data = ImGui::GetDrawData();

    zest_cmd_BindVertexBuffer(command_list, 0, 1, vertex_buffer);
    zest_cmd_BindIndexBuffer(command_list, index_buffer);

    zest_pipeline_template last_pipeline = ZEST_NULL;
    zest_shader_resources_handle last_resources = { 0 };

    zest_cmd_SetScreenSizedViewport(command_list, 0, 1);

	zest_context context = command_list->context;
    
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

                zest_atlas_region current_image = (zest_atlas_region)pcmd->TextureId;
                if (!current_image) {
                    //This means we're trying to draw a render target
                    assert(pcmd->UserCallbackData);
                    ZEST_ASSERT(0); //Needs reimplementing
                }

                zest_imgui_push_t *push_constants = &ZestImGui->push_constants;

				zest_pipeline pipeline = zest_PipelineWithTemplate(ZestImGui->pipeline, command_list);
                switch (ZEST_STRUCT_TYPE(current_image)) {
                case zest_struct_type_atlas_region: {
                    if (last_pipeline != ZestImGui->pipeline || last_resources.value != ZestImGui->font_resources.value) {
                        last_resources = ZestImGui->font_resources;
                        zest_cmd_BindPipelineShaderResource(command_list, pipeline, last_resources);
                        last_pipeline = ZestImGui->pipeline;
                    }
                    push_constants->font_texture_index = ZestImGui->font_texture_binding_index;
                    push_constants->font_sampler_index = ZestImGui->font_sampler_binding_index;
                    push_constants->image_layer = zest_ImageLayerIndex(current_image);
                    break;
                }
                break;
                default:
                    //Invalid image
					pipeline = zest_PipelineWithTemplate(ZestImGui->pipeline, command_list);
                    ZEST_PRINT_WARNING("%s", "Invalid image found when trying to draw an imgui image. This is usually caused when a texture is changed in another thread before drawing is complete causing the image handle to become invalid due to it being freed.");
                    continue;
                }

                zest_cmd_SendPushConstants(command_list, pipeline, push_constants);

                ImVec2 clip_min((pcmd->ClipRect.x - clip_off.x) * clip_scale.x, (pcmd->ClipRect.y - clip_off.y) * clip_scale.y);
                ImVec2 clip_max((pcmd->ClipRect.z - clip_off.x) * clip_scale.x, (pcmd->ClipRect.w - clip_off.y) * clip_scale.y);

                // Clamp to viewport as vkCmdSetScissor() won't accept values that are off bounds
                if (clip_max.x > zest_ScreenWidth(context)) { clip_max.x = zest_ScreenWidthf(context); }
                if (clip_max.y > zest_ScreenHeight(context)) { clip_max.y = zest_ScreenHeightf(context); }
                if (clip_max.x <= clip_min.x || clip_max.y <= clip_min.y) {
                    continue;
                }

                zest_scissor_rect_t scissor_rect;
                scissor_rect.offset.x = ZEST__MAX((int32_t)(clip_min.x), 0);
                scissor_rect.offset.y = ZEST__MAX((int32_t)(clip_min.y), 0);
                scissor_rect.extent.width = (zest_uint)(clip_max.x - clip_min.x);
                scissor_rect.extent.height = (zest_uint)(clip_max.y - clip_min.y);

                zest_cmd_Scissor(command_list, &scissor_rect);
                zest_cmd_DrawIndexed(command_list, pcmd->ElemCount, 1, index_offset + pcmd->IdxOffset, vertex_offset, 0);
            }
            index_offset += cmd_list->IdxBuffer.Size;
            vertex_offset += cmd_list->VtxBuffer.Size;
        }
    }
    zest_scissor_rect_t scissor = { { 0, 0 }, { zest_ScreenWidth(context), zest_ScreenHeight(context) } };
	zest_cmd_Scissor(command_list, &scissor);
}

zest_buffer zest_imgui_VertexBufferProvider(zest_resource_node resource) {
    return ZestImGui->vertex_device_buffer[ZestImGui->fif];
}

zest_buffer zest_imgui_IndexBufferProvider(zest_resource_node resource) {
    return ZestImGui->index_device_buffer[ZestImGui->fif];
}

zest_resource_node zest_imgui_ImportVertexResources(const char *name) {
    ImDrawData *imgui_draw_data = ImGui::GetDrawData();
    if (imgui_draw_data) {
		zest_resource_node node = zest_ImportBufferResource(name, ZestImGui->vertex_device_buffer[ZestImGui->fif], zest_imgui_VertexBufferProvider);
        return node;
    }
    return NULL;
}

zest_resource_node zest_imgui_ImportIndexResources(const char *name) {
    ImDrawData *imgui_draw_data = ImGui::GetDrawData();
    if (imgui_draw_data) {
		zest_resource_node node = zest_ImportBufferResource(name, ZestImGui->index_device_buffer[ZestImGui->fif], zest_imgui_IndexBufferProvider);
        return node;
    }
    return NULL;
}

void zest_imgui_UpdateBuffers() {
    ImDrawData *imgui_draw_data = ImGui::GetDrawData();

	ZestImGui->fif = (ZestImGui->fif + 1) % ZEST_MAX_FIF;
	zest_FreeBuffer(ZestImGui->vertex_staging_buffer[ZestImGui->fif]);
	zest_FreeBuffer(ZestImGui->index_staging_buffer[ZestImGui->fif]);

	zest_context context = ZestImGui->context;

    if (imgui_draw_data) {
        ZestImGui->dirty[ZestImGui->fif] = 1;
		zest_size index_memory_in_use = imgui_draw_data->TotalIdxCount * sizeof(ImDrawIdx);
		zest_size vertex_memory_in_use = imgui_draw_data->TotalVtxCount * sizeof(ImDrawVert);
		ZestImGui->vertex_staging_buffer[ZestImGui->fif] = zest_CreateStagingBuffer(context, vertex_memory_in_use, 0);
		ZestImGui->index_staging_buffer[ZestImGui->fif] = zest_CreateStagingBuffer(context, index_memory_in_use, 0);
		zest_buffer vertex_buffer = ZestImGui->vertex_staging_buffer[ZestImGui->fif];
		zest_buffer index_buffer = ZestImGui->index_staging_buffer[ZestImGui->fif];
        zest_SetBufferMemoryInUse(vertex_buffer, vertex_memory_in_use);
        zest_SetBufferMemoryInUse(index_buffer, index_memory_in_use);
		zest_buffer device_vertex_buffer = ZestImGui->vertex_device_buffer[ZestImGui->fif];
		zest_buffer device_index_buffer = ZestImGui->index_device_buffer[ZestImGui->fif];
        ZEST_ASSERT(vertex_buffer); //Make sure you call zest_imgui_Initialise first!
        ZEST_ASSERT(index_buffer);	//Make sure you call zest_imgui_Initialise first!


		ZestImGui->push_constants.transform = zest_Vec4Set(2.0f / zest_ScreenWidthf(context), 2.0f / zest_ScreenHeightf(context), -1.f, -1.f);

        if (index_memory_in_use > zest_BufferSize(device_index_buffer)) {
            if (zest_GrowBuffer(&device_index_buffer, sizeof(ImDrawIdx), zest_BufferMemoryInUse(index_buffer))) {
				ZestImGui->index_device_buffer[ZestImGui->fif] = device_index_buffer;
            }
        }
        if (vertex_memory_in_use > zest_BufferSize(device_vertex_buffer)) {
            if (zest_GrowBuffer(&device_vertex_buffer, sizeof(ImDrawVert), zest_BufferMemoryInUse(vertex_buffer))) {
				ZestImGui->vertex_device_buffer[ZestImGui->fif] = device_vertex_buffer;
            }
        }
        ImDrawIdx *idxDst = (ImDrawIdx *)zest_BufferData(index_buffer);
        ImDrawVert *vtxDst = (ImDrawVert *)zest_BufferData(vertex_buffer);

        for (int n = 0; n < imgui_draw_data->CmdListsCount; n++) {
            const ImDrawList *cmd_list = imgui_draw_data->CmdLists[n];
            memcpy(vtxDst, cmd_list->VtxBuffer.Data, cmd_list->VtxBuffer.Size * sizeof(ImDrawVert));
            memcpy(idxDst, cmd_list->IdxBuffer.Data, cmd_list->IdxBuffer.Size * sizeof(ImDrawIdx));
            vtxDst += cmd_list->VtxBuffer.Size;
            idxDst += cmd_list->IdxBuffer.Size;
        }
    }
}

void zest_imgui_DrawImage(zest_atlas_region image, float width, float height) {
    using namespace ImGui;
    zest_extent2d_t image_extent = zest_RegionDimensions(image);
    ImVec2 image_size((float)image_extent.width, (float)image_extent.height);
    float ratio = image_size.x / image_size.y;
    image_size.x = ratio > 1 ? width : width * ratio;
    image_size.y = ratio > 1 ? height / ratio : height;
    ImVec2 image_offset((width - image_size.x) * .5f, (height - image_size.y) * .5f);
    ImGuiWindow *window = GetCurrentWindow();
    const ImRect image_bb(window->DC.CursorPos + image_offset, window->DC.CursorPos + image_offset + image_size);
    ImVec4 tint_col(1.f, 1.f, 1.f, 1.f);
    zest_vec4 uv = zest_ImageUV(image);
    window->DrawList->AddImage((ImTextureID)image, image_bb.Min, image_bb.Max, ImVec2(uv.x, uv.y), ImVec2(uv.z, uv.w), GetColorU32(tint_col));
}

void zest_imgui_DrawImage2(zest_atlas_region image, float width, float height) {
    zest_vec4 uv = zest_ImageUV(image);
    ImGui::Image((ImTextureID)image, ImVec2(width, height), ImVec2(uv.x, uv.y), ImVec2(uv.z, uv.w));
}

void zest_imgui_DrawTexturedRect(zest_atlas_region image, float width, float height, bool tile, float scale_x, float scale_y, float offset_x, float offset_y) {
    /*
    zest_texture_handle texture_handle = zest_ImageTextureHandle(image);
    zest_vec4 uv = zest_ImageUV(image);
    zest_extent2d_t image_size = zest_RegionDimensions(image);
    ImVec2 zw(uv.z, uv.w);
    if (zest_TextureCanTile(texture_handle)) {
        if (tile) {
            if (offset_x || offset_y) {
                offset_x = offset_x / float(image_size.width);
                offset_y = offset_y / float(image_size.height);
                offset_x *= scale_x;
                offset_y *= scale_y;
            }
            scale_x *= width / float(image_size.width);
            scale_y *= height / float(image_size.height);
            zw.x = (uv.z * scale_x) - offset_x;
            zw.y = (uv.w * scale_y) - offset_y;
            uv.x -= offset_x;
            uv.y -= offset_y;
        }
    }

    ImGui::Image((ImTextureID)image, ImVec2(width, height), ImVec2(uv.x, uv.y), zw);
    */
}

bool zest_imgui_DrawButton(zest_atlas_region image, const char *user_texture_id, float width, float height, int frame_padding) {
    using namespace ImGui;

    zest_vec4 uv = zest_ImageUV(image);
    zest_extent2d_t image_dimensions = zest_RegionDimensions(image);

    ImVec2 size(width, height);
    ImVec2 image_size((float)image_dimensions.width, (float)image_dimensions.height);
    float ratio = (float)image_dimensions.width / (float)image_dimensions.height;
    image_dimensions.width = (zest_uint)(ratio > 1.f ? width : width * ratio);
    image_dimensions.height = (zest_uint)(ratio > 1.f ? height / ratio : height);
    ImVec2 image_offset((width - image_dimensions.width) * .5f, (height - image_dimensions.height) * .5f);
    ImVec4 bg_col(0.f, 0.f, 0.f, 0.f);
    ImVec4 tint_col(1.f, 1.f, 1.f, 1.f);
    ImVec2 uv0(uv.x, uv.y);
    ImVec2 uv1(uv.z, uv.w);

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

void zest_imgui_Shutdown() {
	ImGui::DestroyContext();
	zest_ForEachFrameInFlight(fif) {
		zest_FreeBuffer(ZestImGui->index_device_buffer[fif]);
		zest_FreeBuffer(ZestImGui->vertex_device_buffer[fif]);
		zest_FreeBuffer(ZestImGui->index_staging_buffer[fif]);
		zest_FreeBuffer(ZestImGui->vertex_staging_buffer[fif]);
	}
	zest_FreeImage(ZestImGui->font_texture);
	zest_FreePipelineTemplate(ZestImGui->pipeline);
    zest_FreeAtlasRegion(ZestImGui->font_region);
    zest_FreeMemory(ZestImGui->context, ZestImGui);
}
//--End Dear ImGui helper functions
