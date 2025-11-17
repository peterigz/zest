//Zest - A Lightweight Renderer
#ifndef ZEST_RENDERER_H
#define ZEST_RENDERER_H

/*
    Zest Pocket Renderer

    Sections in this header file, you can search for the following keywords to jump to that section :
 * 
	[Zloc_header - Zloc_implementation] Header for the TLSF allocator
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
        [Renderer_functions]
        [Draw_layer_internal_functions]
        [Image_internal_functions]
        [General_layer_internal_functions]
        [Mesh_layer_internal_functions]
        [Misc_Helper_Functions]
        [Pipeline_Helper_Functions]
        [Buffer_allocation_functions]
        [Maintenance_functions]
        [Descriptor_set_functions]
        [Device_set_up]
        [App_initialise_and_run_functions]
		[Enum_to_string_functions]

    --API functions
    [Essential_setup_functions]         Functions for initialising Zest
    [Platform_Helper_Functions]         Just general helper functions for working with the platform APIs if you're doing more custom stuff
    [Pipeline_related_helpers]   		Helper functions for setting up your own pipeline_templates
    [Platform_dependent_callbacks]      These are used depending one whether you're using glfw, sdl or just the os directly
    [Buffer_functions]                  Functions for creating and using gpu buffers
    [General_Math_helper_functions]     Vector and matrix math functions
    [Camera_helpers]                    Functions for setting up and using a camera including frustom and screen ray etc. 
    [Images_and_textures]               Load and setup images for using in textures accessed on the GPU
    [Swapchain_helpers]                 General swapchain helpers to get, set clear color etc.
    [Fonts]                             Basic functions for loading MSDF fonts
    [Draw_Layers_API]                   General helper functions for layers
    [Draw_mesh_layers]                  Functions for drawing the builtin mesh layer pipeline
    [Draw_instance_mesh_layers]         Functions for drawing the builtin instance mesh layer pipeline
    [Compute_shaders]                   Functions for setting up your own custom compute shaders
    [Events_and_States]                 Just one function for now
    [Timer_functions]                   High resolution timer functions
    [General_Helper_functions]          General API functions - get screen dimensions etc 
    [Debug_Helpers]                     Functions for debugging and outputting queues to the console.
  	[Command_buffer_functions]          GPU command buffer commands

 	--Implementations 
 	[Zloc_allocator] 
*/

#define ZEST_DEBUGGING
#define ZLOC_THREAD_SAFE
#define ZLOC_EXTRA_DEBUGGING
#define ZLOC_OUTPUT_ERROR_MESSAGES
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

#include <stdlib.h>
#include <stddef.h>
#include <assert.h>
#include <stdint.h>
#if !defined (ZLOC_ASSERT)
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
#include <stdio.h>
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
ZLOC_API void *zloc_AllocateRemote(zloc_allocator *allocator, zloc_size remote_size);
ZLOC_API zloc_size zloc_CalculateRemoteBlockPoolSize(zloc_allocator *allocator, zloc_size remote_pool_size);
ZLOC_API void zloc_AddRemotePool(zloc_allocator *allocator, void *block_memory, zloc_size block_memory_size, zloc_size remote_pool_size);
ZLOC_API void* zloc_BlockUserExtensionPtr(const zloc_header *block);
ZLOC_API void* zloc_AllocationFromExtensionPtr(const void *block);
typedef struct zloc_linear_allocator_t {
	zloc_size buffer_size;
	zloc_size current_offset;
} zloc_linear_allocator_t;
zloc_linear_allocator_t *zloc_InitialiseLinearAllocator(void *memory, zloc_size size);
void zloc_ResetLinearAllocator(zloc_linear_allocator_t *allocator);
void *zloc_LinearAllocation(zloc_linear_allocator_t *allocator, zloc_size size_requested);
zloc_size zloc_GetMarker(zloc_linear_allocator_t *allocator);
void zloc_ResetToMarker(zloc_linear_allocator_t *allocator, zloc_size marker);

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
zloc_size zloc__get_remote_size(const zloc_header *block1);
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

#include <math.h>
#include <float.h>
#include <string.h>
#include <errno.h>

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

#include <stdio.h>
#include <stdlib.h>

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
#define ZEST_TEST_MODE


#else
#pragma message("ZEST_ASSERT is disabled because NDEBUG is defined.")
#define ZEST_ASSERT(...) (void)0
#define ZEST_ASSERT_TILING_FORMAT(format) void(0)
#endif

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
#define ZEST__NEXTPOW2(x) pow(2, ceil(log(x) / log(2)));
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
static const char *zest_message_multiple_swapchain_usage = "Graph Compile Error in Frame Graph [%s]: This is the second time that the swap chain is used as output, this should not happen. Check that the passes that use the swapchain as output have exactly the same set of other ouputs so that the passes can be properly grouped together.";
static const char *zest_message_resource_added_as_ouput_more_than_once = "Graph Compile Error in Frame Graph [%s]: A resource should only have one producer in a valid graph. Check to make sure you haven't added the same output to a pass more than once";
static const char *zest_message_resource_should_be_imported = "Graph Compile Error in Frame Graph [%s]: ";
static const char *zest_message_cannot_queue_for_execution = "Could not queue frame graph [%s] for execution as there were errors found in the graph. Check the log.";

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

#ifndef ZEST_WARNING_COLOR
#define ZEST_WARNING_COLOR "\033[38;5;208m"
#endif

#ifndef ZEST_NOTICE_COLOR
#define ZEST_NOTICE_COLOR "\033[0m"
#endif

#define ZEST_PRINT(message_f, ...) printf(message_f"\n", ##__VA_ARGS__)

#ifdef ZEST_OUTPUT_WARNING_MESSAGES
#include <stdio.h>
#define ZEST_PRINT_WARNING(message_f, ...) printf(message_f"\n\033[0m", __VA_ARGS__)
#else
#define ZEST_PRINT_WARNING(message_f, ...)
#endif

#ifdef ZEST_OUTPUT_NOTICE_MESSAGES
#include <stdio.h>
#define ZEST_PRINT_NOTICE(message_f, ...) printf(message_f"\n\033[0m", __VA_ARGS__)
#else
#define ZEST_PRINT_NOTICE(message_f, ...)
#endif

#define ZEST_NL u8"\n"
#define ZEST_TAB u8"\t"
#define ZEST_LOG(log_file, message, ...) if (log_file) fprintf(log_file, message, ##__VA_ARGS__)
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

#ifndef ZEST_MAX_DEVICE_MEMORY_POOLS
#define ZEST_MAX_DEVICE_MEMORY_POOLS 64
#endif

#define ZEST_NULL 0
//For each frame in flight macro
#define zest_ForEachFrameInFlight(index) for (unsigned int index = 0; index != ZEST_MAX_FIF; ++index)

//Typedefs_for_numbers

typedef unsigned int zest_uint;
typedef unsigned long zest_long;
typedef int zest_index;
typedef unsigned long long zest_ull;
typedef uint16_t zest_u16;
typedef uint64_t zest_u64;
typedef uint64_t zest_handle;
typedef zest_uint zest_id;
typedef zest_uint zest_millisecs;
typedef zest_uint zest_thread_access;
typedef zest_ull zest_microsecs;
typedef zest_ull zest_key;
typedef uint64_t zest_size;
typedef unsigned char zest_byte;
typedef unsigned int zest_bool;
typedef char* zest_file;

//Handles. These are pointers that remain stable until the object is freed.
#define ZEST__MAKE_HANDLE(handle) typedef struct handle##_t* handle;

#define ZEST__MAKE_USER_HANDLE(handle) typedef struct { zest_handle value; zest_resource_store_t *store; } handle##_handle;
#define ZEST_HANDLE_INDEX(handle) (zest_uint)(handle & 0xFFFFFFFF)
#define ZEST_HANDLE_GENERATION(handle) (zest_uint)((handle & 0xFFFFFF00000000) >> 32ull)
#define ZEST_HANDLE_TYPE(handle) (zest_handle_type)((handle & 0xFF00000000000000) >> 56ull)

#define ZEST_CREATE_HANDLE(type, generation, index) (((zest_u64)type << 56ull) + ((zest_u64)generation << 32ull) + index)

//For allocating a new object with handle. Only used internally.
#define ZEST__NEW(allocator, type) ZEST__ALLOCATE(allocator, sizeof(type##_t))
#define ZEST__NEW_ALIGNED(allocator, type, alignment) ZEST__ALLOCATE_ALIGNED(allocator, sizeof(type##_t), alignment)

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
#include <direct.h>
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
//We'll just use glfw on mac for now. Can maybe add basic cocoa windows later
#include <GLFW/glfw3.h>
#include <sys/stat.h>
#include <sys/types.h>
#define ZEST_ALIGN_PREFIX(v)
#define ZEST_ALIGN_AFFIX(v)  __attribute__((aligned(16)))
#define ZEST_PROTOTYPE void
#define zest_snprintf(buffer, bufferSize, format, ...) snprintf(buffer, bufferSize, format, __VA_ARGS__)
#define zest_strcat(left, size, right) strcat(left, right)
#define zest_strcpy(left, size, right) strcpy(left, right)
#define zest_strerror(buffer, size, error) strerror_r(buffer, size, error)
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
	zest_format_depth = -1,
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
	zest_fence_status_success,
	zest_fence_status_timeout,
	zest_fence_status_error
} zest_fence_status;

typedef zest_uint zest_set_layout_builder_flags;
typedef zest_uint zest_set_layout_flags;

typedef enum zest_frustum_side { zest_LEFT = 0, zest_RIGHT = 1, zest_TOP = 2, zest_BOTTOM = 3, zest_BACK = 4, zest_FRONT = 5 } zest_frustum_size;

typedef enum {
	ZEST_ALL_MIPS = 0xffffffff,
	ZEST_QUEUE_COUNT = 3,
	ZEST_GRAPHICS_QUEUE_INDEX = 0,
	ZEST_COMPUTE_QUEUE_INDEX = 1,
	ZEST_TRANSFER_QUEUE_INDEX = 2,
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
	zest_handle_type_shader_resources,
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
	zest_struct_type_shader_resources = 12 << 16,
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
	zest_struct_type_resource_store = 48 << 16
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
	zest__required_extension_names_count = 3,
	#else
	zest__required_extension_names_count = 2,
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

typedef enum zest_init_flag_bits {
	zest_init_flag_none = 0,
	zest_init_flag_maximised = 1 << 1,
	zest_init_flag_cache_shaders = 1 << 2,
	zest_init_flag_enable_vsync = 1 << 3,
	zest_init_flag_enable_fragment_stores_and_atomics = 1 << 4,
	zest_init_flag_enable_validation_layers = 1 << 6,
	zest_init_flag_enable_validation_layers_with_sync = 1 << 7,
	zest_init_flag_enable_validation_layers_with_best_practices = 1 << 8,
	zest_init_flag_log_validation_errors_to_console = 1 << 9,
	zest_init_flag_log_validation_errors_to_memory = 1 << 10,
} zest_init_flag_bits;

typedef zest_uint zest_init_flags;

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

typedef enum zest_memory_pool_flags {
	zest_memory_pool_flag_none,
	zest_memory_pool_flag_single_buffer,
} zest_memory_pool_flags;

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
	zest_buffer_type_staging,
	zest_buffer_type_vertex,
	zest_buffer_type_index,
	zest_buffer_type_uniform,
	zest_buffer_type_storage,
	zest_buffer_type_indirect,
	zest_buffer_type_vertex_storage,
	zest_buffer_type_index_storage
} zest_buffer_type;

typedef enum {
	zest_memory_usage_gpu_only,
	zest_memory_usage_cpu_to_gpu,
	zest_memory_usage_gpu_to_cpu,
} zest_memory_usage;

typedef enum zest_texture_storage_type {
	zest_texture_storage_type_packed,                        //Pack all of the images into a sprite sheet and onto multiple layers in an image array on the GPU
	zest_texture_storage_type_bank,                          //Packs all images one per layer, best used for repeating textures or color/bump/specular etc
	zest_texture_storage_type_sprite_sheet,                  //Packs all the images onto a single layer spritesheet
	zest_texture_storage_type_single,                        //A single image texture
	zest_texture_storage_type_storage,                       //A storage texture useful for manipulation and other things in a compute shader
	zest_texture_storage_type_stream,                        //A storage texture that you can update every frame
	zest_texture_storage_type_render_target,                 //Texture storage for a render target sampler, so that you can draw the target onto another render target
	zest_texture_storage_type_cube_map                       //Texture is a cube map
} zest_texture_storage_type;

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
} zest_pass_flag_bits;

typedef enum zest_pass_type {
	zest_pass_type_graphics = 1,
	zest_pass_type_compute = 2,
	zest_pass_type_transfer = 3,
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
	zest_pipeline_invalid = 1 << 2        //The pipeline had validation errors when it was build and may be invalid
} zest_pipeline_set_flag_bits;

typedef zest_uint zest_pipeline_set_flags;

typedef enum zest_supported_pipeline_stages {
	zest_pipeline_no_stage = 0,
	zest_pipeline_vertex_input_stage = 1 << 2,
	zest_pipeline_vertex_stage = 1 << 3,
	zest_pipeline_fragment_stage = 1 << 7,
	zest_pipeline_compute_stage = 1 << 11,
	zest_pipeline_transfer_stage = 1 << 12
} zest_supported_pipeline_stages;

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
	zest_report_unconnected_resource,
	zest_report_pass_culled,
	zest_report_resource_culled,
	zest_report_invalid_layer,
	zest_report_cyclic_dependency,
	zest_report_invalid_render_pass,
	zest_report_render_pass_skipped,
	zest_report_expecting_swapchain_usage,
	zest_report_bindless_indexes,
	zest_report_invalid_reference_counts,
	zest_report_missing_end_pass,
	zest_report_invalid_resource,
	zest_report_invalid_pass,
	zest_report_multiple_swapchains,
	zest_report_cannot_execute,
	zest_report_pipeline_invalid,
	zest_report_no_frame_graphs_to_execute,
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

static const int ZEST_STRUCT_IDENTIFIER = 0x4E57;
#define zest_INIT_MAGIC(struct_type) (struct_type | ZEST_STRUCT_IDENTIFIER);

#define ZEST_ASSERT_HANDLE(handle) ZEST_ASSERT(handle && (*((int*)handle) & 0xFFFF) == ZEST_STRUCT_IDENTIFIER)
#define ZEST_ASSERT_BUFFER_HANDLE(handle) ZEST_ASSERT(handle && (*((int*)((char*)handle + offsetof(zest_buffer_t, magic))) & 0xFFFF) == ZEST_STRUCT_IDENTIFIER)
#define ZEST_VALID_HANDLE(handle) (handle && (*((int*)handle) & 0xFFFF) == ZEST_STRUCT_IDENTIFIER)
#define ZEST_STRUCT_TYPE(handle) (*((int*)handle) & 0xFFFF0000)
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
typedef struct zest_set_layout_t zest_set_layout_t;
typedef struct zest_descriptor_set_t zest_descriptor_set_t;
typedef struct zest_descriptor_pool_t zest_descriptor_pool_t;
typedef struct zest_shader_resources_t zest_shader_resources_t;
typedef struct zest_uniform_buffer_t zest_uniform_buffer_t;
typedef struct zest_buffer_allocator_t zest_buffer_allocator_t;
typedef struct zest_compute_t zest_compute_t;
typedef struct zest_buffer_t zest_buffer_t;
typedef struct zest_device_memory_pool_t zest_device_memory_pool_t;
typedef struct zest_timer_t zest_timer_t;
typedef struct zest_shader_t zest_shader_t;
typedef struct zest_frame_graph_semaphores_t zest_frame_graph_semaphores_t;
typedef struct zest_frame_graph_t zest_frame_graph_t;
typedef struct zest_frame_graph_builder_t zest_frame_graph_builder_t;
typedef struct zest_pass_node_t zest_pass_node_t;
typedef struct zest_resource_node_t zest_resource_node_t;
typedef struct zest_queue_t zest_queue_t;
typedef struct zest_context_queue_t zest_context_queue_t;
typedef struct zest_execution_timeline_t zest_execution_timeline_t;
typedef struct zest_swapchain_t zest_swapchain_t;
typedef struct zest_output_group_t zest_output_group_t;
typedef struct zest_rendering_info_t zest_rendering_info_t;
typedef struct zest_mesh_t zest_mesh_t;
typedef struct zest_command_list_t zest_command_list_t;
typedef struct zest_resource_store_t zest_resource_store_t; 

