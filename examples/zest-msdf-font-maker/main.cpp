#include "header.h"
#include "imgui_internal.h"

void InitImGuiApp(ImGuiApp *app) {

	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO();
	io.DisplaySize = ImVec2(zest_ScreenWidthf(), zest_ScreenHeightf());
	io.DisplayFramebufferScale = ImVec2(1.0f, 1.0f);
	unsigned char* pixels;
	int width, height;
	io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
	int upload_size = width * height * 4 * sizeof(char);

	zest_bitmap font_bitmap = zest_CreateBitmapFromRawBuffer("font_bitmap", pixels, upload_size, width, height, 4);
	app->imgui_font_texture_index = zest_CreateTexture("imgui_font", zest_texture_storage_type_single, zest_texture_flag_none, zest_texture_format_rgba, 10);
	zest_texture *font_texture = zest_GetTextureByName("imgui_font");
	zest_index font_image_index = zest_AddTextureImageBitmap(font_texture, &font_bitmap);
	zest_ProcessTextureImages(font_texture);
	io.Fonts->SetTexID(zest_GetImageFromTexture(font_texture, font_image_index));
	ImGui_ImplGlfw_InitForVulkan((GLFWwindow*)ZestApp->window->window_handle, true);

	app->imgui_layer_info.pipeline_index = zest_PipelineIndex("pipeline_imgui");
	zest_ModifyCommandQueue(ZestApp->default_command_queue_index);
	{
		zest_ModifyDrawCommands(ZestApp->default_render_commands_index);
		{
			app->imgui_layer_info.mesh_layer_index = zest_NewMeshLayer("imgui mesh layer", sizeof(ImDrawVert));
			zest_ContextDrawRoutine()->draw_callback = zest_imgui_DrawLayer;
			zest_ContextDrawRoutine()->data = &app->imgui_layer_info;
		}
		zest_FinishQueueSetup();
	}
}

void UpdateCallback(zest_microsecs elapsed, void *user_data) {
	//Don't forget to update the uniform buffer!
	zest_UpdateStandardUniformBuffer();
	zest_SetActiveRenderQueue(0);
	ImGuiApp *app = (ImGuiApp*)user_data;
	zest_instance_layer *sprite_layer = zest_GetInstanceLayerByIndex(0);

	ImGui_ImplGlfw_NewFrame();
	ImGui::NewFrame();
	ImGui::Begin("Test Window");
	ImGui::Text("FPS %i", ZestApp->last_fps);
	ImGui::End();
	ImGui::Render();
	zest_imgui_CopyBuffers(app->imgui_layer_info.mesh_layer_index);
}

int main(void) {

	zest_create_info create_info = zest_CreateInfo();
	zest_implglfw_SetCallbacks(&create_info);

	ImGuiApp imgui_app;

	zest_Initialise(&create_info);
	zest_SetUserData(&imgui_app);
	zest_SetUserUpdateCallback(UpdateCallback);
	InitImGuiApp(&imgui_app);

	zest_Start();

	return 0;
}