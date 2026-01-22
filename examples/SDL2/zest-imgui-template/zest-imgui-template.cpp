#define ZEST_IMPLEMENTATION
#define ZEST_VULKAN_IMPLEMENTATION
#define ZEST_IMAGES_IMPLEMENTATION
#include "zest-imgui-template.h"
#include "zest.h"
#include "imgui_internal.h"
#include "examples/Common/sdl_events.cpp"

/*
Example showing how to implement Dear ImGui with Zest.

This example demonstrates:
- Setting up Dear ImGui with Zest and GLFW
- Configuring docking and multi-viewport support
- Loading custom fonts
- Using a timer to limit UI update frequency
- Rendering ImGui viewports with Zest's simplified API
*/

//We need a custom viewport window for sdl viewports because the imgui implementation stores the window id in the platform handle
//rather then the window pointer.
void CreateSDLViewport(ImGuiViewport* viewport) {
	zest_imgui_viewport_t* app_viewport = (zest_imgui_viewport_t*)viewport->RendererUserData;
    ImGuiIO &io = ImGui::GetIO();
	zest_imgui_t *imgui = (zest_imgui_t*)io.UserData;
	zest_device device = zest_GetContextDevice(imgui->context);
	if (!app_viewport) {
		app_viewport = zest_imgui_AcquireViewport(zest_GetContextDevice(imgui->context)); 
		viewport->RendererUserData = app_viewport;
	}

	// Create Zest context
	zest_create_context_info_t create_info = zest_CreateContextInfo();
	zest_window_data_t window_handles = { };  
	// SDL2 stores window ID, not window pointer
	SDL_Window *sdl_window = SDL_GetWindowFromID((Uint32)(uintptr_t)viewport->PlatformHandle);
	window_handles.window_handle = sdl_window;
	window_handles.native_handle = viewport->PlatformHandleRaw;
	window_handles.width = (int)viewport->Size.x;
	window_handles.height = (int)viewport->Size.y;
	window_handles.window_sizes_callback = zest_imgui_GetWindowSizeCallback;
	window_handles.user_data = viewport;
	app_viewport->context = zest_CreateContext(device, &window_handles, &create_info);
	app_viewport->imgui = imgui;
	app_viewport->imgui_viewport = viewport;
}

void InitImGui(ImGuiApp *app, zest_context context, zest_imgui_t *imgui) {
	//Initialise Dear ImGui for Zest. This sets up the display size, creates the font texture,
	//and configures the rendering pipeline. The third parameter is the window destroy callback
	//used when closing additional viewports.
	zest_imgui_Initialise(context, imgui, zest_implsdl2_DestroyWindow);
	//Also initialise the GLFW backend for ImGui input handling
	ImGui_ImplSDL2_InitForVulkan((SDL_Window *)zest_Window(context));
	ImGuiPlatformIO& platform_io = ImGui::GetPlatformIO();
	platform_io.Renderer_CreateWindow = CreateSDLViewport;

	//Apply a dark color scheme to ImGui
	zest_imgui_DarkStyle(imgui);

	//Configure ImGui to use docking and multiple viewports. Docking allows windows to be
	//docked together into tab bars. Multi-viewport allows ImGui windows to be dragged
	//outside the main application window. Note: if you want multi-viewport support then
	//docking must also be enabled.
	ImGuiIO& io = ImGui::GetIO();
	io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
	io.BackendFlags |= ImGuiBackendFlags_RendererHasViewports;

	//This is an example of how to change the font that ImGui uses. First clear any existing
	//fonts from ImGui's font atlas.
	io.Fonts->Clear();
	float font_size = 16.f;
	unsigned char *font_data;
	int tex_width, tex_height;
	ImFontConfig config;
	config.PixelSnapH = true;
	//Load the font file. You can add multiple fonts by calling AddFontFromFileTTF multiple times.
	io.Fonts->AddFontFromFileTTF("examples/assets/Lato-Regular.ttf", font_size);
	//Build the font atlas and get the pixel data
	io.Fonts->GetTexDataAsRGBA32(&font_data, &tex_width, &tex_height);

	//After changing fonts you must rebuild Zest's font texture with the new atlas data.
	//This uploads the font texture to the GPU and updates the descriptor index.
	zest_imgui_RebuildFontTexture(imgui, tex_width, tex_height, font_data);

	//Create a timer to limit how often we update the UI. This is optional but can help
	//reduce CPU/GPU usage when the UI doesn't need to update every frame.
	app->timer = zest_CreateTimer(60);
}