//Backends
typedef struct zest_device_backend_t zest_device_backend_t;
typedef struct zest_context_backend_t zest_context_backend_t;
typedef struct zest_swapchain_backend_t zest_swapchain_backend_t;
typedef struct zest_queue_backend_t zest_queue_backend_t;
typedef struct zest_context_queue_backend_t zest_context_queue_backend_t;
typedef struct zest_command_list_backend_t zest_command_list_backend_t;
typedef struct zest_device_memory_pool_backend_t zest_device_memory_pool_backend_t;
typedef struct zest_buffer_backend_t zest_buffer_backend_t;
typedef struct zest_uniform_buffer_backend_t zest_uniform_buffer_backend_t;
typedef struct zest_descriptor_pool_backend_t zest_descriptor_pool_backend_t;
typedef struct zest_descriptor_set_backend_t zest_descriptor_set_backend_t;
typedef struct zest_set_layout_backend_t zest_set_layout_backend_t;
typedef struct zest_pipeline_backend_t zest_pipeline_backend_t;
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
ZEST__MAKE_HANDLE(zest_set_layout)
ZEST__MAKE_HANDLE(zest_descriptor_set)
ZEST__MAKE_HANDLE(zest_shader_resources)
ZEST__MAKE_HANDLE(zest_uniform_buffer)
ZEST__MAKE_HANDLE(zest_buffer_allocator)
ZEST__MAKE_HANDLE(zest_descriptor_pool)
ZEST__MAKE_HANDLE(zest_compute)
ZEST__MAKE_HANDLE(zest_buffer)
ZEST__MAKE_HANDLE(zest_device_memory_pool)
ZEST__MAKE_HANDLE(zest_shader)
ZEST__MAKE_HANDLE(zest_queue)
ZEST__MAKE_HANDLE(zest_context_queue)
ZEST__MAKE_HANDLE(zest_execution_timeline)
ZEST__MAKE_HANDLE(zest_frame_graph_semaphores)
ZEST__MAKE_HANDLE(zest_swapchain)
ZEST__MAKE_HANDLE(zest_frame_graph)
ZEST__MAKE_HANDLE(zest_frame_graph_builder)
ZEST__MAKE_HANDLE(zest_pass_node)
ZEST__MAKE_HANDLE(zest_resource_node)
ZEST__MAKE_HANDLE(zest_output_group);
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
ZEST__MAKE_HANDLE(zest_device_memory_pool_backend)
ZEST__MAKE_HANDLE(zest_buffer_backend)
ZEST__MAKE_HANDLE(zest_uniform_buffer_backend)
ZEST__MAKE_HANDLE(zest_descriptor_pool_backend)
ZEST__MAKE_HANDLE(zest_descriptor_set_backend)
ZEST__MAKE_HANDLE(zest_set_layout_backend)
ZEST__MAKE_HANDLE(zest_pipeline_backend)
ZEST__MAKE_HANDLE(zest_compute_backend)
ZEST__MAKE_HANDLE(zest_image_backend)
ZEST__MAKE_HANDLE(zest_image_view_backend)
ZEST__MAKE_HANDLE(zest_sampler_backend)
ZEST__MAKE_HANDLE(zest_submission_batch_backend)
ZEST__MAKE_HANDLE(zest_execution_backend)
ZEST__MAKE_HANDLE(zest_execution_timeline_backend)
ZEST__MAKE_HANDLE(zest_execution_barriers_backend)
ZEST__MAKE_HANDLE(zest_set_layout_builder_backend)

ZEST__MAKE_USER_HANDLE(zest_shader_resources)
ZEST__MAKE_USER_HANDLE(zest_image)
ZEST__MAKE_USER_HANDLE(zest_image_view)
ZEST__MAKE_USER_HANDLE(zest_sampler)
ZEST__MAKE_USER_HANDLE(zest_image_view_array)
ZEST__MAKE_USER_HANDLE(zest_uniform_buffer)
ZEST__MAKE_USER_HANDLE(zest_layer)
ZEST__MAKE_USER_HANDLE(zest_shader)
ZEST__MAKE_USER_HANDLE(zest_compute)

// --Private structs with inline functions
typedef struct zest_queue_family_indices {
	zest_uint graphics_family_index;
	zest_uint transfer_family_index;
	zest_uint compute_family_index;
} zest_queue_family_indices;

// --Pocket Dynamic Array
typedef struct zest_vec {
	int magic;  //For allocation tracking
	int id;     //and finding leaks.
	zest_uint current_size;
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
#define zest_vec_last_index(T) (zest__vec_header(T)->current_size - 1)
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
#define zest_vec_grow(allocator, T) ((!(T) || (zest__vec_header(T)->current_size == zest__vec_header(T)->capacity)) ? T = zest__vec_reserve_wrapper(allocator, T, sizeof(*T), (T ? zest__grow_capacity(T, zest__vec_header(T)->current_size) : 8)) : (void)0)
#define zest_vec_linear_grow(allocator, T) ((!(T) || (zest__vec_header(T)->current_size == zest__vec_header(T)->capacity)) ? T = zest__vec_linear_reserve_wrapper(allocator, (T), sizeof(*T), (T ? zest__grow_capacity(T, zest__vec_header(T)->current_size) : 16)) : (void)0)
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
#define zest_vec_linear_push(allocator, T, value) (zest_vec_linear_grow(allocator, T), (T)[zest__vec_header(T)->current_size++] = value)
#define zest_vec_insert(allocator, T, location, value) do { ptrdiff_t offset = location - T; zest_vec_grow(allocator, T); if (offset < zest_vec_size(T)) memmove(T + offset + 1, T + offset, ((size_t)zest_vec_size(T) - offset) * sizeof(*T)); T[offset] = value; zest_vec_bump(T); } while (0)
#define zest_vec_linear_insert(allocator, T, location, value) do { ptrdiff_t offset = location - T; zest_vec_linear_grow(allocator, T); if (offset < zest_vec_size(T)) memmove(T + offset + 1, T + offset, ((size_t)zest_vec_size(T) - offset) * sizeof(*T)); T[offset] = value; zest_vec_bump(T); } while (0)
#define zest_vec_erase(T, location) do { ptrdiff_t offset = location - T; ZEST_ASSERT(T && offset >= 0 && location < zest_vec_end(T)); memmove(T + offset, T + offset + 1, ((size_t)zest_vec_size(T) - offset) * sizeof(*T)); zest_vec_clip(T); } while (0)
#define zest_vec_erase_range(T, it, it_last) do { ZEST_ASSERT(T && it >= T && it < zest_vec_end(T)); const ptrdiff_t count = it_last - it; const ptrdiff_t off = it - T; memmove(T + off, T + off + count, ((size_t)zest_vec_size(T) - (size_t)off - count) * sizeof(*T)); zest_vec_trim(T, (zest_uint)count); } while (0)
// --end of pocket dynamic array

// --Pocket_bucket_array
// The main purpose of this bucket array is to produce stable pointers for render graph resources
typedef struct zest_bucket_array_t {
	void** buckets;             // A zest_vec of pointers to individual buckets
	zest_context context;
	zest_uint bucket_capacity;  // Number of elements each bucket can hold
	zest_uint current_size;       // Total number of elements across all buckets
	zest_uint element_size;     // The size of a single element
} zest_bucket_array_t;

ZEST_PRIVATE inline void zest__initialise_bucket_array(zest_context context, zest_bucket_array_t *array, zest_uint element_size, zest_uint bucket_capacity);
ZEST_PRIVATE inline void zest__free_bucket_array(zest_bucket_array_t *array);
ZEST_API_TMP inline void *zest__bucket_array_get(zest_bucket_array_t *array, zest_uint index);
ZEST_PRIVATE inline void *zest__bucket_array_add(zest_bucket_array_t *array);
ZEST_PRIVATE inline void *zest__bucket_array_linear_add(zloc_linear_allocator_t *allocator, zest_bucket_array_t *array);

#define zest_bucket_array_init(context, array, T, cap) zest__initialise_bucket_array(context, array, sizeof(T), cap)
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
	void *data;
	zest_uint current_size;
	zest_uint capacity;
	zest_uint struct_size;
	zest_uint alignment;
	zest_uint *generations;
	zest_uint *free_slots;
	zloc_allocator *allocator;
	void *origin;
} zest_resource_store_t;

ZEST_PRIVATE void zest__free_store(zest_resource_store_t *store);
ZEST_PRIVATE void zest__reserve_store(zest_resource_store_t *store, zest_uint new_capacity);
ZEST_PRIVATE void zest__clear_store(zest_resource_store_t *store);
ZEST_PRIVATE zest_uint zest__grow_store_capacity(zest_resource_store_t *store, zest_uint size);
ZEST_PRIVATE void zest__resize_store(zest_resource_store_t *store, zest_uint new_size);
ZEST_PRIVATE void zest__resize_bytes_store(zest_resource_store_t *store, zest_uint new_size);
ZEST_PRIVATE zest_uint zest__size_in_bytes_store(zest_resource_store_t *store);
ZEST_PRIVATE zest_handle zest__add_store_resource(zest_resource_store_t *store);
ZEST_PRIVATE void zest__remove_store_resource(zest_resource_store_t *store, zest_handle handle);
ZEST_PRIVATE void zest__initialise_store(zloc_allocator *allocator, void *origin, zest_resource_store_t *store, zest_uint struct_size);


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
	zest_index index;
} zest_hash_pair;

#ifndef ZEST_HASH_SEED
#define ZEST_HASH_SEED 0xABCDEF99
#endif
ZEST_PRIVATE zest_hash_pair* zest__lower_bound(zest_hash_pair *map, zest_key key) { zest_hash_pair *first = map; zest_hash_pair *last = map ? zest_vec_end(map) : 0; size_t count = (size_t)(last - first); while (count > 0) { size_t count2 = count >> 1; zest_hash_pair* mid = first + count2; if (mid->key < key) { first = ++mid; count -= count2 + 1; } else { count = count2; } } return first; }
ZEST_PRIVATE void zest__map_realign_indexes(zest_hash_pair *map, zest_index index) { zest_vec_foreach(i, map) { if (map[i].index < index) continue; map[i].index--; } }
ZEST_PRIVATE zest_index zest__map_get_index(zest_hash_pair *map, zest_key key) { zest_hash_pair *it = zest__lower_bound(map, key); return (it == zest_vec_end(map) || it->key != key) ? -1 : it->index; }
#define zest_map_hash(hash_map, name) zest_Hash(name, strlen(name), ZEST_HASH_SEED)
#define zest_map_hash_ptr(hash_map, ptr, size) zest_Hash(ptr, size, ZEST_HASH_SEED)
#define zest_hash_map(T) typedef struct { zest_hash_pair *map; T *data; zest_index *free_slots; zest_index last_index; }
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
#define zest_map_remove(allocator, hash_map, name) { zest_key key = zest_map_hash(hash_map, name); zest_hash_pair *it = zest__lower_bound(hash_map.map, key); zest_index index = it->index; zest_vec_erase(hash_map.map, it); zest_vec_push(allocator->device->allocator, hash_map.free_slots, index); }
#define zest_map_remove_key(allocator, hash_map, name) { zest_hash_pair *it = zest__lower_bound(hash_map.map, key); zest_index index = it->index; zest_vec_erase(hash_map.map, it); zest_vec_push(allocator->device->allocator, hash_map.free_slots, index); }
#define zest_map_last_index(hash_map) (hash_map.last_index)
#define zest_map_size(hash_map) (hash_map.map ? zest__vec_header(hash_map.data)->current_size : 0)
#define zest_map_clear(hash_map) zest_vec_clear(hash_map.map); zest_vec_clear(hash_map.data); zest_vec_clear(hash_map.free_slots)
#define zest_map_free(allocator, hash_map) if (hash_map.map) ZEST__FREE(allocator, zest__vec_header(hash_map.map)); if (hash_map.data) ZEST__FREE(allocator, zest__vec_header(hash_map.data)); if (hash_map.free_slots) ZEST__FREE(allocator, zest__vec_header(hash_map.free_slots)); hash_map.data = 0; hash_map.map = 0; hash_map.free_slots = 0
//Use inside a for loop to iterate over the map. The loop index should be be the index into the map array, to get the index into the data array.
#define zest_map_index(hash_map, i) (hash_map.map[i].index)
#define zest_map_foreach(index, hash_map) zest_vec_foreach(index, hash_map.map)
// --End pocket hash map

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

ZEST_API void zest__add_report(zest_context context, zest_report_category category, int line_number, const char *file_name, const char *function_name, const char *entry, ...);

#ifdef ZEST_DEBUGGING
#define ZEST_FRAME_LOG(context, message_f, ...) zest__log_entry(context, message_f, ##__VA_ARGS__)
#define ZEST_RESET_LOG() zest__reset_log()
#define ZEST__MAYBE_REPORT(context, condition, category, entry, ...) if (condition) { zest__add_report(context, category, __LINE__, __FILE__, __FUNCTION__, entry, ##__VA_ARGS__); }
#define ZEST__REPORT(context, category, entry, ...) zest__add_report(context, category, __LINE__, __FILE__, __FUNCTION__, entry, ##__VA_ARGS__)
#else
#define ZEST_FRAME_LOG(message_f, ...)
#define ZEST_RESET_LOG()
#define ZEST__REPORT()
#endif

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

// Forward declaration
struct zest_work_queue_t;

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

typedef struct zest_resource_identifier_t {
	int magic;
	zest_resource_type type;
} zest_resource_identifier_t;

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

// --Buffer Management

//A simple buffer struct for creating and mapping GPU memory
typedef struct zest_device_memory_pool_t {
	int magic;
	zest_context context;
	zest_memory_pool_flags flags;
	zest_device_memory_pool_backend backend;
	zest_size size;
	zest_size minimum_allocation_size;
	zest_size alignment;
	zest_uint memory_type_index;
	void* mapped;
	const char *name;
} zest_device_memory_pool_t;

typedef struct zest_buffer_info_t {
	zest_image_usage_flags image_usage_flags;
	zest_buffer_usage_flags buffer_usage_flags;                    //The usage state_flags of the memory block.
	zest_memory_property_flags property_flags;
	zest_uint memory_type_bits;
	zest_size alignment;
	zest_uint frame_in_flight;
	zest_memory_pool_flags flags;
	//This is an important field to avoid race conditions. Sometimes you may want to only update a buffer
	//when data has changed, like a ui buffer or any buffer that's only updated inside a fixed time loop.
	//This means that the frame in flight index for those buffers is decoupled from the main render loop 
	//frame in flight (context->current_fif). This can create a situation where a decoupled buffer has the same buffer
	//as a transient buffer that's not decoupled. So what can happen is that a vkCmdDraw can use the same
	//buffer that was used in the last frame. This should never be possible. A buffer that's used in one
	//frame - it's last usage should be a minimum of 2 frames ago which means that the frame in flight fence
	//can properly act as syncronization. Therefore you can add this flag to any buffer that needs to be
	//decoupled from the main frame in flight index which will force a unique buffer to be created that's 
	//separate from other buffer pools.
	//Note: I don't like this at all. Find another way.
	zest_uint unique_id;
} zest_buffer_info_t;

typedef struct zest_buffer_pool_size_t {
	const char *name;
	zest_size pool_size;
	zest_size minimum_allocation_size;
} zest_buffer_pool_size_t;

typedef struct zest_buffer_usage_t {
	zest_buffer_usage_flags buffer_usage_flags;
	zest_memory_property_flags property_flags;
	zest_image_usage_flags image_flags;
} zest_buffer_usage_t;

zest_hash_map(zest_buffer_pool_size_t) zest_map_buffer_pool_sizes;

typedef void* zest_pool_range;

typedef struct zest_buffer_details_t {
	zest_size size;
	zest_size memory_offset;
	zest_device_memory_pool memory_pool;
	zest_buffer_allocator buffer_allocator;
	zest_size memory_in_use;
} zest_buffer_details_t;

typedef struct zest_buffer_t {
	zest_size size;
	zest_size memory_offset;
	zest_size buffer_offset;
	int magic;
	zest_context context;
	zest_buffer_backend backend;
	zest_device_memory_pool memory_pool;
	zest_buffer_allocator buffer_allocator;
	zest_size memory_in_use;
	zest_uint array_index;
	void *data;
	void *end;
	//For releasing/acquiring in the frame graph:
	zest_uint owner_queue_family;
} zest_buffer_t;

typedef struct zest_uniform_buffer_t {
	int magic;
	zest_uniform_buffer_handle handle;
	zest_buffer buffer[ZEST_MAX_FIF];
	zest_descriptor_set descriptor_set[ZEST_MAX_FIF];
	zest_set_layout set_layout;
	zest_uniform_buffer_backend backend;
} zest_uniform_buffer_t;

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

typedef struct zest_image_view_array_t {
	int magic;
	zest_image_view_array_handle handle;
	zest_image image;
	zest_uint count;
	zest_image_view_t *views;
} zest_image_view_array_t;

typedef struct zest_image_view_t {
	zest_image image;
	zest_image_view_handle handle;
	zest_image_view_backend_t *backend;
} zest_image_view_t;

