#ifndef GPU_ALLOCATOR_INCLUDE_H
#define GPU_ALLOCATOR_INCLUDE_H

#define ZLOC_ENABLE_REMOTE_MEMORY
#define ZLOC_IMPLEMENTATION
#include "zloc.h"

typedef zloc_uint zloc_gpu_node_index;

typedef struct zloc_gpu_allocation_t {
    zloc_uint offset;
    zloc_gpu_node_index metadata; // internal: node index
} zloc_gpu_allocation_t;

typedef struct zloc_gpu_storage_report_t {
    zloc_uint totalFreeSpace;
    zloc_uint largestFreeRegion;
} zloc_gpu_storage_report_t;

typedef struct zloc_gpu_region_t {
    zloc_uint size;
    zloc_uint count;
} zloc_gpu_region_t;

typedef struct zloc_gpu_storage_report_full_t {
    zloc_gpu_region_t freeRegions[zloc__NUM_LEAF_BINS];
} zloc_gpu_storage_report_full_t;

typedef struct zloc_gpu_node_t {
    zloc_uint dataOffset;
    zloc_uint dataSize;
    zloc_gpu_node_index binListPrev;
    zloc_gpu_node_index binListNext;
    zloc_gpu_node_index neighbourPrev;
    zloc_gpu_node_index neighbourNext;
    int used; // TODO: Merge as bit flag
} zloc_gpu_node_t;

typedef struct zloc_gpu_allocator_t {
    zloc_uint m_size;
    zloc_uint m_maxAllocs;
    zloc_uint m_freeStorage;

    zloc_uint m_usedBinsTop;
    zloc_u8 m_usedBins[zloc__NUM_TOP_BINS];
    zloc_gpu_node_index m_binIndices[zloc__NUM_LEAF_BINS];

    zloc_gpu_node_t *m_nodes;
    zloc_gpu_node_index *m_freeNodes;
    zloc_uint m_freeOffset;
} zloc_gpu_allocator_t;

// Bin sizes follow floating point (exponent + mantissa) distribution (piecewise linear log approx)
// This ensures that for each size class, the average overhead percentage stays the same
inline zloc_uint zloc__uint_to_float_round_up(zloc_uint size)
{
    zloc_uint exp = 0;
    zloc_uint mantissa = 0;

    if (size < zloc__MANTISSA_VALUE)
    {
        // Denorm: 0..(zloc__MANTISSA_VALUE-1)
        mantissa = size;
    } else
    {
        // Normalized: Hidden high bit always 1. Not stored. Just like float.
        zloc_uint leadingZeros = zloc__scan_reverse(size);
        zloc_uint highestSetBit = leadingZeros;

        zloc_uint mantissaStartBit = highestSetBit - zloc__MANTISSA_BITS;
        exp = mantissaStartBit + 1;
        mantissa = (size >> mantissaStartBit) & zloc__MANTISSA_MASK;

        zloc_uint lowBitsMask = (1 << mantissaStartBit) - 1;

        // Round up!
        if ((size & lowBitsMask) != 0)
            mantissa++;
    }

    return (exp << zloc__MANTISSA_BITS) + mantissa; // + allows mantissa->exp overflow for round up
}

inline zloc_uint zloc__uint_to_float_round_down(zloc_uint size)
{
    zloc_uint exp = 0;
    zloc_uint mantissa = 0;

    if (size < zloc__MANTISSA_VALUE)
    {
        // Denorm: 0..(zloc__MANTISSA_VALUE-1)
        mantissa = size;
    } else
    {
        // Normalized: Hidden high bit always 1. Not stored. Just like float.
        zloc_uint leadingZeros = zloc__scan_reverse(size);
        zloc_uint highestSetBit = leadingZeros;

        zloc_uint mantissaStartBit = highestSetBit - zloc__MANTISSA_BITS;
        exp = mantissaStartBit + 1;
        mantissa = (size >> mantissaStartBit) & zloc__MANTISSA_MASK;
    }

    return (exp << zloc__MANTISSA_BITS) | mantissa;
}

inline zloc_uint zloc__float_to_uint(zloc_uint floatValue)
{
    zloc_uint exponent = floatValue >> zloc__MANTISSA_BITS;
    zloc_uint mantissa = floatValue & zloc__MANTISSA_MASK;
    if (exponent == 0)
    {
        // Denorms
        return mantissa;
    } else
    {
        return (mantissa | zloc__MANTISSA_VALUE) << (exponent - 1);
    }
}

