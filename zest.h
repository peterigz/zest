
#ifndef  ZEST_RENDERER_H
#define ZEST_RENDERER_H

#include "vulkan.h"
#include "glfw3.h"
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

#endif // ! ZEST_RENDERER