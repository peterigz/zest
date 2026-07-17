#include "impl_imgui.h"
#include "imgui_internal.h"
#include <math.h>

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
	zest_device device = zest_GetContextDevice(context);

	// ImGui 1.92+ manages all textures (including the font atlas) dynamically through the
	// texture request system. We advertise support for it and honour the create/update/destroy
	// requests in zest_imgui_UpdateTextures(). ImGui queues the font atlas for creation on the
	// first frame, so we no longer build the font texture manually here.
	io.BackendFlags |= ImGuiBackendFlags_RendererHasTextures;

	// All ImGui-managed textures share a single sampler.
	zest_sampler_info_t sampler_info = zest_CreateSamplerInfo();
	imgui->font_sampler = zest_CreateSampler(device, &sampler_info);
	zest_sampler font_sampler = zest_GetSampler(imgui->font_sampler);
	imgui->font_sampler_binding_index = zest_AcquireSamplerIndex(device, font_sampler);

    //imgui->vertex_shader = zest_CreateShaderFromBinary(imgui->spv", shaderc_vertex_shader);
    //imgui->fragment_shader = zest_CreateShaderFromBinary(imgui->spv", shaderc_fragment_shader);

	imgui->vertex_shader = zest_CreateShader(zest_GetContextDevice(imgui->context), zest_shader_imgui_vert, zest_vertex_shader, "imgui_vert", NULL, ZEST_TRUE);
	imgui->fragment_shader = zest_CreateShader(zest_GetContextDevice(imgui->context), zest_shader_imgui_frag, zest_fragment_shader, "imgui_frag", NULL, ZEST_TRUE);

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

// Per-texture backend data for an ImGui-managed texture (font atlas or any texture ImGui
// requests us to create). The atlas region is embedded so its address can be handed back to
// ImGui as the ImTextureID and cast directly to a zest_atlas_region_t* in the draw loop.
typedef struct zest_imgui_backend_texture_t {
    zest_image_handle image;
    zest_uint binding_index;
    zest_atlas_region_t region;
} zest_imgui_backend_texture_t;

// Create (or recreate) the GPU texture backing an ImGui ImTextureData request. Used for both
// ImTextureStatus_WantCreate and ImTextureStatus_WantUpdates: in either case we (re)upload the
// whole RGBA32 bitmap, which keeps layout handling on the well-tested fresh-create path.
static void zest__imgui_create_or_update_texture(zest_imgui_t *imgui, ImTextureData *tex) {
    zest_device device = imgui->device;
    zest_imgui_backend_texture_t *backend = (zest_imgui_backend_texture_t *)tex->BackendUserData;

    if (!backend) {
        backend = (zest_imgui_backend_texture_t *)zest_AllocateMemory(device, sizeof(zest_imgui_backend_texture_t));
        *backend = {};
        tex->BackendUserData = backend;
    } else if (backend->image.value) {
        // MoltenVK note: zest_FreeImage defers the free by ZEST_MAX_FIF frames, so the old
        // texture stays alive long enough for in-flight frames to finish referencing it.
        zest_FreeImage(backend->image);
        backend->image = {};
    }

    int width = tex->Width;
    int height = tex->Height;
    zest_size upload_size = (zest_size)tex->GetSizeInBytes();

    zest_image_info_t image_info = zest_CreateImageInfo(width, height);
    image_info.flags = zest_image_preset_texture | zest_image_flag_force_image_array;
    backend->image = zest_CreateImage(device, &image_info);
    zest_image image = zest_GetImage(backend->image);
    zest_CopyBitmapToImage(device, tex->GetPixels(), upload_size, image, width, height);
    backend->binding_index = zest_AcquireSampledImageIndex(device, image, zest_texture_array_binding);
    backend->region = {};
    zest_BindAtlasRegionToImage(&backend->region, imgui->font_sampler_binding_index, image, zest_texture_array_binding);

    tex->SetTexID((ImTextureID)&backend->region);
    tex->SetStatus(ImTextureStatus_OK);
}

static void zest__imgui_destroy_texture(zest_imgui_t *imgui, ImTextureData *tex) {
    zest_imgui_backend_texture_t *backend = (zest_imgui_backend_texture_t *)tex->BackendUserData;
    if (backend) {
        if (backend->image.value) {
            zest_FreeImage(backend->image);
        }
        zest_FreeMemory(imgui->device, backend);
    }
    tex->SetTexID(ImTextureID_Invalid);
    tex->BackendUserData = NULL;
    tex->SetStatus(ImTextureStatus_Destroyed);
}

// Honour ImGui's queued texture create/update/destroy requests. Must be called each frame after
// ImGui::Render() and before the GPU samples the textures. The texture list is global (shared by
// all viewports) so a single call handles the main window and every platform window.
void zest_imgui_UpdateTextures(zest_imgui_t *imgui) {
    ImGui::SetCurrentContext(imgui->imgui_context);
    for (ImTextureData *tex : ImGui::GetPlatformIO().Textures) {
        switch (tex->Status) {
        case ImTextureStatus_WantCreate:
        case ImTextureStatus_WantUpdates:
            zest__imgui_create_or_update_texture(imgui, tex);
            break;
        case ImTextureStatus_WantDestroy:
            zest__imgui_destroy_texture(imgui, tex);
            break;
        default:
            break;
        }
    }
}

