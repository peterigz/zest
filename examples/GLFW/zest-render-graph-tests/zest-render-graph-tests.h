#pragma once

#include <zest.h>
#include "implementations/impl_imgui.h"
#include "implementations/impl_glfw.h"
#include "implementations/impl_imgui_glfw.h"
#include "imgui/imgui.h"
#include <imgui/misc/freetype/imgui_freetype.h>
#include <imgui/backends/imgui_impl_glfw.h>

#define TEST_COUNT 1

struct ZestTests;
struct Test;

typedef int (*test_callback)(ZestTests *tests, Test *test);

struct Test {
	const char *name;
	test_callback the_test;
	int frame_count;
	zest_render_graph_result result;
};

struct ZestTests {
	Test tests[TEST_COUNT];
	int current_test;
};

void InitialiseTests(ZestTests *tests);

int test__blank_screen(ZestTests *tests, Test *test);
