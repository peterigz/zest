
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

#define ZEST_INVALID 0xFFFFFFFF

#ifndef zest__free
#define zest__free(memory) tloc_Free(ZestDevice->allocator, memory)
#endif

//Helper macros
#define zest__Min(a, b) (((a) < (b)) ? (a) : (b))
#define zest__Max(a, b) (((a) > (b)) ? (a) : (b))

#ifndef zest__reallocate
#define zest__allocate(size) tloc_Allocate(ZestDevice->allocator, size)
#define zest__reallocate(ptr, size) tloc_Reallocate(ZestDevice->allocator, ptr, size)
#endif
#define zest__array(name, type, count) type *name = zest__reallocate(0, sizeof(type) * count)

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
typedef int zest_index;
typedef unsigned long long zest_ull;
typedef zest_ull zest_key;
typedef size_t zest_size;
typedef unsigned int zest_bool;
#define zest_true 1
#define zest_false 0

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

// --Quick and dirty dynamic array
typedef struct zest_vec {
	zest_uint current_size;
	zest_uint capacity;
} zest_vec;

enum {
	zest__VEC_HEADER_OVERHEAD = sizeof(zest_vec)
};

#define zest__vec_header(T) ((zest_vec*)T - 1)
inline void* zest__vec_reserve(void *T, zest_uint unit_size, zest_uint new_capacity) { if (T && new_capacity <= zest__vec_header(T)->capacity) return T; void* new_data = zest__reallocate((T ? zest__vec_header(T) : T), new_capacity * unit_size + zest__VEC_HEADER_OVERHEAD); if (!T) memset(new_data, 0, zest__VEC_HEADER_OVERHEAD); T = ((char*)new_data + zest__VEC_HEADER_OVERHEAD); ((zest_vec*)(new_data))->capacity = new_capacity; return T; }
inline zest_uint zest__grow_capacity(void *T, zest_uint size) { zest_uint new_capacity = T ? (size + size / 2) : 8; return new_capacity > size ? new_capacity : size; }
#define zest_vec_bump(T) zest__vec_header(T)->current_size++;
#define zest_vec_clip(T) zest__vec_header(T)->current_size--;
#define zest_vec_grow(T) ((!(T) || (zest__vec_header(T)->current_size == zest__vec_header(T)->capacity)) ? T = zest__vec_reserve((T), sizeof(*T), (T ? zest__grow_capacity(T, zest__vec_header(T)->current_size) : 8)) : 0)
#define zest_vec(T, name) T *name = zest_null
#define zest_vec_empty(T) (zest__vec_header(T)->current_size == 0)
#define zest_vec_size(T) (zest__vec_header(T)->current_size)
#define zest_vec_capacity(T) (zest__vec_header(T)->capacity)
#define zest_vec_size_in_bytes(T) (zest__vec_header(T)->current_size * sizeof(*T))
#define zest_vec_front(T) (T[0])
#define zest_vec_back(T) (T[zest__vec_header(T)->current_size - 1])
#define zest_vec_end(T) (&(T[zest_vec_size(T)]))
#define zest_vec_clear(T) (zest__vec_header(T)->current_size = 0)
#define zest_vec_free(T) (zest__free(zest__vec_header(T)), T = zest_null)
#define zest_vec_resize(T, new_size) if(zest__vec_header(T)->capacity < new_size) T = zest__vec_reserve(T, sizeof(*T), new_size); zest__vec_header(T)->current_size = new_size
#define zest_vec_push(T, value) zest_vec_grow(T); (T)[zest__vec_header(T)->current_size++] = value 
#define zest_vec_pop(T) (zest__vec_header(T)->current_size--, T[zest__vec_header(T)->current_size])
#define zest_vec_insert(T, location, value) { ptrdiff_t offset = location - T; zest_vec_grow(T); if(offset < zest_vec_size(T)) memmove(T + offset + 1, T + offset, ((size_t)zest_vec_size(T) - offset) * sizeof(*T)); T[offset] = value; zest_vec_bump(T) }
#define zest_vec_erase(T, location) { zest_assert(T && location >= T && location < zest_vec_end(T)); ptrdiff_t offset = location - T; memmove(T + offset, T + offset + 1, ((size_t)zest_vec_size(T) - offset) * sizeof(*T)); zest_vec_clip(T); }
#define zest_foreach_i(T) int i = 0; i != zest_vec_size(T); ++i 
#define zest_foreach_j(T) int j = 0; j != zest_vec_size(T); ++j 
#define zest_foreach_k(T) int k = 0; k != zest_vec_size(T); ++k 
// --end of quick and dirty dynamic array

// --Quick and dirty hasher, converted to c from Stephen Brumme's XXHash code
/*
	MIT License Copyright (c) 2018 Stephan Brumme
	Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
	The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
	THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.  */
#define zest__PRIME1 11400714785074694791ULL
#define zest__PRIME2 14029467366897019727ULL
#define zest__PRIME3 1609587929392839161ULL
#define zest__PRIME4 9650029242287828579ULL
#define zest__PRIME5 2870177450012600261ULL

