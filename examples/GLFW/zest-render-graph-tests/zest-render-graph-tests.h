#pragma once

#include <zest.h>
#include "implementations/impl_imgui.h"
#include "implementations/impl_glfw.h"
#include "implementations/impl_imgui_glfw.h"
#include "imgui/imgui.h"
#include <imgui/misc/freetype/imgui_freetype.h>
#include <imgui/backends/imgui_impl_glfw.h>

#define TEST_COUNT 16

struct ZestTests;
struct Test;

typedef int (*test_callback)(ZestTests *tests, Test *test);

struct TestPushConstants {
	int index1;
	int index2;
	int index3;
	int index4;
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
	zest_render_graph_result result;
	zest_render_graph_result expected_result;
	zest_create_info_t create_info;
};

struct ZestTests {
	Test tests[TEST_COUNT];
	int current_test;
	VkSamplerCreateInfo sampler_info;
	VkSamplerCreateInfo mipped_sampler_info;
	zest_texture texture;
	TestPushConstants push;
	zest_compute compute_write;
	zest_compute compute_verify;
	zest_buffer cpu_buffer;
	zest_uint cpu_buffer_index;
	zest_create_info_t simple_create_info;
	zest_create_info_t depth_create_info;
};

void InitialiseTests(ZestTests *tests);

int test__blank_screen(ZestTests *tests, Test *test);
