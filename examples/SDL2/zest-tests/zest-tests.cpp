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
#include "zest-layer-tests.cpp"
#include "zest-device-reset-tests.cpp"

void InitialiseTests(ZestTests *tests) {
	RegisterTest(tests, { "Empty Graph", test__empty_graph, 0, ZEST_MAX_FIF, 0, zest_fgs_no_work_to_do, tests->headless_create_info });
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
	RegisterTest(tests, { "Depth Attachment", test__depth_attachment, 0, ZEST_MAX_FIF, 0, 0, tests->simple_create_info });
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
	RegisterTest(tests, { "Compute Test Frame Graph and Execute", test__frame_graph_and_execute, 0, 1, 0, 0, tests->headless_create_info });
	RegisterTest(tests, { "Compute Test Timeline Wait External", test__timeline_wait_external, 0, 1, 0, 0, tests->headless_create_info });
	RegisterTest(tests, { "Compute Test Timeline Timeout", test__timeline_timeout, 0, 1, 0, 0, tests->headless_create_info });
	RegisterTest(tests, { "Compute Test Immediate Execute Cached", test__immediate_execute_cached, 0, 1, 0, 0, tests->headless_create_info });
	RegisterTest(tests, { "Compute Test Mipmap Chain", test__compute_mipmap_chain, 0, 1, 0, 0, tests->headless_create_info });
	RegisterTest(tests, { "Compute Test Multiple Timeline Signals", test__multiple_timeline_signals, 0, 1, 0, 0, tests->headless_create_info });
	RegisterTest(tests, { "Compute Test Read Modify Write", test__compute_read_modify_write, 0, 1, 0, 0, tests->headless_create_info });
	RegisterTest(tests, { "Compute Test Only Graph", test__compute_only_graph, 0, 1, 0, 0, tests->headless_create_info });
	RegisterTest(tests, { "Compute Test Immediate Execute No Wait", test__immediate_execute_no_wait, 0, 1, 0, 0, tests->headless_create_info });
	RegisterTest(tests, { "Compute Test WAW Barrier", test__compute_waw_barrier, 0, 1, 0, 0, tests->headless_create_info });
	RegisterTest(tests, { "Compute Test Flush Compile Failure No Hang", test__flush_compile_failure_no_hang, 0, 1, 0, 0, tests->headless_create_info });
	//Registered last: this test perturbs the bindless index free list (it creates transient
	//images), which the Acquire/Release Indexes test is sensitive to as it releases hardcoded
	//index values.
	RegisterTest(tests, { "Resource Test Transient Aliasing Dependency", test__transient_aliasing_dependency, 0, ZEST_MAX_FIF, 0, 0, tests->simple_create_info });
	RegisterTest(tests, { "Essential Unused Transient", test__essential_unused_transient, 0, ZEST_MAX_FIF, 0, 0, tests->simple_create_info });
	RegisterTest(tests, { "Versioned Transient", test__versioned_transient, 0, ZEST_MAX_FIF, 0, 0, tests->simple_create_info });
	RegisterTest(tests, { "Cached Transient Persistence", test__cached_transient_persistence, 0, 6, 0, 0, tests->simple_create_info });
	//Also registered after the Acquire/Release Indexes test: that test expects an exact report
	//count, and inserting any test before it shifts the point where the (independently leaking)
	//bindless index pool runs out, which adds exhaustion reports to its context.
	RegisterTest(tests, { "Resource Test Buffer Offset Alignment", test__buffer_offset_alignment, 0, 1, 0, 0, tests->simple_create_info });
	RegisterTest(tests, { "Resource Test Image Allocator Memory Type Keying", test__image_allocator_memory_type_keying, 0, 1, 0, 0, tests->simple_create_info });
	RegisterTest(tests, { "Resource Test Large Readback Buffer", test__large_readback_buffer, 0, 1, 0, 0, tests->simple_create_info });
	RegisterTest(tests, { "Resource Test Dedicated Buffer", test__dedicated_buffer, 0, 1, 0, 0, tests->simple_create_info });
	RegisterTest(tests, { "Resource Test Buffer Grow Contract", test__buffer_grow_contract, 0, 1, 0, 0, tests->simple_create_info });
	RegisterTest(tests, { "Resource Test Pooled Image Allocations", test__pooled_image_allocations, 0, 1, 0, 0, tests->simple_create_info });
	RegisterTest(tests, { "Cached Transient Placement", test__cached_transient_placement, 0, ZEST_MAX_FIF * 4, 0, 0, tests->simple_create_info });
	RegisterTest(tests, { "Unbacked Transient Barrier", test__unbacked_transient_barrier, 0, ZEST_MAX_FIF * 4, 0, 0, tests->simple_create_info });
	//Arena sharing tests: cached graphs no longer pin their transient arenas, so the pool must stay
	//at the live working set while persistence and cross-graph isolation still hold.
	RegisterTest(tests, { "Arena Memory Bound", test__arena_memory_bound, 0, ARENA_BOUND_KEY_COUNT * 4, 0, 0, tests->simple_create_info });
	RegisterTest(tests, { "Arena Alternation", test__arena_alternation, 0, 12, 0, 0, tests->simple_create_info });
	RegisterTest(tests, { "Intraframe Two Graphs", test__intraframe_two_graphs, 0, ZEST_MAX_FIF * 2, 0, 0, tests->simple_create_info });
	//Layer tests also create transient buffers/images so they stay after the bindless-index
	//sensitive tests above for the same reason.
	RegisterTest(tests, { "Layer Test Instance Upload", test__instance_layer_upload, 0, 1, 0, 0, tests->simple_create_info });
	RegisterTest(tests, { "Layer Test Instruction Batching", test__instance_layer_batching, 0, 1, 0, 0, tests->simple_create_info });
	RegisterTest(tests, { "Layer Test Buffer Growth", test__instance_layer_grow, 0, 1, 0, 0, tests->simple_create_info });
	RegisterTest(tests, { "Layer Test Instance Draw", test__instance_layer_draw, 0, 1, 0, 0, tests->simple_create_info });
	RegisterTest(tests, { "Layer Test Frame In Flight", test__instance_layer_fif, 0, ZEST_MAX_FIF * 2, 0, 0, tests->simple_create_info });
	//Device reset tests run their own reset cycles internally, which rebuilds the bindless index
	//free lists among other things, so they stay last where they can't disturb any test that is
	//sensitive to accumulated device state.
	RegisterTest(tests, { "Device Reset Memory Stability", test__device_reset_memory_stability, 0, 1, 0, 0, tests->simple_create_info });
	RegisterTest(tests, { "Device Reset Deferred Frees", test__device_reset_deferred_frees, 0, 1, 0, 0, tests->simple_create_info });
	RegisterTest(tests, { "Device Reset Pool Sizing", test__device_reset_pool_sizing, 0, 1, 0, 0, tests->simple_create_info });
	RegisterTest(tests, { "Device Reset Bindless Indexes", test__device_reset_bindless_indexes, 0, 1, 0, 0, tests->simple_create_info });

	ZEST_PRINT("Total Tests: %u", tests->test_count);
	tests->sampler_info = zest_CreateSamplerInfo();
	tests->current_test = 0;
    zest_ResetValidationErrors(tests->device);
}

