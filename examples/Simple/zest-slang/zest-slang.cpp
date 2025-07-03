#include <zest.h>
#include "impl_slang.h"

using Slang::ComPtr;

int Init() {
    // First we need to create slang global session with work with the Slang API.
    zest_slang_InitialiseSession();

	const char *path = "examples/Simple/zest-slang/shaders/hello-world.slang";
    zest_shader shader = zest_slang_CreateShader( "examples/Simple/zest-slang/shaders/hello-world.slang", "hello-world.spv", "computeMain", shaderc_compute_shader, true);
    return 0;
 }

//Application specific, this just sets the function to call each render update
void UpdateTfxExample(zest_microsecs ellapsed, void *data) {
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
	ZEST__FLAG(create_info.flags, zest_init_flag_log_validation_errors_to_console);

	//Initialise Zest with the configuration
    if (zest_Initialise(&create_info)) {
        zest_LogFPSToConsole(1);
        //Set the callback you want to use that will be called each frame.
        zest_SetUserUpdateCallback(UpdateTfxExample);

        Init();
        //Start the Zest main loop
        zest_Start();
    } else {
        printf("Unable to initialise Zest, check the logs for reasons why.");
        return -1;
    }

	return 0;
}
#else
int main(void) {
    //Make a config struct where you can configure zest with some options
    zest_create_info_t create_info = zest_CreateInfo();
    create_info.log_path = "./";
    ZEST__UNFLAG(create_info.flags, zest_init_flag_enable_vsync);
    ZEST__FLAG(create_info.flags, zest_init_flag_log_validation_errors_to_console);

    tfx_InitialiseTimelineFX(tfx_GetDefaultThreadCount(), 128 * 1024 * 1024);

    //Initialise Zest with the configuration
    zest_Initialise(&create_info);
    zest_LogFPSToConsole(1);
    //Set the callback you want to use that will be called each frame.
    zest_SetUserUpdateCallback(UpdateTfxExample);

    TimelineFXExample game = { 0 };
    zest_SetUserData(&game);
    Init(&game);

    //Start the Zest main loop
    zest_Start();

    tfx_EndTimelineFX();

    return 0;
}
#endif
