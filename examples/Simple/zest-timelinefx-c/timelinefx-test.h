#ifndef TFX_LIBRARY_HEADER
#define TFX_LIBRARY_HEADER

#define tfxENABLE_PROFILING
#define tfxPROFILER_SAMPLES 60
#define TFX_THREAD_SAFE
#define TFX_EXTRA_DEBUGGING
//Currently there's no advantage to using avx so I have some work to do optimising there, probably to do with cache and general memory bandwidth
//#define tfxUSEAVX

/*    Functions come in 3 flavours:
1) INTERNAL where they're only meant for internal use by the library and not for any use outside it. Note that these functions are declared as static.
2) API where they're meant for access within your games that you're developing
3) EDITOR where they can be accessed from outside the library but really they're mainly useful for editing the effects such as in in the TimelineFX Editor.

All functions in the library will be marked this way for clarity and naturally the API functions will all be properly documented.
*/
//Function marker for any functions meant for external/api use
#ifdef __cplusplus
#define tfxAPI extern "C"
#define tfxAPI_EDITOR extern "C"
#else
#define tfxAPI 
#define tfxAPI_EDITOR 
#endif    

//Function marker for any functions meant mainly for use by the TimelineFX editor and are related to editing effects
//For internal functions
#define tfxINTERNAL static    

/*
	Timeline FX C++ library

	This library is for implementing particle effects into your games and applications.

	This library is render agnostic, so you will have to provide your own means to render the particles. You will use ParticleManager::GetParticleBuffer() to get all of the active particles in the particle manager
	and then use the values in Particle struct to draw a correctly scaled and rotated particle.

	Currently tested on Windows and MacOS, Intel and ARM processors.

	Sections in this header file, you can search for the following keywords to jump to that section:

	[Zest_Pocket_Allocator]				A single header library for allocating memory from a large pool.
	[Header_Includes_and_Typedefs]		Just your basic header stuff for setting up typedefs and some #defines
	[OS_Specific_Functions]				OS specific multithreading and file access
	[XXHash_Implementation]				XXHasher for the storage map.
	[SIMD_defines]						Defines for SIMD intrinsics
	[Enums]								All the definitions for enums and bit flags
	[Constants]							Various constant definitions
	[String_Buffers]					Basic string buffers for storing names of things in the library and reading from library files.
	[Containers_and_Memory]				Container structs and lists and defines for memory is allocated (uses Zest Pocket Allocator by default)
	[Multithreading_Work_Queues]		Implementation for work queues for threading
	[Vector_Math]						Vec2/3/4 and Matrix2/3/4 structs including wide vectors for SIMD
	[Simplex_Noise]						Some setup for implementing simplex noise.
	[Profiling]							Very basic profiling for internal use
	[File_IO]							A package manager for reading/writing files such as a tfx library effects file
	[Struct_Types]						All of the structs used for objects in TimelineFX
	[Internal_Functions]				Mainly internal functions called only by the library but also the Editor, these are marked either tfxINTERNAL or tfxAPI_EDITOR
	[API_Functions]						The main functions for use by users of the library
*/

//Override this if you'd prefer a different way to allocate the pools for sub allocation in host memory.
#ifndef tfxALLOCATE_POOL
#define tfxALLOCATE_POOL malloc
#endif

#ifndef tfxMAX_MEMORY_POOLS
#define tfxMAX_MEMORY_POOLS 32
#endif

#ifndef tfxMAX_THREADS
#define tfxMAX_THREADS 64
#endif

//---------------------------------------
/*  Zest_Pocket_Allocator, a Two Level Segregated Fit memory allocator
	This is my own memory allocator from https://github.com/peterigz/zloc
	This is used in TimelineFX to manage memory allocation. A large pool is created and allocated from. New pools are created if it runs out of space
	(and you initialised TimelineFX to do so).
*/
//---------------------------------------
#include <assert.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdio.h>

#define tfx__Min(a, b) (((a) < (b)) ? (a) : (b))
#define tfx__Max(a, b) (((a) > (b)) ? (a) : (b))
#define tfx__Clamp(min, max, v) (v < min) ? min : (v > max) ? max : v

typedef int tfx_index;
typedef unsigned int tfx_sl_bitmap;
typedef unsigned int tfx_uint;
typedef int tfx_bool;
typedef unsigned char tfx_byte;
typedef void *tfx_pool;

#if !defined (TFX_ASSERT)
#define TFX_ASSERT assert
#endif

#define tfx__is_pow2(x) ((x) && !((x) & ((x) - 1)))
#define tfx__glue2(x, y) x ## y
#define tfx__glue(x, y) tfx__glue2(x, y)
#define tfx__static_assert(exp) \
    typedef char tfx__glue(static_assert, __LINE__) [(exp) ? 1 : -1]

#if (defined(_MSC_VER) && defined(_M_X64)) || defined(__x86_64__)
#define tfx__64BIT
typedef size_t tfx_size;
typedef size_t tfx_fl_bitmap;
typedef int tfxLONG;
#define TFX_ONE 1ULL
#else
typedef size_t tfx_size;
typedef size_t tfx_fl_bitmap;
typedef int tfxLONG;
#define TFX_ONE 1U
#endif

#ifndef MEMORY_ALIGNMENT_LOG2
#if defined(tfx__64BIT)
#define MEMORY_ALIGNMENT_LOG2 3        //64 bit
#else
#define MEMORY_ALIGNMENT_LOG2 2        //32 bit
#endif
#endif

#ifndef TFX_ERROR_NAME
#define TFX_ERROR_NAME "Allocator Error"
#endif

#ifndef TFX_ERROR_COLOR
#define TFX_ERROR_COLOR "\033[31m"
#endif

#ifndef TFX_NOTICE_COLOR
#define TFX_NOTICE_COLOR "\033[0m"
#endif

#ifndef TFX_NOTICE_NAME
#define TFX_NOTICE_NAME "TimelineFX Notice"
#endif

#ifdef TFX_OUTPUT_NOTICE_MESSAGES
#define TFX_PRINT_NOTICE(message_f, ...) printf(message_f"\033[0m", __VA_ARGS__)
#else
#define TFX_PRINT_NOTICE(message_f, ...)
#endif

#ifdef TFX_OUTPUT_ERROR_MESSAGES
#define TFX_PRINT_ERROR(message_f, ...) printf(message_f"\033[0m", __VA_ARGS__)
#else
#define TFX_PRINT_ERROR(message_f, ...)
#endif

#define tfx__KILOBYTE(Value) ((Value) * 1024LL)
#define tfx__MEGABYTE(Value) (tfx__KILOBYTE(Value) * 1024LL)
#define tfx__GIGABYTE(Value) (tfx__MEGABYTE(Value) * 1024LL)

#ifndef TFX_MAX_SIZE_INDEX
#if defined(tfx__64BIT)
#define TFX_MAX_SIZE_INDEX 32
#else
#define TFX_MAX_SIZE_INDEX 30
#endif
#endif

tfx__static_assert(TFX_MAX_SIZE_INDEX < 64);