// Utility functions
inline zloc_uint zloc__find_lowest_set_bit_after(zloc_uint bitMask, zloc_uint startBitIndex)
{
    zloc_uint maskBeforeStartIndex = (1 << startBitIndex) - 1;
    zloc_uint maskAfterStartIndex = ~maskBeforeStartIndex;
    zloc_uint bitsAfter = bitMask & maskAfterStartIndex;
    if (bitsAfter == 0) return zloc__NO_SPACE;
    return zloc__scan_forward(bitsAfter);
}

inline zloc_uint zloc__insert_node_into_bin(zloc_gpu_allocator_t *gpu_allocator, zloc_uint size, zloc_uint dataOffset)
{
    // Round down to bin index to ensure that bin >= alloc
    zloc_uint binIndex = zloc__uint_to_float_round_down(size);

    zloc_uint topBinIndex = binIndex >> zloc__TOP_BINS_INDEX_SHIFT;
    zloc_uint leafBinIndex = binIndex & zloc__LEAF_BINS_INDEX_MASK;

    // Bin was empty before?
    if (gpu_allocator->m_binIndices[binIndex] == zloc__UNUSED)
    {
        // Set bin mask bits
        gpu_allocator->m_usedBins[topBinIndex] |= 1 << leafBinIndex;
        gpu_allocator->m_usedBinsTop |= 1 << topBinIndex;
    }

    // Take a freelist node and insert on top of the bin linked list (next = old top)
    zloc_uint topNodeIndex = gpu_allocator->m_binIndices[binIndex];
    zloc_uint nodeIndex = gpu_allocator->m_freeNodes[gpu_allocator->m_freeOffset--];
    gpu_allocator->m_nodes[nodeIndex] = (zloc_gpu_node_t) {
		.dataOffset = dataOffset, 
		.dataSize = size, 
		.binListPrev = zloc__UNUSED, 
		.binListNext = topNodeIndex, 
		.neighbourPrev = zloc__UNUSED, 
		.neighbourNext = zloc__UNUSED, 
		.used = 0
	};
    if (topNodeIndex != zloc__UNUSED) gpu_allocator->m_nodes[topNodeIndex].binListPrev = nodeIndex;
    gpu_allocator->m_binIndices[binIndex] = nodeIndex;

    gpu_allocator->m_freeStorage += size;

    return nodeIndex;
}

inline void zloc__gpu_allocator_reset(zloc_gpu_allocator_t *gpu_allocator)
{
    gpu_allocator->m_freeStorage = 0;
    gpu_allocator->m_usedBinsTop = 0;
    gpu_allocator->m_freeOffset = gpu_allocator->m_maxAllocs;

    for (zloc_uint i = 0; i < zloc__NUM_TOP_BINS; i++)
        gpu_allocator->m_usedBins[i] = 0;

    for (zloc_uint i = 0; i < zloc__NUM_LEAF_BINS; i++)
        gpu_allocator->m_binIndices[i] = zloc__UNUSED;

    zloc_size node_memory_size = sizeof(zloc_gpu_node_t) * (gpu_allocator->m_maxAllocs + 1);
	memset(gpu_allocator->m_nodes, 0, node_memory_size);

    // Freelist is a stack. Nodes in inverse order so that [0] pops first.
    for (zloc_uint i = 0; i < gpu_allocator->m_maxAllocs + 1; i++)
    {
        gpu_allocator->m_freeNodes[i] = gpu_allocator->m_maxAllocs - i;
    }

    // Start state: Whole storage as one big node
    // Algorithm will split remainders and push them back as smaller nodes
    zloc__insert_node_into_bin(gpu_allocator, gpu_allocator->m_size, 0);
}

