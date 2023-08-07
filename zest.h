
#ifndef  ZEST_RENDERER_H
#define ZEST_RENDERER_H

#ifdef _WIN32
#define WIN_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers
#define NOMINMAX
#include <windows.h>
#else
#include <time.h>
#endif
#include <vulkan/vulkan.h>
#include <GLFW/glfw3.h>
#define TLOC_ENABLE_REMOTE_MEMORY
#include "2loc.h"
#include <stdio.h>
#include <math.h>

#ifndef ZEST_MAX_FIF
#define ZEST_MAX_FIF 2
#endif

#ifndef ZEST_MAX_BUFFERS_PER_ALLOCATOR
#define ZEST_MAX_BUFFERS_PER_ALLOCATOR 64
#endif

#ifndef ZEST_ASSERT
#define ZEST_ASSERT assert
#endif

#ifndef ZEST_API
#define ZEST_API
#endif

#ifndef _DEBUG
#define ZEST_ENABLE_VALIDATION_LAYER 0
#else
#define ZEST_ENABLE_VALIDATION_LAYER 1
#endif

#ifndef ZEST__FREE
#define ZEST__FREE(memory) tloc_Free(ZestDevice->allocator, memory)
#endif

//Helper macros
#define ZEST__MIN(a, b) (((a) < (b)) ? (a) : (b))
#define ZEST__MAX(a, b) (((a) > (b)) ? (a) : (b))

#ifndef ZEST__REALLOCATE
#define ZEST__ALLOCATE(size) tloc_Allocate(ZestDevice->allocator, size)
#define ZEST__REALLOCATE(ptr, size) tloc_Reallocate(ZestDevice->allocator, ptr, size)
#endif
#define ZEST__ARRAY(name, type, count) type *name = ZEST__REALLOCATE(0, sizeof(type) * count)
#define ZEST_FIF ZestDevice->current_fif

#define ZEST_NULL 0
#define ZEST_EACHFFIF_i unsigned int i = 0; i != ZEST_MAX_FIF; ++i

//For error checking vulkan commands
#define ZEST_VK_CHECK_RESULT(res)																			\
	{																										\
		if (res != VK_SUCCESS)																				\
		{																									\
			printf("Fatal : VkResult is \" %s \" in %s at line %i\n", zest__vulkan_error(res), __FILE__, __LINE__);	\
			ZEST_ASSERT(res == VK_SUCCESS);																		\
		}																									\
	}

const char *zest__vulkan_error(VkResult errorCode);

typedef unsigned int zest_uint;
typedef int zest_index;
typedef unsigned long long zest_ull;
typedef zest_uint zest_millisecs;
typedef zest_ull zest_microsecs;
typedef zest_ull zest_key;
typedef size_t zest_size;
typedef unsigned int zest_bool;

#define ZEST_TRUE 1
#define ZEST_FALSE 0
#define ZEST_INVALID 0xFFFFFFFF
#define ZEST_U32_MAX_VALUE ((zest_uint)-1)

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
	zest_app_flag_none =					0,
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

typedef enum {
	zest_buffer_flag_none = 0,
	zest_buffer_flag_read_only = 1 << 0,
	zest_buffer_flag_is_fif = 1 << 1
} zest_buffer_flags;

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

typedef enum {
	zest_renderer_flag_enable_multisampling =					1 << 0,
	zest_renderer_flag_schedule_recreate_textures =				1 << 1,
	zest_renderer_flag_schedule_change_vsync =					1 << 2,
	zest_renderer_flag_schedule_rerecord_final_render_buffer =	1 << 3,
	zest_renderer_flag_drawing_loop_running =					1 << 4,
	zest_renderer_flag_msaa_toggled =							1 << 5,
	zest_renderer_flag_vsync_enabled =							1 << 6,
	zest_renderer_flag_disable_default_uniform_update =			1 << 7,
} zest_renderer_flags;

typedef enum {
	zest_pipeline_set_flag_none										= 0,	
	zest_pipeline_set_flag_is_render_target_pipeline				= 1 << 0,		//True if this pipeline is used for the final render of a render target to the swap chain
	zest_pipeline_set_flag_match_swapchain_view_extent_on_rebuild	= 1 << 0		//True if the pipeline should update it's view extent when the swap chain is recreated (the window is resized)
} zest_pipeline_set_flags;

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

// --Pocket Dynamic Array
typedef struct zest_vec {
	zest_uint current_size;
	zest_uint capacity;
} zest_vec;

enum {
	zest__VEC_HEADER_OVERHEAD = sizeof(zest_vec)
};

