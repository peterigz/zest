#include <zest.h>
#include <zest_vulkan.h>

void UpdateCallback(zest_microsecs elapsed, void *user_data) {
	// Begin Render Graph Definition
	zest_swapchain swapchain = zest_GetMainWindowSwapchain();
	zest_frame_graph_cache_key_t cache_key = zest_InitialiseCacheKey(zest_GetMainWindowSwapchain(), 0, 0);
	if (zest_BeginFrameGraphSwapchain(zest_GetMainWindowSwapchain(), "Render Graph", &cache_key)) {
		zest_BeginGraphicBlankScreen("Draw Nothing"); {
			zest_ConnectSwapChainOutput();
			zest_EndPass();
		}
		zest_EndFrameGraph();
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
	create_info.log_path = "./";
	ZEST__UNFLAG(create_info.flags, zest_init_flag_enable_vsync);
	ZEST__FLAG(create_info.flags, zest_init_flag_log_validation_errors_to_console);
	create_info.thread_count = 0;

	zest_UseVulkan();
	//Initialise Zest with the configuration
	zest_Initialise(&create_info);

    zest_LogFPSToConsole(1);
	//Set the callback you want to use that will be called each frame.
	zest_SetUserUpdateCallback(UpdateCallback);

	//Start the Zest main loop
	zest_Start();
	zest_Shutdown();

	return 0;
}
#else
int main(void) {
	zest_create_info_t create_info = zest_CreateInfo();
	ZEST__UNFLAG(create_info.flags, zest_init_flag_enable_vsync);

	zest_Initialise(&create_info);
    zest_LogFPSToConsole(1);
	zest_SetUserUpdateCallback(UpdateCallback);

	zest_Start();

	return 0;
}
#endif
