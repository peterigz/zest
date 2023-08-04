#include "zest.h"
#define TLOC_IMPLEMENTATION
#define TLOC_OUTPUT_ERROR_MESSAGES
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

void zest_Initialise(zest_create_info *info) {
	void *memory_pool = malloc(tloc__MEGABYTE(128));

	ZEST_ASSERT(memory_pool);	//unable to allocate initial memory pool

	tloc_allocator *allocator = tloc_InitialiseAllocatorWithPool(memory_pool, tloc__MEGABYTE(128));
	ZestDevice = tloc_Allocate(allocator, sizeof(zest_device));
	ZestApp = tloc_Allocate(allocator, sizeof(zest_app));
	ZestRenderer = tloc_Allocate(allocator, sizeof(zest_renderer));
	ZestHasher = tloc_Allocate(allocator, sizeof(zest_hasher));
	memset(ZestDevice, 0, sizeof(zest_device));
	ZestDevice->memory_pools[0] = memory_pool;
	ZestDevice->memory_pool_count = 0;
	ZestDevice->allocator = allocator;
	zest__initialise_app(info);
	zest__initialise_device();
	zest__initialise_renderer(info);
	zest__create_empty_command_queue(&ZestRenderer->empty_queue);
}

void zest_Start() {
	zest__main_loop();
	zest__destroy();
}

void zest__initialise_device() {
	zest__create_instance();
	zest__setup_validation();
	zest__pick_physical_device();
	zest__create_logical_device();
	zest__set_limit_data();
}

/*
Functions that create a vulkan device
*/
void zest__create_instance(void) {
	if (ZEST_ENABLE_VALIDATION_LAYER) {
		ZEST_ASSERT(zest__check_validation_layer_support());
	}

	VkApplicationInfo app_info = { 0 };
	app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	app_info.pApplicationName = "Zest";
	app_info.applicationVersion = VK_API_VERSION_1_0;
	app_info.pEngineName = "No Engine";
	app_info.engineVersion = VK_API_VERSION_1_0;
	app_info.apiVersion = VK_API_VERSION_1_0;
	ZestDevice->api_version = app_info.apiVersion;

	VkInstanceCreateInfo create_info = { 0 };
	create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	create_info.pApplicationInfo = &app_info;
	create_info.flags = 0;
	create_info.pNext = 0;

	zest_uint extension_count = 0;
	const char** extensions = zest__get_required_extensions(&extension_count);
	create_info.enabledExtensionCount = extension_count;
	create_info.ppEnabledExtensionNames = extensions;

	if (ZEST_ENABLE_VALIDATION_LAYER) {
		create_info.enabledLayerCount = (zest_uint)zest__validation_layer_count;
		create_info.ppEnabledLayerNames = zest_validation_layers;
	}
	else {
		create_info.enabledLayerCount = 0;
	}

	zest_uint extension_property_count = 0;
	vkEnumerateInstanceExtensionProperties(ZEST_NULL, &extension_property_count, ZEST_NULL);

	ZEST__ARRAY(available_extensions, VkExtensionProperties, extension_property_count);

	vkEnumerateInstanceExtensionProperties(ZEST_NULL, &extension_property_count, available_extensions);

	for (int i = 0; i != extension_property_count; ++i) {
		printf("\t%s\n", available_extensions[i].extensionName);
	}

	VkResult result = vkCreateInstance(&create_info, ZEST_NULL, &ZestDevice->instance);

	ZEST_VK_CHECK_RESULT(result);

	ZEST__FREE(available_extensions);
	zest__create_window_surface(ZestApp->window);

}

void zest__create_window_surface(zest_window* window) {
	ZEST_VK_CHECK_RESULT(glfwCreateWindowSurface(ZestDevice->instance, window->window_handle, ZEST_NULL, &window->surface));
}

static VKAPI_ATTR VkBool32 VKAPI_CALL zest_debug_callback( VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageType, const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData) {
	printf("Validation Layer: %s\n", pCallbackData->pMessage);

	return VK_FALSE;
}

VkResult zest_create_debug_messenger(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pDebugMessenger) {
	PFN_vkCreateDebugUtilsMessengerEXT func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
	if (func != ZEST_NULL) {
		return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
	}
	else {
		return VK_ERROR_EXTENSION_NOT_PRESENT;
	}
}

void zest_destroy_debug_messenger(void) {
	if (ZestDevice->debug_messenger != VK_NULL_HANDLE) {
		PFN_vkDestroyDebugUtilsMessengerEXT destroyDebugUtilsMessenger =
			(PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(ZestDevice->instance, "vkDestroyDebugUtilsMessengerEXT");
		if (destroyDebugUtilsMessenger) {
			destroyDebugUtilsMessenger(ZestDevice->instance, ZestDevice->debug_messenger, NULL);
		}
		ZestDevice->debug_messenger = VK_NULL_HANDLE;
	}
}

void zest__setup_validation(void) {
	if (!ZEST_ENABLE_VALIDATION_LAYER) return;

	VkDebugUtilsMessengerCreateInfoEXT create_info = { 0 };
	create_info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
	create_info.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
	create_info.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
	create_info.pfnUserCallback = zest_debug_callback;

	ZEST_VK_CHECK_RESULT(zest_create_debug_messenger(ZestDevice->instance, &create_info, ZEST_NULL, &ZestDevice->debug_messenger));

	ZestDevice->pfnSetDebugUtilsObjectNameEXT = (PFN_vkSetDebugUtilsObjectNameEXT)vkGetInstanceProcAddr(ZestDevice->instance, "vkSetDebugUtilsObjectNameEXT");
}

//Get the extensions that we need for the app.
const char** zest__get_required_extensions(zest_uint *extension_count) {
	zest_uint glfw_extension_count = 0;
	const char** glfw_extensions = glfwGetRequiredInstanceExtensions(&glfw_extension_count);

	assert(glfw_extensions); //Vulkan not available

	*extension_count = glfw_extension_count + 1 + ZEST_ENABLE_VALIDATION_LAYER;
	ZEST__ARRAY(extensions, const char*, *extension_count);
	for (int i = 0; i != glfw_extension_count; ++i) {
		extensions[i] = glfw_extensions[i];
	}
	if (ZEST_ENABLE_VALIDATION_LAYER) {
		extensions[*extension_count - 2] = VK_EXT_DEBUG_UTILS_EXTENSION_NAME;
	}
	extensions[*extension_count - 1] = VK_KHR_EXTERNAL_FENCE_CAPABILITIES_EXTENSION_NAME;
	
	return extensions;
}

zest_uint zest_find_memory_type(zest_uint typeFilter, VkMemoryPropertyFlags properties) {
	VkPhysicalDeviceMemoryProperties memory_properties;
	vkGetPhysicalDeviceMemoryProperties(ZestDevice->physical_device, &memory_properties);

	for (zest_uint i = 0; i < memory_properties.memoryTypeCount; i++) {
		if ((typeFilter & (1 << i)) && (memory_properties.memoryTypes[i].propertyFlags & properties) == properties) {
			return i;
		}
	}

	return ZEST_INVALID;
}

zest_bool zest__check_validation_layer_support(void) {
	zest_uint layer_count;
	vkEnumerateInstanceLayerProperties(&layer_count, ZEST_NULL);

	ZEST__ARRAY(available_layers, VkLayerProperties, layer_count);
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

	ZEST__FREE(available_layers);

	return 1;
}

void zest__pick_physical_device(void) {
	zest_uint device_count = 0;
	vkEnumeratePhysicalDevices(ZestDevice->instance, &device_count, ZEST_NULL);

	assert(device_count);		//Failed to find GPUs with Vulkan support!

	ZEST__ARRAY(devices, VkPhysicalDevice, device_count);
	vkEnumeratePhysicalDevices(ZestDevice->instance, &device_count, devices);

	ZestDevice->physical_device = VK_NULL_HANDLE;
	for (int i = 0; i != device_count; ++i) {
		if (zest__is_device_suitable(devices[i])) {
			ZestDevice->physical_device = devices[i];
			ZestDevice->msaa_samples = zest__get_max_useable_sample_count();
			break;
		}
	}

	ZEST_ASSERT(ZestDevice->physical_device != VK_NULL_HANDLE);	//Unable to find suitable GPU

	// Store Properties features, limits and properties of the physical ZestDevice for later use
	// ZestDevice properties also contain limits and sparse properties
	vkGetPhysicalDeviceProperties(ZestDevice->physical_device, &ZestDevice->properties);
	// Features should be checked by the examples before using them
	vkGetPhysicalDeviceFeatures(ZestDevice->physical_device, &ZestDevice->features);
	// Memory properties are used regularly for creating all kinds of buffers
	vkGetPhysicalDeviceMemoryProperties(ZestDevice->physical_device, &ZestDevice->memory_properties);

	//Print out the memory available
	for (int i = 0; i != ZestDevice->memory_properties.memoryHeapCount; ++i) {
		//std::cout << ZestDevice->memory_properties.memoryHeaps[i].flags << " - " << ZestDevice->memory_properties.memoryHeaps[i].size << std::endl;
	}

	ZEST__FREE(devices);
}

zest_bool zest__is_device_suitable(VkPhysicalDevice physical_device) {
	zest_queue_family_indices indices = zest__find_queue_families(physical_device);

	zest_bool extensions_supported = zest__check_device_extension_support(physical_device);

	zest_bool swapchain_adequate = 0;
	if (extensions_supported) {
		ZestDevice->swapchain_support_details = zest__query_swapchain_support(physical_device);
		swapchain_adequate = ZestDevice->swapchain_support_details.formats_count && ZestDevice->swapchain_support_details.present_modes_count;
	}

	VkPhysicalDeviceFeatures supportedFeatures;
	vkGetPhysicalDeviceFeatures(physical_device, &supportedFeatures);

	return zest__family_is_complete(&indices) && extensions_supported && swapchain_adequate && supportedFeatures.samplerAnisotropy;
}

zest_queue_family_indices zest__find_queue_families(VkPhysicalDevice physical_device) {
	zest_queue_family_indices indices;

	zest_uint queue_family_count = 0;
	vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &queue_family_count, ZEST_NULL);

	ZEST__ARRAY(queue_families, VkQueueFamilyProperties, queue_family_count);
	vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &queue_family_count, queue_families);

	int i = 0;
	zest_bool compute_found = 0;
	for (int f = 0; f != queue_family_count; ++f) {
		if (queue_families[f].queueCount > 0 && queue_families[f].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
			zest__set_graphics_family(&indices, i);
		}

		if ((queue_families[f].queueFlags & VK_QUEUE_COMPUTE_BIT) && ((queue_families[f].queueFlags & VK_QUEUE_GRAPHICS_BIT) == 0)) {
			zest__set_compute_family(&indices, i);
			compute_found = 1;
		}

		VkBool32 present_support = VK_FALSE;
		vkGetPhysicalDeviceSurfaceSupportKHR(physical_device, i, ZestApp->window->surface, &present_support);

		if (queue_families[f].queueCount > 0 && present_support) {
			zest__set_present_family(&indices, i);
		}

		if (zest__family_is_complete(&indices)) {
			break;
		}

		i++;
	}

	i = 0;
	if (!compute_found) {
		for (int f = 0; f != queue_family_count; ++f) {
			if (queue_families[f].queueFlags & VK_QUEUE_COMPUTE_BIT) {
				zest__set_compute_family(&indices, i);
			}
			i++;
		}
	}

	ZEST__FREE(queue_families);
	return indices;
}

zest_bool zest__check_device_extension_support(VkPhysicalDevice physical_device) {
	zest_uint extension_count;
	vkEnumerateDeviceExtensionProperties(physical_device, ZEST_NULL, &extension_count, ZEST_NULL);

	ZEST__ARRAY(available_extensions, VkExtensionProperties, extension_count);
	vkEnumerateDeviceExtensionProperties(physical_device, ZEST_NULL, &extension_count, available_extensions);

	zest_uint required_extensions_found = 0;
	for (int i = 0; i != extension_count; ++i) {
		for (int e = 0; e != zest__required_extension_names_count; ++e) {
			if (strcmp(available_extensions[i].extensionName, zest_required_extensions[e]) == 0) {
				required_extensions_found++;
			}
		}
	}

	return required_extensions_found >= zest__required_extension_names_count;
}

zest_swapchain_support_details zest__query_swapchain_support(VkPhysicalDevice physical_device) {
	zest_swapchain_support_details details;

	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physical_device, ZestApp->window->surface, &details.capabilities);

	vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device, ZestApp->window->surface, &details.formats_count, ZEST_NULL);

	if (details.formats_count != 0) {
		details.formats = ZEST__ALLOCATE(sizeof(VkSurfaceFormatKHR) * details.formats_count);
		vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device, ZestApp->window->surface, &details.formats_count, details.formats);
	}

	vkGetPhysicalDeviceSurfacePresentModesKHR(physical_device, ZestApp->window->surface, &details.present_modes_count, ZEST_NULL);

	if (details.present_modes_count != 0) {
		details.present_modes = ZEST__ALLOCATE(sizeof(VkPresentModeKHR) * details.present_modes_count);
		vkGetPhysicalDeviceSurfacePresentModesKHR(physical_device, ZestApp->window->surface, &details.present_modes_count, details.present_modes);
	}

	return details;
}

