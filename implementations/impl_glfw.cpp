#include "impl_glfw.h"

zest_window_t *zest_implglfw_CreateWindowCallback(int x, int y, int width, int height, zest_bool maximised, const char* title) {

	zest_window_t *window = zest_AllocateWindow();

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
		glfwGetWindowSize((GLFWwindow*)window->window_handle, &width, &height);
		window->window_width = width;
		window->window_height = height;
	}

	return window;
}

VkResult zest_implglfw_CreateWindowSurfaceCallback(zest_window window) {
    zest_vk_SetMemoryContext(zest_vk_renderer, zest_vk_surface);
	return glfwCreateWindowSurface(zest_vk_Instance(), (GLFWwindow *)window->window_handle, zest_vk_AllocationCallbacks(), &window->surface);
}

void zest_implglfw_SetWindowSize(zest_window window, int width, int height) {
	ZEST_ASSERT_HANDLE(window);	//Not a valid window handle!
	GLFWwindow *handle = (GLFWwindow *)window->window_handle;

	glfwSetWindowSize(handle, width, height);
}

void zest_implglfw_SetWindowMode(zest_window window, zest_window_mode mode) {
	ZEST_ASSERT_HANDLE(window);	//Not a valid window handle!
	GLFWwindow *handle = (GLFWwindow *)window->window_handle;
	static int last_x, last_y, last_width, last_height;

	switch (mode) {
	case zest_window_mode_fullscreen:
	{
		if (!glfwGetWindowMonitor(handle)) {
			glfwGetWindowPos(handle, &last_x, &last_y);
			glfwGetWindowSize(handle, &last_width, &last_height);
		}

		GLFWmonitor *monitor = glfwGetPrimaryMonitor();
		const GLFWvidmode *vidmode = glfwGetVideoMode(monitor);
		glfwSetWindowMonitor(handle, monitor, 0, 0, vidmode->width, vidmode->height, vidmode->refreshRate);
		break;
	}
	case zest_window_mode_bordered:
		if (glfwGetWindowMonitor(handle)) {
			glfwSetWindowMonitor(handle, NULL, last_x, last_y, last_width, last_height, 0);
		}
		glfwSetWindowAttrib(handle, GLFW_DECORATED, GLFW_TRUE);
	break;
	case zest_window_mode_borderless:
		if (glfwGetWindowMonitor(handle)) {
			glfwSetWindowMonitor(handle, NULL, last_x, last_y, last_width, last_height, 0);
		}
		glfwSetWindowAttrib(handle, GLFW_DECORATED, GLFW_FALSE);
	break;
	}
	window->mode = mode;
}

void zest_implglfw_PollEventsCallback(void) {
	glfwPollEvents();
	double mouse_x, mouse_y;
	GLFWwindow *window_handle = (GLFWwindow*)zest_GetCurrentWindow()->window_handle;
	glfwGetCursorPos(window_handle, &mouse_x, &mouse_y);
	double last_mouse_x = zest_MouseX();
	double last_mouse_y = zest_MouseY();
	zest_MouseXf();
	zest_SetMouseLocation(mouse_x, mouse_y);
	zest_SetMouseDelta(last_mouse_x - mouse_x, last_mouse_y - mouse_y);
	zest_MaybeQuit(glfwWindowShouldClose(window_handle));
}

void zest_implglfw_AddPlatformExtensionsCallback(void) {
	zest_uint count;
	const char **glfw_extensions = glfwGetRequiredInstanceExtensions(&count);
	for (int i = 0; i != count; ++i) {
		zest_AddInstanceExtension((char*)glfw_extensions[i]);
	}
}

void zest_implglfw_GetWindowSizeCallback(void *user_data, int *fb_width, int *fb_height, int *window_width, int *window_height) {
    glfwGetFramebufferSize((GLFWwindow*)zest_GetCurrentWindow()->window_handle, fb_width, fb_height);
	glfwGetWindowSize((GLFWwindow*)zest_GetCurrentWindow()->window_handle, window_width, window_height);
}

void zest_implglfw_DestroyWindowCallback(zest_window window, void *user_data) {
	glfwDestroyWindow((GLFWwindow*)window->window_handle);
    vkDestroySurfaceKHR(zest_vk_Instance(), window->surface, zest_vk_AllocationCallbacks());
}

void zest_implglfw_SetCallbacks(zest_create_info_t *create_info) {
	create_info->add_platform_extensions_callback = zest_implglfw_AddPlatformExtensionsCallback;
	create_info->create_window_callback = zest_implglfw_CreateWindowCallback;
	create_info->poll_events_callback = zest_implglfw_PollEventsCallback;
	create_info->get_window_size_callback = zest_implglfw_GetWindowSizeCallback;
	create_info->destroy_window_callback = zest_implglfw_DestroyWindowCallback;
	create_info->create_window_surface_callback = zest_implglfw_CreateWindowSurfaceCallback;
	create_info->set_window_mode_callback = zest_implglfw_SetWindowMode;
	create_info->set_window_size_callback = zest_implglfw_SetWindowSize;
}