enum { zest__HASH_MAX_BUFFER_SIZE = 31 + 1 };

typedef struct zest_hasher {
	zest_ull      state[4];
	unsigned char buffer[zest__HASH_MAX_BUFFER_SIZE];
	zest_ull      buffer_size;
	zest_ull      total_length;
} zest_hasher;

static inline zest_ull zest__hash_rotate_left(zest_hasher *hasher, zest_ull x, unsigned char bits) { return (x << bits) | (x >> (64 - bits)); }
static inline zest_ull zest__hash_process_single(zest_hasher *hasher, zest_ull previous, zest_ull input) { return zest__hash_rotate_left(hasher, previous + input * zest__PRIME2, 31) * zest__PRIME1; }
static inline void zest__hasher_process(zest_hasher *hasher, void* data, zest_ull *state0, zest_ull *state1, zest_ull *state2, zest_ull *state3) { const zest_ull* block = (const zest_ull*)data; *state0 = zest__hash_process_single(hasher, *state0, block[0]); *state1 = zest__hash_process_single(hasher, *state1, block[1]); *state2 = zest__hash_process_single(hasher, *state2, block[2]); *state3 = zest__hash_process_single(hasher, *state3, block[3]); }
static inline zest_bool zest__hasher_add(zest_hasher *hasher, void* input, zest_ull length)
{
	if (!input || length == 0) return zest_false;

	hasher->total_length += length;
	unsigned char* data = (unsigned char*)input;

	if (hasher->buffer_size + length < zest__HASH_MAX_BUFFER_SIZE)
	{
		while (length-- > 0)
			hasher->buffer[hasher->buffer_size++] = *data++;
		return zest_true;
	}

	const unsigned char* stop = data + length;
	const unsigned char* stopBlock = stop - zest__HASH_MAX_BUFFER_SIZE;

	if (hasher->buffer_size > 0)
	{
		while (hasher->buffer_size < zest__HASH_MAX_BUFFER_SIZE)
			hasher->buffer[hasher->buffer_size++] = *data++;

		zest__hasher_process(hasher, hasher->buffer, &hasher->state[0], &hasher->state[1], &hasher->state[2], &hasher->state[3]);
	}

	zest_ull s0 = hasher->state[0], s1 = hasher->state[1], s2 = hasher->state[2], s3 = hasher->state[3];
	while (data <= stopBlock)
	{
		zest__hasher_process(hasher, data, &s0, &s1, &s2, &s3);
		data += 32;
	}
	hasher->state[0] = s0; hasher->state[1] = s1; hasher->state[2] = s2; hasher->state[3] = s3;

	hasher->buffer_size = stop - data;
	for (zest_ull i = 0; i < hasher->buffer_size; i++)
		hasher->buffer[i] = data[i];

	return zest_true;
}

static inline zest_ull zest__get_hash(zest_hasher *hasher)
{
	zest_ull result;
	if (hasher->total_length >= zest__HASH_MAX_BUFFER_SIZE)
	{
		result = zest__hash_rotate_left(hasher, hasher->state[0], 1) +
			zest__hash_rotate_left(hasher, hasher->state[1], 7) +
			zest__hash_rotate_left(hasher, hasher->state[2], 12) +
			zest__hash_rotate_left(hasher, hasher->state[3], 18);
		result = (result ^ zest__hash_process_single(hasher, 0, hasher->state[0])) * zest__PRIME1 + zest__PRIME4;
		result = (result ^ zest__hash_process_single(hasher, 0, hasher->state[1])) * zest__PRIME1 + zest__PRIME4;
		result = (result ^ zest__hash_process_single(hasher, 0, hasher->state[2])) * zest__PRIME1 + zest__PRIME4;
		result = (result ^ zest__hash_process_single(hasher, 0, hasher->state[3])) * zest__PRIME1 + zest__PRIME4;
	}
	else
	{
		result = hasher->state[2] + zest__PRIME5;
	}

	result += hasher->total_length;
	const unsigned char* data = hasher->buffer;
	const unsigned char* stop = data + hasher->buffer_size;
	for (; data + 8 <= stop; data += 8)
		result = zest__hash_rotate_left(hasher, result ^ zest__hash_process_single(hasher, 0, *(zest_ull*)data), 27) * zest__PRIME1 + zest__PRIME4;
	if (data + 4 <= stop)
	{
		result = zest__hash_rotate_left(hasher, result ^ (*(uint32_t*)data) * zest__PRIME1, 23) * zest__PRIME2 + zest__PRIME3;
		data += 4;
	}
	while (data != stop)
		result = zest__hash_rotate_left(hasher, result ^ (*data++) * zest__PRIME5, 11) * zest__PRIME1;
	result ^= result >> 33;
	result *= zest__PRIME2;
	result ^= result >> 29;
	result *= zest__PRIME3;
	result ^= result >> 32;
	return result;
}
inline void zest__hash_initialise(zest_hasher *hasher, zest_ull seed) { hasher->state[0] = seed + zest__PRIME1 + zest__PRIME2; hasher->state[1] = seed + zest__PRIME2; hasher->state[2] = seed; hasher->state[3] = seed - zest__PRIME1; hasher->buffer_size = 0; hasher->total_length = 0; }

