#include "impl_imgui.h"
#include "imgui_internal.h"

void zest_imgui_Initialise(zest_context context, zest_imgui_t *imgui, zest_destroy_window_callback destroy_window_callback) {
	*imgui = {};
	imgui->context = context;
	imgui->device = zest_GetContextDevice(context);
    imgui->magic = zest_INIT_MAGIC(zest_struct_type_imgui);
    imgui->imgui_context = ImGui::CreateContext();
	imgui->destroy_window_callback = destroy_window_callback;
	ImGui::SetCurrentContext(imgui->imgui_context);
    ImGuiIO &io = ImGui::GetIO();
	io.UserData = imgui;
	io.ConfigFlags |= ImGuiConfigFlags_DockingEnable | ImGuiConfigFlags_ViewportsEnable;
    io.DisplaySize = ImVec2(zest_ScreenWidthf(context), zest_ScreenHeightf(context));
	float dpi_scale = zest_DPIScale(context);
    io.DisplayFramebufferScale = ImVec2(dpi_scale, dpi_scale);
    unsigned char *pixels;
    int width, height;
    io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
    int upload_size = width * height * 4 * sizeof(char);

	zest_device device = zest_GetContextDevice(context);

	zest_image_info_t image_info = zest_CreateImageInfo(width, height);
    image_info.flags = zest_image_preset_texture;
    imgui->font_texture = zest_CreateImage(device, &image_info);
	imgui->font_region = {};
	zest_image font_image = zest_GetImage(imgui->font_texture);
    zest_CopyBitmapToImage(device, pixels, upload_size, font_image, width, height);

    zest_sampler_info_t sampler_info = zest_CreateSamplerInfo();
    imgui->font_sampler = zest_CreateSampler(context, &sampler_info);
	zest_sampler font_sampler = zest_GetSampler(imgui->font_sampler);
    imgui->font_texture_binding_index = zest_AcquireSampledImageIndex(device, font_image, zest_texture_2d_binding);
    imgui->font_sampler_binding_index = zest_AcquireSamplerIndex(device, font_sampler);
	zest_BindAtlasRegionToImage(&imgui->font_region, imgui->font_sampler_binding_index, font_image, zest_texture_2d_binding);
    io.Fonts->SetTexID((ImTextureID)&imgui->font_region);
    zest_atlas_region_t *test = &imgui->font_region;

    //imgui->vertex_shader = zest_CreateShaderSPVMemory(imgui->spv", shaderc_vertex_shader);
    //imgui->fragment_shader = zest_CreateShaderSPVMemory(imgui->spv", shaderc_fragment_shader);

	imgui->vertex_shader = zest_CreateShader(zest_GetContextDevice(imgui->context), zest_shader_imgui_vert, zest_vertex_shader, "imgui_vert", ZEST_TRUE);
	imgui->fragment_shader = zest_CreateShader(zest_GetContextDevice(imgui->context), zest_shader_imgui_frag, zest_fragment_shader, "imgui_frag", ZEST_TRUE);

	zest_pipeline_layout_info_t layout_info = zest_NewPipelineLayoutInfoWithGlobalBindless(device);
	imgui->pipeline_layout = zest_CreatePipelineLayout(&layout_info);

    //ImGuiPipeline
    zest_pipeline_template imgui_pipeline = zest_CreatePipelineTemplate(zest_GetContextDevice(imgui->context), "pipeline_imgui");
    zest_AddVertexInputBindingDescription(imgui_pipeline, 0, sizeof(zest_ImDrawVert_t), zest_input_rate_vertex);
    zest_AddVertexAttribute(imgui_pipeline, 0, 0, zest_format_r32g32_sfloat, offsetof(zest_ImDrawVert_t, pos));    // Location 0: Position
    zest_AddVertexAttribute(imgui_pipeline, 0, 1, zest_format_r32g32_sfloat, offsetof(zest_ImDrawVert_t, uv));    // Location 1: UV
    zest_AddVertexAttribute(imgui_pipeline, 0, 2, zest_format_r8g8b8a8_unorm, offsetof(zest_ImDrawVert_t, col));    // Location 2: Color
	zest_SetPipelineLayout(imgui_pipeline, imgui->pipeline_layout);
    zest_SetPipelineShaders(imgui_pipeline, imgui->vertex_shader, imgui->fragment_shader);
	zest_SetPipelineFrontFace(imgui_pipeline, zest_front_face_counter_clockwise);
	zest_SetPipelineCullMode(imgui_pipeline, zest_cull_mode_none);
    zest_SetPipelineTopology(imgui_pipeline, zest_topology_triangle_list);
	zest_SetPipelineBlend(imgui_pipeline, zest_ImGuiBlendState());
    zest_SetPipelineDepthTest(imgui_pipeline, ZEST_FALSE, ZEST_FALSE);
    ZEST_APPEND_LOG(zest_GetErrorLogPath(device), "ImGui pipeline");

    imgui->pipeline = imgui_pipeline;

	ImGuiPlatformIO& platform_io = ImGui::GetPlatformIO();
	platform_io.Renderer_CreateWindow = zest__imgui_create_viewport;
	platform_io.Renderer_DestroyWindow = zest__imgui_destroy_viewport;
	platform_io.Renderer_RenderWindow = zest__imgui_render_viewport;

	zest_imgui_viewport_t* viewport = zest_imgui_AcquireViewport(device);
	viewport->context = context;
	viewport->imgui = imgui;

	ImGuiViewport* main_viewport = ImGui::GetMainViewport();
	viewport->imgui_viewport = main_viewport;
	main_viewport->RendererUserData = viewport;
	imgui->main_viewport = viewport;
}