void InitialiseSpecificTests(ZestTests *tests) {
	RegisterTest(tests, { "Device Reset Memory Stability", test__device_reset_memory_stability, 0, 1, 0, 0, tests->simple_create_info });
	tests->sampler_info = zest_CreateSamplerInfo();
	tests->current_test = 0;
    zest_ResetValidationErrors(tests->device);
}

void RegisterTest(ZestTests *tests, Test test) {
	//Overflowing the fixed test array silently corrupts the fields after it in ZestTests
	ZEST_ASSERT(tests->test_count < TEST_COUNT);
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
	tests->test_layer = { 0 };
	tests->layer_pipeline = 0;
	tests->stress_resources.image_count = 0;
	tests->stress_resources.buffer_count = 0;
}

void PrintTestUpdate(Test *test, int phase, zest_bool passed) {
	if (passed) {
		ZEST_PRINT("\033[32m\tphase %i of test %s passed\033[0m", phase, test->name);
	} else {
		ZEST_PRINT("\033[31m\tphase %i of test %s failed\033[0m", phase, test->name);
	}
}

int RunTests(ZestTests *tests) {
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
				//zest_ResetDevice recreates the logical device and destroys the context with it, so
				//recreate the context afterwards with the next test's create info.
				zest_ResetDevice(tests->device);
				tests->context = zest_CreateContext(tests->device, &tests->window_data, &tests->tests[tests->current_test].create_info);
				zest_ResetValidationErrors(tests->device);
				ResetTests(tests);
			} else {
				zest_ResetReports(tests->context);
				break;
			}
		}
	}
	ZEST_PRINT("%sTests completed: %i / %i\033[0m", completed_tests < tests->test_count ? "\033[31m" : "\033[32m", completed_tests, tests->test_count);
	return completed_tests;
}

