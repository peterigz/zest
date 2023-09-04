#include "impl_imgui.h"
#include "imgui_internal.h"

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

				VkRect2D scissor_rect;
				scissor_rect.offset.x = ZEST__MAX((int32_t)(pcmd->ClipRect.x), 0);
				scissor_rect.offset.y = ZEST__MAX((int32_t)(pcmd->ClipRect.y), 0);
				scissor_rect.extent.width = (zest_uint)(pcmd->ClipRect.z - pcmd->ClipRect.x);
				scissor_rect.extent.height = (zest_uint)(pcmd->ClipRect.w - pcmd->ClipRect.y);

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
