#include "zest-tests.h"

//Test image format support
int test__image_format_support(ZestTests *tests, Test *test) {
	zest_image_info_t image_info = zest_CreateImageInfo(512, 512);
	image_info.flags = zest_image_preset_texture;

	//Valid formats that should work with tiling optimal
	zest_format formats[] = {
		zest_format_r8_unorm,
		zest_format_r8g8_unorm,
		zest_format_r8g8b8a8_unorm,
		zest_format_b8g8r8a8_unorm,
		zest_format_r8g8b8a8_srgb,
		zest_format_b8g8r8a8_srgb,
		zest_format_r16_sfloat,
		zest_format_r16g16_sfloat,
		zest_format_r16g16b16a16_sfloat,
		zest_format_r32_sfloat,
		zest_format_r32g32_sfloat,
		zest_format_r32g32b32a32_sfloat
	};

	int failed_count = 0;

	for(int i = 0; i < 12; i++) {
		image_info.format = formats[i];
		zest_image_handle image = zest_CreateImage(tests->context, &image_info);
		if(image.value == 0) {
			failed_count++;
		}
		zest_FreeImage(image);
	}

	test->result = failed_count > 0 ? 1 : 0;
	test->result |= zest_GetValidationErrorCount(tests->context);
	test->frame_count++;
	return test->result;
}

//Test image creation and freeing functionality
int test__image_creation_destruction(ZestTests *tests, Test *test) {
	const int frames_to_test = 10; // Internal frame count variable
	
	// Phase 1: Basic creation and immediate destruction
	zest_image_info_t immediate_image_info = zest_CreateImageInfo(256, 256);
	immediate_image_info.format = zest_format_r8_unorm;
	immediate_image_info.flags = zest_image_preset_texture;
	
	zest_image_handle immediate_image = zest_CreateImage(tests->context, &immediate_image_info);
	if(immediate_image.value == 0) {
		return 1;
	}
	zest_FreeImageNow(immediate_image); // Immediate destruction
	
	// Phase 2: Frame-based lifecycle testing
	zest_image_info_t persistent_image_info = zest_CreateImageInfo(512, 512);
	persistent_image_info.format = zest_format_r8_unorm;
	persistent_image_info.flags = zest_image_preset_texture;
	
	zest_image_handle persistent_image = zest_CreateImage(tests->context, &persistent_image_info);
	if(persistent_image.value == 0) {
		return 1;
	}
	
	// Use image for multiple frames before freeing
	for (int frame = 0; frame < frames_to_test; frame++) {
		zest_UpdateDevice(tests->device);
		zest_BeginFrame(tests->context);
		zest_EndFrame(tests->context);
	}
	
	// Phase 3: Multiple image management
	const int test_image_count = 10;
	zest_image_handle test_images[test_image_count];
	int created_images = 0;
	
	// Create multiple images
	for (int i = 0; i < test_image_count; i++) {
		zest_image_info_t info = zest_CreateImageInfo(128, 128);
		info.format = zest_format_r8_unorm;
		info.flags = zest_image_preset_texture;
		test_images[i] = zest_CreateImage(tests->context, &info);
		if(test_images[i].value == 0) {
			// Cleanup created images before failing
			for (int j = 0; j < i; j++) {
				zest_FreeImageNow(test_images[j]);
			}
			zest_FreeImageNow(persistent_image);
			return 1;
		}
		created_images++;
	}
	
	// Free images in different patterns across a few frames
	int resources_freed = 0;
	for (int cleanup_frame = 0; cleanup_frame < 3; cleanup_frame++) {
		resources_freed += zest_UpdateDevice(tests->device);
		zest_BeginFrame(tests->context);
		zest_EndFrame(tests->context);
		
		// Free some images each frame to test deferred destruction
		int start_idx = cleanup_frame * 3;
		int end_idx = start_idx + 3;
		if (end_idx > test_image_count) end_idx = test_image_count;
		
		for (int i = start_idx; i < end_idx; i++) {
			zest_FreeImage(test_images[i]);
		}
	}

	for (int cleanup_frame = 0; cleanup_frame < ZEST_MAX_FIF; cleanup_frame++) {
		resources_freed += zest_UpdateDevice(tests->device);
	}

	// Final cleanup
	zest_FreeImageNow(persistent_image);
	zest_FreeImageNow(test_images[test_image_count - 1]);

	int image_count = zest_GetDeviceResourceCount(tests->device, zest_handle_type_images);
	
	// Final validation
	test->result = zest_GetValidationErrorCount(tests->context);
	test->result |= resources_freed == 9 ? 0 : 1;
	test->result |= image_count == 0 ? 0 : 1;
	test->frame_count++; 
	return test->result;
}

//Test image format validation edge cases
int test__image_format_validation_edge_cases(ZestTests *tests, Test *test) {
	int total_tests = 0;
	int passed_tests = 0;
	
	// Phase 1: Invalid format values
	struct {
		zest_format format;
		const char* description;
	} invalid_formats[] = {
		{zest_format_undefined, "undefined format"},
		{(zest_format)(-1), "negative format value"},
		{(zest_format)10000, "very large format value"},
		{(zest_format)0x7FFFFFFF, "max 32-bit format value"}
	};
	
	for (int i = 0; i < 4; i++) {
		zest_image_info_t image_info = zest_CreateImageInfo(256, 256);
		image_info.format = invalid_formats[i].format;
		image_info.flags = zest_image_preset_texture;
		
		zest_image_handle image = zest_CreateImage(tests->context, &image_info);
		
		// Expected to fail gracefully (no crash, no assert)
		if (image.value != 0) {
			// Unexpected success - this should have failed
			zest_FreeImage(image); // Cleanup if it somehow succeeded
		} else {
			// Expected failure - this is good
			passed_tests++;
		}
		total_tests++;
	}
	
	// Phase 2: Format usage compatibility conflicts
	struct {
		zest_format format;
		zest_image_flags usage;
		const char* description;
	} conflict_tests[] = {
		// Depth format with color attachment usage
		{zest_format_d32_sfloat, zest_image_flag_color_attachment, "depth format as color attachment"},

		// Compressed format with render target usage
		{zest_format_bc1_rgba_unorm_block, zest_image_flag_color_attachment, "compressed as render target"},

		// Color format with depth/stencil usage
		{zest_format_r8g8b8a8_unorm, zest_image_flag_depth_stencil_attachment, "color as depth attachment"}
	};
	
	for (int i = 0; i < 3; i++) {
		zest_image_info_t image_info = zest_CreateImageInfo(256, 256);
		image_info.format = conflict_tests[i].format;
		image_info.flags = conflict_tests[i].usage;
		
		zest_image_handle image = zest_CreateImage(tests->context, &image_info);
		
		// Expected to fail gracefully
		if (image.value != 0) {
			// Unexpected success - this should have failed
			zest_FreeImage(image); // Cleanup if it somehow succeeded
		} else {
			// Expected failure - this is good
			passed_tests++;
		}
		total_tests++;
	}
	
	// Phase 3: Usage flag edge cases
	struct {
		zest_image_flags usage;
		const char* description;
	} usage_edge_cases[] = {
		{0, "no usage flags (should handle gracefully)"},
		{(zest_image_flags)0xFFFFFFFF, "all possible usage flags"}
	};
	
	for (int i = 0; i < 2; i++) {
		zest_image_info_t image_info = zest_CreateImageInfo(256, 256);
		image_info.format = zest_format_r8g8b8a8_unorm; // Valid format
		image_info.flags = usage_edge_cases[i].usage;
		
		zest_image_handle image = zest_CreateImage(tests->context, &image_info);
		
		if (image.value != 0) {
			// This might succeed or fail depending on implementation
			// Just verify no crash and cleanup properly
			zest_FreeImage(image);
			passed_tests++; // Handled gracefully regardless of success/failure
		} else {
			passed_tests++; // Handled gracefully
		}
		total_tests++;
	}
	
	// Test a few additional edge case formats beyond the 12 optimal tiling formats
	struct {
		zest_format format;
		const char* description;
	} additional_edge_formats[] = {
		{zest_format_d16_unorm, "16-bit depth format"},
		{zest_format_d24_unorm_s8_uint, "24-bit depth with stencil"},
		{zest_format_bc3_unorm_block, "BC3 compressed format"},
		{zest_format_astc_4X4_unorm_block, "ASTC 4x4 compressed"}
	};
	
	for (int i = 0; i < 4; i++) {
		zest_image_info_t image_info = zest_CreateImageInfo(256, 256);
		image_info.format = additional_edge_formats[i].format;
		image_info.flags = zest_image_preset_texture;
		
		zest_image_handle image = zest_CreateImage(tests->context, &image_info);
		
		// Should handle gracefully regardless of support
		if (image.value != 0) {
			zest_FreeImage(image);
			passed_tests++; // Successfully created or gracefully rejected
		} else {
			passed_tests++; // Gracefully handled unsupported format
		}
		total_tests++;
	}
	
	// Final validation
	test->result = (passed_tests == total_tests) ? 0 : 1;
	test->result |= zest_GetValidationErrorCount(tests->context) == 6 ? 0 : 1;
	test->frame_count++;
	return test->result;
}

