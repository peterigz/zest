#include "zest-tests.h"

//Device reset tests. zest_ResetDevice clears out all memory used by the device without destroying
//the device itself. The harness runs the reset sequence between every test, but nothing asserts
//the reset semantics themselves - a failure would only surface as downstream weirdness in
//whichever test runs next. These tests pin the contract down directly: the post-reset memory
//footprint is identical across repeated create/reset cycles, resources still pending on the
//deferred freeing lists are processed safely, the default pool sizes survive the reset and the
//bindless index space is fully reclaimed.

//The exact reset sequence the harness runs between tests. zest_ResetDevice recreates the logical
//device and so destroys the context along with it, so the context is recreated with the same create
//info afterwards. (create info is captured before the reset because the reset frees the context.)
void ResetDeviceCycle(ZestTests *tests) {
	zest_create_context_info_t create_info = tests->context->create_info;
	zest_ResetDevice(tests->device);
	tests->context = zest_CreateContext(tests->device, &tests->window_data, &create_info);
}

//Every in-use block in a host allocator falls into one of three buckets. The reset contract is
//different for each, so they are accounted for separately rather than lumped into the allocator's
//aggregate stats.
//  - zest_typed:   a Zest struct (buffer, image, layer, ...), identified by its magic. Fully owned
//                  by Zest code, so its block count must be identical across resets - a drift in
//                  count is a leak (byte totals wobble with TLSF split remainders, so aren't pinned).
//  - zest_untyped: a Zest allocation with no magic at offset 0 (vectors, text). Also Zest-owned and
//                  held to the same exact-match contract.
//  - driver:       a Vulkan driver *host* allocation routed through zest__vk_*_allocate_callback and
//                  wrapped in a zest_platform_memory_info_t. The driver frees these only at
//                  vkDestroyDevice, so a reset (which keeps the device alive) can leave them behind
//                  and their count can wobble cycle to cycle. Out of Zest's control - not pinned.
enum TestBlockBucket {
	test_block_zest_typed,
	test_block_zest_untyped,
	test_block_driver,
};

const char *TestBlockBucketName(TestBlockBucket bucket) {
	switch (bucket) {
		case test_block_zest_typed:   return "zest-typed";
		case test_block_zest_untyped: return "zest-untyped";
		case test_block_driver:       return "driver";
	}
	return "?";
}

//Classify a block by inspecting its first bytes. A Zest struct carries a magic identifier; a
//driver allocation instead starts with a zest_platform_memory_info_t whose context_info low word is
//the same magic (see zest__vk_device_allocate_callback). Anything else is a plain Zest allocation.
//The identifier alone is only a 16 bit test and blocks storing arbitrary data at offset 0 can
//collide with it (a backend struct whose first member is a driver handle whose low word cycles
//through 0x4E57, for example), so a typed block must also carry a known struct type.
TestBlockBucket ClassifyBlock(void *allocation, zest_uint *struct_type_out) {
	if (ZEST_VALID_IDENTIFIER(allocation) && zest__is_known_struct_type((zest_uint)ZEST_STRUCT_TYPE(allocation))) {
		*struct_type_out = (zest_uint)ZEST_STRUCT_TYPE(allocation);
		return test_block_zest_typed;
	}
	*struct_type_out = 0;
	zest_platform_memory_info_t *info = (zest_platform_memory_info_t *)allocation;
	if (ZEST_IS_INTITIALISED(info->context_info)) {
		return test_block_driver;
	}
	return test_block_zest_untyped;
}

//Per-bucket totals for one host allocator.
struct AllocatorCensus {
	zest_size zest_typed_bytes;
	int zest_typed_blocks;
	zest_size zest_untyped_bytes;
	int zest_untyped_blocks;
	zest_size driver_bytes;
	int driver_blocks;
};

int CensusTotalBlocks(const AllocatorCensus *c) {
	return c->zest_typed_blocks + c->zest_untyped_blocks + c->driver_blocks;
}

//Histogram of an allocator's in-use blocks bucketed by (bucket, struct type). Block size is NOT
//part of the key: TLSF can hand back a slightly larger block than requested (e.g. 72 bytes for a
//64 byte ask) when the nearest free block can't be split down to the exact class, so the same
//logical allocation lands in different size buckets from run to run. Block *count* per type is the
//stable identity; total bytes is carried only as a secondary, informational figure. Diffing two
//histograms from identical post-reset states pinpoints exactly which allocations a leaking reset
//leaves behind.
struct BlockHistogram {
	struct Entry {
		TestBlockBucket bucket;
		zest_uint struct_type;         //identifies typed blocks
		zest_uint driver_context_info; //identifies driver blocks: the category+command that allocated them
		int count;
		zest_size total_bytes;
	};
	Entry entries[512];
	int entry_count;
	int overflowed;
};

