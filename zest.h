//Zest - A Vulkan Pocket Renderer
#ifndef ZEST_RENDERER_H
#define ZEST_RENDERER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <vulkan/vulkan.h>
#include <math.h>
#include <string.h>

#ifndef ZEST_MAX_FIF
//The maximum number of frames in flight. If you're very tight on memory then 1 will use less resources.
#define ZEST_MAX_FIF 2
#endif

#ifndef ZEST_ASSERT
#define ZEST_ASSERT assert
#endif

#ifndef ZEST_API
#define ZEST_API
#endif

#ifndef ZEST_ENABLE_VALIDATION_LAYER
#define ZEST_ENABLE_VALIDATION_LAYER 0
#endif

//Helper macros
#define ZEST__MIN(a, b) (((a) < (b)) ? (a) : (b))
#define ZEST__MAX(a, b) (((a) > (b)) ? (a) : (b))
#define ZEST__CLAMP(v, min_v, max_v) ((v < min_v) ? min_v : ((v > max_v) ? max_v : v))
#define ZEST__POW2(x) ((x) && !((x) & ((x) - 1)))
#define ZEST__FLAG(flag, bit) flag |= (bit)
#define ZEST__UNFLAG(flag, bit) flag &= ~bit
#define ZEST__FLAGGED(flag, bit) (flag & (bit)) > 0
#define ZEST__NOT_FLAGGED(flag, bit) (flag & (bit)) == 0

//Override this if you'd prefer a different way to allocate the pools for sub allocation in host memory.
#ifndef ZEST__ALLOCATE_POOL
#define ZEST__ALLOCATE_POOL malloc
#endif

#ifndef ZEST__FREE
#define ZEST__FREE(memory) zloc_Free(ZestDevice->allocator, memory)
#endif

#ifndef ZEST__REALLOCATE
#define ZEST__ALLOCATE(size) zest__allocate(size)
#define ZEST__ALLOCATE_ALIGNED(size, alignment) zest__allocate_aligned(size, alignment)
#define ZEST__REALLOCATE(ptr, size) zest__reallocate(ptr, size)
#endif

#define STBI_MALLOC(sz)           ZEST__ALLOCATE(sz)
#define STBI_REALLOC(p,newsz)     ZEST__REALLOCATE(p,newsz)
#define STBI_FREE(p)              ZEST__FREE(p)

#define ZLOC_ENABLE_REMOTE_MEMORY
#include "zloc.h"
#include "lib_bundle.h"

#ifndef ZEST_WARNING_COLOR
#define ZEST_WARNING_COLOR "\033[38;5;208m"
#endif

#ifndef ZEST_NOTICE_COLOR
#define ZEST_NOTICE_COLOR "\033[0m"
#endif

#define ZEST_OUTPUT_WARNING_MESSAGES
#ifdef ZEST_OUTPUT_WARNING_MESSAGES
#include <stdio.h>
#define ZEST_PRINT_WARNING(message_f, ...) printf(message_f"\n\033[0m", __VA_ARGS__)
#else
#define ZEST_PRINT_WARNING(message_f, ...)
#endif

#define ZEST_OUTPUT_NOTICE_MESSAGES
#ifdef ZEST_OUTPUT_NOTICE_MESSAGES
#include <stdio.h>
#define ZEST_PRINT_NOTICE(message_f, ...) printf(message_f"\n\033[0m", __VA_ARGS__)
#else
#define ZEST_PRINT_NOTICE(message_f, ...)
#endif

#define ZEST__ARRAY(name, type, count) type *name = ZEST__REALLOCATE(0, sizeof(type) * count)
#define ZEST_FIF ZestDevice->current_fif
#define ZEST_MICROSECS_SECOND 1000000ULL
#define ZEST_MICROSECS_MILLISECOND 1000

#ifndef ZEST_MAX_DEVICE_MEMORY_POOLS
#define ZEST_MAX_DEVICE_MEMORY_POOLS 64
#endif

#define ZEST_NULL 0
//For each frame in flight macro
#define ZEST_EACH_FIF_i unsigned int i = 0; i != ZEST_MAX_FIF; ++i

//For error checking vulkan commands
#define ZEST_VK_CHECK_RESULT(res)																					\
	{																												\
		if (res != VK_SUCCESS)																						\
		{																											\
			printf("Fatal : VkResult is \" %s \" in %s at line %i\n", zest__vulkan_error(res), __FILE__, __LINE__);	\
			ZEST_ASSERT(res == VK_SUCCESS);																			\
		}																											\
	}

const char *zest__vulkan_error(VkResult errorCode);

typedef unsigned int zest_uint;
typedef int zest_index;
typedef unsigned long long zest_ull;
typedef zest_uint zest_millisecs;
typedef zest_ull zest_microsecs;
typedef zest_ull zest_key;
typedef size_t zest_size;
typedef unsigned char zest_byte;
typedef unsigned int zest_bool;
typedef VkVertexInputAttributeDescription *zest_vertex_input_descriptions;

//Handles. These are pointers that remain stable until the object is freed.
#define ZEST__MAKE_HANDLE(handle) typedef struct handle##_t* handle;

//For allocating a new object with handle. Only used internally.
#define ZEST__NEW(type) ZEST__ALLOCATE(sizeof(type##_t))
#define ZEST__NEW_ALIGNED(type, alignment) ZEST__ALLOCATE_ALIGNED(sizeof(type##_t), alignment)

//For global variables
#define ZEST_GLOBAL static
//For internal private functions that aren't meant to be called outside of the library
//If there's a case for where it would be useful be able to call a function externally
//then we change it to a ZEST_API function
#define ZEST_PRIVATE static

typedef union zest_packed10bit
{
	struct
	{
		int x : 10;
		int y : 10;
		int z : 10;
		int w : 2;
	} data;
	zest_uint pack;
} zest_packed10bit;

typedef union zest_packed8bit
{
	struct
	{
		int x : 8;
		int y : 8;
		int z : 8;
		int w : 8;
	} data;
	zest_uint pack;
} zest_packed8bit;

/*Platform specific code*/

FILE *zest__open_file(const char *file_name, const char *mode);
ZEST_API zest_millisecs zest_Millisecs(void);
ZEST_API zest_microsecs zest_Microsecs(void);

#if defined (_WIN32)
#include <windows.h>
#include "vulkan/vulkan_win32.h"
#define zest_snprintf(buffer, bufferSize, format, ...) sprintf_s(buffer, bufferSize, format, __VA_ARGS__)
#define zest_strcat(left, size, right) strcat_s(left, size, right)
#define zest_strcpy(left, size, right) strcpy_s(left, size, right)
#define ZEST_ALIGN_PREFIX(v) __declspec(align(v))
#define ZEST_ALIGN_AFFIX(v)
#define ZEST_PROTOTYPE

//Window creation
int main(void);
ZEST_PRIVATE HINSTANCE zest_window_instance;
#define ZEST_WINDOW_INSTANCE HINSTANCE
LRESULT CALLBACK zest__window_proc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
//--

#else
#include <time.h>
//We'll just use glfw on mac for now. Can maybe add basic cocoa windows later
#include <glfw/glfw3.h>
#define ZEST_ALIGN_PREFIX(v) 
#define ZEST_ALIGN_AFFIX(v)  __attribute__((aligned(16)))
#define ZEST_PROTOTYPE void
#define zest_snprintf(buffer, bufferSize, format, ...) snprintf(buffer, bufferSize, format, __VA_ARGS__)
#define zest_strcat(left, size, right) strcat(left, right)
#define zest_strcpy(left, size, right) strcpy(left, right)

//Window creation
//--

#endif
/*end of platform specific code*/

#define ZEST_TRUE 1
#define ZEST_FALSE 0
#define ZEST_INVALID 0xFFFFFFFF
#define ZEST_U32_MAX_VALUE ((zest_uint)-1)

//enums and flags
typedef enum zest_struct_type {
	zest_stuct_type_texture = VK_STRUCTURE_TYPE_MAX_ENUM - 1,
} zest_struct_type;

typedef enum zest_app_flags_e {
	zest_app_flag_none =					0,
	zest_app_flag_suspend_rendering =		1 << 0,
	zest_app_flag_shift_pressed =			1 << 1,
	zest_app_flag_control_pressed =			1 << 2,
	zest_app_flag_cmd_pressed =				1 << 3,
	zest_app_flag_record_input  =			1 << 4,
	zest_app_flag_enable_console =			1 << 5,
	zest_app_flag_quit_application =		1 << 6,
	zest_app_flag_output_fps =				1 << 7
} zest_app_flags_e;

typedef zest_uint zest_app_flags;

enum zest__constants {
	zest__validation_layer_count = 1,
	zest__required_extension_names_count = 1,
};

typedef enum {
	zest_buffer_flag_none = 0,
	zest_buffer_flag_read_only = 1 << 0,
	zest_buffer_flag_is_fif = 1 << 1
} zest_buffer_flags_e;

typedef zest_uint zest_buffer_flags;

typedef enum {
	zest_render_viewport_type_scale_with_window,
	zest_render_viewport_type_fixed
} zest_render_viewport_type;

typedef enum {
	zest_setup_context_type_none,
	zest_setup_context_type_command_queue,
	zest_setup_context_type_render_pass,
	zest_setup_context_type_draw_routine,
	zest_setup_context_type_layer,
	zest_setup_context_type_compute
} zest_setup_context_type;

typedef enum zest_command_queue_flags_e {
	zest_command_queue_flag_none								= 0,
	zest_command_queue_flag_present_dependency					= 1 << 0,
	zest_command_queue_flag_command_queue_dependency			= 1 << 1,
	zest_command_queue_flag_validated							= 1 << 2,
} zest_command_queue_flags_e;

typedef zest_uint zest_command_queue_flags;

typedef enum {
	zest_renderer_flag_enable_multisampling						= 1 << 0,
	zest_renderer_flag_schedule_recreate_textures 				= 1 << 1,
	zest_renderer_flag_schedule_change_vsync 					= 1 << 2,
	zest_renderer_flag_schedule_rerecord_final_render_buffer 	= 1 << 3,
	zest_renderer_flag_drawing_loop_running 					= 1 << 4,
	zest_renderer_flag_msaa_toggled 							= 1 << 5,
	zest_renderer_flag_vsync_enabled 							= 1 << 6,
	zest_renderer_flag_disable_default_uniform_update 			= 1 << 7,
	zest_renderer_flag_swapchain_was_recreated 					= 1 << 8,
	zest_renderer_flag_has_depth_buffer		 					= 1 << 9,
} zest_renderer_flags_e;

typedef zest_uint zest_renderer_flags;

typedef enum {
	zest_pipeline_set_flag_none										= 0,	
	zest_pipeline_set_flag_is_render_target_pipeline				= 1 << 0,		//True if this pipeline is used for the final render of a render target to the swap chain
	zest_pipeline_set_flag_match_swapchain_view_extent_on_rebuild	= 1 << 1		//True if the pipeline should update it's view extent when the swap chain is recreated (the window is resized)
} zest_pipeline_set_flags_e;

typedef zest_uint zest_pipeline_set_flags;

typedef enum {
	zest_init_flag_none									= 0,
	zest_init_flag_initialise_with_command_queue		= 1 << 0,
	zest_init_flag_use_depth_buffer						= 1 << 1,
	zest_init_flag_enable_vsync							= 1 << 6,
} zest_init_flags_e;

typedef zest_uint zest_init_flags;

typedef enum {
	zest_buffer_upload_flag_initialised					= 1 << 0,				//Set to true once AddCopyCommand has been run at least once
	zest_buffer_upload_flag_source_is_fif				= 1 << 1,
	zest_buffer_upload_flag_target_is_fif				= 1 << 2
} zest_buffer_upload_flags_e;

typedef zest_uint zest_buffer_upload_flags;

typedef enum {
	zest_draw_mode_none = 0,			//Default no drawmode set when no drawing has been done yet	
	zest_draw_mode_instance,
	zest_draw_mode_images,
	zest_draw_mode_ribbons,
	zest_draw_mode_lines,
	zest_draw_mode_line_instance,
	zest_draw_mode_line_instance3d,
	zest_draw_mode_linechain,
	zest_draw_mode_polys,
	zest_draw_mode_textured_polys,
	zest_draw_mode_ovals,
	zest_draw_mode_shapes,
	zest_draw_mode_text,
	zest_draw_mode_fill_screen,
	zest_draw_mode_viewport,
	zest_draw_mode_im_gui
} zest_draw_mode;

typedef enum {
	zest_builtin_layer_sprites = 0,
	zest_builtin_layer_billboards,
	zest_builtin_layer_fonts,
	zest_builtin_layer_mesh
} zest_builtin_layer_type;

typedef enum {
	zest_imgui_blendtype_none,				//Just draw with standard alpha blend
	zest_imgui_blendtype_pass,				//Force the alpha channel to 1
	zest_imgui_blendtype_premultiply		//Divide the color channels by the alpha channel
} zest_imgui_blendtype;

typedef enum {
	zest_texture_flag_none								= 0,
	zest_texture_flag_premultiply_mode					= 1 << 0,
	zest_texture_flag_use_filtering						= 1 << 1,
	zest_texture_flag_get_max_radius					= 1 << 2,
	zest_texture_flag_textures_ready					= 1 << 3,
	zest_texture_flag_dirty								= 1 << 4,
	zest_texture_flag_descriptor_sets_created			= 1 << 5,
} zest_texture_flags_e;

typedef zest_uint zest_texture_flags;

typedef enum zest_texture_storage_type {
	zest_texture_storage_type_packed,						//Pack all of the images into a sprite sheet and onto multiple layers in an image array on the GPU
	zest_texture_storage_type_bank,							//Packs all images one per layer, best used for repeating textures or color/bump/specular etc
	zest_texture_storage_type_sprite_sheet,					//Packs all the images onto a single layer spritesheet
	zest_texture_storage_type_single,						//A single image texture
	zest_texture_storage_type_storage,						//A storage texture useful for manipulation and other things in a compute shader
	zest_texture_storage_type_stream,						//A storage texture that you can update every frame
	zest_texture_storage_type_render_target					//Texture storage for a render target sampler, so that you can draw the target onto another render target
} zest_texture_storage_type;

typedef enum zest_camera_flags_e {
	zest_camera_flags_none = 0,
	zest_camera_flags_perspective						= 1 << 0,
	zest_camera_flags_orthagonal						= 1 << 1,
} zest_camera_flags_e;

typedef zest_uint zest_camera_flags;

typedef enum zest_render_target_flags_e {
	zest_render_target_flag_fixed_size					= 1 << 0,	//True if the render target is not tied to the size of the window
	zest_render_target_flag_is_src						= 1 << 1,	//True if the render target is the source of another render target - adds transfer src and dst bits
	zest_render_target_flag_use_msaa					= 1 << 2,	//True if the render target should render with MSAA enabled
	zest_render_target_flag_sampler_size_match_texture	= 1 << 3,
	zest_render_target_flag_single_frame_buffer_only	= 1 << 4,
	zest_render_target_flag_use_depth_buffer			= 1 << 5,
	zest_render_target_flag_render_to_swap_chain		= 1 << 6,
	zest_render_target_flag_initialised 				= 1 << 7,
	zest_render_target_flag_has_imgui_pipeline			= 1 << 8,
} zest_render_target_flags_e;

typedef zest_uint zest_render_target_flags;

typedef enum zest_texture_format {
	zest_texture_format_alpha = VK_FORMAT_R8_UNORM,
	zest_texture_format_rgba = VK_FORMAT_R8G8B8A8_UNORM,
	zest_texture_format_bgra = VK_FORMAT_B8G8R8A8_UNORM,
} zest_texture_format;

typedef enum zest_connector_type {
	zest_connector_type_wait_present,
	zest_connector_type_signal_present,
	zest_connector_type_wait_queue,
	zest_connector_type_signal_queue,
} zest_connector_type;