int test__image_view_creation(ZestTests *tests, Test *test) {
	int failed_count = 0;

	// Phase 1: Create a simple image and verify default view exists
	zest_image_info_t simple_info = zest_CreateImageInfo(256, 256);
	simple_info.format = zest_format_r8g8b8a8_unorm;
	simple_info.flags = zest_image_preset_texture;

	zest_image_handle simple_image_handle = zest_CreateImage(tests->context, &simple_info);
	if (simple_image_handle.value == 0) {
		return 1; // Failed to create base image
	}

	zest_image simple_image = zest_GetImage(simple_image_handle);
	if (!simple_image || !simple_image->default_view) {
		failed_count++;
	}

	// Phase 2: Create an image with mipmaps for view testing
	zest_image_info_t mipped_info = zest_CreateImageInfo(512, 512);
	mipped_info.format = zest_format_r8g8b8a8_unorm;
	mipped_info.flags = zest_image_preset_texture_mipmaps;

	zest_image_handle mipped_image_handle = zest_CreateImage(tests->context, &mipped_info);
	if (mipped_image_handle.value == 0) {
		zest_FreeImageNow(simple_image_handle);
		return 1;
	}

	zest_image mipped_image = zest_GetImage(mipped_image_handle);
	if (!mipped_image || mipped_image->info.mip_levels <= 1) {
		failed_count++; // Should have multiple mip levels
	}

	// Phase 3: Create custom image view with default settings
	zest_image_view_create_info_t view_info = zest_CreateViewImageInfo(mipped_image);
	zest_image_view_handle custom_view = zest_CreateImageView(tests->context, mipped_image, &view_info);
	if (custom_view.value == 0) {
		failed_count++;
	}

	// Phase 4: Create image view for specific mip range
	zest_image_view_create_info_t mip_view_info = zest_CreateViewImageInfo(mipped_image);
	mip_view_info.base_mip_level = 1;
	mip_view_info.level_count = 2; // Just 2 mip levels starting from level 1
	zest_image_view_handle mip_range_view = zest_CreateImageView(tests->context, mipped_image, &mip_view_info);
	if (mip_range_view.value == 0) {
		failed_count++;
	}

	// Phase 5: Create per-mip image view array
	zest_image_view_array_handle view_array = zest_CreateImageViewsPerMip(tests->context, mipped_image);
	if (view_array.value == 0) {
		failed_count++;
	}

	// Phase 6: Verify we can retrieve the views
	zest_image_view retrieved_view = zest_GetImageView(custom_view);
	if (!retrieved_view) {
		failed_count++;
	}

	zest_image_view_array retrieved_array = zest_GetImageViewArray(view_array);
	if (!retrieved_array) {
		failed_count++;
	}

	// Phase 7: Test with different image formats
	zest_image_info_t r16_info = zest_CreateImageInfo(128, 128);
	r16_info.format = zest_format_r16_sfloat;
	r16_info.flags = zest_image_preset_texture;

	zest_image_handle r16_image_handle = zest_CreateImage(tests->context, &r16_info);
	if (r16_image_handle.value == 0) {
		failed_count++;
	} else {
		zest_image r16_image = zest_GetImage(r16_image_handle);
		zest_image_view_create_info_t r16_view_info = zest_CreateViewImageInfo(r16_image);
		zest_image_view_handle r16_view = zest_CreateImageView(tests->context, r16_image, &r16_view_info);
		if (r16_view.value == 0) {
			failed_count++;
		} else {
			zest_FreeImageViewNow(r16_view);
		}
		zest_FreeImageNow(r16_image_handle);
	}

	// Phase 8: Count views created
	int view_count = zest_GetDeviceResourceCount(tests->device, zest_handle_type_views);
	int view_array_count = zest_GetDeviceResourceCount(tests->device, zest_handle_type_view_arrays);

	// Cleanup
	zest_FreeImageNow(simple_image_handle);
	zest_FreeImageNow(mipped_image_handle);
	zest_FreeImageViewNow(custom_view);
	zest_FreeImageViewArrayNow(view_array);
	zest_FreeImageViewNow(mip_range_view);

	// Final validation
	test->result = failed_count > 0 ? 1 : 0;
	test->result |= view_count == 2 ? 0 : 1;
	test->result |= view_array_count == 1 ? 0 : 1;
	test->result |= zest_GetValidationErrorCount(tests->context);
	test->frame_count++;
	return test->result;
}

//Test basic buffer creation and destruction
int test__buffer_creation_destruction(ZestTests *tests, Test *test) {
	int failed_count = 0;

	// Phase 1: Basic creation with different buffer types (GPU-only)
	zest_buffer_type buffer_types[] = {
		zest_buffer_type_vertex,
		zest_buffer_type_index,
		zest_buffer_type_uniform,
		zest_buffer_type_storage,
		zest_buffer_type_indirect,
		zest_buffer_type_vertex_storage,
		zest_buffer_type_index_storage
	};

	for (int i = 0; i < 7; i++) {
		zest_buffer_info_t info = zest_CreateBufferInfo(buffer_types[i], zest_memory_usage_gpu_only);
		zest_buffer buffer = zest_CreateBuffer(tests->context, 1024, &info);
		if (!buffer) {
			failed_count++;
		}
		zest_FreeBuffer(buffer);
	}

	// Phase 2: Memory usage combinations
	struct { zest_buffer_type type; zest_memory_usage usage; } combos[] = {
		{zest_buffer_type_vertex, zest_memory_usage_gpu_only},
		{zest_buffer_type_uniform, zest_memory_usage_cpu_to_gpu},
		{zest_buffer_type_storage, zest_memory_usage_gpu_to_cpu},
		{zest_buffer_type_staging, zest_memory_usage_cpu_to_gpu}
	};

	for (int i = 0; i < 4; i++) {
		zest_buffer_info_t info = zest_CreateBufferInfo(combos[i].type, combos[i].usage);
		zest_buffer buffer = zest_CreateBuffer(tests->context, 1024, &info);
		if (!buffer) {
			failed_count++;
		}
		zest_FreeBuffer(buffer);
	}

	// Phase 3: Staging buffer creation
	zest_buffer staging_null = zest_CreateStagingBuffer(tests->context, 512, NULL);
	if (!staging_null) failed_count++;
	zest_FreeBuffer(staging_null);

	char test_data[64] = {0};
	zest_buffer staging_with_data = zest_CreateStagingBuffer(tests->context, 64, test_data);
	if (!staging_with_data) failed_count++;
	zest_FreeBuffer(staging_with_data);

	// Phase 4: Multiple buffer management
	const int multi_count = 10;
	zest_buffer multi_buffers[10];
	int created = 0;

	for (int i = 0; i < multi_count; i++) {
		zest_buffer_info_t info = zest_CreateBufferInfo(zest_buffer_type_storage, zest_memory_usage_gpu_only);
		multi_buffers[i] = zest_CreateBuffer(tests->context, 256, &info);
		if (multi_buffers[i]) created++;
	}

	if (created != multi_count) failed_count++;

	for (int i = 0; i < multi_count; i++) {
		zest_FreeBuffer(multi_buffers[i]);
	}

	// Phase 5: Buffer resize/grow operations
	zest_buffer_info_t grow_info = zest_CreateBufferInfo(zest_buffer_type_storage, zest_memory_usage_gpu_only);
	zest_buffer grow_buffer = zest_CreateBuffer(tests->context, 1024, &grow_info);
	if (!grow_buffer) {
		failed_count++;
	} else {
		zest_bool grew = zest_GrowBuffer(&grow_buffer, 1, 4096);
		zest_size new_size = zest_GetBufferSize(grow_buffer);
		if (!grew || new_size < 4096) failed_count++;
		zest_FreeBuffer(grow_buffer);
	}

	// Phase 6: Different buffer sizes
	zest_size sizes[] = {64, 4096, 1024 * 1024};
	for (int i = 0; i < 3; i++) {
		zest_buffer_info_t info = zest_CreateBufferInfo(zest_buffer_type_storage, zest_memory_usage_gpu_only);
		zest_buffer buffer = zest_CreateBuffer(tests->context, sizes[i], &info);
		if (!buffer) {
			failed_count++;
		} else {
			if (zest_GetBufferSize(buffer) < sizes[i]) failed_count++;
			zest_FreeBuffer(buffer);
		}
	}

	test->result = failed_count > 0 ? 1 : 0;
	test->result |= zest_GetValidationErrorCount(tests->context);
	test->frame_count++;
	return test->result;
}

