#include "impl_glfw.h"

zest_window_t *zest_implglfw_CreateWindowCallback(zest_context context, int x, int y, int width, int height, zest_bool maximised, const char* title) {
	ZEST_ASSERT(context);        //not a valid context handle

	zest_window_t *window = zest_AllocateWindow(context);
	window->context = context;
	context->window = window;

	glfwInit();

	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	if (maximised)
		glfwWindowHint(GLFW_MAXIMIZED, GLFW_TRUE);

	zest_UpdateWindowSize(context, width, height);
	GLFWwindow *window_handle = glfwCreateWindow(width, height, title, 0, 0);
	zest_SetWindowHandle(context, window_handle);
	if (!maximised) {
		glfwSetWindowPos(window_handle, x, y);
	}
	glfwSetWindowUserPointer(window_handle, ZestApp);

	if (maximised) {
		int width, height;
		glfwGetWindowSize(window_handle, &width, &height);
		zest_UpdateWindowSize(context, width, height);
	}

	return window;
}

void zest_implglfw_SetWindowSize(zest_context context, int width, int height) {
	ZEST_ASSERT_HANDLE(context);	//Not a valid context handle!
	GLFWwindow *handle = (GLFWwindow *)zest_Window(context);

	glfwSetWindowSize(handle, width, height);
}

void zest_implglfw_SetWindowMode(zest_context context, zest_window_mode mode) {
	ZEST_ASSERT_HANDLE(context);	//Not a valid context handle!
	GLFWwindow *handle = (GLFWwindow *)zest_Window(context);
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
	zest_SetWindowMode(context, mode);
}

void zest_implglfw_PollEventsCallback(zest_context context) {
	glfwPollEvents();
	double mouse_x, mouse_y;
	GLFWwindow *handle = (GLFWwindow *)zest_Window(context);
	glfwGetCursorPos(handle, &mouse_x, &mouse_y);
	double last_mouse_x = context->window->mouse_x;
	double last_mouse_y = context->window->mouse_y;
	context->window->mouse_x = mouse_x;
	context->window->mouse_y = mouse_y;
	context->window->mouse_delta_x = last_mouse_x - context->window->mouse_x;
	context->window->mouse_delta_y = last_mouse_y - context->window->mouse_y;
	zest_MaybeQuit(glfwWindowShouldClose(handle));
}

void zest_implglfw_AddPlatformExtensionsCallback(zest_context context) {
	zest_uint count;
	const char **glfw_extensions = glfwGetRequiredInstanceExtensions(&count);
	for (int i = 0; i != count; ++i) {
		zest_AddInstanceExtension(context->device, (char*)glfw_extensions[i]);
	}
}

void zest_implglfw_GetWindowSizeCallback(zest_context context, void *user_data, int *fb_width, int *fb_height, int *window_width, int *window_height) {
	GLFWwindow *handle = (GLFWwindow *)zest_Window(context);
    glfwGetFramebufferSize(handle, fb_width, fb_height);
	glfwGetWindowSize(handle, window_width, window_height);
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

zest_bool zest_implglfw_CreateWindowSurfaceCallback(zest_context context) {
    ZEST_SET_MEMORY_CONTEXT(context->device, zest_platform_renderer, zest_command_surface);
	GLFWwindow *handle = (GLFWwindow *)zest_Window(context);
	VkSurfaceKHR surface;
	VkResult result = glfwCreateWindowSurface(zest_GetVKInstance(context), handle, zest_GetVKAllocationCallbacks(context), &surface);
	zest_vk_SetWindowSurface(context, surface);
	return result == VK_SUCCESS;
}

void zest_implglfw_DestroyWindowCallback(zest_context context, void *user_data) {
	GLFWwindow *handle = (GLFWwindow *)zest_Window(context);
	zest_CleanupWindow(context);
	glfwDestroyWindow(handle);
}

