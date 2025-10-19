#pragma once

// zest_utilities.h
// A collection of single-file header implementations for Zest.

// --- GLFW Utilities --------------------------------------------------------
// To use this implementation, add the following in one of your C/C++ files:
// #define ZEST_IMPLEMENT_GLFW
// #include "zest_utilities.h"

#include "zest.h"
#include <GLFW/glfw3.h>
#ifdef _WIN32
#undef APIENTRY
#ifndef GLFW_EXPOSE_NATIVE_WIN32
#define GLFW_EXPOSE_NATIVE_WIN32
#endif
#include <GLFW/glfw3native.h>   // for glfwGetWin32Window()
#endif

zest_window_data_t zest_implglfw_CreateWindow( int x, int y, int width, int height, zest_bool maximised, const char *title);
void zest_implglfw_GetWindowSizeCallback( void* native_window_handle, int* fb_width, int* fb_height, int* window_width, int* window_height );
void zest_implglfw_DestroyWindow(zest_context context);
void zest_implglfw_PollEventsCallback(zest_context context);

#ifdef ZEST_IMPLEMENT_GLFW

zest_window_data_t zest_implglfw_CreateWindow( int x, int y, int width, int height, zest_bool maximised, const char *title) {
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	if (maximised) {
		glfwWindowHint(GLFW_MAXIMIZED, GLFW_TRUE);
	}

	GLFWwindow *window_handle = glfwCreateWindow(width, height, title, 0, 0);
	if (!maximised) {
		glfwSetWindowPos(window_handle, x, y);
	}

	zest_window_data_t window_handles = { 0 };
	window_handles.width = width;
	window_handles.height = height;

	if (maximised) {
		glfwGetWindowSize(window_handle, &window_handles.width, &window_handles.height);
	}

	window_handles.window_handle = window_handle;
	#if defined(_WIN32)
	window_handles.native_handle = (void*)glfwGetWin32Window(window_handle);
	window_handles.display = GetModuleHandle(NULL);
	#elif defined(__linux__)
	window_handles.native_handle = (void*)glfwGetX11Window(glfw_window);
	window_handles.display = glfwGetX11Display();
	#endif

	window_handles.window_sizes_callback = zest_implglfw_GetWindowSizeCallback;

	return window_handles;
}

void zest_implglfw_GetWindowSizeCallback(void *window_handle, int *fb_width, int *fb_height, int *window_width, int *window_height) {
	GLFWwindow *handle = (GLFWwindow *)window_handle;
    glfwGetFramebufferSize(handle, fb_width, fb_height);
	glfwGetWindowSize(handle, window_width, window_height);
}

void zest_implglfw_DestroyWindow(zest_context context) {
	GLFWwindow *handle = (GLFWwindow *)zest_Window(context);
	glfwDestroyWindow(handle);
}

#endif // ZEST_IMPLEMENT_GLFW
// --- End GLFW Utilities ----------------------------------------------------