#ifdef __cplusplus
extern "C" {
#endif

#define tfx__MAXIMUM_BLOCK_SIZE (TFX_ONE << TFX_MAX_SIZE_INDEX)

	enum tfx__constants {
		tfx__MEMORY_ALIGNMENT = 1 << MEMORY_ALIGNMENT_LOG2,
		tfx__SECOND_LEVEL_INDEX_LOG2 = 5,
		tfx__FIRST_LEVEL_INDEX_COUNT = TFX_MAX_SIZE_INDEX,
		tfx__SECOND_LEVEL_INDEX_COUNT = 1 << tfx__SECOND_LEVEL_INDEX_LOG2,
		tfx__BLOCK_POINTER_OFFSET = sizeof(void *) + sizeof(tfx_size),
		tfx__MINIMUM_BLOCK_SIZE = 16,
		tfx__BLOCK_SIZE_OVERHEAD = sizeof(tfx_size),
		tfx__POINTER_SIZE = sizeof(void *),
		tfx__SMALLEST_CATEGORY = (1 << (tfx__SECOND_LEVEL_INDEX_LOG2 + MEMORY_ALIGNMENT_LOG2))
	};

	typedef enum tfx__boundary_tag_flags {
		tfx__BLOCK_IS_FREE = 1 << 0,
		tfx__PREV_BLOCK_IS_FREE = 1 << 1,
	} tfx__boundary_tag_flags;

	typedef enum tfx__error_codes {
		tfx__OK,
		tfx__INVALID_FIRST_BLOCK,
		tfx__INVALID_BLOCK_FOUND,
		tfx__PHYSICAL_BLOCK_MISALIGNMENT,
		tfx__INVALID_SEGRATED_LIST,
		tfx__WRONG_BLOCK_SIZE_FOUND_IN_SEGRATED_LIST,
		tfx__SECOND_LEVEL_BITMAPS_NOT_INITIALISED
	} tfx__error_codes;

	/*
		Each block has a header that is used only has a pointer to the previous physical block
		and the size. If the block is free then the prev and next free blocks are also stored.
	*/
	typedef struct tfx_header {
		struct tfx_header *prev_physical_block;
		/*    Note that the size is either 4 or 8 bytes aligned so the boundary tag (2 flags denoting
			whether this or the previous block is free) can be stored in the first 2 least
			significant bits    */
		tfx_size size;
		/*
		User allocation will start here when the block is used. When the block is free prev and next
		are pointers in a linked list of free blocks within the same class size of blocks
		*/
		struct tfx_header *prev_free_block;
		struct tfx_header *next_free_block;
	} tfx_header;

	/*
	A struct for making snapshots of a memory pool to get used/free memory stats
	*/
	typedef struct tfx_pool_stats_t {
		int used_blocks;
		int free_blocks;
		tfx_size free_size;
		tfx_size used_size;
	} tfx_pool_stats_t;

	typedef struct tfx_allocator {
		/*    This is basically a terminator block that free blocks can point to if they're at the end
			of a free list. */
		tfx_header null_block;
		//todo: just make thead safe by default and remove these conditions
#if defined(TFX_THREAD_SAFE)
		/* Multithreading protection*/
		volatile tfxLONG access;
#endif
		tfx_size minimum_allocation_size;
		/*    Here we store all of the free block data. first_level_bitmap is either a 32bit int
		or 64bit depending on whether tfx__64BIT is set. Second_level_bitmaps are an array of 32bit
		ints. segregated_lists is a two level array pointing to free blocks or null_block if the list
		is empty. */
		tfx_fl_bitmap first_level_bitmap;
		tfx_sl_bitmap second_level_bitmaps[tfx__FIRST_LEVEL_INDEX_COUNT];
		tfx_header *segregated_lists[tfx__FIRST_LEVEL_INDEX_COUNT][tfx__SECOND_LEVEL_INDEX_COUNT];
	} tfx_allocator;

#if defined (_MSC_VER) && (_MSC_VER >= 1400) && (defined (_M_IX86) || defined (_M_X64))
	/* Microsoft Visual C++ support on x86/X64 architectures. */

#include <intrin.h>

	static inline int tfx__scan_reverse(tfx_size bitmap) {
		unsigned long index;
#if defined(tfx__64BIT)
		return _BitScanReverse64(&index, bitmap) ? index : -1;
#else
		return _BitScanReverse(&index, bitmap) ? index : -1;
#endif
	}

	static inline int tfx__scan_forward(tfx_size bitmap)
	{
		unsigned long index;
#if defined(tfx__64BIT)
		return _BitScanForward64(&index, bitmap) ? index : -1;
#else
		return _BitScanForward(&index, bitmap) ? index : -1;
#endif
	}

#ifdef _WIN32
#include <Windows.h>
	static inline tfxLONG tfx__compare_and_exchange(volatile tfxLONG *target, tfxLONG value, tfxLONG original) {
		return InterlockedCompareExchange((volatile LONG *)target, value, original);
	}

	static inline tfxLONG tfx__exchange(volatile tfxLONG *target, tfxLONG value) {
		return InterlockedExchange((volatile LONG *)target, value);
	}

	static inline unsigned int tfx__increment(unsigned int volatile *target) {
		return InterlockedIncrement(target);
	}
#endif

#define tfx__strlen strnlen_s
#define tfx__writebarrier _WriteBarrier();
#define tfx__readbarrier _ReadBarrier();
#define tfx__strcpy(dst, size, src) strcpy_s(dst, size, src)
#define tfx__fseek _fseeki64
#define tfx__ftell _ftelli64
#define TFX_ALIGN_AFFIX(v)
#define TFX_PACKED_STRUCT

#elif defined(__GNUC__) && ((__GNUC__ > 4) || (__GNUC__ == 4 && __GNUC_MINOR__ >= 8)) && \
      (defined(__i386__) || defined(__x86_64__)) || defined(__clang__)
	/* GNU C/C++ or Clang support on x86/x64 architectures. */

	static inline int tfx__scan_reverse(tfx_size bitmap)
	{
#if defined(tfx__64BIT)
		return 64 - __builtin_clzll(bitmap) - 1;
#else
		return 32 - __builtin_clz((int)bitmap) - 1;
#endif
	}

	static inline int tfx__scan_forward(tfx_size bitmap)
	{
#if defined(tfx__64BIT)
		return __builtin_ffsll(bitmap) - 1;
#else
		return __builtin_ffs((int)bitmap) - 1;
#endif
	}

	static inline tfxLONG tfx__compare_and_exchange(volatile tfxLONG *target, tfxLONG value, tfxLONG original) {
		return __sync_val_compare_and_swap(target, original, value);
	}

	static inline tfxLONG tfx__exchange(volatile tfxLONG *target, tfxLONG value) {
		return __sync_lock_test_and_set(target, value);
	}

	static inline unsigned int tfx__increment(unsigned int volatile *target) {
		return __sync_add_and_fetch(target, 1);
	}

#define tfx__strlen strnlen
#define tfx__writebarrier __asm__ __volatile__ ("" : : : "memory");
#define tfx__readbarrier __asm__ __volatile__ ("" : : : "memory");
#define tfx__strcpy(left, right, size) strcpy(left, right)
#define tfx__fseek fseeko
#define tfx__ftell ftello
#define TFX_ALIGN_AFFIX(v)            __attribute__((aligned(v)))
#define TFX_PACKED_STRUCT            __attribute__((packed))

#endif

	/*
		Initialise an allocator. Pass a block of memory that you want to use to store the allocator data. This will not create a pool, only the
		necessary data structures to store the allocator.
	*/
	tfx_allocator *tfx_InitialiseAllocator(void *memory);

	/*
		Initialise an allocator and a pool at the same time. The data stucture to store the allocator will be stored at the beginning of the memory
		you pass to the function and the remaining memory will be used as the pool.
	*/
	tfx_allocator *tfx_InitialiseAllocatorWithPool(void *memory, tfx_size size, tfx_allocator **allocator);

	/*
		Add a new memory pool to the allocator. Pools don't have to all be the same size, adding a pool will create the biggest block it can within
		the pool and then add that to the segregated list of free blocks in the allocator. All the pools in the allocator will be naturally linked
		together in the segregated list because all blocks are linked together with a linked list either as physical neighbours or free blocks in
		the segregated list.
	*/
	tfx_pool *tfx_AddPool(tfx_allocator *allocator, tfx_pool *memory, tfx_size size);

	/*
		Get the structure size of an allocator. You can use this to take into account the overhead of the allocator when preparing a new allocator
		with memory pool.
	*/
	tfx_size tfx_AllocatorSize(void);

	/*
		If you initialised an allocator with a pool then you can use this function to get a pointer to the start of the pool. It won't get a pointer
		to any other pool in the allocator. You can just get that when you call tfx_AddPool.
	*/
	tfx_pool *tfx_GetPool(tfx_allocator *allocator);

	/*
		Allocate some memory within a tfx_allocator of the specified size. Minimum size is 16 bytes.
	*/
	void *tfx_Allocate(tfx_allocator *allocator, tfx_size size);

	/*
		Try to reallocate an existing memory block within the allocator. If possible the current block will be merged with the physical neigbouring
		block, otherwise a normal tfx_Allocate will take place and the data copied over to the new allocation.
	*/
	void *tfx_Reallocate(tfx_allocator *allocator, void *ptr, tfx_size size);

	/*
	Allocate some memory within a tfx_allocator of the specified size. Minimum size is 16 bytes.
*/
	void *tfx_AllocateAligned(tfx_allocator *allocator, tfx_size size, tfx_size alignment);

	/*
		Free an allocation from a tfx_allocator. When freeing a block of memory any adjacent free blocks are merged together to keep on top of
		fragmentation as much as possible. A check is also done to confirm that the block being freed is still valid and detect any memory corruption
		due to out of bounds writing of this or potentially other blocks.
	*/
	int tfx_Free(tfx_allocator *allocator, void *allocation);

	/*
		Remove a pool from an allocator. Note that all blocks in the pool must be free and therefore all merged together into one block (this happens
		automatically as all blocks are freed are merged together into bigger blocks.
	*/
	tfx_bool tfx_RemovePool(tfx_allocator *allocator, tfx_pool *pool);

	/*
	When using an allocator for managing remote memory, you need to set the bytes per block that a block storing infomation about the remote
	memory allocation will manage. For example you might set the value to 1MB so if you were to then allocate 4MB of remote memory then 4 blocks
	worth of space would be used to allocate that memory. This means that if it were to be freed and then split down to a smaller size they'd be
	enough blocks worth of space to do this.

	Note that the lower the number the more memory you need to track remote memory blocks but the more granular it will be. It will depend alot
	on the size of allocations you will need
*/
	void tfx_SetMinimumAllocationSize(tfx_allocator *allocator, tfx_size size);

	//--End of user functions

	//Private inline functions, user doesn't need to call these
	static inline void tfx__map(tfx_size size, tfx_index *fli, tfx_index *sli) {
		*fli = tfx__scan_reverse(size);
		if (*fli <= tfx__SECOND_LEVEL_INDEX_LOG2) {
			*fli = 0;
			*sli = (int)size / (tfx__SMALLEST_CATEGORY / tfx__SECOND_LEVEL_INDEX_COUNT);
			return;
		}
		size = size & ~(1 << *fli);
		*sli = (tfx_index)(size >> (*fli - tfx__SECOND_LEVEL_INDEX_LOG2)) % tfx__SECOND_LEVEL_INDEX_COUNT;
	}

	//Read only functions
	static inline tfx_bool tfx__has_free_block(const tfx_allocator *allocator, tfx_index fli, tfx_index sli) {
		return allocator->first_level_bitmap & (TFX_ONE << fli) && allocator->second_level_bitmaps[fli] & (1U << sli);
	}

	static inline tfx_bool tfx__is_used_block(const tfx_header *block) {
		return !(block->size & tfx__BLOCK_IS_FREE);
	}

	static inline tfx_bool tfx__is_free_block(const tfx_header *block) {
		//Crashing here? The most likely reason is a pointer into the allocation for this block that became invalid but was still written to at some point.
		//Most likeyly cause is a tfx_vector_t or similar being resized and allocated elsewhere but you didn't account for this happening and update the pointer. Just index
		//into the array instead to fix these issues.
		//Another reason is simply that you're trying to free something that isn't actually a block of memory in the allocator, maybe you're just trying to free a struct or
		//other object?
		return block->size & tfx__BLOCK_IS_FREE;
	}

	static inline tfx_bool tfx__prev_is_free_block(const tfx_header *block) {
		return block->size & tfx__PREV_BLOCK_IS_FREE;
	}

	static inline void *tfx__align_ptr(const void *ptr, tfx_size align) {
		ptrdiff_t aligned = (((ptrdiff_t)ptr) + (align - 1)) & ~(align - 1);
		TFX_ASSERT(0 == (align & (align - 1)) && "must align to a power of two");
		return (void *)aligned;
	}

	static inline tfx_bool tfx__is_aligned(tfx_size size, tfx_size alignment) {
		return (size % alignment) == 0;
	}

	static inline tfx_bool tfx__ptr_is_aligned(void *ptr, tfx_size alignment) {
		uintptr_t address = (uintptr_t)ptr;
		return (address % alignment) == 0;
	}

	static inline tfx_size tfx__align_size_down(tfx_size size, tfx_size alignment) {
		return size - (size % alignment);
	}

	static inline tfx_size tfx__align_size_up(tfx_size size, tfx_size alignment) {
		tfx_size remainder = size % alignment;
		if (remainder != 0) {
			size += alignment - remainder;
		}
		return size;
	}

	static inline tfx_size tfx__adjust_size(tfx_size size, tfx_size minimum_size, tfx_size alignment) {
		return tfx__Min(tfx__Max(tfx__align_size_up(size, alignment), minimum_size), tfx__MAXIMUM_BLOCK_SIZE);
	}

	static inline tfx_size tfx__block_size(const tfx_header *block) {
		return block->size & ~(tfx__BLOCK_IS_FREE | tfx__PREV_BLOCK_IS_FREE);
	}

	static inline tfx_header *tfx__block_from_allocation(const void *allocation) {
		return (tfx_header *)((char *)allocation - tfx__BLOCK_POINTER_OFFSET);
	}

	static inline tfx_header *tfx__null_block(tfx_allocator *allocator) {
		return &allocator->null_block;
	}

	static inline void *tfx__block_user_ptr(const tfx_header *block) {
		return (char *)block + tfx__BLOCK_POINTER_OFFSET;
	}

	static inline tfx_header *tfx__first_block_in_pool(const tfx_pool *pool) {
		return (tfx_header *)((char *)pool - tfx__POINTER_SIZE);
	}

	static inline tfx_header *tfx__next_physical_block(const tfx_header *block) {
		return (tfx_header *)((char *)tfx__block_user_ptr(block) + tfx__block_size(block));
	}

	static inline tfx_bool tfx__next_block_is_free(const tfx_header *block) {
		return tfx__is_free_block(tfx__next_physical_block(block));
	}

	static inline tfx_header *tfx__allocator_first_block(tfx_allocator *allocator) {
		return (tfx_header *)((char *)allocator + tfx_AllocatorSize() - tfx__POINTER_SIZE);
	}

	static inline tfx_bool tfx__is_last_block_in_pool(const tfx_header *block) {
		return tfx__block_size(block) == 0;
	}

	static inline tfx_index tfx__find_next_size_up(tfx_fl_bitmap map, tfx_uint start) {
		//Mask out all bits up to the start point of the scan
		map &= (~0ULL << (start + 1));
		return tfx__scan_forward(map);
	}

	//Debug tool to make sure that if a first level bitmap has a bit set, then the corresponding second level index should contain a value
	//It also checks that the blocks in the list are valid.
	//The most common cause of asserts here is where memory has been written to the wrong address. Check for buffers where they where resized
	//but the buffer pointer that was being written too was not updated after the resize for example.
	static inline void tfx__verify_lists(tfx_allocator *allocator) {
		for (int fli = 0; fli != tfx__FIRST_LEVEL_INDEX_COUNT; ++fli) {
			if (allocator->first_level_bitmap & (1ULL << fli)) {
				//bit in first level is set but according to the second level bitmap array there are no blocks so the first level
				//bitmap bit should have been 0
				TFX_ASSERT(allocator->second_level_bitmaps[fli] > 0);
				int sli = -1;
				sli = tfx__find_next_size_up(allocator->second_level_bitmaps[fli], sli);
				while (sli != -1) {
					tfx_header *block = allocator->segregated_lists[fli][sli];
					tfx_bool is_free = tfx__is_free_block(block);
					TFX_ASSERT(is_free);    //The block should be marked as free
					TFX_ASSERT(block->prev_free_block == &allocator->null_block);    //The first block in in the list should have a prev_free_block that points to the null block in the allocator
					sli = tfx__find_next_size_up(allocator->second_level_bitmaps[fli], sli);
				}
			}
		}
	}

	//Write functions
#if defined(TFX_THREAD_SAFE)

#define tfx__lock_thread_access(alloc)                                        \
    do {                                                                    \
    } while (0 != tfx__compare_and_exchange(&alloc->access, 1, 0));            \
    TFX_ASSERT(alloc->access != 0);

#define tfx__unlock_thread_access(alloc)  alloc->access = 0;

#else

#define tfx__lock_thread_access
#define tfx__unlock_thread_access 

#endif

	static inline void tfx__set_block_size(tfx_header *block, tfx_size size) {
		tfx_size boundary_tag = block->size & (tfx__BLOCK_IS_FREE | tfx__PREV_BLOCK_IS_FREE);
		block->size = size | boundary_tag;
	}

	static inline void tfx__set_prev_physical_block(tfx_header *block, tfx_header *prev_block) {
		block->prev_physical_block = prev_block;
	}

	static inline void tfx__zero_block(tfx_header *block) {
		block->prev_physical_block = 0;
		block->size = 0;
	}

	static inline void tfx__mark_block_as_used(tfx_header *block) {
		block->size &= ~tfx__BLOCK_IS_FREE;
		tfx_header *next_block = tfx__next_physical_block(block);
		next_block->size &= ~tfx__PREV_BLOCK_IS_FREE;
	}

	static inline void tfx__mark_block_as_free(tfx_header *block) {
		block->size |= tfx__BLOCK_IS_FREE;
		tfx_header *next_block = tfx__next_physical_block(block);
		next_block->size |= tfx__PREV_BLOCK_IS_FREE;
	}

	static inline void tfx__block_set_used(tfx_header *block) {
		block->size &= ~tfx__BLOCK_IS_FREE;
	}

	static inline void tfx__block_set_free(tfx_header *block) {
		block->size |= tfx__BLOCK_IS_FREE;
	}

	static inline void tfx__block_set_prev_used(tfx_header *block) {
		block->size &= ~tfx__PREV_BLOCK_IS_FREE;
	}

	static inline void tfx__block_set_prev_free(tfx_header *block) {
		block->size |= tfx__PREV_BLOCK_IS_FREE;
	}

	/*
		Push a block onto the segregated list of free blocks. Called when tfx_Free is called. Generally blocks are
		merged if possible before this is called
	*/
	static inline void tfx__push_block(tfx_allocator *allocator, tfx_header *block) {
		tfx_index fli;
		tfx_index sli;
		//Get the size class of the block
		tfx__map(tfx__block_size(block), &fli, &sli);
		tfx_header *current_block_in_free_list = allocator->segregated_lists[fli][sli];
		//If you hit this assert then it's likely that at somepoint in your code you're trying to free an allocation
		//that was already freed.
		TFX_ASSERT(block != current_block_in_free_list);
		//Insert the block into the list by updating the next and prev free blocks of
		//this and the current block in the free list. The current block in the free
		//list may well be the null_block in the allocator so this just means that this
		//block will be added as the first block in this class of free blocks.
		block->next_free_block = current_block_in_free_list;
		block->prev_free_block = &allocator->null_block;
		current_block_in_free_list->prev_free_block = block;

		allocator->segregated_lists[fli][sli] = block;
		//Flag the bitmaps to mark that this size class now contains a free block
		allocator->first_level_bitmap |= TFX_ONE << fli;
		allocator->second_level_bitmaps[fli] |= 1U << sli;
		if (allocator->first_level_bitmap & (TFX_ONE << fli)) {
			TFX_ASSERT(allocator->second_level_bitmaps[fli] > 0);
		}
		tfx__mark_block_as_free(block);
#ifdef TFX_EXTRA_DEBUGGING
		tfx__verify_lists(allocator);
#endif
	}

	/*
		Remove a block from the segregated list in the allocator and return it. If there is a next free block in the size class
		then move it down the list, otherwise unflag the bitmaps as necessary. This is only called when we're trying to allocate
		some memory with tfx_Allocate and we've determined that there's a suitable free block in segregated_lists.
	*/
	static inline tfx_header *tfx__pop_block(tfx_allocator *allocator, tfx_index fli, tfx_index sli) {
		tfx_header *block = allocator->segregated_lists[fli][sli];

		//If the block in the segregated list is actually the null_block then something went very wrong.
		//Somehow the segregated lists had the end block assigned but the first or second level bitmaps
		//did not have the masks assigned
		TFX_ASSERT(block != &allocator->null_block);
		if (block->next_free_block && block->next_free_block != &allocator->null_block) {
			//If there are more free blocks in this size class then shift the next one down and terminate the prev_free_block
			allocator->segregated_lists[fli][sli] = block->next_free_block;
			allocator->segregated_lists[fli][sli]->prev_free_block = tfx__null_block(allocator);
		}
		else {
			//There's no more free blocks in this size class so flag the second level bitmap for this class to 0.
			allocator->segregated_lists[fli][sli] = tfx__null_block(allocator);
			allocator->second_level_bitmaps[fli] &= ~(1U << sli);
			if (allocator->second_level_bitmaps[fli] == 0) {
				//And if the second level bitmap is 0 then the corresponding bit in the first lebel can be zero'd too.
				allocator->first_level_bitmap &= ~(TFX_ONE << fli);
			}
		}
		if (allocator->first_level_bitmap & (TFX_ONE << fli)) {
			TFX_ASSERT(allocator->second_level_bitmaps[fli] > 0);
		}
		tfx__mark_block_as_used(block);
#ifdef TFX_EXTRA_DEBUGGING
		tfx__verify_lists(allocator);
#endif
		return block;
	}

	/*
		Remove a block from the segregated list. This is only called when we're merging blocks together. The block is
		just removed from the list and marked as used and then merged with an adjacent block.
	*/
	static inline void tfx__remove_block_from_segregated_list(tfx_allocator *allocator, tfx_header *block) {
		tfx_index fli, sli;
		//Get the size class
		tfx__map(tfx__block_size(block), &fli, &sli);
		tfx_header *prev_block = block->prev_free_block;
		tfx_header *next_block = block->next_free_block;
		TFX_ASSERT(prev_block);
		TFX_ASSERT(next_block);
		next_block->prev_free_block = prev_block;
		prev_block->next_free_block = next_block;
		if (allocator->segregated_lists[fli][sli] == block) {
			allocator->segregated_lists[fli][sli] = next_block;
			if (next_block == tfx__null_block(allocator)) {
				allocator->second_level_bitmaps[fli] &= ~(1U << sli);
				if (allocator->second_level_bitmaps[fli] == 0) {
					allocator->first_level_bitmap &= ~(1ULL << fli);
				}
			}
		}
		if (allocator->first_level_bitmap & (TFX_ONE << fli)) {
			TFX_ASSERT(allocator->second_level_bitmaps[fli] > 0);
		}
		tfx__mark_block_as_used(block);
#ifdef TFX_EXTRA_DEBUGGING
		tfx__verify_lists(allocator);
#endif
	}

	/*
		This function is called when tfx_Allocate is called. Once a free block is found then it will be split
		if the size + header overhead + the minimum block size (16b) is greater then the size of the free block.
		If not then it simply returns the free block as it is without splitting.
		If split then the trimmed amount is added back to the segregated list of free blocks.
	*/
	static inline tfx_header *tfx__maybe_split_block(tfx_allocator *allocator, tfx_header *block, tfx_size size, tfx_size remote_size) {
		TFX_ASSERT(!tfx__is_last_block_in_pool(block));
		tfx_size size_plus_overhead = size + tfx__BLOCK_POINTER_OFFSET;
		if (size_plus_overhead + tfx__MINIMUM_BLOCK_SIZE >= tfx__block_size(block)) {
			return block;
		}
		tfx_header *trimmed = (tfx_header *)((char *)tfx__block_user_ptr(block) + size);
		trimmed->size = 0;
		tfx__set_block_size(trimmed, tfx__block_size(block) - size_plus_overhead);
		tfx_header *next_block = tfx__next_physical_block(block);
		tfx__set_prev_physical_block(next_block, trimmed);
		tfx__set_prev_physical_block(trimmed, block);
		tfx__set_block_size(block, size);
		tfx__push_block(allocator, trimmed);
		return block;
	}

	//For splitting blocks when allocating to a specific memory alignment
	static inline tfx_header *tfx__split_aligned_block(tfx_allocator *allocator, tfx_header *block, tfx_size size) {
		TFX_ASSERT(!tfx__is_last_block_in_pool(block));
		tfx_size size_minus_overhead = size - tfx__BLOCK_POINTER_OFFSET;
		tfx_header *trimmed = (tfx_header *)((char *)tfx__block_user_ptr(block) + size_minus_overhead);
		trimmed->size = 0;
		tfx__set_block_size(trimmed, tfx__block_size(block) - size);
		tfx_header *next_block = tfx__next_physical_block(block);
		tfx__set_prev_physical_block(next_block, trimmed);
		tfx__set_prev_physical_block(trimmed, block);
		tfx__set_block_size(block, size_minus_overhead);
		tfx__push_block(allocator, block);
		return trimmed;
	}

	/*
		This function is called when tfx_Free is called and the previous physical block is free. If that's the case
		then this function will merge the block being freed with the previous physical block then add that back into
		the segregated list of free blocks. Note that that happens in the tfx_Free function after attempting to merge
		both ways.
	*/
	static inline tfx_header *tfx__merge_with_prev_block(tfx_allocator *allocator, tfx_header *block) {
		TFX_ASSERT(!tfx__is_last_block_in_pool(block));
		tfx_header *prev_block = block->prev_physical_block;
		tfx__remove_block_from_segregated_list(allocator, prev_block);
		tfx__set_block_size(prev_block, tfx__block_size(prev_block) + tfx__block_size(block) + tfx__BLOCK_POINTER_OFFSET);
		tfx_header *next_block = tfx__next_physical_block(block);
		tfx__set_prev_physical_block(next_block, prev_block);
		tfx__zero_block(block);
		return prev_block;
	}

	/*
		This function might be called when tfx_Free is called to free a block. If the block being freed is not the last
		physical block then this function is called and if the next block is free then it will be merged.
	*/
	static inline void tfx__merge_with_next_block(tfx_allocator *allocator, tfx_header *block) {
		tfx_header *next_block = tfx__next_physical_block(block);
		TFX_ASSERT(next_block->prev_physical_block == block);    //could be potentional memory corruption. Check that you're not writing outside the boundary of the block size
		TFX_ASSERT(!tfx__is_last_block_in_pool(next_block));
		tfx__remove_block_from_segregated_list(allocator, next_block);
		tfx__set_block_size(block, tfx__block_size(next_block) + tfx__block_size(block) + tfx__BLOCK_POINTER_OFFSET);
		tfx_header *block_after_next = tfx__next_physical_block(next_block);
		tfx__set_prev_physical_block(block_after_next, block);
		tfx__zero_block(next_block);
	}

	static inline tfx_header *tfx__find_free_block(tfx_allocator *allocator, tfx_size size, tfx_size remote_size) {
		tfx_index fli;
		tfx_index sli;
		tfx__map(size, &fli, &sli);
		//Note that there may well be an appropriate size block in the class but that block may not be at the head of the list
		//In this situation we could opt to loop through the list of the size class to see if there is an appropriate size but instead
		//we stick to the paper and just move on to the next class up to keep a O1 speed at the cost of some extra fragmentation
		if (tfx__has_free_block(allocator, fli, sli) && tfx__block_size(allocator->segregated_lists[fli][sli]) >= size) {
			tfx_header *block = tfx__pop_block(allocator, fli, sli);
			tfx__unlock_thread_access(allocator);
			return block;
		}
		if (sli == tfx__SECOND_LEVEL_INDEX_COUNT - 1) {
			sli = -1;
		}
		else {
			sli = tfx__find_next_size_up(allocator->second_level_bitmaps[fli], sli);
		}
		if (sli == -1) {
			fli = tfx__find_next_size_up(allocator->first_level_bitmap, fli);
			if (fli > -1) {
				sli = tfx__scan_forward(allocator->second_level_bitmaps[fli]);
				tfx_header *block = tfx__pop_block(allocator, fli, sli);
				tfx_header *split_block = tfx__maybe_split_block(allocator, block, size, 0);
				tfx__unlock_thread_access(allocator);
				return split_block;
			}
		}
		else {
			tfx_header *block = tfx__pop_block(allocator, fli, sli);
			tfx_header *split_block = tfx__maybe_split_block(allocator, block, size, 0);
			tfx__unlock_thread_access(allocator);
			return split_block;
		}

		return 0;
	}
	//--End of internal functions

	//--End of header declarations

//Implementation
#if defined(TFX_ALLOCATOR_IMPLEMENTATION)

//Definitions
	void *tfx_BlockUserExtensionPtr(const tfx_header *block) {
		return (char *)block + sizeof(tfx_header);
	}

	void *tfx_AllocationFromExtensionPtr(const void *block) {
		return (void *)((char *)block - tfx__MINIMUM_BLOCK_SIZE);
	}

	tfx_allocator *tfx_InitialiseAllocator(void *memory) {
		if (!memory) {
			TFX_PRINT_ERROR(TFX_ERROR_COLOR"%s: The memory pointer passed in to the initialiser was NULL, did it allocate properly?\n", TFX_ERROR_NAME);
			return 0;
		}

		tfx_allocator *allocator = (tfx_allocator *)memory;
		memset(allocator, 0, sizeof(tfx_allocator));
		allocator->null_block.next_free_block = &allocator->null_block;
		allocator->null_block.prev_free_block = &allocator->null_block;
		allocator->minimum_allocation_size = tfx__MINIMUM_BLOCK_SIZE;

		//Point all of the segregated list array pointers to the empty block
		for (tfx_uint i = 0; i < tfx__FIRST_LEVEL_INDEX_COUNT; i++) {
			for (tfx_uint j = 0; j < tfx__SECOND_LEVEL_INDEX_COUNT; j++) {
				allocator->segregated_lists[i][j] = &allocator->null_block;
			}
		}

		return allocator;
	}

	tfx_allocator *tfx_InitialiseAllocatorWithPool(void *memory, tfx_size size, tfx_allocator **allocator) {
		tfx_size array_offset = sizeof(tfx_allocator);
		if (size < array_offset + tfx__MEMORY_ALIGNMENT) {
			TFX_PRINT_ERROR(TFX_ERROR_COLOR"%s: Tried to initialise allocator with a memory allocation that is too small. Must be at least: %zi bytes\n", TFX_ERROR_NAME, array_offset + tfx__MEMORY_ALIGNMENT);
			return 0;
		}

		*allocator = tfx_InitialiseAllocator(memory);
		if (!allocator) {
			return 0;
		}
		tfx_AddPool(*allocator, tfx_GetPool(*allocator), size - tfx_AllocatorSize());
		return *allocator;
	}

	tfx_size tfx_AllocatorSize(void) {
		return sizeof(tfx_allocator);
	}

	void tfx_SetMinimumAllocationSize(tfx_allocator *allocator, tfx_size size) {
		TFX_ASSERT(allocator->minimum_allocation_size == tfx__MINIMUM_BLOCK_SIZE);        //You cannot change this once set
		TFX_ASSERT(tfx__is_pow2(size));                                                    //Size must be a power of 2
		allocator->minimum_allocation_size = tfx__Max(tfx__MINIMUM_BLOCK_SIZE, size);
	}

	tfx_pool *tfx_GetPool(tfx_allocator *allocator) {
		return (tfx_pool *)((char *)allocator + tfx_AllocatorSize());
	}

	tfx_pool *tfx_AddPool(tfx_allocator *allocator, tfx_pool *memory, tfx_size size) {
		tfx__lock_thread_access(allocator);

		//Offset it back by the pointer size, we don't need the prev_physical block pointer as there is none
		//for the first block in the pool
		tfx_header *block = tfx__first_block_in_pool(memory);
		block->size = 0;
		//Leave room for an end block
		tfx__set_block_size(block, size - (tfx__BLOCK_POINTER_OFFSET)-tfx__BLOCK_SIZE_OVERHEAD);

		//Make sure it aligns
		tfx__set_block_size(block, tfx__align_size_down(tfx__block_size(block), tfx__MEMORY_ALIGNMENT));
		TFX_ASSERT(tfx__block_size(block) > tfx__MINIMUM_BLOCK_SIZE);
		tfx__block_set_free(block);
		tfx__block_set_prev_used(block);

		//Add a 0 sized block at the end of the pool to cap it off
		tfx_header *last_block = tfx__next_physical_block(block);
		last_block->size = 0;
		tfx__block_set_used(last_block);

		last_block->prev_physical_block = block;
		tfx__push_block(allocator, block);

		tfx__unlock_thread_access(allocator);
		return memory;
	}

	tfx_bool tfx_RemovePool(tfx_allocator *allocator, tfx_pool *pool) {
		tfx__lock_thread_access(allocator);
		tfx_header *block = tfx__first_block_in_pool(pool);

		if (tfx__is_free_block(block) && !tfx__next_block_is_free(block) && tfx__is_last_block_in_pool(tfx__next_physical_block(block))) {
			tfx__remove_block_from_segregated_list(allocator, block);
			tfx__unlock_thread_access(allocator);
			return 1;
		}
#if defined(TFX_THREAD_SAFE)
		tfx__unlock_thread_access(allocator);
		TFX_PRINT_ERROR(TFX_ERROR_COLOR"%s: In order to remove a pool there must be only 1 free block in the pool. Was possibly freed by another thread\n", TFX_ERROR_NAME);
#else
		TFX_PRINT_ERROR(TFX_ERROR_COLOR"%s: In order to remove a pool there must be only 1 free block in the pool.\n", TFX_ERROR_NAME);
#endif
		return 0;
	}

	void *tfx_Allocate(tfx_allocator *allocator, tfx_size size) {
		tfx_size remote_size = 0;
		tfx__lock_thread_access(allocator);
		size = tfx__adjust_size(size, tfx__MINIMUM_BLOCK_SIZE, tfx__MEMORY_ALIGNMENT);
		tfx_header *block = tfx__find_free_block(allocator, size, remote_size);

		if (block) {
			return tfx__block_user_ptr(block);
		}

		//Out of memory;
		TFX_PRINT_ERROR(TFX_ERROR_COLOR"%s: Not enough memory in pool to allocate %zu bytes\n", TFX_ERROR_NAME, size);
		tfx__unlock_thread_access(allocator);
		return 0;
	}

	void *tfx_Reallocate(tfx_allocator *allocator, void *ptr, tfx_size size) {
		tfx__lock_thread_access(allocator);

		if (ptr && size == 0) {
			tfx__unlock_thread_access(allocator);
			tfx_Free(allocator, ptr);
		}

		if (!ptr) {
			tfx__unlock_thread_access(allocator);
			return tfx_Allocate(allocator, size);
		}

		tfx_header *block = tfx__block_from_allocation(ptr);
		tfx_header *next_block = tfx__next_physical_block(block);
		void *allocation = 0;
		tfx_size current_size = tfx__block_size(block);
		tfx_size adjusted_size = tfx__adjust_size(size, allocator->minimum_allocation_size, tfx__MEMORY_ALIGNMENT);
		tfx_size combined_size = current_size + tfx__block_size(next_block);
		if ((!tfx__next_block_is_free(block) || adjusted_size > combined_size) && adjusted_size > current_size) {
			tfx_header *block = tfx__find_free_block(allocator, adjusted_size, 0);
			if (block) {
				allocation = tfx__block_user_ptr(block);
			}

			if (allocation) {
				tfx_size smallest_size = tfx__Min(current_size, size);
				memcpy(allocation, ptr, smallest_size);
				tfx_Free(allocator, ptr);
			}
		}
		else {
			//Reallocation is possible
			if (adjusted_size > current_size)
			{
				tfx__merge_with_next_block(allocator, block);
				tfx__mark_block_as_used(block);
			}
			tfx_header *split_block = tfx__maybe_split_block(allocator, block, adjusted_size, 0);
			allocation = tfx__block_user_ptr(split_block);
		}

		tfx__unlock_thread_access(allocator);
		return allocation;
	}

	void *tfx_AllocateAligned(tfx_allocator *allocator, tfx_size size, tfx_size alignment) {
		tfx__lock_thread_access(allocator);
		tfx_size adjusted_size = tfx__adjust_size(size, allocator->minimum_allocation_size, alignment);
		tfx_size gap_minimum = sizeof(tfx_header);
		tfx_size size_with_gap = tfx__adjust_size(adjusted_size + alignment + gap_minimum, allocator->minimum_allocation_size, alignment);
		size_t aligned_size = (adjusted_size && alignment > tfx__MEMORY_ALIGNMENT) ? size_with_gap : adjusted_size;

		tfx_header *block = tfx__find_free_block(allocator, aligned_size, 0);

		if (block) {
			void *user_ptr = tfx__block_user_ptr(block);
			void *aligned_ptr = tfx__align_ptr(user_ptr, alignment);
			tfx_size gap = (tfx_size)(((ptrdiff_t)aligned_ptr) - (ptrdiff_t)user_ptr);

			/* If gap size is too small, offset to next aligned boundary. */
			if (gap && gap < gap_minimum)
			{
				tfx_size gap_remain = gap_minimum - gap;
				tfx_size offset = tfx__Max(gap_remain, alignment);
				const void *next_aligned = (void *)((ptrdiff_t)aligned_ptr + offset);

				aligned_ptr = tfx__align_ptr(next_aligned, alignment);
				gap = (tfx_size)((ptrdiff_t)aligned_ptr - (ptrdiff_t)user_ptr);
			}

			if (gap)
			{
				TFX_ASSERT(gap >= gap_minimum && "gap size too small");
				block = tfx__split_aligned_block(allocator, block, gap);
				tfx__block_set_used(block);
			}
			TFX_ASSERT(tfx__ptr_is_aligned(tfx__block_user_ptr(block), alignment));    //pointer not aligned to requested alignment
		}
		else {
			tfx__unlock_thread_access(allocator);
			return 0;
		}

		tfx__unlock_thread_access(allocator);
		return tfx__block_user_ptr(block);
	}

	int tfx_Free(tfx_allocator *allocator, void *allocation) {
		if (!allocation) return 0;
		tfx__lock_thread_access(allocator);
		tfx_header *block = tfx__block_from_allocation(allocation);
		if (tfx__prev_is_free_block(block)) {
			TFX_ASSERT(block->prev_physical_block);        //Must be a valid previous physical block
			block = tfx__merge_with_prev_block(allocator, block);
		}
		if (tfx__next_block_is_free(block)) {
			tfx__merge_with_next_block(allocator, block);
		}
		tfx__push_block(allocator, block);
		tfx__unlock_thread_access(allocator);
		return 1;
	}

#endif

#ifdef __cplusplus
}
#endif

size_t tfxGetNextPower(size_t n);
void tfxAddHostMemoryPool(size_t size);
void *tfxAllocate(size_t size);
void *tfxReallocate(void *memory, size_t size);
void *tfxAllocateAligned(size_t size, size_t alignment);
//Do a safe copy where checks are made to ensure that the boundaries of the memory block being copied to are respected
//This assumes that dst is the start address of the block. If you're copying to a range that is offset from the beginning
//of the block then you can use tfx_SafeCopyBlock instead.
tfx_bool tfx_SafeCopy(void *dst, void *src, tfx_size size);
tfx_bool tfx_SafeCopyBlock(void *dst_block_start, void *dst, void *src, tfx_size size);
tfx_bool tfx_SafeMemset(void *allocation, void *dst, int value, tfx_size size);
tfx_allocator *tfxGetAllocator();

//---------------------------------------
//End of allocator code
//---------------------------------------

//----------------------------------------------------------
//Header_Includes_and_Typedefs
//----------------------------------------------------------
#if defined(__x86_64__) || defined(__i386__) || defined(_M_X64)
#define tfxINTEL
#include <immintrin.h>
#elif defined(__arm__) || defined(__aarch64__)
#include <arm_neon.h>
#include <mach/mach_time.h>
#define tfxARM
#endif

#if defined(WIN32) || defined(_WIN32) || defined(__WIN32__) || defined(__NT__)
#define tfxWINDOWS
#elif __APPLE__
#define tfxMAC
#elif __linux__
#define tfxLINUX
#endif

#include <stdint.h>
#include <math.h>

//type defs
typedef uint16_t tfxU16;
typedef uint32_t tfxU32;
typedef unsigned int tfxEmitterID;
typedef int32_t tfxS32;
typedef uint64_t tfxU64;
typedef int64_t tfxS64;
typedef tfxU32 tfxEffectID;
typedef tfxU32 tfxAnimationID;
typedef tfxU64 tfxKey;
typedef tfxU32 tfxParticleID;
typedef short tfxShort;
typedef unsigned short tfxUShort;

#if defined(_WIN32)
#include <SDKDDKVer.h>
#ifndef WIN_LEAN_AND_MEAN
#define WIN_LEAN_AND_MEAN
#endif
#include <Windows.h>
#endif

#define tfxTWO63 0x8000000000000000u 
#define tfxTWO64f (tfxTWO63*2.0)
#define tfxPI 3.14159265359f
#define tfxHALFPI 1.570796f
#define tfxPI2 6.283185307f 
#define tfxINVTWOPI 0.1591549f
#define tfxTHREEHALFPI 4.7123889f
#define tfxQUARTERPI 0.7853982f
#define tfx360Radians 6.28319f
#define tfx180Radians 3.14159f
#define tfx90Radians 1.5708f
#define tfx270Radians 4.71239f
#define tfxMAXDEPTH 3
#define tfxNL u8"\n"
#define tfxPROPERTY_INDEX_MASK 0x00007FFF
#define tfxSPRITE_ALIGNMENT_MASK 0xFF000000
#define tfxSPRITE_IMAGE_FRAME_MASK 0x00FF0000
#define tfxEXTRACT_SPRITE_ALIGNMENT(property_index) ((property_index & tfxSPRITE_ALIGNMENT_MASK) >> 24)
#define tfxEXTRACT_SPRITE_IMAGE_FRAME(property_index) ((property_index & tfxSPRITE_IMAGE_FRAME_MASK) >> 16)
#define tfxEXTRACT_SPRITE_PROPERTY_INDEX(property_index) (property_index & tfxPROPERTY_INDEX_MASK)
#define tfxPACK_SCALE_AND_HANDLE(x, y, lib, property_index) (tfxU16)(x * 127.9960938f) | ((tfxU16)(y * 127.9960938f) << 16) | ((tfxU64)lib->emitter_properties[property_index].image_handle_packed << 32)
#define tfxPACK_SIZE_AND_HANDLE(x, y, lib, property_index) (tfxU16)(x * 7.999755859f) | ((tfxU16)(y * 7.999755859f) << 16) | ((tfxU64)lib->emitter_properties[property_index].image_handle_packed << 32)
#define tfxCIRCLENODES 16
#define tfxPrint(message, ...) printf(message tfxNL, ##__VA_ARGS__)

//----------------------------------------------------------
//Forward declarations

struct tfx_effect_emitter_t;
struct tfx_particle_manager_t;
struct tfx_effect_template_t;
struct tfx_compute_sprite_t;
struct tfx_compute_particle_t;
struct tfx_sprite_sheet_settings_t;
struct tfx_sprite_data_settings_t;
struct tfx_library_t;
struct tfx_str16_t;
struct tfx_str32_t;
struct tfx_str64_t;
struct tfx_str128_t;
struct tfx_str256_t;
struct tfx_str512_t;

//--------------------------------------------------------------
//macros
#define TFX_VERSION "Alpha"
#define TFX_VERSION_NUMBER 6.18.2024

