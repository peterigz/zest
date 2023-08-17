#include "zest.h"
#define TLOC_IMPLEMENTATION
#define TLOC_OUTPUT_ERROR_MESSAGES
#include "2loc.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include "stb_image_resize.h"

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

// --Math
zest_matrix4 zest_M4(float v) {
	zest_matrix4 matrix = { 0 }; 
	matrix.v[0].x = v; 
	matrix.v[1].y = v; 
	matrix.v[2].z = v; 
	matrix.v[3].w = v; 
	return matrix; 
}

zest_matrix4 zest_ScaleMatrix4x4(zest_matrix4 *m, zest_vec4 *v) {
	zest_matrix4 result = zest_M4(1);
	result.v[0] = zest_ScaleVec4(&m->v[0], v->x);
	result.v[1] = zest_ScaleVec4(&m->v[1], v->y);
	result.v[2] = zest_ScaleVec4(&m->v[2], v->z);
	result.v[3] = zest_ScaleVec4(&m->v[3], v->w);
	return result;
}

zest_vec2 zest_Vec2Set1(float v) { 
	zest_vec2 vec; vec.x = v; vec.y = v; return vec; 
}

zest_vec3 zest_Vec3Set1(float v) { 
	zest_vec3 vec; vec.x = v; vec.y = v; vec.z = v; return vec; 
}

zest_vec4 zest_Vec4Set1(float v) { 
	zest_vec4 vec; vec.x = v; vec.y = v; vec.z = v; vec.w = v; return vec; 
}

zest_vec2 zest_Vec2Set(float x, float y) {
	zest_vec2 vec; vec.x = x; vec.y = y; return vec; 
}
zest_vec3 zest_Vec3Set(float x, float y, float z) {
	zest_vec3 vec; vec.x = x; vec.y = y; vec.z = z; return vec;
}
zest_vec4 zest_Vec4Set(float x, float y, float z, float w) {
	zest_vec4 vec; vec.x = x; vec.y = y; vec.z = z; vec.w = w; return vec; 
}

zest_color zest_ColorSet(zest_byte r, zest_byte g, zest_byte b, zest_byte a) {
	zest_color color = {
		.r = r,.g = g,.b = b,.a = a
	};
	return color;
}

zest_color zest_ColorSet1(zest_byte c) {
	zest_color color = {
		.r = c,.g = c,.b = c,.a = c
	};
	return color;
}

zest_vec3 zest_SubVec(zest_vec3 left, zest_vec3 right) {
	zest_vec3 result = {
		.x = left.x - right.x,
		.y = left.y - right.y,
		.z = left.z - right.z,
	};
	return result;
}

zest_vec4 zest_ScaleVec4(zest_vec4 *vec4, float v) {
	zest_vec4 result;
	result.x = vec4->x * v;
	result.y = vec4->y * v;
	result.z = vec4->z * v;
	result.w = vec4->w * v;
	return result;
}

float zest_LengthVec2(zest_vec3 const v) {
	return v.x * v.x + v.y * v.y + v.z * v.z;
}

float zest_LengthVec(zest_vec3 const v) {
	return sqrtf(zest_LengthVec2(v));
}

zest_vec3 zest_NormalizeVec(zest_vec3 const v) {
	float length = zest_LengthVec(v);
	zest_vec3 result = {
		.x = v.x / length,
		.y = v.y / length,
		.z = v.z / length
	};
	return result;
}

zest_vec3 zest_CrossProduct(const zest_vec3 x, const zest_vec3 y)
{
	zest_vec3 result = {
		.x = x.y * y.z - y.y * x.z,
		.y = x.z * y.x - y.z * x.x,
		.z = x.x * y.y - y.x * x.y,
	};
	return result;
}

float zest_DotProduct(const zest_vec3 a, const zest_vec3 b)
{
	return (a.x * b.x + a.y * b.y + a.z * b.z);
}

zest_matrix4 zest_LookAt(const zest_vec3 eye, const zest_vec3 center, const zest_vec3 up)
{
	zest_vec3 f = zest_NormalizeVec(zest_SubVec(center, eye));
	zest_vec3 s = zest_NormalizeVec(zest_CrossProduct(f, up));
	zest_vec3 u = zest_CrossProduct(s, f);

	zest_matrix4 result = { 0 };
	result.v[0].x = s.x;
	result.v[1].x = s.y;
	result.v[2].x = s.z;
	result.v[0].y = u.x;
	result.v[1].y = u.y;
	result.v[2].y = u.z;
	result.v[0].z = -f.x;
	result.v[1].z = -f.y;
	result.v[2].z = -f.z;
	result.v[3].x = -zest_DotProduct(s, eye);
	result.v[3].y = -zest_DotProduct(u, eye);
	result.v[3].z = zest_DotProduct(f, eye);
	result.v[3].w = 1.f;
	return result;
}

zest_matrix4 zest_Ortho(float left, float right, float bottom, float top, float z_near, float z_far)
{
	zest_matrix4 result = { 0 };
	result.v[0].x = 2.f / (right - left);
	result.v[1].y = 2.f / (top - bottom);
	result.v[2].z = -1.f / (z_far - z_near);
	result.v[3].x = -(right + left) / (right - left);
	result.v[3].y = -(top + bottom) / (top - bottom);
	result.v[3].z = -z_near / (z_far - z_near);
	result.v[3].w = 1.f;
	return result;
}

float zest_Distance(float fromx, float fromy, float tox, float toy) {
	float w = tox - fromx;
	float h = toy - fromy;
	return sqrtf(w * w + h * h);
}

zest_uint zest_Pack16bit(float x, float y) {
	union
	{
		signed short in[2];
		zest_uint out;
	} u;

	x = x * 32767.f;
	y = y * 32767.f;

	u.in[0] = (signed short)x;
	u.in[1] = (signed short)y;

	return u.out;
}

zest_size zest_GetNextPower(zest_size n) {
	zest_size t = 1;
	while (t < n)
		t *= 2;
	return t;
}

//  --End Math

void zest_Initialise(zest_create_info *info) {
	void *memory_pool = malloc(info->memory_pool_size);

	ZEST_ASSERT(memory_pool);	//unable to allocate initial memory pool

	tloc_allocator *allocator = tloc_InitialiseAllocatorWithPool(memory_pool, info->memory_pool_size);
	ZestDevice = tloc_Allocate(allocator, sizeof(zest_device));
	ZestApp = tloc_Allocate(allocator, sizeof(zest_app));
	ZestRenderer = tloc_Allocate(allocator, sizeof(zest_renderer));
	ZestHasher = tloc_Allocate(allocator, sizeof(zest_hasher));
	memset(ZestDevice, 0, sizeof(zest_device));
	memset(ZestApp, 0, sizeof(zest_app));
	memset(ZestRenderer, 0, sizeof(zest_renderer));
	ZestDevice->memory_pools[0] = memory_pool;
	ZestDevice->memory_pool_count = 1;
	ZestDevice->allocator = allocator;
	ZestDevice->color_format = info->color_format;
	ZestDevice->allocation_callbacks.pfnAllocation = zest_vk_allocate_callback;
	ZestDevice->allocation_callbacks.pfnReallocation = zest_vk_reallocate_callback;
	ZestDevice->allocation_callbacks.pfnFree = zest_vk_free_callback;
	zest__initialise_app(info);
	zest__initialise_device();
	zest__initialise_renderer(info);
	zest__create_empty_command_queue(&ZestRenderer->empty_queue);
	if (info->flags & zest_init_flag_initialise_with_command_queue) {
		ZestApp->default_command_queue_index = zest_NewCommandQueue("Default command queue", zest_command_dependency_type_present, 0);
		{
			ZestApp->default_render_commands_index = zest_NewRenderPassSetupSC("Default render pass");
			{
				ZestApp->default_layer_index = zest_NewSpriteLayerSetup("Sprite 2d Layer");
			}
			zest_ConnectQueueToPresent();
			zest_Finish();
		}
	}
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
	zest__set_default_pool_sizes();
}

void zest_SetUserData(void* data) {
	ZestApp->user_data = data;
}

void zest_SetUserCallback(void(*callback)(zest_microsecs, void*)) {
	ZestApp->update_callback = callback;
}

void zest_SetActiveRenderQueue(zest_index command_queue_index) {
	ZEST_ASSERT(zest_vec_empty(ZestRenderer->frame_queues));									//You cannot specify more than one render queue per frame, try using SetActiveRenderSet instead for using multiple render queues
	ZEST_ASSERT(command_queue_index < (zest_index)zest_map_size(ZestRenderer->command_queues));	//Must be a valid render queue index
	zest_vec_push(ZestRenderer->frame_queues, command_queue_index);
	ZestRenderer->semaphores[ZEST_FIF].render_complete = zest_GetPresentSemaphore(zest_GetCommandQueue(command_queue_index));
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

	VkResult result = vkCreateInstance(&create_info, &ZestDevice->allocation_callbacks, &ZestDevice->instance);

	ZEST_VK_CHECK_RESULT(result);

	ZEST__FREE(available_extensions);
	zest__create_window_surface(ZestApp->window);

}

void zest__create_window_surface(zest_window* window) {
	ZEST_VK_CHECK_RESULT(glfwCreateWindowSurface(ZestDevice->instance, window->window_handle, &ZestDevice->allocation_callbacks, &window->surface));
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
			destroyDebugUtilsMessenger(ZestDevice->instance, ZestDevice->debug_messenger, &ZestDevice->allocation_callbacks);
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

	ZEST_VK_CHECK_RESULT(zest_create_debug_messenger(ZestDevice->instance, &create_info, &ZestDevice->allocation_callbacks, &ZestDevice->debug_messenger));

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
    zest_queue_family_indices indices = { 0 };

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
	//device_features.wideLines = VK_TRUE;
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

	ZEST_VK_CHECK_RESULT(vkCreateDevice(ZestDevice->physical_device, &create_info, &ZestDevice->allocation_callbacks, &ZestDevice->logical_device));

	vkGetDeviceQueue(ZestDevice->logical_device, indices.graphics_family, 0, &ZestDevice->graphics_queue);
	vkGetDeviceQueue(ZestDevice->logical_device, indices.graphics_family, 0, &ZestDevice->one_time_graphics_queue);
	vkGetDeviceQueue(ZestDevice->logical_device, indices.present_family, 0, &ZestDevice->present_queue);
	vkGetDeviceQueue(ZestDevice->logical_device, indices.compute_family, 0, &ZestDevice->compute_queue);

	ZestDevice->graphics_queue_family_index = indices.graphics_family;
	ZestDevice->present_queue_family_index = indices.present_family;
	ZestDevice->compute_queue_family_index = indices.compute_family;

	VkCommandPoolCreateInfo cmd_info_pool = { 0 };
	cmd_info_pool.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	cmd_info_pool.queueFamilyIndex = ZestDevice->graphics_queue_family_index;
	cmd_info_pool.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
	ZEST_VK_CHECK_RESULT(vkCreateCommandPool(ZestDevice->logical_device, &cmd_info_pool, &ZestDevice->allocation_callbacks, &ZestDevice->command_pool));
}

void zest__set_limit_data() {
	VkPhysicalDeviceProperties physicalDeviceProperties;
	vkGetPhysicalDeviceProperties(ZestDevice->physical_device, &physicalDeviceProperties);

	ZestDevice->max_image_size = physicalDeviceProperties.limits.maxImageDimension2D;
}

void zest__set_default_pool_sizes() {
	zest_buffer_usage usage = { 0 };
	//Depth buffer image
	usage.image_flags = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
	usage.property_flags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
	zest_SetDevicePoolSize(0, usage.property_flags, usage.image_flags, tloc__KILOBYTE(4), tloc__MEGABYTE(16));

	//Texture images
	usage.image_flags = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
	zest_SetDevicePoolSize(0, usage.property_flags, usage.image_flags, tloc__KILOBYTE(4), tloc__MEGABYTE(32));

	//Uniform buffers
	usage.image_flags = 0;
	usage.usage_flags = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
	usage.property_flags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
	zest_SetDevicePoolSize(usage.usage_flags, usage.property_flags, 0, 64, tloc__MEGABYTE(1));

	//Staging buffer
	usage.usage_flags = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
	zest_SetDevicePoolSize(usage.usage_flags, usage.property_flags, 0, tloc__KILOBYTE(2), tloc__MEGABYTE(32));

	//Vertex buffer
	usage.usage_flags = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
	usage.property_flags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
	zest_SetDevicePoolSize(usage.usage_flags, usage.property_flags, 0, tloc__KILOBYTE(2), tloc__MEGABYTE(32));
}

void *zest_vk_allocate_callback(void* pUserData, size_t size, size_t alignment, VkSystemAllocationScope allocationScope) {
	return ZEST__ALLOCATE(size);
}

void *zest_vk_reallocate_callback(void* pUserData, void *memory, size_t size, size_t alignment, VkSystemAllocationScope allocationScope) {
	return ZEST__REALLOCATE(memory, size);
}

void zest_vk_free_callback(void* pUserData, void *memory) {
	ZEST__FREE(memory);
}

/*
End of Device creation functions
*/

void zest__initialise_app(zest_create_info *create_info) {
	memset(ZestApp, 0, sizeof(zest_app));
	ZestApp->create_info = *create_info;
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
	vkDestroySurfaceKHR(ZestDevice->instance, ZestApp->window->surface, &ZestDevice->allocation_callbacks);
	glfwDestroyWindow(ZestApp->window->window_handle);
	glfwTerminate();
	zest_destroy_debug_messenger();
	vkDestroyCommandPool(ZestDevice->logical_device, ZestDevice->command_pool, &ZestDevice->allocation_callbacks);
	vkDestroyDevice(ZestDevice->logical_device, &ZestDevice->allocation_callbacks);
	vkDestroyInstance(ZestDevice->instance, &ZestDevice->allocation_callbacks);
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
	memset(window, 0, sizeof(zest_window));
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

// --Buffer & Memory Management
void* zest__allocate(zest_size size) {
	void *allocation = tloc_Allocate(ZestDevice->allocator, size);
	if (!allocation) {
		ZEST_ASSERT(ZestDevice->memory_pool_count < 32);	//Reached the max number of memory pools
		zest_size pool_size = ZestApp->create_info.memory_pool_size;
		if (pool_size <= size) {
			pool_size = zest_GetNextPower(size);
		}
		ZestDevice->memory_pools[ZestDevice->memory_pool_count] = malloc(pool_size);
		ZEST_ASSERT(ZestDevice->memory_pools[ZestDevice->memory_pool_count]);	//Unable to allocate more memory. Out of memory?
		tloc_AddPool(ZestDevice->allocator, ZestDevice->memory_pools[ZestDevice->memory_pool_count], pool_size);
		allocation = tloc_Allocate(ZestDevice->allocator, size);
		ZEST_ASSERT(allocation);	//Unable to allocate even after adding a pool
		ZestDevice->memory_pool_count++;
		ZEST_PRINT_NOTICE(ZEST_NOTICE_COLOR"Note: Ran out of space in the host memory pool so adding a new one of size %llu. ", pool_size);
	}
	return allocation;
}

void* zest__reallocate(void *memory, zest_size size) {
	void *allocation = tloc_Reallocate(ZestDevice->allocator, memory, size);
	if (!allocation) {
		ZEST_ASSERT(ZestDevice->memory_pool_count < 32);	//Reached the max number of memory pools
		zest_size pool_size = ZestApp->create_info.memory_pool_size;
		if (pool_size <= size) {
			pool_size = zest_GetNextPower(size);
		}
		ZestDevice->memory_pools[ZestDevice->memory_pool_count] = malloc(pool_size);
		ZEST_ASSERT(ZestDevice->memory_pools[ZestDevice->memory_pool_count]);	//Unable to allocate more memory. Out of memory?
		tloc_AddPool(ZestDevice->allocator, ZestDevice->memory_pools[ZestDevice->memory_pool_count], pool_size);
		allocation = tloc_Reallocate(ZestDevice->allocator, memory, size);
		ZEST_ASSERT(allocation);	//Unable to allocate even after adding a pool
		ZestDevice->memory_pool_count++;
		ZEST_PRINT_NOTICE(ZEST_NOTICE_COLOR"Note: Ran out of space in the host memory pool so adding a new one of size %llu. ", pool_size);
	}
	return allocation;
}

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
		vkDestroyBuffer(ZestDevice->logical_device, memory_allocation->buffer, &ZestDevice->allocation_callbacks);
	}
	if (memory_allocation->memory) {
		vkFreeMemory(ZestDevice->logical_device, memory_allocation->memory, &ZestDevice->allocation_callbacks);
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

	ZEST_VK_CHECK_RESULT(vkCreateBuffer(ZestDevice->logical_device, &buffer_info, &ZestDevice->allocation_callbacks, &buffer->buffer));

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
	ZEST_VK_CHECK_RESULT(vkAllocateMemory(ZestDevice->logical_device, &alloc_info, &ZestDevice->allocation_callbacks, &buffer->memory));

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

	ZEST_VK_CHECK_RESULT(vkAllocateMemory(ZestDevice->logical_device, &alloc_info, &ZestDevice->allocation_callbacks, &buffer->memory));
}

tloc_size zest__get_remote_size(const tloc_header *block) {
	zest_buffer *buffer = (zest_buffer*)tloc_BlockUserExtensionPtr(block);
	return buffer->size;
}

void zest__on_add_pool(void *user_data, void *block) {
	zest_buffer_allocator *pools = (zest_buffer_allocator*)user_data;
	zest_buffer *buffer = (zest_buffer*)block;
	buffer->size = pools->memory_pools[zest_vec_last_index(pools->memory_pools)].size;
	buffer->memory_pool = zest_vec_last_index(pools->memory_pools);
	buffer->memory_offset = 0;
}

void zest__on_merge_next(void *user_data, tloc_header *block, tloc_header *next_block) {
	zest_buffer *buffer = tloc_BlockUserExtensionPtr(block);
	zest_buffer *next_buffer = tloc_BlockUserExtensionPtr(next_block);
	buffer->size += next_buffer->size;
	next_buffer->memory_offset = 0;
	next_buffer->size = 0;
}

void zest__on_merge_prev(void *user_data, tloc_header *prev_block, tloc_header *block) {
	zest_buffer *buffer = tloc_BlockUserExtensionPtr(block);
	if (buffer->memory_offset == 0) {
		//Can't merge across pools
		//return;
	}
	zest_buffer *prev_buffer = tloc_BlockUserExtensionPtr(prev_block);
	prev_buffer->size += buffer->size;
	buffer->memory_offset = 0;
	buffer->size = 0;
}

void zest__on_split_block(void *user_data, tloc_header* block, tloc_header *trimmed_block, zest_size remote_size) {
	zest_buffer_allocator *pools = (zest_buffer_allocator*)user_data;
	zest_buffer *buffer = tloc_BlockUserExtensionPtr(block);
	zest_buffer *trimmed_buffer = tloc_BlockUserExtensionPtr(trimmed_block);
	trimmed_buffer->size = buffer->size - remote_size;
	trimmed_buffer->memory_pool = buffer->memory_pool;
	trimmed_buffer->buffer_allocator = buffer->buffer_allocator;
	trimmed_buffer->memory_in_use = 0;
	buffer->size = remote_size;
	trimmed_buffer->memory_offset = buffer->memory_offset + buffer->size;
	if (pools->buffer_info.property_flags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) {
		buffer->data = (void*)((char*)pools->memory_pools[buffer->memory_pool].mapped + buffer->memory_offset);
		buffer->end = (void*)((char*)buffer->data + buffer->size);
	}
}