//Test buffer edge cases and validation
int test__buffer_edge_cases(ZestTests *tests, Test *test) {
	int passed_tests = 0;
	int total_tests = 0;

	// Phase 1: Size edge cases
	// Size = 0 (may round up to minimum or fail)
	{
		zest_buffer_info_t info = zest_CreateBufferInfo(zest_buffer_type_storage, zest_memory_usage_gpu_only);
		zest_buffer buffer = zest_CreateBuffer(tests->context, 0, &info);
		if (buffer) {
			// Size=0 was accepted and rounded up - this is valid behavior
			zest_FreeBuffer(buffer);
			passed_tests++;
		} else {
			// Size=0 was rejected - also valid behavior
			passed_tests++;
		}
		total_tests++;
	}

	// Size = 1 (minimum valid)
	{
		zest_buffer_info_t info = zest_CreateBufferInfo(zest_buffer_type_storage, zest_memory_usage_gpu_only);
		zest_buffer buffer = zest_CreateBuffer(tests->context, 1, &info);
		if (buffer) {
			passed_tests++;
			zest_FreeBuffer(buffer);
		}
		total_tests++;
	}

	// Phase 2: Invalid enum values
	struct { zest_buffer_type type; zest_memory_usage usage; const char* desc; } invalid_combos[] = {
		{(zest_buffer_type)(-1), zest_memory_usage_gpu_only, "negative buffer type"},
		{(zest_buffer_type)(100), zest_memory_usage_gpu_only, "out of range buffer type"},
		{zest_buffer_type_storage, (zest_memory_usage)(-1), "negative memory usage"},
		{zest_buffer_type_storage, (zest_memory_usage)(100), "out of range memory usage"}
	};

	for (int i = 0; i < 4; i++) {
		zest_buffer_info_t info = zest_CreateBufferInfo(invalid_combos[i].type, invalid_combos[i].usage);
		zest_buffer buffer = zest_CreateBuffer(tests->context, 1024, &info);
		// Should handle gracefully regardless of success or failure
		if (buffer) {
			zest_FreeBuffer(buffer);
		}
		passed_tests++; // Handled gracefully (no crash)
		total_tests++;
	}

	// Phase 3: NULL pointer handling
	// zest_FreeBuffer(NULL) should be safe
	zest_FreeBuffer(NULL);
	passed_tests++; // Didn't crash
	total_tests++;

	// Phase 4: Double-free protection
	{
		zest_buffer_info_t info = zest_CreateBufferInfo(zest_buffer_type_storage, zest_memory_usage_gpu_only);
		zest_buffer buffer = zest_CreateBuffer(tests->context, 512, &info);
		if (buffer) {
			zest_FreeBuffer(buffer);
			zest_FreeBuffer(buffer); // Second free should be safe
			passed_tests++; // Didn't crash
		} else {
			passed_tests++; // Creation failed but handled gracefully
		}
		total_tests++;
	}

	// Phase 5: Resize/Grow edge cases
	{
		zest_buffer_info_t info = zest_CreateBufferInfo(zest_buffer_type_storage, zest_memory_usage_gpu_only);
		zest_buffer buffer = zest_CreateBuffer(tests->context, 1024, &info);
		if (buffer) {
			// Try to shrink (should return FALSE)
			zest_bool shrink_result = zest_ResizeBuffer(&buffer, 512);
			if (shrink_result == ZEST_FALSE) {
				passed_tests++; // Correctly rejected shrink
			}
			total_tests++;

			// Try to resize to same size
			zest_size current_size = zest_GetBufferSize(buffer);
			zest_bool same_result = zest_ResizeBuffer(&buffer, current_size);
			// Either succeeds or returns FALSE - both are valid
			passed_tests++;
			total_tests++;

			// GrowBuffer when already larger than minimum
			zest_bool grow_result = zest_GrowBuffer(&buffer, 1, 256);
			if (grow_result == ZEST_FALSE) {
				passed_tests++; // Correctly rejected - already larger
			} else {
				passed_tests++; // Grew anyway - also valid
			}
			total_tests++;

			zest_FreeBuffer(buffer);
		}
	}

	// Phase 6: Very large allocation (may fail gracefully)
	{
		zest_buffer_info_t info = zest_CreateBufferInfo(zest_buffer_type_storage, zest_memory_usage_gpu_only);
		zest_buffer buffer = zest_CreateBuffer(tests->context, (zest_size)1024 * 1024 * 1024, &info); // 1GB
		if (buffer) {
			// Large allocation succeeded
			zest_FreeBuffer(buffer);
		}
		// Either way, handled gracefully
		passed_tests++;
		total_tests++;
	}

	test->result = (passed_tests == total_tests) ? 0 : 1;
	test->result |= zest_GetValidationErrorCount(tests->context) == 1 ? 0 : 1;
	test->frame_count++;
	return test->result;
}

//Test uniform buffer creation and descriptor index management
int test__uniform_buffer_descriptor_management(ZestTests *tests, Test *test) {
	int failed_count = 0;

	// Phase 1: Basic creation
	zest_uniform_buffer_handle ub_handle = zest_CreateUniformBuffer(tests->context, "test_uniform", 64);
	if (ub_handle.value == 0) {
		return 1; // Failed to create
	}

	zest_uniform_buffer ub = zest_GetUniformBuffer(ub_handle);
	if (!ub) {
		failed_count++;
	}

	// Verify descriptor index is valid
	zest_uint index = zest_GetUniformBufferDescriptorIndex(ub);
	if (index == ZEST_INVALID) {
		failed_count++;
	}

	// Phase 2: Multiple uniform buffers get unique indices
	zest_uniform_buffer_handle ub2_handle = zest_CreateUniformBuffer(tests->context, "test_uniform2", 64);
	zest_uniform_buffer_handle ub3_handle = zest_CreateUniformBuffer(tests->context, "test_uniform3", 64);

	zest_uniform_buffer ub2 = zest_GetUniformBuffer(ub2_handle);
	zest_uniform_buffer ub3 = zest_GetUniformBuffer(ub3_handle);

	zest_uint index2 = zest_GetUniformBufferDescriptorIndex(ub2);
	zest_uint index3 = zest_GetUniformBufferDescriptorIndex(ub3);

	// All indices should be unique
	if (index == index2 || index == index3 || index2 == index3) {
		failed_count++;
	}

	// Phase 3: Test descriptor index recycling
	zest_uint original_index = index;
	zest_FreeUniformBuffer(ub_handle);

	// Run cleanup frames to trigger deferred freeing
	for (int frame = 0; frame < ZEST_MAX_FIF + 1; frame++) {
		zest_UpdateDevice(tests->device);
		zest_BeginFrame(tests->context);
		zest_EndFrame(tests->context);
	}

	// Create new uniform buffer - should get recycled index
	zest_uniform_buffer_handle ub4_handle = zest_CreateUniformBuffer(tests->context, "test_uniform4", 64);
	zest_uniform_buffer ub4 = zest_GetUniformBuffer(ub4_handle);
	zest_uint index4 = zest_GetUniformBufferDescriptorIndex(ub4);

	// Index should be recycled (same as original)
	if (index4 != original_index) {
		failed_count++; // Index wasn't recycled
	}

	// Phase 4: Data access
	void *data = zest_GetUniformBufferData(ub4);
	if (!data) {
		failed_count++;
	} else {
		// Write test data
		memset(data, 0xAB, 64);
	}

	test->result = failed_count > 0 ? 1 : 0;
	test->result |= zest_GetValidationErrorCount(tests->context);
	test->frame_count++;
	return test->result;
}

