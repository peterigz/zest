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
#define ZEST_ENABLE_VALIDATION_LAYER 1
#endif

//Helper macros
#define ZEST__MIN(a, b) (((a) < (b)) ? (a) : (b))
#define ZEST__MAX(a, b) (((a) > (b)) ? (a) : (b))
#define ZEST__CLAMP(v, min_v, max_v) ((v < min_v) ? min_v : ((v > max_v) ? max_v : v))
#define ZEST__POW2(x) ((x) && !((x) & ((x) - 1)))
#define ZEST__FLAG(flag, bit) flag |= (bit)
#define ZEST__MAYBE_FLAG(flag, bit, yesno) flag |= (yesno ? bit : 0)
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
#define ZLOC_THREAD_SAFE
#define ZLOC_EXTRA_DEBUGGING
#define ZLOC_OUTPUT_ERROR_MESSAGES
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
//FIF = Frame in Flight
#define ZEST_FIF ZestDevice->current_fif
#define ZEST_MICROSECS_SECOND 1000000ULL
#define ZEST_MICROSECS_MILLISECOND 1000
#define ZEST_SECONDS_IN_MICROSECONDS(seconds) seconds * ZEST_MICROSECS_SECOND
#define ZEST_SECONDS_IN_MILLISECONDS(seconds) seconds * 1000

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
typedef zest_uint zest_thread_access;
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
ZEST_PRIVATE inline zest_thread_access zest__compare_and_exchange(volatile zest_thread_access* target, zest_thread_access value, zest_thread_access original) {
	return InterlockedCompareExchange(target, value, original);
}

//Window creation
ZEST_PRIVATE HINSTANCE zest_window_instance;
#define ZEST_WINDOW_INSTANCE HINSTANCE
LRESULT CALLBACK zest__window_proc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
//--

#elif defined(__GNUC__) && ((__GNUC__ > 4) || (__GNUC__ == 4 && __GNUC_MINOR__ >= 8)) && (defined(__i386__) || defined(__x86_64__)) || defined(__clang__)
#include <time.h>
//We'll just use glfw on mac for now. Can maybe add basic cocoa windows later
#include <glfw/glfw3.h>
#define ZEST_ALIGN_PREFIX(v) 
#define ZEST_ALIGN_AFFIX(v)  __attribute__((aligned(16)))
#define ZEST_PROTOTYPE void
#define zest_snprintf(buffer, bufferSize, format, ...) snprintf(buffer, bufferSize, format, __VA_ARGS__)
#define zest_strcat(left, size, right) strcat(left, right)
#define zest_strcpy(left, size, right) strcpy(left, right)
ZEST_PRIVATE inline zest_thread_access zest__compare_and_exchange(volatile zest_thread_access* target, zest_thread_access value, zest_thread_access original) {
	return __sync_val_compare_and_swap(target, original, value);
}
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
	zest_struct_type_texture = VK_STRUCTURE_TYPE_MAX_ENUM - 1,
	zest_struct_type_image = VK_STRUCTURE_TYPE_MAX_ENUM - 2,
} zest_struct_type;

typedef enum zest_app_flag_bits {
	zest_app_flag_none =					0,
	zest_app_flag_suspend_rendering =		1 << 0,
	zest_app_flag_shift_pressed =			1 << 1,
	zest_app_flag_control_pressed =			1 << 2,
	zest_app_flag_cmd_pressed =				1 << 3,
	zest_app_flag_record_input  =			1 << 4,
	zest_app_flag_enable_console =			1 << 5,
	zest_app_flag_quit_application =		1 << 6,
	zest_app_flag_output_fps =				1 << 7
} zest_app_flag_bits;

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

typedef enum zest_command_queue_flag_bits {
	zest_command_queue_flag_none								= 0,
	zest_command_queue_flag_present_dependency					= 1 << 0,
	zest_command_queue_flag_command_queue_dependency			= 1 << 1,
	zest_command_queue_flag_validated							= 1 << 2,
} zest_command_queue_flag_bits;

typedef zest_uint zest_command_queue_flags;

typedef enum zest_renderer_flag_bits {
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
} zest_renderer_flag_bits;

typedef zest_uint zest_renderer_flags;

typedef enum zest_pipeline_set_flag_bits {
	zest_pipeline_set_flag_none										= 0,	
	zest_pipeline_set_flag_is_render_target_pipeline				= 1 << 0,		//True if this pipeline is used for the final render of a render target to the swap chain
	zest_pipeline_set_flag_match_swapchain_view_extent_on_rebuild	= 1 << 1		//True if the pipeline should update it's view extent when the swap chain is recreated (the window is resized)
} zest_pipeline_set_flag_bits;

typedef zest_uint zest_pipeline_set_flags;

typedef enum zest_init_flag_bits {
	zest_init_flag_none									= 0,
	zest_init_flag_initialise_with_command_queue		= 1 << 0,
	zest_init_flag_use_depth_buffer						= 1 << 1,
	zest_init_flag_maximised							= 1 << 2,
	zest_init_flag_enable_vsync							= 1 << 6,
} zest_init_flag_bits;

typedef zest_uint zest_init_flags;

typedef enum zest_buffer_upload_flag_bits {
	zest_buffer_upload_flag_initialised					= 1 << 0,				//Set to true once AddCopyCommand has been run at least once
	zest_buffer_upload_flag_source_is_fif				= 1 << 1,
	zest_buffer_upload_flag_target_is_fif				= 1 << 2
} zest_buffer_upload_flag_bits;

typedef zest_uint zest_buffer_upload_flags;

typedef enum {
	zest_draw_mode_none = 0,			//Default no drawmode set when no drawing has been done yet	
	zest_draw_mode_instance,
	zest_draw_mode_images,
	zest_draw_mode_mesh,
	zest_draw_mode_line_instance,
	zest_draw_mode_rect_instance,
	zest_draw_mode_dashed_line,
	zest_draw_mode_text,
	zest_draw_mode_fill_screen,
	zest_draw_mode_viewport,
	zest_draw_mode_im_gui
} zest_draw_mode;

typedef enum {
	zest_shape_line = zest_draw_mode_line_instance,
	zest_shape_dashed_line = zest_draw_mode_dashed_line,
	zest_shape_rect = zest_draw_mode_rect_instance
} zest_shape_type;

typedef enum {
	zest_builtin_layer_sprites = 0,
	zest_builtin_layer_billboards,
	zest_builtin_layer_lines,
	zest_builtin_layer_fonts,
	zest_builtin_layer_mesh
} zest_builtin_layer_type;

typedef enum {
	zest_imgui_blendtype_none,				//Just draw with standard alpha blend
	zest_imgui_blendtype_pass,				//Force the alpha channel to 1
	zest_imgui_blendtype_premultiply		//Divide the color channels by the alpha channel
} zest_imgui_blendtype;

typedef enum zest_texture_flag_bits {
	zest_texture_flag_none								= 0,
	zest_texture_flag_premultiply_mode					= 1 << 0,
	zest_texture_flag_use_filtering						= 1 << 1,
	zest_texture_flag_get_max_radius					= 1 << 2,
	zest_texture_flag_textures_ready					= 1 << 3,
	zest_texture_flag_dirty								= 1 << 4,
	zest_texture_flag_descriptor_sets_created			= 1 << 5,
} zest_texture_flag_bits;

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

typedef enum zest_camera_flag_bits {
	zest_camera_flags_none = 0,
	zest_camera_flags_perspective						= 1 << 0,
	zest_camera_flags_orthagonal						= 1 << 1,
} zest_camera_flag_bits;

typedef zest_uint zest_camera_flags;

typedef enum zest_render_target_flag_bits {
	zest_render_target_flag_fixed_size					= 1 << 0,	//True if the render target is not tied to the size of the window
	zest_render_target_flag_is_src						= 1 << 1,	//True if the render target is the source of another render target - adds transfer src and dst bits
	zest_render_target_flag_use_msaa					= 1 << 2,	//True if the render target should render with MSAA enabled
	zest_render_target_flag_sampler_size_match_texture	= 1 << 3,
	zest_render_target_flag_single_frame_buffer_only	= 1 << 4,
	zest_render_target_flag_use_depth_buffer			= 1 << 5,
	zest_render_target_flag_render_to_swap_chain		= 1 << 6,
	zest_render_target_flag_initialised 				= 1 << 7,
	zest_render_target_flag_has_imgui_pipeline			= 1 << 8,
} zest_render_target_flag_bits;

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

typedef enum zest_character_flag_bits {
	zest_character_flag_none							= 0,
	zest_character_flag_skip							= 1 << 0,
	zest_character_flag_new_line						= 1 << 1,
} zest_character_flag_bits;

typedef zest_uint zest_character_flags;

typedef enum zest_compute_flag_bits {
	zest_compute_flag_none								= 0,
	zest_compute_flag_has_render_target					= 1,		// Compute shader uses a render target
	zest_compute_flag_rewrite_command_buffer			= 1 << 1,	// Command buffer for this compute shader should be rewritten
	zest_compute_flag_sync_required						= 1 << 2,	// Compute shader requires syncing with the render target
	zest_compute_flag_is_active							= 1 << 3,	// Compute shader is active then it will be updated when the swap chain is recreated
} zest_compute_flag_bits;

typedef enum zest_layer_flag_bits {
	zest_layer_flag_none								= 0,
	zest_layer_flag_static								= 1 << 0,	// Layer only uploads new buffer data when required
	zest_layer_flag_device_local_direct					= 1 << 1,	// Layer only uploads new buffer data when required
} zest_layer_flag_bits;

typedef zest_uint zest_compute_flags;
typedef zest_uint zest_layer_flags;

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
typedef struct zest_window_t zest_window_t;

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
ZEST__MAKE_HANDLE(zest_window)

typedef zest_descriptor_buffer zest_uniform_buffer;

// --Private structs with inline functions
typedef struct zest_queue_family_indices {
	zest_uint graphics_family;
	zest_uint graphics_family_queue_count;
	zest_uint present_family;
	zest_uint present_family_queue_count;
	zest_uint compute_family;
	zest_uint compute_family_queue_count;

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
#define zest_vec_bump(T) zest__vec_header(T)->current_size++
#define zest_vec_clip(T) zest__vec_header(T)->current_size--
#define zest_vec_trim(T, amount) zest__vec_header(T)->current_size -= amount;
#define zest_vec_grow(T) ((!(T) || (zest__vec_header(T)->current_size == zest__vec_header(T)->capacity)) ? T = zest__vec_reserve((T), sizeof(*T), (T ? zest__grow_capacity(T, zest__vec_header(T)->current_size) : 8)) : 0)
#define zest_vec_empty(T) (!T || zest__vec_header(T)->current_size == 0)
#define zest_vec_size(T) ((T) ? zest__vec_header(T)->current_size : 0)
#define zest_vec_last_index(T) (zest__vec_header(T)->current_size - 1)
#define zest_vec_capacity(T) (zest__vec_header(T)->capacity)
#define zest_vec_size_in_bytes(T) (zest__vec_header(T)->current_size * sizeof(*T))
#define zest_vec_front(T) (T[0])
#define zest_vec_back(T) (T[zest__vec_header(T)->current_size - 1])
#define zest_vec_end(T) (&(T[zest_vec_size(T)]))
#define zest_vec_clear(T) if(T) zest__vec_header(T)->current_size = 0
#define zest_vec_free(T) if(T) { ZEST__FREE(zest__vec_header(T)); T = ZEST_NULL;}
#define zest_vec_reserve(T, new_size) if(!T || zest__vec_header(T)->capacity < new_size) T = zest__vec_reserve(T, sizeof(*T), new_size == 1 ? 8 : new_size); 
#define zest_vec_resize(T, new_size) if(!T || zest__vec_header(T)->capacity < new_size) T = zest__vec_reserve(T, sizeof(*T), new_size == 1 ? 8 : new_size); zest__vec_header(T)->current_size = new_size
#define zest_vec_push(T, value) zest_vec_grow(T); (T)[zest__vec_header(T)->current_size++] = value 
#define zest_vec_pop(T) (zest__vec_header(T)->current_size--, T[zest__vec_header(T)->current_size])
#define zest_vec_insert(T, location, value) { ptrdiff_t offset = location - T; zest_vec_grow(T); if(offset < zest_vec_size(T)) memmove(T + offset + 1, T + offset, ((size_t)zest_vec_size(T) - offset) * sizeof(*T)); T[offset] = value; zest_vec_bump(T); }
#define zest_vec_erase(T, location) { ptrdiff_t offset = location - T; ZEST_ASSERT(T && offset >= 0 && location < zest_vec_end(T)); memmove(T + offset, T + offset + 1, ((size_t)zest_vec_size(T) - offset) * sizeof(*T)); zest_vec_clip(T); }
#define zest_vec_erase_range(T, it, it_last) { ZEST_ASSERT(T && it >= T && it < zest_vec_end(T)); const ptrdiff_t count = it_last - it; const ptrdiff_t off = it - T; memmove(T + off, T + off + count, ((size_t)zest_vec_size(T) - (size_t)off - count) * sizeof(*T)); zest_vec_trim(T, (zest_uint)count); }
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
typedef struct zest_buffer_type_t {
	VkDeviceSize alignment;
	zest_uint memory_type_index;
} zest_buffer_type_t;

typedef struct zest_buffer_info_t {
	VkImageUsageFlags image_usage_flags;
	VkBufferUsageFlags usage_flags;					//The usage state_flags of the memory block. 
	VkMemoryPropertyFlags property_flags;
	zest_buffer_flags flags;
	zest_uint memory_type_bits;
	VkDeviceSize alignment;
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
	zest_uint saved_fif;
	zest_uint max_image_size;
	zest_uint graphics_queue_family_index;
	zest_uint present_queue_family_index;
	zest_uint compute_queue_family_index;
	void *memory_pools[ZEST_MAX_DEVICE_MEMORY_POOLS];
	zest_size memory_pool_sizes[ZEST_MAX_DEVICE_MEMORY_POOLS];
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
	VkQueue graphics_queue;
	VkQueue one_time_graphics_queue;
	VkQueue compute_queue;
	VkCommandPool command_pool;
	VkCommandPool one_time_command_pool;
	PFN_vkSetDebugUtilsObjectNameEXT pfnSetDebugUtilsObjectNameEXT;
	VkFormat color_format;
	zest_map_buffer_pool_sizes pool_sizes;
	void *allocator_start;
	void *allocator_end;
} zest_device_t;

zest_hash_map(VkDescriptorPoolSize) zest_map_descriptor_pool_sizes;

typedef struct zest_create_info_t {
	const char *title;									//Title that shows in the window
	zest_size memory_pool_size;							//The size of each memory pool. More pools are added if needed
	int screen_width, screen_height;					//Default width and height of the window that you open
	int screen_x, screen_y;								//Default position of the window
	int virtual_width, virtual_height;					//The virtial width/height of the viewport
	VkFormat color_format;								//Choose between VK_FORMAT_R8G8B8A8_UNORM and VK_FORMAT_R8G8B8A8_SRGB
	zest_map_descriptor_pool_sizes pool_counts;			//You can define descriptor pool counts here using the zest_SetDescriptorPoolCount for each pool type. Defaults will be added for any not defined
	zest_uint max_descriptor_pool_sets;					//The maximum number of descriptor pool sets for the descriptor pool. 100 is default but set to more depending on your needs.
	zest_uint flags;									//Set flags to apply different initialisation options

	//Callbacks use these to implement your own preferred window creation functionality
	void(*get_window_size_callback)(void *user_data, int *fb_width, int *fb_height, int *window_width, int *window_height);
	void(*destroy_window_callback)(void *user_data);
	void(*poll_events_callback)(ZEST_PROTOTYPE);
	void(*add_platform_extensions_callback)(ZEST_PROTOTYPE);
	zest_window(*create_window_callback)(int x, int y, int width, int height, zest_bool maximised, const char* title);
	void(*create_window_surface_callback)(zest_window window);
} zest_create_info_t;

typedef struct zest_app_t {
	zest_create_info_t create_info;

	void(*update_callback)(zest_microsecs, void*);
	void *user_data;

	zest_window window;

	zest_microsecs current_elapsed;
	zest_microsecs current_elapsed_time;
	float update_time;
	float render_time;
	zest_microsecs frame_timer;

	double mouse_x;
	double mouse_y;
	double mouse_delta_x;
	double mouse_delta_y;
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
	zest_texture texture;
	zest_descriptor_buffer buffer;
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
	VkDynamicState *dynamicStates;
	zest_bool no_vertex_input;
	zest_text vertShaderFile;
	zest_text fragShaderFile;
	zest_text vertShaderFunctionName;
	zest_text fragShaderFunctionName;
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

	zest_text vertShaderFile;
	zest_text fragShaderFile;
} zest_pipeline_template_t;

//A pipeline set is all of the necessary things required to setup and maintain a pipeline
typedef struct zest_pipeline_t {
	zest_pipeline_template_create_info_t create_info;							//A copy of the create info and template is stored so that they can be used to update the pipeline later for any reason (like the swap chain is recreated)
	zest_pipeline_template_t pipeline_template;
	zest_descriptor_set_layout descriptor_layout;								//The descriptor layout being used which is stored in the Renderer. Layouts can be reused an shared between pipelines
	VkDescriptorSet descriptor_set[ZEST_MAX_FIF];								//Descriptor sets are only stored here for certain pipelines like non textured drawing or the final render pipelines for render targets in the swap chain
	VkPipeline pipeline;														//The vulkan handle for the pipeline
	VkPipelineLayout pipeline_layout;											//The vulkan handle for the pipeline layout
	zest_uniform_buffer uniform_buffer;											//Handle of the uniform buffer used in the pipline. Will be set to the default 2d uniform buffer if none is specified
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

//SDF Lines
typedef struct zest_shape_instance_t {
	zest_vec4 rect;						//The rectangle containing the sdf shade, x,y = top left, z,w = bottom right
	zest_vec4 parameters;				//Extra parameters, for example line widths, roundness, depending on the shape type
	zest_color start_color;				//The color tint of the first point in the line
	zest_color end_color;				//The color tint of the second point in the line
} zest_shape_instance_t;

typedef struct zest_vertex_t {
	zest_vec3 pos;						//3d position
	float intensity;					//Alpha level (can go over 1 to increase intensity of colors)
	zest_vec2 uv;						//Texture coordinates
	zest_color color;					//packed color
	zest_uint parameters;				//packed parameters such as texture layer
} zest_vertex_t;

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
			zest_buffer staging_vertex_data;
			zest_buffer staging_index_data;
		};
		struct {
			zest_buffer staging_instance_data;
			zest_buffer device_instance_data;
		};
	};

	union {
		struct { void *instance_ptr; };
		struct { void *vertex_ptr; };
	};

	zest_uint *index_ptr;

	zest_buffer device_index_data;
	zest_buffer device_vertex_data;
	zest_buffer write_to_buffer;

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
} zest_push_constants_t ZEST_ALIGN_AFFIX(16);

