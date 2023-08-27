#pragma once
#include "zest.h"
#include <GLFW/glfw3.h>

zest_window *zest_implglfw_CreateWindowCallback(int x, int y, int width, int height, zest_bool maximised, const char* title);
void zest_implglfw_CreateWindowSurfaceCallback(zest_window* window);
void zest_implglfw_PollEventsCallback(void);
void zest_implglfw_AddPlatformExtensionsCallback(void);
void zest_implglfw_GetWindowSizeCallback(void *user_data, int *width, int *height);
void zest_implglfw_DestroyWindowCallback(void *user_data);
void zest_implglfw_SetCallbacks(zest_create_info *create_info);
