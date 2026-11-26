#define ZEST_IMPLEMENTATION
#ifdef ZEST_ENABLE_VULKAN
#define ZEST_VULKAN_IMPLEMENTATION
#endif
#ifdef ZEST_ENABLE_D3D12
#define ZEST_D3D12_IMPLEMENTATION
#endif
#ifdef ZEST_ENABLE_METAL
#define ZEST_METAL_IMPLEMENTATION
#endif
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
#include "zest-slang-tests.cpp"
#ifdef ZEST_ENABLE_D3D12
#include "zest-d3d12-tests.cpp"
#endif

//What the Metal process ran: the shared suite, then the backend's own list
struct MetalRunCounts {
	int suite_passed;
	int suite_total;
	int backend_passed;
	int backend_total;
};

#ifdef ZEST_ENABLE_METAL
#include "zest-metal-tests.cpp"
#endif

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
	RegisterTest(tests, { "Pipeline Test Winding Coverage", test__pipeline_winding_coverage, 0, 1, 0, 0, tests->simple_create_info });
	RegisterTest(tests, { "Pipeline Test Vertex Draw", test__pipeline_vertex_draw, 0, 1, 0, 0, tests->simple_create_info });
	RegisterTest(tests, { "Pipeline Test Depth Bias", test__pipeline_depth_bias, 0, 1, 0, 0, tests->simple_create_info });
	RegisterTest(tests, { "Pipeline Test Indirect Draw", test__pipeline_indirect_draw, 0, 1, 0, 0, tests->simple_create_info });
	RegisterTest(tests, { "Pipeline Test ImGui Shaders", test__pipeline_imgui_shaders, 0, 1, 0, 0, tests->simple_create_info });
	RegisterTest(tests, { "Shader Cache Invalidation", test__shader_cache_invalidation, 0, 1, 0, 0, tests->simple_create_info });
	RegisterTest(tests, { "Shader Binary Platform Mismatch", test__shader_binary_platform_mismatch, 0, 1, 0, 0, tests->simple_create_info });
	RegisterTest(tests, { "Shader MSL Source Platform", test__shader_msl_source_platform, 0, 1, 0, 0, tests->simple_create_info });
	RegisterTest(tests, { "Shader Platform Macro", test__shader_platform_macro, 0, 1, 0, 0, tests->simple_create_info });
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
	RegisterTest(tests, { "Compute Test Immediate Execute Repeated", test__immediate_execute_repeated, 0, 1, 0, 0, tests->headless_create_info });
	RegisterTest(tests, { "Compute Test Mipmap Chain", test__compute_mipmap_chain, 0, 1, 0, 0, tests->headless_create_info });
	RegisterTest(tests, { "Compute Test Multiple Timeline Signals", test__multiple_timeline_signals, 0, 1, 0, 0, tests->headless_create_info });
	RegisterTest(tests, { "Compute Test Read Modify Write", test__compute_read_modify_write, 0, 1, 0, 0, tests->headless_create_info });
	RegisterTest(tests, { "Compute Test Only Graph", test__compute_only_graph, 0, 1, 0, 0, tests->headless_create_info });
	RegisterTest(tests, { "Compute Test Immediate Execute No Wait", test__immediate_execute_no_wait, 0, 1, 0, 0, tests->headless_create_info });
	RegisterTest(tests, { "Unwaited Graph Mip Views", test__unwaited_graph_mip_views, 0, 1, 0, 0, tests->headless_create_info });
	RegisterTest(tests, { "Compute Test WAW Barrier", test__compute_waw_barrier, 0, 1, 0, 0, tests->headless_create_info });
	RegisterTest(tests, { "Compute Test Flush Compile Failure No Hang", test__flush_compile_failure_no_hang, 0, 1, 0, 0, tests->headless_create_info });
	RegisterTest(tests, { "Compute Test Headless Flush And Wait", test__headless_flush_and_wait, 0, 1, 0, 0, tests->headless_create_info });
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
	RegisterTest(tests, { "Resource Test Concurrent Pool Growth", test__concurrent_pool_growth, 0, 1, 0, 0, tests->simple_create_info });
	RegisterTest(tests, { "Resource Test Stride Aligned Buffers", test__stride_aligned_buffers, 0, 1, 0, 0, tests->simple_create_info });
	RegisterTest(tests, { "Resource Test Structured Buffer Index", test__structured_buffer_index, 0, 1, 0, 0, tests->simple_create_info });
	RegisterTest(tests, { "Resource Test Pooled Image Allocations", test__pooled_image_allocations, 0, 1, 0, 0, tests->simple_create_info });
	RegisterTest(tests, { "Cached Transient Placement", test__cached_transient_placement, 0, ZEST_MAX_FIF * 4, 0, 0, tests->simple_create_info });
	RegisterTest(tests, { "Unbacked Transient Barrier", test__unbacked_transient_barrier, 0, ZEST_MAX_FIF * 4, 0, 0, tests->simple_create_info });
	RegisterTest(tests, { "Transient Placement Failure", test__transient_placement_failure, 0, 1, 0, 0, tests->simple_create_info });
	RegisterTest(tests, { "Transient Placement Failure Retry", test__transient_placement_failure_retry, 0, ZEST_MAX_FIF * 2, 0, 0, tests->simple_create_info });
	//Arena sharing tests: cached graphs no longer pin their transient arenas, so the pool must stay
	//at the live working set while persistence and cross-graph isolation still hold.
	RegisterTest(tests, { "Arena Memory Bound", test__arena_memory_bound, 0, ARENA_BOUND_KEY_COUNT * 4, 0, 0, tests->simple_create_info });
	RegisterTest(tests, { "Arena Alternation", test__arena_alternation, 0, 12, 0, 0, tests->simple_create_info });
	RegisterTest(tests, { "Arena Trim", test__arena_trim, 0, ARENA_TRIM_RUN_COUNT, 0, 0, tests->simple_create_info });
	RegisterTest(tests, { "Transient Report", test__transient_report, 0, REPORT_RUN_COUNT, 0, 0, tests->simple_create_info });
	RegisterTest(tests, { "Intraframe Two Graphs", test__intraframe_two_graphs, 0, ZEST_MAX_FIF * 2, 0, 0, tests->simple_create_info });
	zest_create_context_info_t small_frame_graph_memory_info = tests->simple_create_info;
	small_frame_graph_memory_info.frame_graph_allocator_size = zloc__KILOBYTE(1);
	RegisterTest(tests, { "Transient Mip Views Across Allocator Growth", test__transient_mip_views_allocator_growth, 0, ZEST_MAX_FIF * 2 + 1, 0, 0, small_frame_graph_memory_info });
	zest_create_context_info_t cached_growth_memory_info = tests->simple_create_info;
	cached_growth_memory_info.frame_graph_allocator_size = CACHED_GROWTH_ALLOCATOR_SIZE;
	RegisterTest(tests, { "Cached Graph Allocator Growth", test__cached_graph_allocator_growth, 0, ZEST_MAX_FIF * 2 + 1, 0, 0, cached_growth_memory_info });
	RegisterTest(tests, { "Command Graphs After End Frame", test__command_graphs_after_end_frame, 0, ZEST_MAX_FIF * 3, 0, 0, tests->simple_create_info });
	RegisterTest(tests, { "Frame Graph Transfer Image Output", test__transfer_image_output, 0, 1, 0, 0, tests->simple_create_info });
	RegisterTest(tests, { "Frame Graph Transfer Queue Mip Copy", test__transfer_queue_mip_copy, 0, 1, 0, 0, tests->simple_create_info });
	//Vulkan only: D3D12 blits the same format on the copy queue
	RegisterTest(tests, { "Frame Graph Transfer Queue Blit Report", test__transfer_queue_blit_report, 0, 1, 0, 0, tests->simple_create_info });
	RegisterTest(tests, { "Multi Queue Cached Handoff", test__multi_queue_cached_handoff, 0, ZEST_MAX_FIF * 2 + 1, 0, 0, tests->simple_create_info });
	RegisterTest(tests, { "Cached Imported Image", test__cached_imported_image, 0, ZEST_MAX_FIF * 2 + 1, 0, 0, tests->simple_create_info });
	RegisterTest(tests, { "Cached Imported Image Async", test__cached_imported_image_async, 0, ZEST_MAX_FIF * 2 + 1, 0, 0, tests->simple_create_info });
	RegisterTest(tests, { "Multi Queue Imported Handback", test__multi_queue_imported_handback, 0, ZEST_MAX_FIF * 2 + 1, 0, 0, tests->simple_create_info });
	RegisterTest(tests, { "Frame Graph Transient Structured Buffer", test__transient_structured_buffer, 0, 1, 0, 0, tests->simple_create_info });
	zest_create_context_info_t profiling_create_info = tests->simple_create_info;
	profiling_create_info.flags |= zest_context_init_flag_gpu_profiling | zest_context_init_flag_debug_overlay;
	RegisterTest(tests, { "GPU Profiler Timestamps", test__gpu_profiler_timestamps, 0, ZEST_MAX_FIF * 2 + 2, 0, 0, profiling_create_info });
	//Layer tests also create transient buffers/images so they stay after the bindless-index
	//sensitive tests above for the same reason.
	RegisterTest(tests, { "Layer Test Instance Upload", test__instance_layer_upload, 0, 1, 0, 0, tests->simple_create_info });
	RegisterTest(tests, { "Layer Test Instruction Batching", test__instance_layer_batching, 0, 1, 0, 0, tests->simple_create_info });
	RegisterTest(tests, { "Layer Test Buffer Growth", test__instance_layer_grow, 0, 1, 0, 0, tests->simple_create_info });
	RegisterTest(tests, { "Layer Test Instance Draw", test__instance_layer_draw, 0, 1, 0, 0, tests->simple_create_info });
	RegisterTest(tests, { "Layer Test Frame In Flight", test__instance_layer_fif, 0, ZEST_MAX_FIF * 2, 0, 0, tests->simple_create_info });