typedef struct zest_layer_instruction_t {
	zest_index start_index;						//The starting index
	union {
		zest_uint total_instances;				//The total number of instances to be drawn in the draw instruction
		zest_uint total_indexes;				//The total number of indexes to be drawn in the draw instruction
	};
	zest_index last_instance;					//The last instance that was drawn in the previous instance instruction
	zest_pipeline pipeline;						//The pipeline index to draw the instances. 
	VkDescriptorSet descriptor_set; 			//The descriptor set used to draw the quads.
	zest_push_constants_t push_constants;		//Each draw instruction can have different values in the push constants push_constants
	VkRect2D scissor;							//The drawinstruction can also clip whats drawn
	VkViewport viewport;						//The viewport size of the draw call 
	zest_draw_mode draw_mode;
	void *asset;								//Optional pointer to either texture, font etc
} zest_layer_instruction_t;

typedef struct zest_layer_t {

	const char *name;

	zest_layer_buffers_t memory_refs[ZEST_MAX_FIF];
	zest_bool dirty[ZEST_MAX_FIF];
	zest_uint initial_instance_pool_size;
	zest_command_queue_draw_commands draw_commands;
	zest_buffer_uploader_t vertex_upload;
	zest_buffer_uploader_t index_upload;

	zest_index sprite_pipeline_index;

	zest_layer_instruction_t current_instruction;

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

	zest_layer_instruction_t *draw_instructions[ZEST_MAX_FIF];
	zest_draw_mode last_draw_mode;

	zest_draw_routine draw_routine;
	zest_layer_flags flags;
	zest_builtin_layer_type layer_type;
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

typedef struct zest_compute_push_constant_t {
	//z is reserved for the quad_count;
	zest_vec4 parameters;
} zest_compute_push_constant_t;

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
	zest_compute_push_constant_t push_constants;
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
	zest_index shader_index;
	const char *name;
};

zest_hash_map(zest_descriptor_set) zest_map_texture_descriptor_sets;
zest_hash_map(zest_descriptor_set_builder_t) zest_map_texture_descriptor_builders;