#define tfxMAX_FRAME 20000.f
#define tfxCOLOR_RAMP_WIDTH 256
#define tfxNullParent 0xFFFFFFFF
#define tfxINVALID 0xFFFFFFFF
#define tfxINVALID_SPRITE 0x0FFFFFFF
#define tfxEmitterPropertiesCount 26

#define tfxDel << "=" <<
#define tfxCom << "," <<
#define tfxEndLine << std::endl

#define tfxDelt "=" 
#define tfxComt ","

#define tfxMin(a, b) (((a) < (b)) ? (a) : (b))
#define tfxMax(a, b) (((a) > (b)) ? (a) : (b))
#define tfxArrayCount(Array) (sizeof(Array) / sizeof((Array)[0]))

#ifndef tfxREALLOCATE
#define tfxALLOCATE(size) tfxAllocate(size)
#define tfxALLOCATE_ALIGNED(size, alignment) tfxAllocateAligned(size, alignment)
#define tfxREALLOCATE(ptr, size) tfxReallocate(ptr, size)
#endif

#ifndef tfxREALLOCATE
#define tfxALLOCATE(size) malloc(size)
#define tfxALLOCATE_ALIGNED(size, alignment) malloc(alignment)
#define tfxREALLOCATE(ptr, size) realloc(ptr, size)
#endif

#ifndef tfxFREE
#define tfxFREE(memory) tfx_Free(tfxGetAllocator(), memory)
#endif

#ifndef tfxFREE
#define tfxFREE(memory) free(memory)
#endif

/*    Functions come in 3 flavours:
1) INTERNAL where they're only meant for internal use by the library and not for any use outside it. Note that these functions are declared as static.
2) API where they're meant for access within your games that you're developing
3) EDITOR where they can be accessed from outside the library but really they're mainly useful for editing the effects such as in in the TimelineFX Editor.

All functions in the library will be marked this way for clarity and naturally the API functions will all be properly documented.
*/
//Function marker for any functions meant for external/api use
#ifdef __cplusplus
#define tfxAPI extern "C"
#define tfxAPI_EDITOR extern "C"
#else
#define tfxAPI
#endif    

//Function marker for any functions meant mainly for use by the TimelineFX editor and are related to editing effects
//For internal functions
#define tfxINTERNAL static    

//Override this for more layers, although currently the editor is fixed at 4
#ifndef tfxLAYERS
#define tfxLAYERS 4
#endif 


/*
Helper macro to place inside a for loop, for example:
for(tfxEachLayer)
You can then use layer inside the loop to get the current layer
*/
#define tfxEachLayer int layer = 0; layer != tfxLAYERS; ++layer
#define tfxForEachLayer for (int layer = 0; layer != tfxLAYERS; ++layer)

//Internal use macro

union tfxUInt10bit
{
	struct
	{
		int x : 10;
		int y : 10;
		int z : 10;
		int w : 2;
	} data;
	tfxU32 pack;
};

union tfxUInt8bit
{
	struct
	{
		int x : 8;
		int y : 8;
		int z : 8;
		int w : 8;
	} data;
	tfxU32 pack;
};

//Section: OS_Specific_Functions
#ifdef _WIN32
FILE *tfx__open_file(const char *file_name, const char *mode);

tfxINTERNAL inline tfxU64 tfx_AtomicAdd64(tfxU64 volatile *value, tfxU64 amount_to_add) {
	tfxU64 result = _InterlockedExchangeAdd64((__int64 volatile *)value, amount_to_add);
	return result;
}

tfxINTERNAL inline tfxU32 tfx_AtomicAdd32(tfxU32 volatile *value, tfxU32 amount_to_add) {
	tfxU32 result = _InterlockedExchangeAdd((LONG *)value, amount_to_add);
	return result;
}
#else
FILE *tfx__open_file(const char *file_name, const char *mode);

inline tfxU64 tfx_AtomicAdd64(tfxU64 volatile *value, tfxU64 amount_to_add) {
	return __sync_fetch_and_add(value, amount_to_add);
}

inline tfxU32 tfx_AtomicAdd32(tfxU32 volatile *value, tfxU32 amount_to_add) {
	return __sync_fetch_and_add(value, amount_to_add);
}
#endif

#ifdef _WIN32
tfxINTERNAL tfxU32 tfx_Millisecs(void) {
	LARGE_INTEGER frequency, counter;
	QueryPerformanceFrequency(&frequency);
	QueryPerformanceCounter(&counter);
	tfxU64 ms = (tfxU64)(counter.QuadPart * 1000LL / frequency.QuadPart);
	return (tfxU32)ms;
}

tfxINTERNAL tfxU64 tfx_Microsecs(void) {
	LARGE_INTEGER frequency, counter;
	QueryPerformanceFrequency(&frequency);
	QueryPerformanceCounter(&counter);
	tfxU64 us = (tfxU64)(counter.QuadPart * 1000000LL / frequency.QuadPart);
	return (tfxU64)us;
}
#elif defined(__APPLE__)
#include <mach/mach_time.h>
tfxINTERNAL inline tfxU32 tfx_Millisecs(void) {
	static mach_timebase_info_data_t timebase_info;
	if (timebase_info.denom == 0) {
		mach_timebase_info(&timebase_info);
	}

	uint64_t time_ns = mach_absolute_time() * timebase_info.numer / timebase_info.denom;
	tfxU32 ms = (tfxU32)(time_ns / 1000000);
	return (tfxU32)ms;
}

tfxINTERNAL inline tfxU64 tfx_Microsecs(void) {
	static mach_timebase_info_data_t timebase_info;
	if (timebase_info.denom == 0) {
		mach_timebase_info(&timebase_info);
	}

	uint64_t time_ns = mach_absolute_time() * timebase_info.numer / timebase_info.denom;
	tfxU64 us = (tfxU64)(time_ns / 1000);
	return us;
}
#else
tfxINTERNAL inline tfxU32 tfx_Millisecs(void) {
	struct timespec now;
	clock_gettime(CLOCK_REALTIME, &now);
	long m = now.tv_sec * 1000 + now.tv_nsec / 1000000;
	return (tfxU32)m;
}

tfxINTERNAL inline tfxU64 tfx_Microsecs(void) {
	struct timespec now;
	clock_gettime(CLOCK_REALTIME, &now);
	tfxU64 us = now.tv_sec * 1000000ULL + now.tv_nsec / 1000;
	return (tfxU64)us;
}
#endif

// --Pocket_Hasher, converted to c from Stephen Brumme's XXHash code (https://github.com/stbrumme/xxhash) by Peter Rigby
/*
	MIT License Copyright (c) 2018 Stephan Brumme
	Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
	The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
	THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.  */
#define tfx__PRIME1 11400714785074694791ULL
#define tfx__PRIME2 14029467366897019727ULL
#define tfx__PRIME3 1609587929392839161ULL
#define tfx__PRIME4 9650029242287828579ULL
#define tfx__PRIME5 2870177450012600261ULL

enum { tfx__HASH_MAX_BUFFER_SIZE = 31 + 1 };

typedef struct tfx_hasher_s {
	tfxU64      state[4];
	unsigned char buffer[tfx__HASH_MAX_BUFFER_SIZE];
	tfxU64      buffer_size;
	tfxU64      total_length;
} tfx_hasher_t;

tfxINTERNAL inline tfxU64 tfx__hash_rotate_left(tfxU64 x, unsigned char bits) {
	return (x << bits) | (x >> (64 - bits));
}
tfxINTERNAL inline tfxU64 tfx__hash_process_single(tfxU64 previous, tfxU64 input) {
	return tfx__hash_rotate_left(previous + input * tfx__PRIME2, 31) * tfx__PRIME1;
}
tfxINTERNAL inline void tfx__hasher_process(const void *data, tfxU64 *state0, tfxU64 *state1, tfxU64 *state2, tfxU64 *state3) {
	tfxU64 *block = (tfxU64 *)data;
	tfxU64 blocks[4];
	memcpy(blocks, data, sizeof(tfxU64) * 4);
	*state0 = tfx__hash_process_single(*state0, blocks[0]);
	*state1 = tfx__hash_process_single(*state1, blocks[1]);
	*state2 = tfx__hash_process_single(*state2, blocks[2]);
	*state3 = tfx__hash_process_single(*state3, blocks[3]);
}
tfxINTERNAL inline int tfx__hasher_add(tfx_hasher_t *hasher, const void *input, tfxU64 length)
{
	if (!input || length == 0) return 0;

	hasher->total_length += length;
	unsigned char *data = (unsigned char *)input;

	if (hasher->buffer_size + length < tfx__HASH_MAX_BUFFER_SIZE)
	{
		while (length-- > 0)
			hasher->buffer[hasher->buffer_size++] = *data++;
		return 1;
	}

	const unsigned char *stop = data + length;
	const unsigned char *stopBlock = stop - tfx__HASH_MAX_BUFFER_SIZE;

	if (hasher->buffer_size > 0)
	{
		while (hasher->buffer_size < tfx__HASH_MAX_BUFFER_SIZE)
			hasher->buffer[hasher->buffer_size++] = *data++;

		tfx__hasher_process(hasher->buffer, &hasher->state[0], &hasher->state[1], &hasher->state[2], &hasher->state[3]);
	}

	tfxU64 s0 = hasher->state[0], s1 = hasher->state[1], s2 = hasher->state[2], s3 = hasher->state[3];
	int test = tfx__ptr_is_aligned(&s0, 8);
	while (data <= stopBlock)
	{
		tfx__hasher_process(data, &s0, &s1, &s2, &s3);
		data += 32;
	}
	hasher->state[0] = s0; hasher->state[1] = s1; hasher->state[2] = s2; hasher->state[3] = s3;

	hasher->buffer_size = stop - data;
	for (tfxU64 i = 0; i < hasher->buffer_size; i++)
		hasher->buffer[i] = data[i];

	return 1;
}

tfxINTERNAL inline tfxU64 tfx__get_hash(tfx_hasher_t *hasher)
{
	tfxU64 result;
	if (hasher->total_length >= tfx__HASH_MAX_BUFFER_SIZE)
	{
		result = tfx__hash_rotate_left(hasher->state[0], 1) +
			tfx__hash_rotate_left(hasher->state[1], 7) +
			tfx__hash_rotate_left(hasher->state[2], 12) +
			tfx__hash_rotate_left(hasher->state[3], 18);
		result = (result ^ tfx__hash_process_single(0, hasher->state[0])) * tfx__PRIME1 + tfx__PRIME4;
		result = (result ^ tfx__hash_process_single(0, hasher->state[1])) * tfx__PRIME1 + tfx__PRIME4;
		result = (result ^ tfx__hash_process_single(0, hasher->state[2])) * tfx__PRIME1 + tfx__PRIME4;
		result = (result ^ tfx__hash_process_single(0, hasher->state[3])) * tfx__PRIME1 + tfx__PRIME4;
	} else
	{
		result = hasher->state[2] + tfx__PRIME5;
	}

	result += hasher->total_length;
	const unsigned char *data = hasher->buffer;
	const unsigned char *stop = data + hasher->buffer_size;
	for (; data + 8 <= stop; data += 8)
		result = tfx__hash_rotate_left(result ^ tfx__hash_process_single(0, *(tfxU64 *)data), 27) * tfx__PRIME1 + tfx__PRIME4;
	if (data + 4 <= stop)
	{
		result = tfx__hash_rotate_left(result ^ (*(tfxU32 *)data) * tfx__PRIME1, 23) * tfx__PRIME2 + tfx__PRIME3;
		data += 4;
	}
	while (data != stop)
		result = tfx__hash_rotate_left(result ^ (*data++) * tfx__PRIME5, 11) * tfx__PRIME1;
	result ^= result >> 33;
	result *= tfx__PRIME2;
	result ^= result >> 29;
	result *= tfx__PRIME3;
	result ^= result >> 32;
	return result;
}

tfxAPI_EDITOR inline void tfx__hash_initialise(tfx_hasher_t *hasher, tfxU64 seed) {
	hasher->state[0] = seed + tfx__PRIME1 + tfx__PRIME2; hasher->state[1] = seed + tfx__PRIME2; hasher->state[2] = seed; hasher->state[3] = seed - tfx__PRIME1; hasher->buffer_size = 0; hasher->total_length = 0;
}

//The only command you need for the hasher. Just used internally by the hash map.
tfxAPI_EDITOR inline tfxKey tfx_Hash(tfx_hasher_t *hasher, const void *input, tfxU64 length, tfxU64 seed) {
	tfx__hash_initialise(hasher, seed); tfx__hasher_add(hasher, input, length); return (tfxKey)tfx__get_hash(hasher);
}
//-- End of Pocket Hasher

//----------------------------------------------------------
//Section: SIMD_defines
//----------------------------------------------------------

//Define tfxUSEAVX if you want to compile and use AVX simd operations for updating particles, otherwise SSE will be
//used by default
//Note that avx is currently only slightly faster than SSE, probably because memory bandwidth/caching becomes more of an issue at that point. But also I could be doing it wrong!
#ifdef tfxUSEAVX
#define tfxDataWidth 8    
typedef __m256 tfxWideFloat;
typedef __m256i tfxWideInt;
typedef __m256i tfxWideIntLoader;
#define tfxWideLoad _mm256_load_ps
#define tfxWideLoadi _mm256_load_si256
#define tfx128Set _mm_set_ps
#define tfx128Seti _mm_set_epi32
#define tfx128SetSingle _mm_set_ps1
#define tfx128SetSinglei _mm_set1_epi32
#define tfxWideSet _mm256_set_ps
#define tfxWideSetSingle _mm256_set1_ps
#define tfxWideSeti _mm256_set_epi32
#define tfxWideSetSinglei _mm256_set1_epi32
#define tfxWideAdd _mm256_add_ps
#define tfxWideSub _mm256_sub_ps
#define tfxWideMul _mm256_mul_ps
#define tfxWideDiv _mm256_div_ps
#define tfxWideAddi _mm256_add_epi32
#define tfxWideSubi _mm256_sub_epi32
#define tfxWideMuli _mm256_mullo_epi32
#define tfxWideSqrt _mm256_sqrt_ps
#define tfxWideRSqrt _mm256_rsqrt_ps
#define tfxWideMoveMask _mm256_movemask_epi8
#define tfxWideShiftRight _mm256_srli_epi32
#define tfxWideShiftLeft _mm256_slli_epi32
#define tfxWideGreaterEqual(v1, v2) _mm256_cmp_ps(v1, v2, _CMP_GE_OS)
#define tfxWideGreater(v1, v2) _mm256_cmp_ps(v1, v2, _CMP_GT_OS)
#define tfxWideGreateri _mm256_cmpgt_epi32
#define tfxWideLess(v1, v2) _mm256_cmp_ps(v1, v2, _CMP_LT_OS)
#define tfxWideLessi(v1, v2) _mm256_cmpgt_epi32(v2, v1)
#define tfxWideLessEqeual(v1, v2) _mm256_cmp_ps(v1, v2, _CMP_LE_OS)
#define tfxWideEquals(v1, v2) _mm256_cmp_ps(v1, v2, _CMP_EQ_OS)
#define tfxWideEqualsi _mm256_cmpeq_epi32 
#define tfxWideStore _mm256_store_ps
#define tfxWideStorei _mm256_store_si256
#define tfxWideCasti _mm256_castps_si256
#define tfxWideCast _mm256_castsi256_ps 
#define tfxWideConverti _mm256_cvttps_epi32 
#define tfxWideConvert    _mm256_cvtepi32_ps 
#define tfxWideMin _mm256_min_ps
#define tfxWideMax _mm256_max_ps
#define tfxWideMini _mm256_min_epi32
#define tfxWideMaxi _mm256_max_epi32
#define tfxWideOr _mm256_or_ps
#define tfxWideOri _mm256_or_si256
#define tfxWideXOri _mm256_xor_si256
#define tfxWideXOr _mm256_xor_ps
#define tfxWideAnd _mm256_and_ps
#define tfxWideAndi _mm256_and_si256
#define tfxWideAndNot _mm256_andnot_ps
#define tfxWideAndNoti _mm256_andnot_si256
#define tfxWideSetZero _mm256_setzero_ps()
#define tfxWideSetZeroi _mm256_setzero_si256()
#define tfxWideEqualsi _mm256_cmpeq_epi32 
#define tfxWideLookupSet(lookup, index) tfxWideSet(lookup[index.a[7]], lookup[index.a[6]], lookup[index.a[5]], lookup[index.a[4]], lookup[index.a[3]], lookup[index.a[2]], lookup[index.a[1]], lookup[index.a[0]] )
#define tfxWideLookupSeti(lookup, index) tfxWideSeti(lookup[index.a[7]], lookup[index.a[6]], lookup[index.a[5]], lookup[index.a[4]], lookup[index.a[3]], lookup[index.a[2]], lookup[index.a[1]], lookup[index.a[0]] )
#define tfxWideLookupSetColor(lookup, index) tfxWideSeti(lookup[index.a[7]].color, lookup[index.a[6]].color, lookup[index.a[5]].color, lookup[index.a[4]].color, lookup[index.a[3]].color, lookup[index.a[2]].color, lookup[index.a[1]].color, lookup[index.a[0]].color )
#define tfxWideLookupSetMember(lookup, member, index) tfxWideSet(lookup[index.a[7]].member, lookup[index.a[6]].member, lookup[index.a[5]].member, lookup[index.a[4]].member, lookup[index.a[3]].member, lookup[index.a[2]].member, lookup[index.a[1]].member, lookup[index.a[0]].member )
#define tfxWideLookupSetMemberi(lookup, member, index) tfxWideSeti(lookup[index.a[7]].member, lookup[index.a[6]].member, lookup[index.a[5]].member, lookup[index.a[4]].member, lookup[index.a[3]].member, lookup[index.a[2]].member, lookup[index.a[1]].member, lookup[index.a[0]].member )
#define tfxWideLookupSet2(lookup1, lookup2, index1, index2) tfxWideSet(lookup1[index1.a[7]].lookup2[index2.a[7]], lookup1[index1.a[6]].lookup2[index2.a[6]], lookup1[index1.a[5]].lookup2[index2.a[5]], lookup1[index1.a[4]].lookup2[index2.a[4]], lookup1[index1.a[3]].lookup2[index2.a[3]], lookup1[index1.a[2]].lookup2[index2.a[2]], lookup1[index1.a[1]].lookup2[index2.a[1]], lookup1[index1.a[0]].lookup2[index2.a[0]] )
#define tfxWideLookupSetOffset(lookup, index, offset) tfxWideSet(lookup[index.a[7] + offset], lookup[index.a[6] + offset], lookup[index.a[5] + offset], lookup[index.a[4] + offset], lookup[index.a[3] + offset], lookup[index.a[2] + offset], lookup[index.a[1] + offset], lookup[index.a[0] + offset] )

#define tfxWideSetConst(value) {value, value, value, value, value, value, value, value}

const __m256 tfxWIDEF3_4 = tfxWideSetConst(1.0f / 3.0f);
const __m256 tfxWIDEG3_4 = tfxWideSetConst(1.0f / 6.0f);
const __m256 tfxWIDEG32_4 = tfxWideSetConst((1.0f / 6.0f) * 2.f);
const __m256 tfxWIDEG33_4 = tfxWideSetConst((1.0f / 6.0f) * 3.f);
const __m256i tfxWIDEONEi = tfxWideSetConst(1);
const __m256 tfxWIDEMINUSONE = tfxWideSetConst(-1.f);
const __m256i tfxWIDEMINUSONEi = tfxWideSetConst(-1);
const __m256 tfxWIDEONE = tfxWideSetConst(1.f);
const __m256 tfxWIDE255 = tfxWideSetConst(255.f);
const __m256 tfxWIDEZERO = tfxWideSetConst(0.f);
const __m256 tfxWIDETHIRTYTWO = tfxWideSetConst(32.f);
const __m256i tfxWIDEFF = tfxWideSetConst(0xFF);
const __m256 tfxPWIDESIX = tfxWideSetConst(0.6f);
const __m256 tfxMAXUINTf = tfxWideSetConst((float)UINT32_MAX);
const __m256 tfxDEGREERANGEMR = tfxWideSetConst(0.392699f);

tfxINTERNAL const __m256 SIGNMASK = tfxWideSetConst(-0.f);

typedef union {
	__m256i m;
	int a[8];
} tfxWideArrayi;

typedef union {
	__m256 m;
	float a[8];
} tfxWideArray;

#else

#define tfxDataWidth 4

#ifdef tfxINTEL
//Intel Intrinsics
typedef __m128 tfxWideFloat;
typedef __m128i tfxWideInt;
typedef __m128i tfxWideIntLoader;
#define tfxWideLoad _mm_load_ps
#define tfxWideLoadi _mm_load_si128
#define tfxWideSet _mm_set_ps
#define tfxWideSetSingle _mm_set_ps1
#define tfx128Load _mm_load_ps
#define tfx128Set _mm_set_ps
#define tfx128Seti _mm_set_epi32
#define tfx128SetSingle _mm_set_ps1
#define tfx128SetSinglei _mm_set1_epi32
#define tfxWideSeti _mm_set_epi32
#define tfxWideSetSinglei _mm_set1_epi32
#define tfxWideAdd _mm_add_ps
#define tfxWideSub _mm_sub_ps
#define tfxWideMul _mm_mul_ps
#define tfxWideDiv _mm_div_ps
#define tfxWideAddi _mm_add_epi32
#define tfxWideSubi _mm_sub_epi32
#define tfxWideMuli _mm_mullo_epi32
#define tfxWideSqrt _mm_sqrt_ps
#define tfxWideRSqrt _mm_rsqrt_ps
#define tfxWideMoveMask _mm_movemask_epi8
#define tfxWideShiftRight _mm_srli_epi32
#define tfxWideShiftLeft _mm_slli_epi32
#define tfxWideGreaterEqual(v1, v2) _mm_cmpge_ps(v1, v2)
#define tfxWideGreater(v1, v2) _mm_cmpgt_ps(v1, v2)
#define tfxWideGreateri(v1, v2) _mm_cmpgt_epi32(v1, v2)
#define tfxWideLessEqual(v1, v2) _mm_cmple_ps(v1, v2)
#define tfxWideLess(v1, v2) _mm_cmplt_ps(v1, v2)
#define tfxWideLessi(v1, v2) _mm_cmplt_epi32(v1, v2)
#define tfxWideStore _mm_store_ps
#define tfxWideStorei _mm_store_si128
#define tfxWideCasti _mm_castps_si128 
#define tfxWideCast _mm_castsi128_ps
#define tfxWideConverti _mm_cvttps_epi32 
#define tfxWideConvert _mm_cvtepi32_ps 
#define tfxWideMin _mm_min_ps
#define tfxWideMax _mm_max_ps
#define tfxWideMini _mm_min_epi32
#define tfxWideMaxi _mm_max_epi32
#define tfxWideOr _mm_or_ps
#define tfxWideXOr _mm_xor_ps
#define tfxWideOri _mm_or_si128
#define tfxWideXOri _mm_xor_si128
#define tfxWideAnd _mm_and_ps
#define tfxWideAndNot _mm_andnot_ps
#define tfxWideAndi _mm_and_si128
#define tfxWideAndNoti _mm_andnot_si128
#define tfxWideSetZeroi _mm_setzero_si128()
#define tfxWideSetZero _mm_setzero_ps()
#define tfxWideEqualsi _mm_cmpeq_epi32
#define tfxWideEquals _mm_cmpeq_ps
#define tfxWideShufflei _mm_shuffle_epi32

#define tfxWideSetConst(value) {value, value, value, value}

const __m128 tfxWIDEF3_4 = tfxWideSetConst(1.f / 3.f);
const __m128 tfxWIDEG3_4 = tfxWideSetConst(1.0f / 6.0f);
const __m128 tfxWIDEG32_4 = tfxWideSetConst((1.0f / 6.0f) * 2.f);
const __m128 tfxWIDEG33_4 = tfxWideSetConst((1.0f / 6.0f) * 3.f);
const __m128i tfxWIDEONEi = tfxWideSetConst(1);
const __m128 tfxWIDEMINUSONE = tfxWideSetConst(-1.f);
const __m128i tfxWIDEMINUSONEi = tfxWideSetConst(-1);
const __m128 tfxWIDEONE = tfxWideSetConst(1.f);
const __m128 tfxWIDE255 = tfxWideSetConst(255.f);
const __m128 tfxWIDEZERO = tfxWideSetConst(0.f);
const __m128 tfxWIDETHIRTYTWO = tfxWideSetConst(32.f);
const __m128i tfxWIDEFF = tfxWideSetConst(0xFF);
const __m128 tfxPWIDESIX = tfxWideSetConst(0.6f);
const __m128 tfxMAXUINTf = tfxWideSetConst((float)UINT32_MAX);
const __m128 tfxDEGREERANGEMR = tfxWideSetConst(0.392699f);

tfxINTERNAL const __m128 SIGNMASK = tfxWideSetConst(-0.f);

typedef union {
	__m128i m;
	int a[4];
} tfxWideArrayi;

typedef union {
	__m128 m;
	float a[4];
} tfxWideArray;

