#pragma once
#include "zest.h"
#include "zest_vulkan.h"
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
void zest_implglfw_PollEventsCallback(zest_context context);
