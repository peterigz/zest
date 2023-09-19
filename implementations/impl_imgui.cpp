#include "impl_imgui.h"
#include "imgui_internal.h"

void zest_imgui_Initialise(zest_imgui_layer_info *imgui_layer_info) {
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO();
	io.DisplaySize = ImVec2(zest_ScreenWidthf(), zest_ScreenHeightf());
	io.DisplayFramebufferScale = ImVec2(1.0f, 1.0f);
	unsigned char* pixels;
	int width, height;
	io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
	int upload_size = width * height * 4 * sizeof(char);

	zest_bitmap_t font_bitmap = zest_CreateBitmapFromRawBuffer("font_bitmap", pixels, upload_size, width, height, 4);
	imgui_layer_info->font_texture = zest_CreateTexture("imgui_font", zest_texture_storage_type_single, zest_texture_flag_none, zest_texture_format_rgba, 10);
	zest_image font_image = zest_AddTextureImageBitmap(imgui_layer_info->font_texture, &font_bitmap);
	zest_ProcessTextureImages(imgui_layer_info->font_texture);
	io.Fonts->SetTexID(font_image);
	ImGui_ImplGlfw_InitForVulkan((GLFWwindow*)ZestApp->window->window_handle, true);
}

void zest_imgui_RebuildFontTexture(zest_imgui_layer_info *imgui_layer_info, zest_uint width, zest_uint height, unsigned char* pixels) {
	zest_WaitForIdleDevice();
	int upload_size = width * height * 4 * sizeof(char);
	zest_bitmap_t font_bitmap = zest_CreateBitmapFromRawBuffer("font_bitmap", pixels, upload_size, width, height, 4);
	zest_FreeTextureBitmaps(imgui_layer_info->font_texture);
	zest_image font_image = zest_AddTextureImageBitmap(imgui_layer_info->font_texture, &font_bitmap);
	zest_ProcessTextureImages(imgui_layer_info->font_texture);
	zest_RefreshTextureDescriptors(imgui_layer_info->font_texture);
	ImGuiIO& io = ImGui::GetIO();
	io.Fonts->SetTexID(font_image);
}

void zest_imgui_CreateLayer(zest_imgui_layer_info *imgui_layer_info) {
	imgui_layer_info->mesh_layer = zest_NewMeshLayer("imgui mesh layer", sizeof(ImDrawVert));
	imgui_layer_info->pipeline = zest_Pipeline("pipeline_imgui");
	zest_ContextDrawRoutine()->draw_callback = zest_imgui_DrawLayer;
	zest_ContextDrawRoutine()->user_data = imgui_layer_info;
}