void zest__on_reallocation_copy(void *user_data, tloc_header* block, tloc_header *new_block) {
	zest_buffer_allocator *pools = (zest_buffer_allocator*)user_data;
	zest_buffer *buffer = tloc_BlockUserExtensionPtr(block);
	zest_buffer *new_buffer = tloc_BlockUserExtensionPtr(new_block);
	if (pools->buffer_info.property_flags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) {
		new_buffer->data = (void*)((char*)new_buffer->buffer_allocator->memory_pools[new_buffer->memory_pool].mapped + new_buffer->memory_offset);
		new_buffer->end = (void*)((char*)new_buffer->data + new_buffer->size);
		memcpy(new_buffer->data, buffer->data, buffer->size);
	}
}

tloc_size zest__get_bytes_per_block(zest_size pool_size) {
	return pool_size > tloc__MEGABYTE(1) ? pool_size / 128 : 256;
}

zest_device_memory_pool zest__create_vk_memory_pool(zest_buffer_info *buffer_info, VkImage image, zest_size minimum_size) {
	zest_device_memory_pool buffer_pool = { 0 };
	if (image == VK_NULL_HANDLE) {
		zest_buffer_pool_size pre_defined_pool_size = zest_GetDevicePoolSize(buffer_info->usage_flags, buffer_info->property_flags, buffer_info->image_usage_flags);
		if (pre_defined_pool_size.pool_size > 0) {
			buffer_pool.size = pre_defined_pool_size.pool_size > minimum_size ? pre_defined_pool_size.pool_size : zest_GetNextPower(minimum_size + minimum_size / 2);
		}
		else {
			ZEST_PRINT_WARNING(ZEST_WARNING_COLOR"Allocating memory where no default pool size was found for usage flags: %i and property flags: %i. Defaulting to next power from size + size / 2",
				buffer_info->usage_flags, buffer_info->property_flags);
			buffer_pool.size = zest_GetNextPower(minimum_size + minimum_size / 2);
		}
		zest__create_device_memory_pool(buffer_pool.size, buffer_info->usage_flags, buffer_info->property_flags, &buffer_pool, "");
	}
	else {
		zest_buffer_pool_size pre_defined_pool_size = zest_GetDevicePoolSize(buffer_info->usage_flags, buffer_info->property_flags, buffer_info->image_usage_flags);
		if (pre_defined_pool_size.pool_size > 0) {
			buffer_pool.size = pre_defined_pool_size.pool_size > minimum_size ? pre_defined_pool_size.pool_size : zest_GetNextPower(minimum_size + minimum_size / 2);
		}
		else {
			ZEST_PRINT_WARNING(ZEST_WARNING_COLOR"Allocating image memory where no default pool size was found for image usage flags: %i, and property flags: %i. Defaulting to next power from size + size / 2",
				buffer_info->image_usage_flags, buffer_info->property_flags);
			buffer_pool.size = zest_GetNextPower(minimum_size + minimum_size / 2);
		}
		zest__create_image_memory_pool(buffer_pool.size, image, buffer_info->property_flags, &buffer_pool);
	}
	if (buffer_info->property_flags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) {
		zest__map_memory(&buffer_pool, VK_WHOLE_SIZE, 0);
	}
	return buffer_pool;
}

void zest__add_remote_range_pool(zest_buffer_allocator *buffer_allocator, zest_device_memory_pool *buffer_pool) {
	zest_vec_push(buffer_allocator->memory_pools, *buffer_pool);
	tloc_size range_pool_size = tloc_CalculateRemoteBlockPoolSize(buffer_allocator->allocator, buffer_pool->size);
	zest_pool_range *range_pool = ZEST__ALLOCATE(range_pool_size);
	zest_vec_push(buffer_allocator->range_pools, range_pool);
	tloc_AddRemotePool(buffer_allocator->allocator, range_pool, range_pool_size, buffer_pool->size);
}

void zest__set_buffer_details(zest_buffer_allocator *buffer_allocator, zest_buffer *buffer, zest_bool is_host_visible) {
	buffer->buffer_allocator = buffer_allocator;
	buffer->memory_in_use = 0;
	if (is_host_visible) {
		buffer->data = (void*)((char*)buffer_allocator->memory_pools[buffer->memory_pool].mapped + buffer->memory_offset);
		buffer->end = (void*)((char*)buffer->data + buffer->size);
	}
	else {
		buffer->data = ZEST_NULL;
	}
}