inline void zloc__remove_node_from_bin(zloc_gpu_allocator_t *gpu_allocator, zloc_uint nodeIndex)
{
    zloc_gpu_node_t *node = &gpu_allocator->m_nodes[nodeIndex];

    if (node->binListPrev != zloc__UNUSED)
    {
        // Easy case: We have previous node. Just remove this node from the middle of the list.
        gpu_allocator->m_nodes[node->binListPrev].binListNext = node->binListNext;
        if (node->binListNext != zloc__UNUSED) gpu_allocator->m_nodes[node->binListNext].binListPrev = node->binListPrev;
    } else
    {
        // Hard case: We are the first node in a bin. Find the bin.

        // Round down to bin index to ensure that bin >= alloc
        zloc_uint binIndex = zloc__uint_to_float_round_down(node->dataSize);

        zloc_uint topBinIndex = binIndex >> zloc__TOP_BINS_INDEX_SHIFT;
        zloc_uint leafBinIndex = binIndex & zloc__LEAF_BINS_INDEX_MASK;

        gpu_allocator->m_binIndices[binIndex] = node->binListNext;
        if (node->binListNext != zloc__UNUSED) gpu_allocator->m_nodes[node->binListNext].binListPrev = zloc__UNUSED;

        // Bin empty?
        if (gpu_allocator->m_binIndices[binIndex] == zloc__UNUSED)
        {
            // Remove a leaf bin mask bit
            gpu_allocator->m_usedBins[topBinIndex] &= ~(1 << leafBinIndex);

            // All leaf bins empty?
            if (gpu_allocator->m_usedBins[topBinIndex] == 0)
            {
                // Remove a top bin mask bit
                gpu_allocator->m_usedBinsTop &= ~(1 << topBinIndex);
            }
        }
    }

    // Insert the node to freelist
    gpu_allocator->m_freeNodes[++gpu_allocator->m_freeOffset] = nodeIndex;

    gpu_allocator->m_freeStorage -= node->dataSize;
}

ZLOC_API zloc_size zloc_CalculateGPUAllocatorSize(zloc_uint max_allocations);
ZLOC_API zloc_uint zloc_AllocationSize(zloc_gpu_allocator_t *gpu_allocator, zloc_gpu_allocation_t allocation);
ZLOC_API zloc_gpu_storage_report_t zloc_GPUStorageReport(zloc_gpu_allocator_t *gpu_allocator);
ZLOC_API zloc_gpu_storage_report_full_t zloc_StorageReportFull(zloc_gpu_allocator_t *gpu_allocator);
ZLOC_API zloc_gpu_allocator_t *zloc_CreateGPUAllocator(void *memory, zloc_uint size, zloc_uint max_allocations);
ZLOC_API zloc_gpu_allocation_t zloc_GPUAllocate(zloc_gpu_allocator_t *gpu_allocator, zloc_uint size);
ZLOC_API void zloc_GPUFree(zloc_gpu_allocator_t *gpu_allocator, zloc_gpu_allocation_t allocation);

#if defined(ZLOC_IMPLEMENTATION)

zloc_size zloc_CalculateGPUAllocatorSize(zloc_uint max_allocations) {
    zloc_size node_memory_size = sizeof(zloc_gpu_node_t) * (max_allocations + 1);
    zloc_size free_node_memory_size = sizeof(zloc_gpu_node_index) * (max_allocations + 1);
    return sizeof(zloc_gpu_allocator_t) + node_memory_size + free_node_memory_size;
}

zloc_gpu_allocator_t *zloc_CreateGPUAllocator(void *memory, zloc_uint size, zloc_uint max_allocations) {
    zloc_gpu_allocator_t *gpu_allocator = (zloc_gpu_allocator_t*)memory;
    *gpu_allocator = (zloc_gpu_allocator_t){ 0 };
    gpu_allocator->m_size = size;
    gpu_allocator->m_maxAllocs = max_allocations;
    if (sizeof(zloc_gpu_node_index) == 2)
    {
        ZLOC_ASSERT(max_allocations <= 65536);
    }
    zloc_size node_memory_size = sizeof(zloc_gpu_node_t) * (gpu_allocator->m_maxAllocs + 1);
    char *mem_offset = (char *)memory + sizeof(zloc_gpu_allocator_t);
    gpu_allocator->m_nodes = (zloc_gpu_node_t*)mem_offset;
    mem_offset += node_memory_size;
    gpu_allocator->m_freeNodes = (zloc_gpu_node_index *)mem_offset;
    zloc__gpu_allocator_reset(gpu_allocator);
    return gpu_allocator;
}

