#include "zest-sdl2-imgui-template.h"
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

	app->timer = zest_CreateTimer(60.0);
}

void UpdateCallback(zest_microsecs elapsed, void *user_data) {
	SDL_Event event;
	while (SDL_PollEvent(&event)) {
		ImGui_ImplSDL2_ProcessEvent(&event);
		zest_MaybeQuit(event.type == SDL_QUIT);
		zest_MaybeQuit(event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_CLOSE && event.window.windowID == SDL_GetWindowID((SDL_Window*)ZestApp->current_window->window_handle));
	}
	//Don't forget to update the uniform buffer!
	zest_Update2dUniformBuffer();
	//Set the active command queue to the default one that was created when Zest was initialised
	ImGuiApp *app = (ImGuiApp*)user_data;

	//We can use a timer to only update the gui every 60 times a second (or whatever you decide). This
	//means that the buffers are uploaded less frequently and the command buffer is also re-recorded
	//less frequently.
	zest_StartTimerLoop(app->timer) {
		//Must call the imgui GLFW implementation function
		ImGui_ImplSDL2_NewFrame();
		//Draw our imgui stuff
		ImGui::NewFrame();
		ImGui::ShowDemoWindow();
		ImGui::Begin("Test Window");
		ImGui::Text("FPS %i", ZestApp->last_fps);
		ImGui::Image((ImTextureID)app->test_image, ImVec2(50.f, 50.f), ImVec2(app->test_image->uv.x, app->test_image->uv.y), ImVec2(app->test_image->uv.z, app->test_image->uv.w));
		ImGui::End();
		ImGui::Render();
		//Load the imgui mesh data into the layer staging buffers. When the command queue is recorded, it will then upload that data to the GPU buffers for rendering
		zest_imgui_UpdateBuffers();
	} zest_EndTimerLoop(app->timer);

	//Begin the render graph with the command that acquires a swap chain image (zest_BeginRenderToScreen)
//Use the render graph we created earlier. Will return false if a swap chain image could not be acquired. This will happen
//if the window is resized for example.
	if (zest_BeginRenderToScreen("ImGui")) {
		VkClearColorValue clear_color = { {0.0f, 0.1f, 0.2f, 1.0f} };
		//Import the swap chain into the render pass
		zest_resource_node swapchain_output_resource = zest_ImportSwapChainResource("Swapchain Output");
		zest_pass_node graphics_pass = zest_AddRenderPassNode("Graphics Pass");
		//If there was no imgui data to render then zest_imgui_AddToRenderGraph will return false
		//Import our test texture with the Bunny sprite
		zest_resource_node test_texture = zest_ImportImageResourceReadOnly("test texture", app->test_texture);
		//Add the test texture to the imgui render pass
		zest_ConnectSampledImageInput(graphics_pass, test_texture, zest_pipeline_fragment_stage);
		//If there's imgui to draw then draw it
		if (zest_imgui_AddToRenderGraph(graphics_pass)) {
			//Imgui won't draw anything unless we add a callback with zest_AddPassTask. This has to be done manually rather then
			//taken care of in the zest_imgui_AddToRenderGraph so that you have the flexibility to draw other things to the swap chain
			//or other render target in the order that you want.
			zest_AddPassTask(graphics_pass, zest_imgui_DrawImGuiRenderPass, app);
			//Add the swap chain as an output to the imgui render pass. This is telling the render graph where it should render to.
			zest_ConnectSwapChainOutput(graphics_pass, swapchain_output_resource, clear_color);
		} else {
			//If there's no ImGui to render then just render a blank screen
			zest_pass_node blank_pass = zest_AddGraphicBlankScreen("Draw Nothing");
			//Add the swap chain as an output to the imgui render pass. This is telling the render graph where it should render to.
			zest_ConnectSwapChainOutput(blank_pass, swapchain_output_resource, clear_color);
		}
		//Compile and execute the render graph. T
		zest_render_graph render_graph = zest_EndRenderGraph();
		if (app->request_graph_print) {
			//You can print out the render graph for debugging purposes
			zest_PrintCompiledRenderGraph(render_graph);
			app->request_graph_print = false;
		}
	}

}

#if defined(_WIN32)
// Windows entry point
int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR pCmdLine, int nCmdShow) {
//int main(void) {
	//Create new config struct for Zest
	zest_create_info_t create_info = zest_CreateInfo();
	//Don't enable vsync so we can see the FPS go higher then the refresh rate
	ZEST__UNFLAG(create_info.flags, zest_init_flag_enable_vsync);
	//Implement GLFW for window creation
	zest_implsdl2_SetCallbacks(&create_info);

	ImGuiApp imgui_app = { 0 };

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
	zest_implsdl2_SetCallbacks(&create_info);

	ImGuiApp imgui_app;

	zest_Initialise(&create_info);
	zest_SetUserData(&imgui_app);
	zest_SetUserUpdateCallback(UpdateCallback);
	InitImGuiApp(&imgui_app);

	zest_Start();

	return 0;
}
#endif
