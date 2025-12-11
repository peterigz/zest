#define ZEST_IMPLEMENTATION
#define ZEST_VULKAN_IMPLEMENTATION
#define ZEST_IMAGES_IMPLEMENTATION
#include "zest-imgui-template.h"
#include "zest.h"
#include "imgui_internal.h"


void InitImGui(ImGuiApp *app, zest_context context, zest_imgui_t *imgui) {
	//Initialise Dear ImGui
	zest_imgui_Initialise(context, imgui);
	ImGui_ImplGlfw_InitForVulkan((GLFWwindow *)zest_Window(context), true);

	//Implement a dark style
	zest_imgui_DarkStyle(imgui);
	
	//Configure imgui to use docking and multiple viewports. If you want to use multiple viewports
	//then docking must also be enabled.
    ImGuiIO& io = ImGui::GetIO();
	io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
	io.BackendFlags |= ImGuiBackendFlags_RendererHasViewports;

	//This is an example of how to change the font that ImGui uses
	io.Fonts->Clear();
	float font_size = 16.f;
	unsigned char *font_data;
	int tex_width, tex_height;
	ImFontConfig config;
	config.PixelSnapH = true;
	io.Fonts->AddFontFromFileTTF("examples/assets/Lato-Regular.ttf", font_size);
	io.Fonts->GetTexDataAsRGBA32(&font_data, &tex_width, &tex_height);

	//Rebuild the Zest font texture
	zest_imgui_RebuildFontTexture(imgui, tex_width, tex_height, font_data);

	app->timer = zest_CreateTimer(60);

}

void MainLoop(ImGuiApp *app) {

	while (!glfwWindowShouldClose((GLFWwindow*)zest_Window(app->context))) {
		glfwPollEvents();
		zest_UpdateDevice(app->device);
		ImGuiIO& io = ImGui::GetIO();
		//We can use a timer to only update the gui every 60 times a second (or whatever you decide). This
		//means that the buffers are uploaded less frequently and the command buffer is also re-recorded
		//less frequently.
		zest_StartTimerLoop(app->timer) {
			ImGui_ImplGlfw_NewFrame();
			//Draw our imgui stuff
			ImGui::NewFrame();
			ImGui::DockSpaceOverViewport(ImGui::GetMainViewport()->ID, ImGui::GetMainViewport(), ImGuiDockNodeFlags_PassthruCentralNode);
			ImGui::ShowDemoWindow();
			ImGui::Render();

			ImGui::UpdatePlatformWindows();
		} zest_EndTimerLoop(app->timer);

		//Render the main viewport.
		zest_imgui_RenderViewport(app->imgui.main_viewport);

		//Render any additional viewports
		ImGui::RenderPlatformWindowsDefault(NULL, &app->imgui);
	}
}

#if defined(_WIN32)
// Windows entry point
//int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR pCmdLine, int nCmdShow) {
int main(void) {

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

	zest_create_info_t create_info = zest_CreateInfo();
	zest_window_data_t window_handles = zest_implglfw_CreateWindow(50, 50, 1280, 768, 0, "Dear ImGui Dockspace Example");
	imgui_app.context = zest_CreateContext(imgui_app.device, &window_handles, &create_info);
	InitImGui(&imgui_app, imgui_app.context, &imgui_app.imgui);

	//Start the main loop
	MainLoop(&imgui_app);
	ImGui_ImplGlfw_Shutdown();
	zest_imgui_Destroy(&imgui_app.imgui);
	zest_DestroyDevice(imgui_app.device);

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