typedef enum zest_character_flags_e {
	zest_character_flag_none							= 0,
	zest_character_flag_skip							= 1 << 0,
	zest_character_flag_new_line						= 1 << 1,
} zest_character_flags_e;

typedef zest_uint zest_character_flags;

typedef enum zest_compute_flags_e {
	zest_compute_flag_none								= 0,
	zest_compute_flag_has_render_target					= 1,		// Compute shader uses a render target
	zest_compute_flag_rewrite_command_buffer			= 1 << 1,	// Command buffer for this compute shader should be rewritten
	zest_compute_flag_sync_required						= 1 << 2,	// Compute shader requires syncing with the render target
	zest_compute_flag_is_active							= 1 << 3,	// Compute shader is active then it will be updated when the swap chain is recreated
} zest_compute_flags_e;

typedef zest_uint zest_compute_flags;

typedef void(*zloc__block_output)(void* ptr, size_t size, int used, void* user, int is_final_output);

// --Forward declarations
typedef struct zest_texture_t zest_texture_t;
typedef struct zest_image_t zest_image_t;
typedef struct zest_draw_routine_t zest_draw_routine_t;
typedef struct zest_command_queue_draw_commands_t zest_command_queue_draw_commands_t;
typedef struct zest_command_queue_t zest_command_queue_t;
typedef struct zest_command_queue_compute_t zest_command_queue_compute_t;
typedef struct zest_font_t zest_font_t;
typedef struct zest_layer_t zest_layer_t;
typedef struct zest_pipeline_t zest_pipeline_t;
typedef struct zest_render_pass_t zest_render_pass_t;
typedef struct zest_descriptor_set_layout_t zest_descriptor_set_layout_t;
typedef struct zest_descriptor_set_t zest_descriptor_set_t;
typedef struct zest_descriptor_buffer_t zest_descriptor_buffer_t;
typedef struct zest_render_target_t zest_render_target_t;
typedef struct zest_buffer_allocator_t zest_buffer_allocator_t;
typedef struct zest_compute_t zest_compute_t;
typedef struct zest_buffer_t zest_buffer_t;
typedef struct zest_device_memory_pool_t zest_device_memory_pool_t;
typedef struct zest_timer_t zest_timer_t;

ZEST__MAKE_HANDLE(zest_texture)
ZEST__MAKE_HANDLE(zest_image)
ZEST__MAKE_HANDLE(zest_draw_routine)
ZEST__MAKE_HANDLE(zest_command_queue_draw_commands)
ZEST__MAKE_HANDLE(zest_command_queue)
ZEST__MAKE_HANDLE(zest_command_queue_compute)
ZEST__MAKE_HANDLE(zest_font)
ZEST__MAKE_HANDLE(zest_layer)
ZEST__MAKE_HANDLE(zest_pipeline)
ZEST__MAKE_HANDLE(zest_render_pass)
ZEST__MAKE_HANDLE(zest_descriptor_set_layout)
ZEST__MAKE_HANDLE(zest_descriptor_set)
ZEST__MAKE_HANDLE(zest_descriptor_buffer)
ZEST__MAKE_HANDLE(zest_render_target)
ZEST__MAKE_HANDLE(zest_buffer_allocator)
ZEST__MAKE_HANDLE(zest_compute)
ZEST__MAKE_HANDLE(zest_buffer)
ZEST__MAKE_HANDLE(zest_device_memory_pool)
ZEST__MAKE_HANDLE(zest_timer)

typedef zest_descriptor_buffer zest_uniform_buffer;

// --Private structs with inline functions
typedef struct zest_queue_family_indices {
	zest_uint graphics_family;
	zest_uint present_family;
	zest_uint compute_family;

	zest_bool graphics_set;
	zest_bool present_set;
	zest_bool compute_set;
} zest_queue_family_indices;

// --Pocket Dynamic Array
typedef struct zest_vec {
	zest_uint current_size;
	zest_uint capacity;
} zest_vec;

enum {
	zest__VEC_HEADER_OVERHEAD = sizeof(zest_vec)
};

// --Pocket dynamic array
#define zest__vec_header(T) ((zest_vec*)T - 1)
zest_uint zest__grow_capacity(void *T, zest_uint size);
#define zest_vec_bump(T) zest__vec_header(T)->current_size++;
#define zest_vec_clip(T) zest__vec_header(T)->current_size--;
#define zest_vec_grow(T) ((!(T) || (zest__vec_header(T)->current_size == zest__vec_header(T)->capacity)) ? T = zest__vec_reserve((T), sizeof(*T), (T ? zest__grow_capacity(T, zest__vec_header(T)->current_size) : 8)) : 0)
#define zest_vec_empty(T) (!T || zest__vec_header(T)->current_size == 0)
#define zest_vec_size(T) (T ? zest__vec_header(T)->current_size : 0)
#define zest_vec_last_index(T) (zest__vec_header(T)->current_size - 1)
#define zest_vec_capacity(T) (zest__vec_header(T)->capacity)
#define zest_vec_size_in_bytes(T) (zest__vec_header(T)->current_size * sizeof(*T))
#define zest_vec_front(T) (T[0])
#define zest_vec_back(T) (T[zest__vec_header(T)->current_size - 1])
#define zest_vec_end(T) (&(T[zest_vec_size(T)]))
#define zest_vec_clear(T) if(T) zest__vec_header(T)->current_size = 0
#define zest_vec_free(T) if(T) { ZEST__FREE(zest__vec_header(T)); T = ZEST_NULL;}
#define zest_vec_reserve(T, new_size) if(!T || zest__vec_header(T)->capacity < new_size) T = zest__vec_reserve(T, sizeof(*T), new_size); 
#define zest_vec_resize(T, new_size) if(!T || zest__vec_header(T)->capacity < new_size) T = zest__vec_reserve(T, sizeof(*T), new_size); zest__vec_header(T)->current_size = new_size
#define zest_vec_push(T, value) zest_vec_grow(T); (T)[zest__vec_header(T)->current_size++] = value 
#define zest_vec_pop(T) (zest__vec_header(T)->current_size--, T[zest__vec_header(T)->current_size])
#define zest_vec_insert(T, location, value) { ptrdiff_t offset = location - T; zest_vec_grow(T); if(offset < zest_vec_size(T)) memmove(T + offset + 1, T + offset, ((size_t)zest_vec_size(T) - offset) * sizeof(*T)); T[offset] = value; zest_vec_bump(T) }
#define zest_vec_erase(T, location) { ptrdiff_t offset = location - T; ZEST_ASSERT(T && offset >= 0 && location < zest_vec_end(T)); memmove(T + offset, T + offset + 1, ((size_t)zest_vec_size(T) - offset) * sizeof(*T)); zest_vec_clip(T); }
#define zest_foreach_i(T) int i = 0; i != zest_vec_size(T); ++i 
#define zest_foreach_j(T) int j = 0; j != zest_vec_size(T); ++j 
#define zest_foreach_k(T) int k = 0; k != zest_vec_size(T); ++k 
// --end of pocket dynamic array

// --Pocket Hasher, converted to c from Stephen Brumme's XXHash code
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

ZEST_PRIVATE zest_bool zest__is_aligned(void *ptr, size_t alignment) {
    uintptr_t address = (uintptr_t)ptr;
    return (address % alignment) == 0;
}

ZEST_PRIVATE inline zest_ull zest__hash_rotate_left(zest_ull x, unsigned char bits) {
    return (x << bits) | (x >> (64 - bits));
}
ZEST_PRIVATE inline zest_ull zest__hash_process_single(zest_ull previous, zest_ull input) {
    return zest__hash_rotate_left(previous + input * zest__PRIME2, 31) * zest__PRIME1;
}
ZEST_PRIVATE inline void zest__hasher_process(const void* data, zest_ull *state0, zest_ull *state1, zest_ull *state2, zest_ull *state3) {
    zest_ull *block = (zest_ull*)data;
    zest_ull blocks[4];
    memcpy(blocks, data, sizeof(zest_ull) * 4);
    *state0 = zest__hash_process_single(*state0, blocks[0]);
    *state1 = zest__hash_process_single(*state1, blocks[1]);
    *state2 = zest__hash_process_single(*state2, blocks[2]);
    *state3 = zest__hash_process_single(*state3, blocks[3]);
}
ZEST_PRIVATE inline zest_bool zest__hasher_add(zest_hasher *hasher, const void* input, zest_ull length)
{
	if (!input || length == 0) return ZEST_FALSE;

	hasher->total_length += length;
	unsigned char* data = (unsigned char*)input;

	if (hasher->buffer_size + length < zest__HASH_MAX_BUFFER_SIZE)
	{
		while (length-- > 0)
			hasher->buffer[hasher->buffer_size++] = *data++;
		return ZEST_TRUE;
	}

	const unsigned char* stop = data + length;
	const unsigned char* stopBlock = stop - zest__HASH_MAX_BUFFER_SIZE;

	if (hasher->buffer_size > 0)
	{
		while (hasher->buffer_size < zest__HASH_MAX_BUFFER_SIZE)
			hasher->buffer[hasher->buffer_size++] = *data++;

		zest__hasher_process(hasher->buffer, &hasher->state[0], &hasher->state[1], &hasher->state[2], &hasher->state[3]);
	}

	zest_ull s0 = hasher->state[0], s1 = hasher->state[1], s2 = hasher->state[2], s3 = hasher->state[3];
    zest_bool test = zest__is_aligned(&s0, 8);
	while (data <= stopBlock)
	{
		zest__hasher_process(data, &s0, &s1, &s2, &s3);
		data += 32;
	}
	hasher->state[0] = s0; hasher->state[1] = s1; hasher->state[2] = s2; hasher->state[3] = s3;

	hasher->buffer_size = stop - data;
	for (zest_ull i = 0; i < hasher->buffer_size; i++)
		hasher->buffer[i] = data[i];

	return ZEST_TRUE;
}

ZEST_PRIVATE inline zest_ull zest__get_hash(zest_hasher *hasher)
{
	zest_ull result;
	if (hasher->total_length >= zest__HASH_MAX_BUFFER_SIZE)
	{
		result = zest__hash_rotate_left(hasher->state[0], 1) +
			zest__hash_rotate_left(hasher->state[1], 7) +
			zest__hash_rotate_left(hasher->state[2], 12) +
			zest__hash_rotate_left(hasher->state[3], 18);
		result = (result ^ zest__hash_process_single(0, hasher->state[0])) * zest__PRIME1 + zest__PRIME4;
		result = (result ^ zest__hash_process_single(0, hasher->state[1])) * zest__PRIME1 + zest__PRIME4;
		result = (result ^ zest__hash_process_single(0, hasher->state[2])) * zest__PRIME1 + zest__PRIME4;
		result = (result ^ zest__hash_process_single(0, hasher->state[3])) * zest__PRIME1 + zest__PRIME4;
	}
	else
	{
		result = hasher->state[2] + zest__PRIME5;
	}

	result += hasher->total_length;
	const unsigned char* data = hasher->buffer;
	const unsigned char* stop = data + hasher->buffer_size;
	for (; data + 8 <= stop; data += 8)
		result = zest__hash_rotate_left(result ^ zest__hash_process_single(0, *(zest_ull*)data), 27) * zest__PRIME1 + zest__PRIME4;
	if (data + 4 <= stop)
	{
		result = zest__hash_rotate_left(result ^ (*(zest_uint*)data) * zest__PRIME1, 23) * zest__PRIME2 + zest__PRIME3;
		data += 4;
	}
	while (data != stop)
		result = zest__hash_rotate_left(result ^ (*data++) * zest__PRIME5, 11) * zest__PRIME1;
	result ^= result >> 33;
	result *= zest__PRIME2;
	result ^= result >> 29;
	result *= zest__PRIME3;
	result ^= result >> 32;
	return result;
}
void zest__hash_initialise(zest_hasher *hasher, zest_ull seed);

//The only command you need for the hasher. Just used internally by the hash map.
zest_key zest_Hash(zest_hasher *hasher, const void* input, zest_ull length, zest_ull seed);
extern zest_hasher *ZestHasher;
//-- End of Pocket Hasher

// --Begin pocket hash map
typedef struct {
	zest_key key;
	zest_index index;
} zest_hash_pair;

#ifndef ZEST_HASH_SEED
#define ZEST_HASH_SEED 0xABCDEF99
#endif
ZEST_PRIVATE zest_hash_pair* zest__lower_bound(zest_hash_pair *map, zest_key key) { zest_hash_pair *first = map; zest_hash_pair *last = map ? zest_vec_end(map) : 0; size_t count = (size_t)(last - first); while (count > 0) { size_t count2 = count >> 1; zest_hash_pair* mid = first + count2; if (mid->key < key) { first = ++mid; count -= count2 + 1; } else { count = count2; } } return first; }
ZEST_PRIVATE void zest__map_realign_indexes(zest_hash_pair *map, zest_index index) { for (zest_foreach_i(map)) { if (map[i].index < index) continue; map[i].index--; } }
ZEST_PRIVATE zest_index zest__map_get_index(zest_hash_pair *map, zest_key key) { zest_hash_pair *it = zest__lower_bound(map, key); return (it == zest_vec_end(map) || it->key != key) ? -1 : it->index; }
#define zest_map_hash(hash_map, name) zest_Hash(ZestHasher, name, strlen(name), ZEST_HASH_SEED) 
#define zest_map_hash_ptr(hash_map, ptr, size) zest_Hash(ZestHasher, ptr, size, ZEST_HASH_SEED) 
//Free slots isn't really needed now, I should probably remove it.
#define zest_hash_map(T) typedef struct { zest_hash_pair *map; T *data; zest_index *free_slots; zest_index last_index; }
#define zest_map_valid_name(hash_map, name) (hash_map.map && zest__map_get_index(hash_map.map, zest_map_hash(hash_map, name)) != -1)
#define zest_map_valid_key(hash_map, key) (hash_map.map && zest__map_get_index(hash_map.map, key) != -1)
#define zest_map_valid_index(hash_map, index) (hash_map.map && (zest_uint)index < zest_vec_size(hash_map.data))
#define zest_map_valid_hash(hash_map, ptr, size) (zest__map_get_index(hash_map.map, zest_map_hash_ptr(hash_map, ptr, size)) != -1)
#define zest_map_set_index(hash_map, hash_key, value) zest_hash_pair *it = zest__lower_bound(hash_map.map, hash_key); if(!hash_map.map || it == zest_vec_end(hash_map.map) || it->key != hash_key) { if(zest_vec_size(hash_map.free_slots)) { hash_map.last_index = zest_vec_pop(hash_map.free_slots); hash_map.data[hash_map.last_index] = value; } else {hash_map.last_index = zest_vec_size(hash_map.data); zest_vec_push(hash_map.data, value);} zest_hash_pair new_pair; new_pair.key = hash_key; new_pair.index = hash_map.last_index; zest_vec_insert(hash_map.map, it, new_pair); } else {hash_map.data[it->index] = value;}
#define zest_map_insert(hash_map, name, value) { zest_key key = zest_map_hash(hash_map, name); zest_map_set_index(hash_map, key, value); }
#define zest_map_insert_key(hash_map, hash_key, value) { zest_map_set_index(hash_map, hash_key, value) }
#define zest_map_insert_with_ptr_hash(hash_map, ptr, size, value) { zest_key key = zest_map_hash_ptr(hash_map, ptr, size); zest_map_set_index(hash_map, key, value) }
#define zest_map_at(hash_map, name) &hash_map.data[zest__map_get_index(hash_map.map, zest_map_hash(hash_map, name))]
#define zest_map_at_key(hash_map, key) &hash_map.data[zest__map_get_index(hash_map.map, key)]
#define zest_map_at_index(hash_map, index) &hash_map.data[index]
#define zest_map_get_index_by_key(hash_map, key) zest__map_get_index(hash_map.map, key);
#define zest_map_get_index_by_name(hash_map, name) zest__map_get_index(hash_map.map, zest_map_hash(hash_map, name));
#define zest_map_remove(hash_map, name) {zest_key key = zest_map_hash(hash_map, name); zest_hash_pair *it = zest__lower_bound(hash_map.map, key); zest_index index = it->index; zest_vec_erase(hash_map.map, it); zest_vec_push(hash_map.free_slots, index); }
#define zest_map_last_index(hash_map) (hash_map.last_index)
#define zest_map_size(hash_map) (hash_map.map ? zest__vec_header(hash_map.data)->current_size : 0)
#define zest_map_clear(hash_map) zest_vec_clear(hash_map.map); zest_vec_clear(hash_map.data); zest_vec_clear(hash_map.free_slots)
#define zest_map_free(hash_map) if(hash_map.map) ZEST__FREE(zest__vec_header(hash_map.map)); if(hash_map.data) ZEST__FREE(zest__vec_header(hash_map.data)); if(hash_map.free_slots) ZEST__FREE(zest__vec_header(hash_map.free_slots)); hash_map.data = 0; hash_map.map = 0; hash_map.free_slots = 0
//Use inside a for loop to iterate over the map. The loop index should be be the index into the map array, to get the index into the data array.
#define zest_map_index(hash_map, i) (hash_map.map[i].index)
#define zest_map_foreach_i(hash_map) zest_foreach_i(hash_map.map)
#define zest_map_foreach_j(hash_map) zest_foreach_j(hash_map.map)
// --End pocket hash map