void MainLoop(ImGuiApp *app) {

	int running = 1;
	SDL_Event event;

	while (running) {
		running = PollSDLEvents(app->context, &event);

		//This must be called every frame before any rendering. It handles resource cleanup,
		//advances frame timing, and prepares the device for the next frame.
		zest_UpdateDevice(app->device);

		ImGuiIO& io = ImGui::GetIO();

		//We can use a timer to only update the UI at a fixed rate (60fps in this case).
		//This means that ImGui's vertex/index buffers are uploaded less frequently and
		//command buffers are re-recorded less often, saving CPU/GPU resources.
		//The zest_StartTimerLoop/zest_EndTimerLoop macros handle the timing logic.
		zest_StartTimerLoop(app->timer) {
			//Start a new ImGui frame. This must be called before any ImGui widget calls.
			ImGui_ImplSDL2_NewFrame();
			ImGui::NewFrame();

			//Create a dockspace that covers the entire main viewport. This allows ImGui
			//windows to be docked to the edges or tabbed together.
			//ImGuiDockNodeFlags_PassthruCentralNode allows the background to show through.
			ImGui::DockSpaceOverViewport(ImGui::GetMainViewport()->ID, ImGui::GetMainViewport(), ImGuiDockNodeFlags_PassthruCentralNode);

			//Show the ImGui demo window - replace this with your own UI code
			ImGui::ShowDemoWindow();

			//Finalize the ImGui frame and prepare draw data for rendering
			ImGui::Render();

			//Update any additional platform windows (for multi-viewport support).
			//This creates/destroys/updates any ImGui windows that are outside the main window.
			ImGui::UpdatePlatformWindows();
		} zest_EndTimerLoop(app->timer);

		//Render the main viewport. This is the simplified API that handles frame begin/end,
		//frame graph setup, and swapchain presentation all in one call.
		zest_imgui_RenderViewport(app->imgui.main_viewport);

		//Render any additional viewports (windows dragged outside the main application window).
		//The second parameter passes the imgui context so Zest can access the font texture
		//and pipeline for rendering.
		ImGui::RenderPlatformWindowsDefault(NULL, &app->imgui);
	}
}

int main(int argc, char *argv[]) {

	ImGuiApp imgui_app = {};

	//Create a window using SDL2. We must do this before setting up the device as it's needed to get
	//the extensions info.
	zest_window_data_t window_data = zest_implsdl2_CreateWindow(50, 50, 1280, 768, 0, "Imgui With Docking");

	//Create the Vulkan device. This is a one-time setup that creates the Vulkan instance,
	//selects a physical device, and sets up the logical device with required queues.
	//The false parameter indicates we don't want a debug/validation layer.
	//One device can serve multiple windows/contexts.
	imgui_app.device = zest_implsdl2_CreateVulkanDevice(&window_data, false);

	//Create a context info struct with default settings. You can modify this to change
	//things like the number of frames in flight, MSAA settings, etc.
	zest_create_context_info_t create_info = zest_CreateContextInfo();

	//Create a Zest context for this window. The context manages the swapchain, frame
	//resources, and frame graph execution for this window.
	imgui_app.context = zest_CreateContext(imgui_app.device, &window_data, &create_info);

	//Initialise ImGui with our app, context, and imgui state struct
	InitImGui(&imgui_app, imgui_app.context, &imgui_app.imgui);

	//Run the main application loop
	MainLoop(&imgui_app);

	//Cleanup in reverse order of creation
	//First shutdown the GLFW backend for ImGui
	ImGui_ImplSDL2_Shutdown();
	//Destroy the Zest ImGui resources (font texture, pipeline, etc)
	zest_imgui_Destroy(&imgui_app.imgui);
	//Finally destroy the device which also cleans up all contexts and Vulkan resources.
	//Zest will warn of potential memory leaks if it finds un-freed blocks.
	zest_DestroyDevice(imgui_app.device);

	return 0;
}