VkSampleCountFlagBits zest__get_max_useable_sample_count(void) {
	VkPhysicalDeviceProperties physical_device_properties;
	vkGetPhysicalDeviceProperties(ZestDevice->physical_device, &physical_device_properties);

	VkSampleCountFlags counts = ZEST__MIN(physical_device_properties.limits.framebufferColorSampleCounts, physical_device_properties.limits.framebufferDepthSampleCounts);
	if (counts & VK_SAMPLE_COUNT_64_BIT) { return VK_SAMPLE_COUNT_64_BIT; }
	if (counts & VK_SAMPLE_COUNT_32_BIT) { return VK_SAMPLE_COUNT_32_BIT; }
	if (counts & VK_SAMPLE_COUNT_16_BIT) { return VK_SAMPLE_COUNT_16_BIT; }
	if (counts & VK_SAMPLE_COUNT_8_BIT) { return VK_SAMPLE_COUNT_8_BIT; }
	if (counts & VK_SAMPLE_COUNT_4_BIT) { return VK_SAMPLE_COUNT_4_BIT; }
	if (counts & VK_SAMPLE_COUNT_2_BIT) { return VK_SAMPLE_COUNT_2_BIT; }

	return VK_SAMPLE_COUNT_1_BIT;
}

void zest__create_logical_device(void) {
	zest_queue_family_indices indices = zest__find_queue_families(ZestDevice->physical_device);

	VkDeviceQueueCreateInfo queue_create_infos[3];
	zest_uint unique_queue_families[3] = { indices.graphics_family, indices.present_family, indices.compute_family };
	unique_queue_families[0] = indices.graphics_family;
	unique_queue_families[1] = indices.graphics_family == indices.present_family ? indices.compute_family : indices.present_family;
	unique_queue_families[2] = unique_queue_families[1] == indices.compute_family ? ZEST_INVALID : indices.compute_family;
	if (unique_queue_families[0] == unique_queue_families[1]) {
		unique_queue_families[1] = ZEST_INVALID;
	}

	float graphics_queue_priority[2] = { 0.f, 1.0f };
	float queue_priority = 1.0f;
	zest_uint queue_create_count = 0;
	for (int i = 0; i != 3; ++i) {
		if (unique_queue_families[i] == ZEST_INVALID) {
			break;
		}
		VkDeviceQueueCreateInfo queue_create_info = { 0 };
		queue_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		queue_create_info.queueFamilyIndex = unique_queue_families[i];
		if (unique_queue_families[i] == indices.graphics_family) {
			queue_create_info.queueCount = 2;
			queue_create_info.pQueuePriorities = graphics_queue_priority;
		}
		else {
			queue_create_info.queueCount = 1;
			queue_create_info.pQueuePriorities = &queue_priority;
		}
		queue_create_infos[i] = queue_create_info;
		queue_create_count++;
	}

	VkPhysicalDeviceFeatures device_features = { 0 };
	device_features.samplerAnisotropy = VK_TRUE;
	device_features.wideLines = VK_TRUE;
	//device_features.dualSrcBlend = VK_TRUE;
	//device_features.vertexPipelineStoresAndAtomics = VK_TRUE;
	VkPhysicalDeviceVulkan12Features device_features_12 = { 0 };
	device_features_12.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
	device_features_12.bufferDeviceAddress = VK_TRUE;

	VkDeviceCreateInfo create_info = { 0 };
	create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	create_info.pEnabledFeatures = &device_features;
	create_info.queueCreateInfoCount = queue_create_count;
	create_info.pQueueCreateInfos = queue_create_infos;
	create_info.enabledExtensionCount = zest__required_extension_names_count;
	create_info.ppEnabledExtensionNames = zest_required_extensions;
	if (ZestDevice->api_version == VK_API_VERSION_1_2) {
		create_info.pNext = &device_features_12;
	}

	if (ZEST_ENABLE_VALIDATION_LAYER) {
		create_info.enabledLayerCount = zest__validation_layer_count;
		create_info.ppEnabledLayerNames = zest_validation_layers;
	}
	else {
		create_info.enabledLayerCount = 0;
	}

	ZEST_VK_CHECK_RESULT(vkCreateDevice(ZestDevice->physical_device, &create_info, ZEST_NULL, &ZestDevice->logical_device));

	vkGetDeviceQueue(ZestDevice->logical_device, indices.graphics_family, 0, &ZestDevice->graphics_queue);
	vkGetDeviceQueue(ZestDevice->logical_device, indices.graphics_family, 1, &ZestDevice->one_time_graphics_queue);
	vkGetDeviceQueue(ZestDevice->logical_device, indices.present_family, 0, &ZestDevice->present_queue);
	vkGetDeviceQueue(ZestDevice->logical_device, indices.compute_family, 0, &ZestDevice->compute_queue);

	ZestDevice->graphics_queue_family_index = indices.graphics_family;
	ZestDevice->present_queue_family_index = indices.present_family;
	ZestDevice->compute_queue_family_index = indices.compute_family;

	VkCommandPoolCreateInfo cmd_info_pool = { 0 };
	cmd_info_pool.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	cmd_info_pool.queueFamilyIndex = ZestDevice->graphics_queue_family_index;
	cmd_info_pool.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
	ZEST_VK_CHECK_RESULT(vkCreateCommandPool(ZestDevice->logical_device, &cmd_info_pool, ZEST_NULL, &ZestDevice->command_pool));
}

void zest__set_limit_data() {
	VkPhysicalDeviceProperties physicalDeviceProperties;
	vkGetPhysicalDeviceProperties(ZestDevice->physical_device, &physicalDeviceProperties);

	ZestDevice->max_image_size = physicalDeviceProperties.limits.maxImageDimension2D;
}

/*
End of Device creation functions
*/

void zest__initialise_app(zest_create_info *create_info) {
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

	ZestApp->window = zest__create_window(create_info->screen_x, create_info->screen_y, create_info->screen_width, create_info->screen_height, 0, "Zest");
}