typedef struct zest_atlas_region_t {
	zest_uint top, left;         //the top and left pixel coordinates of the region in the layer
	zest_uint width;
	zest_uint height;
	zest_vec4 uv;                //UV coords 
	zest_u64 uv_packed;          //UV coords packed into 16bit floats
	zest_index layer_index;      //the layer index of the image when it's packed into an image/texture array
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

typedef struct zest_image_t {
	int magic;
	zest_image_handle handle;
	zest_image_backend backend;
	zest_buffer buffer;
	zest_uint bindless_index[zest_max_global_binding_number];
	zest_image_info_t info;
	zest_image_view default_view;
} zest_image_t;

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

typedef struct zest_sampler_t {
	int magic;
	zest_sampler_handle handle;
	zest_sampler_backend backend;
	zest_sampler_info_t create_info;
} zest_sampler_t;

typedef struct zest_execution_timeline_t {
	int magic;
	zest_execution_timeline_backend backend;
	zest_u64 current_value;
} zest_execution_timeline_t;

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

typedef void (*zest_get_window_sizes_callback)(void* window_handle, int* fb_width, int* fb_height, int* window_width, int* window_height);

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
	int width;
	int height;
	zest_get_window_sizes_callback window_sizes_callback;
} zest_window_data_t;

typedef struct zest_resource_usage_t {
	zest_resource_node resource_node;
	zest_pipeline_stage_flags stage_mask;   // Pipeline stages this usage pertains to
	zest_access_flags access_mask;          // access mask for barriers
	zest_image_layout image_layout;         // Required image layout if it's an image
	zest_image_aspect_flags aspect_flags;
	zest_resource_purpose purpose;
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
	zest_init_flags flags;
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
	const char *log_path;                               //path to the log to store log and validation messages
	const char *cached_shader_path;

	// User-provided list of required instance extensions (e.g., from GLFW)
	const char** required_instance_extensions;
} zest_device_builder_t;

typedef struct zest_create_info_t {
	const char *title;                                  //Title that shows in the window
	zest_size frame_graph_allocator_size;               //The size of the linear allocator used by render graphs to store temporary data
	int screen_width, screen_height;                    //Default width and height of the window that you open
	int screen_x, screen_y;                             //Default position of the window
	int virtual_width, virtual_height;                  //The virtial width/height of the viewport
	zest_millisecs fence_wait_timeout_ms;               //The amount of time the main loop fence should wait before timing out
	zest_millisecs max_fence_timeout_ms;                //The maximum amount of time to wait before giving up
	zest_format color_format;                   		//The format to use for the swapchain
	zest_init_flags flags;                              //Set flags to apply different initialisation options
	zest_uint maximum_textures;                         //The maximum number of textures you can load. 1024 is the default.
	zest_platform_type platform;
	zest_size allocator_capacity;
} zest_create_info_t;

zest_hash_map(zest_context_queue) zest_map_queue_value;

typedef struct zest_platform_memory_info_t {
	zest_millisecs timestamp;
	zest_uint context_info;
} zest_platform_memory_info_t;

zest_hash_map(const char *) zest_map_queue_names;
zest_hash_map(zest_text_t) zest_map_validation_errors;

typedef struct zest_queue_t {
	zest_uint family_index;
	zest_device_queue_type type;
	zest_queue_backend backend;
} zest_queue_t;

typedef struct zest_context_queue_t {
	//We decouple the frame in flight on the queue so that the counts don't get out of sync when the swap chain
	//can't be acquired for whatever reason.
	zest_uint fif;
	zest_u64 current_count[ZEST_MAX_FIF];
	zest_u64 signal_value;
	zest_bool has_waited;
	zest_uint next_buffer;
	zest_queue queue;
	zest_context_queue_backend backend;
} zest_context_queue_t;

typedef struct zest_binding_index_for_release_t {
	zest_set_layout layout;
	zest_uint binding_index;
	zest_uint binding_number;
} zest_binding_index_for_release_t;

typedef struct zest_device_destruction_queue_t {
	void **resources[ZEST_MAX_FIF];
} zest_device_destruction_queue_t;

typedef struct zest_context_destruction_queue_t {
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
zest_hash_map(zest_frame_graph_semaphores) zest_map_rg_semaphores;
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

typedef struct zest_device_t {
	int magic;
	zest_uint api_version;
	zest_uint use_labels_address_bit;

	//Device maximums and other settings/formats
	zest_format depth_format;
	zest_uint max_image_size;

	zest_init_flags init_flags;

	void *memory_pools[ZEST_MAX_DEVICE_MEMORY_POOLS];
	zest_size memory_pool_sizes[ZEST_MAX_DEVICE_MEMORY_POOLS];
	zest_uint memory_pool_count;
	zloc_allocator *allocator;
	char **extensions;
	zest_platform_memory_info_t platform_memory_info;
	zest_uint allocation_id;
	zest_uint vector_id;
	zloc_linear_allocator_t *scratch_arena;

	zest_platform platform;
	zest_device_backend backend;

	zest_uint graphics_queue_family_index;
	zest_uint transfer_queue_family_index;
	zest_uint compute_queue_family_index;
	zest_queue_t graphics_queue;
	zest_queue_t compute_queue;
	zest_queue_t transfer_queue;
	zest_queue *queues;
	zest_map_queue_names queue_names;
	zest_text_t log_path;
	zest_device_builder_t setup_info;

	zest_map_buffer_pool_sizes pool_sizes;

	zest_map_validation_errors validation_errors;

	//For scheduled tasks
	zest_uint frame_counter;
	zest_buffer *deferred_staging_buffers_for_freeing;
	zest_device_destruction_queue_t deferred_resource_freeing_list;

	//Debugging
	zest_debug_t debug;
	zest_map_reports reports;

	//Built in shaders that I'll probably remove soon
	zest_builtin_shaders_t builtin_shaders;
	zest_builtin_pipeline_templates_t pipeline_templates;

	//Global descriptor set and layout template.
	zest_set_layout_builder_t global_layout_builder;

	//GPU buffer allocation
	zest_map_buffer_allocators buffer_allocators;
  
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

typedef void (*zest_rg_execution_callback)(const zest_command_list command_list, void *user_data);
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
	zest_rg_execution_callback callback;
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

#ifdef ZEST_TEST_MODE
typedef struct zest_image_barrier_t {
	zest_access_flags src_access_mask;
	zest_access_flags dst_access_mask;
	zest_image_layout old_layout;
	zest_image_layout new_layout;
	zest_uint src_queue_family_index;
	zest_uint dst_queue_family_index;
} zest_image_barrier_t;

typedef struct zest_buffer_barrier_t {
	zest_access_flags src_access_mask;
	zest_access_flags dst_access_mask;
	zest_uint src_queue_family_index;
	zest_uint dst_queue_family_index;
} zest_buffer_barrier_t;
#endif

typedef struct zest_execution_barriers_t {
	zest_execution_barriers_backend backend;
	zest_resource_node *acquire_image_barrier_nodes;
	zest_resource_node *acquire_buffer_barrier_nodes;
	zest_resource_node *release_image_barrier_nodes;
	zest_resource_node *release_buffer_barrier_nodes;
	#ifdef ZEST_TEST_MODE
	zest_image_barrier_t *acquire_image_barriers;
	zest_buffer_barrier_t *acquire_buffer_barriers;
	zest_image_barrier_t *release_image_barriers;
	zest_buffer_barrier_t *release_buffer_barriers;
	zest_pipeline_stage_flags overall_src_stage_mask_for_acquire_barriers;
	zest_pipeline_stage_flags overall_dst_stage_mask_for_acquire_barriers;
	zest_pipeline_stage_flags overall_src_stage_mask_for_release_barriers;
	zest_pipeline_stage_flags overall_dst_stage_mask_for_release_barriers;
	#endif
} zest_execution_barriers_t;

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
} zest_rendering_attachment_info_t;

typedef struct zest_rendering_info_t {
	zest_uint color_attachment_count;
	zest_format color_attachment_formats[ZEST_MAX_ATTACHMENTS];
	zest_format depth_attachment_format;
	zest_format stencil_attachment_format;
	zest_sample_count_flags sample_count;
} zest_rendering_info_t;

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
	zest_pass_node_visit_state visit_state;
} zest_pass_node_t;

typedef zest_buffer(*zest_resource_buffer_provider)(zest_context context, zest_resource_node resource);
typedef zest_image_view(*zest_resource_image_provider)(zest_context context, zest_resource_node resource);

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

typedef struct zest_semaphore_reference_t {
	zest_dynamic_resource_type type;
	zest_u64 semaphore;
} zest_semaphore_reference_t;

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
	zest_pipeline_stage_flags *wait_stages;
	zest_pipeline_stage_flags *wait_dst_stage_masks;

	//References for printing the render graph only
	zest_u64 *wait_values;
	zest_u64 *signal_values;
	zest_bool need_timeline_wait;
	zest_bool need_timeline_signal;

	zest_submission_batch_backend backend;
} zest_submission_batch_t;

typedef struct zest_wave_submission_t {
	zest_uint queue_bits;
	zest_submission_batch_t batches[ZEST_QUEUE_COUNT];
} zest_wave_submission_t;

typedef struct zest_resource_versions_t {
	zest_resource_node *resources;
}zest_resource_versions_t;

typedef struct zest_frame_graph_auto_state_t {
	zest_uint render_width;
	zest_uint render_height;
	zest_format render_format;
} zest_frame_graph_auto_state_t;

typedef struct zest_frame_graph_cache_key_t {
	zest_frame_graph_auto_state_t auto_state;
	const void *user_state;
	zest_size user_state_size;
} zest_frame_graph_cache_key_t;

typedef struct zest_command_list_t {
	int magic;
	zest_context context;
	zest_command_list_backend backend;
	zest_frame_graph frame_graph;
	zest_pass_node pass_node;
	zest_uint submission_index;
	zest_uint queue_index;
	zest_bool began_rendering;
	zest_pipeline_stage_flags timeline_wait_stage;
	zest_rendering_info_t rendering_info;
} zest_command_list_t;

typedef struct zest_frame_graph_semaphores_t {
	int magic;
	zest_frame_graph_semaphores_backend backend;
	zest_size values[ZEST_MAX_FIF][ZEST_QUEUE_COUNT];
} zest_frame_graph_semaphores_t;

zest_hash_map(zest_pass_group_t) zest_map_passes;
zest_hash_map(zest_resource_versions_t) zest_map_resource_versions;

typedef struct zest_frame_graph_builder_t {
	zest_context context;
	zest_frame_graph frame_graph;
	zest_pass_node current_pass;
}zest_frame_graph_builder_t;

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

	zest_execution_timeline *wait_on_timelines;
	zest_execution_timeline *signal_timelines;
	zest_frame_graph_semaphores semaphores;

	zest_execution_wave_t *execution_waves;         // Execution order after compilation

	zest_resource_node swapchain_resource;          // Handle to the current swapchain image resource
	zest_swapchain swapchain;                       // Handle to the current swapchain image resource
	zest_uint id_counter;
	zest_descriptor_pool descriptor_pool;           //Descriptor pool for execution nodes within the graph.
	zest_set_layout bindless_layout;
	zest_descriptor_set bindless_set;

	zest_wave_submission_t *submissions;

	void *user_data;
	zest_command_list_t command_list;
	zest_key cache_key;

	zest_uint timestamp_count;
	zest_query_state query_state[ZEST_MAX_FIF];                      //For checking if the timestamp query is ready
	zest_gpu_timestamp_t *timestamps[ZEST_MAX_FIF];                  //The last recorded frame durations for the whole render pipeline

} zest_frame_graph_t;

// --- Internal render graph function ---
ZEST_PRIVATE zest_bool zest__is_stage_compatible_with_qfi(zest_pipeline_stage_flags stages_to_check, zest_device_queue_type queue_family_capabilities);
ZEST_API_TMP zest_image_layout zest__determine_final_layout(zest_uint pass_index, zest_resource_node node, zest_resource_usage_t *current_usage);
ZEST_API_TMP zest_image_aspect_flags zest__determine_aspect_flag(zest_format format);
ZEST_PRIVATE void zest__interpret_hints(zest_resource_node resource, zest_resource_usage_hint usage_hints);
ZEST_PRIVATE void zest__deferr_resource_destruction(zest_context context, void *handle);
ZEST_PRIVATE void zest__deferr_image_destruction(zest_context context, zest_image image);
ZEST_PRIVATE void zest__deferr_view_array_destruction(zest_context context, zest_image_view_array view_array);
ZEST_PRIVATE zest_pass_node zest__add_pass_node(const char *name, zest_device_queue_type queue_type);
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

// --- Dynamic resource callbacks ---
ZEST_PRIVATE zest_image_view zest__swapchain_resource_provider(zest_context context, zest_resource_node resource);
ZEST_PRIVATE zest_buffer zest__instance_layer_resource_provider(zest_context context, zest_resource_node resource);
ZEST_PRIVATE zest_buffer zest__instance_layer_resource_provider_prev_fif(zest_context context, zest_resource_node resource);
ZEST_PRIVATE zest_buffer zest__instance_layer_resource_provider_current_fif(zest_context context, zest_resource_node resource);

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
ZEST_API zest_image zest_GetResourceImage(zest_resource_node resource_node);
ZEST_API zest_resource_type zest_GetResourceType(zest_resource_node resource_node);
ZEST_API zest_image_info_t zest_GetResourceImageDescription(zest_resource_node resource_node);
ZEST_API void *zest_GetResourceUserData(zest_resource_node resource_node);
ZEST_API void zest_SetResourceUserData(zest_resource_node resource_node, void *user_data);
ZEST_API void zest_SetResourceClearColor(zest_resource_node resource, float red, float green, float blue, float alpha);
ZEST_API zest_frame_graph zest_GetCachedFrameGraph(zest_context context, zest_frame_graph_cache_key_t *cache_key);
ZEST_API void zest_QueueFrameGraphForExecution(zest_context context, zest_frame_graph frame_graph);

// -- Creating and Executing the render graph
ZEST_API zest_bool zest_BeginFrameGraph(zest_context context, const char *name, zest_frame_graph_cache_key_t *cache_key);
ZEST_API zest_frame_graph_cache_key_t zest_InitialiseCacheKey(zest_context context, const void *user_state, zest_size user_state_size);
ZEST_API zest_frame_graph zest_EndFrameGraph();
ZEST_API zest_frame_graph zest_EndFrameGraphAndWait();

// --- Add pass nodes that execute user commands ---
ZEST_API zest_pass_node zest_BeginGraphicBlankScreen(const char *name);
ZEST_API zest_pass_node zest_BeginRenderPass(const char *name);
ZEST_API zest_pass_node zest_BeginComputePass(zest_compute_handle compute, const char *name);
ZEST_API zest_pass_node zest_BeginTransferPass(const char *name);
ZEST_API void zest_EndPass();

// --- Helper functions for acquiring bindless desriptor array indexes---
ZEST_API zest_uint zest_GetTransientSampledImageBindlessIndex(const zest_command_list command_list, zest_resource_node resource, zest_binding_number_type binding_number);
ZEST_API zest_uint *zest_GetTransientSampledMipBindlessIndexes(const zest_command_list command_list, zest_resource_node resource, zest_binding_number_type binding_number);
ZEST_API zest_uint zest_GetTransientBufferBindlessIndex(const zest_command_list command_list, zest_resource_node resource);

// --- Add callback tasks to passes
ZEST_API void zest_SetPassTask(zest_rg_execution_callback callback, void *user_data);
ZEST_API void zest_SetPassInstanceLayerUpload(zest_layer_handle layer);
ZEST_API void zest_SetPassInstanceLayer(zest_layer_handle layer);

// --- Add Transient resources ---
ZEST_API zest_resource_node zest_AddTransientImageResource(const char *name, zest_image_resource_info_t *info);
ZEST_API zest_resource_node zest_AddTransientBufferResource(const char *name, const zest_buffer_resource_info_t *info);
ZEST_API zest_resource_node zest_AddTransientLayerResource(const char *name, const zest_layer_handle layer, zest_bool prev_fif);
ZEST_API void zest_FlagResourceAsEssential(zest_resource_node resource);

// --- Render target groups ---
ZEST_API zest_output_group zest_CreateOutputGroup();
ZEST_API void zest_AddSwapchainToRenderTargetGroup(zest_output_group group);
ZEST_API void zest_AddImageToRenderTargetGroup(zest_output_group group, zest_resource_node image);

// --- Import external resouces into the render graph ---
ZEST_API zest_resource_node zest_ImportSwapchainResource();
ZEST_API zest_resource_node zest_ImportImageResource(const char *name, zest_image_handle image, zest_resource_image_provider provider);
ZEST_API zest_resource_node zest_ImportBufferResource(const char *name, zest_buffer buffer, zest_resource_buffer_provider provider);

// --- Manual Barrier Functions
ZEST_API void zest_ReleaseBufferAfterUse(zest_resource_node dst_buffer);

// --- Connect Resources to Pass Nodes ---
ZEST_API void zest_ConnectInput(zest_resource_node resource);
ZEST_API void zest_ConnectOutput(zest_resource_node resource);
ZEST_API void zest_ConnectSwapChainOutput();
ZEST_API void zest_ConnectGroupedOutput(zest_output_group group);

// --- Connect graphs to each other
ZEST_API void zest_WaitOnTimeline(zest_execution_timeline timeline);
ZEST_API void zest_SignalTimeline(zest_execution_timeline timeline);

// --- State check functions
ZEST_API zest_bool zest_RenderGraphWasExecuted(zest_frame_graph frame_graph);

