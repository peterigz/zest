#define ZEST_IMPLEMENTATION
#define ZEST_VULKAN_IMPLEMENTATION
#define ZEST_TEST_MODE
#include "zest-tests.h"
#include "zest.h"
#include "imgui_internal.h"
#include "zest-frame-graph-tests.cpp"
#include "zest-frame-graph-stress.cpp"

void InitialiseTests(ZestTests *tests) {

	tests->tests[0] = { "Empty Graph", test__empty_graph, 0, 0, zest_fgs_no_work_to_do, tests->simple_create_info};
	tests->tests[1] = { "Single Pass", test__single_pass, 0, 0, zest_fgs_no_work_to_do, tests->simple_create_info};
	tests->tests[2] = { "Blank Screen", test__blank_screen, 0, 0, 0, tests->simple_create_info };
	tests->tests[3] = { "Pass Culling", test__pass_culling, 0, 0, 0, tests->simple_create_info };
	tests->tests[4] = { "Resource Culling", test__resource_culling, 0, 0, 1, tests->simple_create_info };
	tests->tests[5] = { "Chained Pass Culling", test__chained_pass_culling, 0, 0, 0, tests->simple_create_info };
	tests->tests[6] = { "Transient Image", test__transient_image, 0, 0, 0, tests->simple_create_info };
	tests->tests[7] = { "Import Image", test__import_image, 0, 0, 0, tests->simple_create_info };
	tests->tests[8] = { "Image Barriers", test__image_barrier_tests, 0, 0, 0, tests->simple_create_info };
	tests->tests[9] = { "Buffer Read/Write", test__buffer_read_write, 0, 0, 0, tests->simple_create_info };
	tests->tests[10] = { "Multi Reader Barrier", test__multi_reader_barrier, 0, 0, 0, tests->simple_create_info };
	tests->tests[11] = { "Image Write/Read", test__image_read_write, 0, 0, 0, tests->simple_create_info };
	tests->tests[12] = { "Depth Attachment", test__depth_attachment, 0, 0, 0, tests->depth_create_info };
	tests->tests[13] = { "Multi Queue Sync", test__multi_queue_sync, 0, 0, 0, tests->simple_create_info };
	tests->tests[14] = { "Pass Grouping", test__pass_grouping, 0, 0, 0, tests->simple_create_info };
	tests->tests[15] = { "Cyclic Dependency", test__cyclic_dependency, 0, 0, zest_fgs_cyclic_dependency, tests->simple_create_info };
	tests->tests[16] = { "Simple Graph Cache", test__simple_caching, 0, 0, 0, tests->simple_create_info };
	tests->tests[17] = { "Stress Test Simple", test__stress_simple, 0, 0, 0, tests->simple_create_info };
	tests->tests[18] = { "Stress Test Pass Dependencies", test__stress_pass_dependencies, 0, 0, 0, tests->simple_create_info };
	tests->tests[19] = { "Stress Test Pass Dependency Chain", test__stress_pass_dependency_chain, 0, 0, 0, tests->simple_create_info };
	tests->tests[20] = { "Stress Test Transient Buffers", test__stress_transient_buffers, 0, 0, 0, tests->simple_create_info };
	tests->tests[21] = { "Stress Test Transient Images", test__stress_transient_images, 0, 0, 0, tests->simple_create_info };
	tests->tests[22] = { "Stress Test All Transients", test__stress_all_transients, 0, 0, 0, tests->simple_create_info };
	tests->tests[23] = { "Stress Test Multi Queue", test__stress_multi_queue_sync, 0, 0, 0, tests->simple_create_info };

	tests->sampler_info = zest_CreateSamplerInfo();

	tests->current_test = 23;
    zest_ResetValidationErrors(tests->device);
}

void ResetTests(ZestTests *tests) {
	tests->texture = { 0 };
	tests->compute_verify = { 0 };
	tests->compute_write = { 0 };
	tests->cpu_buffer = { 0 };
	tests->sampler = { 0 };
	tests->mipped_sampler = { 0 };
}

void RunTests(ZestTests *tests) {
	int completed_tests = 0;

	while (1) {
		zest_UpdateDevice(tests->device);
		Test *current_test = &tests->tests[tests->current_test];
		int result = current_test->the_test(tests, current_test);

		if (current_test->frame_count == ZEST_MAX_FIF) {
			if (current_test->result != current_test->expected_result) {
				ZEST_PRINT("\033[31m%s test failed\033[0m", current_test->name);
			} else {
				ZEST_PRINT("\033[32m%s test passed\033[0m", current_test->name);
				completed_tests++;
			}
			if (tests->current_test < TEST_COUNT - 1) {
				tests->current_test++;
				zest_SetCreateInfo(tests->context, &tests->tests[tests->current_test].create_info);
				zest_ResetContext(tests->context, 0);
				zest_ResetDevice(tests->device);
				tests->stress_resources.image_count = 0;
				tests->stress_resources.buffer_count = 0;
				zest_ResetValidationErrors(tests->device);
				ResetTests(tests);
			} else {
				break;
			}
		}
	}
	ZEST_PRINT("%sTests completed: %i / %i\033[0m", completed_tests == TEST_COUNT == 0 ? "\033[31m" : "\033[32m", completed_tests, TEST_COUNT);
}

#if defined(_WIN32)
// Windows entry point
int main(void) {

	//Create new config struct for Zest
	zest_create_info_t create_info = zest_CreateInfoWithValidationLayers(zest_validation_flag_enable_sync | zest_validation_flag_best_practices);

	ZestTests tests = {};
	tests.simple_create_info = create_info;
	tests.depth_create_info = create_info;

	if (!glfwInit()) {
		return 0;
	}
	zest_uint count;
	const char **glfw_extensions = glfwGetRequiredInstanceExtensions(&count);

	//Create the device that serves all vulkan based context
	zest_device_builder device_builder = zest_BeginVulkanDeviceBuilder();
	zest_AddDeviceBuilderExtensions(device_builder, glfw_extensions, count);
	zest_AddDeviceBuilderValidation(device_builder);
	zest_DeviceBuilderLogToConsole(device_builder);
	zest_DeviceBuilderLogToMemory(device_builder);
	tests.device = zest_EndDeviceBuilder(device_builder);

	//Create a window using GLFW
	zest_window_data_t window_handles = zest_implglfw_CreateWindow(50, 50, 1280, 768, 0, "PBR Simple Example");
	//Initialise Zest
	tests.context = zest_CreateContext(tests.device, &window_handles, &create_info);

	InitialiseTests(&tests);

	RunTests(&tests);
	
	//Start the main loop
	zest_DestroyDevice(tests.device);

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