#ifdef ZEST_ENABLE_SLANG
	RegisterTest(tests, { "Slang Variants And Linking", test__slang_variants_and_linking, 0, 1, 0, 0, tests->headless_create_info });
	RegisterTest(tests, { "Slang Compile Failures", test__slang_compile_failures, 0, 1, 0, 0, tests->headless_create_info });
	RegisterTest(tests, { "Slang Type Size", test__slang_type_size, 0, 1, 0, 0, tests->headless_create_info });
	RegisterTest(tests, { "Slang Debug Info", test__slang_debug_info, 0, 1, 0, 0, tests->headless_create_info });
	RegisterTest(tests, { "Slang Hot Reload", test__slang_hot_reload, 0, 1, 0, 0, tests->headless_create_info });
	RegisterTest(tests, { "Slang Shadowing Module", test__slang_shadowing_module, 0, 1, 0, 0, tests->headless_create_info });
	RegisterTest(tests, { "Slang TimelineFX Strides", test__slang_timelinefx_strides, 0, 1, 0, 0, tests->headless_create_info });
#endif
	RegisterTest(tests, { "Shader Hot Reload Keeps Macros", test__shader_hot_reload_keeps_macros, 0, 1, 0, 0, tests->simple_create_info });
	//Device reset tests run their own reset cycles internally, which rebuilds the bindless index
	//free lists among other things, so they stay last where they can't disturb any test that is
	//sensitive to accumulated device state.
	RegisterTest(tests, { "Device Reset Memory Stability", test__device_reset_memory_stability, 0, 1, 0, 0, tests->simple_create_info });
	RegisterTest(tests, { "Device Reset Deferred Frees", test__device_reset_deferred_frees, 0, 1, 0, 0, tests->simple_create_info });
	RegisterTest(tests, { "Device Reset Pool Sizing", test__device_reset_pool_sizing, 0, 1, 0, 0, tests->simple_create_info });
	RegisterTest(tests, { "Device Reset Bindless Indexes", test__device_reset_bindless_indexes, 0, 1, 0, 0, tests->simple_create_info });
	RegisterTest(tests, { "User Test Device Platform Query", test__device_platform_query, 0, 1, 0, 0, tests->headless_create_info });
	RegisterTest(tests, { "User Test Descriptor Pool Failure", test__descriptor_pool_failure, 0, 1, 0, 0, tests->headless_create_info });