#elif defined(tfxARM)
//Arm Intrinsics
typedef float32x4_t tfxWideFloat;
typedef int32x4_t tfxWideInt;
typedef int32_t tfxWideIntLoader;
#define tfxWideLoad vld1q_f32
#define tfxWideLoadi vld1q_s32
inline __attribute__((always_inline)) float32x4_t tfx__128_SET(float e3, float e2, float e1, float e0) {
	float32x4_t r;
	alignas(16) float data[4] = { e0, e1, e2, e3 };
	r = vld1q_f32(data);
	return r;
}
inline __attribute__((always_inline)) int32x4_t tfx__128i_SET(int e3, int e2, int e1, int e0) {
	int32x4_t r;
	alignas(16) int data[4] = { e0, e1, e2, e3 };
	r = vld1q_s32(data);
	return r;
}
#define tfx128Load vld1q_f32
#define tfx128Set tfx__128_SET
#define tfx128Seti tfx__128i_SET
#define tfx128SetSingle vdupq_n_f32
#define tfx128SetSinglei vdupq_n_s32
#define tfxWideSet vld1q_f32
#define tfxWideSetSingle vdupq_n_f32
#define tfxWideSeti vld1q_s32
#define tfxWideSetSinglei vdupq_n_s32
#define tfxWideAdd vaddq_f32
#define tfxWideSub vsubq_f32
#define tfxWideMul vmulq_f32
#define tfxWideDiv vdivq_f32
#define tfxWideAddi vaddq_s32
#define tfxWideSubi vsubq_s32
#define tfxWideMuli vmulq_s32
#define tfxWideRSqrt vrsqrteq_f32 // for reciprocal square root approximation
#define tfxWideShiftRight vshrq_n_u32
#define tfxWideShiftLeft vshlq_n_u32
#define tfxWideGreaterEqual(a, b) vreinterpretq_f32_u32(vcgeq_f32(a, b))
#define tfxWideGreater(a, b) vreinterpretq_s32_u32(vcgeq_f32(a,b))
#define tfxWideGreateri vcgtq_s32
#define tfxWideLessEqual(a, b) vreinterpretq_f32_u32(vcleq_f32(a, b))
#define tfxWideLess(a, b) vreinterpretq_f32_u32(vcltq_f32(a, b))
#define tfxWideLessi vcltq_s32
#define tfxWideStore vst1q_f32
#define tfxWideStorei vst1q_s32
#define tfxWideCasti vreinterpretq_s32_f32
#define tfxWideCast vreinterpretq_f32_s32
#define tfxWideConverti vcvtq_s32_f32
#define tfxWideConvert vcvtq_f32_s32
#define tfxWideMin vminq_f32
#define tfxWideMax vmaxq_f32
#define tfxWideMini vminq_s32
#define tfxWideMaxi vmaxq_s32
#define tfxWideOr(a, b) vreinterpretq_f32_s32(vorrq_s32(vreinterpretq_s32_f32(a), vreinterpretq_s32_f32(b)))
#define tfxWideXOr(a, b) vreinterpretq_f32_s32(veorq_s32(vreinterpretq_s32_f32(a), vreinterpretq_s32_f32(b)))
#define tfxWideOri vorrq_s32
#define tfxWideXOri veorq_s32
#define tfxWideAnd(a, b) vreinterpretq_f32_s32(vandq_s32(vreinterpretq_s32_f32(a), vreinterpretq_s32_f32(b)))
#define tfxWideAndi vandq_s32
#define tfxWideAndNoti vbicq_s32
//#define tfxWideAndNot(a, b) vreinterpretq_f32_s32(vandq_s32(vmvnq_s32(vreinterpretq_s32_f32(a)), vreinterpretq_s32_f32(b)))
#define tfxWideAndNot(a, b) vreinterpretq_f32_s32(vbicq_s32(vreinterpretq_s32_f32(b), vreinterpretq_s32_f32(a)))
#define tfxWideSetZeroi vdupq_n_s32(0)
#define tfxWideSetZero vdupq_n_f32(0.0f)
#define tfxWideEqualsi vceqq_s32
#define tfxWideEquals(a, b) vreinterpretq_f32_u32(vceqq_f32(a, b))

#define tfxSIMD_AND(a,b) vreinterpretq_f32_s32(vandq_s32(vreinterpretq_s32_f32(a),vreinterpretq_s32_f32(b)))
#define tfxSIMD_AND_NOT(a,b) vreinterpretq_f32_s32(vandq_s32(vmvnq_s32(vreinterpretq_s32_f32(a)),vreinterpretq_s32_f32(b)))
#define tfxSIMD_XOR(a,b) vreinterpretq_f32_s32(veorq_s32(vreinterpretq_s32_f32(a),vreinterpretq_s32_f32(b)))

#define tfxWideSetConst(value) {value, value, value, value}

const float32x4_t tfxWIDEF3_4 = tfxWideSetConst(1.0f / 3.0f);
const float32x4_t tfxWIDEG3_4 = tfxWideSetConst(1.0f / 6.0f);
const float32x4_t tfxWIDEG32_4 = tfxWideSetConst((1.0f / 6.0f) * 2.f);
const float32x4_t tfxWIDEG33_4 = tfxWideSetConst((1.0f / 6.0f) * 3.f);
const int32x4_t tfxWIDEONEi = tfxWideSetConst(1);
const float32x4_t tfxWIDEMINUSONE = tfxWideSetConst(-1.f);
const int32x4_t tfxWIDEMINUSONEi = tfxWideSetConst(-1);
const float32x4_t tfxWIDEONE = tfxWideSetConst(1.f);
const float32x4_t tfxWIDE255 = tfxWideSetConst(255.f);
const float32x4_t tfxWIDEZERO = tfxWideSetConst(0.f);
const float32x4_t tfxWIDETHIRTYTWO = tfxWideSetConst(32.f);
const int32x4_t tfxWIDEFF = tfxWideSetConst(0xFF);
const float32x4_t tfxPWIDESIX = tfxWideSetConst(0.6f);
const float32x4_t tfxMAXUINTf = tfxWideSetConst((float)UINT32_MAX);
const float32x4_t tfxDEGREERANGEMR = tfxWideSetConst(0.392699f);

tfxINTERNAL const float32x4_t SIGNMASK = tfxWideSetConst(-0.f);

typedef union {
	int32x4_t m;
	int a[4];
} tfxWideArrayi;

typedef union {
	float32x4_t m;
	float a[4];
} tfxWideArray;


#endif

#define tfxWideLookupSet(lookup, index) tfx128Set( lookup[index.a[3]], lookup[index.a[2]], lookup[index.a[1]], lookup[index.a[0]] )
#define tfxWideLookupSetMember(lookup, member, index) tfx128Set( lookup[index.a[3]].member, lookup[index.a[2]].member, lookup[index.a[1]].member, lookup[index.a[0]].member )
#define tfxWideLookupSetMemberi(lookup, member, index) tfx128Seti( lookup[index.a[3]].member, lookup[index.a[2]].member, lookup[index.a[1]].member, lookup[index.a[0]].member )
#define tfxWideLookupSet2(lookup1, lookup2, index1, index2) tfx128Set( lookup1[index1.a[3]].lookup2[index2.a[3]], lookup1[index1.a[2]].lookup2[index2.a[2]], lookup1[index1.a[1]].lookup2[index2.a[1]], lookup1[index1.a[0]].lookup2[index2.a[0]] )
#define tfxWideLookupSeti(lookup, index) tfx128Seti( lookup[index.a[3]], lookup[index.a[2]], lookup[index.a[1]], lookup[index.a[0]] )
#define tfxWideLookupSetColor(lookup, index) tfx128Seti( lookup[index.a[3]].color, lookup[index.a[2]].color, lookup[index.a[1]].color, lookup[index.a[0]].color )
#define tfxWideLookupSetOffset(lookup, index, offset) tfx128Set( lookup[index.a[3] + offset], lookup[index.a[2] + offset], lookup[index.a[1] + offset], lookup[index.a[0] + offset] )

#endif

#ifdef tfxINTEL
typedef __m128 tfx128;
typedef __m128i tfx128i;

typedef union {
	__m128i m;
	int a[4];
} tfx128iArray;

typedef union {
	__m128i m;
	tfxU64 a[2];
} tfx128iArray64;

typedef union {
	__m128 m;
	float a[4];
} tfx128Array;

//simd floor function thanks to Stephanie Rancourt: http://dss.stephanierct.com/DevBlog/?p=8
tfxINTERNAL inline tfx128 tfxFloor128(const tfx128 *x) {
	//__m128i v0 = _mm_setzero_si128();
	//__m128i v1 = _mm_cmpeq_epi32(v0, v0);
	//__m128i ji = _mm_srli_epi32(v1, 25);
	//__m128 j = *(__m128*)&_mm_slli_epi32(ji, 23); //create vector 1.0f
	//Worth noting that we only need to floor small numbers for the noise algorithm so can get away with this function.
	__m128 j = _mm_set1_ps(1.f); //create vector 1.0f
	__m128i i = _mm_cvttps_epi32(*x);
	__m128 fi = _mm_cvtepi32_ps(i);
	__m128 igx = _mm_cmpgt_ps(fi, *x);
	j = _mm_and_ps(igx, j);
	return _mm_sub_ps(fi, j);
}

tfxINTERNAL inline uint64_t tfx__rdtsc() {
	return __rdtsc();
}

#elif defined(tfxARM)

typedef float32x4_t tfx128;
typedef int32x4_t tfx128i;

typedef union {
	tfx128i m;
	int a[4];
} tfx128iArray;

typedef union {
	tfx128i m;
	tfxU64 a[2];
} tfx128iArray64;

typedef union {
	tfx128 m;
	float a[4];
} tfx128Array;

#define tfxFloor128 vrndmq_f32

tfxINTERNAL inline uint64_t tfx__rdtsc() {
	return mach_absolute_time();
}

#endif

inline tfxWideArrayi tfxConvertWideArray(tfxWideInt in) {
	tfxWideArrayi out;
	out.m = in;
	return out;
}

/*
Copyright (c) 2013, Robin Lobel
All rights reserved. https://github.com/divideconcept/FastTrigo

I just extracted the necessary functions that I need from the above code base and modified to use tfx SIMD_DEFINES
Also fixed a bug in atan2 function where x <= y
*/

tfxINTERNAL inline tfxWideFloat tfxWideFastSqrt(tfxWideFloat squared)
{
	//    static int csr = 0;
	//#if defined(tfxINTEL)
		//if (!csr) csr = _mm_getcsr() | 0x8040; //DAZ,FTZ (divide by zero=0)
		//_mm_setcsr(csr);
	//#endif
	return tfxWideMul(tfxWideRSqrt(squared), squared);
}

/*
possible arm function for andnot
float32x4_t andnot_ps(float32x4_t a, float32x4_t b) {
	uint32x4_t not_b = vreinterpretq_u32_f32(vmvnq_f32(b)); // Bitwise NOT of b
	return vreinterpretq_f32_u32(vandq_u32(vreinterpretq_u32_f32(a), not_b)); // Bitwise AND of a and NOT(b)
}
*/

tfxINTERNAL inline tfxWideFloat tfxWideAtan(tfxWideFloat x)
{
	//                                      tfxQUARTERPI*x
	//                                      - x*(fabs(x) - 1)
	//                                      *(0.2447f+0.0663f*fabs(x));
	return tfxWideSub(tfxWideMul(tfxWideSetSingle(tfxQUARTERPI), x),
		tfxWideMul(tfxWideMul(x, tfxWideSub(tfxWideAndNot(SIGNMASK, x), tfxWideSetSingle(1.f))),
			(tfxWideAdd(tfxWideSetSingle(0.2447f), tfxWideMul(tfxWideSetSingle(0.0663f), tfxWideAndNot(SIGNMASK, x))))));
}

/*
float atan2(float y, float x)
{
	if (fabs(x) > fabs(y)) {
		float atan = atanf(y / x);
		if (x > 0.f)
			return atan;
		else
			return y > 0.f ? atan + pi : atan - pi;
	}
	else {
		float atan = atanf(x / y);
		return y > 0.f ? halfpi - atan : (-halfpi) - atan;
	}
}
*/

tfxINTERNAL inline tfxWideFloat tfxWideAtan2(tfxWideFloat y, tfxWideFloat x)
{
	tfxWideFloat absxgreaterthanabsy = tfxWideGreater(tfxWideAndNot(SIGNMASK, x), tfxWideAndNot(SIGNMASK, y));
	tfxWideFloat ratio = tfxWideDiv(tfxWideAdd(tfxWideAnd(absxgreaterthanabsy, y), tfxWideAndNot(absxgreaterthanabsy, x)),
		tfxWideAdd(tfxWideAnd(absxgreaterthanabsy, x), tfxWideAndNot(absxgreaterthanabsy, y)));
	tfxWideFloat atan = tfxWideAtan(ratio);

	tfxWideFloat xgreaterthan0 = tfxWideGreater(x, tfxWideSetSingle(0.f));
	tfxWideFloat ygreaterthan0 = tfxWideGreater(y, tfxWideSetSingle(0.f));

	atan = tfxWideXOr(atan, tfxWideAndNot(absxgreaterthanabsy, SIGNMASK)); //negate atan if absx<=absy & x>0

	tfxWideFloat shift = tfxWideSetSingle(tfxPI);
	shift = tfxWideSub(shift, tfxWideAndNot(absxgreaterthanabsy, tfxWideSetSingle(tfxHALFPI))); //substract tfxHALFPI if absx<=absy
	shift = tfxWideXOr(shift, tfxWideAndNot(ygreaterthan0, SIGNMASK)); //negate shift if y<=0
	shift = tfxWideAndNot(tfxWideAnd(absxgreaterthanabsy, xgreaterthan0), shift); //null if abs>absy & x>0

	return tfxWideAdd(atan, shift);
}

inline tfxWideFloat tfxWideCos52s(tfxWideFloat x)
{
	const tfxWideFloat c1 = tfxWideSetSingle(0.9999932946f);
	const tfxWideFloat c2 = tfxWideSetSingle(-0.4999124376f);
	const tfxWideFloat c3 = tfxWideSetSingle(0.0414877472f);
	const tfxWideFloat c4 = tfxWideSetSingle(-0.0012712095f);
	tfxWideFloat x2;      // The input argument squared
	x2 = tfxWideMul(x, x);
	//               (c1+           x2*          (c2+           x2*          (c3+           c4*x2)));
	return tfxWideAdd(c1, tfxWideMul(x2, tfxWideAdd(c2, tfxWideMul(x2, tfxWideAdd(c3, tfxWideMul(c4, x2))))));
}

inline void tfxWideSinCos(tfxWideFloat angle, tfxWideFloat *sin, tfxWideFloat *cos) {
	tfxWideFloat anglesign = tfxWideOr(tfxWideSetSingle(1.f), tfxWideAnd(SIGNMASK, angle));

	//clamp to the range 0..2pi

	//take absolute value
	angle = tfxWideAndNot(SIGNMASK, angle);
	//fmod(angle,tfxPI2)
	angle = tfxWideSub(angle, tfxWideMul(tfxWideConvert(tfxWideConverti(tfxWideMul(angle, tfxWideSetSingle(tfxINVTWOPI)))), tfxWideSetSingle(tfxPI2))); //simplied SSE2 fmod, must always operate on absolute value
	//if SSE4.1 is always available, comment the line above and uncomment the line below
	//angle=tfxWideSub(angle,tfxWideMul(_mm_floor_ps(tfxWideMul(angle,tfxWideSetSingle(tfxINVTWOPI))),tfxWideSetSingle(tfxPI2))); //faster if SSE4.1 is always available

	tfxWideFloat cosangle = angle;
	cosangle = tfxWideXOr(cosangle, tfxWideAnd(tfxWideGreaterEqual(angle, tfxWideSetSingle(tfxHALFPI)), tfxWideXOr(cosangle, tfxWideSub(tfxWideSetSingle(tfxPI), angle))));
	cosangle = tfxWideXOr(cosangle, tfxWideAnd(tfxWideGreaterEqual(angle, tfxWideSetSingle(tfxPI)), SIGNMASK));
	cosangle = tfxWideXOr(cosangle, tfxWideAnd(tfxWideGreaterEqual(angle, tfxWideSetSingle(tfxTHREEHALFPI)), tfxWideXOr(cosangle, tfxWideSub(tfxWideSetSingle(tfxPI2), angle))));

	tfxWideFloat result = tfxWideCos52s(cosangle);

	result = tfxWideXOr(result, tfxWideAnd(tfxWideAnd(tfxWideGreaterEqual(angle, tfxWideSetSingle(tfxHALFPI)), tfxWideLess(angle, tfxWideSetSingle(tfxTHREEHALFPI))), SIGNMASK));
	*cos = result;

	tfxWideFloat sinmultiplier = tfxWideMul(anglesign, tfxWideOr(tfxWideSetSingle(1.f), tfxWideAnd(tfxWideGreater(angle, tfxWideSetSingle(tfxPI)), SIGNMASK)));
	*sin = tfxWideMul(sinmultiplier, tfxWideFastSqrt(tfxWideSub(tfxWideSetSingle(1.f), tfxWideMul(result, result))));
}

inline void tfxWideSinCosAdd(tfxWideFloat angle, tfxWideFloat *sin, tfxWideFloat *cos) {
	tfxWideFloat anglesign = tfxWideOr(tfxWideSetSingle(1.f), tfxWideAnd(SIGNMASK, angle));

	//clamp to the range 0..2pi

	//take absolute value
	angle = tfxWideAndNot(SIGNMASK, angle);
	//fmod(angle,tfxPI2)
	angle = tfxWideSub(angle, tfxWideMul(tfxWideConvert(tfxWideConverti(tfxWideMul(angle, tfxWideSetSingle(tfxINVTWOPI)))), tfxWideSetSingle(tfxPI2))); //simplied SSE2 fmod, must always operate on absolute value
	//if SSE4.1 is always available, comment the line above and uncomment the line below
	//angle=tfxWideSub(angle,tfxWideMul(_mm_floor_ps(tfxWideMul(angle,tfxWideSetSingle(tfxINVTWOPI))),tfxWideSetSingle(tfxPI2))); //faster if SSE4.1 is always available

	tfxWideFloat cosangle = angle;
	cosangle = tfxWideXOr(cosangle, tfxWideAnd(tfxWideGreaterEqual(angle, tfxWideSetSingle(tfxHALFPI)), tfxWideXOr(cosangle, tfxWideSub(tfxWideSetSingle(tfxPI), angle))));
	cosangle = tfxWideXOr(cosangle, tfxWideAnd(tfxWideGreaterEqual(angle, tfxWideSetSingle(tfxPI)), SIGNMASK));
	cosangle = tfxWideXOr(cosangle, tfxWideAnd(tfxWideGreaterEqual(angle, tfxWideSetSingle(tfxTHREEHALFPI)), tfxWideXOr(cosangle, tfxWideSub(tfxWideSetSingle(tfxPI2), angle))));

	tfxWideFloat result = tfxWideCos52s(cosangle);

	result = tfxWideXOr(result, tfxWideAnd(tfxWideAnd(tfxWideGreaterEqual(angle, tfxWideSetSingle(tfxHALFPI)), tfxWideLess(angle, tfxWideSetSingle(tfxTHREEHALFPI))), SIGNMASK));
	*cos = tfxWideAdd(*cos, result);

	tfxWideFloat sinmultiplier = tfxWideMul(anglesign, tfxWideOr(tfxWideSetSingle(1.f), tfxWideAnd(tfxWideGreater(angle, tfxWideSetSingle(tfxPI)), SIGNMASK)));
	*sin = tfxWideAdd(*sin, tfxWideMul(sinmultiplier, tfxWideFastSqrt(tfxWideSub(tfxWideSetSingle(1.f), tfxWideMul(result, result)))));
}
/*
End of Robin Lobel code
*/

//simd mod function thanks to Stephanie Rancourt: http://dss.stephanierct.com/DevBlog/?p=8
tfxINTERNAL inline tfxWideFloat tfxWideMod(const tfxWideFloat *a, const tfxWideFloat *aDiv) {
	tfxWideFloat c = tfxWideDiv(*a, *aDiv);
	tfxWideInt i = tfxWideConverti(c);
	tfxWideFloat cTrunc = tfxWideConvert(i);
	tfxWideFloat base = tfxWideMul(cTrunc, *aDiv);
	tfxWideFloat r = tfxWideSub(*a, base);
	return r;
}

tfxINTERNAL inline tfxWideFloat tfxWideAbs(tfxWideFloat v) {
	return tfxWideAnd(tfxWideCast(tfxWideShiftRight(tfxWideSetSinglei(-1), 1)), v);
}

tfxINTERNAL inline tfxWideInt tfxWideAbsi(tfxWideInt v) {
	return tfxWideAndi(tfxWideShiftRight(tfxWideSetSinglei(-1), 1), v);
}

//End of SIMD setup

//----------------------------------------------------------
//Section: Enums
//----------------------------------------------------------

//tfx_graph_t presets to determine limits and scales of different graphs, mainly used for the editor
enum tfx_graph_preset {
	tfxGlobalPercentPreset,
	tfxGlobalOpacityPreset,
	tfxGlobalPercentPresetSigned,
	tfxAnglePreset,
	tfxArcPreset,
	tfxEmissionRangePreset,
	tfxDimensionsPreset,
	tfxTranslationPreset,
	tfxLifePreset,
	tfxAmountPreset,
	tfxVelocityPreset,
	tfxVelocityOvertimePreset,
	tfxWeightPreset,
	tfxWeightVariationPreset,
	tfxNoiseOffsetVariationPreset,
	tfxNoiseResolutionPreset,
	tfxWeightOvertimePreset,
	tfxSpinPreset,
	tfxSpinVariationPreset,
	tfxSpinOvertimePreset,
	tfxDirectionOvertimePreset,
	tfxDirectionVariationPreset,
	tfxFrameratePreset,
	tfxVelocityTurbulancePreset,
	tfxOpacityOvertimePreset,
	tfxColorPreset,
	tfxPercentOvertime,
	tfxIntensityOvertimePreset,
	tfxPathDirectionOvertimePreset,
	tfxPathTranslationOvertimePreset,
};

enum tfx_graph_category {
	tfxGraphCategory_global,
	tfxGraphCategory_transform,
	tfxGraphCategory_property,
	tfxGraphCategory_base,
	tfxGraphCategory_variation,
	tfxGraphCategory_overtime,
	tfxGraphCategory_lifetime,
	tfxGraphCategory_spawn_rate,
	tfxGraphCategory_size,
	tfxGraphCategory_velocity,
	tfxGraphCategory_weight,
	tfxGraphCategory_spin,
	tfxGraphCategory_noise,
	tfxGraphCategory_color,
	tfxGraphCategory_max
};

typedef enum tfx_color_ramp_format {
	tfx_color_format_rgba,
	tfx_color_format_bgra
} tfx_color_ramp_format;

#define TFX_GLOBAL_COUNT     17
#define TFX_PROPERTY_COUNT   10
#define TFX_BASE_COUNT       10
#define TFX_VARIATION_COUNT  12
#define TFX_OVERTIME_COUNT   25
#define TFX_FACTOR_COUNT     4
#define TFX_TRANSFORM_COUNT  6

enum {
	TFX_GLOBAL_START = 0,
	TFX_PROPERTY_START = TFX_GLOBAL_COUNT,
	TFX_BASE_START = (TFX_PROPERTY_START + TFX_PROPERTY_COUNT),
	TFX_VARIATION_START = (TFX_BASE_START + TFX_BASE_COUNT),
	TFX_OVERTIME_START = (TFX_VARIATION_START + TFX_VARIATION_COUNT),
	TFX_FACTOR_START = (TFX_OVERTIME_START + TFX_OVERTIME_COUNT),
	TFX_TRANSFORM_START = (TFX_FACTOR_START + TFX_FACTOR_COUNT)
};

//All the different types of graphs, split into main type: global, property, base, variation and overtime
enum tfx_graph_type {
	tfxGlobal_life,
	tfxGlobal_amount,
	tfxGlobal_velocity,
	tfxGlobal_noise,
	tfxGlobal_width,
	tfxGlobal_height,
	tfxGlobal_weight,
	tfxGlobal_roll_spin,
	tfxGlobal_pitch_spin,
	tfxGlobal_yaw_spin,
	tfxGlobal_stretch,
	tfxGlobal_overal_scale,
	tfxGlobal_intensity,
	tfxGlobal_splatter,
	tfxGlobal_emitter_width,
	tfxGlobal_emitter_height,
	tfxGlobal_emitter_depth,

	tfxProperty_emission_pitch,
	tfxProperty_emission_yaw,
	tfxProperty_emission_range,
	tfxProperty_splatter,
	tfxProperty_emitter_width,        //Also used for linear extrusion for paths as well
	tfxProperty_emitter_height,
	tfxProperty_emitter_depth,
	tfxProperty_extrusion,
	tfxProperty_arc_size,
	tfxProperty_arc_offset,

	tfxBase_life,
	tfxBase_amount,
	tfxBase_velocity,
	tfxBase_width,
	tfxBase_height,
	tfxBase_weight,
	tfxBase_pitch_spin,
	tfxBase_yaw_spin,
	tfxBase_roll_spin,
	tfxBase_noise_offset,

	tfxVariation_life,
	tfxVariation_amount,
	tfxVariation_velocity,
	tfxVariation_width,
	tfxVariation_height,
	tfxVariation_weight,
	tfxVariation_pitch_spin,
	tfxVariation_yaw_spin,
	tfxVariation_roll_spin,
	tfxVariation_noise_offset,
	tfxVariation_noise_resolution,
	tfxVariation_motion_randomness,

