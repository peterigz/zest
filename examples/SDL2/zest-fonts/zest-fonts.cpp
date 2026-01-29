// Define implementations in exactly one .cpp file before including headers
#define ZEST_IMPLEMENTATION
#define ZEST_VULKAN_IMPLEMENTATION
#define ZEST_MSDF_IMPLEMENTATION		// Enables MSDF font generation/loading in zest-utilities
#include <SDL.h>
#include "implementations/impl_imgui.h"
#include "imgui/imgui.h"
#include <imgui/misc/freetype/imgui_freetype.h>
#include <imgui/backends/imgui_impl_sdl2.h>
#include <zest.h>
#include "examples/Common/sdl_events.cpp"

/*
MSDF Font Rendering Example
---------------------------
Demonstrates rendering crisp, scalable text using Multi-channel Signed Distance Field (MSDF) fonts.
MSDF fonts stay sharp at any size without needing multiple texture atlases.

This example:
- Generates an MSDF font from a TTF file (cached to disk for faster subsequent loads)
- Creates a font layer for GPU-accelerated text rendering
- Uses the frame graph to compose font and ImGui passes
*/

// Cache key data for frame graph caching. When this data changes, the frame graph is rebuilt.
struct RenderCacheInfo {
	bool draw_imgui;
	zest_image_handle test_texture;
};

// Application state containing all rendering resources
typedef struct zest_fonts_example {
	zest_timer_t timer;					// Timer for rate-limiting ImGui updates
	zest_imgui_t imgui;					// ImGui integration state
	zest_context context;				// Window/swapchain context
	zest_device device;					// Vulkan device (one per application)
	RenderCacheInfo cache_info;			// Frame graph cache key data
	zest_msdf_font_t font;				// MSDF font atlas and glyph data
	zest_font_resources_t font_resources;	// Font shader pipeline resources
	zest_layer_handle font_layer;		// Layer for batching text draw calls
	float font_size;					// Current font scale factor
	bool print_frame_graph;
} zest_fonts_example;

void InitExample(zest_fonts_example *app) {
	// Initialize Dear ImGui with Zest and GLFW backend
	zest_imgui_Initialise(app->context, &app->imgui, zest_implsdl2_DestroyWindow);
    ImGui_ImplSDL2_InitForVulkan((SDL_Window *)zest_Window(app->context));
	zest_imgui_DarkStyle(&app->imgui);

	// Timer for rate-limiting ImGui updates (reduces buffer uploads and command recording)
	app->timer = zest_CreateTimer(60);

	// Load MSDF font from cache, or generate from TTF if not cached
	// zest_CreateMSDF params: context, ttf path, sampler binding, font size (px), distance range
	if (!zest__file_exists("examples/assets/vaders/Anta-Regular.msdf")) {
		app->font = zest_CreateMSDF(app->context, "examples/assets/vaders/Anta-Regular.ttf", app->imgui.font_sampler_binding_index, 64.f, 4.f);
		zest_SaveMSDF(&app->font, "examples/assets/vaders/Anta-Regular.msdf");
	} else {
		app->font = zest_LoadMSDF(app->context, "examples/assets/vaders/Anta-Regular.msdf", app->imgui.font_sampler_binding_index);
	}

	// Create font rendering resources: shader pipeline for MSDF text
	app->font_resources = zest_CreateFontResources(app->context, "examples/assets/shaders/font.vert", "examples/assets/shaders/font.frag");
	// Create a layer to batch text draw calls (100 = initial instance capacity)
	app->font_layer = zest_CreateFontLayer(app->context, "MSDF Font Example Layer", 100);
	app->font_size = 1.f;
}