// --- Syncronization Helpers ---
ZEST_API zest_execution_timeline zest_CreateExecutionTimeline(zest_context context);

// -- General pass and resource getters/setters
ZEST_API zest_key zest_GetPassOutputKey(zest_pass_node pass);

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
ZEST_API void zest_PrintCompiledFrameGraph(zest_frame_graph frame_graph);
ZEST_API void zest_PrintCachedFrameGraph(zest_context context, zest_frame_graph_cache_key_t *cache_key);

// --- [Swapchain_helpers]
ZEST_API zest_swapchain zest_GetSwapchain(zest_context context);
ZEST_API zest_format zest_GetSwapchainFormat(zest_swapchain swapchain);
ZEST_API void zest_SetSwapchainClearColor(zest_context context, float red, float green, float blue, float alpha);
//End Swapchain helpers

typedef struct zest_descriptor_indices_t {
	zest_uint *free_indices;
	zest_uint next_new_index;
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

typedef struct zest_shader_resources_t {
	int magic;
	zest_shader_resources_handle handle;
	zest_descriptor_set *sets[ZEST_MAX_FIF];
} zest_shader_resources_t ZEST_ALIGN_AFFIX(16);

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
	zest_descriptor_binding_desc_t *bindings;
	zest_set_layout_backend backend;
	zest_descriptor_indices_t *descriptor_indexes;
	zest_text_t name;
	zest_u64 binding_indexes;
	zest_descriptor_pool pool;
	zest_set_layout_flags flags;
} zest_set_layout_t;

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
	void *push_constants;                                                        //Pointer to user push constant data
    
	zest_vertex_attribute_desc_t *attribute_descriptions;
	zest_vertex_binding_desc_t *binding_descriptions;
	zest_bool no_vertex_input;
	const char *vertShaderFunctionName;
	const char *fragShaderFunctionName;
	zest_shader_handle vertex_shader;
	zest_shader_handle fragment_shader;

	zest_color_blend_attachment_t color_blend_attachment;
	zest_set_layout *set_layouts;
	zest_push_constant_range_t push_constant_range;
} zest_pipeline_template_t;

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

//A pipeline set is all of the necessary things required to setup and maintain a pipeline
typedef struct zest_pipeline_t {
	int magic;
	zest_context context;
	zest_pipeline_backend backend;
	zest_pipeline_template pipeline_template;
	void(*rebuild_pipeline_function)(void*);                                     //Override the function to rebuild the pipeline when the swap chain is recreated
	zest_pipeline_set_flags flags;                                               //Flag bits
} zest_pipeline_t;

typedef struct zest_compute_t {
	int magic;
	zest_compute_backend backend;
	zest_compute_handle handle;
	zest_set_layout bindless_layout;                   		  // The bindless descriptor set layout to use in the compute shader
	zest_shader_handle *shaders;                              // List of compute shaders to use
	void *compute_data;                                       // Connect this to any custom data that is required to get what you need out of the compute process.
	void *user_data;                                          // Custom user data
	zest_compute_flags flags;
	zest_uint fif;                                            // Used for manual frame in flight compute
} zest_compute_t;

typedef struct zest_sprite_instance_t {            //52 bytes
	zest_vec4 uv;                                  //The UV coords of the image in the texture packed into a u64 snorm (4 16bit floats)
	zest_vec4 position_rotation;                   //The position of the sprite with rotation in w and stretch in z
	zest_u64 size_handle;                          //Size of the sprite in pixels and the handle packed into a u64 (4 16bit floats)
	zest_uint alignment;                           //normalised alignment vector 2 floats packed into 16bits
	zest_color_t color;                            //The color tint of the sprite
	zest_uint intensity_texture_array;             //reference for the texture array (8bits) and intensity (24bits)
	zest_uint padding[3];
} zest_sprite_instance_t;

typedef struct zest_billboard_instance_t {         //56 bytes
	zest_vec3 position;                            //The position of the sprite
	zest_uint alignment;                           //Alignment x, y and z packed into a uint as 8bit floats
	zest_vec4 rotations_stretch;                   //Pitch, yaw, roll and stretch 
	zest_u64 uv;                                   //The UV coords of the image in the texture packed into a u64 snorm (4 16bit floats)
	zest_u64 scale_handle;                         //The scale and handle of the billboard packed into u64 (4 16bit floats)
	zest_uint intensity_texture_array;             //reference for the texture array (8bits) and intensity (24bits)
	zest_color_t color;                              //The color tint of the sprite
	zest_u64 padding;
} zest_billboard_instance_t;

//SDF Lines
typedef struct zest_shape_instance_t {
	zest_vec4 rect;                                //The rectangle containing the sdf shade, x,y = top left, z,w = bottom right
	zest_vec4 parameters;                          //Extra parameters, for example line widths, roundness, depending on the shape type
	zest_color_t start_color;                        //The color tint of the first point in the line
	zest_color_t end_color;                          //The color tint of the second point in the line
} zest_shape_instance_t;

//SDF 3D Lines
typedef struct zest_line_instance_t {
	zest_vec4 start;
	zest_vec4 end;
	zest_color_t start_color;
	zest_color_t end_color;
} zest_line_instance_t;

typedef struct zest_textured_vertex_t {
	zest_vec3 pos;                                 //3d position
	float intensity;                               //Alpha level (can go over 1 to increase intensity of colors)
	zest_vec2 uv;                                  //Texture coordinates
	zest_color_t color;                              //packed color
	zest_uint parameters;                          //packed parameters such as texture layer
} zest_textured_vertex_t;

typedef struct zest_mesh_instance_t {
	zest_vec3 pos;                                 //3d position
	zest_color_t color;                              //packed color
	zest_vec3 rotation;
	zest_uint parameters;                          //packed parameters
	zest_vec3 scale;
} zest_mesh_instance_t;

typedef struct zest_vertex_t {
	zest_vec3 pos;                                 //3d position
	zest_color_t color;
	zest_vec3 normal;                              //3d normal
	zest_uint group;
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
} zest_layer_buffers_t;

typedef struct zest_layer_instruction_t {
	char push_constant[128];                      //Each draw instruction can have different values in the push constants push_constants
	zest_scissor_rect_t scissor;                  //The drawinstruction can also clip whats drawn
	zest_viewport_t viewport;                     //The viewport size of the draw call
	zest_index start_index;                       //The starting index
	union {
		zest_uint total_instances;                //The total number of instances to be drawn in the draw instruction
		zest_uint total_indexes;                  //The total number of indexes to be drawn in the draw instruction
	};
	zest_index last_instance;                     //The last instance that was drawn in the previous instance instruction
	zest_pipeline_template pipeline_template;     //The pipeline template to draw the instances.
	zest_shader_resources_handle shader_resources;       //The descriptor set shader_resources used to draw with
	void *asset;                                  //Optional pointer to either texture, font etc
	zest_draw_mode draw_mode;
} zest_layer_instruction_t ZEST_ALIGN_AFFIX(16);

//Todo: do we need this now?
typedef struct zest_layer_builder_t {
	zest_context context;
	zest_size type_size;
	zest_uint initial_count;
} zest_layer_builder_t;

typedef struct zest_layer_t {
	int magic;
	zest_layer_handle handle;

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


typedef struct zest_compute_builder_t {
	zest_context context;
	zest_set_layout *non_bindless_layouts;
	zest_set_layout bindless_layout;
	zest_shader_handle *shaders;
	zest_uint push_constant_size;
	zest_compute_flags flags;
	void *user_data;
} zest_compute_builder_t;

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
	void                       (*set_execution_fence)(zest_context context, zest_execution_backend backend, zest_bool is_intraframe);
	zest_frame_graph_semaphores(*get_frame_graph_semaphores)(zest_context context, const char *name);
	zest_bool                  (*submit_frame_graph_batch)(zest_frame_graph frame_graph, zest_execution_backend backend, zest_submission_batch_t *batch, zest_map_queue_value *queues);
	zest_bool                  (*begin_render_pass)(const zest_command_list command_list, zest_execution_details_t *exe_details);
	void                       (*end_render_pass)(const zest_command_list command_list);
	void                       (*carry_over_semaphores)(zest_frame_graph frame_graph, zest_wave_submission_t *wave_submission, zest_execution_backend backend);
	zest_bool                  (*frame_graph_fence_wait)(zest_context context, zest_execution_backend backend);
	zest_bool                  (*create_execution_timeline_backend)(zest_context context, zest_execution_timeline timeline);
	void                       (*add_frame_graph_buffer_barrier)(zest_resource_node resource, zest_execution_barriers_t *barriers,
		zest_bool acquire, zest_access_flags src_access, zest_access_flags dst_access,
		zest_uint src_family, zest_uint dst_family, zest_pipeline_stage_flags src_stage,
		zest_pipeline_stage_flags dst_stage);
	void                       (*add_frame_graph_image_barrier)(zest_resource_node resource, zest_execution_barriers_t *barriers, zest_bool acquire,
		zest_access_flags src_access, zest_access_flags dst_access, zest_image_layout old_layout, zest_image_layout new_layout,
		zest_uint src_family, zest_uint dst_family, zest_pipeline_stage_flags src_stage, zest_pipeline_stage_flags dst_stage);
	void                       (*validate_barrier_pipeline_stages)(zest_execution_barriers_t *barriers);
	zest_bool                  (*present_frame)(zest_context context);
	zest_bool                  (*dummy_submit_for_present_only)(zest_context context);
	zest_bool                  (*acquire_swapchain_image)(zest_swapchain swapchain);
	void*                  	   (*new_frame_graph_image_backend)(zloc_linear_allocator_t *allocator, zest_image image, zest_image imported_image);
	//Buffer and memory
	zest_bool                  (*create_buffer_memory_pool)(zest_context context, zest_size size, zest_buffer_info_t *buffer_info, zest_device_memory_pool memory_pool, const char *name);
	zest_bool                  (*create_image_memory_pool)(zest_context context, zest_size size_in_bytes, zest_buffer_info_t *buffer_info, zest_device_memory_pool buffer);
	zest_bool                  (*map_memory)(zest_device_memory_pool memory_allocation, zest_size size, zest_size offset);
	void 		               (*unmap_memory)(zest_device_memory_pool memory_allocation);
	void					   (*set_buffer_backend_details)(zest_buffer buffer);
	void					   (*flush_used_buffers)(zest_context context);
	void					   (*cmd_copy_buffer_one_time)(zest_buffer src_buffer, zest_buffer dst_buffer, zest_size size);
	void					   (*push_buffer_for_freeing)(zest_buffer buffer);
	zest_access_flags   	   (*get_buffer_last_access_mask)(zest_buffer buffer);
	//Images
	zest_bool 				   (*create_image)(zest_context context, zest_image image, zest_uint layer_count, zest_sample_count_flags num_samples, zest_image_flags flags);
	zest_image_view 		   (*create_image_view)(zest_context context, zest_image image, zest_image_view_type view_type, zest_uint mip_levels_this_view, zest_uint base_mip, zest_uint base_array_index, zest_uint layer_count, zloc_linear_allocator_t *linear_allocator);
	zest_image_view_array 	   (*create_image_views_per_mip)(zest_context context, zest_image image, zest_image_view_type view_type, zest_uint base_array_index, zest_uint layer_count, zloc_linear_allocator_t *linear_allocator);
	zest_bool 				   (*copy_buffer_regions_to_image)(zest_context context, zest_buffer_image_copy_t *regions, zest_uint regions_count, zest_buffer buffer, zest_size src_offset, zest_image image);
	zest_bool 				   (*transition_image_layout)(zest_context context, zest_image image, zest_image_layout new_layout, zest_uint base_mip_index, zest_uint mip_levels, zest_uint base_array_index, zest_uint layer_count);
	zest_bool 				   (*create_sampler)(zest_sampler sampler);
	int 	  				   (*get_image_raw_layout)(zest_image image);
	zest_bool 				   (*copy_buffer_to_image)(zest_buffer buffer, zest_size src_offset, zest_image image, zest_uint width, zest_uint height);
	zest_bool 				   (*generate_mipmaps)(zest_context context, zest_image image);
	//Descriptor Sets
	zest_bool                  (*create_uniform_descriptor_set)(zest_uniform_buffer buffer, zest_set_layout associated_layout);
	//Pipelines
	zest_bool                  (*build_pipeline)(zest_pipeline pipeline, zest_command_list command_list);
	zest_bool				   (*finish_compute)(zest_compute_builder_t *builder, zest_compute compute);
	//Fences
	zest_fence_status          (*wait_for_renderer_fences)(zest_context context);
	zest_bool                  (*reset_renderer_fences)(zest_context context);
	//Set layouts
	zest_bool                  (*create_set_layout)(zest_context context, zest_set_layout_builder_t *builder, zest_set_layout layout, zest_bool is_bindless);
	zest_bool                  (*create_set_pool)(zest_context context, zest_descriptor_pool pool, zest_set_layout layout, zest_uint max_set_count, zest_bool bindles);
	zest_descriptor_set        (*create_bindless_set)(zest_set_layout layout);
	void                       (*update_bindless_image_descriptor)(zest_context context, zest_uint binding_number, zest_uint array_index, zest_descriptor_type type, zest_image image, zest_image_view view, zest_sampler sampler, zest_descriptor_set set);
	void                       (*update_bindless_buffer_descriptor)(zest_uint binding_number, zest_uint array_index, zest_buffer buffer, zest_descriptor_set set);
	//Command buffers/queues
	void					   (*reset_queue_command_pool)(zest_context context, zest_context_queue queue);
	zest_bool 				   (*begin_single_time_commands)(zest_context context);
	zest_bool 				   (*end_single_time_commands)(zest_context context);
	//General Context
	void                       (*set_depth_format)(zest_device device);
	zest_bool                  (*initialise_context_backend)(zest_context context);
	zest_sample_count_flags	   (*get_msaa_sample_count)(zest_context context);
	zest_bool 				   (*initialise_swapchain)(zest_context context);
	zest_bool				   (*initialise_context_queue_backend)(zest_context context, zest_context_queue context_queue);
	//Device/OS
	void                  	   (*wait_for_idle_device)(zest_context context);
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
	void*                      (*new_memory_pool_backend)(zest_context context);
	void*					   (*new_device_backend)(zest_device device);
	void*					   (*new_context_backend)(zest_context context);
	void*					   (*new_frame_graph_context_backend)(zest_context context);
	void*					   (*new_swapchain_backend)(zest_context context);
	void*					   (*new_buffer_backend)(zest_device device);
	void*					   (*new_uniform_buffer_backend)(zest_context context);
	void					   (*set_uniform_buffer_backend)(zest_uniform_buffer buffer);
	void*					   (*new_image_backend)(zest_context context);
	void*					   (*new_compute_backend)(zest_context context);
	void*					   (*new_queue_backend)(zest_device device);
	void*					   (*new_submission_batch_backend)(zest_context context);
	void*					   (*new_set_layout_backend)(zloc_allocator *allocator);
	void*					   (*new_descriptor_pool_backend)(zest_context context);
	void*					   (*new_sampler_backend)(zest_context context);
	void*					   (*new_context_queue_backend)(zest_context context);
	//Cleanup backends
	void                       (*cleanup_frame_graph_semaphore)(zest_context context, zest_frame_graph_semaphores semaphores);
	void                       (*cleanup_image_backend)(zest_image image);
	void                       (*cleanup_image_view_backend)(zest_image_view image_view);
	void                       (*cleanup_image_view_array_backend)(zest_image_view_array image_view);
	void                       (*cleanup_memory_pool_backend)(zest_device_memory_pool memory_allocation);
	void                       (*cleanup_device_backend)(zest_device device);
	void                       (*cleanup_buffer_backend)(zest_buffer buffer);
	void                       (*cleanup_context_backend)(zest_context context);
	void                       (*destroy_context_surface)(zest_context context);
	void 					   (*cleanup_swapchain_backend)(zest_swapchain swapchain);
	void 					   (*cleanup_uniform_buffer_backend)(zest_uniform_buffer buffer);
	void 					   (*cleanup_compute_backend)(zest_compute compute);
	void 					   (*cleanup_set_layout)(zest_set_layout layout);
	void 					   (*cleanup_pipeline_backend)(zest_pipeline pipeline);
	void 					   (*cleanup_sampler_backend)(zest_sampler sampler);
	void 					   (*cleanup_queue_backend)(zest_device device, zest_queue queue);
	void 					   (*cleanup_context_queue_backend)(zest_context context, zest_context_queue context_queue);
	void 					   (*cleanup_set_layout_backend)(zest_set_layout sampler);
	//Debugging
	void*                      (*get_final_signal_ptr)(zest_submission_batch_t *batch, zest_uint semaphore_index);
	void*                      (*get_final_wait_ptr)(zest_submission_batch_t *batch, zest_uint semaphore_index);
	void*                      (*get_resource_ptr)(zest_resource_node resource);
	//Command recording
	void					   (*blit_image_mip)(const zest_command_list command_list, zest_resource_node src, zest_resource_node dst, zest_uint mip_to_blit, zest_supported_pipeline_stages pipeline_stage);
	void                       (*copy_image_mip)(const zest_command_list command_list, zest_resource_node src, zest_resource_node dst, zest_uint mip_to_blit, zest_supported_pipeline_stages pipeline_stage);
	void                       (*insert_compute_image_barrier)(const zest_command_list command_list, zest_resource_node resource, zest_uint base_mip);
	void                       (*set_screensized_viewport)(const zest_command_list command_list, float min_depth, float max_depth);
	void                       (*scissor)(const zest_command_list command_list, zest_scissor_rect_t *scissor);
	void                       (*viewport)(const zest_command_list command_list, zest_viewport_t *viewport);
	void                       (*clip)(const zest_command_list command_list, float x, float y, float width, float height, float minDepth, float maxDepth);
	void                       (*bind_pipeline)(const zest_command_list command_list, zest_pipeline pipeline, zest_descriptor_set *descriptor_set, zest_uint set_count);
	void                       (*bind_compute_pipeline)(const zest_command_list command_list, zest_compute_handle compute, zest_descriptor_set *descriptor_set, zest_uint set_count);
	void                       (*bind_pipeline_shader_resource)(const zest_command_list command_list, zest_pipeline pipeline, zest_shader_resources_handle shader_resources);
	void                       (*copy_buffer)(const zest_command_list command_list, zest_buffer staging_buffer, zest_buffer device_buffer, zest_size size);
	zest_bool                  (*upload_buffer)(const zest_command_list command_list, zest_buffer_uploader_t *uploader);
	void                       (*bind_vertex_buffer)(const zest_command_list command_list, zest_uint first_binding, zest_uint binding_count, zest_buffer buffer);
	void                       (*bind_index_buffer)(const zest_command_list command_list, zest_buffer buffer);
	zest_bool                  (*image_clear)(const zest_command_list command_list, zest_image_handle image);
	void                       (*bind_mesh_vertex_buffer)(const zest_command_list command_list, zest_layer_handle layer);
	void                       (*bind_mesh_index_buffer)(const zest_command_list command_list, zest_layer_handle layer);
	void                       (*send_custom_compute_push_constants)(const zest_command_list command_list, zest_compute_handle compute, const void *push_constant);
	void                       (*dispatch_compute)(const zest_command_list command_list, zest_compute_handle compute, zest_uint group_count_x, zest_uint group_count_y, zest_uint group_count_z);
	void                       (*send_push_constants)(const zest_command_list command_list, zest_pipeline pipeline, void *data);
	void                       (*draw)(const zest_command_list command_list, zest_uint vertex_count, zest_uint instance_count, zest_uint first_vertex, zest_uint first_instance);
	void                       (*draw_layer_instruction)(const zest_command_list command_list, zest_uint vertex_count, zest_layer_instruction_t *instruction);
	void                       (*draw_indexed)(const zest_command_list command_list, zest_uint index_count, zest_uint instance_count, zest_uint first_index, int32_t vertex_offset, zest_uint first_instance);
} zest_platform_t;