	tfxOvertime_velocity,
	tfxOvertime_width,
	tfxOvertime_height,
	tfxOvertime_weight,
	tfxOvertime_pitch_spin,
	tfxOvertime_yaw_spin,
	tfxOvertime_roll_spin,
	tfxOvertime_stretch,
	tfxOvertime_red,
	tfxOvertime_green,
	tfxOvertime_blue,
	tfxOvertime_blendfactor,
	tfxOvertime_red_hint,
	tfxOvertime_green_hint,
	tfxOvertime_blue_hint,
	tfxOvertime_blendfactor_hint,
	tfxOvertime_velocity_turbulance,
	tfxOvertime_direction_turbulance,
	tfxOvertime_velocity_adjuster,
	tfxOvertime_intensity,
	tfxOvertime_alpha_sharpness,
	tfxOvertime_curved_alpha,
	tfxOvertime_direction,
	tfxOvertime_noise_resolution,
	tfxOvertime_motion_randomness,

	tfxFactor_life,
	tfxFactor_size,
	tfxFactor_velocity,
	tfxFactor_intensity,

	tfxTransform_roll,
	tfxTransform_pitch,
	tfxTransform_yaw,
	tfxTransform_translate_x,
	tfxTransform_translate_y,
	tfxTransform_translate_z,
	tfxEmitterGraphMaxIndex,

	tfxPath_angle_x,
	tfxPath_angle_y,
	tfxPath_angle_z,
	tfxPath_offset_x,
	tfxPath_offset_y,
	tfxPath_offset_z,
	tfxPath_distance,
	tfxPath_rotation_range,
	tfxPath_rotation_pitch,
	tfxPath_rotation_yaw,
	tfxGraphMaxIndex
};

//tfx_effect_emitter_t type - effect contains emitters, and emitters spawn particles, but they both share the same struct for simplicity
enum tfx_effect_emitter_type {
	tfxEffectType,
	tfxEmitterType,
	tfxStage,
	tfxFolder
};

//Different ways that particles can be emitted
enum tfx_emission_type {
	tfxPoint,
	tfxArea,
	tfxLine,
	tfxEllipse,
	tfxCylinder,
	tfxIcosphere,
	tfxPath,
	tfxOtherEmitter,
	tfxEmissionTypeMax,
};

enum tfx_path_extrusion_type {
	tfxExtrusionArc,
	tfxExtrusionLinear
};

//Determines how for area, line and ellipse emitters the direction that particles should travel when they spawn
enum tfx_emission_direction {
	tfxInwards,
	tfxOutwards,
	tfxBothways,
	tfxSpecified,
	tfxSurface,
	tfxOrbital,
	tfxPathGradient
};

//For line effects where traverse line is switched on
enum tfx_line_traversal_end_behaviour {
	tfxLoop,
	tfxKill,
	tfxLetFree
};

//Mainly for the editor, maybe this can just be moved there instead?
enum tfx_export_color_options {
	tfxFullColor,
	tfxOneColor,
	tfxGreyScale
};

//Mainly for the editor, maybe this can just be moved there instead?
enum tfx_export_options {
	tfxSpriteSheet,
	tfxStrip,
	tfxSeparateImages
};

//tfx_graph_t data can be looked up in one of 2 ways, either by just using linear/bezier interpolation (slower), or look up the value in a pre-compiled look up table.
enum tfx_lookup_mode {
	tfxPrecise,
	tfxFast
};

enum tfx_record_progress {
	tfxCalculateFrames,
	tfxBakeSpriteData,
	tfxLinkUpSprites,
	tfxCompressFrames,
	tfxBakingDone
};

//Used in file loading - for loading effects library
enum tfx_data_type {
	tfxString,
	tfxSInt,
	tfxUint,
	tfxFloat,
	tfxDouble,
	tfxBool,
	tfxColor,
	tfxUInt64,
	tfxFloat3,
	tfxFloat2
};

//Block designators for loading effects library and other files like animation sprite data
//The values of existing enums below must never change or older files won't load anymore!
enum tfx_effect_library_stream_context {
	tfxStartEffect = 0x00FFFF00,
	tfxEndEffect,
	tfxStartEmitter,
	tfxEndEmitter,
	tfxStartGraphs,
	tfxEndGraphs,
	tfxStartShapes,
	tfxEndShapes,
	tfxStartAnimationSettings,
	tfxEndAnimationSettings,
	tfxStartImageData,
	tfxStartEffectData,
	tfxEndOfFile,
	tfxStartFolder,
	tfxEndFolder,
	tfxStartPreviewCameraSettings,
	tfxEndPreviewCameraSettings,
	tfxStartStage,
	tfxEndStage,
	tfxStartEffectAnimationInfo,
	tfxEndEffectAnimationInfo,
	tfxStartFrameMeta,
	tfxEndFrameMeta,
	tfxStartFrameOffsets,
	tfxEndFrameOffsets,
};

typedef tfxU32 tfxEmitterPropertyFlags;         //tfx_emitter_property_flag_bits
typedef tfxU32 tfxColorRampFlags;		        //tfx_color_ramp_flag_bits
typedef tfxU32 tfxEffectPropertyFlags;          //tfx_effect_property_flag_bits
typedef tfxU32 tfxVectorFieldFlags;             //tfx_vector_field_flag_bits
typedef tfxU32 tfxParticleFlags;                //tfx_particle_flag_bits
typedef tfxU32 tfxEmitterStateFlags;            //tfx_emitter_state_flag_bits
typedef tfxU32 tfxEffectStateFlags;             //tfx_effect_state_flag_bits
typedef tfxU32 tfxParticleControlFlags;         //tfx_particle_control_flag_bits
typedef tfxU32 tfxAttributeNodeFlags;           //tfx_attribute_node_flag_bits
typedef tfxU32 tfxAngleSettingFlags;            //tfx_angle_setting_flag_bits
typedef tfxU32 tfxParticleManagerFlags;         //tfx_particle_manager_flag_bits
typedef tfxU32 tfxErrorFlags;                   //tfx_error_flag_bits
typedef tfxU32 tfxEffectCloningFlags;           //tfx_effect_cloning_flag_bits
typedef tfxU32 tfxAnimationFlags;               //tfx_animation_flag_bits
typedef tfxU32 tfxAnimationInstanceFlags;       //tfx_animation_instance_flag_bits
typedef tfxU32 tfxAnimationManagerFlags;        //tfx_animation_manager_flag_bits
typedef tfxU32 tfxEmitterPathFlags;             //tfx_emitter_path_flag_bits
typedef tfxU32 tfxEmitterControlProfileFlags;   //tfx_emitter_control_profile_flag_bits
typedef tfxU32 tfxPackageFlags;                 //tfx_package_flag_bits

enum tfx_error_flag_bits {
	tfxErrorCode_success = 0,
	tfxErrorCode_incorrect_package_format = 1 << 0,
	tfxErrorCode_data_could_not_be_loaded = 1 << 1,
	tfxErrorCode_could_not_add_shape = 1 << 2,
	tfxErrorCode_error_loading_shapes = 1 << 3,
	tfxErrorCode_some_data_not_loaded = 1 << 4,
	tfxErrorCode_unable_to_open_file = 1 << 5,
	tfxErrorCode_unable_to_read_file = 1 << 6,
	tfxErrorCode_wrong_file_size = 1 << 7,
	tfxErrorCode_invalid_format = 1 << 8,
	tfxErrorCode_no_inventory = 1 << 9,
	tfxErrorCode_invalid_inventory = 1 << 10,
	tfxErrorCode_sprite_data_is_3d_but_animation_manager_is_2d = 1 << 11,
	tfxErrorCode_sprite_data_is_2d_but_animation_manager_is_3d = 1 << 12,
	tfxErrorCode_library_loaded_without_shape_loader = 1 << 13
};

enum tfx_package_flag_bits {
	tfxPackageFlags_none = 0,
	tfxPackageFlags_loaded_from_memory = 1
};

enum tfx_effect_cloning_flag_bits {
	tfxEffectCloningFlags_none = 0,
	tfxEffectCloningFlags_keep_user_data = 1 << 0,
	tfxEffectCloningFlags_force_clone_global = 1 << 1,
	tfxEffectCloningFlags_clone_graphs = 1 << 2,
	tfxEffectCloningFlags_compile_graphs = 1 << 3
};

enum tfx_particle_manager_mode {
	tfxParticleManagerMode_unordered,
	tfxParticleManagerMode_ordered_by_age,
	tfxParticleManagerMode_ordered_by_depth,
	tfxParticleManagerMode_ordered_by_depth_guaranteed
};

enum tfx_particle_manager_setup {
	tfxParticleManagerSetup_none,
	tfxParticleManagerSetup_2d_unordered,
	tfxParticleManagerSetup_2d_ordered_by_age,
	tfxParticleManagerSetup_2d_group_sprites_by_effect,
	tfxParticleManagerSetup_3d_unordered,
	tfxParticleManagerSetup_3d_ordered_by_age,
	tfxParticleManagerSetup_3d_ordered_by_depth,
	tfxParticleManagerSetup_3d_ordered_by_depth_guaranteed,
	tfxParticleManagerSetup_3d_group_sprites_by_effect,
};

enum tfx_billboarding_option {
	tfxBillboarding_align_to_camera = 0,            //Align to Camera only
	tfxBillboarding_free_align = 1,                    //Free align
	tfxBillboarding_align_to_camera_and_vector = 2,    //Align to camera and vector
	tfxBillboarding_align_to_vector = 3,            //Align to vector
	tfxBillboarding_max = 4
};

enum tfx_emitter_control_profile_flag_bits {
	tfxEmitterControlProfile_basic = 0,
	tfxEmitterControlProfile_noise = 1 << 0,
	tfxEmitterControlProfile_orbital = 1 << 1,
	tfxEmitterControlProfile_motion_randomness = 1 << 2,
	tfxEmitterControlProfile_path = 1 << 3,
	tfxEmitterControlProfile_path_rotated_path = 1 << 4,
	tfxEmitterControlProfile_edge_traversal = 1 << 5,
	tfxEmitterControlProfile_edge_kill = 1 << 6,
	tfxEmitterControlProfile_edge_loop = 1 << 7,
	tfxEmitterControlProfile_stretch = 1 << 8
};

enum tfx_particle_manager_flag_bits {
	tfxParticleManagerFlags_none = 0,
	tfxParticleManagerFlags_disable_spawning = 1,
	tfxParticleManagerFlags_force_capture = 2,            //Unused
	tfxParticleManagerFlags_use_compute_shader = 1 << 3,
	tfxParticleManagerFlags_order_by_depth = 1 << 4,
	tfxParticleManagerFlags_guarantee_order = 1 << 5,
	tfxParticleManagerFlags_update_base_values = 1 << 6,
	tfxParticleManagerFlags_dynamic_sprite_allocation = 1 << 7,
	tfxParticleManagerFlags_3d_effects = 1 << 8,
	tfxParticleManagerFlags_unordered = 1 << 9,
	tfxParticleManagerFlags_ordered_by_age = 1 << 10,
	tfxParticleManagerFlags_update_age_only = 1 << 11,
	tfxParticleManagerFlags_single_threaded = 1 << 12,
	tfxParticleManagerFlags_double_buffer_sprites = 1 << 13,
	tfxParticleManagerFlags_recording_sprites = 1 << 14,
	tfxParticleManagerFlags_using_uids = 1 << 15,
	tfxParticleManagerFlags_2d_and_3d = 1 << 16,
	tfxParticleManagerFlags_update_bounding_boxes = 1 << 17,
	tfxParticleManagerFlags_use_effect_sprite_buffers = 1 << 18,
	tfxParticleManagerFlags_auto_order_effects = 1 << 19,
	tfxParticleManagerFlags_direct_to_staging_buffer = 1 << 20,
};

enum tfx_vector_align_type {
	tfxVectorAlignType_motion,
	tfxVectorAlignType_emission,
	tfxVectorAlignType_emitter,
	tfxVectorAlignType_max,
};

enum tfx_path_generator_type {
	tfxPathGenerator_spiral,
	tfxPathGenerator_loop,
	tfxPathGenerator_arc,
	tfxPathGenerator_s_curve,
	tfxPathGenerator_bend,
	tfxPathGenerator_free_mode_origin,
	tfxPathGenerator_free_mode_distance,
	tfxPathGenerator_max,
};

enum tfx_emitter_path_flag_bits {
	tfxPathFlags_none,
	tfxPathFlags_2d = 1 << 0,
	tfxPathFlags_mode_origin = 1 << 1,
	tfxPathFlags_mode_node = 1 << 2,
	tfxPathFlags_space_nodes_evenly = 1 << 3,
	tfxPathFlags_reverse_direction = 1 << 4,
	tfxPathFlags_rotation_range_yaw_only = 1 << 5
};

//Particle property that defines how a particle will rotate
enum tfx_angle_setting_flag_bits {
	tfxAngleSettingFlags_none = 0,                                                        //No flag
	tfxAngleSettingFlags_align_roll = 1 << 0,                                            //Align the particle with it's direction of travel in 2d
	tfxAngleSettingFlags_random_roll = 1 << 1,                                            //Chose a random angle at spawn time/state_flags
	tfxAngleSettingFlags_specify_roll = 1 << 2,                                            //Specify the angle at spawn time
	tfxAngleSettingFlags_align_with_emission = 1 << 3,                                    //Align the particle with the emission direction only
	tfxAngleSettingFlags_random_pitch = 1 << 4,                                            //3d mode allows for rotating pitch and yaw when not using billboarding (when particle always faces the camera)
	tfxAngleSettingFlags_random_yaw = 1 << 5,
	tfxAngleSettingFlags_specify_pitch = 1 << 6,
	tfxAngleSettingFlags_specify_yaw = 1 << 7
};

//All the state_flags needed by the ControlParticle function put into one enum to save space
enum tfx_particle_control_flag_bits {
	tfxParticleControlFlags_none = 0,
	tfxParticleControlFlags_random_color = 1 << 0,
	tfxParticleControlFlags_relative_position = 1 << 1,
	tfxParticleControlFlags_relative_angle = 1 << 2,
	tfxParticleControlFlags_point = 1 << 3,
	tfxParticleControlFlags_area = 1 << 4,
	tfxParticleControlFlags_line = 1 << 5,
	tfxParticleControlFlags_ellipse = 1 << 6,
	tfxParticleControlFlags_loop = 1 << 7,
	tfxParticleControlFlags_kill = 1 << 8,
	tfxParticleControlFlags_letFree = 1 << 9,
	tfxParticleControlFlags_edge_traversal = 1 << 10,
	tfxParticleControlFlags_remove = 1 << 11,
	tfxParticleControlFlags_base_uniform_size = 1 << 12,
	tfxParticleControlFlags_lifetime_uniform_size = 1 << 13,
	tfxParticleControlFlags_animate = 1 << 14,
	tfxParticleControlFlags_reverse_animation = 1 << 15,
	tfxParticleControlFlags_play_once = 1 << 16,
	tfxParticleControlFlags_align = 1 << 17,
	tfxParticleControlFlags_emission = 1 << 18,
	tfxParticleControlFlags_random_roll = 1 << 19,
	tfxParticleControlFlags_specify_roll = 1 << 20,
	tfxParticleControlFlags_random_pitch = 1 << 21,
	tfxParticleControlFlags_specify_pitch = 1 << 22,
	tfxParticleControlFlags_random_yaw = 1 << 23,
	tfxParticleControlFlags_specify_yaw = 1 << 24,
};

enum tfx_effect_property_flag_bits {
	tfxEffectPropertyFlags_none = 0,
	tfxEffectPropertyFlags_depth_draw_order = 1 << 1,
	tfxEffectPropertyFlags_guaranteed_order = 1 << 2,
	tfxEffectPropertyFlags_age_order = 1 << 3,
	tfxEffectPropertyFlags_use_keyframes = 1 << 4,
	tfxEffectPropertyFlags_include_in_sprite_data_export = 1 << 5,        //In the editor you can specify which effects you want to be included in a spritedata export
	tfxEffectPropertyFlags_global_uniform_size = 1 << 6,                //Keep the global particle size uniform
};

enum tfx_emitter_property_flag_bits {
	tfxEmitterPropertyFlags_none = 0,
	tfxEmitterPropertyFlags_random_color = 1 << 0,                      //Pick a random color from the color overtime gradient rather then change the color over the lifetime of the particle
	tfxEmitterPropertyFlags_relative_position = 1 << 1,                 //Keep the particles position relative to the current position of the emitter
	tfxEmitterPropertyFlags_relative_angle = 1 << 2,                    //Keep the angle of the particles relative to the current angle of the emitter
	tfxEmitterPropertyFlags_image_handle_auto_center = 1 << 3,          //Set the offset of the particle to the center of the image
	tfxEmitterPropertyFlags_single = 1 << 4,                            //Only spawn a single particle (or number of particles specified by spawn_amount) that does not expire
	tfxEmitterPropertyFlags_spawn_on_grid = 1 << 5,                     //When using an area, line or ellipse emitter, spawn along a grid
	tfxEmitterPropertyFlags_grid_spawn_clockwise = 1 << 6,              //Spawn clockwise/left to right around the area
	tfxEmitterPropertyFlags_fill_area = 1 << 7,                         //Fill the area
	tfxEmitterPropertyFlags_emitter_handle_auto_center = 1 << 8,        //Center the handle of the emitter
	tfxEmitterPropertyFlags_edge_traversal = 1 << 9,                    //Line and Path emitters only: make particles traverse the line/path
	tfxEmitterPropertyFlags_base_uniform_size = 1 << 10,                //Keep the base particle size uniform
	tfxEmitterPropertyFlags_lifetime_uniform_size = 1 << 11,            //Keep the size over lifetime of the particle uniform
	tfxEmitterPropertyFlags_animate = 1 << 12,                          //Animate the particle shape if it has more than one frame of animation
	tfxEmitterPropertyFlags_reverse_animation = 1 << 13,                //Make the image animation go in reverse
	tfxEmitterPropertyFlags_play_once = 1 << 14,                        //Play the animation once only
	tfxEmitterPropertyFlags_random_start_frame = 1 << 15,               //Start the animation of the image from a random frame
	tfxEmitterPropertyFlags_wrap_single_sprite = 1 << 16,               //When recording sprite data, single particles can have their invalid capured index set to the current frame for better looping
	tfxEmitterPropertyFlags_is_in_folder = 1 << 17,                     //This effect is located inside a folder. Todo: move this to effect properties
	tfxEmitterPropertyFlags_use_spawn_ratio = 1 << 18,                  //Option for area emitters to multiply the amount spawned by a ration of particles per pixels squared
	tfxEmitterPropertyFlags_effect_is_3d = 1 << 19,                     //Makes the effect run in 3d mode for 3d effects todo: does this need to be here, the effect dictates this?
	tfxEmitterPropertyFlags_grid_spawn_random = 1 << 20,                //Spawn on grid points but randomly rather then in sequence
	tfxEmitterPropertyFlags_area_open_ends = 1 << 21,                   //Only sides of the area/cylinder are spawned on when fill area is not checked
	tfxEmitterPropertyFlags_exclude_from_hue_adjustments = 1 << 22,     //Emitter will be excluded from effect hue adjustments if this flag is checked
	tfxEmitterPropertyFlags_enabled = 1 << 23,                          //The emitter is enabled or not, meaning it will or will not be added the particle manager with tfx__add_effect
	tfxEmitterPropertyFlags_match_amount_to_grid_points = 1 << 24,      //Match the amount to spawn with a single emitter to the number of grid points in the effect
	tfxEmitterPropertyFlags_use_path_for_direction = 1 << 25,           //Make the particles use a path to dictate their direction of travel
	tfxEmitterPropertyFlags_alt_velocity_lifetime_sampling = 1 << 26,	//The point on the path dictates where on the velocity overtime graph that the particle should sample from rather then the age of the particle
	tfxEmitterPropertyFlags_alt_color_lifetime_sampling = 1 << 27,      //The point on the path dictates where on the color overtime graph that the particle should sample from rather then the age of the particle
	tfxEmitterPropertyFlags_alt_size_lifetime_sampling = 1 << 28,       //The point on the path dictates where on the size overtime graph that the particle should sample from rather then the age of the particle
	tfxEmitterPropertyFlags_use_simple_motion_randomness = 1 << 29,     //Use a simplified way to generate random particle movement which is much less computationally intensive than simplex noise
	tfxEmitterPropertyFlags_spawn_location_source = 1 << 30,            //This emitter is the source for another emitter that uses it to spawn particles at the location of this emitters' particles
	tfxEmitterPropertyFlags_use_color_hint = 1 << 31	                //Activate a second color to tint the particles and mix between the two colors.
};

enum tfx_color_ramp_flag_bits {
	tfxColorRampFlags_none = 0,
	tfxColorRampFlags_use_sinusoidal_ramp_generation = 1 << 0,			//Use this flag to toggle between sinusoidal color ramp generation
};

enum tfx_particle_flag_bits {
	tfxParticleFlags_none = 0,
	tfxParticleFlags_fresh = 1 << 0,                                    //Particle has just spawned this frame    
	tfxParticleFlags_remove = 1 << 4,                                   //Particle will be removed this or next frame
	tfxParticleFlags_has_velocity = 1 << 5,                             //Flagged if the particle is currently moving
	tfxParticleFlags_has_sub_effects = 1 << 6,                          //Flagged if the particle has sub effects
	tfxParticleFlags_capture_after_transform = 1 << 15,                 //Particle will be captured after a transfrom, used for traversing lines and looping back to the beginning to avoid lerping imbetween
};

enum tfx_emitter_state_flag_bits {
	tfxEmitterStateFlags_none = 0,
	tfxEmitterStateFlags_random_color = 1 << 0,
	tfxEmitterStateFlags_relative_position = 1 << 1,                    //Keep the particles position relative to the current position of the emitter
	tfxEmitterStateFlags_relative_angle = 1 << 2,                       //Keep the angle of the particles relative to the current angle of the emitter
	tfxEmitterStateFlags_stop_spawning = 1 << 3,                        //Tells the emitter to stop spawning
	tfxEmitterStateFlags_remove = 1 << 4,                               //Tells the effect/emitter to remove itself from the particle manager immediately
	tfxEmitterStateFlags_unused1 = 1 << 5,                              //the emitter is enabled. **moved to property state_flags**
	tfxEmitterStateFlags_retain_matrix = 1 << 6,                        //Internal flag about matrix usage
	tfxEmitterStateFlags_no_tween_this_update = 1 << 7,                 //Internal flag generally, but you could use it if you want to teleport the effect to another location
	tfxEmitterStateFlags_is_single = 1 << 8,
	tfxEmitterStateFlags_not_line = 1 << 9,
	tfxEmitterStateFlags_base_uniform_size = 1 << 10,
	tfxEmitterStateFlags_lifetime_uniform_size = 1 << 11,               //Keep the size over lifetime of the particle uniform
	tfxEmitterStateFlags_can_spin = 1 << 12,
	tfxEmitterStateFlags_is_edge_traversal = 1 << 13,
	tfxEmitterStateFlags_play_once = 1 << 14,                           //Play the animation once only
	tfxEmitterStateFlags_loop = 1 << 15,
	tfxEmitterStateFlags_kill = 1 << 16,
	tfxEmitterStateFlags_single_shot_done = 1 << 17,
	tfxEmitterStateFlags_is_line_loop_or_kill = 1 << 18,
	tfxEmitterStateFlags_is_area = 1 << 19,
	tfxEmitterStateFlags_no_tween = 1 << 20,
	tfxEmitterStateFlags_align_with_velocity = 1 << 21,
	tfxEmitterStateFlags_is_sub_emitter = 1 << 22,
	tfxEmitterStateFlags_unused2 = 1 << 23,
	tfxEmitterStateFlags_can_spin_pitch_and_yaw = 1 << 24,              //For 3d emitters that have free alignment and not always facing the camera
	tfxEmitterStateFlags_has_path = 1 << 25,
	tfxEmitterStateFlags_is_bottom_emitter = 1 << 26,                   //This emitter has no child effects, so can spawn particles that could be used in a compute shader if it's enabled
	tfxEmitterStateFlags_has_rotated_path = 1 << 27,
	tfxEmitterStateFlags_max_active_paths_reached = 1 << 28,
	tfxEmitterStateFlags_is_in_ordered_effect = 1 << 29,
	tfxEmitterStateFlags_wrap_single_sprite = 1 << 30
};

enum tfx_effect_state_flag_bits {
	tfxEffectStateFlags_none = 0,
	tfxEffectStateFlags_stop_spawning = 1 << 3,                            //Tells the emitter to stop spawning
	tfxEffectStateFlags_remove = 1 << 4,                                //Tells the effect/emitter to remove itself from the particle manager immediately
	tfxEffectStateFlags_retain_matrix = 1 << 6,                            //Internal flag about matrix usage
	tfxEffectStateFlags_no_tween_this_update = 1 << 7,                    //Internal flag generally, but you could use it if you want to teleport the effect to another location
	tfxEffectStateFlags_override_overal_scale = 1 << 8,                    //Flagged when the over scale is overridden with tfx_SetEffectOveralScale
	tfxEffectStateFlags_override_orientiation = 1 << 9,                    //Flagged when any of the effect angles are overridden
	tfxEffectStateFlags_override_size_multiplier = 1 << 10,                //Flagged when any of the effect size multipliers are overridden
	tfxEffectStateFlags_no_tween = 1 << 20
};

enum tfx_vector_field_flag_bits {
	tfxVectorFieldFlags_none = 0,
	tfxVectorFieldFlags_repeat_horizontal = 1 << 0,                        //Field will repeat horizontally
	tfxVectorFieldFlags_repeat_vertical = 1 << 1                        //Field will repeat vertically
};