//The only command you need for the hasher
inline zest_key zest_Hash(zest_hasher *hasher, void* input, zest_ull length, zest_ull seed) { zest__hash_initialise(hasher, seed); zest__hasher_add(hasher, input, length); return (zest_key)zest__get_hash(hasher); }
//-- End of hashing

// --Begin quick and dirty hash map
typedef struct {
	zest_key key;
	zest_index index;
} zest_hash_pair;

#ifndef ZEST_HASH_SEED
#define ZEST_HASH_SEED 0xABCDEF99
#endif
zest_hash_pair* zest__lower_bound(zest_hash_pair *map, zest_key key) { zest_hash_pair *first = map; zest_hash_pair *last = map ? zest_vec_end(map) : 0; size_t count = (size_t)(last - first); while (count > 0) { size_t count2 = count >> 1; zest_hash_pair* mid = first + count2; if (mid->key < key) { first = ++mid; count -= count2 + 1; } else { count = count2; } } return first; }
void zest__map_realign_indexes(zest_hash_pair *map, zest_index index) { for (zest_foreach_i(map)) { if (map[i].index < index) continue; map[i].index--; } }
zest_index zest__map_get_index(zest_hash_pair *map, zest_key key) { zest_hash_pair *it = zest__lower_bound(map, key); return (it == zest_vec_end(map) || it->key != key) ? -1 : it->index; }
#define zest_map_hash(hash_map, name) zest_Hash(&hash_map.hasher, name, strlen(name), ZEST_HASH_SEED) 
#define zest_hash_map(T) typedef struct { zest_hasher hasher; zest_hash_pair *map; T *data; }
#define zest_map_valid_name(hash_map, name) (zest__map_get_index(hash_map.map, zest_map_hash(hash_map, name)) != -1)
#define zest_map_set_index(hash_map, key, value) zest_hash_pair *it = zest__lower_bound(hash_map.map, key); if(!hash_map.map || it == zest_vec_end(hash_map.map) || it->key != key) { zest_vec_push(hash_map.data, value); zest_hash_pair new_pair; new_pair.key = key; new_pair.index = zest_vec_size(hash_map.data) - 1; zest_vec_insert(hash_map.map, it, new_pair); } else {hash_map.data[it->index] = value;}
#define zest_map_insert(hash_map, name, value) { zest_key key = zest_map_hash(hash_map, name); zest_map_set_index(hash_map, key, value) }
#define zest_map_at(hash_map, name) &hash_map.data[zest__map_get_index(hash_map.map, zest_map_hash(hash_map, name))]
#define zest_map_remove(hash_map, name) {zest_key key = zest_map_hash(hash_map, name); zest_hash_pair *it = zest__lower_bound(hash_map.map, key); zest_index index = it->index; zest_vec_erase(hash_map.map, it); zest_vec_erase(hash_map.data, &hash_map.data[index]); zest__map_realign_indexes(hash_map.map, index); }
// --End quick and dirty hash map

//--Internal functions
//Device set up 
void zest__create_instance(void);
void zest__create_window_surface(zest_window* window);
void zest__setup_validation(void);
static VKAPI_ATTR VkBool32 VKAPI_CALL zest_debug_callback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageType, const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData);
VkResult zest_create_debug_messenger(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pDebugMessenger);
void zest_destroy_debug_messenger(void);
void zest__pick_physical_device(void);
zest_bool zest__is_device_suitable(VkPhysicalDevice physical_device);
zest_queue_family_indices zest__find_queue_families(VkPhysicalDevice physical_device);
zest_bool zest__check_device_extension_support(VkPhysicalDevice physical_device);
zest_swap_chain_support_details zest__query_swap_chain_support(VkPhysicalDevice physical_device);
VkSampleCountFlagBits zest__get_max_useable_sample_count(void);
void zest__create_logical_device(void);
void zest__set_limit_data(void);
zest_bool zest__check_validation_layer_support(void);
const char** zest__get_required_extensions(zest_uint *extension_count);
//end device setup functions

//App initialise functions
void zest__initialise_app();
void zest__initialise_device();
void zest__destroy();
zest_window* zest__create_window(int x, int y, int width, int height, zest_bool maximised, const char*);
void zest__create_window_surface(zest_window*);
void zest__main_loop();
void zest__keyboard_input_callback(GLFWwindow* window, int key, int scancode, int action, int mods);
void zest__mouse_scroll_callback(GLFWwindow* window, double offset_x, double offset_y);
void zest__framebuffer_size_callback(GLFWwindow* window, int width, int height);
//-- end of internal functions

//User API functions
ZEST_API void zest_Initialise();
ZEST_API void zest_Start();

#endif // ! ZEST_RENDERER
