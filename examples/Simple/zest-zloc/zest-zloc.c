#define OA_IMPLEMENTATION
#include "gpu_allocator.h"
#include <assert.h>

// We want to allocate the shortest lifetime memory last and the longest
// lifetime first.

int main(int argc, char **argv) {
    // Denorms, exp=1 and exp=2 + mantissa = 0 are all precise.
	// NOTE: Assuming 8 value (3 bit) mantissa.
	// If this test fails, please change this assumption!
    {
        oa_uint preciseNumberCount = 17;
        for (oa_uint i = 0; i < preciseNumberCount; i++)
        {
            oa_uint roundUp = oa__uint_to_float_round_up(i);
            oa_uint roundDown = oa__uint_to_float_round_down(i);
            assert(i == roundUp);
            assert(i == roundDown);
        }

        // Test some random picked numbers
        typedef struct NumberFloatUpDown
        {
            oa_uint number;
            oa_uint up;
            oa_uint down;
        }NumberFloatUpDown;

        NumberFloatUpDown testData[] = {
            {.number = 17, .up = 17, .down = 16},
            {.number = 118, .up = 39, .down = 38},
            {.number = 1024, .up = 64, .down = 64},
            {.number = 65536, .up = 112, .down = 112},
            {.number = 529445, .up = 137, .down = 136},
            {.number = 1048575, .up = 144, .down = 143},
        };

        for (oa_uint i = 0; i < sizeof(testData) / sizeof(NumberFloatUpDown); i++)
        {
            NumberFloatUpDown v = testData[i];
            oa_uint roundUp = oa__uint_to_float_round_up(v.number);
            oa_uint roundDown = oa__uint_to_float_round_down(v.number);
            assert(roundUp == v.up);
            assert(roundDown == v.down);
        }
    }

    {
        // Denorms, exp=1 and exp=2 + mantissa = 0 are all precise.
		// NOTE: Assuming 8 value (3 bit) mantissa.
		// If this test fails, please change this assumption!
        oa_uint preciseNumberCount = 17;
        for (oa_uint i = 0; i < preciseNumberCount; i++)
        {
            oa_uint v = oa__float_to_uint(i);
            assert(i == v);
        }

        // Test that float->uint->float conversion is precise for all numbers
        // NOTE: Test values < 240. 240->4G = overflows 32 bit integer
        for (oa_uint i = 0; i < 240; i++)
        {
            oa_uint v = oa__float_to_uint(i);
            oa_uint roundUp = oa__uint_to_float_round_up(v);
            oa_uint roundDown = oa__uint_to_float_round_down(v);
            assert(i == roundUp);
            assert(i == roundDown);
            //if ((i%8) == 0) printf("\n");
            //printf("%u->%u ", i, v);
        }
    }

    {
        void *memory = malloc(oa_CalculateGPUAllocatorSize(100));
        oa_gpu_allocator_t *gpu_allocator = oa_CreateGPUAllocator(memory, oa__MEGABYTE(2), 100);
        oa_gpu_allocation_t a = oa_GPUAllocate(gpu_allocator, 1337);
        oa_uint offset = a.offset;
        assert(offset == 0);
        oa_GPUFree(gpu_allocator, a);
        free(gpu_allocator);
    }

    {
        void *memory = malloc(oa_CalculateGPUAllocatorSize(100));
        oa_gpu_allocator_t *gpu_allocator = oa_CreateGPUAllocator(memory, oa__MEGABYTE(256), 100);
        // Free merges neighbor empty nodes. Next allocation should also have offset = 0
        oa_gpu_allocation_t a = oa_GPUAllocate(gpu_allocator, 0);
        assert(a.offset == 0);

        oa_gpu_allocation_t b = oa_GPUAllocate(gpu_allocator, 1);
        assert(b.offset == 0);

        oa_gpu_allocation_t c = oa_GPUAllocate(gpu_allocator, 123);
        assert(c.offset == 1);

        oa_gpu_allocation_t d = oa_GPUAllocate(gpu_allocator, 1234);
        assert(d.offset == 124);

        oa_GPUFree(gpu_allocator, a);
        oa_GPUFree(gpu_allocator, b);
        oa_GPUFree(gpu_allocator, c);
        oa_GPUFree(gpu_allocator, d);

        // End: Validate that allocator has no fragmentation left. Should be 100% clean.
        oa_gpu_allocation_t validateAll = oa_GPUAllocate(gpu_allocator, oa__MEGABYTE(256));
        assert(validateAll.offset == 0);
        oa_GPUFree(gpu_allocator, validateAll);

        free(gpu_allocator);
    }

    {
        void *memory = malloc(oa_CalculateGPUAllocatorSize(100));
        oa_gpu_allocator_t *gpu_allocator = oa_CreateGPUAllocator(memory, oa__MEGABYTE(256), 100);
        // Free merges neighbor empty nodes. Next allocation should also have offset = 0
        oa_gpu_allocation_t a = oa_GPUAllocate(gpu_allocator, 1337);
        assert(a.offset == 0);
        oa_GPUFree(gpu_allocator, a);

        oa_gpu_allocation_t b = oa_GPUAllocate(gpu_allocator, 1337);
        assert(b.offset == 0);
        oa_GPUFree(gpu_allocator, b);

        // End: Validate that allocator has no fragmentation left. Should be 100% clean.
        oa_gpu_allocation_t validateAll = oa_GPUAllocate(gpu_allocator, oa__MEGABYTE(256));
        assert(validateAll.offset == 0);
        oa_GPUFree(gpu_allocator, validateAll);

        free(gpu_allocator);
    }

    {
        void *memory = malloc(oa_CalculateGPUAllocatorSize(100));
        oa_gpu_allocator_t *gpu_allocator = oa_CreateGPUAllocator(memory, oa__MEGABYTE(256), 100);
        // Allocator should reuse node freed by A since the allocation C fits in the same bin (using pow2 size to be sure)
        oa_gpu_allocation_t a = oa_GPUAllocate(gpu_allocator, 1024);
        assert(a.offset == 0);

        oa_gpu_allocation_t b = oa_GPUAllocate(gpu_allocator, 3456);
        assert(b.offset == 1024);

        oa_GPUFree(gpu_allocator, a);

        oa_gpu_allocation_t c = oa_GPUAllocate(gpu_allocator, 1024);
        assert(c.offset == 0);

        oa_GPUFree(gpu_allocator, c);
        oa_GPUFree(gpu_allocator, b);

        // End: Validate that allocator has no fragmentation left. Should be 100% clean.
        oa_gpu_allocation_t validateAll = oa_GPUAllocate(gpu_allocator, oa__MEGABYTE(256));
        assert(validateAll.offset == 0);
        oa_GPUFree(gpu_allocator, validateAll);
        free(gpu_allocator);
    }

    {
        void *memory = malloc(oa_CalculateGPUAllocatorSize(100));
        oa_gpu_allocator_t *gpu_allocator = oa_CreateGPUAllocator(memory, oa__MEGABYTE(256), 100);
        // Allocator should not reuse node freed by A since the allocation C doesn't fit in the same bin
		// However node D and E fit there and should reuse node from A
        oa_gpu_allocation_t a = oa_GPUAllocate(gpu_allocator, 1024);
        assert(a.offset == 0);

        oa_gpu_allocation_t b = oa_GPUAllocate(gpu_allocator, 3456);
        assert(b.offset == 1024);

        oa_GPUFree(gpu_allocator, a);

        oa_gpu_allocation_t c = oa_GPUAllocate(gpu_allocator, 2345);
        assert(c.offset == 1024 + 3456);

        oa_gpu_allocation_t d = oa_GPUAllocate(gpu_allocator, 456);
        assert(d.offset == 0);

        oa_gpu_allocation_t e = oa_GPUAllocate(gpu_allocator, 512);
        assert(e.offset == 456);

        oa_gpu_storage_report_t report = oa_GPUStorageReport(gpu_allocator);
        assert(report.totalFreeSpace == oa__MEGABYTE(256) - 3456 - 2345 - 456 - 512);
        assert(report.largestFreeRegion != report.totalFreeSpace);

        oa_GPUFree(gpu_allocator, c);
        oa_GPUFree(gpu_allocator, d);
        oa_GPUFree(gpu_allocator, b);
        oa_GPUFree(gpu_allocator, e);

        // End: Validate that allocator has no fragmentation left. Should be 100% clean.
        oa_gpu_allocation_t validateAll = oa_GPUAllocate(gpu_allocator, oa__MEGABYTE(256));
        assert(validateAll.offset == 0);
        oa_GPUFree(gpu_allocator, validateAll);
        free(gpu_allocator);
    }

    {
        void *memory = malloc(oa_CalculateGPUAllocatorSize(256));
        oa_gpu_allocator_t *gpu_allocator = oa_CreateGPUAllocator(memory, oa__MEGABYTE(256), 256);
        // Allocate 256x 1MB. Should fit. Then free four random slots and reallocate four slots.
         // Plus free four contiguous slots an allocate 4x larger slot. All must be zero fragmentation!
        oa_gpu_allocation_t allocations[256];
        for (oa_uint i = 0; i < 256; i++)
        {
            allocations[i] = oa_GPUAllocate(gpu_allocator, 1024 * 1024);
            assert(allocations[i].offset == i * 1024 * 1024);
        }

        oa_gpu_storage_report_t report = oa_GPUStorageReport(gpu_allocator);
        assert(report.totalFreeSpace == 0);
        assert(report.largestFreeRegion == 0);

        // Free four random slots
        oa_GPUFree(gpu_allocator, allocations[243]);
        oa_GPUFree(gpu_allocator, allocations[5]);
        oa_GPUFree(gpu_allocator, allocations[123]);
        oa_GPUFree(gpu_allocator, allocations[95]);

        // Free four contiguous slot (allocator must merge)
        oa_GPUFree(gpu_allocator, allocations[151]);
        oa_GPUFree(gpu_allocator, allocations[152]);
        oa_GPUFree(gpu_allocator, allocations[153]);
        oa_GPUFree(gpu_allocator, allocations[154]);

        allocations[243] = oa_GPUAllocate(gpu_allocator, 1024 * 1024);
        allocations[5] = oa_GPUAllocate(gpu_allocator, 1024 * 1024);
        allocations[123] = oa_GPUAllocate(gpu_allocator, 1024 * 1024);
        allocations[95] = oa_GPUAllocate(gpu_allocator, 1024 * 1024);
        allocations[151] = oa_GPUAllocate(gpu_allocator, 1024 * 1024 * 4); // 4x larger
        assert(allocations[243].offset != oa__NO_SPACE);
        assert(allocations[5].offset != oa__NO_SPACE);
        assert(allocations[123].offset != oa__NO_SPACE);
        assert(allocations[95].offset != oa__NO_SPACE);
        assert(allocations[151].offset != oa__NO_SPACE);

        for (oa_uint i = 0; i < 256; i++)
        {
            if (i < 152 || i > 154)
                oa_GPUFree(gpu_allocator, allocations[i]);
        }

        oa_gpu_storage_report_t report2 = oa_GPUStorageReport(gpu_allocator);
        assert(report2.totalFreeSpace == oa__MEGABYTE(256));
        assert(report2.largestFreeRegion == oa__MEGABYTE(256));

        // End: Validate that allocator has no fragmentation left. Should be 100% clean.
        oa_gpu_allocation_t validateAll = oa_GPUAllocate(gpu_allocator, oa__MEGABYTE(256));
        assert(validateAll.offset == 0);
        oa_GPUFree(gpu_allocator, validateAll);
        free(gpu_allocator);
    }

    return 0;
}