#define zest__vec_header(T) ((zest_vec*)T - 1)
inline zest_uint zest__grow_capacity(void *T, zest_uint size) { zest_uint new_capacity = T ? (size + size / 2) : 8; return new_capacity > size ? new_capacity : size; }
#define zest_vec_bump(T) zest__vec_header(T)->current_size++;
#define zest_vec_clip(T) zest__vec_header(T)->current_size--;
#define zest_vec_grow(T) ((!(T) || (zest__vec_header(T)->current_size == zest__vec_header(T)->capacity)) ? T = zest__vec_reserve((T), sizeof(*T), (T ? zest__grow_capacity(T, zest__vec_header(T)->current_size) : 8)) : 0)
#define zest_vec(T, name) T *name = ZEST_NULL
#define zest_vec_empty(T) (!T || zest__vec_header(T)->current_size == 0)
#define zest_vec_size(T) (zest__vec_header(T)->current_size)
#define zest_vec_last_index(T) (zest__vec_header(T)->current_size - 1)
#define zest_vec_capacity(T) (zest__vec_header(T)->capacity)
#define zest_vec_size_in_bytes(T) (zest__vec_header(T)->current_size * sizeof(*T))
#define zest_vec_front(T) (T[0])
#define zest_vec_back(T) (T[zest__vec_header(T)->current_size - 1])
#define zest_vec_end(T) (&(T[zest_vec_size(T)]))
#define zest_vec_clear(T) if(T) zest__vec_header(T)->current_size = 0
#define zest_vec_free(T) (ZEST__FREE(zest__vec_header(T)), T = ZEST_NULL)
#define zest_vec_resize(T, new_size) if(!T || zest__vec_header(T)->capacity < new_size) T = zest__vec_reserve(T, sizeof(*T), new_size); zest__vec_header(T)->current_size = new_size
#define zest_vec_push(T, value) zest_vec_grow(T); (T)[zest__vec_header(T)->current_size++] = value 
#define zest_vec_pop(T) (zest__vec_header(T)->current_size--, T[zest__vec_header(T)->current_size])
#define zest_vec_insert(T, location, value) { ptrdiff_t offset = location - T; zest_vec_grow(T); if(offset < zest_vec_size(T)) memmove(T + offset + 1, T + offset, ((size_t)zest_vec_size(T) - offset) * sizeof(*T)); T[offset] = value; zest_vec_bump(T) }
#define zest_vec_erase(T, location) { ZEST_ASSERT(T && location >= T && location < zest_vec_end(T)); ptrdiff_t offset = location - T; memmove(T + offset, T + offset + 1, ((size_t)zest_vec_size(T) - offset) * sizeof(*T)); zest_vec_clip(T); }
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

static inline zest_ull zest__hash_rotate_left(zest_hasher *hasher, zest_ull x, unsigned char bits) { return (x << bits) | (x >> (64 - bits)); }
static inline zest_ull zest__hash_process_single(zest_hasher *hasher, zest_ull previous, zest_ull input) { return zest__hash_rotate_left(hasher, previous + input * zest__PRIME2, 31) * zest__PRIME1; }
static inline void zest__hasher_process(zest_hasher *hasher, const void* data, zest_ull *state0, zest_ull *state1, zest_ull *state2, zest_ull *state3) { zest_ull* block = (zest_ull*)data; *state0 = zest__hash_process_single(hasher, *state0, block[0]); *state1 = zest__hash_process_single(hasher, *state1, block[1]); *state2 = zest__hash_process_single(hasher, *state2, block[2]); *state3 = zest__hash_process_single(hasher, *state3, block[3]); }
static inline zest_bool zest__hasher_add(zest_hasher *hasher, const void* input, zest_ull length)
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

	return ZEST_TRUE;
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
		result = zest__hash_rotate_left(hasher, result ^ (*(zest_uint*)data) * zest__PRIME1, 23) * zest__PRIME2 + zest__PRIME3;
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
inline zest_key zest_Hash(zest_hasher *hasher, const void* input, zest_ull length, zest_ull seed) { zest__hash_initialise(hasher, seed); zest__hasher_add(hasher, input, length); return (zest_key)zest__get_hash(hasher); }
static zest_hasher *ZestHasher = 0;
//-- End of Pocket Hasher

// --Begin pocket hash map
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
#define zest_map_hash(hash_map, name) zest_Hash(ZestHasher, name, strlen(name), ZEST_HASH_SEED) 
#define zest_map_hash_ptr(hash_map, ptr, size) zest_Hash(ZestHasher, ptr, size, ZEST_HASH_SEED) 
#define zest_hash_map(T) typedef struct { zest_hash_pair *map; T *data; }
#define zest_map_valid_name(hash_map, name) (hash_map.map && zest__map_get_index(hash_map.map, zest_map_hash(hash_map, name)) != -1)
#define zest_map_valid_key(hash_map, key) (hash_map.map && zest__map_get_index(hash_map.map, key) != -1)
#define zest_map_valid_index(hash_map, index) (hash_map.map && (zest_uint)index < zest_vec_size(hash_map.data))
#define zest_map_valid_hash(hash_map, ptr, size) (zest__map_get_index(hash_map.map, zest_map_hash_ptr(hash_map, ptr, size)) != -1)
#define zest_map_set_index(hash_map, key, value) zest_hash_pair *it = zest__lower_bound(hash_map.map, key); if(!hash_map.map || it == zest_vec_end(hash_map.map) || it->key != key) { zest_vec_push(hash_map.data, value); zest_hash_pair new_pair; new_pair.key = key; new_pair.index = zest_vec_size(hash_map.data) - 1; zest_vec_insert(hash_map.map, it, new_pair); } else {hash_map.data[it->index] = value;}
#define zest_map_insert(hash_map, name, value) { zest_key key = zest_map_hash(hash_map, name); zest_map_set_index(hash_map, key, value) }
#define zest_map_insert_key(hash_map, key, value) { zest_map_set_index(hash_map, key, value) }
#define zest_map_insert_with_ptr_hash(hash_map, ptr, size, value) { zest_key key = zest_map_hash_ptr(hash_map, ptr, size); zest_map_set_index(hash_map, key, value) }
#define zest_map_at(hash_map, name) &hash_map.data[zest__map_get_index(hash_map.map, zest_map_hash(hash_map, name))]
#define zest_map_at_key(hash_map, key) &hash_map.data[zest__map_get_index(hash_map.map, key)]
#define zest_map_at_index(hash_map, index) &hash_map.data[index]
#define zest_map_get_index_by_key(hash_map, key) zest__map_get_index(hash_map.map, key);
#define zest_map_get_index_by_name(hash_map, name) zest__map_get_index(hash_map.map, zest_map_hash(hash_map, name));
#define zest_map_remove(hash_map, name) {zest_key key = zest_map_hash(hash_map, name); zest_hash_pair *it = zest__lower_bound(hash_map.map, key); zest_index index = it->index; zest_vec_erase(hash_map.map, it); zest_vec_erase(hash_map.data, &hash_map.data[index]); zest__map_realign_indexes(hash_map.map, index); }
#define zest_map_last_index(hash_map) (zest__vec_header(hash_map.data)->current_size - 1)
#define zest_map_clear(hash_map) zest_vec_clear(hash_map.map); zest_vec_clear(hash_map.data);
#define zest_map_foreach_i(hash_map) zest_foreach_i(hash_map.data)
#define zest_map_foreach_j(hash_map) zest_foreach_j(hash_map.data)
// --End pocket hash map

