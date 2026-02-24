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

	RegisterTest(tests, { "Empty Graph", test__empty_graph, 0, ZEST_MAX_FIF, 0, zest_fgs_no_work_to_do, tests->simple_create_info });
	RegisterTest(tests, { "Single Pass", test__single_pass, 0, ZEST_MAX_FIF, 0, zest_fgs_no_work_to_do, tests->simple_create_info });
	RegisterTest(tests, { "Blank Screen", test__blank_screen, 0, ZEST_MAX_FIF, 0, 0, tests->simple_create_info });
	RegisterTest(tests, { "Pass Culling", test__pass_culling, 0, ZEST_MAX_FIF, 0, 0, tests->simple_create_info });
	RegisterTest(tests, { "Resource Culling", test__resource_culling, 0, ZEST_MAX_FIF, 0, 1, tests->simple_create_info });
	RegisterTest(tests, { "Chained Pass Culling", test__chained_pass_culling, 0, ZEST_MAX_FIF, 0, 0, tests->simple_create_info });
	RegisterTest(tests, { "Transient Image", test__transient_image, 0, ZEST_MAX_FIF, 0, 0, tests->simple_create_info });
	RegisterTest(tests, { "Import Image", test__import_image, 0, ZEST_MAX_FIF, 0, 0, tests->simple_create_info });
	RegisterTest(tests, { "Image Barriers", test__image_barrier_tests, 0, ZEST_MAX_FIF, 0, 0, tests->simple_create_info });
	RegisterTest(tests, { "Buffer Read/Write", test__buffer_read_write, 0, ZEST_MAX_FIF, 0, 0, tests->simple_create_info });
	RegisterTest(tests, { "Multi Reader Barrier", test__multi_reader_barrier, 0, ZEST_MAX_FIF, 0, 0, tests->simple_create_info });
	RegisterTest(tests, { "Image Write/Read", test__image_read_write, 0, ZEST_MAX_FIF, 0, 0, tests->simple_create_info });
	RegisterTest(tests, { "Depth Attachment", test__depth_attachment, 0, ZEST_MAX_FIF, 0, 0, tests->depth_create_info });
	RegisterTest(tests, { "Multi Queue Sync", test__multi_queue_sync, 0, ZEST_MAX_FIF, 0, 0, tests->simple_create_info });
	RegisterTest(tests, { "Pass Grouping", test__pass_grouping, 0, ZEST_MAX_FIF, 0, 0, tests->simple_create_info });
	RegisterTest(tests, { "Cyclic Dependency", test__cyclic_dependency, ZEST_MAX_FIF, 0, 0, zest_fgs_cyclic_dependency, tests->simple_create_info });
	RegisterTest(tests, { "Simple Graph Cache", test__simple_caching, 0, ZEST_MAX_FIF, 0, 0, tests->simple_create_info });
	RegisterTest(tests, { "Stress Test Simple", test__stress_simple, 0, ZEST_MAX_FIF, 0, 0, tests->simple_create_info });
	RegisterTest(tests, { "Stress Test Pass Dependencies", test__stress_pass_dependencies, 0, ZEST_MAX_FIF, 0, 0, tests->simple_create_info });
	RegisterTest(tests, { "Stress Test Pass Dependency Chain", test__stress_pass_dependency_chain, 0, ZEST_MAX_FIF, 0, 0, tests->simple_create_info });
	RegisterTest(tests, { "Stress Test Transient Buffers", test__stress_transient_buffers, 0, ZEST_MAX_FIF, 0, 0, tests->simple_create_info });
	RegisterTest(tests, { "Stress Test Transient Images", test__stress_transient_images, 0, ZEST_MAX_FIF, 0, 0, tests->simple_create_info });
	RegisterTest(tests, { "Stress Test All Transients", test__stress_all_transients, 0, ZEST_MAX_FIF, 0, 0, tests->simple_create_info });
	RegisterTest(tests, { "Stress Test Multi Queue", test__stress_multi_queue_sync, 0, ZEST_MAX_FIF, 0, 0, tests->simple_create_info });
	RegisterTest(tests, { "Pipeline Test State Depth", test__pipeline_state_depth, 0, ZEST_MAX_FIF, 0, 0, tests->simple_create_info });
	RegisterTest(tests, { "Pipeline Test State Blending", test__pipeline_state_blending, 0, 1, 0, 0, tests->simple_create_info });
	RegisterTest(tests, { "Pipeline Test State Culling", test__pipeline_state_culling, 0, 1, 0, 0, tests->simple_create_info });
	RegisterTest(tests, { "Pipeline Test State Topology", test__pipeline_state_topology, 0, 1, 0, 0, tests->simple_create_info });
	RegisterTest(tests, { "Pipeline Test State Polygon Mode", test__pipeline_state_polygon_mode, 0, 1, 0, 0, tests->simple_create_info });
	RegisterTest(tests, { "Pipeline Test State Front Face", test__pipeline_state_front_face, 0, 1, 0, 0, tests->simple_create_info });
	RegisterTest(tests, { "Pipeline Test State Vertex Input", test__pipeline_state_vertex_input, 0, 1, 0, 0, tests->simple_create_info });
	RegisterTest(tests, { "Pipeline Test State MultiBlend", test__pipeline_state_multiblend, 0, 1, 0, 0, tests->simple_create_info });
	RegisterTest(tests, { "Pipeline Test State Rasterization", test__pipeline_state_rasterization, 0, 1, 0, 0, tests->simple_create_info });
	RegisterTest(tests, { "Resource Test Image Format Support", test__image_format_support, 0, 1, 0, 0, tests->simple_create_info });
	RegisterTest(tests, { "Resource Test Image Creation Destruction", test__image_creation_destruction, 0, 1, 0, 0, tests->simple_create_info });
	RegisterTest(tests, { "Resource Test Image Format Edge Cases", test__image_format_validation_edge_cases, 0, 1, 0, 0, tests->simple_create_info });
	RegisterTest(tests, { "Resource Test Image View Creation", test__image_view_creation, 0, 1, 0, 0, tests->simple_create_info });
	RegisterTest(tests, { "Resource Test Buffer Creation Destruction", test__buffer_creation_destruction, 0, 1, 0, 0, tests->simple_create_info });
	RegisterTest(tests, { "Resource Test Buffer Edge Cases", test__buffer_edge_cases, 0, 1, 0, 0, tests->simple_create_info });
	RegisterTest(tests, { "Resource Test Uniform Buffer Descriptor Management", test__uniform_buffer_descriptor_management, 0, 1, 0, 0, tests->simple_create_info });
	RegisterTest(tests, { "Resource Test Uniform Buffer Edge Cases", test__uniform_buffer_edge_cases, 0, 1, 0, 0, tests->simple_create_info });
	RegisterTest(tests, { "Resource Test Staging Buffer Operations", test__staging_buffer_operations, 0, 1, 0, 0, tests->simple_create_info });
	RegisterTest(tests, { "Resource Test Image With Pixels Creation", test__image_with_pixels_creation, 0, 1, 0, 0, tests->simple_create_info });
	RegisterTest(tests, { "Resource Test Sampler Creation", test__sampler_creation, 0, 1, 0, 0, tests->simple_create_info });
	RegisterTest(tests, { "Resource Test Image Array Descriptor Indexes", test__image_array_descriptor_indexes, 0, 1, 0, 0, tests->simple_create_info });
	RegisterTest(tests, { "Resource Test Compute Shader Resources", test__compute_shader_resources, 0, 1, 0, 0, tests->simple_create_info });
	RegisterTest(tests, { "User Test No Update Device", test__no_update_device, 0, 1, 0, 0, tests->simple_create_info });
	RegisterTest(tests, { "User Test No End Frame", test__no_end_frame, 0, 1, 0, 0, tests->simple_create_info });
	RegisterTest(tests, { "User Test No Swapchain Import", test__no_swapchain_import, 0, 1, 0, 0, tests->simple_create_info });
	RegisterTest(tests, { "User Test Missing End Pass", test__no_end_pass, 0, 1, 0, 0, tests->simple_create_info });
	RegisterTest(tests, { "User Test Bad Frame Graph Ordering", test__bad_frame_graph_ordering, 0, 1, 0, 0, tests->simple_create_info });
	RegisterTest(tests, { "User Test Frame Graph State Errors", test__frame_graph_state_errors, 0, 1, 0, 0, tests->simple_create_info });
	RegisterTest(tests, { "User Test Pass Without Task", test__pass_without_task, 0, 1, 0, 0, tests->simple_create_info });
	RegisterTest(tests, { "User Test Unused Imported Resource", test__unused_imported_resource, 0, 1, 0, 0, tests->simple_create_info });
	RegisterTest(tests, { "User Test Unused Swapchain", test__unused_swapchain, 0, 1, 0, 0, tests->simple_create_info });
	RegisterTest(tests, { "User Test Buffer Output In Render Pass", test__buffer_output_in_render_pass, 0, 1, 0, 0, tests->simple_create_info });
	RegisterTest(tests, { "User Test Multiple Swapchain Imports", test__multiple_swapchain_imports, 0, 1, 0, 0, tests->simple_create_info });
	RegisterTest(tests, { "User Test Transient Dependency Ordering", test__transient_dependency_ordering, 0, 1, 0, 0, tests->simple_create_info });
	RegisterTest(tests, { "User Test Resources Named the Same", test__resources_with_same_name, 0, 1, 0, 0, tests->simple_create_info });
	RegisterTest(tests, { "User Test Add the same resource twice", test__add_2_resources_the_same, 0, 1, 0, 0, tests->simple_create_info });
	RegisterTest(tests, { "User Test Acquire/Release Indexes", test__acquire_release_resources, 0, 1, 0, 0, tests->simple_create_info });
	RegisterTest(tests, { "Compute Test Frame Graph and Execute", test__frame_graph_and_execute, 0, 1, 0, 0, tests->simple_create_info });
	RegisterTest(tests, { "Compute Test Timeline Wait External", test__timeline_wait_external, 0, 1, 0, 0, tests->simple_create_info });
	RegisterTest(tests, { "Compute Test Timeline Timeout", test__timeline_timeout, 0, 1, 0, 0, tests->simple_create_info });
	RegisterTest(tests, { "Compute Test Immediate Execute Cached", test__immediate_execute_cached, 0, 1, 0, 0, tests->simple_create_info });
	RegisterTest(tests, { "Compute Test Mipmap Chain", test__compute_mipmap_chain, 0, 1, 0, 0, tests->simple_create_info });
	RegisterTest(tests, { "Compute Test Multiple Timeline Signals", test__multiple_timeline_signals, 0, 1, 0, 0, tests->simple_create_info });
	RegisterTest(tests, { "Compute Test Read Modify Write", test__compute_read_modify_write, 0, 1, 0, 0, tests->simple_create_info });
	RegisterTest(tests, { "Compute Test Only Graph", test__compute_only_graph, 0, 1, 0, 0, tests->simple_create_info });
	RegisterTest(tests, { "Compute Test Immediate Execute No Wait", test__immediate_execute_no_wait, 0, 1, 0, 0, tests->simple_create_info });
	// Try to release descriptor indexes that are already released or don't exist for a certain binding

	ZEST_PRINT("Total Tests: %u", tests->test_count);
	tests->sampler_info = zest_CreateSamplerInfo();
	tests->current_test = 0;
    zest_ResetValidationErrors(tests->device);
}