#ifdef ZEST_ENABLE_VULKAN
	RegisterTest(tests, { "User Test Device Creation Pool Failure", test__device_creation_pool_failure, 0, 1, 0, 0, tests->headless_create_info });
	RegisterTest(tests, { "User Test Sync Validation Flag", test__sync_validation_flag, 0, 1, 0, 0, tests->headless_create_info });
	RegisterTest(tests, { "User Test Device Builder Freed", test__device_builder_freed, 0, 1, 0, 0, tests->headless_create_info });
#endif
	RegisterTest(tests, { "User Test Swapchain Resize", test__swapchain_resize, 0, 1, 0, 0, tests->simple_create_info });

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

//The shared tests only the Vulkan suite runs
static const char *vulkan_only_tests[] = {
	//Builds Vulkan devices; "D3D12 Pool Failure Rejected" covers D3D12
	"User Test Device Creation Pool Failure",
	//Tests a Vulkan helper
	"User Test Sync Validation Flag",
	//Builds a Vulkan device
	"User Test Device Builder Freed",
	//Expects a blit report D3D12 never gives, as it copies a matching format on the copy queue
	"Frame Graph Transfer Queue Blit Report",
};

static zest_bool IsVulkanOnlyTest(const char *name) {
	for (size_t index = 0; index != sizeof(vulkan_only_tests) / sizeof(vulkan_only_tests[0]); ++index) {
		if (strcmp(vulkan_only_tests[index], name) == 0) return ZEST_TRUE;
	}
	return ZEST_FALSE;
}

