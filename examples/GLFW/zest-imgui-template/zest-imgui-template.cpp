#include "zest-imgui-template.h"
#include "imgui_internal.h"

void InitImGuiApp(ImGuiApp *app) {
	//Initialise Dear ImGui
	zest_imgui_Initialise();
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
	zest_imgui_RebuildFontTexture(tex_width, tex_height, font_data);

	//Create a texture to load in a test image to show drawing that image in an imgui window
	app->test_texture = zest_CreateTexture("Bunny", zest_texture_storage_type_sprite_sheet, zest_texture_flag_use_filtering, zest_texture_format_rgba_unorm, 10);
	//Load in the image and add it to the texture
	app->test_image = zest_AddTextureImageFile(app->test_texture, "examples/assets/wabbit_alpha.png");
	//Process the texture so that its ready to be used
	zest_ProcessTextureImages(app->test_texture);

	//Create a descriptor set for imgui to use when drawing from the Bunny texture
	app->test_descriptor_set = VK_NULL_HANDLE;

	app->render_graph = zest_NewRenderGraph("ImGui", false);

	app->timer = zest_CreateTimer(60);
}

void UpdateCallback(zest_microsecs elapsed, void* user_data) {
	//Set the active command queue to the default one that was created when Zest was initialised
	ImGuiApp* app = (ImGuiApp*)user_data;

	//We can use a timer to only update the gui every 60 times a second (or whatever you decide). This
	//means that the buffers are uploaded less frequently and the command buffer is also re-recorded
	//less frequently.
	zest_StartTimerLoop(app->timer) {
		//Must call the imgui GLFW implementation function
		ImGui_ImplGlfw_NewFrame();
		//Draw our imgui stuff
		ImGui::NewFrame();
		//ImGui::ShowDemoWindow();
		ImGui::Begin("Test Window");
		ImGui::Text("FPS %i", ZestApp->last_fps);
		if (ImGui::Button("Toggle Refresh Rate Sync")) {
			if (app->sync_refresh) {
				zest_DisableVSync();
				app->sync_refresh = false;
			} else {
				zest_EnableVSync();
				app->sync_refresh = true;
			}
		}
		if (ImGui::Button("Print Render Graph")) {
			app->request_graph_print = true;
		}
		ImGui::Image((ImTextureID)app->test_image, ImVec2(50.f, 50.f), ImVec2(app->test_image->uv.x, app->test_image->uv.y), ImVec2(app->test_image->uv.z, app->test_image->uv.w));
		/*
		for (int i = 0; i != ZestDevice->memory_pool_count; ++i) {
			zloc_pool_stats_t stats = zloc_CreateMemorySnapshot(zloc__first_block_in_pool(zloc_GetPool(ZestDevice->allocator)));
			ImGui::Text("Free Blocks: %i, Used Blocks: %i", stats.free_blocks, stats.used_blocks);
			ImGui::Text("Free Memory: %zu(bytes) %zu(kb) %zu(mb), Used Memory: %zu(bytes) %zu(kb) %zu(mb)", stats.free_size, stats.free_size / 1024, stats.free_size / 1024 / 1024, stats.used_size, stats.used_size / 1024, stats.used_size / 1024 / 1024);
		}
		*/
		//zest_imgui_DrawImage(app->test_image, app->test_descriptor_set, 50.f, 50.f);
		ImGui::End();
		ImGui::Render();
		//An imgui layer is a manual layer, meaning that you need to let it know that the buffers need updating.
		//Load the imgui mesh data into the layer staging buffers. When the command queue is recorded, it will then upload that data to the GPU buffers for rendering
		zest_imgui_UpdateBuffers();
	} zest_EndTimerLoop(app->timer);

	ImGui::GetDrawData();

	if (zest_BeginRenderToScreen(app->render_graph)) {
		VkClearColorValue clear_color = { {0.0f, 0.1f, 0.2f, 1.0f} };
		zest_rg_resource_node swapchain_output_resource = zest_ImportSwapChainResource(app->render_graph, "Swapchain Output");
		zest_rg_pass_node imgui_pass = zest_imgui_AddToRenderGraph(app->render_graph);
		//If there was no imgui data to render then zest_imgui_AddToRenderGraph will return null
		if (imgui_pass) {
			zest_rg_resource_node test_texture = zest_ImportImageResourceReadOnly(app->render_graph, "test texture", app->test_texture);
			zest_AddPassSampledImageInput(imgui_pass, test_texture, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
			zest_AddPassSwapChainOutput(imgui_pass, swapchain_output_resource, clear_color);
		} else {
			//Just render a blank screen if imgui didn't render anything
			zest_rg_pass_node blank_pass = zest_AddGraphicBlankScreen(app->render_graph, "Draw Nothing");
			zest_AddPassSwapChainOutput(blank_pass, swapchain_output_resource, clear_color);
		}
		zest_EndRenderGraph(app->render_graph);
		if (app->request_graph_print) {
			zest_PrintCompiledRenderGraph(app->render_graph);
			app->request_graph_print = false;
		}
		zest_ExecuteRenderGraph(app->render_graph);
	}
}

#if defined(_WIN32)
// Windows entry point
//int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR pCmdLine, int nCmdShow) {
int main(void) {
	//Create new config struct for Zest
	zest_create_info_t create_info = zest_CreateInfoWithValidationLayers();
    create_info.log_path = ".";
	ZEST__FLAG(create_info.flags, zest_init_flag_log_validation_errors_to_console);
	//Implement GLFW for window creation
	zest_implglfw_SetCallbacks(&create_info);

	ImGuiApp imgui_app = {};

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
