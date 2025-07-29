#include <zest.h>

void UpdateCallback(zest_microsecs elapsed, void *user_data) {
	// Begin Render Graph Definition
	if (zest_BeginRenderToScreen(zest_GetMainWindowSwapchain(), "Render Graph")) {
		zest_pass_node clear_pass = zest_AddGraphicBlankScreen("Draw Nothing");
		VkClearColorValue clear_color = { {0.0f, 0.1f, 0.2f, 1.0f} };
		zest_ConnectSwapChainOutput(clear_pass, clear_color);
		zest_EndRenderGraph();
	}
}

#if defined(_WIN32)
// Windows entry point
//int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR pCmdLine, int nCmdShow)
int main(void) 
{
	//Make a config struct where you can configure zest with some options
	zest_create_info_t create_info = zest_CreateInfo();
	create_info.log_path = "./";
	ZEST__UNFLAG(create_info.flags, zest_init_flag_enable_vsync);
	ZEST__FLAG(create_info.flags, zest_init_flag_log_validation_errors_to_console);
	create_info.thread_count = 0;

	//Initialise Zest with the configuration
	zest_Initialise(&create_info);

    zest_LogFPSToConsole(1);
	//Set the callback you want to use that will be called each frame.
	zest_SetUserUpdateCallback(UpdateCallback);

	//Start the Zest main loop
	zest_Start();

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