void zest_imgui_DrawLayer(zest_draw_routine_t *draw_routine, VkCommandBuffer command_buffer) {
	ImDrawData *imgui_draw_data = ImGui::GetDrawData();
	zest_pipeline last_pipeline = ZEST_NULL;
	VkDescriptorSet last_descriptor_set = VK_NULL_HANDLE;

	zest_imgui_layer_info *layer_info = (zest_imgui_layer_info*)draw_routine->user_data;
	zest_layer_t *imgui_layer = layer_info->mesh_layer;

	zest_BindMeshVertexBuffer(imgui_layer);
	zest_BindMeshIndexBuffer(imgui_layer);

	if (imgui_draw_data && imgui_draw_data->CmdListsCount > 0) {

		int32_t vertex_offset = 0;
		int32_t index_offset = 0;
        
        ImVec2 clip_off = imgui_draw_data->DisplayPos;
        ImVec2 clip_scale = imgui_draw_data->FramebufferScale;

		for (int32_t i = 0; i < imgui_draw_data->CmdListsCount; i++)
		{
			const ImDrawList* cmd_list = imgui_draw_data->CmdLists[i];
			for (int32_t j = 0; j < cmd_list->CmdBuffer.Size; j++)
			{
				const ImDrawCmd* pcmd = &cmd_list->CmdBuffer[j];

				zest_image_t *current_image = (zest_image_t*)pcmd->TextureId;
				if (!current_image) {
					//This means we're trying to draw a render target
					assert(pcmd->UserCallbackData);
					//QulkanRenderTarget *render_target = static_cast<QulkanRenderTarget*>(pcmd->UserCallbackData);
					//current_image = &render_target->GetRenderTargetImage();
				}

				if (last_pipeline != layer_info->pipeline || last_descriptor_set != zest_CurrentTextureDescriptorSet(current_image->texture)) {
					zest_BindPipeline(layer_info->pipeline, zest_CurrentTextureDescriptorSet(current_image->texture));
					last_descriptor_set = zest_CurrentTextureDescriptorSet(current_image->texture);
					last_pipeline = layer_info->pipeline;
				}

				imgui_layer->push_constants.parameters2.x = current_image->layer;
				imgui_layer->push_constants.flags = 0;

				zest_SendStandardPushConstants(layer_info->pipeline, &imgui_layer->push_constants);
                
                ImVec2 clip_min((pcmd->ClipRect.x - clip_off.x) * clip_scale.x, (pcmd->ClipRect.y - clip_off.y) * clip_scale.y);
                ImVec2 clip_max((pcmd->ClipRect.z - clip_off.x) * clip_scale.x, (pcmd->ClipRect.w - clip_off.y) * clip_scale.y);

                // Clamp to viewport as vkCmdSetScissor() won't accept values that are off bounds
                if (clip_max.x > zest_SwapChainWidth()) { clip_max.x = zest_SwapChainWidthf(); }
                if (clip_max.y > zest_SwapChainHeight()) { clip_max.y = zest_SwapChainHeightf(); }
                if (clip_max.x <= clip_min.x || clip_max.y <= clip_min.y)
                    continue;

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
}

void zest_imgui_CopyBuffers(zest_layer imgui_layer) {
	ImDrawData *imgui_draw_data = ImGui::GetDrawData();

	zest_buffer_t *vertex_buffer = zest_GetVertexStagingBuffer(imgui_layer);
	zest_buffer_t *index_buffer = zest_GetIndexStagingBuffer(imgui_layer);

	if (imgui_draw_data) {
		index_buffer->memory_in_use = imgui_draw_data->TotalIdxCount * sizeof(ImDrawIdx);
		vertex_buffer->memory_in_use = imgui_draw_data->TotalVtxCount * sizeof(ImDrawVert);
	}
	else {
		index_buffer->memory_in_use = 0;
		vertex_buffer->memory_in_use = 0;
	}

	imgui_layer->push_constants.parameters1 = zest_Vec4Set(2.0f / zest_ScreenWidthf(), 2.0f / zest_ScreenHeightf(), -1.f, -1.f);
	imgui_layer->push_constants.parameters2 = zest_Vec4Set1(0.f);

	zest_buffer_t *device_vertex_buffer = zest_GetVertexDeviceBuffer(imgui_layer);
	zest_buffer_t *device_index_buffer = zest_GetIndexDeviceBuffer(imgui_layer);

	if (imgui_draw_data) {
		if (index_buffer->memory_in_use > index_buffer->size) {
			zest_GrowMeshIndexBuffers(imgui_layer);
		}
		if (vertex_buffer->memory_in_use > vertex_buffer->size) {
			zest_GrowMeshVertexBuffers(imgui_layer);
		}
		ImDrawIdx* idxDst = (ImDrawIdx*)index_buffer->data;
		ImDrawVert* vtxDst = (ImDrawVert*)vertex_buffer->data;

		for (int n = 0; n < imgui_draw_data->CmdListsCount; n++) {
			const ImDrawList* cmd_list = imgui_draw_data->CmdLists[n];
			memcpy(vtxDst, cmd_list->VtxBuffer.Data, cmd_list->VtxBuffer.Size * sizeof(ImDrawVert));
			memcpy(idxDst, cmd_list->IdxBuffer.Data, cmd_list->IdxBuffer.Size * sizeof(ImDrawIdx));
			vtxDst += cmd_list->VtxBuffer.Size;
			idxDst += cmd_list->IdxBuffer.Size;
		}
	}
}

void zest_imgui_DrawImage(zest_image image, float width, float height) {
	using namespace ImGui;

	ImVec2 image_size((float)image->width, (float)image->height);
	float ratio = image_size.x / image_size.y;
	image_size.x = ratio > 1 ? width : width * ratio;
	image_size.y = ratio > 1 ? height / ratio : height;
	ImVec2 image_offset((width - image_size.x) * .5f, (height - image_size.y) * .5f);
	ImGuiWindow* window = GetCurrentWindow();
	const ImRect image_bb(window->DC.CursorPos + image_offset, window->DC.CursorPos + image_offset + image_size);
	ImVec4 tint_col(1.f, 1.f, 1.f, 1.f);
	window->DrawList->AddImage(image, image_bb.Min, image_bb.Max, ImVec2(image->uv.x, image->uv.y), ImVec2(image->uv.z, image->uv.w), GetColorU32(tint_col));
}

void zest_imgui_DrawImage2(zest_image image, float width, float height) {
	ImGui::Image(image, ImVec2(width, height), ImVec2(image->uv.x, image->uv.y), ImVec2(image->uv.z, image->uv.w));
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

	ImGui::Image(&image, ImVec2(width, height), ImVec2(uv.x, uv.y), zw);
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
	ImGuiWindow* window = ImGui::GetCurrentWindow();
	if (window->SkipItems)
		return;

	window->DrawList->PushTextureID(nullptr);
	ImRect bb(window->DC.CursorPos, window->DC.CursorPos + ImVec2(width, height));
	ImGui::ItemSize(bb);
	if (!ImGui::ItemAdd(bb, 0))
		return;

	window->DrawList->PrimReserve(6, 4);
	window->DrawList->PrimRectUV(bb.Min, bb.Max, ImVec2(uv.x, uv.y), zw, IM_COL32_WHITE);
	IM_ASSERT_PARANOID(window->DrawList->CmdBuffer.Size > 0);
	ImDrawCmd* curr_cmd = &window->DrawList->CmdBuffer.Data[window->DrawList->CmdBuffer.Size - 1];
	curr_cmd->UserCallbackData = &render_target;

	window->DrawList->PopTextureID();
}
