typedef int (*d3d12_test_callback)(void);

struct D3D12Test {
	const char *name;
	d3d12_test_callback run;
};

static zest_bool d3d12_use_warp = ZEST_FALSE;
static zest_bool d3d12_use_gpu_validation = ZEST_FALSE;

zest_device_builder BeginD3D12TestBuilder(void) {
	zest_device_init_flags flags = zest_device_init_flag_enable_validation_layers | zest_device_init_flag_log_validation_errors_to_memory | zest_device_init_flag_log_validation_errors_to_console;
	if (d3d12_use_gpu_validation) {
		flags |= zest_device_init_flag_enable_validation_layers_with_sync;
	}
	zest_device_builder builder = zest_BeginD3D12DeviceBuilder(flags);
	if (d3d12_use_warp) {
		zest_DeviceBuilderUseWarp(builder);
	}
	return builder;
}

static int finish_device(zest_device device);

int test_d3d12_builder(void) {
	zest_device_builder builder = BeginD3D12TestBuilder();
	int failed = builder->platform != zest_platform_d3d12;
	failed |= strcmp(builder->cached_shader_path, "./dxil/") != 0;
	failed |= ZEST__NOT_FLAGGED(builder->flags, zest_device_init_flag_enable_validation_layers);
	//Ending the builder is what frees it
	zest_device device = zest_EndDeviceBuilder(builder);
	if (!device) return 1;
	return failed | finish_device(device);
}

#define CHECK_INTERFACE_ID(type) failed |= memcmp(&zest__dx_iid_##type, &__uuidof(type), sizeof(GUID)) != 0
int test_d3d12_interface_ids(void) {
	int failed = 0;
	CHECK_INTERFACE_ID(IDXGIFactory6);
	CHECK_INTERFACE_ID(IDXGIAdapter4);
	CHECK_INTERFACE_ID(ID3D12Device);
	CHECK_INTERFACE_ID(ID3D12Device10);
	CHECK_INTERFACE_ID(ID3D12Debug3);
	CHECK_INTERFACE_ID(ID3D12DebugDevice);
	CHECK_INTERFACE_ID(ID3D12InfoQueue1);
	CHECK_INTERFACE_ID(ID3D12DeviceRemovedExtendedDataSettings1);
	CHECK_INTERFACE_ID(ID3D12CommandQueue);
	CHECK_INTERFACE_ID(ID3D12CommandAllocator);
	CHECK_INTERFACE_ID(ID3D12GraphicsCommandList7);
	CHECK_INTERFACE_ID(ID3D12Fence);
	CHECK_INTERFACE_ID(ID3D12Resource);
	CHECK_INTERFACE_ID(ID3D12Heap);
	CHECK_INTERFACE_ID(ID3D12DescriptorHeap);
	return failed;
}

static zest_dx_feature_support_t minimum_support(void) {
	zest_dx_feature_support_t support = {};
	support.feature_level = D3D_FEATURE_LEVEL_12_0;
	support.resource_binding_tier = D3D12_RESOURCE_BINDING_TIER_3;
	support.enhanced_barriers = ZEST_TRUE;
	support.inverted_viewport_height = ZEST_TRUE;
	return support;
}

int test_d3d12_minimum_spec(void) {
	const char *missing = NULL;
	zest_dx_feature_support_t support = minimum_support();
	int failed = !zest__dx_meets_minimum_spec(&support, &missing);
	support.feature_level = D3D_FEATURE_LEVEL_11_1;
	failed |= zest__dx_meets_minimum_spec(&support, &missing) || strcmp(missing, "feature level 12_0") != 0;
	support = minimum_support(); support.resource_binding_tier = D3D12_RESOURCE_BINDING_TIER_2;
	failed |= zest__dx_meets_minimum_spec(&support, &missing) || strcmp(missing, "resource binding tier 3") != 0;
	support = minimum_support(); support.enhanced_barriers = ZEST_FALSE;
	failed |= zest__dx_meets_minimum_spec(&support, &missing) || strcmp(missing, "enhanced barriers") != 0;
	support = minimum_support(); support.inverted_viewport_height = ZEST_FALSE;
	failed |= zest__dx_meets_minimum_spec(&support, &missing) || strcmp(missing, "inverted viewport height") != 0;
	return failed;
}

int test_d3d12_capability_translation(void) {
	zest_dx_feature_support_t support = minimum_support();
	zest_capability_flags flags = zest__dx_translate_capabilities(&support);
	zest_capability_flags always = zest_capability_multi_draw_indirect | zest_capability_nonuniform_storage_buffer_indexing |
		zest_capability_nonuniform_uniform_buffer_indexing | zest_capability_nonuniform_storage_image_indexing |
		zest_capability_nonuniform_sampled_image_indexing | zest_capability_anisotropic_filtering | zest_capability_wireframe |
		zest_capability_image_cube_array | zest_capability_tessellation | zest_capability_geometry_shader |
		zest_capability_fragment_stores_and_atomics | zest_capability_texture_compression_bc;
	zest_capability_flags never = zest_capability_buffer_device_address | zest_capability_texture_compression_astc_ldr |
		zest_capability_texture_compression_etc2 | zest_capability_scalar_block_layout | zest_capability_shader_int64;
	int failed = (flags & always) != always || (flags & never) != 0;
	support.int64_shader_ops = ZEST_TRUE;
	failed |= !(zest__dx_translate_capabilities(&support) & zest_capability_shader_int64);
	return failed;
}

int test_d3d12_cleanup_uninitialised_backend(void) {
	void *memory = malloc(zloc__MEGABYTE(1));
	zest_device_t device = {};
	zest_platform_t platform = {};
	device.allocator = zloc_InitialiseAllocatorWithPool(memory, zloc__MEGABYTE(1));
	device.platform = &platform;
	zest__dx_initialise_platform_callbacks(&platform);
	device.backend = (zest_device_backend)platform.new_device_backend(&device);
	int failed = device.backend == NULL;
	platform.cleanup_device_backend(&device);
	failed |= device.backend != NULL;
	free(memory);
	return failed | (zest__dx_last_device_release_count() != 0);
}

int test_d3d12_heap_properties(void) {
	int failed = zest__dx_heap_properties(zest_memory_property_device_local_bit, ZEST_TRUE).Type != D3D12_HEAP_TYPE_DEFAULT;
	failed |= zest__dx_heap_properties(0, ZEST_TRUE).Type != D3D12_HEAP_TYPE_DEFAULT;
	D3D12_HEAP_PROPERTIES upload = zest__dx_heap_properties(zest_memory_property_host_visible_bit | zest_memory_property_host_coherent_bit, ZEST_TRUE);
	failed |= upload.Type != D3D12_HEAP_TYPE_CUSTOM || upload.CPUPageProperty != D3D12_CPU_PAGE_PROPERTY_WRITE_COMBINE || upload.MemoryPoolPreference != D3D12_MEMORY_POOL_L0;
	D3D12_HEAP_PROPERTIES readback = zest__dx_heap_properties(zest_memory_property_host_visible_bit | zest_memory_property_host_cached_bit, ZEST_TRUE);
	failed |= readback.Type != D3D12_HEAP_TYPE_CUSTOM || readback.CPUPageProperty != D3D12_CPU_PAGE_PROPERTY_WRITE_BACK;
	failed |= zest__dx_heap_properties(zest_memory_property_device_local_bit | zest_memory_property_host_visible_bit, ZEST_TRUE).Type != D3D12_HEAP_TYPE_GPU_UPLOAD;
	failed |= zest__dx_heap_properties(zest_memory_property_device_local_bit | zest_memory_property_host_visible_bit, ZEST_FALSE).CPUPageProperty != D3D12_CPU_PAGE_PROPERTY_WRITE_COMBINE;
	//GPU upload memory is write combined, so a host cached request falls through to a write back custom heap
	D3D12_HEAP_PROPERTIES cached_device_local = zest__dx_heap_properties(zest_memory_property_device_local_bit | zest_memory_property_host_visible_bit | zest_memory_property_host_cached_bit, ZEST_TRUE);
	failed |= cached_device_local.Type != D3D12_HEAP_TYPE_CUSTOM || cached_device_local.CPUPageProperty != D3D12_CPU_PAGE_PROPERTY_WRITE_BACK || cached_device_local.MemoryPoolPreference != D3D12_MEMORY_POOL_L0;
	failed |= zest__dx_image_heap_flags(ZEST_DX_HEAP_CATEGORY_TEXTURES, D3D12_RESOURCE_HEAP_TIER_1) != D3D12_HEAP_FLAG_ALLOW_ONLY_NON_RT_DS_TEXTURES;
	failed |= zest__dx_image_heap_flags(ZEST_DX_HEAP_CATEGORY_TEXTURES, D3D12_RESOURCE_HEAP_TIER_2) != D3D12_HEAP_FLAG_ALLOW_ALL_BUFFERS_AND_TEXTURES;
	failed |= zest__dx_image_heap_flags(ZEST_DX_HEAP_CATEGORY_MSAA, D3D12_RESOURCE_HEAP_TIER_1) != D3D12_HEAP_FLAG_ALLOW_ONLY_RT_DS_TEXTURES;
	failed |= zest__dx_image_heap_flags(ZEST_DX_HEAP_CATEGORY_MSAA, D3D12_RESOURCE_HEAP_TIER_2) != D3D12_HEAP_FLAG_ALLOW_ALL_BUFFERS_AND_TEXTURES;
	failed |= zest__dx_image_heap_flags(ZEST_DX_HEAP_CATEGORY_TARGETS, D3D12_RESOURCE_HEAP_TIER_1) != D3D12_HEAP_FLAG_ALLOW_ONLY_RT_DS_TEXTURES;
	failed |= zest__dx_image_heap_flags(ZEST_DX_HEAP_CATEGORY_TARGETS, D3D12_RESOURCE_HEAP_TIER_2) != D3D12_HEAP_FLAG_ALLOW_ONLY_RT_DS_TEXTURES;
	return failed;
}

