#define ZEST_IMPLEMENTATION
#define ZEST_VULKAN_IMPLEMENTATION
#include <SDL.h>
#include <zest.h>
#include <string>

/**
	Minimal app with a frame graph that renders a hello world triangle to the swapchain.
	The triangle needs no vertex or index buffers at all - the vertex shader generates the
	positions and colors from gl_VertexIndex.
 */

struct minimal_app_t {
	zest_device device;
	zest_context context;
	zest_pipeline_template triangle_pipeline;
};

void InitApp(minimal_app_t *app) {
	//Compile the shaders. Paths are relative to the working directory, which is expected to
	//be the root of the Zest repository.
	zest_shader_handle vert = zest_CreateShaderFromFile(app->device, "examples/SDL2/zest-minimal-template/shaders/triangle.vert", "triangle_vert", zest_vertex_shader, NULL, ZEST_TRUE);
	zest_shader_handle frag = zest_CreateShaderFromFile(app->device, "examples/SDL2/zest-minimal-template/shaders/triangle.frag", "triangle_frag", zest_fragment_shader, NULL, ZEST_TRUE);

	//Create the pipeline template. New templates already use the device's default (bindless)
	//pipeline layout so there's nothing else to set up here.
	app->triangle_pipeline = zest_CreatePipelineTemplate(app->device, "Triangle Pipeline");
	zest_SetPipelineShaders(app->triangle_pipeline, vert, frag);
	//The triangle is opaque. Note this is not zest_BlendStateNone, which masks out color writes entirely.
	zest_SetPipelineBlend(app->triangle_pipeline, zest_BlendStateOpaque());
	//No vertex buffers, so the pipeline needs no vertex input state
	zest_SetPipelineDisableVertexInput(app->triangle_pipeline);
}

void DrawTriangle(const zest_command_list command_list, void *user_data) {
	minimal_app_t *app = (minimal_app_t*)user_data;

	//Viewport and scissor are dynamic state, so set them to the size of the swapchain
	zest_cmd_SetScreenSizedViewport(command_list, 0.f, 1.f);

	//Fetch the pipeline for this context/render pass combination and bind it
	zest_pipeline pipeline = zest_GetPipeline(app->triangle_pipeline, command_list);
	zest_cmd_BindPipeline(command_list, pipeline);

	//Draw 3 vertices, 1 instance. No vertex buffer is bound - the vertex shader builds the
	//triangle from gl_VertexIndex.
	zest_cmd_Draw(command_list, 3, 1, 0, 0);
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
					zest_BeginRenderPass("Draw Triangle"); {
						//Tell the render pass that we want to output to the swap chain
						zest_ConnectSwapChainOutput();
						//Set the callback that will record the command buffer that draws to the swap chain
						zest_SetPassTask(DrawTriangle, app);
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

	minimal_app_t app = {};

	//Create a window using SDL2. We must do this before setting up the device as it's needed to get
	//the extensions info.
	zest_window_data_t window_data = zest_implsdl2_CreateWindow(50, 50, 1280, 768, 0, "Minimal Template");

	//Create the device that serves all vulkan based contexts
	app.device = zest_implsdl2_CreateVulkanDevice(&window_data, 0);

	//Initialise Zest
	app.context = zest_CreateContext(app.device, &window_data, &create_info);

	//Create the shaders and pipeline used to draw the triangle
	InitApp(&app);

	//Start the Zest main loop
	MainLoop(&app);
	zest_DestroyDevice(app.device);

	return 0;
}
