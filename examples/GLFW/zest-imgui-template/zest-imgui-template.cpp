#define ZEST_IMPLEMENTATION
#define ZEST_VULKAN_IMPLEMENTATION
#define ZEST_IMAGES_IMPLEMENTATION
#include "zest-imgui-template.h"
#include "zest.h"
#include "imgui_internal.h"

void InitImGuiApp(ImGuiApp *app) {
	//Initialise Dear ImGui
	app->imgui = zest_imgui_Initialise(app->context);
    ImGui_ImplGlfw_InitForVulkan((GLFWwindow *)zest_Window(app->context), true);

	//Implement a dark style
	zest_imgui_DarkStyle();
	
	//This is an exmaple of how to change the font that ImGui uses
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

	zest_image_collection_t atlas = zest_CreateImageAtlasCollection(zest_format_r8g8_unorm, 8);
	int width, height, channels;
	stbi_uc *pixels = stbi_load("examples/assets/wabbit_alpha.png", &width, &height, &channels, 0);
	int size = width * height * channels;
	app->wabbit_sprite = zest_AddImageAtlasPixels(&atlas, pixels, size, width, height, zest_format_r8g8_unorm);
	zest_image_handle image_atlas = zest_CreateImageAtlas(app->context, &atlas, 1024, 1024, 0);
	zest_sampler_info_t sampler_info = zest_CreateSamplerInfo();
    zest_sampler_handle sampler = zest_CreateSampler(app->context, &sampler_info);
	app->atlas_binding_index = zest_AcquireSampledImageIndex(app->context, image_atlas, zest_texture_2d_binding);
	app->atlas_sampler_binding_index = zest_AcquireSamplerIndex(app->context, sampler);
	zest_BindAtlasRegionToImage(app->wabbit_sprite, app->atlas_sampler_binding_index, image_atlas, zest_texture_2d_binding);
	STBI_FREE(pixels);
	//Create a texture to load in a test image to show drawing that image in an imgui window
	//app->test_texture = zest_CreateTexture("Bunny", zest_texture_storage_type_sprite_sheet, zest_image_flag_use_filtering, zest_format_r8g8b8a8_unorm, 10);
	//Load in the image and add it to the texture
	//app->test_image = zest_AddTextureImageFile(app->test_texture, "examples/assets/wabbit_alpha.png");
	//Process the texture so that its ready to be used
	//zest_ProcessTextureImages(app->test_texture);

	app->imgui_sprite_shader = zest_CreateShader(zest_GetContextDevice(app->context), zest_shader_imgui_r8g8_frag, zest_fragment_shader, "imgui_sprite_frag", ZEST_TRUE);

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
	data->render_state->pipeline = zest_PipelineWithTemplate(app->imgui_sprite_pipeline, data->command_list);
	data->render_state->resources = app->imgui.font_resources;
	return;
}

void MainLoop(ImGuiApp *app) {

	while (!glfwWindowShouldClose((GLFWwindow*)zest_Window(app->context))) {
		glfwPollEvents();
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
			if (ImGui::Button("Toggle Refresh Rate Sync")) {
				if (app->sync_refresh) {
					zest_DisableVSync(app->context);
					app->sync_refresh = false;
				} else {
					zest_EnableVSync(app->context);
					app->sync_refresh = true;
				}
			}
			zloc_allocation_stats_t device_stats = zest_GetDeviceMemoryStats(app->device);
			zloc_allocation_stats_t context_stats = zest_GetContextMemoryStats(app->context);
			ImGui::Text("Device Memory: Free: %zukb, Used: %zukb", device_stats.free / 1024, (device_stats.capacity - device_stats.free) / 1024);
			ImGui::Text("Context Memory: Free: %zukb, Used: %zukb", context_stats.free / 1024, (context_stats.capacity - context_stats.free) / 1024);
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
			zest_vec4 uv = zest_RegionUV(app->wabbit_sprite);
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
			ImGui_ImplGlfw_Shutdown();
			zest_imgui_Destroy(&app->imgui);
			zest_implglfw_DestroyWindow(app->context);
			zest_window_data_t window_handles = zest_implglfw_CreateWindow(50, 50, 1280, 768, 0, "PBR Simple Example");
			zest_ResetContext(app->context, &window_handles);
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
					//If there's imgui to draw then zest__imgui_BeginPass with return a pass, otherwise you can 
					//do something else like draw a blank screen.
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
				zest_PrintCompiledFrameGraph(frame_graph);
				app->request_graph_print = false;
			}
			zest_EndFrame(app->context);
		}
	}
}

#if defined(_WIN32)
// Windows entry point
//int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR pCmdLine, int nCmdShow) {
int main(void) {
	//Create new config struct for Zest
	zest_create_info_t create_info = zest_CreateInfoWithValidationLayers(zest_validation_flag_enable_sync);
	//zest_create_info_t create_info = zest_CreateInfo();

	if (!glfwInit()) {
		return 0;
	}

	ImGuiApp imgui_app = {};

	zest_uint count;
	const char **glfw_extensions = glfwGetRequiredInstanceExtensions(&count);

	//Create the device that serves all vulkan based contexts
	zest_device_builder device_builder = zest_BeginVulkanDeviceBuilder();
	zest_AddDeviceBuilderExtensions(device_builder, glfw_extensions, count);
	zest_AddDeviceBuilderValidation(device_builder);
	zest_DeviceBuilderLogToConsole(device_builder);
	imgui_app.device = zest_EndDeviceBuilder(device_builder);

	//Create a window using GLFW
	zest_window_data_t window_handles = zest_implglfw_CreateWindow(50, 50, 1280, 768, 0, "Dear ImGui Example");
	//Initialise Zest
	imgui_app.context = zest_CreateContext(imgui_app.device, &window_handles, &create_info);

	//Set the Zest use data
	zest_SetContextUserData(imgui_app.context, &imgui_app);
	//Initialise our example
	InitImGuiApp(&imgui_app);

	//Start the main loop
	MainLoop(&imgui_app);
	ImGui_ImplGlfw_Shutdown();
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
