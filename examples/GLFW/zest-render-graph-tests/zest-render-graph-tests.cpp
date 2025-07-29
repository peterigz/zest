#include "zest-render-graph-tests.h"
#include "imgui_internal.h"

/*
  Core Functionality & Culling
   1. Empty Graph: Compile and execute an empty render graph. It should do nothing and not crash.
   2. Single Pass - No I/O: A graph with one pass that has no resource inputs or outputs. It should execute correctly.
   3. Unused Pass Culling: Define two passes, A and B. Pass A writes to a resource, but that resource is never used as an input to Pass B or as a final
	  output. Verify that Pass A is culled and never executed.
   4. Unused Resource Culling: Declare a resource that is never used by any pass. The graph should compile and run without trying to allocate memory for it.
   5. Chained Culling: Pass A writes to Resource X. Pass B reads from Resource X and writes to Resource Y. If Resource Y is not used as a final output, both
	  Pass A and Pass B should be culled.

  Resource Management & Barriers
   6. Transient Resource Test: Pass A writes to a transient texture. Pass B reads from that texture. Verify the data is passed correctly. The graph should
	  manage the creation and destruction of the transient texture.
   7. Input/Output Test: A pass that takes a pre-existing resource (e.g., a user-created buffer) as input and writes to a final output resource (e.g., the
	  swapchain).
   8. Automatic Barrier Test (Layout Transition):
	   * Pass A: Renders to a texture (Color Attachment Optimal layout).
	   * Pass B: Uses that same texture as a shader input (Shader Read-Only Optimal layout).
	   * The graph must insert the correct pipeline barrier to transition the texture layout between passes.
   9. Multi-Reader Barrier Test: Pass A writes to a resource. Passes B and C both read from that same resource. The graph should correctly synchronize this so
	  B and C only execute after A is complete.

  Data Integrity
   10. Buffer Write/Read:
	   * Pass A (Compute): Writes a specific data pattern into a storage buffer.
	   * Pass B (Compute): Reads from the buffer and verifies the data is correct (e.g., by writing a success/fail flag to another buffer that can be read by
		 the CPU).
   11. Image Write/Read (Clear Color):
	   * Pass A (Graphics): Clears a transient image to a specific color.
	   * Pass B (Compute): Reads the image pixels and verifies they match the clear color.
   12. Depth Attachment Test:
	   * Pass A: Renders geometry with depth testing/writing enabled to a transient depth buffer.
	   * Pass B: Renders geometry that should be occluded by the objects from Pass A.
	   * Verify the final image shows correct occlusion.

  Complex Scenarios & Error Handling
   13. Multi-Queue Synchronization:
	   * Pass A (Compute Queue): Processes data in a buffer.
	   * Pass B (Graphics Queue): Uses that buffer as a vertex buffer for rendering.
	   * The graph must handle the queue ownership transfer and synchronization (semaphores).
   14. Pass Grouping: Define two graphics passes that render to the same render target but have no dependency on each other. The graph compiler should ideally
	   group these into a single render pass with two subpasses for efficiency.
   15. Cyclic Dependency: Create a graph where Pass A depends on Pass B's output, and Pass B depends on Pass A's output. The graph compiler should detect this
	   cycle and return an error instead of crashing.
   16. Re-executing Graph: Compile a graph once, then execute it for multiple consecutive frames. Verify that no memory leaks or synchronization issues occur.
*/

int test__blank_screen(ZestTests *tests, Test *test) {
	if (zest_BeginRenderToScreen(zest_GetMainWindowSwapchain(), "Render Graph")) {
		zest_pass_node clear_pass = zest_AddGraphicBlankScreen("Draw Nothing");
		VkClearColorValue clear_color = { {0.0f, 0.1f, 0.2f, 1.0f} };
		zest_ConnectSwapChainOutput(clear_pass, clear_color);
		zest_render_graph render_graph = zest_EndRenderGraph();
		test->result |= render_graph->error_status;
	}
	test->frame_count++;
	return test->result;
}

void InitialiseTests(ZestTests *tests) {
	*tests = {};
	tests->tests[0] = { "Blank Screen", test__blank_screen, 0};
}

void UpdateCallback(zest_microsecs elapsed, void* user_data) {
	//The struct for this example app from the user data we set when initialising Zest
	ZestTests* tests = (ZestTests*)user_data;

	Test *current_test = &tests->tests[tests->current_test];
	int result = current_test->the_test(tests, current_test);

	if (current_test->frame_count > 10 && tests->current_test < TEST_COUNT - 1) {
		if (current_test->result != 0) {
			ZEST_PRINT("%s test failed", current_test->name);
		} else {
			ZEST_PRINT("%s test passed", current_test->name);
		}
		tests->current_test++;
		zest_ResetRenderer();
	}

}

#if defined(_WIN32)
// Windows entry point
//int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR pCmdLine, int nCmdShow) {
int main(void) {

	//Create new config struct for Zest
	zest_create_info_t create_info = zest_CreateInfoWithValidationLayers(0);
//	zest_create_info_t create_info = zest_CreateInfo();
	//ZEST__UNFLAG(create_info.flags, zest_init_flag_enable_vsync);
	ZEST__FLAG(create_info.flags, zest_init_flag_log_validation_errors_to_console);
	ZEST__UNFLAG(create_info.flags, zest_init_flag_cache_shaders);
	ZEST__UNFLAG(create_info.flags, zest_init_flag_enable_vsync);
	create_info.log_path = "./";
	create_info.thread_count = 0;

	ZestTests tests = {};

	//Initialise Zest
	zest_Initialise(&create_info);

	InitialiseTests(&tests);

	//Set the Zest use data
	zest_SetUserData(&tests);
	//Set the udpate callback to be called every frame
	zest_SetUserUpdateCallback(UpdateCallback);
	
	//Start the main loop
	zest_Start();

	return 0;
}
#else
int main(void) {
	zest_create_info_t create_info = zest_CreateInfo();
	zest_implglfw_SetCallbacks(&create_info);
    ZEST__FLAG(create_info.flags, zest_init_flag_maximised);

	ImGuiApp imgui_app;

    create_info.log_path = ".";
	zest_Initialise(&create_info);
	zest_SetUserData(&imgui_app);
	zest_SetUserUpdateCallback(UpdateCallback);
	InitImGuiApp(&imgui_app);

	zest_Start();

	return 0;
}
#endif