void zest__destroy(void) {
	zest_WaitForIdleDevice();
	zest__cleanup_renderer();
	vkDestroySurfaceKHR(ZestDevice->instance, ZestApp->window->surface, ZEST_NULL);
	glfwDestroyWindow(ZestApp->window->window_handle);
	glfwTerminate();
	zest_destroy_debug_messenger();
	vkDestroyCommandPool(ZestDevice->logical_device, ZestDevice->command_pool, ZEST_NULL);
	vkDestroyDevice(ZestDevice->logical_device, ZEST_NULL);
	vkDestroyInstance(ZestDevice->instance, ZEST_NULL);
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
	ZEST_ASSERT(ZestDevice);		//Must initialise the ZestDevice first

	zest_window *window = ZEST__ALLOCATE(sizeof(zest_window));
	glfwInit();

	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	if (maximised)
		glfwWindowHint(GLFW_MAXIMIZED, GLFW_TRUE);

	window->window_width = width;
	window->window_height = height;

	window->window_handle = glfwCreateWindow(width, height, title, 0, 0);
	if (!maximised) {
		glfwSetWindowPos(window->window_handle, x, y);
	}
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

void zest__update_window_size(zest_window* window, zest_uint width, zest_uint height) {
	window->window_width = width;
	window->window_height = height;
}

// --Vulkan Buffer Management
VkResult zest__bind_memory(zest_device_memory_pool *memory_allocation, VkDeviceSize offset) {
	return vkBindBufferMemory(ZestDevice->logical_device, memory_allocation->buffer, memory_allocation->memory, offset);
}

VkResult zest__map_memory(zest_device_memory_pool *memory_allocation, VkDeviceSize size, VkDeviceSize offset) {
	return vkMapMemory(ZestDevice->logical_device, memory_allocation->memory, offset, size, 0, &memory_allocation->mapped);
}

void zest__unmap_memory(zest_device_memory_pool *memory_allocation) {
	if (memory_allocation->mapped) {
		vkUnmapMemory(ZestDevice->logical_device, memory_allocation->memory);
		memory_allocation->mapped = ZEST_NULL;
	}
}

void zest__destroy_memory(zest_device_memory_pool *memory_allocation) {
	if (memory_allocation->buffer) {
		vkDestroyBuffer(ZestDevice->logical_device, memory_allocation->buffer, ZEST_NULL);
	}
	if (memory_allocation->memory) {
		vkFreeMemory(ZestDevice->logical_device, memory_allocation->memory, ZEST_NULL);
	}
	memory_allocation->mapped = ZEST_NULL;
}

VkResult zest__flush_memory(zest_device_memory_pool *memory_allocation, VkDeviceSize size, VkDeviceSize offset) {
	VkMappedMemoryRange mappedRange = { 0 };
	mappedRange.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
	mappedRange.memory = memory_allocation->memory;
	mappedRange.offset = offset;
	mappedRange.size = size;
	return vkFlushMappedMemoryRanges(ZestDevice->logical_device, 1, &mappedRange);
}

void zest__create_device_memory_pool(VkDeviceSize size, VkBufferUsageFlags usage_flags, VkMemoryPropertyFlags property_flags, zest_device_memory_pool *buffer, const char *name) {
	VkBufferCreateInfo buffer_info = { 0 };
	buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	buffer_info.size = size;
	buffer_info.usage = usage_flags;
	buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	ZEST_VK_CHECK_RESULT(vkCreateBuffer(ZestDevice->logical_device, &buffer_info, ZEST_NULL, &buffer->buffer));

	VkMemoryRequirements memory_requirements;
	vkGetBufferMemoryRequirements(ZestDevice->logical_device, buffer->buffer, &memory_requirements);

	VkMemoryAllocateFlagsInfo flags;
	flags.deviceMask = 0;
	flags.flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT;
	flags.pNext = NULL;
	flags.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO;

	VkMemoryAllocateInfo alloc_info = { 0 };
	alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	alloc_info.allocationSize = memory_requirements.size;
	alloc_info.memoryTypeIndex = zest_find_memory_type(memory_requirements.memoryTypeBits, property_flags);
	ZEST_ASSERT(alloc_info.memoryTypeIndex != ZEST_INVALID);
	if (ZEST_ENABLE_VALIDATION_LAYER && ZestDevice->api_version == VK_API_VERSION_1_2) {
		alloc_info.pNext = &flags;
	}
	ZEST_VK_CHECK_RESULT(vkAllocateMemory(ZestDevice->logical_device, &alloc_info, ZEST_NULL, &buffer->memory));

	if (ZEST_ENABLE_VALIDATION_LAYER && ZestDevice->api_version == VK_API_VERSION_1_2) {
		ZestDevice->use_labels_address_bit = VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
		const VkDebugUtilsObjectNameInfoEXT buffer_name_info =
		{
			VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT, // sType
			NULL,                                               // pNext
			VK_OBJECT_TYPE_BUFFER,                              // objectType
			(uint64_t)buffer->buffer,                           // objectHandle
			name,					                            // pObjectName
		};

		ZestDevice->pfnSetDebugUtilsObjectNameEXT(ZestDevice->logical_device, &buffer_name_info);
	}

	buffer->size = memory_requirements.size;
	buffer->alignment = memory_requirements.alignment;
	buffer->memory_type_index = alloc_info.memoryTypeIndex;
	buffer->property_flags = property_flags;
	buffer->usage_flags = usage_flags;
	buffer->descriptor.buffer = buffer->buffer;
	buffer->descriptor.offset = 0;
	buffer->descriptor.range = buffer->size;

	ZEST_VK_CHECK_RESULT(zest__bind_memory(buffer, 0));
}

void zest__create_image_memory_pool(VkDeviceSize size_in_bytes, VkImage image, VkMemoryPropertyFlags property_flags, zest_device_memory_pool *buffer) {
	VkMemoryRequirements memory_requirements;
	vkGetImageMemoryRequirements(ZestDevice->logical_device, image, &memory_requirements);

	VkMemoryAllocateInfo alloc_info = { 0 };
	alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	alloc_info.allocationSize = size_in_bytes;
	alloc_info.memoryTypeIndex = zest_find_memory_type(memory_requirements.memoryTypeBits, property_flags);
	ZEST_ASSERT(alloc_info.memoryTypeIndex != ZEST_INVALID);

	//Found no suitable blocks or ranges, so create a new one
	buffer->size = size_in_bytes;
	buffer->alignment = memory_requirements.alignment;
	buffer->memory_type_index = alloc_info.memoryTypeIndex;
	buffer->property_flags = property_flags;
	buffer->usage_flags = 0;

	ZEST_VK_CHECK_RESULT(vkAllocateMemory(ZestDevice->logical_device, &alloc_info, ZEST_NULL, &buffer->memory));
}

zest_buffer *zest_CreateBuffer(VkDeviceSize size, zest_buffer_info *buffer_info, VkImage image, VkDeviceSize pool_size) {
	if (image != VK_NULL_HANDLE) {
		VkMemoryRequirements memory_requirements;
		vkGetImageMemoryRequirements(ZestDevice->logical_device, image, &memory_requirements);
		buffer_info->memoryTypeBits = memory_requirements.memoryTypeBits;
		buffer_info->alignment = memory_requirements.alignment;
	}

	zest_key key = zest_map_hash_ptr(ZestRenderer->buffer_allocators, buffer_info, sizeof(zest_buffer_info));
	if (!zest_map_valid_key(ZestRenderer->buffer_allocators, key)) {
		//If an allocator doesn't exist yet for this combination of usage and buffer properties then create one.
		zest_buffer_allocator buffer_allocator = { 0 };
		buffer_allocator.property_flags = buffer_info->property_flags;
		buffer_allocator.usage_flags = buffer_info->usage_flags;
		zest_device_memory_pool buffer_pool = { 0 };
		if (pool_size < size) {
			pool_size = size * 2;
		}
		if (image == VK_NULL_HANDLE) {
			zest__create_device_memory_pool(pool_size ? pool_size : tloc__MEGABYTE(64), buffer_info->usage_flags, buffer_info->property_flags, &buffer_pool, "");
		}
		else {
			zest__create_image_memory_pool(pool_size ? pool_size : tloc__MEGABYTE(128), image, buffer_info->property_flags, &buffer_pool);
		}
		if (buffer_info->property_flags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) {
			zest__map_memory(&buffer_pool, VK_WHOLE_SIZE, 0);
		}
		zest_vec_push(buffer_allocator.memory_pools, buffer_pool);
		tloc_size size_of_allocator = tloc_AllocatorSize() + (sizeof(zest_buffer) * ZEST_MAX_BUFFERS_PER_ALLOCATOR);
		buffer_allocator.allocator = ZEST__REALLOCATE(buffer_allocator.allocator, size_of_allocator);
		buffer_allocator.allocator = tloc_InitialiseAllocatorWithPool(buffer_allocator.allocator, size_of_allocator);
		zest_map_insert_key(ZestRenderer->buffer_allocators, key, buffer_allocator);
		zest_buffer *dummy_buffer = tloc_Allocate(buffer_allocator.allocator, sizeof(zest_buffer));
		dummy_buffer->size = 0;
		dummy_buffer->memory_offset = 0;
		dummy_buffer->memory_pool = zest_vec_last_index(buffer_allocator.memory_pools);
	}

	zest_buffer_allocator *buffer_allocator = zest_map_at_key(ZestRenderer->buffer_allocators, key);
	zest_buffer *buffer = tloc_Allocate(buffer_allocator->allocator, sizeof(zest_buffer));
	buffer->buffer_allocator = buffer_allocator;
	if (buffer) {
		//If buffer is null then more then ZEST_MAX_BUFFERS_PER_ALLOCATOR buffers have been created
		zest_buffer *previous_buffer = tloc__block_user_ptr(tloc__block_from_allocation(buffer)->prev_physical_block);
		buffer->memory_offset = previous_buffer->memory_offset + previous_buffer->size;
		buffer->memory_pool = previous_buffer->memory_pool;
		buffer->size = tloc__align_size_up(size, buffer_allocator->memory_pools[buffer->memory_pool].alignment);
		if (buffer_info->property_flags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) {
			buffer->data = (void*)((char*)buffer_allocator->memory_pools[buffer->memory_pool].mapped + buffer->memory_offset);
		}
		else {
			buffer->data = ZEST_NULL;
		}
		if (buffer->memory_offset + buffer->size > buffer_allocator->memory_pools[buffer->memory_pool].size) {
			//Pool has run out of space, add a new one if we can
			zest_device_memory_pool buffer_pool;
			zest__create_device_memory_pool(tloc__MEGABYTE(128), buffer_info->usage_flags, buffer_info->property_flags, &buffer_pool, "");
			zest_vec_push(buffer_allocator->memory_pools, buffer_pool);
			buffer->memory_pool = zest_vec_last_index(buffer_allocator->memory_pools);
			buffer->memory_offset = 0;
			if (buffer_info->property_flags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) {
				buffer->data = (void*)((char*)buffer_allocator->memory_pools[buffer->memory_pool].mapped + buffer->memory_offset);
			}
			else {
				buffer->data = ZEST_NULL;
			}
		}
	}

	return buffer;
}

void zest_FreeBuffer(zest_buffer *buffer) {
	ZEST_ASSERT(buffer);	//Buffer must point to a valid buffer
	tloc_Free(buffer->buffer_allocator->allocator, buffer);
}
// --End Vulkan Buffer Management

// --Renderer and related functions
void zest__initialise_renderer(zest_create_info *create_info) {
	memset(ZestRenderer, 0, sizeof(zest_renderer));
	zest__create_swapchain();
	zest__create_swapchain_image_views();

	zest__make_standard_render_passes();
	ZestRenderer->final_render_pass.render_pass = zest__map_get_index(ZestRenderer->render_passes.map, zest_map_hash(ZestHasher, "Render pass present"));

	ZestRenderer->depth_resource_buffer = zest__create_depth_resources();
	zest__create_frame_buffers();
	zest__create_sync_objects();
	ZestRenderer->push_constants.screen_resolution.x = 1.f / ZestRenderer->swapchain_extent.width;
	ZestRenderer->push_constants.screen_resolution.y = 1.f / ZestRenderer->swapchain_extent.height;

	ZestRenderer->standard_uniform_buffer_id = zest_create_uniform_buffer("Standard Uniform Buffer", sizeof(zest_uniform_buffer_data));
	ZestRenderer->update_uniform_buffer_callback = zest_update_uniform_buffer;

	zest_update_uniform_buffer(ZEST_NULL);

	zest__create_renderer_command_pools();
	zest__create_descriptor_pools(create_info->pool_counts);
	zest__make_standard_descriptor_layouts();
	zest__prepare_standard_pipelines();

	VkFenceCreateInfo fence_info = { 0 };
	fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fence_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;
	for (ZEST_EACHFFIF_i) {
		ZEST_VK_CHECK_RESULT(vkCreateFence(ZestDevice->logical_device, &fence_info, ZEST_NULL, &ZestRenderer->fif_fence[i]));
	}

	zest__create_final_render_command_buffer();
}

void zest__create_swapchain() {
	zest_swapchain_support_details swapchain_support = zest__query_swapchain_support(ZestDevice->physical_device);

	VkSurfaceFormatKHR surfaceFormat = zest__choose_swapchain_format(swapchain_support.formats);
	VkPresentModeKHR presentMode = zest_choose_present_mode(swapchain_support.present_modes, ZestRenderer->flags & zest_renderer_flag_vsync_enabled);
	VkExtent2D extent = zest_choose_swap_extent(&swapchain_support.capabilities);

	ZestRenderer->swapchain_image_format = surfaceFormat.format;
	ZestRenderer->swapchain_extent = extent;

	zest_uint image_count = swapchain_support.capabilities.minImageCount + 1;

	if (swapchain_support.capabilities.maxImageCount > 0 && image_count > swapchain_support.capabilities.maxImageCount) {
		image_count = swapchain_support.capabilities.maxImageCount;
	}

	VkSwapchainCreateInfoKHR createInfo = {0};
	createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	createInfo.surface = ZestApp->window->surface;

	createInfo.minImageCount = image_count;
	createInfo.imageFormat = surfaceFormat.format;
	createInfo.imageColorSpace = surfaceFormat.colorSpace;
	createInfo.imageExtent = extent;
	createInfo.imageArrayLayers = 1;
	createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	createInfo.preTransform = swapchain_support.capabilities.currentTransform;
	createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	createInfo.presentMode = presentMode;
	createInfo.clipped = VK_TRUE;
	createInfo.oldSwapchain = VK_NULL_HANDLE;

	ZEST_VK_CHECK_RESULT(vkCreateSwapchainKHR(ZestDevice->logical_device, &createInfo, ZEST_NULL, &ZestRenderer->swapchain));

	vkGetSwapchainImagesKHR(ZestDevice->logical_device, ZestRenderer->swapchain, &image_count, ZEST_NULL);
	zest_vec_resize(ZestRenderer->swapchain_images, image_count);
	vkGetSwapchainImagesKHR(ZestDevice->logical_device, ZestRenderer->swapchain, &image_count, ZestRenderer->swapchain_images);
}

VkPresentModeKHR zest_choose_present_mode(VkPresentModeKHR *available_present_modes, zest_bool use_vsync) {
	VkPresentModeKHR best_mode = VK_PRESENT_MODE_FIFO_KHR;

	if (use_vsync)
		return best_mode;

	for (zest_foreach_i(available_present_modes)) {
		if (available_present_modes[i] == VK_PRESENT_MODE_MAILBOX_KHR) {
			return available_present_modes[i];
		}
		else if (available_present_modes[i] == VK_PRESENT_MODE_IMMEDIATE_KHR) {
			best_mode = available_present_modes[i];
		}
	}

	return best_mode;
}

VkExtent2D zest_choose_swap_extent(VkSurfaceCapabilitiesKHR *capabilities) {
	if (capabilities->currentExtent.width != ZEST_U32_MAX_VALUE) {
		return capabilities->currentExtent;
	}
	else {
		int width, height;
		glfwGetFramebufferSize(ZestApp->window->window_handle, &width, &height);

		VkExtent2D actual_extent = {
			.width	= (zest_uint)(width),
			.height = (zest_uint)(height)
		};

		actual_extent.width = ZEST__MAX(capabilities->minImageExtent.width, ZEST__MIN(capabilities->maxImageExtent.width, actual_extent.width));
		actual_extent.height = ZEST__MAX(capabilities->minImageExtent.height, ZEST__MIN(capabilities->maxImageExtent.height, actual_extent.height));

		return actual_extent;
	}
}

VkSurfaceFormatKHR zest__choose_swapchain_format(VkSurfaceFormatKHR *available_formats) {
	if (zest_vec_size(available_formats) == 1 && available_formats[0].format == VK_FORMAT_UNDEFINED) {
		VkSurfaceFormatKHR format = {
			.format = VK_FORMAT_B8G8R8A8_UNORM,
			.colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR
		};
		return format;
	}

	VkFormat format = ZestApp->create_info.color_format == VK_FORMAT_R8G8B8A8_UNORM ? VK_FORMAT_B8G8R8A8_UNORM : VK_FORMAT_B8G8R8A8_SRGB;

	for (zest_foreach_i(available_formats)) {
		if (available_formats[i].format == format && available_formats[i].colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
			return available_formats[i];
		}
	}

	return available_formats[0];
}

void zest__cleanup_swapchain() {
	//vkDestroyImageView(ZestDevice->logical_device, ZestRenderer->final_render_pass.color.view, ZEST_NULL);
	//vkDestroyImage(ZestDevice->logical_device, ZestRenderer->final_render_pass.color.image, ZEST_NULL);

	vkDestroyImageView(ZestDevice->logical_device, ZestRenderer->final_render_pass.depth.view, ZEST_NULL);
	vkDestroyImage(ZestDevice->logical_device, ZestRenderer->final_render_pass.depth.image, ZEST_NULL);

	for (zest_foreach_i(ZestRenderer->swapchain_frame_buffers)) {
		vkDestroyFramebuffer(ZestDevice->logical_device, ZestRenderer->swapchain_frame_buffers[i], ZEST_NULL);
	}

	//vkFreeCommandBuffers(ZestDevice->logical_device, ZestRenderer->present_command_pool, 1, &ZestRenderer->final_render_pass.command_buffer);

	for (zest_foreach_i(ZestRenderer->swapchain_image_views)) {
		vkDestroyImageView(ZestDevice->logical_device, ZestRenderer->swapchain_image_views[i], ZEST_NULL);
	}

	vkDestroySwapchainKHR(ZestDevice->logical_device, ZestRenderer->swapchain, ZEST_NULL);
}

void zest__destroy_pipeline_set(zest_pipeline_set *p) {
	vkDestroyPipeline(ZestDevice->logical_device, p->pipeline, ZEST_NULL);
	vkDestroyPipelineLayout(ZestDevice->logical_device, p->pipeline_layout, ZEST_NULL);
}

void zest__cleanup_pipelines() {
	for (zest_map_foreach_i(ZestRenderer->pipeline_sets)) {
		zest__destroy_pipeline_set(&ZestRenderer->pipeline_sets.data[i]);
	}
}

void zest__cleanup_renderer() {
	zest__cleanup_swapchain();
	vkDestroyDescriptorPool(ZestDevice->logical_device, ZestRenderer->descriptor_pool, ZEST_NULL);

	zest__cleanup_pipelines();
	zest_map_clear(ZestRenderer->pipeline_sets);

	//CleanUpTextures();

	//for (auto& render_target : ZestRenderer->render_targets.data) {
		//render_target.CleanUp();
	//}
	//ZestRenderer->render_targets.Clear();

	for (zest_map_foreach_i(ZestRenderer->render_passes)) {
		vkDestroyRenderPass(ZestDevice->logical_device, ZestRenderer->render_passes.data[i].render_pass, ZEST_NULL);
	}
	zest_map_clear(ZestRenderer->render_passes);

	//for (auto& draw_routine : ZestRenderer->draw_routines.data) {
		//if (draw_routine.clean_up_callback) {
			//draw_routine.clean_up_callback(draw_routine);
		//}
	//}
	zest_map_clear(ZestRenderer->draw_routines);

	for (zest_map_foreach_i(ZestRenderer->descriptor_layouts)) {
		vkDestroyDescriptorSetLayout(ZestDevice->logical_device, ZestRenderer->descriptor_layouts.data[i].descriptor_layout, ZEST_NULL);
	}
	zest_map_clear(ZestRenderer->descriptor_layouts);


	for (ZEST_EACHFFIF_i) {
		vkDestroySemaphore(ZestDevice->logical_device, ZestRenderer->semaphores[i].present_complete, ZEST_NULL);
		vkDestroyFence(ZestDevice->logical_device, ZestRenderer->fif_fence[i], ZEST_NULL);
	}

	for (zest_map_foreach_i(ZestRenderer->buffer_allocators)) {
		for (zest_foreach_j(ZestRenderer->buffer_allocators.data[i].memory_pools)) {
			zest__destroy_memory(&ZestRenderer->buffer_allocators.data[i].memory_pools[j]);
		}
	}

	//for (auto &buffer : buffers) {
		//buffer.CleanUp();
	//}

	//for (auto &command_queue : ZestRenderer->command_queues.data) {
		//command_queue.CleanUp();
	//}
	zest__cleanup_command_queue(&ZestRenderer->empty_queue);

	vkDestroyCommandPool(ZestDevice->logical_device, ZestRenderer->present_command_pool, VK_NULL_HANDLE);
}

void zest__recreate_swapchain() {
	int width = 0, height = 0;
	while (width == 0 || height == 0) {
		glfwGetFramebufferSize(ZestApp->window->window_handle, &width, &height);
		if (width == 0 || height == 0) {
			glfwWaitEvents();
		}
	}

	zest__update_window_size(ZestApp->window, width, height);
	ZestRenderer->push_constants.screen_resolution.x = 1.f / width;
	ZestRenderer->push_constants.screen_resolution.y = 1.f / height;

	zest_WaitForIdleDevice();

	zest__cleanup_swapchain();
	zest__cleanup_pipelines();

	zest__create_swapchain();
	zest__create_swapchain_image_views();

	zest_FreeBuffer(ZestRenderer->depth_resource_buffer);
	ZestRenderer->depth_resource_buffer = zest__create_depth_resources();

	zest__create_frame_buffers();
	//for (auto& render_target : ZestRenderer->render_targets.data) {
		//render_target.RecreateRenderResources();
	//}
	VkExtent2D extent;
	extent.width = width;
	extent.height = height;
	for (zest_map_foreach_i(ZestRenderer->pipeline_sets)) {
		zest_pipeline_set *pipeline = &ZestRenderer->pipeline_sets.data[i];
		if (!pipeline->rebuild_pipeline_function) {
			zest__rebuild_pipeline(pipeline);
		}
		else {
			pipeline->rebuild_pipeline_function(pipeline);
		}
	}

	zest__create_final_render_command_buffer();
	ZestRenderer->update_uniform_buffer_callback(ZestRenderer->user_uniform_data);
	/*
	for (auto &compute : App.computers) {
		if (compute.is_active) {
			for (EachFrameInFlight) {
				compute.descriptor_update_callback(compute);
				//compute.command_buffer_update_callback(compute, compute.command_buffer[i]);
			}
		}
	}

	if (App.create_info.use_imgui) {
		ImGuiIO& io = ImGui::GetIO();
		io.DisplaySize = ImVec2(fScreenWidth(), fScreenHeight());
	}

	for (auto& draw_routine : ZestRenderer->draw_routines.data) {
		if (!draw_routine.update_resolution_callback && draw_routine.draw_index != -1) {
			QulkanLayer &layer = GetLayer(draw_routine.draw_index);
			layer.UpdateResolution();
		}
		else if (draw_routine.update_resolution_callback)
			draw_routine.update_resolution_callback(draw_routine);
	}

	for (auto &command_queue : ZestRenderer->command_queues.data) {
		for (auto &render_command_index : command_queue.render_commands) {
			CommandQueueDraw &render_command = ZestRenderer->command_queue_render_passes[render_command_index];
			if (render_command.viewport_type == RenderViewportType_scale_with_window) {
				render_command.viewport.extent.width = u32((float)width * render_command.viewport_scale.x);
				render_command.viewport.extent.height = u32((float)height * render_command.viewport_scale.y);
			}
		}
	}
	*/

	for (zest_foreach_i(ZestRenderer->empty_queue.render_commands)) {
		zest_index render_command_index = ZestRenderer->empty_queue.render_commands[i];
		zest_command_queue_draw *render_command = zest_map_at_index(ZestRenderer->command_queue_render_passes, render_command_index);
		render_command->viewport.extent.width = width;
		render_command->viewport.extent.height = height;
	}

}

void zest__create_swapchain_image_views() {
	zest_vec_resize(ZestRenderer->swapchain_image_views, zest_vec_size(ZestRenderer->swapchain_images));

	for (zest_foreach_i(ZestRenderer->swapchain_images)) {
		ZestRenderer->swapchain_image_views[i] = zest__create_image_view(ZestRenderer->swapchain_images[i], ZestRenderer->swapchain_image_format, VK_IMAGE_ASPECT_COLOR_BIT, 1, VK_IMAGE_VIEW_TYPE_2D_ARRAY, 1);
	}
}

void zest__make_standard_render_passes() {
	zest__add_render_pass("Render pass present", zest__create_render_pass(ZestRenderer->swapchain_image_format, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, VK_ATTACHMENT_LOAD_OP_CLEAR));
	zest__add_render_pass("Render pass standard", zest__create_render_pass(ZestRenderer->swapchain_image_format, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ATTACHMENT_LOAD_OP_CLEAR));
	zest__add_render_pass("Render pass standard no clear", zest__create_render_pass(ZestRenderer->swapchain_image_format, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ATTACHMENT_LOAD_OP_DONT_CARE));
}

zest_uint zest__add_render_pass(const char *name, VkRenderPass render_pass) {
	zest_render_pass r;
	r.render_pass = render_pass;
	r.name = name;
	zest_map_insert(ZestRenderer->render_passes, name, r);
	return zest_map_last_index(ZestRenderer->render_passes);
}

zest_buffer *zest__create_depth_resources() {
	VkFormat depth_format = zest__find_depth_format();

	zest_buffer *buffer = zest__create_image(ZestRenderer->swapchain_extent.width, ZestRenderer->swapchain_extent.height, 1, VK_SAMPLE_COUNT_1_BIT, depth_format, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &ZestRenderer->final_render_pass.depth.image);
	ZestRenderer->final_render_pass.depth.view = zest__create_image_view(ZestRenderer->final_render_pass.depth.image, depth_format, VK_IMAGE_ASPECT_DEPTH_BIT, 1, VK_IMAGE_VIEW_TYPE_2D_ARRAY, 1);

	zest__transition_image_layout(ZestRenderer->final_render_pass.depth.image, depth_format, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, 1, 1);

	return buffer;
}

void zest__create_frame_buffers() {
	zest_vec_resize(ZestRenderer->swapchain_frame_buffers, zest_vec_size(ZestRenderer->swapchain_image_views));

	for (zest_foreach_i(ZestRenderer->swapchain_image_views)) {
		VkFramebufferCreateInfo frame_buffer_info = { 0 };

			VkImageView attachments[2] = {
				ZestRenderer->swapchain_image_views[i],
				ZestRenderer->final_render_pass.depth.view
			};
			frame_buffer_info.attachmentCount = 2;
			frame_buffer_info.pAttachments = attachments;
			frame_buffer_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
			frame_buffer_info.renderPass = zest_GetRenderPassByIndex(ZestRenderer->final_render_pass.render_pass);
			frame_buffer_info.width = ZestRenderer->swapchain_extent.width;
			frame_buffer_info.height = ZestRenderer->swapchain_extent.height;
			frame_buffer_info.layers = 1;

			ZEST_VK_CHECK_RESULT(vkCreateFramebuffer(ZestDevice->logical_device, &frame_buffer_info, ZEST_NULL, &ZestRenderer->swapchain_frame_buffers[i]));

	}
}

void zest__create_sync_objects() {
	VkSemaphoreCreateInfo semaphore_info = { 0 };
	semaphore_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

	VkFenceCreateInfo fence_info = { 0 };
	fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fence_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;

	for (ZEST_EACHFFIF_i) {
		ZEST_VK_CHECK_RESULT(vkCreateSemaphore(ZestDevice->logical_device, &semaphore_info, ZEST_NULL, &ZestRenderer->semaphores[i].present_complete));
	}
}

zest_index zest_create_uniform_buffer(const char *name, zest_size uniform_struct_size) {
	zest_uniform_buffer uniform_buffer;
	zest_buffer_info buffer_info;
	buffer_info.usage_flags = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
	buffer_info.property_flags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
	for (ZEST_EACHFFIF_i) {
		uniform_buffer.buffer[i] = zest_CreateBuffer(uniform_struct_size, &buffer_info, ZEST_NULL, tloc__MEGABYTE(2));
		uniform_buffer.view_buffer_info[i].offset = 0;
		uniform_buffer.view_buffer_info[i].range = uniform_struct_size;
	}
	return zest_add_uniform_buffer(name, &uniform_buffer);
}

void zest_update_uniform_buffer(void *user_uniform_data) {
	if (ZestRenderer->flags & zest_renderer_flag_disable_default_uniform_update) {
		return;
	}
	zest_uniform_buffer *uniform_buffer = zest_GetUniformBuffer(ZestRenderer->standard_uniform_buffer_id);
	zest_uniform_buffer_data *ubo_ptr = (zest_uniform_buffer_data*)uniform_buffer->buffer[ZEST_FIF]->data;
	zest_vec3 eye = { .x = 0.f, .y = 0.f, .z = -1 };
	zest_vec3 center = { 0 };
	zest_vec3 up = { .x = 0.f, .y = -1.f, .z = 0.f };
	ubo_ptr->view = zest_LookAt(eye, center, up);
	ubo_ptr->proj = zest_Ortho(0.f, (float)ZestRenderer->swapchain_extent.width, 0.f, -(float)ZestRenderer->swapchain_extent.height, -1000.f, 1000.f);
	ubo_ptr->screen_size.x = zest_ScreenWidthf();
	ubo_ptr->screen_size.y = zest_ScreenHeightf();
	ubo_ptr->millisecs = zest_Millisecs();
}

zest_index zest_add_uniform_buffer(const char *name, zest_uniform_buffer *buffer) { 
	zest_map_insert(ZestRenderer->uniform_buffers, name, *buffer); 
	return zest_map_get_index_by_name(ZestRenderer->uniform_buffers, name);
}

void zest__create_renderer_command_pools() {
	VkCommandPoolCreateInfo cmdPoolInfo = { 0 };
	cmdPoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	cmdPoolInfo.queueFamilyIndex = ZestDevice->graphics_queue_family_index;
	cmdPoolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
	ZEST_VK_CHECK_RESULT(vkCreateCommandPool(ZestDevice->logical_device, &cmdPoolInfo, ZEST_NULL, &ZestRenderer->present_command_pool));
}

void zest__create_descriptor_pools(VkDescriptorPoolSize *pool_sizes) {
	if (zest_vec_empty(pool_sizes)) {
		VkDescriptorPoolSize pool_size;
		pool_size.type = VK_DESCRIPTOR_TYPE_SAMPLER;
		pool_size.descriptorCount = 10;
		zest_vec_push(pool_sizes, pool_size);
		pool_size.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		pool_size.descriptorCount = 100;
		zest_vec_push(pool_sizes, pool_size);
		pool_size.type = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
		pool_size.descriptorCount = 10;
		zest_vec_push(pool_sizes, pool_size);
		pool_size.type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
		pool_size.descriptorCount = 10;
		zest_vec_push(pool_sizes, pool_size);
		pool_size.type = VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER;
		pool_size.descriptorCount = 10;
		zest_vec_push(pool_sizes, pool_size);
		pool_size.type = VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER;
		pool_size.descriptorCount = 10;
		zest_vec_push(pool_sizes, pool_size);
		pool_size.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		pool_size.descriptorCount = 100;
		zest_vec_push(pool_sizes, pool_size);
		pool_size.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		pool_size.descriptorCount = 10;
		zest_vec_push(pool_sizes, pool_size);
		pool_size.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
		pool_size.descriptorCount = 10;
		zest_vec_push(pool_sizes, pool_size);
		pool_size.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC;
		pool_size.descriptorCount = 10;
		zest_vec_push(pool_sizes, pool_size);
		pool_size.type = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
		pool_size.descriptorCount = 10;
		zest_vec_push(pool_sizes, pool_size);
	}

	VkDescriptorPoolCreateInfo pool_info = { 0 };
	pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	pool_info.poolSizeCount = (zest_uint)(zest_vec_size(pool_sizes));
	pool_info.pPoolSizes = pool_sizes;
	pool_info.maxSets = 100;

	ZEST_VK_CHECK_RESULT(vkCreateDescriptorPool(ZestDevice->logical_device, &pool_info, ZEST_NULL, &ZestRenderer->descriptor_pool));
}

void zest__make_standard_descriptor_layouts() {
	zest_AddDescriptorLayout("Standard 1 uniform 1 sampler", zest_CreateDescriptorSetLayout(1, 1, 0));
	zest_AddDescriptorLayout("Polygon layout (no sampler)", zest_CreateDescriptorSetLayout(1, 0, 0));
	zest_AddDescriptorLayout("Render target layout", zest_CreateDescriptorSetLayout(0, 1, 0));
	zest_AddDescriptorLayout("Ribbon 2d layout", zest_CreateDescriptorSetLayout(1, 0, 1));
}

zest_index zest_AddDescriptorLayout(const char *name, VkDescriptorSetLayout layout) {
	zest_descriptor_set_layout l;
	l.name = name;
	l.descriptor_layout = layout;
	zest_map_insert(ZestRenderer->descriptor_layouts, name, l);
	return zest_map_last_index(ZestRenderer->descriptor_layouts);
}

VkDescriptorSetLayout zest_CreateDescriptorSetLayout(zest_uint uniforms, zest_uint samplers, zest_uint storage_buffers) {
	VkDescriptorSetLayoutBinding *bindings = 0;

	for (int c = 0; c != uniforms; ++c) {
		zest_vec_push(bindings, zest_CreateUniformLayoutBinding(c));
	}

	for (int c = 0; c != samplers; ++c) {
		zest_vec_push(bindings, zest_CreateSamplerLayoutBinding(c + uniforms));
	}

	for (int c = 0; c != storage_buffers; ++c) {
		zest_vec_push(bindings, zest_CreateStorageLayoutBinding(c + samplers + uniforms));
	}

	VkDescriptorSetLayout layout = zest_CreateDescriptorSetLayoutWithBindings(bindings);
	zest_vec_free(bindings);
	return layout;
}

VkDescriptorSetLayout zest_CreateDescriptorSetLayoutWithBindings(VkDescriptorSetLayoutBinding *bindings) {
	ZEST_ASSERT(bindings);	//must have bindings to create the layout

	VkDescriptorSetLayoutCreateInfo layoutInfo = { 0 };
	layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	layoutInfo.bindingCount = (zest_uint)zest_vec_size(bindings);
	layoutInfo.pBindings = bindings;

	VkDescriptorSetLayout layout;

	ZEST_VK_CHECK_RESULT(vkCreateDescriptorSetLayout(ZestDevice->logical_device, &layoutInfo, ZEST_NULL, &layout) != VK_SUCCESS);

	return layout;
}

VkDescriptorSetLayoutBinding zest_CreateUniformLayoutBinding(zest_uint binding) {

	VkDescriptorSetLayoutBinding viewLayoutBinding = { 0 };
	viewLayoutBinding.binding = binding;
	viewLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	viewLayoutBinding.descriptorCount = 1;
	viewLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
	viewLayoutBinding.pImmutableSamplers = ZEST_NULL; 

	return viewLayoutBinding;
}

VkDescriptorSetLayoutBinding zest_CreateSamplerLayoutBinding(zest_uint binding) {

	VkDescriptorSetLayoutBinding samplerLayoutBinding = { 0 };
	samplerLayoutBinding.binding = binding;
	samplerLayoutBinding.descriptorCount = 1;
	samplerLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	samplerLayoutBinding.pImmutableSamplers = ZEST_NULL;
	samplerLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

	return samplerLayoutBinding;
}

VkDescriptorSetLayoutBinding zest_CreateStorageLayoutBinding(zest_uint binding) {

	VkDescriptorSetLayoutBinding storage_layout_binding = { 0 };
	storage_layout_binding.binding = binding;
	storage_layout_binding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	storage_layout_binding.descriptorCount = 1;
	storage_layout_binding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
	storage_layout_binding.pImmutableSamplers = ZEST_NULL;

	return storage_layout_binding;

}

zest_pipeline_set zest_CreatePipelineSet() {
	zest_pipeline_set pipeline_set = {
		.create_info = {0},
		.pipeline_template = {0},
		.descriptor_layout = {0},
		.pipeline = VK_NULL_HANDLE,
		.pipeline_layout = VK_NULL_HANDLE,
		.uniforms = 1,
		.push_constant_size = 0,
		.textures = 0,
		.rebuild_pipeline_function = ZEST_NULL,
		.flags = zest_pipeline_set_flag_none,
	};
	return pipeline_set;
}

zest_pipeline_template_create_info zest_CreatePipelineTemplateCreateInfo(void) {
	zest_pipeline_template_create_info create_info = {
		.vertShaderFunctionName = "main",
		.fragShaderFunctionName = "main",
		.descriptorSetLayout = {0},
		.no_vertex_input = ZEST_FALSE,
		.attributeDescriptions = 0,
		.bindingDescription = 0,
		.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST
	};
	zest_vec_push(create_info.dynamicStates, VK_DYNAMIC_STATE_VIEWPORT);
	zest_vec_push(create_info.dynamicStates, VK_DYNAMIC_STATE_SCISSOR);
	create_info.viewport.offset.x = 0;
	create_info.viewport.offset.y = 0;
	return create_info;
}

VkPipelineColorBlendAttachmentState zest_AdditiveBlendState() {
	VkPipelineColorBlendAttachmentState color_blend_attachment;
	color_blend_attachment.blendEnable = VK_TRUE;
	color_blend_attachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	color_blend_attachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
	color_blend_attachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE;
	color_blend_attachment.colorBlendOp = VK_BLEND_OP_ADD;
	color_blend_attachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
	color_blend_attachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
	color_blend_attachment.alphaBlendOp = VK_BLEND_OP_ADD;
	return color_blend_attachment;
}

VkPipelineColorBlendAttachmentState zest_AlphaOnlyBlendState() {
	VkPipelineColorBlendAttachmentState color_blend_attachment;
	color_blend_attachment.blendEnable = VK_TRUE;
	color_blend_attachment.colorWriteMask = VK_COLOR_COMPONENT_A_BIT;
	color_blend_attachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
	color_blend_attachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE;
	color_blend_attachment.colorBlendOp = VK_BLEND_OP_ADD;
	color_blend_attachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
	color_blend_attachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
	color_blend_attachment.alphaBlendOp = VK_BLEND_OP_ADD;
	return color_blend_attachment;
}

VkPipelineColorBlendAttachmentState zest_AlphaBlendState() {
	VkPipelineColorBlendAttachmentState color_blend_attachment;
	color_blend_attachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	color_blend_attachment.blendEnable = VK_TRUE;
	color_blend_attachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
	color_blend_attachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
	color_blend_attachment.colorBlendOp = VK_BLEND_OP_ADD;
	color_blend_attachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
	color_blend_attachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
	color_blend_attachment.alphaBlendOp = VK_BLEND_OP_ADD;
	return color_blend_attachment;
}

VkPipelineColorBlendAttachmentState zest_PreMultiplyBlendState() {
	VkPipelineColorBlendAttachmentState color_blend_attachment;
	color_blend_attachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	color_blend_attachment.blendEnable = VK_TRUE;
	color_blend_attachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
	color_blend_attachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
	color_blend_attachment.colorBlendOp = VK_BLEND_OP_ADD;
	color_blend_attachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
	color_blend_attachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
	color_blend_attachment.alphaBlendOp = VK_BLEND_OP_ADD;
	return color_blend_attachment;
}

VkPipelineColorBlendAttachmentState zest_PreMultiplyBlendStateForSwap() {
	VkPipelineColorBlendAttachmentState color_blend_attachment;
	color_blend_attachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	color_blend_attachment.blendEnable = VK_TRUE;
	color_blend_attachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
	color_blend_attachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
	color_blend_attachment.colorBlendOp = VK_BLEND_OP_ADD;
	color_blend_attachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
	color_blend_attachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
	color_blend_attachment.alphaBlendOp = VK_BLEND_OP_ADD;
	return color_blend_attachment;
}

VkPipelineColorBlendAttachmentState zest_MaxAlphaBlendState() {
	VkPipelineColorBlendAttachmentState color_blend_attachment;
	color_blend_attachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	color_blend_attachment.blendEnable = VK_TRUE;
	color_blend_attachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
	color_blend_attachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA;
	color_blend_attachment.colorBlendOp = VK_BLEND_OP_ADD;
	color_blend_attachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
	color_blend_attachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
	color_blend_attachment.alphaBlendOp = VK_BLEND_OP_ADD;
	return color_blend_attachment;
}

VkPipelineColorBlendAttachmentState zest_ImGuiBlendState() {
	VkPipelineColorBlendAttachmentState color_blend_attachment;
	color_blend_attachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	color_blend_attachment.blendEnable = VK_TRUE;
	color_blend_attachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
	color_blend_attachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
	color_blend_attachment.colorBlendOp = VK_BLEND_OP_ADD;
	color_blend_attachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
	color_blend_attachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
	color_blend_attachment.alphaBlendOp = VK_BLEND_OP_ADD;
	return color_blend_attachment;
}

void zest_SetPipelineTemplate(zest_pipeline_template *pipeline_template, zest_pipeline_template_create_info *create_info) {
	pipeline_template->vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

	pipeline_template->vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	pipeline_template->vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
	pipeline_template->vertShaderStageInfo.pName = create_info->vertShaderFunctionName;

	pipeline_template->fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	pipeline_template->fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	pipeline_template->fragShaderStageInfo.pName = create_info->fragShaderFunctionName;

	pipeline_template->vertShaderFile = create_info->vertShaderFile;
	pipeline_template->fragShaderFile = create_info->fragShaderFile;

	pipeline_template->inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	pipeline_template->inputAssembly.topology = create_info->topology;
	pipeline_template->inputAssembly.primitiveRestartEnable = VK_FALSE;

	if (!create_info->no_vertex_input) {
		if (zest_vec_empty(create_info->bindingDescriptions)) {
			pipeline_template->vertexInputInfo.vertexBindingDescriptionCount = 1;
			pipeline_template->vertexInputInfo.pVertexBindingDescriptions = &create_info->bindingDescription;
		}
		else {
			pipeline_template->vertexInputInfo.vertexBindingDescriptionCount = (zest_uint)zest_vec_size(create_info->bindingDescriptions);
			pipeline_template->vertexInputInfo.pVertexBindingDescriptions = create_info->bindingDescriptions;
		}
		pipeline_template->vertexInputInfo.vertexAttributeDescriptionCount = (zest_uint)zest_vec_size(create_info->attributeDescriptions);
		pipeline_template->vertexInputInfo.pVertexAttributeDescriptions = create_info->attributeDescriptions;
	}

	pipeline_template->viewport.x = 0.0f;
	pipeline_template->viewport.y = 0.0f;
	pipeline_template->viewport.width = (float)create_info->viewport.extent.width;
	pipeline_template->viewport.height = (float)create_info->viewport.extent.height;
	pipeline_template->viewport.minDepth = 0.0f;
	pipeline_template->viewport.maxDepth = 1.0f;

	pipeline_template->scissor = create_info->viewport;

	pipeline_template->viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	pipeline_template->viewportState.viewportCount = 1;
	pipeline_template->viewportState.pViewports = &pipeline_template->viewport;
	pipeline_template->viewportState.scissorCount = 1;
	pipeline_template->viewportState.pScissors = &pipeline_template->scissor;

	pipeline_template->rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	pipeline_template->rasterizer.depthClampEnable = VK_FALSE;
	pipeline_template->rasterizer.rasterizerDiscardEnable = VK_FALSE;
	pipeline_template->rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
	pipeline_template->rasterizer.lineWidth = 1.0f;
	pipeline_template->rasterizer.cullMode = VK_CULL_MODE_NONE;
	pipeline_template->rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;
	pipeline_template->rasterizer.depthBiasEnable = VK_FALSE;

	pipeline_template->multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	pipeline_template->multisampling.sampleShadingEnable = VK_FALSE;
	pipeline_template->multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

	pipeline_template->depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	pipeline_template->depthStencil.depthTestEnable = VK_TRUE;
	pipeline_template->depthStencil.depthWriteEnable = VK_TRUE;
	pipeline_template->depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;
	pipeline_template->depthStencil.depthBoundsTestEnable = VK_FALSE;
	pipeline_template->depthStencil.stencilTestEnable = VK_FALSE;

	pipeline_template->colorBlendAttachment = zest_AlphaBlendState();

	pipeline_template->colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	pipeline_template->colorBlending.logicOpEnable = VK_FALSE;
	pipeline_template->colorBlending.logicOp = VK_LOGIC_OP_COPY;
	pipeline_template->colorBlending.attachmentCount = 1;
	pipeline_template->colorBlending.pAttachments = &pipeline_template->colorBlendAttachment;
	pipeline_template->colorBlending.blendConstants[0] = 0.0f;
	pipeline_template->colorBlending.blendConstants[1] = 0.0f;
	pipeline_template->colorBlending.blendConstants[2] = 0.0f;
	pipeline_template->colorBlending.blendConstants[3] = 0.0f;

	pipeline_template->pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipeline_template->pipelineLayoutInfo.setLayoutCount = 1;
	pipeline_template->pipelineLayoutInfo.pSetLayouts = zest_GetDescriptorSetLayoutByIndex(create_info->descriptorSetLayout);

	if (!zest_vec_empty(create_info->pushConstantRange)) {
		pipeline_template->pipelineLayoutInfo.pPushConstantRanges = create_info->pushConstantRange;
		pipeline_template->pipelineLayoutInfo.pushConstantRangeCount = zest_vec_size(create_info->pushConstantRange);
	}

	pipeline_template->renderPass = create_info->renderPass;

	pipeline_template->dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	pipeline_template->dynamicState.dynamicStateCount = (zest_uint)(zest_vec_size(create_info->dynamicStates));
	pipeline_template->dynamicState.pDynamicStates = create_info->dynamicStates;
	pipeline_template->dynamicState.flags = 0;
}

ZEST_API void zest_BuildPipeline(zest_pipeline_set *pipeline) {
	ZEST_VK_CHECK_RESULT(vkCreatePipelineLayout(ZestDevice->logical_device, &pipeline->pipeline_template.pipelineLayoutInfo, ZEST_NULL, &pipeline->pipeline_layout));

	if (!pipeline->pipeline_template.vertShaderFile || !pipeline->pipeline_template.fragShaderFile) {
		ZEST_ASSERT(0);		//You must specify a vertex and frag shader file to load
	}
	char *vert_shader_code = zest_ReadEntireFile(pipeline->pipeline_template.vertShaderFile, ZEST_FALSE);
	char *frag_shader_code = zest_ReadEntireFile(pipeline->pipeline_template.fragShaderFile, ZEST_FALSE);

	ZEST_ASSERT(vert_shader_code);		//Failed to load the shader file, make sure it exists at the location
	ZEST_ASSERT(frag_shader_code);		//Failed to load the shader file, make sure it exists at the location

	VkShaderModule vert_shader_module = zest_CreateShaderModule(vert_shader_code);
	VkShaderModule frag_shader_module = zest_CreateShaderModule(frag_shader_code);

	zest_vec_free(vert_shader_code);
	zest_vec_free(frag_shader_code);

	pipeline->pipeline_template.vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	pipeline->pipeline_template.vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
	pipeline->pipeline_template.vertShaderStageInfo.module = vert_shader_module;
	pipeline->pipeline_template.vertShaderStageInfo.pName = pipeline->create_info.vertShaderFunctionName;

	pipeline->pipeline_template.fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	pipeline->pipeline_template.fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	pipeline->pipeline_template.fragShaderStageInfo.module = frag_shader_module;
	pipeline->pipeline_template.fragShaderStageInfo.pName = pipeline->create_info.fragShaderFunctionName;

	VkPipelineShaderStageCreateInfo shaderStages[2] = { pipeline->pipeline_template.vertShaderStageInfo, pipeline->pipeline_template.fragShaderStageInfo };

	VkGraphicsPipelineCreateInfo pipeline_info = { 0 };
	pipeline_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	pipeline_info.stageCount = 2;
	pipeline_info.pStages = shaderStages;
	pipeline_info.pVertexInputState = &pipeline->pipeline_template.vertexInputInfo;
	pipeline_info.pInputAssemblyState = &pipeline->pipeline_template.inputAssembly;
	pipeline_info.pViewportState = &pipeline->pipeline_template.viewportState;
	pipeline_info.pRasterizationState = &pipeline->pipeline_template.rasterizer;
	pipeline_info.pMultisampleState = &pipeline->pipeline_template.multisampling;
	pipeline_info.pColorBlendState = &pipeline->pipeline_template.colorBlending;
	pipeline_info.pDepthStencilState = &pipeline->pipeline_template.depthStencil;
	pipeline_info.layout = pipeline->pipeline_layout;
	pipeline_info.renderPass = pipeline->pipeline_template.renderPass;
	pipeline_info.subpass = 0;
	pipeline_info.basePipelineHandle = VK_NULL_HANDLE;
	if (pipeline->pipeline_template.dynamicState.dynamicStateCount) {
		pipeline_info.pDynamicState = &pipeline->pipeline_template.dynamicState;
	}

	ZEST_VK_CHECK_RESULT(vkCreateGraphicsPipelines(ZestDevice->logical_device, VK_NULL_HANDLE, 1, &pipeline_info, ZEST_NULL, &pipeline->pipeline));

	vkDestroyShaderModule(ZestDevice->logical_device, frag_shader_module, ZEST_NULL);
	vkDestroyShaderModule(ZestDevice->logical_device, vert_shader_module, ZEST_NULL);
}

void zest_MakePipelineTemplate(zest_pipeline_set *pipeline, VkRenderPass render_pass, zest_pipeline_template_create_info *create_info) {
	ZEST_ASSERT(zest_map_valid_index(ZestRenderer->descriptor_layouts, pipeline->descriptor_layout));	//Must be a valid descriptor layout index in the pipeline

	if (!pipeline->flags & zest_pipeline_set_flag_is_render_target_pipeline)
		create_info->renderPass = render_pass;
	pipeline->create_info.viewport.extent = zest_GetSwapChainExtent();

	zest_SetPipelineTemplate(&pipeline->pipeline_template, create_info);
	pipeline->pipeline_template.multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

	if (&pipeline->create_info != create_info)
		pipeline->create_info = *create_info;
}

VkShaderModule zest_CreateShaderModule(char *code) {
	VkShaderModuleCreateInfo create_info = { 0 };
	create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	create_info.codeSize = zest_vec_size(code);
	create_info.pCode = (zest_uint*)(code);

	VkShaderModule shader_module;
	ZEST_VK_CHECK_RESULT(vkCreateShaderModule(ZestDevice->logical_device, &create_info, ZEST_NULL, &shader_module));

	return shader_module;
}

void zest__rebuild_pipeline(zest_pipeline_set *pipeline) {
	//Note: not setting this for pipelines messes with scaling, but not sure if some pipelines need this to be fixed
	pipeline->create_info.viewport.extent = zest_GetSwapChainExtent();
	pipeline->pipeline_template.renderPass = zest_GetStandardRenderPass();
	zest_UpdatePipelineTemplate(&pipeline->pipeline_template, &pipeline->create_info);
	zest_BuildPipeline(pipeline);
	for (ZEST_EACHFFIF_i) {
		zest_vec_clear(pipeline->descriptor_writes);
	}
}

void zest__prepare_standard_pipelines() {
	VkRenderPass render_pass = zest_GetStandardRenderPass();

	//Final Render Pipelines
	zest_uint final_render_index = zest_AddPipeline("pipeline_final_render");
	zest_pipeline_set *final_render = zest_PipelineByIndex(final_render_index);

	final_render->create_info = zest_CreatePipelineTemplateCreateInfo();
	final_render->create_info.viewport.extent = zest_GetSwapChainExtent();
	VkPushConstantRange final_render_pushconstant_range;
	final_render_pushconstant_range.offset = 0;
	final_render_pushconstant_range.size = sizeof(zest_final_render_push_constants);
	final_render_pushconstant_range.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
	zest_vec_push(final_render->create_info.pushConstantRange, final_render_pushconstant_range);
	final_render->create_info.renderPass = zest_GetRenderPassByIndex(ZestRenderer->final_render_pass.render_pass);
	final_render->create_info.no_vertex_input = ZEST_TRUE;
	final_render->create_info.vertShaderFile = "spv/swap.spv";
	final_render->create_info.fragShaderFile = "spv/swap.spv";
	final_render->uniforms = 0;
	final_render->flags = zest_pipeline_set_flag_is_render_target_pipeline;
	final_render->descriptor_layout = zest_map_get_index_by_name(ZestRenderer->descriptor_layouts, "Standard 1 uniform 1 sampler");
	final_render->create_info.descriptorSetLayout = final_render->descriptor_layout;

	zest_MakePipelineTemplate(final_render, render_pass, &final_render->create_info);
	final_render->pipeline_template.depthStencil.depthWriteEnable = VK_FALSE;
	final_render->pipeline_template.depthStencil.depthTestEnable = VK_FALSE;

	final_render->pipeline_template.colorBlendAttachment = zest_PreMultiplyBlendStateForSwap();

	zest_BuildPipeline(final_render);
}

void zest_UpdatePipelineTemplate(zest_pipeline_template *pipeline_template, zest_pipeline_template_create_info *create_info) {
	pipeline_template->viewport.width = (float)create_info->viewport.extent.width;
	pipeline_template->viewport.height = (float)create_info->viewport.extent.height;
	pipeline_template->scissor.extent.width = create_info->viewport.extent.width;
	pipeline_template->scissor.extent.height = create_info->viewport.extent.height;

	pipeline_template->vertShaderStageInfo.pName = create_info->vertShaderFunctionName;
	pipeline_template->fragShaderStageInfo.pName = create_info->fragShaderFunctionName;

	if (!create_info->no_vertex_input) {
		if (zest_vec_empty(create_info->bindingDescriptions)) {
			pipeline_template->vertexInputInfo.vertexBindingDescriptionCount = 1;
			pipeline_template->vertexInputInfo.pVertexBindingDescriptions = &create_info->bindingDescription;
		}
		else {
			pipeline_template->vertexInputInfo.vertexBindingDescriptionCount = zest_vec_size(create_info->bindingDescriptions);
			pipeline_template->vertexInputInfo.pVertexBindingDescriptions = create_info->bindingDescriptions;
		}
		pipeline_template->vertexInputInfo.pVertexAttributeDescriptions = create_info->attributeDescriptions;
	}
	pipeline_template->viewportState.pViewports = &pipeline_template->viewport;
	pipeline_template->viewportState.pScissors = &pipeline_template->scissor;
	pipeline_template->colorBlending.pAttachments = &pipeline_template->colorBlendAttachment;
	pipeline_template->pipelineLayoutInfo.pSetLayouts = zest_GetDescriptorSetLayoutByIndex(create_info->descriptorSetLayout);
	pipeline_template->dynamicState.pDynamicStates = create_info->dynamicStates;

	if (!zest_vec_empty(create_info->pushConstantRange)) {
		pipeline_template->pipelineLayoutInfo.pPushConstantRanges = create_info->pushConstantRange;
		pipeline_template->pipelineLayoutInfo.pushConstantRangeCount = zest_vec_size(create_info->pushConstantRange);
	}
}

void zest__create_final_render_command_buffer() {
	zest_vec_resize(ZestRenderer->present_command_buffers, zest_vec_size(ZestRenderer->swapchain_frame_buffers));

	VkCommandBufferAllocateInfo alloc_info = { 0 };
	alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	alloc_info.commandPool = ZestRenderer->present_command_pool;
	alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	alloc_info.commandBufferCount = zest_vec_size(ZestRenderer->present_command_buffers);

	ZEST_VK_CHECK_RESULT(vkAllocateCommandBuffers(ZestDevice->logical_device, &alloc_info, ZestRenderer->present_command_buffers))

	VkClearValue clear_values[2] = {
		[0].color = { 0 },
		[1].depthStencil = { .depth = 1.0f, .stencil = 0 }
	};

	for (zest_foreach_i(ZestRenderer->present_command_buffers)) {
		VkCommandBufferBeginInfo begin_info = { 0 };
		begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		begin_info.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;
		begin_info.pInheritanceInfo = ZEST_NULL; // Optional

		ZEST_VK_CHECK_RESULT(vkBeginCommandBuffer(ZestRenderer->present_command_buffers[i], &begin_info));

		VkRenderPassBeginInfo render_pass_info = { 0 };
		render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		render_pass_info.renderPass = zest_GetRenderPassByIndex(ZestRenderer->final_render_pass.render_pass);
		render_pass_info.framebuffer = ZestRenderer->swapchain_frame_buffers[i];
		render_pass_info.renderArea.offset.x = 0;
		render_pass_info.renderArea.offset.y = 0;
		render_pass_info.renderArea.extent = ZestRenderer->swapchain_extent;

		render_pass_info.clearValueCount = 2;
		render_pass_info.pClearValues = clear_values;

		vkCmdBeginRenderPass(ZestRenderer->present_command_buffers[i], &render_pass_info, VK_SUBPASS_CONTENTS_INLINE);

		VkViewport view = zest_CreateViewport((float)ZestRenderer->swapchain_extent.width, (float)ZestRenderer->swapchain_extent.height, 0.f, 1.f);
		VkRect2D scissor = zest_CreateRect2D(ZestRenderer->swapchain_extent.width, ZestRenderer->swapchain_extent.height, 0, 1);
		vkCmdSetViewport(ZestRenderer->present_command_buffers[i], 0, 1, &view);
		vkCmdSetScissor(ZestRenderer->present_command_buffers[i], 0, 1, &scissor);

		VkDeviceSize offsets[] = { 0 };

		vkCmdEndRenderPass(ZestRenderer->present_command_buffers[i]);

		ZEST_VK_CHECK_RESULT(vkEndCommandBuffer(ZestRenderer->present_command_buffers[i]));
	}
}

void zest__rerecord_final_render_command_buffer() {
	VkClearValue clear_values[2] = {
		[0].color = { 0 },
		[1].depthStencil = { .depth = 1.0f, .stencil = 0 }
	};

	for (zest_foreach_i(ZestRenderer->present_command_buffers)) {

		VkCommandBufferBeginInfo begin_info = { 0 };
		begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		begin_info.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;
		begin_info.pInheritanceInfo = ZEST_NULL; // Optional

		ZEST_VK_CHECK_RESULT(vkBeginCommandBuffer(ZestRenderer->present_command_buffers[i], &begin_info));

		VkRenderPassBeginInfo render_pass_info = { 0 };
		render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		render_pass_info.renderPass = zest_GetRenderPassByIndex(ZestRenderer->final_render_pass.render_pass);
		render_pass_info.framebuffer = ZestRenderer->swapchain_frame_buffers[i];
		render_pass_info.renderArea.offset.x = 0;
		render_pass_info.renderArea.offset.y = 0;
		render_pass_info.renderArea.extent = ZestRenderer->swapchain_extent;

		render_pass_info.clearValueCount = 2;
		render_pass_info.pClearValues = clear_values;

		vkCmdBeginRenderPass(ZestRenderer->present_command_buffers[i], &render_pass_info, VK_SUBPASS_CONTENTS_INLINE);

		VkViewport view = zest_CreateViewport((float)ZestRenderer->swapchain_extent.width, (float)ZestRenderer->swapchain_extent.height, 0.f, 1.f);
		VkRect2D scissor = zest_CreateRect2D(ZestRenderer->swapchain_extent.width, ZestRenderer->swapchain_extent.height, 0, 0);
		vkCmdSetViewport(ZestRenderer->present_command_buffers[i], 0, 1, &view);
		vkCmdSetScissor(ZestRenderer->present_command_buffers[i], 0, 1, &scissor);

		VkDeviceSize offsets[] = { 0 };

		vkCmdEndRenderPass(ZestRenderer->present_command_buffers[i]);

		ZEST_VK_CHECK_RESULT(vkEndCommandBuffer(ZestRenderer->present_command_buffers[i]));
	}
}

void zest__create_empty_command_queue(zest_command_queue *command_queue) {
	VkCommandPoolCreateInfo cmd_pool_info = { 0 };
	cmd_pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	cmd_pool_info.queueFamilyIndex = ZestDevice->graphics_queue_family_index;
	cmd_pool_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
	ZEST_VK_CHECK_RESULT(vkCreateCommandPool(ZestDevice->logical_device, &cmd_pool_info, ZEST_NULL, &command_queue->command_pool));

	VkCommandBufferAllocateInfo alloc_info = { 0 };
	alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	alloc_info.commandBufferCount = ZEST_MAX_FIF;
	alloc_info.commandPool = command_queue->command_pool;
	ZEST_VK_CHECK_RESULT(vkAllocateCommandBuffers(ZestDevice->logical_device, &alloc_info, command_queue->command_buffer));

	for (ZEST_EACHFFIF_i) {
		zest_vec_push(command_queue->fif_incoming_semaphores[i], ZestRenderer->semaphores[i].present_complete);
		zest_vec_push(command_queue->fif_stage_flags[i], VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
	}
	zest_command_queue_draw render_commands;
	render_commands.render_pass = ZestRenderer->final_render_pass.render_pass;
	render_commands.get_frame_buffer = zest_GetRendererFrameBuffer;
	render_commands.render_pass_function = zest__render_blank;
	render_commands.viewport.extent = zest_GetSwapChainExtent();
	render_commands.viewport.offset.x = render_commands.viewport.offset.y = 0;
	zest_ConnectCommandQueueToPresent(command_queue, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
	zest_map_insert(ZestRenderer->command_queue_render_passes, "Blank Render Pass", render_commands);
	zest_vec_push(command_queue->render_commands, zest_map_last_index(ZestRenderer->command_queue_render_passes));
}

void zest__render_blank(zest_command_queue_draw *item, VkCommandBuffer command_buffer, zest_index render_pass, VkFramebuffer framebuffer) {
	VkRenderPassBeginInfo render_pass_info = { 0 };
	render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	render_pass_info.renderPass = zest_GetRenderPassByIndex(render_pass);
	render_pass_info.framebuffer = framebuffer;
	render_pass_info.renderArea = item->viewport;

	VkClearValue clear_values[2] = {
		[0].color = { 0 },
		[1].depthStencil = {.depth = 1.0f, .stencil = 0 }
	};

	render_pass_info.clearValueCount = 2;
	render_pass_info.pClearValues = clear_values;

	vkCmdBeginRenderPass(command_buffer, &render_pass_info, VK_SUBPASS_CONTENTS_INLINE);
	vkCmdEndRenderPass(command_buffer);
}

void zest__add_push_constant(zest_pipeline_template_create_info *create_info, VkPushConstantRange push_constant) {
	zest_vec_push(create_info->pushConstantRange, push_constant);
}

void zest__add_draw_routine(zest_command_queue_draw *command_queue_draw, zest_draw_routine *draw_routine) {
	zest_vec_push(command_queue_draw->draw_routines, draw_routine->routine_id);
	draw_routine->cq_render_pass_index = zest_vec_size(command_queue_draw->draw_routines) - 1;
}

void zest__acquire_next_swapchain_image() {

	VkResult result = vkAcquireNextImageKHR(ZestDevice->logical_device, ZestRenderer->swapchain, UINT64_MAX, ZestRenderer->semaphores[ZEST_FIF].present_complete, ZEST_NULL, &ZestRenderer->current_frame);

	//Has the window been resized? if so rebuild the swap chain.
	if (result == VK_ERROR_OUT_OF_DATE_KHR) {
		zest__recreate_swapchain();
		return;
	}
	else {
		ZEST_VK_CHECK_RESULT(result);
	}
}

void zest__draw_renderer_frame() {

	ZestRenderer->flags |= zest_renderer_flag_drawing_loop_running;

	zest__acquire_next_swapchain_image();
	ZestRenderer->update_uniform_buffer_callback(ZestRenderer->user_uniform_data);

	if (zest_vec_empty(ZestRenderer->frame_queues)) {
		//if there's no render queues at all, then we can draw this blank one to prevent errors when presenting the frame
		ZestRenderer->semaphores[ZestDevice->current_fif].render_complete = zest__get_command_queue_present_semaphore(&ZestRenderer->empty_queue);
		zest__record_and_commit_command_queue(&ZestRenderer->empty_queue, ZestRenderer->fif_fence[ZEST_FIF]);
		//FlushLayers();
	}
	else {
		for (zest_foreach_i(ZestRenderer->frame_queues)) {
			zest_index command_queue_index = ZestRenderer->frame_queues[i];
			zest__record_and_commit_command_queue(zest_map_at_index(ZestRenderer->command_queues, command_queue_index), ZestRenderer->fif_fence[ZEST_FIF]);
		}
	}
	zest_vec_clear(ZestRenderer->frame_queues);

	zest__present_frame();
}

void zest__present_frame() {
	VkPresentInfoKHR presentInfo = { 0 };
	presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

	presentInfo.waitSemaphoreCount = 1;
	presentInfo.pWaitSemaphores = &ZestRenderer->semaphores[ZEST_FIF].render_complete;
	VkSwapchainKHR swapChains[] = { ZestRenderer->swapchain };
	presentInfo.swapchainCount = 1;
	presentInfo.pSwapchains = swapChains;
	presentInfo.pImageIndices = &ZestRenderer->current_frame;
	presentInfo.pResults = ZEST_NULL;

	VkResult result = vkQueuePresentKHR(ZestDevice->present_queue, &presentInfo);

	if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || ZestApp->window->framebuffer_resized || (ZestRenderer->flags & zest_renderer_flag_schedule_change_vsync)) {
		ZestApp->window->framebuffer_resized = ZEST_FALSE;
		ZestRenderer->flags &= ~zest_renderer_flag_schedule_change_vsync;

		zest__recreate_swapchain();
		ZestRenderer->current_frame = 0;
	}
	else {
		ZEST_VK_CHECK_RESULT(result);
	}
	ZestDevice->current_fif = (ZestDevice->current_fif + 1) % ZEST_MAX_FIF;

}
// --End Renderer functions

// --Command Queue functions
void zest__cleanup_command_queue(zest_command_queue *command_queue) {	
	for (ZEST_EACHFFIF_i) {
		for (zest_foreach_j(command_queue->fif_outgoing_semaphores[i])) {
			VkSemaphore semaphore = command_queue->fif_outgoing_semaphores[i][j];
			vkDestroySemaphore(ZestDevice->logical_device, semaphore, ZEST_NULL);
		}
	}
	vkFreeCommandBuffers(ZestDevice->logical_device, command_queue->command_pool, ZEST_MAX_FIF, command_queue->command_buffer);
	vkDestroyCommandPool(ZestDevice->logical_device, command_queue->command_pool, VK_NULL_HANDLE);
}

VkSemaphore zest__get_command_queue_present_semaphore(zest_command_queue *command_queue) {
	ZEST_ASSERT(command_queue->present_semaphore_index != -1);
	return command_queue->fif_outgoing_semaphores[ZEST_FIF][command_queue->present_semaphore_index];
}

void zest__record_and_commit_command_queue(zest_command_queue *command_queue, VkFence fence) {
	ZestRenderer->current_command_queue = command_queue;
	VkCommandBufferBeginInfo begin_info = { 0 };
	begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	begin_info.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;
	vkResetCommandBuffer(command_queue->command_buffer[ZEST_FIF], 0);
	ZEST_VK_CHECK_RESULT(vkBeginCommandBuffer(command_queue->command_buffer[ZEST_FIF], &begin_info) != VK_SUCCESS);

	ZestRenderer->current_command_buffer = command_queue->command_buffer[ZEST_FIF];

	//for (zest_foreach_i(command_queue->compute_items)) {
		//zest_command_queue_compute &compute_item = GetCommandQueueCompute(compute_item_index);
		//ZEST_ASSERT(compute_item.compute_function);		//Compute item must have its compute function callback set
		//ZestRenderer->current_compute_routine = &compute_item;
		//compute_item.compute_function(compute_item);
	//}

	for (zest_foreach_i(command_queue->render_commands)) {
		zest_command_queue_draw *render_command = &ZestRenderer->command_queue_render_passes.data[command_queue->render_commands[i]];
		ZestRenderer->current_render_pass = render_command;
		render_command->render_pass_function(render_command, command_queue->command_buffer[ZEST_FIF], render_command->render_pass, render_command->get_frame_buffer(render_command));
	}

	ZEST_VK_CHECK_RESULT(vkEndCommandBuffer(command_queue->command_buffer[ZEST_FIF]));

	ZestRenderer->current_command_queue = ZEST_NULL;
	ZestRenderer->current_command_buffer = ZEST_NULL;
	ZestRenderer->current_compute_routine = ZEST_NULL;
	ZestRenderer->current_render_pass = ZEST_NULL;

	VkSubmitInfo submit_info = { 0 };
	submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	if (!zest_vec_empty(command_queue->fif_incoming_semaphores[ZEST_FIF])) {
		submit_info.pWaitSemaphores = command_queue->fif_incoming_semaphores[ZEST_FIF];
		submit_info.waitSemaphoreCount = zest_vec_size(command_queue->fif_incoming_semaphores[ZEST_FIF]);
		submit_info.pWaitDstStageMask = command_queue->fif_stage_flags[ZEST_FIF];
	}
	else {
		submit_info.pWaitSemaphores = VK_NULL_HANDLE;
		submit_info.waitSemaphoreCount = 0;
		submit_info.pWaitDstStageMask = VK_NULL_HANDLE;
	}
	submit_info.signalSemaphoreCount = zest_vec_size(command_queue->fif_outgoing_semaphores[ZEST_FIF]);
	submit_info.pSignalSemaphores = command_queue->fif_outgoing_semaphores[ZEST_FIF];
	submit_info.commandBufferCount = 1;
	submit_info.pCommandBuffers = &command_queue->command_buffer[ZEST_FIF];

	vkResetFences(ZestDevice->logical_device, 1, &fence);
	ZEST_VK_CHECK_RESULT(vkQueueSubmit(ZestDevice->graphics_queue, 1, &submit_info, fence));

}

void zest_ConnectCommandQueueToPresent(zest_command_queue *sender, VkPipelineStageFlags stage_flags) {
	VkSemaphoreCreateInfo semaphore_info = { 0 };
	semaphore_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
	for (ZEST_EACHFFIF_i) {
		VkSemaphore semaphore = VK_NULL_HANDLE;
		ZEST_VK_CHECK_RESULT(vkCreateSemaphore(ZestDevice->logical_device, &semaphore_info, ZEST_NULL, &semaphore));
		zest_vec_push(sender->fif_outgoing_semaphores[i], semaphore);
		zest_vec_push(sender->fif_stage_flags[i], stage_flags);
		sender->present_semaphore_index = zest_vec_last_index(sender->fif_outgoing_semaphores[i]);
	}
}
// --Command Queue functions

// --General Helper Functions
VkViewport zest_CreateViewport(float width, float height, float minDepth, float maxDepth) {
	VkViewport viewport = { 0 };
	viewport.width = width;
	viewport.height = height;
	viewport.minDepth = minDepth;
	viewport.maxDepth = maxDepth;
	return viewport;
}

VkRect2D zest_CreateRect2D(zest_uint width, zest_uint height, int offsetX, int offsetY) {
	VkRect2D rect2D = { 0 };
	rect2D.extent.width = width;
	rect2D.extent.height = height;
	rect2D.offset.x = offsetX;
	rect2D.offset.y = offsetY;
	return rect2D;
}

VkImageView zest__create_image_view(VkImage image, VkFormat format, VkImageAspectFlags aspect_flags, zest_uint mip_levels, VkImageViewType view_type, zest_uint layer_count) {
	VkImageViewCreateInfo viewInfo = { 0 };
	viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	viewInfo.image = image;
	viewInfo.viewType = view_type;
	viewInfo.format = format;
	viewInfo.subresourceRange.aspectMask = aspect_flags;
	viewInfo.subresourceRange.baseMipLevel = 0;
	viewInfo.subresourceRange.levelCount = mip_levels;
	viewInfo.subresourceRange.baseArrayLayer = 0;
	viewInfo.subresourceRange.layerCount = layer_count;

	VkImageView image_view;
	ZEST_VK_CHECK_RESULT(vkCreateImageView(ZestDevice->logical_device, &viewInfo, ZEST_NULL, &image_view) != VK_SUCCESS);

	return image_view;
}

zest_buffer *zest__create_image(zest_uint width, zest_uint height, zest_uint mipLevels, VkSampleCountFlagBits numSamples, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkImage *image) {
	VkImageCreateInfo image_info = {0};
	image_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	image_info.imageType = VK_IMAGE_TYPE_2D;
	image_info.extent.width = width;
	image_info.extent.height = height;
	image_info.extent.depth = 1;
	image_info.mipLevels = mipLevels;
	image_info.arrayLayers = 1;
	image_info.format = format;
	image_info.tiling = tiling;
	image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	image_info.usage = usage;
	image_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	image_info.samples = numSamples;

	ZEST_VK_CHECK_RESULT(vkCreateImage(ZestDevice->logical_device, &image_info, ZEST_NULL, image));

	VkMemoryRequirements memory_requirements;
	vkGetImageMemoryRequirements(ZestDevice->logical_device, *image, &memory_requirements);

	zest_buffer_info buffer_info = { 0 };
	buffer_info.image_usage_flags = usage;
	buffer_info.property_flags = properties;
	zest_buffer *buffer = zest_CreateBuffer(memory_requirements.size, &buffer_info, *image, tloc__MEGABYTE(64));

	vkBindImageMemory(ZestDevice->logical_device, *image, zest_GetBufferDeviceMemory(buffer), buffer->memory_offset);

	return buffer;
}

void zest__transition_image_layout(VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout, zest_uint mipLevels, zest_uint layerCount) {
	VkCommandBuffer command_buffer = zest__begin_single_time_commands();

	VkImageMemoryBarrier barrier = { 0 };
	barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	barrier.oldLayout = oldLayout;
	barrier.newLayout = newLayout;
	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.image = image;
	barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	barrier.subresourceRange.baseMipLevel = 0;
	barrier.subresourceRange.levelCount = mipLevels;
	barrier.subresourceRange.baseArrayLayer = 0;
	barrier.subresourceRange.layerCount = layerCount;
	barrier.srcAccessMask = 0;
	VkPipelineStageFlags source_stage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
	VkPipelineStageFlags destination_stage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;

	if (newLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL) {
		barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;

		if (zest__has_stencil_format(format)) {
			barrier.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
		}
	}
	else {
		barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	}

	if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
		barrier.srcAccessMask = 0;
		barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

		source_stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
		destination_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
	}
	else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
		barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

		source_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
		destination_stage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
	}
	else if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL) {
		barrier.srcAccessMask = 0;
		barrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

		source_stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
		destination_stage = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
	}
	else if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL) {
		barrier.srcAccessMask = 0;
		barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		source_stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
		destination_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	}
	else {
		//throw std::invalid_argument("unsupported layout transition!");
		//Unsupported or General transition
	}

	vkCmdPipelineBarrier( command_buffer, source_stage, destination_stage, 0, 0, ZEST_NULL, 0, ZEST_NULL, 1, &barrier );

	zest__end_single_time_commands(command_buffer);

}

