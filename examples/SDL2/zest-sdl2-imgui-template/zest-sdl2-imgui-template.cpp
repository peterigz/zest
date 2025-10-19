#define ZEST_IMPLEMENTATION
#define ZEST_VULKAN_IMPLEMENTATION
#include <zest.h>
#include "zest-sdl2-imgui-template.h"
#include "zest_utilities.h"
#include "imgui_internal.h"

void InitImGuiApp(ImGuiApp *app) {
	//Initialise Dear ImGui
	app->imgui = zest_imgui_Initialise(app->context);
    ImGui_ImplSDL2_InitForVulkan((SDL_Window *)zest_Window(app->context));

	//Implement a dark style
	zest_imgui_DarkStyle();
	
	//This is an example of how to change the font that ImGui uses
	ImGuiIO& io = ImGui::GetIO();
	io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
	io.Fonts->Clear();
	float font_size = 16.f;
	unsigned char *font_data;
	int tex_width, tex_height;
	ImFontConfig config;
	config.PixelSnapH = true;
	io.Fonts->AddFontFromFileTTF("examples/assets/Lato-Regular.ttf", font_size);
	io.Fonts->GetTexDataAsRGBA32(&font_data, &tex_width, &tex_height);

	//Rebuild the Zest font texture
	zest_imgui_RebuildFontTexture(&app->imgui, tex_width, tex_height, font_data);

	zest_image_collection_handle atlas = zest_CreateImageAtlasCollection(app->context, zest_format_r8g8_unorm);
	app->wabbit_sprite = zest_AddImageAtlasPNG(atlas, "examples/assets/wabbit_alpha.png", "wabbit_alpha");
	zest_image_handle image_atlas = zest_CreateImageAtlas(atlas, 1024, 1024, 0);
    zest_sampler_handle sampler = zest_CreateSamplerForImage(image_atlas);
	app->atlas_binding_index = zest_AcquireGlobalSampledImageIndex(image_atlas, zest_texture_2d_binding);
	app->atlas_sampler_binding_index = zest_AcquireGlobalSamplerIndex(sampler);
	zest_BindAtlasRegionToImage(app->wabbit_sprite, app->atlas_sampler_binding_index, image_atlas, zest_texture_2d_binding);
	//free(pixels);
	//Create a texture to load in a test image to show drawing that image in an imgui window
	//app->test_texture = zest_CreateTexture("Bunny", zest_texture_storage_type_sprite_sheet, zest_image_flag_use_filtering, zest_format_r8g8b8a8_unorm, 10);
	//Load in the image and add it to the texture
	//app->test_image = zest_AddTextureImageFile(app->test_texture, "examples/assets/wabbit_alpha.png");
	//Process the texture so that its ready to be used
	//zest_ProcessTextureImages(app->test_texture);

	shaderc_compiler_t compiler = shaderc_compiler_initialize();
	app->imgui_sprite_shader = zest_CreateShader(app->context, zest_shader_imgui_r8g8_frag, shaderc_fragment_shader, "imgui_sprite_frag", ZEST_TRUE, compiler, 0);
	shaderc_compiler_release(compiler);

	app->imgui_sprite_pipeline = zest_CopyPipelineTemplate(app->context, "ImGui Sprite Pipeline", app->imgui.pipeline);
	zest_SetPipelineFragShader(app->imgui_sprite_pipeline, app->imgui_sprite_shader);


	//We can use a timer to only update imgui 60 times per second
	app->timer = zest_CreateTimer(app->context, 60);
	app->request_graph_print = true;
	app->reset = false;
}

void ImGuiSpriteDrawCallback(const ImDrawList* parent_list, const ImDrawCmd* cmd) {
	zest_imgui_callback_data_t *data = (zest_imgui_callback_data_t *)cmd->UserCallbackData;
	ImGuiApp *app = (ImGuiApp*)data->user_data;
	data->render_state->pipeline = zest_PipelineWithTemplate(app->imgui_sprite_pipeline, data->command_list);
	data->render_state->resources = app->imgui.font_resources;
	return;
}