//Runs the full test suite on a fresh device. The render pass mode (dynamic rendering vs legacy
//VkRenderPass) is a device creation decision, so covering both paths means building the device
//twice; the same SDL window can back consecutive device lifetimes.
int RunSuite(zest_window_data_t *window_data, const char **instance_extensions, unsigned int extension_count, zest_bool force_legacy_render_pass, int *total_out) {
	zest_create_context_info_t create_info = zest_CreateContextInfo();

	ZestTests tests = {};
	tests.simple_create_info = create_info;
	tests.headless_create_info = create_info;
	tests.headless_create_info.flags |= zest_context_init_flag_headless;

	// Create the device that serves all vulkan based contexts
	zest_device_builder device_builder = zest_BeginVulkanDeviceBuilder(0);
	zest_AddDeviceBuilderExtensions(device_builder, instance_extensions, extension_count);
	zest_AddDeviceBuilderValidation(device_builder);	//TEMP: leak experiment
	zest_DeviceBuilderLogToConsole(device_builder);
	zest_DeviceBuilderLogToMemory(device_builder);
	if (force_legacy_render_pass) {
		zest_DeviceBuilderForceLegacyRenderPass(device_builder);
	}
	// The pipeline-state tests exercise tessellation/geometry/wireframe pipelines, so opt in to
	// those optional device features here (shipping examples do not request them).
	zest_RequestDeviceFeature(device_builder, zest_capability_tessellation);
	zest_RequestDeviceFeature(device_builder, zest_capability_geometry_shader);
	tests.device = zest_EndDeviceBuilder(device_builder);

	if (!force_legacy_render_pass && zest__using_legacy_render_pass(tests.device)) {
		ZEST_PRINT("\033[33mWARNING: dynamic rendering is not supported on this device, this run will use the legacy render pass path instead.\033[0m");
	}

	//Initialise Zest
	tests.window_data = *window_data;
	tests.context = zest_CreateContext(tests.device, window_data, &create_info);

	InitialiseTests(&tests);
	//InitialiseSpecificTests(&tests);

	int completed_tests = RunTests(&tests);
	*total_out = tests.test_count;

	zest_DestroyDevice(tests.device);

	return completed_tests;
}

int main(int argc, char *argv[]) {
	//Unbuffered output so that when a test crashes the process, the last test name printed is
	//actually the test that crashed rather than whatever was last flushed
	setvbuf(stdout, NULL, _IONBF, 0);

	zest_window_data_t window_data = zest_implsdl2_CreateWindow(50, 50, 1280, 768, 0, "Tests");

	unsigned int count = 0;
	SDL_Vulkan_GetInstanceExtensions((SDL_Window*)window_data.window_handle, &count, NULL);
	const char** sdl_extensions = (const char**)malloc(sizeof(const char*) * count);
	SDL_Vulkan_GetInstanceExtensions((SDL_Window*)window_data.window_handle, &count, sdl_extensions);

	int legacy_total = 0;
	int dynamic_total = 0;
	int dynamic_completed = 0;
	int legacy_completed = 0;

	ZEST_PRINT("=== Test suite run: dynamic rendering ===");
	dynamic_completed = RunSuite(&window_data, sdl_extensions, count, ZEST_FALSE, &dynamic_total);

	ZEST_PRINT("");
	ZEST_PRINT("=== Test suite run: legacy render pass ===");
	legacy_completed = RunSuite(&window_data, sdl_extensions, count, ZEST_TRUE, &legacy_total);

	// Clean up the extensions array
	free(sdl_extensions);

	ZEST_PRINT("");
	ZEST_PRINT("%sDynamic rendering:  %i / %i\033[0m", dynamic_completed < dynamic_total ? "\033[31m" : "\033[32m", dynamic_completed, dynamic_total);
	ZEST_PRINT("%sLegacy render pass: %i / %i\033[0m", legacy_completed < legacy_total ? "\033[31m" : "\033[32m", legacy_completed, legacy_total);

	return (dynamic_completed == dynamic_total && legacy_completed == legacy_total) ? 0 : 1;
}