VkRenderPass zest__create_render_pass(VkFormat render_format, VkImageLayout final_layout, VkAttachmentLoadOp load_op) {
	VkAttachmentDescription color_attachment = {0};
	color_attachment.format = render_format;
	color_attachment.samples = VK_SAMPLE_COUNT_1_BIT;
	color_attachment.loadOp = load_op;
	color_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	color_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	color_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	color_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	color_attachment.finalLayout = final_layout;

	VkAttachmentDescription depth_attachment = {0};
	depth_attachment.format = zest__find_depth_format();
	depth_attachment.samples = VK_SAMPLE_COUNT_1_BIT;
	depth_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	depth_attachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	depth_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	depth_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	depth_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	depth_attachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	VkAttachmentReference color_attachment_ref = {0};
	color_attachment_ref.attachment = 0;
	color_attachment_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	VkAttachmentReference depth_attachment_ref = {0};
	depth_attachment_ref.attachment = 1;
	depth_attachment_ref.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	VkSubpassDescription subpass = {0};
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass.colorAttachmentCount = 1;
	subpass.pColorAttachments = &color_attachment_ref;
	subpass.pDepthStencilAttachment = &depth_attachment_ref;

	VkSubpassDependency dependency = {0};
	dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
	dependency.dstSubpass = 0;
	dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependency.srcAccessMask = 0;
	dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

	VkAttachmentDescription attachments[2] = { color_attachment, depth_attachment };
	VkRenderPassCreateInfo render_pass_info = {0};
	render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	render_pass_info.attachmentCount = 2;
	render_pass_info.pAttachments = attachments;
	render_pass_info.subpassCount = 1;
	render_pass_info.pSubpasses = &subpass;
	render_pass_info.dependencyCount = 1;
	render_pass_info.pDependencies = &dependency;

	VkRenderPass render_pass;
	ZEST_VK_CHECK_RESULT(vkCreateRenderPass(ZestDevice->logical_device, &render_pass_info, ZEST_NULL, &render_pass));

	return render_pass;
}