//Test uniform buffer edge cases and validation
int test__uniform_buffer_edge_cases(ZestTests *tests, Test *test) {
	int passed_tests = 0;
	int total_tests = 0;

	// Phase 1: Size edge cases
	// Size = 0
	{
		zest_uniform_buffer_handle ub = zest_CreateUniformBuffer(tests->context, "size_zero", 0);
		if (ub.value != 0) {
			// Created successfully - cleanup
			zest_FreeUniformBuffer(ub);
		}
		passed_tests++; // Handled gracefully (no crash)
		total_tests++;
	}

	// Size = 1 (minimum)
	{
		zest_uniform_buffer_handle ub = zest_CreateUniformBuffer(tests->context, "size_one", 1);
		if (ub.value != 0) {
			zest_uniform_buffer uniform = zest_GetUniformBuffer(ub);
			if (uniform) {
				void *data = zest_GetUniformBufferData(uniform);
				if (data) {
					*(char*)data = 0xAB; // Write single byte
				}
			}
			zest_FreeUniformBuffer(ub);
			passed_tests++;
		}
		total_tests++;
	}

	// Size at limit (64KB)
	{
		zest_uniform_buffer_handle ub = zest_CreateUniformBuffer(tests->context, "size_at_limit", 65536);
		if (ub.value != 0) {
			zest_FreeUniformBuffer(ub);
			passed_tests++;
		}
		total_tests++;
	}

	// Size exceeding typical maxUniformBufferRange (64KB + 1)
	// This should either fail gracefully or produce a validation error
	{
		zest_uniform_buffer_handle ub = zest_CreateUniformBuffer(tests->context, "size_over_limit", 65536 + 1);
		if (ub.value != 0) {
			// Created despite exceeding limit - cleanup
			zest_FreeUniformBuffer(ub);
		}
		passed_tests++; // Handled gracefully (no crash)
		total_tests++;
	}

	// Phase 2: Name edge cases
	// NULL name
	{
		zest_uniform_buffer_handle ub = zest_CreateUniformBuffer(tests->context, NULL, 64);
		if (ub.value != 0) {
			zest_FreeUniformBuffer(ub);
		}
		passed_tests++; // Handled gracefully
		total_tests++;
	}

	// Empty string name
	{
		zest_uniform_buffer_handle ub = zest_CreateUniformBuffer(tests->context, "", 64);
		if (ub.value != 0) {
			zest_FreeUniformBuffer(ub);
		}
		passed_tests++; // Handled gracefully
		total_tests++;
	}

	// Phase 3: Invalid handle operations
	// Zero handle
	{
		zest_uniform_buffer_handle null_handle = { 0, NULL };
		// zest_GetUniformBuffer with invalid handle - may assert or return NULL
		// zest_FreeUniformBuffer with invalid handle - should be safe
		zest_uniform_buffer uniform_buffer = zest_GetUniformBuffer(null_handle);
		zest_FreeUniformBuffer(null_handle);
		passed_tests++;
		total_tests++;
	}

	// Phase 4: Multiple creation/deletion cycles
	{
		const int cycle_count = 5;
		for (int i = 0; i < cycle_count; i++) {
			zest_uniform_buffer_handle ub = zest_CreateUniformBuffer(tests->context, "cycle_test", 128);
			if (ub.value == 0) {
				break; // Failed to create
			}
			zest_FreeUniformBuffer(ub);
		}

		// Run cleanup frames
		for (int frame = 0; frame < ZEST_MAX_FIF + 1; frame++) {
			zest_UpdateDevice(tests->device);
			zest_BeginFrame(tests->context);
			zest_EndFrame(tests->context);
		}

		// Create one more to verify handles are being reused
		zest_uniform_buffer_handle final_ub = zest_CreateUniformBuffer(tests->context, "final_test", 128);
		if (final_ub.value != 0) {
			passed_tests++;
			zest_FreeUniformBuffer(final_ub);
		}
		total_tests++;
	}

	// Phase 5: Data access verification
	{
		zest_uniform_buffer_handle ub = zest_CreateUniformBuffer(tests->context, "data_test", 256);
		if (ub.value != 0) {
			zest_uniform_buffer uniform = zest_GetUniformBuffer(ub);
			if (uniform) {
				void *data = zest_GetUniformBufferData(uniform);
				if (data) {
					// Write pattern
					memset(data, 0xCD, 256);
					// Verify write didn't crash
					passed_tests++;
				}
			}
			zest_FreeUniformBuffer(ub);
		}
		total_tests++;
	}

	test->result = (passed_tests == total_tests) ? 0 : 1;
	test->result |= zest_GetValidationErrorCount(tests->context);
	test->frame_count++;
	return test->result;
}

//Test staging buffer operations
int test__staging_buffer_operations(ZestTests *tests, Test *test) {
	int passed_tests = 0;
	int total_tests = 0;

	// Phase 1: Basic Creation
	// Create staging buffer with NULL data
	{
		zest_buffer staging = zest_CreateStagingBuffer(tests->context, 1024, NULL);
		if (staging) {
			zest_size size = zest_GetBufferSize(staging);
			if (size >= 1024) passed_tests++;
			total_tests++;

			void *data = zest_BufferData(staging);
			if (data) passed_tests++;
			total_tests++;

			zest_FreeBuffer(staging);
		} else {
			total_tests += 2;
		}
	}

	// Create staging buffer with initial data
	{
		char test_data[256];
		memset(test_data, 0xAB, 256);

		zest_buffer staging = zest_CreateStagingBuffer(tests->context, 256, test_data);
		if (staging) {
			char *buffer_data = (char*)zest_BufferData(staging);
			if (buffer_data) {
				// Verify data was copied
				int match = 1;
				for (int i = 0; i < 256; i++) {
					if (buffer_data[i] != (char)0xAB) {
						match = 0;
						break;
					}
				}
				if (match) passed_tests++;
			}
			total_tests++;
			zest_FreeBuffer(staging);
		} else {
			total_tests++;
		}
	}

	// Phase 2: Data Staging Operations
	{
		zest_buffer staging = zest_CreateStagingBuffer(tests->context, 512, NULL);
		if (staging) {
			// Stage data using zest_StageData
			char stage_data[128];
			memset(stage_data, 0xCD, 128);
			zest_StageData(stage_data, staging, 128);

			// Verify data was staged
			char *buffer_data = (char*)zest_BufferData(staging);
			if (buffer_data) {
				int match = 1;
				for (int i = 0; i < 128; i++) {
					if (buffer_data[i] != (char)0xCD) {
						match = 0;
						break;
					}
				}
				if (match) passed_tests++;
			}
			total_tests++;
			zest_FreeBuffer(staging);
		} else {
			total_tests++;
		}
	}

	// Phase 3: Immediate Buffer-to-Buffer Transfer (round-trip test)
	{
		// Create staging buffer with test pattern
		char test_pattern[256];
		for (int i = 0; i < 256; i++) {
			test_pattern[i] = (char)(i & 0xFF);
		}
		zest_buffer upload_staging = zest_CreateStagingBuffer(tests->context, 256, test_pattern);

		// Create device buffer (GPU-only storage)
		zest_buffer_info_t gpu_info = zest_CreateBufferInfo(zest_buffer_type_storage, zest_memory_usage_gpu_only);
		zest_buffer gpu_buffer = zest_CreateBuffer(tests->context, 256, &gpu_info);

		// Create readback staging buffer (GPU-to-CPU)
		zest_buffer_info_t readback_info = zest_CreateBufferInfo(zest_buffer_type_staging, zest_memory_usage_gpu_to_cpu);
		zest_buffer readback_staging = zest_CreateBuffer(tests->context, 256, &readback_info);

		if (upload_staging && gpu_buffer && readback_staging) {
			// Transfer upload staging -> GPU
			zest_queue queue = zest_BeginImmediateCommandBuffer(tests->device, zest_queue_transfer);
			zest_imm_CopyBuffer(queue, upload_staging, gpu_buffer, 256);
			zest_EndImmediateCommandBuffer(queue);

			// Transfer GPU -> readback staging
			queue = zest_BeginImmediateCommandBuffer(tests->device, zest_queue_transfer);
			zest_imm_CopyBuffer(queue, gpu_buffer, readback_staging, 256);
			zest_EndImmediateCommandBuffer(queue);

			// Verify round-trip data integrity
			char *readback_data = (char*)zest_BufferData(readback_staging);
			if (readback_data) {
				int match = 1;
				for (int i = 0; i < 256; i++) {
					if (readback_data[i] != (char)(i & 0xFF)) {
						match = 0;
						break;
					}
				}
				if (match) passed_tests++;
			}
			total_tests++;
		} else {
			total_tests++;
		}

		if (upload_staging) zest_FreeBuffer(upload_staging);
		if (gpu_buffer) zest_FreeBuffer(gpu_buffer);
		if (readback_staging) zest_FreeBuffer(readback_staging);
	}

	// Phase 4: Multiple Staging Buffers
	{
		const int multi_count = 5;
		zest_buffer multi_staging[5];
		int created = 0;

		for (int i = 0; i < multi_count; i++) {
			multi_staging[i] = zest_CreateStagingBuffer(tests->context, 128 * (i + 1), NULL);
			if (multi_staging[i]) {
				created++;
				// Write unique data to each
				memset(zest_BufferData(multi_staging[i]), (char)(i + 1), 128 * (i + 1));
			}
		}

		// Verify all created and have independent data
		if (created == multi_count) {
			int independent = 1;
			for (int i = 0; i < multi_count; i++) {
				char *data = (char*)zest_BufferData(multi_staging[i]);
				if (data && data[0] != (char)(i + 1)) {
					independent = 0;
					break;
				}
			}
			if (independent) passed_tests++;
		}
		total_tests++;

		// Free all
		for (int i = 0; i < multi_count; i++) {
			if (multi_staging[i]) zest_FreeBuffer(multi_staging[i]);
		}
	}

	// Phase 5: Buffer Resize Operations
	{
		zest_buffer staging = zest_CreateStagingBuffer(tests->context, 256, NULL);
		if (staging) {
			zest_size original_size = zest_GetBufferSize(staging);

			// Resize to larger
			zest_bool resized = zest_ResizeBuffer(&staging, 1024);
			if (resized) {
				zest_size new_size = zest_GetBufferSize(staging);
				if (new_size >= 1024) passed_tests++;
			}
			total_tests++;

			// Stage data at new size
			if (staging) {
				char *data = (char*)zest_BufferData(staging);
				if (data) {
					memset(data, 0xEF, 1024);
					passed_tests++; // Data write at new size succeeded
				}
				total_tests++;
				zest_FreeBuffer(staging);
			} else {
				total_tests++;
			}
		} else {
			total_tests += 2;
		}
	}

	// Phase 6: Edge Cases
	// Size = 0
	{
		zest_buffer staging = zest_CreateStagingBuffer(tests->context, 0, NULL);
		if (staging) {
			zest_FreeBuffer(staging);
		}
		passed_tests++; // Handled gracefully (no crash)
		total_tests++;
	}

	// Size = 1
	{
		char one_byte = 0x42;
		zest_buffer staging = zest_CreateStagingBuffer(tests->context, 1, &one_byte);
		if (staging) {
			char *data = (char*)zest_BufferData(staging);
			if (data && *data == 0x42) passed_tests++;
			total_tests++;
			zest_FreeBuffer(staging);
		} else {
			total_tests++;
		}
	}

	// Large staging buffer (1MB)
	{
		zest_buffer staging = zest_CreateStagingBuffer(tests->context, 1024 * 1024, NULL);
		if (staging) {
			zest_size size = zest_GetBufferSize(staging);
			if (size >= 1024 * 1024) passed_tests++;
			total_tests++;
			zest_FreeBuffer(staging);
		} else {
			total_tests++;
		}
	}

	// Create/free cycles for memory pool stress
	{
		const int cycle_count = 10;
		int successful_cycles = 0;
		for (int i = 0; i < cycle_count; i++) {
			zest_buffer staging = zest_CreateStagingBuffer(tests->context, 4096, NULL);
			if (staging) {
				memset(zest_BufferData(staging), (char)i, 4096);
				zest_FreeBuffer(staging);
				successful_cycles++;
			}
		}
		if (successful_cycles == cycle_count) passed_tests++;
		total_tests++;
	}

	test->result = (passed_tests == total_tests) ? 0 : 1;
	test->result |= zest_GetValidationErrorCount(tests->context);
	test->frame_count++;
	return test->result;
}

