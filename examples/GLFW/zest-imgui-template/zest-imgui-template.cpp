#include "zest-imgui-template.h"
#include "imgui_internal.h"

void InitImGuiApp(ImGuiApp *app) {
	//Initialise Dear ImGui
	zest_imgui_Initialise();
	//Implement a dark style
	zest_imgui_DarkStyle();
	
	/*
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
	//zest_imgui_RebuildFontTexture(tex_width, tex_height, font_data);
	*/

	//Create a texture to load in a test image to show drawing that image in an imgui window
	app->test_texture = zest_CreateTexture("Bunny", zest_texture_storage_type_sprite_sheet, zest_texture_flag_use_filtering, zest_texture_format_rgba_unorm, 10);
	//Load in the image and add it to the texture
	app->test_image = zest_AddTextureImageFile(app->test_texture, "examples/assets/wabbit_alpha.png");
	//Process the texture so that its ready to be used
	zest_ProcessTextureImages(app->test_texture);

	//We can use a timer to only update imgui 60 times per second
	app->timer = zest_CreateTimer(60);
	app->request_graph_print = true;
	app->reset = false;
}

void UpdateCallback(zest_microsecs elapsed, void* user_data) {
	//The struct for this example app from the user data we set when initialising Zest
	ImGuiApp* app = (ImGuiApp*)user_data;

	//We can use a timer to only update the gui every 60 times a second (or whatever you decide). This
	//means that the buffers are uploaded less frequently and the command buffer is also re-recorded
	//less frequently.
	zest_StartTimerLoop(app->timer) {
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
			} else {
				zest_EnableVSync();
				app->sync_refresh = true;
			}
		}
		if (ImGui::Button("Print Render Graph")) {
			app->request_graph_print = true;
			zloc_VerifyAllRemoteBlocks(0, 0);
		}
		if (ImGui::Button("Bordered")) {
			zest_SetWindowMode(zest_GetCurrentWindow(), zest_window_mode_bordered);
		}
		if (ImGui::Button("Borderless")) {
			zest_SetWindowMode(zest_GetCurrentWindow(), zest_window_mode_borderless);
		}
		if (ImGui::Button("Full Screen")) {
			zest_SetWindowMode(zest_GetCurrentWindow(), zest_window_mode_fullscreen);
		}
		if (ImGui::Button("Set window size to 1000 x 750")) {
			zest_SetWindowSize(zest_GetCurrentWindow(), 1000, 750);
		}
		if (ImGui::Button("Reset Renderer")) {
			app->reset = true;
		}
		ImGui::Image((ImTextureID)app->test_image, ImVec2(50.f, 50.f), ImVec2(app->test_image->uv.x, app->test_image->uv.y), ImVec2(app->test_image->uv.z, app->test_image->uv.w));
		//Test for memory leaks in zest
		for (int i = 0; i != ZestDevice->memory_pool_count; ++i) {
			zloc_pool_stats_t stats = zloc_CreateMemorySnapshot(zloc__first_block_in_pool(zloc_GetPool(ZestDevice->allocator)));
			ImGui::Text("Free Blocks: %i, Used Blocks: %i", stats.free_blocks, stats.used_blocks);
			ImGui::Text("Free Memory: %zu(bytes) %zu(kb) %zu(mb), Used Memory: %zu(bytes) %zu(kb) %zu(mb)", stats.free_size, stats.free_size / 1024, stats.free_size / 1024 / 1024, stats.used_size, stats.used_size / 1024, stats.used_size / 1024 / 1024);
		}
		ImGui::End();
		ImGui::Render();
		//An imgui layer is a manual layer, meaning that you need to let it know that the buffers need updating.
		//Load the imgui mesh data into the layer staging buffers. When the command queue is recorded, it will then upload that data to the GPU buffers for rendering
		zest_imgui_UpdateBuffers();
	} zest_EndTimerLoop(app->timer);

	if (app->reset) {
		app->reset = false;
		zest_imgui_Shutdown();
		zest_ResetRenderer();
		InitImGuiApp(app);
	}

	//Begin the render graph with the command that acquires a swap chain image (zest_BeginRenderToScreen)
	//Use the render graph we created earlier. Will return false if a swap chain image could not be acquired. This will happen
	//if the window is resized for example.
	if (zest_BeginRenderToScreen(zest_GetMainWindowSwapchain(), "ImGui")) {
		VkClearColorValue clear_color = { {0.0f, 0.1f, 0.2f, 1.0f} };
		//If there was no imgui data to render then zest_imgui_AddToRenderGraph will return false
		//Import our test texture with the Bunny sprite
		zest_resource_node test_texture = zest_ImportImageResourceReadOnly("test texture", app->test_texture);
		//------------------------ ImGui Pass ----------------------------------------------------------------
		//If there's imgui to draw then draw it
		zest_pass_node imgui_pass = zest_imgui_AddToRenderGraph();
		if (imgui_pass) {
			zest_ConnectSampledImageInput(imgui_pass, test_texture);
			zest_ConnectSwapChainOutput(imgui_pass, clear_color);
		} else {
			//If there's no ImGui to render then just render a blank screen
			zest_pass_node blank_pass = zest_AddGraphicBlankScreen("Draw Nothing");
			//Add the swap chain as an output to the imgui render pass. This is telling the render graph where it should render to.
			zest_ConnectSwapChainOutput(blank_pass, clear_color);
		}
		//----------------------------------------------------------------------------------------------------
		//End the render graph and execute it. This will submit it to the GPU.
		zest_render_graph render_graph = zest_EndRenderGraph();
		if (app->request_graph_print) {
			//You can print out the render graph for debugging purposes
			//zest_PrintCompiledRenderGraph(render_graph);
			app->request_graph_print = false;
		}
	}
}

#if defined(_WIN32)
// Windows entry point
//int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR pCmdLine, int nCmdShow) {
int main(void) {
	//Create new config struct for Zest
	zest_create_info_t create_info = zest_CreateInfoWithValidationLayers(0);
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