typedef struct zest_context_t {
	int magic;

	//Platform specific data
	zest_context_backend backend;
	zest_uint allocation_id;
	zest_platform_memory_info_t platform_memory_info;

	//The current fence count of the last frame
	zest_uint fence_count[ZEST_MAX_FIF];

	//Queues and command buffer pools
	zest_context_queue queues[ZEST_QUEUE_COUNT];
	zest_uint active_queue_indexes[ZEST_QUEUE_COUNT];
	zest_uint active_queue_count;

	//Frame in flight indexes
	zest_uint previous_fif;
	zest_uint current_fif;
	zest_uint saved_fif;

	//The timeout for the fence that waits for gpu work to finish for the frame
	zest_u64 fence_wait_timeout_ns;
	zest_wait_timeout_callback fence_wait_timeout_callback;

	//Window data
	zest_extent2d_t window_extent;
	float dpi_scale;
	zest_swapchain swapchain;
	zest_window_data_t window_data;

	//Context data
	zest_frame_graph *frame_graphs;       //All the frame graphs used this frame. Gets cleared at the beginning of each frame

	//Linear allocator for building the render graph each frame. The memory for this is allocated from
	//The device TLSF allocator
	zloc_linear_allocator_t *frame_graph_allocator[ZEST_MAX_FIF];

	//Resource storage
	zloc_allocator *allocator;
	zest_resource_store_t resource_stores[zest_max_context_handle_type];
	zest_context_destruction_queue_t deferred_resource_freeing_list;
	zest_uint vector_id;

	//Timeline semaphores for creating dependencies between frames
	//Might not want to keep these
	zest_execution_timeline *timeline_semaphores;

	//Bindless set layout and set
	zest_set_layout bindless_set_layout;
	zest_descriptor_set bindless_set;

	//Mip indexes for images
	zest_map_mip_indexes mip_indexes;

	//Cached pipelines
	zest_map_cached_pipelines cached_pipelines;

	//Frame Graph Cache Storage
	zest_map_cached_frame_graphs cached_frame_graphs;
	zest_map_rg_semaphores cached_frame_graph_semaphores;
 
	//Flags
	zest_context_flags flags;

	void *user_data;
	zest_device_t *device;
	zest_create_info_t create_info;

} zest_context_t;

extern zest_platform_t ZestPlatform;

typedef struct zest_generic_handle {
	zest_u64 value;
	zest_resource_store_t *store;
}zest_generic_handle;

ZEST_API inline zest_bool zest_IsValidHandle(void *handle) {
	zest_generic_handle *generic_handle = (zest_generic_handle *)handle;
	if (!generic_handle && ZEST_VALID_HANDLE(generic_handle->store)) {
		return ZEST_FALSE;
	}
	zest_uint index = ZEST_HANDLE_INDEX(generic_handle->value);
	zest_uint generation = ZEST_HANDLE_GENERATION(generic_handle->value);
	if (generation > 0) {
		if (index < generic_handle->store->current_size) {
			char *resource = (char*)generic_handle->store->data + generic_handle->store->struct_size * index;
			return ZEST_VALID_HANDLE(resource);
		}
	}
	return ZEST_FALSE;
}

ZEST_PRIVATE inline void *zest__get_store_resource(zest_resource_store_t *store, zest_handle handle) {
	zest_uint index = ZEST_HANDLE_INDEX(handle);
	zest_uint generation = ZEST_HANDLE_GENERATION(handle);
	if (index < store->capacity && store->generations[index] == generation) {
		return (void *)((char *)store->data + index * store->struct_size);
	}
	return NULL;
}

ZEST_PRIVATE inline void *zest__get_store_resource_checked(zest_resource_store_t *store, zest_handle handle) {
	zest_uint index = ZEST_HANDLE_INDEX(handle);
	zest_uint generation = ZEST_HANDLE_GENERATION(handle);
	void *resource = NULL;
	if (index < store->capacity && store->generations[index] == generation) {
		resource = (void *)((char *)store->data + index * store->struct_size);
	}
	ZEST_ASSERT(resource);   //Not a valid handle for the resource. Check the stack trace for the calling function and resource type
	return resource;
}

typedef void(*zest__platform_setup)(zest_platform_t *platform);
extern zest__platform_setup zest__platform_setup_callbacks[zest_max_platforms];

static const zest_image_t zest__image_zero = { 0 };

//--Internal_functions
//Only available outside lib for some implementations like SDL2
ZEST_API zest_size zest_GetNextPower(zest_size n);

//Platform_dependent_functions
//These functions need a different implementation depending on the platform being run on
//See definitions at the top of zest.c
ZEST_PRIVATE zest_bool zest__create_folder(zest_device device, const char *path);
//-- End Platform dependent functions

//Buffer_and_Memory_Management
ZEST_PRIVATE inline void zest__add_host_memory_pool(zest_device device, zest_size size) {
	ZEST_ASSERT(device->memory_pool_count < 32);    //Reached the max number of memory pools
	zest_size pool_size = device->setup_info.memory_pool_size;
	if (pool_size <= size) {
		pool_size = zest_GetNextPower(size);
	}
	device->memory_pools[device->memory_pool_count] = ZEST__ALLOCATE_POOL(pool_size);
	ZEST_ASSERT(device->memory_pools[device->memory_pool_count]);    //Unable to allocate more memory. Out of memory?
	zloc_AddPool(device->allocator, device->memory_pools[device->memory_pool_count], pool_size);
	device->memory_pool_sizes[device->memory_pool_count] = pool_size;
	device->memory_pool_count++;
	ZEST_PRINT_NOTICE(ZEST_NOTICE_COLOR"Note: Ran out of space in the host memory pool so adding a new one of size %zu. ", pool_size);
}

ZEST_PRIVATE inline void *zest__allocate(zloc_allocator *allocator, zest_size size) {
	void* allocation = zloc_Allocate(allocator, size);
	ptrdiff_t offset_from_allocator = (ptrdiff_t)allocation - (ptrdiff_t)allocator;
	if (offset_from_allocator == 8696) {
		int d = 0;
	}
	// If there's something that isn't being freed on zest shutdown and it's of an unknown type then 
	// it should print out the offset from the allocator, you can use that offset to break here and 
	// find out what's being allocated.
	if (!allocation) {
		zest_device device = (zest_device)allocator->user_data;
		zest_size pool_size = (zest_size)ZEST__NEXTPOW2((double)size * 2);
		ZEST_PRINT("Added a new pool size of %zu", pool_size);
		zest__add_host_memory_pool(device, pool_size);
		allocation = zloc_Allocate(allocator, size);
		ZEST_ASSERT(allocation);    //Out of memory? Unable to allocate even after adding a pool
	}
	return allocation;
}

ZEST_PRIVATE void *zest__allocate_aligned(zloc_allocator *allocator, zest_size size, zest_size alignment);
ZEST_PRIVATE inline void *zest__reallocate(zloc_allocator *allocator, void *memory, zest_size size) {
	void* allocation = zloc_Reallocate(allocator, memory, size);
	if (!allocation) {
		zest_device device = (zest_device)allocator->user_data;
		zest__add_host_memory_pool(device, size);
		allocation = zloc_Reallocate(allocator, memory, size);
		ZEST_ASSERT(allocation);    //Unable to allocate even after adding a pool
	}
	return allocation;
}
ZEST_PRIVATE void zest__destroy_memory(zest_device_memory_pool memory_allocation);
ZEST_PRIVATE zest_bool zest__create_memory_pool(zest_context context, zest_buffer_info_t *buffer_info, zest_key key, zest_size minimum_size, zest_device_memory_pool *memory_pool);
ZEST_PRIVATE void zest__add_remote_range_pool(zest_buffer_allocator buffer_allocator, zest_device_memory_pool buffer_pool);
ZEST_PRIVATE void zest__set_buffer_details(zest_context context, zest_buffer_allocator buffer_allocator, zest_buffer buffer, zest_bool is_host_visible);
ZEST_PRIVATE void zest__cleanup_buffers_in_allocators(zest_context context);
//End Buffer Management

//Renderer_functions
ZEST_API inline zest_uint zest_CurrentFIF(zest_context context) {
	return context->current_fif;
}
ZEST_PRIVATE zest_bool zest__initialise_context(zest_context context, zest_create_info_t *create_info);
ZEST_PRIVATE zest_swapchain zest__create_swapchain(zest_context context, const char *name);
ZEST_PRIVATE void zest__get_window_size_callback(zest_context context, void *user_data, int *fb_width, int *fb_height, int *window_width, int *window_height);
ZEST_PRIVATE void zest__destroy_window_callback(zest_context window, void *user_data);
ZEST_PRIVATE void zest__cleanup_swapchain(zest_swapchain swapchain);
ZEST_PRIVATE void zest__free_all_device_resource_stores(zest_device device);
ZEST_PRIVATE void zest__cleanup_device(zest_device device);
ZEST_PRIVATE void zest__cleanup_context(zest_context context);
ZEST_PRIVATE void zest__cleanup_shader_resource_store(zest_context context);
ZEST_PRIVATE void zest__cleanup_image_store(zest_device device);
ZEST_PRIVATE void zest__cleanup_sampler_store(zest_device device);
ZEST_PRIVATE void zest__cleanup_uniform_buffer_store(zest_context context);
ZEST_PRIVATE void zest__cleanup_layer_store(zest_context context);
ZEST_PRIVATE void zest__cleanup_shader_store(zest_device device);
ZEST_PRIVATE void zest__cleanup_compute_store(zest_device device);
ZEST_PRIVATE void zest__cleanup_view_store(zest_device device);
ZEST_PRIVATE void zest__cleanup_view_array_store(zest_device device);
ZEST_PRIVATE void zest__free_handle(void *handle);
ZEST_PRIVATE void zest__scan_memory_and_free_resources(zloc_allocator *allocator);
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
ZEST_PRIVATE void zest__end_instance_instructions(zest_layer layer);
ZEST_PRIVATE void zest__start_instance_instructions(zest_layer layer);
ZEST_PRIVATE void zest__reset_instance_layer_drawing(zest_layer layer);
ZEST_PRIVATE void zest__set_layer_push_constants(zest_layer layer);

// --Image_internal_functions
ZEST_PRIVATE zest_image_handle zest__new_image(zest_context device);
ZEST_PRIVATE void zest__release_all_global_texture_indexes(zest_context context, zest_image image);
ZEST_PRIVATE void zest__release_all_context_texture_indexes(zest_context context);
ZEST_PRIVATE void zest__cleanup_image_view(zest_image_view layout);
ZEST_PRIVATE void zest__cleanup_image_view_array(zest_image_view_array layout);

// --General_layer_internal_functions
ZEST_PRIVATE zest_layer_handle zest__create_instance_layer(zest_context context, const char *name, zest_size instance_type_size, zest_uint initial_instance_count);

// --Mesh_layer_internal_functions
ZEST_PRIVATE void zest__initialise_mesh_layer(zest_context context, zest_layer mesh_layer, zest_size vertex_struct_size, zest_size initial_vertex_capacity);

// --Misc_Helper_Functions
ZEST_PRIVATE zest_image_view_type zest__get_image_view_type(zest_image image);
ZEST_PRIVATE zest_bool zest__create_transient_image(zest_context context, zest_resource_node node);
ZEST_PRIVATE void zest__create_transient_buffer(zest_context context, zest_resource_node node);
ZEST_PRIVATE zest_index zest__next_fif(zest_context context);
// --End Misc_Helper_Functions

// --Pipeline_Helper_Functions
ZEST_PRIVATE zest_pipeline zest__create_pipeline(zest_context context);
ZEST_PRIVATE zest_bool zest__cache_pipeline(zest_pipeline_template pipeline_template, zest_command_list context, zest_key key, zest_pipeline *out_pipeline);
ZEST_PRIVATE void zest__cleanup_pipeline_template(zest_pipeline_template pipeline);
// --End Pipeline Helper Functions

// --Buffer_allocation_funcitons
ZEST_PRIVATE zest_size zest__get_minimum_block_size(zest_size pool_size);
ZEST_PRIVATE void zest__on_add_pool(void *user_data, void *block);
ZEST_PRIVATE void zest__on_split_block(void *user_data, zloc_header* block, zloc_header *trimmed_block, zest_size remote_size);
// --End Buffer allocation funcitons

// --Uniform_buffers
// --End Uniform Buffers

// --Maintenance_functions
ZEST_PRIVATE void zest__cleanup_image(zest_image image);
ZEST_PRIVATE void zest__cleanup_sampler(zest_sampler sampler);
ZEST_PRIVATE void zest__cleanup_uniform_buffer(zest_uniform_buffer uniform_buffer);
// --End Maintenance functions

// --Shader_functions
ZEST_PRIVATE zest_shader_handle zest__new_shader(zest_device device, zest_shader_type type);
ZEST_API void zest__cache_shader(zest_device device, zest_shader shader);
// --End Shader functions

// --Descriptor_set_functions
ZEST_PRIVATE zest_set_layout zest__new_descriptor_set_layout(zest_context context, const char *name);
ZEST_PRIVATE zest_descriptor_pool zest__create_descriptor_pool(zest_context context, zest_uint max_sets);
ZEST_PRIVATE zest_bool zest__binding_exists_in_layout_builder(zest_set_layout_builder_t *builder, zest_uint binding);
ZEST_PRIVATE zest_uint zest__acquire_bindless_index(zest_set_layout layout, zest_uint binding_number);
ZEST_PRIVATE void zest__release_bindless_index(zest_set_layout layout, zest_uint binding_number, zest_uint index_to_release);
ZEST_PRIVATE void zest__cleanup_set_layout(zest_set_layout layout);
ZEST_PRIVATE void zest__add_descriptor_set_to_resources(zest_context context, zest_shader_resources resources, zest_descriptor_set descriptor_set, zest_uint fif);
ZEST_PRIVATE zest_uint zest__acquire_bindless_image_index(zest_context context, zest_image image, zest_image_view view, zest_set_layout layout, zest_descriptor_set set, zest_binding_number_type target_binding_number, zest_descriptor_type descriptor_type);
ZEST_PRIVATE zest_uint zest__acquire_bindless_sampler_index(zest_context context, zest_sampler sampler, zest_set_layout layout, zest_descriptor_set set, zest_binding_number_type target_binding_number);
// --End Descriptor set functions

