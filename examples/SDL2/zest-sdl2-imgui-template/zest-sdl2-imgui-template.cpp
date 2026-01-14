#define ZEST_IMPLEMENTATION
#define ZEST_VULKAN_IMPLEMENTATION
#define ZEST_IMAGES_IMPLEMENTATION
#include "zest-sdl2-imgui-template.h"
#include <zest.h>
#include "imgui_internal.h"

/*
	Use SDL2 to create a window and also demonstrates how to load images and render in a separate thread
	without causing race conditions.
*/

void LoadSprite(ImGuiApp *app, const char *filename) {
	/*
	Don't call this function if the last image that was loaded hasn't been made active yet. We have to make active
	after a call to zest_BeginFrame but if the swapchain is not ready yet then new image won't made made active
	and so this function could be called again and load a new image over the top of the existing staging_image
	which hasn't been made active or had a chance to be freed leading to a memory leak and other potential issues.

	Obviously loading and replacing the same image that's in use every frame is not likely something that's going to happen
	a lot but this is just a test for robustness.
	*/
	int width, height, channels;
	stbi_uc *pixels = stbi_load(filename, &width, &height, &channels, 4);
	int size = width * height * 4;
	zest_image_info_t image_info = zest_CreateImageInfo(width, height);
	image_info.format = zest_format_r8g8b8a8_unorm;
	image_info.flags = zest_image_preset_texture;
	zest_image_handle image_handle = zest_CreateImageWithPixels(app->device, pixels, size, &image_info);
	STBI_FREE(pixels);
	zest_image image = zest_GetImage(image_handle);
	if (image) {
		zest_atlas_region_t sprite = zest_NewAtlasRegion();
		sprite.width = width;
		sprite.height = height;
		zest_AcquireSampledImageIndex(app->device, image, zest_texture_2d_binding);
		zest_BindAtlasRegionToImage(&sprite, app->sampler_index, image, zest_texture_2d_binding);
		app->sprite_state.staging_image_handle = image_handle;
		app->sprite_state.staging_sprite = sprite;
		app->sprite_state.update_ready.store(true);
	}
}

void InitImGuiApp(ImGuiApp *app) {

	//Initialise Dear ImGui
	zest_imgui_Initialise(app->context, &app->imgui, zest_implsdl2_DestroyWindow);
    ImGui_ImplSDL2_InitForVulkan((SDL_Window *)zest_Window(app->context));

	//Implement a dark style
	zest_imgui_DarkStyle(&app->imgui);
	
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

	app->loader_thread = std::thread(LoadSprite, app, "examples/assets/wabbit_alpha.png");
	//LoadSprite(app, "examples/assets/wabbit_alpha.png");

	zest_sampler_info_t sampler_info = zest_CreateSamplerInfo();
    zest_sampler_handle sampler_handle = zest_CreateSampler(app->context, &sampler_info);
	zest_sampler sampler = zest_GetSampler(sampler_handle);
	app->sampler_index = zest_AcquireSamplerIndex(app->device, sampler);

	app->imgui_sprite_shader = zest_CreateShader(app->device, zest_shader_imgui_r8g8_frag, zest_fragment_shader, "imgui_sprite_frag", ZEST_TRUE);

	app->imgui_sprite_pipeline = zest_CopyPipelineTemplate("ImGui Sprite Pipeline", app->imgui.pipeline);
	zest_SetPipelineFragShader(app->imgui_sprite_pipeline, app->imgui_sprite_shader);

	//We can use a timer to only update imgui 60 times per second
	app->timer = zest_CreateTimer(60);
	app->request_graph_print = true;
	app->reset = false;
}