//Set by --test, so that only the tests whose name contains the text are registered
static const char *suite_test_filter = NULL;

void RegisterTest(ZestTests *tests, Test test) {
	if (tests->platform != zest_platform_vulkan && IsVulkanOnlyTest(test.name)) return;
	if (suite_test_filter && !strstr(test.name, suite_test_filter)) return;
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
	//Backend allocation steady state. zest_ResetDevice tears down every context, pool and arena, so
	//the live vkAllocateMemory count immediately afterwards must return to the same baseline on
	//every cycle regardless of which test just ran. A climb pins the leak on that specific test.
	int alloc_baseline = -1;
	int leaking_tests = 0;

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
				const char *finished_test = current_test->name;
				tests->current_test++;
				//zest_ResetDevice recreates the logical device and destroys the context with it, so
				//recreate the context afterwards with the next test's create info.
				zest_ResetDevice(tests->device);
				int post_reset_allocations = tests->device->memory_allocation_count;
				if (alloc_baseline < 0) {
					alloc_baseline = post_reset_allocations;
				} else if (post_reset_allocations > alloc_baseline) {
					ZEST_PRINT("\033[31m\t%s leaked %i device memory allocation(s) - live count %i, expected %i\033[0m", finished_test, post_reset_allocations - alloc_baseline, post_reset_allocations, alloc_baseline);
					leaking_tests++;
					//Rebaseline so one leak does not flag every test that follows it
					alloc_baseline = post_reset_allocations;
				}
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
	if (leaking_tests > 0) {
		ZEST_PRINT("\033[31m%i test(s) leaked device memory allocations - see the per-test leak lines above\033[0m", leaking_tests);
	}
	return completed_tests;
}

static zest_device CreateSuiteDevice(zest_platform_type platform, const char **instance_extensions, unsigned int extension_count, zest_bool force_legacy_render_pass) {
#ifdef ZEST_ENABLE_D3D12
	if (platform == zest_platform_d3d12) {
		return zest_EndDeviceBuilder(BeginD3D12TestBuilder());
	}
#endif
#ifdef ZEST_ENABLE_METAL
	if (platform == zest_platform_metal) {
		return zest_EndDeviceBuilder(BeginMetalTestBuilder());
	}
#endif
#ifdef ZEST_ENABLE_VULKAN
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
	return zest_EndDeviceBuilder(device_builder);
#else
	(void)instance_extensions;
	(void)extension_count;
	(void)force_legacy_render_pass;
	return NULL;
#endif
}