// --Device_set_up
ZEST_PRIVATE void zest__set_default_pool_sizes(zest_device device);
ZEST_PRIVATE zest_bool zest__initialise_vulkan_device(zest_device device, zest_device_builder info);
ZEST_PRIVATE inline zest_bool zest__is_vulkan_device(zest_device device) { return device->setup_info.platform == zest_platform_vulkan; }
//end device setup functions

//App_initialise_and_run_functions
ZEST_API zest_device_builder zest__begin_device_builder();
ZEST_PRIVATE void zest__do_context_scheduled_tasks(zest_context context);
ZEST_PRIVATE void zest__destroy(zest_context context);
ZEST_PRIVATE zest_fence_status zest__main_loop_fence_wait(zest_context context);
//-- end of internal functions

// Enum_to_string_functions - Helper functions to convert enums to strings 
ZEST_PRIVATE const char *zest__image_layout_to_string(zest_image_layout layout);
ZEST_PRIVATE zest_text_t zest__access_flags_to_string(zest_context context, zest_access_flags flags);
ZEST_PRIVATE zest_text_t zest__pipeline_stage_flags_to_string(zest_context context, zest_pipeline_stage_flags flags);
// -- end Enum_to_string_functions

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
ZEST_API void zest_DeviceBuilderLogToMemory(zest_device_builder builder);
ZEST_API void zest_DeviceBuilderLogPath(zest_device_builder builder, const char *log_path);
//Set the default pool size for the cpu memory used for the device
ZEST_API void zest_SetDeviceBuilderMemoryPoolSize(zest_device_builder builder, zest_size size);
//Finish and create the device
ZEST_API zest_device zest_EndDeviceBuilder(zest_device_builder builder);
//Create a new zest_create_info_t struct with default values for initialising Zest
ZEST_API zest_create_info_t zest_CreateInfo();
//Create a new zest_create_info_t struct with default values for initialising Zest but also enable validation layers as well
ZEST_API zest_create_info_t zest_CreateInfoWithValidationLayers(zest_validation_flags flags);
//Initialise Zest. You must call this in order to use Zest. Use zest_CreateInfo() to set up some default values to initialise the renderer.
ZEST_API zest_context zest_CreateContext(zest_device device, zest_window_data_t *window_data, zest_create_info_t* info);
//Begin a new frame for a context. Within the BeginFrame and EndFrame you can create a frame graph and present a frame.
//This funciton will wait on the fence from the previous time a frame was submitted.
ZEST_API zest_bool zest_BeginFrame(zest_context context);
ZEST_API void zest_EndFrame(zest_context context);
//Maintenance function to be run each time the application loops to flush any unused resources that have been 
//marked for deletion.
ZEST_API void zest_UpdateDevice(zest_device device);
//Shutdown zest and unload/free everything. Call this after zest_Start.
ZEST_API void zest_DestroyContext(zest_context context);
//Free all memory used in the renderer and reset it back to an initial state.
ZEST_API void zest_ResetContext(zest_context context, zest_window_data_t *window_data);
//Set the create info for the renderer, to be used optionally before a call to zest_ResetRenderer to change the configuration
//of the renderer
ZEST_API void zest_SetCreateInfo(zest_context context, zest_create_info_t *info);
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
ZEST_API zest_set_layout zest_FinishDescriptorSetLayoutForBindless(zest_context context, zest_set_layout_builder_t *builder, zest_uint num_global_sets_this_pool_should_support, const char *name, ...);
//Build the descriptor set layout and add it to the render. The layout is also returned from the function.
ZEST_API zest_set_layout zest_FinishDescriptorSetLayout(zest_context context, zest_set_layout_builder_t *builder, const char *name, ...);

ZEST_API zest_descriptor_set zest_CreateBindlessSet(zest_set_layout layout);
ZEST_API zest_uint zest_AcquireSampledImageIndex(zest_context context, zest_image_handle image_handle, zest_binding_number_type binding_number);
ZEST_API zest_uint zest_AcquireStorageImageIndex(zest_context context, zest_image_handle image_handle, zest_binding_number_type binding_number);
ZEST_API zest_uint zest_AcquireSamplerIndex(zest_context context, zest_sampler_handle sampler_handle);
ZEST_API zest_uint zest_AcquireStorageBufferIndex(zest_context context, zest_buffer buffer);
ZEST_API zest_uint *zest_AcquireImageMipIndexes(zest_context context, zest_image_handle handle, zest_image_view_array_handle image_views, zest_binding_number_type binding_number, zest_descriptor_type descriptor_type);
ZEST_API void zest_AcquireInstanceLayerBufferIndex(zest_context context, zest_layer_handle layer);
ZEST_API void zest_ReleaseStorageBufferIndex(zest_context context, zest_buffer buffer);
ZEST_API void zest_ReleaseImageIndex(zest_context context, zest_image_handle image, zest_binding_number_type binding_number);
ZEST_API void zest_ReleaseAllImageIndexes(zest_context context, zest_image_handle image);
ZEST_API void zest_ReleaseBindlessIndex(zest_context context, zest_uint index, zest_binding_number_type binding_number);
ZEST_API zest_descriptor_set zest_GetBindlessSet(zest_context context);
ZEST_API zest_set_layout zest_GetBindlessLayout(zest_context context);
//Create a new descriptor set shader_resources
ZEST_API zest_shader_resources_handle zest_CreateShaderResources(zest_context context);
//Delete shader resources from the renderer and free the memory. This does not free or destroy the actual
//descriptor sets that you added to the resources
ZEST_API void zest_FreeShaderResources(zest_shader_resources_handle shader_resources);
//Add a descriptor set to a descriptor set shader_resources. Bundles are used for binding to a draw call so the descriptor sets can be passed in to the shaders
//according to their set and binding number. So therefore it's important that you add the descriptor sets to the shader_resources in the same order
//that you set up the descriptor set layouts. You must also specify the frame in flight for the descriptor set that you're addeding.
//Use zest_AddDescriptorSetsToResources to add all frames in flight by passing an array of desriptor sets
ZEST_API void zest_AddDescriptorSetToResources(zest_shader_resources_handle shader_resources, zest_descriptor_set descriptor_set, zest_uint fif);
//Add the descriptor set for a uniform buffer to a shader resource
ZEST_API void zest_AddUniformBufferToResources(zest_shader_resources_handle shader_resources, zest_uniform_buffer_handle buffer);
//Add the descriptor set for a uniform buffer to a shader resource
ZEST_API void zest_AddGlobalBindlessSetToResources(zest_shader_resources_handle shader_resources);
//Add the descriptor set for a uniform buffer to a shader resource
ZEST_API void zest_AddBindlessSetToResources(zest_shader_resources_handle shader_resources, zest_descriptor_set set);
//Clear all the descriptor sets in a shader resources object. This does not free the memory, call zest_FreeShaderResources to do that.
ZEST_API void zest_ClearShaderResources(zest_shader_resources_handle shader_resources);
//Update the descriptor set in a shader_resources. You'll need this whenever you update a descriptor set for whatever reason. Pass the index of the
//descriptor set in the shader_resources that you want to update.
ZEST_API void zest_UpdateShaderResources(zest_shader_resources_handle shader_resources, zest_descriptor_set descriptor_set, zest_uint index, zest_uint fif);
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
//Reload a shader. Use this if you edited a shader file and you want to refresh it/hot reload it
//The shader must have been created from a file with zest_CreateShaderFromFile. Once the shader is reloaded you can call
//zest_CompileShader or zest_ValidateShader to recompile it. You'll then have to call zest_SchedulePipelineRecreate to recreate
//the pipeline that uses the shader. Returns true if the shader was successfully loaded.
ZEST_API zest_bool zest_ReloadShader(zest_shader_handle shader);
//Creates and compiles a new shader from a string and add it to the library of shaders in the renderer
ZEST_API zest_bool zest_CompileShader(zest_shader_handle shader);
//Add a shader straight from an spv file and return a handle to the shader. Note that no prefix is added to the filename here so 
//pass in the full path to the file relative to the executable being run.
ZEST_API zest_shader_handle zest_AddShaderFromSPVFile(zest_device device, const char *filename, zest_shader_type type);
//Add an spv shader straight from memory and return a handle to the shader. Note that the name should just be the name of the shader, 
//If a path prefix is set (context->device->shader_path_prefix, set when initialising Zest in the create_info struct, spv is default) then
//This prefix will be prepending to the name you pass in here.
ZEST_API zest_shader_handle zest_AddShaderFromSPVMemory(zest_device device, const char *name, const void *buffer, zest_uint size, zest_shader_type type);
//Add a shader to the renderer list of shaders.
ZEST_API void zest_AddShader(zest_shader_handle shader, const char *name);
//Free the memory for a shader and remove if from the shader list in the renderer (if it exists there)
ZEST_API void zest_FreeShader(zest_shader_handle shader);
//Get the maximum image dimension available on the device
ZEST_API zest_uint zest_GetMaxImageSize(zest_context context);
//Get the device/platform associated with a context
ZEST_API zest_device zest_GetContextDevice(zest_context context);
// -- End Platform_Helper_Functions

//-----------------------------------------------
//        Pipeline_related_helpers
//        Pipelines are essential to drawing things on screen. There are some builtin pipeline_templates that Zest creates
//        for sprite/billboard/mesh/font drawing. You can take a look in zest__prepare_standard_pipelines to see how
//        the following functions are utilised, plus look at the exmaples for building your own custom pipeline_templates.
//-----------------------------------------------
//Add a new pipeline template to the renderer and return its handle.
ZEST_API zest_pipeline_template zest_BeginPipelineTemplate(zest_device device, const char *name);
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
//Clear the push constant ranges in a pipeline. You can use this after copying a pipeline to set up your own push constants instead
//if your pipeline will use a different push constant range.
ZEST_API void zest_ClearPipelinePushConstantRanges(zest_pipeline_template pipeline_template);
//Add a vertex attribute to a zest_vertex_input_descriptions array. You can use zest_CreateVertexInputDescription
//helper function to create the description
ZEST_API void zest_AddVertexAttribute(zest_pipeline_template pipeline_template, zest_uint binding, zest_uint location, zest_format format, zest_uint offset);
//Set up the push contant that you might plan to use in the pipeline. Just pass in the size of the push constant struct, the offset and the shader
//stage flags where the push constant will be used. Use this if you only want to set up a single push constant range
ZEST_API void zest_SetPipelinePushConstantRange(zest_pipeline_template create_info, zest_uint size, zest_supported_shader_stages stage_flags);
//You can set a pointer in the pipeline template to point to the push constant data that you want to pass to the shader.
//It MUST match the same data layout/size that you set with zest_SetPipelinePushConstantRange and align with the 
//push constants that you use in the shader. The point you use must be stable! Or update it if it changes for any reason.
ZEST_API void zest_SetPipelinePushConstants(zest_pipeline_template pipeline_template, void *push_constants);
ZEST_API void zest_SetPipelineBlend(zest_pipeline_template pipeline_template, zest_color_blend_attachment_t blend_attachment);
ZEST_API void zest_SetPipelineDepthTest(zest_pipeline_template pipeline_template, zest_bool enable_test, zest_bool write_enable);
ZEST_API void zest_SetPipelineEnableVertexInput(zest_pipeline_template pipeline_template);
ZEST_API void zest_SetPipelineDisableVertexInput(zest_pipeline_template pipeline_template);
//Add a descriptor layout to the pipeline template. Use this function only when setting up the pipeline before you call zest__build_pipeline
ZEST_API void zest_AddPipelineDescriptorLayout(zest_pipeline_template pipeline_template, zest_set_layout layout);
//Clear the descriptor layouts in a pipeline template create info
ZEST_API void zest_ClearPipelineDescriptorLayouts(zest_pipeline_template pipeline_template);
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
ZEST_API zest_pipeline zest_PipelineWithTemplate(zest_pipeline_template pipeline_template, const zest_command_list command_list);
//Copy the zest_pipeline_template_create_info_t from an existing pipeline. This can be useful if you want to create a new pipeline based
//on an existing pipeline with just a few tweaks like setting a different shader to use.
ZEST_API zest_pipeline_template zest_CopyPipelineTemplate(const char *name, zest_pipeline_template pipeline_template);
//Delete a pipeline including the template and any cached versions of the pipeline
ZEST_API void zest_FreePipelineTemplate(zest_pipeline_template pipeline_template);
ZEST_API zest_bool zest_PipelineIsValid(zest_pipeline_template pipeline);
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
ZEST_API zest_buffer zest_CreateBuffer(zest_context context, zest_size size, zest_buffer_info_t *buffer_info);
//Create a staging buffer which you can use to prep data for uploading to another buffer on the GPU
ZEST_API zest_buffer zest_CreateStagingBuffer(zest_context context, zest_size size, void *data);
//Generate a zest_buffer_info_t with the corresponding buffer configuration to create buffers with
ZEST_API zest_buffer_info_t zest_CreateBufferInfo(zest_buffer_type type, zest_memory_usage usage);
//Grow a buffer if the minium_bytes is more then the current buffer size.
ZEST_API zest_bool zest_GrowBuffer(zest_buffer *buffer, zest_size unit_size, zest_size minimum_bytes);
//Resize a buffer if the new size if more than the current size of the buffer. Returns true if the buffer was resized successfully.
ZEST_API zest_bool zest_ResizeBuffer(zest_buffer *buffer, zest_size new_size);
//Copy data to a staging buffer
void zest_StageData(void *src_data, zest_buffer dst_staging_buffer, zest_size size);
//Free a zest_buffer and return it's memory to the pool
ZEST_API void zest_FreeBuffer(zest_buffer buffer);
//When creating your own draw routines you will probably need to upload data from a staging buffer to a GPU buffer like a vertex buffer. You can
//use this command with zest_cmd_UploadBuffer to upload the buffers that you need. You can call zest_AddCopyCommand multiple times depending on
//how many buffers you need to upload data for and then call zest_cmd_UploadBuffer passing the zest_buffer_uploader_t to copy all the buffers in
//one go. For examples see the builtin draw layers that do this like: zest__update_instance_layer_buffers_callback
ZEST_API void zest_AddCopyCommand(zest_buffer_uploader_t *uploader, zest_buffer source_buffer, zest_buffer target_buffer, zest_size target_offset);

//Get the default pool size that is set for a specific pool hash.
//Todo: is this needed?
ZEST_API zest_buffer_pool_size_t zest_GetDevicePoolSize(zest_context context, zest_key hash);
//Get the default pool size that is set for a specific combination of usage, property and image flags
ZEST_API zest_buffer_pool_size_t zest_GetDeviceBufferPoolSize(zest_context context, zest_buffer_usage_flags buffer_usage_flags, zest_memory_property_flags property_flags, zest_image_usage_flags image_flags);
//Get the default pool size for an image pool.
ZEST_API zest_buffer_pool_size_t zest_GetDeviceImagePoolSize(zest_context context, const char *name);
//Set the default pool size for a specific type of buffer set by the usage and property flags. You must call this before you call zest_Initialise
//otherwise it might not take effect on any buffers that are created during initialisation.
//Note that minimum allocation size may get overridden if it is smaller than the alignment reported by vkGetBufferMemoryRequirements at pool creation
ZEST_API void zest_SetDeviceBufferPoolSize(zest_device device, const char *name, zest_buffer_usage_flags buffer_usage_flags, zest_memory_property_flags property_flags, zest_size minimum_allocation, zest_size pool_size);
//Set the default pool size for images. based on image usage and property flags.
//Note that minimum allocation size may get overridden if it is smaller than the alignment reported by vkGetImageMemoryRequirements at pool creation
ZEST_API void zest_SetDeviceImagePoolSize(zest_device device, const char *name, zest_image_usage_flags image_flags, zest_memory_property_flags property_flags, zest_size minimum_allocation, zest_size pool_size);
//Create a buffer specifically for use as a uniform buffer. This will also create a descriptor set for the uniform
//buffers as well so it's ready for use in shaders.
ZEST_API zest_uniform_buffer_handle zest_CreateUniformBuffer(zest_context context, const char *name, zest_size uniform_struct_size);
//Free a uniform buffer and all it's resources
ZEST_API void zest_FreeUniformBuffer(zest_uniform_buffer_handle uniform_buffer);
//Get a pointer to a uniform buffer. This will return a void* which you can cast to whatever struct your storing in the uniform buffer. This will get the buffer
//with the current frame in flight index.
ZEST_API void *zest_GetUniformBufferData(zest_uniform_buffer_handle uniform_buffer);
ZEST_API zest_set_layout zest_GetUniformBufferLayout(zest_uniform_buffer_handle buffer);
//Get the descriptor set in a uniform buffer. You can use this when binding a pipeline for a draw call or compute
//dispatch etc. 
ZEST_API zest_descriptor_set zest_GetUniformBufferSet(zest_uniform_buffer_handle buffer);
ZEST_API zest_descriptor_set zest_GetFIFUniformBufferSet(zest_uniform_buffer_handle buffer, zest_uint fif);
ZEST_API zest_uint zest_GetBufferDescriptorIndex(zest_buffer buffer);
//Should only be used in zest implementations only
ZEST_API void *zest_AllocateMemory(zest_context context, zest_size size);
ZEST_API void zest_FreeMemory(zest_context context, void *allocation);
//When you free buffers the platform buffer is added to a list that is either freed at the end of the program
//or you can call this to free them whenever you want.
ZEST_API void zest_FlushUsedBuffers(zest_context context);
//Get the mapped data of a buffer. For CPU visible buffers only
ZEST_API void *zest_BufferData(zest_buffer buffer);
//Get the capacity of a buffer
ZEST_API zest_size zest_BufferSize(zest_buffer buffer);
//Returns the amount of memory current in use by the buffer
ZEST_API zest_size zest_BufferMemoryInUse(zest_buffer buffer);
//Set the current amount of memory in the buffer that you're actually using
ZEST_API void zest_SetBufferMemoryInUse(zest_buffer buffer, zest_size size);
//--End Buffer related