// --Begin pocket text buffer
typedef struct zest_text {
	char *str;
} zest_text;

ZEST_API void zest_SetText(zest_text *buffer, const char *text);
ZEST_API void zest_FreeText(zest_text *buffer);
ZEST_API zest_uint zest_TextLength(zest_text *buffer);
// --End pocket text buffer

// --Matrix and vector structs
typedef struct zest_vec2 {
	float x, y;
} zest_vec2;

typedef struct zest_vec3 {
	float x, y, z;
} zest_vec3;

//Note that using alignas on windows causes a crash in release mode relating to the allocator.
//Not sure why though. We need the align as on Mac otherwise metal complains about the alignment
//in the shaders
typedef struct zest_vec4 {
	float x, y, z, w;
} zest_vec4 ZEST_ALIGN_AFFIX(16);

typedef struct zest_matrix4 {
	zest_vec4 v[4];
} zest_matrix4 ZEST_ALIGN_AFFIX(16);

typedef struct zest_rgba8 {
	union {
		struct { unsigned char r, g, b, a; };
		struct { zest_uint color; };
	};
} zest_rgba8;
typedef zest_rgba8 zest_color;

typedef struct zest_timer_t {
	double start_time;
	double delta_time;
	double update_frequency;
	double update_tick_length;
	double update_time;
	double ticker;
	double accumulator;
	double accumulator_delta;
	double current_time;
	double lerp;
} zest_timer_t;

typedef struct zest_camera_t {
	zest_vec3 position;
	zest_vec3 up;
	zest_vec3 right;
	zest_vec3 front;
	float yaw;
	float pitch;
	float fov;
	float ortho_scale;
	zest_camera_flags flags;
} zest_camera_t;

// --Vulkan Buffer Management
typedef struct zest_buffer_info_t {
	VkImageUsageFlags image_usage_flags;
	VkBufferUsageFlags usage_flags;					//The usage state_flags of the memory block. 
	VkMemoryPropertyFlags property_flags;
	zest_buffer_flags flags;
	VkDeviceSize alignment;
	zest_uint memory_type_bits;
} zest_buffer_info_t;

typedef struct zest_buffer_pool_size_t {
	const char *name;
	zest_size pool_size;
	zest_size minimum_allocation_size;
} zest_buffer_pool_size_t;

typedef struct zest_buffer_usage_t { 
	VkBufferUsageFlags usage_flags; 
	VkMemoryPropertyFlags property_flags; 
	VkImageUsageFlags image_flags; 
} zest_buffer_usage_t;

zest_hash_map(zest_buffer_pool_size_t) zest_map_buffer_pool_sizes;

//A simple buffer struct for creating and mapping GPU memory
typedef struct zest_device_memory_pool_t {
	VkBuffer buffer;
	VkDeviceMemory memory;
	VkDeviceSize size;
	VkDeviceSize minimum_allocation_size;
	VkDeviceSize alignment;
	VkImageUsageFlags usage_flags;
	VkMemoryPropertyFlags property_flags;
	VkDescriptorBufferInfo descriptor;
	zest_uint memory_type_index;
	void* mapped;
	const char *name;
} zest_device_memory_pool_t;

typedef void* zest_pool_range;

typedef struct zest_buffer_allocator_t {
	zest_buffer_info_t buffer_info;
	zloc_allocator *allocator;
	zest_size alignment;
	zest_device_memory_pool *memory_pools;
	zest_pool_range *range_pools;
} zest_buffer_allocator_t;

typedef struct zest_buffer_t {
	VkDeviceSize size;
	VkDeviceSize memory_offset;
	zest_device_memory_pool memory_pool;
	zest_buffer_allocator buffer_allocator;
	zest_size memory_in_use;
	void *data;
	void *end;
} zest_buffer_t;

//Simple stuct for uploading buffers from the staging buffer to the device local buffers
typedef struct zest_buffer_uploader_t {
	zest_buffer_upload_flags flags;
	zest_buffer_t *source_buffer;			//The source memory allocation (cpu visible staging buffer)
	zest_buffer_t *target_buffer;			//The target memory allocation that we're uploading to (device local buffer)
	VkBufferCopy *buffer_copies;		//List of vulkan copy info commands to upload staging buffers to the gpu each frame
} zest_buffer_uploader_t;
// --End Vulkan Buffer Management

typedef struct zest_swapchain_support_details_t {
	VkSurfaceCapabilitiesKHR capabilities;
	VkSurfaceFormatKHR *formats;
	zest_uint formats_count;
	VkPresentModeKHR *present_modes;
	zest_uint present_modes_count;
} zest_swapchain_support_details_t;

typedef struct zest_window_t {
	void *window_handle;
	VkSurfaceKHR surface;
	zest_uint window_width;
	zest_uint window_height;
	zest_bool framebuffer_resized;
} zest_window_t;

typedef struct zest_device_t {
	zest_uint api_version;
	zest_uint use_labels_address_bit;
	zest_uint current_fif;
	zest_uint max_fif;
	zest_uint max_image_size;
	zest_uint graphics_queue_family_index;
	zest_uint present_queue_family_index;
	zest_uint compute_queue_family_index;
	void *memory_pools[ZEST_MAX_DEVICE_MEMORY_POOLS];
	zest_uint memory_pool_count;
	zloc_allocator *allocator;
	VkAllocationCallbacks allocation_callbacks;
	char **extensions;
	VkInstance instance;
	VkPhysicalDevice physical_device;
	VkDevice logical_device;
	VkDebugUtilsMessengerEXT debug_messenger;
	VkSampleCountFlagBits msaa_samples;
	zest_swapchain_support_details_t swapchain_support_details;
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
	zest_map_buffer_pool_sizes pool_sizes;
	void *allocator_start;
	void *allocator_end;
} zest_device_t;

typedef struct zest_create_info_t {
	zest_size memory_pool_size;							//The size of each memory pool. More pools are added if needed
	int screen_width, screen_height;					//Default width and height of the window that you open
	int screen_x, screen_y;								//Default position of the window
	int virtual_width, virtual_height;					//The virtial width/height of the viewport
	VkFormat color_format;								//Choose between VK_FORMAT_R8G8B8A8_UNORM and VK_FORMAT_R8G8B8A8_SRGB
	VkDescriptorPoolSize *pool_counts;					//You can define descriptor pool counts here.
	zest_uint flags;									//Set flags to apply different initialisation options

	//Callbacks use these to implement your own preferred window creation functionality
	void(*get_window_size_callback)(void *user_data, int *fb_width, int *fb_height, int *window_width, int *window_height);
	void(*destroy_window_callback)(void *user_data);
	void(*poll_events_callback)(ZEST_PROTOTYPE);
	void(*add_platform_extensions_callback)(ZEST_PROTOTYPE);
	zest_window_t*(*create_window_callback)(int x, int y, int width, int height, zest_bool maximised, const char* title);
	void(*create_window_surface_callback)(zest_window_t* window);
} zest_create_info_t;

typedef struct zest_app_t {
	zest_create_info_t create_info;

	void(*update_callback)(zest_microsecs, void*);
	void *user_data;

	zest_window_t *window;

	zest_microsecs current_elapsed;
	zest_microsecs current_elapsed_time;
	float update_time;
	float render_time;
	zest_microsecs frame_timer;

	float mouse_x;
	float mouse_y;
	int mouse_button;

	zest_uint frame_count;
	zest_uint last_fps;

	zest_layer default_layer;
	zest_command_queue default_command_queue;
	zest_command_queue_draw_commands default_draw_commands;
	zest_uint default_font_index;

	zest_app_flags flags;
} zest_app_t;

typedef struct zest_semaphores_t {
	VkSemaphore incoming;
	VkSemaphore outgoing;
} zest_semaphores_t;

typedef struct zest_frame_buffer_attachment_t {
	VkImage image;
	zest_buffer_t *buffer;
	VkImageView view;
	VkFormat format;
} zest_frame_buffer_attachment_t;

typedef struct zest_frame_buffer_t {
	zest_uint width, height;
	VkFormat format;
	VkFramebuffer device_frame_buffer;
	zest_frame_buffer_attachment_t color_buffer, depth_buffer, resolve_buffer;
	VkSampler sampler;
} zest_frame_buffer_t;

//Struct to store all the necessary data for a render pass
typedef struct zest_render_pass_data_t {
	zest_render_pass render_pass;
} zest_render_pass_data_t;

typedef struct zest_final_render_push_constants_t {
	zest_vec2 screen_resolution;			//the current size of the window/resolution
} zest_final_render_push_constants_t;

typedef struct zest_semaphore_connector_t {
	VkSemaphore *fif_incoming_semaphores;				//Must be waited on, eg a command queue waiting on present signal
	VkSemaphore *fif_outgoing_semaphores;				//To signal the next process, eg a command queue signalling the present process
	zest_connector_type type;
} zest_semaphore_connector_t;

//command queues are the main thing you use to draw things to the screen. A simple app will create one for you, or you can create your own. See examples like PostEffects and also 
typedef struct zest_command_queue_t {
	zest_semaphore_connector_t semaphores[ZEST_MAX_FIF];
	const char *name;
	VkCommandPool command_pool;										//The command pool for command buffers
	VkCommandBuffer command_buffer[ZEST_MAX_FIF];					//A vulkan command buffer for each frame in flight
	VkPipelineStageFlags *fif_wait_stage_flags[ZEST_MAX_FIF];		//Stage state_flags relavent to the incoming semaphores
	zest_command_queue_draw_commands *draw_commands;				//A list of draw command handles - mostly these will be draw_commands that are recorded to the command buffer
	zest_command_queue_compute *compute_commands;					//Compute items to be recorded to the command buffer
	zest_index present_semaphore_index[ZEST_MAX_FIF];				//An index to the semaphore representing the swap chain if required. (command queues don't necessarily have to wait for the swap chain)
	zest_command_queue_flags flags;									//Can be either dependent on the swap chain to present or another command queue
} zest_command_queue_t;

typedef struct zest_render_pass_t {
	VkRenderPass render_pass;
	const char *name;
} zest_render_pass_t;

typedef struct zest_descriptor_set_layout_t {
	VkDescriptorSetLayout descriptor_layout;
	const char *name;
} zest_descriptor_set_layout_t;

typedef struct zest_descriptor_set_t {
	zest_descriptor_buffer buffer;
	VkWriteDescriptorSet *descriptor_writes[ZEST_MAX_FIF];
	VkDescriptorSet descriptor_set[ZEST_MAX_FIF];
} zest_descriptor_set_t;

typedef struct zest_descriptor_set_builder_t {
	VkWriteDescriptorSet *writes;
} zest_descriptor_set_builder_t;

typedef struct zest_descriptor_buffer_t {
	zest_buffer buffer[ZEST_MAX_FIF];
	VkDescriptorBufferInfo descriptor_info[ZEST_MAX_FIF];
	zest_bool all_frames_in_flight;
} zest_descriptor_buffer_t;

typedef struct zest_descriptor_infos_for_binding_t {
	VkDescriptorBufferInfo descriptor_buffer_info[ZEST_MAX_FIF];
	VkDescriptorImageInfo descriptor_image_info[ZEST_MAX_FIF];
	zest_bool all_frames_in_flight;
} zest_descriptor_infos_for_binding_t;

typedef struct zest_uniform_buffer_data_t {
	zest_matrix4 view;
	zest_matrix4 proj;
	zest_vec4 parameters1;
	zest_vec4 parameters2;
	zest_vec2 screen_size;
	zest_uint millisecs;
} zest_uniform_buffer_data_t;

//Pipeline create template to setup the creation of a pipeline. Create this first and then use it with MakePipelineTemplate or SetPipelineTemplate to create a PipelineTemplate which you can then customise
//further if needed before calling CreatePipeline
typedef struct zest_pipeline_template_create_info_t {
	VkRect2D viewport;
	zest_descriptor_set_layout descriptorSetLayout;
	VkPushConstantRange *pushConstantRange;
	VkRenderPass renderPass;
	VkVertexInputAttributeDescription *attributeDescriptions;
	VkVertexInputBindingDescription *bindingDescriptions;
	VkVertexInputBindingDescription bindingDescription;
	VkDynamicState *dynamicStates;
	zest_bool no_vertex_input;
	const char *vertShaderFile;
	const char *fragShaderFile;
	const char *vertShaderFunctionName;
	const char *fragShaderFunctionName;
	VkPrimitiveTopology topology;
} zest_pipeline_template_create_info_t;

//Pipeline template is used with CreatePipeline to create a vulkan pipeline. Use PipelineTemplate() or SetPipelineTemplate with PipelineTemplateCreateInfo to create a PipelineTemplate
typedef struct zest_pipeline_template_t {
	VkPipelineShaderStageCreateInfo vertShaderStageInfo;
	VkPipelineShaderStageCreateInfo fragShaderStageInfo;
	VkShaderModule vertShaderCode;
	VkShaderModule fragShaderCode;
	VkPipelineVertexInputStateCreateInfo vertexInputInfo;
	VkPipelineInputAssemblyStateCreateInfo inputAssembly;
	VkViewport viewport;
	VkRect2D scissor;
	VkPipelineViewportStateCreateInfo viewportState;
	VkPipelineRasterizationStateCreateInfo rasterizer;
	VkPipelineMultisampleStateCreateInfo multisampling;
	VkPipelineDepthStencilStateCreateInfo depthStencil;
	VkPipelineColorBlendAttachmentState colorBlendAttachment;
	VkPipelineColorBlendStateCreateInfo colorBlending;
	VkPipelineLayoutCreateInfo pipelineLayoutInfo;
	VkPushConstantRange pushConstantRange;
	VkPipelineDynamicStateCreateInfo dynamicState;
	VkRenderPass renderPass;

	const char *vertShaderFile;
	const char *fragShaderFile;
} zest_pipeline_template_t;

//A pipeline set is all of the necessary things required to setup and maintain a pipeline
typedef struct zest_pipeline_t {
	zest_pipeline_template_create_info_t create_info;							//A copy of the create info and template is stored so that they can be used to update the pipeline later for any reason (like the swap chain is recreated)
	zest_pipeline_template_t pipeline_template;
	zest_descriptor_set_layout descriptor_layout;								//The descriptor layout being used which is stored in the Renderer. Layouts can be reused an shared between pipelines
	VkDescriptorSet descriptor_set[ZEST_MAX_FIF];								//Descriptor sets are only stored here for certain pipelines like non textured drawing or the final render pipelines for render targets in the swap chain
	VkPipeline pipeline;														//The vulkan handle for the pipeline
	VkPipelineLayout pipeline_layout;											//The vulkan handle for the pipeline layout
	zest_uint uniforms;															//Number of uniform buffers in the pipeline, usually 1 or 0
	zest_uint push_constant_size;												//Size of the push constant struct if it uses one
	zest_uint *textures;														//A reference to the textures used by the pipeline - only used by final render, not even sure if it's needed.
	VkWriteDescriptorSet *descriptor_writes[ZEST_MAX_FIF];						//Descriptor writes for creating the descriptor sets - is this needed here? only for certain pipelines, textures store their own
	const char *name;															//Name for the pipeline just for labelling it when listing all the renderer objects in debug
	void(*rebuild_pipeline_function)(void*);									//Override the function to rebuild the pipeline when the swap chain is recreated
	zest_pipeline_set_flags flags;												//Flag bits	
} zest_pipeline_t;