int test_d3d12_format_round_trip(void) {
	int failed = 0;
	for (int format = zest_format_undefined + 1; format < zest_max_format; ++format) {
		DXGI_FORMAT dxgi = zest__dx_to_dxgi_format((zest_format)format);
		if (dxgi != DXGI_FORMAT_UNKNOWN && zest__dx_to_dxgi_format(zest__dx_from_dxgi_format(dxgi)) != dxgi) failed = 1;
	}
	failed |= zest__dx_from_dxgi_format(DXGI_FORMAT_R8G8B8A8_UNORM) != zest_format_r8g8b8a8_unorm;
	failed |= zest__dx_to_dxgi_format(zest_format_r8g8b8a8_unorm) != DXGI_FORMAT_R8G8B8A8_UNORM;
	failed |= zest__dx_to_dxgi_format(zest_format_b8g8r8a8_srgb) != DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;
	failed |= zest__dx_to_dxgi_format(zest_format_d32_sfloat) != DXGI_FORMAT_D32_FLOAT;
	failed |= zest__dx_to_dxgi_format(zest_format_astc_4X4_unorm_block) != DXGI_FORMAT_UNKNOWN;
	failed |= zest__dx_to_dxgi_format(zest_format_r8g8b8_unorm) != DXGI_FORMAT_UNKNOWN;
	failed |= zest__dx_shader_view_format(DXGI_FORMAT_D32_FLOAT) != DXGI_FORMAT_R32_FLOAT;
	failed |= zest__dx_typeless_format(DXGI_FORMAT_D24_UNORM_S8_UINT) != DXGI_FORMAT_R24G8_TYPELESS;
	return failed;
}

int test_d3d12_image_heap_category(void) {
	int failed = zest__dx_image_heap_category(zest_image_flag_color_attachment, 4, D3D12_RESOURCE_HEAP_TIER_2) != ZEST_DX_HEAP_CATEGORY_MSAA;
	failed |= zest__dx_image_heap_category(zest_image_flag_color_attachment, 1, D3D12_RESOURCE_HEAP_TIER_1) != ZEST_DX_HEAP_CATEGORY_TARGETS;
	failed |= zest__dx_image_heap_category(zest_image_flag_color_attachment, 1, D3D12_RESOURCE_HEAP_TIER_2) != ZEST_DX_HEAP_CATEGORY_TEXTURES;
	failed |= zest__dx_image_heap_category(zest_image_flag_sampled, 1, D3D12_RESOURCE_HEAP_TIER_1) != ZEST_DX_HEAP_CATEGORY_TEXTURES;
	zest_device device = zest_EndDeviceBuilder(BeginD3D12TestBuilder());
	if (!device) return 1;
	ID3D12Heap *heap = NULL;
	int baseline = device->memory_allocation_count;
	failed |= zest__dx_create_image_heap(device, zloc__KILOBYTE(64), 0, &heap) != ZEST_FALSE;
	failed |= heap != NULL || device->memory_allocation_count != baseline;
	failed |= zest_GetValidationErrorCount(device) != 1;
	zest_ResetValidationErrors(device);
	return failed | finish_device(device);
}

int test_d3d12_layout_table(void) {
	int failed = zest__dx_barrier_layout(zest_image_layout_shader_read_only_optimal, zest_access_shader_read_bit, zest_queue_graphics) != D3D12_BARRIER_LAYOUT_SHADER_RESOURCE;
	failed |= zest__dx_barrier_layout(zest_image_layout_general, zest_access_shader_write_bit, zest_queue_compute) != D3D12_BARRIER_LAYOUT_UNORDERED_ACCESS;
	failed |= zest__dx_barrier_layout(zest_image_layout_general, zest_access_shader_read_bit, zest_queue_graphics) != D3D12_BARRIER_LAYOUT_DIRECT_QUEUE_COMMON;
	failed |= zest__dx_barrier_layout(zest_image_layout_general, zest_access_shader_read_bit, zest_queue_compute) != D3D12_BARRIER_LAYOUT_COMPUTE_QUEUE_COMMON;
	failed |= zest__dx_barrier_layout(zest_image_layout_transfer_dst_optimal, zest_access_transfer_write_bit, zest_queue_graphics) != D3D12_BARRIER_LAYOUT_COPY_DEST;
	failed |= zest__dx_barrier_layout(zest_image_layout_transfer_dst_optimal, zest_access_transfer_write_bit, zest_queue_transfer) != D3D12_BARRIER_LAYOUT_COMMON;
	failed |= zest__dx_barrier_layout(zest_image_layout_shader_read_only_optimal, zest_access_shader_read_bit, zest_queue_transfer) != D3D12_BARRIER_LAYOUT_COMMON;
	failed |= zest__dx_barrier_layout(zest_image_layout_undefined, zest_access_none, zest_queue_transfer) != D3D12_BARRIER_LAYOUT_UNDEFINED;
	failed |= zest__dx_barrier_layout(zest_image_layout_undefined, zest_access_none, zest_queue_graphics) != D3D12_BARRIER_LAYOUT_UNDEFINED;
	failed |= zest__dx_barrier_layout(zest_image_layout_depth_stencil_attachment_optimal, zest_access_depth_stencil_attachment_write_bit, zest_queue_graphics) != D3D12_BARRIER_LAYOUT_DEPTH_STENCIL_WRITE;
	failed |= zest__dx_barrier_layout(zest_image_layout_present, zest_access_none, zest_queue_graphics) != D3D12_BARRIER_LAYOUT_PRESENT;
	failed |= zest__dx_barrier_access(zest_access_shader_read_bit, ZEST_FALSE, D3D12_BARRIER_LAYOUT_UNDEFINED) != (D3D12_BARRIER_ACCESS_SHADER_RESOURCE | D3D12_BARRIER_ACCESS_UNORDERED_ACCESS);
	failed |= zest__dx_barrier_access(zest_access_shader_read_bit, ZEST_TRUE, D3D12_BARRIER_LAYOUT_UNORDERED_ACCESS) != D3D12_BARRIER_ACCESS_UNORDERED_ACCESS;
	failed |= zest__dx_barrier_access(zest_access_host_read_bit, ZEST_FALSE, D3D12_BARRIER_LAYOUT_UNDEFINED) != D3D12_BARRIER_ACCESS_NO_ACCESS;
	failed |= zest__dx_barrier_sync(zest_pipeline_stage_top_of_pipe_bit, ZEST_TRUE) != D3D12_BARRIER_SYNC_NONE;
	failed |= zest__dx_barrier_sync(zest_pipeline_stage_top_of_pipe_bit, ZEST_FALSE) != D3D12_BARRIER_SYNC_ALL;
	failed |= zest__dx_barrier_sync(zest_pipeline_stage_bottom_of_pipe_bit, ZEST_TRUE) != D3D12_BARRIER_SYNC_ALL;
	failed |= zest__dx_barrier_sync(zest_pipeline_stage_bottom_of_pipe_bit, ZEST_FALSE) != D3D12_BARRIER_SYNC_NONE;
	failed |= zest__dx_barrier_sync(zest_pipeline_stage_fragment_shader_bit, ZEST_FALSE) != D3D12_BARRIER_SYNC_PIXEL_SHADING;
	failed |= zest__dx_barrier_sync(zest_pipeline_stage_vertex_input_bit, ZEST_FALSE) != (D3D12_BARRIER_SYNC_INDEX_INPUT | D3D12_BARRIER_SYNC_VERTEX_SHADING);
	failed |= zest__dx_barrier_layout(zest_image_layout_depth_stencil_read_only_optimal, zest_access_depth_stencil_attachment_read_bit, zest_queue_graphics) != D3D12_BARRIER_LAYOUT_DEPTH_STENCIL_READ;
	failed |= zest__dx_barrier_layout(zest_image_layout_depth_stencil_read_only_optimal, zest_access_depth_stencil_attachment_read_bit | zest_access_shader_read_bit, zest_queue_graphics) != D3D12_BARRIER_LAYOUT_DIRECT_QUEUE_GENERIC_READ;
	failed |= zest__dx_barrier_layout(zest_image_layout_depth_stencil_read_only_optimal, zest_access_shader_read_bit, zest_queue_graphics) != D3D12_BARRIER_LAYOUT_SHADER_RESOURCE;
	failed |= zest__dx_barrier_layout(zest_image_layout_depth_stencil_read_only_optimal, zest_access_depth_stencil_attachment_read_bit | zest_access_shader_read_bit, zest_queue_compute) != D3D12_BARRIER_LAYOUT_SHADER_RESOURCE;
	failed |= zest__dx_barrier_layout(zest_image_layout_depth_stencil_read_only_optimal, zest_access_shader_read_bit, zest_queue_compute) != D3D12_BARRIER_LAYOUT_SHADER_RESOURCE;
	failed |= zest__dx_barrier_layout(zest_image_layout_read_only_optimal, zest_access_depth_stencil_attachment_read_bit | zest_access_shader_read_bit, zest_queue_graphics) != D3D12_BARRIER_LAYOUT_DIRECT_QUEUE_GENERIC_READ;
	failed |= zest__dx_barrier_access(zest_access_depth_stencil_attachment_write_bit | zest_access_depth_stencil_attachment_read_bit, ZEST_TRUE, D3D12_BARRIER_LAYOUT_DEPTH_STENCIL_WRITE) != D3D12_BARRIER_ACCESS_DEPTH_STENCIL_WRITE;
	failed |= zest__dx_barrier_access(zest_access_depth_stencil_attachment_read_bit, ZEST_TRUE, D3D12_BARRIER_LAYOUT_DEPTH_STENCIL_READ) != D3D12_BARRIER_ACCESS_DEPTH_STENCIL_READ;
	failed |= zest__dx_barrier_sync(zest_pipeline_stage_transfer_bit, ZEST_FALSE) != (D3D12_BARRIER_SYNC_COPY | D3D12_BARRIER_SYNC_RESOLVE);
	failed |= zest__dx_normalise_sync(zest__dx_barrier_sync(zest_pipeline_stage_transfer_bit, ZEST_FALSE), D3D12_BARRIER_ACCESS_COPY_DEST) != D3D12_BARRIER_SYNC_COPY;
	failed |= zest__dx_normalise_sync(zest__dx_barrier_sync(zest_pipeline_stage_transfer_bit, ZEST_TRUE), D3D12_BARRIER_ACCESS_COPY_SOURCE) != D3D12_BARRIER_SYNC_COPY;
	failed |= zest__dx_normalise_sync(zest__dx_barrier_sync(zest_pipeline_stage_transfer_bit, ZEST_FALSE), D3D12_BARRIER_ACCESS_RESOLVE_DEST) != (D3D12_BARRIER_SYNC_COPY | D3D12_BARRIER_SYNC_RESOLVE);
	failed |= zest__dx_normalise_sync(zest__dx_barrier_sync(zest_pipeline_stage_transfer_bit, ZEST_FALSE), D3D12_BARRIER_ACCESS_COMMON) != (D3D12_BARRIER_SYNC_COPY | D3D12_BARRIER_SYNC_RESOLVE);
	return failed;
}