//Test image creation with pixel data
int test__image_with_pixels_creation(ZestTests *tests, Test *test) {
	int passed_tests = 0;
	int total_tests = 0;

	// Phase 1: Different Pixel Formats
	// Test various formats with appropriate pixel data
	struct {
		zest_format format;
		int bytes_per_pixel;
		const char *name;
	} format_tests[] = {
		{zest_format_r8_unorm, 1, "R8"},
		{zest_format_r8g8_unorm, 2, "RG8"},
		{zest_format_r8g8b8a8_unorm, 4, "RGBA8"},
		{zest_format_r16_sfloat, 2, "R16F"},
		{zest_format_r32_sfloat, 4, "R32F"},
		{zest_format_r16g16b16a16_sfloat, 8, "RGBA16F"}
	};

	for (int i = 0; i < 6; i++) {
		const int width = 64;
		const int height = 64;
		zest_size pixel_size = width * height * format_tests[i].bytes_per_pixel;

		// Allocate and fill pixel data with gradient pattern
		zest_byte *pixels = (zest_byte*)malloc(pixel_size);
		if (pixels) {
			for (zest_size j = 0; j < pixel_size; j++) {
				pixels[j] = (zest_byte)(j & 0xFF);
			}

			zest_image_info_t info = zest_CreateImageInfo(width, height);
			info.format = format_tests[i].format;
			info.flags = zest_image_preset_texture;

			zest_image_handle image = zest_CreateImageWithPixels(tests->context, pixels, pixel_size, &info);
			if (image.value != 0) {
				passed_tests++;
				zest_FreeImageNow(image);
			}
			total_tests++;

			free(pixels);
		} else {
			total_tests++;
		}
	}

	// Phase 2: Different Image Sizes
	struct {
		int width;
		int height;
		const char *name;
	} size_tests[] = {
		{16, 16, "small 16x16"},
		{256, 256, "medium 256x256"},
		{1024, 1024, "large 1024x1024"},
		{100, 100, "non-power-of-two 100x100"}
	};

	for (int i = 0; i < 4; i++) {
		int width = size_tests[i].width;
		int height = size_tests[i].height;
		zest_size pixel_size = width * height * 4; // RGBA8

		zest_byte *pixels = (zest_byte*)malloc(pixel_size);
		if (pixels) {
			// Fill with gradient based on position
			for (int y = 0; y < height; y++) {
				for (int x = 0; x < width; x++) {
					int idx = (y * width + x) * 4;
					pixels[idx + 0] = (zest_byte)((x * 255) / width);      // R
					pixels[idx + 1] = (zest_byte)((y * 255) / height);     // G
					pixels[idx + 2] = (zest_byte)(((x + y) * 127) / (width + height)); // B
					pixels[idx + 3] = 255; // A
				}
			}

			zest_image_info_t info = zest_CreateImageInfo(width, height);
			info.format = zest_format_r8g8b8a8_unorm;
			info.flags = zest_image_preset_texture;

			zest_image_handle image = zest_CreateImageWithPixels(tests->context, pixels, pixel_size, &info);
			if (image.value != 0) {
				passed_tests++;
				zest_FreeImageNow(image);
			}
			total_tests++;

			free(pixels);
		} else {
			total_tests++;
		}
	}

	// Phase 3: With/Without Mipmaps
	{
		const int width = 256;
		const int height = 256;
		zest_size pixel_size = width * height * 4;

		zest_byte *pixels = (zest_byte*)malloc(pixel_size);
		if (pixels) {
			memset(pixels, 0x80, pixel_size);

			// Without mipmaps
			zest_image_info_t info_no_mip = zest_CreateImageInfo(width, height);
			info_no_mip.format = zest_format_r8g8b8a8_unorm;
			info_no_mip.flags = zest_image_preset_texture;

			zest_image_handle image_no_mip = zest_CreateImageWithPixels(tests->context, pixels, pixel_size, &info_no_mip);
			if (image_no_mip.value != 0) {
				zest_image img = zest_GetImage(image_no_mip);
				if (img && img->info.mip_levels == 1) {
					passed_tests++;
				}
				total_tests++;
				zest_FreeImageNow(image_no_mip);
			} else {
				total_tests++;
			}

			// With mipmaps
			zest_image_info_t info_mip = zest_CreateImageInfo(width, height);
			info_mip.format = zest_format_r8g8b8a8_unorm;
			info_mip.flags = zest_image_preset_texture_mipmaps;

			zest_image_handle image_mip = zest_CreateImageWithPixels(tests->context, pixels, pixel_size, &info_mip);
			if (image_mip.value != 0) {
				zest_image img = zest_GetImage(image_mip);
				if (img && img->info.mip_levels > 1) {
					passed_tests++;
				}
				total_tests++;
				zest_FreeImageNow(image_mip);
			} else {
				total_tests++;
			}

			free(pixels);
		} else {
			total_tests += 2;
		}
	}

	// Phase 4: Gradient Pattern (verify upload path works)
	{
		const int width = 128;
		const int height = 128;
		zest_size pixel_size = width * height * 4;

		zest_byte *pixels = (zest_byte*)malloc(pixel_size);
		if (pixels) {
			// Create checkerboard pattern
			for (int y = 0; y < height; y++) {
				for (int x = 0; x < width; x++) {
					int idx = (y * width + x) * 4;
					zest_byte color = ((x / 8) + (y / 8)) % 2 ? 255 : 0;
					pixels[idx + 0] = color;
					pixels[idx + 1] = color;
					pixels[idx + 2] = color;
					pixels[idx + 3] = 255;
				}
			}

			zest_image_info_t info = zest_CreateImageInfo(width, height);
			info.format = zest_format_r8g8b8a8_unorm;
			info.flags = zest_image_preset_texture;

			zest_image_handle image = zest_CreateImageWithPixels(tests->context, pixels, pixel_size, &info);
			if (image.value != 0) {
				passed_tests++;
				zest_FreeImageNow(image);
			}
			total_tests++;

			free(pixels);
		} else {
			total_tests++;
		}
	}

	// Phase 5: Multiple Images
	{
		const int multi_count = 5;
		zest_image_handle multi_images[5];
		int created = 0;

		for (int i = 0; i < multi_count; i++) {
			int size = 32 * (i + 1);
			zest_size pixel_size = size * size * 4;

			zest_byte *pixels = (zest_byte*)malloc(pixel_size);
			if (pixels) {
				memset(pixels, (zest_byte)(i * 50), pixel_size);

				zest_image_info_t info = zest_CreateImageInfo(size, size);
				info.format = zest_format_r8g8b8a8_unorm;
				info.flags = zest_image_preset_texture;

				multi_images[i] = zest_CreateImageWithPixels(tests->context, pixels, pixel_size, &info);
				if (multi_images[i].value != 0) {
					created++;
				}

				free(pixels);
			}
		}

		if (created == multi_count) passed_tests++;
		total_tests++;

		// Cleanup
		for (int i = 0; i < multi_count; i++) {
			if (multi_images[i].value != 0) {
				zest_FreeImageNow(multi_images[i]);
			}
		}
	}

	// Phase 6: Edge Cases
	// Minimum size (1x1)
	{
		zest_byte pixel[4] = {255, 128, 64, 255};
		zest_image_info_t info = zest_CreateImageInfo(1, 1);
		info.format = zest_format_r8g8b8a8_unorm;
		info.flags = zest_image_preset_texture;

		zest_image_handle image = zest_CreateImageWithPixels(tests->context, pixel, 4, &info);
		if (image.value != 0) {
			passed_tests++;
			zest_FreeImageNow(image);
		}
		total_tests++;
	}

	// Non-square image (256x64)
	{
		const int width = 256;
		const int height = 64;
		zest_size pixel_size = width * height * 4;

		zest_byte *pixels = (zest_byte*)malloc(pixel_size);
		if (pixels) {
			memset(pixels, 0xAB, pixel_size);

			zest_image_info_t info = zest_CreateImageInfo(width, height);
			info.format = zest_format_r8g8b8a8_unorm;
			info.flags = zest_image_preset_texture;

			zest_image_handle image = zest_CreateImageWithPixels(tests->context, pixels, pixel_size, &info);
			if (image.value != 0) {
				zest_image img = zest_GetImage(image);
				if (img && img->info.extent.width == width && img->info.extent.height == height) {
					passed_tests++;
				}
				total_tests++;
				zest_FreeImageNow(image);
			} else {
				total_tests++;
			}

			free(pixels);
		} else {
			total_tests++;
		}
	}

	// Large image (2048x2048) - only if memory allows
	{
		const int width = 2048;
		const int height = 2048;
		zest_size pixel_size = width * height * 4;

		zest_byte *pixels = (zest_byte*)malloc(pixel_size);
		if (pixels) {
			// Fill with simple pattern (avoid slow per-pixel loop for large image)
			memset(pixels, 0x55, pixel_size);

			zest_image_info_t info = zest_CreateImageInfo(width, height);
			info.format = zest_format_r8g8b8a8_unorm;
			info.flags = zest_image_preset_texture;

			zest_image_handle image = zest_CreateImageWithPixels(tests->context, pixels, pixel_size, &info);
			if (image.value != 0) {
				passed_tests++;
				zest_FreeImageNow(image);
			}
			// Don't fail test if large allocation fails
			total_tests++;

			free(pixels);
		} else {
			// Memory allocation failed - still counts as handled gracefully
			passed_tests++;
			total_tests++;
		}
	}

	test->result = (passed_tests == total_tests) ? 0 : 1;
	test->result |= zest_GetValidationErrorCount(tests->context);
	test->frame_count++;
	return test->result;
}

