#pragma once

// zest_utilities.h
// A collection of single-file header implementations for Zest.

// --- GLFW Utilities --------------------------------------------------------
// To use this implementation, include <GLFW/glfw3.h> before this file

#if defined(ZEST_VULKAN_IMPLEMENTATION) && defined(GLFW_VERSION_MAJOR)
#ifdef _WIN32
#ifndef GLFW_EXPOSE_NATIVE_WIN32
#define GLFW_EXPOSE_NATIVE_WIN32
#endif
#include <GLFW/glfw3native.h>   // for glfwGetWin32Window()
#endif
#endif

zest_window_data_t zest_implglfw_CreateWindow( int x, int y, int width, int height, zest_bool maximised, const char *title);
void zest_implglfw_GetWindowSizeCallback( void* native_window_handle, int* fb_width, int* fb_height, int* window_width, int* window_height );
void zest_implglfw_DestroyWindow(zest_context context);
void zest_implglfw_PollEventsCallback(zest_context context);

#ifdef GLFW_VERSION_MAJOR

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

#endif // GLFW_VERSION_MAJOR
// --- End GLFW Utilities ----------------------------------------------------

// --- SDL2 Utilities --------------------------------------------------------
// To use this implementation, include <SDL.h> before this file

#if defined(ZEST_VULKAN_IMPLEMENTATION) && defined(SDL_MAJOR_VERSION)
#include <SDL_vulkan.h>
#endif

zest_window_data_t zest_implsdl2_CreateWindow(int x, int y, int width, int height, zest_bool maximised, const char* title);
void zest_implsdl2_GetWindowSizeCallback(void *window_handle, int *fb_width, int *fb_height, int *window_width, int *window_height);
void zest_implsdl2_DestroyWindow(zest_context context);

#if defined(SDL_MAJOR_VERSION)
#include <SDL_syswm.h>

zest_window_data_t zest_implsdl2_CreateWindow(int x, int y, int width, int height, zest_bool maximised, const char *title) {
    Uint32 flags = 0;
#if defined(ZEST_VULKAN_IMPLEMENTATION)
    flags = SDL_WINDOW_VULKAN;
#elif defined(ZEST_IMPLEMENT_DX12)
    // flags = SDL_WINDOW_DX12; // Placeholder for DX12
#elif defined(ZEST_IMPLEMENT_METAL)
    // flags = SDL_WINDOW_METAL; // Placeholder for Metal
#endif

    if (maximised) {
        flags |= SDL_WINDOW_MAXIMIZED;
    }

    SDL_Window *window_handle = SDL_CreateWindow(title, x, y, width, height, flags);

    zest_window_data_t window_handles = { 0 };
    window_handles.width = width;
    window_handles.height = height;

    if (maximised) {
        SDL_GetWindowSize(window_handle, &window_handles.width, &window_handles.height);
    }

    window_handles.window_handle = window_handle;

    SDL_SysWMinfo wmi;
    SDL_VERSION(&wmi.version);
    if (!SDL_GetWindowWMInfo(window_handle, &wmi)) {
        // Handle error, for now returning partially filled struct
        return window_handles;
    }

#if defined(_WIN32)
    window_handles.native_handle = wmi.info.win.window;
    window_handles.display = wmi.info.win.hinstance;
#elif defined(__linux__)
#if defined(ZEST_VULKAN_IMPLEMENTATION)
    window_handles.native_handle = (void*)(uintptr_t)wmi.info.x11.window;
    window_handles.display = wmi.info.x11.display;
#endif
#endif

    window_handles.window_sizes_callback = zest_implsdl2_GetWindowSizeCallback;

    return window_handles;
}

void zest_implsdl2_GetWindowSizeCallback(void *window_handle, int *fb_width, int *fb_height, int *window_width, int *window_height) {
#if defined(ZEST_VULKAN_IMPLEMENTATION)
	SDL_Vulkan_GetDrawableSize((SDL_Window*)window_handle, fb_width, fb_height);
#elif defined(ZEST_IMPLEMENT_DX12)
    // DX12 equivalent
#elif defined(ZEST_IMPLEMENT_METAL)
    // Metal equivalent
#endif
	SDL_GetWindowSize((SDL_Window*)window_handle, window_width, window_height);
}

void zest_implsdl2_DestroyWindow(zest_context context) {
	SDL_Window *handle = (SDL_Window *)zest_Window(context);
	SDL_DestroyWindow(handle);
}

#endif // SDL_MAJOR_VERSION
// --- End SDL2 Utilities ----------------------------------------------------