zest_buffer *zest_CreateBuffer(VkDeviceSize size, zest_buffer_info *buffer_info, VkImage image) {
	if (image != VK_NULL_HANDLE) {
		VkMemoryRequirements memory_requirements;
		vkGetImageMemoryRequirements(ZestDevice->logical_device, image, &memory_requirements);
		buffer_info->memory_type_bits = memory_requirements.memoryTypeBits;
		buffer_info->alignment = memory_requirements.alignment;
	}

	zest_key key = zest_map_hash_ptr(ZestRenderer->buffer_allocators, buffer_info, sizeof(zest_buffer_info));
	if (!zest_map_valid_key(ZestRenderer->buffer_allocators, key)) {
		//If an allocator doesn't exist yet for this combination of usage and buffer properties then create one.
		zest_buffer_allocator buffer_allocator = { 0 };
		buffer_allocator.buffer_info = *buffer_info;
		zest_device_memory_pool buffer_pool = zest__create_vk_memory_pool(buffer_info, image, size);

		buffer_allocator.alignment = buffer_pool.alignment;
		zest_vec_push(buffer_allocator.memory_pools, buffer_pool);
		buffer_allocator.allocator = ZEST__REALLOCATE(buffer_allocator.allocator, tloc_AllocatorSize());
		buffer_allocator.allocator = tloc_InitialiseAllocator(buffer_allocator.allocator);
		tloc_SetBlockExtensionSize(buffer_allocator.allocator, sizeof(zest_buffer));
		tloc_SetBytesPerBlock(buffer_allocator.allocator, zest__get_bytes_per_block(buffer_pool.size));
		tloc_size range_pool_size = tloc_CalculateRemoteBlockPoolSize(buffer_allocator.allocator, buffer_pool.size);
		zest_pool_range *range_pool = ZEST__ALLOCATE(range_pool_size);
		zest_vec_push(buffer_allocator.range_pools, range_pool);
		zest_map_insert_key(ZestRenderer->buffer_allocators, key, buffer_allocator);
		buffer_allocator.allocator->user_data = zest_map_at_key(ZestRenderer->buffer_allocators, key);
		buffer_allocator.allocator->get_block_size_callback = zest__get_remote_size;
		buffer_allocator.allocator->add_pool_callback = zest__on_add_pool;
		buffer_allocator.allocator->split_block_callback = zest__on_split_block;
		buffer_allocator.allocator->merge_next_callback = zest__on_merge_next;
		buffer_allocator.allocator->merge_prev_callback = zest__on_merge_prev;
		buffer_allocator.allocator->unable_to_reallocate_callback = zest__on_reallocation_copy;
		tloc_AddRemotePool(buffer_allocator.allocator, range_pool, range_pool_size, buffer_pool.size);
	}

	zest_buffer_allocator *buffer_allocator = zest_map_at_key(ZestRenderer->buffer_allocators, key);
	tloc_size adjusted_size = tloc__align_size_up(size, buffer_allocator->alignment);
	zest_buffer *buffer = tloc_AllocateRemote(buffer_allocator->allocator, adjusted_size);
	if (buffer) {
		zest__set_buffer_details(buffer_allocator, buffer, buffer_info->property_flags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
	}
	else {
		zest_device_memory_pool buffer_pool = zest__create_vk_memory_pool(buffer_info, image, size);
		ZEST_ASSERT(buffer_pool.alignment == buffer_allocator->alignment);	//The new pool should have the same alignment as the alignment set in the allocator, this
																			//would have been set when the first pool was created

		void zest__add_remote_range_pool(buffer_allocator, buffer_pool);
		zest_buffer *buffer = tloc_AllocateRemote(buffer_allocator->allocator, adjusted_size);
		ZEST_ASSERT(buffer);	//Unable to allocate memory. Out of memory?
		zest__set_buffer_details(buffer_allocator, buffer, buffer_info->property_flags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
	}

	return buffer;
}

zest_bool zest_GrowBuffer(zest_buffer **buffer, zest_size unit_size) {
	ZEST_ASSERT(unit_size);
	zest_size units = (*buffer)->size / unit_size;
	zest_size new_size = (units ? units + units / 2 : 8) * unit_size;
	zest_buffer_allocator *buffer_allocator = (*buffer)->buffer_allocator;
	zest_buffer *new_buffer = tloc_ReallocateRemote(buffer_allocator->allocator, *buffer, new_size);
	if (new_buffer) {
		new_buffer->buffer_allocator = (*buffer)->buffer_allocator;
		new_buffer->memory_in_use = (*buffer)->memory_in_use;
		*buffer = new_buffer;
		zest__set_buffer_details(buffer_allocator, *buffer, buffer_allocator->buffer_info.property_flags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
	}
	else {
		//Create a new memory pool and try again
		zest_device_memory_pool buffer_pool = zest__create_vk_memory_pool(&buffer_allocator->buffer_info, ZEST_NULL, new_size);
		ZEST_ASSERT(buffer_pool.alignment == buffer_allocator->alignment);	//The new pool should have the same alignment as the alignment set in the allocator, this
																			//would have been set when the first pool was created
		void zest__add_remote_range_pool(buffer_allocator, buffer_pool);
		zest_buffer *new_buffer = tloc_ReallocateRemote(buffer_allocator->allocator, *buffer, new_size);
		ZEST_ASSERT(new_buffer);	//Unable to allocate memory. Out of memory?
		*buffer = new_buffer;
		zest__set_buffer_details(buffer_allocator, *buffer, buffer_allocator->buffer_info.property_flags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
	}
	return new_buffer ? ZEST_TRUE : ZEST_FALSE;
}

zest_buffer_info zest_CreateVertexBufferInfo() {
	zest_buffer_info buffer_info = { 0 };
	buffer_info.usage_flags = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
	buffer_info.property_flags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
	return buffer_info;
}

zest_buffer_info zest_CreateStagingBufferInfo() {
	zest_buffer_info buffer_info = { 0 };
	buffer_info.usage_flags = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
	buffer_info.property_flags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
	return buffer_info;
}

void zest_FreeBuffer(zest_buffer *buffer) {
	ZEST_ASSERT(buffer);	//Buffer must point to a valid buffer
	tloc_FreeRemote(buffer->buffer_allocator->allocator, buffer);
}

VkDeviceMemory zest_GetBufferDeviceMemory(zest_buffer *buffer) { 
	return buffer->buffer_allocator->memory_pools[buffer->memory_pool].memory; 
}

VkBuffer *zest_GetBufferDeviceBuffer(zest_buffer *buffer) { 
	return &buffer->buffer_allocator->memory_pools[buffer->memory_pool].buffer; 
}

void zest_AddCopyCommand(zest_buffer_uploader *uploader, zest_buffer *source_buffer, zest_buffer *target_buffer, VkDeviceSize target_offset) {
	if (uploader->flags & zest_buffer_upload_flag_initialised)
		ZEST_ASSERT(uploader->source_buffer == source_buffer && uploader->target_buffer == target_buffer);	//Buffer uploads must be to the same source and target ids with each copy. Use a separate BufferUpload for each combination of source and target buffers

	uploader->source_buffer = source_buffer;
	uploader->target_buffer = target_buffer;
	uploader->flags |= zest_buffer_upload_flag_initialised;

	VkBufferCopy buffer_info;
	buffer_info.srcOffset = source_buffer->memory_offset;
	buffer_info.dstOffset = target_buffer->memory_offset + target_offset;
	buffer_info.size = source_buffer->memory_in_use;
	zest_vec_push(uploader->buffer_copies, buffer_info);
	target_buffer->memory_in_use = source_buffer->memory_in_use;
}

zest_bool zest_UploadBuffer(zest_buffer_uploader *uploader, VkCommandBuffer command_buffer) {
	if (!zest_vec_size(uploader->buffer_copies)) {
		return ZEST_FALSE;
	}

	vkCmdCopyBuffer(command_buffer, *zest_GetBufferDeviceBuffer(uploader->source_buffer), *zest_GetBufferDeviceBuffer(uploader->target_buffer), zest_vec_size(uploader->buffer_copies), uploader->buffer_copies); 
	zest_vec_free(uploader->buffer_copies);

	return ZEST_TRUE;
}

zest_buffer_pool_size zest_GetDevicePoolSize(VkBufferUsageFlags usage_flags, VkMemoryPropertyFlags property_flags, VkImageUsageFlags image_flags) {
	zest_buffer_usage usage;
	usage.usage_flags = usage_flags;
	usage.property_flags = property_flags;
	usage.image_flags = image_flags;
	zest_key usage_hash = zest_Hash(ZestHasher, &usage, sizeof(zest_buffer_usage), ZEST_HASH_SEED);
	if (zest_map_valid_key(ZestDevice->pool_sizes, usage_hash)) {
		return *zest_map_at_key(ZestDevice->pool_sizes, usage_hash);
	}
	zest_buffer_pool_size pool_size = { 0 };
	return pool_size;
}

void zest_SetDevicePoolSize(VkBufferUsageFlags usage_flags, VkMemoryPropertyFlags property_flags, VkImageUsageFlags image_flags, zest_size minimum_allocation, zest_size pool_size) {
	ZEST_ASSERT(minimum_allocation > tloc__MEMORY_ALIGNMENT);	//minimum_allocation must be higher then the memoryalignment
	ZEST_ASSERT(pool_size);					//Must set a pool size
	ZEST_ASSERT(ZEST_POW2(pool_size));		//Pool size must be a power of 2
	zest_index size_index = ZEST__MAX(tloc__scan_forward(pool_size) - 20, 0);
	minimum_allocation = ZEST__MAX(minimum_allocation, 64 << size_index);
	zest_buffer_usage usage;
	usage.usage_flags = usage_flags;
	usage.property_flags = property_flags;
	usage.image_flags = image_flags;
	zest_key usage_hash = zest_Hash(ZestHasher, &usage, sizeof(zest_buffer_usage), ZEST_HASH_SEED);
	zest_buffer_pool_size pool_sizes;
	pool_sizes.minimum_allocation_size = minimum_allocation;
	pool_sizes.pool_size = pool_size;
	zest_map_insert_key(ZestDevice->pool_sizes, usage_hash, pool_sizes);
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
	for (ZEST_EACH_FIF_i) {
		ZEST_VK_CHECK_RESULT(vkCreateFence(ZestDevice->logical_device, &fence_info, &ZestDevice->allocation_callbacks, &ZestRenderer->fif_fence[i]));
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

	ZEST_VK_CHECK_RESULT(vkCreateSwapchainKHR(ZestDevice->logical_device, &createInfo, &ZestDevice->allocation_callbacks, &ZestRenderer->swapchain));

	vkGetSwapchainImagesKHR(ZestDevice->logical_device, ZestRenderer->swapchain, &image_count, ZEST_NULL);
	zest_vec_resize(ZestRenderer->swapchain_images, image_count);
	vkGetSwapchainImagesKHR(ZestDevice->logical_device, ZestRenderer->swapchain, &image_count, ZestRenderer->swapchain_images);
}

VkPresentModeKHR zest_choose_present_mode(VkPresentModeKHR *available_present_modes, zest_bool use_vsync) {
	VkPresentModeKHR best_mode = VK_PRESENT_MODE_FIFO_KHR;

	if (use_vsync) {
		return best_mode;
	}

	for (zest_foreach_i(available_present_modes)) {
		if (available_present_modes[i] == VK_PRESENT_MODE_MAILBOX_KHR) {
			return available_present_modes[i];
		}
		else if (available_present_modes[i] == VK_PRESENT_MODE_IMMEDIATE_KHR) {
            return available_present_modes[i];
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

	vkDestroyImageView(ZestDevice->logical_device, ZestRenderer->final_render_pass.depth.view, &ZestDevice->allocation_callbacks);
	vkDestroyImage(ZestDevice->logical_device, ZestRenderer->final_render_pass.depth.image, &ZestDevice->allocation_callbacks);

	for (zest_foreach_i(ZestRenderer->swapchain_frame_buffers)) {
		vkDestroyFramebuffer(ZestDevice->logical_device, ZestRenderer->swapchain_frame_buffers[i], &ZestDevice->allocation_callbacks);
	}

	//vkFreeCommandBuffers(ZestDevice->logical_device, ZestRenderer->present_command_pool, 1, &ZestRenderer->final_render_pass.command_buffer);

	for (zest_foreach_i(ZestRenderer->swapchain_image_views)) {
		vkDestroyImageView(ZestDevice->logical_device, ZestRenderer->swapchain_image_views[i], &ZestDevice->allocation_callbacks);
	}

	vkDestroySwapchainKHR(ZestDevice->logical_device, ZestRenderer->swapchain, &ZestDevice->allocation_callbacks);
}

void zest__destroy_pipeline_set(zest_pipeline_set *p) {
	vkDestroyPipeline(ZestDevice->logical_device, p->pipeline, &ZestDevice->allocation_callbacks);
	vkDestroyPipelineLayout(ZestDevice->logical_device, p->pipeline_layout, &ZestDevice->allocation_callbacks);
}

void zest__cleanup_pipelines() {
	for (zest_map_foreach_i(ZestRenderer->pipeline_sets)) {
		zest__destroy_pipeline_set(&ZestRenderer->pipeline_sets.data[i]);
	}
}

void zest__cleanup_textures() {
	for (zest_map_foreach_i(ZestRenderer->textures)) {
		zest_texture *texture = zest_map_at_index(ZestRenderer->textures, i);
		if (texture->flags & zest_texture_flag_textures_ready) {
			zest__cleanup_texture(texture);
		}
	}
}

void zest__cleanup_texture(zest_texture *texture) {
	//It would be nice if we didn't have to wait for the device to be idle here, but images can't be destroyed if they're currently used
	//in a command buffer
	zest_WaitForIdleDevice();
	vkDestroySampler(ZestDevice->logical_device, texture->sampler, &ZestDevice->allocation_callbacks);
	vkDestroyImageView(ZestDevice->logical_device, texture->frame_buffer.view, &ZestDevice->allocation_callbacks);
	vkDestroyImage(ZestDevice->logical_device, texture->frame_buffer.image, &ZestDevice->allocation_callbacks);
	if (texture->stream_staging_buffer != ZEST_NULL) {
		tloc_FreeRemote(texture->stream_staging_buffer->buffer_allocator->allocator, texture->stream_staging_buffer);
		texture->stream_staging_buffer = ZEST_NULL;
	}
	tloc_FreeRemote(texture->frame_buffer.buffer->buffer_allocator->allocator, texture->frame_buffer.buffer);
	texture->flags &= ~zest_texture_flag_textures_ready;
}

void zest__cleanup_renderer() {
	zest__cleanup_swapchain();
	vkDestroyDescriptorPool(ZestDevice->logical_device, ZestRenderer->descriptor_pool, &ZestDevice->allocation_callbacks);

	zest__cleanup_pipelines();
	zest_map_clear(ZestRenderer->pipeline_sets);

	zest__cleanup_textures();

	//for (auto& render_target : ZestRenderer->render_targets.data) {
		//render_target.CleanUp();
	//}
	//ZestRenderer->render_targets.Clear();

	for (zest_map_foreach_i(ZestRenderer->render_passes)) {
		vkDestroyRenderPass(ZestDevice->logical_device, ZestRenderer->render_passes.data[i].render_pass, &ZestDevice->allocation_callbacks);
	}
	zest_map_clear(ZestRenderer->render_passes);

	//for (auto& draw_routine : ZestRenderer->draw_routines.data) {
		//if (draw_routine.clean_up_callback) {
			//draw_routine.clean_up_callback(draw_routine);
		//}
	//}
	zest_map_clear(ZestRenderer->draw_routines);

	for (zest_map_foreach_i(ZestRenderer->descriptor_layouts)) {
		vkDestroyDescriptorSetLayout(ZestDevice->logical_device, ZestRenderer->descriptor_layouts.data[i].descriptor_layout, &ZestDevice->allocation_callbacks);
	}
	zest_map_clear(ZestRenderer->descriptor_layouts);


	for (ZEST_EACH_FIF_i) {
		vkDestroySemaphore(ZestDevice->logical_device, ZestRenderer->semaphores[i].present_complete, &ZestDevice->allocation_callbacks);
		vkDestroyFence(ZestDevice->logical_device, ZestRenderer->fif_fence[i], &ZestDevice->allocation_callbacks);
	}

	for (zest_map_foreach_i(ZestRenderer->buffer_allocators)) {
		for (zest_foreach_j(ZestRenderer->buffer_allocators.data[i].memory_pools)) {
			zest__destroy_memory(&ZestRenderer->buffer_allocators.data[i].memory_pools[j]);
		}
	}

	//for (auto &buffer : buffers) {
		//buffer.CleanUp();
	//}

	for (zest_map_foreach_i(ZestRenderer->command_queues)) {
		zest__cleanup_command_queue(zest_map_at_index(ZestRenderer->command_queues, i));
	}
	zest__cleanup_command_queue(&ZestRenderer->empty_queue);

	vkDestroyCommandPool(ZestDevice->logical_device, ZestRenderer->present_command_pool, &ZestDevice->allocation_callbacks);
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

	for (zest_map_foreach_i(ZestRenderer->draw_routines)) {
		zest_draw_routine *draw_routine = zest_map_at_index(ZestRenderer->draw_routines, i);
		if (!draw_routine->update_resolution_callback && draw_routine->draw_index != -1) {
			zest_instance_layer *layer = zest_GetLayerByIndex(draw_routine->draw_index);
			zest__update_draw_layer_resolution(layer);
		}
		else if (draw_routine->update_resolution_callback)
			draw_routine->update_resolution_callback(draw_routine);
	}
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
	*/

	for (zest_map_foreach_i(ZestRenderer->command_queues)) {
		zest_command_queue *command_queue = zest_map_at_index(ZestRenderer->command_queues, i);
		for (zest_foreach_j(command_queue->render_commands)) {
			zest_index render_command_index = command_queue->render_commands[j];
			zest_command_queue_draw_commands *render_command = zest_map_at_index(ZestRenderer->command_queue_render_passes, render_command_index);
			if (render_command->viewport_type == zest_render_viewport_type_scale_with_window) {
				render_command->viewport.extent.width = (zest_uint)((float)width * render_command->viewport_scale.x);
				render_command->viewport.extent.height = (zest_uint)((float)height * render_command->viewport_scale.y);
			}
		}
	}

	for (zest_foreach_i(ZestRenderer->empty_queue.render_commands)) {
		zest_index render_command_index = ZestRenderer->empty_queue.render_commands[i];
		zest_command_queue_draw_commands *render_command = zest_map_at_index(ZestRenderer->command_queue_render_passes, render_command_index);
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

			ZEST_VK_CHECK_RESULT(vkCreateFramebuffer(ZestDevice->logical_device, &frame_buffer_info, &ZestDevice->allocation_callbacks, &ZestRenderer->swapchain_frame_buffers[i]));

	}
}

void zest__create_sync_objects() {
	VkSemaphoreCreateInfo semaphore_info = { 0 };
	semaphore_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

	VkFenceCreateInfo fence_info = { 0 };
	fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fence_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;

	for (ZEST_EACH_FIF_i) {
		ZEST_VK_CHECK_RESULT(vkCreateSemaphore(ZestDevice->logical_device, &semaphore_info, &ZestDevice->allocation_callbacks, &ZestRenderer->semaphores[i].present_complete));
	}
}

zest_index zest_create_uniform_buffer(const char *name, zest_size uniform_struct_size) {
	zest_uniform_buffer uniform_buffer;
	zest_buffer_info buffer_info = { 0 };
	buffer_info.usage_flags = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
	buffer_info.property_flags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
	for (ZEST_EACH_FIF_i) {
		uniform_buffer.buffer[i] = zest_CreateBuffer(uniform_struct_size, &buffer_info, ZEST_NULL);
		uniform_buffer.view_buffer_info[i].buffer = *zest_GetBufferDeviceBuffer(uniform_buffer.buffer[i]);
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
	ZEST_VK_CHECK_RESULT(vkCreateCommandPool(ZestDevice->logical_device, &cmdPoolInfo, &ZestDevice->allocation_callbacks, &ZestRenderer->present_command_pool));
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

	ZEST_VK_CHECK_RESULT(vkCreateDescriptorPool(ZestDevice->logical_device, &pool_info, &ZestDevice->allocation_callbacks, &ZestRenderer->descriptor_pool));
}

VkRenderPass zest_GetRenderPassByIndex(zest_index index) { ZEST_ASSERT(zest_map_valid_index(ZestRenderer->render_passes, index)); return *zest_map_at_index(ZestRenderer->render_passes, index).render_pass; }

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

	ZEST_VK_CHECK_RESULT(vkCreateDescriptorSetLayout(ZestDevice->logical_device, &layoutInfo, &ZestDevice->allocation_callbacks, &layout) != VK_SUCCESS);

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

VkWriteDescriptorSet zest_CreateBufferDescriptorWriteWithType(VkDescriptorSet descriptor_set, VkDescriptorBufferInfo *view_buffer_info, zest_uint dst_binding, VkDescriptorType type) {
	VkWriteDescriptorSet write = { 0 };
	write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	write.dstSet = descriptor_set;
	write.dstBinding = dst_binding;
	write.dstArrayElement = 0;
	write.descriptorType = type;
	write.descriptorCount = 1;
	write.pBufferInfo = view_buffer_info;
	return write;
}

VkWriteDescriptorSet zest_CreateImageDescriptorWriteWithType(VkDescriptorSet descriptor_set, VkDescriptorImageInfo *view_image_info, zest_uint dst_binding, VkDescriptorType type) {
	VkWriteDescriptorSet write = { 0 };
	write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	write.dstSet = descriptor_set;
	write.dstBinding = dst_binding;
	write.dstArrayElement = 0;
	write.descriptorType = type;
	write.descriptorCount = 1;
	write.pImageInfo = view_image_info;
	return write;
}

void zest_AllocateDescriptorSet(VkDescriptorPool descriptor_pool, VkDescriptorSetLayout descriptor_layout, VkDescriptorSet *descriptor_set) {
	VkDescriptorSetAllocateInfo alloc_info = { 0 };
	alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	alloc_info.descriptorPool = descriptor_pool;
	alloc_info.descriptorSetCount = 1;
	alloc_info.pSetLayouts = &descriptor_layout;

	ZEST_VK_CHECK_RESULT(vkAllocateDescriptorSets(ZestDevice->logical_device, &alloc_info, descriptor_set));
}

void zest_UpdateDescriptorSet(VkWriteDescriptorSet *descriptor_writes) {
	vkUpdateDescriptorSets(ZestDevice->logical_device, zest_vec_size(descriptor_writes), descriptor_writes, 0, ZEST_NULL);
}

zest_pipeline_set zest_CreatePipelineSet() {
	zest_pipeline_set pipeline_set = {
		.create_info = {0},
		.pipeline_template = {0},
		.descriptor_layout = 0,
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
		.descriptorSetLayout = 0,
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

VkVertexInputBindingDescription zest_CreateVertexInputBindingDescription(zest_uint binding, zest_uint stride, VkVertexInputRate input_rate) {
	VkVertexInputBindingDescription inpute_binding_description = { 0 };
	inpute_binding_description.binding = binding;
	inpute_binding_description.stride = stride;
	inpute_binding_description.inputRate = input_rate;
	return inpute_binding_description;
}

VkVertexInputAttributeDescription zest_CreateVertexInputDescription(zest_uint binding, zest_uint location, VkFormat format, zest_uint offset) {
	VkVertexInputAttributeDescription inpute_attribute_description = { 0 };
	inpute_attribute_description.location = location;
	inpute_attribute_description.binding = binding;
	inpute_attribute_description.format = format;
	inpute_attribute_description.offset = offset;
	return inpute_attribute_description;
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

void zest_BindPipeline(VkCommandBuffer command_buffer, zest_pipeline_set *pipeline, VkDescriptorSet descriptor_set) {
	vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->pipeline);
	vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->pipeline_layout, 0, 1, &descriptor_set, 0, 0);
}

zest_pipeline_set *zest_Pipeline(zest_index pipeline_index) {
	ZEST_ASSERT(zest_map_valid_index(ZestRenderer->pipeline_sets, pipeline_index));	//That index could not be found in the pipeline storage
	return zest_map_at_index(ZestRenderer->pipeline_sets, pipeline_index);
}

zest_index zest_PipelineIndex(const char *name) {
	ZEST_ASSERT(zest_map_valid_name(ZestRenderer->pipeline_sets, name));	//That index could not be found in the pipeline storage
	return zest_map_get_index_by_name(ZestRenderer->pipeline_sets, name);
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

void zest_BuildPipeline(zest_pipeline_set *pipeline) {
	ZEST_VK_CHECK_RESULT(vkCreatePipelineLayout(ZestDevice->logical_device, &pipeline->pipeline_template.pipelineLayoutInfo, &ZestDevice->allocation_callbacks, &pipeline->pipeline_layout));

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

	ZEST_VK_CHECK_RESULT(vkCreateGraphicsPipelines(ZestDevice->logical_device, VK_NULL_HANDLE, 1, &pipeline_info, &ZestDevice->allocation_callbacks, &pipeline->pipeline));

	vkDestroyShaderModule(ZestDevice->logical_device, frag_shader_module, &ZestDevice->allocation_callbacks);
	vkDestroyShaderModule(ZestDevice->logical_device, vert_shader_module, &ZestDevice->allocation_callbacks);
}

void zest_MakePipelineTemplate(zest_pipeline_set *pipeline, VkRenderPass render_pass, zest_pipeline_template_create_info *create_info) {
	ZEST_ASSERT(zest_map_valid_index(ZestRenderer->descriptor_layouts, pipeline->descriptor_layout));	//Must be a valid descriptor layout index in the pipeline

	if (!(pipeline->flags & zest_pipeline_set_flag_is_render_target_pipeline))
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
	ZEST_VK_CHECK_RESULT(vkCreateShaderModule(ZestDevice->logical_device, &create_info, &ZestDevice->allocation_callbacks, &shader_module));

	return shader_module;
}

void zest__rebuild_pipeline(zest_pipeline_set *pipeline) {
	//Note: not setting this for pipelines messes with scaling, but not sure if some pipelines need this to be fixed
	pipeline->create_info.viewport.extent = zest_GetSwapChainExtent();
	pipeline->pipeline_template.renderPass = zest_GetStandardRenderPass();
	zest_UpdatePipelineTemplate(&pipeline->pipeline_template, &pipeline->create_info);
	zest_BuildPipeline(pipeline);
	for (ZEST_EACH_FIF_i) {
		zest_vec_clear(pipeline->descriptor_writes);
	}
}

//Inline API functions
zest_index zest_AddPipeline(const char *name) { zest_map_insert(ZestRenderer->pipeline_sets, name, zest_CreatePipelineSet()); return zest_map_last_index(ZestRenderer->pipeline_sets); }

VkRenderPass zest_GetStandardRenderPass() { return *zest_map_at(ZestRenderer->render_passes, "Render pass standard").render_pass; }

zest_key zest_Hash(zest_hasher *hasher, const void* input, zest_ull length, zest_ull seed) { zest__hash_initialise(hasher, seed); zest__hasher_add(hasher, input, length); return (zest_key)zest__get_hash(hasher); }

VkFramebuffer zest_GetRendererFrameBuffer(zest_command_queue_draw_commands *item) { return ZestRenderer->swapchain_frame_buffers[ZestRenderer->current_frame]; }
VkDescriptorSetLayout *zest_GetDescriptorSetLayoutByIndex(zest_index index) { return zest_map_at_index(ZestRenderer->descriptor_layouts, index).descriptor_layout; }
VkDescriptorSetLayout *zest_GetDescriptorSetLayoutByName(const char *name) { return zest_map_at(ZestRenderer->descriptor_layouts, name).descriptor_layout; }
VkDescriptorBufferInfo *zest_GetUniformBufferInfoByName(const char *name, zest_index fif) { ZEST_ASSERT(zest_map_valid_name(ZestRenderer->uniform_buffers, name)); return zest_map_at(ZestRenderer->uniform_buffers, name).view_buffer_info[fif]; }
VkDescriptorBufferInfo *zest_GetUniformBufferInfoByIndex(zest_index index, zest_index fif) { ZEST_ASSERT(zest_map_valid_index(ZestRenderer->uniform_buffers, index)); return zest_map_at_index(ZestRenderer->uniform_buffers, index).view_buffer_info[fif]; }
VkRenderPass zest_GetRenderPassByName(const char *name) { ZEST_ASSERT(zest_map_valid_name(ZestRenderer->render_passes, name)); return *zest_map_at(ZestRenderer->render_passes, name).render_pass; }
zest_pipeline_set *zest_PipelineByIndex(zest_index index) { ZEST_ASSERT(zest_map_valid_index(ZestRenderer->pipeline_sets, index)); return zest_map_at_index(ZestRenderer->pipeline_sets, index); }
zest_pipeline_set *zest_PipelineByName(const char *name) { ZEST_ASSERT(zest_map_valid_name(ZestRenderer->pipeline_sets, name)); return zest_map_at(ZestRenderer->pipeline_sets, name); }
zest_pipeline_template_create_info zest_PipelineCreateInfo(const char *name) { ZEST_ASSERT(zest_map_valid_name(ZestRenderer->pipeline_sets, name)); zest_pipeline_set *pipeline = zest_map_at(ZestRenderer->pipeline_sets, name); return pipeline->create_info; }
VkExtent2D zest_GetSwapChainExtent() { return ZestRenderer->swapchain_extent; }
zest_uint zest_ScreenWidth() { return ZestApp->window->window_width; }
zest_uint zest_ScreenHeight() { return ZestApp->window->window_height; }
float zest_ScreenWidthf() { return (float)ZestApp->window->window_width; }
float zest_ScreenHeightf() { return (float)ZestApp->window->window_height; }
zest_uniform_buffer *zest_GetUniformBuffer(zest_index index) { ZEST_ASSERT(zest_map_valid_index(ZestRenderer->uniform_buffers, index)); return zest_map_at_index(ZestRenderer->uniform_buffers, index); }
zest_uniform_buffer *zest_GetUniformBufferByName(const char *name) { ZEST_ASSERT(zest_map_valid_name(ZestRenderer->uniform_buffers, name)); return zest_map_at(ZestRenderer->uniform_buffers, name); }
zest_bool zest_UniformBufferExists(const char *name) { return zest_map_valid_name(ZestRenderer->uniform_buffers, name); }
void zest_WaitForIdleDevice() { vkDeviceWaitIdle(ZestDevice->logical_device); }
void zest_ShowFPSInTitle() {
	ZestApp->flags |= zest_app_flag_show_fps_in_title;
}
void zest_HideFPSInTitle() {
	ZestApp->flags &= ~zest_app_flag_show_fps_in_title;
}

void zest__hash_initialise(zest_hasher *hasher, zest_ull seed) { hasher->state[0] = seed + zest__PRIME1 + zest__PRIME2; hasher->state[1] = seed + zest__PRIME2; hasher->state[2] = seed; hasher->state[3] = seed - zest__PRIME1; hasher->buffer_size = 0; hasher->total_length = 0; }

zest_uint zest__grow_capacity(void *T, zest_uint size) { zest_uint new_capacity = T ? (size + size / 2) : 8; return new_capacity > size ? new_capacity : size; }
void* zest__vec_reserve(void *T, zest_uint unit_size, zest_uint new_capacity) { if (T && new_capacity <= zest__vec_header(T)->capacity) return T; void* new_data = ZEST__REALLOCATE((T ? zest__vec_header(T) : T), new_capacity * unit_size + zest__VEC_HEADER_OVERHEAD); if (!T) memset(new_data, 0, zest__VEC_HEADER_OVERHEAD); T = ((char*)new_data + zest__VEC_HEADER_OVERHEAD); ((zest_vec*)(new_data))->capacity = new_capacity; return T; }


#ifdef _WIN32
zest_millisecs zest_Millisecs(void) { FILETIME ft; GetSystemTimeAsFileTime(&ft); ULARGE_INTEGER time; time.LowPart = ft.dwLowDateTime; time.HighPart = ft.dwHighDateTime; zest_ull ms = time.QuadPart / 10000ULL; return (zest_millisecs)ms; }
zest_microsecs zest_Microsecs(void) { FILETIME ft; GetSystemTimeAsFileTime(&ft); ULARGE_INTEGER time; time.LowPart = ft.dwLowDateTime; time.HighPart = ft.dwHighDateTime; zest_ull us = time.QuadPart / 10ULL; return (zest_microsecs)us; }
FILE *zest__open_file(const char *file_name, const char *mode) {
	FILE *file = NULL;
	errno_t err = fopen_s(&file, file_name, mode);
	if (err != 0 || file == NULL) {
		return NULL;
	}
	return file;
}
#else
zest_millisecs zest_Millisecs(void) { struct timespec now; clock_gettime(CLOCK_REALTIME, &now); long m = now.tv_sec * 1000 + now.tv_nsec / 1000000; return (zest_millisecs)m; }
zest_microsecs zest_Microsecs(void) { struct timespec now; clock_gettime(CLOCK_REALTIME, &now); zest_ull us = now.tv_sec * 1000000ULL + now.tv_nsec / 1000; return (zest_microsecs)us; }
FILE *zest__open_file(const char *file_name, const char *mode) {
	return fopen(file_name, mode);
}
#endif

//End api functions

void zest__prepare_standard_pipelines() {
	VkRenderPass render_pass = zest_GetStandardRenderPass();

	zest_pipeline_template_create_info create_info = zest_CreatePipelineTemplateCreateInfo();
	create_info.viewport.extent = zest_GetSwapChainExtent();

	VkPushConstantRange image_pushconstant_range;
	image_pushconstant_range.size = sizeof(zest_push_constants);
	image_pushconstant_range.offset = 0;
	image_pushconstant_range.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
	zest_vec_push(create_info.pushConstantRange, image_pushconstant_range);

	//2d sprite rendering
	zest_index index = zest_AddPipeline("pipeline_2d_sprites");
	zest_pipeline_set *sprite_instance_pipeline = zest_Pipeline(index);
	zest_pipeline_template_create_info instance_create_info = create_info;
	//instance_create_info.bindingDescriptions.push_back(CreateVertexInputBindingDescription(0, sizeof(InstanceVertex), VK_VERTEX_INPUT_RATE_VERTEX));
	instance_create_info.bindingDescription = zest_CreateVertexInputBindingDescription(0, sizeof(zest_sprite_instance), VK_VERTEX_INPUT_RATE_INSTANCE);
	VkVertexInputAttributeDescription *instance_vertex_input_attributes = 0;

	zest_vec_push(instance_vertex_input_attributes, zest_CreateVertexInputDescription(0, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(zest_sprite_instance, size)));					// Location 0: Size of the sprite in pixels
	zest_vec_push(instance_vertex_input_attributes, zest_CreateVertexInputDescription(0, 1, VK_FORMAT_R32G32_SFLOAT, offsetof(zest_sprite_instance, handle)));					// Location 1: Handle of the sprite
	zest_vec_push(instance_vertex_input_attributes, zest_CreateVertexInputDescription(0, 2, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(zest_sprite_instance, uv)));				// Location 2: UV coords
	zest_vec_push(instance_vertex_input_attributes, zest_CreateVertexInputDescription(0, 3, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(zest_sprite_instance, position_rotation)));	// Location 3: Instance Position and rotation
	zest_vec_push(instance_vertex_input_attributes, zest_CreateVertexInputDescription(0, 4, VK_FORMAT_R32_SFLOAT, offsetof(zest_sprite_instance, intensity)));					// Location 4: Intensity
	zest_vec_push(instance_vertex_input_attributes, zest_CreateVertexInputDescription(0, 5, VK_FORMAT_R16G16_SNORM, offsetof(zest_sprite_instance, alignment)));				// Location 5: Alignment
	zest_vec_push(instance_vertex_input_attributes, zest_CreateVertexInputDescription(0, 6, VK_FORMAT_R8G8B8A8_UNORM, offsetof(zest_sprite_instance, color)));					// Location 6: Instance Color
	zest_vec_push(instance_vertex_input_attributes, zest_CreateVertexInputDescription(0, 7, VK_FORMAT_R32_UINT, offsetof(zest_sprite_instance, image_layer_index)));			// Location 7: Instance Parameters

	instance_create_info.attributeDescriptions = instance_vertex_input_attributes;
	instance_create_info.vertShaderFile = "spv/instance.spv";
	instance_create_info.fragShaderFile = "spv/instance.spv";
	zest_MakePipelineTemplate(sprite_instance_pipeline, render_pass, &instance_create_info);
	sprite_instance_pipeline->pipeline_template.colorBlendAttachment = zest_PreMultiplyBlendState();
	sprite_instance_pipeline->pipeline_template.depthStencil.depthWriteEnable = VK_FALSE;
	zest_BuildPipeline(sprite_instance_pipeline);

	//3d billboards
	instance_create_info.bindingDescription = zest_CreateVertexInputBindingDescription(0, sizeof(zest_billboard_instance), VK_VERTEX_INPUT_RATE_INSTANCE);
	VkVertexInputAttributeDescription *billboard_vertex_input_attributes = 0;

	zest_vec_push(billboard_vertex_input_attributes, zest_CreateVertexInputDescription(0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(zest_billboard_instance, position)));			// Location 0: Position
	zest_vec_push(billboard_vertex_input_attributes, zest_CreateVertexInputDescription(0, 1, VK_FORMAT_R16G16_SNORM, offsetof(zest_billboard_instance, uv_xy)));				// Location 1: uv_xy
	zest_vec_push(billboard_vertex_input_attributes, zest_CreateVertexInputDescription(0, 2, VK_FORMAT_R32G32B32_SFLOAT, offsetof(zest_billboard_instance, rotations)));		// Location 2: scale pitch yaw
	zest_vec_push(billboard_vertex_input_attributes, zest_CreateVertexInputDescription(0, 3, VK_FORMAT_R16G16_SNORM, offsetof(zest_billboard_instance, uv_zw)));				// Location 3: uv_zw
	zest_vec_push(billboard_vertex_input_attributes, zest_CreateVertexInputDescription(0, 4, VK_FORMAT_R32G32_SFLOAT, offsetof(zest_billboard_instance, scale)));				// Location 4: Handle
	zest_vec_push(billboard_vertex_input_attributes, zest_CreateVertexInputDescription(0, 5, VK_FORMAT_R32G32_SFLOAT, offsetof(zest_billboard_instance, handle)));				// Location 5: Handle
	zest_vec_push(billboard_vertex_input_attributes, zest_CreateVertexInputDescription(0, 6, VK_FORMAT_R32_SFLOAT, offsetof(zest_billboard_instance, stretch)));				// Location 6: Stretch amount
	zest_vec_push(billboard_vertex_input_attributes, zest_CreateVertexInputDescription(0, 7, VK_FORMAT_R32_UINT, offsetof(zest_billboard_instance, blend_texture_array)));		// Location 7: texture array index
	zest_vec_push(billboard_vertex_input_attributes, zest_CreateVertexInputDescription(0, 8, VK_FORMAT_R8G8B8A8_UNORM, offsetof(zest_billboard_instance, color)));				// Location 8: Instance Color
	zest_vec_push(billboard_vertex_input_attributes, zest_CreateVertexInputDescription(0, 9, VK_FORMAT_A2R10G10B10_SNORM_PACK32, offsetof(zest_billboard_instance, alignment)));// Location 9: Alignment

	index = zest_AddPipeline("pipeline_billboard");
	instance_create_info.attributeDescriptions = billboard_vertex_input_attributes;
	zest_pipeline_set *billboard_instance_pipeline = zest_Pipeline(index);
	instance_create_info.vertShaderFile = "spv/billboard.spv";
	instance_create_info.fragShaderFile = "spv/billboard.spv";
	zest_MakePipelineTemplate(billboard_instance_pipeline, render_pass, &instance_create_info);
	billboard_instance_pipeline->pipeline_template.depthStencil.depthWriteEnable = VK_FALSE;
	zest_BuildPipeline(billboard_instance_pipeline);

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

		vkCmdEndRenderPass(ZestRenderer->present_command_buffers[i]);

		ZEST_VK_CHECK_RESULT(vkEndCommandBuffer(ZestRenderer->present_command_buffers[i]));
	}
}

void zest__create_empty_command_queue(zest_command_queue *command_queue) {
	VkCommandPoolCreateInfo cmd_pool_info = { 0 };
	cmd_pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	cmd_pool_info.queueFamilyIndex = ZestDevice->graphics_queue_family_index;
	cmd_pool_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
	ZEST_VK_CHECK_RESULT(vkCreateCommandPool(ZestDevice->logical_device, &cmd_pool_info, &ZestDevice->allocation_callbacks, &command_queue->command_pool));

	VkCommandBufferAllocateInfo alloc_info = { 0 };
	alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	alloc_info.commandBufferCount = ZEST_MAX_FIF;
	alloc_info.commandPool = command_queue->command_pool;
	ZEST_VK_CHECK_RESULT(vkAllocateCommandBuffers(ZestDevice->logical_device, &alloc_info, command_queue->command_buffer));

	for (ZEST_EACH_FIF_i) {
		zest_vec_push(command_queue->fif_incoming_semaphores[i], ZestRenderer->semaphores[i].present_complete);
		zest_vec_push(command_queue->fif_stage_flags[i], VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
	}
	zest_command_queue_draw_commands render_commands = { 0 };
	render_commands.render_pass = ZestRenderer->final_render_pass.render_pass;
	render_commands.get_frame_buffer = zest_GetRendererFrameBuffer;
	render_commands.render_pass_function = zest__render_blank;
	render_commands.viewport.extent = zest_GetSwapChainExtent();
	render_commands.viewport.offset.x = render_commands.viewport.offset.y = 0;
	render_commands.viewport_scale.x = 1.f;
	render_commands.viewport_scale.y = 1.f;
	zest_ConnectCommandQueueToPresent(command_queue, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
	zest_map_insert(ZestRenderer->command_queue_render_passes, "Blank Render Pass", render_commands);
	zest_vec_push(command_queue->render_commands, zest_map_last_index(ZestRenderer->command_queue_render_passes));
}

void zest__render_blank(zest_command_queue_draw_commands *item, VkCommandBuffer command_buffer, zest_index render_pass, VkFramebuffer framebuffer) {
	VkRenderPassBeginInfo render_pass_info = { 0 };
	render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	render_pass_info.renderPass = zest_GetRenderPassByIndex(render_pass);
	render_pass_info.framebuffer = framebuffer;
	render_pass_info.renderArea = item->viewport;

	VkClearValue clear_values[2] = {
		[0].color = { .float32[0] = 0.1f, .float32[1] = 0.0f, .float32[2] = 0.1f, .float32[3] = 1.f },
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

void zest__add_draw_routine(zest_command_queue_draw_commands *command_queue_draw, zest_draw_routine *draw_routine) {
	zest_vec_push(command_queue_draw->draw_routines, draw_routine->routine_id);
	draw_routine->cq_render_pass_index = zest_vec_size(command_queue_draw->draw_routines) - 1;
}

void zest__acquire_next_swapchain_image() {

	VkResult result = vkAcquireNextImageKHR(ZestDevice->logical_device, ZestRenderer->swapchain, UINT64_MAX, ZestRenderer->semaphores[ZEST_FIF].present_complete, ZEST_NULL, &ZestRenderer->current_frame);

	//Has the window been resized? if so rebuild the swap chain.
	if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
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

void zest_RenderDrawRoutinesCallback(zest_command_queue_draw_commands *item, VkCommandBuffer command_buffer, zest_index render_pass, VkFramebuffer framebuffer) {
	VkRenderPassBeginInfo renderPassInfo = { 0 };
	renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	renderPassInfo.renderPass = zest_GetRenderPassByIndex(render_pass);
	renderPassInfo.framebuffer = framebuffer;
	renderPassInfo.renderArea = item->viewport;
	VkClearValue clear_values[2] = {
		[0].color = { .float32[0] = item->cls_color[0], .float32[1] = item->cls_color[1], .float32[2] = item->cls_color[2], .float32[3] = item->cls_color[4] },
		[1].depthStencil = {.depth = 1.0f,.stencil = 0 }
	};

	renderPassInfo.clearValueCount = 2;
	renderPassInfo.pClearValues = clear_values;

	//Copy the buffers
	zest_buffer_uploader vertex_upload = { 0 };

	for (zest_foreach_i(item->draw_routines)) {
		zest_index index = item->draw_routines[i];
		zest_draw_routine *draw_routine = zest_GetDrawRoutineByIndex(index);
		if (draw_routine->update_buffers_callback) {
			draw_routine->update_buffers_callback(draw_routine, &vertex_upload);
		}
	}

	zest_UploadBuffer(&vertex_upload, command_buffer);

	vkCmdBeginRenderPass(command_buffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

	VkViewport view = zest_CreateViewport((float)item->viewport.extent.width, (float)item->viewport.extent.height, 0.f, 1.f);
	VkRect2D scissor = zest_CreateRect2D(item->viewport.extent.width, item->viewport.extent.height, 0, 0);
	vkCmdSetViewport(command_buffer, 0, 1, &view);
	vkCmdSetScissor(command_buffer, 0, 1, &scissor);

	for (zest_foreach_i(item->draw_routines)) {
		zest_index index = item->draw_routines[i];
		zest_draw_routine *draw_routine = zest_GetDrawRoutineByIndex(index);
		if (draw_routine->draw_callback)
			draw_routine->draw_callback(draw_routine);
	}

	vkCmdEndRenderPass(command_buffer);

}

zest_draw_routine *zest_GetDrawRoutineByIndex(zest_index index) {
	assert(zest_map_valid_index(ZestRenderer->draw_routines, index));	//That index could not be found in the storage
	return zest_map_at_index(ZestRenderer->draw_routines, index);
}

zest_draw_routine *zest_GetDrawRoutineByName(const char *name) {
	assert(zest_map_valid_name(ZestRenderer->draw_routines, name));	//That index could not be found in the storage
	return zest_map_at(ZestRenderer->draw_routines, name);
}
// --End Renderer functions

// --Command Queue functions
void zest__cleanup_command_queue(zest_command_queue *command_queue) {	
	for (ZEST_EACH_FIF_i) {
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
		zest_command_queue_draw_commands *render_command = &ZestRenderer->command_queue_render_passes.data[command_queue->render_commands[i]];
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
	for (ZEST_EACH_FIF_i) {
		VkSemaphore semaphore = VK_NULL_HANDLE;
		ZEST_VK_CHECK_RESULT(vkCreateSemaphore(ZestDevice->logical_device, &semaphore_info, &ZestDevice->allocation_callbacks, &semaphore));
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
	ZEST_VK_CHECK_RESULT(vkCreateImageView(ZestDevice->logical_device, &viewInfo, &ZestDevice->allocation_callbacks, &image_view) != VK_SUCCESS);

	return image_view;
}

zest_buffer *zest__create_image(zest_uint width, zest_uint height, zest_uint mip_levels, VkSampleCountFlagBits numSamples, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkImage *image) {
	VkImageCreateInfo image_info = {0};
	image_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	image_info.imageType = VK_IMAGE_TYPE_2D;
	image_info.extent.width = width;
	image_info.extent.height = height;
	image_info.extent.depth = 1;
	image_info.mipLevels = mip_levels;
	image_info.arrayLayers = 1;
	image_info.format = format;
	image_info.tiling = tiling;
	image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	image_info.usage = usage;
	image_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	image_info.samples = numSamples;

	ZEST_VK_CHECK_RESULT(vkCreateImage(ZestDevice->logical_device, &image_info, &ZestDevice->allocation_callbacks, image));

	VkMemoryRequirements memory_requirements;
	vkGetImageMemoryRequirements(ZestDevice->logical_device, *image, &memory_requirements);

	zest_buffer_info buffer_info = { 0 };
	buffer_info.image_usage_flags = usage;
	buffer_info.property_flags = properties;
	zest_buffer *buffer = zest_CreateBuffer(memory_requirements.size, &buffer_info, *image);

	vkBindImageMemory(ZestDevice->logical_device, *image, zest_GetBufferDeviceMemory(buffer), buffer->memory_offset);

	return buffer;
}

zest_buffer *zest__create_image_array(zest_uint width, zest_uint height, zest_uint mipLevels, zest_uint layers, VkSampleCountFlagBits numSamples, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkImage *image) {
	VkImageCreateInfo image_info = { 0 };
	image_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	image_info.imageType = VK_IMAGE_TYPE_2D;
	image_info.extent.width = width;
	image_info.extent.height = height;
	image_info.extent.depth = 1;
	image_info.mipLevels = mipLevels;
	image_info.arrayLayers = layers;
	image_info.format = format;
	image_info.tiling = tiling;
	image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	image_info.usage = usage;
	image_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	image_info.samples = numSamples;

	ZEST_VK_CHECK_RESULT(vkCreateImage(ZestDevice->logical_device, &image_info, &ZestDevice->allocation_callbacks, image));

	VkMemoryRequirements memory_requirements;
	vkGetImageMemoryRequirements(ZestDevice->logical_device, *image, &memory_requirements);

	zest_buffer_info buffer_info = { 0 };
	buffer_info.image_usage_flags = usage;
	buffer_info.property_flags = properties;
	zest_buffer *buffer = zest_CreateBuffer(memory_requirements.size, &buffer_info, *image);

	vkBindImageMemory(ZestDevice->logical_device, *image, zest_GetBufferDeviceMemory(buffer), buffer->memory_offset);
	return buffer;
}

void zest__copy_buffer_to_image(VkBuffer buffer, VkDeviceSize src_offset, VkImage image, zest_uint width, zest_uint height, VkImageLayout image_layout) {
	VkCommandBuffer command_buffer = zest__begin_single_time_commands();

	VkBufferImageCopy region = { 0 };
	region.bufferOffset = src_offset;
	region.bufferRowLength = 0;
	region.bufferImageHeight = 0;

	region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	region.imageSubresource.mipLevel = 0;
	region.imageSubresource.baseArrayLayer = 0;
	region.imageSubresource.layerCount = 1;

	region.imageOffset.x = 0;
	region.imageOffset.y = 0;
	region.imageOffset.z = 0;
	region.imageExtent.width = width;
	region.imageExtent.height = height;
	region.imageExtent.depth = 1;

	vkCmdCopyBufferToImage( command_buffer, buffer, image, image_layout, 1, &region );

	zest__end_single_time_commands(command_buffer);
}

void zest__copy_buffer_regions_to_image(VkBufferImageCopy *regions, VkBuffer buffer, VkDeviceSize src_offset, VkImage image) {
	VkCommandBuffer command_buffer = zest__begin_single_time_commands();

	for (zest_foreach_i(regions)) {
		regions[i].bufferOffset += src_offset;
	}

	vkCmdCopyBufferToImage( command_buffer, buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, zest_vec_size(regions), regions );

	zest__end_single_time_commands(command_buffer);
}

void zest__generate_mipmaps(VkImage image, VkFormat image_format, int32_t texture_width, int32_t texture_height, zest_uint mip_levels, zest_uint layer_count, VkImageLayout image_layout) {
	VkFormatProperties format_properties;
	vkGetPhysicalDeviceFormatProperties(ZestDevice->physical_device, image_format, &format_properties);

	ZEST_ASSERT(format_properties.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT); //texture image format does not support linear blitting!

	VkCommandBuffer command_buffer = zest__begin_single_time_commands();

	VkImageMemoryBarrier barrier = { 0 };
	barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	barrier.image = image;
	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	barrier.subresourceRange.baseArrayLayer = 0;
	barrier.subresourceRange.layerCount = layer_count;
	barrier.subresourceRange.levelCount = 1;

	int32_t mip_width = texture_width;
	int32_t mip_height = texture_height;

	for (zest_uint i = 1; i < mip_levels; i++) {
		barrier.subresourceRange.baseMipLevel = i - 1;
		barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
		barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

		vkCmdPipelineBarrier(command_buffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, ZEST_NULL, 0, ZEST_NULL, 1, &barrier); 

		VkImageBlit blit = { 0 };
		blit.srcOffsets[0].x = 0;
		blit.srcOffsets[0].y = 0;
		blit.srcOffsets[0].z = 0;
		blit.srcOffsets[1].x = mip_width;
		blit.srcOffsets[1].y = mip_height;
		blit.srcOffsets[1].z = 1;
		blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		blit.srcSubresource.mipLevel = i - 1;
		blit.srcSubresource.baseArrayLayer = 0;
		blit.srcSubresource.layerCount = layer_count;
		blit.dstOffsets[0].x = 0;
		blit.dstOffsets[0].y = 0;
		blit.dstOffsets[0].z = 0;
		blit.dstOffsets[1].x = mip_width > 1 ? mip_width / 2 : 1;
		blit.dstOffsets[1].y = mip_height > 1 ? mip_height / 2 : 1;
		blit.dstOffsets[1].z = 1;
		blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		blit.dstSubresource.mipLevel = i;
		blit.dstSubresource.baseArrayLayer = 0;
		blit.dstSubresource.layerCount = layer_count;

		vkCmdBlitImage(command_buffer,
			image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
			image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			1, &blit,
			VK_FILTER_LINEAR);

		barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
		barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
		barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

		vkCmdPipelineBarrier(command_buffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, ZEST_NULL, 0, ZEST_NULL, 1, &barrier);

		if (mip_width > 1) mip_width /= 2;
		if (mip_height > 1) mip_height /= 2;
	}

	barrier.subresourceRange.baseMipLevel = mip_levels - 1;
	barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	barrier.newLayout = image_layout;
	barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
	barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

	vkCmdPipelineBarrier(command_buffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, ZEST_NULL, 0, ZEST_NULL, 1, &barrier);

	zest__end_single_time_commands(command_buffer);
}

void zest__transition_image_layout(VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout, zest_uint mip_levels, zest_uint layerCount) {
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
	barrier.subresourceRange.levelCount = mip_levels;
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
	ZEST_VK_CHECK_RESULT(vkCreateRenderPass(ZestDevice->logical_device, &render_pass_info, &ZestDevice->allocation_callbacks, &render_pass));

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

zest_index zest__next_fif() { 
	return (ZestDevice->current_fif + 1) % ZEST_MAX_FIF; 
}

zest_create_info zest_CreateInfo() {
	zest_create_info create_info = {
		.memory_pool_size = tloc__MEGABYTE(16),
		.screen_width = 1280, 
		.screen_height = 768,			
		.screen_x = 0, 
		.screen_y = 50,
		.virtual_width = 1280, 
		.virtual_height = 768,
		.color_format = VK_FORMAT_R8G8B8A8_UNORM,
		.pool_counts = ZEST_NULL,
		.flags = zest_init_flag_initialise_with_command_queue,
	};
	return create_info;
}

char* zest_ReadEntireFile(const char *file_name, zest_bool terminate) {
	char* buffer = 0;
	FILE *file = NULL;
    file = zest__open_file( file_name, "rb");
	if(!file) {
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

// --Command queue setup and modify functions
void zest__set_queue_context(zest_setup_context_type context) {
	if (ZestRenderer->setup_context.type == zest_setup_context_type_command_queue) {
		ZestRenderer->setup_context.render_pass_index = -1;
		ZestRenderer->setup_context.layer_index = -1;
		ZestRenderer->setup_context.draw_routine_index = -1;
		ZestRenderer->setup_context.compute_index = -1;
	}
	else if (ZestRenderer->setup_context.type == zest_setup_context_type_render_pass) {
		ZestRenderer->setup_context.layer_index = -1;
		ZestRenderer->setup_context.compute_index = -1;
		ZestRenderer->setup_context.draw_routine_index = -1;
	}
}

zest_index zest_NewCommandQueue(const char *name, zest_command_dependency_type dependency_type, zest_index dependency) {
	assert(ZestRenderer->setup_context.type == zest_setup_context_type_none);	//Current setup context must be none. You can't setup a new command queue within another one
	zest__set_queue_context(zest_setup_context_type_command_queue);
	zest_index command_queue_index = zest__create_command_queue(name);
	zest_command_queue *command_queue = zest_GetCommandQueue(command_queue_index);
	ZestRenderer->setup_context.command_queue_index = command_queue_index;
	ZestRenderer->setup_context.type = zest_setup_context_type_command_queue;
	if (dependency_type == zest_command_dependency_type_present) {
		zest_ConnectPresentToCommandQueue(command_queue, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
	}
	return command_queue_index;
}

zest_index zest__create_command_queue(const char *name) {
	zest_command_queue command_queue = { 0 };
	command_queue.name = name;

	VkCommandPoolCreateInfo cmd_pool_info = { 0 };
	cmd_pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	cmd_pool_info.queueFamilyIndex = ZestDevice->graphics_queue_family_index;
	cmd_pool_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
	ZEST_VK_CHECK_RESULT(vkCreateCommandPool(ZestDevice->logical_device, &cmd_pool_info, &ZestDevice->allocation_callbacks, &command_queue.command_pool));

	VkCommandBufferAllocateInfo alloc_info = { 0 };
	alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	alloc_info.commandBufferCount = ZEST_MAX_FIF;
	alloc_info.commandPool = command_queue.command_pool;
	ZEST_VK_CHECK_RESULT(vkAllocateCommandBuffers(ZestDevice->logical_device, &alloc_info, command_queue.command_buffer));

	zest_map_insert(ZestRenderer->command_queues, name, command_queue);
	zest_vec_back(ZestRenderer->command_queues.data).index_in_renderer = zest_map_last_index(ZestRenderer->command_queues);
	return zest_map_last_index(ZestRenderer->command_queues);
}

zest_command_queue *zest_GetCommandQueue(zest_index index) {
	ZEST_ASSERT(zest_map_valid_index(ZestRenderer->command_queues, index)); //Not a valid command queue index
	return zest_map_at_index(ZestRenderer->command_queues, index);
}

zest_command_queue_draw_commands *zest_GetCommandQueueRenderPass(zest_index index) {
	assert(zest_map_valid_index(ZestRenderer->command_queue_render_passes, index));	//index of command queue render pass not found
	return zest_map_at_index(ZestRenderer->command_queue_render_passes, index);
}

void zest_ConnectPresentToCommandQueue(zest_command_queue *receiver, VkPipelineStageFlags stage_flags) {
	VkSemaphoreCreateInfo semaphoreInfo = { 0 };
	semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
	for (ZEST_EACH_FIF_i) {
		zest_vec_push(receiver->fif_incoming_semaphores[i], ZestRenderer->semaphores[i].present_complete);
		zest_vec_push(receiver->fif_stage_flags[i], stage_flags);
	}
}

void zest_Finish() {
	ZestRenderer->setup_context.command_queue_index = -1;
	ZestRenderer->setup_context.render_pass_index = -1;
	ZestRenderer->setup_context.draw_routine_index = -1;
	ZestRenderer->setup_context.layer_index = -1;
	ZestRenderer->setup_context.type = zest_setup_context_type_none;
}

VkCommandBuffer zest_CurrentCommandBuffer() { 
	return ZestRenderer->current_command_buffer; 
}

zest_command_queue *zest_CurrentCommandQueue() { 
	return ZestRenderer->current_command_queue; 
}

zest_command_queue_draw_commands *zest_CurrentRenderPass() { 
	return ZestRenderer->current_render_pass; 
}


void zest_ConnectCommandQueues(zest_index sender_index, zest_index receiver_index, VkPipelineStageFlags stage_flags) {
	zest_command_queue *sender = zest_GetCommandQueue(sender_index);
	zest_command_queue *receiver = zest_GetCommandQueue(receiver_index);
	VkSemaphoreCreateInfo semaphoreInfo = { 0 };
	semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
	for (ZEST_EACH_FIF_i) {
		VkSemaphore semaphore = VK_NULL_HANDLE;
		ZEST_VK_CHECK_RESULT(vkCreateSemaphore(ZestDevice->logical_device, &semaphoreInfo, &ZestDevice->allocation_callbacks, &semaphore));
		zest_vec_push(sender->fif_outgoing_semaphores[i], semaphore);
		zest_vec_push(receiver->fif_incoming_semaphores[i], semaphore);
		zest_vec_push(sender->fif_stage_flags[i], stage_flags);
	}
}

void zest_ConnectQueueTo(zest_index receiver, VkPipelineStageFlags stage_flags) {
	ZEST_ASSERT(ZestRenderer->setup_context.type != zest_setup_context_type_none);	//Must be within a command queue setup context
	zest_ConnectCommandQueues(ZestRenderer->setup_context.command_queue_index, receiver, stage_flags);
}

void zest_ConnectQueueToPresent() {
	ZEST_ASSERT(ZestRenderer->setup_context.type != zest_setup_context_type_none);	//Must be within a command queue setup context
	zest_command_queue *command_queue = zest_GetCommandQueue(ZestRenderer->setup_context.command_queue_index);
	zest_ConnectCommandQueueToPresent(command_queue, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
}

VkSemaphore zest_GetPresentSemaphore(zest_command_queue *command_queue) {
	ZEST_ASSERT(command_queue->present_semaphore_index != -1);
	return command_queue->fif_outgoing_semaphores[ZEST_FIF][command_queue->present_semaphore_index];
}

zest_index zest_NewRenderPassSetupSC(const char *name) {
	zest__set_queue_context(zest_setup_context_type_render_pass);
	zest_command_queue *command_queue = zest_GetCommandQueue(ZestRenderer->setup_context.command_queue_index);
	zest_index render_commands_index = zest_create_command_queue_render_pass(name);
	zest_vec_push(command_queue->render_commands, render_commands_index);
	zest_command_queue_draw_commands *render_commands = zest_GetCommandQueueRenderPass(render_commands_index);
	render_commands->name = name;
	render_commands->render_pass = ZestRenderer->final_render_pass.render_pass;
	render_commands->get_frame_buffer = zest_GetRendererFrameBuffer;
	render_commands->render_pass_function = zest_RenderDrawRoutinesCallback;
	render_commands->viewport.extent = zest_GetSwapChainExtent();
	render_commands->viewport.offset.x = render_commands->viewport.offset.y = 0;
	ZestRenderer->setup_context.render_pass_index = render_commands_index;
	ZestRenderer->setup_context.type = zest_setup_context_type_render_pass;
	return render_commands_index;
}

zest_index zest_create_command_queue_render_pass(const char *name) {
	zest_command_queue_draw_commands render_commands = { 0 };
	render_commands.name = name;
	render_commands.viewport_scale.x = 1.f;
	render_commands.viewport_scale.y = 1.f;
	render_commands.render_target_index = -1;
	render_commands.viewport_type = zest_render_viewport_type_scale_with_window;

	//render_commands.command_queue_index = command_queue.index_in_renderer;
	zest_map_insert(ZestRenderer->command_queue_render_passes, name, render_commands);
	return zest_map_last_index(ZestRenderer->command_queue_render_passes);
}

zest_index zest_NewSpriteLayerSetup(const char *name) {
	zest__set_queue_context(zest_setup_context_type_layer);
	ZEST_ASSERT(ZestRenderer->setup_context.type == zest_setup_context_type_render_pass || ZestRenderer->setup_context.type == zest_setup_context_type_layer);	//The current setup context must be a render pass, layer or compute
	ZEST_ASSERT(strlen(name) <= 50);	//Layer name must be less then 50 characters
	zest_command_queue_draw_commands *render_commands = zest_GetCommandQueueRenderPass(ZestRenderer->setup_context.render_pass_index);
	zest_draw_routine *draw_routine = zest__create_draw_routine_with_sprite_layer(name, 1000);
	zest_instance_layer *layer = zest_GetLayerByIndex(draw_routine->draw_index);
	ZestRenderer->setup_context.layer_index = draw_routine->draw_index;
	//layer.command_queue_index = render_commands.command_queue_index;
	layer->render_pass_index = ZestRenderer->setup_context.render_pass_index;
	layer->name = name;
	zest_AddDrawRoutineToRenderPass(render_commands, draw_routine);
	ZestRenderer->setup_context.type = zest_setup_context_type_layer;
	return draw_routine->draw_index;
}

zest_draw_routine *zest__create_draw_routine_with_sprite_layer(const char *name, zest_uint initial_sprite_count) {
	ZEST_ASSERT(!zest_map_valid_name(ZestRenderer->draw_routines, name));	//A draw routine with that name already exists
	zest_instance_layer sprite_layer = { 0 };

	zest_draw_routine draw_routine = { 0 };
	zest_InitialiseSpriteLayer(&sprite_layer, initial_sprite_count);
	zest_map_insert(ZestRenderer->instance_layers, name, sprite_layer);
	draw_routine.draw_index = zest_map_last_index(ZestRenderer->instance_layers);
	draw_routine.update_buffers_callback = zest__update_draw_layer_buffers_callback;
	draw_routine.draw_callback = zest__draw_sprite_layer_callback;
	draw_routine.name = name;

	zest_map_insert(ZestRenderer->draw_routines, name, draw_routine);
	zest_index index = zest_map_last_index(ZestRenderer->draw_routines);

	zest_GetDrawRoutineByIndex(index)->routine_id = index;
	zest_GetLayerByIndex(draw_routine.draw_index)->draw_routine_index = index;
	return zest_GetDrawRoutineByIndex(index);
}

void zest_AddDrawRoutine(zest_index index) {
	zest__set_queue_context(zest_setup_context_type_layer);
	ZEST_ASSERT(ZestRenderer->setup_context.type == zest_setup_context_type_render_pass || ZestRenderer->setup_context.type == zest_setup_context_type_layer);	//The current setup context must be a render pass, layer or compute
	ZEST_ASSERT(index < (zest_index)zest_map_size(ZestRenderer->draw_routines));							//Must use a valid draw routine you made with CreateDrawRoutine (outside of a render queue setup)
	zest_command_queue_draw_commands *render_commands = zest_GetCommandQueueRenderPass(ZestRenderer->setup_context.render_pass_index);
	ZestRenderer->setup_context.type = zest_setup_context_type_layer;
	ZestRenderer->setup_context.draw_routine_index = index;
	zest_AddDrawRoutineToRenderPass(render_commands, zest_GetDrawRoutineByIndex(index));
}

void zest_AddDrawRoutineToRenderPass(zest_command_queue_draw_commands *render_pass, zest_draw_routine *draw_routine) {
	zest_vec_push(render_pass->draw_routines, draw_routine->routine_id);
	//draw_routine.cq_index = command_queue_index;
	draw_routine->cq_render_pass_index = zest_vec_last_index(render_pass->draw_routines);
}

zest_instance_layer *zest_GetLayerByIndex(zest_index index) {
	ZEST_ASSERT(zest_map_valid_index(ZestRenderer->instance_layers, index));	//That index could not be found in the storage
	return zest_map_at_index(ZestRenderer->instance_layers, index);
}

zest_instance_layer *zest_GetLayerByName(const char *name) {
	ZEST_ASSERT(zest_map_valid_name(ZestRenderer->instance_layers, name));	//That index could not be found in the storage
	return zest_map_at(ZestRenderer->instance_layers, name);
}

zest_index zest_GetLayerIndex(const char *name) {
	ZEST_ASSERT(zest_map_valid_name(ZestRenderer->instance_layers, name));	//That index could not be found in the storage
	return zest_map_get_index_by_name(ZestRenderer->instance_layers, name);
}

// --End Command queue setup and modify functions

// --Texture and Image functions
zest_map_textures *zest_GetTextures() {
	return &ZestRenderer->textures;
}

zest_texture zest_NewTexture() {
	zest_texture texture = { 0 };
	texture.flags = zest_texture_flag_use_filtering;
	texture.sampler = VK_NULL_HANDLE;
	texture.storage_type = zest_texture_storage_type_single;
	texture.image_index = 1;
	texture.image_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	texture.image_format = VK_FORMAT_R8G8B8A8_UNORM;
	texture.desired_channels = 0;
	texture.imgui_blend_type = zest_imgui_blendtype_none;
	texture.image_view_type = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
	texture.packed_border_size = 0;
	texture.sampler_info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	texture.sampler_info.magFilter = VK_FILTER_LINEAR;
	texture.sampler_info.minFilter = VK_FILTER_LINEAR;
	texture.sampler_info.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	texture.sampler_info.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	texture.sampler_info.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	texture.sampler_info.anisotropyEnable = VK_FALSE;
	texture.sampler_info.maxAnisotropy = 1.f;
	texture.sampler_info.unnormalizedCoordinates = VK_FALSE;
	texture.sampler_info.compareEnable = VK_FALSE;
	texture.sampler_info.compareOp = VK_COMPARE_OP_ALWAYS;
	texture.sampler_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
	texture.sampler_info.borderColor = VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK;
	texture.sampler_info.mipLodBias = 0.f;
	texture.sampler_info.minLod = 0.0f;
	texture.sampler_info.maxLod = 1.0f;
	texture.sampler_info.pNext = VK_NULL_HANDLE;
	texture.sampler_info.flags = 0;
	texture.sampler = VK_NULL_HANDLE;
	texture.texture_layer_size = 2048;
	texture.stream_staging_buffer = ZEST_NULL;

	return texture;
}

zest_image zest_CreateImage() {
	zest_image image = { 0 };
	image.scale = zest_Vec2Set1(1.f);
	image.handle = zest_Vec2Set1(0.5f);
	image.uv.x = 0.f;
	image.uv.y = 0.f;
	image.uv.z = 1.f;
	image.uv.w = 1.f;
	image.layer = 0;
	return image;
}

void zest_AllocateBitmap(zest_bitmap *bitmap, int width, int height, int channels, zest_uint fill_color) {
	bitmap->size = width * height * channels;
	if (bitmap->size > 0) {
		bitmap->data = (zest_byte*)ZEST__ALLOCATE(bitmap->size);
		bitmap->width = width;
		bitmap->height = height;
		bitmap->channels = channels;
		bitmap->stride = width * channels;
	}
	memset(bitmap->data, fill_color, bitmap->size);
}

void zest_LoadBitmapImage(zest_bitmap *image, const char *file, int desired_channels) {
	int width, height, original_no_channels;
	unsigned char *img = stbi_load(file, &width, &height, &original_no_channels, desired_channels);
	if (img != NULL) {
		image->width = width;
		image->height = height;
		image->data = img;
		image->channels = desired_channels ? desired_channels : original_no_channels;
		image->stride = width * original_no_channels;
		image->size = width * height * original_no_channels;
		image->name = file;
	}
	else {
		image->data = ZEST_NULL;
	}
}

void zest_LoadBitmapImageMemory(zest_bitmap *image, unsigned char *buffer, int size, int desired_no_channels) {
	int width, height, original_no_channels;
	unsigned char *img = stbi_load_from_memory(buffer, size, &width, &height, &original_no_channels, desired_no_channels);
	if (img != NULL) {
		image->width = width;
		image->height = height;
		image->data = img;
		image->channels = original_no_channels;
		image->stride = width * original_no_channels;
		image->size = width * height * original_no_channels;
	}
	else {
		image->data = ZEST_NULL;
	}
}

void zest_FreeBitmap(zest_bitmap *image) {
	if (image->data) {
		ZEST__FREE(image->data);
	}
	image->data = ZEST_NULL;
}

zest_bitmap zest_NewBitmap() {
	zest_bitmap bitmap = { 0 };
	return bitmap;
}

zest_bitmap *zest_GetBitmap(zest_texture *texture, zest_index bitmap_index) {
	return &texture->image_bitmaps[bitmap_index];
}

void zest_ConvertBitmapToRGBA(zest_bitmap *src, zest_byte alpha_level) {
	//Todo: simd this
	if (src->channels == 4)
		return;

	assert(src->data);

	zest_byte *new_image;
	int channels = 4;
	zest_size new_size = src->width * src->height * channels;
	new_image = (zest_byte*)ZEST__ALLOCATE(new_size);

	zest_size pos = 0;
	zest_size new_pos = 0;

	if (src->channels == 1) {
		while (pos < src->size) {
			*(new_image + new_pos) = *(src->data + pos);
			*(new_image + new_pos + 1) = *(src->data + pos);
			*(new_image + new_pos + 2) = *(src->data + pos);
			*(new_image + new_pos + 3) = alpha_level;
			pos += src->channels;
			new_pos += 4;
		}
	}
	else if (src->channels == 2) {
		while (pos < src->size) {
			*(new_image + new_pos) = *(src->data + pos);
			*(new_image + new_pos + 1) = *(src->data + pos);
			*(new_image + new_pos + 2) = *(src->data + pos);
			*(new_image + new_pos + 3) = *(src->data + pos + 1);
			pos += src->channels;
			new_pos += 4;
		}
	}
	else if (src->channels == 3) {
		while (pos < src->size) {
			*(new_image + new_pos) = *(src->data + pos);
			*(new_image + new_pos + 1) = *(src->data + pos + 1);
			*(new_image + new_pos + 2) = *(src->data + pos + 2);
			*(new_image + new_pos + 3) = alpha_level;
			pos += src->channels;
			new_pos += 4;
		}
	}

	ZEST__FREE(src->data);
	src->channels = channels;
	src->size = new_size;
	src->stride = src->width * channels;
	src->data = new_image;

}

void zest_CopyWholeBitmap(zest_bitmap *src, zest_bitmap *dst) {
	ZEST_ASSERT(src->data && src->size);

	*dst = *src;
	dst->data = ZEST_NULL;
	dst->data = (zest_byte*)ZEST__ALLOCATE(src->size);
	memcpy(dst->data, src->data, src->size);

}

void zest_CopyBitmap(zest_bitmap *src, int from_x, int from_y, int width, int height, zest_bitmap *dst, int to_x, int to_y) {
	ZEST_ASSERT(src->data);
	ZEST_ASSERT(dst->data);
	ZEST_ASSERT(src->channels == dst->channels);

	if (from_x + width > src->width)
		width = src->width - from_x;
	if (from_y + height > src->height)
		height = src->height - from_y;

	if (to_x + width > dst->width)
		to_x = dst->width - width;
	if (to_y + height > dst->height)
		to_y = dst->height - height;

	if (src->data && dst->data && width > 0 && height > 0) {
		int src_row = from_y * src->stride + (from_x * src->channels);
		int dst_row = to_y * dst->stride + (to_x * dst->channels);
		size_t row_size = width * src->channels;
		int rows_copied = 0;
		while (rows_copied < height) {
			memcpy(dst->data + dst_row, src->data + src_row, row_size);
			rows_copied++;
			src_row += src->stride;
			dst_row += dst->stride;
		}
	}
}

zest_color zest_SampleBitmap(zest_bitmap *image, int x, int y) {
	ZEST_ASSERT(image->data);

	size_t offset = y * image->stride + (x * image->channels);
    zest_color c = { 0 };
	if (offset < image->size) {
		c.r = *(image->data + offset);

		if (image->channels == 2) {
			c.a = *(image->data + offset + 1);
		}

		if (image->channels == 3) {
			c.g = *(image->data + offset + 1);
			c.b = *(image->data + offset + 2);
		}

		if (image->channels == 4) {
			c.g = *(image->data + offset + 1);
			c.b = *(image->data + offset + 2);
			c.a = *(image->data + offset + 3);
		}
	}

	return c;
}

float zest_FindBitmapRadius(zest_bitmap *image) {
	//Todo: optimise with SIMD
	ZEST_ASSERT(image->data);
	float max_radius = 0;
	for (int x = 0; x < image->width; ++x) {
		for (int y = 0; y < image->height; ++y) {
			zest_color c = zest_SampleBitmap(image, x, y);
			if (c.a) {
				max_radius = ceilf(ZEST__MAX(max_radius, zest_Distance((float)image->width / 2.f, (float)image->height / 2.f, (float)x, (float)y)));
			}
		}
	}
	return ceilf(max_radius);
}

zest_index zest_GetImageIndex(zest_texture *texture) {
	return texture->image_index;
}

void zest_DestroyBitmapArray(zest_bitmap_array *bitmap_array) { 
	if (bitmap_array->data) {
		ZEST__FREE(bitmap_array->data);
	}
	bitmap_array->data = ZEST_NULL; 
	bitmap_array->size_of_array = 0; 
}

zest_bitmap zest_GetImageFromArray(zest_bitmap_array *bitmap_array, zest_index index) {
	zest_bitmap image;
	image.width = bitmap_array->width;
	image.height = bitmap_array->height;
	image.channels = bitmap_array->channels;
	image.stride = bitmap_array->width * bitmap_array->channels;
	image.data = bitmap_array->data + index * bitmap_array->size_of_each_image;
	return image;
}

zest_index zest_LoadImageFile(zest_texture *texture, const char* filename) {
	zest_vec_push(texture->images, zest_CreateImage());
	texture->image_index = zest_vec_last_index(texture->images);
	zest_image *image = &zest_vec_back(texture->images);
	image->index = texture->image_index;
	zest_vec_push(texture->image_bitmaps, zest_NewBitmap());
	zest_bitmap *bitmap = zest_GetBitmap(texture, zest_GetImageIndex(texture));

	zest_LoadBitmapImage(bitmap, filename, texture->desired_channels);
	ZEST_ASSERT(bitmap->data != ZEST_NULL);			//No image data found, make sure the image is loading ok
	zest_ConvertBitmapToRGBA(bitmap, 255);

	image->width = bitmap->width;
	image->height = bitmap->height;
	image->texture_index = texture->index_in_renderer;
	if (texture->flags & zest_texture_flag_get_max_radius) {
		image->max_radius = zest_FindBitmapRadius(bitmap);
	}
	image->name = filename;
	zest_UpdateImageVertices(image);

	return image->index;
}

zest_index zest_LoadImageBitmap(zest_texture *texture, zest_bitmap *bitmap_to_load) {
	zest_vec_push(texture->images, zest_CreateImage());
	texture->image_index = zest_vec_last_index(texture->images);
	zest_image *image = &zest_vec_back(texture->images);
	image->index = texture->image_index;
	zest_bitmap image_copy;
	zest_CopyWholeBitmap(bitmap_to_load, &image_copy);
	zest_vec_push(texture->image_bitmaps, image_copy);
	zest_bitmap *bitmap = zest_GetBitmap(texture, zest_GetImageIndex(texture));

	ZEST_ASSERT(bitmap->data != ZEST_NULL);
	zest_ConvertBitmapToRGBA(bitmap, 255);

	image->width = bitmap->width;
	image->height = bitmap->height;
	image->texture_index = texture->index_in_renderer;
	if (texture->flags & zest_texture_flag_get_max_radius) {
		image->max_radius = zest_FindBitmapRadius(bitmap);
	}
	image->name = bitmap->name;
	zest_UpdateImageVertices(image);

	return image->index;
}

zest_index zest_LoadImageMemory(zest_texture *texture, const char* name, unsigned char* buffer, int buffer_size) {
	zest_vec_push(texture->images, zest_CreateImage());
	texture->image_index = zest_vec_last_index(texture->images);
	zest_image *image = &zest_vec_back(texture->images);
	image->index = texture->image_index;
	zest_vec_push(texture->image_bitmaps, zest_NewBitmap());
	zest_bitmap *bitmap = zest_GetBitmap(texture, zest_GetImageIndex(texture));

	zest_LoadBitmapImageMemory(bitmap, buffer, buffer_size, texture->desired_channels);
	ZEST_ASSERT(bitmap->data != ZEST_NULL);
	zest_ConvertBitmapToRGBA(bitmap, 255);

	image->width = bitmap->width;
	image->height = bitmap->height;
	image->texture_index = texture->index_in_renderer;
	if (texture->flags & zest_texture_flag_get_max_radius) {
		image->max_radius = zest_FindBitmapRadius(bitmap);
	}
	image->name = bitmap->name;
	zest_UpdateImageVertices(image);

	return image->index;
}

zest_index zest_LoadAnimationFile(zest_texture *texture, const char* filename, int width, int height, zest_uint frames, float *max_radius, zest_bool row_by_row) {
	zest_bitmap spritesheet;

	zest_LoadBitmapImage(&spritesheet, filename, texture->desired_channels);
	ZEST_ASSERT(spritesheet.data != ZEST_NULL);
	zest_ConvertBitmapToRGBA(&spritesheet, 255);

	zest_uint animation_area = spritesheet.width * spritesheet.height;
	zest_uint frame_area = width * height;
	zest_index first_index = zest_vec_size(texture->images);

	ZEST_ASSERT(frames <= animation_area / frame_area);	// ERROR: The animation being loaded is the wrong size for the number of frames that you want to load for the specified width and height.
	ZEST_ASSERT(spritesheet.width >= width);			// ERROR: The animation being loaded is not wide enough for the width of each frame specified.
	ZEST_ASSERT(spritesheet.height >= height);			// ERROR: The animation being loaded is not heigh enough for the height of each frame specified.

	*max_radius = zest_CopyAnimationFrames(texture, &spritesheet, width, height, frames, row_by_row);
	zest_FreeBitmap(&spritesheet);

	return first_index;
}

zest_index zest_LoadAnimationImage(zest_texture *texture, zest_bitmap *spritesheet, int width, int height, zest_uint frames, float *max_radius, zest_bool row_by_row) {

	ZEST_ASSERT(spritesheet->data != ZEST_NULL);
	zest_ConvertBitmapToRGBA(spritesheet, 255);
	spritesheet->width;
	spritesheet->height;

	zest_uint animation_area = spritesheet->width * spritesheet->height;
	zest_uint frame_area = width * height;

	zest_index first_index = zest_vec_size(texture->images);

	ZEST_ASSERT(frames <= animation_area / frame_area);	// ERROR: The animation being loaded is the wrong size for the number of frames that you want to load for the specified width and height.
	ZEST_ASSERT(spritesheet->width >= width);			// ERROR: The animation being loaded is not wide enough for the width of each frame specified.
	ZEST_ASSERT(spritesheet->height >= height);			// ERROR: The animation being loaded is not heigh enough for the height of each frame specified.

	*max_radius = zest_CopyAnimationFrames(texture, spritesheet, width, height, frames, row_by_row);

	return first_index;
}

zest_index zest_LoadAnimationMemory(zest_texture *texture, const char* name, unsigned char *buffer, int buffer_size, int width, int height, zest_uint frames, float *max_radius, zest_bool row_by_row) {
	zest_bitmap spritesheet = { 0 };

	zest_LoadBitmapImageMemory(&spritesheet, buffer, buffer_size, texture->desired_channels);
	ZEST_ASSERT(spritesheet.data != ZEST_NULL);
	zest_ConvertBitmapToRGBA(&spritesheet, 255);
	//PreMultiplyAlpha(spritesheet, load_filter);
	spritesheet.width;
	spritesheet.height;

	zest_uint animation_area = spritesheet.width * spritesheet.height;
	zest_uint frame_area = width * height;

	zest_index first_index = zest_vec_size(texture->images);

	ZEST_ASSERT(frames <= animation_area / frame_area);	//ERROR: The animation being loaded is the wrong size for the number of frames that you want to load for the specified width and height.
	ZEST_ASSERT(spritesheet.width >= width);				// ERROR: The animation being loaded is not wide enough for the width of each frame specified.
	ZEST_ASSERT(spritesheet.height >= height);			// ERROR: The animation being loaded is not heigh enough for the height of each frame specified.

	*max_radius = zest_CopyAnimationFrames(texture, &spritesheet, width, height, frames, row_by_row);
	zest_FreeBitmap(&spritesheet);

	return first_index;
}

float zest_CopyAnimationFrames(zest_texture *texture, zest_bitmap *spritesheet, int width, int height, zest_uint frames, zest_bool row_by_row) {
	zest_uint rows = spritesheet->height / height;
	zest_uint cols = spritesheet->width / width;

	zest_uint frame_count = 0;
	float max_radius = 0;

	for (zest_uint r = 0; r != (row_by_row ? rows : cols); ++r) {
		for (zest_uint c = 0; c != (row_by_row ? cols : rows); ++c) {
			if (frame_count >= frames) {
				break;
			}
			zest_vec_push(texture->images, zest_CreateImage());
			texture->image_index = zest_vec_last_index(texture->images);
			zest_image *frame = zest_GetImageFromTexture(texture, texture->image_index);
			frame->index = texture->image_index;
			zest_vec_push(texture->image_bitmaps, zest_NewBitmap());
			zest_bitmap *image_bitmap = zest_GetBitmap(texture, texture->image_index);
			zest_AllocateBitmap(image_bitmap, width, height, spritesheet->channels, 0);
			zest_CopyBitmap(spritesheet, c * width, r * height, width, height, image_bitmap, 0, 0);
			frame->width = image_bitmap->width;
			frame->height = image_bitmap->height;
			zest_UpdateImageVertices(frame);
			frame->texture_index = texture->index_in_renderer;
			if (texture->flags & zest_texture_flag_get_max_radius) {
				frame->max_radius = zest_FindBitmapRadius(image_bitmap);
				max_radius = ZEST__MAX(max_radius, frame->max_radius);
			}
			frame_count++;
		}
	}

	return max_radius;

}

void zest_CleanUpTexture(zest_texture *texture) {
	//It would be nice if we didn't have to wait for the device to be idle here, but images can't be destroyed if they're currently used
	//in a command buffer
	zest_WaitForIdleDevice();
	vkDestroySampler(ZestDevice->logical_device, texture->sampler, ZEST_NULL);
	vkDestroyImageView(ZestDevice->logical_device, texture->frame_buffer.view, ZEST_NULL);
	vkDestroyImage(ZestDevice->logical_device, texture->frame_buffer.image, ZEST_NULL);
	zest_FreeBuffer(texture->frame_buffer.buffer);
	texture->flags &= ~zest_texture_flag_textures_ready;
}

void zest_ProcessTextureImages(zest_texture *texture) {
	if (texture->flags & zest_texture_flag_textures_ready) {
		zest_CleanUpTexture(texture);
	}

	if (zest_vec_empty(texture->images) && texture->storage_type != zest_texture_storage_type_storage && texture->storage_type != zest_texture_storage_type_single)
		return;

	if (texture->storage_type == zest_texture_storage_type_packed) {
		zest_PackImages(texture, texture->texture_layer_size);
	}
	else if (texture->storage_type == zest_texture_storage_type_bank) {
		zest_MakeImageBank(texture, texture->images[0].width);
	}
	else if (texture->storage_type == zest_texture_storage_type_sprite_sheet) {
		zest_MakeSpriteSheet(texture);
	}

	zest_uint mip_levels = 1;

	if (texture->storage_type < zest_texture_storage_type_sprite_sheet) {

		if (texture->flags & zest_texture_flag_use_filtering)
			mip_levels = (zest_uint)floor(log2(ZEST__MAX(texture->bitmap_array.width, texture->bitmap_array.height))) + 1;
		else
			mip_levels = 1;

		zest_CreateTextureImageArray(texture, mip_levels);
		zest_CreateTextureImageView(texture, texture->image_view_type, mip_levels, texture->layer_count);
		zest_CreateTextureSampler(texture, texture->sampler_info, mip_levels);

		texture->descriptor.imageView = texture->frame_buffer.view;
		texture->descriptor.sampler = texture->sampler;
		texture->descriptor.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

		texture->flags |= zest_texture_flag_textures_ready;
	}
	else if (texture->storage_type == zest_texture_storage_type_sprite_sheet) {

		if (texture->flags & zest_texture_flag_use_filtering)
			mip_levels = (zest_uint)floor(log2(ZEST__MAX(texture->images[0].width, texture->images[0].height))) + 1;
		else
			mip_levels = 1;

		texture->layer_count = 1;

		VkImageUsageFlags flags = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
		if (texture->image_layout == VK_IMAGE_LAYOUT_GENERAL) {
			flags |= VK_IMAGE_USAGE_STORAGE_BIT;
			zest_CreateTextureImage(texture, mip_levels, flags, VK_IMAGE_LAYOUT_GENERAL, ZEST_TRUE);
			texture->descriptor.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
		}
		else {
			zest_CreateTextureImage(texture, mip_levels, flags, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, ZEST_TRUE);
			texture->descriptor.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		}

		zest_CreateTextureImageView(texture, texture->image_view_type, mip_levels, texture->layer_count);
		zest_CreateTextureSampler(texture, texture->sampler_info, mip_levels);

		texture->descriptor.imageView = texture->frame_buffer.view;
		texture->descriptor.sampler = texture->sampler;
		texture->flags |= zest_texture_flag_textures_ready;

	}
	else if (texture->storage_type == zest_texture_storage_type_single) {

		if (texture->flags & zest_texture_flag_use_filtering)
			mip_levels = (zest_uint)floor(log2(ZEST__MAX(texture->images[0].width, texture->images[0].height))) + 1;
		else
			mip_levels = 1;

		texture->layer_count = 1;

		VkImageUsageFlags flags = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
		if (texture->image_layout == VK_IMAGE_LAYOUT_GENERAL) {
			flags |= VK_IMAGE_USAGE_STORAGE_BIT;
			zest_CreateTextureImage(texture, mip_levels, flags, VK_IMAGE_LAYOUT_GENERAL, ZEST_TRUE);
			texture->descriptor.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
		}
		else {
			zest_CreateTextureImage(texture, mip_levels, flags, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, ZEST_TRUE);
			texture->descriptor.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		}

		zest_CreateTextureImageView(texture, texture->image_view_type, mip_levels, texture->layer_count);
		zest_CreateTextureSampler(texture, texture->sampler_info, mip_levels);

		texture->descriptor.imageView = texture->frame_buffer.view;
		texture->descriptor.sampler = texture->sampler;
		texture->flags |= zest_texture_flag_textures_ready;

	}
	else if (texture->storage_type == zest_texture_storage_type_stream) {

		if (texture->flags & zest_texture_flag_use_filtering)
			mip_levels = (zest_uint)floor(log2(ZEST__MAX(texture->images[0].width, texture->images[0].height))) + 1;
		else
			mip_levels = 1;

		texture->layer_count = 1;

		VkImageUsageFlags flags = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
		if (texture->image_layout == VK_IMAGE_LAYOUT_GENERAL) {
			flags |= VK_IMAGE_USAGE_STORAGE_BIT;
			zest_CreateTextureStream(texture, mip_levels, flags, VK_IMAGE_LAYOUT_GENERAL, ZEST_TRUE);
			texture->descriptor.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
		}
		else {
			zest_CreateTextureStream(texture, mip_levels, flags, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, ZEST_TRUE);
			texture->descriptor.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		}

		zest_CreateTextureImageView(texture, texture->image_view_type, mip_levels, texture->layer_count);
		zest_CreateTextureSampler(texture, texture->sampler_info, mip_levels);

		texture->descriptor.imageView = texture->frame_buffer.view;
		texture->descriptor.sampler = texture->sampler;
		texture->flags |= zest_texture_flag_textures_ready;

	}
	else if (texture->storage_type == zest_texture_storage_type_storage) {
		mip_levels = 1;
		texture->layer_count = 1;

		zest_CreateTextureImage(texture, mip_levels, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT, VK_IMAGE_LAYOUT_GENERAL, ZEST_FALSE);
		zest_CreateTextureImageView(texture, texture->image_view_type, mip_levels, texture->layer_count);
		zest_CreateTextureSampler(texture, texture->sampler_info, mip_levels);

		texture->descriptor.imageView = texture->frame_buffer.view;
		texture->descriptor.sampler = texture->sampler;
		texture->descriptor.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
		texture->flags |= zest_texture_flag_textures_ready;
	}

	if (!(texture->flags & zest_texture_flag_descriptor_sets_created)) {
		zest_CreateTextureDescriptorSets(texture, "Default", "Standard Uniform Buffer");
		zest_SwitchTextureDescriptorSet(texture, "Default");
	}

	zest_DeleteTextureLayers(texture);

	return;
}

void zest_DeleteTextureLayers(zest_texture *texture) {
	for (zest_foreach_i(texture->layers)) {
		zest_FreeBitmap(&texture->layers[i]);
	}
	zest_vec_clear(texture->layers);
}

void zest_CreateTextureDescriptorSets(zest_texture *texture, const char *name, const char *uniform_buffer_name) {
	ZEST_ASSERT(zest_map_valid_name(ZestRenderer->uniform_buffers, uniform_buffer_name));
	zest_descriptor_set set = { 0 };
	set.uniform_buffer_index = zest_map_get_index_by_name(ZestRenderer->uniform_buffers, uniform_buffer_name);
	for (ZEST_EACH_FIF_i) {
		zest_AllocateDescriptorSet(ZestRenderer->descriptor_pool, *zest_GetDescriptorSetLayoutByName("Standard 1 uniform 1 sampler"), &set.descriptor_set[i]);
		zest_vec_push(set.descriptor_writes[i], zest_CreateBufferDescriptorWriteWithType(set.descriptor_set[i], zest_GetUniformBufferInfoByIndex(set.uniform_buffer_index, i), 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER));
		zest_vec_push(set.descriptor_writes[i], zest_CreateImageDescriptorWriteWithType(set.descriptor_set[i], &texture->descriptor, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER));
	}
	zest_AddTextureDescriptorSet(texture, name, set);
	zest_UpdateTextureSingleDescriptorSet(texture, name);
	texture->flags |= zest_texture_flag_descriptor_sets_created;
}

void zest_UpdateTextureSingleDescriptorSet(zest_texture *texture, const char *name) {
	ZEST_ASSERT(zest_map_valid_name(texture->descriptor_sets, name));
	zest_descriptor_set *set = zest_map_at(texture->descriptor_sets, name);
	for (ZEST_EACH_FIF_i) {
		zest_UpdateDescriptorSet(set->descriptor_writes[i]);
	}
}

void zest_UpdateAllTextureDescriptorWrites(zest_texture *texture) {
	for (ZEST_EACH_FIF_i) {
		for (zest_map_foreach_j(texture->descriptor_sets)) {
			zest_descriptor_set *set = zest_map_at_index(texture->descriptor_sets, j);
			for (zest_foreach_k(set->descriptor_writes[i])) {
				VkWriteDescriptorSet *write = &set->descriptor_writes[i][k];
				write->dstSet = set->descriptor_set[i];
				if (write->descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER) {
					write->pBufferInfo = zest_GetUniformBufferInfoByIndex(set->uniform_buffer_index, i);
				}
				else if (write->descriptorType == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER) {
					write->pImageInfo = &texture->descriptor;
				}
			}
		}
	}
}

void zest_SwitchTextureDescriptorSet(zest_texture *texture, const char *name) {
	ZEST_ASSERT(zest_map_valid_name(texture->descriptor_sets, name));
	zest_descriptor_set *set = zest_map_at(texture->descriptor_sets, name);
	for (ZEST_EACH_FIF_i) {
		texture->current_descriptor_set[i] = set->descriptor_set[i];
	}
}

void zest_AddTextureDescriptorSet(zest_texture *texture, const char *name, zest_descriptor_set descriptor_set) { 
	zest_map_insert(texture->descriptor_sets, name, descriptor_set); 
}

zest_bitmap *zest_GetTextureSingleBitmap(zest_texture *texture) {
	if (texture->texture_bitmap.data) {
		return &texture->texture_bitmap;
	}

	if (zest_vec_size(texture->images) > 0) {
		return &texture->image_bitmaps[0];
	}

	return &texture->texture_bitmap;
}

void zest_CreateTextureImage(zest_texture *texture, zest_uint mip_levels, VkImageUsageFlags usage_flags, VkImageLayout image_layout, zest_bool copy_bitmap) {
	int channels = 4;
	if (ZestDevice->color_format == VK_FORMAT_R8G8_UNORM)
		channels = 2;
	VkDeviceSize image_size = zest_GetTextureSingleBitmap(texture)->width * zest_GetTextureSingleBitmap(texture)->height * channels;

	zest_buffer *staging_buffer = 0;
	if (copy_bitmap) {
		zest_buffer_info buffer_info = zest_CreateStagingBufferInfo();
		staging_buffer = zest_CreateBuffer(image_size, &buffer_info, ZEST_NULL);
		memcpy(staging_buffer->data, zest_GetTextureSingleBitmap(texture)->data, (zest_size)image_size);
	}

	texture->frame_buffer.buffer = zest__create_image(zest_GetTextureSingleBitmap(texture)->width, zest_GetTextureSingleBitmap(texture)->height, mip_levels, VK_SAMPLE_COUNT_1_BIT, ZestDevice->color_format, VK_IMAGE_TILING_OPTIMAL, usage_flags, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &texture->frame_buffer.image);

	texture->frame_buffer.format = ZestDevice->color_format;
	if (copy_bitmap) {
		zest__transition_image_layout(texture->frame_buffer.image, ZestDevice->color_format, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, mip_levels, 1);
		zest__copy_buffer_to_image(*zest_GetBufferDeviceBuffer(staging_buffer), staging_buffer->memory_offset, texture->frame_buffer.image, (zest_uint)zest_GetTextureSingleBitmap(texture)->width, (zest_uint)zest_GetTextureSingleBitmap(texture)->height, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
		tloc_FreeRemote(staging_buffer->buffer_allocator->allocator, staging_buffer);
		zest__generate_mipmaps(texture->frame_buffer.image, ZestDevice->color_format, zest_GetTextureSingleBitmap(texture)->width, zest_GetTextureSingleBitmap(texture)->height, mip_levels, 1, image_layout);
	}
	else {
		zest__transition_image_layout(texture->frame_buffer.image, ZestDevice->color_format, VK_IMAGE_LAYOUT_UNDEFINED, image_layout, mip_levels, 1);
	}
}

void zest_CreateTextureImageArray(zest_texture *texture, zest_uint mip_levels) {
	VkDeviceSize image_size = texture->bitmap_array.total_mem_size;

	zest_buffer_info buffer_info = zest_CreateStagingBufferInfo();
	zest_buffer *staging_buffer = zest_CreateBuffer(image_size, &buffer_info, ZEST_NULL);

	//memcpy(staging_buffer.mapped, texture.TextureArray().data(), texture.TextureArray().size());
	memcpy(staging_buffer->data, texture->bitmap_array.data, texture->bitmap_array.total_mem_size);

	//texture.FrameBuffer().allocation_id = CreateImageArray(texture.TextureArray().extent().x, texture.TextureArray().extent().y, mip_levels, texture.LayerCount(), VK_SAMPLE_COUNT_1_BIT, image_format, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, texture.FrameBuffer().image);
	texture->frame_buffer.buffer = zest__create_image_array(texture->bitmap_array.width, texture->bitmap_array.height, mip_levels, texture->layer_count, VK_SAMPLE_COUNT_1_BIT, ZestDevice->color_format, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &texture->frame_buffer.image);
	texture->frame_buffer.format = ZestDevice->color_format;

	zest__transition_image_layout(texture->frame_buffer.image, ZestDevice->color_format, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, mip_levels, texture->layer_count);
	zest__copy_buffer_regions_to_image(texture->buffer_copy_regions, *zest_GetBufferDeviceBuffer(staging_buffer), staging_buffer->memory_offset, texture->frame_buffer.image);

	tloc_FreeRemote(staging_buffer->buffer_allocator->allocator, staging_buffer);

	zest__generate_mipmaps(texture->frame_buffer.image, VK_FORMAT_R8G8B8A8_UNORM, texture->bitmap_array.width, texture->bitmap_array.height, mip_levels, texture->layer_count, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
}

void zest_CreateTextureStream(zest_texture *texture, zest_uint mip_levels, VkImageUsageFlags usage_flags, VkImageLayout image_layout, zest_bool copy_bitmap) {
	int channels = 4;
	if (ZestDevice->color_format == VK_FORMAT_R8G8_UNORM)
		channels = 2;

	VkDeviceSize image_size = zest_GetTextureSingleBitmap(texture)->width * zest_GetTextureSingleBitmap(texture)->height * channels;

	if (copy_bitmap) {
		zest_buffer_info buffer_info = zest_CreateStagingBufferInfo();
		texture->stream_staging_buffer = zest_CreateBuffer(image_size, &buffer_info, ZEST_NULL);
		memcpy(texture->stream_staging_buffer->data, zest_GetTextureSingleBitmap(texture)->data, (zest_size)image_size);
	}

	texture->frame_buffer.buffer = zest__create_image(zest_GetTextureSingleBitmap(texture)->width, zest_GetTextureSingleBitmap(texture)->height, mip_levels, VK_SAMPLE_COUNT_1_BIT, ZestDevice->color_format, VK_IMAGE_TILING_OPTIMAL, usage_flags, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &texture->frame_buffer.image);

	texture->frame_buffer.format = ZestDevice->color_format;
	if (copy_bitmap) {
		zest__transition_image_layout(texture->frame_buffer.image, ZestDevice->color_format, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, mip_levels, 1);
		zest__copy_buffer_to_image(*zest_GetBufferDeviceBuffer(texture->stream_staging_buffer), texture->stream_staging_buffer->memory_offset, texture->frame_buffer.image, (zest_uint)zest_GetTextureSingleBitmap(texture)->width, (zest_uint)zest_GetTextureSingleBitmap(texture)->height, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
		zest__generate_mipmaps(texture->frame_buffer.image, ZestDevice->color_format, zest_GetTextureSingleBitmap(texture)->width, zest_GetTextureSingleBitmap(texture)->width, mip_levels, 1, image_layout);
	}
	else {
		zest__transition_image_layout(texture->frame_buffer.image, ZestDevice->color_format, VK_IMAGE_LAYOUT_UNDEFINED, image_layout, mip_levels, 1);
	}
}

void zest_CreateTextureImageView(zest_texture *texture, VkImageViewType view_type, zest_uint mip_levels, zest_uint layer_count) {
	texture->frame_buffer.view = zest__create_image_view(texture->frame_buffer.image, ZestDevice->color_format, VK_IMAGE_ASPECT_COLOR_BIT, mip_levels, view_type, layer_count);
}

void zest_CreateTextureSampler(zest_texture *texture, VkSamplerCreateInfo sampler_info, zest_uint mip_levels) {

	sampler_info.maxLod = (float)mip_levels;

	ZEST_VK_CHECK_RESULT(vkCreateSampler(ZestDevice->logical_device, &sampler_info, &ZestDevice->allocation_callbacks, &texture->sampler));
}

zest_byte zest_CalculateTextureLayers(stbrp_rect *rects, zest_uint size, const zest_uint node_count) {
	stbrp_node *nodes = 0;
	zest_vec_resize(nodes, node_count);
	zest_byte layers = 0;
	stbrp_rect *current_rects = 0;
	stbrp_rect *rects_copy = 0;
	zest_vec_resize(rects_copy, zest_vec_size(rects));
	memcpy(rects_copy, rects, zest_vec_size_in_bytes(rects));
	while (!zest_vec_empty(rects_copy) && layers <= 255) {

		stbrp_context context;
		stbrp_init_target(&context, size, size, nodes, node_count);
		stbrp_pack_rects(&context, rects_copy, (int)zest_vec_size(rects_copy));

		for (zest_foreach_i(rects_copy)) {
			zest_vec_push(current_rects, rects_copy[i]);
		}

		zest_vec_clear(rects_copy);

		for (zest_foreach_i(current_rects)) {
			if (!rects_copy[i].was_packed) {
				zest_vec_push(rects_copy, rects_copy[i]);
			}
		}

		zest_vec_clear(current_rects);
		layers++;
	}

	zest_vec_free( nodes);
	zest_vec_free(current_rects);
	zest_vec_free(rects_copy);
	return layers;
}

void zest_CreateBitmapArray(zest_bitmap_array *images, int width, int height, int channels, zest_uint size_of_array) {
	assert(size_of_array);			//must create with atleast one image in the array
	if (images->data) {
		ZEST__FREE(images->data);
		images->data = ZEST_NULL;
	}
	images->width = width;
	images->height = height;
	images->channels = channels;
	images->stride = width * channels;
	images->size_of_array = size_of_array;
	images->size_of_each_image = width * height * channels;
	images->total_mem_size = images->size_of_array * images->size_of_each_image;
	images->data = (zest_byte*)ZEST__ALLOCATE(size_of_array * images->total_mem_size);
}

void zest_ClearBitmapArray(zest_bitmap_array *images) {
	if (images->data) {
		free(images->data);
	}
	images->data = ZEST_NULL;
	images->size_of_array = 0;
}

void zest_MakeImageBank(zest_texture *texture, zest_uint size) {

	//array_texture = gli::texture2d_array(gli::FORMAT_RGBA8_UNORM_PACK8, gli::texture2d_array::extent_type(size, size), images.size());
	zest_CreateBitmapArray(&texture->bitmap_array, size, size, 4, zest_vec_size(texture->images));

	zest_uint id = 0;

	for (zest_foreach_i(texture->images)) {
		zest_image *image = &texture->images[i];

		image->uv.x = 0;
		image->uv.y = 0;
		image->uv.z = 1.f;
		image->uv.w = 1.f;
		image->uv_xy = zest_Pack16bit(image->uv.x, image->uv.y);
		image->uv_zw = zest_Pack16bit(image->uv.z, image->uv.w);
		image->left = 0;
		image->top = 0;
		image->layer = id;
		image->texture_index = texture->index_in_renderer;

		zest_bitmap tmp_image = zest_NewBitmap();;
		zest_AllocateBitmap(&tmp_image, image->width, image->height, 4, 0);

		if (image->width != size || image->height != size) {
			zest_bitmap *image_bitmap = &texture->image_bitmaps[image->index];
			zest_CopyBitmap(image_bitmap, 0, 0, image->width, image->height, &tmp_image, 0, 0);
			stbir_resize_uint8(tmp_image.data, tmp_image.width, tmp_image.height, tmp_image.stride, tmp_image.data, size, size, size * 4, 4);
		}
		else {
			zest_bitmap *image_bitmap = &texture->image_bitmaps[image->index];
			zest_CopyBitmap(image_bitmap, 0, 0, size, size, &tmp_image, 0, 0);
		}

		tmp_image.width = size;
		tmp_image.height = size;

		//auto format = gli::FORMAT_RGBA8_UNORM_PACK8;

		//array_texture[id] = gli::texture2d(format, gli::texture2d::extent_type(size, size), 1);

		//memcpy(array_texture[id][0].data(), tmp_image.data, array_texture[id][0].size());
		memcpy(zest_BitmapArrayLookUp(&texture->bitmap_array, id), tmp_image.data, texture->bitmap_array.size_of_each_image);
		zest_FreeBitmap(&tmp_image);

		id++;
	}

	texture->layer_count = id;

	zest_size offset = 0;

	zest_vec_clear(texture->buffer_copy_regions);

	for (zest_uint layer = 0; layer < texture->layer_count; layer++) {
		VkBufferImageCopy buffer_copy_region = { 0 };
		buffer_copy_region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		buffer_copy_region.imageSubresource.mipLevel = 0;
		buffer_copy_region.imageSubresource.baseArrayLayer = layer;
		buffer_copy_region.imageSubresource.layerCount = 1;
		buffer_copy_region.imageExtent.width = size;
		buffer_copy_region.imageExtent.height = size;
		buffer_copy_region.imageExtent.depth = 1;
		buffer_copy_region.bufferOffset = offset;

		zest_vec_push(texture->buffer_copy_regions, buffer_copy_region);

		// Increase offset into staging buffer for next level / face
		//offset += array_texture[layer].size();
		offset += texture->bitmap_array.size_of_each_image;
	}

}

void zest_MakeSpriteSheet(zest_texture *texture) {
	stbrp_rect *rects = 0;
	stbrp_rect *rects_to_process = 0;

	zest_uint id = 0;
	zest_uint max_width = 0, max_height = 0;
	for (zest_foreach_i(texture->images)) {
		zest_image *image = &texture->images[i];
		stbrp_rect rect;
		rect.w = image->width + texture->packed_border_size * 2;
		rect.h = image->height + texture->packed_border_size * 2;
		rect.x = 0;
		rect.y = 0;
		rect.was_packed = 0;
		rect.id = image->index;

		zest_vec_push(rects, rect);
		zest_vec_push(rects_to_process, rect);

		max_width = (zest_uint)ZEST__MAX((float)max_width, (float)rect.w);
		max_height = (zest_uint)ZEST__MAX((float)max_height, (float)rect.h);

		id++;
	}

	zest_uint size = (zest_uint)ZEST__MAX((float)max_width, (float)max_height);
	size = (zest_uint)zest_GetNextPower(size);

	const zest_uint node_count = size * 2;
	stbrp_node *nodes = 0;
	zest_vec_resize(nodes, node_count);

	while (zest_CalculateTextureLayers(rects, size, node_count) != 1 || size > ZestDevice->max_image_size) {
		size *= 2;
	}

	ZEST_ASSERT(size <= ZestDevice->max_image_size);	//Texture layer size is greater then GPU capability

	texture->layer_count = 1;

	zest_byte current_layer = 0;

	for (zest_foreach_i(texture->layers)) {
		zest_FreeBitmap(&texture->layers[i]);
	}
	zest_vec_clear(texture->layers);

	stbrp_context context;
	stbrp_init_target(&context, size, size, nodes, node_count);
	stbrp_pack_rects(&context, rects, (int)zest_vec_size(rects));

	stbrp_rect *current_rects = 0;
	for (zest_foreach_i(rects)) {
		zest_vec_push(current_rects, rects[i]);
	}

	zest_vec_clear(rects);

	zest_FreeBitmap(&texture->texture_bitmap);
	zest_AllocateBitmap(&texture->texture_bitmap, size, size, 4, 0);
	texture->texture.width = size;
	texture->texture.height = size;

	for (zest_foreach_i(current_rects)) {
		stbrp_rect *rect = &current_rects[i];

		if (rect->was_packed) {
			zest_image *image = &texture->images[rect->id];

			float rect_x = (float)rect->x + texture->packed_border_size;
			float rect_y = (float)rect->y + texture->packed_border_size;

			image->uv.x = (rect_x + 0.5f) / (float)size;
			image->uv.y = (rect_y + 0.5f) / (float)size;
			image->uv.z = ((float)image->width + (rect_x - 0.5f)) / (float)size;
			image->uv.w = ((float)image->height + (rect_y - 0.5f)) / (float)size;
			image->uv_xy = zest_Pack16bit(image->uv.x, image->uv.y);
			image->uv_zw = zest_Pack16bit(image->uv.z, image->uv.w);
			image->left = (zest_uint)rect_x;
			image->top = (zest_uint)rect_y;
			image->layer = current_layer;
			image->texture_index = texture->index_in_renderer;

			zest_CopyBitmap(&texture->image_bitmaps[rect->id], 0, 0, image->width, image->height, &texture->texture_bitmap, image->left, image->top);
		}
		else {
			zest_vec_push(rects, *rect);
		}
	}

	zest_vec_free(rects);
	zest_vec_free(rects_to_process);
	zest_vec_free(nodes);
	zest_vec_free(current_rects);
}

void zest_PackImages(zest_texture *texture, zest_uint size) {
	stbrp_rect *rects = 0;
	stbrp_rect *rects_to_process = 0;

    zest_uint max_width = 0;
	for (zest_foreach_i(texture->images)) {
		zest_image *image = &texture->images[i];
		stbrp_rect rect;
		rect.w = image->width + texture->packed_border_size * 2;
		rect.h = image->height + texture->packed_border_size * 2;
		rect.x = 0;
		rect.y = 0;
		rect.was_packed = 0;
		rect.id = image->index;

		zest_vec_push(rects, rect);
		zest_vec_push(rects_to_process, rect);
	}

	const zest_uint node_count = size * 2;
	stbrp_node *nodes = 0;
	zest_vec_resize(nodes, node_count);

	if (texture->storage_type == zest_texture_storage_type_sprite_sheet) {
		while (zest_CalculateTextureLayers(rects, max_width, node_count) != 1 || size > ZestDevice->max_image_size) {
			max_width *= 2;
		}

		if (max_width > ZestDevice->max_image_size) {
			return;		//todo: return somekind of error
		}
	}
	else {
		texture->layer_count = zest_CalculateTextureLayers(rects, size, node_count);

		zest_ClearBitmapArray(&texture->bitmap_array);
		zest_CreateBitmapArray(&texture->bitmap_array, size, size, 4, texture->layer_count);
	}

	zest_byte current_layer = 0;

	for (zest_foreach_i(texture->layers)) {
		zest_FreeBitmap(&texture->layers[i]);
	}
	zest_vec_clear(texture->layers);

	zest_color fillcolor;
	fillcolor.r = 0;
	fillcolor.g = 0;
	fillcolor.b = 0;
	fillcolor.a = 0;

	stbrp_rect *current_rects = 0;
	while (!zest_vec_empty(rects) && current_layer < texture->layer_count) {
		stbrp_context context;
		stbrp_init_target(&context, size, size, nodes, node_count);
		stbrp_pack_rects(&context, rects, (int)zest_vec_size(rects));

		for (zest_foreach_i(rects)) {
			zest_vec_push(current_rects, rects[i]);
		}

		zest_vec_clear(rects);

		zest_bitmap tmp_image = zest_NewBitmap();
		zest_AllocateBitmap(&tmp_image, size, size, 4, 0);
		int count = 0;

		for (zest_foreach_i(current_rects)) {
			stbrp_rect *rect = &current_rects[i];

			if (rect->was_packed) {
				zest_image *image = &texture->images[rect->id];

				float rect_x = (float)rect->x + texture->packed_border_size;
				float rect_y = (float)rect->y + texture->packed_border_size;

				image->uv.x = (rect_x + 0.5f) / (float)size;
				image->uv.y = (rect_y + 0.5f) / (float)size;
				image->uv.z = ((float)image->width + (rect_x - 0.5f)) / (float)size;
				image->uv.w = ((float)image->height + (rect_y - 0.5f)) / (float)size;
				image->uv_xy = zest_Pack16bit(image->uv.x, image->uv.y);
				image->uv_zw = zest_Pack16bit(image->uv.z, image->uv.w);
				image->left = (zest_uint)rect_x;
				image->top = (zest_uint)rect_y;
				image->layer = current_layer;
				image->texture_index = texture->index_in_renderer;

				zest_CopyBitmap(&texture->image_bitmaps[rect->id], 0, 0, image->width, image->height, &tmp_image, image->left, image->top);

				count++;
			}
			else {
				zest_vec_push(rects, *rect);
			}
		}

		memcpy(zest_BitmapArrayLookUp(&texture->bitmap_array, current_layer), tmp_image.data, texture->bitmap_array.size_of_each_image);

		zest_vec_push(texture->layers, tmp_image);
		current_layer++;
		zest_vec_clear(current_rects);
	}

	size_t offset = 0;

	zest_vec_clear(texture->buffer_copy_regions);

	for (zest_uint layer = 0; layer < texture->layer_count; layer++)
	{
		VkBufferImageCopy buffer_copy_region = { 0 };
		buffer_copy_region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		buffer_copy_region.imageSubresource.mipLevel = 0;
		buffer_copy_region.imageSubresource.baseArrayLayer = layer;
		buffer_copy_region.imageSubresource.layerCount = 1;
		buffer_copy_region.imageExtent.width = size;
		buffer_copy_region.imageExtent.height = size;
		buffer_copy_region.imageExtent.depth = 1;
		buffer_copy_region.bufferOffset = offset;

		zest_vec_push(texture->buffer_copy_regions, buffer_copy_region);

		// Increase offset into staging buffer for next level / face
		offset += texture->bitmap_array.size_of_each_image;
	}

	zest_vec_free(rects);
	zest_vec_free(rects_to_process);
	zest_vec_free(nodes);
	zest_vec_free(current_rects);

}

void zest_UpdateImageVertices(zest_image *image) {
	image->min.x = image->width * (0.f - image->handle.x) * image->scale.x;
	image->min.y = image->height * (0.f - image->handle.y) * image->scale.y;
	image->max.x = image->width * (1.f - image->handle.x) * image->scale.x;
	image->max.y = image->height * (1.f - image->handle.y) * image->scale.y;
}

void zest_SetUseFiltering(zest_texture *texture, zest_bool value) {
	if (value) {
		texture->flags |= zest_texture_flag_use_filtering;
	}
	else {
		texture->flags &= ~zest_texture_flag_use_filtering;
	}
}

void zest_SetTextureStorageType(zest_texture *texture, zest_texture_storage_type value) {
	texture->storage_type = value;
}

zest_index zest_CreateTexture(const char *name, zest_texture_storage_type storage_type, zest_texture_flags use_filtering, zest_uint reserve_images) {
	ZEST_ASSERT(!zest_map_valid_name(ZestRenderer->textures, name));	//That name already exists
	zest_texture texture = zest_NewTexture();
	if (storage_type < zest_texture_storage_type_render_target) {
		zest_SetTextureStorageType(&texture, storage_type);
		zest_SetUseFiltering(&texture, use_filtering);
	}
	zest_vec_reserve(texture.images, reserve_images);
	//texture.pipeline_index_premultiply = zest_map_get_index_by_name(ZestRenderer->pipeline_sets, "pipeline_premultiply_blend");
	//texture.pipeline_index_premultiply_depth = zest_map_get_index_by_name(ZestRenderer->pipeline_sets, "pipeline_premultiply_blend_depth");
	//texture.pipeline_index_premultiply_instanced = zest_map_get_index_by_name(ZestRenderer->pipeline_sets, "pipeline_instanced_premultiply_blend");
	//texture.pipeline_index_billboard = zest_map_get_index_by_name(ZestRenderer->pipeline_sets, "pipeline_billboard");
	//texture.imgui_pipeline_index = zest_map_get_index_by_name(ZestRenderer->pipeline_sets, "pipeline_imgui_texture");
	zest_map_insert(ZestRenderer->textures, name, texture);
	zest_vec_back(zest_GetTextures()->data).index_in_renderer = zest_map_last_index(ZestRenderer->textures);
	return zest_map_last_index(ZestRenderer->textures);
}

zest_byte *zest_BitmapArrayLookUp(zest_bitmap_array *bitmap_array, zest_index index) { 
	ZEST_ASSERT((zest_uint)index < bitmap_array->size_of_array); 
	return bitmap_array->data + index * bitmap_array->size_of_each_image; 
}

zest_image *zest_GetImageFromTexture(zest_texture *texture, zest_index index) {
	return &texture->images[index];
}

zest_texture *zest_GetTextureByIndex(zest_index index) {
	ZEST_ASSERT(zest_map_valid_index(ZestRenderer->textures, index));	//That index could not be found in the storage
	return zest_map_at_index(ZestRenderer->textures, index);
}

zest_texture *zest_GetTextureByName(const char *name) {
	ZEST_ASSERT(zest_map_valid_name(ZestRenderer->textures, name));	//That name could not be found in the storage
	return zest_map_at(ZestRenderer->textures, name);
}

void zest_SetTextureWrapping(zest_texture *texture, VkSamplerAddressMode u, VkSamplerAddressMode v, VkSamplerAddressMode w) {
	texture->sampler_info.addressModeU = u;
	texture->sampler_info.addressModeV = v;
	texture->sampler_info.addressModeW = w;
}

void zest_SetTextureWrappingClamp(zest_texture *texture) {
	texture->sampler_info.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	texture->sampler_info.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	texture->sampler_info.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
}

void zest_SetTextureWrappingBorder(zest_texture *texture) {
	texture->sampler_info.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
	texture->sampler_info.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
	texture->sampler_info.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
}

void zest_SetTextureWrappingRepeat(zest_texture *texture) {
	texture->sampler_info.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	texture->sampler_info.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	texture->sampler_info.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
}

void zest_SetTextureLayerSize(zest_texture *texture, zest_uint size) {
	ZEST_ASSERT(ZEST_POW2(size));
	texture->texture_layer_size = size;
}

//-- End Texture and Image Functions

//-- Draw Layers
//-- internal
void zest_InitialiseSpriteLayer(zest_instance_layer *sprite_layer, zest_uint instance_pool_size) {
	sprite_layer->current_color.r = 255;
	sprite_layer->current_color.g = 255;
	sprite_layer->current_color.b = 255;
	sprite_layer->current_color.a = 255;
	sprite_layer->push_constants.model = zest_M4(1);
	sprite_layer->push_constants.parameters1 = zest_Vec4Set1(0.f);
	sprite_layer->push_constants.parameters2.x = 0.f;
	sprite_layer->push_constants.parameters2.y = 0.f;
	sprite_layer->push_constants.parameters2.z = 0.25f;
	sprite_layer->push_constants.parameters2.w = 0.5f;
	sprite_layer->layer_size = zest_Vec2Set1(1.f);

	zest_buffer_info device_buffer_info = zest_CreateVertexBufferInfo();
	zest_buffer_info staging_buffer_info = zest_CreateStagingBufferInfo();
	for (ZEST_EACH_FIF_i) {
		sprite_layer->instance_memory_refs[i].device_data = zest_CreateBuffer(sizeof(zest_sprite_instance) * instance_pool_size, &device_buffer_info, ZEST_NULL);
		sprite_layer->instance_memory_refs[i].staging_data = zest_CreateBuffer(sizeof(zest_sprite_instance) * instance_pool_size, &staging_buffer_info, ZEST_NULL);
		sprite_layer->instance_memory_refs[i].instance_count = 0;
		sprite_layer->instance_memory_refs[i].instance_count = 0;
	}

	zest__sync_sprite_layer_to_current_fif(sprite_layer);

	sprite_layer->viewport_size.x = (float)zest_GetSwapChainExtent().width;
	sprite_layer->viewport_size.y = (float)zest_GetSwapChainExtent().height;

	zest_ResetDrawLayerDrawing(sprite_layer);
}

ZEST_API zest_instance_instruction zest_InstanceInstruction() {
	zest_instance_instruction instruction = { 0 };
	instruction.pipeline = ZEST_INVALID;
	return instruction;
}

void zest_ResetDrawLayerDrawing(zest_instance_layer *sprite_layer) {
	zest_vec_clear(sprite_layer->instance_instructions[ZEST_FIF]);
	//sprite_layer->flags &= ~QLayerFlags_has_instance_instructions;
	sprite_layer->instance_memory_refs[ZEST_FIF].staging_data->memory_in_use = 0;
	sprite_layer->current_instance_instruction = zest_InstanceInstruction();
	sprite_layer->instance_memory_refs[ZEST_FIF].instance_count = 0;
}

void zest__sync_sprite_layer_to_current_fif(zest_instance_layer *sprite_layer) {
	sprite_layer->synced_staging_ptr = sprite_layer->instance_memory_refs[ZEST_FIF].staging_data->data;
	sprite_layer->instance_ptr = (zest_sprite_instance*)(sprite_layer->synced_staging_ptr) + sprite_layer->instance_memory_refs[ZEST_FIF].instance_count;
}

void zest__start_draw_instructions(zest_instance_layer *instance_layer) {
	instance_layer->current_instance_instruction.start_index = instance_layer->instance_memory_refs[ZEST_FIF].instance_count ? instance_layer->instance_memory_refs[ZEST_FIF].instance_count : 0;
	instance_layer->current_instance_instruction.push_constants = instance_layer->push_constants;
}

void zest__end_draw_instructions(zest_instance_layer *instance_layer) {
	if (instance_layer->current_instance_instruction.total_instances) {
		instance_layer->last_draw_mode = zest_draw_mode_none;
		zest_vec_push(instance_layer->instance_instructions[ZEST_FIF], instance_layer->current_instance_instruction);

		instance_layer->instance_memory_refs[ZEST_FIF].staging_data->memory_in_use += instance_layer->current_instance_instruction.total_instances * sizeof(zest_sprite_instance);
		instance_layer->current_instance_instruction.total_instances = 0;
		instance_layer->current_instance_instruction.start_index = 0;
	}
	else if (instance_layer->current_instance_instruction.draw_mode == zest_draw_mode_viewport) {
		zest_vec_push(instance_layer->instance_instructions[ZEST_FIF], instance_layer->current_instance_instruction);

		instance_layer->last_draw_mode = zest_draw_mode_none;
	}
}

void zest__map_sprite_instance_to_next_fif(zest_instance_layer *sprite_layer) {
	zest_index next_fif;
	next_fif = zest__next_fif();
	sprite_layer->instance_ptr = (zest_sprite_instance*)sprite_layer->instance_memory_refs[next_fif].staging_data->data;
}

void zest__update_draw_layer_buffers_callback(zest_draw_routine *draw_routine, zest_buffer_uploader *vertex_upload) {
	zest_instance_layer *instance_layer = zest_GetLayerByIndex(draw_routine->draw_index);
	zest__end_draw_instructions(instance_layer);

	if (!zest_vec_empty(instance_layer->instance_instructions[ZEST_FIF])) {
		zest_AddCopyCommand(vertex_upload, instance_layer->instance_memory_refs[ZEST_FIF].staging_data, instance_layer->instance_memory_refs[ZEST_FIF].device_data, 0);
	}

}

void zest__draw_sprite_layer_callback(zest_draw_routine *draw_routine) {
	zest_instance_layer *sprite_layer = zest_GetLayerByIndex(draw_routine->draw_index);
	zest__draw_instance_layer(sprite_layer, zest_CurrentCommandBuffer());
	zest_ResetDrawLayerDrawing(sprite_layer);
	zest__map_sprite_instance_to_next_fif(sprite_layer);
}

void zest__update_draw_layer_resolution(zest_instance_layer *layer) {
	//GetRenderTarget(render_target_index).GetViewportSize(viewport_size.x, viewport_size.y);
	layer->viewport_size.x = (float)zest_GetSwapChainExtent().width;
	layer->viewport_size.y = (float)zest_GetSwapChainExtent().height;
	layer->screen_scale.x = layer->viewport_size.x / layer->layer_size.x;
	layer->screen_scale.y = layer->viewport_size.y / layer->layer_size.y;
}

void zest__draw_instance_layer(zest_instance_layer *instance_layer, VkCommandBuffer command_buffer) {
	VkDeviceSize instance_data_offsets[] = { instance_layer->instance_memory_refs[ZEST_FIF].device_data->memory_offset };

	for (zest_foreach_i(instance_layer->instance_instructions[ZEST_FIF])) {
		zest_instance_instruction *current = &instance_layer->instance_instructions[ZEST_FIF][i];

		if (current->draw_mode == zest_draw_mode_viewport) {
			vkCmdSetViewport(command_buffer, 0, 1, &current->viewport);
			vkCmdSetScissor(command_buffer, 0, 1, &current->scissor);
			continue;
		}
		
		vkCmdBindVertexBuffers(command_buffer, 0, 1, zest_GetBufferDeviceBuffer(instance_layer->instance_memory_refs[ZEST_FIF].device_data), instance_data_offsets);

		zest_BindPipeline(command_buffer, zest_Pipeline(current->pipeline), current->descriptor_set);
        
		vkCmdPushConstants(
			command_buffer,
			zest_Pipeline(current->pipeline)->pipeline_layout,
			VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
			0,
			sizeof(zest_push_constants),
			&current->push_constants);

		vkCmdDraw(command_buffer, 6, current->total_instances, 0, current->start_index);
	}
}

void zest__next_sprite_instance(zest_instance_layer *layer) {
	layer->instance_ptr = (zest_sprite_instance*)layer->instance_ptr + 1;
	if (layer->instance_ptr == layer->instance_memory_refs[ZEST_FIF].staging_data->end) {
		zest_bool grown = zest_GrowBuffer(&layer->instance_memory_refs[ZestDevice->current_fif].staging_data, sizeof(zest_sprite_instance));
		zest_GrowBuffer(&layer->instance_memory_refs[ZEST_FIF].device_data, sizeof(zest_sprite_instance));
		if (grown) {
			layer->instance_memory_refs[ZEST_FIF].instance_count++;
			layer->instance_ptr = (zest_sprite_instance*)layer->instance_memory_refs[ZEST_FIF].staging_data->data + layer->instance_memory_refs[ZEST_FIF].instance_count;
		}
		else {
			layer->instance_ptr = (zest_sprite_instance*)layer->instance_ptr - 1;
		}
	}
	else {
		layer->instance_memory_refs[ZEST_FIF].instance_count++;
	}
}

//-- Draw Layers API
void zest_SetViewPort(zest_instance_layer *instance_layer, int x, int y, zest_uint width, zest_uint height, float viewport_width, float viewport_height) {
	zest__end_draw_instructions(instance_layer);
	zest__start_draw_instructions(instance_layer);
	instance_layer->current_instance_instruction.draw_mode = zest_draw_mode_viewport;
	instance_layer->last_draw_mode = zest_draw_mode_viewport;
	instance_layer->current_instance_instruction.scissor = zest_CreateRect2D(width, height, x, y);
	instance_layer->current_instance_instruction.viewport = zest_CreateViewport(viewport_width, viewport_height, 0.f, 1.f);
}

void zest_SetSpriteDrawing(zest_instance_layer *sprite_layer, zest_texture *texture, zest_index pipeline_index) {
	ZEST_ASSERT(zest_map_valid_index(ZestRenderer->pipeline_sets, pipeline_index));	//That index could not be found in the pipeline storage, you must use a valid pipeline index
	zest__end_draw_instructions(sprite_layer);
	zest__start_draw_instructions(sprite_layer);
	zest__sync_sprite_layer_to_current_fif(sprite_layer);
	sprite_layer->current_instance_instruction.pipeline = pipeline_index;
	sprite_layer->current_instance_instruction.descriptor_set = texture->current_descriptor_set[ZEST_FIF];
	sprite_layer->current_instance_instruction.draw_mode = zest_draw_mode_instance;
	sprite_layer->last_draw_mode = zest_draw_mode_instance;
}

void zest_DrawSprite(zest_instance_layer *layer, zest_image *image, float x, float y, float r, float sx, float sy, float hx, float hy, zest_uint alignment, float stretch, zest_uint align_type) {
	assert(layer->current_instance_instruction.draw_mode = zest_draw_mode_instance);	//Call zest_StartSpriteDrawing before calling this function

	zest_sprite_instance *sprite = (zest_sprite_instance*)layer->instance_ptr;

	sprite->size = zest_Vec2Set(sx, sy);
	sprite->handle = zest_Vec2Set(hx, hy);
	sprite->position_rotation = zest_Vec4Set(x, y, stretch, r);
	sprite->uv = image->uv;
	sprite->intensity = layer->multiply_blend_factor;
	sprite->alignment = alignment;
	sprite->color = layer->current_color;
	sprite->image_layer_index = (image->layer << 24) + align_type;
	layer->current_instance_instruction.total_instances++;

	zest__next_sprite_instance(layer);
}
//-- End Draw Layers

zest_microsecs zest__set_elapsed_time() {
	ZestApp->current_elapsed = zest_Microsecs() - ZestApp->current_elapsed_time;
	ZestApp->current_elapsed_time = zest_Microsecs();

	return ZestApp->current_elapsed;
}

void zest__main_loop(void) {
	ZestApp->current_elapsed_time = zest_Microsecs();
	while (!glfwWindowShouldClose(ZestApp->window->window_handle)) {

		ZEST_VK_CHECK_RESULT(vkWaitForFences(ZestDevice->logical_device, 1, &ZestRenderer->fif_fence[ZEST_FIF], VK_TRUE, UINT64_MAX));
		//DoScheduledTasks(ZestDevice->current_fif);

		glfwPollEvents();

		zest__set_elapsed_time();

		if (ZestApp->update_callback) {
			ZestApp->update_callback(ZestApp->current_elapsed_time, ZestApp->user_data);
		}

		zest__draw_renderer_frame();

		if (ZestApp->flags & zest_app_flag_show_fps_in_title) {
			ZestApp->frame_timer += ZestApp->current_elapsed;
			ZestApp->frame_count++;
			if (ZestApp->frame_timer >= ZEST_MICROSECS_SECOND) {
				char fps_str[20];
                zest_snprintf(fps_str, 20, "FPS: %u", ZestApp->frame_count);
                
				glfwSetWindowTitle(ZestApp->window->window_handle, fps_str);

				ZestApp->last_fps = ZestApp->frame_count;
				ZestApp->frame_count = 0;
				ZestApp->frame_timer = ZestApp->frame_timer - ZEST_MICROSECS_SECOND;
			}
		}
	}
}

//Temp example
typedef struct zest_example {
	zest_index texture_index;
	zest_index image1;
	zest_index sprite_pipeline;
	zest_index layer;
} zest_example;

void InitExample(zest_example *example) {
	example->texture_index = zest_CreateTexture("Example Texture", zest_texture_storage_type_sprite_sheet, zest_texture_flag_use_filtering, 10);
	zest_texture *texture = zest_GetTextureByIndex(example->texture_index);
	example->image1 = zest_LoadImageFile(texture, "examples/wabbit_alpha.png");
	zest_ProcessTextureImages(texture);
	example->sprite_pipeline = zest_PipelineIndex("pipeline_2d_sprites");
	example->layer = zest_GetLayerIndex("Sprite 2d Layer");
}

void test_update_callback(zest_microsecs elapsed, void *user_data) {
	zest_example *example = (zest_example*)user_data;
	zest_instance_layer *layer = zest_GetLayerByIndex(example->layer);
	zest_texture *texture = zest_GetTextureByIndex(example->texture_index);
	zest_SetActiveRenderQueue(0);
	layer->multiply_blend_factor = 1.f;
	//zest_SetViewPort(layer, 0, 0, 300, 300, zest_ScreenWidthf(), zest_ScreenHeightf());
	zest_SetSpriteDrawing(layer, texture, example->sprite_pipeline);
	for (float x = 0; x != 75; ++x) {
		for (float y = 0; y != 15; ++y) {
			layer->current_color.r = (zest_byte)(1 - ((y + 1) / 16.f) * 255.f);
			layer->current_color.g = (zest_byte)(y / 15.f * 255.f);
			layer->current_color.b = (zest_byte)(x / 75.f * 255.f);
			zest_DrawSprite(layer, zest_GetImageFromTexture(texture, example->image1), x * 16.f + 20.f, y * 40.f + 20.f, 0.f, 32.f, 32.f, 0.5f, 0.5f, 0, 0.f, 0);
		}
	}
}

int main(void) {
	zest_example example;

	zest_create_info create_info = zest_CreateInfo();

	zest_Initialise(&create_info);
	zest_SetUserData(&example);
	zest_SetUserCallback(test_update_callback);
    
	InitExample(&example);
	zest_ShowFPSInTitle();

	zest_Start();

	return 0;
}