VkFormat zest__find_depth_format() {
	VkFormat depth_formats[3] = { VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT };
	return zest__find_supported_format(depth_formats, 3, VK_IMAGE_TILING_OPTIMAL, VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);
}

zest_bool zest__has_stencil_format(VkFormat format) {
	return format == VK_FORMAT_D32_SFLOAT_S8_UINT || format == VK_FORMAT_D24_UNORM_S8_UINT;
}

VkFormat zest__find_supported_format(VkFormat *candidates, zest_uint candidates_count, VkImageTiling tiling, VkFormatFeatureFlags features) {
	for (int i = 0; i != candidates_count; ++i) {
		VkFormatProperties props;
		vkGetPhysicalDeviceFormatProperties(ZestDevice->physical_device, candidates[i], &props);
		if (tiling == VK_IMAGE_TILING_LINEAR && (props.linearTilingFeatures & features) == features) {
			return candidates[i];
		}
		else if (tiling == VK_IMAGE_TILING_OPTIMAL && (props.optimalTilingFeatures & features) == features) {
			return candidates[i];
		}
	}

	return 0; 
}

VkCommandBuffer zest__begin_single_time_commands() {
	VkCommandBufferAllocateInfo alloc_info = { 0 };
	alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	alloc_info.commandPool = ZestDevice->command_pool;
	alloc_info.commandBufferCount = 1;

	VkCommandBuffer command_buffer;
	vkAllocateCommandBuffers(ZestDevice->logical_device, &alloc_info, &command_buffer);

	VkCommandBufferBeginInfo begin_info = { 0 };
	begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

	vkBeginCommandBuffer(command_buffer, &begin_info);

	return command_buffer;
}