//Test sampler creation with different configurations
int test__sampler_creation(ZestTests *tests, Test *test) {
	int passed_tests = 0;
	int total_tests = 0;

	// Phase 1: Basic creation with default settings
	{
		zest_sampler_info_t info = zest_CreateSamplerInfo();
		zest_sampler_handle sampler = zest_CreateSampler(tests->context, &info);
		if (sampler.value != 0) {
			zest_sampler s = zest_GetSampler(sampler);
			if (s) {
				// Verify default values were applied
				if (s->create_info.mag_filter == zest_filter_linear &&
					s->create_info.min_filter == zest_filter_linear &&
					s->create_info.address_mode_u == zest_sampler_address_mode_clamp_to_edge) {
					passed_tests++;
				}
				total_tests++;
			} else {
				total_tests++;
			}
			zest_FreeSamplerNow(sampler);
		} else {
			total_tests++;
		}
	}

	// Phase 2: Different filter modes
	struct {
		zest_filter_type mag;
		zest_filter_type min;
		const char *name;
	} filter_tests[] = {
		{zest_filter_nearest, zest_filter_nearest, "nearest/nearest"},
		{zest_filter_linear, zest_filter_linear, "linear/linear"},
		{zest_filter_nearest, zest_filter_linear, "nearest/linear"},
		{zest_filter_linear, zest_filter_nearest, "linear/nearest"}
	};

	for (int i = 0; i < 4; i++) {
		zest_sampler_info_t info = zest_CreateSamplerInfo();
		info.mag_filter = filter_tests[i].mag;
		info.min_filter = filter_tests[i].min;

		zest_sampler_handle sampler = zest_CreateSampler(tests->context, &info);
		if (sampler.value != 0) {
			passed_tests++;
			zest_FreeSamplerNow(sampler);
		}
		total_tests++;
	}

	// Phase 3: Different address modes
	zest_sampler_address_mode address_modes[] = {
		zest_sampler_address_mode_repeat,
		zest_sampler_address_mode_mirrored_repeat,
		zest_sampler_address_mode_clamp_to_edge,
		zest_sampler_address_mode_clamp_to_border,
		zest_sampler_address_mode_mirror_clamp_to_edge
	};

	for (int i = 0; i < 5; i++) {
		zest_sampler_info_t info = zest_CreateSamplerInfo();
		info.address_mode_u = address_modes[i];
		info.address_mode_v = address_modes[i];
		info.address_mode_w = address_modes[i];

		zest_sampler_handle sampler = zest_CreateSampler(tests->context, &info);
		if (sampler.value != 0) {
			passed_tests++;
			zest_FreeSamplerNow(sampler);
		}
		total_tests++;
	}

	// Phase 4: Different mipmap modes
	{
		// Nearest mipmap
		zest_sampler_info_t info_nearest = zest_CreateSamplerInfo();
		info_nearest.mipmap_mode = zest_mipmap_mode_nearest;

		zest_sampler_handle sampler_nearest = zest_CreateSampler(tests->context, &info_nearest);
		if (sampler_nearest.value != 0) {
			passed_tests++;
			zest_FreeSamplerNow(sampler_nearest);
		}
		total_tests++;

		// Linear mipmap (trilinear filtering)
		zest_sampler_info_t info_linear = zest_CreateSamplerInfo();
		info_linear.mipmap_mode = zest_mipmap_mode_linear;

		zest_sampler_handle sampler_linear = zest_CreateSampler(tests->context, &info_linear);
		if (sampler_linear.value != 0) {
			passed_tests++;
			zest_FreeSamplerNow(sampler_linear);
		}
		total_tests++;
	}

	// Phase 5: Anisotropic filtering
	{
		float aniso_levels[] = {1.0f, 2.0f, 4.0f, 8.0f, 16.0f};

		for (int i = 0; i < 5; i++) {
			zest_sampler_info_t info = zest_CreateSamplerInfo();
			info.anisotropy_enable = ZEST_TRUE;
			info.max_anisotropy = aniso_levels[i];

			zest_sampler_handle sampler = zest_CreateSampler(tests->context, &info);
			if (sampler.value != 0) {
				passed_tests++;
				zest_FreeSamplerNow(sampler);
			}
			total_tests++;
		}
	}

	// Phase 6: Compare operations (for shadow mapping)
	{
		zest_compare_op compare_ops[] = {
			zest_compare_op_never,
			zest_compare_op_less,
			zest_compare_op_equal,
			zest_compare_op_less_or_equal,
			zest_compare_op_greater,
			zest_compare_op_not_equal,
			zest_compare_op_greater_or_equal,
			zest_compare_op_always
		};

		for (int i = 0; i < 8; i++) {
			zest_sampler_info_t info = zest_CreateSamplerInfo();
			info.compare_enable = ZEST_TRUE;
			info.compare_op = compare_ops[i];

			zest_sampler_handle sampler = zest_CreateSampler(tests->context, &info);
			if (sampler.value != 0) {
				passed_tests++;
				zest_FreeSamplerNow(sampler);
			}
			total_tests++;
		}
	}

	// Phase 7: Border colors (for clamp_to_border mode)
	{
		zest_border_color border_colors[] = {
			zest_border_color_float_transparent_black,
			zest_border_color_int_transparent_black,
			zest_border_color_float_opaque_black,
			zest_border_color_int_opaque_black,
			zest_border_color_float_opaque_white,
			zest_border_color_int_opaque_white
		};

		for (int i = 0; i < 6; i++) {
			zest_sampler_info_t info = zest_CreateSamplerInfo();
			info.address_mode_u = zest_sampler_address_mode_clamp_to_border;
			info.address_mode_v = zest_sampler_address_mode_clamp_to_border;
			info.border_color = border_colors[i];

			zest_sampler_handle sampler = zest_CreateSampler(tests->context, &info);
			if (sampler.value != 0) {
				passed_tests++;
				zest_FreeSamplerNow(sampler);
			}
			total_tests++;
		}
	}

	// Phase 8: LOD settings
	{
		// Custom LOD range
		zest_sampler_info_t info = zest_CreateSamplerInfo();
		info.min_lod = 0.0f;
		info.max_lod = 8.0f;
		info.mip_lod_bias = 0.5f;

		zest_sampler_handle sampler = zest_CreateSampler(tests->context, &info);
		if (sampler.value != 0) {
			zest_sampler s = zest_GetSampler(sampler);
			if (s && s->create_info.max_lod == 8.0f && s->create_info.mip_lod_bias == 0.5f) {
				passed_tests++;
			}
			total_tests++;
			zest_FreeSamplerNow(sampler);
		} else {
			total_tests++;
		}
	}

	// Phase 9: Multiple samplers simultaneously
	{
		const int multi_count = 10;
		zest_sampler_handle multi_samplers[10];
		int created = 0;

		for (int i = 0; i < multi_count; i++) {
			zest_sampler_info_t info = zest_CreateSamplerInfo();
			// Vary settings slightly
			info.mag_filter = (i % 2 == 0) ? zest_filter_nearest : zest_filter_linear;
			info.min_filter = (i % 2 == 0) ? zest_filter_linear : zest_filter_nearest;

			multi_samplers[i] = zest_CreateSampler(tests->context, &info);
			if (multi_samplers[i].value != 0) {
				created++;
			}
		}

		if (created == multi_count) passed_tests++;
		total_tests++;

		// Cleanup
		for (int i = 0; i < multi_count; i++) {
			if (multi_samplers[i].value != 0) {
				zest_FreeSamplerNow(multi_samplers[i]);
			}
		}
	}

	// Phase 10: Descriptor index acquisition
	{
		zest_sampler_info_t info = zest_CreateSamplerInfo();
		zest_sampler_handle sampler = zest_CreateSampler(tests->context, &info);

		if (sampler.value != 0) {
			zest_sampler s = zest_GetSampler(sampler);
			if (s) {
				// Acquire bindless descriptor index
				zest_uint index = zest_AcquireSamplerIndex(tests->device, s);
				if (index != ZEST_INVALID) {
					passed_tests++;
				}
				total_tests++;
			} else {
				total_tests++;
			}
			zest_FreeSamplerNow(sampler);
		} else {
			total_tests++;
		}
	}

	// Phase 11: Complex sampler configuration (all features combined)
	{
		zest_sampler_info_t info = zest_CreateSamplerInfo();
		info.mag_filter = zest_filter_linear;
		info.min_filter = zest_filter_linear;
		info.mipmap_mode = zest_mipmap_mode_linear;
		info.address_mode_u = zest_sampler_address_mode_repeat;
		info.address_mode_v = zest_sampler_address_mode_repeat;
		info.address_mode_w = zest_sampler_address_mode_clamp_to_edge;
		info.anisotropy_enable = ZEST_TRUE;
		info.max_anisotropy = 8.0f;
		info.min_lod = 0.0f;
		info.max_lod = 12.0f;
		info.mip_lod_bias = -0.5f;

		zest_sampler_handle sampler = zest_CreateSampler(tests->context, &info);
		if (sampler.value != 0) {
			passed_tests++;
			zest_FreeSamplerNow(sampler);
		}
		total_tests++;
	}

	test->result = (passed_tests == total_tests) ? 0 : 1;
	test->result |= zest_GetValidationErrorCount(tests->context);
	test->frame_count++;
	return test->result;
}

