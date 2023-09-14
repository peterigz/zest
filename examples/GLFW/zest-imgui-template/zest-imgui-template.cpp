#include "zest-imgui-template.h"
#include "imgui_internal.h"

void InitImGuiApp(ImGuiApp *app) {

	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO();
	io.DisplaySize = ImVec2(zest_SwapChainWidthf(), zest_SwapChainHeightf());
	io.DisplayFramebufferScale = ImVec2(1.0f, 1.0f);
	unsigned char* pixels;
	int width, height;
	io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
	int upload_size = width * height * 4 * sizeof(char);

	zest_bitmap_t font_bitmap = zest_CreateBitmapFromRawBuffer("font_bitmap", pixels, upload_size, width, height, 4);
	app->imgui_font_texture = zest_CreateTexture("imgui_font", zest_texture_storage_type_single, zest_texture_flag_none, zest_texture_format_rgba, 10);
	zest_texture font_texture = zest_GetTexture("imgui_font");
	zest_image font_image = zest_AddTextureImageBitmap(font_texture, &font_bitmap);
	zest_ProcessTextureImages(font_texture);
	io.Fonts->SetTexID(font_image);
	ImGui_ImplGlfw_InitForVulkan((GLFWwindow*)ZestApp->window->window_handle, true);

	app->test_texture = zest_CreateTexture("Bunny", zest_texture_storage_type_sprite_sheet, zest_texture_flag_use_filtering, zest_texture_format_rgba, 10);
	app->test_image = zest_AddTextureImageFile(app->test_texture, "examples/assets/wabbit_alpha.png");
	zest_ProcessTextureImages(app->test_texture);

	app->imgui_layer_info.pipeline = zest_Pipeline("pipeline_imgui");
	zest_ModifyCommandQueue(ZestApp->default_command_queue);
	{
		zest_ModifyDrawCommands(ZestApp->default_draw_commands);
		{
			app->imgui_layer_info.mesh_layer = zest_NewMeshLayer("imgui mesh layer", sizeof(ImDrawVert));
			zest_ContextDrawRoutine()->draw_callback = zest_imgui_DrawLayer;
			zest_ContextDrawRoutine()->user_data = &app->imgui_layer_info;
		}
		zest_FinishQueueSetup();
	}
}

void UpdateCallback(zest_microsecs elapsed, void *user_data) {
	//Don't forget to update the uniform buffer!
	zest_Update2dUniformBuffer();
	zest_SetActiveCommandQueue(ZestApp->default_command_queue);
	ImGuiApp *app = (ImGuiApp*)user_data;

	ImGui_ImplGlfw_NewFrame();
	ImGui::NewFrame();
	ImGui::ShowDemoWindow();
	ImGui::Begin("Test Window");
	ImGui::Text("FPS %i", ZestApp->last_fps);
	zest_imgui_DrawImage(app->test_image, 50.f, 50.f);
	ImGui::End();
	ImGui::Render();
	zest_imgui_CopyBuffers(app->imgui_layer_info.mesh_layer);
}

#if defined(_WIN32)
// Windows entry point
int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR pCmdLine, int nCmdShow) {
	zest_create_info_t create_info = zest_CreateInfo();
	ZEST__UNFLAG(create_info.flags, zest_init_flag_enable_vsync);
	zest_implglfw_SetCallbacks(&create_info);

	ImGuiApp imgui_app;

	zest_Initialise(&create_info);
	zest_SetUserData(&imgui_app);
	zest_SetUserUpdateCallback(UpdateCallback);
	InitImGuiApp(&imgui_app);

	zest_Start();

	return 0;
}
#else
int main(void) {
	zest_create_info_t create_info = zest_CreateInfo();
	zest_implglfw_SetCallbacks(&create_info);

	ImGuiApp imgui_app;

	zest_Initialise(&create_info);
	zest_SetUserData(&imgui_app);
	zest_SetUserUpdateCallback(UpdateCallback);
	InitImGuiApp(&imgui_app);

	zest_Start();

	return 0;
}
#endif
