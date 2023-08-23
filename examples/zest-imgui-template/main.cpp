#include "zest_imgui.h"

zest_window *CreateWindowCallback(int x, int y, int width, int height, zest_bool maximised, const char* title) {
	ZEST_ASSERT(ZestDevice);        //Must initialise the ZestDevice first

	zest_window *window = zest_AllocateWindow();
	
	glfwInit();

	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	if (maximised)
		glfwWindowHint(GLFW_MAXIMIZED, GLFW_TRUE);

	window->window_width = width;
	window->window_height = height;

	window->window_handle = glfwCreateWindow(width, height, title, 0, 0);
	if (!maximised) {
		glfwSetWindowPos((GLFWwindow*)window->window_handle, x, y);
	}
	glfwSetWindowUserPointer((GLFWwindow*)window->window_handle, ZestApp);

	if (maximised) {
		int width, height;
		glfwGetFramebufferSize((GLFWwindow*)window->window_handle, &width, &height);
		window->window_width = width;
		window->window_height = height;
	}

	return window;
}

void CreateWindowSurfaceCallback(zest_window* window) {
	ZEST_VK_CHECK_RESULT(glfwCreateWindowSurface(ZestDevice->instance, (GLFWwindow*)window->window_handle, &ZestDevice->allocation_callbacks, &window->surface));
}

void PollEventsCallback(void) {
	glfwPollEvents();
	zest_MaybeQuit(glfwWindowShouldClose((GLFWwindow*)ZestApp->window->window_handle));
}

void AddPlatformExtensionsCallback(void) {
	zest_uint count;
	char **extensions = 0;
	const char **glfw_extensions = glfwGetRequiredInstanceExtensions(&count);
	for (int i = 0; i != count; ++i) {
		zest_AddInstanceExtension((char*)glfw_extensions[i]);
	}
}

void GetWindowSizeCallback(void *user_data, int *width, int *height) {
	glfwGetFramebufferSize((GLFWwindow*)ZestApp->window->window_handle, width, height);
}

void DestroyWindowCallback(void *user_data) {
	glfwDestroyWindow((GLFWwindow*)ZestApp->window->window_handle);
}

void InitImGuiApp(ImGuiApp *app) {
	app->imgui_pipeline = zest_PipelineIndex("pipeline_imgui");

	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO();
	io.DisplaySize = ImVec2(zest_ScreenWidthf(), zest_ScreenHeightf());
	io.DisplayFramebufferScale = ImVec2(1.0f, 1.0f);
	unsigned char* pixels;
	int width, height;
	io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
	int upload_size = width * height * 4 * sizeof(char);

	zest_bitmap font_bitmap = zest_CreateBitmapFromRawBuffer("font_bitmap", pixels, upload_size, width, height, 4);
	app->imgui_font_texture_index = zest_CreateTexture("imgui_font", zest_texture_storage_type_single, zest_texture_flag_none, 10);
	zest_texture *font_texture = zest_GetTextureByName("imgui_font");
	zest_index font_image_index = zest_LoadImageBitmap(font_texture, &font_bitmap);
	zest_ProcessTextureImages(font_texture);
	io.Fonts->SetTexID(zest_GetImageFromTexture(font_texture, font_image_index));
	ImGui_ImplGlfw_InitForVulkan((GLFWwindow*)ZestApp->window->window_handle, true);

	zest_AppendCommandQueue(ZestApp->default_command_queue_index);
	{
		zest_AppendRenderPassSetup(ZestApp->default_render_commands_index);
		{
			app->imgui_layer_index = zest_NewMeshLayer("imgui mesh layer", sizeof(ImDrawVert), 10000);
			zest_ContextDrawRoutine()->draw_callback = DrawImGuiLayer;
			zest_ContextDrawRoutine()->data = app;
		}
		zest_FinishQueueSetup();
	}
}

void DrawImGuiLayer(zest_draw_routine *draw_routine, VkCommandBuffer command_buffer) {
	ImDrawData *imgui_draw_data = ImGui::GetDrawData();
	zest_index last_pipeline_index = -1;

	ImGuiApp *app = (ImGuiApp*)draw_routine->data;
	zest_mesh_layer *imgui_layer = zest_GetMeshLayerByIndex(app->imgui_layer_index);

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

				zest_image *current_image = (zest_image*)pcmd->TextureId;
				if (!current_image) {
					//This means we're trying to draw a render target
					assert(pcmd->UserCallbackData);
					//QulkanRenderTarget *render_target = static_cast<QulkanRenderTarget*>(pcmd->UserCallbackData);
					//current_image = &render_target->GetRenderTargetImage();
				}
				zest_texture *texture = zest_GetTextureByIndex(current_image->texture_index);
				zest_index current_pipeline = app->imgui_pipeline;

				if (last_pipeline_index != current_pipeline) {
					zest_BindPipeline(command_buffer, zest_Pipeline(current_pipeline), zest_GetTextureDescriptorSet(texture, 0));
					last_pipeline_index = current_pipeline;
				}

				imgui_layer->push_constants.parameters2.x = current_image->layer;
				imgui_layer->push_constants.flags = 0;

				zest_SendStandardPushConstants(zest_Pipeline(current_pipeline), &imgui_layer->push_constants);

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

void CopyImGuiBuffers(ImGuiApp *app) {
	ImDrawData *imgui_draw_data = ImGui::GetDrawData();
	zest_mesh_layer *imgui_layer = zest_GetMeshLayerByIndex(app->imgui_layer_index);

	zest_buffer *vertex_buffer = zest_GetVertexStagingBuffer(imgui_layer);
	zest_buffer *index_buffer = zest_GetIndexStagingBuffer(imgui_layer);

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

	zest_buffer *device_vertex_buffer = zest_GetVertexDeviceBuffer(imgui_layer);
	zest_buffer *device_index_buffer = zest_GetIndexDeviceBuffer(imgui_layer);

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

void UpdateCallback(zest_microsecs elapsed, void *user_data) {
	//Don't forget to update the uniform buffer!
	zest_UpdateStandardUniformBuffer();
	zest_SetActiveRenderQueue(0);
	ImGuiApp *app = (ImGuiApp*)user_data;
	zest_instance_layer *sprite_layer = zest_GetInstanceLayerByIndex(0);
	zest_texture *texture = zest_GetTextureByIndex(app->imgui_font_texture_index);
	zest_SetSpriteDrawing(sprite_layer, texture, 0, 0);
	sprite_layer->multiply_blend_factor = 1.f;
	zest_DrawSprite(sprite_layer, zest_GetImageFromTexture(texture, 0), 20.f, 20.f, 0, 512.f, 64.f, 0.f, 0.f, 0, 0.f, 0);

	ImGui_ImplGlfw_NewFrame();
	ImGui::NewFrame();
	ImGui::ShowDemoWindow();
	ImGui::Render();
	CopyImGuiBuffers(app);
}

int main(void) {

	zest_create_info create_info = zest_CreateInfo();

	create_info.add_platform_extensions_callback = AddPlatformExtensionsCallback;
	create_info.create_window_callback = CreateWindowCallback;
	create_info.poll_events_callback = PollEventsCallback;
	create_info.get_window_size_callback = GetWindowSizeCallback;
	create_info.destroy_window_callback = DestroyWindowCallback;
	create_info.create_window_surface_callback = CreateWindowSurfaceCallback;

	ImGuiApp imgui_app;

	zest_Initialise(&create_info);
	zest_SetUserData(&imgui_app);
	zest_SetUserUpdateCallback(UpdateCallback);
	InitImGuiApp(&imgui_app);

	zest_Start();

	return 0;
}