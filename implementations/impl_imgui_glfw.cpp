#include "impl_imgui_glfw.h"
#include "imgui_internal.h"

void zest_imgui_Initialise(zest_imgui_layer_info_t *imgui_layer_info) {
	ZEST_ASSERT(!ZEST_VALID_HANDLE(imgui_layer_info));	//imgui layer object is already initialised! Use zest_imgui_RebuildFontTexture if you need to rebuild the fonts
	ZEST_ASSERT(imgui_layer_info);	//Not a valid pointer to an imgui layer info type
	memset(imgui_layer_info, 0, sizeof(zest_imgui_layer_info_t));
	imgui_layer_info->magic = zest_INIT_MAGIC;
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO();
	io.DisplaySize = ImVec2(zest_ScreenWidthf(), zest_ScreenHeightf());
	io.DisplayFramebufferScale = ImVec2(ZestRenderer->dpi_scale, ZestRenderer->dpi_scale);
	unsigned char* pixels;
	int width, height;
	io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
	int upload_size = width * height * 4 * sizeof(char);

	zest_bitmap_t font_bitmap = zest_CreateBitmapFromRawBuffer("font_bitmap", pixels, upload_size, width, height, 4);
	imgui_layer_info->font_texture = zest_CreateTexture("imgui_font", zest_texture_storage_type_single, zest_texture_flag_none, zest_texture_format_rgba_unorm, 10);
	zest_image font_image = zest_AddTextureImageBitmap(imgui_layer_info->font_texture, &font_bitmap);
	zest_ProcessTextureImages(imgui_layer_info->font_texture);

	imgui_layer_info->descriptor_pool = zest_CreateDescriptorPoolForLayout(zest_GetDescriptorSetLayout("Combined Sampler"), ZEST_MAX_FIF, 0);

	zest_descriptor_set_t *set = &imgui_layer_info->descriptor_set;
    zest_descriptor_set_builder_t builder = zest_BeginDescriptorSetBuilder(zest_GetDescriptorSetLayout("Combined Sampler"));
	zest_AddSetBuilderCombinedImageSampler(&builder, 0, 0, imgui_layer_info->font_texture->sampler->vk_sampler, zest_GetTextureDescriptorImageInfo(imgui_layer_info->font_texture)->imageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    set->vk_descriptor_set = zest_FinishDescriptorSet(imgui_layer_info->descriptor_pool, &builder, set->vk_descriptor_set);

	io.Fonts->SetTexID((ImTextureID)font_image);
	ImGui_ImplGlfw_InitForVulkan((GLFWwindow*)ZestApp->window->window_handle, true);

    imgui_layer_info->pipeline = zest_PipelineTemplate("pipeline_imgui");

	imgui_layer_info->vertex_staging_buffer = zest_CreateStagingBuffer(1024 * 1024, 0);
	imgui_layer_info->index_staging_buffer = zest_CreateStagingBuffer(1024 * 1024, 0);
}