void InitialiseSpecificTests(ZestTests *tests) {
	RegisterTest(tests, { "User Test Bad Frame Graph Ordering", test__bad_frame_graph_ordering, 0, 1, 0, 0, tests->simple_create_info });
	RegisterTest(tests, { "User Test Frame Graph State Errors", test__frame_graph_state_errors, 0, 1, 0, 0, tests->simple_create_info });
	RegisterTest(tests, { "User Test Pass Without Task", test__pass_without_task, 0, 1, 0, 0, tests->simple_create_info });
	RegisterTest(tests, { "User Test Unused Imported Resource", test__unused_imported_resource, 0, 1, 0, 0, tests->simple_create_info });
	RegisterTest(tests, { "User Test Unused Swapchain", test__unused_swapchain, 0, 1, 0, 0, tests->simple_create_info });
	RegisterTest(tests, { "User Test Buffer Output In Render Pass", test__buffer_output_in_render_pass, 0, 1, 0, 0, tests->simple_create_info });
	RegisterTest(tests, { "User Test Multiple Swapchain Imports", test__multiple_swapchain_imports, 0, 1, 0, 0, tests->simple_create_info });
	RegisterTest(tests, { "User Test Transient Dependency Ordering", test__transient_dependency_ordering, 0, 1, 0, 0, tests->simple_create_info });
	RegisterTest(tests, { "User Test Resources Named the Same", test__resources_with_same_name, 0, 1, 0, 0, tests->simple_create_info });
	RegisterTest(tests, { "User Test Add the same resource twice", test__add_2_resources_the_same, 0, 1, 0, 0, tests->simple_create_info });
	RegisterTest(tests, { "User Test Acquire/Release Indexes", test__acquire_release_resources, 0, 1, 0, 0, tests->simple_create_info });
	RegisterTest(tests, { "Compute Test Frame Graph and Execute", test__frame_graph_and_execute, 0, 1, 0, 0, tests->simple_create_info });
	RegisterTest(tests, { "Compute Test Timeline Wait External", test__timeline_wait_external, 0, 1, 0, 0, tests->simple_create_info });
	RegisterTest(tests, { "Compute Test Timeline Timeout", test__timeline_timeout, 0, 1, 0, 0, tests->simple_create_info });
	RegisterTest(tests, { "Compute Test Immediate Execute Cached", test__immediate_execute_cached, 0, 1, 0, 0, tests->simple_create_info });
	RegisterTest(tests, { "Compute Test Mipmap Chain", test__compute_mipmap_chain, 0, 1, 0, 0, tests->simple_create_info });
	RegisterTest(tests, { "Compute Test Multiple Timeline Signals", test__multiple_timeline_signals, 0, 1, 0, 0, tests->simple_create_info });
	RegisterTest(tests, { "Compute Test Read Modify Write", test__compute_read_modify_write, 0, 1, 0, 0, tests->simple_create_info });
	RegisterTest(tests, { "Compute Test Only Graph", test__compute_only_graph, 0, 1, 0, 0, tests->simple_create_info });
	RegisterTest(tests, { "Compute Test Immediate Execute No Wait", test__immediate_execute_no_wait, 0, 1, 0, 0, tests->simple_create_info });
	tests->sampler_info = zest_CreateSamplerInfo();
	tests->current_test = 0;
    zest_ResetValidationErrors(tests->device);
}