void MainLoop(ImGuiApp *app) {
	int running = 1;
	SDL_Event event;

	while (running) {
		while (SDL_PollEvent(&event)) {
			ImGui_ImplSDL2_ProcessEvent(&event);
			if (event.type == SDL_QUIT) {
				running = 0;
			}
			if (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_CLOSE && event.window.windowID == SDL_GetWindowID((SDL_Window*)zest_Window(app->context))) {
				running = 0;
			}
		}
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
			if (ImGui::Button("Toggle Refresh Rate Sync")) {
				if (app->sync_refresh) {
					zest_DisableVSync(app->context);
					app->sync_refresh = false;
				} else {
					zest_EnableVSync(app->context);
					app->sync_refresh = true;
				}
			}
			if (ImGui::Button("Print Render Graph")) {
				app->request_graph_print = true;
				zloc_VerifyAllRemoteBlocks(app->context, 0, 0);
			}
			if (ImGui::Button("Reset Renderer")) {
				app->reset = true;
			}
			/*
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
			*/
			/*
		if (ImGui::Button("Glow Image")) {
			zest_FreeTexture(app->test_texture);
			app->test_texture = zest_CreateTexture("Bunny", zest_texture_storage_type_sprite_sheet, zest_image_flag_use_filtering, zest_format_r8g8b8a8_unorm, 10);
			app->test_image = zest_AddTextureImageFile(app->test_texture, "examples/assets/glow.png");
			zest_ProcessTextureImages(app->test_texture);
		}
		if (ImGui::Button("Bunny Image")) {
			zest_FreeTexture(app->test_texture);
			app->test_texture = zest_CreateTexture("Bunny", zest_texture_storage_type_sprite_sheet, zest_image_flag_use_filtering, zest_format_r8g8b8a8_unorm, 10);
			app->test_image = zest_AddTextureImageFile(app->test_texture, "examples/assets/wabbit_alpha.png");
			zest_ProcessTextureImages(app->test_texture);
		}
		*/
			//Test for memory leaks in zest
			/*
		for (int i = 0; i != context->device->memory_pool_count; ++i) {
			zloc_pool_stats_t stats = zloc_CreateMemorySnapshot(zloc__first_block_in_pool(zloc_GetPool(context->device->allocator)));
			ImGui::Text("Free Blocks: %i, Used Blocks: %i", stats.free_blocks, stats.used_blocks);
			ImGui::Text("Free Memory: %zu(bytes) %zu(kb) %zu(mb), Used Memory: %zu(bytes) %zu(kb) %zu(mb)", stats.free_size, stats.free_size / 1024, stats.free_size / 1024 / 1024, stats.used_size, stats.used_size / 1024, stats.used_size / 1024 / 1024);
		}
		*/
			zest_vec4 uv = zest_ImageUV(app->wabbit_sprite);
			//ImGui::Image((ImTextureID)app->wabbit_sprite, ImVec2(50.f, 50.f), ImVec2(uv.x, uv.y), ImVec2(uv.z, uv.w));
			zest_imgui_DrawImage(app->wabbit_sprite, 50.f, 50.f, ImGuiSpriteDrawCallback, app);
			ImGui::End();
			ImGui::Render();
			//An imgui layer is a manual layer, meaning that you need to let it know that the buffers need updating.
			//Load the imgui mesh data into the layer staging buffers. When the command queue is recorded, it will then upload that data to the GPU buffers for rendering
			zest_imgui_UpdateBuffers(&app->imgui);
		} zest_EndTimerLoop(app->timer);

		if (app->reset) {
			app->reset = false;
			ImGui_ImplSDL2_Shutdown();
			zest_imgui_Destroy(&app->imgui);
			zest_implsdl2_DestroyWindow(app->context);
			zest_window_data_t window_handles = zest_implsdl2_CreateWindow(50, 50, 1280, 768, 0, "PBR Simple Example");
			zest_ResetRenderer(app->context, &window_handles);
			InitImGuiApp(app);
		}

		app->cache_info.draw_imgui = zest_imgui_HasGuiToDraw();
		//app->cache_info.test_texture = app->test_texture;
		zest_frame_graph_cache_key_t cache_key = {};
		cache_key = zest_InitialiseCacheKey(app->context, &app->cache_info, sizeof(RenderCacheInfo));

		if (zest_BeginFrame(app->context)) {
			zest_frame_graph frame_graph = zest_GetCachedFrameGraph(app->context, &cache_key);
			//Begin the render graph with the command that acquires a swap chain image (zest_BeginFrameGraphSwapchain)
			//Use the render graph we created earlier. Will return false if a swap chain image could not be acquired. This will happen
			//if the window is resized for example.
			if (!frame_graph) {
				if (zest_BeginFrameGraph(app->context, "ImGui", &cache_key)) {
					zest_ImportSwapchainResource();
					//If there was no imgui data to render then zest_imgui_BeginPass will return false
					//Import our test texture with the Bunny sprite
					//zest_resource_node test_texture = zest_ImportImageResource("test texture", app->test_texture, 0);
					//------------------------ ImGui Pass ----------------------------------------------------------------
					//If there's imgui to draw then draw it
					zest_pass_node imgui_pass = zest_imgui_BeginPass(&app->imgui);
					if (imgui_pass) {
						//zest_ConnectInput(test_texture, 0);
						zest_ConnectSwapChainOutput();
					} else {
						//If there's no ImGui to render then just render a blank screen
						zest_pass_node blank_pass = zest_BeginGraphicBlankScreen("Draw Nothing");
						//Add the swap chain as an output to the imgui render pass. This is telling the render graph where it should render to.
						zest_ConnectSwapChainOutput();
					}
					zest_EndPass();
					//----------------------------------------------------------------------------------------------------
					//End the render graph and execute it. This will submit it to the GPU.
					zest_frame_graph render_graph = zest_EndFrameGraph();
				}
			} else {
				zest_QueueFrameGraphForExecution(app->context, frame_graph);
			}
			if (app->request_graph_print) {
				//You can print out the render graph for debugging purposes
				zest_PrintCompiledRenderGraph(frame_graph);
				app->request_graph_print = false;
			}
			zest_EndFrame(app->context);
		}
	}
}

