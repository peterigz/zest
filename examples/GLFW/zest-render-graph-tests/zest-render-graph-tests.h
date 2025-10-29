#pragma once

#include <GLFW/glfw3.h>
#include <zest.h>
#include "implementations/impl_imgui.h"
#include "imgui/imgui.h"
#include <imgui/misc/freetype/imgui_freetype.h>
#include <imgui/backends/imgui_impl_glfw.h>

#define TEST_COUNT 17

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
	zest_frame_graph_result result;
	zest_frame_graph_result expected_result;
	zest_create_info_t create_info;
	int cache_count;
};

struct ZestTests {
	zest_context context;
	Test tests[TEST_COUNT];
	int current_test;
	zest_sampler_info_t sampler_info;
	zest_sampler_info_t mipped_sampler_info;
	zest_image_handle texture;
	zest_sampler_handle sampler;
	zest_uint sampler_index;
	zest_sampler_handle mipped_sampler;
	TestPushConstants push;
	zest_compute_handle compute_write;
	zest_compute_handle compute_verify;
	zest_buffer cpu_buffer;
	zest_uint cpu_buffer_index;
	zest_create_info_t simple_create_info;
	zest_create_info_t depth_create_info;
};

void InitialiseTests(ZestTests *tests);

int test__blank_screen(ZestTests *tests, Test *test);
