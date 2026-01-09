#define ZEST_IMPLEMENTATION
#define ZEST_VULKAN_IMPLEMENTATION
#define ZEST_TEST_MODE
#include "zest-tests.h"
#include "zest.h"
#include "imgui_internal.h"
#include "zest-frame-graph-tests.cpp"
#include "zest-frame-graph-stress.cpp"
#include "zest-pipeline-tests.cpp"
#include "zest-resource-management-tests.cpp"
#include "zest-user-tests.cpp"
#include "zest-compute-tests.cpp"

void InitialiseTests(ZestTests *tests) {

	tests->tests[0] = { "Empty Graph", test__empty_graph, 0, ZEST_MAX_FIF, 0, zest_fgs_no_work_to_do, tests->simple_create_info};
	tests->tests[1] = { "Single Pass", test__single_pass, 0, ZEST_MAX_FIF, 0, zest_fgs_no_work_to_do, tests->simple_create_info};
	tests->tests[2] = { "Blank Screen", test__blank_screen, 0, ZEST_MAX_FIF, 0, 0, tests->simple_create_info };
	tests->tests[3] = { "Pass Culling", test__pass_culling, 0, ZEST_MAX_FIF, 0, 0, tests->simple_create_info };
	tests->tests[4] = { "Resource Culling", test__resource_culling, 0, ZEST_MAX_FIF, 0, 1, tests->simple_create_info };
	tests->tests[5] = { "Chained Pass Culling", test__chained_pass_culling, 0, ZEST_MAX_FIF, 0, 0, tests->simple_create_info };
	tests->tests[6] = { "Transient Image", test__transient_image, 0, ZEST_MAX_FIF, 0, 0, tests->simple_create_info };
	tests->tests[7] = { "Import Image", test__import_image, 0, ZEST_MAX_FIF, 0, 0, tests->simple_create_info };
	tests->tests[8] = { "Image Barriers", test__image_barrier_tests, 0, ZEST_MAX_FIF, 0, 0, tests->simple_create_info };
	tests->tests[9] = { "Buffer Read/Write", test__buffer_read_write, 0, ZEST_MAX_FIF, 0, 0, tests->simple_create_info };
	tests->tests[10] = { "Multi Reader Barrier", test__multi_reader_barrier, 0, ZEST_MAX_FIF, 0, 0, tests->simple_create_info };
	tests->tests[11] = { "Image Write/Read", test__image_read_write, 0, ZEST_MAX_FIF, 0, 0, tests->simple_create_info };
	tests->tests[12] = { "Depth Attachment", test__depth_attachment, 0, ZEST_MAX_FIF, 0, 0, tests->depth_create_info };
	tests->tests[13] = { "Multi Queue Sync", test__multi_queue_sync, 0, ZEST_MAX_FIF, 0, 0, tests->simple_create_info };
	tests->tests[14] = { "Pass Grouping", test__pass_grouping, 0, ZEST_MAX_FIF, 0, 0, tests->simple_create_info };
	tests->tests[15] = { "Cyclic Dependency", test__cyclic_dependency, ZEST_MAX_FIF, 0, 0, zest_fgs_cyclic_dependency, tests->simple_create_info };
	tests->tests[16] = { "Simple Graph Cache", test__simple_caching, 0, ZEST_MAX_FIF, 0, 0, tests->simple_create_info };
	tests->tests[17] = { "Stress Test Simple", test__stress_simple, 0, ZEST_MAX_FIF, 0, 0, tests->simple_create_info };
	tests->tests[18] = { "Stress Test Pass Dependencies", test__stress_pass_dependencies, 0, ZEST_MAX_FIF, 0, 0, tests->simple_create_info };
	tests->tests[19] = { "Stress Test Pass Dependency Chain", test__stress_pass_dependency_chain, 0, ZEST_MAX_FIF, 0, 0, tests->simple_create_info };
	tests->tests[20] = { "Stress Test Transient Buffers", test__stress_transient_buffers, 0, ZEST_MAX_FIF, 0, 0, tests->simple_create_info };
	tests->tests[21] = { "Stress Test Transient Images", test__stress_transient_images, 0, ZEST_MAX_FIF, 0, 0, tests->simple_create_info };
	tests->tests[22] = { "Stress Test All Transients", test__stress_all_transients, 0, ZEST_MAX_FIF, 0, 0, tests->simple_create_info };
	tests->tests[23] = { "Stress Test Multi Queue", test__stress_multi_queue_sync, 0, ZEST_MAX_FIF, 0, 0, tests->simple_create_info };
	tests->tests[24] = { "Pipeline Test State Depth", test__pipeline_state_depth, 0, ZEST_MAX_FIF, 0, 0, tests->simple_create_info };
	tests->tests[25] = { "Pipeline Test State Blending", test__pipeline_state_blending, 0, 1, 0, 0, tests->simple_create_info };
	tests->tests[26] = { "Pipeline Test State Culling", test__pipeline_state_culling, 0, 1, 0, 0, tests->simple_create_info };
	tests->tests[27] = { "Pipeline Test State Topology", test__pipeline_state_topology, 0, 1, 0, 0, tests->simple_create_info };
	tests->tests[28] = { "Pipeline Test State Polygon Mode", test__pipeline_state_polygon_mode, 0, 1, 0, 0, tests->simple_create_info };
	tests->tests[29] = { "Pipeline Test State Front Face", test__pipeline_state_front_face, 0, 1, 0, 0, tests->simple_create_info };
	tests->tests[30] = { "Pipeline Test State Vertex Input", test__pipeline_state_vertex_input, 0, 1, 0, 0, tests->simple_create_info };
	tests->tests[31] = { "Pipeline Test State MultiBlend", test__pipeline_state_multiblend, 0, 1, 0, 0, tests->simple_create_info };
	tests->tests[32] = { "Pipeline Test State Rasterization", test__pipeline_state_rasterization, 0, 1, 0, 0, tests->simple_create_info };
	tests->tests[33] = { "Resource Test Image Format Support", test__image_format_support, 0, 1, 0, 0, tests->simple_create_info };
	tests->tests[34] = { "Resource Test Image Creation Destruction", test__image_creation_destruction, 0, 1, 0, 0, tests->simple_create_info };
	tests->tests[35] = { "Resource Test Image Format Edge Cases", test__image_format_validation_edge_cases, 0, 1, 0, 0, tests->simple_create_info };
	tests->tests[36] = { "Resource Test Image View Creation", test__image_view_creation, 0, 1, 0, 0, tests->simple_create_info };
	tests->tests[37] = { "Resource Test Buffer Creation Destruction", test__buffer_creation_destruction, 0, 1, 0, 0, tests->simple_create_info };
	tests->tests[38] = { "Resource Test Buffer Edge Cases", test__buffer_edge_cases, 0, 1, 0, 0, tests->simple_create_info };
	tests->tests[39] = { "Resource Test Uniform Buffer Descriptor Management", test__uniform_buffer_descriptor_management, 0, 1, 0, 0, tests->simple_create_info };
	tests->tests[40] = { "Resource Test Uniform Buffer Edge Cases", test__uniform_buffer_edge_cases, 0, 1, 0, 0, tests->simple_create_info };
	tests->tests[41] = { "Resource Test Staging Buffer Operations", test__staging_buffer_operations, 0, 1, 0, 0, tests->simple_create_info };
	tests->tests[42] = { "Resource Test Image With Pixels Creation", test__image_with_pixels_creation, 0, 1, 0, 0, tests->simple_create_info };
	tests->tests[43] = { "Resource Test Sampler Creation", test__sampler_creation, 0, 1, 0, 0, tests->simple_create_info };
	tests->tests[44] = { "Resource Test Image Array Descriptor Indexes", test__image_array_descriptor_indexes, 0, 1, 0, 0, tests->simple_create_info };
	tests->tests[45] = { "Resource Test Compute Shader Resources", test__compute_shader_resources, 0, 1, 0, 0, tests->simple_create_info };
	tests->tests[46] = { "User Test No Update Device", test__no_update_device, 0, 1, 0, 0, tests->simple_create_info };
	tests->tests[47] = { "User Test No End Frame", test__no_end_frame, 0, 1, 0, 0, tests->simple_create_info };
	tests->tests[48] = { "User Test No Swapchain Import", test__no_swapchain_import, 0, 1, 0, 0, tests->simple_create_info };
	tests->tests[49] = { "User Test Missing End Pass", test__no_end_pass, 0, 1, 0, 0, tests->simple_create_info };
	tests->tests[50] = { "User Test Bad Frame Graph Ordering", test__bad_frame_graph_ordering, 0, 1, 0, 0, tests->simple_create_info };
	tests->tests[51] = { "User Test Frame Graph State Errors", test__frame_graph_state_errors, 0, 1, 0, 0, tests->simple_create_info };
	tests->tests[52] = { "User Test Pass Without Task", test__pass_without_task, 0, 1, 0, 0, tests->simple_create_info };
	tests->tests[53] = { "User Test Unused Imported Resource", test__unused_imported_resource, 0, 1, 0, 0, tests->simple_create_info };
	tests->tests[54] = { "User Test Unused Swapchain", test__unused_swapchain, 0, 1, 0, 0, tests->simple_create_info };
	tests->tests[55] = { "User Test Buffer Output In Render Pass", test__buffer_output_in_render_pass, 0, 1, 0, 0, tests->simple_create_info };
	tests->tests[56] = { "User Test Multiple Swapchain Imports", test__multiple_swapchain_imports, 0, 1, 0, 0, tests->simple_create_info };
	tests->tests[57] = { "User Test Transient Dependency Ordering", test__transient_dependency_ordering, 0, 1, 0, 0, tests->simple_create_info };
	tests->tests[58] = { "User Test Invalid Compute Handle", test__invalid_compute_handle, 0, 1, 0, 0, tests->simple_create_info };
	tests->tests[59] = { "Compute Test Frame Graph and Execute", test__frame_graph_and_execute, 0, 1, 0, 0, tests->simple_create_info };
	tests->tests[60] = { "Compute Test Timeline Wait External", test__timeline_wait_external, 0, 1, 0, 0, tests->simple_create_info };
	tests->tests[61] = { "Compute Test Timeline Timeout", test__timeline_timeout, 0, 1, 0, 0, tests->simple_create_info };
	tests->tests[62] = { "Compute Test Immediate Execute Cached", test__immediate_execute_cached, 0, 1, 0, 0, tests->simple_create_info };
	tests->tests[63] = { "Compute Test Mipmap Chain", test__compute_mipmap_chain, 0, 1, 0, 0, tests->simple_create_info };
	tests->tests[64] = { "Compute Test Multiple Timeline Signals", test__multiple_timeline_signals, 0, 1, 0, 0, tests->simple_create_info };
	tests->tests[65] = { "Compute Test Read Modify Write", test__compute_read_modify_write, 0, 1, 0, 0, tests->simple_create_info };
	tests->tests[66] = { "Compute Test Only Graph", test__compute_only_graph, 0, 1, 0, 0, tests->simple_create_info };
	tests->tests[67] = { "Compute Test Immediate Execute No Wait", test__immediate_execute_no_wait, 0, 1, 0, 0, tests->simple_create_info };
	// Comment out remaining tests until we get working build
	// Need a test for calling multiple resources the same name
	// Can we test for adding the same resource twice?
	// Test for memory leak when a layer is not reset each frame
	// Can we test for push constant values being suspicially out of range?
	// Test for functions not being ecapsulated within a begin/end frame.
	// Try to release descriptor indexes that are already release or don't exist for a certain binding
	// tests->tests[44] = { "Resource Test Memory Pool Configuration", test__memory_pool_configuration, 0, 0, 0, tests->simple_create_info };
	// tests->tests[45] = { "Resource Test Memory Pool Exhaustion", test__memory_pool_exhaustion, 0, 0, 0, tests->simple_create_info };
	// tests->tests[46] = { "Resource Test Memory Pool Queries", test__memory_pool_queries, 0, 0, 0, tests->simple_create_info };
	// tests->tests[47] = { "Resource Test Memory Pool Optimization", test__memory_pool_optimization, 0, 0, 0, tests->simple_create_info };
	// tests->tests[48] = { "Resource Test Lifetime Management", test__resource_lifetime_management, 0, 0, 0, tests->simple_create_info };
	// tests->tests[49] = { "Resource Test State Transitions", test__resource_state_transitions, 0, 0, 0, tests->simple_create_info };
	// tests->tests[50] = { "Resource Test Frame Sync", test__resource_frame_sync, 0, 0, 0, tests->simple_create_info };
	// tests->tests[51] = { "Resource Test Cleanup Validation", test__resource_cleanup_validation, 0, 0, 0, tests->simple_create_info };
	// tests->tests[52] = { "Resource Test Aliasing Optimization", test__resource_aliasing_optimization, 0, 0, 0, tests->simple_create_info };
	// tests->tests[53] = { "Resource Test Provider Functionality", test__resource_provider_functionality, 0, 0, 0, tests->simple_create_info };
	// tests->tests[54] = { "Resource Test Userdata Management", test__resource_userdata_management, 0, 0, 0, tests->simple_create_info };
	// tests->tests[55] = { "Resource Test Invalid Parameters", test__invalid_resource_parameters, 0, 0, 0, tests->simple_create_info };
	// tests->tests[56] = { "Resource Test Leak Detection", test__resource_leak_detection, 0, 0, 0, tests->simple_create_info };
	// tests->tests[57] = { "Resource Test Concurrent Access", test__concurrent_resource_access, 0, 0, 0, tests->simple_create_info };

	tests->sampler_info = zest_CreateSamplerInfo();

	tests->current_test = 0;
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

void PrintTestUpdate(Test *test, int phase, zest_bool passed) {
	if (passed) {
		ZEST_PRINT("\033[32m\tphase %i of test %s passed\033[0m", phase, test->name);
	} else {
		ZEST_PRINT("\033[31m\tphase %i of test %s failed\033[0m", phase, test->name);
	}
}

void RunTests(ZestTests *tests) {
	int completed_tests = 0;

	while (1) {
		Test *current_test = &tests->tests[tests->current_test];
		int result = current_test->the_test(tests, current_test);

		if (current_test->frame_count >= current_test->run_count) {
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
				zest_ResetReports(tests->context);
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
	zest_create_context_info_t create_info = zest_CreateContextInfo();

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
	device_builder->graphics_queue_count = -1;
	device_builder->compute_queue_count = -1;
	device_builder->transfer_queue_count = -1;
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
	zest_create_context_info_t create_info = zest_CreateContextInfo();
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