#include "zest.h"
#define TLOC_IMPLEMENTATION
#include "2loc.h"

const char *zest__vulkan_error(VkResult errorCode)
{
	switch (errorCode)
	{
#define ZEST_ERROR_STR(r) case VK_ ##r: return #r
		ZEST_ERROR_STR(NOT_READY);
		ZEST_ERROR_STR(TIMEOUT);
		ZEST_ERROR_STR(EVENT_SET);
		ZEST_ERROR_STR(EVENT_RESET);
		ZEST_ERROR_STR(INCOMPLETE);
		ZEST_ERROR_STR(ERROR_OUT_OF_HOST_MEMORY);
		ZEST_ERROR_STR(ERROR_OUT_OF_DEVICE_MEMORY);
		ZEST_ERROR_STR(ERROR_INITIALIZATION_FAILED);
		ZEST_ERROR_STR(ERROR_DEVICE_LOST);
		ZEST_ERROR_STR(ERROR_MEMORY_MAP_FAILED);
		ZEST_ERROR_STR(ERROR_LAYER_NOT_PRESENT);
		ZEST_ERROR_STR(ERROR_EXTENSION_NOT_PRESENT);
		ZEST_ERROR_STR(ERROR_FEATURE_NOT_PRESENT);
		ZEST_ERROR_STR(ERROR_INCOMPATIBLE_DRIVER);
		ZEST_ERROR_STR(ERROR_TOO_MANY_OBJECTS);
		ZEST_ERROR_STR(ERROR_FORMAT_NOT_SUPPORTED);
		ZEST_ERROR_STR(ERROR_SURFACE_LOST_KHR);
		ZEST_ERROR_STR(ERROR_NATIVE_WINDOW_IN_USE_KHR);
		ZEST_ERROR_STR(SUBOPTIMAL_KHR);
		ZEST_ERROR_STR(ERROR_OUT_OF_DATE_KHR);
		ZEST_ERROR_STR(ERROR_INCOMPATIBLE_DISPLAY_KHR);
		ZEST_ERROR_STR(ERROR_VALIDATION_FAILED_EXT);
		ZEST_ERROR_STR(ERROR_INVALID_SHADER_NV);
#undef STR
	default:
		return "UNKNOWN_ERROR";
	}
}

void zest_Initialise() {
	void *memory_pool = malloc(tloc__MEGABYTE(128));

	zest_assert(memory_pool);	//unable to allocate initial memory pool

	tloc_allocator *allocator = tloc_InitialiseAllocatorWithPool(memory_pool, tloc__MEGABYTE(128));
	ZestDevice = tloc_Allocate(allocator, sizeof(zest_device));
	ZestApp = tloc_Allocate(allocator, sizeof(zest_app));
	ZestDevice->memory_pools[0] = memory_pool;
	ZestDevice->memory_pool_count = 0;
	ZestDevice->allocator = allocator;
	zest__initialise_app();
	zest__initialise_device();
}

void zest_Start() {
	zest__main_loop();
	zest__destroy();
}

void zest__initialise_device() {
	zest__create_instance();
	zest__setup_validation();

	//zest__pick_physical_device();

	//zest__create_logical_device();

	//zest__set_limit_data();
}

void zest__create_instance() {
	if (ZEST_ENABLE_VALIDATION_LAYER) {
		zest_assert(zest__check_validation_layer_support());
	}

	zest__vk_create_info(app_info, VkApplicationInfo);
	app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	app_info.pApplicationName = "Zest";
	app_info.applicationVersion = VK_API_VERSION_1_0;
	app_info.pEngineName = "No Engine";
	app_info.engineVersion = VK_API_VERSION_1_0;
	app_info.apiVersion = VK_API_VERSION_1_0;
	ZestDevice->api_version = app_info.apiVersion;

	zest__vk_create_info(createInfo, VkInstanceCreateInfo);
	createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	createInfo.pApplicationInfo = &app_info;
	createInfo.flags = 0;
	createInfo.pNext = 0;

	zest_uint extension_count = 0;
	const char** extensions = zest__get_required_extensions(&extension_count);
	createInfo.enabledExtensionCount = extension_count;
	createInfo.ppEnabledExtensionNames = extensions;

	if (ZEST_ENABLE_VALIDATION_LAYER) {
		createInfo.enabledLayerCount = (zest_uint)zest__validation_layer_count;
		createInfo.ppEnabledLayerNames = zest_validation_layers;
	}
	else {
		createInfo.enabledLayerCount = 0;
	}

	zest_uint extension_property_count = 0;
	vkEnumerateInstanceExtensionProperties(zest_null, &extension_property_count, zest_null);

	zest__array(available_extensions, VkExtensionProperties, extension_property_count);

	vkEnumerateInstanceExtensionProperties(zest_null, &extension_property_count, available_extensions);

	for (int i = 0; i != extension_property_count; ++i) {
		printf("\t%s\n", available_extensions[i].extensionName);
	}

	VkResult result = vkCreateInstance(&createInfo, zest_null, &ZestDevice->instance);

	ZEST_VK_CHECK_RESULT(result);

	zest__free(available_extensions);
	//CreateWindowSurface(Device->window);

}