// --Matrix and vector structs

typedef struct zest_vec2 {
	float x, y;
} zest_vec2;

typedef struct zest_vec3 {
	float x, y, z;
} zest_vec3;

typedef struct zest_vec4 {
	float x, y, z, w;
} zest_vec4;

typedef struct zest_matrix4 {
	zest_vec4 v[4];
} zest_matrix4;

typedef struct zest_rgba8 {
	union {
		struct { unsigned char r, g, b, a; };
		struct { zest_uint color; };
	};
} zest_rgba8;

static inline zest_vec3 zest_SubVec(zest_vec3 left, zest_vec3 right) {
	zest_vec3 result = {
		.x = left.x - right.x,
		.y = left.y - right.y,
		.z = left.z - right.z,
	};
	return result;
}

static inline float zest_LengthVec2(zest_vec3 const v) {
	return v.x * v.x + v.y * v.y + v.z * v.z;
}

static inline float zest_LengthVec(zest_vec3 const v) {
	return sqrtf(zest_LengthVec2(v));
}

static inline zest_vec3 zest_NormalizeVec(zest_vec3 const v) {
	float length = zest_LengthVec(v);
	zest_vec3 result = {
		.x = v.x / length,
		.y = v.y / length,
		.z = v.z / length
	};
	return result;
}

static inline zest_vec3 zest_CrossProduct(const zest_vec3 x, const zest_vec3 y)
{
	zest_vec3 result = {
		.x = x.y * y.z - y.y * x.z,
		.y = x.z * y.x - y.z * x.x,
		.z = x.x * y.y - y.x * x.y,
	};
	return result;
}

static inline float zest_DotProduct(const zest_vec3 a, const zest_vec3 b)
{
	return (a.x * b.x + a.y * b.y + a.z * b.z);
}

static inline zest_matrix4 zest_LookAt(const zest_vec3 eye, const zest_vec3 center, const zest_vec3 up)
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

static inline zest_matrix4 zest_Ortho(float left, float right, float bottom, float top, float z_near, float z_far)
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

// --Vulkan Buffer Management
typedef struct zest_buffer_info{
	VkImageUsageFlags image_usage_flags;
	VkBufferUsageFlags usage_flags;					//The usage state_flags of the memory block. 
	VkMemoryPropertyFlags property_flags;
	zest_buffer_flags flags;
	VkDeviceSize alignment;
	zest_uint memoryTypeBits;
} zest_buffer_info;

//A simple buffer struct for creating and mapping GPU memory
typedef struct zest_device_memory_pool{
	VkBuffer buffer;
	VkDeviceMemory memory;
	VkDeviceSize size;
	VkDeviceSize alignment;
	VkImageUsageFlags usage_flags;
	VkMemoryPropertyFlags property_flags;
	VkDescriptorBufferInfo descriptor;
	zest_uint memory_type_index;
	void* mapped;
	const char *name;
} zest_device_memory_pool;

typedef void* zest_pool_range;

typedef struct zest_buffer_allocator{
	VkBufferUsageFlags usage_flags;					//The usage state_flags of the memory block. 
	VkMemoryPropertyFlags property_flags;			//The property state_flags of the memory block.	
	tloc_allocator *allocator;
	tloc_size alignment;
	zest_device_memory_pool *memory_pools;
	zest_pool_range *range_pools;
} zest_buffer_allocator;

typedef struct zest_buffer{
	VkDeviceSize size;
	VkDeviceSize memory_offset;
	zest_index memory_pool;
	zest_buffer_allocator *buffer_allocator;
	void *data;
} zest_buffer;
// --End Vulkan Buffer Management

typedef struct zest_swapchain_support_details{
	VkSurfaceCapabilitiesKHR capabilities;
	VkSurfaceFormatKHR *formats;
	zest_uint formats_count;
	VkPresentModeKHR *present_modes;
	zest_uint present_modes_count;
} zest_swapchain_support_details;

typedef struct zest_window{
	GLFWwindow *window_handle;
	VkSurfaceKHR surface;
	zest_uint window_width;
	zest_uint window_height;
	zest_bool framebuffer_resized;
	zest_create_flags flags;
} zest_window;

typedef struct zest_device{
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
	zest_swapchain_support_details swapchain_support_details;
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
	VkDescriptorPoolSize *pool_counts;					//You can define descriptor pool counts here.

	//The starting sizes of various buffers. Whilst these will be resized as necessary, they are best set to a size so they have enough space and do not need to be resized at all.
	//Set output_memory_usage_on_exit to true so that you can determine the sizes as you develop your application
	//All values are the size in bytes
} zest_create_info;

