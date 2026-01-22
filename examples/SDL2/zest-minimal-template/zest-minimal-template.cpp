#define ZEST_IMPLEMENTATION
#define ZEST_VULKAN_IMPLEMENTATION
#include <SDL.h>
#include <zest.h>
#include <string>

/**
	Minimal app with a frame graph that renders a blank screen to the swapchain
 */

struct minimal_app_t {
	zest_device device;
	zest_context context;
};

void BlankScreen(const zest_command_list command_list, void *user_data) {
	//Usually you'd have zest_cmd_ commands like zest_cmd_Draw or zest_cmd_DrawIndexed
	//But with nothing here it will just be a blank screen.
}

int PollSDLEvents(zest_context context, SDL_Event *event) {
	int running = 1;
	while (SDL_PollEvent(event)) {
		if (event->type == SDL_QUIT) {
			running = 0;
		}
		if (event->type == SDL_WINDOWEVENT && event->window.event == SDL_WINDOWEVENT_CLOSE && event->window.windowID == SDL_GetWindowID((SDL_Window*)zest_Window(context))) {
			running = 0;
		}
	}
	return running;
}

void MainLoop(minimal_app_t *app) {
	// In loop:
	int running = 1;
	SDL_Event event;

	while (running) {
		//Check if the window was closed
		running = PollSDLEvents(app->context, &event);
		zest_UpdateDevice(app->device);
		//Generate a cache key
		zest_frame_graph_cache_key_t cache_key = zest_InitialiseCacheKey(app->context, 0, 0);
		if (zest_BeginFrame(app->context)) {
			//Try and fetch a cached frame graph using the key
			zest_frame_graph frame_graph = zest_GetCachedFrameGraph(app->context, &cache_key);
			if (!frame_graph) {
				//If no frame graph is cached then start recording a new frame graph
				//Specifying the cache_key will make the frame graph get cached after it's done recording
				if (zest_BeginFrameGraph(app->context, "Render Graph", &cache_key)) {
					//Import the swap chain as a resource
					zest_ImportSwapchainResource();
					//Create a new render pass to draw to the swapchain
					zest_BeginRenderPass("Draw Nothing"); {
						//Tell the render pass that we want to output to the swap chain
						zest_ConnectSwapChainOutput();
						//Set the callback that will record the command buffer that draws to the swap chain. In this
						//case though we're not drawing anything other then a blank screen.
						zest_SetPassTask(BlankScreen, 0);
						//Declare the end of the render pass
						zest_EndPass();
					}
					//End the frame graph. This will compile the frame graph ready to be executed.
					frame_graph = zest_EndFrameGraph();
				}
			}
			//When building a frame graph that is within a zest_BeginFrame and zest_EndFrame you can 
			//queue it for execution. This means that it will be executed inside zest_EndFrame and presented
			//to the window surface.
			zest_EndFrame(app->context, frame_graph);
		}
	}
}

int main(int argc, char *argv[]) {
	//Make a config struct where you can configure zest with some options
	zest_create_context_info_t create_info = zest_CreateContextInfo();
	ZEST__UNFLAG(create_info.flags, zest_context_init_flag_enable_vsync);

	minimal_app_t app = {};

	//Create a window using SDL2. We must do this before setting up the device as it's needed to get
	//the extensions info.
	zest_window_data_t window_data = zest_implsdl2_CreateWindow(50, 50, 1280, 768, 0, "PBR Simple Example");

	//Create the device that serves all vulkan based contexts
	app.device = zest_implsdl2_CreateVulkanDevice(&window_data, true);

	//Initialise Zest
	app.context = zest_CreateContext(app.device, &window_data, &create_info);

	//Start the Zest main loop
	MainLoop(&app);
	zest_DestroyDevice(app.device);

	return 0;
}
