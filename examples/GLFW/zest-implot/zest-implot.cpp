#define ZEST_IMPLEMENTATION
#define ZEST_VULKAN_IMPLEMENTATION
#include "zest-implot.h"
#include <zest.h>
#include "imgui_internal.h"

void InitImGuiApp(ImGuiApp *app) {
	//Initialise Dear ImGui
	zest_imgui_Initialise(app->context, &app->imgui);
    ImGui_ImplGlfw_InitForVulkan((GLFWwindow *)zest_Window(app->context), true);
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
	zest_imgui_RebuildFontTexture(&app->imgui, tex_width, tex_height, font_data);

	app->sync_refresh = true;

	app->timer = zest_CreateTimer(60);
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
			ImPlot::ShowDemoWindow();
			ImGui::Render();
			//Load the imgui mesh data into the layer staging buffers. When the command queue is recorded, it will then upload that data to the GPU buffers for rendering
			zest_imgui_UpdateBuffers(&app->imgui);
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
				zest_pass_node imgui_pass = zest_imgui_BeginPass(&app->imgui);
				if (imgui_pass) {
					zest_ConnectSwapChainOutput();
				} else {
					//If there's no ImGui to render then just render a blank screen
					zest_pass_node blank_pass = zest_BeginGraphicBlankScreen("Draw Nothing");
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

#if defined(_WIN32)
// Windows entry point
//int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR pCmdLine, int nCmdShow) {
int main(void) {
	//Create new config struct for Zest
	zest_create_info_t create_info = zest_CreateInfo();
	//Don't enable vsync so we can see the FPS go higher then the refresh rate
	//ZEST__UNFLAG(create_info.flags, zest_init_flag_enable_vsync);

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

	ImPlot::CreateContext();
	//Start the main loop
	MainLoop(&imgui_app);
	ImGui_ImplGlfw_Shutdown();
	zest_imgui_Destroy(&imgui_app.imgui);
	zest_DestroyContext(imgui_app.context);

	ImPlot::DestroyContext();

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