typedef struct zest_command_setup_context_t {
	zest_command_queue command_queue;
	zest_command_queue_draw_commands draw_commands;
	zest_draw_routine draw_routine;
	zest_layer layer;
	zest_index compute_index;
	zest_setup_context_type type;
} zest_command_setup_context_t ;

ZEST_PRIVATE inline void zest__reset_command_setup_context(zest_command_setup_context_t *context) {
	context->command_queue = ZEST_NULL;
	context->draw_commands = ZEST_NULL;
	context->draw_routine = ZEST_NULL;
	context->layer = ZEST_NULL;
	context->type = zest_setup_context_type_none;
}

//A draw routine is used to actually draw things to the render target. Qulkan provides Layers that have a set of draw commands for doing this or you can develop your own
//By settting the callbacks and data pointers in the draw routine
struct zest_draw_routine_t {
	const char *name;
	zest_command_queue command_queue;											//The index of the render queue that this draw routine is within
	zest_command_queue_draw_commands draw_commands;								//The draw commands within the command queue that this draw routine belongs to
	void *draw_data;															//The user index of the draw routine. Use this to index the routine in your own lists. For Qulkan layers, this is used to hold the index of the layer in the renderer
	zest_index *command_queues;													//A list of the render queues that this draw routine belongs to
	void *user_data;															//Pointer to some user data
	void(*update_buffers_callback)(zest_draw_routine draw_routine, VkCommandBuffer command_buffer);			//The callback used to update and upload the buffers before rendering
	void(*draw_callback)(zest_draw_routine draw_routine, VkCommandBuffer command_buffer);						//draw callback called by the render target when rendering
	void(*update_resolution_callback)(zest_draw_routine draw_routine);			//Callback used when the window size changes
	void(*clean_up_callback)(zest_draw_routine draw_routine);					//Clean up function call back called when the draw routine is deleted
};

//Every command queue will have either one or more render passes (unless it's purely for compute shading). Render pass contains various data for drawing things during the render pass.
//These can be draw routines that you can use to draw various things to the screen. You can set the render_pass_function to whatever you need to record the command buffer. See existing render pass functions such as:
//RenderDrawRoutine, RenderRenderTarget and RenderTargetToSwapChain.
typedef struct zest_command_queue_draw_commands_t {
	VkFramebuffer(*get_frame_buffer)(zest_command_queue_draw_commands_t *item);
	void(*render_pass_function)(zest_command_queue_draw_commands item, VkCommandBuffer command_buffer, zest_render_pass render_pass, VkFramebuffer framebuffer);
	zest_draw_routine *draw_routines;
	zest_render_target *render_targets;
	VkRect2D viewport;
	zest_render_viewport_type viewport_type;
	zest_vec2 viewport_scale;
	zest_render_pass render_pass;
	zest_vec4 cls_color;
	zest_render_target render_target;
	const char *name;
} zest_command_queue_draw_commands_t;

typedef struct zest_sprite_instance_t {			//64 bytes
	zest_vec2 size;						//Size of the sprite in pixels
	zest_vec2 handle;					//The handle of the sprite
	zest_vec4 uv;						//The UV coords of the image in the texture
	zest_vec4 position_rotation;		//The position of the sprite with rotation in w and stretch in z
	float intensity;					//Multiply blend factor, y is unused
	zest_uint alignment;				//normalised alignment vector 2 floats packed into 16bits
	zest_color color;					//The color tint of the sprite
	zest_uint image_layer_index;		//reference for the texture array if used
} zest_sprite_instance_t;

typedef struct zest_billboard_instance_t {		//64 bytes
	zest_vec3 position;					//The position of the sprite
	zest_uint uv_xy;					//The UV coords of the image in the texture, x and y packed in a zest_uint as SNORM 16bit floats
	zest_vec3 rotations;				//Pitch yaw and roll
	zest_uint uv_zw;					//The UV coords of the image in the texture, z and w packed in a zest_uint as SNORM 16bit floats
	zest_vec2 scale;					//The scale of the billboard
	zest_vec2 handle;					//The handle of the billboard
	float stretch;						//Amount to stretch the billboard along it's alignment vector
	zest_uint blend_texture_array;		//reference for the texture array (8bits) and blend factor (24bits)
	zest_color color;					//The color tint of the sprite
	zest_uint alignment;				//normalised alignment vector 3 floats packed into 10bits each with 2 bits left over
} zest_billboard_instance_t;

//We just have a copy of the ImGui Draw vert here so that we can setup things things for imgui
//should anyone choose to use it
typedef struct zest_ImDrawVert_t
{
	zest_vec2 pos;
	zest_vec2 uv;
	zest_uint col;
} zest_ImDrawVert_t;

typedef struct zest_layer_buffers_t {
	union {
		struct {
			zest_buffer_t *staging_vertex_data;
			zest_buffer_t *staging_index_data;
		};
		struct {
			zest_buffer_t *staging_instance_data;
			zest_buffer_t *device_instance_data;
		};
	};

	union {
		struct { void *instance_ptr; };
		struct { void *vertex_ptr; };
	};

	zest_uint *index_ptr;

	zest_buffer_t *device_index_data;
	zest_buffer_t *device_vertex_data;

	zest_uint instance_count;
	zest_uint index_count;
	zest_uint index_position;
	zest_uint last_index;
	zest_uint vertex_count;
} zest_layer_buffers_t;

typedef struct zest_push_constants_t {
	zest_matrix4 model;				//Can be used for anything
	zest_vec4 parameters1;			//Can be used for anything
	zest_vec4 parameters2;			//Can be used for anything
    zest_vec4 parameters3;			//Can be used for anything		
	zest_vec4 camera;				//For 3d drawing
	zest_uint flags;				//bit flag field
} zest_push_constants_t ZEST_ALIGN_AFFIX(16);

typedef struct zest_instance_instruction_t {
	zest_index start_index;						//The starting index
	zest_uint total_instances;					//The total number of instances to be drawn in the draw instruction
	zest_index last_instance;					//The last instance that was drawn in the previous instance instruction
	zest_pipeline pipeline;						//The pipeline index to draw the instances. 
	VkDescriptorSet descriptor_set; 			//The descriptor set used to draw the quads.
	zest_push_constants_t push_constants;		//Each draw instruction can have different values in the push constants push_constants
	VkRect2D scissor;							//The drawinstruction can also clip whats drawn
	VkViewport viewport;						//The viewport size of the draw call 
	zest_draw_mode draw_mode;
	void *asset;								//Optional pointer to either texture, font etc
} zest_instance_instruction_t;

typedef struct zest_layer_t {

	const char *name;

	zest_layer_buffers_t memory_refs[ZEST_MAX_FIF];
	zest_uint initial_instance_pool_size;
	zest_command_queue_draw_commands draw_commands;
	zest_buffer_uploader_t vertex_upload;
	zest_buffer_uploader_t index_upload;

	zest_index sprite_pipeline_index;

	zest_instance_instruction_t current_instance_instruction;

	union {
		struct { zest_size instance_struct_size; };
		struct { zest_size vertex_struct_size; };
	};

	zest_color current_color;
	float intensity;
	zest_push_constants_t push_constants;

	zest_vec2 layer_size;
	zest_vec2 screen_scale;

	VkRect2D scissor;
	VkViewport viewport;

	zest_instance_instruction_t *instance_instructions[ZEST_MAX_FIF];
	zest_draw_mode last_draw_mode;

	zest_draw_routine draw_routine;
} zest_layer_t ZEST_ALIGN_AFFIX(16);

typedef struct zest_font_character_t {
	char character[4];
	float width;
	float height;
	float xoffset;
	float yoffset;
	float xadvance;
    zest_uint flags;
    float reserved1;
    zest_vec4 uv;
    float reserved2[4];
} zest_font_character_t;

typedef struct zest_font_t {
	zest_text name;
	zest_texture texture;
	zest_pipeline pipeline;
	zest_descriptor_set descriptor_set;
	float pixel_range;
	float miter_limit;
	float padding;
	float font_size;
	float max_yoffset;
	zest_uint character_count;
	zest_uint character_offset;
	zest_font_character_t *characters;
} zest_font_t;	

typedef struct zest_compute_push_constant {
	//z is reserved for the quad_count;
	zest_vec4 parameters;
} zest_compute_push_constant;

typedef struct zest_compute_t zest_compute_t;
struct zest_compute_t {
	VkQueue queue;											// Separate queue for compute commands (queue family may differ from the one used for graphics)
	VkCommandPool command_pool;								// Use a separate command pool (queue family may differ from the one used for graphics)
	VkCommandBuffer command_buffer[ZEST_MAX_FIF];			// Command buffer storing the dispatch commands and barriers
	VkFence fence[ZEST_MAX_FIF];							// Synchronization fence to avoid rewriting compute CB if still in use
	VkSemaphore fif_outgoing_semaphore[ZEST_MAX_FIF];		// Signal semaphores
	VkSemaphore fif_incoming_semaphore[ZEST_MAX_FIF];		// Wait semaphores. The compute shader will not be executed on the GPU until these are signalled
	zest_bool frame_has_run[ZEST_MAX_FIF];					// True if the compute shader has run yet
	VkDescriptorPool descriptor_pool;						// The descriptor pool for sets created by the compute - may move this into the renderer
	VkDescriptorSetLayout descriptor_set_layout;			// Compute shader binding layout
	VkDescriptorSet descriptor_set[ZEST_MAX_FIF];			// Compute shader bindings
	VkPipelineLayout pipeline_layout;						// Layout of the compute pipeline
	VkDescriptorSetLayoutBinding *set_layout_bindings;
	zest_text *shader_names;								// Names of the shader files to use 
	VkPipeline *pipelines;									// Compute pipelines, one for each shader
	zest_descriptor_infos_for_binding_t *descriptor_infos;	// All the buffers/images that are bound to the compute shader
	int32_t pipeline_index;									// Current image filtering compute pipeline index
	uint32_t queue_family_index;							// Family index of the graphics queue, used for barriers
	zest_uint group_count_x;							
	zest_uint group_count_y;
	zest_uint group_count_z;
	zest_compute_push_constant push_constants;
	VkPushConstantRange pushConstantRange;

	void *compute_data;										// Connect this to any custom data that is required to get what you need out of the compute process.
	zest_render_target render_target;						// The render target that this compute shader works with
	void *user_data;										// Custom user data
	zest_compute_flags flags;

	// Over ride the descriptor udpate function with a customised one
	void(*descriptor_update_callback)(zest_compute compute);
	// Over ride the command buffer update function with a customised one
	void(*command_buffer_update_callback)(zest_compute compute, VkCommandBuffer command_buffer);		
	// Additional cleanup function callback for the extra compute_data you're using
	void(*extra_cleanup_callback)(zest_compute compute);				
};

zest_hash_map(VkDescriptorPoolSize) zest_map_descriptor_pool_sizes;

typedef struct zest_compute_builder_t {
    zest_map_descriptor_pool_sizes descriptor_pool_sizes;
	VkDescriptorSetLayoutBinding *layout_bindings;
	zest_descriptor_infos_for_binding_t *descriptor_infos;
	zest_text *shader_names;
	VkPipelineLayoutCreateInfo layout_create_info;
	zest_uint push_constant_size;
	void *user_data;

	void(*descriptor_update_callback)(zest_compute compute);
	void(*command_buffer_update_callback)(zest_compute compute, VkCommandBuffer command_buffer);
	void(*extra_cleanup_callback)(zest_compute compute);
} zest_compute_builder_t;

//In addition to render passes, you can also run compute shaders by adding this struct to the list of compute items in the command queue
typedef struct zest_command_queue_compute_t zest_command_queue_compute_t;
struct zest_command_queue_compute_t {
	void(*compute_function)(zest_command_queue_compute item);				//Call back function which will have the vkcommands the dispatch the compute shader task
	zest_compute *compute_shaders;											//List of zest_compute indexes so multiple compute shaders can be run in the same compute item
	const char *name;
};

zest_hash_map(zest_descriptor_set) zest_map_texture_descriptor_sets;
zest_hash_map(zest_descriptor_set_builder_t) zest_map_texture_descriptor_builders;

typedef struct zest_bitmap_t {
	int width;
	int height;
	int channels;
	int stride;
	const char *name;
	size_t size;
	zest_byte *data;
} zest_bitmap_t;

typedef struct zest_bitmap_array_t {
	int width;
	int height;
	int channels;
	int stride;
	const char *name;
	zest_uint size_of_array;
	size_t size_of_each_image;
	size_t total_mem_size;
	zest_byte *data;
} zest_bitmap_array_t;

typedef struct zest_image_t {
	zest_index index;			//index within the QulkanTexture
	const char *name;			//Filename of the image
	zest_uint width;
	zest_uint height;
	zest_vec4 uv;				//UV coords are set after the ProcessImages function is called and the images are packed into the texture
	zest_uint uv_xy;
	zest_uint uv_zw;
	zest_byte layer;			//the layer index of the image when it's packed into an image/texture array
	zest_uint top, left;		//the top left location of the image on the layer or spritesheet
	zest_vec2 min;				//The minimum coords of the vertices in the quad of the image
	zest_vec2 max;				//The maximum coords of the vertices in the quad of the image
	zest_vec2 scale;			//The scale of the image. 1.f if default, changing this will update the min/max coords
	zest_vec2 handle;			//The handle of the image. 0.5f is the center of the image
	float max_radius;			//You can load images and calculate the max radius of the image which is the furthest pixel from the center.
	zest_texture texture;		//Pointer to the texture that this image belongs to
} zest_image_t;

typedef struct zest_framebuffer_attachment_t {
	VkImage image;
	zest_buffer_t *buffer;
	VkImageView view;
	VkFormat format;
} zest_framebuffer_attachment_t;

typedef struct zest_texture_t {
	zest_struct_type struct_type;
	VkDescriptorImageInfo descriptor;
	VkImageLayout image_layout;
	VkFormat image_format;

	zest_text name;

	zest_imgui_blendtype imgui_blend_type;
	VkSampler sampler;
	zest_index image_index;									//Tracks the UID of image indexes in the qvec
	VkDescriptorSet current_descriptor_set[ZEST_MAX_FIF];
	zest_map_texture_descriptor_sets descriptor_sets;
	zest_uint packed_border_size;

	zest_buffer stream_staging_buffer;						//Used for stream buffers only if you need to update the texture every frame
	zest_uint texture_layer_size;
	zest_image_t texture;
	zest_bitmap_t texture_bitmap;
	zest_frame_buffer_attachment_t frame_buffer;
	//Todo: option to not keep the images in memory after they're uploaded to the graphics card
	zest_bitmap_t *image_bitmaps;
	zest_image *images;
	zest_bitmap_t *layers;
	zest_bitmap_array_t bitmap_array;
	zest_uint layer_count;
	VkBufferImageCopy *buffer_copy_regions;
	zest_texture_storage_type storage_type;

	VkSamplerCreateInfo sampler_info;
	zest_uint color_channels;

	//Use this for adding samplers to bind to the shader
	VkImageViewType image_view_type;
	zest_texture_flags flags;
} zest_texture_t;