//Walk every pool of a host allocator, tallying the three-bucket census and (optionally) filling a
//per-block histogram. Pool 0 is the allocator's built-in pool; pools 1..count-1 are the extra pools
//tracked in the device/context memory_pools array.
void WalkAllocator(zloc_allocator *allocator, void **pools, zest_uint pool_count, AllocatorCensus *census, BlockHistogram *histogram) {
	memset(census, 0, sizeof(*census));
	if (histogram) {
		histogram->entry_count = 0;
		histogram->overflowed = 0;
	}
	for (zest_uint i = 0; i != pool_count; i++) {
		zloc_header *current_block;
		if (i == 0) {
			current_block = zloc__first_block_in_pool(zloc_GetPool(allocator));
		} else {
			current_block = zloc__first_block_in_pool((zloc_pool*)pools[i]);
		}
		while (!zloc__is_last_block_in_pool(current_block)) {
			if (!zloc__is_free_block(current_block)) {
				zest_size block_size = zloc__block_size(current_block);
				void *allocation = (void *)((char *)current_block + zloc__BLOCK_POINTER_OFFSET);
				zest_uint struct_type;
				TestBlockBucket bucket = ClassifyBlock(allocation, &struct_type);
				switch (bucket) {
					case test_block_zest_typed:   census->zest_typed_bytes += block_size;   census->zest_typed_blocks++;   break;
					case test_block_zest_untyped: census->zest_untyped_bytes += block_size; census->zest_untyped_blocks++; break;
					case test_block_driver:       census->driver_bytes += block_size;       census->driver_blocks++;       break;
				}
				if (histogram) {
					//Driver blocks have no struct type, but the platform_memory_info header records
					//the category+command that requested them - sub-key on that so the diff can name
					//which subsystem's allocations grew instead of lumping all driver blocks together.
					zest_uint driver_context_info = (bucket == test_block_driver) ? ((zest_platform_memory_info_t *)allocation)->context_info : 0;
					int found = 0;
					for (int e = 0; e != histogram->entry_count; ++e) {
						if (histogram->entries[e].bucket == bucket && histogram->entries[e].struct_type == struct_type && histogram->entries[e].driver_context_info == driver_context_info) {
							histogram->entries[e].count++;
							histogram->entries[e].total_bytes += block_size;
							found = 1;
							break;
						}
					}
					if (!found) {
						if (histogram->entry_count < 512) {
							BlockHistogram::Entry *entry = &histogram->entries[histogram->entry_count++];
							entry->bucket = bucket;
							entry->struct_type = struct_type;
							entry->driver_context_info = driver_context_info;
							entry->count = 1;
							entry->total_bytes = block_size;
						} else {
							histogram->overflowed = 1;
						}
					}
				}
			}
			current_block = zloc__next_physical_block(current_block);
		}
	}
}

struct ResetCycleStats {
	AllocatorCensus device;
	AllocatorCensus context;
	int memory_allocation_count;
};

//Captures the three-bucket census of both the device and context host allocators, plus the live
//backend (GPU) memory allocation count. The device histogram is filled in too when a sink is
//supplied so a failing comparison can print exactly which struct types drifted.
ResetCycleStats CaptureResetStats(ZestTests *tests, BlockHistogram *device_histogram) {
	ResetCycleStats stats;
	WalkAllocator(tests->device->allocator, tests->device->memory_pools, tests->device->memory_pool_count, &stats.device, device_histogram);
	WalkAllocator(tests->context->allocator, tests->context->memory_pools, tests->context->memory_pool_count, &stats.context, NULL);
	stats.memory_allocation_count = tests->device->memory_allocation_count;
	return stats;
}

//The Zest-owned buckets (typed + untyped) of both allocators plus the backend allocation count must
//hold the same block *count* across resets - a leak shows up as an extra live block. Byte totals are
//deliberately not compared: TLSF split remainders make the same set of allocations sum to slightly
//different bytes from run to run. The driver bucket is excluded here and checked separately for
//unbounded growth.
int ZestBucketsEqual(const ResetCycleStats *a, const ResetCycleStats *b) {
	return a->device.zest_typed_blocks == b->device.zest_typed_blocks &&
		a->device.zest_untyped_blocks == b->device.zest_untyped_blocks &&
		a->context.zest_typed_blocks == b->context.zest_typed_blocks &&
		a->context.zest_untyped_blocks == b->context.zest_untyped_blocks &&
		a->memory_allocation_count == b->memory_allocation_count;
}

//A per-reset leak of a driver-backed object (a swapchain or descriptor pool Zest fails to destroy
//each cycle) forces the driver to allocate more host blocks on every rebuild - so the driver block
//count grows on every single transition. Wobble that goes up and down is tolerated; strict monotonic
//growth across the whole run is not. Counted in blocks, not bytes, for the same split-remainder
//reason. Needs at least two transitions to mean anything.
int DriverBucketGrowsEveryCycle(const ResetCycleStats *stats, int first, int count) {
	if (count - first < 3) return 0;
	int device_grows = 1;
	int context_grows = 1;
	for (int c = first + 1; c != count; ++c) {
		if (stats[c].device.driver_blocks <= stats[c - 1].device.driver_blocks) device_grows = 0;
		if (stats[c].context.driver_blocks <= stats[c - 1].context.driver_blocks) context_grows = 0;
	}
	return device_grows || context_grows;
}

