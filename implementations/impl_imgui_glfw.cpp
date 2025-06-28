#include "impl_imgui_glfw.h"
#include "imgui_internal.h"

zest_imgui zest_imgui_Initialise() {
	zest_imgui imgui_info = &ZestRenderer->imgui_info;
	ZEST_ASSERT(!imgui_info->vertex_staging_buffer);	//imgui already initialised!
	ZEST_ASSERT(!imgui_info->index_staging_buffer);
	memset(imgui_info, 0, sizeof(zest_imgui_t));
	imgui_info->magic = zest_INIT_MAGIC;
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO();
	io.DisplaySize = ImVec2(zest_ScreenWidthf(), zest_ScreenHeightf());
	io.DisplayFramebufferScale = ImVec2(ZestRenderer->dpi_scale, ZestRenderer->dpi_scale);
	unsigned char* pixels;
	int width, height;
	io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
	int upload_size = width * height * 4 * sizeof(char);

	zest_bitmap_t font_bitmap = zest_CreateBitmapFromRawBuffer("font_bitmap", pixels, upload_size, width, height, 4);
	imgui_info->font_texture = zest_CreateTexture("imgui_font", zest_texture_storage_type_single, zest_texture_flag_none, zest_texture_format_rgba_unorm, 10);
	zest_image font_image = zest_AddTextureImageBitmap(imgui_info->font_texture, &font_bitmap);
	zest_ProcessTextureImages(imgui_info->font_texture);

	//ImGuiPipeline
	zest_pipeline_template imgui_pipeline = zest_BeginPipelineTemplate("pipeline_imgui");
	imgui_pipeline->scissor.offset.x = 0;
	imgui_pipeline->scissor.offset.y = 0;
	zest_SetPipelinePushConstantRange(imgui_pipeline, sizeof(zest_push_constants_t), zest_shader_render_stages);
	zest_AddVertexInputBindingDescription(imgui_pipeline, 0, sizeof(zest_ImDrawVert_t), VK_VERTEX_INPUT_RATE_VERTEX);
	zest_AddVertexAttribute(imgui_pipeline, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(zest_ImDrawVert_t, pos));    // Location 0: Position
	zest_AddVertexAttribute(imgui_pipeline, 1, VK_FORMAT_R32G32_SFLOAT, offsetof(zest_ImDrawVert_t, uv));    // Location 1: UV
	zest_AddVertexAttribute(imgui_pipeline, 2, VK_FORMAT_R8G8B8A8_UNORM, offsetof(zest_ImDrawVert_t, col));    // Location 2: Color

	zest_SetText(&imgui_pipeline->vertShaderFile, "imgui_vert.spv");
	zest_SetText(&imgui_pipeline->fragShaderFile, "imgui_frag.spv");

	imgui_pipeline->scissor.extent = zest_GetSwapChainExtent();
	imgui_pipeline->flags |= zest_pipeline_set_flag_match_swapchain_view_extent_on_rebuild;
	zest_ClearPipelineDescriptorLayouts(imgui_pipeline);
	zest_AddPipelineDescriptorLayout(imgui_pipeline, ZestRenderer->texture_debug_layout->vk_layout);
	zest_EndPipelineTemplate(imgui_pipeline);

	imgui_pipeline->rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
	imgui_pipeline->rasterizer.cullMode = VK_CULL_MODE_NONE;
	imgui_pipeline->rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
	imgui_pipeline->inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

	imgui_pipeline->colorBlendAttachment = zest_ImGuiBlendState();
	imgui_pipeline->depthStencil.depthTestEnable = VK_FALSE;
	imgui_pipeline->depthStencil.depthWriteEnable = VK_FALSE;
	ZEST_APPEND_LOG(ZestDevice->log_path.str, "ImGui pipeline");

	io.Fonts->SetTexID((ImTextureID)font_image);
	ImGui_ImplGlfw_InitForVulkan((GLFWwindow*)ZestApp->window->window_handle, true);

    imgui_info->pipeline = imgui_pipeline;

	imgui_info->vertex_staging_buffer = zest_CreateFrameStagingBuffer(1024 * 1024);
	imgui_info->index_staging_buffer = zest_CreateFrameStagingBuffer(1024 * 1024);
	zest_ForEachFrameInFlight(fif) {
		imgui_info->vertex_device_buffer[fif] = zest_CreateUniqueVertexBuffer(1024 * 1024, fif, 0xDEA41);
		imgui_info->index_device_buffer[fif] = zest_CreateUniqueIndexBuffer(1024 * 1024, fif, 0xDEA41);
	}

	return imgui_info;
}