typedef struct zest_render_target_create_info_t {
	VkRect2D viewport;												//The viewport/size of the render target
	zest_vec2 ratio_of_screen_size;									//If you want the size of the target to be a ration of the current swap chain/window size
	VkFormat render_format;											//Always set to the swap chain render format
	zest_imgui_blendtype imgui_blend_type;							//Set the blend type for imgui. useful to change this if you're rendering this render target to an imgui window
	VkSamplerAddressMode sampler_address_mode_u;
	VkSamplerAddressMode sampler_address_mode_v;
	VkSamplerAddressMode sampler_address_mode_w;
	zest_render_target_flags flags;
	zest_render_target input_source;
} zest_render_target_create_info_t;

typedef struct zest_render_target_t {
	const char *name;

	zest_final_render_push_constants_t push_constants;
	zest_render_target_create_info_t create_info;

	void(*post_process_callback)(zest_render_target_t *target, void *user_data);
	void *post_process_user_data;

	zest_frame_buffer_t framebuffers[ZEST_MAX_FIF];
	zest_render_pass render_pass;
	VkRect2D viewport;
	zest_vec4 cls_color;
	VkRect2D render_viewport;
	zest_index frames_in_flight;
	zest_render_target input_source;
	int render_width, render_height;
	VkFormat render_format;

	zest_render_target_flags flags;

	zest_image_t sampler_image;
	zest_pipeline_template_create_info_t sampler_pipeline_template;
	zest_pipeline_template_create_info_t im_gui_rt_pipeline_template;
	zest_pipeline final_render;

	zest_texture sampler_textures[ZEST_MAX_FIF];
	VkCommandBuffer *layer_command_buffers;
} zest_render_target_t;

zest_hash_map(zest_command_queue) zest_map_command_queues;
zest_hash_map(zest_render_pass) zest_map_render_passes;
zest_hash_map(zest_descriptor_set_layout) zest_map_descriptor_layouts;
zest_hash_map(zest_pipeline) zest_map_pipelines;
zest_hash_map(zest_command_queue_draw_commands) zest_map_command_queue_draw_commands;
zest_hash_map(zest_command_queue_compute) zest_map_command_queue_computes;
zest_hash_map(zest_draw_routine) zest_map_draw_routines;
zest_hash_map(zest_buffer_allocator) zest_map_buffer_allocators;
zest_hash_map(zest_layer) zest_map_layers;
zest_hash_map(zest_descriptor_buffer) zest_map_uniform_buffers;
zest_hash_map(VkDescriptorSet) zest_map_descriptor_sets;
zest_hash_map(zest_texture) zest_map_textures;
zest_hash_map(zest_render_target) zest_map_render_targets;
zest_hash_map(zest_font) zest_map_fonts;
zest_hash_map(zest_compute) zest_map_computes;

typedef struct zest_renderer_t {
	zest_semaphores_t semaphores[ZEST_MAX_FIF];

	VkFormat swapchain_image_format;
    VkExtent2D swapchain_extent;
	VkExtent2D window_extent;
    float dpi_scale;
	VkSwapchainKHR swapchain;

	VkFence fif_fence[ZEST_MAX_FIF];
	zest_descriptor_buffer standard_uniform_buffer;

	VkImage *swapchain_images;
	VkImageView *swapchain_image_views;
	VkFramebuffer *swapchain_frame_buffers;

	zest_buffer_t *image_resource_buffer;
	zest_buffer_t *depth_resource_buffer;
	zest_frame_buffer_t *framebuffer;

	zest_render_pass final_render_pass;
	zest_frame_buffer_attachment_t final_render_pass_depth_attachment;
	zest_final_render_push_constants_t push_constants;
	VkDescriptorBufferInfo view_buffer_info[ZEST_MAX_FIF];

	VkPipelineCache pipeline_cache;

	zest_map_buffer_allocators buffer_allocators;									//For non frame in flight buffers

	//Context data
	VkCommandBuffer current_command_buffer;
	zest_command_queue_draw_commands current_draw_commands;
	zest_command_queue_compute current_compute_routine;

	//General Storage
	zest_map_command_queues command_queues;
	zest_map_render_passes render_passes;
	zest_map_descriptor_layouts descriptor_layouts;
	zest_map_pipelines pipelines;
	zest_map_command_queue_draw_commands command_queue_draw_commands;
	zest_map_command_queue_computes command_queue_computes;
	zest_map_draw_routines draw_routines;
	zest_map_layers layers;
	zest_map_textures textures;
	zest_map_render_targets render_targets;
	zest_map_fonts fonts;
	zest_map_uniform_buffers uniform_buffers;
	zest_map_descriptor_sets descriptor_sets;
	zest_map_computes computes;

	zest_command_queue active_command_queue;
	zest_command_queue_t empty_queue;
	VkDescriptorPool descriptor_pool;
	zest_command_setup_context_t setup_context;

	zest_render_target current_render_target;
	zest_render_target root_render_target;

	//For scheduled tasks
	zest_render_target *render_target_recreate_queue;
	zest_index *rt_sampler_refresh_queue[ZEST_MAX_FIF];
	zest_texture *texture_refresh_queue[ZEST_MAX_FIF];
	zest_texture *texture_reprocess_queue[ZEST_MAX_FIF];
	zest_texture *texture_delete_queue;
	zest_uint current_frame;

	//Flags
	zest_renderer_flags flags;

	//Callbacks for customising window and surface creation
	void(*get_window_size_callback)(void *user_data, int *fb_width, int *fb_height, int *window_width, int *window_height);
	void(*destroy_window_callback)(void *user_data);
	void(*poll_events_callback)(ZEST_PROTOTYPE);
	void(*add_platform_extensions_callback)(ZEST_PROTOTYPE);
	zest_window_t*(*create_window_callback)(int x, int y, int width, int height, zest_bool maximised, const char* title);
	void(*create_window_surface_callback)(zest_window_t* window);

} zest_renderer_t;

extern zest_device_t *ZestDevice;
extern zest_app_t *ZestApp;
extern zest_renderer_t *ZestRenderer;

ZEST_GLOBAL const char* zest_validation_layers[zest__validation_layer_count] = {
	"VK_LAYER_KHRONOS_validation"
};

ZEST_GLOBAL const char* zest_required_extensions[zest__required_extension_names_count] = {
	VK_KHR_SWAPCHAIN_EXTENSION_NAME
};


//--Internal functions

//Platform dependent functions
//These functions need a different implementation depending on the platform being run on
//See definitions at the top of zest.c
ZEST_PRIVATE zest_window_t *zest__os_create_window(int x, int y, int width, int height, zest_bool maximised, const char* title);
ZEST_PRIVATE void zest__os_create_window_surface(zest_window_t* window);
ZEST_PRIVATE void zest__os_poll_events(ZEST_PROTOTYPE);
ZEST_PRIVATE void zest__os_add_platform_extensions(ZEST_PROTOTYPE);
ZEST_PRIVATE void zest__os_set_window_title(const char *title);
//-- End Platform dependent functions

ZEST_PRIVATE void* zest__vec_reserve(void *T, zest_uint unit_size, zest_uint new_capacity);

//Buffer & Memory Management
ZEST_PRIVATE void zest__add_host_memory_pool(zest_size size);
ZEST_PRIVATE void *zest__allocate(zest_size size);
ZEST_PRIVATE void *zest__allocate_aligned(zest_size size, zest_size alignment);
ZEST_PRIVATE void *zest__reallocate(void *memory, zest_size size);
ZEST_PRIVATE VkResult zest__bind_memory(zest_device_memory_pool memory_allocation, VkDeviceSize offset);
ZEST_PRIVATE VkResult zest__map_memory(zest_device_memory_pool memory_allocation, VkDeviceSize size, VkDeviceSize offset);
ZEST_PRIVATE void zest__unmap_memory(zest_device_memory_pool memory_allocation);
ZEST_PRIVATE void zest__destroy_memory(zest_device_memory_pool memory_allocation);
ZEST_PRIVATE VkResult zest__flush_memory(zest_device_memory_pool memory_allocation, VkDeviceSize size, VkDeviceSize offset);
ZEST_PRIVATE zest_device_memory_pool zest__create_vk_memory_pool(zest_buffer_info_t *buffer_info, VkImage image, zest_size size);
ZEST_PRIVATE void zest__add_remote_range_pool(zest_buffer_allocator buffer_allocator, zest_device_memory_pool buffer_pool);
ZEST_PRIVATE void zest__set_buffer_details(zest_buffer_allocator buffer_allocator, zest_buffer_t *buffer, zest_bool is_host_visible);
//End Buffer Management

//Renderer functions
ZEST_PRIVATE void zest__initialise_renderer(zest_create_info_t *create_info);
ZEST_PRIVATE void zest__create_swapchain(void);
ZEST_PRIVATE VkSurfaceFormatKHR zest__choose_swapchain_format(VkSurfaceFormatKHR *availableFormats);
ZEST_PRIVATE VkPresentModeKHR zest_choose_present_mode(VkPresentModeKHR *available_present_modes, zest_bool use_vsync);
ZEST_PRIVATE VkExtent2D zest_choose_swap_extent(VkSurfaceCapabilitiesKHR *capabilities);
ZEST_PRIVATE void zest__create_pipeline_cache();
ZEST_PRIVATE void zest__get_window_size_callback(void *user_data, int *fb_width, int *fb_height, int *window_width, int *window_height);
ZEST_PRIVATE void zest__destroy_window_callback(void *user_data);
ZEST_PRIVATE void zest__cleanup_swapchain(void);
ZEST_PRIVATE void zest__cleanup_renderer(void);
ZEST_PRIVATE void zest__clean_up_compute(zest_compute compute);
ZEST_PRIVATE void zest__recreate_swapchain(void);
ZEST_PRIVATE void zest__add_push_constant(zest_pipeline_template_create_info_t *create_info, VkPushConstantRange push_constant);
ZEST_PRIVATE void zest__add_draw_routine(zest_command_queue_draw_commands command_queue_draw, zest_draw_routine draw_routine);
ZEST_PRIVATE void zest__create_swapchain_image_views(void);
ZEST_PRIVATE void zest__make_standard_render_passes(void);
ZEST_PRIVATE zest_render_pass zest__add_render_pass(const char *name, VkRenderPass render_pass);
ZEST_PRIVATE zest_buffer_t *zest__create_depth_resources(void);
ZEST_PRIVATE void zest__create_swap_chain_frame_buffers(zest_bool depth_buffer);
ZEST_PRIVATE void zest__create_sync_objects(void);
ZEST_PRIVATE zest_uniform_buffer zest__add_uniform_buffer(const char *name, zest_uniform_buffer buffer);
ZEST_PRIVATE void zest__create_descriptor_pools(VkDescriptorPoolSize *pool_sizes);
ZEST_PRIVATE void zest__make_standard_descriptor_layouts(void);
ZEST_PRIVATE void zest__prepare_standard_pipelines(void);
ZEST_PRIVATE void zest__create_empty_command_queue(zest_command_queue command_queue);
ZEST_PRIVATE void zest__render_blank(zest_command_queue_draw_commands item, VkCommandBuffer command_buffer, zest_render_pass render_pass, VkFramebuffer framebuffer);
ZEST_PRIVATE void zest__destroy_pipeline_set(zest_pipeline_t *p);
ZEST_PRIVATE void zest__cleanup_pipelines(void);
ZEST_PRIVATE void zest__cleanup_textures(void);
ZEST_PRIVATE void zest__cleanup_framebuffer(zest_frame_buffer_t *frame_buffer);
ZEST_PRIVATE void zest__rebuild_pipeline(zest_pipeline_t *pipeline);
ZEST_PRIVATE void zest__present_frame(void);
ZEST_PRIVATE zest_bool zest__acquire_next_swapchain_image(void);
ZEST_PRIVATE void zest__draw_renderer_frame(void);
// --End Renderer functions

// --Command Queue functions
ZEST_PRIVATE void zest__cleanup_command_queue(zest_command_queue command_queue);
ZEST_PRIVATE void zest__record_command_queue(zest_command_queue command_queue, zest_index fif);
ZEST_PRIVATE void zest__submit_command_queue(zest_command_queue command_queue, VkFence fence);
ZEST_PRIVATE zest_command_queue_draw_commands zest__create_command_queue_draw_commands(const char *name);
// --End Command Queue functions

// --Command Queue Setup functions
ZEST_PRIVATE zest_command_queue zest__create_command_queue(const char *name);
ZEST_PRIVATE void zest__set_queue_context(zest_setup_context_type context);
ZEST_PRIVATE zest_draw_routine zest__create_draw_routine_with_builtin_layer(const char *name, zest_builtin_layer_type builtin_layer);

// --Draw layer internal functions
ZEST_PRIVATE void zest__start_instance_instructions(zest_layer instance_layer);
ZEST_PRIVATE void zest__update_instance_layer_buffers_callback(zest_draw_routine draw_routine, VkCommandBuffer command_buffer);
ZEST_PRIVATE void zest__update_instance_layer_resolution(zest_layer layer);
ZEST_PRIVATE void zest__draw_instance_layer(zest_layer instance_layer, VkCommandBuffer command_buffer);
ZEST_PRIVATE zest_instance_instruction_t zest__instance_instruction(void);
ZEST_PRIVATE void zest__reset_instance_layer_drawing(zest_layer sprite_layer);

// --Sprite layer internal functions
ZEST_PRIVATE void zest__draw_sprite_layer_callback(zest_draw_routine draw_routine, VkCommandBuffer command_buffer);
ZEST_PRIVATE void zest__next_sprite_instance(zest_layer layer);

// --Font layer internal functions
ZEST_PRIVATE void zest__draw_font_layer_callback(zest_draw_routine draw_routine, VkCommandBuffer command_buffer);
ZEST_PRIVATE void zest__next_font_instance(zest_layer layer);

// --Billboard layer internal functions
ZEST_PRIVATE void zest__draw_billboard_layer_callback(zest_draw_routine draw_routine, VkCommandBuffer command_buffer);
ZEST_PRIVATE void zest__next_billboard_instance(zest_layer layer);

// --General Helper Functions
ZEST_PRIVATE VkImageView zest__create_image_view(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags, zest_uint mipLevels, VkImageViewType viewType, zest_uint layerCount);
ZEST_PRIVATE zest_buffer_t *zest__create_image(zest_uint width, zest_uint height, zest_uint mipLevels, VkSampleCountFlagBits numSamples, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkImage *image);
ZEST_PRIVATE zest_buffer_t *zest__create_image_array(zest_uint width, zest_uint height, zest_uint mipLevels, zest_uint layers, VkSampleCountFlagBits numSamples, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkImage *image);
ZEST_PRIVATE void zest__copy_buffer_to_image(VkBuffer buffer, VkDeviceSize src_offset, VkImage image, zest_uint width, zest_uint height, VkImageLayout image_layout);
ZEST_PRIVATE void zest__copy_buffer_regions_to_image(VkBufferImageCopy *regions, VkBuffer buffer, VkDeviceSize src_offset, VkImage image);
ZEST_PRIVATE void zest__transition_image_layout(VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout, zest_uint mipLevels, zest_uint layerCount);
ZEST_PRIVATE VkRenderPass zest__create_render_pass(VkFormat render_format, VkImageLayout final_layout, VkAttachmentLoadOp load_op, zest_bool depth_buffer);
ZEST_PRIVATE VkFormat zest__find_depth_format(void);
ZEST_PRIVATE zest_bool zest__has_stencil_format(VkFormat format);
ZEST_PRIVATE VkFormat zest__find_supported_format(VkFormat *candidates, zest_uint candidates_count, VkImageTiling tiling, VkFormatFeatureFlags features);
ZEST_PRIVATE VkCommandBuffer zest__begin_single_time_commands(void);
ZEST_PRIVATE void zest__end_single_time_commands(VkCommandBuffer command_buffer);
ZEST_PRIVATE zest_index zest__next_fif(void);
// --End General Helper Functions