//Runs the full test suite on a fresh device. The render pass mode (dynamic rendering vs legacy
//VkRenderPass) is a device creation decision, so covering both paths means building the device
//twice; the same SDL window can back consecutive device lifetimes.
int RunSuite(zest_platform_type platform, zest_window_data_t *window_data, const char **instance_extensions, unsigned int extension_count, zest_bool force_legacy_render_pass, int *total_out) {
	zest_create_context_info_t create_info = zest_CreateContextInfo();

	ZestTests tests = {};
	tests.platform = platform;
	tests.simple_create_info = create_info;
	tests.headless_create_info = create_info;
	tests.headless_create_info.flags |= zest_context_init_flag_headless;

	tests.device = CreateSuiteDevice(platform, instance_extensions, extension_count, force_legacy_render_pass);
	if (!tests.device) {
		ZEST_PRINT("\033[31mThe suite's device couldn't be created, so none of its tests ran.\033[0m");
		*total_out = 1;
		return 0;
	}

#ifdef ZEST_ENABLE_METAL
	//Without validation a Metal test that makes a mistake still passes, and validation can't come on once a window exists
	if (platform == zest_platform_metal && !ZEST_MTL_BACKEND(device, tests.device)->api_validation) {
		ZEST_PRINT("\033[31mThe Metal suite's device has no validation, so none of its tests ran. zest_PrepareMetalValidation has to be called before the suite's window is created.\033[0m");
		zest_DestroyDevice(tests.device);
		*total_out = 1;
		return 0;
	}
#endif

	if (!force_legacy_render_pass && zest__using_legacy_render_pass(tests.device)) {
		ZEST_PRINT("\033[33mWARNING: dynamic rendering is not supported on this device, this run will use the legacy render pass path instead.\033[0m");
	}

	tests.window_data = *window_data;
	InitialiseTests(&tests);
	//InitialiseSpecificTests(&tests);

	//RunTests runs the first test without looking at the count
	if (tests.test_count == 0) {
		ZEST_PRINT("No test in this suite has %s in its name, so the suite was skipped.", suite_test_filter ? suite_test_filter : "");
		zest_DestroyDevice(tests.device);
		*total_out = 0;
		return 0;
	}

	//The first test runs on this context, so it's made from that test's create info as RunTests makes every other one
	tests.context = zest_CreateContext(tests.device, window_data, &tests.tests[0].create_info);

	int completed_tests = RunTests(&tests);
	*total_out = tests.test_count;

	zest_DestroyDevice(tests.device);

	return completed_tests;
}