//Block counts are the asserted figures; byte totals are printed in parens as informational only
//(they wobble with TLSF split remainders even when nothing has leaked).
void PrintCensusDiff(const char *label, const ResetCycleStats *baseline, const ResetCycleStats *current) {
	ZEST_PRINT("\t%s: device zest-typed %i -> %i blocks (~%llu -> ~%llu b), zest-untyped %i -> %i (~%llu -> ~%llu b), driver %i -> %i (~%llu -> ~%llu b)",
		label,
		baseline->device.zest_typed_blocks, current->device.zest_typed_blocks,
		(unsigned long long)baseline->device.zest_typed_bytes, (unsigned long long)current->device.zest_typed_bytes,
		baseline->device.zest_untyped_blocks, current->device.zest_untyped_blocks,
		(unsigned long long)baseline->device.zest_untyped_bytes, (unsigned long long)current->device.zest_untyped_bytes,
		baseline->device.driver_blocks, current->device.driver_blocks,
		(unsigned long long)baseline->device.driver_bytes, (unsigned long long)current->device.driver_bytes);
	ZEST_PRINT("\t%s: context zest-typed %i -> %i blocks (~%llu -> ~%llu b), zest-untyped %i -> %i (~%llu -> ~%llu b), driver %i -> %i (~%llu -> ~%llu b)",
		label,
		baseline->context.zest_typed_blocks, current->context.zest_typed_blocks,
		(unsigned long long)baseline->context.zest_typed_bytes, (unsigned long long)current->context.zest_typed_bytes,
		baseline->context.zest_untyped_blocks, current->context.zest_untyped_blocks,
		(unsigned long long)baseline->context.zest_untyped_bytes, (unsigned long long)current->context.zest_untyped_bytes,
		baseline->context.driver_blocks, current->context.driver_blocks,
		(unsigned long long)baseline->context.driver_bytes, (unsigned long long)current->context.driver_bytes);
	ZEST_PRINT("\t%s: backend memory allocations %i -> %i", label, baseline->memory_allocation_count, current->memory_allocation_count);
}