//Dear ImGui helper functions
void zest_imgui_RebuildFontTexture(zest_imgui_t *imgui, zest_uint width, zest_uint height, unsigned char *pixels) {
    int upload_size = width * height * 4 * sizeof(char);
    zest_FreeImage(imgui->font_texture);
	zest_image_info_t image_info = zest_CreateImageInfo(width, height);
    image_info.flags = zest_image_preset_texture;
    imgui->font_texture = zest_CreateImage(imgui->device, &image_info);
	zest_image font_image = zest_GetImage(imgui->font_texture);
	imgui->font_region = {};
    zest_CopyBitmapToImage(imgui->device, pixels, upload_size, font_image, width, height);
    imgui->font_texture_binding_index = zest_AcquireSampledImageIndex(imgui->device, font_image, zest_texture_2d_binding);
	zest_BindAtlasRegionToImage(&imgui->font_region, imgui->font_sampler_binding_index, font_image, zest_texture_2d_binding);
    
    ImGuiIO &io = ImGui::GetIO();
    io.Fonts->SetTexID((ImTextureID)&imgui->font_region);
}

bool zest_imgui_HasGuiToDraw(zest_imgui_t *imgui) {
	ImGui::SetCurrentContext(imgui->imgui_context);
    ImDrawData *imgui_draw_data = ImGui::GetDrawData();
    return (imgui_draw_data && imgui_draw_data->TotalVtxCount > 0 && imgui_draw_data->TotalIdxCount > 0);
}

void zest_imgui_GetWindowSizeCallback(zest_window_data_t *window_data, int *fb_width, int *fb_height, int *window_width, int *window_height) {
	ImGuiViewport *viewport = (ImGuiViewport *)window_data->user_data;
	*fb_width = (int)viewport->Size.x;
	*fb_height = (int)viewport->Size.y;
	*window_width = (int)viewport->Size.x;
	*window_height = (int)viewport->Size.y;
}

zest_pass_node zest_imgui_BeginPass(zest_imgui_t *imgui, zest_imgui_viewport_t *viewport) {
    ImDrawData *imgui_draw_data = viewport->imgui_viewport->DrawData;
    ImDrawData *all_draw_data =  ImGui::GetDrawData();
    if (imgui_draw_data && imgui_draw_data->TotalVtxCount > 0 && imgui_draw_data->TotalIdxCount > 0) {
        //Declare resources
		zest_image font_image = zest_GetImage(imgui->font_texture);
        zest_resource_node imgui_font_texture = zest_ImportImageResource("Imgui Font", font_image, 0);
		zest_resource_node imgui_vertex_buffer = zest_imgui_AddVertexResources(imgui_draw_data, "Viewport Vertex Buffer");
		zest_resource_node imgui_index_buffer = zest_imgui_AddIndexResources(imgui_draw_data, "Viewport Index Buffer");
        //Transfer Pass
        zest_BeginTransferPass("Upload ImGui Viewport"); {
            zest_ConnectOutput(imgui_vertex_buffer);
            zest_ConnectOutput(imgui_index_buffer);
            //Task
            zest_SetPassTask(zest_imgui_UploadImGuiPass, viewport);
            zest_EndPass();
        }
        //Graphics Pass for ImGui outputting to the output passed in to this function
		zest_pass_node imgui_pass = zest_BeginRenderPass("Dear ImGui Viewport Pass");
        //inputs
		zest_ConnectInput(imgui_font_texture);
        zest_ConnectInput(imgui_vertex_buffer);
		zest_ConnectInput(imgui_index_buffer);
        //Task
		zest_SetPassTask(zest_imgui_DrawImGuiViewportRenderPass, viewport);
        return imgui_pass;
    }
    return 0;
}