static VKAPI_ATTR VkBool32 VKAPI_CALL zest_debug_callback( VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageType, const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData) {
	printf("Validation Layer: %s\n", pCallbackData->pMessage);

	return VK_FALSE;
}

VkResult zest_create_debug_messenger(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pDebugMessenger) {
	PFN_vkCreateDebugUtilsMessengerEXT func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
	if (func != zest_null) {
		return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
	}
	else {
		return VK_ERROR_EXTENSION_NOT_PRESENT;
	}
}

void zest_destroy_debug_messenger() {
	if (ZestDevice->debug_messenger != VK_NULL_HANDLE) {
		PFN_vkDestroyDebugUtilsMessengerEXT destroyDebugUtilsMessenger =
			(PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(ZestDevice->instance, "vkDestroyDebugUtilsMessengerEXT");
		if (destroyDebugUtilsMessenger) {
			destroyDebugUtilsMessenger(ZestDevice->instance, ZestDevice->debug_messenger, NULL);
		}
		ZestDevice->debug_messenger = VK_NULL_HANDLE;
	}
}

void zest__setup_validation() {
	if (!ZEST_ENABLE_VALIDATION_LAYER) return;

	zest__vk_create_info(create_info, VkDebugUtilsMessengerCreateInfoEXT);
	create_info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
	create_info.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
	create_info.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
	create_info.pfnUserCallback = zest_debug_callback;

	ZEST_VK_CHECK_RESULT(zest_create_debug_messenger(ZestDevice->instance, &create_info, zest_null, &ZestDevice->debug_messenger));

	ZestDevice->pfnSetDebugUtilsObjectNameEXT = (PFN_vkSetDebugUtilsObjectNameEXT)vkGetInstanceProcAddr(ZestDevice->instance, "vkSetDebugUtilsObjectNameEXT");
}

//Get the extensions that we need for the app.
const char** zest__get_required_extensions(zest_uint *extension_count) {
	zest_uint glfw_extension_count = 0;
	const char** glfw_extensions = glfwGetRequiredInstanceExtensions(&glfw_extension_count);

	assert(glfw_extensions); //Vulkan not available

	*extension_count = glfw_extension_count + 1 + ZEST_ENABLE_VALIDATION_LAYER;
	zest__array(extensions, const char*, *extension_count);
	for (int i = 0; i != glfw_extension_count; ++i) {
		extensions[i] = glfw_extensions[i];
	}
	if (ZEST_ENABLE_VALIDATION_LAYER) {
		extensions[*extension_count - 2] = VK_EXT_DEBUG_UTILS_EXTENSION_NAME;
	}
	extensions[*extension_count - 1] = VK_KHR_EXTERNAL_FENCE_CAPABILITIES_EXTENSION_NAME;
	
	return extensions;
}

zest_bool zest__check_validation_layer_support() {
	zest_uint layer_count;
	vkEnumerateInstanceLayerProperties(&layer_count, zest_null);

	zest__array(available_layers, VkLayerProperties, layer_count);
	vkEnumerateInstanceLayerProperties(&layer_count, available_layers);

	for (int i = 0; i != zest__validation_layer_count; ++i) {
		zest_bool layer_found = 0;
		const char *layer_name = zest_validation_layers[i];

		for (int layer = 0; layer != layer_count; ++layer) {
			if (strcmp(layer_name, available_layers[layer].layerName) == 0) {
				layer_found = 1;
				break;
			}
		}

		if (!layer_found) {
			return 0;
		}
	}

	zest__free(available_layers);

	return 1;
}

void zest__initialise_app() {
	memset(ZestApp, 0, sizeof(zest_app));
	ZestApp->update_callback = 0;
	ZestApp->current_elapsed = 0;
	ZestApp->update_time = 0;
	ZestApp->render_time = 0;
	ZestApp->frame_timer = 0;
	ZestApp->mouse_x = 0;
	ZestApp->mouse_y = 0;
	ZestApp->mouse_wheel_v = 0;
	ZestApp->mouse_wheel_h = 0;
	ZestApp->mouse_delta_x = 0;
	ZestApp->mouse_delta_y = 0;
	ZestApp->last_mouse_x = 0;
	ZestApp->last_mouse_y = 0;
	ZestApp->virtual_mouse_x = 0;
	ZestApp->virtual_mouse_y = 0;

	ZestApp->window = zest__create_window(50, 50, 800, 600, 0, "Zest");
}

void zest__destroy() {
	zest_destroy_debug_messenger();
	vkDestroyInstance(ZestDevice->instance, zest_null);
	free(ZestDevice->memory_pools[0]);
}

void zest__keyboard_input_callback(GLFWwindow* window, int key, int scancode, int action, int mods) {
	if (key == GLFW_KEY_UNKNOWN) return;

	if (action != GLFW_PRESS && action != GLFW_RELEASE)
		return;

	int state = glfwGetKey(window, key);

	printf("Key State: %i\n", state);
}

void zest__mouse_scroll_callback(GLFWwindow* window, double offset_x, double offset_y) {
	ZestApp->mouse_wheel_h = offset_x;
	ZestApp->mouse_wheel_v = offset_y;
}

void zest__framebuffer_size_callback(GLFWwindow* window, int width, int height) {
	ZestApp->window->framebuffer_resized = 1;
}

zest_window* zest__create_window(int x, int y, int width, int height, zest_bool maximised, const char *title) {
	zest_assert(ZestDevice);		//Must initialise the ZestDevice first

	zest_window *window = zest__allocate(sizeof(zest_window));
	glfwInit();

	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	if (maximised)
		glfwWindowHint(GLFW_MAXIMIZED, GLFW_TRUE);

	window->window_width = width;
	window->window_height = height;

	window->window_handle = glfwCreateWindow(width, height, title, 0, 0);
	if (!maximised)
		glfwSetWindowPos(window->window_handle, x, y);
	glfwSetWindowUserPointer(window->window_handle, ZestApp);
	glfwSetKeyCallback(window->window_handle, zest__keyboard_input_callback);
	glfwSetScrollCallback(window->window_handle, zest__mouse_scroll_callback);
	glfwSetFramebufferSizeCallback(window->window_handle, zest__framebuffer_size_callback);
	//glfwSetCharCallback(window->window_handle, character_func);

	if (maximised) {
		int width, height;
		glfwGetFramebufferSize(window->window_handle, &width, &height);
		window->window_width = width;
		window->window_height = height;
	}

	return window;
}

void zest__main_loop() {
	while (!glfwWindowShouldClose(ZestApp->window->window_handle)) {

		//VK_CHECK_RESULT(vkWaitForFences(Device->logical_device, 1, &Renderer->fif_fence[Device->current_fif], VK_TRUE, UINT64_MAX));
		//DoScheduledTasks(Device->current_fif);

		glfwPollEvents();

		if (ZestApp->update_callback) {
			ZestApp->update_callback(0, 0);
		}

		//DrawRendererFrame();

		if (ZestApp->flags & zest_app_flag_show_fps_in_title) {
			ZestApp->frame_timer += ZestApp->current_elapsed;
			ZestApp->frame_count++;
			if (ZestApp->frame_timer >= 1.f) {
				glfwSetWindowTitle(ZestApp->window->window_handle, "FPS in Title");

				ZestApp->last_fps = ZestApp->frame_count;
				ZestApp->frame_count = 0;
				ZestApp->frame_timer -= 1.f;
			}
		}

	}
}

//Testing

int main() {
	printf("I ran!");

	zest_Initialise();
	zest_Start();

	return 0;
}