//Test image arrays and descriptor index acquisition
int test__image_array_descriptor_indexes(ZestTests *tests, Test *test) {
	int passed_tests = 0;
	int total_tests = 0;

	// Phase 1: 2D Array Image
	{
		int phase_total = 0;
		int phase_passed = 0;
		zest_image_info_t info = zest_CreateImageInfo(64, 64);
		info.format = zest_format_r8g8b8a8_unorm;
		info.flags = zest_image_preset_texture;
		info.layer_count = 4; // 4-layer array texture

		zest_image_handle image_handle = zest_CreateImage(tests->context, &info);
		if (image_handle.value != 0) {
			zest_image image = zest_GetImage(image_handle);
			if (image && image->info.layer_count == 4) {
				phase_passed++;
			}
			phase_total++;

			zest_queue queue = zest_BeginImmediateCommandBuffer(tests->device, zest_queue_graphics);
			zest_imm_TransitionImage(queue, image, zest_image_layout_read_only_optimal, 0, 1, 0, 4);
			zest_EndImmediateCommandBuffer(queue);

			// Acquire sampled index with array binding
			zest_uint index = zest_AcquireSampledImageIndex(tests->device, image, zest_texture_array_binding);
			if (index != ZEST_INVALID) {
				phase_passed++;
			}
			phase_total++;

			zest_FreeImageNow(image_handle);
		} else {
			phase_total += 2;
		}
		PrintTestUpdate(test, 1, phase_passed == phase_total);
		passed_tests += phase_passed;
		total_tests += phase_total;
	}

	// Phase 2: Cubemap Image
	{
		int phase_total = 0;
		int phase_passed = 0;
		zest_image_info_t info = zest_CreateImageInfo(64, 64);
		info.format = zest_format_r8g8b8a8_unorm;
		info.flags = zest_image_preset_storage_cubemap; // Includes cubemap flag
		info.layer_count = 6; // Cubemaps always have 6 faces

		zest_image_handle image_handle = zest_CreateImage(tests->context, &info);
		if (image_handle.value != 0) {
			zest_image image = zest_GetImage(image_handle);
			if (image && image->info.layer_count == 6) {
				phase_passed++;
			}
			phase_total++;

			zest_queue queue = zest_BeginImmediateCommandBuffer(tests->device, zest_queue_graphics);
			zest_imm_TransitionImage(queue, image, zest_image_layout_read_only_optimal, 0, 1, 0, 6);
			zest_EndImmediateCommandBuffer(queue);

			// Acquire sampled index with cube binding
			zest_uint index = zest_AcquireSampledImageIndex(tests->device, image, zest_texture_cube_binding);
			if (index != ZEST_INVALID) {
				phase_passed++;
			}
			phase_total++;

			zest_FreeImageNow(image_handle);
		} else {
			phase_total += 2;
		}
		PrintTestUpdate(test, 2, phase_passed == phase_total);
		passed_tests += phase_passed;
		total_tests += phase_total;
	}

	// Phase 3: Storage Image
	{
		int phase_total = 0;
		int phase_passed = 0;
		zest_image_info_t info = zest_CreateImageInfo(64, 64);
		info.format = zest_format_r8g8b8a8_unorm;
		info.flags = zest_image_preset_storage;

		zest_image_handle image_handle = zest_CreateImage(tests->context, &info);
		if (image_handle.value != 0) {
			zest_image image = zest_GetImage(image_handle);

			// Acquire storage index
			zest_uint index = zest_AcquireStorageImageIndex(tests->device, image, zest_storage_image_binding);
			if (index != ZEST_INVALID) {
				phase_passed++;
			}
			phase_total++;

			zest_FreeImageNow(image_handle);
		} else {
			phase_total++;
		}
		PrintTestUpdate(test, 3, phase_passed == phase_total);
		passed_tests += phase_passed;
		total_tests += phase_total;
	}

	// Phase 4: Multiple Binding Types for Same Image
	{
		int phase_total = 0;
		int phase_passed = 0;
		// Create image with both sampled and storage flags
		zest_image_info_t info = zest_CreateImageInfo(64, 64);
		info.format = zest_format_r8g8b8a8_unorm;
		info.flags = zest_image_flag_sampled | zest_image_flag_storage |
		             zest_image_flag_transfer_dst | zest_image_flag_device_local;

		zest_image_handle image_handle = zest_CreateImage(tests->context, &info);
		if (image_handle.value != 0) {
			zest_image image = zest_GetImage(image_handle);

			zest_queue queue = zest_BeginImmediateCommandBuffer(tests->device, zest_queue_graphics);
			zest_imm_TransitionImage(queue, image, zest_image_layout_general, 0, 1, 0, 1);
			zest_EndImmediateCommandBuffer(queue);

			// Acquire sampled index
			zest_uint sampled_index = zest_AcquireSampledImageIndex(tests->device, image, zest_texture_2d_binding);
			// Acquire storage index
			zest_uint storage_index = zest_AcquireStorageImageIndex(tests->device, image, zest_storage_image_binding);

			if (sampled_index != ZEST_INVALID && storage_index != ZEST_INVALID) {
				phase_passed++;
			}
			phase_total++;

			zest_FreeImageNow(image_handle);
		} else {
			phase_total++;
		}
		PrintTestUpdate(test, 4, phase_passed == phase_total);
		passed_tests += phase_passed;
		total_tests += phase_total;
	}

	// Phase 5: Index Uniqueness
	{
		int phase_total = 0;
		int phase_passed = 0;
		const int image_count = 5;
		zest_image_handle images[5];
		zest_uint indexes[5];
		int created = 0;

		for (int i = 0; i < image_count; i++) {
			zest_image_info_t info = zest_CreateImageInfo(32, 32);
			info.format = zest_format_r8g8b8a8_unorm;
			info.flags = zest_image_preset_texture;

			images[i] = zest_CreateImage(tests->context, &info);
			if (images[i].value != 0) {
				zest_image image = zest_GetImage(images[i]);

				zest_queue queue = zest_BeginImmediateCommandBuffer(tests->device, zest_queue_graphics);
				zest_imm_TransitionImage(queue, image, zest_image_layout_read_only_optimal, 0, 1, 0, 1);
				zest_EndImmediateCommandBuffer(queue);

				indexes[i] = zest_AcquireSampledImageIndex(tests->device, image, zest_texture_2d_binding);
				if (indexes[i] != ZEST_INVALID) {
					created++;
				}
			}
		}

		// Verify all indexes are unique
		int all_unique = 1;
		for (int i = 0; i < created; i++) {
			for (int j = i + 1; j < created; j++) {
				if (indexes[i] == indexes[j]) {
					all_unique = 0;
					break;
				}
			}
			if (!all_unique) break;
		}

		if (created == image_count && all_unique) {
			phase_passed++;
		}
		phase_total++;

		// Cleanup
		for (int i = 0; i < image_count; i++) {
			if (images[i].value != 0) {
				zest_FreeImageNow(images[i]);
			}
		}
		PrintTestUpdate(test, 5, phase_passed == phase_total);
		passed_tests += phase_passed;
		total_tests += phase_total;
	}

	// Phase 6: Per-Mip View Indexes
	{
		int phase_total = 0;
		int phase_passed = 0;
		zest_image_info_t info = zest_CreateImageInfo(256, 256);
		info.format = zest_format_r8g8b8a8_unorm;
		info.flags = zest_image_preset_storage; // Storage allows mip writes
		info.flags |= zest_image_flag_generate_mipmaps | zest_image_flag_transfer_src;

		zest_image_handle image_handle = zest_CreateImage(tests->context, &info);
		if (image_handle.value != 0) {
			zest_image image = zest_GetImage(image_handle);

			// Create per-mip views
			zest_image_view_array_handle view_array_handle = zest_CreateImageViewsPerMip(tests->context, image);
			if (view_array_handle.value != 0) {
				zest_image_view_array view_array = zest_GetImageViewArray(view_array_handle);
				if (view_array && view_array->count > 1) {
					phase_passed++;
				}
				phase_total++;

				// Acquire per-mip indexes
				zest_uint *mip_indexes = zest_AcquireImageMipIndexes(
					tests->device, image, view_array,
					zest_storage_image_binding, zest_descriptor_type_storage_image
				);
				if (mip_indexes) {
					// Verify at least first index is valid
					if (mip_indexes[0] != ZEST_INVALID) {
						phase_passed++;
					}
					phase_total++;
				} else {
					phase_total++;
				}

				zest_FreeImageViewArrayNow(view_array_handle);
			} else {
				phase_total += 2;
			}

			zest_FreeImageNow(image_handle);
		} else {
			phase_total += 2;
		}
		PrintTestUpdate(test, 6, phase_passed == phase_total);
		passed_tests += phase_passed;
		total_tests += phase_total;
	}

	// Phase 7: Different Layer Counts
	{
		int phase_total = 0;
		int phase_passed = 0;
		int layer_counts[] = {1, 2, 4, 8};
		int successful = 0;

		for (int i = 0; i < 4; i++) {
			zest_image_info_t info = zest_CreateImageInfo(32, 32);
			info.format = zest_format_r8g8b8a8_unorm;
			info.flags = zest_image_preset_texture;
			info.layer_count = layer_counts[i];

			zest_image_handle image_handle = zest_CreateImage(tests->context, &info);
			if (image_handle.value != 0) {
				zest_image image = zest_GetImage(image_handle);
				if (image && image->info.layer_count == (zest_uint)layer_counts[i]) {
					successful++;
				}
				zest_FreeImageNow(image_handle);
			}
		}

		if (successful == 4) {
			phase_passed++;
		}
		phase_total++;
		PrintTestUpdate(test, 7, phase_passed == phase_total);
		passed_tests += phase_passed;
		total_tests += phase_total;
	}

	// Phase 8: Storage Buffer Index
	{
		int phase_total = 0;
		int phase_passed = 0;
		zest_buffer_info_t buffer_info = zest_CreateBufferInfo(zest_buffer_type_storage, zest_memory_usage_gpu_only);
		zest_buffer buffer = zest_CreateBuffer(tests->context, 1024, &buffer_info);
		if (buffer) {
			zest_uint index = zest_AcquireStorageBufferIndex(tests->device, buffer);
			if (index != ZEST_INVALID) {
				phase_passed++;
			}
			phase_total++;
			zest_FreeBuffer(buffer);
		} else {
			phase_total++;
		}
		PrintTestUpdate(test, 8, phase_passed == phase_total);
		passed_tests += phase_passed;
		total_tests += phase_total;
	}

	// Phase 9: Uniform Buffer Index (via uniform buffer API)
	{
		int phase_total = 0;
		int phase_passed = 0;
		zest_uniform_buffer_handle ub = zest_CreateUniformBuffer(tests->context, "test_ub", 256);
		if (ub.value != 0) {
			zest_uniform_buffer uniform = zest_GetUniformBuffer(ub);
			zest_uint index = zest_GetUniformBufferDescriptorIndex(uniform);
			if (index != ZEST_INVALID) {
				phase_passed++;
			}
			phase_total++;
			zest_FreeUniformBuffer(ub);
		} else {
			phase_total++;
		}
		PrintTestUpdate(test, 9, phase_passed == phase_total);
		passed_tests += phase_passed;
		total_tests += phase_total;
	}

	test->result = (passed_tests == total_tests) ? 0 : 1;
	test->result |= zest_GetValidationErrorCount(tests->context);
	test->frame_count++;
	return test->result;
}