// This is the function that will be called for each viewport if you're using multi viewport.
void zest_imgui_DrawImGuiViewportRenderPass(const zest_command_list command_list, void *user_data) {
	zest_imgui_viewport_t *imgui_viewport = (zest_imgui_viewport_t*)user_data;
    zest_buffer vertex_buffer = zest_GetPassInputBuffer(command_list, "Viewport Vertex Buffer");
    zest_buffer index_buffer = zest_GetPassInputBuffer(command_list, "Viewport Index Buffer");
    zest_imgui_RecordViewport(command_list, imgui_viewport, vertex_buffer, index_buffer);
}

// This is the function that will be called for your pass.
void zest_imgui_UploadImGuiPass(const zest_command_list command_list, void *user_data) {
	zest_imgui_viewport_t *viewport = (zest_imgui_viewport_t*)user_data;

    ImDrawData *imgui_draw_data = viewport->imgui_viewport->DrawData;
	zest_imgui_t *imgui = viewport->imgui;

	zest_context context = zest_GetContext(command_list);

	zest_uint fif = zest_CurrentFIF(context);

	zest_FreeBuffer(viewport->index_staging_buffer[fif]);
	zest_FreeBuffer(viewport->vertex_staging_buffer[fif]);

    if (imgui_draw_data) {
		zest_size index_memory_in_use = imgui_draw_data->TotalIdxCount * sizeof(ImDrawIdx);
		zest_size vertex_memory_in_use = imgui_draw_data->TotalVtxCount * sizeof(ImDrawVert);
		zest_buffer staging_index_buffer = zest_CreateStagingBuffer(imgui->device, index_memory_in_use, 0);
		zest_buffer staging_vertex_buffer = zest_CreateStagingBuffer(imgui->device, vertex_memory_in_use, 0);
		viewport->index_staging_buffer[fif] = staging_index_buffer;
		viewport->vertex_staging_buffer[fif] = staging_vertex_buffer;
		zest_buffer index_buffer = zest_GetPassOutputBuffer(command_list, "Viewport Index Buffer");
		zest_buffer vertex_buffer = zest_GetPassOutputBuffer(command_list, "Viewport Vertex Buffer");

		zest_vec2 scale = { 2.0f / imgui_draw_data->DisplaySize.x, 2.0f / imgui_draw_data->DisplaySize.y };
		zest_vec2 translate = { -1.0f - imgui_draw_data->DisplayPos.x * scale.x, -1.0f - imgui_draw_data->DisplayPos.y * scale.y };

		viewport->push_constants.transform = zest_Vec4Set(scale.x, scale.y, translate.x, translate.y);

        ImDrawIdx *idxDst = (ImDrawIdx *)zest_BufferData(staging_index_buffer);
        ImDrawVert *vtxDst = (ImDrawVert *)zest_BufferData(staging_vertex_buffer);

        for (int n = 0; n < imgui_draw_data->CmdListsCount; n++) {
            const ImDrawList *cmd_list = imgui_draw_data->CmdLists[n];
            memcpy(vtxDst, cmd_list->VtxBuffer.Data, cmd_list->VtxBuffer.Size * sizeof(ImDrawVert));
            memcpy(idxDst, cmd_list->IdxBuffer.Data, cmd_list->IdxBuffer.Size * sizeof(ImDrawIdx));
            vtxDst += cmd_list->VtxBuffer.Size;
            idxDst += cmd_list->IdxBuffer.Size;
        }

		zest_buffer_uploader_t index_upload = { 0, staging_index_buffer, index_buffer, 0 };
		zest_buffer_uploader_t vertex_upload = { 0, staging_vertex_buffer, vertex_buffer, 0 };

		if (vertex_memory_in_use && index_memory_in_use) {
			zest_AddCopyCommand(context, &vertex_upload, staging_vertex_buffer, vertex_buffer, vertex_memory_in_use);
			zest_AddCopyCommand(context, &index_upload, staging_index_buffer, index_buffer, index_memory_in_use);
		}

		zest_uint vertex_size = zest_vec_size(vertex_upload.buffer_copies);

		zest_cmd_UploadBuffer(command_list, &vertex_upload);
		zest_cmd_UploadBuffer(command_list, &index_upload);
    }
}

