//Zest - A Lightweight Renderer
#ifndef ZEST_RENDERER_H
#define ZEST_RENDERER_H

/*
    Zest Pocket Renderer

    Sections in this header file, you can search for the following keywords to jump to that section :
 * 
	[About_Zest]
	[Zloc_header - Zloc_implementation] TLSF allocator. Handles all allocations in Zest.
    [Macro_Defines]                     Just a bunch of typedefs and macro definitions
    [Typedefs_for_numbers]          
    [Platform_specific_code]            Windows/Mac specific, no linux yet
    [Shader_code]                       glsl shader code for all the built in shaders
    [Enums_and_flags]                   Enums and bit flag definitions
        [pipeline_enums]
    [Forward_declarations]              Forward declarations for structs and setting up of handles
    [Pocket_dynamic_array]              Simple dynamic array
    [Pocket_bucket_array]               Simple bucket array
    [Pocket_Hasher]                     XXHash code for use in hash map
    [Pocket_hash_map]                   Simple hash map
    [Pocket_text_buffer]                Very simple struct and functions for storing strings
    [Threading]                         Some thread helpers, unused as yet.
    [Structs]                           All the structs are defined here.
        [Vectors]
        [frame_graph_types]
	[Platform_callbacks_struct]
    [Internal_functions]                Private functions only, no API access
		[Platform_dependent_functions]
        [Buffer_and_Memory_Management]
        [Queue_management]
        [Renderer_functions]
        [Draw_layer_internal_functions]
        [Image_internal_functions]
        [General_layer_internal_functions]
        [Mesh_layer_internal_functions]
        [Misc_Helper_Functions]
        [Pipeline_Helper_Functions]
        [Maintenance_functions]
        [Descriptor_set_functions]
        [Device_set_up]
        [App_initialise_and_run_functions]
		[Enum_to_string_functions]
		[Internal_render_graph_functions]

    --API functions
    [Essential_setup_functions]         Functions for initialising Zest
    [Platform_Helper_Functions]         Just general helper functions for working with the platform APIs if you're doing more custom stuff
	[External_lib_helpers]				Helper fuctions for setting up a device and window with GLFW, SDL2
    [Pipeline_related_helpers]   		Helper functions for setting up your own pipeline_templates
    [Platform_dependent_callbacks]      These are used depending one whether you're using glfw, sdl or just the os directly
    [Buffer_functions]                  Functions for creating and using gpu buffers
	[Frame_graph_api]					API functions relating to setting up and utilising a frame graph
    [General_Math_helper_functions]     Vector and matrix math functions
    [Camera_helpers]                    Functions for setting up and using a camera including frustom and screen ray etc. 
    [Images_and_textures]               Load and setup images for using in textures accessed on the GPU
    [Swapchain_helpers]                 General swapchain helpers to get, set clear color etc.
    [Draw_Layers_API]                   General helper functions for layers
    [Draw_mesh_layers]                  Functions for drawing meshes
    [Draw_instance_mesh_layers]         Functions for drawing instances of meshes
    [Compute_shaders]                   Functions for setting up your own custom compute shaders
    [Events_and_States]                 Just one function for now
    [Timer_functions]                   High resolution timer functions
    [General_Helper_functions]          General API functions - get screen dimensions etc 
    [Debug_Helpers]                     Functions for debugging and outputting queues to the console.
  	[Command_buffer_functions]          GPU command buffer commands

 	--Implementations 
 	[Zloc_allocator] 
 	[Zest_implementation]

--- [About_Zest]

A lightweight single header library + platform layer, so for example you would include zest.h and then the 
platform that you want to use such as zest_vulkan.h. The aim of this library is to provide the minimal API
to execute things on the GPU and handle the tedious tasks of synchronization, barriers, image transitions etc.
This is for programmers that don't want bloat and features that they won't need, rather they can code their own 
things without a complex renderer getting in the way.

There is also a zest_utilities.h file which is more of an example file that is used by all the examples to 
show how you can implement various things such as loading images, fonts, opening windows with sdl or glfw etc. 
You don't have to use this file but it can help with just getting started with the library.

This is a more forward looking library in that Vulkan 1.2 is the minimum requirement and the GPU must be able to 
handle bindless descriptor sets and dynamic rendering and so it's more for desktop rendering, this is not a renderer
for mobiles for now although it will more going in to the future.

Core Architecture & Features
* 	Separate platform layer so that the API can be compiled with Vulkan/DX12/Metal. Currently only Vulkan exists 
	in the API in the file zest_vulkan.h
*	Two main objects are create at startup, a "Device" which handles the platform layer of the rendering
	(Vulkan, DX12, Metal) and then Contexts which handle windows swap chains and frame graphs. Each window that you 
	open will have it's own context so there is essentially a one to many relationship between the Device and 
	Contexts.
* 	Dedicated Two Level Segregated Fit (TLSF) Allocator to manage both Host memory and also GPU Memory for speed and 
	minimal fragmentation. A linear allocator is also used for short lifetime memory. This means that memory 
	pools are allocated infrequently from the system and then the TLSF sub-allocates from that pool. This 
	allocator also handles memory allocations on the GPU as well in a similar way (pools and sub-allocations).
* 	Frame graph that allows you to declare resources, passes and inputs and outputs to those passes. The frame 
	graph is compiled and executed with necessary semaphores, barriers, image transitions and transient resource 
	creation/freeing. 
	It also makes use of async where it can, otherwise everything is merged on to the graphics queue 
	to optimize queue ownership and transfer of resources. All of the memory for the frame graph has a lifetime 
	of a single frame using a linear allocator located in the Context that the frame graph belongs to. 

	Frame graphs also create transient buffers and images and use the TLSF allocator to for memory aliasing.
	This means that any resource that is used in a frame and gets freed, that memory can then be immediately 
	re-used in the same frame to save memory.

	Frame graphs can also be cached so that they don't have to be compiled each frame.
* 	Descriptor sets are all bindless and managed with a single global descriptor layout stored in the device. This
	means that this global set is bound once in each command buffer and that's it, you can just index into the
	descriptor array that you need.
* 	Render passes are handled using dynamic render passes.
* 	Simple to use API for creating pipelines, compute shader pipelines and GPU resources like buffers and images.
* 	It also includes a separate implementation for Dear Imgui so it can be easily added to a project.
* 	Shaderc is used to compile shaders with Vulkan and there's also an implementation for slang 
	(see zest-compute-example).
*/

#define ZEST_OUTPUT_WARNING_MESSAGES
#define ZLOC_THREAD_SAFE
//#define ZLOC_EXTRA_DEBUGGING
//#define ZLOC_OUTPUT_ERROR_MESSAGES
#define ZLOC_SAFEGUARDS

//Zloc_header
#define zloc__Min(a, b) (((a) < (b)) ? (a) : (b))
#define zloc__Max(a, b) (((a) > (b)) ? (a) : (b))

#ifndef ZLOC_API
#define ZLOC_API
#endif

typedef int zloc_index;
typedef unsigned int zloc_sl_bitmap;
typedef unsigned int zloc_uint;
typedef unsigned int zloc_thread_access;
typedef int zloc_bool;
typedef void* zloc_pool;

#include <stdio.h>		//For printf mainly and loading files
#include <stdint.h>		//For uint32_t etc.
#include <stddef.h>		//For uint32_t etc.
#include <string.h>		//For memcpy, memset etc.
#include <stdarg.h>		//For va_start, va_end etc.
#include <math.h>
#if !defined (ZLOC_ASSERT)
#include <assert.h>
#define ZLOC_ASSERT assert
#endif

#define zloc__is_pow2(x) ((x) && !((x) & ((x) - 1)))
#define zloc__glue2(x, y) x ## y
#define zloc__glue(x, y) zloc__glue2(x, y)
#define zloc__static_assert(exp) \
typedef char zloc__glue(static_assert, __LINE__) [(exp) ? 1 : -1]

#if (defined(_MSC_VER) && defined(_M_X64)) || defined(__x86_64__)
#define zloc__64BIT
typedef size_t zloc_size;
typedef size_t zloc_fl_bitmap;
#define ZLOC_ONE 1ULL
#else
typedef size_t zloc_size;
typedef size_t zloc_fl_bitmap;
#define ZLOC_ONE 1U
#endif

#ifndef MEMORY_ALIGNMENT_LOG2
#if defined(zloc__64BIT)
#define MEMORY_ALIGNMENT_LOG2 3		//64 bit
#else
#define MEMORY_ALIGNMENT_LOG2 2		//32 bit
#endif
#endif

#ifndef ZLOC_ERROR_NAME
#define ZLOC_ERROR_NAME "Allocator Error"
#endif

#ifndef ZLOC_ERROR_COLOR
#define ZLOC_ERROR_COLOR "\033[31m"
#endif

//Redo this and output to a user defined log file instead
#ifdef ZLOC_OUTPUT_ERROR_MESSAGES
#define ZLOC_PRINT_ERROR(message_f, ...) printf(message_f"\033[0m", __VA_ARGS__)
#else
#define ZLOC_PRINT_ERROR(message_f, ...)
#endif

#define zloc__KILOBYTE(Value) ((Value) * 1024LL)
#define zloc__MEGABYTE(Value) (zloc__KILOBYTE(Value) * 1024LL)
#define zloc__GIGABYTE(Value) (zloc__MEGABYTE(Value) * 1024LL)

#ifndef ZLOC_MAX_SIZE_INDEX
#if defined(zloc__64BIT)
#define ZLOC_MAX_SIZE_INDEX 32
#else
#define ZLOC_MAX_SIZE_INDEX 30
#endif
#endif

zloc__static_assert(ZLOC_MAX_SIZE_INDEX < 64);

#ifdef __cplusplus
extern "C" {
#endif

#define zloc__MAXIMUM_BLOCK_SIZE (ZLOC_ONE << ZLOC_MAX_SIZE_INDEX)

enum zloc__constants {
	zloc__MEMORY_ALIGNMENT = 1 << MEMORY_ALIGNMENT_LOG2,
	zloc__SECOND_LEVEL_INDEX_LOG2 = 5,
	zloc__FIRST_LEVEL_INDEX_COUNT = ZLOC_MAX_SIZE_INDEX,
	zloc__SECOND_LEVEL_INDEX_COUNT = 1 << zloc__SECOND_LEVEL_INDEX_LOG2,
	#ifdef ZLOC_SAFEGUARDS
	zloc__BLOCK_POINTER_OFFSET = sizeof(void*) * 2 + sizeof(zloc_size) * 2,
	zloc__BLOCK_SIZE_OVERHEAD = sizeof(zloc_size) * 2 + sizeof(void*),
	#else
	zloc__BLOCK_POINTER_OFFSET = sizeof(void*) + sizeof(zloc_size),
	zloc__BLOCK_SIZE_OVERHEAD = sizeof(zloc_size),
	#endif
	zloc__MINIMUM_BLOCK_SIZE = 16,
	zloc__POINTER_SIZE = sizeof(void*),
	zloc__SMALLEST_CATEGORY = (1 << (zloc__SECOND_LEVEL_INDEX_LOG2 + MEMORY_ALIGNMENT_LOG2))
};

typedef enum zloc__boundary_tag_flags {
	zloc__BLOCK_IS_FREE = 1 << 0,
	zloc__PREV_BLOCK_IS_FREE = 1 << 1,
} zloc__boundary_tag_flags;

typedef enum zloc__error_codes {
	zloc__OK,
	zloc__INVALID_FIRST_BLOCK,
	zloc__INVALID_BLOCK_FOUND,
	zloc__PHYSICAL_BLOCK_MISALIGNMENT,
	zloc__INVALID_SEGRATED_LIST,
	zloc__WRONG_BLOCK_SIZE_FOUND_IN_SEGRATED_LIST,
	zloc__SECOND_LEVEL_BITMAPS_NOT_INITIALISED
} zloc__error_codes;

typedef enum zloc__thread_ops {
	zloc__FREEING_BLOCK = 1 << 0,
	zloc__ALLOCATING_BLOCK = 1 << 1
} zloc__thread_ops;

/*
	Each block has a header that if used only has a pointer to the previous physical block
	and the size. If the block is free then the prev and next free blocks are also stored.
*/
typedef struct zloc_header {
	struct zloc_header *prev_physical_block;
	/*	Note that the size is either 4 or 8 bytes aligned so the boundary tag (2 flags denoting
		whether this or the previous block is free) can be stored in the first 2 least
		significant bits	*/
	zloc_size size;
	#ifdef ZLOC_SAFEGUARDS
	struct zloc_allocator *allocator;
	zloc_size magic;
	#endif
	/*
	User allocation will start here when the block is used. When the block is free prev and next
	are pointers in a linked list of free blocks within the same class size of blocks
	*/
	struct zloc_header *prev_free_block;
	struct zloc_header *next_free_block;
} zloc_header;

typedef struct zloc_allocation_stats_t {
	zloc_size capacity;
	zloc_size free;
	int blocks_in_use;
	int free_blocks;
} zloc_allocation_stats_t;

typedef struct zloc_allocator {
	/*	This is basically a terminator block that free blocks can point to if they're at the end
		of a free list. */
	zloc_header null_block;
	#if defined(ZLOC_THREAD_SAFE)
	/* Multithreading protection*/
	volatile zloc_thread_access access;
	#endif
	void *remote_user_data;
	zloc_size(*get_block_size_callback)(const zloc_header* block);
	void(*merge_next_callback)(void *remote_user_data, zloc_header* block, zloc_header *next_block);
	void(*merge_prev_callback)(void *remote_user_data, zloc_header* prev_block, zloc_header *block);
	void(*split_block_callback)(void *remote_user_data, zloc_header* block, zloc_header* trimmed_block, zloc_size remote_size);
	void(*add_pool_callback)(void *remote_user_data, void* block_extension);
	void(*unable_to_reallocate_callback)(void *remote_user_data, zloc_header *block, zloc_header *new_block);
	zloc_size block_extension_size;
	void *user_data;
	zloc_size minimum_allocation_size;
	zloc_size allocated_size;
	/*	Here we store all of the free block data. first_level_bitmap is either a 32bit int
	or 64bit depending on whether zloc__64BIT is set. Second_level_bitmaps are an array of 32bit
	ints. segregated_lists is a two level array pointing to free blocks or null_block if the list
	is empty. */
	zloc_fl_bitmap first_level_bitmap;
	zloc_sl_bitmap second_level_bitmaps[zloc__FIRST_LEVEL_INDEX_COUNT];
	zloc_header *segregated_lists[zloc__FIRST_LEVEL_INDEX_COUNT][zloc__SECOND_LEVEL_INDEX_COUNT];
	zloc_allocation_stats_t stats;
} zloc_allocator;

/*
A minimal remote header block. You can define your own header to store additional information but it must include
"zloc_size" size and memory_offset in the first 2 fields.
*/
typedef struct zloc_remote_header {
	zloc_size size;
	zloc_size memory_offset;
} zloc_remote_header;

typedef struct zloc_pool_stats_t {
	int used_blocks;
	int free_blocks;
	zloc_size free_size;
	zloc_size used_size;
} zloc_pool_stats_t;

#define zloc__map_size (remote_size ? remote_size : size)
#define zloc__do_size_class_callback(block) allocator->get_block_size_callback(block)
#define zloc__do_merge_next_callback allocator->merge_next_callback(allocator->remote_user_data, block, next_block)
#define zloc__do_merge_prev_callback allocator->merge_prev_callback(allocator->remote_user_data, prev_block, block)
#define zloc__do_split_block_callback allocator->split_block_callback(allocator->remote_user_data, block, trimmed, remote_size)
#define zloc__do_add_pool_callback allocator->add_pool_callback(allocator->remote_user_data, block)
#define zloc__do_unable_to_reallocate_callback zloc_header *new_block = zloc__block_from_allocation(allocation); zloc_header *block = zloc__block_from_allocation(ptr); allocator->unable_to_reallocate_callback(allocator->remote_user_data, block, new_block)
#define zloc__block_extension_size (allocator->block_extension_size & ~1)
#define zloc__call_maybe_split_block zloc__maybe_split_block(allocator, block, size, remote_size)

#if defined (_MSC_VER) && (_MSC_VER >= 1400) && (defined (_M_IX86) || defined (_M_X64))
/* Microsoft Visual C++ support on x86/X64 architectures. */

#include <intrin.h>

static inline int zloc__scan_reverse(zloc_size bitmap) {
	unsigned long index;
	#if defined(zloc__64BIT)
	return _BitScanReverse64(&index, bitmap) ? index : -1;
	#else
	return _BitScanReverse(&index, bitmap) ? index : -1;
	#endif
}

static inline unsigned int zloc__count_bits(unsigned int number) {
	return __popcnt(number);
}

static inline int zloc__scan_forward(zloc_size bitmap)
{
	unsigned long index;
	#if defined(zloc__64BIT)
	return _BitScanForward64(&index, bitmap) ? index : -1;
	#else
	return _BitScanForward(&index, bitmap) ? index : -1;
	#endif
}

#ifdef _WIN32
#include <Windows.h>
static inline zloc_thread_access zloc__compare_and_exchange(volatile zloc_thread_access* target, zloc_thread_access value, zloc_thread_access original) {
	return InterlockedCompareExchange(target, value, original);
}
#endif

#elif defined(__GNUC__) && ((__GNUC__ > 4) || (__GNUC__ == 4 && __GNUC_MINOR__ >= 8)) && \
(defined(__i386__) || defined(__x86_64__)) || defined(__clang__)
/* GNU C/C++ or Clang support on x86/x64 architectures. */

static inline int zloc__scan_reverse(zloc_size bitmap)
{
	if (bitmap == 0) return -1;
	#if defined(zloc__64BIT)
	return 64 - __builtin_clzll(bitmap) - 1;
	#else
	return 32 - __builtin_clz((int)bitmap) - 1;
	#endif
}

static inline unsigned int zloc__count_bits(unsigned int number) {
	return __builtin_popcount(number);
}

static inline int zloc__scan_forward(zloc_size bitmap)
{
	#if defined(zloc__64BIT)
	return __builtin_ffsll(bitmap) - 1;
	#else
	return __builtin_ffs((int)bitmap) - 1;
	#endif
}

static inline zloc_thread_access zloc__compare_and_exchange(volatile zloc_thread_access* target, zloc_thread_access value, zloc_thread_access original) {
	return __sync_val_compare_and_swap(target, original, value);
}
#elif

static inline unsigned int zloc__count_bits(unsigned int n) {
	unsigned int count = 0;
	while (n > 0) {
		n &= (n - 1);
		count++;
	}
	return count;
}

#endif

ZLOC_API zloc_allocator *zloc_InitialiseAllocator(void *memory);
ZLOC_API zloc_allocator *zloc_InitialiseAllocatorWithPool(void *memory, zloc_size size);
ZLOC_API zloc_pool *zloc_AddPool(zloc_allocator *allocator, void *memory, zloc_size size);
ZLOC_API zloc_size zloc_AllocatorSize(void);
ZLOC_API zloc_pool *zloc_GetPool(zloc_allocator *allocator);
ZLOC_API void *zloc_Allocate(zloc_allocator *allocator, zloc_size size);
ZLOC_API void *zloc_Reallocate(zloc_allocator *allocator, void *ptr, zloc_size size);
ZLOC_API void *zloc_AllocateAligned(zloc_allocator *allocator, zloc_size size, zloc_size alignment);
ZLOC_API int zloc_Free(zloc_allocator *allocator, void *allocation);
ZLOC_API void* zloc_PromoteLinearBlock(zloc_allocator *allocator, void* linear_alloc_mem, zloc_size used_size);
ZLOC_API zloc_bool zloc_RemovePool(zloc_allocator *allocator, zloc_pool *pool);
ZLOC_API void zloc_SetMinimumAllocationSize(zloc_allocator *allocator, zloc_size size);
ZLOC_API zloc_pool_stats_t zloc_CreateMemorySnapshot(zloc_header *first_block);

//Remote memory
ZLOC_API zloc_allocator *zloc_InitialiseAllocatorForRemote(void *memory);
ZLOC_API void zloc_SetBlockExtensionSize(zloc_allocator *allocator, zloc_size size);
ZLOC_API int zloc_FreeRemote(zloc_allocator *allocator, void *allocation);
ZLOC_API void *zloc_AllocateRemote(zloc_allocator *allocator, zloc_size remote_size, zloc_size remote_alignment);
ZLOC_API zloc_size zloc_CalculateRemoteBlockPoolSize(zloc_allocator *allocator, zloc_size remote_pool_size);
ZLOC_API void zloc_AddRemotePool(zloc_allocator *allocator, void *block_memory, zloc_size block_memory_size, zloc_size remote_pool_size);
ZLOC_API void* zloc_BlockUserExtensionPtr(const zloc_header *block);
ZLOC_API void* zloc_AllocationFromExtensionPtr(const void *block);

//Linear allocator
typedef struct zloc_linear_allocator_t {
	void *data;
	zloc_size buffer_size;
	zloc_size current_offset;
	void *user_data;
	struct zloc_linear_allocator_t *next;
} zloc_linear_allocator_t;
ZLOC_API int zloc_InitialiseLinearAllocator(zloc_linear_allocator_t *allocator, void *memory, zloc_size size);
ZLOC_API void zloc_ResetLinearAllocator(zloc_linear_allocator_t *allocator);
ZLOC_API void *zloc_LinearAllocation(zloc_linear_allocator_t *allocator, zloc_size size_requested);
ZLOC_API zloc_size zloc_GetMarker(zloc_linear_allocator_t *allocator);
ZLOC_API void zloc_ResetToMarker(zloc_linear_allocator_t *allocator, zloc_size marker);
ZLOC_API void zloc_SetLinearAllocatorUserData(zloc_linear_allocator_t *allocator, void *user_data);
ZLOC_API void zloc_AddNextLinearAllocator(zloc_linear_allocator_t *allocator, zloc_linear_allocator_t *next);
ZLOC_API zloc_size zloc_GetLinearAllocatorCapacity(zloc_linear_allocator_t *allocator);

//--End of user functions

//Private inline functions, user doesn't need to call these

static inline void zloc__map(zloc_size size, zloc_index *fli, zloc_index *sli) {
	*fli = zloc__scan_reverse(size);
	if (*fli <= zloc__SECOND_LEVEL_INDEX_LOG2) {
		*fli = 0;
		*sli = (int)size / (zloc__SMALLEST_CATEGORY / zloc__SECOND_LEVEL_INDEX_COUNT);
		return;
	}
	size = size & ~(1 << *fli);
	*sli = (zloc_index)(size >> (*fli - zloc__SECOND_LEVEL_INDEX_LOG2)) % zloc__SECOND_LEVEL_INDEX_COUNT;
}

static inline void zloc__null_merge_callback(void *remote_user_data, zloc_header *block1, zloc_header *block2) { return; }
void zloc__remote_merge_next_callback(void *remote_user_data, zloc_header *block1, zloc_header *block2);
void zloc__remote_merge_prev_callback(void *remote_user_data, zloc_header *block1, zloc_header *block2);
zloc_size zloc__get_remote_size(const zloc_header *block);
static inline void zloc__null_split_callback(void *remote_user_data, zloc_header *block, zloc_header *trimmed, zloc_size remote_size) { return; }
static inline void zloc__null_add_pool_callback(void *remote_user_data, void *block) { return; }
static inline void zloc__null_unable_to_reallocate_callback(void *remote_user_data, zloc_header *block, zloc_header *new_block) { return; }
static inline void zloc__unset_remote_block_limit_reached(zloc_allocator *allocator) { allocator->block_extension_size &= ~1; };

//Debug tool to make sure that if a first level bitmap has a bit set, then the corresponding second level index should contain a value
static inline void zloc__verify_lists(zloc_allocator *allocator) {
	for (int fli = 0; fli != zloc__FIRST_LEVEL_INDEX_COUNT; ++fli) {
		if (allocator->first_level_bitmap & (1ULL << fli)) {
			//bit in first level is set but according to the second level bitmap array there are no blocks so the first level
			//bitmap bit should have been 0
			ZLOC_ASSERT(allocator->second_level_bitmaps[fli] > 0);
		}
	}
}

//Read only functions
static inline zloc_bool zloc__has_free_block(const zloc_allocator *allocator, zloc_index fli, zloc_index sli) {
	return allocator->first_level_bitmap & (ZLOC_ONE << fli) && allocator->second_level_bitmaps[fli] & (1U << sli);
}

static inline zloc_bool zloc__is_used_block(const zloc_header *block) {
	return !(block->size & zloc__BLOCK_IS_FREE);
}

static inline zloc_bool zloc__is_free_block(const zloc_header *block) {
	return block->size & zloc__BLOCK_IS_FREE;   
	//If you're crashing here, then you're probably trying to free
	//something that isn't a memory block. Maybe you should be
	//zest_vec_free or zest_map_free or maybe freeing someting twice?
}

static inline zloc_bool zloc__prev_is_free_block(const zloc_header *block) {
	return block->size & zloc__PREV_BLOCK_IS_FREE;
}

static inline void* zloc__align_ptr(const void* ptr, zloc_size align) {
	ptrdiff_t aligned = (((ptrdiff_t)ptr) + (align - 1)) & ~(align - 1);
	ZLOC_ASSERT(0 == (align & (align - 1)) && "must align to a power of two");
	return (void*)aligned;
}

static inline zloc_bool zloc__is_aligned(zloc_size size, zloc_size alignment) {
	return (size % alignment) == 0;
}

static inline zloc_bool zloc__ptr_is_aligned(void *ptr, zloc_size alignment) {
	uintptr_t address = (uintptr_t)ptr;
	return (address % alignment) == 0;
}

static inline zloc_size zloc__align_size_down(zloc_size size, zloc_size alignment) {
	return size - (size % alignment);
}

static inline zloc_size zloc__align_size_up(zloc_size size, zloc_size alignment) {
	zloc_size remainder = size % alignment;
	if (remainder != 0) {
		size += alignment - remainder;
	}
	return size;
}

static inline zloc_size zloc__adjust_size(zloc_size size, zloc_size minimum_size, zloc_size alignment) {
	return zloc__Min(zloc__Max(zloc__align_size_up(size, alignment), minimum_size), zloc__MAXIMUM_BLOCK_SIZE);
}

static inline zloc_size zloc__block_size(const zloc_header *block) {
	return block->size & ~(zloc__BLOCK_IS_FREE | zloc__PREV_BLOCK_IS_FREE);
}

static inline zloc_header *zloc__block_from_allocation(const void *allocation) {
	return (zloc_header*)((char*)allocation - zloc__BLOCK_POINTER_OFFSET);
}

static inline zloc_header *zloc__null_block(zloc_allocator *allocator) {
	return &allocator->null_block;
}

static inline void* zloc__block_user_ptr(const zloc_header *block) {
	return (char*)block + zloc__BLOCK_POINTER_OFFSET;
}

static inline zloc_header* zloc__first_block_in_pool(const zloc_pool *pool) {
	return (zloc_header*)((char*)pool - zloc__POINTER_SIZE);
}

static inline zloc_header *zloc__next_physical_block(const zloc_header *block) {
	return (zloc_header*)((char*)zloc__block_user_ptr(block) + zloc__block_size(block));
}

static inline zloc_bool zloc__next_block_is_free(const zloc_header *block) {
	return zloc__is_free_block(zloc__next_physical_block(block));
}

static inline zloc_header *zloc__allocator_first_block(zloc_allocator *allocator) {
	return (zloc_header*)((char*)allocator + zloc_AllocatorSize() - zloc__POINTER_SIZE);
}

static inline zloc_bool zloc__is_last_block_in_pool(const zloc_header *block) {
	return zloc__block_size(block) == 0;
}

static inline zloc_index zloc__find_next_size_up(zloc_fl_bitmap map, zloc_uint start) {
	//Mask out all bits up to the start point of the scan
	map &= (~0ULL << (start + 1));
	return zloc__scan_forward(map);
}

//Write functions
#if defined(ZLOC_THREAD_SAFE)

#define zloc__lock_thread_access												\
do { \
} while (0 != zloc__compare_and_exchange(&allocator->access, 1, 0)); \
ZLOC_ASSERT(allocator->access != 0);

#define zloc__unlock_thread_access allocator->access = 0;

#else

#define zloc__lock_thread_access
#define zloc__unlock_thread_access

#endif
void *zloc__allocate(zloc_allocator *allocator, zloc_size size, zloc_size remote_size);

static inline void zloc__set_block_size(zloc_header *block, zloc_size size) {
	zloc_size boundary_tag = block->size & (zloc__BLOCK_IS_FREE | zloc__PREV_BLOCK_IS_FREE);
	block->size = size | boundary_tag;
}

static inline void zloc__set_prev_physical_block(zloc_header *block, zloc_header *prev_block) {
	block->prev_physical_block = prev_block;
}

static inline void zloc__zero_block(zloc_header *block) {
	block->prev_physical_block = 0;
	block->size = 0;
}

static inline void zloc__mark_block_as_used(zloc_header *block) {
	block->size &= ~zloc__BLOCK_IS_FREE;
	zloc_header *next_block = zloc__next_physical_block(block);
	next_block->size &= ~zloc__PREV_BLOCK_IS_FREE;
}

static inline void zloc__mark_block_as_free(zloc_header *block) {
	block->size |= zloc__BLOCK_IS_FREE;
	zloc_header *next_block = zloc__next_physical_block(block);
	next_block->size |= zloc__PREV_BLOCK_IS_FREE;
}

static inline void zloc__block_set_used(zloc_header *block) {
	block->size &= ~zloc__BLOCK_IS_FREE;
}

static inline void zloc__block_set_free(zloc_header *block) {
	block->size |= zloc__BLOCK_IS_FREE;
}

static inline void zloc__block_set_prev_used(zloc_header *block) {
	block->size &= ~zloc__PREV_BLOCK_IS_FREE;
}

static inline void zloc__block_set_prev_free(zloc_header *block) {
	block->size |= zloc__PREV_BLOCK_IS_FREE;
}

/*
	Push a block onto the segregated list of free blocks. Called when zloc_Free is called. Generally blocks are
	merged if possible before this is called
*/
static inline void zloc__push_block(zloc_allocator *allocator, zloc_header *block) {
	zloc_index fli;
	zloc_index sli;
	//Get the size class of the block
	zloc__map(zloc__do_size_class_callback(block), &fli, &sli);
	zloc_header *current_block_in_free_list = allocator->segregated_lists[fli][sli];
	//If you hit this assert then it's likely that at somepoint in your code you're trying to free an allocation
	//that was already freed or trying to free something that wasn't allocated by the allocator.
	ZLOC_ASSERT(block != current_block_in_free_list);
	//Insert the block into the list by updating the next and prev free blocks of
	//this and the current block in the free list. The current block in the free
	//list may well be the null_block in the allocator so this just means that this
	//block will be added as the first block in this class of free blocks.
	block->next_free_block = current_block_in_free_list;
	block->prev_free_block = &allocator->null_block;
	current_block_in_free_list->prev_free_block = block;

	allocator->segregated_lists[fli][sli] = block;
	//Flag the bitmaps to mark that this size class now contains a free block
	allocator->first_level_bitmap |= ZLOC_ONE << fli;
	allocator->second_level_bitmaps[fli] |= 1U << sli;
	if (allocator->first_level_bitmap & (ZLOC_ONE << fli)) {
		ZLOC_ASSERT(allocator->second_level_bitmaps[fli] > 0);
	}
	zloc__mark_block_as_free(block);
	allocator->stats.free += zloc__block_size(block);
	allocator->stats.free_blocks++;
	allocator->stats.blocks_in_use--;
	#ifdef ZLOC_EXTRA_DEBUGGING
	zloc__verify_lists(allocator);
	#endif
}

/*
	Remove a block from the segregated list in the allocator and return it. If there is a next free block in the size class
	then move it down the list, otherwise unflag the bitmaps as necessary. This is only called when we're trying to allocate
	some memory with zloc_Allocate and we've determined that there's a suitable free block in segregated_lists.
*/
static inline zloc_header *zloc__pop_block(zloc_allocator *allocator, zloc_index fli, zloc_index sli) {
	zloc_header *block = allocator->segregated_lists[fli][sli];

	//If the block in the segregated list is actually the null_block then something went very wrong.
	//Somehow the segregated lists had the end block assigned but the first or second level bitmaps
	//did not have the masks assigned
	ZLOC_ASSERT(block != &allocator->null_block);
	if (block->next_free_block && block->next_free_block != &allocator->null_block) {
		//If there are more free blocks in this size class then shift the next one down and terminate the prev_free_block
		allocator->segregated_lists[fli][sli] = block->next_free_block;
		allocator->segregated_lists[fli][sli]->prev_free_block = zloc__null_block(allocator);
	}
	else {
		//There's no more free blocks in this size class so flag the second level bitmap for this class to 0.
		allocator->segregated_lists[fli][sli] = zloc__null_block(allocator);
		allocator->second_level_bitmaps[fli] &= ~(1U << sli);
		if (allocator->second_level_bitmaps[fli] == 0) {
			//And if the second level bitmap is 0 then the corresponding bit in the first lebel can be zero'd too.
			allocator->first_level_bitmap &= ~(ZLOC_ONE << fli);
		}
	}
	if (allocator->first_level_bitmap & (ZLOC_ONE << fli)) {
		ZLOC_ASSERT(allocator->second_level_bitmaps[fli] > 0);
	}
	zloc__mark_block_as_used(block);
	#ifdef ZLOC_SAFEGUARDS
	block->allocator = allocator;
	#endif
	allocator->stats.free -= zloc__block_size(block);
	allocator->stats.free_blocks--;
	allocator->stats.blocks_in_use++;
	#ifdef ZLOC_EXTRA_DEBUGGING
	zloc__verify_lists(allocator);
	#endif
	return block;
}

/*
	Remove a block from the segregated list. This is only called when we're merging blocks together. The block is
	just removed from the list and marked as used and then merged with an adjacent block.
*/
static inline void zloc__remove_block_from_segregated_list(zloc_allocator *allocator, zloc_header *block) {
	zloc_index fli, sli;
	//Get the size class
	zloc__map(zloc__do_size_class_callback(block), &fli, &sli);
	zloc_header *prev_block = block->prev_free_block;
	zloc_header *next_block = block->next_free_block;
	ZLOC_ASSERT(prev_block);
	ZLOC_ASSERT(next_block);
	next_block->prev_free_block = prev_block;
	prev_block->next_free_block = next_block;
	if (allocator->segregated_lists[fli][sli] == block) {
		allocator->segregated_lists[fli][sli] = next_block;
		if (next_block == zloc__null_block(allocator)) {
			allocator->second_level_bitmaps[fli] &= ~(1U << sli);
			if (allocator->second_level_bitmaps[fli] == 0) {
				allocator->first_level_bitmap &= ~(1ULL << fli);
			}
		}
	}
	if (allocator->first_level_bitmap & (ZLOC_ONE << fli)) {
		ZLOC_ASSERT(allocator->second_level_bitmaps[fli] > 0);
	}
	zloc__mark_block_as_used(block);
	allocator->stats.free -= zloc__block_size(block);
	allocator->stats.free_blocks--;
	allocator->stats.blocks_in_use++;
	#ifdef ZLOC_EXTRA_DEBUGGING
	zloc__verify_lists(allocator);
	#endif
}

/*
	This function is called when zloc_Allocate is called. Once a free block is found then it will be split
	if the size + header overhead + the minimum block size (16b) is greater then the size of the free block.
	If not then it simply returns the free block as it is without splitting.
	If split then the trimmed amount is added back to the segregated list of free blocks.
*/
static inline zloc_header *zloc__maybe_split_block(zloc_allocator *allocator, zloc_header *block, zloc_size size, zloc_size remote_size) {
	//If you crash here it could be that you tried to free something that isn't actually a block allocation,
	//perhaps it's the first object in a list that was allocated like a zest store resource. So when that got
	//freed it could be added to the free block lists as a 0 sized block.
	ZLOC_ASSERT(!zloc__is_last_block_in_pool(block));
	zloc_size size_plus_overhead = size + zloc__BLOCK_POINTER_OFFSET + zloc__block_extension_size;
	if (size_plus_overhead + zloc__MINIMUM_BLOCK_SIZE >= zloc__block_size(block) - zloc__block_extension_size) {
		return block;
	}
	zloc_header *trimmed = (zloc_header*)((char*)zloc__block_user_ptr(block) + size + zloc__block_extension_size);
	trimmed->size = 0;
	zloc__set_block_size(trimmed, zloc__block_size(block) - size_plus_overhead);
	zloc_header *next_block = zloc__next_physical_block(block);
	zloc__set_prev_physical_block(next_block, trimmed);
	zloc__set_prev_physical_block(trimmed, block);
	zloc__set_block_size(block, size + zloc__block_extension_size);
	zloc__do_split_block_callback;
	zloc__push_block(allocator, trimmed);
	return block;
}

//For splitting blocks when allocating to a specific memory alignment
static inline zloc_header *zloc__split_aligned_block(zloc_allocator *allocator, zloc_header *block, zloc_size size) {
	ZLOC_ASSERT(!zloc__is_last_block_in_pool(block));
	zloc_size size_minus_overhead = size - zloc__BLOCK_POINTER_OFFSET;
	zloc_header *trimmed = (zloc_header*)((char*)zloc__block_user_ptr(block) + size_minus_overhead);
	trimmed->size = 0;
	zloc__set_block_size(trimmed, zloc__block_size(block) - size);
	zloc_header *next_block = zloc__next_physical_block(block);
	zloc__set_prev_physical_block(next_block, trimmed);
	zloc__set_prev_physical_block(trimmed, block);
	zloc__set_block_size(block, size_minus_overhead);
	zloc__push_block(allocator, block);
#ifdef ZLOC_SAFEGUARDS
	trimmed->allocator = allocator;
#endif
	return trimmed;
}

/*
	This function is called when zloc_Free is called and the previous physical block is free. If that's the case
	then this function will merge the block being freed with the previous physical block then add that back into
	the segregated list of free blocks. Note that that happens in the zloc_Free function after attempting to merge
	both ways.
*/
static inline zloc_header *zloc__merge_with_prev_block(zloc_allocator *allocator, zloc_header *block) {
	ZLOC_ASSERT(!zloc__is_last_block_in_pool(block));
	zloc_header *prev_block = block->prev_physical_block;
	zloc__remove_block_from_segregated_list(allocator, prev_block);
	zloc__do_merge_prev_callback;
	zloc__set_block_size(prev_block, zloc__block_size(prev_block) + zloc__block_size(block) + zloc__BLOCK_POINTER_OFFSET);
	zloc_header *next_block = zloc__next_physical_block(block);
	zloc__set_prev_physical_block(next_block, prev_block);
	zloc__zero_block(block);
	return prev_block;
}

/*
	This function might be called when zloc_Free is called to free a block. If the block being freed is not the last
	physical block then this function is called and if the next block is free then it will be merged.
*/
static inline void zloc__merge_with_next_block(zloc_allocator *allocator, zloc_header *block) {
	zloc_header *next_block = zloc__next_physical_block(block);
	ZLOC_ASSERT(next_block->prev_physical_block == block);	//could be potentional memory corruption. Check that you're not writing outside the boundary of the block size
	ZLOC_ASSERT(!zloc__is_last_block_in_pool(next_block));
	zloc__remove_block_from_segregated_list(allocator, next_block);
	zloc__do_merge_next_callback;
	zloc__set_block_size(block, zloc__block_size(next_block) + zloc__block_size(block) + zloc__BLOCK_POINTER_OFFSET);
	zloc_header *block_after_next = zloc__next_physical_block(next_block);
	zloc__set_prev_physical_block(block_after_next, block);
	zloc__zero_block(next_block);
}

static inline zloc_header *zloc__find_free_block(zloc_allocator *allocator, zloc_size size, zloc_size remote_size) {
	zloc_index fli;
	zloc_index sli;
	zloc__map(zloc__map_size, &fli, &sli);
	//Note that there may well be an appropriate size block in the class but that block may not be at the head of the list
	//In this situation we could opt to loop through the list of the size class to see if there is an appropriate size but instead
	//we stick to the paper and just move on to the next class up to keep a O1 speed at the cost of some extra fragmentation
	if (zloc__has_free_block(allocator, fli, sli) && zloc__do_size_class_callback(allocator->segregated_lists[fli][sli]) >= zloc__map_size) {
		zloc_header *block = zloc__pop_block(allocator, fli, sli);
		return block;
	}
	if (sli == zloc__SECOND_LEVEL_INDEX_COUNT - 1) {
		sli = -1;
	}
	else {
		sli = zloc__find_next_size_up(allocator->second_level_bitmaps[fli], sli);
	}
	if (sli == -1) {
		fli = zloc__find_next_size_up(allocator->first_level_bitmap, fli);
		if (fli > -1) {
			sli = zloc__scan_forward(allocator->second_level_bitmaps[fli]);
			zloc_header *block = zloc__pop_block(allocator, fli, sli);
			zloc_header *split_block = zloc__call_maybe_split_block;
			return split_block;
		}
	}
	else {
		zloc_header *block = zloc__pop_block(allocator, fli, sli);
		zloc_header *split_block = zloc__call_maybe_split_block;
		return split_block;
	}

	return 0;
}
//--End of internal functions

//--End of header declarations

//Macro_Defines
#if defined(__x86_64__) || defined(__i386__) || defined(_M_X64)
#define ZEST_INTEL
#include <immintrin.h>
#elif defined(__arm__) || defined(__aarch64__)
#include <arm_neon.h>
#define ZEST_ARM
#endif

#ifndef ZEST_MAX_FIF
//The maximum number of frames in flight. If you're very tight on memory then 1 will use less resources.
#define ZEST_MAX_FIF 2
#endif

#ifndef ZEST_ASSERT

#ifndef NDEBUG

#define ZEST_DEBUGGING

#define ZEST_INTERNAL_ASSERT_IMPL(condition, format, ...) \
    do { \
        if (!(condition)) { \
            fprintf(stderr, "Assertion failed at %s:%d: ", __FILE__, __LINE__); \
            fprintf(stderr, format, ##__VA_ARGS__); \
            fprintf(stderr, "\n"); \
            abort(); \
        } \
    } while (0)

#define ZEST_INTERNAL_ASSERT_NO_MSG(condition) \
    do { \
        if (!(condition)) { \
            fprintf(stderr, "Assertion failed at %s:%d with condition: %s\n", __FILE__, __LINE__, #condition); \
            abort(); \
        } \
    } while (0)

#define ZEST_GET_MACRO(_1, _2, NAME, ...) NAME
#define ZEST_APPLY_MACRO(macro, args) macro args
#define ZEST_CHOOSE_ASSERT(...) ZEST_APPLY_MACRO(ZEST_GET_MACRO, (__VA_ARGS__, ZEST_INTERNAL_ASSERT_IMPL, ZEST_INTERNAL_ASSERT_NO_MSG))
#define ZEST_ASSERT(...) ZEST_APPLY_MACRO(ZEST_CHOOSE_ASSERT(__VA_ARGS__), (__VA_ARGS__))
#define ZEST_ASSERT(...) ZEST_APPLY_MACRO(ZEST_CHOOSE_ASSERT(__VA_ARGS__), (__VA_ARGS__))
#define ZEST_ASSERT_TILING_FORMAT(format) ZEST_ASSERT( \
    format == zest_format_r8_unorm || \
    format == zest_format_r8g8_unorm || \
    format == zest_format_r8g8b8a8_unorm || \
    format == zest_format_b8g8r8a8_unorm || \
    format == zest_format_r8g8b8a8_srgb || \
    format == zest_format_b8g8r8a8_srgb || \
    format == zest_format_r16_sfloat || \
    format == zest_format_r16g16_sfloat || \
    format == zest_format_r16g16b16a16_sfloat || \
    format == zest_format_r32_sfloat || \
    format == zest_format_r32g32_sfloat || \
    format == zest_format_r32g32b32a32_sfloat \
)

#else
#pragma message("ZEST_ASSERT is disabled because NDEBUG is defined.")
#define ZEST_ASSERT(...) (void)0
#define ZEST_ASSERT_TILING_FORMAT(format) void(0)
#endif

#endif

// Test mode validation macro - validates gracefully and returns on failure instead of asserting.
// Use this for input validation that should assert in normal builds but allow graceful failure in tests.
// To enable test mode, define ZEST_TEST_MODE before including zest.h
#ifdef ZEST_TEST_MODE
#define ZEST_ASSERT_OR_VALIDATE(condition, device, message, fail_return) \
    do { \
        if (!(condition)) { \
            zest__log_validation_error(device, message); \
            return fail_return; \
        } \
    } while(0)
#else
#define ZEST_ASSERT_OR_VALIDATE(condition, device, message, fail_return) \
    ZEST_ASSERT(condition, message)
#endif

#ifndef ZEST_API
#define ZEST_API
#endif

//I had to add this because some dell laptops have their own drivers that are basically broken. Even though the gpu signals that
//it can write direct it actually doesn't and just crashes. Maybe there's another way to detect or do a pre-test?
#define ZEST_DISABLE_GPU_DIRECT_WRITE 1

//Helper macros
#define ZEST__MIN(a, b) (((a) < (b)) ? (a) : (b))
#define ZEST__MAX(a, b) (((a) > (b)) ? (a) : (b))
#define ZEST__CLAMP(v, min_v, max_v) ((v < min_v) ? min_v : ((v > max_v) ? max_v : v))
#define ZEST__POW2(x) ((x) && !((x) & ((x) - 1)))
#define ZEST__FLAG(flag, bit) flag |= (bit)
#define ZEST__MAYBE_FLAG(flag, bit, yesno) flag |= (yesno ? bit : 0)
#define ZEST__UNFLAG(flag, bit) flag &= ~bit
#define ZEST__FLAGGED(flag, bit) ((flag & (bit)) > 0)
#define ZEST__NOT_FLAGGED(flag, bit) (flag & (bit)) == 0
#define ZEST__MAKE_SUBMISSION_ID(wave_index, execution_index, queue_id) (queue_id << 24) | (wave_index << 16) | execution_index
#define ZEST__EXECUTION_INDEX(id) (id & 0xFFFF)
#define ZEST__SUBMISSION_INDEX(id) ((id & 0x00FF0000) >> 16)
#define ZEST__EXECUTION_ORDER_ID(id) (id & 0xFFFFFF)
#define ZEST__QUEUE_INDEX(id) ((id & 0xFF000000) >> 24)
#define ZEST__ALL_MIPS 0xFFFFFFFF
#define ZEST__ALL_LAYERS 0xFFFFFFFF
#define ZEST_QUEUE_FAMILY_IGNORED (~0U)
#ifdef __cplusplus
#define ZEST__ZERO_INIT(T) T()
#else
#define ZEST__ZERO_INIT(T) (T) { 0 }
#endif

static const char *zest_message_pass_culled = "Pass [%s] culled because there were no outputs.";
static const char *zest_message_pass_culled_no_work = "Pass [%s] culled because there was no work found.";
static const char *zest_message_pass_culled_not_consumed = "Pass [%s] culled because it's output was not consumed by any subsequent passes. This won't happen if the ouput is an imported buffer or image. If the resource is transient then it will be discarded immediately once it has no further use. Also note that passes at the front of a chain can be culled if ultimately nothing consumes the output from the last pass in the chain.";
static const char *zest_message_cyclic_dependency = "Cyclic dependency detected in render graph [%s] with pass [%s]. You have a situation where  Pass A depends on Pass B's output, and Pass B depends on Pass A's output.";
static const char *zest_message_invalid_reference_counts = "Graph Compile Error in Frame Graph [%s]: When culling potential pass in reference counts should never go below 0. This could be a bug in the compile logic.";
static const char *zest_message_usage_has_no_resource = "Graph Compile Error in Frame Graph [%s]: A usage node did not have a valid resource node.";
static const char *zest_message_multiple_swapchain_usage = "Graph Compile Error in Frame Graph [%s]: This is the second time that the swap chain is used as output, this should not happen. Check that the passes that use the swapchain as output have exactly the same set of other ouputs so that the passes can be properly grouped together. If you want to use multiple passes to render in different stages then use transient image resources and composite them to the swapchain in a final compositing pass.";
static const char *zest_message_resource_added_as_ouput_more_than_once = "Graph Compile Error in Frame Graph [%s]: A resource should only have one producer in a valid graph. Check to make sure you haven't added the same output to a pass more than once";
static const char *zest_message_resource_should_be_imported = "Graph Compile Error in Frame Graph [%s]: ";
static const char *zest_message_cannot_queue_for_execution = "Could not queue frame graph [%s] for execution as there were errors found in the graph. Check other reports for reasons why.";

//Override this if you'd prefer a different way to allocate the pools for sub allocation in host memory.
#ifndef ZEST__ALLOCATE_POOL
#define ZEST__ALLOCATE_POOL malloc
#endif

#ifndef ZEST__FREE_POOL
#define ZEST__FREE_POOL free
#endif

#ifndef ZEST__FREE
#define ZEST__FREE(allocator, memory) zloc_Free(allocator, memory)
#endif

#ifndef ZEST__ALLOCATE
#define ZEST__ALLOCATE(allocator, size) zest__allocate(allocator, size)
#define ZEST__ALLOCATE_ALIGNED(allocator, size, alignment) zest__allocate_aligned(allocator, size, alignment)
#define ZEST__REALLOCATE(allocator, ptr, size) zest__reallocate(allocator, ptr, size)
#endif

#ifndef ZEST_ALERT_COLOR
#define ZEST_ALERT_COLOR "\033[1;33m"    // Bold yellow - classic warning
/*
Other color possibilities
#define ZEST_ALERT_COLOR1 "\033[1;31m"    // Bold red - more urgent
#define ZEST_ALERT_COLOR2 "\033[93m"      // Bright yellow

#define ZEST_ALERT_COLOR3 "\033[38;5;214m"  // Lighter orange/gold
#define ZEST_ALERT_COLOR4 "\033[38;5;202m"  // Red-orange (more urgent feel)
#define ZEST_ALERT_COLOR5 "\033[38;5;220m"  // Gold/amber
#define ZEST_ALERT_COLOR6 "\033[38;5;226m"  // Bright yellow
#define ZEST_ALERT_COLOR7 "\033[38;5;196m"  // Pure bright red

#define ZEST_ALERT_COLOR8 "\033[1;37;41m"   // Bold white on red
#define ZEST_ALERT_COLOR9 "\033[1;33;40m"   // Bold yellow on black
*/
#endif

#ifndef ZEST_NOTICE_COLOR
#define ZEST_NOTICE_COLOR "\033[0m"
#endif

#define ZEST_PRINT(message_f, ...) printf(message_f "\n", ##__VA_ARGS__)
#define ZEST_ALERT(message_f, ...) printf(ZEST_ALERT_COLOR message_f "\n\033[0m", ##__VA_ARGS__)

#ifdef ZEST_OUTPUT_NOTICE_MESSAGES
#define ZEST_PRINT_NOTICE(message_f, ...) printf(ZEST_NOTICE_COLOR message_f "\n\033[0m", ##__VA_ARGS__)
#else
#define ZEST_PRINT_NOTICE(message_f, ...)
#endif

#define ZEST_NL u8"\n"
#define ZEST_TAB u8"\t"
#define ZEST_APPEND_LOG(log_path, message, ...) if (log_path) { FILE *log_file = zest__open_file(log_path, "a"); fprintf(log_file, message ZEST_NL, ##__VA_ARGS__); fclose(log_file); }

#define ZEST_MICROSECS_SECOND 1000000ULL
#define ZEST_MICROSECS_MILLISECOND 1000
#define ZEST_MILLISECONDS_IN_MICROSECONDS(millisecs) millisecs * ZEST_MICROSECS_MILLISECOND
#define ZEST_MICROSECONDS_TO_MILLISECONDS(microseconds) double(microseconds) / 1000.0
#define ZEST_SECONDS_IN_MICROSECONDS(seconds) seconds * ZEST_MICROSECS_SECOND
#define ZEST_SECONDS_IN_MILLISECONDS(seconds) seconds * 1000
#define ZEST_PI 3.14159265359f
#define ZEST_FIXED_LOOP_BUFFER 0xF18
#define ZEST_MAX_ATTACHMENTS 8
#define ZEST_MAX_FLOAT     3.402823466e+38F

#ifndef ZEST_MAX_DEVICE_MEMORY_POOLS
#define ZEST_MAX_DEVICE_MEMORY_POOLS 64
#endif

#define ZEST_NULL 0
//For each frame in flight macro
#define zest_ForEachFrameInFlight(index) for (unsigned int index = 0; index != ZEST_MAX_FIF; ++index)

//Typedefs_for_numbers
typedef uint32_t zest_uint;
typedef unsigned long zest_long;
typedef unsigned long long zest_ull;
typedef uint16_t zest_u16;
typedef uint64_t zest_u64;
typedef uint64_t zest_handle;
typedef zest_uint zest_id;
typedef zest_uint zest_millisecs;
typedef zest_uint zest_thread_access;
typedef zest_ull zest_microsecs;
typedef zest_ull zest_key;
typedef zest_ull zest_size;
typedef unsigned char zest_byte;
typedef unsigned int zest_bool;
typedef char* zest_file;

//Handles. These are pointers that remain stable until the object is freed.
#define ZEST__MAKE_HANDLE(handle) typedef struct handle##_t* handle;

#define ZEST__MAKE_USER_HANDLE(handle) typedef struct { zest_handle value; zest_resource_store_t *store; } handle##_handle;
#define ZEST_HANDLE_INDEX(handle) (zest_uint)(handle & 0xFFFFFFFF)
#define ZEST_HANDLE_GENERATION(handle) (zest_uint)((handle & 0xFFFFFF00000000) >> 32ull)
#define ZEST_HANDLE_TYPE(handle) (zest_handle_type)((handle & 0xFF00000000000000) >> 56ull)

#define ZEST_CREATE_HANDLE(generation, index) (((zest_u64)generation << 32ull) + index)

//For allocating a new object with handle. Only used internally.
#define ZEST__NEW(allocator, type) (type)ZEST__ALLOCATE(allocator, sizeof(type##_t))
#define ZEST__NEW_ALIGNED(allocator, type, alignment) (type)ZEST__ALLOCATE_ALIGNED(allocator, sizeof(type##_t), alignment)

//For global variables
#define ZEST_GLOBAL static
//For internal private functions that aren't meant to be called outside of the library
//If there's a case for where it would be useful be able to call a function externally
//then we change it to a ZEST_API function
#if defined(ZEST_INTERNAL_BUILD)
#define ZEST_INTERNAL
#define ZEST_API_INTERNAL
#else
#define ZEST_INTERNAL static
#endif

#define ZEST_PRIVATE static

#ifdef ZEST_API_INTERNAL
#endif
ZEST_API void* zest__vec_reserve(zloc_allocator *allocator, void* T, zest_uint unit_size, zest_uint new_capacity);

#define ZEST_API_TMP

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

/* Platform_specific_code*/

FILE *zest__open_file(const char *file_name, const char *mode);
zest_bool zest__file_exists(const char *file_name);
ZEST_API zest_millisecs zest_Millisecs(void);
ZEST_API zest_microsecs zest_Microsecs(void);

#ifndef ZEST_THREAD_LOCAL
#if __cplusplus >= 201103L
#define ZEST_THREAD_LOCAL thread_local
#elif __STDC_VERSION__ >= 201112L
#define ZEST_THREAD_LOCAL _Thread_local
#elif defined(__GNUC__)
#define ZEST_THREAD_LOCAL __thread
#elif defined(_MSC_VER)
#define ZEST_THREAD_LOCAL __declspec(thread)
#else
#error "No support for thread-local storage"
#endif
#endif

#ifdef __cplusplus
#define ZEST_STRUCT_LITERAL(type, ...) type { __VA_ARGS__ }
#else
#define ZEST_STRUCT_LITERAL(type, ...) (type) { __VA_ARGS__ }
#endif

#if defined (_WIN32)
#include <windows.h>
#include <direct.h>	//For creating a directory when caching shaders
#define zest_snprintf(buffer, bufferSize, format, ...) sprintf_s(buffer, bufferSize, format, __VA_ARGS__)
#define zest_strcat(left, size, right) strcat_s(left, size, right)
#define zest_strcpy(left, size, right) strcpy_s(left, size, right)
#define zest_strerror(buffer, size, error) strerror_s(buffer, size, error)
#define ZEST_ALIGN_PREFIX(v) __declspec(align(v))
#define ZEST_ALIGN_AFFIX(v)
#define ZEST_PROTOTYPE
ZEST_PRIVATE inline zest_thread_access zest__compare_and_exchange(volatile zest_thread_access* target, zest_thread_access value, zest_thread_access original) {
	return InterlockedCompareExchange((LONG*)target, value, original);
}

//Window creation
ZEST_PRIVATE HINSTANCE zest_window_instance;
#define ZEST_WINDOW_INSTANCE HINSTANCE
LRESULT CALLBACK zest__window_proc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
//--
#define ZEST_CREATE_DIR(path) _mkdir(path)

#define zest__writebarrier _WriteBarrier();
#define zest__readbarrier _ReadBarrier();

#elif defined(__GNUC__) && ((__GNUC__ > 4) || (__GNUC__ == 4 && __GNUC_MINOR__ >= 8)) && (defined(__i386__) || defined(__x86_64__)) || defined(__clang__)
#include <time.h>
#include <sys/stat.h>
#include <sys/types.h>
#define ZEST_ALIGN_PREFIX(v)
#define ZEST_ALIGN_AFFIX(v)  __attribute__((aligned(16)))
#define ZEST_PROTOTYPE void
#define zest_snprintf(buffer, bufferSize, format, ...) snprintf(buffer, bufferSize, format, __VA_ARGS__)
#define zest_strcat(left, size, right) strcat(left, right)
#define zest_strcpy(left, size, right) strcpy(left, right)
#define zest_strerror(buffer, size, error) strerror_r(error, buffer, size)
ZEST_PRIVATE inline zest_thread_access zest__compare_and_exchange(volatile zest_thread_access* target, zest_thread_access value, zest_thread_access original) {
	return __sync_val_compare_and_swap(target, original, value);
}
#define ZEST_CREATE_DIR(path) mkdir(path, 0777)
#define zest__writebarrier __asm__ __volatile__ ("" : : : "memory");
#define zest__readbarrier __asm__ __volatile__ ("" : : : "memory");

//Window creation
//--

#endif
/*end of platform specific code*/
#ifdef __APPLE__
#define ZEST_PORTABILITY_ENUMERATION
#endif

#define ZEST_TRUE 1
#define ZEST_FALSE 0
#define ZEST_INVALID 0xFFFFFFFF
#define ZEST_WHOLE_SIZE (~0ULL)
#define ZEST_NOT_BINDLESS 0xFFFFFFFF
#define ZEST_U32_MAX_VALUE ((zest_uint)-1)

//Shader_code
//For nicer formatting of the shader code, but note that new lines are ignored when this becomes an actual string.
#define ZEST_GLSL(version, shader) "#version " #version "\n" "#extension GL_EXT_nonuniform_qualifier : require\n" #shader

//----------------------
//Swap chain vert shader
//----------------------
static const char *zest_shader_swap_vert = ZEST_GLSL(450,
													layout(location = 0) out vec2 outUV;

	out gl_PerVertex
	{
		vec4 gl_Position;
	};

	void main()
	{
		outUV = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);
		gl_Position = vec4(outUV * 2.0f - 1.0f, 0.0f, 1.0f);
	}
);

//----------------------
//Swap chain frag shader
//----------------------
static const char *zest_shader_swap_frag = ZEST_GLSL(450,
													layout(set = 0, binding = 0) uniform sampler2D samplerColor;
	layout(location = 0) in vec2 inUV;
	layout(location = 0) out vec4 outFragColor;
	void main(void)
	{
		outFragColor = texture(samplerColor, inUV);
	}
);

//Enums_and_flags
typedef enum zest_platform_type {
	zest_platform_vulkan,
	zest_platform_d3d12,
	zest_platform_metal,
	zest_platform_headless,
	zest_max_platforms
} zest_platform_type;

typedef enum zest_format {
	zest_format_undefined = 0,
	zest_format_r4g4_unorm_pack8 = 1,
	zest_format_r4g4b4a4_unorm_pack16 = 2,
	zest_format_b4g4r4a4_unorm_pack16 = 3,
	zest_format_r5g6b5_unorm_pack16 = 4,
	zest_format_b5g6r5_unorm_pack16 = 5,
	zest_format_r5g5b5a1_unorm_pack16 = 6,
	zest_format_b5g5r5a1_unorm_pack16 = 7,
	zest_format_a1r5g5b5_unorm_pack16 = 8,
	zest_format_r8_unorm = 9,
	zest_format_r8_snorm = 10,
	zest_format_r8_uscaled = 11,
	zest_format_r8_sscaled = 12,
	zest_format_r8_uint = 13,
	zest_format_r8_sint = 14,
	zest_format_r8_srgb = 15,
	zest_format_r8g8_unorm = 16,
	zest_format_r8g8_snorm = 17,
	zest_format_r8g8_uscaled = 18,
	zest_format_r8g8_sscaled = 19,
	zest_format_r8g8_uint = 20,
	zest_format_r8g8_sint = 21,
	zest_format_r8g8_srgb = 22,
	zest_format_r8g8b8_unorm = 23,
	zest_format_r8g8b8_snorm = 24,
	zest_format_r8g8b8_uscaled = 25,
	zest_format_r8g8b8_sscaled = 26,
	zest_format_r8g8b8_uint = 27,
	zest_format_r8g8b8_sint = 28,
	zest_format_r8g8b8_srgb = 29,
	zest_format_b8g8r8_unorm = 30,
	zest_format_b8g8r8_snorm = 31,
	zest_format_b8g8r8_uscaled = 32,
	zest_format_b8g8r8_sscaled = 33,
	zest_format_b8g8r8_uint = 34,
	zest_format_b8g8r8_sint = 35,
	zest_format_b8g8r8_srgb = 36,
	zest_format_r8g8b8a8_unorm = 37,
	zest_format_r8g8b8a8_snorm = 38,
	zest_format_r8g8b8a8_uscaled = 39,
	zest_format_r8g8b8a8_sscaled = 40,
	zest_format_r8g8b8a8_uint = 41,
	zest_format_r8g8b8a8_sint = 42,
	zest_format_r8g8b8a8_srgb = 43,
	zest_format_b8g8r8a8_unorm = 44,
	zest_format_b8g8r8a8_snorm = 45,
	zest_format_b8g8r8a8_uscaled = 46,
	zest_format_b8g8r8a8_sscaled = 47,
	zest_format_b8g8r8a8_uint = 48,
	zest_format_b8g8r8a8_sint = 49,
	zest_format_b8g8r8a8_srgb = 50,
	zest_format_a8b8g8r8_unorm_pack32 = 51,
	zest_format_a8b8g8r8_snorm_pack32 = 52,
	zest_format_a8b8g8r8_uscaled_pack32 = 53,
	zest_format_a8b8g8r8_sscaled_pack32 = 54,
	zest_format_a8b8g8r8_uint_pack32 = 55,
	zest_format_a8b8g8r8_sint_pack32 = 56,
	zest_format_a8b8g8r8_srgb_pack32 = 57,
	zest_format_a2r10g10b10_unorm_pack32 = 58,
	zest_format_a2r10g10b10_snorm_pack32 = 59,
	zest_format_a2r10g10b10_uscaled_pack32 = 60,
	zest_format_a2r10g10b10_sscaled_pack32 = 61,
	zest_format_a2r10g10b10_uint_pack32 = 62,
	zest_format_a2r10g10b10_sint_pack32 = 63,
	zest_format_a2b10g10r10_unorm_pack32 = 64,
	zest_format_a2b10g10r10_snorm_pack32 = 65,
	zest_format_a2b10g10r10_uscaled_pack32 = 66,
	zest_format_a2b10g10r10_sscaled_pack32 = 67,
	zest_format_a2b10g10r10_uint_pack32 = 68,
	zest_format_a2b10g10r10_sint_pack32 = 69,
	zest_format_r16_unorm = 70,
	zest_format_r16_snorm = 71,
	zest_format_r16_uscaled = 72,
	zest_format_r16_sscaled = 73,
	zest_format_r16_uint = 74,
	zest_format_r16_sint = 75,
	zest_format_r16_sfloat = 76,
	zest_format_r16g16_unorm = 77,
	zest_format_r16g16_snorm = 78,
	zest_format_r16g16_uscaled = 79,
	zest_format_r16g16_sscaled = 80,
	zest_format_r16g16_uint = 81,
	zest_format_r16g16_sint = 82,
	zest_format_r16g16_sfloat = 83,
	zest_format_r16g16b16_unorm = 84,
	zest_format_r16g16b16_snorm = 85,
	zest_format_r16g16b16_uscaled = 86,
	zest_format_r16g16b16_sscaled = 87,
	zest_format_r16g16b16_uint = 88,
	zest_format_r16g16b16_sint = 89,
	zest_format_r16g16b16_sfloat = 90,
	zest_format_r16g16b16a16_unorm = 91,
	zest_format_r16g16b16a16_snorm = 92,
	zest_format_r16g16b16a16_uscaled = 93,
	zest_format_r16g16b16a16_sscaled = 94,
	zest_format_r16g16b16a16_uint = 95,
	zest_format_r16g16b16a16_sint = 96,
	zest_format_r16g16b16a16_sfloat = 97,
	zest_format_r32_uint = 98,
	zest_format_r32_sint = 99,
	zest_format_r32_sfloat = 100,
	zest_format_r32g32_uint = 101,
	zest_format_r32g32_sint = 102,
	zest_format_r32g32_sfloat = 103,
	zest_format_r32g32b32_uint = 104,
	zest_format_r32g32b32_sint = 105,
	zest_format_r32g32b32_sfloat = 106,
	zest_format_r32g32b32a32_uint = 107,
	zest_format_r32g32b32a32_sint = 108,
	zest_format_r32g32b32a32_sfloat = 109,
	zest_format_r64_uint = 110,
	zest_format_r64_sint = 111,
	zest_format_r64_sfloat = 112,
	zest_format_r64g64_uint = 113,
	zest_format_r64g64_sint = 114,
	zest_format_r64g64_sfloat = 115,
	zest_format_r64g64b64_uint = 116,
	zest_format_r64g64b64_sint = 117,
	zest_format_r64g64b64_sfloat = 118,
	zest_format_r64g64b64a64_uint = 119,
	zest_format_r64g64b64a64_sint = 120,
	zest_format_r64g64b64a64_sfloat = 121,
	zest_format_b10g11r11_ufloat_pack32 = 122,
	zest_format_e5b9g9r9_ufloat_pack32 = 123,
	zest_format_d16_unorm = 124,
	zest_format_x8_d24_unorm_pack32 = 125,
	zest_format_d32_sfloat = 126,
	zest_format_s8_uint = 127,
	zest_format_d16_unorm_s8_uint = 128,
	zest_format_d24_unorm_s8_uint = 129,
	zest_format_d32_sfloat_s8_uint = 130,
	zest_format_bc1_rgb_unorm_block = 131,
	zest_format_bc1_rgb_srgb_block = 132,
	zest_format_bc1_rgba_unorm_block = 133,
	zest_format_bc1_rgba_srgb_block = 134,
	zest_format_bc2_unorm_block = 135,
	zest_format_bc2_srgb_block = 136,
	zest_format_bc3_unorm_block = 137,
	zest_format_bc3_srgb_block = 138,
	zest_format_bc4_unorm_block = 139,
	zest_format_bc4_snorm_block = 140,
	zest_format_bc5_unorm_block = 141,
	zest_format_bc5_snorm_block = 142,
	zest_format_bc6h_ufloat_block = 143,
	zest_format_bc6h_sfloat_block = 144,
	zest_format_bc7_unorm_block = 145,
	zest_format_bc7_srgb_block = 146,
	zest_format_etc2_r8g8b8_unorm_block = 147,
	zest_format_etc2_r8g8b8_srgb_block = 148,
	zest_format_etc2_r8g8b8a1_unorm_block = 149,
	zest_format_etc2_r8g8b8a1_srgb_block = 150,
	zest_format_etc2_r8g8b8a8_unorm_block = 151,
	zest_format_etc2_r8g8b8a8_srgb_block = 152,
	zest_format_eac_r11_unorm_block = 153,
	zest_format_eac_r11_snorm_block = 154,
	zest_format_eac_r11g11_unorm_block = 155,
	zest_format_eac_r11g11_snorm_block = 156,
	zest_format_astc_4X4_unorm_block = 157,
	zest_format_astc_4X4_srgb_block = 158,
	zest_format_astc_5X4_unorm_block = 159,
	zest_format_astc_5X4_srgb_block = 160,
	zest_format_astc_5X5_unorm_block = 161,
	zest_format_astc_5X5_srgb_block = 162,
	zest_format_astc_6X5_unorm_block = 163,
	zest_format_astc_6X5_srgb_block = 164,
	zest_format_astc_6X6_unorm_block = 165,
	zest_format_astc_6X6_srgb_block = 166,
	zest_format_astc_8X5_unorm_block = 167,
	zest_format_astc_8X5_srgb_block = 168,
	zest_format_astc_8X6_unorm_block = 169,
	zest_format_astc_8X6_srgb_block = 170,
	zest_format_astc_8X8_unorm_block = 171,
	zest_format_astc_8X8_srgb_block = 172,
	zest_format_astc_10X5_unorm_block = 173,
	zest_format_astc_10X5_srgb_block = 174,
	zest_format_astc_10X6_unorm_block = 175,
	zest_format_astc_10X6_srgb_block = 176,
	zest_format_astc_10X8_unorm_block = 177,
	zest_format_astc_10X8_srgb_block = 178,
	zest_format_astc_10X10_unorm_block = 179,
	zest_format_astc_10X10_srgb_block = 180,
	zest_format_astc_12X10_unorm_block = 181,
	zest_format_astc_12X10_srgb_block = 182,
	zest_format_astc_12X12_unorm_block = 183,
	zest_format_astc_12X12_srgb_block = 184,
	zest_format_depth = 185,
	zest_max_format = 186
} zest_format;

typedef enum {
	zest_input_rate_vertex,
	zest_input_rate_instance,
} zest_input_rate;

typedef enum {
	zest_image_usage_transfer_src_bit = 0x00000001,
	zest_image_usage_transfer_dst_bit = 0x00000002,
	zest_image_usage_sampled_bit = 0x00000004,
	zest_image_usage_storage_bit = 0x00000008,
	zest_image_usage_color_attachment_bit = 0x00000010,
	zest_image_usage_depth_stencil_attachment_bit = 0x00000020,
	zest_image_usage_transient_attachment_bit = 0x00000040,
	zest_image_usage_input_attachment_bit = 0x00000080,
	zest_image_usage_host_transfer_bit = 0x00400000,
} zest_image_usage_flag_bits;

typedef zest_uint zest_image_usage_flags;

typedef enum {
	zest_buffer_usage_transfer_src_bit = 0x00000001,
	zest_buffer_usage_transfer_dst_bit = 0x00000002,
	zest_buffer_usage_uniform_texel_buffer_bit = 0x00000004,
	zest_buffer_usage_storage_texel_buffer_bit = 0x00000008,
	zest_buffer_usage_uniform_buffer_bit = 0x00000010,
	zest_buffer_usage_storage_buffer_bit = 0x00000020,
	zest_buffer_usage_index_buffer_bit = 0x00000040,
	zest_buffer_usage_vertex_buffer_bit = 0x00000080,
	zest_buffer_usage_indirect_buffer_bit = 0x00000100,
	zest_buffer_usage_shader_device_address_bit = 0x00020000,
} zest_buffer_usage_flag_bits;

typedef zest_uint zest_buffer_usage_flags;

typedef enum zest_resource_state {
	zest_resource_state_undefined,

	//General states
	zest_resource_state_shader_read,          // For reading in any shader stage (texture, SRV)
	zest_resource_state_unordered_access,     // For read/write in a shader (UAV)
	zest_resource_state_copy_src,
	zest_resource_state_copy_dst,

	//Images
	zest_resource_state_render_target,        // For use as a color attachment
	zest_resource_state_depth_stencil_write,  // For use as a depth/stencil attachment
	zest_resource_state_depth_stencil_read,   // For read-only depth/stencil
	zest_resource_state_present,              // For presenting to the screen

	//Buffers
	zest_resource_state_index_buffer,
	zest_resource_state_vertex_buffer,
	zest_resource_state_uniform_buffer,
	zest_resource_state_indirect_argument,
} zest_resource_state;

typedef enum {
	zest_memory_property_device_local_bit = 0x00000001,
	zest_memory_property_host_visible_bit = 0x00000002,
	zest_memory_property_host_coherent_bit = 0x00000004,
	zest_memory_property_host_cached_bit = 0x00000008,
	zest_memory_property_lazily_allocated_bit = 0x00000010,
	zest_memory_property_protected_bit = 0x00000020,
} zest_memory_property_flag_bits;

typedef zest_uint zest_memory_property_flags;

typedef enum {
	zest_filter_nearest,
	zest_filter_linear,
} zest_filter_type;

typedef enum {
	zest_mipmap_mode_nearest,
	zest_mipmap_mode_linear,
} zest_mipmap_mode;

typedef enum {
	zest_sampler_address_mode_repeat,
	zest_sampler_address_mode_mirrored_repeat,
	zest_sampler_address_mode_clamp_to_edge,
	zest_sampler_address_mode_clamp_to_border,
	zest_sampler_address_mode_mirror_clamp_to_edge,
} zest_sampler_address_mode;

typedef enum zest_border_color {
	zest_border_color_float_transparent_black, // Black, (0,0,0,0)
	zest_border_color_int_transparent_black,
	zest_border_color_float_opaque_black,      // Black, (0,0,0,1)
	zest_border_color_int_opaque_black,
	zest_border_color_float_opaque_white,      // White, (1,1,1,1)
	zest_border_color_int_opaque_white
} zest_border_color;

typedef enum {
	zest_compare_op_never,
	zest_compare_op_less,
	zest_compare_op_equal,
	zest_compare_op_less_or_equal,
	zest_compare_op_greater,
	zest_compare_op_not_equal,
	zest_compare_op_greater_or_equal,
	zest_compare_op_always,
} zest_compare_op;

typedef enum {
	zest_blend_factor_zero,
	zest_blend_factor_one,
	zest_blend_factor_src_color,
	zest_blend_factor_one_minus_src_color,
	zest_blend_factor_dst_color,
	zest_blend_factor_one_minus_dst_color,
	zest_blend_factor_src_alpha,
	zest_blend_factor_one_minus_src_alpha,
	zest_blend_factor_dst_alpha,
	zest_blend_factor_one_minus_dst_alpha,
	zest_blend_factor_constant_color,
	zest_blend_factor_one_minus_constant_color,
	zest_blend_factor_constant_alpha,
	zest_blend_factor_one_minus_constant_alpha,
	zest_blend_factor_src_alpha_saturate,
	zest_blend_factor_src1_color,
	zest_blend_factor_one_minus_src1_color,
	zest_blend_factor_src1_alpha,
	zest_blend_factor_one_minus_src1_alpha,
} zest_blend_factor;

typedef enum {
	zest_blend_op_add,
	zest_blend_op_subtract,
	zest_blend_op_reverse_subtract,
	zest_blend_op_min,
	zest_blend_op_max,
} zest_blend_op;

typedef enum {
	zest_color_component_r_bit = 0x00000001,
	zest_color_component_g_bit = 0x00000002,
	zest_color_component_b_bit = 0x00000004,
	zest_color_component_a_bit = 0x00000008,
} zest_color_component_bits;

typedef enum {
	zest_image_tiling_optimal,
	zest_image_tiling_linear,
} zest_image_tiling;

typedef enum {
	zest_sample_count_1_bit = 0x00000001,
	zest_sample_count_2_bit = 0x00000002,
	zest_sample_count_4_bit = 0x00000004,
	zest_sample_count_8_bit = 0x00000008,
	zest_sample_count_16_bit = 0x00000010,
	zest_sample_count_32_bit = 0x00000020,
	zest_sample_count_64_bit = 0x00000040,
} zest_sample_count_bits;

typedef enum {
	zest_image_aspect_none = 0,
	zest_image_aspect_color_bit = 0x00000001,
	zest_image_aspect_depth_bit = 0x00000002,
	zest_image_aspect_stencil_bit = 0x00000004,
} zest_image_aspect_flag_bits;

typedef enum {
	zest_image_view_type_1d = 0,
	zest_image_view_type_2d = 1,
	zest_image_view_type_3d = 2,
	zest_image_view_type_cube = 3,
	zest_image_view_type_1d_array = 4,
	zest_image_view_type_2d_array = 5,
	zest_image_view_type_cube_array = 6,
} zest_image_view_type;

typedef enum {
	zest_image_layout_undefined = 0,
	zest_image_layout_general = 1,
	zest_image_layout_color_attachment_optimal = 2,
	zest_image_layout_depth_stencil_attachment_optimal = 3,
	zest_image_layout_depth_stencil_read_only_optimal = 4,
	zest_image_layout_shader_read_only_optimal = 5,
	zest_image_layout_transfer_src_optimal = 6,
	zest_image_layout_transfer_dst_optimal = 7,
	zest_image_layout_preinitialized = 8,
	zest_image_layout_depth_read_only_stencil_attachment_optimal = 1000117000,
	zest_image_layout_depth_attachment_stencil_read_only_optimal = 1000117001,
	zest_image_layout_depth_attachment_optimal = 1000241000,
	zest_image_layout_depth_read_only_optimal = 1000241001,
	zest_image_layout_stencil_attachment_optimal = 1000241002,
	zest_image_layout_stencil_read_only_optimal = 1000241003,
	zest_image_layout_read_only_optimal = 1000314000,
	zest_image_layout_attachment_optimal = 1000314001,
	zest_image_layout_present = 1000001002,
} zest_image_layout;

typedef enum {
	zest_access_none = 0,
	zest_access_indirect_command_read_bit = 0x00000001,
	zest_access_index_read_bit = 0x00000002,
	zest_access_vertex_attribute_read_bit = 0x00000004,
	zest_access_uniform_read_bit = 0x00000008,
	zest_access_input_attachment_read_bit = 0x00000010,
	zest_access_shader_read_bit = 0x00000020,
	zest_access_shader_write_bit = 0x00000040,
	zest_access_color_attachment_read_bit = 0x00000080,
	zest_access_color_attachment_write_bit = 0x00000100,
	zest_access_depth_stencil_attachment_read_bit = 0x00000200,
	zest_access_depth_stencil_attachment_write_bit = 0x00000400,
	zest_access_transfer_read_bit = 0x00000800,
	zest_access_transfer_write_bit = 0x00001000,
	zest_access_host_read_bit = 0x00002000,
	zest_access_host_write_bit = 0x00004000,
	zest_access_memory_read_bit = 0x00008000,
	zest_access_memory_write_bit = 0x00010000,
} zest_access_flag_bits;

typedef enum {
	zest_pipeline_stage_top_of_pipe_bit = 0x00000001,
	zest_pipeline_stage_draw_indirect_bit = 0x00000002,
	zest_pipeline_stage_vertex_input_bit = 0x00000004,
	zest_pipeline_stage_vertex_shader_bit = 0x00000008,
	zest_pipeline_stage_tessellation_control_shader_bit = 0x00000010,
	zest_pipeline_stage_tessellation_evaluation_shader_bit = 0x00000020,
	zest_pipeline_stage_geometry_shader_bit = 0x00000040,
	zest_pipeline_stage_fragment_shader_bit = 0x00000080,
	zest_pipeline_stage_early_fragment_tests_bit = 0x00000100,
	zest_pipeline_stage_late_fragment_tests_bit = 0x00000200,
	zest_pipeline_stage_color_attachment_output_bit = 0x00000400,
	zest_pipeline_stage_compute_shader_bit = 0x00000800,
	zest_pipeline_stage_transfer_bit = 0x00001000,
	zest_pipeline_stage_bottom_of_pipe_bit = 0x00002000,
	zest_pipeline_stage_host_bit = 0x00004000,
	zest_pipeline_stage_all_graphics_bit = 0x00008000,
	zest_pipeline_stage_all_commands_bit = 0x00010000,
	zest_pipeline_stage_index_input_bit =  0x00020000,
	zest_pipeline_stage_none = 0
} zest_pipeline_stage_flag_bits;

typedef enum {
	zest_vertex_shader,
	zest_fragment_shader,
	zest_compute_shader,
} zest_shader_type;

typedef zest_uint zest_image_aspect_flags;
typedef zest_uint zest_sample_count_flags;
typedef zest_uint zest_access_flags;
typedef zest_uint zest_pipeline_stage_flags;
typedef zest_uint zest_color_component_flags;

typedef enum {
	zest_load_op_load,
	zest_load_op_clear,
	zest_load_op_dont_care,
} zest_load_op;

typedef enum {
	zest_store_op_store,
	zest_store_op_dont_care,
} zest_store_op;

typedef enum {
	zest_descriptor_type_sampler,
	zest_descriptor_type_sampled_image,
	zest_descriptor_type_combined_image_sampler,
	zest_descriptor_type_storage_image,
	zest_descriptor_type_uniform_buffer,
	zest_descriptor_type_storage_buffer,
	zest_descriptor_type_count,
} zest_descriptor_type;

typedef enum {
	zest_set_layout_builder_flag_none = 0,
	zest_set_layout_builder_flag_update_after_bind = 1,
} zest_set_layout_builder_flag_bits;

typedef enum {
	zest_set_layout_flag_none = 0,
	zest_set_layout_flag_bindless = 1,
} zest_set_layout_flag_bits;

typedef enum {
	zest_semaphore_status_success,
	zest_semaphore_status_timeout,
	zest_semaphore_status_error
} zest_semaphore_status;

typedef zest_uint zest_set_layout_builder_flags;
typedef zest_uint zest_set_layout_flags;

typedef enum zest_frustum_side { zest_LEFT = 0, zest_RIGHT = 1, zest_TOP = 2, zest_BOTTOM = 3, zest_BACK = 4, zest_FRONT = 5 } zest_frustum_size;

typedef enum {
	ZEST_ALL_MIPS = 0xffffffff,
	ZEST_QUEUE_COUNT = 3,
	ZEST_GRAPHICS_QUEUE_INDEX = 0,
	ZEST_COMPUTE_QUEUE_INDEX = 1,
	ZEST_TRANSFER_QUEUE_INDEX = 2,
	ZEST_BITS_PER_WORD = (sizeof(zest_size) * 8),
	ZEST_MAX_PUSH_SIZE = 128
} zest_constants;

typedef enum {
	zest_handle_type_images,
	zest_handle_type_views,
	zest_handle_type_view_arrays,
	zest_handle_type_samplers,
	zest_handle_type_shaders,
	zest_handle_type_compute_pipelines,
	zest_max_device_handle_type
} zest_device_handle_type;

typedef enum {
	zest_handle_type_uniform_buffers,
	zest_handle_type_layers,
	zest_max_context_handle_type
} zest_context_handle_type;

//Used for memory tracking and debugging
typedef enum zest_struct_type {
	zest_struct_type_view_array = 1 << 16,
	zest_struct_type_image = 2 << 16,
	zest_struct_type_imgui_image = 3 << 16,
	zest_struct_type_image_collection = 4 << 16,
	zest_struct_type_sampler = 5 << 16,
	zest_struct_type_layer = 7 << 16,
	zest_struct_type_pipeline = 8 << 16,
	zest_struct_type_pipeline_template = 9 << 16,
	zest_struct_type_set_layout = 10 << 16,
	zest_struct_type_descriptor_set = 11 << 16,
	zest_struct_type_uniform_buffer = 13 << 16,
	zest_struct_type_buffer_allocator = 14 << 16,
	zest_struct_type_descriptor_pool = 15 << 16,
	zest_struct_type_compute = 16 << 16,
	zest_struct_type_buffer = 17 << 16,
	zest_struct_type_device_memory_pool = 18 << 16,
	zest_struct_type_timer = 19 << 16,
	zest_struct_type_window = 20 << 16,
	zest_struct_type_shader = 21 << 16,
	zest_struct_type_imgui = 22 << 16,
	zest_struct_type_queue = 23 << 16,
	zest_struct_type_execution_timeline = 24 << 16,
	zest_struct_type_frame_graph_semaphores = 25 << 16,
	zest_struct_type_swapchain = 26 << 16,
	zest_struct_type_frame_graph = 27 << 16,
	zest_struct_type_pass_node = 28 << 16,
	zest_struct_type_resource_node = 29 << 16,
	zest_struct_type_wave_submission = 30 << 16,
	zest_struct_type_device = 32 << 16,
	zest_struct_type_app = 33 << 16,
	zest_struct_type_vector = 34 << 16,
	zest_struct_type_bitmap = 35 << 16,
	zest_struct_type_render_target_group = 36 << 16,
	zest_struct_type_slang_info = 37 << 16,
	zest_struct_type_render_pass = 38 << 16,
	zest_struct_type_mesh = 39 << 16,
	zest_struct_type_texture_asset = 40 << 16,
	zest_struct_type_frame_graph_context = 41 << 16,
	zest_struct_type_atlas_region = 42 << 16,
	zest_struct_type_view = 43 << 16,
	zest_struct_type_buffer_backend = 44 << 16,
	zest_struct_type_context = 45 << 16,
	zest_struct_type_device_builder = 46 << 16,
	zest_struct_type_file = 47 << 16,
	zest_struct_type_resource_store = 48 << 16,
	zest_struct_type_buffer_linear_allocator = 49 << 16,
	zest_struct_type_pipeline_layout = 50 << 16,
} zest_struct_type;

typedef enum zest_platform_memory_context {
	zest_platform_none = 0,
	zest_platform_context = 1,
	zest_platform_device = 2,
} zest_platform_memory_context;

typedef enum zest_platform_command {
	zest_command_none,
	zest_command_surface,
	zest_command_instance,
	zest_command_logical_device,
	zest_command_debug_messenger,
	zest_command_device_instance,
	zest_command_semaphore,
	zest_command_command_pool,
	zest_command_command_buffer,
	zest_command_buffer,
	zest_command_allocate_memory_pool,
	zest_command_allocate_memory_image,
	zest_command_fence,
	zest_command_swapchain,
	zest_command_pipeline_cache,
	zest_command_descriptor_layout,
	zest_command_descriptor_pool,
	zest_command_pipeline_layout,
	zest_command_pipelines,
	zest_command_shader_module,
	zest_command_sampler,
	zest_command_image,
	zest_command_image_view,
	zest_command_render_pass,
	zest_command_frame_buffer,
	zest_command_query_pool,
	zest_command_compute_pipeline,
} zest_platform_command;

typedef enum zest_window_mode {
	zest_window_mode_bordered,
	zest_window_mode_borderless,
	zest_window_mode_fullscreen,
	zest_window_mode_fullscreen_borderless,
}zest_window_mode;

enum zest__constants {
	zest__validation_layer_count = 1,
	#if defined(__APPLE__)
	zest__required_extension_names_count = 4,
	#else
	zest__required_extension_names_count = 3,
	#endif
};

typedef enum {
	zest_left_mouse = 1,
	zest_right_mouse = 1 << 1,
	zest_middle_mouse = 1 << 2
} zest_mouse_button_e;

typedef zest_uint zest_mouse_button;

typedef enum {
	zest_render_viewport_type_scale_with_window,
	zest_render_viewport_type_fixed
} zest_render_viewport_type;

typedef enum zest_context_flag_bits {
	zest_context_flag_initialised = 1 << 0,
	zest_context_flag_schedule_recreate_textures = 1 << 1,
	zest_context_flag_schedule_change_vsync = 1 << 2,
	zest_context_flag_schedule_rerecord_final_render_buffer = 1 << 3,
	zest_context_flag_drawing_loop_running = 1 << 4,
	zest_context_flag_msaa_toggled = 1 << 5,
	zest_context_flag_vsync_enabled = 1 << 6,
	zest_context_flag_disable_default_uniform_update = 1 << 7,
	zest_context_flag_has_depth_buffer = 1 << 8,
	zest_context_flag_swap_chain_was_acquired = 1 << 9,
	zest_context_flag_work_was_submitted = 1 << 10,
	zest_context_flag_building_frame_graph = 1 << 11,
	zest_context_flag_enable_multisampling = 1 << 12,
	zest_context_flag_critical_error = 1 << 13,
} zest_context_flag_bits;

typedef zest_uint zest_context_flags;

typedef enum zest_swapchain_flag_bits {
	zest_swapchain_flag_none = 0,
	zest_swapchain_flag_has_depth_buffer = 1 << 0,
	zest_swapchain_flag_has_msaa = 1 << 1,
	zest_swapchain_flag_was_recreated = 1 << 2,
}zest_swapchain_flag_bits;

typedef zest_uint zest_swapchain_flags;

typedef enum zest_device_init_flag_bits {
	zest_device_init_flag_none = 0,
	zest_device_init_flag_cache_shaders = 1 << 0,
	zest_device_init_flag_enable_fragment_stores_and_atomics = 1 << 1,
	zest_device_init_flag_enable_validation_layers = 1 << 2,
	zest_device_init_flag_enable_validation_layers_with_sync = 1 << 3,
	zest_device_init_flag_enable_validation_layers_with_best_practices = 1 << 4,
	zest_device_init_flag_log_validation_errors_to_console = 1 << 5,
	zest_device_init_flag_log_validation_errors_to_memory = 1 << 6,
	zest_device_init_flag_output_memory_pool_info = 1 << 7,
} zest_device_init_flag_bits;

typedef zest_uint zest_device_init_flags;

typedef enum zest_context_init_flag_bits {
	zest_context_init_flag_none = 0,
	zest_context_init_flag_maximised = 1 << 1,
	zest_context_init_flag_enable_vsync = 1 << 2,
} zest_context_init_flag_bits;

typedef zest_uint zest_context_init_flags;

typedef enum zest_validation_flag_bits {
	zest_validation_flag_none = 0,
	zest_validation_flag_enable_sync = 1 << 0,
	zest_validation_flag_best_practices = 1 << 1,
} zest_validation_flag_bits;

typedef zest_uint zest_validation_flags;

typedef enum zest_buffer_upload_flag_bits {
	zest_buffer_upload_flag_initialised = 1 << 0,                //Set to true once AddCopyCommand has been run at least once
	zest_buffer_upload_flag_source_is_fif = 1 << 1,
	zest_buffer_upload_flag_target_is_fif = 1 << 2
} zest_buffer_upload_flag_bits;

typedef zest_uint zest_buffer_upload_flags;

typedef enum zest_memory_pool_flags_bits{
	zest_memory_pool_flag_none = 0,
	zest_memory_pool_flag_single_buffer = 1 << 0,
	zest_memory_pool_flag_transient = 2 << 0,
} zest_memory_pool_flag_bits;

typedef enum zest_memory_pool_type {
	zest_memory_pool_type_buffers,
	zest_memory_pool_type_images,
	zest_memory_pool_type_transient_buffers,
	zest_memory_pool_type_transient_images,
	zest_memory_pool_type_small_buffers,
	zest_memory_pool_type_small_transient_buffers,
} zest_memory_pool_type;

typedef zest_uint zest_memory_pool_flags;
typedef zest_uint zest_buffer_flags;

typedef enum zest_command_buffer_flag_bits {
	zest_command_buffer_flag_none = 0,
	zest_command_buffer_flag_outdated = 1 << 0,
	zest_command_buffer_flag_primary = 1 << 1,
	zest_command_buffer_flag_secondary = 1 << 2,
	zest_command_buffer_flag_recording = 1 << 3,
} zest_command_buffer_flag_bits;

typedef zest_uint zest_command_buffer_flags;

//Be more careful about changing these numbers as they correlate to shaders. See zest_shape_type.
typedef enum {
	zest_draw_mode_none = 0,            //Default no drawmode set when no drawing has been done yet
	zest_draw_mode_instance = 1,
	zest_draw_mode_images = 2,
	zest_draw_mode_mesh = 3,
	zest_draw_mode_line_instance = 4,
	zest_draw_mode_rect_instance = 5,
	zest_draw_mode_dashed_line = 6,
	zest_draw_mode_text = 7,
	zest_draw_mode_fill_screen = 8,
	zest_draw_mode_viewport = 9,
	zest_draw_mode_im_gui = 10,
	zest_draw_mode_mesh_instance = 11,
} zest_draw_mode;

typedef enum {
	zest_shape_line = zest_draw_mode_line_instance,
	zest_shape_dashed_line = zest_draw_mode_dashed_line,
	zest_shape_rect = zest_draw_mode_rect_instance
} zest_shape_type;

typedef enum {
	zest_imgui_blendtype_none,					//Just draw with standard alpha blend
	zest_imgui_blendtype_pass,                  //Force the alpha channel to 1
	zest_imgui_blendtype_premultiply            //Divide the color channels by the alpha channel
} zest_imgui_blendtype;

typedef enum zest_image_flag_bits {
	zest_image_flag_none = 0,

	// --- Memory & Access Flags ---
	// Resides on dedicated GPU memory for fastest access. This is the default if host_visible is not set.
	zest_image_flag_device_local = 1 << 0,
	// Accessible by the CPU (mappable). The backend will likely use linear tiling.
	zest_image_flag_host_visible = 1 << 1,
	// When used with host_visible, CPU writes are visible to the GPU without manual flushing.
	zest_image_flag_host_coherent = 1 << 2,

	// --- GPU Usage Flags ---
	// Can be sampled in a shader (e.g., as a texture).
	zest_image_flag_sampled = 1 << 3,
	// Can be used for general read/write in shaders (storage image).
	zest_image_flag_storage = 1 << 4,
	// Can be a color attachment in a render pass.
	zest_image_flag_color_attachment = 1 << 5,
	// Can be a depth or stencil attachment.
	zest_image_flag_depth_stencil_attachment = 1 << 6,
	// Can be the source of a transfer operation (copy, blit).
	zest_image_flag_transfer_src = 1 << 7,
	// Can be the destination of a transfer operation.
	zest_image_flag_transfer_dst = 1 << 8,
	// Can be used as an input attachment for sub-passes.
	zest_image_flag_input_attachment = 1 << 9,

	// --- Configuration Flags ---
	// Image is a cubemap. This will create a 2D array image with 6 layers.
	zest_image_flag_cubemap = 1 << 10,
	// The chosen image format must support linear filtering.
	zest_image_flag_allow_linear_filtering = 1 << 11,
	// Automatically generate mipmaps when image data is uploaded. Implies transfer_src and transfer_dst.
	zest_image_flag_generate_mipmaps = 1 << 12,

	// Allows the image to be viewed with a different but compatible format (e.g., UNORM vs SRGB).
	zest_image_flag_mutable_format = 1 << 13,
	// Allows a 3D image to be viewed as a 2D array.
	zest_image_flag_3d_as_2d_array = 1 << 14,
	// For multi-planar formats (e.g., video), indicating each plane can have separate memory.
	zest_image_flag_disjoint_planes = 1 << 15,

	//For transient images in a frame graph
	zest_image_flag_transient = 1 << 16,

	//Convenient preset flags for common usages
	// For a standard texture loaded from CPU.
	zest_image_preset_texture = zest_image_flag_sampled | zest_image_flag_transfer_dst | zest_image_flag_device_local,

	// For a standard texture with mipmap generation.
	zest_image_preset_texture_mipmaps = zest_image_preset_texture | zest_image_flag_transfer_src | zest_image_flag_generate_mipmaps,

	// For a render target that can be sampled (e.g., post-processing).
	zest_image_preset_color_attachment = zest_image_flag_color_attachment | zest_image_flag_sampled | zest_image_flag_device_local,

	// For a depth buffer that can be sampled (e.g., shadow mapping, SSAO).
	zest_image_preset_depth_attachment = zest_image_flag_depth_stencil_attachment | zest_image_flag_sampled | zest_image_flag_device_local,

	// For an image to be written to by a compute shader and then sampled.
	zest_image_preset_storage = zest_image_flag_storage | zest_image_flag_sampled | zest_image_flag_device_local,

	// For working on cubemaps in a compute shader, like preparing for pbr lighting
	zest_image_preset_storage_cubemap = zest_image_flag_storage | zest_image_flag_sampled | zest_image_flag_device_local | zest_image_flag_cubemap,

	// For working on cubemaps in a compute shader with mipmaps, like preparing for pbr lighting
	zest_image_preset_storage_mipped_cubemap = zest_image_flag_storage | zest_image_flag_sampled | zest_image_flag_device_local | zest_image_flag_generate_mipmaps | zest_image_flag_cubemap,

} zest_image_flag_bits;

typedef zest_uint zest_image_flags;
typedef zest_uint zest_capability_flags;

typedef enum {
	zest_buffer_type_staging,		//Used to upload data from CPU side to GPU
	zest_buffer_type_vertex,		//Any kind of data for storing vertices used in vertex shaders
	zest_buffer_type_index,			//Index data for use in vertex shaders
	zest_buffer_type_uniform,		//Small buffers for uploading data to the GPU every frame
	zest_buffer_type_storage,		//General purpose storage buffers mainly for compute but any other shader type can access too
	zest_buffer_type_indirect,		
	zest_buffer_type_vertex_storage,//Vertex data that can also be accessed/written to by the GPU
	zest_buffer_type_index_storage	//Index data that can also be accessed/written to by the GPU
} zest_buffer_type;

typedef enum {
	zest_memory_usage_gpu_only,
	zest_memory_usage_cpu_to_gpu,
	zest_memory_usage_gpu_to_cpu,
} zest_memory_usage;

typedef enum zest_camera_flag_bits {
	zest_camera_flags_none = 0,
	zest_camera_flags_perspective = 1 << 0,
	zest_camera_flags_orthagonal = 1 << 1,
} zest_camera_flag_bits;

typedef zest_uint zest_camera_flags;

typedef enum zest_compute_flag_bits {
	zest_compute_flag_none = 0,
	zest_compute_flag_rewrite_command_buffer = 1 << 1,    // Command buffer for this compute shader should be rewritten
	zest_compute_flag_sync_required = 1 << 2,    // Compute shader requires syncing with the render target
	zest_compute_flag_is_active = 1 << 3,    // Compute shader is active then it will be updated when the swap chain is recreated
	zest_compute_flag_manual_fif = 1 << 4,    // You decide when the compute shader should be re-recorded
	zest_compute_flag_primary_recorder = 1 << 5,    // For using with zest_RunCompute only. You will not be able to use this compute as part of a frame render
} zest_compute_flag_bits;

typedef enum zest_layer_flag_bits {
	zest_layer_flag_none = 0,
	zest_layer_flag_static = 1 << 0,    // Layer only uploads new buffer data when required
	zest_layer_flag_device_local_direct = 1 << 1,    // Upload directly to device buffer (has issues so is disabled by default for now)
	zest_layer_flag_manual_fif = 1 << 2,    // Manually set the frame in flight for the layer
	zest_layer_flag_using_global_bindless_layout = 1 << 3,    // Flagged if the layer is automatically setting the descriptor array index for the device buffers
	zest_layer_flag_dont_reset_instructions = 1 << 4,    // Flagged if the layer is automatically setting the descriptor array index for the device buffers
} zest_layer_flag_bits;

typedef enum zest_draw_buffer_result {
	zest_draw_buffer_result_ok,
	zest_draw_buffer_result_buffer_grew,
	zest_draw_buffer_result_failed_to_grow
} zest_draw_buffer_result;

typedef enum {
	zest_query_state_none,
	zest_query_state_ready,
} zest_query_state;

typedef enum {
	zest_queue_graphics = 1,
	zest_queue_compute = 1 << 1,
	zest_queue_transfer = 1 << 2
} zest_device_queue_type;

typedef enum {
	zest_resource_type_none = 0,
	zest_resource_type_image = 1 << 0,
	zest_resource_type_buffer = 1 << 1,
	zest_resource_type_vertex_buffer = 1 << 2,
	zest_resource_type_index_buffer = 1 << 3,
	zest_resource_type_swap_chain_image = 1 << 4,
	zest_resource_type_depth = 1 << 5,
	zest_resource_type_is_image = zest_resource_type_image | zest_resource_type_swap_chain_image | zest_resource_type_depth,
	zest_resource_type_is_image_or_depth = zest_resource_type_image | zest_resource_type_depth
} zest_resource_type_bits;

typedef zest_uint zest_resource_type;

typedef enum zest_resource_node_flag_bits {
	zest_resource_node_flag_none = 0,
	zest_resource_node_flag_transient = 1 << 0,
	zest_resource_node_flag_imported = 1 << 1,
	zest_resource_node_flag_used_in_output = 1 << 2,
	zest_resource_node_flag_is_bindless = 1 << 3,
	zest_resource_node_flag_release_after_use = 1 << 4,
	zest_resource_node_flag_essential_output = 1 << 5,
	zest_resource_node_flag_requires_storage = 1 << 6,
	zest_resource_node_flag_aliased = 1 << 7,
	zest_resource_node_flag_has_producer = 1 << 8,
	zest_resource_node_flag_preserve = 1 << 9,
	zest_resource_node_flag_read_only = 1 << 10,
} zest_resource_node_flag_bits;

typedef zest_uint zest_resource_node_flags;

typedef enum zest_resource_usage_hint_bits {
	zest_resource_usage_hint_none = 0,
	zest_resource_usage_hint_copy_src = 1 << 0,
	zest_resource_usage_hint_copy_dst = 1 << 1,
	zest_resource_usage_hint_cpu_read = (1 << 2),
	zest_resource_usage_hint_cpu_write = (1 << 3),
	zest_resource_usage_hint_vertex_buffer = 1 << 4,
	zest_resource_usage_hint_index_buffer = 1 << 5,
	zest_resource_usage_hint_msaa = 1 << 6,
	zest_resource_usage_hint_copyable = zest_resource_usage_hint_copy_src | zest_resource_usage_hint_copy_dst,
	zest_resource_usage_hint_cpu_transfer = zest_resource_usage_hint_cpu_read | zest_resource_usage_hint_cpu_write
} zest_resource_usage_hint_bits;

typedef zest_uint zest_resource_usage_hint;

typedef enum zest_frame_graph_flag_bits {
	zest_frame_graph_flag_none = 0,
	zest_frame_graph_expecting_swap_chain_usage = 1 << 0,
	zest_frame_graph_is_compiled = 1 << 1,
	zest_frame_graph_is_executed = 1 << 2,
	zest_frame_graph_present_after_execute = 1 << 3,
	zest_frame_graph_is_cached = 1 << 4,
} zest_frame_graph_flag_bits;

typedef zest_uint zest_frame_graph_flags;

typedef enum {
	zest_access_write_bits_general = zest_access_shader_write_bit | zest_access_color_attachment_write_bit | zest_access_depth_stencil_attachment_write_bit | zest_access_transfer_write_bit,
	zest_access_read_bits_general = zest_access_shader_read_bit | zest_access_color_attachment_read_bit | zest_access_depth_stencil_attachment_read_bit | zest_access_transfer_read_bit | zest_access_index_read_bit | zest_access_vertex_attribute_read_bit | zest_access_indirect_command_read_bit,
	zest_access_render_pass_bits = zest_access_depth_stencil_attachment_read_bit | zest_access_color_attachment_read_bit | zest_access_depth_stencil_attachment_write_bit | zest_access_color_attachment_write_bit,
} zest_general_access_bits;

typedef enum zest_resource_purpose {
	zest_purpose_none = 0,
	// Buffer Usages
	zest_purpose_vertex_buffer = 1,
	zest_purpose_index_buffer,
	zest_purpose_uniform_buffer,                      // Might need shader stage too if general
	zest_purpose_storage_buffer_read,                 // Needs shader stage
	zest_purpose_storage_buffer_write,                // Needs shader stage
	zest_purpose_storage_buffer_read_write,           // Needs shader stage
	zest_purpose_indirect_buffer,
	zest_purpose_transfer_buffer,

	// Image Usages
	zest_purpose_sampled_image,                       // Needs shader stage
	zest_purpose_storage_image_read,                  // Needs shader stage
	zest_purpose_storage_image_write,                 // Needs shader stage
	zest_purpose_storage_image_read_write,            // Needs shader stage
	zest_purpose_color_attachment_write,
	zest_purpose_color_attachment_resolve,
	zest_purpose_color_attachment_read,               // For blending or input attachments
	zest_purpose_depth_stencil_attachment_read,
	zest_purpose_depth_stencil_attachment_write,
	zest_purpose_depth_stencil_attachment_read_write, // Common
	zest_purpose_input_attachment,                    // Needs shader stage (typically fragment)
	zest_purpose_transfer_image,
	zest_purpose_present_src,                         // For swapchain final layout
} zest_resource_purpose;

typedef enum zest_pass_flag_bits {
	zest_pass_flag_none = 0,
	zest_pass_flag_disabled = 1,
	zest_pass_flag_do_not_cull = 1 << 1,
	zest_pass_flag_culled = 1 << 2,
	zest_pass_flag_output_resolve = 1 << 3,
	zest_pass_flag_outputs_to_swapchain = 1 << 4,
	zest_pass_flag_sync_only = 1 << 5,
} zest_pass_flag_bits;

typedef enum zest_pass_type {
	zest_pass_type_graphics = 1,
	zest_pass_type_compute = 2,
	zest_pass_type_transfer = 3,
	zest_pass_type_sync = 4,
} zest_pass_type;

typedef enum zest_dynamic_resource_type {
	zest_dynamic_resource_none = 0,
	zest_dynamic_resource_image_available_semaphore,
	zest_dynamic_resource_render_finished_semaphore,
} zest_dynamic_resource_type;

//pipeline_enums
typedef enum zest_pipeline_set_flag_bits {
	zest_pipeline_set_flag_none = 0,
	zest_pipeline_set_flag_is_render_target_pipeline = 1 << 0,        //True if this pipeline is used for the final render of a render target to the swap chain
	zest_pipeline_set_flag_match_swapchain_view_extent_on_rebuild = 1 << 1,        //True if the pipeline should update it's view extent when the swap chain is recreated (the window is resized)
	zest_pipeline_invalid = 1 << 2        //The pipeline had validation errors when it was built and may be invalid
} zest_pipeline_set_flag_bits;

typedef zest_uint zest_pipeline_set_flags;

typedef enum zest_pipeline_bind_point {
	zest_bind_point_graphics,
	zest_bind_point_compute,
	zest_bind_point_none = 0xFFFFFFFF
} zest_pipeline_bind_point;

typedef enum zest_front_face {
	zest_front_face_counter_clockwise,
	zest_front_face_clockwise,
} zest_front_face;

typedef enum zest_topology {
	zest_topology_point_list,
	zest_topology_line_list,
	zest_topology_line_strip,
	zest_topology_triangle_list,
	zest_topology_triangle_strip,
	zest_topology_triangle_fan,
	zest_topology_line_list_with_adjacency,
	zest_topology_line_strip_with_adjacency,
	zest_topology_triangle_list_with_adjacency,
	zest_topology_triangle_strip_with_adjacency,
	zest_topology_patch_list,
} zest_topology;

typedef enum zest_cull_mode {
	zest_cull_mode_none,
	zest_cull_mode_front,
	zest_cull_mode_back,
	zest_cull_mode_front_and_back,
} zest_cull_mode;

typedef enum zest_polygon_mode {
	zest_polygon_mode_fill,
	zest_polygon_mode_line,
	zest_polygon_mode_point,
} zest_polygon_mode;

typedef enum zest_supported_shader_stage_bits {
	zest_shader_vertex_stage = 1,
	zest_shader_fragment_stage = 16,
	zest_shader_compute_stage = 32,
	zest_shader_render_stages = zest_shader_vertex_stage | zest_shader_fragment_stage,
	zest_shader_all_stages = zest_shader_render_stages | zest_shader_compute_stage,
} zest_supported_shader_stage_bits;

typedef enum zest_report_category {
	zest_report_unused_pass,
	zest_report_taskless_pass,
	zest_report_no_work,
	zest_report_unconnected_resource,
	zest_report_pass_culled,
	zest_report_resource_culled,
	zest_report_invalid_layer,
	zest_report_cyclic_dependency,
	zest_report_invalid_render_pass,
	zest_report_render_pass_skipped,
	zest_report_expecting_swapchain_usage,
	zest_report_swapchain_not_acquired,
	zest_report_bindless_indexes,
	zest_report_invalid_reference_counts,
	zest_report_missing_end_pass,
	zest_report_invalid_resource,
	zest_report_invalid_pass,
	zest_report_multiple_swapchains,
	zest_report_cannot_execute,
	zest_report_pipeline_invalid,
	zest_report_no_frame_graphs_to_execute,
	zest_report_incompatible_stage_for_queue,
	zest_report_unused_swapchain,
	zest_report_last_batch_already_signalled,
	zest_report_layers,
	zest_report_memory,
} zest_report_category;

typedef enum zest_binding_number_type {
	zest_sampler_binding = 0,
	zest_texture_2d_binding,
	zest_texture_cube_binding,
	zest_texture_array_binding,
	zest_texture_3d_binding,
	zest_storage_buffer_binding,
	zest_storage_image_binding,
	zest_uniform_buffer_binding,
	zest_max_global_binding_number
} zest_binding_number_type;

typedef enum zest_frame_graph_result_bits {
	zest_fgs_success = 0,
	zest_fgs_no_work_to_do = 1 << 0,
	zest_fgs_cyclic_dependency = 1 << 1,
	zest_fgs_invalid_render_pass = 1 << 2,
	zest_fgs_critical_error = 1 << 3,
	zest_fgs_transient_resource_failure = 1 << 4,
	zest_fgs_unable_to_acquire_command_buffer = 1 << 5,
	zest_fgs_out_of_memory = 1 << 6,
} zest_frame_graph_result_bits;

typedef enum zest_pass_node_visit_state {
	zest_pass_node_unvisited = 0,
	zest_pass_node_visiting,
	zest_pass_node_visited,
} zest_pass_node_visit_state;

typedef zest_uint zest_supported_shader_stages;		         //zest_shader_stage_bits
typedef zest_uint zest_compute_flags;		                 //zest_compute_flag_bits
typedef zest_uint zest_layer_flags;                          //zest_layer_flag_bits
typedef zest_uint zest_pass_flags;                           //zest_pass_flag_bits
typedef zest_uint zest_frame_graph_result;                   //zest_frame_graph_result_bits

typedef void(*zloc__block_output)(void* ptr, size_t size, int used, void* user, int is_final_output);

#define ZEST_STRUCT_IDENTIFIER 0x4E57
#define zest_INIT_MAGIC(struct_type) (struct_type | ZEST_STRUCT_IDENTIFIER);

#define ZEST_ASSERT_HANDLE(handle) ZEST_ASSERT(handle && (*((int*)handle) & 0xFFFF) == ZEST_STRUCT_IDENTIFIER)
#define ZEST_ASSERT_BUFFER_HANDLE(handle) ZEST_ASSERT(handle && (*((int*)((char*)handle + offsetof(zest_buffer_t, magic))) & 0xFFFF) == ZEST_STRUCT_IDENTIFIER)
#define ZEST_VALID_IDENTIFIER(handle) (handle && (*((int*)handle) & 0xFFFF) == ZEST_STRUCT_IDENTIFIER)
#define ZEST_STRUCT_TYPE(handle) (zest_struct_type)(*((int*)handle) & 0xFFFF0000)
#define ZEST_VALID_HANDLE(handle, struct_type) (ZEST_VALID_IDENTIFIER(handle) && ZEST_STRUCT_TYPE(handle) == struct_type)
#define ZEST_STRUCT_MAGIC_TYPE(magic) (magic & 0xFFFF0000)
#define ZEST_IS_INTITIALISED(magic) (magic & 0xFFFF) == ZEST_STRUCT_IDENTIFIER

#define ZEST_SET_MEMORY_CONTEXT(device, mem_context, command) device->platform_memory_info.timestamp = device->allocation_id++; \
device->platform_memory_info.context_info = ZEST_STRUCT_IDENTIFIER | (mem_context << 16) | (command << 24)


// --Forward_declarations
typedef struct zest_context_t zest_context_t;
typedef struct zest_device_t zest_device_t;
typedef struct zest_device_builder_t zest_device_builder_t;
typedef struct zest_platform_t zest_platform_t;
typedef struct zest_image_t zest_image_t;
typedef struct zest_image_view_t zest_image_view_t;
typedef struct zest_image_view_array_t zest_image_view_array_t;
typedef struct zest_sampler_t zest_sampler_t;
typedef struct zest_layer_t zest_layer_t;
typedef struct zest_pipeline_t zest_pipeline_t;
typedef struct zest_pipeline_template_t zest_pipeline_template_t;
typedef struct zest_pipeline_layout_t zest_pipeline_layout_t;
typedef struct zest_set_layout_t zest_set_layout_t;
typedef struct zest_descriptor_set_t zest_descriptor_set_t;
typedef struct zest_descriptor_pool_t zest_descriptor_pool_t;
typedef struct zest_uniform_buffer_t zest_uniform_buffer_t;
typedef struct zest_buffer_allocator_t zest_buffer_allocator_t;
typedef struct zest_buffer_linear_allocator_t zest_buffer_linear_allocator_t;
typedef struct zest_compute_t zest_compute_t;
typedef struct zest_buffer_t zest_buffer_t;
typedef struct zest_device_memory_pool_t zest_device_memory_pool_t;
typedef struct zest_device_memory_t zest_device_memory_t;
typedef struct zest_timer_t zest_timer_t;
typedef struct zest_shader_t zest_shader_t;
typedef struct zest_frame_graph_semaphores_t zest_frame_graph_semaphores_t;
typedef struct zest_frame_graph_t zest_frame_graph_t;
typedef struct zest_frame_graph_builder_t zest_frame_graph_builder_t;
typedef struct zest_pass_node_t zest_pass_node_t;
typedef struct zest_pass_group_t zest_pass_group_t;
typedef struct zest_execution_details_t zest_execution_details_t;
typedef struct zest_execution_barriers_t zest_execution_barriers_t;
typedef struct zest_submission_batch_t zest_submission_batch_t;
typedef struct zest_wave_submission_t zest_wave_submission_t;
typedef struct zest_resource_node_t zest_resource_node_t;
typedef struct zest_queue_t zest_queue_t;
typedef struct zest_queue_manager_t zest_queue_manager_t;
typedef struct zest_context_queue_t zest_context_queue_t;
typedef struct zest_execution_timeline_t zest_execution_timeline_t;
typedef struct zest_swapchain_t zest_swapchain_t;
typedef struct zest_resource_group_t zest_resource_group_t;
typedef struct zest_rendering_info_t zest_rendering_info_t;
typedef struct zest_mesh_t zest_mesh_t;
typedef struct zest_mesh_offset_data_t zest_mesh_offset_data_t;
typedef struct zest_command_list_t zest_command_list_t;
typedef struct zest_resource_store_t zest_resource_store_t; 

//Backends
typedef struct zest_device_backend_t zest_device_backend_t;
typedef struct zest_context_backend_t zest_context_backend_t;
typedef struct zest_swapchain_backend_t zest_swapchain_backend_t;
typedef struct zest_queue_backend_t zest_queue_backend_t;
typedef struct zest_context_queue_backend_t zest_context_queue_backend_t;
typedef struct zest_command_list_backend_t zest_command_list_backend_t;
typedef struct zest_buffer_allocator_backend_t zest_buffer_allocator_backend_t;
typedef struct zest_buffer_linear_allocator_backend_t zest_buffer_linear_allocator_backend_t;
typedef struct zest_device_memory_pool_backend_t zest_device_memory_pool_backend_t;
typedef struct zest_device_memory_backend_t zest_device_memory_backend_t;
typedef struct zest_uniform_buffer_backend_t zest_uniform_buffer_backend_t;
typedef struct zest_descriptor_pool_backend_t zest_descriptor_pool_backend_t;
typedef struct zest_descriptor_set_backend_t zest_descriptor_set_backend_t;
typedef struct zest_set_layout_backend_t zest_set_layout_backend_t;
typedef struct zest_pipeline_backend_t zest_pipeline_backend_t;
typedef struct zest_pipeline_layout_backend_t zest_pipeline_layout_backend_t;
typedef struct zest_compute_backend_t zest_compute_backend_t;
typedef struct zest_image_backend_t zest_image_backend_t;
typedef struct zest_image_view_backend_t zest_image_view_backend_t;
typedef struct zest_sampler_backend_t zest_sampler_backend_t;
typedef struct zest_submission_batch_backend_t zest_submission_batch_backend_t;
typedef struct zest_execution_backend_t zest_execution_backend_t;
typedef struct zest_frame_graph_semaphores_backend_t zest_frame_graph_semaphores_backend_t;
typedef struct zest_execution_timeline_backend_t zest_execution_timeline_backend_t;
typedef struct zest_execution_barriers_backend_t zest_execution_barriers_backend_t;
typedef struct zest_set_layout_builder_backend_t zest_set_layout_builder_backend_t;

//Generate handles for the struct types. These are all pointers to memory where the object is stored.
ZEST__MAKE_HANDLE(zest_context)
ZEST__MAKE_HANDLE(zest_device)
ZEST__MAKE_HANDLE(zest_device_builder)
ZEST__MAKE_HANDLE(zest_platform)
ZEST__MAKE_HANDLE(zest_image)
ZEST__MAKE_HANDLE(zest_image_view)
ZEST__MAKE_HANDLE(zest_image_view_array)
ZEST__MAKE_HANDLE(zest_sampler)
ZEST__MAKE_HANDLE(zest_layer)
ZEST__MAKE_HANDLE(zest_pipeline)
ZEST__MAKE_HANDLE(zest_pipeline_template)
ZEST__MAKE_HANDLE(zest_pipeline_layout)
ZEST__MAKE_HANDLE(zest_set_layout)
ZEST__MAKE_HANDLE(zest_descriptor_set)
ZEST__MAKE_HANDLE(zest_uniform_buffer)
ZEST__MAKE_HANDLE(zest_buffer_allocator)
ZEST__MAKE_HANDLE(zest_buffer_linear_allocator)
ZEST__MAKE_HANDLE(zest_descriptor_pool)
ZEST__MAKE_HANDLE(zest_compute)
ZEST__MAKE_HANDLE(zest_buffer)
ZEST__MAKE_HANDLE(zest_device_memory_pool)
ZEST__MAKE_HANDLE(zest_device_memory)
ZEST__MAKE_HANDLE(zest_shader)
ZEST__MAKE_HANDLE(zest_queue)
ZEST__MAKE_HANDLE(zest_queue_manager)
ZEST__MAKE_HANDLE(zest_context_queue)
ZEST__MAKE_HANDLE(zest_execution_timeline)
ZEST__MAKE_HANDLE(zest_frame_graph_semaphores)
ZEST__MAKE_HANDLE(zest_swapchain)
ZEST__MAKE_HANDLE(zest_frame_graph)
ZEST__MAKE_HANDLE(zest_frame_graph_builder)
ZEST__MAKE_HANDLE(zest_pass_node)
ZEST__MAKE_HANDLE(zest_resource_node)
ZEST__MAKE_HANDLE(zest_resource_group);
ZEST__MAKE_HANDLE(zest_render_pass)
ZEST__MAKE_HANDLE(zest_mesh)
ZEST__MAKE_HANDLE(zest_command_list)

ZEST__MAKE_HANDLE(zest_device_backend)
ZEST__MAKE_HANDLE(zest_context_backend)
ZEST__MAKE_HANDLE(zest_swapchain_backend)
ZEST__MAKE_HANDLE(zest_queue_backend)
ZEST__MAKE_HANDLE(zest_context_queue_backend)
ZEST__MAKE_HANDLE(zest_command_list_backend)
ZEST__MAKE_HANDLE(zest_frame_graph_semaphores_backend)
ZEST__MAKE_HANDLE(zest_buffer_allocator_backend)
ZEST__MAKE_HANDLE(zest_buffer_linear_allocator_backend)
ZEST__MAKE_HANDLE(zest_device_memory_pool_backend)
ZEST__MAKE_HANDLE(zest_device_memory_backend)
ZEST__MAKE_HANDLE(zest_descriptor_pool_backend)
ZEST__MAKE_HANDLE(zest_descriptor_set_backend)
ZEST__MAKE_HANDLE(zest_set_layout_backend)
ZEST__MAKE_HANDLE(zest_pipeline_backend)
ZEST__MAKE_HANDLE(zest_pipeline_layout_backend)
ZEST__MAKE_HANDLE(zest_compute_backend)
ZEST__MAKE_HANDLE(zest_image_backend)
ZEST__MAKE_HANDLE(zest_image_view_backend)
ZEST__MAKE_HANDLE(zest_sampler_backend)
ZEST__MAKE_HANDLE(zest_submission_batch_backend)
ZEST__MAKE_HANDLE(zest_execution_backend)
ZEST__MAKE_HANDLE(zest_execution_timeline_backend)
ZEST__MAKE_HANDLE(zest_execution_barriers_backend)
ZEST__MAKE_HANDLE(zest_set_layout_builder_backend)

ZEST__MAKE_USER_HANDLE(zest_image)
ZEST__MAKE_USER_HANDLE(zest_image_view)
ZEST__MAKE_USER_HANDLE(zest_image_view_array)
ZEST__MAKE_USER_HANDLE(zest_sampler)
ZEST__MAKE_USER_HANDLE(zest_uniform_buffer)
ZEST__MAKE_USER_HANDLE(zest_layer)
ZEST__MAKE_USER_HANDLE(zest_shader)
ZEST__MAKE_USER_HANDLE(zest_compute)

//Threading

#ifdef _WIN32
#include <windows.h>
#include <process.h>
#else
#include <pthread.h>
#endif

#ifndef ZEST_MAX_QUEUES
#define ZEST_MAX_QUEUES 64
#endif

#ifndef ZEST_MAX_QUEUE_ENTRIES
#define ZEST_MAX_QUEUE_ENTRIES 512
#endif

#ifndef ZEST_MAX_THREADS
#define ZEST_MAX_THREADS 64
#endif

// Platform-specific synchronization wrapper
typedef struct zest_sync_t {
	#ifdef _WIN32
	CRITICAL_SECTION mutex;
	CONDITION_VARIABLE empty_condition;
	CONDITION_VARIABLE full_condition;
	#else
	pthread_mutex_t mutex;
	pthread_cond_t empty_condition;
	pthread_cond_t full_condition;
	#endif
} zest_sync_t;

// Platform-specific atomic operations
ZEST_PRIVATE inline zest_uint zest__atomic_increment(volatile zest_uint *value) {
	#ifdef _WIN32
	return InterlockedIncrement((LONG *)value);
	#else
	return __sync_fetch_and_add(value, 1) + 1;
	#endif
}

ZEST_PRIVATE inline int zest__atomic_compare_exchange(volatile int *dest, int exchange, int comparand) {
	#ifdef _WIN32
	return InterlockedCompareExchange((LONG *)dest, exchange, comparand) == comparand;
	#else
	return __sync_bool_compare_and_swap(dest, comparand, exchange);
	#endif
}

ZEST_PRIVATE inline zest_size zest__atomic_fetch_and(volatile zest_size *ptr, zest_size mask) {
	#ifdef _WIN32
	#if defined(_WIN64)
	return InterlockedAnd64((LONG64 *)ptr, mask);
	#else
	return InterlockedAnd((LONG *)ptr, mask);
	#endif
	#else
	return __sync_fetch_and_and(ptr, mask);
	#endif
}

ZEST_PRIVATE inline zest_size zest__atomic_fetch_or(volatile zest_size *ptr, zest_size mask) {
	#ifdef _WIN32
	#if defined(_WIN64)
	return InterlockedOr64((LONG64 *)ptr, mask);
	#else
	return InterlockedOr((LONG *)ptr, mask);
	#endif
	#else
	return __sync_fetch_and_or(ptr, mask);
	#endif
}

ZEST_API inline void zest__atomic_store(volatile int *ptr, int value) {
	#ifdef _WIN32
	InterlockedExchange((LONG *)ptr, value);
	#else
	__sync_lock_test_and_set(ptr, value);
	#endif
}

ZEST_API inline int zest__atomic_load(volatile int *ptr) {
	#ifdef _WIN32
	return InterlockedOr((LONG *)ptr, 0);  // OR with 0 = read without modifying
	#else
	return __sync_fetch_and_or(ptr, 0);
	#endif
}

ZEST_PRIVATE inline void zest__memory_barrier(void) {
	#ifdef _WIN32
	MemoryBarrier();
	#else
	__sync_synchronize();
	#endif
}

// Initialize synchronization primitives
ZEST_PRIVATE inline void zest__sync_init(zest_sync_t *sync) {
	#ifdef _WIN32
	InitializeCriticalSection(&sync->mutex);
	InitializeConditionVariable(&sync->empty_condition);
	InitializeConditionVariable(&sync->full_condition);
	#else
	pthread_mutex_init(&sync->mutex, NULL);
	pthread_cond_init(&sync->empty_condition, NULL);
	pthread_cond_init(&sync->full_condition, NULL);
	#endif
}

ZEST_PRIVATE inline void zest__sync_cleanup(zest_sync_t *sync) {
	#ifdef _WIN32
	DeleteCriticalSection(&sync->mutex);
	#else
	pthread_mutex_destroy(&sync->mutex);
	pthread_cond_destroy(&sync->empty_condition);
	pthread_cond_destroy(&sync->full_condition);
	#endif
}

ZEST_PRIVATE inline void zest__sync_lock(zest_sync_t *sync) {
	#ifdef _WIN32
	EnterCriticalSection(&sync->mutex);
	#else
	pthread_mutex_lock(&sync->mutex);
	#endif
}

ZEST_PRIVATE inline void zest__sync_unlock(zest_sync_t *sync) {
	#ifdef _WIN32
	LeaveCriticalSection(&sync->mutex);
	#else
	pthread_mutex_unlock(&sync->mutex);
	#endif
}

ZEST_PRIVATE inline void zest__sync_wait_empty(zest_sync_t *sync) {
	#ifdef _WIN32
	SleepConditionVariableCS(&sync->empty_condition, &sync->mutex, INFINITE);
	#else
	pthread_cond_wait(&sync->empty_condition, &sync->mutex);
	#endif
}

ZEST_PRIVATE inline void zest__sync_wait_full(zest_sync_t *sync) {
	#ifdef _WIN32
	SleepConditionVariableCS(&sync->full_condition, &sync->mutex, INFINITE);
	#else
	pthread_cond_wait(&sync->full_condition, &sync->mutex);
	#endif
}

ZEST_PRIVATE inline void zest__sync_signal_empty(zest_sync_t *sync) {
	#ifdef _WIN32
	WakeConditionVariable(&sync->empty_condition);
	#else
	pthread_cond_signal(&sync->empty_condition);
	#endif
}

ZEST_PRIVATE inline void zest__sync_signal_full(zest_sync_t *sync) {
	#ifdef _WIN32
	WakeConditionVariable(&sync->full_condition);
	#else
	pthread_cond_signal(&sync->full_condition);
	#endif
}

ZEST_PRIVATE inline unsigned int zest_HardwareConcurrency(void) {
	#ifdef _WIN32
	SYSTEM_INFO sysinfo;
	GetSystemInfo(&sysinfo);
	return sysinfo.dwNumberOfProcessors;
	#else
	#ifdef _SC_NPROCESSORS_ONLN
	long count = sysconf(_SC_NPROCESSORS_ONLN);
	return (count > 0) ? (unsigned int)count : 0;
	#else
	return 0;
	#endif
	#endif
}

// Safe version that always returns at least 1
ZEST_API unsigned int zest_HardwareConcurrencySafe(void);

// Helper function to get a good default thread count for thread pools
// Usually hardware threads - 1 to leave a core for the OS/main thread
ZEST_API unsigned int zest_GetDefaultThreadCount(void);

// --Private structs with inline functions
typedef struct zest_queue_family_indices {
	zest_uint graphics_family_index;
	zest_uint compute_family_index;
	zest_uint transfer_family_index;
	float *graphics_priorities;
	float *compute_priorities;
	float *transfer_priorities;
} zest_queue_family_indices;

// --Pocket Dynamic Array
typedef struct zest_vec {
	int magic;  //For allocation tracking
	int id;     //and finding leaks.
	volatile zest_uint current_size;
	zest_uint capacity;
} zest_vec;

enum {
	zest__VEC_HEADER_OVERHEAD = sizeof(zest_vec)
};

ZEST_API_TMP void* zest__vec_linear_reserve(zloc_linear_allocator_t *allocator, void *T, zest_uint unit_size, zest_uint new_capacity);

// --Pocket_dynamic_array
#define zest__vec_header(T) ((zest_vec*)T - 1)
#define zest_vec_test(T, index) ZEST_ASSERT(index < zest__vec_header(T)->current_size)
zest_uint zest__grow_capacity(void *T, zest_uint size);
#define zest_vec_bump(T) zest__vec_header(T)->current_size++
#define zest_vec_clip(T) zest__vec_header(T)->current_size--
#define zest_vec_trim(T, amount) zest__vec_header(T)->current_size -= amount;
#define zest_vec_empty(T) (!T || zest__vec_header(T)->current_size == 0)
#define zest_vec_size(T) ((T) ? zest__vec_header(T)->current_size : 0)
#define zest_vec_last_index(T) ((T) ? zest__vec_header(T)->current_size - 1 : 0)
#define zest_vec_capacity(T) ((T) ? zest__vec_header(T)->capacity : 0)
#define zest_vec_size_in_bytes(T) (zest_vec_size(T) * sizeof(*T))
#define zest_vec_front(T) (T[0])
#define zest_vec_back(T) (T[zest__vec_header(T)->current_size - 1])
#define zest_vec_end(T) (&(T[zest_vec_size(T)]))
#define zest_vec_clear(T) if (T) zest__vec_header(T)->current_size = 0
#define zest_vec_pop(T) (zest__vec_header(T)->current_size--, T[zest__vec_header(T)->current_size])
#define zest_vec_set(T, index, value) ZEST_ASSERT((zest_uint)index < zest__vec_header(T)->current_size); T[index] = value;
#define zest_vec_foreach(index, T) for (int index = 0; index != zest_vec_size(T); ++index)
#define zest_vec_free(allocator, T) if (T) { ZEST__FREE(allocator, zest__vec_header(T)); T = ZEST_NULL; }

#ifdef __cplusplus
}
#endif

#ifdef __cplusplus
template<class T> T* zest__vec_reserve_wrapper(zloc_allocator *allocator, T *a, zest_uint unit_size, zest_uint new_capacity) {
	return (T *)zest__vec_reserve(allocator, a, unit_size, new_capacity);
}
ZEST_API template<class T> T* zest__vec_linear_reserve_wrapper(zloc_linear_allocator_t *allocator, T *a, zest_uint unit_size, zest_uint new_capacity) {
	return (T *)zest__vec_linear_reserve(allocator, a, unit_size, new_capacity);
}
//ZEST_API template<class T> T* zest__vec_linear_reserve(zloc_linear_allocator_t *allocator, T *a, zest_uint unit_size, zest_uint new_capacity);
ZEST_API void* zest__vec_linear_reserve(zloc_linear_allocator_t *allocator, void *T, zest_uint unit_size, zest_uint new_capacity);
#define zest_vec_grow(allocator, T) ((!(T) || (zest__vec_header(T)->current_size == zest__vec_header(T)->capacity)) ? (void)(T = zest__vec_reserve_wrapper(allocator, T, sizeof(*T), (T ? zest__grow_capacity(T, zest__vec_header(T)->current_size) : 8))) : (void)0)
#define zest_vec_linear_grow(allocator, T) ((!(T) || (zest__vec_header(T)->current_size == zest__vec_header(T)->capacity)) ? (void)(T = zest__vec_linear_reserve_wrapper(allocator, (T), sizeof(*T), (T ? zest__grow_capacity(T, zest__vec_header(T)->current_size) : 16))) : (void)0)
#define zest_vec_reserve(allocator, T, new_size) do { if (!T || zest__vec_header(T)->capacity < new_size) T = zest__vec_reserve_wrapper(allocator, T, sizeof(*T), new_size == 1 ? 8 : new_size); } while (0);
#define zest_vec_resize(allocator, T, new_size)  do { if (!T || zest__vec_header(T)->capacity < new_size) T = zest__vec_reserve_wrapper(allocator, T, sizeof(*T), new_size == 1 ? 8 : new_size); if (T) zest__vec_header(T)->current_size = new_size; } while (0);
#define zest_vec_linear_reserve(allocator, T, new_size) do { if (!T || zest__vec_header(T)->capacity < new_size) T = zest__vec_linear_reserve_wrapper(allocator, T, sizeof(*T), new_size == 1 ? 8 : new_size); } while (0)
#define zest_vec_linear_resize(allocator, T, new_size) do { if (!T || zest__vec_header(T)->capacity < new_size) T = zest__vec_linear_reserve_wrapper(allocator, T, sizeof(*T), new_size == 1 ? 8 : new_size); if (T) zest__vec_header(T)->current_size = new_size; } while (0)
#define ZEST__ARRAY(allocator, name, T, count) T *name = static_cast<T *>(ZEST__REALLOCATE(allocator, 0, sizeof(T) * count))
#else
#define zest_vec_grow(allocator, T) ((!(T) || (zest__vec_header(T)->current_size == zest__vec_header(T)->capacity)) ? T = zest__vec_reserve(allocator, (T), sizeof(*T), (T ? zest__grow_capacity(T, zest__vec_header(T)->current_size) : 8)) : 0)
#define zest_vec_linear_grow(allocator, T) ((!(T) || (zest__vec_header(T)->current_size == zest__vec_header(T)->capacity)) ? T = zest__vec_linear_reserve(allocator, (T), sizeof(*T), (T ? zest__grow_capacity(T, zest__vec_header(T)->current_size) : 16)) : 0)
#define zest_vec_reserve(allocator, T, new_size) if (!T || zest__vec_header(T)->capacity < new_size) T = zest__vec_reserve(allocator, T, sizeof(*T), new_size == 1 ? 8 : new_size);
#define zest_vec_resize(allocator, T, new_size) if (!T || zest__vec_header(T)->capacity < new_size) T = zest__vec_reserve(allocator, T, sizeof(*T), new_size == 1 ? 8 : new_size); zest__vec_header(T)->current_size = new_size
#define zest_vec_linear_reserve(allocator, T, new_size) if (!T || zest__vec_header(T)->capacity < new_size) T = zest__vec_linear_reserve(allocator, T, sizeof(*T), new_size == 1 ? 8 : new_size);
#define zest_vec_linear_resize(allocator, T, new_size) if (!T || zest__vec_header(T)->capacity < new_size) T = zest__vec_linear_reserve(allocator, T, sizeof(*T), new_size == 1 ? 8 : new_size); zest__vec_header(T)->current_size = new_size
#define ZEST__ARRAY(allocator, name, T, count) T *name = ZEST__REALLOCATE(allocator, 0, sizeof(T) * count)
#endif

#ifdef __cplusplus
extern "C" {
#endif

#define zest_vec_push(allocator, T, value) (zest_vec_grow(allocator, T), (T)[zest__vec_header(T)->current_size++] = value)
#define zest_vec_linear_push(allocator, T, value) (zest_vec_linear_grow(allocator, T), (T) ? (void)((T)[zest__vec_header(T)->current_size++] = value) : (void)0)
#define zest_vec_insert(allocator, T, location, value) do { ptrdiff_t offset = location - T; zest_vec_grow(allocator, T); if (offset < zest_vec_size(T)) memmove(T + offset + 1, T + offset, ((size_t)zest_vec_size(T) - offset) * sizeof(*T)); T[offset] = value; zest_vec_bump(T); } while (0)
#define zest_vec_linear_insert(allocator, T, location, value) do { ptrdiff_t offset = location - T; zest_vec_linear_grow(allocator, T); if (offset < zest_vec_size(T)) memmove(T + offset + 1, T + offset, ((size_t)zest_vec_size(T) - offset) * sizeof(*T)); T[offset] = value; zest_vec_bump(T); } while (0)
#define zest_vec_erase(T, location) do { ptrdiff_t offset = location - T; ZEST_ASSERT(T && offset >= 0 && location < zest_vec_end(T)); memmove(T + offset, T + offset + 1, ((size_t)zest_vec_size(T) - offset) * sizeof(*T)); zest_vec_clip(T); } while (0)
#define zest_vec_erase_range(T, it, it_last) do { ZEST_ASSERT(T && it >= T && it < zest_vec_end(T)); const ptrdiff_t count = it_last - it; const ptrdiff_t off = it - T; memmove(T + off, T + off + count, ((size_t)zest_vec_size(T) - (size_t)off - count) * sizeof(*T)); zest_vec_trim(T, (zest_uint)count); } while (0)
// --end of pocket dynamic array

// --Pocket_bucket_array
// The main purpose of this bucket array is to produce stable pointers for render graph resources
// Also used for resource stores.
typedef struct zest_bucket_array_t {
	void** buckets;             // A zest_vec of pointers to individual buckets
	zloc_allocator *allocator;
	zest_uint bucket_capacity;  // Number of elements each bucket can hold
	zest_uint current_size;       // Total number of elements across all buckets
	zest_uint element_size;     // The size of a single element
} zest_bucket_array_t;

ZEST_PRIVATE void zest__initialise_bucket_array(zloc_allocator *allocator, zest_bucket_array_t *array, zest_uint element_size, zest_uint bucket_capacity);
ZEST_PRIVATE void zest__free_bucket_array(zest_bucket_array_t *array);
ZEST_API_TMP void *zest__bucket_array_get(zest_bucket_array_t *array, zest_uint index);
ZEST_PRIVATE void *zest__bucket_array_add(zest_bucket_array_t *array);
ZEST_PRIVATE void *zest__bucket_array_linear_add(zloc_linear_allocator_t *allocator, zest_bucket_array_t *array);

#define zest_bucket_array_init(allocator, array, T, cap) zest__initialise_bucket_array(allocator, array, sizeof(T), cap)
#define zest_bucket_array_get(array, T, index) ((T *)zest__bucket_array_get(array, index))
#define zest_bucket_array_add(array, T) ((T *)zest__bucket_array_add(array))
#define zest_bucket_array_linear_add(allocator, array, T) ((T *)zest__bucket_array_linear_add(allocator, array))
#define zest_bucket_array_push(array, T, value) do { T* new_slot = zest_bucket_array_add(array, T); *new_slot = value; } while (0)
#define zest_bucket_array_linear_push(allocator, array, T, value) do { T* new_slot = zest_bucket_array_linear_add(allocator, array, T); *new_slot = value; } while (0)
#define zest_bucket_array_size(array) ((array)->current_size)
#define zest_bucket_array_foreach(index, array) for (int index = 0; index != array.current_size; ++index)
// --end of pocket bucket array

// --Generic container used to store resources that use int handles
typedef struct zest_resource_store_t {
	int magic;
	zest_bucket_array_t data;
	zest_struct_type struct_type;
	zest_uint alignment;
	zest_uint *generations;
	zest_uint *free_slots;
	zest_u64 *initialised;
	void *origin;
	zest_sync_t sync;	//Todo: use atomics instead
} zest_resource_store_t;

ZEST_PRIVATE void zest__free_store(zest_resource_store_t *store);
ZEST_PRIVATE void zest__clear_store(zest_resource_store_t *store);
ZEST_PRIVATE zest_uint zest__size_in_bytes_store(zest_resource_store_t *store);
ZEST_PRIVATE zest_handle zest__add_store_resource(zest_resource_store_t *store);
ZEST_PRIVATE void zest__remove_store_resource(zest_resource_store_t *store, zest_handle handle);
ZEST_PRIVATE void zest__initialise_store(zloc_allocator *allocator, void *origin, zest_resource_store_t *store, zest_uint struct_size, zest_struct_type struct_type);
ZEST_PRIVATE void zest__activate_resource(zest_resource_store_t *store, zest_handle handle);
ZEST_API int zest_GetDeviceResourceCount(zest_device device, zest_device_handle_type type);


// --Pocket_Hasher, converted to c from Stephen Brumme's XXHash code (https://github.com/stbrumme/xxhash) by Peter Rigby
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

typedef struct zest_hasher_t {
	zest_ull state[4];
	unsigned char buffer[zest__HASH_MAX_BUFFER_SIZE];
	zest_ull buffer_size;
	zest_ull total_length;
} zest_hasher_t;

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

ZEST_PRIVATE inline zest_bool zest__hasher_add(zest_hasher_t *hasher, const void* input, zest_ull length)
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

ZEST_PRIVATE inline zest_ull zest__get_hash(zest_hasher_t *hasher)
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
void zest__hash_initialise(zest_hasher_t *hasher, zest_ull seed);

//The only command you need for the hasher. Just used internally by the hash map.
zest_key zest_Hash(const void* input, zest_ull length, zest_ull seed);
//-- End of Pocket Hasher

// --Begin Pocket_hash_map
//  For internal use only
typedef struct {
	zest_key key;
	zest_uint index;
} zest_hash_pair;

#ifndef ZEST_HASH_SEED
#define ZEST_HASH_SEED 0xABCDEF99
#endif
ZEST_PRIVATE zest_hash_pair* zest__lower_bound(zest_hash_pair *map, zest_key key) { zest_hash_pair *first = map; zest_hash_pair *last = map ? zest_vec_end(map) : 0; size_t count = (size_t)(last - first); while (count > 0) { size_t count2 = count >> 1; zest_hash_pair* mid = first + count2; if (mid->key < key) { first = ++mid; count -= count2 + 1; } else { count = count2; } } return first; }
ZEST_PRIVATE void zest__map_realign_indexes(zest_hash_pair *map, zest_uint index) { zest_vec_foreach(i, map) { if (map[i].index < index) continue; map[i].index--; } }
ZEST_PRIVATE zest_uint zest__map_get_index(zest_hash_pair *map, zest_key key) { zest_hash_pair *it = zest__lower_bound(map, key); return (it == zest_vec_end(map) || it->key != key) ? -1 : it->index; }
#define zest_map_hash(hash_map, name) zest_Hash(name, strlen(name), ZEST_HASH_SEED)
#define zest_map_hash_ptr(ptr, size) zest_Hash(ptr, size, ZEST_HASH_SEED)
#define zest_hash_map(T) typedef struct { zest_hash_pair *map; T *data; zest_uint *free_slots; zest_uint last_index; }
#define zest_map_valid_name(hash_map, name) (hash_map.map && zest__map_get_index(hash_map.map, zest_map_hash(hash_map, name)) != -1)
#define zest_map_valid_key(hash_map, key) (hash_map.map && zest__map_get_index(hash_map.map, key) != -1)
#define zest_map_valid_index(hash_map, index) (hash_map.map && (zest_uint)index < zest_vec_size(hash_map.data))
#define zest_map_valid_hash(hash_map, ptr, size) (zest__map_get_index(hash_map.map, zest_map_hash_ptr(hash_map, ptr, size)) != -1)
#define zest_map_set_index(allocator, hash_map, hash_key, value) zest_hash_pair *it = zest__lower_bound(hash_map.map, hash_key); if (!hash_map.map || it == zest_vec_end(hash_map.map) || it->key != hash_key) { if (zest_vec_size(hash_map.free_slots)) { hash_map.last_index = zest_vec_pop(hash_map.free_slots); hash_map.data[hash_map.last_index] = value; } else { hash_map.last_index = zest_vec_size(hash_map.data); zest_vec_push(allocator, hash_map.data, value); } zest_hash_pair new_pair; new_pair.key = hash_key; new_pair.index = hash_map.last_index; zest_vec_insert(allocator, hash_map.map, it, new_pair); } else { hash_map.data[it->index] = value; }
#define zest_map_set_linear_index(allocator, hash_map, hash_key, value) zest_hash_pair *it = zest__lower_bound(hash_map.map, hash_key); if (!hash_map.map || it == zest_vec_end(hash_map.map) || it->key != hash_key) { if (zest_vec_size(hash_map.free_slots)) { hash_map.last_index = zest_vec_pop(hash_map.free_slots); hash_map.data[hash_map.last_index] = value; } else { hash_map.last_index = zest_vec_size(hash_map.data); zest_vec_linear_push(allocator, hash_map.data, value); } zest_hash_pair new_pair; new_pair.key = hash_key; new_pair.index = hash_map.last_index; zest_vec_linear_insert(allocator, hash_map.map, it, new_pair); } else { hash_map.data[it->index] = value; }
#define zest_map_insert(allocator, hash_map, name, value) { zest_key key = zest_Hash(name, strlen(name), ZEST_HASH_SEED); zest_map_set_index(allocator, hash_map, key, value); }
#define zest_map_linear_insert(allocator, hash_map, name, value) { zest_key key = zest_Hash(name, strlen(name), ZEST_HASH_SEED); zest_map_set_linear_index(allocator, hash_map, key, value); }
#define zest_map_insert_key(allocator, hash_map, hash_key, value) { zest_map_set_index(allocator, hash_map, hash_key, value) }
#define zest_map_insert_linear_key(allocator, hash_map, hash_key, value) { zest_map_set_linear_index(allocator, hash_map, hash_key, value) }
#define zest_map_insert_with_ptr_hash(allocator, hash_map, ptr, size, value) { zest_key key = zest_map_hash_ptr(hash_map, ptr, size); zest_map_set_index(allocator, hash_map, key, value) }
#define zest_map_at(hash_map, name) &hash_map.data[zest__map_get_index(hash_map.map, zest_map_hash(hash_map, name))]
#define zest_map_at_key(hash_map, key) &hash_map.data[zest__map_get_index(hash_map.map, key)]
#define zest_map_at_index(hash_map, index) &hash_map.data[index]
#define zest_map_get_index_by_key(hash_map, key) zest__map_get_index(hash_map.map, key);
#define zest_map_get_index_by_name(hash_map, name) zest__map_get_index(hash_map.map, zest_map_hash(hash_map, name));
#define zest_map_remove(allocator, hash_map, name) { zest_key key = zest_map_hash(hash_map, name); zest_hash_pair *it = zest__lower_bound(hash_map.map, key); zest_uint index = it->index; zest_vec_erase(hash_map.map, it); zest_vec_push(allocator->device->allocator, hash_map.free_slots, index); }
#define zest_map_remove_key(allocator, hash_map, name) { zest_hash_pair *it = zest__lower_bound(hash_map.map, key); zest_uint index = it->index; zest_vec_erase(hash_map.map, it); zest_vec_push(allocator->device->allocator, hash_map.free_slots, index); }
#define zest_map_last_index(hash_map) (hash_map.last_index)
#define zest_map_size(hash_map) (hash_map.map ? zest__vec_header(hash_map.data)->current_size : 0)
#define zest_map_clear(hash_map) zest_vec_clear(hash_map.map); zest_vec_clear(hash_map.data); zest_vec_clear(hash_map.free_slots)
#define zest_map_free(allocator, hash_map) if (hash_map.map) ZEST__FREE(allocator, zest__vec_header(hash_map.map)); if (hash_map.data) ZEST__FREE(allocator, zest__vec_header(hash_map.data)); if (hash_map.free_slots) ZEST__FREE(allocator, zest__vec_header(hash_map.free_slots)); hash_map.data = 0; hash_map.map = 0; hash_map.free_slots = 0
//Use inside a for loop to iterate over the map. The loop index should be be the index into the map array, to get the index into the data array.
#define zest_map_index(hash_map, i) (hash_map.map[i].index)
#define zest_map_foreach(index, hash_map) zest_vec_foreach(index, hash_map.map)
// --End pocket hash map

//Another hash map with open addressing probing. This is very simple and designed for frame graphs only but
//but the frame graph only has to hash a few things and there's negligable difference between this and the above
//hash map when the numbers are small so therefore this is currently unused. The above hash map uses binary 
//search and has the advantage of not needing the data table to be larger to avoid collisions and there's not 
//much overhead if the table runs out of space and needs to be resized. 

//If the below hash map runs out of space then everything needs to be re-hashed again so it needs to be oversized 
//to begin with. Because it's for the frame graph only there's no need to be able to remove things from the table
//but it wouldn't be too hard to add I think.
typedef struct zest_map_t {
	void *table;				//Table containing all the data
	zest_uint *filled_slots;	//A parallel table so that elements can be iterated over easily
	zest_uint element_size;		//The size of each data element stored in the table
	zest_uint current_size;		//The current number of used slots in the table
	zest_uint capacity;			//The total capacity of the table
	zest_uint collisions;		//Keep a count of all the collisions that happened
} zest_map_t;

void zest__initialise_map(zest_context context, zest_map_t *map, zest_uint element_size, zest_uint capacity);
zest_bool zest__insert_key(zest_map_t *map, zest_key key, void *value);
zest_bool zest__insert(zest_map_t *map, const char *name, void *value);
void *zest__at_key(zest_map_t *map, zest_key key);
void *zest__at_index(zest_map_t *map, zest_uint index);
void *zest__at(zest_map_t *map, const char *name);
zest_bool zest__find_key(zest_map_t *map, zest_key key, void **out_value);
zest_bool zest__find(zest_map_t *map, const char *name, void **out_value);
zest_bool zest__ensure_capacity(zest_map_t *map, zest_uint required_capacity);
void zest__free_map(zest_context context, zest_map_t *map);

// --Begin Pocket_text_buffer
typedef struct zest_text_t {
	char *str;
} zest_text_t;

ZEST_API void zest_SetText(zloc_allocator *allocator, zest_text_t *buffer, const char *text);
ZEST_API void zest_ResizeText(zloc_allocator *allocator, zest_text_t *buffer, zest_uint size);
ZEST_API void zest_SetTextf(zloc_allocator *allocator, zest_text_t *buffer, const char *text, ...);
ZEST_API void zest_SetTextfv(zloc_allocator *allocator, zest_text_t *buffer, const char *text, va_list args);
ZEST_API void zest_AppendTextf(zloc_allocator *allocator, zest_text_t *buffer, const char *format, ...);
ZEST_API void zest_FreeText(zloc_allocator *allocator, zest_text_t *buffer);
ZEST_API zest_uint zest_TextSize(zest_text_t *buffer);      //Will include a null terminator
ZEST_API zest_uint zest_TextLength(zest_text_t *buffer);    //Uses strlen to get the length
// --End pocket text buffer

#ifndef ZEST_LOG_ENTRY_SIZE
#define ZEST_LOG_ENTRY_SIZE 256
#endif
typedef struct zest_log_entry_t {
	char str[ZEST_LOG_ENTRY_SIZE];
	zest_microsecs time;
} zest_log_entry_t;

ZEST_PRIVATE void zest__log_entry(zest_context context, const char *entry, ...);
ZEST_PRIVATE void zest__log_entry_v(char *str, const char *entry, ...);
ZEST_PRIVATE void zest__reset_frame_log(zest_context context, char *str, const char *entry, ...);
ZEST_PRIVATE void zest__log_validation_error(zest_device device, const char *message);

ZEST_API void zest__add_report(zest_device device, zest_report_category category, int line_number, const char *file_name, const char *function_name, const char *entry, ...);

#ifdef ZEST_DEBUGGING
#define ZEST_FRAME_LOG(context, message_f, ...) zest__log_entry(context, message_f, ##__VA_ARGS__)
#define ZEST_RESET_LOG() zest__reset_log()
#define ZEST_MAYBE_REPORT(device, condition, category, entry, ...) if (condition) { zest__add_report(device, category, __LINE__, __FILE__, __FUNCTION__, entry, ##__VA_ARGS__); }
#define ZEST_REPORT(device, category, entry, ...) zest__add_report(device, category, __LINE__, __FILE__, __FUNCTION__, entry, ##__VA_ARGS__)
#else
#define ZEST_FRAME_LOG(message_f, ...)
#define ZEST_RESET_LOG()
#define ZEST_MAYBE_REPORT() 
#define ZEST_REPORT()
#endif

// --Structs
// Vectors
// --Matrix and vector structs
typedef struct zest_vec2 {
	float x, y;
} zest_vec2;

typedef struct zest_ivec2 {
	int x, y;
} zest_ivec2;

typedef struct zest_vec3 {
	float x, y, z;
} zest_vec3;

typedef struct zest_ivec3 {
	int x, y, z;
} zest_ivec3;

typedef struct zest_bounding_box_t {
	zest_vec3 min_bounds;
	zest_vec3 max_bounds;
} zest_bounding_box_t;

typedef struct zest_extent2d_t {
	zest_uint width;
	zest_uint height;
} zest_extent2d_t;

typedef struct zest_extent3d_t {
	zest_uint width;
	zest_uint height;
	zest_uint depth;
} zest_extent3d_t;

typedef struct zest_scissor_rect_t {
	struct {
		int x;
		int y;
	} offset;
	struct {
		zest_uint width;
		zest_uint height;
	} extent;
} zest_scissor_rect_t;

typedef struct zest_viewport_t {
	float x;
	float y;
	float width;
	float height;
	float minDepth;
	float maxDepth;
} zest_viewport_t;

//Note that using alignas on windows causes a crash in release mode relating to the allocator.
//Not sure why though. We need the align as on Mac otherwise metal complains about the alignment
//in the shaders
typedef struct zest_vec4 {
	union {
		struct { float x, y, z, w; };
		struct { float r, g, b, a; };
	};
} zest_vec4 ZEST_ALIGN_AFFIX(16);

typedef union zest_clear_value_t {
	zest_vec4 color;
	struct {
		float depth;
		zest_uint stencil;
	} depth_stencil;
} zest_clear_value_t;

typedef struct zest_matrix4 {
	zest_vec4 v[4];
} zest_matrix4 ZEST_ALIGN_AFFIX(16);

typedef struct zest_plane
{
	// unit vector
	zest_vec3 normal;

	// distance from origin to the nearest point in the plane
	float distance;
} zest_plane;

typedef struct zest_frustum
{
	zest_plane top_face;
	zest_plane bottom_face;

	zest_plane right_face;
	zest_plane left_face;

	zest_plane far_face;
	zest_plane near_face;
} zest_frustum;

typedef struct zest_rgba8_t {
	union {
		struct { unsigned char r, g, b, a; };
		struct { zest_uint color; };
	};
} zest_rgba8_t;
typedef zest_rgba8_t zest_color_t;

typedef struct zest_rgba32 {
	float r, g, b, a;
} zest_rgba32;

typedef struct zest_memory_stats_t {
	zest_uint device_allocations;
	zest_uint renderer_allocations;
	zest_uint filter_count;
} zest_memory_stats_t;

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
    double time_passed;
    double seconds_passed;
    double max_elapsed_time;
    int update_count;
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

typedef struct zest_debug_t {
	zest_uint function_depth;
	zest_log_entry_t *frame_log;
} zest_debug_t;

typedef struct zest_buffer_info_t {
	zest_image_usage_flags image_usage_flags;
	zest_buffer_usage_flags buffer_usage_flags;                    //The usage state_flags of the memory block.
	zest_memory_property_flags property_flags;
	zest_uint memory_type_bits;
	zest_size alignment;
	zest_uint frame_in_flight;
	zest_memory_pool_flags flags;
} zest_buffer_info_t;

typedef struct zest_buffer_pool_size_t {
	const char *name;
	zest_size pool_size;
	zest_size minimum_allocation_size;
} zest_buffer_pool_size_t;

typedef struct zest_buffer_usage_t {
	zest_memory_property_flags property_flags;
	zest_memory_pool_type memory_pool_type;
	zest_uint memory_type_index;
	zest_size alignment;
} zest_buffer_usage_t;

typedef struct zest_buffer_allocator_key_t {
	zest_buffer_usage_t usage;
	zest_uint frame_in_flight;
} zest_buffer_allocator_key_t;

zest_hash_map(zest_buffer_pool_size_t) zest_map_buffer_pool_sizes;

typedef void* zest_pool_range;

typedef struct zest_buffer_details_t {
	zest_size size;
	zest_size memory_offset;
	zest_device_memory_pool memory_pool;
	zest_buffer_allocator buffer_allocator;
	zest_size memory_in_use;
} zest_buffer_details_t;

typedef struct zest_memory_requirements_t {
	zest_size alignment;
	zest_size size;
} zest_memory_requirements_t;

typedef struct zest_mip_index_collection {
	zest_uint binding_numbers;
	zest_uint *mip_indexes[zest_max_global_binding_number];
} zest_mip_index_collection;

zest_hash_map(zest_mip_index_collection) zest_map_mip_indexes;

typedef struct zest_buffer_copy_t {
	zest_size src_offset;
	zest_size dst_offset;
	zest_size size;
} zest_buffer_copy_t;

//Simple stuct for uploading buffers from the staging buffer to the device local buffers
typedef struct zest_buffer_uploader_t {
	zest_buffer_upload_flags flags;
	zest_buffer source_buffer;           //The source memory allocation (cpu visible staging buffer)
	zest_buffer target_buffer;           //The target memory allocation that we're uploading to (device local buffer)
	zest_buffer_copy_t *buffer_copies;   //List of copy info commands to upload staging buffers to the gpu each frame
} zest_buffer_uploader_t;

// --End Buffer Management

typedef struct zest_image_view_create_info_t {
	zest_image_view_type view_type;
	zest_uint base_mip_level;
	zest_uint level_count;
	zest_uint base_array_layer;
	zest_uint layer_count;
} zest_image_view_create_info_t;

typedef struct zest_atlas_region_t {
	zest_uint top, left;         //the top and left pixel coordinates of the region in the layer
	zest_uint width;
	zest_uint height;
	zest_vec4 uv;                //UV coords 
	zest_u64 uv_packed;          //UV coords packed into 16bit floats
	zest_uint layer_index;      //the layer index of the image when it's packed into an image/texture array
	zest_uint frames;            //Will be one if this is a single image or more if it's part of an animation
	zest_binding_number_type binding_number;
	zest_uint atlas_index;		 //The index with an image collection where the region might be stored
	zest_uint image_index;       //Index of the image to lookup in the shader
	zest_uint sampler_index;     //Index of the sampler to lookup in the shader
	zest_vec2 handle;
	zest_vec2 min;
	zest_vec2 max;
} zest_atlas_region_t;

typedef struct zest_image_info_t {
	zest_extent3d_t extent;
	zest_uint mip_levels;
	zest_uint layer_count;
	zest_format format;
	zest_image_aspect_flags aspect_flags;
	zest_sample_count_flags sample_count;
	zest_image_flags flags;
	zest_image_layout layout;
} zest_image_info_t;

typedef struct zest_sampler_info_t {
	zest_filter_type mag_filter;
	zest_filter_type min_filter;
	zest_mipmap_mode mipmap_mode;
	zest_sampler_address_mode address_mode_u;
	zest_sampler_address_mode address_mode_v;
	zest_sampler_address_mode address_mode_w;
	zest_border_color border_color;
	float mip_lod_bias;
	zest_bool anisotropy_enable;
	float max_anisotropy;
	zest_bool compare_enable;
	zest_compare_op compare_op;
	float min_lod;
	float max_lod;
} zest_sampler_info_t;

struct zest_window_data_t;  // Forward declaration
typedef void (*zest_get_window_sizes_callback)(struct zest_window_data_t* window_data, int* fb_width, int* fb_height, int* window_width, int* window_height);
typedef void (*zest_create_window_surface_callback)(zest_context context);

typedef struct zest_window_data_t {
	/**
* The main window handle.
* - On Windows: A pointer to the HWND.
* - On Linux (X11): The xcb_window_t or xlib Window.
* - On macOS: A pointer to the CAMetalLayer.
*/
	void *native_handle;

	/*
* Used for a 3rd party lib handle like GLFW or SDL
*/
	void *window_handle;

	/**
* The display or connection handle (can be NULL on some platforms).
* - On Windows: A pointer to the HINSTANCE.
- On Linux (X11): A pointer to the xcb_connection_t or Display*.
* - On macOS: Not used (can be NULL).
*/
	void *display;
	void *user_data;
	int width;
	int height;
	zest_get_window_sizes_callback window_sizes_callback;
	zest_create_window_surface_callback create_surface_callback;
} zest_window_data_t;

typedef struct zest_resource_usage_t {
	zest_resource_node resource_node;
	zest_pipeline_stage_flags stage_mask;   // Pipeline stages this usage pertains to
	zest_image_layout image_layout;         // Required image layout if it's an image
	zest_image_aspect_flags aspect_flags;
	zest_resource_purpose purpose;			
	zest_access_flags access_mask;          // access mask for barriers
	// For framebuffer attachments
	zest_load_op load_op;
	zest_store_op store_op;
	zest_load_op stencil_load_op;
	zest_store_op stencil_store_op;
	zest_clear_value_t clear_value;
	zest_bool is_output;
} zest_resource_usage_t;

typedef struct zest_ssbo_binding_t {
	int magic;
	const char *name;
	zest_uint max_buffers;
	zest_supported_shader_stages shader_stages;
	zest_uint binding_number;
} zest_ssbo_binding_t;

typedef struct zest_device_builder_t {
	int magic;
	zloc_allocator *allocator;
	// Flag to enable validation layers, etc.
	zest_device_init_flags flags;
	zest_platform_type platform;
	zest_size memory_pool_size;
	int thread_count;
	zest_uint bindless_sampler_count;
	zest_uint bindless_texture_2d_count;
	zest_uint bindless_texture_cube_count;
	zest_uint bindless_texture_array_count;
	zest_uint bindless_texture_3d_count;
	zest_uint bindless_storage_buffer_count;
	zest_uint bindless_storage_image_count;
	zest_uint bindless_uniform_buffer_count;
	int graphics_queue_count;	//For testing only
	int transfer_queue_count;	//For testing only
	int compute_queue_count;	//For testing only
	zest_size max_small_buffer_size;
	const char *log_path;                               //path to the log to store log and validation messages
	const char *cached_shader_path;

	// User-provided list of required instance extensions (e.g., from GLFW)
	const char** required_instance_extensions;
} zest_device_builder_t;

typedef struct zest_create_context_info_t {
	const char *title;                                  //Title that shows in the window
	zest_size frame_graph_allocator_size;               //The size of the linear allocator used by render graphs to store temporary data
	int screen_width, screen_height;                    //Default width and height of the window that you open
	int screen_x, screen_y;                             //Default position of the window
	int virtual_width, virtual_height;                  //The virtial width/height of the viewport
	zest_millisecs semaphore_wait_timeout_ms;           //The amount of time the main loop fence should wait before timing out
	zest_millisecs max_semaphore_timeout_ms;            //The maximum amount of time to wait before giving up
	zest_format color_format;                   		//The format to use for the swapchain
	zest_context_init_flags flags;                              //Set flags to apply different initialisation options
	zest_platform_type platform;
	zest_size memory_pool_size;
} zest_create_context_info_t;

zest_hash_map(zest_context_queue) zest_map_queue_value;

typedef struct zest_platform_memory_info_t {
	zest_millisecs timestamp;
	zest_uint context_info;
} zest_platform_memory_info_t;

zest_hash_map(const char *) zest_map_queue_names;
zest_hash_map(zest_text_t) zest_map_validation_errors;

typedef struct zest_binding_index_for_release_t {
	zest_set_layout layout;
	zest_uint binding_index;
	zest_uint binding_number;
} zest_binding_index_for_release_t;

typedef struct zest_device_destruction_queue_t {
	void **resources[ZEST_MAX_FIF];
	zest_buffer *buffers[ZEST_MAX_FIF];
} zest_device_destruction_queue_t;

typedef struct zest_context_destruction_queue_t {
	void **resources[ZEST_MAX_FIF];
	zest_image_t *transient_images[ZEST_MAX_FIF];
	zest_image_view_array_t *transient_view_arrays[ZEST_MAX_FIF];
	zest_binding_index_for_release_t *transient_binding_indexes[ZEST_MAX_FIF];
} zest_context_destruction_queue_t;

typedef struct zest_report_t {
	zest_text_t message;
	zest_report_category category;
	int count;
	const char *file_name;
	const char *function_name;
	int line_number;
} zest_report_t;

typedef struct zest_cached_frame_graph_t {
	void *memory;
	zest_frame_graph frame_graph;
} zest_cached_frame_graph_t;

zest_hash_map(zest_report_t) zest_map_reports;
zest_hash_map(zest_buffer_allocator) zest_map_buffer_allocators;
zest_hash_map(zest_cached_frame_graph_t) zest_map_cached_frame_graphs;
zest_hash_map(zest_frame_graph_semaphores) zest_map_frame_graph_semaphores;
zest_hash_map(zest_pipeline) zest_map_cached_pipelines;

typedef struct zest_builtin_shaders_t {
	zest_shader_handle swap_vert;
	zest_shader_handle swap_frag;
} zest_builtin_shaders_t;

typedef struct zest_builtin_pipeline_templates_t {
	zest_pipeline_template swap;
} zest_builtin_pipeline_templates_t;

typedef struct zest_descriptor_binding_desc_t {
	zest_uint binding;                      // The binding slot (register in HLSL, binding in GLSL, [[id(n)]] in MSL)
	zest_descriptor_type type;              // The generic resource type
	zest_uint count;                        // Number of descriptors. 
	zest_supported_shader_stages stages;    // Which shader stages can access this 
} zest_descriptor_binding_desc_t;

typedef struct zest_set_layout_builder_t {
	zloc_allocator *allocator;
	zest_descriptor_binding_desc_t *bindings;
	zest_u64 binding_capacity;
	zest_set_layout_builder_flags flags;
} zest_set_layout_builder_t;

typedef int (*zest_wait_timeout_callback)(zest_millisecs total_wait_time_ms, zest_uint retry_count, void *user_data);

typedef struct zest_mip_push_constants_t {
	zest_vec4 settings;
	zest_vec2 source_resolution;            //the current size of the window/resolution
	zest_vec2 padding;
} zest_mip_push_constants_t;

typedef struct zest_gpu_timestamp_s {
	zest_u64 start;
	zest_u64 end;
} zest_gpu_timestamp_t;

typedef struct zest_timestamp_duration_s {
	double nanoseconds;
	double microseconds;
	double milliseconds;
} zest_timestamp_duration_t;

//frame_graph_types

typedef void (*zest_fg_execution_callback)(const zest_command_list command_list, void *user_data);
typedef void* zest_resource_handle;

typedef struct zest_buffer_description_t {
	zest_size size;
	zest_buffer_info_t buffer_info;
} zest_buffer_description_t;

typedef struct zest_pass_adjacency_list_t {
	int *pass_indices;
} zest_pass_adjacency_list_t;

typedef struct zest_execution_wave_t {
	zest_uint level;
	zest_uint queue_bits;
	int *pass_indices;
} zest_execution_wave_t;

typedef struct zest_batch_key {
	zest_u64 next_pass_indexes;
	zest_uint current_family_index;
	zest_u64 next_family_indexes;
} zest_batch_key;

zest_hash_map(zest_resource_usage_t) zest_map_resource_usages;

typedef struct zest_pass_execution_callback_t {
	zest_fg_execution_callback callback;
	void *user_data;
} zest_pass_execution_callback_t;

typedef struct zest_resource_state_t {
	zest_uint pass_index;
	zest_resource_usage_t usage;
	zest_uint queue_family_index;
	zest_uint submission_id;
	zest_bool was_released;
} zest_resource_state_t;

typedef struct zest_pass_queue_info_t {
	zest_context_queue queue;
	zest_uint queue_family_index;
	zest_pipeline_stage_flags timeline_wait_stage;
	zest_device_queue_type queue_type;
}zest_pass_queue_info_t;

#ifdef ZEST_DEBUGGING
typedef struct zest_image_barrier_t {
	zest_access_flags src_access_mask;
	zest_pipeline_stage_flags src_stage;
	zest_access_flags dst_access_mask;
	zest_pipeline_stage_flags dst_stage;
	zest_image_layout old_layout;
	zest_image_layout new_layout;
	zest_uint src_queue_family_index;
	zest_uint dst_queue_family_index;
} zest_image_barrier_t;

typedef struct zest_buffer_barrier_t {
	zest_access_flags src_access_mask;
	zest_pipeline_stage_flags src_stage;
	zest_access_flags dst_access_mask;
	zest_pipeline_stage_flags dst_stage;
	zest_uint src_queue_family_index;
	zest_uint dst_queue_family_index;
} zest_buffer_barrier_t;
#endif

typedef struct zest_image_resource_info_t {
	zest_format format;
	zest_resource_usage_hint usage_hints;
	zest_uint width;
	zest_uint height;
	zest_uint mip_levels;
	zest_uint layer_count;
} zest_image_resource_info_t;

typedef struct zest_buffer_resource_info_t {
	zest_resource_usage_hint usage_hints;
	zest_size size;
} zest_buffer_resource_info_t;

zest_hash_map(zest_uint) attachment_idx;

typedef struct zest_rendering_attachment_info_t {
	zest_image_view *image_view;
	zest_image_layout layout;
	zest_image_view *resolve_image_view;
	zest_image_layout resolve_layout;
	zest_load_op load_op;
	zest_store_op store_op;
	zest_clear_value_t clear_value;
	zest_bool is_swapchain_image;
} zest_rendering_attachment_info_t;

typedef struct zest_rendering_info_t {
	zest_uint color_attachment_count;
	zest_format color_attachment_formats[ZEST_MAX_ATTACHMENTS];
	zest_format depth_attachment_format;
	zest_format stencil_attachment_format;
	zest_sample_count_flags sample_count;
	zest_uint view_mask;
} zest_rendering_info_t;

typedef zest_buffer(*zest_resource_buffer_provider)(zest_context context, zest_resource_node resource);
typedef zest_image_view(*zest_resource_image_provider)(zest_context context, zest_resource_node resource);

typedef struct zest_semaphore_reference_t {
	zest_dynamic_resource_type type;
	zest_u64 semaphore;
} zest_semaphore_reference_t;

typedef struct zest_resource_versions_t {
	zest_resource_node *resources;
}zest_resource_versions_t;

typedef struct zest_frame_graph_auto_state_t {
	zest_uint render_width;
	zest_uint render_height;
	zest_format render_format;
	zest_bool vsync;
} zest_frame_graph_auto_state_t;

typedef struct zest_frame_graph_cache_key_t {
	zest_frame_graph_auto_state_t auto_state;
	const void *user_state;
	zest_size user_state_size;
} zest_frame_graph_cache_key_t;

typedef struct zest_frame_graph_builder_t {
	zest_context context;
	zest_frame_graph frame_graph;
	zest_pass_node current_pass;
}zest_frame_graph_builder_t;

typedef struct zest_descriptor_indices_t {
	zest_uint *free_indices;
	zest_size *is_free;
	volatile zest_uint next_new_index;
	zest_uint capacity;
	zest_descriptor_type descriptor_type;
} zest_descriptor_indices_t;

typedef struct zest_layout_type_counts_t {
	zest_uint sampler_count;
	zest_uint sampler_image_count;
	zest_uint uniform_buffer_count;
	zest_uint storage_buffer_count;
	zest_uint combined_image_sampler_count;
} zest_layout_type_counts_t;

typedef struct zest_uniform_buffer_data_t {
	zest_matrix4 view;
	zest_matrix4 proj;
	zest_vec4 parameters1;
	zest_vec4 parameters2;
	zest_vec2 screen_size;
	zest_uint millisecs;
} zest_uniform_buffer_data_t;

typedef struct zest_vertex_attribute_desc_t {
	zest_uint location;
	zest_uint binding;
	zest_format format;
	zest_uint offset;
} zest_vertex_attribute_desc_t;

typedef struct zest_vertex_binding_desc_t {
	zest_uint binding;
	zest_uint stride;
	zest_input_rate input_rate;
} zest_vertex_binding_desc_t;

typedef struct zest_vertex_input_desc_t {
	zest_uint attribute_count;
	zest_vertex_attribute_desc_t *p_attributes;
	zest_uint binding_count;
	zest_vertex_binding_desc_t *p_bindings;
} zest_vertex_input_desc_t;

typedef struct zest_rasterization_state_t {
	zest_polygon_mode polygon_mode;
	zest_cull_mode cull_mode;
	zest_front_face front_face;
	float line_width;
	zest_bool depth_clamp_enable;
	zest_bool rasterizer_discard_enable;
	zest_bool depth_bias_enable;
	float depth_bias_constant_factor;
	float depth_bias_clamp;
	float depth_bias_slope_factor;
} zest_rasterization_state_t;

typedef struct zest_depth_stencil_state_t {
	zest_bool depth_test_enable;
	zest_bool depth_write_enable;
	zest_bool depth_bounds_test_enable;
	zest_bool stencil_test_enable;
	zest_compare_op depth_compare_op;
} zest_depth_stencil_state_t;

typedef struct zest_color_blend_attachment_t {
	zest_bool blend_enable;
	zest_blend_factor src_color_blend_factor;
	zest_blend_factor dst_color_blend_factor;
	zest_blend_op color_blend_op;
	zest_blend_factor src_alpha_blend_factor;
	zest_blend_factor dst_alpha_blend_factor;
	zest_blend_op alpha_blend_op;
	zest_color_component_flags color_write_mask;
} zest_color_blend_attachment_t;

typedef struct zest_push_constant_range_t {
	zest_supported_shader_stages stage_flags;
	zest_uint offset;
	zest_uint size;
} zest_push_constant_range_t;

//Pipeline template is used with CreatePipeline to create a graphics pipeline. Use PipelineTemplate() or SetPipelineTemplate with PipelineTemplateCreateInfo to create a PipelineTemplate
typedef struct zest_pipeline_template_t {
	int magic;
	zest_device device;
	const char *name;                                                            //Name for the pipeline just for labelling it when listing all the renderer objects in debug
	zest_topology primitive_topology;
	zest_rasterization_state_t rasterization;
	zest_depth_stencil_state_t depth_stencil;
	zest_pipeline_set_flags flags;                                               //Flag bits
	zest_uint uniforms;                                                          //Number of uniform buffers in the pipeline, usually 1 or 0
    
	zest_uint view_mask;
	zest_vertex_attribute_desc_t *attribute_descriptions;
	zest_vertex_binding_desc_t *binding_descriptions;
	zest_bool no_vertex_input;
	const char *vertShaderFunctionName;
	const char *fragShaderFunctionName;
	zest_shader_handle vertex_shader;
	zest_shader_handle fragment_shader;

	zest_pipeline_layout layout;

	zest_color_blend_attachment_t color_blend_attachment;
} zest_pipeline_template_t;

typedef struct zest_pipeline_layout_info_t {
	zest_device device;
	zest_set_layout *set_layouts;
	zest_push_constant_range_t push_constant_range;
} zest_pipeline_layout_info_t;

typedef struct zest_buffer_image_copy_t {
	zest_size buffer_offset;
	zest_uint buffer_row_length;
	zest_uint buffer_image_height;
	zest_image_aspect_flags image_aspect;
	zest_uint mip_level;
	zest_uint base_array_layer;
	zest_uint layer_count;
	zest_ivec3 image_offset;
	zest_extent3d_t image_extent;
} zest_buffer_image_copy_t;

//We just have a copy of the ImGui Draw vert here so that we can setup things for imgui
//should anyone choose to use it
typedef struct zest_ImDrawVert_t {
	zest_vec2 pos;
	zest_vec2 uv;
	zest_uint col;
} zest_ImDrawVert_t;

typedef struct zest_layer_buffers_t {
	union {
		zest_buffer staging_vertex_data;
		zest_buffer staging_instance_data;
	};
	zest_buffer staging_index_data;
	zest_buffer device_vertex_data;

	union {
		struct { void *instance_ptr; };
		struct { void *vertex_ptr; };
	};

	zest_uint *index_ptr;

	zest_uint instance_count;
	zest_uint index_count;
	zest_uint index_position;
	zest_uint last_index;
	zest_uint vertex_count;
	zest_size vertex_memory_in_use;
	zest_size index_memory_in_use;
	zest_uint descriptor_array_index;
} zest_layer_buffers_t;

typedef struct zest_layer_instruction_t {
	char push_constant[128];                    //Each draw instruction can have different values in the push constants push_constants
	zest_scissor_rect_t scissor;                //The drawinstruction can also clip whats drawn
	zest_viewport_t viewport;                   //The viewport size of the draw call
	zest_uint start_index;                      //The starting index
	union {
		zest_uint total_instances;              //The total number of instances to be drawn in the draw instruction
		zest_uint total_indexes;                //The total number of indexes to be drawn in the draw instruction
	};
	zest_uint last_instance;                    //The last instance that was drawn in the previous instance instruction
	zest_uint mesh_index;						//The index of the mesh being drawn in the layer if it has mesh data
	zest_pipeline_template pipeline_template;   //The pipeline template to draw the instances.
	void *asset;                                //Optional pointer to either texture, font etc
	zest_draw_mode draw_mode;
} zest_layer_instruction_t ZEST_ALIGN_AFFIX(16);

zest_hash_map(zest_render_pass) zest_map_render_passes;
zest_hash_map(zest_sampler_handle) zest_map_samplers;
zest_hash_map(zest_descriptor_pool) zest_map_descriptor_pool;
zest_hash_map(zest_frame_graph) zest_map_frame_graphs;

// -- Platform_callbacks_struct
typedef struct zest_platform_t {
	//Frame Graph Platform Commands
	zest_bool                  (*begin_command_buffer)(const zest_command_list command_list);
	void                       (*end_command_buffer)(const zest_command_list command_list);
	zest_bool                  (*set_next_command_buffer)(const zest_command_list command_list, zest_context_queue queue);
	void                       (*acquire_barrier)(const zest_command_list command_list, zest_execution_details_t *exe_details);
	void                       (*release_barrier)(const zest_command_list command_list, zest_execution_details_t *exe_details);
	void*                      (*new_execution_backend)(zloc_linear_allocator_t *allocator);
	zest_frame_graph_semaphores(*get_frame_graph_semaphores)(zest_context context, const char *name);
	zest_bool                  (*submit_frame_graph_batch)(zest_frame_graph frame_graph, zest_execution_backend backend, zest_submission_batch_t *batch, zest_map_queue_value *queues);
	zest_bool                  (*begin_render_pass)(const zest_command_list command_list, zest_execution_details_t *exe_details);
	void                       (*end_render_pass)(const zest_command_list command_list);
	void                       (*carry_over_semaphores)(zest_frame_graph frame_graph, zest_wave_submission_t *wave_submission, zest_execution_backend backend);
	zest_bool                  (*create_execution_timeline_backend)(zest_device device, zest_execution_timeline timeline);
	void  		               (*cleanup_execution_timeline_backend)(zest_execution_timeline timeline);
	void                       (*add_frame_graph_buffer_barrier)(zest_resource_node resource, zest_execution_barriers_t *barriers,
		zest_bool acquire, zest_access_flags src_access, zest_access_flags dst_access,
		zest_uint src_family, zest_uint dst_family, zest_pipeline_stage_flags src_stage,
		zest_pipeline_stage_flags dst_stage);
	void                       (*add_frame_graph_image_barrier)(zest_resource_node resource, zest_execution_barriers_t *barriers, zest_bool acquire,
		zest_access_flags src_access, zest_access_flags dst_access, zest_image_layout old_layout, zest_image_layout new_layout,
		zest_uint src_family, zest_uint dst_family, zest_pipeline_stage_flags src_stage, zest_pipeline_stage_flags dst_stage);
	zest_bool                  (*present_frame)(zest_context context);
	zest_bool                  (*dummy_submit_for_present_only)(zest_context context);
	zest_bool                  (*acquire_swapchain_image)(zest_swapchain swapchain);
	void*                  	   (*new_frame_graph_image_backend)(zloc_linear_allocator_t *allocator, zest_image image, zest_image imported_image);
	//Buffer and memory
	void*                      (*create_buffer_allocator_backend)(zest_device device, zest_context context, zest_size size, zest_buffer_info_t *buffer_info);
	void*                      (*create_buffer_linear_allocator_backend)(zest_device device, zest_size size, zest_buffer_info_t *buffer_info);
	zest_bool                  (*add_buffer_memory_pool)(zest_device device, zest_context context, zest_size size, zest_buffer_allocator buffer_allocator, zest_device_memory_pool memory_pool);
	zest_bool                  (*create_image_memory_pool)(zest_device device, zest_context context, zest_size size_in_bytes, zest_buffer_info_t *buffer_info, zest_device_memory_pool buffer);
	zest_bool                  (*create_device_memory)(zest_device device, zest_size size_in_bytes, zest_buffer_info_t *buffer_info, zest_device_memory memory);
	zest_bool                  (*map_memory)(zest_device_memory_pool memory_allocation, zest_size size, zest_size offset);
	void 		               (*unmap_memory)(zest_device_memory_pool memory_allocation);
	void					   (*flush_used_buffers)(zest_context context, zest_uint fif);
	void					   (*cmd_copy_buffer_one_time)(zest_queue queue, zest_buffer src_buffer, zest_buffer dst_buffer, zest_size size, zest_size src_offset, zest_size dst_offset);
	//Images
	zest_bool 				   (*is_image_format_supported)(zest_device device, zest_format format, zest_image_flags flags);
	zest_bool 				   (*create_image)(zest_device device, zest_image image, zest_uint layer_count, zest_sample_count_flags num_samples, zest_image_flags flags);
	zest_image_view 		   (*create_image_view)(zest_device device, zest_image image, zest_image_view_type view_type, zest_uint mip_levels_this_view, zest_uint base_mip, zest_uint base_array_index, zest_uint layer_count, zloc_linear_allocator_t *linear_allocator);
	zest_image_view_array 	   (*create_image_views_per_mip)(zest_device device, zest_image image, zest_image_view_type view_type, zest_uint base_array_index, zest_uint layer_count, zloc_linear_allocator_t *linear_allocator);
	zest_bool 				   (*copy_buffer_regions_to_image)(zest_queue queue, zest_buffer_image_copy_t *regions, zest_uint regions_count, zest_buffer buffer, zest_size src_offset, zest_image image);
	zest_bool 				   (*transition_image_layout)(zest_queue queue, zest_image image, zest_image_layout new_layout, zest_uint base_mip_index, zest_uint mip_levels, zest_uint base_array_index, zest_uint layer_count);
	zest_bool 				   (*create_sampler)(zest_sampler sampler);
	int 	  				   (*get_image_raw_layout)(zest_image image);
	zest_bool  				   (*image_layout_is_valid_for_descriptor)(zest_image image);
	zest_bool 				   (*copy_buffer_to_image)(zest_queue queue, zest_buffer buffer, zest_size src_offset, zest_image image, zest_uint width, zest_uint height);
	zest_bool 				   (*generate_mipmaps)(zest_queue queue, zest_image image);
	//Descriptor Sets
	zest_bool                  (*create_uniform_descriptor_set)(zest_uniform_buffer buffer, zest_set_layout associated_layout);
	//Pipelines
	zest_bool                  (*build_pipeline)(zest_pipeline pipeline, zest_command_list command_list);
	zest_bool                  (*build_pipeline_layout)(zest_device device, zest_pipeline_layout layout, zest_pipeline_layout_info_t *info);
	zest_bool				   (*finish_compute)(zest_device device, zest_compute compute);
	//Semaphores
	zest_semaphore_status      (*wait_for_renderer_semaphore)(zest_context context);
	zest_semaphore_status      (*wait_for_timeline)(zest_execution_timeline timeline, zest_microsecs timeout);
	//Set layouts
	zest_bool                  (*create_set_layout)(zest_device device, zest_context context, zest_set_layout_builder_t *builder, zest_set_layout layout, zest_bool is_bindless);
	zest_bool                  (*create_set_pool)(zest_device device, zest_context context, zest_descriptor_pool pool, zest_set_layout layout, zest_uint max_set_count, zest_bool bindles);
	zest_descriptor_set        (*create_bindless_set)(zest_set_layout layout);
	void                       (*update_bindless_image_descriptor)(zest_device device, zest_uint binding_number, zest_uint array_index, zest_descriptor_type type, zest_image image, zest_image_view view, zest_sampler sampler, zest_descriptor_set set);
	void                       (*update_bindless_storage_buffer_descriptor)(zest_device device, zest_uint binding_number, zest_uint array_index, zest_buffer buffer, zest_descriptor_set set);
	void                       (*update_bindless_uniform_buffer_descriptor)(zest_device device, zest_uint binding_number, zest_uint array_index, zest_buffer buffer, zest_descriptor_set set);
	//Command buffers/queues
	void					   (*reset_queue_command_pool)(zest_context context, zest_context_queue queue);
	//General Context
	void                       (*set_depth_format)(zest_device device);
	zest_bool                  (*initialise_context_backend)(zest_context context);
	zest_sample_count_flags	   (*get_msaa_sample_count)(zest_context context);
	zest_bool 				   (*initialise_swapchain)(zest_context context);
	zest_bool				   (*initialise_context_queue_backend)(zest_context context, zest_context_queue context_queue);
	//Device/OS
	void                  	   (*wait_for_idle_device)(zest_device device);
	zest_bool 				   (*initialise_device)(zest_device device);
	void					   (*os_add_platform_extensions)(zest_context context);
	zest_bool				   (*create_window_surface)(zest_context context);
	//Shader Compiling
	zest_bool				   (*validate_shader)(zest_device device, const char *shader_code, zest_shader_type type, const char *name);
	zest_bool				   (*compile_shader)(zest_shader shader, const char *code, zest_uint code_length, zest_shader_type, const char *name, const char *entry_point, void *options);
	//Create backends
	void*                      (*new_frame_graph_semaphores_backend)(zest_context context);
	void*                      (*new_execution_barriers_backend)(zloc_linear_allocator_t *allocator);
	void*                      (*new_pipeline_backend)(zest_context context);
	void*                      (*new_pipeline_layout_backend)(zest_device device);
	void*                      (*new_memory_pool_backend)(zest_buffer_allocator buffer_allocator);
	void*                      (*new_memory_backend)(zest_device device);
	void*					   (*new_device_backend)(zest_device device);
	void*					   (*new_context_backend)(zest_context context);
	void*					   (*new_frame_graph_context_backend)(zest_context context);
	void*					   (*new_swapchain_backend)(zest_context context);
	void*					   (*new_image_backend)(zest_device device);
	void*					   (*new_compute_backend)(zest_device device);
	void*					   (*new_queue_backend)(zest_device device, zest_uint family_index);
	void*					   (*new_submission_batch_backend)(zest_context context);
	void*					   (*new_set_layout_backend)(zloc_allocator *allocator);
	void*					   (*new_descriptor_pool_backend)(zloc_allocator *allocator);
	void*					   (*new_sampler_backend)(zest_context context);
	void*					   (*new_context_queue_backend)(zest_context context);
	//Cleanup backends
	void                       (*cleanup_frame_graph_semaphore)(zest_context context, zest_frame_graph_semaphores semaphores);
	void                       (*cleanup_image_backend)(zest_image image);
	void                       (*cleanup_image_view_backend)(zest_image_view image_view);
	void                       (*cleanup_image_view_array_backend)(zest_image_view_array image_view);
	void                       (*cleanup_buffer_allocator_backend)(zest_buffer_allocator buffer_allocator);
	void                       (*cleanup_memory_pool_backend)(zest_device_memory_pool memory_allocation);
	void                       (*cleanup_memory_backend)(zest_device_memory memory);
	void                       (*cleanup_device_backend)(zest_device device);
	void                       (*cleanup_context_backend)(zest_context context);
	void                       (*destroy_context_surface)(zest_context context);
	void 					   (*cleanup_swapchain_backend)(zest_swapchain swapchain);
	void 					   (*cleanup_compute_backend)(zest_compute compute);
	void 					   (*cleanup_set_layout)(zest_set_layout layout);
	void 					   (*cleanup_pipeline_backend)(zest_pipeline pipeline);
	void 					   (*cleanup_pipeline_layout_backend)(zest_pipeline_layout layout);
	void 					   (*cleanup_sampler_backend)(zest_sampler sampler);
	void 					   (*cleanup_queue_backend)(zest_device device, zest_queue queue);
	void 					   (*cleanup_context_queue_backend)(zest_context context, zest_context_queue context_queue);
	void 					   (*cleanup_set_layout_backend)(zest_set_layout sampler);
	//Debugging
	void*                      (*get_final_signal_ptr)(zest_submission_batch_t *batch, zest_uint semaphore_index);
	void*                      (*get_final_wait_ptr)(zest_submission_batch_t *batch, zest_uint semaphore_index);
	void*                      (*get_resource_ptr)(zest_resource_node resource);
	void*                      (*get_buffer_ptr)(zest_buffer buffer);
	void*	      			   (*get_swapchain_wait_semaphore)(zest_swapchain swapchain, zest_uint index);
	void*	      			   (*get_swapchain_signal_semaphore)(zest_swapchain swapchain, zest_uint index);
	//Command recording
	void					   (*blit_image_mip)(const zest_command_list command_list, zest_resource_node src, zest_resource_node dst, zest_uint mip_to_blit, zest_pipeline_stage_flags pipeline_stage);
	void                       (*copy_image_mip)(const zest_command_list command_list, zest_resource_node src, zest_resource_node dst, zest_uint mip_to_blit, zest_pipeline_stage_flags pipeline_stage);
	void                       (*insert_compute_image_barrier)(const zest_command_list command_list, zest_resource_node resource, zest_uint base_mip);
	void                       (*set_screensized_viewport)(const zest_command_list command_list, float min_depth, float max_depth);
	void                       (*scissor)(const zest_command_list command_list, zest_scissor_rect_t *scissor);
	void                       (*viewport)(const zest_command_list command_list, zest_viewport_t *viewport);
	void                       (*clip)(const zest_command_list command_list, float x, float y, float width, float height, float minDepth, float maxDepth);
	void                       (*bind_descriptor_sets)(const zest_command_list command_list, zest_pipeline_bind_point bind_point, zest_pipeline_layout layout, zest_descriptor_set *descriptor_sets, zest_uint set_count, zest_uint first_set);
	void                       (*bind_pipeline)(const zest_command_list command_list, zest_pipeline pipeline);
	void                       (*bind_compute_pipeline)(const zest_command_list command_list, zest_compute compute);
	void                       (*copy_buffer)(const zest_command_list command_list, zest_buffer staging_buffer, zest_buffer device_buffer, zest_size size);
	zest_bool                  (*upload_buffer)(const zest_command_list command_list, zest_buffer_uploader_t *uploader);
	void                       (*bind_vertex_buffer)(const zest_command_list command_list, zest_uint first_binding, zest_uint binding_count, zest_buffer buffer);
	void                       (*bind_index_buffer)(const zest_command_list command_list, zest_buffer buffer);
	zest_bool                  (*image_clear)(const zest_command_list command_list, zest_image image);
	void                       (*bind_mesh_vertex_buffer)(const zest_command_list command_list, zest_layer layer);
	void                       (*bind_mesh_index_buffer)(const zest_command_list command_list, zest_layer layer);
	void                       (*dispatch_compute)(const zest_command_list command_list, zest_uint group_count_x, zest_uint group_count_y, zest_uint group_count_z);
	void                       (*send_push_constants)(const zest_command_list command_list, zest_pipeline_layout layout, void *data, zest_uint size);
	void                       (*draw)(const zest_command_list command_list, zest_uint vertex_count, zest_uint instance_count, zest_uint first_vertex, zest_uint first_instance);
	void                       (*draw_layer_instruction)(const zest_command_list command_list, zest_uint vertex_count, zest_layer_instruction_t *instruction);
	void                       (*draw_indexed)(const zest_command_list command_list, zest_uint index_count, zest_uint instance_count, zest_uint first_index, int32_t vertex_offset, zest_uint first_instance);
	void                       (*set_depth_bias)(const zest_command_list command_list, float factor, float clamp, float slope);
} zest_platform_t;

extern zest_platform_t ZestPlatform;

typedef struct zest_generic_handle {
	zest_u64 value;
	zest_resource_store_t *store;
}zest_generic_handle;

ZEST_API inline zest_bool zest_IsValidHandle(void *handle) {
	zest_generic_handle *generic_handle = (zest_generic_handle *)handle;
	if (!generic_handle && ZEST_VALID_HANDLE(generic_handle->store, zest_struct_type_resource_store)) {
		return ZEST_FALSE;
	}
	zest_uint index = ZEST_HANDLE_INDEX(generic_handle->value);
	zest_uint generation = ZEST_HANDLE_GENERATION(generic_handle->value);
	if (generation > 0) {
		if (index < generic_handle->store->data.current_size) {
			char *resource = (char*)zest__bucket_array_get(&generic_handle->store->data, index);
			return ZEST_VALID_HANDLE(resource, generic_handle->store->struct_type);
		}
	}
	return ZEST_FALSE;
}

ZEST_PRIVATE inline zest_bool zest__resource_is_initialised(zest_resource_store_t *store, zest_uint index) {
	zest_uint word_idx = index / ZEST_BITS_PER_WORD;
	zest_u64 bit_idx = index % ZEST_BITS_PER_WORD;
	return (store->initialised[word_idx] & (1ULL << bit_idx)) > 0;
}

ZEST_PRIVATE inline void *zest__get_store_resource_unsafe(zest_resource_store_t *store, zest_handle handle) {
	ZEST_ASSERT(store, "Tried to fetch a resource but the store was null. Check the stack trace and make sure it's a valid handle that you're tring to fetch.");
	zest_uint index = ZEST_HANDLE_INDEX(handle);
	zest_uint generation = ZEST_HANDLE_GENERATION(handle);
	void *resource;
	zest__sync_lock(&store->sync);
	if (store->generations[index] == generation) {
		resource = zest__bucket_array_get(&store->data, index);
	}
	zest__sync_unlock(&store->sync);
	return resource;
}

ZEST_PRIVATE inline void *zest__get_store_resource_checked(zest_resource_store_t *store, zest_handle handle) {
	ZEST_ASSERT(store, "Tried to fetch a resource but the store was null. Check the stack trace and make sure it's a valid handle that you're tring to fetch.");
	zest_uint index = ZEST_HANDLE_INDEX(handle);
	zest_uint generation = ZEST_HANDLE_GENERATION(handle);
	void *resource = NULL;
	zest__sync_lock(&store->sync);
	if (store->generations[index] == generation && zest__resource_is_initialised(store, index)) {
		resource = zest__bucket_array_get(&store->data, index);
	}
	zest__sync_unlock(&store->sync);
	return resource;
}

typedef void(*zest__platform_setup)(zest_platform_t *platform);
extern zest__platform_setup zest__platform_setup_callbacks[zest_max_platforms];

//--Internal_functions
//Only available outside lib for some implementations like SDL2
ZEST_API zest_size zest_GetNextPower(zest_size n);

//Platform_dependent_functions
//These functions need a different implementation depending on the platform being run on
//See definitions at the top of zest.c
ZEST_PRIVATE zest_bool zest__create_folder(zest_device device, const char *path);
//-- End Platform dependent functions

//Buffer_and_Memory_Management
ZEST_PRIVATE void zest__add_host_memory_pool(zest_device device, zest_size size);
ZEST_PRIVATE void zest__add_context_memory_pool(zest_context context, zest_size size);
ZEST_PRIVATE void zest__add_memory_pool(zloc_allocator *allocator, zest_size requested_size);
ZEST_PRIVATE void *zest__allocate(zloc_allocator *allocator, zest_size size);
ZEST_PRIVATE void *zest__allocate_aligned(zloc_allocator *allocator, zest_size size, zest_size alignment);
ZEST_PRIVATE void *zest__reallocate(zloc_allocator *allocator, void *memory, zest_size size);
ZEST_PRIVATE void *zest__linear_allocate(zloc_linear_allocator_t *allocator, zest_size size);
ZEST_PRIVATE zest_size zest__get_largest_slab(zloc_linear_allocator_t *allocator);
ZEST_PRIVATE zest_buffer zest__create_transient_buffer(zest_context context, zest_size size, zest_buffer_info_t* buffer_info);
ZEST_PRIVATE void zest__unmap_memory(zest_device_memory_pool memory_allocation);
ZEST_PRIVATE void zest__destroy_memory(zest_device_memory_pool memory_allocation);
ZEST_PRIVATE zest_buffer_allocator zest__create_buffer_allocator(zest_device device, zest_context context, zest_buffer_info_t *buffer_info, zest_key key, zest_key pool_key);
ZEST_PRIVATE zest_bool zest__add_gpu_memory_pool(zest_buffer_allocator allocator, zest_size minimum_size, zest_device_memory_pool *memory_pool);
ZEST_PRIVATE zest_device_memory zest__create_device_memory(zest_device device, zest_size size, zest_buffer_info_t *buffer_info);
ZEST_PRIVATE void zest__add_remote_range_pool(zest_buffer_allocator buffer_allocator, zest_device_memory_pool buffer_pool);
ZEST_PRIVATE void zest__cleanup_buffers_in_allocators(zest_device device);
ZEST_PRIVATE zest_buffer_linear_allocator zest__create_linear_buffer_allocator(zest_context context, zest_buffer_info_t *buffer_info, zest_size size);
//End Buffer Management

//Queue_management
ZEST_PRIVATE zest_queue zest__acquire_queue(zest_device device, zest_uint family_index);
ZEST_PRIVATE void zest__release_queue(zest_queue queue);
ZEST_PRIVATE zest_bool zest__initialise_timeline(zest_device device, zest_execution_timeline_t *timeline);
//End Queue_management

//Renderer_functions
ZEST_PRIVATE zest_bool zest__initialise_context(zest_context context, zest_create_context_info_t *create_info);
ZEST_PRIVATE zest_swapchain zest__create_swapchain(zest_context context, const char *name);
ZEST_PRIVATE void zest__get_window_size_callback(zest_context context, void *user_data, int *fb_width, int *fb_height, int *window_width, int *window_height);
ZEST_PRIVATE void zest__destroy_window_callback(zest_context window, void *user_data);
ZEST_PRIVATE void zest__cleanup_swapchain(zest_swapchain swapchain);
ZEST_PRIVATE void zest__free_all_device_resource_stores(zest_device device);
ZEST_PRIVATE void zest__cleanup_device(zest_device device);
ZEST_PRIVATE void zest__cleanup_context(zest_context context);
ZEST_PRIVATE void zest__cleanup_image_store(zest_device device);
ZEST_PRIVATE void zest__cleanup_sampler_store(zest_device device);
ZEST_PRIVATE void zest__cleanup_uniform_buffer_store(zest_context context);
ZEST_PRIVATE void zest__cleanup_layer_store(zest_context context);
ZEST_PRIVATE void zest__cleanup_shader_store(zest_device device);
ZEST_PRIVATE void zest__cleanup_compute_store(zest_device device);
ZEST_PRIVATE void zest__cleanup_view_store(zest_device device);
ZEST_PRIVATE void zest__cleanup_view_array_store(zest_device device);
ZEST_PRIVATE void zest__free_handle(zloc_allocator *allocator, void *handle);
ZEST_PRIVATE void zest__scan_memory_and_free_resources(void *origin, zest_bool including_context);
ZEST_PRIVATE void zest__cleanup_compute(zest_compute compute);
ZEST_PRIVATE zest_bool zest__recreate_swapchain(zest_context context);
ZEST_PRIVATE void zest__add_line(zest_text_t *text, char current_char, zest_uint *position, zest_uint tabs);
ZEST_PRIVATE void zest__compile_builtin_shaders(zest_device device);
ZEST_PRIVATE void zest__prepare_standard_pipelines(zest_device device);
ZEST_PRIVATE void zest__cleanup_pipeline(zest_pipeline pipeline);
ZEST_PRIVATE void zest__cleanup_pipelines(zest_context context);
ZEST_PRIVATE zest_render_pass zest__create_render_pass(void);
// --End Renderer functions

// --Draw_layer_internal_functions
ZEST_PRIVATE zest_layer_handle zest__new_layer(zest_context context, zest_layer *layer);
ZEST_PRIVATE void zest__start_mesh_instructions(zest_layer layer);
ZEST_PRIVATE void zest__end_mesh_instructions(zest_layer layer);
ZEST_PRIVATE void zest__update_instance_layer_resolution(zest_layer layer);
ZEST_PRIVATE zest_layer_instruction_t zest__layer_instruction(void);
ZEST_PRIVATE void zest__reset_mesh_layer_drawing(zest_layer layer);
ZEST_PRIVATE zest_bool zest__grow_instance_buffer(zest_layer layer, zest_size type_size, zest_size minimum_size);
ZEST_PRIVATE void zest__cleanup_layer(zest_layer layer);
ZEST_PRIVATE void zest__initialise_instance_layer(zest_context context, zest_layer layer, zest_size type_size, zest_uint instance_pool_size);
ZEST_PRIVATE void zest__start_instance_instructions(zest_layer layer);
ZEST_PRIVATE void zest__end_instance_instructions(zest_layer layer);
ZEST_PRIVATE void zest__reset_instance_layer_drawing(zest_layer layer);
ZEST_PRIVATE void zest__set_layer_push_constants(zest_layer layer, void *push_constants, zest_size size);

// --Image_internal_functions
ZEST_PRIVATE zest_image_handle zest__new_image(zest_device device);
ZEST_PRIVATE zest_image_handle zest__create_image(zest_device device, zest_image_info_t *create_info);
ZEST_PRIVATE void zest__release_all_global_texture_indexes(zest_device device, zest_image image);
ZEST_PRIVATE void zest__release_all_image_indexes(zest_device device);
ZEST_PRIVATE void zest__cleanup_image_view(zest_image_view layout);
ZEST_PRIVATE void zest__cleanup_image_view_array(zest_image_view_array layout);
ZEST_PRIVATE zest_image zest__get_image_unsafe(zest_image_handle handle);

// --General_layer_internal_functions
ZEST_PRIVATE zest_layer_handle zest__create_instance_layer(zest_context context, const char *name, zest_size instance_type_size, zest_uint initial_instance_count);
ZEST_PRIVATE void zest__draw_instance_mesh_layer(zest_command_list command_list, zest_layer layer, zest_pipeline_template pipeline_template);

// --Mesh_layer_internal_functions
ZEST_PRIVATE void zest__initialise_mesh_layer(zest_context context, zest_layer mesh_layer, zest_size vertex_struct_size, zest_size initial_vertex_capacity);

// --Misc_Helper_Functions
ZEST_PRIVATE zest_image_view_type zest__get_image_view_type(zest_image image);
ZEST_PRIVATE zest_bool zest__create_transient_image(zest_context context, zest_resource_node node);
ZEST_PRIVATE zest_uint zest__next_fif(zest_context context);
ZEST_PRIVATE zloc_linear_allocator_t *zest__get_scratch_arena(zest_device device);
// --End Misc_Helper_Functions

// --Pipeline_Helper_Functions
ZEST_PRIVATE zest_pipeline zest__create_pipeline(zest_context context);
ZEST_PRIVATE zest_bool zest__cache_pipeline(zest_pipeline_template pipeline_template, zest_command_list context, zest_key key, zest_pipeline *out_pipeline);
ZEST_PRIVATE void zest__cleanup_pipeline_template(zest_pipeline_template pipeline);
ZEST_PRIVATE void zest__cleanup_pipeline_layout(zest_pipeline_layout layout);
// --End Pipeline Helper Functions

// --Buffer_allocation_funcitons
ZEST_PRIVATE zest_size zest__get_minimum_block_size(zest_size pool_size);
ZEST_PRIVATE void zest__on_add_pool(void *user_data, void *block);
ZEST_PRIVATE void zest__on_split_block(void *user_data, zloc_header* block, zloc_header *trimmed_block, zloc_size remote_size);
// --End Buffer allocation funcitons

// --Uniform_buffers
// --End Uniform Buffers

// --Maintenance_functions
ZEST_PRIVATE void zest__cleanup_image(zest_image image);
ZEST_PRIVATE void zest__cleanup_sampler(zest_sampler sampler);
ZEST_PRIVATE void zest__cleanup_uniform_buffer(zest_uniform_buffer uniform_buffer);
ZEST_PRIVATE void zest__cleanup_execution_timeline(zest_execution_timeline timeline);
// --End Maintenance functions

// --Shader_functions
ZEST_PRIVATE zest_shader_handle zest__new_shader(zest_device device, zest_shader_type type);
ZEST_API void zest__cache_shader(zest_device device, zest_shader shader);
// --End Shader functions

// --Descriptor_set_functions
ZEST_PRIVATE zest_set_layout zest__new_descriptor_set_layout(zest_device device, zest_context context, const char *name);
ZEST_PRIVATE zest_descriptor_pool zest__create_descriptor_pool(zest_device device, zloc_allocator *allocator, zest_uint max_sets);
ZEST_PRIVATE zest_bool zest__binding_exists_in_layout_builder(zest_set_layout_builder_t *builder, zest_uint binding);
ZEST_PRIVATE zest_uint zest__acquire_bindless_index(zest_set_layout layout, zest_uint binding_number);
ZEST_PRIVATE void zest__release_bindless_index(zest_set_layout layout, zest_uint binding_number, zest_uint index_to_release);
ZEST_PRIVATE void zest__cleanup_set_layout(zest_set_layout layout);
ZEST_PRIVATE zest_uint zest__acquire_bindless_image_index(zest_device device, zest_image image, zest_image_view view, zest_set_layout layout, zest_descriptor_set set, zest_binding_number_type target_binding_number, zest_descriptor_type descriptor_type);
ZEST_PRIVATE zest_uint zest__acquire_bindless_storage_buffer_index(zest_device device, zest_buffer buffer, zest_set_layout layout, zest_descriptor_set set, zest_uint target_binding_number);
ZEST_PRIVATE zest_uint zest__acquire_bindless_sampler_index(zest_device device, zest_sampler sampler, zest_set_layout layout, zest_descriptor_set set, zest_binding_number_type target_binding_number);
// --End Descriptor set functions

// --Device_set_up
ZEST_PRIVATE void zest__initialise_device_stores(zest_device device);
ZEST_PRIVATE void zest__set_default_pool_sizes(zest_device device);
ZEST_PRIVATE zest_bool zest__initialise_vulkan_device(zest_device device, zest_device_builder info);
ZEST_PRIVATE zest_bool zest__is_vulkan_device(zest_device device); 
ZEST_PRIVATE void zest__create_default_images(zest_device device, zest_device_builder builder);
//end device setup functions

//App_initialise_and_run_functions
ZEST_API zest_device_builder zest__begin_device_builder();
ZEST_PRIVATE void zest__do_context_scheduled_tasks(zest_context context);
ZEST_PRIVATE void zest__destroy_device(zest_device device);
ZEST_PRIVATE zest_semaphore_status zest__main_loop_semaphore_wait(zest_context context);
ZEST_PRIVATE void zest__free_device_buffer_allocators(zest_device device);
ZEST_PRIVATE void zest__free_context_buffer_allocators(zest_context context);
//-- end of internal functions

// Enum_to_string_functions - Helper functions to convert enums to strings 
ZEST_PRIVATE const char *zest__image_layout_to_string(zest_image_layout layout);
ZEST_PRIVATE const char *zest__memory_property_to_string(zest_memory_property_flags property_flags);
ZEST_PRIVATE const char *zest__memory_type_to_string(zest_memory_pool_type memory_type);
ZEST_PRIVATE zest_text_t zest__access_flags_to_string(zest_context context, zest_access_flags flags);
ZEST_PRIVATE zest_text_t zest__pipeline_stage_flags_to_string(zest_context context, zest_pipeline_stage_flags flags);
// -- end Enum_to_string_functions

// --- Internal_render_graph_functions ---
ZEST_PRIVATE zest_bool zest__is_stage_compatible_with_qfi(zest_pipeline_stage_flags stages_to_check, zest_device_queue_type queue_family_capabilities);
ZEST_API_TMP zest_image_layout zest__determine_final_layout(zest_uint pass_index, zest_resource_node node, zest_resource_usage_t *current_usage);
ZEST_API_TMP zest_image_aspect_flags zest__determine_aspect_flag(zest_format format);
ZEST_API_TMP zest_image_aspect_flags zest__determine_aspect_flag_for_view(zest_format format);
ZEST_PRIVATE zest_bool zest__is_depth_stencil_format(zest_format format);
ZEST_PRIVATE zest_bool zest__is_compressed_format(zest_format format);
ZEST_PRIVATE void zest__interpret_hints(zest_resource_node resource, zest_resource_usage_hint usage_hints);
ZEST_PRIVATE void zest__deferr_resource_destruction(zest_context context, void *handle);
ZEST_PRIVATE void zest__deferr_image_destruction(zest_context context, zest_image image);
ZEST_PRIVATE void zest__deferr_view_array_destruction(zest_context context, zest_image_view_array view_array);
ZEST_PRIVATE zest_pass_node zest__add_pass_node(const char *name, zest_device_queue_type intended_queue_type);
ZEST_PRIVATE zest_resource_node zest__add_frame_graph_resource(zest_resource_node resource);
ZEST_PRIVATE zest_resource_versions_t *zest__maybe_add_resource_version(zest_resource_node resource);
ZEST_PRIVATE zest_resource_node_t zest__create_import_image_resource_node(const char *name, zest_image image);
ZEST_PRIVATE zest_resource_node_t zest__create_import_buffer_resource_node(const char *name, zest_buffer buffer);
ZEST_PRIVATE zest_resource_node zest__add_transient_image_resource(const char *name, const zest_image_info_t *desc, zest_bool assign_bindless, zest_bool image_view_binding_only);
ZEST_PRIVATE zest_bool zest__create_transient_resource(zest_context context, zest_resource_node resource);
ZEST_PRIVATE void zest__free_transient_resource(zest_resource_node resource);
ZEST_PRIVATE void zest__add_pass_buffer_usage(zest_pass_node pass_node, zest_resource_node buffer_resource, zest_resource_purpose purpose, zest_pipeline_stage_flags relevant_pipeline_stages, zest_bool is_output);
ZEST_PRIVATE void zest__add_pass_image_usage(zest_pass_node pass_node, zest_resource_node image_resource, zest_resource_purpose purpose, zest_pipeline_stage_flags relevant_pipeline_stages, zest_bool is_output, zest_load_op load_op, zest_store_op store_op, zest_load_op stencil_load_op, zest_store_op stencil_store_op, zest_clear_value_t clear_value);
ZEST_PRIVATE zest_frame_graph zest__new_frame_graph(zest_context context, const char *name);
ZEST_PRIVATE zest_frame_graph zest__compile_frame_graph();
ZEST_PRIVATE void zest__prepare_render_pass(zest_pass_group_t *pass, zest_execution_details_t *exe_details, zest_uint current_pass_index);
ZEST_PRIVATE zest_bool zest__execute_frame_graph(zest_context context, zest_frame_graph frame_graph, zest_bool is_intraframe);
ZEST_PRIVATE void zest__add_image_barriers(zest_frame_graph frame_graph, zloc_linear_allocator_t *allocator, zest_resource_node resource, zest_execution_barriers_t *barriers,
										zest_resource_state_t *current_state, zest_resource_state_t *prev_state, zest_resource_state_t *next_state);
ZEST_PRIVATE zest_resource_usage_t zest__configure_image_usage(zest_resource_node resource, zest_resource_purpose purpose, zest_format format, zest_load_op load_op, zest_load_op stencil_load_op, zest_pipeline_stage_flags relevant_pipeline_stages);
ZEST_PRIVATE zest_image_usage_flags zest__get_image_usage_from_state(zest_resource_state state);
ZEST_PRIVATE zest_submission_batch_t *zest__get_submission_batch(zest_uint submission_id);
ZEST_PRIVATE void zest__set_rg_error_status(zest_frame_graph frame_graph, zest_frame_graph_result result);
ZEST_PRIVATE zest_bool zest__detect_cyclic_recursion(zest_frame_graph frame_graph, zest_pass_node pass_node);
ZEST_PRIVATE void zest__cache_frame_graph(zest_frame_graph frame_graph);
ZEST_PRIVATE zest_key zest__hash_frame_graph_cache_key(zest_frame_graph_cache_key_t *cache_key);
// --- End Internal_render_graph_functions ---

//User API functions

//-----------------------------------------------
//        Essential_setup_functions
//-----------------------------------------------
//Device creation builder
ZEST_API zest_device_builder zest_BeginVulkanDeviceBuilder();
//Add a required extension to the device builder. This will be used to find a suitable GPU in the machine
ZEST_API void zest_AddDeviceBuilderExtension(zest_device_builder builder, const char *extension_name);
ZEST_API void zest_AddDeviceBuilderExtensions(zest_device_builder builder, const char **extension_names, int cout);
ZEST_API void zest_AddDeviceBuilderValidation(zest_device_builder builder);
ZEST_API void zest_AddDeviceBuilderFullValidation(zest_device_builder builder);
ZEST_API void zest_DeviceBuilderLogToConsole(zest_device_builder builder);
ZEST_API void zest_DeviceBuilderPrintMemoryInfo(zest_device_builder builder);
ZEST_API void zest_DeviceBuilderLogToMemory(zest_device_builder builder);
ZEST_API void zest_DeviceBuilderLogPath(zest_device_builder builder, const char *log_path);
//Set the default pool size for the cpu memory used for the device
ZEST_API void zest_SetDeviceBuilderMemoryPoolSize(zest_device_builder builder, zest_size size);
//Finish and create the device
ZEST_API zest_device zest_EndDeviceBuilder(zest_device_builder builder);
//Create a new zest_create_context_info_t struct with default values for initialising Zest
ZEST_API zest_create_context_info_t zest_CreateContextInfo();
//Initialise Zest. You must call this in order to use Zest. Use zest_CreateContextInfo() to set up some default values to initialise the renderer.
ZEST_API zest_context zest_CreateContext(zest_device device, zest_window_data_t *window_data, zest_create_context_info_t* info);
//Begin a new frame for a context. Within the BeginFrame and EndFrame you can create a frame graph and present a frame.
//This funciton will wait on the fence from the previous time a frame was submitted.
ZEST_API zest_bool zest_BeginFrame(zest_context context);
ZEST_API void zest_EndFrame(zest_context context, zest_frame_graph frame_gaph);
//Maintenance function to be run each time the application loops to flush any unused resources that have been 
//marked for deletion.
ZEST_API int zest_UpdateDevice(zest_device device);
//Shutdown zest and unload/free everything. Call this at the end of your app to free all memory and shut things
//down gracefully
ZEST_API void zest_DestroyDevice(zest_device device);
//Free all memory used in a context 
ZEST_API void zest_DestroyContext(zest_context context);
//Free all memory used in the device and reset it back to an initial state.
ZEST_API void zest_ResetDevice(zest_device device);
//Free all memory used in a context and reset it back to an initial state.
ZEST_API void zest_ResetContext(zest_context context, zest_window_data_t *window_data);
//Set the create info for the renderer, to be used optionally before a call to zest_ResetRenderer to change the configuration
//of the renderer
ZEST_API void zest_SetCreateInfo(zest_context context, zest_create_context_info_t *info);
//Set the pointer to user data in the context
ZEST_API void zest_SetContextUserData(zest_context context, void *user_data);
//Used internally by platform layers
ZEST_API void zest__register_platform(zest_platform_type type, zest__platform_setup callback);

//-----------------------------------------------
//        Platform_Helper_Functions
//        These functions are for more advanced customisation of the render where more knowledge of how graphics APIs work is required.
//-----------------------------------------------
//Add an instance extension. You don't really need to worry about this function unless you're looking to customise the render with some specific extensions
ZEST_API void zest_AddInstanceExtension(zest_device device, char *extension);
//Create a descriptor pool based on a descriptor set layout. This will take the max sets value and create a pool 
//with enough descriptor pool types based on the bindings found in the layout
ZEST_API zest_bool zest_CreateDescriptorPoolForLayout(zest_set_layout layout, zest_uint max_set_count);
//Create a descriptor layout builder object that you can use with the AddBuildLayout commands to put together more complex/fine tuned descriptor
//set layouts
ZEST_API zest_set_layout_builder_t zest_BeginSetLayoutBuilder(zloc_allocator *allocator);

ZEST_API void zest_AddLayoutBuilderBinding(zest_set_layout_builder_t *builder, zest_descriptor_binding_desc_t description);

//Build the descriptor set layout and add it to the renderer. This is for large global descriptor set layouts
ZEST_API zest_set_layout zest_FinishDescriptorSetLayoutForBindless(zest_device device, zest_set_layout_builder_t *builder, zest_uint num_global_sets_this_pool_should_support, const char *name, ...);
//Build the descriptor set layout and add it to the render. The layout is also returned from the function.
ZEST_API zest_set_layout zest_FinishDescriptorSetLayout(zest_context context, zest_set_layout_builder_t *builder, const char *name, ...);

ZEST_API zest_descriptor_set zest_CreateBindlessSet(zest_set_layout layout);
ZEST_API zest_uint zest_AcquireSampledImageIndex(zest_device device, zest_image image, zest_binding_number_type binding_number);
ZEST_API zest_uint zest_AcquireStorageImageIndex(zest_device device, zest_image image, zest_binding_number_type binding_number);
ZEST_API zest_uint zest_AcquireSamplerIndex(zest_device device, zest_sampler sampler);
ZEST_API zest_uint zest_AcquireStorageBufferIndex(zest_device device, zest_buffer buffer);
ZEST_API zest_uint zest_AcquireUniformBufferIndex(zest_device device, zest_buffer buffer);
ZEST_API zest_uint *zest_AcquireImageMipIndexes(zest_device device, zest_image image, zest_image_view_array image_view_array, zest_binding_number_type binding_number, zest_descriptor_type descriptor_type);
ZEST_API void zest_AcquireInstanceLayerBufferIndex(zest_device device, zest_layer layer);
ZEST_API void zest_ReleaseStorageBufferIndex(zest_device device, zest_uint array_index);
ZEST_API void zest_ReleaseImageIndex(zest_device device, zest_image image, zest_binding_number_type binding_number);
ZEST_API void zest_ReleaseImageMipIndexes(zest_device device, zest_image image, zest_binding_number_type binding_number);
ZEST_API void zest_ReleaseAllImageIndexes(zest_device device, zest_image image);
ZEST_API void zest_ReleaseBindlessIndex(zest_device device, zest_uint index, zest_binding_number_type binding_number);
ZEST_API zest_descriptor_set zest_GetBindlessSet(zest_device device);
ZEST_API zest_set_layout zest_GetBindlessLayout(zest_device device);
ZEST_API zest_pipeline_layout zest_GetDefaultPipelineLayout(zest_device device);
//Create a zest_viewport_t for setting the viewport when rendering
ZEST_API zest_viewport_t zest_CreateViewport(float x, float y, float width, float height, float minDepth, float maxDepth);
//Create a zest_scissor_rect_t for render clipping
ZEST_API zest_scissor_rect_t zest_CreateRect2D(zest_uint width, zest_uint height, int offsetX, int offsetY);
//Validate a shader from a string and add it to the library of shaders in the renderer
ZEST_API zest_bool zest_ValidateShader(zest_device device, const char *shader_code, zest_shader_type type, const char *name);
//Creates and compiles a new shader from a string and add it to the library of shaders in the renderer
ZEST_API zest_shader_handle zest_CreateShader(zest_device device, const char *shader_code, zest_shader_type type, const char *name, zest_bool disable_caching);
//Creates a shader from a file containing the shader glsl code
ZEST_API zest_shader_handle zest_CreateShaderFromFile(zest_device device, const char *file, const char *name, zest_shader_type type, zest_bool disable_caching);
//Creates and compiles a new shader from a string and add it to the library of shaders in the renderer
ZEST_API zest_shader_handle zest_CreateShaderSPVMemory(zest_device device, const unsigned char *shader_code, zest_uint spv_length, const char *name, zest_shader_type type);
//Get a shader pointer that you can use to pass in to functions. Curently there's no real use for this so may
//remove in the future.
ZEST_API zest_shader zest_GetShader(zest_shader_handle shader_handle);
//Reload a shader. Use this if you edited a shader file and you want to refresh it/hot reload it
//The shader must have been created from a file with zest_CreateShaderFromFile. Once the shader is reloaded you can call
//zest_CompileShader or zest_ValidateShader to recompile it. You'll then have to call zest_SchedulePipelineRecreate to recreate
//the pipeline that uses the shader. Returns true if the shader was successfully loaded.
ZEST_API zest_bool zest_ReloadShader(zest_shader_handle shader);
//Creates and compiles a new shader from a string and add it to the library of shaders in the renderer
ZEST_API zest_bool zest_CompileShader(zest_shader_handle shader);
//Add a shader straight from an spv file and return a handle to the shader. Note that no prefix is added to the filename here so 
//pass in the full path to the file relative to the executable being run.
ZEST_API zest_shader_handle zest_CreateShaderFromSPVFile(zest_device device, const char *filename, zest_shader_type type);
//Add an spv shader straight from memory and return a handle to the shader. Note that the name should just be the name of the shader, 
//If a path prefix is set (context->device->shader_path_prefix, set when initialising Zest in the create_info struct, spv is default) then
//This prefix will be prepending to the name you pass in here.
ZEST_API zest_shader_handle zest_AddShaderFromSPVMemory(zest_device device, const char *name, const void *buffer, zest_uint size, zest_shader_type type);
//Free the memory for a shader and remove if from the shader list in the renderer (if it exists there)
ZEST_API void zest_FreeShader(zest_shader_handle shader);
//Get the maximum image dimension available on the device
ZEST_API zest_uint zest_GetMaxImageSize(zest_context context);
//Get the device/platform associated with a context
ZEST_API zest_device zest_GetContextDevice(zest_context context);
// -- End Platform_Helper_Functions

//--External_lib_helpers
//GLFW Header
ZEST_API zest_device zest_implglfw_CreateVulkanDevice(zest_bool enable_validation);
ZEST_API zest_window_data_t zest_implglfw_CreateWindow( int x, int y, int width, int height, zest_bool maximised, const char *title);
ZEST_API void zest_implglfw_GetWindowSizeCallback(zest_window_data_t *window_data, int* fb_width, int* fb_height, int* window_width, int* window_height );
ZEST_API zest_bool zest_implglfw_WindowIsFocused(void *window_handle);
ZEST_API void zest_implglfw_DestroyWindow(zest_context context);

//SDL Header
ZEST_API zest_window_data_t zest_implsdl2_CreateWindow(int x, int y, int width, int height, zest_bool maximised, const char* title);
ZEST_API void zest_implsdl2_GetWindowSizeCallback(zest_window_data_t *window_data, int *fb_width, int *fb_height, int *window_width, int *window_height);
ZEST_API void zest_implsdl2_SetWindowMode(zest_context context, zest_window_mode mode);
ZEST_API void zest_implsdl2_DestroyWindow(zest_context context);
//End External_lib_helpers

//-----------------------------------------------
//        Pipeline_related_helpers
//        Pipelines are essential to drawing things on screen. There are some builtin pipeline_templates that Zest creates
//        for sprite/billboard/mesh/font drawing. You can take a look in zest__prepare_standard_pipelines to see how
//        the following functions are utilised, plus look at the exmaples for building your own custom pipeline_templates.
//-----------------------------------------------
//Add a new pipeline template to the renderer and return its handle.
ZEST_API zest_pipeline_template zest_CreatePipelineTemplate(zest_device device, const char *name);
//Set the name of the file to use for the vert and frag shader in the zest_pipeline_template_create_info_t
ZEST_API void zest_SetPipelineVertShader(zest_pipeline_template pipeline_template, zest_shader_handle vert_shader);
ZEST_API void zest_SetPipelineFragShader(zest_pipeline_template pipeline_template, zest_shader_handle frag_shader);
ZEST_API void zest_SetPipelineShaders(zest_pipeline_template pipeline_template, zest_shader_handle vertex_shader, zest_shader_handle fragment_shader);
ZEST_API void zest_SetPipelineFrontFace(zest_pipeline_template pipeline_template, zest_front_face front_face);
ZEST_API void zest_SetPipelineTopology(zest_pipeline_template pipeline_template, zest_topology topology);
ZEST_API void zest_SetPipelineCullMode(zest_pipeline_template pipeline_template, zest_cull_mode cull_mode);
ZEST_API void zest_SetPipelinePolygonFillMode(zest_pipeline_template pipeline_template, zest_polygon_mode polygon_mode);
//Set the name of both the fragment and vertex shader to the same file (frag and vertex shaders can be combined into the same spv)
ZEST_API void zest_SetPipelineShader(zest_pipeline_template pipeline_template, zest_shader_handle combined_vertex_and_fragment_shader);
//Add a new binding description which is used to set the size of the struct (stride) and the vertex input rate.
//You can add as many bindings as you need, just make sure you set the correct binding index for each one
ZEST_API zest_vertex_binding_desc_t zest_AddVertexInputBindingDescription(zest_pipeline_template pipeline_template, zest_uint binding, zest_uint stride, zest_input_rate input_rate);
//Clear the input binding descriptions from the zest_pipeline_template_create_info_t bindingDescriptions array.
ZEST_API void zest_ClearVertexInputBindingDescriptions(zest_pipeline_template pipeline_template);
//Clear the input attribute descriptions from the zest_pipeline_template_create_info_t attributeDescriptions array.
ZEST_API void zest_ClearVertexAttributeDescriptions(zest_pipeline_template pipeline_template);
//Add a vertex attribute to a zest_vertex_input_descriptions array. You can use zest_CreateVertexInputDescription
//helper function to create the description
ZEST_API void zest_AddVertexAttribute(zest_pipeline_template pipeline_template, zest_uint binding, zest_uint location, zest_format format, zest_uint offset);
ZEST_API void zest_SetPipelineBlend(zest_pipeline_template pipeline_template, zest_color_blend_attachment_t blend_attachment);
ZEST_API void zest_SetPipelineDepthTest(zest_pipeline_template pipeline_template, zest_bool enable_test, zest_bool write_enable);
ZEST_API void zest_SetPipelineDepthBias(zest_pipeline_template pipeline_template, zest_bool enabled);
ZEST_API void zest_SetPipelineEnableVertexInput(zest_pipeline_template pipeline_template);
ZEST_API void zest_SetPipelineDisableVertexInput(zest_pipeline_template pipeline_template);
//When using multiview to render to multiple views in a single render pass then you must set the pipeline to the
//same number of layers/views as the render that it will use
ZEST_API void zest_SetPipelineViewCount(zest_pipeline_template pipeline_template, zest_uint view_count);
//Add a descriptor layout to the pipeline template. Use this function only when setting up the pipeline before you call zest__build_pipeline
ZEST_API void zest_SetPipelineLayout(zest_pipeline_template pipeline_template, zest_pipeline_layout pipeline_layout);
//The following are helper functions to set color blend attachment states for various blending setups
//Just take a look inside the functions for the values being used
ZEST_API zest_color_blend_attachment_t zest_BlendStateNone(void);
ZEST_API zest_color_blend_attachment_t zest_AdditiveBlendState(void);
ZEST_API zest_color_blend_attachment_t zest_AdditiveBlendState2(void);
ZEST_API zest_color_blend_attachment_t zest_AlphaOnlyBlendState(void);
ZEST_API zest_color_blend_attachment_t zest_AlphaBlendState(void);
ZEST_API zest_color_blend_attachment_t zest_PreMultiplyBlendState(void);
ZEST_API zest_color_blend_attachment_t zest_PreMultiplyBlendStateForSwap(void);
ZEST_API zest_color_blend_attachment_t zest_MaxAlphaBlendState(void);
ZEST_API zest_color_blend_attachment_t zest_ImGuiBlendState(void);
ZEST_API zest_pipeline zest_GetPipeline(zest_pipeline_template pipeline_template, const zest_command_list command_list);
//Copy the zest_pipeline_template_create_info_t from an existing pipeline. This can be useful if you want to create a new pipeline based
//on an existing pipeline with just a few tweaks like setting a different shader to use.
ZEST_API zest_pipeline_template zest_CopyPipelineTemplate(const char *name, zest_pipeline_template pipeline_template);
//Delete a pipeline including the template and any cached versions of the pipeline
ZEST_API void zest_FreePipelineTemplate(zest_pipeline_template pipeline_template);
ZEST_API zest_bool zest_PipelineIsValid(zest_pipeline_template pipeline);
//Create a new pipeline layout info for creating a pipeline layout. You can use the same pipeline layout
//in multiple pipelines
ZEST_API zest_pipeline_layout_info_t zest_NewPipelineLayoutInfo(zest_device device);
ZEST_API zest_pipeline_layout_info_t zest_NewPipelineLayoutInfoWithGlobalBindless(zest_device device);
//Add a descriptor layout to the pipeline layout info
ZEST_API void zest_AddPipelineLayoutDescriptorLayout(zest_pipeline_layout_info_t *info, zest_set_layout layout);
//Set up the push contant that you might plan to use in the pipeline. Just pass in the size of the push constant struct, the offset and the shader
//stage flags where the push constant will be used. Use this if you only want to set up a single push constant range
ZEST_API void zest_SetPipelineLayoutPushConstantRange(zest_pipeline_layout_info_t *info, zest_uint size, zest_supported_shader_stages stage_flags);
//Create a pipeline layout using a zest_pipeline_layout_info_t object
ZEST_API zest_pipeline_layout zest_CreatePipelineLayout(zest_pipeline_layout_info_t *info);
//Get the pipeline layout from a pipeline
ZEST_API zest_pipeline_layout zest_GetPipelineLayout(zest_pipeline pipeline);
//-- End Pipeline related

//--End Pipeline_related_helpers

//-----------------------------------------------
//        Buffer_functions.
//        Use these functions to create memory buffers mainly on the GPU
//        but you can also create staging buffers that are cpu visible and
//        used to upload data to the GPU
//        All buffers are allocated from a memory pool. Generally each buffer type has it's own separate pool
//        depending on the memory requirements. You can define the size of memory pools before you initialise
//        Zest using zest_SetDeviceBufferPoolSize and zest_SetDeviceImagePoolsize. When you create a buffer, if
//        there is no memory left to allocate the buffer then it will try and create a new pool to allocate from.
//-----------------------------------------------
//Debug functions, will output info about the buffer to the console and also verify the integrity of host and device memory blocks
ZEST_API void zloc__output_buffer_info(void* ptr, size_t size, int free, void* user, int count);
ZEST_API zloc__error_codes zloc_VerifyRemoteBlocks(zloc_header *first_block, zloc__block_output output_function, void *user_data);
ZEST_API zloc__error_codes zloc_VerifyAllRemoteBlocks(zest_context context, zloc__block_output output_function, void *user_data);
ZEST_API zloc__error_codes zloc_VerifyBlocks(zloc_header *first_block, zloc__block_output output_function, void *user_data);
ZEST_API zest_uint zloc_CountBlocks(zloc_header *first_block);
//---------
//Create a new buffer configured with the zest_buffer_info_t that you pass into the function. 
//You can use helper functions to create commonly used buffer types such as zest_CreateVertexBufferInfo below, and you can just use
//helper functions to create the buffers without needed to create the zest_buffer_info_t, see functions just below this one.
ZEST_API zest_buffer zest_CreateBuffer(zest_device device, zest_size size, zest_buffer_info_t *buffer_info);
//Create a staging buffer which you can use to prep data for uploading to another buffer on the GPU
ZEST_API zest_buffer zest_CreateStagingBuffer(zest_device device, zest_size size, void *data);
//Generate a zest_buffer_info_t with the corresponding buffer configuration to create buffers with
ZEST_API zest_buffer_info_t zest_CreateBufferInfo(zest_buffer_type type, zest_memory_usage usage);
//Grow a buffer if the minium_bytes is more then the current buffer size.
ZEST_API zest_bool zest_GrowBuffer(zest_buffer *buffer, zest_size unit_size, zest_size minimum_bytes);
//Resize a buffer if the new size if more than the current size of the buffer. Returns true if the buffer was resized successfully.
ZEST_API zest_bool zest_ResizeBuffer(zest_buffer *buffer, zest_size new_size);
//Get the size of a buffer
ZEST_API zest_size zest_GetBufferSize(zest_buffer buffer);
//Copy data to a staging buffer
void zest_StageData(void *src_data, zest_buffer dst_staging_buffer, zest_size size);
//Free a zest_buffer and return it's memory to the pool. The freeing is deferred to the next frame.
ZEST_API void zest_FreeBuffer(zest_buffer buffer);
//Free a buffer immediately. It's up to you to ensure it's not in inuse.
ZEST_API void zest_FreeBufferNow(zest_buffer buffer);
//When creating your own draw routines you will probably need to upload data from a staging buffer to a GPU buffer like a vertex buffer. You can
//use this command with zest_cmd_UploadBuffer to upload the buffers that you need. You can call zest_AddCopyCommand multiple times depending on
//how many buffers you need to upload data for and then call zest_cmd_UploadBuffer passing the zest_buffer_uploader_t to copy all the buffers in
//one go. For examples see the builtin draw layers that do this like: zest__update_instance_layer_buffers_callback
ZEST_API void zest_AddCopyCommand(zest_context context, zest_buffer_uploader_t *uploader, zest_buffer source_buffer, zest_buffer target_buffer, zest_size size);
//For host visible buffers, you can use this to get a pointer to the mapped memory range

//Get the default pool size that is set for a specific pool hash.
//Todo: is this needed?
ZEST_API zest_buffer_pool_size_t zest_GetDevicePoolSizeKey(zest_device device, zest_key hash);
//Get the default pool size that is set for a specific combination of usage, property and image flags
ZEST_API zest_buffer_pool_size_t zest_GetDevicePoolSize(zest_device device, zest_memory_property_flags property_flags);
//Set the default pool size for a specific type of buffer set by the usage and property flags. You must call this before you call zest_Initialise
//otherwise it might not take effect on any buffers that are created during initialisation.
//Note that minimum allocation size may get overridden if it is smaller than the alignment reported by vkGetBufferMemoryRequirements at pool creation
ZEST_API void zest_SetDevicePoolSize(zest_device device, const char *name, zest_buffer_usage_t property_flags, zest_size minimum_allocation_size, zest_size pool_size);
//Create a buffer specifically for use as a uniform buffer. This will also create a descriptor set for the uniform
//buffers as well so it's ready for use in shaders.
ZEST_API zest_uniform_buffer_handle zest_CreateUniformBuffer(zest_context context, const char *name, zest_size uniform_struct_size);
//Get the pointer to a uniform buffer
ZEST_API zest_uniform_buffer zest_GetUniformBuffer(zest_uniform_buffer_handle uniform_buffer);
//Free a uniform buffer and all it's resources
ZEST_API void zest_FreeUniformBuffer(zest_uniform_buffer_handle handle);
//Get a pointer to a uniform buffer. This will return a void* which you can cast to whatever struct your storing in the uniform buffer. This will get the buffer
//with the current frame in flight index.
ZEST_API void *zest_GetUniformBufferData(zest_uniform_buffer uniform_buffer);
//Get the descriptor array index for a uniform buffer.
ZEST_API zest_uint zest_GetUniformBufferDescriptorIndex(zest_uniform_buffer uniform_buffer);
//Should only be used in zest implementations only
ZEST_API void *zest_AllocateMemory(zest_device device, zest_size size);
ZEST_API void zest_FreeMemory(zest_device device, void *allocation);
//When you free buffers the platform buffer is added to a list that is either freed at the end of the program
//or you can call this to free them whenever you want.
ZEST_API void zest_FlushUsedBuffers(zest_context context, zest_uint fif);
//Get the mapped data of a buffer. For CPU visible buffers only
ZEST_API void *zest_BufferData(zest_buffer buffer);
//Get the end of the mapped data of a buffer. For CPU visible buffers only
ZEST_API void *zest_BufferDataEnd(zest_buffer buffer);
//Get the capacity of a buffer
ZEST_API zest_size zest_BufferSize(zest_buffer buffer);
//These functions can be used to set the base size of GPU memory pools. Call these functions immediately after
//creating a device and before you actually create any buffers.
ZEST_API void zest_SetGPUBufferPoolSize(zest_device device, zest_size minimum_size, zest_size pool_size);
ZEST_API void zest_SetGPUSmallBufferPoolSize(zest_device device, zest_size minimum_size, zest_size pool_size);
ZEST_API void zest_SetGPUTransientBufferPoolSize(zest_device device, zest_size minimum_size, zest_size pool_size);
ZEST_API void zest_SetGPUSmallTransientBufferPoolSize(zest_device device, zest_size minimum_size, zest_size pool_size);
ZEST_API void zest_SetSmallHostBufferPoolSize(zest_device device, zest_size minimum_size, zest_size pool_size);
ZEST_API void zest_SetStagingBufferPoolSize(zest_device device, zest_size minimum_size, zest_size pool_size);

//--End Buffer related

//Helper functions for creating the builtin layers. these can be called separately outside of a command queue setup context
ZEST_API zest_layer_handle zest_CreateMeshLayer(zest_context context, const char *name, zest_size vertex_type_size);
ZEST_API zest_layer_handle zest_CreateInstanceMeshLayer(zest_context context, const char *name, zest_size instance_struct_size, zest_size vertex_capacity, zest_size index_capacity);

// --- Dynamic resource callbacks ---
ZEST_PRIVATE zest_image_view zest__swapchain_resource_provider(zest_context context, zest_resource_node resource);
ZEST_PRIVATE zest_buffer zest__instance_layer_resource_provider(zest_context context, zest_resource_node resource);
ZEST_PRIVATE zest_buffer zest__instance_layer_resource_provider_prev_fif(zest_context context, zest_resource_node resource);
ZEST_PRIVATE zest_buffer zest__instance_layer_resource_provider_current_fif(zest_context context, zest_resource_node resource);

// --- Frame_graph_api
// -- Creating and Executing the render graph
ZEST_API zest_bool zest_BeginFrameGraph(zest_context context, const char *name, zest_frame_graph_cache_key_t *cache_key);
ZEST_API zest_frame_graph_cache_key_t zest_InitialiseCacheKey(zest_context context, const void *user_state, zest_size user_state_size);
ZEST_API zest_frame_graph zest_EndFrameGraph();
ZEST_API zest_frame_graph zest_EndFrameGraphAndExecute();
ZEST_API zest_semaphore_status zest_WaitForSignal(zest_execution_timeline timeline, zest_microsecs timeout);

// --- Add Transient resources ---
ZEST_API zest_resource_node zest_AddTransientImageResource(const char *name, zest_image_resource_info_t *info);
ZEST_API zest_resource_node zest_AddTransientBufferResource(const char *name, const zest_buffer_resource_info_t *info);
ZEST_API zest_resource_node zest_AddTransientLayerResource(const char *name, const zest_layer layer, zest_bool prev_fif);

// --- Import external resouces into the render graph ---
ZEST_API zest_resource_node zest_ImportSwapchainResource();
ZEST_API zest_resource_node zest_ImportImageResource(const char *name, zest_image image, zest_resource_image_provider provider);
ZEST_API zest_resource_node zest_ImportBufferResource(const char *name, zest_buffer buffer, zest_resource_buffer_provider provider);

// --- Add pass nodes that execute user commands ---
ZEST_API zest_pass_node zest_BeginRenderPass(const char *name);
ZEST_API zest_pass_node zest_BeginComputePass(zest_compute compute, const char *name);
ZEST_API zest_pass_node zest_BeginTransferPass(const char *name);
ZEST_API void zest_EndPass();

// --- Add callback tasks to passes
ZEST_API void zest_SetPassTask(zest_fg_execution_callback callback, void *user_data);

// --- Utility callbacks ---
ZEST_API void zest_EmptyRenderPass(const zest_command_list command_list, void *user_data);

// --- General resource functions ---
ZEST_API zest_resource_node zest_GetPassInputResource(const zest_command_list command_list, const char *name);
ZEST_API zest_resource_node zest_GetPassOutputResource(const zest_command_list command_list, const char *name);
ZEST_API zest_buffer zest_GetPassInputBuffer(const zest_command_list command_list, const char *name);
ZEST_API zest_buffer zest_GetPassOutputBuffer(const zest_command_list command_list, const char *name);
ZEST_API zest_uint zest_GetResourceMipLevels(zest_resource_node resource);
ZEST_API zest_uint zest_GetResourceWidth(zest_resource_node resource);
ZEST_API zest_uint zest_GetResourceHeight(zest_resource_node resource);
ZEST_API void zest_SetResourceBufferSize(zest_resource_node resource, zest_size size);
ZEST_API zest_image zest_GetResourceImage(zest_resource_node resource_node);
ZEST_API zest_resource_type zest_GetResourceType(zest_resource_node resource_node);
ZEST_API zest_image_info_t zest_GetResourceImageDescription(zest_resource_node resource_node);
ZEST_API void *zest_GetResourceUserData(zest_resource_node resource_node);
ZEST_API void zest_SetResourceUserData(zest_resource_node resource_node, void *user_data);
ZEST_API void zest_SetResourceBufferProvider(zest_resource_node resource_node, zest_resource_buffer_provider buffer_provider);
ZEST_API void zest_SetResourceImageProvider(zest_resource_node resource_node, zest_resource_image_provider image_provider);
ZEST_API void zest_SetResourceClearColor(zest_resource_node resource, float red, float green, float blue, float alpha);
ZEST_API zest_frame_graph zest_GetCachedFrameGraph(zest_context context, zest_frame_graph_cache_key_t *cache_key);
ZEST_API void zest_FlushCachedFrameGraphs(zest_context context);

// --- Helper functions for acquiring bindless desriptor array indexes---
ZEST_API zest_uint zest_GetTransientSampledImageBindlessIndex(const zest_command_list command_list, zest_resource_node resource, zest_binding_number_type binding_number);
ZEST_API zest_uint *zest_GetTransientSampledMipBindlessIndexes(const zest_command_list command_list, zest_resource_node resource, zest_binding_number_type binding_number);
ZEST_API zest_uint zest_GetTransientBufferBindlessIndex(const zest_command_list command_list, zest_resource_node resource);

// --- Special flags for passes and resource
ZEST_API void zest_FlagResourceAsEssential(zest_resource_node resource);
ZEST_API void zest_DoNotCull();

// --- Render target groups ---
ZEST_API zest_resource_group zest_CreateResourceGroup();
ZEST_API void zest_AddSwapchainToGroup(zest_resource_group group);
ZEST_API void zest_AddResourceToGroup(zest_resource_group group, zest_resource_node image);

// --- Manual Barrier Functions
ZEST_API void zest_ReleaseBufferAfterUse(zest_resource_node dst_buffer);

// --- Connect Resources to Pass Nodes ---
ZEST_API void zest_ConnectInput(zest_resource_node resource);
ZEST_API void zest_ConnectOutput(zest_resource_node resource);
ZEST_API void zest_ConnectSwapChainOutput();
ZEST_API void zest_ConnectOutputGroup(zest_resource_group group);
ZEST_API void zest_ConnectInputGroup(zest_resource_group group);

// --- Connect graphs to each other
ZEST_API void zest_WaitOnTimeline(zest_execution_timeline timeline);
ZEST_API void zest_SignalTimeline(zest_execution_timeline timeline);

// --- State check functions
ZEST_API zest_bool zest_RenderGraphWasExecuted(zest_frame_graph frame_graph);

// --- Syncronization Helpers ---
ZEST_API zest_execution_timeline zest_CreateExecutionTimeline(zest_device device);
ZEST_API void zest_FreeExecutionTimeline(zest_execution_timeline timeline);

// -- General pass and resource getters/setters
ZEST_API zest_key zest_GetPassOutputKey(zest_pass_node pass);

// --- Descriptor Sets
ZEST_API void zest_SetDescriptorSets(zest_pipeline_layout layout, zest_descriptor_set *descriptor_sets, zest_uint set_count);

// -- Command list helpers
ZEST_API zest_context zest_GetContext(zest_command_list command_list);

// --- Render graph debug functions ---
ZEST_API zest_frame_graph_result zest_GetFrameGraphResult(zest_frame_graph frame_graph);
ZEST_API zest_uint zest_GetFrameGraphFinalPassCount(zest_frame_graph frame_graph);
ZEST_API zest_uint zest_GetFrameGraphPassTransientCreateCount(zest_frame_graph frame_graph, zest_key output_key);
ZEST_API zest_uint zest_GetFrameGraphPassTransientFreeCount(zest_frame_graph frame_graph, zest_key output_key);
ZEST_API zest_uint zest_GetFrameGraphCulledResourceCount(zest_frame_graph frame_graph);
ZEST_API zest_uint zest_GetFrameGraphCulledPassesCount(zest_frame_graph frame_graph);
ZEST_API zest_uint zest_GetFrameGraphSubmissionCount(zest_frame_graph frame_graph);
ZEST_API zest_uint zest_GetFrameGraphSubmissionBatchCount(zest_frame_graph frame_graph, zest_uint submission_index);
ZEST_API zest_uint zest_GetSubmissionBatchPassCount(const zest_submission_batch_t *batch);
ZEST_API const zest_submission_batch_t *zest_GetFrameGraphSubmissionBatch(zest_frame_graph frame_graph, zest_uint submission_index, zest_uint batch_index);
ZEST_API const zest_pass_group_t *zest_GetFrameGraphFinalPass(zest_frame_graph frame_graph, zest_uint pass_index);
ZEST_API zest_size *zest_GetFrameGraphSize(zest_frame_graph frame_graph);
ZEST_API void zest_PrintCompiledFrameGraph(zest_frame_graph frame_graph);
ZEST_API void zest_PrintCachedFrameGraph(zest_context context, zest_frame_graph_cache_key_t *cache_key);

// --- [Swapchain_helpers]
ZEST_API zest_swapchain zest_GetSwapchain(zest_context context);
ZEST_API zest_format zest_GetSwapchainFormat(zest_swapchain swapchain);
ZEST_API void zest_SetSwapchainClearColor(zest_context context, float red, float green, float blue, float alpha);
//End Swapchain helpers
// --- End Frame_graph_api

//-----------------------------------------------
//        General_Math_helper_functions
//-----------------------------------------------

//Subtract right from left vector and return the result
ZEST_API zest_vec2 zest_SubVec2(zest_vec2 left, zest_vec2 right);
ZEST_API zest_vec3 zest_SubVec3(zest_vec3 left, zest_vec3 right);
ZEST_API zest_vec4 zest_SubVec4(zest_vec4 left, zest_vec4 right);
ZEST_API zest_vec2 zest_AddVec2(zest_vec2 left, zest_vec2 right);
ZEST_API zest_vec3 zest_AddVec3(zest_vec3 left, zest_vec3 right);
ZEST_API zest_vec4 zest_AddVec4(zest_vec4 left, zest_vec4 right);
ZEST_API zest_ivec2 zest_AddiVec2(zest_ivec2 left, zest_ivec2 right);
ZEST_API zest_ivec3 zest_AddiVec3(zest_ivec3 left, zest_ivec3 right);
//Scale a vector by a scalar and return the result
ZEST_API zest_vec2 zest_ScaleVec2(zest_vec2 vec2, float v);
ZEST_API zest_vec3 zest_ScaleVec3(zest_vec3 vec3, float v);
ZEST_API zest_vec4 zest_ScaleVec4(zest_vec4 vec4, float v);
//Multiply 2 vectors and return the result
ZEST_API zest_vec3 zest_MulVec3(zest_vec3 left, zest_vec3 right);
ZEST_API zest_vec4 zest_MulVec4(zest_vec4 left, zest_vec4 right);
//Divide 2 vectors and return the result
ZEST_API zest_vec3 zest_DivVec3(zest_vec3 left, zest_vec3 right);
ZEST_API zest_vec4 zest_DivVec4(zest_vec4 left, zest_vec4 right);
//Get the length of a vec without square rooting
ZEST_API float zest_LengthVec3NS(zest_vec3 const v);
ZEST_API float zest_LengthVec4NS(zest_vec4 const v);
ZEST_API float zest_LengthVec2NS(zest_vec2 const v);
//Get the length of a vec
ZEST_API float zest_LengthVec3(zest_vec3 const v);
ZEST_API float zest_LengthVec2(zest_vec2 const v);
//Normalise vectors
ZEST_API zest_vec2 zest_NormalizeVec2(zest_vec2 const v);
ZEST_API zest_vec3 zest_NormalizeVec3(zest_vec3 const v);
ZEST_API zest_vec4 zest_NormalizeVec4(zest_vec4 const v);
//Transform a vector by a 4x4 matrix
ZEST_API zest_vec4 zest_MatrixTransformVector(zest_matrix4 *mat, zest_vec4 vec);
//Transpose a matrix, switch columns to rows and vice versa
zest_matrix4 zest_TransposeMatrix4(zest_matrix4 *mat);
//Transform 2 matrix 4s
ZEST_API zest_matrix4 zest_MatrixTransform(zest_matrix4 *in, zest_matrix4 *m);
//Get the inverse of a 4x4 matrix
ZEST_API zest_matrix4 zest_Inverse(zest_matrix4 *m);
//Create a 4x4 matrix with rotation, position and scale applied
ZEST_API zest_matrix4 zest_CreateMatrix4(float pitch, float yaw, float roll, float x, float y, float z, float sx, float sy, float sz);
//Create a rotation matrix on the x axis
ZEST_API zest_matrix4 zest_Matrix4RotateX(float angle);
//Create a rotation matrix on the y axis
ZEST_API zest_matrix4 zest_Matrix4RotateY(float angle);
//Create a rotation matrix on the z axis
ZEST_API zest_matrix4 zest_Matrix4RotateZ(float angle);
//Get the cross product between 2 vec3s
ZEST_API zest_vec3 zest_CrossProduct(const zest_vec3 x, const zest_vec3 y);
//Get the dot product between 2 vec3s
ZEST_API float zest_DotProduct3(const zest_vec3 a, const zest_vec3 b);
//Get the dot product between 2 vec4s
ZEST_API float zest_DotProduct4(const zest_vec4 a, const zest_vec4 b);
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
ZEST_API zest_uint zest_Pack16bit2SNorm(float x, float y);
ZEST_API zest_u64 zest_Pack16bit4SNorm(float x, float y, float z, float w);
//Convert 4 32bit floats into packed 16bit scaled floats. You can pass 2 max_values if you need to scale xy/zw separately
ZEST_API zest_uint zest_Pack16bit2SScaled(float x, float y, float max_value);
//Convert a 32bit float to a 16bit float packed into a 16bit uint
ZEST_API zest_uint zest_FloatToHalf(float f);
//Convert 4 32bit floats into packed 16bit floats
ZEST_API zest_u64 zest_Pack16bit4SFloat(float x, float y, float z, float w);
//Convert 4 32bit floats into packed 16bit scaled floats. You can pass 2 max_values if you need to scale xy/zw separately
ZEST_API zest_u64 zest_Pack16bit4SScaled(float x, float y, float z, float w, float max_value_xy, float max_value_zw);
//Convert 2 32bit floats into packed 16bit scaled floats and pass zw as a prepacked value. 
ZEST_API zest_u64 zest_Pack16bit4SScaledZWPacked(float x, float y, zest_uint zw, float max_value_xy);
ZEST_API zest_uint zest_Pack16bitStretch(float x, float y);
//Pack 3 floats into an unsigned int
ZEST_API zest_uint zest_Pack8bit(float x, float y, float z);
//Convert degrees into radians
ZEST_API float zest_Radians(float degrees);
//Convert radians into degrees
ZEST_API float zest_Degrees(float radians);
//Interpolate 2 vec4s and return the result
ZEST_API zest_vec4 zest_LerpVec4(zest_vec4 *from, zest_vec4 *to, float lerp);
//Interpolate 2 vec3s and return the result
ZEST_API zest_vec3 zest_LerpVec3(zest_vec3 *from, zest_vec3 *to, float lerp);
//Interpolate 2 vec2s and return the result
ZEST_API zest_vec2 zest_LerpVec2(zest_vec2 *from, zest_vec2 *to, float lerp);
//Interpolate 2 floats and return the result
ZEST_API float zest_Lerp(float from, float to, float lerp);
//Initialise a new 4x4 matrix
ZEST_API zest_matrix4 zest_M4(float v);
//Flip the signs on a vec3 and return the result
ZEST_API zest_vec3 zest_FlipVec3(zest_vec3 vec3);
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
ZEST_API zest_color_t zest_ColorSet(zest_byte r, zest_byte g, zest_byte b, zest_byte a);
//Set the color with a single value
ZEST_API zest_color_t zest_ColorSet1(zest_byte c);
//--End General Math


//-----------------------------------------------
//        Camera_helpers
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
ZEST_API void zest_CameraPosition(zest_camera_t *camera, float position[3]);
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
ZEST_API zest_vec2 zest_WorldToScreen(const float point[3], float view_width, float view_height, zest_matrix4* projection, zest_matrix4* view);
//Convert world orthographic 3d coordinates into screen x and y coordinates
ZEST_API zest_vec2 zest_WorldToScreenOrtho(const float point[3], float view_width, float view_height, zest_matrix4* projection, zest_matrix4* view);
//Create a perspective 4x4 matrix passing in the fov in radians, aspect ratio of the viewport and te near/far values
ZEST_API zest_matrix4 zest_Perspective(float fovy, float aspect, float zNear, float zFar);
//Calculate the 6 planes of the camera fustrum
ZEST_API void zest_CalculateFrustumPlanes(zest_matrix4 *view_matrix, zest_matrix4 *proj_matrix, zest_vec4 planes[6]);
//Take the 6 planes of a camera fustrum and determine if a point is inside that fustrum
ZEST_API zest_bool zest_IsPointInFrustum(const zest_vec4 planes[6], const float point[3]);
//Take the 6 planes of a camera fustrum and determine if a sphere is inside that fustrum
ZEST_API zest_bool zest_IsSphereInFrustum(const zest_vec4 planes[6], const float point[3], float radius);
//-- End camera and other helpers

//-----------------------------------------------
//        Images_and_textures
//        Images are for storing textures on the GPU to be read or written to depending on what you need
//-----------------------------------------------
//Create a new image info struct which you use to create a new image. Change the members of the info
//struct to create the create that you need.
ZEST_API zest_image_info_t zest_CreateImageInfo(zest_uint width, zest_uint height);
ZEST_API zest_image_view_create_info_t zest_CreateViewImageInfo(zest_image image);
ZEST_API zest_image_handle zest_CreateImage(zest_device device, zest_image_info_t *create_info);
ZEST_API zest_image_handle zest_CreateImageWithPixels(zest_device device, void *pixels, zest_size size, zest_image_info_t *create_info);
ZEST_API zest_image zest_GetImage(zest_image_handle handle);
ZEST_API void zest_FreeImage(zest_image_handle image_handle);
ZEST_API void zest_FreeImageNow(zest_image_handle image_handle);
ZEST_API zest_image_view_handle zest_CreateImageView(zest_device device, zest_image image, zest_image_view_create_info_t *create_info);
ZEST_API zest_image_view_array_handle zest_CreateImageViewsPerMip(zest_device device, zest_image image);
ZEST_API zest_image_view zest_GetImageView(zest_image_view_handle handle);
ZEST_API zest_image_view_array zest_GetImageViewArray(zest_image_view_array_handle handle);
ZEST_API void zest_FreeImageView(zest_image_view_handle view_handle);
ZEST_API void zest_FreeImageViewNow(zest_image_view_handle view_handle);
ZEST_API void zest_FreeImageViewArray(zest_image_view_array_handle view_handle);
ZEST_API void zest_FreeImageViewArrayNow(zest_image_view_array_handle view_handle);
ZEST_API void zest_GetFormatPixelData(zest_format format, int *channels, int *bytes_per_pixel, int *block_width, int *block_height, int *bytes_per_block);
ZEST_API zest_bool zest_CopyBitmapToImage(zest_device device, void *bitmap, zest_size image_size, zest_image dst_image, zest_uint width, zest_uint height);
//Get the extent of the image
ZEST_API const zest_image_info_t *zest_ImageInfo(zest_image image);
//Get a descriptor index from an image for a specific binding number. You must have already acquired the index
//for the binding number first with zest_GetGlobalSampler.
ZEST_API zest_uint zest_ImageDescriptorIndex(zest_image image, zest_binding_number_type binding_number);
//Get the raw value of the current image layout of the image as an int. This will be whatever layout is represented
//in the backend api you are using (vulkan, metal, dx12 etc.). You can use this if you just need the current layout
//of the image without translating it into a zest layout enum because you just need to know if the layout changed
//for frame graph caching purposes etc.
ZEST_API int zest_ImageRawLayout(zest_image image);
ZEST_API zest_extent2d_t zest_RegionDimensions(zest_atlas_region_t *region);
ZEST_API zest_extent2d_t zest_RegionDimensions(zest_atlas_region_t *region);
ZEST_API zest_uint zest_RegionLayerIndex(zest_atlas_region_t *region);
ZEST_API zest_vec4 zest_RegionUV(zest_atlas_region_t *region);
ZEST_API void zest_BindAtlasRegionToImage(zest_atlas_region_t *region, zest_uint sampler_index, zest_image image, zest_binding_number_type binding_number);
//-- End Images and textures

// --Sampler_functions
//Gets a sampler from the sampler storage in the renderer. If no match is found for the info that you pass into the sampler
//then a new one will be created.
ZEST_API zest_sampler_handle zest_CreateSampler(zest_context context, zest_sampler_info_t *info);
ZEST_API zest_sampler_info_t zest_CreateSamplerInfo();
ZEST_API zest_sampler_info_t zest_CreateSamplerInfoRepeat();
ZEST_API zest_sampler_info_t zest_CreateSamplerInfoMirrorRepeat();
ZEST_API zest_sampler zest_GetSampler(zest_sampler_handle handle);
ZEST_API void zest_FreeSampler(zest_sampler_handle handle);
ZEST_API void zest_FreeSamplerNow(zest_sampler_handle handle);
// --End Sampler_functions

//-----------------------------------------------
//-- Draw_Layers_API
//-----------------------------------------------

//Create a new layer for instanced drawing. This just creates a standard layer with default options and callbacks, all
//you need to pass in is the size of type used for the instance struct that you'll use with whatever pipeline you setup
//to use with the layer.
ZEST_API zest_layer_handle zest_CreateInstanceLayer(zest_context context, const char* name, zest_size type_size, zest_uint max_instances);
//Get an opaque pointer to a layer that you can pass in to all of the layer functions. You can do this once for
//each frame and then use the pointer for all the proceeding functions. This saves having to look up the resource
//every function call
ZEST_API zest_layer zest_GetLayer(zest_layer_handle layer_handle);
//Creates a layer with buffers for each frame in flight located on the device. This means that you can manually decide
//When to upload to the buffer on the render graph rather then using transient buffers each frame that will be
//discarded. In order to avoid syncing issues on the GPU, pass a unique id to generate a unique Buffer. This
//id can be shared with any other frame in flight layer that will flip their frame in flight index at the same
//time, like when ever the update loop is run.
ZEST_API zest_layer_handle zest_CreateFIFInstanceLayer(zest_context context, const char *name, zest_size type_size, zest_uint max_instances);
//Start a new set of draw instructs for a standard zest_layer. These were internal functions but they've been made api functions for making you're own custom
//instance layers more easily
ZEST_API void zest_StartInstanceInstructions(zest_layer layer);
//Set the layer frame in flight to the next layer. Use this if you're manually setting the current fif for the layer so
//that you can avoid uploading the staging buffers every frame and only do so when it's neccessary.
ZEST_API void zest_ResetLayer(zest_layer layer);
//Same as ResetLayer but specifically for an instance layer
ZEST_API void zest_ResetInstanceLayer(zest_layer layer);
//End a set of draw instructs for a standard zest_layer
ZEST_API void zest_EndInstanceInstructions(zest_layer layer);
//Callback that can be used to upload layer data to the gpu
ZEST_API void zest_UploadInstanceLayerData(const zest_command_list command_list, void *user_data);
//For layers that are manually flipping the frame in flight, we can use this to only end the instructions if the last know fif for the layer
//is not equal to the current one. Returns true if the instructions were ended false if not. If true then you can assume that the staging
//buffer for the layer can then be uploaded to the gpu. This should be called in an upload buffer callback in any custom draw routine/layer.
ZEST_API zest_bool zest_MaybeEndInstanceInstructions(zest_layer layer);
//Get the current size in bytes of all instances being drawn in the layer
ZEST_API zest_size zest_GetLayerInstanceSize(zest_layer layer);
//Get the current amount of index memory used by the layer
ZEST_API zest_size zest_GetLayerIndexMemoryInUse(zest_layer layer);
//Get the current amount of vertex memory used by the layer
ZEST_API zest_size zest_GetLayerVertexMemoryInUse(zest_layer layer);
//Reset the drawing for an instance layer. This is called after all drawing is done and dispatched to the gpu
ZEST_API void zest_ResetInstanceLayerDrawing(zest_layer layer);
//Get the current amount of instances being drawn in the layer
ZEST_API zest_uint zest_GetInstanceLayerCount(zest_layer layer);
//Move the pointer in memory to the next instance to write to.
ZEST_API void *zest_NextInstance(zest_layer layer);
//Free a layer and all it's resources
ZEST_API void zest_FreeLayer(zest_layer_handle layer);
//Set the viewport of a layer. This is important to set right as when the layer is drawn it needs to be clipped correctly and in a lot of cases match how the
//uniform buffer is setup
ZEST_API void zest_SetLayerViewPort(zest_layer layer, int x, int y, zest_uint scissor_width, zest_uint scissor_height, float viewport_width, float viewport_height);
ZEST_API void zest_SetLayerScissor(zest_layer layer, int offset_x, int offset_y, zest_uint scissor_width, zest_uint scissor_height);
//Update the layer viewport to match the swapchain
ZEST_API void zest_SetLayerSizeToSwapchain(zest_layer layer);
//Set the size of the layer. Called internally to set it to the window size. Can this be internal?
ZEST_API void zest_SetLayerSize(zest_layer layer, float width, float height);
//Set the layer color. This is used to apply color to the sprite/font/billboard that you're drawing or you can use it in your own draw routines that use zest_layers.
//Note that the alpha value here is actually a transition between additive and alpha blending. Where 0 is the most additive and 1 is the most alpha blending. Anything
//imbetween is a combination of the 2.
ZEST_API void zest_SetLayerColor(zest_layer layer, zest_byte red, zest_byte green, zest_byte blue, zest_byte alpha);
ZEST_API void zest_SetLayerColorf(zest_layer layer, float red, float green, float blue, float alpha);
//Set the intensity of the layer. This is used to apply an alpha value to the sprite or billboard you're drawing or you can use it in your own draw routines that
//use zest_layers. Note that intensity levels can exceed 1.f to make your sprites extra bright because of pre-multiplied blending in the sprite.
ZEST_API void zest_SetLayerIntensity(zest_layer layer, float value);
//Set the user data of a layer. You can use this to extend the functionality of the layers for your own needs.
ZEST_API void zest_SetLayerUserData(zest_layer layer, void *data);
ZEST_API void *zest_GetLayerUserData(zest_layer layer);
ZEST_API zest_uint zest_GetLayerVertexDescriptorIndex(zest_layer layer, zest_bool last_frame);
ZEST_API zest_buffer zest_GetLayerResourceBuffer(zest_layer layer);
ZEST_API zest_buffer zest_GetLayerVertexBuffer(zest_layer layer);
ZEST_API zest_buffer zest_GetLayerStagingVertexBuffer(zest_layer layer);
ZEST_API zest_buffer zest_GetLayerStagingIndexBuffer(zest_layer layer);
ZEST_API const zest_mesh_offset_data_t *zest_GetLayerMeshOffsets(zest_layer layer, zest_uint mesh_index);
ZEST_API void zest_UploadLayerStagingData(zest_layer layer, const zest_command_list command_list);
ZEST_API void zest_DrawInstanceLayer(const zest_command_list command_list, void *user_data);
ZEST_API zest_layer_instruction_t *zest_NextLayerInstruction(zest_layer layer);
ZEST_API zest_uint zest_GetLayerInstructionCount(zest_layer layer);
ZEST_API const zest_layer_instruction_t *zest_GetLayerInstruction(zest_layer layer, zest_uint index);
ZEST_API zest_uint zest_GetLayerInstructionCount(zest_layer layer);
ZEST_API zest_scissor_rect_t zest_GetLayerScissor(zest_layer layer);
ZEST_API zest_viewport_t zest_GetLayerViewport(zest_layer layer);
//-- End Draw Layers


//-----------------------------------------------
//        Draw_instance_layers
//        General purpose functions you can use to either draw built in instances like instanced meshes and billboards
//-----------------------------------------------
//Set the texture, descriptor set shader_resources and pipeline for any calls to the same layer that come after it. You must call this function if you want to do any instance drawing for a particular layer, and you
//must call it again if you wish to switch either the texture, descriptor set or pipeline to do the drawing. Everytime you call this function it creates a new draw instruction
//in the layer for drawing instances so each call represents a separate draw call in the render. So if you just call this function once you can call a draw instance function as many times
//as you want (like zest_DrawBillboard or your own custom draw instance function) and they will all be drawn with a single draw call.
//Pass in the zest_layer, zest_texture, zest_descriptor_set and zest_pipeline. A few things to note:
//1) The descriptor layout used to create the descriptor sets in the shader_resources must match the layout used in the pipeline.
//2) You can pass 0 in the descriptor set and it will just use the default descriptor set used in the texture.
ZEST_API void zest_StartInstanceDrawing(zest_layer layer, zest_pipeline_template pipeline);
//Draw all the contents in a buffer. You can use this if you prepare all the instance data elsewhere in your code and then want
//to just dump it all into the staging buffer of the layer in one go. This will move the instance pointer in the layer to the next point
//in the buffer as well as bump up the instance count by the amount you pass into the function. The instance buffer will be grown if
//there is not enough room.
//Note that the struct size of the data you're copying MUST be the same size as the layer->struct_size.
ZEST_API zest_draw_buffer_result zest_DrawInstanceBuffer(zest_layer layer, void *src, zest_uint amount);
//In situations where you write directly to a staging buffer you can use this function to simply tell the draw instruction
//how many instances should be drawn. You will still need to call zest_StartInstanceDrawing
ZEST_API void zest_DrawInstanceInstruction(zest_layer layer, zest_uint amount);
//Set the viewport and scissors of the next draw instructions for a layer. Otherwise by default it will use either the screen size
//of or the viewport size you set with zest_SetLayerViewPort
ZEST_API void zest_SetLayerDrawingViewport(zest_layer layer, int x, int y, zest_uint scissor_width, zest_uint scissor_height, float viewport_width, float viewport_height);
//Set the current instruction push contants in the layer. Ensure that this is called *after* a call to
//zest_SetInstanceLayer or zest_SetInstanceMeshLayer
ZEST_API void zest_SetLayerPushConstants(zest_layer layer, void *push_constants, zest_size size);
//Get the current push constants in the layer for the current instruction
ZEST_API void *zest_GetLayerPushConstants(zest_layer layer);
//Get the current push constants in the layer for the current instruction
ZEST_API int zest_GetLayerFrameInFlight(zest_layer layer);


//-----------------------------------------------
//        Draw_mesh_layers
//        Mesh layers let you upload a vertex and index buffer to draw meshes. I set this up primarily for
//        use with Dear ImGui
//-----------------------------------------------
ZEST_API zest_buffer zest_GetVertexWriteBuffer(zest_layer layer);
//Get the index staging buffer. You'll need to get the staging buffers to copy your mesh data to or even just record mesh data directly to the staging buffer
ZEST_API zest_buffer zest_GetIndexWriteBuffer(zest_layer layer);
//Grow the mesh vertex buffers. You must update the buffer->memory_in_use so that it can decide if a buffer needs growing
ZEST_API void zest_GrowMeshVertexBuffers(zest_layer layer);
//Grow the mesh index buffers. You must update the buffer->memory_in_use so that it can decide if a buffer needs growing
ZEST_API void zest_GrowMeshIndexBuffers(zest_layer layer);
//Set the mesh drawing specifying any texture, descriptor set and pipeline that you want to use for the drawing
ZEST_API void zest_SetMeshDrawing(zest_layer layer, zest_pipeline_template pipeline);
//Helper funciton Push an index to the index staging buffer. It will automatically grow the buffers if needed
ZEST_API void zest_PushIndex(zest_layer layer, zest_uint offset);
//Callback for the frame graph
ZEST_API void zest_DrawInstanceMeshLayer(const zest_command_list command_list, void *user_data);
//Draw an instance mesh layer with a specific pipeline. So every instruction in the layer will be draw with the 
//pipeline that you pass in to the layer.
ZEST_API void zest_DrawInstanceMeshLayerWithPipeline(const zest_command_list command_list, zest_layer layer, zest_pipeline_template pipeline);

//-----------------------------------------------
//        Draw_instance_mesh_layers
//        Instance mesh layers are for creating meshes and then drawing instances of them. 
//        Very basic stuff currently, I'm just using them to create 3d widgets I can use in TimelineFX
//        but this can all be expanded on for general 3d models in the future.
//-----------------------------------------------
ZEST_API void zest_StartInstanceMeshDrawing(zest_layer layer, zest_uint mesh_index, zest_pipeline_template pipeline);
//Push an index to a mesh to build triangles
ZEST_API void zest_PushMeshIndex(zest_mesh mesh, zest_uint index);
//Rather then PushMeshIndex you can call this to add three indexes at once to build a triangle in the mesh
ZEST_API void zest_PushMeshTriangle(zest_mesh mesh, zest_uint i1, zest_uint i2, zest_uint i3);
//Create a new mesh and return the handle to it
ZEST_API zest_mesh zest_NewMesh(zest_context context, zest_size vertex_struct_size);
//Free the memeory used for the mesh. You can free the mesh once it's been added to the layer
ZEST_API void zest_FreeMesh(zest_mesh mesh);
//Get a pointer to a vertex at the specified index
ZEST_API void* zest_GetMeshVertex(zest_mesh mesh, zest_uint index);
//Get the next vertex from the current vertex pointer. Returns NULL if at the end.
ZEST_API void* zest_NextMeshVertex(zest_mesh mesh, void* current);
//Push vertex data to the mesh. Copies vertex_struct_size bytes from the provided pointer.
ZEST_API void zest_PushMeshVertexData(zest_mesh mesh, const void* vertex_data);
//Copy vertex data to the mesh. All vertex data in the mesh will be overwritten.
ZEST_API void zest_CopyMeshVertexData(zest_mesh mesh, const void* vertex_data, zest_size size);
//Copy index data to the mesh. All index data in the mesh will be overwritten.
ZEST_API void zest_CopyMeshIndexData(zest_mesh mesh, const zest_uint *index_data, zest_uint count);
//Reserve capacity for vertices in the mesh
ZEST_API void zest_ReserveMeshVertices(zest_mesh mesh, zest_uint count);
//Clear all vertices from the mesh (keeps capacity)
ZEST_API void zest_ClearMeshVertices(zest_mesh mesh);
//Initialise a new bounding box to 0
ZEST_API zest_bounding_box_t zest_NewBoundingBox();
//Add a mesh to an instanced mesh layer. Existing vertex data in the layer will be deleted.
ZEST_API zest_uint zest_AddMeshToLayer(zest_layer layer, zest_mesh src_mesh, zest_uint texture_index);
//Get the vertex count in the mesh
ZEST_API zest_uint zest_MeshVertexCount(zest_mesh mesh);
//Get the index count in the mesh
ZEST_API zest_uint zest_MeshIndexCount(zest_mesh mesh);
//Get the size in bytes for the vertex data in a mesh
ZEST_API zest_size zest_MeshVertexDataSize(zest_mesh mesh);
//Get the size in bytes for the index data in a mesh
ZEST_API zest_size zest_MeshIndexDataSize(zest_mesh mesh);
//Pass in an array of meshes and have it calculate the total vertex and index capacity of all the meshes in
//the array.
ZEST_API void zest_MeshDataSizes(zest_mesh *meshes, zest_uint mesh_count, zest_size *vertex_capacity, zest_size *index_capacity);
//Get a pointer to the index mesh data. You can use this as a source data for copying to a staging buffer
ZEST_API zest_uint *zest_MeshIndexData(zest_mesh mesh);
//Get a pointer to the vertex mesh data. You can use this as a source data for copying to a staging buffer
ZEST_API void *zest_MeshVertexData(zest_mesh mesh);
ZEST_API zest_uint zest_GetLayerMeshTextureIndex(zest_layer layer, zest_uint mesh_index);
//--End Instance Draw mesh layers

//-----------------------------------------------
//        Compute_shaders
//        Build custom compute shaders and integrate into a command queue or just run separately as you need.
//        See zest-compute-example for a full working example
//-----------------------------------------------
//Create a blank ready-to-build compute object and store by name in the renderer.
ZEST_PRIVATE zest_compute zest__new_compute(zest_device device, const char *name);
//Once you have finished calling the builder commands you will need to call this to actually build the compute shader. Pass a pointer to the builder and the zest_compute
//handle that you got from calling zest__new_compute. You can then use this handle to add the compute shader to a command queue with zest_NewComputeSetup in a
//command queue context (see the section on Command queue setup and creation)
ZEST_API zest_compute_handle zest_CreateCompute(zest_device device, const char *name, zest_shader_handle shader);
//Get a compute pointer from a handle that you can use to pass in to functions. If the the compute resource
//is freed then the pointer is no longer valid.
ZEST_API zest_compute zest_GetCompute(zest_compute_handle compute_handle);
//Free a compute shader and all its resources
ZEST_API void zest_FreeCompute(zest_compute_handle compute);
//--End Compute shaders

//-----------------------------------------------
//        Events_and_States
//-----------------------------------------------
//Returns true if the swap chain was recreated last frame. The swap chain will mainly be recreated if the window size changes
ZEST_API zest_bool zest_SwapchainWasRecreated(zest_context context);
//--End Events and States

//-----------------------------------------------
//        Timer_functions
//        This is a simple API for a high resolution timer. You can use this to implement fixed step
//        updating for your logic in the main loop, plus for anything else that you need to time.
//-----------------------------------------------
ZEST_API zest_timer_t zest_CreateTimer(double update_frequency);                                  //Create a new timer and return its handle
ZEST_API void zest_TimerSetUpdateFrequency(zest_timer_t *timer, double update_frequency);          //Set the update frequency for timing loop functions, accumulators and such
ZEST_API void zest_TimerSetMaxFrames(zest_timer_t *timer, double frames);                          //Set the maximum amount of frames that can pass each update. This helps avoid simulations blowing up
ZEST_API void zest_TimerReset(zest_timer_t *timer);                                                //Set the clock time to now
ZEST_API double zest_TimerDeltaTime(zest_timer_t *timer);                                          //The amount of time passed since the last tick
ZEST_API void zest_TimerTick(zest_timer_t *timer);                                                 //Update the delta time
ZEST_API double zest_TimerUpdateTime(zest_timer_t *timer);                                         //Gets the update time (1.f / update_frequency)
ZEST_API double zest_TimerFrameLength(zest_timer_t *timer);                                        //Gets the update_tick_length (1000.f / update_frequency)
ZEST_API double zest_TimerAccumulate(zest_timer_t *timer);                                         //Accumulate the amount of time that has passed since the last render
ZEST_API int zest_TimerPendingTicks(zest_timer_t *timer);                                          //Returns the number of times the update loop will run this frame.
ZEST_API void zest_TimerUnAccumulate(zest_timer_t *timer);                                         //Unaccumulate 1 tick length from the accumulator. While the accumulator is more then the tick length an update should be done
ZEST_API zest_bool zest_TimerDoUpdate(zest_timer_t *timer);                                        //Return true if accumulator is more or equal to the update_tick_length
ZEST_API double zest_TimerLerp(zest_timer_t *timer);                                               //Return the current tween/lerp value
ZEST_API void zest_TimerSet(zest_timer_t *timer);                                                  //Set the current tween value
ZEST_API double zest_TimerUpdateFrequency(zest_timer_t *timer);                                    //Get the update frequency set in the timer
ZEST_API zest_bool zest_TimerUpdateWasRun(zest_timer_t *timer);                                    //Returns true if an update loop was run
//Help macros for starting/ending an update loop if you prefer.
#define zest_StartTimerLoop(timer) 	zest_TimerAccumulate(&timer); \
int pending_ticks = zest_TimerPendingTicks(&timer); \
while (zest_TimerDoUpdate(&timer)) {

#define zest_EndTimerLoop(timer) zest_TimerUnAccumulate(&timer); \
} \
zest_TimerSet(&timer);
//--End Timer Functions

//-----------------------------------------------
//        General_Helper_functions
//-----------------------------------------------
//Read a file from disk into memory. Set terminate to 1 if you want to add \0 to the end of the file in memory
ZEST_API zest_file zest_ReadEntireFile(zest_device device, const char *file_name, zest_bool terminate);
//Free the data from a file
ZEST_API void zest_FreeFile(zest_device device, zest_file file);
//Get the swap chain extent which will basically be the size of the window returned in a zest_extent2d_t struct.
ZEST_API zest_extent2d_t zest_GetSwapChainExtent(zest_context context);
//Get the window size in a zest_extent2d_t. In most cases this is the same as the swap chain extent.
ZEST_API zest_extent2d_t zest_GetWindowExtent(zest_context context);
//Get the current swap chain width
ZEST_API zest_uint zest_ScreenWidth(zest_context context);
//Get the current swap chain height
ZEST_API zest_uint zest_ScreenHeight(zest_context context);
//Get the current swap chain width as a float
ZEST_API float zest_ScreenWidthf(zest_context context);
//Get the current swap chain height as a float
ZEST_API float zest_ScreenHeightf(zest_context context);
//Get the native window pointer for the context
ZEST_API void *zest_NativeWindow(zest_context context);
//Get the window pointer for the context that represents a pointer to the 3rd party window handle like GLFW or SDL
ZEST_API void *zest_Window(zest_context context);
//For retina screens this will return the current screen DPI
ZEST_API float zest_DPIScale(zest_context context);
//Set the DPI scale
ZEST_API void zest_SetDPIScale(zest_context context, float scale);
//Get the current frame in flight for a context
ZEST_API zest_uint zest_CurrentFIF(zest_context context);
//Wait for the device to be idle (finish executing all commands). Only recommended if you need to do a one-off operation like change a texture that could
//still be in use by the GPU
ZEST_API void zest_WaitForIdleDevice(zest_device device);
//Enable vsync so that the frame rate is limited to the current monitor refresh rate. Will cause the swap chain to be rebuilt.
ZEST_API void zest_EnableVSync(zest_context context);
//Disable vsync so that the frame rate is not limited to the current monitor refresh rate, frames will render as fast as they can. Will cause the swap chain to be rebuilt.
ZEST_API void zest_DisableVSync(zest_context context);
//Set the current frame in flight. You should only use this if you want to execute a floating command queue that contains resources that only have a single frame in flight.
//Once you're done you can call zest_RestoreFrameInFlight
ZEST_API void zest_SetFrameInFlight(zest_context context, zest_uint fif);
//Only use after a call to zest_SetFrameInFlight. Will restore the frame in flight that was in use before the call to zest_SetFrameInFlight.
ZEST_API void zest_RestoreFrameInFlight(zest_context context);
//Convert a linear color value to an srgb color space value
ZEST_API float zest_LinearToSRGB(float value);
//Get memory stats for device and contexts
ZEST_API zloc_allocation_stats_t zest_GetDeviceMemoryStats(zest_device device);
ZEST_API zloc_allocation_stats_t zest_GetContextMemoryStats(zest_context context);
//--End General Helper functions

//-----------------------------------------------
//Debug_Helpers
//-----------------------------------------------
//About all of the current memory usage that you're using in the renderer to the console. This won't include any allocations you've done elsewhere using you own allocation
//functions or malloc/virtual_alloc etc.
ZEST_API void zest_OutputMemoryUsage(zest_context context);
//Set the path to the log where validation messages and other log messages can be output to. If this is not set and ZEST_VALIDATION_LAYER is 1 then validation messages and other log
//messages will be output to the console if it's available.
ZEST_API zest_bool zest_SetErrorLogPath(zest_device device, const char *path);
ZEST_API const char *zest_GetErrorLogPath(zest_device device);
//Print out any reports that have been collected to the console
ZEST_API zest_uint zest_ReportCount(zest_context context);
ZEST_API void zest_PrintReports(zest_context context);
ZEST_API void zest_ResetReports(zest_context context);
ZEST_PRIVATE void zest__print_block_info(zloc_allocator *allocator, void *allocation, zloc_header *current_block, zest_platform_memory_context context_filter, zest_platform_command command_filter);
ZEST_API void zest_PrintMemoryBlocks(zloc_allocator *allocator, zloc_header *first_block, zest_bool output_all, zest_platform_memory_context context_filter, zest_platform_command command_filter);
ZEST_API zest_uint zest_GetValidationErrorCount(zest_context context);
ZEST_API void zest_ResetValidationErrors(zest_device device);
//--End Debug Helpers

//Helper functions for executing commands on the GPU immediately
//For now this just handles buffer/image copying but will expand this as we go.
ZEST_API zest_queue zest_imm_BeginCommandBuffer(zest_device device, zest_device_queue_type target_queue);
//End the immediate command buffer, submit and wait for completion (blocking). Queue is released immediately after
ZEST_API zest_bool zest_imm_EndCommandBuffer(zest_queue queue);
//Copy a buffer to another buffer. Generally this will be a staging buffer copying to a buffer on the GPU (device_buffer). You must specify
//the size as well that you want to copy. Must be called inside a zest_BeginOneTimeCommandBuffer.
ZEST_API zest_bool zest_imm_CopyBuffer(zest_queue queue, zest_buffer src_buffer, zest_buffer dst_buffer, zest_size size);
ZEST_API zest_bool zest_imm_CopyBufferRegion(zest_queue queue, zest_buffer src_buffer, zest_size src_offset, zest_buffer dst_buffer, zest_size dst_offset, zest_size size);
ZEST_API zest_bool zest_imm_CopyBufferToImage(zest_queue queue, zest_buffer src_buffer, zest_image dst_image, zest_size size);
//Fill a buffer with a value
ZEST_API void zest_imm_FillBuffer (zest_queue queue, zest_buffer buffer, zest_uint value);
//Update a buffer (max size 64k)
ZEST_API void zest_imm_UpdateBuffer (zest_queue queue, zest_buffer buffer, void *data, zest_size intended_size);
//Copies an area of a zest_texture to another zest_texture
//ZEST_API zest_bool zest_imm_CopyImageToImage(zest_image src_image, zest_image target, int src_x, int src_y, int dst_x, int dst_y, int width, int height);
ZEST_API zest_bool zest_imm_TransitionImage(zest_queue queue, zest_image image, zest_image_layout new_layout, zest_uint base_mip_index, zest_uint mip_levels, zest_uint base_array_index, zest_uint layer_count);
ZEST_API zest_bool zest_imm_CopyBufferRegionsToImage(zest_queue queue, zest_buffer_image_copy_t *regions, zest_uint regions_count, zest_buffer buffer, zest_image image_handle);
ZEST_API zest_bool zest_imm_GenerateMipMaps(zest_queue queue, zest_image image_handle);
//Clear a color image to the specified color value. Image must be a color format.
ZEST_API zest_bool zest_imm_ClearColorImage(zest_queue queue, zest_image image, zest_clear_value_t clear_value);
//Clear a depth/stencil image to the specified depth and stencil values.
ZEST_API zest_bool zest_imm_ClearDepthStencilImage(zest_queue queue, zest_image image, float depth, zest_uint stencil);
//Blit (scaled copy) from one image to another with filtering. Useful for resizing images or format conversion.
ZEST_API zest_bool zest_imm_BlitImage(zest_queue queue, zest_image src_image, zest_image dst_image, int src_x, int src_y, int src_width, int src_height, int dst_x, int dst_y, int dst_width, int dst_height, zest_filter_type filter);
//Resolve a multisampled image to a single-sampled image.
ZEST_API zest_bool zest_imm_ResolveImage(zest_queue queue, zest_image src_image, zest_image dst_image);
//Send push constants to a shader. Must be called inside zest_imm_BeginCommandBuffer.
ZEST_API void zest_imm_SendPushConstants(zest_queue queue, void *data, zest_uint size);
//Bind a compute pipeline. Must be called inside zest_imm_BeginCommandBuffer and will be used for all subsequent
//calls to zest_imm_DispatchCompute.
ZEST_API void zest_imm_BindComputePipeline(zest_queue queue, zest_compute compute);
//Dispatch a compute pipeline. Must be called inside zest_imm_BeginCommandBuffer and you must have called zest_imm_BindComputePipeline
ZEST_API zest_bool zest_imm_DispatchCompute(zest_queue queue, zest_uint group_count_x, zest_uint group_count_y, zest_uint group_count_z);

//-----------------------------------------------
// Command_buffer_functions
// All these functions are called inside a frame graph context in callbacks in order to perform commands
// on the GPU. These all require a platform specific implementation
//-----------------------------------------------
ZEST_API void zest_cmd_BlitImageMip(const zest_command_list command_list, zest_resource_node src, zest_resource_node dst, zest_uint mip_to_blit, zest_pipeline_stage_flags pipeline_stage);
ZEST_API void zest_cmd_CopyImageMip(const zest_command_list command_list, zest_resource_node src, zest_resource_node dst, zest_uint mip_to_blit, zest_pipeline_stage_flags pipeline_stage);
// -- Helper functions to insert barrier functions within pass callbacks
ZEST_API void zest_cmd_InsertComputeImageBarrier(const zest_command_list command_list, zest_resource_node resource, zest_uint base_mip);
//Set a screen sized viewport and scissor command in the render pass
ZEST_API void zest_cmd_SetScreenSizedViewport(const zest_command_list command_list, float min_depth, float max_depth);
ZEST_API void zest_cmd_LayerViewport(const zest_command_list command_list, zest_layer layer);
ZEST_API void zest_cmd_Scissor(const zest_command_list command_list, zest_scissor_rect_t *scissor);
ZEST_API void zest_cmd_ViewPort(const zest_command_list command_list, zest_viewport_t *viewport);
//Create a scissor and view port command. Must be called within a command buffer
ZEST_API void zest_cmd_Clip(const zest_command_list command_list, float x, float y, float width, float height, float minDepth, float maxDepth);
//Bind a pipeline for use in a draw routing. Once you have built the pipeline at some point you will want to actually use it to draw things.
//In order to do that you can bind the pipeline using this function. Just pass in the pipeline handle. Note that the
ZEST_API void zest_cmd_BindPipeline(const zest_command_list command_list, zest_pipeline pipeline);
//Bind a pipeline for a compute shader
ZEST_API void zest_cmd_BindComputePipeline(const zest_command_list command_list, zest_compute compute);
//Bind a descriptor set. By default the global bindless set is bound at the start of a command buffer and you shouldn't
//need to bind it again. You can call this function to bind additional sets that you need like uniform buffers
ZEST_API void zest_cmd_BindDescriptorSets(const zest_command_list command_list, zest_pipeline_bind_point bind_point, zest_pipeline_layout layout, zest_descriptor_set *sets, zest_uint set_count, zest_uint first_set);
//Exactly the same as zest_CopyBuffer but you can specify a command buffer to use to make the copy. This can be useful if you are doing a
//one off copy with a separate command buffer
ZEST_API void zest_cmd_CopyBuffer(const zest_command_list command_list, zest_buffer staging_buffer, zest_buffer device_buffer, zest_size size);
ZEST_API zest_bool zest_cmd_UploadBuffer(const zest_command_list command_list, zest_buffer_uploader_t *uploader);
//Bind a vertex buffer. For use inside a draw routine callback function.
ZEST_API void zest_cmd_BindVertexBuffer(const zest_command_list command_list, zest_uint first_binding, zest_uint binding_count, zest_buffer buffer);
//Bind an index buffer. For use inside a draw routine callback function.
ZEST_API void zest_cmd_BindIndexBuffer(const zest_command_list command_list, zest_buffer buffer);
//Clear an image within a frame graph
ZEST_API zest_bool zest_cmd_ImageClear(const zest_command_list command_list, zest_image image);
ZEST_API void zest_cmd_BindMeshVertexBuffer(const zest_command_list command_list, zest_layer layer);
ZEST_API void zest_cmd_BindMeshIndexBuffer(const zest_command_list command_list, zest_layer layer);
//Helper function to dispatch a compute shader so you can call this instead of vkCmdDispatch. Specify a command buffer for use in one off dispataches
ZEST_API void zest_cmd_DispatchCompute(const zest_command_list command_list, zest_uint group_count_x, zest_uint group_count_y, zest_uint group_count_z);
//Send push constants. For use inside a draw routine callback function. pass in the pipeline,
//and a pointer to the data containing the push constants. The data MUST match the push constant range in the pipeline
ZEST_API void zest_cmd_SendPushConstants(const zest_command_list command_list, void *data, zest_uint size);
//Helper function to record the command to draw via a pipeline. Will record with the current command buffer being used in the active command queue. For use inside
//a draw routine callback function
ZEST_API void zest_cmd_Draw(const zest_command_list command_list, zest_uint vertex_count, zest_uint instance_count, zest_uint first_vertex, zest_uint first_instance);
ZEST_API void zest_cmd_DrawLayerInstruction(const zest_command_list command_list, zest_uint vertex_count, zest_layer_instruction_t *instruction);
//Helper function to record the command to draw indexed vertex data. Will record with the current command buffer being used in the active command queue. For use inside
//a draw routine callback function
ZEST_API void zest_cmd_DrawIndexed(const zest_command_list command_list, zest_uint index_count, zest_uint instance_count, zest_uint first_index, int32_t vertex_offset, zest_uint first_instance);
//Set the depth bias for when depth bias is enabled in the pipeline
ZEST_API void zest_cmd_SetDepthBias(const zest_command_list command_list, float factor, float clamp, float slope);

#ifdef __cplusplus
}
#endif

#define ZEST_CLEANUP_ON_FALSE(res) do { \
if ((res) == ZEST_FALSE) { \
goto cleanup; \
}                                                                                                                  \
} while (0)

// [Zloc_implementation]
#if defined(ZEST_IMPLEMENTATION)

//Definitions
ZLOC_API void* zloc_BlockUserExtensionPtr(const zloc_header *block) {
	return (char*)block + sizeof(zloc_header);
}

ZLOC_API void* zloc_AllocationFromExtensionPtr(const void *block) {
	return (void*)((char*)block - zloc__MINIMUM_BLOCK_SIZE);
}

zloc_allocator *zloc_InitialiseAllocator(void *memory) {
	if (!memory) {
		ZLOC_PRINT_ERROR(ZLOC_ERROR_COLOR"%s: The memory pointer passed in to the initialiser was NULL, did it allocate properly?\n", ZLOC_ERROR_NAME);
		return 0;
	}

	zloc_allocator *allocator = (zloc_allocator*)memory;
	memset(allocator, 0, sizeof(zloc_allocator));
	allocator->null_block.next_free_block = &allocator->null_block;
	allocator->null_block.prev_free_block = &allocator->null_block;
	allocator->minimum_allocation_size = zloc__MINIMUM_BLOCK_SIZE;

	//Point all of the segregated list array pointers to the empty block
	for (zloc_uint i = 0; i < zloc__FIRST_LEVEL_INDEX_COUNT; i++) {
		for (zloc_uint j = 0; j < zloc__SECOND_LEVEL_INDEX_COUNT; j++) {
			allocator->segregated_lists[i][j] = &allocator->null_block;
		}
	}

	allocator->get_block_size_callback = zloc__block_size;
	allocator->merge_next_callback = zloc__null_merge_callback;
	allocator->merge_prev_callback = zloc__null_merge_callback;
	allocator->split_block_callback = zloc__null_split_callback;
	allocator->add_pool_callback = zloc__null_add_pool_callback;
	allocator->unable_to_reallocate_callback = zloc__null_unable_to_reallocate_callback;

	return allocator;
}

zloc_allocator *zloc_InitialiseAllocatorWithPool(void *memory, zloc_size size) {
	zloc_size array_offset = sizeof(zloc_allocator);
	if (size < array_offset + zloc__MEMORY_ALIGNMENT) {
		ZLOC_PRINT_ERROR(ZLOC_ERROR_COLOR"%s: Tried to initialise allocator with a memory allocation that is too small. Must be at least: %zi bytes\n", ZLOC_ERROR_NAME, array_offset + zloc__MEMORY_ALIGNMENT);
		return 0;
	}

	zloc_allocator *allocator = zloc_InitialiseAllocator(memory);
	if (!allocator) {
		return 0;
	}

	zloc_AddPool(allocator, zloc_GetPool(allocator), size - zloc_AllocatorSize());
	return allocator;
}

zloc_size zloc_AllocatorSize(void) {
	return sizeof(zloc_allocator);
}

void zloc_SetMinimumAllocationSize(zloc_allocator *allocator, zloc_size size) {
	ZLOC_ASSERT(allocator->minimum_allocation_size == zloc__MINIMUM_BLOCK_SIZE);		//You cannot change this once set
	ZLOC_ASSERT(zloc__is_pow2(size));													//Size must be a power of 2
	allocator->minimum_allocation_size = zloc__Max(zloc__MINIMUM_BLOCK_SIZE, size);
}

zloc_pool *zloc_GetPool(zloc_allocator *allocator) {
	return (zloc_pool*)((char*)allocator + zloc_AllocatorSize());
}

zloc_pool *zloc_AddPool(zloc_allocator *allocator, void *memory, zloc_size size) {
	zloc__lock_thread_access;

	//Offset it back by the pointer size, we don't need the prev_physical block pointer as there is none
	//for the first block in the pool
	zloc_header *block = zloc__first_block_in_pool((const zloc_pool*)memory);
	block->size = 0;
	//Leave room for an end block
	zloc__set_block_size(block, size - (zloc__BLOCK_POINTER_OFFSET)-zloc__BLOCK_SIZE_OVERHEAD);

	//Make sure it aligns
	zloc__set_block_size(block, zloc__align_size_down(zloc__block_size(block), zloc__MEMORY_ALIGNMENT));
	ZLOC_ASSERT(zloc__block_size(block) > zloc__MINIMUM_BLOCK_SIZE);
	zloc__block_set_free(block);
	zloc__block_set_prev_used(block);

	//Add a 0 sized block at the end of the pool to cap it off
	zloc_header *last_block = zloc__next_physical_block(block);
	last_block->size = 0;
	zloc__block_set_used(last_block);

	allocator->stats.capacity += zloc__block_size(block);
	last_block->prev_physical_block = block;
	allocator->stats.blocks_in_use++;
	zloc__push_block(allocator, block);

	zloc__unlock_thread_access;
	return (zloc_pool*)memory;
}

zloc_bool zloc_RemovePool(zloc_allocator *allocator, zloc_pool *pool) {
	zloc__lock_thread_access;
	zloc_header *block = zloc__first_block_in_pool(pool);

	if (zloc__is_free_block(block) && !zloc__next_block_is_free(block) && zloc__is_last_block_in_pool(zloc__next_physical_block(block))) {
		zloc__remove_block_from_segregated_list(allocator, block);
		zloc__unlock_thread_access;
		return 1;
	}
	#if defined(ZLOC_THREAD_SAFE)
	zloc__unlock_thread_access;
	ZLOC_PRINT_ERROR(ZLOC_ERROR_COLOR"%s: In order to remove a pool there must be only 1 free block in the pool. Was possibly freed by another thread\n", ZLOC_ERROR_NAME);
	#else
	ZLOC_PRINT_ERROR(ZLOC_ERROR_COLOR"%s: In order to remove a pool there must be only 1 free block in the pool.\n", ZLOC_ERROR_NAME);
	#endif
	return 0;
}

void *zloc__allocate(zloc_allocator *allocator, zloc_size size, zloc_size remote_size) {
	zloc__lock_thread_access;
	size = zloc__adjust_size(size, zloc__MINIMUM_BLOCK_SIZE, zloc__MEMORY_ALIGNMENT);
	zloc_header *block = zloc__find_free_block(allocator, size, remote_size);

	if (block) {
		zloc__unlock_thread_access;
		return zloc__block_user_ptr(block);
	}

	//Out of memory;
	ZLOC_PRINT_ERROR(ZLOC_ERROR_COLOR"%s: Not enough memory in pool to allocate %llu bytes\n", ZLOC_ERROR_NAME, zloc__map_size);
	zloc__unlock_thread_access;
	return 0;
}

void *zloc_Reallocate(zloc_allocator *allocator, void *ptr, zloc_size size) {
	zloc__lock_thread_access;

	if (ptr && size == 0) {
		zloc__unlock_thread_access;
		zloc_Free(allocator, ptr);
		zloc__lock_thread_access;
	}

	if (!ptr) {
		zloc__unlock_thread_access;
		return zloc__allocate(allocator, size, 0);
	}

	zloc_header *block = zloc__block_from_allocation(ptr);
	zloc_header *next_block = zloc__next_physical_block(block);
	void *allocation = 0;
	zloc_size current_size = zloc__block_size(block);
	zloc_size adjusted_size = zloc__adjust_size(size, allocator->minimum_allocation_size, zloc__MEMORY_ALIGNMENT);
	zloc_size combined_size = current_size + zloc__block_size(next_block);
	if ((!zloc__next_block_is_free(block) || adjusted_size > combined_size) && adjusted_size > current_size) {
		zloc_header *new_block = zloc__find_free_block(allocator, adjusted_size, 0);
		if (new_block) {
			allocation = zloc__block_user_ptr(new_block);
		}
		if (allocation) {
			zloc_size smallest_size = zloc__Min(current_size, size);
			memcpy(allocation, ptr, smallest_size);
			zloc__do_unable_to_reallocate_callback;
			zloc__unlock_thread_access;
			zloc_Free(allocator, ptr);
			zloc__lock_thread_access;
		}
	} else {
		//Reallocation is possible
		if (adjusted_size > current_size) {
			zloc__merge_with_next_block(allocator, block);
			zloc__mark_block_as_used(block);
		}
		zloc_header *split_block = zloc__maybe_split_block(allocator, block, adjusted_size, 0);
		allocation = zloc__block_user_ptr(split_block);
	}

	zloc__unlock_thread_access;
	return allocation;
}

void *zloc_AllocateAligned(zloc_allocator *allocator, zloc_size size, zloc_size alignment) {
	zloc__lock_thread_access;
	zloc_size adjusted_size = zloc__adjust_size(size, allocator->minimum_allocation_size, alignment);
	zloc_size gap_minimum = sizeof(zloc_header);
	zloc_size size_with_gap = zloc__adjust_size(adjusted_size + alignment + gap_minimum, allocator->minimum_allocation_size, alignment);
	size_t aligned_size = (adjusted_size && alignment > zloc__MEMORY_ALIGNMENT) ? size_with_gap : adjusted_size;

	zloc_header *block = zloc__find_free_block(allocator, aligned_size, 0);

	if (block) {
		void *user_ptr = zloc__block_user_ptr(block);
		void *aligned_ptr = zloc__align_ptr(user_ptr, alignment);
		zloc_size gap = (zloc_size)(((ptrdiff_t)aligned_ptr) - (ptrdiff_t)user_ptr);

		/* If gap size is too small, offset to next aligned boundary. */
		if (gap && gap < gap_minimum)
		{
			zloc_size gap_remain = gap_minimum - gap;
			zloc_size offset = zloc__Max(gap_remain, alignment);
			const void* next_aligned = (void*)((ptrdiff_t)aligned_ptr + offset);

			aligned_ptr = zloc__align_ptr(next_aligned, alignment);
			gap = (zloc_size)((ptrdiff_t)aligned_ptr - (ptrdiff_t)user_ptr);
		}

		if (gap)
		{
			ZLOC_ASSERT(gap >= gap_minimum && "gap size too small");
			block = zloc__split_aligned_block(allocator, block, gap);
			zloc__block_set_used(block);
		}
		ZLOC_ASSERT(zloc__ptr_is_aligned(zloc__block_user_ptr(block), alignment));	//pointer not aligned to requested alignment
	}
	else {
		zloc__unlock_thread_access;
		return 0;
	}

	zloc__unlock_thread_access;
	return zloc__block_user_ptr(block);
}

int zloc_Free(zloc_allocator *allocator, void* allocation) {
	if (!allocation) return 0;
	zloc__lock_thread_access;
	zloc_header *block = zloc__block_from_allocation(allocation);
	//Asserting here means that there's probably been a mix up between a context allocator and a device allocator.
	ZLOC_ASSERT(block->allocator == allocator);
	if (zloc__prev_is_free_block(block)) {
		ZLOC_ASSERT(block->prev_physical_block);		//Must be a valid previous physical block
		block = zloc__merge_with_prev_block(allocator, block);
	}
	if (zloc__next_block_is_free(block)) {
		zloc__merge_with_next_block(allocator, block);
	}
	zloc__push_block(allocator, block);
	zloc__unlock_thread_access;
	return 1;
}

ZLOC_API void* zloc_PromoteLinearBlock(zloc_allocator *allocator, void* linear_alloc_mem, zloc_size used_size) {
	if (!allocator || !linear_alloc_mem || used_size == 0) {
		return 0;
	}

	zloc__lock_thread_access;

	zloc_header *block = zloc__block_from_allocation(linear_alloc_mem);
	ZLOC_ASSERT(allocator == block->allocator);	//allocator MUST match the block allocator

	// Ensure the block is valid and currently in use.
	if (zloc__is_free_block(block)) {
		ZLOC_PRINT_ERROR(ZLOC_ERROR_COLOR"%s: Cannot promote a block that is already free.\n", ZLOC_ERROR_NAME);
		zloc__unlock_thread_access;
		return 0;
	}

	zloc_size original_block_size = zloc__block_size(block);
	zloc_size aligned_keep_size = zloc__adjust_size(used_size, zloc__MINIMUM_BLOCK_SIZE, zloc__MEMORY_ALIGNMENT);

	if (original_block_size <= aligned_keep_size + zloc__MINIMUM_BLOCK_SIZE) {
		// Not enough space to split, so we just "promote" the whole block by doing nothing.
		zloc__unlock_thread_access;
		return linear_alloc_mem;
	}

	zloc_header *next_block = zloc__next_physical_block(block);
	zloc_size new_free_block_size = (char *)next_block - ((char *)linear_alloc_mem + aligned_keep_size);
	zloc_header *trimmed_free_block = (zloc_header *)((char *)linear_alloc_mem + aligned_keep_size);

	new_free_block_size -= zloc__BLOCK_POINTER_OFFSET;
	zloc__set_block_size(trimmed_free_block, new_free_block_size);
	zloc__block_set_free(trimmed_free_block);
	zloc__set_block_size(block, aligned_keep_size);

	zloc__set_prev_physical_block(next_block, trimmed_free_block);
	zloc__set_prev_physical_block(trimmed_free_block, block);
	zloc__block_set_prev_used(trimmed_free_block);

	zloc_header *next_block_from_trimmed_block = zloc__next_physical_block(trimmed_free_block);
	zloc_header *next_block_from_promoted_block = zloc__next_physical_block(block);

	//Sanity checks to double check the blocks all connect up
	ZLOC_ASSERT(next_block_from_trimmed_block == next_block);
	ZLOC_ASSERT(next_block_from_promoted_block == trimmed_free_block);
	ZLOC_ASSERT(block == trimmed_free_block->prev_physical_block);
	ZLOC_ASSERT(trimmed_free_block == next_block->prev_physical_block);

	zloc__push_block(allocator, trimmed_free_block);

	zloc__unlock_thread_access;

	return linear_alloc_mem;
}

int zloc_SafeCopy(void *dst, void *src, zloc_size size) {
	zloc_header *block = zloc__block_from_allocation(dst);
	if (size > block->size) {
		assert(0);  //Trying to copy outside of the memory block
		return 0;
	}
	zloc_header *next_physical_block = zloc__next_physical_block(block);
	ptrdiff_t diff_check = (ptrdiff_t)((char *)dst + size) - (ptrdiff_t)next_physical_block;
	if (diff_check > 0) {
		assert(0);  //Trying to copy outside of the memory block
		return 0;
	}
	memcpy(dst, src, size);
	return 1;
}

int zloc_SafeCopyBlock(void *dst_block_start, void *dst, const void *src, zloc_size size) {
	zloc_header *block = zloc__block_from_allocation(dst_block_start);
	zloc_header *next_physical_block = zloc__next_physical_block(block);
	ptrdiff_t diff_check = (ptrdiff_t)((char *)dst + size) - (ptrdiff_t)next_physical_block;
	if (diff_check > 0) {
		assert(0);  //Trying to copy outside of the memory block
		return 0;
	}
	memcpy(dst, src, size);
	return 1;
}

zloc_pool_stats_t zloc_CreateMemorySnapshot(zloc_header *first_block) {
	zloc_pool_stats_t stats = { 0 };
	zloc_header *current_block = first_block;
	while (!zloc__is_last_block_in_pool(current_block)) {
		if (zloc__is_free_block(current_block)) {
			stats.free_blocks++;
			stats.free_size += zloc__block_size(current_block);
		} else {
			stats.used_blocks++;
			stats.used_size += zloc__block_size(current_block);
		}
		current_block = zloc__next_physical_block(current_block);
	}
	if (zloc__is_free_block(current_block)) {
		stats.free_blocks++;
		stats.free_size += zloc__block_size(current_block);
	} else if (zloc__block_size(current_block) > 0) {
		stats.used_blocks++;
		stats.used_size += zloc__block_size(current_block);
	}
	return stats;
}

/*
	Standard callbacks, you can copy paste these to replace with your own as needed to add any extra functionality
	that you might need
*/
void zloc__remote_merge_next_callback(void *remote_user_data, zloc_header *block, zloc_header *next_block) {
	zloc_remote_header *remote_block = (zloc_remote_header*)zloc_BlockUserExtensionPtr(block);
	zloc_remote_header *next_remote_block = (zloc_remote_header*)zloc_BlockUserExtensionPtr(next_block);
	remote_block->size += next_remote_block->size;
	next_remote_block->memory_offset = 0;
	next_remote_block->size = 0;
}

void zloc__remote_merge_prev_callback(void *remote_user_data, zloc_header *prev_block, zloc_header *block) {
	zloc_remote_header *remote_block = (zloc_remote_header*)zloc_BlockUserExtensionPtr(block);
	zloc_remote_header *prev_remote_block = (zloc_remote_header*)zloc_BlockUserExtensionPtr(prev_block);
	prev_remote_block->size += remote_block->size;
	remote_block->memory_offset = 0;
	remote_block->size = 0;
}

zloc_size zloc__get_remote_size(const zloc_header *block) {
	zloc_remote_header *remote_block = (zloc_remote_header*)zloc_BlockUserExtensionPtr(block);
	return remote_block->size;
}

void *zloc_Allocate(zloc_allocator *allocator, zloc_size size) {
	return zloc__allocate(allocator, size, 0);
}

void zloc_SetBlockExtensionSize(zloc_allocator *allocator, zloc_size size) {
	ZLOC_ASSERT(allocator->block_extension_size == 0);	//You cannot change this once set
	allocator->block_extension_size = zloc__align_size_up(size, zloc__MEMORY_ALIGNMENT);
}

zloc_size zloc_CalculateRemoteBlockPoolSize(zloc_allocator *allocator, zloc_size remote_pool_size) {
	ZLOC_ASSERT(allocator->block_extension_size);	//You must set the block extension size first
	ZLOC_ASSERT(allocator->minimum_allocation_size);		//You must set the number of bytes per block
	return (sizeof(zloc_header) + allocator->block_extension_size) * (remote_pool_size / allocator->minimum_allocation_size) + zloc__BLOCK_POINTER_OFFSET;
}

void zloc_AddRemotePool(zloc_allocator *allocator, void *block_memory, zloc_size block_memory_size, zloc_size remote_pool_size) {
	ZLOC_ASSERT(allocator->add_pool_callback);	//You must set all the necessary callbacks to handle remote memory management
	ZLOC_ASSERT(allocator->get_block_size_callback);
	ZLOC_ASSERT(allocator->merge_next_callback);
	ZLOC_ASSERT(allocator->merge_prev_callback);
	ZLOC_ASSERT(allocator->split_block_callback);
	ZLOC_ASSERT(allocator->get_block_size_callback != zloc__block_size);	//Make sure you initialise the remote allocator with zloc_InitialiseAllocatorForRemote

	void *block = zloc_BlockUserExtensionPtr(zloc__first_block_in_pool((zloc_pool*)block_memory));
	zloc__do_add_pool_callback;
	zloc_AddPool(allocator, block_memory, block_memory_size);
}

zloc_allocator *zloc_InitialiseAllocatorForRemote(void *memory) {
	if (!memory) {
		ZLOC_PRINT_ERROR(ZLOC_ERROR_COLOR"%s: The memory pointer passed in to the initialiser was NULL, did it allocate properly?\n", ZLOC_ERROR_NAME);
		return 0;
	}

	zloc_allocator *allocator = zloc_InitialiseAllocator(memory);
	if (!allocator) {
		return 0;
	}

	allocator->get_block_size_callback = zloc__get_remote_size;
	allocator->merge_next_callback = zloc__remote_merge_next_callback;
	allocator->merge_prev_callback = zloc__remote_merge_prev_callback;

	return allocator;
}

void *zloc_AllocateRemote(zloc_allocator *allocator, zloc_size remote_size) {
	ZLOC_ASSERT(allocator->minimum_allocation_size > 0);
	remote_size = zloc__Max(remote_size, allocator->minimum_allocation_size);
	void* allocation = zloc__allocate(allocator, (remote_size / allocator->minimum_allocation_size) * (allocator->block_extension_size + zloc__BLOCK_POINTER_OFFSET), remote_size);
	return allocation ? (char*)allocation + zloc__MINIMUM_BLOCK_SIZE : 0;
}

void *zloc__reallocate_remote(zloc_allocator *allocator, void *ptr, zloc_size size, zloc_size remote_size) {
	zloc__lock_thread_access;

	if (ptr && remote_size == 0) {
		zloc__unlock_thread_access;
		zloc_FreeRemote(allocator, ptr);
		zloc__lock_thread_access;
	}

	if (!ptr) {
		zloc__unlock_thread_access;
		return zloc__allocate(allocator, size, remote_size);
	}

	zloc_header *block = zloc__block_from_allocation(ptr);
	zloc_header *next_block = zloc__next_physical_block(block);
	void *allocation = 0;
	zloc_size current_size = zloc__block_size(block);
	zloc_size current_remote_size = zloc__do_size_class_callback(block);
	zloc_size adjusted_size = zloc__adjust_size(size, allocator->minimum_allocation_size, zloc__MEMORY_ALIGNMENT);
	zloc_size combined_size = current_size + zloc__block_size(next_block);
	zloc_size combined_remote_size = current_remote_size + zloc__do_size_class_callback(next_block);
	if ((!zloc__next_block_is_free(block) || adjusted_size > combined_size || remote_size > combined_remote_size) && (remote_size > current_remote_size)) {
		zloc_header *new_block = zloc__find_free_block(allocator, size, remote_size);
		if (new_block) {
			allocation = zloc__block_user_ptr(new_block);
		}

		if (allocation) {
			zloc__do_unable_to_reallocate_callback;
			zloc__unlock_thread_access;
			zloc_Free(allocator, ptr);
			zloc__lock_thread_access;
		}
	}
	else {
		//Reallocation is possible
		if (remote_size > current_remote_size)
		{
			zloc__merge_with_next_block(allocator, block);
			zloc__mark_block_as_used(block);
		}
		zloc_header *split_block = zloc__maybe_split_block(allocator, block, adjusted_size, remote_size);
		allocation = zloc__block_user_ptr(split_block);
	}

	zloc__unlock_thread_access;
	return allocation;
}

void *zloc_ReallocateRemote(zloc_allocator *allocator, void *block_extension, zloc_size remote_size) {
	ZLOC_ASSERT(allocator->minimum_allocation_size > 0);
	remote_size = zloc__Max(remote_size, allocator->minimum_allocation_size);
	void* allocation = zloc__reallocate_remote(allocator, block_extension ? zloc_AllocationFromExtensionPtr(block_extension) : block_extension, (remote_size / allocator->minimum_allocation_size) * (allocator->block_extension_size + zloc__BLOCK_POINTER_OFFSET), remote_size);
	return allocation ? (char*)allocation + zloc__MINIMUM_BLOCK_SIZE : 0;
}

int zloc_FreeRemote(zloc_allocator *allocator, void* block_extension) {
	void *allocation = (char*)block_extension - zloc__MINIMUM_BLOCK_SIZE;
	return zloc_Free(allocator, allocation);
}

int zloc_InitialiseLinearAllocator(zloc_linear_allocator_t *allocator, void *memory, zloc_size size) {
	if (!memory) {
		ZLOC_PRINT_ERROR(ZLOC_ERROR_COLOR"%s: The memory pointer passed in to the initialiser was NULL, did it allocate properly?\n", ZLOC_ERROR_NAME);
		memset(allocator, 0, sizeof(zloc_linear_allocator_t));
		return 0;
	}
	if (size <= zloc__MINIMUM_BLOCK_SIZE) {
		ZLOC_PRINT_ERROR(ZLOC_ERROR_COLOR"%s: Size of linear allocator size is too small. It must be a mimimum of %i\n", ZLOC_ERROR_NAME, zloc__MINIMUM_BLOCK_SIZE);
		memset(allocator, 0, sizeof(zloc_linear_allocator_t));
		return 0;
	}
	allocator->data = memory;
	allocator->buffer_size = size;
	allocator->current_offset = 0;
	allocator->user_data = 0;
	allocator->next = 0;
	return 1;
}

void zloc_ResetLinearAllocator(zloc_linear_allocator_t *allocator) {
	while (allocator) {
		allocator->current_offset = 0;
		allocator = allocator->next;
	}
}

void *zloc_LinearAllocation(zloc_linear_allocator_t *allocator, zloc_size size_requested) {
	if (!allocator) return NULL;
	void *aligned_address = NULL;

	while (allocator) {
		zloc_size actual_size = size_requested < zloc__MINIMUM_BLOCK_SIZE ? zloc__MINIMUM_BLOCK_SIZE : size_requested;
		zloc_size alignment = sizeof(void *);

		char *current_ptr = (char *)allocator->data + allocator->current_offset;
		aligned_address = (void *)(((uintptr_t)current_ptr + alignment - 1) & ~(alignment - 1));

		zloc_size new_offset = (zloc_size)((char *)aligned_address - (char *)allocator->data) + actual_size;

		if (new_offset > allocator->buffer_size) {
			if (!allocator->next) {
				ZLOC_PRINT_ERROR(ZLOC_ERROR_COLOR"%s: Out of memory in linear allocator.\n", ZLOC_ERROR_NAME);
				return NULL;
			} else {
				allocator = allocator->next;
				continue;
			}
		}

		allocator->current_offset = new_offset;
		break;
	}
	return aligned_address;
}

zloc_size zloc_GetMarker(zloc_linear_allocator_t *allocator) {
	ZLOC_ASSERT(allocator);     //Not a valid allocator!
	return allocator->current_offset;
}

void zloc_ResetToMarker(zloc_linear_allocator_t *allocator, zloc_size marker) {
	ZLOC_ASSERT(allocator);     //Not a valid allocator!
	//marker point not valid!
	ZLOC_ASSERT(marker >= sizeof(zloc_linear_allocator_t) && marker <= allocator->current_offset && marker <= allocator->buffer_size);     //Not a valid allocator!
	allocator->current_offset = marker;
}

void zloc_SetLinearAllocatorUserData(zloc_linear_allocator_t *allocator, void *user_data) {
	ZLOC_ASSERT(allocator);     //Not a valid allocator!
	allocator->user_data = user_data;
}

void zloc_AddNextLinearAllocator(zloc_linear_allocator_t *allocator, zloc_linear_allocator_t *next) {
	while (allocator) {
		if (!allocator->next) {
			allocator->next = next;
			return;
		}
		allocator = allocator->next;
	}
	ZEST_ASSERT(0, "Unable to add next allocator, allocators may be currupted!");
}

zloc_size zloc_GetLinearAllocatorCapacity(zloc_linear_allocator_t *allocator) {
	zloc_size size = 0;
	while (allocator) {
		size += allocator->buffer_size;
		allocator = allocator->next;
	}
	return size;
}

//Zest_implementation
#ifndef ZEST_IMPL_H
#define ZEST_IMPL_H

zest__platform_setup zest__platform_setup_callbacks[zest_max_platforms] = { 0 };
//The thread local frame graph setup context
static ZEST_THREAD_LOCAL zest_frame_graph_builder zest__frame_graph_builder = NULL;

// --[Struct_definitions]
typedef struct zest_mesh_t {
    int magic;
	zest_context context;
	zest_size vertex_struct_size;
	void *vertex_data;
	zest_uint vertex_count;
	zest_uint vertex_capacity;
    zest_uint* indexes;
	zest_uint material;
} zest_mesh_t;

typedef struct zest_resource_group_t {
    int magic;
    zest_resource_node *resources;
} zest_resource_group_t;

typedef struct zest_buffer_allocator_t {
    int magic;
	const char *name;
	zest_buffer_allocator_backend backend;
	zest_device device;
	zest_context context;
	zest_buffer_info_t buffer_info;
	zest_buffer_usage_t usage;
    zloc_allocator *allocator;
    zest_device_memory_pool *memory_pools;
    zest_pool_range *range_pools;
	zest_buffer_pool_size_t pre_defined_pool_size;
} zest_buffer_allocator_t;

typedef struct zest_buffer_linear_allocator_t {
    int magic;
	zest_buffer_linear_allocator_backend backend;
	zest_context context;
	zest_size buffer_size;
	zest_size current_offset;
} zest_buffer_linear_allocator_t;

typedef struct zest_execution_timeline_t {
	int magic;
	zest_device device;
	zest_execution_timeline_backend backend;
	zest_u64 current_value;
} zest_execution_timeline_t;

typedef struct zest_queue_t {
	int magic;
	zest_queue_backend backend;
	zest_execution_timeline_t timeline;
	zest_uint index;
	zest_uint family_index;
	zest_device device;
	zest_compute last_bound_compute;
	volatile int in_use;
} zest_queue_t;

typedef struct zest_queue_manager_t {
	zest_uint family_index;
	zest_device_queue_type type;
	zest_queue_t *queues;
	zest_sync_t sync;
	zest_uint current_queue_index;
	zest_uint queue_count;
} zest_queue_manager_t;

typedef struct zest_execution_barriers_t {
	zest_execution_barriers_backend backend;
	zest_resource_node *acquire_image_barrier_nodes;
	zest_resource_node *acquire_buffer_barrier_nodes;
	zest_resource_node *release_image_barrier_nodes;
	zest_resource_node *release_buffer_barrier_nodes;
	#ifdef ZEST_DEBUGGING
	zest_image_barrier_t *acquire_image_barriers;
	zest_buffer_barrier_t *acquire_buffer_barriers;
	zest_image_barrier_t *release_image_barriers;
	zest_buffer_barrier_t *release_buffer_barriers;
	#endif
} zest_execution_barriers_t;

typedef struct zest_context_queue_t {
	//We decouple the frame in flight on the queue so that the counts don't get out of sync when the swap chain
	//can't be acquired for whatever reason.
	zest_uint fif;
	zest_u64 current_count[ZEST_MAX_FIF];
	zest_u64 signal_value;
	zest_uint next_buffer;
	zest_queue_manager_t *queue_manager;
	zest_queue queue;
	zest_context_queue_backend backend;
} zest_context_queue_t;

typedef struct zest_device_t {
	int magic;
	zest_uint api_version;
	zest_uint use_labels_address_bit;

	//Device maximums and other settings/formats
	zest_format depth_format;
	zest_uint max_image_size;
	zest_uint max_multiview_view_count;
	zest_size min_uniform_buffer_offset_alignment;
	zest_size max_uniform_buffer_size;
	zest_size max_storage_buffer_size;

	zest_device_init_flags init_flags;

	void *memory_pools[ZEST_MAX_DEVICE_MEMORY_POOLS];
	zest_size memory_pool_sizes[ZEST_MAX_DEVICE_MEMORY_POOLS];
	zest_uint memory_pool_count;
	zloc_allocator *allocator;
	char **extensions;
	zest_platform_memory_info_t platform_memory_info;
	zest_uint allocation_id;
	zest_uint vector_id;
	zloc_linear_allocator_t scratch_arena;

	zest_platform platform;
	zest_device_backend backend;

	zest_uint graphics_queue_family_index;
	zest_uint transfer_queue_family_index;
	zest_uint compute_queue_family_index;
	zest_queue_manager_t graphics_queues;
	zest_queue_manager_t compute_queues;
	zest_queue_manager_t transfer_queues;
	zest_queue_manager *queues;
	zest_map_queue_names queue_names;
	zest_text_t log_path;
	zest_device_builder_t setup_info;

	zest_map_buffer_pool_sizes pool_sizes;

	zest_map_validation_errors validation_errors;

	//For scheduled tasks
	zest_uint frame_counter;
	zest_device_destruction_queue_t deferred_resource_freeing_list;

	//Debugging
	zest_debug_t debug;
	zest_map_reports reports;

	//Built in shaders that I'll probably remove soon
	zest_builtin_shaders_t builtin_shaders;
	zest_builtin_pipeline_templates_t pipeline_templates;

	//Global descriptor set and layout template.
	zest_set_layout_builder_t global_layout_builder;

	//Bindless set layout and set
	zest_set_layout bindless_set_layout;
	zest_descriptor_set bindless_set;
	zest_pipeline_layout pipeline_layout;

	//Mip indexes for images
	zest_map_mip_indexes mip_indexes;

	//GPU buffer allocation
	zest_map_buffer_allocators buffer_allocators;

	//Default images for unbound descriptor indexes
	zest_image default_image_2d;
	zest_image default_image_cube;
  
	//Where to save/load cached shaders
	zest_text_t cached_shaders_path;

	//Resource storage
	zest_resource_store_t resource_stores[zest_max_device_handle_type];
 
	//Threading
	zest_uint thread_count;

	//Slang
	void *slang_info;

	//Callbacks
	void(*add_platform_extensions_callback)(ZEST_PROTOTYPE);
} zest_device_t;

typedef struct zest_context_t {
	int magic;

	//Platform specific data
	zest_context_backend backend;
	zest_uint allocation_id;
	zest_platform_memory_info_t platform_memory_info;

	//Queues and command buffer pools
	zest_context_queue queues[ZEST_QUEUE_COUNT];
	zest_uint active_queue_indexes[ZEST_QUEUE_COUNT];
	zest_uint active_queue_count;

	//Frame in flight indexes
	zest_uint frame_counter;
	zest_uint current_fif;
	zest_uint saved_fif;

	//The timeout for the fence that waits for gpu work to finish for the frame
	zest_u64 fence_wait_timeout_ns;
	zest_wait_timeout_callback frame_wait_timeout_callback;
	zest_execution_timeline_t frame_timeline[ZEST_MAX_FIF];
	zest_execution_timeline frame_sync_timeline[ZEST_MAX_FIF];

	//Window data
	zest_extent2d_t window_extent;
	float dpi_scale;
	zest_swapchain swapchain;
	zest_window_data_t window_data;

	//Linear allocator for building the render graph each frame. The memory for this is allocated from
	//The device TLSF allocator
	zloc_linear_allocator_t frame_graph_allocator[ZEST_MAX_FIF];

	//Resource storage
	void *memory_pools[ZEST_MAX_DEVICE_MEMORY_POOLS];
	zest_size memory_pool_sizes[ZEST_MAX_DEVICE_MEMORY_POOLS];
	zest_uint memory_pool_count;
	zloc_allocator *allocator;
	zest_resource_store_t resource_stores[zest_max_context_handle_type];
	zest_context_destruction_queue_t deferred_resource_freeing_list;
	zest_uint vector_id;

	//GPU buffer allocation
	zest_map_buffer_allocators buffer_allocators;

	//Cached pipelines
	zest_map_cached_pipelines cached_pipelines;

	//Frame Graph Cache Storage
	zest_map_cached_frame_graphs cached_frame_graphs;
	zest_map_frame_graph_semaphores cached_frame_graph_semaphores;
 
	//Flags
	zest_context_flags flags;

	void *user_data;
	zest_device_t *device;
	zest_uint device_frame_counter;
	zest_create_context_info_t create_info;
} zest_context_t;

typedef struct zest_pipeline_layout_t {
	int magic;
	zest_device device;
	zest_pipeline_layout_backend backend;
	zest_push_constant_range_t push_constant_range;
} zest_pipeline_layout_t;

typedef struct zest_command_list_t {
	int magic;
	zest_context context;
	zest_device device;
	zest_command_list_backend backend;
	zest_frame_graph frame_graph;
	zest_pass_node pass_node;
	zest_uint submission_index;
	zest_uint queue_index;
	zest_bool began_rendering;
	zest_pipeline_stage_flags timeline_wait_stage;
	zest_rendering_info_t rendering_info;
} zest_command_list_t;

typedef struct zest_submission_batch_t {
	zest_uint magic;
	zest_context_queue queue;
	zest_uint queue_family_index;
	zest_device_queue_type queue_type;
	zest_uint *pass_indices;
	zest_bool outputs_to_swapchain;
	zest_semaphore_reference_t *wait_semaphores;
	zest_semaphore_reference_t *signal_semaphores;
	zest_pipeline_stage_flags timeline_wait_stage;
	zest_pipeline_stage_flags queue_wait_stages;
	zest_pipeline_stage_flags *wait_dst_stage_masks;
	zest_pipeline_stage_flags *signal_dst_stage_masks;

	//References for printing the render graph only
	zest_u64 *wait_values;
	zest_u64 *signal_values;
	zest_pipeline_stage_flags *wait_stages;
	zest_pipeline_stage_flags *signal_stages;
	zest_bool need_timeline_wait;
	zest_bool need_timeline_signal;

	zest_submission_batch_backend backend;
} zest_submission_batch_t;

typedef struct zest_wave_submission_t {
	zest_uint queue_bits;
	zest_submission_batch_t batches[ZEST_QUEUE_COUNT];
} zest_wave_submission_t;

typedef struct zest_execution_details_t {
	attachment_idx attachment_indexes;
	zest_rendering_attachment_info_t *color_attachments;
	zest_rendering_attachment_info_t depth_attachment;
	zest_swapchain swapchain;
	zest_scissor_rect_t render_area;
	zest_clear_value_t *clear_values;
	zest_rendering_info_t rendering_info;
	zest_execution_barriers_t barriers;
	zest_bool requires_dynamic_render_pass;
} zest_execution_details_t;

typedef struct zest_pass_group_t {
	zest_pass_queue_info_t queue_info;
	zest_pass_queue_info_t compiled_queue_info;
	zest_map_resource_usages inputs;
	zest_map_resource_usages outputs;
	zest_resource_node *transient_resources_to_create;
	zest_resource_node *transient_resources_to_free;
	zest_uint submission_id;
	zest_execution_details_t execution_details;
	zest_pass_node *passes;
	zest_pass_flags flags;
} zest_pass_group_t;

zest_hash_map(zest_pass_group_t) zest_map_passes;
zest_hash_map(zest_resource_versions_t) zest_map_resource_versions;

typedef struct zest_frame_graph_semaphores_t {
	int magic;
	zest_frame_graph_semaphores_backend backend;
	zest_size values[ZEST_MAX_FIF][ZEST_QUEUE_COUNT];
} zest_frame_graph_semaphores_t;

typedef struct zest_frame_graph_t {
	int magic;
	zest_frame_graph_flags flags;
	zest_frame_graph_result error_status;
	zest_uint culled_passes_count;
	zest_uint culled_resources_count;
	const char *name;

	zest_bucket_array_t potential_passes;
	zest_map_passes final_passes;
	zest_bucket_array_t resources;
	zest_map_resource_versions resource_versions;
	zest_resource_node *resources_to_update;
	zest_pass_group_t **pass_execution_order;

	zest_resource_node *deferred_image_destruction;

	zest_descriptor_set *descriptor_sets;
	zest_pipeline_layout pipeline_layout;

	zest_execution_timeline wait_on_timeline;
	zest_execution_timeline signal_timeline;
	zest_frame_graph_semaphores semaphores;

	zest_execution_wave_t *execution_waves;         // Execution order after compilation

	zest_resource_node swapchain_resource;          // Handle to the current swapchain image resource
	zest_swapchain swapchain;                       // Handle to the current swapchain image resource
	zest_uint id_counter;
	zest_descriptor_pool descriptor_pool;           //Descriptor pool for execution nodes within the graph.
	zest_set_layout bindless_layout;				//Todo: remove as this will always be in the device.
	zest_descriptor_set bindless_set;

	zest_wave_submission_t *submissions;

	void *user_data;
	zest_command_list_t command_list;
	zest_key cache_key;
	zest_size cached_size;

	zest_microsecs compile_time;
	zest_microsecs execute_time;

	zest_uint timestamp_count;
	zest_query_state query_state[ZEST_MAX_FIF];                      //For checking if the timestamp query is ready
	zest_gpu_timestamp_t *timestamps[ZEST_MAX_FIF];                  //The last recorded frame durations for the whole render pipeline

} zest_frame_graph_t;

typedef struct zest_descriptor_pool_t {
	int magic;
	zest_descriptor_pool_backend backend;
	zest_uint max_sets;
	zest_uint allocations;
} zest_descriptor_pool_t;

typedef struct zest_descriptor_set_t {
	int magic;
	zest_descriptor_set_backend backend;
} zest_descriptor_set_t;

typedef struct zest_set_layout_t {
	int magic;
	zest_context context;
	zest_device device;
	zest_descriptor_binding_desc_t *bindings;
	zest_set_layout_backend backend;
	zest_descriptor_indices_t *descriptor_indexes;
	zest_text_t name;
	zest_u64 binding_indexes;
	zest_descriptor_pool pool;
	zest_set_layout_flags flags;
	zest_sync_t sync;
} zest_set_layout_t;

//A pipeline set is all of the necessary things required to setup and maintain a pipeline
typedef struct zest_pipeline_t {
	int magic;
	zest_context context;
	zest_pipeline_backend backend;
	zest_pipeline_template pipeline_template;
	zest_pipeline_layout layout;
	void(*rebuild_pipeline_function)(void*);                                     //Override the function to rebuild the pipeline when the swap chain is recreated
	zest_pipeline_set_flags flags;                                               //Flag bits
} zest_pipeline_t;

typedef struct zest_pass_node_t {
	int magic;
	zest_id id;
	const char *name;

	zest_pass_queue_info_t queue_info;

	zest_map_resource_usages inputs;
	zest_map_resource_usages outputs;
	zest_key output_key;
	zest_pass_execution_callback_t execution_callback;
	zest_compute compute;
	zest_pass_flags flags;
	zest_pass_type type;
	zest_pipeline_bind_point bind_point;
	zest_pass_node_visit_state visit_state;
} zest_pass_node_t;

typedef struct zest_shader_t {
	int magic;
	zest_shader_handle handle;
	char *spv;
	zest_size spv_size;
	zest_text_t file_path;
	zest_text_t shader_code;
	zest_text_t name;
	zest_shader_type type;
} zest_shader_t;

typedef struct zest_sampler_t {
	int magic;
	zest_sampler_handle handle;
	zest_sampler_backend backend;
	zest_sampler_info_t create_info;
} zest_sampler_t;

typedef struct zest_image_t {
	int magic;
	zest_image_handle handle;
	zest_image_backend backend;
	void *buffer;
	zest_uint bindless_index[zest_max_global_binding_number];
	zest_image_info_t info;
	zest_image_view default_view;
} zest_image_t;

typedef struct zest_image_view_array_t {
	int magic;
	zest_image_view_array_handle handle;
	zest_image image;
	zest_uint count;
	zest_image_view_t *views;
} zest_image_view_array_t;

typedef struct zest_image_view_t {
	int magic;
	zest_image image;
	zest_image_view_handle handle;
	zest_image_view_backend_t *backend;
} zest_image_view_t;

static const zest_image_t zest__image_zero = { 0 };

typedef struct zest_swapchain_t {
	int magic;
	zest_context context;
	zest_swapchain_backend backend;
	const char *name;
	zest_image_t *images;
	zest_image_view *views;
	zest_format format;
	zest_vec2 resolution;
	zest_extent2d_t size;
	zest_clear_value_t clear_color;
	zest_uint current_image_frame;
	zest_uint image_count;
	zest_swapchain_flags flags;
	zest_bool framebuffer_resized;
} zest_swapchain_t;

// --Buffer Management
typedef struct zest_device_memory_t {
	int magic;
	zest_device device;
	zest_device_memory_backend backend;
	zest_size size;
} zest_device_memory_t;

//A simple buffer struct for creating and mapping GPU memory
typedef struct zest_device_memory_pool_t {
	int magic;
	zest_device_memory_pool_backend backend;
	zest_device device;
	zest_buffer_allocator allocator;
	zest_size size;
	zest_size minimum_allocation_size;
	zest_size alignment;
	zest_uint memory_type_index;
	void* mapped;
} zest_device_memory_pool_t;

typedef struct zest_buffer_t {
	zest_size size;							//Size of the buffer on the GPU
	zest_size memory_offset;				//Offset from the start of the memory pool on the GPU
	zest_device_memory_pool memory_pool;	//Pointer to the memory pool where this allocation belongs
} zest_buffer_t;

typedef struct zest_uniform_buffer_t {
	int magic;
	const char *name;
	zest_uniform_buffer_handle handle;
	zest_buffer buffer[ZEST_MAX_FIF];
	zest_uint descriptor_index[ZEST_MAX_FIF];
} zest_uniform_buffer_t;

typedef struct zest_mesh_offset_data_t {
	zest_uint vertex_offset;
	zest_uint index_offset;
	zest_uint vertex_count;
	zest_uint index_count;
	zest_uint texture_index;
} zest_mesh_offset_data_t;

typedef struct zest_layer_t {
	int magic;
	zest_layer_handle handle;
	zest_context context;

	const char *name;

	zest_uint fif;
	zest_uint prev_fif;

	zest_layer_buffers_t memory_refs[ZEST_MAX_FIF];
	zest_bool dirty[ZEST_MAX_FIF];
	zest_uint initial_instance_pool_size;
	zest_buffer_uploader_t vertex_upload;
	zest_buffer_uploader_t index_upload;
	zest_descriptor_set bindless_set;

	zest_buffer vertex_data;
	zest_buffer index_data;
	zest_size used_vertex_data;
	zest_size used_index_data;
	zest_mesh_offset_data_t *mesh_offsets;

	zest_layer_instruction_t current_instruction;

	union {
		struct { zest_size instance_struct_size; };
		struct { zest_size vertex_struct_size; };
	};

	zest_color_t current_color;
	float intensity;

	zest_vec2 layer_size;
	zest_vec2 screen_scale;
	zest_uint index_count;

	zest_scissor_rect_t scissor;
	zest_viewport_t viewport;

	zest_layer_instruction_t *draw_instructions[ZEST_MAX_FIF];
	zest_uint instruction_index;
	zest_draw_mode last_draw_mode;

	zest_resource_node vertex_buffer_node;
	zest_resource_node index_buffer_node;

	zest_layer_flags flags;
	void *user_data;
} zest_layer_t ZEST_ALIGN_AFFIX(16);

typedef struct zest_compute_t {
	int magic;
	zest_compute_backend backend;
	zest_compute_handle handle;
	zest_pipeline_layout pipeline_layout;
	zest_shader_handle shader;                                // The shader handle that the compute dispatches to
	void *compute_data;                                       // Connect this to any custom data that is required to get what you need out of the compute process.
	zest_compute_flags flags;
} zest_compute_t;

typedef struct zest_resource_node_t {
	int magic;
	const char *name;
	zest_resource_type type;
	zest_id id;                                     // Unique identifier within the graph
	zest_uint version;
	zest_uint original_id;
	zest_frame_graph frame_graph;
	zest_resource_node aliased_resource;
	zest_swapchain swapchain;

	zest_buffer_description_t buffer_desc; // Used if transient buffer
	zest_vec4 clear_color;

	zest_image_t image;
	zest_image_layout *linked_layout;
	zest_image_view view;
	zest_image_view_array view_array;
	zest_buffer storage_buffer;
	zest_uint bindless_index[zest_max_global_binding_number];   		   //The index to use in the shader
	zest_uint *mip_level_bindless_indexes[zest_max_global_binding_number]; //The mip indexes to use in the shader

	zest_uint reference_count;

	zest_uint current_state_index;
	zest_uint current_queue_family_index;

	zest_image_layout image_layout;
	zest_access_flags access_mask;
	zest_pipeline_stage_flags last_stage_mask;

	zest_resource_state_t *journey;                 // List of the different states this resource has over the render graph, used to build barriers where needed
	int producer_pass_idx;                          // Index of the pass that writes/creates this resource (-1 if imported)
	int *consumer_pass_indices;                     // Dynamic array of pass indices that read this
	zest_resource_node_flags flags;                 // Imported/Transient Etc.
	zest_uint first_usage_pass_idx;                 // For lifetime tracking
	zest_uint last_usage_pass_idx;                  // For lifetime tracking

	zest_resource_buffer_provider buffer_provider;
	zest_resource_image_provider image_provider;
	void *user_data;
} zest_resource_node_t;

// -- End Struct Definions

#ifdef _WIN32
zest_millisecs zest_Millisecs(void) {
    LARGE_INTEGER frequency, counter;
    QueryPerformanceFrequency(&frequency);
    QueryPerformanceCounter(&counter);
    zest_ull ms = (zest_ull)(counter.QuadPart * 1000LL / frequency.QuadPart);
    return (zest_millisecs)ms;
}

zest_microsecs zest_Microsecs(void) {
    LARGE_INTEGER frequency, counter;
    QueryPerformanceFrequency(&frequency);
    QueryPerformanceCounter(&counter);
    zest_ull us = (zest_ull)(counter.QuadPart * 1000000LL / frequency.QuadPart);
    return (zest_microsecs)us;
}
#elif defined(__APPLE__)
#include <mach/mach_time.h>
zest_millisecs zest_Millisecs(void) {
    static mach_timebase_info_data_t timebase_info;
    if (timebase_info.denom == 0) {
        mach_timebase_info(&timebase_info);
    }

    uint64_t time_ns = mach_absolute_time() * timebase_info.numer / timebase_info.denom;
    zest_millisecs ms = (zest_millisecs)(time_ns / 1000000);
    return (zest_millisecs)ms;
}

zest_microsecs zest_Microsecs(void) {
    static mach_timebase_info_data_t timebase_info;
    if (timebase_info.denom == 0) {
        mach_timebase_info(&timebase_info);
    }

    uint64_t time_ns = mach_absolute_time() * timebase_info.numer / timebase_info.denom;
    zest_microsecs us = (zest_microsecs)(time_ns / 1000);
    return us;
}
#else
zest_millisecs zest_Millisecs(void) {
    struct timespec now;
    clock_gettime(CLOCK_REALTIME, &now);
    long m = now.tv_sec * 1000 + now.tv_nsec / 1000000;
    return (zest_millisecs)m;
}

zest_microsecs zest_Microsecs(void) {
    struct timespec now;
    clock_gettime(CLOCK_REALTIME, &now);
    zest_ull us = now.tv_sec * 1000000ULL + now.tv_nsec / 1000;
    return (zest_microsecs)us;
}
#endif

#ifdef _WIN32
FILE* zest__open_file(const char* file_name, const char* mode) {
    FILE* file = NULL;
    errno_t err = fopen_s(&file, file_name, mode);
    if (err != 0 || file == NULL) {
        return NULL;
    }
    return file;
}

#else

FILE* zest__open_file(const char* file_name, const char* mode) {
    return fopen(file_name, mode);
}

#endif

zest_bool zest__file_exists(const char* file_name) {
	FILE* file = zest__open_file(file_name, "r");
	if (file) {
		fclose(file);
		return ZEST_TRUE;
	}
	return ZEST_FALSE;
}

zest_bool zest__create_folder(zest_device device, const char *path) {
    int result = ZEST_CREATE_DIR(path);
    if (result == 0) {
        ZEST_APPEND_LOG(device->log_path.str, "Folder created successfully: %s", path);
        return ZEST_TRUE;
    } else {
        if (result == -1) {
            return ZEST_TRUE;
        } else {
            char buffer[100];
            if (zest_strerror(buffer, sizeof(buffer), result) != 0) {
				ZEST_APPEND_LOG(device->log_path.str, "Error creating folder: %s (Error: Unknown)", path);
            } else {
				ZEST_APPEND_LOG(device->log_path.str, "Error creating folder: %s (Error: %s)", path, buffer);
            }
            return ZEST_FALSE;
        }
    }
}

// --Math
zest_matrix4 zest_M4(float v) {
    zest_matrix4 matrix = ZEST__ZERO_INIT(zest_matrix4);
    matrix.v[0].x = v;
    matrix.v[1].y = v;
    matrix.v[2].z = v;
    matrix.v[3].w = v;
    return matrix;
}

zest_matrix4 zest_M4SetWithVecs(zest_vec4 a, zest_vec4 b, zest_vec4 c, zest_vec4 d) {
    zest_matrix4 r;
    r.v[0].x = a.x; r.v[0].y = a.y; r.v[0].z = a.z; r.v[0].w = a.w;
    r.v[1].x = b.x; r.v[1].y = b.y; r.v[1].z = b.z; r.v[1].w = b.w;
    r.v[2].x = c.x; r.v[2].y = c.y; r.v[2].z = c.z; r.v[2].w = c.w;
    r.v[3].x = d.x; r.v[3].y = d.y; r.v[3].z = d.z; r.v[3].w = d.w;
    return r;
}

zest_matrix4 zest_ScaleMatrix4x4(zest_matrix4* m, zest_vec4* v) {
    zest_matrix4 result = zest_M4(1);
    result.v[0] = zest_ScaleVec4(m->v[0], v->x);
    result.v[1] = zest_ScaleVec4(m->v[1], v->y);
    result.v[2] = zest_ScaleVec4(m->v[2], v->z);
    result.v[3] = zest_ScaleVec4(m->v[3], v->w);
    return result;
}

zest_matrix4 zest_ScaleMatrix4(zest_matrix4* m, float scalar) {
    zest_matrix4 result = zest_M4(1);
    result.v[0] = zest_ScaleVec4(m->v[0], scalar);
    result.v[1] = zest_ScaleVec4(m->v[1], scalar);
    result.v[2] = zest_ScaleVec4(m->v[2], scalar);
    result.v[3] = zest_ScaleVec4(m->v[3], scalar);
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

zest_vec4 zest_Vec4SetVec(zest_vec4 in) {
    zest_vec4 vec; vec.x = in.x; vec.y = in.y; vec.z = in.z; vec.w = in.w; return vec;
}

zest_color_t zest_ColorSet(zest_byte r, zest_byte g, zest_byte b, zest_byte a) {
	zest_color_t color;
	color.r = r;
	color.g = g;
	color.b = b;
	color.a = a;
    return color;
}

zest_color_t zest_ColorSet1(zest_byte c) {
	zest_color_t color;
	color.r = c;
	color.g = c;
	color.b = c;
	color.a = c;
    return color;
}

zest_vec2 zest_AddVec2(zest_vec2 left, zest_vec2 right) {
	zest_vec2 result;
	result.x = left.x + right.x;
	result.y = left.y + right.y;
    return result;
}

zest_vec3 zest_AddVec3(zest_vec3 left, zest_vec3 right) {
	zest_vec3 result;
	result.x = left.x + right.x;
	result.y = left.y + right.y;
	result.z = left.z + right.z;
    return result;
}

zest_vec4 zest_AddVec4(zest_vec4 left, zest_vec4 right) {
	zest_vec4 result;
	result.x = left.x + right.x;
	result.y = left.y + right.y;
	result.z = left.z + right.z;
	result.w = left.w + right.w;
    return result;
}

zest_ivec2 zest_AddiVec2(zest_ivec2 left, zest_ivec2 right) {
	zest_ivec2 result;
	result.x = left.x + right.x;
	result.y = left.y + right.y;
    return result;
}

zest_ivec3 zest_AddiVec3(zest_ivec3 left, zest_ivec3 right) {
	zest_ivec3 result;
	result.x = left.x + right.x;
	result.y = left.y + right.y;
	result.z = left.z + right.z;
    return result;
}

zest_vec2 zest_SubVec2(zest_vec2 left, zest_vec2 right) {
	zest_vec2 result;
	result.x = left.x - right.x;
	result.y = left.y - right.y;
    return result;
}

zest_vec3 zest_SubVec3(zest_vec3 left, zest_vec3 right) {
	zest_vec3 result;
	result.x = left.x - right.x;
	result.y = left.y - right.y;
	result.z = left.z - right.z;
    return result;
}

zest_vec4 zest_SubVec4(zest_vec4 left, zest_vec4 right) {
	zest_vec4 result;
	result.x = left.x - right.x;
	result.y = left.y - right.y;
	result.z = left.z - right.z;
	result.w = left.w - right.w;
    return result;
}

zest_vec3 zest_FlipVec3(zest_vec3 vec3) {
    zest_vec3 flipped;
    flipped.x = -vec3.x;
    flipped.y = -vec3.y;
    flipped.z = -vec3.z;
    return flipped;
}

zest_vec2 zest_ScaleVec2(zest_vec2 vec, float scalar) {
    zest_vec2 result;
    result.x = vec.x * scalar;
    result.y = vec.y * scalar;
    return result;
}

zest_vec3 zest_ScaleVec3(zest_vec3 vec, float scalar) {
    zest_vec3 result;
    result.x = vec.x * scalar;
    result.y = vec.y * scalar;
    result.z = vec.z * scalar;
    return result;
}

zest_vec4 zest_ScaleVec4(zest_vec4 vec, float scalar) {
    zest_vec4 result;
    result.x = vec.x * scalar;
    result.y = vec.y * scalar;
    result.z = vec.z * scalar;
    result.w = vec.w * scalar;
    return result;
}

zest_vec3 zest_MulVec3(zest_vec3 left, zest_vec3 right) {
	zest_vec3 result;
	result.x = left.x * right.x;
	result.y = left.y * right.y;
	result.z = left.z * right.z;
    return result;
}

zest_vec4 zest_MulVec4(zest_vec4 left, zest_vec4 right) {
	zest_vec4 result;
	result.x = left.x * right.x;
	result.y = left.y * right.y;
	result.z = left.z * right.z;
	result.w = left.w * right.w;
    return result;
}

zest_vec3 zest_DivVec3(zest_vec3 left, zest_vec3 right) {
	zest_vec3 result;
	result.x = left.x / right.x;
	result.y = left.y / right.y;
	result.z = left.z / right.z;
    return result;
}

zest_vec4 zest_DivVec4(zest_vec4 left, zest_vec4 right) {
	zest_vec4 result;
	result.x = left.x / right.x;
	result.y = left.y / right.y;
	result.z = left.z / right.z;
	result.w = left.w / right.w;
    return result;
}

float zest_LengthVec2(zest_vec2 const v) {
    return sqrtf(v.x * v.x + v.y * v.y);
}

float zest_LengthVec2NS(zest_vec2 const v) {
    return v.x * v.x + v.y * v.y;
}

float zest_LengthVec3NS(zest_vec3 const v) {
    return v.x * v.x + v.y * v.y + v.z * v.z;
}

float zest_LengthVec4NS(zest_vec4 const v) {
    return v.x * v.x + v.y * v.y + v.z * v.z + v.w * v.w;
}

float zest_LengthVec3(zest_vec3 const v) {
    return sqrtf(v.x * v.x + v.y * v.y + v.z * v.z);
}

zest_vec2 zest_NormalizeVec2(zest_vec2 const v) {
    float length = zest_LengthVec2(v);
	zest_vec2 result;
	result.x = v.x / length;
	result.y = v.y / length;
    return result;
}

zest_vec3 zest_NormalizeVec3(zest_vec3 const v) {
    float length = zest_LengthVec3(v);
	zest_vec3 result;
	result.x = v.x / length;
	result.y = v.y / length;
	result.z = v.z / length;
    return result;
}

zest_vec4 zest_NormalizeVec4(zest_vec4 const v) {
    float length = sqrtf(zest_LengthVec4NS(v));
	zest_vec4 result;
	result.x = v.x / length;
	result.y = v.y / length;
	result.z = v.z / length;
	result.w = v.w / length;
    return result;
}

zest_matrix4 zest_Inverse(zest_matrix4* m) {
    float Coef00 = m->v[2].z * m->v[3].w - m->v[3].z * m->v[2].w;
    float Coef02 = m->v[1].z * m->v[3].w - m->v[3].z * m->v[1].w;
    float Coef03 = m->v[1].z * m->v[2].w - m->v[2].z * m->v[1].w;

    float Coef04 = m->v[2].y * m->v[3].w - m->v[3].y * m->v[2].w;
    float Coef06 = m->v[1].y * m->v[3].w - m->v[3].y * m->v[1].w;
    float Coef07 = m->v[1].y * m->v[2].w - m->v[2].y * m->v[1].w;

    float Coef08 = m->v[2].y * m->v[3].z - m->v[3].y * m->v[2].z;
    float Coef10 = m->v[1].y * m->v[3].z - m->v[3].y * m->v[1].z;
    float Coef11 = m->v[1].y * m->v[2].z - m->v[2].y * m->v[1].z;

    float Coef12 = m->v[2].x * m->v[3].w - m->v[3].x * m->v[2].w;
    float Coef14 = m->v[1].x * m->v[3].w - m->v[3].x * m->v[1].w;
    float Coef15 = m->v[1].x * m->v[2].w - m->v[2].x * m->v[1].w;

    float Coef16 = m->v[2].x * m->v[3].z - m->v[3].x * m->v[2].z;
    float Coef18 = m->v[1].x * m->v[3].z - m->v[3].x * m->v[1].z;
    float Coef19 = m->v[1].x * m->v[2].z - m->v[2].x * m->v[1].z;

    float Coef20 = m->v[2].x * m->v[3].y - m->v[3].x * m->v[2].y;
    float Coef22 = m->v[1].x * m->v[3].y - m->v[3].x * m->v[1].y;
    float Coef23 = m->v[1].x * m->v[2].y - m->v[2].x * m->v[1].y;

    zest_vec4 Fac0 = zest_Vec4Set(Coef00, Coef00, Coef02, Coef03);
    zest_vec4 Fac1 = zest_Vec4Set(Coef04, Coef04, Coef06, Coef07);
    zest_vec4 Fac2 = zest_Vec4Set(Coef08, Coef08, Coef10, Coef11);
    zest_vec4 Fac3 = zest_Vec4Set(Coef12, Coef12, Coef14, Coef15);
    zest_vec4 Fac4 = zest_Vec4Set(Coef16, Coef16, Coef18, Coef19);
    zest_vec4 Fac5 = zest_Vec4Set(Coef20, Coef20, Coef22, Coef23);

    zest_vec4 Vec0 = zest_Vec4Set(m->v[1].x, m->v[0].x, m->v[0].x, m->v[0].x);
    zest_vec4 Vec1 = zest_Vec4Set(m->v[1].y, m->v[0].y, m->v[0].y, m->v[0].y);
    zest_vec4 Vec2 = zest_Vec4Set(m->v[1].z, m->v[0].z, m->v[0].z, m->v[0].z);
    zest_vec4 Vec3 = zest_Vec4Set(m->v[1].w, m->v[0].w, m->v[0].w, m->v[0].w);

    zest_vec4 Inv0 = zest_Vec4SetVec(zest_AddVec4(zest_SubVec4(zest_MulVec4(Vec1, Fac0), zest_MulVec4(Vec2, Fac1)), zest_MulVec4(Vec3, Fac2)));
    zest_vec4 Inv1 = zest_Vec4SetVec(zest_AddVec4(zest_SubVec4(zest_MulVec4(Vec0, Fac0), zest_MulVec4(Vec2, Fac3)), zest_MulVec4(Vec3, Fac4)));
    zest_vec4 Inv2 = zest_Vec4SetVec(zest_AddVec4(zest_SubVec4(zest_MulVec4(Vec0, Fac1), zest_MulVec4(Vec1, Fac3)), zest_MulVec4(Vec3, Fac5)));
    zest_vec4 Inv3 = zest_Vec4SetVec(zest_AddVec4(zest_SubVec4(zest_MulVec4(Vec0, Fac2), zest_MulVec4(Vec1, Fac4)), zest_MulVec4(Vec2, Fac5)));

    zest_vec4 SignA = zest_Vec4Set(+1, -1, +1, -1);
    zest_vec4 SignB = zest_Vec4Set(-1, +1, -1, +1);
    zest_matrix4 inverse = zest_M4SetWithVecs(zest_MulVec4(Inv0, SignA), zest_MulVec4(Inv1, SignB), zest_MulVec4(Inv2, SignA), zest_MulVec4(Inv3, SignB));

    zest_vec4 Row0 = zest_Vec4Set(inverse.v[0].x, inverse.v[1].x, inverse.v[2].x, inverse.v[3].x);

    zest_vec4 Dot0 = zest_Vec4SetVec(zest_MulVec4(m->v[0], Row0));
    float Dot1 = (Dot0.x + Dot0.y) + (Dot0.z + Dot0.w);

    float OneOverDeterminant = 1.f / Dot1;

    return zest_ScaleMatrix4(&inverse, OneOverDeterminant);
}

zest_matrix4 zest_Matrix4RotateX(float angle) {
    float c = cosf(angle);
    float s = sinf(angle);
    zest_matrix4 r =
    { {
        {1, 0, 0, 0},
        {0, c,-s, 0},
        {0, s, c, 0},
        {0, 0, 0, 1}},
    };
    return r;
}

zest_matrix4 zest_Matrix4RotateY(float angle) {
    float c = cosf(angle);
    float s = sinf(angle);
    zest_matrix4 r =
    { {
        { c, 0, s, 0},
        { 0, 1, 0, 0},
        {-s, 0, c, 0},
        { 0, 0, 0, 1}},
    };
    return r;
}

zest_matrix4 zest_Matrix4RotateZ(float angle) {
    float c = cosf(angle);
    float s = sinf(angle);
    zest_matrix4 r =
    { {
        {c, -s, 0, 0},
        {s,  c, 0, 0},
        {0,  0, 1, 0},
        {0,  0, 0, 1}},
    };
    return r;
}

zest_matrix4 zest_CreateMatrix4(float pitch, float yaw, float roll, float x, float y, float z, float sx, float sy, float sz) {
    zest_matrix4 roll_mat = zest_Matrix4RotateZ(roll);
    zest_matrix4 pitch_mat = zest_Matrix4RotateX(pitch);
    zest_matrix4 yaw_mat = zest_Matrix4RotateY(yaw);
    zest_matrix4 matrix = zest_MatrixTransform(&yaw_mat, &pitch_mat);
    matrix = zest_MatrixTransform(&matrix, &roll_mat);
    matrix.v[0].w = x;
    matrix.v[1].w = y;
    matrix.v[2].w = z;
    matrix.v[3].x = sx;
    matrix.v[3].y = sy;
    matrix.v[3].z = sz;
    return matrix;
}

#ifdef ZEST_INTEL

zest_vec4 zest_MatrixTransformVector(zest_matrix4* mat, zest_vec4 vec) {
    zest_vec4 v;

    // Load matrix columns (column-major storage)
    __m128 col0 = _mm_load_ps(&mat->v[0].x);
    __m128 col1 = _mm_load_ps(&mat->v[1].x);
    __m128 col2 = _mm_load_ps(&mat->v[2].x);
    __m128 col3 = _mm_load_ps(&mat->v[3].x);

    // Broadcast each vector component
    __m128 vx = _mm_set1_ps(vec.x);
    __m128 vy = _mm_set1_ps(vec.y);
    __m128 vz = _mm_set1_ps(vec.z);
    __m128 vw = _mm_set1_ps(vec.w);

    // result = col0 * vec.x + col1 * vec.y + col2 * vec.z + col3 * vec.w
    __m128 result = _mm_add_ps(
        _mm_add_ps(_mm_mul_ps(col0, vx), _mm_mul_ps(col1, vy)),
        _mm_add_ps(_mm_mul_ps(col2, vz), _mm_mul_ps(col3, vw))
    );

    _mm_store_ps(&v.x, result);
    return v;
}

zest_matrix4 zest_MatrixTransform(zest_matrix4* left, zest_matrix4* right) {
    zest_matrix4 res = ZEST__ZERO_INIT(zest_matrix4);

    __m128 in_row[4];
    in_row[0] = _mm_load_ps(&right->v[0].x);
    in_row[1] = _mm_load_ps(&right->v[1].x);
    in_row[2] = _mm_load_ps(&right->v[2].x);
    in_row[3] = _mm_load_ps(&right->v[3].x);

    __m128 m_row1 = _mm_set_ps(left->v[3].x, left->v[2].x, left->v[1].x, left->v[0].x);
    __m128 m_row2 = _mm_set_ps(left->v[3].y, left->v[2].y, left->v[1].y, left->v[0].y);
    __m128 m_row3 = _mm_set_ps(left->v[3].z, left->v[2].z, left->v[1].z, left->v[0].z);
    __m128 m_row4 = _mm_set_ps(left->v[3].w, left->v[2].w, left->v[1].w, left->v[0].w);

    for (int r = 0; r <= 3; ++r)
    {

        __m128 row1result = _mm_mul_ps(in_row[r], m_row1);
        __m128 row2result = _mm_mul_ps(in_row[r], m_row2);
        __m128 row3result = _mm_mul_ps(in_row[r], m_row3);
        __m128 row4result = _mm_mul_ps(in_row[r], m_row4);

        float tmp[4];
        _mm_store_ps(tmp, row1result);
        res.v[r].x = tmp[0] + tmp[1] + tmp[2] + tmp[3];
        _mm_store_ps(tmp, row2result);
        res.v[r].y = tmp[0] + tmp[1] + tmp[2] + tmp[3];
        _mm_store_ps(tmp, row3result);
        res.v[r].z = tmp[0] + tmp[1] + tmp[2] + tmp[3];
        _mm_store_ps(tmp, row4result);
        res.v[r].w = tmp[0] + tmp[1] + tmp[2] + tmp[3];

    }
    return res;
}

#elif defined(ZEST_ARM)
zest_vec4 zest_MatrixTransformVector(zest_matrix4* mat, zest_vec4 vec) {
    zest_vec4 v;

    // Load matrix columns (column-major storage)
    float32x4_t col0 = vld1q_f32(&mat->v[0].x);
    float32x4_t col1 = vld1q_f32(&mat->v[1].x);
    float32x4_t col2 = vld1q_f32(&mat->v[2].x);
    float32x4_t col3 = vld1q_f32(&mat->v[3].x);

    // Broadcast each vector component
    float32x4_t vx = vdupq_n_f32(vec.x);
    float32x4_t vy = vdupq_n_f32(vec.y);
    float32x4_t vz = vdupq_n_f32(vec.z);
    float32x4_t vw = vdupq_n_f32(vec.w);

    // result = col0 * vec.x + col1 * vec.y + col2 * vec.z + col3 * vec.w
    float32x4_t result = vaddq_f32(
        vaddq_f32(vmulq_f32(col0, vx), vmulq_f32(col1, vy)),
        vaddq_f32(vmulq_f32(col2, vz), vmulq_f32(col3, vw))
    );

    vst1q_f32(&v.x, result);
    return v;
}

zest_matrix4 zest_MatrixTransform(zest_matrix4* left, zest_matrix4* right) {
    zest_matrix4 res = ZEST__ZERO_INIT(zest_matrix4);

    float32x4_t in_row[4];
    in_row[0] = vld1q_f32(&right->v[0].x);
    in_row[1] = vld1q_f32(&right->v[1].x);
    in_row[2] = vld1q_f32(&right->v[2].x);
    in_row[3] = vld1q_f32(&right->v[3].x);

    float32x4_t m_row1 = vsetq_lane_f32(left->v[0].x, vdupq_n_f32(left->v[1].x), 0);
    m_row1 = vsetq_lane_f32(left->v[2].x, m_row1, 1);
    m_row1 = vsetq_lane_f32(left->v[3].x, m_row1, 2);

    float32x4_t m_row2 = vsetq_lane_f32(left->v[0].y, vdupq_n_f32(left->v[1].y), 0);
    m_row2 = vsetq_lane_f32(left->v[2].y, m_row2, 1);
    m_row2 = vsetq_lane_f32(left->v[3].y, m_row2, 2);

    float32x4_t m_row3 = vsetq_lane_f32(left->v[0].z, vdupq_n_f32(left->v[1].z), 0);
    m_row3 = vsetq_lane_f32(left->v[2].z, m_row3, 1);
    m_row3 = vsetq_lane_f32(left->v[3].z, m_row3, 2);

    float32x4_t m_row4 = vsetq_lane_f32(left->v[0].w, vdupq_n_f32(left->v[1].w), 0);
    m_row4 = vsetq_lane_f32(left->v[2].w, m_row4, 1);
    m_row4 = vsetq_lane_f32(left->v[3].w, m_row4, 2);

    for (int r = 0; r <= 3; ++r)
    {
        float32x4_t row1result = vmulq_f32(in_row[r], m_row1);
        float32x4_t row2result = vmulq_f32(in_row[r], m_row2);
        float32x4_t row3result = vmulq_f32(in_row[r], m_row3);
        float32x4_t row4result = vmulq_f32(in_row[r], m_row4);

        float32x4_t tmp;
        tmp = vaddq_f32(row1result, row2result);
        tmp = vaddq_f32(tmp, row3result);
        tmp = vaddq_f32(tmp, row4result);

        res.v[r].x = vgetq_lane_f32(tmp, 0);
        res.v[r].y = vgetq_lane_f32(tmp, 1);
        res.v[r].z = vgetq_lane_f32(tmp, 2);
        res.v[r].w = vgetq_lane_f32(tmp, 3);
    }
    return res;
}
#endif

zest_vec3 zest_CrossProduct(const zest_vec3 x, const zest_vec3 y)
{
	zest_vec3 result;
	result.x = x.y * y.z - y.y * x.z;
	result.y = x.z * y.x - y.z * x.x;
	result.z = x.x * y.y - y.x * x.y;
    return result;
}

float zest_DotProduct3(const zest_vec3 a, const zest_vec3 b)
{
    return (a.x * b.x + a.y * b.y + a.z * b.z);
}

float zest_DotProduct4(const zest_vec4 a, const zest_vec4 b)
{
    return (a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w);
}

zest_matrix4 zest_LookAt(const zest_vec3 eye, const zest_vec3 center, const zest_vec3 up)
{
    zest_vec3 f = zest_NormalizeVec3(zest_SubVec3(center, eye));
    zest_vec3 s = zest_NormalizeVec3(zest_CrossProduct(f, up));
    zest_vec3 u = zest_CrossProduct(s, f);

    zest_matrix4 result = ZEST__ZERO_INIT(zest_matrix4);
    result.v[0].x = s.x;
    result.v[1].x = s.y;
    result.v[2].x = s.z;
    result.v[0].y = u.x;
    result.v[1].y = u.y;
    result.v[2].y = u.z;
    result.v[0].z = -f.x;
    result.v[1].z = -f.y;
    result.v[2].z = -f.z;
    result.v[3].x = -zest_DotProduct3(s, eye);
    result.v[3].y = -zest_DotProduct3(u, eye);
    result.v[3].z = zest_DotProduct3(f, eye);
    result.v[3].w = 1.f;
    return result;
}

zest_matrix4 zest_Ortho(float left, float right, float bottom, float top, float z_near, float z_far)
{
    zest_matrix4 result = ZEST__ZERO_INIT(zest_matrix4);
    result.v[0].x = 2.f / (right - left);
    result.v[1].y = 2.f / (top - bottom);
    result.v[2].z = -1.f / (z_far - z_near);
    result.v[3].x = -(right + left) / (right - left);
    result.v[3].y = -(top + bottom) / (top - bottom);
    result.v[3].z = -z_near / (z_far - z_near);
    result.v[3].w = 1.f;
    return result;
}

//-- Events and States
zest_bool zest_SwapchainWasRecreated(zest_context context) {
    return ZEST__FLAGGED(context->swapchain->flags, zest_swapchain_flag_was_recreated);
}
//-- End Events and States

//-- Camera and other helpers
zest_camera_t zest_CreateCamera() {
    zest_camera_t camera = ZEST__ZERO_INIT(zest_camera_t);
    camera.yaw = 0.f;
    camera.pitch = 0.f;
    camera.fov = 1.5708f;
    camera.ortho_scale = 5.f;
    camera.up = zest_Vec3Set(0.f, 1.f, 0.f);
    camera.right = zest_Vec3Set(1.f, 0.f, 0.f);
    zest_CameraUpdateFront(&camera);
    return camera;
}

void zest_TurnCamera(zest_camera_t* camera, float turn_x, float turn_y, float sensitivity) {
    camera->yaw -= zest_Radians(turn_x * sensitivity);
    camera->pitch += zest_Radians(turn_y * sensitivity);
    camera->pitch = ZEST__CLAMP(camera->pitch, -1.55334f, 1.55334f);
    zest_CameraUpdateFront(camera);
}

void zest_CameraUpdateFront(zest_camera_t* camera) {
    zest_vec3 direction;
    direction.x = cosf(camera->yaw) * cosf(camera->pitch);
    direction.y = sinf(camera->pitch);
    direction.z = sinf(camera->yaw) * cosf(camera->pitch);
    camera->front = zest_NormalizeVec3(direction);
}

void zest_CameraMoveForward(zest_camera_t* camera, float speed) {
    camera->position = zest_AddVec3(camera->position, zest_ScaleVec3(camera->front, speed));
}

void zest_CameraMoveBackward(zest_camera_t* camera, float speed) {
    camera->position = zest_SubVec3(camera->position, zest_ScaleVec3(camera->front, speed));
}

void zest_CameraMoveUp(zest_camera_t* camera, float speed) {
    zest_vec3 cross = zest_NormalizeVec3(zest_CrossProduct(camera->front, camera->right));
    camera->position = zest_AddVec3(camera->position, zest_ScaleVec3(cross, speed));
}

void zest_CameraMoveDown(zest_camera_t* camera, float speed) {
    zest_vec3 cross = zest_NormalizeVec3(zest_CrossProduct(camera->front, camera->right));
    camera->position = zest_SubVec3(camera->position, zest_ScaleVec3(cross, speed));
}

void zest_CameraStrafLeft(zest_camera_t* camera, float speed) {
    zest_vec3 cross = zest_NormalizeVec3(zest_CrossProduct(camera->front, camera->up));
    camera->position = zest_SubVec3(camera->position, zest_ScaleVec3(cross, speed));
}

void zest_CameraStrafRight(zest_camera_t* camera, float speed) {
    zest_vec3 cross = zest_NormalizeVec3(zest_CrossProduct(camera->front, camera->up));
    camera->position = zest_AddVec3(camera->position, zest_ScaleVec3(cross, speed));
}

void zest_CameraPosition(zest_camera_t* camera, float position[3]) {
	camera->position = { position[0], position[1], position[2] };
}

void zest_CameraSetFoV(zest_camera_t* camera, float degrees) {
    camera->fov = zest_Radians(degrees);
}

void zest_CameraSetPitch(zest_camera_t* camera, float degrees) {
    camera->pitch = zest_Radians(degrees);
}

void zest_CameraSetYaw(zest_camera_t* camera, float degrees) {
    camera->yaw = zest_Radians(degrees);
}

zest_bool zest_RayIntersectPlane(zest_vec3 ray_origin, zest_vec3 ray_direction, zest_vec3 plane, zest_vec3 plane_normal, float* distance, zest_vec3* intersection) {
    float ray_to_plane_normal_dp = zest_DotProduct3(plane_normal, ray_direction);
    if (ray_to_plane_normal_dp >= 0)
        return ZEST_FALSE;
    float d = zest_DotProduct3(plane, plane_normal);
    *distance = (d - zest_DotProduct3(ray_origin, plane_normal)) / ray_to_plane_normal_dp;
    *intersection = zest_ScaleVec3(ray_direction, *distance);
    *intersection = zest_AddVec3(*intersection, ray_origin);
    return ZEST_TRUE;
}

zest_vec3 zest_ScreenRay(float xpos, float ypos, float view_width, float view_height, zest_matrix4* projection, zest_matrix4* view) {
    // converts a position from the 2d xpos, ypos to a normalized 3d direction
    float x = (2.0f * xpos) / view_width - 1.f;
    float y = (2.0f * ypos) / view_height - 1.f;
    zest_vec4 ray_clip = zest_Vec4Set(x, y, -1.f, 1.0f);
    // eye space to clip we would multiply by projection so
    // clip space to eye space is the inverse projection
    zest_matrix4 inverse_projection = zest_Inverse(projection);
    zest_vec4 ray_eye = zest_MatrixTransformVector(&inverse_projection, ray_clip);
    // convert point to forwards
    ray_eye = zest_Vec4Set(ray_eye.x, ray_eye.y, -1.f, 1.0f);
    // world space to eye space is usually multiply by view so
    // eye space to world space is inverse view
    zest_matrix4 inverse_view = zest_Inverse(view);
    zest_vec4 inv_ray_wor = zest_MatrixTransformVector(&inverse_view, ray_eye);
    zest_vec3 ray_wor = zest_Vec3Set(inv_ray_wor.x, inv_ray_wor.y, inv_ray_wor.z);
    return ray_wor;
}

zest_vec2 zest_WorldToScreen(const float point[3], float view_width, float view_height, zest_matrix4* projection, zest_matrix4* view) {
    float w = view->v[0].z * point[0] + view->v[1].z * point[1] + view->v[2].z * point[2] + view->v[3].z;
    // If you try to convert the camera's "from" position to screen space, you will
    // end up dividing by zero (please don't do that)
    //if (w <= 0) return [-1, -1];
    if (w == 0) return zest_Vec2Set(-1.f, -1.f);
    float cx = projection->v[2].x + projection->v[0].x * (view->v[0].x * point[0] + view->v[1].x * point[1] + view->v[2].x * point[2] + view->v[3].x) / w;
    float cy = projection->v[2].y + projection->v[1].y * (view->v[0].y * point[0] + view->v[1].y * point[1] + view->v[2].y * point[2] + view->v[3].y) / w;

    return zest_Vec2Set((0.5f + 0.5f * -cx) * view_width, (0.5f + 0.5f * -cy) * view_height);
}

zest_vec2 zest_WorldToScreenOrtho(const float point[3], float view_width, float view_height, zest_matrix4* projection, zest_matrix4* view) {
    float cx = projection->v[3].x + projection->v[0].x * (view->v[0].x * point[0] + view->v[1].x * point[1] + view->v[2].x * point[2] + view->v[3].x);
    float cy = projection->v[3].y + projection->v[1].y * (view->v[0].y * point[0] + view->v[1].y * point[1] + view->v[2].y * point[2] + view->v[3].y);

    return zest_Vec2Set((0.5f + 0.5f * -cx) * view_width, (0.5f + 0.5f * -cy) * view_height);
}

zest_matrix4 zest_Perspective(float fovy, float aspect, float zNear, float zFar) {
    float const tanHalfFovy = tanf(fovy / 2.f);
    zest_matrix4 result = zest_M4(0.f);
    result.v[0].x = 1.f / (aspect * tanHalfFovy);
    result.v[1].y = 1.f / (tanHalfFovy);
    result.v[2].z = zFar / (zNear - zFar);
    result.v[2].w = -1.f;
    result.v[3].z = -(zFar * zNear) / (zFar - zNear);
    return result;
}

zest_matrix4 zest_TransposeMatrix4(zest_matrix4* mat) {
    zest_matrix4 r;
    r.v[0].x = mat->v[0].x; r.v[0].y = mat->v[1].x; r.v[0].z = mat->v[2].x; r.v[0].w = mat->v[3].x;
    r.v[1].x = mat->v[0].y; r.v[1].y = mat->v[1].y; r.v[1].z = mat->v[2].y; r.v[1].w = mat->v[3].y;
    r.v[2].x = mat->v[0].z; r.v[2].y = mat->v[1].z; r.v[2].z = mat->v[2].z; r.v[2].w = mat->v[3].z;
    r.v[3].x = mat->v[0].w; r.v[3].y = mat->v[1].w; r.v[3].z = mat->v[2].w; r.v[3].w = mat->v[3].w;
    return r;
}

void zest_CalculateFrustumPlanes(zest_matrix4* view_matrix, zest_matrix4* proj_matrix, zest_vec4 planes[6]) {
    zest_matrix4 matrix = zest_MatrixTransform(proj_matrix, view_matrix);
    // Extracting frustum planes from view-projection matrix

    planes[zest_LEFT].x = matrix.v[0].w + matrix.v[0].x;
    planes[zest_LEFT].y = matrix.v[1].w + matrix.v[1].x;
    planes[zest_LEFT].z = matrix.v[2].w + matrix.v[2].x;
    planes[zest_LEFT].w = matrix.v[3].w + matrix.v[3].x;

    planes[zest_RIGHT].x = matrix.v[0].w - matrix.v[0].x;
    planes[zest_RIGHT].y = matrix.v[1].w - matrix.v[1].x;
    planes[zest_RIGHT].z = matrix.v[2].w - matrix.v[2].x;
    planes[zest_RIGHT].w = matrix.v[3].w - matrix.v[3].x;

    planes[zest_TOP].x = matrix.v[0].w - matrix.v[0].y;
    planes[zest_TOP].y = matrix.v[1].w - matrix.v[1].y;
    planes[zest_TOP].z = matrix.v[2].w - matrix.v[2].y;
    planes[zest_TOP].w = matrix.v[3].w - matrix.v[3].y;

    planes[zest_BOTTOM].x = matrix.v[0].w + matrix.v[0].y;
    planes[zest_BOTTOM].y = matrix.v[1].w + matrix.v[1].y;
    planes[zest_BOTTOM].z = matrix.v[2].w + matrix.v[2].y;
    planes[zest_BOTTOM].w = matrix.v[3].w + matrix.v[3].y;

    planes[zest_BACK].x = matrix.v[0].w + matrix.v[0].z;
    planes[zest_BACK].y = matrix.v[1].w + matrix.v[1].z;
    planes[zest_BACK].z = matrix.v[2].w + matrix.v[2].z;
    planes[zest_BACK].w = matrix.v[3].w + matrix.v[3].z;

    planes[zest_FRONT].x = matrix.v[0].w - matrix.v[0].z;
    planes[zest_FRONT].y = matrix.v[1].w - matrix.v[1].z;
    planes[zest_FRONT].z = matrix.v[2].w - matrix.v[2].z;
    planes[zest_FRONT].w = matrix.v[3].w - matrix.v[3].z;

    for (int i = 0; i < 6; ++i) {
        float length = sqrtf(planes[i].x * planes[i].x + planes[i].y * planes[i].y + planes[i].z * planes[i].z);
        planes[i].x /= length;
        planes[i].y /= length;
        planes[i].z /= length;
        planes[i].w /= length;
    }
}

zest_bool zest_IsPointInFrustum(const zest_vec4 planes[6], const float point[3]) {
    for (int i = 0; i < 6; i++) {
        if ((planes[i].x * point[0]) + (planes[i].y * point[1]) + (planes[i].z * point[2]) + planes[i].w < 0)
        {
            return ZEST_FALSE;
        }
    }
    // Point is inside the frustum
    return ZEST_TRUE;
}

zest_bool zest_IsSphereInFrustum(const zest_vec4 planes[6], const float point[3], float radius) {
    for (int i = 0; i < 6; i++) {
        if ((planes[i].x * point[0]) + (planes[i].y * point[1]) + (planes[i].z * point[2]) + planes[i].w <= -radius)
        {
            return ZEST_FALSE;
        }
    }
    // Point is inside the frustum
    return ZEST_TRUE;
}
//-- End camera and other helpers

float zest_Distance(float fromx, float fromy, float tox, float toy) {
    float w = tox - fromx;
    float h = toy - fromy;
    return sqrtf(w * w + h * h);
}

zest_uint zest_Pack16bit2SNorm(float x, float y) {
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

zest_u64 zest_Pack16bit4SNorm(float x, float y, float z, float w) {
    union
    {
        signed short in[4];
        zest_u64 out;
    } u;

    x = x * 32767.f;
    y = y * 32767.f;
    z = z * 32767.f;
    w = w * 32767.f;

    u.in[0] = (signed short)x;
    u.in[1] = (signed short)y;
    u.in[2] = (signed short)z;
    u.in[3] = (signed short)w;

    return u.out;
}

zest_uint zest_FloatToHalf(float f) {
    union {
        float f;
        zest_uint u;
    } u = { f };

    zest_uint sign = (u.u & 0x80000000) >> 16;
    zest_uint rest = u.u & 0x7FFFFFFF;

    if (rest >= 0x47800000) {  // Infinity or NaN
        return (uint16_t)(sign | 0x7C00 | (rest > 0x7F800000 ? 0x0200 : 0));
    }

    if (rest < 0x38800000) {  // Subnormal or zero
        rest = (rest & 0x007FFFFF) | 0x00800000;  // Add leading 1
        int shift = 113 - (rest >> 23);
        rest = (rest << 14) >> shift;
        return (uint16_t)(sign | rest);
    }

    zest_uint exponent = rest & 0x7F800000;
    zest_uint mantissa = rest & 0x007FFFFF;
    return (uint16_t)(sign | ((exponent - 0x38000000) >> 13) | (mantissa >> 13));
}

zest_u64 zest_Pack16bit4SFloat(float x, float y, float z, float w) {
    uint16_t hx = zest_FloatToHalf(x);
    uint16_t hy = zest_FloatToHalf(y);
    uint16_t hz = zest_FloatToHalf(z);
    uint16_t hw = zest_FloatToHalf(w);
    return ((zest_u64)hx) |
        ((zest_u64)hy << 16) |
        ((zest_u64)hz << 32) |
        ((zest_u64)hw << 48);
}

zest_uint zest_Pack16bit2SScaled(float x, float y, float max_value) {
    int16_t x_scaled = (int16_t)(x * 32767.0f / max_value);
    int16_t y_scaled = (int16_t)(y * 32767.0f / max_value);
    return ((zest_uint)x_scaled) | ((zest_uint)y_scaled << 16);
}

zest_u64 zest_Pack16bit4SScaled(float x, float y, float z, float w, float max_value_xy, float max_value_zw) {
    int16_t x_scaled = (int16_t)(x * 32767.0f / max_value_xy);
    int16_t y_scaled = (int16_t)(y * 32767.0f / max_value_xy);
    int16_t z_scaled = (int16_t)(z * 32767.0f / max_value_zw);
    int16_t w_scaled = (int16_t)(w * 32767.0f / max_value_zw);
    return ((zest_u64)x_scaled) | ((zest_u64)y_scaled << 16) | ((zest_u64)z_scaled << 32) | ((zest_u64)w_scaled << 48);
}

zest_u64 zest_Pack16bit4SScaledZWPacked(float x, float y, zest_uint zw, float max_value_xy) {
    int16_t x_scaled = (int16_t)(x * 32767.0f / max_value_xy);
    int16_t y_scaled = (int16_t)(y * 32767.0f / max_value_xy);
    return ((zest_u64)x_scaled) | ((zest_u64)y_scaled << 16) | ((zest_u64)zw << 32);
}

zest_uint zest_Pack8bit(float x, float y, float z) {
    union
    {
        signed char in[4];
        zest_uint out;
    } u;

    x = x * 127.f;
    y = y * 127.f;
    z = z * 127.f;

    u.in[0] = (signed char)x;
    u.in[1] = (signed char)y;
    u.in[2] = (signed char)z;
    u.in[3] = 0;

    return u.out;
}

zest_uint zest_Pack16bitStretch(float x, float y) {
    union
    {
        signed short in[2];
        zest_uint out;
    } u;

    x = x * 655.34f;
    y = y * 32767.f;

    u.in[0] = (signed short)x;
    u.in[1] = (signed short)y;

    return u.out;
}

zest_uint zest_Pack10bit(zest_vec3* v, zest_uint extra) {
    zest_vec3 converted = zest_ScaleVec3(*v, 511.f);
    zest_packed10bit result = ZEST__ZERO_INIT(zest_packed10bit);
    result.pack = 0;
    result.data.x = (zest_uint)converted.z;
    result.data.y = (zest_uint)converted.y;
    result.data.z = (zest_uint)converted.x;
    result.data.w = extra;
    return result.pack;
}

zest_uint zest_Pack8bitx3(zest_vec3* v) {
    zest_vec3 converted = zest_ScaleVec3(*v, 255.f);
    zest_packed8bit result = ZEST__ZERO_INIT(zest_packed8bit);
    result.pack = 0;
    result.data.x = (zest_uint)converted.z;
    result.data.y = (zest_uint)converted.y;
    result.data.z = (zest_uint)converted.x;
    result.data.w = 0;
    return result.pack;
}

zest_size zest_GetNextPower(zest_size n) {
    return 1ULL << (zloc__scan_reverse(n) + 1);
}


zest_size zest_RoundUpToNearestPower(zest_size n) {
	if ((n & (n - 1)) == 0) return n;
	return 1ULL << (zloc__scan_reverse(n) + 1);
}

float zest_Radians(float degrees) { return degrees * 0.01745329251994329576923690768489f; }
float zest_Degrees(float radians) { return radians * 57.295779513082320876798154814105f; }

zest_vec4 zest_LerpVec4(zest_vec4* from, zest_vec4* to, float lerp) {
    return zest_AddVec4(zest_ScaleVec4(*to, lerp), zest_ScaleVec4(*from, (1.f - lerp)));
}

zest_vec3 zest_LerpVec3(zest_vec3* from, zest_vec3* to, float lerp) {
    return zest_AddVec3(zest_ScaleVec3(*to, lerp), zest_ScaleVec3(*from, (1.f - lerp)));
}

zest_vec2 zest_LerpVec2(zest_vec2* from, zest_vec2* to, float lerp) {
    return zest_AddVec2(zest_ScaleVec2(*to, lerp), zest_ScaleVec2(*from, (1.f - lerp)));
}

float zest_Lerp(float from, float to, float lerp) {
    return to * lerp + from * (1.f - lerp);
}
//  --End Math

void zest__register_platform(zest_platform_type type, zest__platform_setup callback) {
	zest__platform_setup_callbacks[type] = callback;
}

// Initialisation and destruction
zest_context zest_CreateContext(zest_device device, zest_window_data_t *window_data, zest_create_context_info_t* info) {
	ZEST_ASSERT_HANDLE(device);		//Not a valid device handle
	zloc_allocator *allocator = device->allocator;
    zest_context context = (zest_context)zloc_Allocate(allocator, sizeof(zest_context_t));
    *context = ZEST__ZERO_INIT(zest_context_t);
	context->magic = zest_INIT_MAGIC(zest_struct_type_context);

	context->device = device;

	context->create_info = *info;
    context->fence_wait_timeout_ns = info->semaphore_wait_timeout_ms * 1000 * 1000;
	context->window_data = *window_data;
	zest_bool result = zest__initialise_context(context, info);
	if (result != ZEST_TRUE) {
		ZEST_ALERT("Unable to initialise the renderer. Check any validation errors.");
		zest__cleanup_context(context);
		return NULL;
	}
    return context;
}

zest_device_builder zest__begin_device_builder() {
    void* memory_pool = ZEST__ALLOCATE_POOL(zloc__MEGABYTE(1));
	zloc_allocator *allocator = zloc_InitialiseAllocatorWithPool(memory_pool, zloc__MEGABYTE(1));
	zest_device_builder builder = (zest_device_builder)ZEST__NEW(allocator, zest_device_builder);
	*builder = ZEST__ZERO_INIT(zest_device_builder_t);
	builder->magic = zest_INIT_MAGIC(zest_struct_type_device_builder);
	builder->platform = zest_platform_vulkan;
	builder->allocator = allocator;
	builder->memory_pool_size = zloc__MEGABYTE(64);
	builder->thread_count = zest_GetDefaultThreadCount();
	builder->log_path = "./";
	builder->cached_shader_path = "./spv/";
	builder->bindless_sampler_count = 64;
	builder->bindless_texture_2d_count = 1024;
	builder->bindless_texture_cube_count = 1024;
	builder->bindless_texture_array_count = 1024;
	builder->bindless_texture_3d_count = 256;
	builder->bindless_storage_buffer_count = 1024;
	builder->bindless_storage_image_count = 1024;
	builder->bindless_uniform_buffer_count = 64;
	builder->max_small_buffer_size = 65536;
	return builder;
}

void zest_AddDeviceBuilderExtension(zest_device_builder builder, const char *extension_name) {
	ZEST_ASSERT_HANDLE(builder);	//Not a valid zest_device_builder handle. Make sure you call zest_Begin[Platform]DeviceBuilder
	size_t len = strlen(extension_name) + 1;
	char* name_copy = (char*)ZEST__ALLOCATE(builder->allocator, len);
	ZEST_ASSERT(name_copy);	//Unable to allocate enough space for the extension name
	memcpy(name_copy, extension_name, len);
	zest_vec_push(builder->allocator, builder->required_instance_extensions, name_copy);
}

void zest_AddDeviceBuilderExtensions(zest_device_builder builder, const char **extension_names, int count) {
	ZEST_ASSERT_HANDLE(builder);	//Not a valid zest_device_builder handle. Make sure you call zest_Begin[Platform]DeviceBuilder
	for (int i = 0; i != count; ++i) {
		const char *extension_name = extension_names[i];
		ZEST_PRINT("Extension: %s", extension_name);
		size_t len = strlen(extension_name) + 1;
		char* name_copy = (char*)ZEST__ALLOCATE(builder->allocator, len);
		ZEST_ASSERT(name_copy);	//Unable to allocate enough space for the extension name
		memcpy(name_copy, extension_name, len);
		zest_vec_push(builder->allocator, builder->required_instance_extensions, name_copy);
	}
}

void zest_AddDeviceBuilderValidation(zest_device_builder builder) {
    ZEST__FLAG(builder->flags, zest_device_init_flag_enable_validation_layers);
	ZEST__FLAG(builder->flags, zest_device_init_flag_enable_validation_layers_with_sync);
}

void zest_AddDeviceBuilderFullValidation(zest_device_builder builder) {
    ZEST__FLAG(builder->flags, zest_device_init_flag_enable_validation_layers);
	ZEST__FLAG(builder->flags, zest_device_init_flag_enable_validation_layers_with_sync);
	ZEST__FLAG(builder->flags, zest_device_init_flag_enable_validation_layers_with_best_practices);
}

void zest_DeviceBuilderPrintMemoryInfo(zest_device_builder builder) {
	ZEST__FLAG(builder->flags, zest_device_init_flag_output_memory_pool_info);
}

void zest_DeviceBuilderLogToConsole(zest_device_builder builder) {
	ZEST__FLAG(builder->flags, zest_device_init_flag_log_validation_errors_to_console);
}

void zest_DeviceBuilderLogToMemory(zest_device_builder builder) {
	ZEST__FLAG(builder->flags, zest_device_init_flag_log_validation_errors_to_memory);
}

void zest_DeviceBuilderLogPath(zest_device_builder builder, const char *log_path) {
	builder->log_path = log_path;
}

void zest_SetDeviceBuilderMemoryPoolSize(zest_device_builder builder, zest_size size) {
	ZEST_ASSERT_HANDLE(builder);	//Not a valid zest_device_builder handle. Make sure you call zest_Begin[Platform]DeviceBuilder
	ZEST_ASSERT(size > zloc__MEGABYTE(2));	//Size for the memory pool must be greater than 2 megabytes
	builder->memory_pool_size = size;
}

zest_device zest_EndDeviceBuilder(zest_device_builder builder) {
	ZEST_ASSERT_HANDLE(builder);	//Not a valid zest_device_builder handle. Make sure you call zest_Begin[Platform]DeviceBuilder

    void* memory_pool = ZEST__ALLOCATE_POOL(builder->memory_pool_size);
	ZEST_ASSERT(memory_pool);    //unable to allocate initial memory pool

    zloc_allocator* allocator = zloc_InitialiseAllocatorWithPool(memory_pool, builder->memory_pool_size);

    zest_device device = (zest_device)zloc_Allocate(allocator, sizeof(zest_device_t));
	allocator->user_data = device;

	*device = ZEST__ZERO_INIT(zest_device_t);
    device->allocator = allocator;
	device->platform = (zest_platform)zloc_Allocate(allocator, sizeof(zest_platform_t));
	device->init_flags = builder->flags;

    device->thread_count = builder->thread_count;

    device->magic = zest_INIT_MAGIC(zest_struct_type_device);
    device->allocator = allocator;
    device->memory_pools[0] = memory_pool;
    device->memory_pool_sizes[0] = builder->memory_pool_size;
    device->memory_pool_count = 1;
    device->setup_info = *builder;
	device->setup_info.allocator = NULL;
    void *scratch_memory = ZEST__ALLOCATE(device->allocator, zloc__MEGABYTE(1));
    int result = zloc_InitialiseLinearAllocator(&device->scratch_arena, scratch_memory, zloc__MEGABYTE(1));
	ZEST_ASSERT(result, "Unable to allocate a scratch arena, out of memory.");
    if (builder->log_path) {
        zest_SetErrorLogPath(device, builder->log_path);
    }

    zest_SetText(device->allocator, &device->cached_shaders_path, builder->cached_shader_path);

	switch (builder->platform) {
		case zest_platform_vulkan: {
			if (!zest__initialise_vulkan_device(device, builder)) {
				ZEST__FREE_POOL(memory_pool);
				return NULL;
			}
			break;
		}
	}

	zest__sync_init(&device->graphics_queues.sync);
	zest__sync_init(&device->compute_queues.sync);
	zest__sync_init(&device->transfer_queues.sync);

	zest__initialise_device_stores(device);

    //Create a global bindless descriptor set for storage buffers and texture samplers
    zest_set_layout_builder_t layout_builder = zest_BeginSetLayoutBuilder(device->allocator);
    zest_AddLayoutBuilderBinding(&layout_builder, ZEST_STRUCT_LITERAL( zest_descriptor_binding_desc_t, zest_sampler_binding, zest_descriptor_type_sampler, builder->bindless_sampler_count, zest_shader_compute_stage | zest_shader_fragment_stage ) );
    zest_AddLayoutBuilderBinding(&layout_builder, ZEST_STRUCT_LITERAL( zest_descriptor_binding_desc_t, zest_texture_2d_binding, zest_descriptor_type_sampled_image, builder->bindless_texture_2d_count, zest_shader_compute_stage | zest_shader_fragment_stage ) );
    zest_AddLayoutBuilderBinding(&layout_builder, ZEST_STRUCT_LITERAL( zest_descriptor_binding_desc_t, zest_texture_cube_binding, zest_descriptor_type_sampled_image, builder->bindless_texture_cube_count, zest_shader_compute_stage | zest_shader_fragment_stage ) );
    zest_AddLayoutBuilderBinding(&layout_builder, ZEST_STRUCT_LITERAL( zest_descriptor_binding_desc_t, zest_texture_array_binding, zest_descriptor_type_sampled_image, builder->bindless_texture_array_count, zest_shader_compute_stage | zest_shader_fragment_stage ) );
    zest_AddLayoutBuilderBinding(&layout_builder, ZEST_STRUCT_LITERAL( zest_descriptor_binding_desc_t, zest_texture_3d_binding, zest_descriptor_type_sampled_image, builder->bindless_texture_3d_count, zest_shader_compute_stage | zest_shader_fragment_stage ) );
    zest_AddLayoutBuilderBinding(&layout_builder, ZEST_STRUCT_LITERAL( zest_descriptor_binding_desc_t, zest_storage_buffer_binding, zest_descriptor_type_storage_buffer, builder->bindless_storage_buffer_count, zest_shader_all_stages ) );
    zest_AddLayoutBuilderBinding(&layout_builder, ZEST_STRUCT_LITERAL( zest_descriptor_binding_desc_t, zest_storage_image_binding, zest_descriptor_type_storage_image, builder->bindless_storage_image_count, zest_shader_compute_stage | zest_shader_fragment_stage ) );
    zest_AddLayoutBuilderBinding(&layout_builder, ZEST_STRUCT_LITERAL( zest_descriptor_binding_desc_t, zest_uniform_buffer_binding, zest_descriptor_type_uniform_buffer, builder->bindless_uniform_buffer_count, zest_shader_compute_stage | zest_shader_vertex_stage | zest_shader_fragment_stage ) );
	device->global_layout_builder = layout_builder;
    device->bindless_set_layout = zest_FinishDescriptorSetLayoutForBindless(device, &device->global_layout_builder, 1, "Zest Global Descriptor Layout");
    device->bindless_set = zest_CreateBindlessSet(device->bindless_set_layout);

	zest_pipeline_layout_info_t pipeline_layout_info = zest_NewPipelineLayoutInfo(device);
	zest_AddPipelineLayoutDescriptorLayout(&pipeline_layout_info, device->bindless_set_layout);
	device->pipeline_layout = zest_CreatePipelineLayout(&pipeline_layout_info);

	zest__create_default_images(device, builder);

    ZEST_APPEND_LOG(device->log_path.str, "Compile shaders");
    zest__compile_builtin_shaders(device);

    ZEST_APPEND_LOG(device->log_path.str, "Create standard pipelines");
	//TODO: remove?
    zest__prepare_standard_pipelines(device);

    device->platform->set_depth_format(device);

	ZEST__FREE_POOL(builder->allocator);
	return device;
}

void zest__create_default_images(zest_device device, zest_device_builder builder) {
	zest_uint pixel = 0xFFFF00FF;
	zest_image_info_t image_info = zest_CreateImageInfo(1, 1);
    image_info.flags = zest_image_preset_texture;
	zest_image_handle image_2d = zest_CreateImageWithPixels(device, &pixel, sizeof(zest_uint), &image_info);
	image_info.layer_count = 6;
	image_info.flags |= zest_image_flag_cubemap;
	zest_image_handle image_cube = zest_CreateImage(device, &image_info);

	device->default_image_2d = zest_GetImage(image_2d);
	device->default_image_cube = zest_GetImage(image_cube);

	zest_buffer_image_copy_t regions[6];
    for(zest_uint i = 0; i != 6; ++i) {
        zest_buffer_image_copy_t buffer_copy_region = ZEST__ZERO_INIT(zest_buffer_image_copy_t);
        buffer_copy_region.image_aspect = zest_image_aspect_color_bit;
        buffer_copy_region.mip_level = 0;
        buffer_copy_region.base_array_layer = 0;
        buffer_copy_region.layer_count = 6;
        buffer_copy_region.image_extent.width = 1;
        buffer_copy_region.image_extent.height = 1;
        buffer_copy_region.image_extent.depth = 1;
        buffer_copy_region.buffer_offset = 0;

		regions[i] = buffer_copy_region;
    }
    zest_size image_size = sizeof(zest_uint);

    zest_buffer staging_buffer = zest_CreateStagingBuffer(device, image_size, &pixel);

	zest_queue queue = zest_imm_BeginCommandBuffer(device, zest_queue_graphics);
	zest_imm_TransitionImage(queue, device->default_image_cube, zest_image_layout_transfer_dst_optimal, 0, 1, 0, 6);
	zest_imm_CopyBufferRegionsToImage(queue, regions, 6, staging_buffer, device->default_image_cube);
    zest_imm_TransitionImage(queue, device->default_image_cube, zest_image_layout_shader_read_only_optimal, 0, 1, 0, 6);
	zest_imm_EndCommandBuffer(queue);

	for (int i = 0; i != builder->bindless_texture_2d_count; i++) {
		device->platform->update_bindless_image_descriptor(device, zest_texture_2d_binding, i, zest_descriptor_type_sampled_image, device->default_image_2d, device->default_image_2d->default_view, 0, device->bindless_set);
	}
	for (int i = 0; i != builder->bindless_texture_array_count; i++) {
		device->platform->update_bindless_image_descriptor(device, zest_texture_array_binding, i, zest_descriptor_type_sampled_image, device->default_image_2d, device->default_image_2d->default_view, 0, device->bindless_set);
	}
	for (int i = 0; i != builder->bindless_texture_3d_count; i++) {
		device->platform->update_bindless_image_descriptor(device, zest_texture_3d_binding, i, zest_descriptor_type_sampled_image, device->default_image_2d, device->default_image_2d->default_view, 0, device->bindless_set);
	}
	for (int i = 0; i != builder->bindless_texture_cube_count; i++) {
		device->platform->update_bindless_image_descriptor(device, zest_texture_cube_binding, i, zest_descriptor_type_sampled_image, device->default_image_cube, device->default_image_2d->default_view, 0, device->bindless_set);
	}
}

zest_bool zest__is_vulkan_device(zest_device device) { 
	return device->setup_info.platform == zest_platform_vulkan; 
}

zest_bool zest__initialise_vulkan_device(zest_device device, zest_device_builder info) {

	if (zest__platform_setup_callbacks[zest_platform_vulkan]) {
		zest__platform_setup_callbacks[zest_platform_vulkan](device->platform);
	} else {
		zloc_Free(device->allocator, device->platform);
		zloc_Free(device->allocator, device);
		ZEST_ALERT("No platform set up function found. Make sure you called the appropriate function for the platform that you want to use like zest_BeginVulkanDevice()");
		return ZEST_FALSE;
	}

    device->backend = (zest_device_backend)device->platform->new_device_backend(device);

	zest_vec_foreach(i, info->required_instance_extensions) {
		char *extension = (char*)info->required_instance_extensions[i];
		zest_vec_push(device->allocator, device->extensions, extension);
	}

	if (device->platform->initialise_device(device)) {
		return ZEST_TRUE;
	}

	zest__cleanup_device(device);
	return ZEST_FALSE;
}

zest_bool zest_BeginFrame(zest_context context) {
	ZEST_ASSERT_OR_VALIDATE(ZEST__NOT_FLAGGED(context->flags, zest_context_flag_swap_chain_was_acquired), context->device, 
							"You have called zest_BeginFrame but a swap chain image has already been acquired. Make sure that you call zest_EndFrame before you loop around to zest_BeginFrame again.",
							ZEST_FALSE);
	ZEST_ASSERT_OR_VALIDATE(context->device_frame_counter < context->device->frame_counter, context->device,
							"zest_UpdateDevice was not called this frame. Make sure you call it at least once each frame before calling zest_BeginFrame.",
							ZEST_FALSE);
	context->device_frame_counter = context->device->frame_counter;
	zest_semaphore_status fence_wait_result = zest__main_loop_semaphore_wait(context);
	if (fence_wait_result == zest_semaphore_status_success) {
	} else if (fence_wait_result == zest_semaphore_status_timeout) {
		ZEST_ALERT("Fence wait timed out.");
		ZEST__FLAG(context->flags, zest_context_flag_critical_error);
		return ZEST_FALSE;
	} else {
		ZEST_ALERT("Critical error when waiting for the main loop fence.");
		ZEST__FLAG(context->flags, zest_context_flag_critical_error);
		return ZEST_FALSE;
	}

	ZEST__UNFLAG(context->flags, zest_context_flag_swap_chain_was_acquired);

	if (context->frame_graph_allocator[context->current_fif].next) {
		//If extra memory was allocated to create a frame graph then consolidate it in to a single memory block.
		//Frame graphs can only be cached if the frame graph is all contained in a single block of memory so that
		//it can easily be promoted to persistent memory.
		zest_size capacity = zloc_GetLinearAllocatorCapacity(&context->frame_graph_allocator[context->current_fif]);
		zloc_linear_allocator_t *allocator = &context->frame_graph_allocator[context->current_fif];
		while (allocator) {
			ZEST__FREE(context->allocator, allocator->data);
			if (allocator != &context->frame_graph_allocator[context->current_fif]) {
				ZEST__FREE(context->allocator, allocator);
			}
			allocator = allocator->next;
		}
		void *frame_graph_linear_memory = ZEST__ALLOCATE(context->allocator, context->create_info.frame_graph_allocator_size);
        int result = zloc_InitialiseLinearAllocator(&context->frame_graph_allocator[context->current_fif], frame_graph_linear_memory, capacity);
		ZEST_ASSERT(result, "Unable to allocate a frame graph allocator, out of memory.");
		zloc_SetLinearAllocatorUserData(&context->frame_graph_allocator[context->current_fif], context);
	} else {
		zloc_ResetLinearAllocator(&context->frame_graph_allocator[context->current_fif]);
	}

	zest__do_context_scheduled_tasks(context);

	if (!context->device->platform->acquire_swapchain_image(context->swapchain)) {
		zest__recreate_swapchain(context);
		ZEST__UNFLAG(context->flags, zest_context_flag_building_frame_graph);
		return ZEST_FALSE;
	}
	ZEST__FLAG(context->flags, zest_context_flag_swap_chain_was_acquired);
	ZEST__UNFLAG(context->swapchain->flags, zest_swapchain_flag_was_recreated);

	return fence_wait_result == zest_semaphore_status_success;
}

void zest_EndFrame(zest_context context, zest_frame_graph frame_graph) {
    ZEST_ASSERT_OR_VALIDATE(ZEST__FLAGGED(context->flags, zest_context_flag_swap_chain_was_acquired),
							context->device, 
							"zest_EndFrame was called but a swap chain image was not acquired. Make sure that zest_BeginFrame was called to acquire a swap chain image.",
							(void)0);
	if (zest__frame_graph_builder) {
		ZEST_ASSERT_OR_VALIDATE(!zest__frame_graph_builder, context->device, 
							"If zest_EndFrame is called then the frame graph builder should be NULL. If it's not NULL then this indicates that zest_BeginFrameGraph was called but no zest_EndFrameGraph was called after to finish and compile the frame graph.",
							(void)0);
	}
	zest_frame_graph_flags flags = 0;
	ZEST__UNFLAG(context->flags, zest_context_flag_work_was_submitted);
    if (ZEST_VALID_HANDLE(frame_graph, zest_struct_type_frame_graph)) {
		if (frame_graph->error_status != zest_fgs_success) {
			ZEST_REPORT(context->device, zest_report_cannot_execute, zest_message_cannot_queue_for_execution, frame_graph->name);
		} else {
			zest_frame_graph_builder_t builder = ZEST__ZERO_INIT(zest_frame_graph_builder_t);
			zest__frame_graph_builder = &builder;
			zest__frame_graph_builder->context = context;
			zest__frame_graph_builder->frame_graph = frame_graph;
			zest__frame_graph_builder->current_pass = 0;
			if (zest__execute_frame_graph(context, frame_graph, ZEST_FALSE)) {
				flags |= frame_graph->flags & zest_frame_graph_present_after_execute;
			}
			zest__frame_graph_builder = NULL;
		}
    } else {
        ZEST_REPORT(context->device, zest_report_no_frame_graphs_to_execute, "WARNING: There were no frame graphs to execute this frame. \n\nIt could just be that you have a condition (like if imgui doesn't return a pass because it has nothing to render yet), in which case this can be ignored.");
    }
	
	if (ZEST_VALID_HANDLE(context->swapchain, zest_struct_type_swapchain) && ZEST__FLAGGED(flags, zest_frame_graph_present_after_execute)) {
		zest_bool presented = context->device->platform->present_frame(context);
		if(!presented) {
			zest__recreate_swapchain(context);
			zest_FlushCachedFrameGraphs(context);
		}
	}

	//Cover some cases where a frame graph wasn't created or it was but there was nothing to render etc., to make sure
	//that the fence is always signalled and another frame can happen
	if (ZEST__NOT_FLAGGED(flags, zest_frame_graph_present_after_execute) && ZEST__FLAGGED(context->flags, zest_context_flag_swap_chain_was_acquired)) {
		if (ZEST__NOT_FLAGGED(context->flags, zest_context_flag_work_was_submitted)) {
			if (context->device->platform->dummy_submit_for_present_only(context)) {
				zest_bool presented = context->device->platform->present_frame(context);
				if (!presented) {
					zest__recreate_swapchain(context);
					zest_FlushCachedFrameGraphs(context);
				}
			}
		}
	}
	ZEST__UNFLAG(context->flags, zest_context_flag_swap_chain_was_acquired);

	for (int i = 0; i != ZEST_QUEUE_COUNT; i++) {
		zest_context_queue context_queue = context->queues[i];
		if (context_queue->queue) {
			zest__release_queue(context_queue->queue);
			context_queue->queue = NULL;
		}
	}
}

void zest_DestroyDevice(zest_device device) {
    zest__destroy_device(device);
}

void zest_SetCreateInfo(zest_context context, zest_create_context_info_t *info) {
    context->create_info = *info;
}

void zest_SetContextUserData(zest_context context, void *user_data) {
	ZEST_ASSERT_HANDLE(context);		//Not a valid context handle
	context->user_data = user_data;
}

void zest_DestroyContext(zest_context context) {
    zest_WaitForIdleDevice(context->device);
    zest__cleanup_context(context);
	ZEST__FREE(context->device->allocator, context);
}

void zest_ResetDevice(zest_device device) {
    zest__cleanup_buffers_in_allocators(device);
	zest__cleanup_pipeline_layout(device->pipeline_layout);
	zest__scan_memory_and_free_resources(device, ZEST_FALSE);
	zest__free_all_device_resource_stores(device);
	zest_ResetValidationErrors(device);

    zest_map_foreach(i, device->reports) {
        zest_report_t *report = &device->reports.data[i];
        zest_FreeText(device->allocator, &report->message);
    }

	zest__free_device_buffer_allocators(device);

	zest_ForEachFrameInFlight(fif) {
		zest_vec_free(device->allocator, device->deferred_resource_freeing_list.resources[fif]);
		zest_vec_free(device->allocator, device->deferred_resource_freeing_list.buffers[fif]);
	}

    zest_vec_free(device->allocator, device->debug.frame_log);
    zest_map_free(device->allocator, device->reports);
    zest_map_free(device->allocator, device->buffer_allocators);

	zest__initialise_device_stores(device);

	device->frame_counter = 0;

	zest_pipeline_layout_info_t pipeline_layout_info = zest_NewPipelineLayoutInfo(device);
	zest_AddPipelineLayoutDescriptorLayout(&pipeline_layout_info, device->bindless_set_layout);
	device->pipeline_layout = zest_CreatePipelineLayout(&pipeline_layout_info);

	zest__create_default_images(device, &device->setup_info);
}

void zest_ResetContext(zest_context context, zest_window_data_t *window_data) {

    zest_WaitForIdleDevice(context->device);
    zest__cleanup_context(context);

	zest_create_context_info_t create_info = context->create_info;
	zest_device device = context->device;
	zest_window_data_t win_dat = context->window_data;
    *context = ZEST__ZERO_INIT(zest_context_t);
	context->magic = zest_INIT_MAGIC(zest_struct_type_context);
	context->device = device;
	context->create_info = create_info;

    context->fence_wait_timeout_ns = create_info.semaphore_wait_timeout_ms * 1000 * 1000;
	context->window_data = win_dat;

	context->frame_counter = 0;

	zest__frame_graph_builder = NULL;

	if (window_data) {
		context->window_data = *window_data;
	}
    zest__initialise_context(context, &context->create_info);
}

//-- End Initialisation and destruction

/*
Functions that create a vulkan device
*/


void zest_AddInstanceExtension(zest_device device, char* extension) {
    zest_vec_push(device->allocator, device->extensions, extension);
}

void zest__initialise_device_stores(zest_device device) {
	for (int i = 0; i != zest_max_device_handle_type; ++i) {
		switch ((zest_device_handle_type)i) {
			case zest_handle_type_images: zest__initialise_store(device->allocator, device, &device->resource_stores[i], sizeof(zest_image_t), zest_struct_type_image); break;
			case zest_handle_type_views: zest__initialise_store(device->allocator, device, &device->resource_stores[i], sizeof(zest_image_view), zest_struct_type_view); break;
			case zest_handle_type_view_arrays: zest__initialise_store(device->allocator, device, &device->resource_stores[i], sizeof(zest_image_view_array), zest_struct_type_view_array); break;
			case zest_handle_type_samplers: zest__initialise_store(device->allocator, device, &device->resource_stores[i], sizeof(zest_sampler_t), zest_struct_type_sampler); break;
			case zest_handle_type_shaders: zest__initialise_store(device->allocator, device, &device->resource_stores[i], sizeof(zest_shader_t), zest_struct_type_shader); break;
			case zest_handle_type_compute_pipelines: zest__initialise_store(device->allocator, device, &device->resource_stores[i], sizeof(zest_compute_t), zest_struct_type_compute); break;
		}
	}
}

void zest__set_default_pool_sizes(zest_device device) {
    zest_buffer_usage_t usage = ZEST__ZERO_INIT(zest_buffer_usage_t);

	//Transient Image type buffers
	//Bear in mind that a pool will be created to cater for each image memory type
    usage.property_flags = zest_memory_property_device_local_bit;
	usage.memory_pool_type = zest_memory_pool_type_transient_images;
    zest_SetDevicePoolSize(device, "Transient Image Buffers", usage, zloc__KILOBYTE(64), zloc__MEGABYTE(64));

	//Buffers
    usage.property_flags = zest_memory_property_device_local_bit;
	usage.memory_pool_type = zest_memory_pool_type_buffers;
	zest_SetDevicePoolSize(device, "Device Buffers", usage, zloc__KILOBYTE(64), zloc__MEGABYTE(32));
	usage.memory_pool_type = zest_memory_pool_type_small_buffers;
	zest_SetDevicePoolSize(device, "Small Device Buffers", usage, zloc__KILOBYTE(1), zloc__MEGABYTE(4));
	usage.memory_pool_type = zest_memory_pool_type_transient_buffers;
	zest_SetDevicePoolSize(device, "Transient Buffers", usage, zloc__KILOBYTE(64), zloc__MEGABYTE(32));
	usage.memory_pool_type = zest_memory_pool_type_small_transient_buffers;
	zest_SetDevicePoolSize(device, "Small Transient Buffers", usage, zloc__KILOBYTE(1), zloc__MEGABYTE(4));

    usage.property_flags = zest_memory_property_host_visible_bit | zest_memory_property_host_coherent_bit;
	usage.memory_pool_type = zest_memory_pool_type_buffers;
    zest_SetDevicePoolSize(device, "Host buffers", usage, zloc__KILOBYTE(64), zloc__MEGABYTE(32));
	usage.memory_pool_type = zest_memory_pool_type_small_buffers;
    zest_SetDevicePoolSize(device, "Small Host buffers", usage, zloc__KILOBYTE(1), zloc__MEGABYTE(4));

    usage.property_flags = zest_memory_property_host_visible_bit | zest_memory_property_host_cached_bit;
	usage.memory_pool_type = zest_memory_pool_type_small_buffers;
    zest_SetDevicePoolSize(device, "GPU Read Buffers", usage, zloc__KILOBYTE(1), zloc__MEGABYTE(4));

    ZEST_APPEND_LOG(device->log_path.str, "Set device pool sizes");
}

/*
End of Device creation functions
*/

void zest__destroy_device(zest_device device) {
	zest_WaitForIdleDevice(device);
	zest__cleanup_device(device);
	zest_ResetValidationErrors(device);
	zloc_allocator *allocator = device->allocator;
	void *memory_pools[ZEST_MAX_DEVICE_MEMORY_POOLS];
	zest_uint memory_pool_count = device->memory_pool_count;
    for (int i = 0; i != memory_pool_count; i++) {
        memory_pools[i] = device->memory_pools[i];
    }
    ZEST__FREE(allocator, device->platform);
    ZEST__FREE(allocator, device);
	zloc_pool_stats_t stats = zloc_CreateMemorySnapshot(zloc__first_block_in_pool(zloc_GetPool(allocator)));
    if (stats.used_blocks > 0) {
        ZEST_ALERT("There are still used memory blocks in a Zest Device, this indicates a memory leak and a possible bug in the Zest Renderer. There should be no used blocks after Zest has shutdown. Check the type of allocation in the list below and check to make sure you're freeing those objects.");
        zest_PrintMemoryBlocks(allocator, zloc__first_block_in_pool(zloc_GetPool(allocator)), 1, zest_platform_none, zest_command_none);
    } else {
		ZEST_PRINT("Successful shutdown of Zest Device.");
    }
	for (int i = 0; i != memory_pool_count; i++) {
        void *pool = memory_pools[i];
		ZEST__FREE_POOL(pool);
	}
}

int zest_UpdateDevice(zest_device device) {
	device->frame_counter++;

	zest_uint index = device->frame_counter % ZEST_MAX_FIF;

	int resources_freed = 0;

    if (zest_vec_size(device->deferred_resource_freeing_list.resources[index])) {
        zest_vec_foreach(i, device->deferred_resource_freeing_list.resources[index]) {
            void *handle = device->deferred_resource_freeing_list.resources[index][i];
			zest__free_handle(device->allocator, handle);
			resources_freed++;
        }
		zest_vec_clear(device->deferred_resource_freeing_list.resources[index]);
    }

	if (zest_vec_size(device->deferred_resource_freeing_list.buffers[index])) {
		zest_vec_foreach(i, device->deferred_resource_freeing_list.buffers[index]) {
			zest_buffer buffer = device->deferred_resource_freeing_list.buffers[index][i];
			zest_FreeBufferNow(buffer);
			resources_freed++;
		}
		zest_vec_clear(device->deferred_resource_freeing_list.buffers[index]);
	}
	return resources_freed;
}

void zest__do_context_scheduled_tasks(zest_context context) {
    for(int i = 0; i != context->active_queue_count; ++i) {
        zest_uint queue_index = context->active_queue_indexes[i];
		zest_context_queue queue = context->queues[i];
		context->device->platform->reset_queue_command_pool(context, queue);
		queue->next_buffer = 0;
    }

    if (zest_vec_size(context->deferred_resource_freeing_list.transient_binding_indexes[context->current_fif])) {
        zest_vec_foreach(i, context->deferred_resource_freeing_list.transient_binding_indexes[context->current_fif]) {
            zest_binding_index_for_release_t index = context->deferred_resource_freeing_list.transient_binding_indexes[context->current_fif][i];
            zest__release_bindless_index(index.layout, index.binding_number, index.binding_index);
        }
		zest_vec_clear(context->deferred_resource_freeing_list.transient_binding_indexes[context->current_fif]);
    }
   
    if (zest_vec_size(context->deferred_resource_freeing_list.transient_images[context->current_fif])) {
        zest_vec_foreach(i, context->deferred_resource_freeing_list.transient_images[context->current_fif]) {
            zest_image image = &context->deferred_resource_freeing_list.transient_images[context->current_fif][i];
            context->device->platform->cleanup_image_backend(image);
            zest_FreeBufferNow((zest_buffer)image->buffer);
            if (image->default_view) {
				context->device->platform->cleanup_image_view_backend(image->default_view);
            }
        }
		zest_vec_clear(context->deferred_resource_freeing_list.transient_images[context->current_fif]);
    }
 
    if (zest_vec_size(context->deferred_resource_freeing_list.transient_view_arrays[context->current_fif])) {
        zest_vec_foreach(i, context->deferred_resource_freeing_list.transient_view_arrays[context->current_fif]) {
            zest_image_view_array view_array = &context->deferred_resource_freeing_list.transient_view_arrays[context->current_fif][i];
            context->device->platform->cleanup_image_view_array_backend(view_array);
        }
		zest_vec_clear(context->deferred_resource_freeing_list.transient_view_arrays[context->current_fif]);
    }

    if (zest_vec_size(context->deferred_resource_freeing_list.resources[context->current_fif])) {
        zest_vec_foreach(i, context->deferred_resource_freeing_list.resources[context->current_fif]) {
            void *handle = context->deferred_resource_freeing_list.resources[context->current_fif][i];
			zest__free_handle(context->allocator, handle);
        }
		zest_vec_clear(context->deferred_resource_freeing_list.resources[context->current_fif]);
    }

	zest_FlushUsedBuffers(context, context->current_fif);
}

zest_semaphore_status zest__main_loop_semaphore_wait(zest_context context) {
	context->frame_counter++;
	context->current_fif = context->frame_counter % ZEST_MAX_FIF;
	if (context->frame_sync_timeline[context->current_fif]) {
		zest_millisecs start_time = zest_Millisecs();
		zest_uint retry_count = 0;
		while(1) {
			zest_semaphore_status result = context->device->platform->wait_for_renderer_semaphore(context);
            if (result == zest_semaphore_status_success) {
                break;
            } else if (result == zest_semaphore_status_timeout) {
				zest_millisecs total_wait_time = zest_Millisecs() - start_time;
				if (context->frame_wait_timeout_callback) {
                    if (context->frame_wait_timeout_callback(total_wait_time, retry_count++, context->user_data)) {
                        continue;
                    } else {
                        return result;
                    }
				} else {
					if (total_wait_time > context->create_info.max_semaphore_timeout_ms) {
                        return result;
					}
				}
			} else {
                return result;
			}
		}
	}
	context->frame_sync_timeline[context->current_fif] = 0;
	return zest_semaphore_status_success;
}

// Enum_to_string_functions - Helper functions to convert enums to strings
const char *zest__image_layout_to_string(zest_image_layout layout) {
    switch (layout) {
		case zest_image_layout_undefined: return "UNDEFINED"; break;
		case zest_image_layout_general: return "GENERAL"; break;
		case zest_image_layout_color_attachment_optimal: return "COLOR_ATTACHMENT_OPTIMAL"; break;
		case zest_image_layout_depth_stencil_attachment_optimal: return "DEPTH_STENCIL_ATTACHMENT_OPTIMAL"; break;
		case zest_image_layout_depth_stencil_read_only_optimal: return "DEPTH_STENCIL_READ_ONLY_OPTIMAL"; break;
		case zest_image_layout_shader_read_only_optimal: return "SHADER_READ_ONLY_OPTIMAL"; break;
		case zest_image_layout_transfer_src_optimal: return "TRANSFER_SRC_OPTIMAL"; break;
		case zest_image_layout_transfer_dst_optimal: return "TRANSFER_DST_OPTIMAL"; break;
		case zest_image_layout_preinitialized: return "PREINITIALIZED"; break;
		case zest_image_layout_depth_read_only_stencil_attachment_optimal: return "DEPTH_READ_ONLY_STENCIL_ATTACHMENT_OPTIMAL"; break;
		case zest_image_layout_depth_attachment_stencil_read_only_optimal: return "DEPTH_ATTACHMENT_STENCIL_READ_ONLY_OPTIMAL"; break;
		case zest_image_layout_depth_attachment_optimal: return "DEPTH_ATTACHMENT_OPTIMAL"; break;
		case zest_image_layout_depth_read_only_optimal: return "DEPTH_READ_ONLY_OPTIMAL"; break;
		case zest_image_layout_stencil_attachment_optimal: return "STENCIL_ATTACHMENT_OPTIMAL"; break;
		case zest_image_layout_stencil_read_only_optimal: return "STENCIL_READ_ONLY_OPTIMAL"; break;
		case zest_image_layout_read_only_optimal: return "READ_ONLY_OPTIMAL"; break;
		case zest_image_layout_attachment_optimal: return "ATTACHMENT_OPTIMAL"; break;
		default: return "Unknown Layout";
    }
}

const char *zest__memory_property_to_string(zest_memory_property_flags property_flags) {
	switch (property_flags) {
		case zest_memory_property_device_local_bit: 
			return "Device Local"; 
			break;
		case zest_memory_property_host_visible_bit | zest_memory_property_host_coherent_bit: 
			return "Host Visible, Host Coherent"; 
			break;
		case zest_memory_property_host_visible_bit | zest_memory_property_host_cached_bit: 
			return "Host Visible, Host Cached"; 
			break;
		default:
			return "Unknown";
	}
}

const char *zest__memory_type_to_string(zest_memory_pool_type memory_type) {
	switch (memory_type) {
		case zest_memory_pool_type_images: return "Images"; break;
		case zest_memory_pool_type_buffers: return "Large Buffers"; break;
		case zest_memory_pool_type_transient_buffers: return "Temporary buffers"; break;
		case zest_memory_pool_type_transient_images: return "Temporary images"; break;
		case zest_memory_pool_type_small_buffers: return "Small buffers."; break;
		case zest_memory_pool_type_small_transient_buffers: return "Temporary small buffers"; break;
		default: return "Unknown";
	}
}

zest_text_t zest__access_flags_to_string(zest_context context, zest_access_flags flags) {
    zest_text_t string = ZEST__ZERO_INIT(zest_text_t);
    if (!flags) {
        zest_AppendTextf(context->device->allocator, &string, "%s", "NONE");
        return string;
    }
    zloc_size flags_field = flags;
    while (flags_field) {
        if (zest_TextSize(&string)) {
            zest_AppendTextf(context->device->allocator, &string, ", ");
        }
        zest_uint index = zloc__scan_forward(flags_field);
        switch (1ull << index) {
			case zest_access_indirect_command_read_bit: zest_AppendTextf(context->device->allocator, &string, "%s", "INDIRECT_COMMAND_READ_BIT"); break;
			case zest_access_index_read_bit: zest_AppendTextf(context->device->allocator, &string, "%s", "INDEX_READ_BIT"); break;
			case zest_access_vertex_attribute_read_bit: zest_AppendTextf(context->device->allocator, &string, "%s", "VERTEX_ATTRIBUTE_READ_BIT"); break;
			case zest_access_uniform_read_bit: zest_AppendTextf(context->device->allocator, &string, "%s", "UNIFORM_READ_BIT"); break;
			case zest_access_input_attachment_read_bit: zest_AppendTextf(context->device->allocator, &string, "%s", "INPUT_ATTACHMENT_READ_BIT"); break;
			case zest_access_shader_read_bit: zest_AppendTextf(context->device->allocator, &string, "%s", "SHADER_READ_BIT"); break;
			case zest_access_shader_write_bit: zest_AppendTextf(context->device->allocator, &string, "%s", "SHADER_WRITE_BIT"); break;
			case zest_access_color_attachment_read_bit: zest_AppendTextf(context->device->allocator, &string, "%s", "COLOR_ATTACHMENT_READ_BIT"); break;
			case zest_access_color_attachment_write_bit: zest_AppendTextf(context->device->allocator, &string, "%s", "COLOR_ATTACHMENT_WRITE_BIT"); break;
			case zest_access_depth_stencil_attachment_read_bit: zest_AppendTextf(context->device->allocator, &string, "%s", "DEPTH_STENCIL_ATTACHMENT_READ_BIT"); break;
			case zest_access_depth_stencil_attachment_write_bit: zest_AppendTextf(context->device->allocator, &string, "%s", "DEPTH_STENCIL_ATTACHMENT_WRITE_BIT"); break;
			case zest_access_transfer_read_bit: zest_AppendTextf(context->device->allocator, &string, "%s", "TRANSFER_READ_BIT"); break;
			case zest_access_transfer_write_bit: zest_AppendTextf(context->device->allocator, &string, "%s", "TRANSFER_WRITE_BIT"); break;
			case zest_access_host_read_bit: zest_AppendTextf(context->device->allocator, &string, "%s", "HOST_READ_BIT"); break;
			case zest_access_host_write_bit: zest_AppendTextf(context->device->allocator, &string, "%s", "HOST_WRITE_BIT"); break;
			case zest_access_memory_read_bit: zest_AppendTextf(context->device->allocator, &string, "%s", "MEMORY_READ_BIT"); break;
			case zest_access_memory_write_bit: zest_AppendTextf(context->device->allocator, &string, "%s", "MEMORY_WRITE_BIT"); break;
			case zest_access_none: zest_AppendTextf(context->device->allocator, &string, "%s", "NONE"); break;
			default: zest_AppendTextf(context->device->allocator, &string, "%s", "Unknown Access Flags"); break;
        }
        flags_field &= ~(1ull << index);
    }
    return string;
}

zest_text_t zest__pipeline_stage_flags_to_string(zest_context context, zest_pipeline_stage_flags flags) {
    zest_text_t string = ZEST__ZERO_INIT(zest_text_t);
    if (!flags) {
        zest_AppendTextf(context->device->allocator, &string, "%s", "NONE");
        return string;
    }
    zloc_size flags_field = flags;
    while (flags_field) {
        if (zest_TextSize(&string)) {
            zest_AppendTextf(context->device->allocator, &string, ", ");
        }
        zest_uint index = zloc__scan_forward(flags_field);
        switch (1ull << index) {
			case zest_pipeline_stage_top_of_pipe_bit: zest_AppendTextf(context->device->allocator, &string, "%s", "TOP_OF_PIPE_BIT"); break;
			case zest_pipeline_stage_draw_indirect_bit: zest_AppendTextf(context->device->allocator, &string, "%s", "DRAW_INDIRECT_BIT"); break;
			case zest_pipeline_stage_vertex_input_bit: zest_AppendTextf(context->device->allocator, &string, "%s", "VERTEX_INPUT_BIT"); break;
			case zest_pipeline_stage_index_input_bit: zest_AppendTextf(context->device->allocator, &string, "%s", "INDEX_INPUT_BIT"); break;
			case zest_pipeline_stage_vertex_shader_bit: zest_AppendTextf(context->device->allocator, &string, "%s", "VERTEX_SHADER_BIT"); break;
			case zest_pipeline_stage_tessellation_control_shader_bit: zest_AppendTextf(context->device->allocator, &string, "%s", "TESSELLATION_CONTROL_SHADER_BIT"); break;
			case zest_pipeline_stage_tessellation_evaluation_shader_bit: zest_AppendTextf(context->device->allocator, &string, "%s", "TESSELLATION_EVALUATION_SHADER_BIT"); break;
			case zest_pipeline_stage_geometry_shader_bit: zest_AppendTextf(context->device->allocator, &string, "%s", "GEOMETRY_SHADER_BIT"); break;
			case zest_pipeline_stage_fragment_shader_bit: zest_AppendTextf(context->device->allocator, &string, "%s", "FRAGMENT_SHADER_BIT"); break;
			case zest_pipeline_stage_early_fragment_tests_bit: zest_AppendTextf(context->device->allocator, &string, "%s", "EARLY_FRAGMENT_TESTS_BIT"); break;
			case zest_pipeline_stage_late_fragment_tests_bit: zest_AppendTextf(context->device->allocator, &string, "%s", "LATE_FRAGMENT_TESTS_BIT"); break;
			case zest_pipeline_stage_color_attachment_output_bit: zest_AppendTextf(context->device->allocator, &string, "%s", "COLOR_ATTACHMENT_OUTPUT_BIT"); break;
			case zest_pipeline_stage_compute_shader_bit: zest_AppendTextf(context->device->allocator, &string, "%s", "COMPUTE_SHADER_BIT"); break;
			case zest_pipeline_stage_transfer_bit: zest_AppendTextf(context->device->allocator, &string, "%s", "TRANSFER_BIT"); break;
			case zest_pipeline_stage_bottom_of_pipe_bit: zest_AppendTextf(context->device->allocator, &string, "%s", "BOTTOM_OF_PIPE_BIT"); break;
			case zest_pipeline_stage_host_bit: zest_AppendTextf(context->device->allocator, &string, "%s", "HOST_BIT"); break;
			case zest_pipeline_stage_all_graphics_bit: zest_AppendTextf(context->device->allocator, &string, "%s", "ALL_GRAPHICS_BIT"); break;
			case zest_pipeline_stage_all_commands_bit: zest_AppendTextf(context->device->allocator, &string, "%s", "ALL_COMMANDS_BIT"); break;
			case zest_pipeline_stage_none: zest_AppendTextf(context->device->allocator, &string, "%s", "NONE"); break;
			default: zest_AppendTextf(context->device->allocator, &string, "%s", "Unknown Pipeline Stage"); break;
        }
        flags_field &= ~(1ull << index);
    }
    return string;
}

// --Threading related functions
unsigned int zest_HardwareConcurrencySafe(void) {
    unsigned int count = zest_HardwareConcurrency();
    return count > 0 ? count : 1;
}

unsigned int zest_GetDefaultThreadCount(void) {
    unsigned int count = zest_HardwareConcurrency();
    return count > 1 ? count - 1 : 1;
}

// --Buffer & Memory Management
void *zest_AllocateMemory(zest_device device, zest_size size) {
    return ZEST__ALLOCATE(device->allocator, size);
}

void zest_FreeMemory(zest_device device, void *allocation) {
    ZEST__FREE(device->allocator, allocation);
}

void* zest__allocate_aligned(zloc_allocator *allocator, zest_size size, zest_size alignment) {
    void* allocation = zloc_AllocateAligned(allocator, size, alignment);
    if (!allocation) {
		zest__add_memory_pool(allocator, size);
        allocation = zloc_AllocateAligned(allocator, size, alignment);
        ZEST_ASSERT(allocation);    //Unable to allocate even after adding a pool
    }
    return allocation;
}

void zest__add_memory_pool(zloc_allocator *allocator, zest_size requested_size) {
	void *user_data = (zest_device)allocator->user_data;
	zest_struct_type struct_type = ZEST_STRUCT_TYPE(user_data);
	zest_size min_pool_size = zest_GetNextPower(requested_size);
	switch (struct_type) {
		case zest_struct_type_device: {
			zest_device device = (zest_device)allocator->user_data;
			zest_size pool_size = ZEST__MAX(device->setup_info.memory_pool_size, min_pool_size);
			if (ZEST__FLAGGED(device->init_flags, zest_device_init_flag_output_memory_pool_info)) {
				ZEST_REPORT(device, zest_report_memory, "Adding a new device memory pool of size %llu", pool_size);
			}
			zest__add_host_memory_pool(device, pool_size);
			break;
		}
		case zest_struct_type_context: {
			zest_context context = (zest_context)allocator->user_data;
			zest_size pool_size = ZEST__MAX(context->create_info.memory_pool_size, min_pool_size);
			if (ZEST__FLAGGED(context->device->init_flags, zest_device_init_flag_output_memory_pool_info)) {
				ZEST_REPORT(context->device, zest_report_memory, "Adding a new context memory pool of size %llu", pool_size);
			}
			zest__add_context_memory_pool(context, pool_size);
			break;
		}
		default:
			break;
	}
}

void zest__add_host_memory_pool(zest_device device, zest_size size) {
	ZEST_ASSERT(device->memory_pool_count < 32);    //Reached the max number of memory pools
	zest_size pool_size = device->setup_info.memory_pool_size;
	if (pool_size <= size) {
		pool_size = zest_GetNextPower(size);
	}
	device->memory_pools[device->memory_pool_count] = ZEST__ALLOCATE_POOL(pool_size);
	ZEST_ASSERT(device->memory_pools[device->memory_pool_count], "Unable to allocate more memory for the device. Out of memory?");
	zloc_AddPool(device->allocator, device->memory_pools[device->memory_pool_count], pool_size);
	device->memory_pool_sizes[device->memory_pool_count] = pool_size;
	device->memory_pool_count++;
	if (ZEST__FLAGGED(device->init_flags, zest_device_init_flag_output_memory_pool_info)) {
		ZEST_REPORT(device, zest_report_memory, "Note: Ran out of space in the device memory pool so adding a new one of size %llu. ", pool_size);
	}
}

void zest__add_context_memory_pool(zest_context context, zest_size size) {
	ZEST_ASSERT(context->memory_pool_count < 32);    //Reached the max number of memory pools for contexts
	zest_size pool_size = context->create_info.memory_pool_size;
	if (pool_size <= size) {
		pool_size = zest_GetNextPower(size);
	}
	context->memory_pools[context->memory_pool_count] = ZEST__ALLOCATE(context->device->allocator, pool_size);
	ZEST_ASSERT(context->memory_pools[context->memory_pool_count], "Unable to allocate more memory for the context. Out of memory?");
	zloc_AddPool(context->allocator, context->memory_pools[context->memory_pool_count], pool_size);
	context->memory_pool_sizes[context->memory_pool_count] = pool_size;
	context->memory_pool_count++;
	if (ZEST__FLAGGED(context->device->init_flags, zest_device_init_flag_output_memory_pool_info)) {
		ZEST_REPORT(context->device, zest_report_memory, "Note: Ran out of space in the context memory pool so adding a new one of size %llu. ", pool_size);
	}
}

void *zest__allocate(zloc_allocator *allocator, zest_size size) {
	void* allocation = zloc_Allocate(allocator, size);
	ptrdiff_t offset_from_allocator = (ptrdiff_t)allocation - (ptrdiff_t)allocator;
	if (offset_from_allocator == 47604744) {
		int d = 0;
	}
	// If there's something that isn't being freed on zest shutdown and it's of an unknown type then 
	// it should print out the offset from the allocator, you can use that offset to break here and 
	// find out what's being allocated.
	if (!allocation) {
		zest__add_memory_pool(allocator, size);
		allocation = zloc_Allocate(allocator, size);
		ZEST_ASSERT(allocation);    //Out of memory? Unable to allocate even after trying to add a new pool
	}
	return allocation;
}

void *zest__reallocate(zloc_allocator *allocator, void *memory, zest_size size) {
	void* allocation = zloc_Reallocate(allocator, memory, size);
	if (!allocation) {
		zest__add_memory_pool(allocator, size);
		allocation = zloc_Reallocate(allocator, memory, size);
		ZEST_ASSERT(allocation);    //Unable to allocate even after adding a pool
	}
	return allocation;
}

zest_size zest__get_largest_slab(zloc_linear_allocator_t *allocator) {
	zest_size size = 0;
	while (allocator) {
		size = ZEST__MAX(size, allocator->buffer_size);
		allocator = allocator->next;
	}
	return size;
}

void *zest__linear_allocate(zloc_linear_allocator_t *allocator, zest_size size) {
	void *data = zloc_LinearAllocation(allocator, size);
	if (!data && allocator->user_data) {
		zest_context context = (zest_context)allocator->user_data;
		zest_size minimum_size = zest_GetNextPower(size);
		zest_size pool_size = ZEST__MAX(zest__get_largest_slab(allocator) * 2, minimum_size);
		void *new_memory = zest__allocate(context->allocator, pool_size);
		ZEST_ASSERT(new_memory, "Out of Memory. Unable to expand a linear allocator in the context.");
		zloc_linear_allocator_t *next = (zloc_linear_allocator_t*)zest__allocate(context->allocator, sizeof(zloc_linear_allocator_t));
		zloc_InitialiseLinearAllocator(next, new_memory, pool_size);
		ZEST_REPORT(context->device, zest_report_memory, "Added a new linear allocator of size %llu.", pool_size);
		zloc_SetLinearAllocatorUserData(next, context);
		zloc_AddNextLinearAllocator(allocator, next);
		data = zloc_LinearAllocation(allocator, size);
	}
	return data;
}

void zest__unmap_memory(zest_device_memory_pool memory_allocation) {
    if (memory_allocation->mapped) {
		zest_device device = memory_allocation->device;
		device->platform->unmap_memory(memory_allocation);
        memory_allocation->mapped = ZEST_NULL;
    }
}

void zest__destroy_memory(zest_device_memory_pool memory_allocation) {
	zest_device device = memory_allocation->device;
    zest_context context = memory_allocation->allocator->context;
	zloc_allocator *allocator = context ? context->allocator : device->allocator;
    device->platform->cleanup_memory_pool_backend(memory_allocation);
    ZEST__FREE(allocator, memory_allocation);
    memory_allocation->mapped = ZEST_NULL;
}

void zest__on_add_pool(void* user_data, void* block) {
    zest_buffer_allocator_t* pools = (zest_buffer_allocator_t*)user_data;
    zest_buffer_t* buffer = (zest_buffer_t*)block;
    buffer->size = pools->memory_pools[zest_vec_last_index(pools->memory_pools)]->size;
    buffer->memory_pool = pools->memory_pools[zest_vec_last_index(pools->memory_pools)];
    buffer->memory_offset = 0;
}

void zest__on_split_block(void* user_data, zloc_header* block, zloc_header* trimmed_block, zloc_size remote_size) {
    zest_buffer_allocator_t* buffer_allocator = (zest_buffer_allocator_t*)user_data;
    zest_buffer buffer = (zest_buffer)zloc_BlockUserExtensionPtr(block);
    zest_buffer trimmed_buffer = (zest_buffer)zloc_BlockUserExtensionPtr(trimmed_block);
	zest_size offset_diff = 0;
    trimmed_buffer->size = buffer->size - remote_size;
    buffer->size = remote_size;
    trimmed_buffer->memory_offset = buffer->memory_offset + buffer->size;
    //--
    trimmed_buffer->memory_pool = buffer->memory_pool;
    trimmed_buffer->memory_pool->allocator = buffer_allocator;
}

void zest__on_reallocation_copy(void* user_data, zloc_header* block, zloc_header* new_block) {
    zest_buffer_allocator buffer_allocator = (zest_buffer_allocator_t*)user_data;
    zest_buffer buffer = (zest_buffer)zloc_BlockUserExtensionPtr(block);
    zest_buffer new_buffer = (zest_buffer)zloc_BlockUserExtensionPtr(new_block);
    if (buffer_allocator->buffer_info.property_flags & zest_memory_property_host_visible_bit) {
        void *new_data = (void*)((char*)new_buffer->memory_pool->mapped + new_buffer->memory_offset);
        void *data = (void*)((char*)buffer->memory_pool->mapped + buffer->memory_offset);
        memcpy(new_data, data, buffer->size);
    }
}

void zest__remote_merge_next_callback(void *user_data, zloc_header *block, zloc_header *next_block) {
    zest_buffer_allocator buffer_allocator = (zest_buffer_allocator)user_data;
	ZEST_ASSERT_HANDLE(buffer_allocator);
    zest_buffer remote_block = (zest_buffer)zloc_BlockUserExtensionPtr(block);
    zest_buffer next_remote_block = (zest_buffer)zloc_BlockUserExtensionPtr(next_block);
    remote_block->size += next_remote_block->size;
    next_remote_block->memory_offset = 0;
    next_remote_block->size = 0;
	zest_device device = buffer_allocator->device;
}

void zest__remote_merge_prev_callback(void *user_data, zloc_header *prev_block, zloc_header *block) {
    zest_buffer_allocator buffer_allocator = (zest_buffer_allocator)user_data;
	ZEST_ASSERT_HANDLE(buffer_allocator);
    zest_buffer remote_block = (zest_buffer)zloc_BlockUserExtensionPtr(block);
    zest_buffer prev_remote_block = (zest_buffer)zloc_BlockUserExtensionPtr(prev_block);
    prev_remote_block->size += remote_block->size;
    remote_block->memory_offset = 0;
    remote_block->size = 0;
	zest_device device = buffer_allocator->device;
}

zest_size zest__get_minimum_block_size(zest_size pool_size) {
    return pool_size > zloc__MEGABYTE(1) ? pool_size / 128 : 256;
}

zest_buffer_allocator zest__create_buffer_allocator(zest_device device, zest_context context, zest_buffer_info_t *buffer_info, zest_key key, zest_key pool_key) {
	zloc_allocator *allocator = context ? context->allocator : device->allocator;
	zest_buffer_allocator buffer_allocator = (zest_buffer_allocator)ZEST__NEW(allocator, zest_buffer_allocator);
	*buffer_allocator = ZEST__ZERO_INIT(zest_buffer_allocator_t);
	buffer_allocator->buffer_info = *buffer_info;
	buffer_allocator->magic = zest_INIT_MAGIC(zest_struct_type_buffer_allocator);
	buffer_allocator->device = device;
	buffer_allocator->context = context;
	zest_buffer_pool_size_t pre_defined_pool_size = ZEST__ZERO_INIT(zest_buffer_pool_size_t);
	if (buffer_info->image_usage_flags) {
		zest_buffer_usage_t usage_key = ZEST__ZERO_INIT(zest_buffer_usage_t);
		usage_key.property_flags = buffer_info->property_flags;
		usage_key.memory_pool_type = zest_memory_pool_type_transient_images;
		zest_key image_key = zest_map_hash_ptr(&usage_key, sizeof(zest_buffer_usage_t));
		pre_defined_pool_size = zest_GetDevicePoolSizeKey(device, image_key);
	} else {
		pre_defined_pool_size = zest_GetDevicePoolSizeKey(device, pool_key);
	}
	ZEST_ASSERT(pre_defined_pool_size.pool_size);
	buffer_allocator->pre_defined_pool_size = pre_defined_pool_size;
	buffer_allocator->backend = (zest_buffer_allocator_backend)device->platform->create_buffer_allocator_backend(device, context, pre_defined_pool_size.pool_size, buffer_info);
	buffer_allocator->name = pre_defined_pool_size.name;
	buffer_allocator->allocator = (zloc_allocator *)ZEST__ALLOCATE(allocator, zloc_AllocatorSize());
	buffer_allocator->allocator = zloc_InitialiseAllocatorForRemote(buffer_allocator->allocator);
	zloc_SetBlockExtensionSize(buffer_allocator->allocator, sizeof(zest_buffer_t));
	zloc_SetMinimumAllocationSize(buffer_allocator->allocator, pre_defined_pool_size.minimum_allocation_size);
	buffer_allocator->allocator->remote_user_data = buffer_allocator;
	buffer_allocator->allocator->add_pool_callback = zest__on_add_pool;
	buffer_allocator->allocator->split_block_callback = zest__on_split_block;
	buffer_allocator->allocator->unable_to_reallocate_callback = zest__on_reallocation_copy;
	buffer_allocator->allocator->merge_next_callback = zest__remote_merge_next_callback;
	buffer_allocator->allocator->merge_prev_callback = zest__remote_merge_prev_callback;
	zest_map_buffer_allocators *buffer_allocators = context ? &context->buffer_allocators : &device->buffer_allocators;
	zest_map_insert_key(allocator, (*buffer_allocators), key, buffer_allocator);
	return buffer_allocator;
}

zest_buffer_linear_allocator zest__create_linear_buffer_allocator(zest_context context, zest_buffer_info_t *buffer_info, zest_size size) {
	zest_buffer_linear_allocator allocator = (zest_buffer_linear_allocator)ZEST__NEW(context->allocator, zest_buffer_linear_allocator);
	allocator->magic = zest_INIT_MAGIC(zest_struct_type_buffer_linear_allocator);
	allocator->context = context;
	allocator->backend = (zest_buffer_linear_allocator_backend)context->device->platform->create_buffer_linear_allocator_backend(context->device, size, buffer_info);
	allocator->buffer_size = size;
	allocator->current_offset = 0;
	return allocator;
}

zest_bool zest__add_gpu_memory_pool(zest_buffer_allocator buffer_allocator, zest_size minimum_size, zest_device_memory_pool *memory_pool) {
	zloc_allocator *allocator = buffer_allocator->context ? buffer_allocator->context->allocator : buffer_allocator->device->allocator;
    zest_device_memory_pool buffer_pool = (zest_device_memory_pool)ZEST__NEW(allocator, zest_device_memory_pool);
	zest_device device = buffer_allocator->device;
    *buffer_pool = ZEST__ZERO_INIT(zest_device_memory_pool_t);
    buffer_pool->magic = zest_INIT_MAGIC(zest_struct_type_device_memory_pool);
	buffer_pool->device = device;
    buffer_pool->backend = (zest_device_memory_pool_backend)device->platform->new_memory_pool_backend(buffer_allocator);
	buffer_pool->allocator = buffer_allocator;
    zest_bool result = ZEST_TRUE;
	buffer_pool->size = buffer_allocator->pre_defined_pool_size.pool_size > minimum_size ? buffer_allocator->pre_defined_pool_size.pool_size : zest_RoundUpToNearestPower(minimum_size);
	zest_uint pool_count = zest_vec_size(buffer_allocator->memory_pools);
	if (pool_count) {
		buffer_pool->size += zest_vec_back(buffer_allocator->memory_pools)->size;
	}
	zest_context context = buffer_allocator->context;
	buffer_pool->minimum_allocation_size = buffer_allocator->pre_defined_pool_size.minimum_allocation_size;
    if (buffer_allocator->buffer_info.buffer_usage_flags) {
        result = device->platform->add_buffer_memory_pool(device, context, buffer_pool->size, buffer_allocator, buffer_pool);
        if (result != ZEST_TRUE) {
            ZEST_APPEND_LOG(device->log_path.str, "Unable to allocate memory for buffer memory pool. Tried to allocate %llu.", buffer_pool->size);
            goto cleanup;
            return result;
        }
    }
    else {
        result = device->platform->create_image_memory_pool(device, context, buffer_pool->size, &buffer_allocator->buffer_info, buffer_pool);
        if (result != ZEST_TRUE) {
            ZEST_APPEND_LOG(device->log_path.str, "Unable to allocate memory for image memory pool. Tried to allocate %llu.", buffer_pool->size);
            goto cleanup;
            return result;
        }
    }
    if (buffer_allocator->buffer_info.property_flags & zest_memory_property_host_visible_bit) {
        device->platform->map_memory(buffer_pool, ZEST_WHOLE_SIZE, 0);
    }
	zest__add_remote_range_pool(buffer_allocator, buffer_pool);
    *memory_pool = buffer_pool;
    return ZEST_TRUE;
    cleanup:
		zest__destroy_memory(buffer_pool);
    return ZEST_FALSE;
}

zest_device_memory zest__create_device_memory(zest_device device, zest_size size, zest_buffer_info_t *buffer_info) {
    zest_device_memory memory = (zest_device_memory)ZEST__NEW(device->allocator, zest_device_memory);
    *memory = ZEST__ZERO_INIT(zest_device_memory_t);
	memory->backend = (zest_device_memory_backend)device->platform->new_memory_backend(device);
	if (!device->platform->create_device_memory(device, size, buffer_info, memory)) {
		device->platform->cleanup_memory_backend(memory);
		ZEST__FREE(device->allocator, memory);
		return NULL;
	}
	memory->device = device;
	return memory;
}

void zest__add_remote_range_pool(zest_buffer_allocator buffer_allocator, zest_device_memory_pool buffer_pool) {
	zest_context context = buffer_allocator->context;
	zest_device device = buffer_allocator->device;
	zloc_allocator *allocator = context ? context->allocator : device->allocator;
    zest_vec_push(allocator, buffer_allocator->memory_pools, buffer_pool);
    zloc_size range_pool_size = zloc_CalculateRemoteBlockPoolSize(buffer_allocator->allocator, buffer_pool->size);
    zest_pool_range* range_pool = (zest_pool_range*)ZEST__ALLOCATE(allocator, range_pool_size);
    zest_vec_push(allocator, buffer_allocator->range_pools, range_pool);
    zloc_AddRemotePool(buffer_allocator->allocator, range_pool, range_pool_size, buffer_pool->size);
}

void zest_FlushUsedBuffers(zest_context context, zest_uint fif) {
	context->device->platform->flush_used_buffers(context, fif);
}

void zest__cleanup_buffers_in_allocators(zest_device device) {
    zest_map_foreach(i, device->buffer_allocators) {
        zest_buffer_allocator allocator = device->buffer_allocators.data[i];
        zest_vec_foreach(j, allocator->range_pools) {
            zest_pool_range pool = allocator->range_pools[j];
            zloc_header *current_block = (zloc_header*)zloc__first_block_in_pool((zloc_pool*)pool);
            while (!zloc__is_last_block_in_pool(current_block)) {
                zest_buffer buffer = (zest_buffer)zloc_BlockUserExtensionPtr(current_block);
                zloc_header *last_block = current_block;
                current_block = zloc__next_physical_block(current_block);
            }
        }
    }
}

void zloc__output_buffer_info(void* ptr, size_t size, int free, void* user, int count)
{
    zest_buffer_t* buffer = (zest_buffer_t*)user;
    printf("%i) \t%s range size: \t%zi \tbuffer size: %llu \toffset: %llu \n", count, free ? "free" : "used", size, buffer->size, buffer->memory_offset);
}

zloc__error_codes zloc_VerifyAllRemoteBlocks(zest_context context, zloc__block_output output_function, void* user_data) {
    zest_map_foreach(i, context->device->buffer_allocators) {
        zest_buffer_allocator allocator = context->device->buffer_allocators.data[i];
        zest_vec_foreach(j, allocator->range_pools) {
            zest_pool_range pool = allocator->range_pools[j];
            zloc_header *current_block = (zloc_header*)zloc__first_block_in_pool((zloc_pool*)pool);
            int count = 0;
            while (!zloc__is_last_block_in_pool(current_block)) {
                void *remote_block = zloc_BlockUserExtensionPtr(current_block);
                if (output_function) {
                    output_function(current_block, zloc__block_size(current_block), zloc__is_free_block(current_block), remote_block, ++count);
                }
                zloc_header *last_block = current_block;
                current_block = zloc__next_physical_block(current_block);
                if (last_block != current_block->prev_physical_block) {
                    ZEST_ASSERT(0);
                    return zloc__PHYSICAL_BLOCK_MISALIGNMENT;
                }
            }
        }
    }
    return zloc__OK;
}

zloc__error_codes zloc_VerifyRemoteBlocks(zloc_header* first_block, zloc__block_output output_function, void* user_data) {
    zloc_header* current_block = first_block;
    int count = 0;
    while (!zloc__is_last_block_in_pool(current_block)) {
        void* remote_block = zloc_BlockUserExtensionPtr(current_block);
        if (output_function) {
            output_function(current_block, zloc__block_size(current_block), zloc__is_free_block(current_block), remote_block, ++count);
        }
        zloc_header* last_block = current_block;
        current_block = zloc__next_physical_block(current_block);
        if (last_block != current_block->prev_physical_block) {
            ZEST_ASSERT(0);
            return zloc__PHYSICAL_BLOCK_MISALIGNMENT;
        }
    }
    return zloc__OK;
}

zloc__error_codes zloc_VerifyBlocks(zloc_header* first_block, zloc__block_output output_function, void* user_data) {
    zloc_header* current_block = first_block;
    while (!zloc__is_last_block_in_pool(current_block)) {
        if (output_function) {
            output_function(current_block, zloc__block_size(current_block), zloc__is_free_block(current_block), user_data, 0);
        }
        zloc_header* last_block = current_block;
        current_block = zloc__next_physical_block(current_block);
        if (last_block != current_block->prev_physical_block) {
            ZEST_ASSERT(0);
            return zloc__PHYSICAL_BLOCK_MISALIGNMENT;
        }
    }
    return zloc__OK;
}

zest_uint zloc_CountBlocks(zloc_header* first_block) {
    zloc_header* current_block = first_block;
    int count = 0;
    while (!zloc__is_last_block_in_pool(current_block)) {
        void* remote_block = zloc_BlockUserExtensionPtr(current_block);
        zloc_header* last_block = current_block;
        current_block = zloc__next_physical_block(current_block);
        ZEST_ASSERT(last_block == current_block->prev_physical_block);
        count++;
    }
    return count;
}

zest_buffer zest__create_transient_buffer(zest_context context, zest_size size, zest_buffer_info_t* buffer_info) {
	zest_device device = context->device;
	zest_buffer_usage_t usage = ZEST_STRUCT_LITERAL(zest_buffer_usage_t, buffer_info->property_flags);
	zest_map_buffer_allocators *buffer_allocators = &context->buffer_allocators;

	if (buffer_info->image_usage_flags) {
		usage.memory_pool_type = zest_memory_pool_type_transient_images;
		usage.alignment = buffer_info->alignment;
	} else if(size > device->setup_info.max_small_buffer_size) {
		usage.memory_pool_type =  zest_memory_pool_type_transient_buffers;
	} else {
		usage.memory_pool_type = zest_memory_pool_type_small_buffers;
	}
	zest_buffer_allocator_key_t usage_key;
	usage_key.usage = usage;
    usage_key.frame_in_flight = buffer_info->frame_in_flight;
    zest_key key = zest_map_hash_ptr(&usage_key, sizeof(zest_buffer_allocator_key_t));
    if (!zest_map_valid_key((*buffer_allocators), key)) {
        //If an allocator doesn't exist yet for this combination of buffer properties then create one.
		zest_key pool_key = zest_map_hash_ptr(&usage, sizeof(zest_buffer_usage_t));
		zest_buffer_allocator buffer_allocator = zest__create_buffer_allocator(device, context, buffer_info, key, pool_key);
		buffer_allocator->usage = usage;
		if (ZEST__FLAGGED(device->init_flags, zest_device_init_flag_output_memory_pool_info)) {
			ZEST_REPORT(device, zest_report_memory, "Creating %s GPU Allocator. Property flags: %s. Intended use: %s.",
					   buffer_allocator->name,
					   zest__memory_property_to_string(usage.property_flags),
					   zest__memory_type_to_string(usage.memory_pool_type)
					   );
		}
        zest_device_memory_pool buffer_pool = 0;
        if (zest__add_gpu_memory_pool(buffer_allocator, size, &buffer_pool) != ZEST_TRUE) {
			return 0;
        }
    }

    zest_buffer_allocator buffer_allocator = *zest_map_at_key((*buffer_allocators), key);
    zest_buffer buffer = (zest_buffer)zloc_AllocateRemote(buffer_allocator->allocator, size);
    if (!buffer) {
        zest_device_memory_pool buffer_pool = 0;
        if(zest__add_gpu_memory_pool(buffer_allocator, size, &buffer_pool) != ZEST_TRUE) {
            return 0;
        }

        buffer = (zest_buffer)zloc_AllocateRemote(buffer_allocator->allocator, size);
        if (!buffer) {    //Unable to allocate memory. Out of memory?
            ZEST_APPEND_LOG(device->log_path.str, "Unable to allocate %llu of memory.", size);
            return 0;
        }
		if (ZEST__FLAGGED(device->init_flags, zest_device_init_flag_output_memory_pool_info)) {
			ZEST_REPORT(device, zest_report_memory, "Note: Ran out of space in the Device memory pool (%s) so adding a new one of size %llu. ", buffer_allocator->name, (size_t)buffer_pool->size);
		}
    }

    return buffer;
}

zest_buffer zest_CreateBuffer(zest_device device, zest_size size, zest_buffer_info_t* buffer_info) {
	ZEST_ASSERT_OR_VALIDATE((buffer_info->property_flags & zest_memory_property_device_local_bit) ||
							(buffer_info->property_flags & (zest_memory_property_host_visible_bit | zest_memory_property_host_coherent_bit)) ||
							(buffer_info->property_flags & (zest_memory_property_host_visible_bit | zest_memory_property_host_cached_bit)),
							device, "Trying to create buffer with invalid memory usage", NULL);
	ZEST_ASSERT_OR_VALIDATE(ZEST__NOT_FLAGGED(buffer_info->flags, zest_memory_pool_flag_transient), 
							device, "Transient buffers are for use in frame graphs only. Just simply zest_FreeBuffer after use if you're done with it.", NULL);
	if (buffer_info->buffer_usage_flags & zest_buffer_usage_uniform_buffer_bit) {
		buffer_info->alignment = ZEST__MAX(buffer_info->alignment, device->min_uniform_buffer_offset_alignment);
		size = zloc__adjust_size(size, size, buffer_info->alignment);
		ZEST_ASSERT_OR_VALIDATE(size <= device->max_uniform_buffer_size, device, "Trying to allocate a uniform buffer large then the maximum size.", NULL);
	}
	zest_buffer_usage_t usage = ZEST_STRUCT_LITERAL(zest_buffer_usage_t, buffer_info->property_flags);
	zest_map_buffer_allocators *buffer_allocators = &device->buffer_allocators;

	if (buffer_info->image_usage_flags) {
		usage.memory_pool_type = zest_memory_pool_type_transient_images;
		usage.alignment = buffer_info->alignment;
	} else if(size > device->setup_info.max_small_buffer_size) {
		usage.memory_pool_type = ZEST__FLAGGED(buffer_info->flags, zest_memory_pool_flag_transient) ? zest_memory_pool_type_transient_buffers : zest_memory_pool_type_buffers;
	} else {
		usage.memory_pool_type = ZEST__FLAGGED(buffer_info->flags, zest_memory_pool_flag_transient) ? zest_memory_pool_type_small_transient_buffers : zest_memory_pool_type_small_buffers;
	}
	zest_buffer_allocator_key_t usage_key;
	usage_key.usage = usage;
    usage_key.frame_in_flight = buffer_info->frame_in_flight;
    zest_key key = zest_map_hash_ptr(&usage_key, sizeof(zest_buffer_allocator_key_t));
    if (!zest_map_valid_key((*buffer_allocators), key)) {
        //If an allocator doesn't exist yet for this combination of buffer properties then create one.
		zest_key pool_key = zest_map_hash_ptr(&usage, sizeof(zest_buffer_usage_t));
		zest_buffer_allocator buffer_allocator = zest__create_buffer_allocator(device, NULL, buffer_info, key, pool_key);
		buffer_allocator->usage = usage;
		if (ZEST__FLAGGED(device->init_flags, zest_device_init_flag_output_memory_pool_info)) {
			ZEST_REPORT(device, zest_report_memory, "Creating %s GPU Allocator. Property flags: %s. Intended use: %s.",
					   buffer_allocator->name,
					   zest__memory_property_to_string(usage.property_flags),
					   zest__memory_type_to_string(usage.memory_pool_type)
					   );
		}
        zest_device_memory_pool buffer_pool = 0;
        if (zest__add_gpu_memory_pool(buffer_allocator, size, &buffer_pool) != ZEST_TRUE) {
			return 0;
        }
    }

    zest_buffer_allocator buffer_allocator = *zest_map_at_key((*buffer_allocators), key);
    zest_buffer buffer = (zest_buffer)zloc_AllocateRemote(buffer_allocator->allocator, size);
    if (!buffer) {
        zest_device_memory_pool buffer_pool = 0;
        if(zest__add_gpu_memory_pool(buffer_allocator, size, &buffer_pool) != ZEST_TRUE) {
            return 0;
        }

        buffer = (zest_buffer)zloc_AllocateRemote(buffer_allocator->allocator, size);
        if (!buffer) {    //Unable to allocate memory. Out of memory?
            ZEST_APPEND_LOG(device->log_path.str, "Unable to allocate %llu of memory.", size);
            return 0;
        }
		if (ZEST__FLAGGED(device->init_flags, zest_device_init_flag_output_memory_pool_info)) {
			ZEST_REPORT(device, zest_report_memory, "Note: Ran out of space in the Device memory pool (%s) so adding a new one of size %llu. ", buffer_allocator->name, (size_t)buffer_pool->size);
		}
    }

    return buffer;
}

zest_buffer zest_CreateStagingBuffer(zest_device device, zest_size size, void* data) {
    zest_buffer_info_t buffer_info = zest_CreateBufferInfo(zest_buffer_type_staging, zest_memory_usage_cpu_to_gpu);
    zest_buffer buffer = zest_CreateBuffer(device, size, &buffer_info);
    if (data) {
        memcpy(zest_BufferData(buffer), data, size);
    }
    return buffer;
}

zest_queue zest__acquire_queue(zest_device device, zest_uint family_index) {
	zest_queue_manager queue_manager = device->queues[family_index];
	zest__sync_lock(&queue_manager->sync);
	zest_uint index = queue_manager->current_queue_index;
	while (queue_manager->queues[index].in_use != 0) { 
		index++;  
		if (index >= queue_manager->queue_count) index = 0;
	}
	queue_manager->current_queue_index = index;
	zest_queue queue = &queue_manager->queues[index];
	queue->in_use = 1;
	zest__sync_unlock(&queue_manager->sync);
	return queue;
}

void zest__release_queue(zest_queue queue) {
	zest_device device = queue->device;
	zest_queue_manager queue_manager = device->queues[queue->family_index];
	zest__sync_lock(&queue_manager->sync);
	queue->in_use = 0;
	queue_manager->current_queue_index = queue->index;
	zest__sync_unlock(&queue_manager->sync);
}

zest_bool zest_imm_TransitionImage(zest_queue queue, zest_image image, zest_image_layout new_layout, zest_uint base_mip_index, zest_uint mip_levels, zest_uint base_array_index, zest_uint layer_count) {
	ZEST_ASSERT_HANDLE(queue);			//Not a valid queue handle
	ZEST_ASSERT_HANDLE(image);			//Not a valid image handle
	mip_levels = ZEST__MIN(mip_levels, image->info.mip_levels);
	layer_count = ZEST__MIN(layer_count, image->info.layer_count);
	zest_device device = queue->device;
	if (device->platform->transition_image_layout(queue, image, new_layout, base_mip_index, mip_levels, base_array_index, layer_count)) {
		image->info.layout = new_layout;
		return ZEST_TRUE;
	}
	return ZEST_FALSE;
}

zest_bool zest_imm_CopyBufferRegionsToImage(zest_queue queue, zest_buffer_image_copy_t *regions, zest_uint regions_count, zest_buffer staging_buffer, zest_image image) {
	ZEST_ASSERT_HANDLE(queue);			//Not a valid queue handle
	if (!regions_count) return ZEST_FALSE;
	return queue->device->platform->copy_buffer_regions_to_image(queue, regions, regions_count, staging_buffer, staging_buffer->memory_offset, image);
}

zest_bool zest_imm_GenerateMipMaps(zest_queue queue, zest_image image) {
	ZEST_ASSERT_HANDLE(queue);			//Not a valid queue handle
	ZEST_ASSERT_OR_VALIDATE(!zest__is_compressed_format(image->info.format), queue->device,
		"Cannot generate mipmaps for compressed formats. Use pre-compressed mipmaps in KTX files instead.", ZEST_FALSE);
	return queue->device->platform->generate_mipmaps(queue, image);
}

zest_bool zest_imm_CopyBuffer(zest_queue queue, zest_buffer src_buffer, zest_buffer dst_buffer, zest_size size) {
	ZEST_ASSERT_HANDLE(queue);					//Not a valid queue handle
    ZEST_ASSERT(size <= src_buffer->size, "Size must be less than or equal to the staging buffer size and the device buffer size");      
    ZEST_ASSERT(size <= dst_buffer->size, "Size must be less than or equal to the staging buffer size and the device buffer size");
	queue->device->platform->cmd_copy_buffer_one_time(queue, src_buffer, dst_buffer, size, 0, 0);
    return ZEST_TRUE;
}

zest_bool zest_imm_CopyBufferRegion(zest_queue queue, zest_buffer src_buffer, zest_size src_offset, zest_buffer dst_buffer, zest_size dst_offset, zest_size size) {
	ZEST_ASSERT_HANDLE(queue);					//Not a valid queue handle
    ZEST_ASSERT(src_offset + size <= src_buffer->size, "Size must be less than or equal to the staging buffer size and the device buffer size whilst also taking into account the offsets");
    ZEST_ASSERT(dst_offset + size <= dst_buffer->size, "Size must be less than or equal to the staging buffer size and the device buffer size whilst also taking into account the offsets");
	queue->device->platform->cmd_copy_buffer_one_time(queue, src_buffer, dst_buffer, size, src_offset, dst_offset);
    return ZEST_TRUE;
}

zest_bool zest_imm_CopyBufferToImage(zest_queue queue, zest_buffer src_buffer, zest_image dst_image, zest_size size) {
	ZEST_ASSERT_HANDLE(queue);						//Not a valid queue handle
    ZEST_ASSERT(size <= src_buffer->size);       	//size must be less than or equal to the staging buffer size and the device buffer size
	zest_buffer buffer = (zest_buffer)dst_image->buffer;
    ZEST_ASSERT(size <= buffer->size);
	queue->device->platform->copy_buffer_to_image(queue, src_buffer, src_buffer->memory_offset, dst_image, dst_image->info.extent.width, dst_image->info.extent.height);
    return ZEST_TRUE;
}

void zest_StageData(void *src_data, zest_buffer dst_staging_buffer, zest_size size) {
    ZEST_ASSERT(src_data);                  //No source data to copy!
    ZEST_ASSERT(size <= dst_staging_buffer->size);  //Staging buffer not large enough
    memcpy(zest_BufferData(dst_staging_buffer), src_data, size);
}

zest_bool zest_GrowBuffer(zest_buffer* buffer, zest_size unit_size, zest_size minimum_bytes) {
    ZEST_ASSERT(unit_size);
    if (minimum_bytes && (*buffer)->size > minimum_bytes) {
        return ZEST_FALSE;
    }
    zest_size units = (*buffer)->size / unit_size;
    zest_size new_size = (units ? units + units / 2 : 8) * unit_size;
    new_size = ZEST__MAX(new_size, minimum_bytes);
    if (new_size <= (*buffer)->size) {
        return ZEST_FALSE;
    }
	zest_device device = (*buffer)->memory_pool->device;
    zest_buffer_allocator_t* buffer_allocator = (*buffer)->memory_pool->allocator;
    zest_buffer new_buffer = (zest_buffer)zloc_ReallocateRemote(buffer_allocator->allocator, *buffer, new_size);
	//Preserve the bindless array index
    if (new_buffer) {
        *buffer = new_buffer;
    }
    else {
        //Create a new memory pool and try again
        zest_device_memory_pool buffer_pool = 0;
        if (zest__add_gpu_memory_pool(buffer_allocator, new_size, &buffer_pool) != ZEST_TRUE) {
            return ZEST_FALSE;
        } else {
            new_buffer = (zest_buffer)zloc_ReallocateRemote(buffer_allocator->allocator, *buffer, new_size);
            ZEST_ASSERT(new_buffer);    //Unable to allocate memory. Out of memory?
            *buffer = new_buffer;
        }
    }
    return new_buffer ? ZEST_TRUE : ZEST_FALSE;
}

zest_bool zest_ResizeBuffer(zest_buffer *buffer, zest_size new_size) {
    ZEST_ASSERT(new_size);
    if ((*buffer)->size > new_size) {
        return ZEST_FALSE;
    }
	zest_device device = (*buffer)->memory_pool->device;
    zest_buffer_allocator_t* buffer_allocator = (*buffer)->memory_pool->allocator;
    zest_buffer new_buffer = (zest_buffer)zloc_ReallocateRemote(buffer_allocator->allocator, *buffer, new_size);
    if (new_buffer) {
        *buffer = new_buffer;
    }
    else {
        //Create a new memory pool and try again
        zest_device_memory_pool buffer_pool = 0;
        if (zest__add_gpu_memory_pool(buffer_allocator, new_size, &buffer_pool) != ZEST_TRUE) {
            return ZEST_FALSE;
        } else {
            new_buffer = (zest_buffer)zloc_ReallocateRemote(buffer_allocator->allocator, *buffer, new_size);
            ZEST_ASSERT(new_buffer);    //Unable to allocate memory. Out of memory?
            *buffer = new_buffer;
        }
    }
    return new_buffer ? ZEST_TRUE : ZEST_FALSE;
}

zest_size zest_GetBufferSize(zest_buffer buffer) {
	ZEST_ASSERT(buffer, "Passed in a NULL buffer when getting the size.");
	return buffer->size;
}

zest_buffer_info_t zest_CreateBufferInfo(zest_buffer_type type, zest_memory_usage usage) {
    zest_buffer_info_t buffer_info = ZEST__ZERO_INIT(zest_buffer_info_t);
	//Implicit src and dst flags
	if (type != zest_buffer_type_uniform) {
		buffer_info.buffer_usage_flags = zest_buffer_usage_transfer_dst_bit | zest_buffer_usage_transfer_src_bit;
	}

	switch (usage) {
		case zest_memory_usage_gpu_only: buffer_info.property_flags = zest_memory_property_device_local_bit; break;
		case zest_memory_usage_cpu_to_gpu: buffer_info.property_flags = zest_memory_property_host_visible_bit | zest_memory_property_host_coherent_bit; break;
		case zest_memory_usage_gpu_to_cpu: buffer_info.property_flags = zest_memory_property_host_visible_bit | zest_memory_property_host_cached_bit; break;
		default: break;
	}

	switch (type) {
		case zest_buffer_type_staging: buffer_info.flags = zest_memory_pool_flag_single_buffer; break;
		case zest_buffer_type_vertex: buffer_info.buffer_usage_flags |= zest_buffer_usage_vertex_buffer_bit; break;
		case zest_buffer_type_index: buffer_info.buffer_usage_flags |= zest_buffer_usage_index_buffer_bit; break;
		case zest_buffer_type_uniform: buffer_info.buffer_usage_flags |= zest_buffer_usage_uniform_buffer_bit; break;
		case zest_buffer_type_storage: buffer_info.buffer_usage_flags |= zest_buffer_usage_storage_buffer_bit; break;
		case zest_buffer_type_indirect: buffer_info.buffer_usage_flags |= zest_buffer_usage_indirect_buffer_bit; break;
		case zest_buffer_type_vertex_storage: buffer_info.buffer_usage_flags |= zest_buffer_usage_storage_buffer_bit | zest_buffer_usage_vertex_buffer_bit; break;
		case zest_buffer_type_index_storage: buffer_info.buffer_usage_flags |= zest_buffer_usage_storage_buffer_bit | zest_buffer_usage_index_buffer_bit; break;
		default: break;
	}

	buffer_info.flags = zest_memory_pool_flag_single_buffer;
    
	return buffer_info;
}

void zest_FreeBuffer(zest_buffer buffer) {
    if (!buffer) return;    		//Nothing to free
	//Get the proxy block in host memory to make sure that the buffer wasn't already freed
	void *proxy_allocation = (char*)buffer - zloc__MINIMUM_BLOCK_SIZE;
	zloc_header *proxy_header = zloc__block_from_allocation(proxy_allocation);
	zloc_bool is_free = zloc__is_free_block(proxy_header);
	if (is_free) {
		return;
	}
	zest_device device = buffer->memory_pool->device;
	zest_uint index = device->frame_counter % ZEST_MAX_FIF;
	zest_vec_push(device->allocator, device->deferred_resource_freeing_list.buffers[index], buffer);
}

void zest_FreeBufferNow(zest_buffer buffer) {
    if (!buffer) return;    		//Nothing to free
	//Get the proxy block in host memory to make sure that the buffer wasn't already freed
	void *proxy_allocation = (char*)buffer - zloc__MINIMUM_BLOCK_SIZE;
	zloc_header *proxy_header = zloc__block_from_allocation(proxy_allocation);
	zloc_bool is_free = zloc__is_free_block(proxy_header);
	if (is_free) {
		return;
	}
    zloc_FreeRemote(buffer->memory_pool->allocator->allocator, buffer);
}

void zest_AddCopyCommand(zest_context context, zest_buffer_uploader_t *uploader, zest_buffer source_buffer, zest_buffer target_buffer, zest_size size) {
    if (uploader->flags & zest_buffer_upload_flag_initialised) {
        ZEST_ASSERT(uploader->source_buffer == source_buffer && uploader->target_buffer == target_buffer);    //Buffer uploads must be to the same source and target ids with each copy. Use a separate BufferUpload for each combination of source and target buffers
    }

    uploader->source_buffer = source_buffer;
    uploader->target_buffer = target_buffer;
    uploader->flags |= zest_buffer_upload_flag_initialised;

    zest_buffer_copy_t buffer_info = ZEST__ZERO_INIT(zest_buffer_copy_t);
    buffer_info.src_offset = source_buffer->memory_offset;
    buffer_info.dst_offset = target_buffer->memory_offset;
    ZEST_ASSERT(size <= target_buffer->size);
    buffer_info.size = size;
    zest_vec_linear_push(&context->frame_graph_allocator[context->current_fif], uploader->buffer_copies, buffer_info);
}

void *zest_BufferData(zest_buffer buffer) {
	ZEST_ASSERT(buffer, "Buffer pointer was null");
	return (void*)((char*)buffer->memory_pool->mapped + buffer->memory_offset);
}

void *zest_BufferDataEnd(zest_buffer buffer) {
	ZEST_ASSERT(buffer, "Buffer pointer was null");
	return (void*)((char*)buffer->memory_pool->mapped + buffer->memory_offset + buffer->size);
}

zest_buffer_pool_size_t zest_GetDevicePoolSizeKey(zest_device device, zest_key hash) {
    if (zest_map_valid_key(device->pool_sizes, hash)) {
        return *zest_map_at_key(device->pool_sizes, hash);
    }
    zest_buffer_pool_size_t pool_size = ZEST__ZERO_INIT(zest_buffer_pool_size_t);
    return pool_size;
}

zest_buffer_pool_size_t zest_GetDevicePoolSize(zest_device device, zest_memory_property_flags property_flags) {
    zest_buffer_usage_t usage;
    usage.property_flags = property_flags;
    zest_key usage_hash = zest_Hash(&usage, sizeof(zest_buffer_usage_t), ZEST_HASH_SEED);
    if (zest_map_valid_key(device->pool_sizes, usage_hash)) {
        return *zest_map_at_key(device->pool_sizes, usage_hash);
    }
    zest_buffer_pool_size_t pool_size = ZEST__ZERO_INIT(zest_buffer_pool_size_t);
    return pool_size;
}

zest_buffer_pool_size_t zest_GetDeviceImagePoolSize(zest_context context, const char* name) {
    zest_key usage_hash = zest_Hash(name, strlen(name), ZEST_HASH_SEED);
    if (zest_map_valid_key(context->device->pool_sizes, usage_hash)) {
        return *zest_map_at_key(context->device->pool_sizes, usage_hash);
    }
    zest_buffer_pool_size_t pool_size = ZEST__ZERO_INIT(zest_buffer_pool_size_t);
    return pool_size;
}

void zest_SetDevicePoolSize(zest_device device, const char *name, zest_buffer_usage_t usage, zest_size minimum_allocation_size, zest_size pool_size) {
    ZEST_ASSERT(pool_size);                    //Must set a pool size
    ZEST_ASSERT(ZEST__POW2(pool_size));        //Pool size must be a power of 2
    zest_key usage_hash = zest_Hash(&usage, sizeof(zest_buffer_usage_t), ZEST_HASH_SEED);
    zest_buffer_pool_size_t pool_sizes;
    pool_sizes.name = name;
    pool_sizes.minimum_allocation_size = minimum_allocation_size;
    pool_sizes.pool_size = pool_size;
    zest_map_insert_key(device->allocator, device->pool_sizes, usage_hash, pool_sizes);
}
// --End Vulkan Buffer Management

// --Renderer and related functions
zest_bool zest__initialise_context(zest_context context, zest_create_context_info_t* create_info) {
    context->flags |= (create_info->flags & zest_context_init_flag_enable_vsync) ? zest_context_flag_vsync_enabled : 0;
    ZEST_APPEND_LOG(context->device->log_path.str, "Create swap chain");

	void *context_memory = ZEST__ALLOCATE(context->device->allocator, create_info->memory_pool_size);
	context->allocator = zloc_InitialiseAllocatorWithPool(context_memory, create_info->memory_pool_size);
    context->allocator->user_data = context;
    context->backend = (zest_context_backend)context->device->platform->new_context_backend(context);

    context->memory_pools[0] = context_memory;
    context->memory_pool_sizes[0] = create_info->memory_pool_size;
    context->memory_pool_count = 1;

	for (int i = 0; i != zest_max_context_handle_type; ++i) {
		switch ((zest_context_handle_type)i) {
			case zest_handle_type_uniform_buffers: 		zest__initialise_store(context->allocator, context, &context->resource_stores[i], sizeof(zest_uniform_buffer_t), zest_struct_type_uniform_buffer); break;
			case zest_handle_type_layers: 				zest__initialise_store(context->allocator, context, &context->resource_stores[i], sizeof(zest_layer_t), zest_struct_type_layer); break;
		}
	}

	if (!context->device->platform->create_window_surface(context)) {
		ZEST_APPEND_LOG(context->device->log_path.str, "Unable to create window surface!");
		ZEST_ALERT("Unable to create window surface!");
		return ZEST_FALSE;
	}
    context->swapchain = zest__create_swapchain(context, create_info->title);

    if (!context->device->platform->initialise_context_backend(context)) {
        return ZEST_FALSE;
    }

	context->active_queue_count = 0;
	zest_vec_foreach(i, context->device->queues) {
		zest_queue_manager_t *queue_manager = context->device->queues[i];
		zest_context_queue context_queue = (zest_context_queue)ZEST__NEW(context->allocator, zest_context_queue);
		*context_queue = ZEST__ZERO_INIT(zest_context_queue_t);
		context_queue->queue_manager = queue_manager;
		context_queue->backend = (zest_context_queue_backend)context->device->platform->new_context_queue_backend(context);
		context->device->platform->initialise_context_queue_backend(context, context_queue);
		switch (queue_manager->type) {
			case zest_queue_graphics: 
				context->queues[ZEST_GRAPHICS_QUEUE_INDEX] = context_queue; 
				context->active_queue_indexes[i] = ZEST_GRAPHICS_QUEUE_INDEX;
				break;
			case zest_queue_compute: 
				context->queues[ZEST_COMPUTE_QUEUE_INDEX] = context_queue;
				context->active_queue_indexes[i] = ZEST_COMPUTE_QUEUE_INDEX;
				break;
			case zest_queue_transfer: 
				context->queues[ZEST_TRANSFER_QUEUE_INDEX] = context_queue;
				context->active_queue_indexes[i] = ZEST_TRANSFER_QUEUE_INDEX;
				break;
		}
		context->active_queue_count++;
	}

	ZEST_ASSERT(context->queues[ZEST_GRAPHICS_QUEUE_INDEX], "Unable to create a graphics queue!");
	if (!context->queues[ZEST_COMPUTE_QUEUE_INDEX]) context->queues[ZEST_COMPUTE_QUEUE_INDEX] = context->queues[ZEST_GRAPHICS_QUEUE_INDEX];
	if (!context->queues[ZEST_TRANSFER_QUEUE_INDEX]) context->queues[ZEST_TRANSFER_QUEUE_INDEX] = context->queues[ZEST_GRAPHICS_QUEUE_INDEX];

    zest_ForEachFrameInFlight(fif) {
		void *frame_graph_linear_memory = ZEST__ALLOCATE(context->allocator, context->create_info.frame_graph_allocator_size);
        int result = zloc_InitialiseLinearAllocator(&context->frame_graph_allocator[fif], frame_graph_linear_memory, context->create_info.frame_graph_allocator_size);
		ZEST_ASSERT(result, "Unable to allocate a frame graph allocator, out of memory.");
		zloc_SetLinearAllocatorUserData(&context->frame_graph_allocator[fif], context);
		zest__initialise_timeline(context->device, &context->frame_timeline[fif]);
    }

    ZEST_APPEND_LOG(context->device->log_path.str, "Finished zest initialisation");

    ZEST__FLAG(context->flags, zest_context_flag_initialised);

    return ZEST_TRUE;
}

zest_swapchain zest__create_swapchain(zest_context context, const char *name) {
    zest_swapchain swapchain = (zest_swapchain)ZEST__NEW(context->allocator, zest_swapchain);
    *swapchain = ZEST__ZERO_INIT(zest_swapchain_t);
    swapchain->magic = zest_INIT_MAGIC(zest_struct_type_swapchain);
    swapchain->backend = (zest_swapchain_backend)context->device->platform->new_swapchain_backend(context);
	swapchain->context = context;
    swapchain->name = name;
	context->swapchain = swapchain;
    if (!context->device->platform->initialise_swapchain(context)) {
        zest__cleanup_swapchain(swapchain);
        return NULL;
    }

    swapchain->resolution.x = 1.f / swapchain->size.width;
    swapchain->resolution.y = 1.f / swapchain->size.height;
    return swapchain;
}

void zest__cleanup_swapchain(zest_swapchain swapchain) {
	zest_context context = swapchain->context;
    context->device->platform->cleanup_swapchain_backend(swapchain);
	context->device->platform->destroy_context_surface(context);
    zest_vec_free(context->allocator, swapchain->images);
    zest_vec_free(context->allocator, swapchain->views);
	ZEST__FREE(context->allocator, swapchain);
}

void zest__cleanup_pipeline(zest_pipeline pipeline) {
	zest_context context = pipeline->context;
	context->device->platform->cleanup_pipeline_backend(pipeline);
	ZEST__FREE(context->allocator, pipeline);
}

void zest__cleanup_pipeline_layout(zest_pipeline_layout layout) {
	zest_device device = layout->device;
	device->platform->cleanup_pipeline_layout_backend(layout);
	ZEST__FREE(device->allocator, layout);
}

void zest__cleanup_pipelines(zest_context context) {
    zest_map_foreach(i, context->cached_pipelines) {
        zest_pipeline pipeline = *zest_map_at_index(context->cached_pipelines, i);
        context->device->platform->cleanup_pipeline_backend(pipeline);
        ZEST__FREE(context->allocator, pipeline);
    }
}

void zest__free_all_device_resource_stores(zest_device device) {
    zest__cleanup_image_store(device);
    zest__cleanup_shader_store(device);
    zest__cleanup_compute_store(device);
    zest__cleanup_sampler_store(device);
    zest__cleanup_view_store(device);
    zest__cleanup_view_array_store(device);
}

void zest__free_device_buffer_allocators(zest_device device) {
    zest_map_foreach(i, device->buffer_allocators) {
        zest_buffer_allocator buffer_allocator = *zest_map_at_index(device->buffer_allocators, i);
		if (ZEST__FLAGGED(device->init_flags, zest_device_init_flag_output_memory_pool_info)) {
			ZEST_PRINT("  Allocator %s. ", buffer_allocator->name);
			ZEST_PRINT("    Property flags: %s. Intended use: %s.",
					   zest__memory_property_to_string(buffer_allocator->usage.property_flags),
					   zest__memory_type_to_string(buffer_allocator->usage.memory_pool_type)
					   );
		}
		zest_size total_size = 0;
        zest_vec_foreach(j, buffer_allocator->memory_pools) {
			zest_device_memory_pool memory_pool = buffer_allocator->memory_pools[j];
			if (ZEST__FLAGGED(device->init_flags, zest_device_init_flag_output_memory_pool_info)) {
				ZEST_PRINT("      Pool %i) Size: %llu (%llu kb, %llu mb), Alignment: %llu", j, memory_pool->size, memory_pool->size / 1024, memory_pool->size / 1024 / 1024, memory_pool->alignment);
			}
			total_size += memory_pool->size;
            zest__destroy_memory(buffer_allocator->memory_pools[j]);
        }
        zest_vec_free(device->allocator, buffer_allocator->memory_pools);
        zest_vec_foreach(j, buffer_allocator->range_pools) {
            ZEST__FREE(device->allocator, buffer_allocator->range_pools[j]);
        }
        zest_vec_free(device->allocator, buffer_allocator->range_pools);
		device->platform->cleanup_buffer_allocator_backend(buffer_allocator);
        ZEST__FREE(device->allocator, buffer_allocator->allocator);
        ZEST__FREE(device->allocator, buffer_allocator);
    }
}

void zest__free_context_buffer_allocators(zest_context context) {
    zest_map_foreach(i, context->buffer_allocators) {
        zest_buffer_allocator buffer_allocator = *zest_map_at_index(context->buffer_allocators, i);
		zest_size total_size = 0;
        zest_vec_foreach(j, buffer_allocator->memory_pools) {
			zest_device_memory_pool memory_pool = buffer_allocator->memory_pools[j];
			if(0) ZEST_PRINT("      Pool %i) Size: %llu (%llu kb, %llu mb), Alignment: %llu", j, memory_pool->size, memory_pool->size / 1024, memory_pool->size / 1024 / 1024, memory_pool->alignment);
			total_size += memory_pool->size;
            zest__destroy_memory(buffer_allocator->memory_pools[j]);
        }
		if (zest_vec_size(buffer_allocator->memory_pools) > 1) {
			if(0) ZEST_ALERT("      More than 1 pool created, consider increasing the pool size for this memory type.");
			if(0) ZEST_ALERT("      Total pool size was %llu (%llu kb, %llu mb).", total_size, total_size / 1024, total_size / 1024 / 1024);
		}
        zest_vec_free(context->allocator, buffer_allocator->memory_pools);
        zest_vec_foreach(j, buffer_allocator->range_pools) {
            ZEST__FREE(context->allocator, buffer_allocator->range_pools[j]);
        }
        zest_vec_free(context->allocator, buffer_allocator->range_pools);
		context->device->platform->cleanup_buffer_allocator_backend(buffer_allocator);
        ZEST__FREE(context->allocator, buffer_allocator->allocator);
        ZEST__FREE(context->allocator, buffer_allocator);
    }
}

void zest__cleanup_device(zest_device device) {
	int stage = 1;
	zest_vec_foreach(i, device->queues) {
		zest_queue_manager_t *queue_manager = device->queues[i];
		zest_vec_foreach(c, queue_manager->queues) {
			zest_queue queue = &queue_manager->queues[c];
			device->platform->cleanup_execution_timeline_backend(&queue->timeline);
			device->platform->cleanup_queue_backend(device, queue);
		}
		zest_vec_free(device->allocator, queue_manager->queues);
	}

	zest_ForEachFrameInFlight(fif) {
		if (zest_vec_size(device->deferred_resource_freeing_list.resources[fif])) {
			zest_vec_foreach(i, device->deferred_resource_freeing_list.resources[fif]) {
				void *handle = device->deferred_resource_freeing_list.resources[fif][i];
				zest__free_handle(device->allocator, handle);
			}
		}
		if (zest_vec_size(device->deferred_resource_freeing_list.buffers[fif])) {
			zest_vec_foreach(i, device->deferred_resource_freeing_list.buffers[fif]) {
				zest_buffer buffer = device->deferred_resource_freeing_list.buffers[fif][i];
				zest_FreeBufferNow(buffer);
			}
		}
		zest_vec_free(device->allocator, device->deferred_resource_freeing_list.resources[fif]);
		zest_vec_free(device->allocator, device->deferred_resource_freeing_list.buffers[fif]);
	}

	device->default_image_2d = NULL;
	device->default_image_cube = NULL;

	zest__cleanup_pipeline_layout(device->pipeline_layout);
    zest__cleanup_buffers_in_allocators(device);
	zest__scan_memory_and_free_resources(device, ZEST_TRUE);
	zest__free_all_device_resource_stores(device);

	zest__cleanup_set_layout(device->bindless_set_layout);
    ZEST__FREE(device->allocator, device->bindless_set->backend);
    ZEST__FREE(device->allocator, device->bindless_set);
	zest__release_all_image_indexes(device);

	zest_map_foreach(i, device->mip_indexes) {
		zest_mip_index_collection *collection = &device->mip_indexes.data[i];
		zest_uint active_bindings = collection->binding_numbers;
		zest_uint current_binding = zloc__scan_reverse(active_bindings);
		while (current_binding >= 0) {
			zest_vec_foreach(i, collection->mip_indexes[current_binding]) {
				zest_uint index = collection->mip_indexes[current_binding][i];
				zest_ReleaseBindlessIndex(device, index, (zest_binding_number_type)current_binding);
			}
			zest_vec_free(device->allocator, collection->mip_indexes[current_binding]);
			active_bindings &= ~(1 << current_binding);
			current_binding = zloc__scan_reverse(active_bindings);
		}
	}

	zest__sync_cleanup(&device->graphics_queues.sync);
	zest__sync_cleanup(&device->compute_queues.sync);
	zest__sync_cleanup(&device->transfer_queues.sync);

	for (int i = 0; i != zest_max_device_handle_type; ++i) {
		switch ((zest_device_handle_type)i) {
			case zest_handle_type_images: zest__free_store(&device->resource_stores[i]); break;
			case zest_handle_type_views: zest__free_store(&device->resource_stores[i]); break;
			case zest_handle_type_view_arrays: zest__free_store(&device->resource_stores[i]); break;
			case zest_handle_type_samplers: zest__free_store(&device->resource_stores[i]); break;
			case zest_handle_type_shaders: zest__free_store(&device->resource_stores[i]); break;
			case zest_handle_type_compute_pipelines: zest__free_store(&device->resource_stores[i]); break;
		}
	}

	zest_vec_free(device->allocator, device->global_layout_builder.bindings);

    zest_map_foreach(i, device->reports) {
        zest_report_t *report = &device->reports.data[i];
        zest_FreeText(device->allocator, &report->message);
    }

	zest__free_device_buffer_allocators(device);

	device->platform->cleanup_device_backend(device);
    ZEST__FREE(device->allocator, device->scratch_arena.data);

    zest_vec_free(device->allocator, device->debug.frame_log);
    zest_map_free(device->allocator, device->reports);
    zest_map_free(device->allocator, device->buffer_allocators);
    zest_vec_free(device->allocator, device->extensions);
    zest_vec_free(device->allocator, device->queues);
    zest_map_free(device->allocator, device->queue_names);
    zest_map_free(device->allocator, device->pool_sizes);
    zest_FreeText(device->allocator, &device->log_path);
    zest_FreeText(device->allocator, &device->cached_shaders_path);
}

void zest__free_handle(zloc_allocator *allocator, void *handle) {
    zest_struct_type struct_type = (zest_struct_type)ZEST_STRUCT_TYPE(handle);
    switch (struct_type) {
		case zest_struct_type_pipeline_template: {
			zest__cleanup_pipeline_template((zest_pipeline_template)handle);
			break;
		}
		case zest_struct_type_image: {
			zest_image image = (zest_image)handle;
			zest__cleanup_image((zest_image)handle);
			break;
		}
		case zest_struct_type_view: {
			zest_image_view view = (zest_image_view)handle;
			zest__cleanup_image_view(view);
			break;
		}
		case zest_struct_type_view_array: {
			zest_image_view_array view = (zest_image_view_array)handle;
			zest__cleanup_image_view_array(view);
			break;
		}
		case zest_struct_type_sampler: {
			zest_sampler sampler = (zest_sampler)handle;
			zest__cleanup_sampler(sampler);
			break;
		}
		case zest_struct_type_layer: {
			zest_layer layer = (zest_layer)handle;
			zest__cleanup_layer(layer);
			break;
		}
		case zest_struct_type_uniform_buffer: {
			zest_uniform_buffer uniform_buffer = (zest_uniform_buffer)handle;
			zest__cleanup_uniform_buffer(uniform_buffer);
			break;
		}
		case zest_struct_type_execution_timeline: {
			zest_execution_timeline timeline = (zest_execution_timeline)handle;
			zest__cleanup_execution_timeline(timeline);
			break;
		}
		case zest_struct_type_pipeline_layout: {
			zest_pipeline_layout layout = (zest_pipeline_layout)handle;
			zest__cleanup_pipeline_layout(layout);
			break;
		}
		case zest_struct_type_context: {
			zest_context context = (zest_context)handle;
			zest_PrintReports(context);
			zest__cleanup_context(context);
			ZEST__FREE(context->device->allocator, context);
			break;
		}
    }
}

void zest__scan_memory_and_free_resources(void *origin, zest_bool including_context) {
	void **memory_pools = 0;
	zest_uint memory_pool_count;
	zloc_allocator *allocator = 0;
	if (ZEST_STRUCT_TYPE(origin) == zest_struct_type_device) {
		zest_device device = (zest_device)origin;
		memory_pools = device->memory_pools;
		memory_pool_count = device->memory_pool_count;
		allocator = device->allocator;
	} else {
		zest_context context = (zest_context)origin;
		memory_pools = context->memory_pools;
		memory_pool_count = context->memory_pool_count;
		allocator = context->allocator;
	}
    zloc_pool_stats_t stats = zloc_CreateMemorySnapshot(zloc__first_block_in_pool(zloc_GetPool(allocator)));
    if (stats.used_blocks == 0) {
        return;
    }
    void **memory_to_free = 0;
    zest_vec_reserve(allocator, memory_to_free, (zest_uint)stats.used_blocks);
	zloc_header *current_block = 0;
	for (int i = 0; i != memory_pool_count; i++) {
		if (i == 0) {
			current_block = zloc__first_block_in_pool(zloc_GetPool(allocator));
		} else {
			current_block = zloc__first_block_in_pool((zloc_pool*)memory_pools[i]);
		}
		while (!zloc__is_last_block_in_pool(current_block)) {
			if (!zloc__is_free_block(current_block)) {
				zest_size block_size = zloc__block_size(current_block);
				void *allocation = (void *)((char *)current_block + zloc__BLOCK_POINTER_OFFSET);
				if (ZEST_VALID_IDENTIFIER(allocation)) {
					zest_struct_type struct_type = (zest_struct_type)ZEST_STRUCT_TYPE(allocation);
					switch (struct_type) {
						case zest_struct_type_pipeline_template:
							zest_vec_push(allocator, memory_to_free, allocation);
							break;
						case zest_struct_type_execution_timeline:
							zest_vec_push(allocator, memory_to_free, allocation);
							break;
						case zest_struct_type_buffer_backend:
							zest_vec_push(allocator, memory_to_free, allocation);
							break;
						case zest_struct_type_pipeline_layout:
							zest_vec_push(allocator, memory_to_free, allocation);
							break;
						case zest_struct_type_context:
							if (including_context) {
								zest_vec_push(allocator, memory_to_free, allocation);
							}
							break;
					}
				}
			}
			current_block = zloc__next_physical_block(current_block);
		}
	}
    zest_vec_foreach(i, memory_to_free) {
        void *allocation = memory_to_free[i];
        zest__free_handle(allocator, allocation);
    }
    zest_vec_free(allocator, memory_to_free);
}

void zest__cleanup_image_store(zest_device device) {
	zest_resource_store_t *store = &device->resource_stores[zest_handle_type_images];
    for (int i = 0; i != store->data.current_size; ++i) {
		if (zest__resource_is_initialised(store, i)) {
			zest_image image = zest_bucket_array_get(&store->data, zest_image_t, i);
			if (ZEST_VALID_HANDLE(image, zest_struct_type_image)) {
				zest__cleanup_image(image);
			}
		}
    }
	zest__free_store(store);
}

void zest__cleanup_sampler_store(zest_device device) {
	zest_resource_store_t *store = &device->resource_stores[zest_handle_type_samplers];
    for (int i = 0; i != store->data.current_size; ++i) {
		if (zest__resource_is_initialised(store, i)) {
			zest_sampler sampler = zest_bucket_array_get(&store->data, zest_sampler_t, i);
			if (ZEST_VALID_HANDLE(sampler, zest_struct_type_sampler)) {
				zest__cleanup_sampler(sampler);
			}
		}
    }
	zest__free_store(store);
}

void zest__cleanup_uniform_buffer_store(zest_context context) {
	zest_resource_store_t *store = &context->resource_stores[zest_handle_type_uniform_buffers];
    for (int i = 0; i != store->data.current_size; ++i) {
		if (zest__resource_is_initialised(store, i)) {
			zest_uniform_buffer buffer = zest_bucket_array_get(&store->data, zest_uniform_buffer_t, i);
			if (ZEST_VALID_HANDLE(buffer, zest_struct_type_uniform_buffer)) {
				zest__cleanup_uniform_buffer(buffer);
			}
		}
    }
	zest__free_store(store);
}

void zest__cleanup_layer_store(zest_context context) {
	zest_resource_store_t *store = &context->resource_stores[zest_handle_type_layers];
    for (int i = 0; i != store->data.current_size; ++i) {
		if (zest__resource_is_initialised(store, i)) {
			zest_layer layer = zest_bucket_array_get(&store->data, zest_layer_t, i);
			if (ZEST_VALID_HANDLE(layer, zest_struct_type_layer)) {
				zest__cleanup_layer(layer);
			}
		}
    }
	zest__free_store(store);
}

void zest__cleanup_shader_store(zest_device device) {
	zest_resource_store_t *store = &device->resource_stores[zest_handle_type_shaders];
    for (int i = 0; i != store->data.current_size; ++i) {
		if (zest__resource_is_initialised(store, i)) {
			zest_shader shader = zest_bucket_array_get(&store->data, zest_shader_t, i);
			if (ZEST_VALID_HANDLE(shader, zest_struct_type_shader)) {
				zest_FreeShader(shader->handle);
			}
		}
    }
	zest__free_store(store);
}

void zest__cleanup_compute_store(zest_device device) {
	zest_resource_store_t *store = &device->resource_stores[zest_handle_type_compute_pipelines];
    for (int i = 0; i != store->data.current_size; ++i) {
		if (zest__resource_is_initialised(store, i)) {
			zest_compute compute = zest_bucket_array_get(&store->data, zest_compute_t, i);
			if (ZEST_VALID_HANDLE(compute, zest_struct_type_compute)) {
				zest__cleanup_compute(compute);
			}
		}
    }
	zest__free_store(store);
}

void zest__cleanup_view_store(zest_device device) {
	zest_resource_store_t *store = &device->resource_stores[zest_handle_type_views];
    for (int i = 0; i != store->data.current_size; ++i) {
		if (zest__resource_is_initialised(store, i)) {
			zest_image_view *view = (zest_image_view *)zest__bucket_array_get(&store->data, i);
			if (ZEST_VALID_HANDLE(*view, zest_struct_type_view)) {
				zest__cleanup_image_view(*view);
			}
		}
    }
	zest__free_store(store);
}

void zest__cleanup_view_array_store(zest_device device) {
	zest_resource_store_t *store = &device->resource_stores[zest_handle_type_view_arrays];
    for (int i = 0; i != store->data.current_size; ++i) {
		if (zest__resource_is_initialised(store, i)) {
			zest_image_view_array *view = (zest_image_view_array *)zest__bucket_array_get(&store->data, i);
			if (ZEST_VALID_HANDLE(*view, zest_struct_type_view_array)) {
				zest__cleanup_image_view_array(*view);
			}
		}
    }
	zest__free_store(store);
}

void zest__cleanup_context(zest_context context) {
    zest__cleanup_uniform_buffer_store(context);
    zest__cleanup_layer_store(context);
	zest__cleanup_swapchain(context->swapchain);
    zest__cleanup_pipelines(context);

	for (int i = 0; i != zest_max_context_handle_type; ++i) {
		zest__free_store(&context->resource_stores[i]); 
	}

	zest__scan_memory_and_free_resources(context, ZEST_FALSE);

    zest_map_foreach(i, context->cached_frame_graph_semaphores) {
        zest_frame_graph_semaphores semaphores = context->cached_frame_graph_semaphores.data[i];
        context->device->platform->cleanup_frame_graph_semaphore(context, semaphores);
        ZEST__FREE(context->allocator, semaphores);
    }
	
    for(int i = 0; i != context->active_queue_count; ++i) {
        zest_uint queue_index = context->active_queue_indexes[i];
		zest_context_queue queue = context->queues[i];
		context->device->platform->cleanup_context_queue_backend(context, queue);
		ZEST__FREE(context->allocator, queue);
    }

    zest_ForEachFrameInFlight(fif) {
		zest_FlushUsedBuffers(context, fif);

		if (zest_vec_size(context->deferred_resource_freeing_list.transient_binding_indexes[fif])) {
			zest_vec_foreach(i, context->deferred_resource_freeing_list.transient_binding_indexes[fif]) {
				zest_binding_index_for_release_t index = context->deferred_resource_freeing_list.transient_binding_indexes[fif][i];
				zest__release_bindless_index(index.layout, index.binding_number, index.binding_index);
			}
			zest_vec_clear(context->deferred_resource_freeing_list.transient_binding_indexes[fif]);
		}

        if (zest_vec_size(context->deferred_resource_freeing_list.transient_images[fif])) {
            zest_vec_foreach(i, context->deferred_resource_freeing_list.transient_images[fif]) {
                zest_image image = &context->deferred_resource_freeing_list.transient_images[fif][i];
                context->device->platform->cleanup_image_backend(image);
                zest_FreeBufferNow((zest_buffer)image->buffer);
                if (image->default_view) {
                    context->device->platform->cleanup_image_view_backend(image->default_view);
                }
            }
            zest_vec_clear(context->deferred_resource_freeing_list.transient_images[fif]);
        }

		if (zest_vec_size(context->deferred_resource_freeing_list.transient_view_arrays[fif])) {
			zest_vec_foreach(i, context->deferred_resource_freeing_list.transient_view_arrays[fif]) {
				zest_image_view_array view_array = &context->deferred_resource_freeing_list.transient_view_arrays[fif][i];
				context->device->platform->cleanup_image_view_array_backend(view_array);
			}
			zest_vec_clear(context->deferred_resource_freeing_list.transient_view_arrays[fif]);
		}

        zest_vec_free(context->allocator, context->deferred_resource_freeing_list.resources[fif]);
        zest_vec_free(context->allocator, context->deferred_resource_freeing_list.transient_images[fif]);
        zest_vec_free(context->allocator, context->deferred_resource_freeing_list.transient_binding_indexes[fif]);
        zest_vec_free(context->allocator, context->deferred_resource_freeing_list.transient_view_arrays[fif]);

		context->device->platform->cleanup_execution_timeline_backend(&context->frame_timeline[fif]);
    }

    zest_map_foreach(i, context->cached_frame_graphs) {
        zest_cached_frame_graph_t *cached_graph = &context->cached_frame_graphs.data[i];
        ZEST__FREE(context->allocator, cached_graph->memory);
    }

    zest_ForEachFrameInFlight(fif) {
		zloc_linear_allocator_t *allocator = &context->frame_graph_allocator[fif];
		while (allocator) {
			ZEST__FREE(context->allocator, allocator->data);
			if (allocator != &context->frame_graph_allocator[fif]) {
				ZEST__FREE(context->allocator, allocator);
			}
			allocator = allocator->next;
		}
    }

	zest__free_context_buffer_allocators(context);

    zest_map_free(context->allocator, context->cached_pipelines);
    zest_map_free(context->allocator, context->cached_frame_graph_semaphores);
    zest_map_free(context->allocator, context->cached_frame_graphs);
    zest_map_free(context->allocator, context->buffer_allocators);

	context->device->platform->cleanup_context_backend(context);
	zloc_pool_stats_t stats = zloc_CreateMemorySnapshot(zloc__first_block_in_pool(zloc_GetPool(context->allocator)));
    if (stats.used_blocks > 0) {
        ZEST_ALERT("There are still used memory blocks in a zest context, this indicates a memory leak and a possible bug in the Zest Renderer. There should be no used blocks after a zest context has shutdown. Check the type of allocation in the list below and check to make sure you're freeing those objects.");
        zest_PrintMemoryBlocks(context->allocator, zloc__first_block_in_pool(zloc_GetPool(context->allocator)), 1, zest_platform_none, zest_command_none);
    }
	for (int i = 0; i != context->memory_pool_count; i++) {
		ZEST__FREE(context->device->allocator, context->memory_pools[i]);
	}
}

zest_bool zest__recreate_swapchain(zest_context context) {
	zest_swapchain swapchain = context->swapchain;
    int fb_width = 0, fb_height = 0;
    int window_width = 0, window_height = 0;
    while (fb_width == 0 || fb_height == 0) {
        context->window_data.window_sizes_callback(&context->window_data, &fb_width, &fb_height, &window_width, &window_height);
    }

    swapchain->size = ZEST_STRUCT_LITERAL(zest_extent2d_t, (zest_uint)fb_width, (zest_uint)fb_height );
    swapchain->resolution.x = 1.f / fb_width;
    swapchain->resolution.y = 1.f / fb_height;

    zest_WaitForIdleDevice(context->device);

	const char *name = swapchain->name;

	//clean up the swapchain except for the surface, that can be re-used
    context->device->platform->cleanup_swapchain_backend(swapchain);
    zest_vec_free(context->allocator, swapchain->images);
    zest_vec_free(context->allocator, swapchain->views);
	ZEST__FREE(context->allocator, swapchain);

    zest__cleanup_pipelines(context);
    zest_map_free(context->allocator, context->cached_pipelines);

	swapchain = zest__create_swapchain(context, name);
    ZEST__FLAG(swapchain->flags, zest_swapchain_flag_was_recreated);
    return swapchain != NULL;
}

zest_uniform_buffer_handle zest_CreateUniformBuffer(zest_context context, const char *name, zest_size uniform_struct_size) {
	zest_resource_store_t *store = &context->resource_stores[zest_handle_type_uniform_buffers];
    zest_uniform_buffer_handle handle = ZEST_STRUCT_LITERAL(zest_uniform_buffer_handle, zest__add_store_resource(store), store );
	handle.store = store;
    zest_uniform_buffer uniform_buffer = (zest_uniform_buffer)zest__get_store_resource_unsafe(store, handle.value);
    *uniform_buffer = ZEST__ZERO_INIT(zest_uniform_buffer_t);
    uniform_buffer->magic = zest_INIT_MAGIC(zest_struct_type_uniform_buffer);
    uniform_buffer->handle = handle;
	uniform_buffer->name = name;
    zest_buffer_info_t buffer_info = ZEST__ZERO_INIT(zest_buffer_info_t);
    buffer_info.buffer_usage_flags = zest_buffer_usage_uniform_buffer_bit;
    buffer_info.property_flags = zest_memory_property_host_visible_bit | zest_memory_property_host_coherent_bit;

    zest_ForEachFrameInFlight(fif) {
        uniform_buffer->buffer[fif] = zest_CreateBuffer(context->device, uniform_struct_size, &buffer_info);
		if (!uniform_buffer->buffer[fif]) {
			for (int i = 0; i != fif; i++) {
				zest_FreeBufferNow(uniform_buffer->buffer[i]);
			}
			zest__remove_store_resource(store, handle.value);
			return ZEST__ZERO_INIT(zest_uniform_buffer_handle);
		}
		uniform_buffer->descriptor_index[fif] = zest_AcquireUniformBufferIndex(context->device, uniform_buffer->buffer[fif]);
    }
	zest_device device = context->device;
	zest__activate_resource(handle.store, handle.value);
    return handle;
}

zest_uniform_buffer zest_GetUniformBuffer(zest_uniform_buffer_handle handle) {
	if (handle.value == 0) return NULL;
	zest_uniform_buffer uniform_buffer = (zest_uniform_buffer)zest__get_store_resource_checked(handle.store, handle.value);
	return uniform_buffer;
}

void zest_FreeUniformBuffer(zest_uniform_buffer_handle handle) {
	if (handle.value == 0) return;
    zest_uniform_buffer uniform_buffer = (zest_uniform_buffer)zest__get_store_resource_checked(handle.store, handle.value);
	zest_context context = (zest_context)handle.store->origin;
    zest_vec_push(context->allocator, context->deferred_resource_freeing_list.resources[context->current_fif], uniform_buffer);
}

zest_set_layout_builder_t zest_BeginSetLayoutBuilder(zloc_allocator *allocator) {
    zest_set_layout_builder_t builder = ZEST__ZERO_INIT(zest_set_layout_builder_t);
	builder.allocator = allocator;
    return builder;
}

zest_bool zest__binding_exists_in_layout_builder(zest_set_layout_builder_t *builder, zest_uint binding) {
    zest_vec_foreach(i, builder->bindings) {
        if (builder->bindings[i].binding == binding) {
            return ZEST_TRUE;
        }
    }
    return ZEST_FALSE;
}

void zest_AddLayoutBuilderBinding(zest_set_layout_builder_t *builder, zest_descriptor_binding_desc_t description) {
    zest_bool binding_exists = zest__binding_exists_in_layout_builder(builder, description.binding);
    ZEST_ASSERT(!binding_exists);       //That binding number already exists in the layout builder
    zest_vec_push(builder->allocator, builder->bindings, description);
}

zest_set_layout zest_FinishDescriptorSetLayout(zest_context context, zest_set_layout_builder_t *builder, const char *name, ...) {
	zest_device device = context->device;
    ZEST_ASSERT(name);  //Give the descriptor set a unique name
    zest_text_t layout_name = ZEST__ZERO_INIT(zest_text_t);
    va_list args;
    va_start(args, name);
    zest_SetTextfv(context->allocator, &layout_name, name, args);
    va_end(args);
    ZEST_ASSERT(builder->bindings);     //must have bindings to create the layout
    zest_uint binding_count = (zest_uint)zest_vec_size(builder->bindings);
    ZEST_ASSERT(binding_count > 0);     //Must add bindings before finishing the descriptor layout builder

    zest_set_layout set_layout = zest__new_descriptor_set_layout(context->device, context, layout_name.str);

    if (!device->platform->create_set_layout(device, context, builder, set_layout, ZEST_FALSE)) {
		zest_vec_free(context->allocator, builder->bindings);
        zest__cleanup_set_layout(set_layout);
		zest_FreeText(context->allocator, &layout_name);
        return NULL;
    }

    zest_vec_resize(context->allocator, set_layout->descriptor_indexes, zest_vec_size(builder->bindings));
    zest_vec_foreach(i, builder->bindings) {
        set_layout->descriptor_indexes[i] = ZEST__ZERO_INIT(zest_descriptor_indices_t);
        set_layout->descriptor_indexes[i].capacity = builder->bindings[i].count;
        set_layout->descriptor_indexes[i].descriptor_type = builder->bindings[i].type;
    }

    set_layout->bindings = builder->bindings;
    zest_FreeText(context->allocator, &layout_name);
    return set_layout;
}

zest_set_layout zest_FinishDescriptorSetLayoutForBindless(zest_device device, zest_set_layout_builder_t *builder, zest_uint num_global_sets_this_pool_should_support, const char *name, ...) {
    ZEST_ASSERT(name);  //Give the descriptor set a unique name
    zest_text_t layout_name = ZEST__ZERO_INIT(zest_text_t);
    va_list args;
    va_start(args, name);
    zest_SetTextfv(device->allocator, &layout_name, name, args);
    va_end(args);
    ZEST_ASSERT(builder->bindings);     //must have bindings to create the layout
    zest_uint binding_count = (zest_uint)zest_vec_size(builder->bindings);
	ZEST_ASSERT(binding_count > 0);     //Must add bindings before finishing the descriptor layout builder

    zest_set_layout set_layout = zest__new_descriptor_set_layout(device, NULL, layout_name.str);
    if (!device->platform->create_set_layout(device, NULL, builder, set_layout, ZEST_TRUE)) {
        zest__cleanup_set_layout(set_layout);
		zest_FreeText(device->allocator, &layout_name);
        return NULL;
    }

    ZEST__FLAG(set_layout->flags, zest_set_layout_flag_bindless);

    zest_CreateDescriptorPoolForLayout(set_layout, 1);

    zest_vec_resize(device->allocator, set_layout->descriptor_indexes, zest_vec_size(builder->bindings));
    zest_vec_foreach(i, builder->bindings) {
		zest_descriptor_indices_t *manager = &set_layout->descriptor_indexes[i];
		*manager = ZEST__ZERO_INIT(zest_descriptor_indices_t);
        manager->capacity = builder->bindings[i].count;
        manager->descriptor_type = builder->bindings[i].type;
		zest_vec_reserve(device->allocator, manager->free_indices, manager->capacity);
		memset(manager->free_indices, 0, zest_vec_size_in_bytes(manager->free_indices));
		zest_uint is_free_size = manager->capacity / (8 * sizeof(zest_size));
		zest_vec_resize(device->allocator, manager->is_free, is_free_size);
		memset(manager->is_free, 0, zest_vec_size_in_bytes(manager->is_free));
    }

    set_layout->bindings = builder->bindings;
    zest_FreeText(device->allocator, &layout_name);
    return set_layout;
}

ZEST_API zest_descriptor_set zest_CreateBindlessSet(zest_set_layout layout) {
    // max_sets_in_pool was set during pool creation, usually 1 for a global bindless set.
    ZEST_ASSERT(layout->pool->max_sets >= 1 && "Pool was not created to allow allocation of at least one set.");

	zest_device device = layout->device;
    return device->platform->create_bindless_set(layout);
}

zest_set_layout zest__new_descriptor_set_layout(zest_device device, zest_context context, const char *name) {
	zloc_allocator *allocator = context ? context->allocator : device->allocator;
    zest_set_layout descriptor_layout = (zest_set_layout)ZEST__NEW(allocator, zest_set_layout);
    *descriptor_layout = ZEST__ZERO_INIT(zest_set_layout_t);
    descriptor_layout->backend = (zest_set_layout_backend)device->platform->new_set_layout_backend(allocator);
    descriptor_layout->name.str = 0;
    descriptor_layout->magic = zest_INIT_MAGIC(zest_struct_type_set_layout);
	descriptor_layout->context = context;
	descriptor_layout->device = device;
	zest__sync_init(&descriptor_layout->sync);
    zest_SetText(allocator, &descriptor_layout->name, name);
    return descriptor_layout;
}

zest_uint zest__acquire_bindless_index(zest_set_layout layout, zest_uint binding_number) {
    if (binding_number >= zest_vec_size(layout->descriptor_indexes)) {
        ZEST_REPORT(layout->device, zest_report_bindless_indexes, "Attempted to acquire index for out-of-bounds binding_number %u for layout '%s'.", 
                   binding_number, layout->name.str);
        return ZEST_INVALID;
    }
    
    zest_descriptor_indices_t *manager = &layout->descriptor_indexes[binding_number];
    zest_uint current_size = zest_vec_size(manager->free_indices);
    
    // Try to pop from free list
    while (current_size > 0) {
        zest_uint new_size = current_size - 1;
        zest_uint index = manager->free_indices[new_size];  // Read BEFORE CAS
        
        // Atomically check and clear the bit
        zest_size word_idx = index / ZEST_BITS_PER_WORD;
        zest_size bit_idx = index % ZEST_BITS_PER_WORD;
        zest_size mask = (zest_size)1 << bit_idx;
        
        // Atomic test-and-clear
        zest_size old_word = zest__atomic_fetch_and((volatile zest_size*)&manager->is_free[word_idx], ~mask);
        
        if (!(old_word & mask)) {
            // Bit was already clear - someone else got it, retry
            current_size = zest_vec_size(manager->free_indices);
            continue;
        }
        
        // We successfully claimed it, now remove from list
        if (zest__atomic_compare_exchange((volatile int*)&zest__vec_header(manager->free_indices)->current_size, 
										  (int)new_size, (int)current_size)) {
            return index;
        }
        
        // CAS failed, put the bit back and retry
        zest__atomic_fetch_or((volatile zest_size*)&manager->is_free[word_idx], mask);
        current_size = zest_vec_size(manager->free_indices);
    }
    
    // Allocate new index
    zest_uint current = manager->next_new_index;
    while (1) {
        if (current >= manager->capacity) {
            ZEST_REPORT(layout->device, zest_report_bindless_indexes, 
						 "Ran out of bindless indices for binding %u in layout '%s'!",
						 binding_number, layout->name.str);
            return ZEST_INVALID;
        }
        
        zest_uint new_val = current + 1;
        if (zest__atomic_compare_exchange((volatile int*)&manager->next_new_index, 
										  (int)new_val, (int)current)) {
            // New indices start as allocated (bit clear), so nothing to update
            return current;
        }
        current = manager->next_new_index;
    }
}

void zest__release_bindless_index(zest_set_layout layout, zest_uint binding_number, zest_uint index_to_release) {
    ZEST_ASSERT_HANDLE(layout);
    if (index_to_release == ZEST_INVALID) return;
    
    if (binding_number >= zest_vec_size(layout->descriptor_indexes)) {
        ZEST_REPORT(layout->device, zest_report_bindless_indexes, "Attempted to release index for out-of-bounds binding_number %u for layout '%s'.", 
                   binding_number, layout->name.str);
        return;
    }
    
    zest_descriptor_indices_t *manager = &layout->descriptor_indexes[binding_number];
    ZEST_ASSERT(index_to_release < manager->capacity, 
                "Trying to release an index that's outside the bounds of the descriptor");
    
    zest_size word_idx = index_to_release / ZEST_BITS_PER_WORD;
    zest_size bit_idx = index_to_release % ZEST_BITS_PER_WORD;
    zest_size mask = (zest_size)1 << bit_idx;
    
    // Atomically test-and-set the bit
    zest_size old_word = zest__atomic_fetch_or((volatile zest_size*)&manager->is_free[word_idx], mask);
    
    if (old_word & mask) {
        // Bit was already set - double free!
        ZEST_REPORT(layout->device, zest_report_bindless_indexes, "Attempted to release index %u for binding_number %u for layout '%s' that is already free.", 
                   index_to_release, binding_number, layout->name.str);
        return;
    }
    
    // Bit successfully set (marked as free), now add to list
    zest_uint current_size = zest_vec_size(manager->free_indices);
    while (1) {
        zest_uint new_size = current_size + 1;
        if (zest__atomic_compare_exchange((volatile int*)&zest__vec_header(manager->free_indices)->current_size, 
										  (int)new_size, (int)current_size)) {
            manager->free_indices[current_size] = index_to_release;
			zest_device device = layout->device;
			if (!device->default_image_2d) break;
			switch (binding_number) {
				case zest_texture_2d_binding:
				case zest_texture_array_binding:
				case zest_texture_3d_binding: {
					device->platform->update_bindless_image_descriptor(
						device, binding_number, index_to_release, zest_descriptor_type_sampled_image, 
						device->default_image_2d, device->default_image_2d->default_view, NULL, device->bindless_set);
					break;
				}
				case zest_texture_cube_binding: {
					device->platform->update_bindless_image_descriptor(
						device, binding_number, index_to_release, zest_descriptor_type_sampled_image, 
						device->default_image_cube, device->default_image_cube->default_view, NULL, device->bindless_set);
					break;
				}
			}
            break;
        }
        current_size = zest_vec_size(manager->free_indices);
    }
}

void zest__cleanup_set_layout(zest_set_layout layout) {
	zest_context context = layout->context;
    zloc_allocator *allocator = context ? context->allocator : layout->device->allocator;
    zest_FreeText(allocator, &layout->name);
	zest_vec_foreach(i, layout->descriptor_indexes) {
		zest_vec_free(allocator, layout->descriptor_indexes[i].free_indices);
		zest_vec_free(allocator, layout->descriptor_indexes[i].is_free);
	}
	zest_vec_free(allocator, layout->descriptor_indexes);
    layout->device->platform->cleanup_set_layout_backend(layout);
	zest__sync_cleanup(&layout->sync);
    ZEST__FREE(allocator, layout->pool);
	ZEST__FREE(allocator, layout);
}

void zest__cleanup_image_view(zest_image_view view) {
	zest_device device = (zest_device)view->handle.store->origin;
    device->platform->cleanup_image_view_backend(view);
    if (view->handle.value) {
        //Default views in images are not in resource storage so only remove from store if the handle has
		//a value
        zest__remove_store_resource(view->handle.store, view->handle.value);
    }
	view->magic = 0;
    ZEST__FREE(device->allocator, view);
}

void zest__cleanup_image_view_array(zest_image_view_array view_array) {
	zest_device device = (zest_device)view_array->handle.store->origin;
    device->platform->cleanup_image_view_array_backend(view_array);
    ZEST__FREE(device->allocator, view_array->views);
    zest__remove_store_resource(view_array->handle.store, view_array->handle.value);
	view_array->magic = 0;
    ZEST__FREE(device->allocator, view_array);
}

zest_descriptor_pool zest__create_descriptor_pool(zest_device device, zloc_allocator *allocator, zest_uint max_sets) {
    zest_descriptor_pool_t blank = ZEST__ZERO_INIT(zest_descriptor_pool_t);
    zest_descriptor_pool pool = (zest_descriptor_pool)ZEST__NEW(allocator, zest_descriptor_pool);
    *pool = blank;
    pool->max_sets = max_sets;
    pool->magic = zest_INIT_MAGIC(zest_struct_type_descriptor_pool);
    pool->backend = (zest_descriptor_pool_backend)device->platform->new_descriptor_pool_backend(allocator);
    return pool;
}

zest_bool zest_CreateDescriptorPoolForLayout(zest_set_layout layout, zest_uint max_set_count) {
	zest_bool bindless = ZEST__FLAGGED(layout->flags, zest_set_layout_flag_bindless);
	max_set_count = bindless ? 1 : max_set_count;
	zloc_allocator *allocator = layout->context ? layout->context->allocator : layout->device->allocator;
	zest_descriptor_pool pool = zest__create_descriptor_pool(layout->device, allocator, max_set_count);
	if (!layout->device->platform->create_set_pool(layout->device, layout->context, pool, layout, max_set_count, bindless)) {
		ZEST__FREE(allocator, pool);
	}
	layout->pool = pool;
	return ZEST_TRUE;
}

zest_pipeline zest__create_pipeline(zest_context context) {
    zest_pipeline pipeline = (zest_pipeline)ZEST__NEW(context->allocator, zest_pipeline);
    *pipeline = ZEST__ZERO_INIT(zest_pipeline_t);
    pipeline->magic = zest_INIT_MAGIC(zest_struct_type_pipeline);
    pipeline->backend = (zest_pipeline_backend)context->device->platform->new_pipeline_backend(context);
	pipeline->context = context;
    return pipeline;
}

void zest_SetPipelineVertShader(zest_pipeline_template pipeline_template, zest_shader_handle shader_handle) {
    ZEST_ASSERT_HANDLE(pipeline_template);
    zest_shader vertex_shader = (zest_shader)zest__get_store_resource_checked(shader_handle.store, shader_handle.value);
    ZEST_ASSERT_HANDLE(vertex_shader);  //Not a valid vertex shader handle
    pipeline_template->vertex_shader = shader_handle;
}

void zest_SetPipelineFragShader(zest_pipeline_template pipeline_template, zest_shader_handle shader_handle) {
    ZEST_ASSERT_HANDLE(pipeline_template);
    zest_shader fragment_shader = (zest_shader)zest__get_store_resource_checked(shader_handle.store, shader_handle.value);
    ZEST_ASSERT_HANDLE(fragment_shader);	//Not a valid fragment shader handle
    pipeline_template->fragment_shader = shader_handle;
}

void zest_SetPipelineShaders(zest_pipeline_template pipeline_template, zest_shader_handle vertex_shader_handle, zest_shader_handle fragment_shader_handle) {
    ZEST_ASSERT_HANDLE(pipeline_template);
	ZEST_ASSERT(vertex_shader_handle.store == fragment_shader_handle.store);	//Vertex and fragment shader must be in the same context!
    zest_shader vertex_shader = (zest_shader)zest__get_store_resource_checked(vertex_shader_handle.store, vertex_shader_handle.value);
    zest_shader fragment_shader = (zest_shader)zest__get_store_resource_checked(fragment_shader_handle.store, fragment_shader_handle.value);
    ZEST_ASSERT_HANDLE(vertex_shader);       //Not a valid vertex shader handle
    ZEST_ASSERT_HANDLE(fragment_shader);     //Not a valid fragment shader handle
    pipeline_template->vertex_shader = vertex_shader_handle;
    pipeline_template->fragment_shader = fragment_shader_handle;
}

void zest_SetPipelineShader(zest_pipeline_template pipeline_template, zest_shader_handle combined_vertex_and_fragment_shader_handle) {
    ZEST_ASSERT_HANDLE(pipeline_template);
    zest_shader combined_vertex_and_fragment_shader = (zest_shader)zest__get_store_resource_checked(combined_vertex_and_fragment_shader_handle.store, combined_vertex_and_fragment_shader_handle.value);
    ZEST_ASSERT_HANDLE(combined_vertex_and_fragment_shader);       //Not a valid shader handle
    pipeline_template->vertex_shader = combined_vertex_and_fragment_shader_handle;
}

void zest_SetPipelineFrontFace(zest_pipeline_template pipeline_template, zest_front_face front_face) {
    ZEST_ASSERT_HANDLE(pipeline_template);  //invalid pipeline template handle
    pipeline_template->rasterization.front_face = front_face;
}

void zest_SetPipelineTopology(zest_pipeline_template pipeline_template, zest_topology topology) {
    ZEST_ASSERT_HANDLE(pipeline_template);  //invalid pipeline template handle
    pipeline_template->primitive_topology = topology;
}

void zest_SetPipelineCullMode(zest_pipeline_template pipeline_template, zest_cull_mode cull_mode) {
    ZEST_ASSERT_HANDLE(pipeline_template);  //invalid pipeline template handle
    pipeline_template->rasterization.cull_mode = cull_mode;
}

void zest_SetPipelinePolygonFillMode(zest_pipeline_template pipeline_template, zest_polygon_mode polygon_mode) {
    ZEST_ASSERT_HANDLE(pipeline_template);  //invalid pipeline template handle
    pipeline_template->rasterization.polygon_mode = polygon_mode;
}

void zest_SetPipelineBlend(zest_pipeline_template pipeline_template, zest_color_blend_attachment_t blend_attachment) {
    ZEST_ASSERT_HANDLE(pipeline_template);  //Not a valid pipeline template handle
    pipeline_template->color_blend_attachment = blend_attachment;
}

void zest_SetPipelineDepthTest(zest_pipeline_template pipeline_template, zest_bool enable_test, zest_bool write_enable) {
    ZEST_ASSERT_HANDLE(pipeline_template);  //Not a valid pipeline template handle
	pipeline_template->depth_stencil.depth_test_enable = enable_test;
	pipeline_template->depth_stencil.depth_write_enable = write_enable;
	pipeline_template->depth_stencil.depth_bounds_test_enable = ZEST_FALSE;
	pipeline_template->depth_stencil.stencil_test_enable = ZEST_FALSE;
}

void zest_SetPipelineDepthBias(zest_pipeline_template pipeline_template, zest_bool enabled) {
    ZEST_ASSERT_HANDLE(pipeline_template);  //Not a valid pipeline template handle
	pipeline_template->rasterization.depth_bias_enable = enabled;
}

void zest_SetPipelineEnableVertexInput(zest_pipeline_template pipeline_template) {
    ZEST_ASSERT_HANDLE(pipeline_template);  //Not a valid pipeline template handle
	pipeline_template->no_vertex_input = ZEST_FALSE;
}

void zest_SetPipelineDisableVertexInput(zest_pipeline_template pipeline_template) {
    ZEST_ASSERT_HANDLE(pipeline_template);  //Not a valid pipeline template handle
	pipeline_template->no_vertex_input = ZEST_TRUE;
}

void zest_SetPipelineViewCount(zest_pipeline_template pipeline_template, zest_uint view_count) {
    ZEST_ASSERT_HANDLE(pipeline_template);  //Not a valid pipeline template handle
	view_count = ZEST__MIN(view_count, pipeline_template->device->max_multiview_view_count);
	view_count = ZEST__MAX(1, view_count);
	pipeline_template->view_mask = (1u << view_count) - 1;
}

void zest_SetPipelineLayout(zest_pipeline_template pipeline_template, zest_pipeline_layout layout) {
    ZEST_ASSERT_HANDLE(pipeline_template);  //Not a valid pipeline template handle
	pipeline_template->layout = layout;
}

zest_pipeline_layout_info_t zest_NewPipelineLayoutInfo(zest_device device) {
	zest_pipeline_layout_info_t info = ZEST__ZERO_INIT(zest_pipeline_layout_info_t);
	zest_SetPipelineLayoutPushConstantRange(&info, 128, zest_shader_all_stages);
	info.device = device;
	return info;
}

zest_pipeline_layout_info_t zest_NewPipelineLayoutInfoWithGlobalBindless(zest_device device) {
	zest_pipeline_layout_info_t info = ZEST__ZERO_INIT(zest_pipeline_layout_info_t);
	info.device = device;
	zest_AddPipelineLayoutDescriptorLayout(&info, zest_GetBindlessLayout(device));
	zest_SetPipelineLayoutPushConstantRange(&info, 128, zest_shader_all_stages);
	return info;
}

void zest_AddPipelineLayoutDescriptorLayout(zest_pipeline_layout_info_t *info, zest_set_layout layout) {
	ZEST_ASSERT_HANDLE(info->device);	//Not a valid device set in the layout info.
	zest_vec_push(info->device->allocator, info->set_layouts, layout);
}

void zest_SetPipelineLayoutPushConstantRange(zest_pipeline_layout_info_t *info, zest_uint size, zest_supported_shader_stages stage_flags) {
    zest_push_constant_range_t range;
    range.size = size;
    range.offset = 0;
    range.stage_flags = stage_flags;
    info->push_constant_range = range;
}

zest_pipeline_layout zest_CreatePipelineLayout(zest_pipeline_layout_info_t *info) {
	zest_uint set_count = zest_vec_size(info->set_layouts);
	zest_device device = info->device;
	ZEST_ASSERT_HANDLE(device);		//Not a valid device handle in the info.
	ZEST_ASSERT(set_count > 0 || info->push_constant_range.size > 0, "Error: Either the descriptor set count or the push constant range must be defined when creating a pipeline layout.");
	zest_pipeline_layout layout = (zest_pipeline_layout)ZEST__NEW(device->allocator, zest_pipeline_layout);
	*layout = ZEST__ZERO_INIT(zest_pipeline_layout_t);
	layout->magic = zest_INIT_MAGIC(zest_struct_type_pipeline_layout);
	layout->device = device;
	layout->push_constant_range = info->push_constant_range;
	layout->backend = (zest_pipeline_layout_backend)device->platform->new_pipeline_layout_backend(device);
	if (!device->platform->build_pipeline_layout(device, layout, info)) {
		ZEST_ALERT("Error: Could not create a pipeline layout. Check for validation errors.");
		device->platform->cleanup_pipeline_layout_backend(layout);
		ZEST__FREE(device->allocator, layout);
		layout = NULL;
	}
	zest_vec_free(device->allocator, info->set_layouts);
	return layout;
}

zest_pipeline_layout zest_GetPipelineLayout(zest_pipeline pipeline) {
	ZEST_ASSERT_HANDLE(pipeline);		//Not a valid pipeline!
	return pipeline->layout;
}

zest_vertex_binding_desc_t zest_AddVertexInputBindingDescription(zest_pipeline_template pipeline_template, zest_uint binding, zest_uint stride, zest_input_rate input_rate) {
    zest_vec_foreach(i, pipeline_template->binding_descriptions) {
        //You already have a binding with that index in the bindindDescriptions array
        //Maybe you copied a template with zest_CopyPipelineTemplate but didn't call zest_ClearVertexInputBindingDescriptions on the copy before
        //adding your own
        ZEST_ASSERT(binding != pipeline_template->binding_descriptions[i].binding);
    }
    zest_vertex_binding_desc_t input_binding_description = ZEST__ZERO_INIT(zest_vertex_binding_desc_t);
    input_binding_description.binding = binding;
    input_binding_description.stride = stride;
    input_binding_description.input_rate = input_rate;
    zest_vec_push(pipeline_template->device->allocator, pipeline_template->binding_descriptions, input_binding_description);
    zest_size size = zest_vec_size(pipeline_template->binding_descriptions);
    return input_binding_description;
}

void zest_ClearVertexInputBindingDescriptions(zest_pipeline_template pipeline_template) {
    zest_vec_clear(pipeline_template->binding_descriptions);
}

void zest_ClearVertexAttributeDescriptions(zest_pipeline_template pipeline_template) {
    zest_vec_clear(pipeline_template->attribute_descriptions);
}

zest_color_blend_attachment_t zest_AdditiveBlendState() {
    zest_color_blend_attachment_t color_blend_attachment;
    color_blend_attachment.blend_enable = ZEST_TRUE;
    color_blend_attachment.color_write_mask = zest_color_component_r_bit | zest_color_component_g_bit | zest_color_component_b_bit | zest_color_component_a_bit;
    color_blend_attachment.src_color_blend_factor = zest_blend_factor_one;
    color_blend_attachment.dst_color_blend_factor = zest_blend_factor_one;
    color_blend_attachment.color_blend_op = zest_blend_op_add;
    color_blend_attachment.src_alpha_blend_factor = zest_blend_factor_one;
    color_blend_attachment.dst_alpha_blend_factor = zest_blend_factor_one;
    color_blend_attachment.alpha_blend_op = zest_blend_op_add;
    return color_blend_attachment;
}

zest_color_blend_attachment_t zest_AdditiveBlendState2() {
    zest_color_blend_attachment_t color_blend_attachment;
    color_blend_attachment.blend_enable = ZEST_TRUE;
    color_blend_attachment.color_write_mask = zest_color_component_r_bit | zest_color_component_g_bit | zest_color_component_b_bit | zest_color_component_a_bit;
    color_blend_attachment.src_color_blend_factor = zest_blend_factor_one;
    color_blend_attachment.dst_color_blend_factor = zest_blend_factor_one;
    color_blend_attachment.color_blend_op = zest_blend_op_add;
    color_blend_attachment.src_alpha_blend_factor = zest_blend_factor_src_alpha;
    color_blend_attachment.dst_alpha_blend_factor = zest_blend_factor_dst_alpha;
    color_blend_attachment.alpha_blend_op = zest_blend_op_add;
    return color_blend_attachment;
}

zest_color_blend_attachment_t zest_AlphaOnlyBlendState() {
    zest_color_blend_attachment_t color_blend_attachment;
    color_blend_attachment.blend_enable = ZEST_TRUE;
    color_blend_attachment.color_write_mask = zest_color_component_a_bit;
    color_blend_attachment.src_color_blend_factor = zest_blend_factor_one;
    color_blend_attachment.dst_color_blend_factor = zest_blend_factor_one;
    color_blend_attachment.color_blend_op = zest_blend_op_add;
    color_blend_attachment.src_alpha_blend_factor = zest_blend_factor_one;
    color_blend_attachment.dst_alpha_blend_factor = zest_blend_factor_one_minus_src_alpha;
    color_blend_attachment.alpha_blend_op = zest_blend_op_add;
    return color_blend_attachment;
}

zest_color_blend_attachment_t zest_BlendStateNone() {
    zest_color_blend_attachment_t color_blend_attachment = ZEST__ZERO_INIT(zest_color_blend_attachment_t);
    color_blend_attachment.color_write_mask = 0;
    color_blend_attachment.blend_enable = ZEST_FALSE;
    return color_blend_attachment;
}

zest_color_blend_attachment_t zest_AlphaBlendState() {
    zest_color_blend_attachment_t color_blend_attachment;
    color_blend_attachment.color_write_mask = zest_color_component_r_bit | zest_color_component_g_bit | zest_color_component_b_bit | zest_color_component_a_bit;
    color_blend_attachment.blend_enable = ZEST_TRUE;
    color_blend_attachment.src_color_blend_factor = zest_blend_factor_one;
    color_blend_attachment.dst_color_blend_factor = zest_blend_factor_one_minus_src_alpha;
    color_blend_attachment.color_blend_op = zest_blend_op_add;
    color_blend_attachment.src_alpha_blend_factor = zest_blend_factor_one;
    color_blend_attachment.dst_alpha_blend_factor = zest_blend_factor_one_minus_src_alpha;
    color_blend_attachment.alpha_blend_op = zest_blend_op_add;
    return color_blend_attachment;
}

zest_color_blend_attachment_t zest_PreMultiplyBlendState() {
    zest_color_blend_attachment_t color_blend_attachment;
    color_blend_attachment.color_write_mask = zest_color_component_r_bit | zest_color_component_g_bit | zest_color_component_b_bit | zest_color_component_a_bit;
    color_blend_attachment.blend_enable = ZEST_TRUE;
    color_blend_attachment.src_color_blend_factor = zest_blend_factor_one;
    color_blend_attachment.dst_color_blend_factor = zest_blend_factor_one_minus_src_alpha;
    color_blend_attachment.color_blend_op = zest_blend_op_add;
    color_blend_attachment.src_alpha_blend_factor = zest_blend_factor_one;
    color_blend_attachment.dst_alpha_blend_factor = zest_blend_factor_one_minus_src_alpha;
    color_blend_attachment.alpha_blend_op = zest_blend_op_add;
    return color_blend_attachment;
}

zest_color_blend_attachment_t zest_PreMultiplyBlendStateForSwap() {
    zest_color_blend_attachment_t color_blend_attachment;
    color_blend_attachment.color_write_mask = zest_color_component_r_bit | zest_color_component_g_bit | zest_color_component_b_bit | zest_color_component_a_bit;
    color_blend_attachment.blend_enable = ZEST_TRUE;
    color_blend_attachment.src_color_blend_factor = zest_blend_factor_one;
    color_blend_attachment.dst_color_blend_factor = zest_blend_factor_one_minus_src_alpha;
    color_blend_attachment.color_blend_op = zest_blend_op_add;
    color_blend_attachment.src_alpha_blend_factor = zest_blend_factor_one;
    color_blend_attachment.dst_alpha_blend_factor = zest_blend_factor_zero;
    color_blend_attachment.alpha_blend_op = zest_blend_op_add;
    return color_blend_attachment;
}

zest_color_blend_attachment_t zest_MaxAlphaBlendState() {
    zest_color_blend_attachment_t color_blend_attachment;
    color_blend_attachment.color_write_mask = zest_color_component_r_bit | zest_color_component_g_bit | zest_color_component_b_bit | zest_color_component_a_bit;
    color_blend_attachment.blend_enable = ZEST_TRUE;
    color_blend_attachment.src_color_blend_factor = zest_blend_factor_src_alpha;
    color_blend_attachment.dst_color_blend_factor = zest_blend_factor_one_minus_dst_alpha;
    color_blend_attachment.color_blend_op = zest_blend_op_add;
    color_blend_attachment.src_alpha_blend_factor = zest_blend_factor_one;
    color_blend_attachment.dst_alpha_blend_factor = zest_blend_factor_one_minus_src_alpha;
    color_blend_attachment.alpha_blend_op = zest_blend_op_add;
    return color_blend_attachment;
}

zest_color_blend_attachment_t zest_ImGuiBlendState() {
    zest_color_blend_attachment_t color_blend_attachment;
    color_blend_attachment.color_write_mask = zest_color_component_r_bit | zest_color_component_g_bit | zest_color_component_b_bit | zest_color_component_a_bit;
    color_blend_attachment.blend_enable = ZEST_TRUE;
    color_blend_attachment.src_color_blend_factor = zest_blend_factor_src_alpha;
    color_blend_attachment.dst_color_blend_factor = zest_blend_factor_one_minus_src_alpha;
    color_blend_attachment.color_blend_op = zest_blend_op_add;
    color_blend_attachment.src_alpha_blend_factor = zest_blend_factor_one_minus_src_alpha;
    color_blend_attachment.dst_alpha_blend_factor = zest_blend_factor_zero;
    color_blend_attachment.alpha_blend_op = zest_blend_op_add;
    return color_blend_attachment;
}

zest_bool zest__cache_pipeline(zest_pipeline_template pipeline_template, zest_command_list command_list, zest_key pipeline_key, zest_pipeline *out_pipeline) {
	zest_context context = command_list->context;
	zest_pipeline pipeline = zest__create_pipeline(context);
    pipeline->pipeline_template = pipeline_template;
	pipeline->layout = pipeline_template->layout;
    zest_bool result = context->device->platform->build_pipeline(pipeline, command_list);
	if (result == ZEST_TRUE) {
		zest_map_insert_key(context->allocator, context->cached_pipelines, pipeline_key, pipeline);
		*out_pipeline = pipeline;
	} else {
		ZEST__FLAG(pipeline_template->flags, zest_pipeline_invalid);
		zest__cleanup_pipeline(pipeline);
	}
    return result;
}

zest_pipeline_template zest_CopyPipelineTemplate(const char *name, zest_pipeline_template pipeline_to_copy) {
	zest_device device = pipeline_to_copy->device;
    zest_pipeline_template copy = zest_CreatePipelineTemplate(device, name);
    copy->no_vertex_input = pipeline_to_copy->no_vertex_input;
    copy->primitive_topology = pipeline_to_copy->primitive_topology;
    copy->rasterization = pipeline_to_copy->rasterization;
	copy->color_blend_attachment = pipeline_to_copy->color_blend_attachment;
	copy->depth_stencil = pipeline_to_copy->depth_stencil;
	copy->layout = pipeline_to_copy->layout;
    if (pipeline_to_copy->binding_descriptions) {
        zest_vec_resize(device->allocator, copy->binding_descriptions, zest_vec_size(pipeline_to_copy->binding_descriptions));
        memcpy(copy->binding_descriptions, pipeline_to_copy->binding_descriptions, zest_vec_size_in_bytes(pipeline_to_copy->binding_descriptions));
    }
    if (pipeline_to_copy->attribute_descriptions) {
        zest_vec_resize(device->allocator, copy->attribute_descriptions, zest_vec_size(pipeline_to_copy->attribute_descriptions));
        memcpy(copy->attribute_descriptions, pipeline_to_copy->attribute_descriptions, zest_vec_size_in_bytes(pipeline_to_copy->attribute_descriptions));
    }
    copy->vertex_shader = pipeline_to_copy->vertex_shader;
    copy->fragment_shader = pipeline_to_copy->fragment_shader;
    return copy;
}

void zest_FreePipelineTemplate(zest_pipeline_template pipeline_template) {
    ZEST_ASSERT_HANDLE(pipeline_template);   //Not a valid pipeline template handle
    zest__cleanup_pipeline_template(pipeline_template);
}

zest_bool zest_PipelineIsValid(zest_pipeline_template pipeline_template) {
	return ZEST__NOT_FLAGGED(pipeline_template->flags, zest_pipeline_invalid);
}

void zest__cleanup_pipeline_template(zest_pipeline_template pipeline_template) {
    zest_vec_free(pipeline_template->device->allocator, pipeline_template->attribute_descriptions);
    zest_vec_free(pipeline_template->device->allocator, pipeline_template->binding_descriptions);
    ZEST__FREE(pipeline_template->device->allocator, pipeline_template);
}

zest_bool zest_ValidateShader(zest_device device, const char *shader_code, zest_shader_type type, const char *name) {
	if (!device->platform->validate_shader(device, shader_code, type, name)) {
		return ZEST_FALSE;
	}
    return ZEST_TRUE;
}

zest_bool zest_CompileShader(zest_shader_handle shader_handle) {
	zest_device device = (zest_device)shader_handle.store->origin;
    zest_shader shader = (zest_shader)zest__get_store_resource_checked(shader_handle.store, shader_handle.value);
	
    if (device->platform->compile_shader(shader, shader->shader_code.str, zest_TextLength(&shader->shader_code), shader->type, shader->name.str, "main", NULL)) {
		ZEST_APPEND_LOG(device->log_path.str, "Successfully compiled shader: %s.", shader->name.str);
        return ZEST_TRUE;
    }
    return ZEST_FALSE;
}

zest_shader_handle zest_CreateShaderFromFile(zest_device device, const char *file, const char *name, zest_shader_type type, zest_bool disable_caching) {
    char *shader_code = zest_ReadEntireFile(device, file, ZEST_TRUE);
    ZEST_ASSERT(shader_code, "Unable to load the shader code, check the path is valid."); 
    zest_shader_handle shader_handle = zest_CreateShader(device, shader_code, type, name, disable_caching);
	zest_shader shader = (zest_shader)zest__get_store_resource_checked(shader_handle.store, shader_handle.value);
    zest_SetText(device->allocator, &shader->file_path, file);
    zest_vec_free(device->allocator, shader_code);
    return shader_handle;
}

zest_bool zest_ReloadShader(zest_shader_handle shader_handle) {
	zest_shader shader = (zest_shader)zest__get_store_resource_checked(shader_handle.store, shader_handle.value);
    ZEST_ASSERT(zest_TextLength(&shader->file_path));    //The shader must have a file path set.
	zest_device device = (zest_device)shader_handle.store->origin;
    char *shader_code = zest_ReadEntireFile(device, shader->file_path.str, ZEST_TRUE);
    if (!shader_code) {
        return 0;
    }
    zest_SetText(device->allocator, &shader->shader_code, shader_code);
    return 1;
}

zest_shader_handle zest_CreateShader(zest_device device, const char *shader_code, zest_shader_type type, const char *name, zest_bool disable_caching) {
	ZEST_ASSERT_HANDLE(device);		//Not a valid device handle
    ZEST_ASSERT(name);     //You must give the shader a name
    zest_text_t shader_name = ZEST__ZERO_INIT(zest_text_t);
    if (zest_TextSize(&device->cached_shaders_path)) {
        zest_SetTextf(device->allocator, &shader_name, "%s%s", device->cached_shaders_path, name);
    }
    else {
        zest_SetTextf(device->allocator, &shader_name, "%s", name);
    }
    zest_shader_handle shader_handle = zest__new_shader(device, type);
    zest_shader shader = (zest_shader)zest__get_store_resource_unsafe(shader_handle.store, shader_handle.value);
    shader->name = shader_name;
    if (!disable_caching && device->init_flags & zest_device_init_flag_cache_shaders) {
        shader->spv = zest_ReadEntireFile(device, shader->name.str, ZEST_FALSE);
        if (shader->spv) {
            shader->spv_size = zest_vec_size(shader->spv);
			zest_SetText(device->allocator, &shader->shader_code, shader_code);
			ZEST_APPEND_LOG(device->log_path.str, "Loaded shader %s from cache.", name);
			zest__activate_resource(shader_handle.store, shader_handle.value);
            return shader_handle;
        }
    }

	zest_SetText(device->allocator, &shader->shader_code, shader_code);
	if (!device->platform->compile_shader(shader, shader->shader_code.str, zest_TextLength(&shader->shader_code), type, name, "main", NULL)) {
		zest__activate_resource(shader_handle.store, shader_handle.value);
        zest_FreeShader(shader_handle);
        ZEST_ASSERT(0, "There's a bug in this shader that needs fixing. You can check the log file for the error message");
	}

    if (!disable_caching && device->init_flags & zest_device_init_flag_cache_shaders) {
        zest__cache_shader(device, shader);
    }
	zest__activate_resource(shader_handle.store, shader_handle.value);
    return shader_handle;
}

void zest__cache_shader(zest_device device, zest_shader shader) {
    zest__create_folder(device, device->cached_shaders_path.str);
    FILE *shader_file = zest__open_file(shader->name.str, "wb");
    if (shader_file == NULL) {
        ZEST_APPEND_LOG(device->log_path.str, "Failed to open file for writing: %s", shader->name.str);
        return;
    }
    size_t written = fwrite(shader->spv, 1, shader->spv_size, shader_file);
    if (written != shader->spv_size) {
        ZEST_APPEND_LOG(device->log_path.str, "Failed to write entire shader to file: %s", shader->name.str);
        fclose(shader_file);
    }
    fclose(shader_file);
}

zest_shader_handle zest_CreateShaderSPVMemory(zest_device device, const unsigned char *shader_code, zest_uint spv_length, const char *name, zest_shader_type type) {
	ZEST_ASSERT_HANDLE(device);		//Not a valid device handle
    ZEST_ASSERT(shader_code);   //No shader code!
    ZEST_ASSERT(name);     //You must give the shader a name
    zest_text_t shader_name = ZEST__ZERO_INIT(zest_text_t);
    if (zest_TextSize(&device->cached_shaders_path)) {
        zest_SetTextf(device->allocator, &shader_name, "%s%s", device->cached_shaders_path, name);
    } else {
        zest_SetTextf(device->allocator, &shader_name, "%s", name);
    }
    zest_shader_handle shader_handle = zest__new_shader(device, type);
    zest_shader shader = (zest_shader)zest__get_store_resource_unsafe(shader_handle.store, shader_handle.value);
    zest_vec_resize(device->allocator, shader->spv, spv_length);
    memcpy(shader->spv, shader_code, spv_length);
    shader->spv_size = spv_length;
    zest_FreeText(device->allocator, &shader_name);
	zest__activate_resource(shader_handle.store, shader_handle.value);
    return shader_handle;
}

zest_shader zest_GetShader(zest_shader_handle shader_handle) {
	zest_shader shader = (zest_shader)zest__get_store_resource_checked(shader_handle.store, shader_handle.value);
	return shader;
}

void zest__compile_builtin_shaders(zest_device device) {
    device->builtin_shaders.swap_vert          = zest_CreateShader(device, zest_shader_swap_vert, zest_vertex_shader, "swap_vert.spv", 1);
    device->builtin_shaders.swap_frag          = zest_CreateShader(device, zest_shader_swap_frag, zest_fragment_shader, "swap_frag.spv", 1);
}

zest_shader_handle zest__new_shader(zest_device device, zest_shader_type type) {
	zest_resource_store_t *store = &device->resource_stores[zest_handle_type_shaders];
    zest_shader_handle handle = ZEST_STRUCT_LITERAL(zest_shader_handle, zest__add_store_resource(store), store );
	handle.store = store;
    zest_shader shader = (zest_shader)zest__get_store_resource_unsafe(store, handle.value);
    *shader = ZEST__ZERO_INIT(zest_shader_t);
    shader->magic = zest_INIT_MAGIC(zest_struct_type_shader);
    shader->type = type;
    shader->handle = handle;
    return handle;
}

zest_shader_handle zest_CreateShaderFromSPVFile(zest_device device, const char *filename, zest_shader_type type) {
	ZEST_ASSERT_HANDLE(device);		//Not a valid device handle
    ZEST_ASSERT(filename);     //You must give the shader a name
    zest_shader_handle shader_handle = zest__new_shader(device, type);
    zest_shader shader = (zest_shader)zest__get_store_resource_unsafe(shader_handle.store, shader_handle.value);
    shader->spv = zest_ReadEntireFile(device, filename, ZEST_FALSE);
    ZEST_ASSERT(shader->spv);   //File not found, could not load this shader!
    shader->spv_size = zest_vec_size(shader->spv);
	zest_SetText(device->allocator, &shader->name, filename);
	ZEST_APPEND_LOG(device->log_path.str, "Loaded shader %s and added to renderer shaders.", filename);
	zest__activate_resource(shader_handle.store, shader_handle.value);
	return shader_handle;
}

zest_shader_handle zest_AddShaderFromSPVMemory(zest_device device, const char *name, const void *buffer, zest_uint size, zest_shader_type type) {
	ZEST_ASSERT_HANDLE(device);		//Not a valid device handle
    ZEST_ASSERT(name);     //You must give the shader a name
    ZEST_ASSERT(!strstr(name, "/"));    //name must not contain /, the shader will be prefixed with the cache folder automatically
    if (buffer && size) {
		zest_shader_handle shader_handle = zest__new_shader(device, type);
		zest_shader shader = (zest_shader)zest__get_store_resource_unsafe(shader_handle.store, shader_handle.value);
		if (zest_TextSize(&device->cached_shaders_path)) {
			zest_SetTextf(device->allocator, &shader->name, "%s%s", device->cached_shaders_path, name);
		}
		else {
			zest_SetTextf(device->allocator, &shader->name, "%s", name);
		}
		zest_vec_resize(device->allocator, shader->spv, size);
		memcpy(shader->spv, buffer, size);
        ZEST_APPEND_LOG(device->log_path.str, "Read shader %s from memory and added to renderer shaders.", name);
        shader->spv_size = size;
		zest__activate_resource(shader_handle.store, shader_handle.value);
        return shader_handle;
    }
    return ZEST__ZERO_INIT(zest_shader_handle);
}

void zest_FreeShader(zest_shader_handle shader_handle) {
	zest_shader shader = (zest_shader)zest__get_store_resource_checked(shader_handle.store, shader_handle.value);
	zest_device device = (zest_device)shader_handle.store->origin;
    zest_FreeText(device->allocator, &shader->name);
    zest_FreeText(device->allocator, &shader->shader_code);
    zest_FreeText(device->allocator, &shader->file_path);
    if (shader->spv) {
        zest_vec_free(device->allocator, shader->spv);
    }
    zest__remove_store_resource(shader_handle.store, shader_handle.value);
}

zest_uint zest_GetMaxImageSize(zest_context context) {
	ZEST_ASSERT_HANDLE(context);
	return context->device->max_image_size;
}

zest_device zest_GetContextDevice(zest_context context) {
	ZEST_ASSERT_HANDLE(context);
	return context->device;
}

zest_uint zest_GetLayerVertexDescriptorIndex(zest_layer layer, zest_bool last_frame) {
	ZEST_ASSERT_HANDLE(layer); //ERROR: Not a valid layer pointer
    return layer->memory_refs[last_frame ? layer->prev_fif : layer->fif].descriptor_array_index;
}

zest_pipeline_template zest_CreatePipelineTemplate(zest_device device, const char* name) {
    zest_pipeline_template pipeline_template = (zest_pipeline_template)ZEST__NEW(device->allocator, zest_pipeline_template);
    *pipeline_template = ZEST__ZERO_INIT(zest_pipeline_template_t);
    pipeline_template->magic = zest_INIT_MAGIC(zest_struct_type_pipeline_template);
    pipeline_template->name = name;
    pipeline_template->no_vertex_input = ZEST_FALSE;
    pipeline_template->fragShaderFunctionName = "main";
    pipeline_template->vertShaderFunctionName = "main";
    pipeline_template->color_blend_attachment = zest_AlphaBlendState();

    pipeline_template->rasterization.depth_clamp_enable = ZEST_FALSE;
    pipeline_template->rasterization.rasterizer_discard_enable = ZEST_FALSE;
    pipeline_template->rasterization.polygon_mode = zest_polygon_mode_fill;
	pipeline_template->primitive_topology = zest_topology_triangle_list;
    pipeline_template->rasterization.line_width = 1.0f;
    pipeline_template->rasterization.cull_mode = zest_cull_mode_none;
    pipeline_template->rasterization.front_face = zest_front_face_clockwise;
    pipeline_template->rasterization.depth_bias_enable = ZEST_FALSE;
	pipeline_template->depth_stencil.depth_compare_op = zest_compare_op_less_or_equal;

	pipeline_template->device = device;
	pipeline_template->layout = device->pipeline_layout;

    return pipeline_template;
}

void zest_AddVertexAttribute(zest_pipeline_template pipeline_template, zest_uint binding, zest_uint location, zest_format format, zest_uint offset) {
    zest_vertex_attribute_desc_t input_attribute_description = ZEST__ZERO_INIT(zest_vertex_attribute_desc_t);
    input_attribute_description.location = location;
    input_attribute_description.binding = binding;
    input_attribute_description.format = format;
    input_attribute_description.offset = offset;
    zest_vec_push(pipeline_template->device->allocator, pipeline_template->attribute_descriptions, input_attribute_description);
}

zest_key zest_Hash(const void* input, zest_ull length, zest_ull seed) { 
    zest_hasher_t hasher;
    zest__hash_initialise(&hasher, seed); 
    zest__hasher_add(&hasher, input, length); 
    return (zest_key)zest__get_hash(&hasher); 
}

zest_pipeline zest_GetPipeline(zest_pipeline_template pipeline_template, const zest_command_list command_list) {
	zest_context context = command_list->context;
    if (!ZEST_VALID_HANDLE(pipeline_template->layout, zest_struct_type_pipeline_layout)) {
        ZEST_ALERT("ERROR: You're trying to build a pipeline (%s) that has no pipeline layout configured. You can add descriptor layouts when building the pipeline with zest_SetPipelineLayout.", pipeline_template->name);
        return NULL;
    }
	if (!zest_PipelineIsValid(pipeline_template)) {
		ZEST_REPORT(context->device, zest_report_unused_pass, "You're trying to build a pipeline (%s) that has been marked as invalid. This means that the last time this pipeline was created it failed with errors. You can check for validation errors to see what they were.", pipeline_template->name);
		return NULL;
	}
    zest_key pipeline_key = (zest_key)pipeline_template;
	zest_key cached_pipeline_key = zest_Hash(&command_list->rendering_info, sizeof(zest_rendering_info_t), pipeline_key);
    if (zest_map_valid_key(context->cached_pipelines, pipeline_key)) {
		return *zest_map_at_key(context->cached_pipelines, pipeline_key); 
    }
    zest_pipeline pipeline = 0;
	if (!zest__cache_pipeline(pipeline_template, command_list, pipeline_key, &pipeline)) {
		ZEST_ALERT("ERROR: Unable to build and cache pipeline [%s]. Check the log and validation errors for the most recent errors.", pipeline_template->name);
	}
	return pipeline;
}

zest_extent2d_t zest_GetSwapChainExtent(zest_context context) { return context->swapchain->size; }
zest_uint zest_ScreenWidth(zest_context context) { return (zest_uint)context->swapchain->size.width; }
zest_uint zest_ScreenHeight(zest_context context) { return (zest_uint)context->swapchain->size.height; }
float zest_ScreenWidthf(zest_context context) { return (float)context->swapchain->size.width; }
float zest_ScreenHeightf(zest_context context) { return (float)context->swapchain->size.height; }
void *zest_NativeWindow(zest_context context) { return context->window_data.native_handle; }
void *zest_Window(zest_context context) { return context->window_data.window_handle; }
float zest_DPIScale(zest_context context) { return context->dpi_scale; }
zest_uint zest_CurrentFIF(zest_context context) { return context->current_fif; }
void zest_SetDPIScale(zest_context context, float scale) { context->dpi_scale = scale; }
void zest__hash_initialise(zest_hasher_t* hasher, zest_ull seed) { hasher->state[0] = seed + zest__PRIME1 + zest__PRIME2; hasher->state[1] = seed + zest__PRIME2; hasher->state[2] = seed; hasher->state[3] = seed - zest__PRIME1; hasher->buffer_size = 0; hasher->total_length = 0; }

void zest_WaitForIdleDevice(zest_device device) { 
	device->platform->wait_for_idle_device(device); 
	zest_vec_foreach(i, device->queues) {
		zest_queue_manager queue_manager = device->queues[i];
		zest_vec_foreach(j, queue_manager->queues) {
			zest_queue queue = &queue_manager->queues[j];
			zest__release_queue(queue);
		}
	}
}

zest_uint zest_GetUniformBufferDescriptorIndex(zest_uniform_buffer uniform_buffer) {
    ZEST_ASSERT_HANDLE(uniform_buffer);  //Not a valid buffer handle
	zest_context context = (zest_context)uniform_buffer->handle.store->origin;
	return uniform_buffer->descriptor_index[context->current_fif];
}

void* zest_GetUniformBufferData(zest_uniform_buffer uniform_buffer) {
    ZEST_ASSERT_HANDLE(uniform_buffer);  //Not a valid buffer handle
	zest_context context = (zest_context)uniform_buffer->handle.store->origin;
    return zest_BufferData(uniform_buffer->buffer[context->current_fif]);
}

zest_size zest_BufferSize(zest_buffer buffer) {
    ZEST_ASSERT(buffer);  //Not a valid buffer handle
    return buffer->size;
}

void zest_SetGPUBufferPoolSize(zest_device device, zest_size minimum_size, zest_size pool_size) {
    zest_buffer_usage_t usage = ZEST__ZERO_INIT(zest_buffer_usage_t);
    usage.property_flags = zest_memory_property_device_local_bit;
	pool_size = zest_RoundUpToNearestPower(pool_size);
	minimum_size = zest_RoundUpToNearestPower(minimum_size);
	usage.memory_pool_type = zest_memory_pool_type_buffers;
	zest_SetDevicePoolSize(device, "Device Buffers", usage, minimum_size, pool_size);
}

void zest_SetGPUSmallBufferPoolSize(zest_device device, zest_size minimum_size, zest_size pool_size) {
    zest_buffer_usage_t usage = ZEST__ZERO_INIT(zest_buffer_usage_t);
    usage.property_flags = zest_memory_property_device_local_bit;
	pool_size = zest_RoundUpToNearestPower(pool_size);
	minimum_size = zest_RoundUpToNearestPower(minimum_size);
	usage.memory_pool_type = zest_memory_pool_type_small_buffers;
	zest_SetDevicePoolSize(device, "Small Device Buffers", usage, minimum_size, pool_size);
}

void zest_SetGPUTransientBufferPoolSize(zest_device device, zest_size minimum_size, zest_size pool_size) {
    zest_buffer_usage_t usage = ZEST__ZERO_INIT(zest_buffer_usage_t);
    usage.property_flags = zest_memory_property_device_local_bit;
	pool_size = zest_RoundUpToNearestPower(pool_size);
	minimum_size = zest_RoundUpToNearestPower(minimum_size);
	usage.memory_pool_type = zest_memory_pool_type_transient_buffers;
	zest_SetDevicePoolSize(device, "Transient Buffers", usage, minimum_size, pool_size);
}

void zest_SetGPUSmallTransientBufferPoolSize(zest_device device, zest_size minimum_size, zest_size pool_size) {
    zest_buffer_usage_t usage = ZEST__ZERO_INIT(zest_buffer_usage_t);
    usage.property_flags = zest_memory_property_device_local_bit;
	pool_size = zest_RoundUpToNearestPower(pool_size);
	minimum_size = zest_RoundUpToNearestPower(minimum_size);
	usage.memory_pool_type = zest_memory_pool_type_small_transient_buffers;
	zest_SetDevicePoolSize(device, "Small Transient Buffers", usage, minimum_size, pool_size);
}

void zest_SetStagingBufferPoolSize(zest_device device, zest_size minimum_size, zest_size pool_size) {
    zest_buffer_usage_t usage = ZEST__ZERO_INIT(zest_buffer_usage_t);
    usage.property_flags = zest_memory_property_host_visible_bit | zest_memory_property_host_coherent_bit;
	usage.memory_pool_type = zest_memory_pool_type_buffers;
	pool_size = zest_RoundUpToNearestPower(pool_size);
	minimum_size = zest_RoundUpToNearestPower(minimum_size);
    zest_SetDevicePoolSize(device, "Host buffers", usage, minimum_size, pool_size);
}

void zest_SetSmallHostBufferPoolSize(zest_device device, zest_size minimum_size, zest_size pool_size) {
    zest_buffer_usage_t usage = ZEST__ZERO_INIT(zest_buffer_usage_t);
    usage.property_flags = zest_memory_property_host_visible_bit | zest_memory_property_host_coherent_bit;
	usage.memory_pool_type = zest_memory_pool_type_small_buffers;
	pool_size = zest_RoundUpToNearestPower(pool_size);
	minimum_size = zest_RoundUpToNearestPower(minimum_size);
    zest_SetDevicePoolSize(device, "Small Host buffers", usage, minimum_size, pool_size);
}

zest_compute zest_GetCompute(zest_compute_handle compute_handle) {
	zest_compute compute = (zest_compute)zest__get_store_resource_checked(compute_handle.store, compute_handle.value);
	return compute;
}

void zest_FreeCompute(zest_compute_handle compute_handle) {
    zest_compute compute = (zest_compute)zest__get_store_resource_checked(compute_handle.store, compute_handle.value);
	zest_device device = (zest_device)compute_handle.store->origin;
	zest_uint index = device->frame_counter % ZEST_MAX_FIF;
    zest_vec_push(device->allocator, device->deferred_resource_freeing_list.resources[index], compute);
}

void zest_EnableVSync(zest_context context) {
    ZEST__FLAG(context->flags, zest_context_flag_schedule_change_vsync);
}

void zest_DisableVSync(zest_context context) {
    ZEST__FLAG(context->flags, zest_context_flag_schedule_change_vsync);
}

void zest_SetFrameInFlight(zest_context context, zest_uint fif) {
    ZEST_ASSERT(fif < ZEST_MAX_FIF);        //Frame in flight must be less than the maximum amount of frames in flight
    context->saved_fif = context->current_fif;
    context->current_fif = fif;
}

void zest_RestoreFrameInFlight(zest_context context) {
    context->current_fif = context->saved_fif;
}

float zest_LinearToSRGB(float value) {
    return powf(value, 2.2f);
}

zloc_allocation_stats_t zest_GetDeviceMemoryStats(zest_device device) {
	ZEST_ASSERT_HANDLE(device); 	//Not a valid device handle
	return device->allocator->stats;
}

zloc_allocation_stats_t zest_GetContextMemoryStats(zest_context context) {
	ZEST_ASSERT_HANDLE(context); 	//Not a valid context handle
	return context->allocator->stats;
}

zest_uint zest__grow_capacity(void* T, zest_uint size) {
    zest_uint new_capacity = T ? (size + size / 2) : 8;
    return new_capacity > size ? new_capacity : size;
}

void* zest__vec_reserve(zloc_allocator *allocator, void* T, zest_uint unit_size, zest_uint new_capacity) {
    if (T && new_capacity <= zest__vec_header(T)->capacity) {
        return T;
    }
    void* new_data = ZEST__REALLOCATE(allocator, (T ? zest__vec_header(T) : T), new_capacity * unit_size + zest__VEC_HEADER_OVERHEAD);
    if (!T) memset(new_data, 0, zest__VEC_HEADER_OVERHEAD);
    T = ((char*)new_data + zest__VEC_HEADER_OVERHEAD);
    zest_vec *header = (zest_vec *)new_data;
    if (allocator->user_data) {
        if (ZEST_STRUCT_TYPE(allocator->user_data) == zest_struct_type_device) {
			zest_device device = (zest_device)allocator->user_data;
			header->id = device->vector_id++;
			if (header->id == 84) {
				int d = 0;
			}
        } else if (ZEST_STRUCT_TYPE(allocator->user_data) == zest_struct_type_context) {
			zest_context context = (zest_context)allocator->user_data;
			header->id = context->vector_id++;
			if (header->id == 16) {
				int d = 0;
			}
        }
    }
    header->magic = zest_INIT_MAGIC(zest_struct_type_vector);
    header->capacity = new_capacity;
    return T;
}

void *zest__vec_linear_reserve(zloc_linear_allocator_t *allocator, void *T, zest_uint unit_size, zest_uint new_capacity) {
	if (T && new_capacity <= zest__vec_header(T)->capacity) {
		return T;
	}
    void* new_data = zest__linear_allocate(allocator, new_capacity * unit_size + zest__VEC_HEADER_OVERHEAD);
	if (!new_data) return NULL;
    zest_vec *header = (zest_vec *)new_data;
	if (!T) {
		memset(new_data, 0, zest__VEC_HEADER_OVERHEAD);
	} else {
		zest_vec *current_header = zest__vec_header(T);
		memcpy(((char*)new_data + zest__VEC_HEADER_OVERHEAD), T, current_header->capacity * unit_size);
		header->current_size = current_header->current_size;
	}
    header->magic = zest_INIT_MAGIC(zest_struct_type_vector);
    header->capacity = new_capacity;
    return ((char*)new_data + zest__VEC_HEADER_OVERHEAD);
}

void zest__initialise_bucket_array(zloc_allocator *allocator, zest_bucket_array_t *array, zest_uint element_size, zest_uint bucket_capacity) {
    array->buckets = NULL; // zest_vec will handle initialization on first push
    array->bucket_capacity = bucket_capacity;
    array->current_size = 0;
    array->element_size = element_size;
	array->allocator = allocator;
}

void zest__free_bucket_array(zest_bucket_array_t *array) {
    if (!array || !array->buckets) return;
    // Free each individual bucket
    zest_vec_foreach(i, array->buckets) {
        ZEST__FREE(array->allocator, array->buckets[i]);
    }
    // Free the zest_vec that holds the bucket pointers
    zest_vec_free(array->allocator, array->buckets);
    array->buckets = NULL;
    array->current_size = 0;
}

void *zest__bucket_array_get(zest_bucket_array_t *array, zest_uint index) {
    if (!array || index >= array->current_size) {
        return NULL;
    }
    zest_uint bucket_index = index / array->bucket_capacity;
    zest_uint index_in_bucket = index % array->bucket_capacity;
    return (void *)((char *)array->buckets[bucket_index] + index_in_bucket * array->element_size);
}

void *zest__bucket_array_add(zest_bucket_array_t *array) {
    ZEST_ASSERT(array);
    // If the array is empty or the last bucket is full, allocate a new one.
    if (zest_vec_empty(array->buckets) || (array->current_size % array->bucket_capacity == 0)) {
        void *new_bucket = ZEST__ALLOCATE(array->allocator, array->element_size * array->bucket_capacity);
		if (!new_bucket) return NULL;
        zest_vec_push(array->allocator, array->buckets, new_bucket);
    }

    // Get the pointer to the new element's location
    zest_uint bucket_index = (array->current_size) / array->bucket_capacity;
    zest_uint index_in_bucket = (array->current_size) % array->bucket_capacity;
    void *new_element = (void *)((char *)array->buckets[bucket_index] + index_in_bucket * array->element_size);

    array->current_size++;
    return new_element;
}

inline void *zest__bucket_array_linear_add(zloc_linear_allocator_t *allocator, zest_bucket_array_t *array) {
    ZEST_ASSERT(array);
    // If the array is empty or the last bucket is full, allocate a new one.
    if (zest_vec_empty(array->buckets) || (array->current_size % array->bucket_capacity == 0)) {
		void *new_bucket = zest__linear_allocate(allocator, array->element_size * array->bucket_capacity);
		if (!new_bucket) {
			return NULL;
		}
        zest_vec_linear_push(allocator, array->buckets, new_bucket);
    }

    // Get the pointer to the new element's location
    zest_uint bucket_index = (array->current_size) / array->bucket_capacity;
    zest_uint index_in_bucket = (array->current_size) % array->bucket_capacity;
    void *new_element = (void *)((char *)array->buckets[bucket_index] + index_in_bucket * array->element_size);

    array->current_size++;
    return new_element;
}

void zest__free_store(zest_resource_store_t *store) { 
	zest_vec_foreach(i, store->data.buckets) {
		void *bucket = store->data.buckets[i];
		memset(bucket, 0, store->data.bucket_capacity * store->data.element_size);
	}
	zest__free_bucket_array(&store->data);
	zest_vec_free(store->data.allocator, store->generations);
	zest_vec_free(store->data.allocator, store->free_slots);
	zest_vec_free(store->data.allocator, store->initialised);
	zest__sync_cleanup(&store->sync);
	*store = ZEST__ZERO_INIT(zest_resource_store_t);
}

void zest__clear_store(zest_resource_store_t *store) {
	zest_vec_foreach(i, store->data.buckets) {
		void *bucket = store->data.buckets[i];
		memset(bucket, 0, store->data.bucket_capacity * store->data.element_size);
	}
	zest__free_bucket_array(&store->data);
	zest_vec_clear(store->free_slots);
	zest_vec_clear(store->generations);
	zest_vec_clear(store->initialised);
}

zest_uint zest__size_in_bytes_store(zest_resource_store_t *store) {
    return store->data.current_size * store->data.element_size;
}

zest_handle zest__add_store_resource(zest_resource_store_t *store) {
	zest_uint index;                                                                                                           
	zest_uint generation;                                                                                                      
	zest__sync_lock(&store->sync);
	if (zest_vec_size(store->free_slots) > 0) {
		index = zest_vec_back(store->free_slots);                                                                          
		zest_vec_pop(store->free_slots);                                                                                  
		generation = ++store->generations[index]; 
	} else {
		index = store->data.current_size;                                                                                             
		zest_vec_push(store->data.allocator, store->generations, 1);                                                                             
		generation = 1;                                                                       
		zest__bucket_array_add(&store->data);
	}                           
	zest_u64 word_idx = index / ZEST_BITS_PER_WORD;
	zest_u64 bit_idx = index % ZEST_BITS_PER_WORD;
	zest_u64 mask = 1ULL << bit_idx;
	if (word_idx >= zest_vec_size(store->initialised)) {
		zest_vec_push(store->data.allocator, store->initialised, 0ULL);
	}
	store->initialised[word_idx] &= ~mask;
	zest__sync_unlock(&store->sync);
	return ZEST_STRUCT_LITERAL(zest_handle, ZEST_CREATE_HANDLE(generation, index));
}

void zest__activate_resource(zest_resource_store_t *store, zest_handle handle) {
	zest_uint index = ZEST_HANDLE_INDEX(handle);
	zest_u64 word_idx = index / ZEST_BITS_PER_WORD;
	zest_u64 bit_idx = index % ZEST_BITS_PER_WORD;
	zest__sync_lock(&store->sync);
	store->initialised[word_idx] |= (1ULL << bit_idx);
	zest__sync_unlock(&store->sync);
}

void zest__remove_store_resource(zest_resource_store_t *store, zest_handle handle) {
	zest_uint index = ZEST_HANDLE_INDEX(handle);                                                                     
	zest__sync_lock(&store->sync);
	zest_vec_push(store->data.allocator, store->free_slots, index);                                                                               
	zest_u64 word_idx = index / ZEST_BITS_PER_WORD;
	zest_u64 bit_idx = index % ZEST_BITS_PER_WORD;
	store->initialised[word_idx] &= ~(1ULL << bit_idx);
	zest__sync_unlock(&store->sync);
}                                                                                                                             


void zest__initialise_store(zloc_allocator *allocator, void *origin, zest_resource_store_t *store, zest_uint struct_size, zest_struct_type struct_type) {
    ZEST_ASSERT(!store->data.buckets);   //Must be an empty store
	store->magic = zest_INIT_MAGIC(zest_struct_type_resource_store);
	zest__initialise_bucket_array(allocator, &store->data, struct_size, 32);
    store->alignment = 16;
	store->origin = origin;
	store->struct_type = struct_type;
	zest__sync_init(&store->sync);
}

int zest_GetDeviceResourceCount(zest_device device, zest_device_handle_type type) {
	ZEST_ASSERT(type < zest_max_device_handle_type);
	zest_resource_store_t *store = &device->resource_stores[type];
	return store->data.current_size - zest_vec_size(store->free_slots);
}

void zest__initialise_map(zest_context context, zest_map_t *map, zest_uint element_size, zest_uint capacity) {
	*map = ZEST__ZERO_INIT(zest_map_t);
	map->element_size = element_size;
	map->capacity = capacity;
	map->table = zest__allocate(context->allocator, (element_size + sizeof(zest_key)) * capacity);
	map->filled_slots = (zest_uint*)zest__allocate(context->allocator, sizeof(zest_uint) * capacity);
	memset(map->table, 0, (map->element_size + sizeof(zest_key)) * capacity);
	memset(map->filled_slots, 0, sizeof(zest_uint) * capacity);
}

zest_bool zest__insert_key(zest_map_t *map, zest_key key, void *value) {
	// Check if key already exists first
	void *existing_value;
	if (zest__find_key(map, key, &existing_value)) {
		// Key exists, update the value
		memcpy(existing_value, value, map->element_size);
		return ZEST_TRUE;
	}
	
	// Check for capacity overflow
	if (map->current_size >= map->capacity) {
		return ZEST_FALSE;
	}
	
	zest_uint index = key % map->capacity;
	zest_uint size = map->element_size + sizeof(zest_key);
	zest_key *element = (zest_key*)((char*)map->table + size * index);
	zest_uint probe = 1;
	
	// Find empty slot using quadratic probing
	while (*element != 0) {
		map->collisions++;
		index = (index + probe * probe) % map->capacity;
		element = (zest_key*)((char*)map->table + size * index);
		probe++;
		
		// Safety check to prevent infinite loop
		if (probe > map->capacity) {
			return ZEST_FALSE;
		}
	}
	
	// Store the key and value
	*element = key;
	memcpy((char*)element + sizeof(zest_key), value, map->element_size);
	map->filled_slots[map->current_size] = index;
	map->current_size++;
	return ZEST_TRUE;
}

zest_bool zest__insert(zest_map_t *map, const char *name, void *value) {
	if (!name) return ZEST_FALSE;
	zest_key key = zest_Hash(name, strlen(name), ZEST_HASH_SEED);
	return zest__insert_key(map, key, value);
}

void *zest__at_key(zest_map_t *map, zest_key key) {
	if (map->current_size == 0) return NULL;
	
	zest_uint index = key % map->capacity;
	zest_uint size = map->element_size + sizeof(zest_key);
	zest_key *element = (zest_key*)((char*)map->table + size * index);
	zest_uint probe = 1;
	
	while (*element != 0) {
		if (key == *element) {
			return (char*)element + sizeof(zest_key);
		}
		index = (index + probe * probe) % map->capacity;
		element = (zest_key*)((char*)map->table + size * index);
		probe++;
		
		// Safety check to prevent infinite loop
		if (probe > map->capacity) {
			return NULL;
		}
	}
	
	// Key not found
	return NULL;
}

void *zest__at_index(zest_map_t *map, zest_uint index) {
	if (index >= map->capacity) return NULL;
	
	zest_uint size = map->element_size + sizeof(zest_key);
	zest_key *element = (zest_key*)((char*)map->table + size * index);
	return (char*)element + sizeof(zest_key);
}

void *zest__at(zest_map_t *map, const char *name) {
	if (!name) return NULL;
	zest_key key = zest_Hash(name, strlen(name), ZEST_HASH_SEED);
	return zest__at_key(map, key);
}

zest_bool zest__find_key(zest_map_t *map, zest_key key, void **out_value) {
	if (!out_value || map->current_size == 0) return ZEST_FALSE;
	
	zest_uint index = key % map->capacity;
	zest_uint size = map->element_size + sizeof(zest_key);
	zest_key *element = (zest_key*)((char*)map->table + size * index);
	zest_uint probe = 1;
	
	while (*element != 0) {
		if (key == *element) {
			*out_value = (char*)element + sizeof(zest_key);
			return ZEST_TRUE;
		}
		index = (index + probe * probe) % map->capacity;
		element = (zest_key*)((char*)map->table + size * index);
		probe++;
		
		// Safety check to prevent infinite loop
		if (probe > map->capacity) {
			break;
		}
	}
	
	*out_value = NULL;
	return ZEST_FALSE;
}

zest_bool zest__find(zest_map_t *map, const char *name, void **out_value) {
	if (!name || !out_value) return ZEST_FALSE;
	zest_key key = zest_Hash(name, strlen(name), ZEST_HASH_SEED);
	return zest__find_key(map, key, out_value);
}

zest_bool zest__ensure_capacity(zest_map_t *map, zest_uint required_capacity) {
	if (required_capacity <= map->capacity) {
		return ZEST_TRUE;
	}
	
	// Needs implementing
	ZEST_ALERT("WARNING: zest__ensure_capacity called but not implemented. Current capacity: %u, Required: %u", 
		map->capacity, required_capacity);
	return ZEST_FALSE;
}

void zest__free_map(zest_context context, zest_map_t *map) {
	if (!map) return;
	
	if (map->table) {
		ZEST__FREE(context->allocator, map->table);
		map->table = NULL;
	}
	
	if (map->filled_slots) {
		ZEST__FREE(context->allocator, map->filled_slots);
		map->filled_slots = NULL;
	}
	
	// Reset all fields
	memset(map, 0, sizeof(zest_map_t));
}

void zest_SetText(zloc_allocator *allocator, zest_text_t* buffer, const char* text) {
    if (!text) {
        zest_FreeText(allocator, buffer);
        return;
    }
    zest_uint length = (zest_uint)strlen(text) + 1;
    zest_vec_resize(allocator, buffer->str, length);
    zest_strcpy(buffer->str, length, text);
}

void zest_ResizeText(zloc_allocator *allocator, zest_text_t *buffer, zest_uint size) {
    zest_vec_resize(allocator, buffer->str, size);
}

void zest_SetTextfv(zloc_allocator *allocator, zest_text_t* buffer, const char* text, va_list args)
{
    va_list args2;
    va_copy(args2, args);

	#ifdef _MSC_VER
    int len = vsnprintf(NULL, 0, text, args);
    ZEST_ASSERT(len >= 0);

    if ((int)zest_TextSize(buffer) < len + 1)
    {
        zest_vec_resize(allocator, buffer->str, (zest_uint)(len + 1));
    }
    vsnprintf(buffer->str, len + 1, text, args2);
	#else
    int len = vsnprintf(buffer->str ? buffer->str : NULL, (size_t)zest_TextSize(buffer), text, args);
    ZEST_ASSERT(len >= 0);

    if (zest_TextSize(buffer) < len + 1)
    {
        zest_vec_resize(allocator, buffer->str, len + 1);
        vsnprintf(buffer->str, (size_t)len + 1, text, args2);
    }
	#endif

    ZEST_ASSERT(buffer->str);
}

void zest_AppendTextf(zloc_allocator *allocator, zest_text_t *buffer, const char *format, ...) {
    va_list args;
    va_start(args, format);

    va_list args_copy;
    va_copy(args_copy, args);

    int len = vsnprintf(NULL, 0, format, args);
    ZEST_ASSERT(len >= 0);
    if (len <= 0)
    {
        va_end(args_copy);
        return;
    }

    if (zest_TextSize(buffer)) {
        if (zest_vec_back(buffer->str) == '\0') {
            zest_vec_pop(buffer->str);
        }
    }

    const zest_uint write_off = zest_TextSize(buffer);
    const zest_uint needed_sz = write_off + len + 1;
	zest_vec_resize(allocator, buffer->str, needed_sz);
    vsnprintf(buffer->str + write_off, len + 1, format, args_copy);
    va_end(args_copy);

    va_end(args);
}

void zest_SetTextf(zloc_allocator *allocator, zest_text_t* buffer, const char* text, ...) {
    va_list args;
    va_start(args, text);
    zest_SetTextfv(allocator, buffer, text, args);
    va_end(args);
}

void zest_FreeText(zloc_allocator *allocator, zest_text_t* buffer) {
    zest_vec_free(allocator, buffer->str);
    buffer->str = ZEST_NULL;
}

zest_uint zest_TextLength(zest_text_t* buffer) {
    return (zest_uint)strlen(buffer->str);
}

zest_uint zest_TextSize(zest_text_t* buffer) {
    return zest_vec_size(buffer->str);
}
//End api functions

void zest__log_entry_v(char *str, const char *text, va_list args)
{
    va_list args2;
    va_copy(args2, args);

	#ifdef _MSC_VER
    int len = vsnprintf(NULL, 0, text, args);
    ZEST_ASSERT(len >= 0 && len + 1 < ZEST_LOG_ENTRY_SIZE); //log entry must be within buffer limit

    vsnprintf(str, len + 1, text, args2);
	#else
    int len = vsnprintf(str ? str : NULL, (size_t)ZEST_LOG_ENTRY_SIZE, text, args);
    ZEST_ASSERT(len >= 0 && len + 1 < ZEST_LOG_ENTRY_SIZE); //log entry must be within buffer limit

	vsnprintf(str, (size_t)len + 1, text, args2);
	#endif

    ZEST_ASSERT(str);
}

void zest__add_report(zest_device device, zest_report_category category, int line_number, const char *file_name, const char *function_name, const char *entry, ...) {
    zest_text_t message = ZEST__ZERO_INIT(zest_text_t);
    va_list args;
    va_start(args, entry);
    zest_SetTextfv(device->allocator, &message, entry, args);
    va_end(args);
    zest_key report_hash = zest_Hash(message.str, zest_TextSize(&message), 0);
    if (zest_map_valid_key(device->reports, report_hash)) {
        zest_report_t *report = zest_map_at_key(device->reports, report_hash);
        report->count++;
        zest_FreeText(device->allocator, &message);
    } else {
        zest_report_t report = ZEST__ZERO_INIT(zest_report_t);
        report.count = 1;
        report.line_number = line_number;
        report.file_name = file_name;
        report.function_name = function_name;
        report.message = message;
        report.category = category;
        zest_map_insert_key(device->allocator, device->reports, report_hash, report);
    }
}

void zest__log_entry(zest_context context, const char *text, ...) {
    va_list args;
    va_start(args, text);
    zest_log_entry_t entry = ZEST__ZERO_INIT(zest_log_entry_t);
    entry.time = zest_Microsecs();
    zest__log_entry_v(entry.str, text, args);
    zest_vec_push(context->device->allocator, context->device->debug.frame_log, entry);
    va_end(args);
}

void zest__reset_frame_log(zest_context context, char *str, const char *entry, ...) {
    zest_vec_clear(context->device->debug.frame_log);
    context->device->debug.function_depth = 0;
}

void zest__log_validation_error(zest_device device, const char *message) {
    if (ZEST__FLAGGED(device->init_flags, zest_device_init_flag_log_validation_errors_to_console)) {
        ZEST_ALERT("Zest Validation Error: %s", message);
    }
    if (ZEST__FLAGGED(device->init_flags, zest_device_init_flag_log_validation_errors_to_memory)) {
        zest_key key = zest_Hash(message, (zest_uint)strlen(message), ZEST_HASH_SEED);
        if (!zest_map_valid_key(device->validation_errors, key)) {
            zest_text_t error = ZEST__ZERO_INIT(zest_text_t);
            zest_SetText(device->allocator, &error, message);
            zest_map_insert_key(device->allocator, device->validation_errors, key, error);
        }
    }
}

void zest__add_line(zest_text_t *text, char current_char, zest_uint *position, zest_uint tabs) {
    ZEST_ASSERT(*position < zest_TextSize(text));
    text->str[(*position)++] = current_char;
    ZEST_ASSERT(*position < zest_TextSize(text));
    text->str[(*position)++] = '\n';
    for (int t = 0; t != tabs; ++t) {
        text->str[(*position)++] = '\t';
    }
}

zest_sampler_handle zest_CreateSampler(zest_context context, zest_sampler_info_t *info) {
	zest_device device = context->device;
	zest_resource_store_t *store = &device->resource_stores[zest_handle_type_samplers];
	zest_sampler_handle sampler_handle = ZEST_STRUCT_LITERAL(zest_sampler_handle, zest__add_store_resource(store), store );
	sampler_handle.store = store;
    zest_sampler sampler = (zest_sampler)zest__get_store_resource_unsafe(store, sampler_handle.value);
    sampler->magic = zest_INIT_MAGIC(zest_struct_type_sampler);
    sampler->create_info = *info;
    sampler->backend = (zest_sampler_backend)context->device->platform->new_sampler_backend(context);
    sampler->handle = sampler_handle;

    if (device->platform->create_sampler(sampler)) {
		zest__activate_resource(sampler_handle.store, sampler_handle.value);
        return sampler_handle;
    }

    zest__cleanup_sampler(sampler);
    return ZEST__ZERO_INIT(zest_sampler_handle);
}

zest_sampler_info_t zest_CreateSamplerInfo() {
    zest_sampler_info_t sampler_info = ZEST__ZERO_INIT(zest_sampler_info_t);
    sampler_info.mag_filter = zest_filter_linear;
    sampler_info.min_filter = zest_filter_linear;
    sampler_info.address_mode_u = zest_sampler_address_mode_clamp_to_edge;
    sampler_info.address_mode_v = zest_sampler_address_mode_clamp_to_edge;
    sampler_info.address_mode_w = zest_sampler_address_mode_clamp_to_edge;
    sampler_info.anisotropy_enable = ZEST_FALSE;
    sampler_info.max_anisotropy = 1.f;
    sampler_info.compare_enable = ZEST_FALSE;
    sampler_info.compare_op = zest_compare_op_always;
    sampler_info.mipmap_mode = zest_mipmap_mode_linear;
    sampler_info.border_color = zest_border_color_float_transparent_black;
    sampler_info.mip_lod_bias = 0.f;
    sampler_info.min_lod = 0.0f;
    sampler_info.max_lod = 14.0f;
    return sampler_info;
}

zest_sampler_info_t zest_CreateSamplerInfoRepeat() {
    zest_sampler_info_t sampler_info = zest_CreateSamplerInfo();
    sampler_info.address_mode_u = zest_sampler_address_mode_repeat;
    sampler_info.address_mode_v = zest_sampler_address_mode_repeat;
    sampler_info.address_mode_w = zest_sampler_address_mode_repeat;
	return sampler_info;
}

zest_sampler_info_t zest_CreateSamplerInfoMirrorRepeat() {
    zest_sampler_info_t sampler_info = zest_CreateSamplerInfo();
    sampler_info.address_mode_u = zest_sampler_address_mode_mirrored_repeat;
    sampler_info.address_mode_v = zest_sampler_address_mode_mirrored_repeat;
    sampler_info.address_mode_w = zest_sampler_address_mode_mirrored_repeat;
	return sampler_info;
}

zest_sampler zest_GetSampler(zest_sampler_handle handle) {
	zest_sampler sampler = (zest_sampler)zest__get_store_resource_checked(handle.store, handle.value);
	return sampler;
}

void zest_FreeSampler(zest_sampler_handle handle) {
	if (!handle.value) return;
    zest_sampler sampler = zest_GetSampler(handle);
	zest_device device = (zest_device)handle.store->origin;
	zest_uint index = device->frame_counter % ZEST_MAX_FIF;
    zest_vec_push(device->allocator, device->deferred_resource_freeing_list.resources[index], sampler);
}

void zest_FreeSamplerNow(zest_sampler_handle handle) {
	if (!handle.value) return;
    zest_sampler sampler = zest_GetSampler(handle);
	zest_device device = (zest_device)handle.store->origin;
	zest__free_handle(device->allocator, sampler);
}

void zest__prepare_standard_pipelines(zest_device device) {
	//TODO: this needs looking at. Do we even need this at all?
	zest_pipeline_layout_info_t info = zest_NewPipelineLayoutInfo(device);
	zest_pipeline_layout swap_layout = zest_CreatePipelineLayout(&info);

    //Final Render Pipelines
    device->pipeline_templates.swap = zest_CreatePipelineTemplate(device, "pipeline_swap_chain");
    zest_pipeline_template swap = device->pipeline_templates.swap;
    zest_SetPipelineBlend(swap, zest_PreMultiplyBlendStateForSwap());
    swap->no_vertex_input = ZEST_TRUE;
    zest_SetPipelineVertShader(swap, device->builtin_shaders.swap_vert);
    zest_SetPipelineFragShader(swap, device->builtin_shaders.swap_frag);
    swap->uniforms = 0;
    swap->flags = zest_pipeline_set_flag_is_render_target_pipeline;

	zest_SetPipelineLayout(device->pipeline_templates.swap, swap_layout);
    swap->depth_stencil.depth_write_enable = ZEST_FALSE;
    swap->depth_stencil.depth_test_enable = ZEST_FALSE;

    swap->color_blend_attachment = zest_PreMultiplyBlendStateForSwap();

    ZEST_APPEND_LOG(device->log_path.str, "Final render pipeline");
}

// --End Renderer functions

// --General Helper Functions
zest_viewport_t zest_CreateViewport(float x, float y, float width, float height, float minDepth, float maxDepth) {
    zest_viewport_t viewport = ZEST__ZERO_INIT(zest_viewport_t);
    viewport.width = width;
    viewport.height = height;
    viewport.minDepth = minDepth;
    viewport.maxDepth = maxDepth;
    return viewport;
}

zest_scissor_rect_t zest_CreateRect2D(zest_uint width, zest_uint height, int offsetX, int offsetY) {
    zest_scissor_rect_t rect2D = ZEST__ZERO_INIT(zest_scissor_rect_t);
    rect2D.extent.width = width;
    rect2D.extent.height = height;
    rect2D.offset.x = offsetX;
    rect2D.offset.y = offsetY;
    return rect2D;
}

zest_bool zest__create_transient_image(zest_context context, zest_resource_node resource) {
    zest_image image = &resource->image;
    image->magic = zest_INIT_MAGIC(zest_struct_type_image);
    image->backend = (zest_image_backend)context->device->platform->new_frame_graph_image_backend(&context->frame_graph_allocator[context->current_fif], image, NULL);
    for (int i = 0; i != zest_max_global_binding_number; ++i) {
        image->bindless_index[i] = ZEST_INVALID;
    }
    image->info.flags |= zest_image_flag_transient;
    image->info.flags |= zest_image_flag_device_local;
    image->info.aspect_flags = zest__determine_aspect_flag_for_view(resource->image.info.format);
    image->info.mip_levels = resource->image.info.mip_levels > 0 ? resource->image.info.mip_levels : 1;
    if (ZEST__FLAGGED(image->info.flags, zest_image_flag_cubemap)) {
        ZEST_ASSERT(image->info.layer_count > 0 && image->info.layer_count % 6 == 0); // Cubemap must have layers in multiples of 6!
    }
    if (ZEST__FLAGGED(resource->image.info.flags, zest_image_flag_generate_mipmaps) && image->info.mip_levels == 1) {
        image->info.mip_levels = (zest_uint)floor(log2(ZEST__MAX(resource->image.info.extent.width, resource->image.info.extent.height))) + 1;
    }
    if (!context->device->platform->create_image(context->device, image, image->info.layer_count, zest_sample_count_1_bit, resource->image.info.flags)) {
		context->device->platform->cleanup_image_backend(image);
        return ZEST_FALSE;
    }
	image->handle.store = &context->device->resource_stores[zest_handle_type_images];
    image->info.layout = resource->image_layout = zest_image_layout_undefined;
    zest_image_view_type view_type = zest__get_image_view_type(image);
    image->default_view = context->device->platform->create_image_view(context->device, image, view_type, image->info.mip_levels, 0, 0, image->info.layer_count, &context->frame_graph_allocator[context->current_fif]);
	image->default_view->handle.store = &context->device->resource_stores[zest_handle_type_views];
    resource->view = image->default_view;
	resource->linked_layout = &image->info.layout;

    return ZEST_TRUE;
}

zest_uint zest__next_fif(zest_context context) {
    return (context->current_fif + 1) % ZEST_MAX_FIF;
}

zloc_linear_allocator_t *zest__get_scratch_arena(zest_device device) {
	ZEST_ASSERT(device->scratch_arena.current_offset == 0, "Bug in Zest. The scratch arena should be reset to 0 each time it's requested indicating that it wasn't reset the last time it was used in a function.");
	return &device->scratch_arena;
}

zest_create_context_info_t zest_CreateContextInfo() {
	zest_create_context_info_t create_info;
	create_info.title = "Zest Window";
	create_info.frame_graph_allocator_size = zloc__KILOBYTE(256);
	create_info.screen_width = 1280;
	create_info.screen_height = 768;
	create_info.screen_x = 0;
	create_info.screen_y = 50;
	create_info.virtual_width = 1280;
	create_info.virtual_height = 768;
	create_info.semaphore_wait_timeout_ms = 250;
	create_info.max_semaphore_timeout_ms = ZEST_SECONDS_IN_MILLISECONDS(10);
	create_info.color_format = zest_format_b8g8r8a8_unorm;
	create_info.flags = 0;
	create_info.platform = zest_platform_vulkan;
	create_info.memory_pool_size = zloc__MEGABYTE(8);
    return create_info;
}

zest_file zest_ReadEntireFile(zest_device device, const char* file_name, zest_bool terminate) {
    char* buffer = 0;
    FILE* file = NULL;
    file = zest__open_file(file_name, "rb");
    if (!file) {
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
        zest_vec_resize(device->allocator, buffer, (zest_uint)length + 1);
    }
    else {
        zest_vec_resize(device->allocator, buffer, (zest_uint)length);
    }
    if (buffer == 0 || fread(buffer, 1, length, file) != length) {
        zest_vec_free(device->allocator, buffer);
        fclose(file);
        return buffer;
    }
    if (terminate) {
        buffer[length] = '\0';
    }

    fclose(file);
    return buffer;
}

void zest_FreeFile(zest_device device, zest_file file) {
	zest_vec_free(device->allocator, file);
}
// --End General Helper Functions

// --frame graph functions
zest_frame_graph zest__new_frame_graph(zest_context context, const char *name) {
    zest_frame_graph frame_graph = (zest_frame_graph)zest__linear_allocate(&context->frame_graph_allocator[context->current_fif], sizeof(zest_frame_graph_t));
    *frame_graph = ZEST__ZERO_INIT(zest_frame_graph_t);
    frame_graph->magic = zest_INIT_MAGIC(zest_struct_type_frame_graph);
    frame_graph->command_list.magic = zest_INIT_MAGIC(zest_struct_type_frame_graph_context);
	frame_graph->command_list.context = context;
	frame_graph->command_list.device = context->device;
    frame_graph->name = name;
    frame_graph->bindless_layout = context->device->bindless_set_layout;
    frame_graph->bindless_set = context->device->bindless_set;
    zest_bucket_array_init(context->allocator, &frame_graph->resources, zest_resource_node_t, 8);
    zest_bucket_array_init(context->allocator, &frame_graph->potential_passes, zest_pass_node_t, 8);
    return frame_graph;
}

zest_key zest__hash_frame_graph_cache_key(zest_frame_graph_cache_key_t *cache_key) {
    zest_key key = zest_Hash(&cache_key->auto_state, sizeof(zest_frame_graph_auto_state_t), ZEST_HASH_SEED);
    if (cache_key->user_state && cache_key->user_state_size) {
        key += zest_Hash(cache_key->user_state, cache_key->user_state_size, ZEST_HASH_SEED);
    }
    return key;
}

void zest__cache_frame_graph(zest_frame_graph frame_graph) {
    ZEST_ASSERT_HANDLE(frame_graph);        //Not a valid frame graph! Make sure you called BeginRenderGraph or BeginRenderToScreen
	zest_context context = zest__frame_graph_builder->context;
	//Don't cache the frame graph if the frame graph allocator had to be increased in size.
	if (context->frame_graph_allocator[context->current_fif].next) {
		zest_size capacity = zloc_GetLinearAllocatorCapacity(&context->frame_graph_allocator[context->current_fif]);
		ZEST_ALERT("Cannot cache the frame graph this frame as the frame graph allocator was not big enough. Consider increasing the size of the frame graph allocator by setting frame_graph_allocation_size in the create info of the context when you create it. The current capacity of the frame graph after being grown is %llu bytes.", capacity);
		return;
	}
    if (!frame_graph->cache_key) return;    //Only cache if there's a key
    if (frame_graph->error_status) return;  //Don't cache a frame graph that had errors
	/*
	Improvement required here: If you execute multiple frame graphs in a frame and one is cached but the 
	prior frame graphs were not cached then this doesn't currently take into account the fact that the start
	of the allocation space will be filled with a frame graph that is unused and therefore shouldn't be included
	in the cached memory. Care also needs to be taken to preserve the memory of the old frame graphs so that 
	any deferred resources for destruction can be taken care of. So the likely solution will be to create a new 
	block starting from the caching frame graph offset for it's capacity, trimming the free space at the end 
	then setting the capacity of the linear allocator to the offset of the frame graph that was cached. Then 
	when the allocator is reset n frames in the future it should check the capacity of the allocator, if it's 
	lower then what it should be then free it and re-allocate. 
	If frame graphs are then continued to be created after that the linear allocator will be at capacity so therefore
	caching will not be possible at that point until the allocator is consolidated and remade. I don't know if
	this is a problem or not, need tests to experiment.

	Aside from that caching is very fast as all we do is promote the linear memory space that the frame graph
	used and promote it to persistent memory, then allocate a new linear allocator for other graphs to use.
	*/
	zest_size offset = context->frame_graph_allocator[context->current_fif].current_offset;
	frame_graph->cached_size = offset;
    zest_cached_frame_graph_t new_cached_graph = {
        zloc_PromoteLinearBlock(context->allocator, context->frame_graph_allocator[context->current_fif].data, context->frame_graph_allocator[context->current_fif].current_offset),
        frame_graph
    };
    ZEST__FLAG(frame_graph->flags, zest_frame_graph_is_cached);
	void *frame_graph_linear_memory = ZEST__ALLOCATE(context->allocator, context->create_info.frame_graph_allocator_size);
	int result = zloc_InitialiseLinearAllocator(&context->frame_graph_allocator[context->current_fif], frame_graph_linear_memory, context->create_info.frame_graph_allocator_size);
	ZEST_ASSERT(result, "Unable to allocate a new frame graph allocator.");
    if (zest_map_valid_key(context->cached_frame_graphs, frame_graph->cache_key)) {
        zest_cached_frame_graph_t *cached_graph = zest_map_at_key(context->cached_frame_graphs, frame_graph->cache_key);
        ZEST__FREE(context->allocator, cached_graph->memory);
        *cached_graph = new_cached_graph;
    } else {
        zest_map_insert_key(context->allocator, context->cached_frame_graphs, frame_graph->cache_key, new_cached_graph);
    }
}

zest_image_view zest__swapchain_resource_provider(zest_context context, zest_resource_node resource) {
    return resource->swapchain->views[resource->swapchain->current_image_frame];
}

zest_buffer zest__instance_layer_resource_provider(zest_context context, zest_resource_node resource) {
    zest_layer layer = (zest_layer)resource->user_data;
    layer->vertex_buffer_node = resource;
    zest__end_instance_instructions(layer); //Make sure the staging buffer memory in use is up to date
	zest_size layer_size = layer->memory_refs[layer->fif].instance_count * layer->instance_struct_size;
	if (layer_size == 0) return NULL;
	layer->fif = context->current_fif;
	layer->dirty[context->current_fif] = 1;
    resource->buffer_desc.size = layer_size;
    return NULL;
}

zest_buffer zest__instance_layer_resource_provider_prev_fif(zest_context context, zest_resource_node resource) {
    zest_layer layer = (zest_layer)resource->user_data;
    layer->vertex_buffer_node = resource;
    zest__end_instance_instructions(layer); //Make sure the staging buffer memory in use is up to date
	resource->bindless_index[0] = layer->memory_refs[layer->prev_fif].descriptor_array_index;
	return layer->memory_refs[layer->prev_fif].device_vertex_data;
}

zest_buffer zest__instance_layer_resource_provider_current_fif(zest_context context, zest_resource_node resource) {
    zest_layer layer = (zest_layer)resource->user_data;
    layer->vertex_buffer_node = resource;
    zest__end_instance_instructions(layer); //Make sure the staging buffer memory in use is up to date
	resource->bindless_index[0] = layer->memory_refs[layer->fif].descriptor_array_index;
	return layer->memory_refs[layer->fif].device_vertex_data;
}

zest_frame_graph zest_GetCachedFrameGraph(zest_context context, zest_frame_graph_cache_key_t *cache_key) {
	if (!cache_key) return NULL;
	zest_key key = zest__hash_frame_graph_cache_key(cache_key);
    if (zest_map_valid_key(context->cached_frame_graphs, key)) {
        zest_cached_frame_graph_t *cached_graph = zest_map_at_key(context->cached_frame_graphs, key);
        return cached_graph->frame_graph;
    }
    return NULL;
}

void zest_FlushCachedFrameGraphs(zest_context context) {
	zest_map_foreach(i, context->cached_frame_graphs) {
		zest_cached_frame_graph_t cached_graph = context->cached_frame_graphs.data[i];
		ZEST__FREE(context->allocator, cached_graph.memory);
	}
	zest_map_clear(context->cached_frame_graphs);
}

zest_frame_graph_cache_key_t zest_InitialiseCacheKey(zest_context context, const void *user_state, zest_size user_state_size) {
    zest_frame_graph_cache_key_t key = ZEST__ZERO_INIT(zest_frame_graph_cache_key_t);
    key.auto_state.render_format = context->swapchain->format;
    key.auto_state.render_width = (zest_uint)context->swapchain->size.width;
    key.auto_state.render_height = (zest_uint)context->swapchain->size.height;
	key.auto_state.vsync = ZEST__FLAGGED(context->flags, zest_context_flag_vsync_enabled);
    key.user_state = user_state;
    key.user_state_size = user_state_size;
    return key;
}

zest_bool zest_BeginFrameGraph(zest_context context, const char *name, zest_frame_graph_cache_key_t *cache_key) {
	ZEST_ASSERT_OR_VALIDATE(ZEST__NOT_FLAGGED(context->flags, zest_context_flag_building_frame_graph), 
							context->device, "Frame graph already being built. You cannot build a frame graph within another begin frame graph process.",
							ZEST_FALSE);  
	
	ZEST_ASSERT(zest__frame_graph_builder == NULL);		//The thread local frame graph builder must be null
	//If it's not null then is there already a frame graph being set up, and 
	//did you call EndFrameGraph?

	zest__frame_graph_builder = (zest_frame_graph_builder)zest__linear_allocate(&context->frame_graph_allocator[context->current_fif], sizeof(zest_frame_graph_builder_t));
	zest__frame_graph_builder->context = context;
	zest__frame_graph_builder->current_pass = 0;

    zest_key key = 0;
    if (cache_key) {
        key = zest__hash_frame_graph_cache_key(cache_key);
    }
    zest_frame_graph frame_graph = zest__new_frame_graph(context, name);
    frame_graph->cache_key = key;

	zest_device device = context->device;
    frame_graph->semaphores = device->platform->get_frame_graph_semaphores(context, name);
    frame_graph->command_list.backend = (zest_command_list_backend)device->platform->new_frame_graph_context_backend(context);

	ZEST__UNFLAG(frame_graph->flags, zest_frame_graph_expecting_swap_chain_usage);
	ZEST__FLAG(context->flags, zest_context_flag_building_frame_graph);
	zest__frame_graph_builder->frame_graph = frame_graph;

	zest_SetDescriptorSets(device->pipeline_layout, &device->bindless_set, 1);
    return ZEST_TRUE;
}

zest_bool zest__is_stage_compatible_with_qfi(zest_pipeline_stage_flags stages_to_check, zest_device_queue_type queue_family_capabilities) {
    if (stages_to_check == zest_pipeline_stage_none) { // Or just == 0
        return ZEST_TRUE; // No stages specified is trivially compatible
    }

    zest_pipeline_stage_flags current_stages_to_evaluate = stages_to_check;

    // Iterate through each individual bit set in stages_to_check
    while (current_stages_to_evaluate != 0) {
        // Get the lowest set bit
        zest_pipeline_stage_flags single_stage_bit = current_stages_to_evaluate & (~current_stages_to_evaluate + 1);
        // Remove this bit for the next iteration
        current_stages_to_evaluate &= ~single_stage_bit;

        zest_bool stage_is_compatible = ZEST_FALSE;
        switch (single_stage_bit) {
            // Stages allowed on ANY queue type that supports any command submission
			case zest_pipeline_stage_top_of_pipe_bit:
			case zest_pipeline_stage_bottom_of_pipe_bit:
			case zest_pipeline_stage_host_bit: // Host access can always be synchronized against
			case zest_pipeline_stage_all_commands_bit: // Valid, but its scope is limited by queue caps
				stage_is_compatible = ZEST_TRUE;
				break;

            // Stages requiring GRAPHICS_BIT capability
			case zest_pipeline_stage_vertex_input_bit:
			case zest_pipeline_stage_index_input_bit:
			case zest_pipeline_stage_vertex_shader_bit:
			case zest_pipeline_stage_tessellation_control_shader_bit:
			case zest_pipeline_stage_tessellation_evaluation_shader_bit:
			case zest_pipeline_stage_geometry_shader_bit:
			case zest_pipeline_stage_fragment_shader_bit:
			case zest_pipeline_stage_early_fragment_tests_bit:
			case zest_pipeline_stage_late_fragment_tests_bit:
			case zest_pipeline_stage_color_attachment_output_bit:
			case zest_pipeline_stage_all_graphics_bit:
				//Can add more extensions here if needed
				if (queue_family_capabilities & zest_queue_graphics) {
					stage_is_compatible = ZEST_TRUE;
				}
				break;

			// Stage requiring COMPUTE_BIT capability
			case zest_pipeline_stage_compute_shader_bit:
				if (queue_family_capabilities & zest_queue_compute) {
					stage_is_compatible = ZEST_TRUE;
				}
				break;

			// Stage DRAW_INDIRECT can be used by Graphics or Compute
			case zest_pipeline_stage_draw_indirect_bit:
				if (queue_family_capabilities & (zest_queue_graphics | zest_queue_compute)) {
					stage_is_compatible = ZEST_TRUE;
				}
				break;

			// Stage requiring TRANSFER_BIT capability (also often implied by Graphics/Compute)
			case zest_pipeline_stage_transfer_bit:
				if (queue_family_capabilities & (zest_queue_graphics | zest_queue_compute | zest_queue_transfer)) {
					stage_is_compatible = ZEST_TRUE;
				}
				break;

			default:
				stage_is_compatible = ZEST_TRUE; 
				break;
        }

        if (!stage_is_compatible) {
            return ZEST_FALSE; 
        }
    }

    return ZEST_TRUE; 
}

void zest__free_transient_resource(zest_resource_node resource) {
    if (resource->type & zest_resource_type_buffer) {
        zest_FreeBufferNow(resource->storage_buffer);
        resource->storage_buffer = 0;
    } else if (resource->type & zest_resource_type_is_image_or_depth) {
        zest_FreeBufferNow((zest_buffer)resource->image.buffer);
        resource->image.buffer = 0;
    }
}

zest_bool zest__create_transient_resource(zest_context context, zest_resource_node resource) {
    if (resource->type & zest_resource_type_is_image) {
        if (!zest__create_transient_image(context, resource)) {
            return ZEST_FALSE;
        }

        resource->image_layout = zest_image_layout_undefined;
        resource->access_mask = 0;
        resource->last_stage_mask = zest_pipeline_stage_top_of_pipe_bit;
		zest_frame_graph frame_graph = zest__frame_graph_builder->frame_graph;
		zest_vec_linear_push(&context->frame_graph_allocator[context->current_fif], frame_graph->deferred_image_destruction, resource);
    } else if (ZEST__FLAGGED(resource->type, zest_resource_type_buffer) && resource->storage_buffer == NULL) {
		resource->buffer_desc.buffer_info.frame_in_flight = context->current_fif;
		resource->storage_buffer = zest__create_transient_buffer(context, resource->buffer_desc.size, &resource->buffer_desc.buffer_info);
        if (!resource->storage_buffer) {
            return ZEST_FALSE;
        }
        resource->access_mask = 0;
        resource->last_stage_mask = zest_pipeline_stage_top_of_pipe_bit;
    }
    return ZEST_TRUE;
}

#ifdef ZEST_DEBUGGING
void zest__add_image_barrier(zest_resource_node resource, zest_execution_barriers_t *barriers, zest_bool acquire, 
							 zest_access_flags src_access, zest_access_flags dst_access, zest_image_layout old_layout, zest_image_layout new_layout, 
							 zest_uint src_family, zest_uint dst_family, zest_pipeline_stage_flags src_stage, zest_pipeline_stage_flags dst_stage) {
	zest_context context = resource->frame_graph->command_list.context;
	zest_image_barrier_t image_barrier = {
		src_access, src_stage,
		dst_access, dst_stage,
		old_layout, new_layout,
		src_family,
		dst_family
	};
    if (acquire) {
        zest_vec_linear_push(&context->frame_graph_allocator[context->current_fif], barriers->acquire_image_barriers, image_barrier);
    } else {
        zest_vec_linear_push(&context->frame_graph_allocator[context->current_fif], barriers->release_image_barriers, image_barrier);
    }
}

void zest__add_buffer_barrier(zest_resource_node resource, zest_execution_barriers_t *barriers, zest_bool acquire, zest_access_flags src_access, zest_access_flags dst_access,
							  zest_uint src_family, zest_uint dst_family, zest_pipeline_stage_flags src_stage, zest_pipeline_stage_flags dst_stage) {
	zest_context context = resource->frame_graph->command_list.context;
	zest_buffer_barrier_t buffer_barrier = ZEST_STRUCT_LITERAL (zest_buffer_barrier_t,
																src_access, src_stage,
																dst_access, dst_stage,
																src_family, dst_family
																);
    if (acquire) {
        zest_vec_linear_push(&context->frame_graph_allocator[context->current_fif], barriers->acquire_buffer_barriers, buffer_barrier);
    } else {
        zest_vec_linear_push(&context->frame_graph_allocator[context->current_fif], barriers->release_buffer_barriers, buffer_barrier);
    }
}
#endif

void zest__add_image_barriers(zest_frame_graph frame_graph, zloc_linear_allocator_t *allocator, zest_resource_node resource, zest_execution_barriers_t *barriers, 
                              zest_resource_state_t *current_state, zest_resource_state_t *prev_state, zest_resource_state_t *next_state) {
	zest_resource_usage_t *current_usage = &current_state->usage;
	zest_image_layout current_usage_layout = current_usage->image_layout;
	zest_context context = zest__frame_graph_builder->context;
    if (!prev_state) {
        //This is the first state of the resource
        //If there's no previous state then we need to see if a barrier is needed to transition from the resource
        //start state. We put this in the acquire barrier as it needs to be put in place before the pass is executed.
        zest_uint src_queue_family_index = resource->current_queue_family_index;
        zest_uint dst_queue_family_index = ZEST_QUEUE_FAMILY_IGNORED;
        if (src_queue_family_index == ZEST_QUEUE_FAMILY_IGNORED) {
            if (resource->image_layout != current_usage_layout ||
                (resource->access_mask & zest_access_write_bits_general) &&
                (current_usage->access_mask & zest_access_read_bits_general)) {
                context->device->platform->add_frame_graph_image_barrier(resource, barriers, ZEST_TRUE,
																		 resource->access_mask, current_usage->access_mask,
																		 resource->image_layout, current_usage_layout,
																		 src_queue_family_index, dst_queue_family_index,
																		 resource->last_stage_mask, current_usage->stage_mask);
				#ifdef ZEST_DEBUGGING
                zest__add_image_barrier(resource, barriers, ZEST_TRUE,
										resource->access_mask, current_usage->access_mask,
										resource->image_layout, current_usage_layout,
										src_queue_family_index, dst_queue_family_index,
										resource->last_stage_mask, current_usage->stage_mask);
				#endif
            }
        } else {
            //This resource already belongs to a queue which means that it's an imported resource
            //If the frame graph is on the graphics queue only then there's no need to acquire from a prior release.
            ZEST_ASSERT(ZEST__FLAGGED(resource->flags, zest_resource_node_flag_imported));
            dst_queue_family_index = current_state->queue_family_index;
            if (src_queue_family_index != ZEST_QUEUE_FAMILY_IGNORED) {
                context->device->platform->add_frame_graph_image_barrier(resource, barriers, ZEST_TRUE,
																		 resource->access_mask, current_usage->access_mask,
																		 resource->image_layout, current_usage_layout,
																		 src_queue_family_index, dst_queue_family_index,
																		 resource->last_stage_mask, current_usage->stage_mask);
				#ifdef ZEST_DEBUGGING
                zest__add_image_barrier(resource, barriers, ZEST_TRUE,
										resource->access_mask, current_usage->access_mask,
										resource->image_layout, current_usage_layout,
										src_queue_family_index, dst_queue_family_index,
										resource->last_stage_mask, current_usage->stage_mask);
				#endif
            }
        }
        zest_bool needs_releasing = ZEST_FALSE;
    } else {
        //There is a previous state, do we need to acquire the resource from a different queue?
        zest_uint src_queue_family_index = ZEST_QUEUE_FAMILY_IGNORED;
        zest_uint dst_queue_family_index = ZEST_QUEUE_FAMILY_IGNORED;
        zest_bool needs_acquiring = ZEST_FALSE;
        if (prev_state && (prev_state->queue_family_index != current_state->queue_family_index || prev_state->was_released)) {
            //The next state is in a different queue so the resource needs to be released to that queue
            src_queue_family_index = prev_state->queue_family_index;
            dst_queue_family_index = current_state->queue_family_index;
            needs_acquiring = ZEST_TRUE;
        }
        if (needs_acquiring) {
            zest_resource_usage_t *prev_usage = &prev_state->usage;
            //Acquire the resource. No transitioning is done here, acquire only if needed
            context->device->platform->add_frame_graph_image_barrier(resource, barriers, ZEST_TRUE,
																	 zest_access_none, current_usage->access_mask,
																	 prev_usage->image_layout, current_usage_layout,
																	 src_queue_family_index, dst_queue_family_index,
																	 zest_pipeline_stage_top_of_pipe_bit, current_usage->stage_mask);
			#ifdef ZEST_DEBUGGING
            zest__add_image_barrier(resource, barriers, ZEST_TRUE,
									zest_access_none, current_usage->access_mask,
									prev_usage->image_layout, current_usage_layout,
									src_queue_family_index, dst_queue_family_index,
									zest_pipeline_stage_top_of_pipe_bit, current_usage->stage_mask);
			#endif
            //Make sure that the batch that this resource is in (at this point) has the correct wait
            //stage to wait on.
            zest_submission_batch_t *batch = zest__get_submission_batch(current_state->submission_id);
            batch->queue_wait_stages |= zest_pipeline_stage_top_of_pipe_bit;
        }
    }
    if (next_state) {
        //If the resource has a state after this one then we can check if a barrier is needed to transition and/or
        //release to another queue family
        zest_uint src_queue_family_index = current_state->queue_family_index;
        zest_uint dst_queue_family_index = next_state->queue_family_index;
        zest_resource_usage_t *next_usage = &next_state->usage;
		zest_image_layout next_usage_layout = next_usage->image_layout;
        zest_bool needs_releasing = ZEST_FALSE;
        if (next_state && next_state->queue_family_index != current_state->queue_family_index) {
            //The next state is in a different queue so the resource needs to be released to that queue
            src_queue_family_index = current_state->queue_family_index;
            dst_queue_family_index = next_state->queue_family_index;
            needs_releasing = ZEST_TRUE;
        }
        if ((needs_releasing || current_usage->image_layout != next_usage->image_layout ||
            (current_usage->access_mask & zest_access_write_bits_general) &&
            (next_usage->access_mask & zest_access_read_bits_general))) {
            zest_pipeline_stage_flags dst_stage = zest_pipeline_stage_bottom_of_pipe_bit;
            if (needs_releasing) {
                current_state->was_released = ZEST_TRUE;
            } else {
                dst_stage = next_state->usage.stage_mask;
                current_state->was_released = ZEST_FALSE;
            }
            context->device->platform->add_frame_graph_image_barrier(resource, barriers, ZEST_FALSE,
																	 current_usage->access_mask, zest_access_none,
																	 current_usage->image_layout, next_usage_layout,
																	 src_queue_family_index, dst_queue_family_index,
																	 current_usage->stage_mask, dst_stage);
			#ifdef ZEST_DEBUGGING
            zest__add_image_barrier(resource, barriers, ZEST_FALSE,
									current_usage->access_mask, zest_access_none,
									current_usage->image_layout, next_usage_layout,
									src_queue_family_index, dst_queue_family_index,
									current_usage->stage_mask, dst_stage);
			#endif
        }
    } else if (resource->flags & zest_resource_node_flag_imported
        && current_state->queue_family_index != context->device->transfer_queue_family_index) {
        //Maybe we add something for images here if it's needed.
    }
}

zest_bool zest__detect_cyclic_recursion(zest_frame_graph frame_graph, zest_pass_node pass_node) {
    pass_node->visit_state = zest_pass_node_visiting;
    zest_map_foreach(i, pass_node->outputs) {
        zest_resource_usage_t *output_usage = &pass_node->outputs.data[i];
		if (output_usage->access_mask == zest_access_depth_stencil_attachment_read_bit) {
			continue;
		}
        zest_resource_node output_resource = output_usage->resource_node;
        zest_bucket_array_foreach(p_idx, frame_graph->potential_passes) {
            zest_pass_node dependent_pass = zest_bucket_array_get(&frame_graph->potential_passes, zest_pass_node_t, p_idx);
            if (zest_map_valid_name(dependent_pass->inputs, output_resource->name)) {
                if (dependent_pass == pass_node) {
                    continue;
                }
                if (dependent_pass->visit_state == zest_pass_node_visiting) {
                    return ZEST_TRUE; // Signal that a cycle was found
                }
                if (dependent_pass->visit_state == zest_pass_node_unvisited) {
                    if (zest__detect_cyclic_recursion(frame_graph, dependent_pass)) {
                        return ZEST_TRUE; // Propagate the cycle detection signal up
                    }
                }
            }
        }
    }
    pass_node->visit_state = zest_pass_node_visited;
    return ZEST_FALSE; // No cycle found from this pass
}

/*
frame graph compiler index:

Main compile phases:
- Initial cull of passes with no outputs or execution callbacks
- Put non culled passes into the ungrouped_passes list
- Calculate dependencies on these ungrouped passes to build the graph
- Build an ordered execution list from this
- Group passes together that can be based on shared output, input dependencies and queue families
- Build "resource journeys" for each resource for future barrier creation.
- Calculate the lifetimes of resources for transient memory aliasing
- Group these passes in to submission batches based on queue families
- Create semaphores for the submission batches
- Create lists ready for execution for barrier and transient buffer/image craeation during the execution stage
- Process exexution list and create or fetch cached render passes.

[Check_unused_resources_and_passes]
[Set_producers_and_consumers]
[Set_adjacency_list]
[Create_execution_waves]
[Create_command_batches]
[Sync_wave]
[Resource_journeys]
[Calculate_lifetime_of_resources]
[Create_semaphores]
[Plan_transient_buffers]
[Plan_resource_barriers]
[Process_compiled_execution_order]
[Prepare_render_passes]
*/
zest_frame_graph zest__compile_frame_graph() {
	zest_microsecs start_time = zest_Microsecs();
    zest_frame_graph frame_graph = zest__frame_graph_builder->frame_graph;
	zest_context context = zest__frame_graph_builder->context;
	ZEST_ASSERT_HANDLE(frame_graph);        //Not a valid frame graph! Make sure you called BeginRenderGraph or BeginRenderToScreen

	if (ZEST__FLAGGED(frame_graph->flags, zest_frame_graph_expecting_swap_chain_usage) && !frame_graph->signal_timeline) {
		zest_execution_timeline timeline = &context->frame_timeline[context->current_fif];
		zest_SignalTimeline(timeline);
	}

    zloc_linear_allocator_t *allocator = &context->frame_graph_allocator[context->current_fif];

    zest_bucket_array_foreach(i, frame_graph->potential_passes) {
        zest_pass_node pass_node = zest_bucket_array_get(&frame_graph->potential_passes, zest_pass_node_t, i);
		if (pass_node->visit_state == zest_pass_node_unvisited) {
			if (zest__detect_cyclic_recursion(frame_graph, pass_node)) {
				ZEST_REPORT(context->device, zest_report_cyclic_dependency, zest_message_cyclic_dependency, frame_graph->name, pass_node->name);
				ZEST__FLAG(frame_graph->error_status, zest_fgs_cyclic_dependency);
				return frame_graph;
			}
		}
    }

    //Check_unused_resources_and_passes and cull them if necessary
    zest_bool a_pass_was_culled = 0;
    zest_bucket_array_foreach(i, frame_graph->potential_passes) {
        zest_pass_node pass_node = zest_bucket_array_get(&frame_graph->potential_passes, zest_pass_node_t, i);
		if (ZEST__FLAGGED(pass_node->flags, zest_pass_flag_do_not_cull)) continue;
        if (zest_map_size(pass_node->outputs) == 0 || pass_node->execution_callback.callback == 0) {
			//Cull passes that have no output and/or no execution callback and reduce any input resource reference counts 
            zest_map_foreach(j, pass_node->inputs) {
                pass_node->inputs.data[j].resource_node->reference_count--;
            }
            ZEST__FLAG(pass_node->flags, zest_pass_flag_culled);
            a_pass_was_culled = ZEST_TRUE;
            if (zest_map_size(pass_node->outputs) == 0) {
                ZEST_REPORT(context->device, zest_report_pass_culled, zest_message_pass_culled, pass_node->name);
            } else {
                ZEST_REPORT(context->device, zest_report_pass_culled, zest_message_pass_culled_no_work, pass_node->name);
            }
        } else {
            zest_uint reference_counts = 0;
            zest_map_foreach(output_index, pass_node->outputs) {
                zest_resource_node resource = pass_node->outputs.data[output_index].resource_node;
                if (ZEST__FLAGGED(resource->flags, zest_resource_node_flag_essential_output)) {
					reference_counts += 1;
                    continue;
                }
				reference_counts += resource->reference_count;
            }
            if (reference_counts == 0) {
                //Outputs are not consumed by any other pass so this pass can be culled
				zest_map_foreach(j, pass_node->inputs) {
					pass_node->inputs.data[j].resource_node->reference_count--;
				}
                ZEST__FLAG(pass_node->flags, zest_pass_flag_culled);
				a_pass_was_culled = ZEST_TRUE;
				ZEST_REPORT(context->device, zest_report_pass_culled, zest_message_pass_culled_not_consumed, pass_node->name);
            }
        }
    }

    //Keep iterating and culling passes whose output resources have 0 reference counts and keep reducing input resource reference counts
    while (a_pass_was_culled) {
        a_pass_was_culled = ZEST_FALSE;
		zest_bucket_array_foreach(i, frame_graph->potential_passes) {
			zest_pass_node pass_node = zest_bucket_array_get(&frame_graph->potential_passes, zest_pass_node_t, i);
            if (ZEST__NOT_FLAGGED(pass_node->flags, zest_pass_flag_culled) && ZEST__NOT_FLAGGED(pass_node->flags, zest_pass_flag_do_not_cull)) {
                zest_bool all_outputs_non_referenced = ZEST_TRUE;
                zest_map_foreach(j, pass_node->outputs) {
                    zest_resource_node resource = pass_node->outputs.data[j].resource_node;
                    if (resource->reference_count > 0 || ZEST__FLAGGED(resource->flags, zest_resource_node_flag_essential_output)) {
                        all_outputs_non_referenced = ZEST_FALSE;
                        break;
                    }
                }
                if (all_outputs_non_referenced) {
                    ZEST__FLAG(pass_node->flags, zest_pass_flag_culled);
                    a_pass_was_culled = ZEST_TRUE;
                    zest_map_foreach(j, pass_node->inputs) {
                        pass_node->inputs.data[j].resource_node->reference_count--;
                        if (pass_node->inputs.data[j].resource_node->reference_count == 0) {
                            ZEST_REPORT(context->device, zest_report_invalid_reference_counts, zest_message_pass_culled_not_consumed, frame_graph->name);
                            frame_graph->error_status |= zest_fgs_critical_error;
                            return frame_graph;
                        }
                    }
					ZEST_REPORT(context->device, zest_report_pass_culled, zest_message_pass_culled_not_consumed, pass_node->name);
                }
            }
        }
    }

    zest_bool has_execution_callback = ZEST_FALSE;

    //Now group the potential passes in to final passes by ignoring any culled passes and grouping together passes that
    //have the same output. We need to be careful here though to make sure that even though 2 passes might share the same output
    //they might have conflicting dependencies. Output keys are generated by hashing the usage options for the resource.
	zest_bucket_array_foreach(i, frame_graph->potential_passes) {
		zest_pass_node pass_node = zest_bucket_array_get(&frame_graph->potential_passes, zest_pass_node_t, i);
        //Ignore culled passes
        if (ZEST__NOT_FLAGGED(pass_node->flags, zest_pass_flag_culled)) {
            //If no entry for the current pass output key exists, create one.
            if (!zest_map_valid_key(frame_graph->final_passes, pass_node->output_key)) {
                zest_pass_group_t pass_group = ZEST__ZERO_INIT(zest_pass_group_t);
                pass_group.execution_details.barriers.backend = (zest_execution_barriers_backend)context->device->platform->new_execution_barriers_backend(allocator);
                pass_group.queue_info = pass_node->queue_info;
                zest_map_foreach(output_index, pass_node->outputs) {
                    zest_hash_pair pair = pass_node->outputs.map[output_index];
                    zest_resource_usage_t *usage = &pass_node->outputs.data[output_index];
                    if (!ZEST_VALID_HANDLE(usage->resource_node, zest_struct_type_resource_node)) {
                        ZEST_REPORT(context->device, zest_report_invalid_resource, zest_message_usage_has_no_resource, frame_graph->name);
						frame_graph->error_status |= zest_fgs_critical_error;
						return frame_graph;
                    }
					if (ZEST__FLAGGED(usage->resource_node->type, zest_resource_type_swap_chain_image)) {
						ZEST__FLAG(pass_group.flags, zest_pass_flag_outputs_to_swapchain);
					}
                    if (usage->resource_node->reference_count > 0 || ZEST__FLAGGED(usage->resource_node->flags, zest_resource_node_flag_essential_output)) {
                        zest_map_linear_insert(allocator, pass_group.outputs, usage->resource_node->name, *usage);
                    }
                }
                zest_map_foreach(input_index, pass_node->inputs) {
                    zest_hash_pair pair = pass_node->inputs.map[input_index];
					zest_resource_usage_t *usage = &pass_node->inputs.data[input_index];
                    if (!ZEST_VALID_HANDLE(usage->resource_node, zest_struct_type_resource_node)) {
                        ZEST_REPORT(context->device, zest_report_invalid_resource, zest_message_usage_has_no_resource, frame_graph->name);
						frame_graph->error_status |= zest_fgs_critical_error;
						return frame_graph;
                    }
                    zest_map_linear_insert(allocator, pass_group.inputs, usage->resource_node->name, *usage);
                }
                if (pass_node->execution_callback.callback) {
                    has_execution_callback = ZEST_TRUE;
                }
                ZEST__FLAG(pass_group.flags, pass_node->flags);
                zest_vec_linear_push(allocator, pass_group.passes, pass_node);
                zest_map_insert_linear_key(allocator, frame_graph->final_passes, pass_node->output_key, pass_group);
            } else {
                zest_pass_group_t *pass_group = zest_map_at_key(frame_graph->final_passes, pass_node->output_key);
				if (pass_group->queue_info.queue_family_index != pass_node->queue_info.queue_family_index ||
                    pass_group->queue_info.timeline_wait_stage != pass_node->queue_info.timeline_wait_stage) {
					ZEST_REPORT(context->device, zest_report_invalid_pass, zest_message_multiple_swapchain_usage, frame_graph->name);
					frame_graph->error_status |= zest_fgs_critical_error;
					return frame_graph;
				}
                if (pass_node->execution_callback.callback) {
                    has_execution_callback = ZEST_TRUE;
                }
                ZEST__FLAG(pass_group->flags, pass_node->flags);
                zest_vec_linear_push(allocator, pass_group->passes, pass_node);
                zest_map_foreach(input_index, pass_node->inputs) {
                    zest_hash_pair pair = pass_node->inputs.map[input_index];
                    zest_resource_usage_t *usage = &pass_node->inputs.data[pair.index];
                    if (!ZEST_VALID_HANDLE(usage->resource_node, zest_struct_type_resource_node)) {
						ZEST_REPORT(context->device, zest_report_invalid_resource, zest_message_usage_has_no_resource, frame_graph->name);
						frame_graph->error_status |= zest_fgs_critical_error;
						return frame_graph;
                    }
                    zest_map_linear_insert(allocator, pass_group->inputs, usage->resource_node->name, *usage);
                }
            }
        } else {
            frame_graph->culled_passes_count++;
        }
    }

    zest_uint potential_passes = zest_bucket_array_size(&frame_graph->potential_passes);
    zest_uint final_passes = zest_map_size(frame_graph->final_passes);

    if (!has_execution_callback) {
		ZEST_REPORT(context->device, zest_report_no_work, "No passes in frame graph [%s] had any tasks set so there is no work to do in the frame graph.", frame_graph->name);
        zest__set_rg_error_status(frame_graph, zest_fgs_no_work_to_do);
        return frame_graph;
    }

    zest_pass_adjacency_list_t *adjacency_list = 0;
    zest_uint *dependency_count = 0;
    zest_vec_linear_resize(allocator, adjacency_list, zest_map_size(frame_graph->final_passes));
    zest_vec_linear_resize(allocator, dependency_count, zest_map_size(frame_graph->final_passes));

    // Set_producers_and_consumers:
    // For each output in a pass, we set it's producer_pass_idx - the pass that writes the output
    // and for each input in a pass we add all of the comuser_pass_idx's that read the inputs
    zest_map_foreach(i, frame_graph->final_passes) {
        zest_pass_group_t *pass_node = &frame_graph->final_passes.data[i];
        dependency_count[i] = 0; // Initialize in-degree
        zest_pass_adjacency_list_t adj_list = ZEST__ZERO_INIT(zest_pass_adjacency_list_t);
        adjacency_list[i] = adj_list;

		//Confirm the actual queue that will be used. Even though the pass was intended for a specific queue
		//that queue might not be available on the gpu so set the type here to whatever will actually be used.
		zest_uint queue_index = zloc__scan_reverse(pass_node->queue_info.queue_type);
		pass_node->queue_info.queue_type = context->queues[queue_index]->queue_manager->type;

        switch (pass_node->queue_info.queue_type) {
			case zest_queue_graphics:
				pass_node->queue_info.queue = context->queues[ZEST_GRAPHICS_QUEUE_INDEX];
				pass_node->queue_info.queue_family_index = context->device->graphics_queue_family_index;
				break;
			case zest_queue_compute:
				pass_node->queue_info.queue = context->queues[ZEST_COMPUTE_QUEUE_INDEX];
				pass_node->queue_info.queue_family_index = context->device->compute_queue_family_index;
				break;
			case zest_queue_transfer:
				pass_node->queue_info.queue = context->queues[ZEST_TRANSFER_QUEUE_INDEX];
				pass_node->queue_info.queue_family_index = context->device->transfer_queue_family_index;
				break;
        }

		pass_node->compiled_queue_info = pass_node->queue_info;

        zest_map_foreach(j, pass_node->inputs) {
            zest_resource_usage_t *input_usage = &pass_node->inputs.data[j];
            zest_resource_node resource = input_usage->resource_node;
            zest_resource_versions_t *versions = zest_map_at_key(frame_graph->resource_versions, resource->original_id);
            ZEST_ASSERT(versions);  //Bug, this shouldn't be possible, all resources should have a version

            zest_resource_node latest_version = zest_vec_back(versions->resources);
            if (!zest_map_valid_name(pass_node->inputs, latest_version->name)) {
                latest_version->reference_count++;
            }
            input_usage->resource_node = latest_version;

            zest_map_linear_insert(&context->frame_graph_allocator[context->current_fif], pass_node->inputs, latest_version->name, *input_usage);
            zest_vec_linear_push(allocator, resource->consumer_pass_indices, i); // pass 'i' consumes this
        }

        zest_map_foreach(j, pass_node->outputs) {
            zest_resource_usage_t *output_usage = &pass_node->outputs.data[j];
            zest_resource_node resource = output_usage->resource_node;
            zest_resource_versions_t *versions = zest__maybe_add_resource_version(resource);
            zest_resource_node latest_version = zest_vec_back(versions->resources);
            if (latest_version->id != resource->id) {
                if (resource->type == zest_resource_type_swap_chain_image) {
                    ZEST_REPORT(context->device, zest_report_multiple_swapchains, zest_message_multiple_swapchain_usage, frame_graph->name);
                    frame_graph->error_status |= zest_fgs_critical_error;
					return frame_graph;
                }
			    //Add the versioned alias to the outputs instead
                output_usage->resource_node = latest_version;
                zest_map_linear_insert(&context->frame_graph_allocator[context->current_fif], pass_node->outputs, latest_version->name, *output_usage);
                //Check if the user already added this as input:
                if (!zest_map_valid_name(pass_node->inputs, resource->name)) {
                    //If not then add the resource as input for correct dependency chain with default usages
                    zest_resource_usage_t input_usage = ZEST__ZERO_INIT(zest_resource_usage_t);
                    switch (pass_node->queue_info.queue_type) {
						case zest_queue_graphics: {
							if (resource->image.info.flags & zest_image_flag_depth_stencil_attachment) {
								input_usage = zest__configure_image_usage(resource, zest_purpose_depth_stencil_attachment_read_write, resource->image.info.format, output_usage->load_op, output_usage->stencil_load_op, output_usage->stage_mask);
							} else {
								input_usage = zest__configure_image_usage(resource, zest_purpose_color_attachment_read, resource->image.info.format, output_usage->load_op, output_usage->stencil_load_op, output_usage->stage_mask);
							}
							break;
						}
						case zest_queue_compute:
						case zest_queue_transfer:
							input_usage = zest__configure_image_usage(resource, zest_purpose_storage_image_read, resource->image.info.format, output_usage->load_op, output_usage->stencil_load_op, output_usage->stage_mask);
							break;
                    }
                    input_usage.store_op = output_usage->store_op;
                    input_usage.stencil_store_op = output_usage->stencil_store_op;
                    input_usage.clear_value = output_usage->clear_value;
                    input_usage.purpose = output_usage->purpose;
                    resource->reference_count++;
                    zest_map_linear_insert(&context->frame_graph_allocator[context->current_fif], pass_node->inputs, resource->name, input_usage);
                }
            }

            if (output_usage->resource_node->producer_pass_idx != -1 && output_usage->resource_node->producer_pass_idx != i) {
				ZEST_REPORT(context->device, zest_report_multiple_swapchains, zest_message_resource_added_as_ouput_more_than_once, frame_graph->name);
				frame_graph->error_status |= zest_fgs_critical_error;
				return frame_graph;
            }
            output_usage->resource_node->producer_pass_idx = i;
        }

    }

    // Set_adjacency_list
    zest_map_foreach(consumer_idx, frame_graph->final_passes) {
        zest_pass_group_t *consumer_pass = &frame_graph->final_passes.data[consumer_idx];
        zest_map_foreach(j, consumer_pass->inputs) {
            zest_resource_usage_t *input_usage = &consumer_pass->inputs.data[j];
            zest_resource_node resource = input_usage->resource_node;

            int producer_idx = resource->producer_pass_idx;
            if (producer_idx != -1 && producer_idx != consumer_idx) {
                // Dependency: producer_idx ---> consumer_idx
                // Add consumer_idx to the list of passes that producer_idx must precede
                // Ensure no duplicates if a pass reads the same produced resource multiple times in its input list
                zest_bool already_linked = ZEST_FALSE;
                zest_vec_foreach(k_adj, adjacency_list[producer_idx].pass_indices) {
                    if (adjacency_list[producer_idx].pass_indices[k_adj] == consumer_idx) {
                        already_linked = ZEST_TRUE;
                        break;
                    }
                }
                if (!already_linked) {
                    zest_vec_linear_push(allocator, adjacency_list[producer_idx].pass_indices, consumer_idx);
                    dependency_count[consumer_idx]++; // consumer_idx gains an incoming edge
                }
            }
        }
    }
    
	//[Create_execution_waves]
	zest_execution_wave_t *initial_waves = 0;
    zest_execution_wave_t first_wave = ZEST__ZERO_INIT(zest_execution_wave_t);
    zest_uint pass_count = 0;

    int *dependency_queue = 0;
    zest_vec_foreach(i, dependency_count) {
        if (dependency_count[i] == 0) {
            zest_pass_group_t *pass = &frame_graph->final_passes.data[i];
            first_wave.queue_bits |= pass->queue_info.queue_type;
            zest_vec_linear_push(allocator, dependency_queue, i);
            zest_vec_linear_push(allocator, first_wave.pass_indices, i);
            pass_count++;
        }
    }
    if (pass_count > 0) {
		if (zloc__count_bits(first_wave.queue_bits) == 1) {
			zest_vec_foreach(i, first_wave.pass_indices) {
				zest_pass_group_t *pass = &frame_graph->final_passes.data[first_wave.pass_indices[i]];
				if (pass->compiled_queue_info.queue_type != zest_queue_graphics) {
					pass->compiled_queue_info.queue = context->queues[ZEST_GRAPHICS_QUEUE_INDEX];
					pass->compiled_queue_info.queue_family_index = context->device->graphics_queue_family_index;
					pass->compiled_queue_info.queue_type = zest_queue_graphics;
				}
			}
			first_wave.queue_bits = zest_queue_graphics;
		}
        zest_vec_linear_push(allocator, initial_waves, first_wave);
    }

    zest_uint current_wave_index = 0;
    while (current_wave_index < zest_vec_size(initial_waves)) {
        zest_execution_wave_t next_wave = ZEST__ZERO_INIT(zest_execution_wave_t);
        zest_execution_wave_t *current_wave = &initial_waves[current_wave_index];
        next_wave.level = current_wave->level + 1;
        zest_vec_foreach(i, current_wave->pass_indices) {
            int pass_index_of_current_wave = current_wave->pass_indices[i];
            zest_vec_foreach(j, adjacency_list[pass_index_of_current_wave].pass_indices) {
                int dependent_pass_index = adjacency_list[pass_index_of_current_wave].pass_indices[j];
                dependency_count[dependent_pass_index]--;
                if (dependency_count[dependent_pass_index] == 0) {
					zest_pass_group_t *pass = &frame_graph->final_passes.data[dependent_pass_index];
                    next_wave.queue_bits |= pass->queue_info.queue_type;
                    zest_vec_linear_push(allocator, next_wave.pass_indices, dependent_pass_index);
                    pass_count++;
                }
            }
        }
        if (zest_vec_size(next_wave.pass_indices) > 0) {
			//If this wave is NOT an async wave (ie., only a single queue is used) then it should only
			//use the graphics queue.
			if (zloc__count_bits(next_wave.queue_bits) == 1) {
				zest_vec_foreach(i, next_wave.pass_indices) {
					zest_pass_group_t *pass = &frame_graph->final_passes.data[next_wave.pass_indices[i]];
					if (pass->compiled_queue_info.queue_type != zest_queue_graphics) {
						pass->compiled_queue_info.queue = context->queues[ZEST_GRAPHICS_QUEUE_INDEX];
						pass->compiled_queue_info.queue_family_index = context->device->graphics_queue_family_index;
						pass->compiled_queue_info.queue_type = zest_queue_graphics;
					}
				}
				next_wave.queue_bits = zest_queue_graphics;
			}
			zest_vec_linear_push(allocator, initial_waves, next_wave);
        }
        current_wave_index++;
    }

    if (zest_vec_size(initial_waves) == 0) {
        //Nothing to send to the GPU!
        return frame_graph;
    }

	for (zest_uint i = 0; i < zest_vec_size(initial_waves); ++i) {
		zest_execution_wave_t *current_wave = &initial_waves[i];
		if (current_wave->queue_bits == zest_queue_graphics) {
			// This is a graphics wave. Start a new merged wave.
			zest_execution_wave_t merged_wave = *current_wave;
			// Look ahead to see how many subsequent waves are also graphics waves.
			zest_uint waves_to_merge = 0;
			for (zest_uint j = i + 1; j < zest_vec_size(initial_waves); ++j) {
				if (initial_waves[j].queue_bits == zest_queue_graphics) {
					// Merge the passes from the next graphics wave into our new merged_wave.
					zest_vec_foreach(k, initial_waves[j].pass_indices) {
						zest_vec_linear_push(allocator, merged_wave.pass_indices, initial_waves[j].pass_indices[k]);
					}
					waves_to_merge++;
				} else {
					// Stop when we hit a non-graphics (async) wave.
					break;
				}
			}
			// Add the big merged wave to our final list.
			zest_vec_linear_push(allocator, frame_graph->execution_waves, merged_wave);
			// Skip the outer loop ahead past the waves we just merged.
			i += waves_to_merge;
		} else {
			// This is a true async wave (mixed queues), so add it to the list as-is.
			zest_vec_linear_push(allocator, frame_graph->execution_waves, *current_wave);
		}
	}

    //Bug, pass count which is a count of all pass groups added to waves should equal the number of final passes.
    ZEST_ASSERT(pass_count == zest_map_size(frame_graph->final_passes));

    //Create_command_batches
    //We take the waves that we created that identified passes that can run in parallel on separate queues and 
    //organise them into wave submission batches:
    zest_wave_submission_t current_submission = ZEST__ZERO_INIT(zest_wave_submission_t);

    zest_bool interframe_has_waited[ZEST_QUEUE_COUNT] = { 0 };
    zest_bool interframe_has_signalled[ZEST_QUEUE_COUNT] = { 0 };
	zest_uint last_wave_that_presented = ZEST_INVALID;

    zest_vec_foreach(wave_index, frame_graph->execution_waves) {
        zest_execution_wave_t *wave = &frame_graph->execution_waves[wave_index];
        //If the wave has more than one queue then the passes can run in parallel in separate submission batches
        zest_uint queue_type_count = zloc__count_bits(wave->queue_bits);
        //This should be impossible to hit, if no queue bit
        ZEST_ASSERT(queue_type_count);  //Must be using at lease 1 queue
        if (queue_type_count > 1) {
            if (current_submission.batches[0].magic || current_submission.batches[1].magic || current_submission.batches[2].magic) {
				zest_vec_linear_push(allocator, frame_graph->submissions, current_submission);
				current_submission = ZEST__ZERO_INIT(zest_wave_submission_t);
            }
            //Create parallel batches
            zest_vec_foreach(pass_index, wave->pass_indices) {
                zest_uint current_pass_index = wave->pass_indices[pass_index];
                zest_pass_group_t *pass = &frame_graph->final_passes.data[current_pass_index];
                zest_uint qi = zloc__scan_reverse(pass->compiled_queue_info.queue_type);
				if (!current_submission.batches[qi].magic) {
					current_submission.batches[qi].magic = zest_INIT_MAGIC(zest_struct_type_wave_submission);
					current_submission.batches[qi].backend = (zest_submission_batch_backend)context->device->platform->new_submission_batch_backend(context);
					current_submission.batches[qi].queue = pass->compiled_queue_info.queue;
					current_submission.batches[qi].queue_family_index = pass->compiled_queue_info.queue_family_index;
					current_submission.batches[qi].queue_type = pass->compiled_queue_info.queue_type;
					current_submission.batches[qi].need_timeline_wait = interframe_has_waited[qi] ? ZEST_FALSE : ZEST_TRUE;
				}
				current_submission.batches[qi].timeline_wait_stage |= pass->compiled_queue_info.timeline_wait_stage;
				if (ZEST__FLAGGED(pass->flags, zest_pass_flag_outputs_to_swapchain)) {
					current_submission.batches[qi].outputs_to_swapchain = ZEST_TRUE;
					last_wave_that_presented = zest_vec_size(frame_graph->submissions);
				}
				interframe_has_waited[qi] = ZEST_TRUE;
				current_submission.queue_bits |= pass->compiled_queue_info.queue_type;
                zest_vec_linear_push(allocator, current_submission.batches[qi].pass_indices, current_pass_index);
            }
            zest_vec_linear_push(allocator, frame_graph->submissions, current_submission);
            current_submission = ZEST__ZERO_INIT(zest_wave_submission_t);
        } else {
            //Waves that have no parallel submissions 
			zest_uint qi = zloc__scan_reverse(wave->queue_bits);
            if (!current_submission.batches[qi].magic && (current_submission.batches[0].magic || current_submission.batches[1].magic || current_submission.batches[2].magic)) {
                //The queue is different from the last wave, finalise this submission
				zest_vec_linear_push(allocator, frame_graph->submissions, current_submission);
				current_submission = ZEST__ZERO_INIT(zest_wave_submission_t);
            }
            zest_vec_foreach(pass_index, wave->pass_indices) {
                zest_uint current_pass_index = wave->pass_indices[pass_index];
                zest_pass_group_t *pass = &frame_graph->final_passes.data[current_pass_index];
				if (!current_submission.batches[qi].magic) {
					current_submission.batches[qi].magic = zest_INIT_MAGIC(zest_struct_type_wave_submission);
					current_submission.batches[qi].backend = (zest_submission_batch_backend)context->device->platform->new_submission_batch_backend(context);
					current_submission.batches[qi].queue = pass->compiled_queue_info.queue;
					current_submission.batches[qi].queue_family_index = pass->compiled_queue_info.queue_family_index;
					current_submission.batches[qi].queue_type = pass->compiled_queue_info.queue_type;
					current_submission.batches[qi].need_timeline_wait = interframe_has_waited[qi] ? ZEST_FALSE : ZEST_TRUE;
				}
				current_submission.batches[qi].timeline_wait_stage |= pass->compiled_queue_info.timeline_wait_stage;
				if (ZEST__FLAGGED(pass->flags, zest_pass_flag_outputs_to_swapchain)) {
					current_submission.batches[qi].outputs_to_swapchain = ZEST_TRUE;
					last_wave_that_presented = zest_vec_size(frame_graph->submissions);
				}
				interframe_has_waited[qi] = ZEST_TRUE;
				current_submission.queue_bits = pass->compiled_queue_info.queue_type;
                zest_vec_linear_push(allocator, current_submission.batches[qi].pass_indices, current_pass_index);
            }
        }
    }

    //Add the last batch that was being processed if it was a sequential one.
    if (current_submission.batches[0].magic || current_submission.batches[1].magic || current_submission.batches[2].magic) {
        zest_vec_linear_push(allocator, frame_graph->submissions, current_submission);
    }

	//[Sync_wave]
	//If the last wave has multiple queue batches then create a new wave purely to synchronize into the context frame timeline
	zest_execution_wave_t *last_execution_wave = &frame_graph->execution_waves[zest_vec_last_index(frame_graph->execution_waves)];
    if (zloc__count_bits(last_execution_wave->queue_bits) > 1) {
		zest_pass_group_t sync_pass = ZEST__ZERO_INIT(zest_pass_group_t);
		sync_pass.compiled_queue_info.queue = context->queues[ZEST_GRAPHICS_QUEUE_INDEX];
		sync_pass.compiled_queue_info.queue_family_index = context->device->graphics_queue_family_index;
		sync_pass.compiled_queue_info.queue_type = zest_queue_graphics;
		sync_pass.compiled_queue_info.timeline_wait_stage = zest_pipeline_stage_bottom_of_pipe_bit;
		sync_pass.flags = zest_pass_flag_sync_only;
		zest_pass_node pass_node = zest__add_pass_node("Sync Pass", zest_queue_graphics);
		pass_node->type = zest_pass_type_sync;
		pass_node->flags = zest_pass_flag_sync_only;
		zest_vec_linear_push(allocator, sync_pass.passes, pass_node);
		zest_map_linear_insert(allocator, frame_graph->final_passes, "Sync Pass", sync_pass);
		int sync_pass_index = zest_map_last_index(frame_graph->final_passes);
		zest_wave_submission_t sync_submission = ZEST__ZERO_INIT(zest_wave_submission_t);
		sync_submission.queue_bits = zest_queue_graphics;
		sync_submission.batches[0].magic = zest_INIT_MAGIC(zest_struct_type_wave_submission);
		sync_submission.batches[0].backend = (zest_submission_batch_backend)context->device->platform->new_submission_batch_backend(context);
		sync_submission.batches[0].queue = sync_pass.compiled_queue_info.queue;
		sync_submission.batches[0].queue_family_index = sync_pass.compiled_queue_info.queue_family_index;
		sync_submission.batches[0].queue_type = sync_pass.compiled_queue_info.queue_type;
		sync_submission.batches[0].timeline_wait_stage |= sync_pass.compiled_queue_info.timeline_wait_stage;
		zest_execution_wave_t sync_wave = ZEST__ZERO_INIT(zest_execution_wave_t);
		sync_wave.queue_bits = zest_queue_graphics;
		sync_wave.level = last_execution_wave->level + 1;
		zest_vec_linear_push(allocator, sync_wave.pass_indices, sync_pass_index);
		zest_vec_linear_push(allocator, frame_graph->execution_waves, sync_wave);
		zest_vec_linear_push(allocator, sync_submission.batches[0].pass_indices, sync_pass_index);
		zest_vec_linear_push(allocator, frame_graph->submissions, sync_submission);
    }

    //[Resource_journeys]
    //Now that the passes have been grouped into wave submissions we can calculate the resource journey and set the
    //first and last usage index for each resource.
    zest_uint execution_order_index = 0;
    zest_vec_foreach(submission_index, frame_graph->submissions) {
        zest_wave_submission_t *current_wave = &frame_graph->submissions[submission_index];
        for (zest_uint queue_index = 0; queue_index != ZEST_QUEUE_COUNT; ++queue_index) {
            if (!current_wave->batches[queue_index].magic) continue;
            zest_submission_batch_t *batch = &current_wave->batches[queue_index];
            zest_vec_foreach(execution_index, batch->pass_indices) {
                zest_uint current_pass_index = batch->pass_indices[execution_index];
                zest_pass_group_t *current_pass = &frame_graph->final_passes.data[current_pass_index];
                current_pass->submission_id = ZEST__MAKE_SUBMISSION_ID(submission_index, execution_index, queue_index);

                zest_vec_linear_push(allocator, frame_graph->pass_execution_order, current_pass);

                zest_batch_key batch_key = ZEST__ZERO_INIT(zest_batch_key);
                batch_key.current_family_index = current_pass->compiled_queue_info.queue_family_index;

                //Calculate_lifetime_of_resources and also create a state for each resource and plot
                //it's journey through the frame graph so that the appropriate barriers and semaphores
                //can be set up
                zest_map_foreach(input_idx, current_pass->inputs) {
                    zest_resource_node resource_node = current_pass->inputs.data[input_idx].resource_node;
                    if (resource_node->aliased_resource) resource_node = resource_node->aliased_resource;
                    if (resource_node) {
                        resource_node->first_usage_pass_idx = ZEST__MIN(resource_node->first_usage_pass_idx, execution_order_index);
                        resource_node->last_usage_pass_idx = ZEST__MAX(resource_node->last_usage_pass_idx, execution_order_index);
                    }
                    if (resource_node->producer_pass_idx != -1) {
                        zest_pass_group_t *producer_pass = &frame_graph->final_passes.data[resource_node->producer_pass_idx];
                        zest_uint producer_queue_index = ZEST__QUEUE_INDEX(producer_pass->submission_id);
                        if (queue_index != producer_queue_index) {
                            batch->queue_wait_stages |= current_pass->inputs.data[input_idx].stage_mask;
                        }
                    }
                    zest_resource_state_t state = ZEST__ZERO_INIT(zest_resource_state_t);
                    state.pass_index = current_pass_index;
                    state.queue_family_index = current_pass->compiled_queue_info.queue_family_index;
                    state.usage = current_pass->inputs.data[input_idx];
                    state.submission_id = current_pass->submission_id;
                    zest_vec_linear_push(allocator, resource_node->journey, state);
                }

                // Check OUTPUTS of the current pass
                zest_bool requires_new_batch = ZEST_FALSE;
                zest_map_foreach(output_idx, current_pass->outputs) {
                    zest_resource_node resource_node = current_pass->outputs.data[output_idx].resource_node;
                    if (resource_node->aliased_resource) resource_node = resource_node->aliased_resource;
                    if (resource_node) {
                        resource_node->first_usage_pass_idx = ZEST__MIN(resource_node->first_usage_pass_idx, execution_order_index);
                        resource_node->last_usage_pass_idx = ZEST__MAX(resource_node->last_usage_pass_idx, execution_order_index);
                    }
                    zest_resource_state_t state = ZEST__ZERO_INIT(zest_resource_state_t);
                    state.pass_index = current_pass_index;
                    state.queue_family_index = current_pass->compiled_queue_info.queue_family_index;
                    state.usage = current_pass->outputs.data[output_idx];
                    state.submission_id = current_pass->submission_id;
                    zest_vec_linear_push(allocator, resource_node->journey, state);
                    zest_vec_foreach(adjacent_index, adjacency_list[current_pass_index].pass_indices) {
                        zest_uint consumer_pass_index = adjacency_list[current_pass_index].pass_indices[adjacent_index];
                        batch_key.next_pass_indexes |= (1ull << consumer_pass_index);
                        batch_key.next_family_indexes |= (1ull << frame_graph->final_passes.data[consumer_pass_index].compiled_queue_info.queue_family_index);
                    }
                }
                execution_order_index++;
            }
        }
    }

	//[Create_semaphores]
    //Potentially connect the first submission that uses the swap chain image
    if (zest_vec_size(frame_graph->submissions) > 0) {
		// --- Handle imageAvailableSemaphore ---
		if (ZEST__FLAGGED(frame_graph->flags, zest_frame_graph_expecting_swap_chain_usage)) {
            zest_submission_batch_t *first_batch_to_wait = NULL;
            zest_pipeline_stage_flags wait_stage_for_acquire_semaphore = zest_pipeline_stage_top_of_pipe_bit; // Safe default

            zest_resource_node swapchain_node = frame_graph->swapchain_resource; 
            if (swapchain_node->first_usage_pass_idx != ZEST_INVALID) {
                zest_pass_group_t *pass = frame_graph->pass_execution_order[swapchain_node->first_usage_pass_idx];
                zest_uint submission_index = ZEST__SUBMISSION_INDEX(pass->submission_id);
                zest_uint execution_index = ZEST__EXECUTION_INDEX(pass->submission_id);
                zest_uint queue_index = ZEST__QUEUE_INDEX(pass->submission_id);
                zest_wave_submission_t *wave_submission = &frame_graph->submissions[submission_index];
                zest_uint pass_index = wave_submission->batches[queue_index].pass_indices[execution_index];
				zest_bool uses_swapchain = ZEST_FALSE;
				zest_pipeline_stage_flags first_swapchain_usage_stage_in_this_batch = zest_pipeline_stage_top_of_pipe_bit;
                if (zest_map_valid_name(pass->inputs, swapchain_node->name)) {
					zest_resource_usage_t *usage = zest_map_at(pass->inputs, swapchain_node->name);
					uses_swapchain = ZEST_TRUE;
					first_swapchain_usage_stage_in_this_batch = usage->stage_mask;
                } else if (zest_map_valid_name(pass->outputs, swapchain_node->name)) {
					zest_resource_usage_t *usage = zest_map_at(pass->outputs, swapchain_node->name);
					uses_swapchain = ZEST_TRUE;
					first_swapchain_usage_stage_in_this_batch = usage->stage_mask;
                }
                if (uses_swapchain) {
                    first_batch_to_wait = &wave_submission->batches[queue_index];
                    //wait_stage_for_acquire_semaphore = first_swapchain_usage_stage_in_this_batch;
                    // Ensure this stage is compatible with the batch's queue
                    if (!zest__is_stage_compatible_with_qfi(wait_stage_for_acquire_semaphore, context->device->queues[queue_index]->type)) {
                        ZEST_REPORT(context->device, zest_report_incompatible_stage_for_queue, "Swapchain usage stage %i is not compatible with queue family %u for wave submission %i",
								   wait_stage_for_acquire_semaphore,
								   first_batch_to_wait->queue_family_index, submission_index);
                        // Fallback or error. Forcing TOP_OF_PIPE might be safer if this happens,
                        // though it indicates a graph definition error.
                        wait_stage_for_acquire_semaphore = zest_pipeline_stage_top_of_pipe_bit;
                    }
                }
            }

            // 2. Assign the wait:
            if (first_batch_to_wait) {
                // The first batch that actually uses the swapchain will wait for it.
				zest_semaphore_reference_t semaphore_reference = { zest_dynamic_resource_image_available_semaphore, 0 };
                zest_vec_linear_push(allocator, first_batch_to_wait->wait_semaphores, semaphore_reference);
                zest_vec_linear_push(allocator, first_batch_to_wait->wait_dst_stage_masks, wait_stage_for_acquire_semaphore);
            } else {
                // Image was acquired, but no pass in the graph uses it.
                // The *very first submission batch of the graph* must wait on the semaphore to consume it.
                // The wait stage must be compatible with this first batch's queue.
                zest_wave_submission_t *actual_first_wave = &frame_graph->submissions[0];
                zest_pipeline_stage_flags compatible_dummy_wait_stage = zest_pipeline_stage_top_of_pipe_bit;
                zest_uint queue_family_index = ZEST_INVALID;
                zest_submission_batch_t *first_batch = NULL;

                // Make the dummy wait stage more specific if possible, but ensure compatibility
                for (int queue_index = 0; queue_index != ZEST_QUEUE_COUNT; ++queue_index) {
                    if (actual_first_wave->batches[queue_index].magic) {
                        first_batch = &actual_first_wave->batches[queue_index];
                        if (context->device->queues[queue_index]->type & zest_queue_transfer) {
                            compatible_dummy_wait_stage = zest_pipeline_stage_transfer_bit;
                            queue_family_index = actual_first_wave->batches[queue_index].queue_family_index;
                        } else if (context->device->queues[queue_index]->type & zest_queue_compute) {
                            compatible_dummy_wait_stage = zest_pipeline_stage_compute_shader_bit;
                            queue_family_index = actual_first_wave->batches[queue_index].queue_family_index;
                        }
                        break;
                    }
                }
                // Graphics queue can also do TOP_OF_PIPE or even COLOR_ATTACHMENT_OUTPUT_BIT if it's just for semaphore flow.

                if (first_batch) {
                    zest_semaphore_reference_t semaphore_reference = { zest_dynamic_resource_image_available_semaphore, 0 };
                    zest_vec_linear_push(allocator, first_batch->wait_semaphores, semaphore_reference);
                    zest_vec_linear_push(allocator, first_batch->wait_dst_stage_masks, compatible_dummy_wait_stage);
                    ZEST_REPORT(context->device, zest_report_unused_swapchain,"RenderGraph: Swapchain image acquired but not used by any pass. Graph invalidated because present would have no semaphore to wait on. First batch (on QFI %u) will wait on imageAvailableSemaphore at stage %i.",
							   queue_family_index,
							   compatible_dummy_wait_stage);
					frame_graph->error_status |= zest_fgs_critical_error;
                }
            }
        }

		// --- Handle renderFinishedSemaphore for the last batch ---
		if (last_wave_that_presented != ZEST_INVALID) {
			zest_wave_submission_t *last_wave = &frame_graph->submissions[last_wave_that_presented];
			if (last_wave->batches[ZEST_GRAPHICS_QUEUE_INDEX].magic &&
				last_wave->batches[ZEST_GRAPHICS_QUEUE_INDEX].outputs_to_swapchain == ZEST_TRUE) {  //Only if it's a graphics queue and it actually outputs to the swapchain
				// This assumes the last batch's *primary* signal is renderFinished.
				if (!last_wave->batches[ZEST_GRAPHICS_QUEUE_INDEX].signal_semaphores) {
					zest_semaphore_reference_t semaphore_reference = { zest_dynamic_resource_render_finished_semaphore, 0 };
					zest_vec_linear_push(allocator, last_wave->batches[ZEST_GRAPHICS_QUEUE_INDEX].signal_semaphores, semaphore_reference);
					zest_vec_linear_push(allocator, last_wave->batches[ZEST_GRAPHICS_QUEUE_INDEX].signal_dst_stage_masks, zest_pipeline_stage_bottom_of_pipe_bit);
				} else {
					//We should write a test for this scenario
					ZEST_REPORT(context->device, zest_report_last_batch_already_signalled, "Last batch already has an internal signal_semaphore. Logic to add external renderFinishedSemaphore needs p_signal_semaphores to be a list.");
				}
			}
		}
    }

    //Plan_transient_buffers
    zest_bucket_array_foreach(resource_index, frame_graph->resources) {
        zest_resource_node resource = zest_bucket_array_get(&frame_graph->resources, zest_resource_node_t, resource_index);

        // Check if this resource is transient AND actually used in the compiled graph
        if (ZEST__FLAGGED(resource->flags, zest_resource_node_flag_transient)) {
            if (resource->reference_count > 0 || ZEST__FLAGGED(resource->flags, zest_resource_node_flag_essential_output) &&
                resource->first_usage_pass_idx <= resource->last_usage_pass_idx && // Ensures it's used
                resource->first_usage_pass_idx != ZEST_INVALID) {

                zest_pass_group_t *first_pass = frame_graph->pass_execution_order[resource->first_usage_pass_idx];
                zest_pass_group_t *last_pass = frame_graph->pass_execution_order[resource->last_usage_pass_idx];

                zest_vec_linear_push(allocator, first_pass->transient_resources_to_create, resource);
                zest_vec_linear_push(allocator, last_pass->transient_resources_to_free, resource);
            } else {
                frame_graph->culled_resources_count++;
                ZEST_REPORT(context->device, zest_report_resource_culled, "Transient resource [%s] was culled because it's was not consumed by any other passes. If you intended to use this output outside of the frame graph once it's executed then you can call zest_FlagResourceAsEssential.", resource->name);
            }
        } 
        if (resource->buffer_provider || resource->image_provider) {
            zest_vec_linear_push(allocator, frame_graph->resources_to_update, resource);
        }
    }

    //Plan_resource_barriers
    zest_bucket_array_foreach(resource_index, frame_graph->resources) {
        zest_resource_node resource = zest_bucket_array_get(&frame_graph->resources, zest_resource_node_t, resource_index);
        if (resource->aliased_resource) continue;
        zest_resource_state_t *prev_state = NULL;
        int starting_state_index = 0;
        zest_vec_foreach(state_index, resource->journey) {
            if (resource->reference_count == 0 && ZEST__NOT_FLAGGED(resource->flags, zest_resource_node_flag_essential_output)) {
                continue;
            }
            zest_resource_state_t *current_state = &resource->journey[state_index];
            zest_pass_group_t *pass = &frame_graph->final_passes.data[current_state->pass_index];
            zest_resource_usage_t *current_usage = &current_state->usage;
            zest_resource_state_t *next_state = NULL;
            zest_execution_details_t *exe_details = &pass->execution_details;
            zest_execution_barriers_t *barriers = &exe_details->barriers;
            if (state_index + 1 < (int)zest_vec_size(resource->journey)) {
                next_state = &resource->journey[state_index + 1];
            }
            zest_pipeline_stage_flags required_stage = current_usage->stage_mask;
            if (resource->type & zest_resource_type_is_image) {
                zest__add_image_barriers(frame_graph, allocator, resource, barriers, current_state, prev_state, next_state);
                prev_state = current_state;
                if (current_state->usage.access_mask & zest_access_render_pass_bits) {
                    exe_details->requires_dynamic_render_pass = ZEST_TRUE;
                }
            } else if(resource->type & zest_resource_type_buffer) {
                if (!prev_state) {
                    //This is the first state of the resource
                    //If there's no previous state then we need to see if a barrier is needed to transition from the resource
                    //start state. We put this in the acquire barrier as it needs to be put in place before the pass is executed.
                    zest_uint src_queue_family_index = resource->current_queue_family_index;
                    zest_uint dst_queue_family_index = ZEST_QUEUE_FAMILY_IGNORED;
                    if (src_queue_family_index == ZEST_QUEUE_FAMILY_IGNORED) {
                        if ((resource->access_mask & zest_access_write_bits_general) &&
                            (current_usage->access_mask & zest_access_read_bits_general)) {
                            context->device->platform->add_frame_graph_buffer_barrier(resource, barriers, ZEST_TRUE,
																					  resource->access_mask, current_usage->access_mask,
																					  src_queue_family_index, dst_queue_family_index,
																					  resource->last_stage_mask, current_state->usage.stage_mask);
							#ifdef ZEST_DEBUGGING
                            zest__add_buffer_barrier(resource, barriers, ZEST_TRUE,
													 resource->access_mask, current_usage->access_mask,
													 src_queue_family_index, dst_queue_family_index,
													 resource->last_stage_mask, current_state->usage.stage_mask);
							#endif
                        }
                    } else {
                        //This resource already belongs to a queue which means that it's an imported resource
                        //If the frame graph is on the graphics queue only then there's no need to acquire from a prior release.

                        //It shouldn't be possible for transient resources to be considered for a barrier at this point. 
                        ZEST_ASSERT(ZEST__FLAGGED(resource->flags, zest_resource_node_flag_imported));
                        dst_queue_family_index = current_state->queue_family_index;
                        if (src_queue_family_index != ZEST_QUEUE_FAMILY_IGNORED) {
                            context->device->platform->add_frame_graph_buffer_barrier(resource, barriers, ZEST_TRUE,
																					  resource->access_mask, current_usage->access_mask,
																					  src_queue_family_index, dst_queue_family_index,
																					  resource->last_stage_mask, current_state->usage.stage_mask);
							#ifdef ZEST_DEBUGGING
                            zest__add_buffer_barrier(resource, barriers, ZEST_TRUE,
													 resource->access_mask, current_usage->access_mask,
													 src_queue_family_index, dst_queue_family_index,
													 resource->last_stage_mask, current_state->usage.stage_mask);
							#endif
                        }
                    }
                    zest_bool needs_releasing = ZEST_FALSE;
                } else {
                    //There is a previous state, do we need to acquire the resource from a different queue?
                    zest_uint src_queue_family_index = ZEST_QUEUE_FAMILY_IGNORED;
                    zest_uint dst_queue_family_index = ZEST_QUEUE_FAMILY_IGNORED;
                    zest_bool needs_acquiring = ZEST_FALSE;
                    if (prev_state && (prev_state->queue_family_index != current_state->queue_family_index || prev_state->was_released)) {
                        //The next state is in a different queue so the resource needs to be released to that queue
                        src_queue_family_index = prev_state->queue_family_index;
                        dst_queue_family_index = current_state->queue_family_index;
                        needs_acquiring = ZEST_TRUE;
                    }
                    if (needs_acquiring) {
                        zest_resource_usage_t *prev_usage = &prev_state->usage;
                        //Acquire the resource. No transitioning is done here, acquire only if needed
                        context->device->platform->add_frame_graph_buffer_barrier(resource, barriers, ZEST_TRUE,
																				  prev_usage->access_mask, current_usage->access_mask,
																				  src_queue_family_index, dst_queue_family_index,
																				  prev_state->usage.stage_mask, current_state->usage.stage_mask);
						#ifdef ZEST_DEBUGGING
                        zest__add_buffer_barrier(resource, barriers, ZEST_TRUE,
												 prev_usage->access_mask, current_usage->access_mask,
												 src_queue_family_index, dst_queue_family_index,
												 prev_state->usage.stage_mask, current_state->usage.stage_mask);
						#endif
                        zest_submission_batch_t *batch = zest__get_submission_batch(current_state->submission_id);
                        batch->queue_wait_stages |= prev_state->usage.stage_mask;
                    }
                }
                if (next_state) {
                    //If the resource has a state after this one then we can check if a barrier is needed to transition and/or
                    //release to another queue family
                    zest_uint src_queue_family_index = current_state->queue_family_index;
                    zest_uint dst_queue_family_index = next_state->queue_family_index;
                    zest_resource_usage_t *next_usage = &next_state->usage;
                    zest_bool needs_releasing = ZEST_FALSE;
                    if (next_state && next_state->queue_family_index != current_state->queue_family_index) {
                        //The next state is in a different queue so the resource needs to be released to that queue
                        src_queue_family_index = current_state->queue_family_index;
                        dst_queue_family_index = next_state->queue_family_index;
                        needs_releasing = ZEST_TRUE;
                    }
                    if (needs_releasing || 
                        (current_usage->access_mask & zest_access_write_bits_general) &&
                        (next_usage->access_mask & zest_access_read_bits_general)) {
                        zest_pipeline_stage_flags dst_stage = zest_pipeline_stage_bottom_of_pipe_bit;
                        if (needs_releasing) {
                            current_state->was_released = ZEST_TRUE;
                        } else {
                            dst_stage = next_state->usage.stage_mask;
                            current_state->was_released = ZEST_FALSE;
                        }
                        context->device->platform->add_frame_graph_buffer_barrier(resource, barriers, ZEST_FALSE,
																				  current_usage->access_mask, next_state->usage.access_mask,
																				  src_queue_family_index, dst_queue_family_index,
																				  current_state->usage.stage_mask, dst_stage);
						#ifdef ZEST_DEBUGGING
                        zest__add_buffer_barrier(resource, barriers, ZEST_FALSE,
												 current_usage->access_mask, next_state->usage.access_mask,
												 src_queue_family_index, dst_queue_family_index,
												 current_state->usage.stage_mask, dst_stage);
						#endif
                    }
                } else if (resource->flags & zest_resource_node_flag_release_after_use
                    && current_state->queue_family_index != context->device->transfer_queue_family_index) {
                    //Release the buffer so that it's ready to be acquired by any other queue in the next frame
                    //Release to the transfer queue by default (if it's not already on the transfer queue).
					context->device->platform->add_frame_graph_buffer_barrier(resource, barriers, ZEST_FALSE,
																			  current_usage->access_mask, zest_access_none,
																			  current_state->queue_family_index, ZEST_GRAPHICS_QUEUE_INDEX,
																			  current_state->usage.stage_mask, zest_pipeline_stage_bottom_of_pipe_bit);
					#ifdef ZEST_DEBUGGING
					zest__add_buffer_barrier(resource, barriers, ZEST_FALSE,
											 current_usage->access_mask, zest_access_none,
											 current_state->queue_family_index, ZEST_QUEUE_FAMILY_IGNORED,
											 current_state->usage.stage_mask, zest_pipeline_stage_bottom_of_pipe_bit);
					#endif
                }
                prev_state = current_state;
            }
 
        }
    }

    //Process_compiled_execution_order
	//Prepare_render_pass
    zest_vec_foreach(submission_index, frame_graph->submissions) {
        zest_wave_submission_t *submission = &frame_graph->submissions[submission_index];
        for (int queue_index = 0; queue_index != ZEST_QUEUE_COUNT; ++queue_index) {
            zest_vec_foreach(batch_index, submission->batches[queue_index].pass_indices) {
                int current_pass_index = submission->batches[queue_index].pass_indices[batch_index];
                zest_pass_group_t *pass = &frame_graph->final_passes.data[current_pass_index];
                zest_execution_details_t *exe_details = &pass->execution_details;

				zest__prepare_render_pass(pass, exe_details, current_pass_index);
            }   //Passes within batch loop
        }
        
    }   //Batch loop

    if (ZEST__FLAGGED(frame_graph->flags, zest_frame_graph_expecting_swap_chain_usage)) {
        if (ZEST__NOT_FLAGGED(frame_graph->flags, zest_frame_graph_present_after_execute)) {
            ZEST_REPORT(context->device, zest_report_expecting_swapchain_usage, "Swapchain usage was expected but the frame graph present flag was not set in frame graph [%s], indicating that a render pass could not be created. Check other reports.", frame_graph->name);
            ZEST__FLAG(frame_graph->flags, zest_frame_graph_present_after_execute);
        }
        //Error: the frame graph is trying to render to the screen but no swap chain image was used!
        //Make sure that you call zest_ConnectSwapChainOutput in your frame graph setup.
    }
	ZEST__FLAG(frame_graph->flags, zest_frame_graph_is_compiled);  

	//If no descriptor sets are set in the graph then just set the global bindless set by default
	if (!frame_graph->descriptor_sets) {
		zest_descriptor_set set = zest_GetBindlessSet(context->device);
		zest_SetDescriptorSets(context->device->pipeline_layout, &set, 1);
	}

	frame_graph->compile_time = zest_Microsecs() - start_time;
    return frame_graph;
}

void zest__prepare_render_pass(zest_pass_group_t *pass, zest_execution_details_t *exe_details, zest_uint current_pass_index) {
	if (exe_details->requires_dynamic_render_pass) {
		zest_context context = zest__frame_graph_builder->context;
		zloc_linear_allocator_t *allocator = &context->frame_graph_allocator[context->current_fif];
		zest_uint color_attachment_index = 0;
		//Determine attachments for color and depth (resolve can come later), first for outputs
		exe_details->depth_attachment.image_view = 0;
		exe_details->rendering_info.sample_count = zest_sample_count_1_bit;
		zest_map_foreach(o, pass->outputs) {
			zest_resource_usage_t *output_usage = &pass->outputs.data[o];
			zest_resource_node resource = pass->outputs.data[o].resource_node;
			if (resource->type & zest_resource_type_is_image) {
				if (resource->type != zest_resource_type_depth && ZEST__FLAGGED(pass->flags, zest_pass_flag_output_resolve) && resource->image.info.sample_count == 1) {
					output_usage->purpose = zest_purpose_color_attachment_resolve;
				}
				if (output_usage->purpose == zest_purpose_color_attachment_write) {
					zest_rendering_attachment_info_t color = ZEST__ZERO_INIT(zest_rendering_attachment_info_t);
					color.image_view = resource->aliased_resource ? &resource->aliased_resource->view : &resource->view;
					color.layout = output_usage->image_layout;
					color.load_op = output_usage->load_op;
					color.store_op = output_usage->store_op;
					color.clear_value = output_usage->clear_value;
					exe_details->rendering_info.color_attachment_formats[color_attachment_index] = resource->image.info.format;
					if (color_attachment_index == 0) {
						exe_details->render_area.offset.x = 0;
						exe_details->render_area.offset.y = 0;
						exe_details->render_area.extent.width = resource->image.info.extent.width;
						exe_details->render_area.extent.height = resource->image.info.extent.height;
					}
					color_attachment_index++;
					exe_details->rendering_info.color_attachment_count++;
					if (resource->image.info.layer_count > 1) {
						zest_uint view_mask = (1u << resource->image.info.layer_count) - 1;
						zest_uint current_mask = exe_details->rendering_info.view_mask;
						ZEST_ASSERT(current_mask == 0 || current_mask == view_mask, "If using multiviews, all output images/depth attachments must have the same number of layers.");
						exe_details->rendering_info.view_mask = view_mask;
					}
					//zest_image_layout final_layout = zest__determine_final_layout(zest__determine_final_layout(current_pass_index, resource, output_usage));
					if (resource->type == zest_resource_type_swap_chain_image) {
						color.is_swapchain_image = ZEST_TRUE;
						context->device->platform->add_frame_graph_image_barrier(
							resource, &exe_details->barriers, ZEST_FALSE,
							output_usage->access_mask, zest_access_none,
							output_usage->image_layout, zest_image_layout_present,
							resource->current_queue_family_index, resource->current_queue_family_index,
							output_usage->stage_mask, zest_pipeline_stage_bottom_of_pipe_bit
						);
						ZEST__FLAG(zest__frame_graph_builder->frame_graph->flags, zest_frame_graph_present_after_execute);
					}
					zest_vec_linear_push(allocator, exe_details->color_attachments, color);
				} else if (output_usage->purpose == zest_purpose_color_attachment_resolve) {
					exe_details->color_attachments[color_attachment_index].resolve_layout = output_usage->image_layout;
					exe_details->color_attachments[color_attachment_index].resolve_image_view = &resource->view;
					exe_details->rendering_info.sample_count = context->device->platform->get_msaa_sample_count(context);
				} else if (output_usage->purpose == zest_purpose_depth_stencil_attachment_write) {
					zest_rendering_attachment_info_t *depth = &exe_details->depth_attachment;
					*depth = ZEST__ZERO_INIT(zest_rendering_attachment_info_t);
					depth->image_view = resource->aliased_resource ? &resource->aliased_resource->view : &resource->view;
					depth->layout = output_usage->image_layout;
					depth->load_op = output_usage->load_op;
					depth->store_op = output_usage->store_op;
					depth->clear_value = output_usage->clear_value;
					if (color_attachment_index == 0) {
						exe_details->render_area.offset.x = 0;
						exe_details->render_area.offset.y = 0;
						exe_details->render_area.extent.width = resource->image.info.extent.width;
						exe_details->render_area.extent.height = resource->image.info.extent.height;
					}
					exe_details->rendering_info.depth_attachment_format = resource->image.info.format;
					if (resource->image.info.layer_count > 1) {
						zest_uint view_mask = (1u << resource->image.info.layer_count) - 1;
						zest_uint current_mask = exe_details->rendering_info.view_mask;
						ZEST_ASSERT(current_mask == 0 || current_mask == view_mask, "If using multiviews, all output images/depth attachments must have the same number of layers.");
						exe_details->rendering_info.view_mask = (1u << resource->image.info.layer_count) - 1;
					}
					ZEST__FLAG(resource->flags, zest_resource_node_flag_used_in_output);
				}
			}
		}
	}
}

zest_frame_graph zest_EndFrameGraph() {
    zest_frame_graph frame_graph = zest__compile_frame_graph();
	zest_context context = zest__frame_graph_builder->context;

    ZEST_MAYBE_REPORT(context->device, zest__frame_graph_builder->current_pass, zest_report_missing_end_pass, "Warning in frame graph [%s]: The current pass in the frame graph context is not null. This means that a call to zest_EndPass is missing in the frame graph setup.", frame_graph->name);
    zest__frame_graph_builder->current_pass = 0;

    if (frame_graph->error_status != zest_fgs_critical_error) {
        zest__cache_frame_graph(frame_graph);
    } else {
        ZEST__UNFLAG(context->flags, zest_context_flag_work_was_submitted);
    }

    zest__frame_graph_builder = NULL;
	ZEST__UNFLAG(context->flags, zest_context_flag_building_frame_graph);  

    return frame_graph;
}

zest_semaphore_status zest_WaitForSignal(zest_execution_timeline timeline, zest_microsecs timeout) {
	return timeline->device->platform->wait_for_timeline(timeline, timeout);
}

zest_frame_graph zest_EndFrameGraphAndExecute() {
    zest_frame_graph frame_graph = zest__compile_frame_graph();

	zest_context context = zest__frame_graph_builder->context;

    if (frame_graph->error_status != zest_fgs_critical_error) {
        zest__cache_frame_graph(frame_graph);
        zest__execute_frame_graph(context, frame_graph, ZEST_TRUE);
    } else {
        ZEST__UNFLAG(context->flags, zest_context_flag_work_was_submitted);
    }

    zest__frame_graph_builder = NULL;
	ZEST__UNFLAG(context->flags, zest_context_flag_building_frame_graph);  

    return frame_graph;
}

zest_image_aspect_flags zest__determine_aspect_flag(zest_format format) {
    switch (format) {
        // Depth-Only Formats
		case zest_format_d16_unorm:
		case zest_format_x8_d24_unorm_pack32: // D24_UNORM component, X8 is undefined
		case zest_format_d32_sfloat:
			return zest_image_aspect_depth_bit;
			// Stencil-Only Formats
		case zest_format_s8_uint:
			return zest_image_aspect_stencil_bit;
			// Combined Depth/Stencil Formats
		case zest_format_d16_unorm_s8_uint:
		case zest_format_d24_unorm_s8_uint:
		case zest_format_d32_sfloat_s8_uint:
			return zest_image_aspect_depth_bit | zest_image_aspect_stencil_bit;
		default:
			return zest_image_aspect_color_bit;
    }
}

zest_image_aspect_flags zest__determine_aspect_flag_for_view(zest_format format) {
	/*	Views must be either depth or stencil they can't be both hence the separate function. At some point
		this needs updating to take a usage hint or something, for now though depth is returned by default.	
*/
    switch (format) {
        // Depth-Only Formats
		case zest_format_d16_unorm:
		case zest_format_x8_d24_unorm_pack32: // D24_UNORM component, X8 is undefined
		case zest_format_d32_sfloat:
			return zest_image_aspect_depth_bit;
			// Stencil-Only Formats
		case zest_format_s8_uint:
			return zest_image_aspect_stencil_bit;
			// Combined Depth/Stencil Formats
		case zest_format_d16_unorm_s8_uint:
		case zest_format_d24_unorm_s8_uint:
		case zest_format_d32_sfloat_s8_uint:
			return zest_image_aspect_depth_bit;
		default:
			return zest_image_aspect_color_bit;
    }
}

zest_bool zest__is_depth_stencil_format(zest_format format) {
	return format >= zest_format_d16_unorm && format <= zest_format_d32_sfloat_s8_uint;
}

zest_bool zest__is_compressed_format(zest_format format) {
	return format >= zest_format_bc1_rgb_unorm_block && format <= zest_format_astc_12X12_srgb_block;
}

void zest__interpret_hints(zest_resource_node resource, zest_resource_usage_hint usage_hints) {
    if (usage_hints & zest_resource_usage_hint_copy_dst) {
        resource->image.info.flags |= zest_image_flag_transfer_dst;
    }
    if (usage_hints & zest_resource_usage_hint_copy_src) {
        resource->image.info.flags |= zest_image_flag_transfer_src;
    }
    if (usage_hints & zest_resource_usage_hint_cpu_transfer) {
        resource->image.info.flags |= zest_image_flag_host_visible | zest_image_flag_host_coherent;
        resource->image.info.flags |= zest_image_flag_transfer_src | zest_image_flag_transfer_dst;
    }
}

zest_image_layout zest__determine_final_layout(zest_uint pass_index, zest_resource_node node, zest_resource_usage_t *current_usage) {
    zest_frame_graph frame_graph = zest__frame_graph_builder->frame_graph;
    ZEST_ASSERT_HANDLE(frame_graph);        //Not a valid frame graph! Make sure you called BeginRenderGraph or BeginRenderToScreen
    zest_vec_foreach(state_index, node->journey) {
        zest_resource_state_t *state = &node->journey[state_index];
        if (state->pass_index == pass_index && (zest_uint)state_index < zest_vec_size(node->journey) - 1) {
            return node->journey[state_index + 1].usage.image_layout;
        }
    }
    if (node->type == zest_resource_type_swap_chain_image) {
        return zest_image_layout_present;
    } else if (ZEST__FLAGGED(node->flags, zest_resource_node_flag_imported)) {
        return node->image_layout; 
    } 
    if (current_usage->store_op == zest_store_op_dont_care) {
        return zest_image_layout_undefined;
    }
    return zest_image_layout_shader_read_only_optimal;
}

void zest__deferr_resource_destruction(zest_context context, void* handle) {
    zest_vec_push(context->device->allocator, context->device->deferred_resource_freeing_list.resources[context->current_fif], handle);
}

void zest__deferr_image_destruction(zest_context context, zest_image image) {
    zest_vec_push(context->allocator, context->deferred_resource_freeing_list.transient_images[context->current_fif], *image);
}

void zest__deferr_view_array_destruction(zest_context context, zest_image_view_array view_array) {
    zest_vec_push(context->allocator, context->deferred_resource_freeing_list.transient_view_arrays[context->current_fif], *view_array);
}

zest_bool zest__execute_frame_graph(zest_context context, zest_frame_graph frame_graph, zest_bool is_intraframe) {
    ZEST_ASSERT_HANDLE(frame_graph);        //Not a valid frame graph! Make sure you called BeginRenderGraph or BeginRenderToScreen
	zest_microsecs start_time = zest_Microsecs();
    zloc_linear_allocator_t *allocator = &context->frame_graph_allocator[context->current_fif];
    zest_map_queue_value queues = ZEST__ZERO_INIT(zest_map_queue_value);

    zest_execution_backend backend = (zest_execution_backend)context->device->platform->new_execution_backend(allocator);

	zest_vec_foreach(resource_index, frame_graph->resources_to_update) {
		zest_resource_node resource = frame_graph->resources_to_update[resource_index];
		if (resource->buffer_provider) {
			resource->storage_buffer = resource->buffer_provider(context, resource);
		} else if (resource->image_provider) {
            resource->view = resource->image_provider(context, resource);
		}
	}

    zest_vec_foreach(submission_index, frame_graph->submissions) {
        zest_wave_submission_t *wave_submission = &frame_graph->submissions[submission_index];

        for (zest_uint queue_index = 0; queue_index != ZEST_QUEUE_COUNT; ++queue_index) {
            zest_submission_batch_t *batch = &wave_submission->batches[queue_index];
            if (!batch->magic) continue;
            ZEST_ASSERT(zest_vec_size(batch->pass_indices));    //A batch was created without any pass indices. Bug in the Compile stage!

            zest_pipeline_stage_flags timeline_wait_stage;

            // 1. acquire an appropriate command buffer
            switch (batch->queue_type) {
				case zest_queue_graphics:
					ZEST_CLEANUP_ON_FALSE(context->device->platform->set_next_command_buffer(&frame_graph->command_list, batch->queue));
					timeline_wait_stage = zest_pipeline_stage_vertex_input_bit;
					break;
				case zest_queue_compute:
					ZEST_CLEANUP_ON_FALSE(context->device->platform->set_next_command_buffer(&frame_graph->command_list, batch->queue));
					timeline_wait_stage = zest_pipeline_stage_compute_shader_bit;
					break;
				case zest_queue_transfer:
					ZEST_CLEANUP_ON_FALSE(context->device->platform->set_next_command_buffer(&frame_graph->command_list, batch->queue));
					timeline_wait_stage = zest_pipeline_stage_transfer_bit;
					break;
				default:
					ZEST_ASSERT(0); //Unknown queue type for batch. Corrupt memory perhaps?!
            }

            ZEST_CLEANUP_ON_FALSE(context->device->platform->begin_command_buffer(&frame_graph->command_list));

			// Bind the global bindless descriptor set for graphics and compute queues
			if (batch->queue_type != zest_queue_transfer && frame_graph->descriptor_sets) {
				if (batch->queue_type == zest_queue_graphics) {
					// Graphics queue can do both graphics and compute
					zest_cmd_BindDescriptorSets(&frame_graph->command_list, zest_bind_point_graphics, frame_graph->pipeline_layout, frame_graph->descriptor_sets, zest_vec_size(frame_graph->descriptor_sets), 0);
					zest_cmd_BindDescriptorSets(&frame_graph->command_list, zest_bind_point_compute, frame_graph->pipeline_layout, frame_graph->descriptor_sets, zest_vec_size(frame_graph->descriptor_sets), 0);
				} else {
					// Compute queue is compute-only
					zest_cmd_BindDescriptorSets(&frame_graph->command_list, zest_bind_point_compute, frame_graph->pipeline_layout, frame_graph->descriptor_sets, zest_vec_size(frame_graph->descriptor_sets), 0);
				}
			}

            zest_vec_foreach(i, batch->pass_indices) {
                zest_uint pass_index = batch->pass_indices[i];
                zest_pass_group_t *grouped_pass = &frame_graph->final_passes.data[pass_index];
                zest_execution_details_t *exe_details = &grouped_pass->execution_details;

                frame_graph->command_list.submission_index = submission_index;
                frame_graph->command_list.timeline_wait_stage = timeline_wait_stage;
                frame_graph->command_list.queue_index = queue_index;

				if (grouped_pass->flags & zest_pass_flag_sync_only) {
					break;
				}

                //Create any transient resources where they're first used in this grouped_pass
                zest_vec_foreach(r, grouped_pass->transient_resources_to_create) {
                    zest_resource_node resource = grouped_pass->transient_resources_to_create[r];
                    if (!zest__create_transient_resource(context, resource)) {
                        frame_graph->error_status |= zest_fgs_transient_resource_failure;
                        return ZEST_FALSE;
                    }
					int d = 0;
                }

                //Batch execute acquire barriers for images and buffers
				context->device->platform->acquire_barrier(&frame_graph->command_list, exe_details);

                zest_bool has_render_pass = exe_details->requires_dynamic_render_pass;

                //Begin the render pass if the pass has one
                if (has_render_pass) {
                    context->device->platform->begin_render_pass(&frame_graph->command_list, exe_details);
					frame_graph->command_list.rendering_info = exe_details->rendering_info;
					frame_graph->command_list.began_rendering = ZEST_TRUE;
                }

                //Execute the callbacks in the pass
                zest_vec_foreach(pass_index, grouped_pass->passes) {
                    zest_pass_node pass = grouped_pass->passes[pass_index];

                    if (pass->type == zest_pass_type_graphics && !frame_graph->command_list.began_rendering) {
                        ZEST_REPORT(context->device, zest_report_render_pass_skipped, "Pass execution was skipped for pass [%s] becuase rendering did not start. Check for validation errors.", pass->name);
                        continue;
                    }
                    frame_graph->command_list.pass_node = pass;
                    frame_graph->command_list.frame_graph = frame_graph;
                    pass->execution_callback.callback(&frame_graph->command_list, pass->execution_callback.user_data);
                }

                zest_map_foreach(pass_input_index, grouped_pass->inputs) {
                    zest_resource_node resource = grouped_pass->inputs.data[pass_input_index].resource_node;
                    if (resource->aliased_resource) {
                        resource->aliased_resource->current_state_index++;
                    } else {
                        resource->current_state_index++;
                    }
                }

                zest_map_foreach(pass_output_index, grouped_pass->outputs) {
                    zest_resource_node resource = grouped_pass->outputs.data[pass_output_index].resource_node;
                    if (resource->aliased_resource) {
                        resource->aliased_resource->current_state_index++;
                    } else {
                        resource->current_state_index++;
                    }
                }

                if (has_render_pass) {
                    context->device->platform->end_render_pass(&frame_graph->command_list);
					frame_graph->command_list.began_rendering = ZEST_FALSE;
                }

                //Batch execute release barriers for images and buffers

				context->device->platform->release_barrier(&frame_graph->command_list, exe_details);

                zest_vec_foreach(r, grouped_pass->transient_resources_to_free) {
                    zest_resource_node resource = grouped_pass->transient_resources_to_free[r];
                    zest__free_transient_resource(resource);
                }
                //End pass
            }
			context->device->platform->end_command_buffer(&frame_graph->command_list);
            context->device->platform->submit_frame_graph_batch(frame_graph, backend, batch, &queues);
        }   //Batch

        //For each batch in the last wave add the queue semaphores that were used so that the next batch submissions can wait on them
        context->device->platform->carry_over_semaphores(frame_graph, wave_submission, backend);
    }   //Wave 

    zest_map_foreach(i, queues) {
        zest_context_queue queue = queues.data[i];
        queue->fif = (queue->fif + 1) % ZEST_MAX_FIF;
    }

	zest_bucket_array_foreach(index, frame_graph->resources) {
		zest_resource_node resource = zest_bucket_array_get(&frame_graph->resources, zest_resource_node_t, index);
        //Reset the resource state indexes (the point in their graph journey). This is necessary for cached
        //frame graphs.
        if (resource->aliased_resource) {
            resource->aliased_resource->current_state_index = 0;
        } else {
            resource->current_state_index = 0;
        }
	}

	zest_vec_foreach(i, frame_graph->deferred_image_destruction) {
		zest_resource_node resource = frame_graph->deferred_image_destruction[i];
		zest__deferr_image_destruction(context, &resource->image);
		if (resource->view_array) zest__deferr_view_array_destruction(context, resource->view_array);
		resource->view_array = 0;
		resource->view = 0;
		memset(resource->mip_level_bindless_indexes, 0, sizeof(zest_uint*) * zest_max_global_binding_number);
	}
	frame_graph->deferred_image_destruction = 0;

    ZEST__FLAG(frame_graph->flags, zest_frame_graph_is_executed);

	frame_graph->execute_time = zest_Microsecs() - start_time;
    return ZEST_TRUE;

	cleanup:
		return ZEST_FALSE;
}

zest_context zest_GetContext(zest_command_list command_list) {
	ZEST_ASSERT_HANDLE(command_list);	//Not a valid command list
	return command_list->context;
}

zest_key zest_GetPassOutputKey(zest_pass_node pass) {
    ZEST_ASSERT_HANDLE(pass);   //Not a valid pass node handle
    return pass->output_key;
}

zest_bool zest_RenderGraphWasExecuted(zest_frame_graph frame_graph) {
    return ZEST__FLAGGED(frame_graph->flags, zest_frame_graph_is_executed);
}

void zest_PrintCachedFrameGraph(zest_context context, zest_frame_graph_cache_key_t *cache_key) {
    zest_frame_graph frame_graph = zest_GetCachedFrameGraph(context, cache_key);
    if (frame_graph) {
        zest_PrintCompiledFrameGraph(frame_graph);
    }
}

zest_frame_graph_result zest_GetFrameGraphResult(zest_frame_graph frame_graph) {
    ZEST_ASSERT_HANDLE(frame_graph);        //Not a valid frame graph! Make sure you called BeginRenderGraph or BeginRenderToScreen
    return frame_graph->error_status;
}

zest_uint zest_GetFrameGraphCulledResourceCount(zest_frame_graph frame_graph) {
    ZEST_ASSERT_HANDLE(frame_graph);        //Not a valid frame graph! Make sure you called BeginRenderGraph or BeginRenderToScreen
    return frame_graph->culled_resources_count;
}

zest_uint zest_GetFrameGraphFinalPassCount(zest_frame_graph frame_graph) {
    ZEST_ASSERT_HANDLE(frame_graph);        //Not a valid frame graph! Make sure you called BeginRenderGraph or BeginRenderToScreen
    return zest_map_size(frame_graph->final_passes);
}

zest_uint zest_GetFrameGraphPassTransientCreateCount(zest_frame_graph frame_graph, zest_key output_key) {
    ZEST_ASSERT_HANDLE(frame_graph);        //Not a valid frame graph! Make sure you called BeginRenderGraph or BeginRenderToScreen
    if (zest_map_valid_key(frame_graph->final_passes, output_key)) {
		zest_pass_group_t *final_pass = zest_map_at_key(frame_graph->final_passes, output_key);
        return zest_vec_size(final_pass->transient_resources_to_create);
    }
    return 0;
}

zest_uint zest_GetFrameGraphPassTransientFreeCount(zest_frame_graph frame_graph, zest_key output_key) {
    ZEST_ASSERT_HANDLE(frame_graph);        //Not a valid frame graph! Make sure you called BeginRenderGraph or BeginRenderToScreen
    if (zest_map_valid_key(frame_graph->final_passes, output_key)) {
		zest_pass_group_t *final_pass = zest_map_at_key(frame_graph->final_passes, output_key);
        return zest_vec_size(final_pass->transient_resources_to_free);
    }
    return 0;
}

zest_uint zest_GetFrameGraphCulledPassesCount(zest_frame_graph frame_graph) {
    ZEST_ASSERT_HANDLE(frame_graph);        //Not a valid frame graph! Make sure you called BeginRenderGraph or BeginRenderToScreen
    return frame_graph->culled_passes_count;
}

zest_uint zest_GetFrameGraphSubmissionCount(zest_frame_graph frame_graph) {
    ZEST_ASSERT_HANDLE(frame_graph);        //Not a valid frame graph! Make sure you called BeginRenderGraph or BeginRenderToScreen
    return zest_vec_size(frame_graph->submissions);
}

zest_uint zest_GetFrameGraphSubmissionBatchCount(zest_frame_graph frame_graph, zest_uint submission_index) {
    ZEST_ASSERT_HANDLE(frame_graph);        //Not a valid frame graph! Make sure you called BeginRenderGraph or BeginRenderToScreen
    if (submission_index < zest_vec_size(frame_graph->submissions)) {
        return zest_vec_size(frame_graph->submissions[submission_index].batches);
    }
    return 0;
}

zest_uint zest_GetSubmissionBatchPassCount(const zest_submission_batch_t *batch) {
    return zest_vec_size(batch->pass_indices);
}

const zest_submission_batch_t *zest_GetFrameGraphSubmissionBatch(zest_frame_graph frame_graph, zest_uint submission_index, zest_uint batch_index) {
    ZEST_ASSERT_HANDLE(frame_graph);        //Not a valid frame graph! Make sure you called BeginRenderGraph or BeginRenderToScreen
    if (submission_index < zest_vec_size(frame_graph->submissions)) {
		zest_wave_submission_t *submission = &frame_graph->submissions[submission_index];
        if (batch_index < zest_vec_size(submission->batches)) {
            return &submission->batches[batch_index];
        }
    }
    return NULL;
}

const zest_pass_group_t *zest_GetFrameGraphFinalPass(zest_frame_graph frame_graph, zest_uint pass_index) {
    ZEST_ASSERT_HANDLE(frame_graph);        //Not a valid frame graph! Make sure you called BeginRenderGraph or BeginRenderToScreen
    if (pass_index < zest_map_size(frame_graph->final_passes)) {
        return &frame_graph->final_passes.data[pass_index];
    }
    return NULL;
}

zest_size zest_GetFrameGraphCachedSize(zest_frame_graph frame_graph) {
    ZEST_ASSERT_HANDLE(frame_graph);        //Not a valid frame graph! Make sure you called BeginRenderGraph or BeginRenderToScreen
	return frame_graph->cached_size;
}

void zest_PrintCompiledFrameGraph(zest_frame_graph frame_graph) {
#ifdef ZEST_DEBUGGING
    if (!ZEST_VALID_HANDLE(frame_graph, zest_struct_type_frame_graph)) {
        ZEST_PRINT("frame graph handle is NULL.");
        return;
    }

	zest_context context = frame_graph->command_list.context;

	ZEST_PRINT("Swapchain Info");
	ZEST_PRINT("Image Available Wait Semaphores:");
	zest_ForEachFrameInFlight(fif) {
		void *wait_semaphore = context->device->platform->get_swapchain_wait_semaphore(context->swapchain, fif);
		ZEST_PRINT("%p %s", wait_semaphore, fif == context->current_fif ? "*" : "");
	}
	ZEST_PRINT("");
	ZEST_PRINT("Render Finished Signal Semaphores:");
	for (int i = 0; i != context->swapchain->image_count; i++) {
		void *signal_semaphore = context->device->platform->get_swapchain_signal_semaphore(context->swapchain, i);
		ZEST_PRINT("%p %s", signal_semaphore, i == context->swapchain->current_image_frame ? "*" : "");
	}

	ZEST_PRINT("");

    ZEST_PRINT("--- Frame graph Execution Plan, Current FIF: %i ---", context->current_fif);

    if (ZEST__NOT_FLAGGED(frame_graph->flags, zest_frame_graph_is_compiled)) {
        ZEST_PRINT("frame graph is not in a compiled state");
        return;
    }

    ZEST_PRINT("Resource List: Total Resources: %u\n", zest_bucket_array_size(&frame_graph->resources));

    zest_bucket_array_foreach(resource_index, frame_graph->resources) {
		zest_resource_node resource = zest_bucket_array_get(&frame_graph->resources, zest_resource_node_t, resource_index);
		void *resource_ptr = context->device->platform->get_resource_ptr(resource);
        if (resource->type & zest_resource_type_buffer) {
			ZEST_PRINT("Buffer: %s (%p) - Size: %llu, Offset: %llu", resource->name, resource_ptr, resource->buffer_desc.size, ZEST__FLAGGED(resource->flags, zest_resource_node_flag_imported) ? resource->storage_buffer->memory_offset : 0);
        } else if (resource->type & zest_resource_type_image) {
            ZEST_PRINT("Image: %s (%p) - Size: %u x %u", 
					   resource->name, resource_ptr, resource->image.info.extent.width, resource->image.info.extent.height);
        } else if (resource->type == zest_resource_type_swap_chain_image) {
            ZEST_PRINT("Swapchain Image: %s (%p)", 
					   resource->name, resource_ptr);
        }
    }

    ZEST_PRINT("");
	ZEST_PRINT("Graph Wave Layout ([G]raphics, [C]ompute, [T]ransfer)");
	ZEST_PRINT("Wave Index\tG\tC\tT\tPass Count");
	zest_vec_foreach(wave_index, frame_graph->execution_waves) {
		zest_execution_wave_t *wave = &frame_graph->execution_waves[wave_index];
		char g[2] = { (wave->queue_bits & zest_queue_graphics) > 0 ? 'X' : ' ', '\0' };
		char c[2] = { (wave->queue_bits & zest_queue_compute) > 0 ? 'X' : ' ', '\0' };
		char t[2] = { (wave->queue_bits & zest_queue_transfer) > 0 ? 'X' : ' ', '\0' };
		zest_uint pass_count = zest_vec_size(wave->pass_indices);
		ZEST_PRINT("%u\t\t%s\t%s\t%s\t%u", wave_index, g, c, t, pass_count);
	}


    ZEST_PRINT("");
    ZEST_PRINT("Number of Submission Batches: %u\n", zest_vec_size(frame_graph->submissions));

    zest_vec_foreach(submission_index, frame_graph->submissions) {
        zest_wave_submission_t *wave_submission = &frame_graph->submissions[submission_index];
		ZEST_PRINT("Wave Submission Index %i:", submission_index);
        for (zest_uint queue_index = 0; queue_index != ZEST_QUEUE_COUNT; ++queue_index) {
            zest_submission_batch_t *batch = &wave_submission->batches[queue_index];
            if (!batch->magic) continue;
            if (zest_map_valid_key(context->device->queue_names, batch->queue_family_index)) {
                ZEST_PRINT("  Target Queue Family: %s - index: %u", *zest_map_at_key(context->device->queue_names, batch->queue_family_index), batch->queue_family_index);
            } else {
                ZEST_PRINT("  Target Queue Family: Ignored - index: %u", batch->queue_family_index);
            }

            // --- Print Wait Semaphores for the Batch ---
            // For simplicity, assuming single wait_on_batch_semaphore for now, and you'd identify if it's external
            if (batch->wait_values) {
                // This stage should ideally be stored with the batch submission info by EndRenderGraph
                ZEST_PRINT("  Waits on the following Semaphores:");
                zest_vec_foreach(semaphore_index, batch->wait_values) {
                    zest_text_t pipeline_stages = zest__pipeline_stage_flags_to_string(context, batch->wait_stages[semaphore_index]);
                    if (zest_vec_size(batch->wait_values) && batch->wait_values[semaphore_index] > 0) {
                        ZEST_PRINT("     Timeline Semaphore: %p, Value: %zu, Stages: %s", context->device->platform->get_final_wait_ptr(batch, semaphore_index), batch->wait_values[semaphore_index], pipeline_stages.str);
                    } else {
                        ZEST_PRINT("     Binary Semaphore:   %p, Stages: %s", context->device->platform->get_final_wait_ptr(batch, semaphore_index), pipeline_stages.str);
                    }
                    zest_FreeText(context->device->allocator, &pipeline_stages);
                }
            } else {
                ZEST_PRINT("  Does not wait on any semaphores.");
            }

            ZEST_PRINT("  Passes in this batch:");
            zest_vec_foreach(batch_pass_index, batch->pass_indices) {
                int pass_index = batch->pass_indices[batch_pass_index];
                zest_pass_group_t *pass_node = &frame_graph->final_passes.data[pass_index];
                zest_execution_details_t *exe_details = &pass_node->execution_details;

                ZEST_PRINT("    Pass [%d] (QueueType: %d)",
						   pass_index, pass_node->compiled_queue_info.queue_type);
                zest_vec_foreach(pass_index, pass_node->passes) {
                    ZEST_PRINT("       %s", pass_node->passes[pass_index]->name);
                }

                if (zest_vec_size(exe_details->barriers.acquire_buffer_barriers) > 0 ||
                    zest_vec_size(exe_details->barriers.acquire_image_barriers) > 0) {

					if (zest_vec_size(exe_details->barriers.acquire_image_barriers)) {
						ZEST_PRINT("        Acquire Images:");
						zest_vec_foreach(barrier_index, exe_details->barriers.acquire_image_barriers) {
							zest_image_barrier_t *imb = &exe_details->barriers.acquire_image_barriers[barrier_index];
							zest_resource_node image_resource = exe_details->barriers.acquire_image_barrier_nodes[barrier_index];
							zest_text_t src_access_mask = zest__access_flags_to_string(context, imb->src_access_mask);
							zest_text_t dst_access_mask = zest__access_flags_to_string(context, imb->dst_access_mask);
							zest_text_t src_pipeline_stage = zest__pipeline_stage_flags_to_string(context, imb->src_stage);
							zest_text_t dst_pipeline_stage = zest__pipeline_stage_flags_to_string(context, imb->dst_stage);
							void *resource_ptr = context->device->platform->get_resource_ptr(image_resource);
							ZEST_PRINT("            %s (%p), Layout: %s -> %s, \n"
									   "            Access: %s -> %s, \n"
									   "            Pipeline Stage: %s -> %s, \n"
									   "            QFI: %u -> %u",
									   image_resource->name, resource_ptr,
									   zest__image_layout_to_string(imb->old_layout), zest__image_layout_to_string(imb->new_layout),
									   src_access_mask.str, dst_access_mask.str,
									   src_pipeline_stage.str, dst_pipeline_stage.str,
									   imb->src_queue_family_index, imb->dst_queue_family_index);
							zest_FreeText(context->device->allocator, &src_access_mask);
							zest_FreeText(context->device->allocator, &dst_access_mask);
							zest_FreeText(context->device->allocator, &src_pipeline_stage);
							zest_FreeText(context->device->allocator, &dst_pipeline_stage);
						}
					}

					if (zest_vec_size(exe_details->barriers.acquire_buffer_barriers)) {
						ZEST_PRINT("        Acquire Buffers:");
						zest_vec_foreach(barrier_index, exe_details->barriers.acquire_buffer_barriers) {
							zest_buffer_barrier_t *bmb = &exe_details->barriers.acquire_buffer_barriers[barrier_index];
							zest_resource_node buffer_resource = exe_details->barriers.acquire_buffer_barrier_nodes[barrier_index];
							// You need a robust way to get resource_name from bmb->image
							zest_text_t src_access_mask = zest__access_flags_to_string(context, bmb->src_access_mask);
							zest_text_t dst_access_mask = zest__access_flags_to_string(context, bmb->dst_access_mask);
							zest_text_t src_pipeline_stage = zest__pipeline_stage_flags_to_string(context, bmb->src_stage);
							zest_text_t dst_pipeline_stage = zest__pipeline_stage_flags_to_string(context, bmb->dst_stage);
							void *resource_ptr = context->device->platform->get_resource_ptr(buffer_resource);
							ZEST_PRINT("            %s (%p) | \n"
									   "            Access: %s -> %s, \n"
									   "            Pipeline Stage: %s -> %s, \n"
									   "            QFI: %u -> %u, Size: %llu",
									   buffer_resource->name, resource_ptr,
									   src_access_mask.str, dst_access_mask.str,
									   src_pipeline_stage.str, dst_pipeline_stage.str,
									   bmb->src_queue_family_index, bmb->dst_queue_family_index,
									   buffer_resource->buffer_desc.size);
							zest_FreeText(context->device->allocator, &src_access_mask);
							zest_FreeText(context->device->allocator, &dst_access_mask);
							zest_FreeText(context->device->allocator, &src_pipeline_stage);
							zest_FreeText(context->device->allocator, &dst_pipeline_stage);
						}
					}
                }

                // Print Inputs and Outputs (simplified)
                // ...

                if (zest_vec_size(exe_details->color_attachments)) {
                    ZEST_PRINT("      RenderArea: (%d,%d)-(%ux%u)",
							   exe_details->render_area.offset.x, exe_details->render_area.offset.y,
							   exe_details->render_area.extent.width, exe_details->render_area.extent.height);
                    // Further detail: iterate VkRenderPassCreateInfo's attachments (if stored or re-derived)
                    // and print each VkAttachmentDescription's load/store/layouts and clear values.
                }

                if (zest_vec_size(exe_details->barriers.release_buffer_barriers) > 0 ||
                    zest_vec_size(exe_details->barriers.release_image_barriers) > 0) {

					if (zest_vec_size(exe_details->barriers.release_image_barriers)) {
						ZEST_PRINT("        Release Images:");
						zest_vec_foreach(barrier_index, exe_details->barriers.release_image_barriers) {
							zest_image_barrier_t *imb = &exe_details->barriers.release_image_barriers[barrier_index];
							zest_resource_node image_resource = exe_details->barriers.release_image_barrier_nodes[barrier_index];
							zest_text_t src_access_mask = zest__access_flags_to_string(context, imb->src_access_mask);
							zest_text_t dst_access_mask = zest__access_flags_to_string(context, imb->dst_access_mask);
							zest_text_t src_pipeline_stage = zest__pipeline_stage_flags_to_string(context, imb->src_stage);
							zest_text_t dst_pipeline_stage = zest__pipeline_stage_flags_to_string(context, imb->dst_stage);
							void *resource_ptr = context->device->platform->get_resource_ptr(image_resource);
							ZEST_PRINT("            %s (%p), Layout: %s -> %s, \n"
									   "            Access: %s -> %s, \n"
									   "            Pipeline Stage: %s -> %s, \n"
									   "            QFI: %u -> %u",
									   image_resource->name, resource_ptr,
									   zest__image_layout_to_string(imb->old_layout), zest__image_layout_to_string(imb->new_layout),
									   src_access_mask.str, dst_access_mask.str,
									   src_pipeline_stage.str, dst_pipeline_stage.str,
									   imb->src_queue_family_index, imb->dst_queue_family_index);
							zest_FreeText(context->device->allocator, &src_access_mask);
							zest_FreeText(context->device->allocator, &dst_access_mask);
							zest_FreeText(context->device->allocator, &src_pipeline_stage);
							zest_FreeText(context->device->allocator, &dst_pipeline_stage);
						}
					}

					if (zest_vec_size(exe_details->barriers.release_buffer_barriers)) {
						ZEST_PRINT("        Release Buffers:");
						zest_vec_foreach(barrier_index, exe_details->barriers.release_buffer_barriers) {
							zest_buffer_barrier_t *bmb = &exe_details->barriers.release_buffer_barriers[barrier_index];
							zest_resource_node buffer_resource = exe_details->barriers.release_buffer_barrier_nodes[barrier_index];
							// You need a robust way to get resource_name from bmb->image
							zest_text_t src_access_mask = zest__access_flags_to_string(context, bmb->src_access_mask);
							zest_text_t dst_access_mask = zest__access_flags_to_string(context, bmb->dst_access_mask);
							zest_text_t src_pipeline_stage = zest__pipeline_stage_flags_to_string(context, bmb->src_stage);
							zest_text_t dst_pipeline_stage = zest__pipeline_stage_flags_to_string(context, bmb->dst_stage);
							void *resource_ptr = context->device->platform->get_resource_ptr(buffer_resource);
							ZEST_PRINT("            %s (%p) | \n"
									   "            Access: %s -> %s, \n"
									   "            Pipeline Stage: %s -> %s, \n"
									   "            QFI: %u -> %u, Size: %llu",
									   buffer_resource->name, resource_ptr,
									   src_access_mask.str, dst_access_mask.str,
									   src_pipeline_stage.str, dst_pipeline_stage.str,
									   bmb->src_queue_family_index, bmb->dst_queue_family_index,
									   buffer_resource->buffer_desc.size);
							zest_FreeText(context->device->allocator, &src_access_mask);
							zest_FreeText(context->device->allocator, &dst_access_mask);
							zest_FreeText(context->device->allocator, &src_pipeline_stage);
							zest_FreeText(context->device->allocator, &dst_pipeline_stage);
						}
					}
                }
            }

            // --- Print Signal Semaphores for the Batch ---
            if (batch->signal_values != 0) {
                ZEST_PRINT("  Signal Semaphores:");
                zest_vec_foreach(signal_index, batch->signal_values) {
                    zest_size stage_size = zest_vec_size(batch->signal_stages);
                    ZEST_ASSERT(signal_index < stage_size);
					zest_text_t pipeline_stages = zest__pipeline_stage_flags_to_string(context, batch->signal_stages[signal_index]);
                    if (batch->signal_values[signal_index] > 0) {
                        ZEST_PRINT("  Timeline Semaphore: %p, Stage: %s, Value: %zu", context->device->platform->get_final_signal_ptr(batch, signal_index), pipeline_stages.str, batch->signal_values[signal_index]);
                    } else {
                        ZEST_PRINT("  Binary Semaphore: %p Stage: %s,", context->device->platform->get_final_signal_ptr(batch, signal_index), pipeline_stages.str);
                    }
					zest_FreeText(context->device->allocator, &pipeline_stages);
                }
            }
            ZEST_PRINT(""); // End of batch
        }
    }
	ZEST_PRINT("--- End of Report ---");
#else
	ZEST_PRINT("--- Printing the frame graph is only available in debug mode or if you define ZEST_DEBUGGING ---");
#endif
}

void zest_EmptyRenderPass(const zest_command_list command_list, void *user_data) {
    //Nothing here to render, it's just for frame graphs that have nothing to render
}

zest_uint zest__acquire_bindless_image_index(zest_device device, zest_image image, zest_image_view view, zest_set_layout layout, zest_descriptor_set set, zest_binding_number_type target_binding_number, zest_descriptor_type descriptor_type) {
	ZEST_ASSERT_OR_VALIDATE(device->platform->image_layout_is_valid_for_descriptor(image), 
							device, "Tried to aquire an image index for an image that has an invalid layout.", ZEST_INVALID);
    zest_uint binding_number = ZEST_INVALID;
	zest_vec_foreach(i, layout->bindings) {
		zest_descriptor_binding_desc_t *binding = &layout->bindings[i];
		if (binding->binding == target_binding_number && binding->type == descriptor_type) {
			binding_number = binding->binding;
			break;
        }
	}

    ZEST_ASSERT(binding_number != ZEST_INVALID);    //Could not find an appropriate descriptor type in the layout with that target binding number!
    zest_uint array_index = zest__acquire_bindless_index(layout, binding_number);
    if (array_index == ZEST_INVALID) {
        //Ran out of space in the descriptor pool
        ZEST_REPORT(device, zest_report_bindless_indexes, "Ran out of space in the descriptor pool when trying to acquire an index for image, binding number %i.", binding_number);
        return ZEST_INVALID;
    }

    device->platform->update_bindless_image_descriptor(device, binding_number, array_index, descriptor_type, image, view, 0, set);

    image->bindless_index[binding_number] = array_index;
    return array_index;
}

zest_uint zest__acquire_bindless_sampler_index(zest_device device, zest_sampler sampler, zest_set_layout layout, zest_descriptor_set set, zest_binding_number_type target_binding_number) {
    zest_uint binding_number = ZEST_INVALID;
    zest_descriptor_type descriptor_type;
    zest_vec_foreach(i, layout->bindings) {
        zest_descriptor_binding_desc_t *binding = &layout->bindings[i];
        if (binding->binding == target_binding_number && binding->type == zest_descriptor_type_sampler) {
            descriptor_type = binding->type;
            binding_number = binding->binding;
            break;
        }
    }

    ZEST_ASSERT(binding_number != ZEST_INVALID);    //Could not find an appropriate descriptor type in the layout with that target binding number!
    zest_uint array_index = zest__acquire_bindless_index(layout, binding_number);
    if (array_index == ZEST_INVALID) {
        //Ran out of space in the descriptor pool
        ZEST_REPORT(device, zest_report_bindless_indexes, "Ran out of space in the descriptor pool when trying to acquire an index for a sampler, binding number %i.", binding_number);
        return ZEST_INVALID;
    }

    device->platform->update_bindless_image_descriptor(device, binding_number, array_index, descriptor_type, 0, 0, sampler, set);

    return array_index;
}

zest_uint zest__acquire_bindless_storage_buffer_index(zest_device device, zest_buffer buffer, zest_set_layout layout, zest_descriptor_set set, zest_uint target_binding_number) {
    zest_uint binding_number = ZEST_INVALID;
    zest_vec_foreach(i, layout->bindings) {
        zest_descriptor_binding_desc_t *layout_binding = &layout->bindings[i];
        if (target_binding_number == layout_binding->binding && layout_binding->type == zest_descriptor_type_storage_buffer) {
            binding_number = layout_binding->binding;
            break;
        }
    }

    ZEST_ASSERT(binding_number != ZEST_INVALID);    //Could not find an appropriate descriptor type in the layout with that target binding number!
    zest_uint array_index = zest__acquire_bindless_index(layout, binding_number);
    if (array_index == ZEST_INVALID) {
        //Ran out of space in the descriptor pool
        ZEST_REPORT(device, zest_report_bindless_indexes, "Ran out of space in the descriptor pool when trying to acquire an index for a storage buffer, binding number %i.", binding_number);
        return ZEST_INVALID;
    }
    device->platform->update_bindless_storage_buffer_descriptor(device, binding_number, array_index, buffer, set);

    return array_index;
}

zest_uint zest__acquire_bindless_uniform_buffer_index(zest_device device, zest_buffer buffer, zest_set_layout layout, zest_descriptor_set set, zest_uint target_binding_number) {
    zest_uint binding_number = ZEST_INVALID;
    zest_vec_foreach(i, layout->bindings) {
        zest_descriptor_binding_desc_t *layout_binding = &layout->bindings[i];
        if (target_binding_number == layout_binding->binding && layout_binding->type == zest_descriptor_type_uniform_buffer) {
            binding_number = layout_binding->binding;
            break;
        }
    }

    ZEST_ASSERT(binding_number != ZEST_INVALID);    //Could not find an appropriate descriptor type in the layout with that target binding number!
    zest_uint array_index = zest__acquire_bindless_index(layout, binding_number);
    if (array_index == ZEST_INVALID) {
        //Ran out of space in the descriptor pool
        ZEST_REPORT(device, zest_report_bindless_indexes, "Ran out of space in the descriptor pool when trying to acquire an index for a storage buffer, binding number %i.", binding_number);
        return ZEST_INVALID;
    }
    device->platform->update_bindless_uniform_buffer_descriptor(device, binding_number, array_index, buffer, set);

    return array_index;
}

zest_uint zest_AcquireSampledImageIndex(zest_device device, zest_image image, zest_binding_number_type binding_number) {
	ZEST_ASSERT_HANDLE(image);		//Not a valid image handle
    zest_uint index = zest__acquire_bindless_image_index(device, image, image->default_view, device->bindless_set_layout, device->bindless_set, binding_number, zest_descriptor_type_sampled_image);
    return index;
}

zest_uint zest_AcquireStorageImageIndex(zest_device device, zest_image image, zest_binding_number_type binding_number) {
	ZEST_ASSERT_HANDLE(image);		//Not a valid image handle
    zest_uint index = zest__acquire_bindless_image_index(device, image, image->default_view, device->bindless_set_layout, device->bindless_set, binding_number, zest_descriptor_type_storage_image);
    return index;
}

zest_uint zest_AcquireSamplerIndex(zest_device device, zest_sampler sampler) {
	ZEST_ASSERT_HANDLE(sampler);			//Not a valid sampler handle
    zest_uint index = zest__acquire_bindless_sampler_index(device, sampler, device->bindless_set_layout, device->bindless_set, zest_sampler_binding);
    return index;
}

zest_uint *zest_AcquireImageMipIndexes(zest_device device, zest_image image, zest_image_view_array view_array, zest_binding_number_type binding_number, zest_descriptor_type descriptor_type) {
	ZEST_ASSERT_HANDLE(image);			//Not a valid image handle
	ZEST_ASSERT_HANDLE(view_array);		//Not a valid view array handle
	ZEST_ASSERT(image->handle.store->data.allocator == view_array->handle.store->data.allocator);	//image and view arrays must have the same context!
    ZEST_ASSERT(image->info.mip_levels > 1);         //The resource does not have any mip levels. Make sure to set the number of mip levels when creating the resource in the frame graph

	zest_key image_key = zest_Hash(&image->handle, sizeof(zest_image_handle), ZEST_HASH_SEED);
    if (zest_map_valid_key(device->mip_indexes, image_key)) {
        zest_mip_index_collection *mip_collection = zest_map_at_key(device->mip_indexes, image_key);
		if (mip_collection->binding_numbers & (1 << binding_number)) {
			return mip_collection->mip_indexes[binding_number];
		}
	} else {
		zest_mip_index_collection mip_collection = ZEST__ZERO_INIT(zest_mip_index_collection);
		zest_map_insert_key(device->allocator, device->mip_indexes, image_key, mip_collection);
	}

	zest_mip_index_collection *mip_collection = zest_map_at_key(device->mip_indexes, image_key);

    mip_collection->binding_numbers |= (1 << binding_number);
    zest_set_layout global_layout = device->bindless_set_layout;
	zest_sampler sampler = 0;
    for (int mip_index = 0; mip_index != view_array->count; ++mip_index) {
        zest_uint bindless_index = zest__acquire_bindless_index(global_layout, binding_number);
		if (bindless_index == ZEST_INVALID) {
			//Ran out of space in the descriptor pool
			ZEST_REPORT(device, zest_report_bindless_indexes, "Ran out of space in the descriptor pool when trying to acquire an index for an image mip index, binding number %i.", binding_number);
			zest_vec_foreach(i, mip_collection->mip_indexes[binding_number]) {
				zest_ReleaseBindlessIndex(device, mip_collection->mip_indexes[binding_number][i], binding_number);
				zest_vec_free(device->allocator, mip_collection->mip_indexes[binding_number]);
			}
			return NULL;
		}
        device->platform->update_bindless_image_descriptor(device, binding_number, bindless_index, descriptor_type, image, &view_array->views[mip_index], sampler, device->bindless_set);
		zest_vec_push(device->allocator, mip_collection->mip_indexes[binding_number], bindless_index);
    }
	return mip_collection->mip_indexes[binding_number];
}

zest_uint zest_AcquireStorageBufferIndex(zest_device device, zest_buffer buffer) {
	ZEST_ASSERT(buffer);	//Not a valid buffer handle
    zest_uint index = zest__acquire_bindless_storage_buffer_index(device, buffer, device->bindless_set_layout, device->bindless_set, zest_storage_buffer_binding);
	return index;
}

zest_uint zest_AcquireUniformBufferIndex(zest_device device, zest_buffer buffer) {
	ZEST_ASSERT(buffer);	//Not a valid buffer handle
    return zest__acquire_bindless_uniform_buffer_index(device, buffer, device->bindless_set_layout, device->bindless_set, zest_uniform_buffer_binding);
}

void zest_AcquireInstanceLayerBufferIndex(zest_device device, zest_layer layer) {
	ZEST_ASSERT_HANDLE(layer); //ERROR: Not a valid layer pointer
    ZEST_ASSERT(ZEST__FLAGGED(layer->flags, zest_layer_flag_manual_fif));   //Layer must have been created with a persistant vertex buffer
    zest_ForEachFrameInFlight(fif) {
		layer->memory_refs[fif].descriptor_array_index = zest_AcquireStorageBufferIndex(device, layer->memory_refs[fif].device_vertex_data);
    }
    layer->bindless_set = device->bindless_set;
    ZEST__FLAG(layer->flags, zest_layer_flag_using_global_bindless_layout);
}

void zest_ReleaseStorageBufferIndex(zest_device device, zest_uint array_index) {
    zest__release_bindless_index(device->bindless_set_layout, zest_storage_buffer_binding, array_index);
}

void zest_ReleaseImageIndex(zest_device device, zest_image image, zest_binding_number_type binding_number) {
	ZEST_ASSERT_HANDLE(device);		//Not a valid device handle
	ZEST_ASSERT_HANDLE(image);		//Not a valid image handle
    ZEST_ASSERT(binding_number < zest_max_global_binding_number);
    if (image->bindless_index[binding_number] != ZEST_INVALID) {
        zest__release_bindless_index(device->bindless_set_layout, binding_number, image->bindless_index[binding_number]);
    }
}

void zest_ReleaseImageMipIndexes(zest_device device, zest_image image, zest_binding_number_type binding_number) {
	ZEST_ASSERT_HANDLE(device);		//Not a valid device handle
	zest_key image_key = zest_Hash(&image->handle, sizeof(zest_image_handle), ZEST_HASH_SEED);
    if (zest_map_valid_key(device->mip_indexes, image_key)) {
        zest_mip_index_collection *mip_collection = zest_map_at_key(device->mip_indexes, image_key);
		if (mip_collection->binding_numbers & (1 << binding_number)) {
			zest_vec_foreach(i, mip_collection->mip_indexes[binding_number]) {
				zest_uint index = mip_collection->mip_indexes[binding_number][i];
				zest__release_bindless_index(device->bindless_set_layout, binding_number, index);
			}
		}
	}
}

void zest_ReleaseAllImageIndexes(zest_device device, zest_image image) {
	ZEST_ASSERT_HANDLE(image);		//Not a valid image handle
    zest__release_all_global_texture_indexes(device, image);
}

void zest_ReleaseBindlessIndex(zest_device device, zest_uint index, zest_binding_number_type binding_number) {
    ZEST_ASSERT(index != ZEST_INVALID);
    zest__release_bindless_index(device->bindless_set_layout, binding_number, index);
}

zest_set_layout zest_GetBindlessLayout(zest_device device) {
	ZEST_ASSERT_HANDLE(device);		//Not a valid device handle
    return device->bindless_set_layout;
}

zest_descriptor_set zest_GetBindlessSet(zest_device device) {
	ZEST_ASSERT_HANDLE(device);		//Not a valid device handle
    return device->bindless_set;
}

zest_pipeline_layout zest_GetDefaultPipelineLayout(zest_device device) {
	ZEST_ASSERT_HANDLE(device);		//Not a valid device handle
    return device->pipeline_layout;
}

zest_resource_node zest__add_transient_image_resource(const char *name, const zest_image_info_t *description, zest_bool assign_bindless, zest_bool image_view_binding_only) {
    ZEST_ASSERT_HANDLE(zest__frame_graph_builder->frame_graph);        //Not a valid frame graph! Make sure you called BeginRenderGraph or BeginRenderToScreen
    zest_frame_graph frame_graph = zest__frame_graph_builder->frame_graph;
    zest_resource_node_t resource = ZEST__ZERO_INIT(zest_resource_node_t);
	zest_context context = zest__frame_graph_builder->context;
    resource.name = name;
	resource.id = frame_graph->id_counter++;
    resource.first_usage_pass_idx = ZEST_INVALID;
    resource.image.info = *description;
	resource.type = description->format == context->device->depth_format ? zest_resource_type_depth : zest_resource_type_image;
    resource.frame_graph = frame_graph;
    resource.current_queue_family_index = ZEST_QUEUE_FAMILY_IGNORED;
    resource.magic = zest_INIT_MAGIC(zest_struct_type_resource_node);
    resource.producer_pass_idx = -1;
    resource.image.backend = (zest_image_backend)context->device->platform->new_frame_graph_image_backend(&context->frame_graph_allocator[context->current_fif], &resource.image, NULL);
	ZEST__FLAG(resource.flags, zest_resource_node_flag_transient);
    zest_resource_node node = zest__add_frame_graph_resource(&resource);
    node->image_layout = zest_image_layout_undefined;
    return node;
}

void zest_FlagResourceAsEssential(zest_resource_node resource) {
    ZEST_ASSERT_HANDLE(resource);   //Not a valid resource handle!
    ZEST__FLAG(resource->flags, zest_resource_node_flag_essential_output);
}

void zest_DoNotCull() {
    ZEST_ASSERT_HANDLE(zest__frame_graph_builder->frame_graph);  	//This function must be called withing a Being/EndRenderGraph block
	zest_context context = zest__frame_graph_builder->context;
    ZEST_ASSERT_HANDLE(zest__frame_graph_builder->current_pass);    //No current pass found. Make sure you call zest_BeginPass
    zest_pass_node pass = zest__frame_graph_builder->current_pass;
	ZEST__FLAG(pass->flags, zest_pass_flag_do_not_cull);
}

void zest_AddSwapchainToGroup(zest_resource_group group) {
    ZEST_ASSERT_HANDLE(zest__frame_graph_builder->frame_graph);        //Not a valid frame graph! Make sure you called BeginRenderGraph or BeginRenderToScreen
    zest_frame_graph frame_graph = zest__frame_graph_builder->frame_graph;
    ZEST_ASSERT_HANDLE(frame_graph->swapchain_resource);    //frame graph must have a swapchain, use zest_ImportSwapchainResource to import the context swapchain to the render graph
    ZEST_ASSERT_HANDLE(group);                      //Not a valid render target group
	zest_context context = zest__frame_graph_builder->context;
    zest_vec_linear_push(&context->frame_graph_allocator[context->current_fif], group->resources, frame_graph->swapchain_resource);
}

void zest_AddResourceToGroup(zest_resource_group group, zest_resource_node image) {
    ZEST_ASSERT_HANDLE(zest__frame_graph_builder->frame_graph);        //Not a valid frame graph! Make sure you called BeginRenderGraph or BeginRenderToScreen
    zest_frame_graph frame_graph = zest__frame_graph_builder->frame_graph;
    ZEST_ASSERT_HANDLE(group);                      //Not a valid render target group
    ZEST_ASSERT(image->type & zest_resource_type_is_image_or_depth);  //Must be a depth or image resource type
	zest_FlagResourceAsEssential(image);
	zest_context context = zest__frame_graph_builder->context;
    zest_vec_linear_push(&context->frame_graph_allocator[context->current_fif], group->resources, image);
}

zest_resource_group zest_CreateResourceGroup() {
    ZEST_ASSERT_HANDLE(zest__frame_graph_builder->frame_graph);        //Not a valid frame graph! Make sure you called BeginRenderGraph or BeginRenderToScreen
	zest_context context = zest__frame_graph_builder->context;
    zest_frame_graph frame_graph = zest__frame_graph_builder->frame_graph;
    zest_resource_group group = (zest_resource_group)zest__linear_allocate(&context->frame_graph_allocator[context->current_fif], sizeof(zest_resource_group_t));
    *group = ZEST__ZERO_INIT(zest_resource_group_t);
    group->magic = zest_INIT_MAGIC(zest_struct_type_render_target_group);
    return group;
}

zest_resource_node zest_AddTransientImageResource(const char *name, zest_image_resource_info_t *info) {
    ZEST_ASSERT_HANDLE(zest__frame_graph_builder->frame_graph);        //Not a valid frame graph! Make sure you called BeginRenderGraph or BeginRenderToScreen
    zest_frame_graph frame_graph = zest__frame_graph_builder->frame_graph;
	zest_context context = zest__frame_graph_builder->context;
    zest_image_info_t description = ZEST__ZERO_INIT(zest_image_info_t);
    description.extent.width = info->width ? info->width : zest_ScreenWidth(context);
    description.extent.height = info->height ? info->height : zest_ScreenHeight(context);
    description.sample_count = (info->usage_hints & zest_resource_usage_hint_msaa) ? context->device->platform->get_msaa_sample_count(context) : zest_sample_count_1_bit;
    description.mip_levels = info->mip_levels ? info->mip_levels : 1;
    description.layer_count = info->layer_count ? info->layer_count : 1;
	description.format = info->format == zest_format_depth ? context->device->depth_format : info->format;
    description.aspect_flags = zest__determine_aspect_flag(description.format);
	zest_resource_node resource = zest__add_transient_image_resource(name, &description, 0, ZEST_TRUE);
    zest__interpret_hints(resource, info->usage_hints);
    return resource;
}

zest_resource_node zest_AddTransientBufferResource(const char *name, const zest_buffer_resource_info_t *info) {
    ZEST_ASSERT_HANDLE(zest__frame_graph_builder->frame_graph);        //Not a valid frame graph! Make sure you called BeginRenderGraph or BeginRenderToScreen
    zest_frame_graph frame_graph = zest__frame_graph_builder->frame_graph;
	zest_context context = zest__frame_graph_builder->context;
    zest_resource_node_t node = ZEST__ZERO_INIT(zest_resource_node_t);
    node.name = name;
    node.id = frame_graph->id_counter++;
    node.first_usage_pass_idx = ZEST_INVALID;
    node.buffer_desc.size = info->size;
    node.type = zest_resource_type_buffer;
	zest_memory_usage usage = info->usage_hints & zest_resource_usage_hint_cpu_transfer ? zest_memory_usage_cpu_to_gpu : zest_memory_usage_gpu_only;
    if (info->usage_hints & zest_resource_usage_hint_vertex_buffer) {
        node.buffer_desc.buffer_info = zest_CreateBufferInfo(zest_buffer_type_vertex, usage);
		ZEST__FLAG(node.type, zest_resource_type_vertex_buffer);
    } else if (info->usage_hints & zest_resource_usage_hint_index_buffer) {
        node.buffer_desc.buffer_info = zest_CreateBufferInfo(zest_buffer_type_index, usage);
		ZEST__FLAG(node.type, zest_resource_type_index_buffer);
    } else {
        node.buffer_desc.buffer_info = zest_CreateBufferInfo(zest_buffer_type_storage, usage);
    }
    if (info->usage_hints & zest_resource_usage_hint_copy_src) {
        node.buffer_desc.buffer_info.buffer_usage_flags |= zest_buffer_usage_transfer_src_bit;
    }
    node.frame_graph = frame_graph;
	node.buffer_desc.buffer_info.flags = zest_memory_pool_flag_single_buffer | zest_memory_pool_flag_transient;
    node.current_queue_family_index = ZEST_QUEUE_FAMILY_IGNORED;
    node.magic = zest_INIT_MAGIC(zest_struct_type_resource_node);
    node.producer_pass_idx = -1;
    ZEST__FLAG(node.flags, zest_resource_node_flag_transient);
    return zest__add_frame_graph_resource(&node);
}

zest_resource_node zest_AddTransientLayerResource(const char *name, const zest_layer layer, zest_bool prev_fif) {
	ZEST_ASSERT_HANDLE(layer); //ERROR: Not a valid layer pointer
    zest_size layer_size = layer->memory_refs[layer->fif].instance_count * layer->instance_struct_size;
    ZEST_ASSERT_HANDLE(zest__frame_graph_builder->frame_graph);        //Not a valid frame graph! Make sure you called BeginRenderGraph or BeginRenderToScreen
    zest_frame_graph frame_graph = zest__frame_graph_builder->frame_graph;
    zest_resource_node resource = 0;
	zest_context context = zest__frame_graph_builder->context;
    if (ZEST__NOT_FLAGGED(layer->flags, zest_layer_flag_manual_fif)) {
        zest_buffer_resource_info_t buffer_desc = ZEST__ZERO_INIT(zest_buffer_resource_info_t);
        buffer_desc.size = layer_size;
        buffer_desc.usage_hints = zest_resource_usage_hint_vertex_buffer;
        resource = zest_AddTransientBufferResource(name, &buffer_desc);
		resource->buffer_provider = zest__instance_layer_resource_provider;
    } else {
        zest_uint fif = prev_fif ? layer->prev_fif : layer->fif;
        zest_buffer buffer = layer->memory_refs[fif].device_vertex_data;
		zest_resource_node_t node = zest__create_import_buffer_resource_node(name, buffer);
		resource = zest__add_frame_graph_resource(&node);
        resource->bindless_index[0] = layer->memory_refs[fif].descriptor_array_index;
		if (prev_fif) {
			resource->buffer_provider = zest__instance_layer_resource_provider_prev_fif;
		} else {
			resource->buffer_provider = zest__instance_layer_resource_provider_current_fif;
		}
    }
    resource->user_data = layer;
    layer->vertex_buffer_node = resource;
    return resource;
}

zest_resource_node_t zest__create_import_buffer_resource_node(const char *name, zest_buffer buffer) {
    ZEST_ASSERT_HANDLE(zest__frame_graph_builder->frame_graph);        //Not a valid frame graph! Make sure you called BeginRenderGraph or BeginRenderToScreen
    zest_frame_graph frame_graph = zest__frame_graph_builder->frame_graph;
    zest_resource_node_t node = ZEST__ZERO_INIT(zest_resource_node_t);
    node.name = name;
	node.id = frame_graph->id_counter++;
    node.first_usage_pass_idx = ZEST_INVALID;
	node.type = zest_resource_type_buffer;
	if (buffer->memory_pool->allocator->buffer_info.buffer_usage_flags & zest_buffer_usage_index_buffer_bit) {
		ZEST__FLAG(node.type, zest_resource_type_index_buffer);
	} else if (buffer->memory_pool->allocator->buffer_info.buffer_usage_flags & zest_buffer_usage_vertex_buffer_bit) {
		ZEST__FLAG(node.type, zest_resource_type_vertex_buffer);
	}
    node.frame_graph = frame_graph;
    node.magic = zest_INIT_MAGIC(zest_struct_type_resource_node);
    node.storage_buffer = buffer;
	node.buffer_desc.size = buffer->size;
    node.current_queue_family_index = ZEST_QUEUE_FAMILY_IGNORED;
    node.producer_pass_idx = -1;
	ZEST__FLAG(node.flags, zest_resource_node_flag_imported);
	ZEST__FLAG(node.flags, zest_resource_node_flag_essential_output);
    return node;
}

zest_resource_node_t zest__create_import_image_resource_node(const char *name, zest_image image) {
    ZEST_ASSERT_HANDLE(zest__frame_graph_builder->frame_graph);        //Not a valid frame graph! Make sure you called BeginRenderGraph or BeginRenderToScreen
    zest_frame_graph frame_graph = zest__frame_graph_builder->frame_graph;
    zest_resource_node_t node = ZEST__ZERO_INIT(zest_resource_node_t);
    node.name = name;
	node.id = frame_graph->id_counter++;
    node.first_usage_pass_idx = ZEST_INVALID;
	node.type = zest_resource_type_image;
    node.frame_graph = frame_graph;
    node.magic = zest_INIT_MAGIC(zest_struct_type_resource_node);
    node.image = *image;
    node.view = image->default_view;
    node.current_queue_family_index = ZEST_QUEUE_FAMILY_IGNORED;
    node.producer_pass_idx = -1;
	ZEST__FLAG(node.flags, zest_resource_node_flag_imported);
	ZEST__FLAG(node.flags, zest_resource_node_flag_essential_output);
    return node;
}

zest_resource_node zest_ImportImageResource(const char *name, zest_image image, zest_resource_image_provider provider) {
	ZEST_ASSERT_HANDLE(image);		//Not a valid image handle
    ZEST_ASSERT_HANDLE(zest__frame_graph_builder->frame_graph);        //Not a valid frame graph! Make sure you called BeginRenderGraph or BeginRenderToScreen
    zest_frame_graph frame_graph = zest__frame_graph_builder->frame_graph;
    zest_resource_node_t resource = zest__create_import_image_resource_node(name, image);
	ZEST__FLAG(resource.flags, zest_resource_node_flag_imported);
    resource.image_provider = provider;
    zest_resource_node node = zest__add_frame_graph_resource(&resource);
	node->image_layout = image->info.layout;
    node->linked_layout = &image->info.layout;
    return node;
}

zest_resource_node zest_ImportBufferResource(const char *name, zest_buffer buffer, zest_resource_buffer_provider provider) {
    ZEST_ASSERT_HANDLE(zest__frame_graph_builder->frame_graph);        //Not a valid frame graph! Make sure you called BeginRenderGraph or BeginRenderToScreen
    zest_frame_graph frame_graph = zest__frame_graph_builder->frame_graph;
	zest_context context = zest__frame_graph_builder->context;
    zest_resource_node_t resource = zest__create_import_buffer_resource_node(name, buffer);
    resource.buffer_provider = provider;
    zest_resource_node node = zest__add_frame_graph_resource(&resource);
    return node;
}

zest_resource_node zest_ImportSwapchainResource() {
    ZEST_ASSERT_HANDLE(zest__frame_graph_builder->frame_graph);        //Not a valid frame graph! Make sure you called BeginRenderGraph or BeginRenderToScreen
    zest_frame_graph frame_graph = zest__frame_graph_builder->frame_graph;
	zest_context context = zest__frame_graph_builder->context;
    ZEST_ASSERT_OR_VALIDATE(ZEST__FLAGGED(context->flags, zest_context_flag_swap_chain_was_acquired),
							context->device, 
							"Swap chain is being imported but no swap chain image has been acquired. Make sure that you call zest_BeginFrame() and build the frame graph within zest_BeginFrame and zest_EndFrame to make sure that a swapchain image is acquired.",
							NULL);
    ZEST_ASSERT_OR_VALIDATE(!ZEST_VALID_HANDLE(frame_graph->swapchain, zest_struct_type_swapchain),
							context->device, 
							"Swap chain resource has already been imported! You only need to import it once per frame graph.",
							NULL);
	zest_swapchain swapchain = context->swapchain;
    zest__frame_graph_builder->frame_graph->swapchain = swapchain;
    zest_resource_node_t node = ZEST__ZERO_INIT(zest_resource_node_t);
    node.swapchain = swapchain;
    node.name = swapchain->name;
	node.id = frame_graph->id_counter++;
    node.first_usage_pass_idx = ZEST_INVALID;
	node.type = zest_resource_type_swap_chain_image;
    node.frame_graph = frame_graph;
    node.magic = zest_INIT_MAGIC(zest_struct_type_resource_node);
    node.image.backend = (zest_image_backend)context->device->platform->new_frame_graph_image_backend(&context->frame_graph_allocator[context->current_fif], &node.image, &swapchain->images[swapchain->current_image_frame]);
    node.image.info = swapchain->images[0].info;
    node.view = swapchain->views[swapchain->current_image_frame];
    node.current_queue_family_index = ZEST_QUEUE_FAMILY_IGNORED;
    node.producer_pass_idx = -1;
    node.image_provider = zest__swapchain_resource_provider;
	ZEST__FLAG(node.flags, zest_resource_node_flag_imported);
	ZEST__FLAG(node.flags, zest_resource_node_flag_essential_output);
    frame_graph->swapchain_resource = zest__add_frame_graph_resource(&node);
    frame_graph->swapchain_resource->image_layout = zest_image_layout_undefined;
    frame_graph->swapchain_resource->last_stage_mask = zest_pipeline_stage_top_of_pipe_bit;
	ZEST__FLAG(frame_graph->flags, zest_frame_graph_expecting_swap_chain_usage);
	if (!frame_graph->wait_on_timeline) {
		zest_execution_timeline timeline = &context->frame_timeline[context->current_fif];
		zest_WaitOnTimeline(timeline);
	}
    return frame_graph->swapchain_resource;
}

void zest_SetDescriptorSets(zest_pipeline_layout layout, zest_descriptor_set *descriptor_sets, zest_uint set_count) {
    ZEST_ASSERT_HANDLE(zest__frame_graph_builder->frame_graph);        //Not a valid frame graph! Make sure you called BeginRenderGraph or BeginRenderToScreen
    zest_frame_graph frame_graph = zest__frame_graph_builder->frame_graph;
	zest_context context = zest__frame_graph_builder->context;
	ZEST_ASSERT_HANDLE(layout);	//Invalid layout handle
	ZEST_ASSERT(set_count && set_count <= 64, "set_count must a value greater then 0 and less than 64 must match the number of descriptor sets that you pass in the array");
	zloc_linear_allocator_t *allocator = &context->frame_graph_allocator[context->current_fif];
	zest_vec_linear_resize(allocator, frame_graph->descriptor_sets, set_count);
	zest_vec_foreach(i, frame_graph->descriptor_sets) {
		ZEST_ASSERT_HANDLE(descriptor_sets[i]);	//Not a valid descriptor set handle!
		frame_graph->descriptor_sets[i] = descriptor_sets[i];
	}
	frame_graph->pipeline_layout = layout;
}

void zest_SetPassTask(zest_fg_execution_callback callback, void *user_data) {
	zest_context context = zest__frame_graph_builder->context;
    ZEST_ASSERT_OR_VALIDATE(zest__frame_graph_builder->current_pass, context->device, 
							"No current pass found, make sure you call zest_BeginPass", (void)0);
    zest_pass_node pass = zest__frame_graph_builder->current_pass;
    zest_pass_execution_callback_t callback_data;
    callback_data.callback = callback;
    callback_data.user_data = user_data;
    pass->execution_callback = callback_data;
}

zest_pass_node zest__add_pass_node(const char *name, zest_device_queue_type queue_type) {
    zest_frame_graph frame_graph = zest__frame_graph_builder->frame_graph;
    ZEST_ASSERT_HANDLE(frame_graph);        //Not a valid frame graph! Make sure you called BeginRenderGraph or BeginRenderToScreen
	zest_context context = zest__frame_graph_builder->context;
    zest_pass_node_t node = ZEST__ZERO_INIT(zest_pass_node_t);
    node.id = frame_graph->id_counter++;
    node.queue_info.queue_type = queue_type;
    node.name = name;
    node.magic = zest_INIT_MAGIC(zest_struct_type_pass_node);
    node.output_key = queue_type;
    switch (queue_type) {
		case zest_queue_graphics:
			node.queue_info.timeline_wait_stage = zest_pipeline_stage_index_input_bit | zest_pipeline_stage_vertex_input_bit | zest_pipeline_stage_vertex_shader_bit;
			node.type = zest_pass_type_graphics;
			break;
		case zest_queue_compute:
			node.queue_info.timeline_wait_stage = zest_pipeline_stage_compute_shader_bit;
			node.type = zest_pass_type_compute;
			break;
		case zest_queue_transfer:
			node.queue_info.timeline_wait_stage = zest_pipeline_stage_transfer_bit;
			node.type = zest_pass_type_transfer;
			break;
    }
    zest_pass_node pass_node = zest_bucket_array_linear_add(&context->frame_graph_allocator[context->current_fif], &frame_graph->potential_passes, zest_pass_node_t);
    *pass_node = node;
    return pass_node;
}

zest_resource_node zest__add_frame_graph_resource(zest_resource_node resource) {
    zest_frame_graph frame_graph = zest__frame_graph_builder->frame_graph;
    ZEST_ASSERT_HANDLE(frame_graph);        //Not a valid frame graph! Make sure you called BeginRenderGraph or BeginRenderToScreen
	zest_context context = zest__frame_graph_builder->context;
    zloc_linear_allocator_t *allocator = &context->frame_graph_allocator[context->current_fif];
    zest_resource_node node = zest_bucket_array_linear_add(allocator, &frame_graph->resources, zest_resource_node_t);
    *node = *resource;
    for (int i = 0; i != zest_max_global_binding_number; ++i) {
        node->bindless_index[i] = ZEST_INVALID;
    }
    zest_resource_versions_t resource_version = ZEST__ZERO_INIT(zest_resource_versions_t);
    node->version = 0;
    node->original_id = node->id;
    zest_vec_linear_push(allocator, resource_version.resources, node);
	if (!resource_version.resources) return NULL;
    zest_map_insert_linear_key(allocator, frame_graph->resource_versions, resource->id, resource_version);
    return node;
}

zest_resource_versions_t *zest__maybe_add_resource_version(zest_resource_node resource) {
    zest_frame_graph frame_graph = zest__frame_graph_builder->frame_graph;
    ZEST_ASSERT_HANDLE(frame_graph);        //Not a valid frame graph! Make sure you called BeginRenderGraph or BeginRenderToScreen
	zest_context context = zest__frame_graph_builder->context;
    zest_resource_versions_t *versions = zest_map_at_key(frame_graph->resource_versions, resource->original_id);
    zest_resource_node last_version = zest_vec_back(versions->resources);
    if (ZEST__FLAGGED(last_version->flags, zest_resource_node_flag_has_producer)) {
		zest_resource_node_t new_resource = ZEST__ZERO_INIT(zest_resource_node_t);
		new_resource.magic = zest_INIT_MAGIC(zest_struct_type_resource_node);
		new_resource.name = last_version->name;
		new_resource.frame_graph = frame_graph;
		new_resource.type = last_version->type;
		new_resource.aliased_resource = versions->resources[0];
		new_resource.image.info = resource->image.info;
		new_resource.buffer_desc = resource->buffer_desc;
		new_resource.id = frame_graph->id_counter++;
        new_resource.version = last_version->version + 1;
        new_resource.original_id = last_version->original_id;
        new_resource.producer_pass_idx = -1;
		ZEST__FLAG(new_resource.flags, zest_resource_node_flag_aliased);
        ZEST__FLAG(new_resource.flags, zest_resource_node_flag_has_producer);
		zloc_linear_allocator_t *allocator = &context->frame_graph_allocator[context->current_fif];
		zest_resource_node node = zest_bucket_array_linear_add(allocator, &frame_graph->resources, zest_resource_node_t);
        *node = new_resource;
        zest_vec_linear_push(allocator, versions->resources, node);
    }
	ZEST__FLAG(resource->flags, zest_resource_node_flag_has_producer);
    return versions;
}

zest_pass_node zest_BeginRenderPass(const char *name) {
	if (!zest__frame_graph_builder) return NULL;
	zest_context context = zest__frame_graph_builder->context;
	ZEST_ASSERT_OR_VALIDATE(ZEST_VALID_HANDLE(zest__frame_graph_builder->frame_graph, zest_struct_type_frame_graph),
							context->device, "Not a valid frame graph! Make sure you called BeginRenderGraph or BeginRenderToScreen", NULL);
	if (zest__frame_graph_builder->current_pass) {
		zest_EndPass();
	}
    zest_pass_node pass = zest__add_pass_node(name, zest_queue_graphics);
    pass->queue_info.timeline_wait_stage = zest_pipeline_stage_vertex_input_bit | zest_pipeline_stage_vertex_shader_bit;
	pass->bind_point = zest_bind_point_graphics;
    zest__frame_graph_builder->current_pass = pass;
    return pass;
}

zest_pass_node zest_BeginComputePass(zest_compute compute, const char *name) {
    ZEST_ASSERT_HANDLE(zest__frame_graph_builder->frame_graph);        //Not a valid frame graph! Make sure you called BeginRenderGraph or BeginRenderToScreen
	zest_context context = zest__frame_graph_builder->context;
    ZEST_ASSERT_OR_VALIDATE(compute, context->device, "Invalid compute object passed into zest_BeingComputePass.", NULL);        //Not a valid compute handle
	if (zest__frame_graph_builder->current_pass) {
		zest_EndPass();
	}
    zest_pass_node node = zest__add_pass_node(name, zest_queue_compute);
    node->compute = compute;
    node->queue_info.timeline_wait_stage = zest_pipeline_stage_compute_shader_bit;
    zest__frame_graph_builder->current_pass = node;
	node->bind_point = zest_bind_point_compute;
    return node;
}

zest_pass_node zest_BeginTransferPass(const char *name) {
    ZEST_ASSERT_HANDLE(zest__frame_graph_builder->frame_graph);        //Not a valid frame graph! Make sure you called BeginRenderGraph or BeginRenderToScreen
	zest_context context = zest__frame_graph_builder->context;
	if (zest__frame_graph_builder->current_pass) {
		zest_EndPass();
	}
    zest_pass_node pass = zest__add_pass_node(name, zest_queue_transfer);
    pass->queue_info.timeline_wait_stage = zest_pipeline_stage_transfer_bit;
    zest__frame_graph_builder->current_pass = pass;
	pass->bind_point = zest_bind_point_none;
    return pass;
}

void zest_EndPass() {
	zest_context context = zest__frame_graph_builder->context;
    ZEST_ASSERT_OR_VALIDATE(zest__frame_graph_builder->current_pass, context->device, 
							"No begin pass found, make sure you call BeginRender/Compute/TransferPass", (void)0); 
    zest__frame_graph_builder->current_pass = 0;
}

zest_resource_node zest_GetPassInputResource(const zest_command_list command_list, const char *name) {
    ZEST_ASSERT(zest_map_valid_name(command_list->pass_node->inputs, name));  //Not a valid input resource name. Check the name and also maybe you meant to get from outputs?
    zest_resource_usage_t *usage = zest_map_at(command_list->pass_node->inputs, name);
    return ZEST_VALID_HANDLE(usage->resource_node->aliased_resource, zest_struct_type_resource_node) ? usage->resource_node->aliased_resource : usage->resource_node;
}

zest_buffer zest_GetPassInputBuffer(const zest_command_list command_list, const char *name) {
    ZEST_ASSERT(zest_map_valid_name(command_list->pass_node->inputs, name));  //Not a valid input resource name. Check the name and also maybe you meant to get from outputs?
    zest_resource_usage_t *usage = zest_map_at(command_list->pass_node->inputs, name);
    zest_resource_node resource = ZEST_VALID_HANDLE(usage->resource_node->aliased_resource, zest_struct_type_resource_node) ? usage->resource_node->aliased_resource : usage->resource_node;
    return resource->storage_buffer;
}

zest_buffer zest_GetPassOutputBuffer(const zest_command_list command_list, const char *name) {
    ZEST_ASSERT(zest_map_valid_name(command_list->pass_node->outputs, name));  //Not a valid input resource name. Check the name and also maybe you meant to get from inputs?
    zest_resource_usage_t *usage = zest_map_at(command_list->pass_node->outputs, name);
    zest_resource_node resource = ZEST_VALID_HANDLE(usage->resource_node->aliased_resource, zest_struct_type_resource_node) ? usage->resource_node->aliased_resource : usage->resource_node;
    return resource->storage_buffer;
}

zest_resource_node zest_GetPassOutputResource(const zest_command_list command_list, const char *name) {
    ZEST_ASSERT(zest_map_valid_name(command_list->pass_node->outputs, name));  //Not a valid output resource name. Check the name and also maybe you meant to get from inputs?
    zest_resource_usage_t *usage = zest_map_at(command_list->pass_node->outputs, name);
    return ZEST_VALID_HANDLE(usage->resource_node->aliased_resource, zest_struct_type_resource_node) ? usage->resource_node->aliased_resource : usage->resource_node;
}

zest_uint zest_GetTransientSampledImageBindlessIndex(const zest_command_list command_list, zest_resource_node resource, zest_binding_number_type binding_number) {
    ZEST_ASSERT_HANDLE(resource);            // Not a valid resource handle
    ZEST_ASSERT(resource->type & zest_resource_type_is_image);  //Must be an image resource type
    if (resource->bindless_index[binding_number] != ZEST_INVALID) return resource->bindless_index[binding_number];
	zest_context context = zest__frame_graph_builder->context;
	zest_device device = context->device;
    zest_frame_graph frame_graph = command_list->frame_graph;
    zest_set_layout bindless_layout = frame_graph->bindless_layout;
    zest_uint bindless_index = zest__acquire_bindless_index(bindless_layout, binding_number);
    if (bindless_index == ZEST_INVALID) {
        //Ran out of space in the descriptor pool
        ZEST_REPORT(context->device, zest_report_bindless_indexes, "Ran out of space in the descriptor pool when trying to acquire an index for a sampled image, binding number %i.", binding_number);
        return ZEST_INVALID;
    }

	zest_descriptor_type descriptor_type = binding_number == zest_storage_image_binding ? zest_descriptor_type_storage_image : zest_descriptor_type_sampled_image;
    device->platform->update_bindless_image_descriptor(device, binding_number, bindless_index, descriptor_type, &resource->image, resource->view, 0, frame_graph->bindless_set);

    zest_binding_index_for_release_t binding_index = { frame_graph->bindless_layout, bindless_index, (zest_uint)binding_number };
    zest_vec_push(context->allocator, context->deferred_resource_freeing_list.transient_binding_indexes[context->current_fif], binding_index);

    resource->bindless_index[binding_number];
    return bindless_index;
}

zest_uint *zest_GetTransientSampledMipBindlessIndexes(const zest_command_list command_list, zest_resource_node resource, zest_binding_number_type binding_number) {
    zest_set_layout bindless_layout = command_list->frame_graph->bindless_layout;
    ZEST_ASSERT(resource->type & zest_resource_type_is_image);  //Must be an image resource type
    ZEST_ASSERT(resource->image.info.mip_levels > 1);   //The resource does not have any mip levels. Make sure to set the number of mip levels when creating the resource in the frame graph
    ZEST_ASSERT(resource->current_state_index < zest_vec_size(resource->journey));
	if (zest_vec_size(resource->mip_level_bindless_indexes[binding_number])) {
		return resource->mip_level_bindless_indexes[binding_number];
	}
    zest_frame_graph frame_graph = command_list->frame_graph;
	zest_context context = zest__frame_graph_builder->context;
	zest_device device = context->device;

    zloc_linear_allocator_t *allocator = &context->frame_graph_allocator[context->current_fif];
    if (!resource->view_array) {
        resource->view_array = device->platform->create_image_views_per_mip(context->device, &resource->image, zest_image_view_type_2d, 0, resource->image.info.layer_count, allocator);
		resource->view_array->handle.store = &context->device->resource_stores[zest_handle_type_view_arrays];
    }
	zest_descriptor_type descriptor_type = binding_number == zest_storage_image_binding ? zest_descriptor_type_storage_image : zest_descriptor_type_sampled_image;
	for (int mip_index = 0; mip_index != resource->view_array->count; ++mip_index) {
		zest_uint bindless_index = zest__acquire_bindless_index(bindless_layout, binding_number);
		if (bindless_index == ZEST_INVALID) {
			//Ran out of space in the descriptor pool
			ZEST_REPORT(context->device, zest_report_bindless_indexes, "Ran out of space in the descriptor pool when trying to acquire an index for an image mip index, binding number %i.", binding_number);
			return NULL;
		}
		zest_vec_linear_push(allocator, resource->mip_level_bindless_indexes[binding_number], bindless_index);

        device->platform->update_bindless_image_descriptor(device, binding_number, bindless_index, descriptor_type, &resource->image, &resource->view_array->views[mip_index], 0, frame_graph->bindless_set);

		zest_binding_index_for_release_t mip_binding_index = { frame_graph->bindless_layout, bindless_index, (zest_uint)binding_number };
		zest_vec_push(context->allocator, context->deferred_resource_freeing_list.transient_binding_indexes[context->current_fif], mip_binding_index);
	}
	return resource->mip_level_bindless_indexes[binding_number];
}

zest_uint zest_GetTransientBufferBindlessIndex(const zest_command_list command_list, zest_resource_node resource) {
    zest_set_layout bindless_layout = command_list->frame_graph->bindless_layout;
    ZEST_ASSERT_HANDLE(resource);   //Not a valid resource handle
    ZEST_ASSERT(resource->type & zest_resource_type_buffer);   //Must be a buffer resource type for this bindlesss index acquisition
	zest_context context = zest__frame_graph_builder->context;
	zest_device device = context->device;
    if (resource->bindless_index[0] != ZEST_INVALID) return resource->bindless_index[0];
    zest_frame_graph frame_graph = command_list->frame_graph;
	zest_uint bindless_index = zest__acquire_bindless_index(bindless_layout, zest_storage_buffer_binding);
	if (bindless_index == ZEST_INVALID) return bindless_index;

    device->platform->update_bindless_storage_buffer_descriptor(device, zest_storage_buffer_binding, bindless_index, resource->storage_buffer, frame_graph->bindless_set);

	zest_binding_index_for_release_t binding_index = { frame_graph->bindless_layout, bindless_index, zest_storage_buffer_binding };
	zest_vec_push(context->allocator, context->deferred_resource_freeing_list.transient_binding_indexes[context->current_fif], binding_index);
    resource->bindless_index[0] = bindless_index;
    return bindless_index;
}

zest_uint zest_GetResourceMipLevels(zest_resource_node resource) {
    ZEST_ASSERT_HANDLE(resource);	//Not a valid resource handle!
    return resource->image.info.mip_levels;
}

zest_uint zest_GetResourceWidth(zest_resource_node resource) {
    ZEST_ASSERT_HANDLE(resource);	//Not a valid resource handle!
    return resource->image.info.extent.width;
}

zest_uint zest_GetResourceHeight(zest_resource_node resource) {
    ZEST_ASSERT_HANDLE(resource);	//Not a valid resource handle!
    return resource->image.info.extent.height;
}

void zest_SetResourceBufferSize(zest_resource_node resource, zest_size size) {
    ZEST_ASSERT_HANDLE(resource);	//Not a valid resource handle!
	resource->buffer_desc.size = size;
}

zest_image zest_GetResourceImage(zest_resource_node resource) {
	ZEST_ASSERT_HANDLE(resource);   //Not a valid resource handle!
	if (resource->type & zest_resource_type_is_image_or_depth) {
        return &resource->image;
	}
	return NULL;
}

zest_resource_type zest_GetResourceType(zest_resource_node resource_node) {
	ZEST_ASSERT_HANDLE(resource_node);   //Not a valid resource handle!
	return resource_node->type;
}

zest_image_info_t zest_GetResourceImageDescription(zest_resource_node resource_node) {
	ZEST_ASSERT_HANDLE(resource_node);   //Not a valid resource handle!
	return resource_node->image.info;
}

void *zest_GetResourceUserData(zest_resource_node resource_node) {
	ZEST_ASSERT_HANDLE(resource_node);	 //Not a valid resource handle!
	return resource_node->user_data;
}

void zest_SetResourceUserData(zest_resource_node resource_node, void *user_data) {
	ZEST_ASSERT_HANDLE(resource_node);	 //Not a valid resource handle!
	resource_node->user_data = user_data;
}

void zest_SetResourceBufferProvider(zest_resource_node resource_node, zest_resource_buffer_provider buffer_provider) {
	ZEST_ASSERT_HANDLE(resource_node);	//Not a valid resource node!
	resource_node->buffer_provider = buffer_provider;
}

void zest_SetResourceImageProvider(zest_resource_node resource_node, zest_resource_image_provider image_provider) {
	ZEST_ASSERT_HANDLE(resource_node);	//Not a valid resource node!
	resource_node->image_provider = image_provider;
}

void zest_SetResourceClearColor(zest_resource_node resource, float red, float green, float blue, float alpha) {
    ZEST_ASSERT_HANDLE(resource);   //Not a valid resource handle!
    resource->clear_color = ZEST_STRUCT_LITERAL(zest_vec4, red, green, blue, alpha);
}

void zest__add_pass_buffer_usage(zest_pass_node pass_node, zest_resource_node resource, zest_resource_purpose purpose, zest_pipeline_stage_flags relevant_pipeline_stages, zest_bool is_output) {
    zest_resource_usage_t usage = ZEST__ZERO_INIT(zest_resource_usage_t);    
    usage.resource_node = resource;
    usage.purpose = purpose;

    switch (purpose) {
		case zest_purpose_vertex_buffer:
			usage.access_mask = zest_access_vertex_attribute_read_bit;
			usage.stage_mask = zest_pipeline_stage_vertex_input_bit;
			resource->buffer_desc.buffer_info.buffer_usage_flags |= zest_buffer_usage_vertex_buffer_bit;
			break;
		case zest_purpose_index_buffer:
			usage.access_mask = zest_access_index_read_bit | zest_access_vertex_attribute_read_bit;
			usage.stage_mask = zest_pipeline_stage_index_input_bit | zest_pipeline_stage_vertex_input_bit;
			resource->buffer_desc.buffer_info.buffer_usage_flags |= zest_buffer_usage_index_buffer_bit;
			break;
		case zest_purpose_uniform_buffer:
			usage.access_mask = zest_access_uniform_read_bit;
			usage.stage_mask = relevant_pipeline_stages; 
			resource->buffer_desc.buffer_info.buffer_usage_flags |= zest_buffer_usage_uniform_buffer_bit;
			break;
		case zest_purpose_storage_buffer_read:
			usage.access_mask = zest_access_shader_read_bit;
			usage.stage_mask = relevant_pipeline_stages;
			resource->buffer_desc.buffer_info.buffer_usage_flags |= zest_buffer_usage_storage_buffer_bit;
			break;
		case zest_purpose_storage_buffer_write:
			usage.access_mask = zest_access_shader_write_bit;
			usage.stage_mask = relevant_pipeline_stages;
			resource->buffer_desc.buffer_info.buffer_usage_flags |= zest_buffer_usage_storage_buffer_bit;
			break;
		case zest_purpose_storage_buffer_read_write:
			usage.access_mask = zest_access_shader_read_bit | zest_access_shader_write_bit;
			usage.stage_mask = relevant_pipeline_stages;
			resource->buffer_desc.buffer_info.buffer_usage_flags |= zest_buffer_usage_storage_buffer_bit;
			break;
		case zest_purpose_transfer_buffer:
			usage.access_mask = zest_access_transfer_read_bit | zest_access_transfer_write_bit;
			usage.stage_mask = zest_pipeline_stage_transfer_bit;
			resource->buffer_desc.buffer_info.buffer_usage_flags |= zest_buffer_usage_transfer_src_bit | zest_buffer_usage_transfer_dst_bit;
			break;
		default:
			ZEST_ASSERT(0);     //Unhandled buffer access purpose! Make sure you pass in a valid zest_resource_purpose
			return;
    }

	zest_context context = zest__frame_graph_builder->context;
    if (is_output) { // Or derive is_output from purpose (e.g. WRITE implies output)
        zest_map_linear_insert(&context->frame_graph_allocator[context->current_fif], pass_node->outputs, resource->name, usage);
		pass_node->output_key += resource->id + zest_Hash(&usage, offsetof(zest_resource_usage_t, access_mask), 0);
    } else {
		resource->reference_count++;
        zest_map_linear_insert(&context->frame_graph_allocator[context->current_fif], pass_node->inputs, resource->name, usage);
    }
}

zest_submission_batch_t *zest__get_submission_batch(zest_uint submission_id) {
    zest_uint submission_index = ZEST__SUBMISSION_INDEX(submission_id);
    zest_uint queue_index = ZEST__QUEUE_INDEX(submission_id);
    zest_frame_graph frame_graph = zest__frame_graph_builder->frame_graph;
    ZEST_ASSERT_HANDLE(frame_graph);
    return &frame_graph->submissions[submission_index].batches[queue_index];
}

void zest__set_rg_error_status(zest_frame_graph frame_graph, zest_frame_graph_result result) {
    ZEST__FLAG(frame_graph->error_status, result);
	zest_context context = zest__frame_graph_builder->context;
	ZEST__UNFLAG(context->flags, zest_context_flag_building_frame_graph);
}

zest_image_usage_flags zest__get_image_usage_from_state(zest_resource_state state) {
    switch (state) {
		case zest_resource_state_render_target:
			return zest_image_usage_color_attachment_bit;
		case zest_resource_state_depth_stencil_write:
		case zest_resource_state_depth_stencil_read:
			return zest_image_usage_depth_stencil_attachment_bit;
		case zest_resource_state_shader_read:
			// This state could be fulfilled by a sampled image or an input attachment.
			// To be safe, you can require both, or have more specific states.
			// Let's assume for now it implies sampled.
			return zest_image_usage_sampled_bit;
		case zest_resource_state_unordered_access:
			return zest_image_usage_storage_bit;
		case zest_resource_state_copy_src:
			return zest_image_usage_transfer_src_bit;
		case zest_resource_state_copy_dst:
			return zest_image_usage_transfer_dst_bit;
		case zest_resource_state_present:
			// Presenting requires being a color attachment.
			return zest_image_usage_color_attachment_bit;
		default:
			return 0;
    }
}

zest_resource_usage_t zest__configure_image_usage(zest_resource_node resource, zest_resource_purpose purpose, zest_format format, zest_load_op load_op, zest_load_op stencil_load_op, zest_pipeline_stage_flags relevant_pipeline_stages) {

    zest_resource_usage_t usage = ZEST__ZERO_INIT(zest_resource_usage_t);

    usage.resource_node = resource;
    usage.aspect_flags = zest__determine_aspect_flag(format);   // Default based on format
    usage.load_op = load_op;
    usage.stencil_load_op = stencil_load_op;

    switch (purpose) {
		case zest_purpose_color_attachment_write:
			usage.image_layout = zest_image_layout_color_attachment_optimal;
			usage.access_mask = zest_access_color_attachment_write_bit;
			// If load_op is zest_attachment_load_op_load, AN IMPLICIT READ OCCURS.
			// If blending with destination, zest_access_color_attachment_read_bit IS ALSO INVOLVED.
			// For simplicity, if this is purely about the *write* aspect:
			if (load_op == zest_load_op_load) {
				usage.access_mask |= zest_access_color_attachment_read_bit; // For the load itself
			}
			usage.stage_mask = zest_pipeline_stage_color_attachment_output_bit;
			usage.aspect_flags = zest_image_aspect_color_bit;
			usage.is_output = ZEST_TRUE;
			resource->image.info.flags |= zest_image_flag_color_attachment;
			break;

		case zest_purpose_color_attachment_read:
			usage.image_layout = zest_image_layout_color_attachment_optimal;
			usage.access_mask = zest_access_color_attachment_read_bit;
			usage.stage_mask = zest_pipeline_stage_color_attachment_output_bit;
			usage.aspect_flags = zest_image_aspect_color_bit;
			resource->image.info.flags |= zest_image_flag_color_attachment;
			usage.is_output = ZEST_FALSE;
			break;

		case zest_purpose_sampled_image:
			usage.image_layout = zest_image_layout_shader_read_only_optimal;
			usage.access_mask = zest_access_shader_read_bit;
			usage.stage_mask = relevant_pipeline_stages;
			resource->image.info.flags |= zest_image_flag_sampled;
			usage.is_output = ZEST_FALSE;
			break;

		case zest_purpose_storage_image_read:
			usage.image_layout = zest_image_layout_general;
			usage.access_mask = zest_access_shader_read_bit;
			usage.stage_mask = relevant_pipeline_stages;
			resource->image.info.flags |= zest_image_flag_sampled | zest_image_flag_storage;
			usage.is_output = ZEST_FALSE;
			break;

		case zest_purpose_storage_image_write:
			usage.image_layout = zest_image_layout_general;
			usage.access_mask = zest_access_shader_write_bit;
			usage.stage_mask = relevant_pipeline_stages;
			resource->image.info.flags |= zest_image_flag_storage;
			usage.is_output = ZEST_TRUE;
			break;

		case zest_purpose_storage_image_read_write:
			usage.image_layout = zest_image_layout_general;
			usage.access_mask = zest_access_shader_read_bit | zest_access_shader_write_bit;
			usage.stage_mask = relevant_pipeline_stages;
			resource->image.info.flags |= zest_image_flag_sampled | zest_image_flag_storage;
			usage.is_output = ZEST_TRUE;
			break;

		case zest_purpose_depth_stencil_attachment_read:
			usage.image_layout = zest_image_layout_depth_stencil_read_only_optimal;
			usage.access_mask = zest_access_depth_stencil_attachment_read_bit;
			usage.stage_mask = zest_pipeline_stage_early_fragment_tests_bit | zest_pipeline_stage_late_fragment_tests_bit;
			usage.aspect_flags = zest_image_aspect_depth_bit;
			resource->image.info.flags |= zest_image_flag_depth_stencil_attachment;
			usage.is_output = ZEST_FALSE;
			break;

		case zest_purpose_depth_stencil_attachment_write:
			usage.image_layout = zest_image_layout_depth_stencil_attachment_optimal;
			usage.access_mask = zest_access_depth_stencil_attachment_write_bit;
			if (load_op == zest_load_op_load || stencil_load_op == zest_load_op_load) {
				usage.access_mask |= zest_access_depth_stencil_attachment_read_bit;
			}
			usage.aspect_flags = zest_image_aspect_depth_bit;
			usage.stage_mask = zest_pipeline_stage_early_fragment_tests_bit | zest_pipeline_stage_late_fragment_tests_bit;
			resource->image.info.flags |= zest_image_flag_depth_stencil_attachment;
			usage.is_output = ZEST_TRUE;
			break;

		case zest_purpose_depth_stencil_attachment_read_write: // Typical depth testing and writing
			usage.image_layout = zest_image_layout_depth_stencil_attachment_optimal;
			usage.access_mask = zest_access_depth_stencil_attachment_read_bit | zest_access_depth_stencil_attachment_write_bit;
			usage.stage_mask = zest_pipeline_stage_early_fragment_tests_bit | zest_pipeline_stage_late_fragment_tests_bit;
			usage.aspect_flags = zest_image_aspect_depth_bit;
			resource->image.info.flags |= zest_image_flag_depth_stencil_attachment;
			usage.is_output = ZEST_TRUE; // Modifies resource
			break;

		case zest_purpose_input_attachment: // For subpass reads
			usage.image_layout = zest_image_layout_shader_read_only_optimal;
			usage.access_mask = zest_access_input_attachment_read_bit;
			usage.stage_mask = relevant_pipeline_stages;
			resource->image.info.flags |= zest_image_flag_input_attachment;
			usage.is_output = ZEST_FALSE;
			break;

		case zest_purpose_transfer_image:
			usage.image_layout = zest_image_layout_transfer_src_optimal;
			usage.access_mask = zest_access_transfer_read_bit;
			usage.stage_mask = zest_pipeline_stage_transfer_bit;
			resource->image.info.flags |= zest_image_flag_transfer_src | zest_image_flag_transfer_dst;
			usage.is_output = ZEST_FALSE;
			break;

		case zest_purpose_present_src:
			usage.image_layout = zest_image_layout_present;
			usage.access_mask = 0; // No specific GPU access by the pass itself for this state.
			usage.stage_mask = zest_pipeline_stage_bottom_of_pipe_bit; // ENSURE ALL PRIOR WORK IS DONE.
			resource->image.info.flags |= zest_image_flag_color_attachment;
			usage.is_output = ZEST_TRUE;
			break;

		default:
			ZEST_ASSERT(0); //Unhandled image access purpose!"
    }
	return usage;
}

void zest__add_pass_image_usage(zest_pass_node pass_node, zest_resource_node image_resource, zest_resource_purpose purpose, zest_pipeline_stage_flags relevant_pipeline_stages, zest_bool is_output, zest_load_op load_op, zest_store_op store_op, zest_load_op stencil_load_op, zest_store_op stencil_store_op, zest_clear_value_t clear_value) {
    zest_resource_usage_t usage = zest__configure_image_usage(image_resource, purpose, image_resource->image.info.format, load_op, stencil_load_op, relevant_pipeline_stages);

    // Attachment ops are only relevant for attachment purposes
    // For other usages, they'll default to 0/DONT_CARE.
    usage.store_op = store_op;
    usage.stencil_store_op = stencil_store_op;
    usage.clear_value = clear_value;
    usage.purpose = purpose;
	zest_context context = zest__frame_graph_builder->context;
  
    if (usage.is_output) {
		//This is the first time this resource has been used as output
		usage.resource_node = image_resource;
		zest_map_linear_insert(&context->frame_graph_allocator[context->current_fif], pass_node->outputs, image_resource->name, usage);
		zest_size offset = offsetof(zest_resource_usage_t, load_op);
		pass_node->output_key += image_resource->id + zest_Hash(&usage, offsetof(zest_resource_usage_t, access_mask), 0);
    } else {
        usage.resource_node = image_resource;
        ZEST_ASSERT(usage.resource_node);
		image_resource->reference_count++;
		zest_map_linear_insert(&context->frame_graph_allocator[context->current_fif], pass_node->inputs, image_resource->name, usage);
    }
}

void zest_ReleaseBufferAfterUse(zest_resource_node node) {
    ZEST_ASSERT(ZEST__FLAGGED(node->flags, zest_resource_node_flag_imported));  //The resource must be imported, transient buffers are simply discarded after use.
    ZEST__FLAG(node->flags, zest_resource_node_flag_release_after_use);
}

// --- Image Helpers ---
void zest_ConnectInput(zest_resource_node resource) {
    ZEST_ASSERT_HANDLE(zest__frame_graph_builder->frame_graph);  //This function must be called withing a Being/EndRenderGraph block
	zest_context context = zest__frame_graph_builder->context;
    ZEST_ASSERT_HANDLE(zest__frame_graph_builder->current_pass);          //No current pass found. Make sure you call zest_BeginPass
    zest_pass_node pass = zest__frame_graph_builder->current_pass;
    ZEST_ASSERT_OR_VALIDATE(ZEST_VALID_HANDLE(resource, zest_struct_type_resource_node), 
							context->device, "Not a valid resource node pointer. Make sure you pass in a valid resource node", (void)0);
    zest_pipeline_stage_flags stages = 0;
    zest_resource_purpose purpose = zest_purpose_none;
    if (resource->type & zest_resource_type_is_image) {
        switch (pass->type) {
			case zest_pass_type_graphics:
				stages = zest_pipeline_stage_fragment_shader_bit;
				purpose = zest_purpose_sampled_image;
				break;
			case zest_pass_type_compute:
				stages = zest_pipeline_stage_compute_shader_bit;
				purpose = zest_purpose_storage_image_read;
				break;
			case zest_pass_type_transfer:
				stages = zest_pipeline_stage_transfer_bit;
				purpose = zest_purpose_transfer_image;
				break;
        }
        zest__add_pass_image_usage(pass, resource, purpose, stages, ZEST_FALSE,
								   zest_load_op_dont_care, zest_store_op_dont_care, zest_load_op_dont_care, zest_store_op_dont_care, ZEST__ZERO_INIT(zest_clear_value_t));
		//Flag the resource as read only. This means that if it's used as output in a future pass then it will be
		//marked as attachment readonly. For example you might want to write to a depth buffer but then only
		//read from it in future passes.
		ZEST__FLAG(resource->flags, zest_resource_node_flag_read_only);
    } else {
        //Buffer input
        switch (pass->type) {
			case zest_pass_type_graphics: {
				stages = zest_pipeline_stage_vertex_input_bit;
				if (resource->type & zest_resource_type_index_buffer) {
					purpose = zest_purpose_index_buffer;
				} else {
					purpose = zest_purpose_vertex_buffer;
				}
				break;
			}
			case zest_pass_type_compute: {
				stages = zest_pipeline_stage_compute_shader_bit;
				purpose = zest_purpose_storage_buffer_read_write;
				break;
			}
			case zest_pass_type_transfer: {
				stages = zest_pipeline_stage_transfer_bit;
				purpose = zest_purpose_transfer_buffer;
				break;
			}
        }
		zest__add_pass_buffer_usage(pass, resource, purpose, stages, ZEST_FALSE);
    }
}

void zest_ConnectSwapChainOutput() {
    ZEST_ASSERT_HANDLE(zest__frame_graph_builder->frame_graph);  //This function must be called withing a Being/EndRenderGraph block
	zest_context context = zest__frame_graph_builder->context;
    ZEST_ASSERT_OR_VALIDATE(ZEST_VALID_HANDLE(zest__frame_graph_builder->current_pass, zest_struct_type_pass_node), 
							context->device, "Tried to connect output but no BeginPass function was called.", (void)0); //No current pass found. Make sure you call zest_BeginPass
    zest_pass_node pass = zest__frame_graph_builder->current_pass;
    zest_frame_graph frame_graph = zest__frame_graph_builder->frame_graph;
    ZEST_ASSERT_OR_VALIDATE(ZEST_VALID_HANDLE(frame_graph->swapchain_resource, zest_struct_type_resource_node),
							context->device, "Not a valid swapchain resource, did you call zest_ImportSwapchainResource and/or call zest_BeginFrame to ensure a swap chain image was acquired?",
							(void)0);  //
    zest_clear_value_t cv = frame_graph->swapchain->clear_color;
    // Assuming clear for swapchain if not explicitly loaded
    zest__add_pass_image_usage(pass, frame_graph->swapchain_resource, zest_purpose_color_attachment_write, 
							   zest_pipeline_stage_color_attachment_output_bit, ZEST_TRUE,
							   zest_load_op_clear, zest_store_op_store, zest_load_op_dont_care, zest_store_op_dont_care, cv);
}

void zest_ConnectOutput(zest_resource_node resource) {
    ZEST_ASSERT_HANDLE(zest__frame_graph_builder->frame_graph);  //This function must be called withing a Being/EndRenderGraph block
	zest_context context = zest__frame_graph_builder->context;
    ZEST_ASSERT_OR_VALIDATE(ZEST_VALID_HANDLE(zest__frame_graph_builder->current_pass, zest_struct_type_pass_node), 
							context->device, "Tried to connect output but no BeginPass function was called.", (void)0); //No current pass found. Make sure you call zest_BeginPass
    zest_pass_node pass = zest__frame_graph_builder->current_pass;
    ZEST_ASSERT_OR_VALIDATE(ZEST_VALID_HANDLE(resource, zest_struct_type_resource_node), 
							context->device, "Not a valid resource node pointer. Make sure you pass in a valid resource node", (void)0);
    if (resource->image.info.sample_count > 1) {
        ZEST__FLAG(pass->flags, zest_pass_flag_output_resolve);
    }
    if (resource->type & zest_resource_type_is_image) {
        zest_clear_value_t cv = ZEST__ZERO_INIT(zest_clear_value_t);
		// If the resource was already used as output in a render pass then preserve the contents
		zest_bool preserve_contents = ZEST__FLAGGED(resource->flags, zest_resource_node_flag_preserve);
		zest_bool readonly = ZEST__FLAGGED(resource->flags, zest_resource_node_flag_read_only);
        switch (pass->type) {
			case zest_pass_type_graphics: {
				ZEST_ASSERT(resource->type & zest_resource_type_is_image); //Resource must be an image buffer when used as output in a graphics pass
				if (zest__is_depth_stencil_format(resource->image.info.format)) {
					cv.depth_stencil.depth = 1.f;
					cv.depth_stencil.stencil = 0;
					if (!preserve_contents) {
						zest__add_pass_image_usage(pass, resource, zest_purpose_depth_stencil_attachment_write,
												   0, ZEST_TRUE,
												   zest_load_op_clear, zest_store_op_store,
												   zest_load_op_dont_care, zest_store_op_dont_care,
												   cv);
					} else {
						zest__add_pass_image_usage(pass, resource, readonly ? zest_purpose_depth_stencil_attachment_read : zest_purpose_depth_stencil_attachment_write,
												   0, ZEST_TRUE,
												   zest_load_op_load, zest_store_op_store,
												   zest_load_op_dont_care, zest_store_op_dont_care,
												   cv);
					}
				} else {
					cv.color = resource->clear_color;
					zest__add_pass_image_usage(pass, resource, zest_purpose_color_attachment_write,
											   zest_pipeline_stage_color_attachment_output_bit, ZEST_TRUE,
											   preserve_contents ? zest_load_op_load : zest_load_op_clear, zest_store_op_store,
											   zest_load_op_dont_care, zest_store_op_dont_care,
											   cv);
				}
				break;
			}
			case zest_pass_type_compute: {
				zest__add_pass_image_usage(pass, resource, zest_purpose_storage_image_write, zest_pipeline_stage_compute_shader_bit,
										   ZEST_FALSE, zest_load_op_dont_care, zest_store_op_dont_care,
										   zest_load_op_dont_care, zest_store_op_dont_care, ZEST__ZERO_INIT(zest_clear_value_t));
				break;
			}
			case zest_pass_type_transfer: {
				zest__add_pass_image_usage(pass, resource, zest_purpose_transfer_image, zest_pipeline_stage_compute_shader_bit,
										   ZEST_FALSE, zest_load_op_dont_care, zest_store_op_dont_care,
										   zest_load_op_dont_care, zest_store_op_dont_care, ZEST__ZERO_INIT(zest_clear_value_t));
				break;
			}
        }
		//Preserve the contents of the buffer from this point on so that it can be used as output in other passes
		ZEST__FLAG(resource->flags, zest_resource_node_flag_preserve);
    } else {
        //Buffer output
        switch (pass->type) {
			case zest_pass_type_graphics: {
				if (resource->type & zest_resource_type_is_image) {
					zest__add_pass_buffer_usage(pass, resource, zest_purpose_storage_image_write,
												zest_pipeline_stage_color_attachment_output_bit, ZEST_TRUE);
				} else {
					zest__add_pass_buffer_usage(pass, resource, zest_purpose_transfer_buffer,
												zest_pipeline_stage_transfer_bit, ZEST_TRUE);
				}
				break;

			}
			case zest_pass_type_compute: {
				zest__add_pass_buffer_usage(pass, resource, zest_purpose_storage_buffer_write,
											zest_pipeline_stage_compute_shader_bit, ZEST_TRUE);
				break;
			}
			case zest_pass_type_transfer: {
				zest__add_pass_buffer_usage(pass, resource, zest_purpose_transfer_buffer, 
											zest_pipeline_stage_transfer_bit, ZEST_TRUE);
				break;
			}
        }

    }
}

void zest_ConnectOutputGroup(zest_resource_group group) {
    ZEST_ASSERT_HANDLE(zest__frame_graph_builder->frame_graph);  //This function must be called withing a Being/EndRenderGraph block
	zest_context context = zest__frame_graph_builder->context;
    ZEST_ASSERT_HANDLE(zest__frame_graph_builder->current_pass);          //No current pass found. Make sure you call zest_BeginPass
    zest_pass_node pass = zest__frame_graph_builder->current_pass;
    zest_frame_graph frame_graph = zest__frame_graph_builder->frame_graph;
    ZEST_ASSERT_HANDLE(group);  //Not a valid render target group
    ZEST_ASSERT_HANDLE(pass);   //Not a valid pass node
    ZEST_ASSERT(zest_vec_size(group->resources));   //There are no resources in the group!
    zest_vec_foreach(i, group->resources) {
        zest_resource_node resource = group->resources[i];
        switch (resource->type) {
			case zest_resource_type_swap_chain_image: zest_ConnectSwapChainOutput(); break;
			default: zest_ConnectOutput(resource); break;
        }
    }
}

void zest_ConnectInputGroup(zest_resource_group group) {
    ZEST_ASSERT_HANDLE(zest__frame_graph_builder->frame_graph);  //This function must be called withing a Being/EndRenderGraph block
	zest_context context = zest__frame_graph_builder->context;
    ZEST_ASSERT_HANDLE(zest__frame_graph_builder->current_pass);          //No current pass found. Make sure you call zest_BeginPass
    zest_pass_node pass = zest__frame_graph_builder->current_pass;
    zest_frame_graph frame_graph = zest__frame_graph_builder->frame_graph;
    ZEST_ASSERT_HANDLE(group);  //Not a valid render target group
    ZEST_ASSERT_HANDLE(pass);   //Not a valid pass node
    ZEST_ASSERT(zest_vec_size(group->resources));   //There are no resources in the group!
    zest_vec_foreach(i, group->resources) {
        zest_resource_node resource = group->resources[i];
		ZEST_ASSERT_OR_VALIDATE(resource->type != zest_resource_type_swap_chain_image, context->device,
								"A swapchain resource cannot be used as input.", (void)0);
		zest_ConnectInput(resource);
    }
}

void zest_WaitOnTimeline(zest_execution_timeline timeline) {
    ZEST_ASSERT_HANDLE(timeline);    //Not a valid execution timeline. Use zest_CreateExecutionTimeline to create one
    ZEST_ASSERT_HANDLE(zest__frame_graph_builder->frame_graph);  //This function must be called withing a Being/EndRenderGraph block
    zest_frame_graph frame_graph = zest__frame_graph_builder->frame_graph;
	zest_context context = zest__frame_graph_builder->context;
    frame_graph->wait_on_timeline = timeline;
}

void zest_SignalTimeline(zest_execution_timeline timeline) {
    ZEST_ASSERT_HANDLE(timeline);    //Not a valid execution timeline. Use zest_CreateExecutionTimeline to create one
    ZEST_ASSERT_HANDLE(zest__frame_graph_builder->frame_graph);  //This function must be called withing a Being/EndRenderGraph block
    zest_frame_graph frame_graph = zest__frame_graph_builder->frame_graph;
	zest_context context = zest__frame_graph_builder->context;
    frame_graph->signal_timeline = timeline;
}

zest_bool zest__initialise_timeline(zest_device device, zest_execution_timeline_t *timeline) {
    timeline->magic = zest_INIT_MAGIC(zest_struct_type_execution_timeline);
    timeline->current_value = 0;
	timeline->device = device;
    if (!device->platform->create_execution_timeline_backend(device, timeline)) {
        if (timeline->backend) {
            ZEST__FREE(device->allocator, timeline->backend);
        }
		ZEST__FREE(device->allocator, timeline);
		return ZEST_FALSE;
    }
	return ZEST_TRUE;
}

zest_execution_timeline zest_CreateExecutionTimeline(zest_device device) {
    zest_execution_timeline timeline = ZEST__NEW(device->allocator, zest_execution_timeline);
    *timeline = ZEST__ZERO_INIT(zest_execution_timeline_t);
	if (!zest__initialise_timeline(device, timeline)) {
		ZEST__FREE(device->allocator, timeline);
		timeline = NULL;
	}
    return timeline;
}

void zest_FreeExecutionTimeline(zest_execution_timeline timeline) {
	ZEST_ASSERT_HANDLE(timeline);	//Not a valid timeline handle
	zest_device device = timeline->device;
	zest_uint index = device->frame_counter % ZEST_MAX_FIF;
	zest_vec_push(device->allocator, device->deferred_resource_freeing_list.resources[index], timeline);
}

// --End frame graph functions

// [Swapchain]
zest_swapchain zest_GetSwapchain(zest_context context) {
    return context->swapchain;
}

zest_format zest_GetSwapchainFormat(zest_swapchain swapchain) {
    ZEST_ASSERT_HANDLE(swapchain);  //Not a valid swapchain handle
    return swapchain->format;
}

void zest_SetSwapchainClearColor(zest_context context, float red, float green, float blue, float alpha) {
    context->swapchain->clear_color = ZEST_STRUCT_LITERAL(zest_clear_value_t, red, green, blue, alpha);
}

zest_layer_handle zest__create_instance_layer(zest_context context, const char *name, zest_size instance_type_size, zest_uint initial_instance_count) {
    zest_layer layer;
    zest_layer_handle handle = zest__new_layer(context, &layer);
    layer->name = name;
    zest__initialise_instance_layer(context, layer, instance_type_size, initial_instance_count);
	zest__activate_resource(handle.store, handle.value);
    return handle;
}

void zest__draw_instance_mesh_layer(zest_command_list command_list, zest_layer layer, zest_pipeline_template pipeline_template) {
    if (layer->vertex_data && layer->index_data) {
        zest_cmd_BindMeshVertexBuffer(command_list, layer);
        zest_cmd_BindMeshIndexBuffer(command_list, layer);
    } else {
        ZEST_REPORT(command_list->context->device, zest_report_layers, "No Vertex/Index data found in mesh layer [%s]!", layer->name);
        return;
    }

	zest_buffer device_buffer = zest_GetLayerResourceBuffer(layer);
	ZEST_ASSERT(device_buffer, "Transient buffer not found in the layer. Make sure you're passing in the correct layer in the user data and you connected the layer resource as input.");
	zest_cmd_BindVertexBuffer(command_list, 1, 1, device_buffer);

	zest_device device = command_list->context->device;

	zest_pipeline current_pipeline = 0;
	zest_pipeline pipeline = 0;
	if (ZEST_VALID_HANDLE(pipeline_template, zest_struct_type_pipeline_template)) {
		pipeline = zest_GetPipeline(pipeline_template, command_list);
		if (pipeline) {
			zest_cmd_BindPipeline(command_list, pipeline);
			current_pipeline = pipeline;
		}
	}

    zest_bool has_instruction_view_port = ZEST_FALSE;
    zest_vec_foreach(i, layer->draw_instructions[layer->fif]) {
        zest_layer_instruction_t *current = &layer->draw_instructions[layer->fif][i];

		zest_mesh_offset_data_t *mesh_offsets = &layer->mesh_offsets[current->mesh_index];

        if (current->draw_mode == zest_draw_mode_viewport) {
			zest_cmd_Scissor(command_list, &current->scissor);
            zest_cmd_ViewPort(command_list, &current->viewport);
            has_instruction_view_port = ZEST_TRUE;
            continue;
        } else if(!has_instruction_view_port) {
			zest_cmd_Scissor(command_list, &layer->scissor);
            zest_cmd_ViewPort(command_list, &layer->viewport);
        }

        pipeline = !pipeline ? zest_GetPipeline(current->pipeline_template, command_list) : pipeline;
        if (pipeline && pipeline != current_pipeline) {
			zest_cmd_BindPipeline(command_list, pipeline);
			current_pipeline = pipeline;
        } else if(!pipeline) {
            continue;
        }

		zest_cmd_SendPushConstants(command_list, (void*)current->push_constant, ZEST_MAX_PUSH_SIZE);

		zest_cmd_DrawIndexed(command_list, mesh_offsets->index_count, current->total_instances, mesh_offsets->index_offset, mesh_offsets->vertex_offset, current->start_index);
    }
}

zest_layer_handle zest_CreateMeshLayer(zest_context context, const char* name, zest_size vertex_struct_size, zest_size vertex_capacity, zest_size index_capacity) {
    zest_layer layer;
    zest_layer_handle handle = zest__new_layer(context, &layer);
    layer->name = name;
    zest__initialise_mesh_layer(context, layer, vertex_struct_size, 1000);
	zest__activate_resource(handle.store, handle.value);
    return handle;
}

zest_layer_handle zest_CreateInstanceMeshLayer(zest_context context, const char *name, zest_size instance_struct_size, zest_size vertex_capacity, zest_size index_capacity) {
    zest_layer layer;
    zest_layer_handle handle = zest__new_layer(context, &layer);
    layer->name = name;
    zest__initialise_instance_layer(context, layer, instance_struct_size, 1000);
	zest_buffer_info_t vertex_info = zest_CreateBufferInfo(zest_buffer_type_vertex, zest_memory_usage_gpu_only);
	zest_buffer_info_t index_info = zest_CreateBufferInfo(zest_buffer_type_index, zest_memory_usage_gpu_only);
	layer->vertex_data = zest_CreateBuffer(context->device, vertex_capacity, &vertex_info);
	layer->index_data = zest_CreateBuffer(context->device, index_capacity, &index_info);
	zest__activate_resource(handle.store, handle.value);
    return handle;
}

// --Texture and Image functions
zest_image_handle zest__new_image(zest_device device) {
	zest_resource_store_t *store = &device->resource_stores[zest_handle_type_images];
    zest_image_handle handle = ZEST_STRUCT_LITERAL(zest_image_handle, zest__add_store_resource(store), store );
	handle.store = store;
    zest_image image = (zest_image)zest__get_store_resource_unsafe(store, handle.value);
    *image = ZEST__ZERO_INIT(zest_image_t);
    image->magic = zest_INIT_MAGIC(zest_struct_type_image);
    image->backend = (zest_image_backend)device->platform->new_image_backend(device);
    for (int i = 0; i != zest_max_global_binding_number; ++i) {
        image->bindless_index[i] = ZEST_INVALID;
    }
	image->handle = handle;
    return handle;
}

ZEST_PRIVATE zest_image_handle zest__create_image(zest_device device, zest_image_info_t *create_info) {
	zest_image_handle null_handle = ZEST__ZERO_INIT(zest_image_handle);
	ZEST_ASSERT_OR_VALIDATE(create_info->extent.width * create_info->extent.height * create_info->extent.depth > 0,
		device, "Image has 0 dimensions", null_handle);
	ZEST_ASSERT_OR_VALIDATE(create_info->flags,
		device, "You must set flags in the image info to specify how the image will be used", null_handle);
	ZEST_ASSERT_OR_VALIDATE(create_info->format > zest_format_undefined && create_info->format < zest_max_format,
		device, "Invalid image format specified", null_handle);
	//Validate format/usage compatibility
	if (zest__is_depth_stencil_format(create_info->format)) {
		ZEST_ASSERT_OR_VALIDATE(!ZEST__FLAGGED(create_info->flags, zest_image_flag_color_attachment),
			device, "Depth/stencil format cannot be used as color attachment", null_handle);
	} else {
		ZEST_ASSERT_OR_VALIDATE(!ZEST__FLAGGED(create_info->flags, zest_image_flag_depth_stencil_attachment),
			device, "Color format cannot be used as depth/stencil attachment", null_handle);
	}
	if (zest__is_compressed_format(create_info->format)) {
		ZEST_ASSERT_OR_VALIDATE(!ZEST__FLAGGED(create_info->flags, zest_image_flag_color_attachment) &&
			!ZEST__FLAGGED(create_info->flags, zest_image_flag_depth_stencil_attachment),
			device, "Compressed format cannot be used as render attachment", null_handle);
		ZEST_ASSERT_OR_VALIDATE(!ZEST__FLAGGED(create_info->flags, zest_image_flag_generate_mipmaps),
			device, "Cannot generate mipmaps for compressed formats. Use pre-compressed mipmaps in KTX files instead.", null_handle);
	}
	if (ZEST__FLAGGED(create_info->flags, zest_image_flag_cubemap)) {
		ZEST_ASSERT_OR_VALIDATE(create_info->layer_count > 0 && create_info->layer_count % 6 == 0,
			device, "Cubemap must have layers in multiples of 6", null_handle);
	}
	//Check if the format is supported with the requested flags
	ZEST_ASSERT_OR_VALIDATE(device->platform->is_image_format_supported(device, create_info->format, create_info->flags),
		device, "Image format is not supported with the requested usage flags", null_handle);
	//For example you could use zest_image_preset_texture. Lookup the zest_image_flag_bits enum
	//to see all the flags available.
	zest_image_handle handle = zest__new_image(device);
    zest_image image = (zest_image)zest__get_store_resource_unsafe(handle.store, handle.value);
    image->info = *create_info;
    image->info.aspect_flags = zest__determine_aspect_flag_for_view(create_info->format);
    image->info.mip_levels = create_info->mip_levels > 0 ? create_info->mip_levels : 1;
    if (ZEST__FLAGGED(create_info->flags, zest_image_flag_generate_mipmaps) && image->info.mip_levels == 1) {
        image->info.mip_levels = (zest_uint)floor(log2(ZEST__MAX(create_info->extent.width, create_info->extent.height))) + 1;
    }
    if (!device->platform->create_image(device, image, image->info.layer_count, zest_sample_count_1_bit, create_info->flags)) {
        zest__cleanup_image(image);
        return ZEST__ZERO_INIT(zest_image_handle);
    }
	image->info.layout = zest_image_layout_undefined;
    if (ZEST__FLAGGED(image->info.flags, zest_image_flag_storage)) {
        zest_queue queue = zest_imm_BeginCommandBuffer(device, zest_queue_graphics);
        zest_imm_TransitionImage(queue, image, zest_image_layout_general, 0, ZEST__ALL_MIPS, 0, ZEST__ALL_LAYERS);
        zest_imm_EndCommandBuffer(queue);
    }
    zest_image_view_type view_type = zest__get_image_view_type(image);
    image->default_view = device->platform->create_image_view(device, image, view_type, image->info.mip_levels, 0, 0, image->info.layer_count, 0);
	image->default_view->handle.store = &device->resource_stores[zest_handle_type_views];
	return handle;
}

zest_image zest__get_image_unsafe(zest_image_handle handle) {
	zest_image image = (zest_image)zest__get_store_resource_unsafe(handle.store, handle.value);
	return image;
}

void zest__release_all_global_texture_indexes(zest_device device, zest_image image) {
    if (!device->bindless_set_layout) {
        return;
    }
    for (int i = 0; i != zest_max_global_binding_number; ++i) {
        if (image->bindless_index[i] != ZEST_INVALID) {
            zest__release_bindless_index(device->bindless_set_layout, (zest_binding_number_type)i, image->bindless_index[i]);
            image->bindless_index[i] = ZEST_INVALID;
        }
    }

	zest_key hashed_key = zest_Hash(&image->handle, sizeof(zest_image_handle), ZEST_HASH_SEED);
	if (zest_map_valid_key(device->mip_indexes, hashed_key)) {
		zest_mip_index_collection *collection = zest_map_at_key(device->mip_indexes, hashed_key);
		int active_bindings = collection->binding_numbers;
		if (active_bindings) {
			int current_binding = zloc__scan_reverse(active_bindings);
			while (current_binding >= 0) {
				zest_vec_foreach(i, collection->mip_indexes[current_binding]) {
					zest_uint index = collection->mip_indexes[current_binding][i];
					zest_ReleaseBindlessIndex(device, index, (zest_binding_number_type)current_binding);
				}
				zest_vec_free(device->allocator, collection->mip_indexes[current_binding]);
				active_bindings &= ~(1 << current_binding);
				current_binding = active_bindings ? zloc__scan_reverse(active_bindings) : -1;
			}
		}
		collection->binding_numbers = 0;
	}
}

void zest__release_all_image_indexes(zest_device device) {
    if (!device->bindless_set_layout) {
        return;
    }

	zest_map_foreach (i, device->mip_indexes) {
		zest_mip_index_collection *collection = &device->mip_indexes.data[i];
		zest_uint active_bindings = collection->binding_numbers;
		if (!active_bindings) continue;
		int current_binding = zloc__scan_reverse(active_bindings);
		while (current_binding >= 0) {
			zest_vec_foreach(j, collection->mip_indexes[current_binding]) {
				zest_uint index = collection->mip_indexes[current_binding][j];
				zest_ReleaseBindlessIndex(device, index, (zest_binding_number_type)current_binding);
			}
			zest_vec_free(device->allocator, collection->mip_indexes[current_binding]);
			active_bindings &= ~(1 << current_binding);
			current_binding = zloc__scan_reverse(active_bindings);
		}
		collection->binding_numbers = 0;
	}
	zest_map_free(device->allocator, device->mip_indexes);
}



// --Internal Texture functions
void zest__cleanup_image(zest_image image) {
	zest_device device = (zest_device)image->handle.store->origin;
	zest__release_all_global_texture_indexes(device, image);
	device->platform->cleanup_image_backend(image);
	if (ZEST__NOT_FLAGGED(image->info.flags, zest_image_flag_transient)) {
		ZEST__FREE(device->allocator, image->buffer);
	}
    if (image->default_view) {
        device->platform->cleanup_image_view_backend(image->default_view);
        ZEST__FREE(device->allocator, image->default_view);
    }
    image->magic = 0;
    zest__remove_store_resource(image->handle.store, image->handle.value);
}

void zest__cleanup_sampler(zest_sampler sampler) {
	zest_device device = (zest_device)sampler->handle.store->origin;
    device->platform->cleanup_sampler_backend(sampler);
    zest__remove_store_resource(sampler->handle.store, sampler->handle.value);
    sampler->magic = 0;
}

void zest__cleanup_uniform_buffer(zest_uniform_buffer uniform_buffer) {
	zest_context context = (zest_context)uniform_buffer->handle.store->origin;
    zest_ForEachFrameInFlight(fif) {
        zest_buffer buffer = uniform_buffer->buffer[fif];
        zest_FreeBufferNow(buffer);
		zest__release_bindless_index(context->device->bindless_set_layout, zest_uniform_buffer_binding, uniform_buffer->descriptor_index[fif]);
		uniform_buffer->descriptor_index[fif] = ZEST_INVALID;
    }
    zest__remove_store_resource(uniform_buffer->handle.store, uniform_buffer->handle.value);
}

void zest__cleanup_execution_timeline(zest_execution_timeline timeline) {
	timeline->device->platform->cleanup_execution_timeline_backend(timeline);
	ZEST__FREE(timeline->device->allocator, timeline);
}

void zest_GetFormatPixelData(zest_format format, int *channels, int *bytes_per_pixel, int *block_width, int *block_height, int *bytes_per_block) {
	*channels = 0;
	*bytes_per_pixel = 0;
	*block_width = 1;
	*block_height = 1;
	*bytes_per_block = 0;
    switch (format) {
		case zest_format_undefined:
		case zest_format_r8_unorm:
		case zest_format_r8_snorm:
		case zest_format_r8_srgb: {
			*channels = 1;
			*bytes_per_pixel = 1;
			*bytes_per_block = 1;
			break;
		}
		case zest_format_r8g8_unorm:
		case zest_format_r8g8_snorm:
		case zest_format_r8g8_uint:
		case zest_format_r8g8_sint:
		case zest_format_r8g8_srgb: {
			*channels = 2;
			*bytes_per_pixel = 2;
			*bytes_per_block = 2;
			break;
		}
		case zest_format_r8g8b8_unorm:
		case zest_format_r8g8b8_snorm:
		case zest_format_r8g8b8_uint:
		case zest_format_r8g8b8_sint:
		case zest_format_r8g8b8_srgb:
		case zest_format_b8g8r8_unorm:
		case zest_format_b8g8r8_snorm:
		case zest_format_b8g8r8_uint:
		case zest_format_b8g8r8_sint:
		case zest_format_b8g8r8_srgb: {
			*channels = 3;
			*bytes_per_pixel = 3;
			*bytes_per_block = 3;
			break;
		}
		case zest_format_r8g8b8a8_unorm:
		case zest_format_r8g8b8a8_snorm:
		case zest_format_r8g8b8a8_uint:
		case zest_format_r8g8b8a8_sint:
		case zest_format_r8g8b8a8_srgb:
		case zest_format_b8g8r8a8_unorm:
		case zest_format_b8g8r8a8_snorm:
		case zest_format_b8g8r8a8_uint:
		case zest_format_b8g8r8a8_sint:
		case zest_format_b8g8r8a8_srgb: {
			*channels = 4;
			*bytes_per_pixel = 4;
			*bytes_per_block = 4;
			break;
		}
		case zest_format_r16_sfloat:
		case zest_format_r16_uint:
		case zest_format_r16_sint: {
			*channels = 1;
			*bytes_per_pixel = 2;
			*bytes_per_block = 2;
			break;
		}
		case zest_format_r16g16_sfloat:
		case zest_format_r16g16_uint:
		case zest_format_r16g16_sint: {
			*channels = 2;
			*bytes_per_pixel = 4;
			*bytes_per_block = 4;
			break;
		}
		case zest_format_r16g16b16_sfloat:
		case zest_format_r16g16b16_uint:
		case zest_format_r16g16b16_sint: {
			*channels = 3;
			*bytes_per_pixel = 6;
			*bytes_per_block = 6;
			break;
		}
		case zest_format_r16g16b16a16_sfloat:
		case zest_format_r16g16b16a16_uint:
		case zest_format_r16g16b16a16_sint: {
			*channels = 4;
			*bytes_per_pixel = 8;
			*bytes_per_block = 8;
			break;
		}
		case zest_format_r32_sfloat:
		case zest_format_r32_uint:
		case zest_format_r32_sint: {
			*channels = 1;
			*bytes_per_pixel = 4;
			*bytes_per_block = 4;
			break;
		}
		case zest_format_r32g32_sfloat:
		case zest_format_r32g32_uint:
		case zest_format_r32g32_sint: {
			*channels = 2;
			*bytes_per_pixel = 8;
			*bytes_per_block = 8;
			break;
		}
		case zest_format_r32g32b32_sfloat:
		case zest_format_r32g32b32_uint:
		case zest_format_r32g32b32_sint: {
			*channels = 3;
			*bytes_per_pixel = 12;
			*bytes_per_block = 12;
			break;
		}
		case zest_format_r32g32b32a32_sfloat:
		case zest_format_r32g32b32a32_uint:
		case zest_format_r32g32b32a32_sint: {
			*channels = 4;
			*bytes_per_pixel = 16;
			*bytes_per_block = 16;
			break;
		}
		// BC compressed formats (all use 4x4 blocks)
		case zest_format_bc1_rgb_unorm_block:
		case zest_format_bc1_rgb_srgb_block: {
			*channels = 3;
			*block_width = 4;
			*block_height = 4;
			*bytes_per_block = 8;
			break;
		}
		case zest_format_bc1_rgba_unorm_block:
		case zest_format_bc1_rgba_srgb_block: {
			*channels = 4;
			*block_width = 4;
			*block_height = 4;
			*bytes_per_block = 8;
			break;
		}
		case zest_format_bc2_unorm_block:
		case zest_format_bc2_srgb_block:
		case zest_format_bc3_unorm_block:
		case zest_format_bc3_srgb_block:
		case zest_format_bc7_unorm_block:
		case zest_format_bc7_srgb_block: {
			*channels = 4;
			*block_width = 4;
			*block_height = 4;
			*bytes_per_block = 16;
			break;
		}
		case zest_format_bc4_unorm_block:
		case zest_format_bc4_snorm_block: {
			*channels = 1;
			*block_width = 4;
			*block_height = 4;
			*bytes_per_block = 8;
			break;
		}
		case zest_format_bc5_unorm_block:
		case zest_format_bc5_snorm_block: {
			*channels = 2;
			*block_width = 4;
			*block_height = 4;
			*bytes_per_block = 16;
			break;
		}
		case zest_format_bc6h_ufloat_block:
		case zest_format_bc6h_sfloat_block: {
			*channels = 3;
			*block_width = 4;
			*block_height = 4;
			*bytes_per_block = 16;
			break;
		}
		// ETC2 compressed formats (all use 4x4 blocks)
		case zest_format_etc2_r8g8b8_unorm_block:
		case zest_format_etc2_r8g8b8_srgb_block: {
			*channels = 3;
			*block_width = 4;
			*block_height = 4;
			*bytes_per_block = 8;
			break;
		}
		case zest_format_etc2_r8g8b8a1_unorm_block:
		case zest_format_etc2_r8g8b8a1_srgb_block: {
			*channels = 4;
			*block_width = 4;
			*block_height = 4;
			*bytes_per_block = 8;
			break;
		}
		case zest_format_etc2_r8g8b8a8_unorm_block:
		case zest_format_etc2_r8g8b8a8_srgb_block: {
			*channels = 4;
			*block_width = 4;
			*block_height = 4;
			*bytes_per_block = 16;
			break;
		}
		// EAC compressed formats (all use 4x4 blocks)
		case zest_format_eac_r11_unorm_block:
		case zest_format_eac_r11_snorm_block: {
			*channels = 1;
			*block_width = 4;
			*block_height = 4;
			*bytes_per_block = 8;
			break;
		}
		case zest_format_eac_r11g11_unorm_block:
		case zest_format_eac_r11g11_snorm_block: {
			*channels = 2;
			*block_width = 4;
			*block_height = 4;
			*bytes_per_block = 16;
			break;
		}
		// ASTC compressed formats (all 16 bytes per block, varying block sizes)
		case zest_format_astc_4X4_unorm_block:
		case zest_format_astc_4X4_srgb_block: {
			*channels = 4;
			*block_width = 4;
			*block_height = 4;
			*bytes_per_block = 16;
			break;
		}
		case zest_format_astc_5X4_unorm_block:
		case zest_format_astc_5X4_srgb_block: {
			*channels = 4;
			*block_width = 5;
			*block_height = 4;
			*bytes_per_block = 16;
			break;
		}
		case zest_format_astc_5X5_unorm_block:
		case zest_format_astc_5X5_srgb_block: {
			*channels = 4;
			*block_width = 5;
			*block_height = 5;
			*bytes_per_block = 16;
			break;
		}
		case zest_format_astc_6X5_unorm_block:
		case zest_format_astc_6X5_srgb_block: {
			*channels = 4;
			*block_width = 6;
			*block_height = 5;
			*bytes_per_block = 16;
			break;
		}
		case zest_format_astc_6X6_unorm_block:
		case zest_format_astc_6X6_srgb_block: {
			*channels = 4;
			*block_width = 6;
			*block_height = 6;
			*bytes_per_block = 16;
			break;
		}
		case zest_format_astc_8X5_unorm_block:
		case zest_format_astc_8X5_srgb_block: {
			*channels = 4;
			*block_width = 8;
			*block_height = 5;
			*bytes_per_block = 16;
			break;
		}
		case zest_format_astc_8X6_unorm_block:
		case zest_format_astc_8X6_srgb_block: {
			*channels = 4;
			*block_width = 8;
			*block_height = 6;
			*bytes_per_block = 16;
			break;
		}
		case zest_format_astc_8X8_unorm_block:
		case zest_format_astc_8X8_srgb_block: {
			*channels = 4;
			*block_width = 8;
			*block_height = 8;
			*bytes_per_block = 16;
			break;
		}
		case zest_format_astc_10X5_unorm_block:
		case zest_format_astc_10X5_srgb_block: {
			*channels = 4;
			*block_width = 10;
			*block_height = 5;
			*bytes_per_block = 16;
			break;
		}
		case zest_format_astc_10X6_unorm_block:
		case zest_format_astc_10X6_srgb_block: {
			*channels = 4;
			*block_width = 10;
			*block_height = 6;
			*bytes_per_block = 16;
			break;
		}
		case zest_format_astc_10X8_unorm_block:
		case zest_format_astc_10X8_srgb_block: {
			*channels = 4;
			*block_width = 10;
			*block_height = 8;
			*bytes_per_block = 16;
			break;
		}
		case zest_format_astc_10X10_unorm_block:
		case zest_format_astc_10X10_srgb_block: {
			*channels = 4;
			*block_width = 10;
			*block_height = 10;
			*bytes_per_block = 16;
			break;
		}
		case zest_format_astc_12X10_unorm_block:
		case zest_format_astc_12X10_srgb_block: {
			*channels = 4;
			*block_width = 12;
			*block_height = 10;
			*bytes_per_block = 16;
			break;
		}
		case zest_format_astc_12X12_unorm_block:
		case zest_format_astc_12X12_srgb_block: {
			*channels = 4;
			*block_width = 12;
			*block_height = 12;
			*bytes_per_block = 16;
			break;
		}
		default:
			break;
    }
}

zest_image_info_t zest_CreateImageInfo(zest_uint width, zest_uint height) {
    zest_image_info_t info = {
        {width, height, 1},
        1,
        1,
        zest_format_r8g8b8a8_unorm,
        0,
        zest_sample_count_1_bit,
        0
    };
    return info;
}

zest_image_view_type zest__get_image_view_type(zest_image image) {
    zest_image_view_type view_type;
    if (image->info.extent.depth > 1) {
        view_type = zest_image_view_type_3d;
    } else if (ZEST__FLAGGED(image->info.flags, zest_image_flag_cubemap)) {
        view_type = (image->info.layer_count > 6) ? zest_image_view_type_cube_array : zest_image_view_type_cube;
    } else {
        view_type = (image->info.layer_count > 1) ? zest_image_view_type_2d_array : zest_image_view_type_2d;
    }
    return view_type;
}

zest_image_handle zest_CreateImage(zest_device device, zest_image_info_t *create_info) {
	zest_image_handle image_handle = zest__create_image(device, create_info);
	if (image_handle.value) {
		zest__activate_resource(image_handle.store, image_handle.value);
	}
    return image_handle;
} 

zest_image_handle zest_CreateImageWithPixels(zest_device device, void *pixels, zest_size size, zest_image_info_t *create_info) {
	int channels, bytes_per_pixel, block_width, block_height, bytes_per_block;
	zest_GetFormatPixelData(create_info->format, &channels, &bytes_per_pixel, &block_width, &block_height, &bytes_per_block);
	zest_size blocks_x = (create_info->extent.width + block_width - 1) / block_width;
	zest_size blocks_y = (create_info->extent.height + block_height - 1) / block_height;
	zest_size expected_size = blocks_x * blocks_y * bytes_per_block;
	ZEST_ASSERT(size == expected_size, "Size of pixels memory does not match the image info passed in to the function. Make sure you choose the correct format and width/height of the image.");
	zest_image_handle image_handle = zest__create_image(device, create_info);
	zest_image image = zest__get_image_unsafe(image_handle);

	zest_CopyBitmapToImage(device, pixels, size, image, create_info->extent.width, create_info->extent.height);

	zest__activate_resource(image_handle.store, image_handle.value);
	return image_handle;
}

zest_image zest_GetImage(zest_image_handle handle) {
	zest_image image = (zest_image)zest__get_store_resource_checked(handle.store, handle.value);
	return image;
}

void zest_FreeImage(zest_image_handle handle) {
	if (!handle.value) return;
    zest_image image = (zest_image)zest__get_store_resource_checked(handle.store, handle.value);
	zest_device device = (zest_device)handle.store->origin;
	zest_uint index = device->frame_counter % ZEST_MAX_FIF;
    zest_vec_push(device->allocator, device->deferred_resource_freeing_list.resources[index], image);
}

void zest_FreeImageNow(zest_image_handle handle) {
	if (!handle.value) return;
    zest_image image = (zest_image)zest__get_store_resource_checked(handle.store, handle.value);
	zest_device device = (zest_device)handle.store->origin;
	zest__free_handle(device->allocator, image);
}

void zest_FreeImageView(zest_image_view_handle handle) {
	if (!handle.value) return;
    zest_image_view view = *(zest_image_view*)zest__get_store_resource_checked(handle.store, handle.value);
	zest_device device = (zest_device)handle.store->origin;
	zest_uint index = device->frame_counter % ZEST_MAX_FIF;
    zest_vec_push(device->allocator, device->deferred_resource_freeing_list.resources[index], view);
}

void zest_FreeImageViewNow(zest_image_view_handle handle) {
	if (!handle.value) return;
    zest_image_view view = *(zest_image_view*)zest__get_store_resource_checked(handle.store, handle.value);
	zest_device device = (zest_device)handle.store->origin;
	zest__free_handle(device->allocator, view);
}

void zest_FreeImageViewArray(zest_image_view_array_handle handle) {
	if (!handle.value) return;
    zest_image_view_array view = *(zest_image_view_array*)zest__get_store_resource_checked(handle.store, handle.value);
	zest_device device = (zest_device)handle.store->origin;
	zest_uint index = device->frame_counter % ZEST_MAX_FIF;
    zest_vec_push(device->allocator, device->deferred_resource_freeing_list.resources[index], view);
}

void zest_FreeImageViewArrayNow(zest_image_view_array_handle handle) {
	if (!handle.value) return;
    zest_image_view_array view = *(zest_image_view_array*)zest__get_store_resource_checked(handle.store, handle.value);
	zest_device device = (zest_device)handle.store->origin;
	zest__free_handle(device->allocator, view);
}

zest_image_view_create_info_t zest_CreateViewImageInfo(zest_image image) {
	ZEST_ASSERT_HANDLE(image);	//Not a valid image handle
    zest_image_view_type view_type = zest__get_image_view_type(image);
    zest_image_view_create_info_t info = {
        view_type,
        0,
        image->info.mip_levels,
        0,
        image->info.layer_count
    };
    return info;
}

zest_image_view_handle zest_CreateImageView(zest_device device, zest_image image, zest_image_view_create_info_t *create_info) {
	ZEST_ASSERT_HANDLE(image);	//Not a valid image handle
	zest_resource_store_t *view_store = &device->resource_stores[zest_handle_type_views];
    zest_image_view_handle view_handle = ZEST_STRUCT_LITERAL(zest_image_view_handle, zest__add_store_resource(view_store) );
	view_handle.store = view_store;
    zest_image_view *view = (zest_image_view*)zest__get_store_resource_unsafe(view_store, view_handle.value);
    *view = device->platform->create_image_view(device, image, create_info->view_type, create_info->level_count, 
														 create_info->base_mip_level, create_info->base_array_layer,
														 create_info->layer_count, 0);
    (*view)->magic = zest_INIT_MAGIC(zest_struct_type_view);
    (*view)->handle = view_handle;
	zest__activate_resource(view_handle.store, view_handle.value);
    return view_handle;
}

zest_image_view_array_handle zest_CreateImageViewsPerMip(zest_device device, zest_image image) {
	ZEST_ASSERT_HANDLE(image);	//Not a valid image handle
	zest_resource_store_t *view_store = &device->resource_stores[zest_handle_type_view_arrays];
    zest_image_view_array_handle view_handle = ZEST_STRUCT_LITERAL(zest_image_view_array_handle, zest__add_store_resource(view_store), view_store );
	view_handle.store = view_store;
    zest_image_view_array *view = (zest_image_view_array*)zest__get_store_resource_unsafe(view_store, view_handle.value);
    zest_image_view_type view_type = zest__get_image_view_type(image);
    *view = device->platform->create_image_views_per_mip(device, image, view_type, 0, image->info.layer_count, 0);
    (*view)->magic = zest_INIT_MAGIC(zest_struct_type_view_array);
    (*view)->handle = view_handle;
	zest__activate_resource(view_handle.store, view_handle.value);
    return view_handle;
}

zest_image_view zest_GetImageView(zest_image_view_handle handle) {
	zest_image_view image_view = *(zest_image_view*)zest__get_store_resource_checked(handle.store, handle.value);
	return image_view;
}

zest_image_view_array zest_GetImageViewArray(zest_image_view_array_handle handle) {
	zest_image_view_array image_view = *(zest_image_view_array*)zest__get_store_resource_checked(handle.store, handle.value);
	return image_view;
}

const zest_image_info_t *zest_ImageInfo(zest_image image) {
	ZEST_ASSERT_HANDLE(image);	//Not a valid image handle
    return &image->info;
}

zest_uint zest_ImageDescriptorIndex(zest_image image, zest_binding_number_type binding_number) {
	ZEST_ASSERT_HANDLE(image);	//Not a valid image handle
    ZEST_ASSERT(binding_number < zest_max_global_binding_number);   //Invalid binding number
    return image->bindless_index[binding_number];
}

int zest_ImageRawLayout(zest_image image) {
	ZEST_ASSERT_HANDLE(image);	//Not a valid image handle
	zest_device device = (zest_device)image->handle.store->origin;
    return device->platform->get_image_raw_layout(image);
}

zest_extent2d_t zest_RegionDimensions(zest_atlas_region_t *region) {
    return ZEST_STRUCT_LITERAL(zest_extent2d_t, region->width, region->height );
}

zest_uint zest_RegionLayerIndex(zest_atlas_region_t *region) {
    return region->layer_index;
}

zest_vec4 zest_RegionUV(zest_atlas_region_t *region) {
    return region->uv;
}

void zest_BindAtlasRegionToImage(zest_atlas_region_t *region, zest_uint sampler_index, zest_image image, zest_binding_number_type binding_number) {
	ZEST_ASSERT_HANDLE(image);	//Not a valid image handle
	region->image_index = zest_ImageDescriptorIndex(image, binding_number);
	region->sampler_index = sampler_index;
	region->binding_number = binding_number;
}
//-- End Texture and Image Functions

void zest_UploadInstanceLayerData(const zest_command_list command_list, void *user_data) {
	zest_layer layer = (zest_layer)user_data;

    ZEST_MAYBE_REPORT(command_list->device, !ZEST_VALID_HANDLE(layer, zest_struct_type_layer), zest_report_invalid_layer, "Error in [%s] The zest_UploadInstanceLayerData was called with invalid layer data. Pass in a valid layer or array of layers to the zest_SetPassTask function in the frame graph.", zest__frame_graph_builder->frame_graph->name);

    if (ZEST_VALID_HANDLE(layer, zest_struct_type_layer)) {  //You must pass in the zest_layer in the user data

        if (!layer->dirty[layer->fif]) {
            return;
        }

        zest_buffer staging_buffer = layer->memory_refs[layer->fif].staging_instance_data;
        zest_buffer device_buffer = ZEST__FLAGGED(layer->flags, zest_layer_flag_manual_fif) ? layer->memory_refs[layer->fif].device_vertex_data : layer->vertex_buffer_node->storage_buffer;

        zest_buffer_uploader_t instance_upload = { 0, staging_buffer, device_buffer, 0 };

        layer->dirty[layer->fif] = 0;

		zest_size memory_in_use = layer->memory_refs[layer->fif].vertex_memory_in_use;
        if (memory_in_use && device_buffer) {
            zest_AddCopyCommand(command_list->context, &instance_upload, staging_buffer, device_buffer, memory_in_use);
        } else {
            return;
        }

        zest_uint vertex_size = zest_vec_size(instance_upload.buffer_copies);

        zest_cmd_UploadBuffer(command_list, &instance_upload);
    }
}

//-- Draw Layers
//-- internal
zest_layer_instruction_t zest__layer_instruction() {
    zest_layer_instruction_t instruction = ZEST__ZERO_INIT(zest_layer_instruction_t);
    instruction.pipeline_template = ZEST_NULL;
    return instruction;
}

void zest__reset_instance_layer_drawing(zest_layer layer) {
    zest_vec_clear(layer->draw_instructions[layer->fif]);
    layer->current_instruction = zest__layer_instruction();
    layer->memory_refs[layer->fif].instance_count = 0;
    layer->memory_refs[layer->fif].instance_ptr = zest_BufferData(layer->memory_refs[layer->fif].staging_instance_data);
    layer->memory_refs[layer->fif].vertex_memory_in_use = 0;
}

void zest_ResetInstanceLayerDrawing(zest_layer layer) {
	ZEST_ASSERT_HANDLE(layer); //ERROR: Not a valid layer pointer
    zest_vec_clear(layer->draw_instructions[layer->fif]);
    layer->current_instruction = zest__layer_instruction();
    layer->memory_refs[layer->fif].instance_count = 0;
    layer->memory_refs[layer->fif].instance_ptr = zest_BufferData(layer->memory_refs[layer->fif].staging_instance_data);
    layer->memory_refs[layer->fif].vertex_memory_in_use = 0;
}

zest_uint zest_GetInstanceLayerCount(zest_layer layer) {
	ZEST_ASSERT_HANDLE(layer); //ERROR: Not a valid layer pointer
    return layer->memory_refs[layer->fif].instance_count;
}

zest_bool zest__grow_instance_buffer(zest_layer layer, zest_size type_size, zest_size minimum_size) {
    zest_bool grown = 0;
    if (ZEST__FLAGGED(layer->flags, zest_layer_flag_manual_fif)) {
		grown = zest_GrowBuffer(&layer->memory_refs[layer->fif].staging_instance_data, type_size, minimum_size);
        zest_GrowBuffer(&layer->memory_refs[layer->fif].device_vertex_data, type_size, layer->memory_refs[layer->fif].staging_instance_data->size);
		layer->memory_refs[layer->fif].staging_instance_data = layer->memory_refs[layer->fif].staging_instance_data;
		zest_uint array_index = layer->memory_refs[layer->fif].descriptor_array_index;
        if (ZEST__FLAGGED(layer->flags, zest_layer_flag_using_global_bindless_layout) && array_index != ZEST_INVALID) {
            zest_buffer instance_buffer = layer->memory_refs[layer->fif].device_vertex_data;
			zest_context context = (zest_context)layer->handle.store->origin;
			context->device->platform->update_bindless_storage_buffer_descriptor(layer->context->device, zest_storage_buffer_binding, array_index, instance_buffer, layer->bindless_set);
        }
    } else {
		grown = zest_GrowBuffer(&layer->memory_refs[layer->fif].staging_instance_data, type_size, minimum_size);
		layer->memory_refs[layer->fif].staging_instance_data = layer->memory_refs[layer->fif].staging_instance_data;
    }
    return grown;
}

void zest__cleanup_layer(zest_layer layer) {
	zest_context context = (zest_context)layer->handle.store->origin;
	zest_ForEachFrameInFlight(fif) {
		zest_FreeBuffer(layer->memory_refs[fif].device_vertex_data);
		zest_FreeBuffer(layer->memory_refs[fif].staging_vertex_data);
		zest_FreeBuffer(layer->memory_refs[fif].staging_index_data);
		zest_vec_free(context->device->allocator, layer->draw_instructions[fif]);
	}
	zest_FreeBuffer(layer->vertex_data);
	zest_FreeBuffer(layer->index_data);
	zest_vec_free(context->allocator, layer->mesh_offsets);
    zest__remove_store_resource(layer->handle.store, layer->handle.value);
}

void zest__reset_mesh_layer_drawing(zest_layer layer) {
    zest_vec_clear(layer->draw_instructions[layer->fif]);
    layer->memory_refs[layer->fif].vertex_memory_in_use = 0;
    layer->memory_refs[layer->fif].index_memory_in_use = 0;
    layer->current_instruction = zest__layer_instruction();
    layer->memory_refs[layer->fif].index_count = 0;
    layer->memory_refs[layer->fif].vertex_count = 0;
    layer->memory_refs[layer->fif].vertex_ptr = zest_BufferData(layer->memory_refs[layer->fif].staging_vertex_data);
    layer->memory_refs[layer->fif].index_ptr = (zest_uint*)zest_BufferData(layer->memory_refs[layer->fif].staging_index_data);
}

void zest__start_instance_instructions(zest_layer layer) {
    layer->current_instruction.start_index = layer->memory_refs[layer->fif].instance_count ? layer->memory_refs[layer->fif].instance_count : 0;
}

void zest__set_layer_push_constants(zest_layer layer, void *push_constants, zest_size size) {
    ZEST_ASSERT(size <= 128);   //Push constant size must not exceed 128 bytes
    memcpy(layer->current_instruction.push_constant, push_constants, size);
}

void zest_StartInstanceInstructions(zest_layer layer) {
	ZEST_ASSERT_HANDLE(layer); //ERROR: Not a valid layer pointer
    layer->current_instruction.start_index = layer->memory_refs[layer->fif].instance_count ? layer->memory_refs[layer->fif].instance_count : 0;
}

void zest_ResetLayer(zest_layer layer) {
	ZEST_ASSERT_HANDLE(layer); //ERROR: Not a valid layer pointer
    layer->fif = (layer->fif + 1) % ZEST_MAX_FIF;
}

void zest_ResetInstanceLayer(zest_layer layer) {
	ZEST_ASSERT_HANDLE(layer); //ERROR: Not a valid layer pointer
    ZEST_ASSERT(ZEST__FLAGGED(layer->flags, zest_layer_flag_manual_fif));   //You must have created the layer with zest_CreateFIFInstanceLayer
	//if you want to manually reset the layer
    layer->prev_fif = layer->fif;
    layer->fif = (layer->fif + 1) % ZEST_MAX_FIF;
    layer->dirty[layer->fif] = 1;
    zest__reset_instance_layer_drawing(layer);
}

void zest__start_mesh_instructions(zest_layer layer) {
    layer->current_instruction.start_index = layer->memory_refs[layer->fif].index_count ? layer->memory_refs[layer->fif].index_count : 0;
}

void zest__end_instance_instructions(zest_layer layer) {
	zest_context context = (zest_context)layer->handle.store->origin;
    if (ZEST__NOT_FLAGGED(layer->flags, zest_layer_flag_manual_fif)) {
		if (layer->fif != context->current_fif) {
			zest__reset_instance_layer_drawing(layer);
		}
        layer->fif = context->current_fif;
    }
    if (layer->current_instruction.total_instances) {
        layer->last_draw_mode = zest_draw_mode_none;
        zest_vec_push(context->device->allocator, layer->draw_instructions[layer->fif], layer->current_instruction);
        layer->current_instruction.total_instances = 0;
        layer->current_instruction.start_index = 0;
    }
    else if (layer->current_instruction.draw_mode == zest_draw_mode_viewport) {
        zest_vec_push(context->device->allocator, layer->draw_instructions[layer->fif], layer->current_instruction);
        layer->last_draw_mode = zest_draw_mode_none;
    }
    layer->memory_refs[layer->fif].vertex_memory_in_use = layer->memory_refs[layer->fif].instance_count * layer->instance_struct_size;
}

void zest_EndInstanceInstructions(zest_layer layer) {
	ZEST_ASSERT_HANDLE(layer); //ERROR: Not a valid layer pointer
	zest__end_instance_instructions(layer);
}

zest_bool zest_MaybeEndInstanceInstructions(zest_layer layer) {
	ZEST_ASSERT_HANDLE(layer); //ERROR: Not a valid layer pointer
	zest_context context = layer->context;
    if (layer->current_instruction.total_instances) {
        layer->last_draw_mode = zest_draw_mode_none;
        zest_vec_push(context->device->allocator, layer->draw_instructions[layer->fif], layer->current_instruction);
        layer->current_instruction.total_instances = 0;
        layer->current_instruction.start_index = 0;
    }
    else if (layer->current_instruction.draw_mode == zest_draw_mode_viewport) {
        zest_vec_push(context->device->allocator, layer->draw_instructions[layer->fif], layer->current_instruction);
        layer->last_draw_mode = zest_draw_mode_none;
    }
    return 1;
}

zest_size zest_GetLayerInstanceSize(zest_layer layer) {
	ZEST_ASSERT_HANDLE(layer); //ERROR: Not a valid layer pointer
	return layer->memory_refs[layer->fif].instance_count * layer->instance_struct_size;
}

zest_size zest_GetLayerIndexMemoryInUse(zest_layer layer) {
	ZEST_ASSERT_HANDLE(layer); //ERROR: Not a valid layer pointer
	return layer->memory_refs[layer->fif].index_memory_in_use;
}

zest_size zest_GetLayerVertexMemoryInUse(zest_layer layer) {
	ZEST_ASSERT_HANDLE(layer); //ERROR: Not a valid layer pointer
	return layer->memory_refs[layer->fif].vertex_memory_in_use;
}

void zest__end_mesh_instructions(zest_layer layer) {
	zest_context context = (zest_context)layer->handle.store->origin;
    if (layer->current_instruction.total_indexes) {
        layer->last_draw_mode = zest_draw_mode_none;
        zest_vec_push(context->device->allocator, layer->draw_instructions[layer->fif], layer->current_instruction);

        layer->memory_refs[layer->fif].index_memory_in_use += layer->current_instruction.total_indexes * sizeof(zest_uint);
        layer->memory_refs[layer->fif].vertex_memory_in_use += layer->current_instruction.total_indexes * layer->vertex_struct_size;
        layer->current_instruction.total_indexes = 0;
        layer->current_instruction.start_index = 0;
    }
    else if (layer->current_instruction.draw_mode == zest_draw_mode_viewport) {
        zest_vec_push(context->device->allocator, layer->draw_instructions[layer->fif], layer->current_instruction);

        layer->last_draw_mode = zest_draw_mode_none;
    }
}

void zest__update_instance_layer_resolution(zest_layer layer) {
	zest_context context = (zest_context)layer->handle.store->origin;
    layer->viewport.width = (float)zest_GetSwapChainExtent(context).width;
    layer->viewport.height = (float)zest_GetSwapChainExtent(context).height;
    layer->screen_scale.x = layer->viewport.width / layer->layer_size.x;
    layer->screen_scale.y = layer->viewport.height / layer->layer_size.y;
    layer->scissor.extent.width = (zest_uint)layer->viewport.width;
    layer->scissor.extent.height = (zest_uint)layer->viewport.height;
}

//Start general instance layer functionality -----
void *zest_NextInstance(zest_layer layer) {
	ZEST_ASSERT_HANDLE(layer); //ERROR: Not a valid layer pointer
    zest_byte* instance_ptr = (zest_byte*)layer->memory_refs[layer->fif].instance_ptr + layer->instance_struct_size;
    //Make sure we're not trying to write outside of the buffer range
    ZEST_ASSERT(instance_ptr >= (zest_byte*)zest_BufferData(layer->memory_refs[layer->fif].staging_instance_data) && instance_ptr <= (zest_byte*)zest_BufferDataEnd(layer->memory_refs[layer->fif].staging_instance_data));
    if (instance_ptr == zest_BufferDataEnd(layer->memory_refs[layer->fif].staging_instance_data)) {
        if (zest__grow_instance_buffer(layer, layer->instance_struct_size, 0)) {
            layer->memory_refs[layer->fif].instance_count++;
            instance_ptr = (zest_byte*)zest_BufferData(layer->memory_refs[layer->fif].staging_instance_data);
            instance_ptr += layer->memory_refs[layer->fif].instance_count * layer->instance_struct_size;
			layer->current_instruction.total_instances++;
        }
        else {
            instance_ptr = instance_ptr - layer->instance_struct_size;
        }
    }
    else {
		layer->current_instruction.total_instances++;
        layer->memory_refs[layer->fif].instance_count++;
    }
    layer->memory_refs[layer->fif].instance_ptr = instance_ptr;
	return (zest_byte *)instance_ptr - layer->instance_struct_size;
}

zest_draw_buffer_result zest_DrawInstanceBuffer(zest_layer layer, void *src, zest_uint amount) {
	ZEST_ASSERT_HANDLE(layer); //ERROR: Not a valid layer pointer
	zest_context context = layer->context;
    if (!amount) return zest_draw_buffer_result_ok;
    zest_draw_buffer_result result = zest_draw_buffer_result_ok;
    zest_size size_in_bytes_to_copy = amount * layer->instance_struct_size;
    zest_byte *instance_ptr = (zest_byte *)layer->memory_refs[layer->fif].instance_ptr;
    int fif = context->current_fif;
    ptrdiff_t diff = (zest_byte *)zest_BufferDataEnd(layer->memory_refs[layer->fif].staging_instance_data) - (instance_ptr + size_in_bytes_to_copy);
    if (diff <= 0) {
        if (zest__grow_instance_buffer(layer, layer->instance_struct_size, (layer->memory_refs[layer->fif].instance_count * layer->instance_struct_size) + size_in_bytes_to_copy)) {
            instance_ptr = (zest_byte *)zest_BufferData(layer->memory_refs[layer->fif].staging_instance_data);
            instance_ptr += layer->memory_refs[layer->fif].instance_count * layer->instance_struct_size;
            diff = (zest_byte *)zest_BufferData(layer->memory_refs[layer->fif].staging_instance_data) - instance_ptr;
            result = zest_draw_buffer_result_buffer_grew;
        }
        else {
            return zest_draw_buffer_result_failed_to_grow;
        }
    }
    memcpy(instance_ptr, src, size_in_bytes_to_copy);
    layer->memory_refs[layer->fif].instance_count += amount;
    layer->current_instruction.total_instances += amount;
    instance_ptr += size_in_bytes_to_copy;
    layer->memory_refs[layer->fif].instance_ptr = instance_ptr;
    return result;
}

void zest_DrawInstanceInstruction(zest_layer layer, zest_uint amount) {
	ZEST_ASSERT_HANDLE(layer); //ERROR: Not a valid layer pointer
    layer->memory_refs[layer->fif].instance_count += amount;
    layer->current_instruction.total_instances += amount;
    zest_size size_in_bytes_to_draw = amount * layer->instance_struct_size;
    zest_byte* instance_ptr = (zest_byte*)layer->memory_refs[layer->fif].instance_ptr;
    instance_ptr += size_in_bytes_to_draw;
    layer->memory_refs[layer->fif].instance_ptr = instance_ptr;
}

// End general instance layer functionality -----

//-- Draw Layers API
zest_layer_handle zest__new_layer(zest_context context, zest_layer *out_layer) {
	zest_resource_store_t *store = &context->resource_stores[zest_handle_type_layers];
    zest_layer_handle handle = ZEST_STRUCT_LITERAL(zest_layer_handle, zest__add_store_resource(store), store );
	handle.store = store;
    zest_layer layer = (zest_layer)zest__get_store_resource_unsafe(store, handle.value);
    *layer = ZEST__ZERO_INIT(zest_layer_t);
    layer->magic = zest_INIT_MAGIC(zest_struct_type_layer);
    layer->handle = handle;
	layer->context = context;
    *out_layer = layer;
    return handle;
}

void zest_FreeLayer(zest_layer_handle layer_handle) {
    zest_layer layer = (zest_layer)zest__get_store_resource_checked(layer_handle.store, layer_handle.value);
	zest_context context = layer->context;
    zest_vec_push(context->allocator, context->deferred_resource_freeing_list.resources[context->current_fif], layer);
}

void zest_SetLayerViewPort(zest_layer layer, int x, int y, zest_uint scissor_width, zest_uint scissor_height, float viewport_width, float viewport_height) {
	ZEST_ASSERT_HANDLE(layer); //ERROR: Not a valid layer pointer
    layer->scissor = zest_CreateRect2D(scissor_width, scissor_height, x, y);
    layer->viewport = zest_CreateViewport((float)x, (float)y, viewport_width, viewport_height, 0.f, 1.f);
}

void zest_SetLayerScissor(zest_layer layer, int offset_x, int offset_y, zest_uint scissor_width, zest_uint scissor_height) {
	ZEST_ASSERT_HANDLE(layer); //ERROR: Not a valid layer pointer
    layer->scissor = zest_CreateRect2D(scissor_width, scissor_height, offset_x, offset_y);
}

void zest_SetLayerSizeToSwapchain(zest_layer layer) {
	ZEST_ASSERT_HANDLE(layer); //ERROR: Not a valid layer pointer
	zest_context context = layer->context;
	zest_swapchain swapchain = context->swapchain;
    ZEST_ASSERT_HANDLE(swapchain);	//Not a valid swapchain handle!
    layer->scissor = zest_CreateRect2D((zest_uint)swapchain->size.width, (zest_uint)swapchain->size.height, 0, 0);
    layer->viewport = zest_CreateViewport(0.f, 0.f, (float)swapchain->size.width, (float)swapchain->size.height, 0.f, 1.f);

}

void zest_SetLayerSize(zest_layer layer, float width, float height) {
	ZEST_ASSERT_HANDLE(layer); //ERROR: Not a valid layer pointer
    layer->layer_size.x = width;
    layer->layer_size.y = height;
}

void zest_SetLayerColor(zest_layer layer, zest_byte red, zest_byte green, zest_byte blue, zest_byte alpha) {
	ZEST_ASSERT_HANDLE(layer); //ERROR: Not a valid layer pointer
    layer->current_color = zest_ColorSet(red, green, blue, alpha);
}

void zest_SetLayerColorf(zest_layer layer, float red, float green, float blue, float alpha) {
	ZEST_ASSERT_HANDLE(layer); //ERROR: Not a valid layer pointer
    layer->current_color = zest_ColorSet((zest_byte)(red * 255.f), (zest_byte)(green * 255.f), (zest_byte)(blue * 255.f), (zest_byte)(alpha * 255.f));
}

void zest_SetLayerIntensity(zest_layer layer, float value) {
	ZEST_ASSERT_HANDLE(layer); //ERROR: Not a valid layer pointer
    layer->intensity = value;
}

void zest_SetLayerChanged(zest_layer layer) {
	ZEST_ASSERT_HANDLE(layer); //ERROR: Not a valid layer pointer
    zest_ForEachFrameInFlight(i) {
        layer->dirty[i] = 1;
    }
}

zest_bool zest_LayerHasChanged(zest_layer layer) {
	ZEST_ASSERT_HANDLE(layer); //ERROR: Not a valid layer pointer
    return layer->dirty[layer->fif];
}

void zest_SetLayerUserData(zest_layer layer, void *data) {
	ZEST_ASSERT_HANDLE(layer); //ERROR: Not a valid layer pointer
    layer->user_data = data;
}

void *zest_GetLayerUserData(zest_layer layer, void *data) {
	ZEST_ASSERT_HANDLE(layer); //ERROR: Not a valid layer pointer
    return layer->user_data;
}

void zest_UploadLayerStagingData(zest_layer layer, const zest_command_list command_list) {
	zest_context context = layer->context;

    ZEST_MAYBE_REPORT(context->device, !ZEST_VALID_HANDLE(layer, zest_struct_type_layer), zest_report_invalid_layer, "Error in [%s] The zest_UploadLayerStagingData was called with invalid layer data. Pass in a valid layer or array of layers to the zest_SetPassTask function in the frame graph.", zest__frame_graph_builder->frame_graph->name);

    if (ZEST_VALID_HANDLE(layer, zest_struct_type_layer)) {  //You must pass in the zest_layer in the user data

        if (!layer->dirty[layer->fif]) {
            return;
        }

        zest_buffer staging_buffer = layer->memory_refs[layer->fif].staging_instance_data;
        zest_buffer device_buffer = ZEST__FLAGGED(layer->flags, zest_layer_flag_manual_fif) ? layer->memory_refs[layer->fif].device_vertex_data : layer->vertex_buffer_node->storage_buffer;

		if (!device_buffer) return;
		if (!staging_buffer) return;

        zest_buffer_uploader_t instance_upload = { 0, staging_buffer, device_buffer, 0 };

        layer->dirty[layer->fif] = 0;

		if (layer->memory_refs[layer->fif].vertex_memory_in_use) {
			zest_AddCopyCommand(context, &instance_upload, staging_buffer, device_buffer, layer->memory_refs[layer->fif].vertex_memory_in_use);
        } else {
            return;
        }

        zest_uint vertex_size = zest_vec_size(instance_upload.buffer_copies);

        zest_cmd_UploadBuffer(command_list, &instance_upload);
    }
}

zest_buffer zest_GetLayerResourceBuffer(zest_layer layer) {
	ZEST_ASSERT_HANDLE(layer); //ERROR: Not a valid layer pointer
	zest_context context = layer->context;
    if (ZEST__FLAGGED(layer->flags, zest_layer_flag_manual_fif)) {
        return layer->memory_refs[layer->fif].device_vertex_data;
    } else if(ZEST_VALID_HANDLE(layer->vertex_buffer_node, zest_struct_type_resource_node)) {
		//Make sure you add it to the frame graph
        ZEST_MAYBE_REPORT(context->device, layer->vertex_buffer_node->reference_count == 0, zest_report_resource_culled, "zest_GetLayerResourceBuffer was called for resourcee [%s] that has been culled. Passes will be culled (and therefore their transient resources will not be created) if they have no outputs and therefore deemed as unnecessary and also bear in mind that passes in the chain may also be culled.", layer->vertex_buffer_node->name);   
        return layer->vertex_buffer_node->storage_buffer;
    } else {
        ZEST_REPORT(context->device, zest_report_resource_culled, "zest_GetLayerResourceBuffer was called for layer [%s] but the layer doesn't have a resource node. Make sure that you add the layer to the frame graph with zest_AddInstanceLayerBufferResource", layer->name);   
    }
    return NULL;
}

zest_buffer zest_GetLayerStagingVertexBuffer(zest_layer layer) {
	ZEST_ASSERT_HANDLE(layer); //ERROR: Not a valid layer pointer
    return layer->memory_refs[layer->fif].staging_instance_data;
}

zest_buffer zest_GetLayerStagingIndexBuffer(zest_layer layer) {
	ZEST_ASSERT_HANDLE(layer); //ERROR: Not a valid layer pointer
    return layer->memory_refs[layer->fif].staging_index_data;
}

const zest_mesh_offset_data_t *zest_GetLayerMeshOffsets(zest_layer layer, zest_uint mesh_index) {
	ZEST_ASSERT_HANDLE(layer); //ERROR: Not a valid layer pointer
	ZEST_ASSERT(mesh_index < zest_vec_size(layer->mesh_offsets), "The mesh index is out of bounds when trying to fetch mesh offsets from the layer.");
	return &layer->mesh_offsets[mesh_index];
}

zest_buffer zest_GetLayerVertexBuffer(zest_layer layer) {
	ZEST_ASSERT_HANDLE(layer); //ERROR: Not a valid layer pointer
    return layer->memory_refs[layer->fif].device_vertex_data;
}

zest_uint zest_GetLayerInstructionCount(zest_layer layer) {
	ZEST_ASSERT_HANDLE(layer); //ERROR: Not a valid layer pointer
	return zest_vec_size(layer->draw_instructions[layer->fif]);
}

zest_scissor_rect_t zest_GetLayerScissor(zest_layer layer) {
	ZEST_ASSERT_HANDLE(layer); //ERROR: Not a valid layer pointer
	return layer->scissor;
}
zest_viewport_t zest_GetLayerViewport(zest_layer layer) {
	ZEST_ASSERT_HANDLE(layer); //ERROR: Not a valid layer pointer
	return layer->viewport;
}

const zest_layer_instruction_t *zest_GetLayerInstruction(zest_layer layer, zest_uint index) {
	ZEST_ASSERT_HANDLE(layer); //ERROR: Not a valid layer pointer
	ZEST_ASSERT(index < zest_vec_size(layer->draw_instructions[layer->fif]));
	return &layer->draw_instructions[layer->fif][index];
}

zest_layer_instruction_t *zest_NextLayerInstruction(zest_layer layer) {
	ZEST_ASSERT_HANDLE(layer); //ERROR: Not a valid layer pointer
	if (layer->instruction_index < zest_vec_size(layer->draw_instructions[layer->fif])) {
		zest_layer_instruction_t *instruction = &layer->draw_instructions[layer->fif][layer->instruction_index];
		layer->instruction_index++;
		return instruction;
	}
	layer->instruction_index = 0;
	return NULL;
}

void zest_DrawInstanceLayer(const zest_command_list command_list, void *user_data) {
    zest_layer layer = (zest_layer)user_data;

    ZEST_MAYBE_REPORT(command_list->device, !ZEST_VALID_HANDLE(layer, zest_struct_type_layer), zest_report_invalid_layer, "Error in [%s] The zest_DrawInstanceLayer was called with invalid layer data. Pass in a valid layer or array of layers to the zest_SetPassTask function in the frame graph.", zest__frame_graph_builder->frame_graph->name);

    if (!layer->vertex_buffer_node) return; //It could be that the frame graph culled the pass because it was unreferenced or disabled
    if (!layer->vertex_buffer_node->storage_buffer) return;
	zest_buffer device_buffer = layer->vertex_buffer_node->storage_buffer;
	zest_cmd_BindVertexBuffer(command_list, 0, 1, device_buffer);

	zest_pipeline current_pipeline = 0;

    zest_bool has_instruction_view_port = ZEST_FALSE;
    zest_vec_foreach(i, layer->draw_instructions[layer->fif]) {
        zest_layer_instruction_t* current = &layer->draw_instructions[layer->fif][i];

        if (current->draw_mode == zest_draw_mode_viewport) {
			zest_cmd_ViewPort(command_list, &current->viewport);
			zest_cmd_Scissor(command_list, &current->scissor);
            has_instruction_view_port = ZEST_TRUE;
            continue;
        } else if(!has_instruction_view_port) {
			zest_cmd_ViewPort(command_list, &layer->viewport);
			zest_cmd_Scissor(command_list, &layer->scissor);
        }

        zest_pipeline pipeline = zest_GetPipeline(current->pipeline_template, command_list);
        if (pipeline && pipeline != current_pipeline) {
			zest_cmd_BindPipeline(command_list, pipeline);
			current_pipeline = pipeline;
        } else if(!pipeline) {
            continue;
        }

		zest_cmd_SendPushConstants(command_list, current->push_constant, ZEST_MAX_PUSH_SIZE);

		zest_cmd_Draw(command_list, 6, current->total_instances, 0, current->start_index);
    }
}
//-- End Draw Layers

//-- Start Instance Drawing API
zest_layer_handle zest_CreateInstanceLayer(zest_context context, const char* name, zest_size type_size, zest_uint max_instances) {
    zest_layer_handle layer_handle = zest__create_instance_layer(context, name, type_size, max_instances);
    return layer_handle;
}

ZEST_API zest_layer zest_GetLayer(zest_layer_handle layer_handle) {
    zest_layer layer = (zest_layer)zest__get_store_resource_checked(layer_handle.store, layer_handle.value);
	return layer;
}

zest_layer_handle zest_CreateFIFInstanceLayer(zest_context context, const char* name, zest_size type_size, zest_uint max_instances) {
    zest_layer_handle layer_handle = zest_CreateInstanceLayer(context, name, type_size, max_instances);
    zest_layer layer = (zest_layer)zest__get_store_resource_checked(layer_handle.store, layer_handle.value);
    zest_ForEachFrameInFlight(fif) {
		zest_buffer_info_t buffer_info = zest_CreateBufferInfo(zest_buffer_type_vertex_storage, zest_memory_usage_gpu_only);
		buffer_info.frame_in_flight = fif;
        layer->memory_refs[fif].device_vertex_data = zest_CreateBuffer(context->device, layer->memory_refs[fif].staging_instance_data->size, &buffer_info);
    }
    ZEST__FLAG(layer->flags, zest_layer_flag_manual_fif);
    return layer_handle;
}

void zest__initialise_instance_layer(zest_context context, zest_layer layer, zest_size type_size, zest_uint instance_pool_size) {
    layer->current_color.r = 255;
    layer->current_color.g = 255;
    layer->current_color.b = 255;
    layer->current_color.a = 255;
    layer->intensity = 1;
    layer->layer_size = zest_Vec2Set1(1.f);
    layer->instance_struct_size = type_size;

    zest_buffer_info_t staging_buffer_info = zest_CreateBufferInfo(zest_buffer_type_staging, zest_memory_usage_cpu_to_gpu);
    zest_ForEachFrameInFlight(fif) {
		layer->memory_refs[fif].staging_instance_data = zest_CreateBuffer(context->device, type_size * instance_pool_size, &staging_buffer_info);
		layer->memory_refs[fif].instance_ptr = zest_BufferData(layer->memory_refs[fif].staging_instance_data);
		layer->memory_refs[fif].staging_instance_data = layer->memory_refs[fif].staging_instance_data;
        layer->memory_refs[fif].instance_count = 0;
        zest_vec_clear(layer->draw_instructions[fif]);
        layer->memory_refs[fif].vertex_memory_in_use = 0;
        layer->memory_refs[fif].index_memory_in_use = 0;
		layer->memory_refs[fif].descriptor_array_index = ZEST_INVALID;
        layer->current_instruction = zest__layer_instruction();
        layer->memory_refs[fif].instance_count = 0;
        layer->memory_refs[fif].instance_ptr = zest_BufferData(layer->memory_refs[fif].staging_instance_data);
    }

    layer->scissor = zest_CreateRect2D(zest_ScreenWidth(context), zest_ScreenHeight(context), 0, 0);
    layer->viewport = zest_CreateViewport(0.f, 0.f, zest_ScreenWidthf(context), zest_ScreenHeightf(context), 0.f, 1.f);
}

//-- Start Instance Drawing API
void zest_StartInstanceDrawing(zest_layer layer, zest_pipeline_template pipeline) {
	ZEST_ASSERT_HANDLE(pipeline); 			//ERROR: Not a valid pipeline template pointer
	ZEST_ASSERT_HANDLE(layer); 				//ERROR: Not a valid layer pointer
	zest_context context = layer->context;
	zest__end_instance_instructions(layer);
	zest__start_instance_instructions(layer);
	layer->current_instruction.pipeline_template = pipeline;

	layer->current_instruction.draw_mode = zest_draw_mode_instance;
	layer->last_draw_mode = zest_draw_mode_instance;
}

void zest_SetLayerDrawingViewport(zest_layer layer, int x, int y, zest_uint scissor_width, zest_uint scissor_height, float viewport_width, float viewport_height) {
	ZEST_ASSERT_HANDLE(layer); 		//ERROR: Not a valid layer pointer
    zest__end_instance_instructions(layer);
    zest__start_instance_instructions(layer);
    layer->current_instruction.scissor = zest_CreateRect2D(scissor_width, scissor_height, x, y);
    layer->current_instruction.viewport = zest_CreateViewport((float)x, (float)y, viewport_width, viewport_height, 0.f, 1.f);
    layer->current_instruction.draw_mode = zest_draw_mode_viewport;
}

void zest_SetLayerPushConstants(zest_layer layer, void *push_constants, zest_size size) {
	ZEST_ASSERT_HANDLE(layer); 		//ERROR: Not a valid layer pointer
	zest__set_layer_push_constants(layer, push_constants, size);
}

void *zest_GetLayerPushConstants(zest_layer layer) {
	ZEST_ASSERT_HANDLE(layer); //ERROR: Not a valid layer pointer
	return (void*)layer->current_instruction.push_constant;
}

int zest_GetLayerFrameInFlight(zest_layer layer) {
	return (int)layer->fif;
}

//-- Start Mesh Drawing API
void zest__initialise_mesh_layer(zest_context context, zest_layer mesh_layer, zest_size vertex_struct_size, zest_size initial_vertex_capacity) {
    mesh_layer->current_color.r = 255;
    mesh_layer->current_color.g = 255;
    mesh_layer->current_color.b = 255;
    mesh_layer->current_color.a = 255;
    mesh_layer->intensity = 1;
    mesh_layer->layer_size = zest_Vec2Set1(1.f);
    mesh_layer->vertex_struct_size = vertex_struct_size;

    zest_buffer_info_t staging_buffer_info = zest_CreateBufferInfo(zest_buffer_type_staging, zest_memory_usage_cpu_to_gpu);

    zest_ForEachFrameInFlight(fif) {
        mesh_layer->memory_refs[fif].index_count = 0;
        mesh_layer->memory_refs[fif].index_position = 0;
        mesh_layer->memory_refs[fif].last_index = 0;
        mesh_layer->memory_refs[fif].vertex_count = 0;
		mesh_layer->memory_refs[fif].staging_vertex_data = zest_CreateBuffer(context->device, initial_vertex_capacity, &staging_buffer_info);
		mesh_layer->memory_refs[fif].staging_index_data = zest_CreateBuffer(context->device, initial_vertex_capacity, &staging_buffer_info);
		mesh_layer->memory_refs[fif].vertex_ptr = zest_BufferData(mesh_layer->memory_refs[fif].staging_vertex_data);
		mesh_layer->memory_refs[fif].index_ptr = (zest_uint*)zest_BufferData(mesh_layer->memory_refs[fif].staging_index_data);
		mesh_layer->memory_refs[fif].staging_vertex_data = mesh_layer->memory_refs[fif].staging_vertex_data;
		mesh_layer->memory_refs[fif].staging_index_data = mesh_layer->memory_refs[fif].staging_index_data;
    }

    mesh_layer->scissor = zest_CreateRect2D(zest_ScreenWidth(context), zest_ScreenHeight(context), 0, 0);
    mesh_layer->viewport = zest_CreateViewport(0.f, 0.f, zest_ScreenWidthf(context), zest_ScreenHeightf(context), 0.f, 1.f);
    zest_ForEachFrameInFlight(fif) {
        zest_vec_clear(mesh_layer->draw_instructions[fif]);
        mesh_layer->memory_refs[fif].vertex_memory_in_use = 0;
        mesh_layer->memory_refs[fif].index_memory_in_use = 0;
        mesh_layer->current_instruction = zest__layer_instruction();
        mesh_layer->memory_refs[fif].index_count = 0;
        mesh_layer->memory_refs[fif].vertex_count = 0;
        mesh_layer->memory_refs[fif].vertex_ptr = zest_BufferData(mesh_layer->memory_refs[fif].staging_vertex_data);
        mesh_layer->memory_refs[fif].index_ptr = (zest_uint*)zest_BufferData(mesh_layer->memory_refs[fif].staging_index_data);
    }
}

zest_buffer zest_GetVertexWriteBuffer(zest_layer layer) {
	ZEST_ASSERT_HANDLE(layer); //ERROR: Not a valid layer pointer
    return layer->memory_refs[layer->fif].staging_vertex_data;
}

zest_buffer zest_GetIndexWriteBuffer(zest_layer layer) {
	ZEST_ASSERT_HANDLE(layer); //ERROR: Not a valid layer pointer
    return layer->memory_refs[layer->fif].staging_index_data;
}

void zest_GrowMeshVertexBuffers(zest_layer layer) {
	ZEST_ASSERT_HANDLE(layer); //ERROR: Not a valid layer pointer
	zest_size memory_in_use = layer->memory_refs[layer->fif].vertex_memory_in_use;
    zest_GrowBuffer(&layer->memory_refs[layer->fif].staging_vertex_data, layer->vertex_struct_size, memory_in_use);
}

void zest_GrowMeshIndexBuffers(zest_layer layer) {
	ZEST_ASSERT_HANDLE(layer); //ERROR: Not a valid layer pointer
	zest_size memory_in_use = layer->memory_refs[layer->fif].vertex_memory_in_use;
    zest_GrowBuffer(&layer->memory_refs[layer->fif].staging_index_data, sizeof(zest_uint), memory_in_use);
}

void zest_PushIndex(zest_layer layer, zest_uint offset) {
	ZEST_ASSERT_HANDLE(layer); //ERROR: Not a valid layer pointer
    zest_uint index = layer->memory_refs[layer->fif].vertex_count + offset;
    zest_uint* index_ptr = (zest_uint*)layer->memory_refs[layer->fif].index_ptr;
    *index_ptr = index;
    index_ptr = index_ptr + 1;
    ZEST_ASSERT(index_ptr >= (zest_uint*)zest_BufferData(layer->memory_refs[layer->fif].staging_index_data) && index_ptr <= (zest_uint*)zest_BufferDataEnd(layer->memory_refs[layer->fif].staging_index_data));
    if (index_ptr == zest_BufferDataEnd(layer->memory_refs[layer->fif].staging_index_data)) {
        zest_bool grown = 0;
		grown = zest_GrowBuffer(&layer->memory_refs[layer->fif].staging_vertex_data, sizeof(zest_uint), 0);
		layer->memory_refs[layer->fif].staging_vertex_data = layer->memory_refs[layer->fif].staging_index_data;
        if (grown) {
            layer->memory_refs[layer->fif].index_count++;
            layer->current_instruction.total_indexes++;
            index_ptr = (zest_uint*)zest_BufferData(layer->memory_refs[layer->fif].staging_index_data);
            index_ptr += layer->memory_refs[layer->fif].index_count;
        }
        else {
            index_ptr = index_ptr - 1;
        }
    }
    else {
        layer->memory_refs[layer->fif].index_count++;
        layer->current_instruction.total_indexes++;
    }
    layer->memory_refs[layer->fif].index_ptr = index_ptr;
}

void zest_SetMeshDrawing(zest_layer layer, zest_pipeline_template pipeline) {
	ZEST_ASSERT_HANDLE(layer); 				//Not a valid layer pointer
    ZEST_ASSERT_HANDLE(pipeline);			//Not a valid pipeline template
    zest__end_mesh_instructions(layer);
    zest__start_mesh_instructions(layer);
    layer->current_instruction.pipeline_template = pipeline;
    layer->current_instruction.draw_mode = zest_draw_mode_mesh;
    layer->last_draw_mode = zest_draw_mode_mesh;
}

void zest_DrawInstanceMeshLayer(const zest_command_list command_list, void *user_data) {
    zest_layer layer = (zest_layer)user_data;
	zest__draw_instance_mesh_layer(command_list, layer, 0);
}

void zest_DrawInstanceMeshLayerWithPipeline(const zest_command_list command_list, zest_layer layer, zest_pipeline_template pipeline) {
	zest__draw_instance_mesh_layer(command_list, layer, pipeline);
}
//-- End Mesh Drawing API

//-- Instanced_mesh_drawing
void zest_StartInstanceMeshDrawing(zest_layer layer, zest_uint mesh_index, zest_pipeline_template pipeline) {
	ZEST_ASSERT_HANDLE(layer); //ERROR: Not a valid layer pointer
    ZEST_ASSERT_HANDLE(pipeline);	//Not a valid handle!
	ZEST_ASSERT(mesh_index < zest_vec_size(layer->mesh_offsets), "Mesh index is out of bounds. Make sure you add all your meshes to the layer with zest_AddMeshToLayer");
    zest__end_instance_instructions(layer);
    zest__start_instance_instructions(layer);
    layer->current_instruction.pipeline_template = pipeline;
    layer->current_instruction.draw_mode = zest_draw_mode_mesh_instance;
    layer->current_instruction.scissor = layer->scissor;
    layer->current_instruction.viewport = layer->viewport;
	layer->current_instruction.mesh_index = mesh_index;
    layer->last_draw_mode = zest_draw_mode_mesh_instance;
}

void zest_PushMeshIndex(zest_mesh mesh, zest_uint index) {
	ZEST_ASSERT_HANDLE(mesh);	//Not a valid mesh handle
    ZEST_ASSERT(index < mesh->vertex_count); //Add vertices first before triangles to make sure you're indexing vertices that exist
    zest_vec_push(mesh->context->device->allocator, mesh->indexes, index);
}

void zest_PushMeshTriangle(zest_mesh mesh, zest_uint i1, zest_uint i2, zest_uint i3) {
	ZEST_ASSERT_HANDLE(mesh);	//Not a valid mesh handle
    ZEST_ASSERT(i1 < mesh->vertex_count);	//Add vertices first before triangles to make sure you're indexing vertices that exist
    ZEST_ASSERT(i2 < mesh->vertex_count);	//Add vertices first before triangles to make sure you're indexing vertices that exist
    ZEST_ASSERT(i3 < mesh->vertex_count);	//Add vertices first before triangles to make sure you're indexing vertices that exist
    zest_vec_push(mesh->context->device->allocator, mesh->indexes, i1);
    zest_vec_push(mesh->context->device->allocator, mesh->indexes, i2);
    zest_vec_push(mesh->context->device->allocator, mesh->indexes, i3);
}

zest_mesh zest_NewMesh(zest_context context, zest_size vertex_struct_size) {
    zest_mesh mesh = (zest_mesh)ZEST__NEW(context->device->allocator, zest_mesh);
    *mesh = ZEST__ZERO_INIT(zest_mesh_t);
    mesh->magic = zest_INIT_MAGIC(zest_struct_type_mesh);
	mesh->context = context;
	mesh->vertex_struct_size = vertex_struct_size;
    return mesh;
}

void zest_FreeMesh(zest_mesh mesh) {
	ZEST_ASSERT_HANDLE(mesh);	//Not a valid mesh handle
    if (mesh->vertex_data) {
        ZEST__FREE(mesh->context->device->allocator, mesh->vertex_data);
    }
    zest_vec_free(mesh->context->device->allocator, mesh->indexes);
    ZEST__FREE(mesh->context->device->allocator, mesh);
}

void* zest_GetMeshVertex(zest_mesh mesh, zest_uint index) {
	ZEST_ASSERT_HANDLE(mesh);	//Not a valid mesh handle
    ZEST_ASSERT(index < mesh->vertex_count);
    return (char*)mesh->vertex_data + (index * mesh->vertex_struct_size);
}

void* zest_FirstMeshVertex(zest_mesh mesh) {
	ZEST_ASSERT_HANDLE(mesh);	//Not a valid mesh handle
	if (!mesh->vertex_data) {
		zest_size new_size = 256;
		void* new_data = ZEST__ALLOCATE(mesh->context->device->allocator, new_size);
	}
    return mesh->vertex_data;
}

void* zest_NextMeshVertex(zest_mesh mesh, void* current) {
	ZEST_ASSERT_HANDLE(mesh);	//Not a valid mesh handle
    char* next = (char*)current + mesh->vertex_struct_size;
    char* end = (char*)mesh->vertex_data + (mesh->vertex_count * mesh->vertex_struct_size);
    return (next < end && next > mesh->vertex_data) ? next : NULL;
}

void zest_ReserveMeshVertices(zest_mesh mesh, zest_uint count) {
	ZEST_ASSERT_HANDLE(mesh);	//Not a valid mesh handle
    if (count <= mesh->vertex_capacity) return;
    zest_size new_size = count * mesh->vertex_struct_size;
    void* new_data = ZEST__ALLOCATE(mesh->context->device->allocator, new_size);
    if (mesh->vertex_data) {
        memcpy(new_data, mesh->vertex_data, mesh->vertex_count * mesh->vertex_struct_size);
        ZEST__FREE(mesh->context->device->allocator, mesh->vertex_data);
    }
    mesh->vertex_data = new_data;
    mesh->vertex_capacity = count;
}

void zest_PushMeshVertexData(zest_mesh mesh, const void* vertex_data) {
	ZEST_ASSERT_HANDLE(mesh);	//Not a valid mesh handle
    if (mesh->vertex_count >= mesh->vertex_capacity) {
        zest_uint new_capacity = mesh->vertex_capacity ? mesh->vertex_capacity * 2 : 8;
        zest_ReserveMeshVertices(mesh, new_capacity);
    }
    void* dest = (char*)mesh->vertex_data + (mesh->vertex_count * mesh->vertex_struct_size);
    memcpy(dest, vertex_data, mesh->vertex_struct_size);
    mesh->vertex_count++;
}

ZEST_API void zest_CopyMeshVertexData(zest_mesh mesh, const void* vertex_data, zest_size size) {
	ZEST_ASSERT_HANDLE(mesh);	//Not a valid mesh handle
	zest_size size_check = size % mesh->vertex_struct_size;
	ZEST_ASSERT(!size_check, "The vertex data size that you're trying to copy to the mesh is not divisable by the vertext struct size set in the mesh. Make sure you're uploading the correct vertex data.");
	if (mesh->vertex_capacity * mesh->vertex_struct_size < size) {
        zest_ReserveMeshVertices(mesh, (zest_uint)(size / mesh->vertex_struct_size));
	}
	memcpy(mesh->vertex_data, vertex_data, size);
	mesh->vertex_count = (zest_uint)(size / mesh->vertex_struct_size);
}

ZEST_API void zest_CopyMeshIndexData(zest_mesh mesh, const zest_uint *index_data, zest_uint count) {
	ZEST_ASSERT_HANDLE(mesh);	//Not a valid mesh handle
	zest_vec_resize(mesh->context->device->allocator, mesh->indexes, count);
	memcpy(mesh->indexes, index_data, count * sizeof(zest_uint));
}

void zest_ClearMeshVertices(zest_mesh mesh) {
	ZEST_ASSERT_HANDLE(mesh);	//Not a valid mesh handle
    mesh->vertex_count = 0;
}

zest_bounding_box_t zest_NewBoundingBox() {
    zest_bounding_box_t bb = ZEST__ZERO_INIT(zest_bounding_box_t);
    bb.max_bounds = zest_Vec3Set( -9999999.f, -9999999.f, -9999999.f );
    bb.min_bounds = zest_Vec3Set( ZEST_MAX_FLOAT, ZEST_MAX_FLOAT, ZEST_MAX_FLOAT );
    return bb;
}

zest_uint zest_MeshVertexCount(zest_mesh mesh) {
	ZEST_ASSERT_HANDLE(mesh);	//Not a valid mesh handle
    return mesh->vertex_count;
}

zest_uint zest_MeshIndexCount(zest_mesh mesh) {
	ZEST_ASSERT_HANDLE(mesh);	//Not a valid mesh handle
    return zest_vec_size(mesh->indexes);
}

zest_size zest_MeshVertexDataSize(zest_mesh mesh) {
	ZEST_ASSERT_HANDLE(mesh);	//Not a valid mesh handle
    return mesh->vertex_count * mesh->vertex_struct_size;
}

zest_size zest_MeshIndexDataSize(zest_mesh mesh) {
	ZEST_ASSERT_HANDLE(mesh);	//Not a valid mesh handle
    return zest_vec_size_in_bytes(mesh->indexes);
}

void zest_MeshDataSizes(zest_mesh *meshes, zest_uint mesh_count, zest_size *vertex_capacity, zest_size *index_capacity) {
	ZEST_ASSERT(meshes, "Null mesh array passed in to zest_MeshDataSizes.");
	*vertex_capacity = 0;
	*index_capacity = 0;
	for (int i = 0; i != mesh_count; i++) {
		zest_mesh mesh = meshes[i];
		*vertex_capacity += zest_MeshVertexDataSize(mesh);
		*index_capacity += zest_MeshIndexDataSize(mesh);
	}
}

zest_uint *zest_MeshIndexData(zest_mesh mesh) {
	ZEST_ASSERT_HANDLE(mesh);	//Not a valid mesh handle
	return mesh->indexes;
}

void *zest_MeshVertexData(zest_mesh mesh) {
	ZEST_ASSERT_HANDLE(mesh);	//Not a valid mesh handle
	return mesh->vertex_data;
}

zest_uint zest_AddMeshToLayer(zest_layer layer, zest_mesh src_mesh, zest_uint texture_index) {
	ZEST_ASSERT_HANDLE(layer); 			//ERROR: Not a valid layer pointer
	ZEST_ASSERT(layer->vertex_data);	//ERROR: No vertex buffer found. Make sure you call zest_CreateInstanceMeshLayer with appropriate capacity
	ZEST_ASSERT(layer->index_data); 	//ERROR: No index buffer found. Make sure you call zest_CreateInstanceMeshLayer with appropriate capacity
	zest_device device = layer->context->device;
	zest_size src_mesh_vertex_size = zest_MeshVertexDataSize(src_mesh);
	zest_size src_mesh_index_size = zest_MeshIndexDataSize(src_mesh);
	ZEST_ASSERT(layer->vertex_data->size - layer->used_vertex_data >= src_mesh_vertex_size, "Not enough space left in the layer vertex buffer to add this mesh data.");
	ZEST_ASSERT(layer->index_data->size - layer->used_index_data >= src_mesh_index_size, "Not enough space left in the layer index buffer to add this mesh data.");
    zest_buffer vertex_staging_buffer = zest_CreateStagingBuffer(device, src_mesh_vertex_size, src_mesh->vertex_data);
    zest_buffer index_staging_buffer = zest_CreateStagingBuffer(device, src_mesh_index_size, src_mesh->indexes);
	zest_queue queue = zest_imm_BeginCommandBuffer(device, zest_queue_transfer);
    zest_imm_CopyBufferRegion(queue, vertex_staging_buffer, 0, layer->vertex_data, layer->used_vertex_data, src_mesh_vertex_size);
    zest_imm_CopyBufferRegion(queue, index_staging_buffer, 0, layer->index_data, layer->used_index_data, src_mesh_index_size);
	zest_imm_EndCommandBuffer(queue);
	zest_mesh_offset_data_t last_mesh_offsets = ZEST__ZERO_INIT(zest_mesh_offset_data_t);
	if (zest_vec_size(layer->mesh_offsets)) {
		last_mesh_offsets = zest_vec_back(layer->mesh_offsets);
	}
	zest_mesh_offset_data_t offset_data = {
		last_mesh_offsets.vertex_offset + last_mesh_offsets.vertex_count,
		last_mesh_offsets.index_offset + last_mesh_offsets.index_count,
		zest_MeshVertexCount(src_mesh),
		zest_MeshIndexCount(src_mesh),
		texture_index
	};
	zest_uint index = zest_vec_size(layer->mesh_offsets);
	zest_vec_push(layer->context->allocator, layer->mesh_offsets, offset_data);
	layer->used_vertex_data += src_mesh_vertex_size;
	layer->used_index_data += src_mesh_index_size;
    zest_FreeBufferNow(vertex_staging_buffer);
    zest_FreeBufferNow(index_staging_buffer);
	return index;
}

zest_uint zest_GetLayerMeshTextureIndex(zest_layer layer, zest_uint mesh_index) {
	ZEST_ASSERT(mesh_index < zest_vec_size(layer->mesh_offsets), "Mesh index outside of bounds.");
	return layer->mesh_offsets[mesh_index].texture_index;
}

//--End Instance Draw mesh layers

//Compute shaders
zest_compute zest__new_compute(zest_device device, const char* name) {
	zest_resource_store_t *store = &device->resource_stores[zest_handle_type_compute_pipelines];
    zest_compute_handle handle = ZEST_STRUCT_LITERAL(zest_compute_handle, zest__add_store_resource(store), store );
	handle.store = store;
    zest_compute compute = (zest_compute)zest__get_store_resource_unsafe(store, handle.value);
    *compute = ZEST__ZERO_INIT(zest_compute_t);
    compute->magic = zest_INIT_MAGIC(zest_struct_type_compute);
    compute->backend = (zest_compute_backend)device->platform->new_compute_backend(device);
    compute->handle = handle;
    return compute;
}

zest_compute_handle zest_CreateCompute(zest_device device, const char *name, zest_shader_handle shader) {
	zest_compute compute = zest__new_compute(device, name);
	compute->shader = shader;

    compute->pipeline_layout = device->pipeline_layout;

    ZEST__FLAG(compute->flags, zest_compute_flag_sync_required);
    ZEST__FLAG(compute->flags, zest_compute_flag_rewrite_command_buffer);
    ZEST__FLAG(compute->flags, zest_compute_flag_is_active);

    zest_bool result = device->platform->finish_compute(device, compute);
    if (!result) {
        zest__cleanup_compute(compute);
        return ZEST__ZERO_INIT(zest_compute_handle);
    }
	zest__activate_resource(compute->handle.store, compute->handle.value);
    return compute->handle;
}

void zest__cleanup_compute(zest_compute compute) {
	ZEST_ASSERT_HANDLE(compute);	//Not a valid compute handle
	zest_device device = (zest_device)compute->handle.store->origin;
    device->platform->cleanup_compute_backend(compute);
    zest__remove_store_resource(compute->handle.store, compute->handle.value);
}
//--End Compute shaders

//-- Start debug helpers
void zest_OutputMemoryUsage(zest_context context) {
    printf("Host Memory Pools\n");
    zest_size total_host_memory = 0;
    zest_size total_device_memory = 0;
    for (int i = 0; i != context->device->memory_pool_count; ++i) {
        printf("\tMemory Pool Size: %llu\n", context->device->memory_pool_sizes[i]);
        total_host_memory += context->device->memory_pool_sizes[i];
    }
    printf("Device Memory Pools\n");
    zest_map_foreach(i, context->device->buffer_allocators) {
        zest_buffer_allocator buffer_allocator = *zest_map_at_index(context->device->buffer_allocators, i);
        zest_buffer_usage_t usage = { buffer_allocator->buffer_info.property_flags };
        zest_key usage_key = zest_map_hash_ptr(&usage, sizeof(zest_buffer_usage_t));
        zest_buffer_pool_size_t pool_size = *zest_map_at_key(context->device->pool_sizes, usage_key);
        if (buffer_allocator->buffer_info.image_usage_flags) {
            printf("\t%s (%s), Usage: %u, Image Usage: %u\n", "Image", pool_size.name, buffer_allocator->buffer_info.buffer_usage_flags, buffer_allocator->buffer_info.image_usage_flags);
        }
        else {
            printf("\t%s (%s), Usage: %u, Properties: %u\n", "Buffer", pool_size.name, buffer_allocator->buffer_info.buffer_usage_flags, buffer_allocator->buffer_info.property_flags);
        }
        zest_vec_foreach(j, buffer_allocator->memory_pools) {
            zest_device_memory_pool memory_pool = buffer_allocator->memory_pools[j];
            printf("\t\tMemory Pool Size: %llu\n", memory_pool->size);
            total_device_memory += memory_pool->size;
        }
    }
    printf("Total Host Memory: %llu (%llu MB)\n", total_host_memory, total_host_memory / zloc__MEGABYTE(1));
    printf("Total Device Memory: %llu (%llu MB)\n", total_device_memory, total_device_memory / zloc__MEGABYTE(1));
    printf("\n");
    printf("\t\t\t\t-- * --\n");
    printf("\n");
}

void zest_PrintReports(zest_context context) {
    if (zest_map_size(context->device->reports)) {
		ZEST_PRINT("--- Zest Reports ---");
        ZEST_PRINT("");
        zest_map_foreach(i, context->device->reports) {
            zest_report_t *report = &context->device->reports.data[i];
			ZEST_PRINT("Count: %i. Message in file [%s] function [%s] on line %i: %s", report->count, report->file_name, report->function_name, report->line_number, report->message.str);
            ZEST_PRINT("-------------------------------------------------");
        }
        ZEST_PRINT("");
    }
}

zest_uint zest_ReportCount(zest_context context) {
	ZEST_ASSERT_HANDLE(context);	//Not a valid context pointer
	return (zest_map_size(context->device->reports));
}

void zest_ResetReports(zest_context context) {
    zest_map_foreach(i, context->device->reports) {
        zest_report_t *report = &context->device->reports.data[i];
        zest_FreeText(context->device->allocator, &report->message);
    }
	zest_map_free(context->device->allocator, context->device->reports);
}

const char *zest__struct_type_to_string(zest_struct_type struct_type) {
    switch (struct_type) {
		case zest_struct_type_view                    : return "image_view"; break;
		case zest_struct_type_view_array              : return "image_view_array"; break;
		case zest_struct_type_image                   : return "image"; break;
		case zest_struct_type_imgui_image             : return "imgui_image"; break;
		case zest_struct_type_image_collection        : return "image_collection"; break;
		case zest_struct_type_sampler                 : return "sampler"; break;
		case zest_struct_type_layer                   : return "layer"; break;
		case zest_struct_type_pipeline                : return "pipeline"; break;
		case zest_struct_type_pipeline_template       : return "pipeline_template"; break;
		case zest_struct_type_set_layout              : return "set_layout"; break;
		case zest_struct_type_descriptor_set          : return "descriptor_set"; break;
		case zest_struct_type_uniform_buffer          : return "uniform_buffer"; break;
		case zest_struct_type_buffer_allocator        : return "buffer_allocator"; break;
		case zest_struct_type_descriptor_pool         : return "descriptor_pool"; break;
		case zest_struct_type_compute                 : return "compute"; break;
		case zest_struct_type_buffer                  : return "buffer"; break;
		case zest_struct_type_device_memory_pool      : return "device_memory_pool"; break;
		case zest_struct_type_timer                   : return "timer"; break;
		case zest_struct_type_window                  : return "window"; break;
		case zest_struct_type_shader                  : return "shader"; break;
		case zest_struct_type_imgui                   : return "imgui"; break;
		case zest_struct_type_queue                   : return "queue"; break;
		case zest_struct_type_execution_timeline      : return "execution_timeline"; break;
		case zest_struct_type_frame_graph_semaphores  : return "frame_graph_semaphores"; break;
		case zest_struct_type_swapchain               : return "swapchain"; break;
		case zest_struct_type_frame_graph             : return "frame_graph"; break;
		case zest_struct_type_pass_node               : return "pass_node"; break;
		case zest_struct_type_resource_node           : return "resource_node"; break;
		case zest_struct_type_wave_submission         : return "wave_submission"; break;
		case zest_struct_type_device                  : return "device"; break;
		case zest_struct_type_app                     : return "app"; break;
		case zest_struct_type_vector                  : return "vector"; break;
		case zest_struct_type_bitmap                  : return "bitmap"; break;
		case zest_struct_type_render_target_group     : return "render_target_group"; break;
		case zest_struct_type_slang_info              : return "slang_info"; break;
		case zest_struct_type_render_pass             : return "render_pass"; break;
		case zest_struct_type_mesh                    : return "mesh"; break;
		case zest_struct_type_texture_asset           : return "texture_asset"; break;
		case zest_struct_type_atlas_region            : return "atlas_region"; break;
		default: return "UNKNOWN"; break;
    }
    return "UNKNOWN";
}

const char *zest__platform_command_to_string(zest_platform_command command) {
    switch (command) {
		case zest_command_surface                : return "Surface"; break;
		case zest_command_instance               : return "Instance"; break;
		case zest_command_logical_device         : return "Instance"; break;
		case zest_command_debug_messenger        : return "Debug Messenger"; break;
		case zest_command_device_instance        : return "Device Instance"; break;
		case zest_command_semaphore              : return "Semaphore"; break;
		case zest_command_command_pool           : return "Command Pool"; break;
		case zest_command_command_buffer         : return "Command Buffer"; break;
		case zest_command_buffer                 : return "Buffer"; break;
		case zest_command_allocate_memory_pool   : return "Allocate Memory Pool"; break;
		case zest_command_allocate_memory_image  : return "Allocate Memory Image"; break;
		case zest_command_fence                  : return "Fence"; break;
		case zest_command_swapchain              : return "Swapchain"; break;
		case zest_command_pipeline_cache         : return "Pipeline Cache"; break;
		case zest_command_descriptor_layout      : return "Descriptor Layout"; break;
		case zest_command_descriptor_pool        : return "Descriptor Pool"; break;
		case zest_command_pipeline_layout        : return "Pipeline Layout"; break;
		case zest_command_pipelines              : return "Pipelines"; break;
		case zest_command_shader_module          : return "Shader Module"; break;
		case zest_command_sampler                : return "Sampler"; break;
		case zest_command_image                  : return "Image"; break;
		case zest_command_image_view             : return "Image View"; break;
		case zest_command_render_pass            : return "Render Pass"; break;
		case zest_command_frame_buffer           : return "Frame Buffer"; break;
		case zest_command_query_pool             : return "Query Pool"; break;
		case zest_command_compute_pipeline       : return "Compute Pipeline"; break;
		default: return "UNKNOWN"; break;
    }
    return "UNKNOWN";
}

const char *zest__platform_context_to_string(zest_platform_memory_context context) {
    switch (context) {
		case zest_platform_context         : return "Renderer"; break;
		case zest_platform_device           : return "Device"; break;
		default: return "UNKNOWN"; break;
    }
    return "UNKNOWN";
}

void zest__print_block_info(zloc_allocator *allocator, void *allocation, zloc_header *current_block, zest_platform_memory_context context_filter, zest_platform_command command_filter) {
    if (ZEST_VALID_IDENTIFIER(allocation)) {
		zest_struct_type struct_type = (zest_struct_type)ZEST_STRUCT_TYPE(allocation);
		ptrdiff_t offset_from_allocator = (ptrdiff_t)allocation - (ptrdiff_t)allocator;
		zest_vec *vector = (zest_vec*)allocation;
		switch (struct_type) {
			case zest_struct_type_vector:
				ZEST_PRINT("Allocation: %p, id: %i, size: %zu, offset: %zi, type: %s", allocation, vector->id, current_block->size, offset_from_allocator, zest__struct_type_to_string(struct_type));
				break;
			default:
				ZEST_PRINT("Allocation: %p, size: %zu, offset: %zi, type: %s", allocation, current_block->size, offset_from_allocator, zest__struct_type_to_string(struct_type));
				break;
		}
    } else {
        //Is it a vulkan allocation?
        zest_platform_memory_info_t *vulkan_info = (zest_platform_memory_info_t *)allocation;
        zest_uint mem_context = (vulkan_info->context_info & 0xff0000) >> 16;
        zest_uint command = (vulkan_info->context_info & 0xff000000) >> 24;
        if ((!command_filter || command == command_filter) && (!context_filter || context_filter == mem_context)) {
            if (ZEST_IS_INTITIALISED(vulkan_info->context_info)) {
                ZEST_PRINT("Vulkan allocation: %p, Timestamp: %u, Category: %s, Command: %s", allocation, vulkan_info->timestamp,
						   zest__platform_context_to_string((zest_platform_memory_context)mem_context),
						   zest__platform_command_to_string((zest_platform_command)command)
						   );
			} else if(zloc__block_size(current_block) > 0) {
				//Unknown block, print what we can about it
                ptrdiff_t offset_from_allocator = (ptrdiff_t)allocation - (ptrdiff_t)allocator;
				ZEST_PRINT("Unknown Block - size: %zu (%p) Offset from allocator: %zi", zloc__block_size(current_block), allocation, offset_from_allocator);
            }
        }
    }
}

void zest_PrintMemoryBlocks(zloc_allocator *allocator, zloc_header *first_block, zest_bool output_all, zest_platform_memory_context context_filter, zest_platform_command command_filter) {
    zloc_pool_stats_t stats = ZEST__ZERO_INIT(zloc_pool_stats_t);
    zest_memory_stats_t zest_stats = ZEST__ZERO_INIT(zest_memory_stats_t);
    zloc_header *current_block = first_block;
    ZEST_PRINT("Current used blocks in ZEST memory");
    while (!zloc__is_last_block_in_pool(current_block)) {
        if (zloc__is_free_block(current_block)) {
            stats.free_blocks++;
            stats.free_size += zloc__block_size(current_block);
        } else {
            stats.used_blocks++;
            zest_size block_size = zloc__block_size(current_block);
            stats.used_size += block_size;
            void *allocation = (void*)((char*)current_block + zloc__BLOCK_POINTER_OFFSET);
            if (output_all) {
                zest__print_block_info(allocator, allocation, current_block, context_filter, command_filter);
            }
			zest_platform_memory_info_t *vulkan_info = (zest_platform_memory_info_t *)allocation;
			zest_uint mem_context = (vulkan_info->context_info & 0xff0000) >> 16;
			zest_uint command = (vulkan_info->context_info & 0xff000000) >> 24;
            if (ZEST_IS_INTITIALISED(vulkan_info->context_info)) {
                if (mem_context == zest_platform_device) {
                    zest_stats.device_allocations++;
                } else if (mem_context == zest_platform_context) {
                    zest_stats.renderer_allocations++;
                }
                if (command_filter == command && context_filter == mem_context) {
                    zest_stats.filter_count++;
                }
            }
        }
        current_block = zloc__next_physical_block(current_block);
    }
    if (zloc__is_free_block(current_block)) {
        stats.free_blocks++;
        stats.free_size += zloc__block_size(current_block);
    } else {
		zest_size block_size = zloc__block_size(current_block);
		void *allocation = (void*)((char*)current_block + zloc__BLOCK_SIZE_OVERHEAD);
        if (block_size) {
			stats.used_blocks++;
            stats.used_size += block_size ? block_size : 0;
			ZEST_PRINT("Block size: %zi (%p)", zloc__block_size(current_block), allocation);
        }
        if (output_all) {
            zest__print_block_info(allocator, allocation, current_block, context_filter, command_filter);
        }
        zest_platform_memory_info_t *vulkan_info = (zest_platform_memory_info_t *)allocation;
        zest_uint mem_context = (vulkan_info->context_info & 0xff0000) >> 16;
        zest_uint command = (vulkan_info->context_info & 0xff000000) >> 24;
        if (ZEST_IS_INTITIALISED(vulkan_info->context_info)) {
            if (mem_context == zest_platform_device) {
                zest_stats.device_allocations++;
            } else if (mem_context == zest_platform_context) {
                zest_stats.renderer_allocations++;
            }
            if (command_filter == command && context_filter == mem_context) {
                zest_stats.filter_count++;
            }
        }
    }
    ZEST_PRINT("Free blocks: %u, Used blocks: %u", stats.free_blocks, stats.used_blocks);
    ZEST_PRINT("Device allocations: %u, Renderer Allocations: %u, Filter Allocations: %u", zest_stats.device_allocations, zest_stats.renderer_allocations, zest_stats.filter_count);
}

zest_uint zest_GetValidationErrorCount(zest_context context) {
    return zest_map_size(context->device->validation_errors);
}

void zest_ResetValidationErrors(zest_device device) {
    zest_map_foreach(i, device->validation_errors) {
        zest_FreeText(device->allocator, &device->validation_errors.data[i]);
    }
    zest_map_free(device->allocator, device->validation_errors);
}

zest_bool zest_SetErrorLogPath(zest_device device, const char* path) {
    ZEST_ASSERT_HANDLE(device);    //Have you initialised Zest yet?
    zest_SetTextf(device->allocator, &device->log_path, "%s/%s", path, "zest_log.txt");
    FILE* log_file = zest__open_file(device->log_path.str, "wb");
    if (!log_file) {
        printf("Could Not Create Log file!");
        return ZEST_FALSE;
    }
	fprintf(log_file, "Start of Log\n");
    fclose(log_file);
    return ZEST_TRUE;
}

const char *zest_GetErrorLogPath(zest_device device) {
    ZEST_ASSERT_HANDLE(device);    //Have you initialised Zest yet?
	return device->log_path.str;
}
//-- End Debug helpers

//-- Timer functions
zest_timer_t zest_CreateTimer(double update_frequency) {
    zest_timer_t timer = ZEST__ZERO_INIT(zest_timer_t);
    timer.update_frequency = update_frequency;
    timer.update_tick_length = ZEST_MICROSECS_SECOND / timer.update_frequency;
    timer.update_time = 1.f / timer.update_frequency;
    timer.ticker = zest_Microsecs() - timer.update_tick_length;
    timer.max_elapsed_time = timer.update_tick_length * 4.0;
    timer.start_time = (double)zest_Microsecs();
    timer.current_time = (double)zest_Microsecs();
    return timer;
}

void zest_TimerReset(zest_timer_t *timer) {
    timer->start_time = (double)zest_Microsecs();
    timer->current_time = (double)zest_Microsecs();
}

double zest_TimerDeltaTime(zest_timer_t *timer) {
    return timer->delta_time;
}

void zest_TimerTick(zest_timer_t *timer) {
    timer->delta_time = (double)zest_Microsecs() - timer->start_time;
}

double zest_TimerUpdateTime(zest_timer_t *timer) {
    return timer->update_time;
}

double zest_TimerFrameLength(zest_timer_t *timer) {
    return timer->update_tick_length;
}

double zest_TimerAccumulate(zest_timer_t *timer) {
    double new_time = (double)zest_Microsecs();
    double frame_time = fabs(new_time - timer->current_time);
    frame_time = ZEST__MIN(frame_time, ZEST_MICROSECS_SECOND);
    timer->current_time = new_time;

    timer->accumulator_delta = timer->accumulator;
    timer->accumulator += frame_time;
    timer->accumulator = ZEST__MIN(timer->accumulator, timer->max_elapsed_time);
    timer->update_count = 0;
    return frame_time;
}

int zest_TimerPendingTicks(zest_timer_t *timer) {
    return (int)(timer->accumulator / timer->update_tick_length);
}

void zest_TimerUnAccumulate(zest_timer_t *timer) {
    timer->accumulator -= timer->update_tick_length;
    timer->accumulator_delta = timer->accumulator_delta - timer->accumulator;
    timer->update_count++;
    timer->time_passed += timer->update_time;
    timer->seconds_passed += timer->update_time * 1000.f;
}

zest_bool zest_TimerDoUpdate(zest_timer_t *timer) {
    return timer->accumulator >= timer->update_tick_length;
}

double zest_TimerLerp(zest_timer_t *timer) {
    return timer->update_count > 1 ? 1.f : timer->lerp;
}

void zest_TimerSet(zest_timer_t *timer) {
    timer->lerp = timer->accumulator / timer->update_tick_length;
}

void zest_TimerSetMaxFrames(zest_timer_t *timer, double frames) {
    timer->max_elapsed_time = timer->update_tick_length * frames;
}

void zest_TimerSetUpdateFrequency(zest_timer_t *timer, double frequency) {
    timer->update_frequency = frequency;
    timer->update_tick_length = ZEST_MICROSECS_SECOND / timer->update_frequency;
    timer->update_time = 1.f / timer->update_frequency;
    timer->ticker = zest_Microsecs() - timer->update_tick_length;
}

double zest_TimerUpdateFrequency(zest_timer_t *timer) {
    return timer->update_frequency;
}

zest_bool zest_TimerUpdateWasRun(zest_timer_t *timer) {
    return timer->update_count > 0;
}
//-- End Timer Functions

//-- Command_buffer_functions
// -- Frame_graph_context_functions
zest_bool zest_cmd_UploadBuffer(const zest_command_list command_list, zest_buffer_uploader_t *uploader) {
    if (!zest_vec_size(uploader->buffer_copies)) {
        return ZEST_FALSE;
    }
    return command_list->context->device->platform->upload_buffer(command_list, uploader);
}

void zest_cmd_DrawIndexed(const zest_command_list command_list, zest_uint index_count, zest_uint instance_count, zest_uint first_index, int32_t vertex_offset, zest_uint first_instance) {
    ZEST_ASSERT_HANDLE(command_list);        //Not valid command_list, this command must be called within a frame graph execution callback
	command_list->context->device->platform->draw_indexed(command_list, index_count, instance_count, first_index, vertex_offset, first_instance);
}

void zest_cmd_SetDepthBias(const zest_command_list command_list, float factor, float clamp, float slope) {
    ZEST_ASSERT_HANDLE(command_list);        //Not valid command_list, this command must be called within a frame graph execution callback
	command_list->context->device->platform->set_depth_bias(command_list, factor, clamp, slope);
}

void zest_cmd_CopyBuffer(const zest_command_list command_list, zest_buffer src_buffer, zest_buffer dst_buffer, zest_size size) {
    ZEST_ASSERT(size <= src_buffer->size);        //size must be less than or equal to the staging buffer size and the device buffer size
    ZEST_ASSERT(size <= dst_buffer->size);
    ZEST_ASSERT_HANDLE(command_list);                  //Not valid command_list, this command must be called within a frame graph execution callback
	command_list->context->device->platform->copy_buffer(command_list, src_buffer, dst_buffer, size);
}

void zest_cmd_BindDescriptorSets(const zest_command_list command_list, zest_pipeline_bind_point bind_point, zest_pipeline_layout layout, zest_descriptor_set *sets, zest_uint set_count, zest_uint first_set) {
    ZEST_ASSERT_HANDLE(command_list);        //Not valid command_list, this command must be called within a frame graph execution callback
    ZEST_ASSERT(set_count && sets);    //No descriptor sets. Must bind the pipeline with a valid desriptor set
	command_list->context->device->platform->bind_descriptor_sets(command_list, bind_point, layout, sets, set_count, first_set);
}

void zest_cmd_BindPipeline(const zest_command_list command_list, zest_pipeline pipeline) {
    ZEST_ASSERT_HANDLE(command_list);        //Not valid command_list, this command must be called within a frame graph execution callback
	command_list->context->device->platform->bind_pipeline(command_list, pipeline);
}

void zest_cmd_BindComputePipeline(const zest_command_list command_list, zest_compute compute) {
    ZEST_ASSERT_HANDLE(command_list);        //Not valid command_list, this command must be called within a frame graph execution callback
	ZEST_ASSERT_HANDLE(compute);			//Not a valid compute handle
	command_list->context->device->platform->bind_compute_pipeline(command_list, compute);
}

void zest_cmd_BindVertexBuffer(const zest_command_list command_list, zest_uint first_binding, zest_uint binding_count, zest_buffer buffer) {
    ZEST_ASSERT_HANDLE(command_list);        //Not valid command_list, this command must be called within a frame graph execution callback
	command_list->context->device->platform->bind_vertex_buffer(command_list, first_binding, binding_count, buffer);
}

void zest_cmd_BindIndexBuffer(const zest_command_list command_list, zest_buffer buffer) {
    ZEST_ASSERT_HANDLE(command_list);        //Not valid command_list, this command must be called within a frame graph execution callback
	command_list->context->device->platform->bind_index_buffer(command_list, buffer);
}

void zest_cmd_SendPushConstants(const zest_command_list command_list, void *data, zest_uint size) {
    ZEST_ASSERT_HANDLE(command_list);        //Not valid command_list, this command must be called within a frame graph execution callback
	command_list->context->device->platform->send_push_constants(command_list, command_list->device->pipeline_layout, data, size);
}

void zest_cmd_Draw(const zest_command_list command_list, zest_uint vertex_count, zest_uint instance_count, zest_uint first_vertex, zest_uint first_instance) {
    ZEST_ASSERT_HANDLE(command_list);        //Not valid command_list, this command must be called within a frame graph execution callback
	command_list->context->device->platform->draw(command_list, vertex_count, instance_count, first_vertex, first_instance);
}

void zest_cmd_DrawLayerInstruction(const zest_command_list command_list, zest_uint vertex_count, zest_layer_instruction_t *instruction) {
    ZEST_ASSERT_HANDLE(command_list);        //Not valid command_list, this command must be called within a frame graph execution callback
	command_list->context->device->platform->draw_layer_instruction(command_list, vertex_count, instruction);
}

void zest_cmd_DispatchCompute(const zest_command_list command_list, zest_uint group_count_x, zest_uint group_count_y, zest_uint group_count_z) {
    ZEST_ASSERT_HANDLE(command_list);        //Not valid command_list, this command must be called within a frame graph execution callback
	command_list->context->device->platform->dispatch_compute(command_list, group_count_x, group_count_y, group_count_z);
}

void zest_cmd_SetScreenSizedViewport(const zest_command_list command_list, float min_depth, float max_depth) {
    //This function must be called within a frame graph execution pass callback
    ZEST_ASSERT(command_list);    //Must be a valid command buffer
    ZEST_ASSERT_HANDLE(command_list->frame_graph);
    ZEST_ASSERT_HANDLE(command_list->frame_graph->swapchain);    //frame graph must be set up with a swapchain to use this function
	command_list->context->device->platform->set_screensized_viewport(command_list, min_depth, max_depth);
}

void zest_cmd_LayerViewport(const zest_command_list command_list, zest_layer layer) {
	command_list->context->device->platform->scissor(command_list, &layer->scissor);
	command_list->context->device->platform->viewport(command_list, &layer->viewport);
}

void zest_cmd_Scissor(const zest_command_list command_list, zest_scissor_rect_t *scissor) {
    ZEST_ASSERT_HANDLE(command_list);        //Not valid command_list, this command must be called within a frame graph execution callback
	command_list->context->device->platform->scissor(command_list, scissor);
}

void zest_cmd_ViewPort(const zest_command_list command_list, zest_viewport_t *viewport) {
    ZEST_ASSERT_HANDLE(command_list);        //Not valid command_list, this command must be called within a frame graph execution callback
	command_list->context->device->platform->viewport(command_list, viewport);
}

void zest_cmd_BlitImageMip(const zest_command_list command_list, zest_resource_node src, zest_resource_node dst, zest_uint mip_to_blit, zest_pipeline_stage_flags pipeline_stage) {
    ZEST_ASSERT_HANDLE(command_list);        //Not valid command_list, this command must be called within a frame graph execution callback
    ZEST_ASSERT_HANDLE(src);
    ZEST_ASSERT_HANDLE(dst);
    ZEST_ASSERT(src->type == zest_resource_type_image && dst->type == zest_resource_type_image);
    //Source and destination images must be the same width/height and have the same number of mip levels
    ZEST_ASSERT(src->image.info.extent.width == dst->image.info.extent.width);
    ZEST_ASSERT(src->image.info.extent.height == dst->image.info.extent.height);
    ZEST_ASSERT(src->image.info.mip_levels == dst->image.info.mip_levels);
	command_list->context->device->platform->blit_image_mip(command_list, src, dst, mip_to_blit, pipeline_stage);
}

void zest_cmd_CopyImageMip(const zest_command_list command_list, zest_resource_node src, zest_resource_node dst, zest_uint mip_to_copy, zest_pipeline_stage_flags pipeline_stage) {
    ZEST_ASSERT_HANDLE(command_list);        //Not valid command_list, this command must be called within a frame graph execution callback
    ZEST_ASSERT_HANDLE(src);
    ZEST_ASSERT_HANDLE(dst);
    ZEST_ASSERT(src->type == zest_resource_type_image && dst->type == zest_resource_type_image);
    //Source and destination images must be the same width/height and have the same number of mip levels
    ZEST_ASSERT(src->image.info.extent.width == dst->image.info.extent.width);
    ZEST_ASSERT(src->image.info.extent.height == dst->image.info.extent.height);
    ZEST_ASSERT(src->image.info.mip_levels == dst->image.info.mip_levels);

    //You must ensure that when creating the images that you use usage hints to indicate that you intend to copy
    //the images. When creating a transient image resource you can set the usage hints in the zest_image_resource_info_t
    //type that you pass into zest_CreateTransientImageResource. Trying to copy images that don't have the appropriate
    //usage flags set will result in validation errors.
    ZEST_ASSERT(src->image.info.flags & zest_image_flag_transfer_src);
    ZEST_ASSERT(dst->image.info.flags & zest_image_flag_transfer_dst);
	command_list->context->device->platform->copy_image_mip(command_list, src, dst, mip_to_copy, pipeline_stage);
}

void zest_cmd_Clip(const zest_command_list command_list, float x, float y, float width, float height, float min_depth, float max_depth) {
    ZEST_ASSERT_HANDLE(command_list);        //Not valid command_list, this command must be called within a frame graph execution callback
	command_list->context->device->platform->clip(command_list, x, y, width, height, min_depth, max_depth);
}

void zest_cmd_InsertComputeImageBarrier(const zest_command_list command_list, zest_resource_node resource, zest_uint base_mip) {
    ZEST_ASSERT_HANDLE(command_list);        //Not valid command_list, this command must be called within a frame graph execution callback
    ZEST_ASSERT_HANDLE(resource);    //Not a valid resource handle!
    ZEST_ASSERT(resource->type == zest_resource_type_image);    //resource type must be an image
	command_list->context->device->platform->insert_compute_image_barrier(command_list, resource, base_mip);
}

zest_bool zest_cmd_ImageClear(const zest_command_list command_list, zest_image image) {
	return command_list->context->device->platform->image_clear(command_list, image);
}

void zest_cmd_BindMeshVertexBuffer(const zest_command_list command_list, zest_layer layer) {
	ZEST_ASSERT_HANDLE(layer); 				//ERROR: Not a valid layer pointer
    ZEST_ASSERT_HANDLE(command_list);       //Not valid command_list, this command must be called within a frame graph execution callback
	command_list->context->device->platform->bind_mesh_vertex_buffer(command_list, layer);
}

void zest_cmd_BindMeshIndexBuffer(const zest_command_list command_list, zest_layer layer) {
	ZEST_ASSERT_HANDLE(layer); //ERROR: Not a valid layer pointer
    ZEST_ASSERT_HANDLE(command_list);        //Not valid command_list, this command must be called within a frame graph execution callback
	command_list->context->device->platform->bind_mesh_index_buffer(command_list, layer);
}
//-- End Command_buffer_functions

//-- Exteneral helper functions
#ifdef GLFW_VERSION_MAJOR
#ifdef _WIN32
#ifndef GLFW_EXPOSE_NATIVE_WIN32
#define GLFW_EXPOSE_NATIVE_WIN32
#endif
#include <GLFW/glfw3native.h>
#elif defined(__linux__)
#ifndef GLFW_EXPOSE_NATIVE_X11
#define GLFW_EXPOSE_NATIVE_X11
#endif
#include <GLFW/glfw3native.h>
#endif

zest_device zest_implglfw_CreateVulkanDevice(zest_bool enable_validation) {
	if (!glfwInit()) {
		return 0;
	}

	zest_uint count;
	const char **glfw_extensions = glfwGetRequiredInstanceExtensions(&count);

	//Create the device that serves all vulkan based contexts
	zest_device_builder device_builder = zest_BeginVulkanDeviceBuilder();
	zest_AddDeviceBuilderExtensions(device_builder, glfw_extensions, count);
	zest_SetDeviceBuilderMemoryPoolSize(device_builder, zloc__MEGABYTE(32));
	if (enable_validation) {
		zest_AddDeviceBuilderValidation(device_builder);
		zest_DeviceBuilderLogToConsole(device_builder);
		zest_DeviceBuilderPrintMemoryInfo(device_builder);
	}
	zest_device device = zest_EndDeviceBuilder(device_builder);

	return device;
}

zest_window_data_t zest_implglfw_CreateWindow( int x, int y, int width, int height, zest_bool maximised, const char *title) {
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	if (maximised) {
		glfwWindowHint(GLFW_MAXIMIZED, GLFW_TRUE);
	}

	GLFWwindow *window_handle = glfwCreateWindow(width, height, title, 0, 0);
	if (!maximised) {
		glfwSetWindowPos(window_handle, x, y);
	}

	zest_window_data_t window_handles = { 0 };
	window_handles.width = width;
	window_handles.height = height;

	if (maximised) {
		glfwGetWindowSize(window_handle, &window_handles.width, &window_handles.height);
	}

	window_handles.window_handle = window_handle;
	#if defined(_WIN32)
	window_handles.native_handle = (void*)glfwGetWin32Window(window_handle);
	window_handles.display = GetModuleHandle(NULL);
	#endif

	window_handles.window_sizes_callback = zest_implglfw_GetWindowSizeCallback;

	return window_handles;
}

void zest_implglfw_GetWindowSizeCallback(zest_window_data_t *window_data, int *fb_width, int *fb_height, int *window_width, int *window_height) {
	GLFWwindow *handle = (GLFWwindow *)window_data->window_handle;
    glfwGetFramebufferSize(handle, fb_width, fb_height);
	glfwGetWindowSize(handle, window_width, window_height);
}

zest_bool zest_implglfw_WindowIsFocused(void *window_handle) {
	GLFWwindow *handle = (GLFWwindow *)window_handle;
	return (zest_bool)glfwGetWindowAttrib(handle, GLFW_FOCUSED);
}

void zest_implglfw_DestroyWindow(zest_context context) {
	GLFWwindow *handle = (GLFWwindow *)zest_Window(context);
	glfwDestroyWindow(handle);
}

#endif // GLFW_VERSION_MAJOR
// --- End GLFW Utilities ----------------------------------------------------

// --- SDL2_Utilities --------------------------------------------------------
// To use this implementation, include <SDL.h> before this file

#if defined(ZEST_VULKAN_IMPLEMENTATION) && defined(SDL_MAJOR_VERSION)
#include <SDL_vulkan.h>
#endif

#if defined(SDL_MAJOR_VERSION)
#include <SDL_syswm.h>

zest_device zest_implsdl2_CreateVulkanDevice(zest_window_data_t *window_data, zest_bool enable_validation) {
	unsigned int count = 0;
	SDL_Vulkan_GetInstanceExtensions((SDL_Window*)window_data->window_handle, &count, NULL);
	const char** sdl_extensions = (const char**)malloc(sizeof(const char*) * count);
	SDL_Vulkan_GetInstanceExtensions((SDL_Window*)window_data->window_handle, &count, sdl_extensions);

	// Create the device that serves all vulkan based contexts
	zest_device_builder device_builder = zest_BeginVulkanDeviceBuilder();
	zest_AddDeviceBuilderExtensions(device_builder, sdl_extensions, count);
	if (enable_validation) {
		zest_AddDeviceBuilderValidation(device_builder);
		zest_DeviceBuilderLogToConsole(device_builder);
	}
	zest_device device = zest_EndDeviceBuilder(device_builder);

	// Clean up the extensions array
	free(sdl_extensions);
	return device;
}

zest_window_data_t zest_implsdl2_CreateWindow(int x, int y, int width, int height, zest_bool maximised, const char *title) {
    Uint32 flags = 0;
#if defined(ZEST_VULKAN_IMPLEMENTATION)
    flags = SDL_WINDOW_VULKAN;
#elif defined(ZEST_IMPLEMENT_DX12)
    // flags = SDL_WINDOW_DX12; // Placeholder for DX12
#elif defined(ZEST_IMPLEMENT_METAL)
    // flags = SDL_WINDOW_METAL; // Placeholder for Metal
#endif

    if (maximised) {
        flags |= SDL_WINDOW_MAXIMIZED;
    }

    SDL_Window *window_handle = SDL_CreateWindow(title, x, y, width, height, flags);

    zest_window_data_t window_data = { 0 };
    window_data.width = width;
    window_data.height = height;

    if (maximised) {
        SDL_GetWindowSize(window_handle, &window_data.width, &window_data.height);
    }

    window_data.window_handle = window_handle;

    SDL_SysWMinfo wmi;
    SDL_VERSION(&wmi.version);
    if (!SDL_GetWindowWMInfo(window_handle, &wmi)) {
        // Handle error, for now returning partially filled struct
        return window_data;
    }

#if defined(_WIN32)
    window_data.native_handle = wmi.info.win.window;
    window_data.display = wmi.info.win.hinstance;
#elif defined(__linux__)
#if defined(ZEST_VULKAN_IMPLEMENTATION)
    window_data.native_handle = (void*)(uintptr_t)wmi.info.x11.window;
    window_data.display = wmi.info.x11.display;
#endif
#endif

    window_data.window_sizes_callback = zest_implsdl2_GetWindowSizeCallback;

    return window_data;
}

void zest_implsdl2_GetWindowSizeCallback(zest_window_data_t *window_data, int *fb_width, int *fb_height, int *window_width, int *window_height) {
#if defined(ZEST_VULKAN_IMPLEMENTATION)
	SDL_Vulkan_GetDrawableSize((SDL_Window*)window_data->window_handle, fb_width, fb_height);
#elif defined(ZEST_IMPLEMENT_DX12)
    // DX12 equivalent
#elif defined(ZEST_IMPLEMENT_METAL)
    // Metal equivalent
#endif
	SDL_GetWindowSize((SDL_Window*)window_data->window_handle, window_width, window_height);
}

void zest_implsdl2_DestroyWindow(zest_context context) {
	SDL_Window *handle = (SDL_Window *)zest_Window(context);
	SDL_DestroyWindow(handle);
}

void zest_implsdl2_SetWindowMode(zest_context context, zest_window_mode mode) {
	switch (mode) {
		case zest_window_mode_bordered: 
			SDL_SetWindowFullscreen((SDL_Window *)zest_Window(context), (SDL_bool)ZEST_FALSE);
			SDL_SetWindowBordered((SDL_Window *)zest_Window(context), (SDL_bool)ZEST_TRUE);
			SDL_SetWindowPosition((SDL_Window *)zest_Window(context), 50, 50);
			break;
		case zest_window_mode_borderless: 
			SDL_SetWindowFullscreen((SDL_Window *)zest_Window(context), (SDL_bool)ZEST_FALSE);
			SDL_SetWindowBordered((SDL_Window *)zest_Window(context), (SDL_bool)ZEST_FALSE);
			break;
		case zest_window_mode_fullscreen: 
			SDL_SetWindowFullscreen((SDL_Window *)zest_Window(context), (SDL_bool)ZEST_TRUE);
			break;
		case zest_window_mode_fullscreen_borderless: 
			SDL_SetWindowFullscreen((SDL_Window *)zest_Window(context), (SDL_bool)ZEST_FALSE);
			SDL_SetWindowBordered((SDL_Window *)zest_Window(context), (SDL_bool)ZEST_FALSE);
			SDL_Rect bounds;
			SDL_GetDisplayBounds(0, &bounds);
			SDL_SetWindowPosition((SDL_Window *)zest_Window(context), bounds.x, bounds.y);
			SDL_SetWindowSize((SDL_Window *)zest_Window(context), bounds.w, bounds.h);
			break;
	}
}

#endif // SDL_MAJOR_VERSION
// --- End SDL2 Utilities ----------------------------------------------------

//End Zest_implementation

#endif

#endif

#include "zest_utilities.h"

#ifdef ZEST_VULKAN_IMPLEMENTATION
#include "zest_vulkan.h"
#endif
#ifdef ZEST_D3D12_IMPLEMENTATION
#endif
#ifdef ZEST_METAL_IMPLEMENTATION
#endif

#endif // ! ZEST_POCKET_RENDERER
