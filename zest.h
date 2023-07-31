
#ifndef  ZEST_RENDERER_H
#define ZEST_RENDERER_H

#include <vulkan/vulkan.h>
#include <GLFW/glfw3.h>
#include "2loc.h"
#include <stdio.h>

#ifndef ZEST_MAX_FIF
#define ZEST_MAX_FIF
#endif

#ifndef zest_assert
#define zest_assert assert
#endif

#ifndef ZEST_API
#define ZEST_API
#endif

#ifndef _DEBUG
#define ZEST_ENABLE_VALIDATION_LAYER 0
#else
#define ZEST_ENABLE_VALIDATION_LAYER 1
#endif

#ifndef zest__free
#define zest__free(memory) tloc_Free(ZestDevice->allocator, memory)
#endif

//Helper macros
#define zest__Min(a, b) (((a) < (b)) ? (a) : (b))
#define zest__Max(a, b) (((a) > (b)) ? (a) : (b))

#ifndef zest__allocate
#define zest__allocate(size) tloc_Allocate(ZestDevice->allocator, zest__Max(size, tloc__MINIMUM_BLOCK_SIZE))
#endif
#define zest__array(name, type, count) type *name = zest__allocate(zest__Max(sizeof(type) * count, tloc__MINIMUM_BLOCK_SIZE))

#define zest_null 0
#define zest__vk_create_info(name, type) type name; memset(&name, 0, sizeof(type))

//For error checking vulkan commands
#define ZEST_VK_CHECK_RESULT(f)																				\
	{																										\
		VkResult res = (f);																					\
		if (res != VK_SUCCESS)																				\
		{																									\
			printf("Fatal : VkResult is \" %s \" in %s at line %i\n", zest__vulkan_error(res), __FILE__, __LINE__);	\
			assert(res == VK_SUCCESS);																		\
		}																									\
	}

const char *zest__vulkan_error(VkResult errorCode);

typedef unsigned int zest_uint;
typedef size_t zest_size;
typedef unsigned int zest_bool;

//Callback typedefs
typedef void(*zest_keyboard_input_callback)(GLFWwindow* window, int key, int scancode, int action, int mods);

//enums and flags
typedef enum zest_create_flags {
	zest_create_flag_none							= 0,
	zest_create_flag_hide_mouse_pointer				= 1 << 0,		//Start the app with the mouse pointer hidden
	zest_create_flag_maximised						= 1 << 1,		//Start the application maximized
	zest_create_flag_initialise_with_command_queue	= 1 << 2,		//A render queue with layer draw operations will be createdon app initialization
	zest_create_flag_enable_console					= 1 << 3,		//Redundant, will probably redo a console using imgui
	zest_create_flag_enable_vsync					= 1 << 4,		//Enable vysnc
	zest_create_flag_show_fps_in_title				= 1 << 5,		//Show the FPS in the window title
	zest_create_flag_use_imgui						= 1 << 6,		//Initialise the App with ImGui
	zest_create_flag_create_imgui_layer				= 1 << 7,		//Add a layer to the default render queue created with the app. NA if initialise_with_command_queue = false or use_imgui = false
	zest_create_flag_enable_multi_sampling			= 1 << 8,		//Enable multisampling anti aliasing. May remove this at some point, not sure.
	zest_create_flag_output_memory_usage_on_exit	= 1 << 9		//Outputs a report on the amount of memory used by the various vulkan buffers.
} zest_create_flags;

typedef enum zest_app_flags {
	zest_app_flag_suspend_rendering =		1 << 0,
	zest_app_flag_show_fps_in_title =		1 << 1,
	zest_app_flag_shift_pressed =			1 << 2,
	zest_app_flag_control_pressed =			1 << 3,
	zest_app_flag_cmd_pressed =				1 << 4,
	zest_app_flag_record_input  =			1 << 5,
	zest_app_flag_enable_console =			1 << 6
} zest_app_flags;

enum zest__constants {
	zest__validation_layer_count = 1,
	zest__required_extension_names_count = 1,
};

//Private structs with inline functions
typedef struct zest_queue_family_indices {
	zest_uint graphics_family;
	zest_uint present_family;
	zest_uint compute_family;

	zest_bool graphics_set;
	zest_bool present_set;
	zest_bool compute_set;
} zest_queue_family_indices;

static inline void zest__set_graphics_family(zest_queue_family_indices *family, zest_uint v) { family->graphics_family = v; family->graphics_set = 1; }
static inline void zest__set_present_family(zest_queue_family_indices *family, zest_uint v) { family->present_family = v; family->present_set = 1; }
static inline void zest__set_compute_family(zest_queue_family_indices *family, zest_uint v) { family->compute_family = v; family->compute_set = 1; }
static inline zest_bool zest__family_is_complete(zest_queue_family_indices *family) { return family->graphics_set && family->present_set && family->compute_set; }