// --Buffer allocation funcitons
ZEST_PRIVATE void zest__create_device_memory_pool(VkDeviceSize size, VkBufferUsageFlags usage_flags, VkMemoryPropertyFlags property_flags, zest_device_memory_pool buffer, const char *name);
ZEST_PRIVATE void zest__create_image_memory_pool(VkDeviceSize size_in_bytes, VkImage image, VkMemoryPropertyFlags property_flags, zest_device_memory_pool buffer);
ZEST_PRIVATE zest_size zest__get_minimum_block_size(zest_size pool_size);
ZEST_PRIVATE void zest__on_add_pool(void *user_data, void *block);
ZEST_PRIVATE void zest__on_split_block(void *user_data, zloc_header* block, zloc_header *trimmed_block, zest_size remote_size);
// --End Buffer allocation funcitons

// --Maintenance functions
ZEST_PRIVATE void zest__delete_texture(zest_texture texture);
ZEST_PRIVATE void zest__delete_font(zest_font_t *font);
ZEST_PRIVATE void zest__cleanup_texture(zest_texture texture);
ZEST_PRIVATE void zest__free_all_texture_images(zest_texture texture);
// --End Maintenance functions

//Device set up 
ZEST_PRIVATE void zest__create_instance(void);
ZEST_PRIVATE void zest__setup_validation(void);
ZEST_PRIVATE VKAPI_ATTR VkBool32 VKAPI_CALL zest_debug_callback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageType, const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData);
ZEST_PRIVATE VkResult zest_create_debug_messenger(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pDebugMessenger);
ZEST_PRIVATE void zest_destroy_debug_messenger(void);
ZEST_PRIVATE void zest__pick_physical_device(void);
ZEST_PRIVATE zest_bool zest__is_device_suitable(VkPhysicalDevice physical_device);
ZEST_PRIVATE zest_queue_family_indices zest__find_queue_families(VkPhysicalDevice physical_device);
ZEST_PRIVATE inline void zest__set_graphics_family(zest_queue_family_indices *family, zest_uint v) { family->graphics_family = v; family->graphics_set = 1; }
ZEST_PRIVATE inline void zest__set_present_family(zest_queue_family_indices *family, zest_uint v) { family->present_family = v; family->present_set = 1; }
ZEST_PRIVATE inline void zest__set_compute_family(zest_queue_family_indices *family, zest_uint v) { family->compute_family = v; family->compute_set = 1; }
ZEST_PRIVATE inline zest_bool zest__family_is_complete(zest_queue_family_indices *family) { return family->graphics_set && family->present_set && family->compute_set; }
ZEST_PRIVATE zest_bool zest__check_device_extension_support(VkPhysicalDevice physical_device);
ZEST_PRIVATE zest_swapchain_support_details_t zest__query_swapchain_support(VkPhysicalDevice physical_device);
ZEST_PRIVATE VkSampleCountFlagBits zest__get_max_useable_sample_count(void);
ZEST_PRIVATE void zest__create_logical_device(void);
ZEST_PRIVATE void zest__set_limit_data(void);
ZEST_PRIVATE zest_bool zest__check_validation_layer_support(void);
ZEST_PRIVATE void zest__get_required_extensions(void);
ZEST_PRIVATE zest_uint zest_find_memory_type(zest_uint typeFilter, VkMemoryPropertyFlags properties);
ZEST_PRIVATE void zest__set_default_pool_sizes(void);
ZEST_PRIVATE void *zest_vk_allocate_callback(void* pUserData, size_t size, size_t alignment, VkSystemAllocationScope allocationScope);
ZEST_PRIVATE void *zest_vk_reallocate_callback(void* pUserData, void *memory, size_t size, size_t alignment, VkSystemAllocationScope allocationScope);
ZEST_PRIVATE void zest_vk_free_callback(void* pUserData, void *memory);
//end device setup functions

//App initialise/run functions
ZEST_PRIVATE void zest__do_scheduled_tasks(zest_index index);
ZEST_PRIVATE void zest__initialise_app(zest_create_info_t *create_info);
ZEST_PRIVATE void zest__initialise_device(void);
ZEST_PRIVATE void zest__destroy(void);
ZEST_PRIVATE void zest__main_loop(void);
ZEST_PRIVATE zest_microsecs zest__set_elapsed_time(void);
//-- end of internal functions

//-- Window related functions
ZEST_PRIVATE void zest__update_window_size(zest_window_t* window, zest_uint width, zest_uint height);
//-- End Window related functions

//User API functions
ZEST_API zest_create_info_t zest_CreateInfo(void);
ZEST_API void zest_Initialise(zest_create_info_t *info);
ZEST_API void zest_AddInstanceExtension(char *extension);
ZEST_API void zest_Start(void);
ZEST_API zest_window_t *zest_AllocateWindow();
ZEST_API zest_descriptor_set_layout zest_AddDescriptorLayout(const char *name, VkDescriptorSetLayout layout);
ZEST_API VkDescriptorSetLayout zest_CreateDescriptorSetLayout(zest_uint uniforms, zest_uint samplers, zest_uint storage_buffers);
ZEST_API VkDescriptorSetLayoutBinding zest_CreateDescriptorLayoutBinding(VkDescriptorType type, VkShaderStageFlags stageFlags, zest_uint binding, zest_uint descriptorCount);
ZEST_API VkDescriptorSetLayoutBinding zest_CreateUniformLayoutBinding(zest_uint binding);
ZEST_API VkDescriptorSetLayoutBinding zest_CreateSamplerLayoutBinding(zest_uint binding);
ZEST_API VkDescriptorSetLayoutBinding zest_CreateStorageLayoutBinding(zest_uint binding);
ZEST_API VkDescriptorSetLayout zest_CreateDescriptorSetLayoutWithBindings(VkDescriptorSetLayoutBinding *bindings);
ZEST_API VkWriteDescriptorSet zest_CreateBufferDescriptorWriteWithType(VkDescriptorSet descriptor_set, VkDescriptorBufferInfo *view_buffer_info, zest_uint dst_binding, VkDescriptorType type);
ZEST_API VkWriteDescriptorSet zest_CreateImageDescriptorWriteWithType(VkDescriptorSet descriptor_set, VkDescriptorImageInfo *view_buffer_info, zest_uint dst_binding, VkDescriptorType type);
ZEST_API zest_descriptor_set_builder_t zest_NewDescriptorSetBuilder();
ZEST_API void zest_AddBuilderDescriptorWriteImage(zest_descriptor_set_builder_t *builder, VkDescriptorImageInfo *view_image_info, zest_uint dst_binding, VkDescriptorType type);
ZEST_API void zest_AddBuilderDescriptorWriteBuffer(zest_descriptor_set_builder_t *builder, VkDescriptorBufferInfo *view_buffer_info, zest_uint dst_binding, VkDescriptorType type);
ZEST_API void zest_AddBuilderDescriptorWriteImages(zest_descriptor_set_builder_t *builder, zest_uint image_count, VkDescriptorImageInfo *view_image_info, zest_uint dst_binding, VkDescriptorType type);
ZEST_API void zest_AddBuilderDescriptorWriteBuffers(zest_descriptor_set_builder_t *builder, zest_uint buffer_count, VkDescriptorBufferInfo *view_buffer_info, zest_uint dst_binding, VkDescriptorType type);
ZEST_API zest_descriptor_set_t zest_BuildDescriptorSet(zest_descriptor_set_builder_t *builder, zest_descriptor_set_layout layout);
ZEST_API void zest_AllocateDescriptorSet(VkDescriptorPool descriptor_pool, VkDescriptorSetLayout descriptor_layout, VkDescriptorSet *descriptor_set);
ZEST_API void zest_UpdateDescriptorSet(VkWriteDescriptorSet *descriptor_writes);
ZEST_API VkViewport zest_CreateViewport(float x, float y, float width, float height, float minDepth, float maxDepth);
ZEST_API VkRect2D zest_CreateRect2D(zest_uint width, zest_uint height, int offsetX, int offsetY);
ZEST_API void zest_SetUserData(void* data);
ZEST_API void zest_SetUserUpdateCallback(void(*callback)(zest_microsecs, void*));
ZEST_API void zest_SetActiveCommandQueue(zest_command_queue command_queue);
ZEST_API void zest_SetDestroyWindowCallback(void(*destroy_window_callback)(void *user_data));
ZEST_API void zest_SetGetWindowSizeCallback(void(*get_window_size_callback)(void *user_data, int *fb_width, int *fb_height, int *window_width, int *window_height));
ZEST_API void zest_SetPollEventsCallback(void(*poll_events_callback)(void));
ZEST_API void zest_SetPlatformExtensionsCallback(void(*add_platform_extensions_callback)(void));
//--End User API functions

//Pipeline related 
ZEST_API zest_pipeline zest_AddPipeline(const char *name);
ZEST_API zest_vertex_input_descriptions zest_NewVertexInputDescriptions();
ZEST_API void zest_AddVertexInputDescription(zest_vertex_input_descriptions *descriptions, VkVertexInputAttributeDescription description);
ZEST_API void zest_BuildPipeline(zest_pipeline pipeline);
ZEST_API void zest_UpdatePipelineTemplate(zest_pipeline_template_t *pipeline_template, zest_pipeline_template_create_info_t *create_info);
ZEST_API void zest_SetPipelineTemplate(zest_pipeline_template_t *pipeline_template, zest_pipeline_template_create_info_t *create_info);
ZEST_API void zest_MakePipelineTemplate(zest_pipeline pipeline, VkRenderPass render_pass, zest_pipeline_template_create_info_t *create_info);
ZEST_API void zest_SetPipelineTemplatePushConstant(zest_pipeline_template_create_info_t *create_info, zest_uint size, zest_uint offset, VkShaderStageFlags stage_flags);
ZEST_API zest_pipeline_template_t *zest_PipelineTemplate(zest_pipeline pipeline);
ZEST_API VkShaderModule zest_CreateShaderModule(char *code);
ZEST_API zest_pipeline_template_create_info_t zest_CreatePipelineTemplateCreateInfo(void);
ZEST_API void zest_AddPipelineTemplatePushConstantRange(zest_pipeline_template_create_info_t *create_info, VkPushConstantRange range);
ZEST_API VkVertexInputBindingDescription zest_CreateVertexInputBindingDescription(zest_uint binding, zest_uint stride, VkVertexInputRate input_rate);
ZEST_API VkVertexInputAttributeDescription zest_CreateVertexInputDescription(zest_uint binding, zest_uint location, VkFormat format, zest_uint offset);
ZEST_API VkPipelineColorBlendAttachmentState zest_AdditiveBlendState(void);
ZEST_API VkPipelineColorBlendAttachmentState zest_AdditiveBlendState2(void);
ZEST_API VkPipelineColorBlendAttachmentState zest_AlphaOnlyBlendState(void);
ZEST_API VkPipelineColorBlendAttachmentState zest_AlphaBlendState(void);
ZEST_API VkPipelineColorBlendAttachmentState zest_PreMultiplyBlendState(void);
ZEST_API VkPipelineColorBlendAttachmentState zest_PreMultiplyBlendStateForSwap(void);
ZEST_API VkPipelineColorBlendAttachmentState zest_MaxAlphaBlendState(void);
ZEST_API VkPipelineColorBlendAttachmentState zest_ImGuiBlendState(void);
ZEST_API void zest_BindPipeline(zest_pipeline_t *pipeline, VkDescriptorSet descriptor_set);
ZEST_API void zest_BindComputePipeline(zest_compute compute, zest_index shader_index);
ZEST_API void zest_BindPipelineCB(VkCommandBuffer command_buffer, zest_pipeline_t *pipeline, VkDescriptorSet descriptor_set);
ZEST_API zest_pipeline zest_Pipeline(const char *name);
ZEST_API zest_pipeline_template_create_info_t zest_CopyTemplateFromPipeline(const char *pipeline_name);
//-- End Pipeline related 

//Buffer related
ZEST_API void zloc__output_buffer_info(void* ptr, size_t size, int free, void* user, int count);
ZEST_API zloc__error_codes zloc_VerifyRemoteBlocks(zloc_header *first_block, zloc__block_output output_function, void *user_data);
ZEST_API zest_uint zloc_CountBlocks(zloc_header *first_block);
ZEST_API zest_buffer_t *zest_CreateBuffer(VkDeviceSize size, zest_buffer_info_t *buffer_info, VkImage image);
ZEST_API zest_buffer_t *zest_CreateStagingBuffer(VkDeviceSize size, void *data);
ZEST_API zest_buffer_t *zest_CreateIndexBuffer(VkDeviceSize size, zest_buffer staging_buffer);
ZEST_API zest_buffer_t *zest_CreateVertexBuffer(VkDeviceSize size, zest_buffer staging_buffer);
ZEST_API zest_buffer_t *zest_CreateStorageBuffer(VkDeviceSize size, zest_buffer staging_buffer);
ZEST_API zest_buffer_t *zest_CreateComputeVertexBuffer(VkDeviceSize size, zest_buffer staging_buffer);
ZEST_API zest_buffer_t *zest_CreateComputeIndexBuffer(VkDeviceSize size, zest_buffer staging_buffer);
ZEST_API zest_bool zest_GrowBuffer(zest_buffer_t **buffer, zest_size unit_size, zest_size minimum_bytes);
ZEST_API zest_buffer_info_t zest_CreateVertexBufferInfo(void);
ZEST_API zest_buffer_info_t zest_CreateStorageBufferInfo(void);
ZEST_API zest_buffer_info_t zest_CreateComputeVertexBufferInfo(void);
ZEST_API zest_buffer_info_t zest_CreateComputeIndexBufferInfo(void);
ZEST_API zest_buffer_info_t zest_CreateIndexBufferInfo(void);
ZEST_API zest_buffer_info_t zest_CreateStagingBufferInfo(void);
ZEST_API void zest_CopyBuffer(zest_buffer staging_buffer, zest_buffer device_buffer);
ZEST_API void zest_FreeBuffer(zest_buffer_t *buffer);
ZEST_API VkDeviceMemory zest_GetBufferDeviceMemory(zest_buffer_t *buffer);
ZEST_API VkBuffer *zest_GetBufferDeviceBuffer(zest_buffer_t *buffer);
ZEST_API void zest_AddCopyCommand(zest_buffer_uploader_t *uploader, zest_buffer_t *source_buffer, zest_buffer_t *target_buffer, VkDeviceSize target_offset);
ZEST_API zest_bool zest_UploadBuffer(zest_buffer_uploader_t *uploader, VkCommandBuffer command_buffer);
ZEST_API zest_buffer_pool_size_t zest_GetDevicePoolSize(VkBufferUsageFlags usage_flags, VkMemoryPropertyFlags property_flags, VkImageUsageFlags image_flags);
ZEST_API void zest_SetDevicePoolSize(const char *name, VkBufferUsageFlags usage_flags, VkMemoryPropertyFlags property_flags, VkImageUsageFlags image_flags, zest_size minimum_allocation, zest_size pool_size);
ZEST_API zest_uniform_buffer zest_CreateUniformBuffer(const char *name, zest_size uniform_struct_size);
ZEST_API void zest_Update2dUniformBuffer(void);
ZEST_API void zest_Update2dUniformBufferFIF(zest_index fif);
ZEST_API zest_descriptor_buffer zest_CreateDescriptorBuffer(zest_buffer_info_t *buffer_info, zest_size size, zest_bool all_frames_in_flight);
ZEST_API zest_buffer zest_GetBufferFromDescriptorBuffer(zest_descriptor_buffer descriptor_buffer);
//--End Buffer related