int test_d3d12_row_repack(void) {
	int failed = !zest__dx_copy_needs_repack(4, 0, ZEST_FALSE);
	failed |= zest__dx_copy_needs_repack(256, 512, ZEST_FALSE);
	failed |= !zest__dx_copy_needs_repack(256, 256, ZEST_FALSE);
	failed |= zest__dx_copy_needs_repack(4, 0, ZEST_TRUE);
	unsigned char source[36];
	unsigned char destination[3 * 256];
	for (int index = 0; index != 36; ++index) source[index] = (unsigned char)index;
	memset(destination, 0xCD, sizeof(destination));
	zest__dx_repack_rows(source, 12, destination, 256, 12, 3);
	for (int row = 0; row != 3; ++row) {
		for (int column = 0; column != 12; ++column) failed |= destination[row * 256 + column] != row * 12 + column;
		failed |= destination[row * 256 + 12] != 0xCD;
	}
	return failed;
}

int test_d3d12_bindless_regions(void) {
	zest_uint counts[zest_max_global_binding_number] = { 64, 1024, 1024, 1024, 256, 1024, 1024, 64, 256 };
	zest_dx_bindless_regions_t regions = {};
	zest__dx_compute_bindless_regions(counts, &regions);
	zest_uint expected[zest_max_global_binding_number] = { 0, 0, 1024, 2048, 3072, 3328, 5376, 6400, 6464 };
	int failed = memcmp(regions.resource_offsets, expected, sizeof(expected)) != 0;
	failed |= regions.storage_buffer_uav_offset != 4352;
	failed |= regions.resource_descriptor_count != 6720;
	failed |= regions.sampler_descriptor_count != 64;
	return failed;
}

