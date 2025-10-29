#define ZEST_IMPLEMENTATION
#define ZEST_VULKAN_IMPLEMENTATION
#include <GLFW/glfw3.h>
#include <zest.h>

typedef struct minimal_app_t {
	zest_context context;
} minimal_app_t;

void MainLoop(minimal_app_t *app) {
	// Begin Render Graph Definition
	while (!glfwWindowShouldClose((GLFWwindow*)zest_Window(app->context))) {
		glfwPollEvents();
		zest_frame_graph_cache_key_t cache_key = zest_InitialiseCacheKey(app->context, 0, 0);
		if (zest_BeginFrame(app->context)) {
			zest_frame_graph frame_graph = zest_GetCachedFrameGraph(app->context, &cache_key);
			if (!frame_graph) {
				if (zest_BeginFrameGraph(app->context, "Render Graph", &cache_key)) {
					zest_ImportSwapchainResource();
					zest_BeginGraphicBlankScreen("Draw Nothing"); {
						zest_ConnectSwapChainOutput();
						zest_EndPass();
					}
					frame_graph = zest_EndFrameGraph();
					zest_QueueFrameGraphForExecution(app->context, frame_graph);
				}
			} else {
				zest_QueueFrameGraphForExecution(app->context, frame_graph);
			}
			zest_EndFrame(app->context);
		}
	}
}

#if defined(_WIN32)
// Windows entry point
//int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR pCmdLine, int nCmdShow)
int main(void) 
{
	//Make a config struct where you can configure zest with some options
	zest_create_info_t create_info = zest_CreateInfoWithValidationLayers(zest_validation_flag_enable_sync);
	//zest_create_info_t create_info = zest_CreateInfo();
	ZEST__UNFLAG(create_info.flags, zest_init_flag_enable_vsync);
	ZEST__FLAG(create_info.flags, zest_init_flag_log_validation_errors_to_console);

	if (!glfwInit()) {
		return 0;
	}

	minimal_app_t app = {};

	zest_uint count;
	const char **glfw_extensions = glfwGetRequiredInstanceExtensions(&count);

	//Create the device that serves all vulkan based contexts
	zest_device_builder device_builder = zest_BeginVulkanDeviceBuilder();
	zest_AddDeviceBuilderExtensions(device_builder, glfw_extensions, count);
	zest_AddDeviceBuilderValidation(device_builder);
	zest_DeviceBuilderLogToConsole(device_builder);
	zest_device device = zest_EndDeviceBuilder(device_builder);

	zest_window_data_t window_handles = zest_implglfw_CreateWindow(50, 50, 1280, 768, 0, "Minimal Example");
	//Initialise Zest
	app.context = zest_CreateContext(device, &window_handles, &create_info);

	//Start the Zest main loop
	MainLoop(&app);
	zest_DestroyContext(app.context);

	return 0;
}
#else
int main(void) {
	zest_create_info_t create_info = zest_CreateInfo();
	ZEST__UNFLAG(create_info.flags, zest_init_flag_enable_vsync);

	zest_CreateContext(&create_info);
    zest_LogFPSToConsole(1);
	zest_SetUserUpdateCallback(UpdateCallback);

	zest_Start();

	return 0;
}
#endif
