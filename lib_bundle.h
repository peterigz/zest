/*
this file bundles together a few single header c libraries for simplicity:
zloc.h				A simple memory allocator
stb_image.h			Image loader
stb_rect_pack.h		Rect packer
stb_resize.h		Image resizer
*/


/*	Pocket Allocator, a Two Level Segregated Fit memory allocator

	This software is dual-licensed. See bottom of file for license details.
*/

#ifndef ZLOC_INCLUDE_H
#define ZLOC_INCLUDE_H

#include <stdlib.h>
#include <stddef.h>
#include <assert.h>

//Header
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
		zloc__BLOCK_POINTER_OFFSET = sizeof(void*) + sizeof(zloc_size),
		zloc__MINIMUM_BLOCK_SIZE = 16,
		zloc__BLOCK_SIZE_OVERHEAD = sizeof(zloc_size),
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
		/*
		User allocation will start here when the block is used. When the block is free prev and next
		are pointers in a linked list of free blocks within the same class size of blocks
		*/
		struct zloc_header *prev_free_block;
		struct zloc_header *next_free_block;
	} zloc_header;

	typedef struct zloc_allocator {
		/*	This is basically a terminator block that free blocks can point to if they're at the end
			of a free list. */
		zloc_header null_block;
#if defined(ZLOC_THREAD_SAFE)
		/* Multithreading protection*/
		volatile zloc_thread_access access;
#endif
#if defined(ZLOC_ENABLE_REMOTE_MEMORY)
		void *user_data;
		zloc_size(*get_block_size_callback)(const zloc_header* block);
		void(*merge_next_callback)(void *user_data, zloc_header* block, zloc_header *next_block);
		void(*merge_prev_callback)(void *user_data, zloc_header* prev_block, zloc_header *block);
		void(*split_block_callback)(void *user_data, zloc_header* block, zloc_header* trimmed_block, zloc_size remote_size);
		void(*add_pool_callback)(void *user_data, void* block_extension);
		void(*unable_to_reallocate_callback)(void *user_data, zloc_header *block, zloc_header *new_block);
		zloc_size block_extension_size;
#endif
		zloc_size minimum_allocation_size;
		/*	Here we store all of the free block data. first_level_bitmap is either a 32bit int
		or 64bit depending on whether zloc__64BIT is set. Second_level_bitmaps are an array of 32bit
		ints. segregated_lists is a two level array pointing to free blocks or null_block if the list
		is empty. */
		zloc_fl_bitmap first_level_bitmap;
		zloc_sl_bitmap second_level_bitmaps[zloc__FIRST_LEVEL_INDEX_COUNT];
		zloc_header *segregated_lists[zloc__FIRST_LEVEL_INDEX_COUNT][zloc__SECOND_LEVEL_INDEX_COUNT];
	} zloc_allocator;

#if defined(ZLOC_ENABLE_REMOTE_MEMORY)
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
#define zloc__do_merge_next_callback allocator->merge_next_callback(allocator->user_data, block, next_block)
#define zloc__do_merge_prev_callback allocator->merge_prev_callback(allocator->user_data, prev_block, block)
#define zloc__do_split_block_callback allocator->split_block_callback(allocator->user_data, block, trimmed, remote_size)
#define zloc__do_add_pool_callback allocator->add_pool_callback(allocator->user_data, block)
#define zloc__do_unable_to_reallocate_callback zloc_header *new_block = zloc__block_from_allocation(allocation); zloc_header *block = zloc__block_from_allocation(ptr); allocator->unable_to_reallocate_callback(allocator->user_data, block, new_block)
#define zloc__block_extension_size (allocator->block_extension_size & ~1)
#define zloc__call_maybe_split_block zloc__maybe_split_block(allocator, block, size, remote_size) 
#else
#define zloc__map_size size
#define zloc__do_size_class_callback(block) zloc__block_size(block)
#define zloc__do_merge_next_callback
#define zloc__do_merge_prev_callback
#define zloc__do_split_block_callback
#define zloc__do_add_pool_callback
#define zloc__do_unable_to_reallocate_callback
#define zloc__block_extension_size 0
#define zloc__call_maybe_split_block zloc__maybe_split_block(allocator, block, size, 0) 
#endif

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

	/*
	User functions
	This are the main functions you'll need to use this library, everything else is either internal private functions or functions for debugging
	*/

	/*
		Initialise an allocator. Pass a block of memory that you want to use to store the allocator data. This will not create a pool, only the
		necessary data structures to store the allocator.

		@param	void*					A pointer to some previously allocated memory that was created with malloc, VirtualAlloc etc.
		@param	zloc_size				The size of the memory you're passing
		@returns zloc_allocator*			A pointer to a zloc_allocator which you'll need to use when calling zloc_Allocate or zloc_Free. Note that
										this pointer will be the same address as the memory you're passing in as all the information the allocator
										stores to organise memory blocks is stored at the beginning of the memory.
										If something went wrong then 0 is returned. Define ZLOC_OUTPUT_ERROR_MESSAGES before including this header
										file to see any errors in the console.
	*/
	ZLOC_API zloc_allocator *zloc_InitialiseAllocator(void *memory);

	/*
		Initialise an allocator and a pool at the same time. The data stucture to store the allocator will be stored at the beginning of the memory
		you pass to the function and the remaining memory will be used as the pool.

		@param	void*					A pointer to some previously allocated memory that was created with malloc, VirtualAlloc etc.
		@param	zloc_size				The size of the memory you're passing
		@returns zloc_allocator*		A pointer to a zloc_allocator which you'll need to use when calling zloc_Allocate or zloc_Free.
										If something went wrong then 0 is returned. Define ZLOC_OUTPUT_ERROR_MESSAGES before including this header
										file to see any errors in the console.
	*/
	ZLOC_API zloc_allocator *zloc_InitialiseAllocatorWithPool(void *memory, zloc_size size);

	/*
		Add a new memory pool to the allocator. Pools don't have to all be the same size, adding a pool will create the biggest block it can within
		the pool and then add that to the segregated list of free blocks in the allocator. All the pools in the allocator will be naturally linked
		together in the segregated list because all blocks are linked together with a linked list either as physical neighbours or free blocks in
		the segregated list.

		@param	zloc_allocator*			A pointer to some previously initialised allocator
		@param	void*					A pointer to some previously allocated memory that was created with malloc, VirtualAlloc etc.
		@param	zloc_size				The size of the memory you're passing
		@returns zloc_pool*				A pointer to the pool
	*/
	ZLOC_API zloc_pool *zloc_AddPool(zloc_allocator *allocator, void *memory, zloc_size size);

	/*
		Get the structure size of an allocator. You can use this to take into account the overhead of the allocator when preparing a new allocator
		with memory pool.

		@returns zloc_size				The struct size of the allocator in bytes
	*/
	ZLOC_API zloc_size zloc_AllocatorSize(void);

	/*
		If you initialised an allocator with a pool then you can use this function to get a pointer to the start of the pool. It won't get a pointer
		to any other pool in the allocator. You can just get that when you call zloc_AddPool.

		@param	zloc_allocator*			A pointer to some previously initialised allocator
		@returns zloc_pool				A pointer to the pool memory in the allocator
	*/
	ZLOC_API zloc_pool *zloc_GetPool(zloc_allocator *allocator);

	/*
		Allocate some memory within a zloc_allocator of the specified size. Minimum size is 16 bytes.

		@param	zloc_allocator			A pointer to an initialised zloc_allocator
		@param	zloc_size				The size of the memory you're passing
		@returns void*					A pointer to the block of memory that is allocated. Returns 0 if it was unable to allocate the memory due to
										no free memory. If that happens then you may want to add a pool at that point.
	*/
	ZLOC_API void *zloc_Allocate(zloc_allocator *allocator, zloc_size size);

	/*
		Try to reallocate an existing memory block within the allocator. If possible the current block will be merged with the physical neigbouring
		block, otherwise a normal zloc_Allocate will take place and the data copied over to the new allocation.

		@param	zloc_size				The size of the memory you're passing
		@param	void*					A ptr to the current memory you're reallocating
		@returns void*					A pointer to the block of memory that is allocated. Returns 0 if it was unable to allocate the memory due to
										no free memory. If that happens then you may want to add a pool at that point.
	*/
	ZLOC_API void *zloc_Reallocate(zloc_allocator *allocator, void *ptr, zloc_size size);

	/*
	Allocate some memory within a zloc_allocator of the specified size. Minimum size is 16 bytes.

	@param	zloc_allocator			A pointer to an initialised zloc_allocator
	@param	zloc_size				The size of the memory you're passing
	@returns void*					A pointer to the block of memory that is allocated. Returns 0 if it was unable to allocate the memory due to
									no free memory. If that happens then you may want to add a pool at that point.
*/
	ZLOC_API void *zloc_AllocateAligned(zloc_allocator *allocator, zloc_size size, zloc_size alignment);

	/*
		Free an allocation from a zloc_allocator. When freeing a block of memory any adjacent free blocks are merged together to keep on top of
		fragmentation as much as possible. A check is also done to confirm that the block being freed is still valid and detect any memory corruption
		due to out of bounds writing of this or potentially other blocks.

		It's recommended to call this function with an assert: ZLOC_ASSERT(zloc_Free(allocator, allocation));
		An error is also output to console as long as ZLOC_OUTPUT_ERROR_MESSAGES is defined.

		@returns int		returns 1 if the allocation was successfully freed, 0 otherwise.
	*/
	ZLOC_API int zloc_Free(zloc_allocator *allocator, void *allocation);

	/*
		Remove a pool from an allocator. Note that all blocks in the pool must be free and therefore all merged together into one block (this happens
		automatically as all blocks are freed are merged together into bigger blocks.

		@param zloc_allocator*			A pointer to a tcoc_allocator that you want to reset
		@param zloc_allocator*			A pointer to the memory pool that you want to free. You get this pointer when you add a pool to the allocator.
	*/
	ZLOC_API zloc_bool zloc_RemovePool(zloc_allocator *allocator, zloc_pool *pool);

	/*
	When using an allocator for managing remote memory, you need to set the bytes per block that a block storing infomation about the remote
	memory allocation will manage. For example you might set the value to 1MB so if you were to then allocate 4MB of remote memory then 4 blocks
	worth of space would be used to allocate that memory. This means that if it were to be freed and then split down to a smaller size they'd be
	enough blocks worth of space to do this.

	Note that the lower the number the more memory you need to track remote memory blocks but the more granular it will be. It will depend alot
	on the size of allocations you will need

	@param zloc_allocator*			A pointer to an initialised allocator
	@param zloc_size				The bytes per block you want it to be set to. Must be a power of 2
*/
	ZLOC_API void zloc_SetMinimumAllocationSize(zloc_allocator *allocator, zloc_size size);
    zloc_pool_stats_t zloc_CreateMemorySnapshot(zloc_header *first_block);

#if defined(ZLOC_ENABLE_REMOTE_MEMORY)
	/*
		Initialise an allocator and a pool at the same time and flag it for use as a remote memory manager.
		The data stucture to store the allocator will be stored at the beginning of the memory you pass to the function and the remaining memory will
		be used as the pool. Use with zloc_CalculateRemoteBlockPoolSize to allow for the number of memory ranges you might need to manage in the
		remote memory pool(s)

		@param	void*					A pointer to some previously allocated memory that was created with malloc, VirtualAlloc etc.
		@param	zloc_size				The size of the memory you're passing
		@returns zloc_allocator*		A pointer to a zloc_allocator which you'll need to use when calling zloc_AllocateRemote or zloc_FreeRemote.
										If something went wrong then 0 is returned. Define ZLOC_OUTPUT_ERROR_MESSAGES before including this header
										file to see any errors in the console.
	*/
	ZLOC_API zloc_allocator *zloc_InitialiseAllocatorForRemote(void *memory);

	/*
		When using an allocator for managing remote memory, you need to set the size of the struct that you will be using to store information about
		the remote block of memory. This will be like an extension the existing zloc_header.

		@param zloc_allocator*			A pointer to an initialised allocator
		@param zloc_size				The size of the block extension. Will be aligned up to zloc__MEMORY_ALIGNMENT
	*/
	ZLOC_API void zloc_SetBlockExtensionSize(zloc_allocator *allocator, zloc_size size);

	/*
		Free a remote allocation from a zloc_allocator. You must have set up merging callbacks so that you can update your block extensions with the
		necessary buffer sizes and offsets

		It's recommended to call this function with an assert: ZLOC_ASSERT(zloc_FreeRemote(allocator, allocation));
		An error is also output to console as long as ZLOC_OUTPUT_ERROR_MESSAGES is defined.

		@returns int		returns 1 if the allocation was successfully freed, 0 otherwise.
	*/
	ZLOC_API int zloc_FreeRemote(zloc_allocator *allocator, void *allocation);

	/*
		Allocate some memory in a remote location from the normal heap. This is generally for allocating GPU memory.

		@param	zloc_allocator			A pointer to an initialised zloc_allocator
		@param	zloc_size				The size of the memory you're passing which should be the size of the block with the information about the
										buffer you're creating in the remote location
		@param	zloc_size				The remote size of the memory you're passing
		@returns void*					A pointer to the block of memory that is allocated. Returns 0 if it was unable to allocate the memory due to
										no free memory. If that happens then you may want to add a pool at that point.
	*/
	ZLOC_API void *zloc_AllocateRemote(zloc_allocator *allocator, zloc_size remote_size);

	/*
		Get the size of a block plus the block extension size so that you can use this to create an allocator pool to store all the blocks that will
		track the remote memory. Be sure that you have already called and set the block extension size with zloc_SetBlockExtensionSize.

		@param	zloc_allocator			A pointer to an initialised zloc_allocator
		@returns zloc_size				The size of the block
	*/
	ZLOC_API zloc_size zloc_CalculateRemoteBlockPoolSize(zloc_allocator *allocator, zloc_size remote_pool_size);

	ZLOC_API void zloc_AddRemotePool(zloc_allocator *allocator, void *block_memory, zloc_size block_memory_size, zloc_size remote_pool_size);

	ZLOC_API void* zloc_BlockUserExtensionPtr(const zloc_header *block);

	ZLOC_API void* zloc_AllocationFromExtensionPtr(const void *block);

    //Very simple linear allocator.
    typedef struct zloc_linear_allocator_t {
        zloc_size buffer_size;
        zloc_size current_offset;
    } zloc_linear_allocator_t;

    /*
        Initialise a linear allocator. Best used for transient data. All memory allocated from here is
        aligned to 8 bytes.
		@param	void*			A pointer to a block of memory you want to use for the arena
		@returns zloc_size		The size of the memory.
    */
    zloc_linear_allocator_t *zloc_InitialiseLinearAllocator(void *memory, zloc_size size);

    /*
        Reset the linear allocator to 0 size used.
    */
	void zloc_ResetLinearAllocator(zloc_linear_allocator_t *allocator);

    /*
        Allocate a block of memory from a linear allocator
		@param	zloc_linear_allocator*			A pointer to a linear allocator
		@returns zloc_size		                The size of the memory that you want to allocate from the linear allocator
    */
	void *zloc_LinearAllocation(zloc_linear_allocator_t *allocator, zloc_size size_requested);

    /*  Get the current offset of the allocator. This can be use to reset to a point in the allocator rather
        then reset the whole thing
		@param	zloc_linear_allocator*			A pointer to a linear allocator
    */
    zloc_size zloc_GetMarker(zloc_linear_allocator_t *allocator);

    /*  Reset the allocator to a specific point that you got using zloc_GetMarker
        then reset the whole thing
		@param	zloc_linear_allocator*			A pointer to a linear allocator
		@param	zloc_size			            The marker offset to reset to
    */
    void zloc_ResetToMarker(zloc_linear_allocator_t *allocator, zloc_size marker);

#endif

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

#if defined(ZLOC_ENABLE_REMOTE_MEMORY)
	static inline void zloc__null_merge_callback(void *user_data, zloc_header *block1, zloc_header *block2) { return; }
	void zloc__remote_merge_next_callback(void *user_data, zloc_header *block1, zloc_header *block2);
	void zloc__remote_merge_prev_callback(void *user_data, zloc_header *block1, zloc_header *block2);
	zloc_size zloc__get_remote_size(const zloc_header *block1);
	static inline void zloc__null_split_callback(void *user_data, zloc_header *block, zloc_header *trimmed, zloc_size remote_size) { return; }
	static inline void zloc__null_add_pool_callback(void *user_data, void *block) { return; }
	static inline void zloc__null_unable_to_reallocate_callback(void *user_data, zloc_header *block, zloc_header *new_block) { return; }
	static inline void zloc__unset_remote_block_limit_reached(zloc_allocator *allocator) { allocator->block_extension_size &= ~1; };
#endif

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
	do {																	\
	} while (0 != zloc__compare_and_exchange(&allocator->access, 1, 0));	\
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

#ifdef __cplusplus
}
#endif

#endif

//Implementation
#if defined(ZLOC_IMPLEMENTATION)

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

#if defined(ZLOC_ENABLE_REMOTE_MEMORY)
	allocator->get_block_size_callback = zloc__block_size;
	allocator->merge_next_callback = zloc__null_merge_callback;
	allocator->merge_prev_callback = zloc__null_merge_callback;
	allocator->split_block_callback = zloc__null_split_callback;
	allocator->add_pool_callback = zloc__null_add_pool_callback;
	allocator->unable_to_reallocate_callback = zloc__null_unable_to_reallocate_callback;
#endif

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
	return (void*)((char*)allocator + zloc_AllocatorSize());
}

zloc_pool *zloc_AddPool(zloc_allocator *allocator, void *memory, zloc_size size) {
	zloc__lock_thread_access;

	//Offset it back by the pointer size, we don't need the prev_physical block pointer as there is none
	//for the first block in the pool
	zloc_header *block = zloc__first_block_in_pool(memory);
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

	last_block->prev_physical_block = block;
	zloc__push_block(allocator, block);

	zloc__unlock_thread_access;
	return memory;
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

#if defined(ZLOC_ENABLE_REMOTE_MEMORY)
void *zloc__allocate(zloc_allocator *allocator, zloc_size size, zloc_size remote_size) {
#else
void *zloc_Allocate(zloc_allocator *allocator, zloc_size size) {
	zloc_size remote_size = 0;
#endif
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
		return zloc_Allocate(allocator, size);
	}

	zloc_header *block = zloc__block_from_allocation(ptr);
	zloc_header *next_block = zloc__next_physical_block(block);
	void *allocation = 0;
	zloc_size current_size = zloc__block_size(block);
	zloc_size adjusted_size = zloc__adjust_size(size, allocator->minimum_allocation_size, zloc__MEMORY_ALIGNMENT);
	zloc_size combined_size = current_size + zloc__block_size(next_block);
	if ((!zloc__next_block_is_free(block) || adjusted_size > combined_size) && adjusted_size > current_size) {
		zloc_header *block = zloc__find_free_block(allocator, adjusted_size, 0);
		if (block) {
			allocation = zloc__block_user_ptr(block);
		}
		
		if (allocation) {
			zloc_size smallest_size = zloc__Min(current_size, size);
			memcpy(allocation, ptr, smallest_size);
			zloc__do_unable_to_reallocate_callback;
			zloc__unlock_thread_access;
			zloc_Free(allocator, ptr);
			zloc__lock_thread_access;
		}
	}
	else {
		//Reallocation is possible
		if (adjusted_size > current_size)
		{
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

int zloc_SafeCopyBlock(void *dst_block_start, void *dst, void *src, zloc_size size) {
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
    } else {
        stats.used_blocks++;
        stats.used_size += zloc__block_size(current_block);
    }
    return stats;
}

#if defined(ZLOC_ENABLE_REMOTE_MEMORY)
/*
	Standard callbacks, you can copy paste these to replace with your own as needed to add any extra functionality
	that you might need
*/
void zloc__remote_merge_next_callback(void *user_data, zloc_header *block, zloc_header *next_block) {
	zloc_remote_header *remote_block = (zloc_remote_header*)zloc_BlockUserExtensionPtr(block);
	zloc_remote_header *next_remote_block = (zloc_remote_header*)zloc_BlockUserExtensionPtr(next_block);
	remote_block->size += next_remote_block->size;
	next_remote_block->memory_offset = 0;
	next_remote_block->size = 0;
}

void zloc__remote_merge_prev_callback(void *user_data, zloc_header *prev_block, zloc_header *block) {
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

	void *block = zloc_BlockUserExtensionPtr(zloc__first_block_in_pool(block_memory));
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
		zloc_header *block = zloc__find_free_block(allocator, size, remote_size);
		if (block) {
			allocation = zloc__block_user_ptr(block);
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

#endif


#endif

/*
------------------------------------------------------------------------------
This software is available under 2 licenses -- choose whichever you prefer.
------------------------------------------------------------------------------
ALTERNATIVE A - MIT License
Copyright (c) 2023 Peter Rigby
Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files (the "Software"), to deal in
the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
of the Software, and to permit persons to whom the Software is furnished to do
so, subject to the following conditions:
The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.
THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
------------------------------------------------------------------------------
ALTERNATIVE B - Public Domain (www.unlicense.org)
This is free and unencumbered software released into the public domain.
Anyone is free to copy, modify, publish, use, compile, sell, or distribute this
software, either in source code form or as a compiled binary, for any purpose,
commercial or non-commercial, and by any means.
In jurisdictions that recognize copyright laws, the author or authors of this
software dedicate any and all copyright interest in the software to the public
domain. We make this dedication for the benefit of the public at large and to
the detriment of our heirs and successors. We intend this dedication to be an
overt act of relinquishment in perpetuity of all present and future rights to
this software under copyright law.
THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
------------------------------------------------------------------------------
*/



/* stb_image_resize - v0.96 - public domain image resizing
   by Jorge L Rodriguez (@VinoBS) - 2014
   http://github.com/nothings/stb

   Written with emphasis on usability, portability, and efficiency. (No
   SIMD or threads, so it be easily outperformed by libs that use those.)
   Only scaling and translation is supported, no rotations or shears.
   Easy API downsamples w/Mitchell filter, upsamples w/cubic interpolation.

   COMPILING & LINKING
	  In one C/C++ file that #includes this file, do this:
		 #define STB_IMAGE_RESIZE_IMPLEMENTATION
	  before the #include. That will create the implementation in that file.

   QUICKSTART
	  stbir_resize_uint8(      input_pixels , in_w , in_h , 0,
							   output_pixels, out_w, out_h, 0, num_channels)
	  stbir_resize_float(...)
	  stbir_resize_uint8_srgb( input_pixels , in_w , in_h , 0,
							   output_pixels, out_w, out_h, 0,
							   num_channels , alpha_chan  , 0)
	  stbir_resize_uint8_srgb_edgemode(
							   input_pixels , in_w , in_h , 0,
							   output_pixels, out_w, out_h, 0,
							   num_channels , alpha_chan  , 0, STBIR_EDGE_CLAMP)
															// WRAP/REFLECT/ZERO

   FULL API
	  See the "header file" section of the source for API documentation.

   ADDITIONAL DOCUMENTATION

	  SRGB & FLOATING POINT REPRESENTATION
		 The sRGB functions presume IEEE floating point. If you do not have
		 IEEE floating point, define STBIR_NON_IEEE_FLOAT. This will use
		 a slower implementation.

	  MEMORY ALLOCATION
		 The resize functions here perform a single memory allocation using
		 malloc. To control the memory allocation, before the #include that
		 triggers the implementation, do:

			#define STBIR_MALLOC(size,context) ...
			#define STBIR_FREE(ptr,context)   ...

		 Each resize function makes exactly one call to malloc/free, so to use
		 temp memory, store the temp memory in the context and return that.

	  ASSERT
		 Define STBIR_ASSERT(boolval) to override assert() and not use assert.h

	  OPTIMIZATION
		 Define STBIR_SATURATE_INT to compute clamp values in-range using
		 integer operations instead of float operations. This may be faster
		 on some platforms.

	  DEFAULT FILTERS
		 For functions which don't provide explicit control over what filters
		 to use, you can change the compile-time defaults with

			#define STBIR_DEFAULT_FILTER_UPSAMPLE     STBIR_FILTER_something
			#define STBIR_DEFAULT_FILTER_DOWNSAMPLE   STBIR_FILTER_something

		 See stbir_filter in the header-file section for the list of filters.

	  NEW FILTERS
		 A number of 1D filter kernels are used. For a list of
		 supported filters see the stbir_filter enum. To add a new filter,
		 write a filter function and add it to stbir__filter_info_table.

	  PROGRESS
		 For interactive use with slow resize operations, you can install
		 a progress-report callback:

			#define STBIR_PROGRESS_REPORT(val)   some_func(val)

		 The parameter val is a float which goes from 0 to 1 as progress is made.

		 For example:

			static void my_progress_report(float progress);
			#define STBIR_PROGRESS_REPORT(val) my_progress_report(val)

			#define STB_IMAGE_RESIZE_IMPLEMENTATION
			#include "stb_image_resize.h"

			static void my_progress_report(float progress)
			{
			   printf("Progress: %f%%\n", progress*100);
			}

	  MAX CHANNELS
		 If your image has more than 64 channels, define STBIR_MAX_CHANNELS
		 to the max you'll have.

	  ALPHA CHANNEL
		 Most of the resizing functions provide the ability to control how
		 the alpha channel of an image is processed. The important things
		 to know about this:

		 1. The best mathematically-behaved version of alpha to use is
		 called "premultiplied alpha", in which the other color channels
		 have had the alpha value multiplied in. If you use premultiplied
		 alpha, linear filtering (such as image resampling done by this
		 library, or performed in texture units on GPUs) does the "right
		 thing". While premultiplied alpha is standard in the movie CGI
		 industry, it is still uncommon in the videogame/real-time world.

		 If you linearly filter non-premultiplied alpha, strange effects
		 occur. (For example, the 50/50 average of 99% transparent bright green
		 and 1% transparent black produces 50% transparent dark green when
		 non-premultiplied, whereas premultiplied it produces 50%
		 transparent near-black. The former introduces green energy
		 that doesn't exist in the source image.)

		 2. Artists should not edit premultiplied-alpha images; artists
		 want non-premultiplied alpha images. Thus, art tools generally output
		 non-premultiplied alpha images.

		 3. You will get best results in most cases by converting images
		 to premultiplied alpha before processing them mathematically.

		 4. If you pass the flag STBIR_FLAG_ALPHA_PREMULTIPLIED, the
		 resizer does not do anything special for the alpha channel;
		 it is resampled identically to other channels. This produces
		 the correct results for premultiplied-alpha images, but produces
		 less-than-ideal results for non-premultiplied-alpha images.

		 5. If you do not pass the flag STBIR_FLAG_ALPHA_PREMULTIPLIED,
		 then the resizer weights the contribution of input pixels
		 based on their alpha values, or, equivalently, it multiplies
		 the alpha value into the color channels, resamples, then divides
		 by the resultant alpha value. Input pixels which have alpha=0 do
		 not contribute at all to output pixels unless _all_ of the input
		 pixels affecting that output pixel have alpha=0, in which case
		 the result for that pixel is the same as it would be without
		 STBIR_FLAG_ALPHA_PREMULTIPLIED. However, this is only true for
		 input images in integer formats. For input images in float format,
		 input pixels with alpha=0 have no effect, and output pixels
		 which have alpha=0 will be 0 in all channels. (For float images,
		 you can manually achieve the same result by adding a tiny epsilon
		 value to the alpha channel of every image, and then subtracting
		 or clamping it at the end.)

		 6. You can suppress the behavior described in #5 and make
		 all-0-alpha pixels have 0 in all channels by #defining
		 STBIR_NO_ALPHA_EPSILON.

		 7. You can separately control whether the alpha channel is
		 interpreted as linear or affected by the colorspace. By default
		 it is linear; you almost never want to apply the colorspace.
		 (For example, graphics hardware does not apply sRGB conversion
		 to the alpha channel.)

   CONTRIBUTORS
	  Jorge L Rodriguez: Implementation
	  Sean Barrett: API design, optimizations
	  Aras Pranckevicius: bugfix
	  Nathan Reed: warning fixes

   REVISIONS
	  0.97 (2020-02-02) fixed warning
	  0.96 (2019-03-04) fixed warnings
	  0.95 (2017-07-23) fixed warnings
	  0.94 (2017-03-18) fixed warnings
	  0.93 (2017-03-03) fixed bug with certain combinations of heights
	  0.92 (2017-01-02) fix integer overflow on large (>2GB) images
	  0.91 (2016-04-02) fix warnings; fix handling of subpixel regions
	  0.90 (2014-09-17) first released version

   LICENSE
	 See end of file for license information.

   TODO
	  Don't decode all of the image data when only processing a partial tile
	  Don't use full-width decode buffers when only processing a partial tile
	  When processing wide images, break processing into tiles so data fits in L1 cache
	  Installable filters?
	  Resize that respects alpha test coverage
		 (Reference code: FloatImage::alphaTestCoverage and FloatImage::scaleAlphaToCoverage:
		 https://code.google.com/p/nvidia-texture-tools/source/browse/trunk/src/nvimage/FloatImage.cpp )
*/

#ifndef STBIR_INCLUDE_STB_IMAGE_RESIZE_H
#define STBIR_INCLUDE_STB_IMAGE_RESIZE_H

#ifdef _MSC_VER
typedef unsigned char  stbir_uint8;
typedef unsigned short stbir_uint16;
typedef unsigned int   stbir_uint32;
#else
#include <stdint.h>
typedef uint8_t  stbir_uint8;
typedef uint16_t stbir_uint16;
typedef uint32_t stbir_uint32;
#endif

#ifndef STBIRDEF
#ifdef STB_IMAGE_RESIZE_STATIC
#define STBIRDEF static
#else
#ifdef __cplusplus
#define STBIRDEF extern "C"
#else
#define STBIRDEF extern
#endif
#endif
#endif

//////////////////////////////////////////////////////////////////////////////
//
// Easy-to-use API:
//
//     * "input pixels" points to an array of image data with 'num_channels' channels (e.g. RGB=3, RGBA=4)
//     * input_w is input image width (x-axis), input_h is input image height (y-axis)
//     * stride is the offset between successive rows of image data in memory, in bytes. you can
//       specify 0 to mean packed continuously in memory
//     * alpha channel is treated identically to other channels.
//     * colorspace is linear or sRGB as specified by function name
//     * returned result is 1 for success or 0 in case of an error.
//       #define STBIR_ASSERT() to trigger an assert on parameter validation errors.
//     * Memory required grows approximately linearly with input and output size, but with
//       discontinuities at input_w == output_w and input_h == output_h.
//     * These functions use a "default" resampling filter defined at compile time. To change the filter,
//       you can change the compile-time defaults by #defining STBIR_DEFAULT_FILTER_UPSAMPLE
//       and STBIR_DEFAULT_FILTER_DOWNSAMPLE, or you can use the medium-complexity API.

STBIRDEF int stbir_resize_uint8(const unsigned char *input_pixels, int input_w, int input_h, int input_stride_in_bytes,
	unsigned char *output_pixels, int output_w, int output_h, int output_stride_in_bytes,
	int num_channels);

STBIRDEF int stbir_resize_float(const float *input_pixels, int input_w, int input_h, int input_stride_in_bytes,
	float *output_pixels, int output_w, int output_h, int output_stride_in_bytes,
	int num_channels);


// The following functions interpret image data as gamma-corrected sRGB.
// Specify STBIR_ALPHA_CHANNEL_NONE if you have no alpha channel,
// or otherwise provide the index of the alpha channel. Flags value
// of 0 will probably do the right thing if you're not sure what
// the state_flags mean.

#define STBIR_ALPHA_CHANNEL_NONE       -1

// Set this flag if your texture has premultiplied alpha. Otherwise, stbir will
// use alpha-weighted resampling (effectively premultiplying, resampling,
// then unpremultiplying).
#define STBIR_FLAG_ALPHA_PREMULTIPLIED    (1 << 0)
// The specified alpha channel should be handled as gamma-corrected value even
// when doing sRGB operations.
#define STBIR_FLAG_ALPHA_USES_COLORSPACE  (1 << 1)

STBIRDEF int stbir_resize_uint8_srgb(const unsigned char *input_pixels, int input_w, int input_h, int input_stride_in_bytes,
	unsigned char *output_pixels, int output_w, int output_h, int output_stride_in_bytes,
	int num_channels, int alpha_channel, int flags);


typedef enum
{
	STBIR_EDGE_CLAMP = 1,
	STBIR_EDGE_REFLECT = 2,
	STBIR_EDGE_WRAP = 3,
	STBIR_EDGE_ZERO = 4,
} stbir_edge;

// This function adds the ability to specify how requests to sample off the edge of the image are handled.
STBIRDEF int stbir_resize_uint8_srgb_edgemode(const unsigned char *input_pixels, int input_w, int input_h, int input_stride_in_bytes,
	unsigned char *output_pixels, int output_w, int output_h, int output_stride_in_bytes,
	int num_channels, int alpha_channel, int flags,
	stbir_edge edge_wrap_mode);

//////////////////////////////////////////////////////////////////////////////
//
// Medium-complexity API
//
// This extends the easy-to-use API as follows:
//
//     * Alpha-channel can be processed separately
//       * If alpha_channel is not STBIR_ALPHA_CHANNEL_NONE
//         * Alpha channel will not be gamma corrected (unless state_flags&STBIR_FLAG_GAMMA_CORRECT)
//         * Filters will be weighted by alpha channel (unless state_flags&STBIR_FLAG_ALPHA_PREMULTIPLIED)
//     * Filter can be selected explicitly
//     * uint16 image type
//     * sRGB colorspace available for all types
//     * context parameter for passing to STBIR_MALLOC

typedef enum
{
	STBIR_FILTER_DEFAULT = 0,  // use same filter type that easy-to-use API chooses
	STBIR_FILTER_BOX = 1,  // A trapezoid w/1-pixel wide ramps, same result as box for integer scale ratios
	STBIR_FILTER_TRIANGLE = 2,  // On upsampling, produces same results as bilinear texture filtering
	STBIR_FILTER_CUBICBSPLINE = 3,  // The cubic b-spline (aka Mitchell-Netrevalli with B=1,C=0), gaussian-esque
	STBIR_FILTER_CATMULLROM = 4,  // An interpolating cubic spline
	STBIR_FILTER_MITCHELL = 5,  // Mitchell-Netrevalli filter with B=1/3, C=1/3
} stbir_filter;

typedef enum
{
	STBIR_COLORSPACE_LINEAR,
	STBIR_COLORSPACE_SRGB,

	STBIR_MAX_COLORSPACES,
} stbir_colorspace;

// The following functions are all identical except for the type of the image data

STBIRDEF int stbir_resize_uint8_generic(const unsigned char *input_pixels, int input_w, int input_h, int input_stride_in_bytes,
	unsigned char *output_pixels, int output_w, int output_h, int output_stride_in_bytes,
	int num_channels, int alpha_channel, int flags,
	stbir_edge edge_wrap_mode, stbir_filter filter, stbir_colorspace space,
	void *alloc_context);

STBIRDEF int stbir_resize_uint16_generic(const stbir_uint16 *input_pixels, int input_w, int input_h, int input_stride_in_bytes,
	stbir_uint16 *output_pixels, int output_w, int output_h, int output_stride_in_bytes,
	int num_channels, int alpha_channel, int flags,
	stbir_edge edge_wrap_mode, stbir_filter filter, stbir_colorspace space,
	void *alloc_context);

STBIRDEF int stbir_resize_float_generic(const float *input_pixels, int input_w, int input_h, int input_stride_in_bytes,
	float *output_pixels, int output_w, int output_h, int output_stride_in_bytes,
	int num_channels, int alpha_channel, int flags,
	stbir_edge edge_wrap_mode, stbir_filter filter, stbir_colorspace space,
	void *alloc_context);



//////////////////////////////////////////////////////////////////////////////
//
// Full-complexity API
//
// This extends the medium API as follows:
//
//       * uint32 image type
//     * not typesafe
//     * separate filter types for each axis
//     * separate edge modes for each axis
//     * can specify scale explicitly for subpixel correctness
//     * can specify image source tile using texture coordinates

typedef enum
{
	STBIR_TYPE_UINT8,
	STBIR_TYPE_UINT16,
	STBIR_TYPE_UINT32,
	STBIR_TYPE_FLOAT,

	STBIR_MAX_TYPES
} stbir_datatype;

STBIRDEF int stbir_resize(const void *input_pixels, int input_w, int input_h, int input_stride_in_bytes,
	void *output_pixels, int output_w, int output_h, int output_stride_in_bytes,
	stbir_datatype datatype,
	int num_channels, int alpha_channel, int flags,
	stbir_edge edge_mode_horizontal, stbir_edge edge_mode_vertical,
	stbir_filter filter_horizontal, stbir_filter filter_vertical,
	stbir_colorspace space, void *alloc_context);

STBIRDEF int stbir_resize_subpixel(const void *input_pixels, int input_w, int input_h, int input_stride_in_bytes,
	void *output_pixels, int output_w, int output_h, int output_stride_in_bytes,
	stbir_datatype datatype,
	int num_channels, int alpha_channel, int flags,
	stbir_edge edge_mode_horizontal, stbir_edge edge_mode_vertical,
	stbir_filter filter_horizontal, stbir_filter filter_vertical,
	stbir_colorspace space, void *alloc_context,
	float x_scale, float y_scale,
	float x_offset, float y_offset);

STBIRDEF int stbir_resize_region(const void *input_pixels, int input_w, int input_h, int input_stride_in_bytes,
	void *output_pixels, int output_w, int output_h, int output_stride_in_bytes,
	stbir_datatype datatype,
	int num_channels, int alpha_channel, int flags,
	stbir_edge edge_mode_horizontal, stbir_edge edge_mode_vertical,
	stbir_filter filter_horizontal, stbir_filter filter_vertical,
	stbir_colorspace space, void *alloc_context,
	float s0, float t0, float s1, float t1);
// (s0, t0) & (s1, t1) are the top-left and bottom right corner (uv addressing style: [0, 1]x[0, 1]) of a region of the input image to use.

//
//
////   end header file   /////////////////////////////////////////////////////
#endif // STBIR_INCLUDE_STB_IMAGE_RESIZE_H





#ifdef STB_IMAGE_RESIZE_IMPLEMENTATION

#ifndef STBIR_ASSERT
#include <assert.h>
#define STBIR_ASSERT(x) assert(x)
#endif

// For memset
#include <string.h>

#include <math.h>

#ifndef STBIR_MALLOC
#include <stdlib.h>
// use comma operator to evaluate c, to avoid "unused parameter" warnings
#define STBIR_MALLOC(size,c) ((void)(c), malloc(size))
#define STBIR_FREE(ptr,c)    ((void)(c), free(ptr))
#endif

#ifndef _MSC_VER
#ifdef __cplusplus
#define stbir__inline inline
#else
#define stbir__inline
#endif
#else
#define stbir__inline __forceinline
#endif


// should produce compiler error if size is wrong
typedef unsigned char stbir__validate_uint32[sizeof(stbir_uint32) == 4 ? 1 : -1];

#ifdef _MSC_VER
#define STBIR__NOTUSED(v)  (void)(v)
#else
#define STBIR__NOTUSED(v)  (void)sizeof(v)
#endif

#define STBIR__ARRAY_SIZE(a) (sizeof((a))/sizeof((a)[0]))

#ifndef STBIR_DEFAULT_FILTER_UPSAMPLE
#define STBIR_DEFAULT_FILTER_UPSAMPLE    STBIR_FILTER_CATMULLROM
#endif

#ifndef STBIR_DEFAULT_FILTER_DOWNSAMPLE
#define STBIR_DEFAULT_FILTER_DOWNSAMPLE  STBIR_FILTER_MITCHELL
#endif

#ifndef STBIR_PROGRESS_REPORT
#define STBIR_PROGRESS_REPORT(float_0_to_1)
#endif

#ifndef STBIR_MAX_CHANNELS
#define STBIR_MAX_CHANNELS 64
#endif

#if STBIR_MAX_CHANNELS > 65536
#error "Too many channels; STBIR_MAX_CHANNELS must be no more than 65536."
// because we store the indices in 16-bit variables
#endif

// This value is added to alpha just before premultiplication to avoid
// zeroing out color values. It is equivalent to 2^-80. If you don't want
// that behavior (it may interfere if you have floating point images with
// very small alpha values) then you can define STBIR_NO_ALPHA_EPSILON to
// disable it.
#ifndef STBIR_ALPHA_EPSILON
#define STBIR_ALPHA_EPSILON ((float)1 / (1 << 20) / (1 << 20) / (1 << 20) / (1 << 20))
#endif



#ifdef _MSC_VER
#define STBIR__UNUSED_PARAM(v)  (void)(v)
#else
#define STBIR__UNUSED_PARAM(v)  (void)sizeof(v)
#endif

// must match stbir_datatype
static unsigned char stbir__type_size[] = {
	1, // STBIR_TYPE_UINT8
	2, // STBIR_TYPE_UINT16
	4, // STBIR_TYPE_UINT32
	4, // STBIR_TYPE_FLOAT
};

// Kernel function centered at 0
typedef float (stbir__kernel_fn)(float x, float scale);
typedef float (stbir__support_fn)(float scale);

typedef struct
{
	stbir__kernel_fn* kernel;
	stbir__support_fn* support;
} stbir__filter_info;

// When upsampling, the contributors are which source pixels contribute.
// When downsampling, the contributors are which destination pixels are contributed to.
typedef struct
{
	int n0; // First contributing pixel
	int n1; // Last contributing pixel
} stbir__contributors;

typedef struct
{
	const void* input_data;
	int input_w;
	int input_h;
	int input_stride_bytes;

	void* output_data;
	int output_w;
	int output_h;
	int output_stride_bytes;

	float s0, t0, s1, t1;

	float horizontal_shift; // Units: output pixels
	float vertical_shift;   // Units: output pixels
	float horizontal_scale;
	float vertical_scale;

	int channels;
	int alpha_channel;
	stbir_uint32 flags;
	stbir_datatype type;
	stbir_filter horizontal_filter;
	stbir_filter vertical_filter;
	stbir_edge edge_horizontal;
	stbir_edge edge_vertical;
	stbir_colorspace colorspace;

	stbir__contributors* horizontal_contributors;
	float* horizontal_coefficients;

	stbir__contributors* vertical_contributors;
	float* vertical_coefficients;

	int decode_buffer_pixels;
	float* decode_buffer;

	float* horizontal_buffer;

	// cache these because ceil/floor are inexplicably showing up in profile
	int horizontal_coefficient_width;
	int vertical_coefficient_width;
	int horizontal_filter_pixel_width;
	int vertical_filter_pixel_width;
	int horizontal_filter_pixel_margin;
	int vertical_filter_pixel_margin;
	int horizontal_num_contributors;
	int vertical_num_contributors;

	int ring_buffer_length_bytes;   // The length of an individual entry in the ring buffer. The total number of ring buffers is stbir__get_filter_pixel_width(filter)
	int ring_buffer_num_entries;    // Total number of entries in the ring buffer.
	int ring_buffer_first_scanline;
	int ring_buffer_last_scanline;
	int ring_buffer_begin_index;    // first_scanline is at this index in the ring buffer
	float* ring_buffer;

	float* encode_buffer; // A temporary buffer to store floats so we don't lose precision while we do multiply-adds.

	int horizontal_contributors_size;
	int horizontal_coefficients_size;
	int vertical_contributors_size;
	int vertical_coefficients_size;
	int decode_buffer_size;
	int horizontal_buffer_size;
	int ring_buffer_size;
	int encode_buffer_size;
} stbir__info;


static const float stbir__max_uint8_as_float = 255.0f;
static const float stbir__max_uint16_as_float = 65535.0f;
static const double stbir__max_uint32_as_float = 4294967295.0;


static stbir__inline int stbir__min(int a, int b)
{
	return a < b ? a : b;
}

static stbir__inline float stbir__saturate(float x)
{
	if (x < 0)
		return 0;

	if (x > 1)
		return 1;

	return x;
}

#ifdef STBIR_SATURATE_INT
static stbir__inline stbir_uint8 stbir__saturate8(int x)
{
	if ((unsigned int)x <= 255)
		return x;

	if (x < 0)
		return 0;

	return 255;
}

static stbir__inline stbir_uint16 stbir__saturate16(int x)
{
	if ((unsigned int)x <= 65535)
		return x;

	if (x < 0)
		return 0;

	return 65535;
}
#endif

static float stbir__srgb_uchar_to_linear_float[256] = {
	0.000000f, 0.000304f, 0.000607f, 0.000911f, 0.001214f, 0.001518f, 0.001821f, 0.002125f, 0.002428f, 0.002732f, 0.003035f,
	0.003347f, 0.003677f, 0.004025f, 0.004391f, 0.004777f, 0.005182f, 0.005605f, 0.006049f, 0.006512f, 0.006995f, 0.007499f,
	0.008023f, 0.008568f, 0.009134f, 0.009721f, 0.010330f, 0.010960f, 0.011612f, 0.012286f, 0.012983f, 0.013702f, 0.014444f,
	0.015209f, 0.015996f, 0.016807f, 0.017642f, 0.018500f, 0.019382f, 0.020289f, 0.021219f, 0.022174f, 0.023153f, 0.024158f,
	0.025187f, 0.026241f, 0.027321f, 0.028426f, 0.029557f, 0.030713f, 0.031896f, 0.033105f, 0.034340f, 0.035601f, 0.036889f,
	0.038204f, 0.039546f, 0.040915f, 0.042311f, 0.043735f, 0.045186f, 0.046665f, 0.048172f, 0.049707f, 0.051269f, 0.052861f,
	0.054480f, 0.056128f, 0.057805f, 0.059511f, 0.061246f, 0.063010f, 0.064803f, 0.066626f, 0.068478f, 0.070360f, 0.072272f,
	0.074214f, 0.076185f, 0.078187f, 0.080220f, 0.082283f, 0.084376f, 0.086500f, 0.088656f, 0.090842f, 0.093059f, 0.095307f,
	0.097587f, 0.099899f, 0.102242f, 0.104616f, 0.107023f, 0.109462f, 0.111932f, 0.114435f, 0.116971f, 0.119538f, 0.122139f,
	0.124772f, 0.127438f, 0.130136f, 0.132868f, 0.135633f, 0.138432f, 0.141263f, 0.144128f, 0.147027f, 0.149960f, 0.152926f,
	0.155926f, 0.158961f, 0.162029f, 0.165132f, 0.168269f, 0.171441f, 0.174647f, 0.177888f, 0.181164f, 0.184475f, 0.187821f,
	0.191202f, 0.194618f, 0.198069f, 0.201556f, 0.205079f, 0.208637f, 0.212231f, 0.215861f, 0.219526f, 0.223228f, 0.226966f,
	0.230740f, 0.234551f, 0.238398f, 0.242281f, 0.246201f, 0.250158f, 0.254152f, 0.258183f, 0.262251f, 0.266356f, 0.270498f,
	0.274677f, 0.278894f, 0.283149f, 0.287441f, 0.291771f, 0.296138f, 0.300544f, 0.304987f, 0.309469f, 0.313989f, 0.318547f,
	0.323143f, 0.327778f, 0.332452f, 0.337164f, 0.341914f, 0.346704f, 0.351533f, 0.356400f, 0.361307f, 0.366253f, 0.371238f,
	0.376262f, 0.381326f, 0.386430f, 0.391573f, 0.396755f, 0.401978f, 0.407240f, 0.412543f, 0.417885f, 0.423268f, 0.428691f,
	0.434154f, 0.439657f, 0.445201f, 0.450786f, 0.456411f, 0.462077f, 0.467784f, 0.473532f, 0.479320f, 0.485150f, 0.491021f,
	0.496933f, 0.502887f, 0.508881f, 0.514918f, 0.520996f, 0.527115f, 0.533276f, 0.539480f, 0.545725f, 0.552011f, 0.558340f,
	0.564712f, 0.571125f, 0.577581f, 0.584078f, 0.590619f, 0.597202f, 0.603827f, 0.610496f, 0.617207f, 0.623960f, 0.630757f,
	0.637597f, 0.644480f, 0.651406f, 0.658375f, 0.665387f, 0.672443f, 0.679543f, 0.686685f, 0.693872f, 0.701102f, 0.708376f,
	0.715694f, 0.723055f, 0.730461f, 0.737911f, 0.745404f, 0.752942f, 0.760525f, 0.768151f, 0.775822f, 0.783538f, 0.791298f,
	0.799103f, 0.806952f, 0.814847f, 0.822786f, 0.830770f, 0.838799f, 0.846873f, 0.854993f, 0.863157f, 0.871367f, 0.879622f,
	0.887923f, 0.896269f, 0.904661f, 0.913099f, 0.921582f, 0.930111f, 0.938686f, 0.947307f, 0.955974f, 0.964686f, 0.973445f,
	0.982251f, 0.991102f, 1.0f
};

static float stbir__srgb_to_linear(float f)
{
	if (f <= 0.04045f)
		return f / 12.92f;
	else
		return (float)pow((f + 0.055f) / 1.055f, 2.4f);
}

static float stbir__linear_to_srgb(float f)
{
	if (f <= 0.0031308f)
		return f * 12.92f;
	else
		return 1.055f * (float)pow(f, 1 / 2.4f) - 0.055f;
}

#ifndef STBIR_NON_IEEE_FLOAT
// From https://gist.github.com/rygorous/2203834

typedef union
{
	stbir_uint32 u;
	float f;
} stbir__FP32;

static const stbir_uint32 fp32_to_srgb8_tab4[104] = {
	0x0073000d, 0x007a000d, 0x0080000d, 0x0087000d, 0x008d000d, 0x0094000d, 0x009a000d, 0x00a1000d,
	0x00a7001a, 0x00b4001a, 0x00c1001a, 0x00ce001a, 0x00da001a, 0x00e7001a, 0x00f4001a, 0x0101001a,
	0x010e0033, 0x01280033, 0x01410033, 0x015b0033, 0x01750033, 0x018f0033, 0x01a80033, 0x01c20033,
	0x01dc0067, 0x020f0067, 0x02430067, 0x02760067, 0x02aa0067, 0x02dd0067, 0x03110067, 0x03440067,
	0x037800ce, 0x03df00ce, 0x044600ce, 0x04ad00ce, 0x051400ce, 0x057b00c5, 0x05dd00bc, 0x063b00b5,
	0x06970158, 0x07420142, 0x07e30130, 0x087b0120, 0x090b0112, 0x09940106, 0x0a1700fc, 0x0a9500f2,
	0x0b0f01cb, 0x0bf401ae, 0x0ccb0195, 0x0d950180, 0x0e56016e, 0x0f0d015e, 0x0fbc0150, 0x10630143,
	0x11070264, 0x1238023e, 0x1357021d, 0x14660201, 0x156601e9, 0x165a01d3, 0x174401c0, 0x182401af,
	0x18fe0331, 0x1a9602fe, 0x1c1502d2, 0x1d7e02ad, 0x1ed4028d, 0x201a0270, 0x21520256, 0x227d0240,
	0x239f0443, 0x25c003fe, 0x27bf03c4, 0x29a10392, 0x2b6a0367, 0x2d1d0341, 0x2ebe031f, 0x304d0300,
	0x31d105b0, 0x34a80555, 0x37520507, 0x39d504c5, 0x3c37048b, 0x3e7c0458, 0x40a8042a, 0x42bd0401,
	0x44c20798, 0x488e071e, 0x4c1c06b6, 0x4f76065d, 0x52a50610, 0x55ac05cc, 0x5892058f, 0x5b590559,
	0x5e0c0a23, 0x631c0980, 0x67db08f6, 0x6c55087f, 0x70940818, 0x74a007bd, 0x787d076c, 0x7c330723,
};

static stbir_uint8 stbir__linear_to_srgb_uchar(float in)
{
	static const stbir__FP32 almostone = { 0x3f7fffff }; // 1-eps
	static const stbir__FP32 minval = { (127 - 13) << 23 };
	stbir_uint32 tab, bias, scale, t;
	stbir__FP32 f;

	// Clamp to [2^(-13), 1-eps]; these two values map to 0 and 1, respectively.
	// The tests are carefully written so that NaNs map to 0, same as in the reference
	// implementation.
	if (!(in > minval.f)) // written this way to catch NaNs
		in = minval.f;
	if (in > almostone.f)
		in = almostone.f;

	// Do the table lookup and unpack bias, scale
	f.f = in;
	tab = fp32_to_srgb8_tab4[(f.u - minval.u) >> 20];
	bias = (tab >> 16) << 9;
	scale = tab & 0xffff;

	// Grab next-highest mantissa bits and perform linear interpolation
	t = (f.u >> 12) & 0xff;
	return (unsigned char)((bias + scale * t) >> 16);
}

#else
// sRGB transition values, scaled by 1<<28
static int stbir__srgb_offset_to_linear_scaled[256] =
{
			0,     40738,    122216,    203693,    285170,    366648,    448125,    529603,
	   611080,    692557,    774035,    855852,    942009,   1033024,   1128971,   1229926,
	  1335959,   1447142,   1563542,   1685229,   1812268,   1944725,   2082664,   2226148,
	  2375238,   2529996,   2690481,   2856753,   3028870,   3206888,   3390865,   3580856,
	  3776916,   3979100,   4187460,   4402049,   4622919,   4850123,   5083710,   5323731,
	  5570236,   5823273,   6082892,   6349140,   6622065,   6901714,   7188133,   7481369,
	  7781466,   8088471,   8402427,   8723380,   9051372,   9386448,   9728650,  10078021,
	 10434603,  10798439,  11169569,  11548036,  11933879,  12327139,  12727857,  13136073,
	 13551826,  13975156,  14406100,  14844697,  15290987,  15745007,  16206795,  16676389,
	 17153826,  17639142,  18132374,  18633560,  19142734,  19659934,  20185196,  20718552,
	 21260042,  21809696,  22367554,  22933648,  23508010,  24090680,  24681686,  25281066,
	 25888850,  26505076,  27129772,  27762974,  28404716,  29055026,  29713942,  30381490,
	 31057708,  31742624,  32436272,  33138682,  33849884,  34569912,  35298800,  36036568,
	 36783260,  37538896,  38303512,  39077136,  39859796,  40651528,  41452360,  42262316,
	 43081432,  43909732,  44747252,  45594016,  46450052,  47315392,  48190064,  49074096,
	 49967516,  50870356,  51782636,  52704392,  53635648,  54576432,  55526772,  56486700,
	 57456236,  58435408,  59424248,  60422780,  61431036,  62449032,  63476804,  64514376,
	 65561776,  66619028,  67686160,  68763192,  69850160,  70947088,  72053992,  73170912,
	 74297864,  75434880,  76581976,  77739184,  78906536,  80084040,  81271736,  82469648,
	 83677792,  84896192,  86124888,  87363888,  88613232,  89872928,  91143016,  92423512,
	 93714432,  95015816,  96327688,  97650056,  98982952, 100326408, 101680440, 103045072,
	104420320, 105806224, 107202800, 108610064, 110028048, 111456776, 112896264, 114346544,
	115807632, 117279552, 118762328, 120255976, 121760536, 123276016, 124802440, 126339832,
	127888216, 129447616, 131018048, 132599544, 134192112, 135795792, 137410592, 139036528,
	140673648, 142321952, 143981456, 145652208, 147334208, 149027488, 150732064, 152447968,
	154175200, 155913792, 157663776, 159425168, 161197984, 162982240, 164777968, 166585184,
	168403904, 170234160, 172075968, 173929344, 175794320, 177670896, 179559120, 181458992,
	183370528, 185293776, 187228736, 189175424, 191133888, 193104112, 195086128, 197079968,
	199085648, 201103184, 203132592, 205173888, 207227120, 209292272, 211369392, 213458480,
	215559568, 217672656, 219797792, 221934976, 224084240, 226245600, 228419056, 230604656,
	232802400, 235012320, 237234432, 239468736, 241715280, 243974080, 246245120, 248528464,
	250824112, 253132064, 255452368, 257785040, 260130080, 262487520, 264857376, 267239664,
};

static stbir_uint8 stbir__linear_to_srgb_uchar(float f)
{
	int x = (int)(f * (1 << 28)); // has headroom so you don't need to clamp
	int v = 0;
	int i;

	// Refine the guess with a short binary search.
	i = v + 128; if (x >= stbir__srgb_offset_to_linear_scaled[i]) v = i;
	i = v + 64; if (x >= stbir__srgb_offset_to_linear_scaled[i]) v = i;
	i = v + 32; if (x >= stbir__srgb_offset_to_linear_scaled[i]) v = i;
	i = v + 16; if (x >= stbir__srgb_offset_to_linear_scaled[i]) v = i;
	i = v + 8; if (x >= stbir__srgb_offset_to_linear_scaled[i]) v = i;
	i = v + 4; if (x >= stbir__srgb_offset_to_linear_scaled[i]) v = i;
	i = v + 2; if (x >= stbir__srgb_offset_to_linear_scaled[i]) v = i;
	i = v + 1; if (x >= stbir__srgb_offset_to_linear_scaled[i]) v = i;

	return (stbir_uint8)v;
}
#endif

static float stbir__filter_trapezoid(float x, float scale)
{
	float halfscale = scale / 2;
	float t = 0.5f + halfscale;
	STBIR_ASSERT(scale <= 1);

	x = (float)fabs(x);

	if (x >= t)
		return 0;
	else
	{
		float r = 0.5f - halfscale;
		if (x <= r)
			return 1;
		else
			return (t - x) / scale;
	}
}

static float stbir__support_trapezoid(float scale)
{
	STBIR_ASSERT(scale <= 1);
	return 0.5f + scale / 2;
}

static float stbir__filter_triangle(float x, float s)
{
	STBIR__UNUSED_PARAM(s);

	x = (float)fabs(x);

	if (x <= 1.0f)
		return 1 - x;
	else
		return 0;
}

static float stbir__filter_cubic(float x, float s)
{
	STBIR__UNUSED_PARAM(s);

	x = (float)fabs(x);

	if (x < 1.0f)
		return (4 + x * x*(3 * x - 6)) / 6;
	else if (x < 2.0f)
		return (8 + x * (-12 + x * (6 - x))) / 6;

	return (0.0f);
}

static float stbir__filter_catmullrom(float x, float s)
{
	STBIR__UNUSED_PARAM(s);

	x = (float)fabs(x);

	if (x < 1.0f)
		return 1 - x * x*(2.5f - 1.5f*x);
	else if (x < 2.0f)
		return 2 - x * (4 + x * (0.5f*x - 2.5f));

	return (0.0f);
}

static float stbir__filter_mitchell(float x, float s)
{
	STBIR__UNUSED_PARAM(s);

	x = (float)fabs(x);

	if (x < 1.0f)
		return (16 + x * x*(21 * x - 36)) / 18;
	else if (x < 2.0f)
		return (32 + x * (-60 + x * (36 - 7 * x))) / 18;

	return (0.0f);
}

static float stbir__support_zero(float s)
{
	STBIR__UNUSED_PARAM(s);
	return 0;
}

static float stbir__support_one(float s)
{
	STBIR__UNUSED_PARAM(s);
	return 1;
}

static float stbir__support_two(float s)
{
	STBIR__UNUSED_PARAM(s);
	return 2;
}

static stbir__filter_info stbir__filter_info_table[] = {
		{ NULL,                     stbir__support_zero },
		{ stbir__filter_trapezoid,  stbir__support_trapezoid },
		{ stbir__filter_triangle,   stbir__support_one },
		{ stbir__filter_cubic,      stbir__support_two },
		{ stbir__filter_catmullrom, stbir__support_two },
		{ stbir__filter_mitchell,   stbir__support_two },
};

stbir__inline static int stbir__use_upsampling(float ratio)
{
	return ratio > 1;
}

stbir__inline static int stbir__use_width_upsampling(stbir__info* stbir_info)
{
	return stbir__use_upsampling(stbir_info->horizontal_scale);
}

stbir__inline static int stbir__use_height_upsampling(stbir__info* stbir_info)
{
	return stbir__use_upsampling(stbir_info->vertical_scale);
}

// This is the maximum number of input samples that can affect an output sample
// with the given filter
static int stbir__get_filter_pixel_width(stbir_filter filter, float scale)
{
	STBIR_ASSERT(filter != 0);
	STBIR_ASSERT(filter < STBIR__ARRAY_SIZE(stbir__filter_info_table));

	if (stbir__use_upsampling(scale))
		return (int)ceil(stbir__filter_info_table[filter].support(1 / scale) * 2);
	else
		return (int)ceil(stbir__filter_info_table[filter].support(scale) * 2 / scale);
}

// This is how much to expand buffers to account for filters seeking outside
// the image boundaries.
static int stbir__get_filter_pixel_margin(stbir_filter filter, float scale)
{
	return stbir__get_filter_pixel_width(filter, scale) / 2;
}

static int stbir__get_coefficient_width(stbir_filter filter, float scale)
{
	if (stbir__use_upsampling(scale))
		return (int)ceil(stbir__filter_info_table[filter].support(1 / scale) * 2);
	else
		return (int)ceil(stbir__filter_info_table[filter].support(scale) * 2);
}

static int stbir__get_contributors(float scale, stbir_filter filter, int input_size, int output_size)
{
	if (stbir__use_upsampling(scale))
		return output_size;
	else
		return (input_size + stbir__get_filter_pixel_margin(filter, scale) * 2);
}

static int stbir__get_total_horizontal_coefficients(stbir__info* info)
{
	return info->horizontal_num_contributors
		* stbir__get_coefficient_width(info->horizontal_filter, info->horizontal_scale);
}

static int stbir__get_total_vertical_coefficients(stbir__info* info)
{
	return info->vertical_num_contributors
		* stbir__get_coefficient_width(info->vertical_filter, info->vertical_scale);
}

static stbir__contributors* stbir__get_contributor(stbir__contributors* contributors, int n)
{
	return &contributors[n];
}

// For perf reasons this code is duplicated in stbir__resample_horizontal_upsample/downsample,
// if you change it here change it there too.
static float* stbir__get_coefficient(float* coefficients, stbir_filter filter, float scale, int n, int c)
{
	int width = stbir__get_coefficient_width(filter, scale);
	return &coefficients[width*n + c];
}

static int stbir__edge_wrap_slow(stbir_edge edge, int n, int max)
{
	switch (edge)
	{
	case STBIR_EDGE_ZERO:
		return 0; // we'll decode the wrong pixel here, and then overwrite with 0s later

	case STBIR_EDGE_CLAMP:
		if (n < 0)
			return 0;

		if (n >= max)
			return max - 1;

		return n; // NOTREACHED

	case STBIR_EDGE_REFLECT:
	{
		if (n < 0)
		{
			if (n < max)
				return -n;
			else
				return max - 1;
		}

		if (n >= max)
		{
			int max2 = max * 2;
			if (n >= max2)
				return 0;
			else
				return max2 - n - 1;
		}

		return n; // NOTREACHED
	}

	case STBIR_EDGE_WRAP:
		if (n >= 0)
			return (n % max);
		else
		{
			int m = (-n) % max;

			if (m != 0)
				m = max - m;

			return (m);
		}
		// NOTREACHED

	default:
		STBIR_ASSERT(!"Unimplemented edge type");
		return 0;
	}
}

stbir__inline static int stbir__edge_wrap(stbir_edge edge, int n, int max)
{
	// avoid per-pixel switch
	if (n >= 0 && n < max)
		return n;
	return stbir__edge_wrap_slow(edge, n, max);
}

// What input pixels contribute to this output pixel?
static void stbir__calculate_sample_range_upsample(int n, float out_filter_radius, float scale_ratio, float out_shift, int* in_first_pixel, int* in_last_pixel, float* in_center_of_out)
{
	float out_pixel_center = (float)n + 0.5f;
	float out_pixel_influence_lowerbound = out_pixel_center - out_filter_radius;
	float out_pixel_influence_upperbound = out_pixel_center + out_filter_radius;

	float in_pixel_influence_lowerbound = (out_pixel_influence_lowerbound + out_shift) / scale_ratio;
	float in_pixel_influence_upperbound = (out_pixel_influence_upperbound + out_shift) / scale_ratio;

	*in_center_of_out = (out_pixel_center + out_shift) / scale_ratio;
	*in_first_pixel = (int)(floor(in_pixel_influence_lowerbound + 0.5));
	*in_last_pixel = (int)(floor(in_pixel_influence_upperbound - 0.5));
}

// What output pixels does this input pixel contribute to?
static void stbir__calculate_sample_range_downsample(int n, float in_pixels_radius, float scale_ratio, float out_shift, int* out_first_pixel, int* out_last_pixel, float* out_center_of_in)
{
	float in_pixel_center = (float)n + 0.5f;
	float in_pixel_influence_lowerbound = in_pixel_center - in_pixels_radius;
	float in_pixel_influence_upperbound = in_pixel_center + in_pixels_radius;

	float out_pixel_influence_lowerbound = in_pixel_influence_lowerbound * scale_ratio - out_shift;
	float out_pixel_influence_upperbound = in_pixel_influence_upperbound * scale_ratio - out_shift;

	*out_center_of_in = in_pixel_center * scale_ratio - out_shift;
	*out_first_pixel = (int)(floor(out_pixel_influence_lowerbound + 0.5));
	*out_last_pixel = (int)(floor(out_pixel_influence_upperbound - 0.5));
}

static void stbir__calculate_coefficients_upsample(stbir_filter filter, float scale, int in_first_pixel, int in_last_pixel, float in_center_of_out, stbir__contributors* contributor, float* coefficient_group)
{
	int i;
	float total_filter = 0;
	float filter_scale;

	STBIR_ASSERT(in_last_pixel - in_first_pixel <= (int)ceil(stbir__filter_info_table[filter].support(1 / scale) * 2)); // Taken directly from stbir__get_coefficient_width() which we can't call because we don't know if we're horizontal or vertical.

	contributor->n0 = in_first_pixel;
	contributor->n1 = in_last_pixel;

	STBIR_ASSERT(contributor->n1 >= contributor->n0);

	for (i = 0; i <= in_last_pixel - in_first_pixel; i++)
	{
		float in_pixel_center = (float)(i + in_first_pixel) + 0.5f;
		coefficient_group[i] = stbir__filter_info_table[filter].kernel(in_center_of_out - in_pixel_center, 1 / scale);

		// If the coefficient is zero, flags it. (Don't do the <0 check here, we want the influence of those outside pixels.)
		if (i == 0 && !coefficient_group[i])
		{
			contributor->n0 = ++in_first_pixel;
			i--;
			continue;
		}

		total_filter += coefficient_group[i];
	}

	STBIR_ASSERT(stbir__filter_info_table[filter].kernel((float)(in_last_pixel + 1) + 0.5f - in_center_of_out, 1 / scale) == 0);

	STBIR_ASSERT(total_filter > 0.9);
	STBIR_ASSERT(total_filter < 1.1f); // Make sure it's not way off.

	// Make sure the sum of all coefficients is 1.
	filter_scale = 1 / total_filter;

	for (i = 0; i <= in_last_pixel - in_first_pixel; i++)
		coefficient_group[i] *= filter_scale;

	for (i = in_last_pixel - in_first_pixel; i >= 0; i--)
	{
		if (coefficient_group[i])
			break;

		// This line has no weight. We can flags it.
		contributor->n1 = contributor->n0 + i - 1;
	}
}

static void stbir__calculate_coefficients_downsample(stbir_filter filter, float scale_ratio, int out_first_pixel, int out_last_pixel, float out_center_of_in, stbir__contributors* contributor, float* coefficient_group)
{
	int i;

	STBIR_ASSERT(out_last_pixel - out_first_pixel <= (int)ceil(stbir__filter_info_table[filter].support(scale_ratio) * 2)); // Taken directly from stbir__get_coefficient_width() which we can't call because we don't know if we're horizontal or vertical.

	contributor->n0 = out_first_pixel;
	contributor->n1 = out_last_pixel;

	STBIR_ASSERT(contributor->n1 >= contributor->n0);

	for (i = 0; i <= out_last_pixel - out_first_pixel; i++)
	{
		float out_pixel_center = (float)(i + out_first_pixel) + 0.5f;
		float x = out_pixel_center - out_center_of_in;
		coefficient_group[i] = stbir__filter_info_table[filter].kernel(x, scale_ratio) * scale_ratio;
	}

	STBIR_ASSERT(stbir__filter_info_table[filter].kernel((float)(out_last_pixel + 1) + 0.5f - out_center_of_in, scale_ratio) == 0);

	for (i = out_last_pixel - out_first_pixel; i >= 0; i--)
	{
		if (coefficient_group[i])
			break;

		// This line has no weight. We can flags it.
		contributor->n1 = contributor->n0 + i - 1;
	}
}

static void stbir__normalize_downsample_coefficients(stbir__contributors* contributors, float* coefficients, stbir_filter filter, float scale_ratio, int input_size, int output_size)
{
	int num_contributors = stbir__get_contributors(scale_ratio, filter, input_size, output_size);
	int num_coefficients = stbir__get_coefficient_width(filter, scale_ratio);
	int i, j;
	int flags;

	for (i = 0; i < output_size; i++)
	{
		float scale;
		float total = 0;

		for (j = 0; j < num_contributors; j++)
		{
			if (i >= contributors[j].n0 && i <= contributors[j].n1)
			{
				float coefficient = *stbir__get_coefficient(coefficients, filter, scale_ratio, j, i - contributors[j].n0);
				total += coefficient;
			}
			else if (i < contributors[j].n0)
				break;
		}

		STBIR_ASSERT(total > 0.9f);
		STBIR_ASSERT(total < 1.1f);

		scale = 1 / total;

		for (j = 0; j < num_contributors; j++)
		{
			if (i >= contributors[j].n0 && i <= contributors[j].n1)
				*stbir__get_coefficient(coefficients, filter, scale_ratio, j, i - contributors[j].n0) *= scale;
			else if (i < contributors[j].n0)
				break;
		}
	}

	// Optimize: Skip zero coefficients and contributions outside of image bounds.
	// Do this after normalizing because normalization depends on the n0/n1 values.
	for (j = 0; j < num_contributors; j++)
	{
		int range, max, width;

		flags = 0;
		while (*stbir__get_coefficient(coefficients, filter, scale_ratio, j, flags) == 0)
			flags++;

		contributors[j].n0 += flags;

		while (contributors[j].n0 < 0)
		{
			contributors[j].n0++;
			flags++;
		}

		range = contributors[j].n1 - contributors[j].n0 + 1;
		max = stbir__min(num_coefficients, range);

		width = stbir__get_coefficient_width(filter, scale_ratio);
		for (i = 0; i < max; i++)
		{
			if (i + flags >= width)
				break;

			*stbir__get_coefficient(coefficients, filter, scale_ratio, j, i) = *stbir__get_coefficient(coefficients, filter, scale_ratio, j, i + flags);
		}

		continue;
	}

	// Using min to avoid writing into invalid pixels.
	for (i = 0; i < num_contributors; i++)
		contributors[i].n1 = stbir__min(contributors[i].n1, output_size - 1);
}

// Each scan line uses the same kernel values so we should calculate the kernel
// values once and then we can use them for every scan line.
static void stbir__calculate_filters(stbir__contributors* contributors, float* coefficients, stbir_filter filter, float scale_ratio, float shift, int input_size, int output_size)
{
	int n;
	int total_contributors = stbir__get_contributors(scale_ratio, filter, input_size, output_size);

	if (stbir__use_upsampling(scale_ratio))
	{
		float out_pixels_radius = stbir__filter_info_table[filter].support(1 / scale_ratio) * scale_ratio;

		// Looping through out pixels
		for (n = 0; n < total_contributors; n++)
		{
			float in_center_of_out; // Center of the current out pixel in the in pixel space
			int in_first_pixel, in_last_pixel;

			stbir__calculate_sample_range_upsample(n, out_pixels_radius, scale_ratio, shift, &in_first_pixel, &in_last_pixel, &in_center_of_out);

			stbir__calculate_coefficients_upsample(filter, scale_ratio, in_first_pixel, in_last_pixel, in_center_of_out, stbir__get_contributor(contributors, n), stbir__get_coefficient(coefficients, filter, scale_ratio, n, 0));
		}
	}
	else
	{
		float in_pixels_radius = stbir__filter_info_table[filter].support(scale_ratio) / scale_ratio;

		// Looping through in pixels
		for (n = 0; n < total_contributors; n++)
		{
			float out_center_of_in; // Center of the current out pixel in the in pixel space
			int out_first_pixel, out_last_pixel;
			int n_adjusted = n - stbir__get_filter_pixel_margin(filter, scale_ratio);

			stbir__calculate_sample_range_downsample(n_adjusted, in_pixels_radius, scale_ratio, shift, &out_first_pixel, &out_last_pixel, &out_center_of_in);

			stbir__calculate_coefficients_downsample(filter, scale_ratio, out_first_pixel, out_last_pixel, out_center_of_in, stbir__get_contributor(contributors, n), stbir__get_coefficient(coefficients, filter, scale_ratio, n, 0));
		}

		stbir__normalize_downsample_coefficients(contributors, coefficients, filter, scale_ratio, input_size, output_size);
	}
}

static float* stbir__get_decode_buffer(stbir__info* stbir_info)
{
	// The 0 index of the decode buffer starts after the margin. This makes
	// it okay to use negative indexes on the decode buffer.
	return &stbir_info->decode_buffer[stbir_info->horizontal_filter_pixel_margin * stbir_info->channels];
}

#define STBIR__DECODE(type, colorspace) ((int)(type) * (STBIR_MAX_COLORSPACES) + (int)(colorspace))

static void stbir__decode_scanline(stbir__info* stbir_info, int n)
{
	int c;
	int channels = stbir_info->channels;
	int alpha_channel = stbir_info->alpha_channel;
	int type = stbir_info->type;
	int colorspace = stbir_info->colorspace;
	int input_w = stbir_info->input_w;
	size_t input_stride_bytes = stbir_info->input_stride_bytes;
	float* decode_buffer = stbir__get_decode_buffer(stbir_info);
	stbir_edge edge_horizontal = stbir_info->edge_horizontal;
	stbir_edge edge_vertical = stbir_info->edge_vertical;
	size_t in_buffer_row_offset = stbir__edge_wrap(edge_vertical, n, stbir_info->input_h) * input_stride_bytes;
	const void* input_data = (char *)stbir_info->input_data + in_buffer_row_offset;
	int max_x = input_w + stbir_info->horizontal_filter_pixel_margin;
	int decode = STBIR__DECODE(type, colorspace);

	int x = -stbir_info->horizontal_filter_pixel_margin;

	// special handling for STBIR_EDGE_ZERO because it needs to return an item that doesn't appear in the input,
	// and we want to avoid paying overhead on every pixel if not STBIR_EDGE_ZERO
	if (edge_vertical == STBIR_EDGE_ZERO && (n < 0 || n >= stbir_info->input_h))
	{
		for (; x < max_x; x++)
			for (c = 0; c < channels; c++)
				decode_buffer[x*channels + c] = 0;
		return;
	}

	switch (decode)
	{
	case STBIR__DECODE(STBIR_TYPE_UINT8, STBIR_COLORSPACE_LINEAR):
		for (; x < max_x; x++)
		{
			int decode_pixel_index = x * channels;
			int input_pixel_index = stbir__edge_wrap(edge_horizontal, x, input_w) * channels;
			for (c = 0; c < channels; c++)
				decode_buffer[decode_pixel_index + c] = ((float)((const unsigned char*)input_data)[input_pixel_index + c]) / stbir__max_uint8_as_float;
		}
		break;

	case STBIR__DECODE(STBIR_TYPE_UINT8, STBIR_COLORSPACE_SRGB):
		for (; x < max_x; x++)
		{
			int decode_pixel_index = x * channels;
			int input_pixel_index = stbir__edge_wrap(edge_horizontal, x, input_w) * channels;
			for (c = 0; c < channels; c++)
				decode_buffer[decode_pixel_index + c] = stbir__srgb_uchar_to_linear_float[((const unsigned char*)input_data)[input_pixel_index + c]];

			if (!(stbir_info->flags&STBIR_FLAG_ALPHA_USES_COLORSPACE))
				decode_buffer[decode_pixel_index + alpha_channel] = ((float)((const unsigned char*)input_data)[input_pixel_index + alpha_channel]) / stbir__max_uint8_as_float;
		}
		break;

	case STBIR__DECODE(STBIR_TYPE_UINT16, STBIR_COLORSPACE_LINEAR):
		for (; x < max_x; x++)
		{
			int decode_pixel_index = x * channels;
			int input_pixel_index = stbir__edge_wrap(edge_horizontal, x, input_w) * channels;
			for (c = 0; c < channels; c++)
				decode_buffer[decode_pixel_index + c] = ((float)((const unsigned short*)input_data)[input_pixel_index + c]) / stbir__max_uint16_as_float;
		}
		break;

	case STBIR__DECODE(STBIR_TYPE_UINT16, STBIR_COLORSPACE_SRGB):
		for (; x < max_x; x++)
		{
			int decode_pixel_index = x * channels;
			int input_pixel_index = stbir__edge_wrap(edge_horizontal, x, input_w) * channels;
			for (c = 0; c < channels; c++)
				decode_buffer[decode_pixel_index + c] = stbir__srgb_to_linear(((float)((const unsigned short*)input_data)[input_pixel_index + c]) / stbir__max_uint16_as_float);

			if (!(stbir_info->flags&STBIR_FLAG_ALPHA_USES_COLORSPACE))
				decode_buffer[decode_pixel_index + alpha_channel] = ((float)((const unsigned short*)input_data)[input_pixel_index + alpha_channel]) / stbir__max_uint16_as_float;
		}
		break;

	case STBIR__DECODE(STBIR_TYPE_UINT32, STBIR_COLORSPACE_LINEAR):
		for (; x < max_x; x++)
		{
			int decode_pixel_index = x * channels;
			int input_pixel_index = stbir__edge_wrap(edge_horizontal, x, input_w) * channels;
			for (c = 0; c < channels; c++)
				decode_buffer[decode_pixel_index + c] = (float)(((double)((const unsigned int*)input_data)[input_pixel_index + c]) / stbir__max_uint32_as_float);
		}
		break;

	case STBIR__DECODE(STBIR_TYPE_UINT32, STBIR_COLORSPACE_SRGB):
		for (; x < max_x; x++)
		{
			int decode_pixel_index = x * channels;
			int input_pixel_index = stbir__edge_wrap(edge_horizontal, x, input_w) * channels;
			for (c = 0; c < channels; c++)
				decode_buffer[decode_pixel_index + c] = stbir__srgb_to_linear((float)(((double)((const unsigned int*)input_data)[input_pixel_index + c]) / stbir__max_uint32_as_float));

			if (!(stbir_info->flags&STBIR_FLAG_ALPHA_USES_COLORSPACE))
				decode_buffer[decode_pixel_index + alpha_channel] = (float)(((double)((const unsigned int*)input_data)[input_pixel_index + alpha_channel]) / stbir__max_uint32_as_float);
		}
		break;

	case STBIR__DECODE(STBIR_TYPE_FLOAT, STBIR_COLORSPACE_LINEAR):
		for (; x < max_x; x++)
		{
			int decode_pixel_index = x * channels;
			int input_pixel_index = stbir__edge_wrap(edge_horizontal, x, input_w) * channels;
			for (c = 0; c < channels; c++)
				decode_buffer[decode_pixel_index + c] = ((const float*)input_data)[input_pixel_index + c];
		}
		break;

	case STBIR__DECODE(STBIR_TYPE_FLOAT, STBIR_COLORSPACE_SRGB):
		for (; x < max_x; x++)
		{
			int decode_pixel_index = x * channels;
			int input_pixel_index = stbir__edge_wrap(edge_horizontal, x, input_w) * channels;
			for (c = 0; c < channels; c++)
				decode_buffer[decode_pixel_index + c] = stbir__srgb_to_linear(((const float*)input_data)[input_pixel_index + c]);

			if (!(stbir_info->flags&STBIR_FLAG_ALPHA_USES_COLORSPACE))
				decode_buffer[decode_pixel_index + alpha_channel] = ((const float*)input_data)[input_pixel_index + alpha_channel];
		}

		break;

	default:
		STBIR_ASSERT(!"Unknown type/colorspace/channels combination.");
		break;
	}

	if (!(stbir_info->flags & STBIR_FLAG_ALPHA_PREMULTIPLIED))
	{
		for (x = -stbir_info->horizontal_filter_pixel_margin; x < max_x; x++)
		{
			int decode_pixel_index = x * channels;

			// If the alpha value is 0 it will clobber the color values. Make sure it's not.
			float alpha = decode_buffer[decode_pixel_index + alpha_channel];
#ifndef STBIR_NO_ALPHA_EPSILON
			if (stbir_info->type != STBIR_TYPE_FLOAT) {
				alpha += STBIR_ALPHA_EPSILON;
				decode_buffer[decode_pixel_index + alpha_channel] = alpha;
			}
#endif
			for (c = 0; c < channels; c++)
			{
				if (c == alpha_channel)
					continue;

				decode_buffer[decode_pixel_index + c] *= alpha;
			}
		}
	}

	if (edge_horizontal == STBIR_EDGE_ZERO)
	{
		for (x = -stbir_info->horizontal_filter_pixel_margin; x < 0; x++)
		{
			for (c = 0; c < channels; c++)
				decode_buffer[x*channels + c] = 0;
		}
		for (x = input_w; x < max_x; x++)
		{
			for (c = 0; c < channels; c++)
				decode_buffer[x*channels + c] = 0;
		}
	}
}

static float* stbir__get_ring_buffer_entry(float* ring_buffer, int index, int ring_buffer_length)
{
	return &ring_buffer[index * ring_buffer_length];
}

static float* stbir__add_empty_ring_buffer_entry(stbir__info* stbir_info, int n)
{
	int ring_buffer_index;
	float* ring_buffer;

	stbir_info->ring_buffer_last_scanline = n;

	if (stbir_info->ring_buffer_begin_index < 0)
	{
		ring_buffer_index = stbir_info->ring_buffer_begin_index = 0;
		stbir_info->ring_buffer_first_scanline = n;
	}
	else
	{
		ring_buffer_index = (stbir_info->ring_buffer_begin_index + (stbir_info->ring_buffer_last_scanline - stbir_info->ring_buffer_first_scanline)) % stbir_info->ring_buffer_num_entries;
		STBIR_ASSERT(ring_buffer_index != stbir_info->ring_buffer_begin_index);
	}

	ring_buffer = stbir__get_ring_buffer_entry(stbir_info->ring_buffer, ring_buffer_index, stbir_info->ring_buffer_length_bytes / sizeof(float));
	memset(ring_buffer, 0, stbir_info->ring_buffer_length_bytes);

	return ring_buffer;
}


static void stbir__resample_horizontal_upsample(stbir__info* stbir_info, float* output_buffer)
{
	int x, k;
	int output_w = stbir_info->output_w;
	int channels = stbir_info->channels;
	float* decode_buffer = stbir__get_decode_buffer(stbir_info);
	stbir__contributors* horizontal_contributors = stbir_info->horizontal_contributors;
	float* horizontal_coefficients = stbir_info->horizontal_coefficients;
	int coefficient_width = stbir_info->horizontal_coefficient_width;

	for (x = 0; x < output_w; x++)
	{
		int n0 = horizontal_contributors[x].n0;
		int n1 = horizontal_contributors[x].n1;

		int out_pixel_index = x * channels;
		int coefficient_group = coefficient_width * x;
		int coefficient_counter = 0;

		STBIR_ASSERT(n1 >= n0);
		STBIR_ASSERT(n0 >= -stbir_info->horizontal_filter_pixel_margin);
		STBIR_ASSERT(n1 >= -stbir_info->horizontal_filter_pixel_margin);
		STBIR_ASSERT(n0 < stbir_info->input_w + stbir_info->horizontal_filter_pixel_margin);
		STBIR_ASSERT(n1 < stbir_info->input_w + stbir_info->horizontal_filter_pixel_margin);

		switch (channels) {
		case 1:
			for (k = n0; k <= n1; k++)
			{
				int in_pixel_index = k * 1;
				float coefficient = horizontal_coefficients[coefficient_group + coefficient_counter++];
				STBIR_ASSERT(coefficient != 0);
				output_buffer[out_pixel_index + 0] += decode_buffer[in_pixel_index + 0] * coefficient;
			}
			break;
		case 2:
			for (k = n0; k <= n1; k++)
			{
				int in_pixel_index = k * 2;
				float coefficient = horizontal_coefficients[coefficient_group + coefficient_counter++];
				STBIR_ASSERT(coefficient != 0);
				output_buffer[out_pixel_index + 0] += decode_buffer[in_pixel_index + 0] * coefficient;
				output_buffer[out_pixel_index + 1] += decode_buffer[in_pixel_index + 1] * coefficient;
			}
			break;
		case 3:
			for (k = n0; k <= n1; k++)
			{
				int in_pixel_index = k * 3;
				float coefficient = horizontal_coefficients[coefficient_group + coefficient_counter++];
				STBIR_ASSERT(coefficient != 0);
				output_buffer[out_pixel_index + 0] += decode_buffer[in_pixel_index + 0] * coefficient;
				output_buffer[out_pixel_index + 1] += decode_buffer[in_pixel_index + 1] * coefficient;
				output_buffer[out_pixel_index + 2] += decode_buffer[in_pixel_index + 2] * coefficient;
			}
			break;
		case 4:
			for (k = n0; k <= n1; k++)
			{
				int in_pixel_index = k * 4;
				float coefficient = horizontal_coefficients[coefficient_group + coefficient_counter++];
				STBIR_ASSERT(coefficient != 0);
				output_buffer[out_pixel_index + 0] += decode_buffer[in_pixel_index + 0] * coefficient;
				output_buffer[out_pixel_index + 1] += decode_buffer[in_pixel_index + 1] * coefficient;
				output_buffer[out_pixel_index + 2] += decode_buffer[in_pixel_index + 2] * coefficient;
				output_buffer[out_pixel_index + 3] += decode_buffer[in_pixel_index + 3] * coefficient;
			}
			break;
		default:
			for (k = n0; k <= n1; k++)
			{
				int in_pixel_index = k * channels;
				float coefficient = horizontal_coefficients[coefficient_group + coefficient_counter++];
				int c;
				STBIR_ASSERT(coefficient != 0);
				for (c = 0; c < channels; c++)
					output_buffer[out_pixel_index + c] += decode_buffer[in_pixel_index + c] * coefficient;
			}
			break;
		}
	}
}

static void stbir__resample_horizontal_downsample(stbir__info* stbir_info, float* output_buffer)
{
	int x, k;
	int input_w = stbir_info->input_w;
	int channels = stbir_info->channels;
	float* decode_buffer = stbir__get_decode_buffer(stbir_info);
	stbir__contributors* horizontal_contributors = stbir_info->horizontal_contributors;
	float* horizontal_coefficients = stbir_info->horizontal_coefficients;
	int coefficient_width = stbir_info->horizontal_coefficient_width;
	int filter_pixel_margin = stbir_info->horizontal_filter_pixel_margin;
	int max_x = input_w + filter_pixel_margin * 2;

	STBIR_ASSERT(!stbir__use_width_upsampling(stbir_info));

	switch (channels) {
	case 1:
		for (x = 0; x < max_x; x++)
		{
			int n0 = horizontal_contributors[x].n0;
			int n1 = horizontal_contributors[x].n1;

			int in_x = x - filter_pixel_margin;
			int in_pixel_index = in_x * 1;
			int max_n = n1;
			int coefficient_group = coefficient_width * x;

			for (k = n0; k <= max_n; k++)
			{
				int out_pixel_index = k * 1;
				float coefficient = horizontal_coefficients[coefficient_group + k - n0];
				STBIR_ASSERT(coefficient != 0);
				output_buffer[out_pixel_index + 0] += decode_buffer[in_pixel_index + 0] * coefficient;
			}
		}
		break;

	case 2:
		for (x = 0; x < max_x; x++)
		{
			int n0 = horizontal_contributors[x].n0;
			int n1 = horizontal_contributors[x].n1;

			int in_x = x - filter_pixel_margin;
			int in_pixel_index = in_x * 2;
			int max_n = n1;
			int coefficient_group = coefficient_width * x;

			for (k = n0; k <= max_n; k++)
			{
				int out_pixel_index = k * 2;
				float coefficient = horizontal_coefficients[coefficient_group + k - n0];
				STBIR_ASSERT(coefficient != 0);
				output_buffer[out_pixel_index + 0] += decode_buffer[in_pixel_index + 0] * coefficient;
				output_buffer[out_pixel_index + 1] += decode_buffer[in_pixel_index + 1] * coefficient;
			}
		}
		break;

	case 3:
		for (x = 0; x < max_x; x++)
		{
			int n0 = horizontal_contributors[x].n0;
			int n1 = horizontal_contributors[x].n1;

			int in_x = x - filter_pixel_margin;
			int in_pixel_index = in_x * 3;
			int max_n = n1;
			int coefficient_group = coefficient_width * x;

			for (k = n0; k <= max_n; k++)
			{
				int out_pixel_index = k * 3;
				float coefficient = horizontal_coefficients[coefficient_group + k - n0];
				STBIR_ASSERT(coefficient != 0);
				output_buffer[out_pixel_index + 0] += decode_buffer[in_pixel_index + 0] * coefficient;
				output_buffer[out_pixel_index + 1] += decode_buffer[in_pixel_index + 1] * coefficient;
				output_buffer[out_pixel_index + 2] += decode_buffer[in_pixel_index + 2] * coefficient;
			}
		}
		break;

	case 4:
		for (x = 0; x < max_x; x++)
		{
			int n0 = horizontal_contributors[x].n0;
			int n1 = horizontal_contributors[x].n1;

			int in_x = x - filter_pixel_margin;
			int in_pixel_index = in_x * 4;
			int max_n = n1;
			int coefficient_group = coefficient_width * x;

			for (k = n0; k <= max_n; k++)
			{
				int out_pixel_index = k * 4;
				float coefficient = horizontal_coefficients[coefficient_group + k - n0];
				STBIR_ASSERT(coefficient != 0);
				output_buffer[out_pixel_index + 0] += decode_buffer[in_pixel_index + 0] * coefficient;
				output_buffer[out_pixel_index + 1] += decode_buffer[in_pixel_index + 1] * coefficient;
				output_buffer[out_pixel_index + 2] += decode_buffer[in_pixel_index + 2] * coefficient;
				output_buffer[out_pixel_index + 3] += decode_buffer[in_pixel_index + 3] * coefficient;
			}
		}
		break;

	default:
		for (x = 0; x < max_x; x++)
		{
			int n0 = horizontal_contributors[x].n0;
			int n1 = horizontal_contributors[x].n1;

			int in_x = x - filter_pixel_margin;
			int in_pixel_index = in_x * channels;
			int max_n = n1;
			int coefficient_group = coefficient_width * x;

			for (k = n0; k <= max_n; k++)
			{
				int c;
				int out_pixel_index = k * channels;
				float coefficient = horizontal_coefficients[coefficient_group + k - n0];
				STBIR_ASSERT(coefficient != 0);
				for (c = 0; c < channels; c++)
					output_buffer[out_pixel_index + c] += decode_buffer[in_pixel_index + c] * coefficient;
			}
		}
		break;
	}
}

static void stbir__decode_and_resample_upsample(stbir__info* stbir_info, int n)
{
	// Decode the nth scanline from the source image into the decode buffer.
	stbir__decode_scanline(stbir_info, n);

	// Now resample it into the ring buffer.
	if (stbir__use_width_upsampling(stbir_info))
		stbir__resample_horizontal_upsample(stbir_info, stbir__add_empty_ring_buffer_entry(stbir_info, n));
	else
		stbir__resample_horizontal_downsample(stbir_info, stbir__add_empty_ring_buffer_entry(stbir_info, n));

	// Now it's sitting in the ring buffer ready to be used as source for the vertical sampling.
}

static void stbir__decode_and_resample_downsample(stbir__info* stbir_info, int n)
{
	// Decode the nth scanline from the source image into the decode buffer.
	stbir__decode_scanline(stbir_info, n);

	memset(stbir_info->horizontal_buffer, 0, stbir_info->output_w * stbir_info->channels * sizeof(float));

	// Now resample it into the horizontal buffer.
	if (stbir__use_width_upsampling(stbir_info))
		stbir__resample_horizontal_upsample(stbir_info, stbir_info->horizontal_buffer);
	else
		stbir__resample_horizontal_downsample(stbir_info, stbir_info->horizontal_buffer);

	// Now it's sitting in the horizontal buffer ready to be distributed into the ring buffers.
}

// Get the specified scan line from the ring buffer.
static float* stbir__get_ring_buffer_scanline(int get_scanline, float* ring_buffer, int begin_index, int first_scanline, int ring_buffer_num_entries, int ring_buffer_length)
{
	int ring_buffer_index = (begin_index + (get_scanline - first_scanline)) % ring_buffer_num_entries;
	return stbir__get_ring_buffer_entry(ring_buffer, ring_buffer_index, ring_buffer_length);
}


static void stbir__encode_scanline(stbir__info* stbir_info, int num_pixels, void *output_buffer, float *encode_buffer, int channels, int alpha_channel, int decode)
{
	int x;
	int n;
	int num_nonalpha;
	stbir_uint16 nonalpha[STBIR_MAX_CHANNELS];

	if (!(stbir_info->flags&STBIR_FLAG_ALPHA_PREMULTIPLIED))
	{
		for (x = 0; x < num_pixels; ++x)
		{
			int pixel_index = x * channels;

			float alpha = encode_buffer[pixel_index + alpha_channel];
			float reciprocal_alpha = alpha ? 1.0f / alpha : 0;

			// unrolling this produced a 1% slowdown upscaling a large RGBA linear-space image on my machine - stb
			for (n = 0; n < channels; n++)
				if (n != alpha_channel)
					encode_buffer[pixel_index + n] *= reciprocal_alpha;

			// We added in a small epsilon to prevent the color channel from being deleted with zero alpha.
			// Because we only add it for integer types, it will automatically be discarded on integer
			// conversion, so we don't need to subtract it back out (which would be problematic for
			// numeric precision reasons).
		}
	}

	// build a table of all channels that need colorspace correction, so
	// we don't perform colorspace correction on channels that don't need it.
	for (x = 0, num_nonalpha = 0; x < channels; ++x)
	{
		if (x != alpha_channel || (stbir_info->flags & STBIR_FLAG_ALPHA_USES_COLORSPACE))
		{
			nonalpha[num_nonalpha++] = (stbir_uint16)x;
		}
	}

#define STBIR__ROUND_INT(f)    ((int)          ((f)+0.5))
#define STBIR__ROUND_UINT(f)   ((stbir_uint32) ((f)+0.5))

#ifdef STBIR__SATURATE_INT
#define STBIR__ENCODE_LINEAR8(f)   stbir__saturate8 (STBIR__ROUND_INT((f) * stbir__max_uint8_as_float ))
#define STBIR__ENCODE_LINEAR16(f)  stbir__saturate16(STBIR__ROUND_INT((f) * stbir__max_uint16_as_float))
#else
#define STBIR__ENCODE_LINEAR8(f)   (unsigned char ) STBIR__ROUND_INT(stbir__saturate(f) * stbir__max_uint8_as_float )
#define STBIR__ENCODE_LINEAR16(f)  (unsigned short) STBIR__ROUND_INT(stbir__saturate(f) * stbir__max_uint16_as_float)
#endif

	switch (decode)
	{
	case STBIR__DECODE(STBIR_TYPE_UINT8, STBIR_COLORSPACE_LINEAR):
		for (x = 0; x < num_pixels; ++x)
		{
			int pixel_index = x * channels;

			for (n = 0; n < channels; n++)
			{
				int index = pixel_index + n;
				((unsigned char*)output_buffer)[index] = STBIR__ENCODE_LINEAR8(encode_buffer[index]);
			}
		}
		break;

	case STBIR__DECODE(STBIR_TYPE_UINT8, STBIR_COLORSPACE_SRGB):
		for (x = 0; x < num_pixels; ++x)
		{
			int pixel_index = x * channels;

			for (n = 0; n < num_nonalpha; n++)
			{
				int index = pixel_index + nonalpha[n];
				((unsigned char*)output_buffer)[index] = stbir__linear_to_srgb_uchar(encode_buffer[index]);
			}

			if (!(stbir_info->flags & STBIR_FLAG_ALPHA_USES_COLORSPACE))
				((unsigned char *)output_buffer)[pixel_index + alpha_channel] = STBIR__ENCODE_LINEAR8(encode_buffer[pixel_index + alpha_channel]);
		}
		break;

	case STBIR__DECODE(STBIR_TYPE_UINT16, STBIR_COLORSPACE_LINEAR):
		for (x = 0; x < num_pixels; ++x)
		{
			int pixel_index = x * channels;

			for (n = 0; n < channels; n++)
			{
				int index = pixel_index + n;
				((unsigned short*)output_buffer)[index] = STBIR__ENCODE_LINEAR16(encode_buffer[index]);
			}
		}
		break;

	case STBIR__DECODE(STBIR_TYPE_UINT16, STBIR_COLORSPACE_SRGB):
		for (x = 0; x < num_pixels; ++x)
		{
			int pixel_index = x * channels;

			for (n = 0; n < num_nonalpha; n++)
			{
				int index = pixel_index + nonalpha[n];
				((unsigned short*)output_buffer)[index] = (unsigned short)STBIR__ROUND_INT(stbir__linear_to_srgb(stbir__saturate(encode_buffer[index])) * stbir__max_uint16_as_float);
			}

			if (!(stbir_info->flags&STBIR_FLAG_ALPHA_USES_COLORSPACE))
				((unsigned short*)output_buffer)[pixel_index + alpha_channel] = STBIR__ENCODE_LINEAR16(encode_buffer[pixel_index + alpha_channel]);
		}

		break;

	case STBIR__DECODE(STBIR_TYPE_UINT32, STBIR_COLORSPACE_LINEAR):
		for (x = 0; x < num_pixels; ++x)
		{
			int pixel_index = x * channels;

			for (n = 0; n < channels; n++)
			{
				int index = pixel_index + n;
				((unsigned int*)output_buffer)[index] = (unsigned int)STBIR__ROUND_UINT(((double)stbir__saturate(encode_buffer[index])) * stbir__max_uint32_as_float);
			}
		}
		break;

	case STBIR__DECODE(STBIR_TYPE_UINT32, STBIR_COLORSPACE_SRGB):
		for (x = 0; x < num_pixels; ++x)
		{
			int pixel_index = x * channels;

			for (n = 0; n < num_nonalpha; n++)
			{
				int index = pixel_index + nonalpha[n];
				((unsigned int*)output_buffer)[index] = (unsigned int)STBIR__ROUND_UINT(((double)stbir__linear_to_srgb(stbir__saturate(encode_buffer[index]))) * stbir__max_uint32_as_float);
			}

			if (!(stbir_info->flags&STBIR_FLAG_ALPHA_USES_COLORSPACE))
				((unsigned int*)output_buffer)[pixel_index + alpha_channel] = (unsigned int)STBIR__ROUND_INT(((double)stbir__saturate(encode_buffer[pixel_index + alpha_channel])) * stbir__max_uint32_as_float);
		}
		break;

	case STBIR__DECODE(STBIR_TYPE_FLOAT, STBIR_COLORSPACE_LINEAR):
		for (x = 0; x < num_pixels; ++x)
		{
			int pixel_index = x * channels;

			for (n = 0; n < channels; n++)
			{
				int index = pixel_index + n;
				((float*)output_buffer)[index] = encode_buffer[index];
			}
		}
		break;

	case STBIR__DECODE(STBIR_TYPE_FLOAT, STBIR_COLORSPACE_SRGB):
		for (x = 0; x < num_pixels; ++x)
		{
			int pixel_index = x * channels;

			for (n = 0; n < num_nonalpha; n++)
			{
				int index = pixel_index + nonalpha[n];
				((float*)output_buffer)[index] = stbir__linear_to_srgb(encode_buffer[index]);
			}

			if (!(stbir_info->flags&STBIR_FLAG_ALPHA_USES_COLORSPACE))
				((float*)output_buffer)[pixel_index + alpha_channel] = encode_buffer[pixel_index + alpha_channel];
		}
		break;

	default:
		STBIR_ASSERT(!"Unknown type/colorspace/channels combination.");
		break;
	}
}

static void stbir__resample_vertical_upsample(stbir__info* stbir_info, int n)
{
	int x, k;
	int output_w = stbir_info->output_w;
	stbir__contributors* vertical_contributors = stbir_info->vertical_contributors;
	float* vertical_coefficients = stbir_info->vertical_coefficients;
	int channels = stbir_info->channels;
	int alpha_channel = stbir_info->alpha_channel;
	int type = stbir_info->type;
	int colorspace = stbir_info->colorspace;
	int ring_buffer_entries = stbir_info->ring_buffer_num_entries;
	void* output_data = stbir_info->output_data;
	float* encode_buffer = stbir_info->encode_buffer;
	int decode = STBIR__DECODE(type, colorspace);
	int coefficient_width = stbir_info->vertical_coefficient_width;
	int coefficient_counter;
	int contributor = n;

	float* ring_buffer = stbir_info->ring_buffer;
	int ring_buffer_begin_index = stbir_info->ring_buffer_begin_index;
	int ring_buffer_first_scanline = stbir_info->ring_buffer_first_scanline;
	int ring_buffer_length = stbir_info->ring_buffer_length_bytes / sizeof(float);

	int n0, n1, output_row_start;
	int coefficient_group = coefficient_width * contributor;

	n0 = vertical_contributors[contributor].n0;
	n1 = vertical_contributors[contributor].n1;

	output_row_start = n * stbir_info->output_stride_bytes;

	STBIR_ASSERT(stbir__use_height_upsampling(stbir_info));

	memset(encode_buffer, 0, output_w * sizeof(float) * channels);

	// I tried reblocking this for better cache usage of encode_buffer
	// (using x_outer, k, x_inner), but it lost speed. -- stb

	coefficient_counter = 0;
	switch (channels) {
	case 1:
		for (k = n0; k <= n1; k++)
		{
			int coefficient_index = coefficient_counter++;
			float* ring_buffer_entry = stbir__get_ring_buffer_scanline(k, ring_buffer, ring_buffer_begin_index, ring_buffer_first_scanline, ring_buffer_entries, ring_buffer_length);
			float coefficient = vertical_coefficients[coefficient_group + coefficient_index];
			for (x = 0; x < output_w; ++x)
			{
				int in_pixel_index = x * 1;
				encode_buffer[in_pixel_index + 0] += ring_buffer_entry[in_pixel_index + 0] * coefficient;
			}
		}
		break;
	case 2:
		for (k = n0; k <= n1; k++)
		{
			int coefficient_index = coefficient_counter++;
			float* ring_buffer_entry = stbir__get_ring_buffer_scanline(k, ring_buffer, ring_buffer_begin_index, ring_buffer_first_scanline, ring_buffer_entries, ring_buffer_length);
			float coefficient = vertical_coefficients[coefficient_group + coefficient_index];
			for (x = 0; x < output_w; ++x)
			{
				int in_pixel_index = x * 2;
				encode_buffer[in_pixel_index + 0] += ring_buffer_entry[in_pixel_index + 0] * coefficient;
				encode_buffer[in_pixel_index + 1] += ring_buffer_entry[in_pixel_index + 1] * coefficient;
			}
		}
		break;
	case 3:
		for (k = n0; k <= n1; k++)
		{
			int coefficient_index = coefficient_counter++;
			float* ring_buffer_entry = stbir__get_ring_buffer_scanline(k, ring_buffer, ring_buffer_begin_index, ring_buffer_first_scanline, ring_buffer_entries, ring_buffer_length);
			float coefficient = vertical_coefficients[coefficient_group + coefficient_index];
			for (x = 0; x < output_w; ++x)
			{
				int in_pixel_index = x * 3;
				encode_buffer[in_pixel_index + 0] += ring_buffer_entry[in_pixel_index + 0] * coefficient;
				encode_buffer[in_pixel_index + 1] += ring_buffer_entry[in_pixel_index + 1] * coefficient;
				encode_buffer[in_pixel_index + 2] += ring_buffer_entry[in_pixel_index + 2] * coefficient;
			}
		}
		break;
	case 4:
		for (k = n0; k <= n1; k++)
		{
			int coefficient_index = coefficient_counter++;
			float* ring_buffer_entry = stbir__get_ring_buffer_scanline(k, ring_buffer, ring_buffer_begin_index, ring_buffer_first_scanline, ring_buffer_entries, ring_buffer_length);
			float coefficient = vertical_coefficients[coefficient_group + coefficient_index];
			for (x = 0; x < output_w; ++x)
			{
				int in_pixel_index = x * 4;
				encode_buffer[in_pixel_index + 0] += ring_buffer_entry[in_pixel_index + 0] * coefficient;
				encode_buffer[in_pixel_index + 1] += ring_buffer_entry[in_pixel_index + 1] * coefficient;
				encode_buffer[in_pixel_index + 2] += ring_buffer_entry[in_pixel_index + 2] * coefficient;
				encode_buffer[in_pixel_index + 3] += ring_buffer_entry[in_pixel_index + 3] * coefficient;
			}
		}
		break;
	default:
		for (k = n0; k <= n1; k++)
		{
			int coefficient_index = coefficient_counter++;
			float* ring_buffer_entry = stbir__get_ring_buffer_scanline(k, ring_buffer, ring_buffer_begin_index, ring_buffer_first_scanline, ring_buffer_entries, ring_buffer_length);
			float coefficient = vertical_coefficients[coefficient_group + coefficient_index];
			for (x = 0; x < output_w; ++x)
			{
				int in_pixel_index = x * channels;
				int c;
				for (c = 0; c < channels; c++)
					encode_buffer[in_pixel_index + c] += ring_buffer_entry[in_pixel_index + c] * coefficient;
			}
		}
		break;
	}
	stbir__encode_scanline(stbir_info, output_w, (char *)output_data + output_row_start, encode_buffer, channels, alpha_channel, decode);
}

static void stbir__resample_vertical_downsample(stbir__info* stbir_info, int n)
{
	int x, k;
	int output_w = stbir_info->output_w;
	stbir__contributors* vertical_contributors = stbir_info->vertical_contributors;
	float* vertical_coefficients = stbir_info->vertical_coefficients;
	int channels = stbir_info->channels;
	int ring_buffer_entries = stbir_info->ring_buffer_num_entries;
	float* horizontal_buffer = stbir_info->horizontal_buffer;
	int coefficient_width = stbir_info->vertical_coefficient_width;
	int contributor = n + stbir_info->vertical_filter_pixel_margin;

	float* ring_buffer = stbir_info->ring_buffer;
	int ring_buffer_begin_index = stbir_info->ring_buffer_begin_index;
	int ring_buffer_first_scanline = stbir_info->ring_buffer_first_scanline;
	int ring_buffer_length = stbir_info->ring_buffer_length_bytes / sizeof(float);
	int n0, n1;

	n0 = vertical_contributors[contributor].n0;
	n1 = vertical_contributors[contributor].n1;

	STBIR_ASSERT(!stbir__use_height_upsampling(stbir_info));

	for (k = n0; k <= n1; k++)
	{
		int coefficient_index = k - n0;
		int coefficient_group = coefficient_width * contributor;
		float coefficient = vertical_coefficients[coefficient_group + coefficient_index];

		float* ring_buffer_entry = stbir__get_ring_buffer_scanline(k, ring_buffer, ring_buffer_begin_index, ring_buffer_first_scanline, ring_buffer_entries, ring_buffer_length);

		switch (channels) {
		case 1:
			for (x = 0; x < output_w; x++)
			{
				int in_pixel_index = x * 1;
				ring_buffer_entry[in_pixel_index + 0] += horizontal_buffer[in_pixel_index + 0] * coefficient;
			}
			break;
		case 2:
			for (x = 0; x < output_w; x++)
			{
				int in_pixel_index = x * 2;
				ring_buffer_entry[in_pixel_index + 0] += horizontal_buffer[in_pixel_index + 0] * coefficient;
				ring_buffer_entry[in_pixel_index + 1] += horizontal_buffer[in_pixel_index + 1] * coefficient;
			}
			break;
		case 3:
			for (x = 0; x < output_w; x++)
			{
				int in_pixel_index = x * 3;
				ring_buffer_entry[in_pixel_index + 0] += horizontal_buffer[in_pixel_index + 0] * coefficient;
				ring_buffer_entry[in_pixel_index + 1] += horizontal_buffer[in_pixel_index + 1] * coefficient;
				ring_buffer_entry[in_pixel_index + 2] += horizontal_buffer[in_pixel_index + 2] * coefficient;
			}
			break;
		case 4:
			for (x = 0; x < output_w; x++)
			{
				int in_pixel_index = x * 4;
				ring_buffer_entry[in_pixel_index + 0] += horizontal_buffer[in_pixel_index + 0] * coefficient;
				ring_buffer_entry[in_pixel_index + 1] += horizontal_buffer[in_pixel_index + 1] * coefficient;
				ring_buffer_entry[in_pixel_index + 2] += horizontal_buffer[in_pixel_index + 2] * coefficient;
				ring_buffer_entry[in_pixel_index + 3] += horizontal_buffer[in_pixel_index + 3] * coefficient;
			}
			break;
		default:
			for (x = 0; x < output_w; x++)
			{
				int in_pixel_index = x * channels;

				int c;
				for (c = 0; c < channels; c++)
					ring_buffer_entry[in_pixel_index + c] += horizontal_buffer[in_pixel_index + c] * coefficient;
			}
			break;
		}
	}
}

static void stbir__buffer_loop_upsample(stbir__info* stbir_info)
{
	int y;
	float scale_ratio = stbir_info->vertical_scale;
	float out_scanlines_radius = stbir__filter_info_table[stbir_info->vertical_filter].support(1 / scale_ratio) * scale_ratio;

	STBIR_ASSERT(stbir__use_height_upsampling(stbir_info));

	for (y = 0; y < stbir_info->output_h; y++)
	{
		float in_center_of_out = 0; // Center of the current out scanline in the in scanline space
		int in_first_scanline = 0, in_last_scanline = 0;

		stbir__calculate_sample_range_upsample(y, out_scanlines_radius, scale_ratio, stbir_info->vertical_shift, &in_first_scanline, &in_last_scanline, &in_center_of_out);

		STBIR_ASSERT(in_last_scanline - in_first_scanline + 1 <= stbir_info->ring_buffer_num_entries);

		if (stbir_info->ring_buffer_begin_index >= 0)
		{
			// Get rid of whatever we don't need anymore.
			while (in_first_scanline > stbir_info->ring_buffer_first_scanline)
			{
				if (stbir_info->ring_buffer_first_scanline == stbir_info->ring_buffer_last_scanline)
				{
					// We just popped the last scanline off the ring buffer.
					// Reset it to the empty state.
					stbir_info->ring_buffer_begin_index = -1;
					stbir_info->ring_buffer_first_scanline = 0;
					stbir_info->ring_buffer_last_scanline = 0;
					break;
				}
				else
				{
					stbir_info->ring_buffer_first_scanline++;
					stbir_info->ring_buffer_begin_index = (stbir_info->ring_buffer_begin_index + 1) % stbir_info->ring_buffer_num_entries;
				}
			}
		}

		// Load in new ones.
		if (stbir_info->ring_buffer_begin_index < 0)
			stbir__decode_and_resample_upsample(stbir_info, in_first_scanline);

		while (in_last_scanline > stbir_info->ring_buffer_last_scanline)
			stbir__decode_and_resample_upsample(stbir_info, stbir_info->ring_buffer_last_scanline + 1);

		// Now all buffers should be ready to write a row of vertical sampling.
		stbir__resample_vertical_upsample(stbir_info, y);

		STBIR_PROGRESS_REPORT((float)y / stbir_info->output_h);
	}
}

static void stbir__empty_ring_buffer(stbir__info* stbir_info, int first_necessary_scanline)
{
	int output_stride_bytes = stbir_info->output_stride_bytes;
	int channels = stbir_info->channels;
	int alpha_channel = stbir_info->alpha_channel;
	int type = stbir_info->type;
	int colorspace = stbir_info->colorspace;
	int output_w = stbir_info->output_w;
	void* output_data = stbir_info->output_data;
	int decode = STBIR__DECODE(type, colorspace);

	float* ring_buffer = stbir_info->ring_buffer;
	int ring_buffer_length = stbir_info->ring_buffer_length_bytes / sizeof(float);

	if (stbir_info->ring_buffer_begin_index >= 0)
	{
		// Get rid of whatever we don't need anymore.
		while (first_necessary_scanline > stbir_info->ring_buffer_first_scanline)
		{
			if (stbir_info->ring_buffer_first_scanline >= 0 && stbir_info->ring_buffer_first_scanline < stbir_info->output_h)
			{
				int output_row_start = stbir_info->ring_buffer_first_scanline * output_stride_bytes;
				float* ring_buffer_entry = stbir__get_ring_buffer_entry(ring_buffer, stbir_info->ring_buffer_begin_index, ring_buffer_length);
				stbir__encode_scanline(stbir_info, output_w, (char *)output_data + output_row_start, ring_buffer_entry, channels, alpha_channel, decode);
				STBIR_PROGRESS_REPORT((float)stbir_info->ring_buffer_first_scanline / stbir_info->output_h);
			}

			if (stbir_info->ring_buffer_first_scanline == stbir_info->ring_buffer_last_scanline)
			{
				// We just popped the last scanline off the ring buffer.
				// Reset it to the empty state.
				stbir_info->ring_buffer_begin_index = -1;
				stbir_info->ring_buffer_first_scanline = 0;
				stbir_info->ring_buffer_last_scanline = 0;
				break;
			}
			else
			{
				stbir_info->ring_buffer_first_scanline++;
				stbir_info->ring_buffer_begin_index = (stbir_info->ring_buffer_begin_index + 1) % stbir_info->ring_buffer_num_entries;
			}
		}
	}
}

static void stbir__buffer_loop_downsample(stbir__info* stbir_info)
{
	int y;
	float scale_ratio = stbir_info->vertical_scale;
	int output_h = stbir_info->output_h;
	float in_pixels_radius = stbir__filter_info_table[stbir_info->vertical_filter].support(scale_ratio) / scale_ratio;
	int pixel_margin = stbir_info->vertical_filter_pixel_margin;
	int max_y = stbir_info->input_h + pixel_margin;

	STBIR_ASSERT(!stbir__use_height_upsampling(stbir_info));

	for (y = -pixel_margin; y < max_y; y++)
	{
		float out_center_of_in; // Center of the current out scanline in the in scanline space
		int out_first_scanline, out_last_scanline;

		stbir__calculate_sample_range_downsample(y, in_pixels_radius, scale_ratio, stbir_info->vertical_shift, &out_first_scanline, &out_last_scanline, &out_center_of_in);

		STBIR_ASSERT(out_last_scanline - out_first_scanline + 1 <= stbir_info->ring_buffer_num_entries);

		if (out_last_scanline < 0 || out_first_scanline >= output_h)
			continue;

		stbir__empty_ring_buffer(stbir_info, out_first_scanline);

		stbir__decode_and_resample_downsample(stbir_info, y);

		// Load in new ones.
		if (stbir_info->ring_buffer_begin_index < 0)
			stbir__add_empty_ring_buffer_entry(stbir_info, out_first_scanline);

		while (out_last_scanline > stbir_info->ring_buffer_last_scanline)
			stbir__add_empty_ring_buffer_entry(stbir_info, stbir_info->ring_buffer_last_scanline + 1);

		// Now the horizontal buffer is ready to write to all ring buffer rows.
		stbir__resample_vertical_downsample(stbir_info, y);
	}

	stbir__empty_ring_buffer(stbir_info, stbir_info->output_h);
}

static void stbir__setup(stbir__info *info, int input_w, int input_h, int output_w, int output_h, int channels)
{
	info->input_w = input_w;
	info->input_h = input_h;
	info->output_w = output_w;
	info->output_h = output_h;
	info->channels = channels;
}

static void stbir__calculate_transform(stbir__info *info, float s0, float t0, float s1, float t1, float *transform)
{
	info->s0 = s0;
	info->t0 = t0;
	info->s1 = s1;
	info->t1 = t1;

	if (transform)
	{
		info->horizontal_scale = transform[0];
		info->vertical_scale = transform[1];
		info->horizontal_shift = transform[2];
		info->vertical_shift = transform[3];
	}
	else
	{
		info->horizontal_scale = ((float)info->output_w / info->input_w) / (s1 - s0);
		info->vertical_scale = ((float)info->output_h / info->input_h) / (t1 - t0);

		info->horizontal_shift = s0 * info->output_w / (s1 - s0);
		info->vertical_shift = t0 * info->output_h / (t1 - t0);
	}
}

static void stbir__choose_filter(stbir__info *info, stbir_filter h_filter, stbir_filter v_filter)
{
	if (h_filter == 0)
		h_filter = stbir__use_upsampling(info->horizontal_scale) ? STBIR_DEFAULT_FILTER_UPSAMPLE : STBIR_DEFAULT_FILTER_DOWNSAMPLE;
	if (v_filter == 0)
		v_filter = stbir__use_upsampling(info->vertical_scale) ? STBIR_DEFAULT_FILTER_UPSAMPLE : STBIR_DEFAULT_FILTER_DOWNSAMPLE;
	info->horizontal_filter = h_filter;
	info->vertical_filter = v_filter;
}

static stbir_uint32 stbir__calculate_memory(stbir__info *info)
{
	int pixel_margin = stbir__get_filter_pixel_margin(info->horizontal_filter, info->horizontal_scale);
	int filter_height = stbir__get_filter_pixel_width(info->vertical_filter, info->vertical_scale);

	info->horizontal_num_contributors = stbir__get_contributors(info->horizontal_scale, info->horizontal_filter, info->input_w, info->output_w);
	info->vertical_num_contributors = stbir__get_contributors(info->vertical_scale, info->vertical_filter, info->input_h, info->output_h);

	// One extra entry because floating point precision problems sometimes cause an extra to be necessary.
	info->ring_buffer_num_entries = filter_height + 1;

	info->horizontal_contributors_size = info->horizontal_num_contributors * sizeof(stbir__contributors);
	info->horizontal_coefficients_size = stbir__get_total_horizontal_coefficients(info) * sizeof(float);
	info->vertical_contributors_size = info->vertical_num_contributors * sizeof(stbir__contributors);
	info->vertical_coefficients_size = stbir__get_total_vertical_coefficients(info) * sizeof(float);
	info->decode_buffer_size = (info->input_w + pixel_margin * 2) * info->channels * sizeof(float);
	info->horizontal_buffer_size = info->output_w * info->channels * sizeof(float);
	info->ring_buffer_size = info->output_w * info->channels * info->ring_buffer_num_entries * sizeof(float);
	info->encode_buffer_size = info->output_w * info->channels * sizeof(float);

	STBIR_ASSERT(info->horizontal_filter != 0);
	STBIR_ASSERT(info->horizontal_filter < STBIR__ARRAY_SIZE(stbir__filter_info_table)); // this now happens too late
	STBIR_ASSERT(info->vertical_filter != 0);
	STBIR_ASSERT(info->vertical_filter < STBIR__ARRAY_SIZE(stbir__filter_info_table)); // this now happens too late

	if (stbir__use_height_upsampling(info))
		// The horizontal buffer is for when we're downsampling the height and we
		// can't output the result of sampling the decode buffer directly into the
		// ring buffers.
		info->horizontal_buffer_size = 0;
	else
		// The encode buffer is to retain precision in the height upsampling method
		// and isn't used when height downsampling.
		info->encode_buffer_size = 0;

	return info->horizontal_contributors_size + info->horizontal_coefficients_size
		+ info->vertical_contributors_size + info->vertical_coefficients_size
		+ info->decode_buffer_size + info->horizontal_buffer_size
		+ info->ring_buffer_size + info->encode_buffer_size;
}

static int stbir__resize_allocated(stbir__info *info,
	const void* input_data, int input_stride_in_bytes,
	void* output_data, int output_stride_in_bytes,
	int alpha_channel, stbir_uint32 flags, stbir_datatype type,
	stbir_edge edge_horizontal, stbir_edge edge_vertical, stbir_colorspace colorspace,
	void* tempmem, size_t tempmem_size_in_bytes)
{
	size_t memory_required = stbir__calculate_memory(info);

	int width_stride_input = input_stride_in_bytes ? input_stride_in_bytes : info->channels * info->input_w * stbir__type_size[type];
	int width_stride_output = output_stride_in_bytes ? output_stride_in_bytes : info->channels * info->output_w * stbir__type_size[type];

#ifdef STBIR_DEBUG_OVERWRITE_TEST
#define OVERWRITE_ARRAY_SIZE 8
	unsigned char overwrite_output_before_pre[OVERWRITE_ARRAY_SIZE];
	unsigned char overwrite_tempmem_before_pre[OVERWRITE_ARRAY_SIZE];
	unsigned char overwrite_output_after_pre[OVERWRITE_ARRAY_SIZE];
	unsigned char overwrite_tempmem_after_pre[OVERWRITE_ARRAY_SIZE];

	size_t begin_forbidden = width_stride_output * (info->output_h - 1) + info->output_w * info->channels * stbir__type_size[type];
	memcpy(overwrite_output_before_pre, &((unsigned char*)output_data)[-OVERWRITE_ARRAY_SIZE], OVERWRITE_ARRAY_SIZE);
	memcpy(overwrite_output_after_pre, &((unsigned char*)output_data)[begin_forbidden], OVERWRITE_ARRAY_SIZE);
	memcpy(overwrite_tempmem_before_pre, &((unsigned char*)tempmem)[-OVERWRITE_ARRAY_SIZE], OVERWRITE_ARRAY_SIZE);
	memcpy(overwrite_tempmem_after_pre, &((unsigned char*)tempmem)[tempmem_size_in_bytes], OVERWRITE_ARRAY_SIZE);
#endif

	STBIR_ASSERT(info->channels >= 0);
	STBIR_ASSERT(info->channels <= STBIR_MAX_CHANNELS);

	if (info->channels < 0 || info->channels > STBIR_MAX_CHANNELS)
		return 0;

	STBIR_ASSERT(info->horizontal_filter < STBIR__ARRAY_SIZE(stbir__filter_info_table));
	STBIR_ASSERT(info->vertical_filter < STBIR__ARRAY_SIZE(stbir__filter_info_table));

	if (info->horizontal_filter >= STBIR__ARRAY_SIZE(stbir__filter_info_table))
		return 0;
	if (info->vertical_filter >= STBIR__ARRAY_SIZE(stbir__filter_info_table))
		return 0;

	if (alpha_channel < 0)
		flags |= STBIR_FLAG_ALPHA_USES_COLORSPACE | STBIR_FLAG_ALPHA_PREMULTIPLIED;

	if (!(flags&STBIR_FLAG_ALPHA_USES_COLORSPACE) || !(flags&STBIR_FLAG_ALPHA_PREMULTIPLIED)) {
		STBIR_ASSERT(alpha_channel >= 0 && alpha_channel < info->channels);
	}

	if (alpha_channel >= info->channels)
		return 0;

	STBIR_ASSERT(tempmem);

	if (!tempmem)
		return 0;

	STBIR_ASSERT(tempmem_size_in_bytes >= memory_required);

	if (tempmem_size_in_bytes < memory_required)
		return 0;

	memset(tempmem, 0, tempmem_size_in_bytes);

	info->input_data = input_data;
	info->input_stride_bytes = width_stride_input;

	info->output_data = output_data;
	info->output_stride_bytes = width_stride_output;

	info->alpha_channel = alpha_channel;
	info->flags = flags;
	info->type = type;
	info->edge_horizontal = edge_horizontal;
	info->edge_vertical = edge_vertical;
	info->colorspace = colorspace;

	info->horizontal_coefficient_width = stbir__get_coefficient_width(info->horizontal_filter, info->horizontal_scale);
	info->vertical_coefficient_width = stbir__get_coefficient_width(info->vertical_filter, info->vertical_scale);
	info->horizontal_filter_pixel_width = stbir__get_filter_pixel_width(info->horizontal_filter, info->horizontal_scale);
	info->vertical_filter_pixel_width = stbir__get_filter_pixel_width(info->vertical_filter, info->vertical_scale);
	info->horizontal_filter_pixel_margin = stbir__get_filter_pixel_margin(info->horizontal_filter, info->horizontal_scale);
	info->vertical_filter_pixel_margin = stbir__get_filter_pixel_margin(info->vertical_filter, info->vertical_scale);

	info->ring_buffer_length_bytes = info->output_w * info->channels * sizeof(float);
	info->decode_buffer_pixels = info->input_w + info->horizontal_filter_pixel_margin * 2;

#define STBIR__NEXT_MEMPTR(current, newtype) (newtype*)(((unsigned char*)current) + current##_size)

	info->horizontal_contributors = (stbir__contributors *)tempmem;
	info->horizontal_coefficients = STBIR__NEXT_MEMPTR(info->horizontal_contributors, float);
	info->vertical_contributors = STBIR__NEXT_MEMPTR(info->horizontal_coefficients, stbir__contributors);
	info->vertical_coefficients = STBIR__NEXT_MEMPTR(info->vertical_contributors, float);
	info->decode_buffer = STBIR__NEXT_MEMPTR(info->vertical_coefficients, float);

	if (stbir__use_height_upsampling(info))
	{
		info->horizontal_buffer = NULL;
		info->ring_buffer = STBIR__NEXT_MEMPTR(info->decode_buffer, float);
		info->encode_buffer = STBIR__NEXT_MEMPTR(info->ring_buffer, float);

		STBIR_ASSERT((size_t)STBIR__NEXT_MEMPTR(info->encode_buffer, unsigned char) == (size_t)tempmem + tempmem_size_in_bytes);
	}
	else
	{
		info->horizontal_buffer = STBIR__NEXT_MEMPTR(info->decode_buffer, float);
		info->ring_buffer = STBIR__NEXT_MEMPTR(info->horizontal_buffer, float);
		info->encode_buffer = NULL;

		STBIR_ASSERT((size_t)STBIR__NEXT_MEMPTR(info->ring_buffer, unsigned char) == (size_t)tempmem + tempmem_size_in_bytes);
	}

#undef STBIR__NEXT_MEMPTR

	// This signals that the ring buffer is empty
	info->ring_buffer_begin_index = -1;

	stbir__calculate_filters(info->horizontal_contributors, info->horizontal_coefficients, info->horizontal_filter, info->horizontal_scale, info->horizontal_shift, info->input_w, info->output_w);
	stbir__calculate_filters(info->vertical_contributors, info->vertical_coefficients, info->vertical_filter, info->vertical_scale, info->vertical_shift, info->input_h, info->output_h);

	STBIR_PROGRESS_REPORT(0);

	if (stbir__use_height_upsampling(info))
		stbir__buffer_loop_upsample(info);
	else
		stbir__buffer_loop_downsample(info);

	STBIR_PROGRESS_REPORT(1);

#ifdef STBIR_DEBUG_OVERWRITE_TEST
	STBIR_ASSERT(memcmp(overwrite_output_before_pre, &((unsigned char*)output_data)[-OVERWRITE_ARRAY_SIZE], OVERWRITE_ARRAY_SIZE) == 0);
	STBIR_ASSERT(memcmp(overwrite_output_after_pre, &((unsigned char*)output_data)[begin_forbidden], OVERWRITE_ARRAY_SIZE) == 0);
	STBIR_ASSERT(memcmp(overwrite_tempmem_before_pre, &((unsigned char*)tempmem)[-OVERWRITE_ARRAY_SIZE], OVERWRITE_ARRAY_SIZE) == 0);
	STBIR_ASSERT(memcmp(overwrite_tempmem_after_pre, &((unsigned char*)tempmem)[tempmem_size_in_bytes], OVERWRITE_ARRAY_SIZE) == 0);
#endif

	return 1;
}


static int stbir__resize_arbitrary(
	void *alloc_context,
	const void* input_data, int input_w, int input_h, int input_stride_in_bytes,
	void* output_data, int output_w, int output_h, int output_stride_in_bytes,
	float s0, float t0, float s1, float t1, float *transform,
	int channels, int alpha_channel, stbir_uint32 flags, stbir_datatype type,
	stbir_filter h_filter, stbir_filter v_filter,
	stbir_edge edge_horizontal, stbir_edge edge_vertical, stbir_colorspace colorspace)
{
	stbir__info info;
	int result;
	size_t memory_required;
	void* extra_memory;

	stbir__setup(&info, input_w, input_h, output_w, output_h, channels);
	stbir__calculate_transform(&info, s0, t0, s1, t1, transform);
	stbir__choose_filter(&info, h_filter, v_filter);
	memory_required = stbir__calculate_memory(&info);
	extra_memory = STBIR_MALLOC(memory_required, alloc_context);

	if (!extra_memory)
		return 0;

	result = stbir__resize_allocated(&info, input_data, input_stride_in_bytes,
		output_data, output_stride_in_bytes,
		alpha_channel, flags, type,
		edge_horizontal, edge_vertical,
		colorspace, extra_memory, memory_required);

	STBIR_FREE(extra_memory, alloc_context);

	return result;
}

STBIRDEF int stbir_resize_uint8(const unsigned char *input_pixels, int input_w, int input_h, int input_stride_in_bytes,
	unsigned char *output_pixels, int output_w, int output_h, int output_stride_in_bytes,
	int num_channels)
{
	return stbir__resize_arbitrary(NULL, input_pixels, input_w, input_h, input_stride_in_bytes,
		output_pixels, output_w, output_h, output_stride_in_bytes,
		0, 0, 1, 1, NULL, num_channels, -1, 0, STBIR_TYPE_UINT8, STBIR_FILTER_DEFAULT, STBIR_FILTER_DEFAULT,
		STBIR_EDGE_CLAMP, STBIR_EDGE_CLAMP, STBIR_COLORSPACE_LINEAR);
}

STBIRDEF int stbir_resize_float(const float *input_pixels, int input_w, int input_h, int input_stride_in_bytes,
	float *output_pixels, int output_w, int output_h, int output_stride_in_bytes,
	int num_channels)
{
	return stbir__resize_arbitrary(NULL, input_pixels, input_w, input_h, input_stride_in_bytes,
		output_pixels, output_w, output_h, output_stride_in_bytes,
		0, 0, 1, 1, NULL, num_channels, -1, 0, STBIR_TYPE_FLOAT, STBIR_FILTER_DEFAULT, STBIR_FILTER_DEFAULT,
		STBIR_EDGE_CLAMP, STBIR_EDGE_CLAMP, STBIR_COLORSPACE_LINEAR);
}

STBIRDEF int stbir_resize_uint8_srgb(const unsigned char *input_pixels, int input_w, int input_h, int input_stride_in_bytes,
	unsigned char *output_pixels, int output_w, int output_h, int output_stride_in_bytes,
	int num_channels, int alpha_channel, int flags)
{
	return stbir__resize_arbitrary(NULL, input_pixels, input_w, input_h, input_stride_in_bytes,
		output_pixels, output_w, output_h, output_stride_in_bytes,
		0, 0, 1, 1, NULL, num_channels, alpha_channel, flags, STBIR_TYPE_UINT8, STBIR_FILTER_DEFAULT, STBIR_FILTER_DEFAULT,
		STBIR_EDGE_CLAMP, STBIR_EDGE_CLAMP, STBIR_COLORSPACE_SRGB);
}

STBIRDEF int stbir_resize_uint8_srgb_edgemode(const unsigned char *input_pixels, int input_w, int input_h, int input_stride_in_bytes,
	unsigned char *output_pixels, int output_w, int output_h, int output_stride_in_bytes,
	int num_channels, int alpha_channel, int flags,
	stbir_edge edge_wrap_mode)
{
	return stbir__resize_arbitrary(NULL, input_pixels, input_w, input_h, input_stride_in_bytes,
		output_pixels, output_w, output_h, output_stride_in_bytes,
		0, 0, 1, 1, NULL, num_channels, alpha_channel, flags, STBIR_TYPE_UINT8, STBIR_FILTER_DEFAULT, STBIR_FILTER_DEFAULT,
		edge_wrap_mode, edge_wrap_mode, STBIR_COLORSPACE_SRGB);
}

STBIRDEF int stbir_resize_uint8_generic(const unsigned char *input_pixels, int input_w, int input_h, int input_stride_in_bytes,
	unsigned char *output_pixels, int output_w, int output_h, int output_stride_in_bytes,
	int num_channels, int alpha_channel, int flags,
	stbir_edge edge_wrap_mode, stbir_filter filter, stbir_colorspace space,
	void *alloc_context)
{
	return stbir__resize_arbitrary(alloc_context, input_pixels, input_w, input_h, input_stride_in_bytes,
		output_pixels, output_w, output_h, output_stride_in_bytes,
		0, 0, 1, 1, NULL, num_channels, alpha_channel, flags, STBIR_TYPE_UINT8, filter, filter,
		edge_wrap_mode, edge_wrap_mode, space);
}

STBIRDEF int stbir_resize_uint16_generic(const stbir_uint16 *input_pixels, int input_w, int input_h, int input_stride_in_bytes,
	stbir_uint16 *output_pixels, int output_w, int output_h, int output_stride_in_bytes,
	int num_channels, int alpha_channel, int flags,
	stbir_edge edge_wrap_mode, stbir_filter filter, stbir_colorspace space,
	void *alloc_context)
{
	return stbir__resize_arbitrary(alloc_context, input_pixels, input_w, input_h, input_stride_in_bytes,
		output_pixels, output_w, output_h, output_stride_in_bytes,
		0, 0, 1, 1, NULL, num_channels, alpha_channel, flags, STBIR_TYPE_UINT16, filter, filter,
		edge_wrap_mode, edge_wrap_mode, space);
}


STBIRDEF int stbir_resize_float_generic(const float *input_pixels, int input_w, int input_h, int input_stride_in_bytes,
	float *output_pixels, int output_w, int output_h, int output_stride_in_bytes,
	int num_channels, int alpha_channel, int flags,
	stbir_edge edge_wrap_mode, stbir_filter filter, stbir_colorspace space,
	void *alloc_context)
{
	return stbir__resize_arbitrary(alloc_context, input_pixels, input_w, input_h, input_stride_in_bytes,
		output_pixels, output_w, output_h, output_stride_in_bytes,
		0, 0, 1, 1, NULL, num_channels, alpha_channel, flags, STBIR_TYPE_FLOAT, filter, filter,
		edge_wrap_mode, edge_wrap_mode, space);
}


STBIRDEF int stbir_resize(const void *input_pixels, int input_w, int input_h, int input_stride_in_bytes,
	void *output_pixels, int output_w, int output_h, int output_stride_in_bytes,
	stbir_datatype datatype,
	int num_channels, int alpha_channel, int flags,
	stbir_edge edge_mode_horizontal, stbir_edge edge_mode_vertical,
	stbir_filter filter_horizontal, stbir_filter filter_vertical,
	stbir_colorspace space, void *alloc_context)
{
	return stbir__resize_arbitrary(alloc_context, input_pixels, input_w, input_h, input_stride_in_bytes,
		output_pixels, output_w, output_h, output_stride_in_bytes,
		0, 0, 1, 1, NULL, num_channels, alpha_channel, flags, datatype, filter_horizontal, filter_vertical,
		edge_mode_horizontal, edge_mode_vertical, space);
}


STBIRDEF int stbir_resize_subpixel(const void *input_pixels, int input_w, int input_h, int input_stride_in_bytes,
	void *output_pixels, int output_w, int output_h, int output_stride_in_bytes,
	stbir_datatype datatype,
	int num_channels, int alpha_channel, int flags,
	stbir_edge edge_mode_horizontal, stbir_edge edge_mode_vertical,
	stbir_filter filter_horizontal, stbir_filter filter_vertical,
	stbir_colorspace space, void *alloc_context,
	float x_scale, float y_scale,
	float x_offset, float y_offset)
{
	float transform[4];
	transform[0] = x_scale;
	transform[1] = y_scale;
	transform[2] = x_offset;
	transform[3] = y_offset;
	return stbir__resize_arbitrary(alloc_context, input_pixels, input_w, input_h, input_stride_in_bytes,
		output_pixels, output_w, output_h, output_stride_in_bytes,
		0, 0, 1, 1, transform, num_channels, alpha_channel, flags, datatype, filter_horizontal, filter_vertical,
		edge_mode_horizontal, edge_mode_vertical, space);
}

STBIRDEF int stbir_resize_region(const void *input_pixels, int input_w, int input_h, int input_stride_in_bytes,
	void *output_pixels, int output_w, int output_h, int output_stride_in_bytes,
	stbir_datatype datatype,
	int num_channels, int alpha_channel, int flags,
	stbir_edge edge_mode_horizontal, stbir_edge edge_mode_vertical,
	stbir_filter filter_horizontal, stbir_filter filter_vertical,
	stbir_colorspace space, void *alloc_context,
	float s0, float t0, float s1, float t1)
{
	return stbir__resize_arbitrary(alloc_context, input_pixels, input_w, input_h, input_stride_in_bytes,
		output_pixels, output_w, output_h, output_stride_in_bytes,
		s0, t0, s1, t1, NULL, num_channels, alpha_channel, flags, datatype, filter_horizontal, filter_vertical,
		edge_mode_horizontal, edge_mode_vertical, space);
}

#endif // STB_IMAGE_RESIZE_IMPLEMENTATION

/*
------------------------------------------------------------------------------
This software is available under 2 licenses -- choose whichever you prefer.
------------------------------------------------------------------------------
ALTERNATIVE A - MIT License
Copyright (c) 2017 Sean Barrett
Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files (the "Software"), to deal in
the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
of the Software, and to permit persons to whom the Software is furnished to do
so, subject to the following conditions:
The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.
THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
------------------------------------------------------------------------------
ALTERNATIVE B - Public Domain (www.unlicense.org)
This is free and unencumbered software released into the public domain.
Anyone is free to copy, modify, publish, use, compile, sell, or distribute this
software, either in source code form or as a compiled binary, for any purpose,
commercial or non-commercial, and by any means.
In jurisdictions that recognize copyright laws, the author or authors of this
software dedicate any and all copyright interest in the software to the public
domain. We make this dedication for the benefit of the public at large and to
the detriment of our heirs and successors. We intend this dedication to be an
overt act of relinquishment in perpetuity of all present and future rights to
this software under copyright law.
THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
------------------------------------------------------------------------------
*/


// stb_rect_pack.h - v1.00 - public domain - rectangle packing
// Sean Barrett 2014
//
// Useful for e.g. packing rectangular textures into an atlas.
// Does not do rotation.
//
// Not necessarily the awesomest packing method, but better than
// the totally naive one in stb_truetype (which is primarily what
// this is meant to replace).
//
// Has only had a few tests run, may have issues.
//
// More docs to come.
//
// No memory allocations; uses qsort() and assert() from stdlib.
// Can override those by defining STBRP_SORT and STBRP_ASSERT.
//
// This library currently uses the Skyline Bottom-Left algorithm.
//
// Please note: better rectangle packers are welcome! Please
// implement them to the same API, but with a different init
// function.
//
// Credits
//
//  Library
//    Sean Barrett
//  Minor features
//    Martins Mozeiko
//    github:IntellectualKitty
//    
//  Bugfixes / warning fixes
//    Jeremy Jaussaud
//    Fabian Giesen
//
// Version history:
//
//     1.00  (2019-02-25)  avoid small space waste; gracefully fail too-wide rectangles
//     0.99  (2019-02-07)  warning fixes
//     0.11  (2017-03-03)  return packing success/fail result
//     0.10  (2016-10-25)  remove cast-away-const to avoid warnings
//     0.09  (2016-08-27)  fix compiler warnings
//     0.08  (2015-09-13)  really fix bug with empty rects (w=0 or h=0)
//     0.07  (2015-09-13)  fix bug with empty rects (w=0 or h=0)
//     0.06  (2015-04-15)  added STBRP_SORT to allow replacing qsort
//     0.05:  added STBRP_ASSERT to allow replacing assert
//     0.04:  fixed minor bug in STBRP_LARGE_RECTS support
//     0.01:  initial release
//
// LICENSE
//
//   See end of file for license information.

//////////////////////////////////////////////////////////////////////////////
//
//       INCLUDE SECTION
//

#ifndef STB_INCLUDE_STB_RECT_PACK_H
#define STB_INCLUDE_STB_RECT_PACK_H

#define STB_RECT_PACK_VERSION  1

#ifdef STBRP_STATIC
#define STBRP_DEF static
#else
#define STBRP_DEF extern
#endif

#ifdef __cplusplus
extern "C" {
#endif

	typedef struct stbrp_context stbrp_context;
	typedef struct stbrp_node    stbrp_node;
	typedef struct stbrp_rect    stbrp_rect;

#ifdef STBRP_LARGE_RECTS
	typedef int            stbrp_coord;
#else
	typedef unsigned short stbrp_coord;
#endif

	STBRP_DEF int stbrp_pack_rects(stbrp_context *context, stbrp_rect *rects, int num_rects);
	// Assign packed locations to rectangles. The rectangles are of type
	// 'stbrp_rect' defined below, stored in the array 'rects', and there
	// are 'num_rects' many of them.
	//
	// Rectangles which are successfully packed have the 'was_packed' flag
	// set to a non-zero value and 'x' and 'y' store the minimum location
	// on each axis (i.e. bottom-left in cartesian coordinates, top-left
	// if you imagine y increasing downwards). Rectangles which do not fit
	// have the 'was_packed' flag set to 0.
	//
	// You should not try to access the 'rects' array from another thread
	// while this function is running, as the function temporarily reorders
	// the array while it executes.
	//
	// To pack into another rectangle, you need to call stbrp_init_target
	// again. To continue packing into the same rectangle, you can call
	// this function again. Calling this multiple times with multiple rect
	// arrays will probably produce worse packing results than calling it
	// a single time with the full rectangle array, but the option is
	// available.
	//
	// The function returns 1 if all of the rectangles were successfully
	// packed and 0 otherwise.

	struct stbrp_rect
	{
		// reserved for your use:
		int            id;

		// input:
		stbrp_coord    w, h;

		// output:
		stbrp_coord    x, y;
		int            was_packed;  // non-zero if valid packing

	}; // 16 bytes, nominally


	STBRP_DEF void stbrp_init_target(stbrp_context *context, int width, int height, stbrp_node *nodes, int num_nodes);
	// Initialize a rectangle packer to:
	//    pack a rectangle that is 'width' by 'height' in dimensions
	//    using temporary storage provided by the array 'nodes', which is 'num_nodes' long
	//
	// You must call this function every time you start packing into a new target.
	//
	// There is no "shutdown" function. The 'nodes' memory must stay valid for
	// the following stbrp_pack_rects() call (or calls), but can be freed after
	// the call (or calls) finish.
	//
	// Note: to guarantee best results, either:
	//       1. make sure 'num_nodes' >= 'width'
	//   or  2. call stbrp_allow_out_of_mem() defined below with 'allow_out_of_mem = 1'
	//
	// If you don't do either of the above things, widths will be quantized to multiples
	// of small integers to guarantee the algorithm doesn't run out of temporary storage.
	//
	// If you do #2, then the non-quantized algorithm will be used, but the algorithm
	// may run out of temporary storage and be unable to pack some rectangles.

	STBRP_DEF void stbrp_setup_allow_out_of_mem(stbrp_context *context, int allow_out_of_mem);
	// Optionally call this function after init but before doing any packing to
	// change the handling of the out-of-temp-memory scenario, described above.
	// If you call init again, this will be reset to the default (false).


	STBRP_DEF void stbrp_setup_heuristic(stbrp_context *context, int heuristic);
	// Optionally select which packing heuristic the library should use. Different
	// heuristics will produce better/worse results for different data sets.
	// If you call init again, this will be reset to the default.

	enum
	{
		STBRP_HEURISTIC_Skyline_default = 0,
		STBRP_HEURISTIC_Skyline_BL_sortHeight = STBRP_HEURISTIC_Skyline_default,
		STBRP_HEURISTIC_Skyline_BF_sortHeight
	};


	//////////////////////////////////////////////////////////////////////////////
	//
	// the details of the following structures don't matter to you, but they must
	// be visible so you can handle the memory allocations for them

	struct stbrp_node
	{
		stbrp_coord  x, y;
		stbrp_node  *next;
	};

	struct stbrp_context
	{
		int width;
		int height;
		int align;
		int init_mode;
		int heuristic;
		int num_nodes;
		stbrp_node *active_head;
		stbrp_node *free_head;
		stbrp_node extra[2]; // we allocate two extra nodes so optimal user-node-count is 'width' not 'width+2'
	};

#ifdef __cplusplus
}
#endif

#endif

//////////////////////////////////////////////////////////////////////////////
//
//     IMPLEMENTATION SECTION
//

#ifdef STB_RECT_PACK_IMPLEMENTATION
#ifndef STBRP_SORT
#include <stdlib.h>
#define STBRP_SORT qsort
#endif

#ifndef STBRP_ASSERT
#include <assert.h>
#define STBRP_ASSERT assert
#endif

#ifdef _MSC_VER
#define STBRP__NOTUSED(v)  (void)(v)
#else
#define STBRP__NOTUSED(v)  (void)sizeof(v)
#endif

enum
{
	STBRP__INIT_skyline = 1
};

STBRP_DEF void stbrp_setup_heuristic(stbrp_context *context, int heuristic)
{
	switch (context->init_mode) {
	case STBRP__INIT_skyline:
		STBRP_ASSERT(heuristic == STBRP_HEURISTIC_Skyline_BL_sortHeight || heuristic == STBRP_HEURISTIC_Skyline_BF_sortHeight);
		context->heuristic = heuristic;
		break;
	default:
		STBRP_ASSERT(0);
	}
}

STBRP_DEF void stbrp_setup_allow_out_of_mem(stbrp_context *context, int allow_out_of_mem)
{
	if (allow_out_of_mem)
		// if it's ok to run out of memory, then don't bother aligning them;
		// this gives better packing, but may fail due to OOM (even though
		// the rectangles easily fit). @TODO a smarter approach would be to only
		// quantize once we've hit OOM, then we could get rid of this parameter.
		context->align = 1;
	else {
		// if it's not ok to run out of memory, then quantize the widths
		// so that num_nodes is always enough nodes.
		//
		// I.e. num_nodes * align >= width
		//                  align >= width / num_nodes
		//                  align = ceil(width/num_nodes)

		context->align = (context->width + context->num_nodes - 1) / context->num_nodes;
	}
}

STBRP_DEF void stbrp_init_target(stbrp_context *context, int width, int height, stbrp_node *nodes, int num_nodes)
{
	int i;
#ifndef STBRP_LARGE_RECTS
	STBRP_ASSERT(width <= 0xffff && height <= 0xffff);
#endif

	for (i = 0; i < num_nodes - 1; ++i)
		nodes[i].next = &nodes[i + 1];
	nodes[i].next = NULL;
	context->init_mode = STBRP__INIT_skyline;
	context->heuristic = STBRP_HEURISTIC_Skyline_default;
	context->free_head = &nodes[0];
	context->active_head = &context->extra[0];
	context->width = width;
	context->height = height;
	context->num_nodes = num_nodes;
	stbrp_setup_allow_out_of_mem(context, 0);

	// node 0 is the full width, node 1 is the sentinel (lets us not store width explicitly)
	context->extra[0].x = 0;
	context->extra[0].y = 0;
	context->extra[0].next = &context->extra[1];
	context->extra[1].x = (stbrp_coord)width;
#ifdef STBRP_LARGE_RECTS
	context->extra[1].y = (1 << 30);
#else
	context->extra[1].y = 65535;
#endif
	context->extra[1].next = NULL;
}

// find minimum y position if it starts at x1
static int stbrp__skyline_find_min_y(stbrp_context *c, stbrp_node *first, int x0, int width, int *pwaste)
{
	stbrp_node *node = first;
	int x1 = x0 + width;
	int min_y, visited_width, waste_area;

	STBRP__NOTUSED(c);

	STBRP_ASSERT(first->x <= x0);

#if 0
	// flags in case we're past the node
	while (node->next->x <= x0)
		++node;
#else
	STBRP_ASSERT(node->next->x > x0); // we ended up handling this in the caller for efficiency
#endif

	STBRP_ASSERT(node->x <= x0);

	min_y = 0;
	waste_area = 0;
	visited_width = 0;
	while (node->x < x1) {
		if (node->y > min_y) {
			// raise min_y higher.
			// we've accounted for all waste up to min_y,
			// but we'll now add more waste for everything we've visted
			waste_area += visited_width * (node->y - min_y);
			min_y = node->y;
			// the first time through, visited_width might be reduced
			if (node->x < x0)
				visited_width += node->next->x - x0;
			else
				visited_width += node->next->x - node->x;
		}
		else {
			// add waste area
			int under_width = node->next->x - node->x;
			if (under_width + visited_width > width)
				under_width = width - visited_width;
			waste_area += under_width * (min_y - node->y);
			visited_width += under_width;
		}
		node = node->next;
	}

	*pwaste = waste_area;
	return min_y;
}

typedef struct
{
	int x, y;
	stbrp_node **prev_link;
} stbrp__findresult;

static stbrp__findresult stbrp__skyline_find_best_pos(stbrp_context *c, int width, int height)
{
	int best_waste = (1 << 30), best_x, best_y = (1 << 30);
	stbrp__findresult fr;
	stbrp_node **prev, *node, *tail, **best = NULL;

	// align to multiple of c->align
	width = (width + c->align - 1);
	width -= width % c->align;
	STBRP_ASSERT(width % c->align == 0);

	// if it can't possibly fit, bail immediately
	if (width > c->width || height > c->height) {
		fr.prev_link = NULL;
		fr.x = fr.y = 0;
		return fr;
	}

	node = c->active_head;
	prev = &c->active_head;
	while (node->x + width <= c->width) {
		int y, waste;
		y = stbrp__skyline_find_min_y(c, node, node->x, width, &waste);
		if (c->heuristic == STBRP_HEURISTIC_Skyline_BL_sortHeight) { // actually just want to test BL
		   // bottom left
			if (y < best_y) {
				best_y = y;
				best = prev;
			}
		}
		else {
			// best-fit
			if (y + height <= c->height) {
				// can only use it if it first vertically
				if (y < best_y || (y == best_y && waste < best_waste)) {
					best_y = y;
					best_waste = waste;
					best = prev;
				}
			}
		}
		prev = &node->next;
		node = node->next;
	}

	best_x = (best == NULL) ? 0 : (*best)->x;

	// if doing best-fit (BF), we also have to try aligning right edge to each node position
	//
	// e.g, if fitting
	//
	//     ____________________
	//    |____________________|
	//
	//            into
	//
	//   |                         |
	//   |             ____________|
	//   |____________|
	//
	// then right-aligned reduces waste, but bottom-left BL is always chooses left-aligned
	//
	// This makes BF take about 2x the time

	if (c->heuristic == STBRP_HEURISTIC_Skyline_BF_sortHeight) {
		tail = c->active_head;
		node = c->active_head;
		prev = &c->active_head;
		// find first node that's admissible
		while (tail->x < width)
			tail = tail->next;
		while (tail) {
			int xpos = tail->x - width;
			int y, waste;
			STBRP_ASSERT(xpos >= 0);
			// find the left position that matches this
			while (node->next->x <= xpos) {
				prev = &node->next;
				node = node->next;
			}
			STBRP_ASSERT(node->next->x > xpos && node->x <= xpos);
			y = stbrp__skyline_find_min_y(c, node, xpos, width, &waste);
			if (y + height <= c->height) {
				if (y <= best_y) {
					if (y < best_y || waste < best_waste || (waste == best_waste && xpos < best_x)) {
						best_x = xpos;
						STBRP_ASSERT(y <= best_y);
						best_y = y;
						best_waste = waste;
						best = prev;
					}
				}
			}
			tail = tail->next;
		}
	}

	fr.prev_link = best;
	fr.x = best_x;
	fr.y = best_y;
	return fr;
}

static stbrp__findresult stbrp__skyline_pack_rectangle(stbrp_context *context, int width, int height)
{
	// find best position according to heuristic
	stbrp__findresult res = stbrp__skyline_find_best_pos(context, width, height);
	stbrp_node *node, *cur;

	// bail if:
	//    1. it failed
	//    2. the best node doesn't fit (we don't always check this)
	//    3. we're out of memory
	if (res.prev_link == NULL || res.y + height > context->height || context->free_head == NULL) {
		res.prev_link = NULL;
		return res;
	}

	// on success, create new node
	node = context->free_head;
	node->x = (stbrp_coord)res.x;
	node->y = (stbrp_coord)(res.y + height);

	context->free_head = node->next;

	// insert the new node into the right starting point, and
	// let 'cur' point to the remaining nodes needing to be
	// stiched back in

	cur = *res.prev_link;
	if (cur->x < res.x) {
		// preserve the existing one, so start testing with the next one
		stbrp_node *next = cur->next;
		cur->next = node;
		cur = next;
	}
	else {
		*res.prev_link = node;
	}

	// from here, traverse cur and free the nodes, until we get to one
	// that shouldn't be freed
	while (cur->next && cur->next->x <= res.x + width) {
		stbrp_node *next = cur->next;
		// move the current node to the free list
		cur->next = context->free_head;
		context->free_head = cur;
		cur = next;
	}

	// stitch the list back in
	node->next = cur;

	if (cur->x < res.x + width)
		cur->x = (stbrp_coord)(res.x + width);

#ifdef _DEBUG
	cur = context->active_head;
	while (cur->x < context->width) {
		STBRP_ASSERT(cur->x < cur->next->x);
		cur = cur->next;
	}
	STBRP_ASSERT(cur->next == NULL);

	{
		int count = 0;
		cur = context->active_head;
		while (cur) {
			cur = cur->next;
			++count;
		}
		cur = context->free_head;
		while (cur) {
			cur = cur->next;
			++count;
		}
		STBRP_ASSERT(count == context->num_nodes + 2);
	}
#endif

	return res;
}

static int rect_height_compare(const void *a, const void *b)
{
	const stbrp_rect *p = (const stbrp_rect *)a;
	const stbrp_rect *q = (const stbrp_rect *)b;
	if (p->h > q->h)
		return -1;
	if (p->h < q->h)
		return  1;
	return (p->w > q->w) ? -1 : (p->w < q->w);
}

static int rect_original_order(const void *a, const void *b)
{
	const stbrp_rect *p = (const stbrp_rect *)a;
	const stbrp_rect *q = (const stbrp_rect *)b;
	return (p->was_packed < q->was_packed) ? -1 : (p->was_packed > q->was_packed);
}

#ifdef STBRP_LARGE_RECTS
#define STBRP__MAXVAL  0xffffffff
#else
#define STBRP__MAXVAL  0xffff
#endif

STBRP_DEF int stbrp_pack_rects(stbrp_context *context, stbrp_rect *rects, int num_rects)
{
	int i, all_rects_packed = 1;

	// we use the 'was_packed' field internally to allow sorting/unsorting
	for (i = 0; i < num_rects; ++i) {
		rects[i].was_packed = i;
	}

	// sort according to heuristic
	STBRP_SORT(rects, num_rects, sizeof(rects[0]), rect_height_compare);

	for (i = 0; i < num_rects; ++i) {
		if (rects[i].w == 0 || rects[i].h == 0) {
			rects[i].x = rects[i].y = 0;  // empty rect needs no space
		}
		else {
			stbrp__findresult fr = stbrp__skyline_pack_rectangle(context, rects[i].w, rects[i].h);
			if (fr.prev_link) {
				rects[i].x = (stbrp_coord)fr.x;
				rects[i].y = (stbrp_coord)fr.y;
			}
			else {
				rects[i].x = rects[i].y = STBRP__MAXVAL;
			}
		}
	}

	// unsort
	STBRP_SORT(rects, num_rects, sizeof(rects[0]), rect_original_order);

	// set was_packed state_flags and all_rects_packed status
	for (i = 0; i < num_rects; ++i) {
		rects[i].was_packed = !(rects[i].x == STBRP__MAXVAL && rects[i].y == STBRP__MAXVAL);
		if (!rects[i].was_packed)
			all_rects_packed = 0;
	}

	// return the all_rects_packed status
	return all_rects_packed;
}
#endif

/*
ALTERNATIVE A - MIT License
Copyright (c) 2017 Sean Barrett
Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files (the "Software"), to deal in
the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
of the Software, and to permit persons to whom the Software is furnished to do
so, subject to the following conditions:
The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.
THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

/* stb_image - v2.25 - public domain image loader - http://nothings.org/stb
                                  no warranty implied; use at your own risk

   Do this:
      #define STB_IMAGE_IMPLEMENTATION
   before you include this file in *one* C or C++ file to create the implementation.

   // i.e. it should look like this:
   #include ...
   #include ...
   #include ...
   #define STB_IMAGE_IMPLEMENTATION
   #include "stb_image.h"

   You can #define STBI_ASSERT(x) before the #include to avoid using assert.h.
   And #define STBI_MALLOC, STBI_REALLOC, and STBI_FREE to avoid using malloc,realloc,free


   QUICK NOTES:
      Primarily of interest to game developers and other people who can
          avoid problematic images and only need the trivial interface

      JPEG baseline & progressive (12 bpc/arithmetic not supported, same as stock IJG lib)
      PNG 1/2/4/8/16-bit-per-channel

      TGA (not sure what subset, if a subset)
      BMP non-1bpp, non-RLE
      PSD (composited view only, no extra channels, 8/16 bit-per-channel)

      GIF (*comp always reports as 4-channel)
      HDR (radiance rgbE format)
      PIC (Softimage PIC)
      PNM (PPM and PGM binary only)

      Animated GIF still needs a proper API, but here's one way to do it:
          http://gist.github.com/urraka/685d9a6340b26b830d49

      - decode from memory or through FILE (define STBI_NO_STDIO to remove code)
      - decode from arbitrary I/O callbacks
      - SIMD acceleration on x86/x64 (SSE2) and ARM (NEON)

   Full documentation under "DOCUMENTATION" below.


LICENSE

  See end of file for license information.

RECENT REVISION HISTORY:

      2.25  (2020-02-02) fix warnings
      2.24  (2020-02-02) fix warnings; thread-local failure_reason and flip_vertically
      2.23  (2019-08-11) fix clang static analysis warning
      2.22  (2019-03-04) gif fixes, fix warnings
      2.21  (2019-02-25) fix typo in comment
      2.20  (2019-02-07) support utf8 filenames in Windows; fix warnings and platform ifdefs
      2.19  (2018-02-11) fix warning
      2.18  (2018-01-30) fix warnings
      2.17  (2018-01-29) bugfix, 1-bit BMP, 16-bitness query, fix warnings
      2.16  (2017-07-23) all functions have 16-bit variants; optimizations; bugfixes
      2.15  (2017-03-18) fix png-1,2,4; all Imagenet JPGs; no runtime SSE detection on GCC
      2.14  (2017-03-03) remove deprecated STBI_JPEG_OLD; fixes for Imagenet JPGs
      2.13  (2016-12-04) experimental 16-bit API, only for PNG so far; fixes
      2.12  (2016-04-02) fix typo in 2.11 PSD fix that caused crashes
      2.11  (2016-04-02) 16-bit PNGS; enable SSE2 in non-gcc x64
                         RGB-format JPEG; remove white matting in PSD;
                         allocate large structures on the stack;
                         correct channel count for PNG & BMP
      2.10  (2016-01-22) avoid warning introduced in 2.09
      2.09  (2016-01-16) 16-bit TGA; comments in PNM files; STBI_REALLOC_SIZED

   See end of file for full revision history.


 ============================    Contributors    =========================

 Image formats                          Extensions, features
    Sean Barrett (jpeg, png, bmp)          Jetro Lauha (stbi_info)
    Nicolas Schulz (hdr, psd)              Martin "SpartanJ" Golini (stbi_info)
    Jonathan Dummer (tga)                  James "moose2000" Brown (iPhone PNG)
    Jean-Marc Lienher (gif)                Ben "Disch" Wenger (io callbacks)
    Tom Seddon (pic)                       Omar Cornut (1/2/4-bit PNG)
    Thatcher Ulrich (psd)                  Nicolas Guillemot (vertical flip)
    Ken Miller (pgm, ppm)                  Richard Mitton (16-bit PSD)
    github:urraka (animated gif)           Junggon Kim (PNM comments)
    Christopher Forseth (animated gif)     Daniel Gibson (16-bit TGA)
                                           socks-the-fox (16-bit PNG)
                                           Jeremy Sawicki (handle all ImageNet JPGs)
 Optimizations & bugfixes                  Mikhail Morozov (1-bit BMP)
    Fabian "ryg" Giesen                    Anael Seghezzi (is-16-bit query)
    Arseny Kapoulkine
    John-Mark Allen
    Carmelo J Fdez-Aguera

 Bug & warning fixes
    Marc LeBlanc            David Woo          Guillaume George   Martins Mozeiko
    Christpher Lloyd        Jerry Jansson      Joseph Thomson     Phil Jordan
    Dave Moore              Roy Eltham         Hayaki Saito       Nathan Reed
    Won Chun                Luke Graham        Johan Duparc       Nick Verigakis
    the Horde3D community   Thomas Ruf         Ronny Chevalier    github:rlyeh
    Janez Zemva             John Bartholomew   Michal Cichon      github:romigrou
    Jonathan Blow           Ken Hamada         Tero Hanninen      github:svdijk
    Laurent Gomila          Cort Stratton      Sergio Gonzalez    github:snagar
    Aruelien Pocheville     Thibault Reuille   Cass Everitt       github:Zelex
    Ryamond Barbiero        Paul Du Bois       Engin Manap        github:grim210
    Aldo Culquicondor       Philipp Wiesemann  Dale Weiler        github:sammyhw
    Oriol Ferrer Mesia      Josh Tobin         Matthew Gregan     github:phprus
    Julian Raschke          Gregory Mullen     Baldur Karlsson    github:poppolopoppo
    Christian Floisand      Kevin Schmidt      JR Smith           github:darealshinji
    Brad Weinberger         Matvey Cherevko                       github:Michaelangel007
    Blazej Dariusz Roszkowski                  Alexander Veselov
*/

#ifndef STBI_INCLUDE_STB_IMAGE_H
#define STBI_INCLUDE_STB_IMAGE_H

// DOCUMENTATION
//
// Limitations:
//    - no 12-bit-per-channel JPEG
//    - no JPEGs with arithmetic coding
//    - GIF always returns *comp=4
//
// Basic usage (see HDR discussion below for HDR usage):
//    int x,y,n;
//    unsigned char *data = stbi_load(filename, &x, &y, &n, 0);
//    // ... process data if not NULL ...
//    // ... x = width, y = height, n = # 8-bit components per pixel ...
//    // ... replace '0' with '1'..'4' to force that many components per pixel
//    // ... but 'n' will always be the number that it would have been if you said 0
//    stbi_image_free(data)
//
// Standard parameters:
//    int *x                 -- outputs image width in pixels
//    int *y                 -- outputs image height in pixels
//    int *channels_in_file  -- outputs # of image components in image file
//    int desired_channels   -- if non-zero, # of image components requested in result
//
// The return value from an image loader is an 'unsigned char *' which points
// to the pixel data, or NULL on an allocation failure or if the image is
// corrupt or invalid. The pixel data consists of *y scanlines of *x pixels,
// with each pixel consisting of N interleaved 8-bit components; the first
// pixel pointed to is top-left-most in the image. There is no padding between
// image scanlines or between pixels, regardless of format. The number of
// components N is 'desired_channels' if desired_channels is non-zero, or
// *channels_in_file otherwise. If desired_channels is non-zero,
// *channels_in_file has the number of components that _would_ have been
// output otherwise. E.g. if you set desired_channels to 4, you will always
// get RGBA output, but you can check *channels_in_file to see if it's trivially
// opaque because e.g. there were only 3 channels in the source image.
//
// An output image with N components has the following components interleaved
// in this order in each pixel:
//
//     N=#comp     components
//       1           grey
//       2           grey, alpha
//       3           red, green, blue
//       4           red, green, blue, alpha
//
// If image loading fails for any reason, the return value will be NULL,
// and *x, *y, *channels_in_file will be unchanged. The function
// stbi_failure_reason() can be queried for an extremely brief, end-user
// unfriendly explanation of why the load failed. Define STBI_NO_FAILURE_STRINGS
// to avoid compiling these strings at all, and STBI_FAILURE_USERMSG to get slightly
// more user-friendly ones.
//
// Paletted PNG, BMP, GIF, and PIC images are automatically depalettized.
//
// ===========================================================================
//
// UNICODE:
//
//   If compiling for Windows and you wish to use Unicode filenames, compile
//   with
//       #define STBI_WINDOWS_UTF8
//   and pass utf8-encoded filenames. Call stbi_convert_wchar_to_utf8 to convert
//   Windows wchar_t filenames to utf8.
//
// ===========================================================================
//
// Philosophy
//
// stb libraries are designed with the following priorities:
//
//    1. easy to use
//    2. easy to maintain
//    3. good performance
//
// Sometimes I let "good performance" creep up in priority over "easy to maintain",
// and for best performance I may provide less-easy-to-use APIs that give higher
// performance, in addition to the easy-to-use ones. Nevertheless, it's important
// to keep in mind that from the standpoint of you, a client of this library,
// all you care about is #1 and #3, and stb libraries DO NOT emphasize #3 above all.
//
// Some secondary priorities arise directly from the first two, some of which
// provide more explicit reasons why performance can't be emphasized.
//
//    - Portable ("ease of use")
//    - Small source code footprint ("easy to maintain")
//    - No dependencies ("ease of use")
//
// ===========================================================================
//
// I/O callbacks
//
// I/O callbacks allow you to read from arbitrary sources, like packaged
// files or some other source. Data read from callbacks are processed
// through a small internal buffer (currently 128 bytes) to try to reduce
// overhead.
//
// The three functions you must define are "read" (reads some bytes of data),
// "flags" (skips some bytes of data), "eof" (reports if the stream is at the end).
//
// ===========================================================================
//
// SIMD support
//
// The JPEG decoder will try to automatically use SIMD kernels on x86 when
// supported by the compiler. For ARM Neon support, you must explicitly
// request it.
//
// (The old do-it-yourself SIMD API is no longer supported in the current
// code.)
//
// On x86, SSE2 will automatically be used when available based on a run-time
// test; if not, the generic C versions are used as a fall-back. On ARM targets,
// the typical path is to have separate builds for NEON and non-NEON devices
// (at least this is true for iOS and Android). Therefore, the NEON support is
// toggled by a build flag: define STBI_NEON to get NEON loops.
//
// If for some reason you do not want to use any of SIMD code, or if
// you have issues compiling it, you can disable it entirely by
// defining STBI_NO_SIMD.
//
// ===========================================================================
//
// HDR image support   (disable by defining STBI_NO_HDR)
//
// stb_image supports loading HDR images in general, and currently the Radiance
// .HDR file format specifically. You can still load any file through the existing
// interface; if you attempt to load an HDR file, it will be automatically remapped
// to LDR, assuming gamma 2.2 and an arbitrary scale factor defaulting to 1;
// both of these constants can be reconfigured through this interface:
//
//     stbi_hdr_to_ldr_gamma(2.2f);
//     stbi_hdr_to_ldr_scale(1.0f);
//
// (note, do not use _inverse_ constants; stbi_image will invert them
// appropriately).
//
// Additionally, there is a new, parallel interface for loading files as
// (linear) floats to preserve the full dynamic range:
//
//    float *data = stbi_loadf(filename, &x, &y, &n, 0);
//
// If you load LDR images through this interface, those images will
// be promoted to floating point values, run through the inverse of
// constants corresponding to the above:
//
//     stbi_ldr_to_hdr_scale(1.0f);
//     stbi_ldr_to_hdr_gamma(2.2f);
//
// Finally, given a filename (or an open file or memory block--see header
// file for details) containing image data, you can query for the "most
// appropriate" interface to use (that is, whether the image is HDR or
// not), using:
//
//     stbi_is_hdr(char *filename);
//
// ===========================================================================
//
// iPhone PNG support:
//
// By default we convert iphone-formatted PNGs back to RGB, even though
// they are internally encoded differently. You can disable this conversion
// by calling stbi_convert_iphone_png_to_rgb(0), in which case
// you will always just get the native iphone "format" through (which
// is BGR stored in RGB).
//
// Call stbi_set_unpremultiply_on_load(1) as well to force a divide per
// pixel to remove any premultiplied alpha *only* if the image file explicitly
// says there's premultiplied data (currently only happens in iPhone images,
// and only if iPhone convert-to-rgb processing is on).
//
// ===========================================================================
//
// ADDITIONAL CONFIGURATION
//
//  - You can suppress implementation of any of the decoders to reduce
//    your code footprint by #defining one or more of the following
//    symbols before creating the implementation.
//
//        STBI_NO_JPEG
//        STBI_NO_PNG
//        STBI_NO_BMP
//        STBI_NO_PSD
//        STBI_NO_TGA
//        STBI_NO_GIF
//        STBI_NO_HDR
//        STBI_NO_PIC
//        STBI_NO_PNM   (.ppm and .pgm)
//
//  - You can request *only* certain decoders and suppress all other ones
//    (this will be more forward-compatible, as addition of new decoders
//    doesn't require you to disable them explicitly):
//
//        STBI_ONLY_JPEG
//        STBI_ONLY_PNG
//        STBI_ONLY_BMP
//        STBI_ONLY_PSD
//        STBI_ONLY_TGA
//        STBI_ONLY_GIF
//        STBI_ONLY_HDR
//        STBI_ONLY_PIC
//        STBI_ONLY_PNM   (.ppm and .pgm)
//
//   - If you use STBI_NO_PNG (or _ONLY_ without PNG), and you still
//     want the zlib decoder to be available, #define STBI_SUPPORT_ZLIB
//


#ifndef STBI_NO_STDIO
#include <stdio.h>
#endif // STBI_NO_STDIO

#define STBI_VERSION 1

enum
{
   STBI_default = 0, // only used for desired_channels

   STBI_grey       = 1,
   STBI_grey_alpha = 2,
   STBI_rgb        = 3,
   STBI_rgb_alpha  = 4
};

#include <stdlib.h>
typedef unsigned char stbi_uc;
typedef unsigned short stbi_us;

#ifdef __cplusplus
extern "C" {
#endif

#ifndef STBIDEF
#ifdef STB_IMAGE_STATIC
#define STBIDEF static
#else
#define STBIDEF extern
#endif
#endif

//////////////////////////////////////////////////////////////////////////////
//
// PRIMARY API - works on images of any type
//

//
// load image by filename, open file, or memory buffer
//

typedef struct
{
   int      (*read)  (void *user,char *data,int size);   // fill 'data' with 'size' bytes.  return number of bytes actually read
   void     (*flags)  (void *user,int n);                 // flags the next 'n' bytes, or 'unget' the last -n bytes if negative
   int      (*eof)   (void *user);                       // returns nonzero if we are at end of file/data
} stbi_io_callbacks;

////////////////////////////////////
//
// 8-bits-per-channel interface
//

STBIDEF stbi_uc *stbi_load_from_memory   (stbi_uc           const *buffer, int len   , int *x, int *y, int *channels_in_file, int desired_channels);
STBIDEF stbi_uc *stbi_load_from_callbacks(stbi_io_callbacks const *clbk  , void *user, int *x, int *y, int *channels_in_file, int desired_channels);

#ifndef STBI_NO_STDIO
STBIDEF stbi_uc *stbi_load            (char const *filename, int *x, int *y, int *channels_in_file, int desired_channels);
STBIDEF stbi_uc *stbi_load_from_file  (FILE *f, int *x, int *y, int *channels_in_file, int desired_channels);
// for stbi_load_from_file, file pointer is left pointing immediately after image
#endif

#ifndef STBI_NO_GIF
STBIDEF stbi_uc *stbi_load_gif_from_memory(stbi_uc const *buffer, int len, int **delays, int *x, int *y, int *z, int *comp, int req_comp);
#endif

#ifdef STBI_WINDOWS_UTF8
STBIDEF int stbi_convert_wchar_to_utf8(char *buffer, size_t bufferlen, const wchar_t* input);
#endif

////////////////////////////////////
//
// 16-bits-per-channel interface
//

STBIDEF stbi_us *stbi_load_16_from_memory   (stbi_uc const *buffer, int len, int *x, int *y, int *channels_in_file, int desired_channels);
STBIDEF stbi_us *stbi_load_16_from_callbacks(stbi_io_callbacks const *clbk, void *user, int *x, int *y, int *channels_in_file, int desired_channels);

#ifndef STBI_NO_STDIO
STBIDEF stbi_us *stbi_load_16          (char const *filename, int *x, int *y, int *channels_in_file, int desired_channels);
STBIDEF stbi_us *stbi_load_from_file_16(FILE *f, int *x, int *y, int *channels_in_file, int desired_channels);
#endif

////////////////////////////////////
//
// float-per-channel interface
//
#ifndef STBI_NO_LINEAR
   STBIDEF float *stbi_loadf_from_memory     (stbi_uc const *buffer, int len, int *x, int *y, int *channels_in_file, int desired_channels);
   STBIDEF float *stbi_loadf_from_callbacks  (stbi_io_callbacks const *clbk, void *user, int *x, int *y,  int *channels_in_file, int desired_channels);

   #ifndef STBI_NO_STDIO
   STBIDEF float *stbi_loadf            (char const *filename, int *x, int *y, int *channels_in_file, int desired_channels);
   STBIDEF float *stbi_loadf_from_file  (FILE *f, int *x, int *y, int *channels_in_file, int desired_channels);
   #endif
#endif

#ifndef STBI_NO_HDR
   STBIDEF void   stbi_hdr_to_ldr_gamma(float gamma);
   STBIDEF void   stbi_hdr_to_ldr_scale(float scale);
#endif // STBI_NO_HDR

#ifndef STBI_NO_LINEAR
   STBIDEF void   stbi_ldr_to_hdr_gamma(float gamma);
   STBIDEF void   stbi_ldr_to_hdr_scale(float scale);
#endif // STBI_NO_LINEAR

// stbi_is_hdr is always defined, but always returns false if STBI_NO_HDR
STBIDEF int    stbi_is_hdr_from_callbacks(stbi_io_callbacks const *clbk, void *user);
STBIDEF int    stbi_is_hdr_from_memory(stbi_uc const *buffer, int len);
#ifndef STBI_NO_STDIO
STBIDEF int      stbi_is_hdr          (char const *filename);
STBIDEF int      stbi_is_hdr_from_file(FILE *f);
#endif // STBI_NO_STDIO


// get a VERY brief reason for failure
// on most compilers (and ALL modern mainstream compilers) this is threadsafe
STBIDEF const char *stbi_failure_reason  (void);

// free the loaded image -- this is just free()
STBIDEF void     stbi_image_free      (void *retval_from_stbi_load);

// get image dimensions & components without fully decoding
STBIDEF int      stbi_info_from_memory(stbi_uc const *buffer, int len, int *x, int *y, int *comp);
STBIDEF int      stbi_info_from_callbacks(stbi_io_callbacks const *clbk, void *user, int *x, int *y, int *comp);
STBIDEF int      stbi_is_16_bit_from_memory(stbi_uc const *buffer, int len);
STBIDEF int      stbi_is_16_bit_from_callbacks(stbi_io_callbacks const *clbk, void *user);

#ifndef STBI_NO_STDIO
STBIDEF int      stbi_info               (char const *filename,     int *x, int *y, int *comp);
STBIDEF int      stbi_info_from_file     (FILE *f,                  int *x, int *y, int *comp);
STBIDEF int      stbi_is_16_bit          (char const *filename);
STBIDEF int      stbi_is_16_bit_from_file(FILE *f);
#endif



// for image formats that explicitly notate that they have premultiplied alpha,
// we just return the colors as stored in the file. set this flag to force
// unpremultiplication. results are undefined if the unpremultiply overflow.
STBIDEF void stbi_set_unpremultiply_on_load(int flag_true_if_should_unpremultiply);

// indicate whether we should process iphone images back to canonical format,
// or just pass them through "as-is"
STBIDEF void stbi_convert_iphone_png_to_rgb(int flag_true_if_should_convert);

// flip the image vertically, so the first pixel in the output array is the bottom left
STBIDEF void stbi_set_flip_vertically_on_load(int flag_true_if_should_flip);

// as above, but only applies to images loaded on the thread that calls the function
// this function is only available if your compiler supports thread-local variables;
// calling it will fail to link if your compiler doesn't
STBIDEF void stbi_set_flip_vertically_on_load_thread(int flag_true_if_should_flip);

// ZLIB client - used by PNG, available for other purposes

STBIDEF char *stbi_zlib_decode_malloc_guesssize(const char *buffer, int len, int initial_size, int *outlen);
STBIDEF char *stbi_zlib_decode_malloc_guesssize_headerflag(const char *buffer, int len, int initial_size, int *outlen, int parse_header);
STBIDEF char *stbi_zlib_decode_malloc(const char *buffer, int len, int *outlen);
STBIDEF int   stbi_zlib_decode_buffer(char *obuffer, int olen, const char *ibuffer, int ilen);

STBIDEF char *stbi_zlib_decode_noheader_malloc(const char *buffer, int len, int *outlen);
STBIDEF int   stbi_zlib_decode_noheader_buffer(char *obuffer, int olen, const char *ibuffer, int ilen);


#ifdef __cplusplus
}
#endif

//
//
////   end header file   /////////////////////////////////////////////////////
#endif // STBI_INCLUDE_STB_IMAGE_H

#ifdef STB_IMAGE_IMPLEMENTATION

#if defined(STBI_ONLY_JPEG) || defined(STBI_ONLY_PNG) || defined(STBI_ONLY_BMP) \
  || defined(STBI_ONLY_TGA) || defined(STBI_ONLY_GIF) || defined(STBI_ONLY_PSD) \
  || defined(STBI_ONLY_HDR) || defined(STBI_ONLY_PIC) || defined(STBI_ONLY_PNM) \
  || defined(STBI_ONLY_ZLIB)
   #ifndef STBI_ONLY_JPEG
   #define STBI_NO_JPEG
   #endif
   #ifndef STBI_ONLY_PNG
   #define STBI_NO_PNG
   #endif
   #ifndef STBI_ONLY_BMP
   #define STBI_NO_BMP
   #endif
   #ifndef STBI_ONLY_PSD
   #define STBI_NO_PSD
   #endif
   #ifndef STBI_ONLY_TGA
   #define STBI_NO_TGA
   #endif
   #ifndef STBI_ONLY_GIF
   #define STBI_NO_GIF
   #endif
   #ifndef STBI_ONLY_HDR
   #define STBI_NO_HDR
   #endif
   #ifndef STBI_ONLY_PIC
   #define STBI_NO_PIC
   #endif
   #ifndef STBI_ONLY_PNM
   #define STBI_NO_PNM
   #endif
#endif

#if defined(STBI_NO_PNG) && !defined(STBI_SUPPORT_ZLIB) && !defined(STBI_NO_ZLIB)
#define STBI_NO_ZLIB
#endif


#include <stdarg.h>
#include <stddef.h> // ptrdiff_t on osx
#include <stdlib.h>
#include <string.h>
#include <limits.h>

#if !defined(STBI_NO_LINEAR) || !defined(STBI_NO_HDR)
#include <math.h>  // ldexp, pow
#endif

#ifndef STBI_NO_STDIO
#include <stdio.h>
#endif

#ifndef STBI_ASSERT
#include <assert.h>
#define STBI_ASSERT(x) assert(x)
#endif

#ifdef __cplusplus
#define STBI_EXTERN extern "C"
#else
#define STBI_EXTERN extern
#endif


#ifndef _MSC_VER
   #ifdef __cplusplus
   #define stbi_inline inline
   #else
   #define stbi_inline
   #endif
#else
   #define stbi_inline __forceinline
#endif

#ifndef STBI_NO_THREAD_LOCALS
   #if defined(__cplusplus) &&  __cplusplus >= 201103L
      #define STBI_THREAD_LOCAL       thread_local
   #elif defined (__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
      #define STBI_THREAD_LOCAL       _Thread_local
   #elif defined(__GNUC__)
      #define STBI_THREAD_LOCAL       __thread
   #elif defined(_MSC_VER)
      #define STBI_THREAD_LOCAL       __declspec(thread)
#endif
#endif

#ifdef _MSC_VER
typedef unsigned short stbi__uint16;
typedef   signed short stbi__int16;
typedef unsigned int   stbi__uint32;
typedef   signed int   stbi__int32;
#else
#include <stdint.h>
typedef uint16_t stbi__uint16;
typedef int16_t  stbi__int16;
typedef uint32_t stbi__uint32;
typedef int32_t  stbi__int32;
#endif

// should produce compiler error if size is wrong
typedef unsigned char validate_uint32[sizeof(stbi__uint32)==4 ? 1 : -1];

#ifdef _MSC_VER
#define STBI_NOTUSED(v)  (void)(v)
#else
#define STBI_NOTUSED(v)  (void)sizeof(v)
#endif

#ifdef _MSC_VER
#define STBI_HAS_LROTL
#endif

#ifdef STBI_HAS_LROTL
   #define stbi_lrot(x,y)  _lrotl(x,y)
#else
   #define stbi_lrot(x,y)  (((x) << (y)) | ((x) >> (32 - (y))))
#endif

#if defined(STBI_MALLOC) && defined(STBI_FREE) && (defined(STBI_REALLOC) || defined(STBI_REALLOC_SIZED))
// ok
#elif !defined(STBI_MALLOC) && !defined(STBI_FREE) && !defined(STBI_REALLOC) && !defined(STBI_REALLOC_SIZED)
// ok
#else
#error "Must define all or none of STBI_MALLOC, STBI_FREE, and STBI_REALLOC (or STBI_REALLOC_SIZED)."
#endif

#ifndef STBI_MALLOC
#define STBI_MALLOC(sz)           malloc(sz)
#define STBI_REALLOC(p,newsz)     realloc(p,newsz)
#define STBI_FREE(p)              free(p)
#endif

#ifndef STBI_REALLOC_SIZED
#define STBI_REALLOC_SIZED(p,oldsz,newsz) STBI_REALLOC(p,newsz)
#endif

// x86/x64 detection
#if defined(__x86_64__) || defined(_M_X64)
#define STBI__X64_TARGET
#elif defined(__i386) || defined(_M_IX86)
#define STBI__X86_TARGET
#endif

#if defined(__GNUC__) && defined(STBI__X86_TARGET) && !defined(__SSE2__) && !defined(STBI_NO_SIMD)
// gcc doesn't support sse2 intrinsics unless you compile with -msse2,
// which in turn means it gets to use SSE2 everywhere. This is unfortunate,
// but previous attempts to provide the SSE2 functions with runtime
// detection caused numerous issues. The way architecture extensions are
// exposed in GCC/Clang is, sadly, not really suited for one-file libs.
// New behavior: if compiled with -msse2, we use SSE2 without any
// detection; if not, we don't use it at all.
#define STBI_NO_SIMD
#endif

#if defined(__MINGW32__) && defined(STBI__X86_TARGET) && !defined(STBI_MINGW_ENABLE_SSE2) && !defined(STBI_NO_SIMD)
// Note that __MINGW32__ doesn't actually mean 32-bit, so we have to avoid STBI__X64_TARGET
//
// 32-bit MinGW wants ESP to be 16-byte aligned, but this is not in the
// Windows ABI and VC++ as well as Windows DLLs don't maintain that invariant.
// As a result, enabling SSE2 on 32-bit MinGW is dangerous when not
// simultaneously enabling "-mstackrealign".
//
// See https://github.com/nothings/stb/issues/81 for more information.
//
// So default to no SSE2 on 32-bit MinGW. If you've read this far and added
// -mstackrealign to your build settings, feel free to #define STBI_MINGW_ENABLE_SSE2.
#define STBI_NO_SIMD
#endif

#if !defined(STBI_NO_SIMD) && (defined(STBI__X86_TARGET) || defined(STBI__X64_TARGET))
#define STBI_SSE2
#include <emmintrin.h>

#ifdef _MSC_VER

#if _MSC_VER >= 1400  // not VC6
#include <intrin.h> // __cpuid
static int stbi__cpuid3(void)
{
   int info[4];
   __cpuid(info,1);
   return info[3];
}
#else
static int stbi__cpuid3(void)
{
   int res;
   __asm {
      mov  eax,1
      cpuid
      mov  res,edx
   }
   return res;
}
#endif

#define STBI_SIMD_ALIGN(type, name) __declspec(align(16)) type name

#if !defined(STBI_NO_JPEG) && defined(STBI_SSE2)
static int stbi__sse2_available(void)
{
   int info3 = stbi__cpuid3();
   return ((info3 >> 26) & 1) != 0;
}
#endif

#else // assume GCC-style if not VC++
#define STBI_SIMD_ALIGN(type, name) type name __attribute__((aligned(16)))

#if !defined(STBI_NO_JPEG) && defined(STBI_SSE2)
static int stbi__sse2_available(void)
{
   // If we're even attempting to compile this on GCC/Clang, that means
   // -msse2 is on, which means the compiler is allowed to use SSE2
   // instructions at will, and so are we.
   return 1;
}
#endif

#endif
#endif

// ARM NEON
#if defined(STBI_NO_SIMD) && defined(STBI_NEON)
#undef STBI_NEON
#endif

#ifdef STBI_NEON
#include <arm_neon.h>
// assume GCC or Clang on ARM targets
#define STBI_SIMD_ALIGN(type, name) type name __attribute__((aligned(16)))
#endif

#ifndef STBI_SIMD_ALIGN
#define STBI_SIMD_ALIGN(type, name) type name
#endif

///////////////////////////////////////////////
//
//  stbi__context struct and start_xxx functions

// stbi__context structure is our basic context used by all images, so it
// contains all the IO context, plus some basic image information
typedef struct
{
   stbi__uint32 img_x, img_y;
   int img_n, img_out_n;

   stbi_io_callbacks io;
   void *io_user_data;

   int read_from_callbacks;
   int buflen;
   stbi_uc buffer_start[128];

   stbi_uc *img_buffer, *img_buffer_end;
   stbi_uc *img_buffer_original, *img_buffer_original_end;
} stbi__context;


static void stbi__refill_buffer(stbi__context *s);

// initialize a memory-decode context
static void stbi__start_mem(stbi__context *s, stbi_uc const *buffer, int len)
{
   s->io.read = NULL;
   s->read_from_callbacks = 0;
   s->img_buffer = s->img_buffer_original = (stbi_uc *) buffer;
   s->img_buffer_end = s->img_buffer_original_end = (stbi_uc *) buffer+len;
}

// initialize a callback-based context
static void stbi__start_callbacks(stbi__context *s, stbi_io_callbacks *c, void *user)
{
   s->io = *c;
   s->io_user_data = user;
   s->buflen = sizeof(s->buffer_start);
   s->read_from_callbacks = 1;
   s->img_buffer_original = s->buffer_start;
   stbi__refill_buffer(s);
   s->img_buffer_original_end = s->img_buffer_end;
}

#ifndef STBI_NO_STDIO

static int stbi__stdio_read(void *user, char *data, int size)
{
   return (int) fread(data,1,size,(FILE*) user);
}

static void stbi__stdio_skip(void *user, int n)
{
   fseek((FILE*) user, n, SEEK_CUR);
}

static int stbi__stdio_eof(void *user)
{
   return feof((FILE*) user);
}

static stbi_io_callbacks stbi__stdio_callbacks =
{
   stbi__stdio_read,
   stbi__stdio_skip,
   stbi__stdio_eof,
};

static void stbi__start_file(stbi__context *s, FILE *f)
{
   stbi__start_callbacks(s, &stbi__stdio_callbacks, (void *) f);
}

//static void stop_file(stbi__context *s) { }

#endif // !STBI_NO_STDIO

static void stbi__rewind(stbi__context *s)
{
   // conceptually rewind SHOULD rewind to the beginning of the stream,
   // but we just rewind to the beginning of the initial buffer, because
   // we only use it after doing 'test', which only ever looks at at most 92 bytes
   s->img_buffer = s->img_buffer_original;
   s->img_buffer_end = s->img_buffer_original_end;
}

enum
{
   STBI_ORDER_RGB,
   STBI_ORDER_BGR
};

typedef struct
{
   int bits_per_channel;
   int num_channels;
   int channel_order;
} stbi__result_info;

#ifndef STBI_NO_JPEG
static int      stbi__jpeg_test(stbi__context *s);
static void    *stbi__jpeg_load(stbi__context *s, int *x, int *y, int *comp, int req_comp, stbi__result_info *ri);
static int      stbi__jpeg_info(stbi__context *s, int *x, int *y, int *comp);
#endif

#ifndef STBI_NO_PNG
static int      stbi__png_test(stbi__context *s);
static void    *stbi__png_load(stbi__context *s, int *x, int *y, int *comp, int req_comp, stbi__result_info *ri);
static int      stbi__png_info(stbi__context *s, int *x, int *y, int *comp);
static int      stbi__png_is16(stbi__context *s);
#endif

#ifndef STBI_NO_BMP
static int      stbi__bmp_test(stbi__context *s);
static void    *stbi__bmp_load(stbi__context *s, int *x, int *y, int *comp, int req_comp, stbi__result_info *ri);
static int      stbi__bmp_info(stbi__context *s, int *x, int *y, int *comp);
#endif

#ifndef STBI_NO_TGA
static int      stbi__tga_test(stbi__context *s);
static void    *stbi__tga_load(stbi__context *s, int *x, int *y, int *comp, int req_comp, stbi__result_info *ri);
static int      stbi__tga_info(stbi__context *s, int *x, int *y, int *comp);
#endif

#ifndef STBI_NO_PSD
static int      stbi__psd_test(stbi__context *s);
static void    *stbi__psd_load(stbi__context *s, int *x, int *y, int *comp, int req_comp, stbi__result_info *ri, int bpc);
static int      stbi__psd_info(stbi__context *s, int *x, int *y, int *comp);
static int      stbi__psd_is16(stbi__context *s);
#endif

#ifndef STBI_NO_HDR
static int      stbi__hdr_test(stbi__context *s);
static float   *stbi__hdr_load(stbi__context *s, int *x, int *y, int *comp, int req_comp, stbi__result_info *ri);
static int      stbi__hdr_info(stbi__context *s, int *x, int *y, int *comp);
#endif

#ifndef STBI_NO_PIC
static int      stbi__pic_test(stbi__context *s);
static void    *stbi__pic_load(stbi__context *s, int *x, int *y, int *comp, int req_comp, stbi__result_info *ri);
static int      stbi__pic_info(stbi__context *s, int *x, int *y, int *comp);
#endif

#ifndef STBI_NO_GIF
static int      stbi__gif_test(stbi__context *s);
static void    *stbi__gif_load(stbi__context *s, int *x, int *y, int *comp, int req_comp, stbi__result_info *ri);
static void    *stbi__load_gif_main(stbi__context *s, int **delays, int *x, int *y, int *z, int *comp, int req_comp);
static int      stbi__gif_info(stbi__context *s, int *x, int *y, int *comp);
#endif

#ifndef STBI_NO_PNM
static int      stbi__pnm_test(stbi__context *s);
static void    *stbi__pnm_load(stbi__context *s, int *x, int *y, int *comp, int req_comp, stbi__result_info *ri);
static int      stbi__pnm_info(stbi__context *s, int *x, int *y, int *comp);
#endif

static
#ifdef STBI_THREAD_LOCAL
STBI_THREAD_LOCAL
#endif
const char *stbi__g_failure_reason;

STBIDEF const char *stbi_failure_reason(void)
{
   return stbi__g_failure_reason;
}

#ifndef STBI_NO_FAILURE_STRINGS
static int stbi__err(const char *str)
{
   stbi__g_failure_reason = str;
   return 0;
}
#endif

static void *stbi__malloc(size_t size)
{
    return STBI_MALLOC(size);
}

// stb_image uses ints pervasively, including for offset calculations.
// therefore the largest decoded image size we can support with the
// current code, even on 64-bit targets, is INT_MAX. this is not a
// significant limitation for the intended use case.
//
// we do, however, need to make sure our size calculations don't
// overflow. hence a few helper functions for size calculations that
// multiply integers together, making sure that they're non-negative
// and no overflow occurs.

// return 1 if the sum is valid, 0 on overflow.
// negative terms are considered invalid.
static int stbi__addsizes_valid(int a, int b)
{
   if (b < 0) return 0;
   // now 0 <= b <= INT_MAX, hence also
   // 0 <= INT_MAX - b <= INTMAX.
   // And "a + b <= INT_MAX" (which might overflow) is the
   // same as a <= INT_MAX - b (no overflow)
   return a <= INT_MAX - b;
}

// returns 1 if the product is valid, 0 on overflow.
// negative factors are considered invalid.
static int stbi__mul2sizes_valid(int a, int b)
{
   if (a < 0 || b < 0) return 0;
   if (b == 0) return 1; // mul-by-0 is always safe
   // portable way to check for no overflows in a*b
   return a <= INT_MAX/b;
}

#if !defined(STBI_NO_JPEG) || !defined(STBI_NO_PNG) || !defined(STBI_NO_TGA) || !defined(STBI_NO_HDR)
// returns 1 if "a*b + add" has no negative terms/factors and doesn't overflow
static int stbi__mad2sizes_valid(int a, int b, int add)
{
   return stbi__mul2sizes_valid(a, b) && stbi__addsizes_valid(a*b, add);
}
#endif

// returns 1 if "a*b*c + add" has no negative terms/factors and doesn't overflow
static int stbi__mad3sizes_valid(int a, int b, int c, int add)
{
   return stbi__mul2sizes_valid(a, b) && stbi__mul2sizes_valid(a*b, c) &&
      stbi__addsizes_valid(a*b*c, add);
}

// returns 1 if "a*b*c*d + add" has no negative terms/factors and doesn't overflow
#if !defined(STBI_NO_LINEAR) || !defined(STBI_NO_HDR)
static int stbi__mad4sizes_valid(int a, int b, int c, int d, int add)
{
   return stbi__mul2sizes_valid(a, b) && stbi__mul2sizes_valid(a*b, c) &&
      stbi__mul2sizes_valid(a*b*c, d) && stbi__addsizes_valid(a*b*c*d, add);
}
#endif

#if !defined(STBI_NO_JPEG) || !defined(STBI_NO_PNG) || !defined(STBI_NO_TGA) || !defined(STBI_NO_HDR)
// mallocs with size overflow checking
static void *stbi__malloc_mad2(int a, int b, int add)
{
   if (!stbi__mad2sizes_valid(a, b, add)) return NULL;
   return stbi__malloc(a*b + add);
}
#endif

static void *stbi__malloc_mad3(int a, int b, int c, int add)
{
   if (!stbi__mad3sizes_valid(a, b, c, add)) return NULL;
   return stbi__malloc(a*b*c + add);
}

#if !defined(STBI_NO_LINEAR) || !defined(STBI_NO_HDR)
static void *stbi__malloc_mad4(int a, int b, int c, int d, int add)
{
   if (!stbi__mad4sizes_valid(a, b, c, d, add)) return NULL;
   return stbi__malloc(a*b*c*d + add);
}
#endif

// stbi__err - error
// stbi__errpf - error returning pointer to float
// stbi__errpuc - error returning pointer to unsigned char

#ifdef STBI_NO_FAILURE_STRINGS
   #define stbi__err(x,y)  0
#elif defined(STBI_FAILURE_USERMSG)
   #define stbi__err(x,y)  stbi__err(y)
#else
   #define stbi__err(x,y)  stbi__err(x)
#endif

#define stbi__errpf(x,y)   ((float *)(size_t) (stbi__err(x,y)?NULL:NULL))
#define stbi__errpuc(x,y)  ((unsigned char *)(size_t) (stbi__err(x,y)?NULL:NULL))

STBIDEF void stbi_image_free(void *retval_from_stbi_load)
{
   STBI_FREE(retval_from_stbi_load);
}

#ifndef STBI_NO_LINEAR
static float   *stbi__ldr_to_hdr(stbi_uc *data, int x, int y, int comp);
#endif

#ifndef STBI_NO_HDR
static stbi_uc *stbi__hdr_to_ldr(float   *data, int x, int y, int comp);
#endif

static int stbi__vertically_flip_on_load_global = 0;

STBIDEF void stbi_set_flip_vertically_on_load(int flag_true_if_should_flip)
{
   stbi__vertically_flip_on_load_global = flag_true_if_should_flip;
}

#ifndef STBI_THREAD_LOCAL
#define stbi__vertically_flip_on_load  stbi__vertically_flip_on_load_global
#else
static STBI_THREAD_LOCAL int stbi__vertically_flip_on_load_local, stbi__vertically_flip_on_load_set;

STBIDEF void stbi_set_flip_vertically_on_load_thread(int flag_true_if_should_flip)
{
   stbi__vertically_flip_on_load_local = flag_true_if_should_flip;
   stbi__vertically_flip_on_load_set = 1;
}

#define stbi__vertically_flip_on_load  (stbi__vertically_flip_on_load_set       \
                                         ? stbi__vertically_flip_on_load_local  \
                                         : stbi__vertically_flip_on_load_global)
#endif // STBI_THREAD_LOCAL

static void *stbi__load_main(stbi__context *s, int *x, int *y, int *comp, int req_comp, stbi__result_info *ri, int bpc)
{
   memset(ri, 0, sizeof(*ri)); // make sure it's initialized if we add new fields
   ri->bits_per_channel = 8; // default is 8 so most paths don't have to be changed
   ri->channel_order = STBI_ORDER_RGB; // all current input & output are this, but this is here so we can add BGR order
   ri->num_channels = 0;

   #ifndef STBI_NO_JPEG
   if (stbi__jpeg_test(s)) return stbi__jpeg_load(s,x,y,comp,req_comp, ri);
   #endif
   #ifndef STBI_NO_PNG
   if (stbi__png_test(s))  return stbi__png_load(s,x,y,comp,req_comp, ri);
   #endif
   #ifndef STBI_NO_BMP
   if (stbi__bmp_test(s))  return stbi__bmp_load(s,x,y,comp,req_comp, ri);
   #endif
   #ifndef STBI_NO_GIF
   if (stbi__gif_test(s))  return stbi__gif_load(s,x,y,comp,req_comp, ri);
   #endif
   #ifndef STBI_NO_PSD
   if (stbi__psd_test(s))  return stbi__psd_load(s,x,y,comp,req_comp, ri, bpc);
   #else
   STBI_NOTUSED(bpc);
   #endif
   #ifndef STBI_NO_PIC
   if (stbi__pic_test(s))  return stbi__pic_load(s,x,y,comp,req_comp, ri);
   #endif
   #ifndef STBI_NO_PNM
   if (stbi__pnm_test(s))  return stbi__pnm_load(s,x,y,comp,req_comp, ri);
   #endif

   #ifndef STBI_NO_HDR
   if (stbi__hdr_test(s)) {
      float *hdr = stbi__hdr_load(s, x,y,comp,req_comp, ri);
      return stbi__hdr_to_ldr(hdr, *x, *y, req_comp ? req_comp : *comp);
   }
   #endif

   #ifndef STBI_NO_TGA
   // test tga last because it's a crappy test!
   if (stbi__tga_test(s))
      return stbi__tga_load(s,x,y,comp,req_comp, ri);
   #endif

   return stbi__errpuc("unknown image type", "Image not of any known type, or corrupt");
}

static stbi_uc *stbi__convert_16_to_8(stbi__uint16 *orig, int w, int h, int channels)
{
   int i;
   int img_len = w * h * channels;
   stbi_uc *reduced;

   reduced = (stbi_uc *) stbi__malloc(img_len);
   if (reduced == NULL) return stbi__errpuc("outofmem", "Out of memory");

   for (i = 0; i < img_len; ++i)
      reduced[i] = (stbi_uc)((orig[i] >> 8) & 0xFF); // top half of each byte is sufficient approx of 16->8 bit scaling

   STBI_FREE(orig);
   return reduced;
}

static stbi__uint16 *stbi__convert_8_to_16(stbi_uc *orig, int w, int h, int channels)
{
   int i;
   int img_len = w * h * channels;
   stbi__uint16 *enlarged;

   enlarged = (stbi__uint16 *) stbi__malloc(img_len*2);
   if (enlarged == NULL) return (stbi__uint16 *) stbi__errpuc("outofmem", "Out of memory");

   for (i = 0; i < img_len; ++i)
      enlarged[i] = (stbi__uint16)((orig[i] << 8) + orig[i]); // replicate to high and low byte, maps 0->0, 255->0xffff

   STBI_FREE(orig);
   return enlarged;
}

static void stbi__vertical_flip(void *image, int w, int h, int bytes_per_pixel)
{
   int row;
   size_t bytes_per_row = (size_t)w * bytes_per_pixel;
   stbi_uc temp[2048];
   stbi_uc *bytes = (stbi_uc *)image;

   for (row = 0; row < (h>>1); row++) {
      stbi_uc *row0 = bytes + row*bytes_per_row;
      stbi_uc *row1 = bytes + (h - row - 1)*bytes_per_row;
      // swap row0 with row1
      size_t bytes_left = bytes_per_row;
      while (bytes_left) {
         size_t bytes_copy = (bytes_left < sizeof(temp)) ? bytes_left : sizeof(temp);
         memcpy(temp, row0, bytes_copy);
         memcpy(row0, row1, bytes_copy);
         memcpy(row1, temp, bytes_copy);
         row0 += bytes_copy;
         row1 += bytes_copy;
         bytes_left -= bytes_copy;
      }
   }
}

#ifndef STBI_NO_GIF
static void stbi__vertical_flip_slices(void *image, int w, int h, int z, int bytes_per_pixel)
{
   int slice;
   int slice_size = w * h * bytes_per_pixel;

   stbi_uc *bytes = (stbi_uc *)image;
   for (slice = 0; slice < z; ++slice) {
      stbi__vertical_flip(bytes, w, h, bytes_per_pixel);
      bytes += slice_size;
   }
}
#endif

static unsigned char *stbi__load_and_postprocess_8bit(stbi__context *s, int *x, int *y, int *comp, int req_comp)
{
   stbi__result_info ri;
   void *result = stbi__load_main(s, x, y, comp, req_comp, &ri, 8);

   if (result == NULL)
      return NULL;

   if (ri.bits_per_channel != 8) {
      STBI_ASSERT(ri.bits_per_channel == 16);
      result = stbi__convert_16_to_8((stbi__uint16 *) result, *x, *y, req_comp == 0 ? *comp : req_comp);
      ri.bits_per_channel = 8;
   }

   // @TODO: move stbi__convert_format to here

   if (stbi__vertically_flip_on_load) {
      int channels = req_comp ? req_comp : *comp;
      stbi__vertical_flip(result, *x, *y, channels * sizeof(stbi_uc));
   }

   return (unsigned char *) result;
}

static stbi__uint16 *stbi__load_and_postprocess_16bit(stbi__context *s, int *x, int *y, int *comp, int req_comp)
{
   stbi__result_info ri;
   void *result = stbi__load_main(s, x, y, comp, req_comp, &ri, 16);

   if (result == NULL)
      return NULL;

   if (ri.bits_per_channel != 16) {
      STBI_ASSERT(ri.bits_per_channel == 8);
      result = stbi__convert_8_to_16((stbi_uc *) result, *x, *y, req_comp == 0 ? *comp : req_comp);
      ri.bits_per_channel = 16;
   }

   // @TODO: move stbi__convert_format16 to here
   // @TODO: special case RGB-to-Y (and RGBA-to-YA) for 8-bit-to-16-bit case to keep more precision

   if (stbi__vertically_flip_on_load) {
      int channels = req_comp ? req_comp : *comp;
      stbi__vertical_flip(result, *x, *y, channels * sizeof(stbi__uint16));
   }

   return (stbi__uint16 *) result;
}

#if !defined(STBI_NO_HDR) && !defined(STBI_NO_LINEAR)
static void stbi__float_postprocess(float *result, int *x, int *y, int *comp, int req_comp)
{
   if (stbi__vertically_flip_on_load && result != NULL) {
      int channels = req_comp ? req_comp : *comp;
      stbi__vertical_flip(result, *x, *y, channels * sizeof(float));
   }
}
#endif

#ifndef STBI_NO_STDIO

#if defined(_MSC_VER) && defined(STBI_WINDOWS_UTF8)
STBI_EXTERN __declspec(dllimport) int __stdcall MultiByteToWideChar(unsigned int cp, unsigned long flags, const char *str, int cbmb, wchar_t *widestr, int cchwide);
STBI_EXTERN __declspec(dllimport) int __stdcall WideCharToMultiByte(unsigned int cp, unsigned long flags, const wchar_t *widestr, int cchwide, char *str, int cbmb, const char *defchar, int *used_default);
#endif

#if defined(_MSC_VER) && defined(STBI_WINDOWS_UTF8)
STBIDEF int stbi_convert_wchar_to_utf8(char *buffer, size_t bufferlen, const wchar_t* input)
{
	return WideCharToMultiByte(65001 /* UTF8 */, 0, input, -1, buffer, (int) bufferlen, NULL, NULL);
}
#endif

static FILE *stbi__fopen(char const *filename, char const *mode)
{
   FILE *f;
#if defined(_MSC_VER) && defined(STBI_WINDOWS_UTF8)
   wchar_t wMode[64];
   wchar_t wFilename[1024];
	if (0 == MultiByteToWideChar(65001 /* UTF8 */, 0, filename, -1, wFilename, sizeof(wFilename)))
      return 0;

	if (0 == MultiByteToWideChar(65001 /* UTF8 */, 0, mode, -1, wMode, sizeof(wMode)))
      return 0;

#if _MSC_VER >= 1400
	if (0 != _wfopen_s(&f, wFilename, wMode))
		f = 0;
#else
   f = _wfopen(wFilename, wMode);
#endif

#elif defined(_MSC_VER) && _MSC_VER >= 1400
   if (0 != fopen_s(&f, filename, mode))
      f=0;
#else
   f = fopen(filename, mode);
#endif
   return f;
}


STBIDEF stbi_uc *stbi_load(char const *filename, int *x, int *y, int *comp, int req_comp)
{
   FILE *f = stbi__fopen(filename, "rb");
   unsigned char *result;
   if (!f) return stbi__errpuc("can't fopen", "Unable to open file");
   result = stbi_load_from_file(f,x,y,comp,req_comp);
   fclose(f);
   return result;
}

STBIDEF stbi_uc *stbi_load_from_file(FILE *f, int *x, int *y, int *comp, int req_comp)
{
   unsigned char *result;
   stbi__context s;
   stbi__start_file(&s,f);
   result = stbi__load_and_postprocess_8bit(&s,x,y,comp,req_comp);
   if (result) {
      // need to 'unget' all the characters in the IO buffer
      fseek(f, - (int) (s.img_buffer_end - s.img_buffer), SEEK_CUR);
   }
   return result;
}

STBIDEF stbi__uint16 *stbi_load_from_file_16(FILE *f, int *x, int *y, int *comp, int req_comp)
{
   stbi__uint16 *result;
   stbi__context s;
   stbi__start_file(&s,f);
   result = stbi__load_and_postprocess_16bit(&s,x,y,comp,req_comp);
   if (result) {
      // need to 'unget' all the characters in the IO buffer
      fseek(f, - (int) (s.img_buffer_end - s.img_buffer), SEEK_CUR);
   }
   return result;
}

STBIDEF stbi_us *stbi_load_16(char const *filename, int *x, int *y, int *comp, int req_comp)
{
   FILE *f = stbi__fopen(filename, "rb");
   stbi__uint16 *result;
   if (!f) return (stbi_us *) stbi__errpuc("can't fopen", "Unable to open file");
   result = stbi_load_from_file_16(f,x,y,comp,req_comp);
   fclose(f);
   return result;
}


#endif //!STBI_NO_STDIO

STBIDEF stbi_us *stbi_load_16_from_memory(stbi_uc const *buffer, int len, int *x, int *y, int *channels_in_file, int desired_channels)
{
   stbi__context s;
   stbi__start_mem(&s,buffer,len);
   return stbi__load_and_postprocess_16bit(&s,x,y,channels_in_file,desired_channels);
}

STBIDEF stbi_us *stbi_load_16_from_callbacks(stbi_io_callbacks const *clbk, void *user, int *x, int *y, int *channels_in_file, int desired_channels)
{
   stbi__context s;
   stbi__start_callbacks(&s, (stbi_io_callbacks *)clbk, user);
   return stbi__load_and_postprocess_16bit(&s,x,y,channels_in_file,desired_channels);
}

STBIDEF stbi_uc *stbi_load_from_memory(stbi_uc const *buffer, int len, int *x, int *y, int *comp, int req_comp)
{
   stbi__context s;
   stbi__start_mem(&s,buffer,len);
   return stbi__load_and_postprocess_8bit(&s,x,y,comp,req_comp);
}

STBIDEF stbi_uc *stbi_load_from_callbacks(stbi_io_callbacks const *clbk, void *user, int *x, int *y, int *comp, int req_comp)
{
   stbi__context s;
   stbi__start_callbacks(&s, (stbi_io_callbacks *) clbk, user);
   return stbi__load_and_postprocess_8bit(&s,x,y,comp,req_comp);
}

#ifndef STBI_NO_GIF
STBIDEF stbi_uc *stbi_load_gif_from_memory(stbi_uc const *buffer, int len, int **delays, int *x, int *y, int *z, int *comp, int req_comp)
{
   unsigned char *result;
   stbi__context s;
   stbi__start_mem(&s,buffer,len);

   result = (unsigned char*) stbi__load_gif_main(&s, delays, x, y, z, comp, req_comp);
   if (stbi__vertically_flip_on_load) {
      stbi__vertical_flip_slices( result, *x, *y, *z, *comp );
   }

   return result;
}
#endif

#ifndef STBI_NO_LINEAR
static float *stbi__loadf_main(stbi__context *s, int *x, int *y, int *comp, int req_comp)
{
   unsigned char *data;
   #ifndef STBI_NO_HDR
   if (stbi__hdr_test(s)) {
      stbi__result_info ri;
      float *hdr_data = stbi__hdr_load(s,x,y,comp,req_comp, &ri);
      if (hdr_data)
         stbi__float_postprocess(hdr_data,x,y,comp,req_comp);
      return hdr_data;
   }
   #endif
   data = stbi__load_and_postprocess_8bit(s, x, y, comp, req_comp);
   if (data)
      return stbi__ldr_to_hdr(data, *x, *y, req_comp ? req_comp : *comp);
   return stbi__errpf("unknown image type", "Image not of any known type, or corrupt");
}

STBIDEF float *stbi_loadf_from_memory(stbi_uc const *buffer, int len, int *x, int *y, int *comp, int req_comp)
{
   stbi__context s;
   stbi__start_mem(&s,buffer,len);
   return stbi__loadf_main(&s,x,y,comp,req_comp);
}

STBIDEF float *stbi_loadf_from_callbacks(stbi_io_callbacks const *clbk, void *user, int *x, int *y, int *comp, int req_comp)
{
   stbi__context s;
   stbi__start_callbacks(&s, (stbi_io_callbacks *) clbk, user);
   return stbi__loadf_main(&s,x,y,comp,req_comp);
}

#ifndef STBI_NO_STDIO
STBIDEF float *stbi_loadf(char const *filename, int *x, int *y, int *comp, int req_comp)
{
   float *result;
   FILE *f = stbi__fopen(filename, "rb");
   if (!f) return stbi__errpf("can't fopen", "Unable to open file");
   result = stbi_loadf_from_file(f,x,y,comp,req_comp);
   fclose(f);
   return result;
}

STBIDEF float *stbi_loadf_from_file(FILE *f, int *x, int *y, int *comp, int req_comp)
{
   stbi__context s;
   stbi__start_file(&s,f);
   return stbi__loadf_main(&s,x,y,comp,req_comp);
}
#endif // !STBI_NO_STDIO

#endif // !STBI_NO_LINEAR

// these is-hdr-or-not is defined independent of whether STBI_NO_LINEAR is
// defined, for API simplicity; if STBI_NO_LINEAR is defined, it always
// reports false!

STBIDEF int stbi_is_hdr_from_memory(stbi_uc const *buffer, int len)
{
   #ifndef STBI_NO_HDR
   stbi__context s;
   stbi__start_mem(&s,buffer,len);
   return stbi__hdr_test(&s);
   #else
   STBI_NOTUSED(buffer);
   STBI_NOTUSED(len);
   return 0;
   #endif
}

#ifndef STBI_NO_STDIO
STBIDEF int      stbi_is_hdr          (char const *filename)
{
   FILE *f = stbi__fopen(filename, "rb");
   int result=0;
   if (f) {
      result = stbi_is_hdr_from_file(f);
      fclose(f);
   }
   return result;
}

STBIDEF int stbi_is_hdr_from_file(FILE *f)
{
   #ifndef STBI_NO_HDR
   long pos = ftell(f);
   int res;
   stbi__context s;
   stbi__start_file(&s,f);
   res = stbi__hdr_test(&s);
   fseek(f, pos, SEEK_SET);
   return res;
   #else
   STBI_NOTUSED(f);
   return 0;
   #endif
}
#endif // !STBI_NO_STDIO

STBIDEF int      stbi_is_hdr_from_callbacks(stbi_io_callbacks const *clbk, void *user)
{
   #ifndef STBI_NO_HDR
   stbi__context s;
   stbi__start_callbacks(&s, (stbi_io_callbacks *) clbk, user);
   return stbi__hdr_test(&s);
   #else
   STBI_NOTUSED(clbk);
   STBI_NOTUSED(user);
   return 0;
   #endif
}

#ifndef STBI_NO_LINEAR
static float stbi__l2h_gamma=2.2f, stbi__l2h_scale=1.0f;

STBIDEF void   stbi_ldr_to_hdr_gamma(float gamma) { stbi__l2h_gamma = gamma; }
STBIDEF void   stbi_ldr_to_hdr_scale(float scale) { stbi__l2h_scale = scale; }
#endif

static float stbi__h2l_gamma_i=1.0f/2.2f, stbi__h2l_scale_i=1.0f;

STBIDEF void   stbi_hdr_to_ldr_gamma(float gamma) { stbi__h2l_gamma_i = 1/gamma; }
STBIDEF void   stbi_hdr_to_ldr_scale(float scale) { stbi__h2l_scale_i = 1/scale; }


//////////////////////////////////////////////////////////////////////////////
//
// Common code used by all image loaders
//

enum
{
   STBI__SCAN_load=0,
   STBI__SCAN_type,
   STBI__SCAN_header
};

static void stbi__refill_buffer(stbi__context *s)
{
   int n = (s->io.read)(s->io_user_data,(char*)s->buffer_start,s->buflen);
   if (n == 0) {
      // at end of file, treat same as if from memory, but need to handle case
      // where s->img_buffer isn't pointing to safe memory, e.g. 0-byte file
      s->read_from_callbacks = 0;
      s->img_buffer = s->buffer_start;
      s->img_buffer_end = s->buffer_start+1;
      *s->img_buffer = 0;
   } else {
      s->img_buffer = s->buffer_start;
      s->img_buffer_end = s->buffer_start + n;
   }
}

stbi_inline static stbi_uc stbi__get8(stbi__context *s)
{
   if (s->img_buffer < s->img_buffer_end)
      return *s->img_buffer++;
   if (s->read_from_callbacks) {
      stbi__refill_buffer(s);
      return *s->img_buffer++;
   }
   return 0;
}

#if defined(STBI_NO_JPEG) && defined(STBI_NO_HDR) && defined(STBI_NO_PIC) && defined(STBI_NO_PNM)
// nothing
#else
stbi_inline static int stbi__at_eof(stbi__context *s)
{
   if (s->io.read) {
      if (!(s->io.eof)(s->io_user_data)) return 0;
      // if feof() is true, check if buffer = end
      // special case: we've only got the special 0 character at the end
      if (s->read_from_callbacks == 0) return 1;
   }

   return s->img_buffer >= s->img_buffer_end;
}
#endif

#if defined(STBI_NO_JPEG) && defined(STBI_NO_PNG) && defined(STBI_NO_BMP) && defined(STBI_NO_PSD) && defined(STBI_NO_TGA) && defined(STBI_NO_GIF) && defined(STBI_NO_PIC)
// nothing
#else
static void stbi__skip(stbi__context *s, int n)
{
   if (n < 0) {
      s->img_buffer = s->img_buffer_end;
      return;
   }
   if (s->io.read) {
      int blen = (int) (s->img_buffer_end - s->img_buffer);
      if (blen < n) {
         s->img_buffer = s->img_buffer_end;
         (s->io.flags)(s->io_user_data, n - blen);
         return;
      }
   }
   s->img_buffer += n;
}
#endif

#if defined(STBI_NO_PNG) && defined(STBI_NO_TGA) && defined(STBI_NO_HDR) && defined(STBI_NO_PNM)
// nothing
#else
static int stbi__getn(stbi__context *s, stbi_uc *buffer, int n)
{
   if (s->io.read) {
      int blen = (int) (s->img_buffer_end - s->img_buffer);
      if (blen < n) {
         int res, count;

         memcpy(buffer, s->img_buffer, blen);

         count = (s->io.read)(s->io_user_data, (char*) buffer + blen, n - blen);
         res = (count == (n-blen));
         s->img_buffer = s->img_buffer_end;
         return res;
      }
   }

   if (s->img_buffer+n <= s->img_buffer_end) {
      memcpy(buffer, s->img_buffer, n);
      s->img_buffer += n;
      return 1;
   } else
      return 0;
}
#endif

#if defined(STBI_NO_JPEG) && defined(STBI_NO_PNG) && defined(STBI_NO_PSD) && defined(STBI_NO_PIC)
// nothing
#else
static int stbi__get16be(stbi__context *s)
{
   int z = stbi__get8(s);
   return (z << 8) + stbi__get8(s);
}
#endif

#if defined(STBI_NO_PNG) && defined(STBI_NO_PSD) && defined(STBI_NO_PIC)
// nothing
#else
static stbi__uint32 stbi__get32be(stbi__context *s)
{
   stbi__uint32 z = stbi__get16be(s);
   return (z << 16) + stbi__get16be(s);
}
#endif

#if defined(STBI_NO_BMP) && defined(STBI_NO_TGA) && defined(STBI_NO_GIF)
// nothing
#else
static int stbi__get16le(stbi__context *s)
{
   int z = stbi__get8(s);
   return z + (stbi__get8(s) << 8);
}
#endif

#ifndef STBI_NO_BMP
static stbi__uint32 stbi__get32le(stbi__context *s)
{
   stbi__uint32 z = stbi__get16le(s);
   return z + (stbi__get16le(s) << 16);
}
#endif

#define STBI__BYTECAST(x)  ((stbi_uc) ((x) & 255))  // truncate int to byte without warnings

#if defined(STBI_NO_JPEG) && defined(STBI_NO_PNG) && defined(STBI_NO_BMP) && defined(STBI_NO_PSD) && defined(STBI_NO_TGA) && defined(STBI_NO_GIF) && defined(STBI_NO_PIC) && defined(STBI_NO_PNM)
// nothing
#else
//////////////////////////////////////////////////////////////////////////////
//
//  generic converter from built-in img_n to req_comp
//    individual types do this automatically as much as possible (e.g. jpeg
//    does all cases internally since it needs to colorspace convert anyway,
//    and it never has alpha, so very few cases ). png can automatically
//    interleave an alpha=255 channel, but falls back to this for other cases
//
//  assume data buffer is malloced, so malloc a new one and free that one
//  only failure mode is malloc failing

static stbi_uc stbi__compute_y(int r, int g, int b)
{
   return (stbi_uc) (((r*77) + (g*150) +  (29*b)) >> 8);
}
#endif

#if defined(STBI_NO_PNG) && defined(STBI_NO_BMP) && defined(STBI_NO_PSD) && defined(STBI_NO_TGA) && defined(STBI_NO_GIF) && defined(STBI_NO_PIC) && defined(STBI_NO_PNM)
// nothing
#else
static unsigned char *stbi__convert_format(unsigned char *data, int img_n, int req_comp, unsigned int x, unsigned int y)
{
   int i,j;
   unsigned char *good;

   if (req_comp == img_n) return data;
   STBI_ASSERT(req_comp >= 1 && req_comp <= 4);

   good = (unsigned char *) stbi__malloc_mad3(req_comp, x, y, 0);
   if (good == NULL) {
      STBI_FREE(data);
      return stbi__errpuc("outofmem", "Out of memory");
   }

   for (j=0; j < (int) y; ++j) {
      unsigned char *src  = data + j * x * img_n   ;
      unsigned char *dest = good + j * x * req_comp;

      #define STBI__COMBO(a,b)  ((a)*8+(b))
      #define STBI__CASE(a,b)   case STBI__COMBO(a,b): for(i=x-1; i >= 0; --i, src += a, dest += b)
      // convert source image with img_n components to one with req_comp components;
      // avoid switch per pixel, so use switch per scanline and massive macros
      switch (STBI__COMBO(img_n, req_comp)) {
         STBI__CASE(1,2) { dest[0]=src[0]; dest[1]=255;                                     } break;
         STBI__CASE(1,3) { dest[0]=dest[1]=dest[2]=src[0];                                  } break;
         STBI__CASE(1,4) { dest[0]=dest[1]=dest[2]=src[0]; dest[3]=255;                     } break;
         STBI__CASE(2,1) { dest[0]=src[0];                                                  } break;
         STBI__CASE(2,3) { dest[0]=dest[1]=dest[2]=src[0];                                  } break;
         STBI__CASE(2,4) { dest[0]=dest[1]=dest[2]=src[0]; dest[3]=src[1];                  } break;
         STBI__CASE(3,4) { dest[0]=src[0];dest[1]=src[1];dest[2]=src[2];dest[3]=255;        } break;
         STBI__CASE(3,1) { dest[0]=stbi__compute_y(src[0],src[1],src[2]);                   } break;
         STBI__CASE(3,2) { dest[0]=stbi__compute_y(src[0],src[1],src[2]); dest[1] = 255;    } break;
         STBI__CASE(4,1) { dest[0]=stbi__compute_y(src[0],src[1],src[2]);                   } break;
         STBI__CASE(4,2) { dest[0]=stbi__compute_y(src[0],src[1],src[2]); dest[1] = src[3]; } break;
         STBI__CASE(4,3) { dest[0]=src[0];dest[1]=src[1];dest[2]=src[2];                    } break;
         default: STBI_ASSERT(0);
      }
      #undef STBI__CASE
   }

   STBI_FREE(data);
   return good;
}
#endif

#if defined(STBI_NO_PNG) && defined(STBI_NO_PSD)
// nothing
#else
static stbi__uint16 stbi__compute_y_16(int r, int g, int b)
{
   return (stbi__uint16) (((r*77) + (g*150) +  (29*b)) >> 8);
}
#endif

#if defined(STBI_NO_PNG) && defined(STBI_NO_PSD)
// nothing
#else
static stbi__uint16 *stbi__convert_format16(stbi__uint16 *data, int img_n, int req_comp, unsigned int x, unsigned int y)
{
   int i,j;
   stbi__uint16 *good;

   if (req_comp == img_n) return data;
   STBI_ASSERT(req_comp >= 1 && req_comp <= 4);

   good = (stbi__uint16 *) stbi__malloc(req_comp * x * y * 2);
   if (good == NULL) {
      STBI_FREE(data);
      return (stbi__uint16 *) stbi__errpuc("outofmem", "Out of memory");
   }

   for (j=0; j < (int) y; ++j) {
      stbi__uint16 *src  = data + j * x * img_n   ;
      stbi__uint16 *dest = good + j * x * req_comp;

      #define STBI__COMBO(a,b)  ((a)*8+(b))
      #define STBI__CASE(a,b)   case STBI__COMBO(a,b): for(i=x-1; i >= 0; --i, src += a, dest += b)
      // convert source image with img_n components to one with req_comp components;
      // avoid switch per pixel, so use switch per scanline and massive macros
      switch (STBI__COMBO(img_n, req_comp)) {
         STBI__CASE(1,2) { dest[0]=src[0]; dest[1]=0xffff;                                     } break;
         STBI__CASE(1,3) { dest[0]=dest[1]=dest[2]=src[0];                                     } break;
         STBI__CASE(1,4) { dest[0]=dest[1]=dest[2]=src[0]; dest[3]=0xffff;                     } break;
         STBI__CASE(2,1) { dest[0]=src[0];                                                     } break;
         STBI__CASE(2,3) { dest[0]=dest[1]=dest[2]=src[0];                                     } break;
         STBI__CASE(2,4) { dest[0]=dest[1]=dest[2]=src[0]; dest[3]=src[1];                     } break;
         STBI__CASE(3,4) { dest[0]=src[0];dest[1]=src[1];dest[2]=src[2];dest[3]=0xffff;        } break;
         STBI__CASE(3,1) { dest[0]=stbi__compute_y_16(src[0],src[1],src[2]);                   } break;
         STBI__CASE(3,2) { dest[0]=stbi__compute_y_16(src[0],src[1],src[2]); dest[1] = 0xffff; } break;
         STBI__CASE(4,1) { dest[0]=stbi__compute_y_16(src[0],src[1],src[2]);                   } break;
         STBI__CASE(4,2) { dest[0]=stbi__compute_y_16(src[0],src[1],src[2]); dest[1] = src[3]; } break;
         STBI__CASE(4,3) { dest[0]=src[0];dest[1]=src[1];dest[2]=src[2];                       } break;
         default: STBI_ASSERT(0);
      }
      #undef STBI__CASE
   }

   STBI_FREE(data);
   return good;
}
#endif

#ifndef STBI_NO_LINEAR
static float   *stbi__ldr_to_hdr(stbi_uc *data, int x, int y, int comp)
{
   int i,k,n;
   float *output;
   if (!data) return NULL;
   output = (float *) stbi__malloc_mad4(x, y, comp, sizeof(float), 0);
   if (output == NULL) { STBI_FREE(data); return stbi__errpf("outofmem", "Out of memory"); }
   // compute number of non-alpha components
   if (comp & 1) n = comp; else n = comp-1;
   for (i=0; i < x*y; ++i) {
      for (k=0; k < n; ++k) {
         output[i*comp + k] = (float) (pow(data[i*comp+k]/255.0f, stbi__l2h_gamma) * stbi__l2h_scale);
      }
   }
   if (n < comp) {
      for (i=0; i < x*y; ++i) {
         output[i*comp + n] = data[i*comp + n]/255.0f;
      }
   }
   STBI_FREE(data);
   return output;
}
#endif

#ifndef STBI_NO_HDR
#define stbi__float2int(x)   ((int) (x))
static stbi_uc *stbi__hdr_to_ldr(float   *data, int x, int y, int comp)
{
   int i,k,n;
   stbi_uc *output;
   if (!data) return NULL;
   output = (stbi_uc *) stbi__malloc_mad3(x, y, comp, 0);
   if (output == NULL) { STBI_FREE(data); return stbi__errpuc("outofmem", "Out of memory"); }
   // compute number of non-alpha components
   if (comp & 1) n = comp; else n = comp-1;
   for (i=0; i < x*y; ++i) {
      for (k=0; k < n; ++k) {
         float z = (float) pow(data[i*comp+k]*stbi__h2l_scale_i, stbi__h2l_gamma_i) * 255 + 0.5f;
         if (z < 0) z = 0;
         if (z > 255) z = 255;
         output[i*comp + k] = (stbi_uc) stbi__float2int(z);
      }
      if (k < comp) {
         float z = data[i*comp+k] * 255 + 0.5f;
         if (z < 0) z = 0;
         if (z > 255) z = 255;
         output[i*comp + k] = (stbi_uc) stbi__float2int(z);
      }
   }
   STBI_FREE(data);
   return output;
}
#endif

//////////////////////////////////////////////////////////////////////////////
//
//  "baseline" JPEG/JFIF decoder
//
//    simple implementation
//      - doesn't support delayed output of y-dimension
//      - simple interface (only one output format: 8-bit interleaved RGB)
//      - doesn't try to recover corrupt jpegs
//      - doesn't allow partial loading, loading multiple at once
//      - still fast on x86 (copying globals into locals doesn't help x86)
//      - allocates lots of intermediate memory (full size of all components)
//        - non-interleaved case requires this anyway
//        - allows good upsampling (see next)
//    high-quality
//      - upsampled channels are bilinearly interpolated, even across blocks
//      - quality integer IDCT derived from IJG's 'slow'
//    performance
//      - fast huffman; reasonable integer IDCT
//      - some SIMD kernels for common paths on targets with SSE2/NEON
//      - uses a lot of intermediate memory, could cache poorly

#ifndef STBI_NO_JPEG

// huffman decoding acceleration
#define FAST_BITS   9  // larger handles more cases; smaller stomps less cache

typedef struct
{
   stbi_uc  fast[1 << FAST_BITS];
   // weirdly, repacking this into AoS is a 10% speed loss, instead of a win
   stbi__uint16 code[256];
   stbi_uc  values[256];
   stbi_uc  size[257];
   unsigned int maxcode[18];
   int    delta[17];   // old 'firstsymbol' - old 'firstcode'
} stbi__huffman;

typedef struct
{
   stbi__context *s;
   stbi__huffman huff_dc[4];
   stbi__huffman huff_ac[4];
   stbi__uint16 dequant[4][64];
   stbi__int16 fast_ac[4][1 << FAST_BITS];

// sizes for components, interleaved MCUs
   int img_h_max, img_v_max;
   int img_mcu_x, img_mcu_y;
   int img_mcu_w, img_mcu_h;

// definition of jpeg image component
   struct
   {
      int id;
      int h,v;
      int tq;
      int hd,ha;
      int dc_pred;

      int x,y,w2,h2;
      stbi_uc *data;
      void *raw_data, *raw_coeff;
      stbi_uc *linebuf;
      short   *coeff;   // progressive only
      int      coeff_w, coeff_h; // number of 8x8 coefficient blocks
   } img_comp[4];

   stbi__uint32   code_buffer; // jpeg entropy-coded buffer
   int            code_bits;   // number of valid bits
   unsigned char  marker;      // marker seen while filling entropy buffer
   int            nomore;      // flag if we saw a marker so must stop

   int            progressive;
   int            spec_start;
   int            spec_end;
   int            succ_high;
   int            succ_low;
   int            eob_run;
   int            jfif;
   int            app14_color_transform; // Adobe APP14 tag
   int            rgb;

   int scan_n, order[4];
   int restart_interval, todo;

// kernels
   void (*idct_block_kernel)(stbi_uc *out, int out_stride, short data[64]);
   void (*YCbCr_to_RGB_kernel)(stbi_uc *out, const stbi_uc *y, const stbi_uc *pcb, const stbi_uc *pcr, int count, int step);
   stbi_uc *(*resample_row_hv_2_kernel)(stbi_uc *out, stbi_uc *in_near, stbi_uc *in_far, int w, int hs);
} stbi__jpeg;

static int stbi__build_huffman(stbi__huffman *h, int *count)
{
   int i,j,k=0;
   unsigned int code;
   // build size list for each symbol (from JPEG spec)
   for (i=0; i < 16; ++i)
      for (j=0; j < count[i]; ++j)
         h->size[k++] = (stbi_uc) (i+1);
   h->size[k] = 0;

   // compute actual symbols (from jpeg spec)
   code = 0;
   k = 0;
   for(j=1; j <= 16; ++j) {
      // compute delta to add to code to compute symbol id
      h->delta[j] = k - code;
      if (h->size[k] == j) {
         while (h->size[k] == j)
            h->code[k++] = (stbi__uint16) (code++);
         if (code-1 >= (1u << j)) return stbi__err("bad code lengths","Corrupt JPEG");
      }
      // compute largest code + 1 for this size, preshifted as needed later
      h->maxcode[j] = code << (16-j);
      code <<= 1;
   }
   h->maxcode[j] = 0xffffffff;

   // build non-spec acceleration table; 255 is flag for not-accelerated
   memset(h->fast, 255, 1 << FAST_BITS);
   for (i=0; i < k; ++i) {
      int s = h->size[i];
      if (s <= FAST_BITS) {
         int c = h->code[i] << (FAST_BITS-s);
         int m = 1 << (FAST_BITS-s);
         for (j=0; j < m; ++j) {
            h->fast[c+j] = (stbi_uc) i;
         }
      }
   }
   return 1;
}

// build a table that decodes both magnitude and value of small ACs in
// one go.
static void stbi__build_fast_ac(stbi__int16 *fast_ac, stbi__huffman *h)
{
   int i;
   for (i=0; i < (1 << FAST_BITS); ++i) {
      stbi_uc fast = h->fast[i];
      fast_ac[i] = 0;
      if (fast < 255) {
         int rs = h->values[fast];
         int run = (rs >> 4) & 15;
         int magbits = rs & 15;
         int len = h->size[fast];

         if (magbits && len + magbits <= FAST_BITS) {
            // magnitude code followed by receive_extend code
            int k = ((i << len) & ((1 << FAST_BITS) - 1)) >> (FAST_BITS - magbits);
            int m = 1 << (magbits - 1);
            if (k < m) k += (~0U << magbits) + 1;
            // if the result is small enough, we can fit it in fast_ac table
            if (k >= -128 && k <= 127)
               fast_ac[i] = (stbi__int16) ((k * 256) + (run * 16) + (len + magbits));
         }
      }
   }
}

static void stbi__grow_buffer_unsafe(stbi__jpeg *j)
{
   do {
      unsigned int b = j->nomore ? 0 : stbi__get8(j->s);
      if (b == 0xff) {
         int c = stbi__get8(j->s);
         while (c == 0xff) c = stbi__get8(j->s); // consume fill bytes
         if (c != 0) {
            j->marker = (unsigned char) c;
            j->nomore = 1;
            return;
         }
      }
      j->code_buffer |= b << (24 - j->code_bits);
      j->code_bits += 8;
   } while (j->code_bits <= 24);
}

// (1 << n) - 1
static const stbi__uint32 stbi__bmask[17]={0,1,3,7,15,31,63,127,255,511,1023,2047,4095,8191,16383,32767,65535};

// decode a jpeg huffman value from the bitstream
stbi_inline static int stbi__jpeg_huff_decode(stbi__jpeg *j, stbi__huffman *h)
{
   unsigned int temp;
   int c,k;

   if (j->code_bits < 16) stbi__grow_buffer_unsafe(j);

   // look at the top FAST_BITS and determine what symbol ID it is,
   // if the code is <= FAST_BITS
   c = (j->code_buffer >> (32 - FAST_BITS)) & ((1 << FAST_BITS)-1);
   k = h->fast[c];
   if (k < 255) {
      int s = h->size[k];
      if (s > j->code_bits)
         return -1;
      j->code_buffer <<= s;
      j->code_bits -= s;
      return h->values[k];
   }

   // naive test is to shift the code_buffer down so k bits are
   // valid, then test against maxcode. To speed this up, we've
   // preshifted maxcode left so that it has (16-k) 0s at the
   // end; in other words, regardless of the number of bits, it
   // wants to be compared against something shifted to have 16;
   // that way we don't need to shift inside the loop.
   temp = j->code_buffer >> 16;
   for (k=FAST_BITS+1 ; ; ++k)
      if (temp < h->maxcode[k])
         break;
   if (k == 17) {
      // error! code not found
      j->code_bits -= 16;
      return -1;
   }

   if (k > j->code_bits)
      return -1;

   // convert the huffman code to the symbol id
   c = ((j->code_buffer >> (32 - k)) & stbi__bmask[k]) + h->delta[k];
   STBI_ASSERT((((j->code_buffer) >> (32 - h->size[c])) & stbi__bmask[h->size[c]]) == h->code[c]);

   // convert the id to a symbol
   j->code_bits -= k;
   j->code_buffer <<= k;
   return h->values[c];
}

// bias[n] = (-1<<n) + 1
static const int stbi__jbias[16] = {0,-1,-3,-7,-15,-31,-63,-127,-255,-511,-1023,-2047,-4095,-8191,-16383,-32767};

// combined JPEG 'receive' and JPEG 'extend', since baseline
// always extends everything it receives.
stbi_inline static int stbi__extend_receive(stbi__jpeg *j, int n)
{
   unsigned int k;
   int sgn;
   if (j->code_bits < n) stbi__grow_buffer_unsafe(j);

   sgn = (stbi__int32)j->code_buffer >> 31; // sign bit is always in MSB
   k = stbi_lrot(j->code_buffer, n);
   STBI_ASSERT(n >= 0 && n < (int) (sizeof(stbi__bmask)/sizeof(*stbi__bmask)));
   j->code_buffer = k & ~stbi__bmask[n];
   k &= stbi__bmask[n];
   j->code_bits -= n;
   return k + (stbi__jbias[n] & ~sgn);
}

// get some unsigned bits
stbi_inline static int stbi__jpeg_get_bits(stbi__jpeg *j, int n)
{
   unsigned int k;
   if (j->code_bits < n) stbi__grow_buffer_unsafe(j);
   k = stbi_lrot(j->code_buffer, n);
   j->code_buffer = k & ~stbi__bmask[n];
   k &= stbi__bmask[n];
   j->code_bits -= n;
   return k;
}

stbi_inline static int stbi__jpeg_get_bit(stbi__jpeg *j)
{
   unsigned int k;
   if (j->code_bits < 1) stbi__grow_buffer_unsafe(j);
   k = j->code_buffer;
   j->code_buffer <<= 1;
   --j->code_bits;
   return k & 0x80000000;
}

// given a value that's at position X in the zigzag stream,
// where does it appear in the 8x8 matrix coded as row-major?
static const stbi_uc stbi__jpeg_dezigzag[64+15] =
{
    0,  1,  8, 16,  9,  2,  3, 10,
   17, 24, 32, 25, 18, 11,  4,  5,
   12, 19, 26, 33, 40, 48, 41, 34,
   27, 20, 13,  6,  7, 14, 21, 28,
   35, 42, 49, 56, 57, 50, 43, 36,
   29, 22, 15, 23, 30, 37, 44, 51,
   58, 59, 52, 45, 38, 31, 39, 46,
   53, 60, 61, 54, 47, 55, 62, 63,
   // let corrupt input sample past end
   63, 63, 63, 63, 63, 63, 63, 63,
   63, 63, 63, 63, 63, 63, 63
};

// decode one 64-entry block--
static int stbi__jpeg_decode_block(stbi__jpeg *j, short data[64], stbi__huffman *hdc, stbi__huffman *hac, stbi__int16 *fac, int b, stbi__uint16 *dequant)
{
   int diff,dc,k;
   int t;

   if (j->code_bits < 16) stbi__grow_buffer_unsafe(j);
   t = stbi__jpeg_huff_decode(j, hdc);
   if (t < 0) return stbi__err("bad huffman code","Corrupt JPEG");

   // 0 all the ac values now so we can do it 32-bits at a time
   memset(data,0,64*sizeof(data[0]));

   diff = t ? stbi__extend_receive(j, t) : 0;
   dc = j->img_comp[b].dc_pred + diff;
   j->img_comp[b].dc_pred = dc;
   data[0] = (short) (dc * dequant[0]);

   // decode AC components, see JPEG spec
   k = 1;
   do {
      unsigned int zig;
      int c,r,s;
      if (j->code_bits < 16) stbi__grow_buffer_unsafe(j);
      c = (j->code_buffer >> (32 - FAST_BITS)) & ((1 << FAST_BITS)-1);
      r = fac[c];
      if (r) { // fast-AC path
         k += (r >> 4) & 15; // run
         s = r & 15; // combined length
         j->code_buffer <<= s;
         j->code_bits -= s;
         // decode into unzigzag'd location
         zig = stbi__jpeg_dezigzag[k++];
         data[zig] = (short) ((r >> 8) * dequant[zig]);
      } else {
         int rs = stbi__jpeg_huff_decode(j, hac);
         if (rs < 0) return stbi__err("bad huffman code","Corrupt JPEG");
         s = rs & 15;
         r = rs >> 4;
         if (s == 0) {
            if (rs != 0xf0) break; // end block
            k += 16;
         } else {
            k += r;
            // decode into unzigzag'd location
            zig = stbi__jpeg_dezigzag[k++];
            data[zig] = (short) (stbi__extend_receive(j,s) * dequant[zig]);
         }
      }
   } while (k < 64);
   return 1;
}

static int stbi__jpeg_decode_block_prog_dc(stbi__jpeg *j, short data[64], stbi__huffman *hdc, int b)
{
   int diff,dc;
   int t;
   if (j->spec_end != 0) return stbi__err("can't merge dc and ac", "Corrupt JPEG");

   if (j->code_bits < 16) stbi__grow_buffer_unsafe(j);

   if (j->succ_high == 0) {
      // first scan for DC coefficient, must be first
      memset(data,0,64*sizeof(data[0])); // 0 all the ac values now
      t = stbi__jpeg_huff_decode(j, hdc);
      diff = t ? stbi__extend_receive(j, t) : 0;

      dc = j->img_comp[b].dc_pred + diff;
      j->img_comp[b].dc_pred = dc;
      data[0] = (short) (dc << j->succ_low);
   } else {
      // refinement scan for DC coefficient
      if (stbi__jpeg_get_bit(j))
         data[0] += (short) (1 << j->succ_low);
   }
   return 1;
}

// @OPTIMIZE: store non-zigzagged during the decode passes,
// and only de-zigzag when dequantizing
static int stbi__jpeg_decode_block_prog_ac(stbi__jpeg *j, short data[64], stbi__huffman *hac, stbi__int16 *fac)
{
   int k;
   if (j->spec_start == 0) return stbi__err("can't merge dc and ac", "Corrupt JPEG");

   if (j->succ_high == 0) {
      int shift = j->succ_low;

      if (j->eob_run) {
         --j->eob_run;
         return 1;
      }

      k = j->spec_start;
      do {
         unsigned int zig;
         int c,r,s;
         if (j->code_bits < 16) stbi__grow_buffer_unsafe(j);
         c = (j->code_buffer >> (32 - FAST_BITS)) & ((1 << FAST_BITS)-1);
         r = fac[c];
         if (r) { // fast-AC path
            k += (r >> 4) & 15; // run
            s = r & 15; // combined length
            j->code_buffer <<= s;
            j->code_bits -= s;
            zig = stbi__jpeg_dezigzag[k++];
            data[zig] = (short) ((r >> 8) << shift);
         } else {
            int rs = stbi__jpeg_huff_decode(j, hac);
            if (rs < 0) return stbi__err("bad huffman code","Corrupt JPEG");
            s = rs & 15;
            r = rs >> 4;
            if (s == 0) {
               if (r < 15) {
                  j->eob_run = (1 << r);
                  if (r)
                     j->eob_run += stbi__jpeg_get_bits(j, r);
                  --j->eob_run;
                  break;
               }
               k += 16;
            } else {
               k += r;
               zig = stbi__jpeg_dezigzag[k++];
               data[zig] = (short) (stbi__extend_receive(j,s) << shift);
            }
         }
      } while (k <= j->spec_end);
   } else {
      // refinement scan for these AC coefficients

      short bit = (short) (1 << j->succ_low);

      if (j->eob_run) {
         --j->eob_run;
         for (k = j->spec_start; k <= j->spec_end; ++k) {
            short *p = &data[stbi__jpeg_dezigzag[k]];
            if (*p != 0)
               if (stbi__jpeg_get_bit(j))
                  if ((*p & bit)==0) {
                     if (*p > 0)
                        *p += bit;
                     else
                        *p -= bit;
                  }
         }
      } else {
         k = j->spec_start;
         do {
            int r,s;
            int rs = stbi__jpeg_huff_decode(j, hac); // @OPTIMIZE see if we can use the fast path here, advance-by-r is so slow, eh
            if (rs < 0) return stbi__err("bad huffman code","Corrupt JPEG");
            s = rs & 15;
            r = rs >> 4;
            if (s == 0) {
               if (r < 15) {
                  j->eob_run = (1 << r) - 1;
                  if (r)
                     j->eob_run += stbi__jpeg_get_bits(j, r);
                  r = 64; // force end of block
               } else {
                  // r=15 s=0 should write 16 0s, so we just do
                  // a run of 15 0s and then write s (which is 0),
                  // so we don't have to do anything special here
               }
            } else {
               if (s != 1) return stbi__err("bad huffman code", "Corrupt JPEG");
               // sign bit
               if (stbi__jpeg_get_bit(j))
                  s = bit;
               else
                  s = -bit;
            }

            // advance by r
            while (k <= j->spec_end) {
               short *p = &data[stbi__jpeg_dezigzag[k++]];
               if (*p != 0) {
                  if (stbi__jpeg_get_bit(j))
                     if ((*p & bit)==0) {
                        if (*p > 0)
                           *p += bit;
                        else
                           *p -= bit;
                     }
               } else {
                  if (r == 0) {
                     *p = (short) s;
                     break;
                  }
                  --r;
               }
            }
         } while (k <= j->spec_end);
      }
   }
   return 1;
}

// take a -128..127 value and stbi__clamp it and convert to 0..255
stbi_inline static stbi_uc stbi__clamp(int x)
{
   // trick to use a single test to catch both cases
   if ((unsigned int) x > 255) {
      if (x < 0) return 0;
      if (x > 255) return 255;
   }
   return (stbi_uc) x;
}

#define stbi__f2f(x)  ((int) (((x) * 4096 + 0.5)))
#define stbi__fsh(x)  ((x) * 4096)

// derived from jidctint -- DCT_ISLOW
#define STBI__IDCT_1D(s0,s1,s2,s3,s4,s5,s6,s7) \
   int t0,t1,t2,t3,p1,p2,p3,p4,p5,x0,x1,x2,x3; \
   p2 = s2;                                    \
   p3 = s6;                                    \
   p1 = (p2+p3) * stbi__f2f(0.5411961f);       \
   t2 = p1 + p3*stbi__f2f(-1.847759065f);      \
   t3 = p1 + p2*stbi__f2f( 0.765366865f);      \
   p2 = s0;                                    \
   p3 = s4;                                    \
   t0 = stbi__fsh(p2+p3);                      \
   t1 = stbi__fsh(p2-p3);                      \
   x0 = t0+t3;                                 \
   x3 = t0-t3;                                 \
   x1 = t1+t2;                                 \
   x2 = t1-t2;                                 \
   t0 = s7;                                    \
   t1 = s5;                                    \
   t2 = s3;                                    \
   t3 = s1;                                    \
   p3 = t0+t2;                                 \
   p4 = t1+t3;                                 \
   p1 = t0+t3;                                 \
   p2 = t1+t2;                                 \
   p5 = (p3+p4)*stbi__f2f( 1.175875602f);      \
   t0 = t0*stbi__f2f( 0.298631336f);           \
   t1 = t1*stbi__f2f( 2.053119869f);           \
   t2 = t2*stbi__f2f( 3.072711026f);           \
   t3 = t3*stbi__f2f( 1.501321110f);           \
   p1 = p5 + p1*stbi__f2f(-0.899976223f);      \
   p2 = p5 + p2*stbi__f2f(-2.562915447f);      \
   p3 = p3*stbi__f2f(-1.961570560f);           \
   p4 = p4*stbi__f2f(-0.390180644f);           \
   t3 += p1+p4;                                \
   t2 += p2+p3;                                \
   t1 += p2+p4;                                \
   t0 += p1+p3;

static void stbi__idct_block(stbi_uc *out, int out_stride, short data[64])
{
   int i,val[64],*v=val;
   stbi_uc *o;
   short *d = data;

   // columns
   for (i=0; i < 8; ++i,++d, ++v) {
      // if all zeroes, shortcut -- this avoids dequantizing 0s and IDCTing
      if (d[ 8]==0 && d[16]==0 && d[24]==0 && d[32]==0
           && d[40]==0 && d[48]==0 && d[56]==0) {
         //    no shortcut                 0     seconds
         //    (1|2|3|4|5|6|7)==0          0     seconds
         //    all separate               -0.047 seconds
         //    1 && 2|3 && 4|5 && 6|7:    -0.047 seconds
         int dcterm = d[0]*4;
         v[0] = v[8] = v[16] = v[24] = v[32] = v[40] = v[48] = v[56] = dcterm;
      } else {
         STBI__IDCT_1D(d[ 0],d[ 8],d[16],d[24],d[32],d[40],d[48],d[56])
         // constants scaled things up by 1<<12; let's bring them back
         // down, but keep 2 extra bits of precision
         x0 += 512; x1 += 512; x2 += 512; x3 += 512;
         v[ 0] = (x0+t3) >> 10;
         v[56] = (x0-t3) >> 10;
         v[ 8] = (x1+t2) >> 10;
         v[48] = (x1-t2) >> 10;
         v[16] = (x2+t1) >> 10;
         v[40] = (x2-t1) >> 10;
         v[24] = (x3+t0) >> 10;
         v[32] = (x3-t0) >> 10;
      }
   }

   for (i=0, v=val, o=out; i < 8; ++i,v+=8,o+=out_stride) {
      // no fast case since the first 1D IDCT spread components out
      STBI__IDCT_1D(v[0],v[1],v[2],v[3],v[4],v[5],v[6],v[7])
      // constants scaled things up by 1<<12, plus we had 1<<2 from first
      // loop, plus horizontal and vertical each scale by sqrt(8) so together
      // we've got an extra 1<<3, so 1<<17 total we need to remove.
      // so we want to round that, which means adding 0.5 * 1<<17,
      // aka 65536. Also, we'll end up with -128 to 127 that we want
      // to encode as 0..255 by adding 128, so we'll add that before the shift
      x0 += 65536 + (128<<17);
      x1 += 65536 + (128<<17);
      x2 += 65536 + (128<<17);
      x3 += 65536 + (128<<17);
      // tried computing the shifts into temps, or'ing the temps to see
      // if any were out of range, but that was slower
      o[0] = stbi__clamp((x0+t3) >> 17);
      o[7] = stbi__clamp((x0-t3) >> 17);
      o[1] = stbi__clamp((x1+t2) >> 17);
      o[6] = stbi__clamp((x1-t2) >> 17);
      o[2] = stbi__clamp((x2+t1) >> 17);
      o[5] = stbi__clamp((x2-t1) >> 17);
      o[3] = stbi__clamp((x3+t0) >> 17);
      o[4] = stbi__clamp((x3-t0) >> 17);
   }
}

#ifdef STBI_SSE2
// sse2 integer IDCT. not the fastest possible implementation but it
// produces bit-identical results to the generic C version so it's
// fully "transparent".
static void stbi__idct_simd(stbi_uc *out, int out_stride, short data[64])
{
   // This is constructed to match our regular (generic) integer IDCT exactly.
   __m128i row0, row1, row2, row3, row4, row5, row6, row7;
   __m128i tmp;

   // dot product constant: even elems=x, odd elems=y
   #define dct_const(x,y)  _mm_setr_epi16((x),(y),(x),(y),(x),(y),(x),(y))

   // out(0) = c0[even]*x + c0[odd]*y   (c0, x, y 16-bit, out 32-bit)
   // out(1) = c1[even]*x + c1[odd]*y
   #define dct_rot(out0,out1, x,y,c0,c1) \
      __m128i c0##lo = _mm_unpacklo_epi16((x),(y)); \
      __m128i c0##hi = _mm_unpackhi_epi16((x),(y)); \
      __m128i out0##_l = _mm_madd_epi16(c0##lo, c0); \
      __m128i out0##_h = _mm_madd_epi16(c0##hi, c0); \
      __m128i out1##_l = _mm_madd_epi16(c0##lo, c1); \
      __m128i out1##_h = _mm_madd_epi16(c0##hi, c1)

   // out = in << 12  (in 16-bit, out 32-bit)
   #define dct_widen(out, in) \
      __m128i out##_l = _mm_srai_epi32(_mm_unpacklo_epi16(_mm_setzero_si128(), (in)), 4); \
      __m128i out##_h = _mm_srai_epi32(_mm_unpackhi_epi16(_mm_setzero_si128(), (in)), 4)

   // wide add
   #define dct_wadd(out, a, b) \
      __m128i out##_l = _mm_add_epi32(a##_l, b##_l); \
      __m128i out##_h = _mm_add_epi32(a##_h, b##_h)

   // wide sub
   #define dct_wsub(out, a, b) \
      __m128i out##_l = _mm_sub_epi32(a##_l, b##_l); \
      __m128i out##_h = _mm_sub_epi32(a##_h, b##_h)

   // butterfly a/b, add bias, then shift by "s" and pack
   #define dct_bfly32o(out0, out1, a,b,bias,s) \
      { \
         __m128i abiased_l = _mm_add_epi32(a##_l, bias); \
         __m128i abiased_h = _mm_add_epi32(a##_h, bias); \
         dct_wadd(sum, abiased, b); \
         dct_wsub(dif, abiased, b); \
         out0 = _mm_packs_epi32(_mm_srai_epi32(sum_l, s), _mm_srai_epi32(sum_h, s)); \
         out1 = _mm_packs_epi32(_mm_srai_epi32(dif_l, s), _mm_srai_epi32(dif_h, s)); \
      }

   // 8-bit interleave step (for transposes)
   #define dct_interleave8(a, b) \
      tmp = a; \
      a = _mm_unpacklo_epi8(a, b); \
      b = _mm_unpackhi_epi8(tmp, b)

   // 16-bit interleave step (for transposes)
   #define dct_interleave16(a, b) \
      tmp = a; \
      a = _mm_unpacklo_epi16(a, b); \
      b = _mm_unpackhi_epi16(tmp, b)

   #define dct_pass(bias,shift) \
      { \
         /* even part */ \
         dct_rot(t2e,t3e, row2,row6, rot0_0,rot0_1); \
         __m128i sum04 = _mm_add_epi16(row0, row4); \
         __m128i dif04 = _mm_sub_epi16(row0, row4); \
         dct_widen(t0e, sum04); \
         dct_widen(t1e, dif04); \
         dct_wadd(x0, t0e, t3e); \
         dct_wsub(x3, t0e, t3e); \
         dct_wadd(x1, t1e, t2e); \
         dct_wsub(x2, t1e, t2e); \
         /* odd part */ \
         dct_rot(y0o,y2o, row7,row3, rot2_0,rot2_1); \
         dct_rot(y1o,y3o, row5,row1, rot3_0,rot3_1); \
         __m128i sum17 = _mm_add_epi16(row1, row7); \
         __m128i sum35 = _mm_add_epi16(row3, row5); \
         dct_rot(y4o,y5o, sum17,sum35, rot1_0,rot1_1); \
         dct_wadd(x4, y0o, y4o); \
         dct_wadd(x5, y1o, y5o); \
         dct_wadd(x6, y2o, y5o); \
         dct_wadd(x7, y3o, y4o); \
         dct_bfly32o(row0,row7, x0,x7,bias,shift); \
         dct_bfly32o(row1,row6, x1,x6,bias,shift); \
         dct_bfly32o(row2,row5, x2,x5,bias,shift); \
         dct_bfly32o(row3,row4, x3,x4,bias,shift); \
      }

   __m128i rot0_0 = dct_const(stbi__f2f(0.5411961f), stbi__f2f(0.5411961f) + stbi__f2f(-1.847759065f));
   __m128i rot0_1 = dct_const(stbi__f2f(0.5411961f) + stbi__f2f( 0.765366865f), stbi__f2f(0.5411961f));
   __m128i rot1_0 = dct_const(stbi__f2f(1.175875602f) + stbi__f2f(-0.899976223f), stbi__f2f(1.175875602f));
   __m128i rot1_1 = dct_const(stbi__f2f(1.175875602f), stbi__f2f(1.175875602f) + stbi__f2f(-2.562915447f));
   __m128i rot2_0 = dct_const(stbi__f2f(-1.961570560f) + stbi__f2f( 0.298631336f), stbi__f2f(-1.961570560f));
   __m128i rot2_1 = dct_const(stbi__f2f(-1.961570560f), stbi__f2f(-1.961570560f) + stbi__f2f( 3.072711026f));
   __m128i rot3_0 = dct_const(stbi__f2f(-0.390180644f) + stbi__f2f( 2.053119869f), stbi__f2f(-0.390180644f));
   __m128i rot3_1 = dct_const(stbi__f2f(-0.390180644f), stbi__f2f(-0.390180644f) + stbi__f2f( 1.501321110f));

   // rounding biases in column/row passes, see stbi__idct_block for explanation.
   __m128i bias_0 = _mm_set1_epi32(512);
   __m128i bias_1 = _mm_set1_epi32(65536 + (128<<17));

   // load
   row0 = _mm_load_si128((const __m128i *) (data + 0*8));
   row1 = _mm_load_si128((const __m128i *) (data + 1*8));
   row2 = _mm_load_si128((const __m128i *) (data + 2*8));
   row3 = _mm_load_si128((const __m128i *) (data + 3*8));
   row4 = _mm_load_si128((const __m128i *) (data + 4*8));
   row5 = _mm_load_si128((const __m128i *) (data + 5*8));
   row6 = _mm_load_si128((const __m128i *) (data + 6*8));
   row7 = _mm_load_si128((const __m128i *) (data + 7*8));

   // column pass
   dct_pass(bias_0, 10);

   {
      // 16bit 8x8 transpose pass 1
      dct_interleave16(row0, row4);
      dct_interleave16(row1, row5);
      dct_interleave16(row2, row6);
      dct_interleave16(row3, row7);

      // transpose pass 2
      dct_interleave16(row0, row2);
      dct_interleave16(row1, row3);
      dct_interleave16(row4, row6);
      dct_interleave16(row5, row7);

      // transpose pass 3
      dct_interleave16(row0, row1);
      dct_interleave16(row2, row3);
      dct_interleave16(row4, row5);
      dct_interleave16(row6, row7);
   }

   // row pass
   dct_pass(bias_1, 17);

   {
      // pack
      __m128i p0 = _mm_packus_epi16(row0, row1); // a0a1a2a3...a7b0b1b2b3...b7
      __m128i p1 = _mm_packus_epi16(row2, row3);
      __m128i p2 = _mm_packus_epi16(row4, row5);
      __m128i p3 = _mm_packus_epi16(row6, row7);

      // 8bit 8x8 transpose pass 1
      dct_interleave8(p0, p2); // a0e0a1e1...
      dct_interleave8(p1, p3); // c0g0c1g1...

      // transpose pass 2
      dct_interleave8(p0, p1); // a0c0e0g0...
      dct_interleave8(p2, p3); // b0d0f0h0...

      // transpose pass 3
      dct_interleave8(p0, p2); // a0b0c0d0...
      dct_interleave8(p1, p3); // a4b4c4d4...

      // store
      _mm_storel_epi64((__m128i *) out, p0); out += out_stride;
      _mm_storel_epi64((__m128i *) out, _mm_shuffle_epi32(p0, 0x4e)); out += out_stride;
      _mm_storel_epi64((__m128i *) out, p2); out += out_stride;
      _mm_storel_epi64((__m128i *) out, _mm_shuffle_epi32(p2, 0x4e)); out += out_stride;
      _mm_storel_epi64((__m128i *) out, p1); out += out_stride;
      _mm_storel_epi64((__m128i *) out, _mm_shuffle_epi32(p1, 0x4e)); out += out_stride;
      _mm_storel_epi64((__m128i *) out, p3); out += out_stride;
      _mm_storel_epi64((__m128i *) out, _mm_shuffle_epi32(p3, 0x4e));
   }

#undef dct_const
#undef dct_rot
#undef dct_widen
#undef dct_wadd
#undef dct_wsub
#undef dct_bfly32o
#undef dct_interleave8
#undef dct_interleave16
#undef dct_pass
}

#endif // STBI_SSE2

#ifdef STBI_NEON

// NEON integer IDCT. should produce bit-identical
// results to the generic C version.
static void stbi__idct_simd(stbi_uc *out, int out_stride, short data[64])
{
   int16x8_t row0, row1, row2, row3, row4, row5, row6, row7;

   int16x4_t rot0_0 = vdup_n_s16(stbi__f2f(0.5411961f));
   int16x4_t rot0_1 = vdup_n_s16(stbi__f2f(-1.847759065f));
   int16x4_t rot0_2 = vdup_n_s16(stbi__f2f( 0.765366865f));
   int16x4_t rot1_0 = vdup_n_s16(stbi__f2f( 1.175875602f));
   int16x4_t rot1_1 = vdup_n_s16(stbi__f2f(-0.899976223f));
   int16x4_t rot1_2 = vdup_n_s16(stbi__f2f(-2.562915447f));
   int16x4_t rot2_0 = vdup_n_s16(stbi__f2f(-1.961570560f));
   int16x4_t rot2_1 = vdup_n_s16(stbi__f2f(-0.390180644f));
   int16x4_t rot3_0 = vdup_n_s16(stbi__f2f( 0.298631336f));
   int16x4_t rot3_1 = vdup_n_s16(stbi__f2f( 2.053119869f));
   int16x4_t rot3_2 = vdup_n_s16(stbi__f2f( 3.072711026f));
   int16x4_t rot3_3 = vdup_n_s16(stbi__f2f( 1.501321110f));

#define dct_long_mul(out, inq, coeff) \
   int32x4_t out##_l = vmull_s16(vget_low_s16(inq), coeff); \
   int32x4_t out##_h = vmull_s16(vget_high_s16(inq), coeff)

#define dct_long_mac(out, acc, inq, coeff) \
   int32x4_t out##_l = vmlal_s16(acc##_l, vget_low_s16(inq), coeff); \
   int32x4_t out##_h = vmlal_s16(acc##_h, vget_high_s16(inq), coeff)

#define dct_widen(out, inq) \
   int32x4_t out##_l = vshll_n_s16(vget_low_s16(inq), 12); \
   int32x4_t out##_h = vshll_n_s16(vget_high_s16(inq), 12)

// wide add
#define dct_wadd(out, a, b) \
   int32x4_t out##_l = vaddq_s32(a##_l, b##_l); \
   int32x4_t out##_h = vaddq_s32(a##_h, b##_h)

// wide sub
#define dct_wsub(out, a, b) \
   int32x4_t out##_l = vsubq_s32(a##_l, b##_l); \
   int32x4_t out##_h = vsubq_s32(a##_h, b##_h)

// butterfly a/b, then shift using "shiftop" by "s" and pack
#define dct_bfly32o(out0,out1, a,b,shiftop,s) \
   { \
      dct_wadd(sum, a, b); \
      dct_wsub(dif, a, b); \
      out0 = vcombine_s16(shiftop(sum_l, s), shiftop(sum_h, s)); \
      out1 = vcombine_s16(shiftop(dif_l, s), shiftop(dif_h, s)); \
   }

#define dct_pass(shiftop, shift) \
   { \
      /* even part */ \
      int16x8_t sum26 = vaddq_s16(row2, row6); \
      dct_long_mul(p1e, sum26, rot0_0); \
      dct_long_mac(t2e, p1e, row6, rot0_1); \
      dct_long_mac(t3e, p1e, row2, rot0_2); \
      int16x8_t sum04 = vaddq_s16(row0, row4); \
      int16x8_t dif04 = vsubq_s16(row0, row4); \
      dct_widen(t0e, sum04); \
      dct_widen(t1e, dif04); \
      dct_wadd(x0, t0e, t3e); \
      dct_wsub(x3, t0e, t3e); \
      dct_wadd(x1, t1e, t2e); \
      dct_wsub(x2, t1e, t2e); \
      /* odd part */ \
      int16x8_t sum15 = vaddq_s16(row1, row5); \
      int16x8_t sum17 = vaddq_s16(row1, row7); \
      int16x8_t sum35 = vaddq_s16(row3, row5); \
      int16x8_t sum37 = vaddq_s16(row3, row7); \
      int16x8_t sumodd = vaddq_s16(sum17, sum35); \
      dct_long_mul(p5o, sumodd, rot1_0); \
      dct_long_mac(p1o, p5o, sum17, rot1_1); \
      dct_long_mac(p2o, p5o, sum35, rot1_2); \
      dct_long_mul(p3o, sum37, rot2_0); \
      dct_long_mul(p4o, sum15, rot2_1); \
      dct_wadd(sump13o, p1o, p3o); \
      dct_wadd(sump24o, p2o, p4o); \
      dct_wadd(sump23o, p2o, p3o); \
      dct_wadd(sump14o, p1o, p4o); \
      dct_long_mac(x4, sump13o, row7, rot3_0); \
      dct_long_mac(x5, sump24o, row5, rot3_1); \
      dct_long_mac(x6, sump23o, row3, rot3_2); \
      dct_long_mac(x7, sump14o, row1, rot3_3); \
      dct_bfly32o(row0,row7, x0,x7,shiftop,shift); \
      dct_bfly32o(row1,row6, x1,x6,shiftop,shift); \
      dct_bfly32o(row2,row5, x2,x5,shiftop,shift); \
      dct_bfly32o(row3,row4, x3,x4,shiftop,shift); \
   }

   // load
   row0 = vld1q_s16(data + 0*8);
   row1 = vld1q_s16(data + 1*8);
   row2 = vld1q_s16(data + 2*8);
   row3 = vld1q_s16(data + 3*8);
   row4 = vld1q_s16(data + 4*8);
   row5 = vld1q_s16(data + 5*8);
   row6 = vld1q_s16(data + 6*8);
   row7 = vld1q_s16(data + 7*8);

   // add DC bias
   row0 = vaddq_s16(row0, vsetq_lane_s16(1024, vdupq_n_s16(0), 0));

   // column pass
   dct_pass(vrshrn_n_s32, 10);

   // 16bit 8x8 transpose
   {
// these three map to a single VTRN.16, VTRN.32, and VSWP, respectively.
// whether compilers actually get this is another story, sadly.
#define dct_trn16(x, y) { int16x8x2_t t = vtrnq_s16(x, y); x = t.val[0]; y = t.val[1]; }
#define dct_trn32(x, y) { int32x4x2_t t = vtrnq_s32(vreinterpretq_s32_s16(x), vreinterpretq_s32_s16(y)); x = vreinterpretq_s16_s32(t.val[0]); y = vreinterpretq_s16_s32(t.val[1]); }
#define dct_trn64(x, y) { int16x8_t x0 = x; int16x8_t y0 = y; x = vcombine_s16(vget_low_s16(x0), vget_low_s16(y0)); y = vcombine_s16(vget_high_s16(x0), vget_high_s16(y0)); }

      // pass 1
      dct_trn16(row0, row1); // a0b0a2b2a4b4a6b6
      dct_trn16(row2, row3);
      dct_trn16(row4, row5);
      dct_trn16(row6, row7);

      // pass 2
      dct_trn32(row0, row2); // a0b0c0d0a4b4c4d4
      dct_trn32(row1, row3);
      dct_trn32(row4, row6);
      dct_trn32(row5, row7);

      // pass 3
      dct_trn64(row0, row4); // a0b0c0d0e0f0g0h0
      dct_trn64(row1, row5);
      dct_trn64(row2, row6);
      dct_trn64(row3, row7);

#undef dct_trn16
#undef dct_trn32
#undef dct_trn64
   }

   // row pass
   // vrshrn_n_s32 only supports shifts up to 16, we need
   // 17. so do a non-rounding shift of 16 first then follow
   // up with a rounding shift by 1.
   dct_pass(vshrn_n_s32, 16);

   {
      // pack and round
      uint8x8_t p0 = vqrshrun_n_s16(row0, 1);
      uint8x8_t p1 = vqrshrun_n_s16(row1, 1);
      uint8x8_t p2 = vqrshrun_n_s16(row2, 1);
      uint8x8_t p3 = vqrshrun_n_s16(row3, 1);
      uint8x8_t p4 = vqrshrun_n_s16(row4, 1);
      uint8x8_t p5 = vqrshrun_n_s16(row5, 1);
      uint8x8_t p6 = vqrshrun_n_s16(row6, 1);
      uint8x8_t p7 = vqrshrun_n_s16(row7, 1);

      // again, these can translate into one instruction, but often don't.
#define dct_trn8_8(x, y) { uint8x8x2_t t = vtrn_u8(x, y); x = t.val[0]; y = t.val[1]; }
#define dct_trn8_16(x, y) { uint16x4x2_t t = vtrn_u16(vreinterpret_u16_u8(x), vreinterpret_u16_u8(y)); x = vreinterpret_u8_u16(t.val[0]); y = vreinterpret_u8_u16(t.val[1]); }
#define dct_trn8_32(x, y) { uint32x2x2_t t = vtrn_u32(vreinterpret_u32_u8(x), vreinterpret_u32_u8(y)); x = vreinterpret_u8_u32(t.val[0]); y = vreinterpret_u8_u32(t.val[1]); }

      // sadly can't use interleaved stores here since we only write
      // 8 bytes to each scan line!

      // 8x8 8-bit transpose pass 1
      dct_trn8_8(p0, p1);
      dct_trn8_8(p2, p3);
      dct_trn8_8(p4, p5);
      dct_trn8_8(p6, p7);

      // pass 2
      dct_trn8_16(p0, p2);
      dct_trn8_16(p1, p3);
      dct_trn8_16(p4, p6);
      dct_trn8_16(p5, p7);

      // pass 3
      dct_trn8_32(p0, p4);
      dct_trn8_32(p1, p5);
      dct_trn8_32(p2, p6);
      dct_trn8_32(p3, p7);

      // store
      vst1_u8(out, p0); out += out_stride;
      vst1_u8(out, p1); out += out_stride;
      vst1_u8(out, p2); out += out_stride;
      vst1_u8(out, p3); out += out_stride;
      vst1_u8(out, p4); out += out_stride;
      vst1_u8(out, p5); out += out_stride;
      vst1_u8(out, p6); out += out_stride;
      vst1_u8(out, p7);

#undef dct_trn8_8
#undef dct_trn8_16
#undef dct_trn8_32
   }

#undef dct_long_mul
#undef dct_long_mac
#undef dct_widen
#undef dct_wadd
#undef dct_wsub
#undef dct_bfly32o
#undef dct_pass
}

#endif // STBI_NEON

#define STBI__MARKER_none  0xff
// if there's a pending marker from the entropy stream, return that
// otherwise, fetch from the stream and get a marker. if there's no
// marker, return 0xff, which is never a valid marker value
static stbi_uc stbi__get_marker(stbi__jpeg *j)
{
   stbi_uc x;
   if (j->marker != STBI__MARKER_none) { x = j->marker; j->marker = STBI__MARKER_none; return x; }
   x = stbi__get8(j->s);
   if (x != 0xff) return STBI__MARKER_none;
   while (x == 0xff)
      x = stbi__get8(j->s); // consume repeated 0xff fill bytes
   return x;
}

// in each scan, we'll have scan_n components, and the order
// of the components is specified by order[]
#define STBI__RESTART(x)     ((x) >= 0xd0 && (x) <= 0xd7)

// after a restart interval, stbi__jpeg_reset the entropy decoder and
// the dc prediction
static void stbi__jpeg_reset(stbi__jpeg *j)
{
   j->code_bits = 0;
   j->code_buffer = 0;
   j->nomore = 0;
   j->img_comp[0].dc_pred = j->img_comp[1].dc_pred = j->img_comp[2].dc_pred = j->img_comp[3].dc_pred = 0;
   j->marker = STBI__MARKER_none;
   j->todo = j->restart_interval ? j->restart_interval : 0x7fffffff;
   j->eob_run = 0;
   // no more than 1<<31 MCUs if no restart_interal? that's plenty safe,
   // since we don't even allow 1<<30 pixels
}

static int stbi__parse_entropy_coded_data(stbi__jpeg *z)
{
   stbi__jpeg_reset(z);
   if (!z->progressive) {
      if (z->scan_n == 1) {
         int i,j;
         STBI_SIMD_ALIGN(short, data[64]);
         int n = z->order[0];
         // non-interleaved data, we just need to process one block at a time,
         // in trivial scanline order
         // number of blocks to do just depends on how many actual "pixels" this
         // component has, independent of interleaved MCU blocking and such
         int w = (z->img_comp[n].x+7) >> 3;
         int h = (z->img_comp[n].y+7) >> 3;
         for (j=0; j < h; ++j) {
            for (i=0; i < w; ++i) {
               int ha = z->img_comp[n].ha;
               if (!stbi__jpeg_decode_block(z, data, z->huff_dc+z->img_comp[n].hd, z->huff_ac+ha, z->fast_ac[ha], n, z->dequant[z->img_comp[n].tq])) return 0;
               z->idct_block_kernel(z->img_comp[n].data+z->img_comp[n].w2*j*8+i*8, z->img_comp[n].w2, data);
               // every data block is an MCU, so countdown the restart interval
               if (--z->todo <= 0) {
                  if (z->code_bits < 24) stbi__grow_buffer_unsafe(z);
                  // if it's NOT a restart, then just bail, so we get corrupt data
                  // rather than no data
                  if (!STBI__RESTART(z->marker)) return 1;
                  stbi__jpeg_reset(z);
               }
            }
         }
         return 1;
      } else { // interleaved
         int i,j,k,x,y;
         STBI_SIMD_ALIGN(short, data[64]);
         for (j=0; j < z->img_mcu_y; ++j) {
            for (i=0; i < z->img_mcu_x; ++i) {
               // scan an interleaved mcu... process scan_n components in order
               for (k=0; k < z->scan_n; ++k) {
                  int n = z->order[k];
                  // scan out an mcu's worth of this component; that's just determined
                  // by the basic H and V specified for the component
                  for (y=0; y < z->img_comp[n].v; ++y) {
                     for (x=0; x < z->img_comp[n].h; ++x) {
                        int x2 = (i*z->img_comp[n].h + x)*8;
                        int y2 = (j*z->img_comp[n].v + y)*8;
                        int ha = z->img_comp[n].ha;
                        if (!stbi__jpeg_decode_block(z, data, z->huff_dc+z->img_comp[n].hd, z->huff_ac+ha, z->fast_ac[ha], n, z->dequant[z->img_comp[n].tq])) return 0;
                        z->idct_block_kernel(z->img_comp[n].data+z->img_comp[n].w2*y2+x2, z->img_comp[n].w2, data);
                     }
                  }
               }
               // after all interleaved components, that's an interleaved MCU,
               // so now count down the restart interval
               if (--z->todo <= 0) {
                  if (z->code_bits < 24) stbi__grow_buffer_unsafe(z);
                  if (!STBI__RESTART(z->marker)) return 1;
                  stbi__jpeg_reset(z);
               }
            }
         }
         return 1;
      }
   } else {
      if (z->scan_n == 1) {
         int i,j;
         int n = z->order[0];
         // non-interleaved data, we just need to process one block at a time,
         // in trivial scanline order
         // number of blocks to do just depends on how many actual "pixels" this
         // component has, independent of interleaved MCU blocking and such
         int w = (z->img_comp[n].x+7) >> 3;
         int h = (z->img_comp[n].y+7) >> 3;
         for (j=0; j < h; ++j) {
            for (i=0; i < w; ++i) {
               short *data = z->img_comp[n].coeff + 64 * (i + j * z->img_comp[n].coeff_w);
               if (z->spec_start == 0) {
                  if (!stbi__jpeg_decode_block_prog_dc(z, data, &z->huff_dc[z->img_comp[n].hd], n))
                     return 0;
               } else {
                  int ha = z->img_comp[n].ha;
                  if (!stbi__jpeg_decode_block_prog_ac(z, data, &z->huff_ac[ha], z->fast_ac[ha]))
                     return 0;
               }
               // every data block is an MCU, so countdown the restart interval
               if (--z->todo <= 0) {
                  if (z->code_bits < 24) stbi__grow_buffer_unsafe(z);
                  if (!STBI__RESTART(z->marker)) return 1;
                  stbi__jpeg_reset(z);
               }
            }
         }
         return 1;
      } else { // interleaved
         int i,j,k,x,y;
         for (j=0; j < z->img_mcu_y; ++j) {
            for (i=0; i < z->img_mcu_x; ++i) {
               // scan an interleaved mcu... process scan_n components in order
               for (k=0; k < z->scan_n; ++k) {
                  int n = z->order[k];
                  // scan out an mcu's worth of this component; that's just determined
                  // by the basic H and V specified for the component
                  for (y=0; y < z->img_comp[n].v; ++y) {
                     for (x=0; x < z->img_comp[n].h; ++x) {
                        int x2 = (i*z->img_comp[n].h + x);
                        int y2 = (j*z->img_comp[n].v + y);
                        short *data = z->img_comp[n].coeff + 64 * (x2 + y2 * z->img_comp[n].coeff_w);
                        if (!stbi__jpeg_decode_block_prog_dc(z, data, &z->huff_dc[z->img_comp[n].hd], n))
                           return 0;
                     }
                  }
               }
               // after all interleaved components, that's an interleaved MCU,
               // so now count down the restart interval
               if (--z->todo <= 0) {
                  if (z->code_bits < 24) stbi__grow_buffer_unsafe(z);
                  if (!STBI__RESTART(z->marker)) return 1;
                  stbi__jpeg_reset(z);
               }
            }
         }
         return 1;
      }
   }
}

static void stbi__jpeg_dequantize(short *data, stbi__uint16 *dequant)
{
   int i;
   for (i=0; i < 64; ++i)
      data[i] *= dequant[i];
}

static void stbi__jpeg_finish(stbi__jpeg *z)
{
   if (z->progressive) {
      // dequantize and idct the data
      int i,j,n;
      for (n=0; n < z->s->img_n; ++n) {
         int w = (z->img_comp[n].x+7) >> 3;
         int h = (z->img_comp[n].y+7) >> 3;
         for (j=0; j < h; ++j) {
            for (i=0; i < w; ++i) {
               short *data = z->img_comp[n].coeff + 64 * (i + j * z->img_comp[n].coeff_w);
               stbi__jpeg_dequantize(data, z->dequant[z->img_comp[n].tq]);
               z->idct_block_kernel(z->img_comp[n].data+z->img_comp[n].w2*j*8+i*8, z->img_comp[n].w2, data);
            }
         }
      }
   }
}

static int stbi__process_marker(stbi__jpeg *z, int m)
{
   int L;
   switch (m) {
      case STBI__MARKER_none: // no marker found
         return stbi__err("expected marker","Corrupt JPEG");

      case 0xDD: // DRI - specify restart interval
         if (stbi__get16be(z->s) != 4) return stbi__err("bad DRI len","Corrupt JPEG");
         z->restart_interval = stbi__get16be(z->s);
         return 1;

      case 0xDB: // DQT - define quantization table
         L = stbi__get16be(z->s)-2;
         while (L > 0) {
            int q = stbi__get8(z->s);
            int p = q >> 4, sixteen = (p != 0);
            int t = q & 15,i;
            if (p != 0 && p != 1) return stbi__err("bad DQT type","Corrupt JPEG");
            if (t > 3) return stbi__err("bad DQT table","Corrupt JPEG");

            for (i=0; i < 64; ++i)
               z->dequant[t][stbi__jpeg_dezigzag[i]] = (stbi__uint16)(sixteen ? stbi__get16be(z->s) : stbi__get8(z->s));
            L -= (sixteen ? 129 : 65);
         }
         return L==0;

      case 0xC4: // DHT - define huffman table
         L = stbi__get16be(z->s)-2;
         while (L > 0) {
            stbi_uc *v;
            int sizes[16],i,n=0;
            int q = stbi__get8(z->s);
            int tc = q >> 4;
            int th = q & 15;
            if (tc > 1 || th > 3) return stbi__err("bad DHT header","Corrupt JPEG");
            for (i=0; i < 16; ++i) {
               sizes[i] = stbi__get8(z->s);
               n += sizes[i];
            }
            L -= 17;
            if (tc == 0) {
               if (!stbi__build_huffman(z->huff_dc+th, sizes)) return 0;
               v = z->huff_dc[th].values;
            } else {
               if (!stbi__build_huffman(z->huff_ac+th, sizes)) return 0;
               v = z->huff_ac[th].values;
            }
            for (i=0; i < n; ++i)
               v[i] = stbi__get8(z->s);
            if (tc != 0)
               stbi__build_fast_ac(z->fast_ac[th], z->huff_ac + th);
            L -= n;
         }
         return L==0;
   }

   // check for comment block or APP blocks
   if ((m >= 0xE0 && m <= 0xEF) || m == 0xFE) {
      L = stbi__get16be(z->s);
      if (L < 2) {
         if (m == 0xFE)
            return stbi__err("bad COM len","Corrupt JPEG");
         else
            return stbi__err("bad APP len","Corrupt JPEG");
      }
      L -= 2;

      if (m == 0xE0 && L >= 5) { // JFIF APP0 segment
         static const unsigned char tag[5] = {'J','F','I','F','\0'};
         int ok = 1;
         int i;
         for (i=0; i < 5; ++i)
            if (stbi__get8(z->s) != tag[i])
               ok = 0;
         L -= 5;
         if (ok)
            z->jfif = 1;
      } else if (m == 0xEE && L >= 12) { // Adobe APP14 segment
         static const unsigned char tag[6] = {'A','d','o','b','e','\0'};
         int ok = 1;
         int i;
         for (i=0; i < 6; ++i)
            if (stbi__get8(z->s) != tag[i])
               ok = 0;
         L -= 6;
         if (ok) {
            stbi__get8(z->s); // version
            stbi__get16be(z->s); // flags0
            stbi__get16be(z->s); // flags1
            z->app14_color_transform = stbi__get8(z->s); // color transform
            L -= 6;
         }
      }

      stbi__skip(z->s, L);
      return 1;
   }

   return stbi__err("unknown marker","Corrupt JPEG");
}

// after we see SOS
static int stbi__process_scan_header(stbi__jpeg *z)
{
   int i;
   int Ls = stbi__get16be(z->s);
   z->scan_n = stbi__get8(z->s);
   if (z->scan_n < 1 || z->scan_n > 4 || z->scan_n > (int) z->s->img_n) return stbi__err("bad SOS component count","Corrupt JPEG");
   if (Ls != 6+2*z->scan_n) return stbi__err("bad SOS len","Corrupt JPEG");
   for (i=0; i < z->scan_n; ++i) {
      int id = stbi__get8(z->s), which;
      int q = stbi__get8(z->s);
      for (which = 0; which < z->s->img_n; ++which)
         if (z->img_comp[which].id == id)
            break;
      if (which == z->s->img_n) return 0; // no match
      z->img_comp[which].hd = q >> 4;   if (z->img_comp[which].hd > 3) return stbi__err("bad DC huff","Corrupt JPEG");
      z->img_comp[which].ha = q & 15;   if (z->img_comp[which].ha > 3) return stbi__err("bad AC huff","Corrupt JPEG");
      z->order[i] = which;
   }

   {
      int aa;
      z->spec_start = stbi__get8(z->s);
      z->spec_end   = stbi__get8(z->s); // should be 63, but might be 0
      aa = stbi__get8(z->s);
      z->succ_high = (aa >> 4);
      z->succ_low  = (aa & 15);
      if (z->progressive) {
         if (z->spec_start > 63 || z->spec_end > 63  || z->spec_start > z->spec_end || z->succ_high > 13 || z->succ_low > 13)
            return stbi__err("bad SOS", "Corrupt JPEG");
      } else {
         if (z->spec_start != 0) return stbi__err("bad SOS","Corrupt JPEG");
         if (z->succ_high != 0 || z->succ_low != 0) return stbi__err("bad SOS","Corrupt JPEG");
         z->spec_end = 63;
      }
   }

   return 1;
}

static int stbi__free_jpeg_components(stbi__jpeg *z, int ncomp, int why)
{
   int i;
   for (i=0; i < ncomp; ++i) {
      if (z->img_comp[i].raw_data) {
         STBI_FREE(z->img_comp[i].raw_data);
         z->img_comp[i].raw_data = NULL;
         z->img_comp[i].data = NULL;
      }
      if (z->img_comp[i].raw_coeff) {
         STBI_FREE(z->img_comp[i].raw_coeff);
         z->img_comp[i].raw_coeff = 0;
         z->img_comp[i].coeff = 0;
      }
      if (z->img_comp[i].linebuf) {
         STBI_FREE(z->img_comp[i].linebuf);
         z->img_comp[i].linebuf = NULL;
      }
   }
   return why;
}

static int stbi__process_frame_header(stbi__jpeg *z, int scan)
{
   stbi__context *s = z->s;
   int Lf,p,i,q, h_max=1,v_max=1,c;
   Lf = stbi__get16be(s);         if (Lf < 11) return stbi__err("bad SOF len","Corrupt JPEG"); // JPEG
   p  = stbi__get8(s);            if (p != 8) return stbi__err("only 8-bit","JPEG format not supported: 8-bit only"); // JPEG baseline
   s->img_y = stbi__get16be(s);   if (s->img_y == 0) return stbi__err("no header height", "JPEG format not supported: delayed height"); // Legal, but we don't handle it--but neither does IJG
   s->img_x = stbi__get16be(s);   if (s->img_x == 0) return stbi__err("0 width","Corrupt JPEG"); // JPEG requires
   c = stbi__get8(s);
   if (c != 3 && c != 1 && c != 4) return stbi__err("bad component count","Corrupt JPEG");
   s->img_n = c;
   for (i=0; i < c; ++i) {
      z->img_comp[i].data = NULL;
      z->img_comp[i].linebuf = NULL;
   }

   if (Lf != 8+3*s->img_n) return stbi__err("bad SOF len","Corrupt JPEG");

   z->rgb = 0;
   for (i=0; i < s->img_n; ++i) {
      static const unsigned char rgb[3] = { 'R', 'G', 'B' };
      z->img_comp[i].id = stbi__get8(s);
      if (s->img_n == 3 && z->img_comp[i].id == rgb[i])
         ++z->rgb;
      q = stbi__get8(s);
      z->img_comp[i].h = (q >> 4);  if (!z->img_comp[i].h || z->img_comp[i].h > 4) return stbi__err("bad H","Corrupt JPEG");
      z->img_comp[i].v = q & 15;    if (!z->img_comp[i].v || z->img_comp[i].v > 4) return stbi__err("bad V","Corrupt JPEG");
      z->img_comp[i].tq = stbi__get8(s);  if (z->img_comp[i].tq > 3) return stbi__err("bad TQ","Corrupt JPEG");
   }

   if (scan != STBI__SCAN_load) return 1;

   if (!stbi__mad3sizes_valid(s->img_x, s->img_y, s->img_n, 0)) return stbi__err("too large", "Image too large to decode");

   for (i=0; i < s->img_n; ++i) {
      if (z->img_comp[i].h > h_max) h_max = z->img_comp[i].h;
      if (z->img_comp[i].v > v_max) v_max = z->img_comp[i].v;
   }

   // compute interleaved mcu info
   z->img_h_max = h_max;
   z->img_v_max = v_max;
   z->img_mcu_w = h_max * 8;
   z->img_mcu_h = v_max * 8;
   // these sizes can't be more than 17 bits
   z->img_mcu_x = (s->img_x + z->img_mcu_w-1) / z->img_mcu_w;
   z->img_mcu_y = (s->img_y + z->img_mcu_h-1) / z->img_mcu_h;

   for (i=0; i < s->img_n; ++i) {
      // number of effective pixels (e.g. for non-interleaved MCU)
      z->img_comp[i].x = (s->img_x * z->img_comp[i].h + h_max-1) / h_max;
      z->img_comp[i].y = (s->img_y * z->img_comp[i].v + v_max-1) / v_max;
      // to simplify generation, we'll allocate enough memory to decode
      // the bogus oversized data from using interleaved MCUs and their
      // big blocks (e.g. a 16x16 iMCU on an image of width 33); we won't
      // discard the extra data until colorspace conversion
      //
      // img_mcu_x, img_mcu_y: <=17 bits; comp[i].h and .v are <=4 (checked earlier)
      // so these muls can't overflow with 32-bit ints (which we require)
      z->img_comp[i].w2 = z->img_mcu_x * z->img_comp[i].h * 8;
      z->img_comp[i].h2 = z->img_mcu_y * z->img_comp[i].v * 8;
      z->img_comp[i].coeff = 0;
      z->img_comp[i].raw_coeff = 0;
      z->img_comp[i].linebuf = NULL;
      z->img_comp[i].raw_data = stbi__malloc_mad2(z->img_comp[i].w2, z->img_comp[i].h2, 15);
      if (z->img_comp[i].raw_data == NULL)
         return stbi__free_jpeg_components(z, i+1, stbi__err("outofmem", "Out of memory"));
      // align blocks for idct using mmx/sse
      z->img_comp[i].data = (stbi_uc*) (((size_t) z->img_comp[i].raw_data + 15) & ~15);
      if (z->progressive) {
         // w2, h2 are multiples of 8 (see above)
         z->img_comp[i].coeff_w = z->img_comp[i].w2 / 8;
         z->img_comp[i].coeff_h = z->img_comp[i].h2 / 8;
         z->img_comp[i].raw_coeff = stbi__malloc_mad3(z->img_comp[i].w2, z->img_comp[i].h2, sizeof(short), 15);
         if (z->img_comp[i].raw_coeff == NULL)
            return stbi__free_jpeg_components(z, i+1, stbi__err("outofmem", "Out of memory"));
         z->img_comp[i].coeff = (short*) (((size_t) z->img_comp[i].raw_coeff + 15) & ~15);
      }
   }

   return 1;
}

// use comparisons since in some cases we handle more than one case (e.g. SOF)
#define stbi__DNL(x)         ((x) == 0xdc)
#define stbi__SOI(x)         ((x) == 0xd8)
#define stbi__EOI(x)         ((x) == 0xd9)
#define stbi__SOF(x)         ((x) == 0xc0 || (x) == 0xc1 || (x) == 0xc2)
#define stbi__SOS(x)         ((x) == 0xda)

#define stbi__SOF_progressive(x)   ((x) == 0xc2)

static int stbi__decode_jpeg_header(stbi__jpeg *z, int scan)
{
   int m;
   z->jfif = 0;
   z->app14_color_transform = -1; // valid values are 0,1,2
   z->marker = STBI__MARKER_none; // initialize cached marker to empty
   m = stbi__get_marker(z);
   if (!stbi__SOI(m)) return stbi__err("no SOI","Corrupt JPEG");
   if (scan == STBI__SCAN_type) return 1;
   m = stbi__get_marker(z);
   while (!stbi__SOF(m)) {
      if (!stbi__process_marker(z,m)) return 0;
      m = stbi__get_marker(z);
      while (m == STBI__MARKER_none) {
         // some files have extra padding after their blocks, so ok, we'll scan
         if (stbi__at_eof(z->s)) return stbi__err("no SOF", "Corrupt JPEG");
         m = stbi__get_marker(z);
      }
   }
   z->progressive = stbi__SOF_progressive(m);
   if (!stbi__process_frame_header(z, scan)) return 0;
   return 1;
}

// decode image to YCbCr format
static int stbi__decode_jpeg_image(stbi__jpeg *j)
{
   int m;
   for (m = 0; m < 4; m++) {
      j->img_comp[m].raw_data = NULL;
      j->img_comp[m].raw_coeff = NULL;
   }
   j->restart_interval = 0;
   if (!stbi__decode_jpeg_header(j, STBI__SCAN_load)) return 0;
   m = stbi__get_marker(j);
   while (!stbi__EOI(m)) {
      if (stbi__SOS(m)) {
         if (!stbi__process_scan_header(j)) return 0;
         if (!stbi__parse_entropy_coded_data(j)) return 0;
         if (j->marker == STBI__MARKER_none ) {
            // handle 0s at the end of image data from IP Kamera 9060
            while (!stbi__at_eof(j->s)) {
               int x = stbi__get8(j->s);
               if (x == 255) {
                  j->marker = stbi__get8(j->s);
                  break;
               }
            }
            // if we reach eof without hitting a marker, stbi__get_marker() below will fail and we'll eventually return 0
         }
      } else if (stbi__DNL(m)) {
         int Ld = stbi__get16be(j->s);
         stbi__uint32 NL = stbi__get16be(j->s);
         if (Ld != 4) return stbi__err("bad DNL len", "Corrupt JPEG");
         if (NL != j->s->img_y) return stbi__err("bad DNL height", "Corrupt JPEG");
      } else {
         if (!stbi__process_marker(j, m)) return 0;
      }
      m = stbi__get_marker(j);
   }
   if (j->progressive)
      stbi__jpeg_finish(j);
   return 1;
}

// static jfif-centered resampling (across block boundaries)

typedef stbi_uc *(*resample_row_func)(stbi_uc *out, stbi_uc *in0, stbi_uc *in1,
                                    int w, int hs);

#define stbi__div4(x) ((stbi_uc) ((x) >> 2))

static stbi_uc *resample_row_1(stbi_uc *out, stbi_uc *in_near, stbi_uc *in_far, int w, int hs)
{
   STBI_NOTUSED(out);
   STBI_NOTUSED(in_far);
   STBI_NOTUSED(w);
   STBI_NOTUSED(hs);
   return in_near;
}

static stbi_uc* stbi__resample_row_v_2(stbi_uc *out, stbi_uc *in_near, stbi_uc *in_far, int w, int hs)
{
   // need to generate two samples vertically for every one in input
   int i;
   STBI_NOTUSED(hs);
   for (i=0; i < w; ++i)
      out[i] = stbi__div4(3*in_near[i] + in_far[i] + 2);
   return out;
}

static stbi_uc*  stbi__resample_row_h_2(stbi_uc *out, stbi_uc *in_near, stbi_uc *in_far, int w, int hs)
{
   // need to generate two samples horizontally for every one in input
   int i;
   stbi_uc *input = in_near;

   if (w == 1) {
      // if only one sample, can't do any interpolation
      out[0] = out[1] = input[0];
      return out;
   }

   out[0] = input[0];
   out[1] = stbi__div4(input[0]*3 + input[1] + 2);
   for (i=1; i < w-1; ++i) {
      int n = 3*input[i]+2;
      out[i*2+0] = stbi__div4(n+input[i-1]);
      out[i*2+1] = stbi__div4(n+input[i+1]);
   }
   out[i*2+0] = stbi__div4(input[w-2]*3 + input[w-1] + 2);
   out[i*2+1] = input[w-1];

   STBI_NOTUSED(in_far);
   STBI_NOTUSED(hs);

   return out;
}

#define stbi__div16(x) ((stbi_uc) ((x) >> 4))

static stbi_uc *stbi__resample_row_hv_2(stbi_uc *out, stbi_uc *in_near, stbi_uc *in_far, int w, int hs)
{
   // need to generate 2x2 samples for every one in input
   int i,t0,t1;
   if (w == 1) {
      out[0] = out[1] = stbi__div4(3*in_near[0] + in_far[0] + 2);
      return out;
   }

   t1 = 3*in_near[0] + in_far[0];
   out[0] = stbi__div4(t1+2);
   for (i=1; i < w; ++i) {
      t0 = t1;
      t1 = 3*in_near[i]+in_far[i];
      out[i*2-1] = stbi__div16(3*t0 + t1 + 8);
      out[i*2  ] = stbi__div16(3*t1 + t0 + 8);
   }
   out[w*2-1] = stbi__div4(t1+2);

   STBI_NOTUSED(hs);

   return out;
}

#if defined(STBI_SSE2) || defined(STBI_NEON)
static stbi_uc *stbi__resample_row_hv_2_simd(stbi_uc *out, stbi_uc *in_near, stbi_uc *in_far, int w, int hs)
{
   // need to generate 2x2 samples for every one in input
   int i=0,t0,t1;

   if (w == 1) {
      out[0] = out[1] = stbi__div4(3*in_near[0] + in_far[0] + 2);
      return out;
   }

   t1 = 3*in_near[0] + in_far[0];
   // process groups of 8 pixels for as long as we can.
   // note we can't handle the last pixel in a row in this loop
   // because we need to handle the filter boundary conditions.
   for (; i < ((w-1) & ~7); i += 8) {
#if defined(STBI_SSE2)
      // load and perform the vertical filtering pass
      // this uses 3*x + y = 4*x + (y - x)
      __m128i zero  = _mm_setzero_si128();
      __m128i farb  = _mm_loadl_epi64((__m128i *) (in_far + i));
      __m128i nearb = _mm_loadl_epi64((__m128i *) (in_near + i));
      __m128i farw  = _mm_unpacklo_epi8(farb, zero);
      __m128i nearw = _mm_unpacklo_epi8(nearb, zero);
      __m128i diff  = _mm_sub_epi16(farw, nearw);
      __m128i nears = _mm_slli_epi16(nearw, 2);
      __m128i curr  = _mm_add_epi16(nears, diff); // current row

      // horizontal filter works the same based on shifted vers of current
      // row. "prev" is current row shifted right by 1 pixel; we need to
      // insert the previous pixel value (from t1).
      // "next" is current row shifted left by 1 pixel, with first pixel
      // of next block of 8 pixels added in.
      __m128i prv0 = _mm_slli_si128(curr, 2);
      __m128i nxt0 = _mm_srli_si128(curr, 2);
      __m128i prev = _mm_insert_epi16(prv0, t1, 0);
      __m128i next = _mm_insert_epi16(nxt0, 3*in_near[i+8] + in_far[i+8], 7);

      // horizontal filter, polyphase implementation since it's convenient:
      // even pixels = 3*cur + prev = cur*4 + (prev - cur)
      // odd  pixels = 3*cur + next = cur*4 + (next - cur)
      // note the shared term.
      __m128i bias  = _mm_set1_epi16(8);
      __m128i curs = _mm_slli_epi16(curr, 2);
      __m128i prvd = _mm_sub_epi16(prev, curr);
      __m128i nxtd = _mm_sub_epi16(next, curr);
      __m128i curb = _mm_add_epi16(curs, bias);
      __m128i even = _mm_add_epi16(prvd, curb);
      __m128i odd  = _mm_add_epi16(nxtd, curb);

      // interleave even and odd pixels, then undo scaling.
      __m128i int0 = _mm_unpacklo_epi16(even, odd);
      __m128i int1 = _mm_unpackhi_epi16(even, odd);
      __m128i de0  = _mm_srli_epi16(int0, 4);
      __m128i de1  = _mm_srli_epi16(int1, 4);

      // pack and write output
      __m128i outv = _mm_packus_epi16(de0, de1);
      _mm_storeu_si128((__m128i *) (out + i*2), outv);
#elif defined(STBI_NEON)
      // load and perform the vertical filtering pass
      // this uses 3*x + y = 4*x + (y - x)
      uint8x8_t farb  = vld1_u8(in_far + i);
      uint8x8_t nearb = vld1_u8(in_near + i);
      int16x8_t diff  = vreinterpretq_s16_u16(vsubl_u8(farb, nearb));
      int16x8_t nears = vreinterpretq_s16_u16(vshll_n_u8(nearb, 2));
      int16x8_t curr  = vaddq_s16(nears, diff); // current row

      // horizontal filter works the same based on shifted vers of current
      // row. "prev" is current row shifted right by 1 pixel; we need to
      // insert the previous pixel value (from t1).
      // "next" is current row shifted left by 1 pixel, with first pixel
      // of next block of 8 pixels added in.
      int16x8_t prv0 = vextq_s16(curr, curr, 7);
      int16x8_t nxt0 = vextq_s16(curr, curr, 1);
      int16x8_t prev = vsetq_lane_s16(t1, prv0, 0);
      int16x8_t next = vsetq_lane_s16(3*in_near[i+8] + in_far[i+8], nxt0, 7);

      // horizontal filter, polyphase implementation since it's convenient:
      // even pixels = 3*cur + prev = cur*4 + (prev - cur)
      // odd  pixels = 3*cur + next = cur*4 + (next - cur)
      // note the shared term.
      int16x8_t curs = vshlq_n_s16(curr, 2);
      int16x8_t prvd = vsubq_s16(prev, curr);
      int16x8_t nxtd = vsubq_s16(next, curr);
      int16x8_t even = vaddq_s16(curs, prvd);
      int16x8_t odd  = vaddq_s16(curs, nxtd);

      // undo scaling and round, then store with even/odd phases interleaved
      uint8x8x2_t o;
      o.val[0] = vqrshrun_n_s16(even, 4);
      o.val[1] = vqrshrun_n_s16(odd,  4);
      vst2_u8(out + i*2, o);
#endif

      // "previous" value for next iter
      t1 = 3*in_near[i+7] + in_far[i+7];
   }

   t0 = t1;
   t1 = 3*in_near[i] + in_far[i];
   out[i*2] = stbi__div16(3*t1 + t0 + 8);

   for (++i; i < w; ++i) {
      t0 = t1;
      t1 = 3*in_near[i]+in_far[i];
      out[i*2-1] = stbi__div16(3*t0 + t1 + 8);
      out[i*2  ] = stbi__div16(3*t1 + t0 + 8);
   }
   out[w*2-1] = stbi__div4(t1+2);

   STBI_NOTUSED(hs);

   return out;
}
#endif

static stbi_uc *stbi__resample_row_generic(stbi_uc *out, stbi_uc *in_near, stbi_uc *in_far, int w, int hs)
{
   // resample with nearest-neighbor
   int i,j;
   STBI_NOTUSED(in_far);
   for (i=0; i < w; ++i)
      for (j=0; j < hs; ++j)
         out[i*hs+j] = in_near[i];
   return out;
}

// this is a reduced-precision calculation of YCbCr-to-RGB introduced
// to make sure the code produces the same results in both SIMD and scalar
#define stbi__float2fixed(x)  (((int) ((x) * 4096.0f + 0.5f)) << 8)
static void stbi__YCbCr_to_RGB_row(stbi_uc *out, const stbi_uc *y, const stbi_uc *pcb, const stbi_uc *pcr, int count, int step)
{
   int i;
   for (i=0; i < count; ++i) {
      int y_fixed = (y[i] << 20) + (1<<19); // rounding
      int r,g,b;
      int cr = pcr[i] - 128;
      int cb = pcb[i] - 128;
      r = y_fixed +  cr* stbi__float2fixed(1.40200f);
      g = y_fixed + (cr*-stbi__float2fixed(0.71414f)) + ((cb*-stbi__float2fixed(0.34414f)) & 0xffff0000);
      b = y_fixed                                     +   cb* stbi__float2fixed(1.77200f);
      r >>= 20;
      g >>= 20;
      b >>= 20;
      if ((unsigned) r > 255) { if (r < 0) r = 0; else r = 255; }
      if ((unsigned) g > 255) { if (g < 0) g = 0; else g = 255; }
      if ((unsigned) b > 255) { if (b < 0) b = 0; else b = 255; }
      out[0] = (stbi_uc)r;
      out[1] = (stbi_uc)g;
      out[2] = (stbi_uc)b;
      out[3] = 255;
      out += step;
   }
}

#if defined(STBI_SSE2) || defined(STBI_NEON)
static void stbi__YCbCr_to_RGB_simd(stbi_uc *out, stbi_uc const *y, stbi_uc const *pcb, stbi_uc const *pcr, int count, int step)
{
   int i = 0;

#ifdef STBI_SSE2
   // step == 3 is pretty ugly on the final interleave, and i'm not convinced
   // it's useful in practice (you wouldn't use it for textures, for example).
   // so just accelerate step == 4 case.
   if (step == 4) {
      // this is a fairly straightforward implementation and not super-optimized.
      __m128i signflip  = _mm_set1_epi8(-0x80);
      __m128i cr_const0 = _mm_set1_epi16(   (short) ( 1.40200f*4096.0f+0.5f));
      __m128i cr_const1 = _mm_set1_epi16( - (short) ( 0.71414f*4096.0f+0.5f));
      __m128i cb_const0 = _mm_set1_epi16( - (short) ( 0.34414f*4096.0f+0.5f));
      __m128i cb_const1 = _mm_set1_epi16(   (short) ( 1.77200f*4096.0f+0.5f));
      __m128i y_bias = _mm_set1_epi8((char) (unsigned char) 128);
      __m128i xw = _mm_set1_epi16(255); // alpha channel

      for (; i+7 < count; i += 8) {
         // load
         __m128i y_bytes = _mm_loadl_epi64((__m128i *) (y+i));
         __m128i cr_bytes = _mm_loadl_epi64((__m128i *) (pcr+i));
         __m128i cb_bytes = _mm_loadl_epi64((__m128i *) (pcb+i));
         __m128i cr_biased = _mm_xor_si128(cr_bytes, signflip); // -128
         __m128i cb_biased = _mm_xor_si128(cb_bytes, signflip); // -128

         // unpack to short (and left-shift cr, cb by 8)
         __m128i yw  = _mm_unpacklo_epi8(y_bias, y_bytes);
         __m128i crw = _mm_unpacklo_epi8(_mm_setzero_si128(), cr_biased);
         __m128i cbw = _mm_unpacklo_epi8(_mm_setzero_si128(), cb_biased);

         // color transform
         __m128i yws = _mm_srli_epi16(yw, 4);
         __m128i cr0 = _mm_mulhi_epi16(cr_const0, crw);
         __m128i cb0 = _mm_mulhi_epi16(cb_const0, cbw);
         __m128i cb1 = _mm_mulhi_epi16(cbw, cb_const1);
         __m128i cr1 = _mm_mulhi_epi16(crw, cr_const1);
         __m128i rws = _mm_add_epi16(cr0, yws);
         __m128i gwt = _mm_add_epi16(cb0, yws);
         __m128i bws = _mm_add_epi16(yws, cb1);
         __m128i gws = _mm_add_epi16(gwt, cr1);

         // descale
         __m128i rw = _mm_srai_epi16(rws, 4);
         __m128i bw = _mm_srai_epi16(bws, 4);
         __m128i gw = _mm_srai_epi16(gws, 4);

         // back to byte, set up for transpose
         __m128i brb = _mm_packus_epi16(rw, bw);
         __m128i gxb = _mm_packus_epi16(gw, xw);

         // transpose to interleave channels
         __m128i t0 = _mm_unpacklo_epi8(brb, gxb);
         __m128i t1 = _mm_unpackhi_epi8(brb, gxb);
         __m128i o0 = _mm_unpacklo_epi16(t0, t1);
         __m128i o1 = _mm_unpackhi_epi16(t0, t1);

         // store
         _mm_storeu_si128((__m128i *) (out + 0), o0);
         _mm_storeu_si128((__m128i *) (out + 16), o1);
         out += 32;
      }
   }
#endif

#ifdef STBI_NEON
   // in this version, step=3 support would be easy to add. but is there demand?
   if (step == 4) {
      // this is a fairly straightforward implementation and not super-optimized.
      uint8x8_t signflip = vdup_n_u8(0x80);
      int16x8_t cr_const0 = vdupq_n_s16(   (short) ( 1.40200f*4096.0f+0.5f));
      int16x8_t cr_const1 = vdupq_n_s16( - (short) ( 0.71414f*4096.0f+0.5f));
      int16x8_t cb_const0 = vdupq_n_s16( - (short) ( 0.34414f*4096.0f+0.5f));
      int16x8_t cb_const1 = vdupq_n_s16(   (short) ( 1.77200f*4096.0f+0.5f));

      for (; i+7 < count; i += 8) {
         // load
         uint8x8_t y_bytes  = vld1_u8(y + i);
         uint8x8_t cr_bytes = vld1_u8(pcr + i);
         uint8x8_t cb_bytes = vld1_u8(pcb + i);
         int8x8_t cr_biased = vreinterpret_s8_u8(vsub_u8(cr_bytes, signflip));
         int8x8_t cb_biased = vreinterpret_s8_u8(vsub_u8(cb_bytes, signflip));

         // expand to s16
         int16x8_t yws = vreinterpretq_s16_u16(vshll_n_u8(y_bytes, 4));
         int16x8_t crw = vshll_n_s8(cr_biased, 7);
         int16x8_t cbw = vshll_n_s8(cb_biased, 7);

         // color transform
         int16x8_t cr0 = vqdmulhq_s16(crw, cr_const0);
         int16x8_t cb0 = vqdmulhq_s16(cbw, cb_const0);
         int16x8_t cr1 = vqdmulhq_s16(crw, cr_const1);
         int16x8_t cb1 = vqdmulhq_s16(cbw, cb_const1);
         int16x8_t rws = vaddq_s16(yws, cr0);
         int16x8_t gws = vaddq_s16(vaddq_s16(yws, cb0), cr1);
         int16x8_t bws = vaddq_s16(yws, cb1);

         // undo scaling, round, convert to byte
         uint8x8x4_t o;
         o.val[0] = vqrshrun_n_s16(rws, 4);
         o.val[1] = vqrshrun_n_s16(gws, 4);
         o.val[2] = vqrshrun_n_s16(bws, 4);
         o.val[3] = vdup_n_u8(255);

         // store, interleaving r/g/b/a
         vst4_u8(out, o);
         out += 8*4;
      }
   }
#endif

   for (; i < count; ++i) {
      int y_fixed = (y[i] << 20) + (1<<19); // rounding
      int r,g,b;
      int cr = pcr[i] - 128;
      int cb = pcb[i] - 128;
      r = y_fixed + cr* stbi__float2fixed(1.40200f);
      g = y_fixed + cr*-stbi__float2fixed(0.71414f) + ((cb*-stbi__float2fixed(0.34414f)) & 0xffff0000);
      b = y_fixed                                   +   cb* stbi__float2fixed(1.77200f);
      r >>= 20;
      g >>= 20;
      b >>= 20;
      if ((unsigned) r > 255) { if (r < 0) r = 0; else r = 255; }
      if ((unsigned) g > 255) { if (g < 0) g = 0; else g = 255; }
      if ((unsigned) b > 255) { if (b < 0) b = 0; else b = 255; }
      out[0] = (stbi_uc)r;
      out[1] = (stbi_uc)g;
      out[2] = (stbi_uc)b;
      out[3] = 255;
      out += step;
   }
}
#endif

// set up the kernels
static void stbi__setup_jpeg(stbi__jpeg *j)
{
   j->idct_block_kernel = stbi__idct_block;
   j->YCbCr_to_RGB_kernel = stbi__YCbCr_to_RGB_row;
   j->resample_row_hv_2_kernel = stbi__resample_row_hv_2;

#ifdef STBI_SSE2
   if (stbi__sse2_available()) {
      j->idct_block_kernel = stbi__idct_simd;
      j->YCbCr_to_RGB_kernel = stbi__YCbCr_to_RGB_simd;
      j->resample_row_hv_2_kernel = stbi__resample_row_hv_2_simd;
   }
#endif

#ifdef STBI_NEON
   j->idct_block_kernel = stbi__idct_simd;
   j->YCbCr_to_RGB_kernel = stbi__YCbCr_to_RGB_simd;
   j->resample_row_hv_2_kernel = stbi__resample_row_hv_2_simd;
#endif
}

// clean up the temporary component buffers
static void stbi__cleanup_jpeg(stbi__jpeg *j)
{
   stbi__free_jpeg_components(j, j->s->img_n, 0);
}

typedef struct
{
   resample_row_func resample;
   stbi_uc *line0,*line1;
   int hs,vs;   // expansion factor in each axis
   int w_lores; // horizontal pixels pre-expansion
   int ystep;   // how far through vertical expansion we are
   int ypos;    // which pre-expansion row we're on
} stbi__resample;

// fast 0..255 * 0..255 => 0..255 rounded multiplication
static stbi_uc stbi__blinn_8x8(stbi_uc x, stbi_uc y)
{
   unsigned int t = x*y + 128;
   return (stbi_uc) ((t + (t >>8)) >> 8);
}

static stbi_uc *load_jpeg_image(stbi__jpeg *z, int *out_x, int *out_y, int *comp, int req_comp)
{
   int n, decode_n, is_rgb;
   z->s->img_n = 0; // make stbi__cleanup_jpeg safe

   // validate req_comp
   if (req_comp < 0 || req_comp > 4) return stbi__errpuc("bad req_comp", "Internal error");

   // load a jpeg image from whichever source, but leave in YCbCr format
   if (!stbi__decode_jpeg_image(z)) { stbi__cleanup_jpeg(z); return NULL; }

   // determine actual number of components to generate
   n = req_comp ? req_comp : z->s->img_n >= 3 ? 3 : 1;

   is_rgb = z->s->img_n == 3 && (z->rgb == 3 || (z->app14_color_transform == 0 && !z->jfif));

   if (z->s->img_n == 3 && n < 3 && !is_rgb)
      decode_n = 1;
   else
      decode_n = z->s->img_n;

   // resample and color-convert
   {
      int k;
      unsigned int i,j;
      stbi_uc *output;
      stbi_uc *coutput[4] = { NULL, NULL, NULL, NULL };

      stbi__resample res_comp[4];

      for (k=0; k < decode_n; ++k) {
         stbi__resample *r = &res_comp[k];

         // allocate line buffer big enough for upsampling off the edges
         // with upsample factor of 4
         z->img_comp[k].linebuf = (stbi_uc *) stbi__malloc(z->s->img_x + 3);
         if (!z->img_comp[k].linebuf) { stbi__cleanup_jpeg(z); return stbi__errpuc("outofmem", "Out of memory"); }

         r->hs      = z->img_h_max / z->img_comp[k].h;
         r->vs      = z->img_v_max / z->img_comp[k].v;
         r->ystep   = r->vs >> 1;
         r->w_lores = (z->s->img_x + r->hs-1) / r->hs;
         r->ypos    = 0;
         r->line0   = r->line1 = z->img_comp[k].data;

         if      (r->hs == 1 && r->vs == 1) r->resample = resample_row_1;
         else if (r->hs == 1 && r->vs == 2) r->resample = stbi__resample_row_v_2;
         else if (r->hs == 2 && r->vs == 1) r->resample = stbi__resample_row_h_2;
         else if (r->hs == 2 && r->vs == 2) r->resample = z->resample_row_hv_2_kernel;
         else                               r->resample = stbi__resample_row_generic;
      }

      // can't error after this so, this is safe
      output = (stbi_uc *) stbi__malloc_mad3(n, z->s->img_x, z->s->img_y, 1);
      if (!output) { stbi__cleanup_jpeg(z); return stbi__errpuc("outofmem", "Out of memory"); }

      // now go ahead and resample
      for (j=0; j < z->s->img_y; ++j) {
         stbi_uc *out = output + n * z->s->img_x * j;
         for (k=0; k < decode_n; ++k) {
            stbi__resample *r = &res_comp[k];
            int y_bot = r->ystep >= (r->vs >> 1);
            coutput[k] = r->resample(z->img_comp[k].linebuf,
                                     y_bot ? r->line1 : r->line0,
                                     y_bot ? r->line0 : r->line1,
                                     r->w_lores, r->hs);
            if (++r->ystep >= r->vs) {
               r->ystep = 0;
               r->line0 = r->line1;
               if (++r->ypos < z->img_comp[k].y)
                  r->line1 += z->img_comp[k].w2;
            }
         }
         if (n >= 3) {
            stbi_uc *y = coutput[0];
            if (z->s->img_n == 3) {
               if (is_rgb) {
                  for (i=0; i < z->s->img_x; ++i) {
                     out[0] = y[i];
                     out[1] = coutput[1][i];
                     out[2] = coutput[2][i];
                     out[3] = 255;
                     out += n;
                  }
               } else {
                  z->YCbCr_to_RGB_kernel(out, y, coutput[1], coutput[2], z->s->img_x, n);
               }
            } else if (z->s->img_n == 4) {
               if (z->app14_color_transform == 0) { // CMYK
                  for (i=0; i < z->s->img_x; ++i) {
                     stbi_uc m = coutput[3][i];
                     out[0] = stbi__blinn_8x8(coutput[0][i], m);
                     out[1] = stbi__blinn_8x8(coutput[1][i], m);
                     out[2] = stbi__blinn_8x8(coutput[2][i], m);
                     out[3] = 255;
                     out += n;
                  }
               } else if (z->app14_color_transform == 2) { // YCCK
                  z->YCbCr_to_RGB_kernel(out, y, coutput[1], coutput[2], z->s->img_x, n);
                  for (i=0; i < z->s->img_x; ++i) {
                     stbi_uc m = coutput[3][i];
                     out[0] = stbi__blinn_8x8(255 - out[0], m);
                     out[1] = stbi__blinn_8x8(255 - out[1], m);
                     out[2] = stbi__blinn_8x8(255 - out[2], m);
                     out += n;
                  }
               } else { // YCbCr + alpha?  Ignore the fourth channel for now
                  z->YCbCr_to_RGB_kernel(out, y, coutput[1], coutput[2], z->s->img_x, n);
               }
            } else
               for (i=0; i < z->s->img_x; ++i) {
                  out[0] = out[1] = out[2] = y[i];
                  out[3] = 255; // not used if n==3
                  out += n;
               }
         } else {
            if (is_rgb) {
               if (n == 1)
                  for (i=0; i < z->s->img_x; ++i)
                     *out++ = stbi__compute_y(coutput[0][i], coutput[1][i], coutput[2][i]);
               else {
                  for (i=0; i < z->s->img_x; ++i, out += 2) {
                     out[0] = stbi__compute_y(coutput[0][i], coutput[1][i], coutput[2][i]);
                     out[1] = 255;
                  }
               }
            } else if (z->s->img_n == 4 && z->app14_color_transform == 0) {
               for (i=0; i < z->s->img_x; ++i) {
                  stbi_uc m = coutput[3][i];
                  stbi_uc r = stbi__blinn_8x8(coutput[0][i], m);
                  stbi_uc g = stbi__blinn_8x8(coutput[1][i], m);
                  stbi_uc b = stbi__blinn_8x8(coutput[2][i], m);
                  out[0] = stbi__compute_y(r, g, b);
                  out[1] = 255;
                  out += n;
               }
            } else if (z->s->img_n == 4 && z->app14_color_transform == 2) {
               for (i=0; i < z->s->img_x; ++i) {
                  out[0] = stbi__blinn_8x8(255 - coutput[0][i], coutput[3][i]);
                  out[1] = 255;
                  out += n;
               }
            } else {
               stbi_uc *y = coutput[0];
               if (n == 1)
                  for (i=0; i < z->s->img_x; ++i) out[i] = y[i];
               else
                  for (i=0; i < z->s->img_x; ++i) { *out++ = y[i]; *out++ = 255; }
            }
         }
      }
      stbi__cleanup_jpeg(z);
      *out_x = z->s->img_x;
      *out_y = z->s->img_y;
      if (comp) *comp = z->s->img_n >= 3 ? 3 : 1; // report original components, not output
      return output;
   }
}

static void *stbi__jpeg_load(stbi__context *s, int *x, int *y, int *comp, int req_comp, stbi__result_info *ri)
{
   unsigned char* result;
   stbi__jpeg* j = (stbi__jpeg*) stbi__malloc(sizeof(stbi__jpeg));
   STBI_NOTUSED(ri);
   j->s = s;
   stbi__setup_jpeg(j);
   result = load_jpeg_image(j, x,y,comp,req_comp);
   STBI_FREE(j);
   return result;
}

static int stbi__jpeg_test(stbi__context *s)
{
   int r;
   stbi__jpeg* j = (stbi__jpeg*)stbi__malloc(sizeof(stbi__jpeg));
   j->s = s;
   stbi__setup_jpeg(j);
   r = stbi__decode_jpeg_header(j, STBI__SCAN_type);
   stbi__rewind(s);
   STBI_FREE(j);
   return r;
}

static int stbi__jpeg_info_raw(stbi__jpeg *j, int *x, int *y, int *comp)
{
   if (!stbi__decode_jpeg_header(j, STBI__SCAN_header)) {
      stbi__rewind( j->s );
      return 0;
   }
   if (x) *x = j->s->img_x;
   if (y) *y = j->s->img_y;
   if (comp) *comp = j->s->img_n >= 3 ? 3 : 1;
   return 1;
}

static int stbi__jpeg_info(stbi__context *s, int *x, int *y, int *comp)
{
   int result;
   stbi__jpeg* j = (stbi__jpeg*) (stbi__malloc(sizeof(stbi__jpeg)));
   j->s = s;
   result = stbi__jpeg_info_raw(j, x, y, comp);
   STBI_FREE(j);
   return result;
}
#endif

// public domain zlib decode    v0.2  Sean Barrett 2006-11-18
//    simple implementation
//      - all input must be provided in an upfront buffer
//      - all output is written to a single output buffer (can malloc/realloc)
//    performance
//      - fast huffman

#ifndef STBI_NO_ZLIB

// fast-way is faster to check than jpeg huffman, but slow way is slower
#define STBI__ZFAST_BITS  9 // accelerate all cases in default tables
#define STBI__ZFAST_MASK  ((1 << STBI__ZFAST_BITS) - 1)

// zlib-style huffman encoding
// (jpegs packs from left, zlib from right, so can't share code)
typedef struct
{
   stbi__uint16 fast[1 << STBI__ZFAST_BITS];
   stbi__uint16 firstcode[16];
   int maxcode[17];
   stbi__uint16 firstsymbol[16];
   stbi_uc  size[288];
   stbi__uint16 value[288];
} stbi__zhuffman;

stbi_inline static int stbi__bitreverse16(int n)
{
  n = ((n & 0xAAAA) >>  1) | ((n & 0x5555) << 1);
  n = ((n & 0xCCCC) >>  2) | ((n & 0x3333) << 2);
  n = ((n & 0xF0F0) >>  4) | ((n & 0x0F0F) << 4);
  n = ((n & 0xFF00) >>  8) | ((n & 0x00FF) << 8);
  return n;
}

stbi_inline static int stbi__bit_reverse(int v, int bits)
{
   STBI_ASSERT(bits <= 16);
   // to bit reverse n bits, reverse 16 and shift
   // e.g. 11 bits, bit reverse and shift away 5
   return stbi__bitreverse16(v) >> (16-bits);
}

static int stbi__zbuild_huffman(stbi__zhuffman *z, const stbi_uc *sizelist, int num)
{
   int i,k=0;
   int code, next_code[16], sizes[17];

   // DEFLATE spec for generating codes
   memset(sizes, 0, sizeof(sizes));
   memset(z->fast, 0, sizeof(z->fast));
   for (i=0; i < num; ++i)
      ++sizes[sizelist[i]];
   sizes[0] = 0;
   for (i=1; i < 16; ++i)
      if (sizes[i] > (1 << i))
         return stbi__err("bad sizes", "Corrupt PNG");
   code = 0;
   for (i=1; i < 16; ++i) {
      next_code[i] = code;
      z->firstcode[i] = (stbi__uint16) code;
      z->firstsymbol[i] = (stbi__uint16) k;
      code = (code + sizes[i]);
      if (sizes[i])
         if (code-1 >= (1 << i)) return stbi__err("bad codelengths","Corrupt PNG");
      z->maxcode[i] = code << (16-i); // preshift for inner loop
      code <<= 1;
      k += sizes[i];
   }
   z->maxcode[16] = 0x10000; // sentinel
   for (i=0; i < num; ++i) {
      int s = sizelist[i];
      if (s) {
         int c = next_code[s] - z->firstcode[s] + z->firstsymbol[s];
         stbi__uint16 fastv = (stbi__uint16) ((s << 9) | i);
         z->size [c] = (stbi_uc     ) s;
         z->value[c] = (stbi__uint16) i;
         if (s <= STBI__ZFAST_BITS) {
            int j = stbi__bit_reverse(next_code[s],s);
            while (j < (1 << STBI__ZFAST_BITS)) {
               z->fast[j] = fastv;
               j += (1 << s);
            }
         }
         ++next_code[s];
      }
   }
   return 1;
}

// zlib-from-memory implementation for PNG reading
//    because PNG allows splitting the zlib stream arbitrarily,
//    and it's annoying structurally to have PNG call ZLIB call PNG,
//    we require PNG read all the IDATs and combine them into a single
//    memory buffer

typedef struct
{
   stbi_uc *zbuffer, *zbuffer_end;
   int num_bits;
   stbi__uint32 code_buffer;

   char *zout;
   char *zout_start;
   char *zout_end;
   int   z_expandable;

   stbi__zhuffman z_length, z_distance;
} stbi__zbuf;

stbi_inline static stbi_uc stbi__zget8(stbi__zbuf *z)
{
   if (z->zbuffer >= z->zbuffer_end) return 0;
   return *z->zbuffer++;
}

static void stbi__fill_bits(stbi__zbuf *z)
{
   do {
      STBI_ASSERT(z->code_buffer < (1U << z->num_bits));
      z->code_buffer |= (unsigned int) stbi__zget8(z) << z->num_bits;
      z->num_bits += 8;
   } while (z->num_bits <= 24);
}

stbi_inline static unsigned int stbi__zreceive(stbi__zbuf *z, int n)
{
   unsigned int k;
   if (z->num_bits < n) stbi__fill_bits(z);
   k = z->code_buffer & ((1 << n) - 1);
   z->code_buffer >>= n;
   z->num_bits -= n;
   return k;
}

static int stbi__zhuffman_decode_slowpath(stbi__zbuf *a, stbi__zhuffman *z)
{
   int b,s,k;
   // not resolved by fast table, so compute it the slow way
   // use jpeg approach, which requires MSbits at top
   k = stbi__bit_reverse(a->code_buffer, 16);
   for (s=STBI__ZFAST_BITS+1; ; ++s)
      if (k < z->maxcode[s])
         break;
   if (s == 16) return -1; // invalid code!
   // code size is s, so:
   b = (k >> (16-s)) - z->firstcode[s] + z->firstsymbol[s];
   STBI_ASSERT(z->size[b] == s);
   a->code_buffer >>= s;
   a->num_bits -= s;
   return z->value[b];
}

stbi_inline static int stbi__zhuffman_decode(stbi__zbuf *a, stbi__zhuffman *z)
{
   int b,s;
   if (a->num_bits < 16) stbi__fill_bits(a);
   b = z->fast[a->code_buffer & STBI__ZFAST_MASK];
   if (b) {
      s = b >> 9;
      a->code_buffer >>= s;
      a->num_bits -= s;
      return b & 511;
   }
   return stbi__zhuffman_decode_slowpath(a, z);
}

static int stbi__zexpand(stbi__zbuf *z, char *zout, int n)  // need to make room for n bytes
{
   char *q;
   int cur, limit, old_limit;
   z->zout = zout;
   if (!z->z_expandable) return stbi__err("output buffer limit","Corrupt PNG");
   cur   = (int) (z->zout     - z->zout_start);
   limit = old_limit = (int) (z->zout_end - z->zout_start);
   while (cur + n > limit)
      limit *= 2;
   q = (char *) STBI_REALLOC_SIZED(z->zout_start, old_limit, limit);
   STBI_NOTUSED(old_limit);
   if (q == NULL) return stbi__err("outofmem", "Out of memory");
   z->zout_start = q;
   z->zout       = q + cur;
   z->zout_end   = q + limit;
   return 1;
}

static const int stbi__zlength_base[31] = {
   3,4,5,6,7,8,9,10,11,13,
   15,17,19,23,27,31,35,43,51,59,
   67,83,99,115,131,163,195,227,258,0,0 };

static const int stbi__zlength_extra[31]=
{ 0,0,0,0,0,0,0,0,1,1,1,1,2,2,2,2,3,3,3,3,4,4,4,4,5,5,5,5,0,0,0 };

static const int stbi__zdist_base[32] = { 1,2,3,4,5,7,9,13,17,25,33,49,65,97,129,193,
257,385,513,769,1025,1537,2049,3073,4097,6145,8193,12289,16385,24577,0,0};

static const int stbi__zdist_extra[32] =
{ 0,0,0,0,1,1,2,2,3,3,4,4,5,5,6,6,7,7,8,8,9,9,10,10,11,11,12,12,13,13};

static int stbi__parse_huffman_block(stbi__zbuf *a)
{
   char *zout = a->zout;
   for(;;) {
      int z = stbi__zhuffman_decode(a, &a->z_length);
      if (z < 256) {
         if (z < 0) return stbi__err("bad huffman code","Corrupt PNG"); // error in huffman codes
         if (zout >= a->zout_end) {
            if (!stbi__zexpand(a, zout, 1)) return 0;
            zout = a->zout;
         }
         *zout++ = (char) z;
      } else {
         stbi_uc *p;
         int len,dist;
         if (z == 256) {
            a->zout = zout;
            return 1;
         }
         z -= 257;
         len = stbi__zlength_base[z];
         if (stbi__zlength_extra[z]) len += stbi__zreceive(a, stbi__zlength_extra[z]);
         z = stbi__zhuffman_decode(a, &a->z_distance);
         if (z < 0) return stbi__err("bad huffman code","Corrupt PNG");
         dist = stbi__zdist_base[z];
         if (stbi__zdist_extra[z]) dist += stbi__zreceive(a, stbi__zdist_extra[z]);
         if (zout - a->zout_start < dist) return stbi__err("bad dist","Corrupt PNG");
         if (zout + len > a->zout_end) {
            if (!stbi__zexpand(a, zout, len)) return 0;
            zout = a->zout;
         }
         p = (stbi_uc *) (zout - dist);
         if (dist == 1) { // run of one byte; common in images.
            stbi_uc v = *p;
            if (len) { do *zout++ = v; while (--len); }
         } else {
            if (len) { do *zout++ = *p++; while (--len); }
         }
      }
   }
}

static int stbi__compute_huffman_codes(stbi__zbuf *a)
{
   static const stbi_uc length_dezigzag[19] = { 16,17,18,0,8,7,9,6,10,5,11,4,12,3,13,2,14,1,15 };
   stbi__zhuffman z_codelength;
   stbi_uc lencodes[286+32+137];//padding for maximum single op
   stbi_uc codelength_sizes[19];
   int i,n;

   int hlit  = stbi__zreceive(a,5) + 257;
   int hdist = stbi__zreceive(a,5) + 1;
   int hclen = stbi__zreceive(a,4) + 4;
   int ntot  = hlit + hdist;

   memset(codelength_sizes, 0, sizeof(codelength_sizes));
   for (i=0; i < hclen; ++i) {
      int s = stbi__zreceive(a,3);
      codelength_sizes[length_dezigzag[i]] = (stbi_uc) s;
   }
   if (!stbi__zbuild_huffman(&z_codelength, codelength_sizes, 19)) return 0;

   n = 0;
   while (n < ntot) {
      int c = stbi__zhuffman_decode(a, &z_codelength);
      if (c < 0 || c >= 19) return stbi__err("bad codelengths", "Corrupt PNG");
      if (c < 16)
         lencodes[n++] = (stbi_uc) c;
      else {
         stbi_uc fill = 0;
         if (c == 16) {
            c = stbi__zreceive(a,2)+3;
            if (n == 0) return stbi__err("bad codelengths", "Corrupt PNG");
            fill = lencodes[n-1];
         } else if (c == 17)
            c = stbi__zreceive(a,3)+3;
         else {
            STBI_ASSERT(c == 18);
            c = stbi__zreceive(a,7)+11;
         }
         if (ntot - n < c) return stbi__err("bad codelengths", "Corrupt PNG");
         memset(lencodes+n, fill, c);
         n += c;
      }
   }
   if (n != ntot) return stbi__err("bad codelengths","Corrupt PNG");
   if (!stbi__zbuild_huffman(&a->z_length, lencodes, hlit)) return 0;
   if (!stbi__zbuild_huffman(&a->z_distance, lencodes+hlit, hdist)) return 0;
   return 1;
}

static int stbi__parse_uncompressed_block(stbi__zbuf *a)
{
   stbi_uc header[4];
   int len,nlen,k;
   if (a->num_bits & 7)
      stbi__zreceive(a, a->num_bits & 7); // discard
   // drain the bit-packed data into header
   k = 0;
   while (a->num_bits > 0) {
      header[k++] = (stbi_uc) (a->code_buffer & 255); // suppress MSVC run-time check
      a->code_buffer >>= 8;
      a->num_bits -= 8;
   }
   STBI_ASSERT(a->num_bits == 0);
   // now fill header the normal way
   while (k < 4)
      header[k++] = stbi__zget8(a);
   len  = header[1] * 256 + header[0];
   nlen = header[3] * 256 + header[2];
   if (nlen != (len ^ 0xffff)) return stbi__err("zlib corrupt","Corrupt PNG");
   if (a->zbuffer + len > a->zbuffer_end) return stbi__err("read past buffer","Corrupt PNG");
   if (a->zout + len > a->zout_end)
      if (!stbi__zexpand(a, a->zout, len)) return 0;
   memcpy(a->zout, a->zbuffer, len);
   a->zbuffer += len;
   a->zout += len;
   return 1;
}

static int stbi__parse_zlib_header(stbi__zbuf *a)
{
   int cmf   = stbi__zget8(a);
   int cm    = cmf & 15;
   /* int cinfo = cmf >> 4; */
   int flg   = stbi__zget8(a);
   if ((cmf*256+flg) % 31 != 0) return stbi__err("bad zlib header","Corrupt PNG"); // zlib spec
   if (flg & 32) return stbi__err("no preset dict","Corrupt PNG"); // preset dictionary not allowed in png
   if (cm != 8) return stbi__err("bad compression","Corrupt PNG"); // DEFLATE required for png
   // window = 1 << (8 + cinfo)... but who cares, we fully buffer output
   return 1;
}

static const stbi_uc stbi__zdefault_length[288] =
{
   8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8, 8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,
   8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8, 8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,
   8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8, 8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,
   8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8, 8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,
   8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8, 9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,
   9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9, 9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,
   9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9, 9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,
   9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9, 9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,
   7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7, 7,7,7,7,7,7,7,7,8,8,8,8,8,8,8,8
};
static const stbi_uc stbi__zdefault_distance[32] =
{
   5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5
};
/*
Init algorithm:
{
   int i;   // use <= to match clearly with spec
   for (i=0; i <= 143; ++i)     stbi__zdefault_length[i]   = 8;
   for (   ; i <= 255; ++i)     stbi__zdefault_length[i]   = 9;
   for (   ; i <= 279; ++i)     stbi__zdefault_length[i]   = 7;
   for (   ; i <= 287; ++i)     stbi__zdefault_length[i]   = 8;

   for (i=0; i <=  31; ++i)     stbi__zdefault_distance[i] = 5;
}
*/

static int stbi__parse_zlib(stbi__zbuf *a, int parse_header)
{
   int final, type;
   if (parse_header)
      if (!stbi__parse_zlib_header(a)) return 0;
   a->num_bits = 0;
   a->code_buffer = 0;
   do {
      final = stbi__zreceive(a,1);
      type = stbi__zreceive(a,2);
      if (type == 0) {
         if (!stbi__parse_uncompressed_block(a)) return 0;
      } else if (type == 3) {
         return 0;
      } else {
         if (type == 1) {
            // use fixed code lengths
            if (!stbi__zbuild_huffman(&a->z_length  , stbi__zdefault_length  , 288)) return 0;
            if (!stbi__zbuild_huffman(&a->z_distance, stbi__zdefault_distance,  32)) return 0;
         } else {
            if (!stbi__compute_huffman_codes(a)) return 0;
         }
         if (!stbi__parse_huffman_block(a)) return 0;
      }
   } while (!final);
   return 1;
}

static int stbi__do_zlib(stbi__zbuf *a, char *obuf, int olen, int exp, int parse_header)
{
   a->zout_start = obuf;
   a->zout       = obuf;
   a->zout_end   = obuf + olen;
   a->z_expandable = exp;

   return stbi__parse_zlib(a, parse_header);
}

STBIDEF char *stbi_zlib_decode_malloc_guesssize(const char *buffer, int len, int initial_size, int *outlen)
{
   stbi__zbuf a;
   char *p = (char *) stbi__malloc(initial_size);
   if (p == NULL) return NULL;
   a.zbuffer = (stbi_uc *) buffer;
   a.zbuffer_end = (stbi_uc *) buffer + len;
   if (stbi__do_zlib(&a, p, initial_size, 1, 1)) {
      if (outlen) *outlen = (int) (a.zout - a.zout_start);
      return a.zout_start;
   } else {
      STBI_FREE(a.zout_start);
      return NULL;
   }
}

STBIDEF char *stbi_zlib_decode_malloc(char const *buffer, int len, int *outlen)
{
   return stbi_zlib_decode_malloc_guesssize(buffer, len, 16384, outlen);
}

STBIDEF char *stbi_zlib_decode_malloc_guesssize_headerflag(const char *buffer, int len, int initial_size, int *outlen, int parse_header)
{
   stbi__zbuf a;
   char *p = (char *) stbi__malloc(initial_size);
   if (p == NULL) return NULL;
   a.zbuffer = (stbi_uc *) buffer;
   a.zbuffer_end = (stbi_uc *) buffer + len;
   if (stbi__do_zlib(&a, p, initial_size, 1, parse_header)) {
      if (outlen) *outlen = (int) (a.zout - a.zout_start);
      return a.zout_start;
   } else {
      STBI_FREE(a.zout_start);
      return NULL;
   }
}

STBIDEF int stbi_zlib_decode_buffer(char *obuffer, int olen, char const *ibuffer, int ilen)
{
   stbi__zbuf a;
   a.zbuffer = (stbi_uc *) ibuffer;
   a.zbuffer_end = (stbi_uc *) ibuffer + ilen;
   if (stbi__do_zlib(&a, obuffer, olen, 0, 1))
      return (int) (a.zout - a.zout_start);
   else
      return -1;
}

STBIDEF char *stbi_zlib_decode_noheader_malloc(char const *buffer, int len, int *outlen)
{
   stbi__zbuf a;
   char *p = (char *) stbi__malloc(16384);
   if (p == NULL) return NULL;
   a.zbuffer = (stbi_uc *) buffer;
   a.zbuffer_end = (stbi_uc *) buffer+len;
   if (stbi__do_zlib(&a, p, 16384, 1, 0)) {
      if (outlen) *outlen = (int) (a.zout - a.zout_start);
      return a.zout_start;
   } else {
      STBI_FREE(a.zout_start);
      return NULL;
   }
}

STBIDEF int stbi_zlib_decode_noheader_buffer(char *obuffer, int olen, const char *ibuffer, int ilen)
{
   stbi__zbuf a;
   a.zbuffer = (stbi_uc *) ibuffer;
   a.zbuffer_end = (stbi_uc *) ibuffer + ilen;
   if (stbi__do_zlib(&a, obuffer, olen, 0, 0))
      return (int) (a.zout - a.zout_start);
   else
      return -1;
}
#endif

// public domain "baseline" PNG decoder   v0.10  Sean Barrett 2006-11-18
//    simple implementation
//      - only 8-bit samples
//      - no CRC checking
//      - allocates lots of intermediate memory
//        - avoids problem of streaming data between subsystems
//        - avoids explicit window management
//    performance
//      - uses stb_zlib, a PD zlib implementation with fast huffman decoding

#ifndef STBI_NO_PNG
typedef struct
{
   stbi__uint32 length;
   stbi__uint32 type;
} stbi__pngchunk;

static stbi__pngchunk stbi__get_chunk_header(stbi__context *s)
{
   stbi__pngchunk c;
   c.length = stbi__get32be(s);
   c.type   = stbi__get32be(s);
   return c;
}

static int stbi__check_png_header(stbi__context *s)
{
   static const stbi_uc png_sig[8] = { 137,80,78,71,13,10,26,10 };
   int i;
   for (i=0; i < 8; ++i)
      if (stbi__get8(s) != png_sig[i]) return stbi__err("bad png sig","Not a PNG");
   return 1;
}

typedef struct
{
   stbi__context *s;
   stbi_uc *idata, *expanded, *out;
   int depth;
} stbi__png;


enum {
   STBI__F_none=0,
   STBI__F_sub=1,
   STBI__F_up=2,
   STBI__F_avg=3,
   STBI__F_paeth=4,
   // synthetic filters used for first scanline to avoid needing a dummy row of 0s
   STBI__F_avg_first,
   STBI__F_paeth_first
};

static stbi_uc first_row_filter[5] =
{
   STBI__F_none,
   STBI__F_sub,
   STBI__F_none,
   STBI__F_avg_first,
   STBI__F_paeth_first
};

static int stbi__paeth(int a, int b, int c)
{
   int p = a + b - c;
   int pa = abs(p-a);
   int pb = abs(p-b);
   int pc = abs(p-c);
   if (pa <= pb && pa <= pc) return a;
   if (pb <= pc) return b;
   return c;
}

static const stbi_uc stbi__depth_scale_table[9] = { 0, 0xff, 0x55, 0, 0x11, 0,0,0, 0x01 };

// create the png data from post-deflated data
static int stbi__create_png_image_raw(stbi__png *a, stbi_uc *raw, stbi__uint32 raw_len, int out_n, stbi__uint32 x, stbi__uint32 y, int depth, int color)
{
   int bytes = (depth == 16? 2 : 1);
   stbi__context *s = a->s;
   stbi__uint32 i,j,stride = x*out_n*bytes;
   stbi__uint32 img_len, img_width_bytes;
   int k;
   int img_n = s->img_n; // copy it into a local for later

   int output_bytes = out_n*bytes;
   int filter_bytes = img_n*bytes;
   int width = x;

   STBI_ASSERT(out_n == s->img_n || out_n == s->img_n+1);
   a->out = (stbi_uc *) stbi__malloc_mad3(x, y, output_bytes, 0); // extra bytes to write off the end into
   if (!a->out) return stbi__err("outofmem", "Out of memory");

   if (!stbi__mad3sizes_valid(img_n, x, depth, 7)) return stbi__err("too large", "Corrupt PNG");
   img_width_bytes = (((img_n * x * depth) + 7) >> 3);
   img_len = (img_width_bytes + 1) * y;

   // we used to check for exact match between raw_len and img_len on non-interlaced PNGs,
   // but issue #276 reported a PNG in the wild that had extra data at the end (all zeros),
   // so just check for raw_len < img_len always.
   if (raw_len < img_len) return stbi__err("not enough pixels","Corrupt PNG");

   for (j=0; j < y; ++j) {
      stbi_uc *cur = a->out + stride*j;
      stbi_uc *prior;
      int filter = *raw++;

      if (filter > 4)
         return stbi__err("invalid filter","Corrupt PNG");

      if (depth < 8) {
         STBI_ASSERT(img_width_bytes <= x);
         cur += x*out_n - img_width_bytes; // store output to the rightmost img_len bytes, so we can decode in place
         filter_bytes = 1;
         width = img_width_bytes;
      }
      prior = cur - stride; // bugfix: need to compute this after 'cur +=' computation above

      // if first row, use special filter that doesn't sample previous row
      if (j == 0) filter = first_row_filter[filter];

      // handle first byte explicitly
      for (k=0; k < filter_bytes; ++k) {
         switch (filter) {
            case STBI__F_none       : cur[k] = raw[k]; break;
            case STBI__F_sub        : cur[k] = raw[k]; break;
            case STBI__F_up         : cur[k] = STBI__BYTECAST(raw[k] + prior[k]); break;
            case STBI__F_avg        : cur[k] = STBI__BYTECAST(raw[k] + (prior[k]>>1)); break;
            case STBI__F_paeth      : cur[k] = STBI__BYTECAST(raw[k] + stbi__paeth(0,prior[k],0)); break;
            case STBI__F_avg_first  : cur[k] = raw[k]; break;
            case STBI__F_paeth_first: cur[k] = raw[k]; break;
         }
      }

      if (depth == 8) {
         if (img_n != out_n)
            cur[img_n] = 255; // first pixel
         raw += img_n;
         cur += out_n;
         prior += out_n;
      } else if (depth == 16) {
         if (img_n != out_n) {
            cur[filter_bytes]   = 255; // first pixel top byte
            cur[filter_bytes+1] = 255; // first pixel bottom byte
         }
         raw += filter_bytes;
         cur += output_bytes;
         prior += output_bytes;
      } else {
         raw += 1;
         cur += 1;
         prior += 1;
      }

      // this is a little gross, so that we don't switch per-pixel or per-component
      if (depth < 8 || img_n == out_n) {
         int nk = (width - 1)*filter_bytes;
         #define STBI__CASE(f) \
             case f:     \
                for (k=0; k < nk; ++k)
         switch (filter) {
            // "none" filter turns into a memcpy here; make that explicit.
            case STBI__F_none:         memcpy(cur, raw, nk); break;
            STBI__CASE(STBI__F_sub)          { cur[k] = STBI__BYTECAST(raw[k] + cur[k-filter_bytes]); } break;
            STBI__CASE(STBI__F_up)           { cur[k] = STBI__BYTECAST(raw[k] + prior[k]); } break;
            STBI__CASE(STBI__F_avg)          { cur[k] = STBI__BYTECAST(raw[k] + ((prior[k] + cur[k-filter_bytes])>>1)); } break;
            STBI__CASE(STBI__F_paeth)        { cur[k] = STBI__BYTECAST(raw[k] + stbi__paeth(cur[k-filter_bytes],prior[k],prior[k-filter_bytes])); } break;
            STBI__CASE(STBI__F_avg_first)    { cur[k] = STBI__BYTECAST(raw[k] + (cur[k-filter_bytes] >> 1)); } break;
            STBI__CASE(STBI__F_paeth_first)  { cur[k] = STBI__BYTECAST(raw[k] + stbi__paeth(cur[k-filter_bytes],0,0)); } break;
         }
         #undef STBI__CASE
         raw += nk;
      } else {
         STBI_ASSERT(img_n+1 == out_n);
         #define STBI__CASE(f) \
             case f:     \
                for (i=x-1; i >= 1; --i, cur[filter_bytes]=255,raw+=filter_bytes,cur+=output_bytes,prior+=output_bytes) \
                   for (k=0; k < filter_bytes; ++k)
         switch (filter) {
            STBI__CASE(STBI__F_none)         { cur[k] = raw[k]; } break;
            STBI__CASE(STBI__F_sub)          { cur[k] = STBI__BYTECAST(raw[k] + cur[k- output_bytes]); } break;
            STBI__CASE(STBI__F_up)           { cur[k] = STBI__BYTECAST(raw[k] + prior[k]); } break;
            STBI__CASE(STBI__F_avg)          { cur[k] = STBI__BYTECAST(raw[k] + ((prior[k] + cur[k- output_bytes])>>1)); } break;
            STBI__CASE(STBI__F_paeth)        { cur[k] = STBI__BYTECAST(raw[k] + stbi__paeth(cur[k- output_bytes],prior[k],prior[k- output_bytes])); } break;
            STBI__CASE(STBI__F_avg_first)    { cur[k] = STBI__BYTECAST(raw[k] + (cur[k- output_bytes] >> 1)); } break;
            STBI__CASE(STBI__F_paeth_first)  { cur[k] = STBI__BYTECAST(raw[k] + stbi__paeth(cur[k- output_bytes],0,0)); } break;
         }
         #undef STBI__CASE

         // the loop above sets the high byte of the pixels' alpha, but for
         // 16 bit png files we also need the low byte set. we'll do that here.
         if (depth == 16) {
            cur = a->out + stride*j; // start at the beginning of the row again
            for (i=0; i < x; ++i,cur+=output_bytes) {
               cur[filter_bytes+1] = 255;
            }
         }
      }
   }

   // we make a separate pass to expand bits to pixels; for performance,
   // this could run two scanlines behind the above code, so it won't
   // intefere with filtering but will still be in the cache.
   if (depth < 8) {
      for (j=0; j < y; ++j) {
         stbi_uc *cur = a->out + stride*j;
         stbi_uc *in  = a->out + stride*j + x*out_n - img_width_bytes;
         // unpack 1/2/4-bit into a 8-bit buffer. allows us to keep the common 8-bit path optimal at minimal cost for 1/2/4-bit
         // png guarante byte alignment, if width is not multiple of 8/4/2 we'll decode dummy trailing data that will be skipped in the later loop
         stbi_uc scale = (color == 0) ? stbi__depth_scale_table[depth] : 1; // scale grayscale values to 0..255 range

         // note that the final byte might overshoot and write more data than desired.
         // we can allocate enough data that this never writes out of memory, but it
         // could also overwrite the next scanline. can it overwrite non-empty data
         // on the next scanline? yes, consider 1-pixel-wide scanlines with 1-bit-per-pixel.
         // so we need to explicitly clamp the final ones

         if (depth == 4) {
            for (k=x*img_n; k >= 2; k-=2, ++in) {
               *cur++ = scale * ((*in >> 4)       );
               *cur++ = scale * ((*in     ) & 0x0f);
            }
            if (k > 0) *cur++ = scale * ((*in >> 4)       );
         } else if (depth == 2) {
            for (k=x*img_n; k >= 4; k-=4, ++in) {
               *cur++ = scale * ((*in >> 6)       );
               *cur++ = scale * ((*in >> 4) & 0x03);
               *cur++ = scale * ((*in >> 2) & 0x03);
               *cur++ = scale * ((*in     ) & 0x03);
            }
            if (k > 0) *cur++ = scale * ((*in >> 6)       );
            if (k > 1) *cur++ = scale * ((*in >> 4) & 0x03);
            if (k > 2) *cur++ = scale * ((*in >> 2) & 0x03);
         } else if (depth == 1) {
            for (k=x*img_n; k >= 8; k-=8, ++in) {
               *cur++ = scale * ((*in >> 7)       );
               *cur++ = scale * ((*in >> 6) & 0x01);
               *cur++ = scale * ((*in >> 5) & 0x01);
               *cur++ = scale * ((*in >> 4) & 0x01);
               *cur++ = scale * ((*in >> 3) & 0x01);
               *cur++ = scale * ((*in >> 2) & 0x01);
               *cur++ = scale * ((*in >> 1) & 0x01);
               *cur++ = scale * ((*in     ) & 0x01);
            }
            if (k > 0) *cur++ = scale * ((*in >> 7)       );
            if (k > 1) *cur++ = scale * ((*in >> 6) & 0x01);
            if (k > 2) *cur++ = scale * ((*in >> 5) & 0x01);
            if (k > 3) *cur++ = scale * ((*in >> 4) & 0x01);
            if (k > 4) *cur++ = scale * ((*in >> 3) & 0x01);
            if (k > 5) *cur++ = scale * ((*in >> 2) & 0x01);
            if (k > 6) *cur++ = scale * ((*in >> 1) & 0x01);
         }
         if (img_n != out_n) {
            int q;
            // insert alpha = 255
            cur = a->out + stride*j;
            if (img_n == 1) {
               for (q=x-1; q >= 0; --q) {
                  cur[q*2+1] = 255;
                  cur[q*2+0] = cur[q];
               }
            } else {
               STBI_ASSERT(img_n == 3);
               for (q=x-1; q >= 0; --q) {
                  cur[q*4+3] = 255;
                  cur[q*4+2] = cur[q*3+2];
                  cur[q*4+1] = cur[q*3+1];
                  cur[q*4+0] = cur[q*3+0];
               }
            }
         }
      }
   } else if (depth == 16) {
      // force the image data from big-endian to platform-native.
      // this is done in a separate pass due to the decoding relying
      // on the data being untouched, but could probably be done
      // per-line during decode if care is taken.
      stbi_uc *cur = a->out;
      stbi__uint16 *cur16 = (stbi__uint16*)cur;

      for(i=0; i < x*y*out_n; ++i,cur16++,cur+=2) {
         *cur16 = (cur[0] << 8) | cur[1];
      }
   }

   return 1;
}

static int stbi__create_png_image(stbi__png *a, stbi_uc *image_data, stbi__uint32 image_data_len, int out_n, int depth, int color, int interlaced)
{
   int bytes = (depth == 16 ? 2 : 1);
   int out_bytes = out_n * bytes;
   stbi_uc *final;
   int p;
   if (!interlaced)
      return stbi__create_png_image_raw(a, image_data, image_data_len, out_n, a->s->img_x, a->s->img_y, depth, color);

   // de-interlacing
   final = (stbi_uc *) stbi__malloc_mad3(a->s->img_x, a->s->img_y, out_bytes, 0);
   for (p=0; p < 7; ++p) {
      int xorig[] = { 0,4,0,2,0,1,0 };
      int yorig[] = { 0,0,4,0,2,0,1 };
      int xspc[]  = { 8,8,4,4,2,2,1 };
      int yspc[]  = { 8,8,8,4,4,2,2 };
      int i,j,x,y;
      // pass1_x[4] = 0, pass1_x[5] = 1, pass1_x[12] = 1
      x = (a->s->img_x - xorig[p] + xspc[p]-1) / xspc[p];
      y = (a->s->img_y - yorig[p] + yspc[p]-1) / yspc[p];
      if (x && y) {
         stbi__uint32 img_len = ((((a->s->img_n * x * depth) + 7) >> 3) + 1) * y;
         if (!stbi__create_png_image_raw(a, image_data, image_data_len, out_n, x, y, depth, color)) {
            STBI_FREE(final);
            return 0;
         }
         for (j=0; j < y; ++j) {
            for (i=0; i < x; ++i) {
               int out_y = j*yspc[p]+yorig[p];
               int out_x = i*xspc[p]+xorig[p];
               memcpy(final + out_y*a->s->img_x*out_bytes + out_x*out_bytes,
                      a->out + (j*x+i)*out_bytes, out_bytes);
            }
         }
         STBI_FREE(a->out);
         image_data += img_len;
         image_data_len -= img_len;
      }
   }
   a->out = final;

   return 1;
}

static int stbi__compute_transparency(stbi__png *z, stbi_uc tc[3], int out_n)
{
   stbi__context *s = z->s;
   stbi__uint32 i, pixel_count = s->img_x * s->img_y;
   stbi_uc *p = z->out;

   // compute color-based transparency, assuming we've
   // already got 255 as the alpha value in the output
   STBI_ASSERT(out_n == 2 || out_n == 4);

   if (out_n == 2) {
      for (i=0; i < pixel_count; ++i) {
         p[1] = (p[0] == tc[0] ? 0 : 255);
         p += 2;
      }
   } else {
      for (i=0; i < pixel_count; ++i) {
         if (p[0] == tc[0] && p[1] == tc[1] && p[2] == tc[2])
            p[3] = 0;
         p += 4;
      }
   }
   return 1;
}

static int stbi__compute_transparency16(stbi__png *z, stbi__uint16 tc[3], int out_n)
{
   stbi__context *s = z->s;
   stbi__uint32 i, pixel_count = s->img_x * s->img_y;
   stbi__uint16 *p = (stbi__uint16*) z->out;

   // compute color-based transparency, assuming we've
   // already got 65535 as the alpha value in the output
   STBI_ASSERT(out_n == 2 || out_n == 4);

   if (out_n == 2) {
      for (i = 0; i < pixel_count; ++i) {
         p[1] = (p[0] == tc[0] ? 0 : 65535);
         p += 2;
      }
   } else {
      for (i = 0; i < pixel_count; ++i) {
         if (p[0] == tc[0] && p[1] == tc[1] && p[2] == tc[2])
            p[3] = 0;
         p += 4;
      }
   }
   return 1;
}

static int stbi__expand_png_palette(stbi__png *a, stbi_uc *palette, int len, int pal_img_n)
{
   stbi__uint32 i, pixel_count = a->s->img_x * a->s->img_y;
   stbi_uc *p, *temp_out, *orig = a->out;

   p = (stbi_uc *) stbi__malloc_mad2(pixel_count, pal_img_n, 0);
   if (p == NULL) return stbi__err("outofmem", "Out of memory");

   // between here and free(out) below, exitting would leak
   temp_out = p;

   if (pal_img_n == 3) {
      for (i=0; i < pixel_count; ++i) {
         int n = orig[i]*4;
         p[0] = palette[n  ];
         p[1] = palette[n+1];
         p[2] = palette[n+2];
         p += 3;
      }
   } else {
      for (i=0; i < pixel_count; ++i) {
         int n = orig[i]*4;
         p[0] = palette[n  ];
         p[1] = palette[n+1];
         p[2] = palette[n+2];
         p[3] = palette[n+3];
         p += 4;
      }
   }
   STBI_FREE(a->out);
   a->out = temp_out;

   STBI_NOTUSED(len);

   return 1;
}

static int stbi__unpremultiply_on_load = 0;
static int stbi__de_iphone_flag = 0;

STBIDEF void stbi_set_unpremultiply_on_load(int flag_true_if_should_unpremultiply)
{
   stbi__unpremultiply_on_load = flag_true_if_should_unpremultiply;
}

STBIDEF void stbi_convert_iphone_png_to_rgb(int flag_true_if_should_convert)
{
   stbi__de_iphone_flag = flag_true_if_should_convert;
}

static void stbi__de_iphone(stbi__png *z)
{
   stbi__context *s = z->s;
   stbi__uint32 i, pixel_count = s->img_x * s->img_y;
   stbi_uc *p = z->out;

   if (s->img_out_n == 3) {  // convert bgr to rgb
      for (i=0; i < pixel_count; ++i) {
         stbi_uc t = p[0];
         p[0] = p[2];
         p[2] = t;
         p += 3;
      }
   } else {
      STBI_ASSERT(s->img_out_n == 4);
      if (stbi__unpremultiply_on_load) {
         // convert bgr to rgb and unpremultiply
         for (i=0; i < pixel_count; ++i) {
            stbi_uc a = p[3];
            stbi_uc t = p[0];
            if (a) {
               stbi_uc half = a / 2;
               p[0] = (p[2] * 255 + half) / a;
               p[1] = (p[1] * 255 + half) / a;
               p[2] = ( t   * 255 + half) / a;
            } else {
               p[0] = p[2];
               p[2] = t;
            }
            p += 4;
         }
      } else {
         // convert bgr to rgb
         for (i=0; i < pixel_count; ++i) {
            stbi_uc t = p[0];
            p[0] = p[2];
            p[2] = t;
            p += 4;
         }
      }
   }
}

#define STBI__PNG_TYPE(a,b,c,d)  (((unsigned) (a) << 24) + ((unsigned) (b) << 16) + ((unsigned) (c) << 8) + (unsigned) (d))

static int stbi__parse_png_file(stbi__png *z, int scan, int req_comp)
{
   stbi_uc palette[1024], pal_img_n=0;
   stbi_uc has_trans=0, tc[3]={0};
   stbi__uint16 tc16[3];
   stbi__uint32 ioff=0, idata_limit=0, i, pal_len=0;
   int first=1,k,interlace=0, color=0, is_iphone=0;
   stbi__context *s = z->s;

   z->expanded = NULL;
   z->idata = NULL;
   z->out = NULL;

   if (!stbi__check_png_header(s)) return 0;

   if (scan == STBI__SCAN_type) return 1;

   for (;;) {
      stbi__pngchunk c = stbi__get_chunk_header(s);
      switch (c.type) {
         case STBI__PNG_TYPE('C','g','B','I'):
            is_iphone = 1;
            stbi__skip(s, c.length);
            break;
         case STBI__PNG_TYPE('I','H','D','R'): {
            int comp,filter;
            if (!first) return stbi__err("multiple IHDR","Corrupt PNG");
            first = 0;
            if (c.length != 13) return stbi__err("bad IHDR len","Corrupt PNG");
            s->img_x = stbi__get32be(s); if (s->img_x > (1 << 24)) return stbi__err("too large","Very large image (corrupt?)");
            s->img_y = stbi__get32be(s); if (s->img_y > (1 << 24)) return stbi__err("too large","Very large image (corrupt?)");
            z->depth = stbi__get8(s);  if (z->depth != 1 && z->depth != 2 && z->depth != 4 && z->depth != 8 && z->depth != 16)  return stbi__err("1/2/4/8/16-bit only","PNG not supported: 1/2/4/8/16-bit only");
            color = stbi__get8(s);  if (color > 6)         return stbi__err("bad ctype","Corrupt PNG");
            if (color == 3 && z->depth == 16)                  return stbi__err("bad ctype","Corrupt PNG");
            if (color == 3) pal_img_n = 3; else if (color & 1) return stbi__err("bad ctype","Corrupt PNG");
            comp  = stbi__get8(s);  if (comp) return stbi__err("bad comp method","Corrupt PNG");
            filter= stbi__get8(s);  if (filter) return stbi__err("bad filter method","Corrupt PNG");
            interlace = stbi__get8(s); if (interlace>1) return stbi__err("bad interlace method","Corrupt PNG");
            if (!s->img_x || !s->img_y) return stbi__err("0-pixel image","Corrupt PNG");
            if (!pal_img_n) {
               s->img_n = (color & 2 ? 3 : 1) + (color & 4 ? 1 : 0);
               if ((1 << 30) / s->img_x / s->img_n < s->img_y) return stbi__err("too large", "Image too large to decode");
               if (scan == STBI__SCAN_header) return 1;
            } else {
               // if paletted, then pal_n is our final components, and
               // img_n is # components to decompress/filter.
               s->img_n = 1;
               if ((1 << 30) / s->img_x / 4 < s->img_y) return stbi__err("too large","Corrupt PNG");
               // if SCAN_header, have to scan to see if we have a tRNS
            }
            break;
         }

         case STBI__PNG_TYPE('P','L','T','E'):  {
            if (first) return stbi__err("first not IHDR", "Corrupt PNG");
            if (c.length > 256*3) return stbi__err("invalid PLTE","Corrupt PNG");
            pal_len = c.length / 3;
            if (pal_len * 3 != c.length) return stbi__err("invalid PLTE","Corrupt PNG");
            for (i=0; i < pal_len; ++i) {
               palette[i*4+0] = stbi__get8(s);
               palette[i*4+1] = stbi__get8(s);
               palette[i*4+2] = stbi__get8(s);
               palette[i*4+3] = 255;
            }
            break;
         }

         case STBI__PNG_TYPE('t','R','N','S'): {
            if (first) return stbi__err("first not IHDR", "Corrupt PNG");
            if (z->idata) return stbi__err("tRNS after IDAT","Corrupt PNG");
            if (pal_img_n) {
               if (scan == STBI__SCAN_header) { s->img_n = 4; return 1; }
               if (pal_len == 0) return stbi__err("tRNS before PLTE","Corrupt PNG");
               if (c.length > pal_len) return stbi__err("bad tRNS len","Corrupt PNG");
               pal_img_n = 4;
               for (i=0; i < c.length; ++i)
                  palette[i*4+3] = stbi__get8(s);
            } else {
               if (!(s->img_n & 1)) return stbi__err("tRNS with alpha","Corrupt PNG");
               if (c.length != (stbi__uint32) s->img_n*2) return stbi__err("bad tRNS len","Corrupt PNG");
               has_trans = 1;
               if (z->depth == 16) {
                  for (k = 0; k < s->img_n; ++k) tc16[k] = (stbi__uint16)stbi__get16be(s); // copy the values as-is
               } else {
                  for (k = 0; k < s->img_n; ++k) tc[k] = (stbi_uc)(stbi__get16be(s) & 255) * stbi__depth_scale_table[z->depth]; // non 8-bit images will be larger
               }
            }
            break;
         }

         case STBI__PNG_TYPE('I','D','A','T'): {
            if (first) return stbi__err("first not IHDR", "Corrupt PNG");
            if (pal_img_n && !pal_len) return stbi__err("no PLTE","Corrupt PNG");
            if (scan == STBI__SCAN_header) { s->img_n = pal_img_n; return 1; }
            if ((int)(ioff + c.length) < (int)ioff) return 0;
            if (ioff + c.length > idata_limit) {
               stbi__uint32 idata_limit_old = idata_limit;
               stbi_uc *p;
               if (idata_limit == 0) idata_limit = c.length > 4096 ? c.length : 4096;
               while (ioff + c.length > idata_limit)
                  idata_limit *= 2;
               STBI_NOTUSED(idata_limit_old);
               p = (stbi_uc *) STBI_REALLOC_SIZED(z->idata, idata_limit_old, idata_limit); if (p == NULL) return stbi__err("outofmem", "Out of memory");
               z->idata = p;
            }
            if (!stbi__getn(s, z->idata+ioff,c.length)) return stbi__err("outofdata","Corrupt PNG");
            ioff += c.length;
            break;
         }

         case STBI__PNG_TYPE('I','E','N','D'): {
            stbi__uint32 raw_len, bpl;
            if (first) return stbi__err("first not IHDR", "Corrupt PNG");
            if (scan != STBI__SCAN_load) return 1;
            if (z->idata == NULL) return stbi__err("no IDAT","Corrupt PNG");
            // initial guess for decoded data size to avoid unnecessary reallocs
            bpl = (s->img_x * z->depth + 7) / 8; // bytes per line, per component
            raw_len = bpl * s->img_y * s->img_n /* pixels */ + s->img_y /* filter mode per row */;
            z->expanded = (stbi_uc *) stbi_zlib_decode_malloc_guesssize_headerflag((char *) z->idata, ioff, raw_len, (int *) &raw_len, !is_iphone);
            if (z->expanded == NULL) return 0; // zlib should set error
            STBI_FREE(z->idata); z->idata = NULL;
            if ((req_comp == s->img_n+1 && req_comp != 3 && !pal_img_n) || has_trans)
               s->img_out_n = s->img_n+1;
            else
               s->img_out_n = s->img_n;
            if (!stbi__create_png_image(z, z->expanded, raw_len, s->img_out_n, z->depth, color, interlace)) return 0;
            if (has_trans) {
               if (z->depth == 16) {
                  if (!stbi__compute_transparency16(z, tc16, s->img_out_n)) return 0;
               } else {
                  if (!stbi__compute_transparency(z, tc, s->img_out_n)) return 0;
               }
            }
            if (is_iphone && stbi__de_iphone_flag && s->img_out_n > 2)
               stbi__de_iphone(z);
            if (pal_img_n) {
               // pal_img_n == 3 or 4
               s->img_n = pal_img_n; // record the actual colors we had
               s->img_out_n = pal_img_n;
               if (req_comp >= 3) s->img_out_n = req_comp;
               if (!stbi__expand_png_palette(z, palette, pal_len, s->img_out_n))
                  return 0;
            } else if (has_trans) {
               // non-paletted image with tRNS -> source image has (constant) alpha
               ++s->img_n;
            }
            STBI_FREE(z->expanded); z->expanded = NULL;
            // end of PNG chunk, read and flags CRC
            stbi__get32be(s);
            return 1;
         }

         default:
            // if critical, fail
            if (first) return stbi__err("first not IHDR", "Corrupt PNG");
            if ((c.type & (1 << 29)) == 0) {
               #ifndef STBI_NO_FAILURE_STRINGS
               // not threadsafe
               static char invalid_chunk[] = "XXXX PNG chunk not known";
               invalid_chunk[0] = STBI__BYTECAST(c.type >> 24);
               invalid_chunk[1] = STBI__BYTECAST(c.type >> 16);
               invalid_chunk[2] = STBI__BYTECAST(c.type >>  8);
               invalid_chunk[3] = STBI__BYTECAST(c.type >>  0);
               #endif
               return stbi__err(invalid_chunk, "PNG not supported: unknown PNG chunk type");
            }
            stbi__skip(s, c.length);
            break;
      }
      // end of PNG chunk, read and flags CRC
      stbi__get32be(s);
   }
}

static void *stbi__do_png(stbi__png *p, int *x, int *y, int *n, int req_comp, stbi__result_info *ri)
{
   void *result=NULL;
   if (req_comp < 0 || req_comp > 4) return stbi__errpuc("bad req_comp", "Internal error");
   if (stbi__parse_png_file(p, STBI__SCAN_load, req_comp)) {
      if (p->depth < 8)
         ri->bits_per_channel = 8;
      else
         ri->bits_per_channel = p->depth;
      result = p->out;
      p->out = NULL;
      if (req_comp && req_comp != p->s->img_out_n) {
         if (ri->bits_per_channel == 8)
            result = stbi__convert_format((unsigned char *) result, p->s->img_out_n, req_comp, p->s->img_x, p->s->img_y);
         else
            result = stbi__convert_format16((stbi__uint16 *) result, p->s->img_out_n, req_comp, p->s->img_x, p->s->img_y);
         p->s->img_out_n = req_comp;
         if (result == NULL) return result;
      }
      *x = p->s->img_x;
      *y = p->s->img_y;
      if (n) *n = p->s->img_n;
   }
   STBI_FREE(p->out);      p->out      = NULL;
   STBI_FREE(p->expanded); p->expanded = NULL;
   STBI_FREE(p->idata);    p->idata    = NULL;

   return result;
}

static void *stbi__png_load(stbi__context *s, int *x, int *y, int *comp, int req_comp, stbi__result_info *ri)
{
   stbi__png p;
   p.s = s;
   return stbi__do_png(&p, x,y,comp,req_comp, ri);
}

static int stbi__png_test(stbi__context *s)
{
   int r;
   r = stbi__check_png_header(s);
   stbi__rewind(s);
   return r;
}

static int stbi__png_info_raw(stbi__png *p, int *x, int *y, int *comp)
{
   if (!stbi__parse_png_file(p, STBI__SCAN_header, 0)) {
      stbi__rewind( p->s );
      return 0;
   }
   if (x) *x = p->s->img_x;
   if (y) *y = p->s->img_y;
   if (comp) *comp = p->s->img_n;
   return 1;
}

static int stbi__png_info(stbi__context *s, int *x, int *y, int *comp)
{
   stbi__png p;
   p.s = s;
   return stbi__png_info_raw(&p, x, y, comp);
}

static int stbi__png_is16(stbi__context *s)
{
   stbi__png p;
   p.s = s;
   if (!stbi__png_info_raw(&p, NULL, NULL, NULL))
	   return 0;
   if (p.depth != 16) {
      stbi__rewind(p.s);
      return 0;
   }
   return 1;
}
#endif

// Microsoft/Windows BMP image

#ifndef STBI_NO_BMP
static int stbi__bmp_test_raw(stbi__context *s)
{
   int r;
   int sz;
   if (stbi__get8(s) != 'B') return 0;
   if (stbi__get8(s) != 'M') return 0;
   stbi__get32le(s); // discard filesize
   stbi__get16le(s); // discard reserved
   stbi__get16le(s); // discard reserved
   stbi__get32le(s); // discard data offset
   sz = stbi__get32le(s);
   r = (sz == 12 || sz == 40 || sz == 56 || sz == 108 || sz == 124);
   return r;
}

static int stbi__bmp_test(stbi__context *s)
{
   int r = stbi__bmp_test_raw(s);
   stbi__rewind(s);
   return r;
}


// returns 0..31 for the highest set bit
static int stbi__high_bit(unsigned int z)
{
   int n=0;
   if (z == 0) return -1;
   if (z >= 0x10000) { n += 16; z >>= 16; }
   if (z >= 0x00100) { n +=  8; z >>=  8; }
   if (z >= 0x00010) { n +=  4; z >>=  4; }
   if (z >= 0x00004) { n +=  2; z >>=  2; }
   if (z >= 0x00002) { n +=  1;/* >>=  1;*/ }
   return n;
}

static int stbi__bitcount(unsigned int a)
{
   a = (a & 0x55555555) + ((a >>  1) & 0x55555555); // max 2
   a = (a & 0x33333333) + ((a >>  2) & 0x33333333); // max 4
   a = (a + (a >> 4)) & 0x0f0f0f0f; // max 8 per 4, now 8 bits
   a = (a + (a >> 8)); // max 16 per 8 bits
   a = (a + (a >> 16)); // max 32 per 8 bits
   return a & 0xff;
}

// extract an arbitrarily-aligned N-bit value (N=bits)
// from v, and then make it 8-bits long and fractionally
// extend it to full full range.
static int stbi__shiftsigned(unsigned int v, int shift, int bits)
{
   static unsigned int mul_table[9] = {
      0,
      0xff/*0b11111111*/, 0x55/*0b01010101*/, 0x49/*0b01001001*/, 0x11/*0b00010001*/,
      0x21/*0b00100001*/, 0x41/*0b01000001*/, 0x81/*0b10000001*/, 0x01/*0b00000001*/,
   };
   static unsigned int shift_table[9] = {
      0, 0,0,1,0,2,4,6,0,
   };
   if (shift < 0)
      v <<= -shift;
   else
      v >>= shift;
   STBI_ASSERT(v < 256);
   v >>= (8-bits);
   STBI_ASSERT(bits >= 0 && bits <= 8);
   return (int) ((unsigned) v * mul_table[bits]) >> shift_table[bits];
}

typedef struct
{
   int bpp, offset, hsz;
   unsigned int mr,mg,mb,ma, all_a;
   int extra_read;
} stbi__bmp_data;

static void *stbi__bmp_parse_header(stbi__context *s, stbi__bmp_data *info)
{
   int hsz;
   if (stbi__get8(s) != 'B' || stbi__get8(s) != 'M') return stbi__errpuc("not BMP", "Corrupt BMP");
   stbi__get32le(s); // discard filesize
   stbi__get16le(s); // discard reserved
   stbi__get16le(s); // discard reserved
   info->offset = stbi__get32le(s);
   info->hsz = hsz = stbi__get32le(s);
   info->mr = info->mg = info->mb = info->ma = 0;
   info->extra_read = 14;

   if (hsz != 12 && hsz != 40 && hsz != 56 && hsz != 108 && hsz != 124) return stbi__errpuc("unknown BMP", "BMP type not supported: unknown");
   if (hsz == 12) {
      s->img_x = stbi__get16le(s);
      s->img_y = stbi__get16le(s);
   } else {
      s->img_x = stbi__get32le(s);
      s->img_y = stbi__get32le(s);
   }
   if (stbi__get16le(s) != 1) return stbi__errpuc("bad BMP", "bad BMP");
   info->bpp = stbi__get16le(s);
   if (hsz != 12) {
      int compress = stbi__get32le(s);
      if (compress == 1 || compress == 2) return stbi__errpuc("BMP RLE", "BMP type not supported: RLE");
      stbi__get32le(s); // discard sizeof
      stbi__get32le(s); // discard hres
      stbi__get32le(s); // discard vres
      stbi__get32le(s); // discard colorsused
      stbi__get32le(s); // discard max important
      if (hsz == 40 || hsz == 56) {
         if (hsz == 56) {
            stbi__get32le(s);
            stbi__get32le(s);
            stbi__get32le(s);
            stbi__get32le(s);
         }
         if (info->bpp == 16 || info->bpp == 32) {
            if (compress == 0) {
               if (info->bpp == 32) {
                  info->mr = 0xffu << 16;
                  info->mg = 0xffu <<  8;
                  info->mb = 0xffu <<  0;
                  info->ma = 0xffu << 24;
                  info->all_a = 0; // if all_a is 0 at end, then we loaded alpha channel but it was all 0
               } else {
                  info->mr = 31u << 10;
                  info->mg = 31u <<  5;
                  info->mb = 31u <<  0;
               }
            } else if (compress == 3) {
               info->mr = stbi__get32le(s);
               info->mg = stbi__get32le(s);
               info->mb = stbi__get32le(s);
               info->extra_read += 12;
               // not documented, but generated by photoshop and handled by mspaint
               if (info->mr == info->mg && info->mg == info->mb) {
                  // ?!?!?
                  return stbi__errpuc("bad BMP", "bad BMP");
               }
            } else
               return stbi__errpuc("bad BMP", "bad BMP");
         }
      } else {
         int i;
         if (hsz != 108 && hsz != 124)
            return stbi__errpuc("bad BMP", "bad BMP");
         info->mr = stbi__get32le(s);
         info->mg = stbi__get32le(s);
         info->mb = stbi__get32le(s);
         info->ma = stbi__get32le(s);
         stbi__get32le(s); // discard color space
         for (i=0; i < 12; ++i)
            stbi__get32le(s); // discard color space parameters
         if (hsz == 124) {
            stbi__get32le(s); // discard rendering intent
            stbi__get32le(s); // discard offset of profile data
            stbi__get32le(s); // discard size of profile data
            stbi__get32le(s); // discard reserved
         }
      }
   }
   return (void *) 1;
}


static void *stbi__bmp_load(stbi__context *s, int *x, int *y, int *comp, int req_comp, stbi__result_info *ri)
{
   stbi_uc *out;
   unsigned int mr=0,mg=0,mb=0,ma=0, all_a;
   stbi_uc pal[256][4];
   int psize=0,i,j,width;
   int flip_vertically, pad, target;
   stbi__bmp_data info;
   STBI_NOTUSED(ri);

   info.all_a = 255;
   if (stbi__bmp_parse_header(s, &info) == NULL)
      return NULL; // error code already set

   flip_vertically = ((int) s->img_y) > 0;
   s->img_y = abs((int) s->img_y);

   mr = info.mr;
   mg = info.mg;
   mb = info.mb;
   ma = info.ma;
   all_a = info.all_a;

   if (info.hsz == 12) {
      if (info.bpp < 24)
         psize = (info.offset - info.extra_read - 24) / 3;
   } else {
      if (info.bpp < 16)
         psize = (info.offset - info.extra_read - info.hsz) >> 2;
   }
   if (psize == 0) {
      STBI_ASSERT(info.offset == (s->img_buffer - s->buffer_start));
   }

   if (info.bpp == 24 && ma == 0xff000000)
      s->img_n = 3;
   else
      s->img_n = ma ? 4 : 3;
   if (req_comp && req_comp >= 3) // we can directly decode 3 or 4
      target = req_comp;
   else
      target = s->img_n; // if they want monochrome, we'll post-convert

   // sanity-check size
   if (!stbi__mad3sizes_valid(target, s->img_x, s->img_y, 0))
      return stbi__errpuc("too large", "Corrupt BMP");

   out = (stbi_uc *) stbi__malloc_mad3(target, s->img_x, s->img_y, 0);
   if (!out) return stbi__errpuc("outofmem", "Out of memory");
   if (info.bpp < 16) {
      int z=0;
      if (psize == 0 || psize > 256) { STBI_FREE(out); return stbi__errpuc("invalid", "Corrupt BMP"); }
      for (i=0; i < psize; ++i) {
         pal[i][2] = stbi__get8(s);
         pal[i][1] = stbi__get8(s);
         pal[i][0] = stbi__get8(s);
         if (info.hsz != 12) stbi__get8(s);
         pal[i][3] = 255;
      }
      stbi__skip(s, info.offset - info.extra_read - info.hsz - psize * (info.hsz == 12 ? 3 : 4));
      if (info.bpp == 1) width = (s->img_x + 7) >> 3;
      else if (info.bpp == 4) width = (s->img_x + 1) >> 1;
      else if (info.bpp == 8) width = s->img_x;
      else { STBI_FREE(out); return stbi__errpuc("bad bpp", "Corrupt BMP"); }
      pad = (-width)&3;
      if (info.bpp == 1) {
         for (j=0; j < (int) s->img_y; ++j) {
            int bit_offset = 7, v = stbi__get8(s);
            for (i=0; i < (int) s->img_x; ++i) {
               int color = (v>>bit_offset)&0x1;
               out[z++] = pal[color][0];
               out[z++] = pal[color][1];
               out[z++] = pal[color][2];
               if (target == 4) out[z++] = 255;
               if (i+1 == (int) s->img_x) break;
               if((--bit_offset) < 0) {
                  bit_offset = 7;
                  v = stbi__get8(s);
               }
            }
            stbi__skip(s, pad);
         }
      } else {
         for (j=0; j < (int) s->img_y; ++j) {
            for (i=0; i < (int) s->img_x; i += 2) {
               int v=stbi__get8(s),v2=0;
               if (info.bpp == 4) {
                  v2 = v & 15;
                  v >>= 4;
               }
               out[z++] = pal[v][0];
               out[z++] = pal[v][1];
               out[z++] = pal[v][2];
               if (target == 4) out[z++] = 255;
               if (i+1 == (int) s->img_x) break;
               v = (info.bpp == 8) ? stbi__get8(s) : v2;
               out[z++] = pal[v][0];
               out[z++] = pal[v][1];
               out[z++] = pal[v][2];
               if (target == 4) out[z++] = 255;
            }
            stbi__skip(s, pad);
         }
      }
   } else {
      int rshift=0,gshift=0,bshift=0,ashift=0,rcount=0,gcount=0,bcount=0,acount=0;
      int z = 0;
      int easy=0;
      stbi__skip(s, info.offset - info.extra_read - info.hsz);
      if (info.bpp == 24) width = 3 * s->img_x;
      else if (info.bpp == 16) width = 2*s->img_x;
      else /* bpp = 32 and pad = 0 */ width=0;
      pad = (-width) & 3;
      if (info.bpp == 24) {
         easy = 1;
      } else if (info.bpp == 32) {
         if (mb == 0xff && mg == 0xff00 && mr == 0x00ff0000 && ma == 0xff000000)
            easy = 2;
      }
      if (!easy) {
         if (!mr || !mg || !mb) { STBI_FREE(out); return stbi__errpuc("bad masks", "Corrupt BMP"); }
         // right shift amt to put high bit in position #7
         rshift = stbi__high_bit(mr)-7; rcount = stbi__bitcount(mr);
         gshift = stbi__high_bit(mg)-7; gcount = stbi__bitcount(mg);
         bshift = stbi__high_bit(mb)-7; bcount = stbi__bitcount(mb);
         ashift = stbi__high_bit(ma)-7; acount = stbi__bitcount(ma);
      }
      for (j=0; j < (int) s->img_y; ++j) {
         if (easy) {
            for (i=0; i < (int) s->img_x; ++i) {
               unsigned char a;
               out[z+2] = stbi__get8(s);
               out[z+1] = stbi__get8(s);
               out[z+0] = stbi__get8(s);
               z += 3;
               a = (easy == 2 ? stbi__get8(s) : 255);
               all_a |= a;
               if (target == 4) out[z++] = a;
            }
         } else {
            int bpp = info.bpp;
            for (i=0; i < (int) s->img_x; ++i) {
               stbi__uint32 v = (bpp == 16 ? (stbi__uint32) stbi__get16le(s) : stbi__get32le(s));
               unsigned int a;
               out[z++] = STBI__BYTECAST(stbi__shiftsigned(v & mr, rshift, rcount));
               out[z++] = STBI__BYTECAST(stbi__shiftsigned(v & mg, gshift, gcount));
               out[z++] = STBI__BYTECAST(stbi__shiftsigned(v & mb, bshift, bcount));
               a = (ma ? stbi__shiftsigned(v & ma, ashift, acount) : 255);
               all_a |= a;
               if (target == 4) out[z++] = STBI__BYTECAST(a);
            }
         }
         stbi__skip(s, pad);
      }
   }

   // if alpha channel is all 0s, replace with all 255s
   if (target == 4 && all_a == 0)
      for (i=4*s->img_x*s->img_y-1; i >= 0; i -= 4)
         out[i] = 255;

   if (flip_vertically) {
      stbi_uc t;
      for (j=0; j < (int) s->img_y>>1; ++j) {
         stbi_uc *p1 = out +      j     *s->img_x*target;
         stbi_uc *p2 = out + (s->img_y-1-j)*s->img_x*target;
         for (i=0; i < (int) s->img_x*target; ++i) {
            t = p1[i]; p1[i] = p2[i]; p2[i] = t;
         }
      }
   }

   if (req_comp && req_comp != target) {
      out = stbi__convert_format(out, target, req_comp, s->img_x, s->img_y);
      if (out == NULL) return out; // stbi__convert_format frees input on failure
   }

   *x = s->img_x;
   *y = s->img_y;
   if (comp) *comp = s->img_n;
   return out;
}
#endif

// Targa Truevision - TGA
// by Jonathan Dummer
#ifndef STBI_NO_TGA
// returns STBI_rgb or whatever, 0 on error
static int stbi__tga_get_comp(int bits_per_pixel, int is_grey, int* is_rgb16)
{
   // only RGB or RGBA (incl. 16bit) or grey allowed
   if (is_rgb16) *is_rgb16 = 0;
   switch(bits_per_pixel) {
      case 8:  return STBI_grey;
      case 16: if(is_grey) return STBI_grey_alpha;
               // fallthrough
      case 15: if(is_rgb16) *is_rgb16 = 1;
               return STBI_rgb;
      case 24: // fallthrough
      case 32: return bits_per_pixel/8;
      default: return 0;
   }
}

static int stbi__tga_info(stbi__context *s, int *x, int *y, int *comp)
{
    int tga_w, tga_h, tga_comp, tga_image_type, tga_bits_per_pixel, tga_colormap_bpp;
    int sz, tga_colormap_type;
    stbi__get8(s);                   // discard Offset
    tga_colormap_type = stbi__get8(s); // colormap type
    if( tga_colormap_type > 1 ) {
        stbi__rewind(s);
        return 0;      // only RGB or indexed allowed
    }
    tga_image_type = stbi__get8(s); // image type
    if ( tga_colormap_type == 1 ) { // colormapped (paletted) image
        if (tga_image_type != 1 && tga_image_type != 9) {
            stbi__rewind(s);
            return 0;
        }
        stbi__skip(s,4);       // flags index of first colormap entry and number of entries
        sz = stbi__get8(s);    //   check bits per palette color entry
        if ( (sz != 8) && (sz != 15) && (sz != 16) && (sz != 24) && (sz != 32) ) {
            stbi__rewind(s);
            return 0;
        }
        stbi__skip(s,4);       // flags image x and y origin
        tga_colormap_bpp = sz;
    } else { // "normal" image w/o colormap - only RGB or grey allowed, +/- RLE
        if ( (tga_image_type != 2) && (tga_image_type != 3) && (tga_image_type != 10) && (tga_image_type != 11) ) {
            stbi__rewind(s);
            return 0; // only RGB or grey allowed, +/- RLE
        }
        stbi__skip(s,9); // flags colormap specification and image x/y origin
        tga_colormap_bpp = 0;
    }
    tga_w = stbi__get16le(s);
    if( tga_w < 1 ) {
        stbi__rewind(s);
        return 0;   // test width
    }
    tga_h = stbi__get16le(s);
    if( tga_h < 1 ) {
        stbi__rewind(s);
        return 0;   // test height
    }
    tga_bits_per_pixel = stbi__get8(s); // bits per pixel
    stbi__get8(s); // ignore alpha bits
    if (tga_colormap_bpp != 0) {
        if((tga_bits_per_pixel != 8) && (tga_bits_per_pixel != 16)) {
            // when using a colormap, tga_bits_per_pixel is the size of the indexes
            // I don't think anything but 8 or 16bit indexes makes sense
            stbi__rewind(s);
            return 0;
        }
        tga_comp = stbi__tga_get_comp(tga_colormap_bpp, 0, NULL);
    } else {
        tga_comp = stbi__tga_get_comp(tga_bits_per_pixel, (tga_image_type == 3) || (tga_image_type == 11), NULL);
    }
    if(!tga_comp) {
      stbi__rewind(s);
      return 0;
    }
    if (x) *x = tga_w;
    if (y) *y = tga_h;
    if (comp) *comp = tga_comp;
    return 1;                   // seems to have passed everything
}

static int stbi__tga_test(stbi__context *s)
{
   int res = 0;
   int sz, tga_color_type;
   stbi__get8(s);      //   discard Offset
   tga_color_type = stbi__get8(s);   //   color type
   if ( tga_color_type > 1 ) goto errorEnd;   //   only RGB or indexed allowed
   sz = stbi__get8(s);   //   image type
   if ( tga_color_type == 1 ) { // colormapped (paletted) image
      if (sz != 1 && sz != 9) goto errorEnd; // colortype 1 demands image type 1 or 9
      stbi__skip(s,4);       // flags index of first colormap entry and number of entries
      sz = stbi__get8(s);    //   check bits per palette color entry
      if ( (sz != 8) && (sz != 15) && (sz != 16) && (sz != 24) && (sz != 32) ) goto errorEnd;
      stbi__skip(s,4);       // flags image x and y origin
   } else { // "normal" image w/o colormap
      if ( (sz != 2) && (sz != 3) && (sz != 10) && (sz != 11) ) goto errorEnd; // only RGB or grey allowed, +/- RLE
      stbi__skip(s,9); // flags colormap specification and image x/y origin
   }
   if ( stbi__get16le(s) < 1 ) goto errorEnd;      //   test width
   if ( stbi__get16le(s) < 1 ) goto errorEnd;      //   test height
   sz = stbi__get8(s);   //   bits per pixel
   if ( (tga_color_type == 1) && (sz != 8) && (sz != 16) ) goto errorEnd; // for colormapped images, bpp is size of an index
   if ( (sz != 8) && (sz != 15) && (sz != 16) && (sz != 24) && (sz != 32) ) goto errorEnd;

   res = 1; // if we got this far, everything's good and we can return 1 instead of 0

errorEnd:
   stbi__rewind(s);
   return res;
}

// read 16bit value and convert to 24bit RGB
static void stbi__tga_read_rgb16(stbi__context *s, stbi_uc* out)
{
   stbi__uint16 px = (stbi__uint16)stbi__get16le(s);
   stbi__uint16 fiveBitMask = 31;
   // we have 3 channels with 5bits each
   int r = (px >> 10) & fiveBitMask;
   int g = (px >> 5) & fiveBitMask;
   int b = px & fiveBitMask;
   // Note that this saves the data in RGB(A) order, so it doesn't need to be swapped later
   out[0] = (stbi_uc)((r * 255)/31);
   out[1] = (stbi_uc)((g * 255)/31);
   out[2] = (stbi_uc)((b * 255)/31);

   // some people claim that the most significant bit might be used for alpha
   // (possibly if an alpha-bit is set in the "image descriptor byte")
   // but that only made 16bit test images completely translucent..
   // so let's treat all 15 and 16bit TGAs as RGB with no alpha.
}

static void *stbi__tga_load(stbi__context *s, int *x, int *y, int *comp, int req_comp, stbi__result_info *ri)
{
   //   read in the TGA header stuff
   int tga_offset = stbi__get8(s);
   int tga_indexed = stbi__get8(s);
   int tga_image_type = stbi__get8(s);
   int tga_is_RLE = 0;
   int tga_palette_start = stbi__get16le(s);
   int tga_palette_len = stbi__get16le(s);
   int tga_palette_bits = stbi__get8(s);
   int tga_x_origin = stbi__get16le(s);
   int tga_y_origin = stbi__get16le(s);
   int tga_width = stbi__get16le(s);
   int tga_height = stbi__get16le(s);
   int tga_bits_per_pixel = stbi__get8(s);
   int tga_comp, tga_rgb16=0;
   int tga_inverted = stbi__get8(s);
   // int tga_alpha_bits = tga_inverted & 15; // the 4 lowest bits - unused (useless?)
   //   image data
   unsigned char *tga_data;
   unsigned char *tga_palette = NULL;
   int i, j;
   unsigned char raw_data[4] = {0};
   int RLE_count = 0;
   int RLE_repeating = 0;
   int read_next_pixel = 1;
   STBI_NOTUSED(ri);
   STBI_NOTUSED(tga_x_origin); // @TODO
   STBI_NOTUSED(tga_y_origin); // @TODO

   //   do a tiny bit of precessing
   if ( tga_image_type >= 8 )
   {
      tga_image_type -= 8;
      tga_is_RLE = 1;
   }
   tga_inverted = 1 - ((tga_inverted >> 5) & 1);

   //   If I'm paletted, then I'll use the number of bits from the palette
   if ( tga_indexed ) tga_comp = stbi__tga_get_comp(tga_palette_bits, 0, &tga_rgb16);
   else tga_comp = stbi__tga_get_comp(tga_bits_per_pixel, (tga_image_type == 3), &tga_rgb16);

   if(!tga_comp) // shouldn't really happen, stbi__tga_test() should have ensured basic consistency
      return stbi__errpuc("bad format", "Can't find out TGA pixelformat");

   //   tga info
   *x = tga_width;
   *y = tga_height;
   if (comp) *comp = tga_comp;

   if (!stbi__mad3sizes_valid(tga_width, tga_height, tga_comp, 0))
      return stbi__errpuc("too large", "Corrupt TGA");

   tga_data = (unsigned char*)stbi__malloc_mad3(tga_width, tga_height, tga_comp, 0);
   if (!tga_data) return stbi__errpuc("outofmem", "Out of memory");

   // flags to the data's starting position (offset usually = 0)
   stbi__skip(s, tga_offset );

   if ( !tga_indexed && !tga_is_RLE && !tga_rgb16 ) {
      for (i=0; i < tga_height; ++i) {
         int row = tga_inverted ? tga_height -i - 1 : i;
         stbi_uc *tga_row = tga_data + row*tga_width*tga_comp;
         stbi__getn(s, tga_row, tga_width * tga_comp);
      }
   } else  {
      //   do I need to load a palette?
      if ( tga_indexed)
      {
         //   any data to flags? (offset usually = 0)
         stbi__skip(s, tga_palette_start );
         //   load the palette
         tga_palette = (unsigned char*)stbi__malloc_mad2(tga_palette_len, tga_comp, 0);
         if (!tga_palette) {
            STBI_FREE(tga_data);
            return stbi__errpuc("outofmem", "Out of memory");
         }
         if (tga_rgb16) {
            stbi_uc *pal_entry = tga_palette;
            STBI_ASSERT(tga_comp == STBI_rgb);
            for (i=0; i < tga_palette_len; ++i) {
               stbi__tga_read_rgb16(s, pal_entry);
               pal_entry += tga_comp;
            }
         } else if (!stbi__getn(s, tga_palette, tga_palette_len * tga_comp)) {
               STBI_FREE(tga_data);
               STBI_FREE(tga_palette);
               return stbi__errpuc("bad palette", "Corrupt TGA");
         }
      }
      //   load the data
      for (i=0; i < tga_width * tga_height; ++i)
      {
         //   if I'm in RLE mode, do I need to get a RLE stbi__pngchunk?
         if ( tga_is_RLE )
         {
            if ( RLE_count == 0 )
            {
               //   yep, get the next byte as a RLE command
               int RLE_cmd = stbi__get8(s);
               RLE_count = 1 + (RLE_cmd & 127);
               RLE_repeating = RLE_cmd >> 7;
               read_next_pixel = 1;
            } else if ( !RLE_repeating )
            {
               read_next_pixel = 1;
            }
         } else
         {
            read_next_pixel = 1;
         }
         //   OK, if I need to read a pixel, do it now
         if ( read_next_pixel )
         {
            //   load however much data we did have
            if ( tga_indexed )
            {
               // read in index, then perform the lookup
               int pal_idx = (tga_bits_per_pixel == 8) ? stbi__get8(s) : stbi__get16le(s);
               if ( pal_idx >= tga_palette_len ) {
                  // invalid index
                  pal_idx = 0;
               }
               pal_idx *= tga_comp;
               for (j = 0; j < tga_comp; ++j) {
                  raw_data[j] = tga_palette[pal_idx+j];
               }
            } else if(tga_rgb16) {
               STBI_ASSERT(tga_comp == STBI_rgb);
               stbi__tga_read_rgb16(s, raw_data);
            } else {
               //   read in the data raw
               for (j = 0; j < tga_comp; ++j) {
                  raw_data[j] = stbi__get8(s);
               }
            }
            //   clear the reading flag for the next pixel
            read_next_pixel = 0;
         } // end of reading a pixel

         // copy data
         for (j = 0; j < tga_comp; ++j)
           tga_data[i*tga_comp+j] = raw_data[j];

         //   in case we're in RLE mode, keep counting down
         --RLE_count;
      }
      //   do I need to invert the image?
      if ( tga_inverted )
      {
         for (j = 0; j*2 < tga_height; ++j)
         {
            int index1 = j * tga_width * tga_comp;
            int index2 = (tga_height - 1 - j) * tga_width * tga_comp;
            for (i = tga_width * tga_comp; i > 0; --i)
            {
               unsigned char temp = tga_data[index1];
               tga_data[index1] = tga_data[index2];
               tga_data[index2] = temp;
               ++index1;
               ++index2;
            }
         }
      }
      //   clear my palette, if I had one
      if ( tga_palette != NULL )
      {
         STBI_FREE( tga_palette );
      }
   }

   // swap RGB - if the source data was RGB16, it already is in the right order
   if (tga_comp >= 3 && !tga_rgb16)
   {
      unsigned char* tga_pixel = tga_data;
      for (i=0; i < tga_width * tga_height; ++i)
      {
         unsigned char temp = tga_pixel[0];
         tga_pixel[0] = tga_pixel[2];
         tga_pixel[2] = temp;
         tga_pixel += tga_comp;
      }
   }

   // convert to target component count
   if (req_comp && req_comp != tga_comp)
      tga_data = stbi__convert_format(tga_data, tga_comp, req_comp, tga_width, tga_height);

   //   the things I do to get rid of an error message, and yet keep
   //   Microsoft's C compilers happy... [8^(
   tga_palette_start = tga_palette_len = tga_palette_bits =
         tga_x_origin = tga_y_origin = 0;
   STBI_NOTUSED(tga_palette_start);
   //   OK, done
   return tga_data;
}
#endif

// *************************************************************************************************
// Photoshop PSD loader -- PD by Thatcher Ulrich, integration by Nicolas Schulz, tweaked by STB

#ifndef STBI_NO_PSD
static int stbi__psd_test(stbi__context *s)
{
   int r = (stbi__get32be(s) == 0x38425053);
   stbi__rewind(s);
   return r;
}

static int stbi__psd_decode_rle(stbi__context *s, stbi_uc *p, int pixelCount)
{
   int count, nleft, len;

   count = 0;
   while ((nleft = pixelCount - count) > 0) {
      len = stbi__get8(s);
      if (len == 128) {
         // No-op.
      } else if (len < 128) {
         // Copy next len+1 bytes literally.
         len++;
         if (len > nleft) return 0; // corrupt data
         count += len;
         while (len) {
            *p = stbi__get8(s);
            p += 4;
            len--;
         }
      } else if (len > 128) {
         stbi_uc   val;
         // Next -len+1 bytes in the dest are replicated from next source byte.
         // (Interpret len as a negative 8-bit int.)
         len = 257 - len;
         if (len > nleft) return 0; // corrupt data
         val = stbi__get8(s);
         count += len;
         while (len) {
            *p = val;
            p += 4;
            len--;
         }
      }
   }

   return 1;
}

static void *stbi__psd_load(stbi__context *s, int *x, int *y, int *comp, int req_comp, stbi__result_info *ri, int bpc)
{
   int pixelCount;
   int channelCount, compression;
   int channel, i;
   int bitdepth;
   int w,h;
   stbi_uc *out;
   STBI_NOTUSED(ri);

   // Check identifier
   if (stbi__get32be(s) != 0x38425053)   // "8BPS"
      return stbi__errpuc("not PSD", "Corrupt PSD image");

   // Check file type version.
   if (stbi__get16be(s) != 1)
      return stbi__errpuc("wrong version", "Unsupported version of PSD image");

   // Skip 6 reserved bytes.
   stbi__skip(s, 6 );

   // Read the number of channels (R, G, B, A, etc).
   channelCount = stbi__get16be(s);
   if (channelCount < 0 || channelCount > 16)
      return stbi__errpuc("wrong channel count", "Unsupported number of channels in PSD image");

   // Read the rows and columns of the image.
   h = stbi__get32be(s);
   w = stbi__get32be(s);

   // Make sure the depth is 8 bits.
   bitdepth = stbi__get16be(s);
   if (bitdepth != 8 && bitdepth != 16)
      return stbi__errpuc("unsupported bit depth", "PSD bit depth is not 8 or 16 bit");

   // Make sure the color mode is RGB.
   // Valid options are:
   //   0: Bitmap
   //   1: Grayscale
   //   2: Indexed color
   //   3: RGB color
   //   4: CMYK color
   //   7: Multichannel
   //   8: Duotone
   //   9: Lab color
   if (stbi__get16be(s) != 3)
      return stbi__errpuc("wrong color format", "PSD is not in RGB color format");

   // Skip the Mode Data.  (It's the palette for indexed color; other info for other modes.)
   stbi__skip(s,stbi__get32be(s) );

   // Skip the image resources.  (resolution, pen tool paths, etc)
   stbi__skip(s, stbi__get32be(s) );

   // Skip the reserved data.
   stbi__skip(s, stbi__get32be(s) );

   // Find out if the data is compressed.
   // Known values:
   //   0: no compression
   //   1: RLE compressed
   compression = stbi__get16be(s);
   if (compression > 1)
      return stbi__errpuc("bad compression", "PSD has an unknown compression format");

   // Check size
   if (!stbi__mad3sizes_valid(4, w, h, 0))
      return stbi__errpuc("too large", "Corrupt PSD");

   // Create the destination image.

   if (!compression && bitdepth == 16 && bpc == 16) {
      out = (stbi_uc *) stbi__malloc_mad3(8, w, h, 0);
      ri->bits_per_channel = 16;
   } else
      out = (stbi_uc *) stbi__malloc(4 * w*h);

   if (!out) return stbi__errpuc("outofmem", "Out of memory");
   pixelCount = w*h;

   // Initialize the data to zero.
   //memset( out, 0, pixelCount * 4 );

   // Finally, the image data.
   if (compression) {
      // RLE as used by .PSD and .TIFF
      // Loop until you get the number of unpacked bytes you are expecting:
      //     Read the next source byte into n.
      //     If n is between 0 and 127 inclusive, copy the next n+1 bytes literally.
      //     Else if n is between -127 and -1 inclusive, copy the next byte -n+1 times.
      //     Else if n is 128, noop.
      // Endloop

      // The RLE-compressed data is preceded by a 2-byte data count for each row in the data,
      // which we're going to just flags.
      stbi__skip(s, h * channelCount * 2 );

      // Read the RLE data by channel.
      for (channel = 0; channel < 4; channel++) {
         stbi_uc *p;

         p = out+channel;
         if (channel >= channelCount) {
            // Fill this channel with default data.
            for (i = 0; i < pixelCount; i++, p += 4)
               *p = (channel == 3 ? 255 : 0);
         } else {
            // Read the RLE data.
            if (!stbi__psd_decode_rle(s, p, pixelCount)) {
               STBI_FREE(out);
               return stbi__errpuc("corrupt", "bad RLE data");
            }
         }
      }

   } else {
      // We're at the raw image data.  It's each channel in order (Red, Green, Blue, Alpha, ...)
      // where each channel consists of an 8-bit (or 16-bit) value for each pixel in the image.

      // Read the data by channel.
      for (channel = 0; channel < 4; channel++) {
         if (channel >= channelCount) {
            // Fill this channel with default data.
            if (bitdepth == 16 && bpc == 16) {
               stbi__uint16 *q = ((stbi__uint16 *) out) + channel;
               stbi__uint16 val = channel == 3 ? 65535 : 0;
               for (i = 0; i < pixelCount; i++, q += 4)
                  *q = val;
            } else {
               stbi_uc *p = out+channel;
               stbi_uc val = channel == 3 ? 255 : 0;
               for (i = 0; i < pixelCount; i++, p += 4)
                  *p = val;
            }
         } else {
            if (ri->bits_per_channel == 16) {    // output bpc
               stbi__uint16 *q = ((stbi__uint16 *) out) + channel;
               for (i = 0; i < pixelCount; i++, q += 4)
                  *q = (stbi__uint16) stbi__get16be(s);
            } else {
               stbi_uc *p = out+channel;
               if (bitdepth == 16) {  // input bpc
                  for (i = 0; i < pixelCount; i++, p += 4)
                     *p = (stbi_uc) (stbi__get16be(s) >> 8);
               } else {
                  for (i = 0; i < pixelCount; i++, p += 4)
                     *p = stbi__get8(s);
               }
            }
         }
      }
   }

   // remove weird white matte from PSD
   if (channelCount >= 4) {
      if (ri->bits_per_channel == 16) {
         for (i=0; i < w*h; ++i) {
            stbi__uint16 *pixel = (stbi__uint16 *) out + 4*i;
            if (pixel[3] != 0 && pixel[3] != 65535) {
               float a = pixel[3] / 65535.0f;
               float ra = 1.0f / a;
               float inv_a = 65535.0f * (1 - ra);
               pixel[0] = (stbi__uint16) (pixel[0]*ra + inv_a);
               pixel[1] = (stbi__uint16) (pixel[1]*ra + inv_a);
               pixel[2] = (stbi__uint16) (pixel[2]*ra + inv_a);
            }
         }
      } else {
         for (i=0; i < w*h; ++i) {
            unsigned char *pixel = out + 4*i;
            if (pixel[3] != 0 && pixel[3] != 255) {
               float a = pixel[3] / 255.0f;
               float ra = 1.0f / a;
               float inv_a = 255.0f * (1 - ra);
               pixel[0] = (unsigned char) (pixel[0]*ra + inv_a);
               pixel[1] = (unsigned char) (pixel[1]*ra + inv_a);
               pixel[2] = (unsigned char) (pixel[2]*ra + inv_a);
            }
         }
      }
   }

   // convert to desired output format
   if (req_comp && req_comp != 4) {
      if (ri->bits_per_channel == 16)
         out = (stbi_uc *) stbi__convert_format16((stbi__uint16 *) out, 4, req_comp, w, h);
      else
         out = stbi__convert_format(out, 4, req_comp, w, h);
      if (out == NULL) return out; // stbi__convert_format frees input on failure
   }

   if (comp) *comp = 4;
   *y = h;
   *x = w;

   return out;
}
#endif

// *************************************************************************************************
// Softimage PIC loader
// by Tom Seddon
//
// See http://softimage.wiki.softimage.com/index.php/INFO:_PIC_file_format
// See http://ozviz.wasp.uwa.edu.au/~pbourke/dataformats/softimagepic/

#ifndef STBI_NO_PIC
static int stbi__pic_is4(stbi__context *s,const char *str)
{
   int i;
   for (i=0; i<4; ++i)
      if (stbi__get8(s) != (stbi_uc)str[i])
         return 0;

   return 1;
}

static int stbi__pic_test_core(stbi__context *s)
{
   int i;

   if (!stbi__pic_is4(s,"\x53\x80\xF6\x34"))
      return 0;

   for(i=0;i<84;++i)
      stbi__get8(s);

   if (!stbi__pic_is4(s,"PICT"))
      return 0;

   return 1;
}

typedef struct
{
   stbi_uc size,type,channel;
} stbi__pic_packet;

static stbi_uc *stbi__readval(stbi__context *s, int channel, stbi_uc *dest)
{
   int mask=0x80, i;

   for (i=0; i<4; ++i, mask>>=1) {
      if (channel & mask) {
         if (stbi__at_eof(s)) return stbi__errpuc("bad file","PIC file too short");
         dest[i]=stbi__get8(s);
      }
   }

   return dest;
}

static void stbi__copyval(int channel,stbi_uc *dest,const stbi_uc *src)
{
   int mask=0x80,i;

   for (i=0;i<4; ++i, mask>>=1)
      if (channel&mask)
         dest[i]=src[i];
}

static stbi_uc *stbi__pic_load_core(stbi__context *s,int width,int height,int *comp, stbi_uc *result)
{
   int act_comp=0,num_packets=0,y,chained;
   stbi__pic_packet packets[10];

   // this will (should...) cater for even some bizarre stuff like having data
    // for the same channel in multiple packets.
   do {
      stbi__pic_packet *packet;

      if (num_packets==sizeof(packets)/sizeof(packets[0]))
         return stbi__errpuc("bad format","too many packets");

      packet = &packets[num_packets++];

      chained = stbi__get8(s);
      packet->size    = stbi__get8(s);
      packet->type    = stbi__get8(s);
      packet->channel = stbi__get8(s);

      act_comp |= packet->channel;

      if (stbi__at_eof(s))          return stbi__errpuc("bad file","file too short (reading packets)");
      if (packet->size != 8)  return stbi__errpuc("bad format","packet isn't 8bpp");
   } while (chained);

   *comp = (act_comp & 0x10 ? 4 : 3); // has alpha channel?

   for(y=0; y<height; ++y) {
      int packet_idx;

      for(packet_idx=0; packet_idx < num_packets; ++packet_idx) {
         stbi__pic_packet *packet = &packets[packet_idx];
         stbi_uc *dest = result+y*width*4;

         switch (packet->type) {
            default:
               return stbi__errpuc("bad format","packet has bad compression type");

            case 0: {//uncompressed
               int x;

               for(x=0;x<width;++x, dest+=4)
                  if (!stbi__readval(s,packet->channel,dest))
                     return 0;
               break;
            }

            case 1://Pure RLE
               {
                  int left=width, i;

                  while (left>0) {
                     stbi_uc count,value[4];

                     count=stbi__get8(s);
                     if (stbi__at_eof(s))   return stbi__errpuc("bad file","file too short (pure read count)");

                     if (count > left)
                        count = (stbi_uc) left;

                     if (!stbi__readval(s,packet->channel,value))  return 0;

                     for(i=0; i<count; ++i,dest+=4)
                        stbi__copyval(packet->channel,dest,value);
                     left -= count;
                  }
               }
               break;

            case 2: {//Mixed RLE
               int left=width;
               while (left>0) {
                  int count = stbi__get8(s), i;
                  if (stbi__at_eof(s))  return stbi__errpuc("bad file","file too short (mixed read count)");

                  if (count >= 128) { // Repeated
                     stbi_uc value[4];

                     if (count==128)
                        count = stbi__get16be(s);
                     else
                        count -= 127;
                     if (count > left)
                        return stbi__errpuc("bad file","scanline overrun");

                     if (!stbi__readval(s,packet->channel,value))
                        return 0;

                     for(i=0;i<count;++i, dest += 4)
                        stbi__copyval(packet->channel,dest,value);
                  } else { // Raw
                     ++count;
                     if (count>left) return stbi__errpuc("bad file","scanline overrun");

                     for(i=0;i<count;++i, dest+=4)
                        if (!stbi__readval(s,packet->channel,dest))
                           return 0;
                  }
                  left-=count;
               }
               break;
            }
         }
      }
   }

   return result;
}

static void *stbi__pic_load(stbi__context *s,int *px,int *py,int *comp,int req_comp, stbi__result_info *ri)
{
   stbi_uc *result;
   int i, x,y, internal_comp;
   STBI_NOTUSED(ri);

   if (!comp) comp = &internal_comp;

   for (i=0; i<92; ++i)
      stbi__get8(s);

   x = stbi__get16be(s);
   y = stbi__get16be(s);
   if (stbi__at_eof(s))  return stbi__errpuc("bad file","file too short (pic header)");
   if (!stbi__mad3sizes_valid(x, y, 4, 0)) return stbi__errpuc("too large", "PIC image too large to decode");

   stbi__get32be(s); //flags `ratio'
   stbi__get16be(s); //flags `fields'
   stbi__get16be(s); //flags `pad'

   // intermediate buffer is RGBA
   result = (stbi_uc *) stbi__malloc_mad3(x, y, 4, 0);
   memset(result, 0xff, x*y*4);

   if (!stbi__pic_load_core(s,x,y,comp, result)) {
      STBI_FREE(result);
      result=0;
   }
   *px = x;
   *py = y;
   if (req_comp == 0) req_comp = *comp;
   result=stbi__convert_format(result,4,req_comp,x,y);

   return result;
}

static int stbi__pic_test(stbi__context *s)
{
   int r = stbi__pic_test_core(s);
   stbi__rewind(s);
   return r;
}
#endif

// *************************************************************************************************
// GIF loader -- public domain by Jean-Marc Lienher -- simplified/shrunk by stb

#ifndef STBI_NO_GIF
typedef struct
{
   stbi__int16 prefix;
   stbi_uc first;
   stbi_uc suffix;
} stbi__gif_lzw;

typedef struct
{
   int w,h;
   stbi_uc *out;                 // output buffer (always 4 components)
   stbi_uc *background;          // The current "background" as far as a gif is concerned
   stbi_uc *history;
   int flags, bgindex, ratio, transparent, eflags;
   stbi_uc  pal[256][4];
   stbi_uc lpal[256][4];
   stbi__gif_lzw codes[8192];
   stbi_uc *color_table;
   int parse, step;
   int lflags;
   int start_x, start_y;
   int max_x, max_y;
   int cur_x, cur_y;
   int line_size;
   int delay;
} stbi__gif;

static int stbi__gif_test_raw(stbi__context *s)
{
   int sz;
   if (stbi__get8(s) != 'G' || stbi__get8(s) != 'I' || stbi__get8(s) != 'F' || stbi__get8(s) != '8') return 0;
   sz = stbi__get8(s);
   if (sz != '9' && sz != '7') return 0;
   if (stbi__get8(s) != 'a') return 0;
   return 1;
}

static int stbi__gif_test(stbi__context *s)
{
   int r = stbi__gif_test_raw(s);
   stbi__rewind(s);
   return r;
}

static void stbi__gif_parse_colortable(stbi__context *s, stbi_uc pal[256][4], int num_entries, int transp)
{
   int i;
   for (i=0; i < num_entries; ++i) {
      pal[i][2] = stbi__get8(s);
      pal[i][1] = stbi__get8(s);
      pal[i][0] = stbi__get8(s);
      pal[i][3] = transp == i ? 0 : 255;
   }
}

static int stbi__gif_header(stbi__context *s, stbi__gif *g, int *comp, int is_info)
{
   stbi_uc version;
   if (stbi__get8(s) != 'G' || stbi__get8(s) != 'I' || stbi__get8(s) != 'F' || stbi__get8(s) != '8')
      return stbi__err("not GIF", "Corrupt GIF");

   version = stbi__get8(s);
   if (version != '7' && version != '9')    return stbi__err("not GIF", "Corrupt GIF");
   if (stbi__get8(s) != 'a')                return stbi__err("not GIF", "Corrupt GIF");

   stbi__g_failure_reason = "";
   g->w = stbi__get16le(s);
   g->h = stbi__get16le(s);
   g->flags = stbi__get8(s);
   g->bgindex = stbi__get8(s);
   g->ratio = stbi__get8(s);
   g->transparent = -1;

   if (comp != 0) *comp = 4;  // can't actually tell whether it's 3 or 4 until we parse the comments

   if (is_info) return 1;

   if (g->flags & 0x80)
      stbi__gif_parse_colortable(s,g->pal, 2 << (g->flags & 7), -1);

   return 1;
}

static int stbi__gif_info_raw(stbi__context *s, int *x, int *y, int *comp)
{
   stbi__gif* g = (stbi__gif*) stbi__malloc(sizeof(stbi__gif));
   if (!stbi__gif_header(s, g, comp, 1)) {
      STBI_FREE(g);
      stbi__rewind( s );
      return 0;
   }
   if (x) *x = g->w;
   if (y) *y = g->h;
   STBI_FREE(g);
   return 1;
}

static void stbi__out_gif_code(stbi__gif *g, stbi__uint16 code)
{
   stbi_uc *p, *c;
   int idx;

   // recurse to decode the prefixes, since the linked-list is backwards,
   // and working backwards through an interleaved image would be nasty
   if (g->codes[code].prefix >= 0)
      stbi__out_gif_code(g, g->codes[code].prefix);

   if (g->cur_y >= g->max_y) return;

   idx = g->cur_x + g->cur_y;
   p = &g->out[idx];
   g->history[idx / 4] = 1;

   c = &g->color_table[g->codes[code].suffix * 4];
   if (c[3] > 128) { // don't render transparent pixels;
      p[0] = c[2];
      p[1] = c[1];
      p[2] = c[0];
      p[3] = c[3];
   }
   g->cur_x += 4;

   if (g->cur_x >= g->max_x) {
      g->cur_x = g->start_x;
      g->cur_y += g->step;

      while (g->cur_y >= g->max_y && g->parse > 0) {
         g->step = (1 << g->parse) * g->line_size;
         g->cur_y = g->start_y + (g->step >> 1);
         --g->parse;
      }
   }
}

static stbi_uc *stbi__process_gif_raster(stbi__context *s, stbi__gif *g)
{
   stbi_uc lzw_cs;
   stbi__int32 len, init_code;
   stbi__uint32 first;
   stbi__int32 codesize, codemask, avail, oldcode, bits, valid_bits, clear;
   stbi__gif_lzw *p;

   lzw_cs = stbi__get8(s);
   if (lzw_cs > 12) return NULL;
   clear = 1 << lzw_cs;
   first = 1;
   codesize = lzw_cs + 1;
   codemask = (1 << codesize) - 1;
   bits = 0;
   valid_bits = 0;
   for (init_code = 0; init_code < clear; init_code++) {
      g->codes[init_code].prefix = -1;
      g->codes[init_code].first = (stbi_uc) init_code;
      g->codes[init_code].suffix = (stbi_uc) init_code;
   }

   // support no starting clear code
   avail = clear+2;
   oldcode = -1;

   len = 0;
   for(;;) {
      if (valid_bits < codesize) {
         if (len == 0) {
            len = stbi__get8(s); // start new block
            if (len == 0)
               return g->out;
         }
         --len;
         bits |= (stbi__int32) stbi__get8(s) << valid_bits;
         valid_bits += 8;
      } else {
         stbi__int32 code = bits & codemask;
         bits >>= codesize;
         valid_bits -= codesize;
         // @OPTIMIZE: is there some way we can accelerate the non-clear path?
         if (code == clear) {  // clear code
            codesize = lzw_cs + 1;
            codemask = (1 << codesize) - 1;
            avail = clear + 2;
            oldcode = -1;
            first = 0;
         } else if (code == clear + 1) { // end of stream code
            stbi__skip(s, len);
            while ((len = stbi__get8(s)) > 0)
               stbi__skip(s,len);
            return g->out;
         } else if (code <= avail) {
            if (first) {
               return stbi__errpuc("no clear code", "Corrupt GIF");
            }

            if (oldcode >= 0) {
               p = &g->codes[avail++];
               if (avail > 8192) {
                  return stbi__errpuc("too many codes", "Corrupt GIF");
               }

               p->prefix = (stbi__int16) oldcode;
               p->first = g->codes[oldcode].first;
               p->suffix = (code == avail) ? p->first : g->codes[code].first;
            } else if (code == avail)
               return stbi__errpuc("illegal code in raster", "Corrupt GIF");

            stbi__out_gif_code(g, (stbi__uint16) code);

            if ((avail & codemask) == 0 && avail <= 0x0FFF) {
               codesize++;
               codemask = (1 << codesize) - 1;
            }

            oldcode = code;
         } else {
            return stbi__errpuc("illegal code in raster", "Corrupt GIF");
         }
      }
   }
}

// this function is designed to support animated gifs, although stb_image doesn't support it
// two back is the image from two frames ago, used for a very specific disposal format
static stbi_uc *stbi__gif_load_next(stbi__context *s, stbi__gif *g, int *comp, int req_comp, stbi_uc *two_back)
{
   int dispose;
   int first_frame;
   int pi;
   int pcount;
   STBI_NOTUSED(req_comp);

   // on first frame, any non-written pixels get the background colour (non-transparent)
   first_frame = 0;
   if (g->out == 0) {
      if (!stbi__gif_header(s, g, comp,0)) return 0; // stbi__g_failure_reason set by stbi__gif_header
      if (!stbi__mad3sizes_valid(4, g->w, g->h, 0))
         return stbi__errpuc("too large", "GIF image is too large");
      pcount = g->w * g->h;
      g->out = (stbi_uc *) stbi__malloc(4 * pcount);
      g->background = (stbi_uc *) stbi__malloc(4 * pcount);
      g->history = (stbi_uc *) stbi__malloc(pcount);
      if (!g->out || !g->background || !g->history)
         return stbi__errpuc("outofmem", "Out of memory");

      // image is treated as "transparent" at the start - ie, nothing overwrites the current background;
      // background colour is only used for pixels that are not rendered first frame, after that "background"
      // color refers to the color that was there the previous frame.
      memset(g->out, 0x00, 4 * pcount);
      memset(g->background, 0x00, 4 * pcount); // state of the background (starts transparent)
      memset(g->history, 0x00, pcount);        // pixels that were affected previous frame
      first_frame = 1;
   } else {
      // second frame - how do we dispoase of the previous one?
      dispose = (g->eflags & 0x1C) >> 2;
      pcount = g->w * g->h;

      if ((dispose == 3) && (two_back == 0)) {
         dispose = 2; // if I don't have an image to revert back to, default to the old background
      }

      if (dispose == 3) { // use previous graphic
         for (pi = 0; pi < pcount; ++pi) {
            if (g->history[pi]) {
               memcpy( &g->out[pi * 4], &two_back[pi * 4], 4 );
            }
         }
      } else if (dispose == 2) {
         // restore what was changed last frame to background before that frame;
         for (pi = 0; pi < pcount; ++pi) {
            if (g->history[pi]) {
               memcpy( &g->out[pi * 4], &g->background[pi * 4], 4 );
            }
         }
      } else {
         // This is a non-disposal case eithe way, so just
         // leave the pixels as is, and they will become the new background
         // 1: do not dispose
         // 0:  not specified.
      }

      // background is what out is after the undoing of the previou frame;
      memcpy( g->background, g->out, 4 * g->w * g->h );
   }

   // clear my history;
   memset( g->history, 0x00, g->w * g->h );        // pixels that were affected previous frame

   for (;;) {
      int tag = stbi__get8(s);
      switch (tag) {
         case 0x2C: /* Image Descriptor */
         {
            stbi__int32 x, y, w, h;
            stbi_uc *o;

            x = stbi__get16le(s);
            y = stbi__get16le(s);
            w = stbi__get16le(s);
            h = stbi__get16le(s);
            if (((x + w) > (g->w)) || ((y + h) > (g->h)))
               return stbi__errpuc("bad Image Descriptor", "Corrupt GIF");

            g->line_size = g->w * 4;
            g->start_x = x * 4;
            g->start_y = y * g->line_size;
            g->max_x   = g->start_x + w * 4;
            g->max_y   = g->start_y + h * g->line_size;
            g->cur_x   = g->start_x;
            g->cur_y   = g->start_y;

            // if the width of the specified rectangle is 0, that means
            // we may not see *any* pixels or the image is malformed;
            // to make sure this is caught, move the current y down to
            // max_y (which is what out_gif_code checks).
            if (w == 0)
               g->cur_y = g->max_y;

            g->lflags = stbi__get8(s);

            if (g->lflags & 0x40) {
               g->step = 8 * g->line_size; // first interlaced spacing
               g->parse = 3;
            } else {
               g->step = g->line_size;
               g->parse = 0;
            }

            if (g->lflags & 0x80) {
               stbi__gif_parse_colortable(s,g->lpal, 2 << (g->lflags & 7), g->eflags & 0x01 ? g->transparent : -1);
               g->color_table = (stbi_uc *) g->lpal;
            } else if (g->flags & 0x80) {
               g->color_table = (stbi_uc *) g->pal;
            } else
               return stbi__errpuc("missing color table", "Corrupt GIF");

            o = stbi__process_gif_raster(s, g);
            if (!o) return NULL;

            // if this was the first frame,
            pcount = g->w * g->h;
            if (first_frame && (g->bgindex > 0)) {
               // if first frame, any pixel not drawn to gets the background color
               for (pi = 0; pi < pcount; ++pi) {
                  if (g->history[pi] == 0) {
                     g->pal[g->bgindex][3] = 255; // just in case it was made transparent, undo that; It will be reset next frame if need be;
                     memcpy( &g->out[pi * 4], &g->pal[g->bgindex], 4 );
                  }
               }
            }

            return o;
         }

         case 0x21: // Comment Extension.
         {
            int len;
            int ext = stbi__get8(s);
            if (ext == 0xF9) { // Graphic Control Extension.
               len = stbi__get8(s);
               if (len == 4) {
                  g->eflags = stbi__get8(s);
                  g->delay = 10 * stbi__get16le(s); // delay - 1/100th of a second, saving as 1/1000ths.

                  // unset old transparent
                  if (g->transparent >= 0) {
                     g->pal[g->transparent][3] = 255;
                  }
                  if (g->eflags & 0x01) {
                     g->transparent = stbi__get8(s);
                     if (g->transparent >= 0) {
                        g->pal[g->transparent][3] = 0;
                     }
                  } else {
                     // don't need transparent
                     stbi__skip(s, 1);
                     g->transparent = -1;
                  }
               } else {
                  stbi__skip(s, len);
                  break;
               }
            }
            while ((len = stbi__get8(s)) != 0) {
               stbi__skip(s, len);
            }
            break;
         }

         case 0x3B: // gif stream termination code
            return (stbi_uc *) s; // using '1' causes warning on some compilers

         default:
            return stbi__errpuc("unknown code", "Corrupt GIF");
      }
   }
}

static void *stbi__load_gif_main(stbi__context *s, int **delays, int *x, int *y, int *z, int *comp, int req_comp)
{
   if (stbi__gif_test(s)) {
      int layers = 0;
      stbi_uc *u = 0;
      stbi_uc *out = 0;
      stbi_uc *two_back = 0;
      stbi__gif g;
      int stride;
      memset(&g, 0, sizeof(g));
      if (delays) {
         *delays = 0;
      }

      do {
         u = stbi__gif_load_next(s, &g, comp, req_comp, two_back);
         if (u == (stbi_uc *) s) u = 0;  // end of animated gif marker

         if (u) {
            *x = g.w;
            *y = g.h;
            ++layers;
            stride = g.w * g.h * 4;

            if (out) {
               void *tmp = (stbi_uc*) STBI_REALLOC( out, layers * stride );
               if (NULL == tmp) {
                  STBI_FREE(g.out);
                  STBI_FREE(g.history);
                  STBI_FREE(g.background);
                  return stbi__errpuc("outofmem", "Out of memory");
               }
               else
                  out = (stbi_uc*) tmp;
               if (delays) {
                  *delays = (int*) STBI_REALLOC( *delays, sizeof(int) * layers );
               }
            } else {
               out = (stbi_uc*)stbi__malloc( layers * stride );
               if (delays) {
                  *delays = (int*) stbi__malloc( layers * sizeof(int) );
               }
            }
            memcpy( out + ((layers - 1) * stride), u, stride );
            if (layers >= 2) {
               two_back = out - 2 * stride;
            }

            if (delays) {
               (*delays)[layers - 1U] = g.delay;
            }
         }
      } while (u != 0);

      // free temp buffer;
      STBI_FREE(g.out);
      STBI_FREE(g.history);
      STBI_FREE(g.background);

      // do the final conversion after loading everything;
      if (req_comp && req_comp != 4)
         out = stbi__convert_format(out, 4, req_comp, layers * g.w, g.h);

      *z = layers;
      return out;
   } else {
      return stbi__errpuc("not GIF", "Image was not as a gif type.");
   }
}

static void *stbi__gif_load(stbi__context *s, int *x, int *y, int *comp, int req_comp, stbi__result_info *ri)
{
   stbi_uc *u = 0;
   stbi__gif g;
   memset(&g, 0, sizeof(g));
   STBI_NOTUSED(ri);

   u = stbi__gif_load_next(s, &g, comp, req_comp, 0);
   if (u == (stbi_uc *) s) u = 0;  // end of animated gif marker
   if (u) {
      *x = g.w;
      *y = g.h;

      // moved conversion to after successful load so that the same
      // can be done for multiple frames.
      if (req_comp && req_comp != 4)
         u = stbi__convert_format(u, 4, req_comp, g.w, g.h);
   } else if (g.out) {
      // if there was an error and we allocated an image buffer, free it!
      STBI_FREE(g.out);
   }

   // free buffers needed for multiple frame loading;
   STBI_FREE(g.history);
   STBI_FREE(g.background);

   return u;
}

static int stbi__gif_info(stbi__context *s, int *x, int *y, int *comp)
{
   return stbi__gif_info_raw(s,x,y,comp);
}
#endif

// *************************************************************************************************
// Radiance RGBE HDR loader
// originally by Nicolas Schulz
#ifndef STBI_NO_HDR
static int stbi__hdr_test_core(stbi__context *s, const char *signature)
{
   int i;
   for (i=0; signature[i]; ++i)
      if (stbi__get8(s) != signature[i])
          return 0;
   stbi__rewind(s);
   return 1;
}

static int stbi__hdr_test(stbi__context* s)
{
   int r = stbi__hdr_test_core(s, "#?RADIANCE\n");
   stbi__rewind(s);
   if(!r) {
       r = stbi__hdr_test_core(s, "#?RGBE\n");
       stbi__rewind(s);
   }
   return r;
}

#define STBI__HDR_BUFLEN  1024
static char *stbi__hdr_gettoken(stbi__context *z, char *buffer)
{
   int len=0;
   char c = '\0';

   c = (char) stbi__get8(z);

   while (!stbi__at_eof(z) && c != '\n') {
      buffer[len++] = c;
      if (len == STBI__HDR_BUFLEN-1) {
         // flush to end of line
         while (!stbi__at_eof(z) && stbi__get8(z) != '\n')
            ;
         break;
      }
      c = (char) stbi__get8(z);
   }

   buffer[len] = 0;
   return buffer;
}

static void stbi__hdr_convert(float *output, stbi_uc *input, int req_comp)
{
   if ( input[3] != 0 ) {
      float f1;
      // Exponent
      f1 = (float) ldexp(1.0f, input[3] - (int)(128 + 8));
      if (req_comp <= 2)
         output[0] = (input[0] + input[1] + input[2]) * f1 / 3;
      else {
         output[0] = input[0] * f1;
         output[1] = input[1] * f1;
         output[2] = input[2] * f1;
      }
      if (req_comp == 2) output[1] = 1;
      if (req_comp == 4) output[3] = 1;
   } else {
      switch (req_comp) {
         case 4: output[3] = 1; /* fallthrough */
         case 3: output[0] = output[1] = output[2] = 0;
                 break;
         case 2: output[1] = 1; /* fallthrough */
         case 1: output[0] = 0;
                 break;
      }
   }
}

static float *stbi__hdr_load(stbi__context *s, int *x, int *y, int *comp, int req_comp, stbi__result_info *ri)
{
   char buffer[STBI__HDR_BUFLEN];
   char *token;
   int valid = 0;
   int width, height;
   stbi_uc *scanline;
   float *hdr_data;
   int len;
   unsigned char count, value;
   int i, j, k, c1,c2, z;
   const char *headerToken;
   STBI_NOTUSED(ri);

   // Check identifier
   headerToken = stbi__hdr_gettoken(s,buffer);
   if (strcmp(headerToken, "#?RADIANCE") != 0 && strcmp(headerToken, "#?RGBE") != 0)
      return stbi__errpf("not HDR", "Corrupt HDR image");

   // Parse header
   for(;;) {
      token = stbi__hdr_gettoken(s,buffer);
      if (token[0] == 0) break;
      if (strcmp(token, "FORMAT=32-bit_rle_rgbe") == 0) valid = 1;
   }

   if (!valid)    return stbi__errpf("unsupported format", "Unsupported HDR format");

   // Parse width and height
   // can't use sscanf() if we're not using stdio!
   token = stbi__hdr_gettoken(s,buffer);
   if (strncmp(token, "-Y ", 3))  return stbi__errpf("unsupported data layout", "Unsupported HDR format");
   token += 3;
   height = (int) strtol(token, &token, 10);
   while (*token == ' ') ++token;
   if (strncmp(token, "+X ", 3))  return stbi__errpf("unsupported data layout", "Unsupported HDR format");
   token += 3;
   width = (int) strtol(token, NULL, 10);

   *x = width;
   *y = height;

   if (comp) *comp = 3;
   if (req_comp == 0) req_comp = 3;

   if (!stbi__mad4sizes_valid(width, height, req_comp, sizeof(float), 0))
      return stbi__errpf("too large", "HDR image is too large");

   // Read data
   hdr_data = (float *) stbi__malloc_mad4(width, height, req_comp, sizeof(float), 0);
   if (!hdr_data)
      return stbi__errpf("outofmem", "Out of memory");

   // Load image data
   // image data is stored as some number of sca
   if ( width < 8 || width >= 32768) {
      // Read flat data
      for (j=0; j < height; ++j) {
         for (i=0; i < width; ++i) {
            stbi_uc rgbe[4];
           main_decode_loop:
            stbi__getn(s, rgbe, 4);
            stbi__hdr_convert(hdr_data + j * width * req_comp + i * req_comp, rgbe, req_comp);
         }
      }
   } else {
      // Read RLE-encoded data
      scanline = NULL;

      for (j = 0; j < height; ++j) {
         c1 = stbi__get8(s);
         c2 = stbi__get8(s);
         len = stbi__get8(s);
         if (c1 != 2 || c2 != 2 || (len & 0x80)) {
            // not run-length encoded, so we have to actually use THIS data as a decoded
            // pixel (note this can't be a valid pixel--one of RGB must be >= 128)
            stbi_uc rgbe[4];
            rgbe[0] = (stbi_uc) c1;
            rgbe[1] = (stbi_uc) c2;
            rgbe[2] = (stbi_uc) len;
            rgbe[3] = (stbi_uc) stbi__get8(s);
            stbi__hdr_convert(hdr_data, rgbe, req_comp);
            i = 1;
            j = 0;
            STBI_FREE(scanline);
            goto main_decode_loop; // yes, this makes no sense
         }
         len <<= 8;
         len |= stbi__get8(s);
         if (len != width) { STBI_FREE(hdr_data); STBI_FREE(scanline); return stbi__errpf("invalid decoded scanline length", "corrupt HDR"); }
         if (scanline == NULL) {
            scanline = (stbi_uc *) stbi__malloc_mad2(width, 4, 0);
            if (!scanline) {
               STBI_FREE(hdr_data);
               return stbi__errpf("outofmem", "Out of memory");
            }
         }

         for (k = 0; k < 4; ++k) {
            int nleft;
            i = 0;
            while ((nleft = width - i) > 0) {
               count = stbi__get8(s);
               if (count > 128) {
                  // Run
                  value = stbi__get8(s);
                  count -= 128;
                  if (count > nleft) { STBI_FREE(hdr_data); STBI_FREE(scanline); return stbi__errpf("corrupt", "bad RLE data in HDR"); }
                  for (z = 0; z < count; ++z)
                     scanline[i++ * 4 + k] = value;
               } else {
                  // Dump
                  if (count > nleft) { STBI_FREE(hdr_data); STBI_FREE(scanline); return stbi__errpf("corrupt", "bad RLE data in HDR"); }
                  for (z = 0; z < count; ++z)
                     scanline[i++ * 4 + k] = stbi__get8(s);
               }
            }
         }
         for (i=0; i < width; ++i)
            stbi__hdr_convert(hdr_data+(j*width + i)*req_comp, scanline + i*4, req_comp);
      }
      if (scanline)
         STBI_FREE(scanline);
   }

   return hdr_data;
}

static int stbi__hdr_info(stbi__context *s, int *x, int *y, int *comp)
{
   char buffer[STBI__HDR_BUFLEN];
   char *token;
   int valid = 0;
   int dummy;

   if (!x) x = &dummy;
   if (!y) y = &dummy;
   if (!comp) comp = &dummy;

   if (stbi__hdr_test(s) == 0) {
       stbi__rewind( s );
       return 0;
   }

   for(;;) {
      token = stbi__hdr_gettoken(s,buffer);
      if (token[0] == 0) break;
      if (strcmp(token, "FORMAT=32-bit_rle_rgbe") == 0) valid = 1;
   }

   if (!valid) {
       stbi__rewind( s );
       return 0;
   }
   token = stbi__hdr_gettoken(s,buffer);
   if (strncmp(token, "-Y ", 3)) {
       stbi__rewind( s );
       return 0;
   }
   token += 3;
   *y = (int) strtol(token, &token, 10);
   while (*token == ' ') ++token;
   if (strncmp(token, "+X ", 3)) {
       stbi__rewind( s );
       return 0;
   }
   token += 3;
   *x = (int) strtol(token, NULL, 10);
   *comp = 3;
   return 1;
}
#endif // STBI_NO_HDR

#ifndef STBI_NO_BMP
static int stbi__bmp_info(stbi__context *s, int *x, int *y, int *comp)
{
   void *p;
   stbi__bmp_data info;

   info.all_a = 255;
   p = stbi__bmp_parse_header(s, &info);
   stbi__rewind( s );
   if (p == NULL)
      return 0;
   if (x) *x = s->img_x;
   if (y) *y = s->img_y;
   if (comp) {
      if (info.bpp == 24 && info.ma == 0xff000000)
         *comp = 3;
      else
         *comp = info.ma ? 4 : 3;
   }
   return 1;
}
#endif

#ifndef STBI_NO_PSD
static int stbi__psd_info(stbi__context *s, int *x, int *y, int *comp)
{
   int channelCount, dummy, depth;
   if (!x) x = &dummy;
   if (!y) y = &dummy;
   if (!comp) comp = &dummy;
   if (stbi__get32be(s) != 0x38425053) {
       stbi__rewind( s );
       return 0;
   }
   if (stbi__get16be(s) != 1) {
       stbi__rewind( s );
       return 0;
   }
   stbi__skip(s, 6);
   channelCount = stbi__get16be(s);
   if (channelCount < 0 || channelCount > 16) {
       stbi__rewind( s );
       return 0;
   }
   *y = stbi__get32be(s);
   *x = stbi__get32be(s);
   depth = stbi__get16be(s);
   if (depth != 8 && depth != 16) {
       stbi__rewind( s );
       return 0;
   }
   if (stbi__get16be(s) != 3) {
       stbi__rewind( s );
       return 0;
   }
   *comp = 4;
   return 1;
}

static int stbi__psd_is16(stbi__context *s)
{
   int channelCount, depth;
   if (stbi__get32be(s) != 0x38425053) {
       stbi__rewind( s );
       return 0;
   }
   if (stbi__get16be(s) != 1) {
       stbi__rewind( s );
       return 0;
   }
   stbi__skip(s, 6);
   channelCount = stbi__get16be(s);
   if (channelCount < 0 || channelCount > 16) {
       stbi__rewind( s );
       return 0;
   }
   (void) stbi__get32be(s);
   (void) stbi__get32be(s);
   depth = stbi__get16be(s);
   if (depth != 16) {
       stbi__rewind( s );
       return 0;
   }
   return 1;
}
#endif

#ifndef STBI_NO_PIC
static int stbi__pic_info(stbi__context *s, int *x, int *y, int *comp)
{
   int act_comp=0,num_packets=0,chained,dummy;
   stbi__pic_packet packets[10];

   if (!x) x = &dummy;
   if (!y) y = &dummy;
   if (!comp) comp = &dummy;

   if (!stbi__pic_is4(s,"\x53\x80\xF6\x34")) {
      stbi__rewind(s);
      return 0;
   }

   stbi__skip(s, 88);

   *x = stbi__get16be(s);
   *y = stbi__get16be(s);
   if (stbi__at_eof(s)) {
      stbi__rewind( s);
      return 0;
   }
   if ( (*x) != 0 && (1 << 28) / (*x) < (*y)) {
      stbi__rewind( s );
      return 0;
   }

   stbi__skip(s, 8);

   do {
      stbi__pic_packet *packet;

      if (num_packets==sizeof(packets)/sizeof(packets[0]))
         return 0;

      packet = &packets[num_packets++];
      chained = stbi__get8(s);
      packet->size    = stbi__get8(s);
      packet->type    = stbi__get8(s);
      packet->channel = stbi__get8(s);
      act_comp |= packet->channel;

      if (stbi__at_eof(s)) {
          stbi__rewind( s );
          return 0;
      }
      if (packet->size != 8) {
          stbi__rewind( s );
          return 0;
      }
   } while (chained);

   *comp = (act_comp & 0x10 ? 4 : 3);

   return 1;
}
#endif

// *************************************************************************************************
// Portable Gray Map and Portable Pixel Map loader
// by Ken Miller
//
// PGM: http://netpbm.sourceforge.net/doc/pgm.html
// PPM: http://netpbm.sourceforge.net/doc/ppm.html
//
// Known limitations:
//    Does not support comments in the header section
//    Does not support ASCII image data (formats P2 and P3)
//    Does not support 16-bit-per-channel

#ifndef STBI_NO_PNM

static int      stbi__pnm_test(stbi__context *s)
{
   char p, t;
   p = (char) stbi__get8(s);
   t = (char) stbi__get8(s);
   if (p != 'P' || (t != '5' && t != '6')) {
       stbi__rewind( s );
       return 0;
   }
   return 1;
}

static void *stbi__pnm_load(stbi__context *s, int *x, int *y, int *comp, int req_comp, stbi__result_info *ri)
{
   stbi_uc *out;
   STBI_NOTUSED(ri);

   if (!stbi__pnm_info(s, (int *)&s->img_x, (int *)&s->img_y, (int *)&s->img_n))
      return 0;

   *x = s->img_x;
   *y = s->img_y;
   if (comp) *comp = s->img_n;

   if (!stbi__mad3sizes_valid(s->img_n, s->img_x, s->img_y, 0))
      return stbi__errpuc("too large", "PNM too large");

   out = (stbi_uc *) stbi__malloc_mad3(s->img_n, s->img_x, s->img_y, 0);
   if (!out) return stbi__errpuc("outofmem", "Out of memory");
   stbi__getn(s, out, s->img_n * s->img_x * s->img_y);

   if (req_comp && req_comp != s->img_n) {
      out = stbi__convert_format(out, s->img_n, req_comp, s->img_x, s->img_y);
      if (out == NULL) return out; // stbi__convert_format frees input on failure
   }
   return out;
}

static int      stbi__pnm_isspace(char c)
{
   return c == ' ' || c == '\t' || c == '\n' || c == '\v' || c == '\f' || c == '\r';
}

static void     stbi__pnm_skip_whitespace(stbi__context *s, char *c)
{
   for (;;) {
      while (!stbi__at_eof(s) && stbi__pnm_isspace(*c))
         *c = (char) stbi__get8(s);

      if (stbi__at_eof(s) || *c != '#')
         break;

      while (!stbi__at_eof(s) && *c != '\n' && *c != '\r' )
         *c = (char) stbi__get8(s);
   }
}

static int      stbi__pnm_isdigit(char c)
{
   return c >= '0' && c <= '9';
}

static int      stbi__pnm_getinteger(stbi__context *s, char *c)
{
   int value = 0;

   while (!stbi__at_eof(s) && stbi__pnm_isdigit(*c)) {
      value = value*10 + (*c - '0');
      *c = (char) stbi__get8(s);
   }

   return value;
}

static int      stbi__pnm_info(stbi__context *s, int *x, int *y, int *comp)
{
   int maxv, dummy;
   char c, p, t;

   if (!x) x = &dummy;
   if (!y) y = &dummy;
   if (!comp) comp = &dummy;

   stbi__rewind(s);

   // Get identifier
   p = (char) stbi__get8(s);
   t = (char) stbi__get8(s);
   if (p != 'P' || (t != '5' && t != '6')) {
       stbi__rewind(s);
       return 0;
   }

   *comp = (t == '6') ? 3 : 1;  // '5' is 1-component .pgm; '6' is 3-component .ppm

   c = (char) stbi__get8(s);
   stbi__pnm_skip_whitespace(s, &c);

   *x = stbi__pnm_getinteger(s, &c); // read width
   stbi__pnm_skip_whitespace(s, &c);

   *y = stbi__pnm_getinteger(s, &c); // read height
   stbi__pnm_skip_whitespace(s, &c);

   maxv = stbi__pnm_getinteger(s, &c);  // read max value

   if (maxv > 255)
      return stbi__err("max value > 255", "PPM image not 8-bit");
   else
      return 1;
}
#endif

static int stbi__info_main(stbi__context *s, int *x, int *y, int *comp)
{
   #ifndef STBI_NO_JPEG
   if (stbi__jpeg_info(s, x, y, comp)) return 1;
   #endif

   #ifndef STBI_NO_PNG
   if (stbi__png_info(s, x, y, comp))  return 1;
   #endif

   #ifndef STBI_NO_GIF
   if (stbi__gif_info(s, x, y, comp))  return 1;
   #endif

   #ifndef STBI_NO_BMP
   if (stbi__bmp_info(s, x, y, comp))  return 1;
   #endif

   #ifndef STBI_NO_PSD
   if (stbi__psd_info(s, x, y, comp))  return 1;
   #endif

   #ifndef STBI_NO_PIC
   if (stbi__pic_info(s, x, y, comp))  return 1;
   #endif

   #ifndef STBI_NO_PNM
   if (stbi__pnm_info(s, x, y, comp))  return 1;
   #endif

   #ifndef STBI_NO_HDR
   if (stbi__hdr_info(s, x, y, comp))  return 1;
   #endif

   // test tga last because it's a crappy test!
   #ifndef STBI_NO_TGA
   if (stbi__tga_info(s, x, y, comp))
       return 1;
   #endif
   return stbi__err("unknown image type", "Image not of any known type, or corrupt");
}

static int stbi__is_16_main(stbi__context *s)
{
   #ifndef STBI_NO_PNG
   if (stbi__png_is16(s))  return 1;
   #endif

   #ifndef STBI_NO_PSD
   if (stbi__psd_is16(s))  return 1;
   #endif

   return 0;
}

#ifndef STBI_NO_STDIO
STBIDEF int stbi_info(char const *filename, int *x, int *y, int *comp)
{
    FILE *f = stbi__fopen(filename, "rb");
    int result;
    if (!f) return stbi__err("can't fopen", "Unable to open file");
    result = stbi_info_from_file(f, x, y, comp);
    fclose(f);
    return result;
}

STBIDEF int stbi_info_from_file(FILE *f, int *x, int *y, int *comp)
{
   int r;
   stbi__context s;
   long pos = ftell(f);
   stbi__start_file(&s, f);
   r = stbi__info_main(&s,x,y,comp);
   fseek(f,pos,SEEK_SET);
   return r;
}

STBIDEF int stbi_is_16_bit(char const *filename)
{
    FILE *f = stbi__fopen(filename, "rb");
    int result;
    if (!f) return stbi__err("can't fopen", "Unable to open file");
    result = stbi_is_16_bit_from_file(f);
    fclose(f);
    return result;
}

STBIDEF int stbi_is_16_bit_from_file(FILE *f)
{
   int r;
   stbi__context s;
   long pos = ftell(f);
   stbi__start_file(&s, f);
   r = stbi__is_16_main(&s);
   fseek(f,pos,SEEK_SET);
   return r;
}
#endif // !STBI_NO_STDIO

STBIDEF int stbi_info_from_memory(stbi_uc const *buffer, int len, int *x, int *y, int *comp)
{
   stbi__context s;
   stbi__start_mem(&s,buffer,len);
   return stbi__info_main(&s,x,y,comp);
}

STBIDEF int stbi_info_from_callbacks(stbi_io_callbacks const *c, void *user, int *x, int *y, int *comp)
{
   stbi__context s;
   stbi__start_callbacks(&s, (stbi_io_callbacks *) c, user);
   return stbi__info_main(&s,x,y,comp);
}

STBIDEF int stbi_is_16_bit_from_memory(stbi_uc const *buffer, int len)
{
   stbi__context s;
   stbi__start_mem(&s,buffer,len);
   return stbi__is_16_main(&s);
}

STBIDEF int stbi_is_16_bit_from_callbacks(stbi_io_callbacks const *c, void *user)
{
   stbi__context s;
   stbi__start_callbacks(&s, (stbi_io_callbacks *) c, user);
   return stbi__is_16_main(&s);
}

#endif // STB_IMAGE_IMPLEMENTATION

/*
   revision history:
      2.20  (2019-02-07) support utf8 filenames in Windows; fix warnings and platform ifdefs
      2.19  (2018-02-11) fix warning
      2.18  (2018-01-30) fix warnings
      2.17  (2018-01-29) change sbti__shiftsigned to avoid clang -O2 bug
                         1-bit BMP
                         *_is_16_bit api
                         avoid warnings
      2.16  (2017-07-23) all functions have 16-bit variants;
                         STBI_NO_STDIO works again;
                         compilation fixes;
                         fix rounding in unpremultiply;
                         optimize vertical flip;
                         disable raw_len validation;
                         documentation fixes
      2.15  (2017-03-18) fix png-1,2,4 bug; now all Imagenet JPGs decode;
                         warning fixes; disable run-time SSE detection on gcc;
                         uniform handling of optional "return" values;
                         thread-safe initialization of zlib tables
      2.14  (2017-03-03) remove deprecated STBI_JPEG_OLD; fixes for Imagenet JPGs
      2.13  (2016-11-29) add 16-bit API, only supported for PNG right now
      2.12  (2016-04-02) fix typo in 2.11 PSD fix that caused crashes
      2.11  (2016-04-02) allocate large structures on the stack
                         remove white matting for transparent PSD
                         fix reported channel count for PNG & BMP
                         re-enable SSE2 in non-gcc 64-bit
                         support RGB-formatted JPEG
                         read 16-bit PNGs (only as 8-bit)
      2.10  (2016-01-22) avoid warning introduced in 2.09 by STBI_REALLOC_SIZED
      2.09  (2016-01-16) allow comments in PNM files
                         16-bit-per-pixel TGA (not bit-per-component)
                         info() for TGA could break due to .hdr handling
                         info() for BMP to shares code instead of sloppy parse
                         can use STBI_REALLOC_SIZED if allocator doesn't support realloc
                         code cleanup
      2.08  (2015-09-13) fix to 2.07 cleanup, reading RGB PSD as RGBA
      2.07  (2015-09-13) fix compiler warnings
                         partial animated GIF support
                         limited 16-bpc PSD support
                         #ifdef unused functions
                         bug with < 92 byte PIC,PNM,HDR,TGA
      2.06  (2015-04-19) fix bug where PSD returns wrong '*comp' value
      2.05  (2015-04-19) fix bug in progressive JPEG handling, fix warning
      2.04  (2015-04-15) try to re-enable SIMD on MinGW 64-bit
      2.03  (2015-04-12) extra corruption checking (mmozeiko)
                         stbi_set_flip_vertically_on_load (nguillemot)
                         fix NEON support; fix mingw support
      2.02  (2015-01-19) fix incorrect assert, fix warning
      2.01  (2015-01-17) fix various warnings; suppress SIMD on gcc 32-bit without -msse2
      2.00b (2014-12-25) fix STBI_MALLOC in progressive JPEG
      2.00  (2014-12-25) optimize JPG, including x86 SSE2 & NEON SIMD (ryg)
                         progressive JPEG (stb)
                         PGM/PPM support (Ken Miller)
                         STBI_MALLOC,STBI_REALLOC,STBI_FREE
                         GIF bugfix -- seemingly never worked
                         STBI_NO_*, STBI_ONLY_*
      1.48  (2014-12-14) fix incorrectly-named assert()
      1.47  (2014-12-14) 1/2/4-bit PNG support, both direct and paletted (Omar Cornut & stb)
                         optimize PNG (ryg)
                         fix bug in interlaced PNG with user-specified channel count (stb)
      1.46  (2014-08-26)
              fix broken tRNS chunk (colorkey-style transparency) in non-paletted PNG
      1.45  (2014-08-16)
              fix MSVC-ARM internal compiler error by wrapping malloc
      1.44  (2014-08-07)
              various warning fixes from Ronny Chevalier
      1.43  (2014-07-15)
              fix MSVC-only compiler problem in code changed in 1.42
      1.42  (2014-07-09)
              don't define _CRT_SECURE_NO_WARNINGS (affects user code)
              fixes to stbi__cleanup_jpeg path
              added STBI_ASSERT to avoid requiring assert.h
      1.41  (2014-06-25)
              fix search&replace from 1.36 that messed up comments/error messages
      1.40  (2014-06-22)
              fix gcc struct-initialization warning
      1.39  (2014-06-15)
              fix to TGA optimization when req_comp != number of components in TGA;
              fix to GIF loading because BMP wasn't rewinding (whoops, no GIFs in my test suite)
              add support for BMP version 5 (more ignored fields)
      1.38  (2014-06-06)
              suppress MSVC warnings on integer casts truncating values
              fix accidental rename of 'flags' field of I/O
      1.37  (2014-06-04)
              remove duplicate typedef
      1.36  (2014-06-03)
              convert to header file single-file library
              if de-iphone isn't set, load iphone images color-swapped instead of returning NULL
      1.35  (2014-05-27)
              various warnings
              fix broken STBI_SIMD path
              fix bug where stbi_load_from_file no longer left file pointer in correct place
              fix broken non-easy path for 32-bit BMP (possibly never used)
              TGA optimization by Arseny Kapoulkine
      1.34  (unknown)
              use STBI_NOTUSED in stbi__resample_row_generic(), fix one more leak in tga failure case
      1.33  (2011-07-14)
              make stbi_is_hdr work in STBI_NO_HDR (as specified), minor compiler-friendly improvements
      1.32  (2011-07-13)
              support for "info" function for all supported filetypes (SpartanJ)
      1.31  (2011-06-20)
              a few more leak fixes, bug in PNG handling (SpartanJ)
      1.30  (2011-06-11)
              added ability to load files via callbacks to accomidate custom input streams (Ben Wenger)
              removed deprecated format-specific test/load functions
              removed support for installable file formats (stbi_loader) -- would have been broken for IO callbacks anyway
              error cases in bmp and tga give messages and don't leak (Raymond Barbiero, grisha)
              fix inefficiency in decoding 32-bit BMP (David Woo)
      1.29  (2010-08-16)
              various warning fixes from Aurelien Pocheville
      1.28  (2010-08-01)
              fix bug in GIF palette transparency (SpartanJ)
      1.27  (2010-08-01)
              cast-to-stbi_uc to fix warnings
      1.26  (2010-07-24)
              fix bug in file buffering for PNG reported by SpartanJ
      1.25  (2010-07-17)
              refix trans_data warning (Won Chun)
      1.24  (2010-07-12)
              perf improvements reading from files on platforms with lock-heavy fgetc()
              minor perf improvements for jpeg
              deprecated type-specific functions so we'll get feedback if they're needed
              attempt to fix trans_data warning (Won Chun)
      1.23    fixed bug in iPhone support
      1.22  (2010-07-10)
              removed image *writing* support
              stbi_info support from Jetro Lauha
              GIF support from Jean-Marc Lienher
              iPhone PNG-extensions from James Brown
              warning-fixes from Nicolas Schulz and Janez Zemva (i.stbi__err. Janez (U+017D)emva)
      1.21    fix use of 'stbi_uc' in header (reported by jon blow)
      1.20    added support for Softimage PIC, by Tom Seddon
      1.19    bug in interlaced PNG corruption check (found by ryg)
      1.18  (2008-08-02)
              fix a threading bug (local mutable static)
      1.17    support interlaced PNG
      1.16    major bugfix - stbi__convert_format converted one too many pixels
      1.15    initialize some fields for thread safety
      1.14    fix threadsafe conversion bug
              header-file-only version (#define STBI_HEADER_FILE_ONLY before including)
      1.13    threadsafe
      1.12    const qualifiers in the API
      1.11    Support installable IDCT, colorspace conversion routines
      1.10    Fixes for 64-bit (don't use "unsigned long")
              optimized upsampling by Fabian "ryg" Giesen
      1.09    Fix format-conversion for PSD code (bad global variables!)
      1.08    Thatcher Ulrich's PSD code integrated by Nicolas Schulz
      1.07    attempt to fix C++ warning/errors again
      1.06    attempt to fix C++ warning/errors again
      1.05    fix TGA loading to return correct *comp and use good luminance calc
      1.04    default float alpha is 1, not 255; use 'void *' for stbi_image_free
      1.03    bugfixes to STBI_NO_STDIO, STBI_NO_HDR
      1.02    support for (subset of) HDR files, float interface for preferred access to them
      1.01    fix bug: possible bug in handling right-side up bmps... not sure
              fix bug: the stbi__bmp_load() and stbi__tga_load() functions didn't work at all
      1.00    interface to zlib that skips zlib header
      0.99    correct handling of alpha in palette
      0.98    TGA loader by lonesock; dynamically add loaders (untested)
      0.97    jpeg errors on too large a file; also catch another malloc failure
      0.96    fix detection of invalid v value - particleman@mollyrocket forum
      0.95    during header scan, seek to markers in case of padding
      0.94    STBI_NO_STDIO to disable stdio usage; rename all #defines the same
      0.93    handle jpegtran output; verbose errors
      0.92    read 4,8,16,24,32-bit BMP files of several formats
      0.91    output 24-bit Windows 3.0 BMP files
      0.90    fix a few more warnings; bump version number to approach 1.0
      0.61    bugfixes due to Marc LeBlanc, Christopher Lloyd
      0.60    fix compiling as c++
      0.59    fix warnings: merge Dave Moore's -Wall fixes
      0.58    fix bug: zlib uncompressed mode len/nlen was wrong endian
      0.57    fix bug: jpg last huffman symbol before marker was >9 bits but less than 16 available
      0.56    fix bug: zlib uncompressed mode len vs. nlen
      0.55    fix bug: restart_interval not initialized to 0
      0.54    allow NULL for 'int *comp'
      0.53    fix bug in png 3->4; speedup png decoding
      0.52    png handles req_comp=3,4 directly; minor cleanup; jpeg comments
      0.51    obey req_comp requests, 1-component jpegs return as 1-component,
              on 'test' only check type, not whether we support this variant
      0.50  (2006-11-19)
              first released version
*/


/*
------------------------------------------------------------------------------
This software is available under 2 licenses -- choose whichever you prefer.
------------------------------------------------------------------------------
ALTERNATIVE A - MIT License
Copyright (c) 2017 Sean Barrett
Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files (the "Software"), to deal in
the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
of the Software, and to permit persons to whom the Software is furnished to do
so, subject to the following conditions:
The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.
THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
------------------------------------------------------------------------------
ALTERNATIVE B - Public Domain (www.unlicense.org)
This is free and unencumbered software released into the public domain.
Anyone is free to copy, modify, publish, use, compile, sell, or distribute this
software, either in source code form or as a compiled binary, for any purpose,
commercial or non-commercial, and by any means.
In jurisdictions that recognize copyright laws, the author or authors of this
software dedicate any and all copyright interest in the software to the public
domain. We make this dedication for the benefit of the public at large and to
the detriment of our heirs and successors. We intend this dedication to be an
overt act of relinquishment in perpetuity of all present and future rights to
this software under copyright law.
THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
------------------------------------------------------------------------------
*/

/*
** Copyright (c) 2014-2020 The Khronos Group Inc.
**
** Permission is hereby granted, free of charge, to any person obtaining a copy
** of this software and/or associated documentation files (the "Materials"),
** to deal in the Materials without restriction, including without limitation
** the rights to use, copy, modify, merge, publish, distribute, sublicense,
** and/or sell copies of the Materials, and to permit persons to whom the
** Materials are furnished to do so, subject to the following conditions:
**
** The above copyright notice and this permission notice shall be included in
** all copies or substantial portions of the Materials.
**
** MODIFICATIONS TO THIS FILE MAY MEAN IT NO LONGER ACCURATELY REFLECTS KHRONOS
** STANDARDS. THE UNMODIFIED, NORMATIVE VERSIONS OF KHRONOS SPECIFICATIONS AND
** HEADER INFORMATION ARE LOCATED AT https://www.khronos.org/registry/
**
** THE MATERIALS ARE PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
** OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
** FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
** THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
** LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
** FROM,OUT OF OR IN CONNECTION WITH THE MATERIALS OR THE USE OR OTHER DEALINGS
** IN THE MATERIALS.
*/

/*
** This header is automatically generated by the same tool that creates
** the Binary Section of the SPIR-V specification.
*/

/*
** Enumeration tokens for SPIR-V, in various styles:
**   C, C++, C++11, JSON, Lua, Python, C#, D, Beef
**
** - C will have tokens with a "Spv" prefix, e.g.: SpvSourceLanguageGLSL
** - C++ will have tokens in the "spv" name space, e.g.: spv::SourceLanguageGLSL
** - C++11 will use enum classes in the spv namespace, e.g.:
*spv::SourceLanguage::GLSL
** - Lua will use tables, e.g.: spv.SourceLanguage.GLSL
** - Python will use dictionaries, e.g.: spv['SourceLanguage']['GLSL']
** - C# will use enum classes in the Specification class located in the "Spv"
*namespace,
**     e.g.: Spv.Specification.SourceLanguage.GLSL
** - D will have tokens under the "spv" module, e.g: spv.SourceLanguage.GLSL
** - Beef will use enum classes in the Specification class located in the "Spv"
*namespace,
**     e.g.: Spv.Specification.SourceLanguage.GLSL
**
** Some tokens act like mask values, which can be OR'd together,
** while others are mutually exclusive.  The mask-like ones have
** "Mask" in their name, and a parallel enum that has the shift
** amount (1 << x) for each corresponding enumerant.
*/

#ifndef spirv_H
#define spirv_H

typedef unsigned int SpvId;

#define SPV_VERSION 0x10600
#define SPV_REVISION 1

static const unsigned int SpvMagicNumber = 0x07230203;
static const unsigned int SpvVersion = 0x00010600;
static const unsigned int SpvRevision = 1;
static const unsigned int SpvOpCodeMask = 0xffff;
static const unsigned int SpvWordCountShift = 16;

typedef enum SpvSourceLanguage_ {
    SpvSourceLanguageUnknown = 0,
    SpvSourceLanguageESSL = 1,
    SpvSourceLanguageGLSL = 2,
    SpvSourceLanguageOpenCL_C = 3,
    SpvSourceLanguageOpenCL_CPP = 4,
    SpvSourceLanguageHLSL = 5,
    SpvSourceLanguageCPP_for_OpenCL = 6,
    SpvSourceLanguageSYCL = 7,
    SpvSourceLanguageHERO_C = 8,
    SpvSourceLanguageNZSL = 9,
    SpvSourceLanguageMax = 0x7fffffff,
} SpvSourceLanguage;

typedef enum SpvExecutionModel_ {
    SpvExecutionModelVertex = 0,
    SpvExecutionModelTessellationControl = 1,
    SpvExecutionModelTessellationEvaluation = 2,
    SpvExecutionModelGeometry = 3,
    SpvExecutionModelFragment = 4,
    SpvExecutionModelGLCompute = 5,
    SpvExecutionModelKernel = 6,
    SpvExecutionModelTaskNV = 5267,
    SpvExecutionModelMeshNV = 5268,
    SpvExecutionModelRayGenerationKHR = 5313,
    SpvExecutionModelRayGenerationNV = 5313,
    SpvExecutionModelIntersectionKHR = 5314,
    SpvExecutionModelIntersectionNV = 5314,
    SpvExecutionModelAnyHitKHR = 5315,
    SpvExecutionModelAnyHitNV = 5315,
    SpvExecutionModelClosestHitKHR = 5316,
    SpvExecutionModelClosestHitNV = 5316,
    SpvExecutionModelMissKHR = 5317,
    SpvExecutionModelMissNV = 5317,
    SpvExecutionModelCallableKHR = 5318,
    SpvExecutionModelCallableNV = 5318,
    SpvExecutionModelTaskEXT = 5364,
    SpvExecutionModelMeshEXT = 5365,
    SpvExecutionModelMax = 0x7fffffff,
} SpvExecutionModel;

typedef enum SpvAddressingModel_ {
    SpvAddressingModelLogical = 0,
    SpvAddressingModelPhysical32 = 1,
    SpvAddressingModelPhysical64 = 2,
    SpvAddressingModelPhysicalStorageBuffer64 = 5348,
    SpvAddressingModelPhysicalStorageBuffer64EXT = 5348,
    SpvAddressingModelMax = 0x7fffffff,
} SpvAddressingModel;

typedef enum SpvMemoryModel_ {
    SpvMemoryModelSimple = 0,
    SpvMemoryModelGLSL450 = 1,
    SpvMemoryModelOpenCL = 2,
    SpvMemoryModelVulkan = 3,
    SpvMemoryModelVulkanKHR = 3,
    SpvMemoryModelMax = 0x7fffffff,
} SpvMemoryModel;

typedef enum SpvExecutionMode_ {
    SpvExecutionModeInvocations = 0,
    SpvExecutionModeSpacingEqual = 1,
    SpvExecutionModeSpacingFractionalEven = 2,
    SpvExecutionModeSpacingFractionalOdd = 3,
    SpvExecutionModeVertexOrderCw = 4,
    SpvExecutionModeVertexOrderCcw = 5,
    SpvExecutionModePixelCenterInteger = 6,
    SpvExecutionModeOriginUpperLeft = 7,
    SpvExecutionModeOriginLowerLeft = 8,
    SpvExecutionModeEarlyFragmentTests = 9,
    SpvExecutionModePointMode = 10,
    SpvExecutionModeXfb = 11,
    SpvExecutionModeDepthReplacing = 12,
    SpvExecutionModeDepthGreater = 14,
    SpvExecutionModeDepthLess = 15,
    SpvExecutionModeDepthUnchanged = 16,
    SpvExecutionModeLocalSize = 17,
    SpvExecutionModeLocalSizeHint = 18,
    SpvExecutionModeInputPoints = 19,
    SpvExecutionModeInputLines = 20,
    SpvExecutionModeInputLinesAdjacency = 21,
    SpvExecutionModeTriangles = 22,
    SpvExecutionModeInputTrianglesAdjacency = 23,
    SpvExecutionModeQuads = 24,
    SpvExecutionModeIsolines = 25,
    SpvExecutionModeOutputVertices = 26,
    SpvExecutionModeOutputPoints = 27,
    SpvExecutionModeOutputLineStrip = 28,
    SpvExecutionModeOutputTriangleStrip = 29,
    SpvExecutionModeVecTypeHint = 30,
    SpvExecutionModeContractionOff = 31,
    SpvExecutionModeInitializer = 33,
    SpvExecutionModeFinalizer = 34,
    SpvExecutionModeSubgroupSize = 35,
    SpvExecutionModeSubgroupsPerWorkgroup = 36,
    SpvExecutionModeSubgroupsPerWorkgroupId = 37,
    SpvExecutionModeLocalSizeId = 38,
    SpvExecutionModeLocalSizeHintId = 39,
    SpvExecutionModeNonCoherentColorAttachmentReadEXT = 4169,
    SpvExecutionModeNonCoherentDepthAttachmentReadEXT = 4170,
    SpvExecutionModeNonCoherentStencilAttachmentReadEXT = 4171,
    SpvExecutionModeSubgroupUniformControlFlowKHR = 4421,
    SpvExecutionModePostDepthCoverage = 4446,
    SpvExecutionModeDenormPreserve = 4459,
    SpvExecutionModeDenormFlushToZero = 4460,
    SpvExecutionModeSignedZeroInfNanPreserve = 4461,
    SpvExecutionModeRoundingModeRTE = 4462,
    SpvExecutionModeRoundingModeRTZ = 4463,
    SpvExecutionModeEarlyAndLateFragmentTestsAMD = 5017,
    SpvExecutionModeStencilRefReplacingEXT = 5027,
    SpvExecutionModeStencilRefUnchangedFrontAMD = 5079,
    SpvExecutionModeStencilRefGreaterFrontAMD = 5080,
    SpvExecutionModeStencilRefLessFrontAMD = 5081,
    SpvExecutionModeStencilRefUnchangedBackAMD = 5082,
    SpvExecutionModeStencilRefGreaterBackAMD = 5083,
    SpvExecutionModeStencilRefLessBackAMD = 5084,
    SpvExecutionModeOutputLinesEXT = 5269,
    SpvExecutionModeOutputLinesNV = 5269,
    SpvExecutionModeOutputPrimitivesEXT = 5270,
    SpvExecutionModeOutputPrimitivesNV = 5270,
    SpvExecutionModeDerivativeGroupQuadsNV = 5289,
    SpvExecutionModeDerivativeGroupLinearNV = 5290,
    SpvExecutionModeOutputTrianglesEXT = 5298,
    SpvExecutionModeOutputTrianglesNV = 5298,
    SpvExecutionModePixelInterlockOrderedEXT = 5366,
    SpvExecutionModePixelInterlockUnorderedEXT = 5367,
    SpvExecutionModeSampleInterlockOrderedEXT = 5368,
    SpvExecutionModeSampleInterlockUnorderedEXT = 5369,
    SpvExecutionModeShadingRateInterlockOrderedEXT = 5370,
    SpvExecutionModeShadingRateInterlockUnorderedEXT = 5371,
    SpvExecutionModeSharedLocalMemorySizeINTEL = 5618,
    SpvExecutionModeRoundingModeRTPINTEL = 5620,
    SpvExecutionModeRoundingModeRTNINTEL = 5621,
    SpvExecutionModeFloatingPointModeALTINTEL = 5622,
    SpvExecutionModeFloatingPointModeIEEEINTEL = 5623,
    SpvExecutionModeMaxWorkgroupSizeINTEL = 5893,
    SpvExecutionModeMaxWorkDimINTEL = 5894,
    SpvExecutionModeNoGlobalOffsetINTEL = 5895,
    SpvExecutionModeNumSIMDWorkitemsINTEL = 5896,
    SpvExecutionModeSchedulerTargetFmaxMhzINTEL = 5903,
    SpvExecutionModeStreamingInterfaceINTEL = 6154,
    SpvExecutionModeRegisterMapInterfaceINTEL = 6160,
    SpvExecutionModeNamedBarrierCountINTEL = 6417,
    SpvExecutionModeMax = 0x7fffffff,
} SpvExecutionMode;

typedef enum SpvStorageClass_ {
    SpvStorageClassUniformConstant = 0,
    SpvStorageClassInput = 1,
    SpvStorageClassUniform = 2,
    SpvStorageClassOutput = 3,
    SpvStorageClassWorkgroup = 4,
    SpvStorageClassCrossWorkgroup = 5,
    SpvStorageClassPrivate = 6,
    SpvStorageClassFunction = 7,
    SpvStorageClassGeneric = 8,
    SpvStorageClassPushConstant = 9,
    SpvStorageClassAtomicCounter = 10,
    SpvStorageClassImage = 11,
    SpvStorageClassStorageBuffer = 12,
    SpvStorageClassTileImageEXT = 4172,
    SpvStorageClassCallableDataKHR = 5328,
    SpvStorageClassCallableDataNV = 5328,
    SpvStorageClassIncomingCallableDataKHR = 5329,
    SpvStorageClassIncomingCallableDataNV = 5329,
    SpvStorageClassRayPayloadKHR = 5338,
    SpvStorageClassRayPayloadNV = 5338,
    SpvStorageClassHitAttributeKHR = 5339,
    SpvStorageClassHitAttributeNV = 5339,
    SpvStorageClassIncomingRayPayloadKHR = 5342,
    SpvStorageClassIncomingRayPayloadNV = 5342,
    SpvStorageClassShaderRecordBufferKHR = 5343,
    SpvStorageClassShaderRecordBufferNV = 5343,
    SpvStorageClassPhysicalStorageBuffer = 5349,
    SpvStorageClassPhysicalStorageBufferEXT = 5349,
    SpvStorageClassHitObjectAttributeNV = 5385,
    SpvStorageClassTaskPayloadWorkgroupEXT = 5402,
    SpvStorageClassCodeSectionINTEL = 5605,
    SpvStorageClassDeviceOnlyINTEL = 5936,
    SpvStorageClassHostOnlyINTEL = 5937,
    SpvStorageClassMax = 0x7fffffff,
} SpvStorageClass;

typedef enum SpvDim_ {
    SpvDim1D = 0,
    SpvDim2D = 1,
    SpvDim3D = 2,
    SpvDimCube = 3,
    SpvDimRect = 4,
    SpvDimBuffer = 5,
    SpvDimSubpassData = 6,
    SpvDimTileImageDataEXT = 4173,
    SpvDimMax = 0x7fffffff,
} SpvDim;

typedef enum SpvSamplerAddressingMode_ {
    SpvSamplerAddressingModeNone = 0,
    SpvSamplerAddressingModeClampToEdge = 1,
    SpvSamplerAddressingModeClamp = 2,
    SpvSamplerAddressingModeRepeat = 3,
    SpvSamplerAddressingModeRepeatMirrored = 4,
    SpvSamplerAddressingModeMax = 0x7fffffff,
} SpvSamplerAddressingMode;

typedef enum SpvSamplerFilterMode_ {
    SpvSamplerFilterModeNearest = 0,
    SpvSamplerFilterModeLinear = 1,
    SpvSamplerFilterModeMax = 0x7fffffff,
} SpvSamplerFilterMode;

typedef enum SpvImageFormat_ {
    SpvImageFormatUnknown = 0,
    SpvImageFormatRgba32f = 1,
    SpvImageFormatRgba16f = 2,
    SpvImageFormatR32f = 3,
    SpvImageFormatRgba8 = 4,
    SpvImageFormatRgba8Snorm = 5,
    SpvImageFormatRg32f = 6,
    SpvImageFormatRg16f = 7,
    SpvImageFormatR11fG11fB10f = 8,
    SpvImageFormatR16f = 9,
    SpvImageFormatRgba16 = 10,
    SpvImageFormatRgb10A2 = 11,
    SpvImageFormatRg16 = 12,
    SpvImageFormatRg8 = 13,
    SpvImageFormatR16 = 14,
    SpvImageFormatR8 = 15,
    SpvImageFormatRgba16Snorm = 16,
    SpvImageFormatRg16Snorm = 17,
    SpvImageFormatRg8Snorm = 18,
    SpvImageFormatR16Snorm = 19,
    SpvImageFormatR8Snorm = 20,
    SpvImageFormatRgba32i = 21,
    SpvImageFormatRgba16i = 22,
    SpvImageFormatRgba8i = 23,
    SpvImageFormatR32i = 24,
    SpvImageFormatRg32i = 25,
    SpvImageFormatRg16i = 26,
    SpvImageFormatRg8i = 27,
    SpvImageFormatR16i = 28,
    SpvImageFormatR8i = 29,
    SpvImageFormatRgba32ui = 30,
    SpvImageFormatRgba16ui = 31,
    SpvImageFormatRgba8ui = 32,
    SpvImageFormatR32ui = 33,
    SpvImageFormatRgb10a2ui = 34,
    SpvImageFormatRg32ui = 35,
    SpvImageFormatRg16ui = 36,
    SpvImageFormatRg8ui = 37,
    SpvImageFormatR16ui = 38,
    SpvImageFormatR8ui = 39,
    SpvImageFormatR64ui = 40,
    SpvImageFormatR64i = 41,
    SpvImageFormatMax = 0x7fffffff,
} SpvImageFormat;

typedef enum SpvImageChannelOrder_ {
    SpvImageChannelOrderR = 0,
    SpvImageChannelOrderA = 1,
    SpvImageChannelOrderRG = 2,
    SpvImageChannelOrderRA = 3,
    SpvImageChannelOrderRGB = 4,
    SpvImageChannelOrderRGBA = 5,
    SpvImageChannelOrderBGRA = 6,
    SpvImageChannelOrderARGB = 7,
    SpvImageChannelOrderIntensity = 8,
    SpvImageChannelOrderLuminance = 9,
    SpvImageChannelOrderRx = 10,
    SpvImageChannelOrderRGx = 11,
    SpvImageChannelOrderRGBx = 12,
    SpvImageChannelOrderDepth = 13,
    SpvImageChannelOrderDepthStencil = 14,
    SpvImageChannelOrdersRGB = 15,
    SpvImageChannelOrdersRGBx = 16,
    SpvImageChannelOrdersRGBA = 17,
    SpvImageChannelOrdersBGRA = 18,
    SpvImageChannelOrderABGR = 19,
    SpvImageChannelOrderMax = 0x7fffffff,
} SpvImageChannelOrder;

typedef enum SpvImageChannelDataType_ {
    SpvImageChannelDataTypeSnormInt8 = 0,
    SpvImageChannelDataTypeSnormInt16 = 1,
    SpvImageChannelDataTypeUnormInt8 = 2,
    SpvImageChannelDataTypeUnormInt16 = 3,
    SpvImageChannelDataTypeUnormShort565 = 4,
    SpvImageChannelDataTypeUnormShort555 = 5,
    SpvImageChannelDataTypeUnormInt101010 = 6,
    SpvImageChannelDataTypeSignedInt8 = 7,
    SpvImageChannelDataTypeSignedInt16 = 8,
    SpvImageChannelDataTypeSignedInt32 = 9,
    SpvImageChannelDataTypeUnsignedInt8 = 10,
    SpvImageChannelDataTypeUnsignedInt16 = 11,
    SpvImageChannelDataTypeUnsignedInt32 = 12,
    SpvImageChannelDataTypeHalfFloat = 13,
    SpvImageChannelDataTypeFloat = 14,
    SpvImageChannelDataTypeUnormInt24 = 15,
    SpvImageChannelDataTypeUnormInt101010_2 = 16,
    SpvImageChannelDataTypeUnsignedIntRaw10EXT = 19,
    SpvImageChannelDataTypeUnsignedIntRaw12EXT = 20,
    SpvImageChannelDataTypeMax = 0x7fffffff,
} SpvImageChannelDataType;

typedef enum SpvImageOperandsShift_ {
    SpvImageOperandsBiasShift = 0,
    SpvImageOperandsLodShift = 1,
    SpvImageOperandsGradShift = 2,
    SpvImageOperandsConstOffsetShift = 3,
    SpvImageOperandsOffsetShift = 4,
    SpvImageOperandsConstOffsetsShift = 5,
    SpvImageOperandsSampleShift = 6,
    SpvImageOperandsMinLodShift = 7,
    SpvImageOperandsMakeTexelAvailableShift = 8,
    SpvImageOperandsMakeTexelAvailableKHRShift = 8,
    SpvImageOperandsMakeTexelVisibleShift = 9,
    SpvImageOperandsMakeTexelVisibleKHRShift = 9,
    SpvImageOperandsNonPrivateTexelShift = 10,
    SpvImageOperandsNonPrivateTexelKHRShift = 10,
    SpvImageOperandsVolatileTexelShift = 11,
    SpvImageOperandsVolatileTexelKHRShift = 11,
    SpvImageOperandsSignExtendShift = 12,
    SpvImageOperandsZeroExtendShift = 13,
    SpvImageOperandsNontemporalShift = 14,
    SpvImageOperandsOffsetsShift = 16,
    SpvImageOperandsMax = 0x7fffffff,
} SpvImageOperandsShift;

typedef enum SpvImageOperandsMask_ {
    SpvImageOperandsMaskNone = 0,
    SpvImageOperandsBiasMask = 0x00000001,
    SpvImageOperandsLodMask = 0x00000002,
    SpvImageOperandsGradMask = 0x00000004,
    SpvImageOperandsConstOffsetMask = 0x00000008,
    SpvImageOperandsOffsetMask = 0x00000010,
    SpvImageOperandsConstOffsetsMask = 0x00000020,
    SpvImageOperandsSampleMask = 0x00000040,
    SpvImageOperandsMinLodMask = 0x00000080,
    SpvImageOperandsMakeTexelAvailableMask = 0x00000100,
    SpvImageOperandsMakeTexelAvailableKHRMask = 0x00000100,
    SpvImageOperandsMakeTexelVisibleMask = 0x00000200,
    SpvImageOperandsMakeTexelVisibleKHRMask = 0x00000200,
    SpvImageOperandsNonPrivateTexelMask = 0x00000400,
    SpvImageOperandsNonPrivateTexelKHRMask = 0x00000400,
    SpvImageOperandsVolatileTexelMask = 0x00000800,
    SpvImageOperandsVolatileTexelKHRMask = 0x00000800,
    SpvImageOperandsSignExtendMask = 0x00001000,
    SpvImageOperandsZeroExtendMask = 0x00002000,
    SpvImageOperandsNontemporalMask = 0x00004000,
    SpvImageOperandsOffsetsMask = 0x00010000,
} SpvImageOperandsMask;

typedef enum SpvFPFastMathModeShift_ {
    SpvFPFastMathModeNotNaNShift = 0,
    SpvFPFastMathModeNotInfShift = 1,
    SpvFPFastMathModeNSZShift = 2,
    SpvFPFastMathModeAllowRecipShift = 3,
    SpvFPFastMathModeFastShift = 4,
    SpvFPFastMathModeAllowContractFastINTELShift = 16,
    SpvFPFastMathModeAllowReassocINTELShift = 17,
    SpvFPFastMathModeMax = 0x7fffffff,
} SpvFPFastMathModeShift;

typedef enum SpvFPFastMathModeMask_ {
    SpvFPFastMathModeMaskNone = 0,
    SpvFPFastMathModeNotNaNMask = 0x00000001,
    SpvFPFastMathModeNotInfMask = 0x00000002,
    SpvFPFastMathModeNSZMask = 0x00000004,
    SpvFPFastMathModeAllowRecipMask = 0x00000008,
    SpvFPFastMathModeFastMask = 0x00000010,
    SpvFPFastMathModeAllowContractFastINTELMask = 0x00010000,
    SpvFPFastMathModeAllowReassocINTELMask = 0x00020000,
} SpvFPFastMathModeMask;

typedef enum SpvFPRoundingMode_ {
    SpvFPRoundingModeRTE = 0,
    SpvFPRoundingModeRTZ = 1,
    SpvFPRoundingModeRTP = 2,
    SpvFPRoundingModeRTN = 3,
    SpvFPRoundingModeMax = 0x7fffffff,
} SpvFPRoundingMode;

typedef enum SpvLinkageType_ {
    SpvLinkageTypeExport = 0,
    SpvLinkageTypeImport = 1,
    SpvLinkageTypeLinkOnceODR = 2,
    SpvLinkageTypeMax = 0x7fffffff,
} SpvLinkageType;

typedef enum SpvAccessQualifier_ {
    SpvAccessQualifierReadOnly = 0,
    SpvAccessQualifierWriteOnly = 1,
    SpvAccessQualifierReadWrite = 2,
    SpvAccessQualifierMax = 0x7fffffff,
} SpvAccessQualifier;

typedef enum SpvFunctionParameterAttribute_ {
    SpvFunctionParameterAttributeZext = 0,
    SpvFunctionParameterAttributeSext = 1,
    SpvFunctionParameterAttributeByVal = 2,
    SpvFunctionParameterAttributeSret = 3,
    SpvFunctionParameterAttributeNoAlias = 4,
    SpvFunctionParameterAttributeNoCapture = 5,
    SpvFunctionParameterAttributeNoWrite = 6,
    SpvFunctionParameterAttributeNoReadWrite = 7,
    SpvFunctionParameterAttributeRuntimeAlignedINTEL = 5940,
    SpvFunctionParameterAttributeMax = 0x7fffffff,
} SpvFunctionParameterAttribute;

typedef enum SpvDecoration_ {
    SpvDecorationRelaxedPrecision = 0,
    SpvDecorationSpecId = 1,
    SpvDecorationBlock = 2,
    SpvDecorationBufferBlock = 3,
    SpvDecorationRowMajor = 4,
    SpvDecorationColMajor = 5,
    SpvDecorationArrayStride = 6,
    SpvDecorationMatrixStride = 7,
    SpvDecorationGLSLShared = 8,
    SpvDecorationGLSLPacked = 9,
    SpvDecorationCPacked = 10,
    SpvDecorationBuiltIn = 11,
    SpvDecorationNoPerspective = 13,
    SpvDecorationFlat = 14,
    SpvDecorationPatch = 15,
    SpvDecorationCentroid = 16,
    SpvDecorationSample = 17,
    SpvDecorationInvariant = 18,
    SpvDecorationRestrict = 19,
    SpvDecorationAliased = 20,
    SpvDecorationVolatile = 21,
    SpvDecorationConstant = 22,
    SpvDecorationCoherent = 23,
    SpvDecorationNonWritable = 24,
    SpvDecorationNonReadable = 25,
    SpvDecorationUniform = 26,
    SpvDecorationUniformId = 27,
    SpvDecorationSaturatedConversion = 28,
    SpvDecorationStream = 29,
    SpvDecorationLocation = 30,
    SpvDecorationComponent = 31,
    SpvDecorationIndex = 32,
    SpvDecorationBinding = 33,
    SpvDecorationDescriptorSet = 34,
    SpvDecorationOffset = 35,
    SpvDecorationXfbBuffer = 36,
    SpvDecorationXfbStride = 37,
    SpvDecorationFuncParamAttr = 38,
    SpvDecorationFPRoundingMode = 39,
    SpvDecorationFPFastMathMode = 40,
    SpvDecorationLinkageAttributes = 41,
    SpvDecorationNoContraction = 42,
    SpvDecorationInputAttachmentIndex = 43,
    SpvDecorationAlignment = 44,
    SpvDecorationMaxByteOffset = 45,
    SpvDecorationAlignmentId = 46,
    SpvDecorationMaxByteOffsetId = 47,
    SpvDecorationNoSignedWrap = 4469,
    SpvDecorationNoUnsignedWrap = 4470,
    SpvDecorationWeightTextureQCOM = 4487,
    SpvDecorationBlockMatchTextureQCOM = 4488,
    SpvDecorationExplicitInterpAMD = 4999,
    SpvDecorationOverrideCoverageNV = 5248,
    SpvDecorationPassthroughNV = 5250,
    SpvDecorationViewportRelativeNV = 5252,
    SpvDecorationSecondaryViewportRelativeNV = 5256,
    SpvDecorationPerPrimitiveEXT = 5271,
    SpvDecorationPerPrimitiveNV = 5271,
    SpvDecorationPerViewNV = 5272,
    SpvDecorationPerTaskNV = 5273,
    SpvDecorationPerVertexKHR = 5285,
    SpvDecorationPerVertexNV = 5285,
    SpvDecorationNonUniform = 5300,
    SpvDecorationNonUniformEXT = 5300,
    SpvDecorationRestrictPointer = 5355,
    SpvDecorationRestrictPointerEXT = 5355,
    SpvDecorationAliasedPointer = 5356,
    SpvDecorationAliasedPointerEXT = 5356,
    SpvDecorationHitObjectShaderRecordBufferNV = 5386,
    SpvDecorationBindlessSamplerNV = 5398,
    SpvDecorationBindlessImageNV = 5399,
    SpvDecorationBoundSamplerNV = 5400,
    SpvDecorationBoundImageNV = 5401,
    SpvDecorationSIMTCallINTEL = 5599,
    SpvDecorationReferencedIndirectlyINTEL = 5602,
    SpvDecorationClobberINTEL = 5607,
    SpvDecorationSideEffectsINTEL = 5608,
    SpvDecorationVectorComputeVariableINTEL = 5624,
    SpvDecorationFuncParamIOKindINTEL = 5625,
    SpvDecorationVectorComputeFunctionINTEL = 5626,
    SpvDecorationStackCallINTEL = 5627,
    SpvDecorationGlobalVariableOffsetINTEL = 5628,
    SpvDecorationCounterBuffer = 5634,
    SpvDecorationHlslCounterBufferGOOGLE = 5634,
    SpvDecorationHlslSemanticGOOGLE = 5635,
    SpvDecorationUserSemantic = 5635,
    SpvDecorationUserTypeGOOGLE = 5636,
    SpvDecorationFunctionRoundingModeINTEL = 5822,
    SpvDecorationFunctionDenormModeINTEL = 5823,
    SpvDecorationRegisterINTEL = 5825,
    SpvDecorationMemoryINTEL = 5826,
    SpvDecorationNumbanksINTEL = 5827,
    SpvDecorationBankwidthINTEL = 5828,
    SpvDecorationMaxPrivateCopiesINTEL = 5829,
    SpvDecorationSinglepumpINTEL = 5830,
    SpvDecorationDoublepumpINTEL = 5831,
    SpvDecorationMaxReplicatesINTEL = 5832,
    SpvDecorationSimpleDualPortINTEL = 5833,
    SpvDecorationMergeINTEL = 5834,
    SpvDecorationBankBitsINTEL = 5835,
    SpvDecorationForcePow2DepthINTEL = 5836,
    SpvDecorationBurstCoalesceINTEL = 5899,
    SpvDecorationCacheSizeINTEL = 5900,
    SpvDecorationDontStaticallyCoalesceINTEL = 5901,
    SpvDecorationPrefetchINTEL = 5902,
    SpvDecorationStallEnableINTEL = 5905,
    SpvDecorationFuseLoopsInFunctionINTEL = 5907,
    SpvDecorationMathOpDSPModeINTEL = 5909,
    SpvDecorationAliasScopeINTEL = 5914,
    SpvDecorationNoAliasINTEL = 5915,
    SpvDecorationInitiationIntervalINTEL = 5917,
    SpvDecorationMaxConcurrencyINTEL = 5918,
    SpvDecorationPipelineEnableINTEL = 5919,
    SpvDecorationBufferLocationINTEL = 5921,
    SpvDecorationIOPipeStorageINTEL = 5944,
    SpvDecorationFunctionFloatingPointModeINTEL = 6080,
    SpvDecorationSingleElementVectorINTEL = 6085,
    SpvDecorationVectorComputeCallableFunctionINTEL = 6087,
    SpvDecorationMediaBlockIOINTEL = 6140,
    SpvDecorationLatencyControlLabelINTEL = 6172,
    SpvDecorationLatencyControlConstraintINTEL = 6173,
    SpvDecorationConduitKernelArgumentINTEL = 6175,
    SpvDecorationRegisterMapKernelArgumentINTEL = 6176,
    SpvDecorationMMHostInterfaceAddressWidthINTEL = 6177,
    SpvDecorationMMHostInterfaceDataWidthINTEL = 6178,
    SpvDecorationMMHostInterfaceLatencyINTEL = 6179,
    SpvDecorationMMHostInterfaceReadWriteModeINTEL = 6180,
    SpvDecorationMMHostInterfaceMaxBurstINTEL = 6181,
    SpvDecorationMMHostInterfaceWaitRequestINTEL = 6182,
    SpvDecorationStableKernelArgumentINTEL = 6183,
    SpvDecorationMax = 0x7fffffff,
} SpvDecoration;

typedef enum SpvBuiltIn_ {
    SpvBuiltInPosition = 0,
    SpvBuiltInPointSize = 1,
    SpvBuiltInClipDistance = 3,
    SpvBuiltInCullDistance = 4,
    SpvBuiltInVertexId = 5,
    SpvBuiltInInstanceId = 6,
    SpvBuiltInPrimitiveId = 7,
    SpvBuiltInInvocationId = 8,
    SpvBuiltInLayer = 9,
    SpvBuiltInViewportIndex = 10,
    SpvBuiltInTessLevelOuter = 11,
    SpvBuiltInTessLevelInner = 12,
    SpvBuiltInTessCoord = 13,
    SpvBuiltInPatchVertices = 14,
    SpvBuiltInFragCoord = 15,
    SpvBuiltInPointCoord = 16,
    SpvBuiltInFrontFacing = 17,
    SpvBuiltInSampleId = 18,
    SpvBuiltInSamplePosition = 19,
    SpvBuiltInSampleMask = 20,
    SpvBuiltInFragDepth = 22,
    SpvBuiltInHelperInvocation = 23,
    SpvBuiltInNumWorkgroups = 24,
    SpvBuiltInWorkgroupSize = 25,
    SpvBuiltInWorkgroupId = 26,
    SpvBuiltInLocalInvocationId = 27,
    SpvBuiltInGlobalInvocationId = 28,
    SpvBuiltInLocalInvocationIndex = 29,
    SpvBuiltInWorkDim = 30,
    SpvBuiltInGlobalSize = 31,
    SpvBuiltInEnqueuedWorkgroupSize = 32,
    SpvBuiltInGlobalOffset = 33,
    SpvBuiltInGlobalLinearId = 34,
    SpvBuiltInSubgroupSize = 36,
    SpvBuiltInSubgroupMaxSize = 37,
    SpvBuiltInNumSubgroups = 38,
    SpvBuiltInNumEnqueuedSubgroups = 39,
    SpvBuiltInSubgroupId = 40,
    SpvBuiltInSubgroupLocalInvocationId = 41,
    SpvBuiltInVertexIndex = 42,
    SpvBuiltInInstanceIndex = 43,
    SpvBuiltInCoreIDARM = 4160,
    SpvBuiltInCoreCountARM = 4161,
    SpvBuiltInCoreMaxIDARM = 4162,
    SpvBuiltInWarpIDARM = 4163,
    SpvBuiltInWarpMaxIDARM = 4164,
    SpvBuiltInSubgroupEqMask = 4416,
    SpvBuiltInSubgroupEqMaskKHR = 4416,
    SpvBuiltInSubgroupGeMask = 4417,
    SpvBuiltInSubgroupGeMaskKHR = 4417,
    SpvBuiltInSubgroupGtMask = 4418,
    SpvBuiltInSubgroupGtMaskKHR = 4418,
    SpvBuiltInSubgroupLeMask = 4419,
    SpvBuiltInSubgroupLeMaskKHR = 4419,
    SpvBuiltInSubgroupLtMask = 4420,
    SpvBuiltInSubgroupLtMaskKHR = 4420,
    SpvBuiltInBaseVertex = 4424,
    SpvBuiltInBaseInstance = 4425,
    SpvBuiltInDrawIndex = 4426,
    SpvBuiltInPrimitiveShadingRateKHR = 4432,
    SpvBuiltInDeviceIndex = 4438,
    SpvBuiltInViewIndex = 4440,
    SpvBuiltInShadingRateKHR = 4444,
    SpvBuiltInBaryCoordNoPerspAMD = 4992,
    SpvBuiltInBaryCoordNoPerspCentroidAMD = 4993,
    SpvBuiltInBaryCoordNoPerspSampleAMD = 4994,
    SpvBuiltInBaryCoordSmoothAMD = 4995,
    SpvBuiltInBaryCoordSmoothCentroidAMD = 4996,
    SpvBuiltInBaryCoordSmoothSampleAMD = 4997,
    SpvBuiltInBaryCoordPullModelAMD = 4998,
    SpvBuiltInFragStencilRefEXT = 5014,
    SpvBuiltInViewportMaskNV = 5253,
    SpvBuiltInSecondaryPositionNV = 5257,
    SpvBuiltInSecondaryViewportMaskNV = 5258,
    SpvBuiltInPositionPerViewNV = 5261,
    SpvBuiltInViewportMaskPerViewNV = 5262,
    SpvBuiltInFullyCoveredEXT = 5264,
    SpvBuiltInTaskCountNV = 5274,
    SpvBuiltInPrimitiveCountNV = 5275,
    SpvBuiltInPrimitiveIndicesNV = 5276,
    SpvBuiltInClipDistancePerViewNV = 5277,
    SpvBuiltInCullDistancePerViewNV = 5278,
    SpvBuiltInLayerPerViewNV = 5279,
    SpvBuiltInMeshViewCountNV = 5280,
    SpvBuiltInMeshViewIndicesNV = 5281,
    SpvBuiltInBaryCoordKHR = 5286,
    SpvBuiltInBaryCoordNV = 5286,
    SpvBuiltInBaryCoordNoPerspKHR = 5287,
    SpvBuiltInBaryCoordNoPerspNV = 5287,
    SpvBuiltInFragSizeEXT = 5292,
    SpvBuiltInFragmentSizeNV = 5292,
    SpvBuiltInFragInvocationCountEXT = 5293,
    SpvBuiltInInvocationsPerPixelNV = 5293,
    SpvBuiltInPrimitivePointIndicesEXT = 5294,
    SpvBuiltInPrimitiveLineIndicesEXT = 5295,
    SpvBuiltInPrimitiveTriangleIndicesEXT = 5296,
    SpvBuiltInCullPrimitiveEXT = 5299,
    SpvBuiltInLaunchIdKHR = 5319,
    SpvBuiltInLaunchIdNV = 5319,
    SpvBuiltInLaunchSizeKHR = 5320,
    SpvBuiltInLaunchSizeNV = 5320,
    SpvBuiltInWorldRayOriginKHR = 5321,
    SpvBuiltInWorldRayOriginNV = 5321,
    SpvBuiltInWorldRayDirectionKHR = 5322,
    SpvBuiltInWorldRayDirectionNV = 5322,
    SpvBuiltInObjectRayOriginKHR = 5323,
    SpvBuiltInObjectRayOriginNV = 5323,
    SpvBuiltInObjectRayDirectionKHR = 5324,
    SpvBuiltInObjectRayDirectionNV = 5324,
    SpvBuiltInRayTminKHR = 5325,
    SpvBuiltInRayTminNV = 5325,
    SpvBuiltInRayTmaxKHR = 5326,
    SpvBuiltInRayTmaxNV = 5326,
    SpvBuiltInInstanceCustomIndexKHR = 5327,
    SpvBuiltInInstanceCustomIndexNV = 5327,
    SpvBuiltInObjectToWorldKHR = 5330,
    SpvBuiltInObjectToWorldNV = 5330,
    SpvBuiltInWorldToObjectKHR = 5331,
    SpvBuiltInWorldToObjectNV = 5331,
    SpvBuiltInHitTNV = 5332,
    SpvBuiltInHitKindKHR = 5333,
    SpvBuiltInHitKindNV = 5333,
    SpvBuiltInCurrentRayTimeNV = 5334,
    SpvBuiltInHitTriangleVertexPositionsKHR = 5335,
    SpvBuiltInIncomingRayFlagsKHR = 5351,
    SpvBuiltInIncomingRayFlagsNV = 5351,
    SpvBuiltInRayGeometryIndexKHR = 5352,
    SpvBuiltInWarpsPerSMNV = 5374,
    SpvBuiltInSMCountNV = 5375,
    SpvBuiltInWarpIDNV = 5376,
    SpvBuiltInSMIDNV = 5377,
    SpvBuiltInCullMaskKHR = 6021,
    SpvBuiltInMax = 0x7fffffff,
} SpvBuiltIn;

typedef enum SpvSelectionControlShift_ {
    SpvSelectionControlFlattenShift = 0,
    SpvSelectionControlDontFlattenShift = 1,
    SpvSelectionControlMax = 0x7fffffff,
} SpvSelectionControlShift;

typedef enum SpvSelectionControlMask_ {
    SpvSelectionControlMaskNone = 0,
    SpvSelectionControlFlattenMask = 0x00000001,
    SpvSelectionControlDontFlattenMask = 0x00000002,
} SpvSelectionControlMask;

typedef enum SpvLoopControlShift_ {
    SpvLoopControlUnrollShift = 0,
    SpvLoopControlDontUnrollShift = 1,
    SpvLoopControlDependencyInfiniteShift = 2,
    SpvLoopControlDependencyLengthShift = 3,
    SpvLoopControlMinIterationsShift = 4,
    SpvLoopControlMaxIterationsShift = 5,
    SpvLoopControlIterationMultipleShift = 6,
    SpvLoopControlPeelCountShift = 7,
    SpvLoopControlPartialCountShift = 8,
    SpvLoopControlInitiationIntervalINTELShift = 16,
    SpvLoopControlMaxConcurrencyINTELShift = 17,
    SpvLoopControlDependencyArrayINTELShift = 18,
    SpvLoopControlPipelineEnableINTELShift = 19,
    SpvLoopControlLoopCoalesceINTELShift = 20,
    SpvLoopControlMaxInterleavingINTELShift = 21,
    SpvLoopControlSpeculatedIterationsINTELShift = 22,
    SpvLoopControlNoFusionINTELShift = 23,
    SpvLoopControlLoopCountINTELShift = 24,
    SpvLoopControlMaxReinvocationDelayINTELShift = 25,
    SpvLoopControlMax = 0x7fffffff,
} SpvLoopControlShift;

typedef enum SpvLoopControlMask_ {
    SpvLoopControlMaskNone = 0,
    SpvLoopControlUnrollMask = 0x00000001,
    SpvLoopControlDontUnrollMask = 0x00000002,
    SpvLoopControlDependencyInfiniteMask = 0x00000004,
    SpvLoopControlDependencyLengthMask = 0x00000008,
    SpvLoopControlMinIterationsMask = 0x00000010,
    SpvLoopControlMaxIterationsMask = 0x00000020,
    SpvLoopControlIterationMultipleMask = 0x00000040,
    SpvLoopControlPeelCountMask = 0x00000080,
    SpvLoopControlPartialCountMask = 0x00000100,
    SpvLoopControlInitiationIntervalINTELMask = 0x00010000,
    SpvLoopControlMaxConcurrencyINTELMask = 0x00020000,
    SpvLoopControlDependencyArrayINTELMask = 0x00040000,
    SpvLoopControlPipelineEnableINTELMask = 0x00080000,
    SpvLoopControlLoopCoalesceINTELMask = 0x00100000,
    SpvLoopControlMaxInterleavingINTELMask = 0x00200000,
    SpvLoopControlSpeculatedIterationsINTELMask = 0x00400000,
    SpvLoopControlNoFusionINTELMask = 0x00800000,
    SpvLoopControlLoopCountINTELMask = 0x01000000,
    SpvLoopControlMaxReinvocationDelayINTELMask = 0x02000000,
} SpvLoopControlMask;

typedef enum SpvFunctionControlShift_ {
    SpvFunctionControlInlineShift = 0,
    SpvFunctionControlDontInlineShift = 1,
    SpvFunctionControlPureShift = 2,
    SpvFunctionControlConstShift = 3,
    SpvFunctionControlOptNoneINTELShift = 16,
    SpvFunctionControlMax = 0x7fffffff,
} SpvFunctionControlShift;

typedef enum SpvFunctionControlMask_ {
    SpvFunctionControlMaskNone = 0,
    SpvFunctionControlInlineMask = 0x00000001,
    SpvFunctionControlDontInlineMask = 0x00000002,
    SpvFunctionControlPureMask = 0x00000004,
    SpvFunctionControlConstMask = 0x00000008,
    SpvFunctionControlOptNoneINTELMask = 0x00010000,
} SpvFunctionControlMask;

typedef enum SpvMemorySemanticsShift_ {
    SpvMemorySemanticsAcquireShift = 1,
    SpvMemorySemanticsReleaseShift = 2,
    SpvMemorySemanticsAcquireReleaseShift = 3,
    SpvMemorySemanticsSequentiallyConsistentShift = 4,
    SpvMemorySemanticsUniformMemoryShift = 6,
    SpvMemorySemanticsSubgroupMemoryShift = 7,
    SpvMemorySemanticsWorkgroupMemoryShift = 8,
    SpvMemorySemanticsCrossWorkgroupMemoryShift = 9,
    SpvMemorySemanticsAtomicCounterMemoryShift = 10,
    SpvMemorySemanticsImageMemoryShift = 11,
    SpvMemorySemanticsOutputMemoryShift = 12,
    SpvMemorySemanticsOutputMemoryKHRShift = 12,
    SpvMemorySemanticsMakeAvailableShift = 13,
    SpvMemorySemanticsMakeAvailableKHRShift = 13,
    SpvMemorySemanticsMakeVisibleShift = 14,
    SpvMemorySemanticsMakeVisibleKHRShift = 14,
    SpvMemorySemanticsVolatileShift = 15,
    SpvMemorySemanticsMax = 0x7fffffff,
} SpvMemorySemanticsShift;

typedef enum SpvMemorySemanticsMask_ {
    SpvMemorySemanticsMaskNone = 0,
    SpvMemorySemanticsAcquireMask = 0x00000002,
    SpvMemorySemanticsReleaseMask = 0x00000004,
    SpvMemorySemanticsAcquireReleaseMask = 0x00000008,
    SpvMemorySemanticsSequentiallyConsistentMask = 0x00000010,
    SpvMemorySemanticsUniformMemoryMask = 0x00000040,
    SpvMemorySemanticsSubgroupMemoryMask = 0x00000080,
    SpvMemorySemanticsWorkgroupMemoryMask = 0x00000100,
    SpvMemorySemanticsCrossWorkgroupMemoryMask = 0x00000200,
    SpvMemorySemanticsAtomicCounterMemoryMask = 0x00000400,
    SpvMemorySemanticsImageMemoryMask = 0x00000800,
    SpvMemorySemanticsOutputMemoryMask = 0x00001000,
    SpvMemorySemanticsOutputMemoryKHRMask = 0x00001000,
    SpvMemorySemanticsMakeAvailableMask = 0x00002000,
    SpvMemorySemanticsMakeAvailableKHRMask = 0x00002000,
    SpvMemorySemanticsMakeVisibleMask = 0x00004000,
    SpvMemorySemanticsMakeVisibleKHRMask = 0x00004000,
    SpvMemorySemanticsVolatileMask = 0x00008000,
} SpvMemorySemanticsMask;

typedef enum SpvMemoryAccessShift_ {
    SpvMemoryAccessVolatileShift = 0,
    SpvMemoryAccessAlignedShift = 1,
    SpvMemoryAccessNontemporalShift = 2,
    SpvMemoryAccessMakePointerAvailableShift = 3,
    SpvMemoryAccessMakePointerAvailableKHRShift = 3,
    SpvMemoryAccessMakePointerVisibleShift = 4,
    SpvMemoryAccessMakePointerVisibleKHRShift = 4,
    SpvMemoryAccessNonPrivatePointerShift = 5,
    SpvMemoryAccessNonPrivatePointerKHRShift = 5,
    SpvMemoryAccessAliasScopeINTELMaskShift = 16,
    SpvMemoryAccessNoAliasINTELMaskShift = 17,
    SpvMemoryAccessMax = 0x7fffffff,
} SpvMemoryAccessShift;

typedef enum SpvMemoryAccessMask_ {
    SpvMemoryAccessMaskNone = 0,
    SpvMemoryAccessVolatileMask = 0x00000001,
    SpvMemoryAccessAlignedMask = 0x00000002,
    SpvMemoryAccessNontemporalMask = 0x00000004,
    SpvMemoryAccessMakePointerAvailableMask = 0x00000008,
    SpvMemoryAccessMakePointerAvailableKHRMask = 0x00000008,
    SpvMemoryAccessMakePointerVisibleMask = 0x00000010,
    SpvMemoryAccessMakePointerVisibleKHRMask = 0x00000010,
    SpvMemoryAccessNonPrivatePointerMask = 0x00000020,
    SpvMemoryAccessNonPrivatePointerKHRMask = 0x00000020,
    SpvMemoryAccessAliasScopeINTELMaskMask = 0x00010000,
    SpvMemoryAccessNoAliasINTELMaskMask = 0x00020000,
} SpvMemoryAccessMask;

typedef enum SpvScope_ {
    SpvScopeCrossDevice = 0,
    SpvScopeDevice = 1,
    SpvScopeWorkgroup = 2,
    SpvScopeSubgroup = 3,
    SpvScopeInvocation = 4,
    SpvScopeQueueFamily = 5,
    SpvScopeQueueFamilyKHR = 5,
    SpvScopeShaderCallKHR = 6,
    SpvScopeMax = 0x7fffffff,
} SpvScope;

typedef enum SpvGroupOperation_ {
    SpvGroupOperationReduce = 0,
    SpvGroupOperationInclusiveScan = 1,
    SpvGroupOperationExclusiveScan = 2,
    SpvGroupOperationClusteredReduce = 3,
    SpvGroupOperationPartitionedReduceNV = 6,
    SpvGroupOperationPartitionedInclusiveScanNV = 7,
    SpvGroupOperationPartitionedExclusiveScanNV = 8,
    SpvGroupOperationMax = 0x7fffffff,
} SpvGroupOperation;

typedef enum SpvKernelEnqueueFlags_ {
    SpvKernelEnqueueFlagsNoWait = 0,
    SpvKernelEnqueueFlagsWaitKernel = 1,
    SpvKernelEnqueueFlagsWaitWorkGroup = 2,
    SpvKernelEnqueueFlagsMax = 0x7fffffff,
} SpvKernelEnqueueFlags;

typedef enum SpvKernelProfilingInfoShift_ {
    SpvKernelProfilingInfoCmdExecTimeShift = 0,
    SpvKernelProfilingInfoMax = 0x7fffffff,
} SpvKernelProfilingInfoShift;

typedef enum SpvKernelProfilingInfoMask_ {
    SpvKernelProfilingInfoMaskNone = 0,
    SpvKernelProfilingInfoCmdExecTimeMask = 0x00000001,
} SpvKernelProfilingInfoMask;

typedef enum SpvCapability_ {
    SpvCapabilityMatrix = 0,
    SpvCapabilityShader = 1,
    SpvCapabilityGeometry = 2,
    SpvCapabilityTessellation = 3,
    SpvCapabilityAddresses = 4,
    SpvCapabilityLinkage = 5,
    SpvCapabilityKernel = 6,
    SpvCapabilityVector16 = 7,
    SpvCapabilityFloat16Buffer = 8,
    SpvCapabilityFloat16 = 9,
    SpvCapabilityFloat64 = 10,
    SpvCapabilityInt64 = 11,
    SpvCapabilityInt64Atomics = 12,
    SpvCapabilityImageBasic = 13,
    SpvCapabilityImageReadWrite = 14,
    SpvCapabilityImageMipmap = 15,
    SpvCapabilityPipes = 17,
    SpvCapabilityGroups = 18,
    SpvCapabilityDeviceEnqueue = 19,
    SpvCapabilityLiteralSampler = 20,
    SpvCapabilityAtomicStorage = 21,
    SpvCapabilityInt16 = 22,
    SpvCapabilityTessellationPointSize = 23,
    SpvCapabilityGeometryPointSize = 24,
    SpvCapabilityImageGatherExtended = 25,
    SpvCapabilityStorageImageMultisample = 27,
    SpvCapabilityUniformBufferArrayDynamicIndexing = 28,
    SpvCapabilitySampledImageArrayDynamicIndexing = 29,
    SpvCapabilityStorageBufferArrayDynamicIndexing = 30,
    SpvCapabilityStorageImageArrayDynamicIndexing = 31,
    SpvCapabilityClipDistance = 32,
    SpvCapabilityCullDistance = 33,
    SpvCapabilityImageCubeArray = 34,
    SpvCapabilitySampleRateShading = 35,
    SpvCapabilityImageRect = 36,
    SpvCapabilitySampledRect = 37,
    SpvCapabilityGenericPointer = 38,
    SpvCapabilityInt8 = 39,
    SpvCapabilityInputAttachment = 40,
    SpvCapabilitySparseResidency = 41,
    SpvCapabilityMinLod = 42,
    SpvCapabilitySampled1D = 43,
    SpvCapabilityImage1D = 44,
    SpvCapabilitySampledCubeArray = 45,
    SpvCapabilitySampledBuffer = 46,
    SpvCapabilityImageBuffer = 47,
    SpvCapabilityImageMSArray = 48,
    SpvCapabilityStorageImageExtendedFormats = 49,
    SpvCapabilityImageQuery = 50,
    SpvCapabilityDerivativeControl = 51,
    SpvCapabilityInterpolationFunction = 52,
    SpvCapabilityTransformFeedback = 53,
    SpvCapabilityGeometryStreams = 54,
    SpvCapabilityStorageImageReadWithoutFormat = 55,
    SpvCapabilityStorageImageWriteWithoutFormat = 56,
    SpvCapabilityMultiViewport = 57,
    SpvCapabilitySubgroupDispatch = 58,
    SpvCapabilityNamedBarrier = 59,
    SpvCapabilityPipeStorage = 60,
    SpvCapabilityGroupNonUniform = 61,
    SpvCapabilityGroupNonUniformVote = 62,
    SpvCapabilityGroupNonUniformArithmetic = 63,
    SpvCapabilityGroupNonUniformBallot = 64,
    SpvCapabilityGroupNonUniformShuffle = 65,
    SpvCapabilityGroupNonUniformShuffleRelative = 66,
    SpvCapabilityGroupNonUniformClustered = 67,
    SpvCapabilityGroupNonUniformQuad = 68,
    SpvCapabilityShaderLayer = 69,
    SpvCapabilityShaderViewportIndex = 70,
    SpvCapabilityUniformDecoration = 71,
    SpvCapabilityCoreBuiltinsARM = 4165,
    SpvCapabilityTileImageColorReadAccessEXT = 4166,
    SpvCapabilityTileImageDepthReadAccessEXT = 4167,
    SpvCapabilityTileImageStencilReadAccessEXT = 4168,
    SpvCapabilityFragmentShadingRateKHR = 4422,
    SpvCapabilitySubgroupBallotKHR = 4423,
    SpvCapabilityDrawParameters = 4427,
    SpvCapabilityWorkgroupMemoryExplicitLayoutKHR = 4428,
    SpvCapabilityWorkgroupMemoryExplicitLayout8BitAccessKHR = 4429,
    SpvCapabilityWorkgroupMemoryExplicitLayout16BitAccessKHR = 4430,
    SpvCapabilitySubgroupVoteKHR = 4431,
    SpvCapabilityStorageBuffer16BitAccess = 4433,
    SpvCapabilityStorageUniformBufferBlock16 = 4433,
    SpvCapabilityStorageUniform16 = 4434,
    SpvCapabilityUniformAndStorageBuffer16BitAccess = 4434,
    SpvCapabilityStoragePushConstant16 = 4435,
    SpvCapabilityStorageInputOutput16 = 4436,
    SpvCapabilityDeviceGroup = 4437,
    SpvCapabilityMultiView = 4439,
    SpvCapabilityVariablePointersStorageBuffer = 4441,
    SpvCapabilityVariablePointers = 4442,
    SpvCapabilityAtomicStorageOps = 4445,
    SpvCapabilitySampleMaskPostDepthCoverage = 4447,
    SpvCapabilityStorageBuffer8BitAccess = 4448,
    SpvCapabilityUniformAndStorageBuffer8BitAccess = 4449,
    SpvCapabilityStoragePushConstant8 = 4450,
    SpvCapabilityDenormPreserve = 4464,
    SpvCapabilityDenormFlushToZero = 4465,
    SpvCapabilitySignedZeroInfNanPreserve = 4466,
    SpvCapabilityRoundingModeRTE = 4467,
    SpvCapabilityRoundingModeRTZ = 4468,
    SpvCapabilityRayQueryProvisionalKHR = 4471,
    SpvCapabilityRayQueryKHR = 4472,
    SpvCapabilityRayTraversalPrimitiveCullingKHR = 4478,
    SpvCapabilityRayTracingKHR = 4479,
    SpvCapabilityTextureSampleWeightedQCOM = 4484,
    SpvCapabilityTextureBoxFilterQCOM = 4485,
    SpvCapabilityTextureBlockMatchQCOM = 4486,
    SpvCapabilityFloat16ImageAMD = 5008,
    SpvCapabilityImageGatherBiasLodAMD = 5009,
    SpvCapabilityFragmentMaskAMD = 5010,
    SpvCapabilityStencilExportEXT = 5013,
    SpvCapabilityImageReadWriteLodAMD = 5015,
    SpvCapabilityInt64ImageEXT = 5016,
    SpvCapabilityShaderClockKHR = 5055,
    SpvCapabilitySampleMaskOverrideCoverageNV = 5249,
    SpvCapabilityGeometryShaderPassthroughNV = 5251,
    SpvCapabilityShaderViewportIndexLayerEXT = 5254,
    SpvCapabilityShaderViewportIndexLayerNV = 5254,
    SpvCapabilityShaderViewportMaskNV = 5255,
    SpvCapabilityShaderStereoViewNV = 5259,
    SpvCapabilityPerViewAttributesNV = 5260,
    SpvCapabilityFragmentFullyCoveredEXT = 5265,
    SpvCapabilityMeshShadingNV = 5266,
    SpvCapabilityImageFootprintNV = 5282,
    SpvCapabilityMeshShadingEXT = 5283,
    SpvCapabilityFragmentBarycentricKHR = 5284,
    SpvCapabilityFragmentBarycentricNV = 5284,
    SpvCapabilityComputeDerivativeGroupQuadsNV = 5288,
    SpvCapabilityFragmentDensityEXT = 5291,
    SpvCapabilityShadingRateNV = 5291,
    SpvCapabilityGroupNonUniformPartitionedNV = 5297,
    SpvCapabilityShaderNonUniform = 5301,
    SpvCapabilityShaderNonUniformEXT = 5301,
    SpvCapabilityRuntimeDescriptorArray = 5302,
    SpvCapabilityRuntimeDescriptorArrayEXT = 5302,
    SpvCapabilityInputAttachmentArrayDynamicIndexing = 5303,
    SpvCapabilityInputAttachmentArrayDynamicIndexingEXT = 5303,
    SpvCapabilityUniformTexelBufferArrayDynamicIndexing = 5304,
    SpvCapabilityUniformTexelBufferArrayDynamicIndexingEXT = 5304,
    SpvCapabilityStorageTexelBufferArrayDynamicIndexing = 5305,
    SpvCapabilityStorageTexelBufferArrayDynamicIndexingEXT = 5305,
    SpvCapabilityUniformBufferArrayNonUniformIndexing = 5306,
    SpvCapabilityUniformBufferArrayNonUniformIndexingEXT = 5306,
    SpvCapabilitySampledImageArrayNonUniformIndexing = 5307,
    SpvCapabilitySampledImageArrayNonUniformIndexingEXT = 5307,
    SpvCapabilityStorageBufferArrayNonUniformIndexing = 5308,
    SpvCapabilityStorageBufferArrayNonUniformIndexingEXT = 5308,
    SpvCapabilityStorageImageArrayNonUniformIndexing = 5309,
    SpvCapabilityStorageImageArrayNonUniformIndexingEXT = 5309,
    SpvCapabilityInputAttachmentArrayNonUniformIndexing = 5310,
    SpvCapabilityInputAttachmentArrayNonUniformIndexingEXT = 5310,
    SpvCapabilityUniformTexelBufferArrayNonUniformIndexing = 5311,
    SpvCapabilityUniformTexelBufferArrayNonUniformIndexingEXT = 5311,
    SpvCapabilityStorageTexelBufferArrayNonUniformIndexing = 5312,
    SpvCapabilityStorageTexelBufferArrayNonUniformIndexingEXT = 5312,
    SpvCapabilityRayTracingPositionFetchKHR = 5336,
    SpvCapabilityRayTracingNV = 5340,
    SpvCapabilityRayTracingMotionBlurNV = 5341,
    SpvCapabilityVulkanMemoryModel = 5345,
    SpvCapabilityVulkanMemoryModelKHR = 5345,
    SpvCapabilityVulkanMemoryModelDeviceScope = 5346,
    SpvCapabilityVulkanMemoryModelDeviceScopeKHR = 5346,
    SpvCapabilityPhysicalStorageBufferAddresses = 5347,
    SpvCapabilityPhysicalStorageBufferAddressesEXT = 5347,
    SpvCapabilityComputeDerivativeGroupLinearNV = 5350,
    SpvCapabilityRayTracingProvisionalKHR = 5353,
    SpvCapabilityCooperativeMatrixNV = 5357,
    SpvCapabilityFragmentShaderSampleInterlockEXT = 5363,
    SpvCapabilityFragmentShaderShadingRateInterlockEXT = 5372,
    SpvCapabilityShaderSMBuiltinsNV = 5373,
    SpvCapabilityFragmentShaderPixelInterlockEXT = 5378,
    SpvCapabilityDemoteToHelperInvocation = 5379,
    SpvCapabilityDemoteToHelperInvocationEXT = 5379,
    SpvCapabilityRayTracingOpacityMicromapEXT = 5381,
    SpvCapabilityShaderInvocationReorderNV = 5383,
    SpvCapabilityBindlessTextureNV = 5390,
    SpvCapabilityRayQueryPositionFetchKHR = 5391,
    SpvCapabilitySubgroupShuffleINTEL = 5568,
    SpvCapabilitySubgroupBufferBlockIOINTEL = 5569,
    SpvCapabilitySubgroupImageBlockIOINTEL = 5570,
    SpvCapabilitySubgroupImageMediaBlockIOINTEL = 5579,
    SpvCapabilityRoundToInfinityINTEL = 5582,
    SpvCapabilityFloatingPointModeINTEL = 5583,
    SpvCapabilityIntegerFunctions2INTEL = 5584,
    SpvCapabilityFunctionPointersINTEL = 5603,
    SpvCapabilityIndirectReferencesINTEL = 5604,
    SpvCapabilityAsmINTEL = 5606,
    SpvCapabilityAtomicFloat32MinMaxEXT = 5612,
    SpvCapabilityAtomicFloat64MinMaxEXT = 5613,
    SpvCapabilityAtomicFloat16MinMaxEXT = 5616,
    SpvCapabilityVectorComputeINTEL = 5617,
    SpvCapabilityVectorAnyINTEL = 5619,
    SpvCapabilityExpectAssumeKHR = 5629,
    SpvCapabilitySubgroupAvcMotionEstimationINTEL = 5696,
    SpvCapabilitySubgroupAvcMotionEstimationIntraINTEL = 5697,
    SpvCapabilitySubgroupAvcMotionEstimationChromaINTEL = 5698,
    SpvCapabilityVariableLengthArrayINTEL = 5817,
    SpvCapabilityFunctionFloatControlINTEL = 5821,
    SpvCapabilityFPGAMemoryAttributesINTEL = 5824,
    SpvCapabilityFPFastMathModeINTEL = 5837,
    SpvCapabilityArbitraryPrecisionIntegersINTEL = 5844,
    SpvCapabilityArbitraryPrecisionFloatingPointINTEL = 5845,
    SpvCapabilityUnstructuredLoopControlsINTEL = 5886,
    SpvCapabilityFPGALoopControlsINTEL = 5888,
    SpvCapabilityKernelAttributesINTEL = 5892,
    SpvCapabilityFPGAKernelAttributesINTEL = 5897,
    SpvCapabilityFPGAMemoryAccessesINTEL = 5898,
    SpvCapabilityFPGAClusterAttributesINTEL = 5904,
    SpvCapabilityLoopFuseINTEL = 5906,
    SpvCapabilityFPGADSPControlINTEL = 5908,
    SpvCapabilityMemoryAccessAliasingINTEL = 5910,
    SpvCapabilityFPGAInvocationPipeliningAttributesINTEL = 5916,
    SpvCapabilityFPGABufferLocationINTEL = 5920,
    SpvCapabilityArbitraryPrecisionFixedPointINTEL = 5922,
    SpvCapabilityUSMStorageClassesINTEL = 5935,
    SpvCapabilityRuntimeAlignedAttributeINTEL = 5939,
    SpvCapabilityIOPipesINTEL = 5943,
    SpvCapabilityBlockingPipesINTEL = 5945,
    SpvCapabilityFPGARegINTEL = 5948,
    SpvCapabilityDotProductInputAll = 6016,
    SpvCapabilityDotProductInputAllKHR = 6016,
    SpvCapabilityDotProductInput4x8Bit = 6017,
    SpvCapabilityDotProductInput4x8BitKHR = 6017,
    SpvCapabilityDotProductInput4x8BitPacked = 6018,
    SpvCapabilityDotProductInput4x8BitPackedKHR = 6018,
    SpvCapabilityDotProduct = 6019,
    SpvCapabilityDotProductKHR = 6019,
    SpvCapabilityRayCullMaskKHR = 6020,
    SpvCapabilityCooperativeMatrixKHR = 6022,
    SpvCapabilityBitInstructions = 6025,
    SpvCapabilityGroupNonUniformRotateKHR = 6026,
    SpvCapabilityAtomicFloat32AddEXT = 6033,
    SpvCapabilityAtomicFloat64AddEXT = 6034,
    SpvCapabilityLongConstantCompositeINTEL = 6089,
    SpvCapabilityOptNoneINTEL = 6094,
    SpvCapabilityAtomicFloat16AddEXT = 6095,
    SpvCapabilityDebugInfoModuleINTEL = 6114,
    SpvCapabilityBFloat16ConversionINTEL = 6115,
    SpvCapabilitySplitBarrierINTEL = 6141,
    SpvCapabilityFPGAKernelAttributesv2INTEL = 6161,
    SpvCapabilityFPGALatencyControlINTEL = 6171,
    SpvCapabilityFPGAArgumentInterfacesINTEL = 6174,
    SpvCapabilityGroupUniformArithmeticKHR = 6400,
    SpvCapabilityMax = 0x7fffffff,
} SpvCapability;

typedef enum SpvRayFlagsShift_ {
    SpvRayFlagsOpaqueKHRShift = 0,
    SpvRayFlagsNoOpaqueKHRShift = 1,
    SpvRayFlagsTerminateOnFirstHitKHRShift = 2,
    SpvRayFlagsSkipClosestHitShaderKHRShift = 3,
    SpvRayFlagsCullBackFacingTrianglesKHRShift = 4,
    SpvRayFlagsCullFrontFacingTrianglesKHRShift = 5,
    SpvRayFlagsCullOpaqueKHRShift = 6,
    SpvRayFlagsCullNoOpaqueKHRShift = 7,
    SpvRayFlagsSkipTrianglesKHRShift = 8,
    SpvRayFlagsSkipAABBsKHRShift = 9,
    SpvRayFlagsForceOpacityMicromap2StateEXTShift = 10,
    SpvRayFlagsMax = 0x7fffffff,
} SpvRayFlagsShift;

typedef enum SpvRayFlagsMask_ {
    SpvRayFlagsMaskNone = 0,
    SpvRayFlagsOpaqueKHRMask = 0x00000001,
    SpvRayFlagsNoOpaqueKHRMask = 0x00000002,
    SpvRayFlagsTerminateOnFirstHitKHRMask = 0x00000004,
    SpvRayFlagsSkipClosestHitShaderKHRMask = 0x00000008,
    SpvRayFlagsCullBackFacingTrianglesKHRMask = 0x00000010,
    SpvRayFlagsCullFrontFacingTrianglesKHRMask = 0x00000020,
    SpvRayFlagsCullOpaqueKHRMask = 0x00000040,
    SpvRayFlagsCullNoOpaqueKHRMask = 0x00000080,
    SpvRayFlagsSkipTrianglesKHRMask = 0x00000100,
    SpvRayFlagsSkipAABBsKHRMask = 0x00000200,
    SpvRayFlagsForceOpacityMicromap2StateEXTMask = 0x00000400,
} SpvRayFlagsMask;

typedef enum SpvRayQueryIntersection_ {
    SpvRayQueryIntersectionRayQueryCandidateIntersectionKHR = 0,
    SpvRayQueryIntersectionRayQueryCommittedIntersectionKHR = 1,
    SpvRayQueryIntersectionMax = 0x7fffffff,
} SpvRayQueryIntersection;

typedef enum SpvRayQueryCommittedIntersectionType_ {
    SpvRayQueryCommittedIntersectionTypeRayQueryCommittedIntersectionNoneKHR = 0,
    SpvRayQueryCommittedIntersectionTypeRayQueryCommittedIntersectionTriangleKHR =
    1,
    SpvRayQueryCommittedIntersectionTypeRayQueryCommittedIntersectionGeneratedKHR =
    2,
    SpvRayQueryCommittedIntersectionTypeMax = 0x7fffffff,
} SpvRayQueryCommittedIntersectionType;

typedef enum SpvRayQueryCandidateIntersectionType_ {
    SpvRayQueryCandidateIntersectionTypeRayQueryCandidateIntersectionTriangleKHR =
    0,
    SpvRayQueryCandidateIntersectionTypeRayQueryCandidateIntersectionAABBKHR = 1,
    SpvRayQueryCandidateIntersectionTypeMax = 0x7fffffff,
} SpvRayQueryCandidateIntersectionType;

typedef enum SpvFragmentShadingRateShift_ {
    SpvFragmentShadingRateVertical2PixelsShift = 0,
    SpvFragmentShadingRateVertical4PixelsShift = 1,
    SpvFragmentShadingRateHorizontal2PixelsShift = 2,
    SpvFragmentShadingRateHorizontal4PixelsShift = 3,
    SpvFragmentShadingRateMax = 0x7fffffff,
} SpvFragmentShadingRateShift;

typedef enum SpvFragmentShadingRateMask_ {
    SpvFragmentShadingRateMaskNone = 0,
    SpvFragmentShadingRateVertical2PixelsMask = 0x00000001,
    SpvFragmentShadingRateVertical4PixelsMask = 0x00000002,
    SpvFragmentShadingRateHorizontal2PixelsMask = 0x00000004,
    SpvFragmentShadingRateHorizontal4PixelsMask = 0x00000008,
} SpvFragmentShadingRateMask;

typedef enum SpvFPDenormMode_ {
    SpvFPDenormModePreserve = 0,
    SpvFPDenormModeFlushToZero = 1,
    SpvFPDenormModeMax = 0x7fffffff,
} SpvFPDenormMode;

typedef enum SpvFPOperationMode_ {
    SpvFPOperationModeIEEE = 0,
    SpvFPOperationModeALT = 1,
    SpvFPOperationModeMax = 0x7fffffff,
} SpvFPOperationMode;

typedef enum SpvQuantizationModes_ {
    SpvQuantizationModesTRN = 0,
    SpvQuantizationModesTRN_ZERO = 1,
    SpvQuantizationModesRND = 2,
    SpvQuantizationModesRND_ZERO = 3,
    SpvQuantizationModesRND_INF = 4,
    SpvQuantizationModesRND_MIN_INF = 5,
    SpvQuantizationModesRND_CONV = 6,
    SpvQuantizationModesRND_CONV_ODD = 7,
    SpvQuantizationModesMax = 0x7fffffff,
} SpvQuantizationModes;

typedef enum SpvOverflowModes_ {
    SpvOverflowModesWRAP = 0,
    SpvOverflowModesSAT = 1,
    SpvOverflowModesSAT_ZERO = 2,
    SpvOverflowModesSAT_SYM = 3,
    SpvOverflowModesMax = 0x7fffffff,
} SpvOverflowModes;

typedef enum SpvPackedVectorFormat_ {
    SpvPackedVectorFormatPackedVectorFormat4x8Bit = 0,
    SpvPackedVectorFormatPackedVectorFormat4x8BitKHR = 0,
    SpvPackedVectorFormatMax = 0x7fffffff,
} SpvPackedVectorFormat;

typedef enum SpvCooperativeMatrixOperandsShift_ {
    SpvCooperativeMatrixOperandsMatrixASignedComponentsShift = 0,
    SpvCooperativeMatrixOperandsMatrixBSignedComponentsShift = 1,
    SpvCooperativeMatrixOperandsMatrixCSignedComponentsShift = 2,
    SpvCooperativeMatrixOperandsMatrixResultSignedComponentsShift = 3,
    SpvCooperativeMatrixOperandsSaturatingAccumulationShift = 4,
    SpvCooperativeMatrixOperandsMax = 0x7fffffff,
} SpvCooperativeMatrixOperandsShift;

typedef enum SpvCooperativeMatrixOperandsMask_ {
    SpvCooperativeMatrixOperandsMaskNone = 0,
    SpvCooperativeMatrixOperandsMatrixASignedComponentsMask = 0x00000001,
    SpvCooperativeMatrixOperandsMatrixBSignedComponentsMask = 0x00000002,
    SpvCooperativeMatrixOperandsMatrixCSignedComponentsMask = 0x00000004,
    SpvCooperativeMatrixOperandsMatrixResultSignedComponentsMask = 0x00000008,
    SpvCooperativeMatrixOperandsSaturatingAccumulationMask = 0x00000010,
} SpvCooperativeMatrixOperandsMask;

typedef enum SpvCooperativeMatrixLayout_ {
    SpvCooperativeMatrixLayoutRowMajorKHR = 0,
    SpvCooperativeMatrixLayoutColumnMajorKHR = 1,
    SpvCooperativeMatrixLayoutMax = 0x7fffffff,
} SpvCooperativeMatrixLayout;

typedef enum SpvCooperativeMatrixUse_ {
    SpvCooperativeMatrixUseMatrixAKHR = 0,
    SpvCooperativeMatrixUseMatrixBKHR = 1,
    SpvCooperativeMatrixUseMatrixAccumulatorKHR = 2,
    SpvCooperativeMatrixUseMax = 0x7fffffff,
} SpvCooperativeMatrixUse;

typedef enum SpvOp_ {
    SpvOpNop = 0,
    SpvOpUndef = 1,
    SpvOpSourceContinued = 2,
    SpvOpSource = 3,
    SpvOpSourceExtension = 4,
    SpvOpName = 5,
    SpvOpMemberName = 6,
    SpvOpString = 7,
    SpvOpLine = 8,
    SpvOpExtension = 10,
    SpvOpExtInstImport = 11,
    SpvOpExtInst = 12,
    SpvOpMemoryModel = 14,
    SpvOpEntryPoint = 15,
    SpvOpExecutionMode = 16,
    SpvOpCapability = 17,
    SpvOpTypeVoid = 19,
    SpvOpTypeBool = 20,
    SpvOpTypeInt = 21,
    SpvOpTypeFloat = 22,
    SpvOpTypeVector = 23,
    SpvOpTypeMatrix = 24,
    SpvOpTypeImage = 25,
    SpvOpTypeSampler = 26,
    SpvOpTypeSampledImage = 27,
    SpvOpTypeArray = 28,
    SpvOpTypeRuntimeArray = 29,
    SpvOpTypeStruct = 30,
    SpvOpTypeOpaque = 31,
    SpvOpTypePointer = 32,
    SpvOpTypeFunction = 33,
    SpvOpTypeEvent = 34,
    SpvOpTypeDeviceEvent = 35,
    SpvOpTypeReserveId = 36,
    SpvOpTypeQueue = 37,
    SpvOpTypePipe = 38,
    SpvOpTypeForwardPointer = 39,
    SpvOpConstantTrue = 41,
    SpvOpConstantFalse = 42,
    SpvOpConstant = 43,
    SpvOpConstantComposite = 44,
    SpvOpConstantSampler = 45,
    SpvOpConstantNull = 46,
    SpvOpSpecConstantTrue = 48,
    SpvOpSpecConstantFalse = 49,
    SpvOpSpecConstant = 50,
    SpvOpSpecConstantComposite = 51,
    SpvOpSpecConstantOp = 52,
    SpvOpFunction = 54,
    SpvOpFunctionParameter = 55,
    SpvOpFunctionEnd = 56,
    SpvOpFunctionCall = 57,
    SpvOpVariable = 59,
    SpvOpImageTexelPointer = 60,
    SpvOpLoad = 61,
    SpvOpStore = 62,
    SpvOpCopyMemory = 63,
    SpvOpCopyMemorySized = 64,
    SpvOpAccessChain = 65,
    SpvOpInBoundsAccessChain = 66,
    SpvOpPtrAccessChain = 67,
    SpvOpArrayLength = 68,
    SpvOpGenericPtrMemSemantics = 69,
    SpvOpInBoundsPtrAccessChain = 70,
    SpvOpDecorate = 71,
    SpvOpMemberDecorate = 72,
    SpvOpDecorationGroup = 73,
    SpvOpGroupDecorate = 74,
    SpvOpGroupMemberDecorate = 75,
    SpvOpVectorExtractDynamic = 77,
    SpvOpVectorInsertDynamic = 78,
    SpvOpVectorShuffle = 79,
    SpvOpCompositeConstruct = 80,
    SpvOpCompositeExtract = 81,
    SpvOpCompositeInsert = 82,
    SpvOpCopyObject = 83,
    SpvOpTranspose = 84,
    SpvOpSampledImage = 86,
    SpvOpImageSampleImplicitLod = 87,
    SpvOpImageSampleExplicitLod = 88,
    SpvOpImageSampleDrefImplicitLod = 89,
    SpvOpImageSampleDrefExplicitLod = 90,
    SpvOpImageSampleProjImplicitLod = 91,
    SpvOpImageSampleProjExplicitLod = 92,
    SpvOpImageSampleProjDrefImplicitLod = 93,
    SpvOpImageSampleProjDrefExplicitLod = 94,
    SpvOpImageFetch = 95,
    SpvOpImageGather = 96,
    SpvOpImageDrefGather = 97,
    SpvOpImageRead = 98,
    SpvOpImageWrite = 99,
    SpvOpImage = 100,
    SpvOpImageQueryFormat = 101,
    SpvOpImageQueryOrder = 102,
    SpvOpImageQuerySizeLod = 103,
    SpvOpImageQuerySize = 104,
    SpvOpImageQueryLod = 105,
    SpvOpImageQueryLevels = 106,
    SpvOpImageQuerySamples = 107,
    SpvOpConvertFToU = 109,
    SpvOpConvertFToS = 110,
    SpvOpConvertSToF = 111,
    SpvOpConvertUToF = 112,
    SpvOpUConvert = 113,
    SpvOpSConvert = 114,
    SpvOpFConvert = 115,
    SpvOpQuantizeToF16 = 116,
    SpvOpConvertPtrToU = 117,
    SpvOpSatConvertSToU = 118,
    SpvOpSatConvertUToS = 119,
    SpvOpConvertUToPtr = 120,
    SpvOpPtrCastToGeneric = 121,
    SpvOpGenericCastToPtr = 122,
    SpvOpGenericCastToPtrExplicit = 123,
    SpvOpBitcast = 124,
    SpvOpSNegate = 126,
    SpvOpFNegate = 127,
    SpvOpIAdd = 128,
    SpvOpFAdd = 129,
    SpvOpISub = 130,
    SpvOpFSub = 131,
    SpvOpIMul = 132,
    SpvOpFMul = 133,
    SpvOpUDiv = 134,
    SpvOpSDiv = 135,
    SpvOpFDiv = 136,
    SpvOpUMod = 137,
    SpvOpSRem = 138,
    SpvOpSMod = 139,
    SpvOpFRem = 140,
    SpvOpFMod = 141,
    SpvOpVectorTimesScalar = 142,
    SpvOpMatrixTimesScalar = 143,
    SpvOpVectorTimesMatrix = 144,
    SpvOpMatrixTimesVector = 145,
    SpvOpMatrixTimesMatrix = 146,
    SpvOpOuterProduct = 147,
    SpvOpDot = 148,
    SpvOpIAddCarry = 149,
    SpvOpISubBorrow = 150,
    SpvOpUMulExtended = 151,
    SpvOpSMulExtended = 152,
    SpvOpAny = 154,
    SpvOpAll = 155,
    SpvOpIsNan = 156,
    SpvOpIsInf = 157,
    SpvOpIsFinite = 158,
    SpvOpIsNormal = 159,
    SpvOpSignBitSet = 160,
    SpvOpLessOrGreater = 161,
    SpvOpOrdered = 162,
    SpvOpUnordered = 163,
    SpvOpLogicalEqual = 164,
    SpvOpLogicalNotEqual = 165,
    SpvOpLogicalOr = 166,
    SpvOpLogicalAnd = 167,
    SpvOpLogicalNot = 168,
    SpvOpSelect = 169,
    SpvOpIEqual = 170,
    SpvOpINotEqual = 171,
    SpvOpUGreaterThan = 172,
    SpvOpSGreaterThan = 173,
    SpvOpUGreaterThanEqual = 174,
    SpvOpSGreaterThanEqual = 175,
    SpvOpULessThan = 176,
    SpvOpSLessThan = 177,
    SpvOpULessThanEqual = 178,
    SpvOpSLessThanEqual = 179,
    SpvOpFOrdEqual = 180,
    SpvOpFUnordEqual = 181,
    SpvOpFOrdNotEqual = 182,
    SpvOpFUnordNotEqual = 183,
    SpvOpFOrdLessThan = 184,
    SpvOpFUnordLessThan = 185,
    SpvOpFOrdGreaterThan = 186,
    SpvOpFUnordGreaterThan = 187,
    SpvOpFOrdLessThanEqual = 188,
    SpvOpFUnordLessThanEqual = 189,
    SpvOpFOrdGreaterThanEqual = 190,
    SpvOpFUnordGreaterThanEqual = 191,
    SpvOpShiftRightLogical = 194,
    SpvOpShiftRightArithmetic = 195,
    SpvOpShiftLeftLogical = 196,
    SpvOpBitwiseOr = 197,
    SpvOpBitwiseXor = 198,
    SpvOpBitwiseAnd = 199,
    SpvOpNot = 200,
    SpvOpBitFieldInsert = 201,
    SpvOpBitFieldSExtract = 202,
    SpvOpBitFieldUExtract = 203,
    SpvOpBitReverse = 204,
    SpvOpBitCount = 205,
    SpvOpDPdx = 207,
    SpvOpDPdy = 208,
    SpvOpFwidth = 209,
    SpvOpDPdxFine = 210,
    SpvOpDPdyFine = 211,
    SpvOpFwidthFine = 212,
    SpvOpDPdxCoarse = 213,
    SpvOpDPdyCoarse = 214,
    SpvOpFwidthCoarse = 215,
    SpvOpEmitVertex = 218,
    SpvOpEndPrimitive = 219,
    SpvOpEmitStreamVertex = 220,
    SpvOpEndStreamPrimitive = 221,
    SpvOpControlBarrier = 224,
    SpvOpMemoryBarrier = 225,
    SpvOpAtomicLoad = 227,
    SpvOpAtomicStore = 228,
    SpvOpAtomicExchange = 229,
    SpvOpAtomicCompareExchange = 230,
    SpvOpAtomicCompareExchangeWeak = 231,
    SpvOpAtomicIIncrement = 232,
    SpvOpAtomicIDecrement = 233,
    SpvOpAtomicIAdd = 234,
    SpvOpAtomicISub = 235,
    SpvOpAtomicSMin = 236,
    SpvOpAtomicUMin = 237,
    SpvOpAtomicSMax = 238,
    SpvOpAtomicUMax = 239,
    SpvOpAtomicAnd = 240,
    SpvOpAtomicOr = 241,
    SpvOpAtomicXor = 242,
    SpvOpPhi = 245,
    SpvOpLoopMerge = 246,
    SpvOpSelectionMerge = 247,
    SpvOpLabel = 248,
    SpvOpBranch = 249,
    SpvOpBranchConditional = 250,
    SpvOpSwitch = 251,
    SpvOpKill = 252,
    SpvOpReturn = 253,
    SpvOpReturnValue = 254,
    SpvOpUnreachable = 255,
    SpvOpLifetimeStart = 256,
    SpvOpLifetimeStop = 257,
    SpvOpGroupAsyncCopy = 259,
    SpvOpGroupWaitEvents = 260,
    SpvOpGroupAll = 261,
    SpvOpGroupAny = 262,
    SpvOpGroupBroadcast = 263,
    SpvOpGroupIAdd = 264,
    SpvOpGroupFAdd = 265,
    SpvOpGroupFMin = 266,
    SpvOpGroupUMin = 267,
    SpvOpGroupSMin = 268,
    SpvOpGroupFMax = 269,
    SpvOpGroupUMax = 270,
    SpvOpGroupSMax = 271,
    SpvOpReadPipe = 274,
    SpvOpWritePipe = 275,
    SpvOpReservedReadPipe = 276,
    SpvOpReservedWritePipe = 277,
    SpvOpReserveReadPipePackets = 278,
    SpvOpReserveWritePipePackets = 279,
    SpvOpCommitReadPipe = 280,
    SpvOpCommitWritePipe = 281,
    SpvOpIsValidReserveId = 282,
    SpvOpGetNumPipePackets = 283,
    SpvOpGetMaxPipePackets = 284,
    SpvOpGroupReserveReadPipePackets = 285,
    SpvOpGroupReserveWritePipePackets = 286,
    SpvOpGroupCommitReadPipe = 287,
    SpvOpGroupCommitWritePipe = 288,
    SpvOpEnqueueMarker = 291,
    SpvOpEnqueueKernel = 292,
    SpvOpGetKernelNDrangeSubGroupCount = 293,
    SpvOpGetKernelNDrangeMaxSubGroupSize = 294,
    SpvOpGetKernelWorkGroupSize = 295,
    SpvOpGetKernelPreferredWorkGroupSizeMultiple = 296,
    SpvOpRetainEvent = 297,
    SpvOpReleaseEvent = 298,
    SpvOpCreateUserEvent = 299,
    SpvOpIsValidEvent = 300,
    SpvOpSetUserEventStatus = 301,
    SpvOpCaptureEventProfilingInfo = 302,
    SpvOpGetDefaultQueue = 303,
    SpvOpBuildNDRange = 304,
    SpvOpImageSparseSampleImplicitLod = 305,
    SpvOpImageSparseSampleExplicitLod = 306,
    SpvOpImageSparseSampleDrefImplicitLod = 307,
    SpvOpImageSparseSampleDrefExplicitLod = 308,
    SpvOpImageSparseSampleProjImplicitLod = 309,
    SpvOpImageSparseSampleProjExplicitLod = 310,
    SpvOpImageSparseSampleProjDrefImplicitLod = 311,
    SpvOpImageSparseSampleProjDrefExplicitLod = 312,
    SpvOpImageSparseFetch = 313,
    SpvOpImageSparseGather = 314,
    SpvOpImageSparseDrefGather = 315,
    SpvOpImageSparseTexelsResident = 316,
    SpvOpNoLine = 317,
    SpvOpAtomicFlagTestAndSet = 318,
    SpvOpAtomicFlagClear = 319,
    SpvOpImageSparseRead = 320,
    SpvOpSizeOf = 321,
    SpvOpTypePipeStorage = 322,
    SpvOpConstantPipeStorage = 323,
    SpvOpCreatePipeFromPipeStorage = 324,
    SpvOpGetKernelLocalSizeForSubgroupCount = 325,
    SpvOpGetKernelMaxNumSubgroups = 326,
    SpvOpTypeNamedBarrier = 327,
    SpvOpNamedBarrierInitialize = 328,
    SpvOpMemoryNamedBarrier = 329,
    SpvOpModuleProcessed = 330,
    SpvOpExecutionModeId = 331,
    SpvOpDecorateId = 332,
    SpvOpGroupNonUniformElect = 333,
    SpvOpGroupNonUniformAll = 334,
    SpvOpGroupNonUniformAny = 335,
    SpvOpGroupNonUniformAllEqual = 336,
    SpvOpGroupNonUniformBroadcast = 337,
    SpvOpGroupNonUniformBroadcastFirst = 338,
    SpvOpGroupNonUniformBallot = 339,
    SpvOpGroupNonUniformInverseBallot = 340,
    SpvOpGroupNonUniformBallotBitExtract = 341,
    SpvOpGroupNonUniformBallotBitCount = 342,
    SpvOpGroupNonUniformBallotFindLSB = 343,
    SpvOpGroupNonUniformBallotFindMSB = 344,
    SpvOpGroupNonUniformShuffle = 345,
    SpvOpGroupNonUniformShuffleXor = 346,
    SpvOpGroupNonUniformShuffleUp = 347,
    SpvOpGroupNonUniformShuffleDown = 348,
    SpvOpGroupNonUniformIAdd = 349,
    SpvOpGroupNonUniformFAdd = 350,
    SpvOpGroupNonUniformIMul = 351,
    SpvOpGroupNonUniformFMul = 352,
    SpvOpGroupNonUniformSMin = 353,
    SpvOpGroupNonUniformUMin = 354,
    SpvOpGroupNonUniformFMin = 355,
    SpvOpGroupNonUniformSMax = 356,
    SpvOpGroupNonUniformUMax = 357,
    SpvOpGroupNonUniformFMax = 358,
    SpvOpGroupNonUniformBitwiseAnd = 359,
    SpvOpGroupNonUniformBitwiseOr = 360,
    SpvOpGroupNonUniformBitwiseXor = 361,
    SpvOpGroupNonUniformLogicalAnd = 362,
    SpvOpGroupNonUniformLogicalOr = 363,
    SpvOpGroupNonUniformLogicalXor = 364,
    SpvOpGroupNonUniformQuadBroadcast = 365,
    SpvOpGroupNonUniformQuadSwap = 366,
    SpvOpCopyLogical = 400,
    SpvOpPtrEqual = 401,
    SpvOpPtrNotEqual = 402,
    SpvOpPtrDiff = 403,
    SpvOpColorAttachmentReadEXT = 4160,
    SpvOpDepthAttachmentReadEXT = 4161,
    SpvOpStencilAttachmentReadEXT = 4162,
    SpvOpTerminateInvocation = 4416,
    SpvOpSubgroupBallotKHR = 4421,
    SpvOpSubgroupFirstInvocationKHR = 4422,
    SpvOpSubgroupAllKHR = 4428,
    SpvOpSubgroupAnyKHR = 4429,
    SpvOpSubgroupAllEqualKHR = 4430,
    SpvOpGroupNonUniformRotateKHR = 4431,
    SpvOpSubgroupReadInvocationKHR = 4432,
    SpvOpTraceRayKHR = 4445,
    SpvOpExecuteCallableKHR = 4446,
    SpvOpConvertUToAccelerationStructureKHR = 4447,
    SpvOpIgnoreIntersectionKHR = 4448,
    SpvOpTerminateRayKHR = 4449,
    SpvOpSDot = 4450,
    SpvOpSDotKHR = 4450,
    SpvOpUDot = 4451,
    SpvOpUDotKHR = 4451,
    SpvOpSUDot = 4452,
    SpvOpSUDotKHR = 4452,
    SpvOpSDotAccSat = 4453,
    SpvOpSDotAccSatKHR = 4453,
    SpvOpUDotAccSat = 4454,
    SpvOpUDotAccSatKHR = 4454,
    SpvOpSUDotAccSat = 4455,
    SpvOpSUDotAccSatKHR = 4455,
    SpvOpTypeCooperativeMatrixKHR = 4456,
    SpvOpCooperativeMatrixLoadKHR = 4457,
    SpvOpCooperativeMatrixStoreKHR = 4458,
    SpvOpCooperativeMatrixMulAddKHR = 4459,
    SpvOpCooperativeMatrixLengthKHR = 4460,
    SpvOpTypeRayQueryKHR = 4472,
    SpvOpRayQueryInitializeKHR = 4473,
    SpvOpRayQueryTerminateKHR = 4474,
    SpvOpRayQueryGenerateIntersectionKHR = 4475,
    SpvOpRayQueryConfirmIntersectionKHR = 4476,
    SpvOpRayQueryProceedKHR = 4477,
    SpvOpRayQueryGetIntersectionTypeKHR = 4479,
    SpvOpImageSampleWeightedQCOM = 4480,
    SpvOpImageBoxFilterQCOM = 4481,
    SpvOpImageBlockMatchSSDQCOM = 4482,
    SpvOpImageBlockMatchSADQCOM = 4483,
    SpvOpGroupIAddNonUniformAMD = 5000,
    SpvOpGroupFAddNonUniformAMD = 5001,
    SpvOpGroupFMinNonUniformAMD = 5002,
    SpvOpGroupUMinNonUniformAMD = 5003,
    SpvOpGroupSMinNonUniformAMD = 5004,
    SpvOpGroupFMaxNonUniformAMD = 5005,
    SpvOpGroupUMaxNonUniformAMD = 5006,
    SpvOpGroupSMaxNonUniformAMD = 5007,
    SpvOpFragmentMaskFetchAMD = 5011,
    SpvOpFragmentFetchAMD = 5012,
    SpvOpReadClockKHR = 5056,
    SpvOpHitObjectRecordHitMotionNV = 5249,
    SpvOpHitObjectRecordHitWithIndexMotionNV = 5250,
    SpvOpHitObjectRecordMissMotionNV = 5251,
    SpvOpHitObjectGetWorldToObjectNV = 5252,
    SpvOpHitObjectGetObjectToWorldNV = 5253,
    SpvOpHitObjectGetObjectRayDirectionNV = 5254,
    SpvOpHitObjectGetObjectRayOriginNV = 5255,
    SpvOpHitObjectTraceRayMotionNV = 5256,
    SpvOpHitObjectGetShaderRecordBufferHandleNV = 5257,
    SpvOpHitObjectGetShaderBindingTableRecordIndexNV = 5258,
    SpvOpHitObjectRecordEmptyNV = 5259,
    SpvOpHitObjectTraceRayNV = 5260,
    SpvOpHitObjectRecordHitNV = 5261,
    SpvOpHitObjectRecordHitWithIndexNV = 5262,
    SpvOpHitObjectRecordMissNV = 5263,
    SpvOpHitObjectExecuteShaderNV = 5264,
    SpvOpHitObjectGetCurrentTimeNV = 5265,
    SpvOpHitObjectGetAttributesNV = 5266,
    SpvOpHitObjectGetHitKindNV = 5267,
    SpvOpHitObjectGetPrimitiveIndexNV = 5268,
    SpvOpHitObjectGetGeometryIndexNV = 5269,
    SpvOpHitObjectGetInstanceIdNV = 5270,
    SpvOpHitObjectGetInstanceCustomIndexNV = 5271,
    SpvOpHitObjectGetWorldRayDirectionNV = 5272,
    SpvOpHitObjectGetWorldRayOriginNV = 5273,
    SpvOpHitObjectGetRayTMaxNV = 5274,
    SpvOpHitObjectGetRayTMinNV = 5275,
    SpvOpHitObjectIsEmptyNV = 5276,
    SpvOpHitObjectIsHitNV = 5277,
    SpvOpHitObjectIsMissNV = 5278,
    SpvOpReorderThreadWithHitObjectNV = 5279,
    SpvOpReorderThreadWithHintNV = 5280,
    SpvOpTypeHitObjectNV = 5281,
    SpvOpImageSampleFootprintNV = 5283,
    SpvOpEmitMeshTasksEXT = 5294,
    SpvOpSetMeshOutputsEXT = 5295,
    SpvOpGroupNonUniformPartitionNV = 5296,
    SpvOpWritePackedPrimitiveIndices4x8NV = 5299,
    SpvOpReportIntersectionKHR = 5334,
    SpvOpReportIntersectionNV = 5334,
    SpvOpIgnoreIntersectionNV = 5335,
    SpvOpTerminateRayNV = 5336,
    SpvOpTraceNV = 5337,
    SpvOpTraceMotionNV = 5338,
    SpvOpTraceRayMotionNV = 5339,
    SpvOpRayQueryGetIntersectionTriangleVertexPositionsKHR = 5340,
    SpvOpTypeAccelerationStructureKHR = 5341,
    SpvOpTypeAccelerationStructureNV = 5341,
    SpvOpExecuteCallableNV = 5344,
    SpvOpTypeCooperativeMatrixNV = 5358,
    SpvOpCooperativeMatrixLoadNV = 5359,
    SpvOpCooperativeMatrixStoreNV = 5360,
    SpvOpCooperativeMatrixMulAddNV = 5361,
    SpvOpCooperativeMatrixLengthNV = 5362,
    SpvOpBeginInvocationInterlockEXT = 5364,
    SpvOpEndInvocationInterlockEXT = 5365,
    SpvOpDemoteToHelperInvocation = 5380,
    SpvOpDemoteToHelperInvocationEXT = 5380,
    SpvOpIsHelperInvocationEXT = 5381,
    SpvOpConvertUToImageNV = 5391,
    SpvOpConvertUToSamplerNV = 5392,
    SpvOpConvertImageToUNV = 5393,
    SpvOpConvertSamplerToUNV = 5394,
    SpvOpConvertUToSampledImageNV = 5395,
    SpvOpConvertSampledImageToUNV = 5396,
    SpvOpSamplerImageAddressingModeNV = 5397,
    SpvOpSubgroupShuffleINTEL = 5571,
    SpvOpSubgroupShuffleDownINTEL = 5572,
    SpvOpSubgroupShuffleUpINTEL = 5573,
    SpvOpSubgroupShuffleXorINTEL = 5574,
    SpvOpSubgroupBlockReadINTEL = 5575,
    SpvOpSubgroupBlockWriteINTEL = 5576,
    SpvOpSubgroupImageBlockReadINTEL = 5577,
    SpvOpSubgroupImageBlockWriteINTEL = 5578,
    SpvOpSubgroupImageMediaBlockReadINTEL = 5580,
    SpvOpSubgroupImageMediaBlockWriteINTEL = 5581,
    SpvOpUCountLeadingZerosINTEL = 5585,
    SpvOpUCountTrailingZerosINTEL = 5586,
    SpvOpAbsISubINTEL = 5587,
    SpvOpAbsUSubINTEL = 5588,
    SpvOpIAddSatINTEL = 5589,
    SpvOpUAddSatINTEL = 5590,
    SpvOpIAverageINTEL = 5591,
    SpvOpUAverageINTEL = 5592,
    SpvOpIAverageRoundedINTEL = 5593,
    SpvOpUAverageRoundedINTEL = 5594,
    SpvOpISubSatINTEL = 5595,
    SpvOpUSubSatINTEL = 5596,
    SpvOpIMul32x16INTEL = 5597,
    SpvOpUMul32x16INTEL = 5598,
    SpvOpConstantFunctionPointerINTEL = 5600,
    SpvOpFunctionPointerCallINTEL = 5601,
    SpvOpAsmTargetINTEL = 5609,
    SpvOpAsmINTEL = 5610,
    SpvOpAsmCallINTEL = 5611,
    SpvOpAtomicFMinEXT = 5614,
    SpvOpAtomicFMaxEXT = 5615,
    SpvOpAssumeTrueKHR = 5630,
    SpvOpExpectKHR = 5631,
    SpvOpDecorateString = 5632,
    SpvOpDecorateStringGOOGLE = 5632,
    SpvOpMemberDecorateString = 5633,
    SpvOpMemberDecorateStringGOOGLE = 5633,
    SpvOpVmeImageINTEL = 5699,
    SpvOpTypeVmeImageINTEL = 5700,
    SpvOpTypeAvcImePayloadINTEL = 5701,
    SpvOpTypeAvcRefPayloadINTEL = 5702,
    SpvOpTypeAvcSicPayloadINTEL = 5703,
    SpvOpTypeAvcMcePayloadINTEL = 5704,
    SpvOpTypeAvcMceResultINTEL = 5705,
    SpvOpTypeAvcImeResultINTEL = 5706,
    SpvOpTypeAvcImeResultSingleReferenceStreamoutINTEL = 5707,
    SpvOpTypeAvcImeResultDualReferenceStreamoutINTEL = 5708,
    SpvOpTypeAvcImeSingleReferenceStreaminINTEL = 5709,
    SpvOpTypeAvcImeDualReferenceStreaminINTEL = 5710,
    SpvOpTypeAvcRefResultINTEL = 5711,
    SpvOpTypeAvcSicResultINTEL = 5712,
    SpvOpSubgroupAvcMceGetDefaultInterBaseMultiReferencePenaltyINTEL = 5713,
    SpvOpSubgroupAvcMceSetInterBaseMultiReferencePenaltyINTEL = 5714,
    SpvOpSubgroupAvcMceGetDefaultInterShapePenaltyINTEL = 5715,
    SpvOpSubgroupAvcMceSetInterShapePenaltyINTEL = 5716,
    SpvOpSubgroupAvcMceGetDefaultInterDirectionPenaltyINTEL = 5717,
    SpvOpSubgroupAvcMceSetInterDirectionPenaltyINTEL = 5718,
    SpvOpSubgroupAvcMceGetDefaultIntraLumaShapePenaltyINTEL = 5719,
    SpvOpSubgroupAvcMceGetDefaultInterMotionVectorCostTableINTEL = 5720,
    SpvOpSubgroupAvcMceGetDefaultHighPenaltyCostTableINTEL = 5721,
    SpvOpSubgroupAvcMceGetDefaultMediumPenaltyCostTableINTEL = 5722,
    SpvOpSubgroupAvcMceGetDefaultLowPenaltyCostTableINTEL = 5723,
    SpvOpSubgroupAvcMceSetMotionVectorCostFunctionINTEL = 5724,
    SpvOpSubgroupAvcMceGetDefaultIntraLumaModePenaltyINTEL = 5725,
    SpvOpSubgroupAvcMceGetDefaultNonDcLumaIntraPenaltyINTEL = 5726,
    SpvOpSubgroupAvcMceGetDefaultIntraChromaModeBasePenaltyINTEL = 5727,
    SpvOpSubgroupAvcMceSetAcOnlyHaarINTEL = 5728,
    SpvOpSubgroupAvcMceSetSourceInterlacedFieldPolarityINTEL = 5729,
    SpvOpSubgroupAvcMceSetSingleReferenceInterlacedFieldPolarityINTEL = 5730,
    SpvOpSubgroupAvcMceSetDualReferenceInterlacedFieldPolaritiesINTEL = 5731,
    SpvOpSubgroupAvcMceConvertToImePayloadINTEL = 5732,
    SpvOpSubgroupAvcMceConvertToImeResultINTEL = 5733,
    SpvOpSubgroupAvcMceConvertToRefPayloadINTEL = 5734,
    SpvOpSubgroupAvcMceConvertToRefResultINTEL = 5735,
    SpvOpSubgroupAvcMceConvertToSicPayloadINTEL = 5736,
    SpvOpSubgroupAvcMceConvertToSicResultINTEL = 5737,
    SpvOpSubgroupAvcMceGetMotionVectorsINTEL = 5738,
    SpvOpSubgroupAvcMceGetInterDistortionsINTEL = 5739,
    SpvOpSubgroupAvcMceGetBestInterDistortionsINTEL = 5740,
    SpvOpSubgroupAvcMceGetInterMajorShapeINTEL = 5741,
    SpvOpSubgroupAvcMceGetInterMinorShapeINTEL = 5742,
    SpvOpSubgroupAvcMceGetInterDirectionsINTEL = 5743,
    SpvOpSubgroupAvcMceGetInterMotionVectorCountINTEL = 5744,
    SpvOpSubgroupAvcMceGetInterReferenceIdsINTEL = 5745,
    SpvOpSubgroupAvcMceGetInterReferenceInterlacedFieldPolaritiesINTEL = 5746,
    SpvOpSubgroupAvcImeInitializeINTEL = 5747,
    SpvOpSubgroupAvcImeSetSingleReferenceINTEL = 5748,
    SpvOpSubgroupAvcImeSetDualReferenceINTEL = 5749,
    SpvOpSubgroupAvcImeRefWindowSizeINTEL = 5750,
    SpvOpSubgroupAvcImeAdjustRefOffsetINTEL = 5751,
    SpvOpSubgroupAvcImeConvertToMcePayloadINTEL = 5752,
    SpvOpSubgroupAvcImeSetMaxMotionVectorCountINTEL = 5753,
    SpvOpSubgroupAvcImeSetUnidirectionalMixDisableINTEL = 5754,
    SpvOpSubgroupAvcImeSetEarlySearchTerminationThresholdINTEL = 5755,
    SpvOpSubgroupAvcImeSetWeightedSadINTEL = 5756,
    SpvOpSubgroupAvcImeEvaluateWithSingleReferenceINTEL = 5757,
    SpvOpSubgroupAvcImeEvaluateWithDualReferenceINTEL = 5758,
    SpvOpSubgroupAvcImeEvaluateWithSingleReferenceStreaminINTEL = 5759,
    SpvOpSubgroupAvcImeEvaluateWithDualReferenceStreaminINTEL = 5760,
    SpvOpSubgroupAvcImeEvaluateWithSingleReferenceStreamoutINTEL = 5761,
    SpvOpSubgroupAvcImeEvaluateWithDualReferenceStreamoutINTEL = 5762,
    SpvOpSubgroupAvcImeEvaluateWithSingleReferenceStreaminoutINTEL = 5763,
    SpvOpSubgroupAvcImeEvaluateWithDualReferenceStreaminoutINTEL = 5764,
    SpvOpSubgroupAvcImeConvertToMceResultINTEL = 5765,
    SpvOpSubgroupAvcImeGetSingleReferenceStreaminINTEL = 5766,
    SpvOpSubgroupAvcImeGetDualReferenceStreaminINTEL = 5767,
    SpvOpSubgroupAvcImeStripSingleReferenceStreamoutINTEL = 5768,
    SpvOpSubgroupAvcImeStripDualReferenceStreamoutINTEL = 5769,
    SpvOpSubgroupAvcImeGetStreamoutSingleReferenceMajorShapeMotionVectorsINTEL =
    5770,
    SpvOpSubgroupAvcImeGetStreamoutSingleReferenceMajorShapeDistortionsINTEL =
    5771,
    SpvOpSubgroupAvcImeGetStreamoutSingleReferenceMajorShapeReferenceIdsINTEL =
    5772,
    SpvOpSubgroupAvcImeGetStreamoutDualReferenceMajorShapeMotionVectorsINTEL =
    5773,
    SpvOpSubgroupAvcImeGetStreamoutDualReferenceMajorShapeDistortionsINTEL = 5774,
    SpvOpSubgroupAvcImeGetStreamoutDualReferenceMajorShapeReferenceIdsINTEL =
    5775,
    SpvOpSubgroupAvcImeGetBorderReachedINTEL = 5776,
    SpvOpSubgroupAvcImeGetTruncatedSearchIndicationINTEL = 5777,
    SpvOpSubgroupAvcImeGetUnidirectionalEarlySearchTerminationINTEL = 5778,
    SpvOpSubgroupAvcImeGetWeightingPatternMinimumMotionVectorINTEL = 5779,
    SpvOpSubgroupAvcImeGetWeightingPatternMinimumDistortionINTEL = 5780,
    SpvOpSubgroupAvcFmeInitializeINTEL = 5781,
    SpvOpSubgroupAvcBmeInitializeINTEL = 5782,
    SpvOpSubgroupAvcRefConvertToMcePayloadINTEL = 5783,
    SpvOpSubgroupAvcRefSetBidirectionalMixDisableINTEL = 5784,
    SpvOpSubgroupAvcRefSetBilinearFilterEnableINTEL = 5785,
    SpvOpSubgroupAvcRefEvaluateWithSingleReferenceINTEL = 5786,
    SpvOpSubgroupAvcRefEvaluateWithDualReferenceINTEL = 5787,
    SpvOpSubgroupAvcRefEvaluateWithMultiReferenceINTEL = 5788,
    SpvOpSubgroupAvcRefEvaluateWithMultiReferenceInterlacedINTEL = 5789,
    SpvOpSubgroupAvcRefConvertToMceResultINTEL = 5790,
    SpvOpSubgroupAvcSicInitializeINTEL = 5791,
    SpvOpSubgroupAvcSicConfigureSkcINTEL = 5792,
    SpvOpSubgroupAvcSicConfigureIpeLumaINTEL = 5793,
    SpvOpSubgroupAvcSicConfigureIpeLumaChromaINTEL = 5794,
    SpvOpSubgroupAvcSicGetMotionVectorMaskINTEL = 5795,
    SpvOpSubgroupAvcSicConvertToMcePayloadINTEL = 5796,
    SpvOpSubgroupAvcSicSetIntraLumaShapePenaltyINTEL = 5797,
    SpvOpSubgroupAvcSicSetIntraLumaModeCostFunctionINTEL = 5798,
    SpvOpSubgroupAvcSicSetIntraChromaModeCostFunctionINTEL = 5799,
    SpvOpSubgroupAvcSicSetBilinearFilterEnableINTEL = 5800,
    SpvOpSubgroupAvcSicSetSkcForwardTransformEnableINTEL = 5801,
    SpvOpSubgroupAvcSicSetBlockBasedRawSkipSadINTEL = 5802,
    SpvOpSubgroupAvcSicEvaluateIpeINTEL = 5803,
    SpvOpSubgroupAvcSicEvaluateWithSingleReferenceINTEL = 5804,
    SpvOpSubgroupAvcSicEvaluateWithDualReferenceINTEL = 5805,
    SpvOpSubgroupAvcSicEvaluateWithMultiReferenceINTEL = 5806,
    SpvOpSubgroupAvcSicEvaluateWithMultiReferenceInterlacedINTEL = 5807,
    SpvOpSubgroupAvcSicConvertToMceResultINTEL = 5808,
    SpvOpSubgroupAvcSicGetIpeLumaShapeINTEL = 5809,
    SpvOpSubgroupAvcSicGetBestIpeLumaDistortionINTEL = 5810,
    SpvOpSubgroupAvcSicGetBestIpeChromaDistortionINTEL = 5811,
    SpvOpSubgroupAvcSicGetPackedIpeLumaModesINTEL = 5812,
    SpvOpSubgroupAvcSicGetIpeChromaModeINTEL = 5813,
    SpvOpSubgroupAvcSicGetPackedSkcLumaCountThresholdINTEL = 5814,
    SpvOpSubgroupAvcSicGetPackedSkcLumaSumThresholdINTEL = 5815,
    SpvOpSubgroupAvcSicGetInterRawSadsINTEL = 5816,
    SpvOpVariableLengthArrayINTEL = 5818,
    SpvOpSaveMemoryINTEL = 5819,
    SpvOpRestoreMemoryINTEL = 5820,
    SpvOpArbitraryFloatSinCosPiINTEL = 5840,
    SpvOpArbitraryFloatCastINTEL = 5841,
    SpvOpArbitraryFloatCastFromIntINTEL = 5842,
    SpvOpArbitraryFloatCastToIntINTEL = 5843,
    SpvOpArbitraryFloatAddINTEL = 5846,
    SpvOpArbitraryFloatSubINTEL = 5847,
    SpvOpArbitraryFloatMulINTEL = 5848,
    SpvOpArbitraryFloatDivINTEL = 5849,
    SpvOpArbitraryFloatGTINTEL = 5850,
    SpvOpArbitraryFloatGEINTEL = 5851,
    SpvOpArbitraryFloatLTINTEL = 5852,
    SpvOpArbitraryFloatLEINTEL = 5853,
    SpvOpArbitraryFloatEQINTEL = 5854,
    SpvOpArbitraryFloatRecipINTEL = 5855,
    SpvOpArbitraryFloatRSqrtINTEL = 5856,
    SpvOpArbitraryFloatCbrtINTEL = 5857,
    SpvOpArbitraryFloatHypotINTEL = 5858,
    SpvOpArbitraryFloatSqrtINTEL = 5859,
    SpvOpArbitraryFloatLogINTEL = 5860,
    SpvOpArbitraryFloatLog2INTEL = 5861,
    SpvOpArbitraryFloatLog10INTEL = 5862,
    SpvOpArbitraryFloatLog1pINTEL = 5863,
    SpvOpArbitraryFloatExpINTEL = 5864,
    SpvOpArbitraryFloatExp2INTEL = 5865,
    SpvOpArbitraryFloatExp10INTEL = 5866,
    SpvOpArbitraryFloatExpm1INTEL = 5867,
    SpvOpArbitraryFloatSinINTEL = 5868,
    SpvOpArbitraryFloatCosINTEL = 5869,
    SpvOpArbitraryFloatSinCosINTEL = 5870,
    SpvOpArbitraryFloatSinPiINTEL = 5871,
    SpvOpArbitraryFloatCosPiINTEL = 5872,
    SpvOpArbitraryFloatASinINTEL = 5873,
    SpvOpArbitraryFloatASinPiINTEL = 5874,
    SpvOpArbitraryFloatACosINTEL = 5875,
    SpvOpArbitraryFloatACosPiINTEL = 5876,
    SpvOpArbitraryFloatATanINTEL = 5877,
    SpvOpArbitraryFloatATanPiINTEL = 5878,
    SpvOpArbitraryFloatATan2INTEL = 5879,
    SpvOpArbitraryFloatPowINTEL = 5880,
    SpvOpArbitraryFloatPowRINTEL = 5881,
    SpvOpArbitraryFloatPowNINTEL = 5882,
    SpvOpLoopControlINTEL = 5887,
    SpvOpAliasDomainDeclINTEL = 5911,
    SpvOpAliasScopeDeclINTEL = 5912,
    SpvOpAliasScopeListDeclINTEL = 5913,
    SpvOpFixedSqrtINTEL = 5923,
    SpvOpFixedRecipINTEL = 5924,
    SpvOpFixedRsqrtINTEL = 5925,
    SpvOpFixedSinINTEL = 5926,
    SpvOpFixedCosINTEL = 5927,
    SpvOpFixedSinCosINTEL = 5928,
    SpvOpFixedSinPiINTEL = 5929,
    SpvOpFixedCosPiINTEL = 5930,
    SpvOpFixedSinCosPiINTEL = 5931,
    SpvOpFixedLogINTEL = 5932,
    SpvOpFixedExpINTEL = 5933,
    SpvOpPtrCastToCrossWorkgroupINTEL = 5934,
    SpvOpCrossWorkgroupCastToPtrINTEL = 5938,
    SpvOpReadPipeBlockingINTEL = 5946,
    SpvOpWritePipeBlockingINTEL = 5947,
    SpvOpFPGARegINTEL = 5949,
    SpvOpRayQueryGetRayTMinKHR = 6016,
    SpvOpRayQueryGetRayFlagsKHR = 6017,
    SpvOpRayQueryGetIntersectionTKHR = 6018,
    SpvOpRayQueryGetIntersectionInstanceCustomIndexKHR = 6019,
    SpvOpRayQueryGetIntersectionInstanceIdKHR = 6020,
    SpvOpRayQueryGetIntersectionInstanceShaderBindingTableRecordOffsetKHR = 6021,
    SpvOpRayQueryGetIntersectionGeometryIndexKHR = 6022,
    SpvOpRayQueryGetIntersectionPrimitiveIndexKHR = 6023,
    SpvOpRayQueryGetIntersectionBarycentricsKHR = 6024,
    SpvOpRayQueryGetIntersectionFrontFaceKHR = 6025,
    SpvOpRayQueryGetIntersectionCandidateAABBOpaqueKHR = 6026,
    SpvOpRayQueryGetIntersectionObjectRayDirectionKHR = 6027,
    SpvOpRayQueryGetIntersectionObjectRayOriginKHR = 6028,
    SpvOpRayQueryGetWorldRayDirectionKHR = 6029,
    SpvOpRayQueryGetWorldRayOriginKHR = 6030,
    SpvOpRayQueryGetIntersectionObjectToWorldKHR = 6031,
    SpvOpRayQueryGetIntersectionWorldToObjectKHR = 6032,
    SpvOpAtomicFAddEXT = 6035,
    SpvOpTypeBufferSurfaceINTEL = 6086,
    SpvOpTypeStructContinuedINTEL = 6090,
    SpvOpConstantCompositeContinuedINTEL = 6091,
    SpvOpSpecConstantCompositeContinuedINTEL = 6092,
    SpvOpConvertFToBF16INTEL = 6116,
    SpvOpConvertBF16ToFINTEL = 6117,
    SpvOpControlBarrierArriveINTEL = 6142,
    SpvOpControlBarrierWaitINTEL = 6143,
    SpvOpGroupIMulKHR = 6401,
    SpvOpGroupFMulKHR = 6402,
    SpvOpGroupBitwiseAndKHR = 6403,
    SpvOpGroupBitwiseOrKHR = 6404,
    SpvOpGroupBitwiseXorKHR = 6405,
    SpvOpGroupLogicalAndKHR = 6406,
    SpvOpGroupLogicalOrKHR = 6407,
    SpvOpGroupLogicalXorKHR = 6408,
    SpvOpMax = 0x7fffffff,
} SpvOp;

#ifdef SPV_ENABLE_UTILITY_CODE
#ifndef __cplusplus
#include <stdbool.h>
#endif
inline void SpvHasResultAndType(SpvOp opcode, bool *hasResult,
    bool *hasResultType) {
    *hasResult = *hasResultType = false;
    switch (opcode) {
    default: /* unknown opcode */
        break;
    case SpvOpNop:
        *hasResult = false;
        *hasResultType = false;
        break;
    case SpvOpUndef:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpSourceContinued:
        *hasResult = false;
        *hasResultType = false;
        break;
    case SpvOpSource:
        *hasResult = false;
        *hasResultType = false;
        break;
    case SpvOpSourceExtension:
        *hasResult = false;
        *hasResultType = false;
        break;
    case SpvOpName:
        *hasResult = false;
        *hasResultType = false;
        break;
    case SpvOpMemberName:
        *hasResult = false;
        *hasResultType = false;
        break;
    case SpvOpString:
        *hasResult = true;
        *hasResultType = false;
        break;
    case SpvOpLine:
        *hasResult = false;
        *hasResultType = false;
        break;
    case SpvOpExtension:
        *hasResult = false;
        *hasResultType = false;
        break;
    case SpvOpExtInstImport:
        *hasResult = true;
        *hasResultType = false;
        break;
    case SpvOpExtInst:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpMemoryModel:
        *hasResult = false;
        *hasResultType = false;
        break;
    case SpvOpEntryPoint:
        *hasResult = false;
        *hasResultType = false;
        break;
    case SpvOpExecutionMode:
        *hasResult = false;
        *hasResultType = false;
        break;
    case SpvOpCapability:
        *hasResult = false;
        *hasResultType = false;
        break;
    case SpvOpTypeVoid:
        *hasResult = true;
        *hasResultType = false;
        break;
    case SpvOpTypeBool:
        *hasResult = true;
        *hasResultType = false;
        break;
    case SpvOpTypeInt:
        *hasResult = true;
        *hasResultType = false;
        break;
    case SpvOpTypeFloat:
        *hasResult = true;
        *hasResultType = false;
        break;
    case SpvOpTypeVector:
        *hasResult = true;
        *hasResultType = false;
        break;
    case SpvOpTypeMatrix:
        *hasResult = true;
        *hasResultType = false;
        break;
    case SpvOpTypeImage:
        *hasResult = true;
        *hasResultType = false;
        break;
    case SpvOpTypeSampler:
        *hasResult = true;
        *hasResultType = false;
        break;
    case SpvOpTypeSampledImage:
        *hasResult = true;
        *hasResultType = false;
        break;
    case SpvOpTypeArray:
        *hasResult = true;
        *hasResultType = false;
        break;
    case SpvOpTypeRuntimeArray:
        *hasResult = true;
        *hasResultType = false;
        break;
    case SpvOpTypeStruct:
        *hasResult = true;
        *hasResultType = false;
        break;
    case SpvOpTypeOpaque:
        *hasResult = true;
        *hasResultType = false;
        break;
    case SpvOpTypePointer:
        *hasResult = true;
        *hasResultType = false;
        break;
    case SpvOpTypeFunction:
        *hasResult = true;
        *hasResultType = false;
        break;
    case SpvOpTypeEvent:
        *hasResult = true;
        *hasResultType = false;
        break;
    case SpvOpTypeDeviceEvent:
        *hasResult = true;
        *hasResultType = false;
        break;
    case SpvOpTypeReserveId:
        *hasResult = true;
        *hasResultType = false;
        break;
    case SpvOpTypeQueue:
        *hasResult = true;
        *hasResultType = false;
        break;
    case SpvOpTypePipe:
        *hasResult = true;
        *hasResultType = false;
        break;
    case SpvOpTypeForwardPointer:
        *hasResult = false;
        *hasResultType = false;
        break;
    case SpvOpConstantTrue:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpConstantFalse:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpConstant:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpConstantComposite:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpConstantSampler:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpConstantNull:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpSpecConstantTrue:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpSpecConstantFalse:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpSpecConstant:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpSpecConstantComposite:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpSpecConstantOp:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpFunction:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpFunctionParameter:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpFunctionEnd:
        *hasResult = false;
        *hasResultType = false;
        break;
    case SpvOpFunctionCall:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpVariable:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpImageTexelPointer:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpLoad:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpStore:
        *hasResult = false;
        *hasResultType = false;
        break;
    case SpvOpCopyMemory:
        *hasResult = false;
        *hasResultType = false;
        break;
    case SpvOpCopyMemorySized:
        *hasResult = false;
        *hasResultType = false;
        break;
    case SpvOpAccessChain:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpInBoundsAccessChain:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpPtrAccessChain:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpArrayLength:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpGenericPtrMemSemantics:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpInBoundsPtrAccessChain:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpDecorate:
        *hasResult = false;
        *hasResultType = false;
        break;
    case SpvOpMemberDecorate:
        *hasResult = false;
        *hasResultType = false;
        break;
    case SpvOpDecorationGroup:
        *hasResult = true;
        *hasResultType = false;
        break;
    case SpvOpGroupDecorate:
        *hasResult = false;
        *hasResultType = false;
        break;
    case SpvOpGroupMemberDecorate:
        *hasResult = false;
        *hasResultType = false;
        break;
    case SpvOpVectorExtractDynamic:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpVectorInsertDynamic:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpVectorShuffle:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpCompositeConstruct:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpCompositeExtract:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpCompositeInsert:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpCopyObject:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpTranspose:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpSampledImage:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpImageSampleImplicitLod:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpImageSampleExplicitLod:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpImageSampleDrefImplicitLod:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpImageSampleDrefExplicitLod:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpImageSampleProjImplicitLod:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpImageSampleProjExplicitLod:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpImageSampleProjDrefImplicitLod:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpImageSampleProjDrefExplicitLod:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpImageFetch:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpImageGather:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpImageDrefGather:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpImageRead:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpImageWrite:
        *hasResult = false;
        *hasResultType = false;
        break;
    case SpvOpImage:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpImageQueryFormat:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpImageQueryOrder:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpImageQuerySizeLod:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpImageQuerySize:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpImageQueryLod:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpImageQueryLevels:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpImageQuerySamples:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpConvertFToU:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpConvertFToS:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpConvertSToF:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpConvertUToF:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpUConvert:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpSConvert:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpFConvert:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpQuantizeToF16:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpConvertPtrToU:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpSatConvertSToU:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpSatConvertUToS:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpConvertUToPtr:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpPtrCastToGeneric:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpGenericCastToPtr:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpGenericCastToPtrExplicit:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpBitcast:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpSNegate:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpFNegate:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpIAdd:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpFAdd:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpISub:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpFSub:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpIMul:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpFMul:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpUDiv:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpSDiv:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpFDiv:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpUMod:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpSRem:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpSMod:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpFRem:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpFMod:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpVectorTimesScalar:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpMatrixTimesScalar:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpVectorTimesMatrix:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpMatrixTimesVector:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpMatrixTimesMatrix:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpOuterProduct:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpDot:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpIAddCarry:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpISubBorrow:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpUMulExtended:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpSMulExtended:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpAny:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpAll:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpIsNan:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpIsInf:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpIsFinite:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpIsNormal:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpSignBitSet:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpLessOrGreater:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpOrdered:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpUnordered:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpLogicalEqual:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpLogicalNotEqual:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpLogicalOr:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpLogicalAnd:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpLogicalNot:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpSelect:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpIEqual:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpINotEqual:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpUGreaterThan:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpSGreaterThan:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpUGreaterThanEqual:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpSGreaterThanEqual:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpULessThan:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpSLessThan:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpULessThanEqual:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpSLessThanEqual:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpFOrdEqual:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpFUnordEqual:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpFOrdNotEqual:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpFUnordNotEqual:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpFOrdLessThan:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpFUnordLessThan:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpFOrdGreaterThan:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpFUnordGreaterThan:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpFOrdLessThanEqual:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpFUnordLessThanEqual:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpFOrdGreaterThanEqual:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpFUnordGreaterThanEqual:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpShiftRightLogical:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpShiftRightArithmetic:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpShiftLeftLogical:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpBitwiseOr:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpBitwiseXor:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpBitwiseAnd:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpNot:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpBitFieldInsert:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpBitFieldSExtract:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpBitFieldUExtract:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpBitReverse:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpBitCount:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpDPdx:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpDPdy:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpFwidth:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpDPdxFine:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpDPdyFine:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpFwidthFine:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpDPdxCoarse:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpDPdyCoarse:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpFwidthCoarse:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpEmitVertex:
        *hasResult = false;
        *hasResultType = false;
        break;
    case SpvOpEndPrimitive:
        *hasResult = false;
        *hasResultType = false;
        break;
    case SpvOpEmitStreamVertex:
        *hasResult = false;
        *hasResultType = false;
        break;
    case SpvOpEndStreamPrimitive:
        *hasResult = false;
        *hasResultType = false;
        break;
    case SpvOpControlBarrier:
        *hasResult = false;
        *hasResultType = false;
        break;
    case SpvOpMemoryBarrier:
        *hasResult = false;
        *hasResultType = false;
        break;
    case SpvOpAtomicLoad:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpAtomicStore:
        *hasResult = false;
        *hasResultType = false;
        break;
    case SpvOpAtomicExchange:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpAtomicCompareExchange:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpAtomicCompareExchangeWeak:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpAtomicIIncrement:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpAtomicIDecrement:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpAtomicIAdd:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpAtomicISub:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpAtomicSMin:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpAtomicUMin:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpAtomicSMax:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpAtomicUMax:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpAtomicAnd:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpAtomicOr:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpAtomicXor:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpPhi:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpLoopMerge:
        *hasResult = false;
        *hasResultType = false;
        break;
    case SpvOpSelectionMerge:
        *hasResult = false;
        *hasResultType = false;
        break;
    case SpvOpLabel:
        *hasResult = true;
        *hasResultType = false;
        break;
    case SpvOpBranch:
        *hasResult = false;
        *hasResultType = false;
        break;
    case SpvOpBranchConditional:
        *hasResult = false;
        *hasResultType = false;
        break;
    case SpvOpSwitch:
        *hasResult = false;
        *hasResultType = false;
        break;
    case SpvOpKill:
        *hasResult = false;
        *hasResultType = false;
        break;
    case SpvOpReturn:
        *hasResult = false;
        *hasResultType = false;
        break;
    case SpvOpReturnValue:
        *hasResult = false;
        *hasResultType = false;
        break;
    case SpvOpUnreachable:
        *hasResult = false;
        *hasResultType = false;
        break;
    case SpvOpLifetimeStart:
        *hasResult = false;
        *hasResultType = false;
        break;
    case SpvOpLifetimeStop:
        *hasResult = false;
        *hasResultType = false;
        break;
    case SpvOpGroupAsyncCopy:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpGroupWaitEvents:
        *hasResult = false;
        *hasResultType = false;
        break;
    case SpvOpGroupAll:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpGroupAny:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpGroupBroadcast:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpGroupIAdd:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpGroupFAdd:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpGroupFMin:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpGroupUMin:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpGroupSMin:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpGroupFMax:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpGroupUMax:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpGroupSMax:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpReadPipe:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpWritePipe:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpReservedReadPipe:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpReservedWritePipe:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpReserveReadPipePackets:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpReserveWritePipePackets:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpCommitReadPipe:
        *hasResult = false;
        *hasResultType = false;
        break;
    case SpvOpCommitWritePipe:
        *hasResult = false;
        *hasResultType = false;
        break;
    case SpvOpIsValidReserveId:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpGetNumPipePackets:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpGetMaxPipePackets:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpGroupReserveReadPipePackets:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpGroupReserveWritePipePackets:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpGroupCommitReadPipe:
        *hasResult = false;
        *hasResultType = false;
        break;
    case SpvOpGroupCommitWritePipe:
        *hasResult = false;
        *hasResultType = false;
        break;
    case SpvOpEnqueueMarker:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpEnqueueKernel:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpGetKernelNDrangeSubGroupCount:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpGetKernelNDrangeMaxSubGroupSize:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpGetKernelWorkGroupSize:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpGetKernelPreferredWorkGroupSizeMultiple:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpRetainEvent:
        *hasResult = false;
        *hasResultType = false;
        break;
    case SpvOpReleaseEvent:
        *hasResult = false;
        *hasResultType = false;
        break;
    case SpvOpCreateUserEvent:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpIsValidEvent:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpSetUserEventStatus:
        *hasResult = false;
        *hasResultType = false;
        break;
    case SpvOpCaptureEventProfilingInfo:
        *hasResult = false;
        *hasResultType = false;
        break;
    case SpvOpGetDefaultQueue:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpBuildNDRange:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpImageSparseSampleImplicitLod:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpImageSparseSampleExplicitLod:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpImageSparseSampleDrefImplicitLod:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpImageSparseSampleDrefExplicitLod:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpImageSparseSampleProjImplicitLod:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpImageSparseSampleProjExplicitLod:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpImageSparseSampleProjDrefImplicitLod:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpImageSparseSampleProjDrefExplicitLod:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpImageSparseFetch:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpImageSparseGather:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpImageSparseDrefGather:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpImageSparseTexelsResident:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpNoLine:
        *hasResult = false;
        *hasResultType = false;
        break;
    case SpvOpAtomicFlagTestAndSet:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpAtomicFlagClear:
        *hasResult = false;
        *hasResultType = false;
        break;
    case SpvOpImageSparseRead:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpSizeOf:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpTypePipeStorage:
        *hasResult = true;
        *hasResultType = false;
        break;
    case SpvOpConstantPipeStorage:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpCreatePipeFromPipeStorage:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpGetKernelLocalSizeForSubgroupCount:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpGetKernelMaxNumSubgroups:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpTypeNamedBarrier:
        *hasResult = true;
        *hasResultType = false;
        break;
    case SpvOpNamedBarrierInitialize:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpMemoryNamedBarrier:
        *hasResult = false;
        *hasResultType = false;
        break;
    case SpvOpModuleProcessed:
        *hasResult = false;
        *hasResultType = false;
        break;
    case SpvOpExecutionModeId:
        *hasResult = false;
        *hasResultType = false;
        break;
    case SpvOpDecorateId:
        *hasResult = false;
        *hasResultType = false;
        break;
    case SpvOpGroupNonUniformElect:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpGroupNonUniformAll:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpGroupNonUniformAny:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpGroupNonUniformAllEqual:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpGroupNonUniformBroadcast:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpGroupNonUniformBroadcastFirst:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpGroupNonUniformBallot:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpGroupNonUniformInverseBallot:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpGroupNonUniformBallotBitExtract:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpGroupNonUniformBallotBitCount:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpGroupNonUniformBallotFindLSB:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpGroupNonUniformBallotFindMSB:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpGroupNonUniformShuffle:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpGroupNonUniformShuffleXor:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpGroupNonUniformShuffleUp:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpGroupNonUniformShuffleDown:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpGroupNonUniformIAdd:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpGroupNonUniformFAdd:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpGroupNonUniformIMul:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpGroupNonUniformFMul:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpGroupNonUniformSMin:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpGroupNonUniformUMin:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpGroupNonUniformFMin:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpGroupNonUniformSMax:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpGroupNonUniformUMax:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpGroupNonUniformFMax:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpGroupNonUniformBitwiseAnd:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpGroupNonUniformBitwiseOr:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpGroupNonUniformBitwiseXor:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpGroupNonUniformLogicalAnd:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpGroupNonUniformLogicalOr:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpGroupNonUniformLogicalXor:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpGroupNonUniformQuadBroadcast:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpGroupNonUniformQuadSwap:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpCopyLogical:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpPtrEqual:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpPtrNotEqual:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpPtrDiff:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpColorAttachmentReadEXT:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpDepthAttachmentReadEXT:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpStencilAttachmentReadEXT:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpTerminateInvocation:
        *hasResult = false;
        *hasResultType = false;
        break;
    case SpvOpSubgroupBallotKHR:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpSubgroupFirstInvocationKHR:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpSubgroupAllKHR:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpSubgroupAnyKHR:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpSubgroupAllEqualKHR:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpGroupNonUniformRotateKHR:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpSubgroupReadInvocationKHR:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpTraceRayKHR:
        *hasResult = false;
        *hasResultType = false;
        break;
    case SpvOpExecuteCallableKHR:
        *hasResult = false;
        *hasResultType = false;
        break;
    case SpvOpConvertUToAccelerationStructureKHR:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpIgnoreIntersectionKHR:
        *hasResult = false;
        *hasResultType = false;
        break;
    case SpvOpTerminateRayKHR:
        *hasResult = false;
        *hasResultType = false;
        break;
    case SpvOpSDot:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpUDot:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpSUDot:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpSDotAccSat:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpUDotAccSat:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpSUDotAccSat:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpTypeCooperativeMatrixKHR:
        *hasResult = true;
        *hasResultType = false;
        break;
    case SpvOpCooperativeMatrixLoadKHR:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpCooperativeMatrixStoreKHR:
        *hasResult = false;
        *hasResultType = false;
        break;
    case SpvOpCooperativeMatrixMulAddKHR:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpCooperativeMatrixLengthKHR:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpTypeRayQueryKHR:
        *hasResult = true;
        *hasResultType = false;
        break;
    case SpvOpRayQueryInitializeKHR:
        *hasResult = false;
        *hasResultType = false;
        break;
    case SpvOpRayQueryTerminateKHR:
        *hasResult = false;
        *hasResultType = false;
        break;
    case SpvOpRayQueryGenerateIntersectionKHR:
        *hasResult = false;
        *hasResultType = false;
        break;
    case SpvOpRayQueryConfirmIntersectionKHR:
        *hasResult = false;
        *hasResultType = false;
        break;
    case SpvOpRayQueryProceedKHR:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpRayQueryGetIntersectionTypeKHR:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpImageSampleWeightedQCOM:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpImageBoxFilterQCOM:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpImageBlockMatchSSDQCOM:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpImageBlockMatchSADQCOM:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpGroupIAddNonUniformAMD:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpGroupFAddNonUniformAMD:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpGroupFMinNonUniformAMD:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpGroupUMinNonUniformAMD:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpGroupSMinNonUniformAMD:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpGroupFMaxNonUniformAMD:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpGroupUMaxNonUniformAMD:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpGroupSMaxNonUniformAMD:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpFragmentMaskFetchAMD:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpFragmentFetchAMD:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpReadClockKHR:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpHitObjectRecordHitMotionNV:
        *hasResult = false;
        *hasResultType = false;
        break;
    case SpvOpHitObjectRecordHitWithIndexMotionNV:
        *hasResult = false;
        *hasResultType = false;
        break;
    case SpvOpHitObjectRecordMissMotionNV:
        *hasResult = false;
        *hasResultType = false;
        break;
    case SpvOpHitObjectGetWorldToObjectNV:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpHitObjectGetObjectToWorldNV:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpHitObjectGetObjectRayDirectionNV:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpHitObjectGetObjectRayOriginNV:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpHitObjectTraceRayMotionNV:
        *hasResult = false;
        *hasResultType = false;
        break;
    case SpvOpHitObjectGetShaderRecordBufferHandleNV:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpHitObjectGetShaderBindingTableRecordIndexNV:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpHitObjectRecordEmptyNV:
        *hasResult = false;
        *hasResultType = false;
        break;
    case SpvOpHitObjectTraceRayNV:
        *hasResult = false;
        *hasResultType = false;
        break;
    case SpvOpHitObjectRecordHitNV:
        *hasResult = false;
        *hasResultType = false;
        break;
    case SpvOpHitObjectRecordHitWithIndexNV:
        *hasResult = false;
        *hasResultType = false;
        break;
    case SpvOpHitObjectRecordMissNV:
        *hasResult = false;
        *hasResultType = false;
        break;
    case SpvOpHitObjectExecuteShaderNV:
        *hasResult = false;
        *hasResultType = false;
        break;
    case SpvOpHitObjectGetCurrentTimeNV:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpHitObjectGetAttributesNV:
        *hasResult = false;
        *hasResultType = false;
        break;
    case SpvOpHitObjectGetHitKindNV:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpHitObjectGetPrimitiveIndexNV:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpHitObjectGetGeometryIndexNV:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpHitObjectGetInstanceIdNV:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpHitObjectGetInstanceCustomIndexNV:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpHitObjectGetWorldRayDirectionNV:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpHitObjectGetWorldRayOriginNV:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpHitObjectGetRayTMaxNV:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpHitObjectGetRayTMinNV:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpHitObjectIsEmptyNV:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpHitObjectIsHitNV:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpHitObjectIsMissNV:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpReorderThreadWithHitObjectNV:
        *hasResult = false;
        *hasResultType = false;
        break;
    case SpvOpReorderThreadWithHintNV:
        *hasResult = false;
        *hasResultType = false;
        break;
    case SpvOpTypeHitObjectNV:
        *hasResult = true;
        *hasResultType = false;
        break;
    case SpvOpImageSampleFootprintNV:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpEmitMeshTasksEXT:
        *hasResult = false;
        *hasResultType = false;
        break;
    case SpvOpSetMeshOutputsEXT:
        *hasResult = false;
        *hasResultType = false;
        break;
    case SpvOpGroupNonUniformPartitionNV:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpWritePackedPrimitiveIndices4x8NV:
        *hasResult = false;
        *hasResultType = false;
        break;
    case SpvOpReportIntersectionNV:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpIgnoreIntersectionNV:
        *hasResult = false;
        *hasResultType = false;
        break;
    case SpvOpTerminateRayNV:
        *hasResult = false;
        *hasResultType = false;
        break;
    case SpvOpTraceNV:
        *hasResult = false;
        *hasResultType = false;
        break;
    case SpvOpTraceMotionNV:
        *hasResult = false;
        *hasResultType = false;
        break;
    case SpvOpTraceRayMotionNV:
        *hasResult = false;
        *hasResultType = false;
        break;
    case SpvOpRayQueryGetIntersectionTriangleVertexPositionsKHR:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpTypeAccelerationStructureNV:
        *hasResult = true;
        *hasResultType = false;
        break;
    case SpvOpExecuteCallableNV:
        *hasResult = false;
        *hasResultType = false;
        break;
    case SpvOpTypeCooperativeMatrixNV:
        *hasResult = true;
        *hasResultType = false;
        break;
    case SpvOpCooperativeMatrixLoadNV:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpCooperativeMatrixStoreNV:
        *hasResult = false;
        *hasResultType = false;
        break;
    case SpvOpCooperativeMatrixMulAddNV:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpCooperativeMatrixLengthNV:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpBeginInvocationInterlockEXT:
        *hasResult = false;
        *hasResultType = false;
        break;
    case SpvOpEndInvocationInterlockEXT:
        *hasResult = false;
        *hasResultType = false;
        break;
    case SpvOpDemoteToHelperInvocation:
        *hasResult = false;
        *hasResultType = false;
        break;
    case SpvOpIsHelperInvocationEXT:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpConvertUToImageNV:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpConvertUToSamplerNV:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpConvertImageToUNV:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpConvertSamplerToUNV:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpConvertUToSampledImageNV:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpConvertSampledImageToUNV:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpSamplerImageAddressingModeNV:
        *hasResult = false;
        *hasResultType = false;
        break;
    case SpvOpSubgroupShuffleINTEL:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpSubgroupShuffleDownINTEL:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpSubgroupShuffleUpINTEL:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpSubgroupShuffleXorINTEL:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpSubgroupBlockReadINTEL:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpSubgroupBlockWriteINTEL:
        *hasResult = false;
        *hasResultType = false;
        break;
    case SpvOpSubgroupImageBlockReadINTEL:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpSubgroupImageBlockWriteINTEL:
        *hasResult = false;
        *hasResultType = false;
        break;
    case SpvOpSubgroupImageMediaBlockReadINTEL:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpSubgroupImageMediaBlockWriteINTEL:
        *hasResult = false;
        *hasResultType = false;
        break;
    case SpvOpUCountLeadingZerosINTEL:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpUCountTrailingZerosINTEL:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpAbsISubINTEL:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpAbsUSubINTEL:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpIAddSatINTEL:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpUAddSatINTEL:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpIAverageINTEL:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpUAverageINTEL:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpIAverageRoundedINTEL:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpUAverageRoundedINTEL:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpISubSatINTEL:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpUSubSatINTEL:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpIMul32x16INTEL:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpUMul32x16INTEL:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpConstantFunctionPointerINTEL:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpFunctionPointerCallINTEL:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpAsmTargetINTEL:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpAsmINTEL:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpAsmCallINTEL:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpAtomicFMinEXT:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpAtomicFMaxEXT:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpAssumeTrueKHR:
        *hasResult = false;
        *hasResultType = false;
        break;
    case SpvOpExpectKHR:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpDecorateString:
        *hasResult = false;
        *hasResultType = false;
        break;
    case SpvOpMemberDecorateString:
        *hasResult = false;
        *hasResultType = false;
        break;
    case SpvOpVmeImageINTEL:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpTypeVmeImageINTEL:
        *hasResult = true;
        *hasResultType = false;
        break;
    case SpvOpTypeAvcImePayloadINTEL:
        *hasResult = true;
        *hasResultType = false;
        break;
    case SpvOpTypeAvcRefPayloadINTEL:
        *hasResult = true;
        *hasResultType = false;
        break;
    case SpvOpTypeAvcSicPayloadINTEL:
        *hasResult = true;
        *hasResultType = false;
        break;
    case SpvOpTypeAvcMcePayloadINTEL:
        *hasResult = true;
        *hasResultType = false;
        break;
    case SpvOpTypeAvcMceResultINTEL:
        *hasResult = true;
        *hasResultType = false;
        break;
    case SpvOpTypeAvcImeResultINTEL:
        *hasResult = true;
        *hasResultType = false;
        break;
    case SpvOpTypeAvcImeResultSingleReferenceStreamoutINTEL:
        *hasResult = true;
        *hasResultType = false;
        break;
    case SpvOpTypeAvcImeResultDualReferenceStreamoutINTEL:
        *hasResult = true;
        *hasResultType = false;
        break;
    case SpvOpTypeAvcImeSingleReferenceStreaminINTEL:
        *hasResult = true;
        *hasResultType = false;
        break;
    case SpvOpTypeAvcImeDualReferenceStreaminINTEL:
        *hasResult = true;
        *hasResultType = false;
        break;
    case SpvOpTypeAvcRefResultINTEL:
        *hasResult = true;
        *hasResultType = false;
        break;
    case SpvOpTypeAvcSicResultINTEL:
        *hasResult = true;
        *hasResultType = false;
        break;
    case SpvOpSubgroupAvcMceGetDefaultInterBaseMultiReferencePenaltyINTEL:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpSubgroupAvcMceSetInterBaseMultiReferencePenaltyINTEL:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpSubgroupAvcMceGetDefaultInterShapePenaltyINTEL:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpSubgroupAvcMceSetInterShapePenaltyINTEL:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpSubgroupAvcMceGetDefaultInterDirectionPenaltyINTEL:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpSubgroupAvcMceSetInterDirectionPenaltyINTEL:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpSubgroupAvcMceGetDefaultIntraLumaShapePenaltyINTEL:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpSubgroupAvcMceGetDefaultInterMotionVectorCostTableINTEL:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpSubgroupAvcMceGetDefaultHighPenaltyCostTableINTEL:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpSubgroupAvcMceGetDefaultMediumPenaltyCostTableINTEL:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpSubgroupAvcMceGetDefaultLowPenaltyCostTableINTEL:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpSubgroupAvcMceSetMotionVectorCostFunctionINTEL:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpSubgroupAvcMceGetDefaultIntraLumaModePenaltyINTEL:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpSubgroupAvcMceGetDefaultNonDcLumaIntraPenaltyINTEL:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpSubgroupAvcMceGetDefaultIntraChromaModeBasePenaltyINTEL:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpSubgroupAvcMceSetAcOnlyHaarINTEL:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpSubgroupAvcMceSetSourceInterlacedFieldPolarityINTEL:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpSubgroupAvcMceSetSingleReferenceInterlacedFieldPolarityINTEL:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpSubgroupAvcMceSetDualReferenceInterlacedFieldPolaritiesINTEL:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpSubgroupAvcMceConvertToImePayloadINTEL:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpSubgroupAvcMceConvertToImeResultINTEL:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpSubgroupAvcMceConvertToRefPayloadINTEL:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpSubgroupAvcMceConvertToRefResultINTEL:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpSubgroupAvcMceConvertToSicPayloadINTEL:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpSubgroupAvcMceConvertToSicResultINTEL:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpSubgroupAvcMceGetMotionVectorsINTEL:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpSubgroupAvcMceGetInterDistortionsINTEL:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpSubgroupAvcMceGetBestInterDistortionsINTEL:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpSubgroupAvcMceGetInterMajorShapeINTEL:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpSubgroupAvcMceGetInterMinorShapeINTEL:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpSubgroupAvcMceGetInterDirectionsINTEL:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpSubgroupAvcMceGetInterMotionVectorCountINTEL:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpSubgroupAvcMceGetInterReferenceIdsINTEL:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpSubgroupAvcMceGetInterReferenceInterlacedFieldPolaritiesINTEL:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpSubgroupAvcImeInitializeINTEL:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpSubgroupAvcImeSetSingleReferenceINTEL:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpSubgroupAvcImeSetDualReferenceINTEL:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpSubgroupAvcImeRefWindowSizeINTEL:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpSubgroupAvcImeAdjustRefOffsetINTEL:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpSubgroupAvcImeConvertToMcePayloadINTEL:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpSubgroupAvcImeSetMaxMotionVectorCountINTEL:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpSubgroupAvcImeSetUnidirectionalMixDisableINTEL:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpSubgroupAvcImeSetEarlySearchTerminationThresholdINTEL:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpSubgroupAvcImeSetWeightedSadINTEL:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpSubgroupAvcImeEvaluateWithSingleReferenceINTEL:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpSubgroupAvcImeEvaluateWithDualReferenceINTEL:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpSubgroupAvcImeEvaluateWithSingleReferenceStreaminINTEL:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpSubgroupAvcImeEvaluateWithDualReferenceStreaminINTEL:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpSubgroupAvcImeEvaluateWithSingleReferenceStreamoutINTEL:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpSubgroupAvcImeEvaluateWithDualReferenceStreamoutINTEL:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpSubgroupAvcImeEvaluateWithSingleReferenceStreaminoutINTEL:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpSubgroupAvcImeEvaluateWithDualReferenceStreaminoutINTEL:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpSubgroupAvcImeConvertToMceResultINTEL:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpSubgroupAvcImeGetSingleReferenceStreaminINTEL:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpSubgroupAvcImeGetDualReferenceStreaminINTEL:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpSubgroupAvcImeStripSingleReferenceStreamoutINTEL:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpSubgroupAvcImeStripDualReferenceStreamoutINTEL:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpSubgroupAvcImeGetStreamoutSingleReferenceMajorShapeMotionVectorsINTEL:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpSubgroupAvcImeGetStreamoutSingleReferenceMajorShapeDistortionsINTEL:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpSubgroupAvcImeGetStreamoutSingleReferenceMajorShapeReferenceIdsINTEL:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpSubgroupAvcImeGetStreamoutDualReferenceMajorShapeMotionVectorsINTEL:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpSubgroupAvcImeGetStreamoutDualReferenceMajorShapeDistortionsINTEL:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpSubgroupAvcImeGetStreamoutDualReferenceMajorShapeReferenceIdsINTEL:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpSubgroupAvcImeGetBorderReachedINTEL:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpSubgroupAvcImeGetTruncatedSearchIndicationINTEL:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpSubgroupAvcImeGetUnidirectionalEarlySearchTerminationINTEL:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpSubgroupAvcImeGetWeightingPatternMinimumMotionVectorINTEL:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpSubgroupAvcImeGetWeightingPatternMinimumDistortionINTEL:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpSubgroupAvcFmeInitializeINTEL:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpSubgroupAvcBmeInitializeINTEL:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpSubgroupAvcRefConvertToMcePayloadINTEL:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpSubgroupAvcRefSetBidirectionalMixDisableINTEL:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpSubgroupAvcRefSetBilinearFilterEnableINTEL:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpSubgroupAvcRefEvaluateWithSingleReferenceINTEL:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpSubgroupAvcRefEvaluateWithDualReferenceINTEL:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpSubgroupAvcRefEvaluateWithMultiReferenceINTEL:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpSubgroupAvcRefEvaluateWithMultiReferenceInterlacedINTEL:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpSubgroupAvcRefConvertToMceResultINTEL:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpSubgroupAvcSicInitializeINTEL:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpSubgroupAvcSicConfigureSkcINTEL:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpSubgroupAvcSicConfigureIpeLumaINTEL:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpSubgroupAvcSicConfigureIpeLumaChromaINTEL:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpSubgroupAvcSicGetMotionVectorMaskINTEL:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpSubgroupAvcSicConvertToMcePayloadINTEL:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpSubgroupAvcSicSetIntraLumaShapePenaltyINTEL:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpSubgroupAvcSicSetIntraLumaModeCostFunctionINTEL:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpSubgroupAvcSicSetIntraChromaModeCostFunctionINTEL:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpSubgroupAvcSicSetBilinearFilterEnableINTEL:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpSubgroupAvcSicSetSkcForwardTransformEnableINTEL:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpSubgroupAvcSicSetBlockBasedRawSkipSadINTEL:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpSubgroupAvcSicEvaluateIpeINTEL:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpSubgroupAvcSicEvaluateWithSingleReferenceINTEL:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpSubgroupAvcSicEvaluateWithDualReferenceINTEL:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpSubgroupAvcSicEvaluateWithMultiReferenceINTEL:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpSubgroupAvcSicEvaluateWithMultiReferenceInterlacedINTEL:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpSubgroupAvcSicConvertToMceResultINTEL:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpSubgroupAvcSicGetIpeLumaShapeINTEL:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpSubgroupAvcSicGetBestIpeLumaDistortionINTEL:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpSubgroupAvcSicGetBestIpeChromaDistortionINTEL:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpSubgroupAvcSicGetPackedIpeLumaModesINTEL:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpSubgroupAvcSicGetIpeChromaModeINTEL:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpSubgroupAvcSicGetPackedSkcLumaCountThresholdINTEL:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpSubgroupAvcSicGetPackedSkcLumaSumThresholdINTEL:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpSubgroupAvcSicGetInterRawSadsINTEL:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpVariableLengthArrayINTEL:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpSaveMemoryINTEL:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpRestoreMemoryINTEL:
        *hasResult = false;
        *hasResultType = false;
        break;
    case SpvOpArbitraryFloatSinCosPiINTEL:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpArbitraryFloatCastINTEL:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpArbitraryFloatCastFromIntINTEL:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpArbitraryFloatCastToIntINTEL:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpArbitraryFloatAddINTEL:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpArbitraryFloatSubINTEL:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpArbitraryFloatMulINTEL:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpArbitraryFloatDivINTEL:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpArbitraryFloatGTINTEL:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpArbitraryFloatGEINTEL:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpArbitraryFloatLTINTEL:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpArbitraryFloatLEINTEL:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpArbitraryFloatEQINTEL:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpArbitraryFloatRecipINTEL:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpArbitraryFloatRSqrtINTEL:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpArbitraryFloatCbrtINTEL:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpArbitraryFloatHypotINTEL:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpArbitraryFloatSqrtINTEL:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpArbitraryFloatLogINTEL:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpArbitraryFloatLog2INTEL:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpArbitraryFloatLog10INTEL:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpArbitraryFloatLog1pINTEL:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpArbitraryFloatExpINTEL:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpArbitraryFloatExp2INTEL:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpArbitraryFloatExp10INTEL:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpArbitraryFloatExpm1INTEL:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpArbitraryFloatSinINTEL:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpArbitraryFloatCosINTEL:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpArbitraryFloatSinCosINTEL:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpArbitraryFloatSinPiINTEL:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpArbitraryFloatCosPiINTEL:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpArbitraryFloatASinINTEL:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpArbitraryFloatASinPiINTEL:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpArbitraryFloatACosINTEL:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpArbitraryFloatACosPiINTEL:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpArbitraryFloatATanINTEL:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpArbitraryFloatATanPiINTEL:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpArbitraryFloatATan2INTEL:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpArbitraryFloatPowINTEL:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpArbitraryFloatPowRINTEL:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpArbitraryFloatPowNINTEL:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpLoopControlINTEL:
        *hasResult = false;
        *hasResultType = false;
        break;
    case SpvOpAliasDomainDeclINTEL:
        *hasResult = true;
        *hasResultType = false;
        break;
    case SpvOpAliasScopeDeclINTEL:
        *hasResult = true;
        *hasResultType = false;
        break;
    case SpvOpAliasScopeListDeclINTEL:
        *hasResult = true;
        *hasResultType = false;
        break;
    case SpvOpFixedSqrtINTEL:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpFixedRecipINTEL:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpFixedRsqrtINTEL:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpFixedSinINTEL:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpFixedCosINTEL:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpFixedSinCosINTEL:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpFixedSinPiINTEL:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpFixedCosPiINTEL:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpFixedSinCosPiINTEL:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpFixedLogINTEL:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpFixedExpINTEL:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpPtrCastToCrossWorkgroupINTEL:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpCrossWorkgroupCastToPtrINTEL:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpReadPipeBlockingINTEL:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpWritePipeBlockingINTEL:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpFPGARegINTEL:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpRayQueryGetRayTMinKHR:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpRayQueryGetRayFlagsKHR:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpRayQueryGetIntersectionTKHR:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpRayQueryGetIntersectionInstanceCustomIndexKHR:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpRayQueryGetIntersectionInstanceIdKHR:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpRayQueryGetIntersectionInstanceShaderBindingTableRecordOffsetKHR:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpRayQueryGetIntersectionGeometryIndexKHR:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpRayQueryGetIntersectionPrimitiveIndexKHR:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpRayQueryGetIntersectionBarycentricsKHR:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpRayQueryGetIntersectionFrontFaceKHR:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpRayQueryGetIntersectionCandidateAABBOpaqueKHR:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpRayQueryGetIntersectionObjectRayDirectionKHR:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpRayQueryGetIntersectionObjectRayOriginKHR:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpRayQueryGetWorldRayDirectionKHR:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpRayQueryGetWorldRayOriginKHR:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpRayQueryGetIntersectionObjectToWorldKHR:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpRayQueryGetIntersectionWorldToObjectKHR:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpAtomicFAddEXT:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpTypeBufferSurfaceINTEL:
        *hasResult = true;
        *hasResultType = false;
        break;
    case SpvOpTypeStructContinuedINTEL:
        *hasResult = false;
        *hasResultType = false;
        break;
    case SpvOpConstantCompositeContinuedINTEL:
        *hasResult = false;
        *hasResultType = false;
        break;
    case SpvOpSpecConstantCompositeContinuedINTEL:
        *hasResult = false;
        *hasResultType = false;
        break;
    case SpvOpConvertFToBF16INTEL:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpConvertBF16ToFINTEL:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpControlBarrierArriveINTEL:
        *hasResult = false;
        *hasResultType = false;
        break;
    case SpvOpControlBarrierWaitINTEL:
        *hasResult = false;
        *hasResultType = false;
        break;
    case SpvOpGroupIMulKHR:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpGroupFMulKHR:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpGroupBitwiseAndKHR:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpGroupBitwiseOrKHR:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpGroupBitwiseXorKHR:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpGroupLogicalAndKHR:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpGroupLogicalOrKHR:
        *hasResult = true;
        *hasResultType = true;
        break;
    case SpvOpGroupLogicalXorKHR:
        *hasResult = true;
        *hasResultType = true;
        break;
    }
}
#endif /* SPV_ENABLE_UTILITY_CODE */

#endif


/*
 Copyright 2017-2022 Google Inc.

 Licensed under the Apache License, Version 2.0 (the "License");
 you may not use this file except in compliance with the License.
 You may obtain a copy of the License at

 http://www.apache.org/licenses/LICENSE-2.0

 Unless required by applicable law or agreed to in writing, software
 distributed under the License is distributed on an "AS IS" BASIS,
 WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 See the License for the specific language governing permissions and
 limitations under the License.
*/

/*

VERSION HISTORY

  1.0   (2018-03-27) Initial public release

*/

// clang-format off
/*!

 @file spirv_reflect.h

*/
#ifndef SPIRV_REFLECT_H
#define SPIRV_REFLECT_H

#include <stdint.h>
#include <string.h>

#ifdef _MSC_VER
#define SPV_REFLECT_DEPRECATED(msg_str) __declspec(deprecated("This symbol is deprecated. Details: " msg_str))
#elif defined(__clang__)
#define SPV_REFLECT_DEPRECATED(msg_str) __attribute__((deprecated(msg_str)))
#elif defined(__GNUC__)
#if GCC_VERSION >= 40500
#define SPV_REFLECT_DEPRECATED(msg_str) __attribute__((deprecated(msg_str)))
#else
#define SPV_REFLECT_DEPRECATED(msg_str) __attribute__((deprecated))
#endif
#else
#define SPV_REFLECT_DEPRECATED(msg_str)
#endif

/*! @enum SpvReflectResult

*/
typedef enum SpvReflectResult {
    SPV_REFLECT_RESULT_SUCCESS,
    SPV_REFLECT_RESULT_NOT_READY,
    SPV_REFLECT_RESULT_ERROR_PARSE_FAILED,
    SPV_REFLECT_RESULT_ERROR_ALLOC_FAILED,
    SPV_REFLECT_RESULT_ERROR_RANGE_EXCEEDED,
    SPV_REFLECT_RESULT_ERROR_NULL_POINTER,
    SPV_REFLECT_RESULT_ERROR_INTERNAL_ERROR,
    SPV_REFLECT_RESULT_ERROR_COUNT_MISMATCH,
    SPV_REFLECT_RESULT_ERROR_ELEMENT_NOT_FOUND,
    SPV_REFLECT_RESULT_ERROR_SPIRV_INVALID_CODE_SIZE,
    SPV_REFLECT_RESULT_ERROR_SPIRV_INVALID_MAGIC_NUMBER,
    SPV_REFLECT_RESULT_ERROR_SPIRV_UNEXPECTED_EOF,
    SPV_REFLECT_RESULT_ERROR_SPIRV_INVALID_ID_REFERENCE,
    SPV_REFLECT_RESULT_ERROR_SPIRV_SET_NUMBER_OVERFLOW,
    SPV_REFLECT_RESULT_ERROR_SPIRV_INVALID_STORAGE_CLASS,
    SPV_REFLECT_RESULT_ERROR_SPIRV_RECURSION,
    SPV_REFLECT_RESULT_ERROR_SPIRV_INVALID_INSTRUCTION,
    SPV_REFLECT_RESULT_ERROR_SPIRV_UNEXPECTED_BLOCK_DATA,
    SPV_REFLECT_RESULT_ERROR_SPIRV_INVALID_BLOCK_MEMBER_REFERENCE,
    SPV_REFLECT_RESULT_ERROR_SPIRV_INVALID_ENTRY_POINT,
    SPV_REFLECT_RESULT_ERROR_SPIRV_INVALID_EXECUTION_MODE,
    SPV_REFLECT_RESULT_ERROR_SPIRV_MAX_RECURSIVE_EXCEEDED,
} SpvReflectResult;

/*! @enum SpvReflectModuleFlagBits

SPV_REFLECT_MODULE_FLAG_NO_COPY - Disables copying of SPIR-V code
  when a SPIRV-Reflect shader module is created. It is the
  responsibility of the calling program to ensure that the pointer
  remains valid and the memory it's pointing to is not freed while
  SPIRV-Reflect operations are taking place. Freeing the backing
  memory will cause undefined behavior or most likely a crash.
  This is flag is intended for cases where the memory overhead of
  storing the copied SPIR-V is undesirable.

*/
typedef enum SpvReflectModuleFlagBits {
    SPV_REFLECT_MODULE_FLAG_NONE = 0x00000000,
    SPV_REFLECT_MODULE_FLAG_NO_COPY = 0x00000001,
} SpvReflectModuleFlagBits;

typedef uint32_t SpvReflectModuleFlags;

/*! @enum SpvReflectTypeFlagBits

*/
typedef enum SpvReflectTypeFlagBits {
    SPV_REFLECT_TYPE_FLAG_UNDEFINED = 0x00000000,
    SPV_REFLECT_TYPE_FLAG_VOID = 0x00000001,
    SPV_REFLECT_TYPE_FLAG_BOOL = 0x00000002,
    SPV_REFLECT_TYPE_FLAG_INT = 0x00000004,
    SPV_REFLECT_TYPE_FLAG_FLOAT = 0x00000008,
    SPV_REFLECT_TYPE_FLAG_VECTOR = 0x00000100,
    SPV_REFLECT_TYPE_FLAG_MATRIX = 0x00000200,
    SPV_REFLECT_TYPE_FLAG_EXTERNAL_IMAGE = 0x00010000,
    SPV_REFLECT_TYPE_FLAG_EXTERNAL_SAMPLER = 0x00020000,
    SPV_REFLECT_TYPE_FLAG_EXTERNAL_SAMPLED_IMAGE = 0x00040000,
    SPV_REFLECT_TYPE_FLAG_EXTERNAL_BLOCK = 0x00080000,
    SPV_REFLECT_TYPE_FLAG_EXTERNAL_ACCELERATION_STRUCTURE = 0x00100000,
    SPV_REFLECT_TYPE_FLAG_EXTERNAL_MASK = 0x00FF0000,
    SPV_REFLECT_TYPE_FLAG_STRUCT = 0x10000000,
    SPV_REFLECT_TYPE_FLAG_ARRAY = 0x20000000,
    SPV_REFLECT_TYPE_FLAG_REF = 0x40000000,
} SpvReflectTypeFlagBits;

typedef uint32_t SpvReflectTypeFlags;

/*! @enum SpvReflectDecorationBits

NOTE: HLSL row_major and column_major decorations are reversed
      in SPIR-V. Meaning that matrices declrations with row_major
      will get reflected as column_major and vice versa. The
      row and column decorations get appied during the compilation.
      SPIRV-Reflect reads the data as is and does not make any
      attempt to correct it to match what's in the source.

      The Patch, PerVertex, and PerTask are used for Interface
      variables that can have array

*/
typedef enum SpvReflectDecorationFlagBits {
    SPV_REFLECT_DECORATION_NONE = 0x00000000,
    SPV_REFLECT_DECORATION_BLOCK = 0x00000001,
    SPV_REFLECT_DECORATION_BUFFER_BLOCK = 0x00000002,
    SPV_REFLECT_DECORATION_ROW_MAJOR = 0x00000004,
    SPV_REFLECT_DECORATION_COLUMN_MAJOR = 0x00000008,
    SPV_REFLECT_DECORATION_BUILT_IN = 0x00000010,
    SPV_REFLECT_DECORATION_NOPERSPECTIVE = 0x00000020,
    SPV_REFLECT_DECORATION_FLAT = 0x00000040,
    SPV_REFLECT_DECORATION_NON_WRITABLE = 0x00000080,
    SPV_REFLECT_DECORATION_RELAXED_PRECISION = 0x00000100,
    SPV_REFLECT_DECORATION_NON_READABLE = 0x00000200,
    SPV_REFLECT_DECORATION_PATCH = 0x00000400,
    SPV_REFLECT_DECORATION_PER_VERTEX = 0x00000800,
    SPV_REFLECT_DECORATION_PER_TASK = 0x00001000,
    SPV_REFLECT_DECORATION_WEIGHT_TEXTURE = 0x00002000,
    SPV_REFLECT_DECORATION_BLOCK_MATCH_TEXTURE = 0x00004000,
} SpvReflectDecorationFlagBits;

typedef uint32_t SpvReflectDecorationFlags;

// Based of SPV_GOOGLE_user_type
typedef enum SpvReflectUserType {
    SPV_REFLECT_USER_TYPE_INVALID = 0,
    SPV_REFLECT_USER_TYPE_CBUFFER,
    SPV_REFLECT_USER_TYPE_TBUFFER,
    SPV_REFLECT_USER_TYPE_APPEND_STRUCTURED_BUFFER,
    SPV_REFLECT_USER_TYPE_BUFFER,
    SPV_REFLECT_USER_TYPE_BYTE_ADDRESS_BUFFER,
    SPV_REFLECT_USER_TYPE_CONSTANT_BUFFER,
    SPV_REFLECT_USER_TYPE_CONSUME_STRUCTURED_BUFFER,
    SPV_REFLECT_USER_TYPE_INPUT_PATCH,
    SPV_REFLECT_USER_TYPE_OUTPUT_PATCH,
    SPV_REFLECT_USER_TYPE_RASTERIZER_ORDERED_BUFFER,
    SPV_REFLECT_USER_TYPE_RASTERIZER_ORDERED_BYTE_ADDRESS_BUFFER,
    SPV_REFLECT_USER_TYPE_RASTERIZER_ORDERED_STRUCTURED_BUFFER,
    SPV_REFLECT_USER_TYPE_RASTERIZER_ORDERED_TEXTURE_1D,
    SPV_REFLECT_USER_TYPE_RASTERIZER_ORDERED_TEXTURE_1D_ARRAY,
    SPV_REFLECT_USER_TYPE_RASTERIZER_ORDERED_TEXTURE_2D,
    SPV_REFLECT_USER_TYPE_RASTERIZER_ORDERED_TEXTURE_2D_ARRAY,
    SPV_REFLECT_USER_TYPE_RASTERIZER_ORDERED_TEXTURE_3D,
    SPV_REFLECT_USER_TYPE_RAYTRACING_ACCELERATION_STRUCTURE,
    SPV_REFLECT_USER_TYPE_RW_BUFFER,
    SPV_REFLECT_USER_TYPE_RW_BYTE_ADDRESS_BUFFER,
    SPV_REFLECT_USER_TYPE_RW_STRUCTURED_BUFFER,
    SPV_REFLECT_USER_TYPE_RW_TEXTURE_1D,
    SPV_REFLECT_USER_TYPE_RW_TEXTURE_1D_ARRAY,
    SPV_REFLECT_USER_TYPE_RW_TEXTURE_2D,
    SPV_REFLECT_USER_TYPE_RW_TEXTURE_2D_ARRAY,
    SPV_REFLECT_USER_TYPE_RW_TEXTURE_3D,
    SPV_REFLECT_USER_TYPE_STRUCTURED_BUFFER,
    SPV_REFLECT_USER_TYPE_SUBPASS_INPUT,
    SPV_REFLECT_USER_TYPE_SUBPASS_INPUT_MS,
    SPV_REFLECT_USER_TYPE_TEXTURE_1D,
    SPV_REFLECT_USER_TYPE_TEXTURE_1D_ARRAY,
    SPV_REFLECT_USER_TYPE_TEXTURE_2D,
    SPV_REFLECT_USER_TYPE_TEXTURE_2D_ARRAY,
    SPV_REFLECT_USER_TYPE_TEXTURE_2DMS,
    SPV_REFLECT_USER_TYPE_TEXTURE_2DMS_ARRAY,
    SPV_REFLECT_USER_TYPE_TEXTURE_3D,
    SPV_REFLECT_USER_TYPE_TEXTURE_BUFFER,
    SPV_REFLECT_USER_TYPE_TEXTURE_CUBE,
    SPV_REFLECT_USER_TYPE_TEXTURE_CUBE_ARRAY,
} SpvReflectUserType;

/*! @enum SpvReflectResourceType

*/
typedef enum SpvReflectResourceType {
    SPV_REFLECT_RESOURCE_FLAG_UNDEFINED = 0x00000000,
    SPV_REFLECT_RESOURCE_FLAG_SAMPLER = 0x00000001,
    SPV_REFLECT_RESOURCE_FLAG_CBV = 0x00000002,
    SPV_REFLECT_RESOURCE_FLAG_SRV = 0x00000004,
    SPV_REFLECT_RESOURCE_FLAG_UAV = 0x00000008,
} SpvReflectResourceType;

/*! @enum SpvReflectFormat

*/
typedef enum SpvReflectFormat {
    SPV_REFLECT_FORMAT_UNDEFINED = 0, // = VK_FORMAT_UNDEFINED
    SPV_REFLECT_FORMAT_R16_UINT = 74, // = VK_FORMAT_R16_UINT
    SPV_REFLECT_FORMAT_R16_SINT = 75, // = VK_FORMAT_R16_SINT
    SPV_REFLECT_FORMAT_R16_SFLOAT = 76, // = VK_FORMAT_R16_SFLOAT
    SPV_REFLECT_FORMAT_R16G16_UINT = 81, // = VK_FORMAT_R16G16_UINT
    SPV_REFLECT_FORMAT_R16G16_SINT = 82, // = VK_FORMAT_R16G16_SINT
    SPV_REFLECT_FORMAT_R16G16_SFLOAT = 83, // = VK_FORMAT_R16G16_SFLOAT
    SPV_REFLECT_FORMAT_R16G16B16_UINT = 88, // = VK_FORMAT_R16G16B16_UINT
    SPV_REFLECT_FORMAT_R16G16B16_SINT = 89, // = VK_FORMAT_R16G16B16_SINT
    SPV_REFLECT_FORMAT_R16G16B16_SFLOAT = 90, // = VK_FORMAT_R16G16B16_SFLOAT
    SPV_REFLECT_FORMAT_R16G16B16A16_UINT = 95, // = VK_FORMAT_R16G16B16A16_UINT
    SPV_REFLECT_FORMAT_R16G16B16A16_SINT = 96, // = VK_FORMAT_R16G16B16A16_SINT
    SPV_REFLECT_FORMAT_R16G16B16A16_SFLOAT = 97, // = VK_FORMAT_R16G16B16A16_SFLOAT
    SPV_REFLECT_FORMAT_R32_UINT = 98, // = VK_FORMAT_R32_UINT
    SPV_REFLECT_FORMAT_R32_SINT = 99, // = VK_FORMAT_R32_SINT
    SPV_REFLECT_FORMAT_R32_SFLOAT = 100, // = VK_FORMAT_R32_SFLOAT
    SPV_REFLECT_FORMAT_R32G32_UINT = 101, // = VK_FORMAT_R32G32_UINT
    SPV_REFLECT_FORMAT_R32G32_SINT = 102, // = VK_FORMAT_R32G32_SINT
    SPV_REFLECT_FORMAT_R32G32_SFLOAT = 103, // = VK_FORMAT_R32G32_SFLOAT
    SPV_REFLECT_FORMAT_R32G32B32_UINT = 104, // = VK_FORMAT_R32G32B32_UINT
    SPV_REFLECT_FORMAT_R32G32B32_SINT = 105, // = VK_FORMAT_R32G32B32_SINT
    SPV_REFLECT_FORMAT_R32G32B32_SFLOAT = 106, // = VK_FORMAT_R32G32B32_SFLOAT
    SPV_REFLECT_FORMAT_R32G32B32A32_UINT = 107, // = VK_FORMAT_R32G32B32A32_UINT
    SPV_REFLECT_FORMAT_R32G32B32A32_SINT = 108, // = VK_FORMAT_R32G32B32A32_SINT
    SPV_REFLECT_FORMAT_R32G32B32A32_SFLOAT = 109, // = VK_FORMAT_R32G32B32A32_SFLOAT
    SPV_REFLECT_FORMAT_R64_UINT = 110, // = VK_FORMAT_R64_UINT
    SPV_REFLECT_FORMAT_R64_SINT = 111, // = VK_FORMAT_R64_SINT
    SPV_REFLECT_FORMAT_R64_SFLOAT = 112, // = VK_FORMAT_R64_SFLOAT
    SPV_REFLECT_FORMAT_R64G64_UINT = 113, // = VK_FORMAT_R64G64_UINT
    SPV_REFLECT_FORMAT_R64G64_SINT = 114, // = VK_FORMAT_R64G64_SINT
    SPV_REFLECT_FORMAT_R64G64_SFLOAT = 115, // = VK_FORMAT_R64G64_SFLOAT
    SPV_REFLECT_FORMAT_R64G64B64_UINT = 116, // = VK_FORMAT_R64G64B64_UINT
    SPV_REFLECT_FORMAT_R64G64B64_SINT = 117, // = VK_FORMAT_R64G64B64_SINT
    SPV_REFLECT_FORMAT_R64G64B64_SFLOAT = 118, // = VK_FORMAT_R64G64B64_SFLOAT
    SPV_REFLECT_FORMAT_R64G64B64A64_UINT = 119, // = VK_FORMAT_R64G64B64A64_UINT
    SPV_REFLECT_FORMAT_R64G64B64A64_SINT = 120, // = VK_FORMAT_R64G64B64A64_SINT
    SPV_REFLECT_FORMAT_R64G64B64A64_SFLOAT = 121, // = VK_FORMAT_R64G64B64A64_SFLOAT
} SpvReflectFormat;

/*! @enum SpvReflectVariableFlagBits

*/
enum SpvReflectVariableFlagBits {
    SPV_REFLECT_VARIABLE_FLAGS_NONE = 0x00000000,
    SPV_REFLECT_VARIABLE_FLAGS_UNUSED = 0x00000001,
    // If variable points to a copy of the PhysicalStorageBuffer struct
    SPV_REFLECT_VARIABLE_FLAGS_PHYSICAL_POINTER_COPY = 0x00000002,
};

typedef uint32_t SpvReflectVariableFlags;

/*! @enum SpvReflectDescriptorType

*/
typedef enum SpvReflectDescriptorType {
    SPV_REFLECT_DESCRIPTOR_TYPE_SAMPLER = 0,        // = VK_DESCRIPTOR_TYPE_SAMPLER
    SPV_REFLECT_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER = 1,        // = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER
    SPV_REFLECT_DESCRIPTOR_TYPE_SAMPLED_IMAGE = 2,        // = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE
    SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_IMAGE = 3,        // = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE
    SPV_REFLECT_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER = 4,        // = VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER
    SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER = 5,        // = VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER
    SPV_REFLECT_DESCRIPTOR_TYPE_UNIFORM_BUFFER = 6,        // = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER
    SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_BUFFER = 7,        // = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER
    SPV_REFLECT_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC = 8,        // = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC
    SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC = 9,        // = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC
    SPV_REFLECT_DESCRIPTOR_TYPE_INPUT_ATTACHMENT = 10,        // = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT
    SPV_REFLECT_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR = 1000150000 // = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR
} SpvReflectDescriptorType;

/*! @enum SpvReflectShaderStageFlagBits

*/
typedef enum SpvReflectShaderStageFlagBits {
    SPV_REFLECT_SHADER_STAGE_VERTEX_BIT = 0x00000001, // = VK_SHADER_STAGE_VERTEX_BIT
    SPV_REFLECT_SHADER_STAGE_TESSELLATION_CONTROL_BIT = 0x00000002, // = VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT
    SPV_REFLECT_SHADER_STAGE_TESSELLATION_EVALUATION_BIT = 0x00000004, // = VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT
    SPV_REFLECT_SHADER_STAGE_GEOMETRY_BIT = 0x00000008, // = VK_SHADER_STAGE_GEOMETRY_BIT
    SPV_REFLECT_SHADER_STAGE_FRAGMENT_BIT = 0x00000010, // = VK_SHADER_STAGE_FRAGMENT_BIT
    SPV_REFLECT_SHADER_STAGE_COMPUTE_BIT = 0x00000020, // = VK_SHADER_STAGE_COMPUTE_BIT
    SPV_REFLECT_SHADER_STAGE_TASK_BIT_NV = 0x00000040, // = VK_SHADER_STAGE_TASK_BIT_NV
    SPV_REFLECT_SHADER_STAGE_TASK_BIT_EXT = SPV_REFLECT_SHADER_STAGE_TASK_BIT_NV, // = VK_SHADER_STAGE_CALLABLE_BIT_EXT
    SPV_REFLECT_SHADER_STAGE_MESH_BIT_NV = 0x00000080, // = VK_SHADER_STAGE_MESH_BIT_NV
    SPV_REFLECT_SHADER_STAGE_MESH_BIT_EXT = SPV_REFLECT_SHADER_STAGE_MESH_BIT_NV, // = VK_SHADER_STAGE_CALLABLE_BIT_EXT
    SPV_REFLECT_SHADER_STAGE_RAYGEN_BIT_KHR = 0x00000100, // = VK_SHADER_STAGE_RAYGEN_BIT_KHR
    SPV_REFLECT_SHADER_STAGE_ANY_HIT_BIT_KHR = 0x00000200, // = VK_SHADER_STAGE_ANY_HIT_BIT_KHR
    SPV_REFLECT_SHADER_STAGE_CLOSEST_HIT_BIT_KHR = 0x00000400, // = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR
    SPV_REFLECT_SHADER_STAGE_MISS_BIT_KHR = 0x00000800, // = VK_SHADER_STAGE_MISS_BIT_KHR
    SPV_REFLECT_SHADER_STAGE_INTERSECTION_BIT_KHR = 0x00001000, // = VK_SHADER_STAGE_INTERSECTION_BIT_KHR
    SPV_REFLECT_SHADER_STAGE_CALLABLE_BIT_KHR = 0x00002000, // = VK_SHADER_STAGE_CALLABLE_BIT_KHR

} SpvReflectShaderStageFlagBits;

/*! @enum SpvReflectGenerator

*/
typedef enum SpvReflectGenerator {
    SPV_REFLECT_GENERATOR_KHRONOS_LLVM_SPIRV_TRANSLATOR = 6,
    SPV_REFLECT_GENERATOR_KHRONOS_SPIRV_TOOLS_ASSEMBLER = 7,
    SPV_REFLECT_GENERATOR_KHRONOS_GLSLANG_REFERENCE_FRONT_END = 8,
    SPV_REFLECT_GENERATOR_GOOGLE_SHADERC_OVER_GLSLANG = 13,
    SPV_REFLECT_GENERATOR_GOOGLE_SPIREGG = 14,
    SPV_REFLECT_GENERATOR_GOOGLE_RSPIRV = 15,
    SPV_REFLECT_GENERATOR_X_LEGEND_MESA_MESAIR_SPIRV_TRANSLATOR = 16,
    SPV_REFLECT_GENERATOR_KHRONOS_SPIRV_TOOLS_LINKER = 17,
    SPV_REFLECT_GENERATOR_WINE_VKD3D_SHADER_COMPILER = 18,
    SPV_REFLECT_GENERATOR_CLAY_CLAY_SHADER_COMPILER = 19,
} SpvReflectGenerator;

enum {
    SPV_REFLECT_MAX_ARRAY_DIMS = 32,
    SPV_REFLECT_MAX_DESCRIPTOR_SETS = 64,
};

enum {
    SPV_REFLECT_BINDING_NUMBER_DONT_CHANGE = ~0,
    SPV_REFLECT_SET_NUMBER_DONT_CHANGE = ~0
};

typedef struct SpvReflectNumericTraits {
    struct Scalar {
        uint32_t                        width;
        uint32_t                        signedness;
    } scalar;

    struct Vector {
        uint32_t                        component_count;
    } vector;

    struct Matrix {
        uint32_t                        column_count;
        uint32_t                        row_count;
        uint32_t                        stride; // Measured in bytes
    } matrix;
} SpvReflectNumericTraits;

typedef struct SpvReflectImageTraits {
    SpvDim                            dim;
    uint32_t                          depth;
    uint32_t                          arrayed;
    uint32_t                          ms; // 0: single-sampled; 1: multisampled
    uint32_t                          sampled;
    SpvImageFormat                    image_format;
} SpvReflectImageTraits;

typedef enum SpvReflectArrayDimType {
    SPV_REFLECT_ARRAY_DIM_RUNTIME = 0,         // OpTypeRuntimeArray
} SpvReflectArrayDimType;

typedef struct SpvReflectArrayTraits {
    uint32_t                          dims_count;
    // Each entry is either:
    // - specialization constant dimension
    // - OpTypeRuntimeArray
    // - the array length otherwise
    uint32_t                          dims[SPV_REFLECT_MAX_ARRAY_DIMS];
    // Stores Ids for dimensions that are specialization constants
    uint32_t                          spec_constant_op_ids[SPV_REFLECT_MAX_ARRAY_DIMS];
    uint32_t                          stride; // Measured in bytes
} SpvReflectArrayTraits;

typedef struct SpvReflectBindingArrayTraits {
    uint32_t                          dims_count;
    uint32_t                          dims[SPV_REFLECT_MAX_ARRAY_DIMS];
} SpvReflectBindingArrayTraits;

/*! @struct SpvReflectTypeDescription
    @brief Information about an OpType* instruction
*/
typedef struct SpvReflectTypeDescription {
    uint32_t                          id;
    SpvOp                             op;
    const char *type_name;
    // Non-NULL if type is member of a struct
    const char *struct_member_name;

    // The storage class (SpvStorageClass) if the type, and -1 if it does not have a storage class.
    int                               storage_class;
    SpvReflectTypeFlags               type_flags;
    SpvReflectDecorationFlags         decoration_flags;

    struct Traits {
        SpvReflectNumericTraits         numeric;
        SpvReflectImageTraits           image;
        SpvReflectArrayTraits           array;
    } traits;

    // If underlying type is a struct (ex. array of structs)
    // this gives access to the OpTypeStruct
    struct SpvReflectTypeDescription *struct_type_description;

    // Some pointers to SpvReflectTypeDescription are really
    // just copies of another reference to the same OpType
    uint32_t                          copied;

    // @deprecated use struct_type_description instead
    uint32_t                          member_count;
    // @deprecated use struct_type_description instead
    struct SpvReflectTypeDescription *members;
} SpvReflectTypeDescription;


/*! @struct SpvReflectInterfaceVariable
    @brief The OpVariable that is either an Input or Output to the module
*/
typedef struct SpvReflectInterfaceVariable {
    uint32_t                            spirv_id;
    const char *name;
    uint32_t                            location;
    uint32_t                            component;
    SpvStorageClass                     storage_class;
    const char *semantic;
    SpvReflectDecorationFlags           decoration_flags;

    // The builtin id (SpvBuiltIn) if the variable is a builtin, and -1 otherwise.
    int                                 built_in;
    SpvReflectNumericTraits             numeric;
    SpvReflectArrayTraits               array;

    uint32_t                            member_count;
    struct SpvReflectInterfaceVariable *members;

    SpvReflectFormat                    format;

    // NOTE: SPIR-V shares type references for variables
    //       that have the same underlying type. This means
    //       that the same type name will appear for multiple
    //       variables.
    SpvReflectTypeDescription *type_description;

    struct {
        uint32_t                          location;
    } word_offset;
} SpvReflectInterfaceVariable;

/*! @struct SpvReflectBlockVariable

*/
typedef struct SpvReflectBlockVariable {
    uint32_t                          spirv_id;
    const char *name;
    // For Push Constants, this is the lowest offset of all memebers
    uint32_t                          offset;           // Measured in bytes
    uint32_t                          absolute_offset;  // Measured in bytes
    uint32_t                          size;             // Measured in bytes
    uint32_t                          padded_size;      // Measured in bytes
    SpvReflectDecorationFlags         decoration_flags;
    SpvReflectNumericTraits           numeric;
    SpvReflectArrayTraits             array;
    SpvReflectVariableFlags           flags;

    uint32_t                          member_count;
    struct SpvReflectBlockVariable *members;

    SpvReflectTypeDescription *type_description;

    struct {
        uint32_t                          offset;
    } word_offset;

} SpvReflectBlockVariable;

/*! @struct SpvReflectDescriptorBinding

*/
typedef struct SpvReflectDescriptorBinding {
    uint32_t                            spirv_id;
    const char *name;
    uint32_t                            binding;
    uint32_t                            input_attachment_index;
    uint32_t                            set;
    SpvReflectDescriptorType            descriptor_type;
    SpvReflectResourceType              resource_type;
    SpvReflectImageTraits               image;
    SpvReflectBlockVariable             block;
    SpvReflectBindingArrayTraits        array;
    uint32_t                            count;
    uint32_t                            accessed;
    uint32_t                            uav_counter_id;
    struct SpvReflectDescriptorBinding *uav_counter_binding;
    uint32_t                            byte_address_buffer_offset_count;
    uint32_t *byte_address_buffer_offsets;

    SpvReflectTypeDescription *type_description;

    struct {
        uint32_t                          binding;
        uint32_t                          set;
    } word_offset;

    SpvReflectDecorationFlags           decoration_flags;
    // Requires SPV_GOOGLE_user_type
    SpvReflectUserType                  user_type;
} SpvReflectDescriptorBinding;

/*! @struct SpvReflectDescriptorSet

*/
typedef struct SpvReflectDescriptorSet {
    uint32_t                          set;
    uint32_t                          binding_count;
    SpvReflectDescriptorBinding **bindings;
} SpvReflectDescriptorSet;

typedef enum SpvReflectExecutionModeValue {
    SPV_REFLECT_EXECUTION_MODE_SPEC_CONSTANT = 0xFFFFFFFF // specialization constant
} SpvReflectExecutionModeValue;

/*! @struct SpvReflectEntryPoint

 */
typedef struct SpvReflectEntryPoint {
    const char *name;
    uint32_t                          id;

    SpvExecutionModel                 spirv_execution_model;
    SpvReflectShaderStageFlagBits     shader_stage;

    uint32_t                          input_variable_count;
    SpvReflectInterfaceVariable **input_variables;
    uint32_t                          output_variable_count;
    SpvReflectInterfaceVariable **output_variables;
    uint32_t                          interface_variable_count;
    SpvReflectInterfaceVariable *interface_variables;

    uint32_t                          descriptor_set_count;
    SpvReflectDescriptorSet *descriptor_sets;

    uint32_t                          used_uniform_count;
    uint32_t *used_uniforms;
    uint32_t                          used_push_constant_count;
    uint32_t *used_push_constants;

    uint32_t                          execution_mode_count;
    SpvExecutionMode *execution_modes;

    struct LocalSize {
        uint32_t                        x;
        uint32_t                        y;
        uint32_t                        z;
    } local_size;
    uint32_t                          invocations; // valid for geometry
    uint32_t                          output_vertices; // valid for geometry, tesselation
} SpvReflectEntryPoint;

/*! @struct SpvReflectCapability

*/
typedef struct SpvReflectCapability {
    SpvCapability                     value;
    uint32_t                          word_offset;
} SpvReflectCapability;


/*! @struct SpvReflectSpecId

*/
typedef struct SpvReflectSpecializationConstant {
    uint32_t spirv_id;
    uint32_t constant_id;
    const char *name;
} SpvReflectSpecializationConstant;

/*! @struct SpvReflectShaderModule

*/
typedef struct SpvReflectShaderModule {
    SpvReflectGenerator               generator;
    const char *entry_point_name;
    uint32_t                          entry_point_id;
    uint32_t                          entry_point_count;
    SpvReflectEntryPoint *entry_points;
    SpvSourceLanguage                 source_language;
    uint32_t                          source_language_version;
    const char *source_file;
    const char *source_source;
    uint32_t                          capability_count;
    SpvReflectCapability *capabilities;
    SpvExecutionModel                 spirv_execution_model;                            // Uses value(s) from first entry point
    SpvReflectShaderStageFlagBits     shader_stage;                                     // Uses value(s) from first entry point
    uint32_t                          descriptor_binding_count;                         // Uses value(s) from first entry point
    SpvReflectDescriptorBinding *descriptor_bindings;                              // Uses value(s) from first entry point
    uint32_t                          descriptor_set_count;                             // Uses value(s) from first entry point
    SpvReflectDescriptorSet           descriptor_sets[SPV_REFLECT_MAX_DESCRIPTOR_SETS]; // Uses value(s) from first entry point
    uint32_t                          input_variable_count;                             // Uses value(s) from first entry point
    SpvReflectInterfaceVariable **input_variables;                                  // Uses value(s) from first entry point
    uint32_t                          output_variable_count;                            // Uses value(s) from first entry point
    SpvReflectInterfaceVariable **output_variables;                                 // Uses value(s) from first entry point
    uint32_t                          interface_variable_count;                         // Uses value(s) from first entry point
    SpvReflectInterfaceVariable *interface_variables;                              // Uses value(s) from first entry point
    uint32_t                          push_constant_block_count;                        // Uses value(s) from first entry point
    SpvReflectBlockVariable *push_constant_blocks;                             // Uses value(s) from first entry point
    uint32_t                          spec_constant_count;                              // Uses value(s) from first entry point
    SpvReflectSpecializationConstant *spec_constants;                                   // Uses value(s) from first entry point

    struct Internal {
        SpvReflectModuleFlags           module_flags;
        size_t                          spirv_size;
        uint32_t *spirv_code;
        uint32_t                        spirv_word_count;

        size_t                          type_description_count;
        SpvReflectTypeDescription *type_descriptions;
    } *_internal;

} SpvReflectShaderModule;

#if defined(__cplusplus)
extern "C" {
#endif

    /*! @fn spvReflectCreateShaderModule

     @param  size      Size in bytes of SPIR-V code.
     @param  p_code    Pointer to SPIR-V code.
     @param  p_module  Pointer to an instance of SpvReflectShaderModule.
     @return           SPV_REFLECT_RESULT_SUCCESS on success.

    */
    SpvReflectResult spvReflectCreateShaderModule(
        size_t                   size,
        const void *p_code,
        SpvReflectShaderModule *p_module
    );

    /*! @fn spvReflectCreateShaderModule2

     @param  flags     Flags for module creations.
     @param  size      Size in bytes of SPIR-V code.
     @param  p_code    Pointer to SPIR-V code.
     @param  p_module  Pointer to an instance of SpvReflectShaderModule.
     @return           SPV_REFLECT_RESULT_SUCCESS on success.

    */
    SpvReflectResult spvReflectCreateShaderModule2(
        SpvReflectModuleFlags    flags,
        size_t                   size,
        const void *p_code,
        SpvReflectShaderModule *p_module
    );

    SPV_REFLECT_DEPRECATED("renamed to spvReflectCreateShaderModule")
        SpvReflectResult spvReflectGetShaderModule(
            size_t                   size,
            const void *p_code,
            SpvReflectShaderModule *p_module
        );


    /*! @fn spvReflectDestroyShaderModule

     @param  p_module  Pointer to an instance of SpvReflectShaderModule.

    */
    void spvReflectDestroyShaderModule(SpvReflectShaderModule *p_module);


    /*! @fn spvReflectGetCodeSize

     @param  p_module  Pointer to an instance of SpvReflectShaderModule.
     @return           Returns the size of the SPIR-V in bytes

    */
    uint32_t spvReflectGetCodeSize(const SpvReflectShaderModule *p_module);


    /*! @fn spvReflectGetCode

     @param  p_module  Pointer to an instance of SpvReflectShaderModule.
     @return           Returns a const pointer to the compiled SPIR-V bytecode.

    */
    const uint32_t *spvReflectGetCode(const SpvReflectShaderModule *p_module);

    /*! @fn spvReflectGetEntryPoint

     @param  p_module     Pointer to an instance of SpvReflectShaderModule.
     @param  entry_point  Name of the requested entry point.
     @return              Returns a const pointer to the requested entry point,
                          or NULL if it's not found.
    */
    const SpvReflectEntryPoint *spvReflectGetEntryPoint(
        const SpvReflectShaderModule *p_module,
        const char *entry_point
    );

    /*! @fn spvReflectEnumerateDescriptorBindings

     @param  p_module     Pointer to an instance of SpvReflectShaderModule.
     @param  p_count      If pp_bindings is NULL, the module's descriptor binding
                          count (across all descriptor sets) will be stored here.
                          If pp_bindings is not NULL, *p_count must contain the
                          module's descriptor binding count.
     @param  pp_bindings  If NULL, the module's total descriptor binding count
                          will be written to *p_count.
                          If non-NULL, pp_bindings must point to an array with
                          *p_count entries, where pointers to the module's
                          descriptor bindings will be written. The caller must not
                          free the binding pointers written to this array.
     @return              If successful, returns SPV_REFLECT_RESULT_SUCCESS.
                          Otherwise, the error code indicates the cause of the
                          failure.

    */
    SpvReflectResult spvReflectEnumerateDescriptorBindings(
        const SpvReflectShaderModule *p_module,
        uint32_t *p_count,
        SpvReflectDescriptorBinding **pp_bindings
    );

    /*! @fn spvReflectEnumerateEntryPointDescriptorBindings
     @brief  Creates a listing of all descriptor bindings that are used in the
             static call tree of the given entry point.
     @param  p_module     Pointer to an instance of SpvReflectShaderModule.
     @param  entry_point  The name of the entry point to get the descriptor bindings for.
     @param  p_count      If pp_bindings is NULL, the entry point's descriptor binding
                          count (across all descriptor sets) will be stored here.
                          If pp_bindings is not NULL, *p_count must contain the
                          entry points's descriptor binding count.
     @param  pp_bindings  If NULL, the entry point's total descriptor binding count
                          will be written to *p_count.
                          If non-NULL, pp_bindings must point to an array with
                          *p_count entries, where pointers to the entry point's
                          descriptor bindings will be written. The caller must not
                          free the binding pointers written to this array.
     @return              If successful, returns SPV_REFLECT_RESULT_SUCCESS.
                          Otherwise, the error code indicates the cause of the
                          failure.

    */
    SpvReflectResult spvReflectEnumerateEntryPointDescriptorBindings(
        const SpvReflectShaderModule *p_module,
        const char *entry_point,
        uint32_t *p_count,
        SpvReflectDescriptorBinding **pp_bindings
    );

    /*! @fn spvReflectEnumerateDescriptorSets

     @param  p_module  Pointer to an instance of SpvReflectShaderModule.
     @param  p_count   If pp_sets is NULL, the module's descriptor set
                       count will be stored here.
                       If pp_sets is not NULL, *p_count must contain the
                       module's descriptor set count.
     @param  pp_sets   If NULL, the module's total descriptor set count
                       will be written to *p_count.
                       If non-NULL, pp_sets must point to an array with
                       *p_count entries, where pointers to the module's
                       descriptor sets will be written. The caller must not
                       free the descriptor set pointers written to this array.
     @return           If successful, returns SPV_REFLECT_RESULT_SUCCESS.
                       Otherwise, the error code indicates the cause of the
                       failure.

    */
    SpvReflectResult spvReflectEnumerateDescriptorSets(
        const SpvReflectShaderModule *p_module,
        uint32_t *p_count,
        SpvReflectDescriptorSet **pp_sets
    );

    /*! @fn spvReflectEnumerateEntryPointDescriptorSets
     @brief  Creates a listing of all descriptor sets and their bindings that are
             used in the static call tree of a given entry point.
     @param  p_module    Pointer to an instance of SpvReflectShaderModule.
     @param  entry_point The name of the entry point to get the descriptor bindings for.
     @param  p_count     If pp_sets is NULL, the module's descriptor set
                         count will be stored here.
                         If pp_sets is not NULL, *p_count must contain the
                         module's descriptor set count.
     @param  pp_sets     If NULL, the module's total descriptor set count
                         will be written to *p_count.
                         If non-NULL, pp_sets must point to an array with
                         *p_count entries, where pointers to the module's
                         descriptor sets will be written. The caller must not
                         free the descriptor set pointers written to this array.
     @return             If successful, returns SPV_REFLECT_RESULT_SUCCESS.
                         Otherwise, the error code indicates the cause of the
                         failure.

    */
    SpvReflectResult spvReflectEnumerateEntryPointDescriptorSets(
        const SpvReflectShaderModule *p_module,
        const char *entry_point,
        uint32_t *p_count,
        SpvReflectDescriptorSet **pp_sets
    );


    /*! @fn spvReflectEnumerateInterfaceVariables
     @brief  If the module contains multiple entry points, this will only get
             the interface variables for the first one.
     @param  p_module      Pointer to an instance of SpvReflectShaderModule.
     @param  p_count       If pp_variables is NULL, the module's interface variable
                           count will be stored here.
                           If pp_variables is not NULL, *p_count must contain
                           the module's interface variable count.
     @param  pp_variables  If NULL, the module's interface variable count will be
                           written to *p_count.
                           If non-NULL, pp_variables must point to an array with
                           *p_count entries, where pointers to the module's
                           interface variables will be written. The caller must not
                           free the interface variables written to this array.
     @return               If successful, returns SPV_REFLECT_RESULT_SUCCESS.
                           Otherwise, the error code indicates the cause of the
                           failure.

    */
    SpvReflectResult spvReflectEnumerateInterfaceVariables(
        const SpvReflectShaderModule *p_module,
        uint32_t *p_count,
        SpvReflectInterfaceVariable **pp_variables
    );

    /*! @fn spvReflectEnumerateEntryPointInterfaceVariables
     @brief  Enumerate the interface variables for a given entry point.
     @param  entry_point The name of the entry point to get the interface variables for.
     @param  p_module      Pointer to an instance of SpvReflectShaderModule.
     @param  p_count       If pp_variables is NULL, the entry point's interface variable
                           count will be stored here.
                           If pp_variables is not NULL, *p_count must contain
                           the entry point's interface variable count.
     @param  pp_variables  If NULL, the entry point's interface variable count will be
                           written to *p_count.
                           If non-NULL, pp_variables must point to an array with
                           *p_count entries, where pointers to the entry point's
                           interface variables will be written. The caller must not
                           free the interface variables written to this array.
     @return               If successful, returns SPV_REFLECT_RESULT_SUCCESS.
                           Otherwise, the error code indicates the cause of the
                           failure.

    */
    SpvReflectResult spvReflectEnumerateEntryPointInterfaceVariables(
        const SpvReflectShaderModule *p_module,
        const char *entry_point,
        uint32_t *p_count,
        SpvReflectInterfaceVariable **pp_variables
    );


    /*! @fn spvReflectEnumerateInputVariables
     @brief  If the module contains multiple entry points, this will only get
             the input variables for the first one.
     @param  p_module      Pointer to an instance of SpvReflectShaderModule.
     @param  p_count       If pp_variables is NULL, the module's input variable
                           count will be stored here.
                           If pp_variables is not NULL, *p_count must contain
                           the module's input variable count.
     @param  pp_variables  If NULL, the module's input variable count will be
                           written to *p_count.
                           If non-NULL, pp_variables must point to an array with
                           *p_count entries, where pointers to the module's
                           input variables will be written. The caller must not
                           free the interface variables written to this array.
     @return               If successful, returns SPV_REFLECT_RESULT_SUCCESS.
                           Otherwise, the error code indicates the cause of the
                           failure.

    */
    SpvReflectResult spvReflectEnumerateInputVariables(
        const SpvReflectShaderModule *p_module,
        uint32_t *p_count,
        SpvReflectInterfaceVariable **pp_variables
    );

    /*! @fn spvReflectEnumerateEntryPointInputVariables
     @brief  Enumerate the input variables for a given entry point.
     @param  entry_point The name of the entry point to get the input variables for.
     @param  p_module      Pointer to an instance of SpvReflectShaderModule.
     @param  p_count       If pp_variables is NULL, the entry point's input variable
                           count will be stored here.
                           If pp_variables is not NULL, *p_count must contain
                           the entry point's input variable count.
     @param  pp_variables  If NULL, the entry point's input variable count will be
                           written to *p_count.
                           If non-NULL, pp_variables must point to an array with
                           *p_count entries, where pointers to the entry point's
                           input variables will be written. The caller must not
                           free the interface variables written to this array.
     @return               If successful, returns SPV_REFLECT_RESULT_SUCCESS.
                           Otherwise, the error code indicates the cause of the
                           failure.

    */
    SpvReflectResult spvReflectEnumerateEntryPointInputVariables(
        const SpvReflectShaderModule *p_module,
        const char *entry_point,
        uint32_t *p_count,
        SpvReflectInterfaceVariable **pp_variables
    );


    /*! @fn spvReflectEnumerateOutputVariables
     @brief  Note: If the module contains multiple entry points, this will only get
             the output variables for the first one.
     @param  p_module      Pointer to an instance of SpvReflectShaderModule.
     @param  p_count       If pp_variables is NULL, the module's output variable
                           count will be stored here.
                           If pp_variables is not NULL, *p_count must contain
                           the module's output variable count.
     @param  pp_variables  If NULL, the module's output variable count will be
                           written to *p_count.
                           If non-NULL, pp_variables must point to an array with
                           *p_count entries, where pointers to the module's
                           output variables will be written. The caller must not
                           free the interface variables written to this array.
     @return               If successful, returns SPV_REFLECT_RESULT_SUCCESS.
                           Otherwise, the error code indicates the cause of the
                           failure.

    */
    SpvReflectResult spvReflectEnumerateOutputVariables(
        const SpvReflectShaderModule *p_module,
        uint32_t *p_count,
        SpvReflectInterfaceVariable **pp_variables
    );

    /*! @fn spvReflectEnumerateEntryPointOutputVariables
     @brief  Enumerate the output variables for a given entry point.
     @param  p_module      Pointer to an instance of SpvReflectShaderModule.
     @param  entry_point   The name of the entry point to get the output variables for.
     @param  p_count       If pp_variables is NULL, the entry point's output variable
                           count will be stored here.
                           If pp_variables is not NULL, *p_count must contain
                           the entry point's output variable count.
     @param  pp_variables  If NULL, the entry point's output variable count will be
                           written to *p_count.
                           If non-NULL, pp_variables must point to an array with
                           *p_count entries, where pointers to the entry point's
                           output variables will be written. The caller must not
                           free the interface variables written to this array.
     @return               If successful, returns SPV_REFLECT_RESULT_SUCCESS.
                           Otherwise, the error code indicates the cause of the
                           failure.

    */
    SpvReflectResult spvReflectEnumerateEntryPointOutputVariables(
        const SpvReflectShaderModule *p_module,
        const char *entry_point,
        uint32_t *p_count,
        SpvReflectInterfaceVariable **pp_variables
    );


    /*! @fn spvReflectEnumeratePushConstantBlocks
     @brief  Note: If the module contains multiple entry points, this will only get
             the push constant blocks for the first one.
     @param  p_module   Pointer to an instance of SpvReflectShaderModule.
     @param  p_count    If pp_blocks is NULL, the module's push constant
                        block count will be stored here.
                        If pp_blocks is not NULL, *p_count must
                        contain the module's push constant block count.
     @param  pp_blocks  If NULL, the module's push constant block count
                        will be written to *p_count.
                        If non-NULL, pp_blocks must point to an
                        array with *p_count entries, where pointers to
                        the module's push constant blocks will be written.
                        The caller must not free the block variables written
                        to this array.
     @return            If successful, returns SPV_REFLECT_RESULT_SUCCESS.
                        Otherwise, the error code indicates the cause of the
                        failure.

    */
    SpvReflectResult spvReflectEnumeratePushConstantBlocks(
        const SpvReflectShaderModule *p_module,
        uint32_t *p_count,
        SpvReflectBlockVariable **pp_blocks
    );
    SPV_REFLECT_DEPRECATED("renamed to spvReflectEnumeratePushConstantBlocks")
        SpvReflectResult spvReflectEnumeratePushConstants(
            const SpvReflectShaderModule *p_module,
            uint32_t *p_count,
            SpvReflectBlockVariable **pp_blocks
        );

    /*! @fn spvReflectEnumerateEntryPointPushConstantBlocks
     @brief  Enumerate the push constant blocks used in the static call tree of a
             given entry point.
     @param  p_module   Pointer to an instance of SpvReflectShaderModule.
     @param  p_count    If pp_blocks is NULL, the entry point's push constant
                        block count will be stored here.
                        If pp_blocks is not NULL, *p_count must
                        contain the entry point's push constant block count.
     @param  pp_blocks  If NULL, the entry point's push constant block count
                        will be written to *p_count.
                        If non-NULL, pp_blocks must point to an
                        array with *p_count entries, where pointers to
                        the entry point's push constant blocks will be written.
                        The caller must not free the block variables written
                        to this array.
     @return            If successful, returns SPV_REFLECT_RESULT_SUCCESS.
                        Otherwise, the error code indicates the cause of the
                        failure.

    */
    SpvReflectResult spvReflectEnumerateEntryPointPushConstantBlocks(
        const SpvReflectShaderModule *p_module,
        const char *entry_point,
        uint32_t *p_count,
        SpvReflectBlockVariable **pp_blocks
    );


    /*! @fn spvReflectEnumerateSpecializationConstants
     @param  p_module      Pointer to an instance of SpvReflectShaderModule.
     @param  p_count       If pp_blocks is NULL, the module's specialization constant
                           count will be stored here. If pp_blocks is not NULL, *p_count
                           must contain the module's specialization constant count.
     @param  pp_constants  If NULL, the module's specialization constant count
                           will be written to *p_count. If non-NULL, pp_blocks must
                           point to an array with *p_count entries, where pointers to
                           the module's specialization constant blocks will be written.
                           The caller must not free the  variables written to this array.
     @return               If successful, returns SPV_REFLECT_RESULT_SUCCESS.
                           Otherwise, the error code indicates the cause of the failure.
    */
    SpvReflectResult spvReflectEnumerateSpecializationConstants(
        const SpvReflectShaderModule *p_module,
        uint32_t *p_count,
        SpvReflectSpecializationConstant **pp_constants
    );

    /*! @fn spvReflectGetDescriptorBinding

     @param  p_module        Pointer to an instance of SpvReflectShaderModule.
     @param  binding_number  The "binding" value of the requested descriptor
                             binding.
     @param  set_number      The "set" value of the requested descriptor binding.
     @param  p_result        If successful, SPV_REFLECT_RESULT_SUCCESS will be
                             written to *p_result. Otherwise, a error code
                             indicating the cause of the failure will be stored
                             here.
     @return                 If the module contains a descriptor binding that
                             matches the provided [binding_number, set_number]
                             values, a pointer to that binding is returned. The
                             caller must not free this pointer.
                             If no match can be found, or if an unrelated error
                             occurs, the return value will be NULL. Detailed
                             error results are written to *pResult.
    @note                    If the module contains multiple desriptor bindings
                             with the same set and binding numbers, there are
                             no guarantees about which binding will be returned.

    */
    const SpvReflectDescriptorBinding *spvReflectGetDescriptorBinding(
        const SpvReflectShaderModule *p_module,
        uint32_t                      binding_number,
        uint32_t                      set_number,
        SpvReflectResult *p_result
    );

    /*! @fn spvReflectGetEntryPointDescriptorBinding
     @brief  Get the descriptor binding with the given binding number and set
             number that is used in the static call tree of a certain entry
             point.
     @param  p_module        Pointer to an instance of SpvReflectShaderModule.
     @param  entry_point     The entry point to get the binding from.
     @param  binding_number  The "binding" value of the requested descriptor
                             binding.
     @param  set_number      The "set" value of the requested descriptor binding.
     @param  p_result        If successful, SPV_REFLECT_RESULT_SUCCESS will be
                             written to *p_result. Otherwise, a error code
                             indicating the cause of the failure will be stored
                             here.
     @return                 If the entry point contains a descriptor binding that
                             matches the provided [binding_number, set_number]
                             values, a pointer to that binding is returned. The
                             caller must not free this pointer.
                             If no match can be found, or if an unrelated error
                             occurs, the return value will be NULL. Detailed
                             error results are written to *pResult.
    @note                    If the entry point contains multiple desriptor bindings
                             with the same set and binding numbers, there are
                             no guarantees about which binding will be returned.

    */
    const SpvReflectDescriptorBinding *spvReflectGetEntryPointDescriptorBinding(
        const SpvReflectShaderModule *p_module,
        const char *entry_point,
        uint32_t                      binding_number,
        uint32_t                      set_number,
        SpvReflectResult *p_result
    );


    /*! @fn spvReflectGetDescriptorSet

     @param  p_module    Pointer to an instance of SpvReflectShaderModule.
     @param  set_number  The "set" value of the requested descriptor set.
     @param  p_result    If successful, SPV_REFLECT_RESULT_SUCCESS will be
                         written to *p_result. Otherwise, a error code
                         indicating the cause of the failure will be stored
                         here.
     @return             If the module contains a descriptor set with the
                         provided set_number, a pointer to that set is
                         returned. The caller must not free this pointer.
                         If no match can be found, or if an unrelated error
                         occurs, the return value will be NULL. Detailed
                         error results are written to *pResult.

    */
    const SpvReflectDescriptorSet *spvReflectGetDescriptorSet(
        const SpvReflectShaderModule *p_module,
        uint32_t                      set_number,
        SpvReflectResult *p_result
    );

    /*! @fn spvReflectGetEntryPointDescriptorSet

     @param  p_module    Pointer to an instance of SpvReflectShaderModule.
     @param  entry_point The entry point to get the descriptor set from.
     @param  set_number  The "set" value of the requested descriptor set.
     @param  p_result    If successful, SPV_REFLECT_RESULT_SUCCESS will be
                         written to *p_result. Otherwise, a error code
                         indicating the cause of the failure will be stored
                         here.
     @return             If the entry point contains a descriptor set with the
                         provided set_number, a pointer to that set is
                         returned. The caller must not free this pointer.
                         If no match can be found, or if an unrelated error
                         occurs, the return value will be NULL. Detailed
                         error results are written to *pResult.

    */
    const SpvReflectDescriptorSet *spvReflectGetEntryPointDescriptorSet(
        const SpvReflectShaderModule *p_module,
        const char *entry_point,
        uint32_t                      set_number,
        SpvReflectResult *p_result
    );


    /* @fn spvReflectGetInputVariableByLocation

     @param  p_module  Pointer to an instance of SpvReflectShaderModule.
     @param  location  The "location" value of the requested input variable.
                       A location of 0xFFFFFFFF will always return NULL
                       with *p_result == ELEMENT_NOT_FOUND.
     @param  p_result  If successful, SPV_REFLECT_RESULT_SUCCESS will be
                       written to *p_result. Otherwise, a error code
                       indicating the cause of the failure will be stored
                       here.
     @return           If the module contains an input interface variable
                       with the provided location value, a pointer to that
                       variable is returned. The caller must not free this
                       pointer.
                       If no match can be found, or if an unrelated error
                       occurs, the return value will be NULL. Detailed
                       error results are written to *pResult.
    @note

    */
    const SpvReflectInterfaceVariable *spvReflectGetInputVariableByLocation(
        const SpvReflectShaderModule *p_module,
        uint32_t                      location,
        SpvReflectResult *p_result
    );
    SPV_REFLECT_DEPRECATED("renamed to spvReflectGetInputVariableByLocation")
        const SpvReflectInterfaceVariable *spvReflectGetInputVariable(
            const SpvReflectShaderModule *p_module,
            uint32_t                      location,
            SpvReflectResult *p_result
        );

    /* @fn spvReflectGetEntryPointInputVariableByLocation

     @param  p_module    Pointer to an instance of SpvReflectShaderModule.
     @param  entry_point The entry point to get the input variable from.
     @param  location    The "location" value of the requested input variable.
                         A location of 0xFFFFFFFF will always return NULL
                         with *p_result == ELEMENT_NOT_FOUND.
     @param  p_result    If successful, SPV_REFLECT_RESULT_SUCCESS will be
                         written to *p_result. Otherwise, a error code
                         indicating the cause of the failure will be stored
                         here.
     @return             If the entry point contains an input interface variable
                         with the provided location value, a pointer to that
                         variable is returned. The caller must not free this
                         pointer.
                         If no match can be found, or if an unrelated error
                         occurs, the return value will be NULL. Detailed
                         error results are written to *pResult.
    @note

    */
    const SpvReflectInterfaceVariable *spvReflectGetEntryPointInputVariableByLocation(
        const SpvReflectShaderModule *p_module,
        const char *entry_point,
        uint32_t                      location,
        SpvReflectResult *p_result
    );

    /* @fn spvReflectGetInputVariableBySemantic

     @param  p_module  Pointer to an instance of SpvReflectShaderModule.
     @param  semantic  The "semantic" value of the requested input variable.
                       A semantic of NULL will return NULL.
                       A semantic of "" will always return NULL with
                       *p_result == ELEMENT_NOT_FOUND.
     @param  p_result  If successful, SPV_REFLECT_RESULT_SUCCESS will be
                       written to *p_result. Otherwise, a error code
                       indicating the cause of the failure will be stored
                       here.
     @return           If the module contains an input interface variable
                       with the provided semantic, a pointer to that
                       variable is returned. The caller must not free this
                       pointer.
                       If no match can be found, or if an unrelated error
                       occurs, the return value will be NULL. Detailed
                       error results are written to *pResult.
    @note

    */
    const SpvReflectInterfaceVariable *spvReflectGetInputVariableBySemantic(
        const SpvReflectShaderModule *p_module,
        const char *semantic,
        SpvReflectResult *p_result
    );

    /* @fn spvReflectGetEntryPointInputVariableBySemantic

     @param  p_module  Pointer to an instance of SpvReflectShaderModule.
     @param  entry_point The entry point to get the input variable from.
     @param  semantic  The "semantic" value of the requested input variable.
                       A semantic of NULL will return NULL.
                       A semantic of "" will always return NULL with
                       *p_result == ELEMENT_NOT_FOUND.
     @param  p_result  If successful, SPV_REFLECT_RESULT_SUCCESS will be
                       written to *p_result. Otherwise, a error code
                       indicating the cause of the failure will be stored
                       here.
     @return           If the entry point contains an input interface variable
                       with the provided semantic, a pointer to that
                       variable is returned. The caller must not free this
                       pointer.
                       If no match can be found, or if an unrelated error
                       occurs, the return value will be NULL. Detailed
                       error results are written to *pResult.
    @note

    */
    const SpvReflectInterfaceVariable *spvReflectGetEntryPointInputVariableBySemantic(
        const SpvReflectShaderModule *p_module,
        const char *entry_point,
        const char *semantic,
        SpvReflectResult *p_result
    );

    /* @fn spvReflectGetOutputVariableByLocation

     @param  p_module  Pointer to an instance of SpvReflectShaderModule.
     @param  location  The "location" value of the requested output variable.
                       A location of 0xFFFFFFFF will always return NULL
                       with *p_result == ELEMENT_NOT_FOUND.
     @param  p_result  If successful, SPV_REFLECT_RESULT_SUCCESS will be
                       written to *p_result. Otherwise, a error code
                       indicating the cause of the failure will be stored
                       here.
     @return           If the module contains an output interface variable
                       with the provided location value, a pointer to that
                       variable is returned. The caller must not free this
                       pointer.
                       If no match can be found, or if an unrelated error
                       occurs, the return value will be NULL. Detailed
                       error results are written to *pResult.
    @note

    */
    const SpvReflectInterfaceVariable *spvReflectGetOutputVariableByLocation(
        const SpvReflectShaderModule *p_module,
        uint32_t                       location,
        SpvReflectResult *p_result
    );
    SPV_REFLECT_DEPRECATED("renamed to spvReflectGetOutputVariableByLocation")
        const SpvReflectInterfaceVariable *spvReflectGetOutputVariable(
            const SpvReflectShaderModule *p_module,
            uint32_t                       location,
            SpvReflectResult *p_result
        );

    /* @fn spvReflectGetEntryPointOutputVariableByLocation

     @param  p_module     Pointer to an instance of SpvReflectShaderModule.
     @param  entry_point  The entry point to get the output variable from.
     @param  location     The "location" value of the requested output variable.
                          A location of 0xFFFFFFFF will always return NULL
                          with *p_result == ELEMENT_NOT_FOUND.
     @param  p_result     If successful, SPV_REFLECT_RESULT_SUCCESS will be
                          written to *p_result. Otherwise, a error code
                          indicating the cause of the failure will be stored
                          here.
     @return              If the entry point contains an output interface variable
                          with the provided location value, a pointer to that
                          variable is returned. The caller must not free this
                          pointer.
                          If no match can be found, or if an unrelated error
                          occurs, the return value will be NULL. Detailed
                          error results are written to *pResult.
    @note

    */
    const SpvReflectInterfaceVariable *spvReflectGetEntryPointOutputVariableByLocation(
        const SpvReflectShaderModule *p_module,
        const char *entry_point,
        uint32_t                       location,
        SpvReflectResult *p_result
    );

    /* @fn spvReflectGetOutputVariableBySemantic

     @param  p_module  Pointer to an instance of SpvReflectShaderModule.
     @param  semantic  The "semantic" value of the requested output variable.
                       A semantic of NULL will return NULL.
                       A semantic of "" will always return NULL with
                       *p_result == ELEMENT_NOT_FOUND.
     @param  p_result  If successful, SPV_REFLECT_RESULT_SUCCESS will be
                       written to *p_result. Otherwise, a error code
                       indicating the cause of the failure will be stored
                       here.
     @return           If the module contains an output interface variable
                       with the provided semantic, a pointer to that
                       variable is returned. The caller must not free this
                       pointer.
                       If no match can be found, or if an unrelated error
                       occurs, the return value will be NULL. Detailed
                       error results are written to *pResult.
    @note

    */
    const SpvReflectInterfaceVariable *spvReflectGetOutputVariableBySemantic(
        const SpvReflectShaderModule *p_module,
        const char *semantic,
        SpvReflectResult *p_result
    );

    /* @fn spvReflectGetEntryPointOutputVariableBySemantic

     @param  p_module  Pointer to an instance of SpvReflectShaderModule.
     @param  entry_point  The entry point to get the output variable from.
     @param  semantic  The "semantic" value of the requested output variable.
                       A semantic of NULL will return NULL.
                       A semantic of "" will always return NULL with
                       *p_result == ELEMENT_NOT_FOUND.
     @param  p_result  If successful, SPV_REFLECT_RESULT_SUCCESS will be
                       written to *p_result. Otherwise, a error code
                       indicating the cause of the failure will be stored
                       here.
     @return           If the entry point contains an output interface variable
                       with the provided semantic, a pointer to that
                       variable is returned. The caller must not free this
                       pointer.
                       If no match can be found, or if an unrelated error
                       occurs, the return value will be NULL. Detailed
                       error results are written to *pResult.
    @note

    */
    const SpvReflectInterfaceVariable *spvReflectGetEntryPointOutputVariableBySemantic(
        const SpvReflectShaderModule *p_module,
        const char *entry_point,
        const char *semantic,
        SpvReflectResult *p_result
    );

    /*! @fn spvReflectGetPushConstantBlock

     @param  p_module  Pointer to an instance of SpvReflectShaderModule.
     @param  index     The index of the desired block within the module's
                       array of push constant blocks.
     @param  p_result  If successful, SPV_REFLECT_RESULT_SUCCESS will be
                       written to *p_result. Otherwise, a error code
                       indicating the cause of the failure will be stored
                       here.
     @return           If the provided index is within range, a pointer to
                       the corresponding push constant block is returned.
                       The caller must not free this pointer.
                       If no match can be found, or if an unrelated error
                       occurs, the return value will be NULL. Detailed
                       error results are written to *pResult.

    */
    const SpvReflectBlockVariable *spvReflectGetPushConstantBlock(
        const SpvReflectShaderModule *p_module,
        uint32_t                       index,
        SpvReflectResult *p_result
    );
    SPV_REFLECT_DEPRECATED("renamed to spvReflectGetPushConstantBlock")
        const SpvReflectBlockVariable *spvReflectGetPushConstant(
            const SpvReflectShaderModule *p_module,
            uint32_t                       index,
            SpvReflectResult *p_result
        );

    /*! @fn spvReflectGetEntryPointPushConstantBlock
     @brief  Get the push constant block corresponding to the given entry point.
             As by the Vulkan specification there can be no more than one push
             constant block used by a given entry point, so if there is one it will
             be returned, otherwise NULL will be returned.
     @param  p_module     Pointer to an instance of SpvReflectShaderModule.
     @param  entry_point  The entry point to get the push constant block from.
     @param  p_result     If successful, SPV_REFLECT_RESULT_SUCCESS will be
                          written to *p_result. Otherwise, a error code
                          indicating the cause of the failure will be stored
                          here.
     @return              If the provided index is within range, a pointer to
                          the corresponding push constant block is returned.
                          The caller must not free this pointer.
                          If no match can be found, or if an unrelated error
                          occurs, the return value will be NULL. Detailed
                          error results are written to *pResult.

    */
    const SpvReflectBlockVariable *spvReflectGetEntryPointPushConstantBlock(
        const SpvReflectShaderModule *p_module,
        const char *entry_point,
        SpvReflectResult *p_result
    );


    /*! @fn spvReflectChangeDescriptorBindingNumbers
     @brief  Assign new set and/or binding numbers to a descriptor binding.
             In addition to updating the reflection data, this function modifies
             the underlying SPIR-V bytecode. The updated code can be retrieved
             with spvReflectGetCode().  If the binding is used in multiple
             entry points within the module, it will be changed in all of them.
     @param  p_module            Pointer to an instance of SpvReflectShaderModule.
     @param  p_binding           Pointer to the descriptor binding to modify.
     @param  new_binding_number  The new binding number to assign to the
                                 provided descriptor binding.
                                 To leave the binding number unchanged, pass
                                 SPV_REFLECT_BINDING_NUMBER_DONT_CHANGE.
     @param  new_set_number      The new set number to assign to the
                                 provided descriptor binding. Successfully changing
                                 a descriptor binding's set number invalidates all
                                 existing SpvReflectDescriptorBinding and
                                 SpvReflectDescriptorSet pointers from this module.
                                 To leave the set number unchanged, pass
                                 SPV_REFLECT_SET_NUMBER_DONT_CHANGE.
     @return                     If successful, returns SPV_REFLECT_RESULT_SUCCESS.
                                 Otherwise, the error code indicates the cause of
                                 the failure.
    */
    SpvReflectResult spvReflectChangeDescriptorBindingNumbers(
        SpvReflectShaderModule *p_module,
        const SpvReflectDescriptorBinding *p_binding,
        uint32_t                           new_binding_number,
        uint32_t                           new_set_number
    );
    SPV_REFLECT_DEPRECATED("Renamed to spvReflectChangeDescriptorBindingNumbers")
        SpvReflectResult spvReflectChangeDescriptorBindingNumber(
            SpvReflectShaderModule *p_module,
            const SpvReflectDescriptorBinding *p_descriptor_binding,
            uint32_t                           new_binding_number,
            uint32_t                           optional_new_set_number
        );

    /*! @fn spvReflectChangeDescriptorSetNumber
     @brief  Assign a new set number to an entire descriptor set (including
             all descriptor bindings in that set).
             In addition to updating the reflection data, this function modifies
             the underlying SPIR-V bytecode. The updated code can be retrieved
             with spvReflectGetCode().  If the descriptor set is used in
             multiple entry points within the module, it will be modified in all
             of them.
     @param  p_module        Pointer to an instance of SpvReflectShaderModule.
     @param  p_set           Pointer to the descriptor binding to modify.
     @param  new_set_number  The new set number to assign to the
                             provided descriptor set, and all its descriptor
                             bindings. Successfully changing a descriptor
                             binding's set number invalidates all existing
                             SpvReflectDescriptorBinding and
                             SpvReflectDescriptorSet pointers from this module.
                             To leave the set number unchanged, pass
                             SPV_REFLECT_SET_NUMBER_DONT_CHANGE.
     @return                 If successful, returns SPV_REFLECT_RESULT_SUCCESS.
                             Otherwise, the error code indicates the cause of
                             the failure.
    */
    SpvReflectResult spvReflectChangeDescriptorSetNumber(
        SpvReflectShaderModule *p_module,
        const SpvReflectDescriptorSet *p_set,
        uint32_t                       new_set_number
    );

    /*! @fn spvReflectChangeInputVariableLocation
     @brief  Assign a new location to an input interface variable.
             In addition to updating the reflection data, this function modifies
             the underlying SPIR-V bytecode. The updated code can be retrieved
             with spvReflectGetCode().
             It is the caller's responsibility to avoid assigning the same
             location to multiple input variables.  If the input variable is used
             by multiple entry points in the module, it will be changed in all of
             them.
     @param  p_module          Pointer to an instance of SpvReflectShaderModule.
     @param  p_input_variable  Pointer to the input variable to update.
     @param  new_location      The new location to assign to p_input_variable.
     @return                   If successful, returns SPV_REFLECT_RESULT_SUCCESS.
                               Otherwise, the error code indicates the cause of
                               the failure.

    */
    SpvReflectResult spvReflectChangeInputVariableLocation(
        SpvReflectShaderModule *p_module,
        const SpvReflectInterfaceVariable *p_input_variable,
        uint32_t                           new_location
    );


    /*! @fn spvReflectChangeOutputVariableLocation
     @brief  Assign a new location to an output interface variable.
             In addition to updating the reflection data, this function modifies
             the underlying SPIR-V bytecode. The updated code can be retrieved
             with spvReflectGetCode().
             It is the caller's responsibility to avoid assigning the same
             location to multiple output variables.  If the output variable is used
             by multiple entry points in the module, it will be changed in all of
             them.
     @param  p_module          Pointer to an instance of SpvReflectShaderModule.
     @param  p_output_variable Pointer to the output variable to update.
     @param  new_location      The new location to assign to p_output_variable.
     @return                   If successful, returns SPV_REFLECT_RESULT_SUCCESS.
                               Otherwise, the error code indicates the cause of
                               the failure.

    */
    SpvReflectResult spvReflectChangeOutputVariableLocation(
        SpvReflectShaderModule *p_module,
        const SpvReflectInterfaceVariable *p_output_variable,
        uint32_t                            new_location
    );


    /*! @fn spvReflectSourceLanguage

     @param  source_lang  The source language code.
     @return Returns string of source language specified in \a source_lang.
             The caller must not free the memory associated with this string.
    */
    const char *spvReflectSourceLanguage(SpvSourceLanguage source_lang);

    /*! @fn spvReflectBlockVariableTypeName

     @param  p_var Pointer to block variable.
     @return Returns string of block variable's type description type name
             or NULL if p_var is NULL.
    */
    const char *spvReflectBlockVariableTypeName(
        const SpvReflectBlockVariable *p_var
    );

#if defined(__cplusplus)
};
#endif

#endif // SPIRV_REFLECT_H

// clang-format on

#if defined(ZEST_SPIRV_REFLECT_IMPLENTATION)
/*
 Copyright 2017-2022 Google Inc.

 Licensed under the Apache License, Version 2.0 (the "License");
 you may not use this file except in compliance with the License.
 You may obtain a copy of the License at

 http://www.apache.org/licenses/LICENSE-2.0

 Unless required by applicable law or agreed to in writing, software
 distributed under the License is distributed on an "AS IS" BASIS,
 WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 See the License for the specific language governing permissions and
 limitations under the License.
*/

#include <assert.h>
#include <stdbool.h>
#include <string.h>

#if defined(WIN32)
#define _CRTDBG_MAP_ALLOC
#include <crtdbg.h>
#include <stdlib.h>
#else
#include <stdlib.h>
#endif

#if defined(__clang__) || (defined(__GNUC__) && __GNUC__ >= 7) || defined(__APPLE_CC__)
#define FALLTHROUGH __attribute__((fallthrough))
#else
#define FALLTHROUGH
#endif

#if defined(SPIRV_REFLECT_ENABLE_ASSERTS)
#define SPV_REFLECT_ASSERT(COND) assert(COND);
#else
#define SPV_REFLECT_ASSERT(COND)
#endif

// clang-format off
enum {
    SPIRV_STARTING_WORD_INDEX = 5,
    SPIRV_WORD_SIZE = sizeof(uint32_t),
    SPIRV_BYTE_WIDTH = 8,
    SPIRV_MINIMUM_FILE_SIZE = SPIRV_STARTING_WORD_INDEX * SPIRV_WORD_SIZE,
    SPIRV_DATA_ALIGNMENT = 4 * SPIRV_WORD_SIZE, // 16
    SPIRV_ACCESS_CHAIN_INDEX_OFFSET = 4,
    SPIRV_PHYSICAL_STORAGE_POINTER_SIZE = 8, // Pointers are defined as 64-bit
};

enum {
    INVALID_VALUE = 0xFFFFFFFF,
};

enum {
    MAX_NODE_NAME_LENGTH = 1024,
    // Number of unique PhysicalStorageBuffer structs tracked to detect recursion
    MAX_RECURSIVE_PHYSICAL_POINTER_CHECK = 128,
};

enum {
    IMAGE_SAMPLED = 1,
    IMAGE_STORAGE = 2,
};

typedef struct SpvReflectPrvArrayTraits {
    uint32_t                        element_type_id;
    uint32_t                        length_id;
} SpvReflectPrvArrayTraits;

typedef struct SpvReflectPrvImageTraits {
    uint32_t                        sampled_type_id;
    SpvDim                          dim;
    uint32_t                        depth;
    uint32_t                        arrayed;
    uint32_t                        ms;
    uint32_t                        sampled;
    SpvImageFormat                  image_format;
} SpvReflectPrvImageTraits;

typedef struct SpvReflectPrvNumberDecoration {
    uint32_t                        word_offset;
    uint32_t                        value;
} SpvReflectPrvNumberDecoration;

typedef struct SpvReflectPrvStringDecoration {
    uint32_t                        word_offset;
    const char *value;
} SpvReflectPrvStringDecoration;

typedef struct SpvReflectPrvDecorations {
    bool                            is_relaxed_precision;
    bool                            is_block;
    bool                            is_buffer_block;
    bool                            is_row_major;
    bool                            is_column_major;
    bool                            is_built_in;
    bool                            is_noperspective;
    bool                            is_flat;
    bool                            is_non_writable;
    bool                            is_non_readable;
    bool                            is_patch;
    bool                            is_per_vertex;
    bool                            is_per_task;
    bool                            is_weight_texture;
    bool                            is_block_match_texture;
    SpvReflectUserType              user_type;
    SpvReflectPrvNumberDecoration   set;
    SpvReflectPrvNumberDecoration   binding;
    SpvReflectPrvNumberDecoration   input_attachment_index;
    SpvReflectPrvNumberDecoration   location;
    SpvReflectPrvNumberDecoration   component;
    SpvReflectPrvNumberDecoration   offset;
    SpvReflectPrvNumberDecoration   uav_counter_buffer;
    SpvReflectPrvStringDecoration   semantic;
    uint32_t                        array_stride;
    uint32_t                        matrix_stride;
    uint32_t                        spec_id;
    SpvBuiltIn                      built_in;
} SpvReflectPrvDecorations;

typedef struct SpvReflectPrvNode {
    uint32_t                        result_id;
    SpvOp                           op;
    uint32_t                        result_type_id;
    uint32_t                        type_id;
    SpvCapability                   capability;
    SpvStorageClass                 storage_class;
    uint32_t                        word_offset;
    uint32_t                        word_count;
    bool                            is_type;

    SpvReflectPrvArrayTraits        array_traits;
    SpvReflectPrvImageTraits        image_traits;
    uint32_t                        image_type_id;

    const char *name;
    SpvReflectPrvDecorations        decorations;
    uint32_t                        member_count;
    const char **member_names;
    SpvReflectPrvDecorations *member_decorations;
} SpvReflectPrvNode;

typedef struct SpvReflectPrvString {
    uint32_t                        result_id;
    const char *string;
} SpvReflectPrvString;

// There are a limit set of instructions that can touch an OpVariable,
// these are represented here with how it was accessed
// Examples:
//    OpImageRead  -> OpLoad -> OpVariable
//    OpImageWrite -> OpLoad -> OpVariable
//    OpStore      -> OpAccessChain -> OpAccessChain -> OpVariable
//    OpAtomicIAdd -> OpAccessChain -> OpVariable
//    OpAtomicLoad -> OpImageTexelPointer -> OpVariable
typedef struct SpvReflectPrvAccessedVariable {
    SpvReflectPrvNode *p_node;
    uint32_t               result_id;
    uint32_t               variable_ptr;
    uint32_t               function_id;
    uint32_t               function_parameter_index;
} SpvReflectPrvAccessedVariable;

typedef struct SpvReflectPrvFunction {
    uint32_t                        id;
    uint32_t                        parameter_count;
    uint32_t *parameters;
    uint32_t                        callee_count;
    uint32_t *callees;
    struct SpvReflectPrvFunction **callee_ptrs;
    uint32_t                        accessed_variable_count;
    SpvReflectPrvAccessedVariable *accessed_variables;
} SpvReflectPrvFunction;

typedef struct SpvReflectPrvAccessChain {
    uint32_t                        result_id;
    uint32_t                        result_type_id;
    //
    // Pointing to the base of a composite object.
    // Generally the id of descriptor block variable
    uint32_t                        base_id;
    //
    // From spec:
    //   The first index in Indexes will select the
    //   top-level member/element/component/element
    //   of the base composite
    uint32_t                        index_count;
    uint32_t *indexes;
    //
    // Block variable ac is pointing to (for block references)
    SpvReflectBlockVariable *block_var;
} SpvReflectPrvAccessChain;

// To prevent infinite recursion, we never walk down a
// PhysicalStorageBuffer struct twice, but incase a 2nd variable
// needs to use that struct, save a copy
typedef struct SpvReflectPrvPhysicalPointerStruct {
    uint32_t struct_id;
    // first variable to see the PhysicalStorageBuffer struct
    SpvReflectBlockVariable *p_var;
} SpvReflectPrvPhysicalPointerStruct;

typedef struct SpvReflectPrvParser {
    size_t                          spirv_word_count;
    uint32_t *spirv_code;
    uint32_t                        string_count;
    SpvReflectPrvString *strings;
    SpvSourceLanguage               source_language;
    uint32_t                        source_language_version;
    uint32_t                        source_file_id;
    const char *source_embedded;
    size_t                          node_count;
    SpvReflectPrvNode *nodes;
    uint32_t                        entry_point_count;
    uint32_t                        capability_count;
    uint32_t                        function_count;
    SpvReflectPrvFunction *functions;
    uint32_t                        access_chain_count;
    SpvReflectPrvAccessChain *access_chains;

    uint32_t                        type_count;
    uint32_t                        descriptor_count;
    uint32_t                        push_constant_count;

    SpvReflectTypeDescription *physical_pointer_check[MAX_RECURSIVE_PHYSICAL_POINTER_CHECK];
    uint32_t                        physical_pointer_count;

    SpvReflectPrvPhysicalPointerStruct *physical_pointer_structs;
    uint32_t                            physical_pointer_struct_count;
} SpvReflectPrvParser;
// clang-format on

static uint32_t Max(uint32_t a, uint32_t b) { return a > b ? a : b; }
static uint32_t Min(uint32_t a, uint32_t b) { return a < b ? a : b; }

static uint32_t RoundUp(uint32_t value, uint32_t multiple) {
    assert(multiple && ((multiple & (multiple - 1)) == 0));
    return (value + multiple - 1) & ~(multiple - 1);
}

#define IsNull(ptr) (ptr == NULL)

#define IsNotNull(ptr) (ptr != NULL)

#define SafeFree(ptr) \
  {                   \
    free((void*)ptr); \
    ptr = NULL;       \
  }

static int SortCompareUint32(const void *a, const void *b) {
    const uint32_t *p_a = (const uint32_t *)a;
    const uint32_t *p_b = (const uint32_t *)b;

    return (int)*p_a - (int)*p_b;
}

static int SortCompareAccessedVariable(const void *a, const void *b) {
    const SpvReflectPrvAccessedVariable *p_a = (const SpvReflectPrvAccessedVariable *)a;
    const SpvReflectPrvAccessedVariable *p_b = (const SpvReflectPrvAccessedVariable *)b;

    return (int)p_a->variable_ptr - (int)p_b->variable_ptr;
}

//
// De-duplicates a sorted array and returns the new size.
//
// Note: The array doesn't actually need to be sorted, just
// arranged into "runs" so that all the entries with one
// value are adjacent.
//
static size_t DedupSortedUint32(uint32_t *arr, size_t size) {
    if (size == 0) {
        return 0;
    }
    size_t dedup_idx = 0;
    for (size_t i = 0; i < size; ++i) {
        if (arr[dedup_idx] != arr[i]) {
            ++dedup_idx;
            arr[dedup_idx] = arr[i];
        }
    }
    return dedup_idx + 1;
}

static bool SearchSortedUint32(const uint32_t *arr, size_t size, uint32_t target) {
    size_t lo = 0;
    size_t hi = size;
    while (lo < hi) {
        size_t mid = (hi - lo) / 2 + lo;
        if (arr[mid] == target) {
            return true;
        } else if (arr[mid] < target) {
            lo = mid + 1;
        } else {
            hi = mid;
        }
    }
    return false;
}

static SpvReflectResult IntersectSortedAccessedVariable(const SpvReflectPrvAccessedVariable *p_arr0, size_t arr0_size,
    const uint32_t *p_arr1, size_t arr1_size, uint32_t **pp_res,
    size_t *res_size) {
    *pp_res = NULL;
    *res_size = 0;
    if (IsNull(p_arr0) || IsNull(p_arr1)) {
        return SPV_REFLECT_RESULT_SUCCESS;
    }

    const SpvReflectPrvAccessedVariable *p_arr0_end = p_arr0 + arr0_size;
    const uint32_t *p_arr1_end = p_arr1 + arr1_size;

    const SpvReflectPrvAccessedVariable *p_idx0 = p_arr0;
    const uint32_t *p_idx1 = p_arr1;
    while (p_idx0 != p_arr0_end && p_idx1 != p_arr1_end) {
        if (p_idx0->variable_ptr < *p_idx1) {
            ++p_idx0;
        } else if (p_idx0->variable_ptr > *p_idx1) {
            ++p_idx1;
        } else {
            ++*res_size;
            ++p_idx0;
            ++p_idx1;
        }
    }

    if (*res_size > 0) {
        *pp_res = (uint32_t *)calloc(*res_size, sizeof(**pp_res));
        if (IsNull(*pp_res)) {
            return SPV_REFLECT_RESULT_ERROR_ALLOC_FAILED;
        }
        uint32_t *p_idxr = *pp_res;
        p_idx0 = p_arr0;
        p_idx1 = p_arr1;
        while (p_idx0 != p_arr0_end && p_idx1 != p_arr1_end) {
            if (p_idx0->variable_ptr < *p_idx1) {
                ++p_idx0;
            } else if (p_idx0->variable_ptr > *p_idx1) {
                ++p_idx1;
            } else {
                *(p_idxr++) = p_idx0->variable_ptr;
                ++p_idx0;
                ++p_idx1;
            }
        }
    }
    return SPV_REFLECT_RESULT_SUCCESS;
}

static bool InRange(const SpvReflectPrvParser *p_parser, uint32_t index) {
    bool in_range = false;
    if (IsNotNull(p_parser)) {
        in_range = (index < p_parser->spirv_word_count);
    }
    return in_range;
}

static SpvReflectResult ReadU32(SpvReflectPrvParser *p_parser, uint32_t word_offset, uint32_t *p_value) {
    assert(IsNotNull(p_parser));
    assert(IsNotNull(p_parser->spirv_code));
    assert(InRange(p_parser, word_offset));
    SpvReflectResult result = SPV_REFLECT_RESULT_ERROR_SPIRV_UNEXPECTED_EOF;
    if (IsNotNull(p_parser) && IsNotNull(p_parser->spirv_code) && InRange(p_parser, word_offset)) {
        *p_value = *(p_parser->spirv_code + word_offset);
        result = SPV_REFLECT_RESULT_SUCCESS;
    }
    return result;
}

#define UNCHECKED_READU32(parser, word_offset, value) \
  { (void)ReadU32(parser, word_offset, (uint32_t*)&(value)); }

#define CHECKED_READU32(parser, word_offset, value)                                              \
  {                                                                                              \
    SpvReflectResult checked_readu32_result = ReadU32(parser, word_offset, (uint32_t*)&(value)); \
    if (checked_readu32_result != SPV_REFLECT_RESULT_SUCCESS) {                                  \
      return checked_readu32_result;                                                             \
    }                                                                                            \
  }

#define CHECKED_READU32_CAST(parser, word_offset, cast_to_type, value)                                                   \
  {                                                                                                                      \
    uint32_t checked_readu32_cast_u32 = UINT32_MAX;                                                                      \
    SpvReflectResult checked_readu32_cast_result = ReadU32(parser, word_offset, (uint32_t*)&(checked_readu32_cast_u32)); \
    if (checked_readu32_cast_result != SPV_REFLECT_RESULT_SUCCESS) {                                                     \
      return checked_readu32_cast_result;                                                                                \
    }                                                                                                                    \
    value = (cast_to_type)checked_readu32_cast_u32;                                                                      \
  }

#define IF_READU32(result, parser, word_offset, value)          \
  if ((result) == SPV_REFLECT_RESULT_SUCCESS) {                 \
    result = ReadU32(parser, word_offset, (uint32_t*)&(value)); \
  }

#define IF_READU32_CAST(result, parser, word_offset, cast_to_type, value) \
  if ((result) == SPV_REFLECT_RESULT_SUCCESS) {                           \
    uint32_t if_readu32_cast_u32 = UINT32_MAX;                            \
    result = ReadU32(parser, word_offset, &if_readu32_cast_u32);          \
    if ((result) == SPV_REFLECT_RESULT_SUCCESS) {                         \
      value = (cast_to_type)if_readu32_cast_u32;                          \
    }                                                                     \
  }

static SpvReflectResult ReadStr(SpvReflectPrvParser *p_parser, uint32_t word_offset, uint32_t word_index, uint32_t word_count,
    uint32_t *p_buf_size, char *p_buf) {
    uint32_t limit = (word_offset + word_count);
    assert(IsNotNull(p_parser));
    assert(IsNotNull(p_parser->spirv_code));
    assert(InRange(p_parser, limit));
    SpvReflectResult result = SPV_REFLECT_RESULT_ERROR_SPIRV_UNEXPECTED_EOF;
    if (IsNotNull(p_parser) && IsNotNull(p_parser->spirv_code) && InRange(p_parser, limit)) {
        const char *c_str = (const char *)(p_parser->spirv_code + word_offset + word_index);
        uint32_t n = word_count * SPIRV_WORD_SIZE;
        uint32_t length_with_terminator = 0;
        for (uint32_t i = 0; i < n; ++i) {
            char c = *(c_str + i);
            if (c == 0) {
                length_with_terminator = i + 1;
                break;
            }
        }

        if (length_with_terminator > 0) {
            result = SPV_REFLECT_RESULT_ERROR_NULL_POINTER;
            if (IsNotNull(p_buf_size) && IsNotNull(p_buf)) {
                result = SPV_REFLECT_RESULT_ERROR_RANGE_EXCEEDED;
                if (length_with_terminator <= *p_buf_size) {
                    memset(p_buf, 0, *p_buf_size);
                    memcpy(p_buf, c_str, length_with_terminator);
                    result = SPV_REFLECT_RESULT_SUCCESS;
                }
            } else {
                if (IsNotNull(p_buf_size)) {
                    *p_buf_size = length_with_terminator;
                    result = SPV_REFLECT_RESULT_SUCCESS;
                }
            }
        }
    }
    return result;
}

static SpvReflectDecorationFlags ApplyDecorations(const SpvReflectPrvDecorations *p_decoration_fields) {
    SpvReflectDecorationFlags decorations = SPV_REFLECT_DECORATION_NONE;
    if (p_decoration_fields->is_relaxed_precision) {
        decorations |= SPV_REFLECT_DECORATION_RELAXED_PRECISION;
    }
    if (p_decoration_fields->is_block) {
        decorations |= SPV_REFLECT_DECORATION_BLOCK;
    }
    if (p_decoration_fields->is_buffer_block) {
        decorations |= SPV_REFLECT_DECORATION_BUFFER_BLOCK;
    }
    if (p_decoration_fields->is_row_major) {
        decorations |= SPV_REFLECT_DECORATION_ROW_MAJOR;
    }
    if (p_decoration_fields->is_column_major) {
        decorations |= SPV_REFLECT_DECORATION_COLUMN_MAJOR;
    }
    if (p_decoration_fields->is_built_in) {
        decorations |= SPV_REFLECT_DECORATION_BUILT_IN;
    }
    if (p_decoration_fields->is_noperspective) {
        decorations |= SPV_REFLECT_DECORATION_NOPERSPECTIVE;
    }
    if (p_decoration_fields->is_flat) {
        decorations |= SPV_REFLECT_DECORATION_FLAT;
    }
    if (p_decoration_fields->is_non_writable) {
        decorations |= SPV_REFLECT_DECORATION_NON_WRITABLE;
    }
    if (p_decoration_fields->is_non_readable) {
        decorations |= SPV_REFLECT_DECORATION_NON_READABLE;
    }
    if (p_decoration_fields->is_patch) {
        decorations |= SPV_REFLECT_DECORATION_PATCH;
    }
    if (p_decoration_fields->is_per_vertex) {
        decorations |= SPV_REFLECT_DECORATION_PER_VERTEX;
    }
    if (p_decoration_fields->is_per_task) {
        decorations |= SPV_REFLECT_DECORATION_PER_TASK;
    }
    if (p_decoration_fields->is_weight_texture) {
        decorations |= SPV_REFLECT_DECORATION_WEIGHT_TEXTURE;
    }
    if (p_decoration_fields->is_block_match_texture) {
        decorations |= SPV_REFLECT_DECORATION_BLOCK_MATCH_TEXTURE;
    }
    return decorations;
}

static void ApplyNumericTraits(const SpvReflectTypeDescription *p_type, SpvReflectNumericTraits *p_numeric_traits) {
    memcpy(p_numeric_traits, &p_type->traits.numeric, sizeof(p_type->traits.numeric));
}

static void ApplyArrayTraits(const SpvReflectTypeDescription *p_type, SpvReflectArrayTraits *p_array_traits) {
    memcpy(p_array_traits, &p_type->traits.array, sizeof(p_type->traits.array));
}

static bool IsSpecConstant(const SpvReflectPrvNode *p_node) {
    return (p_node->op == SpvOpSpecConstant || p_node->op == SpvOpSpecConstantOp || p_node->op == SpvOpSpecConstantTrue ||
        p_node->op == SpvOpSpecConstantFalse);
}

static SpvReflectPrvNode *FindNode(SpvReflectPrvParser *p_parser, uint32_t result_id) {
    SpvReflectPrvNode *p_node = NULL;
    for (size_t i = 0; i < p_parser->node_count; ++i) {
        SpvReflectPrvNode *p_elem = &(p_parser->nodes[i]);
        if (p_elem->result_id == result_id) {
            p_node = p_elem;
            break;
        }
    }
    return p_node;
}

static SpvReflectTypeDescription *FindType(SpvReflectShaderModule *p_module, uint32_t type_id) {
    SpvReflectTypeDescription *p_type = NULL;
    for (size_t i = 0; i < p_module->_internal->type_description_count; ++i) {
        SpvReflectTypeDescription *p_elem = &(p_module->_internal->type_descriptions[i]);
        if (p_elem->id == type_id) {
            p_type = p_elem;
            break;
        }
    }
    return p_type;
}

static SpvReflectPrvAccessChain *FindAccessChain(SpvReflectPrvParser *p_parser, uint32_t id) {
    const uint32_t ac_count = p_parser->access_chain_count;
    for (uint32_t i = 0; i < ac_count; i++) {
        if (p_parser->access_chains[i].result_id == id) {
            return &p_parser->access_chains[i];
        }
    }
    return 0;
}

// Access Chains mostly have their Base ID pointed directly to a OpVariable, but sometimes
// it will be through a load and this funciton handles the edge cases how to find that
static uint32_t FindAccessChainBaseVariable(SpvReflectPrvParser *p_parser, SpvReflectPrvAccessChain *p_access_chain) {
    uint32_t base_id = p_access_chain->base_id;
    SpvReflectPrvNode *base_node = FindNode(p_parser, base_id);
    // TODO - This is just a band-aid to fix crashes.
    // Need to understand why here and hopefully remove
    // https://github.com/KhronosGroup/SPIRV-Reflect/pull/206
    if (IsNull(base_node)) {
        return 0;
    }
    while (base_node->op != SpvOpVariable) {
        switch (base_node->op) {
        case SpvOpLoad: {
            UNCHECKED_READU32(p_parser, base_node->word_offset + 3, base_id);
        } break;
        case SpvOpFunctionParameter: {
            UNCHECKED_READU32(p_parser, base_node->word_offset + 2, base_id);
        } break;
        case SpvOpBitcast:
            // This can be caused by something like GL_EXT_buffer_reference_uvec2 trying to load a pointer.
            // We currently call from a push constant, so no way to have a reference loop back into the PC block
            return 0;
        default: {
            assert(false);
        } break;
        }

        SpvReflectPrvAccessChain *base_ac = FindAccessChain(p_parser, base_id);
        if (base_ac == 0) {
            return 0;
        }
        base_id = base_ac->base_id;
        base_node = FindNode(p_parser, base_id);
        if (IsNull(base_node)) {
            return 0;
        }
    }
    return base_id;
}

static SpvReflectBlockVariable *GetRefBlkVar(SpvReflectPrvParser *p_parser, SpvReflectPrvAccessChain *p_access_chain) {
    uint32_t base_id = p_access_chain->base_id;
    SpvReflectPrvNode *base_node = FindNode(p_parser, base_id);
    assert(base_node->op == SpvOpLoad);
    UNCHECKED_READU32(p_parser, base_node->word_offset + 3, base_id);
    SpvReflectPrvAccessChain *base_ac = FindAccessChain(p_parser, base_id);
    assert(base_ac != 0);
    SpvReflectBlockVariable *base_var = base_ac->block_var;
    assert(base_var != 0);
    return base_var;
}

bool IsPointerToPointer(SpvReflectPrvParser *p_parser, uint32_t type_id) {
    SpvReflectPrvNode *ptr_node = FindNode(p_parser, type_id);
    if (IsNull(ptr_node) || (ptr_node->op != SpvOpTypePointer)) {
        return false;
    }
    uint32_t pte_id = 0;
    UNCHECKED_READU32(p_parser, ptr_node->word_offset + 3, pte_id);
    SpvReflectPrvNode *pte_node = FindNode(p_parser, pte_id);
    if (IsNull(pte_node)) {
        return false;
    }
    return pte_node->op == SpvOpTypePointer;
}

static SpvReflectResult CreateParser(size_t size, void *p_code, SpvReflectPrvParser *p_parser) {
    if (p_code == NULL) {
        return SPV_REFLECT_RESULT_ERROR_NULL_POINTER;
    }

    if (size < SPIRV_MINIMUM_FILE_SIZE) {
        return SPV_REFLECT_RESULT_ERROR_SPIRV_INVALID_CODE_SIZE;
    }
    if ((size % 4) != 0) {
        return SPV_REFLECT_RESULT_ERROR_SPIRV_INVALID_CODE_SIZE;
    }

    p_parser->spirv_word_count = size / SPIRV_WORD_SIZE;
    p_parser->spirv_code = (uint32_t *)p_code;

    if (p_parser->spirv_code[0] != SpvMagicNumber) {
        return SPV_REFLECT_RESULT_ERROR_SPIRV_INVALID_MAGIC_NUMBER;
    }

    return SPV_REFLECT_RESULT_SUCCESS;
}

static void DestroyParser(SpvReflectPrvParser *p_parser) {
    if (!IsNull(p_parser->nodes)) {
        // Free nodes
        for (size_t i = 0; i < p_parser->node_count; ++i) {
            SpvReflectPrvNode *p_node = &(p_parser->nodes[i]);
            if (IsNotNull(p_node->member_names)) {
                SafeFree(p_node->member_names);
            }
            if (IsNotNull(p_node->member_decorations)) {
                SafeFree(p_node->member_decorations);
            }
        }

        // Free functions
        for (size_t i = 0; i < p_parser->function_count; ++i) {
            SafeFree(p_parser->functions[i].parameters);
            SafeFree(p_parser->functions[i].callees);
            SafeFree(p_parser->functions[i].callee_ptrs);
            SafeFree(p_parser->functions[i].accessed_variables);
        }

        // Free access chains
        for (uint32_t i = 0; i < p_parser->access_chain_count; ++i) {
            SafeFree(p_parser->access_chains[i].indexes);
        }

        SafeFree(p_parser->nodes);
        SafeFree(p_parser->strings);
        SafeFree(p_parser->source_embedded);
        SafeFree(p_parser->functions);
        SafeFree(p_parser->access_chains);

        if (IsNotNull(p_parser->physical_pointer_structs)) {
            SafeFree(p_parser->physical_pointer_structs);
        }
        p_parser->node_count = 0;
    }
}

static SpvReflectResult ParseNodes(SpvReflectPrvParser *p_parser) {
    assert(IsNotNull(p_parser));
    assert(IsNotNull(p_parser->spirv_code));

    uint32_t *p_spirv = p_parser->spirv_code;
    uint32_t spirv_word_index = SPIRV_STARTING_WORD_INDEX;

    // Count nodes
    uint32_t node_count = 0;
    while (spirv_word_index < p_parser->spirv_word_count) {
        uint32_t word = p_spirv[spirv_word_index];
        SpvOp op = (SpvOp)(word & 0xFFFF);
        uint32_t node_word_count = (word >> 16) & 0xFFFF;
        if (node_word_count == 0) {
            return SPV_REFLECT_RESULT_ERROR_SPIRV_INVALID_INSTRUCTION;
        }
        if (op == SpvOpAccessChain) {
            ++(p_parser->access_chain_count);
        }
        spirv_word_index += node_word_count;
        ++node_count;
    }

    if (node_count == 0) {
        return SPV_REFLECT_RESULT_ERROR_SPIRV_UNEXPECTED_EOF;
    }

    // Allocate nodes
    p_parser->node_count = node_count;
    p_parser->nodes = (SpvReflectPrvNode *)calloc(p_parser->node_count, sizeof(*(p_parser->nodes)));
    if (IsNull(p_parser->nodes)) {
        return SPV_REFLECT_RESULT_ERROR_ALLOC_FAILED;
    }
    // Mark all nodes with an invalid state
    for (uint32_t i = 0; i < node_count; ++i) {
        p_parser->nodes[i].op = (SpvOp)INVALID_VALUE;
        p_parser->nodes[i].storage_class = (SpvStorageClass)INVALID_VALUE;
        p_parser->nodes[i].decorations.set.value = (uint32_t)INVALID_VALUE;
        p_parser->nodes[i].decorations.binding.value = (uint32_t)INVALID_VALUE;
        p_parser->nodes[i].decorations.location.value = (uint32_t)INVALID_VALUE;
        p_parser->nodes[i].decorations.component.value = (uint32_t)INVALID_VALUE;
        p_parser->nodes[i].decorations.offset.value = (uint32_t)INVALID_VALUE;
        p_parser->nodes[i].decorations.uav_counter_buffer.value = (uint32_t)INVALID_VALUE;
        p_parser->nodes[i].decorations.spec_id = (uint32_t)INVALID_VALUE;
        p_parser->nodes[i].decorations.built_in = (SpvBuiltIn)INVALID_VALUE;
    }
    // Mark source file id node
    p_parser->source_file_id = (uint32_t)INVALID_VALUE;
    p_parser->source_embedded = NULL;

    // Function node
    uint32_t function_node = (uint32_t)INVALID_VALUE;

    // Allocate access chain
    if (p_parser->access_chain_count > 0) {
        p_parser->access_chains = (SpvReflectPrvAccessChain *)calloc(p_parser->access_chain_count, sizeof(*(p_parser->access_chains)));
        if (IsNull(p_parser->access_chains)) {
            return SPV_REFLECT_RESULT_ERROR_ALLOC_FAILED;
        }
    }

    // Parse nodes
    uint32_t node_index = 0;
    uint32_t access_chain_index = 0;
    spirv_word_index = SPIRV_STARTING_WORD_INDEX;
    while (spirv_word_index < p_parser->spirv_word_count) {
        uint32_t word = p_spirv[spirv_word_index];
        SpvOp op = (SpvOp)(word & 0xFFFF);
        uint32_t node_word_count = (word >> 16) & 0xFFFF;

        SpvReflectPrvNode *p_node = &(p_parser->nodes[node_index]);
        p_node->op = op;
        p_node->word_offset = spirv_word_index;
        p_node->word_count = node_word_count;

        switch (p_node->op) {
        default:
            break;

        case SpvOpString: {
            ++(p_parser->string_count);
        } break;

        case SpvOpSource: {
            CHECKED_READU32_CAST(p_parser, p_node->word_offset + 1, SpvSourceLanguage, p_parser->source_language);
            CHECKED_READU32(p_parser, p_node->word_offset + 2, p_parser->source_language_version);
            if (p_node->word_count >= 4) {
                CHECKED_READU32(p_parser, p_node->word_offset + 3, p_parser->source_file_id);
            }
            if (p_node->word_count >= 5) {
                const char *p_source = (const char *)(p_parser->spirv_code + p_node->word_offset + 4);

                const size_t source_len = strlen(p_source);
                char *p_source_temp = (char *)calloc(source_len + 1, sizeof(char));

                if (IsNull(p_source_temp)) {
                    return SPV_REFLECT_RESULT_ERROR_ALLOC_FAILED;
                }

#ifdef _WIN32
                strcpy_s(p_source_temp, source_len + 1, p_source);
#else
                strcpy(p_source_temp, p_source);
#endif

                SafeFree(p_parser->source_embedded);
                p_parser->source_embedded = p_source_temp;
            }
        } break;

        case SpvOpSourceContinued: {
            const char *p_source = (const char *)(p_parser->spirv_code + p_node->word_offset + 1);

            const size_t source_len = strlen(p_source);
            const size_t embedded_source_len = strlen(p_parser->source_embedded);
            char *p_continued_source = (char *)calloc(source_len + embedded_source_len + 1, sizeof(char));

            if (IsNull(p_continued_source)) {
                return SPV_REFLECT_RESULT_ERROR_ALLOC_FAILED;
            }

#ifdef _WIN32
            strcpy_s(p_continued_source, embedded_source_len + 1, p_parser->source_embedded);
            strcat_s(p_continued_source, embedded_source_len + source_len + 1, p_source);
#else
            strcpy(p_continued_source, p_parser->source_embedded);
            strcat(p_continued_source, p_source);
#endif

            SafeFree(p_parser->source_embedded);
            p_parser->source_embedded = p_continued_source;
        } break;

        case SpvOpEntryPoint: {
            ++(p_parser->entry_point_count);
        } break;

        case SpvOpCapability: {
            CHECKED_READU32(p_parser, p_node->word_offset + 1, p_node->capability);
            ++(p_parser->capability_count);
        } break;

        case SpvOpName:
        case SpvOpMemberName: {
            uint32_t member_offset = (p_node->op == SpvOpMemberName) ? 1 : 0;
            uint32_t name_start = p_node->word_offset + member_offset + 2;
            p_node->name = (const char *)(p_parser->spirv_code + name_start);
        } break;

        case SpvOpTypeStruct: {
            p_node->member_count = p_node->word_count - 2;
            FALLTHROUGH;
        }  // Fall through

        // This is all the rest of OpType* that need to be tracked
        // Possible new extensions might expose new type, will need to be added
        // here
        case SpvOpTypeVoid:
        case SpvOpTypeBool:
        case SpvOpTypeInt:
        case SpvOpTypeFloat:
        case SpvOpTypeVector:
        case SpvOpTypeMatrix:
        case SpvOpTypeSampler:
        case SpvOpTypeOpaque:
        case SpvOpTypeFunction:
        case SpvOpTypeEvent:
        case SpvOpTypeDeviceEvent:
        case SpvOpTypeReserveId:
        case SpvOpTypeQueue:
        case SpvOpTypePipe:
        case SpvOpTypeAccelerationStructureKHR:
        case SpvOpTypeRayQueryKHR:
        case SpvOpTypeHitObjectNV:
        case SpvOpTypeCooperativeMatrixNV:
        case SpvOpTypeCooperativeMatrixKHR: {
            CHECKED_READU32(p_parser, p_node->word_offset + 1, p_node->result_id);
            p_node->is_type = true;
        } break;

        case SpvOpTypeImage: {
            CHECKED_READU32(p_parser, p_node->word_offset + 1, p_node->result_id);
            CHECKED_READU32(p_parser, p_node->word_offset + 2, p_node->image_traits.sampled_type_id);
            CHECKED_READU32(p_parser, p_node->word_offset + 3, p_node->image_traits.dim);
            CHECKED_READU32(p_parser, p_node->word_offset + 4, p_node->image_traits.depth);
            CHECKED_READU32(p_parser, p_node->word_offset + 5, p_node->image_traits.arrayed);
            CHECKED_READU32(p_parser, p_node->word_offset + 6, p_node->image_traits.ms);
            CHECKED_READU32(p_parser, p_node->word_offset + 7, p_node->image_traits.sampled);
            CHECKED_READU32(p_parser, p_node->word_offset + 8, p_node->image_traits.image_format);
            p_node->is_type = true;
        } break;

        case SpvOpTypeSampledImage: {
            CHECKED_READU32(p_parser, p_node->word_offset + 1, p_node->result_id);
            CHECKED_READU32(p_parser, p_node->word_offset + 2, p_node->image_type_id);
            p_node->is_type = true;
        } break;

        case SpvOpTypeArray: {
            CHECKED_READU32(p_parser, p_node->word_offset + 1, p_node->result_id);
            CHECKED_READU32(p_parser, p_node->word_offset + 2, p_node->array_traits.element_type_id);
            CHECKED_READU32(p_parser, p_node->word_offset + 3, p_node->array_traits.length_id);
            p_node->is_type = true;
        } break;

        case SpvOpTypeRuntimeArray: {
            CHECKED_READU32(p_parser, p_node->word_offset + 1, p_node->result_id);
            CHECKED_READU32(p_parser, p_node->word_offset + 2, p_node->array_traits.element_type_id);
            p_node->is_type = true;
        } break;

        case SpvOpTypePointer: {
            uint32_t result_id;
            CHECKED_READU32(p_parser, p_node->word_offset + 1, result_id);
            // Look for forward pointer. Clear result id if found
            SpvReflectPrvNode *p_fwd_node = FindNode(p_parser, result_id);
            if (p_fwd_node) {
                p_fwd_node->result_id = 0;
            }
            // Register pointer type
            p_node->result_id = result_id;
            CHECKED_READU32(p_parser, p_node->word_offset + 2, p_node->storage_class);
            CHECKED_READU32(p_parser, p_node->word_offset + 3, p_node->type_id);
            p_node->is_type = true;
        } break;

        case SpvOpTypeForwardPointer: {
            CHECKED_READU32(p_parser, p_node->word_offset + 1, p_node->result_id);
            CHECKED_READU32(p_parser, p_node->word_offset + 2, p_node->storage_class);
            p_node->is_type = true;
        } break;

        case SpvOpConstantTrue:
        case SpvOpConstantFalse:
        case SpvOpConstant:
        case SpvOpConstantComposite:
        case SpvOpConstantSampler:
        case SpvOpConstantNull: {
            CHECKED_READU32(p_parser, p_node->word_offset + 1, p_node->result_type_id);
            CHECKED_READU32(p_parser, p_node->word_offset + 2, p_node->result_id);
        } break;

        case SpvOpSpecConstantTrue:
        case SpvOpSpecConstantFalse:
        case SpvOpSpecConstant:
        case SpvOpSpecConstantComposite:
        case SpvOpSpecConstantOp: {
            CHECKED_READU32(p_parser, p_node->word_offset + 1, p_node->result_type_id);
            CHECKED_READU32(p_parser, p_node->word_offset + 2, p_node->result_id);
        } break;

        case SpvOpVariable: {
            CHECKED_READU32(p_parser, p_node->word_offset + 1, p_node->type_id);
            CHECKED_READU32(p_parser, p_node->word_offset + 2, p_node->result_id);
            CHECKED_READU32(p_parser, p_node->word_offset + 3, p_node->storage_class);
        } break;

        case SpvOpLoad: {
            // Only load enough so OpDecorate can reference the node, skip the remaining operands.
            CHECKED_READU32(p_parser, p_node->word_offset + 1, p_node->result_type_id);
            CHECKED_READU32(p_parser, p_node->word_offset + 2, p_node->result_id);
        } break;

        case SpvOpAccessChain: {
            SpvReflectPrvAccessChain *p_access_chain = &(p_parser->access_chains[access_chain_index]);
            CHECKED_READU32(p_parser, p_node->word_offset + 1, p_access_chain->result_type_id);
            CHECKED_READU32(p_parser, p_node->word_offset + 2, p_access_chain->result_id);
            CHECKED_READU32(p_parser, p_node->word_offset + 3, p_access_chain->base_id);
            //
            // SPIRV_ACCESS_CHAIN_INDEX_OFFSET (4) is the number of words up until the first index:
            //   [Node, Result Type Id, Result Id, Base Id, <Indexes>]
            //
            p_access_chain->index_count = (node_word_count - SPIRV_ACCESS_CHAIN_INDEX_OFFSET);
            if (p_access_chain->index_count > 0) {
                p_access_chain->indexes = (uint32_t *)calloc(p_access_chain->index_count, sizeof(*(p_access_chain->indexes)));
                if (IsNull(p_access_chain->indexes)) {
                    return SPV_REFLECT_RESULT_ERROR_ALLOC_FAILED;
                }
                // Parse any index values for access chain
                for (uint32_t index_index = 0; index_index < p_access_chain->index_count; ++index_index) {
                    // Read index id
                    uint32_t index_id = 0;
                    CHECKED_READU32(p_parser, p_node->word_offset + SPIRV_ACCESS_CHAIN_INDEX_OFFSET + index_index, index_id);
                    // Find OpConstant node that contains index value
                    SpvReflectPrvNode *p_index_value_node = FindNode(p_parser, index_id);
                    if ((p_index_value_node != NULL) &&
                        (p_index_value_node->op == SpvOpConstant || p_index_value_node->op == SpvOpSpecConstant)) {
                        // Read index value
                        uint32_t index_value = UINT32_MAX;
                        CHECKED_READU32(p_parser, p_index_value_node->word_offset + 3, index_value);
                        assert(index_value != UINT32_MAX);
                        // Write index value to array
                        p_access_chain->indexes[index_index] = index_value;
                    }
                }
            }
            ++access_chain_index;
        } break;

        case SpvOpFunction: {
            CHECKED_READU32(p_parser, p_node->word_offset + 2, p_node->result_id);
            // Count function definitions, not function declarations.  To determine
            // the difference, set an in-function variable, and then if an OpLabel
            // is reached before the end of the function increment the function
            // count.
            function_node = node_index;
        } break;

        case SpvOpLabel: {
            if (function_node != (uint32_t)INVALID_VALUE) {
                SpvReflectPrvNode *p_func_node = &(p_parser->nodes[function_node]);
                CHECKED_READU32(p_parser, p_func_node->word_offset + 2, p_func_node->result_id);
                ++(p_parser->function_count);
            }
            FALLTHROUGH;
        }  // Fall through

        case SpvOpFunctionEnd: {
            function_node = (uint32_t)INVALID_VALUE;
        } break;
        case SpvOpFunctionParameter: {
            CHECKED_READU32(p_parser, p_node->word_offset + 2, p_node->result_id);
        } break;
        case SpvOpBitcast:
        case SpvOpShiftRightLogical:
        case SpvOpIAdd:
        case SpvOpISub:
        case SpvOpIMul:
        case SpvOpUDiv:
        case SpvOpSDiv: {
            CHECKED_READU32(p_parser, p_node->word_offset + 2, p_node->result_id);
        } break;
        }

        if (p_node->is_type) {
            ++(p_parser->type_count);
        }

        spirv_word_index += node_word_count;
        ++node_index;
    }

    return SPV_REFLECT_RESULT_SUCCESS;
}

static SpvReflectResult ParseStrings(SpvReflectPrvParser *p_parser) {
    assert(IsNotNull(p_parser));
    assert(IsNotNull(p_parser->spirv_code));
    assert(IsNotNull(p_parser->nodes));

    // Early out
    if (p_parser->string_count == 0) {
        return SPV_REFLECT_RESULT_SUCCESS;
    }

    if (IsNotNull(p_parser) && IsNotNull(p_parser->spirv_code) && IsNotNull(p_parser->nodes)) {
        // Allocate string storage
        p_parser->strings = (SpvReflectPrvString *)calloc(p_parser->string_count, sizeof(*(p_parser->strings)));

        uint32_t string_index = 0;
        for (size_t i = 0; i < p_parser->node_count; ++i) {
            SpvReflectPrvNode *p_node = &(p_parser->nodes[i]);
            if (p_node->op != SpvOpString) {
                continue;
            }

            // Paranoid check against string count
            assert(string_index < p_parser->string_count);
            if (string_index >= p_parser->string_count) {
                return SPV_REFLECT_RESULT_ERROR_COUNT_MISMATCH;
            }

            // Result id
            SpvReflectPrvString *p_string = &(p_parser->strings[string_index]);
            CHECKED_READU32(p_parser, p_node->word_offset + 1, p_string->result_id);

            // String
            uint32_t string_start = p_node->word_offset + 2;
            p_string->string = (const char *)(p_parser->spirv_code + string_start);

            // Increment string index
            ++string_index;
        }
    }

    return SPV_REFLECT_RESULT_SUCCESS;
}

static SpvReflectResult ParseSource(SpvReflectPrvParser *p_parser, SpvReflectShaderModule *p_module) {
    assert(IsNotNull(p_parser));
    assert(IsNotNull(p_parser->spirv_code));

    if (IsNotNull(p_parser) && IsNotNull(p_parser->spirv_code)) {
        // Source file
        if (IsNotNull(p_parser->strings)) {
            for (uint32_t i = 0; i < p_parser->string_count; ++i) {
                SpvReflectPrvString *p_string = &(p_parser->strings[i]);
                if (p_string->result_id == p_parser->source_file_id) {
                    p_module->source_file = p_string->string;
                    break;
                }
            }
        }

        // Source code
        if (IsNotNull(p_parser->source_embedded)) {
            const size_t source_len = strlen(p_parser->source_embedded);
            char *p_source = (char *)calloc(source_len + 1, sizeof(char));

            if (IsNull(p_source)) {
                return SPV_REFLECT_RESULT_ERROR_ALLOC_FAILED;
            }

#ifdef _WIN32
            strcpy_s(p_source, source_len + 1, p_parser->source_embedded);
#else
            strcpy(p_source, p_parser->source_embedded);
#endif

            p_module->source_source = p_source;
        }
    }

    return SPV_REFLECT_RESULT_SUCCESS;
}

static SpvReflectResult ParseFunction(SpvReflectPrvParser *p_parser, SpvReflectPrvNode *p_func_node, SpvReflectPrvFunction *p_func,
    size_t first_label_index) {
    p_func->id = p_func_node->result_id;

    p_func->parameter_count = 0;
    p_func->callee_count = 0;
    p_func->accessed_variable_count = 0;

    // First get count to know how much to allocate
    for (size_t i = first_label_index; i < p_parser->node_count; ++i) {
        SpvReflectPrvNode *p_node = &(p_parser->nodes[i]);
        if (p_node->op == SpvOpFunctionEnd) {
            break;
        }
        switch (p_node->op) {
        case SpvOpFunctionParameter: {
            ++(p_func->parameter_count);
        } break;
        case SpvOpFunctionCall: {
            p_func->accessed_variable_count += p_node->word_count - 4;
            ++(p_func->callee_count);
        } break;
        case SpvOpLoad:
        case SpvOpAccessChain:
        case SpvOpInBoundsAccessChain:
        case SpvOpPtrAccessChain:
        case SpvOpArrayLength:
        case SpvOpGenericPtrMemSemantics:
        case SpvOpInBoundsPtrAccessChain:
        case SpvOpStore:
        case SpvOpImageTexelPointer: {
            ++(p_func->accessed_variable_count);
        } break;
        case SpvOpCopyMemory:
        case SpvOpCopyMemorySized: {
            p_func->accessed_variable_count += 2;
        } break;
        default:
            break;
        }
    }

    if (p_func->parameter_count > 0) {
        p_func->parameters = (uint32_t *)calloc(p_func->parameter_count, sizeof(*(p_func->parameters)));
        if (IsNull(p_func->parameters)) {
            return SPV_REFLECT_RESULT_ERROR_ALLOC_FAILED;
        }
    }

    if (p_func->callee_count > 0) {
        p_func->callees = (uint32_t *)calloc(p_func->callee_count, sizeof(*(p_func->callees)));
        if (IsNull(p_func->callees)) {
            return SPV_REFLECT_RESULT_ERROR_ALLOC_FAILED;
        }
    }

    if (p_func->accessed_variable_count > 0) {
        p_func->accessed_variables =
            (SpvReflectPrvAccessedVariable *)calloc(p_func->accessed_variable_count, sizeof(*(p_func->accessed_variables)));
        if (IsNull(p_func->accessed_variables)) {
            return SPV_REFLECT_RESULT_ERROR_ALLOC_FAILED;
        }
    }

    p_func->parameter_count = 0;
    p_func->callee_count = 0;
    p_func->accessed_variable_count = 0;
    // Now have allocation, fill in values
    for (size_t i = first_label_index; i < p_parser->node_count; ++i) {
        SpvReflectPrvNode *p_node = &(p_parser->nodes[i]);
        if (p_node->op == SpvOpFunctionEnd) {
            break;
        }
        switch (p_node->op) {
        case SpvOpFunctionParameter: {
            CHECKED_READU32(p_parser, p_node->word_offset + 2, p_func->parameters[p_func->parameter_count]);
            (++p_func->parameter_count);
        } break;
        case SpvOpFunctionCall: {
            CHECKED_READU32(p_parser, p_node->word_offset + 3, p_func->callees[p_func->callee_count]);
            const uint32_t result_index = p_node->word_offset + 2;
            for (uint32_t j = 0, parameter_count = p_node->word_count - 4; j < parameter_count; j++) {
                const uint32_t ptr_index = p_node->word_offset + 4 + j;
                SpvReflectPrvAccessedVariable *access_ptr = &p_func->accessed_variables[p_func->accessed_variable_count];

                access_ptr->p_node = p_node;
                // Need to track Result ID as not sure there has been any memory access through here yet
                CHECKED_READU32(p_parser, result_index, access_ptr->result_id);
                CHECKED_READU32(p_parser, ptr_index, access_ptr->variable_ptr);
                access_ptr->function_id = p_func->callees[p_func->callee_count];
                access_ptr->function_parameter_index = j;
                (++p_func->accessed_variable_count);
            }
            (++p_func->callee_count);
        } break;
        case SpvOpLoad:
        case SpvOpAccessChain:
        case SpvOpInBoundsAccessChain:
        case SpvOpPtrAccessChain:
        case SpvOpArrayLength:
        case SpvOpGenericPtrMemSemantics:
        case SpvOpInBoundsPtrAccessChain:
        case SpvOpImageTexelPointer: {
            const uint32_t result_index = p_node->word_offset + 2;
            const uint32_t ptr_index = p_node->word_offset + 3;
            SpvReflectPrvAccessedVariable *access_ptr = &p_func->accessed_variables[p_func->accessed_variable_count];

            access_ptr->p_node = p_node;
            // Need to track Result ID as not sure there has been any memory access through here yet
            CHECKED_READU32(p_parser, result_index, access_ptr->result_id);
            CHECKED_READU32(p_parser, ptr_index, access_ptr->variable_ptr);
            (++p_func->accessed_variable_count);
        } break;
        case SpvOpStore: {
            const uint32_t result_index = p_node->word_offset + 2;
            CHECKED_READU32(p_parser, result_index, p_func->accessed_variables[p_func->accessed_variable_count].variable_ptr);
            p_func->accessed_variables[p_func->accessed_variable_count].p_node = p_node;
            (++p_func->accessed_variable_count);
        } break;
        case SpvOpCopyMemory:
        case SpvOpCopyMemorySized: {
            // There is no result_id or node, being zero is same as being invalid
            CHECKED_READU32(p_parser, p_node->word_offset + 1,
                p_func->accessed_variables[p_func->accessed_variable_count].variable_ptr);
            (++p_func->accessed_variable_count);
            CHECKED_READU32(p_parser, p_node->word_offset + 2,
                p_func->accessed_variables[p_func->accessed_variable_count].variable_ptr);
            (++p_func->accessed_variable_count);
        } break;
        default:
            break;
        }
    }

    if (p_func->callee_count > 0) {
        qsort(p_func->callees, p_func->callee_count, sizeof(*(p_func->callees)), SortCompareUint32);
    }
    p_func->callee_count = (uint32_t)DedupSortedUint32(p_func->callees, p_func->callee_count);

    if (p_func->accessed_variable_count > 0) {
        qsort(p_func->accessed_variables, p_func->accessed_variable_count, sizeof(*(p_func->accessed_variables)),
            SortCompareAccessedVariable);
    }

    return SPV_REFLECT_RESULT_SUCCESS;
}

static int SortCompareFunctions(const void *a, const void *b) {
    const SpvReflectPrvFunction *af = (const SpvReflectPrvFunction *)a;
    const SpvReflectPrvFunction *bf = (const SpvReflectPrvFunction *)b;
    return (int)af->id - (int)bf->id;
}

static SpvReflectResult ParseFunctions(SpvReflectPrvParser *p_parser) {
    assert(IsNotNull(p_parser));
    assert(IsNotNull(p_parser->spirv_code));
    assert(IsNotNull(p_parser->nodes));

    if (IsNotNull(p_parser) && IsNotNull(p_parser->spirv_code) && IsNotNull(p_parser->nodes)) {
        if (p_parser->function_count == 0) {
            return SPV_REFLECT_RESULT_SUCCESS;
        }

        p_parser->functions = (SpvReflectPrvFunction *)calloc(p_parser->function_count, sizeof(*(p_parser->functions)));
        if (IsNull(p_parser->functions)) {
            return SPV_REFLECT_RESULT_ERROR_ALLOC_FAILED;
        }

        size_t function_index = 0;
        for (size_t i = 0; i < p_parser->node_count; ++i) {
            SpvReflectPrvNode *p_node = &(p_parser->nodes[i]);
            if (p_node->op != SpvOpFunction) {
                continue;
            }

            // Skip over function declarations that aren't definitions
            bool func_definition = false;
            for (size_t j = i; j < p_parser->node_count; ++j) {
                if (p_parser->nodes[j].op == SpvOpLabel) {
                    func_definition = true;
                    break;
                }
                if (p_parser->nodes[j].op == SpvOpFunctionEnd) {
                    break;
                }
            }
            if (!func_definition) {
                continue;
            }

            SpvReflectPrvFunction *p_function = &(p_parser->functions[function_index]);

            SpvReflectResult result = ParseFunction(p_parser, p_node, p_function, i);
            if (result != SPV_REFLECT_RESULT_SUCCESS) {
                return result;
            }

            ++function_index;
        }

        qsort(p_parser->functions, p_parser->function_count, sizeof(*(p_parser->functions)), SortCompareFunctions);

        // Once they're sorted, link the functions with pointers to improve graph
        // traversal efficiency
        for (size_t i = 0; i < p_parser->function_count; ++i) {
            SpvReflectPrvFunction *p_func = &(p_parser->functions[i]);
            if (p_func->callee_count == 0) {
                continue;
            }
            p_func->callee_ptrs = (SpvReflectPrvFunction **)calloc(p_func->callee_count, sizeof(*(p_func->callee_ptrs)));
            for (size_t j = 0, k = 0; j < p_func->callee_count; ++j) {
                while (p_parser->functions[k].id != p_func->callees[j]) {
                    ++k;
                    if (k >= p_parser->function_count) {
                        // Invalid called function ID somewhere
                        return SPV_REFLECT_RESULT_ERROR_SPIRV_INVALID_ID_REFERENCE;
                    }
                }
                p_func->callee_ptrs[j] = &(p_parser->functions[k]);
            }
        }
    }

    return SPV_REFLECT_RESULT_SUCCESS;
}

static SpvReflectResult ParseMemberCounts(SpvReflectPrvParser *p_parser) {
    assert(IsNotNull(p_parser));
    assert(IsNotNull(p_parser->spirv_code));
    assert(IsNotNull(p_parser->nodes));

    if (IsNotNull(p_parser) && IsNotNull(p_parser->spirv_code) && IsNotNull(p_parser->nodes)) {
        for (size_t i = 0; i < p_parser->node_count; ++i) {
            SpvReflectPrvNode *p_node = &(p_parser->nodes[i]);
            if ((p_node->op != SpvOpMemberName) && (p_node->op != SpvOpMemberDecorate)) {
                continue;
            }

            uint32_t target_id = 0;
            uint32_t member_index = (uint32_t)INVALID_VALUE;
            CHECKED_READU32(p_parser, p_node->word_offset + 1, target_id);
            CHECKED_READU32(p_parser, p_node->word_offset + 2, member_index);
            SpvReflectPrvNode *p_target_node = FindNode(p_parser, target_id);
            // Not all nodes get parsed, so FindNode returning NULL is expected.
            if (IsNull(p_target_node)) {
                continue;
            }

            if (member_index == INVALID_VALUE) {
                return SPV_REFLECT_RESULT_ERROR_RANGE_EXCEEDED;
            }

            p_target_node->member_count = Max(p_target_node->member_count, member_index + 1);
        }

        for (uint32_t i = 0; i < p_parser->node_count; ++i) {
            SpvReflectPrvNode *p_node = &(p_parser->nodes[i]);
            if (p_node->member_count == 0) {
                continue;
            }

            p_node->member_names = (const char **)calloc(p_node->member_count, sizeof(*(p_node->member_names)));
            if (IsNull(p_node->member_names)) {
                return SPV_REFLECT_RESULT_ERROR_ALLOC_FAILED;
            }

            p_node->member_decorations = (SpvReflectPrvDecorations *)calloc(p_node->member_count, sizeof(*(p_node->member_decorations)));
            if (IsNull(p_node->member_decorations)) {
                return SPV_REFLECT_RESULT_ERROR_ALLOC_FAILED;
            }
        }
    }
    return SPV_REFLECT_RESULT_SUCCESS;
}

static SpvReflectResult ParseNames(SpvReflectPrvParser *p_parser) {
    assert(IsNotNull(p_parser));
    assert(IsNotNull(p_parser->spirv_code));
    assert(IsNotNull(p_parser->nodes));

    if (IsNotNull(p_parser) && IsNotNull(p_parser->spirv_code) && IsNotNull(p_parser->nodes)) {
        for (size_t i = 0; i < p_parser->node_count; ++i) {
            SpvReflectPrvNode *p_node = &(p_parser->nodes[i]);
            if ((p_node->op != SpvOpName) && (p_node->op != SpvOpMemberName)) {
                continue;
            }

            uint32_t target_id = 0;
            CHECKED_READU32(p_parser, p_node->word_offset + 1, target_id);
            SpvReflectPrvNode *p_target_node = FindNode(p_parser, target_id);
            // Not all nodes get parsed, so FindNode returning NULL is expected.
            if (IsNull(p_target_node)) {
                continue;
            }

            const char **pp_target_name = &(p_target_node->name);
            if (p_node->op == SpvOpMemberName) {
                uint32_t member_index = UINT32_MAX;
                CHECKED_READU32(p_parser, p_node->word_offset + 2, member_index);
                pp_target_name = &(p_target_node->member_names[member_index]);
            }

            *pp_target_name = p_node->name;
        }
    }
    return SPV_REFLECT_RESULT_SUCCESS;
}

// Returns true if user_type matches pattern or if user_type begins with pattern and the next character is ':'
// For example, UserTypeMatches("rwbuffer", "rwbuffer") will be true, UserTypeMatches("rwbuffer", "rwbuffer:<S>") will be true, and
// UserTypeMatches("rwbuffer", "rwbufferfoo") will be false.
static bool UserTypeMatches(const char *user_type, const char *pattern) {
    const size_t pattern_length = strlen(pattern);
    if (strncmp(user_type, pattern, pattern_length) == 0) {
        if (user_type[pattern_length] == ':' || user_type[pattern_length] == '\0') {
            return true;
        }
    }
    return false;
}

static SpvReflectResult ParseDecorations(SpvReflectPrvParser *p_parser, SpvReflectShaderModule *p_module) {
    uint32_t spec_constant_count = 0;
    for (uint32_t i = 0; i < p_parser->node_count; ++i) {
        SpvReflectPrvNode *p_node = &(p_parser->nodes[i]);

        if ((p_node->op != SpvOpDecorate) && (p_node->op != SpvOpMemberDecorate) && (p_node->op != SpvOpDecorateId) &&
            (p_node->op != SpvOpDecorateString) && (p_node->op != SpvOpMemberDecorateString)) {
            continue;
        }

        // Need to adjust the read offset if this is a member decoration
        uint32_t member_offset = 0;
        if (p_node->op == SpvOpMemberDecorate) {
            member_offset = 1;
        }

        // Get decoration
        uint32_t decoration = (uint32_t)INVALID_VALUE;
        CHECKED_READU32(p_parser, p_node->word_offset + member_offset + 2, decoration);

        // Filter out the decoration that do not affect reflection, otherwise
        // there will be random crashes because the nodes aren't found.
        bool skip = false;
        switch (decoration) {
        default: {
            skip = true;
        } break;
        case SpvDecorationRelaxedPrecision:
        case SpvDecorationBlock:
        case SpvDecorationBufferBlock:
        case SpvDecorationColMajor:
        case SpvDecorationRowMajor:
        case SpvDecorationArrayStride:
        case SpvDecorationMatrixStride:
        case SpvDecorationBuiltIn:
        case SpvDecorationNoPerspective:
        case SpvDecorationFlat:
        case SpvDecorationNonWritable:
        case SpvDecorationNonReadable:
        case SpvDecorationPatch:
        case SpvDecorationPerVertexKHR:
        case SpvDecorationPerTaskNV:
        case SpvDecorationLocation:
        case SpvDecorationComponent:
        case SpvDecorationBinding:
        case SpvDecorationDescriptorSet:
        case SpvDecorationOffset:
        case SpvDecorationInputAttachmentIndex:
        case SpvDecorationSpecId:
        case SpvDecorationWeightTextureQCOM:
        case SpvDecorationBlockMatchTextureQCOM:
        case SpvDecorationUserTypeGOOGLE:
        case SpvDecorationHlslCounterBufferGOOGLE:
        case SpvDecorationHlslSemanticGOOGLE: {
            skip = false;
        } break;
        }
        if (skip) {
            continue;
        }

        // Find target node
        uint32_t target_id = 0;
        CHECKED_READU32(p_parser, p_node->word_offset + 1, target_id);
        SpvReflectPrvNode *p_target_node = FindNode(p_parser, target_id);
        if (IsNull(p_target_node)) {
            if ((p_node->op == (uint32_t)SpvOpDecorate) && (decoration == SpvDecorationRelaxedPrecision)) {
                // Many OPs can be decorated that we don't care about. Ignore those.
                // See https://github.com/KhronosGroup/SPIRV-Reflect/issues/134
                continue;
            }
            return SPV_REFLECT_RESULT_ERROR_SPIRV_INVALID_ID_REFERENCE;
        }
        // Get decorations
        SpvReflectPrvDecorations *p_target_decorations = &(p_target_node->decorations);
        // Update pointer if this is a member decoration
        if (p_node->op == SpvOpMemberDecorate) {
            uint32_t member_index = (uint32_t)INVALID_VALUE;
            CHECKED_READU32(p_parser, p_node->word_offset + 2, member_index);
            p_target_decorations = &(p_target_node->member_decorations[member_index]);
        }

        switch (decoration) {
        default:
            break;

        case SpvDecorationRelaxedPrecision: {
            p_target_decorations->is_relaxed_precision = true;
        } break;

        case SpvDecorationBlock: {
            p_target_decorations->is_block = true;
        } break;

        case SpvDecorationBufferBlock: {
            p_target_decorations->is_buffer_block = true;
        } break;

        case SpvDecorationColMajor: {
            p_target_decorations->is_column_major = true;
        } break;

        case SpvDecorationRowMajor: {
            p_target_decorations->is_row_major = true;
        } break;

        case SpvDecorationArrayStride: {
            uint32_t word_offset = p_node->word_offset + member_offset + 3;
            CHECKED_READU32(p_parser, word_offset, p_target_decorations->array_stride);
        } break;

        case SpvDecorationMatrixStride: {
            uint32_t word_offset = p_node->word_offset + member_offset + 3;
            CHECKED_READU32(p_parser, word_offset, p_target_decorations->matrix_stride);
        } break;

        case SpvDecorationBuiltIn: {
            p_target_decorations->is_built_in = true;
            uint32_t word_offset = p_node->word_offset + member_offset + 3;
            CHECKED_READU32_CAST(p_parser, word_offset, SpvBuiltIn, p_target_decorations->built_in);
        } break;

        case SpvDecorationNoPerspective: {
            p_target_decorations->is_noperspective = true;
        } break;

        case SpvDecorationFlat: {
            p_target_decorations->is_flat = true;
        } break;

        case SpvDecorationNonWritable: {
            p_target_decorations->is_non_writable = true;
        } break;

        case SpvDecorationNonReadable: {
            p_target_decorations->is_non_readable = true;
        } break;

        case SpvDecorationPatch: {
            p_target_decorations->is_patch = true;
        } break;

        case SpvDecorationPerVertexKHR: {
            p_target_decorations->is_per_vertex = true;
        } break;

        case SpvDecorationPerTaskNV: {
            p_target_decorations->is_per_task = true;
        } break;

        case SpvDecorationLocation: {
            uint32_t word_offset = p_node->word_offset + member_offset + 3;
            CHECKED_READU32(p_parser, word_offset, p_target_decorations->location.value);
            p_target_decorations->location.word_offset = word_offset;
        } break;

        case SpvDecorationComponent: {
            uint32_t word_offset = p_node->word_offset + member_offset + 3;
            CHECKED_READU32(p_parser, word_offset, p_target_decorations->component.value);
            p_target_decorations->component.word_offset = word_offset;
        } break;

        case SpvDecorationBinding: {
            uint32_t word_offset = p_node->word_offset + member_offset + 3;
            CHECKED_READU32(p_parser, word_offset, p_target_decorations->binding.value);
            p_target_decorations->binding.word_offset = word_offset;
        } break;

        case SpvDecorationDescriptorSet: {
            uint32_t word_offset = p_node->word_offset + member_offset + 3;
            CHECKED_READU32(p_parser, word_offset, p_target_decorations->set.value);
            p_target_decorations->set.word_offset = word_offset;
        } break;

        case SpvDecorationOffset: {
            uint32_t word_offset = p_node->word_offset + member_offset + 3;
            CHECKED_READU32(p_parser, word_offset, p_target_decorations->offset.value);
            p_target_decorations->offset.word_offset = word_offset;
        } break;

        case SpvDecorationInputAttachmentIndex: {
            uint32_t word_offset = p_node->word_offset + member_offset + 3;
            CHECKED_READU32(p_parser, word_offset, p_target_decorations->input_attachment_index.value);
            p_target_decorations->input_attachment_index.word_offset = word_offset;
        } break;

        case SpvDecorationSpecId: {
            spec_constant_count++;
        } break;

        case SpvDecorationHlslCounterBufferGOOGLE: {
            uint32_t word_offset = p_node->word_offset + member_offset + 3;
            CHECKED_READU32(p_parser, word_offset, p_target_decorations->uav_counter_buffer.value);
            p_target_decorations->uav_counter_buffer.word_offset = word_offset;
        } break;

        case SpvDecorationHlslSemanticGOOGLE: {
            uint32_t word_offset = p_node->word_offset + member_offset + 3;
            p_target_decorations->semantic.value = (const char *)(p_parser->spirv_code + word_offset);
            p_target_decorations->semantic.word_offset = word_offset;
        } break;

        case SpvDecorationWeightTextureQCOM: {
            p_target_decorations->is_weight_texture = true;
        } break;

        case SpvDecorationBlockMatchTextureQCOM: {
            p_target_decorations->is_block_match_texture = true;
        } break;
        }

        if (p_node->op == SpvOpDecorateString && decoration == SpvDecorationUserTypeGOOGLE) {
            uint32_t terminator = 0;
            SpvReflectResult result = ReadStr(p_parser, p_node->word_offset + 3, 0, p_node->word_count, &terminator, NULL);
            if (result != SPV_REFLECT_RESULT_SUCCESS) {
                return result;
            }
            const char *name = (const char *)(p_parser->spirv_code + p_node->word_offset + 3);
            if (UserTypeMatches(name, "cbuffer")) {
                p_target_decorations->user_type = SPV_REFLECT_USER_TYPE_CBUFFER;
            } else if (UserTypeMatches(name, "tbuffer")) {
                p_target_decorations->user_type = SPV_REFLECT_USER_TYPE_TBUFFER;
            } else if (UserTypeMatches(name, "appendstructuredbuffer")) {
                p_target_decorations->user_type = SPV_REFLECT_USER_TYPE_APPEND_STRUCTURED_BUFFER;
            } else if (UserTypeMatches(name, "buffer")) {
                p_target_decorations->user_type = SPV_REFLECT_USER_TYPE_BUFFER;
            } else if (UserTypeMatches(name, "byteaddressbuffer")) {
                p_target_decorations->user_type = SPV_REFLECT_USER_TYPE_BYTE_ADDRESS_BUFFER;
            } else if (UserTypeMatches(name, "constantbuffer")) {
                p_target_decorations->user_type = SPV_REFLECT_USER_TYPE_CONSTANT_BUFFER;
            } else if (UserTypeMatches(name, "consumestructuredbuffer")) {
                p_target_decorations->user_type = SPV_REFLECT_USER_TYPE_CONSUME_STRUCTURED_BUFFER;
            } else if (UserTypeMatches(name, "inputpatch")) {
                p_target_decorations->user_type = SPV_REFLECT_USER_TYPE_INPUT_PATCH;
            } else if (UserTypeMatches(name, "outputpatch")) {
                p_target_decorations->user_type = SPV_REFLECT_USER_TYPE_OUTPUT_PATCH;
            } else if (UserTypeMatches(name, "rasterizerorderedbuffer")) {
                p_target_decorations->user_type = SPV_REFLECT_USER_TYPE_RASTERIZER_ORDERED_BUFFER;
            } else if (UserTypeMatches(name, "rasterizerorderedbyteaddressbuffer")) {
                p_target_decorations->user_type = SPV_REFLECT_USER_TYPE_RASTERIZER_ORDERED_BYTE_ADDRESS_BUFFER;
            } else if (UserTypeMatches(name, "rasterizerorderedstructuredbuffer")) {
                p_target_decorations->user_type = SPV_REFLECT_USER_TYPE_RASTERIZER_ORDERED_STRUCTURED_BUFFER;
            } else if (UserTypeMatches(name, "rasterizerorderedtexture1d")) {
                p_target_decorations->user_type = SPV_REFLECT_USER_TYPE_RASTERIZER_ORDERED_TEXTURE_1D;
            } else if (UserTypeMatches(name, "rasterizerorderedtexture1darray")) {
                p_target_decorations->user_type = SPV_REFLECT_USER_TYPE_RASTERIZER_ORDERED_TEXTURE_1D_ARRAY;
            } else if (UserTypeMatches(name, "rasterizerorderedtexture2d")) {
                p_target_decorations->user_type = SPV_REFLECT_USER_TYPE_RASTERIZER_ORDERED_TEXTURE_2D;
            } else if (UserTypeMatches(name, "rasterizerorderedtexture2darray")) {
                p_target_decorations->user_type = SPV_REFLECT_USER_TYPE_RASTERIZER_ORDERED_TEXTURE_2D_ARRAY;
            } else if (UserTypeMatches(name, "rasterizerorderedtexture3d")) {
                p_target_decorations->user_type = SPV_REFLECT_USER_TYPE_RASTERIZER_ORDERED_TEXTURE_3D;
            } else if (UserTypeMatches(name, "raytracingaccelerationstructure")) {
                p_target_decorations->user_type = SPV_REFLECT_USER_TYPE_RAYTRACING_ACCELERATION_STRUCTURE;
            } else if (UserTypeMatches(name, "rwbuffer")) {
                p_target_decorations->user_type = SPV_REFLECT_USER_TYPE_RW_BUFFER;
            } else if (UserTypeMatches(name, "rwbyteaddressbuffer")) {
                p_target_decorations->user_type = SPV_REFLECT_USER_TYPE_RW_BYTE_ADDRESS_BUFFER;
            } else if (UserTypeMatches(name, "rwstructuredbuffer")) {
                p_target_decorations->user_type = SPV_REFLECT_USER_TYPE_RW_STRUCTURED_BUFFER;
            } else if (UserTypeMatches(name, "rwtexture1d")) {
                p_target_decorations->user_type = SPV_REFLECT_USER_TYPE_RW_TEXTURE_1D;
            } else if (UserTypeMatches(name, "rwtexture1darray")) {
                p_target_decorations->user_type = SPV_REFLECT_USER_TYPE_RW_TEXTURE_1D_ARRAY;
            } else if (UserTypeMatches(name, "rwtexture2d")) {
                p_target_decorations->user_type = SPV_REFLECT_USER_TYPE_RW_TEXTURE_2D;
            } else if (UserTypeMatches(name, "rwtexture2darray")) {
                p_target_decorations->user_type = SPV_REFLECT_USER_TYPE_RW_TEXTURE_2D_ARRAY;
            } else if (UserTypeMatches(name, "rwtexture3d")) {
                p_target_decorations->user_type = SPV_REFLECT_USER_TYPE_RW_TEXTURE_3D;
            } else if (UserTypeMatches(name, "structuredbuffer")) {
                p_target_decorations->user_type = SPV_REFLECT_USER_TYPE_STRUCTURED_BUFFER;
            } else if (UserTypeMatches(name, "subpassinput")) {
                p_target_decorations->user_type = SPV_REFLECT_USER_TYPE_SUBPASS_INPUT;
            } else if (UserTypeMatches(name, "subpassinputms")) {
                p_target_decorations->user_type = SPV_REFLECT_USER_TYPE_SUBPASS_INPUT_MS;
            } else if (UserTypeMatches(name, "texture1d")) {
                p_target_decorations->user_type = SPV_REFLECT_USER_TYPE_TEXTURE_1D;
            } else if (UserTypeMatches(name, "texture1darray")) {
                p_target_decorations->user_type = SPV_REFLECT_USER_TYPE_TEXTURE_1D_ARRAY;
            } else if (UserTypeMatches(name, "texture2d")) {
                p_target_decorations->user_type = SPV_REFLECT_USER_TYPE_TEXTURE_2D;
            } else if (UserTypeMatches(name, "texture2darray")) {
                p_target_decorations->user_type = SPV_REFLECT_USER_TYPE_TEXTURE_2D_ARRAY;
            } else if (UserTypeMatches(name, "texture2dms")) {
                p_target_decorations->user_type = SPV_REFLECT_USER_TYPE_TEXTURE_2DMS;
            } else if (UserTypeMatches(name, "texture2dmsarray")) {
                p_target_decorations->user_type = SPV_REFLECT_USER_TYPE_TEXTURE_2DMS_ARRAY;
            } else if (UserTypeMatches(name, "texture3d")) {
                p_target_decorations->user_type = SPV_REFLECT_USER_TYPE_TEXTURE_3D;
            } else if (UserTypeMatches(name, "texturebuffer")) {
                p_target_decorations->user_type = SPV_REFLECT_USER_TYPE_TEXTURE_BUFFER;
            } else if (UserTypeMatches(name, "texturecube")) {
                p_target_decorations->user_type = SPV_REFLECT_USER_TYPE_TEXTURE_CUBE;
            } else if (UserTypeMatches(name, "texturecubearray")) {
                p_target_decorations->user_type = SPV_REFLECT_USER_TYPE_TEXTURE_CUBE_ARRAY;
            }
        }
    }

    if (spec_constant_count > 0) {
        p_module->spec_constants = (SpvReflectSpecializationConstant *)calloc(spec_constant_count, sizeof(*p_module->spec_constants));
        if (IsNull(p_module->spec_constants)) {
            return SPV_REFLECT_RESULT_ERROR_ALLOC_FAILED;
        }
    }
    for (uint32_t i = 0; i < p_parser->node_count; ++i) {
        SpvReflectPrvNode *p_node = &(p_parser->nodes[i]);
        if (p_node->op == SpvOpDecorate) {
            uint32_t decoration = (uint32_t)INVALID_VALUE;
            CHECKED_READU32(p_parser, p_node->word_offset + 2, decoration);
            if (decoration == SpvDecorationSpecId) {
                const uint32_t count = p_module->spec_constant_count;
                CHECKED_READU32(p_parser, p_node->word_offset + 1, p_module->spec_constants[count].spirv_id);
                CHECKED_READU32(p_parser, p_node->word_offset + 3, p_module->spec_constants[count].constant_id);
                // If being used for a OpSpecConstantComposite (ex. LocalSizeId), there won't be a name
                SpvReflectPrvNode *target_node = FindNode(p_parser, p_module->spec_constants[count].spirv_id);
                if (IsNotNull(target_node)) {
                    p_module->spec_constants[count].name = target_node->name;
                }
                p_module->spec_constant_count++;
            }
        }
    }

    return SPV_REFLECT_RESULT_SUCCESS;
}

static SpvReflectResult EnumerateAllUniforms(SpvReflectShaderModule *p_module, size_t *p_uniform_count, uint32_t **pp_uniforms) {
    *p_uniform_count = p_module->descriptor_binding_count;
    if (*p_uniform_count == 0) {
        return SPV_REFLECT_RESULT_SUCCESS;
    }
    *pp_uniforms = (uint32_t *)calloc(*p_uniform_count, sizeof(**pp_uniforms));

    if (IsNull(*pp_uniforms)) {
        return SPV_REFLECT_RESULT_ERROR_ALLOC_FAILED;
    }

    for (size_t i = 0; i < *p_uniform_count; ++i) {
        (*pp_uniforms)[i] = p_module->descriptor_bindings[i].spirv_id;
    }
    qsort(*pp_uniforms, *p_uniform_count, sizeof(**pp_uniforms), SortCompareUint32);
    return SPV_REFLECT_RESULT_SUCCESS;
}

static SpvReflectResult ParseType(SpvReflectPrvParser *p_parser, SpvReflectPrvNode *p_node,
    SpvReflectPrvDecorations *p_struct_member_decorations, SpvReflectShaderModule *p_module,
    SpvReflectTypeDescription *p_type) {
    SpvReflectResult result = SPV_REFLECT_RESULT_SUCCESS;

    if (p_node->member_count > 0) {
        p_type->struct_type_description = FindType(p_module, p_node->result_id);
        p_type->member_count = p_node->member_count;
        p_type->members = (SpvReflectTypeDescription *)calloc(p_type->member_count, sizeof(*(p_type->members)));
        if (IsNotNull(p_type->members)) {
            // Mark all members types with an invalid state
            for (size_t i = 0; i < p_type->members->member_count; ++i) {
                SpvReflectTypeDescription *p_member_type = &(p_type->members[i]);
                p_member_type->id = (uint32_t)INVALID_VALUE;
                p_member_type->op = (SpvOp)INVALID_VALUE;
                p_member_type->storage_class = (SpvStorageClass)INVALID_VALUE;
            }
        } else {
            result = SPV_REFLECT_RESULT_ERROR_ALLOC_FAILED;
        }
    }

    if (result == SPV_REFLECT_RESULT_SUCCESS) {
        // Since the parse descends on type information, these will get overwritten
        // if not guarded against assignment. Only assign if the id is invalid.
        if (p_type->id == INVALID_VALUE) {
            p_type->id = p_node->result_id;
            p_type->op = p_node->op;
            p_type->decoration_flags = 0;
        }
        // Top level types need to pick up decorations from all types below it.
        // Issue and fix here: https://github.com/chaoticbob/SPIRV-Reflect/issues/64
        p_type->decoration_flags = ApplyDecorations(&p_node->decorations);

        switch (p_node->op) {
        default:
            break;
        case SpvOpTypeVoid:
            p_type->type_flags |= SPV_REFLECT_TYPE_FLAG_VOID;
            break;

        case SpvOpTypeBool:
            p_type->type_flags |= SPV_REFLECT_TYPE_FLAG_BOOL;
            break;

        case SpvOpTypeInt: {
            p_type->type_flags |= SPV_REFLECT_TYPE_FLAG_INT;
            IF_READU32(result, p_parser, p_node->word_offset + 2, p_type->traits.numeric.scalar.width);
            IF_READU32(result, p_parser, p_node->word_offset + 3, p_type->traits.numeric.scalar.signedness);
        } break;

        case SpvOpTypeFloat: {
            p_type->type_flags |= SPV_REFLECT_TYPE_FLAG_FLOAT;
            IF_READU32(result, p_parser, p_node->word_offset + 2, p_type->traits.numeric.scalar.width);
        } break;

        case SpvOpTypeVector: {
            p_type->type_flags |= SPV_REFLECT_TYPE_FLAG_VECTOR;
            uint32_t component_type_id = (uint32_t)INVALID_VALUE;
            IF_READU32(result, p_parser, p_node->word_offset + 2, component_type_id);
            IF_READU32(result, p_parser, p_node->word_offset + 3, p_type->traits.numeric.vector.component_count);
            // Parse component type
            SpvReflectPrvNode *p_next_node = FindNode(p_parser, component_type_id);
            if (IsNotNull(p_next_node)) {
                result = ParseType(p_parser, p_next_node, NULL, p_module, p_type);
            } else {
                result = SPV_REFLECT_RESULT_ERROR_SPIRV_INVALID_ID_REFERENCE;
                SPV_REFLECT_ASSERT(false);
            }
        } break;

        case SpvOpTypeMatrix: {
            p_type->type_flags |= SPV_REFLECT_TYPE_FLAG_MATRIX;
            uint32_t column_type_id = (uint32_t)INVALID_VALUE;
            IF_READU32(result, p_parser, p_node->word_offset + 2, column_type_id);
            IF_READU32(result, p_parser, p_node->word_offset + 3, p_type->traits.numeric.matrix.column_count);
            SpvReflectPrvNode *p_next_node = FindNode(p_parser, column_type_id);
            if (IsNotNull(p_next_node)) {
                result = ParseType(p_parser, p_next_node, NULL, p_module, p_type);
            } else {
                result = SPV_REFLECT_RESULT_ERROR_SPIRV_INVALID_ID_REFERENCE;
                SPV_REFLECT_ASSERT(false);
            }
            p_type->traits.numeric.matrix.row_count = p_type->traits.numeric.vector.component_count;
            p_type->traits.numeric.matrix.stride = p_node->decorations.matrix_stride;
            // NOTE: Matrix stride is decorated using OpMemberDecoreate - not OpDecoreate.
            if (IsNotNull(p_struct_member_decorations)) {
                p_type->traits.numeric.matrix.stride = p_struct_member_decorations->matrix_stride;
            }
        } break;

        case SpvOpTypeImage: {
            p_type->type_flags |= SPV_REFLECT_TYPE_FLAG_EXTERNAL_IMAGE;
            uint32_t sampled_type_id = (uint32_t)INVALID_VALUE;
            IF_READU32(result, p_parser, p_node->word_offset + 2, sampled_type_id);
            SpvReflectPrvNode *p_next_node = FindNode(p_parser, sampled_type_id);
            if (IsNotNull(p_next_node)) {
                result = ParseType(p_parser, p_next_node, NULL, p_module, p_type);
            } else {
                result = SPV_REFLECT_RESULT_ERROR_SPIRV_INVALID_ID_REFERENCE;
            }
            IF_READU32_CAST(result, p_parser, p_node->word_offset + 3, SpvDim, p_type->traits.image.dim);
            IF_READU32(result, p_parser, p_node->word_offset + 4, p_type->traits.image.depth);
            IF_READU32(result, p_parser, p_node->word_offset + 5, p_type->traits.image.arrayed);
            IF_READU32(result, p_parser, p_node->word_offset + 6, p_type->traits.image.ms);
            IF_READU32(result, p_parser, p_node->word_offset + 7, p_type->traits.image.sampled);
            IF_READU32_CAST(result, p_parser, p_node->word_offset + 8, SpvImageFormat, p_type->traits.image.image_format);
        } break;

        case SpvOpTypeSampler: {
            p_type->type_flags |= SPV_REFLECT_TYPE_FLAG_EXTERNAL_SAMPLER;
        } break;

        case SpvOpTypeSampledImage: {
            p_type->type_flags |= SPV_REFLECT_TYPE_FLAG_EXTERNAL_SAMPLED_IMAGE;
            uint32_t image_type_id = (uint32_t)INVALID_VALUE;
            IF_READU32(result, p_parser, p_node->word_offset + 2, image_type_id);
            SpvReflectPrvNode *p_next_node = FindNode(p_parser, image_type_id);
            if (IsNotNull(p_next_node)) {
                result = ParseType(p_parser, p_next_node, NULL, p_module, p_type);
            } else {
                result = SPV_REFLECT_RESULT_ERROR_SPIRV_INVALID_ID_REFERENCE;
                SPV_REFLECT_ASSERT(false);
            }
        } break;

        case SpvOpTypeArray: {
            p_type->type_flags |= SPV_REFLECT_TYPE_FLAG_ARRAY;
            if (result == SPV_REFLECT_RESULT_SUCCESS) {
                uint32_t element_type_id = (uint32_t)INVALID_VALUE;
                uint32_t length_id = (uint32_t)INVALID_VALUE;
                IF_READU32(result, p_parser, p_node->word_offset + 2, element_type_id);
                IF_READU32(result, p_parser, p_node->word_offset + 3, length_id);
                // NOTE: Array stride is decorated using OpDecorate instead of
                //       OpMemberDecorate, even if the array is apart of a struct.
                p_type->traits.array.stride = p_node->decorations.array_stride;
                // Get length for current dimension
                SpvReflectPrvNode *p_length_node = FindNode(p_parser, length_id);
                if (IsNotNull(p_length_node)) {
                    uint32_t dim_index = p_type->traits.array.dims_count;
                    uint32_t length = 0;
                    IF_READU32(result, p_parser, p_length_node->word_offset + 3, length);
                    if (result == SPV_REFLECT_RESULT_SUCCESS) {
                        p_type->traits.array.dims[dim_index] = length;
                        p_type->traits.array.dims_count += 1;
                        p_type->traits.array.spec_constant_op_ids[dim_index] =
                            IsSpecConstant(p_length_node) ? p_length_node->decorations.spec_id : (uint32_t)INVALID_VALUE;
                    } else {
                        result = SPV_REFLECT_RESULT_ERROR_SPIRV_INVALID_ID_REFERENCE;
                        SPV_REFLECT_ASSERT(false);
                    }
                    // Parse next dimension or element type
                    SpvReflectPrvNode *p_next_node = FindNode(p_parser, element_type_id);
                    if (IsNotNull(p_next_node)) {
                        result = ParseType(p_parser, p_next_node, NULL, p_module, p_type);
                    }
                } else {
                    result = SPV_REFLECT_RESULT_ERROR_SPIRV_INVALID_ID_REFERENCE;
                    SPV_REFLECT_ASSERT(false);
                }
            }
        } break;

        case SpvOpTypeRuntimeArray: {
            p_type->type_flags |= SPV_REFLECT_TYPE_FLAG_ARRAY;
            uint32_t element_type_id = (uint32_t)INVALID_VALUE;
            IF_READU32(result, p_parser, p_node->word_offset + 2, element_type_id);
            p_type->traits.array.stride = p_node->decorations.array_stride;
            uint32_t dim_index = p_type->traits.array.dims_count;
            p_type->traits.array.dims[dim_index] = (uint32_t)SPV_REFLECT_ARRAY_DIM_RUNTIME;
            p_type->traits.array.spec_constant_op_ids[dim_index] = (uint32_t)INVALID_VALUE;
            p_type->traits.array.dims_count += 1;
            // Parse next dimension or element type
            SpvReflectPrvNode *p_next_node = FindNode(p_parser, element_type_id);
            if (IsNotNull(p_next_node)) {
                result = ParseType(p_parser, p_next_node, NULL, p_module, p_type);
            } else {
                result = SPV_REFLECT_RESULT_ERROR_SPIRV_INVALID_ID_REFERENCE;
                SPV_REFLECT_ASSERT(false);
            }
        } break;

        case SpvOpTypeStruct: {
            p_type->type_flags |= SPV_REFLECT_TYPE_FLAG_STRUCT;
            p_type->type_flags |= SPV_REFLECT_TYPE_FLAG_EXTERNAL_BLOCK;
            uint32_t word_index = 2;
            uint32_t member_index = 0;
            for (; word_index < p_node->word_count; ++word_index, ++member_index) {
                uint32_t member_id = (uint32_t)INVALID_VALUE;
                IF_READU32(result, p_parser, p_node->word_offset + word_index, member_id);
                // Find member node
                SpvReflectPrvNode *p_member_node = FindNode(p_parser, member_id);
                if (IsNull(p_member_node)) {
                    result = SPV_REFLECT_RESULT_ERROR_SPIRV_INVALID_ID_REFERENCE;
                    SPV_REFLECT_ASSERT(false);
                    break;
                }

                // Member decorations
                SpvReflectPrvDecorations *p_member_decorations = &p_node->member_decorations[member_index];

                assert(member_index < p_type->member_count);
                // Parse member type
                SpvReflectTypeDescription *p_member_type = &(p_type->members[member_index]);
                p_member_type->id = member_id;
                p_member_type->op = p_member_node->op;
                result = ParseType(p_parser, p_member_node, p_member_decorations, p_module, p_member_type);
                if (result != SPV_REFLECT_RESULT_SUCCESS) {
                    break;
                }
                // This looks wrong
                // p_member_type->type_name = p_member_node->name;
                p_member_type->struct_member_name = p_node->member_names[member_index];
            }
        } break;

        case SpvOpTypeOpaque:
            break;

        case SpvOpTypePointer: {
            p_type->type_flags |= SPV_REFLECT_TYPE_FLAG_REF;
            IF_READU32_CAST(result, p_parser, p_node->word_offset + 2, SpvStorageClass, p_type->storage_class);

            SpvReflectPrvNode *p_next_node = FindNode(p_parser, p_node->type_id);
            if (IsNull(p_next_node)) {
                result = SPV_REFLECT_RESULT_ERROR_SPIRV_INVALID_ID_REFERENCE;
                SPV_REFLECT_ASSERT(false);
                break;
            }

            bool found_recursion = false;
            if (p_type->storage_class == SpvStorageClassPhysicalStorageBuffer) {
                // Need to make sure we haven't started an infinite recursive loop
                for (uint32_t i = 0; i < p_parser->physical_pointer_count; i++) {
                    if (p_type->id == p_parser->physical_pointer_check[i]->id) {
                        found_recursion = true;
                        memcpy(p_type, p_parser->physical_pointer_check[i], sizeof(SpvReflectTypeDescription));
                        p_type->copied = 1;
                        return SPV_REFLECT_RESULT_SUCCESS;
                    }
                }
                if (!found_recursion && p_next_node->op == SpvOpTypeStruct) {
                    p_parser->physical_pointer_struct_count++;
                    p_parser->physical_pointer_check[p_parser->physical_pointer_count] = p_type;
                    p_parser->physical_pointer_count++;
                    if (p_parser->physical_pointer_count >= MAX_RECURSIVE_PHYSICAL_POINTER_CHECK) {
                        return SPV_REFLECT_RESULT_ERROR_SPIRV_MAX_RECURSIVE_EXCEEDED;
                    }
                }
            }

            if (!found_recursion) {
                if (p_next_node->op == SpvOpTypeStruct) {
                    p_type->struct_type_description = FindType(p_module, p_next_node->result_id);
                }

                result = ParseType(p_parser, p_next_node, NULL, p_module, p_type);
            }
        } break;

        case SpvOpTypeAccelerationStructureKHR: {
            p_type->type_flags |= SPV_REFLECT_TYPE_FLAG_EXTERNAL_ACCELERATION_STRUCTURE;
        } break;
        }

        if (result == SPV_REFLECT_RESULT_SUCCESS) {
            // Names get assigned on the way down. Guard against names
            // get overwritten on the way up.
            if (IsNull(p_type->type_name)) {
                p_type->type_name = p_node->name;
            }
        }
    }

    return result;
}

static SpvReflectResult ParseTypes(SpvReflectPrvParser *p_parser, SpvReflectShaderModule *p_module) {
    if (p_parser->type_count == 0) {
        return SPV_REFLECT_RESULT_SUCCESS;
    }

    p_module->_internal->type_description_count = p_parser->type_count;
    p_module->_internal->type_descriptions = (SpvReflectTypeDescription *)calloc(p_module->_internal->type_description_count,
        sizeof(*(p_module->_internal->type_descriptions)));
    if (IsNull(p_module->_internal->type_descriptions)) {
        return SPV_REFLECT_RESULT_ERROR_ALLOC_FAILED;
    }

    // Mark all types with an invalid state
    for (size_t i = 0; i < p_module->_internal->type_description_count; ++i) {
        SpvReflectTypeDescription *p_type = &(p_module->_internal->type_descriptions[i]);
        p_type->id = (uint32_t)INVALID_VALUE;
        p_type->op = (SpvOp)INVALID_VALUE;
        p_type->storage_class = (SpvStorageClass)INVALID_VALUE;
    }

    size_t type_index = 0;
    for (size_t i = 0; i < p_parser->node_count; ++i) {
        SpvReflectPrvNode *p_node = &(p_parser->nodes[i]);
        if (!p_node->is_type) {
            continue;
        }

        SpvReflectTypeDescription *p_type = &(p_module->_internal->type_descriptions[type_index]);
        p_parser->physical_pointer_count = 0;
        SpvReflectResult result = ParseType(p_parser, p_node, NULL, p_module, p_type);
        if (result != SPV_REFLECT_RESULT_SUCCESS) {
            return result;
        }
        ++type_index;
    }

    // allocate now and fill in when parsing struct variable later
    if (p_parser->physical_pointer_struct_count > 0) {
        p_parser->physical_pointer_structs = (SpvReflectPrvPhysicalPointerStruct *)calloc(p_parser->physical_pointer_struct_count,
            sizeof(*(p_parser->physical_pointer_structs)));
        if (IsNull(p_parser->physical_pointer_structs)) {
            return SPV_REFLECT_RESULT_ERROR_ALLOC_FAILED;
        }
    }
    return SPV_REFLECT_RESULT_SUCCESS;
}

static SpvReflectResult ParseCapabilities(SpvReflectPrvParser *p_parser, SpvReflectShaderModule *p_module) {
    if (p_parser->capability_count == 0) {
        return SPV_REFLECT_RESULT_SUCCESS;
    }

    p_module->capability_count = p_parser->capability_count;
    p_module->capabilities = (SpvReflectCapability *)calloc(p_module->capability_count, sizeof(*(p_module->capabilities)));
    if (IsNull(p_module->capabilities)) {
        return SPV_REFLECT_RESULT_ERROR_ALLOC_FAILED;
    }

    // Mark all types with an invalid state
    for (size_t i = 0; i < p_module->capability_count; ++i) {
        SpvReflectCapability *p_cap = &(p_module->capabilities[i]);
        p_cap->value = SpvCapabilityMax;
        p_cap->word_offset = (uint32_t)INVALID_VALUE;
    }

    size_t capability_index = 0;
    for (size_t i = 0; i < p_parser->node_count; ++i) {
        SpvReflectPrvNode *p_node = &(p_parser->nodes[i]);
        if (SpvOpCapability != p_node->op) {
            continue;
        }

        SpvReflectCapability *p_cap = &(p_module->capabilities[capability_index]);
        p_cap->value = p_node->capability;
        p_cap->word_offset = p_node->word_offset + 1;
        ++capability_index;
    }

    return SPV_REFLECT_RESULT_SUCCESS;
}

static int SortCompareDescriptorBinding(const void *a, const void *b) {
    const SpvReflectDescriptorBinding *p_elem_a = (const SpvReflectDescriptorBinding *)a;
    const SpvReflectDescriptorBinding *p_elem_b = (const SpvReflectDescriptorBinding *)b;
    int value = (int)(p_elem_a->binding) - (int)(p_elem_b->binding);
    if (value == 0) {
        // use spirv-id as a tiebreaker to ensure a stable ordering, as they're guaranteed
        // unique.
        assert(p_elem_a->spirv_id != p_elem_b->spirv_id);
        value = (int)(p_elem_a->spirv_id) - (int)(p_elem_b->spirv_id);
    }
    return value;
}

static SpvReflectResult ParseDescriptorBindings(SpvReflectPrvParser *p_parser, SpvReflectShaderModule *p_module) {
    p_module->descriptor_binding_count = 0;
    for (size_t i = 0; i < p_parser->node_count; ++i) {
        SpvReflectPrvNode *p_node = &(p_parser->nodes[i]);
        if ((p_node->op != SpvOpVariable) ||
            ((p_node->storage_class != SpvStorageClassUniform) && (p_node->storage_class != SpvStorageClassStorageBuffer) &&
                (p_node->storage_class != SpvStorageClassUniformConstant))) {
            continue;
        }
        if ((p_node->decorations.set.value == INVALID_VALUE) || (p_node->decorations.binding.value == INVALID_VALUE)) {
            continue;
        }

        p_module->descriptor_binding_count += 1;
    }

    if (p_module->descriptor_binding_count == 0) {
        return SPV_REFLECT_RESULT_SUCCESS;
    }

    p_module->descriptor_bindings =
        (SpvReflectDescriptorBinding *)calloc(p_module->descriptor_binding_count, sizeof(*(p_module->descriptor_bindings)));
    if (IsNull(p_module->descriptor_bindings)) {
        return SPV_REFLECT_RESULT_ERROR_ALLOC_FAILED;
    }

    // Mark all types with an invalid state
    for (uint32_t descriptor_index = 0; descriptor_index < p_module->descriptor_binding_count; ++descriptor_index) {
        SpvReflectDescriptorBinding *p_descriptor = &(p_module->descriptor_bindings[descriptor_index]);
        p_descriptor->binding = (uint32_t)INVALID_VALUE;
        p_descriptor->input_attachment_index = (uint32_t)INVALID_VALUE;
        p_descriptor->set = (uint32_t)INVALID_VALUE;
        p_descriptor->descriptor_type = (SpvReflectDescriptorType)INVALID_VALUE;
        p_descriptor->uav_counter_id = (uint32_t)INVALID_VALUE;
    }

    size_t descriptor_index = 0;
    for (size_t i = 0; i < p_parser->node_count; ++i) {
        SpvReflectPrvNode *p_node = &(p_parser->nodes[i]);
        if ((p_node->op != SpvOpVariable) ||
            ((p_node->storage_class != SpvStorageClassUniform) && (p_node->storage_class != SpvStorageClassStorageBuffer) &&
                (p_node->storage_class != SpvStorageClassUniformConstant))) {
            continue;
        }
        if ((p_node->decorations.set.value == INVALID_VALUE) || (p_node->decorations.binding.value == INVALID_VALUE)) {
            continue;
        }

        SpvReflectTypeDescription *p_type = FindType(p_module, p_node->type_id);
        if (IsNull(p_type)) {
            return SPV_REFLECT_RESULT_ERROR_SPIRV_INVALID_ID_REFERENCE;
        }
        // If the type is a pointer, resolve it. We need to retain the storage class
        // from the pointer so that we can use it to deduce deescriptor types.
        SpvStorageClass pointer_storage_class = SpvStorageClassMax;
        if (p_type->op == SpvOpTypePointer) {
            assert(p_type->storage_class != -1 && "Pointer types must have a valid storage class.");
            pointer_storage_class = (SpvStorageClass)p_type->storage_class;
            // Find the type's node
            SpvReflectPrvNode *p_type_node = FindNode(p_parser, p_type->id);
            if (IsNull(p_type_node)) {
                return SPV_REFLECT_RESULT_ERROR_SPIRV_INVALID_ID_REFERENCE;
            }
            // Should be the resolved type
            p_type = FindType(p_module, p_type_node->type_id);
            if (IsNull(p_type)) {
                return SPV_REFLECT_RESULT_ERROR_SPIRV_INVALID_ID_REFERENCE;
            }
        }

        SpvReflectDescriptorBinding *p_descriptor = &p_module->descriptor_bindings[descriptor_index];
        p_descriptor->spirv_id = p_node->result_id;
        p_descriptor->name = p_node->name;
        p_descriptor->binding = p_node->decorations.binding.value;
        p_descriptor->input_attachment_index = p_node->decorations.input_attachment_index.value;
        p_descriptor->set = p_node->decorations.set.value;
        p_descriptor->count = 1;
        p_descriptor->uav_counter_id = p_node->decorations.uav_counter_buffer.value;
        p_descriptor->type_description = p_type;
        p_descriptor->decoration_flags = ApplyDecorations(&p_node->decorations);
        p_descriptor->user_type = p_node->decorations.user_type;

        // Flags like non-writable and non-readable are found as member decorations only.
        // If all members have one of those decorations set, promote the decoration up
        // to the whole descriptor.
        const SpvReflectPrvNode *p_type_node = FindNode(p_parser, p_type->id);
        if (IsNotNull(p_type_node) && p_type_node->member_count) {
            SpvReflectPrvDecorations common_flags = p_type_node->member_decorations[0];

            for (uint32_t m = 1; m < p_type_node->member_count; ++m) {
                common_flags.is_relaxed_precision &= p_type_node->member_decorations[m].is_relaxed_precision;
                common_flags.is_block &= p_type_node->member_decorations[m].is_block;
                common_flags.is_buffer_block &= p_type_node->member_decorations[m].is_buffer_block;
                common_flags.is_row_major &= p_type_node->member_decorations[m].is_row_major;
                common_flags.is_column_major &= p_type_node->member_decorations[m].is_column_major;
                common_flags.is_built_in &= p_type_node->member_decorations[m].is_built_in;
                common_flags.is_noperspective &= p_type_node->member_decorations[m].is_noperspective;
                common_flags.is_flat &= p_type_node->member_decorations[m].is_flat;
                common_flags.is_non_writable &= p_type_node->member_decorations[m].is_non_writable;
                common_flags.is_non_readable &= p_type_node->member_decorations[m].is_non_readable;
                common_flags.is_patch &= p_type_node->member_decorations[m].is_patch;
                common_flags.is_per_vertex &= p_type_node->member_decorations[m].is_per_vertex;
                common_flags.is_per_task &= p_type_node->member_decorations[m].is_per_task;
                common_flags.is_weight_texture &= p_type_node->member_decorations[m].is_weight_texture;
                common_flags.is_block_match_texture &= p_type_node->member_decorations[m].is_block_match_texture;
            }

            p_descriptor->decoration_flags |= ApplyDecorations(&common_flags);
        }

        // If this is in the StorageBuffer storage class, it's for sure a storage
        // buffer descriptor. We need to handle this case earlier because in SPIR-V
        // there are two ways to indicate a storage buffer:
        // 1) Uniform storage class + BufferBlock decoration, or
        // 2) StorageBuffer storage class + Buffer decoration.
        // The 1) way is deprecated since SPIR-V v1.3. But the Buffer decoration is
        // also used together with Uniform storage class to mean uniform buffer..
        // We'll handle the pre-v1.3 cases in ParseDescriptorType().
        if (pointer_storage_class == SpvStorageClassStorageBuffer) {
            p_descriptor->descriptor_type = SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        }

        // Copy image traits
        if ((p_type->type_flags & SPV_REFLECT_TYPE_FLAG_EXTERNAL_MASK) == SPV_REFLECT_TYPE_FLAG_EXTERNAL_IMAGE) {
            memcpy(&p_descriptor->image, &p_type->traits.image, sizeof(p_descriptor->image));
        }

        // This is a workaround for: https://github.com/KhronosGroup/glslang/issues/1096
        {
            const uint32_t resource_mask = SPV_REFLECT_TYPE_FLAG_EXTERNAL_SAMPLED_IMAGE | SPV_REFLECT_TYPE_FLAG_EXTERNAL_IMAGE;
            if ((p_type->type_flags & resource_mask) == resource_mask) {
                memcpy(&p_descriptor->image, &p_type->traits.image, sizeof(p_descriptor->image));
            }
        }

        // Copy array traits
        if (p_type->traits.array.dims_count > 0) {
            p_descriptor->array.dims_count = p_type->traits.array.dims_count;
            for (uint32_t dim_index = 0; dim_index < p_type->traits.array.dims_count; ++dim_index) {
                uint32_t dim_value = p_type->traits.array.dims[dim_index];
                p_descriptor->array.dims[dim_index] = dim_value;
                p_descriptor->count *= dim_value;
            }
        }

        // Count

        p_descriptor->word_offset.binding = p_node->decorations.binding.word_offset;
        p_descriptor->word_offset.set = p_node->decorations.set.word_offset;

        ++descriptor_index;
    }

    if (p_module->descriptor_binding_count > 0) {
        qsort(p_module->descriptor_bindings, p_module->descriptor_binding_count, sizeof(*(p_module->descriptor_bindings)),
            SortCompareDescriptorBinding);
    }

    return SPV_REFLECT_RESULT_SUCCESS;
}

static SpvReflectResult ParseDescriptorType(SpvReflectShaderModule *p_module) {
    if (p_module->descriptor_binding_count == 0) {
        return SPV_REFLECT_RESULT_SUCCESS;
    }

    for (uint32_t descriptor_index = 0; descriptor_index < p_module->descriptor_binding_count; ++descriptor_index) {
        SpvReflectDescriptorBinding *p_descriptor = &(p_module->descriptor_bindings[descriptor_index]);
        SpvReflectTypeDescription *p_type = p_descriptor->type_description;

        if ((int)p_descriptor->descriptor_type == (int)INVALID_VALUE) {
            switch (p_type->type_flags & SPV_REFLECT_TYPE_FLAG_EXTERNAL_MASK) {
            default:
                assert(false && "unknown type flag");
                break;

            case SPV_REFLECT_TYPE_FLAG_EXTERNAL_IMAGE: {
                if (p_descriptor->image.dim == SpvDimBuffer) {
                    switch (p_descriptor->image.sampled) {
                    default:
                        assert(false && "unknown texel buffer sampled value");
                        break;
                    case IMAGE_SAMPLED:
                        p_descriptor->descriptor_type = SPV_REFLECT_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER;
                        break;
                    case IMAGE_STORAGE:
                        p_descriptor->descriptor_type = SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER;
                        break;
                    }
                } else if (p_descriptor->image.dim == SpvDimSubpassData) {
                    p_descriptor->descriptor_type = SPV_REFLECT_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
                } else {
                    switch (p_descriptor->image.sampled) {
                    default:
                        assert(false && "unknown image sampled value");
                        break;
                    case IMAGE_SAMPLED:
                        p_descriptor->descriptor_type = SPV_REFLECT_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
                        break;
                    case IMAGE_STORAGE:
                        p_descriptor->descriptor_type = SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_IMAGE;
                        break;
                    }
                }
            } break;

            case SPV_REFLECT_TYPE_FLAG_EXTERNAL_SAMPLER: {
                p_descriptor->descriptor_type = SPV_REFLECT_DESCRIPTOR_TYPE_SAMPLER;
            } break;

            case (SPV_REFLECT_TYPE_FLAG_EXTERNAL_SAMPLED_IMAGE | SPV_REFLECT_TYPE_FLAG_EXTERNAL_IMAGE): {
                // This is a workaround for: https://github.com/KhronosGroup/glslang/issues/1096
                if (p_descriptor->image.dim == SpvDimBuffer) {
                    switch (p_descriptor->image.sampled) {
                    default:
                        assert(false && "unknown texel buffer sampled value");
                        break;
                    case IMAGE_SAMPLED:
                        p_descriptor->descriptor_type = SPV_REFLECT_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER;
                        break;
                    case IMAGE_STORAGE:
                        p_descriptor->descriptor_type = SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER;
                        break;
                    }
                } else {
                    p_descriptor->descriptor_type = SPV_REFLECT_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
                }
            } break;

            case SPV_REFLECT_TYPE_FLAG_EXTERNAL_BLOCK: {
                if (p_type->decoration_flags & SPV_REFLECT_DECORATION_BLOCK) {
                    p_descriptor->descriptor_type = SPV_REFLECT_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
                } else if (p_type->decoration_flags & SPV_REFLECT_DECORATION_BUFFER_BLOCK) {
                    p_descriptor->descriptor_type = SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_BUFFER;
                } else {
                    assert(false && "unknown struct");
                }
            } break;

            case SPV_REFLECT_TYPE_FLAG_EXTERNAL_ACCELERATION_STRUCTURE: {
                p_descriptor->descriptor_type = SPV_REFLECT_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
            } break;
            }
        }

        switch (p_descriptor->descriptor_type) {
        case SPV_REFLECT_DESCRIPTOR_TYPE_SAMPLER:
            p_descriptor->resource_type = SPV_REFLECT_RESOURCE_FLAG_SAMPLER;
            break;
        case SPV_REFLECT_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
            p_descriptor->resource_type = (SpvReflectResourceType)(SPV_REFLECT_RESOURCE_FLAG_SAMPLER | SPV_REFLECT_RESOURCE_FLAG_SRV);
            break;
        case SPV_REFLECT_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
            p_descriptor->resource_type = SPV_REFLECT_RESOURCE_FLAG_SRV;
            break;
        case SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_IMAGE:
            p_descriptor->resource_type = SPV_REFLECT_RESOURCE_FLAG_UAV;
            break;
        case SPV_REFLECT_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER:
            p_descriptor->resource_type = SPV_REFLECT_RESOURCE_FLAG_SRV;
            break;
        case SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER:
            p_descriptor->resource_type = SPV_REFLECT_RESOURCE_FLAG_UAV;
            break;
        case SPV_REFLECT_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
            p_descriptor->resource_type = SPV_REFLECT_RESOURCE_FLAG_CBV;
            break;
        case SPV_REFLECT_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC:
            p_descriptor->resource_type = SPV_REFLECT_RESOURCE_FLAG_CBV;
            break;
        case SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_BUFFER:
            p_descriptor->resource_type = SPV_REFLECT_RESOURCE_FLAG_UAV;
            break;
        case SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC:
            p_descriptor->resource_type = SPV_REFLECT_RESOURCE_FLAG_UAV;
            break;
        case SPV_REFLECT_DESCRIPTOR_TYPE_INPUT_ATTACHMENT:
            break;
        case SPV_REFLECT_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR:
            p_descriptor->resource_type = SPV_REFLECT_RESOURCE_FLAG_SRV;
            break;
        }
    }

    return SPV_REFLECT_RESULT_SUCCESS;
}

static SpvReflectResult ParseUAVCounterBindings(SpvReflectShaderModule *p_module) {
    char name[MAX_NODE_NAME_LENGTH];
    const char *k_count_tag = "@count";

    for (uint32_t descriptor_index = 0; descriptor_index < p_module->descriptor_binding_count; ++descriptor_index) {
        SpvReflectDescriptorBinding *p_descriptor = &(p_module->descriptor_bindings[descriptor_index]);

        if (p_descriptor->descriptor_type != SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_BUFFER) {
            continue;
        }

        SpvReflectDescriptorBinding *p_counter_descriptor = NULL;
        // Use UAV counter buffer id if present...
        if (p_descriptor->uav_counter_id != UINT32_MAX) {
            for (uint32_t counter_descriptor_index = 0; counter_descriptor_index < p_module->descriptor_binding_count;
                ++counter_descriptor_index) {
                SpvReflectDescriptorBinding *p_test_counter_descriptor = &(p_module->descriptor_bindings[counter_descriptor_index]);
                if (p_test_counter_descriptor->descriptor_type != SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_BUFFER) {
                    continue;
                }
                if (p_descriptor->uav_counter_id == p_test_counter_descriptor->spirv_id) {
                    p_counter_descriptor = p_test_counter_descriptor;
                    break;
                }
            }
        }
        // ...otherwise use old @count convention.
        else {
            const size_t descriptor_name_length = p_descriptor->name ? strlen(p_descriptor->name) : 0;

            memset(name, 0, MAX_NODE_NAME_LENGTH);
            memcpy(name, p_descriptor->name, descriptor_name_length);
#if defined(_WIN32)
            strcat_s(name, MAX_NODE_NAME_LENGTH, k_count_tag);
#else
            strcat(name, k_count_tag);
#endif

            for (uint32_t counter_descriptor_index = 0; counter_descriptor_index < p_module->descriptor_binding_count;
                ++counter_descriptor_index) {
                SpvReflectDescriptorBinding *p_test_counter_descriptor = &(p_module->descriptor_bindings[counter_descriptor_index]);
                if (p_test_counter_descriptor->descriptor_type != SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_BUFFER) {
                    continue;
                }
                if (p_test_counter_descriptor->name && strcmp(name, p_test_counter_descriptor->name) == 0) {
                    p_counter_descriptor = p_test_counter_descriptor;
                    break;
                }
            }
        }

        if (p_counter_descriptor != NULL) {
            p_descriptor->uav_counter_binding = p_counter_descriptor;
        }
    }

    return SPV_REFLECT_RESULT_SUCCESS;
}

static SpvReflectResult ParseDescriptorBlockVariable(SpvReflectPrvParser *p_parser, SpvReflectShaderModule *p_module,
    SpvReflectTypeDescription *p_type, SpvReflectBlockVariable *p_var) {
    bool has_non_writable = false;

    if (IsNotNull(p_type->members) && (p_type->member_count > 0)) {
        p_var->member_count = p_type->member_count;
        p_var->members = (SpvReflectBlockVariable *)calloc(p_var->member_count, sizeof(*p_var->members));
        if (IsNull(p_var->members)) {
            return SPV_REFLECT_RESULT_ERROR_ALLOC_FAILED;
        }

        SpvReflectPrvNode *p_type_node = FindNode(p_parser, p_type->id);
        if (IsNull(p_type_node)) {
            return SPV_REFLECT_RESULT_ERROR_SPIRV_INVALID_ID_REFERENCE;
        }
        // Resolve to element type if current type is array or run time array
        while (p_type_node->op == SpvOpTypeArray || p_type_node->op == SpvOpTypeRuntimeArray) {
            if (p_type_node->op == SpvOpTypeArray) {
                p_type_node = FindNode(p_parser, p_type_node->array_traits.element_type_id);
            } else {
                // Element type description
                SpvReflectTypeDescription *p_type_temp = FindType(p_module, p_type_node->array_traits.element_type_id);
                if (IsNull(p_type_temp)) {
                    return SPV_REFLECT_RESULT_ERROR_SPIRV_INVALID_ID_REFERENCE;
                }
                // Element type node
                p_type_node = FindNode(p_parser, p_type_temp->id);
            }
            if (IsNull(p_type_node)) {
                return SPV_REFLECT_RESULT_ERROR_SPIRV_INVALID_ID_REFERENCE;
            }
        }

        // Parse members
        for (uint32_t member_index = 0; member_index < p_type->member_count; ++member_index) {
            SpvReflectTypeDescription *p_member_type = &p_type->members[member_index];
            SpvReflectBlockVariable *p_member_var = &p_var->members[member_index];
            // If pointer type, treat like reference and resolve to pointee type
            SpvReflectTypeDescription *p_member_ptr_type = 0;
            bool found_recursion = false;

            if ((p_member_type->storage_class == SpvStorageClassPhysicalStorageBuffer) &&
                (p_member_type->type_flags & SPV_REFLECT_TYPE_FLAG_REF)) {
                // Remember the original type
                p_member_ptr_type = p_member_type;

                // strip array
                if (p_member_type->op == SpvOpTypeArray || p_member_type->op == SpvOpTypeRuntimeArray) {
                    SpvReflectPrvNode *p_node = FindNode(p_parser, p_member_type->id);
                    if (p_node == NULL) {
                        return SPV_REFLECT_RESULT_ERROR_SPIRV_INVALID_ID_REFERENCE;
                    }
                    uint32_t element_type_id = p_node->array_traits.element_type_id;
                    p_member_type = FindType(p_module, element_type_id);
                    if (p_member_type == NULL) {
                        return SPV_REFLECT_RESULT_ERROR_SPIRV_INVALID_ID_REFERENCE;
                    }
                }

                // Need to make sure we haven't started an infinite recursive loop
                for (uint32_t i = 0; i < p_parser->physical_pointer_count; i++) {
                    if (p_member_type->id == p_parser->physical_pointer_check[i]->id) {
                        found_recursion = true;
                        break;  // still need to fill in p_member_type values
                    }
                }
                if (!found_recursion) {
                    SpvReflectTypeDescription *struct_type = FindType(p_module, p_member_type->id);
                    // could be pointer directly to non-struct type here
                    if (struct_type->struct_type_description) {
                        uint32_t struct_id = struct_type->struct_type_description->id;
                        p_parser->physical_pointer_structs[p_parser->physical_pointer_struct_count].struct_id = struct_id;
                        p_parser->physical_pointer_structs[p_parser->physical_pointer_struct_count].p_var = p_member_var;
                        p_parser->physical_pointer_struct_count++;

                        p_parser->physical_pointer_check[p_parser->physical_pointer_count] = p_member_type;
                        p_parser->physical_pointer_count++;
                        if (p_parser->physical_pointer_count >= MAX_RECURSIVE_PHYSICAL_POINTER_CHECK) {
                            return SPV_REFLECT_RESULT_ERROR_SPIRV_MAX_RECURSIVE_EXCEEDED;
                        }
                    }
                }

                SpvReflectPrvNode *p_member_type_node = FindNode(p_parser, p_member_type->id);
                if (IsNull(p_member_type_node)) {
                    return SPV_REFLECT_RESULT_ERROR_SPIRV_INVALID_ID_REFERENCE;
                }
                // Should be the pointee type
                p_member_type = FindType(p_module, p_member_type_node->type_id);
                if (IsNull(p_member_type)) {
                    return SPV_REFLECT_RESULT_ERROR_SPIRV_INVALID_ID_REFERENCE;
                }
            }
            bool is_struct = (p_member_type->type_flags & SPV_REFLECT_TYPE_FLAG_STRUCT) == SPV_REFLECT_TYPE_FLAG_STRUCT;
            if (is_struct) {
                if (!found_recursion) {
                    SpvReflectResult result = ParseDescriptorBlockVariable(p_parser, p_module, p_member_type, p_member_var);
                    if (result != SPV_REFLECT_RESULT_SUCCESS) {
                        return result;
                    }
                } else {
                    // if 2 member of structs are same PhysicalPointer type, copy the
                    // members values that aren't found skipping the recursion call
                    for (uint32_t i = 0; i < p_parser->physical_pointer_struct_count; i++) {
                        if (p_parser->physical_pointer_structs[i].struct_id == p_member_type->id) {
                            p_member_var->members = p_parser->physical_pointer_structs[i].p_var->members;
                            p_member_var->member_count = p_parser->physical_pointer_structs[i].p_var->member_count;
                            // Set here as it is the first time we need to walk down structs
                            p_member_var->flags |= SPV_REFLECT_VARIABLE_FLAGS_PHYSICAL_POINTER_COPY;
                        }
                    }
                }
            }

            if (p_type_node->storage_class == SpvStorageClassPhysicalStorageBuffer && !p_type_node->member_names) {
                // TODO 212 - If a buffer ref has an array of itself, all members are null
                continue;
            }

            p_member_var->name = p_type_node->member_names[member_index];
            p_member_var->offset = p_type_node->member_decorations[member_index].offset.value;
            p_member_var->decoration_flags = ApplyDecorations(&p_type_node->member_decorations[member_index]);
            p_member_var->flags |= SPV_REFLECT_VARIABLE_FLAGS_UNUSED;
            if (!has_non_writable && (p_member_var->decoration_flags & SPV_REFLECT_DECORATION_NON_WRITABLE)) {
                has_non_writable = true;
            }
            ApplyNumericTraits(p_member_type, &p_member_var->numeric);
            if (p_member_type->op == SpvOpTypeArray) {
                ApplyArrayTraits(p_member_type, &p_member_var->array);
            }

            p_member_var->word_offset.offset = p_type_node->member_decorations[member_index].offset.word_offset;
            p_member_var->type_description = p_member_ptr_type ? p_member_ptr_type : p_member_type;
        }
    }

    p_var->name = p_type->type_name;
    p_var->type_description = p_type;
    if (has_non_writable) {
        p_var->decoration_flags |= SPV_REFLECT_DECORATION_NON_WRITABLE;
    }

    return SPV_REFLECT_RESULT_SUCCESS;
}

static uint32_t GetPhysicalPointerStructSize(SpvReflectPrvParser *p_parser, uint32_t id) {
    for (uint32_t i = 0; i < p_parser->physical_pointer_struct_count; i++) {
        if (p_parser->physical_pointer_structs[i].struct_id == id) {
            return p_parser->physical_pointer_structs[i].p_var->size;
        }
    }
    return 0;
}

static SpvReflectResult ParseDescriptorBlockVariableSizes(SpvReflectPrvParser *p_parser, SpvReflectShaderModule *p_module,
    bool is_parent_root, bool is_parent_aos, bool is_parent_rta,
    SpvReflectBlockVariable *p_var) {
    if (p_var->member_count == 0) {
        return SPV_REFLECT_RESULT_SUCCESS;
    }

    bool is_parent_ref = p_var->type_description->op == SpvOpTypePointer;

    // Absolute offsets
    for (uint32_t member_index = 0; member_index < p_var->member_count; ++member_index) {
        SpvReflectBlockVariable *p_member_var = &p_var->members[member_index];
        if (is_parent_root) {
            p_member_var->absolute_offset = p_member_var->offset;
        } else {
            p_member_var->absolute_offset =
                is_parent_aos ? 0 : (is_parent_ref ? p_member_var->offset : p_member_var->offset + p_var->absolute_offset);
        }
    }

    // Size
    for (uint32_t member_index = 0; member_index < p_var->member_count; ++member_index) {
        SpvReflectBlockVariable *p_member_var = &p_var->members[member_index];
        SpvReflectTypeDescription *p_member_type = p_member_var->type_description;

        if (!p_member_type) {
            // TODO 212 - If a buffer ref has an array of itself, all members are null
            continue;
        }
        switch (p_member_type->op) {
        case SpvOpTypeBool: {
            p_member_var->size = SPIRV_WORD_SIZE;
        } break;

        case SpvOpTypeInt:
        case SpvOpTypeFloat: {
            p_member_var->size = p_member_type->traits.numeric.scalar.width / SPIRV_BYTE_WIDTH;
        } break;

        case SpvOpTypeVector: {
            uint32_t size =
                p_member_type->traits.numeric.vector.component_count * (p_member_type->traits.numeric.scalar.width / SPIRV_BYTE_WIDTH);
            p_member_var->size = size;
        } break;

        case SpvOpTypeMatrix: {
            if (p_member_var->decoration_flags & SPV_REFLECT_DECORATION_COLUMN_MAJOR) {
                p_member_var->size = p_member_var->numeric.matrix.column_count * p_member_var->numeric.matrix.stride;
            } else if (p_member_var->decoration_flags & SPV_REFLECT_DECORATION_ROW_MAJOR) {
                p_member_var->size = p_member_var->numeric.matrix.row_count * p_member_var->numeric.matrix.stride;
            }
        } break;

        case SpvOpTypeArray: {
            // If array of structs, parse members first...
            bool is_struct = (p_member_type->type_flags & SPV_REFLECT_TYPE_FLAG_STRUCT) == SPV_REFLECT_TYPE_FLAG_STRUCT;
            if (is_struct) {
                if (p_member_var->flags & SPV_REFLECT_VARIABLE_FLAGS_PHYSICAL_POINTER_COPY) {
                    p_member_var->size = GetPhysicalPointerStructSize(p_parser, p_member_type->id);
                } else {
                    SpvReflectResult result =
                        ParseDescriptorBlockVariableSizes(p_parser, p_module, false, true, is_parent_rta, p_member_var);
                    if (result != SPV_REFLECT_RESULT_SUCCESS) {
                        return result;
                    }
                }
            }
            // ...then array
            uint32_t element_count = (p_member_var->array.dims_count > 0 ? 1 : 0);
            for (uint32_t i = 0; i < p_member_var->array.dims_count; ++i) {
                element_count *= p_member_var->array.dims[i];
            }
            p_member_var->size = element_count * p_member_var->array.stride;
        } break;

        case SpvOpTypeRuntimeArray: {
            bool is_struct = (p_member_type->type_flags & SPV_REFLECT_TYPE_FLAG_STRUCT) == SPV_REFLECT_TYPE_FLAG_STRUCT;
            if (is_struct) {
                SpvReflectResult result = ParseDescriptorBlockVariableSizes(p_parser, p_module, false, true, true, p_member_var);
                if (result != SPV_REFLECT_RESULT_SUCCESS) {
                    return result;
                }
            }
        } break;

        case SpvOpTypePointer: {
            // Reference. Get to underlying struct type.
            SpvReflectPrvNode *p_member_type_node = FindNode(p_parser, p_member_type->id);
            if (IsNull(p_member_type_node)) {
                return SPV_REFLECT_RESULT_ERROR_SPIRV_INVALID_ID_REFERENCE;
            }
            // Get the pointee type
            p_member_type = FindType(p_module, p_member_type_node->type_id);
            if (IsNull(p_member_type)) {
                return SPV_REFLECT_RESULT_ERROR_SPIRV_INVALID_ID_REFERENCE;
            }

            // If we found a struct, we need to fall through and get the size of it or else we grab the size here
            if (p_member_type->op != SpvOpTypeStruct) {
                // If we hit this, we are seeing a POD pointer and the size is fixed
                p_member_var->size = SPIRV_PHYSICAL_STORAGE_POINTER_SIZE;
                break;
            }
            FALLTHROUGH;
        }

        case SpvOpTypeStruct: {
            if (p_member_var->flags & SPV_REFLECT_VARIABLE_FLAGS_PHYSICAL_POINTER_COPY) {
                p_member_var->size = GetPhysicalPointerStructSize(p_parser, p_member_type->id);
            } else {
                SpvReflectResult result =
                    ParseDescriptorBlockVariableSizes(p_parser, p_module, false, is_parent_aos, is_parent_rta, p_member_var);
                if (result != SPV_REFLECT_RESULT_SUCCESS) {
                    return result;
                }
            }
        } break;

        default:
            break;
        }
    }

    // Structs can offset order don't need to match the index order, so first order by offset
    // example:
    //     OpMemberDecorate %struct 0 Offset 4
    //     OpMemberDecorate %struct 1 Offset 0
    SpvReflectBlockVariable **pp_member_offset_order =
        (SpvReflectBlockVariable **)calloc(p_var->member_count, sizeof(SpvReflectBlockVariable *));
    if (IsNull(pp_member_offset_order)) {
        return SPV_REFLECT_RESULT_ERROR_ALLOC_FAILED;
    }

    uint32_t bottom_bound = 0;
    for (uint32_t i = 0; i < p_var->member_count; ++i) {
        uint32_t lowest_offset = UINT32_MAX;
        uint32_t member_index = 0;
        for (uint32_t j = 0; j < p_var->member_count; ++j) {
            const uint32_t offset = p_var->members[j].offset;
            if (offset < lowest_offset && offset >= bottom_bound) {
                member_index = j;
                lowest_offset = offset;
            }
        }
        pp_member_offset_order[i] = &p_var->members[member_index];
        bottom_bound = lowest_offset + 1;  // 2 index can't share the same offset
    }

    // Parse padded size using offset difference for all member except for the last entry...
    for (uint32_t i = 0; i < (p_var->member_count - 1); ++i) {
        SpvReflectBlockVariable *p_member_var = pp_member_offset_order[i];
        SpvReflectBlockVariable *p_next_member_var = pp_member_offset_order[i + 1];
        p_member_var->padded_size = p_next_member_var->offset - p_member_var->offset;
        if (p_member_var->size > p_member_var->padded_size) {
            p_member_var->size = p_member_var->padded_size;
        }
        if (is_parent_rta) {
            p_member_var->padded_size = p_member_var->size;
        }
    }

    // ...last entry just gets rounded up to near multiple of SPIRV_DATA_ALIGNMENT, which is 16 and
    // subtract the offset.
    // last entry == entry with largest offset value
    SpvReflectBlockVariable *p_last_member_var = pp_member_offset_order[p_var->member_count - 1];
    p_last_member_var->padded_size =
        RoundUp(p_last_member_var->offset + p_last_member_var->size, SPIRV_DATA_ALIGNMENT) - p_last_member_var->offset;
    if (p_last_member_var->size > p_last_member_var->padded_size) {
        p_last_member_var->size = p_last_member_var->padded_size;
    }
    if (is_parent_rta) {
        p_last_member_var->padded_size = p_last_member_var->size;
    }

    SafeFree(pp_member_offset_order);

    // If buffer ref, sizes are same as uint64_t
    if (is_parent_ref) {
        p_var->size = p_var->padded_size = 8;
        return SPV_REFLECT_RESULT_SUCCESS;
    }

    // @TODO validate this with assertion
    p_var->size = p_last_member_var->offset + p_last_member_var->padded_size;
    p_var->padded_size = p_var->size;

    return SPV_REFLECT_RESULT_SUCCESS;
}

static void MarkSelfAndAllMemberVarsAsUsed(SpvReflectBlockVariable *p_var) {
    // Clear the current variable's UNUSED flag
    p_var->flags &= ~SPV_REFLECT_VARIABLE_FLAGS_UNUSED;

    SpvOp op_type = p_var->type_description->op;
    if (op_type == SpvOpTypeStruct) {
        for (uint32_t i = 0; i < p_var->member_count; ++i) {
            SpvReflectBlockVariable *p_member_var = &p_var->members[i];
            MarkSelfAndAllMemberVarsAsUsed(p_member_var);
        }
    }
}

static SpvReflectResult ParseDescriptorBlockVariableUsage(SpvReflectPrvParser *p_parser, SpvReflectShaderModule *p_module,
    SpvReflectPrvAccessChain *p_access_chain, uint32_t index_index,
    SpvOp override_op_type, SpvReflectBlockVariable *p_var) {
    // Clear the current variable's UNUSED flag
    p_var->flags &= ~SPV_REFLECT_VARIABLE_FLAGS_UNUSED;

    // Parsing arrays requires overriding the op type for
    // for the lowest dim's element type.
    SpvReflectTypeDescription *p_type = p_var->type_description;
    SpvOp op_type = p_type->op;
    if (override_op_type != (SpvOp)INVALID_VALUE) {
        op_type = override_op_type;
    }

    switch (op_type) {
    default:
        break;

    case SpvOpTypeArray: {
        // Parse through array's type hierarchy to find the actual/non-array element type
        while ((p_type->op == SpvOpTypeArray) && (index_index < p_access_chain->index_count)) {
            // Find the array element type id
            SpvReflectPrvNode *p_node = FindNode(p_parser, p_type->id);
            if (p_node == NULL) {
                return SPV_REFLECT_RESULT_ERROR_SPIRV_INVALID_ID_REFERENCE;
            }
            uint32_t element_type_id = p_node->array_traits.element_type_id;
            // Get the array element type
            p_type = FindType(p_module, element_type_id);
            if (p_type == NULL) {
                return SPV_REFLECT_RESULT_ERROR_SPIRV_INVALID_ID_REFERENCE;
            }
            // Next access chain index
            index_index += 1;
        }

        // Only continue parsing if there's remaining indices in the access
        // chain. If the end of the access chain has been reached then all
        // remaining variables (including those in struct hierarchies)
        // are considered USED.
        //
        // See: https://github.com/KhronosGroup/SPIRV-Reflect/issues/78
        //
        if (index_index < p_access_chain->index_count) {
            // Parse current var again with a type override and advanced index index
            SpvReflectResult result =
                ParseDescriptorBlockVariableUsage(p_parser, p_module, p_access_chain, index_index, p_type->op, p_var);
            if (result != SPV_REFLECT_RESULT_SUCCESS) {
                return result;
            }
        } else {
            // Clear UNUSED flag for remaining variables
            MarkSelfAndAllMemberVarsAsUsed(p_var);
        }
    } break;

    case SpvOpTypePointer: {
        // Reference. Get to underlying struct type.
        SpvReflectPrvNode *p_type_node = FindNode(p_parser, p_type->id);
        if (IsNull(p_type_node)) {
            return SPV_REFLECT_RESULT_ERROR_SPIRV_INVALID_ID_REFERENCE;
        }
        // Get the pointee type
        p_type = FindType(p_module, p_type_node->type_id);
        if (IsNull(p_type)) {
            return SPV_REFLECT_RESULT_ERROR_SPIRV_INVALID_ID_REFERENCE;
        }
        if (p_type->op != SpvOpTypeStruct) {
            break;
        }
        FALLTHROUGH;
    }

    case SpvOpTypeStruct: {
        assert(p_var->member_count > 0);
        if (p_var->member_count == 0) {
            return SPV_REFLECT_RESULT_ERROR_SPIRV_UNEXPECTED_BLOCK_DATA;
        }

        // The access chain can have zero indexes, if used for a runtime array
        if (p_access_chain->index_count == 0) {
            return SPV_REFLECT_RESULT_SUCCESS;
        }

        // Get member variable at the access's chain current index
        uint32_t index = p_access_chain->indexes[index_index];
        if (index >= p_var->member_count) {
            return SPV_REFLECT_RESULT_ERROR_SPIRV_INVALID_BLOCK_MEMBER_REFERENCE;
        }
        SpvReflectBlockVariable *p_member_var = &p_var->members[index];

        bool is_pointer_to_pointer = IsPointerToPointer(p_parser, p_access_chain->result_type_id);
        if (is_pointer_to_pointer) {
            // Remember block var for this access chain for downstream dereference
            p_access_chain->block_var = p_member_var;
        }

        // Next access chain index
        index_index += 1;

        // Only continue parsing if there's remaining indices in the access
        // chain. If the end of the access chain has been reach then all
        // remaining variables (including those in struct hierarchies)
        // are considered USED.
        //
        // See: https://github.com/KhronosGroup/SPIRV-Reflect/issues/78
        //
        if (index_index < p_access_chain->index_count) {
            SpvReflectResult result =
                ParseDescriptorBlockVariableUsage(p_parser, p_module, p_access_chain, index_index, (SpvOp)INVALID_VALUE, p_member_var);
            if (result != SPV_REFLECT_RESULT_SUCCESS) {
                return result;
            }
        } else if (is_pointer_to_pointer) {
            // Clear UNUSED flag, but only for the pointer
            p_member_var->flags &= ~SPV_REFLECT_VARIABLE_FLAGS_UNUSED;
        } else {
            // Clear UNUSED flag for remaining variables
            MarkSelfAndAllMemberVarsAsUsed(p_member_var);
        }
    } break;
    }

    return SPV_REFLECT_RESULT_SUCCESS;
}

static SpvReflectResult ParseDescriptorBlocks(SpvReflectPrvParser *p_parser, SpvReflectShaderModule *p_module) {
    if (p_module->descriptor_binding_count == 0) {
        return SPV_REFLECT_RESULT_SUCCESS;
    }

    p_parser->physical_pointer_struct_count = 0;

    for (uint32_t descriptor_index = 0; descriptor_index < p_module->descriptor_binding_count; ++descriptor_index) {
        SpvReflectDescriptorBinding *p_descriptor = &(p_module->descriptor_bindings[descriptor_index]);
        SpvReflectTypeDescription *p_type = p_descriptor->type_description;
        if ((p_descriptor->descriptor_type != SPV_REFLECT_DESCRIPTOR_TYPE_UNIFORM_BUFFER) &&
            (p_descriptor->descriptor_type != SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_BUFFER)) {
            continue;
        }

        // Mark UNUSED
        p_descriptor->block.flags |= SPV_REFLECT_VARIABLE_FLAGS_UNUSED;
        p_parser->physical_pointer_count = 0;
        // Parse descriptor block
        SpvReflectResult result = ParseDescriptorBlockVariable(p_parser, p_module, p_type, &p_descriptor->block);
        if (result != SPV_REFLECT_RESULT_SUCCESS) {
            return result;
        }

        for (uint32_t access_chain_index = 0; access_chain_index < p_parser->access_chain_count; ++access_chain_index) {
            SpvReflectPrvAccessChain *p_access_chain = &(p_parser->access_chains[access_chain_index]);
            // Skip any access chains that aren't touching this descriptor block
            if (p_descriptor->spirv_id != p_access_chain->base_id) {
                continue;
            }
            result = ParseDescriptorBlockVariableUsage(p_parser, p_module, p_access_chain, 0, (SpvOp)INVALID_VALUE, &p_descriptor->block);
            if (result != SPV_REFLECT_RESULT_SUCCESS) {
                return result;
            }
        }

        p_descriptor->block.name = p_descriptor->name;

        bool is_parent_rta = (p_descriptor->descriptor_type == SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_BUFFER);
        result = ParseDescriptorBlockVariableSizes(p_parser, p_module, true, false, is_parent_rta, &p_descriptor->block);
        if (result != SPV_REFLECT_RESULT_SUCCESS) {
            return result;
        }

        if (is_parent_rta) {
            p_descriptor->block.size = 0;
            p_descriptor->block.padded_size = 0;
        }
    }

    return SPV_REFLECT_RESULT_SUCCESS;
}

static SpvReflectResult ParseFormat(const SpvReflectTypeDescription *p_type, SpvReflectFormat *p_format) {
    SpvReflectResult result = SPV_REFLECT_RESULT_ERROR_INTERNAL_ERROR;
    bool signedness = (p_type->traits.numeric.scalar.signedness != 0);
    uint32_t bit_width = p_type->traits.numeric.scalar.width;
    if (p_type->type_flags & SPV_REFLECT_TYPE_FLAG_VECTOR) {
        uint32_t component_count = p_type->traits.numeric.vector.component_count;
        if (p_type->type_flags & SPV_REFLECT_TYPE_FLAG_FLOAT) {
            switch (bit_width) {
            case 16: {
                switch (component_count) {
                case 2:
                    *p_format = SPV_REFLECT_FORMAT_R16G16_SFLOAT;
                    break;
                case 3:
                    *p_format = SPV_REFLECT_FORMAT_R16G16B16_SFLOAT;
                    break;
                case 4:
                    *p_format = SPV_REFLECT_FORMAT_R16G16B16A16_SFLOAT;
                    break;
                }
            } break;

            case 32: {
                switch (component_count) {
                case 2:
                    *p_format = SPV_REFLECT_FORMAT_R32G32_SFLOAT;
                    break;
                case 3:
                    *p_format = SPV_REFLECT_FORMAT_R32G32B32_SFLOAT;
                    break;
                case 4:
                    *p_format = SPV_REFLECT_FORMAT_R32G32B32A32_SFLOAT;
                    break;
                }
            } break;

            case 64: {
                switch (component_count) {
                case 2:
                    *p_format = SPV_REFLECT_FORMAT_R64G64_SFLOAT;
                    break;
                case 3:
                    *p_format = SPV_REFLECT_FORMAT_R64G64B64_SFLOAT;
                    break;
                case 4:
                    *p_format = SPV_REFLECT_FORMAT_R64G64B64A64_SFLOAT;
                    break;
                }
            }
            }
            result = SPV_REFLECT_RESULT_SUCCESS;
        } else if (p_type->type_flags & (SPV_REFLECT_TYPE_FLAG_INT | SPV_REFLECT_TYPE_FLAG_BOOL)) {
            switch (bit_width) {
            case 16: {
                switch (component_count) {
                case 2:
                    *p_format = signedness ? SPV_REFLECT_FORMAT_R16G16_SINT : SPV_REFLECT_FORMAT_R16G16_UINT;
                    break;
                case 3:
                    *p_format = signedness ? SPV_REFLECT_FORMAT_R16G16B16_SINT : SPV_REFLECT_FORMAT_R16G16B16_UINT;
                    break;
                case 4:
                    *p_format = signedness ? SPV_REFLECT_FORMAT_R16G16B16A16_SINT : SPV_REFLECT_FORMAT_R16G16B16A16_UINT;
                    break;
                }
            } break;

            case 32: {
                switch (component_count) {
                case 2:
                    *p_format = signedness ? SPV_REFLECT_FORMAT_R32G32_SINT : SPV_REFLECT_FORMAT_R32G32_UINT;
                    break;
                case 3:
                    *p_format = signedness ? SPV_REFLECT_FORMAT_R32G32B32_SINT : SPV_REFLECT_FORMAT_R32G32B32_UINT;
                    break;
                case 4:
                    *p_format = signedness ? SPV_REFLECT_FORMAT_R32G32B32A32_SINT : SPV_REFLECT_FORMAT_R32G32B32A32_UINT;
                    break;
                }
            } break;

            case 64: {
                switch (component_count) {
                case 2:
                    *p_format = signedness ? SPV_REFLECT_FORMAT_R64G64_SINT : SPV_REFLECT_FORMAT_R64G64_UINT;
                    break;
                case 3:
                    *p_format = signedness ? SPV_REFLECT_FORMAT_R64G64B64_SINT : SPV_REFLECT_FORMAT_R64G64B64_UINT;
                    break;
                case 4:
                    *p_format = signedness ? SPV_REFLECT_FORMAT_R64G64B64A64_SINT : SPV_REFLECT_FORMAT_R64G64B64A64_UINT;
                    break;
                }
            }
            }
            result = SPV_REFLECT_RESULT_SUCCESS;
        }
    } else if (p_type->type_flags & SPV_REFLECT_TYPE_FLAG_FLOAT) {
        switch (bit_width) {
        case 16:
            *p_format = SPV_REFLECT_FORMAT_R16_SFLOAT;
            break;
        case 32:
            *p_format = SPV_REFLECT_FORMAT_R32_SFLOAT;
            break;
        case 64:
            *p_format = SPV_REFLECT_FORMAT_R64_SFLOAT;
            break;
        }
        result = SPV_REFLECT_RESULT_SUCCESS;
    } else if (p_type->type_flags & (SPV_REFLECT_TYPE_FLAG_INT | SPV_REFLECT_TYPE_FLAG_BOOL)) {
        switch (bit_width) {
        case 16:
            *p_format = signedness ? SPV_REFLECT_FORMAT_R16_SINT : SPV_REFLECT_FORMAT_R16_UINT;
            break;
            break;
        case 32:
            *p_format = signedness ? SPV_REFLECT_FORMAT_R32_SINT : SPV_REFLECT_FORMAT_R32_UINT;
            break;
            break;
        case 64:
            *p_format = signedness ? SPV_REFLECT_FORMAT_R64_SINT : SPV_REFLECT_FORMAT_R64_UINT;
            break;
        }
        result = SPV_REFLECT_RESULT_SUCCESS;
    } else if (p_type->type_flags & SPV_REFLECT_TYPE_FLAG_STRUCT) {
        *p_format = SPV_REFLECT_FORMAT_UNDEFINED;
        result = SPV_REFLECT_RESULT_SUCCESS;
    }
    return result;
}

static SpvReflectResult ParseInterfaceVariable(SpvReflectPrvParser *p_parser,
    const SpvReflectPrvDecorations *p_var_node_decorations,
    const SpvReflectPrvDecorations *p_type_node_decorations,
    SpvReflectShaderModule *p_module, SpvReflectTypeDescription *p_type,
    SpvReflectInterfaceVariable *p_var, bool *p_has_built_in) {
    SpvReflectPrvNode *p_type_node = FindNode(p_parser, p_type->id);
    if (IsNull(p_type_node)) {
        return SPV_REFLECT_RESULT_ERROR_SPIRV_INVALID_ID_REFERENCE;
    }

    if (p_type->member_count > 0) {
        p_var->member_count = p_type->member_count;
        p_var->members = (SpvReflectInterfaceVariable *)calloc(p_var->member_count, sizeof(*p_var->members));
        if (IsNull(p_var->members)) {
            return SPV_REFLECT_RESULT_ERROR_ALLOC_FAILED;
        }

        for (uint32_t member_index = 0; member_index < p_type_node->member_count; ++member_index) {
            SpvReflectPrvDecorations *p_member_decorations = &p_type_node->member_decorations[member_index];
            SpvReflectTypeDescription *p_member_type = &p_type->members[member_index];
            SpvReflectInterfaceVariable *p_member_var = &p_var->members[member_index];

            // Storage class is the same throughout the whole struct
            p_member_var->storage_class = p_var->storage_class;

            SpvReflectResult result =
                ParseInterfaceVariable(p_parser, NULL, p_member_decorations, p_module, p_member_type, p_member_var, p_has_built_in);
            if (result != SPV_REFLECT_RESULT_SUCCESS) {
                SPV_REFLECT_ASSERT(false);
                return result;
            }
        }
    }

    p_var->name = p_type_node->name;
    p_var->decoration_flags = ApplyDecorations(p_type_node_decorations);
    if (p_var_node_decorations != NULL) {
        p_var->decoration_flags |= ApplyDecorations(p_var_node_decorations);
    } else {
        // Apply member decoration values to struct members
        p_var->location = p_type_node_decorations->location.value;
        p_var->component = p_type_node_decorations->component.value;
    }

    p_var->built_in = p_type_node_decorations->built_in;
    ApplyNumericTraits(p_type, &p_var->numeric);
    if (p_type->op == SpvOpTypeArray) {
        ApplyArrayTraits(p_type, &p_var->array);
    }

    p_var->type_description = p_type;

    *p_has_built_in |= p_type_node_decorations->is_built_in;

    // Only parse format for interface variables that are input or output
    if ((p_var->storage_class == SpvStorageClassInput) || (p_var->storage_class == SpvStorageClassOutput)) {
        SpvReflectResult result = ParseFormat(p_var->type_description, &p_var->format);
        if (result != SPV_REFLECT_RESULT_SUCCESS) {
            SPV_REFLECT_ASSERT(false);
            return result;
        }
    }

    return SPV_REFLECT_RESULT_SUCCESS;
}

static SpvReflectResult ParseInterfaceVariables(SpvReflectPrvParser *p_parser, SpvReflectShaderModule *p_module,
    SpvReflectEntryPoint *p_entry, uint32_t interface_variable_count,
    uint32_t *p_interface_variable_ids) {
    if (interface_variable_count == 0) {
        return SPV_REFLECT_RESULT_SUCCESS;
    }

    p_entry->interface_variable_count = interface_variable_count;
    p_entry->input_variable_count = 0;
    p_entry->output_variable_count = 0;
    for (size_t i = 0; i < interface_variable_count; ++i) {
        uint32_t var_result_id = *(p_interface_variable_ids + i);
        SpvReflectPrvNode *p_node = FindNode(p_parser, var_result_id);
        if (IsNull(p_node)) {
            return SPV_REFLECT_RESULT_ERROR_SPIRV_INVALID_ID_REFERENCE;
        }

        if (p_node->storage_class == SpvStorageClassInput) {
            p_entry->input_variable_count += 1;
        } else if (p_node->storage_class == SpvStorageClassOutput) {
            p_entry->output_variable_count += 1;
        }
    }

    if (p_entry->input_variable_count > 0) {
        p_entry->input_variables =
            (SpvReflectInterfaceVariable **)calloc(p_entry->input_variable_count, sizeof(*(p_entry->input_variables)));
        if (IsNull(p_entry->input_variables)) {
            return SPV_REFLECT_RESULT_ERROR_ALLOC_FAILED;
        }
    }

    if (p_entry->output_variable_count > 0) {
        p_entry->output_variables =
            (SpvReflectInterfaceVariable **)calloc(p_entry->output_variable_count, sizeof(*(p_entry->output_variables)));
        if (IsNull(p_entry->output_variables)) {
            return SPV_REFLECT_RESULT_ERROR_ALLOC_FAILED;
        }
    }

    if (p_entry->interface_variable_count > 0) {
        p_entry->interface_variables =
            (SpvReflectInterfaceVariable *)calloc(p_entry->interface_variable_count, sizeof(*(p_entry->interface_variables)));
        if (IsNull(p_entry->interface_variables)) {
            return SPV_REFLECT_RESULT_ERROR_ALLOC_FAILED;
        }
    }

    size_t input_index = 0;
    size_t output_index = 0;
    for (size_t i = 0; i < interface_variable_count; ++i) {
        uint32_t var_result_id = *(p_interface_variable_ids + i);
        SpvReflectPrvNode *p_node = FindNode(p_parser, var_result_id);
        if (IsNull(p_node)) {
            return SPV_REFLECT_RESULT_ERROR_SPIRV_INVALID_ID_REFERENCE;
        }

        SpvReflectTypeDescription *p_type = FindType(p_module, p_node->type_id);
        if (IsNull(p_node) || IsNull(p_type)) {
            return SPV_REFLECT_RESULT_ERROR_SPIRV_INVALID_ID_REFERENCE;
        }
        // If the type is a pointer, resolve it
        if (p_type->op == SpvOpTypePointer) {
            // Find the type's node
            SpvReflectPrvNode *p_type_node = FindNode(p_parser, p_type->id);
            if (IsNull(p_type_node)) {
                return SPV_REFLECT_RESULT_ERROR_SPIRV_INVALID_ID_REFERENCE;
            }
            // Should be the resolved type
            p_type = FindType(p_module, p_type_node->type_id);
            if (IsNull(p_type)) {
                return SPV_REFLECT_RESULT_ERROR_SPIRV_INVALID_ID_REFERENCE;
            }
        }

        SpvReflectPrvNode *p_type_node = FindNode(p_parser, p_type->id);
        if (IsNull(p_type_node)) {
            return SPV_REFLECT_RESULT_ERROR_SPIRV_INVALID_ID_REFERENCE;
        }

        SpvReflectInterfaceVariable *p_var = &(p_entry->interface_variables[i]);
        p_var->storage_class = p_node->storage_class;

        bool has_built_in = p_node->decorations.is_built_in;
        SpvReflectResult result =
            ParseInterfaceVariable(p_parser, &p_node->decorations, &p_type_node->decorations, p_module, p_type, p_var, &has_built_in);
        if (result != SPV_REFLECT_RESULT_SUCCESS) {
            SPV_REFLECT_ASSERT(false);
            return result;
        }

        // Input and output variables
        if (p_var->storage_class == SpvStorageClassInput) {
            p_entry->input_variables[input_index] = p_var;
            ++input_index;
        } else if (p_node->storage_class == SpvStorageClassOutput) {
            p_entry->output_variables[output_index] = p_var;
            ++output_index;
        }

        // SPIR-V result id
        p_var->spirv_id = p_node->result_id;
        // Name
        p_var->name = p_node->name;
        // Semantic
        p_var->semantic = p_node->decorations.semantic.value;

        // Decorate with built-in if any member is built-in
        if (has_built_in) {
            p_var->decoration_flags |= SPV_REFLECT_DECORATION_BUILT_IN;
        }

        // Location is decorated on OpVariable node, not the type node.
        p_var->location = p_node->decorations.location.value;
        p_var->component = p_node->decorations.component.value;
        p_var->word_offset.location = p_node->decorations.location.word_offset;

        // Built in
        if (p_node->decorations.is_built_in) {
            p_var->built_in = p_node->decorations.built_in;
        }
    }

    return SPV_REFLECT_RESULT_SUCCESS;
}

static SpvReflectResult EnumerateAllPushConstants(SpvReflectShaderModule *p_module, size_t *p_push_constant_count,
    uint32_t **p_push_constants) {
    *p_push_constant_count = p_module->push_constant_block_count;
    if (*p_push_constant_count == 0) {
        return SPV_REFLECT_RESULT_SUCCESS;
    }
    *p_push_constants = (uint32_t *)calloc(*p_push_constant_count, sizeof(**p_push_constants));

    if (IsNull(*p_push_constants)) {
        return SPV_REFLECT_RESULT_ERROR_ALLOC_FAILED;
    }

    for (size_t i = 0; i < *p_push_constant_count; ++i) {
        (*p_push_constants)[i] = p_module->push_constant_blocks[i].spirv_id;
    }
    qsort(*p_push_constants, *p_push_constant_count, sizeof(**p_push_constants), SortCompareUint32);
    return SPV_REFLECT_RESULT_SUCCESS;
}

static SpvReflectResult TraverseCallGraph(SpvReflectPrvParser *p_parser, SpvReflectPrvFunction *p_func, size_t *p_func_count,
    uint32_t *p_func_ids, uint32_t depth) {
    if (depth > p_parser->function_count) {
        // Vulkan does not permit recursion (Vulkan spec Appendix A):
        //   "Recursion: The static function-call graph for an entry point must not
        //    contain cycles."
        return SPV_REFLECT_RESULT_ERROR_SPIRV_RECURSION;
    }
    if (IsNotNull(p_func_ids)) {
        p_func_ids[(*p_func_count)++] = p_func->id;
    } else {
        ++*p_func_count;
    }
    for (size_t i = 0; i < p_func->callee_count; ++i) {
        SpvReflectResult result = TraverseCallGraph(p_parser, p_func->callee_ptrs[i], p_func_count, p_func_ids, depth + 1);
        if (result != SPV_REFLECT_RESULT_SUCCESS) {
            return result;
        }
    }
    return SPV_REFLECT_RESULT_SUCCESS;
}

static uint32_t GetUint32Constant(SpvReflectPrvParser *p_parser, uint32_t id) {
    uint32_t result = (uint32_t)INVALID_VALUE;
    SpvReflectPrvNode *p_node = FindNode(p_parser, id);
    if (p_node && p_node->op == SpvOpConstant) {
        UNCHECKED_READU32(p_parser, p_node->word_offset + 3, result);
    }
    return result;
}

static bool HasByteAddressBufferOffset(SpvReflectPrvNode *p_node, SpvReflectDescriptorBinding *p_binding) {
    return IsNotNull(p_node) && IsNotNull(p_binding) && p_node->op == SpvOpAccessChain && p_node->word_count == 6 &&
        (p_binding->user_type == SPV_REFLECT_USER_TYPE_BYTE_ADDRESS_BUFFER ||
            p_binding->user_type == SPV_REFLECT_USER_TYPE_RW_BYTE_ADDRESS_BUFFER);
}

static SpvReflectResult ParseByteAddressBuffer(SpvReflectPrvParser *p_parser, SpvReflectPrvNode *p_node,
    SpvReflectDescriptorBinding *p_binding) {
    const SpvReflectResult not_found = SPV_REFLECT_RESULT_SUCCESS;
    if (!HasByteAddressBufferOffset(p_node, p_binding)) {
        return not_found;
    }

    uint32_t offset = 0;  // starting offset

    uint32_t base_id = 0;
    // expect first index of 2D access is zero
    UNCHECKED_READU32(p_parser, p_node->word_offset + 4, base_id);
    if (GetUint32Constant(p_parser, base_id) != 0) {
        return not_found;
    }
    UNCHECKED_READU32(p_parser, p_node->word_offset + 5, base_id);
    SpvReflectPrvNode *p_next_node = FindNode(p_parser, base_id);
    if (IsNull(p_next_node)) {
        return not_found;
    } else if (p_next_node->op == SpvOpConstant) {
        // The access chain might just be a constant right to the offset
        offset = GetUint32Constant(p_parser, base_id);
        p_binding->byte_address_buffer_offsets[p_binding->byte_address_buffer_offset_count] = offset;
        p_binding->byte_address_buffer_offset_count++;
        return SPV_REFLECT_RESULT_SUCCESS;
    }

    // there is usually 2 (sometimes 3) instrucitons that make up the arithmetic logic to calculate the offset
    SpvReflectPrvNode *arithmetic_node_stack[8];
    uint32_t arithmetic_count = 0;

    while (IsNotNull(p_next_node)) {
        if (p_next_node->op == SpvOpLoad || p_next_node->op == SpvOpBitcast || p_next_node->op == SpvOpConstant) {
            break;  // arithmetic starts here
        }
        arithmetic_node_stack[arithmetic_count++] = p_next_node;
        if (arithmetic_count >= 8) {
            return not_found;
        }

        UNCHECKED_READU32(p_parser, p_next_node->word_offset + 3, base_id);
        p_next_node = FindNode(p_parser, base_id);
    }

    const uint32_t count = arithmetic_count;
    for (uint32_t i = 0; i < count; i++) {
        p_next_node = arithmetic_node_stack[--arithmetic_count];
        // All arithmetic ops takes 2 operands, assumption is the 2nd operand has the constant
        UNCHECKED_READU32(p_parser, p_next_node->word_offset + 4, base_id);
        uint32_t value = GetUint32Constant(p_parser, base_id);
        if (value == INVALID_VALUE) {
            return not_found;
        }

        switch (p_next_node->op) {
        case SpvOpShiftRightLogical:
            offset >>= value;
            break;
        case SpvOpIAdd:
            offset += value;
            break;
        case SpvOpISub:
            offset -= value;
            break;
        case SpvOpIMul:
            offset *= value;
            break;
        case SpvOpUDiv:
            offset /= value;
            break;
        case SpvOpSDiv:
            // OpConstant might be signed, but value should never be negative
            assert((int32_t)value > 0);
            offset /= value;
            break;
        default:
            return not_found;
        }
    }

    p_binding->byte_address_buffer_offsets[p_binding->byte_address_buffer_offset_count] = offset;
    p_binding->byte_address_buffer_offset_count++;
    return SPV_REFLECT_RESULT_SUCCESS;
}

static SpvReflectResult ParseFunctionParameterAccess(SpvReflectPrvParser *p_parser, uint32_t callee_function_id,
    uint32_t function_parameter_index, uint32_t *p_accessed) {
    SpvReflectPrvFunction *p_func = NULL;
    for (size_t i = 0; i < p_parser->function_count; ++i) {
        if (p_parser->functions[i].id == callee_function_id) {
            p_func = &(p_parser->functions[i]);
            break;
        }
    }
    if (p_func == NULL) {
        return SPV_REFLECT_RESULT_ERROR_SPIRV_INVALID_ID_REFERENCE;
    }

    assert(function_parameter_index < p_func->parameter_count);

    for (size_t i = 0; i < p_func->accessed_variable_count; ++i) {
        if (p_func->parameters[function_parameter_index] == p_func->accessed_variables[i].variable_ptr) {
            SpvReflectPrvAccessedVariable *p_var = &p_func->accessed_variables[i];
            if (p_var->function_id > 0) {
                SpvReflectResult result =
                    ParseFunctionParameterAccess(p_parser, p_var->function_id, p_var->function_parameter_index, p_accessed);
                if (result != SPV_REFLECT_RESULT_SUCCESS) {
                    return result;
                }
            } else {
                *p_accessed = 1;
            }
            // Early out as soon as p_accessed is true
            if (*p_accessed) {
                return SPV_REFLECT_RESULT_SUCCESS;
            }
        }
    }

    *p_accessed = 0;
    return SPV_REFLECT_RESULT_SUCCESS;
}

static SpvReflectResult ParseStaticallyUsedResources(SpvReflectPrvParser *p_parser, SpvReflectShaderModule *p_module,
    SpvReflectEntryPoint *p_entry, size_t uniform_count, uint32_t *uniforms,
    size_t push_constant_count, uint32_t *push_constants) {
    // Find function with the right id
    SpvReflectPrvFunction *p_func = NULL;
    for (size_t i = 0; i < p_parser->function_count; ++i) {
        if (p_parser->functions[i].id == p_entry->id) {
            p_func = &(p_parser->functions[i]);
            break;
        }
    }
    if (p_func == NULL) {
        return SPV_REFLECT_RESULT_ERROR_SPIRV_INVALID_ID_REFERENCE;
    }

    size_t called_function_count = 0;
    SpvReflectResult result = TraverseCallGraph(p_parser, p_func, &called_function_count, NULL, 0);
    if (result != SPV_REFLECT_RESULT_SUCCESS) {
        return result;
    }

    uint32_t *p_called_functions = NULL;
    if (called_function_count > 0) {
        p_called_functions = (uint32_t *)calloc(called_function_count, sizeof(*p_called_functions));
        if (IsNull(p_called_functions)) {
            return SPV_REFLECT_RESULT_ERROR_ALLOC_FAILED;
        }
    }

    called_function_count = 0;
    result = TraverseCallGraph(p_parser, p_func, &called_function_count, p_called_functions, 0);
    if (result != SPV_REFLECT_RESULT_SUCCESS) {
        SafeFree(p_called_functions);
        return result;
    }

    if (called_function_count > 0) {
        qsort(p_called_functions, called_function_count, sizeof(*p_called_functions), SortCompareUint32);
    }
    called_function_count = DedupSortedUint32(p_called_functions, called_function_count);

    uint32_t used_acessed_count = 0;
    for (size_t i = 0, j = 0; i < called_function_count; ++i) {
        // No need to bounds check j because a missing ID issue would have been
        // found during TraverseCallGraph
        while (p_parser->functions[j].id != p_called_functions[i]) {
            ++j;
        }
        used_acessed_count += p_parser->functions[j].accessed_variable_count;
    }
    SpvReflectPrvAccessedVariable *p_used_accesses = NULL;
    if (used_acessed_count > 0) {
        p_used_accesses = (SpvReflectPrvAccessedVariable *)calloc(used_acessed_count, sizeof(SpvReflectPrvAccessedVariable));
        if (IsNull(p_used_accesses)) {
            SafeFree(p_called_functions);
            return SPV_REFLECT_RESULT_ERROR_ALLOC_FAILED;
        }
    }
    used_acessed_count = 0;
    for (size_t i = 0, j = 0; i < called_function_count; ++i) {
        while (p_parser->functions[j].id != p_called_functions[i]) {
            ++j;
        }

        memcpy(&p_used_accesses[used_acessed_count], p_parser->functions[j].accessed_variables,
            p_parser->functions[j].accessed_variable_count * sizeof(SpvReflectPrvAccessedVariable));
        used_acessed_count += p_parser->functions[j].accessed_variable_count;
    }
    SafeFree(p_called_functions);

    if (used_acessed_count > 0) {
        qsort(p_used_accesses, used_acessed_count, sizeof(*p_used_accesses), SortCompareAccessedVariable);
    }

    // Do set intersection to find the used uniform and push constants
    size_t used_uniform_count = 0;
    result = IntersectSortedAccessedVariable(p_used_accesses, used_acessed_count, uniforms, uniform_count, &p_entry->used_uniforms,
        &used_uniform_count);
    if (result != SPV_REFLECT_RESULT_SUCCESS) {
        SafeFree(p_used_accesses);
        return result;
    }

    size_t used_push_constant_count = 0;
    result = IntersectSortedAccessedVariable(p_used_accesses, used_acessed_count, push_constants, push_constant_count,
        &p_entry->used_push_constants, &used_push_constant_count);
    if (result != SPV_REFLECT_RESULT_SUCCESS) {
        SafeFree(p_used_accesses);
        return result;
    }

    for (uint32_t i = 0; i < p_module->descriptor_binding_count; ++i) {
        SpvReflectDescriptorBinding *p_binding = &p_module->descriptor_bindings[i];
        uint32_t byte_address_buffer_offset_count = 0;

        for (uint32_t j = 0; j < used_acessed_count; j++) {
            SpvReflectPrvAccessedVariable *p_var = &p_used_accesses[j];
            if (p_var->variable_ptr == p_binding->spirv_id) {
                if (p_var->function_id > 0) {
                    result =
                        ParseFunctionParameterAccess(p_parser, p_var->function_id, p_var->function_parameter_index, &p_binding->accessed);
                    if (result != SPV_REFLECT_RESULT_SUCCESS) {
                        SafeFree(p_used_accesses);
                        return result;
                    }
                } else {
                    p_binding->accessed = 1;
                }

                if (HasByteAddressBufferOffset(p_used_accesses[j].p_node, p_binding)) {
                    byte_address_buffer_offset_count++;
                }
            }
        }

        // only if SPIR-V has ByteAddressBuffer user type
        if (byte_address_buffer_offset_count > 0) {
            bool multi_entrypoint = p_binding->byte_address_buffer_offset_count > 0;
            if (multi_entrypoint) {
                // If there is a 2nd entrypoint, we can have multiple entry points, in this case we want to just combine the accessed
                // offsets and then de-duplicate it
                uint32_t *prev_byte_address_buffer_offsets = p_binding->byte_address_buffer_offsets;
                p_binding->byte_address_buffer_offsets =
                    (uint32_t *)calloc(byte_address_buffer_offset_count + p_binding->byte_address_buffer_offset_count, sizeof(uint32_t));
                memcpy(p_binding->byte_address_buffer_offsets, prev_byte_address_buffer_offsets,
                    sizeof(uint32_t) * p_binding->byte_address_buffer_offset_count);
                SafeFree(prev_byte_address_buffer_offsets);
            } else {
                // possible not all allocated offset slots are used, but this will be a max per binding
                p_binding->byte_address_buffer_offsets = (uint32_t *)calloc(byte_address_buffer_offset_count, sizeof(uint32_t));
            }

            if (IsNull(p_binding->byte_address_buffer_offsets)) {
                SafeFree(p_used_accesses);
                return SPV_REFLECT_RESULT_ERROR_ALLOC_FAILED;
            }

            for (uint32_t j = 0; j < used_acessed_count; j++) {
                if (p_used_accesses[j].variable_ptr == p_binding->spirv_id) {
                    result = ParseByteAddressBuffer(p_parser, p_used_accesses[j].p_node, p_binding);
                    if (result != SPV_REFLECT_RESULT_SUCCESS) {
                        SafeFree(p_used_accesses);
                        return result;
                    }
                }
            }

            if (multi_entrypoint) {
                qsort(p_binding->byte_address_buffer_offsets, p_binding->byte_address_buffer_offset_count,
                    sizeof(*(p_binding->byte_address_buffer_offsets)), SortCompareUint32);
                p_binding->byte_address_buffer_offset_count =
                    (uint32_t)DedupSortedUint32(p_binding->byte_address_buffer_offsets, p_binding->byte_address_buffer_offset_count);
            }
        }
    }

    SafeFree(p_used_accesses);

    p_entry->used_uniform_count = (uint32_t)used_uniform_count;
    p_entry->used_push_constant_count = (uint32_t)used_push_constant_count;

    return SPV_REFLECT_RESULT_SUCCESS;
}

static SpvReflectResult ParseEntryPoints(SpvReflectPrvParser *p_parser, SpvReflectShaderModule *p_module) {
    if (p_parser->entry_point_count == 0) {
        return SPV_REFLECT_RESULT_SUCCESS;
    }

    p_module->entry_point_count = p_parser->entry_point_count;
    p_module->entry_points = (SpvReflectEntryPoint *)calloc(p_module->entry_point_count, sizeof(*(p_module->entry_points)));
    if (IsNull(p_module->entry_points)) {
        return SPV_REFLECT_RESULT_ERROR_ALLOC_FAILED;
    }

    SpvReflectResult result;
    size_t uniform_count = 0;
    uint32_t *uniforms = NULL;
    if ((result = EnumerateAllUniforms(p_module, &uniform_count, &uniforms)) != SPV_REFLECT_RESULT_SUCCESS) {
        return result;
    }
    size_t push_constant_count = 0;
    uint32_t *push_constants = NULL;
    if ((result = EnumerateAllPushConstants(p_module, &push_constant_count, &push_constants)) != SPV_REFLECT_RESULT_SUCCESS) {
        return result;
    }

    size_t entry_point_index = 0;
    for (size_t i = 0; entry_point_index < p_parser->entry_point_count && i < p_parser->node_count; ++i) {
        SpvReflectPrvNode *p_node = &(p_parser->nodes[i]);
        if (p_node->op != SpvOpEntryPoint) {
            continue;
        }

        SpvReflectEntryPoint *p_entry_point = &(p_module->entry_points[entry_point_index]);
        CHECKED_READU32_CAST(p_parser, p_node->word_offset + 1, SpvExecutionModel, p_entry_point->spirv_execution_model);
        CHECKED_READU32(p_parser, p_node->word_offset + 2, p_entry_point->id);

        switch (p_entry_point->spirv_execution_model) {
        default:
            break;
        case SpvExecutionModelVertex:
            p_entry_point->shader_stage = SPV_REFLECT_SHADER_STAGE_VERTEX_BIT;
            break;
        case SpvExecutionModelTessellationControl:
            p_entry_point->shader_stage = SPV_REFLECT_SHADER_STAGE_TESSELLATION_CONTROL_BIT;
            break;
        case SpvExecutionModelTessellationEvaluation:
            p_entry_point->shader_stage = SPV_REFLECT_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;
            break;
        case SpvExecutionModelGeometry:
            p_entry_point->shader_stage = SPV_REFLECT_SHADER_STAGE_GEOMETRY_BIT;
            break;
        case SpvExecutionModelFragment:
            p_entry_point->shader_stage = SPV_REFLECT_SHADER_STAGE_FRAGMENT_BIT;
            break;
        case SpvExecutionModelGLCompute:
            p_entry_point->shader_stage = SPV_REFLECT_SHADER_STAGE_COMPUTE_BIT;
            break;
        case SpvExecutionModelTaskNV:
            p_entry_point->shader_stage = SPV_REFLECT_SHADER_STAGE_TASK_BIT_NV;
            break;
        case SpvExecutionModelTaskEXT:
            p_entry_point->shader_stage = SPV_REFLECT_SHADER_STAGE_TASK_BIT_EXT;
            break;
        case SpvExecutionModelMeshNV:
            p_entry_point->shader_stage = SPV_REFLECT_SHADER_STAGE_MESH_BIT_NV;
            break;
        case SpvExecutionModelMeshEXT:
            p_entry_point->shader_stage = SPV_REFLECT_SHADER_STAGE_MESH_BIT_EXT;
            break;
        case SpvExecutionModelRayGenerationKHR:
            p_entry_point->shader_stage = SPV_REFLECT_SHADER_STAGE_RAYGEN_BIT_KHR;
            break;
        case SpvExecutionModelIntersectionKHR:
            p_entry_point->shader_stage = SPV_REFLECT_SHADER_STAGE_INTERSECTION_BIT_KHR;
            break;
        case SpvExecutionModelAnyHitKHR:
            p_entry_point->shader_stage = SPV_REFLECT_SHADER_STAGE_ANY_HIT_BIT_KHR;
            break;
        case SpvExecutionModelClosestHitKHR:
            p_entry_point->shader_stage = SPV_REFLECT_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
            break;
        case SpvExecutionModelMissKHR:
            p_entry_point->shader_stage = SPV_REFLECT_SHADER_STAGE_MISS_BIT_KHR;
            break;
        case SpvExecutionModelCallableKHR:
            p_entry_point->shader_stage = SPV_REFLECT_SHADER_STAGE_CALLABLE_BIT_KHR;
            break;
        }

        ++entry_point_index;

        // Name length is required to calculate next operand
        uint32_t name_start_word_offset = 3;
        uint32_t name_length_with_terminator = 0;
        result =
            ReadStr(p_parser, p_node->word_offset + name_start_word_offset, 0, p_node->word_count, &name_length_with_terminator, NULL);
        if (result != SPV_REFLECT_RESULT_SUCCESS) {
            return result;
        }
        p_entry_point->name = (const char *)(p_parser->spirv_code + p_node->word_offset + name_start_word_offset);

        uint32_t name_word_count = RoundUp(name_length_with_terminator, SPIRV_WORD_SIZE) / SPIRV_WORD_SIZE;
        uint32_t interface_variable_count = (p_node->word_count - (name_start_word_offset + name_word_count));
        uint32_t *p_interface_variables = NULL;
        if (interface_variable_count > 0) {
            p_interface_variables = (uint32_t *)calloc(interface_variable_count, sizeof(*(p_interface_variables)));
            if (IsNull(p_interface_variables)) {
                return SPV_REFLECT_RESULT_ERROR_ALLOC_FAILED;
            }
        }

        for (uint32_t var_index = 0; var_index < interface_variable_count; ++var_index) {
            uint32_t var_result_id = (uint32_t)INVALID_VALUE;
            uint32_t offset = name_start_word_offset + name_word_count + var_index;
            CHECKED_READU32(p_parser, p_node->word_offset + offset, var_result_id);
            p_interface_variables[var_index] = var_result_id;
        }

        result = ParseInterfaceVariables(p_parser, p_module, p_entry_point, interface_variable_count, p_interface_variables);
        if (result != SPV_REFLECT_RESULT_SUCCESS) {
            return result;
        }
        SafeFree(p_interface_variables);

        result = ParseStaticallyUsedResources(p_parser, p_module, p_entry_point, uniform_count, uniforms, push_constant_count,
            push_constants);
        if (result != SPV_REFLECT_RESULT_SUCCESS) {
            return result;
        }
    }

    SafeFree(uniforms);
    SafeFree(push_constants);

    return SPV_REFLECT_RESULT_SUCCESS;
}

static SpvReflectResult ParseExecutionModes(SpvReflectPrvParser *p_parser, SpvReflectShaderModule *p_module) {
    assert(IsNotNull(p_parser));
    assert(IsNotNull(p_parser->nodes));
    assert(IsNotNull(p_module));

    if (IsNotNull(p_parser) && IsNotNull(p_parser->spirv_code) && IsNotNull(p_parser->nodes)) {
        for (size_t node_idx = 0; node_idx < p_parser->node_count; ++node_idx) {
            SpvReflectPrvNode *p_node = &(p_parser->nodes[node_idx]);
            if (p_node->op != SpvOpExecutionMode && p_node->op != SpvOpExecutionModeId) {
                continue;
            }

            // Read entry point id
            uint32_t entry_point_id = 0;
            CHECKED_READU32(p_parser, p_node->word_offset + 1, entry_point_id);

            // Find entry point
            SpvReflectEntryPoint *p_entry_point = NULL;
            for (size_t entry_point_idx = 0; entry_point_idx < p_module->entry_point_count; ++entry_point_idx) {
                if (p_module->entry_points[entry_point_idx].id == entry_point_id) {
                    p_entry_point = &p_module->entry_points[entry_point_idx];
                    break;
                }
            }
            // Bail if entry point is null
            if (IsNull(p_entry_point)) {
                return SPV_REFLECT_RESULT_ERROR_SPIRV_INVALID_ENTRY_POINT;
            }

            // Read execution mode
            uint32_t execution_mode = (uint32_t)INVALID_VALUE;
            CHECKED_READU32(p_parser, p_node->word_offset + 2, execution_mode);

            // Parse execution mode
            switch (execution_mode) {
            case SpvExecutionModeInvocations: {
                CHECKED_READU32(p_parser, p_node->word_offset + 3, p_entry_point->invocations);
            } break;

            case SpvExecutionModeLocalSize: {
                CHECKED_READU32(p_parser, p_node->word_offset + 3, p_entry_point->local_size.x);
                CHECKED_READU32(p_parser, p_node->word_offset + 4, p_entry_point->local_size.y);
                CHECKED_READU32(p_parser, p_node->word_offset + 5, p_entry_point->local_size.z);
            } break;
            case SpvExecutionModeLocalSizeId: {
                uint32_t local_size_x_id = 0;
                uint32_t local_size_y_id = 0;
                uint32_t local_size_z_id = 0;
                CHECKED_READU32(p_parser, p_node->word_offset + 3, local_size_x_id);
                CHECKED_READU32(p_parser, p_node->word_offset + 4, local_size_y_id);
                CHECKED_READU32(p_parser, p_node->word_offset + 5, local_size_z_id);

                SpvReflectPrvNode *x_node = FindNode(p_parser, local_size_x_id);
                SpvReflectPrvNode *y_node = FindNode(p_parser, local_size_y_id);
                SpvReflectPrvNode *z_node = FindNode(p_parser, local_size_z_id);
                if (IsNotNull(x_node) && IsNotNull(y_node) && IsNotNull(z_node)) {
                    if (IsSpecConstant(x_node)) {
                        p_entry_point->local_size.x = (uint32_t)SPV_REFLECT_EXECUTION_MODE_SPEC_CONSTANT;
                    } else {
                        CHECKED_READU32(p_parser, x_node->word_offset + 3, p_entry_point->local_size.x);
                    }

                    if (IsSpecConstant(y_node)) {
                        p_entry_point->local_size.y = (uint32_t)SPV_REFLECT_EXECUTION_MODE_SPEC_CONSTANT;
                    } else {
                        CHECKED_READU32(p_parser, y_node->word_offset + 3, p_entry_point->local_size.y);
                    }

                    if (IsSpecConstant(z_node)) {
                        p_entry_point->local_size.z = (uint32_t)SPV_REFLECT_EXECUTION_MODE_SPEC_CONSTANT;
                    } else {
                        CHECKED_READU32(p_parser, z_node->word_offset + 3, p_entry_point->local_size.z);
                    }
                }
            } break;

            case SpvExecutionModeOutputVertices: {
                CHECKED_READU32(p_parser, p_node->word_offset + 3, p_entry_point->output_vertices);
            } break;

            default:
                break;
            }
            p_entry_point->execution_mode_count++;
        }
        uint32_t *indices = (uint32_t *)calloc(p_module->entry_point_count, sizeof(indices));
        if (IsNull(indices)) {
            return SPV_REFLECT_RESULT_ERROR_ALLOC_FAILED;
        }
        for (size_t entry_point_idx = 0; entry_point_idx < p_module->entry_point_count; ++entry_point_idx) {
            SpvReflectEntryPoint *p_entry_point = &p_module->entry_points[entry_point_idx];
            if (p_entry_point->execution_mode_count > 0) {
                p_entry_point->execution_modes =
                    (SpvExecutionMode *)calloc(p_entry_point->execution_mode_count, sizeof(*p_entry_point->execution_modes));
                if (IsNull(p_entry_point->execution_modes)) {
                    SafeFree(indices);
                    return SPV_REFLECT_RESULT_ERROR_ALLOC_FAILED;
                }
            }
        }

        for (size_t node_idx = 0; node_idx < p_parser->node_count; ++node_idx) {
            SpvReflectPrvNode *p_node = &(p_parser->nodes[node_idx]);
            if (p_node->op != SpvOpExecutionMode) {
                continue;
            }

            // Read entry point id
            uint32_t entry_point_id = 0;
            CHECKED_READU32(p_parser, p_node->word_offset + 1, entry_point_id);

            // Find entry point
            SpvReflectEntryPoint *p_entry_point = NULL;
            uint32_t *idx = NULL;
            for (size_t entry_point_idx = 0; entry_point_idx < p_module->entry_point_count; ++entry_point_idx) {
                if (p_module->entry_points[entry_point_idx].id == entry_point_id) {
                    p_entry_point = &p_module->entry_points[entry_point_idx];
                    idx = &indices[entry_point_idx];
                    break;
                }
            }

            // Read execution mode
            uint32_t execution_mode = (uint32_t)INVALID_VALUE;
            CHECKED_READU32(p_parser, p_node->word_offset + 2, execution_mode);
            p_entry_point->execution_modes[(*idx)++] = (SpvExecutionMode)execution_mode;
        }
        SafeFree(indices);
    }
    return SPV_REFLECT_RESULT_SUCCESS;
}

static SpvReflectResult ParsePushConstantBlocks(SpvReflectPrvParser *p_parser, SpvReflectShaderModule *p_module) {
    for (size_t i = 0; i < p_parser->node_count; ++i) {
        SpvReflectPrvNode *p_node = &(p_parser->nodes[i]);
        if ((p_node->op != SpvOpVariable) || (p_node->storage_class != SpvStorageClassPushConstant)) {
            continue;
        }

        p_module->push_constant_block_count += 1;
    }

    if (p_module->push_constant_block_count == 0) {
        return SPV_REFLECT_RESULT_SUCCESS;
    }

    p_module->push_constant_blocks =
        (SpvReflectBlockVariable *)calloc(p_module->push_constant_block_count, sizeof(*p_module->push_constant_blocks));
    if (IsNull(p_module->push_constant_blocks)) {
        return SPV_REFLECT_RESULT_ERROR_ALLOC_FAILED;
    }

    p_parser->physical_pointer_struct_count = 0;
    uint32_t push_constant_index = 0;
    for (size_t i = 0; i < p_parser->node_count; ++i) {
        SpvReflectPrvNode *p_node = &(p_parser->nodes[i]);
        if ((p_node->op != SpvOpVariable) || (p_node->storage_class != SpvStorageClassPushConstant)) {
            continue;
        }

        SpvReflectTypeDescription *p_type = FindType(p_module, p_node->type_id);
        if (IsNull(p_node) || IsNull(p_type)) {
            return SPV_REFLECT_RESULT_ERROR_SPIRV_INVALID_ID_REFERENCE;
        }
        // If the type is a pointer, resolve it
        if (p_type->op == SpvOpTypePointer) {
            // Find the type's node
            SpvReflectPrvNode *p_type_node = FindNode(p_parser, p_type->id);
            if (IsNull(p_type_node)) {
                return SPV_REFLECT_RESULT_ERROR_SPIRV_INVALID_ID_REFERENCE;
            }
            // Should be the resolved type
            p_type = FindType(p_module, p_type_node->type_id);
            if (IsNull(p_type)) {
                return SPV_REFLECT_RESULT_ERROR_SPIRV_INVALID_ID_REFERENCE;
            }
        }

        SpvReflectPrvNode *p_type_node = FindNode(p_parser, p_type->id);
        if (IsNull(p_type_node)) {
            return SPV_REFLECT_RESULT_ERROR_SPIRV_INVALID_ID_REFERENCE;
        }

        SpvReflectBlockVariable *p_push_constant = &p_module->push_constant_blocks[push_constant_index];
        p_push_constant->spirv_id = p_node->result_id;
        p_parser->physical_pointer_count = 0;
        SpvReflectResult result = ParseDescriptorBlockVariable(p_parser, p_module, p_type, p_push_constant);
        if (result != SPV_REFLECT_RESULT_SUCCESS) {
            return result;
        }

        for (uint32_t access_chain_index = 0; access_chain_index < p_parser->access_chain_count; ++access_chain_index) {
            SpvReflectPrvAccessChain *p_access_chain = &(p_parser->access_chains[access_chain_index]);
            // Skip any access chains that aren't touching this push constant block
            if (p_push_constant->spirv_id != FindAccessChainBaseVariable(p_parser, p_access_chain)) {
                continue;
            }
            SpvReflectBlockVariable *p_var =
                (p_access_chain->base_id == p_push_constant->spirv_id) ? p_push_constant : GetRefBlkVar(p_parser, p_access_chain);
            result = ParseDescriptorBlockVariableUsage(p_parser, p_module, p_access_chain, 0, (SpvOp)INVALID_VALUE, p_var);
            if (result != SPV_REFLECT_RESULT_SUCCESS) {
                return result;
            }
        }

        p_push_constant->name = p_node->name;
        result = ParseDescriptorBlockVariableSizes(p_parser, p_module, true, false, true, p_push_constant);
        if (result != SPV_REFLECT_RESULT_SUCCESS) {
            return result;
        }

        // Get minimum offset for whole Push Constant block
        // It is not valid SPIR-V to have an empty Push Constant Block
        p_push_constant->offset = UINT32_MAX;
        for (uint32_t k = 0; k < p_push_constant->member_count; ++k) {
            const uint32_t member_offset = p_push_constant->members[k].offset;
            p_push_constant->offset = Min(p_push_constant->offset, member_offset);
        }

        ++push_constant_index;
    }

    return SPV_REFLECT_RESULT_SUCCESS;
}

static int SortCompareDescriptorSet(const void *a, const void *b) {
    const SpvReflectDescriptorSet *p_elem_a = (const SpvReflectDescriptorSet *)a;
    const SpvReflectDescriptorSet *p_elem_b = (const SpvReflectDescriptorSet *)b;
    int value = (int)(p_elem_a->set) - (int)(p_elem_b->set);
    // We should never see duplicate descriptor set numbers in a shader; if so, a tiebreaker
    // would be needed here.
    assert(value != 0);
    return value;
}

static SpvReflectResult ParseEntrypointDescriptorSets(SpvReflectShaderModule *p_module) {
    // Update the entry point's sets
    for (uint32_t i = 0; i < p_module->entry_point_count; ++i) {
        SpvReflectEntryPoint *p_entry = &p_module->entry_points[i];
        for (uint32_t j = 0; j < p_entry->descriptor_set_count; ++j) {
            SafeFree(p_entry->descriptor_sets[j].bindings);
        }
        SafeFree(p_entry->descriptor_sets);
        p_entry->descriptor_set_count = 0;
        for (uint32_t j = 0; j < p_module->descriptor_set_count; ++j) {
            const SpvReflectDescriptorSet *p_set = &p_module->descriptor_sets[j];
            for (uint32_t k = 0; k < p_set->binding_count; ++k) {
                bool found = SearchSortedUint32(p_entry->used_uniforms, p_entry->used_uniform_count, p_set->bindings[k]->spirv_id);
                if (found) {
                    ++p_entry->descriptor_set_count;
                    break;
                }
            }
        }

        p_entry->descriptor_sets = NULL;
        if (p_entry->descriptor_set_count > 0) {
            p_entry->descriptor_sets = (SpvReflectDescriptorSet *)calloc(p_entry->descriptor_set_count, sizeof(*p_entry->descriptor_sets));
            if (IsNull(p_entry->descriptor_sets)) {
                return SPV_REFLECT_RESULT_ERROR_ALLOC_FAILED;
            }
        }
        p_entry->descriptor_set_count = 0;
        for (uint32_t j = 0; j < p_module->descriptor_set_count; ++j) {
            const SpvReflectDescriptorSet *p_set = &p_module->descriptor_sets[j];
            uint32_t count = 0;
            for (uint32_t k = 0; k < p_set->binding_count; ++k) {
                bool found = SearchSortedUint32(p_entry->used_uniforms, p_entry->used_uniform_count, p_set->bindings[k]->spirv_id);
                if (found) {
                    ++count;
                }
            }
            if (count == 0) {
                continue;
            }
            SpvReflectDescriptorSet *p_entry_set = &p_entry->descriptor_sets[p_entry->descriptor_set_count++];
            p_entry_set->set = p_set->set;
            p_entry_set->bindings = (SpvReflectDescriptorBinding **)calloc(count, sizeof(*p_entry_set->bindings));
            if (IsNull(p_entry_set->bindings)) {
                return SPV_REFLECT_RESULT_ERROR_ALLOC_FAILED;
            }
            for (uint32_t k = 0; k < p_set->binding_count; ++k) {
                bool found = SearchSortedUint32(p_entry->used_uniforms, p_entry->used_uniform_count, p_set->bindings[k]->spirv_id);
                if (found) {
                    p_entry_set->bindings[p_entry_set->binding_count++] = p_set->bindings[k];
                }
            }
        }
    }

    return SPV_REFLECT_RESULT_SUCCESS;
}

static SpvReflectResult ParseDescriptorSets(SpvReflectShaderModule *p_module) {
    // Count the descriptors in each set
    for (uint32_t i = 0; i < p_module->descriptor_binding_count; ++i) {
        SpvReflectDescriptorBinding *p_descriptor = &(p_module->descriptor_bindings[i]);

        // Look for a target set using the descriptor's set number
        SpvReflectDescriptorSet *p_target_set = NULL;
        for (uint32_t j = 0; j < SPV_REFLECT_MAX_DESCRIPTOR_SETS; ++j) {
            SpvReflectDescriptorSet *p_set = &p_module->descriptor_sets[j];
            if (p_set->set == p_descriptor->set) {
                p_target_set = p_set;
                break;
            }
        }

        // If a target set isn't found, find the first available one.
        if (IsNull(p_target_set)) {
            for (uint32_t j = 0; j < SPV_REFLECT_MAX_DESCRIPTOR_SETS; ++j) {
                SpvReflectDescriptorSet *p_set = &p_module->descriptor_sets[j];
                if (p_set->set == (uint32_t)INVALID_VALUE) {
                    p_target_set = p_set;
                    p_target_set->set = p_descriptor->set;
                    break;
                }
            }
        }

        if (IsNull(p_target_set)) {
            return SPV_REFLECT_RESULT_ERROR_INTERNAL_ERROR;
        }

        p_target_set->binding_count += 1;
    }

    // Count the descriptor sets
    for (uint32_t i = 0; i < SPV_REFLECT_MAX_DESCRIPTOR_SETS; ++i) {
        const SpvReflectDescriptorSet *p_set = &p_module->descriptor_sets[i];
        if (p_set->set != (uint32_t)INVALID_VALUE) {
            p_module->descriptor_set_count += 1;
        }
    }

    // Sort the descriptor sets based on numbers
    if (p_module->descriptor_set_count > 0) {
        qsort(p_module->descriptor_sets, p_module->descriptor_set_count, sizeof(*(p_module->descriptor_sets)),
            SortCompareDescriptorSet);
    }

    // Build descriptor pointer array
    for (uint32_t i = 0; i < p_module->descriptor_set_count; ++i) {
        SpvReflectDescriptorSet *p_set = &(p_module->descriptor_sets[i]);
        p_set->bindings = (SpvReflectDescriptorBinding **)calloc(p_set->binding_count, sizeof(*(p_set->bindings)));

        uint32_t descriptor_index = 0;
        for (uint32_t j = 0; j < p_module->descriptor_binding_count; ++j) {
            SpvReflectDescriptorBinding *p_descriptor = &(p_module->descriptor_bindings[j]);
            if (p_descriptor->set == p_set->set) {
                assert(descriptor_index < p_set->binding_count);
                p_set->bindings[descriptor_index] = p_descriptor;
                ++descriptor_index;
            }
        }
    }

    return ParseEntrypointDescriptorSets(p_module);
}

static SpvReflectResult DisambiguateStorageBufferSrvUav(SpvReflectShaderModule *p_module) {
    if (p_module->descriptor_binding_count == 0) {
        return SPV_REFLECT_RESULT_SUCCESS;
    }

    for (uint32_t descriptor_index = 0; descriptor_index < p_module->descriptor_binding_count; ++descriptor_index) {
        SpvReflectDescriptorBinding *p_descriptor = &(p_module->descriptor_bindings[descriptor_index]);
        // Skip everything that isn't a STORAGE_BUFFER descriptor
        if (p_descriptor->descriptor_type != SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_BUFFER) {
            continue;
        }

        //
        // Vulkan doesn't disambiguate between SRVs and UAVs so they
        // come back as STORAGE_BUFFER. The block parsing process will
        // mark a block as non-writable should any member of the block
        // or its descendants are non-writable.
        //
        if (p_descriptor->block.decoration_flags & SPV_REFLECT_DECORATION_NON_WRITABLE) {
            p_descriptor->resource_type = SPV_REFLECT_RESOURCE_FLAG_SRV;
        }
    }

    return SPV_REFLECT_RESULT_SUCCESS;
}

static SpvReflectResult SynchronizeDescriptorSets(SpvReflectShaderModule *p_module) {
    // Free and reset all descriptor set numbers
    for (uint32_t i = 0; i < SPV_REFLECT_MAX_DESCRIPTOR_SETS; ++i) {
        SpvReflectDescriptorSet *p_set = &p_module->descriptor_sets[i];
        SafeFree(p_set->bindings);
        p_set->binding_count = 0;
        p_set->set = (uint32_t)INVALID_VALUE;
    }
    // Set descriptor set count to zero
    p_module->descriptor_set_count = 0;

    SpvReflectResult result = ParseDescriptorSets(p_module);
    return result;
}

static SpvReflectResult CreateShaderModule(uint32_t flags, size_t size, const void *p_code, SpvReflectShaderModule *p_module) {
    // Initialize all module fields to zero
    memset(p_module, 0, sizeof(*p_module));

    // Allocate module internals
#ifdef __cplusplus
    p_module->_internal = (SpvReflectShaderModule::Internal *)calloc(1, sizeof(*(p_module->_internal)));
#else
    p_module->_internal = calloc(1, sizeof(*(p_module->_internal)));
#endif
    if (IsNull(p_module->_internal)) {
        return SPV_REFLECT_RESULT_ERROR_ALLOC_FAILED;
    }
    // Copy flags
    p_module->_internal->module_flags = flags;
    // Figure out if we need to copy the SPIR-V code or not
    if (flags & SPV_REFLECT_MODULE_FLAG_NO_COPY) {
        // Set internal size and pointer to args passed in
        p_module->_internal->spirv_size = size;
#if defined(__cplusplus)
        p_module->_internal->spirv_code = const_cast<uint32_t *>(static_cast<const uint32_t *>(p_code));  // cast that const away
#else
        p_module->_internal->spirv_code = (void *)p_code;  // cast that const away
#endif
        p_module->_internal->spirv_word_count = (uint32_t)(size / SPIRV_WORD_SIZE);
    } else {
        // Allocate SPIR-V code storage
        p_module->_internal->spirv_size = size;
        p_module->_internal->spirv_code = (uint32_t *)calloc(1, p_module->_internal->spirv_size);
        p_module->_internal->spirv_word_count = (uint32_t)(size / SPIRV_WORD_SIZE);
        if (IsNull(p_module->_internal->spirv_code)) {
            SafeFree(p_module->_internal);
            return SPV_REFLECT_RESULT_ERROR_ALLOC_FAILED;
        }
        // Copy SPIR-V to code storage
        memcpy(p_module->_internal->spirv_code, p_code, size);
    }

    // Initialize everything to zero
    SpvReflectPrvParser parser;
    memset(&parser, 0, sizeof(SpvReflectPrvParser));

    // Create parser
    SpvReflectResult result = CreateParser(p_module->_internal->spirv_size, p_module->_internal->spirv_code, &parser);

    // Generator
    {
        const uint32_t *p_ptr = (const uint32_t *)p_module->_internal->spirv_code;
        p_module->generator = (SpvReflectGenerator)((*(p_ptr + 2) & 0xFFFF0000) >> 16);
    }

    if (result == SPV_REFLECT_RESULT_SUCCESS) {
        result = ParseNodes(&parser);
        SPV_REFLECT_ASSERT(result == SPV_REFLECT_RESULT_SUCCESS);
    }
    if (result == SPV_REFLECT_RESULT_SUCCESS) {
        result = ParseStrings(&parser);
        SPV_REFLECT_ASSERT(result == SPV_REFLECT_RESULT_SUCCESS);
    }
    if (result == SPV_REFLECT_RESULT_SUCCESS) {
        result = ParseSource(&parser, p_module);
        SPV_REFLECT_ASSERT(result == SPV_REFLECT_RESULT_SUCCESS);
    }
    if (result == SPV_REFLECT_RESULT_SUCCESS) {
        result = ParseFunctions(&parser);
        SPV_REFLECT_ASSERT(result == SPV_REFLECT_RESULT_SUCCESS);
    }
    if (result == SPV_REFLECT_RESULT_SUCCESS) {
        result = ParseMemberCounts(&parser);
        SPV_REFLECT_ASSERT(result == SPV_REFLECT_RESULT_SUCCESS);
    }
    if (result == SPV_REFLECT_RESULT_SUCCESS) {
        result = ParseNames(&parser);
        SPV_REFLECT_ASSERT(result == SPV_REFLECT_RESULT_SUCCESS);
    }
    if (result == SPV_REFLECT_RESULT_SUCCESS) {
        result = ParseDecorations(&parser, p_module);
        SPV_REFLECT_ASSERT(result == SPV_REFLECT_RESULT_SUCCESS);
    }

    // Start of reflection data parsing
    if (result == SPV_REFLECT_RESULT_SUCCESS) {
        p_module->source_language = parser.source_language;
        p_module->source_language_version = parser.source_language_version;

        // Zero out descriptor set data
        p_module->descriptor_set_count = 0;
        memset(p_module->descriptor_sets, 0, SPV_REFLECT_MAX_DESCRIPTOR_SETS * sizeof(*p_module->descriptor_sets));
        // Initialize descriptor set numbers
        for (uint32_t set_number = 0; set_number < SPV_REFLECT_MAX_DESCRIPTOR_SETS; ++set_number) {
            p_module->descriptor_sets[set_number].set = (uint32_t)INVALID_VALUE;
        }
    }
    if (result == SPV_REFLECT_RESULT_SUCCESS) {
        result = ParseTypes(&parser, p_module);
        SPV_REFLECT_ASSERT(result == SPV_REFLECT_RESULT_SUCCESS);
    }
    if (result == SPV_REFLECT_RESULT_SUCCESS) {
        result = ParseDescriptorBindings(&parser, p_module);
        SPV_REFLECT_ASSERT(result == SPV_REFLECT_RESULT_SUCCESS);
    }
    if (result == SPV_REFLECT_RESULT_SUCCESS) {
        result = ParseDescriptorType(p_module);
        SPV_REFLECT_ASSERT(result == SPV_REFLECT_RESULT_SUCCESS);
    }
    if (result == SPV_REFLECT_RESULT_SUCCESS) {
        result = ParseUAVCounterBindings(p_module);
        SPV_REFLECT_ASSERT(result == SPV_REFLECT_RESULT_SUCCESS);
    }
    if (result == SPV_REFLECT_RESULT_SUCCESS) {
        result = ParseDescriptorBlocks(&parser, p_module);
        SPV_REFLECT_ASSERT(result == SPV_REFLECT_RESULT_SUCCESS);
    }
    if (result == SPV_REFLECT_RESULT_SUCCESS) {
        result = ParsePushConstantBlocks(&parser, p_module);
        SPV_REFLECT_ASSERT(result == SPV_REFLECT_RESULT_SUCCESS);
    }
    if (result == SPV_REFLECT_RESULT_SUCCESS) {
        result = ParseEntryPoints(&parser, p_module);
        SPV_REFLECT_ASSERT(result == SPV_REFLECT_RESULT_SUCCESS);
    }
    if (result == SPV_REFLECT_RESULT_SUCCESS) {
        result = ParseCapabilities(&parser, p_module);
        SPV_REFLECT_ASSERT(result == SPV_REFLECT_RESULT_SUCCESS);
    }
    if (result == SPV_REFLECT_RESULT_SUCCESS && p_module->entry_point_count > 0) {
        SpvReflectEntryPoint *p_entry = &(p_module->entry_points[0]);
        p_module->entry_point_name = p_entry->name;
        p_module->entry_point_id = p_entry->id;
        p_module->spirv_execution_model = p_entry->spirv_execution_model;
        p_module->shader_stage = p_entry->shader_stage;
        p_module->input_variable_count = p_entry->input_variable_count;
        p_module->input_variables = p_entry->input_variables;
        p_module->output_variable_count = p_entry->output_variable_count;
        p_module->output_variables = p_entry->output_variables;
        p_module->interface_variable_count = p_entry->interface_variable_count;
        p_module->interface_variables = p_entry->interface_variables;
    }
    if (result == SPV_REFLECT_RESULT_SUCCESS) {
        result = DisambiguateStorageBufferSrvUav(p_module);
        SPV_REFLECT_ASSERT(result == SPV_REFLECT_RESULT_SUCCESS);
    }
    if (result == SPV_REFLECT_RESULT_SUCCESS) {
        result = SynchronizeDescriptorSets(p_module);
        SPV_REFLECT_ASSERT(result == SPV_REFLECT_RESULT_SUCCESS);
    }
    if (result == SPV_REFLECT_RESULT_SUCCESS) {
        result = ParseExecutionModes(&parser, p_module);
        SPV_REFLECT_ASSERT(result == SPV_REFLECT_RESULT_SUCCESS);
    }

    // Destroy module if parse was not successful
    if (result != SPV_REFLECT_RESULT_SUCCESS) {
        spvReflectDestroyShaderModule(p_module);
    }

    DestroyParser(&parser);

    return result;
}

SpvReflectResult spvReflectCreateShaderModule(size_t size, const void *p_code, SpvReflectShaderModule *p_module) {
    return CreateShaderModule(0, size, p_code, p_module);
}

SpvReflectResult spvReflectCreateShaderModule2(uint32_t flags, size_t size, const void *p_code, SpvReflectShaderModule *p_module) {
    return CreateShaderModule(flags, size, p_code, p_module);
}

SpvReflectResult spvReflectGetShaderModule(size_t size, const void *p_code, SpvReflectShaderModule *p_module) {
    return spvReflectCreateShaderModule(size, p_code, p_module);
}

static void SafeFreeTypes(SpvReflectTypeDescription *p_type) {
    if (IsNull(p_type) || p_type->copied) {
        return;
    }

    if (IsNotNull(p_type->members)) {
        for (size_t i = 0; i < p_type->member_count; ++i) {
            SpvReflectTypeDescription *p_member = &p_type->members[i];
            SafeFreeTypes(p_member);
        }

        SafeFree(p_type->members);
        p_type->members = NULL;
    }
}

static void SafeFreeBlockVariables(SpvReflectBlockVariable *p_block) {
    if (IsNull(p_block)) {
        return;
    }

    // We share pointers to Physical Pointer structs and don't want to double free
    if (p_block->flags & SPV_REFLECT_VARIABLE_FLAGS_PHYSICAL_POINTER_COPY) {
        return;
    }

    if (IsNotNull(p_block->members)) {
        for (size_t i = 0; i < p_block->member_count; ++i) {
            SpvReflectBlockVariable *p_member = &p_block->members[i];
            SafeFreeBlockVariables(p_member);
        }

        SafeFree(p_block->members);
        p_block->members = NULL;
    }
}

static void SafeFreeInterfaceVariable(SpvReflectInterfaceVariable *p_interface) {
    if (IsNull(p_interface)) {
        return;
    }

    if (IsNotNull(p_interface->members)) {
        for (size_t i = 0; i < p_interface->member_count; ++i) {
            SpvReflectInterfaceVariable *p_member = &p_interface->members[i];
            SafeFreeInterfaceVariable(p_member);
        }

        SafeFree(p_interface->members);
        p_interface->members = NULL;
    }
}

void spvReflectDestroyShaderModule(SpvReflectShaderModule *p_module) {
    if (IsNull(p_module->_internal)) {
        return;
    }

    SafeFree(p_module->source_source);

    // Descriptor set bindings
    for (size_t i = 0; i < p_module->descriptor_set_count; ++i) {
        SpvReflectDescriptorSet *p_set = &p_module->descriptor_sets[i];
        free(p_set->bindings);
    }

    // Descriptor binding blocks
    for (size_t i = 0; i < p_module->descriptor_binding_count; ++i) {
        SpvReflectDescriptorBinding *p_descriptor = &p_module->descriptor_bindings[i];
        if (IsNotNull(p_descriptor->byte_address_buffer_offsets)) {
            SafeFree(p_descriptor->byte_address_buffer_offsets);
        }
        SafeFreeBlockVariables(&p_descriptor->block);
    }
    SafeFree(p_module->descriptor_bindings);

    // Entry points
    for (size_t i = 0; i < p_module->entry_point_count; ++i) {
        SpvReflectEntryPoint *p_entry = &p_module->entry_points[i];
        for (size_t j = 0; j < p_entry->interface_variable_count; j++) {
            SafeFreeInterfaceVariable(&p_entry->interface_variables[j]);
        }
        for (uint32_t j = 0; j < p_entry->descriptor_set_count; ++j) {
            SafeFree(p_entry->descriptor_sets[j].bindings);
        }
        SafeFree(p_entry->descriptor_sets);
        SafeFree(p_entry->input_variables);
        SafeFree(p_entry->output_variables);
        SafeFree(p_entry->interface_variables);
        SafeFree(p_entry->used_uniforms);
        SafeFree(p_entry->used_push_constants);
        SafeFree(p_entry->execution_modes);
    }
    SafeFree(p_module->capabilities);
    SafeFree(p_module->entry_points);
    SafeFree(p_module->spec_constants);

    // Push constants
    for (size_t i = 0; i < p_module->push_constant_block_count; ++i) {
        SafeFreeBlockVariables(&p_module->push_constant_blocks[i]);
    }
    SafeFree(p_module->push_constant_blocks);

    // Type infos
    for (size_t i = 0; i < p_module->_internal->type_description_count; ++i) {
        SpvReflectTypeDescription *p_type = &p_module->_internal->type_descriptions[i];
        if (IsNotNull(p_type->members)) {
            SafeFreeTypes(p_type);
        }
        SafeFree(p_type->members);
    }
    SafeFree(p_module->_internal->type_descriptions);

    // Free SPIR-V code if there was a copy
    if ((p_module->_internal->module_flags & SPV_REFLECT_MODULE_FLAG_NO_COPY) == 0) {
        SafeFree(p_module->_internal->spirv_code);
    }
    // Free internal
    SafeFree(p_module->_internal);
}

uint32_t spvReflectGetCodeSize(const SpvReflectShaderModule *p_module) {
    if (IsNull(p_module)) {
        return 0;
    }

    return (uint32_t)(p_module->_internal->spirv_size);
}

const uint32_t *spvReflectGetCode(const SpvReflectShaderModule *p_module) {
    if (IsNull(p_module)) {
        return NULL;
    }

    return p_module->_internal->spirv_code;
}

const SpvReflectEntryPoint *spvReflectGetEntryPoint(const SpvReflectShaderModule *p_module, const char *entry_point) {
    if (IsNull(p_module) || IsNull(entry_point)) {
        return NULL;
    }

    for (uint32_t i = 0; i < p_module->entry_point_count; ++i) {
        if (strcmp(p_module->entry_points[i].name, entry_point) == 0) {
            return &p_module->entry_points[i];
        }
    }
    return NULL;
}

SpvReflectResult spvReflectEnumerateDescriptorBindings(const SpvReflectShaderModule *p_module, uint32_t *p_count,
    SpvReflectDescriptorBinding **pp_bindings) {
    if (IsNull(p_module)) {
        return SPV_REFLECT_RESULT_ERROR_NULL_POINTER;
    }
    if (IsNull(p_count)) {
        return SPV_REFLECT_RESULT_ERROR_NULL_POINTER;
    }

    if (IsNotNull(pp_bindings)) {
        if (*p_count != p_module->descriptor_binding_count) {
            return SPV_REFLECT_RESULT_ERROR_COUNT_MISMATCH;
        }

        for (uint32_t index = 0; index < *p_count; ++index) {
            SpvReflectDescriptorBinding *p_bindings = (SpvReflectDescriptorBinding *)&p_module->descriptor_bindings[index];
            pp_bindings[index] = p_bindings;
        }
    } else {
        *p_count = p_module->descriptor_binding_count;
    }

    return SPV_REFLECT_RESULT_SUCCESS;
}

SpvReflectResult spvReflectEnumerateEntryPointDescriptorBindings(const SpvReflectShaderModule *p_module, const char *entry_point,
    uint32_t *p_count, SpvReflectDescriptorBinding **pp_bindings) {
    if (IsNull(p_module)) {
        return SPV_REFLECT_RESULT_ERROR_NULL_POINTER;
    }
    if (IsNull(p_count)) {
        return SPV_REFLECT_RESULT_ERROR_NULL_POINTER;
    }

    const SpvReflectEntryPoint *p_entry = spvReflectGetEntryPoint(p_module, entry_point);
    if (IsNull(p_entry)) {
        return SPV_REFLECT_RESULT_ERROR_ELEMENT_NOT_FOUND;
    }

    uint32_t count = 0;
    for (uint32_t i = 0; i < p_module->descriptor_binding_count; ++i) {
        bool found = SearchSortedUint32(p_entry->used_uniforms, p_entry->used_uniform_count, p_module->descriptor_bindings[i].spirv_id);
        if (found) {
            if (IsNotNull(pp_bindings)) {
                if (count >= *p_count) {
                    return SPV_REFLECT_RESULT_ERROR_COUNT_MISMATCH;
                }
                pp_bindings[count++] = (SpvReflectDescriptorBinding *)&p_module->descriptor_bindings[i];
            } else {
                ++count;
            }
        }
    }
    if (IsNotNull(pp_bindings)) {
        if (count != *p_count) {
            return SPV_REFLECT_RESULT_ERROR_COUNT_MISMATCH;
        }
    } else {
        *p_count = count;
    }
    return SPV_REFLECT_RESULT_SUCCESS;
}

SpvReflectResult spvReflectEnumerateDescriptorSets(const SpvReflectShaderModule *p_module, uint32_t *p_count,
    SpvReflectDescriptorSet **pp_sets) {
    if (IsNull(p_module)) {
        return SPV_REFLECT_RESULT_ERROR_NULL_POINTER;
    }
    if (IsNull(p_count)) {
        return SPV_REFLECT_RESULT_ERROR_NULL_POINTER;
    }

    if (IsNotNull(pp_sets)) {
        if (*p_count != p_module->descriptor_set_count) {
            return SPV_REFLECT_RESULT_ERROR_COUNT_MISMATCH;
        }

        for (uint32_t index = 0; index < *p_count; ++index) {
            SpvReflectDescriptorSet *p_set = (SpvReflectDescriptorSet *)&p_module->descriptor_sets[index];
            pp_sets[index] = p_set;
        }
    } else {
        *p_count = p_module->descriptor_set_count;
    }

    return SPV_REFLECT_RESULT_SUCCESS;
}

SpvReflectResult spvReflectEnumerateEntryPointDescriptorSets(const SpvReflectShaderModule *p_module, const char *entry_point,
    uint32_t *p_count, SpvReflectDescriptorSet **pp_sets) {
    if (IsNull(p_module)) {
        return SPV_REFLECT_RESULT_ERROR_NULL_POINTER;
    }
    if (IsNull(p_count)) {
        return SPV_REFLECT_RESULT_ERROR_NULL_POINTER;
    }

    const SpvReflectEntryPoint *p_entry = spvReflectGetEntryPoint(p_module, entry_point);
    if (IsNull(p_entry)) {
        return SPV_REFLECT_RESULT_ERROR_ELEMENT_NOT_FOUND;
    }

    if (IsNotNull(pp_sets)) {
        if (*p_count != p_entry->descriptor_set_count) {
            return SPV_REFLECT_RESULT_ERROR_COUNT_MISMATCH;
        }

        for (uint32_t index = 0; index < *p_count; ++index) {
            SpvReflectDescriptorSet *p_set = (SpvReflectDescriptorSet *)&p_entry->descriptor_sets[index];
            pp_sets[index] = p_set;
        }
    } else {
        *p_count = p_entry->descriptor_set_count;
    }

    return SPV_REFLECT_RESULT_SUCCESS;
}

SpvReflectResult spvReflectEnumerateInterfaceVariables(const SpvReflectShaderModule *p_module, uint32_t *p_count,
    SpvReflectInterfaceVariable **pp_variables) {
    if (IsNull(p_module)) {
        return SPV_REFLECT_RESULT_ERROR_NULL_POINTER;
    }
    if (IsNull(p_count)) {
        return SPV_REFLECT_RESULT_ERROR_NULL_POINTER;
    }

    if (IsNotNull(pp_variables)) {
        if (*p_count != p_module->interface_variable_count) {
            return SPV_REFLECT_RESULT_ERROR_COUNT_MISMATCH;
        }

        for (uint32_t index = 0; index < *p_count; ++index) {
            SpvReflectInterfaceVariable *p_var = &p_module->interface_variables[index];
            pp_variables[index] = p_var;
        }
    } else {
        *p_count = p_module->interface_variable_count;
    }

    return SPV_REFLECT_RESULT_SUCCESS;
}

SpvReflectResult spvReflectEnumerateEntryPointInterfaceVariables(const SpvReflectShaderModule *p_module, const char *entry_point,
    uint32_t *p_count, SpvReflectInterfaceVariable **pp_variables) {
    if (IsNull(p_module)) {
        return SPV_REFLECT_RESULT_ERROR_NULL_POINTER;
    }
    if (IsNull(p_count)) {
        return SPV_REFLECT_RESULT_ERROR_NULL_POINTER;
    }

    const SpvReflectEntryPoint *p_entry = spvReflectGetEntryPoint(p_module, entry_point);
    if (IsNull(p_entry)) {
        return SPV_REFLECT_RESULT_ERROR_ELEMENT_NOT_FOUND;
    }

    if (IsNotNull(pp_variables)) {
        if (*p_count != p_entry->interface_variable_count) {
            return SPV_REFLECT_RESULT_ERROR_COUNT_MISMATCH;
        }

        for (uint32_t index = 0; index < *p_count; ++index) {
            SpvReflectInterfaceVariable *p_var = &p_entry->interface_variables[index];
            pp_variables[index] = p_var;
        }
    } else {
        *p_count = p_entry->interface_variable_count;
    }

    return SPV_REFLECT_RESULT_SUCCESS;
}

SpvReflectResult spvReflectEnumerateInputVariables(const SpvReflectShaderModule *p_module, uint32_t *p_count,
    SpvReflectInterfaceVariable **pp_variables) {
    if (IsNull(p_module)) {
        return SPV_REFLECT_RESULT_ERROR_NULL_POINTER;
    }
    if (IsNull(p_count)) {
        return SPV_REFLECT_RESULT_ERROR_NULL_POINTER;
    }

    if (IsNotNull(pp_variables)) {
        if (*p_count != p_module->input_variable_count) {
            return SPV_REFLECT_RESULT_ERROR_COUNT_MISMATCH;
        }

        for (uint32_t index = 0; index < *p_count; ++index) {
            SpvReflectInterfaceVariable *p_var = p_module->input_variables[index];
            pp_variables[index] = p_var;
        }
    } else {
        *p_count = p_module->input_variable_count;
    }

    return SPV_REFLECT_RESULT_SUCCESS;
}

SpvReflectResult spvReflectEnumerateEntryPointInputVariables(const SpvReflectShaderModule *p_module, const char *entry_point,
    uint32_t *p_count, SpvReflectInterfaceVariable **pp_variables) {
    if (IsNull(p_module)) {
        return SPV_REFLECT_RESULT_ERROR_NULL_POINTER;
    }
    if (IsNull(p_count)) {
        return SPV_REFLECT_RESULT_ERROR_NULL_POINTER;
    }

    const SpvReflectEntryPoint *p_entry = spvReflectGetEntryPoint(p_module, entry_point);
    if (IsNull(p_entry)) {
        return SPV_REFLECT_RESULT_ERROR_ELEMENT_NOT_FOUND;
    }

    if (IsNotNull(pp_variables)) {
        if (*p_count != p_entry->input_variable_count) {
            return SPV_REFLECT_RESULT_ERROR_COUNT_MISMATCH;
        }

        for (uint32_t index = 0; index < *p_count; ++index) {
            SpvReflectInterfaceVariable *p_var = p_entry->input_variables[index];
            pp_variables[index] = p_var;
        }
    } else {
        *p_count = p_entry->input_variable_count;
    }

    return SPV_REFLECT_RESULT_SUCCESS;
}

SpvReflectResult spvReflectEnumerateOutputVariables(const SpvReflectShaderModule *p_module, uint32_t *p_count,
    SpvReflectInterfaceVariable **pp_variables) {
    if (IsNull(p_module)) {
        return SPV_REFLECT_RESULT_ERROR_NULL_POINTER;
    }
    if (IsNull(p_count)) {
        return SPV_REFLECT_RESULT_ERROR_NULL_POINTER;
    }

    if (IsNotNull(pp_variables)) {
        if (*p_count != p_module->output_variable_count) {
            return SPV_REFLECT_RESULT_ERROR_COUNT_MISMATCH;
        }

        for (uint32_t index = 0; index < *p_count; ++index) {
            SpvReflectInterfaceVariable *p_var = p_module->output_variables[index];
            pp_variables[index] = p_var;
        }
    } else {
        *p_count = p_module->output_variable_count;
    }

    return SPV_REFLECT_RESULT_SUCCESS;
}

SpvReflectResult spvReflectEnumerateEntryPointOutputVariables(const SpvReflectShaderModule *p_module, const char *entry_point,
    uint32_t *p_count, SpvReflectInterfaceVariable **pp_variables) {
    if (IsNull(p_module)) {
        return SPV_REFLECT_RESULT_ERROR_NULL_POINTER;
    }
    if (IsNull(p_count)) {
        return SPV_REFLECT_RESULT_ERROR_NULL_POINTER;
    }

    const SpvReflectEntryPoint *p_entry = spvReflectGetEntryPoint(p_module, entry_point);
    if (IsNull(p_entry)) {
        return SPV_REFLECT_RESULT_ERROR_ELEMENT_NOT_FOUND;
    }

    if (IsNotNull(pp_variables)) {
        if (*p_count != p_entry->output_variable_count) {
            return SPV_REFLECT_RESULT_ERROR_COUNT_MISMATCH;
        }

        for (uint32_t index = 0; index < *p_count; ++index) {
            SpvReflectInterfaceVariable *p_var = p_entry->output_variables[index];
            pp_variables[index] = p_var;
        }
    } else {
        *p_count = p_entry->output_variable_count;
    }

    return SPV_REFLECT_RESULT_SUCCESS;
}

SpvReflectResult spvReflectEnumeratePushConstantBlocks(const SpvReflectShaderModule *p_module, uint32_t *p_count,
    SpvReflectBlockVariable **pp_blocks) {
    if (IsNull(p_module)) {
        return SPV_REFLECT_RESULT_ERROR_NULL_POINTER;
    }
    if (IsNull(p_count)) {
        return SPV_REFLECT_RESULT_ERROR_NULL_POINTER;
    }

    if (pp_blocks != NULL) {
        if (*p_count != p_module->push_constant_block_count) {
            return SPV_REFLECT_RESULT_ERROR_COUNT_MISMATCH;
        }

        for (uint32_t index = 0; index < *p_count; ++index) {
            SpvReflectBlockVariable *p_push_constant_blocks = (SpvReflectBlockVariable *)&p_module->push_constant_blocks[index];
            pp_blocks[index] = p_push_constant_blocks;
        }
    } else {
        *p_count = p_module->push_constant_block_count;
    }

    return SPV_REFLECT_RESULT_SUCCESS;
}
SpvReflectResult spvReflectEnumeratePushConstants(const SpvReflectShaderModule *p_module, uint32_t *p_count,
    SpvReflectBlockVariable **pp_blocks) {
    return spvReflectEnumeratePushConstantBlocks(p_module, p_count, pp_blocks);
}

SpvReflectResult spvReflectEnumerateEntryPointPushConstantBlocks(const SpvReflectShaderModule *p_module, const char *entry_point,
    uint32_t *p_count, SpvReflectBlockVariable **pp_blocks) {
    if (IsNull(p_module)) {
        return SPV_REFLECT_RESULT_ERROR_NULL_POINTER;
    }
    if (IsNull(p_count)) {
        return SPV_REFLECT_RESULT_ERROR_NULL_POINTER;
    }

    const SpvReflectEntryPoint *p_entry = spvReflectGetEntryPoint(p_module, entry_point);
    if (IsNull(p_entry)) {
        return SPV_REFLECT_RESULT_ERROR_ELEMENT_NOT_FOUND;
    }

    uint32_t count = 0;
    for (uint32_t i = 0; i < p_module->push_constant_block_count; ++i) {
        bool found = SearchSortedUint32(p_entry->used_push_constants, p_entry->used_push_constant_count,
            p_module->push_constant_blocks[i].spirv_id);
        if (found) {
            if (IsNotNull(pp_blocks)) {
                if (count >= *p_count) {
                    return SPV_REFLECT_RESULT_ERROR_COUNT_MISMATCH;
                }
                pp_blocks[count++] = (SpvReflectBlockVariable *)&p_module->push_constant_blocks[i];
            } else {
                ++count;
            }
        }
    }
    if (IsNotNull(pp_blocks)) {
        if (count != *p_count) {
            return SPV_REFLECT_RESULT_ERROR_COUNT_MISMATCH;
        }
    } else {
        *p_count = count;
    }
    return SPV_REFLECT_RESULT_SUCCESS;
}

SpvReflectResult spvReflectEnumerateSpecializationConstants(const SpvReflectShaderModule *p_module, uint32_t *p_count,
    SpvReflectSpecializationConstant **pp_constants) {
    if (IsNull(p_module)) {
        return SPV_REFLECT_RESULT_ERROR_NULL_POINTER;
    }
    if (IsNull(p_count)) {
        return SPV_REFLECT_RESULT_ERROR_NULL_POINTER;
    }

    if (IsNotNull(pp_constants)) {
        if (*p_count != p_module->spec_constant_count) {
            return SPV_REFLECT_RESULT_ERROR_COUNT_MISMATCH;
        }

        for (uint32_t index = 0; index < *p_count; ++index) {
            SpvReflectSpecializationConstant *p_constant = (SpvReflectSpecializationConstant *)&p_module->spec_constants[index];
            pp_constants[index] = p_constant;
        }
    } else {
        *p_count = p_module->spec_constant_count;
    }

    return SPV_REFLECT_RESULT_SUCCESS;
}

const SpvReflectDescriptorBinding *spvReflectGetDescriptorBinding(const SpvReflectShaderModule *p_module, uint32_t binding_number,
    uint32_t set_number, SpvReflectResult *p_result) {
    const SpvReflectDescriptorBinding *p_descriptor = NULL;
    if (IsNotNull(p_module)) {
        for (uint32_t index = 0; index < p_module->descriptor_binding_count; ++index) {
            const SpvReflectDescriptorBinding *p_potential = &p_module->descriptor_bindings[index];
            if ((p_potential->binding == binding_number) && (p_potential->set == set_number)) {
                p_descriptor = p_potential;
                break;
            }
        }
    }
    if (IsNotNull(p_result)) {
        *p_result = IsNotNull(p_descriptor)
            ? SPV_REFLECT_RESULT_SUCCESS
            : (IsNull(p_module) ? SPV_REFLECT_RESULT_ERROR_NULL_POINTER : SPV_REFLECT_RESULT_ERROR_ELEMENT_NOT_FOUND);
    }
    return p_descriptor;
}

const SpvReflectDescriptorBinding *spvReflectGetEntryPointDescriptorBinding(const SpvReflectShaderModule *p_module,
    const char *entry_point, uint32_t binding_number,
    uint32_t set_number, SpvReflectResult *p_result) {
    const SpvReflectEntryPoint *p_entry = spvReflectGetEntryPoint(p_module, entry_point);
    if (IsNull(p_entry)) {
        if (IsNotNull(p_result)) {
            *p_result = SPV_REFLECT_RESULT_ERROR_ELEMENT_NOT_FOUND;
        }
        return NULL;
    }
    const SpvReflectDescriptorBinding *p_descriptor = NULL;
    if (IsNotNull(p_module)) {
        for (uint32_t index = 0; index < p_module->descriptor_binding_count; ++index) {
            const SpvReflectDescriptorBinding *p_potential = &p_module->descriptor_bindings[index];
            bool found = SearchSortedUint32(p_entry->used_uniforms, p_entry->used_uniform_count, p_potential->spirv_id);
            if ((p_potential->binding == binding_number) && (p_potential->set == set_number) && found) {
                p_descriptor = p_potential;
                break;
            }
        }
    }
    if (IsNotNull(p_result)) {
        *p_result = IsNotNull(p_descriptor)
            ? SPV_REFLECT_RESULT_SUCCESS
            : (IsNull(p_module) ? SPV_REFLECT_RESULT_ERROR_NULL_POINTER : SPV_REFLECT_RESULT_ERROR_ELEMENT_NOT_FOUND);
    }
    return p_descriptor;
}

const SpvReflectDescriptorSet *spvReflectGetDescriptorSet(const SpvReflectShaderModule *p_module, uint32_t set_number,
    SpvReflectResult *p_result) {
    const SpvReflectDescriptorSet *p_set = NULL;
    if (IsNotNull(p_module)) {
        for (uint32_t index = 0; index < p_module->descriptor_set_count; ++index) {
            const SpvReflectDescriptorSet *p_potential = &p_module->descriptor_sets[index];
            if (p_potential->set == set_number) {
                p_set = p_potential;
            }
        }
    }
    if (IsNotNull(p_result)) {
        *p_result = IsNotNull(p_set)
            ? SPV_REFLECT_RESULT_SUCCESS
            : (IsNull(p_module) ? SPV_REFLECT_RESULT_ERROR_NULL_POINTER : SPV_REFLECT_RESULT_ERROR_ELEMENT_NOT_FOUND);
    }
    return p_set;
}

const SpvReflectDescriptorSet *spvReflectGetEntryPointDescriptorSet(const SpvReflectShaderModule *p_module, const char *entry_point,
    uint32_t set_number, SpvReflectResult *p_result) {
    const SpvReflectDescriptorSet *p_set = NULL;
    if (IsNotNull(p_module)) {
        const SpvReflectEntryPoint *p_entry = spvReflectGetEntryPoint(p_module, entry_point);
        if (IsNull(p_entry)) {
            if (IsNotNull(p_result)) {
                *p_result = SPV_REFLECT_RESULT_ERROR_ELEMENT_NOT_FOUND;
            }
            return NULL;
        }
        for (uint32_t index = 0; index < p_entry->descriptor_set_count; ++index) {
            const SpvReflectDescriptorSet *p_potential = &p_entry->descriptor_sets[index];
            if (p_potential->set == set_number) {
                p_set = p_potential;
            }
        }
    }
    if (IsNotNull(p_result)) {
        *p_result = IsNotNull(p_set)
            ? SPV_REFLECT_RESULT_SUCCESS
            : (IsNull(p_module) ? SPV_REFLECT_RESULT_ERROR_NULL_POINTER : SPV_REFLECT_RESULT_ERROR_ELEMENT_NOT_FOUND);
    }
    return p_set;
}

const SpvReflectInterfaceVariable *spvReflectGetInputVariableByLocation(const SpvReflectShaderModule *p_module, uint32_t location,
    SpvReflectResult *p_result) {
    if (location == INVALID_VALUE) {
        if (IsNotNull(p_result)) {
            *p_result = SPV_REFLECT_RESULT_ERROR_ELEMENT_NOT_FOUND;
        }
        return NULL;
    }
    const SpvReflectInterfaceVariable *p_var = NULL;
    if (IsNotNull(p_module)) {
        for (uint32_t index = 0; index < p_module->input_variable_count; ++index) {
            const SpvReflectInterfaceVariable *p_potential = p_module->input_variables[index];
            if (p_potential->location == location) {
                p_var = p_potential;
            }
        }
    }
    if (IsNotNull(p_result)) {
        *p_result = IsNotNull(p_var)
            ? SPV_REFLECT_RESULT_SUCCESS
            : (IsNull(p_module) ? SPV_REFLECT_RESULT_ERROR_NULL_POINTER : SPV_REFLECT_RESULT_ERROR_ELEMENT_NOT_FOUND);
    }
    return p_var;
}
const SpvReflectInterfaceVariable *spvReflectGetInputVariable(const SpvReflectShaderModule *p_module, uint32_t location,
    SpvReflectResult *p_result) {
    return spvReflectGetInputVariableByLocation(p_module, location, p_result);
}

const SpvReflectInterfaceVariable *spvReflectGetEntryPointInputVariableByLocation(const SpvReflectShaderModule *p_module,
    const char *entry_point, uint32_t location,
    SpvReflectResult *p_result) {
    if (location == INVALID_VALUE) {
        if (IsNotNull(p_result)) {
            *p_result = SPV_REFLECT_RESULT_ERROR_ELEMENT_NOT_FOUND;
        }
        return NULL;
    }

    const SpvReflectInterfaceVariable *p_var = NULL;
    if (IsNotNull(p_module)) {
        const SpvReflectEntryPoint *p_entry = spvReflectGetEntryPoint(p_module, entry_point);
        if (IsNull(p_entry)) {
            if (IsNotNull(p_result)) {
                *p_result = SPV_REFLECT_RESULT_ERROR_ELEMENT_NOT_FOUND;
            }
            return NULL;
        }
        for (uint32_t index = 0; index < p_entry->input_variable_count; ++index) {
            const SpvReflectInterfaceVariable *p_potential = p_entry->input_variables[index];
            if (p_potential->location == location) {
                p_var = p_potential;
            }
        }
    }
    if (IsNotNull(p_result)) {
        *p_result = IsNotNull(p_var)
            ? SPV_REFLECT_RESULT_SUCCESS
            : (IsNull(p_module) ? SPV_REFLECT_RESULT_ERROR_NULL_POINTER : SPV_REFLECT_RESULT_ERROR_ELEMENT_NOT_FOUND);
    }
    return p_var;
}

const SpvReflectInterfaceVariable *spvReflectGetInputVariableBySemantic(const SpvReflectShaderModule *p_module,
    const char *semantic, SpvReflectResult *p_result) {
    if (IsNull(semantic)) {
        if (IsNotNull(p_result)) {
            *p_result = SPV_REFLECT_RESULT_ERROR_NULL_POINTER;
        }
        return NULL;
    }
    if (semantic[0] == '\0') {
        if (IsNotNull(p_result)) {
            *p_result = SPV_REFLECT_RESULT_ERROR_ELEMENT_NOT_FOUND;
        }
        return NULL;
    }
    const SpvReflectInterfaceVariable *p_var = NULL;
    if (IsNotNull(p_module)) {
        for (uint32_t index = 0; index < p_module->input_variable_count; ++index) {
            const SpvReflectInterfaceVariable *p_potential = p_module->input_variables[index];
            if (p_potential->semantic != NULL && strcmp(p_potential->semantic, semantic) == 0) {
                p_var = p_potential;
            }
        }
    }
    if (IsNotNull(p_result)) {
        *p_result = IsNotNull(p_var)
            ? SPV_REFLECT_RESULT_SUCCESS
            : (IsNull(p_module) ? SPV_REFLECT_RESULT_ERROR_NULL_POINTER : SPV_REFLECT_RESULT_ERROR_ELEMENT_NOT_FOUND);
    }
    return p_var;
}

const SpvReflectInterfaceVariable *spvReflectGetEntryPointInputVariableBySemantic(const SpvReflectShaderModule *p_module,
    const char *entry_point, const char *semantic,
    SpvReflectResult *p_result) {
    if (IsNull(semantic)) {
        if (IsNotNull(p_result)) {
            *p_result = SPV_REFLECT_RESULT_ERROR_NULL_POINTER;
        }
        return NULL;
    }
    if (semantic[0] == '\0') {
        if (IsNotNull(p_result)) {
            *p_result = SPV_REFLECT_RESULT_ERROR_ELEMENT_NOT_FOUND;
        }
        return NULL;
    }
    const SpvReflectInterfaceVariable *p_var = NULL;
    if (IsNotNull(p_module)) {
        const SpvReflectEntryPoint *p_entry = spvReflectGetEntryPoint(p_module, entry_point);
        if (IsNull(p_entry)) {
            if (IsNotNull(p_result)) {
                *p_result = SPV_REFLECT_RESULT_ERROR_ELEMENT_NOT_FOUND;
            }
            return NULL;
        }
        for (uint32_t index = 0; index < p_entry->input_variable_count; ++index) {
            const SpvReflectInterfaceVariable *p_potential = p_entry->input_variables[index];
            if (p_potential->semantic != NULL && strcmp(p_potential->semantic, semantic) == 0) {
                p_var = p_potential;
            }
        }
    }
    if (IsNotNull(p_result)) {
        *p_result = IsNotNull(p_var)
            ? SPV_REFLECT_RESULT_SUCCESS
            : (IsNull(p_module) ? SPV_REFLECT_RESULT_ERROR_NULL_POINTER : SPV_REFLECT_RESULT_ERROR_ELEMENT_NOT_FOUND);
    }
    return p_var;
}

const SpvReflectInterfaceVariable *spvReflectGetOutputVariableByLocation(const SpvReflectShaderModule *p_module, uint32_t location,
    SpvReflectResult *p_result) {
    if (location == INVALID_VALUE) {
        if (IsNotNull(p_result)) {
            *p_result = SPV_REFLECT_RESULT_ERROR_ELEMENT_NOT_FOUND;
        }
        return NULL;
    }
    const SpvReflectInterfaceVariable *p_var = NULL;
    if (IsNotNull(p_module)) {
        for (uint32_t index = 0; index < p_module->output_variable_count; ++index) {
            const SpvReflectInterfaceVariable *p_potential = p_module->output_variables[index];
            if (p_potential->location == location) {
                p_var = p_potential;
            }
        }
    }
    if (IsNotNull(p_result)) {
        *p_result = IsNotNull(p_var)
            ? SPV_REFLECT_RESULT_SUCCESS
            : (IsNull(p_module) ? SPV_REFLECT_RESULT_ERROR_NULL_POINTER : SPV_REFLECT_RESULT_ERROR_ELEMENT_NOT_FOUND);
    }
    return p_var;
}
const SpvReflectInterfaceVariable *spvReflectGetOutputVariable(const SpvReflectShaderModule *p_module, uint32_t location,
    SpvReflectResult *p_result) {
    return spvReflectGetOutputVariableByLocation(p_module, location, p_result);
}

const SpvReflectInterfaceVariable *spvReflectGetEntryPointOutputVariableByLocation(const SpvReflectShaderModule *p_module,
    const char *entry_point, uint32_t location,
    SpvReflectResult *p_result) {
    if (location == INVALID_VALUE) {
        if (IsNotNull(p_result)) {
            *p_result = SPV_REFLECT_RESULT_ERROR_ELEMENT_NOT_FOUND;
        }
        return NULL;
    }

    const SpvReflectInterfaceVariable *p_var = NULL;
    if (IsNotNull(p_module)) {
        const SpvReflectEntryPoint *p_entry = spvReflectGetEntryPoint(p_module, entry_point);
        if (IsNull(p_entry)) {
            if (IsNotNull(p_result)) {
                *p_result = SPV_REFLECT_RESULT_ERROR_ELEMENT_NOT_FOUND;
            }
            return NULL;
        }
        for (uint32_t index = 0; index < p_entry->output_variable_count; ++index) {
            const SpvReflectInterfaceVariable *p_potential = p_entry->output_variables[index];
            if (p_potential->location == location) {
                p_var = p_potential;
            }
        }
    }
    if (IsNotNull(p_result)) {
        *p_result = IsNotNull(p_var)
            ? SPV_REFLECT_RESULT_SUCCESS
            : (IsNull(p_module) ? SPV_REFLECT_RESULT_ERROR_NULL_POINTER : SPV_REFLECT_RESULT_ERROR_ELEMENT_NOT_FOUND);
    }
    return p_var;
}

const SpvReflectInterfaceVariable *spvReflectGetOutputVariableBySemantic(const SpvReflectShaderModule *p_module,
    const char *semantic, SpvReflectResult *p_result) {
    if (IsNull(semantic)) {
        if (IsNotNull(p_result)) {
            *p_result = SPV_REFLECT_RESULT_ERROR_NULL_POINTER;
        }
        return NULL;
    }
    if (semantic[0] == '\0') {
        if (IsNotNull(p_result)) {
            *p_result = SPV_REFLECT_RESULT_ERROR_ELEMENT_NOT_FOUND;
        }
        return NULL;
    }
    const SpvReflectInterfaceVariable *p_var = NULL;
    if (IsNotNull(p_module)) {
        for (uint32_t index = 0; index < p_module->output_variable_count; ++index) {
            const SpvReflectInterfaceVariable *p_potential = p_module->output_variables[index];
            if (p_potential->semantic != NULL && strcmp(p_potential->semantic, semantic) == 0) {
                p_var = p_potential;
            }
        }
    }
    if (IsNotNull(p_result)) {
        *p_result = IsNotNull(p_var)
            ? SPV_REFLECT_RESULT_SUCCESS
            : (IsNull(p_module) ? SPV_REFLECT_RESULT_ERROR_NULL_POINTER : SPV_REFLECT_RESULT_ERROR_ELEMENT_NOT_FOUND);
    }
    return p_var;
}

const SpvReflectInterfaceVariable *spvReflectGetEntryPointOutputVariableBySemantic(const SpvReflectShaderModule *p_module,
    const char *entry_point, const char *semantic,
    SpvReflectResult *p_result) {
    if (IsNull(semantic)) {
        if (IsNotNull(p_result)) {
            *p_result = SPV_REFLECT_RESULT_ERROR_NULL_POINTER;
        }
        return NULL;
    }
    if (semantic[0] == '\0') {
        if (IsNotNull(p_result)) {
            *p_result = SPV_REFLECT_RESULT_ERROR_ELEMENT_NOT_FOUND;
        }
        return NULL;
    }
    const SpvReflectInterfaceVariable *p_var = NULL;
    if (IsNotNull(p_module)) {
        const SpvReflectEntryPoint *p_entry = spvReflectGetEntryPoint(p_module, entry_point);
        if (IsNull(p_entry)) {
            if (IsNotNull(p_result)) {
                *p_result = SPV_REFLECT_RESULT_ERROR_ELEMENT_NOT_FOUND;
            }
            return NULL;
        }
        for (uint32_t index = 0; index < p_entry->output_variable_count; ++index) {
            const SpvReflectInterfaceVariable *p_potential = p_entry->output_variables[index];
            if (p_potential->semantic != NULL && strcmp(p_potential->semantic, semantic) == 0) {
                p_var = p_potential;
            }
        }
    }
    if (IsNotNull(p_result)) {
        *p_result = IsNotNull(p_var)
            ? SPV_REFLECT_RESULT_SUCCESS
            : (IsNull(p_module) ? SPV_REFLECT_RESULT_ERROR_NULL_POINTER : SPV_REFLECT_RESULT_ERROR_ELEMENT_NOT_FOUND);
    }
    return p_var;
}

const SpvReflectBlockVariable *spvReflectGetPushConstantBlock(const SpvReflectShaderModule *p_module, uint32_t index,
    SpvReflectResult *p_result) {
    const SpvReflectBlockVariable *p_push_constant = NULL;
    if (IsNotNull(p_module)) {
        if (index < p_module->push_constant_block_count) {
            p_push_constant = &p_module->push_constant_blocks[index];
        }
    }
    if (IsNotNull(p_result)) {
        *p_result = IsNotNull(p_push_constant)
            ? SPV_REFLECT_RESULT_SUCCESS
            : (IsNull(p_module) ? SPV_REFLECT_RESULT_ERROR_NULL_POINTER : SPV_REFLECT_RESULT_ERROR_ELEMENT_NOT_FOUND);
    }
    return p_push_constant;
}
const SpvReflectBlockVariable *spvReflectGetPushConstant(const SpvReflectShaderModule *p_module, uint32_t index,
    SpvReflectResult *p_result) {
    return spvReflectGetPushConstantBlock(p_module, index, p_result);
}

const SpvReflectBlockVariable *spvReflectGetEntryPointPushConstantBlock(const SpvReflectShaderModule *p_module,
    const char *entry_point, SpvReflectResult *p_result) {
    const SpvReflectBlockVariable *p_push_constant = NULL;
    if (IsNotNull(p_module)) {
        const SpvReflectEntryPoint *p_entry = spvReflectGetEntryPoint(p_module, entry_point);
        if (IsNull(p_entry)) {
            if (IsNotNull(p_result)) {
                *p_result = SPV_REFLECT_RESULT_ERROR_ELEMENT_NOT_FOUND;
            }
            return NULL;
        }
        for (uint32_t i = 0; i < p_module->push_constant_block_count; ++i) {
            bool found = SearchSortedUint32(p_entry->used_push_constants, p_entry->used_push_constant_count,
                p_module->push_constant_blocks[i].spirv_id);
            if (found) {
                p_push_constant = &p_module->push_constant_blocks[i];
                break;
            }
        }
    }
    if (IsNotNull(p_result)) {
        *p_result = IsNotNull(p_push_constant)
            ? SPV_REFLECT_RESULT_SUCCESS
            : (IsNull(p_module) ? SPV_REFLECT_RESULT_ERROR_NULL_POINTER : SPV_REFLECT_RESULT_ERROR_ELEMENT_NOT_FOUND);
    }
    return p_push_constant;
}

SpvReflectResult spvReflectChangeDescriptorBindingNumbers(SpvReflectShaderModule *p_module,
    const SpvReflectDescriptorBinding *p_binding, uint32_t new_binding_number,
    uint32_t new_set_binding) {
    if (IsNull(p_module)) {
        return SPV_REFLECT_RESULT_ERROR_NULL_POINTER;
    }
    if (IsNull(p_binding)) {
        return SPV_REFLECT_RESULT_ERROR_NULL_POINTER;
    }

    SpvReflectDescriptorBinding *p_target_descriptor = NULL;
    for (uint32_t index = 0; index < p_module->descriptor_binding_count; ++index) {
        if (&p_module->descriptor_bindings[index] == p_binding) {
            p_target_descriptor = &p_module->descriptor_bindings[index];
            break;
        }
    }

    if (IsNotNull(p_target_descriptor)) {
        if (p_target_descriptor->word_offset.binding > (p_module->_internal->spirv_word_count - 1)) {
            return SPV_REFLECT_RESULT_ERROR_RANGE_EXCEEDED;
        }
        // Binding number
        if (new_binding_number != (uint32_t)SPV_REFLECT_BINDING_NUMBER_DONT_CHANGE) {
            uint32_t *p_code = p_module->_internal->spirv_code + p_target_descriptor->word_offset.binding;
            *p_code = new_binding_number;
            p_target_descriptor->binding = new_binding_number;
        }
        // Set number
        if (new_set_binding != (uint32_t)SPV_REFLECT_SET_NUMBER_DONT_CHANGE) {
            uint32_t *p_code = p_module->_internal->spirv_code + p_target_descriptor->word_offset.set;
            *p_code = new_set_binding;
            p_target_descriptor->set = new_set_binding;
        }
    }

    SpvReflectResult result = SPV_REFLECT_RESULT_SUCCESS;
    if (new_set_binding != (uint32_t)SPV_REFLECT_SET_NUMBER_DONT_CHANGE) {
        result = SynchronizeDescriptorSets(p_module);
    }
    return result;
}
SpvReflectResult spvReflectChangeDescriptorBindingNumber(SpvReflectShaderModule *p_module,
    const SpvReflectDescriptorBinding *p_descriptor_binding,
    uint32_t new_binding_number, uint32_t optional_new_set_number) {
    return spvReflectChangeDescriptorBindingNumbers(p_module, p_descriptor_binding, new_binding_number, optional_new_set_number);
}

SpvReflectResult spvReflectChangeDescriptorSetNumber(SpvReflectShaderModule *p_module, const SpvReflectDescriptorSet *p_set,
    uint32_t new_set_number) {
    if (IsNull(p_module)) {
        return SPV_REFLECT_RESULT_ERROR_NULL_POINTER;
    }
    if (IsNull(p_set)) {
        return SPV_REFLECT_RESULT_ERROR_NULL_POINTER;
    }
    SpvReflectDescriptorSet *p_target_set = NULL;
    for (uint32_t index = 0; index < SPV_REFLECT_MAX_DESCRIPTOR_SETS; ++index) {
        // The descriptor sets for specific entry points might not be in this set,
        // so just match on set index.
        if (p_module->descriptor_sets[index].set == p_set->set) {
            p_target_set = (SpvReflectDescriptorSet *)p_set;
            break;
        }
    }

    SpvReflectResult result = SPV_REFLECT_RESULT_SUCCESS;
    if (IsNotNull(p_target_set) && new_set_number != (uint32_t)SPV_REFLECT_SET_NUMBER_DONT_CHANGE) {
        for (uint32_t index = 0; index < p_target_set->binding_count; ++index) {
            SpvReflectDescriptorBinding *p_descriptor = p_target_set->bindings[index];
            if (p_descriptor->word_offset.set > (p_module->_internal->spirv_word_count - 1)) {
                return SPV_REFLECT_RESULT_ERROR_RANGE_EXCEEDED;
            }

            uint32_t *p_code = p_module->_internal->spirv_code + p_descriptor->word_offset.set;
            *p_code = new_set_number;
            p_descriptor->set = new_set_number;
        }

        result = SynchronizeDescriptorSets(p_module);
    }

    return result;
}

static SpvReflectResult ChangeVariableLocation(SpvReflectShaderModule *p_module, SpvReflectInterfaceVariable *p_variable,
    uint32_t new_location) {
    if (p_variable->word_offset.location > (p_module->_internal->spirv_word_count - 1)) {
        return SPV_REFLECT_RESULT_ERROR_RANGE_EXCEEDED;
    }
    uint32_t *p_code = p_module->_internal->spirv_code + p_variable->word_offset.location;
    *p_code = new_location;
    p_variable->location = new_location;
    return SPV_REFLECT_RESULT_SUCCESS;
}

SpvReflectResult spvReflectChangeInputVariableLocation(SpvReflectShaderModule *p_module,
    const SpvReflectInterfaceVariable *p_input_variable, uint32_t new_location) {
    if (IsNull(p_module)) {
        return SPV_REFLECT_RESULT_ERROR_NULL_POINTER;
    }
    if (IsNull(p_input_variable)) {
        return SPV_REFLECT_RESULT_ERROR_NULL_POINTER;
    }
    for (uint32_t index = 0; index < p_module->input_variable_count; ++index) {
        if (p_module->input_variables[index] == p_input_variable) {
            return ChangeVariableLocation(p_module, p_module->input_variables[index], new_location);
        }
    }
    return SPV_REFLECT_RESULT_ERROR_ELEMENT_NOT_FOUND;
}

SpvReflectResult spvReflectChangeOutputVariableLocation(SpvReflectShaderModule *p_module,
    const SpvReflectInterfaceVariable *p_output_variable,
    uint32_t new_location) {
    if (IsNull(p_module)) {
        return SPV_REFLECT_RESULT_ERROR_NULL_POINTER;
    }
    if (IsNull(p_output_variable)) {
        return SPV_REFLECT_RESULT_ERROR_NULL_POINTER;
    }
    for (uint32_t index = 0; index < p_module->output_variable_count; ++index) {
        if (p_module->output_variables[index] == p_output_variable) {
            return ChangeVariableLocation(p_module, p_module->output_variables[index], new_location);
        }
    }
    return SPV_REFLECT_RESULT_ERROR_ELEMENT_NOT_FOUND;
}

const char *spvReflectSourceLanguage(SpvSourceLanguage source_lang) {
    switch (source_lang) {
    case SpvSourceLanguageESSL:
        return "ESSL";
    case SpvSourceLanguageGLSL:
        return "GLSL";
    case SpvSourceLanguageOpenCL_C:
        return "OpenCL_C";
    case SpvSourceLanguageOpenCL_CPP:
        return "OpenCL_CPP";
    case SpvSourceLanguageHLSL:
        return "HLSL";
    case SpvSourceLanguageCPP_for_OpenCL:
        return "CPP_for_OpenCL";
    case SpvSourceLanguageSYCL:
        return "SYCL";
    case SpvSourceLanguageHERO_C:
        return "Hero C";
    case SpvSourceLanguageNZSL:
        return "NZSL";
    default:
        break;
    }
    // The source language is SpvSourceLanguageUnknown, SpvSourceLanguageMax, or
    // some other value that does not correspond to a knonwn language.
    return "Unknown";
}

const char *spvReflectBlockVariableTypeName(const SpvReflectBlockVariable *p_var) {
    if (p_var == NULL) {
        return NULL;
    }
    return p_var->type_description->type_name;
}

#endif