zloc_gpu_allocation_t zloc_GPUAllocate(zloc_gpu_allocator_t *gpu_allocator, zloc_uint size) {
    // Out of allocations?
    if (gpu_allocator->m_freeOffset == zloc__NO_SPACE)
    {
        return (zloc_gpu_allocation_t) { .offset = zloc__NO_SPACE, .metadata = zloc__NO_SPACE };
    }

    // Round up to bin index to ensure that alloc >= bin
    // Gives us min bin index that fits the size
    zloc_uint minBinIndex = zloc__uint_to_float_round_up(size);

    zloc_uint minTopBinIndex = minBinIndex >> zloc__TOP_BINS_INDEX_SHIFT;
    zloc_uint minLeafBinIndex = minBinIndex & zloc__LEAF_BINS_INDEX_MASK;

    zloc_uint topBinIndex = minTopBinIndex;
    zloc_uint leafBinIndex = zloc__NO_SPACE;

    // If top bin exists, scan its leaf bin. This can fail (NO_SPACE).
    if (gpu_allocator->m_usedBinsTop & (1 << topBinIndex))
    {
        leafBinIndex = zloc__find_lowest_set_bit_after(gpu_allocator->m_usedBins[topBinIndex], minLeafBinIndex);
    }

    // If we didn't find space in top bin, we search top bin from +1
    if (leafBinIndex == zloc__NO_SPACE)
    {
        topBinIndex = zloc__find_lowest_set_bit_after(gpu_allocator->m_usedBinsTop, minTopBinIndex + 1);

        // Out of space?
        if (topBinIndex == zloc__NO_SPACE)
        {
            return (zloc_gpu_allocation_t) { .offset = zloc__NO_SPACE, .metadata = zloc__NO_SPACE };
        }

        // All leaf bins here fit the alloc, since the top bin was rounded up. Start leaf search from bit 0.
        // NOTE: This search can't fail since at least one leaf bit was set because the top bit was set.
        leafBinIndex = zloc__scan_forward(gpu_allocator->m_usedBins[topBinIndex]);
    }

    zloc_uint binIndex = (topBinIndex << zloc__TOP_BINS_INDEX_SHIFT) | leafBinIndex;

    // Pop the top node of the bin. Bin top = node.next.
    zloc_uint nodeIndex = gpu_allocator->m_binIndices[binIndex];
    ZLOC_ASSERT(nodeIndex < gpu_allocator->m_maxAllocs);
    zloc_gpu_node_t *node = &gpu_allocator->m_nodes[nodeIndex];
    zloc_uint nodeTotalSize = node->dataSize;
    node->dataSize = size;
    node->used = 1;
    gpu_allocator->m_binIndices[binIndex] = node->binListNext;
    if (node->binListNext != zloc__UNUSED) gpu_allocator->m_nodes[node->binListNext].binListPrev = zloc__UNUSED;
    gpu_allocator->m_freeStorage -= nodeTotalSize;

    // Bin empty?
    if (gpu_allocator->m_binIndices[binIndex] == zloc__UNUSED)
    {
        // Remove a leaf bin mask bit
        gpu_allocator->m_usedBins[topBinIndex] &= ~(1 << leafBinIndex);

        // All leaf bins empty?
        if (gpu_allocator->m_usedBins[topBinIndex] == 0)
        {
            // Remove a top bin mask bit
            gpu_allocator->m_usedBinsTop &= ~(1 << topBinIndex);
        }
    }

    // Push back reminder N elements to a lower bin
    zloc_uint reminderSize = nodeTotalSize - size;
    if (reminderSize > 0)
    {
        zloc_uint newNodeIndex = zloc__insert_node_into_bin(gpu_allocator, reminderSize, node->dataOffset + size);

        // Link nodes next to each other so that we can merge them later if both are free
        // And update the old next neighbor to point to the new node (in middle)
        if (node->neighbourNext != zloc__UNUSED) gpu_allocator->m_nodes[node->neighbourNext].neighbourPrev = newNodeIndex;
        gpu_allocator->m_nodes[newNodeIndex].neighbourPrev = nodeIndex;
        gpu_allocator->m_nodes[newNodeIndex].neighbourNext = node->neighbourNext;
        node->neighbourNext = newNodeIndex;
    }

    return (zloc_gpu_allocation_t) { .offset = node->dataOffset, .metadata = nodeIndex };
}

