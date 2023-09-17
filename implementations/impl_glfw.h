#pragma once
#include "zest.h"
#include <GLFW/glfw3.h>

zest_window_t *zest_implglfw_CreateWindowCallback(int x, int y, int width, int height, zest_bool maximised, const char* title);
void zest_implglfw_CreateWindowSurfaceCallback(zest_window_t* window);
void zest_implglfw_PollEventsCallback(void);
void zest_implglfw_AddPlatformExtensionsCallback(void);
void zest_implglfw_GetWindowSizeCallback(void *user_data, int *fb_width, int *fb_height, int *window_width, int *window_height);
void zest_implglfw_DestroyWindowCallback(void *user_data);
void zest_implglfw_SetCallbacks(zest_create_info_t *create_info);
