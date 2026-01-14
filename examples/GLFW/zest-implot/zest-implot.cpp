#define ZEST_IMPLEMENTATION
#define ZEST_VULKAN_IMPLEMENTATION
#include "zest-implot.h"
#include <zest.h>
#include "imgui_internal.h"

/*
Example showing how to implement imgui and the implot library for imgui into zest
*/

void InitImGuiApp(ImGuiApp *app) {
	//Initialise Dear ImGui
	zest_imgui_Initialise(app->context, &app->imgui, zest_implglfw_DestroyWindow);
    ImGui_ImplGlfw_InitForVulkan((GLFWwindow *)zest_Window(app->context), true);
	//Implement a dark style
	zest_imgui_DarkStyle(&app->imgui);
	
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
	zest_imgui_RebuildFontTexture(&app->imgui, tex_width, tex_height, font_data);

	app->sync_refresh = true;

	app->timer = zest_CreateTimer(60);
}

void MainLoop(ImGuiApp *app) {

	while (!glfwWindowShouldClose((GLFWwindow*)zest_Window(app->context))) {
		zest_UpdateDevice(app->device);
		glfwPollEvents();
		//We can use a timer to only update the gui every 60 times a second (or whatever you decide). This
		//means that the buffers are uploaded less frequently and the command buffer is also re-recorded
		//less frequently.
		zest_StartTimerLoop(app->timer) {
			//Must call the imgui GLFW implementation function
			ImGui_ImplGlfw_NewFrame();
			//Draw our imgui stuff
			ImGui::NewFrame();
			ImPlot::ShowDemoWindow();
			ImGui::Render();
		} zest_EndTimerLoop(app->timer);

		if (zest_BeginFrame(app->context)) {
			//Begin the render graph with the command that acquires a swap chain image (zest_BeginFrameGraphSwapchain)
			//Use the render graph we created earlier. Will return false if a swap chain image could not be acquired. This will happen
			//if the window is resized for example.
			if (zest_BeginFrameGraph(app->context, "ImGui Plot", 0)) {
				zest_ImportSwapchainResource();
				//If there was no imgui data to render then zest_imgui_BeginPass will return false
				//Import our test texture with the Bunny sprite
				//Add the test texture to the imgui render pass
				zest_pass_node imgui_pass = zest_imgui_BeginPass(&app->imgui, app->imgui.main_viewport);
				if (imgui_pass) {
					zest_ConnectSwapChainOutput();
				} else {
					//If there's no ImGui to render then just render a blank screen
					zest_BeginRenderPass("Draw Nothing");
					zest_SetPassTask(zest_EmptyRenderPass, 0);
					//Add the swap chain as an output to the imgui render pass. This is telling the render graph where it should render to.
					zest_ConnectSwapChainOutput();
				}
				zest_EndPass();
				//If there's imgui to draw then draw it
				//End the render graph. This compiles and executes the render graph.
				zest_frame_graph frame_graph = zest_EndFrameGraph();
				zest_QueueFrameGraphForExecution(app->context, frame_graph);
			}
			zest_EndFrame(app->context);
		}
	}
}

int main(void) {
	//Create new config struct for Zest
	zest_create_context_info_t create_info = zest_CreateContextInfo();

	if (!glfwInit()) {
		return 0;
	}

	ImGuiApp imgui_app = {};

	//Create the device that serves all vulkan based contexts
	imgui_app.device = zest_implglfw_CreateDevice(false);

	//Create a window using GLFW
	zest_window_data_t window_handles = zest_implglfw_CreateWindow(50, 50, 1280, 768, 0, "Dear ImGui Example");
	//Initialise Zest
	imgui_app.context = zest_CreateContext(imgui_app.device, &window_handles, &create_info);

	//Set the Zest use data
	zest_SetContextUserData(imgui_app.context, &imgui_app);
	//Initialise our example
	InitImGuiApp(&imgui_app);

	ImPlot::CreateContext();
	//Start the main loop
	MainLoop(&imgui_app);
	ImGui_ImplGlfw_Shutdown();
	zest_imgui_Destroy(&imgui_app.imgui);
	zest_DestroyDevice(imgui_app.device);

	ImPlot::DestroyContext();

	return 0;
}
