#pragma once

#include <GLFW/glfw3.h>
#include <zest.h>
#include "implementations/impl_imgui.h"
#include "imgui/imgui.h"
#include <imgui/misc/freetype/imgui_freetype.h>
#include <imgui/backends/imgui_impl_glfw.h>

#define TEST_COUNT 68
#define MAX_TEST_RESOURCES 1000

struct ZestTests;
struct Test;

typedef int (*test_callback)(ZestTests *tests, Test *test);

struct TestPushConstants {
	int index1;
	int index2;
	int index3;
	int index4;
	int index5;
};

struct TestData {
	zest_vec4 vec;
};

struct TestResults {
	zest_uint result[16];
};

struct Test {
	const char *name;
	test_callback the_test;
	int frame_count;
	int run_count;		//Number of frames the test should run for
	zest_frame_graph_result result;
	zest_frame_graph_result expected_result;
	zest_create_context_info_t create_info;
	int cache_count;
};

struct StressResources {
	zest_image_handle images[MAX_TEST_RESOURCES];
	zest_buffer buffers[MAX_TEST_RESOURCES];
	int image_count, buffer_count;
};

struct ZestTests {
	zest_device device;
	zest_context context;
	Test tests[TEST_COUNT];
	int current_test;
	zest_sampler_info_t sampler_info;
	zest_image_handle texture;
	zest_sampler_handle sampler;
	zest_uint sampler_index;
	zest_sampler_handle mipped_sampler;
	TestPushConstants push;
	zest_compute_handle compute_write;
	zest_compute_handle compute_verify;
	zest_compute_handle brd_compute;
	zest_shader_handle brd_shader;
	zest_image_handle brd_texture;
	zest_uint brd_bindless_texture_index;
	zest_buffer cpu_buffer;
	zest_uint cpu_buffer_index;
	zest_create_context_info_t simple_create_info;
	zest_create_context_info_t depth_create_info;
	StressResources stress_resources;
};

void InitialiseTests(ZestTests *tests);
void ResetTests(ZestTests *tests);
void RunTests(ZestTests *tests);
void PrintTestUpdate(Test *test, int phase, zest_bool passed);

int test__blank_screen(ZestTests *tests, Test *test);

/* Test ideas:
 Core Tests:
1. test__instanced_rendering_basic - Basic instanced rendering (100 instances)
2. test__instanced_rendering_large_count - Stress test (10K instances)
Done 3. test__vertex_format_validation - Multiple vertex format validation
Done 4. test__pipeline_state_depth - Depth testing configurations
Done 5. test__pipeline_state_blending - Blend state testing
Done 6. test__pipeline_state_culling - Rasterization culling
 Resource Management:
7. test__image_format_support - Multiple image format validation
8. test__buffer_type_validation - Different buffer type testing
9. test__memory_pool_exhaustion - Memory stress testing
10. test__resource_aliasing - Resource reuse optimization
11. test__resource_lifetime - Automatic cleanup validation
12. test__image_format_support - (included in core tests)
 Compute System:
13. test__parallel_sort_algorithm - GPU-based sorting
14. test__parallel_reduction - Sum/min/max operations
15. test__compute_rendering_sync - Compute-to-rendering workflows
16. test__complex_compute_workflow - Multi-pass compute pipelines
 Platform Integration:
17. test__multi_window_basic - Multiple window management
18. test__window_resize_handling - Window resize behavior
19. test__dpi_change_handling - High-DPI scenarios
20. test__display_mode_transition - Display mode changes
 Error Handling:
21. test__device_loss_recovery - Device loss simulation
22. test__out_of_memory_handling - OOM scenario testing
23. test__invalid_resource_handles - Invalid handle handling
24. test__shader_compilation_failure - Compile error handling
 Edge Case Tests:
25. test__empty_frame_graph - Minimal graph testing
26. test__zero_sized_resources - Edge case resource sizes
27. test__circular_dependency_complex - Complex dependency cycles
28. test__resource_leak_detection - Resource leak validation
 Performance Tests:
29. test__compilation_time_scaling - Graph compilation performance
30. test__resource_creation_overhead - Resource allocation benchmarking
31. test__memory_bandwidth - Memory throughput testing
32. test__command_buffer_performance - Command buffer performance
*/

