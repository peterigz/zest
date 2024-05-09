#include "impl_im3d_glfw.h"
#include "im3d.h"

void zest_im3d_Initialise(zest_im3d_layer_info *im3d_layer_info) {
	ImGui_ImplGlfw_InitForVulkan((GLFWwindow*)ZestApp->window->window_handle, true);
}

void zest_im3d_CreateLayer(zest_im3d_layer_info *im3d_layer_info) {
	im3d_layer_info->mesh_layer = zest_NewMeshLayer("imgui mesh layer", sizeof(ImDrawVert));

	//Im3d pipeline
    VkRenderPass render_pass = zest_GetStandardRenderPass();
	zest_pipeline_template_create_info_t im3d_pipeline_template = zest_CreatePipelineTemplateCreateInfo();
	im3d_pipeline_template.viewport.offset.x = 0;
	im3d_pipeline_template.viewport.offset.y = 0;
	VkPushConstantRange mesh_pushconstant_range;
	mesh_pushconstant_range.size = sizeof(zest_push_constants_t);
	mesh_pushconstant_range.offset = 0;
	mesh_pushconstant_range.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
	zest_SetPipelineTemplatePushConstant(&im3d_pipeline_template, sizeof(zest_push_constants_t), 0, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
	zest_AddVertexInputBindingDescription(&im3d_pipeline_template, 0, sizeof(zest_im3d_vertex), VK_VERTEX_INPUT_RATE_VERTEX);
	VkVertexInputAttributeDescription* zest_vertex_input_attributes = 0;
	zest_vertex_input_descriptions im3d_vertice_descriptions;
	zest_AddVertexInputDescription(&im3d_vertice_descriptions, zest_CreateVertexInputDescription(0, 0, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(zest_im3d_vertex, position_size)));        // Location 0: Position
	zest_AddVertexInputDescription(&im3d_vertice_descriptions, zest_CreateVertexInputDescription(0, 1, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(zest_im3d_vertex, color)));        // Location 1: Alpha/Intensity

	im3d_pipeline_template.attributeDescriptions = zest_vertex_input_attributes;
	zest_SetText(&im3d_pipeline_template.vertShaderFile, "im3d.spv");
	zest_SetText(&im3d_pipeline_template.fragShaderFile, "im3d.spv");

	zest_pipeline im3d_pipeline = zest_AddPipeline("pipeline_im3d");

	im3d_pipeline_template.viewport.extent = zest_GetSwapChainExtent();
	im3d_pipeline->flags |= zest_pipeline_set_flag_match_swapchain_view_extent_on_rebuild;
	im3d_pipeline->descriptor_layout = zest_GetDescriptorSetLayout("Standard 1 uniform 1 sampler");
	im3d_pipeline_template.descriptorSetLayout = zest_GetDescriptorSetLayout("Standard 1 uniform 1 sampler");
	zest_MakePipelineTemplate(im3d_pipeline, render_pass, &im3d_pipeline_template);

	im3d_pipeline->pipeline_template.rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
	im3d_pipeline->pipeline_template.rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
	im3d_pipeline->pipeline_template.rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
	im3d_pipeline->pipeline_template.inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

	im3d_pipeline->pipeline_template.colorBlendAttachment = zest_ImGuiBlendState();
	im3d_pipeline->pipeline_template.depthStencil.depthTestEnable = VK_FALSE;
	im3d_pipeline->pipeline_template.depthStencil.depthWriteEnable = VK_FALSE;
	zest_BuildPipeline(im3d_pipeline);

	zest_ContextDrawRoutine()->draw_callback = zest_im3d_DrawLayer;
	zest_ContextDrawRoutine()->user_data = im3d_layer_info;
}

void zest_im3d_DrawLayer(zest_draw_routine_t *draw_routine, VkCommandBuffer command_buffer) {
	ImDrawData *im3d_draw_data = ImGui::GetDrawData();
	zest_pipeline last_pipeline = ZEST_NULL;
	VkDescriptorSet last_descriptor_set = VK_NULL_HANDLE;

	zest_im3d_layer_info *layer_info = (zest_im3d_layer_info*)draw_routine->user_data;
	assert(layer_info);	//Must set user data as layer info is referenced later

	zest_layer_t *im3d_layer = layer_info->mesh_layer;

	zest_BindMeshVertexBuffer(im3d_layer);
	zest_BindMeshIndexBuffer(im3d_layer);

	bool invalid_image_found = false;

	for (int i = 0, n = Im3d::GetDrawListCount(); i < n; ++i)
	{
		const Im3d::DrawList& drawList = Im3d::GetDrawLists()[i];

		/*
		if (drawList.m_layerId == Im3d::MakeId("NamedLayer"))
		{
			// The application may group primitives into layers, which can be used to change the draw state (e.g. enable depth testing, use a different shader)
		}
		*/

	}

		int32_t vertex_offset = 0;
		int32_t index_offset = 0;
        
        ImVec2 clip_off = im3d_draw_data->DisplayPos;
        ImVec2 clip_scale = im3d_draw_data->FramebufferScale;

		for (int32_t i = 0; i < im3d_draw_data->CmdListsCount; i++)
		{
			const ImDrawList* cmd_list = im3d_draw_data->CmdLists[i];
			for (int32_t j = 0; j < cmd_list->CmdBuffer.Size; j++)
			{
				const ImDrawCmd* pcmd = &cmd_list->CmdBuffer[j];

				zest_image_t *current_image = (zest_image_t*)pcmd->TextureId;
				if (!current_image) {
					//This means we're trying to draw a render target
					assert(pcmd->UserCallbackData);
					zest_render_target render_target = static_cast<zest_render_target>(pcmd->UserCallbackData);
					current_image = zest_GetRenderTargetImage(render_target);
				}

				if (current_image->struct_type != zest_struct_type_image) {
					//Invalid image
					ZEST_PRINT_WARNING("%s", "Invalid image found when trying to draw an imgui image. This is usually caused when a texture is changed in another thread before drawing is complete causing the image handle to become invalid due to it being freed.");
					invalid_image_found = true;
					continue;
				}

				if (last_pipeline != layer_info->pipeline || last_descriptor_set != zest_CurrentTextureDescriptorSet(current_image->texture)) {
					zest_BindPipeline(layer_info->pipeline, zest_CurrentTextureDescriptorSet(current_image->texture));
					last_descriptor_set = zest_CurrentTextureDescriptorSet(current_image->texture);
					last_pipeline = layer_info->pipeline;
				}

				im3d_layer->push_constants.parameters2.x = (float)current_image->layer;

				vkCmdPushConstants(ZestRenderer->current_command_buffer, layer_info->pipeline->pipeline_layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(zest_push_constants_t), &im3d_layer->push_constants);
                
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

void zest_im3d_UpdateBuffers(zest_layer im3d_layer) {
	if (!im3d_layer->dirty[ZEST_FIF]) {
		return;
	}

	ImDrawData *im3d_draw_data = ImGui::GetDrawData();

	zest_buffer vertex_buffer = zest_GetVertexWriteBuffer(im3d_layer);
	zest_buffer index_buffer = zest_GetIndexWriteBuffer(im3d_layer);

	if (im3d_draw_data) {
		index_buffer->memory_in_use = im3d_draw_data->TotalIdxCount * sizeof(ImDrawIdx);
		vertex_buffer->memory_in_use = im3d_draw_data->TotalVtxCount * sizeof(ImDrawVert);
	}
	else {
		index_buffer->memory_in_use = 0;
		vertex_buffer->memory_in_use = 0;
	}

	im3d_layer->push_constants.parameters1 = zest_Vec4Set(2.0f / zest_ScreenWidthf(), 2.0f / zest_ScreenHeightf(), -1.f, -1.f);
	im3d_layer->push_constants.parameters2 = zest_Vec4Set1(0.f);

	zest_buffer device_vertex_buffer = zest_GetVertexDeviceBuffer(im3d_layer);
	zest_buffer device_index_buffer = zest_GetIndexDeviceBuffer(im3d_layer);

	if (im3d_draw_data) {
		if (index_buffer->memory_in_use > index_buffer->size) {
			zest_GrowMeshIndexBuffers(im3d_layer);
			index_buffer = zest_GetIndexWriteBuffer(im3d_layer);
		}
		if (vertex_buffer->memory_in_use > vertex_buffer->size) {
			zest_GrowMeshVertexBuffers(im3d_layer);
			vertex_buffer = zest_GetVertexWriteBuffer(im3d_layer);
		}
		ImDrawIdx* idxDst = (ImDrawIdx*)index_buffer->data;
		ImDrawVert* vtxDst = (ImDrawVert*)vertex_buffer->data;

		for (int n = 0; n < im3d_draw_data->CmdListsCount; n++) {
			const ImDrawList* cmd_list = im3d_draw_data->CmdLists[n];
			memcpy(vtxDst, cmd_list->VtxBuffer.Data, cmd_list->VtxBuffer.Size * sizeof(ImDrawVert));
			memcpy(idxDst, cmd_list->IdxBuffer.Data, cmd_list->IdxBuffer.Size * sizeof(ImDrawIdx));
			vtxDst += cmd_list->VtxBuffer.Size;
			idxDst += cmd_list->IdxBuffer.Size;
		}
	}

	im3d_layer->dirty[ZEST_FIF] = 0;
}