typedef struct zest_bitmap_t {
	int width;
	int height;
	int channels;
	int stride;
	zest_text name;
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
	zest_struct_type struct_type;
	zest_index index;			//index within the QulkanTexture
	zest_text name;				//Filename of the image
	zest_uint width;
	zest_uint height;
	zest_vec4 uv;				//UV coords are set after the ProcessImages function is called and the images are packed into the texture
	zest_uint uv_xy;
	zest_uint uv_zw;
	zest_index layer;			//the layer index of the image when it's packed into an image/texture array
	zest_uint frames;			//Will be one if this is a single image or more if it's part of and animation
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
	zest_thread_access lock;
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
	zest_thread_access lock;
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

	zest_map_buffer_allocators buffer_allocators;

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
	zest_render_target *render_target_recreate_queue[ZEST_MAX_FIF];
	zest_render_target *rt_sampler_refresh_queue[ZEST_MAX_FIF];
	zest_texture *texture_refresh_queue[ZEST_MAX_FIF];
	zest_texture *texture_reprocess_queue;
	zest_texture *texture_delete_queue;
	zest_uint current_frame;

	//Flags
	zest_renderer_flags flags;

	//Callbacks for customising window and surface creation
	void(*get_window_size_callback)(void *user_data, int *fb_width, int *fb_height, int *window_width, int *window_height);
	void(*destroy_window_callback)(void *user_data);
	void(*poll_events_callback)(ZEST_PROTOTYPE);
	void(*add_platform_extensions_callback)(ZEST_PROTOTYPE);
	zest_window(*create_window_callback)(int x, int y, int width, int height, zest_bool maximised, const char* title);
	void(*create_window_surface_callback)(zest_window window);

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
ZEST_PRIVATE zest_window zest__os_create_window(int x, int y, int width, int height, zest_bool maximised, const char* title);
ZEST_PRIVATE void zest__os_create_window_surface(zest_window window);
ZEST_PRIVATE void zest__os_poll_events(ZEST_PROTOTYPE);
ZEST_PRIVATE void zest__os_add_platform_extensions(ZEST_PROTOTYPE);
ZEST_PRIVATE void zest__os_set_window_title(const char *title);
//-- End Platform dependent functions

//Only available outsie lib for some implementations like SDL2
ZEST_API void* zest__vec_reserve(void *T, zest_uint unit_size, zest_uint new_capacity);

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
ZEST_PRIVATE zest_device_memory_pool zest__create_vk_memory_pool(zest_buffer_info_t *buffer_info, VkImage image, zest_key key, zest_size size);
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
ZEST_PRIVATE void zest__create_descriptor_pools(zest_map_descriptor_pool_sizes pool_sizes, zest_uint max_sets);
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
ZEST_PRIVATE zest_command_queue_draw_commands zest__create_command_queue_draw_commands(const char *name);
ZEST_PRIVATE void zest__update_command_queue_viewports(void);
// --End Command Queue functions

// --Command Queue Setup functions
ZEST_PRIVATE zest_command_queue zest__create_command_queue(const char *name);
ZEST_PRIVATE void zest__set_queue_context(zest_setup_context_type context);
ZEST_PRIVATE zest_draw_routine zest__create_draw_routine_with_builtin_layer(const char *name, zest_builtin_layer_type builtin_layer);

// --Draw layer internal functions
ZEST_PRIVATE void zest__start_mesh_instructions(zest_layer instance_layer);
ZEST_PRIVATE void zest__end_mesh_instructions(zest_layer instance_layer);
ZEST_PRIVATE void zest__update_instance_layer_buffers_callback(zest_draw_routine draw_routine, VkCommandBuffer command_buffer);
ZEST_PRIVATE void zest__update_instance_layer_resolution(zest_layer layer);
ZEST_PRIVATE void zest__draw_mesh_layer(zest_layer layer, VkCommandBuffer command_buffer);
ZEST_PRIVATE zest_layer_instruction_t zest__layer_instruction(void);
ZEST_PRIVATE void zest__reset_mesh_layer_drawing(zest_layer layer);

// --Texture internal functions
ZEST_PRIVATE zest_index zest__texture_image_index(zest_texture texture);
ZEST_PRIVATE float zest__copy_animation_frames(zest_texture texture, zest_bitmap_t *spritesheet, int width, int height, zest_uint frames, zest_bool row_by_row);
ZEST_PRIVATE void zest__delete_texture_layers(zest_texture texture);
ZEST_PRIVATE void zest__update_all_texture_descriptor_writes(zest_texture texture);
ZEST_PRIVATE void zest__create_texture_image(zest_texture texture, zest_uint mip_levels, VkImageUsageFlags usage_flags, VkImageLayout image_layout, zest_bool copy_bitmap);
ZEST_PRIVATE void zest__create_texture_image_array(zest_texture texture, zest_uint mip_levels);
ZEST_PRIVATE void zest__create_texture_stream(zest_texture texture, zest_uint mip_levels, VkImageUsageFlags usage_flags, VkImageLayout image_layout, zest_bool copy_bitmap);
ZEST_PRIVATE void zest__create_texture_sampler(zest_texture texture, VkSamplerCreateInfo samplerInfo, zest_uint mip_levels);
ZEST_PRIVATE zest_byte zest__calculate_texture_layers(stbrp_rect *rects, zest_uint size, const zest_uint node_count);
ZEST_PRIVATE void zest__make_image_bank(zest_texture texture, zest_uint size);
ZEST_PRIVATE void zest__make_sprite_sheet(zest_texture texture);
ZEST_PRIVATE void zest__pack_images(zest_texture texture, zest_uint size);
ZEST_PRIVATE void zest__update_image_vertices(zest_image image);
ZEST_PRIVATE void zest__update_texture_single_image_meta(zest_texture texture, zest_uint width, zest_uint height);
ZEST_PRIVATE void zest__create_texture_image_view(zest_texture texture, VkImageViewType view_type, zest_uint mip_levels, zest_uint layer_count);
ZEST_PRIVATE void zest__update_all_texture_descriptor_sets(zest_texture texture);

// --Render target internal functions
ZEST_PRIVATE void zest__initialise_render_target(zest_render_target render_target, zest_render_target_create_info_t *info);
ZEST_PRIVATE void zest__create_render_target_sampler_image(zest_render_target render_target);
ZEST_PRIVATE void zest__refresh_render_target_sampler(zest_render_target render_target, zest_index fif);

// --Sprite layer internal functions
ZEST_PRIVATE void zest__draw_sprite_layer_callback(zest_draw_routine draw_routine, VkCommandBuffer command_buffer);
ZEST_PRIVATE void zest__next_sprite_instance(zest_layer layer);
ZEST_PRIVATE void zest__initialise_sprite_layer(zest_layer sprite_layer, zest_uint instance_pool_size);

// --Font layer internal functions
ZEST_PRIVATE void zest__draw_font_layer_callback(zest_draw_routine draw_routine, VkCommandBuffer command_buffer);
ZEST_PRIVATE void zest__next_font_instance(zest_layer layer);
ZEST_PRIVATE void zest__initialise_font_layer(zest_layer font_layer, zest_uint instance_pool_size);
ZEST_PRIVATE void zest__setup_font_texture(zest_font font);
ZEST_PRIVATE zest_font zest__add_font(zest_font font);

// --Billboard layer internal functions
ZEST_PRIVATE void zest__draw_billboard_layer_callback(zest_draw_routine draw_routine, VkCommandBuffer command_buffer);
ZEST_PRIVATE void zest__next_billboard_instance(zest_layer layer);
ZEST_PRIVATE void zest__initialise_billboard_layer(zest_layer billboard_layer, zest_uint instance_pool_size);

// --Line instance layer internal functions
ZEST_PRIVATE void zest__draw_shape_layer_callback(zest_draw_routine draw_routine, VkCommandBuffer command_buffer);
ZEST_PRIVATE void zest__next_shape_instance(zest_layer layer);
ZEST_PRIVATE void zest__initialise_shape_layer(zest_layer line_layer, zest_uint instance_pool_size);

// --Mesh layer internal functions
ZEST_PRIVATE void zest__draw_mesh_layer_callback(zest_draw_routine draw_routine, VkCommandBuffer command_buffer);
ZEST_PRIVATE void zest__initialise_mesh_layer(zest_layer mesh_layer, zest_size vertex_struct_size, zest_size initial_vertex_capacity);

// --General Helper Functions
ZEST_PRIVATE VkImageView zest__create_image_view(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags, zest_uint mipLevels, VkImageViewType viewType, zest_uint layerCount);
ZEST_PRIVATE void zest__create_temporary_image(zest_uint width, zest_uint height, zest_uint mipLevels, VkSampleCountFlagBits numSamples, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkImage *image, VkDeviceMemory *memory);
ZEST_PRIVATE zest_buffer zest__create_image(zest_uint width, zest_uint height, zest_uint mipLevels, VkSampleCountFlagBits numSamples, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkImage *image);
ZEST_PRIVATE zest_buffer zest__create_image_array(zest_uint width, zest_uint height, zest_uint mipLevels, zest_uint layers, VkSampleCountFlagBits numSamples, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkImage *image);
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

// --Pipeline Helper Functions
ZEST_PRIVATE void zest__set_pipeline_template(zest_pipeline_template_t *pipeline_template, zest_pipeline_template_create_info_t *create_info);
ZEST_PRIVATE void zest__update_pipeline_template(zest_pipeline_template_t *pipeline_template, zest_pipeline_template_create_info_t *create_info);
ZEST_PRIVATE VkShaderModule zest__create_shader_module(char *code);
ZEST_PRIVATE zest_pipeline_template_create_info_t zest__copy_pipeline_create_info(zest_pipeline_template_create_info_t *create_info);
ZEST_PRIVATE void zest__free_pipeline_create_info(zest_pipeline_template_create_info_t *create_info);
ZEST_PRIVATE zest_pipeline zest__create_pipeline(void);
ZEST_PRIVATE void zest__add_pipeline_descriptor_write(zest_pipeline pipeline, VkWriteDescriptorSet set, zest_index fif);
// --End Pipeline Helper Functions

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
ZEST_PRIVATE void zest__reindex_texture_images(zest_texture texture);
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
ZEST_PRIVATE inline void zest__set_graphics_family(zest_queue_family_indices *family, zest_uint v, zest_uint queue_count) { family->graphics_family = v; family->graphics_set = 1; family->graphics_family_queue_count = queue_count; }
ZEST_PRIVATE inline void zest__set_present_family(zest_queue_family_indices *family, zest_uint v, zest_uint queue_count) { family->present_family = v; family->present_set = 1; family->present_family_queue_count = queue_count; }
ZEST_PRIVATE inline void zest__set_compute_family(zest_queue_family_indices *family, zest_uint v, zest_uint queue_count) { family->compute_family = v; family->compute_set = 1; family->compute_family_queue_count = queue_count; }
ZEST_PRIVATE inline zest_bool zest__family_is_complete(zest_queue_family_indices *family) { return family->graphics_set && family->present_set && family->compute_set; }
ZEST_PRIVATE zest_bool zest__check_device_extension_support(VkPhysicalDevice physical_device);
ZEST_PRIVATE zest_swapchain_support_details_t zest__query_swapchain_support(VkPhysicalDevice physical_device);
ZEST_PRIVATE VkSampleCountFlagBits zest__get_max_useable_sample_count(void);
ZEST_PRIVATE void zest__create_logical_device(void);
ZEST_PRIVATE void zest__set_limit_data(void);
ZEST_PRIVATE zest_bool zest__check_validation_layer_support(void);
ZEST_PRIVATE void zest__get_required_extensions(void);
ZEST_PRIVATE zest_uint zest_find_memory_type(zest_uint typeFilter, VkMemoryPropertyFlags properties);
ZEST_PRIVATE zest_buffer_type_t zest__get_buffer_memory_type(VkBufferUsageFlags usage_flags, VkMemoryPropertyFlags property_flags, zest_size size);
ZEST_PRIVATE void zest__set_default_pool_sizes(void);
ZEST_PRIVATE void *zest_vk_allocate_callback(void* pUserData, size_t size, size_t alignment, VkSystemAllocationScope allocationScope);
ZEST_PRIVATE void *zest_vk_reallocate_callback(void* pUserData, void *memory, size_t size, size_t alignment, VkSystemAllocationScope allocationScope);
ZEST_PRIVATE void zest_vk_free_callback(void* pUserData, void *memory);
//end device setup functions

//App initialise/run functions
ZEST_PRIVATE void zest__do_scheduled_tasks(void);
ZEST_PRIVATE void zest__initialise_app(zest_create_info_t *create_info);
ZEST_PRIVATE void zest__initialise_device(void);
ZEST_PRIVATE void zest__destroy(void);
ZEST_PRIVATE void zest__main_loop(void);
ZEST_PRIVATE zest_microsecs zest__set_elapsed_time(void);
//-- end of internal functions

//-- Window related functions
ZEST_PRIVATE void zest__update_window_size(zest_window window, zest_uint width, zest_uint height);
//-- End Window related functions

//User API functions

//-----------------------------------------------
//		Essential setup functions
//-----------------------------------------------
//Create a new zest_create_info_t struct with default values for initialising Zest
ZEST_API zest_create_info_t zest_CreateInfo(void);
//Set the pool count for a descriptor type in a zest_create_info_t. Do this for any pools that you want to be different to the default. You only
//need to call this if you're running out of descriptor pool space for a specific VkDescriptorType which you maybe doing if you have a lot of custom
//draw routines
ZEST_API void SetDescriptorPoolCount(zest_create_info_t *info, VkDescriptorType descriptor_type, zest_uint count);
//Initialise Zest. You must call this in order to use Zest. Use zest_CreateInfo() to set up some default values to initialise the renderer.
ZEST_API void zest_Initialise(zest_create_info_t *info);
//Set the custom user data which will get passed through to the user update function each frame.
ZEST_API void zest_SetUserData(void* data);
//Set the user udpate callback that will be called each frame in the main loop of zest. You must set this or the main loop will just render a blank screen.
ZEST_API void zest_SetUserUpdateCallback(void(*callback)(zest_microsecs, void*));
//Start the main loop in the zest renderer. Must be run after zest_Initialise and also zest_SetUserUpdateCallback
ZEST_API void zest_Start(void);

//-----------------------------------------------
//		Vulkan Helper Functions
//		These functions are for more advanced customisation of the render where more knowledge of how Vulkan works is required.
//-----------------------------------------------
//Add a Vulkan instance extension. You don't really need to worry about this function unless you're looking to customise the render with some specific extensions
ZEST_API void zest_AddInstanceExtension(char *extension);
//Allocate space in memory for a zest_window_t which contains data about the window. If you're using your own method for creating a window then you can use
//this and then assign your window handle to zest_window_t.window_handle. Returns a pointer to the zest_window_t
ZEST_API zest_window zest_AllocateWindow(void);
//Add a Vulkan descriptor layout which you can use to setup shaders. Zest adds a few builtin layouts for sprite and billboard drawing.
//Just pass in a name for the layout and the VkDescriptorSetLayout vulkan struct
//You can combine this with zest_CreateDescriptorSetLayout or just pass in your own VkDescriptorLayout struct
ZEST_API zest_descriptor_set_layout zest_AddDescriptorLayout(const char *name, VkDescriptorSetLayout layout);
//Create a vulkan descriptor set layout for use in shaders. This is a helper function, just pass in the number of uniforms, texture samplers and storage buffers
//that you want the layout to have and it returns a VkDescriptorSetLayout struct setup with those values and appropriate bindings.
ZEST_API VkDescriptorSetLayout zest_CreateDescriptorSetLayout(zest_uint uniforms, zest_uint samplers, zest_uint storage_buffers);
//Create a vulkan descriptor layout binding for use in setting up a descriptor set layout. This is a more general function for setting up whichever layout binding
//you need. Just pass in the VkDescriptorType, VkShaderStageFlags, the binding number (which will correspond to the binding in the shader, and the number of descriptors
ZEST_API VkDescriptorSetLayoutBinding zest_CreateDescriptorLayoutBinding(VkDescriptorType type, VkShaderStageFlags stageFlags, zest_uint binding, zest_uint descriptorCount);
//Create a vulkan descriptor layout binding specifically for uniform buffers. Just pass in the binding number that corresponds to the binding in the shader
ZEST_API VkDescriptorSetLayoutBinding zest_CreateUniformLayoutBinding(zest_uint binding);
//Create a vulkan descriptor layout binding specifically for texture samplers. Just pass in the binding number that corresponds to the binding in the shader
ZEST_API VkDescriptorSetLayoutBinding zest_CreateSamplerLayoutBinding(zest_uint binding);
//Create a vulkan descriptor layout binding specifically for storage buffers, more generally used for compute shaders. Just pass in the binding number that corresponds to the binding in the shader
ZEST_API VkDescriptorSetLayoutBinding zest_CreateStorageLayoutBinding(zest_uint binding);
//Create a vulkan descriptor set layout with bindings. Just pass in an array of bindings and a count of how many bindings there are.
ZEST_API VkDescriptorSetLayout zest_CreateDescriptorSetLayoutWithBindings(zest_uint count, VkDescriptorSetLayoutBinding *bindings);
//Create a vulkan descriptor write for a buffer. You need to pass the VkDescriptorSet 
ZEST_API VkWriteDescriptorSet zest_CreateBufferDescriptorWriteWithType(VkDescriptorSet descriptor_set, VkDescriptorBufferInfo *view_buffer_info, zest_uint dst_binding, VkDescriptorType type);
//Create a vulkan descriptor write for an image.
ZEST_API VkWriteDescriptorSet zest_CreateImageDescriptorWriteWithType(VkDescriptorSet descriptor_set, VkDescriptorImageInfo *view_buffer_info, zest_uint dst_binding, VkDescriptorType type);
//To make creating a new VkDescriptorSet you can use a builder. Make sure you call this function to properly initialise the zest_descriptor_set_builder_t
//Once you have a builder, you can call the Add commands to add image and buffer bindings then call zest_BuildDescriptorSet to create the descriptor set
ZEST_API zest_descriptor_set_builder_t zest_NewDescriptorSetBuilder();
//Add a VkDescriptorImageInfo from a zest_texture (or render target) to a descriptor set builder.
ZEST_API void zest_AddBuilderDescriptorWriteImage(zest_descriptor_set_builder_t *builder, VkDescriptorImageInfo *view_image_info, zest_uint dst_binding, VkDescriptorType type);
//Add a VkDescriptorBufferInfo from a zest_descriptor_buffer to a descriptor set builder.
ZEST_API void zest_AddBuilderDescriptorWriteBuffer(zest_descriptor_set_builder_t *builder, VkDescriptorBufferInfo *view_buffer_info, zest_uint dst_binding, VkDescriptorType type);
//Add an array of VkDescriptorImageInfos to a descriptor set builder.
ZEST_API void zest_AddBuilderDescriptorWriteImages(zest_descriptor_set_builder_t *builder, zest_uint image_count, VkDescriptorImageInfo *view_image_info, zest_uint dst_binding, VkDescriptorType type);
//Add an array of VkDescriptorBufferInfos to a descriptor set builder.
ZEST_API void zest_AddBuilderDescriptorWriteBuffers(zest_descriptor_set_builder_t *builder, zest_uint buffer_count, VkDescriptorBufferInfo *view_buffer_info, zest_uint dst_binding, VkDescriptorType type);
//Build a zest_descriptor_set_t using a builder that you made using the AddBuilder command. The layout that you pass to this function must be configured properly.
//zest_descriptor_set_t will contain a VkDescriptorSet for each frame in flight as well as descriptor writes used to create the set.
ZEST_API zest_descriptor_set_t zest_BuildDescriptorSet(zest_descriptor_set_builder_t *builder, zest_descriptor_set_layout layout);
//Allocate a descriptor set from a descriptor pool. The VkDescriptorPool that you pass in must have enough space to make the allocation. You can pass in the main pool from the render found at:
//ZestRenderer->descriptor_pool which you can define the size of at initialisation using the pool_counts using zest_SetDescriptorPoolCount.
ZEST_API void zest_AllocateDescriptorSet(VkDescriptorPool descriptor_pool, VkDescriptorSetLayout descriptor_layout, VkDescriptorSet *descriptor_set);
//Update a VkDescriptorSet with an array of descriptor writes. For when the images/buffers in a descriptor set have changed, the corresponding descriptor set will need to be updated.
ZEST_API void zest_UpdateDescriptorSet(VkWriteDescriptorSet *descriptor_writes);
//Create a VkViewport, generally used when building a render pass.
ZEST_API VkViewport zest_CreateViewport(float x, float y, float width, float height, float minDepth, float maxDepth);
//Create a VkRect2D, generally used when building a render pass.
ZEST_API VkRect2D zest_CreateRect2D(zest_uint width, zest_uint height, int offsetX, int offsetY);

//-----------------------------------------------
//		Pipeline related vulkan helpers
//		Pipelines are essential to drawing things on screen. There are some builtin pipelines that Zest creates
//		for sprite/billboard/mesh/font drawing. You can take a look in zest__prepare_standard_pipelines to see how
//		the following functions are utilised, plus look at the exmaples for building your own custom pipelines.
//-----------------------------------------------
//Add a new pipeline to the renderer and return its handle.
ZEST_API zest_pipeline zest_AddPipeline(const char *name);
//When building a new pipeline, use a zest_pipeline_template_create_info_t struct to do this and make it a lot easier.
//Call this function to create a base version with some default values, and overwrite those values depending on what
//you need for the pipeline. Once you have configured the template create info you can then call zest_MakePipelineTemplate
//to configure the pipeline and then you can do some further tweaks of the zest_pipeline before calling zest_BuildPipeline
//to finalise the pipeline ready for using
ZEST_API zest_pipeline_template_create_info_t zest_CreatePipelineTemplateCreateInfo(void);
//Set the name of the file to use for the vert and frag shader in the zest_pipeline_template_create_info_t
ZEST_API void zest_SetPipelineTemplateVertShader(zest_pipeline_template_create_info_t *create_info, const char *file);
ZEST_API void zest_SetPipelineTemplateFragShader(zest_pipeline_template_create_info_t *create_info, const char *file);
//Set the name of both the fragment and vertex shader to the same file (frag and vertex shaders can be combined into the same spv)
ZEST_API void zest_SetPipelineTemplateShader(zest_pipeline_template_create_info_t *create_info, const char *file);
//Add a new VkVertexInputBindingDescription which is used to set the size of the struct (stride) and the vertex input rate.
//You can add as many bindings as you need, just make sure you set the correct binding index for each one
ZEST_API VkVertexInputBindingDescription zest_AddVertexInputBindingDescription(zest_pipeline_template_create_info_t *create_info, zest_uint binding, zest_uint stride, VkVertexInputRate input_rate);
//Clear the input binding descriptions from the zest_pipeline_template_create_info_t bindingDescriptions array.
ZEST_API void zest_ClearVertexInputBindingDescriptions(zest_pipeline_template_create_info_t *create_info);
//Make a new array of vertex input descriptors for use with zest_AddVertexInputDescription. input descriptions are used
//to define the vertex input types such as position, color, uv coords etc., depending on what you need.
ZEST_API zest_vertex_input_descriptions zest_NewVertexInputDescriptions();
//Add a VkVertexInputeAttributeDescription to a zest_vertex_input_descriptions array. You can use zest_CreateVertexInputDescription
//helper function to create the description
ZEST_API void zest_AddVertexInputDescription(zest_vertex_input_descriptions *descriptions, VkVertexInputAttributeDescription description);
//Create a VkVertexInputAttributeDescription for adding to a zest_vertex_input_descriptions array. Just pass the binding and location in
//the shader, the VkFormat and the offset into the struct that you're using for the vertex data. See zest__prepare_standard_pipelines
//for examples of how the builtin pipelines do this
ZEST_API VkVertexInputAttributeDescription zest_CreateVertexInputDescription(zest_uint binding, zest_uint location, VkFormat format, zest_uint offset);
//Set up the push contant that you might plan to use in the pipeline. Just pass in the size of the push constant struct, the offset and the shader
//stage flags where the push constant will be used. Use this if you only want to set up a single push constant range
ZEST_API void zest_SetPipelineTemplatePushConstant(zest_pipeline_template_create_info_t *create_info, zest_uint size, zest_uint offset, VkShaderStageFlags stage_flags);
//If you have more then one push constant range you want to use with the pipeline then you can use this to add what you need. Just pass in
//the create info struct pointer and a VkPushConstantRange
ZEST_API void zest_AddPipelineTemplatePushConstantRange(zest_pipeline_template_create_info_t *create_info, VkPushConstantRange range);
//Set the uniform buffer that the pipeline should use. You must call zest_MakePipelineDescriptorWrites after setting the uniform buffer.
ZEST_API void zest_SetPipelineUniformBuffer(zest_pipeline pipeline, zest_uniform_buffer uniform_buffer);
//Make a pipeline template ready for building. Pass in the pipeline that you created with zest_AddPipeline, the render pass that you want to 
//use for the pipeline and the zest_pipeline_template_create_info_t you have setup to configure the pipeline. After you have called this
//function you can make a few more alterations to configure the pipeline further if needed before calling zest_BuildPipeline.
//NOTE: the create info that you pass into this function will get copied and then freed so don't use it after calling this function. If 
//you want to create another variation of this pipeline you're creating then you can call zest_CopyTemplateFromPipeline to create a new
//zest_pipeline_template_create_info_t and create another new pipeline with that
ZEST_API void zest_MakePipelineTemplate(zest_pipeline pipeline, VkRenderPass render_pass, zest_pipeline_template_create_info_t *create_info);
//Build the pipeline ready for use in your draw routines. This is the final step in building the pipeline.
ZEST_API void zest_BuildPipeline(zest_pipeline pipeline);
//Helper function to get the pipeline template from a pipeline. 
ZEST_API zest_pipeline_template_t *zest_PipelineTemplate(zest_pipeline pipeline);
//Get the shader stage flags for a specific push constant range in the pipeline
ZEST_API VkShaderStageFlags zest_PipelinePushConstantStageFlags(zest_pipeline pipeline, zest_uint index);
//Get the size of a push constant range for a specif index in the pipeline
ZEST_API zest_uint zest_PipelinePushConstantSize(zest_pipeline pipeline, zest_uint index);
//Get the offset of a push constant range for a specif index in the pipeline
ZEST_API zest_uint zest_PipelinePushConstantOffset(zest_pipeline pipeline, zest_uint index);
//The following are helper functions to set color blend attachment states for various blending setups
//Just take a look inside the functions for the values being used
ZEST_API VkPipelineColorBlendAttachmentState zest_AdditiveBlendState(void);
ZEST_API VkPipelineColorBlendAttachmentState zest_AdditiveBlendState2(void);
ZEST_API VkPipelineColorBlendAttachmentState zest_AlphaOnlyBlendState(void);
ZEST_API VkPipelineColorBlendAttachmentState zest_AlphaBlendState(void);
ZEST_API VkPipelineColorBlendAttachmentState zest_PreMultiplyBlendState(void);
ZEST_API VkPipelineColorBlendAttachmentState zest_PreMultiplyBlendStateForSwap(void);
ZEST_API VkPipelineColorBlendAttachmentState zest_MaxAlphaBlendState(void);
ZEST_API VkPipelineColorBlendAttachmentState zest_ImGuiBlendState(void);
//Bind a pipeline for use in a draw routing. Once you have built the pipeline at some point you will want to actually use it to draw things.
//In order to do that you can bind the pipeline using this function. Just pass in the pipeline handle and a VkDescriptorSet. Note that the 
//descriptor set must be compatible with the shaders that are being using in the pipeline. The command buffer used in the binding will be 
//whatever is defined in ZestRenderer->current_command_buffer which will be set when the command queue is recorded. If you need to specify
//a command buffer then call zest_BindPipelineCB instead.
ZEST_API void zest_BindPipeline(zest_pipeline pipeline, VkDescriptorSet descriptor_set);
//Does the same thing as zest_BindPipeline but you can also pass in a command buffer if you need to specify one.
ZEST_API void zest_BindPipelineCB(VkCommandBuffer command_buffer, zest_pipeline_t *pipeline, VkDescriptorSet descriptor_set);
//Retrieve a pipeline from the renderer storage. Just pass in the name of the pipeline you want to retrieve and the handle to the pipeline 
//will be returned.
ZEST_API zest_pipeline zest_Pipeline(const char *name);
//Copy the zest_pipeline_template_create_info_t from an existing pipeline. This can be useful if you want to create a new pipeline based
//on an existing pipeline with just a few tweaks like setting a different shader to use.
ZEST_API zest_pipeline_template_create_info_t zest_CopyTemplateFromPipeline(const char *pipeline_name);
//Get a pointer to the zest_pipeline_template_create_info_t saved in a pipeline. You use this to alter the pipeline and remake if you need to.
//Note: use zest_CopyTemplateFromPipeline to create a new pipeline based on another, don't use this function to try and make a copy.
ZEST_API zest_pipeline_template_create_info_t *zest_PipelineCreateInfo(const char *name);
//Make the descriptor writes for a pipeine and update the descriptor set
ZEST_API void zest_MakePipelineDescriptorWrites(zest_pipeline pipeline);
//-- End Pipeline related 

//--End vulkan helper functions

//Platform dependent callbacks
//Depending on the platform and method you're using to create a window and poll events callbacks are used to do those things. 
//You can define those callbacks with these functions
ZEST_API void zest_SetDestroyWindowCallback(void(*destroy_window_callback)(void *user_data));
ZEST_API void zest_SetGetWindowSizeCallback(void(*get_window_size_callback)(void *user_data, int *fb_width, int *fb_height, int *window_width, int *window_height));
ZEST_API void zest_SetPollEventsCallback(void(*poll_events_callback)(void));
ZEST_API void zest_SetPlatformExtensionsCallback(void(*add_platform_extensions_callback)(void));

//-----------------------------------------------
//		Buffer functions.
//		Use these functions to create memory buffers mainly on the GPU
//		but you can also create staging buffers that are cpu visible and
//		used to upload data to the GPU
//		All buffers are allocated from a memory pool. Generally each buffer type has it's own separate pool
//		depending on the memory requirements. You can define the size of memory pools before you initialise
//		Zest using zest_SetDeviceBufferPoolSize and zest_SetDeviceImagePoolsize. When you create a buffer, if
//		there is no memory left to allocate the buffer then it will try and create a new pool to allocate from.
//-----------------------------------------------
//Debug functions, will output info about the buffer to the console and also verify the integrity of host and device memory blocks
ZEST_API void zloc__output_buffer_info(void* ptr, size_t size, int free, void* user, int count);
ZEST_API zloc__error_codes zloc_VerifyRemoteBlocks(zloc_header *first_block, zloc__block_output output_function, void *user_data);
ZEST_API zloc__error_codes zloc_VerifyBlocks(zloc_header *first_block, zloc__block_output output_function, void *user_data);
ZEST_API zest_uint zloc_CountBlocks(zloc_header *first_block);
//---------
//Create a new buffer configured with the zest_buffer_info_t that you pass into the function. If you're creating a buffer to store
//and image then pass in the VkImage as well, otherwise just pass in 0.
//You can use helper functions to create commonly used buffer types such as zest_CreateVertexBufferInfo below, and you can just use
//helper functions to create the buffers without needed to create the zest_buffer_info_t, see functions just below this one.
ZEST_API zest_buffer zest_CreateBuffer(VkDeviceSize size, zest_buffer_info_t *buffer_info, VkImage image);
//Create a staging buffer which you can use to prep data for uploading to another buffer on the GPU
ZEST_API zest_buffer zest_CreateStagingBuffer(VkDeviceSize size, void *data);
//Create an index buffer.
ZEST_API zest_buffer zest_CreateIndexBuffer(VkDeviceSize size, zest_buffer staging_buffer);
ZEST_API zest_buffer zest_CreateVertexBuffer(VkDeviceSize size, zest_buffer staging_buffer);
//Create a general storage buffer mainly for use in a compute shader
ZEST_API zest_buffer zest_CreateStorageBuffer(VkDeviceSize size, zest_buffer staging_buffer);
//Create a vertex buffer that is flagged for storage so that you can use it in a compute shader
ZEST_API zest_buffer zest_CreateComputeVertexBuffer(VkDeviceSize size, zest_buffer staging_buffer);
//Create an index buffer that is flagged for storage so that you can use it in a compute shader
ZEST_API zest_buffer zest_CreateComputeIndexBuffer(VkDeviceSize size, zest_buffer staging_buffer);
//The following functions can be used to generate a zest_buffer_info_t with the corresponding buffer configuration to create buffers with
ZEST_API zest_buffer_info_t zest_CreateVertexBufferInfo(zest_bool cpu_visible);
ZEST_API zest_buffer_info_t zest_CreateStorageBufferInfo(void);
ZEST_API zest_buffer_info_t zest_CreateComputeVertexBufferInfo(void);
ZEST_API zest_buffer_info_t zest_CreateComputeIndexBufferInfo(void);
ZEST_API zest_buffer_info_t zest_CreateIndexBufferInfo(void);
ZEST_API zest_buffer_info_t zest_CreateStagingBufferInfo(void);
//Create descriptor buffers with the following functions. Descriptor buffers can be used when you want to bind them in a descriptor set
//for use in a shader. When you create a descriptor buffer it also creates the descriptor info which is necessary when creating the 
//descriptor set. Note that to create a uniform buffer which needs a descriptor info you can just call zest_CreateUniformBuffer. 
//Pass in the size and whether or not you want a buffer for each frame in flight. If you're writing to the buffer every frame then you
//we need a buffer for each frame in flight, otherwise if the buffer is being written too ahead of time and will be read only after that
//then you can pass ZEST_FALSE or 0 for all_frames_in_flight to just create a single buffer.
ZEST_API zest_descriptor_buffer zest_CreateDescriptorBuffer(zest_buffer_info_t *buffer_info, zest_size size, zest_bool all_frames_in_flight);
ZEST_API zest_descriptor_buffer zest_CreateStorageDescriptorBuffer(zest_size size, zest_bool all_frames_in_flight);
ZEST_API zest_descriptor_buffer zest_CreateCPUVisibleStorageDescriptorBuffer(zest_size size, zest_bool all_frames_in_flight);
ZEST_API zest_descriptor_buffer zest_CreateVertexDescriptorBuffer(zest_size size, zest_bool all_frames_in_flight);
ZEST_API zest_descriptor_buffer zest_CreateIndexDescriptorBuffer(zest_size size, zest_bool all_frames_in_flight);
ZEST_API zest_descriptor_buffer zest_CreateComputeVertexDescriptorBuffer(zest_size size, zest_bool all_frames_in_flight);
ZEST_API zest_descriptor_buffer zest_CreateComputeIndexDescriptorBuffer(zest_size size, zest_bool all_frames_in_flight);
//Use this function to get the zest_buffer from the zest_descriptor_buffer handle. If the buffer uses multiple frames in flight then it
//will retrieve the current frame in flight buffer which will be safe to write to.
ZEST_API zest_buffer zest_GetBufferFromDescriptorBuffer(zest_descriptor_buffer descriptor_buffer);
//Grow a buffer if the minium_bytes is more then the current buffer size.
ZEST_API zest_bool zest_GrowBuffer(zest_buffer *buffer, zest_size unit_size, zest_size minimum_bytes);
//Grow a descriptor buffer if the minium_bytes is more then the current buffer size.
ZEST_API zest_bool zest_GrowDescriptorBuffer(zest_descriptor_buffer buffer, zest_size unit_size, zest_size minimum_bytes);
//Copy a buffer to another buffer. Generally this will be a staging buffer copying to a buffer on the GPU (device_buffer). You must specify
//the size as well that you want to copy
ZEST_API void zest_CopyBuffer(zest_buffer src_buffer, zest_buffer dst_buffer, VkDeviceSize size);
//Exactly the same as zest_CopyBuffer but you can specify a command buffer to use to make the copy. This can be useful if you are doing a
//one off copy with a separate command buffer
ZEST_API void zest_CopyBufferCB(VkCommandBuffer command_buffer, zest_buffer staging_buffer, zest_buffer device_buffer, VkDeviceSize size);
//Free a zest_buffer and return it's memory to the pool
ZEST_API void zest_FreeBuffer(zest_buffer buffer);
//Free a zest_descriptor_buffer and return it's memory to the pool
ZEST_API void zest_FreeDescriptorBuffer(zest_descriptor_buffer buffer);
//Get the VkDeviceMemory from a zest_buffer. This is needed for some Vulkan commands
ZEST_API VkDeviceMemory zest_GetBufferDeviceMemory(zest_buffer buffer);
//Get the VkBuffer from a zest_buffer. This is needed for some Vulkan Commands
ZEST_API VkBuffer *zest_GetBufferDeviceBuffer(zest_buffer buffer);
//When creating your own draw routines you will probably need to upload data from a staging buffer to a GPU buffer like a vertex buffer. You can
//use this command with zest_UploadBuffer to upload the buffers that you need. You can call zest_AddCopyCommand multiple times depending on 
//how many buffers you need to upload data for and then call zest_UploadBuffer passing the zest_buffer_uploader_t to copy all the buffers in 
//one go. For examples see the builtin draw layers that do this like: zest__update_instance_layer_buffers_callback
ZEST_API void zest_AddCopyCommand(zest_buffer_uploader_t *uploader, zest_buffer_t *source_buffer, zest_buffer_t *target_buffer, VkDeviceSize target_offset);
ZEST_API zest_bool zest_UploadBuffer(zest_buffer_uploader_t *uploader, VkCommandBuffer command_buffer);
//Get the default pool size that is set for a specific pool hash.
ZEST_API zest_buffer_pool_size_t zest_GetDevicePoolSize(zest_key hash);
//Get the default pool size that is set for a specific combination of usage, property and image flags
ZEST_API zest_buffer_pool_size_t zest_GetDeviceBufferPoolSize(VkBufferUsageFlags usage_flags, VkMemoryPropertyFlags property_flags, VkImageUsageFlags image_flags);
//Get the default pool size for an image pool. 
ZEST_API zest_buffer_pool_size_t zest_GetDeviceImagePoolSize(const char *name);
//Set the default pool size for a specific type of buffer set by the usage and property flags. You must call this before you call zest_Initialise
//otherwise it might not take effect on any buffers that are created during initialisation.
//Note that minimum allocation size may get overridden if it is smaller than the alignment reported by vkGetBufferMemoryRequirements at pool creation
ZEST_API void zest_SetDeviceBufferPoolSize(const char *name, VkBufferUsageFlags usage_flags, VkMemoryPropertyFlags property_flags, zest_size minimum_allocation, zest_size pool_size);
//Set the default pool size for images. based on image usage and property flags.
//Note that minimum allocation size may get overridden if it is smaller than the alignment reported by vkGetImageMemoryRequirements at pool creation
ZEST_API void zest_SetDeviceImagePoolSize(const char *name, VkImageUsageFlags image_flags, VkMemoryPropertyFlags property_flags, zest_size minimum_allocation, zest_size pool_size);
//Create a buffer specifically for use as a uniform buffer. Essentially this is just a zest_descriptor_buffer.
ZEST_API zest_uniform_buffer zest_CreateUniformBuffer(const char *name, zest_size uniform_struct_size);
//Standard builtin functions for updating a uniform buffer for use in 2d shaders where x,y coordinates represent a location on the screen. This will
//update the current frame in flight. If you need to update a specific frame in flight then call zest_UpdateUniformBufferFIF.
ZEST_API void zest_Update2dUniformBuffer(void);
ZEST_API void zest_Update2dUniformBufferFIF(zest_index fif);
//Get a pointer to a uniform buffer. This will return a void* which you can cast to whatever struct your storing in the uniform buffer. This will get the buffer
//with the current frame in flight index. 
ZEST_API void *zest_GetUniformBufferData(zest_uniform_buffer uniform_buffer);
//Get a pointer to a uniform buffer. This will return a void* which you can cast to whatever struct your storing in the uniform buffer. This will get the buffer
//with the frame in flight index that you pass to the buffer
ZEST_API void *zest_GetUniformBufferDataFIF(zest_uniform_buffer uniform_buffer, zest_index fif);
//Get the VkDescriptorBufferInfo of a uniform buffer by name and specific frame in flight. Use ZEST_FIF you just want the current frame in flight
ZEST_API VkDescriptorBufferInfo *zest_GetUniformBufferInfo(const char *name, zest_index fif);
//Get a uniform buffer by name
ZEST_API zest_descriptor_buffer zest_GetUniformBuffer(const char *name);
//Returns true if a uniform buffer exists
ZEST_API zest_bool zest_UniformBufferExists(const char *name);
//Bind a vertex buffer. For use inside a draw routine callback function.
ZEST_API void zest_BindVertexBuffer(zest_buffer buffer);
//Bind an index buffer. For use inside a draw routine callback function.
ZEST_API void zest_BindIndexBuffer(zest_buffer buffer);
//--End Buffer related

//-----------------------------------------------
//		Command queue setup and creation
//		Command queues are where all of the vulkan commands get written for submitting and executing
//		on the GPU. A simple command queue is created by Zest when you Initialise zest for drawing
//		sprites. The best way to understand them would be to look at the examples, such as the render
//		targets example. Note that the command queue setup commands are contextual and must be called
//		in the correct order depending on how you want your queue setup.
//		The declarations below are indented to indicate contextual dependencies of the commands where
//		applicable
//-----------------------------------------------

// -- Contextual command queue setup commands
//Create a new command queue with the name you pass to the function. This command queue assumes that you want to render to the swap chain.
//Context:	None, this is a root set up command that sets the context to: zest_setup_context_type_command_queue 
ZEST_API zest_command_queue zest_NewCommandQueue(const char *name);
//Create a new command queue that you can submit anytime you need to. This is not for rendering to the swap chain but generally for rendering
//to a zest_render_target
//Context:	None, this is a root set up command that sets the context to: zest_setup_context_type_command_queue 
ZEST_API zest_command_queue zest_NewFloatingCommandQueue(const char *name);
	//Create new draw commands. Draw commands are where you can add draw routines, either builtin ones like sprite and billboard drawing, or your
	//own custom ones. With this draw commands setup you can specify a render target that you want to draw to.
	//Context:	Must be called after zest_NewCommandQueue or zest_NewFloatingCommandQueue when the context will be zest_setup_context_type_command_queue
	//			This will set the context to zest_setup_context_type_render_pass
	ZEST_API zest_command_queue_draw_commands zest_NewDrawCommandSetup(const char *name, zest_render_target render_target);
	//Create new draw commands that draw directly to the swap chain. 
	//Context:	Must be called after zest_NewCommandQueue or zest_NewFloatingCommandQueue. This will add the draw commands to the current command
	//			queue being set up. This will set the current context to zest_setup_context_type_render_pass 
	ZEST_API zest_command_queue_draw_commands zest_NewDrawCommandSetupSwap(const char *name);
		//Add a new draw routine to the current draw commands context. Draw routines are where you can setup your own custom drawing commands
		ZEST_API void zest_AddDrawRoutine(zest_draw_routine draw_routine);
			//Get the current context draw routine. Must be within a draw routine context
			ZEST_API zest_draw_routine zest_ContextDrawRoutine();
		//Add a new mesh layer to the current draw commands context. A mesh layer can be used to render anything that will use an index and vertex
		//buffer. A mesh layer is what's used to render Dear ImGui if you're using that but see the implementation and example for imgui.
		//Context:	Must be called within a draw commands context 
		ZEST_API zest_layer zest_NewMeshLayer(const char *name, zest_size vertex_struct_size);
		//Add a new builtin layer that zest provides. Currently this is just a sprite, billboard, mesh or fonts
		//Context:	Must be called within a draw commands context 
		ZEST_API zest_layer zest_NewBuiltinLayerSetup(const char *name, zest_builtin_layer_type builtin_layer);
		//Add a pre-existin layer that you already created elsewhere. 
		//Context:	Must be called within a draw commands context 
		ZEST_API void zest_AddLayer(zest_layer layer);
		//Set the clear color of the current draw commands context
		ZEST_API void zest_ContextSetClsColor(float r, float g, float b, float a);
	//Create new compute shader to run within a command queue. See the compute shader section for all the commands relating to setting up a compute shader.
	//Also see the compute shader example
	//Context:	Must be called after zest_NewCommandQueue or zest_NewFloatingCommandQueue. 
	ZEST_API zest_command_queue_compute zest_NewComputeSetup(const char *name, zest_compute compute_shader, void(*compute_function)(zest_command_queue_compute item));
	//Create draw commands that draw render targets to the swap chain. This is useful if you are drawing to render targets elsewhere
	//in the command queue and want to draw some or all of those to the swap chain.
	//Context:	Must be called after zest_NewCommandQueue or zest_NewFloatingCommandQueue. This will add the draw commands to the current command
	//			queue being set up. This will set the current context to zest_setup_context_type_render_pass 
	ZEST_API zest_command_queue_draw_commands zest_NewDrawCommandSetupRenderTargetSwap(const char *name, zest_render_target render_target);
		//Add a render target to the render pass within a zest_NewDrawCommandSetupRenderTargetSwap
		//Context:	Must be called after zest_NewDrawCommandSetupRenderTargetSwap when the context will be zest_setup_context_type_render_pass 
		ZEST_API void zest_AddRenderTarget(zest_render_target render_target);
	//Connect the current command queue to the swap chain. You must call this if you're intending for this queue to be drawn to the swap chain for
	//for presenting to the screen otherwise the command queue will not be valid.
	//Call this function immediately before zest_FinishQueueSetup
	ZEST_API void zest_ConnectQueueToPresent(void);
	//Finish setting up a command queue. You must call this after setting up a command queue to reset contexts and validate the command queue.
	ZEST_API void zest_FinishQueueSetup(void);

//-----------------------------------------------
//		Modifying command queues
//-----------------------------------------------

//Modify an existing zest_command_queue. This will set the command queue you pass to the function as the current set up context.
//Context:	None. This must be called with no context currently set.
ZEST_API void zest_ModifyCommandQueue(zest_command_queue command_queue);
	//Modify an existing zest_command_queue_draw_commands. The draw commands must exist within the current command queue that you're editing.
	//Context:	Must be called after zest_ModifyCommandQueue
	ZEST_API void zest_ModifyDrawCommands(zest_command_queue_draw_commands draw_commands);

//Get an existing zest_command_queue by name
ZEST_API zest_command_queue zest_GetCommandQueue(const char *name);
//Get an existing zest_command_queue_draw_commands object by name
ZEST_API zest_command_queue_draw_commands zest_GetCommandQueueDrawCommands(const char *name);
//Set the clear color for the render pass that happens within a zest_command_queue_draw_commands object
//Note that this only applies if you use the default draw commands callback: zest_RenderDrawRoutinesCallback
ZEST_API void zest_SetDrawCommandsClsColor(zest_command_queue_draw_commands draw_commands, float r, float g, float b, float a);
//Set up the semaphores to connect the present (swap chain) to a command queue. This command is generally called
//automatically when you set up the command queue. This will ensure that the command queue waits for the swap chain to present
//to the screen before executing.
ZEST_API void zest_ConnectPresentToCommandQueue(zest_command_queue receiver, VkPipelineStageFlags stage_flags);
//Set the semaphores in the command queue to connect it to the swap chain. This ensures that the swap chain will wait
//for the command queue to finish executing before presenting the frame to the screen
ZEST_API void zest_ConnectCommandQueueToPresent(zest_command_queue sender);
//Get the semaphore that connects the command queue to the swap chain
ZEST_API VkSemaphore zest_GetCommandQueuePresentSemaphore(zest_command_queue command_queue);
//Any zest_command_queue_draw_commands that you create can be assigned your own custom render pass callback so that you can call any Vulkan commands that you want
//See zest_RenderDrawRoutinesCallback, zest_DrawToRenderTargetCallback, zest_DrawRenderTargetsToSwapchain for some of the builtin functions that do this
ZEST_API void zest_SetDrawCommandsCallback(void(*render_pass_function)(zest_command_queue_draw_commands item, VkCommandBuffer command_buffer, zest_render_pass render_pass, VkFramebuffer framebuffer));
//Get a zest_draw_routine by name
ZEST_API zest_draw_routine zest_GetDrawRoutine(const char *name);
//Get a zest_command_queue_draw_commands by name
ZEST_API zest_command_queue_draw_commands zest_GetDrawCommands(const char *name);
//Builtin draw routine callbacks for drawing the builtin layers and render target drawing.
ZEST_API void zest_RenderDrawRoutinesCallback(zest_command_queue_draw_commands item, VkCommandBuffer command_buffer, zest_render_pass render_pass, VkFramebuffer framebuffer);
ZEST_API void zest_DrawToRenderTargetCallback(zest_command_queue_draw_commands item, VkCommandBuffer command_buffer, zest_render_pass render_pass, VkFramebuffer framebuffer);
ZEST_API void zest_DrawRenderTargetsToSwapchain(zest_command_queue_draw_commands item, VkCommandBuffer command_buffer, zest_render_pass render_pass, VkFramebuffer framebuffer);
//Add an existing zest_draw_routine to a zest_command_queue_draw_commands. This just lets you add draw routines to a queue separately outside of a command queue
//setup context
ZEST_API void zest_AddDrawRoutineToDrawCommands(zest_command_queue_draw_commands draw_commands, zest_draw_routine draw_routine);
//Helper functions for creating the builtin layers. these can be called separately outside of a command queue setup context
ZEST_API zest_layer zest_CreateBuiltinSpriteLayer(const char *name);
ZEST_API zest_layer zest_CreateBuiltinLineLayer(const char *name);
ZEST_API zest_layer zest_CreateBuiltinBillboardLayer(const char *name);
ZEST_API zest_layer zest_CreateBuiltinFontLayer(const char *name);
ZEST_API zest_layer zest_CreateBuiltinMeshLayer(const char *name);
//Insert a layer into storage. You can use this for custom layers of you're own
ZEST_API void zest_InsertLayer(zest_layer layer);
//Get a zest_layer by name
ZEST_API zest_layer zest_GetLayer(const char *name);
//Create a zest_command_queue_compute and add it to a zest_command_queue. You can call this function outside of a command queue setup context. Otherwise
//just call zest_NewComputeSetup when you setup or modify a command queue
ZEST_API zest_command_queue_compute zest_CreateComputeItem(const char *name, zest_command_queue command_queue);
//Validate a command queue to make sure that all semaphores are connected. This is automatically called when you call zest_FinishQueueSetup but I've just left it
//here as an API function if needed
ZEST_API void zest_ValidateQueue(zest_command_queue queue);
//Connect 2 command queues with semaphores - UNTESTED. Write now things are designed to just use a single command queue per frame.
ZEST_API void zest_ConnectCommandQueues(zest_command_queue sender, zest_command_queue receiver, VkPipelineStageFlags stage_flags);
//Connect 2 command queues with semaphores - UNTESTED. Write now things are designed to just use a single command queue per frame.
ZEST_API void zest_ConnectQueueTo(zest_command_queue receiver, VkPipelineStageFlags stage_flags);
//Reset the compute queue shader index back to 0, this is actually done automatically each frame. It's purpose is so that zest_NextComputeRoutine can iterate through
//the compute routines in a compute shader and dispatch each one starting from 0. But if you're only doing that once each frame (which you probably are) then you won't
//need to call this.
ZEST_API void zest_ResetComputeRoutinesIndex(zest_command_queue_compute compute_queue);
//Get the next compute shader in a command queue's list of compute shaders. You can use this in a while loop like:
/*
	zest_compute compute = 0;
	while (compute = zest_NextComputeRoutine(compute_commands)) {
		zest_BindComputePipeline(compute, 0);
		zest_DispatchCompute(compute, PARTICLE_COUNT / 256, 1, 1);
	}
*/
//You can use this inside a compute_function callback which you set when calling zest_NewComputeSetup
ZEST_API zest_compute zest_NextComputeRoutine(zest_command_queue_compute compute_queue);
//Record a zest_command_queue. If you call zest_SetActiveCommandQueue then these functions are called automatically each frame but for floating command
//queues (see zest_NewFloatingCommandQueue) you can record and submite a command queue using these functions. Pass the command_queue and the frame in flight
//index to record.
ZEST_API void zest_RecordCommandQueue(zest_command_queue command_queue, zest_index fif);
//Submit a command queue to be executed on the GPU. Utilise the fence commands to know when the queue has finished executing: zest_CreateFence, zest_CheckFence,
//zest_WaitForFence and zest_DestoryFence. Pass in a fence which will be signalled once the execution is done.
ZEST_API void zest_SubmitCommandQueue(zest_command_queue command_queue, VkFence fence);
//Returns the current command queue that was set with zest_SetActiveCommandQueue.
ZEST_API zest_command_queue zest_CurrentCommandQueue(void);
//Returns the current command buffer being used to record the queue inside the private function zest__draw_renderer_frame. This can be a useful function to use inside
//draw routine callback functions if you need to retrieve the command buffer being recording to for any reason (like to pass to a vkCmd function.
ZEST_API VkCommandBuffer zest_CurrentCommandBuffer(void); 
//-- End Command queue setup and creation

//-----------------------------------------------
//		General Math helper functions
//-----------------------------------------------

//Subtract right from left vector and return the result
ZEST_API zest_vec3 zest_SubVec3(zest_vec3 left, zest_vec3 right);
ZEST_API zest_vec4 zest_SubVec4(zest_vec4 left, zest_vec4 right);
ZEST_API zest_vec3 zest_AddVec3(zest_vec3 left, zest_vec3 right);
ZEST_API zest_vec4 zest_AddVec4(zest_vec4 left, zest_vec4 right);
//Scale a vector by a scalar and return the result
ZEST_API zest_vec2 zest_ScaleVec2(zest_vec2 *vec2, float v);
ZEST_API zest_vec3 zest_ScaleVec3(zest_vec3 *vec3, float v);
ZEST_API zest_vec4 zest_ScaleVec4(zest_vec4 *vec4, float v);
//Multiply 2 vectors and return the result
ZEST_API zest_vec3 zest_MulVec3(zest_vec3 *left, zest_vec3 *right);
ZEST_API zest_vec4 zest_MulVec4(zest_vec4 *left, zest_vec4 *right);
//Get the length of a vec without square rooting
ZEST_API float zest_LengthVec2(zest_vec3 const v);
ZEST_API float zest_Vec2Length2(zest_vec2 const v);
//Get the length of a vec
ZEST_API float zest_LengthVec(zest_vec3 const v);
ZEST_API float zest_Vec2Length(zest_vec2 const v);
//Normalise vectors
ZEST_API zest_vec2 zest_NormalizeVec2(zest_vec2 const v);
ZEST_API zest_vec3 zest_NormalizeVec3(zest_vec3 const v);
//Transform a vector by a 4x4 matrix
ZEST_API zest_vec4 zest_MatrixTransformVector(zest_matrix4 *mat, zest_vec4 vec);
//Get the inverse of a 4x4 matrix
ZEST_API zest_matrix4 zest_Inverse(zest_matrix4 *m);
//Get the cross product between 2 vec3s
ZEST_API zest_vec3 zest_CrossProduct(const zest_vec3 x, const zest_vec3 y);
//Get the dot product between 2 vec3s
ZEST_API float zest_DotProduct(const zest_vec3 a, const zest_vec3 b);
//Create a 4x4 matrix to look at a point
ZEST_API zest_matrix4 zest_LookAt(const zest_vec3 eye, const zest_vec3 center, const zest_vec3 up);
//Create a 4x4 matrix for orthographic projection
ZEST_API zest_matrix4 zest_Ortho(float left, float right, float bottom, float top, float z_near, float z_far);
//Get the distance between 2 points
ZEST_API float zest_Distance(float fromx, float fromy, float tox, float toy);
//Pack a vec3 into an unsigned int. Store something in the extra 2 bits if needed
ZEST_API zest_uint zest_Pack10bit(zest_vec3 *v, zest_uint extra);
//Pack a vec3 into an unsigned int
ZEST_API zest_uint zest_Pack8bitx3(zest_vec3 *v);
//Pack 2 floats into an unsigned int
ZEST_API zest_uint zest_Pack16bit(float x, float y);
//Get the next power up from the number you pass into the function
ZEST_API zest_size zest_GetNextPower(zest_size n);
//Convert degrees into radians
ZEST_API float zest_Radians(float degrees);
//Convert radians into degrees
ZEST_API float zest_Degrees(float radians);
//Interpolate 2 vec3s
ZEST_API zest_vec3 zest_Lerp(zest_vec3 *captured, zest_vec3 *present, float lerp);
//Initialise a new 4x4 matrix
ZEST_API zest_matrix4 zest_M4(float v);
//Flip the signs on a vec3 and return the result
ZEST_API zest_vec3 zest_FlipVec3(zest_vec3 *vec3);
//Scale a 4x4 matrix by a vec4 and return the result
ZEST_API zest_matrix4 zest_ScaleMatrix4x4(zest_matrix4 *m, zest_vec4 *v);
//Scale a 4x4 matrix by a float scalar and return the result
ZEST_API zest_matrix4 zest_ScaleMatrix4(zest_matrix4 *m, float scalar);
//Set a vec2/3/4 with a single value
ZEST_API zest_vec2 zest_Vec2Set1(float v);
ZEST_API zest_vec3 zest_Vec3Set1(float v);
ZEST_API zest_vec4 zest_Vec4Set1(float v);
//Set a Vec2/3/4 with the values you pass into the vector
ZEST_API zest_vec2 zest_Vec2Set(float x, float y);
ZEST_API zest_vec3 zest_Vec3Set(float x, float y, float z);
ZEST_API zest_vec4 zest_Vec4Set(float x, float y, float z, float w);
//Set the color with the passed in rgba values
ZEST_API zest_color zest_ColorSet(zest_byte r, zest_byte g, zest_byte b, zest_byte a);
//Set the color with a single value
ZEST_API zest_color zest_ColorSet1(zest_byte c);
//--End General Math


//-----------------------------------------------
//		Camera helpers
//-----------------------------------------------
//Create a new zest_camera_t. This will initialise values to default values such as up, right, field of view etc.
ZEST_API zest_camera_t zest_CreateCamera(void);
//Turn the camera by a given amount scaled by the sensitivity that you pass into the function.
ZEST_API void zest_TurnCamera(zest_camera_t *camera, float turn_x, float turn_y, float sensitivity);
//Update the camera vector that points foward out of the camera. This is called automatically in zest_TurnCamera but if you update
//camera values manually then you can can call this to update
ZEST_API void zest_CameraUpdateFront(zest_camera_t *camera);
//Move the camera forward by a given speed
ZEST_API void zest_CameraMoveForward(zest_camera_t *camera, float speed);
//Move the camera backward by a given speed
ZEST_API void zest_CameraMoveBackward(zest_camera_t *camera, float speed);
//Move the camera up by a given speed
ZEST_API void zest_CameraMoveUp(zest_camera_t *camera, float speed);
//Move the camera down by a given speed
ZEST_API void zest_CameraMoveDown(zest_camera_t *camera, float speed);
//Straf the camera to the left by a given speed
ZEST_API void zest_CameraStrafLeft(zest_camera_t *camera, float speed);
//Straf the camera to the right by a given speed
ZEST_API void zest_CameraStrafRight(zest_camera_t *camera, float speed);
//Set the position of the camera
ZEST_API void zest_CameraPosition(zest_camera_t *camera, zest_vec3 position);
//Set the field of view for the camera
ZEST_API void zest_CameraSetFoV(zest_camera_t *camera, float degrees);
//Set the current pitch of the camera (looking up and down)
ZEST_API void zest_CameraSetPitch(zest_camera_t *camera, float degrees);
//Set the current yaw of the camera (looking left and right)
ZEST_API void zest_CameraSetYaw(zest_camera_t *camera, float degrees);
//Function to see if a ray intersects a plane. Returns true if the ray does intersect. If the ray intersects then the distance and intersection point is inserted
//into the pointers you pass into the function.
ZEST_API zest_bool zest_RayIntersectPlane(zest_vec3 ray_origin, zest_vec3 ray_direction, zest_vec3 plane, zest_vec3 plane_normal, float *distance, zest_vec3 *intersection);
//Project x and y screen coordinates into 3d camera space. Pass in the x and y screen position, the width and height of the screen and the projection and view matrix
//of the camera which you'd usually retrieve from the uniform buffer.
ZEST_API zest_vec3 zest_ScreenRay(float xpos, float ypos, float view_width, float view_height, zest_matrix4 *projection, zest_matrix4 *view);
//Convert world 3d coordinates into screen x and y coordinates
ZEST_API zest_vec2 zest_WorldToScreen(const zest_vec3 *point, float view_width, float view_height, zest_matrix4 *projection, zest_matrix4 *view);
//Convert world orthographic 3d coordinates into screen x and y coordinates
ZEST_API zest_vec2 zest_WorldToScreenOrtho(const zest_vec3 *point, float view_width, float view_height, zest_matrix4 *projection, zest_matrix4 *view);
//Create a perspective 4x4 matrix passing in the fov in radians, aspect ratio of the viewport and te near/far values
ZEST_API zest_matrix4 zest_Perspective(float fovy, float aspect, float zNear, float zFar);
//-- End camera and other helpers

//-----------------------------------------------
//		Images and textures
//		Use textures to load in images and sample in a shader to display on screen. They're also used in
//		render targets and any other things where you want to create images.
//-----------------------------------------------
//Get the map containing all the textures (do we need this?)
ZEST_API zest_map_textures *zest_GetTextures(void);
//Create a new blank initialised texture. You probably don't need to call this much (maybe I should move to private function) Use the Create texture functions instead for most things
ZEST_API zest_texture zest_NewTexture(void);
//Create a new texture. Give the texture a unique name. There are a few ways that you can store images in the texture:
/*
zest_texture_storage_type_packed			Pack all of the images into a sprite sheet and onto multiple layers in an image array on the GPU
zest_texture_storage_type_bank				Packs all images one per layer, best used for repeating textures or color/bump/specular etc
zest_texture_storage_type_sprite_sheet		Packs all the images onto a single layer spritesheet
zest_texture_storage_type_single			A single image texture
zest_texture_storage_type_storage			A storage texture useful for manipulation and other things in a compute shader
zest_texture_storage_type_stream			A storage texture that you can update every frame (not sure if this is working currently)
zest_texture_storage_type_render_target	Texture storage for a render target sampler, so that you can draw the target onto another render target. Generally this is used only when creating a render_target so you don't have to worry about it
set use filtering to 1 if you want to generate mipmaps for the texture. You can also choose from the following texture formats:
zest_texture_format_alpha = VK_FORMAT_R8_UNORM,
zest_texture_format_rgba = VK_FORMAT_R8G8B8A8_UNORM,
zest_texture_format_bgra = VK_FORMAT_B8G8R8A8_UNORM. */
ZEST_API zest_texture zest_CreateTexture(const char *name, zest_texture_storage_type storage_type, zest_bool use_filtering, zest_texture_format format, zest_uint reserve_images);
//The following are helper functions to create a texture of a given type. the texture will be set to use filtering by default 
ZEST_API zest_texture zest_CreateTexturePacked(const char *name, zest_texture_format format);
ZEST_API zest_texture zest_CreateTextureSpritesheet(const char *name, zest_texture_format format);
ZEST_API zest_texture zest_CreateTextureSingle(const char *name, zest_texture_format format);
ZEST_API zest_texture zest_CreateTextureBank(const char *name, zest_texture_format format);
ZEST_API zest_texture zest_CreateTextureStorage(const char *name, int width, int height, zest_texture_format format, VkImageViewType view_type);
//Delete a texture from the renderer and free its resources. You must ensure that you don't try writing to the texture while deleting.
ZEST_API void zest_DeleteTexture(zest_texture texture);
//Resetting a texture will remove all it's images and reset it back to it's "just created" status
ZEST_API void zest_ResetTexture(zest_texture texture);
//By default any images that you load into a texture will be stored so that you can rebuild the texture if you need to (for example, you may load in more images
//to pack in to the sprite sheets at a later time). But if you want to free up the memory then you can just free them with this function.
ZEST_API void zest_FreeTextureBitmaps(zest_texture texture);
//Remove a specific zest_image from the texture
ZEST_API void zest_RemoveTextureImage(zest_texture texture, zest_image image);
//Remove a specific animation from the texture. Pass in the first frame of the animation with a zest_image
ZEST_API void zest_RemoveTextureAnimation(zest_texture texture, zest_image first_image);
//Set the format of the texture. You will need to reprocess the texture if you have processed it already with zest_ProcessTextureImages
ZEST_API void zest_SetTextureImageFormat(zest_texture texture, zest_texture_format format);
//Create a blank zest_image. This is more for internal usage, prefer the zest_AddTextureImage functions. Will leave these in the API for now though
ZEST_API zest_image_t zest_NewImage(void);
ZEST_API zest_image zest_CreateImage(void);
ZEST_API zest_image zest_CreateAnimation(zest_uint frames);
//Load a bitmap from a file. Set color_channels to 0 to auto detect the number of channels
ZEST_API void zest_LoadBitmapImage(zest_bitmap_t *image, const char *file, int color_channels);
//Load a bitmap from a memory buffer. Set color_channels to 0 to auto detect the number of channels. Pass in a pointer to the memory buffer containing
//the bitmap, the size in bytes and how many channels it has.
ZEST_API void zest_LoadBitmapImageMemory(zest_bitmap_t *image, unsigned char *buffer, int size, int desired_no_channels);
//Free the memory used in a zest_bitmap_t
ZEST_API void zest_FreeBitmap(zest_bitmap_t *image);
//Create a new initialise zest_bitmap_t
ZEST_API zest_bitmap_t zest_NewBitmap(void);
//Create a new bitmap from a pixel buffer. Pass in the name of the bitmap, a pointer to the buffer, the size in bytes of the buffer, the width and height
//and the number of color channels
ZEST_API zest_bitmap_t zest_CreateBitmapFromRawBuffer(const char *name, unsigned char *pixels, int size, int width, int height, int channels);
//Allocate the memory for a bitmap based on the width, height and number of color channels. You can also specify the fill color
ZEST_API void zest_AllocateBitmap(zest_bitmap_t *bitmap, int width, int height, int channels, zest_uint fill_color);
//Copy all of a source bitmap to a destination bitmap
ZEST_API void zest_CopyWholeBitmap(zest_bitmap_t *src, zest_bitmap_t *dst);
//Copy an area of a source bitmap to another bitmap
ZEST_API void zest_CopyBitmap(zest_bitmap_t *src, int from_x, int from_y, int width, int height, zest_bitmap_t *dst, int to_x, int to_y);
//Convert a bitmap to a specific vulkan color format. Accepted formats are:
//VK_FORMAT_R8G8B8A8_UNORM
//VK_FORMAT_B8G8R8A8_UNORM
//VK_FORMAT_R8_UNORM
ZEST_API void zest_ConvertBitmapToTextureFormat(zest_bitmap_t *src, VkFormat format);
//Convert a bitmap to a specific zest_texture_format. Accepted values are;
//zest_texture_format_alpha
//zest_texture_format_rgba
//zest_texture_format_bgra
ZEST_API void zest_ConvertBitmap(zest_bitmap_t *src, zest_texture_format format, zest_byte alpha_level);
//Convert a bitmap to BGRA format
ZEST_API void zest_ConvertBitmapToBGRA(zest_bitmap_t *src, zest_byte alpha_level);
//Convert a bitmap to RGBA format
ZEST_API void zest_ConvertBitmapToRGBA(zest_bitmap_t *src, zest_byte alpha_level);
//Convert a BGRA bitmap to RGBA format
ZEST_API void zest_ConvertBGRAToRGBA(zest_bitmap_t *src);
//Convert a bitmap to a single alpha channel
ZEST_API void zest_ConvertBitmapToAlpha(zest_bitmap_t *image);
ZEST_API void zest_ConvertBitmapTo1Channel(zest_bitmap_t *image);
//Sample the color of a pixel in a bitmap with the given x/y coordinates
ZEST_API zest_color zest_SampleBitmap(zest_bitmap_t *image, int x, int y);
//Get a pointer to the first pixel in a bitmap within the bitmap array. Index must be less than the number of bitmaps in the array
ZEST_API zest_byte *zest_BitmapArrayLookUp(zest_bitmap_array_t *bitmap_array, zest_index index);
//Get the distance in pixels to the furthes pixel from the center that isn't alpha 0
ZEST_API float zest_FindBitmapRadius(zest_bitmap_t *image);
//Destory a bitmap array and free its resources
ZEST_API void zest_DestroyBitmapArray(zest_bitmap_array_t *bitmap_array);
//Get a bitmap from a bitmap array with the given index
ZEST_API zest_bitmap_t zest_GetImageFromArray(zest_bitmap_array_t *bitmap_array, zest_index index);
//Get a bitmap from a texture with the given index
ZEST_API zest_bitmap_t *zest_GetBitmap(zest_texture texture, zest_index bitmap_index);
//Get a zest_image from a texture by it's index
ZEST_API zest_image zest_GetImageFromTexture(zest_texture texture, zest_index index);
//Add an image to a texture from a file. This uses stb_image to load the image so must be in a format that stb image can load
ZEST_API zest_image zest_AddTextureImageFile(zest_texture texture, const char* name);
//Add an image to a texture using a zest_bitmap_t
ZEST_API zest_image zest_AddTextureImageBitmap(zest_texture texture, zest_bitmap_t *bitmap_to_load);
//Add an image to a texture from a buffer. 
ZEST_API zest_image zest_AddTextureImageMemory(zest_texture texture, const char* name, unsigned char* buffer, int buffer_size);
//Add a sequence of images (in a sprite sheet) to a texture from a file. specify the width and height of each frame (they must all be the same size), the number of frames in the animation.
//You can also pass in a pointer to a float where the max radius can be set (the max radius is the furthest pixel from the center of the frame). Lastly you can specify
//whether the animation runs row by row, set to 0 to read column by column.
ZEST_API zest_image zest_AddTextureAnimationFile(zest_texture texture, const char* filename, int width, int height, zest_uint frames, float *max_radius, zest_bool row_by_row);
//Add an animation from a bitmap.
ZEST_API zest_image zest_AddTextureAnimationBitmap(zest_texture texture, zest_bitmap_t *spritesheet, int width, int height, zest_uint frames, float *max_radius, zest_bool row_by_row);
//Add an animation from a buffer in memory.
ZEST_API zest_image zest_AddTextureAnimationMemory(zest_texture texture, const char* name, unsigned char *buffer, int buffer_size, int width, int height, zest_uint frames, float *max_radius, zest_bool row_by_row);
//After adding all the images you want to a texture, you will then need to process the texture which will create all of the necessary GPU resources and upload the texture to the GPU.
//You can then use the image handles to draw the images along with the descriptor set - either the one that gets created automatically with the the texture to draw sprites and billboards
//or your own descriptor set.
ZEST_API void zest_ProcessTextureImages(zest_texture texture);
//This function will create a simple 1 sampler 1 uniform buffer descriptor set using the uniform buffer that you pass to it by name. Pass the texture that you will
//create the descriptor set in and give it a unique name in the texture. This will return a handle to the descriptor set.
ZEST_API zest_descriptor_set zest_CreateSimpleTextureDescriptorSet(zest_texture texture, const char *name, const char *uniform_buffer_name);
//Get a descriptor set from a texture. When a texture is processed a default descriptor set is created for drawing sprites and billboards called "Default". Otherwise you can build 
//your own descriptor set for the texture tailored to your own shaders.
ZEST_API zest_descriptor_set zest_GetTextureDescriptorSet(zest_texture texture, const char *name);
//Every texture has a current descriptor set as the default descriptor set stored in texture->current_descriptor_set. Use this command to switch the current descriptor set to another
//descriptor set stored in the texture.
ZEST_API void zest_SwitchTextureDescriptorSet(zest_texture texture, const char *name);
//Get the current descriptor set in the texture which was set with zest_SwitchTextureDescriptorSet.
ZEST_API VkDescriptorSet zest_CurrentTextureDescriptorSet(zest_texture texture);
//Update the descriptor set for a single descriptor set in the texture. Just pass the name of the descriptor that you want to update. This is necessary to run after making any changes
//to the texture that required a call to zest_ProcessTexture
ZEST_API void zest_UpdateTextureSingleDescriptorSet(zest_texture texture, const char *name);
//Create a vulkan frame buffer. Maybe this should be a private function, but leaving API for now. This returns a zest_frame_buffer_t containing the frame buffer attachment and other
//details about the frame buffer. You must pass in a valid render pass that the frame buffer will use (you can use zest_GetRenderPass), the width/height of the frame buffer, a valid
//VkFormat, whether or not you want to use a depth buffer with it, and if it's a frame buffer that will be used as a src buffer for something else so that it gets the appropriate
//flags set. You shouldn't need to use this function as it's mainly used when creating zest_render_targets.
ZEST_API zest_frame_buffer_t zest_CreateFrameBuffer(VkRenderPass render_pass, uint32_t width, uint32_t height, VkFormat render_format, zest_bool use_depth_buffer, zest_bool is_src);
//Create and allocate a bitmap array. Bitmap arrays are exactly as they sound, an array of bitmaps. These are used to copy an array of bitmaps into a GPU texture array. These are
//primarily used in the creation of textures that use zest_texture_storage_type_packed and zest_texture_storage_type_bank. 
ZEST_API void zest_CreateBitmapArray(zest_bitmap_array_t *images, int width, int height, int channels, zest_uint size_of_array);
//Free a bitmap array and return memory resources to the allocator
ZEST_API void zest_FreeBitmapArray(zest_bitmap_array_t *images);
//Set the handle of an image. This dictates where the image will be positioned when you draw it with zest_DrawSprite/zest_DrawBillboard. 0.5, 0.5 will center the image at the position you draw it.
ZEST_API void zest_SetImageHandle(zest_image image, float x, float y);
//Change the storage type of the texture. You will need to call zest_ProcessTextureImages after calling this function.
ZEST_API void zest_SetTextureStorageType(zest_texture texture, zest_texture_storage_type value);
//Set whether you want the texture to use mip map filtering or not. You will need to call zest_ProcessTextureImages after calling this function.
ZEST_API void zest_SetTextureUseFiltering(zest_texture texture, zest_bool value);
//Add a border to the images that get packed into the texture. This can make filtered textures look better.
ZEST_API void zest_SetTexturePackedBorder(zest_texture texture, zest_uint value);
//Set the texture wrapping for sampling textures in the shader. If you want textures to repeat then this is more relavent for texture banks or single image textures.
//Just pass in the VkSamplerAddressMode for each dimension u,v,w
ZEST_API void zest_SetTextureWrapping(zest_texture texture, VkSamplerAddressMode u, VkSamplerAddressMode v, VkSamplerAddressMode w);
//Helper functions for setting the texture wrapping to the one you want.
ZEST_API void zest_SetTextureWrappingClamp(zest_texture texture);
ZEST_API void zest_SetTextureWrappingBorder(zest_texture texture);
ZEST_API void zest_SetTextureWrappingRepeat(zest_texture texture);
//Set the size of each layer in a packed texture. When all of the images are processed in a texture, they will be packed into layers of a given size. By default that size is 1024, but you
//can change that here.
ZEST_API void zest_SetTextureLayerSize(zest_texture texture, zest_uint size);
//Toggle whether or not you want the max radius to be calculated for each image/animation that you load into the texture. The max_radius will be recorded in zest_image->max_radius.
ZEST_API void zest_SetTextureMaxRadiusOnLoad(zest_texture texture, zest_bool yesno);
//Set the zest_imgui_blendtype of the texture. If you use dear imgui and are drawing render targets then this can be useful if you need to alter how the render target is blended when drawn. 
//zest_imgui_blendtype_none				Just draw with standard alpha blend
//zest_imgui_blendtype_pass				Force the alpha channel to 1
//zest_imgui_blendtype_premultiply		Divide the color channels by the alpha channel
ZEST_API void zest_SetTextureImguiBlendType(zest_texture texture, zest_imgui_blendtype blend_type);
//Resize a single or storage texture.
ZEST_API void zest_TextureResize(zest_texture texture, zest_uint width, zest_uint height);
//Clear a texture
ZEST_API void zest_TextureClear(zest_texture texture);
//For single or storage textures, get the bitmap for the texture.
ZEST_API zest_bitmap_t *zest_GetTextureSingleBitmap(zest_texture texture);
//Add a descriptor set to the texture. You'll need this if your using your own shaders that have different bindings from the default.
ZEST_API void zest_AddTextureDescriptorSet(zest_texture texture, const char *name, zest_descriptor_set descriptor_set);
//Every texture you create is stored by name in the render, use this to retrieve one by name.
ZEST_API zest_texture zest_GetTexture(const char *name);
//Returns true if the texture has a storage type of zest_texture_storage_type_bank or zest_texture_storage_type_single.
ZEST_API zest_bool zest_TextureCanTile(zest_texture texture);
//Update all descriptor sets in the texture. Call this if the texture has been altered so that it's image view and sampler have changed. But bear in mind that the texture cannot be in use
//by the renderer (accessed by a command queue). 
ZEST_API void zest_RefreshTextureDescriptors(zest_texture texture);
//Schedule a texture to be reprocessed. This will ensure that it only gets processed (zest_ProcessTextureImages) when not in use.
ZEST_API void zest_ScheduleTextureReprocess(zest_texture texture);
//Call this from a separate thread that's waiting for a texture to be reprocessed.
ZEST_API void zest_WaitUntilTexturesReprocessed();
//Copies an area of a frame buffer such as from a render target, to a zest_texture.
ZEST_API void zest_CopyFramebufferToTexture(zest_frame_buffer_t *src_image, zest_texture texture, int src_x, int src_y, int dst_x, int dst_y, int width, int height);
//Copies an area of a zest_texture to another zest_texture
ZEST_API void zest_CopyTextureToTexture(zest_texture src_image, zest_texture target, int src_x, int src_y, int dst_x, int dst_y, int width, int height);
//Copies an area of a zest_texture to a zest_bitmap_t.
ZEST_API void zest_CopyTextureToBitmap(zest_texture src_image, zest_bitmap_t *image, int src_x, int src_y, int dst_x, int dst_y, int width, int height, zest_bool swap_channel);
//-- End Images and textures

//-----------------------------------------------
//-- Fonts
//-----------------------------------------------
//Load a .zft file for use with drawing MSDF fonts
ZEST_API zest_font zest_LoadMSDFFont(const char *filename);
//Unload a zest_font and return it's memory to the allocator.
ZEST_API void zest_UnloadFont(zest_font font);
//Get a font that you previously loaded with zest_LoadMSDFFont. It's stored in the renderer with it's full path that you used to load the font
ZEST_API zest_font zest_GetFont(const char *font);
//-- End Fonts

//-----------------------------------------------
//		Render targets
//		Render targets are objects you can use to draw to. You can set up a command queue to draw to any
//		render targets that you need and then draw those render targets to the swap chain. For a more 
//		advanced example of using render targets see the zest-render-targets example.
//-----------------------------------------------
//When you create a render you can pass in a zest_render_target_create_info_t where you can configure some aspects of the render target. Use this to create an initialised
//zest_render_target_create_info_t for use with this.
ZEST_API zest_render_target_create_info_t zest_RenderTargetCreateInfo();
//Creat a new render target with a name and the zest_render_target_create_info_t containing the configuration of the render target. All render targets are stored in the 
//renderer by name. Returns a handle to the render target.
ZEST_API zest_render_target zest_CreateRenderTargetWithInfo(const char *name, zest_render_target_create_info_t create_info);
//Create a render target with the given name and default settings. By default the size will be the same as the window.
ZEST_API zest_render_target zest_CreateRenderTarget(const char *name);
//Create a new initialised render target.
ZEST_API zest_render_target zest_NewRenderTarget();
//Frees all the resources in the frame buffer
ZEST_API void zest_CleanUpRenderTarget(zest_render_target render_target, zest_index fif);
//Recreate the render target resources. This is called automatically whenever the window is resized (if it's not a fixed size render target). But if you need you can 
//manually call this function.
ZEST_API void zest_RecreateRenderTargetResources(zest_render_target render_target, zest_index fif);
//Add a render target for post processing. This will create a new render target with the name you specify and allow you to do post processing effects. See zest-render-targets
//for a blur effect examples. 
//Pass in a ratio_width and ratio_height. This will size the render target based on the size of the input source render target that you pass into the function. You can also pass
//in some user data that you might need, this gets forwarded on to a callback function that you specify. This callback function is where you can do your post processing in 
//the command queue. Again see the zest-render-targets example.
ZEST_API zest_render_target zest_AddPostProcessRenderTarget(const char *name, float ratio_width, float ratio_height, zest_render_target input_source, void *user_data, void(*render_callback)(zest_render_target target, void *user_data));
//Set the post process callback in the render target. This is the same call back specified in the zest_AddPostProcessRenderTarget function.
ZEST_API void zest_SetRenderTargetPostProcessCallback(zest_render_target render_target, void(*render_callback)(zest_render_target target, void *user_data));
//Set the user data you want to associate with the render target. This is pass through to the post process callback function.
ZEST_API void zest_SetRenderTargetPostProcessUserData(zest_render_target render_target, void *user_data);
//Get render target by name
ZEST_API zest_render_target zest_GetRenderTarget(const char *name);
//Get a pointer to the render target descriptor set. You might need this when binding a pipeline to draw the render target.
ZEST_API VkDescriptorSet *zest_GetRenderTargetSamplerDescriptorSet(zest_render_target render_target);
//Get a pointer the render target's source render target descriptor set. You might need this when binding the source render target in the post processing callback function
ZEST_API VkDescriptorSet *zest_GetRenderTargetSourceDescriptorSet(zest_render_target render_target);
//Get the texture from a zest_render_target
ZEST_API zest_texture zest_GetRenderTargetTexture(zest_render_target render_target);
//Get the zest_image from a zest_render_target. You could use this image to draw a sprite or billboard with it.
ZEST_API zest_image zest_GetRenderTargetImage(zest_render_target render_target);
//Get a pointer to the render target frame buffer.
ZEST_API zest_frame_buffer_t *zest_RenderTargetFramebuffer(zest_render_target render_target);
//Get a pointer to the render target frame buffer. using a specific frame in flight
ZEST_API zest_frame_buffer_t *zest_RenderTargetFramebufferByFIF(zest_render_target render_target, zest_index fif);
//A callback that's used to get the swap chain frame buffer within a command queue
ZEST_API VkFramebuffer zest_GetRendererFrameBufferCallback(zest_command_queue_draw_commands item);
//A callback that's used to get a render target frame buffer within a command queue
ZEST_API VkFramebuffer zest_GetRenderTargetFrameBufferCallback(zest_command_queue_draw_commands item);
//If you need to you can make a render target preserver it's frame buffer between renders. In other words it won't be cleared.
ZEST_API void zest_PreserveRenderTargetFrames(zest_render_target render_target, zest_bool yesno);
//Resize the render target. Applies to fixed sized render targets only
ZEST_API void zest_ResizeRenderTarget(zest_render_target render_target, zest_uint width, zest_uint height);
//Change the render target sampler texture wrapping option to clamped. The render target will be refresh next frame.
ZEST_API void zest_SetRenderTargetSamplerToClamp(zest_render_target render_target);
//Change the render target sampler texture wrapping option to repeat. The render target will be refresh next frame.
ZEST_API void zest_SetRenderTargetSamplerToRepeat(zest_render_target render_target);
//Clear a specific frame in flight of the render target.
ZEST_API void zest_RenderTargetClear(zest_render_target render_target, zest_uint fif);
//-- End Render targets

//-----------------------------------------------
//		Main loop update functions
//-----------------------------------------------
//Set the command queue that you want the next frame to be rendered by. You must call this function every frame or else
//you will only render a blank screen. Just pass the zest_command_queue that you want to use. If you initialised the editor
//with a default command queue then you can call this function with zest_SetActiveCommandQueue(ZestApp->default_command_queue)
//Use the Command queue setup and creation functions to create your own command queues. See the examples for how to do this.
ZEST_API void zest_SetActiveCommandQueue(zest_command_queue command_queue);

//-----------------------------------------------
//		Draw Routines
//		These are used to setup drawing in Zest. There are a few builtin draw routines
//		for drawing sprites and billboards but you can set up your own for anything else.
//-----------------------------------------------
//Create a new draw routine which you can use to set up your own custom drawing
ZEST_API zest_draw_routine zest_CreateDrawRoutine(const char *name);
//-- End Draw Routines

//-----------------------------------------------
//-- Draw Layers API
//-----------------------------------------------
//Start a new set of draw instructs for a standard zest_layer. These were internal functions but they've been made api functions for making you're own custom
//instance layers more easily
ZEST_API void zest_StartInstanceInstructions(zest_layer instance_layer);
//End a set of draw instructs for a standard zest_layer
ZEST_API void zest_EndInstanceInstructions(zest_layer instance_layer);
//Reset the drawing for an instance layer. This is called after all drawing is done and dispatched to the gpu
ZEST_API void zest_ResetInstanceLayerDrawing(zest_layer layer);
//Send all the draw commands for an instance layer to the GPU
ZEST_API void zest_DrawInstanceLayer(zest_layer instance_layer, VkCommandBuffer command_buffer);
//Allocate a new zest_layer and return it's handle. This is mainly used internally but will leave it in the API for now
ZEST_API zest_layer zest_NewLayer();
//Set the viewport of a layer. This is important to set right as when the layer is drawn it needs to be clipped correctly and in a lot of cases match how the 
//uniform buffer is setup
ZEST_API void zest_SetLayerViewPort(zest_layer instance_layer, int x, int y, zest_uint scissor_width, zest_uint scissor_height, float viewport_width, float viewport_height);
ZEST_API void zest_SetLayerScissor(zest_layer instance_layer, int offset_x, int offset_y, zest_uint scissor_width, zest_uint scissor_height);
//Set the size of the layer. Called internally to set it to the window size. Can this be internal?
ZEST_API void zest_SetLayerSize(zest_layer layer, float width, float height);
//Set the scale of the layer. For example, you might have a layer which is 256x256 displaying in a window that is 1280x768, so you can set the scale to 
//zest_SetLayerScale(layer, 256/1280, 256/768)
ZEST_API void zest_SetLayerScale(zest_layer layer, float x, float y);
//Get a layer from the renderer by name.
ZEST_API zest_layer zest_GetLayer(const char *name);
//Set the layer color. This is used to apply color to the sprite/font/billboard that you're drawing or you can use it in your own draw routines that use zest_layers.
//Note that the alpha value here is actually a transition between additive and alpha blending. Where 0 is the most additive and 1 is the most alpha blending. Anything
//imbetween is a combination of the 2.
ZEST_API void zest_SetLayerColor(zest_layer layer, zest_byte red, zest_byte green, zest_byte blue, zest_byte alpha);
ZEST_API void zest_SetLayerColorf(zest_layer layer, float red, float green, float blue, float alpha);
//Set the intensity of the layer. This is used to apply an alpha value to the sprite or billboard you're drawing or you can use it in your own draw routines that
//use zest_layers. Note that intensity levels can exceed 1.f to make your sprites extra bright because of pre-multiplied blending in the sprite.
ZEST_API void zest_SetLayerIntensity(zest_layer layer, float value);
//A dirty layer denotes that it's buffers have changed and therefore needs uploading to the GPU again. This is currently used for Dear Imgui layers.
ZEST_API void zest_SetLayerDirty(zest_layer layer);
//-- End Draw Layers

//-----------------------------------------------
//		Draw sprite layers
//		Built in layer for drawing 2d sprites on the screen. To add this type of layer to
//		a command queue see zest_NewBuiltinLayerSetup or zest_CreateBuiltinSpriteLayer
//-----------------------------------------------
//Set the texture, descriptor set and pipeline for any calls to zest_DrawSprite that come after it. You must call this function if you want to do any sprite drawing, and you
//must call it again if you wish to switch either the texture, descriptor set or pipeline to do the drawing. Everytime you call this function it creates a new draw instruction
//in the layer for drawing sprites so each call represents a separate draw call in the render. So if you just call this function once you can call zest_DrawSprite as many times
//as you want and they will all be drawn with a single draw call.
//Pass in the zest_layer, zest_texture, zest_descriptor_set and zest_pipeline. A few things to note:
//1) The descriptor layout used to create the descriptor set must match the layout used in the pipeline.
//2) You can pass 0 in the descriptor set and it will just use the default descriptor set used in the texture.
ZEST_API void zest_SetSpriteDrawing(zest_layer sprite_layer, zest_texture texture, zest_descriptor_set descriptor_set, zest_pipeline pipeline);
//Draw a sprite on the screen using the zest_layer and zest_image you pass to the function. You must call zest_SetSpriteDrawing for the layer and the texture where the image exists.
//x, y: Coordinates on the screen to position the sprite at.
//r: Rotation of the sprite in radians
//sx, sy: The size of the sprite in pixels
//hx, hy: The handle of the sprite
//alignment: If you're stretching the sprite then this should be a normalised 2d vector. You zest_Pack16bit to pack a vector into a zest_uint
//stretch: the stretch factor of the sprite.
ZEST_API void zest_DrawSprite(zest_layer layer, zest_image image, float x, float y, float r, float sx, float sy, float hx, float hy, zest_uint alignment, float stretch);
//Draw a textured sprite which is basically a textured rect if you just want to repeat a image across an area. The texture that's used for the image should be 
//zest_texture_storage_type_bank or zest_texture_storage_type_single. You must call zest_SetSpriteDrawing for th layer and the texture where the image exists.
//x,y:					The coordinates on the screen of the top left corner of the sprite.
//width, height:		The size in pixels of the area for the textured sprite to cover.
//scale_x, scale_y:		The scale of the uv texture coordinates
//offset_x, offset_y:	The offset of the uv coordinates
ZEST_API void zest_DrawTexturedSprite(zest_layer layer, zest_image image, float x, float y, float width, float height, float scale_x, float scale_y, float offset_x, float offset_y);
//-- End Draw sprite layers

//-----------------------------------------------
//		Draw billboard layers
//		Built in layer for drawing 3d sprites, or billboards. These can either always face the camera or be
//		aligned to a vector, or even both depending on what you need. You'll need a uniform buffer with the
//		appropriate projection and view matrix for 3d. See zest-instance-layers for examples.
//-----------------------------------------------
//Set the texture, descriptor set and pipeline for any calls to zest_DrawBillboard that come after it. You must call this function if you want to do any billboard drawing, and you
//must call it again if you wish to switch either the texture, descriptor set or pipeline to do the drawing. Everytime you call this function it creates a new draw instruction
//in the layer for drawing billboards so each call represents a separate draw call in the render. So if you just call this function once you can call zest_DrawBillboard as many times
//as you want and they will all be drawn with a single draw call.
//Pass in the zest_layer, zest_texture, zest_descriptor_set and zest_pipeline. A few things to note:
//1) The descriptor layout used to create the descriptor set must match the layout used in the pipeline.
//2) You can pass 0 in the descriptor set and it will just use the default descriptor set used in the texture.
ZEST_API void zest_SetBillboardDrawing(zest_layer billboard_layer , zest_texture texture, zest_descriptor_set descriptor_set, zest_pipeline pipeline);
//Draw a billboard in 3d space using the zest_layer (must be a built in billboard layer) and zest_image. Note that you will need to use a uniform buffer that sets an appropriate 
//projection and view matrix.  You must call zest_SetSpriteDrawing for the layer and the texture where the image exists.
//Position:				Should be a pointer to 3 floats representing the x, y and z coordinates to draw the sprite at.
//alignment:			A normalised 3d vector packed into a 8bit snorm uint. This is the alignment of the billboard when using stretch.
//angles:				A pointer to 3 floats representing the pitch, yaw and roll of the billboard
//handle:				A pointer to 2 floats representing the handle of the billboard which is the offset from the position
//stretch:				How much to stretch the billboard along it's alignment.
//alignment_type:		This is a bit flag with 2 bits 00. 00 = align to the camera. 11 = align to the alignment vector. 10 = align to the alignment vector and the camera.
//sx, sy:				The size of the sprite in 3d units
ZEST_API void zest_DrawBillboard(zest_layer layer, zest_image image, float position[3], zest_uint alignment, float angles[3], float handle[2], float stretch, zest_uint alignment_type, float sx, float sy);
//A simplified version of zest_DrawBillboard where you only need to set the position, rotation and size of the billboard. The alignment will always be set to face the camera.
ZEST_API void zest_DrawBillboardSimple(zest_layer layer, zest_image image, float position[3], float angle, float sx, float sy);
//--End Draw billboard layers

//-----------------------------------------------
//		Draw Line layers
//		Built in layer for drawing 2d lines. 
//-----------------------------------------------
//Set descriptor set and pipeline for any calls to zest_DrawBillboard that come after it. You must call this function if you want to do any billboard drawing, and you
//must call it again if you wish to switch either the texture, descriptor set or pipeline to do the drawing. Everytime you call this function it creates a new draw instruction
//in the layer for drawing billboards so each call represents a separate draw call in the render. So if you just call this function once you can call zest_DrawBillboard as many times
//as you want and they will all be drawn with a single draw call.
//Pass in the zest_layer, zest_texture, zest_descriptor_set and zest_pipeline. A few things to note:
//1) The descriptor layout used to create the descriptor set must match the layout used in the pipeline.
//2) You can pass 0 in the descriptor set and it will just use the default descriptor set used in the texture.
ZEST_API void zest_SetShapeDrawing(zest_layer shape_layer, zest_shape_type shape_type, zest_descriptor_set descriptor_set, zest_pipeline pipeline);
//Draw a billboard in 3d space using the zest_layer (must be a built in billboard layer) and zest_image. Note that you will need to use a uniform buffer that sets an appropriate 
//projection and view matrix.  You must call zest_SetSpriteDrawing for the layer and the texture where the image exists.
//Position:				Should be a pointer to 3 floats representing the x, y and z coordinates to draw the sprite at.
//alignment:			A normalised 3d vector packed into a 8bit snorm uint. This is the alignment of the billboard when using stretch.
//angles:				A pointer to 3 floats representing the pitch, yaw and roll of the billboard
//handle:				A pointer to 2 floats representing the handle of the billboard which is the offset from the position
//stretch:				How much to stretch the billboard along it's alignment.
//alignment_type:		This is a bit flag with 2 bits 00. 00 = align to the camera. 11 = align to the alignment vector. 10 = align to the alignment vector and the camera.
//sx, sy:				The size of the sprite in 3d units
ZEST_API void zest_DrawLine(zest_layer layer, float start_point[2], float end_point[2], float width);
ZEST_API void zest_DrawRect(zest_layer layer, float top_left[2], float width, float height);
//--End Draw line layers

//-----------------------------------------------
//		Draw MSDF font layers
//		Draw fonts at any scale using multi channel signed distance fields. This uses a zest font format
//		".zft" which contains a character map with uv coords and a png containing the MSDF image for
//		sampling. You can use a simple application for generating these files in the examples:
//		zest-msdf-font-maker
//-----------------------------------------------
//Before drawing any fonts you must call this function in order to set the font you want to use. the zest_font handle can be used to get the descriptor_set and pipeline. See
//zest-fonts for an example.
ZEST_API void zest_SetMSDFFontDrawing(zest_layer font_layer, zest_font font);
//Set the shadow parameters of your font drawing. This only applies to the current call of zest_SetMSDFFontDrawing. For each text that you want different settings for you must
//call a separate zest_SetMSDFFontDrawing to start a new font drawing instruction.
ZEST_API void zest_SetMSDFFontShadow(zest_layer font_layer, float shadow_length, float shadow_smoothing, float shadow_clipping);
ZEST_API void zest_SetMSDFFontShadowColor(zest_layer font_layer, float red, float green, float blue, float alpha);
//Alter the parameters in the shader to tweak the font details and how it's drawn. This can be useful especially for drawing small scaled fonts.
//bleed: Simulates ink bleeding so higher numbers fatten the font but in a more subtle way then altering the thickness.
//thickness: Makes the font bolder with higher numbers. around 5.f
//aa_factor: The amount to alias the font. Lower numbers work better for smaller fonts
//radius: Scale the distance measured to the font edge. Higher numbers make the font thinner
ZEST_API void zest_TweakMSDFFont(zest_layer font_layer, float bleed, float thickness, float aa_factor, float radius);
//The following functions just let you tweak an individual parameter of the font drawing
ZEST_API void zest_SetMSDFFontBleed(zest_layer font_layer, float bleed);
ZEST_API void zest_SetMSDFFontThickness(zest_layer font_layer, float thickness);
ZEST_API void zest_SetMSDFFontAAFactor(zest_layer font_layer, float aa_factor);
ZEST_API void zest_SetMSDFFontRadius(zest_layer font_layer, float radius);
//Draw a single line of text using an MSDF font. You must call zest_SetMSDFFontDrawing before calling this command to specify which font you want to use to draw with and the zest_layer
//that you pass to the function must match the same layer set with that command.
//text:					The string that you want to display
//x, y:					The position on the screen.
//handle_x, handle_y	How the text is justified. 0.5, 0.5 would center the text at the position. 0, 0 would left justify and 1, 0 would right justify.
//size:					The size of the font in pixels.
//letter_spacing		The amount of spacing imbetween letters
ZEST_API float zest_DrawMSDFText(zest_layer font_layer, const char *text, float x, float y, float handle_x, float handle_y, float size, float letter_spacing);
//Draw a paragraph of text using an MSDF font. You must call zest_SetMSDFFontDrawing before calling this command to specify which font you want to use to draw with and the zest_layer
//that you pass to the function must match the same layer set with that command. Use \n to start new lines in the paragraph.
//text:					The string that you want to display
//x, y:					The position on the screen.
//handle_x, handle_y	How the text is justified. 0.5, 0.5 would center the text at the position. 0, 0 would left justify and 1, 0 would right justify.
//size:					The size of the font in pixels.
//letter_spacing		The amount of spacing imbetween letters
ZEST_API float zest_DrawMSDFParagraph(zest_layer font_layer, const char *text, float x, float y, float handle_x, float handle_y, float size, float letter_spacing, float line_height);
//Get the width of a line of text in pixels given the zest_font, test font size and letter spacing that you pass to the function.
ZEST_API float zest_TextWidth(zest_font font, const char *text, float font_size, float letter_spacing);
//-- End Draw MSDF font layers

//-----------------------------------------------
//		Draw mesh layers
//		Mesh layers let you upload a vertex and index buffer to draw meshes. I set this up primarily for
//		use with Dear ImGui
//-----------------------------------------------
//A default callback function that is used to upload the staging buffers containing the vertex and index buffers to the GPU buffers
//You don't really need to call this manually as it's a callback that's assigned in the draw routine when you call zest_CreateBuiltinMeshLayer or zest_NewMeshLayer.
ZEST_API void zest_UploadMeshBuffersCallback(zest_draw_routine draw_routine, VkCommandBuffer command_buffer);
//These are helper functions you can use to bind the vertex and index buffers in your custom mesh draw routine callback
ZEST_API void zest_BindMeshVertexBuffer(zest_layer layer);
ZEST_API void zest_BindMeshIndexBuffer(zest_layer layer);
//Get the vertex staging buffer. You'll need to get the staging buffers to copy your mesh data to or even just record mesh data directly to the staging buffer
ZEST_API zest_buffer zest_GetVertexStagingBuffer(zest_layer layer);
//Get the index staging buffer. You'll need to get the staging buffers to copy your mesh data to or even just record mesh data directly to the staging buffer
ZEST_API zest_buffer zest_GetIndexStagingBuffer(zest_layer layer);
//Get the vertex buffer on the GPU. 
ZEST_API zest_buffer zest_GetVertexDeviceBuffer(zest_layer layer);
//Get the index buffer on the GPU. 
ZEST_API zest_buffer zest_GetIndexDeviceBuffer(zest_layer layer);
//Grow the mesh vertex buffers. You must update the buffer->memory_in_use so that it can decide if a buffer needs growing
ZEST_API void zest_GrowMeshVertexBuffers(zest_layer layer);
//Grow the mesh index buffers. You must update the buffer->memory_in_use so that it can decide if a buffer needs growing
ZEST_API void zest_GrowMeshIndexBuffers(zest_layer layer);
//Set the mesh drawing specifying any texture, descriptor set and pipeline that you want to use for the drawing
ZEST_API void zest_SetMeshDrawing(zest_layer layer, zest_texture texture, zest_descriptor_set descriptor_set, zest_pipeline pipeline);
//Helper funciton Push a vertex to the vertex staging buffer. It will automatically grow the buffers if needed
ZEST_API void zest_PushVertex(zest_layer layer, float pos_x, float pos_y, float pos_z, float intensity, float uv_x, float uv_y, zest_color color, zest_uint parameters);
//Helper funciton Push an index to the index staging buffer. It will automatically grow the buffers if needed
ZEST_API void zest_PushIndex(zest_layer layer, zest_uint offset);
//Draw a textured plan in 3d. You will need an appropriate 3d uniform buffer for this, see zest-instance-layers for an example usage for this function.
//You must call zest_SetMeshDrawing before calling this function and the layer must match and the image you pass must exist in the same texture.
//x, y, z				The position of the plane
//width, height			The size of the plane. If you make this match the zfar value in the projection matrix then it will effectively be an infinite plane.
//scale_x, scale_y		The scale of the texture repetition
//offset_x, offset_y	The texture uv offset
ZEST_API void zest_DrawTexturedPlane(zest_layer layer, zest_image image, float x, float y, float z, float width, float height, float scale_x, float scale_y, float offset_x, float offset_y);
//--End Draw mesh layers

//-----------------------------------------------
//		Compute shaders
//		Build custom compute shaders and integrate into a command queue or just run separately as you need.
//		See zest-compute-example for a full working example
//-----------------------------------------------
//Create a blank ready-to-build compute object and store by name in the renderer.
ZEST_API zest_compute zest_CreateCompute(const char *name);
//To build a compute shader pipeline you can use a zest_compute_builder_t and corresponding commands to add the various settings for the compute object
ZEST_API zest_compute_builder_t zest_NewComputeBuilder();
//Add a layout bindind a compute builder. This is one of the first things you need to do, just pass a pointer to a zest_compute_builder_t, a VkDescriptorType
//and a binding number. This will be used to create a descripriptor layout for the compute shader.
ZEST_API void zest_AddComputeLayoutBinding(zest_compute_builder_t *builder, VkDescriptorType descriptor_type, int binding);
//Once you've added the layout bindings you can then add each buffer you need to bind in the shader. You must add them in the same order that you added the 
//layout bindings. Use this command to add storage buffers only.
ZEST_API void zest_AddComputeBufferForBinding(zest_compute_builder_t *builder, zest_descriptor_buffer buffer);
//Use this command to add an image binding. This is the same command as zest_AddComputeBufferForBinding but specifically for images
ZEST_API void zest_AddComputeImageForBinding(zest_compute_builder_t *builder, zest_texture texture);
//Add a shader to the compute builder. This will be the shader that is executed on the GPU. Pass a file path where to find the shader.
ZEST_API zest_index zest_AddComputeShader(zest_compute_builder_t *builder, const char *path);
//If you're using a push constant then you can set it's size in the builder here.
ZEST_API void zest_SetComputePushConstantSize(zest_compute_builder_t *builder, zest_uint size);
//If you need a specific way to update the descriptors in the compute shader then you can set that here. By default zest_StandardComputeDescriptorUpdate is used
//which should handle most cases. Descriptors need to be updated if the buffers are changed in any any (like resized).
ZEST_API void zest_SetComputeDescriptorUpdateCallback(zest_compute_builder_t *builder, void(*descriptor_update_callback)(zest_compute compute));
//You must set a command buffer update callback. This is called when the command queue tries to execute the compute shader. Here you can bind buffers and upload
//data to the gpu for use by the compute shader and then dispatch the shader. See zest-compute-example.
ZEST_API void zest_SetComputeCommandBufferUpdateCallback(zest_compute_builder_t *builder, void(*command_buffer_update_callback)(zest_compute compute, VkCommandBuffer command_buffer));
//If you require any custom clean up done for the compute shader then you can sepcify a callback for that here.
ZEST_API void zest_SetComputeExtraCleanUpCallback(zest_compute_builder_t *builder, void(*extra_cleanup_callback)(zest_compute compute));
//Set any pointer to custom user data here. You will be able to access this in the callback functions.
ZEST_API void zest_SetComputeUserData(zest_compute_builder_t *builder, void *data);
//Once you have finished calling the builder commands you will need to call this to actually build the compute shader. Pass a pointer to the builder and the zest_compute
//handle that you got from calling zest_CreateCompute. You can then use this handle to add the compute shader to a command queue with zest_NewComputeSetup in a
//command queue context (see the section on Command queue setup and creation)
ZEST_API void zest_MakeCompute(zest_compute_builder_t *builder, zest_compute compute);
//Attach a render target to a compute shader. Currently un used. Need example
ZEST_API void zest_ComputeAttachRenderTarget(zest_compute compute, zest_render_target render_target);
//You don't have to execute the compute shader as part of a command queue, you can also use this command to run the compute shader separately
ZEST_API void zest_RunCompute(zest_compute compute);
//After calling zest_RunCompute you can call this command to wait for a message from the GPU that it's finished executing
ZEST_API void zest_WaitForCompute(zest_compute compute);
//If your buffers have changed in anyway then you can call this function to update the descriptors for them. This is only needed if you're implementing your own
//descriptor update callback function.
ZEST_API void zest_UpdateComputeDescriptorInfos(zest_compute compute);
//If at anytime you resize buffers used in a compute shader then you will need to update it's descriptor infos so that the compute shader connects to the new
//resized buffer. You can call this function which will just call the callback function you specified when creating the compute shader.
ZEST_API void zest_UpdateComputeDescriptors(zest_compute compute);
//A standard builtin compute descriptor update callback function.
ZEST_API void zest_StandardComputeDescriptorUpdate(zest_compute compute);
//Bind a compute pipeline. You must call this before calling dispatch in your command buffer update callback function. The shader index that you have to specify is
//index of the shader that you added with zest_AddComputeShader when you built the compute shader. 
ZEST_API void zest_BindComputePipeline(zest_compute compute, zest_index shader_index);
//Same command as zest_BindComputePipeline but you can specify a VkCommandBuffer to record to.
ZEST_API void zest_BindComputePipelineCB(VkCommandBuffer command_buffer, zest_compute compute, zest_index shader_index);
//Send the push constants defined in a compute shader. Use inside a compute update command buffer callback function
ZEST_API void zest_SendComputePushConstants(zest_compute compute);
//Send the push constants defined in a compute shader. This variation of the function allows you to specify the command buffer.
ZEST_API void zest_SendComputePushConstantsCB(VkCommandBuffer command_buffer, zest_compute compute);
//Add a barrier to ensure that the compute shader finishes before the vertex input stage. You can use this if the compute shader is writing to a buffer for consumption by the 
//vertex shader.
ZEST_API void zest_ComputeToVertexBarrier();
//Helper function to dispatch a compute shader so you can call this instead of vkCmdDispatch
ZEST_API void zest_DispatchCompute(zest_compute compute, zest_uint group_count_x, zest_uint group_count_y, zest_uint group_count_z);
//Helper function to dispatch a compute shader so you can call this instead of vkCmdDispatch. Specify a command buffer for use in one off dispataches
ZEST_API void zest_DispatchComputeCB(VkCommandBuffer command_buffer, zest_compute compute, zest_uint group_count_x, zest_uint group_count_y, zest_uint group_count_z);
//--End Compute shaders

//-----------------------------------------------
//		Events and States
//-----------------------------------------------
//Returns true is the swap chain was recreated last frame. The swap chain will mainly be recreated if the window size changes
ZEST_API zest_bool zest_SwapchainWasRecreated(void);
//--End Events and States

//-----------------------------------------------
//		Timer functions
//		This is a simple API for a high resolution timer. You can use this to implement fixed step
//		updating for your logic in the main loop, plus for anything else that you need to time.
//-----------------------------------------------
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

//-----------------------------------------------
//		General Helper functions
//-----------------------------------------------
//Read a file from disk into memory. Set terminate to 1 if you want to add \0 to the end of the file in memory
ZEST_API char* zest_ReadEntireFile(const char *file_name, zest_bool terminate);
//Get a render pass by name from the rendererer. You can create your own render pass or use one of the standard builtin renderpasses that are created
//when Zest is initialised, those are:
//Render pass present							//Used for presenting to the swap chain with depth buffer
//Render pass standard							//Color and depth attachment
//Render pass standard no clear					//Color and depth attachment but the color attachment does not clear each frame
//Render pass present no depth					//Presenting to the swap chain with no depth buffer
//Render pass standard no depth					//Color attachment only
//Render pass standard no clear no depth		//Color attachment only with no clearing each frame
ZEST_API zest_render_pass zest_GetRenderPass(const char *name);
//Returns Render pass standard or Render pass standard no depth if zest_renderer_flag_has_depth_buffer is unflagged when initialising Zest. 
ZEST_API VkRenderPass zest_GetStandardRenderPass(void);
//Get the current swap chain frame buffer
ZEST_API VkFramebuffer zest_GetRendererFrameBuffer(zest_command_queue_draw_commands item);
//Get a descriptor set layout by name. The following are built in layouts:
//Standard 1 uniform 1 sampler
//Polygon layout (no sampler)
//Render target layout
//Ribbon 2d layout
ZEST_API zest_descriptor_set_layout zest_GetDescriptorSetLayout(const char *name);
//Get the swap chain extent which will basically be the size of the window returned in a VkExtend2d struct.
ZEST_API VkExtent2D zest_GetSwapChainExtent(void);
//Get the window size in a VkExtent2d. In most cases this is the same as the swap chain extent.
ZEST_API VkExtent2D zest_GetWindowExtent(void);
//Get the current swap chain width
ZEST_API zest_uint zest_SwapChainWidth(void);
//Get the current swap chain height
ZEST_API zest_uint zest_SwapChainHeight(void);
//Get the current swap chain width as a float
ZEST_API float zest_SwapChainWidthf(void);
//Get the current swap chain height as a float
ZEST_API float zest_SwapChainHeightf(void);
//Get the current screen width
ZEST_API zest_uint zest_ScreenWidth(void);
//Get the current screen height
ZEST_API zest_uint zest_ScreenHeight(void);
//Get the current screen width as a float
ZEST_API float zest_ScreenWidthf(void);
//Get the current screen height as a float
ZEST_API float zest_ScreenHeightf(void);
//Get the current mouse x screen coordinate as a float
ZEST_API float zest_MouseXf();
//Get the current mouse y screen coordinate as a float
ZEST_API float zest_MouseYf();
//For retina screens this will return the current screen DPI
ZEST_API float zest_DPIScale(void);
//Set the DPI scale
ZEST_API void zest_SetDPIScale(float scale);
//Get the current frames per second
ZEST_API zest_uint zest_FPS(void);
//Get the current frames per second as a float
ZEST_API float zest_FPSf(void);
//Get the current speed of the mouse in pixels per second. Results are stored in the floats you pass into the function as pointers.
ZEST_API void zest_GetMouseSpeed(double *x, double *y);
//Wait for the device to be idle (finish executing all commands). Only recommended if you need to do a one-off operation like change a texture that could
//still be in use by the GPU
ZEST_API void zest_WaitForIdleDevice(void);
//If you pass true to this function (or any value other then 0) then the zest_app_flag_quit_application meaning the main loop with break at the end of the next frame.
ZEST_API void zest_MaybeQuit(zest_bool condition);
//Send push constants. For use inside a draw routine callback function. pass in the pipeline_layout, shader stage flags (that were set in the pipeline), the 
//size of the push constant struct and a pointer to the data containing the push constants
ZEST_API void zest_SendPushConstants(zest_pipeline pipeline_layout, VkShaderStageFlags shader_flags, zest_uint size, void *data);
//Helper function to send the standard zest_push_constants_t push constants struct.
ZEST_API void zest_SendStandardPushConstants(zest_pipeline_t *pipeline_layout, void *data);
//Helper function to record the vulkan command vkCmdDraw. Will record with the current command buffer being used in the active command queue. For use inside
//a draw routine callback function
ZEST_API void zest_Draw(zest_uint vertex_count, zest_uint instance_count, zest_uint first_vertex, zest_uint first_instance);
//Helper function to record the vulkan command vkCmdDrawIndexed. Will record with the current command buffer being used in the active command queue. For use inside
//a draw routine callback function
ZEST_API void zest_DrawIndexed(zest_uint index_count, zest_uint instance_count, zest_uint first_index, int32_t vertex_offset, zest_uint first_instance);
//Enable vsync so that the frame rate is limited to the current monitor refresh rate. Will cause the swap chain to be rebuilt.
ZEST_API void zest_EnableVSync(void);
//Disable vsync so that the frame rate is not limited to the current monitor refresh rate, frames will render as fast as they can. Will cause the swap chain to be rebuilt.
ZEST_API void zest_DisableVSync(void);
//Log the current FPS to the console once every second
ZEST_API void zest_LogFPSToConsole(zest_bool yesno);
//Return a pointer to the window handle stored in the zest_window_t. This could be anything so it's up to you to cast to the right data type. For example, if you're
//using GLFW then you would cast it to a GLFWwindow*
ZEST_API void *zest_Window(void);
//Set the current frame in flight. You should only use this if you want to execute a floating command queue that contains resources that only have a single frame in flight.
//Once you're done you can call zest_RestoreFrameInFlight
ZEST_API void zest_SetFrameInFlight(zest_uint fif);
//Only use after a call to zest_SetFrameInFlight. Will restore the frame in flight that was in use before the call to zest_SetFrameInFlight.
ZEST_API void zest_RestoreFrameInFlight(void);
//Create a new VkFence
ZEST_API VkFence zest_CreateFence();
//Checks to see if a fence is signalled and if so will destroy the fence.
ZEST_API zest_bool zest_CheckFence(VkFence fence);
//Will wait until a fence has been signalled
ZEST_API void zest_WaitForFence(VkFence fence);
//Destroy a fence that you don't need any more and return its resources.
ZEST_API void zest_DestroyFence(VkFence fence);
//Found out if memory properties are available for the current physical device
ZEST_API zest_bool zest_IsMemoryPropertyAvailable(VkMemoryPropertyFlags flags);
//Find out if the current GPU can use memory that is both device local and cpu visible. If it can then that means that you write directly to gpu memory without the need for
//a staging buffer. AMD cards tend to be better at supporting this then nvidia as it seems nvidia only introduced it later.
ZEST_API zest_bool zest_GPUHasDeviceLocalHostVisible();
//--End General Helper functions

//-----------------------------------------------
//Debug Helpers
//-----------------------------------------------
//Convert one of the standard draw routine callback functions to a string and return it.
ZEST_API const char *zest_DrawFunctionToString(void *function);
//About all the command queues to the console so that you can see the how they're set up.
ZEST_API void zest_OutputQueues();
//About all of the current memory usage that you're using in the renderer to the console. This won't include any allocations you've done elsewhere using you own allocation
//functions or malloc/virtual_alloc etc.
ZEST_API void zest_OutputMemoryUsage();
//--End Debug Helpers

#ifdef __cplusplus
}
#endif

#endif // ! ZEST_POCKET_RENDERER