int main(int argc, char *argv[]) {
	//Unbuffered output so that when a test crashes the process, the last test name printed is
	//actually the test that crashed rather than whatever was last flushed
	setvbuf(stdout, NULL, _IONBF, 0);

	zest_bool run_vulkan = ZEST_FALSE;
#ifdef ZEST_ENABLE_VULKAN
	run_vulkan = ZEST_TRUE;
#endif
	zest_bool run_d3d12 = ZEST_FALSE;
#ifdef ZEST_ENABLE_D3D12
	run_d3d12 = ZEST_TRUE;
#endif
	zest_bool run_metal = ZEST_FALSE;
#ifdef ZEST_ENABLE_METAL
	run_metal = ZEST_TRUE;
#endif

	for (int argument_index = 1; argument_index < argc; ++argument_index) {
		const char *argument = argv[argument_index];
		if (strcmp(argument, "--platform") == 0 && argument_index + 1 < argc) {
			const char *platform_name = argv[++argument_index];
			if (strcmp(platform_name, "vulkan") == 0) {
#ifdef ZEST_ENABLE_VULKAN
				run_vulkan = ZEST_TRUE;
				run_d3d12 = ZEST_FALSE;
				run_metal = ZEST_FALSE;
#else
				ZEST_PRINT("\033[31mThe Vulkan backend isn't compiled in, install the Vulkan SDK and configure with -DZEST_ENABLE_VULKAN=ON\033[0m");
				return 1;
#endif
			} else if (strcmp(platform_name, "metal") == 0) {
#ifdef ZEST_ENABLE_METAL
				run_vulkan = ZEST_FALSE;
				run_d3d12 = ZEST_FALSE;
				run_metal = ZEST_TRUE;
#else
				ZEST_PRINT("\033[31mThe Metal backend isn't compiled in, configure with -DZEST_ENABLE_METAL=ON\033[0m");
				return 1;
#endif
			} else if (strcmp(platform_name, "d3d12") == 0) {
#ifdef ZEST_ENABLE_D3D12
				run_vulkan = ZEST_FALSE;
				run_d3d12 = ZEST_TRUE;
				run_metal = ZEST_FALSE;
#else
				ZEST_PRINT("\033[31mThe D3D12 backend isn't compiled in, configure with -DZEST_ENABLE_D3D12=ON\033[0m");
				return 1;
#endif
			} else if (strcmp(platform_name, "all") == 0) {
#ifdef ZEST_ENABLE_VULKAN
				run_vulkan = ZEST_TRUE;
#endif
#ifdef ZEST_ENABLE_D3D12
				run_d3d12 = ZEST_TRUE;
#endif
#ifdef ZEST_ENABLE_METAL
				run_metal = ZEST_TRUE;
#endif
			} else {
				ZEST_PRINT("\033[31mUnknown platform %s, expected vulkan, d3d12, metal or all\033[0m", platform_name);
				return 1;
			}
		} else if (strcmp(argument, "--test") == 0 && argument_index + 1 < argc) {
			suite_test_filter = argv[++argument_index];
		}
#ifdef ZEST_ENABLE_D3D12
		else if (strcmp(argument, "--warp") == 0) {
			d3d12_use_warp = ZEST_TRUE;
		} else if (strcmp(argument, "--gbv") == 0) {
			d3d12_use_gpu_validation = ZEST_TRUE;
		}
#endif
#ifdef ZEST_ENABLE_METAL
		else if (strcmp(argument, "--shader-validation") == 0) {
			metal_use_shader_validation = ZEST_TRUE;
		} else if (strcmp(argument, "--result-descriptor") == 0 && argument_index + 1 < argc) {
			metal_result_descriptor = atoi(argv[++argument_index]);
		}
#endif
	}

	MetalRunCounts metal_counts = {};
#ifdef ZEST_ENABLE_METAL
	if (run_metal && run_vulkan) {
		ZEST_PRINT("=== Test suite run: Metal, in its own process ===");
		metal_counts = RunMetalTestsInChildProcess(argv[0], suite_test_filter);
		ZEST_PRINT("");
	} else if (run_metal) {
		//Metal fixes its validation when the process makes its first window, so this comes before the suite's window
		zest_PrepareMetalValidation(MetalTestFlags());
		ZEST_PRINT("=== Test suite run: Metal suite ===");
		zest_window_data_t window_data = zest_implsdl2_CreateWindow(50, 50, 1280, 768, 0, "Metal Tests");
		metal_counts.suite_passed = RunSuite(zest_platform_metal, &window_data, NULL, 0, ZEST_FALSE, &metal_counts.suite_total);
		SDL_DestroyWindow((SDL_Window *)window_data.window_handle);

		ZEST_PRINT("");
		ZEST_PRINT("=== Test suite run: Metal backend ===");
		metal_counts.backend_passed = RunMetalSuite(&metal_counts.backend_total);
		ZEST_PRINT("");
		WriteMetalRunCounts(&metal_counts);
	}
#endif

	int legacy_total = 0;
	int dynamic_total = 0;
	int dynamic_completed = 0;
	int legacy_completed = 0;

#ifdef ZEST_ENABLE_VULKAN
	if (run_vulkan) {
		zest_window_data_t window_data = zest_implsdl2_CreateWindow(50, 50, 1280, 768, 0, "Tests");

		unsigned int count = 0;
		SDL_Vulkan_GetInstanceExtensions((SDL_Window*)window_data.window_handle, &count, NULL);
		const char** sdl_extensions = (const char**)malloc(sizeof(const char*) * count);
		SDL_Vulkan_GetInstanceExtensions((SDL_Window*)window_data.window_handle, &count, sdl_extensions);

		ZEST_PRINT("=== Test suite run: dynamic rendering ===");
		dynamic_completed = RunSuite(zest_platform_vulkan, &window_data, sdl_extensions, count, ZEST_FALSE, &dynamic_total);

		ZEST_PRINT("");
		ZEST_PRINT("=== Test suite run: legacy render pass ===");
		legacy_completed = RunSuite(zest_platform_vulkan, &window_data, sdl_extensions, count, ZEST_TRUE, &legacy_total);

		// Clean up the extensions array
		free(sdl_extensions);
	}
#endif

	int d3d12_suite_total = 0;
	int d3d12_suite_completed = 0;
	int d3d12_backend_total = 0;
	int d3d12_backend_completed = 0;
#ifdef ZEST_ENABLE_D3D12
	if (run_d3d12) {
		//The suite's device must be destroyed before the backend list starts, as an adapter holds one Zest D3D12 device at a time
		ZEST_PRINT("");
		ZEST_PRINT("=== Test suite run: D3D12 suite ===");
		zest_window_data_t window_data = zest_implsdl2_CreateWindow(50, 50, 1280, 768, 0, "D3D12 Tests");
		d3d12_suite_completed = RunSuite(zest_platform_d3d12, &window_data, NULL, 0, ZEST_FALSE, &d3d12_suite_total);
		SDL_DestroyWindow((SDL_Window *)window_data.window_handle);

		ZEST_PRINT("");
		ZEST_PRINT("=== Test suite run: D3D12 backend ===");
		d3d12_backend_completed = RunD3D12Suite(&d3d12_backend_total);
	}
#endif

	ZEST_PRINT("");
	if (run_vulkan) {
		ZEST_PRINT("%sDynamic rendering: %i / %i\033[0m", dynamic_completed < dynamic_total ? "\033[31m" : "\033[32m", dynamic_completed, dynamic_total);
		ZEST_PRINT("%sLegacy render pass: %i / %i\033[0m", legacy_completed < legacy_total ? "\033[31m" : "\033[32m", legacy_completed, legacy_total);
	}
	if (run_d3d12) {
		ZEST_PRINT("%sD3D12 suite: %i / %i\033[0m", d3d12_suite_completed < d3d12_suite_total ? "\033[31m" : "\033[32m", d3d12_suite_completed, d3d12_suite_total);
		ZEST_PRINT("%sD3D12 backend: %i / %i\033[0m", d3d12_backend_completed < d3d12_backend_total ? "\033[31m" : "\033[32m", d3d12_backend_completed, d3d12_backend_total);
	}
#ifdef ZEST_ENABLE_METAL
	//A child process leaves its line to the process that started it
	if (run_metal && metal_result_descriptor < 0) {
		ZEST_PRINT("%sMetal suite: %i / %i\033[0m", metal_counts.suite_passed < metal_counts.suite_total ? "\033[31m" : "\033[32m", metal_counts.suite_passed, metal_counts.suite_total);
		ZEST_PRINT("%sMetal backend: %i / %i\033[0m", metal_counts.backend_passed < metal_counts.backend_total ? "\033[31m" : "\033[32m", metal_counts.backend_passed, metal_counts.backend_total);
	}
#endif

	return (dynamic_completed == dynamic_total && legacy_completed == legacy_total && d3d12_suite_completed == d3d12_suite_total && d3d12_backend_completed == d3d12_backend_total
		&& metal_counts.suite_passed == metal_counts.suite_total && metal_counts.backend_passed == metal_counts.backend_total) ? 0 : 1;
}