void ImGuiSpriteDrawCallback(const ImDrawList* parent_list, const ImDrawCmd* cmd) {
	zest_imgui_callback_data_t *data = (zest_imgui_callback_data_t *)cmd->UserCallbackData;
	ImGuiApp *app = (ImGuiApp*)data->user_data;
	data->render_state->pipeline = zest_PipelineWithTemplate(app->imgui.pipeline, data->command_list);
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
		zest_UpdateDevice(app->device);
		//We can use a timer to only update the gui every 60 times a second (or whatever you decide). This
		//means that the buffers are uploaded less frequently and the command buffer is also re-recorded
		//less frequently.
		//zest_StartTimerLoop(app->timer) {
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
			if (ImGui::Button("Bordered")) {
				zest_implsdl2_SetWindowMode(app->context, zest_window_mode_bordered);
			}
			if (ImGui::Button("Borderless")) {
				zest_implsdl2_SetWindowMode(app->context, zest_window_mode_borderless);
			}
			if (ImGui::Button("Full Screen")) {
				zest_implsdl2_SetWindowMode(app->context, zest_window_mode_fullscreen);
			}
			if (ImGui::Button("Full Screen Borderless")) {
				zest_implsdl2_SetWindowMode(app->context, zest_window_mode_fullscreen_borderless);
			}
			
			//Spam load images to test that they can be loaded in ascync
			if (app->loader_thread.joinable() && app->sprite_state.update_ready.load() == false) {
				app->loader_thread.join();
				switch (app->load_image_index % 5) {
					case 0: { app->loader_thread = std::thread(LoadSprite, app, "examples/assets/glow.png"); break; }
					case 1: { app->loader_thread = std::thread(LoadSprite, app, "examples/assets/wabbit_alpha.png"); break; }
					case 2: { app->loader_thread = std::thread(LoadSprite, app, "examples/assets/particle.png"); break; }
					case 3: { app->loader_thread = std::thread(LoadSprite, app, "examples/assets/smoke.png"); break; }
					case 4: { app->loader_thread = std::thread(LoadSprite, app, "examples/assets/texture.jpg"); break; }
				}
				app->load_image_index++;
			}

			//Test for memory leaks in zest
			/*
		for (int i = 0; i != context->device->memory_pool_count; ++i) {
			zloc_pool_stats_t stats = zloc_CreateMemorySnapshot(zloc__first_block_in_pool(zloc_GetPool(context->device->allocator)));
			ImGui::Text("Free Blocks: %i, Used Blocks: %i", stats.free_blocks, stats.used_blocks);
			ImGui::Text("Free Memory: %zu(bytes) %zu(kb) %zu(mb), Used Memory: %zu(bytes) %zu(kb) %zu(mb)", stats.free_size, stats.free_size / 1024, stats.free_size / 1024 / 1024, stats.used_size, stats.used_size / 1024, stats.used_size / 1024 / 1024);
		}
		*/
			zest_vec4 uv = zest_RegionUV(&app->sprite_state.active_sprite);
			//ImGui::Image((ImTextureID)app->wabbit_sprite, ImVec2(50.f, 50.f), ImVec2(uv.x, uv.y), ImVec2(uv.z, uv.w));
			zest_imgui_DrawImage(&app->sprite_state.active_sprite, 50.f, 50.f, ImGuiSpriteDrawCallback, app);
			ImGui::End();
			ImGui::Render();
		//} zest_EndTimerLoop(app->timer);

		if (app->reset) {
			app->reset = false;
			ImGuiPlatformIO& platform_io = ImGui::GetPlatformIO();
			//We don't want imgui to destroy the context with the callback as we will just rest it for
			//testing purposes
			ImGui_ImplSDL2_Shutdown();
			zest_imgui_Destroy(&app->imgui);
			zest_implsdl2_DestroyWindow(app->context);
			zest_window_data_t window_handles = zest_implsdl2_CreateWindow(50, 50, 1280, 768, 0, "PBR Simple Example");
			zest_create_context_info_t create_info = zest_CreateContextInfo();
			app->context = zest_CreateContext(app->device, &window_handles, &create_info);
			InitImGuiApp(app);
		}

		app->cache_info.draw_imgui = zest_imgui_HasGuiToDraw(&app->imgui);
		//app->cache_info.test_texture = app->test_texture;
		zest_frame_graph_cache_key_t cache_key = {};
		cache_key = zest_InitialiseCacheKey(app->context, &app->cache_info, sizeof(RenderCacheInfo));

		if (zest_BeginFrame(app->context)) {
			if (app->sprite_state.update_ready.load() == true) {
				zest_FreeImage(app->sprite_state.active_image_handle);
				app->sprite_state.active_sprite = app->sprite_state.staging_sprite;
				app->sprite_state.active_image_handle = app->sprite_state.staging_image_handle;
				app->sprite_state.update_ready.store(false);
			}
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
					zest_pass_node imgui_pass = zest_imgui_BeginPass(&app->imgui, app->imgui.main_viewport);
					if (imgui_pass) {
						//zest_ConnectInput(test_texture, 0);
						zest_ConnectSwapChainOutput();
						zest_EndPass();
					} else {
						//If there's no ImGui to render then just render a blank screen
						zest_BeginRenderPass("Draw Nothing");
						zest_SetPassTask(zest_EmptyRenderPass, 0);
						//Add the swap chain as an output to the imgui render pass. This is telling the render graph where it should render to.
						zest_ConnectSwapChainOutput();
						zest_EndPass();
					}
					//----------------------------------------------------------------------------------------------------
					//End the render graph and execute it. This will submit it to the GPU.
					frame_graph = zest_EndFrameGraph();
				}
			}
			zest_QueueFrameGraphForExecution(app->context, frame_graph);
			if (app->request_graph_print) {
				//You can print out the render graph for debugging purposes
				zest_PrintCompiledFrameGraph(frame_graph);
				app->request_graph_print = false;
			}
			zest_EndFrame(app->context);
		}
	}
}

int main(int argc, char *argv[]) {

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
	device_builder->bindless_texture_2d_count = 4096;
	zest_AddDeviceBuilderExtensions(device_builder, sdl_extensions, count);
	zest_AddDeviceBuilderValidation(device_builder);
	zest_DeviceBuilderLogToConsole(device_builder);
	imgui_app.device = zest_EndDeviceBuilder(device_builder);

	// Clean up the extensions array
	free(sdl_extensions);

	//Create new config struct for Zest
	zest_create_context_info_t create_info = zest_CreateContextInfo();

	//Initialise a Zest context
	imgui_app.context = zest_CreateContext(imgui_app.device, &window_handles, &create_info);

	//Initialise our example
	InitImGuiApp(&imgui_app);

	//Start the main loop
	MainLoop(&imgui_app);
	if(imgui_app.loader_thread.joinable()) imgui_app.loader_thread.join();
	ImGui_ImplSDL2_Shutdown();
	zest_imgui_Destroy(&imgui_app.imgui);
	zest_DestroyDevice(imgui_app.device);

	return 0;
}
