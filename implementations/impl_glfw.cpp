#include "impl_glfw.h"

zest_window *zest_implglfw_CreateWindowCallback(int x, int y, int width, int height, zest_bool maximised, const char* title) {
	ZEST_ASSERT(ZestDevice);        //Must initialise the ZestDevice first

	zest_window *window = zest_AllocateWindow();

	glfwInit();

	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	if (maximised)
		glfwWindowHint(GLFW_MAXIMIZED, GLFW_TRUE);

	window->window_width = width;
	window->window_height = height;

	window->window_handle = glfwCreateWindow(width, height, title, 0, 0);
	if (!maximised) {
		glfwSetWindowPos((GLFWwindow*)window->window_handle, x, y);
	}
	glfwSetWindowUserPointer((GLFWwindow*)window->window_handle, ZestApp);

	if (maximised) {
		int width, height;
		glfwGetFramebufferSize((GLFWwindow*)window->window_handle, &width, &height);
		window->window_width = width;
		window->window_height = height;
	}

	return window;
}

void zest_implglfw_CreateWindowSurfaceCallback(zest_window* window) {
	ZEST_VK_CHECK_RESULT(glfwCreateWindowSurface(ZestDevice->instance, (GLFWwindow*)window->window_handle, &ZestDevice->allocation_callbacks, &window->surface));
}

void zest_implglfw_PollEventsCallback(void) {
	glfwPollEvents();
	zest_MaybeQuit(glfwWindowShouldClose((GLFWwindow*)ZestApp->window->window_handle));
}

void zest_implglfw_AddPlatformExtensionsCallback(void) {
	zest_uint count;
	char **extensions = 0;
	const char **glfw_extensions = glfwGetRequiredInstanceExtensions(&count);
	for (int i = 0; i != count; ++i) {
		zest_AddInstanceExtension((char*)glfw_extensions[i]);
	}
}

void zest_implglfw_GetWindowSizeCallback(void *user_data, int *width, int *height) {
	glfwGetFramebufferSize((GLFWwindow*)ZestApp->window->window_handle, width, height);
}

void zest_implglfw_DestroyWindowCallback(void *user_data) {
	glfwDestroyWindow((GLFWwindow*)ZestApp->window->window_handle);
}

void zest_implglfw_SetCallbacks(zest_create_info *create_info) {
	create_info->add_platform_extensions_callback = zest_implglfw_AddPlatformExtensionsCallback;
	create_info->create_window_callback = zest_implglfw_CreateWindowCallback;
	create_info->poll_events_callback = zest_implglfw_PollEventsCallback;
	create_info->get_window_size_callback = zest_implglfw_GetWindowSizeCallback;
	create_info->destroy_window_callback = zest_implglfw_DestroyWindowCallback;
	create_info->create_window_surface_callback = zest_implglfw_CreateWindowSurfaceCallback;
}