//Command queue setup and creation
ZEST_API zest_command_queue zest_NewCommandQueue(const char *name);
ZEST_API zest_command_queue zest_NewFloatingCommandQueue(const char *name);
ZEST_API zest_command_queue zest_GetCommandQueue(const char *name);
ZEST_API zest_command_queue_draw_commands zest_GetCommandQueueDrawCommands(const char *name);
ZEST_API void zest_SetDrawCommandsClsColor(zest_command_queue_draw_commands draw_commands, float r, float g, float b, float a);
ZEST_API void zest_ConnectPresentToCommandQueue(zest_command_queue receiver, VkPipelineStageFlags stage_flags);
ZEST_API void zest_ConnectCommandQueueToPresent(zest_command_queue sender);
ZEST_API VkSemaphore zest_GetCommandQueuePresentSemaphore(zest_command_queue command_queue);
ZEST_API zest_command_queue_draw_commands zest_NewDrawCommandSetupSwap(const char *name);
ZEST_API zest_command_queue_draw_commands zest_NewDrawCommandSetupRenderTargetSwap(const char *name, zest_render_target render_target);
ZEST_API void zest_AddRenderTarget(zest_render_target render_target);
ZEST_API zest_command_queue_draw_commands zest_NewDrawCommandSetup(const char *name, zest_render_target render_target);
ZEST_API void zest_SetDrawCommandsCallback(void(*render_pass_function)(zest_command_queue_draw_commands item, VkCommandBuffer command_buffer, zest_render_pass render_pass, VkFramebuffer framebuffer));
ZEST_API zest_draw_routine zest_GetDrawRoutine(const char *name);
ZEST_API zest_command_queue_draw_commands zest_GetDrawCommands(const char *name);
ZEST_API void zest_RenderDrawRoutinesCallback(zest_command_queue_draw_commands item, VkCommandBuffer command_buffer, zest_render_pass render_pass, VkFramebuffer framebuffer);
ZEST_API void zest_DrawToRenderTargetCallback(zest_command_queue_draw_commands item, VkCommandBuffer command_buffer, zest_render_pass render_pass, VkFramebuffer framebuffer);
ZEST_API void zest_DrawRenderTargetsToSwapchain(zest_command_queue_draw_commands item, VkCommandBuffer command_buffer, zest_render_pass render_pass, VkFramebuffer framebuffer);
ZEST_API void zest_AddDrawRoutine(zest_draw_routine draw_routine);
ZEST_API void zest_AddDrawRoutineToDrawCommands(zest_command_queue_draw_commands draw_commands, zest_draw_routine draw_routine);
ZEST_API zest_layer zest_CreateBuiltinSpriteLayer(const char *name);
ZEST_API zest_layer zest_CreateBuiltinBillboardLayer(const char *name);
ZEST_API zest_layer zest_CreateBuiltinFontLayer(const char *name);
ZEST_API zest_layer zest_CreateBuiltinMeshLayer(const char *name);
ZEST_API zest_layer zest_GetLayer(const char *name);
ZEST_API zest_layer zest_NewMeshLayer(const char *name, zest_size vertex_struct_size);
ZEST_API zest_layer zest_NewBuiltinLayerSetup(const char *name, zest_builtin_layer_type builtin_layer);
ZEST_API void zest_AddLayer(zest_layer layer);
ZEST_API zest_command_queue_compute zest_CreateComputeItem(const char *name, zest_command_queue command_queue);
ZEST_API zest_command_queue_compute zest_NewComputeSetup(const char *name, zest_compute compute_shader, void(*compute_function)(zest_command_queue_compute item));
ZEST_API VkCommandBuffer zest_CurrentCommandBuffer(void); 
ZEST_API zest_command_queue zest_CurrentCommandQueue(void);
ZEST_API void zest_ModifyCommandQueue(zest_command_queue command_queue);
ZEST_API void zest_ModifyDrawCommands(zest_command_queue_draw_commands draw_commands);
ZEST_API zest_draw_routine zest_ContextDrawRoutine();
ZEST_API void zest_ContextSetClsColor(float r, float g, float b, float a);
ZEST_API void zest_FinishQueueSetup(void);
ZEST_API void zest_ValidateQueue(zest_command_queue queue);
ZEST_API void zest_ConnectCommandQueues(zest_command_queue sender, zest_command_queue receiver, VkPipelineStageFlags stage_flags);
ZEST_API void zest_ConnectQueueTo(zest_command_queue receiver, VkPipelineStageFlags stage_flags);
ZEST_API void zest_ConnectQueueToPresent(void);
//-- End Command queue setup and creation

//General Math
ZEST_API zest_vec3 zest_SubVec3(zest_vec3 left, zest_vec3 right);
ZEST_API zest_vec4 zest_SubVec4(zest_vec4 left, zest_vec4 right);
ZEST_API zest_vec3 zest_AddVec3(zest_vec3 left, zest_vec3 right);
ZEST_API zest_vec4 zest_AddVec4(zest_vec4 left, zest_vec4 right);
ZEST_API float zest_LengthVec2(zest_vec3 const v);
ZEST_API float zest_LengthVec(zest_vec3 const v);
ZEST_API float zest_Vec2Length(zest_vec2 const v);
ZEST_API float zest_Vec2Length2(zest_vec2 const v);
ZEST_API zest_vec2 zest_NormalizeVec2(zest_vec2 const v);
ZEST_API zest_vec3 zest_NormalizeVec3(zest_vec3 const v);
ZEST_API zest_vec4 zest_MatrixTransformVector(zest_matrix4 *mat, zest_vec4 vec);
ZEST_API zest_matrix4 zest_Inverse(zest_matrix4 *m);
ZEST_API zest_vec3 zest_CrossProduct(const zest_vec3 x, const zest_vec3 y);
ZEST_API float zest_DotProduct(const zest_vec3 a, const zest_vec3 b);
ZEST_API zest_matrix4 zest_LookAt(const zest_vec3 eye, const zest_vec3 center, const zest_vec3 up);
ZEST_API zest_matrix4 zest_Ortho(float left, float right, float bottom, float top, float z_near, float z_far);
ZEST_API float zest_Distance(float fromx, float fromy, float tox, float toy);
ZEST_API zest_uint zest_Pack10bit(zest_vec3 *v, zest_uint extra);
ZEST_API zest_uint zest_Pack8bitx3(zest_vec3 *v);
ZEST_API zest_uint zest_Pack16bit(float x, float y);
ZEST_API zest_size zest_GetNextPower(zest_size n);
ZEST_API float zest_Radians(float degrees);
ZEST_API float zest_Degrees(float radians);
ZEST_API zest_matrix4 zest_M4(float v);
ZEST_API zest_vec2 zest_ScaleVec2(zest_vec2 *vec2, float v);
ZEST_API zest_vec3 zest_ScaleVec3(zest_vec3 *vec3, float v);
ZEST_API zest_vec4 zest_ScaleVec4(zest_vec4 *vec4, float v);
ZEST_API zest_vec4 zest_MulVec4(zest_vec4 *left, zest_vec4 *right);
ZEST_API zest_matrix4 zest_ScaleMatrix4x4(zest_matrix4 *m, zest_vec4 *v);
ZEST_API zest_matrix4 zest_ScaleMatrix4(zest_matrix4 *m, float scalar);
ZEST_API zest_vec2 zest_Vec2Set1(float v);
ZEST_API zest_vec3 zest_Vec3Set1(float v);
ZEST_API zest_vec4 zest_Vec4Set1(float v);
ZEST_API zest_vec2 zest_Vec2Set(float x, float y);
ZEST_API zest_vec3 zest_Vec3Set(float x, float y, float z);
ZEST_API zest_vec4 zest_Vec4Set(float x, float y, float z, float w);
ZEST_API zest_color zest_ColorSet(zest_byte r, zest_byte g, zest_byte b, zest_byte a);
ZEST_API zest_color zest_ColorSet1(zest_byte c);
//--End General Math

//Camera and other helpers
ZEST_API zest_camera_t zest_CreateCamera(void);
ZEST_API void zest_TurnCamera(zest_camera_t *camera, float turn_x, float turn_y, float sensitivity);
ZEST_API void zest_CameraUpdateFront(zest_camera_t *camera);
ZEST_API void zest_CameraMoveForward(zest_camera_t *camera, float speed);
ZEST_API void zest_CameraMoveBackward(zest_camera_t *camera, float speed);
ZEST_API void zest_CameraMoveUp(zest_camera_t *camera, float speed);
ZEST_API void zest_CameraMoveDown(zest_camera_t *camera, float speed);
ZEST_API void zest_CameraStrafLeft(zest_camera_t *camera, float speed);
ZEST_API void zest_CameraStrafRight(zest_camera_t *camera, float speed);
ZEST_API void zest_CameraPosition(zest_camera_t *camera, zest_vec3 position);
ZEST_API void zest_CameraSetFoV(zest_camera_t *camera, float degrees);
ZEST_API zest_bool zest_RayIntersectPlane(zest_vec3 ray_origin, zest_vec3 ray_direction, zest_vec3 plane, zest_vec3 plane_normal, float *distance, zest_vec3 *intersection);
ZEST_API zest_vec3 zest_ScreenRay(float xpos, float ypos, float view_width, float view_height, zest_matrix4 *projection, zest_matrix4 *view);
ZEST_API zest_vec2 zest_WorldToScreen(const zest_vec3 *point, float view_width, float view_height, zest_matrix4 *projection, zest_matrix4 *view);
ZEST_API zest_vec2 zest_WorldToScreenOrtho(const zest_vec3 *point, float view_width, float view_height, zest_matrix4 *projection, zest_matrix4 *view);
ZEST_API zest_matrix4 zest_Perspective(float fovy, float aspect, float zNear, float zFar);
//-- End camera and other helpers

//Images and textures
ZEST_API zest_map_textures *zest_GetTextures(void);
ZEST_API zest_texture zest_NewTexture(void);
ZEST_API zest_texture zest_CreateTexture(const char *name, zest_texture_storage_type storage_type, zest_bool use_filtering, zest_texture_format format, zest_uint reserve_images);
ZEST_API zest_texture zest_CreateTexturePacked(const char *name, zest_texture_format format);
ZEST_API zest_texture zest_CreateTextureSpritesheet(const char *name, zest_texture_format format);
ZEST_API zest_texture zest_CreateTextureSingle(const char *name, zest_texture_format format);
ZEST_API zest_texture zest_CreateTextureBank(const char *name, zest_texture_format format);
ZEST_API void zest_DeleteTexture(zest_texture texture);
ZEST_API void zest_SetTextureImageFormat(zest_texture texture, zest_texture_format format);
ZEST_API zest_image zest_CreateImage(void);
ZEST_API zest_image zest_CreateAnimation(zest_uint frames);
ZEST_API void zest_LoadBitmapImage(zest_bitmap_t *image, const char *file, int color_channels);
ZEST_API void zest_LoadBitmapImageMemory(zest_bitmap_t *image, unsigned char *buffer, int size, int desired_no_channels);
ZEST_API void zest_FreeBitmap(zest_bitmap_t *image);
ZEST_API zest_bitmap_t zest_NewBitmap(void);
ZEST_API zest_bitmap_t zest_CreateBitmapFromRawBuffer(const char *name, unsigned char *pixels, int size, int width, int height, int channels);
ZEST_API void zest_AllocateBitmap(zest_bitmap_t *bitmap, int width, int height, int channels, zest_uint fill_color);
ZEST_API void zest_CopyWholeBitmap(zest_bitmap_t *src, zest_bitmap_t *dst);
ZEST_API void zest_CopyBitmap(zest_bitmap_t *src, int from_x, int from_y, int width, int height, zest_bitmap_t *dst, int to_x, int to_y);
ZEST_API void zest_ConvertBitmapToTextureFormat(zest_bitmap_t *src, VkFormat format);
ZEST_API void zest_ConvertBitmap(zest_bitmap_t *src, zest_texture_format format, zest_byte alpha_level);
ZEST_API void zest_ConvertBitmapToBGRA(zest_bitmap_t *src, zest_byte alpha_level);
ZEST_API void zest_ConvertBitmapToRGBA(zest_bitmap_t *src, zest_byte alpha_level);
ZEST_API void zest_ConvertBitmapToAlpha(zest_bitmap_t *image);
ZEST_API void zest_ConvertBitmapTo1Channel(zest_bitmap_t *image);
ZEST_API zest_color zest_SampleBitmap(zest_bitmap_t *image, int x, int y);
ZEST_API zest_byte *zest_BitmapArrayLookUp(zest_bitmap_array_t *bitmap_array, zest_index index);
ZEST_API float zest_FindBitmapRadius(zest_bitmap_t *image);
ZEST_API void zest_DestroyBitmapArray(zest_bitmap_array_t *bitmap_array);
ZEST_API zest_bitmap_t zest_GetImageFromArray(zest_bitmap_array_t *bitmap_array, zest_index index);
ZEST_API zest_bitmap_t *zest_GetBitmap(zest_texture texture, zest_index bitmap_index);
ZEST_API zest_index zest_GetImageIndex(zest_texture texture);
ZEST_API zest_image zest_GetImageFromTexture(zest_texture texture, zest_index index);
ZEST_API zest_image zest_AddTextureImageFile(zest_texture texture, const char* name);
ZEST_API zest_image zest_AddTextureImageBitmap(zest_texture texture, zest_bitmap_t *bitmap_to_load);
ZEST_API zest_image zest_AddTextureImageMemory(zest_texture texture, const char* name, unsigned char* buffer, int buffer_size);
ZEST_API zest_image zest_AddTextureAnimationFile(zest_texture texture, const char* filename, int width, int height, zest_uint frames, float *max_radius, zest_bool row_by_row);
ZEST_API zest_image zest_AddTextureAnimationImage(zest_texture texture, zest_bitmap_t *spritesheet, int width, int height, zest_uint frames, float *max_radius, zest_bool row_by_row);
ZEST_API zest_image zest_AddTextureAnimationMemory(zest_texture texture, const char* name, unsigned char *buffer, int buffer_size, int width, int height, zest_uint frames, float *max_radius, zest_bool row_by_row);
ZEST_API float zest_CopyAnimationFrames(zest_texture texture, zest_bitmap_t *spritesheet, int width, int height, zest_uint frames, zest_bool row_by_row);
ZEST_API void zest_ProcessTextureImages(zest_texture texture);
ZEST_API void zest_DeleteTextureLayers(zest_texture texture);
ZEST_API zest_descriptor_set zest_CreateTextureSpriteDescriptorSets(zest_texture texture, const char *name, const char *uniform_buffer_name);
ZEST_API zest_descriptor_set zest_GetTextureDescriptorSet(zest_texture texture, const char *name);
ZEST_API void zest_SwitchTextureDescriptorSet(zest_texture texture, const char *name);
ZEST_API VkDescriptorSet zest_CurrentTextureDescriptorSet(zest_texture texture);
ZEST_API void zest_UpdateTextureSingleDescriptorSet(zest_texture texture, const char *name);
ZEST_API void zest_UpdateAllTextureDescriptorWrites(zest_texture texture);
ZEST_API void zest_CreateTextureImage(zest_texture texture, zest_uint mip_levels, VkImageUsageFlags usage_flags, VkImageLayout image_layout, zest_bool copy_bitmap);
ZEST_API void zest_CreateTextureImageArray(zest_texture texture, zest_uint mip_levels);
ZEST_API void zest_CreateTextureStream(zest_texture texture, zest_uint mip_levels, VkImageUsageFlags usage_flags, VkImageLayout image_layout, zest_bool copy_bitmap);
ZEST_API void zest_CreateTextureSampler(zest_texture texture, VkSamplerCreateInfo samplerInfo, zest_uint mip_levels);
ZEST_API zest_frame_buffer_t zest_CreateFrameBuffer(VkRenderPass render_pass, uint32_t width, uint32_t height, VkFormat render_format, zest_bool use_depth_buffer, zest_bool is_src);
ZEST_API zest_byte zest_CalculateTextureLayers(stbrp_rect *rects, zest_uint size, const zest_uint node_count);
ZEST_API void zest_MakeImageBank(zest_texture texture, zest_uint size);
ZEST_API void zest_MakeSpriteSheet(zest_texture texture);
ZEST_API void zest_PackImages(zest_texture texture, zest_uint size);
ZEST_API void zest_UpdateImageVertices(zest_image image);
ZEST_API void zest_UpdateTextureSingleImageMeta(zest_texture texture, zest_uint width, zest_uint height);
ZEST_API void zest_SetTextureStorageType(zest_texture texture, zest_texture_storage_type value);
ZEST_API void zest_SetTextureUseFiltering(zest_texture texture, zest_bool value);
ZEST_API void zest_SetTexturePackedBorder(zest_texture texture, zest_uint value);
ZEST_API void zest_SetTextureWrapping(zest_texture texture, VkSamplerAddressMode u, VkSamplerAddressMode v, VkSamplerAddressMode w);
ZEST_API void zest_SetTextureWrappingClamp(zest_texture texture);
ZEST_API void zest_SetTextureWrappingBorder(zest_texture texture);
ZEST_API void zest_SetTextureWrappingRepeat(zest_texture texture);
ZEST_API void zest_SetTextureLayerSize(zest_texture texture, zest_uint size);
ZEST_API void zest_SetTextureMaxRadiusOnLoad(zest_texture texture, zest_bool yesno);
ZEST_API zest_bitmap_t *zest_GetTextureSingleBitmap(zest_texture texture);
ZEST_API void zest_CreateTextureImageView(zest_texture texture, VkImageViewType view_type, zest_uint mip_levels, zest_uint layer_count);
ZEST_API void zest_AddTextureDescriptorSet(zest_texture texture, const char *name, zest_descriptor_set descriptor_set);
ZEST_API zest_texture zest_GetTexture(const char *name);
ZEST_API void zest_UpdateAllTextureDescriptorSets(zest_texture texture);
ZEST_API void zest_RefreshTextureDescriptors(zest_texture texture);
//-- End Images and textures