void zest_imgui_RebuildFontTexture(zest_imgui_t *imgui, zest_uint width, zest_uint height, unsigned char *pixels) {
    (void)imgui; (void)width; (void)height; (void)pixels;
    // ImGui 1.92+ recreates the font atlas GPU texture automatically via the texture request
    // system (handled in zest_imgui_UpdateTextures). After changing fonts you only need to
    // rebuild ImGui's font atlas; the GPU texture follows on the next frame. Kept for API
    // compatibility with older callers.
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
    // Honour any queued ImGui texture create/update/destroy requests before the pass records draw
    // commands that reference them. This is the common entry point for both the simplified
    // zest_imgui_RenderViewport() path and examples that build the frame graph themselves, so the
    // font atlas (and any other ImGui-managed texture) is always uploaded before it is sampled.
    zest_imgui_UpdateTextures(imgui);

    ImDrawData *imgui_draw_data = viewport->imgui_viewport->DrawData;
    ImDrawData *all_draw_data =  ImGui::GetDrawData();
    if (imgui_draw_data && imgui_draw_data->TotalVtxCount > 0 && imgui_draw_data->TotalIdxCount > 0) {
        //Declare resources
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

                // An ImGui-managed texture (font atlas etc.) has a backing ImTextureData; a
                // user-supplied texture passed via ImGui::Image() only carries a raw ImTextureID.
                bool is_imgui_texture = (pcmd->TexRef._TexData != NULL);
                zest_atlas_region_t *current_image = (zest_atlas_region_t*)pcmd->GetTexID();
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

				if (is_imgui_texture) {
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

	// Handle texture requests up front: on frames that reuse a cached frame graph, zest_imgui_BeginPass
	// (which also calls this) is skipped, so we must ensure textures are uploaded here as well. The
	// call is idempotent - already-created textures report ImTextureStatus_OK and are skipped.
	zest_imgui_UpdateTextures(imgui);

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
	float region_ratio = image_size.x / image_size.y;
	float image_ratio = width / height;
	image_size = ImVec2(width, height);
	if (region->width != (zest_uint)width || region->height != (zest_uint)height) {
		if (region_ratio > image_ratio) {
			image_size.x = width;
			image_size.y = width / region_ratio;
		} else {
			image_size.x = height * region_ratio;
			image_size.y = height;
		}
	}
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
    float ratio = image_size.x / image_size.y;
    image_size.x = ratio > 1.f ? width : width * ratio;
    image_size.y = ratio > 1.f ? height / ratio : height;
    ImVec2 image_offset((width - image_size.x) * .5f, (height - image_size.y) * .5f);
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

	colors[ImGuiCol_Text]					= ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
	colors[ImGuiCol_TextDisabled]			= ImVec4(1.00f, 1.00f, 1.00f, 0.26f);
	colors[ImGuiCol_WindowBg]				= ImVec4(0.15f, 0.15f, 0.15f, 0.95f);
	colors[ImGuiCol_ChildBg]				= ImVec4(0.18f, 0.18f, 0.18f, 1.00f);
	colors[ImGuiCol_PopupBg]				= ImVec4(0.14f, 0.14f, 0.14f, 1.00f);
	colors[ImGuiCol_Border]					= ImVec4(1.00f, 1.00f, 1.00f, 0.07f);
	colors[ImGuiCol_BorderShadow]			= ImVec4(0.00f, 0.00f, 0.00f, 0.32f);
	colors[ImGuiCol_FrameBg]				= ImVec4(0.11f, 0.11f, 0.11f, 1.00f);
	colors[ImGuiCol_FrameBgHovered]			= ImVec4(0.25f, 0.25f, 0.25f, 1.00f);
	colors[ImGuiCol_FrameBgActive]			= ImVec4(0.25f, 0.25f, 0.25f, 1.00f);
	colors[ImGuiCol_TitleBg]				= ImVec4(0.10f, 0.10f, 0.10f, 1.00f);
	colors[ImGuiCol_TitleBgActive]			= ImVec4(0.10f, 0.10f, 0.10f, 1.00f);
	colors[ImGuiCol_TitleBgCollapsed]		= ImVec4(0.10f, 0.10f, 0.10f, 1.00f);
	colors[ImGuiCol_MenuBarBg]				= ImVec4(0.15f, 0.15f, 0.15f, 0.95f);
	colors[ImGuiCol_ScrollbarBg]			= ImVec4(0.02f, 0.02f, 0.02f, 0.34f);
	colors[ImGuiCol_ScrollbarGrab]			= ImVec4(0.22f, 0.22f, 0.22f, 1.00f);
	colors[ImGuiCol_ScrollbarGrabHovered]	= ImVec4(0.31f, 0.31f, 0.31f, 1.00f);
	colors[ImGuiCol_ScrollbarGrabActive]	= ImVec4(0.86f, 0.31f, 0.02f, 1.00f);
	colors[ImGuiCol_CheckMark]				= ImVec4(0.91f, 0.35f, 0.05f, 1.00f);
	colors[ImGuiCol_SliderGrab]				= ImVec4(0.86f, 0.31f, 0.02f, 0.53f);
	colors[ImGuiCol_SliderGrabActive]		= ImVec4(0.86f, 0.31f, 0.02f, 1.00f);
	colors[ImGuiCol_Button]					= ImVec4(0.50f, 0.50f, 0.50f, 0.25f);
	colors[ImGuiCol_ButtonHovered]			= ImVec4(0.86f, 0.31f, 0.02f, 0.53f);
	colors[ImGuiCol_ButtonActive]			= ImVec4(0.86f, 0.31f, 0.02f, 1.00f);
	colors[ImGuiCol_Header]					= ImVec4(0.00f, 0.00f, 0.00f, 0.36f);
	colors[ImGuiCol_HeaderHovered]			= ImVec4(0.86f, 0.31f, 0.02f, 0.53f);
	colors[ImGuiCol_HeaderActive]			= ImVec4(0.86f, 0.31f, 0.02f, 0.53f);
	colors[ImGuiCol_Separator]				= ImVec4(1.00f, 1.00f, 1.00f, 0.15f);
	colors[ImGuiCol_SeparatorHovered]		= ImVec4(0.86f, 0.31f, 0.02f, 0.53f);
	colors[ImGuiCol_SeparatorActive]		= ImVec4(0.86f, 0.31f, 0.02f, 1.00f);
	colors[ImGuiCol_ResizeGrip]				= ImVec4(0.86f, 0.31f, 0.02f, 0.53f);
	colors[ImGuiCol_ResizeGripHovered]		= ImVec4(0.86f, 0.31f, 0.02f, 0.53f);
	colors[ImGuiCol_ResizeGripActive]		= ImVec4(0.86f, 0.31f, 0.02f, 1.00f);
	colors[ImGuiCol_Tab]					= ImVec4(0.17f, 0.17f, 0.17f, 0.00f);
	colors[ImGuiCol_TabHovered]				= ImVec4(0.86f, 0.31f, 0.02f, 0.80f);
	colors[ImGuiCol_TabSelected]			= ImVec4(0.86f, 0.31f, 0.02f, 1.00f);
	colors[ImGuiCol_TabSelectedOverline]	= ImVec4(0.86f, 0.31f, 0.02f, 0.0f);
	colors[ImGuiCol_TabDimmed]				= ImVec4(0.86f, 0.31f, 0.02f, 0.00f);
	colors[ImGuiCol_TabDimmedSelected]		= ImVec4(0.86f, 0.31f, 0.02f, 0.68f);
	colors[ImGuiCol_TabDimmedSelectedOverline]		= ImVec4(0.86f, 0.31f, 0.02f, 0.f);
	colors[ImGuiCol_DockingPreview]			= ImVec4(0.86f, 0.31f, 0.02f, 1.00f);
	colors[ImGuiCol_DockingEmptyBg]			= ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
	colors[ImGuiCol_PlotLines]				= ImVec4(0.61f, 0.61f, 0.61f, 1.00f);
	colors[ImGuiCol_PlotLinesHovered]		= ImVec4(1.00f, 0.43f, 0.35f, 1.00f);
	colors[ImGuiCol_PlotHistogram]			= ImVec4(0.90f, 0.70f, 0.00f, 1.00f);
	colors[ImGuiCol_PlotHistogramHovered]	= ImVec4(1.00f, 0.60f, 0.00f, 1.00f);
	colors[ImGuiCol_TableHeaderBg]			= ImVec4(0.08f, 0.08f, 0.08f, 0.59f);
	colors[ImGuiCol_TableBorderStrong]		= ImVec4(0.00f, 0.00f, 0.00f, 0.20f);
	colors[ImGuiCol_TableBorderLight]		= ImVec4(0.00f, 0.00f, 0.00f, 0.10f);
	colors[ImGuiCol_TableRowBg]				= ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
	colors[ImGuiCol_TableRowBgAlt]			= ImVec4(0.00f, 0.00f, 0.00f, 0.11f);
	colors[ImGuiCol_TextSelectedBg]			= ImVec4(0.26f, 0.59f, 0.98f, 0.35f);
	colors[ImGuiCol_DragDropTarget]			= ImVec4(0.86f, 0.31f, 0.02f, 1.00f);
	colors[ImGuiCol_NavCursor]				= ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
	colors[ImGuiCol_NavWindowingHighlight]	= ImVec4(1.00f, 1.00f, 1.00f, 0.70f);
	colors[ImGuiCol_NavWindowingDimBg]		= ImVec4(0.80f, 0.80f, 0.80f, 0.20f);
	colors[ImGuiCol_ModalWindowDimBg]		= ImVec4(0.80f, 0.80f, 0.80f, 0.35f);

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
    style.GrabMinSize = 10.f;
	style.TabCloseButtonMinWidthUnselected = 0.f;
	style.TabCloseButtonMinWidthSelected = 0.f;
    style.DisplayWindowPadding.x = 19.f;
    style.DisplayWindowPadding.y = 19.f;
    style.DisplaySafeAreaPadding.x = 3.f;
    style.DisplaySafeAreaPadding.y = 3.f;
    style.MouseCursorScale = 1.f;
    style.WindowMinSize.x = 32.f;
    style.WindowMinSize.y = 32.f;
    style.ChildBorderSize = 1.f;
}

// Helper: compute variance-blended bar color for a smoothed profile entry
static ImVec4 zest__imgui_variance_bar_color(zest_gpu_profile_smoothed_t *s) {
	// Coefficient of variation: stddev / mean. 0 = steady, >1 = very erratic
	double stddev = sqrt(s->variance);
	double cv = s->smoothed_us > 0.001 ? stddev / s->smoothed_us : 0.0;
	if (cv > 1.0) cv = 1.0;
	float t = (float)cv;
	float alpha = s->depth > 0 ? 0.6f : 0.85f;

	// Base color per queue type
	float base_r, base_g, base_b;
	switch (s->queue_type) {
		case zest_queue_compute:  base_r = 0.27f; base_g = 0.80f; base_b = 0.27f; break;
		case zest_queue_transfer: base_r = 0.27f; base_g = 0.27f; base_b = 0.80f; break;
		default:                  base_r = 0.80f; base_g = 0.53f; base_b = 0.27f; break;
	}
	// Hot color: red-orange for erratic
	float hot_r = 1.0f, hot_g = 0.27f, hot_b = 0.13f;
	return ImVec4(
		base_r * (1.0f - t) + hot_r * t,
		base_g * (1.0f - t) + hot_g * t,
		base_b * (1.0f - t) + hot_b * t,
		alpha
	);
}

// Helper: draw the time and bar columns for a smoothed profile result row
static void zest__imgui_draw_profile_time_and_bar(zest_gpu_profile_smoothed_t *s, double max_time, float bar_max_width) {
	ImGui::TableNextColumn();
	if (s->smoothed_us >= 1000.0) {
		ImGui::Text("%7.2f ms", s->smoothed_us / 1000.0);
	} else {
		ImGui::Text("%7.1f us", s->smoothed_us);
	}

	ImGui::TableNextColumn();
	float bar_width = (float)(s->smoothed_us / max_time) * bar_max_width;
	if (bar_width < 2.0f) bar_width = 2.0f;

	ImVec4 bar_color = zest__imgui_variance_bar_color(s);

	ImVec2 cursor = ImGui::GetCursorScreenPos();
	float row_height = ImGui::GetTextLineHeight();
	ImGui::GetWindowDrawList()->AddRectFilled(
		cursor,
		ImVec2(cursor.x + bar_width, cursor.y + row_height),
		ImGui::ColorConvertFloat4ToU32(bar_color)
	);
	ImGui::Dummy(ImVec2(bar_max_width, row_height));
}

// Helper: check if entry at index i has children (higher depth entries following it)
static bool zest__imgui_pass_has_children(zest_gpu_profile_smoothed_t *results, zest_uint result_count, zest_uint i) {
	return (i + 1 < result_count && results[i + 1].depth > results[i].depth);
}

// Helper: recursively draw profile tree rows starting at *index, for entries at the given depth.
static void zest__imgui_draw_profile_tree(zest_gpu_profile_smoothed_t *results, zest_uint result_count, zest_uint *index, zest_uint parent_depth, double max_time, float bar_max_width) {
	while (*index < result_count && results[*index].depth > parent_depth) {
		zest_gpu_profile_smoothed_t *s = &results[*index];
		zest_uint current_depth = s->depth;
		const char *name = s->name[0] ? s->name : "???";
		bool has_children = (*index + 1 < result_count && results[*index + 1].depth > current_depth);

		ImGui::TableNextRow();
		ImGui::TableNextColumn();

		ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_SpanFullWidth | ImGuiTreeNodeFlags_DefaultOpen;
		if (!has_children) {
			flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
		}

		bool open = ImGui::TreeNodeEx((void*)(intptr_t)*index, flags, "%s", name);

		zest__imgui_draw_profile_time_and_bar(s, max_time, bar_max_width);

		(*index)++;

		if (has_children) {
			if (open) {
				zest__imgui_draw_profile_tree(results, result_count, index, current_depth, max_time, bar_max_width);
				ImGui::TreePop();
			} else {
				while (*index < result_count && results[*index].depth > current_depth) {
					(*index)++;
				}
			}
		}
	}
}

void zest_imgui_DrawGPUProfileWindow(zest_context context) {
	zest_uint result_count = 0;
	zest_gpu_profile_smoothed_t *results = zest_GetGPUProfileSmoothedResults(context, &result_count);

	if (!ImGui::Begin("GPU Profile", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
		ImGui::End();
		return;
	}

	if (result_count == 0) {
		ImGui::TextDisabled("No profiling data");
		ImGui::End();
		return;
	}

	// Find max smoothed time for bar scaling
	double max_time = 0.001;
	for (zest_uint i = 0; i < result_count; ++i) {
		if (results[i].smoothed_us > max_time) {
			max_time = results[i].smoothed_us;
		}
	}

	// Smoothed total GPU time
	double total_time = zest_GetGPUProfileSmoothedTotalTime(context);
	if (total_time >= 1000.0) {
		ImGui::Text("Total: %.2f ms", total_time / 1000.0);
	} else {
		ImGui::Text("Total: %.1f us", total_time);
	}
	{
		zest_uint cpu_profile_count = 0;
		zest_GetCPUProfileSmoothedResults(context, &cpu_profile_count);
		if (cpu_profile_count > 0) {
			double cpu_us = zest_GetCPUProfileSmoothedTotalTime(context);
			double gpu_us = total_time;
			if (gpu_us > cpu_us * 1.2) {
				ImGui::SameLine(); ImGui::TextColored(ImVec4(0.80f, 0.53f, 0.27f, 1.0f), " [GPU bound]");
			} else if (cpu_us > gpu_us * 1.2) {
				ImGui::SameLine(); ImGui::TextColored(ImVec4(0.27f, 0.67f, 0.80f, 1.0f), " [CPU bound]");
			} else {
				ImGui::SameLine(); ImGui::TextColored(ImVec4(0.27f, 0.80f, 0.27f, 1.0f), " [Balanced]");
			}
		}
	}
	ImGui::Separator();

	float bar_max_width = 120.0f;

	ImGuiTableFlags table_flags = ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_Resizable;
	if (ImGui::BeginTable("gpu_profile", 3, table_flags)) {
		ImGui::TableSetupColumn("Pass", ImGuiTableColumnFlags_WidthFixed, 200.0f);
		ImGui::TableSetupColumn("Time", ImGuiTableColumnFlags_WidthFixed, 90.0f);
		ImGui::TableSetupColumn("##bar", ImGuiTableColumnFlags_WidthFixed, bar_max_width + 4.0f);
		ImGui::TableHeadersRow();

		zest_uint i = 0;
		while (i < result_count) {
			zest_gpu_profile_smoothed_t *s = &results[i];
			const char *name = s->name[0] ? s->name : "???";
			bool has_children = zest__imgui_pass_has_children(results, result_count, i);

			ImGui::TableNextRow();
			ImGui::TableNextColumn();

			ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_SpanFullWidth | ImGuiTreeNodeFlags_DefaultOpen;
			if (!has_children) {
				flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
			}

			bool open = ImGui::TreeNodeEx((void*)(intptr_t)i, flags, "%s", name);

			zest__imgui_draw_profile_time_and_bar(s, max_time, bar_max_width);

			i++;

			if (has_children) {
				if (open) {
					zest__imgui_draw_profile_tree(results, result_count, &i, s->depth, max_time, bar_max_width);
					ImGui::TreePop();
				} else {
					while (i < result_count && results[i].depth > s->depth) {
						i++;
					}
				}
			}
		}

		ImGui::EndTable();
	}

	ImGui::End();
}

// -- CPU profiling ImGui helpers --

static ImVec4 zest__imgui_cpu_variance_bar_color(zest_cpu_profile_smoothed_t *s) {
	double stddev = sqrt(s->variance);
	double cv = s->smoothed_us > 0.001 ? stddev / s->smoothed_us : 0.0;
	if (cv > 1.0) cv = 1.0;
	float t = (float)cv;
	float alpha = s->depth > 1 ? 0.6f : 0.85f;

	// Teal/cyan base color for CPU profiling
	float base_r = 0.27f, base_g = 0.67f, base_b = 0.80f;
	float hot_r = 1.0f, hot_g = 0.27f, hot_b = 0.13f;
	return ImVec4(
		base_r * (1.0f - t) + hot_r * t,
		base_g * (1.0f - t) + hot_g * t,
		base_b * (1.0f - t) + hot_b * t,
		alpha
	);
}

static void zest__imgui_draw_cpu_profile_time_and_bar(zest_cpu_profile_smoothed_t *s, double max_time, float bar_max_width) {
	ImGui::TableNextColumn();
	if (s->smoothed_us >= 1000.0) {
		ImGui::Text("%7.2f ms", s->smoothed_us / 1000.0);
	} else {
		ImGui::Text("%7.1f us", s->smoothed_us);
	}

	ImGui::TableNextColumn();
	float bar_width = (float)(s->smoothed_us / max_time) * bar_max_width;
	if (bar_width < 2.0f) bar_width = 2.0f;

	ImVec4 bar_color = zest__imgui_cpu_variance_bar_color(s);

	ImVec2 cursor = ImGui::GetCursorScreenPos();
	float row_height = ImGui::GetTextLineHeight();
	ImGui::GetWindowDrawList()->AddRectFilled(
		cursor,
		ImVec2(cursor.x + bar_width, cursor.y + row_height),
		ImGui::ColorConvertFloat4ToU32(bar_color)
	);
	ImGui::Dummy(ImVec2(bar_max_width, row_height));
}

static bool zest__imgui_cpu_pass_has_children(zest_cpu_profile_smoothed_t *results, zest_uint result_count, zest_uint i) {
	return (i + 1 < result_count && results[i + 1].depth > results[i].depth);
}

static void zest__imgui_draw_cpu_profile_tree(zest_cpu_profile_smoothed_t *results, zest_uint result_count, zest_uint *index, zest_uint parent_depth, double max_time, float bar_max_width) {
	while (*index < result_count && results[*index].depth > parent_depth) {
		zest_cpu_profile_smoothed_t *s = &results[*index];
		zest_uint current_depth = s->depth;
		const char *name = s->name[0] ? s->name : "???";
		bool has_children = (*index + 1 < result_count && results[*index + 1].depth > current_depth);

		ImGui::TableNextRow();
		ImGui::TableNextColumn();

		ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_SpanFullWidth | ImGuiTreeNodeFlags_DefaultOpen;
		if (!has_children) {
			flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
		}

		bool open = ImGui::TreeNodeEx((void*)(intptr_t)(*index + 0x10000), flags, "%s", name);

		zest__imgui_draw_cpu_profile_time_and_bar(s, max_time, bar_max_width);

		(*index)++;

		if (has_children) {
			if (open) {
				zest__imgui_draw_cpu_profile_tree(results, result_count, index, current_depth, max_time, bar_max_width);
				ImGui::TreePop();
			} else {
				while (*index < result_count && results[*index].depth > current_depth) {
					(*index)++;
				}
			}
		}
	}
}

void zest_imgui_DrawCPUProfileWindow(zest_context context) {
	zest_uint result_count = 0;
	zest_cpu_profile_smoothed_t *results = zest_GetCPUProfileSmoothedResults(context, &result_count);

	if (!ImGui::Begin("CPU Profile", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
		ImGui::End();
		return;
	}

	if (result_count == 0) {
		ImGui::TextDisabled("No profiling data");
		ImGui::End();
		return;
	}

	double max_time = 0.001;
	for (zest_uint i = 0; i < result_count; ++i) {
		if (results[i].smoothed_us > max_time) {
			max_time = results[i].smoothed_us;
		}
	}

	double total_time = zest_GetCPUProfileSmoothedTotalTime(context);
	if (total_time >= 1000.0) {
		ImGui::Text("Total: %.2f ms", total_time / 1000.0);
	} else {
		ImGui::Text("Total: %.1f us", total_time);
	}
	{
		zest_uint gpu_profile_count = 0;
		zest_GetGPUProfileSmoothedResults(context, &gpu_profile_count);
		if (gpu_profile_count > 0) {
			double gpu_us = zest_GetGPUProfileSmoothedTotalTime(context);
			double cpu_us = total_time;
			if (gpu_us > cpu_us * 1.2) {
				ImGui::SameLine(); ImGui::TextColored(ImVec4(0.80f, 0.53f, 0.27f, 1.0f), " [GPU bound]");
			} else if (cpu_us > gpu_us * 1.2) {
				ImGui::SameLine(); ImGui::TextColored(ImVec4(0.27f, 0.67f, 0.80f, 1.0f), " [CPU bound]");
			} else {
				ImGui::SameLine(); ImGui::TextColored(ImVec4(0.27f, 0.80f, 0.27f, 1.0f), " [Balanced]");
			}
		}
	}
	ImGui::Separator();

	float bar_max_width = 120.0f;

	ImGuiTableFlags table_flags = ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_Resizable;
	if (ImGui::BeginTable("cpu_profile", 3, table_flags)) {
		ImGui::TableSetupColumn("Region", ImGuiTableColumnFlags_WidthFixed, 200.0f);
		ImGui::TableSetupColumn("Time", ImGuiTableColumnFlags_WidthFixed, 90.0f);
		ImGui::TableSetupColumn("##bar", ImGuiTableColumnFlags_WidthFixed, bar_max_width + 4.0f);
		ImGui::TableHeadersRow();

		zest_uint i = 0;
		while (i < result_count) {
			zest_cpu_profile_smoothed_t *s = &results[i];
			const char *name = s->name[0] ? s->name : "???";
			bool has_children = zest__imgui_cpu_pass_has_children(results, result_count, i);

			ImGui::TableNextRow();
			ImGui::TableNextColumn();

			ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_SpanFullWidth | ImGuiTreeNodeFlags_DefaultOpen;
			if (!has_children) {
				flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
			}

			bool open = ImGui::TreeNodeEx((void*)(intptr_t)(i + 0x10000), flags, "%s", name);

			zest__imgui_draw_cpu_profile_time_and_bar(s, max_time, bar_max_width);

			i++;

			if (has_children) {
				if (open) {
					zest__imgui_draw_cpu_profile_tree(results, result_count, &i, s->depth, max_time, bar_max_width);
					ImGui::TreePop();
				} else {
					while (i < result_count && results[i].depth > s->depth) {
						i++;
					}
				}
			}
		}

		ImGui::EndTable();
	}

	ImGui::End();
}

void zest_imgui_DrawProfileWindow(zest_context context) {
	if (!ImGui::Begin("Profile", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
		ImGui::End();
		return;
	}

	float bar_max_width = 120.0f;

	// Bound indicator at top when both profilers have data
	zest_uint gpu_count = 0;
	zest_gpu_profile_smoothed_t *gpu_results = zest_GetGPUProfileSmoothedResults(context, &gpu_count);
	zest_uint cpu_count = 0;
	zest_cpu_profile_smoothed_t *cpu_results = zest_GetCPUProfileSmoothedResults(context, &cpu_count);
	if (gpu_count > 0 && cpu_count > 0) {
		double gpu_total = zest_GetGPUProfileSmoothedTotalTime(context);
		double cpu_total = zest_GetCPUProfileSmoothedTotalTime(context);
		if (gpu_total > cpu_total * 1.2) {
			ImGui::TextColored(ImVec4(0.80f, 0.53f, 0.27f, 1.0f), "GPU bound");
		} else if (cpu_total > gpu_total * 1.2) {
			ImGui::TextColored(ImVec4(0.27f, 0.67f, 0.80f, 1.0f), "CPU bound");
		} else {
			ImGui::TextColored(ImVec4(0.27f, 0.80f, 0.27f, 1.0f), "Balanced");
		}
		ImGui::SameLine();
		ImGui::Text(" - CPU: %.2f ms  GPU: %.2f ms", cpu_total / 1000.0, gpu_total / 1000.0);
		ImGui::Separator();
	}

	// GPU section
	if (gpu_count > 0) {
		double gpu_total = zest_GetGPUProfileSmoothedTotalTime(context);
		if (gpu_total >= 1000.0) {
			ImGui::TextColored(ImVec4(0.80f, 0.53f, 0.27f, 1.0f), "GPU - Total: %.2f ms", gpu_total / 1000.0);
		} else {
			ImGui::TextColored(ImVec4(0.80f, 0.53f, 0.27f, 1.0f), "GPU - Total: %.1f us", gpu_total);
		}

		double gpu_max_time = 0.001;
		for (zest_uint i = 0; i < gpu_count; ++i) {
			if (gpu_results[i].smoothed_us > gpu_max_time) {
				gpu_max_time = gpu_results[i].smoothed_us;
			}
		}

		ImGuiTableFlags table_flags = ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_Resizable;
		if (ImGui::BeginTable("gpu_profile_combined", 3, table_flags)) {
			ImGui::TableSetupColumn("Pass", ImGuiTableColumnFlags_WidthFixed, 200.0f);
			ImGui::TableSetupColumn("Time", ImGuiTableColumnFlags_WidthFixed, 90.0f);
			ImGui::TableSetupColumn("##bar", ImGuiTableColumnFlags_WidthFixed, bar_max_width + 4.0f);
			ImGui::TableHeadersRow();

			zest_uint i = 0;
			while (i < gpu_count) {
				zest_gpu_profile_smoothed_t *s = &gpu_results[i];
				const char *name = s->name[0] ? s->name : "???";
				bool has_children = zest__imgui_pass_has_children(gpu_results, gpu_count, i);

				ImGui::TableNextRow();
				ImGui::TableNextColumn();

				ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_SpanFullWidth | ImGuiTreeNodeFlags_DefaultOpen;
				if (!has_children) {
					flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
				}

				bool open = ImGui::TreeNodeEx((void*)(intptr_t)i, flags, "%s", name);
				zest__imgui_draw_profile_time_and_bar(s, gpu_max_time, bar_max_width);

				i++;

				if (has_children) {
					if (open) {
						zest__imgui_draw_profile_tree(gpu_results, gpu_count, &i, s->depth, gpu_max_time, bar_max_width);
						ImGui::TreePop();
					} else {
						while (i < gpu_count && gpu_results[i].depth > s->depth) {
							i++;
						}
					}
				}
			}

			ImGui::EndTable();
		}
	}

	// CPU section
	if (cpu_count > 0) {
		if (gpu_count > 0) ImGui::Spacing();

		double cpu_total = zest_GetCPUProfileSmoothedTotalTime(context);
		if (cpu_total >= 1000.0) {
			ImGui::TextColored(ImVec4(0.27f, 0.67f, 0.80f, 1.0f), "CPU - Total: %.2f ms", cpu_total / 1000.0);
		} else {
			ImGui::TextColored(ImVec4(0.27f, 0.67f, 0.80f, 1.0f), "CPU - Total: %.1f us", cpu_total);
		}

		double cpu_max_time = 0.001;
		for (zest_uint i = 0; i < cpu_count; ++i) {
			if (cpu_results[i].smoothed_us > cpu_max_time) {
				cpu_max_time = cpu_results[i].smoothed_us;
			}
		}

		ImGuiTableFlags table_flags = ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_Resizable;
		if (ImGui::BeginTable("cpu_profile_combined", 3, table_flags)) {
			ImGui::TableSetupColumn("Region", ImGuiTableColumnFlags_WidthFixed, 200.0f);
			ImGui::TableSetupColumn("Time", ImGuiTableColumnFlags_WidthFixed, 90.0f);
			ImGui::TableSetupColumn("##bar", ImGuiTableColumnFlags_WidthFixed, bar_max_width + 4.0f);
			ImGui::TableHeadersRow();

			zest_uint i = 0;
			while (i < cpu_count) {
				zest_cpu_profile_smoothed_t *s = &cpu_results[i];
				const char *name = s->name[0] ? s->name : "???";
				bool has_children = zest__imgui_cpu_pass_has_children(cpu_results, cpu_count, i);

				ImGui::TableNextRow();
				ImGui::TableNextColumn();

				ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_SpanFullWidth | ImGuiTreeNodeFlags_DefaultOpen;
				if (!has_children) {
					flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
				}

				bool open = ImGui::TreeNodeEx((void*)(intptr_t)(i + 0x10000), flags, "%s", name);
				zest__imgui_draw_cpu_profile_time_and_bar(s, cpu_max_time, bar_max_width);

				i++;

				if (has_children) {
					if (open) {
						zest__imgui_draw_cpu_profile_tree(cpu_results, cpu_count, &i, s->depth, cpu_max_time, bar_max_width);
						ImGui::TreePop();
					} else {
						while (i < cpu_count && cpu_results[i].depth > s->depth) {
							i++;
						}
					}
				}
			}

			ImGui::EndTable();
		}
	}

	if (gpu_count == 0 && cpu_count == 0) {
		ImGui::TextDisabled("No profiling data");
	}

	ImGui::End();
}

// -- Memory usage ImGui helpers --

static void zest__imgui_memory_size_string(char *buffer, int buffer_size, zest_size size) {
	if (size >= 1024ull * 1024ull * 1024ull) {
		snprintf(buffer, buffer_size, "%.2f GB", (double)size / (1024.0 * 1024.0 * 1024.0));
	} else if (size >= 1024ull * 1024ull) {
		snprintf(buffer, buffer_size, "%.2f MB", (double)size / (1024.0 * 1024.0));
	} else if (size >= 1024ull) {
		snprintf(buffer, buffer_size, "%.1f KB", (double)size / 1024.0);
	} else {
		snprintf(buffer, buffer_size, "%llu B", (unsigned long long)size);
	}
}

static void zest__imgui_memory_usage_bar(zest_size used, zest_size capacity) {
	char used_str[32], capacity_str[32], overlay[80];
	zest__imgui_memory_size_string(used_str, sizeof(used_str), used);
	zest__imgui_memory_size_string(capacity_str, sizeof(capacity_str), capacity);
	snprintf(overlay, sizeof(overlay), "%s / %s", used_str, capacity_str);
	float fraction = capacity > 0 ? (float)((double)used / (double)capacity) : 0.0f;
	ImGui::ProgressBar(fraction, ImVec2(180.0f, 0.0f), overlay);
}

static void zest__imgui_append_flag_name(char *buffer, int buffer_size, const char *name) {
	if (buffer[0]) {
		strncat(buffer, ", ", buffer_size - (int)strlen(buffer) - 1);
	}
	strncat(buffer, name, buffer_size - (int)strlen(buffer) - 1);
}

static void zest__imgui_memory_property_string(char *buffer, int buffer_size, zest_memory_property_flags flags) {
	buffer[0] = '\0';
	if (flags & zest_memory_property_device_local_bit) zest__imgui_append_flag_name(buffer, buffer_size, "Device Local");
	if (flags & zest_memory_property_host_visible_bit) zest__imgui_append_flag_name(buffer, buffer_size, "Host Visible");
	if (flags & zest_memory_property_host_coherent_bit) zest__imgui_append_flag_name(buffer, buffer_size, "Host Coherent");
	if (flags & zest_memory_property_host_cached_bit) zest__imgui_append_flag_name(buffer, buffer_size, "Host Cached");
	if (flags & zest_memory_property_lazily_allocated_bit) zest__imgui_append_flag_name(buffer, buffer_size, "Lazily Allocated");
	if (flags & zest_memory_property_protected_bit) zest__imgui_append_flag_name(buffer, buffer_size, "Protected");
	if (!buffer[0]) snprintf(buffer, buffer_size, "None");
}

static void zest__imgui_buffer_usage_string(char *buffer, int buffer_size, zest_buffer_usage_flags flags) {
	buffer[0] = '\0';
	if (flags & zest_buffer_usage_transfer_src_bit) zest__imgui_append_flag_name(buffer, buffer_size, "Transfer Src");
	if (flags & zest_buffer_usage_transfer_dst_bit) zest__imgui_append_flag_name(buffer, buffer_size, "Transfer Dst");
	if (flags & zest_buffer_usage_uniform_texel_buffer_bit) zest__imgui_append_flag_name(buffer, buffer_size, "Uniform Texel");
	if (flags & zest_buffer_usage_storage_texel_buffer_bit) zest__imgui_append_flag_name(buffer, buffer_size, "Storage Texel");
	if (flags & zest_buffer_usage_uniform_buffer_bit) zest__imgui_append_flag_name(buffer, buffer_size, "Uniform");
	if (flags & zest_buffer_usage_storage_buffer_bit) zest__imgui_append_flag_name(buffer, buffer_size, "Storage");
	if (flags & zest_buffer_usage_index_buffer_bit) zest__imgui_append_flag_name(buffer, buffer_size, "Index");
	if (flags & zest_buffer_usage_vertex_buffer_bit) zest__imgui_append_flag_name(buffer, buffer_size, "Vertex");
	if (flags & zest_buffer_usage_indirect_buffer_bit) zest__imgui_append_flag_name(buffer, buffer_size, "Indirect");
	if (flags & zest_buffer_usage_shader_device_address_bit) zest__imgui_append_flag_name(buffer, buffer_size, "Device Address");
	if (!buffer[0]) snprintf(buffer, buffer_size, "None");
}

static void zest__imgui_image_usage_string(char *buffer, int buffer_size, zest_image_usage_flags flags) {
	buffer[0] = '\0';
	if (flags & zest_image_usage_transfer_src_bit) zest__imgui_append_flag_name(buffer, buffer_size, "Transfer Src");
	if (flags & zest_image_usage_transfer_dst_bit) zest__imgui_append_flag_name(buffer, buffer_size, "Transfer Dst");
	if (flags & zest_image_usage_sampled_bit) zest__imgui_append_flag_name(buffer, buffer_size, "Sampled");
	if (flags & zest_image_usage_storage_bit) zest__imgui_append_flag_name(buffer, buffer_size, "Storage");
	if (flags & zest_image_usage_color_attachment_bit) zest__imgui_append_flag_name(buffer, buffer_size, "Color Attachment");
	if (flags & zest_image_usage_depth_stencil_attachment_bit) zest__imgui_append_flag_name(buffer, buffer_size, "Depth/Stencil Attachment");
	if (flags & zest_image_usage_transient_attachment_bit) zest__imgui_append_flag_name(buffer, buffer_size, "Transient Attachment");
	if (flags & zest_image_usage_input_attachment_bit) zest__imgui_append_flag_name(buffer, buffer_size, "Input Attachment");
	if (flags & zest_image_usage_host_transfer_bit) zest__imgui_append_flag_name(buffer, buffer_size, "Host Transfer");
	if (!buffer[0]) snprintf(buffer, buffer_size, "None");
}

//A single full width detail line under an open pool tree node
static void zest__imgui_memory_detail_row(const char *fmt, ...) {
	char text[256];
	va_list args;
	va_start(args, fmt);
	vsnprintf(text, sizeof(text), fmt, args);
	va_end(args);
	ImGui::TableNextRow();
	ImGui::TableNextColumn();
	ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen | ImGuiTreeNodeFlags_SpanAllColumns | ImGuiTreeNodeFlags_Bullet;
	ImGui::TreeNodeEx(text, flags);
}

static void zest__imgui_draw_pool_details(zest_buffer_pool_usage_t *pool) {
	char flag_str[192], size_str[32], size_str2[32];

	zest__imgui_memory_property_string(flag_str, sizeof(flag_str), pool->property_flags);
	zest__imgui_memory_detail_row("Memory properties: %s", flag_str);

	if (pool->is_image_pool) {
		zest__imgui_image_usage_string(flag_str, sizeof(flag_str), pool->image_usage_flags);
		zest__imgui_memory_detail_row("Image usage: %s", flag_str);
		zest__imgui_memory_size_string(size_str, sizeof(size_str), pool->alignment);
		zest__imgui_memory_detail_row("Image alignment: %s, memory type bits: 0x%08x", size_str, pool->backend_memory_bits);
	} else {
		zest__imgui_buffer_usage_string(flag_str, sizeof(flag_str), pool->buffer_usage_flags);
		zest__imgui_memory_detail_row("Buffer usage: %s", flag_str);
	}

	const char *pool_type = "Custom";
	switch (pool->memory_pool_type) {
		case zest_memory_pool_type_buffers: pool_type = "Large buffers"; break;
		case zest_memory_pool_type_images: pool_type = "Images"; break;
		case zest_memory_pool_type_small_buffers: pool_type = "Small buffers"; break;
		default: break;
	}
	zest__imgui_memory_size_string(size_str, sizeof(size_str), pool->configured_pool_size);
	zest__imgui_memory_size_string(size_str2, sizeof(size_str2), pool->minimum_allocation_size);
	zest__imgui_memory_detail_row("Sizing: %s (%s per pool, %s minimum allocation)", pool_type, size_str, size_str2);

	zest__imgui_memory_size_string(size_str, sizeof(size_str), pool->offset_granularity);
	zest__imgui_memory_detail_row("Offset granularity: %s", size_str);

	if (pool->frame_in_flight > 0) {
		zest__imgui_memory_detail_row("Dedicated to frame in flight: %u", pool->frame_in_flight);
	}

	if (pool->is_context_pool) {
		zest__imgui_memory_detail_row("Owner: Context (frame lifetime buffers)");
	} else {
		zest__imgui_memory_detail_row("Owner: Device (persistent buffers)");
	}

	char pools_str[192];
	pools_str[0] = '\0';
	zest_uint listed = pool->pool_count < ZEST_MAX_REPORTED_POOLS ? pool->pool_count : ZEST_MAX_REPORTED_POOLS;
	for (zest_uint i = 0; i < listed; ++i) {
		zest__imgui_memory_size_string(size_str, sizeof(size_str), pool->pool_sizes[i]);
		zest__imgui_append_flag_name(pools_str, sizeof(pools_str), size_str);
	}
	zest__imgui_memory_detail_row("Backing pools: %s%s", pools_str, pool->pool_count > listed ? ", ..." : "");
}

void zest_imgui_DrawMemoryUsageWindow(zest_context context) {
	if (!ImGui::Begin("Memory Usage", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
		ImGui::End();
		return;
	}

	zest_memory_usage_t usage = zest_GetMemoryUsage(context);
	ImGuiTableFlags table_flags = ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_Resizable;

	ImGui::SeparatorText("Host (CPU)");
	if (ImGui::BeginTable("host_memory", 3, table_flags)) {
		ImGui::TableSetupColumn("Allocator", ImGuiTableColumnFlags_WidthFixed, 160.0f);
		ImGui::TableSetupColumn("Pools", ImGuiTableColumnFlags_WidthFixed, 45.0f);
		ImGui::TableSetupColumn("Usage");
		ImGui::TableHeadersRow();

		ImGui::TableNextRow();
		ImGui::TableNextColumn(); ImGui::TextUnformatted("Device");
		ImGui::TableNextColumn(); ImGui::Text("%u", usage.host_device_pool_count);
		ImGui::TableNextColumn(); zest__imgui_memory_usage_bar(usage.host_device_used, usage.host_device_capacity);

		ImGui::TableNextRow();
		ImGui::TableNextColumn(); ImGui::TextUnformatted("Context");
		ImGui::TableNextColumn(); ImGui::Text("%u", usage.host_context_pool_count);
		ImGui::TableNextColumn(); zest__imgui_memory_usage_bar(usage.host_context_used, usage.host_context_capacity);

		ImGui::EndTable();
	}

	ImGui::SeparatorText("GPU Pools");
	zest_buffer_pool_usage_t pools[32];
	zest_uint pool_count = zest_GetBufferPoolUsages(context, pools, 32);
	if (pool_count > 32) pool_count = 32;
	if (pool_count == 0) {
		ImGui::TextDisabled("No GPU pools allocated");
	} else if (ImGui::BeginTable("gpu_memory", 5, table_flags)) {
		ImGui::TableSetupColumn("Pool", ImGuiTableColumnFlags_WidthFixed, 200.0f);
		ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthFixed, 90.0f);
		ImGui::TableSetupColumn("Pools", ImGuiTableColumnFlags_WidthFixed, 45.0f);
		ImGui::TableSetupColumn("Allocs", ImGuiTableColumnFlags_WidthFixed, 45.0f);
		ImGui::TableSetupColumn("Usage");
		ImGui::TableHeadersRow();

		for (zest_uint i = 0; i < pool_count; ++i) {
			zest_buffer_pool_usage_t *pool = &pools[i];
			const char *name = pool->name ? pool->name : "???";
			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			ImGuiTreeNodeFlags node_flags = ImGuiTreeNodeFlags_SpanFullWidth;
			bool open = ImGui::TreeNodeEx((void*)(intptr_t)i, node_flags, "%s%s", name, pool->is_context_pool ? " (context)" : "");
			ImGui::TableNextColumn();
			ImGui::Text("%s%s", pool->is_image_pool ? "Image" : "Buffer", pool->host_visible ? " (host)" : "");
			ImGui::TableNextColumn(); ImGui::Text("%u", pool->pool_count);
			ImGui::TableNextColumn(); ImGui::Text("%u", pool->used_blocks);
			ImGui::TableNextColumn(); zest__imgui_memory_usage_bar(pool->used, pool->capacity);
			if (open) {
				zest__imgui_draw_pool_details(pool);
				ImGui::TreePop();
			}
		}

		ImGui::EndTable();
	}

	char size_str[32], size_str2[32];
	if (usage.gpu_dedicated_count > 0) {
		zest__imgui_memory_size_string(size_str, sizeof(size_str), usage.gpu_dedicated_size);
		ImGui::Text("Dedicated image memory: %u allocation%s, %s", usage.gpu_dedicated_count, usage.gpu_dedicated_count == 1 ? "" : "s", size_str);
	}
	if (usage.gpu_transient_arena_count > 0) {
		zest__imgui_memory_size_string(size_str, sizeof(size_str), usage.gpu_transient_high_water);
		zest__imgui_memory_size_string(size_str2, sizeof(size_str2), usage.gpu_transient_capacity);
		ImGui::Text("Transient arenas: %u, high water %s of %s", usage.gpu_transient_arena_count, size_str, size_str2);
	}

	//Every context on the device, not just the one passed in - a leak on a headless worker
	//context is otherwise invisible here.
	zest_device device = zest_GetContextDevice(context);
	zest_uint context_count = zest_GetDeviceContextCount(device);
	if (context_count > 1) {
		ImGui::SeparatorText("Contexts");
		for (zest_uint i = 0; i < context_count; ++i) {
			zest_context device_context = zest_GetDeviceContext(device, i);
			zest_uint arena_count = zest_GetContextTransientArenaCount(device_context);
			zest_uint checked_out = zest_GetContextCheckedOutArenaCount(device_context);
			zest_uint pending = zest_GetContextPendingReleaseCount(device_context);
			ImGui::Text("Context %u%s%s: %u arena%s (%u checked out), %u pending release%s",
				i,
				zest_ContextIsHeadless(device_context) ? " (headless)" : "",
				device_context == context ? " (this)" : "",
				arena_count, arena_count == 1 ? "" : "s", checked_out,
				pending, pending == 1 ? "" : "s");
		}
	}

	ImGui::SeparatorText("Totals");
	zest_size host_capacity = usage.host_device_capacity + usage.host_context_capacity;
	zest_size host_used = usage.host_device_used + usage.host_context_used;
	zest_size gpu_capacity = usage.gpu_pool_capacity + usage.gpu_dedicated_size + usage.gpu_transient_capacity;
	zest_size gpu_used = usage.gpu_pool_used + usage.gpu_dedicated_size + usage.gpu_transient_capacity;
	zest__imgui_memory_size_string(size_str, sizeof(size_str), host_used);
	zest__imgui_memory_size_string(size_str2, sizeof(size_str2), host_capacity);
	ImGui::Text("Host: %s used of %s", size_str, size_str2);
	zest__imgui_memory_size_string(size_str, sizeof(size_str), gpu_used);
	zest__imgui_memory_size_string(size_str2, sizeof(size_str2), gpu_capacity);
	ImGui::Text("GPU: %s used of %s", size_str, size_str2);
	ImGui::Text("Backend allocations: %u / %u", usage.backend_allocation_count, usage.max_backend_allocations);

	ImGui::End();
}

void zest_imgui_Destroy(zest_imgui_t *imgui) {
	// Release the GPU textures and backend book-keeping we created for ImGui's dynamically
	// managed textures (font atlas etc.) before tearing down the ImGui context.
	ImGui::SetCurrentContext(imgui->imgui_context);
	for (ImTextureData *tex : ImGui::GetPlatformIO().Textures) {
		if (tex->BackendUserData) {
			zest__imgui_destroy_texture(imgui, tex);
		}
	}
	ImGui::DestroyContext();
	zest_FreeImage(imgui->font_texture);
	if (imgui->font_texture_previous.value) {
		zest_FreeImage(imgui->font_texture_previous);
	}
	zest_FreePipelineTemplate(imgui->pipeline);
}
//--End Dear ImGui helper functions