void RegisterTest(ZestTests *tests, Test test) {
	tests->tests[tests->test_count] = test;
	tests->test_count++;
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
			if (tests->current_test < tests->test_count - 1) {
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
	ZEST_PRINT("%sTests completed: %i / %i\033[0m", completed_tests == tests->test_count == 0 ? "\033[31m" : "\033[32m", completed_tests, tests->test_count);
}

int main(int argc, char *argv[]) {
	//Create new config struct for Zest
	zest_create_context_info_t create_info = zest_CreateContextInfo();

	ZestTests tests = {};
	tests.simple_create_info = create_info;
	tests.depth_create_info = create_info;

	zest_window_data_t window_data = zest_implsdl2_CreateWindow(50, 50, 1280, 768, 0, "Tests");

	unsigned int count = 0;
	SDL_Vulkan_GetInstanceExtensions((SDL_Window*)window_data.window_handle, &count, NULL);
	const char** sdl_extensions = (const char**)malloc(sizeof(const char*) * count);
	SDL_Vulkan_GetInstanceExtensions((SDL_Window*)window_data.window_handle, &count, sdl_extensions);

	// Create the device that serves all vulkan based contexts
	zest_device_builder device_builder = zest_BeginVulkanDeviceBuilder();
	zest_AddDeviceBuilderExtensions(device_builder, sdl_extensions, count);
	zest_AddDeviceBuilderValidation(device_builder);
	zest_DeviceBuilderLogToConsole(device_builder);
	zest_DeviceBuilderLogToMemory(device_builder);
	tests.device = zest_EndDeviceBuilder(device_builder);

	// Clean up the extensions array
	free(sdl_extensions);

	//Initialise Zest
	tests.context = zest_CreateContext(tests.device, &window_data, &create_info);

	InitialiseTests(&tests);
	//InitialiseSpecificTests(&tests);

	RunTests(&tests);
	
	//Start the main loop
	zest_DestroyDevice(tests.device);

	return 0;
}