typedef struct zest_swap_chain_support_details {
	VkSurfaceCapabilitiesKHR capabilities;
	VkSurfaceFormatKHR *formats;
	zest_uint formats_count;
	VkPresentModeKHR *present_modes;
	zest_uint present_modes_count;
} zest_swap_chain_support_details;

//structs
typedef struct zest_window {
	GLFWwindow *window_handle;
	VkSurfaceKHR surface;
	zest_uint window_width;
	zest_uint window_height;
	zest_bool framebuffer_resized;
	zest_create_flags flags;
} zest_window;

typedef struct zest_device {
	zest_uint api_version;
	zest_uint use_labels_address_bit;
	zest_uint current_fif;
	zest_uint max_fif;
	zest_uint max_image_size;
	zest_uint graphics_queue_family_index;
	zest_uint present_queue_family_index;
	zest_uint compute_queue_family_index;
	void *memory_pools[32];
	zest_uint memory_pool_count;
	tloc_allocator *allocator;
	VkInstance instance;
	VkPhysicalDevice physical_device;
	VkDevice logical_device;
	VkDebugUtilsMessengerEXT debug_messenger;
	VkSampleCountFlagBits msaa_samples;
	zest_swap_chain_support_details swap_chain_support_details;
	VkPhysicalDeviceProperties properties;
	VkPhysicalDeviceFeatures features;
	VkPhysicalDeviceMemoryProperties memory_properties;
	VkQueue present_queue;
	VkQueue graphics_queue;
	VkQueue one_time_graphics_queue;
	VkQueue compute_queue;
	VkCommandPool command_pool;
	PFN_vkSetDebugUtilsObjectNameEXT pfnSetDebugUtilsObjectNameEXT;
	VkFormat color_format;
} zest_device;

typedef struct zest_create_info {
	int screen_width, screen_height;					//Default width and height of the window that you open
	int screen_x, screen_y;								//Default position of the window
	int virtual_width, virtual_height;					//The virtial width/height of the viewport
	VkFormat color_format;								//Choose between VK_FORMAT_R8G8B8A8_UNORM and VK_FORMAT_R8G8B8A8_SRGB

	//The starting sizes of various buffers. Whilst these will be resized as necessary, they are best set to a size so they have enough space and do not need to be resized at all.
	//Set output_memory_usage_on_exit to true so that you can determine the sizes as you develop your application
	//All values are the size in bytes
} zest_create_info;

typedef struct zest_app {
	zest_create_info create_info;

	void(*update_callback)(float, void*);
	void *data;

	zest_window *window;

	float current_elapsed;
	float update_time;
	float render_time;
	float frame_timer;

	double mouse_x;
	double mouse_y;
	double mouse_wheel_v;
	double mouse_wheel_h;
	double mouse_delta_x;
	double mouse_delta_y;
	double last_mouse_x;
	double last_mouse_y;
	double virtual_mouse_x;
	double virtual_mouse_y;

	zest_uint frame_count;
	zest_uint last_fps;

	zest_uint default_layer_index;
	zest_uint default_command_queue_index;
	zest_uint default_render_commands_index;
	zest_uint default_font_index;

	zest_app_flags flags;
} zest_app;

static zest_device *ZestDevice = 0;
static zest_app *ZestApp = 0;

static const char* zest_validation_layers[zest__validation_layer_count] = {
	"VK_LAYER_KHRONOS_validation"
};

static const char* zest_required_extensions[zest__required_extension_names_count] = {
	VK_KHR_SWAPCHAIN_EXTENSION_NAME
};

//User API functions
ZEST_API void zest_Initialise();
ZEST_API void zest_Start();

void zest__initialise_app();
void zest__initialise_device();
void zest__destroy();
zest_window* zest__create_window(int x, int y, int width, int height, zest_bool maximised, const char*);
void zest__create_window_surface(zest_window*);
void zest__main_loop();
void zest__keyboard_input_callback(GLFWwindow* window, int key, int scancode, int action, int mods);
void zest__mouse_scroll_callback(GLFWwindow* window, double offset_x, double offset_y);
void zest__framebuffer_size_callback(GLFWwindow* window, int width, int height);

//Device set up 
void zest__create_instance();
void zest__create_window_surface(zest_window* window);
void zest__setup_validation();
static VKAPI_ATTR VkBool32 VKAPI_CALL zest_debug_callback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageType, const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData);
VkResult zest_create_debug_messenger(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pDebugMessenger);
void zest_destroy_debug_messenger();
void zest__pick_physical_device();
zest_bool zest__is_device_suitable(VkPhysicalDevice physical_device);
zest_queue_family_indices zest__find_queue_families(VkPhysicalDevice physical_device);
zest_bool zest__check_device_extension_support(VkPhysicalDevice physical_device);
zest_swap_chain_support_details zest__query_swap_chain_support(VkPhysicalDevice physical_device);
VkSampleCountFlagBits zest__get_max_useable_sample_count();
void zest__create_logical_device();
void zest__set_limit_data();
zest_bool zest__check_validation_layer_support();
const char** zest__get_required_extensions(zest_uint *extension_count);

#endif // ! ZEST_RENDERER