typedef struct zest_app{
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

typedef struct zest_buffer_uploader{
	zest_bool initialised;					//Set to true once AddCopyCommand has been run at least once
	zest_bool source_is_fif;
	zest_bool target_is_fif;
	zest_index source_buffer_id;			//The id of the source memory allocation (cpu visible staging buffer)
	zest_index target_buffer_id;			//The id of the target memory allocation that we're uploading to (device local buffer)
	VkBufferCopy *buffer_copies;			//List of vulkan copy info commands to upload staging buffers to the gpu each frame
} zest_buffer_uploader;

typedef struct zest_semaphores{
	VkSemaphore present_complete;
	VkSemaphore render_complete;
} zest_semaphores;

typedef struct zest_frame_buffer_attachment{
	VkImage image;
	zest_buffer *buffer;
	VkImageView view;
	VkFormat format;
} zest_frame_buffer_attachment;

typedef struct zest_frame_buffer{
	zest_uint width, height;
	VkFormat format;
	VkFramebuffer frame_buffer;
	zest_frame_buffer_attachment color_buffer, depth_buffer, resolve_buffer;
	VkSampler sampler;
	zest_bool initialised;
} zest_frame_buffer;

//Struct to store all the necessary data for a render pass
typedef struct zest_render_pass_data{
	int width, height;
	VkFramebuffer frame_buffer;
	zest_frame_buffer_attachment color, depth;
	zest_index render_pass;
	VkSampler sampler;
	VkDescriptorImageInfo descriptor;
	VkCommandBuffer command_buffer;
	VkDescriptorSetLayout descriptor_layout;
	VkDescriptorSet descriptor_set;
	VkDescriptorSet descriptor_set_aa;
	VkPipeline pipeline;
	VkPipelineLayout pipeline_layout;
} zest_render_pass_data;

typedef struct FinalRenderPushConstants {
	zest_vec2 screen_resolution;			//the current size of the window/resolution
} zest_final_render_push_constants;

//command queues are the main thing you use to draw things to the screen. A simple app will create one for you, or you can create your own. See examples like PostEffects and also 
typedef struct zest_command_queue{
	const char *name;
	VkCommandPool command_pool;										//The command pool for command buffers
	VkCommandBuffer command_buffer[ZEST_MAX_FIF];					//A vulkan command buffer for each frame in flight
	VkSemaphore *fif_incoming_semaphores[ZEST_MAX_FIF];				//command queues need to be synchronises with other command queues and the swap chain so...
	VkSemaphore *fif_outgoing_semaphores[ZEST_MAX_FIF];				//an array of incoming and outgoing (wait and signal) semaphores are maintained for this purpose
	VkPipelineStageFlags *fif_stage_flags[ZEST_MAX_FIF];			//Stage state_flags relavent to the semaphores
	zest_uint *render_commands;										//A list of render commandsj indexes - mostly these will be render passes that are recorded to the command buffer
	zest_uint *compute_items;										//Compute items to be recorded to the command buffer
	zest_index index_in_renderer;									//A self reference of the index in the Renderer storage array for command queues
	zest_index present_semaphore_index;								//An index to the semaphore representing the swap chain if required. (command queues don't necessarily have to wait for the swap chain)
} zest_command_queue;

typedef struct {
	VkRenderPass render_pass;
	const char *name;
} zest_render_pass;

typedef struct zest_descriptor_set_layout {
	VkDescriptorSetLayout descriptor_layout;
	const char *name;
} zest_descriptor_set_layout;

typedef struct zest_uniform_buffer {
	zest_buffer *buffer[ZEST_MAX_FIF];
	VkDescriptorBufferInfo view_buffer_info[ZEST_MAX_FIF];
} zest_uniform_buffer;

typedef struct zest_uniform_buffer_data {
	zest_matrix4 view;
	zest_matrix4 proj;
	zest_vec4 parameters1;
	zest_vec4 parameters2;
	zest_vec2 screen_size;
	zest_uint millisecs;
} zest_uniform_buffer_data;

//Pipeline create template to setup the creation of a pipeline. Create this first and then use it with MakePipelineTemplate or SetPipelineTemplate to create a PipelineTemplate which you can then customise
//further if needed before calling CreatePipeline
typedef struct zest_pipeline_template_create_info{
	VkRect2D viewport;
	zest_index descriptorSetLayout;
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
} zest_pipeline_template_create_info;

//Pipeline template is used with CreatePipeline to create a vulkan pipeline. Use PipelineTemplate() or SetPipelineTemplate with PipelineTemplateCreateInfo to create a PipelineTemplate
typedef struct zest_pipeline_template{
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
} zest_pipeline_template;

//A pipeline set is all of the necessary things required to setup and maintain a pipeline
typedef struct zest_pipeline_set{
	zest_pipeline_template_create_info create_info;								//A copy of the create info and template is stored so that they can be used to update the pipeline later for any reason (like the swap chain is recreated)
	zest_pipeline_template pipeline_template;
	zest_index descriptor_layout;												//An index for the descriptor layout being used which is stored in the Renderer. Layouts can be reused an shared between pipelines
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
} zest_pipeline_set;

typedef struct zest_command_setup_context{
	zest_index command_queue_index;
	zest_index render_pass_index;
	zest_index draw_routine_index;
	zest_index layer_index;
	zest_index compute_index;
	zest_setup_context_type type;
} zest_command_setup_context ;

static inline void zest__reset_command_setup_context(zest_command_setup_context *context) {
	context->command_queue_index = -1;
	context->render_pass_index = -1;
	context->draw_routine_index = -1;
	context->layer_index = -1;
	context->type = zest_setup_context_type_none;
}

//A draw routine is used to actually draw things to the render target. Qulkan provides Layers that have a set of draw commands for doing this or you can develop your own
//By settting the callbacks and data pointers in the draw routine
typedef struct zest_draw_routine{
	const char *name;
	zest_index cq_index;														//The index of the render queue that this draw routine is within
	zest_index cq_render_pass_index;											//The index of the render pass within the command queue of this draw routine
	zest_index routine_id;														//The index of the draw routine within the renderer
	int draw_index;																//The user index of the draw routine. Use this to index the routine in your own lists. For Qulkan layers, this is used to hold the index of the layer in the renderer
	zest_index *command_queues;													//A list of the render queues that this draw routine belongs to
	void *data;																	//Pointer to some user data
	void(*update_buffers_callback)(void *draw_routine, zest_buffer_uploader *index_upload, zest_buffer_uploader *vertex_upload, zest_buffer_uploader *storage_upload);		//The callback used to update and upload the buffers before rendering
	void(*draw_callback)(void *draw_routine);						//draw callback called by the render target when rendering
	void(*update_resolution_callback)(void *draw_routine);			//Callback used when the window size changes
	void(*clean_up_callback)(void *draw_routine);					//Clean up function call back called when the draw routine is deleted
} zest_draw_routine;

//Every command queue will have either one or more render passes (unless it's purely for compute shading). Render pass contains various data for drawing things during the render pass.
//These can be draw routines that you can use to draw various things to the screen. You can set the render_pass_function to whatever you need to record the command buffer. See existing render pass functions such as:
//RenderDrawRoutine, RenderRenderTarget and RenderTargetToSwapChain.
typedef struct zest_command_queue_draw {
	VkFramebuffer(*get_frame_buffer)(void *item);
	void(*render_pass_function)(void *item, VkCommandBuffer command_buffer, zest_index render_pass, VkFramebuffer framebuffer);
	zest_index *draw_routines;
	zest_index *render_targets;
	VkRect2D viewport;
	zest_render_viewport_type viewport_type;
	float viewport_scale[2];
	zest_index render_pass;
	float cls_color[4];
	int render_target_index;
	const char *name;
} zest_command_queue_draw;

//In addition to render passes, you can also run compute shaders by adding this struct to the list of compute items in the command queue
typedef struct zest_command_queue_compute {
	void(*compute_function)(void *item);			//Call back function which will have the vkcommands the dispatch the compute shader task
	zest_index *compute_routines;										//List of QulkanCompute indexes so multiple compute shaders can be run in the same compute item
	const char *name;
} zest_command_queue_compute;

zest_hash_map(zest_command_queue) zest_map_command_queues;
zest_hash_map(zest_render_pass) zest_map_render_passes;
zest_hash_map(zest_descriptor_set_layout) zest_map_descriptor_layouts;
zest_hash_map(zest_pipeline_set) zest_map_pipeline_sets;
zest_hash_map(zest_command_queue_draw) zest_map_command_queue_render_passes;
zest_hash_map(zest_command_queue_compute) zest_map_command_queue_computes;
zest_hash_map(zest_draw_routine) zest_map_draw_routines;
zest_hash_map(zest_buffer_allocator) zest_map_buffer_allocators;
//zest_hash_map(QulkanLayer) layers;
//zest_hash_map(QulkanRenderTarget) render_targets;
//zest_hash_map(QulkanTexture) textures;
//zest_hash_map(QulkanFont) fonts;
zest_hash_map(zest_uniform_buffer) zest_map_uniform_buffers;
zest_hash_map(VkDescriptorSet) zest_map_descriptor_sets;

typedef struct zest_renderer{
	VkFormat swapchain_image_format;
	VkExtent2D swapchain_extent;
	VkSwapchainKHR swapchain;

	VkCommandPool present_command_pool;

	//Frames in flight storage
	VkFence fif_fence[ZEST_MAX_FIF];

	zest_semaphores semaphores[ZEST_MAX_FIF];

	//VkFence> images_in_flight_fences;
	VkPipelineStageFlags *stage_flags[ZEST_MAX_FIF];
	VkSemaphore *renderer_incoming_semaphores[ZEST_MAX_FIF];
	zest_index standard_uniform_buffer_id;

	VkImage *swapchain_images;
	VkImageView *swapchain_image_views;
	VkFramebuffer *swapchain_frame_buffers;

	zest_buffer *image_resource_buffer;
	zest_buffer *depth_resource_buffer;
	zest_frame_buffer *framebuffer;
	VkCommandBuffer *present_command_buffers;

	zest_render_pass_data final_render_pass;
	zest_final_render_push_constants push_constants;
	VkDescriptorBufferInfo view_buffer_info[ZEST_MAX_FIF];

	zest_map_buffer_allocators buffer_allocators;									//For non frame in flight buffers

	//Context data
	VkCommandBuffer current_command_buffer;
	zest_command_queue *current_command_queue;
	zest_command_queue_draw *current_render_pass;
	zest_command_queue_compute *current_compute_routine;

	//General Storage
	zest_map_command_queues command_queues;
	zest_map_render_passes render_passes;
	zest_map_descriptor_layouts descriptor_layouts;
	zest_map_pipeline_sets pipeline_sets;
	zest_map_command_queue_render_passes command_queue_render_passes;
	zest_map_command_queue_computes command_queue_computes;
	zest_map_draw_routines draw_routines;
	//zest_map_layers layers;
	//zest_map_render_targets render_targets;
	//zest_map_textures textures;
	//zest_map_fonts fonts;
	zest_map_uniform_buffers uniform_buffers;
	zest_map_descriptor_sets descriptor_sets;

	zest_index *frame_queues;
	zest_command_queue empty_queue;
	VkDescriptorPool descriptor_pool;
	zest_command_setup_context setup_context;

	void *user_uniform_data;
	void(*update_uniform_buffer_callback)(void *user_uniform_data);							//Function callback where you can update a uniform buffer

	zest_index *free_layers;
	zest_index *free_drawroutines;

	zest_index current_rt_index;
	zest_index root_rt_index;
	zest_index *render_target_recreate_queue;
	zest_index *rt_sampler_refresh_queue[ZEST_MAX_FIF];
	zest_index *texture_refresh_queue[ZEST_MAX_FIF];
	zest_index *texture_reprocess_queue[ZEST_MAX_FIF];
	zest_uint current_frame;

	//Flags
	zest_renderer_flags flags;
} zest_renderer;

static zest_device *ZestDevice = 0;
static zest_app *ZestApp = 0;
static zest_renderer *ZestRenderer = 0;

static const char* zest_validation_layers[zest__validation_layer_count] = {
	"VK_LAYER_KHRONOS_validation"
};

static const char* zest_required_extensions[zest__required_extension_names_count] = {
	VK_KHR_SWAPCHAIN_EXTENSION_NAME
};

//--Internal functions
inline void* zest__vec_reserve(void *T, zest_uint unit_size, zest_uint new_capacity) { if (T && new_capacity <= zest__vec_header(T)->capacity) return T; void* new_data = ZEST__REALLOCATE((T ? zest__vec_header(T) : T), new_capacity * unit_size + zest__VEC_HEADER_OVERHEAD); if (!T) memset(new_data, 0, zest__VEC_HEADER_OVERHEAD); T = ((char*)new_data + zest__VEC_HEADER_OVERHEAD); ((zest_vec*)(new_data))->capacity = new_capacity; return T; }

//Buffer Management
VkResult zest__bind_memory(zest_device_memory_pool *memory_allocation, VkDeviceSize offset);
VkResult zest__map_memory(zest_device_memory_pool *memory_allocation, VkDeviceSize size, VkDeviceSize offset);
void zest__unmap_memory(zest_device_memory_pool *memory_allocation);
void zest__destroy_memory(zest_device_memory_pool *memory_allocation);
VkResult zest__flush_memory(zest_device_memory_pool *memory_allocation, VkDeviceSize size, VkDeviceSize offset);
//End Buffer Management

//Renderer functions
void zest__initialise_renderer(zest_create_info *create_info);
void zest__create_swapchain(void);
VkSurfaceFormatKHR zest__choose_swapchain_format(VkSurfaceFormatKHR *availableFormats);
VkPresentModeKHR zest_choose_present_mode(VkPresentModeKHR *available_present_modes, zest_bool use_vsync);
VkExtent2D zest_choose_swap_extent(VkSurfaceCapabilitiesKHR *capabilities);
void zest__cleanup_swapchain(void);
void zest__cleanup_renderer(void);
void zest__add_push_constant(zest_pipeline_template_create_info *create_info, VkPushConstantRange push_constant);
void zest__add_draw_routine(zest_command_queue_draw *command_queue_draw, zest_draw_routine *draw_routine);
void zest__create_swapchain_image_views(void);
void zest__make_standard_render_passes(void);
zest_uint zest__add_render_pass(const char *name, VkRenderPass render_pass);
zest_buffer *zest__create_depth_resources(void);
void zest__create_frame_buffers(void);
void zest__create_sync_objects(void);
zest_index zest_create_uniform_buffer(const char *name, zest_size uniform_struct_size);
zest_index zest_add_uniform_buffer(const char *name, zest_uniform_buffer *buffer);
void zest_update_uniform_buffer(void *user_uniform_data);
void zest__create_renderer_command_pools(void);
void zest__create_descriptor_pools(VkDescriptorPoolSize *pool_sizes);
void zest__make_standard_descriptor_layouts(void);
void zest__prepare_standard_pipelines(void);
void zest__create_final_render_command_buffer(void);
void zest__rerecord_final_render_command_buffer(void);
void zest__create_empty_command_queue(zest_command_queue *command_queue);
void zest__render_blank(zest_command_queue_draw *item, VkCommandBuffer command_buffer, zest_index render_pass, VkFramebuffer framebuffer);
void zest__destroy_pipeline_set(zest_pipeline_set *p);
void zest__cleanup_pipelines();
void zest__rebuild_pipeline(zest_pipeline_set *pipeline);
void zest__present_frame();
// --End Renderer functions

// --Command Queue functions
void zest__cleanup_command_queue(zest_command_queue *command_queue);
VkSemaphore zest__get_command_queue_present_semaphore(zest_command_queue *command_queue);
void zest__record_and_commit_command_queue(zest_command_queue *command_queue, VkFence fence);
// --Command Queue functions


// --General Helper Functions
VkImageView zest__create_image_view(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags, zest_uint mipLevels, VkImageViewType viewType, zest_uint layerCount);
zest_buffer *zest__create_image(zest_uint width, zest_uint height, zest_uint mipLevels, VkSampleCountFlagBits numSamples, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkImage *image);
void zest__transition_image_layout(VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout, zest_uint mipLevels, zest_uint layerCount);
VkRenderPass zest__create_render_pass(VkFormat render_format, VkImageLayout final_layout, VkAttachmentLoadOp load_op);
VkFormat zest__find_depth_format(void);
zest_bool zest__has_stencil_format(VkFormat format);
VkFormat zest__find_supported_format(VkFormat *candidates, zest_uint candidates_count, VkImageTiling tiling, VkFormatFeatureFlags features);
VkCommandBuffer zest__begin_single_time_commands(void);
void zest__end_single_time_commands(VkCommandBuffer command_buffer);
char* zest_ReadEntireFile(const char *file_name, zest_bool terminate);
// --End General Helper Functions

// --Buffer allocation funcitons
void zest__create_device_memory_pool(VkDeviceSize size, VkBufferUsageFlags usage_flags, VkMemoryPropertyFlags property_flags, zest_device_memory_pool *buffer, const char *name);
void zest__create_image_memory_pool(VkDeviceSize size_in_bytes, VkImage image, VkMemoryPropertyFlags property_flags, zest_device_memory_pool *buffer);
void zest_ConnectCommandQueueToPresent(zest_command_queue *sender, VkPipelineStageFlags stage_flags);
tloc_size zest__get_bytes_per_block(tloc_size pool_size);
tloc_size zest__get_remote_size(tloc_header *block);
void zest__on_add_pool(void *user_data, void *block);
void zest__on_merge_next(void *user_data, tloc_header *block, tloc_header *next_block);
void zest__on_merge_prev(void *user_data, tloc_header *prev_block, tloc_header *block);
void zest__on_split_block(void *user_data, tloc_header* block, tloc_header *trimmed_block, tloc_size remote_size);
// --End Buffer allocation funcitons

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
zest_swapchain_support_details zest__query_swapchain_support(VkPhysicalDevice physical_device);
VkSampleCountFlagBits zest__get_max_useable_sample_count(void);
void zest__create_logical_device(void);
void zest__set_limit_data(void);
zest_bool zest__check_validation_layer_support(void);
const char** zest__get_required_extensions(zest_uint *extension_count);
zest_uint zest_find_memory_type(zest_uint typeFilter, VkMemoryPropertyFlags properties);
//end device setup functions

//App initialise functions
void zest__initialise_app(zest_create_info *create_info);
void zest__initialise_device(void);
void zest__destroy(void);
zest_window* zest__create_window(int x, int y, int width, int height, zest_bool maximised, const char*);
void zest__create_window_surface(zest_window*);
void zest__main_loop(void);
void zest__keyboard_input_callback(GLFWwindow* window, int key, int scancode, int action, int mods);
void zest__mouse_scroll_callback(GLFWwindow* window, double offset_x, double offset_y);
void zest__framebuffer_size_callback(GLFWwindow* window, int width, int height);
//-- end of internal functions

//-- Window related functions
void zest__update_window_size(zest_window* window, zest_uint width, zest_uint height);
//-- End Window related functions

//User API functions
ZEST_API zest_create_info zest_CreateInfo(void);
ZEST_API void zest_Initialise(zest_create_info *info);
ZEST_API void zest_Start(void);
ZEST_API zest_uniform_buffer *zest_GetUniformBuffer(zest_index index);
ZEST_API zest_uniform_buffer *zest_GetUniformBufferByName(const char *name);
ZEST_API zest_bool zest_UniformBufferExists(const char *name);
ZEST_API zest_index zest_AddDescriptorLayout(const char *name, VkDescriptorSetLayout layout);
ZEST_API VkDescriptorSetLayout zest_CreateDescriptorSetLayout(zest_uint uniforms, zest_uint samplers, zest_uint storage_buffers);
ZEST_API VkDescriptorSetLayoutBinding zest_CreateUniformLayoutBinding(zest_uint binding);
ZEST_API VkDescriptorSetLayoutBinding zest_CreateSamplerLayoutBinding(zest_uint binding);
ZEST_API VkDescriptorSetLayoutBinding zest_CreateStorageLayoutBinding(zest_uint binding);
ZEST_API VkDescriptorSetLayout zest_CreateDescriptorSetLayoutWithBindings(VkDescriptorSetLayoutBinding *bindings);
ZEST_API VkViewport zest_CreateViewport(float width, float height, float minDepth, float maxDepth);
ZEST_API VkRect2D zest_CreateRect2D(zest_uint width, zest_uint height, int offsetX, int offsetY);
//Pipeline related 
ZEST_API void zest_BuildPipeline(zest_pipeline_set *pipeline);
ZEST_API void zest_UpdatePipelineTemplate(zest_pipeline_template *pipeline_template, zest_pipeline_template_create_info *create_info);
ZEST_API void zest_SetPipelineTemplate(zest_pipeline_template *pipeline_template, zest_pipeline_template_create_info *create_info);
ZEST_API void zest_MakePipelineTemplate(zest_pipeline_set *pipeline, VkRenderPass render_pass, zest_pipeline_template_create_info *create_info);
ZEST_API VkShaderModule zest_CreateShaderModule(char *code);
ZEST_API zest_pipeline_template_create_info zest_CreatePipelineTemplateCreateInfo(void);
ZEST_API VkPipelineColorBlendAttachmentState zest_AdditiveBlendState();
ZEST_API VkPipelineColorBlendAttachmentState zest_AlphaOnlyBlendState();
ZEST_API VkPipelineColorBlendAttachmentState zest_AlphaBlendState();
ZEST_API VkPipelineColorBlendAttachmentState zest_PreMultiplyBlendState();
ZEST_API VkPipelineColorBlendAttachmentState zest_PreMultiplyBlendStateForSwap();
ZEST_API VkPipelineColorBlendAttachmentState zest_MaxAlphaBlendState();
ZEST_API VkPipelineColorBlendAttachmentState zest_ImGuiBlendState();
//Buffer related
ZEST_API zest_buffer *zest_CreateBuffer(VkDeviceSize size, zest_buffer_info *buffer_info, VkImage image, VkDeviceSize pool_size);
ZEST_API void zest_FreeBuffer(zest_buffer *buffer);
ZEST_API VkDeviceMemory zest_GetBufferDeviceMemory(zest_buffer *buffer) { return buffer->buffer_allocator->memory_pools[buffer->memory_pool].memory; }

//Inline API functions
VkFramebuffer zest_GetRendererFrameBuffer(zest_command_queue_draw *item) { return ZestRenderer->swapchain_frame_buffers[ZestRenderer->current_frame]; }
ZEST_API VkDescriptorSetLayout *zest_GetDescriptorSetLayoutByIndex(zest_index index) { return zest_map_at_index(ZestRenderer->descriptor_layouts, index).descriptor_layout; }
ZEST_API VkDescriptorSetLayout *zest_GetDescriptorSetLayoutByName(const char *name) { return zest_map_at(ZestRenderer->descriptor_layouts, name).descriptor_layout; }
ZEST_API inline VkRenderPass zest_GetRenderPassByIndex(zest_index index) { ZEST_ASSERT(zest_map_valid_index(ZestRenderer->render_passes, index)); return *zest_map_at_index(ZestRenderer->render_passes, index).render_pass; }
ZEST_API inline VkRenderPass zest_GetRenderPassByName(const char *name) { ZEST_ASSERT(zest_map_valid_name(ZestRenderer->render_passes, name)); return *zest_map_at(ZestRenderer->render_passes, name).render_pass; }
ZEST_API inline VkRenderPass zest_GetStandardRenderPass() { return *zest_map_at(ZestRenderer->render_passes, "Render pass standard").render_pass; }
ZEST_API inline zest_pipeline_set zest_CreatePipelineSet(void);
ZEST_API inline zest_pipeline_set *zest_PipelineByIndex(zest_index index) { ZEST_ASSERT(zest_map_valid_index(ZestRenderer->pipeline_sets, index)); return zest_map_at_index(ZestRenderer->pipeline_sets, index); }
ZEST_API inline zest_pipeline_set *zest_PipelineByName(const char *name) { ZEST_ASSERT(zest_map_valid_name(ZestRenderer->pipeline_sets, name)); return zest_map_at(ZestRenderer->pipeline_sets, name); }
ZEST_API inline zest_pipeline_template_create_info zest_PipelineCreateInfo(const char *name) { ZEST_ASSERT(zest_map_valid_name(ZestRenderer->pipeline_sets, name)); zest_pipeline_set *pipeline = zest_map_at(ZestRenderer->pipeline_sets, name); return pipeline->create_info; }
ZEST_API inline zest_index zest_AddPipeline(const char *name) { zest_map_insert(ZestRenderer->pipeline_sets, name, zest_CreatePipelineSet()); return zest_map_last_index(ZestRenderer->pipeline_sets); }
VkExtent2D zest_GetSwapChainExtent() { return ZestRenderer->swapchain_extent; }
ZEST_API inline zest_uint zest_ScreenWidth() { return ZestApp->window->window_width; }
ZEST_API inline zest_uint zest_ScreenHeight() { return ZestApp->window->window_height; }
ZEST_API inline float zest_ScreenWidthf() { return (float)ZestApp->window->window_width; }
ZEST_API inline float zest_ScreenHeightf() { return (float)ZestApp->window->window_height; }
ZEST_API inline zest_uniform_buffer *zest_GetUniformBuffer(zest_index index) { ZEST_ASSERT(zest_map_valid_index(ZestRenderer->uniform_buffers, index)); return zest_map_at_index(ZestRenderer->uniform_buffers, index); }
ZEST_API inline zest_uniform_buffer *zest_GetUniformBufferByName(const char *name) { ZEST_ASSERT(zest_map_valid_name(ZestRenderer->uniform_buffers, name)); return zest_map_at(ZestRenderer->uniform_buffers, name); }
ZEST_API inline zest_bool zest_UniformBufferExists(const char *name) { return zest_map_valid_name(ZestRenderer->uniform_buffers, name); }
ZEST_API inline void zest_WaitForIdleDevice() { vkDeviceWaitIdle(ZestDevice->logical_device); }
#ifdef _WIN32
zest_millisecs inline zest_Millisecs(void) { FILETIME ft; GetSystemTimeAsFileTime(&ft); ULARGE_INTEGER time; time.LowPart = ft.dwLowDateTime; time.HighPart = ft.dwHighDateTime; zest_ull ms = time.QuadPart / 10000ULL; return (zest_millisecs)ms; }
zest_microsecs inline zest_Microsecs(void) { FILETIME ft; GetSystemTimeAsFileTime(&ft); ULARGE_INTEGER time; time.LowPart = ft.dwLowDateTime; time.HighPart = ft.dwHighDateTime; zest_ull us = time.QuadPart / 10ULL; return (zest_microsecs)us; }
#else
zest_millisecs inline zest_Millisecs(void) { struct timespec now; clock_gettime(CLOCK_REALTIME, &now); zest_uint m = now.tv_sec * 1000 + now.tv_nsec / 1000000; return (zest_millisecs)m; }
zest_microsecs inline zest_Microsecs(void) { struct timespec now; clock_gettime(CLOCK_REALTIME, &now); zest_ull us = now.tv_sec * 1000000ULL + now.tv_nsec / 1000; return (zest_microsecs)us; }
#endif
#endif // ! ZEST_POCKET_RENDERER
