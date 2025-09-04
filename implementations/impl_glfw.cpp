#include "impl_glfw.h"

zest_window_t *zest_implglfw_CreateWindowCallback(int x, int y, int width, int height, zest_bool maximised, const char* title) {
	ZEST_ASSERT(ZestDevice);        //Must initialise the ZestDevice first

	zest_window_t *window = zest_AllocateWindow();
	ZestRenderer->main_window = window;

	glfwInit();

	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	if (maximised)
		glfwWindowHint(GLFW_MAXIMIZED, GLFW_TRUE);

	zest_UpdateWindowSize(window, width, height);
	GLFWwindow *window_handle = glfwCreateWindow(width, height, title, 0, 0);
	zest_SetWindowHandle(window, window_handle);
	if (!maximised) {
		glfwSetWindowPos(window_handle, x, y);
	}
	glfwSetWindowUserPointer(window_handle, ZestApp);

	if (maximised) {
		int width, height;
		glfwGetWindowSize(window_handle, &width, &height);
		zest_UpdateWindowSize(window, width, height);
	}

	return window;
}

void zest_implglfw_SetWindowSize(zest_window window, int width, int height) {
	ZEST_ASSERT_HANDLE(window);	//Not a valid window handle!
	GLFWwindow *handle = (GLFWwindow *)zest_Window();

	glfwSetWindowSize(handle, width, height);
}

void zest_implglfw_SetWindowMode(zest_window window, zest_window_mode mode) {
	ZEST_ASSERT_HANDLE(window);	//Not a valid window handle!
	GLFWwindow *handle = (GLFWwindow *)zest_Window();
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
	zest_SetWindowMode(zest_GetCurrentWindow(), mode);
}

void zest_implglfw_PollEventsCallback(void) {
	glfwPollEvents();
	double mouse_x, mouse_y;
	GLFWwindow *handle = (GLFWwindow *)zest_Window();
	glfwGetCursorPos(handle, &mouse_x, &mouse_y);
	double last_mouse_x = ZestApp->mouse_x;
	double last_mouse_y = ZestApp->mouse_y;
	ZestApp->mouse_x = mouse_x;
	ZestApp->mouse_y = mouse_y;
	ZestApp->mouse_delta_x = last_mouse_x - ZestApp->mouse_x;
	ZestApp->mouse_delta_y = last_mouse_y - ZestApp->mouse_y;
	zest_MaybeQuit(glfwWindowShouldClose(handle));
}

void zest_implglfw_AddPlatformExtensionsCallback(void) {
	zest_uint count;
	const char **glfw_extensions = glfwGetRequiredInstanceExtensions(&count);
	for (int i = 0; i != count; ++i) {
		zest_AddInstanceExtension((char*)glfw_extensions[i]);
	}
}

void zest_implglfw_GetWindowSizeCallback(void *user_data, int *fb_width, int *fb_height, int *window_width, int *window_height) {
	GLFWwindow *handle = (GLFWwindow *)zest_Window();
    glfwGetFramebufferSize(handle, fb_width, fb_height);
	glfwGetWindowSize(handle, window_width, window_height);
}

#if defined ZEST_VULKAN
zest_bool zest_implglfw_CreateWindowSurfaceCallback(zest_window window) {
    ZEST_SET_MEMORY_CONTEXT(zest_vk_renderer, zest_vk_surface);
	GLFWwindow *handle = (GLFWwindow *)zest_Window();
	VkSurfaceKHR surface;
	VkResult result = glfwCreateWindowSurface(zest_GetVKInstance(), handle, zest_GetVKAllocationCallbacks(), &surface);
	zest_SetWindowSurface(surface);
	return result == VK_SUCCESS;
}

void zest_implglfw_DestroyWindowCallback(zest_window window, void *user_data) {
	GLFWwindow *handle = (GLFWwindow *)zest_Window();
	VkSurfaceKHR surface = zest_WindowSurface();
	glfwDestroyWindow(handle);
    vkDestroySurfaceKHR(zest_GetVKInstance(), surface, zest_GetVKAllocationCallbacks());
}
#endif

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
