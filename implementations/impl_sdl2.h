#pragma once
#include "zest.h"
#include <SDL.h>
#include <SDL_vulkan.h>

zest_window_t *zest_implsdl2_CreateWindowCallback(int x, int y, int width, int height, zest_bool maximised, const char* title);
void zest_implsdl2_CreateWindowSurfaceCallback(zest_window_t* window);
void zest_implsdl2_PollEventsCallback(void);
void zest_implsdl2_AddPlatformExtensionsCallback(void);
void zest_implsdl2_GetWindowSizeCallback(void *user_data, int *fb_width, int *fb_height, int *window_width, int *window_height);
void zest_implsdl2_SetWindowMode(zest_window_t *window, zest_window_mode mode);
void zest_implsdl2_DestroyWindowCallback(void *user_data);
void zest_implsdl2_SetCallbacks(zest_create_info_t *create_info);