enum tfx_attribute_node_flag_bits {
	tfxAttributeNodeFlags_none = 0,
	tfxAttributeNodeFlags_is_curve = 1 << 0,
	tfxAttributeNodeFlags_is_left_curve = 1 << 1,
	tfxAttributeNodeFlags_is_right_curve = 1 << 2,
	tfxAttributeNodeFlags_curves_initialised = 1 << 3,
	tfxAttributeNodeFlags_path_node_accumulate = 1 << 4,
};

enum tfx_animation_flag_bits {
	tfxAnimationFlags_none = 0,
	tfxAnimationFlags_loop = 1 << 0,
	tfxAnimationFlags_seamless = 1 << 1,
	tfxAnimationFlags_needs_recording = 1 << 2,
	tfxAnimationFlags_export_with_transparency = 1 << 3,
	tfxAnimationFlags_auto_set_length = 1 << 4,
	tfxAnimationFlags_orthographic = 1 << 5
};

enum tfx_animation_instance_flag_bits {
	tfxAnimationInstanceFlags_none = 0,
	tfxAnimationInstanceFlags_loop = 1 << 0,
};

enum tfx_animation_manager_flag_bits {
	tfxAnimationManagerFlags_none = 0,
	tfxAnimationManagerFlags_has_animated_shapes = 1 << 0,
	tfxAnimationManagerFlags_initialised = 1 << 1,
	tfxAnimationManagerFlags_is_3d = 1 << 2,
};

//-----------------------------------------------------------
//Constants
//-----------------------------------------------------------

const float tfxMIN_FLOAT = -2147483648.f;
const float tfxMAX_FLOAT = 2147483647.f;
const tfxU32 tfxMAX_UINT = 4294967295;
const int tfxMAX_INT = INT_MAX;
const int tfxMIN_INT = INT_MIN;
const tfxS64 tfxMAX_64i = LLONG_MAX;
const tfxS64 tfxMIN_64i = LLONG_MIN;
const tfxU64 tfxMAX_64u = ULLONG_MAX;
const tfxWideFloat tfx180RadiansWide = tfxWideSetConst(3.14159f);
const float tfxGAMMA = 1.f;
#if defined(__x86_64__) || defined(_M_X64)
typedef tfxU64 tfxAddress;
#else
typedef tfxU32 tfxAddress;
#endif
#define tfxSPRITE_SIZE_SSCALE	0.25000762962736f
#define tfxSPRITE_HANDLE_SSCALE 0.00390636921292f

//These constants are the min an max levels for the emitter attribute graphs
const float tfxLIFE_MIN = 0.f;
const float tfxLIFE_MAX = 100000.f;
const float tfxLIFE_STEPS = 200.f;

const float tfxAMOUNT_MIN = 0.f;
const float tfxAMOUNT_MAX = 5000.f;
const float tfxAMOUNT_STEPS = 100.f;

const float tfxGLOBAL_PERCENT_MIN = 0.f;
const float tfxGLOBAL_PERCENT_MAX = 20.f;
const float tfxGLOBAL_PERCENT_STEPS = 100.f;

const float tfxGLOBAL_PERCENT_V_MIN = 0.f;
const float tfxGLOBAL_PERCENT_V_MAX = 10.f;
const float tfxGLOBAL_PERCENT_V_STEPS = 200.f;

const float tfxINTENSITY_MIN = 0.f;
const float tfxINTENSITY_MAX = 5.f;
const float tfxINTENSITY_STEPS = 100.f;

const float tfxANGLE_MIN = 0.f;
const float tfxANGLE_MAX = 1080.f;
const float tfxANGLE_STEPS = 54.f;

const float tfxARC_MIN = 0.f;
const float tfxARC_MAX = 6.28319f;
const float tfxARC_STEPS = .3141595f;

const float tfxEMISSION_RANGE_MIN = 0.f;
const float tfxEMISSION_RANGE_MAX = 180.f;
const float tfxEMISSION_RANGE_STEPS = 30.f;

const float tfxDIMENSIONS_MIN = 0.f;
const float tfxDIMENSIONS_MAX = 4000.f;
const float tfxDIMENSIONS_STEPS = 40.f;

const float tfxVELOCITY_MIN = 0.f;
const float tfxVELOCITY_MAX = 10000.f;
const float tfxVELOCITY_STEPS = 100.f;

const float tfxVELOCITY_OVERTIME_MIN = -20.f;
const float tfxVELOCITY_OVERTIME_MAX = 20.f;
const float tfxVELOCITY_OVERTIME_STEPS = 200.f;

const float tfxWEIGHT_MIN = -2500.f;
const float tfxWEIGHT_MAX = 2500.f;
const float tfxWEIGHT_STEPS = 200.f;

const float tfxWEIGHT_VARIATION_MIN = 0.f;
const float tfxWEIGHT_VARIATION_MAX = 2500.f;
const float tfxWEIGHT_VARIATION_STEPS = 250.f;

const float tfxSPIN_MIN = -2000.f;
const float tfxSPIN_MAX = 2000.f;
const float tfxSPIN_STEPS = 100.f;

const float tfxSPIN_VARIATION_MIN = 0.f;
const float tfxSPIN_VARIATION_MAX = 2000.f;
const float tfxSPIN_VARIATION_STEPS = 100.f;

const float tfxSPIN_OVERTIME_MIN = -20.f;
const float tfxSPIN_OVERTIME_MAX = 20.f;
const float tfxSPIN_OVERTIME_STEPS = 200.f;

const float tfxDIRECTION_OVERTIME_MIN = 0.f;
const float tfxDIRECTION_OVERTIME_MAX = 4320.f;
const float tfxDIRECTION_OVERTIME_STEPS = 216.f;

const float tfxFRAMERATE_MIN = 0.f;
const float tfxFRAMERATE_MAX = 200.f;
const float tfxFRAMERATE_STEPS = 100.f;

const float tfxMAX_DIRECTION_VARIATION = 22.5f;
const float tfxMAX_VELOCITY_VARIATION = 30.f;
const int tfxMOTION_VARIATION_INTERVAL = 30;

//Look up frequency determines the resolution of graphs that are compiled into look up arrays.
static float tfxLOOKUP_FREQUENCY = 10.f;
//Overtime frequency is for lookups that will vary in length depending on the lifetime of the particle. It should generally be a higher resolution than the base graphs
static float tfxLOOKUP_FREQUENCY_OVERTIME = 1.f;

//Look up frequency determines the resolution of graphs that are compiled into look up arrays.
static tfxWideFloat tfxLOOKUP_FREQUENCY_WIDE = tfxWideSetConst(10.f);
//Overtime frequency is for lookups that will vary in length depending on the lifetime of the particle. It should generally be a higher resolution than the base graphs
//Experiment with lower resolution and use interpolation instead? Could be a lot better on the cache.
static tfxWideFloat tfxLOOKUP_FREQUENCY_OVERTIME_WIDE = tfxWideSetConst(1.f);

//-----------------------------------------------------------
//Section: forward_declarations
//-----------------------------------------------------------
#define tfxMAKE_HANDLE(handle) typedef struct handle##_t* handle;

//For allocating a new object with handle. Only used internally.
#define tfxNEW(type) (type)tfxALLOCATE(sizeof(type##_t))
#define tfxNEW_ALIGNED(type, alignment) (type)tfxALLOCATE_ALIGNED(sizeof(type##_t), alignment)

typedef struct tfx_stream_s tfx_stream_t;
typedef struct tfx_package_s tfx_package_t;

tfxMAKE_HANDLE(tfx_stream)
tfxMAKE_HANDLE(tfx_package)

//-----------------------------------------------------------
//Section: String_Buffers
//-----------------------------------------------------------

int tfx_FormatString(char *buf, size_t buf_size, const char *fmt, va_list args);

#ifdef __cplusplus

//Very simple string builder for short stack based strings
template<size_t N>
struct tfx_str_t {
	char data[N];
	static constexpr tfxU32 capacity = N;
	tfxU32 current_size;

	inline tfx_str_t() : current_size(0) { memset(data, '\0', N); }
	inline tfx_str_t(const char *text) : current_size(0) {
		size_t text_len = strlen(text);
		TFX_ASSERT(text_len < capacity); //String is too big for the buffer size allowed
		memset(data, '\0', N);
		tfx__strcpy(data, N, text);
		current_size = text_len + 1;
	}

	inline bool            empty() { return current_size == 0; }
	inline char &operator[](tfxU32 i) { TFX_ASSERT(i < current_size); return data[i]; }
	inline const char &operator[](tfxU32 i) const { TFX_ASSERT(i < current_size); return data[i]; }

	inline void         Clear() { current_size = 0; memset(data, '\0', N); }
	inline char *begin() { return data; }
	inline const char *begin() const { return data; }
	inline char *end() { return data + current_size; }
	inline const char *end() const { return data + current_size; }
	inline char			back() { TFX_ASSERT(current_size > 0); return data[current_size - 1]; }
	inline const char	back() const { TFX_ASSERT(current_size > 0); return data[current_size - 1]; }
	inline void         pop() { TFX_ASSERT(current_size > 0); current_size--; }
	inline void         push_back(const char v) { TFX_ASSERT(current_size < capacity); data[current_size] = char(v); current_size++; }

	inline bool operator==(const char *string) { return !strcmp(string, data); }
	inline bool operator==(const tfx_str_t string) { return !strcmp(data, string.c_str()); }
	inline bool operator!=(const char *string) { return strcmp(string, data); }
	inline bool operator!=(const tfx_str_t string) { return strcmp(data, string.c_str()); }
	inline const char *c_str() const { return data; }
	int Find(const char *needle) {
		tfx_str_t compare = needle;
		tfx_str_t lower = Lower();
		compare = compare.Lower();
		if (compare.Length() > Length()) return -1;
		tfxU32 pos = 0;
		while (compare.Length() + pos <= Length()) {
			if (strncmp(lower.data + pos, compare.data, compare.Length()) == 0) {
				return pos;
			}
			++pos;
		}
		return -1;
	}
	tfx_str_t Lower() {
		tfx_str_t convert = *this;
		for (auto &c : convert) {
			c = tolower(c);
		}
		return convert;
	}
	inline tfxU32 Length() const { return current_size ? current_size - 1 : 0; }
	void AddLine(const char *format, ...) {
		if (!current_size) {
			NullTerminate();
		}
		va_list args;
		va_start(args, format);
		Appendv(format, args);
		va_end(args);
		Append('\n');
	}
	void Set(const char *text) { size_t len = strlen(text); TFX_ASSERT(len < capacity - 1); tfx__strcpy(data, N, text); current_size = (tfxU32)len; NullTerminate(); };
	void Setf(const char *format, ...) {
		Clear();
		va_list args;
		va_start(args, format);

		va_list args_copy;
		va_copy(args_copy, args);

		int len = tfx_FormatString(NULL, 0, format, args);         // FIXME-OPT: could do a first pass write attempt, likely successful on first pass.
		if (len <= 0)
		{
			va_end(args_copy);
			return;
		}

		const int write_off = (current_size != 0) ? current_size : 1;
		const int needed_sz = write_off + len;
		TFX_ASSERT(write_off + (tfxU32)len < capacity);	//Trying to write outside of buffer space, string too small!
		tfx_FormatString(&data[write_off - 1], (size_t)len + 1, format, args_copy);
		va_end(args_copy);

		va_end(args);
		current_size = len + 1;
	}
	void Appendf(const char *format, ...) {
		va_list args;
		va_start(args, format);

		va_list args_copy;
		va_copy(args_copy, args);

		int len = tfx_FormatString(NULL, 0, format, args);         // FIXME-OPT: could do a first pass write attempt, likely successful on first pass.
		if (len <= 0)
		{
			va_end(args_copy);
			return;
		}

		const int write_off = (current_size != 0) ? current_size : 1;
		const int needed_sz = write_off + len;
		TFX_ASSERT(write_off + (tfxU32)len < capacity);	//Trying to write outside of buffer space, string too small!
		tfx_FormatString(&data[write_off - 1], (size_t)len + 1, format, args_copy);
		va_end(args_copy);

		va_end(args);
		current_size += len;
	}
	void Appendv(const char *format, va_list args) {
		va_list args_copy;
		va_copy(args_copy, args);

		int len = tfx_FormatString(NULL, 0, format, args);         // FIXME-OPT: could do a first pass write attempt, likely successful on first pass.
		if (len <= 0)
		{
			va_end(args_copy);
			return;
		}

		const int write_off = (current_size != 0) ? current_size : 1;
		const int needed_sz = write_off + len;
		TFX_ASSERT(write_off + (tfxU32)len < capacity);	//Trying to write outside of buffer space, string too small!
		tfx_FormatString(&data[write_off - 1], (size_t)len + 1, format, args_copy);

		va_end(args_copy);
		current_size += len;
	}
	inline void Append(char c) { if (current_size) { pop(); } push_back(c); NullTerminate(); }
	inline void Pop() { if (!Length()) return; if (back() == 0) pop(); pop(); NullTerminate(); }
	inline void Trim(char c = ' ') { if (!Length()) return; if (back() == 0) pop(); while (back() == c && current_size) { pop(); } NullTerminate(); }
	inline void TrimToZero() { if (current_size < capacity) { memset(data + current_size, '\0', capacity - current_size); } }
	inline void TrimFront(char c = ' ') { if (!Length()) return; tfxU32 pos = 0; while (data[pos] == c && pos < current_size) { pos++; } if (pos < current_size) { memcpy(data, data + pos, current_size - pos); } current_size -= pos; }
	inline void SanitizeLineFeeds() { if (current_size > 1) { while (current_size > 1 && back() == '\n' || back() == '\r' || back() == '\0') { pop(); if (current_size <= 1) { break; } } NullTerminate(); } }
	inline void NullTerminate() { push_back('\0'); }
	inline const bool IsInt() {
		if (!Length()) return false;
		for (auto c : *this) {
			if (c >= '0' && c <= '9');
			else {
				return false;
			}
		}
		return true;
	}
	inline const bool IsFloat() {
		if (!Length()) return false;
		int dot_count = 0;
		for (auto c : *this) {
			if (c >= '0' && c <= '9');
			else if (c == '.' && dot_count == 0) {
				dot_count++;
			} else {
				return false;
			}
		}
		return dot_count == 1;
	}
} TFX_PACKED_STRUCT;

#define tfx_str16_t tfx_str_t<16>
#define tfx_str32_t tfx_str_t<32>
#define tfx_str64_t tfx_str_t<64>
#define tfx_str128_t tfx_str_t<128>
#define tfx_str256_t tfx_str_t<256>
#define tfx_str512_t tfx_str_t<512>
#define tfx_str1024_t tfx_str_t<1024>

//-----------------------------------------------------------
//Containers_and_Memory
//-----------------------------------------------------------

//Storage
//Credit to ocornut https://github.com/ocornut/imgui/commits?author=ocornut for tfxvec although it's quite a lot different now.
//std::vector replacement with some extra stuff and tweaks specific to TimelineFX
template<typename T>
struct tfx_vector_t {
	tfxU32 current_size;
	tfxU32 capacity;
	tfxU32 volatile locked;
	tfxU32 alignment;
	T *data;

	// Provide standard typedefs but we don't use them ourselves.
	typedef T value_type;
	typedef value_type *iterator;
	typedef const value_type *const_iterator;

	inline					tfx_vector_t(const tfx_vector_t<T> &src) { locked = false; current_size = capacity = alignment = 0; data = nullptr; resize(src.current_size); memcpy(data, src.data, (size_t)current_size * sizeof(T)); }
	inline					tfx_vector_t() : locked(0), current_size(0), capacity(0), alignment(0), data(nullptr) {}
	inline					tfx_vector_t<T> &operator=(const tfx_vector_t<T> &src) { TFX_ASSERT(0); return *this; }	//Use copy instead. 
	inline					~tfx_vector_t() { TFX_ASSERT(data == nullptr); } //You must manually free containers!

	inline void				init() { locked = false; current_size = capacity = alignment = 0; data = nullptr; }
	inline bool				empty() { return current_size == 0; }
	inline bool				full() { return current_size == capacity; }
	inline tfxU32			size() { return current_size; }
	inline const tfxU32		size() const { return current_size; }
	inline tfxU32			size_in_bytes() { return current_size * sizeof(T); }
	inline const tfxU32		size_in_bytes() const { return current_size * sizeof(T); }
	inline T &operator[](tfxU32 i) { TFX_ASSERT(i < current_size); return data[i]; }
	inline const T &operator[](tfxU32 i) const { TFX_ASSERT(i < current_size); return data[i]; }
	inline T &ts_at(tfxU32 i) { while (locked > 0); return data[i]; }

	inline void				free() { if (data) { current_size = capacity = 0; tfxFREE(data); data = nullptr; } }
	inline void				clear() { if (data) { current_size = 0; } }
	inline T *begin() { return data; }
	inline const T *begin() const { return data; }
	inline T *end() { return data + current_size; }
	inline const T *end() const { return data + current_size; }
	inline T *rend() { return data; }
	inline const T *rend() const { return data; }
	inline T *rbegin() { return data + current_size; }
	inline const T *rbegin() const { return data + current_size; }
	inline T &front() { TFX_ASSERT(current_size > 0); return data[0]; }
	inline const T &front() const { TFX_ASSERT(current_size > 0); return data[0]; }
	inline T &back() { TFX_ASSERT(current_size > 0); return data[current_size - 1]; }
	inline const T &back() const { TFX_ASSERT(current_size > 0); return data[current_size - 1]; }
	inline T &parent() { TFX_ASSERT(current_size > 1); return data[current_size - 2]; }
	inline const T &parent() const { TFX_ASSERT(current_size > 1); return data[current_size - 2]; }
	inline void				copy(const tfx_vector_t<T> &src) { clear(); resize(src.current_size); memcpy(data, src.data, (size_t)current_size * sizeof(T)); }
	inline tfxU32			_grow_capacity(tfxU32 sz) const { tfxU32 new_capacity = capacity ? (capacity + capacity / 2) : 8; return new_capacity > sz ? new_capacity : sz; }
	inline void				resize(tfxU32 new_size) { if (new_size > capacity) reserve(_grow_capacity(new_size)); current_size = new_size; }
	inline void				resize_bytes(tfxU32 new_size) { if (new_size > capacity) reserve(_grow_capacity(new_size)); current_size = new_size; }
	inline void				resize(tfxU32 new_size, const T &v) { if (new_size > capacity) reserve(_grow_capacity(new_size)); if (new_size > current_size) for (tfxU32 n = current_size; n < new_size; n++) memcpy(&data[n], &v, sizeof(v)); current_size = new_size; }
	inline void				shrink(tfxU32 new_size) { TFX_ASSERT(new_size <= current_size); current_size = new_size; }
	inline void				set_alignment(tfxU32 align_to) { TFX_ASSERT(0 == (align_to & (align_to - 1)) && "must align to a power of two"); alignment = align_to; }
	inline void				reserve(tfxU32 new_capacity) {
		if (new_capacity <= capacity)
			return;
		T *new_data;
		if (alignment != 0) {
			new_data = (T *)tfxALLOCATE_ALIGNED((size_t)new_capacity * sizeof(T), alignment);
		} else {
			new_data = (T *)tfxALLOCATE((size_t)new_capacity * sizeof(T));
		}
		TFX_ASSERT(new_data);    //Unable to allocate memory. todo: better handling
		if (data) {
			memcpy(new_data, data, (size_t)current_size * sizeof(T));
			tfxFREE(data);
		}
		data = new_data;
		capacity = new_capacity;
	}

	inline T &grab() {
		if (current_size == capacity) reserve(_grow_capacity(current_size + 1));
		current_size++;
		return data[current_size - 1];
	}
	inline tfxU32        locked_push_back(const T &v) {
		//suspect, just use a mutex instead?
		while (tfx__compare_and_exchange((tfxLONG volatile *)&locked, 1, 0) > 1);
		if (current_size == capacity) {
			reserve(_grow_capacity(current_size + 1));
		}
		memcpy(&data[current_size], &v, sizeof(T));
		tfxU32 index = current_size++;
		tfx__exchange((tfxLONG volatile *)&locked, 0);
		return index;
	}
	inline T &push_back(const T &v) {
		if (current_size == capacity) {
			reserve(_grow_capacity(current_size + 1));
		}
		memcpy(&data[current_size], &v, sizeof(T));
		current_size++; return data[current_size - 1];
	}
	inline T &push_back_copy(const T &v) {
		if (current_size == capacity) {
			reserve(_grow_capacity(current_size + 1));
		}
		memcpy(&data[current_size], &v, sizeof(v));
		current_size++; return data[current_size - 1];
	}
	inline T &next() {
		return push_back(T());
	}
	inline void				zero() { TFX_ASSERT(capacity > 0); memset(data, 0, capacity * sizeof(T)); }
	inline void				pop() { TFX_ASSERT(current_size > 0); current_size--; }
	inline T &pop_back() { TFX_ASSERT(current_size > 0); current_size--; return data[current_size]; }
	inline void				push_front(const T &v) { if (current_size == 0) push_back(v); else insert(data, v); }
	inline T *erase(const T *it) { TFX_ASSERT(it >= data && it < data + current_size); const ptrdiff_t off = it - data; memmove(data + off, data + off + 1, ((size_t)current_size - (size_t)off - 1) * sizeof(T)); current_size--; return data + off; }
	inline T				pop_front() { TFX_ASSERT(current_size > 0); T front = data[0]; erase(data); return front; }
	inline T *erase(const T *it, const T *it_last) { TFX_ASSERT(it >= data && it < data + current_size && it_last > it && it_last <= data + current_size); const ptrdiff_t count = it_last - it; const ptrdiff_t off = it - data; memmove(data + off, data + off + count, ((size_t)current_size - (size_t)off - count) * sizeof(T)); current_size -= (tfxU32)count; return data + off; }
	inline T *erase_unsorted(const T *it) { TFX_ASSERT(it >= data && it < data + current_size);  const ptrdiff_t off = it - data; if (it < data + current_size - 1) memcpy(data + off, data + current_size - 1, sizeof(T)); current_size--; return data + off; }
	inline T *insert(const T *it, const T &v) { TFX_ASSERT(it >= data && it <= data + current_size); const ptrdiff_t off = it - data; if (current_size == capacity) reserve(_grow_capacity(current_size + 1)); if (off < (ptrdiff_t)current_size) memmove(data + off + 1, data + off, ((size_t)current_size - (size_t)off) * sizeof(T)); memcpy(data + off, &v, sizeof(T)); current_size++; return data + off; }
	inline T *insert_after(const T *it, const T &v) { TFX_ASSERT(it >= data && it <= data + current_size); const ptrdiff_t off = (it + 1) - data; if (current_size == capacity) reserve(_grow_capacity(current_size + 1)); if (off < (ptrdiff_t)current_size) memmove(data + off + 1, data + off, ((size_t)current_size - (size_t)off) * sizeof(T)); memcpy(data + off, &v, sizeof(T)); current_size++; return data + off; }
	inline bool				contains(const T &v) const { const T *_data = data;  const T *data_end = data + current_size; while (_data < data_end) if (*_data++ == v) return true; return false; }
	inline T *find(const T &v) { T *_data = data;  const T *data_end = data + current_size; while (_data < data_end) if (*_data == v) break; else ++_data; return _data; }
	inline const T *find(const T &v) const { const T *_data = data;  const T *data_end = data + current_size; while (_data < data_end) if (*_data == v) break; else ++_data; return _data; }
	inline bool				find_erase(const T &v) { const T *it = find(v); if (it < data + current_size) { erase(it); return true; } return false; }
	inline bool				find_erase_unsorted(const T &v) { const T *it = find(v); if (it < data + current_size) { erase_unsorted(it); return true; } return false; }
	inline tfxU32			index_from_ptr(const T *it) const { TFX_ASSERT(it >= data && it < data + current_size); const ptrdiff_t off = it - data; return (tfxU32)off; }
};

#define tfxCastBuffer(type, buffer) static_cast<type*>(buffer->data)
#define tfxCastBufferRef(type, buffer) static_cast<type*>(buffer.data)

//This simple container struct was created for storing instance_data in the particle manager. I didn't want this templated because either 2d or 3d instance_data could be used so
//I wanted to cast as needed when writing and using the sprite data. See simple cast macros above tfxCastBuffer and tfxCastBufferRef
struct tfx_buffer_t {
	tfxU32 current_size;
	tfxU32 capacity;
	tfxU32 struct_size;
	tfxU32 alignment;
	void *data;

	inline				tfx_buffer_t() { struct_size = current_size = capacity = alignment = 0; data = nullptr; }
	inline void         free() { if (data) { current_size = capacity = alignment = 0; tfxFREE(data); data = nullptr; } }
	inline void         clear() { if (data) { current_size = 0; } }
	inline void			reserve(tfxU32 new_capacity) {
		TFX_ASSERT(struct_size);	//Must assign a value to struct size
		if (new_capacity <= capacity) return;
		void *new_data;
		if (alignment != 0) {
			new_data = tfxALLOCATE_ALIGNED((size_t)new_capacity * struct_size, alignment);
		} else {
			new_data = tfxREALLOCATE(data, (size_t)new_capacity * struct_size);
		}
		TFX_ASSERT(new_data);    //Unable to allocate memory. todo: better handling
		if (data) {
			memcpy(new_data, data, (size_t)current_size * struct_size);
			tfxFREE(data);
		}
		data = new_data;
		capacity = new_capacity;
	}
	inline tfxU32       _grow_capacity(tfxU32 size) const { tfxU32 new_capacity = capacity ? (capacity + capacity / 2) : 8; return new_capacity > size ? new_capacity : size; }
	inline void         resize(tfxU32 new_size) { if (new_size > capacity) reserve(_grow_capacity(new_size)); current_size = new_size; }
	inline void         resize_bytes(tfxU32 new_size) { if (new_size > capacity) reserve(_grow_capacity(new_size)); current_size = new_size; }
	inline tfxU32		size_in_bytes() { return current_size * struct_size; }
	inline const tfxU32	size_in_bytes() const { return current_size * struct_size; }
};
inline tfx_buffer_t tfxCreateBuffer(tfxU32 struct_size, tfxU32 alignment) {
	tfx_buffer_t buffer;
	buffer.struct_size = struct_size;
	buffer.alignment = alignment;
	return buffer;
}
inline void tfxReconfigureBuffer(tfx_buffer_t *buffer, size_t new_struct_size) {
	if ((tfxU32)new_struct_size == buffer->struct_size) return;
	tfxU32 size_in_bytes = buffer->capacity * buffer->struct_size;
	tfxU32 new_capacity = size_in_bytes / (tfxU32)new_struct_size;
	buffer->capacity = new_capacity;
	buffer->struct_size = (tfxU32)new_struct_size;
}