#if defined(_WIN32)
// Windows entry point
//int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR pCmdLine, int nCmdShow) {
int main(int argc, char *argv[]) {
	//Create new config struct for Zest
	zest_create_info_t create_info = zest_CreateInfoWithValidationLayers(zest_validation_flag_enable_sync);
	//zest_create_info_t create_info = zest_CreateInfo();

	 if (SDL_Init(SDL_INIT_VIDEO) < 0) {
		 return 0;
	 }

	ImGuiApp imgui_app = {};

	//Create a window using SDL2. We must do this before setting up the device as it's needed to get
	//the extensions info.
	zest_window_data_t window_handles = zest_implsdl2_CreateWindow(50, 50, 1280, 768, 0, "PBR Simple Example");

	//Create the device that serves all vulkan based contexts
	//Get the required instance extensions from SDL
	unsigned int count = 0;
	SDL_Vulkan_GetInstanceExtensions((SDL_Window*)window_handles.window_handle, &count, NULL);
	const char** sdl_extensions = (const char**)malloc(sizeof(const char*) * count);
	SDL_Vulkan_GetInstanceExtensions((SDL_Window*)window_handles.window_handle, &count, sdl_extensions);

	// Create the device that serves all vulkan based contexts
	zest_device_builder device_builder = zest_BeginVulkanDeviceBuilder();
	zest_AddDeviceBuilderExtensions(device_builder, sdl_extensions, count);
	zest_AddDeviceBuilderValidation(device_builder);
	zest_DeviceBuilderLogToConsole(device_builder);
	zest_device device = zest_EndDeviceBuilder(device_builder);

	// Clean up the extensions array
	free(sdl_extensions);

	//Initialise Zest
	imgui_app.context = zest_CreateContext(device, &window_handles, &create_info);

	//Set the Zest use data
	zest_SetContextUserData(imgui_app.context, &imgui_app);
	//Initialise our example
	InitImGuiApp(&imgui_app);

	//Start the main loop
	MainLoop(&imgui_app);
	ImGui_ImplSDL2_Shutdown();
	zest_imgui_Destroy(&imgui_app.imgui);
	zest_DestroyContext(imgui_app.context);

	return 0;
}
#else
int main(void) {
	zest_create_info_t create_info = zest_CreateInfo();
	zest_implglfw_SetCallbacks(&create_info);
    ZEST__FLAG(create_info.flags, zest_init_flag_maximised);

	ImGuiApp imgui_app;

    create_info.log_path = ".";
	zest_CreateContext(&create_info);
	zest_SetUserData(&imgui_app);
	zest_SetUserUpdateCallback(UpdateCallback);
	InitImGuiApp(&imgui_app);

	zest_Start();

	return 0;
}
#endif