void zloc_GPUFree(zloc_gpu_allocator_t *gpu_allocator, zloc_gpu_allocation_t allocation) {
    ZLOC_ASSERT(allocation.metadata != zloc__NO_SPACE);
    if (!gpu_allocator->m_nodes) return;

    zloc_uint nodeIndex = allocation.metadata;
    zloc_gpu_node_t *node = &gpu_allocator->m_nodes[nodeIndex];

    // Double delete check
    ZLOC_ASSERT(node->used);

    // Merge with neighbors...
    zloc_uint offset = node->dataOffset;
    zloc_uint size = node->dataSize;

    if ((node->neighbourPrev != zloc__UNUSED) && (gpu_allocator->m_nodes[node->neighbourPrev].used == 0))
    {
        // Previous (contiguous) free node-> Sum sizes
        zloc_gpu_node_t *prevNode = &gpu_allocator->m_nodes[node->neighbourPrev];
        offset = prevNode->dataOffset;
        size += prevNode->dataSize;

        // Remove node from the bin linked list and put it in the freelist
        zloc__remove_node_from_bin(gpu_allocator, node->neighbourPrev);

        ZLOC_ASSERT(prevNode->neighbourNext == nodeIndex);
        node->neighbourPrev = prevNode->neighbourPrev;
    }

    if ((node->neighbourNext != zloc__UNUSED) && (gpu_allocator->m_nodes[node->neighbourNext].used == 0))
    {
        // Next (contiguous) free node-> Sum sizes.
        zloc_gpu_node_t *nextNode = &gpu_allocator->m_nodes[node->neighbourNext];
        size += nextNode->dataSize;

        // Remove node from the bin linked list and put it in the freelist
        zloc__remove_node_from_bin(gpu_allocator, node->neighbourNext);

        ZLOC_ASSERT(nextNode->neighbourPrev == nodeIndex);
        node->neighbourNext = nextNode->neighbourNext;
    }

    zloc_uint neighbourNext = node->neighbourNext;
    zloc_uint neighbourPrev = node->neighbourPrev;

    // Insert the removed node to freelist
    gpu_allocator->m_freeNodes[++gpu_allocator->m_freeOffset] = nodeIndex;

    // Insert the (combined) free node to bin
    zloc_uint combinedNodeIndex = zloc__insert_node_into_bin(gpu_allocator, size, offset);

    // Connect neighbors with the new combined node
    if (neighbourNext != zloc__UNUSED)
    {
        gpu_allocator->m_nodes[combinedNodeIndex].neighbourNext = neighbourNext;
        gpu_allocator->m_nodes[neighbourNext].neighbourPrev = combinedNodeIndex;
    }
    if (neighbourPrev != zloc__UNUSED)
    {
        gpu_allocator->m_nodes[combinedNodeIndex].neighbourPrev = neighbourPrev;
        gpu_allocator->m_nodes[neighbourPrev].neighbourNext = combinedNodeIndex;
    }
}

zloc_gpu_storage_report_t zloc_GPUStorageReport(zloc_gpu_allocator_t *gpu_allocator) {
    zloc_uint largestFreeRegion = 0;
    zloc_uint freeStorage = 0;

    // Out of allocations? -> Zero free space
    if (gpu_allocator->m_freeOffset > 0)
    {
        freeStorage = gpu_allocator->m_freeStorage;
        if (gpu_allocator->m_usedBinsTop)
        {
            zloc_uint topBinIndex = zloc__scan_reverse(gpu_allocator->m_usedBinsTop);
            zloc_uint leafBinIndex = zloc__scan_reverse(gpu_allocator->m_usedBins[topBinIndex]);
            largestFreeRegion = zloc__float_to_uint((topBinIndex << zloc__TOP_BINS_INDEX_SHIFT) | leafBinIndex);
            ZLOC_ASSERT(freeStorage >= largestFreeRegion);
        }
    }

    return (zloc_gpu_storage_report_t) { .totalFreeSpace = freeStorage, .largestFreeRegion = largestFreeRegion };
}

zloc_gpu_storage_report_full_t zloc_StorageReportFull(zloc_gpu_allocator_t *gpu_allocator) {
    zloc_gpu_storage_report_full_t report;
    for (zloc_uint i = 0; i < zloc__NUM_LEAF_BINS; i++) {
        zloc_uint count = 0;
        zloc_uint nodeIndex = gpu_allocator->m_binIndices[i];
        while (nodeIndex != zloc__UNUSED) {
            nodeIndex = gpu_allocator->m_nodes[nodeIndex].binListNext;
            count++;
        }
        report.freeRegions[i] = (zloc_gpu_region_t){ .size = zloc__float_to_uint(i), .count = count };
    }
    return report;
}

zloc_uint zloc_AllocationSize(zloc_gpu_allocator_t *gpu_allocator, zloc_gpu_allocation_t allocation) {
    if (allocation.metadata == zloc__NO_SPACE) return 0;
    if (!gpu_allocator->m_nodes) return 0;
    return gpu_allocator->m_nodes[allocation.metadata].dataSize;
}

#endif

#endif