zest_imgui_viewport_t *zest_imgui_AcquireViewport(zest_device device) {
	ZEST_ASSERT_HANDLE(device);	//Not a valid device handle!
	zest_imgui_viewport_t *viewport = (zest_imgui_viewport_t*)zest_AllocateMemory(device, sizeof(zest_imgui_viewport_t));
	*viewport = ZEST__ZERO_INIT(zest_imgui_viewport_t);
	return viewport;
}

void zest_imgui_FreeViewport(zest_device device, zest_imgui_viewport_t *viewport) {
	ZEST_ASSERT_HANDLE(device);	//Not a valid device handle!
	zest_FreeMemory(device, viewport);
}

void zest_imgui_RecordViewport(const zest_command_list command_list, zest_imgui_viewport_t *imgui_viewport, zest_buffer vertex_buffer, zest_buffer index_buffer) {
	ImGui::SetCurrentContext(imgui_viewport->imgui->imgui_context);
    ImDrawData *imgui_draw_data = imgui_viewport->imgui_viewport->DrawData;

    zest_cmd_BindVertexBuffer(command_list, 0, 1, vertex_buffer);
    zest_cmd_BindIndexBuffer(command_list, index_buffer);

	zest_imgui_render_state_t render_state = {};

    zest_cmd_SetScreenSizedViewport(command_list, 0, 1);

	zest_context context = zest_GetContext(command_list);
    
    if (imgui_draw_data && imgui_draw_data->CmdListsCount > 0) {

        int32_t vertex_offset = 0;
        int32_t index_offset = 0;

        ImVec2 clip_off = imgui_draw_data->DisplayPos;
        ImVec2 clip_scale = imgui_draw_data->FramebufferScale;

        for (int32_t i = 0; i < imgui_draw_data->CmdListsCount; i++)
        {
            ImDrawList *cmd_list = imgui_draw_data->CmdLists[i];
            for (int32_t j = 0; j < cmd_list->CmdBuffer.Size; j++)
            {
                ImDrawCmd *pcmd = &cmd_list->CmdBuffer[j];

                zest_atlas_region_t *current_image = (zest_atlas_region_t*)pcmd->TextureId;
                if (!current_image) {
                    //This means we're trying to draw a render target
                    assert(pcmd->UserCallbackData);
                    ZEST_ASSERT(0); //Needs reimplementing
                }

                zest_imgui_push_t *push_constants = &imgui_viewport->push_constants;

				if (pcmd->UserCallback) {
					void* original_callback_data = pcmd->UserCallbackData;
					zest_imgui_callback_data_t data_wrapper = {
						original_callback_data,
						command_list,
						&render_state
					};
					pcmd->UserCallbackData = &data_wrapper;
					pcmd->UserCallback(cmd_list, pcmd);
					pcmd->UserCallbackData = original_callback_data;
					continue;
				}

				if (current_image == &imgui_viewport->imgui->font_region) {
					zest_pipeline pipeline = zest_GetPipeline(imgui_viewport->imgui->pipeline, command_list);
					if (render_state.pipeline != pipeline) {
						render_state.pipeline = pipeline;
						zest_cmd_BindPipeline(command_list, render_state.pipeline);
					}
				} else {
					ZEST_ASSERT(render_state.pipeline, "If the current atlas region is NOT the imgui font image then render state must have been set via a callback.");
					zest_cmd_BindPipeline(command_list, render_state.pipeline);
				}
				push_constants->font_texture_index = current_image->image_index;
				push_constants->font_sampler_index = current_image->sampler_index;
				push_constants->image_layer = zest_RegionLayerIndex(current_image);

                zest_cmd_SendPushConstants(command_list, push_constants, sizeof(zest_imgui_push_t));

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

void zest_imgui_RenderViewport(zest_imgui_viewport_t *viewport) {
	zest__imgui_render_viewport(viewport->imgui_viewport, viewport->imgui);
}

void zest__imgui_create_viewport(ImGuiViewport* viewport) {
	zest_imgui_viewport_t* app_viewport = (zest_imgui_viewport_t*)viewport->RendererUserData;
    ImGuiIO &io = ImGui::GetIO();
	zest_imgui_t *imgui = (zest_imgui_t*)io.UserData;
	zest_device device = zest_GetContextDevice(imgui->context);
	if (!app_viewport) {
		app_viewport = zest_imgui_AcquireViewport(zest_GetContextDevice(imgui->context)); 
		viewport->RendererUserData = app_viewport;
	}

	// Create Zest context
	zest_create_context_info_t create_info = zest_CreateContextInfo();
	zest_window_data_t window_handles = { };  
	window_handles.window_handle = viewport->PlatformHandle;
	window_handles.native_handle = viewport->PlatformHandleRaw;
#ifdef _WIN32
	window_handles.display = GetModuleHandle(NULL);
#endif
	window_handles.width = (int)viewport->Size.x;
	window_handles.height = (int)viewport->Size.y;
	window_handles.window_sizes_callback = zest_imgui_GetWindowSizeCallback;
	window_handles.user_data = viewport;
	app_viewport->context = zest_CreateContext(device, &window_handles, &create_info);
	app_viewport->imgui = imgui;
	app_viewport->imgui_viewport = viewport;
}

void zest__imgui_render_viewport(ImGuiViewport* vp, void* render_arg) {
	zest_imgui_t *imgui = (zest_imgui_t*)render_arg;
	zest_imgui_viewport_t* viewport = (zest_imgui_viewport_t*)vp->RendererUserData;

	zest_frame_graph_cache_key_t cache_key = {};
	cache_key = zest_InitialiseCacheKey(viewport->context, 0, 0);

	zest_device device = zest_GetContextDevice(viewport->context);

	if (zest_BeginFrame(viewport->context)) {
		zest_frame_graph frame_graph = zest_GetCachedFrameGraph(viewport->context, &cache_key);
		if (!frame_graph) {
			if (zest_BeginFrameGraph(viewport->context, "ImGui Viewport", 0)) {
				zest_ImportSwapchainResource();
				zest_pass_node imgui_pass = zest_imgui_BeginPass(imgui, viewport);
				if (imgui_pass) {
					zest_ConnectSwapChainOutput();
					zest_EndPass();
                } else {
                    zest_BeginRenderPass("Blank Viewport");
                    zest_ConnectSwapChainOutput();
                    zest_SetPassTask(zest_EmptyRenderPass, 0);
                    zest_EndPass();
                }
				frame_graph = zest_EndFrameGraph();
			}
		}
		zest_EndFrame(viewport->context, frame_graph);
	}
}

void zest__imgui_destroy_viewport(ImGuiViewport* viewport) {
	zest_imgui_viewport_t* app_viewport = (zest_imgui_viewport_t*)viewport->RendererUserData;
	if (!app_viewport) return;
	zest_context context = app_viewport->context;
	ZEST_ASSERT(app_viewport->imgui->destroy_window_callback, "You must set the destroy window callback that frees the window associated with the viewport. This might be zest_implglfw_DestroyWindow or zest_implsdl2_DestroyWindow if you're using zest_utilities.");
	app_viewport->imgui->destroy_window_callback(context);
	zest_imgui_FreeViewport(zest_GetContextDevice(context), app_viewport);
	zest_PrintReports(context);
	zest_DestroyContext(context);
	viewport->PlatformUserData = NULL;
	viewport->RendererUserData = NULL;
}

zest_buffer zest__imgui_get_vertex_buffer_size(zest_context context, zest_resource_node node) {
	ImDrawData *draw_data = (ImDrawData*)zest_GetResourceUserData(node);
	zest_SetResourceBufferSize(node, draw_data->TotalVtxCount * sizeof(ImDrawVert));
    return NULL;
}

zest_buffer zest__imgui_get_index_buffer_size(zest_context context, zest_resource_node node) {
	ImDrawData *draw_data = (ImDrawData*)zest_GetResourceUserData(node);
	zest_SetResourceBufferSize(node, draw_data->TotalIdxCount * sizeof(ImDrawIdx));
    return NULL;
}

zest_resource_node zest_imgui_AddVertexResources(ImDrawData *draw_data, const char *name) {
    if (draw_data) {
		zest_buffer_resource_info_t info = {};
		info.usage_hints = zest_resource_usage_hint_vertex_buffer;
		info.size = draw_data->TotalVtxCount * sizeof(ImDrawVert);
		zest_resource_node node = zest_AddTransientBufferResource(name, &info);
		zest_SetResourceUserData(node, draw_data);
		zest_SetResourceBufferProvider(node, zest__imgui_get_vertex_buffer_size);
        return node;
    }
    return NULL;
}

zest_resource_node zest_imgui_AddIndexResources(ImDrawData *draw_data, const char *name) {
    if (draw_data) {
		zest_buffer_resource_info_t info = {};
		info.usage_hints = zest_resource_usage_hint_index_buffer;
		info.size = draw_data->TotalIdxCount * sizeof(ImDrawIdx);
		zest_resource_node node = zest_AddTransientBufferResource(name, &info);
		zest_SetResourceUserData(node, draw_data);
		zest_SetResourceBufferProvider(node, zest__imgui_get_vertex_buffer_size);
        return node;
    }
    return NULL;
}

void zest_imgui_DrawImage(zest_atlas_region_t *region, float width, float height, ImDrawCallback callback, void *user_data) {
    using namespace ImGui;
    zest_extent2d_t image_extent = zest_RegionDimensions(region);
    ImVec2 image_size((float)image_extent.width, (float)image_extent.height);
    float ratio = image_size.x / image_size.y;
    image_size.x = ratio > 1 ? width : width * ratio;
    image_size.y = ratio > 1 ? height / ratio : height;
    ImVec2 image_offset((width - image_size.x) * .5f, (height - image_size.y) * .5f);
    ImGuiWindow *window = GetCurrentWindow();
	if (callback) {
		window->DrawList->AddCallback(callback, user_data, 0);
	}
    const ImRect image_bb(window->DC.CursorPos + image_offset, window->DC.CursorPos + image_offset + image_size);
    ImVec4 tint_col(1.f, 1.f, 1.f, 1.f);
    zest_vec4 uv = zest_RegionUV(region);
    window->DrawList->AddImage((ImTextureID)region, image_bb.Min, image_bb.Max, ImVec2(uv.x, uv.y), ImVec2(uv.z, uv.w), GetColorU32(tint_col));
}

void zest_imgui_DrawImage2(zest_atlas_region_t *region, float width, float height) {
    zest_vec4 uv = zest_RegionUV(region);
    ImGui::Image((ImTextureID)region, ImVec2(width, height), ImVec2(uv.x, uv.y), ImVec2(uv.z, uv.w));
}

void zest_imgui_DrawTexturedRect(zest_atlas_region_t *region, float width, float height, bool tile, float scale_x, float scale_y, float offset_x, float offset_y) {
    /*
    zest_texture_handle texture_handle = zest_ImageTextureHandle(image);
    zest_vec4 uv = zest_RegionUV(image);
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

bool zest_imgui_DrawButton(zest_atlas_region_t *region, const char *user_texture_id, float width, float height, int frame_padding) {
    using namespace ImGui;

    zest_vec4 uv = zest_RegionUV(region);
    zest_extent2d_t image_dimensions = zest_RegionDimensions(region);

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
    window->DrawList->AddImage((ImTextureID)region, image_bb.Min, image_bb.Max, uv0, uv1, GetColorU32(tint_col));

    return pressed;
}

void zest_imgui_DarkStyle(zest_imgui_t *imgui) {
	ImGui::SetCurrentContext(imgui->imgui_context);
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

void zest_imgui_Destroy(zest_imgui_t *imgui) {
	ImGui::DestroyContext();
	zest_FreeImage(imgui->font_texture);
	zest_FreePipelineTemplate(imgui->pipeline);
}
//--End Dear ImGui helper functions
