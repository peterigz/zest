#define ZLOC_IMPLEMENTATION
#include "zloc.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define PASSES 20
#define BUFFERS 100
#define MAX_ALLOC_SIZE zloc__MEGABYTE(6)
#define MIN_ALLOC_SIZE zloc__MEGABYTE(2)
#define MAX_POSSIBLE_ALLOC_SIZE zloc__MEGABYTE(8)
#define MAX_ACTIVE_ALLOCATIONS 1000
#define VIRTUAL_ALLOCATOR_INITIAL_POOL_SIZE                                    \
  (zloc_AllocatorSize() + zloc__MEGABYTE(64))

#define MAX_INPUTS_PER_PASS 5
#define MAX_OUTPUTS_PER_PASS 3

typedef struct {
    size_t size;
    int first_pass_idx;
    int last_pass_idx;
    void *allocation;
} buffer_t;

typedef struct {
    int num_inputs;
    int inputs[MAX_INPUTS_PER_PASS];
    int num_outputs;
    int outputs[MAX_OUTPUTS_PER_PASS];
} pass_t;

int rand_range(int min, int max) {
    float r = (float)rand() / RAND_MAX;
    return (int)(r * (max - min + 1)) + min;
}

// We want to allocate the shortest lifetime memory last and the longest
// lifetime first.