//Helper functions for creating the builtin layers. these can be called separately outside of a command queue setup context
ZEST_API zest_layer_handle zest_CreateMeshLayer(zest_context context, const char *name, zest_size vertex_type_size);
ZEST_API zest_layer_handle zest_CreateInstanceMeshLayer(zest_context context, const char *name);
//-- End Command queue setup and creation

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
//Get the length of a vec without square rooting
ZEST_API float zest_LengthVec3(zest_vec3 const v);
ZEST_API float zest_LengthVec4(zest_vec4 const v);
ZEST_API float zest_Vec2Length2(zest_vec2 const v);
//Get the length of a vec
ZEST_API float zest_LengthVec(zest_vec3 const v);
ZEST_API float zest_Vec2Length(zest_vec2 const v);
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
ZEST_API zest_image_view_create_info_t zest_CreateViewImageInfo(zest_image_handle image_handle);
ZEST_API zest_image_handle zest_CreateImage(zest_context context, zest_image_info_t *create_info);
ZEST_API void zest_FreeImage(zest_image_handle image_handle);
ZEST_API zest_image_view_handle zest_CreateImageView(zest_context context, zest_image_handle image_handle, zest_image_view_create_info_t *create_info);
ZEST_API zest_image_view_array_handle zest_CreateImageViewsPerMip(zest_context context, zest_image_handle image_handle);
ZEST_API void zest_GetFormatPixelData(zest_format format, int *channels, int *bytes_per_pixel);
ZEST_API zest_bool zest_CopyBitmapToImage(zest_context context, void *bitmap, zest_size image_size, zest_image_handle dst_handle, zest_uint width, zest_uint height);
//Get the extent of the image
ZEST_API const zest_image_info_t *zest_ImageInfo(zest_image_handle image_handle);
//Get a descriptor index from an image for a specific binding number. You must have already acquired the index
//for the binding number first with zest_GetGlobalSampler.
ZEST_API zest_uint zest_ImageDescriptorIndex(zest_image_handle image_handle, zest_binding_number_type binding_number);
//Get the raw value of the current image layout of the image as an int. This will be whatever layout is represented
//in the backend api you are using (vulkan, metal, dx12 etc.). You can use this if you just need the current layout
//of the image without translating it into a zest layout enum because you just need to know if the layout changed
//for frame graph caching purposes etc.
ZEST_API int zest_ImageRawLayout(zest_image_handle image_handle);
ZEST_API zest_extent2d_t zest_RegionDimensions(zest_atlas_region_t *region);
ZEST_API zest_extent2d_t zest_RegionDimensions(zest_atlas_region_t *region);
ZEST_API zest_index zest_RegionLayerIndex(zest_atlas_region_t *region);
ZEST_API zest_vec4 zest_RegionUV(zest_atlas_region_t *region);
ZEST_API void zest_BindAtlasRegionToImage(zest_atlas_region_t *region, zest_uint sampler_index, zest_image_handle image_handle, zest_binding_number_type binding_number);

// --Sampler functions
//Gets a sampler from the sampler storage in the renderer. If no match is found for the info that you pass into the sampler
//then a new one will be created.
ZEST_API zest_sampler_handle zest_CreateSampler(zest_context context, zest_sampler_info_t *info);
ZEST_API zest_sampler_info_t zest_CreateSamplerInfo();
//-- End Images and textures

//-----------------------------------------------
//-- Draw_Layers_API
//-----------------------------------------------

//Create a new layer for instanced drawing. This just creates a standard layer with default options and callbacks, all
//you need to pass in is the size of type used for the instance struct that you'll use with whatever pipeline you setup
//to use with the layer.
ZEST_API zest_layer_handle zest_CreateInstanceLayer(zest_context context, const char* name, zest_size type_size);
//Creates a layer with buffers for each frame in flight located on the device. This means that you can manually decide
//When to upload to the buffer on the render graph rather then using transient buffers each frame that will be
//discarded. In order to avoid syncing issues on the GPU, pass a unique id to generate a unique Buffer. This
//id can be shared with any other frame in flight layer that will flip their frame in flight index at the same
//time, like when ever the update loop is run.
ZEST_API zest_layer_handle zest_CreateFIFInstanceLayer(zest_context context, const char *name, zest_size type_size, zest_uint id);
//Create a new layer builder which you can use to build new custom layers to draw with using instances
ZEST_API zest_layer_builder_t zest_NewInstanceLayerBuilder(zest_context context, zest_size type_size);
//Once you have configured your layer you can call this to create the layer ready for adding to a command queue
ZEST_API zest_layer_handle zest_FinishInstanceLayer(const char *name, zest_layer_builder_t *builder);
//Start a new set of draw instructs for a standard zest_layer. These were internal functions but they've been made api functions for making you're own custom
//instance layers more easily
ZEST_API void zest_StartInstanceInstructions(zest_layer_handle layer);
//Set the layer frame in flight to the next layer. Use this if you're manually setting the current fif for the layer so
//that you can avoid uploading the staging buffers every frame and only do so when it's neccessary.
ZEST_API void zest_ResetLayer(zest_layer_handle layer);
//Same as ResetLayer but specifically for an instance layer
ZEST_API void zest_ResetInstanceLayer(zest_layer_handle layer);
//End a set of draw instructs for a standard zest_layer
ZEST_API void zest_EndInstanceInstructions(zest_layer_handle layer);
//Callback that can be used to upload layer data to the gpu
ZEST_API void zest_UploadInstanceLayerData(const zest_command_list command_list, void *user_data);
//For layers that are manually flipping the frame in flight, we can use this to only end the instructions if the last know fif for the layer
//is not equal to the current one. Returns true if the instructions were ended false if not. If true then you can assume that the staging
//buffer for the layer can then be uploaded to the gpu. This should be called in an upload buffer callback in any custom draw routine/layer.
ZEST_API zest_bool zest_MaybeEndInstanceInstructions(zest_layer_handle layer);
//Get the current size in bytes of all instances being drawn in the layer
ZEST_API zest_size zest_GetLayerInstanceSize(zest_layer_handle layer);
//Reset the drawing for an instance layer. This is called after all drawing is done and dispatched to the gpu
ZEST_API void zest_ResetInstanceLayerDrawing(zest_layer_handle layer);
//Get the current amount of instances being drawn in the layer
ZEST_API zest_uint zest_GetInstanceLayerCount(zest_layer_handle layer);
//Get the pointer to the current instance in the layer if it's an instanced based layer (meaning you're drawing instances of sprites, billboards meshes etc.)
//This will return a void* so you can cast it to whatever struct you're using for the instance data
#define zest_GetLayerInstance(type, layer, fif) (type *)layer->memory_refs[fif].instance_ptr
//Move the pointer in memory to the next instance to write to.
ZEST_API void *zest_NextInstance(zest_layer layer);
//Free a layer and all it's resources
ZEST_API void zest_FreeLayer(zest_layer_handle layer);
//Set the viewport of a layer. This is important to set right as when the layer is drawn it needs to be clipped correctly and in a lot of cases match how the
//uniform buffer is setup
ZEST_API void zest_SetLayerViewPort(zest_layer_handle layer, int x, int y, zest_uint scissor_width, zest_uint scissor_height, float viewport_width, float viewport_height);
ZEST_API void zest_SetLayerScissor(zest_layer_handle layer, int offset_x, int offset_y, zest_uint scissor_width, zest_uint scissor_height);
//Update the layer viewport to match the swapchain
ZEST_API void zest_SetLayerSizeToSwapchain(zest_layer_handle layer);
//Set the size of the layer. Called internally to set it to the window size. Can this be internal?
ZEST_API void zest_SetLayerSize(zest_layer_handle layer, float width, float height);
//Set the layer color. This is used to apply color to the sprite/font/billboard that you're drawing or you can use it in your own draw routines that use zest_layers.
//Note that the alpha value here is actually a transition between additive and alpha blending. Where 0 is the most additive and 1 is the most alpha blending. Anything
//imbetween is a combination of the 2.
ZEST_API void zest_SetLayerColor(zest_layer_handle layer, zest_byte red, zest_byte green, zest_byte blue, zest_byte alpha);
ZEST_API void zest_SetLayerColorf(zest_layer_handle layer, float red, float green, float blue, float alpha);
//Set the intensity of the layer. This is used to apply an alpha value to the sprite or billboard you're drawing or you can use it in your own draw routines that
//use zest_layers. Note that intensity levels can exceed 1.f to make your sprites extra bright because of pre-multiplied blending in the sprite.
ZEST_API void zest_SetLayerIntensity(zest_layer_handle layer, float value);
//A dirty layer denotes that it's buffers have changed and therefore needs uploading to the GPU again. This is currently used for Dear Imgui layers.
ZEST_API void zest_SetLayerChanged(zest_layer_handle layer);
//Returns 1 if the layer is marked as changed
ZEST_API zest_bool zest_LayerHasChanged(zest_layer_handle layer);
//Set the user data of a layer. You can use this to extend the functionality of the layers for your own needs.
ZEST_API void zest_SetLayerUserData(zest_layer_handle layer, void *data);
//Get the user data from the layer
#define zest_GetLayerUserData(type, layer) ((type *)layer->user_data)
ZEST_API zest_uint zest_GetLayerVertexDescriptorIndex(zest_layer_handle layer, zest_bool last_frame);
ZEST_API zest_buffer zest_GetLayerResourceBuffer(zest_layer_handle layer);
ZEST_API zest_buffer zest_GetLayerVertexBuffer(zest_layer_handle layer);
ZEST_API zest_buffer zest_GetLayerStagingVertexBuffer(zest_layer_handle layer);
ZEST_API zest_buffer zest_GetLayerStagingIndexBuffer(zest_layer_handle layer);
ZEST_API void zest_UploadLayerStagingData(zest_layer_handle layer, const zest_command_list command_list);
ZEST_API void zest_DrawInstanceLayer(const zest_command_list command_list, void *user_data);
ZEST_API zest_layer_instruction_t *zest_NextLayerInstruction(zest_layer_handle layer_handle);
//-- End Draw Layers


//-----------------------------------------------
//        Draw_instance_layers
//        General purpose functions you can use to either draw built in instances like billboards and sprites or your own custom instances
//-----------------------------------------------
//Set the texture, descriptor set shader_resources and pipeline for any calls to the same layer that come after it. You must call this function if you want to do any instance drawing for a particular layer, and you
//must call it again if you wish to switch either the texture, descriptor set or pipeline to do the drawing. Everytime you call this function it creates a new draw instruction
//in the layer for drawing instances so each call represents a separate draw call in the render. So if you just call this function once you can call a draw instance function as many times
//as you want (like zest_DrawBillboard or your own custom draw instance function) and they will all be drawn with a single draw call.
//Pass in the zest_layer, zest_texture, zest_descriptor_set and zest_pipeline. A few things to note:
//1) The descriptor layout used to create the descriptor sets in the shader_resources must match the layout used in the pipeline.
//2) You can pass 0 in the descriptor set and it will just use the default descriptor set used in the texture.
ZEST_API void zest_SetInstanceDrawing(zest_layer_handle layer, zest_shader_resources_handle shader_resources,  zest_pipeline_template pipeline);
//Draw all the contents in a buffer. You can use this if you prepare all the instance data elsewhere in your code and then want
//to just dump it all into the staging buffer of the layer in one go. This will move the instance pointer in the layer to the next point
//in the buffer as well as bump up the instance count by the amount you pass into the function. The instance buffer will be grown if
//there is not enough room.
//Note that the struct size of the data you're copying MUST be the same size as the layer->struct_size.
ZEST_API zest_draw_buffer_result zest_DrawInstanceBuffer(zest_layer_handle layer, void *src, zest_uint amount);
//In situations where you write directly to a staging buffer you can use this function to simply tell the draw instruction
//how many instances should be drawn. You will still need to call zest_SetInstanceDrawing
ZEST_API void zest_DrawInstanceInstruction(zest_layer_handle layer, zest_uint amount);
//Set the viewport and scissors of the next draw instructions for a layer. Otherwise by default it will use either the screen size
//of or the viewport size you set with zest_SetLayerViewPort
ZEST_API void zest_SetLayerDrawingViewport(zest_layer_handle layer, int x, int y, zest_uint scissor_width, zest_uint scissor_height, float viewport_width, float viewport_height);
//Set the current instruction push contants in the layer
ZEST_API void zest_SetLayerPushConstants(zest_layer_handle layer, void *push_constants, zest_size size);
#define zest_CastLayerPushConstants(type, layer) (type *)layer->current_instruction.push_constant


//-----------------------------------------------
//        Draw_mesh_layers
//        Mesh layers let you upload a vertex and index buffer to draw meshes. I set this up primarily for
//        use with Dear ImGui
//-----------------------------------------------
ZEST_API zest_buffer zest_GetVertexWriteBuffer(zest_layer_handle layer);
//Get the index staging buffer. You'll need to get the staging buffers to copy your mesh data to or even just record mesh data directly to the staging buffer
ZEST_API zest_buffer zest_GetIndexWriteBuffer(zest_layer_handle layer);
//Grow the mesh vertex buffers. You must update the buffer->memory_in_use so that it can decide if a buffer needs growing
ZEST_API void zest_GrowMeshVertexBuffers(zest_layer_handle layer);
//Grow the mesh index buffers. You must update the buffer->memory_in_use so that it can decide if a buffer needs growing
ZEST_API void zest_GrowMeshIndexBuffers(zest_layer_handle layer);
//Set the mesh drawing specifying any texture, descriptor set and pipeline that you want to use for the drawing
ZEST_API void zest_SetMeshDrawing(zest_layer_handle layer, zest_shader_resources_handle shader_resources, zest_pipeline_template pipeline);
//Helper funciton Push a vertex to the vertex staging buffer. It will automatically grow the buffers if needed
ZEST_API void zest_PushVertex(zest_layer_handle layer, float pos_x, float pos_y, float pos_z, float intensity, float uv_x, float uv_y, zest_color_t color, zest_uint parameters);
//Helper funciton Push an index to the index staging buffer. It will automatically grow the buffers if needed
ZEST_API void zest_PushIndex(zest_layer_handle layer, zest_uint offset);
//Callback for the frame graph
ZEST_API void zest_DrawInstanceMeshLayer(const zest_command_list command_list, void *user_data);