//-- Fonts
ZEST_API zest_font zest_LoadMSDFFont(const char *filename);
ZEST_API void zest_UnloadFont(zest_font font);
//-- End Fonts

//Render targets
ZEST_API void zest_InitialiseRenderTarget(zest_render_target render_target, zest_render_target_create_info_t *info);
ZEST_API zest_render_target zest_CreateRenderTargetWithInfo(const char *name, zest_render_target_create_info_t create_info);
ZEST_API zest_render_target zest_CreateRenderTarget(const char *name);
ZEST_API zest_render_target zest_NewRenderTarget();
ZEST_API zest_render_target_create_info_t zest_RenderTargetCreateInfo();
ZEST_API void zest_CleanUpRenderTarget(zest_render_target render_target);
ZEST_API void zest_RecreateRenderTargetResources(zest_render_target render_target);
ZEST_API void zest_CreateRenderTargetSamplerImage(zest_render_target render_target);
ZEST_API zest_render_target zest_AddPostProcessRenderTarget(const char *name, float ratio_width, float ratio_height, zest_render_target input_source, void *user_data, void(*render_callback)(zest_render_target target, void *user_data));
ZEST_API void zest_SetRenderTargetPostProcessCallback(zest_render_target render_target, void(*render_callback)(zest_render_target target, void *user_data));
ZEST_API void zest_SetRenderTargetPostProcessUserData(zest_render_target render_target, void *user_data);
ZEST_API zest_render_target zest_GetRenderTarget(const char *name);
ZEST_API VkDescriptorSet *zest_GetRenderTargetSamplerDescriptorSet(zest_render_target render_target);
ZEST_API VkDescriptorSet *zest_GetRenderTargetSourceDescriptorSet(zest_render_target render_target);
ZEST_API zest_texture zest_GetRenderTargetTexture(zest_render_target render_target);
ZEST_API zest_image zest_GetRenderTargetImage(zest_render_target render_target);
ZEST_API zest_frame_buffer_t *zest_RenderTargetFramebuffer(zest_render_target render_target);
ZEST_API zest_frame_buffer_t *zest_RenderTargetFramebufferByFIF(zest_render_target render_target, zest_index fif);
ZEST_API VkFramebuffer zest_GetRendererFrameBufferCallback(zest_command_queue_draw_commands item);
ZEST_API VkFramebuffer zest_GetRenderTargetFrameBufferCallback(zest_command_queue_draw_commands item);
ZEST_API void zest_RefreshRenderTargetSampler(zest_render_target render_target, zest_index fif);
ZEST_API void zest_PreserveRenderTargetFrames(zest_render_target render_target, zest_bool yesno);
//-- End Render targets

//Draw Routines
ZEST_API zest_draw_routine zest_CreateDrawRoutine(const char *name);
//-- End Draw Routines

//-- Draw Layers API
ZEST_API zest_layer zest_NewLayer();
ZEST_API void zest_SetInstanceLayerViewPort(zest_layer instance_layer, int x, int y, zest_uint scissor_width, zest_uint scissor_height, float viewport_width, float viewport_height);
ZEST_API void zest_SetLayerSize(zest_layer layer, float width, float height);
ZEST_API void zest_SetLayerScale(zest_layer layer, float x, float y);
ZEST_API zest_layer zest_GetLayer(const char *name);
ZEST_API void zest_SetLayerColor(zest_layer layer, zest_byte red, zest_byte green, zest_byte blue, zest_byte alpha);
ZEST_API void zest_SetLayerIntensity(zest_layer layer, float value);
//-- End Draw Layers

//Draw sprite layers
ZEST_API void zest_InitialiseSpriteLayer(zest_layer sprite_layer, zest_uint instance_pool_size);
ZEST_API void zest_SetSpriteDrawing(zest_layer sprite_layer, zest_texture texture, zest_descriptor_set descriptor_set, zest_pipeline pipeline);
ZEST_API void zest_DrawSprite(zest_layer layer, zest_image image, float x, float y, float r, float sx, float sy, float hx, float hy, zest_uint alignment, float stretch, zest_uint align_type);
//-- End Draw sprite layers

//Draw billboard layers
ZEST_API void zest_InitialiseBillboardLayer(zest_layer billboard_layer, zest_uint instance_pool_size);
ZEST_API void zest_SetBillboardDrawing(zest_layer sprite_layer, zest_texture texture, zest_descriptor_set descriptor_set, zest_pipeline pipeline);
ZEST_API void zest_DrawBillboardComplex(zest_layer layer, zest_image image, float position[3], zest_uint alignment, float angles[3], float handle[2], float stretch, zest_uint alignment_type, float sx, float sy);
ZEST_API void zest_DrawBillboard(zest_layer layer, zest_image image, float position[3], float angle, float sx, float sy);
//--End Draw billboard layers

//Draw MSDF font layers
ZEST_API void zest_InitialiseMSDFFontLayer(zest_layer font_layer, zest_uint instance_pool_size);
ZEST_API void zest_SetMSDFFontDrawing(zest_layer font_layer, zest_font font, zest_descriptor_set descriptor_set, zest_pipeline pipeline);
ZEST_API void zest_SetMSDFFontShadow(zest_layer font_layer, float shadow_length, float shadow_smoothing, float shadow_clipping);
ZEST_API void zest_SetMSDFFontShadowColor(zest_layer font_layer, float red, float green, float blue, float alpha);
ZEST_API void zest_TweakMSDFFont(zest_layer font_layer, float bleed, float expand, float aa_factor, float radius, float detail);
ZEST_API void zest_SetMSDFFontBleed(zest_layer font_layer, float bleed);
ZEST_API void zest_SetMSDFFontExpand(zest_layer font_layer, float expand);
ZEST_API void zest_SetMSDFFontAAFactor(zest_layer font_layer, float aa_factor);
ZEST_API void zest_SetMSDFFontRadius(zest_layer font_layer, float radius);
ZEST_API void zest_SetMSDFFontDetail(zest_layer font_layer, float detail);
ZEST_API float zest_DrawMSDFText(zest_layer font_layer, const char *text, float x, float y, float handle_x, float handle_y, float size, float letter_spacing, float flip);
ZEST_API float zest_DrawMSDFParagraph(zest_layer font_layer, const char *text, float x, float y, float handle_x, float handle_y, float size, float letter_spacing, float line_height, float flip);
ZEST_API float zest_TextWidth(zest_font_t *font, const char *text, float font_size, float letter_spacing);
//-- End Draw MSDF font layers

//Draw mesh layers
ZEST_API void zest_InitialiseMeshLayer(zest_layer mesh_layer, zest_size vertex_struct_size, zest_size initial_vertex_capacity);
ZEST_API void zest_UploadMeshBuffersCallback(zest_draw_routine draw_routine, VkCommandBuffer command_buffer);
ZEST_API void zest_BindMeshVertexBuffer(zest_layer layer);
ZEST_API void zest_BindMeshIndexBuffer(zest_layer layer);
ZEST_API zest_buffer_t *zest_GetVertexStagingBuffer(zest_layer layer);
ZEST_API zest_buffer_t *zest_GetIndexStagingBuffer(zest_layer layer);
ZEST_API zest_buffer_t *zest_GetVertexDeviceBuffer(zest_layer layer);
ZEST_API zest_buffer_t *zest_GetIndexDeviceBuffer(zest_layer layer);
ZEST_API void zest_GrowMeshVertexBuffers(zest_layer layer);
ZEST_API void zest_GrowMeshIndexBuffers(zest_layer layer);
//--End Draw mesh layers

//Compute shaders
ZEST_API zest_compute zest_CreateCompute(const char *name);
ZEST_API zest_compute_builder_t zest_NewComputeBuilder();
ZEST_API void zest_AddComputeLayoutBinding(zest_compute_builder_t *builder, VkDescriptorType descriptor_type, int binding);
ZEST_API void zest_AddComputeBufferForBinding(zest_compute_builder_t *builder, zest_descriptor_buffer buffer);
ZEST_API void zest_AddComputeImageForBinding(zest_compute_builder_t *builder, zest_texture texture);
ZEST_API zest_index zest_AddComputeShader(zest_compute_builder_t *builder, const char *path);
ZEST_API void zest_SetComputePushConstantSize(zest_compute_builder_t *builder, zest_uint size);
ZEST_API void zest_SetComputeDescriptorUpdateCallback(zest_compute_builder_t *builder, void(*descriptor_update_callback)(zest_compute compute));
ZEST_API void zest_SetComputeCommandBufferUpdateCallback(zest_compute_builder_t *builder, void(*command_buffer_update_callback)(zest_compute compute, VkCommandBuffer command_buffer));
ZEST_API void zest_SetComputeExtraCleanUpCallback(zest_compute_builder_t *builder, void(*extra_cleanup_callback)(zest_compute compute));
ZEST_API void zest_SetComputeUserData(zest_compute_builder_t *builder, void *data);
ZEST_API void zest_RunCompute(zest_compute compute);
ZEST_API void zest_WaitForCompute(zest_compute compute);
ZEST_API void zest_ComputeAttachRenderTarget(zest_compute compute, zest_render_target render_target);
ZEST_API void zest_MakeCompute(zest_compute_builder_t *builder, zest_compute compute);
ZEST_API void zest_StandardComputeDescriptorUpdate(zest_compute compute);
//--End Compute shaders

//Events and States
ZEST_API zest_bool zest_SwapchainWasRecreated(void);
ZEST_API void zest_SetupFontTexture(zest_font font);
ZEST_API zest_font zest_AddFont(zest_font font);
ZEST_API zest_font zest_GetFont(const char *font);
//--End Events and States

//Timer functions
ZEST_API zest_timer zest_CreateTimer(double update_frequency);									//Create a new timer and return its handle
ZEST_API void zest_FreeTimer(zest_timer timer);													//Free a timer and its memory
ZEST_API void zest_TimerSetUpdateFrequency(zest_timer timer, double update_frequency);			//Set the update frequency for timing loop functions, accumulators and such
ZEST_API void zest_TimerReset(zest_timer timer);												//Set the clock time to now
ZEST_API double zest_TimerDeltaTime(zest_timer timer);											//The amount of time passed since the last tick
ZEST_API void zest_TimerTick(zest_timer timer);													//Update the delta time
ZEST_API double zest_TimerUpdateTime(zest_timer timer);											//Gets the update time (1.f / update_frequency)
ZEST_API double zest_TimerFrameLength(zest_timer timer);										//Gets the update_tick_length (1000.f / update_frequency)
ZEST_API double zest_TimerAccumulate(zest_timer timer);											//Accumulate the amount of time that has passed since the last render
ZEST_API void zest_TimerUnAccumulate(zest_timer timer);											//Unaccumulate 1 tick length from the accumulator. While the accumulator is more then the tick length an update should be done
ZEST_API zest_bool zest_TimerDoUpdate(zest_timer timer);										//Return true if accumulator is more or equal to the update_tick_length
ZEST_API double zest_TimerLerp(zest_timer timer);												//Return the current tween/lerp value
ZEST_API void zest_TimerSet(zest_timer timer);													//Set the current tween value
//--End Timer Functions

//General Helper functions
ZEST_API char* zest_ReadEntireFile(const char *file_name, zest_bool terminate);
ZEST_API zest_camera_t zest_CreateCamera(void);
ZEST_API zest_render_pass zest_GetRenderPass(const char *name);
ZEST_API VkRenderPass zest_GetStandardRenderPass(void);
ZEST_API zest_pipeline zest_CreatePipeline(void);
ZEST_API VkFramebuffer zest_GetRendererFrameBuffer(zest_command_queue_draw_commands item);
ZEST_API zest_descriptor_set_layout zest_GetDescriptorSetLayout(const char *name);
ZEST_API void *zest_GetUniformBufferData(zest_uniform_buffer uniform_buffer);
ZEST_API void *zest_GetUniformBufferDataFIF(zest_uniform_buffer uniform_buffer, zest_index fif);
ZEST_API VkDescriptorBufferInfo *zest_GetUniformBufferInfo(const char *name, zest_index fif);
ZEST_API zest_pipeline_template_create_info_t zest_PipelineCreateInfo(const char *name);
ZEST_API VkExtent2D zest_GetSwapChainExtent(void);
ZEST_API VkExtent2D zest_GetWindowExtent(void);
ZEST_API zest_uint zest_SwapChainWidth(void);
ZEST_API zest_uint zest_SwapChainHeight(void);
ZEST_API float zest_SwapChainWidthf(void);
ZEST_API float zest_SwapChainHeightf(void);
ZEST_API zest_uint zest_ScreenWidth(void);
ZEST_API zest_uint zest_ScreenHeight(void);
ZEST_API float zest_ScreenWidthf(void);
ZEST_API float zest_ScreenHeightf(void);
ZEST_API float zest_DPIScale(void);
ZEST_API void zest_SetDPIScale(float scale);
ZEST_API zest_uint zest_FPS();
ZEST_API float zest_FPSf();
ZEST_API zest_descriptor_buffer zest_GetUniformBuffer(const char *name);
ZEST_API zest_bool zest_UniformBufferExists(const char *name);
ZEST_API void zest_WaitForIdleDevice(void);
ZEST_API void zest_MaybeQuit(zest_bool condition);
ZEST_API void zest_BindVertexBuffer(zest_buffer_t *buffer);
ZEST_API void zest_BindIndexBuffer(zest_buffer_t *buffer);
ZEST_API void zest_SendPushConstants(zest_pipeline_t *pipeline_layout, VkShaderStageFlags shader_flags, zest_uint size, void *data);
ZEST_API void zest_SendStandardPushConstants(zest_pipeline_t *pipeline_layout, void *data);
ZEST_API void zest_Draw(zest_uint vertex_count, zest_uint instance_count, zest_uint first_vertex, zest_uint first_instance);
ZEST_API void zest_DrawIndexed(zest_uint index_count, zest_uint instance_count, zest_uint first_index, int32_t vertex_offset, zest_uint first_instance);
ZEST_API void zest_ComputeToVertexBarrier();
ZEST_API void zest_DispatchCompute(zest_compute compute, zest_uint group_count_x, zest_uint group_count_y, zest_uint group_count_z);
ZEST_API void zest_EnableVSync();
ZEST_API void zest_DisnableVSync();
ZEST_API void zest_LogFPSToConsole(zest_bool yesno);
//--End General Helper functions

//Debug Helpers
ZEST_API const char *zest_DrawFunctionToString(void *function);
ZEST_API void zest_OutputQueues();
//--End Debug Helpers

#ifdef __cplusplus
}
#endif

#endif // ! ZEST_POCKET_RENDERER