void zest__end_single_time_commands(VkCommandBuffer command_buffer) {
	vkEndCommandBuffer(command_buffer);

	VkSubmitInfo submit_info = { 0 };
	submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submit_info.commandBufferCount = 1;
	submit_info.pCommandBuffers = &command_buffer;

	vkQueueSubmit(ZestDevice->one_time_graphics_queue, 1, &submit_info, VK_NULL_HANDLE);
	vkQueueWaitIdle(ZestDevice->one_time_graphics_queue);

	vkFreeCommandBuffers(ZestDevice->logical_device, ZestDevice->command_pool, 1, &command_buffer);
}

zest_create_info zest_CreateInfo() {
	zest_create_info create_info = {
		.screen_width = 1280, 
		.screen_height = 768,			
		.screen_x = 0, 
		.screen_y = 50,
		.virtual_width = 1280, 
		.virtual_height = 768,
		.color_format = VK_FORMAT_R8G8B8A8_UNORM,
		.pool_counts = ZEST_NULL
	};
	return create_info;
}

char* zest_ReadEntireFile(const char *file_name, zest_bool terminate) {
	char* buffer = 0;
	FILE *file;
	errno_t err;
	err = fopen_s(&file, file_name, "rb");
	if (err != 0 || file == NULL) {
		return buffer;
	}

	// file invalid? fseek() fail?
	if (file == NULL || fseek(file, 0, SEEK_END)) {
		fclose(file);
		return buffer;
	}

	long length = ftell(file);
	rewind(file);
	// Did ftell() fail?  Is the length too long?
	if (length == -1 || (unsigned long)length >= SIZE_MAX) {
		fclose(file);
		return buffer;
	}

	if (terminate) {
		zest_vec_resize(buffer, (zest_uint)length + 1);
	}
	else {
		zest_vec_resize(buffer, (zest_uint)length);
	}
	if (buffer == 0 || fread(buffer, 1, length, file) != length) {
		zest_vec_free(buffer);
		fclose(file);
		return buffer;
	}
	if (terminate) {
		buffer[length] = '\0';
	}

	fclose(file);
	return buffer;
}
// --End General Helper Functions