static int finish_device(zest_device device) {
	int failed = zest_GetValidationErrorCount(device) != 0;
	zest_DestroyDevice(device);
	failed |= zest__dx_last_device_release_count() != 0;
	return failed;
}
static int run_device_lifecycle(zest_device_builder builder) {
	zest_device device = zest_EndDeviceBuilder(builder);
	if (!device) return 1;
	int failed = zest_GetDevicePlatform(device) != zest_platform_d3d12;
	failed |= zest_GetD3D12Device(device) == NULL;
	zest_create_context_info_t info = zest_CreateContextInfo();
	zest_context context = zest_CreateHeadlessContext(device, &info);
	failed |= context == NULL;
	if (context) zest_DestroyContext(context);
	return failed | finish_device(device);
}
int test_d3d12_device_lifecycle(void) {
	return run_device_lifecycle(BeginD3D12TestBuilder());
}
int test_d3d12_back_to_back_devices(void) {
	int failed = run_device_lifecycle(BeginD3D12TestBuilder());
	return failed | run_device_lifecycle(BeginD3D12TestBuilder());
}
int test_d3d12_warp_device(void) {
	const char *missing = NULL;
	zest_uint release_count = zest__dx_last_device_release_count();
	zest_bool warp_meets_minimum_spec = zest__dx_warp_meets_minimum_spec(&missing);
	int failed = zest__dx_last_device_release_count() != release_count;
	if (!warp_meets_minimum_spec) {
		ZEST_PRINT("D3D12 WARP Device skipped: WARP on this OS lacks %s", missing);
		return failed;
	}
	//The probe released its WARP device, so creating one now can't be rejected as a second device
	zest_device_builder builder = BeginD3D12TestBuilder();
	zest_DeviceBuilderUseWarp(builder);
	return failed | run_device_lifecycle(builder);
}
//The hook only applies while the adapter is selected, so it's cleared as soon as the device exists
static zest_device create_test_device(zest_bool restricted_copy_pitch) {
	zest__dx_forced_restricted_copy_pitch = restricted_copy_pitch;
	zest_device device = zest_EndDeviceBuilder(BeginD3D12TestBuilder());
	zest__dx_forced_restricted_copy_pitch = ZEST_FALSE;
	return device;
}
static int image_round_trip(zest_device device) {
	zest_uint pixels[37 * 5];
	zest_uint read_back[37 * 5];
	for (zest_uint index = 0; index != 37 * 5; ++index) pixels[index] = index * 2654435761u;
	zest_image_info_t image_info = zest_CreateImageInfo(37, 5);
	image_info.format = zest_format_r8g8b8a8_unorm;
	image_info.flags = zest_image_preset_texture | zest_image_flag_transfer_src;
	zest_image_handle image = zest_CreateImageWithPixels(device, pixels, sizeof(pixels), &image_info);
	int failed = !zest_CopyImageToBitmap(device, zest_GetImage(image), read_back);
	failed |= memcmp(pixels, read_back, sizeof(pixels)) != 0;
	zest_FreeImageNow(image);
	return failed;
}
static int run_image_round_trip(zest_bool restricted_copy_pitch) {
	zest_device device = create_test_device(restricted_copy_pitch);
	if (!device) return 1;
	int failed = restricted_copy_pitch && ZEST_DX_BACKEND(device, device)->support.unrestricted_copy_pitch;
	failed |= image_round_trip(device);
	return failed | finish_device(device);
}
int test_d3d12_image_round_trip(void) {
	return run_image_round_trip(ZEST_FALSE);
}
int test_d3d12_allocation_baseline(void) {
	zest_device_builder builder = BeginD3D12TestBuilder();
	zest_SetDeviceBuilderDedicatedImageThreshold(builder, 256 * 1024);
	zest_device device = zest_EndDeviceBuilder(builder);
	if (!device) return 1;
	int baseline = device->memory_allocation_count;
	zest_image_handle images[17];
	for (int index = 0; index != 16; ++index) {
		zest_image_info_t image_info = zest_CreateImageInfo(64, 64);
		image_info.flags = zest_image_preset_texture;
		images[index] = zest_CreateImage(device, &image_info);
	}
	int pooled = device->memory_allocation_count;
	int failed = pooled > baseline + 1;
	zest_image_info_t large_info = zest_CreateImageInfo(512, 512);
	large_info.flags = zest_image_preset_texture;
	images[16] = zest_CreateImage(device, &large_info);
	failed |= device->memory_allocation_count != pooled + 1;
	failed |= ZEST__NOT_FLAGGED(zest_GetImage(images[16])->info.flags, zest_image_flag_dedicated_memory);
	failed |= ZEST__FLAGGED(zest_GetImage(images[0])->info.flags, zest_image_flag_dedicated_memory);
	for (int index = 0; index != 17; ++index) zest_FreeImageNow(images[index]);
	failed |= device->memory_allocation_count != baseline;
	return failed | finish_device(device);
}
int test_d3d12_failed_creation(void) {
	zest__dx_forced_missing_feature = "enhanced barriers";
	zest_device device = zest_EndDeviceBuilder(BeginD3D12TestBuilder());
	zest__dx_forced_missing_feature = NULL;
	int failed = device != NULL;
	failed |= zest__dx_last_device_release_count() != 0;
	return failed | run_device_lifecycle(BeginD3D12TestBuilder());
}
int test_d3d12_unsupported_format(void) {
	zest_device device = zest_EndDeviceBuilder(BeginD3D12TestBuilder());
	if (!device) return 1;
	int failed = zest_IsImageFormatSupported(device, zest_format_astc_4X4_unorm_block, zest_image_preset_texture) != ZEST_FALSE;
	zest_uint errors_before = zest_GetValidationErrorCount(device);
	zest_image_info_t image_info = zest_CreateImageInfo(16, 16);
	image_info.format = zest_format_astc_4X4_unorm_block;
	image_info.flags = zest_image_preset_texture;
	zest_image_handle image = zest_CreateImage(device, &image_info);
	zest_image_handle null_handle = ZEST__ZERO_INIT(zest_image_handle);
	failed |= memcmp(&image, &null_handle, sizeof(image)) != 0;
	failed |= zest_GetValidationErrorCount(device) != errors_before + 1;
	zest_DestroyDevice(device);
	return failed | (zest__dx_last_device_release_count() != 0);
}
int test_d3d12_sampler_ceiling_clamp(void) {
	zest_device_builder builder = BeginD3D12TestBuilder();
	builder->bindless_sampler_count = 4096;
	zest_device device = zest_EndDeviceBuilder(builder);
	if (!device) return 1;
	int failed = device->setup_info.bindless_sampler_count != 2048;
	zest_DestroyDevice(device);
	return failed | (zest__dx_last_device_release_count() != 0);
}
int test_d3d12_restricted_copy_pitch(void) {
	return run_image_round_trip(ZEST_TRUE);
}
static int run_transfer_queue_upload(zest_bool restricted_copy_pitch) {
	zest_device device = create_test_device(restricted_copy_pitch);
	if (!device) return 1;
	int failed = restricted_copy_pitch && ZEST_DX_BACKEND(device, device)->support.unrestricted_copy_pitch;
	zest_uint pixels[37 * 5];
	zest_uint read_back[37 * 5];
	for (zest_uint index = 0; index != 37 * 5; ++index) pixels[index] = index * 2654435761u + 7;
	zest_image_info_t image_info = zest_CreateImageInfo(37, 5);
	image_info.format = zest_format_r8g8b8a8_unorm;
	image_info.flags = zest_image_preset_texture | zest_image_flag_transfer_src;
	zest_image_handle image_handle = zest_CreateImage(device, &image_info);
	zest_image image = zest_GetImage(image_handle);
	zest_buffer staging_buffer = zest_CreateStagingBuffer(device, sizeof(pixels), pixels);
	zest_buffer_image_copy_t region = {};
	region.image_aspect = zest_image_aspect_color_bit;
	region.layer_count = 1;
	region.image_extent.width = 37;
	region.image_extent.height = 5;
	region.image_extent.depth = 1;
	zest_queue queue = zest_imm_BeginCommandBuffer(device, zest_queue_transfer);
	failed |= !queue || ZEST_DX_BACKEND(queue, queue)->type != D3D12_COMMAND_LIST_TYPE_COPY;
	failed |= !zest_imm_TransitionImage(queue, image, zest_resource_state_copy_dst, 0, 1, 0, 1);
	failed |= !zest_imm_CopyBufferRegionsToImage(queue, &region, 1, staging_buffer, image);
	failed |= !zest_imm_TransitionImage(queue, image, zest_resource_state_shader_read, 0, 1, 0, 1);
	failed |= !zest_imm_EndCommandBuffer(queue);
	zest_FreeBufferNow(staging_buffer);
	failed |= ZEST_DX_BACKEND(image, image)->layout != D3D12_BARRIER_LAYOUT_COMMON;
	failed |= !zest_CopyImageToBitmap(device, image, read_back);
	failed |= memcmp(pixels, read_back, sizeof(pixels)) != 0;
	//The readback leaves the image in the exact layout the copy queue left it in
	failed |= ZEST_DX_BACKEND(image, image)->layout != D3D12_BARRIER_LAYOUT_COMMON;
	failed |= image->layout != zest_image_layout_shader_read_only_optimal;
	zest_FreeImageNow(image_handle);
	return failed | finish_device(device);
}
int test_d3d12_transfer_queue_upload(void) {
	int failed = run_transfer_queue_upload(ZEST_FALSE);
	return failed | run_transfer_queue_upload(ZEST_TRUE);
}
//Reads mip 0 of every layer, and every depth slice of a 3D image, back into pixels, tightly packed one after another
static int read_image_layers(zest_device device, zest_image image, zest_uint layer_count, zest_size layer_size, void *pixels) {
	ID3D12Resource *texture = ZEST_DX_BACKEND(image, image)->resource;
	D3D12_RESOURCE_DESC1 description = zest__dx_resource_description(texture);
	D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint = {};
	UINT row_count = 0;
	UINT64 row_size = 0;
	UINT64 total_size = 0;
	ZEST_DX_CALL(ZEST_DX_BACKEND(device, device)->device10, GetCopyableFootprints1, &description, 0, 1, 0, &footprint, &row_count, &row_size, &total_size);
	zest_size layer_stride = zloc__align_size_up(total_size, D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT);
	zest_buffer_info_t buffer_info = zest_CreateBufferInfo(zest_buffer_type_staging, zest_memory_usage_gpu_to_cpu);
	zest_buffer readback_buffer = zest_CreateBuffer(device, layer_stride * layer_count + D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT, &buffer_info);
	if (!readback_buffer) return 1;
	zest_size readback_base = zloc__align_size_up(readback_buffer->memory_offset, D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT);
	zest_queue queue = zest_imm_BeginCommandBuffer(device, zest_queue_graphics);
	int failed = !zest_imm_TransitionImage(queue, image, zest_resource_state_copy_src, 0, 1, 0, layer_count);
	if (queue) {
		D3D12_TEXTURE_COPY_LOCATION source = {};
		source.pResource = texture;
		source.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
		D3D12_TEXTURE_COPY_LOCATION destination = {};
		destination.pResource = ZEST_DX_BACKEND(device_memory_pool, readback_buffer->memory_pool)->buffer;
		destination.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
		destination.PlacedFootprint = footprint;
		for (zest_uint layer = 0; layer != layer_count; ++layer) {
			source.SubresourceIndex = layer * description.MipLevels;
			destination.PlacedFootprint.Offset = readback_base + layer * layer_stride;
			ZEST_DX_CALL(ZEST_DX_BACKEND(queue, queue)->command_list, CopyTextureRegion, &destination, 0, 0, 0, &source, NULL);
		}
	}
	failed |= !zest_imm_TransitionImage(queue, image, zest_resource_state_shader_read, 0, 1, 0, layer_count);
	failed |= !zest_imm_EndCommandBuffer(queue);
	if (!failed) {
		for (zest_uint layer = 0; layer != layer_count; ++layer) {
			for (zest_uint slice = 0; slice != footprint.Footprint.Depth; ++slice) {
				zest__dx_repack_rows((const char *)readback_buffer->memory_pool->mapped + readback_base + layer * layer_stride + (zest_size)slice * footprint.Footprint.RowPitch * row_count, footprint.Footprint.RowPitch,
					(char *)pixels + layer * layer_size + (zest_size)slice * row_size * row_count, (zest_size)row_size, (zest_size)row_size, row_count);
			}
		}
	}
	zest_FreeBufferNow(readback_buffer);
	return failed;
}
static int run_many_layer_upload(zest_bool restricted_copy_pitch) {
	zest_device device = create_test_device(restricted_copy_pitch);
	if (!device) return 1;
	int failed = restricted_copy_pitch && ZEST_DX_BACKEND(device, device)->support.unrestricted_copy_pitch;
	const zest_uint layer_count = 256;
	const zest_uint pixels_per_layer = 4 * 4;
	const zest_size pixel_bytes = layer_count * pixels_per_layer * sizeof(zest_uint);
	zest_uint *pixels = (zest_uint *)malloc(pixel_bytes);
	zest_uint *read_back = (zest_uint *)malloc(pixel_bytes);
	for (zest_uint index = 0; index != layer_count * pixels_per_layer; ++index) pixels[index] = index * 2654435761u + 13;
	memset(read_back, 0, pixel_bytes);
	zest_image_info_t image_info = zest_CreateImageInfo(4, 4);
	image_info.format = zest_format_r8g8b8a8_unorm;
	image_info.layer_count = layer_count;
	image_info.flags = zest_image_preset_texture | zest_image_flag_transfer_src;
	zest_image_handle image_handle = zest_CreateImage(device, &image_info);
	zest_image image = zest_GetImage(image_handle);
	zest_buffer staging_buffer = zest_CreateStagingBuffer(device, pixel_bytes, pixels);
	zest_buffer_image_copy_t region = {};
	region.image_aspect = zest_image_aspect_color_bit;
	region.layer_count = layer_count;
	region.image_extent.width = 4;
	region.image_extent.height = 4;
	region.image_extent.depth = 1;
	zest_queue queue = zest_imm_BeginCommandBuffer(device, zest_queue_graphics);
	failed |= !zest_imm_TransitionImage(queue, image, zest_resource_state_copy_dst, 0, 1, 0, layer_count);
	failed |= !zest_imm_CopyBufferRegionsToImage(queue, &region, 1, staging_buffer, image);
	failed |= !zest_imm_TransitionImage(queue, image, zest_resource_state_shader_read, 0, 1, 0, layer_count);
	failed |= !zest_imm_EndCommandBuffer(queue);
	zest_FreeBufferNow(staging_buffer);
	failed |= read_image_layers(device, image, layer_count, pixels_per_layer * sizeof(zest_uint), read_back);
	failed |= memcmp(pixels, read_back, pixel_bytes) != 0;
	free(pixels);
	free(read_back);
	zest_FreeImageNow(image_handle);
	return failed | finish_device(device);
}
int test_d3d12_many_layer_upload(void) {
	int failed = run_many_layer_upload(ZEST_FALSE);
	return failed | run_many_layer_upload(ZEST_TRUE);
}
int test_d3d12_second_device_rejected(void) {
	zest_device device = zest_EndDeviceBuilder(BeginD3D12TestBuilder());
	if (!device) return 1;
	zest_device second_device = zest_EndDeviceBuilder(BeginD3D12TestBuilder());
	int failed = 0;
	//With another qualifying GPU the in-use adapter is skipped, so the second device must be on a different adapter
	if (second_device) {
		LUID first_adapter = zest_GetD3D12Device(device)->GetAdapterLuid();
		LUID second_adapter = zest_GetD3D12Device(second_device)->GetAdapterLuid();
		failed |= memcmp(&first_adapter, &second_adapter, sizeof(LUID)) == 0;
		failed |= zest_GetValidationErrorCount(second_device) != 0;
		zest_DestroyDevice(second_device);
	}
	//The rejected attempt must leave the first device alive and working
	failed |= ZEST_DX_CALL0(zest_GetD3D12Device(device), GetDeviceRemovedReason) != S_OK;
	failed |= image_round_trip(device);
	return failed | finish_device(device);
}
int test_d3d12_invalid_copy_regions(void) {
	zest_device device = zest_EndDeviceBuilder(BeginD3D12TestBuilder());
	if (!device) return 1;
	zest_image_info_t image_info = zest_CreateImageInfo(8, 8);
	image_info.format = zest_format_r8g8b8a8_unorm;
	image_info.flags = zest_image_preset_texture;
	zest_image_handle image_handle = zest_CreateImage(device, &image_info);
	zest_image image = zest_GetImage(image_handle);
	zest_buffer staging_buffer = zest_CreateStagingBuffer(device, 8 * 8 * sizeof(zest_uint), NULL);
	zest_buffer_image_copy_t valid_region = {};
	valid_region.image_aspect = zest_image_aspect_color_bit;
	valid_region.layer_count = 1;
	valid_region.image_extent.width = 8;
	valid_region.image_extent.height = 8;
	valid_region.image_extent.depth = 1;
	zest_buffer_image_copy_t bad_mip = valid_region;
	bad_mip.mip_level = 1;
	zest_buffer_image_copy_t bad_layer = valid_region;
	bad_layer.base_array_layer = 1;
	zest_buffer_image_copy_t zero_layers = bad_layer;
	zero_layers.layer_count = 0;
	zest_buffer_image_copy_t bad_extent = valid_region;
	bad_extent.image_offset.x = 4;
	zest_buffer_image_copy_t valid_then_bad[2] = { valid_region, bad_extent };
	//The buffer can be larger than requested, so the offset is taken from its actual end to leave the last row one pixel past it
	zest_buffer_image_copy_t past_buffer_end = valid_region;
	past_buffer_end.buffer_offset = staging_buffer->size - 8 * 8 * sizeof(zest_uint) + sizeof(zest_uint);
	//Added to the buffer's offset and the region's span this wraps around to the start of the buffer
	zest_buffer_image_copy_t wrapping_offset = valid_region;
	wrapping_offset.buffer_offset = ~(zest_size)0 - 8 * 8 * sizeof(zest_uint) + 1;
	zest_queue queue = zest_imm_BeginCommandBuffer(device, zest_queue_graphics);
	int failed = !zest_imm_TransitionImage(queue, image, zest_resource_state_copy_dst, 0, 1, 0, 1);
	failed |= zest_imm_CopyBufferRegionsToImage(queue, &bad_mip, 1, staging_buffer, image) != ZEST_FALSE;
	failed |= zest_imm_CopyBufferRegionsToImage(queue, &bad_layer, 1, staging_buffer, image) != ZEST_FALSE;
	failed |= zest_imm_CopyBufferRegionsToImage(queue, &zero_layers, 1, staging_buffer, image) != ZEST_FALSE;
	failed |= zest_imm_CopyBufferRegionsToImage(queue, &bad_extent, 1, staging_buffer, image) != ZEST_FALSE;
	failed |= zest_imm_CopyBufferRegionsToImage(queue, valid_then_bad, 2, staging_buffer, image) != ZEST_FALSE;
	failed |= zest_imm_CopyBufferRegionsToImage(queue, &past_buffer_end, 1, staging_buffer, image) != ZEST_FALSE;
	failed |= zest_imm_CopyBufferRegionsToImage(queue, &wrapping_offset, 1, staging_buffer, image) != ZEST_FALSE;
	failed |= !zest_imm_CopyBufferRegionsToImage(queue, &valid_region, 1, staging_buffer, image);
	failed |= !zest_imm_TransitionImage(queue, image, zest_resource_state_shader_read, 0, 1, 0, 1);
	failed |= !zest_imm_EndCommandBuffer(queue);
	zest_FreeBufferNow(staging_buffer);
	//One entry per distinct rejection, and nothing from the debug layer
	failed |= zest_GetValidationErrorCount(device) != 4;
	zest_FreeImageNow(image_handle);
	zest_DestroyDevice(device);
	return failed | (zest__dx_last_device_release_count() != 0);
}
int test_d3d12_unavailable_immediate_commands(void) {
	zest_device device = zest_EndDeviceBuilder(BeginD3D12TestBuilder());
	if (!device) return 1;
	zest_buffer buffer = zest_CreateStagingBuffer(device, 256, NULL);
	zest_image_info_t image_info = zest_CreateImageInfo(4, 4);
	image_info.flags = zest_image_preset_texture;
	zest_image_handle image_handle = zest_CreateImage(device, &image_info);
	zest_image image = zest_GetImage(image_handle);
	zest_queue queue = zest_imm_BeginCommandBuffer(device, zest_queue_graphics);
	int failed = queue == NULL || buffer == NULL;
	zest_clear_value_t clear_value = {};
	zest_imm_FillBuffer(queue, NULL, 0);
	zest_imm_UpdateBuffer(queue, NULL, NULL, 0);
	failed |= zest_imm_ClearColorImage(queue, NULL, clear_value) != ZEST_FALSE;
	failed |= zest_imm_ClearDepthStencilImage(queue, NULL, 1.f, 0) != ZEST_FALSE;
	failed |= zest_imm_BlitImage(queue, NULL, NULL, 0, 0, 1, 1, 0, 0, 1, 1, zest_filter_linear) != ZEST_FALSE;
	failed |= zest_imm_ResolveImage(queue, NULL, NULL) != ZEST_FALSE;
	zest_imm_SendPushConstants(queue, NULL, 0);
	zest_imm_BindComputePipeline(queue, NULL);
	failed |= zest_imm_DispatchCompute(queue, 1, 1, 1) != ZEST_FALSE;
	failed |= zest_imm_CopyBuffer(queue, buffer, buffer, 64) != ZEST_FALSE;
	failed |= zest_imm_CopyBufferRegion(queue, buffer, 0, buffer, 128, 64) != ZEST_FALSE;
	failed |= zest_imm_CopyBufferToImage(queue, buffer, image, 64) != ZEST_FALSE;
	failed |= zest_imm_GenerateMipMaps(queue, image) != ZEST_FALSE;
	failed |= !zest_imm_EndCommandBuffer(queue);
	//CopyBuffer and CopyBufferRegion share one backend entry, so they share one report
	failed |= zest_GetValidationErrorCount(device) != 12;
	zest_FreeBufferNow(buffer);
	zest_FreeImageNow(image_handle);
	zest_DestroyDevice(device);
	return failed | (zest__dx_last_device_release_count() != 0);
}
int test_d3d12_pool_failure_rejected(void) {
	zest_device device = create_device_with_failing_pool(BeginD3D12TestBuilder());
	int failed = device != NULL;
	return failed | (zest__dx_last_device_release_count() != 0);
}
int test_d3d12_srv_description(void) {
	zest_image_t image = {};
	zest_dx_image_backend_t backend = {};
	image.backend = (zest_image_backend)&backend;
	image.info.format = zest_format_r8g8b8a8_unorm;
	image.info.sample_count = 1;
	backend.sample_count = 1;
	D3D12_SHADER_RESOURCE_VIEW_DESC description = zest__dx_image_srv_description(&image, zest_image_view_type_2d, 3, 1, 0, 1);
	int failed = description.ViewDimension != D3D12_SRV_DIMENSION_TEXTURE2D || description.Texture2D.MostDetailedMip != 1 || description.Texture2D.MipLevels != 3;
	failed |= description.Format != DXGI_FORMAT_R8G8B8A8_UNORM || description.Shader4ComponentMapping != D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	description = zest__dx_image_srv_description(&image, zest_image_view_type_2d_array, 1, 0, 2, 4);
	failed |= description.ViewDimension != D3D12_SRV_DIMENSION_TEXTURE2DARRAY || description.Texture2DArray.FirstArraySlice != 2 || description.Texture2DArray.ArraySize != 4;
	description = zest__dx_image_srv_description(&image, zest_image_view_type_cube, 1, 0, 0, 6);
	failed |= description.ViewDimension != D3D12_SRV_DIMENSION_TEXTURECUBE || description.TextureCube.MipLevels != 1;
	description = zest__dx_image_srv_description(&image, zest_image_view_type_cube, 1, 0, 6, 6);
	failed |= description.ViewDimension != D3D12_SRV_DIMENSION_TEXTURECUBEARRAY || description.TextureCubeArray.First2DArrayFace != 6 || description.TextureCubeArray.NumCubes != 1;
	//The view follows the sample count the resource was created with, not the image info
	backend.sample_count = 4;
	description = zest__dx_image_srv_description(&image, zest_image_view_type_2d, 1, 0, 0, 1);
	failed |= description.ViewDimension != D3D12_SRV_DIMENSION_TEXTURE2DMS;
	description = zest__dx_image_srv_description(&image, zest_image_view_type_2d_array, 1, 0, 1, 2);
	failed |= description.ViewDimension != D3D12_SRV_DIMENSION_TEXTURE2DMSARRAY || description.Texture2DMSArray.FirstArraySlice != 1 || description.Texture2DMSArray.ArraySize != 2;
	backend.sample_count = 1;
	image.info.format = zest_format_bc1_rgb_unorm_block;
	description = zest__dx_image_srv_description(&image, zest_image_view_type_2d, 1, 0, 0, 1);
	failed |= description.Format != DXGI_FORMAT_BC1_UNORM;
	failed |= description.Shader4ComponentMapping != D3D12_ENCODE_SHADER_4_COMPONENT_MAPPING(0, 1, 2, D3D12_SHADER_COMPONENT_MAPPING_FORCE_VALUE_1);
	image.info.swizzle.r = zest_component_swizzle_a;
	description = zest__dx_image_srv_description(&image, zest_image_view_type_2d, 1, 0, 0, 1);
	failed |= description.Shader4ComponentMapping != D3D12_ENCODE_SHADER_4_COMPONENT_MAPPING(D3D12_SHADER_COMPONENT_MAPPING_FORCE_VALUE_1, 1, 2, D3D12_SHADER_COMPONENT_MAPPING_FORCE_VALUE_1);
	image.info.swizzle.r = zest_component_swizzle_identity;
	image.info.format = zest_format_bc1_rgba_unorm_block;
	description = zest__dx_image_srv_description(&image, zest_image_view_type_2d, 1, 0, 0, 1);
	failed |= description.Shader4ComponentMapping != D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	return failed;
}
int test_d3d12_depth_deny_srv(void) {
	zest_device device = zest_EndDeviceBuilder(BeginD3D12TestBuilder());
	if (!device) return 1;
	zest_image_info_t image_info = zest_CreateImageInfo(64, 64);
	image_info.format = zest_format_d32_sfloat;
	image_info.flags = zest_image_flag_depth_stencil_attachment | zest_image_flag_device_local;
	zest_image_handle depth_handle = zest_CreateImage(device, &image_info);
	image_info.flags = zest_image_preset_depth_attachment;
	zest_image_handle sampled_handle = zest_CreateImage(device, &image_info);
	int failed = !zest_GetImage(depth_handle) || !zest_GetImage(sampled_handle);
	if (!failed) {
		D3D12_RESOURCE_DESC1 depth_description = zest__dx_resource_description(ZEST_DX_BACKEND(image, zest_GetImage(depth_handle))->resource);
		D3D12_RESOURCE_DESC1 sampled_description = zest__dx_resource_description(ZEST_DX_BACKEND(image, zest_GetImage(sampled_handle))->resource);
		failed |= !(depth_description.Flags & D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE) || !(depth_description.Flags & D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL);
		failed |= (sampled_description.Flags & D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE) || !(sampled_description.Flags & D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL);
	}
	zest_FreeImageNow(depth_handle);
	zest_FreeImageNow(sampled_handle);
	return failed | finish_device(device);
}
int test_d3d12_format_support(void) {
	zest_device device = zest_EndDeviceBuilder(BeginD3D12TestBuilder());
	if (!device) return 1;
	int failed = zest_IsImageFormatSupported(device, zest_format_r8g8b8a8_unorm, zest_image_preset_texture) != ZEST_TRUE;
	failed |= zest_IsImageFormatSupported(device, zest_format_r8g8b8a8_unorm, zest_image_preset_texture | zest_image_flag_host_visible) != ZEST_FALSE;
	failed |= zest_IsImageFormatSupported(device, zest_format_r32_uint, zest_image_flag_storage) != ZEST_TRUE;
	D3D12_FEATURE_DATA_FORMAT_SUPPORT support = zest__dx_format_support(zest_GetD3D12Device(device), DXGI_FORMAT_R8G8B8A8_UNORM);
	UINT typed_load_and_store = D3D12_FORMAT_SUPPORT2_UAV_TYPED_LOAD | D3D12_FORMAT_SUPPORT2_UAV_TYPED_STORE;
	zest_bool storage_supported = (support.Support1 & D3D12_FORMAT_SUPPORT1_TYPED_UNORDERED_ACCESS_VIEW) && ((UINT)support.Support2 & typed_load_and_store) == typed_load_and_store;
	failed |= zest_IsImageFormatSupported(device, zest_format_r8g8b8a8_unorm, zest_image_flag_storage) != storage_supported;
	//Synthetic support bits, so these hold on any hardware
	D3D12_FEATURE_DATA_FORMAT_SUPPORT synthetic = {};
	synthetic.Support1 = (D3D12_FORMAT_SUPPORT1)(D3D12_FORMAT_SUPPORT1_TEXTURE2D | D3D12_FORMAT_SUPPORT1_TYPED_UNORDERED_ACCESS_VIEW);
	synthetic.Support2 = D3D12_FORMAT_SUPPORT2_UAV_TYPED_STORE;
	failed |= zest__dx_format_support_allows(&synthetic, zest_image_flag_storage, ZEST_TRUE) != ZEST_FALSE;
	synthetic.Support2 = (D3D12_FORMAT_SUPPORT2)(D3D12_FORMAT_SUPPORT2_UAV_TYPED_LOAD | D3D12_FORMAT_SUPPORT2_UAV_TYPED_STORE);
	failed |= zest__dx_format_support_allows(&synthetic, zest_image_flag_storage, ZEST_TRUE) != ZEST_TRUE;
	synthetic.Support1 = D3D12_FORMAT_SUPPORT1_TEXTURE2D;
	failed |= zest__dx_format_support_allows(&synthetic, zest_image_flag_storage, ZEST_TRUE) != ZEST_FALSE;
	failed |= zest__dx_format_support_allows(&synthetic, zest_image_flag_sampled, ZEST_TRUE) != ZEST_FALSE;
	failed |= zest__dx_format_support_allows(&synthetic, zest_image_flag_sampled, ZEST_FALSE) != ZEST_TRUE;
	return failed | finish_device(device);
}
static int run_3d_upload_slice_pitch(zest_bool restricted_copy_pitch) {
	zest_device device = create_test_device(restricted_copy_pitch);
	if (!device) return 1;
	int failed = restricted_copy_pitch && ZEST_DX_BACKEND(device, device)->support.unrestricted_copy_pitch;
	const zest_uint width = 4, height = 4, depth = 3, padded_height = 6;
	const zest_uint sentinel = 0xDEADBEEF;
	zest_uint source[width * padded_height * depth];
	zest_uint expected[width * height * depth];
	zest_uint read_back[width * height * depth];
	for (zest_uint slice = 0; slice != depth; ++slice) {
		for (zest_uint row = 0; row != padded_height; ++row) {
			for (zest_uint column = 0; column != width; ++column) {
				zest_uint value = row < height ? (slice * 64 + row * 8 + column) * 2654435761u + 17 : sentinel;
				source[(slice * padded_height + row) * width + column] = value;
				if (row < height) expected[(slice * height + row) * width + column] = value;
			}
		}
	}
	memset(read_back, 0, sizeof(read_back));
	zest_image_info_t image_info = zest_CreateImageInfo3D(width, height, depth);
	image_info.format = zest_format_r8g8b8a8_unorm;
	image_info.flags = zest_image_preset_texture_3d | zest_image_flag_transfer_src;
	zest_image_handle image_handle = zest_CreateImage(device, &image_info);
	zest_image image = zest_GetImage(image_handle);
	zest_buffer staging_buffer = zest_CreateStagingBuffer(device, sizeof(source), source);
	zest_buffer_image_copy_t region = {};
	region.image_aspect = zest_image_aspect_color_bit;
	region.layer_count = 1;
	region.buffer_image_height = padded_height;
	region.image_extent.width = width;
	region.image_extent.height = height;
	region.image_extent.depth = depth;
	zest_queue queue = zest_imm_BeginCommandBuffer(device, zest_queue_graphics);
	failed |= !zest_imm_TransitionImage(queue, image, zest_resource_state_copy_dst, 0, 1, 0, 1);
	failed |= !zest_imm_CopyBufferRegionsToImage(queue, &region, 1, staging_buffer, image);
	failed |= !zest_imm_TransitionImage(queue, image, zest_resource_state_shader_read, 0, 1, 0, 1);
	failed |= !zest_imm_EndCommandBuffer(queue);
	zest_FreeBufferNow(staging_buffer);
	failed |= read_image_layers(device, image, 1, sizeof(read_back), read_back);
	failed |= memcmp(expected, read_back, sizeof(read_back)) != 0;
	zest_FreeImageNow(image_handle);
	return failed | finish_device(device);
}
int test_d3d12_3d_upload_slice_pitch(void) {
	int failed = run_3d_upload_slice_pitch(ZEST_FALSE);
	return failed | run_3d_upload_slice_pitch(ZEST_TRUE);
}
int test_d3d12_validation_device_conflict(void) {
	if (!zest__dx_debug_layer_enabled) {
		ZEST_PRINT("D3D12 Validation Device Conflict skipped: the D3D12 debug layer isn't available, so there is nothing to refuse");
		return 0;
	}
	zest_device_builder builder = zest_BeginD3D12DeviceBuilder(zest_device_init_flag_log_validation_errors_to_memory | zest_device_init_flag_log_validation_errors_to_console);
	if (d3d12_use_warp) {
		zest_DeviceBuilderUseWarp(builder);
	}
	zest_device device = zest_EndDeviceBuilder(builder);
	if (!device) return 1;
	//Simulates a process that never enabled the debug layer, where enabling it now would remove the device above
	zest_bool debug_layer_enabled = zest__dx_debug_layer_enabled;
	zest__dx_debug_layer_enabled = ZEST_FALSE;
	zest_device validation_device = zest_EndDeviceBuilder(BeginD3D12TestBuilder());
	int failed = validation_device != NULL;
	//Enabling the layer sets this, so it shows the refusal came before the layer was enabled
	failed |= zest__dx_debug_layer_enabled != ZEST_FALSE;
	if (validation_device) {
		zest_DestroyDevice(validation_device);
	}
	failed |= ZEST_DX_CALL0(zest_GetD3D12Device(device), GetDeviceRemovedReason) != S_OK;
	zest__dx_debug_layer_enabled = debug_layer_enabled;
	return failed | finish_device(device);
}
int test_d3d12_validation_without_debug_layer(void) {
	//The validation device goes on WARP so that the two devices never share an adapter
	zest_device device = zest_EndDeviceBuilder(zest_BeginD3D12DeviceBuilder(zest_device_init_flag_log_validation_errors_to_memory | zest_device_init_flag_log_validation_errors_to_console));
	if (!device) {
		ZEST_PRINT("D3D12 Validation Without Debug Layer skipped: there's no hardware D3D12 adapter");
		return 0;
	}
	//A process without the SDK layers, where enabling the debug layer can't happen and so can't remove anything
	zest_bool debug_layer_enabled = zest__dx_debug_layer_enabled;
	zest__dx_debug_layer_enabled = ZEST_FALSE;
	zest__dx_forced_debug_layer_unavailable = ZEST_TRUE;
	zest_device_builder builder = BeginD3D12TestBuilder();
	zest_DeviceBuilderUseWarp(builder);
	zest_device validation_device = zest_EndDeviceBuilder(builder);
	zest__dx_forced_debug_layer_unavailable = ZEST_FALSE;
	int failed = validation_device == NULL;
	failed |= zest__dx_debug_layer_enabled != ZEST_FALSE;
	if (validation_device) {
		//Validation is dropped and the device carries on without it
		failed |= ZEST__FLAGGED(validation_device->setup_info.flags, zest_device_init_flag_enable_validation_layers);
		zest_DestroyDevice(validation_device);
	}
	failed |= ZEST_DX_CALL0(zest_GetD3D12Device(device), GetDeviceRemovedReason) != S_OK;
	zest__dx_debug_layer_enabled = debug_layer_enabled;
	return failed | finish_device(device);
}
int test_d3d12_recovers_from_failed_close(void) {
	zest_device device = zest_EndDeviceBuilder(BeginD3D12TestBuilder());
	if (!device) return 1;
	//Without the debug layer Close succeeds and the GPU would run the out of range copy
	if (!zest__dx_debug_layer_enabled) {
		ZEST_PRINT("D3D12 Recovers From Failed Close skipped: only the D3D12 debug layer makes Close fail, and it isn't available");
		return finish_device(device);
	}
	zest_image_info_t image_info = zest_CreateImageInfo(4, 4);
	image_info.flags = zest_image_preset_texture;
	zest_image_handle image_handle = zest_CreateImage(device, &image_info);
	zest_image image = zest_GetImage(image_handle);
	zest_buffer buffer = zest_CreateStagingBuffer(device, 256, NULL);
	ID3D12Resource *resource = ZEST_DX_BACKEND(device_memory_pool, buffer->memory_pool)->buffer;
	UINT64 resource_size = zest__dx_resource_description(resource).Width;
	//The debug layer's report of the invalid copy is expected, so it goes to memory only
	ZEST__UNFLAG(device->init_flags, zest_device_init_flag_log_validation_errors_to_console);
	zest_queue queue = zest_imm_BeginCommandBuffer(device, zest_queue_graphics);
	int failed = queue == NULL;
	if (queue) {
		ZEST_DX_CALL(ZEST_DX_BACKEND(queue, queue)->command_list, CopyBufferRegion, resource, 0, resource, resource_size, 256);
	}
	failed |= zest_imm_EndCommandBuffer(queue) != ZEST_FALSE;
	ZEST__FLAG(device->init_flags, zest_device_init_flag_log_validation_errors_to_console);
	zest_ResetValidationErrors(device);
	zest_queue second_queue = zest_imm_BeginCommandBuffer(device, zest_queue_graphics);
	failed |= second_queue == NULL || second_queue != queue;
	failed |= !zest_imm_TransitionImage(second_queue, image, zest_resource_state_copy_dst, 0, 1, 0, 1);
	failed |= !zest_imm_TransitionImage(second_queue, image, zest_resource_state_shader_read, 0, 1, 0, 1);
	failed |= !zest_imm_EndCommandBuffer(second_queue);
	zest_FreeBufferNow(buffer);
	zest_FreeImageNow(image_handle);
	return failed | finish_device(device);
}
static DWORD WINAPI signal_gate_after_delay(void *parameter) {
	Sleep(200);
	ID3D12Fence *gate = (ID3D12Fence *)parameter;
	gate->Signal(1);
	return 0;
}
//Opens the gate after a delay, or straight away when no thread can be started so that nothing waits on it for the full timeout
static HANDLE open_gate_after_delay(ID3D12Fence *gate) {
	HANDLE thread = CreateThread(NULL, 0, signal_gate_after_delay, gate, 0, NULL);
	if (!thread) {
		gate->Signal(1);
	}
	return thread;
}
static void finish_gate(ID3D12Fence *gate, HANDLE thread) {
	if (thread) {
		WaitForSingleObject(thread, INFINITE);
		CloseHandle(thread);
	} else if (gate) {
		gate->Signal(1);
	}
	if (gate) {
		gate->Release();
	}
}
int test_d3d12_begin_waits_for_previous_submission(void) {
	zest_device device = zest_EndDeviceBuilder(BeginD3D12TestBuilder());
	if (!device) return 1;
	ID3D12Fence *gate = NULL;
	int failed = FAILED(zest_GetD3D12Device(device)->CreateFence(0, D3D12_FENCE_FLAG_NONE, __uuidof(ID3D12Fence), (void **)&gate));
	zest_queue queue = failed ? NULL : zest_imm_BeginCommandBuffer(device, zest_queue_graphics);
	failed |= queue == NULL;
	HANDLE thread = NULL;
	if (!failed) {
		//Stands in for an end that timed out: the list is submitted behind a gate and the queue released without waiting
		zest_dx_queue_backend_t *queue_backend = ZEST_DX_BACKEND(queue, queue);
		ID3D12Fence *timeline_fence = ZEST_DX_BACKEND(execution_timeline, &queue->timeline)->fence;
		failed |= FAILED(queue_backend->command_list->Close());
		failed |= FAILED(queue_backend->queue->Wait(gate, 1));
		ID3D12CommandList *command_lists[1] = { queue_backend->command_list };
		queue_backend->queue->ExecuteCommandLists(1, command_lists);
		queue->timeline.current_value++;
		failed |= FAILED(queue_backend->queue->Signal(timeline_fence, queue->timeline.current_value));
		zest__dx_release_immediate_queue(queue, ZEST_FALSE);
		thread = open_gate_after_delay(gate);
		failed |= thread == NULL;
		if (thread) {
			zest_queue second_queue = zest_imm_BeginCommandBuffer(device, zest_queue_graphics);
			failed |= second_queue != queue;
			//The gate only opens after the delay, so it's open here only if the begin waited for the gated list
			failed |= gate->GetCompletedValue() != 1;
			failed |= timeline_fence->GetCompletedValue() < queue->timeline.current_value;
			failed |= !zest_imm_EndCommandBuffer(second_queue);
		}
	}
	finish_gate(gate, thread);
	return failed | finish_device(device);
}
int test_d3d12_recovers_from_failed_signal(void) {
	zest_device device = zest_EndDeviceBuilder(BeginD3D12TestBuilder());
	if (!device) return 1;
	ID3D12Fence *gate = NULL;
	int failed = FAILED(zest_GetD3D12Device(device)->CreateFence(0, D3D12_FENCE_FLAG_NONE, __uuidof(ID3D12Fence), (void **)&gate));
	zest_queue queue = failed ? NULL : zest_imm_BeginCommandBuffer(device, zest_queue_graphics);
	failed |= queue == NULL;
	HANDLE thread = NULL;
	if (!failed) {
		//The list runs behind a gate and the end's signal fails, so it's still executing with nothing to wait on
		failed |= FAILED(ZEST_DX_BACKEND(queue, queue)->queue->Wait(gate, 1));
		zest__dx_forced_signal_failure = ZEST_TRUE;
		failed |= zest_imm_EndCommandBuffer(queue) != ZEST_FALSE;
		zest__dx_forced_signal_failure = ZEST_FALSE;
		thread = open_gate_after_delay(gate);
		failed |= thread == NULL;
		if (thread) {
			zest_queue second_queue = zest_imm_BeginCommandBuffer(device, zest_queue_graphics);
			failed |= second_queue != queue;
			failed |= gate->GetCompletedValue() != 1;
			failed |= ZEST_DX_BACKEND(queue, queue)->unfenced_submission != ZEST_FALSE;
			failed |= !zest_imm_EndCommandBuffer(second_queue);
		}
	}
	finish_gate(gate, thread);
	return failed | finish_device(device);
}
int test_d3d12_feature_level_fallback(void) {
	zest__dx_forced_feature_level_failure = ZEST_TRUE;
	zest_device device = zest_EndDeviceBuilder(BeginD3D12TestBuilder());
	zest__dx_forced_feature_level_failure = ZEST_FALSE;
	if (!device) return 1;
	int failed = ZEST_DX_BACKEND(device, device)->support.feature_level != D3D_FEATURE_LEVEL_12_0;
	return failed | finish_device(device);
}

