#include <zest.h>

// This is the function that will be called for your pass.
void ExampleRenderPassCallback(
	VkCommandBuffer command_buffer,
	const zest_render_graph_context_t *context, // Your graph context
	void *user_data                             // Global or per-pass user data
) {
	//Nothing happens here, just clearing the screen as a minimal render graph test
}

void UpdateCallback(zest_microsecs elapsed, void *user_data) {
	zest_render_graph render_graph = static_cast<zest_render_graph>(user_data);

	/*
	for (int i = 0; i != ZestDevice->memory_pool_count; ++i) {
		zloc_pool_stats_t stats = zloc_CreateMemorySnapshot(zloc__first_block_in_pool(zloc_GetPool(ZestDevice->allocator)));
		ZEST_PRINT("%i, %i", stats.free_blocks, stats.used_blocks);
	}
	*/

	// 2. Begin Render Graph Definition
	if (zest_BeginRenderToScreen(render_graph)) {
		zest_resource_node swapchain_output_resource = zest_ImportSwapChainResource( render_graph, "Swapchain Output" );
		zest_pass_node clear_pass = zest_AddGraphicBlankScreen(render_graph, "Draw Nothing");
		VkClearColorValue clear_color = { {0.0f, 0.1f, 0.2f, 1.0f} }; 
		zest_AddPassSwapChainOutput( clear_pass, swapchain_output_resource, clear_color);
		zest_EndRenderGraph(render_graph);
		zest_ExecuteRenderGraph(render_graph);
	}
}

#if defined(_WIN32)
// Windows entry point
//int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR pCmdLine, int nCmdShow)
int main(void) 
{
	//Make a config struct where you can configure zest with some options
	zest_create_info_t create_info = zest_CreateInfoWithValidationLayers();
	create_info.log_path = "./";
	ZEST__UNFLAG(create_info.flags, zest_init_flag_enable_vsync);
	ZEST__FLAG(create_info.flags, zest_init_flag_log_validation_errors_to_console);
	create_info.thread_count = 0;

	//Initialise Zest with the configuration
	zest_Initialise(&create_info);

	zest_render_graph render_graph = zest_NewRenderGraph("Simple Test", 0, 0);

	zest_SetUserData(render_graph);
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