//-----------------------------------------------
//        Draw_instance_mesh_layers
//        Instance mesh layers are for creating meshes and then drawing instances of them. It should be
//        one mesh per layer (and obviously many instances of that mesh can be drawn with the layer).
//        Very basic stuff currently, I'm just using them to create 3d widgets I can use in TimelineFX
//        but this can all be expanded on for general 3d models in the future.
//-----------------------------------------------
ZEST_API void zest_SetInstanceMeshDrawing(zest_layer_handle layer, zest_shader_resources_handle shader_resources, zest_pipeline_template pipeline);
//Push a zest_vertex_t to a mesh. Use this and PushMeshTriangle to build a mesh ready to be added to an instance mesh layer
ZEST_API void zest_PushMeshVertex(zest_mesh mesh, float pos_x, float pos_y, float pos_z, zest_color_t color);
//Push an index to a mesh to build triangles
ZEST_API void zest_PushMeshIndex(zest_mesh mesh, zest_uint index);
//Rather then PushMeshIndex you can call this to add three indexes at once to build a triangle in the mesh
ZEST_API void zest_PushMeshTriangle(zest_mesh mesh, zest_uint i1, zest_uint i2, zest_uint i3);
//Create a new mesh and return the handle to it
ZEST_API zest_mesh zest_NewMesh(zest_context context);
//Free the memeory used for the mesh. You can free the mesh once it's been added to the layer
ZEST_API void zest_FreeMesh(zest_mesh mesh);
//Set the position of the mesh in it's transform matrix
ZEST_API void zest_PositionMesh(zest_mesh mesh, zest_vec3 position);
//Rotate a mesh by the given pitch, yaw and roll values (in radians)
ZEST_API zest_matrix4 zest_RotateMesh(zest_mesh mesh, float pitch, float yaw, float roll);
//Transform a mesh by the given pitch, yaw and roll values (in radians), position x, y, z and scale sx, sy, sz
ZEST_API zest_matrix4 zest_TransformMesh(zest_mesh mesh, float pitch, float yaw, float roll, float x, float y, float z, float sx, float sy, float sz);
//Calculate the normals of a mesh. This will unweld the mesh to create faceted normals.
ZEST_API void zest_CalculateNormals(zest_mesh mesh);
//Initialise a new bounding box to 0
ZEST_API zest_bounding_box_t zest_NewBoundingBox();
//Calculate the bounding box of a mesh and return it
ZEST_API zest_bounding_box_t zest_GetMeshBoundingBox(zest_mesh mesh);
//Add all the vertices and indices of a mesh to another mesh to combine them into a single mesh.
ZEST_API void zest_AddMeshToMesh(zest_mesh dst_mesh, zest_mesh src_mesh);
//Set the group id for every vertex in the mesh. This can be used in the shader to identify different parts of the mesh and do different shader stuff with them.
ZEST_API void zest_SetMeshGroupID(zest_mesh mesh, zest_uint group_id);
//Add a mesh to an instanced mesh layer. Existing vertex data in the layer will be deleted.
ZEST_API void zest_AddMeshToLayer(zest_layer_handle layer, zest_mesh src_mesh);
//Get the size in bytes for the vertex data in a mesh
ZEST_API zest_size zest_MeshVertexDataSize(zest_mesh mesh);
//Get the size in bytes for the index data in a mesh
ZEST_API zest_size zest_MeshIndexDataSize(zest_mesh mesh);
//Draw an instance of a mesh with an instanced mesh layer. Pass in the position, rotations and scale to transform the instance.
//You must call zest_SetInstanceDrawing before calling this function as many times as you need.
ZEST_API void zest_DrawInstancedMesh(zest_layer_handle mesh_layer, float pos[3], float rot[3], float scale[3]);
//Create a cylinder mesh of given number of sides, radius and height. Set cap to 1 to cap the cylinder.
ZEST_API zest_mesh zest_CreateCylinder(zest_context context, int sides, float radius, float height, zest_color_t color, zest_bool cap);
//Create a cone mesh of given number of sides, radius and height.
ZEST_API zest_mesh zest_CreateCone(zest_context context, int sides, float radius, float height, zest_color_t color);
//Create a uv sphere mesh made up using a number of horizontal rings and vertical sectors of a give radius.
ZEST_API zest_mesh zest_CreateSphere(zest_context context, int rings, int sectors, float radius, zest_color_t color);
//Create a cube mesh of a given size.
ZEST_API zest_mesh zest_CreateCube(zest_context context, float size, zest_color_t color);
//Create a flat rounded rectangle of a give width and height. Pass in the radius to use for the corners and number of segments to use for the corners.
ZEST_API zest_mesh zest_CreateRoundedRectangle(zest_context context, float width, float height, float radius, int segments, zest_bool backface, zest_color_t color);
//--End Instance Draw mesh layers

//-----------------------------------------------
//        Compute_shaders
//        Build custom compute shaders and integrate into a command queue or just run separately as you need.
//        See zest-compute-example for a full working example
//-----------------------------------------------
//Create a blank ready-to-build compute object and store by name in the renderer.
ZEST_PRIVATE zest_compute zest__new_compute(zest_context context, const char *name);
//To build a compute shader pipeline you can use a zest_compute_builder_t and corresponding commands to add the various settings for the compute object
ZEST_API zest_compute_builder_t zest_BeginComputeBuilder(zest_context context);
ZEST_API void zest_SetComputeBindlessLayout(zest_compute_builder_t *builder, zest_set_layout bindless_layout);
ZEST_API void zest_AddComputeSetLayout(zest_compute_builder_t *builder, zest_set_layout layout);
//Add a shader to the compute builder. This will be the shader that is executed on the GPU. Pass a file path where to find the shader.
ZEST_API zest_index zest_AddComputeShader(zest_compute_builder_t *builder, zest_shader_handle shader);
//If you're using a push constant then you can set it's size in the builder here.
ZEST_API void zest_SetComputePushConstantSize(zest_compute_builder_t *builder, zest_uint size);
//Set any pointer to custom user data here. You will be able to access this in the callback functions.
ZEST_API void zest_SetComputeUserData(zest_compute_builder_t *builder, void *data);
//Sets the compute shader to manually record so you have to specify when the compute shader should be recorded
ZEST_API void zest_SetComputeManualRecord(zest_compute_builder_t *builder);
//Sets the compute shader to use a primary command buffer recorder so that you can use it with the zest_RunCompute command.
//This means that you will not be able to use this compute shader in a frame loop alongside other render routines.
ZEST_API void zest_SetComputePrimaryRecorder(zest_compute_builder_t *builder);
//Once you have finished calling the builder commands you will need to call this to actually build the compute shader. Pass a pointer to the builder and the zest_compute
//handle that you got from calling zest__new_compute. You can then use this handle to add the compute shader to a command queue with zest_NewComputeSetup in a
//command queue context (see the section on Command queue setup and creation)
ZEST_API zest_compute_handle zest_FinishCompute(zest_compute_builder_t *builder, const char *name);
//Reset compute when manual_fif is being used. This means that you can chose when you want a compute command buffer
//to be recorded again. 
ZEST_API void zest_ResetCompute(zest_compute_handle compute);
//Default always true condition for recording compute command buffers
ZEST_API int zest_ComputeConditionAlwaysTrue(zest_compute_handle compute);
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
ZEST_API void zest_FreeFile(zest_context context, zest_file file);
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
//Wait for the device to be idle (finish executing all commands). Only recommended if you need to do a one-off operation like change a texture that could
//still be in use by the GPU
ZEST_API void zest_WaitForIdleDevice(zest_context context);
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
//Print out any reports that have been collected to the console
ZEST_API void zest_PrintReports(zest_context context);
ZEST_PRIVATE void zest__print_block_info(zloc_allocator *allocator, void *allocation, zloc_header *current_block, zest_platform_memory_context context_filter, zest_platform_command command_filter);
ZEST_API void zest_PrintMemoryBlocks(zloc_allocator *allocator, zloc_header *first_block, zest_bool output_all, zest_platform_memory_context context_filter, zest_platform_command command_filter);
ZEST_API zest_uint zest_GetValidationErrorCount(zest_context context);
ZEST_API void zest_ResetValidationErrors(zest_context context);
//--End Debug Helpers

//Helper functions for executing commands on the GPU immediately
//For now this just handles buffer/image copying but will expand this as we go.
ZEST_API void zest_BeginImmediateCommandBuffer(zest_context context);
ZEST_API void zest_EndImmediateCommandBuffer(zest_context context);
//Copy a buffer to another buffer. Generally this will be a staging buffer copying to a buffer on the GPU (device_buffer). You must specify
//the size as well that you want to copy. Must be called inside a zest_BeginOneTimeCommandBuffer.
ZEST_API zest_bool zest_imm_CopyBuffer(zest_context context, zest_buffer src_buffer, zest_buffer dst_buffer, zest_size size);
//Copies an area of a zest_texture to another zest_texture
ZEST_API zest_bool zest_imm_CopyImageToImage(zest_image_handle src_image, zest_image_handle target, int src_x, int src_y, int dst_x, int dst_y, int width, int height);
ZEST_API zest_bool zest_imm_TransitionImage(zest_context context, zest_image_handle image, zest_image_layout new_layout, zest_uint base_mip_index, zest_uint mip_levels, zest_uint base_array_index, zest_uint layer_count);
ZEST_API zest_bool zest_imm_CopyBufferRegionsToImage(zest_context context, zest_buffer_image_copy_t *regions, zest_uint regions_count, zest_buffer buffer, zest_image_handle image_handle);
ZEST_API zest_bool zest_imm_GenerateMipMaps(zest_context context, zest_image_handle image_handle);
//Copies an area of a zest_texture to a zest_bitmap_t.
//ZEST_API zest_bool zest_imm_CopyTextureToBitmap(zest_image_handle src_image, zest_bitmap image, int src_x, int src_y, int dst_x, int dst_y, int width, int height);
//Get the vertex staging buffer. You'll need to get the staging buffers to copy your mesh data to or even just record mesh data directly to the staging buffer

//-----------------------------------------------
// Command_buffer_functions
// All these functions are called inside a frame graph context in callbacks in order to perform commands
// on the GPU. These all require a platform specific implementation
//-----------------------------------------------
ZEST_API void zest_cmd_BlitImageMip(const zest_command_list command_list, zest_resource_node src, zest_resource_node dst, zest_uint mip_to_blit, zest_supported_pipeline_stages pipeline_stage);
ZEST_API void zest_cmd_CopyImageMip(const zest_command_list command_list, zest_resource_node src, zest_resource_node dst, zest_uint mip_to_blit, zest_supported_pipeline_stages pipeline_stage);
// -- Helper functions to insert barrier functions within pass callbacks
ZEST_API void zest_cmd_InsertComputeImageBarrier(const zest_command_list command_list, zest_resource_node resource, zest_uint base_mip);
//Set a screen sized viewport and scissor command in the render pass
ZEST_API void zest_cmd_SetScreenSizedViewport(const zest_command_list command_list, float min_depth, float max_depth);
ZEST_API void zest_cmd_Scissor(const zest_command_list command_list, zest_scissor_rect_t *scissor);
ZEST_API void zest_cmd_ViewPort(const zest_command_list command_list, zest_viewport_t *viewport);
//Create a scissor and view port command. Must be called within a command buffer
ZEST_API void zest_cmd_Clip(const zest_command_list command_list, float x, float y, float width, float height, float minDepth, float maxDepth);
//Bind a pipeline for use in a draw routing. Once you have built the pipeline at some point you will want to actually use it to draw things.
//In order to do that you can bind the pipeline using this function. Just pass in the pipeline handle and a zest_shader_resources. Note that the
//descriptor sets in the shader_resources must be compatible with the layout that is being using in the pipeline. The command buffer used in the binding will be
//whatever is defined in context->renderer->current_command_buffer which will be set when the command queue is recorded. If you need to specify
//a command buffer then call zest_BindPipelineCB instead.
ZEST_API void zest_cmd_BindPipeline(const zest_command_list command_list, zest_pipeline pipeline, zest_descriptor_set *descriptor_set, zest_uint set_count);
//Bind a pipeline for a compute shader
ZEST_API void zest_cmd_BindComputePipeline(const zest_command_list command_list, zest_compute_handle compute, zest_descriptor_set *descriptor_set, zest_uint set_count);
//Bind a pipeline using a shader resource object. The shader resources must match the descriptor layout used in the pipeline that
//you pass to the function. Pass in a manual frame in flight which will be used as the fif for any descriptor set in the shader
//resource that is marked as static.
ZEST_API void zest_cmd_BindPipelineShaderResource(const zest_command_list command_list, zest_pipeline pipeline, zest_shader_resources_handle shader_resources);
//Exactly the same as zest_CopyBuffer but you can specify a command buffer to use to make the copy. This can be useful if you are doing a
//one off copy with a separate command buffer
ZEST_API void zest_cmd_CopyBuffer(const zest_command_list command_list, zest_buffer staging_buffer, zest_buffer device_buffer, zest_size size);
ZEST_API zest_bool zest_cmd_UploadBuffer(const zest_command_list command_list, zest_buffer_uploader_t *uploader);
//Bind a vertex buffer. For use inside a draw routine callback function.
ZEST_API void zest_cmd_BindVertexBuffer(const zest_command_list command_list, zest_uint first_binding, zest_uint binding_count, zest_buffer buffer);
//Bind an index buffer. For use inside a draw routine callback function.
ZEST_API void zest_cmd_BindIndexBuffer(const zest_command_list command_list, zest_buffer buffer);
//Clear an image within a frame graph
ZEST_API zest_bool zest_cmd_ImageClear(const zest_command_list command_list, zest_image_handle image);
ZEST_API void zest_cmd_BindMeshVertexBuffer(const zest_command_list command_list, zest_layer_handle layer);
ZEST_API void zest_cmd_BindMeshIndexBuffer(const zest_command_list command_list, zest_layer_handle layer);
//Send custom push constants. Use inside a compute update command buffer callback function. The push constatns you pass in to the 
//function must be the same size that you set when creating the compute shader
ZEST_API void zest_cmd_SendCustomComputePushConstants(const zest_command_list command_list, zest_compute_handle compute, const void *push_constant);
//Helper function to dispatch a compute shader so you can call this instead of vkCmdDispatch. Specify a command buffer for use in one off dispataches
ZEST_API void zest_cmd_DispatchCompute(const zest_command_list command_list, zest_compute_handle compute, zest_uint group_count_x, zest_uint group_count_y, zest_uint group_count_z);
//Send push constants. For use inside a draw routine callback function. pass in the pipeline,
//and a pointer to the data containing the push constants. The data MUST match the push constant range in the pipeline
ZEST_API void zest_cmd_SendPushConstants(const zest_command_list command_list, zest_pipeline pipeline, void *data);
//Helper function to record the command to draw via a pipeline. Will record with the current command buffer being used in the active command queue. For use inside
//a draw routine callback function
ZEST_API void zest_cmd_Draw(const zest_command_list command_list, zest_uint vertex_count, zest_uint instance_count, zest_uint first_vertex, zest_uint first_instance);
ZEST_API void zest_cmd_DrawLayerInstruction(const zest_command_list command_list, zest_uint vertex_count, zest_layer_instruction_t *instruction);
//Helper function to record the command to draw indexed vertex data. Will record with the current command buffer being used in the active command queue. For use inside
//a draw routine callback function
ZEST_API void zest_cmd_DrawIndexed(const zest_command_list command_list, zest_uint index_count, zest_uint instance_count, zest_uint first_index, int32_t vertex_offset, zest_uint first_instance);

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

#include <math.h>
#include <limits.h>
#include <stddef.h>
#include <string.h>

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
	ZLOC_PRINT_ERROR(ZLOC_ERROR_COLOR"%s: Not enough memory in pool to allocate %zu bytes\n", ZLOC_ERROR_NAME, zloc__map_size);
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
	remote_size = zloc__adjust_size(remote_size, allocator->minimum_allocation_size, zloc__MEMORY_ALIGNMENT);
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
	void* allocation = zloc__reallocate_remote(allocator, block_extension ? zloc_AllocationFromExtensionPtr(block_extension) : block_extension, (remote_size / allocator->minimum_allocation_size) * (allocator->block_extension_size + zloc__BLOCK_POINTER_OFFSET), remote_size);
	return allocation ? (char*)allocation + zloc__MINIMUM_BLOCK_SIZE : 0;
}

int zloc_FreeRemote(zloc_allocator *allocator, void* block_extension) {
	void *allocation = (char*)block_extension - zloc__MINIMUM_BLOCK_SIZE;
	return zloc_Free(allocator, allocation);
}

zloc_linear_allocator_t *zloc_InitialiseLinearAllocator(void *memory, zloc_size size) {
	if (!memory) {
		ZLOC_PRINT_ERROR(ZLOC_ERROR_COLOR"%s: The memory pointer passed in to the initialiser was NULL, did it allocate properly?\n", ZLOC_ERROR_NAME);
		return NULL;
	}
	if (size <= sizeof(zloc_linear_allocator_t) + zloc__MINIMUM_BLOCK_SIZE) {
		ZLOC_PRINT_ERROR(ZLOC_ERROR_COLOR"%s: Size of linear allocotor size is too small. It must be a mimimum of %llu\n", ZLOC_ERROR_NAME, sizeof(zloc_linear_allocator_t) + zloc__MINIMUM_BLOCK_SIZE);
		return NULL;
	}
	zloc_linear_allocator_t *allocator = (zloc_linear_allocator_t *)memory;
	memset(allocator, 0, sizeof(zloc_linear_allocator_t));
	allocator->buffer_size = size;
	allocator->current_offset = sizeof(zloc_linear_allocator_t);
	return allocator;
}

void zloc_ResetLinearAllocator(zloc_linear_allocator_t *allocator) {
	allocator->current_offset = sizeof(zloc_linear_allocator_t);
}

void *zloc_LinearAllocation(zloc_linear_allocator_t *allocator, zloc_size size_requested) {
	if (!allocator) return NULL;

	zloc_size actual_size = size_requested < zloc__MINIMUM_BLOCK_SIZE ? zloc__MINIMUM_BLOCK_SIZE : size_requested;
	zloc_size alignment = sizeof(void *);

	char *current_ptr = (char *)allocator + allocator->current_offset;
	void *aligned_address = (void *)(((uintptr_t)current_ptr + alignment - 1) & ~(alignment - 1));

	zloc_size new_offset = (zloc_size)((char *)aligned_address - (char *)allocator) + actual_size;

	if (new_offset > allocator->buffer_size) {
		ZLOC_PRINT_ERROR(ZLOC_ERROR_COLOR"%s: Out of memory in linear allocator.\n", ZLOC_ERROR_NAME);
		return NULL;
	}

	allocator->current_offset = new_offset;
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

#include "zest_impl.h"

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