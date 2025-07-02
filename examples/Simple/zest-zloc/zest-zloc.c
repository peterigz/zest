#define ZLOC_ENABLE_REMOTE_MEMORY
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
    srand(9874925);

    void *test_memory = malloc(zloc__MEGABYTE(2));
    zloc_allocator *test_allocator = zloc_InitialiseAllocatorWithPool(test_memory, zloc__MEGABYTE(2));

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

    zloc_InitialiseAllocatorForRemote(zloc_Allocate(test_allocator, zloc__MEGABYTE(64)));


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

