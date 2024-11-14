#include "timelinefx.h"
#include <zest.h>

void UpdateCallback(zest_microsecs elapsed, void *user_data) {
}

#if defined(_WIN32)
// Windows entry point
int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR pCmdLine, int nCmdShow)
//int main(void) 
{
	//Make a config struct where you can configure zest with some options
	zest_create_info_t create_info = zest_CreateInfo();
	create_info.log_path = "./";
	ZEST__UNFLAG(create_info.flags, zest_init_flag_enable_vsync);

	tfx_InitialiseTimelineFX(0, 128 * 1024 * 1024);

	//Initialise Zest with the configuration
	zest_Initialise(&create_info);
	zest_LogFPSToConsole(1);
	//Set the callback you want to use that will be called each frame.
	zest_SetUserUpdateCallback(UpdateCallback);

	//Start the Zest main loop
	zest_Start();

	tfx_EndTimelineFX();

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
