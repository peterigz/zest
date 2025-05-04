#include "impl_imgui_glfw.h"
#include "imgui_internal.h"

void zest_imgui_Initialise(zest_imgui_layer_info_t *imgui_layer_info) {
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
	io.Fonts->SetTexID((ImTextureID)font_image);
	ImGui_ImplGlfw_InitForVulkan((GLFWwindow*)ZestApp->window->window_handle, true);
}