void zest__tests_print_block_info(zloc_allocator *allocator, void *allocation, zloc_header *current_block, zest_platform_memory_context context_filter, zest_platform_command command_filter) {
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

int HistogramEntriesMatch(const BlockHistogram::Entry *a, const BlockHistogram::Entry *b) {
	return a->bucket == b->bucket && a->struct_type == b->struct_type && a->driver_context_info == b->driver_context_info;
}

//Human-readable label for a histogram entry: typed blocks by struct type, driver blocks by the
//category+command that allocated them, untyped as one bucket. Writes into caller storage.
const char *HistogramEntryLabel(const BlockHistogram::Entry *entry, char *buffer, size_t buffer_size) {
	if (entry->bucket == test_block_driver) {
		zest_uint category = (entry->driver_context_info & 0xff0000) >> 16;
		zest_uint command = (entry->driver_context_info & 0xff000000) >> 24;
		snprintf(buffer, buffer_size, "driver [%s / %s]",
			zest__platform_context_to_string((zest_platform_memory_context)category),
			zest__platform_command_to_string((zest_platform_command)command));
	} else if (entry->bucket == test_block_zest_typed) {
		snprintf(buffer, buffer_size, "zest-typed %s (0x%08x)", zest__struct_type_to_string((zest_struct_type)entry->struct_type), entry->struct_type);
	} else {
		snprintf(buffer, buffer_size, "zest-untyped");
	}
	return buffer;
}

void PrintBlockHistogramDiff(ZestTests *tests, BlockHistogram *baseline, BlockHistogram *current) {
	char label[128];
	if (baseline->overflowed || current->overflowed) {
		ZEST_PRINT("\t(block histogram overflowed, diff may be incomplete)");
	}
	for (int c = 0; c != current->entry_count; ++c) {
		BlockHistogram::Entry *entry = &current->entries[c];
		int baseline_count = 0;
		for (int b = 0; b != baseline->entry_count; ++b) {
			if (HistogramEntriesMatch(&baseline->entries[b], entry)) {
				baseline_count = baseline->entries[b].count;
				break;
			}
		}
		if (entry->count != baseline_count) {
			ZEST_PRINT("\t  %s: %i -> %i blocks (~%llu b)", HistogramEntryLabel(entry, label, sizeof(label)), baseline_count, entry->count, (unsigned long long)entry->total_bytes);
		}
	}
	for (int b = 0; b != baseline->entry_count; ++b) {
		BlockHistogram::Entry *entry = &baseline->entries[b];
		int found = 0;
		for (int c = 0; c != current->entry_count; ++c) {
			if (HistogramEntriesMatch(&current->entries[c], entry)) {
				found = 1;
				break;
			}
		}
		if (!found) {
			ZEST_PRINT("\t  %s: %i -> 0 blocks", HistogramEntryLabel(entry, label, sizeof(label)), entry->count);
		}
	}
}

//TEMPORARY debugging aid: hex/ascii peek at the first bytes of in-use untyped blocks of a given
//size so the leaking allocations can be identified (SPIR-V magic, readable text, etc)
void DebugPeekBlocks(zest_device device, zest_size target_size, int max_prints) {
	int printed = 0;
	for (zest_uint i = 0; i != device->memory_pool_count && printed < max_prints; i++) {
		zloc_header *current_block;
		if (i == 0) {
			current_block = zloc__first_block_in_pool(zloc_GetPool(device->allocator));
		} else {
			current_block = zloc__first_block_in_pool((zloc_pool*)device->memory_pools[i]);
		}
		while (!zloc__is_last_block_in_pool(current_block) && printed < max_prints) {
			zest_size this_size = zloc__block_size(current_block);
			int match = target_size >= 100000 ? (this_size >= 100000) : (this_size == target_size);
			if (!zloc__is_free_block(current_block) && match) {
				unsigned char *bytes = (unsigned char *)current_block + zloc__BLOCK_POINTER_OFFSET;
				ZEST_PRINT("\t  block size %llu at %p:", (unsigned long long)target_size, (void*)bytes);
				for (int row = 0; row != 4; ++row) {
					char hex[3 * 32 + 1];
					char ascii[33];
					for (int b = 0; b != 32; ++b) {
						unsigned char byte = bytes[row * 32 + b];
						sprintf(hex + b * 3, "%02x ", byte);
						ascii[b] = (byte >= 32 && byte < 127) ? (char)byte : '.';
					}
					ascii[32] = '\0';
					ZEST_PRINT("\t    %s | %s", hex, ascii);
				}
				printed++;
			}
			current_block = zloc__next_physical_block(current_block);
		}
	}
}

//Creates a representative batch of every resource type a test suite run touches, acquires
//bindless indexes for some of them and renders one real frame through a layer. Nothing is freed:
//the reset that follows is responsible for reclaiming all of it.
int CreateResetResourceBatch(ZestTests *tests) {
	int failed_count = 0;
	zest_device device = tests->device;

	//Images: a mipped texture (created with pixels so it's uploaded and transitioned to a
	//shader-readable layout, which acquiring a sampled index below requires), a storage image
	//and a sampler
	zest_byte pixels[64 * 64 * 4];
	for (zest_size i = 0; i < sizeof(pixels); ++i) {
		pixels[i] = (zest_byte)(i * 13 + 3);
	}
	zest_image_info_t texture_info = zest_CreateImageInfo(64, 64);
	texture_info.flags = zest_image_preset_texture_mipmaps;
	zest_image_handle texture = zest_CreateImageWithPixels(device, pixels, sizeof(pixels), &texture_info);
	zest_image texture_image = zest_GetImage(texture);
	if (!texture_image) failed_count++;

	zest_image_info_t storage_info = zest_CreateImageInfo(64, 64);
	storage_info.flags = zest_image_preset_storage;
	zest_image_handle storage_image = zest_CreateImage(device, &storage_info);
	if (!zest_GetImage(storage_image)) failed_count++;

	zest_sampler_handle sampler = zest_CreateSampler(device, &tests->sampler_info);
	if (!sampler.value) failed_count++;

	//Buffers: device local large + small and a host visible staging buffer
	zest_buffer_info_t storage_buffer_info = zest_CreateBufferInfo(zest_buffer_type_storage, zest_memory_usage_gpu_only);
	zest_buffer large_buffer = zest_CreateBuffer(device, zloc__KILOBYTE(256), &storage_buffer_info);
	zest_buffer small_buffer = zest_CreateBuffer(device, 512, &storage_buffer_info);
	zest_buffer staging_buffer = zest_CreateStagingBuffer(device, zloc__KILOBYTE(64), 0);
	if (!large_buffer || !small_buffer || !staging_buffer) {
		failed_count++;
	} else {
		memset(zest_BufferData(staging_buffer), 0xAB, zloc__KILOBYTE(64));
	}

	//Bindless indexes, left acquired so the reset has to reclaim them
	if (large_buffer) {
		if (zest_AcquireStorageBufferIndex(device, large_buffer) == ZEST_INVALID) failed_count++;
	}
	if (texture_image) {
		if (zest_AcquireSampledImageIndex(device, texture_image, zest_texture_2d_binding) == ZEST_INVALID) failed_count++;
	}

	//Shader and compute pipeline
	zest_shader_handle shader = zest_CreateShaderFromFile(device, "examples/SDL2/zest-tests/shaders/image_verify.comp", "image_verify.spv", zest_compute_shader, NULL, 1);
	zest_compute_handle compute = zest_CreateCompute(device, "Reset Batch Compute", shader);
	if (!zest_IsValidHandle((void*)&compute)) failed_count++;

	//A layer rendered through one real frame so queues, semaphores, command buffers, transient
	//resources and the cached frame graph all carry state into the reset
	zest_layer_handle layer_handle = zest_CreateInstanceLayer(tests->context, "Reset Batch Layer", sizeof(TestData), 32);
	zest_layer layer = zest_GetLayer(layer_handle);
	zest_pipeline_template pipeline = create_instance_pipeline_template(tests, "Reset Batch Pipeline");

	zest_StartInstanceDrawing(layer, pipeline);
	for (zest_uint i = 0; i != 8; ++i) {
		TestData *instance = (TestData *)zest_NextInstance(layer);
		SetLayerTestInstance(instance, i, 0);
	}
	zest_EndInstanceInstructions(layer);

	zest_UpdateDevice(device);
	if (zest_BeginFrame(tests->context)) {
		zest_frame_graph frame_graph = NULL;
		if (zest_BeginFrameGraph(tests->context, "Reset Batch Frame", 0)) {
			zest_ImportSwapchainResource();
			zest_resource_node layer_resource = zest_AddTransientLayerResource("Layer Data", layer, ZEST_FALSE);

			zest_BeginTransferPass("Upload Layer");
			zest_ConnectOutput(layer_resource);
			zest_SetPassTask(zest_UploadInstanceLayerData, layer);
			zest_EndPass();

			zest_BeginRenderPass("Draw Layer");
			zest_ConnectInput(layer_resource);
			zest_ConnectSwapChainOutput();
			zest_SetPassTask(zest_DrawInstanceLayer, layer);
			zest_EndPass();

			frame_graph = zest_EndFrameGraph();
		}
		zest_EndFrame(tests->context, frame_graph);
		if (zest_GetFrameGraphResult(frame_graph) != 0) failed_count++;
	} else {
		failed_count++;
	}

	return failed_count;
}

int test__device_reset_simple(ZestTests *tests, Test *test) {

	const int cycle_count = 6;
	ResetCycleStats stats[cycle_count];
	static BlockHistogram histograms[cycle_count];
	int loaded_blocks_in_use[cycle_count];
	int failed_count = 0;

	for (int cycle = 0; cycle != cycle_count; ++cycle) {
		loaded_blocks_in_use[cycle] = zest_GetDeviceMemoryStats(tests->device).blocks_in_use;

		ResetDeviceCycle(tests);

		zest_uint validation_errors = zest_GetValidationErrorCount(tests->device);
		if (validation_errors) {
			ZEST_PRINT("\tReset stability: %u validation errors in cycle %i", validation_errors, cycle);
			failed_count++;
		}
		zest_ResetValidationErrors(tests->device);
		stats[cycle] = CaptureResetStats(tests, &histograms[cycle]);
	}

	//The Zest-owned buckets must hold the same block count from the second reset on. The driver
	//bucket is not pinned here (the driver frees its host allocations only at device destroy), but it
	//must not grow on every cycle - that would mean a driver-backed object is leaking per reset.
	for (int cycle = 2; cycle != cycle_count; ++cycle) {
		if (!ZestBucketsEqual(&stats[1], &stats[cycle])) {
			ZEST_PRINT("\tReset stability: Zest-owned memory footprint changed between cycle 1 and cycle %i", cycle);
			PrintCensusDiff("cycle drift", &stats[1], &stats[cycle]);
			PrintBlockHistogramDiff(tests, &histograms[1], &histograms[cycle]);
			failed_count++;
		}
	}

	if (DriverBucketGrowsEveryCycle(stats, 1, cycle_count)) {
		ZEST_PRINT("\tReset stability: driver host allocations grew on every reset - a driver-backed object is leaking");
		PrintCensusDiff("driver growth", &stats[1], &stats[cycle_count - 1]);
		failed_count++;
	}

	//This test allocates nothing between resets, so there is no "batch raised the block count"
	//sanity check to make here (that belongs to the memory-stability test); it purely verifies that
	//repeated bare resets land on a stable block count.
	(void)loaded_blocks_in_use;

	test->result = failed_count > 0 ? 1 : 0;
	test->frame_count++;
	return test->result;
}

/*
Reset Memory Stability: The test runs in two phases. In the create phase each cycle builds a full
batch of device and context resources (nothing explicitly freed) plus one rendered frame, then
resets. In the drain phase it does nothing but reset, cycle after cycle.

Two separate contracts fall out of that split:

  - Zest-owned block counts must return to the same baseline after every reset in either phase - a
    drift is a genuine leak of a live Zest allocation. (Byte totals are not compared: TLSF split
    remainders make them wobble harmlessly.)

  - The driver retains a bounded amount of host memory per object it creates and only frees it at
    vkDestroyDevice, so its block count is *expected* to climb while the create phase keeps making
    new pipelines - that is out of Zest's hands and not a leak. The real driver-leak test is the
    drain phase: with nothing being created, per-object retention plateaus immediately, so if the
    driver block count keeps climbing once creation stops, Zest is failing to destroy a
    driver-backed object on reset.
*/
int test__device_reset_memory_stability(ZestTests *tests, Test *test) {
	const int create_cycles = 4;   //warm up + let the driver build its per-pipeline retention
	const int drain_cycles = 4;    //reset only, create nothing - the driver must plateau here
	const int cycle_count = create_cycles + drain_cycles;
	ResetCycleStats stats[create_cycles + drain_cycles];
	static BlockHistogram histograms[create_cycles + drain_cycles];
	int failed_count = 0;
	int loaded_after_create = 0;

	for (int cycle = 0; cycle != cycle_count; ++cycle) {
		if (cycle < create_cycles) {
			failed_count += CreateResetResourceBatch(tests);
			if (cycle == 1) loaded_after_create = zest_GetDeviceMemoryStats(tests->device).blocks_in_use;
		}

		ResetDeviceCycle(tests);

		zest_uint validation_errors = zest_GetValidationErrorCount(tests->device);
		if (validation_errors) {
			ZEST_PRINT("\tReset stability: %u validation errors in cycle %i", validation_errors, cycle);
			failed_count++;
		}
		zest_ResetValidationErrors(tests->device);
		stats[cycle] = CaptureResetStats(tests, &histograms[cycle]);
	}

	//Zest-owned buckets must hold the same block count from the second reset on, in both phases.
	for (int cycle = 2; cycle != cycle_count; ++cycle) {
		if (!ZestBucketsEqual(&stats[1], &stats[cycle])) {
			ZEST_PRINT("\tReset stability: Zest-owned memory footprint changed between cycle 1 and cycle %i", cycle);
			PrintCensusDiff("cycle drift", &stats[1], &stats[cycle]);
			PrintBlockHistogramDiff(tests, &histograms[1], &histograms[cycle]);
			failed_count++;
		}
	}

	//Driver leak test: once creation stops, the driver block count must not climb any further. The
	//first drain cycle already holds all the legitimate per-object retention from the create phase,
	//so every later drain cycle is compared against it and must not exceed it.
	int first_drain = create_cycles;
	int driver_climbed = 0;
	for (int cycle = first_drain + 1; cycle != cycle_count; ++cycle) {
		if (stats[cycle].device.driver_blocks > stats[first_drain].device.driver_blocks ||
			stats[cycle].context.driver_blocks > stats[first_drain].context.driver_blocks) {
			driver_climbed = 1;
		}
	}
	if (driver_climbed) {
		ZEST_PRINT("\tReset stability: driver host allocations kept climbing after creation stopped - Zest is not destroying a driver-backed object on reset");
		//Per-cycle counts so the create-phase climb and the drain-phase plateau (or continued climb)
		//are both visible.
		for (int cycle = 0; cycle != cycle_count; ++cycle) {
			ZEST_PRINT("\t  driver block count after reset %i (%s): %i (device) / %i (context)", cycle,
				cycle < create_cycles ? "create" : "drain", stats[cycle].device.driver_blocks, stats[cycle].context.driver_blocks);
		}
		//Attribute the drain-phase growth to a category+command so the leaking subsystem is named.
		PrintBlockHistogramDiff(tests, &histograms[first_drain], &histograms[cycle_count - 1]);
		failed_count++;
	}

	//Sanity check on the test itself: the batch must actually raise the block count above the
	//post-reset baseline or the comparison above proves nothing
	if (loaded_after_create <= CensusTotalBlocks(&stats[1].device)) {
		ZEST_PRINT("\tReset stability: resource batch did not raise blocks in use (%i loaded vs %i after reset)", loaded_after_create, CensusTotalBlocks(&stats[1].device));
		failed_count++;
	}

	test->result = failed_count > 0 ? 1 : 0;
	test->frame_count++;
	return test->result;
}

/*
Reset With Pending Deferred Frees: zest_FreeBuffer and zest_FreeImage park resources on the
per-frame-in-flight deferred freeing lists rather than freeing immediately. Populate the lists in
two different frame in flight slots and reset while both are still pending - zest_ResetDevice has
an explicit path that drains these lists which is otherwise only hit by timing luck. The lists
must end up empty, the device must be fully usable afterwards and repeating the cycle must land
on an identical memory footprint.
*/
int test__device_reset_deferred_frees(ZestTests *tests, Test *test) {
	const int cycle_count = 3;
	ResetCycleStats stats[cycle_count];
	int failed_count = 0;
	zest_device device = tests->device;

	for (int cycle = 0; cycle != cycle_count; ++cycle) {
		zest_buffer_info_t storage_buffer_info = zest_CreateBufferInfo(zest_buffer_type_storage, zest_memory_usage_gpu_only);
		zest_image_info_t image_info = zest_CreateImageInfo(64, 64);
		image_info.flags = zest_image_preset_texture;

		//Batch A lands in the current frame in flight's deferred lists
		zest_buffer buffer_a = zest_CreateBuffer(device, zloc__KILOBYTE(128), &storage_buffer_info);
		zest_buffer staging_a = zest_CreateStagingBuffer(device, zloc__KILOBYTE(32), 0);
		zest_image_handle image_a = zest_CreateImage(device, &image_info);
		if (!buffer_a || !staging_a || !zest_GetImage(image_a)) failed_count++;
		zest_FreeBuffer(buffer_a);
		zest_FreeBuffer(staging_a);
		zest_FreeImage(image_a);
		zest_uint fif_a = device->frame_counter % ZEST_MAX_FIF;

		//Advance one frame so batch B parks in a different slot. UpdateDevice drains the slot it
		//advances into, not the one batch A is waiting in, so both stay pending.
		zest_UpdateDevice(device);
		zest_buffer buffer_b = zest_CreateBuffer(device, zloc__KILOBYTE(64), &storage_buffer_info);
		zest_image_handle image_b = zest_CreateImage(device, &image_info);
		if (!buffer_b || !zest_GetImage(image_b)) failed_count++;
		zest_FreeBuffer(buffer_b);
		zest_FreeImage(image_b);
		zest_uint fif_b = device->frame_counter % ZEST_MAX_FIF;

		//Both slots must actually be populated before the reset or the test proves nothing
		if (fif_a == fif_b) failed_count++;
		if (zest_vec_size(device->deferred_resource_freeing_list.buffers[fif_a]) < 2) failed_count++;
		if (zest_vec_size(device->deferred_resource_freeing_list.resources[fif_a]) < 1) failed_count++;
		if (zest_vec_size(device->deferred_resource_freeing_list.buffers[fif_b]) < 1) failed_count++;
		if (zest_vec_size(device->deferred_resource_freeing_list.resources[fif_b]) < 1) failed_count++;
		//...and the deferred entries must still be live allocations (deferred, not already freed)
		if (buffer_a) {
			void *proxy_allocation = (char *)buffer_a - zloc__MINIMUM_BLOCK_SIZE;
			if (zloc__is_free_block(zloc__block_from_allocation(proxy_allocation))) failed_count++;
		}

		ResetDeviceCycle(tests);

		//The reset must have drained every pending free that was queued above. It then re-creates its
		//own default resources (default images + debug font) at the tail of the reset, and those
		//upload pixels through staging buffers that are deferred-freed into the current frame in
		//flight (reset zeroes frame_counter, so that is fif 0). So the current fif legitimately
		//carries a few default-recreation frees afterwards - that is not a leak. What must hold is
		//that every *other* fif slot, including the one batch B parked in, was fully drained to NULL.
		zest_uint post_reset_fif = device->frame_counter % ZEST_MAX_FIF;
		zest_ForEachFrameInFlight(fif) {
			if (fif == post_reset_fif) continue;
			if (device->deferred_resource_freeing_list.buffers[fif] != NULL) failed_count++;
			if (device->deferred_resource_freeing_list.resources[fif] != NULL) failed_count++;
		}

		zest_uint validation_errors = zest_GetValidationErrorCount(device);
		if (validation_errors) {
			ZEST_PRINT("\tDeferred frees: %u validation errors in cycle %i", validation_errors, cycle);
			failed_count++;
		}
		zest_ResetValidationErrors(device);
		stats[cycle] = CaptureResetStats(tests, NULL);

		//The device must be fully usable after resetting with pending frees
		zest_buffer check_buffer = zest_CreateBuffer(device, zloc__KILOBYTE(4), &storage_buffer_info);
		if (!check_buffer) failed_count++;
		zest_FreeBufferNow(check_buffer);
	}

	//Draining the deferred lists must land the Zest-owned footprint back on the same baseline. The
	//driver bucket is left out of this equality (it is out of Zest's control across resets).
	if (!ZestBucketsEqual(&stats[1], &stats[2])) {
		ZEST_PRINT("\tDeferred frees: Zest-owned memory footprint changed between cycles");
		PrintCensusDiff("cycle drift", &stats[1], &stats[2]);
		failed_count++;
	}

	test->result = failed_count > 0 ? 1 : 0;
	test->frame_count++;
	return test->result;
}

/*
Post-Reset Pool Sizing: zest_ResetDevice frees the registered pool size map, and without
repopulating it every buffer allocator built after a reset would fall into the "Derived Pool"
fallback with its 32k minimum allocation size, silently oversizing small buffers. Regression test:
after a reset, small buffers must come from the registered small-buffer pools with their 1k
minimum allocation.
*/
int test__device_reset_pool_sizing(ZestTests *tests, Test *test) {
	int failed_count = 0;
	zest_device device = tests->device;

	for (int cycle = 0; cycle != 2; ++cycle) {
		ResetDeviceCycle(tests);

		//Small device-local buffer: registered small pool, not the derived fallback
		zest_buffer_info_t storage_buffer_info = zest_CreateBufferInfo(zest_buffer_type_storage, zest_memory_usage_gpu_only);
		zest_buffer small_device = zest_CreateBuffer(device, 512, &storage_buffer_info);
		if (!small_device) {
			failed_count++;
		} else {
			zest_buffer_allocator allocator = small_device->memory_pool->allocator;
			if (strcmp(allocator->name, "Small Device Buffers") != 0) {
				ZEST_PRINT("\tPool sizing: small device buffer served by '%s' in cycle %i", allocator->name, cycle);
				failed_count++;
			}
			//The registered 1k minimum, rounded up to the offset granularity - not the derived 32k
			zest_size expected_minimum = zloc__align_size_up(zloc__KILOBYTE(1), allocator->offset_granularity);
			if (allocator->pre_defined_pool_size.minimum_allocation_size != expected_minimum) {
				ZEST_PRINT("\tPool sizing: small device buffer minimum allocation %llu, expected %llu (cycle %i)",
					(unsigned long long)allocator->pre_defined_pool_size.minimum_allocation_size, (unsigned long long)expected_minimum, cycle);
				failed_count++;
			}
		}

		//Small host-visible staging buffer likewise
		zest_buffer small_host = zest_CreateStagingBuffer(device, 512, 0);
		if (!small_host) {
			failed_count++;
		} else {
			zest_buffer_allocator allocator = small_host->memory_pool->allocator;
			if (strcmp(allocator->name, "Small Host buffers") != 0) {
				ZEST_PRINT("\tPool sizing: small host buffer served by '%s' in cycle %i", allocator->name, cycle);
				failed_count++;
			}
		}

		zest_FreeBufferNow(small_device);
		zest_FreeBufferNow(small_host);
	}

	test->result = failed_count > 0 ? 1 : 0;
	test->result |= zest_GetValidationErrorCount(tests->device);
	test->frame_count++;
	return test->result;
}

/*
Bindless Indexes After Reset: Acquire a spread of bindless indexes without releasing any of them,
reset, and verify the whole index space came back: the per-binding next_new_index counters must
match the post-reset baseline every cycle and re-acquiring must hand out exactly the same index
values. If indexes leak across resets these values creep upwards instead.
*/
int test__device_reset_bindless_indexes(ZestTests *tests, Test *test) {
	const int cycle_count = 3;
	zest_uint baseline_storage_counter[cycle_count];
	zest_uint baseline_texture_counter[cycle_count];
	zest_uint first_storage_index[cycle_count];
	zest_uint first_texture_index[cycle_count];
	int failed_count = 0;
	zest_device device = tests->device;

	for (int cycle = 0; cycle != cycle_count; ++cycle) {
		ResetDeviceCycle(tests);

		//Straight after a reset only the device defaults (default images, debug font) have
		//consumed indexes, so these counters must be identical every cycle
		baseline_storage_counter[cycle] = device->bindless_set_layout->descriptor_indexes[zest_storage_buffer_binding].next_new_index;
		baseline_texture_counter[cycle] = device->bindless_set_layout->descriptor_indexes[zest_texture_2d_binding].next_new_index;

		zest_buffer_info_t storage_buffer_info = zest_CreateBufferInfo(zest_buffer_type_storage, zest_memory_usage_gpu_only);
		zest_buffer buffer = zest_CreateBuffer(device, zloc__KILOBYTE(4), &storage_buffer_info);
		//Created with pixels so the image is transitioned to a shader-readable layout, which
		//acquiring a sampled index requires
		zest_byte pixels[64 * 64 * 4];
		memset(pixels, 0x5A, sizeof(pixels));
		zest_image_info_t image_info = zest_CreateImageInfo(64, 64);
		image_info.flags = zest_image_preset_texture;
		zest_image_handle image_handle = zest_CreateImageWithPixels(device, pixels, sizeof(pixels), &image_info);
		zest_image image = zest_GetImage(image_handle);
		if (!buffer || !image) {
			failed_count++;
			continue;
		}

		first_storage_index[cycle] = zest_AcquireStorageBufferIndex(device, buffer);
		first_texture_index[cycle] = zest_AcquireSampledImageIndex(device, image, zest_texture_2d_binding);
		if (first_storage_index[cycle] == ZEST_INVALID) failed_count++;
		if (first_texture_index[cycle] == ZEST_INVALID) failed_count++;

		//Burn through a block of indexes and never release them: the reset must reclaim the lot
		for (int i = 0; i != 100; ++i) {
			if (zest_AcquireStorageBufferIndex(device, buffer) == ZEST_INVALID) failed_count++;
		}
	}

	for (int cycle = 1; cycle != cycle_count; ++cycle) {
		if (baseline_storage_counter[cycle] != baseline_storage_counter[0]) {
			ZEST_PRINT("\tBindless indexes: storage buffer counter after reset was %u in cycle %i, expected %u", baseline_storage_counter[cycle], cycle, baseline_storage_counter[0]);
			failed_count++;
		}
		if (baseline_texture_counter[cycle] != baseline_texture_counter[0]) {
			ZEST_PRINT("\tBindless indexes: texture counter after reset was %u in cycle %i, expected %u", baseline_texture_counter[cycle], cycle, baseline_texture_counter[0]);
			failed_count++;
		}
		if (first_storage_index[cycle] != first_storage_index[0]) {
			ZEST_PRINT("\tBindless indexes: first storage index was %u in cycle %i, expected %u", first_storage_index[cycle], cycle, first_storage_index[0]);
			failed_count++;
		}
		if (first_texture_index[cycle] != first_texture_index[0]) {
			ZEST_PRINT("\tBindless indexes: first texture index was %u in cycle %i, expected %u", first_texture_index[cycle], cycle, first_texture_index[0]);
			failed_count++;
		}
	}

	test->result = failed_count > 0 ? 1 : 0;
	test->result |= zest_GetValidationErrorCount(tests->device);
	test->frame_count++;
	return test->result;
}