void MainLoop(zest_fonts_example *app) {
	// FPS tracking variables
	zest_microsecs running_time = zest_Microsecs();
	zest_microsecs frame_time = 0;
	zest_uint frame_count = 0;
	zest_uint fps = 0;
	int running = 1;
	SDL_Event event;

	while (running) {
		running = PollSDLEvents(app->context, &event);
		// Calculate FPS
		zest_microsecs current_frame_time = zest_Microsecs() - running_time;
		running_time = zest_Microsecs();
		frame_time += current_frame_time;
		frame_count += 1;
		if (frame_time >= ZEST_MICROSECS_SECOND) {
			frame_time -= ZEST_MICROSECS_SECOND;
			fps = frame_count;
			frame_count = 0;
		}

		// Process device updates (handles resource cleanup, etc.)
		zest_UpdateDevice(app->device);

		// Rate-limit ImGui updates to reduce GPU uploads and command recording
		zest_StartTimerLoop(app->timer) {
			ImGui_ImplSDL2_NewFrame();
			ImGui::NewFrame();
			ImGui::Begin("Test Window");

			// Expose MSDF font settings for real-time tweaking
			ImGui::DragFloat("Font Size", &app->font_size, 0.01f);
			ImGui::DragFloat2("Unit range", &app->font.settings.unit_range.x, 0.001f);
			ImGui::DragFloat2("Shadow Offset", &app->font.settings.shadow_offset.x, 0.01f);
			ImGui::DragFloat4("Shadow Color", &app->font.settings.shadow_color.x, 0.01f);
			ImGui::DragFloat("In bias", &app->font.settings.in_bias, 0.01f);
			ImGui::DragFloat("Out bias", &app->font.settings.out_bias, 0.01f);
			ImGui::DragFloat("Smoothness", &app->font.settings.smoothness, 0.01f);
			ImGui::DragFloat("Gamma", &app->font.settings.gamma, 0.01f);
			if (ImGui::Button("Print Frame Graph")) {
				app->print_frame_graph = true;
			}

			ImGui::End();
			ImGui::Render();
		} zest_EndTimerLoop(app->timer);

		// Get the font layer for this frame
		zest_layer font_layer = zest_GetLayer(app->font_layer);

		if (zest_BeginFrame(app->context)) {
			// Queue text draw calls on the font layer
			// These are batched and uploaded to GPU via the transfer pass below
			zest_SetMSDFFontDrawing(font_layer, &app->font, &app->font_resources);
			zest_SetLayerColor(font_layer, 255, 255, 255, 255);
			zest_DrawMSDFText(font_layer, 20.f, 150.f, .0f, 0.0f, app->font_size, 0.f, "This is a test %u", fps);
			zest_DrawMSDFText(font_layer, 20.f, 220.f, .0f, 0.0f, app->font_size, 0.f, "Some more test text !£$%^&*()", fps);

			// Frame graph caching: when cache_info changes, the graph is rebuilt
			// This avoids rebuilding the graph every frame when nothing structural changes
			app->cache_info.draw_imgui = zest_imgui_HasGuiToDraw(&app->imgui);
			zest_frame_graph_cache_key_t cache_key = {};
			cache_key = zest_InitialiseCacheKey(app->context, &app->cache_info, sizeof(RenderCacheInfo));

			zest_SetSwapchainClearColor(app->context, 0.f, 0.2f, 0.5f, 1.f);
			zest_frame_graph frame_graph = zest_GetCachedFrameGraph(app->context, &cache_key);

			// Build frame graph if not cached (first frame or cache key changed)
			if (!frame_graph) {
				if (zest_BeginFrameGraph(app->context, "ImGui", &cache_key)) {
					// Declare resources used in the frame graph
					// Transient resources are automatically managed (created/destroyed as needed)
					zest_resource_node font_layer_resource = zest_AddTransientLayerResource("Font layer", font_layer, ZEST_FALSE);
					zest_ImportSwapchainResource();

					// Pass 1: Upload font layer instance data to GPU
					zest_BeginTransferPass("Upload font layer"); {
						zest_ConnectOutput(font_layer_resource);
						zest_SetPassTask(zest_UploadInstanceLayerData, font_layer);
						zest_EndPass();
					}

					// Pass 2: Render text to swapchain
					// The frame graph compiler automatically inserts barriers between passes
					zest_BeginRenderPass("Font pass"); {
						zest_ConnectInput(font_layer_resource);
						zest_ConnectSwapChainOutput();
						zest_SetPassTask(zest_DrawInstanceLayer, font_layer);
						zest_EndPass();
					}

					// Pass 3: Render ImGui overlay (if there's UI to draw)
					zest_pass_node imgui_pass = zest_imgui_BeginPass(&app->imgui, app->imgui.main_viewport);
					if (imgui_pass) {
						zest_ConnectSwapChainOutput();
					} else {
						// Fallback pass when no ImGui content
						zest_BeginRenderPass("Draw Nothing");
						zest_SetPassTask(zest_EmptyRenderPass, 0);
						zest_ConnectSwapChainOutput();
					}
					zest_EndPass();

					// Compile and cache the frame graph
					frame_graph = zest_EndFrameGraph();

				}
			}
			if (app->print_frame_graph) {
				app->print_frame_graph = false;
				zest_PrintCompiledFrameGraph(frame_graph);
			}
			zest_EndFrame(app->context, frame_graph);
		}

		// Handle window resize: update layer dimensions and font projection matrix
		if (zest_SwapchainWasRecreated(app->context)) {
			zest_SetLayerSizeToSwapchain(font_layer);
			zest_UpdateFontTransform(&app->font);
		}
	}

}

int main(int argc, char *argv[]) {
	// Configure context settings (disable vsync for uncapped framerate)
	zest_create_context_info_t create_info = zest_CreateContextInfo();
	ZEST__UNFLAG(create_info.flags, zest_context_init_flag_enable_vsync);

	zest_fonts_example fonts_app = {};

	//Create a window using SDL2. We must do this before setting up the device as it's needed to get
	//the extensions info.
	zest_window_data_t window_data = zest_implsdl2_CreateWindow(50, 50, 1280, 768, 0, "MSDF Fonts");

	//Create a device using a helper function for GLFW.
	fonts_app.device = zest_implsdl2_CreateVulkanDevice(&window_data, true);

	// Create window and context (one context per window/swapchain)
	fonts_app.context = zest_CreateContext(fonts_app.device, &window_data, &create_info);

	// Store app pointer for access in callbacks
	zest_SetContextUserData(fonts_app.context, &fonts_app);
	InitExample(&fonts_app);

	// Run until window closed
	MainLoop(&fonts_app);

	// Cleanup in reverse order of creation
	zest_FreeFont(&fonts_app.font);
	ImGui_ImplSDL2_Shutdown();
	zest_imgui_Destroy(&fonts_app.imgui);
	zest_DestroyDevice(fonts_app.device);

	return 0;
}