// --Buffer allocation funcitons
// --End Buffer allocation functions

void zest__main_loop(void) {
	while (!glfwWindowShouldClose(ZestApp->window->window_handle)) {

		ZEST_VK_CHECK_RESULT(vkWaitForFences(ZestDevice->logical_device, 1, &ZestRenderer->fif_fence[ZEST_FIF], VK_TRUE, UINT64_MAX));
		//DoScheduledTasks(ZestDevice->current_fif);

		glfwPollEvents();

		if (ZestApp->update_callback) {
			ZestApp->update_callback(0, 0);
		}

		zest__draw_renderer_frame();

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

int main(void) {

	zest_create_info create_info = zest_CreateInfo();
	zest_Initialise(&create_info);

	zest_vec(zest_uint, T);
	zest_vec_push(T, 0);
	zest_vec_push(T, 1);
	zest_vec_push(T, 2);
	zest_vec_push(T, 3);
	zest_uint *loc = &T[1];
	zest_vec_insert(T, loc, 5);
	zest_vec_erase(T, &T[1]);

	//zest_uint test = zest_vec_pop(T);

	zest_vec *vec = zest__vec_header(T);
	if (!zest_vec_empty(T)) {
		printf("size: %i, size in bytes: %zu, capacity: %i\n", zest_vec_size(T), zest_vec_size_in_bytes(T), zest_vec_capacity(T));
	}
	for (zest_foreach_i(T)) {
		printf("%i: %i\n", i, T[i]);
	}
	zest_vec_resize(T, 20);
	printf("size: %i, size in bytes: %zu, capacity: %i\n", zest_vec_size(T), zest_vec_size_in_bytes(T), zest_vec_capacity(T));
	zest_vec_free(T);

	zest_hasher hasher;
	zest_key hash = zest_Hash(&hasher, "Hash this", 9, 0);
	printf("hash: %zu\n", zest_Hash(&hasher, "the key", strlen("the key"), 0));
	printf("hash: %zu\n", zest_Hash(&hasher, "another key", strlen("another key"), 0));
	printf("hash: %zu\n", zest_Hash(&hasher, "some other key", strlen("some other key"), 0));

	zest_hash_map(zest_uint) hash_map;
	hash_map my_hash_map = {0};
	zest_map_insert(my_hash_map, "the key", 777);
	zest_map_insert(my_hash_map, "another key", 222);
	zest_map_insert(my_hash_map, "some other key", 333);
	zest_map_insert(my_hash_map, "the key 2", 111);
	zest_map_remove(my_hash_map, "another key");
	for (zest_foreach_i(my_hash_map.data)) {
		printf("%i: %i, %i\n", i, my_hash_map.data[i], zest_vec_size(my_hash_map.data));
	}
	printf("%i \n", *zest_map_at(my_hash_map, "the key 2"));
	if (!zest_map_valid_name(my_hash_map, "bad name")) {
		printf("not a valid name\n");
	}

	zest_Start();

	return 0;
}