int main(int argc, char **argv) {
    // Denorms, exp=1 and exp=2 + mantissa = 0 are all precise.
 // NOTE: Assuming 8 value (3 bit) mantissa.
 // If this test fails, please change this assumption!
    {
        zloc_uint preciseNumberCount = 17;
        for (zloc_uint i = 0; i < preciseNumberCount; i++)
        {
            zloc_uint roundUp = zloc__uint_to_float_round_up(i);
            zloc_uint roundDown = zloc__uint_to_float_round_down(i);
            ZLOC_ASSERT(i == roundUp);
            ZLOC_ASSERT(i == roundDown);
        }

        // Test some random picked numbers
        typedef struct NumberFloatUpDown
        {
            zloc_uint number;
            zloc_uint up;
            zloc_uint down;
        }NumberFloatUpDown;

        NumberFloatUpDown testData[] = {
            {.number = 17, .up = 17, .down = 16},
            {.number = 118, .up = 39, .down = 38},
            {.number = 1024, .up = 64, .down = 64},
            {.number = 65536, .up = 112, .down = 112},
            {.number = 529445, .up = 137, .down = 136},
            {.number = 1048575, .up = 144, .down = 143},
        };

        for (zloc_uint i = 0; i < sizeof(testData) / sizeof(NumberFloatUpDown); i++)
        {
            NumberFloatUpDown v = testData[i];
            zloc_uint roundUp = zloc__uint_to_float_round_up(v.number);
            zloc_uint roundDown = zloc__uint_to_float_round_down(v.number);
            ZLOC_ASSERT(roundUp == v.up);
            ZLOC_ASSERT(roundDown == v.down);
        }
    }

    {
        // Denorms, exp=1 and exp=2 + mantissa = 0 are all precise.
		// NOTE: Assuming 8 value (3 bit) mantissa.
		// If this test fails, please change this assumption!
        zloc_uint preciseNumberCount = 17;
        for (zloc_uint i = 0; i < preciseNumberCount; i++)
        {
            zloc_uint v = zloc__float_to_uint(i);
            ZLOC_ASSERT(i == v);
        }

        // Test that float->uint->float conversion is precise for all numbers
        // NOTE: Test values < 240. 240->4G = overflows 32 bit integer
        for (zloc_uint i = 0; i < 240; i++)
        {
            zloc_uint v = zloc__float_to_uint(i);
            zloc_uint roundUp = zloc__uint_to_float_round_up(v);
            zloc_uint roundDown = zloc__uint_to_float_round_down(v);
            ZLOC_ASSERT(i == roundUp);
            ZLOC_ASSERT(i == roundDown);
            //if ((i%8) == 0) printf("\n");
            //printf("%u->%u ", i, v);
        }
    }

    void *test_memory = malloc(zloc__MEGABYTE(16));
    zloc_allocator *test_allocator = zloc_InitialiseAllocatorWithPool(test_memory, zloc__MEGABYTE(16));

    {
        void *memory = zloc_Allocate(test_allocator, zloc_CalculateGPUAllocatorSize(100));
        zloc_gpu_allocator_t *gpu_allocator = zloc_CreateGPUAllocator(memory, zloc__MEGABYTE(2), 100);
        zloc_gpu_allocation_t a = zloc_GPUAllocate(gpu_allocator, 1337);
        zloc_uint offset = a.offset;
        ZLOC_ASSERT(offset == 0);
        zloc_GPUFree(gpu_allocator, a);
        zloc_Free(test_allocator, gpu_allocator);
    }

    {
        void *memory = zloc_Allocate(test_allocator, zloc_CalculateGPUAllocatorSize(100));
        zloc_gpu_allocator_t *gpu_allocator = zloc_CreateGPUAllocator(memory, zloc__MEGABYTE(256), 100);
        // Free merges neighbor empty nodes. Next allocation should also have offset = 0
        zloc_gpu_allocation_t a = zloc_GPUAllocate(gpu_allocator, 0);
        ZLOC_ASSERT(a.offset == 0);

        zloc_gpu_allocation_t b = zloc_GPUAllocate(gpu_allocator, 1);
        ZLOC_ASSERT(b.offset == 0);

        zloc_gpu_allocation_t c = zloc_GPUAllocate(gpu_allocator, 123);
        ZLOC_ASSERT(c.offset == 1);

        zloc_gpu_allocation_t d = zloc_GPUAllocate(gpu_allocator, 1234);
        ZLOC_ASSERT(d.offset == 124);

        zloc_GPUFree(gpu_allocator, a);
        zloc_GPUFree(gpu_allocator, b);
        zloc_GPUFree(gpu_allocator, c);
        zloc_GPUFree(gpu_allocator, d);

        // End: Validate that allocator has no fragmentation left. Should be 100% clean.
        zloc_gpu_allocation_t validateAll = zloc_GPUAllocate(gpu_allocator, zloc__MEGABYTE(256));
        ZLOC_ASSERT(validateAll.offset == 0);
        zloc_GPUFree(gpu_allocator, validateAll);

        zloc_Free(test_allocator, gpu_allocator);
    }

    {
        void *memory = zloc_Allocate(test_allocator, zloc_CalculateGPUAllocatorSize(100));
        zloc_gpu_allocator_t *gpu_allocator = zloc_CreateGPUAllocator(memory, zloc__MEGABYTE(256), 100);
        // Free merges neighbor empty nodes. Next allocation should also have offset = 0
        zloc_gpu_allocation_t a = zloc_GPUAllocate(gpu_allocator, 1337);
        ZLOC_ASSERT(a.offset == 0);
        zloc_GPUFree(gpu_allocator, a);

        zloc_gpu_allocation_t b = zloc_GPUAllocate(gpu_allocator, 1337);
        ZLOC_ASSERT(b.offset == 0);
        zloc_GPUFree(gpu_allocator, b);

        // End: Validate that allocator has no fragmentation left. Should be 100% clean.
        zloc_gpu_allocation_t validateAll = zloc_GPUAllocate(gpu_allocator, zloc__MEGABYTE(256));
        ZLOC_ASSERT(validateAll.offset == 0);
        zloc_GPUFree(gpu_allocator, validateAll);

        zloc_Free(test_allocator, gpu_allocator);
    }

    {
        void *memory = zloc_Allocate(test_allocator, zloc_CalculateGPUAllocatorSize(100));
        zloc_gpu_allocator_t *gpu_allocator = zloc_CreateGPUAllocator(memory, zloc__MEGABYTE(256), 100);
        // Allocator should reuse node freed by A since the allocation C fits in the same bin (using pow2 size to be sure)
        zloc_gpu_allocation_t a = zloc_GPUAllocate(gpu_allocator, 1024);
        ZLOC_ASSERT(a.offset == 0);

        zloc_gpu_allocation_t b = zloc_GPUAllocate(gpu_allocator, 3456);
        ZLOC_ASSERT(b.offset == 1024);

        zloc_GPUFree(gpu_allocator, a);

        zloc_gpu_allocation_t c = zloc_GPUAllocate(gpu_allocator, 1024);
        ZLOC_ASSERT(c.offset == 0);

        zloc_GPUFree(gpu_allocator, c);
        zloc_GPUFree(gpu_allocator, b);

        // End: Validate that allocator has no fragmentation left. Should be 100% clean.
        zloc_gpu_allocation_t validateAll = zloc_GPUAllocate(gpu_allocator, zloc__MEGABYTE(256));
        ZLOC_ASSERT(validateAll.offset == 0);
        zloc_GPUFree(gpu_allocator, validateAll);
        zloc_Free(test_allocator, gpu_allocator);
    }

    {
        void *memory = zloc_Allocate(test_allocator, zloc_CalculateGPUAllocatorSize(100));
        zloc_gpu_allocator_t *gpu_allocator = zloc_CreateGPUAllocator(memory, zloc__MEGABYTE(256), 100);
        // Allocator should not reuse node freed by A since the allocation C doesn't fit in the same bin
		// However node D and E fit there and should reuse node from A
        zloc_gpu_allocation_t a = zloc_GPUAllocate(gpu_allocator, 1024);
        ZLOC_ASSERT(a.offset == 0);

        zloc_gpu_allocation_t b = zloc_GPUAllocate(gpu_allocator, 3456);
        ZLOC_ASSERT(b.offset == 1024);

        zloc_GPUFree(gpu_allocator, a);

        zloc_gpu_allocation_t c = zloc_GPUAllocate(gpu_allocator, 2345);
        ZLOC_ASSERT(c.offset == 1024 + 3456);

        zloc_gpu_allocation_t d = zloc_GPUAllocate(gpu_allocator, 456);
        ZLOC_ASSERT(d.offset == 0);

        zloc_gpu_allocation_t e = zloc_GPUAllocate(gpu_allocator, 512);
        ZLOC_ASSERT(e.offset == 456);

        zloc_gpu_storage_report_t report = zloc_GPUStorageReport(gpu_allocator);
        ZLOC_ASSERT(report.totalFreeSpace == zloc__MEGABYTE(256) - 3456 - 2345 - 456 - 512);
        ZLOC_ASSERT(report.largestFreeRegion != report.totalFreeSpace);

        zloc_GPUFree(gpu_allocator, c);
        zloc_GPUFree(gpu_allocator, d);
        zloc_GPUFree(gpu_allocator, b);
        zloc_GPUFree(gpu_allocator, e);

        // End: Validate that allocator has no fragmentation left. Should be 100% clean.
        zloc_gpu_allocation_t validateAll = zloc_GPUAllocate(gpu_allocator, zloc__MEGABYTE(256));
        ZLOC_ASSERT(validateAll.offset == 0);
        zloc_GPUFree(gpu_allocator, validateAll);
        zloc_Free(test_allocator, gpu_allocator);
    }

    {
        void *memory = zloc_Allocate(test_allocator, zloc_CalculateGPUAllocatorSize(256));
        zloc_gpu_allocator_t *gpu_allocator = zloc_CreateGPUAllocator(memory, zloc__MEGABYTE(256), 256);
        // Allocate 256x 1MB. Should fit. Then free four random slots and reallocate four slots.
         // Plus free four contiguous slots an allocate 4x larger slot. All must be zero fragmentation!
        zloc_gpu_allocation_t allocations[256];
        for (zloc_uint i = 0; i < 256; i++)
        {
            allocations[i] = zloc_GPUAllocate(gpu_allocator, 1024 * 1024);
            ZLOC_ASSERT(allocations[i].offset == i * 1024 * 1024);
        }

        zloc_gpu_storage_report_t report = zloc_GPUStorageReport(gpu_allocator);
        ZLOC_ASSERT(report.totalFreeSpace == 0);
        ZLOC_ASSERT(report.largestFreeRegion == 0);

        // Free four random slots
        zloc_GPUFree(gpu_allocator, allocations[243]);
        zloc_GPUFree(gpu_allocator, allocations[5]);
        zloc_GPUFree(gpu_allocator, allocations[123]);
        zloc_GPUFree(gpu_allocator, allocations[95]);

        // Free four contiguous slot (allocator must merge)
        zloc_GPUFree(gpu_allocator, allocations[151]);
        zloc_GPUFree(gpu_allocator, allocations[152]);
        zloc_GPUFree(gpu_allocator, allocations[153]);
        zloc_GPUFree(gpu_allocator, allocations[154]);

        allocations[243] = zloc_GPUAllocate(gpu_allocator, 1024 * 1024);
        allocations[5] = zloc_GPUAllocate(gpu_allocator, 1024 * 1024);
        allocations[123] = zloc_GPUAllocate(gpu_allocator, 1024 * 1024);
        allocations[95] = zloc_GPUAllocate(gpu_allocator, 1024 * 1024);
        allocations[151] = zloc_GPUAllocate(gpu_allocator, 1024 * 1024 * 4); // 4x larger
        ZLOC_ASSERT(allocations[243].offset != zloc__NO_SPACE);
        ZLOC_ASSERT(allocations[5].offset != zloc__NO_SPACE);
        ZLOC_ASSERT(allocations[123].offset != zloc__NO_SPACE);
        ZLOC_ASSERT(allocations[95].offset != zloc__NO_SPACE);
        ZLOC_ASSERT(allocations[151].offset != zloc__NO_SPACE);

        for (zloc_uint i = 0; i < 256; i++)
        {
            if (i < 152 || i > 154)
                zloc_GPUFree(gpu_allocator, allocations[i]);
        }

        zloc_gpu_storage_report_t report2 = zloc_GPUStorageReport(gpu_allocator);
        ZLOC_ASSERT(report2.totalFreeSpace == zloc__MEGABYTE(256));
        ZLOC_ASSERT(report2.largestFreeRegion == zloc__MEGABYTE(256));

        // End: Validate that allocator has no fragmentation left. Should be 100% clean.
        zloc_gpu_allocation_t validateAll = zloc_GPUAllocate(gpu_allocator, zloc__MEGABYTE(256));
        ZLOC_ASSERT(validateAll.offset == 0);
        zloc_GPUFree(gpu_allocator, validateAll);
        zloc_Free(test_allocator, gpu_allocator);
    }

    srand(9874925);

    buffer_t buffers[BUFFERS];
    pass_t passes[PASSES];

    // Initialize buffers
    size_t total_buffer_size = 0;
    for (int i = 0; i < BUFFERS; ++i) {
        buffers[i].size = rand_range(MIN_ALLOC_SIZE, MAX_ALLOC_SIZE);
        buffers[i].first_pass_idx = -1;
        buffers[i].last_pass_idx = -1;
        buffers[i].allocation = 0;
        total_buffer_size += buffers[i].size;
    }

    // Randomly connect buffers to passes as inputs and outputs
    for (int i = 0; i < PASSES; ++i) {
        passes[i].num_inputs = rand_range(1, MAX_INPUTS_PER_PASS);
        passes[i].num_outputs = rand_range(1, MAX_OUTPUTS_PER_PASS);

        // Assign outputs
        for (int j = 0; j < passes[i].num_outputs; ++j) {
            int buffer_idx = rand_range(0, BUFFERS - 1);
            passes[i].outputs[j] = buffer_idx;
            // Update buffer lifetime
            if (buffers[buffer_idx].first_pass_idx == -1 ||
                i < buffers[buffer_idx].first_pass_idx) {
                buffers[buffer_idx].first_pass_idx = i;
            }
            if (i > buffers[buffer_idx].last_pass_idx) {
                buffers[buffer_idx].last_pass_idx = i;
            }
        }

        // Assign inputs
        int inputs = passes[i].num_inputs;
        int index = 0;
        for (int j = 0; j < inputs; ++j) {
            int buffer_idx = rand() % BUFFERS;
            if (buffers[buffer_idx].first_pass_idx != -1 && i > buffers[buffer_idx].first_pass_idx) {
                passes[i].inputs[index] = buffer_idx;
                if (i > buffers[buffer_idx].last_pass_idx) {
                    buffers[buffer_idx].last_pass_idx = i;
                }
                index++;
            } else {
                passes[i].num_inputs--;
            }
        }

    }

    size_t current_buffer_size = 0;
    size_t peak_buffer_size = 0;
    for (int i = 0; i < PASSES; ++i) {
        for (int j = 0; j < passes[i].num_outputs; ++j) {
            buffer_t *buffer = &buffers[passes[i].outputs[j]];
            if (buffer->first_pass_idx == j) {
                current_buffer_size += buffer->size;
                peak_buffer_size = zloc__Max(peak_buffer_size, current_buffer_size);
            }
        }
        for (int j = 0; j < passes[i].num_inputs; ++j) {
            buffer_t *buffer = &buffers[passes[i].inputs[j]];
            if (buffer->last_pass_idx == j) {
                current_buffer_size -= buffer->size;
            }
        }
    }

    // Print out the results to verify
    printf("--- Render Graph Simulation ---\n");
    for (int i = 0; i < BUFFERS; ++i) {
        if (buffers[i].first_pass_idx !=
            -1) { // Only print buffers that were used
            printf("Buffer %3d: Size=%-8zu First Pass=%-3d Last Pass=%-3d "
                "Lifetime=%-3d passes\n",
                i, buffers[i].size, buffers[i].first_pass_idx,
                buffers[i].last_pass_idx,
                buffers[i].last_pass_idx - buffers[i].first_pass_idx + 1);
        }
    }

    printf("Peak usage: %zu, Total buffer size: %zu\n", peak_buffer_size, total_buffer_size);

     /*
    // --- Phase 2: Real allocation replay and verification ---
    printf("\n--- Phase 2: Real Allocation Replay & Verification ---\n");

    char real_allocator_memory[sizeof(zloc_allocator)];
    zloc_allocator *real_allocator = zloc_InitialiseAllocator(real_allocator_memory);
    real_allocator->minimum_allocation_size = 64;
    char *real_pool = (char *)malloc(peak_usage);
    ZLOC_ASSERT(real_pool != NULL);
    zloc_AddPool(real_allocator, real_pool, peak_usage);

    // Reset allocations array for the real run
    memset(allocations, 0, sizeof(allocations));
    allocation_count = 0;

    for (int i = 0; i < NUM_OPERATIONS; ++i) {
        if (operations[i].type == OP_ALLOC) {
            void *ptr = zloc_Allocate(real_allocator, operations[i].size);
            // This is the critical test: assert that the real allocation succeeds
            ZLOC_ASSERT(ptr != NULL && "Real allocation failed, peak usage was underestimated!");
            allocations[operations[i].allocation_index] = ptr;
            allocation_count++;
        } else { // OP_FREE
            int index_to_free = operations[i].allocation_index;
            void *ptr_to_free = allocations[index_to_free];
            ZLOC_ASSERT(ptr_to_free != NULL);
            zloc_Free(real_allocator, ptr_to_free);
            allocations[index_to_free] = allocations[allocation_count - 1];
            allocations[allocation_count - 1] = NULL;
            allocation_count--;
        }
        zloc_pool_stats_t stats = zloc_CreateMemorySnapshot(zloc__first_block_in_pool((zloc_pool *)real_pool));
        printf("%i) Free Blocks: %i, Used Blocks: %i, Largest free: %zu(bytes), largest used: %zu(bytes) - ", i, stats.free_blocks, stats.used_blocks, stats.largest_free_block, stats.largest_used_block);
        printf("Free Memory: %zu(bytes), Used Memory: %zu(bytes)\n", stats.free_size, stats.used_size);
    }

    printf("Real run complete. All allocations succeeded within the calculated peak usage.\n");

    // --- Cleanup ---
    free(real_pool);

    printf("\nTest passed successfully!\n");
    */

    free(test_memory);

    return 0;
}

