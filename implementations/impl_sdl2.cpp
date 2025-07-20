#include "impl_sdl2.h"

zest_window_t *zest_implsdl2_CreateWindowCallback(int x, int y, int width, int height, zest_bool maximised, const char* title) {
	ZEST_ASSERT(ZestDevice);        //Must initialise the ZestDevice first

	zest_window_t *window = zest_AllocateWindow();

	SDL_Init(SDL_INIT_EVERYTHING);

	SDL_GL_SetAttribute(SDL_GL_CONTEXT_NO_ERROR, 1); // Set to 1 for equivalent of GLFW_NO_API

	if (maximised) {
		SDL_SetWindowFullscreen((SDL_Window*)window->window_handle, SDL_WINDOW_FULLSCREEN_DESKTOP);
	}

	window->window_width = width;
	window->window_height = height;

	window->window_handle = SDL_CreateWindow(title, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, width, height, SDL_WINDOW_VULKAN | SDL_WINDOW_SHOWN);
	if (!maximised) {
		SDL_SetWindowPosition((SDL_Window*)window->window_handle, x, y);
	}
	SDL_SetWindowData((SDL_Window*)window->window_handle, "ZestApp", ZestApp);

	if (maximised) {
		int width, height;
		SDL_GL_GetDrawableSize((SDL_Window*)window->window_handle, &width, &height);
		window->window_width = width;
		window->window_height = height;
	}

	return window;
}

void zest_implsdl2_CreateWindowSurfaceCallback(zest_window_t* window) {
	if (!SDL_Vulkan_CreateSurface((SDL_Window*)window->window_handle, ZestDevice->instance, &window->surface)) {
		assert(0);	//Unable to create a vulkan surface in SDL
	}
}

void zest_implsdl2_PollEventsCallback(void) {
	int mouse_x, mouse_y;
	SDL_GetMouseState(&mouse_x, &mouse_y);
	double last_mouse_x = ZestApp->mouse_x;
	double last_mouse_y = ZestApp->mouse_y;
	ZestApp->mouse_x = mouse_x;
	ZestApp->mouse_y = mouse_y;
	ZestApp->mouse_delta_x = last_mouse_x - ZestApp->mouse_x;
	ZestApp->mouse_delta_y = last_mouse_y - ZestApp->mouse_y;
}


void zest_implsdl2_AddPlatformExtensionsCallback(void) {
	zest_uint count;
	SDL_Vulkan_GetInstanceExtensions((SDL_Window*)ZestApp->current_window->window_handle, &count, nullptr);
	const char **sdl_extensions = 0;
	if (!sdl_extensions || zest__vec_header(sdl_extensions)->capacity < count) sdl_extensions = (const char**)zest__vec_reserve(sdl_extensions, sizeof(*sdl_extensions), count == 1 ? 8 : count); zest__vec_header(sdl_extensions)->current_size = count;
	SDL_Vulkan_GetInstanceExtensions((SDL_Window*)ZestApp->current_window->window_handle, &count, sdl_extensions);
	for (int i = 0; i != count; ++i) {
		zest_AddInstanceExtension((char*)sdl_extensions[i]);
	}
}

void zest_implsdl2_GetWindowSizeCallback(void *user_data, int *fb_width, int *fb_height, int *window_width, int *window_height) {
	SDL_GL_GetDrawableSize((SDL_Window*)ZestApp->current_window->window_handle, fb_width, fb_height);
	SDL_GetWindowSize((SDL_Window*)ZestApp->current_window->window_handle, window_width, window_height);
}

void zest_implsdl2_DestroyWindowCallback(void *user_data) {
	SDL_DestroyWindow((SDL_Window*)ZestApp->current_window->window_handle);
}

void zest_implsdl2_SetCallbacks(zest_create_info_t *create_info) {
	create_info->add_platform_extensions_callback = zest_implsdl2_AddPlatformExtensionsCallback;
	create_info->create_window_callback = zest_implsdl2_CreateWindowCallback;
	create_info->poll_events_callback = zest_implsdl2_PollEventsCallback;
	create_info->get_window_size_callback = zest_implsdl2_GetWindowSizeCallback;
	create_info->destroy_window_callback = zest_implsdl2_DestroyWindowCallback;
	create_info->create_window_surface_callback = zest_implsdl2_CreateWindowSurfaceCallback;
}