static D3D12Test d3d12_tests[] = {
	{ "D3D12 Builder", test_d3d12_builder },
	{ "D3D12 Interface IDs", test_d3d12_interface_ids },
	{ "D3D12 Minimum Spec", test_d3d12_minimum_spec },
	{ "D3D12 Capability Translation", test_d3d12_capability_translation },
	{ "D3D12 Cleanup Uninitialised Backend", test_d3d12_cleanup_uninitialised_backend },
	{ "D3D12 Heap Properties", test_d3d12_heap_properties },
	{ "D3D12 Format Round Trip", test_d3d12_format_round_trip },
	{ "D3D12 Image Heap Category", test_d3d12_image_heap_category },
	{ "D3D12 Layout Table", test_d3d12_layout_table },
	{ "D3D12 Row Repack", test_d3d12_row_repack },
	{ "D3D12 Bindless Regions", test_d3d12_bindless_regions },
	{ "D3D12 Device Lifecycle", test_d3d12_device_lifecycle },
	{ "D3D12 Back To Back Devices", test_d3d12_back_to_back_devices },
	{ "D3D12 WARP Device", test_d3d12_warp_device },
	{ "D3D12 Image Round Trip", test_d3d12_image_round_trip },
	{ "D3D12 Allocation Baseline", test_d3d12_allocation_baseline },
	{ "D3D12 Failed Creation", test_d3d12_failed_creation },
	{ "D3D12 Unsupported Format", test_d3d12_unsupported_format },
	{ "D3D12 Sampler Ceiling Clamp", test_d3d12_sampler_ceiling_clamp },
	{ "D3D12 Restricted Copy Pitch", test_d3d12_restricted_copy_pitch },
	{ "D3D12 Transfer Queue Upload", test_d3d12_transfer_queue_upload },
	{ "D3D12 Many Layer Upload", test_d3d12_many_layer_upload },
	{ "D3D12 Second Device Rejected", test_d3d12_second_device_rejected },
	{ "D3D12 Invalid Copy Regions", test_d3d12_invalid_copy_regions },
	{ "D3D12 Unavailable Immediate Commands", test_d3d12_unavailable_immediate_commands },
	{ "D3D12 Pool Failure Rejected", test_d3d12_pool_failure_rejected },
	{ "D3D12 SRV Description", test_d3d12_srv_description },
	{ "D3D12 Depth Deny SRV", test_d3d12_depth_deny_srv },
	{ "D3D12 Format Support", test_d3d12_format_support },
	{ "D3D12 3D Upload Slice Pitch", test_d3d12_3d_upload_slice_pitch },
	{ "D3D12 Validation Device Conflict", test_d3d12_validation_device_conflict },
	{ "D3D12 Recovers From Failed Close", test_d3d12_recovers_from_failed_close },
	{ "D3D12 Begin Waits For Previous Submission", test_d3d12_begin_waits_for_previous_submission },
	{ "D3D12 Feature Level Fallback", test_d3d12_feature_level_fallback },
	{ "D3D12 Validation Without Debug Layer", test_d3d12_validation_without_debug_layer },
	{ "D3D12 Recovers From Failed Signal", test_d3d12_recovers_from_failed_signal },
};

int RunD3D12Suite(int *total_out) {
	int passed = 0;
	int total = (int)(sizeof(d3d12_tests) / sizeof(d3d12_tests[0]));
	for (int index = 0; index != total; ++index) {
		if (d3d12_tests[index].run() == 0) {
			ZEST_PRINT("\033[32m%s test passed\033[0m", d3d12_tests[index].name);
			passed++;
		} else {
			ZEST_PRINT("\033[31m%s test failed\033[0m", d3d12_tests[index].name);
		}
	}
	ZEST_PRINT("%sD3D12 tests completed: %i / %i\033[0m", passed < total ? "\033[31m" : "\033[32m", passed, total);
	*total_out = total;
	return passed;
}
