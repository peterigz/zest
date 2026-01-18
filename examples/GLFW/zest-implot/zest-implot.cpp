#define ZEST_IMPLEMENTATION
#define ZEST_VULKAN_IMPLEMENTATION
#include "zest-implot.h"
#include <zest.h>
#include "imgui_internal.h"

/*
Example showing how to implement Dear ImGui with the ImPlot extension library in Zest.

This example demonstrates:
- Setting up Dear ImGui and ImPlot with Zest and GLFW
- Using the frame graph API directly (instead of the simplified zest_imgui_RenderViewport)
- Handling the case when there's no ImGui content to render
- Loading custom fonts
- Using a timer to limit UI update frequency
*/

void InitImGuiApp(ImGuiApp *app) {
	//Initialise Dear ImGui for Zest. This sets up the display size, creates the font texture,
	//and configures the rendering pipeline. The third parameter is the window destroy callback
	//used when closing additional viewports.
	zest_imgui_Initialise(app->context, &app->imgui, zest_implglfw_DestroyWindow);
	//Also initialise the GLFW backend for ImGui input handling
	ImGui_ImplGlfw_InitForVulkan((GLFWwindow *)zest_Window(app->context), true);

	//Apply a dark color scheme to ImGui
	zest_imgui_DarkStyle(&app->imgui);

	//Configure ImGui to use docking. This allows windows to be docked together into tab bars.
	ImGuiIO& io = ImGui::GetIO();
	io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

	//This is an example of how to change the font that ImGui uses. First clear any existing
	//fonts from ImGui's font atlas.
	io.Fonts->Clear();
	float font_size = 15.f;
	unsigned char *font_data;
	int tex_width, tex_height;
	ImFontConfig config;
	config.PixelSnapH = true;
	//Load the font file
	io.Fonts->AddFontFromFileTTF("examples/assets/Lato-Regular.ttf", font_size);
	//Build the font atlas and get the pixel data
	io.Fonts->GetTexDataAsRGBA32(&font_data, &tex_width, &tex_height);

	//After changing fonts you must rebuild Zest's font texture with the new atlas data.
	//This uploads the font texture to the GPU and updates the descriptor index.
	zest_imgui_RebuildFontTexture(&app->imgui, tex_width, tex_height, font_data);

	app->sync_refresh = true;

	//Create a timer to limit how often we update the UI. This is optional but can help
	//reduce CPU/GPU usage when the UI doesn't need to update every frame.
	app->timer = zest_CreateTimer(60);
}

void MainLoop(ImGuiApp *app) {

	while (!glfwWindowShouldClose((GLFWwindow*)zest_Window(app->context))) {
		//This must be called every frame before any rendering. It handles resource cleanup,
		//advances frame timing, and prepares the device for the next frame.
		zest_UpdateDevice(app->device);

		//Poll for window events (keyboard, mouse, etc)
		glfwPollEvents();

		//We can use a timer to only update the UI at a fixed rate (60fps in this case).
		//This means that ImGui's vertex/index buffers are uploaded less frequently and
		//command buffers are re-recorded less often, saving CPU/GPU resources.
		zest_StartTimerLoop(app->timer) {
			//Start a new ImGui frame. This must be called before any ImGui widget calls.
			ImGui_ImplGlfw_NewFrame();
			ImGui::NewFrame();

			//Show the ImPlot demo window - replace this with your own plotting code
			ImPlot::ShowDemoWindow();

			//Finalize the ImGui frame and prepare draw data for rendering
			ImGui::Render();
		} zest_EndTimerLoop(app->timer);

		//Begin a new frame. This function is essential for rendering to the window.
		//Returns false if the swapchain had to be recreated (e.g., window resize).
		if (zest_BeginFrame(app->context)) {
			zest_frame_graph frame_graph = NULL;

			//Begin building the frame graph. The second parameter is a name for debugging,
			//and the third is an optional cache key (0 means no caching).
			if (zest_BeginFrameGraph(app->context, "ImGui Plot", 0)) {
				//Import the swapchain as a resource so we can render to it
				zest_ImportSwapchainResource();

				//Try to begin an ImGui render pass. Returns NULL if there's no ImGui to render.
				zest_pass_node imgui_pass = zest_imgui_BeginPass(&app->imgui, app->imgui.main_viewport);
				if (imgui_pass) {
					//ImGui has content to render - connect the swapchain as the output
					zest_ConnectSwapChainOutput();
				} else {
					//No ImGui content to render - we still need a render pass that outputs
					//to the swapchain, otherwise nothing will be presented.
					zest_BeginRenderPass("Draw Nothing");
					//Use the built-in empty render pass callback that just clears the screen
					zest_SetPassTask(zest_EmptyRenderPass, 0);
					//Connect the swapchain as the output for this pass
					zest_ConnectSwapChainOutput();
				}
				zest_EndPass();

				//Finalize and compile the frame graph. This returns the compiled graph
				//ready for execution.
				frame_graph = zest_EndFrameGraph();
			}

			//Execute the frame graph and present to the swapchain
			zest_EndFrame(app->context, frame_graph);
		}
	}
}

int main(void) {
	//Create a context info struct with default settings. You can modify this to change
	//things like the number of frames in flight, MSAA settings, etc.
	zest_create_context_info_t create_info = zest_CreateContextInfo();

	//Initialise GLFW. This must be done before creating any windows or Vulkan devices.
	if (!glfwInit()) {
		return 0;
	}

	ImGuiApp imgui_app = {};

	//Create the Vulkan device. This is a one-time setup that creates the Vulkan instance,
	//selects a physical device, and sets up the logical device with required queues.
	//The false parameter indicates we don't want a debug/validation layer.
	imgui_app.device = zest_implglfw_CreateVulkanDevice(false);

	//Create the GLFW window and get the window handles needed by Zest
	zest_window_data_t window_handles = zest_implglfw_CreateWindow(50, 50, 1280, 768, 0, "Dear ImGui Example");

	//Create a Zest context for this window. The context manages the swapchain, frame
	//resources, and frame graph execution for this window.
	imgui_app.context = zest_CreateContext(imgui_app.device, &window_handles, &create_info);

	//Initialise ImGui with our app struct
	InitImGuiApp(&imgui_app);

	//Create the ImPlot context. This must be done after ImGui is initialised but before
	//using any ImPlot functions.
	ImPlot::CreateContext();

	//Run the main application loop
	MainLoop(&imgui_app);

	//Cleanup in reverse order of creation
	//First shutdown the GLFW backend for ImGui
	ImGui_ImplGlfw_Shutdown();
	//Destroy the Zest ImGui resources (font texture, pipeline, etc)
	zest_imgui_Destroy(&imgui_app.imgui);
	//Destroy the device which also cleans up all contexts and Vulkan resources.
	//Zest will warn of potential memory leaks if it finds un-freed blocks.
	zest_DestroyDevice(imgui_app.device);

	//Finally destroy the ImPlot context
	ImPlot::DestroyContext();

	return 0;
}
