#pragma once
#include "zest.h"
#include "zest_vulkan.h"
#include <GLFW/glfw3.h>

zest_window zest_implglfw_CreateWindowCallback(zest_context context, int x, int y, int width, int height, zest_bool maximised, const char *title);
zest_bool zest_implglfw_CreateWindowSurfaceCallback(zest_context context);
void zest_implglfw_GetWindowSizeCallback(zest_context context, void *user_data, int *fb_width, int *fb_height, int *window_width, int *window_height);
void zest_implglfw_PollEventsCallback(zest_context context);
void zest_implglfw_AddPlatformExtensionsCallback(zest_context context);
void zest_implglfw_GetWindowSizeCallback(void *user_data, int *fb_width, int *fb_height, int *window_width, int *window_height);
void zest_implglfw_SetWindowMode(zest_context context, zest_window_mode mode);
void zest_implglfw_SetWindowSize(zest_context context, int width, int height);
void zest_implglfw_DestroyWindowCallback(zest_context context, void *user_data);
void zest_implglfw_SetCallbacks(zest_create_info_t *create_info);
