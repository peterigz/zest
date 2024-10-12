#include "zest-imgui-template.h"
#include "imgui_internal.h"

void InitImGuiApp(ImGuiApp *app) {
	//Initialise Dear ImGui
	zest_imgui_Initialise(&app->imgui_layer_info);
	//Implement a dark style
	zest_imgui_DarkStyle();
	
	//This is an exmaple of how to change the font that ImGui uses
	ImGuiIO& io = ImGui::GetIO();
	io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
	io.Fonts->Clear();
	float font_size = 15.f;
	unsigned char *font_data;
	int tex_width, tex_height;
	ImFontConfig config;
	config.PixelSnapH = true;
	io.Fonts->AddFontFromFileTTF("examples/assets/Lato-Regular.ttf", font_size);
	io.Fonts->GetTexDataAsRGBA32(&font_data, &tex_width, &tex_height);

	//Rebuild the Zest font texture
	zest_imgui_RebuildFontTexture(&app->imgui_layer_info, tex_width, tex_height, font_data);

	//Create a texture to load in a test image to show drawing that image in an imgui window
	app->test_texture = zest_CreateTexture("Bunny", zest_texture_storage_type_sprite_sheet, zest_texture_flag_use_filtering, zest_texture_format_rgba, 10);
	//Load in the image and add it to the texture
	app->test_image = zest_AddTextureImageFile(app->test_texture, "examples/assets/wabbit_alpha.png");
	//Process the texture so that its ready to be used
	zest_ProcessTextureImages(app->test_texture);

	app->sync_refresh = true;

	//Modify the existing default queue
	zest_ModifyCommandQueue(ZestApp->default_command_queue);
	{
		zest_ModifyDrawCommands(ZestApp->default_draw_commands);
		{
			//Create a Dear ImGui layer
			zest_imgui_CreateLayer(&app->imgui_layer_info);
		}
		//Don't forget to finish the queue set up
		zest_FinishQueueSetup();
	}

}

void UpdateCallback(zest_microsecs elapsed, void* user_data) {
	//Don't forget to update the uniform buffer!
	zest_Update2dUniformBuffer();
	//Set the active command queue to the default one that was created when Zest was initialised
	zest_SetActiveCommandQueue(ZestApp->default_command_queue);
	ImGuiApp* app = (ImGuiApp*)user_data;

	//Must call the imgui GLFW implementation function
	ImGui_ImplGlfw_NewFrame();
	//Draw our imgui stuff
	ImGui::NewFrame();
	ImGui::ShowDemoWindow();
	ImGui::Begin("Test Window");
	ImGui::Text("FPS %i", ZestApp->last_fps);
	if (ImGui::Button("Toggle Refresh Rate Sync")) {
		if (app->sync_refresh) {
			zest_DisableVSync();
			app->sync_refresh = false;
		}
		else {
			zest_EnableVSync();
			app->sync_refresh = true;
		}
	}
	ImGui::Image(app->test_image, ImVec2(50.f, 50.f), ImVec2(app->test_image->uv.x, app->test_image->uv.y), ImVec2(app->test_image->uv.z, app->test_image->uv.w));
	//zest_imgui_DrawImage(app->test_image, 50.f, 50.f);
	ImGui::End();
	ImGui::Render();
	//An imgui layer is a manual layer, meaning that you need to let it know that the buffers need updating.
	zest_ResetLayer(app->imgui_layer_info.mesh_layer);
	//Load the imgui mesh data into the layer staging buffers. When the command queue is recorded, it will then upload that data to the GPU buffers for rendering
	zest_imgui_UpdateBuffers(app->imgui_layer_info.mesh_layer);
}

#if defined(_WIN32)
// Windows entry point
int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR pCmdLine, int nCmdShow) {
//int main(void) {
	//Create new config struct for Zest
	zest_create_info_t create_info = zest_CreateInfo();
	//Don't enable vsync so we can see the FPS go higher then the refresh rate
	//ZEST__UNFLAG(create_info.flags, zest_init_flag_enable_vsync);
	//Implement GLFW for window creation
	zest_implglfw_SetCallbacks(&create_info);

	ImGuiApp imgui_app;

	//Initialise Zest
	zest_Initialise(&create_info);
	//Set the Zest use data
	zest_SetUserData(&imgui_app);
	//Set the udpate callback to be called every frame
	zest_SetUserUpdateCallback(UpdateCallback);
	//Initialise our example
	InitImGuiApp(&imgui_app);

	//Start the main loop
	zest_Start();

	return 0;
}
#else
int main(void) {
	zest_create_info_t create_info = zest_CreateInfo();
	zest_implglfw_SetCallbacks(&create_info);
    ZEST__FLAG(create_info.flags, zest_init_flag_maximised);

	ImGuiApp imgui_app;

    create_info.log_path = ".";
	zest_Initialise(&create_info);
	zest_SetUserData(&imgui_app);
	zest_SetUserUpdateCallback(UpdateCallback);
	InitImGuiApp(&imgui_app);

	zest_Start();

	return 0;
}
#endif