//Simple storage map for storing things by key/pair. The data will be in order that you add items, but the map will be in key order so just do a foreach on the data
//and use At() to retrieve data items by name use [] overload to fetch by index if you have that.
//Should not be used to constantly insert/remove things every frame, it's designed for setting up lists and fetching values in loops (by index preferably), and modifying based on user interaction or setting up new situation.
//Note that if you reference things by index and you then remove something then that index may not be valid anymore so you would need to keep checks on that.
//Not sure how efficient a hash lookup with this is, could probably be better, but isn't used much at all in any realtime particle updating.
template<typename T>
struct tfx_storage_map_t {
	struct pair {
		tfxKey key;
		tfxU32 index;
		pair(tfxKey k, tfxU32 i) : key(k), index(i) {}
	};

	tfx_hasher_t hasher;
	tfx_vector_t<pair> map;
	tfx_vector_t<T> data;
	void(*remove_callback)(T &item) = nullptr;

	tfx_storage_map_t() {}
	tfx_storage_map_t(const char *map_tracker, const char *data_tracker) : map(tfxCONSTRUCTOR_VEC_INIT2(map_tracker)), data(tfxCONSTRUCTOR_VEC_INIT2(data_tracker)) {}

	inline void reserve(tfxU32 size) {
		if (size > data.capacity) {
			map.reserve(size);
			data.reserve(size);
		}
	}

	//Insert a new T value into the storage
	inline tfxKey Insert(const char *name, const T &value) {
		tfxKey key = tfx_Hash(&hasher, name, strlen(name), 0);
		SetIndex(key, value);
		return key;
	}

	//Insert a new T value into the storage
	inline void InsertWithLength(const char *name, tfxU32 length, const T &value) {
		tfxKey key = tfx_Hash(&hasher, name, length, 0);
		SetIndex(key, value);
	}

	//Insert a new T value into the storage
	inline void Insert(tfxKey key, const T &value) {
		SetIndex(key, value);
	}

	//Insert a new T value into the storage
	inline void InsertByInt(int name, const T &value) {
		tfxKey key = name;
		SetIndex(key, value);
	}

	inline void Clear() {
		data.clear();
		map.clear();
	}

	inline tfxKey MakeKey(const char *name) {
		return tfx_Hash(&hasher, name, strlen(name), 0);
	}

	inline void FreeAll() {
		data.free();
		map.free();
	}

	inline tfxU32 Size() {
		return data.current_size;
	}

	inline tfxU32 LastIndex() {
		return data.current_size - 1;
	}

	inline bool ValidIndex(tfxU32 index) {
		return index < data.current_size;
	}

	inline bool ValidName(const char *name, tfxU32 length = 0) {
		TFX_ASSERT(name);    //Can't search for anything that's null
		return GetIndex(name, length) > -1;
	}

	inline bool ValidKey(tfxKey key) {
		return GetIndex(key) > -1;
	}

	inline bool ValidIntName(tfxU32 name) {
		return GetIntIndex(name) > -1;
	}

	//Remove an item from the data. Slow function, 2 memmoves and then the map has to be iterated and indexes reduced by one
	//to re-align them
	inline void Remove(const char *name) {
		tfxKey key = tfx_Hash(&hasher, name, strlen(name), 0);
		pair *it = LowerBound(key);
		if (remove_callback)
			remove_callback(data[it->index]);
		tfxU32 index = it->index;
		T *list_it = &data[index];
		map.erase(it);
		data.erase(list_it);
		for (auto &p : map) {
			if (p.index < index) continue;
			p.index--;
		}
	}

	//Remove an item from the data. Slow function, 2 memmoves and then the map has to be iterated and indexes reduced by one
	//to re-align them
	inline void Remove(const tfxKey &key) {
		pair *it = LowerBound(key);
		if (remove_callback)
			remove_callback(data[it->index]);
		tfxU32 index = it->index;
		T *list_it = &data[index];
		map.erase(it);
		data.erase(list_it);
		for (auto &p : map) {
			if (p.index < index) continue;
			p.index--;
		}
	}

	//Remove an item from the data. Slow function, 2 memmoves and then the map has to be iterated and indexes reduced by one
	//to re-align them
	inline void RemoveInt(int name) {
		tfxKey key = name;
		pair *it = LowerBound(key);
		if (remove_callback)
			remove_callback(data[it->index]);
		tfxU32 index = it->index;
		T *list_it = &data[index];
		map.erase(it);
		data.erase(list_it);
		for (auto &p : map) {
			if (p.index < index) continue;
			p.index--;
		}
	}

	inline T &At(const char *name) {
		int index = GetIndex(name);
		TFX_ASSERT(index > -1);                        //Key was not found
		return data[index];
	}

	inline T &AtInt(int name) {
		int index = GetIntIndex(name);
		TFX_ASSERT(index > -1);                        //Key was not found
		return data[index];
	}

	inline T &At(tfxKey key) {
		int index = GetIndex(key);
		TFX_ASSERT(index > -1);                        //Key was not found
		return data[index];
	}

	inline T &operator[](const tfxU32 index) {
		TFX_ASSERT(index < data.current_size);        //Index was out of range
		return data[index];
	}

	void SetIndex(tfxKey key, const T &value) {
		pair *it = LowerBound(key);
		if (it == map.end() || it->key != key)
		{
			data.push_back(value);
			map.insert(it, pair(key, data.current_size - 1));
			return;
		}
		data[it->index] = value;
	}

	int GetIndex(const char *name, tfxU32 length = 0) {
		tfxKey key = tfx_Hash(&hasher, name, length ? length : strlen(name), 0);
		pair *it = LowerBound(key);
		if (it == map.end() || it->key != key)
			return -1;
		return it->index;
	}

	int GetIntIndex(int name) {
		tfxKey key = name;
		pair *it = LowerBound(key);
		if (it == map.end() || it->key != key)
			return -1;
		return it->index;
	}

	int GetIndex(tfxKey key) {
		pair *it = LowerBound(key);
		if (it == map.end() || it->key != key)
			return -1;
		return it->index;
	}

	pair *LowerBound(tfxKey key)
	{
		tfx_storage_map_t::pair *first = map.data;
		tfx_storage_map_t::pair *last = map.data + map.current_size;
		size_t count = (size_t)(last - first);
		while (count > 0)
		{
			size_t count2 = count >> 1;
			tfx_storage_map_t::pair *mid = first + count2;
			if (mid->key < key)
			{
				first = ++mid;
				count -= count2 + 1;
			} else
			{
				count = count2;
			}
		}
		return first;
	}

};

#define tfxKilobyte(Value) ((Value)*1024LL)
#define tfxMegabyte(Value) (tfxKilobyte(Value)*1024LL)
#define tfxGigabyte(Value) (tfxMegabyte(Value)*1024LL)

#ifndef tfxSTACK_SIZE
#define tfxSTACK_SIZE tfxMegabyte(2)
#endif

#ifndef tfxMT_STACK_SIZE
#define tfxMT_STACK_SIZE tfxMegabyte(4)
#endif

//Used in tfx_soa_buffer_t to store pointers to arrays inside a struct of arrays
struct tfx_soa_data_t {
	void *ptr = nullptr;        //A pointer to the array in the struct
	size_t offset = 0;			//The offset to the memory location in the buffer where the array starts
	size_t unit_size = 0;		//The size of each data type in the array
};

//A buffer designed to contain structs of arrays. If the arrays need to grow then a new memory block is made and all copied over
//together. All arrays in the struct will be the same capacity but can all have different unit sizes/types.
//In order to use this you need to first prepare the buffer by calling tfx__add_struct_array for each struct member of the SoA you're setting up. 
//All members must be of the same struct.
//Then call tfx__finish_soa_buffer_setup to create the memory for the struct of arrays with an initial reserve amount.
struct tfx_soa_buffer_t {
	size_t current_arena_size = 0;				//The current size of the arena that contains all the arrays
	size_t struct_size = 0;						//The size of the struct (each member unit size added up)
	tfxU32 current_size = 0;					//current size of each array
	tfxU32 start_index = 0;						//Start index if you're using the buffer as a ring buffer
	tfxU32 last_bump = 0;						//the amount the the start index was bumped by the last time tfx__bump_soa_buffer was called
	tfxU32 capacity = 0;						//capacity of each array
	tfxU32 block_size = tfxDataWidth;			//Keep the capacity to the nearest block size
	tfxU32 alignment = 4;						//The alignment of the memory. If you're planning on using simd for the memory, then 16 will be necessary.
	tfx_vector_t<tfx_soa_data_t> array_ptrs;    //Container for all the pointers into the arena
	void *user_data = nullptr;
	void(*resize_callback)(tfx_soa_buffer_t *ring, tfxU32 new_index_start) = nullptr;
	void *struct_of_arrays = nullptr;			//Pointer to the struct of arrays. Important that this is a stable pointer! Set with tfx__finish_soa_buffer_setup
	void *data = nullptr;						//Pointer to the area in memory that contains all of the array data    
};

//Note this doesn't free memory, call tfx__free_soa_buffer to do that.
inline void tfx__reset_soa_buffer(tfx_soa_buffer_t *buffer) {
	buffer->current_arena_size = 0;
	buffer->struct_size = 0;
	buffer->current_size = 0;
	buffer->start_index = 0;
	buffer->last_bump = 0;
	buffer->capacity = 0;
	buffer->block_size = tfxDataWidth;
	buffer->user_data = nullptr;
	buffer->resize_callback = nullptr;
	buffer->struct_of_arrays = nullptr;
	buffer->data = nullptr;
}

inline void *tfx__get_end_of_buffer_ptr(tfx_soa_buffer_t *buffer) {
	TFX_ASSERT(buffer->data);
	return (char *)buffer->data + buffer->current_arena_size;
}

//Get the amount of free space in the buffer
inline tfxU32 tfx__free_sprite_buffer_space(tfx_soa_buffer_t *buffer) {
	return buffer->capacity - buffer->current_size;
}

//Get the index based on the buffer being a ring buffer
inline tfxU32 tfx__get_circular_index(tfx_soa_buffer_t *buffer, tfxU32 index) {
	return (buffer->start_index + index) % buffer->capacity;
}

//Convert a circular index back into an index from the start of the buffer
inline tfxU32 tfx__get_absolute_index(tfx_soa_buffer_t *buffer, tfxU32 circular_index) {
	return buffer->capacity - (circular_index % buffer->capacity);
}

//Add an array to a SoABuffer. parse in the size of the data type and the offset to the member variable within the struct.
//You must add all the member veriables in the struct before calling tfx__finish_soa_buffer_setup
inline void tfx__add_struct_array(tfx_soa_buffer_t *buffer, size_t unit_size, size_t offset) {
	tfx_soa_data_t data;
	data.unit_size = unit_size;
	data.offset = offset;
	buffer->array_ptrs.push_back(data);
}

//In order to ensure memory alignment of all arrays in the buffer we need the following function to get the correct amount
//of memory required to contain all the data in the buffer for each array in the struct of arrays.
inline size_t tfx__get_soa_capacity_requirement(tfx_soa_buffer_t *buffer, size_t capacity) {
	size_t size_requirement = 0;
	for (int i = 0; i != buffer->array_ptrs.current_size; ++i) {
		size_requirement += buffer->array_ptrs[i].unit_size * capacity;
		size_t mod = size_requirement % buffer->alignment;
		size_requirement += mod ? buffer->alignment - mod : 0;
	}
	return size_requirement;
}

//Once you have called tfx__add_struct_array for all your member variables you must call this function in order to 
//set up the memory for all your arrays. One block of memory will be created and all your arrays will be line up
//inside the space
inline void tfx__finish_soa_buffer_setup(tfx_soa_buffer_t *buffer, void *struct_of_arrays, tfxU32 reserve_amount, tfxU32 alignment = 4) {
	TFX_ASSERT(buffer->data == nullptr && buffer->array_ptrs.current_size > 0);    //Must be an unitialised soa buffer
	TFX_ASSERT(alignment >= 4);        //Alignment must be 4 or greater
	for (int i = 0; i != buffer->array_ptrs.current_size; ++i) {
		buffer->struct_size += buffer->array_ptrs[i].unit_size;
	}
	reserve_amount = (reserve_amount / buffer->block_size + 1) * buffer->block_size;
	buffer->capacity = reserve_amount;
	buffer->alignment = alignment;
	buffer->current_arena_size = tfx__get_soa_capacity_requirement(buffer, reserve_amount);
	buffer->data = tfxALLOCATE_ALIGNED(buffer->current_arena_size, buffer->alignment);
	TFX_ASSERT(buffer->data);    //Unable to allocate memory. Todo: better handling
	memset(buffer->data, 0, buffer->current_arena_size);
	buffer->struct_of_arrays = struct_of_arrays;
	size_t running_offset = 0;
	for (int i = 0; i != buffer->array_ptrs.current_size; ++i) {
		buffer->array_ptrs[i].ptr = (char *)buffer->data + running_offset;
		memcpy((char *)buffer->struct_of_arrays + buffer->array_ptrs[i].offset, &buffer->array_ptrs[i].ptr, sizeof(void *));
		running_offset += buffer->array_ptrs[i].unit_size * buffer->capacity;
		size_t mod = running_offset % buffer->alignment;
		running_offset += mod ? buffer->alignment - mod : 0;
	}
	if (buffer->resize_callback) {
		buffer->resize_callback(buffer, 0);
	}
}

//Call this function to increase the capacity of all the arrays in the buffer. Data that is already in the arrays is preserved if keep_data passed as true (default).
inline bool tfx__grow_soa_arrays(tfx_soa_buffer_t *buffer, tfxU32 first_new_index, tfxU32 new_target_size, bool keep_data = true) {
	TFX_ASSERT(buffer->capacity);            //buffer must already have a capacity!
	tfxU32 new_capacity = 0;
	new_capacity = new_target_size > buffer->capacity ? new_target_size + new_target_size / 2 : buffer->capacity + buffer->capacity / 2;
	new_capacity = (new_capacity / buffer->block_size + 1) * buffer->block_size;
	void *new_data = tfxALLOCATE_ALIGNED(tfx__get_soa_capacity_requirement(buffer, new_capacity), buffer->alignment);
	TFX_ASSERT(new_data);    //Unable to allocate memory. Todo: better handling
	memset(new_data, 0, new_capacity * buffer->struct_size);
	size_t running_offset = 0;
	if (keep_data) {
		for (int i = 0; i != buffer->array_ptrs.current_size; ++i) {
			size_t capacity = buffer->capacity * buffer->array_ptrs[i].unit_size;
			size_t start_index = buffer->start_index * buffer->array_ptrs[i].unit_size;
			if ((buffer->start_index + buffer->current_size - 1) > buffer->capacity) {
				memcpy((char *)new_data + running_offset, (char *)buffer->array_ptrs[i].ptr + start_index, (size_t)(capacity - start_index));
				memcpy((char *)new_data + (capacity - start_index) + running_offset, (char *)buffer->array_ptrs[i].ptr, (size_t)(start_index));
			} else {
				memcpy((char *)new_data + running_offset, (char *)buffer->array_ptrs[i].ptr + start_index, (size_t)(capacity - start_index));
			}
			running_offset += buffer->array_ptrs[i].unit_size * new_capacity;
			size_t mod = running_offset % buffer->alignment;
			running_offset += mod ? buffer->alignment - mod : 0;
		}
	}
	void *old_data = buffer->data;

	buffer->data = new_data;
	buffer->capacity = new_capacity;
	buffer->current_arena_size = new_capacity * buffer->struct_size;
	running_offset = 0;
	for (int i = 0; i != buffer->array_ptrs.current_size; ++i) {
		buffer->array_ptrs[i].ptr = (char *)buffer->data + running_offset;
		memcpy((char *)buffer->struct_of_arrays + buffer->array_ptrs[i].offset, &buffer->array_ptrs[i].ptr, sizeof(void *));
		running_offset += buffer->array_ptrs[i].unit_size * buffer->capacity;
		size_t mod = running_offset % buffer->alignment;
		running_offset += mod ? buffer->alignment - mod : 0;
	}
	tfxFREE(old_data);

	if (buffer->resize_callback) {
		buffer->resize_callback(buffer, first_new_index);
	}

	buffer->start_index = 0;

	return true;
}

//Increase current size of a SoA Buffer and grow if necessary.
inline void tfx__resize_soa_buffer(tfx_soa_buffer_t *buffer, tfxU32 new_size) {
	TFX_ASSERT(buffer->data);            //No data allocated in buffer
	if (new_size >= buffer->capacity) {
		tfx__grow_soa_arrays(buffer, buffer->capacity, new_size);
	}
	buffer->current_size = new_size;
}

//Copy a buffer
inline void tfx__copy_soa_buffer(tfx_soa_buffer_t *dst, tfx_soa_buffer_t *src) {
	memcpy(dst, src, sizeof(tfx_soa_buffer_t));
	dst->array_ptrs.init();
	dst->array_ptrs.copy(src->array_ptrs);
}

//Increase current size of a SoA Buffer and grow if necessary. This will not shrink the capacity so if new_size is not bigger than the
//current capacity then nothing will happen
inline void tfx__set_soa_capacity(tfx_soa_buffer_t *buffer, tfxU32 new_size) {
	TFX_ASSERT(buffer->data);            //No data allocated in buffer
	if (new_size >= buffer->capacity) {
		tfx__grow_soa_arrays(buffer, buffer->capacity, new_size);
	}
}

//Increase current size of a SoA Buffer by 1 and grow if grow is true. Returns the last index.
inline tfxU32 tfx__add_soa_row(tfx_soa_buffer_t *buffer, bool grow = false) {
	TFX_ASSERT(buffer->data);            //No data allocated in buffer
	tfxU32 new_size = ++buffer->current_size;
	if (grow && new_size == buffer->capacity) {
		tfx__grow_soa_arrays(buffer, buffer->capacity, new_size);
	}
	buffer->current_size = new_size;
	TFX_ASSERT(buffer->current_size <= buffer->capacity);    //Capacity of buffer is exceeded, set grow to true or don't exceed the capacity
	return buffer->current_size - 1;
}

//Increase current size of a SoA Buffer by a specific amount and grow if grow is true. Returns the last index.
//You can also pass in a boolean to know if the buffer had to be increased in size or not. Returns the index where the new rows start.
inline tfxU32 tfx__add_soa_rows_grew(tfx_soa_buffer_t *buffer, tfxU32 amount, bool grow, bool &grew) {
	TFX_ASSERT(buffer->data);            //No data allocated in buffer
	tfxU32 first_new_index = buffer->current_size;
	tfxU32 new_size = buffer->current_size += amount;
	if (grow && new_size >= buffer->capacity) {
		grew = tfx__grow_soa_arrays(buffer, buffer->capacity, new_size);
	}
	buffer->current_size = new_size;
	TFX_ASSERT(buffer->current_size < buffer->capacity);    //Capacity of buffer is exceeded, set grow to true or don't exceed the capacity
	return first_new_index;
}

//Increase current size of a SoA Buffer and grow if grow is true. Returns the index where the new rows start.
inline tfxU32 tfx__add_soa_rows(tfx_soa_buffer_t *buffer, tfxU32 amount, bool grow) {
	TFX_ASSERT(buffer->data);            //No data allocated in buffer
	tfxU32 first_new_index = buffer->current_size;
	tfxU32 new_size = buffer->current_size + amount;
	if (grow && new_size >= buffer->capacity) {
		tfx__grow_soa_arrays(buffer, buffer->capacity, new_size);
	}
	buffer->current_size = new_size;
	TFX_ASSERT(buffer->current_size < buffer->capacity);    //Capacity of buffer is exceeded, set grow to true or don't exceed the capacity
	return first_new_index;
}

//Decrease the current size of a SoA Buffer by 1.
inline void tfx__pop_soa_row(tfx_soa_buffer_t *buffer) {
	TFX_ASSERT(buffer->data && buffer->current_size > 0);            //No data allocated in buffer
	buffer->current_size--;
}

//tfx__bump_soa_buffer the start index of the SoA buffer (ring buffer usage)
inline void tfx__bump_soa_buffer(tfx_soa_buffer_t *buffer) {
	TFX_ASSERT(buffer->data && buffer->current_size > 0);            //No data allocated in buffer
	if (buffer->current_size == 0)
		return;
	buffer->start_index++; buffer->start_index %= buffer->capacity; buffer->current_size--;
}

//tfx__bump_soa_buffer the start index of the SoA buffer (ring buffer usage)
inline void tfx__bump_soa_buffer_amount(tfx_soa_buffer_t *buffer, tfxU32 amount) {
	TFX_ASSERT(buffer->data && buffer->current_size > 0);            //No data allocated in buffer
	if (buffer->current_size == 0)
		return;
	if (amount > buffer->current_size)
		amount = buffer->current_size;
	buffer->start_index += amount;
	buffer->start_index %= buffer->capacity;
	buffer->current_size -= amount;
	buffer->last_bump = amount;
}

//Free the SoA buffer
inline void tfx__free_soa_buffer(tfx_soa_buffer_t *buffer) {
	buffer->current_arena_size = buffer->current_size = buffer->capacity = 0;
	if (buffer->data)
		tfxFREE(buffer->data);
	buffer->array_ptrs.free();
	tfx__reset_soa_buffer(buffer);
}

//Clear the SoA buffer
inline void tfx__clear_soa_buffer(tfx_soa_buffer_t *buffer) {
	buffer->current_size = buffer->start_index = 0;
}

//Trim an SoA buffer to the current size. This is a bit rough and ready and I just created it for trimming compressed sprite data down to size
inline void tfx__trim_soa_buffer(tfx_soa_buffer_t *buffer) {
	if (buffer->current_size == buffer->capacity) {
		return;
	}
	if (buffer->current_size == 0) {
		tfx__free_soa_buffer(buffer);
		return;
	}
	TFX_ASSERT(buffer->current_size < buffer->capacity);
	tfxU32 new_capacity = buffer->current_size;
	void *new_data = tfxALLOCATE_ALIGNED(tfx__get_soa_capacity_requirement(buffer, new_capacity), buffer->alignment);
	TFX_ASSERT(new_data);    //Unable to allocate memory. Todo: better handling
	memset(new_data, 0, new_capacity * buffer->struct_size);
	size_t running_offset = 0;
	for (int i = 0; i != buffer->array_ptrs.current_size; ++i) {
		size_t capacity = new_capacity * buffer->array_ptrs[i].unit_size;
		size_t start_index = buffer->start_index * buffer->array_ptrs[i].unit_size;
		if ((buffer->start_index + buffer->current_size - 1) > buffer->capacity) {
			memcpy((char *)new_data + running_offset, (char *)buffer->array_ptrs[i].ptr + start_index, (size_t)(capacity - start_index));
			memcpy((char *)new_data + (capacity - start_index) + running_offset, (char *)buffer->array_ptrs[i].ptr, (size_t)(start_index));
		} else {
			memcpy((char *)new_data + running_offset, (char *)buffer->array_ptrs[i].ptr + start_index, (size_t)(capacity - start_index));
		}
		running_offset += buffer->array_ptrs[i].unit_size * new_capacity;
		size_t mod = running_offset % buffer->alignment;
		running_offset += mod ? buffer->alignment - mod : 0;

	}
	void *old_data = buffer->data;

	buffer->data = new_data;
	buffer->capacity = new_capacity;
	buffer->current_arena_size = new_capacity * buffer->struct_size;
	running_offset = 0;
	for (int i = 0; i != buffer->array_ptrs.current_size; ++i) {
		buffer->array_ptrs[i].ptr = (char *)buffer->data + running_offset;
		memcpy((char *)buffer->struct_of_arrays + buffer->array_ptrs[i].offset, &buffer->array_ptrs[i].ptr, sizeof(void *));
		running_offset += buffer->array_ptrs[i].unit_size * buffer->capacity;
		size_t mod = running_offset % buffer->alignment;
		running_offset += mod ? buffer->alignment - mod : 0;
	}
	tfxFREE(old_data);
}

#define tmpStack(type, name) tfx_vector_t<type> name

template <typename T>
struct tfx_bucket_t {
	tfx_vector_t<T> data;
	tfx_bucket_t *next;
};

template <typename T>
inline tfx_bucket_t<T> *tfxCreateBucket(tfxU32 size) {
	tfx_bucket_t<T> *bucket = (tfx_bucket_t<T>*)tfxALLOCATE(sizeof(tfx_bucket_t<T>));
	bucket->data.data = nullptr;
	bucket->data.current_size = 0;
	bucket->data.capacity = 0;
	bucket->data.locked = 0;
	bucket->data.alignment = 0;
	bucket->data.reserve(size);
	bucket->next = nullptr;
	return bucket;
}

template <typename T>
struct tfx_bucket_array_t {
	tfxU32 current_size;
	tfxU32 capacity;
	tfxU32 size_of_each_bucket;
	tfxLONG volatile locked;
	tfx_vector_t<tfx_bucket_t<T> *> bucket_list;

	tfx_bucket_array_t() : size_of_each_bucket(8), current_size(0), capacity(0), locked(0) {}

	inline void			 init() { size_of_each_bucket = 8; current_size = capacity = locked = 0; bucket_list.init(); }
	inline bool          empty() { return current_size == 0; }
	inline tfxU32        size() { return current_size; }
	inline void            free() {
		for (tfx_bucket_t<T> *bucket : bucket_list) {
			bucket->data.free();
			tfxFREE(bucket);
		}
		current_size = capacity = 0;
		bucket_list.free();
	}
	inline T &operator[](tfxU32 i) {
		TFX_ASSERT(i < current_size);        //Index is out of bounds
		tfxU32 bucket_index = i / size_of_each_bucket;
		tfxU32 element_index = i % size_of_each_bucket;
		return (*bucket_list[bucket_index]).data[element_index];
	}
	inline const T &operator[](tfxU32 i) const {
		TFX_ASSERT(i < current_size);        //Index is out of bounds
		tfxU32 bucket_index = i / size_of_each_bucket;
		tfxU32 element_index = i % size_of_each_bucket;
		return (*bucket_list[bucket_index]).data[element_index];
	}
	inline T *begin() { return bucket_list.current_size ? (T *)bucket_list[0]->data.data : nullptr; }
	inline const T *begin() const { return bucket_list.current_size ? (T *)bucket_list[0]->data.data : nullptr; }
	inline T *end() { return bucket_list.current_size ? (T *)bucket_list[(current_size - 1) / size_of_each_bucket]->data.end() : nullptr; }
	inline const T *end() const { return bucket_list.current_size ? (T *)bucket_list[(current_size - 1) / size_of_each_bucket]->data.end() : nullptr; }
	inline T &front() { TFX_ASSERT(current_size > 0); return bucket_list[0]->data.front(); }
	inline const T &front() const { TFX_ASSERT(current_size > 0); return bucket_list[0]->data.front(); }
	inline T &back() { TFX_ASSERT(current_size > 0); return bucket_list[(current_size - 1) / size_of_each_bucket]->data.back(); }
	inline const T &back() const { TFX_ASSERT(current_size > 0); return bucket_list[(current_size - 1) / size_of_each_bucket]->data.back(); }
	inline tfxU32        active_buckets() { return current_size == 0 ? 0 : current_size / size_of_each_bucket + 1; }
	inline void         clear() {
		for (tfx_bucket_t<T> *bucket : bucket_list) {
			bucket->data.clear();
		}
		current_size = 0;
	}

	inline tfx_bucket_t<T> *add_bucket(tfxU32 size_of_each_bucket) {
		//current_bucket must be the last bucket in the chain
		tfx_bucket_t<T> *bucket = tfxCreateBucket<T>(size_of_each_bucket);
		bucket_list.push_back(bucket);
		return bucket;
	}

	inline T &push_back(const T &v) {
		if (current_size == capacity) {
			add_bucket(size_of_each_bucket);
			capacity += size_of_each_bucket;
		}
		tfxU32 current_bucket = current_size / size_of_each_bucket;
		bucket_list[current_bucket]->data.push_back(v);
		current_size++;
		return bucket_list[current_bucket]->data.back();
	}

	inline tfxU32 locked_push_back(const T &v) {
		while (tfx__compare_and_exchange(&locked, 1, 0) > 1);

		push_back(v);

		tfx__exchange(&locked, 0);
		return current_size - 1;
	}

	inline T *insert(tfxU32 insert_index, const T &v) {
		TFX_ASSERT(insert_index < current_size);
		tfxU32 insert_bucket = insert_index / size_of_each_bucket;
		tfxU32 element_index = insert_index % size_of_each_bucket;
		if (bucket_list[insert_bucket]->data.current_size < bucket_list[insert_bucket]->data.capacity) {
			//We're inserting in the last bucket
			return bucket_list[insert_bucket]->data.insert(&bucket_list[insert_bucket]->data[element_index], v);
		}
		T end_element = bucket_list[insert_bucket]->data.pop_back();
		T end_element2;
		bool end_pushed = false;
		bool end2_pushed = true;
		bucket_list[insert_bucket]->data.insert(&bucket_list[insert_bucket]->data[element_index], v);
		tfxU32 current_insert_bucket = insert_bucket;
		tfxU32 alternator = 0;
		while (current_insert_bucket++ < active_buckets() - 1) {
			if (bucket_list[current_insert_bucket]->data.full()) {
				if (alternator == 0) {
					end_element2 = bucket_list[current_insert_bucket]->data.pop_back();
					end2_pushed = false;
					bucket_list[current_insert_bucket]->data.push_front(end_element);
					end_pushed = true;
				} else {
					end_element = bucket_list[current_insert_bucket]->data.pop_back();
					end_pushed = false;
					bucket_list[current_insert_bucket]->data.push_front(end_element2);
					end2_pushed = true;
				}
				alternator = alternator ^ 1;
			} else {
				bucket_list[current_insert_bucket]->data.push_front(alternator == 0 ? end_element : end_element2);
				end_pushed = true;
				end2_pushed = true;
				break;
			}
		}
		if (!end_pushed || !end2_pushed) {
			push_back(!end_pushed ? end_element : end_element2);
		} else {
			current_size++;
		}
		return &bucket_list[insert_bucket]->data[element_index];
	}

	inline T *insert(T *position, const T &v) {
		tfxU32 index = 0;
		bool find_result = find(position, index);
		TFX_ASSERT(find_result);    //Could not find the object to insert at, make sure it exists
		return insert(index, v);
	}

	inline void erase(tfxU32 index) {
		TFX_ASSERT(index < current_size);
		tfxU32 bucket_index = index / size_of_each_bucket;
		tfxU32 element_index = index % size_of_each_bucket;
		bucket_list[bucket_index]->data.erase(&bucket_list[bucket_index]->data[element_index]);
		current_size--;
		if (bucket_index == bucket_list.current_size - 1) {
			//We're erasing in the last bucket
			return;
		}
		tfxU32 current_bucket_index = bucket_index;
		while (current_bucket_index < active_buckets() - 1) {
			T front = bucket_list[current_bucket_index + 1]->data.pop_front();
			bucket_list[current_bucket_index]->data.push_back(front);
			current_bucket_index++;
		}
		trim_buckets();
	}

	inline void erase(T *it) {
		tfxU32 index = 0;
		bool find_result = find(it, index);
		TFX_ASSERT(find_result);    //pointer not found in list
		erase(index);
	}

	inline bool find(T *it, tfxU32 &index) {
		for (int i = 0; i != current_size; ++i) {
			if (it == &(*this)[i]) {
				index = i;
				return true;
			}
		}
		return false;
	}

	inline T *find(T *it) {
		for (int i = 0; i != current_size; ++i) {
			if (*it == (*this)[i]) {
				return &(*this)[i];
			}
		}
		return nullptr;
	}

	inline void trim_buckets() {
		if (active_buckets() < bucket_list.current_size) {
			for (int i = active_buckets(); i != bucket_list.current_size; ++i) {
				bucket_list[i]->data.free();
				tfxFREE(bucket_list[i]);
				capacity -= size_of_each_bucket;
			}
			bucket_list.current_size -= bucket_list.current_size - active_buckets();
		}
	}

};

template <typename T>
inline void tfxInitBucketArray(tfx_bucket_array_t<T> *bucket_array, tfxU32 size_of_each_bucket) {
	bucket_array->current_size = bucket_array->locked = bucket_array->capacity = 0;
	bucket_array->size_of_each_bucket = size_of_each_bucket;
}

template <typename T>
inline void tfxCopyBucketArray(tfx_bucket_array_t<T> *dst, tfx_bucket_array_t<T> *src) {
	if (src == dst) {
		return;
	}
	dst->free();
	for (tfx_bucket_t<T> *bucket : src->bucket_list) {
		tfx_bucket_t<T> *copy = dst->add_bucket(src->size_of_each_bucket);
		copy->data.copy(bucket->data);
	}
	dst->current_size = src->current_size;
	dst->capacity = src->capacity;
	dst->size_of_each_bucket = src->size_of_each_bucket;
}

#define tfxBucketLoop(bucket, index) int index = 0; index != bucket.current_size; ++index

struct tfx_line_t {
	const char *start;
	const char *end;
	int length;
};

tfxAPI_EDITOR inline int tfx_FindInLine(tfx_line_t *line, const char *needle) {
	size_t needle_length = strlen(needle);
	if (needle_length > line->length) return -1;
	tfxU32 pos = 0;
	while (needle_length + pos <= line->length) {
		if (strncmp(line->start + pos, needle, needle_length) == 0) {
			return pos;
		}
		++pos;
	}
	return -1;
}

//A char buffer you can use to load a file into and read from
//Has no deconstructor so make sure you call Free() when done
//This is meant for limited usage in timeline fx only and not recommended for use outside!
struct tfx_stream_t {
	tfxU64 size;
	tfxU64 capacity;
	tfxU64 position;
	char *data;

	inline tfx_stream_t() { size = position = capacity = 0; data = nullptr; }
	inline tfx_stream_t(tfxU64 qty) { size = position = capacity = 0; data = nullptr; Resize(qty); }

	inline bool Read(char *dst, tfxU64 count) {
		if (count + position <= size) {
			memcpy(dst, data + position, count);
			position += count;
			return true;
		}
		return false;
	}
	tfx_line_t ReadLine();
	inline bool Write(void *src, tfxU64 count) {
		if (count + position <= size) {
			memcpy(data + position, src, count);
			position += count;
			return true;
		}
		return false;
	}
	void AddLine(const char *format, ...);
	void Appendf(const char *format, ...);
	void Appendv(const char *format, va_list args);
	void SetText(const char *text);
	inline void Append(char c) { if (size) { size--; } data[size] = c; size++; NullTerminate(); }
	inline bool EoF() { return position >= size; }
	inline void AddReturn() { if (size + 1 >= capacity) { tfxU64 new_capacity = capacity * 2; Reserve(new_capacity); } data[size] = '\n'; size++; }
	inline void Seek(tfxU64 offset) {
		if (offset < size)
			position = offset;
		else
			position = size;
	}
	inline void TrimToZero() {
		if (size < capacity) {
			tfx_bool result = tfx_SafeMemset(data, data + size, '\0', capacity - size);
			TFX_ASSERT(result);
		}
	}

	inline bool            Empty() { return size == 0; }
	inline tfxU64        Size() { return size; }
	inline const tfxU64    Size() const { return size; }
	inline tfxU64        Length() { if (!data) { return 0; } return data[size] == '\0' && size > 0 ? size - 1 : size; }
	inline const tfxU64    Length() const { if (!data) { return 0; } return data[size] == '\0' && size > 0 ? size - 1 : size; }

	inline void            Free() { if (data) { size = position = capacity = 0; tfxFREE(data); data = nullptr; } }
	inline void         Clear() { if (data) { size = 0; } }

	inline void         GrowSize(tfxU64 extra_size) {
		Resize(size + extra_size);
	}
	inline void         Reserve(tfxU64 new_capacity) {
		if (new_capacity <= capacity) {
			return;
		}
		char *new_data = (char *)tfxALLOCATE_ALIGNED((tfxU64)new_capacity * sizeof(char), 16);
		memset(new_data, 0, new_capacity);
		TFX_ASSERT(new_data);    //Unable to allocate memory. Todo: better handling
		if (data) {
			memcpy(new_data, data, (tfxU64)size * sizeof(char));
			tfxFREE(data);
		}
		data = new_data;
		capacity = new_capacity;
	}
	inline void         Resize(tfxU64 new_size) {
		if (new_size <= capacity) {
			size = new_size;
			return;
		}
		Reserve(new_size);
		size = new_size;
		position = 0;
	}
	inline void            NullTerminate() { *(data + size++) = '\0'; }

};

tfxAPI_EDITOR inline tfx_stream tfx__create_stream() {
	tfx_stream_t blank_stream = { 0 };
	tfx_stream stream = tfxNEW(tfx_stream);
	*stream = blank_stream;
	return stream;
}

tfxAPI_EDITOR inline void tfx_FreeStream(tfx_stream stream) {
	stream->Free();
	tfxFREE(stream);
}

#endif	//__cplusplus

typedef struct tfx_str_s tfx_str_t;  
typedef struct tfx_buffer_s stf_buffer_t;
typedef struct tfx_storage_map_s tfx_storage_map_t;
typedef struct tfx_soa_data_s tfx_soa_data_t;
typedef struct tfx_soa_buffer_s tfx_soa_buffer_t;
typedef struct tfx_bucket_s tfx_bucket_t;
typedef struct tfx_bucket_array_s tfx_bucket_array_t;
typedef struct tfx_line_s tfx_line_t;

inline tfxU32 tfxIsPowerOf2(tfxU32 v)
{
	return ((v & ~(v - 1)) == v);
}

//-----------------------------------------------------------
//Section: Global_Variables
//-----------------------------------------------------------
extern const tfxU32 tfxPROFILE_COUNT;

extern int tfxNumberOfThreadsInAdditionToMain;

#ifndef tfxMAX_QUEUES
#define tfxMAX_QUEUES 64
#endif

#ifndef tfxMAX_QUEUE_ENTRIES
#define tfxMAX_QUEUE_ENTRIES 512
#endif

#ifdef _WIN32
#include <process.h>
#else
#include <pthread.h>
#endif

typedef struct tfx_work_queue_s tfx_work_queue_t;

// Callback function type
typedef void (*tfx_work_queue_callback)(struct tfx_work_queue_s *queue, void *data);

typedef struct tfx_work_queue_entry_s {
	tfx_work_queue_callback call_back;
	void *data;
} tfx_work_queue_entry_t;

typedef struct tfx_work_queue_s {
	volatile tfx_uint entry_completion_goal;
	volatile tfx_uint entry_completion_count;
	volatile int next_read_entry;
	volatile int next_write_entry;
	tfx_work_queue_entry_t entries[tfxMAX_QUEUE_ENTRIES];
} tfx_work_queue_t;

// Platform-specific synchronization wrapper
typedef struct tfx_sync_s {
#ifdef _WIN32
	CRITICAL_SECTION mutex;
	CONDITION_VARIABLE empty_condition;
	CONDITION_VARIABLE full_condition;
#else
	pthread_mutex_t mutex;
	pthread_cond_t empty_condition;
	pthread_cond_t full_condition;
#endif
} tfx_sync_t;

typedef struct tfx_queue_processor_s {
	tfx_sync_t sync;
	tfx_uint count;
	volatile tfx_bool end_all_threads;
	tfx_work_queue_t *queues[tfxMAX_QUEUES];
} tfx_queue_processor_t;

typedef struct tfx_data_types_dictionary_s {
	int initialised;
	tfx_storage_map_t *names_and_types;
} tfx_data_types_dictionary_t;

//Global variables
typedef struct tfx_storage_s {
	tfxU32 memory_pool_count;
	size_t default_memory_pool_size;
	size_t memory_pool_sizes[tfxMAX_MEMORY_POOLS];
	tfx_pool *memory_pools[tfxMAX_MEMORY_POOLS];
	tfx_data_types_dictionary_t data_types;
	float circle_path_x[tfxCIRCLENODES];
	float circle_path_z[tfxCIRCLENODES];
	tfx_color_ramp_format color_ramp_format;
	tfx_queue_processor_t thread_queues;
	tfx_hasher_t hasher;
#ifdef _WIN32
	HANDLE threads[tfxMAX_THREADS];
#else
	pthread_t threads[tfxMAX_THREADS];
#endif
	tfxU32 thread_count;
} tfx_storage_t;

extern tfx_storage_t *tfxStore;
extern tfx_allocator *tfxMemoryAllocator;

//-----------------------------------------------------------
//Section: Multithreading_Work_Queues
//-----------------------------------------------------------

//Tried to keep this as simple as possible, was originally based on Casey Muratory's Hand Made Hero threading which used the Windows API for
//threading but for the sake of supporting other platforms I changed it to use std::thread which was actually a lot more simple to do then 
//I expected. I just had to swap the semaphores for condition_variable and that was pretty much it other then obviously using std::thread as well.
//There is a single thread pool created to serve multiple queues. Currently each particle manager that you create will have it's own queue and then
//each emitter that the particle manager uses will be given it's own thread.

// Platform-specific atomic operations
tfxINTERNAL inline tfxU32 tfx__atomic_increment(volatile tfxU32 *value) {
#ifdef _WIN32
	return InterlockedIncrement((LONG *)value);
#else
	return __sync_fetch_and_add(value, 1) + 1;
#endif
}

tfxINTERNAL inline int tfx__atomic_compare_exchange(volatile int *dest, int exchange, int comparand) {
#ifdef _WIN32
	return InterlockedCompareExchange((LONG *)dest, exchange, comparand) == comparand;
#else
	return __sync_bool_compare_and_swap(dest, comparand, exchange);
#endif
}

tfxINTERNAL inline void tfx__memory_barrier(void) {
#ifdef _WIN32
	MemoryBarrier();
#else
	__sync_synchronize();
#endif
}

// Initialize synchronization primitives
tfxINTERNAL inline void tfx__sync_init(tfx_sync_t *sync) {
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

tfxINTERNAL inline void tfx__sync_cleanup(tfx_sync_t *sync) {
#ifdef _WIN32
	DeleteCriticalSection(&sync->mutex);
#else
	pthread_mutex_destroy(&sync->mutex);
	pthread_cond_destroy(&sync->empty_condition);
	pthread_cond_destroy(&sync->full_condition);
#endif
}

tfxINTERNAL inline void tfx__sync_lock(tfx_sync_t *sync) {
#ifdef _WIN32
	EnterCriticalSection(&sync->mutex);
#else
	pthread_mutex_lock(&sync->mutex);
#endif
}

tfxINTERNAL inline void tfx__sync_unlock(tfx_sync_t *sync) {
#ifdef _WIN32
	LeaveCriticalSection(&sync->mutex);
#else
	pthread_mutex_unlock(&sync->mutex);
#endif
}

tfxINTERNAL inline void tfx__sync_wait_empty(tfx_sync_t *sync) {
#ifdef _WIN32
	SleepConditionVariableCS(&sync->empty_condition, &sync->mutex, INFINITE);
#else
	pthread_cond_wait(&sync->empty_condition, &sync->mutex);
#endif
}

tfxINTERNAL inline void tfx__sync_wait_full(tfx_sync_t *sync) {
#ifdef _WIN32
	SleepConditionVariableCS(&sync->full_condition, &sync->mutex, INFINITE);
#else
	pthread_cond_wait(&sync->full_condition, &sync->mutex);
#endif
}

tfxINTERNAL inline void tfx__sync_signal_empty(tfx_sync_t *sync) {
#ifdef _WIN32
	WakeConditionVariable(&sync->empty_condition);
#else
	pthread_cond_signal(&sync->empty_condition);
#endif
}

tfxINTERNAL inline void tfx__sync_signal_full(tfx_sync_t *sync) {
#ifdef _WIN32
	WakeConditionVariable(&sync->full_condition);
#else
	pthread_cond_signal(&sync->full_condition);
#endif
}

// Initialize queue processor
tfxINTERNAL inline void tfx__initialise_thread_queues(tfx_queue_processor_t *queues) {
	queues->count = 0;
	memset(queues->queues, 0, tfxMAX_QUEUES * sizeof(void *));
	tfx__sync_init(&queues->sync);
	queues->end_all_threads = 0;
}

tfxINTERNAL inline void tfx__cleanup_thread_queues(tfx_queue_processor_t *queues) {
	tfx__sync_cleanup(&queues->sync);
}

tfxINTERNAL inline tfx_work_queue_t *tfx__get_queue_with_work(tfx_queue_processor_t *thread_processor) {
	tfx__sync_lock(&thread_processor->sync);

	while (thread_processor->count == 0 && !thread_processor->end_all_threads) {
		tfx__sync_wait_full(&thread_processor->sync);
	}

	tfx_work_queue_t *queue = NULL;
	if (thread_processor->count > 0) {
		queue = thread_processor->queues[--thread_processor->count];
		tfx__sync_signal_empty(&thread_processor->sync);
	}

	tfx__sync_unlock(&thread_processor->sync);
	return queue;
}

tfxINTERNAL inline void tfx__push_queue_work(tfx_queue_processor_t *thread_processor, tfx_work_queue_t *queue) {
	tfx__sync_lock(&thread_processor->sync);

	while (thread_processor->count >= tfxMAX_QUEUES) {
		tfx__sync_wait_empty(&thread_processor->sync);
	}

	thread_processor->queues[thread_processor->count++] = queue;
	tfx__sync_signal_full(&thread_processor->sync);

	tfx__sync_unlock(&thread_processor->sync);
}

tfxINTERNAL inline tfx_bool tfx__do_next_work_queue(tfx_queue_processor_t *queue_processor) {
	tfx_work_queue_t *queue = tfx__get_queue_with_work(queue_processor);

	if (queue) {
		tfxU32 original_read_entry = queue->next_read_entry;
		tfxU32 new_original_read_entry = (original_read_entry + 1) % tfxMAX_QUEUE_ENTRIES;

		if (original_read_entry != queue->next_write_entry) {
			if (tfx__atomic_compare_exchange(&queue->next_read_entry, new_original_read_entry, original_read_entry)) {
				tfx_work_queue_entry_t entry = queue->entries[original_read_entry];
				entry.call_back(queue, entry.data);
				tfx__atomic_increment(&queue->entry_completion_count);
			}
		}
	}
	return queue_processor->end_all_threads;
}

tfxINTERNAL inline void tfx__do_next_work_queue_entry(tfx_work_queue_t *queue) {
	tfxU32 original_read_entry = queue->next_read_entry;
	tfxU32 new_original_read_entry = (original_read_entry + 1) % tfxMAX_QUEUE_ENTRIES;

	if (original_read_entry != queue->next_write_entry) {
		if (tfx__atomic_compare_exchange(&queue->next_read_entry, new_original_read_entry, original_read_entry)) {
			tfx_work_queue_entry_t entry = queue->entries[original_read_entry];
			entry.call_back(queue, entry.data);
			tfx__atomic_increment(&queue->entry_completion_count);
		}
	}
}

tfxINTERNAL inline void tfx__add_work_queue_entry(tfx_work_queue_t *queue, void *data, tfx_work_queue_callback call_back) {
	if (!tfxStore->thread_count) {
		call_back(queue, data);
		return;
	}

	tfxU32 new_entry_to_write = (queue->next_write_entry + 1) % tfxMAX_QUEUE_ENTRIES;
	while (new_entry_to_write == queue->next_read_entry) {        //Not enough room in work queue
		//We can do this because we're single producer
		tfx__do_next_work_queue_entry(queue);
	}
	queue->entries[queue->next_write_entry].data = data;
	queue->entries[queue->next_write_entry].call_back = call_back;
	tfx__atomic_increment(&queue->entry_completion_goal);

	tfx__writebarrier;

	tfx__push_queue_work(&tfxStore->thread_queues, queue);
	queue->next_write_entry = new_entry_to_write;

}

tfxINTERNAL inline void tfx__complete_all_work(tfx_work_queue_t *queue) {
	tfx_work_queue_entry_t entry = { 0 };
	while (queue->entry_completion_goal != queue->entry_completion_count) {
		tfx__do_next_work_queue_entry(queue);
	}
	queue->entry_completion_count = 0;
	queue->entry_completion_goal = 0;
}

#ifdef _WIN32
tfxINTERNAL inline unsigned WINAPI tfx__thread_worker(void *arg) {
#else
inline void *tfx__thread_worker(void *arg) {
#endif
	tfx_queue_processor_t *queue_processor = (tfx_queue_processor_t *)arg;
	while (!tfx__do_next_work_queue(queue_processor)) {
		// Continue processing
	}
	return 0;
}

// Thread creation helper function
tfxINTERNAL inline int tfx__create_worker_thread(tfx_storage_t * storage, int thread_index) {
#ifdef _WIN32
	storage->threads[thread_index] = (HANDLE)_beginthreadex(
		NULL,
		0,
		tfx__thread_worker,
		&storage->thread_queues,
		0,
		NULL
	);
	return storage->threads[thread_index] != NULL;
#else
	return pthread_create(
		&storage->threads[thread_index],
		NULL,
		tfx__thread_worker,
		&storage->thread_queues
	) == 0;
#endif
}

// Thread cleanup helper function
tfxINTERNAL inline void tfx__cleanup_thread(tfx_storage_t * storage, int thread_index) {
#ifdef _WIN32
	if (storage->threads[thread_index]) {
		WaitForSingleObject(storage->threads[thread_index], INFINITE);
		CloseHandle(storage->threads[thread_index]);
		storage->threads[thread_index] = NULL;
	}
#else
	pthread_join(storage->threads[thread_index], NULL);
#endif
}

tfxAPI inline unsigned int tfx_HardwareConcurrency(void) {
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
tfxAPI inline unsigned int tfx_HardwareConcurrencySafe(void) {
	unsigned int count = tfx_HardwareConcurrency();
	return count > 0 ? count : 1;
}

// Helper function to get a good default thread count for thread pools
// Usually hardware threads - 1 to leave a core for the OS/main thread
tfxAPI inline unsigned int tfx_GetDefaultThreadCount(void) {
	unsigned int count = tfx_HardwareConcurrency();
	return count > 1 ? count - 1 : 1;
}

#endif
