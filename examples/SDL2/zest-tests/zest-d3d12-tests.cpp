#include <dxcapi.h>
typedef int (*d3d12_test_callback)(void);

struct D3D12Test {
	const char *name;
	d3d12_test_callback run;
};

static zest_bool d3d12_use_warp = ZEST_FALSE;
static zest_bool d3d12_use_gpu_validation = ZEST_FALSE;

static zest_device_init_flags D3D12TestFlags(void) {
	zest_device_init_flags flags = zest_device_init_flag_enable_validation_layers | zest_device_init_flag_log_validation_errors_to_memory | zest_device_init_flag_log_validation_errors_to_console;
	if (d3d12_use_gpu_validation) {
		flags |= zest_device_init_flag_enable_validation_layers_with_sync;
	}
	return flags;
}

zest_device_builder BeginD3D12TestBuilder(void) {
	zest_device_builder builder = zest_BeginD3D12DeviceBuilder(D3D12TestFlags());
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

//DXGI_DEBUG_DXGI is only declared by the headers, and defined in dxguid.lib
#pragma comment(lib, "dxguid.lib")
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
	CHECK_INTERFACE_ID(IDXGISwapChain1);
	CHECK_INTERFACE_ID(IDXGISwapChain3);
	CHECK_INTERFACE_ID(IDXGIInfoQueue);
	CHECK_INTERFACE_ID(ID3D12Device5);
	CHECK_INTERFACE_ID(ID3D12RootSignature);
	CHECK_INTERFACE_ID(ID3D12PipelineState);
	failed |= memcmp(&zest__dx_iid_IDxcCompiler3, &__uuidof(IDxcCompiler3), sizeof(GUID)) != 0;
	failed |= memcmp(&zest__dx_iid_IDxcResult, &__uuidof(IDxcResult), sizeof(GUID)) != 0;
	failed |= memcmp(&zest__dx_clsid_DxcCompiler, &CLSID_DxcCompiler, sizeof(GUID)) != 0;
	failed |= memcmp(&zest__dx_debug_dxgi, &DXGI_DEBUG_DXGI, sizeof(GUID)) != 0;
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
	int failed = zest__dx_image_heap_category(D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET, 4, D3D12_RESOURCE_HEAP_TIER_2) != ZEST_DX_HEAP_CATEGORY_MSAA;
	failed |= zest__dx_image_heap_category(D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET, 1, D3D12_RESOURCE_HEAP_TIER_1) != ZEST_DX_HEAP_CATEGORY_TARGETS;
	failed |= zest__dx_image_heap_category(D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET, 1, D3D12_RESOURCE_HEAP_TIER_2) != ZEST_DX_HEAP_CATEGORY_TEXTURES;
	failed |= zest__dx_image_heap_category(D3D12_RESOURCE_FLAG_NONE, 1, D3D12_RESOURCE_HEAP_TIER_1) != ZEST_DX_HEAP_CATEGORY_TEXTURES;
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
	failed |= regions.transient_offset != 6720 || regions.transient_count != ZEST_DX_TRANSIENT_DESCRIPTOR_COUNT;
	failed |= regions.resource_descriptor_count != 6720 + ZEST_DX_TRANSIENT_DESCRIPTOR_COUNT;
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
//Raw buffer views need a 16 byte aligned offset that is also a multiple of the structured stride
int test_d3d12_stride_aligned_allocation(void) {
	zest_device device = zest_EndDeviceBuilder(BeginD3D12TestBuilder());
	if (!device) return 1;
	const zest_size strides[] = { 48, 76, 96, 112, 912 };
	const int stride_count = (int)(sizeof(strides) / sizeof(strides[0]));
	zest_buffer blockers[sizeof(strides) / sizeof(strides[0])] = {};
	zest_buffer buffers[sizeof(strides) / sizeof(strides[0])] = {};
	int failed = device->min_storage_buffer_offset_alignment != D3D12_RAW_UAV_SRV_BYTE_ALIGNMENT;
	zest_buffer_info_t info = zest_CreateBufferInfo(zest_buffer_type_storage, zest_memory_usage_gpu_only);
	for (int index = 0; index != stride_count; ++index) {
		blockers[index] = zest_CreateBuffer(device, 300, &info);
		zest_buffer_info_t stride_info = info;
		stride_info.element_alignment = strides[index];
		buffers[index] = zest_CreateBuffer(device, strides[index] * 37, &stride_info);
		if (!blockers[index] || !buffers[index]) {
			failed = 1;
			continue;
		}
		failed |= buffers[index]->memory_offset % strides[index] != 0;
		failed |= buffers[index]->memory_offset % 16 != 0;
		failed |= buffers[index]->size < strides[index] * 37;
	}
	for (int index = 0; index != stride_count; ++index) {
		zest_FreeBufferNow(buffers[index]);
		zest_FreeBufferNow(blockers[index]);
	}
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
static const char *hlsl_vertex_source =
	"float4 main(uint vertex_id : SV_VertexID) : SV_Position {\n"
	"	float2 corner = float2((vertex_id << 1) & 2, vertex_id & 2);\n"
	"	return float4(corner * 2.0 - 1.0, 0.0, 1.0);\n"
	"}\n";
static const char *hlsl_pixel_source =
	"float4 main() : SV_Target {\n"
	"	return float4(1.0, 0.5, 0.25, 1.0);\n"
	"}\n";

static int check_dxil_container(ID3DBlob *dxil) {
	if (!dxil) return 1;
	const zest_uint *words = (const zest_uint *)ZEST_DX_CALL0(dxil, GetBufferPointer);
	SIZE_T size = ZEST_DX_CALL0(dxil, GetBufferSize);
	return size < 32 || memcmp(words, "DXBC", 4) != 0 || words[6] != (zest_uint)size;
}

int test_d3d12_hlsl_compile(void) {
	zest_device device = zest_EndDeviceBuilder(BeginD3D12TestBuilder());
	if (!device) return 1;
	int failed = 0;
	ID3DBlob *vertex_dxil = NULL;
	ID3DBlob *pixel_dxil = NULL;
	failed |= !zest__dx_compile_hlsl(device, hlsl_vertex_source, (zest_size)strlen(hlsl_vertex_source), "test_vertex", "main", "vs_6_0", &vertex_dxil);
	failed |= check_dxil_container(vertex_dxil);
	zest_dxc_compiler3_t *compiler = ZEST_DX_BACKEND(device, device)->dxc_compiler;
	failed |= compiler == NULL;
	failed |= !zest__dx_compile_hlsl(device, hlsl_pixel_source, (zest_size)strlen(hlsl_pixel_source), "test_pixel", "main", "ps_6_0", &pixel_dxil);
	failed |= check_dxil_container(pixel_dxil);
	failed |= ZEST_DX_BACKEND(device, device)->dxc_compiler != compiler;
	ZEST_DX_RELEASE(vertex_dxil);
	ZEST_DX_RELEASE(pixel_dxil);
	return failed | finish_device(device);
}

int test_d3d12_hlsl_compile_error(void) {
	zest_device device = zest_EndDeviceBuilder(BeginD3D12TestBuilder());
	if (!device) return 1;
	int failed = 0;
	const char *broken_source = "float4 main( : SV_Target { return float4(1.0; }\n";
	ID3DBlob *dxil = (ID3DBlob *)(void *)&failed;
	failed |= zest__dx_compile_hlsl(device, broken_source, (zest_size)strlen(broken_source), "broken", "main", "ps_6_0", &dxil) != ZEST_FALSE;
	failed |= dxil != NULL;
	failed |= !zest__dx_compile_hlsl(device, hlsl_pixel_source, (zest_size)strlen(hlsl_pixel_source), "test_pixel", "main", "ps_6_0", &dxil);
	failed |= check_dxil_container(dxil);
	ZEST_DX_RELEASE(dxil);
	return failed | finish_device(device);
}

int test_d3d12_dxc_missing(void) {
	zest_device device = zest_EndDeviceBuilder(BeginD3D12TestBuilder());
	if (!device) return 1;
	int failed = 0;
	ID3DBlob *dxil = NULL;
	zest_uint error_count = zest_GetValidationErrorCount(device);
	zest__dx_forced_dxc_library = "zest-missing-dxcompiler.dll";
	failed |= zest__dx_compile_hlsl(device, hlsl_pixel_source, (zest_size)strlen(hlsl_pixel_source), "test_pixel", "main", "ps_6_0", &dxil) != ZEST_FALSE;
	zest__dx_forced_dxc_library = NULL;
	failed |= dxil != NULL;
	failed |= zest_GetValidationErrorCount(device) != error_count + 1;
	failed |= ZEST_DX_BACKEND(device, device)->dxc_compiler != NULL;
	failed |= !zest__dx_compile_hlsl(device, hlsl_pixel_source, (zest_size)strlen(hlsl_pixel_source), "test_pixel", "main", "ps_6_0", &dxil);
	failed |= check_dxil_container(dxil);
	ZEST_DX_RELEASE(dxil);
	zest_ResetValidationErrors(device);
	return failed | finish_device(device);
}
int test_d3d12_unavailable_immediate_commands(void) {
	zest_device device = zest_EndDeviceBuilder(BeginD3D12TestBuilder());
	if (!device) return 1;
	zest_buffer buffer = zest_CreateStagingBuffer(device, 256, NULL);
	zest_queue queue = zest_imm_BeginCommandBuffer(device, zest_queue_graphics);
	int failed = queue == NULL || buffer == NULL;
	zest_imm_SendPushConstants(queue, NULL, 0);
	zest_imm_BindComputePipeline(queue, NULL);
	failed |= zest_imm_DispatchCompute(queue, 1, 1, 1) != ZEST_FALSE;
	failed |= !zest_imm_EndCommandBuffer(queue);
	failed |= zest_GetValidationErrorCount(device) != 3;
	zest_FreeBufferNow(buffer);
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
int test_d3d12_image_barrier_builder(void) {
	zest_image_t image = {};
	zest_dx_image_backend_t backend = {};
	image.backend = (zest_image_backend)&backend;
	image.info.format = zest_format_r8g8b8a8_unorm;
	image.info.sample_count = 1;
	image.info.flags = zest_image_flag_color_attachment;
	backend.sample_count = 1;
	backend.layout = D3D12_BARRIER_LAYOUT_PRESENT;
	D3D12_TEXTURE_BARRIER barrier = zest__dx_image_barrier(&image, D3D12_BARRIER_LAYOUT_RENDER_TARGET, 0, zest_access_color_attachment_write_bit, zest_pipeline_stage_color_attachment_output_bit, zest_pipeline_stage_color_attachment_output_bit, 0, 1, 0, 1);
	int failed = barrier.LayoutBefore != D3D12_BARRIER_LAYOUT_PRESENT || barrier.LayoutAfter != D3D12_BARRIER_LAYOUT_RENDER_TARGET;
	failed |= barrier.AccessBefore != D3D12_BARRIER_ACCESS_NO_ACCESS || barrier.SyncBefore != D3D12_BARRIER_SYNC_NONE;
	failed |= barrier.AccessAfter != D3D12_BARRIER_ACCESS_RENDER_TARGET || barrier.SyncAfter != D3D12_BARRIER_SYNC_RENDER_TARGET;
	failed |= barrier.Flags != D3D12_TEXTURE_BARRIER_FLAG_NONE;
	backend.layout = D3D12_BARRIER_LAYOUT_RENDER_TARGET;
	barrier = zest__dx_image_barrier(&image, D3D12_BARRIER_LAYOUT_PRESENT, zest_access_color_attachment_write_bit, 0, zest_pipeline_stage_color_attachment_output_bit, zest_pipeline_stage_bottom_of_pipe_bit, 0, 1, 0, 1);
	failed |= barrier.AccessAfter != D3D12_BARRIER_ACCESS_NO_ACCESS || barrier.SyncAfter != D3D12_BARRIER_SYNC_NONE;
	failed |= barrier.AccessBefore != D3D12_BARRIER_ACCESS_RENDER_TARGET || barrier.SyncBefore != D3D12_BARRIER_SYNC_RENDER_TARGET;
	backend.layout = D3D12_BARRIER_LAYOUT_UNDEFINED;
	backend.resource_flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
	barrier = zest__dx_image_barrier(&image, D3D12_BARRIER_LAYOUT_RENDER_TARGET, 0, zest_access_color_attachment_write_bit, zest_pipeline_stage_top_of_pipe_bit, zest_pipeline_stage_color_attachment_output_bit, 0, 1, 0, 1);
	failed |= !(barrier.Flags & D3D12_TEXTURE_BARRIER_FLAG_DISCARD);
	image.info.flags = zest_image_preset_texture;
	backend.resource_flags = D3D12_RESOURCE_FLAG_NONE;
	barrier = zest__dx_image_barrier(&image, D3D12_BARRIER_LAYOUT_SHADER_RESOURCE, 0, zest_access_shader_read_bit, zest_pipeline_stage_top_of_pipe_bit, zest_pipeline_stage_fragment_shader_bit, 0, 1, 0, 1);
	failed |= barrier.Flags != D3D12_TEXTURE_BARRIER_FLAG_NONE;
	D3D12_BARRIER_LAYOUT copy_layout = zest__dx_barrier_layout(zest_image_layout_shader_read_only_optimal, zest_access_shader_read_bit, zest_queue_transfer);
	barrier = zest__dx_image_barrier(&image, copy_layout, 0, zest_access_shader_read_bit, zest_pipeline_stage_top_of_pipe_bit, zest_pipeline_stage_transfer_bit, 0, 1, 0, 1);
	failed |= barrier.LayoutAfter != D3D12_BARRIER_LAYOUT_COMMON || barrier.AccessAfter != D3D12_BARRIER_ACCESS_SHADER_RESOURCE;
	return failed;
}
int test_d3d12_image_resource_flags(void) {
	zest_device device = zest_EndDeviceBuilder(BeginD3D12TestBuilder());
	if (!device) return 1;
	int failed = zest__dx_image_resource_flags(device, zest_image_preset_texture, DXGI_FORMAT_R8G8B8A8_UNORM, 1) != D3D12_RESOURCE_FLAG_NONE;
	failed |= zest__dx_image_resource_flags(device, zest_image_preset_texture, DXGI_FORMAT_R8G8B8A8_UNORM, 4) != D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
	failed |= zest__dx_image_resource_flags(device, zest_image_preset_texture_mipmaps, DXGI_FORMAT_R8G8B8A8_UNORM, 4) != D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
	failed |= zest__dx_image_resource_flags(device, zest_image_preset_texture_mipmaps, DXGI_FORMAT_BC1_UNORM, 4) != D3D12_RESOURCE_FLAG_NONE;
	failed |= zest__dx_image_resource_flags(device, zest_image_flag_transfer_dst | zest_image_flag_sampled, DXGI_FORMAT_D32_FLOAT, 4) != D3D12_RESOURCE_FLAG_NONE;
	failed |= zest__dx_image_resource_flags(device, zest_image_preset_depth_attachment, DXGI_FORMAT_D32_FLOAT, 1) != D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
	failed |= zest__dx_image_resource_flags(device, zest_image_preset_storage, DXGI_FORMAT_R8G8B8A8_UNORM, 1) != D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
	failed |= zest__dx_image_resource_flags(device, zest_image_preset_color_attachment, DXGI_FORMAT_R8G8B8A8_UNORM, 1) != D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
	return failed | finish_device(device);
}
int test_d3d12_discard_follows_resource_flags(void) {
	zest_image_t image = {};
	zest_dx_image_backend_t backend = {};
	image.backend = (zest_image_backend)&backend;
	image.info.format = zest_format_r8g8b8a8_unorm;
	image.info.sample_count = 1;
	image.info.mip_levels = 4;
	image.info.flags = zest_image_preset_texture_mipmaps;
	backend.sample_count = 1;
	backend.layout = D3D12_BARRIER_LAYOUT_UNDEFINED;
	backend.resource_flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
	D3D12_TEXTURE_BARRIER barrier = zest__dx_image_barrier(&image, D3D12_BARRIER_LAYOUT_COPY_DEST, 0, zest_access_transfer_write_bit, zest_pipeline_stage_top_of_pipe_bit, zest_pipeline_stage_transfer_bit, 0, 4, 0, 1);
	int failed = !(barrier.Flags & D3D12_TEXTURE_BARRIER_FLAG_DISCARD);
	backend.resource_flags = D3D12_RESOURCE_FLAG_NONE;
	barrier = zest__dx_image_barrier(&image, D3D12_BARRIER_LAYOUT_COPY_DEST, 0, zest_access_transfer_write_bit, zest_pipeline_stage_top_of_pipe_bit, zest_pipeline_stage_transfer_bit, 0, 4, 0, 1);
	failed |= barrier.Flags != D3D12_TEXTURE_BARRIER_FLAG_NONE;
	//Zest's attachment flag alone doesn't make the resource a target
	image.info.flags = zest_image_preset_color_attachment;
	barrier = zest__dx_image_barrier(&image, D3D12_BARRIER_LAYOUT_RENDER_TARGET, 0, zest_access_color_attachment_write_bit, zest_pipeline_stage_top_of_pipe_bit, zest_pipeline_stage_color_attachment_output_bit, 0, 1, 0, 1);
	failed |= barrier.Flags != D3D12_TEXTURE_BARRIER_FLAG_NONE;
	image.info.format = zest_format_d32_sfloat;
	image.info.flags = zest_image_preset_texture;
	backend.resource_flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
	barrier = zest__dx_image_barrier(&image, D3D12_BARRIER_LAYOUT_DEPTH_STENCIL_WRITE, 0, zest_access_depth_stencil_attachment_write_bit, zest_pipeline_stage_top_of_pipe_bit, zest_pipeline_stage_early_fragment_tests_bit, 0, 1, 0, 1);
	failed |= !(barrier.Flags & D3D12_TEXTURE_BARRIER_FLAG_DISCARD);
	return failed;
}
int test_d3d12_attachment_views(void) {
	zest_device device = zest_EndDeviceBuilder(BeginD3D12TestBuilder());
	if (!device) return 1;
	zest_image_info_t image_info = zest_CreateImageInfo(16, 16);
	image_info.format = zest_format_r8g8b8a8_unorm;
	image_info.flags = zest_image_preset_color_attachment;
	zest_image_handle color_handle = zest_CreateImage(device, &image_info);
	image_info.format = zest_format_d32_sfloat;
	image_info.flags = zest_image_preset_depth_attachment;
	zest_image_handle depth_handle = zest_CreateImage(device, &image_info);
	image_info.format = zest_format_r8g8b8a8_unorm;
	image_info.flags = zest_image_preset_texture;
	zest_image_handle texture_handle = zest_CreateImage(device, &image_info);
	zest_image color = zest_GetImage(color_handle);
	zest_image depth = zest_GetImage(depth_handle);
	zest_image texture = zest_GetImage(texture_handle);
	int failed = !color || !depth || !texture;
	if (!failed) {
		zest_dx_image_view_backend_t *color_view = ZEST_DX_BACKEND(image_view, color->default_view);
		zest_dx_image_view_backend_t *depth_view = ZEST_DX_BACKEND(image_view, depth->default_view);
		zest_dx_image_view_backend_t *texture_view = ZEST_DX_BACKEND(image_view, texture->default_view);
		failed |= !color_view->rtv.ptr || color_view->dsv.ptr;
		failed |= !depth_view->dsv.ptr || depth_view->rtv.ptr;
		failed |= texture_view->rtv.ptr || texture_view->dsv.ptr;
	}
	zest_FreeImageNow(color_handle);
	zest_FreeImageNow(depth_handle);
	zest_FreeImageNow(texture_handle);
	return failed | finish_device(device);
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

int test_d3d12_device_reset(void) {
	zest_device device = zest_EndDeviceBuilder(BeginD3D12TestBuilder());
	if (!device) return 1;
	int failed = image_round_trip(device);
	zest_ResetDevice(device);
	failed |= zest_GetValidationErrorCount(device) != 0;
	failed |= zest__dx_last_device_release_count() != 0;
	failed |= image_round_trip(device);
	zest_ResetDevice(device);
	failed |= image_round_trip(device);
	return failed | finish_device(device);
}

int test_d3d12_failed_reset_teardown(void) {
	zest_device device = zest_EndDeviceBuilder(BeginD3D12TestBuilder());
	if (!device) return 1;
	ID3D12Device *native_device = zest_GetD3D12Device(device);
	native_device->AddRef();
	int failed = zest__dx_reinit_logical_device(device) != ZEST_FALSE;
	failed |= zest__dx_last_device_release_count() == 0;
	failed |= zest_GetD3D12Device(device) != NULL;
	zest_DestroyDevice(device);
	//Zero means nothing Zest created still held the device
	failed |= native_device->Release() != 0;
	return failed;
}

int test_d3d12_sdl_helpers(void) {
	zest_bool initialised_video = ZEST_FALSE;
	if (SDL_WasInit(SDL_INIT_VIDEO) == 0) {
		if (SDL_Init(SDL_INIT_VIDEO) != 0) return 1;
		initialised_video = ZEST_TRUE;
	}
	int failed = 0;
	zest_window_data_t window_data = zest_implsdl2_CreateWindow(50, 50, 320, 200, 0, "D3D12 Helpers");
	failed |= window_data.window_handle == NULL || window_data.native_handle == NULL;
	int framebuffer_width = 0, framebuffer_height = 0, window_width = 0, window_height = 0;
	window_data.window_sizes_callback(&window_data, &framebuffer_width, &framebuffer_height, &window_width, &window_height);
	failed |= framebuffer_width < 320 || framebuffer_height < 200 || window_width != 320 || window_height != 200;
	zest_device device = zest_implsdl2_CreateD3D12Device(&window_data, D3D12TestFlags() | (d3d12_use_warp ? zest_device_init_flag_use_warp : 0));
	if (device) {
		failed |= zest_GetDevicePlatform(device) != zest_platform_d3d12;
		failed |= finish_device(device);
	} else {
		failed = 1;
	}
	if (window_data.window_handle) {
		SDL_DestroyWindow((SDL_Window *)window_data.window_handle);
	}
	SDL_Window *plain_window = SDL_CreateWindow("D3D12 Plain", 50, 50, 320, 200, SDL_WINDOW_ALLOW_HIGHDPI);
	if (plain_window) {
		zest_window_data_t plain_data = { 0 };
		plain_data.window_handle = plain_window;
		zest_implsdl2_GetWindowSizeCallback(&plain_data, &framebuffer_width, &framebuffer_height, &window_width, &window_height);
		failed |= framebuffer_width < 320 || framebuffer_height < 200 || window_width != 320 || window_height != 200;
		SDL_DestroyWindow(plain_window);
	} else {
		failed = 1;
	}
	if (initialised_video) {
		SDL_QuitSubSystem(SDL_INIT_VIDEO);
	}
	return failed;
}

int test_d3d12_render_pass_access(void) {
	int failed = zest__dx_beginning_access(zest_load_op_clear) != D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_CLEAR;
	failed |= zest__dx_beginning_access(zest_load_op_load) != D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_PRESERVE;
	failed |= zest__dx_beginning_access(zest_load_op_dont_care) != D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_DISCARD;
	failed |= zest__dx_ending_access(zest_store_op_store) != D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_PRESERVE;
	failed |= zest__dx_ending_access(zest_store_op_dont_care) != D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_DISCARD;
	return failed;
}

int test_d3d12_viewport_flip(void) {
	D3D12_VIEWPORT viewport = zest__dx_viewport(0.f, 0.f, 640.f, 480.f, 0.f, 1.f);
	int failed = viewport.TopLeftX != 0.f || viewport.TopLeftY != 480.f || viewport.Width != 640.f || viewport.Height != -480.f;
	failed |= viewport.MinDepth != 0.f || viewport.MaxDepth != 1.f;
	viewport = zest__dx_viewport(10.f, 20.f, 100.f, 50.f, 0.f, 1.f);
	failed |= viewport.TopLeftX != 10.f || viewport.TopLeftY != 70.f || viewport.Width != 100.f || viewport.Height != -50.f;
	//A viewport already flipped the Vulkan way comes out as a plain one
	viewport = zest__dx_viewport(0.f, 480.f, 640.f, -480.f, 0.f, 1.f);
	failed |= viewport.TopLeftY != 0.f || viewport.Height != 480.f || viewport.Width != 640.f;
	return failed;
}

int test_d3d12_empty_command_graph(void) {
	zest_device device = zest_EndDeviceBuilder(BeginD3D12TestBuilder());
	if (!device) return 1;
	zest_create_context_info_t info = zest_CreateContextInfo();
	zest_context context = zest_CreateHeadlessContext(device, &info);
	int failed = context == NULL;
	if (context) {
		failed |= !zest_BeginCommandGraph(context, "Empty", 0);
		zest_frame_graph frame_graph = zest_EndFrameGraph();
		failed |= frame_graph == NULL || ZEST__NOT_FLAGGED(zest_GetFrameGraphResult(frame_graph), zest_fgs_no_work_to_do);
		if (frame_graph) {
			failed |= zest_FlushFrameGraph(frame_graph) != zest_semaphore_status_success;
		}
		zest_DestroyContext(context);
	}
	return failed | finish_device(device);
}

static int pixels_match(const zest_uint *pixels, zest_uint pixel_count, const int expected[4]) {
	for (zest_uint index = 0; index != pixel_count; ++index) {
		const unsigned char *channels = (const unsigned char *)&pixels[index];
		for (int channel = 0; channel != 4; ++channel) {
			if (abs((int)channels[channel] - expected[channel]) > 1) return 0;
		}
	}
	return 1;
}

static const int blue_expected[4] = { 51, 102, 153, 255 };
static const int orange_expected[4] = { 153, 102, 51, 255 };

static zest_image_handle create_clear_target(zest_device device) {
	zest_image_info_t image_info = zest_CreateImageInfo(16, 16);
	image_info.format = zest_format_r8g8b8a8_unorm;
	image_info.flags = zest_image_preset_color_attachment | zest_image_flag_transfer_src;
	return zest_CreateImage(device, &image_info);
}

//Clears the image to blue, or orange, in a one pass command graph named "Clear", then reads it back and checks every pixel
static int run_clear_graph(zest_context context, zest_image image, zest_bool orange, zest_semaphore_status expected_status) {
	if (!zest_BeginCommandGraph(context, "Clear", 0)) return 1;
	zest_resource_node node = zest_ImportImageResource("Target", image, 0);
	if (orange) {
		zest_SetResourceClearColor(node, 0.6f, 0.4f, 0.2f, 1.0f);
	} else {
		zest_SetResourceClearColor(node, 0.2f, 0.4f, 0.6f, 1.0f);
	}
	zest_BeginRenderPass("Clear Pass");
	zest_ConnectOutput(node);
	zest_SetPassTask(zest_EmptyRenderPass, 0);
	zest_EndPass();
	zest_frame_graph frame_graph = zest_EndFrameGraph();
	int failed = zest_FlushFrameGraphAndWait(frame_graph) != expected_status;
	if (expected_status != zest_semaphore_status_success) return failed;
	zest_uint pixels[16 * 16];
	memset(pixels, 0, sizeof(pixels));
	failed |= !zest_CopyImageToBitmap(context->device, image, pixels);
	failed |= !pixels_match(pixels, 16 * 16, orange ? orange_expected : blue_expected);
	return failed;
}

int test_d3d12_render_pass_clear(void) {
	zest_device device = zest_EndDeviceBuilder(BeginD3D12TestBuilder());
	if (!device) return 1;
	zest_create_context_info_t info = zest_CreateContextInfo();
	zest_context context = zest_CreateHeadlessContext(device, &info);
	int failed = context == NULL;
	zest_image_handle image_handle = create_clear_target(device);
	zest_image image = zest_GetImage(image_handle);
	failed |= image == NULL;
	ID3D12GraphicsCommandList7 *first_command_list = NULL;
	//A headless context stays on one frame in flight, so every iteration resets and reuses the one command list
	for (int iteration = 0; iteration != 3 && !failed; ++iteration) {
		failed |= run_clear_graph(context, image, iteration == 1, zest_semaphore_status_success);
		if (failed) break;
		zest_context_queue context_queue = *zest_map_at(context->cached_frame_graph_queues[context->graphics_queue_index], "Clear");
		ID3D12GraphicsCommandList7 *command_list = ZEST_DX_BACKEND(context_queue, context_queue)->command_list;
		if (iteration == 0) {
			first_command_list = command_list;
		}
		failed |= command_list == NULL || command_list != first_command_list;
	}
	zest_FreeImageNow(image_handle);
	if (context) {
		zest_DestroyContext(context);
	}
	return failed | finish_device(device);
}

int test_d3d12_failed_batch_signal_drains(void) {
	zest_device device = zest_EndDeviceBuilder(BeginD3D12TestBuilder());
	if (!device) return 1;
	zest_create_context_info_t info = zest_CreateContextInfo();
	zest_context context = zest_CreateHeadlessContext(device, &info);
	ID3D12Fence *gate = NULL;
	int failed = context == NULL || FAILED(zest_GetD3D12Device(device)->CreateFence(0, D3D12_FENCE_FLAG_NONE, __uuidof(ID3D12Fence), (void **)&gate));
	zest_image_handle image_handle = create_clear_target(device);
	zest_image image = zest_GetImage(image_handle);
	failed |= image == NULL;
	HANDLE thread = NULL;
	if (!failed) {
		//Holds back the queue the context picks first, so the graph's list is still running when its signal fails
		zest_queue first_queue = &device->queue_families[context->graphics_family_index]->queues[context->preferred_queue_index[context->graphics_queue_index]];
		failed |= FAILED(ZEST_DX_BACKEND(queue, first_queue)->queue->Wait(gate, 1));
		thread = open_gate_after_delay(gate);
		failed |= thread == NULL;
		zest__dx_forced_signal_failure = ZEST_TRUE;
		failed |= run_clear_graph(context, image, ZEST_FALSE, zest_semaphore_status_error);
		zest__dx_forced_signal_failure = ZEST_FALSE;
		//The gate only opens after the delay, so it's open here only if the failed submission drained the device
		failed |= gate->GetCompletedValue() != 1;
		//The next flush of the same graph resets the list and allocator the failed submission used
		failed |= run_clear_graph(context, image, ZEST_TRUE, zest_semaphore_status_success);
	}
	finish_gate(gate, thread);
	zest_FreeImageNow(image_handle);
	if (context) {
		zest_DestroyContext(context);
	}
	return failed | finish_device(device);
}

int test_d3d12_swapchain_formats(void) {
	DXGI_FORMAT buffer_format = DXGI_FORMAT_UNKNOWN;
	DXGI_FORMAT view_format = DXGI_FORMAT_UNKNOWN;
	zest_format reported = zest_format_undefined;
	int failed = zest__dx_swapchain_formats(zest_format_b8g8r8a8_unorm, &buffer_format, &view_format, &reported) != ZEST_TRUE;
	failed |= buffer_format != DXGI_FORMAT_B8G8R8A8_UNORM || view_format != DXGI_FORMAT_B8G8R8A8_UNORM || reported != zest_format_b8g8r8a8_unorm;
	failed |= zest__dx_swapchain_formats(zest_format_b8g8r8a8_srgb, &buffer_format, &view_format, &reported) != ZEST_TRUE;
	failed |= buffer_format != DXGI_FORMAT_B8G8R8A8_UNORM || view_format != DXGI_FORMAT_B8G8R8A8_UNORM_SRGB || reported != zest_format_b8g8r8a8_srgb;
	failed |= zest__dx_swapchain_formats(zest_format_r8g8b8a8_srgb, &buffer_format, &view_format, &reported) != ZEST_TRUE;
	failed |= buffer_format != DXGI_FORMAT_R8G8B8A8_UNORM || view_format != DXGI_FORMAT_R8G8B8A8_UNORM_SRGB || reported != zest_format_r8g8b8a8_srgb;
	failed |= zest__dx_swapchain_formats(zest_format_r16g16b16a16_sfloat, &buffer_format, &view_format, &reported) != ZEST_TRUE;
	failed |= buffer_format != DXGI_FORMAT_R16G16B16A16_FLOAT || view_format != DXGI_FORMAT_R16G16B16A16_FLOAT || reported != zest_format_r16g16b16a16_sfloat;
	failed |= zest__dx_swapchain_formats(zest_format_r8_unorm, &buffer_format, &view_format, &reported) != ZEST_FALSE;
	failed |= buffer_format != DXGI_FORMAT_B8G8R8A8_UNORM || view_format != DXGI_FORMAT_B8G8R8A8_UNORM || reported != zest_format_b8g8r8a8_unorm;
	return failed;
}

int test_d3d12_present_parameters(void) {
	UINT sync_interval = 99;
	UINT flags = 99;
	zest__dx_present_parameters(ZEST_TRUE, ZEST_TRUE, &sync_interval, &flags);
	int failed = sync_interval != 1 || flags != 0;
	zest__dx_present_parameters(ZEST_TRUE, ZEST_FALSE, &sync_interval, &flags);
	failed |= sync_interval != 1 || flags != 0;
	zest__dx_present_parameters(ZEST_FALSE, ZEST_TRUE, &sync_interval, &flags);
	failed |= sync_interval != 0 || flags != DXGI_PRESENT_ALLOW_TEARING;
	zest__dx_present_parameters(ZEST_FALSE, ZEST_FALSE, &sync_interval, &flags);
	failed |= sync_interval != 0 || flags != 0;
	return failed;
}

//A window of its own, with SDL's video started first when nothing else has started it
struct D3D12TestWindow {
	zest_window_data_t window_data;
	zest_bool initialised_video;
};

static zest_bool open_test_window(D3D12TestWindow *window, const char *title) {
	window->window_data = ZEST__ZERO_INIT(zest_window_data_t);
	window->initialised_video = ZEST_FALSE;
	if (SDL_WasInit(SDL_INIT_VIDEO) == 0) {
		if (SDL_Init(SDL_INIT_VIDEO) != 0) return ZEST_FALSE;
		window->initialised_video = ZEST_TRUE;
	}
	window->window_data = zest_implsdl2_CreateWindow(50, 50, 320, 200, 0, title);
	return window->window_data.window_handle != NULL;
}

//zest_implsdl2_DestroyWindow needs a context, and the context is gone by now
static void close_test_window(D3D12TestWindow *window) {
	if (window->window_data.window_handle) {
		SDL_DestroyWindow((SDL_Window *)window->window_data.window_handle);
	}
	if (window->initialised_video) {
		SDL_QuitSubSystem(SDL_INIT_VIDEO);
	}
}

static zest_device create_windowed_test_device(D3D12TestWindow *window) {
	return zest_implsdl2_CreateD3D12Device(&window->window_data, D3D12TestFlags() | (d3d12_use_warp ? zest_device_init_flag_use_warp : 0));
}

static void set_test_clear_color(zest_context context) {
	zest_SetSwapchainClearColor(context, 0.1f, 0.2f, 0.3f, 1.0f);
}

struct D3D12FrameTimes {
	zest_microsecs begin;
	zest_microsecs end;
};

//One frame whose cached graph imports the swapchain and clears it, returning whether zest_BeginFrame acquired an image
static zest_bool run_windowed_clear_frame(zest_context context, D3D12FrameTimes *times) {
	SDL_Event event;
	while (SDL_PollEvent(&event)) {}
	zest_UpdateDevice(context->device);
	zest_frame_graph_cache_key_t cache_key = zest_InitialiseCacheKey(context, 0, 0);
	zest_microsecs start = zest_Microsecs();
	zest_bool began = zest_BeginFrame(context);
	times->begin = zest_Microsecs() - start;
	times->end = 0;
	if (!began) return ZEST_FALSE;
	zest_frame_graph frame_graph = zest_GetCachedFrameGraph(context, &cache_key);
	if (!frame_graph && zest_BeginFrameGraph(context, "Windowed Clear", &cache_key)) {
		zest_ImportSwapchainResource();
		zest_BeginRenderPass("Clear Swapchain");
		zest_ConnectSwapChainOutput();
		zest_SetPassTask(zest_EmptyRenderPass, 0);
		zest_EndPass();
		frame_graph = zest_EndFrameGraph();
	}
	start = zest_Microsecs();
	zest_EndFrame(context, frame_graph);
	times->end = zest_Microsecs() - start;
	return ZEST_TRUE;
}

struct D3D12FrameRun {
	int began_count;
	int submitted_count;
	int recreate_count;
	zest_microsecs slowest_frame;
};

//Puts back the clear colour each recreate loses
static D3D12FrameRun run_windowed_clear_frames(zest_context context, int frame_count, zest_bool stop_at_recreate) {
	D3D12FrameRun run = {};
	for (int frame = 0; frame != frame_count; ++frame) {
		D3D12FrameTimes times;
		if (run_windowed_clear_frame(context, &times)) {
			run.began_count++;
			run.submitted_count += ZEST__FLAGGED(context->flags, zest_context_flag_work_was_submitted);
		}
		run.slowest_frame = ZEST__MAX(run.slowest_frame, times.begin + times.end);
		if (zest_SwapchainWasRecreated(context)) {
			run.recreate_count++;
			set_test_clear_color(context);
			if (stop_at_recreate) break;
		}
	}
	return run;
}

static int resize_test_window(zest_context context, SDL_Window *window, int width, int height) {
	SDL_SetWindowSize(window, width, height);
	int failed = run_windowed_clear_frames(context, 10, ZEST_TRUE).recreate_count != 1;
	int framebuffer_width = 0, framebuffer_height = 0, window_width = 0, window_height = 0;
	context->window_data.window_sizes_callback(&context->window_data, &framebuffer_width, &framebuffer_height, &window_width, &window_height);
	zest_extent2d_t extent = zest_GetSwapChainExtent(context);
	failed |= extent.width != (zest_uint)framebuffer_width || extent.height != (zest_uint)framebuffer_height;
	DXGI_SWAP_CHAIN_DESC1 description = {};
	failed |= FAILED(ZEST_DX_BACKEND(context, context)->swapchain->GetDesc1(&description));
	failed |= description.Width != extent.width || description.Height != extent.height;
	failed |= run_windowed_clear_frames(context, 5, ZEST_FALSE).recreate_count != 0;
	return failed;
}

//The recreated swapchain must keep the flags it was created with, as ResizeBuffers rejects a change of flags
static int toggle_test_vsync(zest_context context, zest_bool enable) {
	if (enable) {
		zest_EnableVSync(context);
	} else {
		zest_DisableVSync(context);
	}
	int failed = run_windowed_clear_frames(context, 5, ZEST_FALSE).recreate_count != 1;
	failed |= ZEST__FLAGGED(context->flags, zest_context_flag_vsync_enabled) != enable;
	zest_dx_context_backend_t *backend = ZEST_DX_BACKEND(context, context);
	DXGI_SWAP_CHAIN_DESC1 description = {};
	failed |= FAILED(backend->swapchain->GetDesc1(&description));
	failed |= description.Flags != backend->swapchain_flags;
	failed |= ZEST_DX_BACKEND(device, context->device)->support.tearing && !(backend->swapchain_flags & DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING);
	return failed;
}

//Clears an imported image in a command graph on the graphics slot, so it runs on whichever direct queue the slot holds
static zest_semaphore_status flush_offscreen_clear(zest_context context, zest_image image) {
	if (!zest_BeginCommandGraph(context, "Offscreen Clear", 0)) return zest_semaphore_status_error;
	zest_resource_node node = zest_ImportImageResource("Offscreen Target", image, 0);
	zest_SetResourceClearColor(node, 0.2f, 0.4f, 0.6f, 1.0f);
	zest_BeginRenderPass("Offscreen Pass");
	zest_ConnectOutput(node);
	zest_SetPassTask(zest_EmptyRenderPass, 0);
	zest_EndPass();
	return zest_FlushFrameGraphAndWait(zest_EndFrameGraph());
}

//Offscreen graphs run on the slot's queue 1 and swapchain batches on queue 0, so each submission waits for the slot's previous one
static int run_cross_queue_ordering(zest_context context) {
	zest_device device = context->device;
	zest_uint slot = context->graphics_queue_index;
	zest_execution_timeline ordering_timeline = &context->queue_ordering_timelines[slot];
	context->preferred_queue_index[slot] = 1;
	context->has_preferred_queue[slot] = ZEST_TRUE;
	zest_image_handle image_handle = create_clear_target(device);
	zest_image image = zest_GetImage(image_handle);
	ID3D12Fence *gate = NULL;
	int failed = image == NULL || FAILED(zest_GetD3D12Device(device)->CreateFence(0, D3D12_FENCE_FLAG_NONE, __uuidof(ID3D12Fence), (void **)&gate));
	HANDLE thread = NULL;
	if (!failed) {
		D3D12FrameTimes times;
		for (int frame = 0; frame != 3; ++frame) {
			failed |= flush_offscreen_clear(context, image) != zest_semaphore_status_success;
			failed |= !run_windowed_clear_frame(context, &times);
			failed |= zest_WaitForSignal(ordering_timeline, ZEST_SECONDS_IN_MICROSECONDS(1)) != zest_semaphore_status_success;
		}
		//Holds back the swapchain's queue, so the frame's swapchain batch only runs once the gate opens
		failed |= FAILED(zest_GetD3D12CommandQueue(context)->Wait(gate, 1));
		thread = open_gate_after_delay(gate);
		failed |= thread == NULL;
		failed |= !run_windowed_clear_frame(context, &times);
		failed |= flush_offscreen_clear(context, image) != zest_semaphore_status_success;
		//The gate only opens after the delay, so it's open here only if the offscreen graph waited for the swapchain batch
		failed |= gate->GetCompletedValue() != 1;
		failed |= zest_WaitForSignal(ordering_timeline, ZEST_SECONDS_IN_MICROSECONDS(1)) != zest_semaphore_status_success;
	}
	finish_gate(gate, thread);
	zest_FreeImageNow(image_handle);
	return failed;
}

//Flip model buffers can't be sRGB, so an sRGB swapchain renders through sRGB views of UNORM buffers
static int run_srgb_swapchain(zest_device device) {
	D3D12TestWindow window;
	if (!open_test_window(&window, "D3D12 sRGB Swapchain")) {
		close_test_window(&window);
		return 1;
	}
	zest_create_context_info_t info = zest_CreateContextInfo();
	info.color_format = zest_format_b8g8r8a8_srgb;
	zest_context context = zest_CreateContext(device, &window.window_data, &info);
	int failed = context == NULL;
	if (context) {
		set_test_clear_color(context);
		D3D12FrameRun run = run_windowed_clear_frames(context, 5, ZEST_FALSE);
		failed |= run.began_count != 5 || run.submitted_count != 5 || run.recreate_count != 0;
		failed |= resize_test_window(context, (SDL_Window *)window.window_data.window_handle, 400, 300);
		failed |= zest_GetSwapchainFormat(zest_GetSwapchain(context)) != zest_format_b8g8r8a8_srgb;
		zest_dx_swapchain_backend_t *swapchain_backend = ZEST_DX_BACKEND(swapchain, context->swapchain);
		failed |= swapchain_backend->buffer_format != DXGI_FORMAT_B8G8R8A8_UNORM || swapchain_backend->view_format != DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;
		DXGI_SWAP_CHAIN_DESC1 description = {};
		failed |= FAILED(ZEST_DX_BACKEND(context, context)->swapchain->GetDesc1(&description));
		failed |= description.Format != DXGI_FORMAT_B8G8R8A8_UNORM;
		failed |= zest_GetValidationErrorCount(device) != 0;
		zest_DestroyContext(context);
	}
	close_test_window(&window);
	return failed;
}

int test_d3d12_windowed_frames(void) {
	D3D12TestWindow window;
	if (!open_test_window(&window, "D3D12 Windowed Frames")) {
		close_test_window(&window);
		return 1;
	}
	SDL_Window *sdl_window = (SDL_Window *)window.window_data.window_handle;
	zest_device device = create_windowed_test_device(&window);
	if (!device) {
		close_test_window(&window);
		return 1;
	}
	zest_create_context_info_t info = zest_CreateContextInfo();
	zest_context context = zest_CreateContext(device, &window.window_data, &info);
	int failed = context == NULL;
	if (context) {
		set_test_clear_color(context);
		for (int frame = 0; frame != 10; ++frame) {
			D3D12FrameTimes times;
			failed |= !run_windowed_clear_frame(context, &times);
		}
		failed |= zest_GetD3D12CommandQueue(context) == NULL;
		failed |= zest_GetValidationErrorCount(device) != 0;

		failed |= resize_test_window(context, sdl_window, 400, 300);
		failed |= zest_GetValidationErrorCount(device) != 0;
		failed |= resize_test_window(context, sdl_window, 320, 200);
		failed |= zest_GetValidationErrorCount(device) != 0;

		failed |= toggle_test_vsync(context, ZEST_TRUE);
		failed |= zest_GetValidationErrorCount(device) != 0;
		failed |= toggle_test_vsync(context, ZEST_FALSE);
		failed |= zest_GetValidationErrorCount(device) != 0;

		//The size helper reports the last non-zero size while minimised, and Present reports occlusion as success
		SDL_MinimizeWindow(sdl_window);
		D3D12FrameRun minimised = run_windowed_clear_frames(context, 5, ZEST_FALSE);
		failed |= minimised.recreate_count != 0;
		//A minimised window can stop signalling the frame latency waitable, so a frame may wait out the timeout but must not hang
		failed |= minimised.slowest_frame > (zest_microsecs)(context->fence_wait_timeout_ns / 1000) + ZEST_SECONDS_IN_MICROSECONDS(1);
		SDL_RestoreWindow(sdl_window);
		D3D12FrameRun restored = run_windowed_clear_frames(context, 5, ZEST_FALSE);
		failed |= restored.began_count != 5 || restored.submitted_count != 5 || restored.recreate_count != 0;
		failed |= zest_GetValidationErrorCount(device) != 0;

		zest_uint slot = context->graphics_queue_index;
		zest_queue second_queue = &device->queue_families[context->graphics_family_index]->queues[1];
		failed |= ZEST_DX_BACKEND(queue, second_queue)->queue == zest_GetD3D12CommandQueue(context);
		failed |= run_cross_queue_ordering(context);
		failed |= zest_GetValidationErrorCount(device) != 0;

		//The slot acquires the queue asked for, and a batch that writes the back buffer still runs on the swapchain's queue, as D3D12 requires
		for (int frame = 0; frame != 6; ++frame) {
			zest_uint preferred_index = (zest_uint)(frame % 2);
			context->preferred_queue_index[slot] = preferred_index;
			context->has_preferred_queue[slot] = ZEST_TRUE;
			D3D12FrameTimes times;
			failed |= !run_windowed_clear_frame(context, &times);
			failed |= context->preferred_queue_index[slot] != preferred_index;
		}
		context->has_preferred_queue[slot] = ZEST_FALSE;
		failed |= zest_GetValidationErrorCount(device) != 0;

		//No-work frames take the dummy submit and must still advance the frame timeline, the first on queue 1 straight after a frame on queue 0
		for (int frame = 0; frame != 3; ++frame) {
			SDL_Event event;
			while (SDL_PollEvent(&event)) {}
			zest_UpdateDevice(device);
			zest_microsecs start = zest_Microsecs();
			zest_bool began = zest_BeginFrame(context);
			failed |= !began || zest_Microsecs() - start > ZEST_SECONDS_IN_MICROSECONDS(1);
			if (began) {
				zest_EndFrame(context, 0);
			}
		}
		//Signals that landed out of order across the two queues would leave the fence short of the slot's last value
		failed |= zest_WaitForSignal(&context->queue_ordering_timelines[slot], ZEST_SECONDS_IN_MICROSECONDS(1)) != zest_semaphore_status_success;
		failed |= zest_GetValidationErrorCount(device) != 0;
		zest_DestroyContext(context);
	}
	failed |= run_srgb_swapchain(device);
	failed |= finish_device(device);
	close_test_window(&window);
	return failed;
}

int test_d3d12_dxgi_messages_counted(void) {
	D3D12TestWindow window;
	if (!open_test_window(&window, "D3D12 DXGI Messages")) {
		close_test_window(&window);
		return 1;
	}
	zest_device device = create_windowed_test_device(&window);
	if (!device) {
		close_test_window(&window);
		return 1;
	}
	int failed = 0;
	zest_dx_device_backend_t *device_backend = ZEST_DX_BACKEND(device, device);
	if (!device_backend->support.tearing) {
		ZEST_PRINT("D3D12 DXGI Messages Counted skipped: this system doesn't support tearing, so a tearing present isn't an invalid call to provoke");
	} else if (!device_backend->dxgi_info_queue) {
		ZEST_PRINT("D3D12 DXGI Messages Counted skipped: the DXGI debug layer isn't available");
	} else {
		zest_create_context_info_t info = zest_CreateContextInfo();
		zest_context context = zest_CreateContext(device, &window.window_data, &info);
		failed |= context == NULL;
		if (context) {
			//Tearing with a sync interval is invalid, and its message is expected, so it goes to memory only
			ZEST__UNFLAG(device->init_flags, zest_device_init_flag_log_validation_errors_to_console);
			HRESULT result = ZEST_DX_BACKEND(context, context)->swapchain->Present(1, DXGI_PRESENT_ALLOW_TEARING);
			zest__dx_drain_dxgi_messages(device);
			ZEST__FLAG(device->init_flags, zest_device_init_flag_log_validation_errors_to_console);
			failed |= SUCCEEDED(result);
			failed |= zest_GetValidationErrorCount(device) < 1;
			zest_ResetValidationErrors(device);
			zest_DestroyContext(context);
		}
	}
	failed |= finish_device(device);
	close_test_window(&window);
	return failed;
}

int test_d3d12_per_mip_views(void) {
	zest_device device = zest_EndDeviceBuilder(BeginD3D12TestBuilder());
	if (!device) return 1;
	zest_image_info_t image_info = zest_CreateImageInfo(64, 64);
	image_info.format = zest_format_r8g8b8a8_unorm;
	image_info.flags = zest_image_preset_texture_mipmaps | zest_image_flag_storage;
	zest_image_handle image_handle = zest_CreateImage(device, &image_info);
	zest_image image = zest_GetImage(image_handle);
	int failed = image == NULL;
	if (image) {
		zest_dx_cpu_descriptor_heap_t *resource_heap = &ZEST_DX_BACKEND(device, device)->cpu_resource_heap;
		zest_image_view_array_handle view_array_handle = zest_CreateImageViewsPerMip(device, image);
		zest_image_view_array view_array = view_array_handle.value ? zest_GetImageViewArray(view_array_handle) : NULL;
		failed |= view_array == NULL;
		if (view_array) {
			failed |= view_array->count != 7;
			for (zest_uint mip_index = 0; mip_index != view_array->count; ++mip_index) {
				zest_dx_image_view_backend_t *view_backend = ZEST_DX_BACKEND(image_view, &view_array->views[mip_index]);
				failed |= !view_backend->srv.ptr || !view_backend->uav.ptr;
			}
			zest_uint *storage_indexes = zest_AcquireImageMipIndexes(device, image, view_array, zest_storage_image_binding, zest_descriptor_type_storage_image);
			zest_uint *texture_indexes = zest_AcquireImageMipIndexes(device, image, view_array, zest_texture_2d_binding, zest_descriptor_type_sampled_image);
			failed |= zest_vec_size(storage_indexes) != 7 || zest_vec_size(texture_indexes) != 7;
			zest_vec_foreach(index, storage_indexes) {
				failed |= storage_indexes[index] == ZEST_INVALID;
			}
			zest_vec_foreach(index, texture_indexes) {
				failed |= texture_indexes[index] == ZEST_INVALID;
			}
			failed |= zest_GetValidationErrorCount(device) != 0;
			zest_uint free_handles_before = zest_vec_size(resource_heap->free_handles);
			zest_FreeImageViewArrayNow(view_array_handle);
			//Seven SRVs and seven UAVs
			failed |= zest_vec_size(resource_heap->free_handles) != free_handles_before + 14;
		}
		failed |= zest_AcquireStorageImageIndex(device, image, zest_storage_image_binding) == ZEST_INVALID;
	}
	zest_FreeImageNow(image_handle);
	return failed | finish_device(device);
}

static const char *orientation_vertex_source =
	"float4 main(uint vertex_id : SV_VertexID) : SV_Position {\n"
	"	float2 corner = float2((vertex_id << 1) & 2, vertex_id & 2);\n"
	"	return float4(corner - 1.0, 0.0, 1.0);\n"
	"}\n";
static const char *orientation_pixel_source =
	"float4 main() : SV_Target {\n"
	"	return float4(1.0, 0.0, 0.0, 1.0);\n"
	"}\n";

//Draws the winding test's triangle on an immediate graphics list and reads the target back
static int draw_orientation_triangle(zest_device device, zest_image image, ID3DBlob *vertex_dxil, ID3DBlob *pixel_dxil, D3D12_CULL_MODE cull_mode, BOOL front_counter_clockwise, const char *name, WindingCoverage *coverage) {
	ID3D12PipelineState *pipeline = zest__dx_create_utility_pipeline(device, vertex_dxil, pixel_dxil, DXGI_FORMAT_R8G8B8A8_UNORM, cull_mode, front_counter_clockwise);
	if (!pipeline) return 1;
	zest_queue queue = zest_imm_BeginCommandBuffer(device, zest_queue_graphics);
	if (!queue) {
		ZEST_DX_RELEASE(pipeline);
		return 1;
	}
	ID3D12GraphicsCommandList7 *command_list = ZEST_DX_BACKEND(queue, queue)->command_list;
	int failed = !zest_imm_TransitionImage(queue, image, zest_resource_state_render_target, 0, 1, 0, 1);
	D3D12_CPU_DESCRIPTOR_HANDLE render_target_view = ZEST_DX_BACKEND(image_view, image->default_view)->rtv;
	const FLOAT clear_colour[4] = { 0.f, 0.f, 0.f, 1.f };
	D3D12_VIEWPORT viewport = zest__dx_viewport(0.f, 0.f, 16.f, 16.f, 0.f, 1.f);
	D3D12_RECT scissor = { 0, 0, 16, 16 };
	ZEST_DX_CALL(command_list, ClearRenderTargetView, render_target_view, clear_colour, 0, NULL);
	ZEST_DX_CALL(command_list, OMSetRenderTargets, 1, &render_target_view, FALSE, NULL);
	ZEST_DX_CALL(command_list, RSSetViewports, 1, &viewport);
	ZEST_DX_CALL(command_list, RSSetScissorRects, 1, &scissor);
	ZEST_DX_CALL(command_list, SetGraphicsRootSignature, zest__dx_utility_root_signature(device));
	ZEST_DX_CALL(command_list, SetPipelineState, pipeline);
	ZEST_DX_CALL(command_list, IASetPrimitiveTopology, D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	ZEST_DX_CALL(command_list, DrawInstanced, 3, 1, 0, 0);
	failed |= !zest_imm_EndCommandBuffer(queue);
	ZEST_DX_RELEASE(pipeline);
	zest_uint pixels[16 * 16];
	memset(pixels, 0, sizeof(pixels));
	failed |= !zest_CopyImageToBitmap(device, image, pixels);
	*coverage = winding_coverage(pixels);
	ZEST_PRINT("\t%s: %u red pixels, %u of them inside x + y < 15, %u neither red nor black", name, coverage->red_count, coverage->red_inside_count, coverage->other_count);
	return failed;
}

//The control has no culling, so it checks the pipeline and the viewport flip apart from facing
int test_d3d12_viewport_orientation(void) {
	zest_device device = zest_EndDeviceBuilder(BeginD3D12TestBuilder());
	if (!device) return 1;
	ID3DBlob *vertex_dxil = NULL;
	ID3DBlob *pixel_dxil = NULL;
	int failed = !zest__dx_compile_hlsl(device, orientation_vertex_source, (zest_size)strlen(orientation_vertex_source), "orientation_vertex", "main", "vs_6_0", &vertex_dxil);
	failed |= !zest__dx_compile_hlsl(device, orientation_pixel_source, (zest_size)strlen(orientation_pixel_source), "orientation_pixel", "main", "ps_6_0", &pixel_dxil);
	zest_image_handle image_handle = create_clear_target(device);
	zest_image image = zest_GetImage(image_handle);
	failed |= image == NULL;
	if (!failed) {
		WindingCoverage control = {};
		WindingCoverage front_clockwise = {};
		WindingCoverage front_counter_clockwise = {};
		failed |= draw_orientation_triangle(device, image, vertex_dxil, pixel_dxil, D3D12_CULL_MODE_NONE, FALSE, "Orientation control, no culling", &control);
		failed |= draw_orientation_triangle(device, image, vertex_dxil, pixel_dxil, D3D12_CULL_MODE_BACK, FALSE, "Orientation cull back, FrontCounterClockwise FALSE", &front_clockwise);
		failed |= draw_orientation_triangle(device, image, vertex_dxil, pixel_dxil, D3D12_CULL_MODE_BACK, TRUE, "Orientation cull back, FrontCounterClockwise TRUE", &front_counter_clockwise);
		BOOL clockwise_setting = zest__dx_front_counter_clockwise(zest_front_face_clockwise);
		WindingCoverage matching = clockwise_setting ? front_counter_clockwise : front_clockwise;
		WindingCoverage other = clockwise_setting ? front_clockwise : front_counter_clockwise;
		failed |= !winding_covers_triangle(control);
		failed |= !winding_covers_triangle(matching) || !winding_covers_nothing(other);
	}
	zest_FreeImageNow(image_handle);
	ZEST_DX_RELEASE(vertex_dxil);
	ZEST_DX_RELEASE(pixel_dxil);
	//The root signature belongs to the old ID3D12Device, so a reset releases it first and the next use recreates it
	zest_ResetDevice(device);
	failed |= zest__dx_last_device_release_count() != 0;
	failed |= ZEST_DX_BACKEND(device, device)->utility_root_signature != NULL;
	failed |= zest__dx_utility_root_signature(device) == NULL;
	return failed | finish_device(device);
}

static void remove_test_device(zest_device device) {
	ID3D12Device5 *device5 = NULL;
	if (SUCCEEDED(ZEST_DX_CALL(zest_GetD3D12Device(device), QueryInterface, ZEST_DX_IID(ID3D12Device5), (void **)&device5))) {
		device5->RemoveDevice();
		device5->Release();
	}
}

//Between frames the next frame's fence wait is the first to see the removal, and inside one it's the Present, whose recreate must keep the swapchain
static int run_device_removal(D3D12TestWindow *window, zest_bool inside_frame) {
	zest_device device = create_windowed_test_device(window);
	if (!device) return 1;
	zest_create_context_info_t info = zest_CreateContextInfo();
	zest_context context = zest_CreateContext(device, &window->window_data, &info);
	int failed = context == NULL;
	if (context) {
		set_test_clear_color(context);
		for (int frame = 0; frame != 3; ++frame) {
			D3D12FrameTimes times;
			failed |= !run_windowed_clear_frame(context, &times);
		}
		failed |= zest_GetValidationErrorCount(device) != 0;
		//What the debug layer reports about the removal is expected, so it goes to memory only
		ZEST__UNFLAG(device->init_flags, zest_device_init_flag_log_validation_errors_to_console);
		zest_microsecs frame_limit = 2 * (zest_microsecs)(context->fence_wait_timeout_ns / 1000);
		if (inside_frame) {
			zest_UpdateDevice(device);
			zest_frame_graph_cache_key_t cache_key = zest_InitialiseCacheKey(context, 0, 0);
			failed |= !zest_BeginFrame(context);
			zest_frame_graph frame_graph = zest_GetCachedFrameGraph(context, &cache_key);
			failed |= frame_graph == NULL;
			remove_test_device(device);
			zest_microsecs start = zest_Microsecs();
			zest_EndFrame(context, frame_graph);
			failed |= zest_Microsecs() - start > frame_limit;
			failed |= context->swapchain == NULL;
		} else {
			remove_test_device(device);
		}
		for (int frame = 0; frame != 3; ++frame) {
			D3D12FrameTimes times;
			run_windowed_clear_frame(context, &times);
			failed |= times.begin > frame_limit || times.end > frame_limit;
		}
		failed |= ZEST__NOT_FLAGGED(context->flags, zest_context_flag_device_lost);
		zest_ResetValidationErrors(device);
		zest_DestroyContext(context);
	}
	zest_ResetValidationErrors(device);
	zest_DestroyDevice(device);
	return failed | (zest__dx_last_device_release_count() != 0);
}

//Must stay the last D3D12 test, as a reference leaked from a removed device would hand that device to every test after it
int test_d3d12_device_removed_mid_frame(void) {
	D3D12TestWindow window;
	if (!open_test_window(&window, "D3D12 Device Removed")) {
		close_test_window(&window);
		return 1;
	}
	int failed = run_device_removal(&window, ZEST_FALSE);
	failed |= run_device_removal(&window, ZEST_TRUE);
	close_test_window(&window);
	return failed;
}

static void fill_words(zest_buffer buffer, zest_uint seed) {
	zest_uint *words = (zest_uint *)zest_BufferData(buffer);
	for (zest_uint index = 0; index != buffer->size / 4; ++index) words[index] = seed + index * 2654435761u;
}

static int words_equal(zest_buffer buffer, zest_uint value) {
	const zest_uint *words = (const zest_uint *)zest_BufferData(buffer);
	for (zest_uint index = 0; index != buffer->size / 4; ++index) {
		if (words[index] != value) return 0;
	}
	return 1;
}

static int run_buffer_copy_on_queue(zest_device device, zest_device_queue_type queue_type, zest_buffer source, zest_buffer middle, zest_buffer destination) {
	memset(zest_BufferData(destination), 0, destination->size);
	zest_queue queue = zest_imm_BeginCommandBuffer(device, queue_type);
	if (!queue) return 1;
	int failed = !zest_imm_CopyBuffer(queue, source, middle, 4096);
	failed |= !zest_imm_CopyBufferRegion(queue, middle, 256, destination, 512, 1024);
	failed |= !zest_imm_EndCommandBuffer(queue);
	failed |= memcmp((const char *)zest_BufferData(destination) + 512, (const char *)zest_BufferData(source) + 256, 1024) != 0;
	return failed;
}

//The update writes its 10 bytes padded with zeros to 12, over a fill of 0xA5
static int holds_fill_then_update(zest_buffer destination, const unsigned char update_data[10]) {
	const unsigned char *bytes = (const unsigned char *)zest_BufferData(destination);
	int failed = memcmp(bytes, update_data, 10) != 0 || bytes[10] != 0 || bytes[11] != 0;
	for (zest_uint index = 12; index != destination->size; ++index) failed |= bytes[index] != 0xA5;
	return !failed;
}

//Both are copy writes to the same bytes, so the update only lands on top when the barrier between them orders writes
static int run_fill_then_update_on_queue(zest_device device, zest_device_queue_type queue_type, zest_buffer destination, const unsigned char update_data[10]) {
	memset(zest_BufferData(destination), 0xCD, destination->size);
	zest_queue queue = zest_imm_BeginCommandBuffer(device, queue_type);
	if (!queue) return 1;
	zest_imm_FillBuffer(queue, destination, 0xA5A5A5A5u);
	zest_imm_UpdateBuffer(queue, destination, (void *)update_data, 10);
	int failed = !zest_imm_EndCommandBuffer(queue);
	return failed | !holds_fill_then_update(destination, update_data);
}

int test_d3d12_buffer_copies(void) {
	zest_device device = zest_EndDeviceBuilder(BeginD3D12TestBuilder());
	if (!device) return 1;
	zest_buffer_info_t upload_info = zest_CreateBufferInfo(zest_buffer_type_staging, zest_memory_usage_cpu_to_gpu);
	zest_buffer_info_t readback_info = zest_CreateBufferInfo(zest_buffer_type_staging, zest_memory_usage_gpu_to_cpu);
	zest_buffer_info_t device_info = zest_CreateBufferInfo(zest_buffer_type_staging, zest_memory_usage_gpu_only);
	zest_buffer source = zest_CreateBuffer(device, 4096, &upload_info);
	zest_buffer middle = zest_CreateBuffer(device, 4096, &device_info);
	zest_buffer destination = zest_CreateBuffer(device, 4096, &readback_info);
	zest_buffer large = zest_CreateBuffer(device, 200 * 1024, &readback_info);
	int failed = !source || !middle || !destination || !large;
	if (!failed) {
		fill_words(source, 0x1234u);
		failed |= run_buffer_copy_on_queue(device, zest_queue_graphics, source, middle, destination);
		failed |= run_buffer_copy_on_queue(device, zest_queue_compute, source, middle, destination);
		failed |= run_buffer_copy_on_queue(device, zest_queue_transfer, source, middle, destination);

		memset(zest_BufferData(large), 0, large->size);
		zest_queue queue = zest_imm_BeginCommandBuffer(device, zest_queue_transfer);
		failed |= queue == NULL;
		zest_imm_FillBuffer(queue, destination, 0xA5A5A5A5u);
		zest_imm_FillBuffer(queue, large, 0xA5A5A5A5u);
		failed |= !zest_imm_EndCommandBuffer(queue);
		failed |= !words_equal(destination, 0xA5A5A5A5u) || !words_equal(large, 0xA5A5A5A5u);

		const unsigned char update_data[10] = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 };
		queue = zest_imm_BeginCommandBuffer(device, zest_queue_transfer);
		failed |= queue == NULL;
		zest_imm_UpdateBuffer(queue, destination, (void *)update_data, 10);
		failed |= !zest_imm_EndCommandBuffer(queue);
		failed |= !holds_fill_then_update(destination, update_data);

		failed |= run_fill_then_update_on_queue(device, zest_queue_graphics, destination, update_data);
		failed |= run_fill_then_update_on_queue(device, zest_queue_compute, destination, update_data);
		failed |= run_fill_then_update_on_queue(device, zest_queue_transfer, destination, update_data);
	}
	zest_FreeBufferNow(source);
	zest_FreeBufferNow(middle);
	zest_FreeBufferNow(destination);
	zest_FreeBufferNow(large);

	zest_image_info_t image_info = zest_CreateImageInfo(1, 1);
	image_info.format = zest_format_r8g8b8a8_unorm;
	image_info.flags = zest_image_preset_texture | zest_image_flag_transfer_src;
	zest_image_handle image_handle = zest_CreateImage(device, &image_info);
	zest_image image = zest_GetImage(image_handle);
	const zest_uint pixel = 0x80402010u;
	zest_buffer pixel_buffer = zest_CreateStagingBuffer(device, 4, (void *)&pixel);
	failed |= image == NULL || pixel_buffer == NULL;
	if (!failed) {
		zest_queue queue = zest_imm_BeginCommandBuffer(device, zest_queue_graphics);
		failed |= !zest_imm_TransitionImage(queue, image, zest_resource_state_copy_dst, 0, 1, 0, 1);
		failed |= !zest_imm_CopyBufferToImage(queue, pixel_buffer, image, 4);
		failed |= !zest_imm_TransitionImage(queue, image, zest_resource_state_shader_read, 0, 1, 0, 1);
		failed |= !zest_imm_EndCommandBuffer(queue);
		zest_uint read_back = 0;
		failed |= read_image_layers(device, image, 1, 4, &read_back);
		failed |= read_back != pixel;
	}
	zest_FreeBufferNow(pixel_buffer);
	zest_FreeImageNow(image_handle);
	return failed | finish_device(device);
}

int test_d3d12_buffer_descriptors(void) {
	zest_device device = zest_EndDeviceBuilder(BeginD3D12TestBuilder());
	if (!device) return 1;
	zest_buffer_info_t storage_info = zest_CreateBufferInfo(zest_buffer_type_storage, zest_memory_usage_gpu_only);
	zest_buffer_info_t uniform_info = zest_CreateBufferInfo(zest_buffer_type_uniform, zest_memory_usage_cpu_to_gpu);
	zest_buffer_info_t stride_info = storage_info;
	stride_info.element_alignment = 48;
	zest_buffer storage_buffer = zest_CreateBuffer(device, 4096, &storage_info);
	zest_buffer uniform_buffer = zest_CreateBuffer(device, 256, &uniform_info);
	zest_buffer stride_buffer = zest_CreateBuffer(device, 48 * 37, &stride_info);
	int failed = !storage_buffer || !uniform_buffer || !stride_buffer;
	if (!failed) {
		zest_uint storage_index = zest_AcquireStorageBufferIndex(device, storage_buffer);
		zest_uint uniform_index = zest_AcquireUniformBufferIndex(device, uniform_buffer);
		zest_uint stride_index = zest_AcquireStorageBufferIndex(device, stride_buffer);
		failed |= storage_index == ZEST_INVALID || uniform_index == ZEST_INVALID || stride_index == ZEST_INVALID;
		failed |= zest_GetValidationErrorCount(device) != 0;
		zest_ReleaseStorageBufferIndex(device, storage_index);
		zest_ReleaseBindlessIndex(device, uniform_index, zest_uniform_buffer_binding);
		zest_ReleaseStorageBufferIndex(device, stride_index);
	}
	zest_FreeBufferNow(storage_buffer);
	zest_FreeBufferNow(uniform_buffer);
	zest_FreeBufferNow(stride_buffer);

	//A uniform usage buffer can't be created this large, so a storage buffer stands in for one
	zest_buffer_info_t oversized_info = zest_CreateBufferInfo(zest_buffer_type_storage, zest_memory_usage_cpu_to_gpu);
	zest_buffer oversized_buffer = zest_CreateBuffer(device, 65537, &oversized_info);
	failed |= oversized_buffer == NULL;
	if (oversized_buffer) {
		zest_uint errors_before = zest_GetValidationErrorCount(device);
		zest_uint oversized_index = zest_AcquireUniformBufferIndex(device, oversized_buffer);
		failed |= zest_GetValidationErrorCount(device) != errors_before + 1;
		zest_ReleaseBindlessIndex(device, oversized_index, zest_uniform_buffer_binding);
		zest_FreeBufferNow(oversized_buffer);
		zest_ResetValidationErrors(device);
	}
	return failed | finish_device(device);
}

typedef struct d3d12_buffer_copy_task_t {
	zest_buffer_uploader_t uploader;
	zest_buffer upload_source;
	zest_buffer upload_target;
	zest_buffer copy_source;
	zest_buffer copy_target;
} d3d12_buffer_copy_task_t;

static void d3d12_buffer_copy_pass(const zest_command_list command_list, void *user_data) {
	d3d12_buffer_copy_task_t *task = (d3d12_buffer_copy_task_t *)user_data;
	zest_cmd_CopyBuffer(command_list, task->copy_source, task->copy_target, 512);
	zest_cmd_UploadBuffer(command_list, &task->uploader);
}

int test_d3d12_frame_graph_buffer_copies(void) {
	zest_device device = zest_EndDeviceBuilder(BeginD3D12TestBuilder());
	if (!device) return 1;
	zest_create_context_info_t info = zest_CreateContextInfo();
	zest_context context = zest_CreateHeadlessContext(device, &info);
	int failed = context == NULL;
	zest_buffer_info_t upload_info = zest_CreateBufferInfo(zest_buffer_type_staging, zest_memory_usage_cpu_to_gpu);
	zest_buffer_info_t readback_info = zest_CreateBufferInfo(zest_buffer_type_staging, zest_memory_usage_gpu_to_cpu);
	d3d12_buffer_copy_task_t task = {};
	task.upload_source = zest_CreateBuffer(device, 1024, &upload_info);
	task.copy_source = zest_CreateBuffer(device, 1024, &upload_info);
	task.upload_target = zest_CreateBuffer(device, 1024, &readback_info);
	task.copy_target = zest_CreateBuffer(device, 1024, &readback_info);
	failed |= !task.upload_source || !task.copy_source || !task.upload_target || !task.copy_target;
	if (!failed) {
		fill_words(task.upload_source, 0x1111u);
		fill_words(task.copy_source, 0x2222u);
		memset(zest_BufferData(task.upload_target), 0, task.upload_target->size);
		memset(zest_BufferData(task.copy_target), 0, task.copy_target->size);
		failed |= !zest_BeginCommandGraph(context, "Buffer Copies", 0);
		//The pass declares no resources, so nothing is a graph resource and no buffer barrier is emitted
		zest_BeginTransferPass("Copy Pass");
		zest_DoNotCull();
		zest_SetPassTask(d3d12_buffer_copy_pass, &task);
		zest_EndPass();
		zest_AddCopyCommand(context, &task.uploader, task.upload_source, task.upload_target, 256);
		zest_frame_graph frame_graph = zest_EndFrameGraph();
		failed |= frame_graph == NULL || zest_FlushFrameGraphAndWait(frame_graph) != zest_semaphore_status_success;
		failed |= memcmp(zest_BufferData(task.upload_target), zest_BufferData(task.upload_source), 256) != 0;
		failed |= memcmp(zest_BufferData(task.copy_target), zest_BufferData(task.copy_source), 512) != 0;
		failed |= zest_vec_size(task.uploader.buffer_copies) != 0 || task.uploader.flags != 0 || task.uploader.source_buffer != NULL || task.uploader.target_buffer != NULL;
	}
	zest_FreeBufferNow(task.upload_source);
	zest_FreeBufferNow(task.copy_source);
	zest_FreeBufferNow(task.upload_target);
	zest_FreeBufferNow(task.copy_target);
	if (context) zest_DestroyContext(context);
	return failed | finish_device(device);
}

int test_d3d12_transient_descriptor_chunks(void) {
	zest_device device = zest_EndDeviceBuilder(BeginD3D12TestBuilder());
	if (!device) return 1;
	zest_dx_descriptor_pool_backend_t *pool = ZEST_DX_BACKEND(descriptor_set, device->bindless_set)->pool;
	zest_uint free_chunks_before = zest_vec_size(pool->free_transient_chunks);
	SIZE_T region_start = pool->resource_cpu_start.ptr + (SIZE_T)pool->regions.transient_offset * pool->resource_increment;
	SIZE_T region_end = region_start + (SIZE_T)pool->regions.transient_count * pool->resource_increment;
	zest_dx_transient_descriptors_t descriptors = {};
	D3D12_CPU_DESCRIPTOR_HANDLE first_cpu = {};
	D3D12_GPU_DESCRIPTOR_HANDLE first_gpu = {};
	D3D12_CPU_DESCRIPTOR_HANDLE previous_cpu = {};
	D3D12_GPU_DESCRIPTOR_HANDLE previous_gpu = {};
	D3D12_CPU_DESCRIPTOR_HANDLE cpu = {};
	D3D12_GPU_DESCRIPTOR_HANDLE gpu = {};
	int failed = pool->regions.transient_count != ZEST_DX_TRANSIENT_DESCRIPTOR_COUNT;
	for (zest_uint index = 0; index != 193; ++index) {
		failed |= !zest__dx_allocate_transient_descriptor(device, &descriptors, &cpu, &gpu);
		failed |= cpu.ptr < region_start || cpu.ptr >= region_end;
		if (index == 0) {
			first_cpu = cpu;
			first_gpu = gpu;
		} else if (index % ZEST_DX_TRANSIENT_DESCRIPTOR_CHUNK != 0) {
			failed |= cpu.ptr != previous_cpu.ptr + pool->resource_increment || gpu.ptr != previous_gpu.ptr + pool->resource_increment;
		}
		previous_cpu = cpu;
		previous_gpu = gpu;
	}
	failed |= zest_vec_size(descriptors.chunks) != 4;
	failed |= zest_vec_size(pool->free_transient_chunks) != free_chunks_before - 4;

	zest__dx_reset_transient_descriptors(&descriptors);
	failed |= !zest__dx_allocate_transient_descriptor(device, &descriptors, &cpu, &gpu);
	failed |= cpu.ptr != first_cpu.ptr || gpu.ptr != first_gpu.ptr;
	failed |= zest_vec_size(descriptors.chunks) != 4;

	zest__dx_release_transient_descriptors(device, &descriptors);
	failed |= zest_vec_size(pool->free_transient_chunks) != free_chunks_before;
	failed |= zest_vec_size(descriptors.chunks) != 0;

	zest_uint errors_before = zest_GetValidationErrorCount(device);
	zest_uint allocated = 0;
	while (allocated <= ZEST_DX_TRANSIENT_DESCRIPTOR_COUNT && zest__dx_allocate_transient_descriptor(device, &descriptors, &cpu, &gpu)) {
		allocated++;
	}
	failed |= allocated != free_chunks_before * ZEST_DX_TRANSIENT_DESCRIPTOR_CHUNK;
	failed |= zest_GetValidationErrorCount(device) != errors_before + 1;
	zest_ResetValidationErrors(device);
	//Later tests on this pool need its free list whole
	zest__dx_release_transient_descriptors(device, &descriptors);
	failed |= zest_vec_size(pool->free_transient_chunks) != free_chunks_before;
	return failed | finish_device(device);
}

static const int mip_red[4] = { 255, 0, 0, 255 };
static const int mip_blue[4] = { 0, 0, 255, 255 };
static const int mip_purple[4] = { 128, 0, 128, 255 };

static int pixels_within(const zest_uint *pixels, zest_uint pixel_count, const int expected[4], int tolerance) {
	for (zest_uint index = 0; index != pixel_count; ++index) {
		const unsigned char *channels = (const unsigned char *)&pixels[index];
		for (int channel = 0; channel != 4; ++channel) {
			if (abs((int)channels[channel] - expected[channel]) > tolerance) return 0;
		}
	}
	return 1;
}

//Mip 1 keeps the red and blue halves exactly, and the last mip is their average
static int check_split_mips(zest_device device, zest_image image) {
	zest_uint mip_one[8 * 8];
	zest_uint last_mip = 0;
	memset(mip_one, 0, sizeof(mip_one));
	int failed = !zest__dx_read_subresource(device, image, 1, 0, mip_one);
	failed |= !zest__dx_read_subresource(device, image, 4, 0, &last_mip);
	for (zest_uint row = 0; row != 8; ++row) {
		failed |= !pixels_within(mip_one + row * 8, 4, mip_red, 0);
		failed |= !pixels_within(mip_one + row * 8 + 4, 4, mip_blue, 0);
	}
	failed |= !pixels_within(&last_mip, 1, mip_purple, 1);
	failed |= zest_ImageRawLayout(image) != zest_image_layout_shader_read_only_optimal;
	failed |= ZEST_DX_BACKEND(image, image)->layout != D3D12_BARRIER_LAYOUT_SHADER_RESOURCE;
	return failed;
}

//A 16x16 image with a red left half and a blue right half, uploaded with its mips generated
static zest_image_handle upload_split_mip_image(zest_device device, int *failed) {
	zest_uint pixels[16 * 16];
	for (zest_uint index = 0; index != 16 * 16; ++index) pixels[index] = (index % 16) < 8 ? 0xFF0000FFu : 0xFFFF0000u;
	zest_image_info_t image_info = zest_CreateImageInfo(16, 16);
	image_info.format = zest_format_r8g8b8a8_unorm;
	image_info.flags = zest_image_preset_texture_mipmaps;
	zest_image_handle image_handle = zest_CreateImage(device, &image_info);
	if (!image_handle.value) {
		*failed = 1;
		return image_handle;
	}
	zest_image image = zest_GetImage(image_handle);
	*failed |= image->info.mip_levels != 5;
	*failed |= !zest_CopyBitmapToImage(device, pixels, sizeof(pixels), image, 16, 16);
	*failed |= check_split_mips(device, image);
	return image_handle;
}

//Linear 0.5, which is the average of black and white, encodes to 188 in sRGB
static int check_srgb_mips(zest_device device) {
	zest_uint pixels[2 * 2] = { 0xFF000000u, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFF000000u };
	zest_image_info_t image_info = zest_CreateImageInfo(2, 2);
	image_info.format = zest_format_r8g8b8a8_srgb;
	image_info.flags = zest_image_preset_texture_mipmaps;
	zest_image_handle image_handle = zest_CreateImage(device, &image_info);
	if (!image_handle.value) return 1;
	zest_image image = zest_GetImage(image_handle);
	int failed = image->info.mip_levels != 2;
	failed |= !zest_CopyBitmapToImage(device, pixels, sizeof(pixels), image, 2, 2);
	zest_uint last_mip = 0;
	failed |= !zest__dx_read_subresource(device, image, 1, 0, &last_mip);
	const int expected[4] = { 188, 188, 188, 255 };
	failed |= !pixels_within(&last_mip, 1, expected, 2);
	zest_FreeImageNow(image_handle);
	return failed;
}

static int check_cube_mips(zest_device device) {
	static const zest_uint face_colours[6] = { 0xFF0000FFu, 0xFF00FF00u, 0xFFFF0000u, 0xFF00FFFFu, 0xFFFFFF00u, 0xFFFF00FFu };
	zest_uint pixels[6 * 8 * 8];
	for (zest_uint index = 0; index != 6 * 8 * 8; ++index) pixels[index] = face_colours[index / 64];
	zest_image_info_t image_info = zest_CreateImageInfo(8, 8);
	image_info.format = zest_format_r8g8b8a8_unorm;
	image_info.layer_count = 6;
	image_info.flags = zest_image_preset_texture_mipmaps | zest_image_flag_cubemap;
	zest_image_handle image_handle = zest_CreateImage(device, &image_info);
	if (!image_handle.value) return 1;
	zest_image image = zest_GetImage(image_handle);
	int failed = image->info.mip_levels != 4;
	zest_buffer staging_buffer = zest_CreateStagingBuffer(device, sizeof(pixels), pixels);
	zest_buffer_image_copy_t region = {};
	region.image_aspect = zest_image_aspect_color_bit;
	region.layer_count = 6;
	region.image_extent.width = 8;
	region.image_extent.height = 8;
	region.image_extent.depth = 1;
	zest_queue queue = zest_imm_BeginCommandBuffer(device, zest_queue_graphics);
	failed |= !zest_imm_TransitionImage(queue, image, zest_resource_state_copy_dst, 0, image->info.mip_levels, 0, 6);
	failed |= !zest_imm_CopyBufferRegionsToImage(queue, &region, 1, staging_buffer, image);
	failed |= !zest_imm_GenerateMipMaps(queue, image);
	failed |= !zest_imm_EndCommandBuffer(queue);
	zest_FreeBufferNow(staging_buffer);
	for (zest_uint face = 0; face != 6; ++face) {
		zest_uint last_mip = 0;
		failed |= !zest__dx_read_subresource(device, image, 3, face, &last_mip);
		failed |= last_mip != face_colours[face];
	}
	failed |= zest_ImageRawLayout(image) != zest_image_layout_shader_read_only_optimal;
	zest_FreeImageNow(image_handle);
	return failed;
}

int test_d3d12_generate_mipmaps(void) {
	zest_device device = zest_EndDeviceBuilder(BeginD3D12TestBuilder());
	if (!device) return 1;
	int failed = 0;
	zest_image_handle image_handle = upload_split_mip_image(device, &failed);
	failed |= check_srgb_mips(device);
	failed |= check_cube_mips(device);
	if (image_handle.value) {
		zest_image image = zest_GetImage(image_handle);
		zest_queue queue = zest_imm_BeginCommandBuffer(device, zest_queue_graphics);
		failed |= !zest_imm_GenerateMipMaps(queue, image);
		failed |= !zest_imm_EndCommandBuffer(queue);
		failed |= check_split_mips(device, image);
		zest_FreeImageNow(image_handle);
	}
	return failed | finish_device(device);
}

int test_d3d12_reset_after_blit(void) {
	zest_device device = zest_EndDeviceBuilder(BeginD3D12TestBuilder());
	if (!device) return 1;
	int failed = 0;
	zest_image_handle image_handle = upload_split_mip_image(device, &failed);
	if (image_handle.value) zest_FreeImageNow(image_handle);
	zest_ResetDevice(device);
	failed |= zest__dx_last_device_release_count() != 0;
	failed |= zest_GetValidationErrorCount(device) != 0;
	zest_dx_descriptor_pool_backend_t *pool = ZEST_DX_BACKEND(descriptor_set, device->bindless_set)->pool;
	failed |= zest_vec_size(pool->free_transient_chunks) != ZEST_DX_TRANSIENT_DESCRIPTOR_COUNT / ZEST_DX_TRANSIENT_DESCRIPTOR_CHUNK;
	zest_queue queue = zest_imm_BeginCommandBuffer(device, zest_queue_graphics);
	failed |= queue == NULL || zest_vec_size(ZEST_DX_BACKEND(queue, queue)->transient_descriptors.chunks) != 0;
	failed |= !zest_imm_EndCommandBuffer(queue);
	image_handle = upload_split_mip_image(device, &failed);
	if (image_handle.value) zest_FreeImageNow(image_handle);
	return failed | finish_device(device);
}

//Top left, top right, bottom left and bottom right
static const zest_uint blit_quadrant_colours[4] = { 0xFF0000FFu, 0xFF00FF00u, 0xFFFF0000u, 0xFFFFFFFFu };

static int quadrants_equal(const zest_uint *pixels, zest_uint size, const zest_uint colours[4]) {
	zest_uint half = size / 2;
	for (zest_uint index = 0; index != size * size; ++index) {
		zest_uint quadrant = ((index / size) >= half ? 2 : 0) + ((index % size) >= half ? 1 : 0);
		if (pixels[index] != colours[quadrant]) return 0;
	}
	return 1;
}

static int blit_and_check(zest_device device, zest_image source, zest_image destination, const int source_rect[4], const int destination_rect[4], zest_filter_type filter, const zest_uint expected[4]) {
	zest_queue queue = zest_imm_BeginCommandBuffer(device, zest_queue_graphics);
	if (!queue) return 1;
	int failed = !zest_imm_BlitImage(queue, source, destination, source_rect[0], source_rect[1], source_rect[2], source_rect[3], destination_rect[0], destination_rect[1], destination_rect[2], destination_rect[3], filter);
	failed |= !zest_imm_EndCommandBuffer(queue);
	zest_uint pixels[8 * 8];
	memset(pixels, 0, sizeof(pixels));
	failed |= !zest_CopyImageToBitmap(device, destination, pixels);
	failed |= !quadrants_equal(pixels, 8, expected);
	//Both images go back to the layouts they're tracked in
	failed |= ZEST_DX_BACKEND(image, source)->layout != D3D12_BARRIER_LAYOUT_SHADER_RESOURCE || zest_ImageRawLayout(source) != zest_image_layout_shader_read_only_optimal;
	failed |= ZEST_DX_BACKEND(image, destination)->layout != D3D12_BARRIER_LAYOUT_SHADER_RESOURCE || zest_ImageRawLayout(destination) != zest_image_layout_shader_read_only_optimal;
	return failed;
}

int test_d3d12_blit_image(void) {
	zest_device device = zest_EndDeviceBuilder(BeginD3D12TestBuilder());
	if (!device) return 1;
	zest_uint source_pixels[16 * 16];
	for (zest_uint index = 0; index != 16 * 16; ++index) {
		source_pixels[index] = blit_quadrant_colours[((index / 16) >= 8 ? 2 : 0) + ((index % 16) >= 8 ? 1 : 0)];
	}
	zest_uint grey_pixels[8 * 8];
	for (zest_uint index = 0; index != 8 * 8; ++index) grey_pixels[index] = 0xFF808080u;
	zest_image_info_t source_info = zest_CreateImageInfo(16, 16);
	source_info.format = zest_format_r8g8b8a8_unorm;
	source_info.flags = zest_image_preset_texture;
	zest_image_handle source_handle = zest_CreateImageWithPixels(device, source_pixels, sizeof(source_pixels), &source_info);
	//A single mip texture can't be a blit destination, so the destination is an attachment that can also be uploaded and read back
	zest_image_info_t destination_info = zest_CreateImageInfo(8, 8);
	destination_info.format = zest_format_r8g8b8a8_unorm;
	destination_info.flags = zest_image_preset_color_attachment | zest_image_flag_transfer_dst | zest_image_flag_transfer_src;
	zest_image_handle destination_handle = zest_CreateImageWithPixels(device, grey_pixels, sizeof(grey_pixels), &destination_info);
	zest_image_info_t texture_info = zest_CreateImageInfo(8, 8);
	texture_info.format = zest_format_r8g8b8a8_unorm;
	texture_info.flags = zest_image_preset_texture | zest_image_flag_transfer_src;
	zest_image_handle texture_handle = zest_CreateImageWithPixels(device, grey_pixels, sizeof(grey_pixels), &texture_info);
	zest_image_handle undefined_handle = zest_CreateImage(device, &destination_info);
	int failed = !source_handle.value || !destination_handle.value || !texture_handle.value || !undefined_handle.value;
	if (!failed) {
		zest_image source = zest_GetImage(source_handle);
		zest_image destination = zest_GetImage(destination_handle);
		zest_image texture = zest_GetImage(texture_handle);
		zest_image undefined_destination = zest_GetImage(undefined_handle);
		const int full_source[4] = { 0, 0, 16, 16 };
		const int full_destination[4] = { 0, 0, 8, 8 };
		failed |= blit_and_check(device, source, destination, full_source, full_destination, zest_filter_nearest, blit_quadrant_colours);

		const int top_left_source[4] = { 0, 0, 8, 8 };
		const int bottom_right_destination[4] = { 4, 4, 4, 4 };
		const zest_uint sub_rect_expected[4] = { blit_quadrant_colours[0], blit_quadrant_colours[1], blit_quadrant_colours[2], blit_quadrant_colours[0] };
		failed |= blit_and_check(device, source, destination, top_left_source, bottom_right_destination, zest_filter_linear, sub_rect_expected);

		//Vulkan takes the far edge as x plus width, so this region runs from 8 back to 0
		const int flipped_destination[4] = { 8, 0, -8, 8 };
		const zest_uint flipped_expected[4] = { blit_quadrant_colours[1], blit_quadrant_colours[0], blit_quadrant_colours[3], blit_quadrant_colours[2] };
		failed |= blit_and_check(device, source, destination, full_source, flipped_destination, zest_filter_nearest, flipped_expected);

		//A never written destination stays a render target, so the transition after it keeps the blit instead of discarding it
		zest_queue queue = zest_imm_BeginCommandBuffer(device, zest_queue_graphics);
		failed |= queue == NULL || !zest_imm_BlitImage(queue, source, undefined_destination, 0, 0, 16, 16, 0, 0, 8, 8, zest_filter_nearest);
		failed |= ZEST_DX_BACKEND(image, undefined_destination)->layout != D3D12_BARRIER_LAYOUT_RENDER_TARGET || zest_ImageRawLayout(undefined_destination) != zest_image_layout_color_attachment_optimal;
		failed |= !zest_imm_TransitionImage(queue, undefined_destination, zest_resource_state_shader_read, 0, 1, 0, 1);
		failed |= !zest_imm_EndCommandBuffer(queue);
		failed |= ZEST_DX_BACKEND(image, undefined_destination)->layout != D3D12_BARRIER_LAYOUT_SHADER_RESOURCE;
		zest_uint blitted_pixels[8 * 8];
		memset(blitted_pixels, 0, sizeof(blitted_pixels));
		failed |= !zest_CopyImageToBitmap(device, undefined_destination, blitted_pixels);
		failed |= !quadrants_equal(blitted_pixels, 8, blit_quadrant_colours);

		zest_uint errors_before = zest_GetValidationErrorCount(device);
		queue = zest_imm_BeginCommandBuffer(device, zest_queue_graphics);
		failed |= queue == NULL || zest_imm_BlitImage(queue, source, texture, 0, 0, 16, 16, 0, 0, 8, 8, zest_filter_nearest) != ZEST_FALSE;
		failed |= !zest_imm_EndCommandBuffer(queue);
		failed |= zest_GetValidationErrorCount(device) != errors_before + 1;

		queue = zest_imm_BeginCommandBuffer(device, zest_queue_transfer);
		failed |= queue == NULL || zest_imm_BlitImage(queue, source, destination, 0, 0, 16, 16, 0, 0, 8, 8, zest_filter_nearest) != ZEST_FALSE;
		failed |= !zest_imm_EndCommandBuffer(queue);
		failed |= zest_GetValidationErrorCount(device) != errors_before + 2;
		zest_ResetValidationErrors(device);

		//Neither rejected blit recorded anything
		zest_uint pixels[8 * 8];
		memset(pixels, 0, sizeof(pixels));
		failed |= !zest_CopyImageToBitmap(device, destination, pixels);
		failed |= !quadrants_equal(pixels, 8, flipped_expected);
		memset(pixels, 0, sizeof(pixels));
		failed |= !zest_CopyImageToBitmap(device, texture, pixels);
		for (zest_uint index = 0; index != 8 * 8; ++index) failed |= pixels[index] != 0xFF808080u;
	}
	if (source_handle.value) zest_FreeImageNow(source_handle);
	if (destination_handle.value) zest_FreeImageNow(destination_handle);
	if (texture_handle.value) zest_FreeImageNow(texture_handle);
	if (undefined_handle.value) zest_FreeImageNow(undefined_handle);
	return failed | finish_device(device);
}

typedef struct d3d12_mip_command_task_t {
	zest_bool copy_mip;
	zest_uint blit_mip_count;
	zest_device_queue_type queue_type;
	int run_count;
} d3d12_mip_command_task_t;

static void d3d12_image_mip_pass(const zest_command_list command_list, void *user_data) {
	d3d12_mip_command_task_t *task = (d3d12_mip_command_task_t *)user_data;
	task->run_count++;
	task->queue_type = ZEST_DX_BACKEND(command_list, command_list)->queue_type;
	//A transfer pass takes both images as transfer inputs
	zest_resource_node source = zest_GetPassInputResource(command_list, "Mip Source");
	zest_resource_node destination = zest_GetPassInputResource(command_list, "Mip Destination");
	if (task->copy_mip) {
		zest_cmd_CopyImageMip(command_list, source, destination, 1, zest_shader_fragment_stage);
	}
	for (zest_uint mip = 0; mip != task->blit_mip_count; ++mip) {
		zest_cmd_BlitImageMip(command_list, source, destination, mip, zest_shader_fragment_stage);
	}
}

//A wave that uses one queue type runs on the graphics queue, so a compute pass beside the transfer pass keeps it on the copy queue
static int run_mip_command_graph(zest_context context, const char *name, zest_image source, zest_image destination, zest_bool copy_mip, zest_uint blit_mip_count, zest_device_queue_type queue_type) {
	if (!zest_BeginCommandGraph(context, name, 0)) return 1;
	d3d12_mip_command_task_t task = {};
	task.copy_mip = copy_mip;
	task.blit_mip_count = blit_mip_count;
	zest_resource_node source_node = zest_ImportImageResource("Mip Source", source, 0);
	zest_resource_node destination_node = zest_ImportImageResource("Mip Destination", destination, 0);
	zest_BeginTransferPass("Mip Pass");
	//A transfer pass declares the destination as a transfer input too, so with no output the pass would be culled
	zest_DoNotCull();
	zest_ConnectInput(source_node);
	zest_ConnectOutput(destination_node);
	zest_SetPassTask(d3d12_image_mip_pass, &task);
	zest_EndPass();
	if (queue_type == zest_queue_transfer) {
		zest_BeginComputePass("Wave Partner");
		zest_DoNotCull();
		zest_SetPassTask(zest_EmptyRenderPass, 0);
		zest_EndPass();
	}
	zest_frame_graph frame_graph = zest_EndFrameGraph();
	int failed = frame_graph == NULL || zest_FlushFrameGraphAndWait(frame_graph) != zest_semaphore_status_success;
	return failed | (task.run_count != 1) | (task.queue_type != queue_type);
}

static zest_image_handle create_two_mip_image(zest_device device, zest_format format, zest_image_flags flags) {
	zest_image_info_t image_info = zest_CreateImageInfo(8, 8);
	image_info.format = format;
	image_info.flags = flags;
	image_info.mip_levels = 2;
	return zest_CreateImage(device, &image_info);
}

static int mips_equal(zest_device device, zest_image source, zest_image destination) {
	zest_uint source_pixels[8 * 8];
	zest_uint destination_pixels[8 * 8];
	int failed = 0;
	for (zest_uint mip = 0; mip != 2; ++mip) {
		memset(source_pixels, 0, sizeof(source_pixels));
		memset(destination_pixels, 0xCD, sizeof(destination_pixels));
		failed |= !zest__dx_read_subresource(device, source, mip, 0, source_pixels);
		failed |= !zest__dx_read_subresource(device, destination, mip, 0, destination_pixels);
		zest_uint mip_size = 8 >> mip;
		failed |= memcmp(source_pixels, destination_pixels, mip_size * mip_size * sizeof(zest_uint)) != 0;
	}
	return failed;
}

static float half_to_float(unsigned short half) {
	int exponent = (half >> 10) & 0x1F;
	float mantissa = (float)(half & 0x3FF);
	float magnitude = exponent ? ldexpf(1.f + mantissa / 1024.f, exponent - 15) : ldexpf(mantissa / 1024.f, -14);
	return (half & 0x8000) ? -magnitude : magnitude;
}

//Each mip of the half float destination holds the unorm source's channels converted to floats
static int converted_mips_equal(zest_device device, zest_image source, zest_image destination) {
	zest_uint source_pixels[8 * 8];
	unsigned short destination_channels[8 * 8 * 4];
	int failed = 0;
	for (zest_uint mip = 0; mip != 2; ++mip) {
		memset(source_pixels, 0, sizeof(source_pixels));
		memset(destination_channels, 0, sizeof(destination_channels));
		failed |= !zest__dx_read_subresource(device, source, mip, 0, source_pixels);
		failed |= !zest__dx_read_subresource(device, destination, mip, 0, destination_channels);
		zest_uint mip_size = 8 >> mip;
		for (zest_uint channel_index = 0; channel_index != mip_size * mip_size * 4; ++channel_index) {
			float expected = (float)((const unsigned char *)source_pixels)[channel_index];
			failed |= fabsf(half_to_float(destination_channels[channel_index]) * 255.f - expected) > 0.5f;
		}
	}
	return failed;
}

int test_d3d12_image_mip_commands(void) {
	zest_device device = zest_EndDeviceBuilder(BeginD3D12TestBuilder());
	if (!device) return 1;
	zest_create_context_info_t info = zest_CreateContextInfo();
	zest_context context = zest_CreateHeadlessContext(device, &info);
	int failed = context == NULL;
	zest_uint pixels[8 * 8];
	for (zest_uint index = 0; index != 8 * 8; ++index) pixels[index] = (index * 2654435761u) | 0xFF000000u;
	zest_image_handle source_handle = create_two_mip_image(device, zest_format_r8g8b8a8_unorm, zest_image_preset_texture_mipmaps);
	zest_image_handle destination_handle = create_two_mip_image(device, zest_format_r8g8b8a8_unorm, zest_image_preset_texture | zest_image_flag_transfer_src);
	zest_image_handle float_handle = create_two_mip_image(device, zest_format_r16g16b16a16_sfloat, zest_image_preset_texture | zest_image_flag_transfer_src);
	zest_image_handle converted_handle = create_two_mip_image(device, zest_format_r16g16b16a16_sfloat, zest_image_preset_texture | zest_image_flag_transfer_src);
	failed |= !source_handle.value || !destination_handle.value || !float_handle.value || !converted_handle.value;
	if (!failed) {
		zest_image source = zest_GetImage(source_handle);
		zest_image destination = zest_GetImage(destination_handle);
		zest_image float_destination = zest_GetImage(float_handle);
		zest_image converted_destination = zest_GetImage(converted_handle);
		failed |= source->info.mip_levels != 2 || destination->info.mip_levels != 2;
		//So the destination's first barrier, on a copy list, goes from UNDEFINED to COMMON with the discard flag
		failed |= !(ZEST_DX_BACKEND(image, destination)->resource_flags & D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET) || ZEST_DX_BACKEND(image, destination)->layout != D3D12_BARRIER_LAYOUT_UNDEFINED;
		failed |= !zest_CopyBitmapToImage(device, pixels, sizeof(pixels), source, 8, 8);
		//A copy queue barrier can't start from SHADER_RESOURCE (1334), and the frame graph doesn't release to COMMON yet, so the source waits there
		const D3D12_BARRIER_LAYOUT common_layout = D3D12_BARRIER_LAYOUT_COMMON;
		zest_queue queue = zest_imm_BeginCommandBuffer(device, zest_queue_graphics);
		failed |= queue == NULL || !zest__dx_transition_image_layout(queue, source, zest_image_layout_shader_read_only_optimal, &common_layout, 0, 2, 0, 1);
		failed |= !zest_imm_EndCommandBuffer(queue);
		failed |= run_mip_command_graph(context, "Image Mip Commands", source, destination, ZEST_TRUE, 1, zest_queue_transfer);
		failed |= mips_equal(device, source, destination);

		zest_uint errors_before = zest_GetValidationErrorCount(device);
		failed |= run_mip_command_graph(context, "Image Mip Format Blit", source, float_destination, ZEST_FALSE, 1, zest_queue_transfer);
		failed |= zest_GetValidationErrorCount(device) != errors_before + 1;
		zest_ResetValidationErrors(device);

		//Alone in its wave the transfer pass runs on the graphics queue, where a blit between formats draws
		failed |= run_mip_command_graph(context, "Image Mip Format Draw", source, converted_destination, ZEST_FALSE, 2, zest_queue_graphics);
		failed |= converted_mips_equal(device, source, converted_destination);
	}
	if (source_handle.value) zest_FreeImageNow(source_handle);
	if (destination_handle.value) zest_FreeImageNow(destination_handle);
	if (float_handle.value) zest_FreeImageNow(float_handle);
	if (converted_handle.value) zest_FreeImageNow(converted_handle);
	if (context) zest_DestroyContext(context);
	return failed | finish_device(device);
}

static zest_clear_value_t colour_clear_value(float red, float green, float blue) {
	zest_clear_value_t clear_value = {};
	clear_value.color.x = red;
	clear_value.color.y = green;
	clear_value.color.z = blue;
	clear_value.color.w = 1.f;
	return clear_value;
}

//Every mip of a 16x16 image reads back as the expected colour
static int mips_match(zest_device device, zest_image image, const int expected[4]) {
	zest_uint pixels[16 * 16];
	int failed = 0;
	for (zest_uint mip = 0; mip != image->info.mip_levels; ++mip) {
		zest_uint mip_size = 16 >> mip;
		memset(pixels, 0, sizeof(pixels));
		failed |= !zest__dx_read_subresource(device, image, mip, 0, pixels);
		failed |= !pixels_match(pixels, mip_size * mip_size, expected);
	}
	return failed;
}

//Clears on an immediate queue of the given type, and checks the image is still tracked in the layouts it started in
static int clear_colour_image(zest_device device, zest_image image, zest_device_queue_type queue_type, zest_clear_value_t clear_value, zest_bool expected) {
	int layout = zest_ImageRawLayout(image);
	D3D12_BARRIER_LAYOUT backend_layout = ZEST_DX_BACKEND(image, image)->layout;
	zest_queue queue = zest_imm_BeginCommandBuffer(device, queue_type);
	if (!queue) return 1;
	int failed = zest__dx_queue_type(queue) != queue_type;
	failed |= zest_imm_ClearColorImage(queue, image, clear_value) != expected;
	failed |= !zest_imm_EndCommandBuffer(queue);
	failed |= zest_ImageRawLayout(image) != layout || ZEST_DX_BACKEND(image, image)->layout != backend_layout;
	return failed;
}

static zest_image_handle create_integer_target(zest_device device, zest_format format) {
	zest_image_info_t image_info = zest_CreateImageInfo(16, 16);
	image_info.format = format;
	image_info.flags = zest_image_flag_color_attachment | zest_image_flag_storage | zest_image_flag_transfer_src | zest_image_flag_device_local;
	return zest_CreateImage(device, &image_info);
}

//As on Vulkan, an integer image clears to the colour's bits read as integers, on either path
static int integer_clear_matches(zest_device device, zest_image image, zest_uint value, zest_bool forced_uav) {
	zest_clear_value_t clear_value = {};
	zest_uint bits[4] = { value, value, value, value };
	memcpy(&clear_value.color, bits, sizeof(bits));
	zest__dx_forced_uav_clear = forced_uav;
	int failed = clear_colour_image(device, image, zest_queue_graphics, clear_value, ZEST_TRUE);
	zest__dx_forced_uav_clear = ZEST_FALSE;
	zest_uint pixels[16 * 16];
	memset(pixels, 0xCD, sizeof(pixels));
	failed |= !zest__dx_read_subresource(device, image, 0, 0, pixels);
	for (zest_uint index = 0; index != 16 * 16; ++index) failed |= pixels[index] != value;
	return failed;
}

//Clears through the render target view, then resets and clears through the UAV, and both must give the same value
static int integer_paths_match(zest_device device, zest_format format, zest_uint value) {
	zest_image_handle image_handle = create_integer_target(device, format);
	if (!image_handle.value) return 1;
	zest_image image = zest_GetImage(image_handle);
	D3D12_RESOURCE_FLAGS resource_flags = ZEST_DX_BACKEND(image, image)->resource_flags;
	int failed = !(resource_flags & D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET) || !(resource_flags & D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
	failed |= integer_clear_matches(device, image, value, ZEST_FALSE);
	failed |= integer_clear_matches(device, image, 0, ZEST_TRUE);
	failed |= integer_clear_matches(device, image, value, ZEST_TRUE);
	zest_FreeImageNow(image_handle);
	return failed;
}

int test_d3d12_clear_colour_image(void) {
	zest_device device = zest_EndDeviceBuilder(BeginD3D12TestBuilder());
	if (!device) return 1;
	zest_uint black_pixels[16 * 16];
	for (zest_uint index = 0; index != 16 * 16; ++index) black_pixels[index] = 0xFF000000u;
	//Mip generation draws into the mips, so the mipmaps preset makes the image a render target
	zest_image_info_t mipped_info = zest_CreateImageInfo(16, 16);
	mipped_info.format = zest_format_r8g8b8a8_unorm;
	mipped_info.flags = zest_image_preset_texture_mipmaps;
	mipped_info.mip_levels = 3;
	zest_image_handle mipped_handle = zest_CreateImageWithPixels(device, black_pixels, sizeof(black_pixels), &mipped_info);
	zest_image_info_t storage_info = zest_CreateImageInfo(16, 16);
	storage_info.format = zest_format_r8g8b8a8_unorm;
	storage_info.flags = zest_image_flag_storage | zest_image_flag_transfer_dst | zest_image_flag_transfer_src | zest_image_flag_device_local;
	storage_info.mip_levels = 3;
	zest_image_handle storage_handle = zest_CreateImage(device, &storage_info);
	zest_image_info_t texture_info = zest_CreateImageInfo(16, 16);
	texture_info.format = zest_format_r8g8b8a8_unorm;
	texture_info.flags = zest_image_preset_texture;
	zest_image_handle texture_handle = zest_CreateImageWithPixels(device, black_pixels, sizeof(black_pixels), &texture_info);
	int failed = !mipped_handle.value || !storage_handle.value || !texture_handle.value;
	if (!failed) {
		zest_image mipped = zest_GetImage(mipped_handle);
		zest_image storage = zest_GetImage(storage_handle);
		zest_image texture = zest_GetImage(texture_handle);
		D3D12_RESOURCE_FLAGS mipped_flags = ZEST_DX_BACKEND(image, mipped)->resource_flags;
		D3D12_RESOURCE_FLAGS storage_flags = ZEST_DX_BACKEND(image, storage)->resource_flags;
		failed |= !(mipped_flags & D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET) || (mipped_flags & D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
		failed |= !(storage_flags & D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET) || !(storage_flags & D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
		failed |= ZEST_DX_BACKEND(image, texture)->resource_flags & (D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET | D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
		failed |= clear_colour_image(device, mipped, zest_queue_graphics, colour_clear_value(0.2f, 0.4f, 0.6f), ZEST_TRUE);
		failed |= mips_match(device, mipped, blue_expected);

		//The storage image could take either path on the graphics queue, and a compute queue can only clear through a UAV
		zest__dx_forced_uav_clear = ZEST_TRUE;
		failed |= clear_colour_image(device, storage, zest_queue_graphics, colour_clear_value(0.6f, 0.4f, 0.2f), ZEST_TRUE);
		failed |= mips_match(device, storage, orange_expected);
		failed |= clear_colour_image(device, storage, zest_queue_compute, colour_clear_value(0.2f, 0.4f, 0.6f), ZEST_TRUE);
		zest__dx_forced_uav_clear = ZEST_FALSE;
		failed |= mips_match(device, storage, blue_expected);

		//Both values are below 2^24, which a render target view's float colour holds exactly
		failed |= integer_paths_match(device, zest_format_r32_uint, 0x00ABCDEFu);
		failed |= integer_paths_match(device, zest_format_r32_sint, (zest_uint)-1234567);

		zest_uint errors_before = zest_GetValidationErrorCount(device);
		failed |= clear_colour_image(device, texture, zest_queue_graphics, colour_clear_value(0.2f, 0.4f, 0.6f), ZEST_FALSE);
		failed |= zest_GetValidationErrorCount(device) != errors_before + 1;
		failed |= clear_colour_image(device, mipped, zest_queue_transfer, colour_clear_value(0.6f, 0.4f, 0.2f), ZEST_FALSE);
		failed |= zest_GetValidationErrorCount(device) != errors_before + 2;
		zest_ResetValidationErrors(device);

		//Neither rejected clear recorded anything
		const int black_expected[4] = { 0, 0, 0, 255 };
		failed |= mips_match(device, texture, black_expected);
		failed |= mips_match(device, mipped, blue_expected);
	}
	if (mipped_handle.value) zest_FreeImageNow(mipped_handle);
	if (storage_handle.value) zest_FreeImageNow(storage_handle);
	if (texture_handle.value) zest_FreeImageNow(texture_handle);
	return failed | finish_device(device);
}

int test_d3d12_clear_depth_image(void) {
	zest_device device = zest_EndDeviceBuilder(BeginD3D12TestBuilder());
	if (!device) return 1;
	zest_image_info_t depth_info = zest_CreateImageInfo(16, 16);
	depth_info.format = zest_format_d32_sfloat;
	depth_info.flags = zest_image_preset_depth_attachment | zest_image_flag_transfer_src;
	zest_image_handle depth_handle = zest_CreateImage(device, &depth_info);
	int failed = !depth_handle.value;
	if (!failed) {
		zest_image depth_image = zest_GetImage(depth_handle);
		zest_queue queue = zest_imm_BeginCommandBuffer(device, zest_queue_graphics);
		failed |= queue == NULL || !zest_imm_ClearDepthStencilImage(queue, depth_image, 0.25f, 0);
		failed |= !zest_imm_EndCommandBuffer(queue);
		float depths[16 * 16];
		memset(depths, 0, sizeof(depths));
		failed |= !zest__dx_read_subresource(device, depth_image, 0, 0, depths);
		for (zest_uint index = 0; index != 16 * 16; ++index) failed |= depths[index] != 0.25f;

		zest_uint errors_before = zest_GetValidationErrorCount(device);
		queue = zest_imm_BeginCommandBuffer(device, zest_queue_compute);
		failed |= queue == NULL || zest_imm_ClearDepthStencilImage(queue, depth_image, 1.f, 0) != ZEST_FALSE;
		failed |= !zest_imm_EndCommandBuffer(queue);
		failed |= zest_GetValidationErrorCount(device) != errors_before + 1;
		zest_ResetValidationErrors(device);
		memset(depths, 0, sizeof(depths));
		failed |= !zest__dx_read_subresource(device, depth_image, 0, 0, depths);
		for (zest_uint index = 0; index != 16 * 16; ++index) failed |= depths[index] != 0.25f;
	}
	if (depth_handle.value) zest_FreeImageNow(depth_handle);
	return failed | finish_device(device);
}

typedef struct d3d12_image_clear_task_t {
	zest_bool cleared;
	zest_device_queue_type queue_type;
	int run_count;
} d3d12_image_clear_task_t;

static void d3d12_image_clear_pass(const zest_command_list command_list, void *user_data) {
	d3d12_image_clear_task_t *task = (d3d12_image_clear_task_t *)user_data;
	task->run_count++;
	task->queue_type = ZEST_DX_BACKEND(command_list, command_list)->queue_type;
	zest_resource_node target = zest_GetPassOutputResource(command_list, "Clear Target");
	task->cleared = zest_cmd_ImageClear(command_list, zest_GetResourceImage(target));
}

//A wave that uses one queue type runs on the graphics queue, so a transfer pass beside the compute pass keeps it on the compute queue
static int run_image_clear_graph(zest_context context, const char *name, zest_image image, zest_bool render_pass, zest_device_queue_type queue_type, zest_bool expected) {
	if (!zest_BeginCommandGraph(context, name, 0)) return 1;
	d3d12_image_clear_task_t task = {};
	zest_resource_node node = zest_ImportImageResource("Clear Target", image, 0);
	if (render_pass) {
		zest_BeginRenderPass("Clear Pass");
	} else {
		zest_BeginComputePass("Clear Pass");
	}
	zest_ConnectOutput(node);
	zest_SetPassTask(d3d12_image_clear_pass, &task);
	zest_EndPass();
	if (queue_type == zest_queue_compute) {
		zest_BeginTransferPass("Wave Partner");
		zest_DoNotCull();
		zest_SetPassTask(zest_EmptyRenderPass, 0);
		zest_EndPass();
	}
	zest_frame_graph frame_graph = zest_EndFrameGraph();
	int failed = frame_graph == NULL || zest_FlushFrameGraphAndWait(frame_graph) != zest_semaphore_status_success;
	return failed | (task.run_count != 1) | (task.queue_type != queue_type) | (task.cleared != expected);
}

static int image_is_zero(zest_device device, zest_image image) {
	zest_uint pixels[16 * 16];
	memset(pixels, 0xCD, sizeof(pixels));
	int failed = !zest__dx_read_subresource(device, image, 0, 0, pixels);
	for (zest_uint index = 0; index != 16 * 16; ++index) failed |= pixels[index] != 0;
	return failed;
}

int test_d3d12_image_clear_command(void) {
	zest_device device = zest_EndDeviceBuilder(BeginD3D12TestBuilder());
	if (!device) return 1;
	zest_create_context_info_t info = zest_CreateContextInfo();
	zest_context context = zest_CreateHeadlessContext(device, &info);
	int failed = context == NULL;
	zest_uint white_pixels[16 * 16];
	for (zest_uint index = 0; index != 16 * 16; ++index) white_pixels[index] = 0xFFFFFFFFu;
	zest_image_info_t storage_info = zest_CreateImageInfo(16, 16);
	storage_info.format = zest_format_r8g8b8a8_unorm;
	storage_info.flags = zest_image_preset_storage | zest_image_flag_transfer_src | zest_image_flag_transfer_dst;
	zest_image_handle storage_handle = zest_CreateImageWithPixels(device, white_pixels, sizeof(white_pixels), &storage_info);
	zest_image_handle target_handle = create_clear_target(device);
	failed |= !storage_handle.value || !target_handle.value;
	if (!failed) {
		zest_image storage = zest_GetImage(storage_handle);
		failed |= run_image_clear_graph(context, "Image Clear", storage, ZEST_FALSE, zest_queue_graphics, ZEST_TRUE);
		failed |= image_is_zero(device, storage);
		failed |= zest_GetValidationErrorCount(device) != 0;

		failed |= !zest_CopyBitmapToImage(device, white_pixels, sizeof(white_pixels), storage, 16, 16);
		failed |= run_image_clear_graph(context, "Image Clear Compute", storage, ZEST_FALSE, zest_queue_compute, ZEST_TRUE);
		failed |= image_is_zero(device, storage);
		failed |= zest_GetValidationErrorCount(device) != 0;

		//D3D12 can't clear inside a render pass
		failed |= run_image_clear_graph(context, "Image Clear Render Pass", zest_GetImage(target_handle), ZEST_TRUE, zest_queue_graphics, ZEST_FALSE);
		failed |= zest_GetValidationErrorCount(device) != 1;
		zest_ResetValidationErrors(device);
	}
	if (storage_handle.value) zest_FreeImageNow(storage_handle);
	if (target_handle.value) zest_FreeImageNow(target_handle);
	if (context) zest_DestroyContext(context);
	return failed | finish_device(device);
}

//zest_CreateImage only makes single sampled images, so this follows its steps with a sample count
static zest_image_handle create_multisampled_attachment(zest_device device, zest_uint width, zest_uint height, zest_sample_count_flags sample_count) {
	zest_image_handle handle = zest__new_image(device);
	zest_image image = zest__get_image_unsafe(handle);
	image->info = zest_CreateImageInfo(width, height);
	image->info.format = zest_format_r8g8b8a8_unorm;
	image->info.flags = zest_image_preset_color_attachment;
	image->info.sample_count = sample_count;
	image->info.aspect_flags = zest_image_aspect_color_bit;
	if (!device->platform->create_image(device, 0, image, 1, sample_count, image->info.flags)) {
		zest__cleanup_image(image);
		return ZEST__ZERO_INIT(zest_image_handle);
	}
	image->default_view = device->platform->create_image_view(device, image, zest_image_view_type_2d, 1, 0, 0, 1, 0);
	image->default_view->handle.store = &device->resource_stores[zest_handle_type_views];
	zest__activate_resource(handle.store, handle.value);
	return handle;
}

int test_d3d12_resolve_image(void) {
	zest_device device = zest_EndDeviceBuilder(BeginD3D12TestBuilder());
	if (!device) return 1;
	zest_uint black_pixels[16 * 16];
	for (zest_uint index = 0; index != 16 * 16; ++index) black_pixels[index] = 0xFF000000u;
	zest_image_handle multisampled_handle = create_multisampled_attachment(device, 16, 16, zest_sample_count_4_bit);
	zest_image_info_t resolved_info = zest_CreateImageInfo(16, 16);
	resolved_info.format = zest_format_r8g8b8a8_unorm;
	resolved_info.flags = zest_image_preset_texture | zest_image_flag_transfer_src;
	zest_image_handle resolved_handle = zest_CreateImageWithPixels(device, black_pixels, sizeof(black_pixels), &resolved_info);
	zest_image_handle single_handle = zest_CreateImageWithPixels(device, black_pixels, sizeof(black_pixels), &resolved_info);
	zest_image_info_t fresh_info = zest_CreateImageInfo(16, 16);
	fresh_info.format = zest_format_r8g8b8a8_unorm;
	fresh_info.flags = zest_image_preset_color_attachment | zest_image_flag_transfer_src;
	zest_image_handle fresh_handle = zest_CreateImage(device, &fresh_info);
	int failed = !multisampled_handle.value || !resolved_handle.value || !single_handle.value || !fresh_handle.value;
	if (!failed) {
		zest_image multisampled = zest_GetImage(multisampled_handle);
		zest_image resolved = zest_GetImage(resolved_handle);
		zest_image single = zest_GetImage(single_handle);
		failed |= ZEST_DX_BACKEND(image, multisampled)->sample_count != 4;
		zest_queue queue = zest_imm_BeginCommandBuffer(device, zest_queue_graphics);
		failed |= queue == NULL || !zest_imm_ClearColorImage(queue, multisampled, colour_clear_value(0.2f, 0.4f, 0.6f));
		failed |= !zest_imm_EndCommandBuffer(queue);
		int multisampled_layout = zest_ImageRawLayout(multisampled);
		D3D12_BARRIER_LAYOUT multisampled_backend_layout = ZEST_DX_BACKEND(image, multisampled)->layout;
		queue = zest_imm_BeginCommandBuffer(device, zest_queue_graphics);
		failed |= queue == NULL || !zest_imm_ResolveImage(queue, multisampled, resolved);
		failed |= !zest_imm_EndCommandBuffer(queue);
		failed |= mips_match(device, resolved, blue_expected);
		failed |= zest_ImageRawLayout(multisampled) != multisampled_layout || ZEST_DX_BACKEND(image, multisampled)->layout != multisampled_backend_layout;
		failed |= zest_ImageRawLayout(resolved) != zest_image_layout_shader_read_only_optimal || ZEST_DX_BACKEND(image, resolved)->layout != D3D12_BARRIER_LAYOUT_SHADER_RESOURCE;

		//A never written destination keeps what was resolved into it, and is left where Vulkan's resolve leaves it
		zest_image fresh = zest_GetImage(fresh_handle);
		failed |= !(ZEST_DX_BACKEND(image, fresh)->resource_flags & D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET) || ZEST_DX_BACKEND(image, fresh)->layout != D3D12_BARRIER_LAYOUT_UNDEFINED;
		queue = zest_imm_BeginCommandBuffer(device, zest_queue_graphics);
		failed |= queue == NULL || !zest_imm_ResolveImage(queue, multisampled, fresh);
		failed |= !zest_imm_EndCommandBuffer(queue);
		failed |= zest_ImageRawLayout(fresh) != zest_image_layout_transfer_dst_optimal || ZEST_DX_BACKEND(image, fresh)->layout != D3D12_BARRIER_LAYOUT_COPY_DEST;
		failed |= mips_match(device, fresh, blue_expected);
		failed |= zest_GetValidationErrorCount(device) != 0;

		zest_uint errors_before = zest_GetValidationErrorCount(device);
		queue = zest_imm_BeginCommandBuffer(device, zest_queue_graphics);
		failed |= queue == NULL || zest_imm_ResolveImage(queue, single, resolved) != ZEST_FALSE;
		failed |= !zest_imm_EndCommandBuffer(queue);
		failed |= zest_GetValidationErrorCount(device) != errors_before + 1;
		zest_ResetValidationErrors(device);
		failed |= mips_match(device, resolved, blue_expected);
	}
	if (multisampled_handle.value) zest_FreeImageNow(multisampled_handle);
	if (resolved_handle.value) zest_FreeImageNow(resolved_handle);
	if (single_handle.value) zest_FreeImageNow(single_handle);
	if (fresh_handle.value) zest_FreeImageNow(fresh_handle);
	return failed | finish_device(device);
}

int test_d3d12_transient_first_use_layout(void) {
	zest_resource_node_t node = {};
	zest_dx_image_backend_t backend = {};
	node.flags = zest_resource_node_flag_transient;
	node.image.backend = (zest_image_backend)&backend;
	node.image.info.format = zest_format_r8g8b8a8_unorm;
	node.image.info.sample_count = 1;
	node.image.info.flags = zest_image_preset_color_attachment;
	backend.sample_count = 1;
	backend.resource_flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
	//A cached graph's reused image still tracks the layout its previous execution left it in
	backend.layout = D3D12_BARRIER_LAYOUT_RENDER_TARGET;
	zest_dx_frame_graph_image_barrier_t record = {};
	record.resource = &node;
	record.old_layout = zest_image_layout_undefined;
	record.new_layout = zest_image_layout_color_attachment_optimal;
	record.dst_access = zest_access_color_attachment_write_bit;
	record.src_stage = zest_pipeline_stage_top_of_pipe_bit;
	record.dst_stage = zest_pipeline_stage_color_attachment_output_bit;
	D3D12_BARRIER_LAYOUT layout_before = zest__dx_record_layout_before(&record, &node.image);
	int failed = layout_before != D3D12_BARRIER_LAYOUT_UNDEFINED;
	D3D12_TEXTURE_BARRIER barrier = zest__dx_subresource_barrier(&node.image, layout_before, D3D12_BARRIER_LAYOUT_RENDER_TARGET, record.src_access, record.dst_access, record.src_stage, record.dst_stage, 0, 1, 0, 1);
	failed |= barrier.LayoutBefore != D3D12_BARRIER_LAYOUT_UNDEFINED || !(barrier.Flags & D3D12_TEXTURE_BARRIER_FLAG_DISCARD);
	//An aliased transient's record carries the previous occupant's last write, which UNDEFINED can't be paired with
	record.src_access = zest_access_color_attachment_write_bit;
	record.src_stage = zest_pipeline_stage_color_attachment_output_bit;
	barrier = zest__dx_subresource_barrier(&node.image, zest__dx_record_layout_before(&record, &node.image), D3D12_BARRIER_LAYOUT_RENDER_TARGET, record.src_access, record.dst_access, record.src_stage, record.dst_stage, 0, 1, 0, 1);
	failed |= barrier.AccessBefore != D3D12_BARRIER_ACCESS_NO_ACCESS || barrier.SyncBefore != D3D12_BARRIER_SYNC_NONE;
	failed |= barrier.AccessAfter != D3D12_BARRIER_ACCESS_RENDER_TARGET || barrier.SyncAfter != D3D12_BARRIER_SYNC_RENDER_TARGET;
	failed |= !(barrier.Flags & D3D12_TEXTURE_BARRIER_FLAG_DISCARD);
	//Only the first use starts over, later records follow the tracked layout
	record.old_layout = zest_image_layout_color_attachment_optimal;
	failed |= zest__dx_record_layout_before(&record, &node.image) != D3D12_BARRIER_LAYOUT_RENDER_TARGET;
	//An image that isn't transient keeps its contents, so it starts from its tracked layout
	record.old_layout = zest_image_layout_undefined;
	node.flags = zest_resource_node_flag_imported;
	failed |= zest__dx_record_layout_before(&record, &node.image) != D3D12_BARRIER_LAYOUT_RENDER_TARGET;
	return failed;
}

int test_d3d12_arena_backing(void) {
	zest_device device = zest_EndDeviceBuilder(BeginD3D12TestBuilder());
	if (!device) return 1;
	zest_create_context_info_t info = zest_CreateContextInfo();
	zest_context context = zest_CreateHeadlessContext(device, &info);
	int failed = context == NULL;
	if (context) {
		const zest_uint categories[3] = { ZEST_ARENA_CATEGORY_GPU_BUFFERS, ZEST_ARENA_CATEGORY_CPU_BUFFERS, ZEST_ARENA_CATEGORY_IMAGE_BASE + ZEST_DX_HEAP_CATEGORY_TEXTURES };
		const zest_size backing_size = 1024 * 1024;
		int allocations_before = device->memory_allocation_count;
		zest_device_memory_pool backings[3] = {};
		for (int index = 0; index != 3; ++index) {
			backings[index] = device->platform->create_arena_backing(device, context, categories[index], backing_size);
			failed |= backings[index] == NULL || backings[index]->size != backing_size;
		}
		failed |= device->memory_allocation_count != allocations_before + 3;
		failed |= backings[1] == NULL || backings[1]->mapped == NULL;
		failed |= backings[2] == NULL || ZEST_DX_BACKEND(device_memory_pool, backings[2])->heap == NULL;
		if (backings[0]) {
			//A transient buffer is a proxy into the backing, so the buffer helpers reach the backing's buffer at the proxy's offset
			ID3D12Resource *backing_buffer = ZEST_DX_BACKEND(device_memory_pool, backings[0])->buffer;
			zest_buffer_t proxy = {};
			proxy.memory_pool = backings[0];
			proxy.memory_offset = 256;
			failed |= backing_buffer == NULL || zest__dx_buffer_resource(&proxy) != backing_buffer;
			failed |= backing_buffer && zest__dx_buffer_address(&proxy) != backing_buffer->GetGPUVirtualAddress() + 256;
		}
		for (int index = 0; index != 3; ++index) {
			if (backings[index]) device->platform->free_arena_backing(device, context, backings[index]);
		}
		failed |= device->memory_allocation_count != allocations_before;
		zest_DestroyContext(context);
	}
	return failed | finish_device(device);
}

//Clears a colour target, samples it in a pass that clears a depth target and a second colour target, then samples that into a third target, which can take the bytes the first two left
static zest_frame_graph build_transient_image_graph(zest_context context, zest_frame_graph_cache_key_t *cache_key, zest_bool windowed) {
	zest_bool began = windowed ? zest_BeginFrameGraph(context, "Transient Images", cache_key) : zest_BeginCommandGraph(context, "Transient Images", cache_key);
	if (!began) return NULL;
	if (windowed) {
		zest_ImportSwapchainResource();
		zest_BeginRenderPass("Present Pass");
		zest_ConnectSwapChainOutput();
		zest_SetPassTask(zest_EmptyRenderPass, 0);
		zest_EndPass();
	}
	zest_image_resource_info_t colour_info = { zest_format_r8g8b8a8_unorm };
	colour_info.width = 64;
	colour_info.height = 64;
	zest_image_resource_info_t depth_info = { zest_format_depth };
	depth_info.width = 64;
	depth_info.height = 64;
	zest_resource_node first_target = zest_AddTransientImageResource("First Target", &colour_info);
	zest_resource_node depth_target = zest_AddTransientImageResource("Depth Target", &depth_info);
	zest_resource_node second_target = zest_AddTransientImageResource("Second Target", &colour_info);
	zest_resource_node third_target = zest_AddTransientImageResource("Third Target", &colour_info);
	zest_FlagResourceAsEssential(depth_target);
	zest_FlagResourceAsEssential(third_target);
	zest_SetResourceClearColor(first_target, 0.2f, 0.4f, 0.6f, 1.0f);

	zest_BeginRenderPass("First Pass");
	zest_ConnectOutput(first_target);
	zest_SetPassTask(zest_EmptyRenderPass, 0);
	zest_EndPass();

	zest_BeginRenderPass("Depth Pass");
	zest_ConnectInput(first_target);
	zest_ConnectOutput(second_target);
	zest_ConnectOutput(depth_target);
	zest_SetPassTask(zest_EmptyRenderPass, 0);
	zest_EndPass();

	zest_BeginRenderPass("Third Pass");
	zest_ConnectInput(second_target);
	zest_ConnectOutput(third_target);
	zest_SetPassTask(zest_EmptyRenderPass, 0);
	zest_EndPass();
	return zest_EndFrameGraph();
}

//The third target's first barrier was compiled with the depth target's last write, and must have been sent with none of it
static int third_target_barrier_is_clean(zest_frame_graph frame_graph) {
	int found = 0;
	for (zest_uint pass_index = 0; pass_index != zest_GetFrameGraphFinalPassCount(frame_graph); ++pass_index) {
		const zest_pass_group_t *pass_group = zest_GetFrameGraphFinalPass(frame_graph, pass_index);
		zest_dx_execution_barriers_backend_t *backend = ZEST_DX_BACKEND(execution_barriers, &pass_group->execution_details.barriers);
		for (zest_uint index = 0; index != zest_vec_size(backend->acquire_images); ++index) {
			const zest_dx_frame_graph_image_barrier_t *record = &backend->acquire_images[index];
			if (strcmp(record->resource->name, "Third Target") != 0) continue;
			const D3D12_TEXTURE_BARRIER *barrier = &backend->acquire_translated[index];
			found = ZEST__FLAGGED(record->src_access, zest_access_depth_stencil_attachment_write_bit);
			found &= barrier->LayoutBefore == D3D12_BARRIER_LAYOUT_UNDEFINED && barrier->LayoutAfter == D3D12_BARRIER_LAYOUT_RENDER_TARGET;
			found &= barrier->AccessBefore == D3D12_BARRIER_ACCESS_NO_ACCESS && barrier->SyncBefore == D3D12_BARRIER_SYNC_NONE;
			found &= (barrier->Flags & D3D12_TEXTURE_BARRIER_FLAG_DISCARD) != 0;
		}
	}
	return found;
}

//The barrier last sent for the third target's first use, and the image it was sent for
static const D3D12_TEXTURE_BARRIER *third_target_first_barrier(zest_frame_graph frame_graph, zest_dx_image_backend_t **image_backend) {
	for (zest_uint pass_index = 0; pass_index != zest_GetFrameGraphFinalPassCount(frame_graph); ++pass_index) {
		const zest_pass_group_t *pass_group = zest_GetFrameGraphFinalPass(frame_graph, pass_index);
		zest_dx_execution_barriers_backend_t *backend = ZEST_DX_BACKEND(execution_barriers, &pass_group->execution_details.barriers);
		for (zest_uint index = 0; index != zest_vec_size(backend->acquire_images); ++index) {
			const zest_dx_frame_graph_image_barrier_t *record = &backend->acquire_images[index];
			if (strcmp(record->resource->name, "Third Target") != 0 || record->old_layout != zest_image_layout_undefined) continue;
			*image_backend = ZEST_DX_BACKEND(image, record->resource->view->image);
			return &backend->acquire_translated[index];
		}
	}
	return NULL;
}

//Command graphs can't run again from the cache, so frames reuse each slot's images, which still track the layout the slot's last frame left
static int run_cached_transient_frames(void) {
	D3D12TestWindow window;
	if (!open_test_window(&window, "D3D12 Transient Images")) {
		close_test_window(&window);
		return 1;
	}
	zest_device device = create_windowed_test_device(&window);
	if (!device) {
		close_test_window(&window);
		return 1;
	}
	zest_create_context_info_t info = zest_CreateContextInfo();
	zest_context context = zest_CreateContext(device, &window.window_data, &info);
	int failed = context == NULL;
	if (context) {
		const int frame_count = ZEST_MAX_FIF + 2;
		zest_dx_image_backend_t *slot_backends[ZEST_MAX_FIF + 2] = {};
		D3D12_BARRIER_LAYOUT layouts_left[ZEST_MAX_FIF + 2] = {};
		D3D12_BARRIER_LAYOUT layouts_before[ZEST_MAX_FIF + 2] = {};
		int cached_count = 0;
		for (int frame = 0; frame != frame_count && !failed; ++frame) {
			SDL_Event event;
			while (SDL_PollEvent(&event)) {}
			zest_UpdateDevice(device);
			if (!zest_BeginFrame(context)) {
				failed = 1;
				break;
			}
			zest_frame_graph_cache_key_t cache_key = zest_InitialiseCacheKey(context, 0, 0);
			zest_frame_graph frame_graph = zest_GetCachedFrameGraph(context, &cache_key);
			cached_count += frame_graph != NULL;
			if (!frame_graph) frame_graph = build_transient_image_graph(context, &cache_key, ZEST_TRUE);
			zest_EndFrame(context, frame_graph);
			failed |= frame_graph == NULL || zest_GetFrameGraphResult(frame_graph) != zest_fgs_success;
			const D3D12_TEXTURE_BARRIER *barrier = frame_graph ? third_target_first_barrier(frame_graph, &slot_backends[frame]) : NULL;
			failed |= barrier == NULL;
			if (barrier) {
				layouts_before[frame] = barrier->LayoutBefore;
				layouts_left[frame] = slot_backends[frame]->layout;
			}
		}
		failed |= cached_count != frame_count - 1;
		for (int frame = ZEST_MAX_FIF; frame != frame_count; ++frame) {
			//The slot's image was left in a defined layout, and its first use still starts from UNDEFINED
			failed |= slot_backends[frame] != slot_backends[frame - ZEST_MAX_FIF];
			failed |= layouts_left[frame - ZEST_MAX_FIF] == D3D12_BARRIER_LAYOUT_UNDEFINED;
			failed |= layouts_before[frame] != D3D12_BARRIER_LAYOUT_UNDEFINED;
		}
		failed |= zest_GetValidationErrorCount(device) != 0;
		zest_DestroyContext(context);
	}
	failed |= finish_device(device);
	close_test_window(&window);
	return failed;
}

int test_d3d12_transient_images(void) {
	zest_device device = zest_EndDeviceBuilder(BeginD3D12TestBuilder());
	if (!device) return 1;
	zest_create_context_info_t info = zest_CreateContextInfo();
	zest_context context = zest_CreateHeadlessContext(device, &info);
	int failed = context == NULL;
	if (context) {
		zest_frame_graph frame_graph = build_transient_image_graph(context, NULL, ZEST_FALSE);
		failed |= frame_graph == NULL || zest_GetFrameGraphResult(frame_graph) != zest_fgs_success;
		failed |= frame_graph == NULL || zest_FlushFrameGraphAndWait(frame_graph) != zest_semaphore_status_success;
		failed |= zest_GetValidationErrorCount(device) != 0;

		zest_uint cache_state = 1;
		zest_frame_graph_cache_key_t cache_key = zest_InitialiseCacheKey(context, &cache_state, sizeof(cache_state));
		frame_graph = build_transient_image_graph(context, &cache_key, ZEST_FALSE);
		failed |= frame_graph == NULL || zest_GetFrameGraphResult(frame_graph) != zest_fgs_success;
		failed |= frame_graph == NULL || zest_FlushFrameGraphAndWait(frame_graph) != zest_semaphore_status_success;
		failed |= zest_GetCachedFrameGraph(context, &cache_key) != frame_graph;
		failed |= frame_graph == NULL || !third_target_barrier_is_clean(frame_graph);
		failed |= zest_GetValidationErrorCount(device) != 0;
		zest_DestroyContext(context);
	}
	failed |= finish_device(device);
	return failed | run_cached_transient_frames();
}

typedef struct d3d12_stride_capture_t {
	zest_size offset;
	zest_size stride_remainder;
	zest_size view_remainder;
	zest_size size;
	int run_count;
} d3d12_stride_capture_t;

static void d3d12_stride_capture_pass(const zest_command_list command_list, void *user_data) {
	d3d12_stride_capture_t *capture = (d3d12_stride_capture_t *)user_data;
	zest_buffer buffer = zest_GetPassOutputResource(command_list, "Stride Buffer")->storage_buffer;
	capture->run_count++;
	if (!buffer) return;
	capture->offset = buffer->memory_offset;
	capture->stride_remainder = buffer->memory_offset % 76;
	capture->view_remainder = buffer->memory_offset % 16;
	capture->size = buffer->size;
}

//The leading buffer takes the start of the arena, so the stride buffer's raw offset is one that needs padding
int test_d3d12_transient_buffer_stride(void) {
	D3D12TestWindow window;
	if (!open_test_window(&window, "D3D12 Transient Buffer Stride")) {
		close_test_window(&window);
		return 1;
	}
	zest_device device = create_windowed_test_device(&window);
	if (!device) {
		close_test_window(&window);
		return 1;
	}
	zest_create_context_info_t info = zest_CreateContextInfo();
	zest_context context = zest_CreateContext(device, &window.window_data, &info);
	int failed = context == NULL;
	if (context) {
		set_test_clear_color(context);
		d3d12_stride_capture_t capture = {};
		SDL_Event event;
		while (SDL_PollEvent(&event)) {}
		zest_UpdateDevice(device);
		failed |= !zest_BeginFrame(context);
		if (!failed) {
			zest_frame_graph frame_graph = NULL;
			if (zest_BeginFrameGraph(context, "Transient Buffer Stride", 0)) {
				zest_ImportSwapchainResource();
				zest_buffer_resource_info_t leading_info = {};
				leading_info.size = 100;
				zest_buffer_resource_info_t stride_info = {};
				stride_info.size = 760;
				stride_info.element_alignment = 76;
				zest_resource_node leading_buffer = zest_AddTransientBufferResource("Leading Buffer", &leading_info);
				zest_resource_node stride_buffer = zest_AddTransientBufferResource("Stride Buffer", &stride_info);
				zest_FlagResourceAsEssential(leading_buffer);
				zest_FlagResourceAsEssential(stride_buffer);
				zest_BeginRenderPass("Stride Pass");
				zest_ConnectSwapChainOutput();
				zest_ConnectOutput(leading_buffer);
				zest_ConnectOutput(stride_buffer);
				zest_SetPassTask(d3d12_stride_capture_pass, &capture);
				zest_EndPass();
				frame_graph = zest_EndFrameGraph();
			}
			zest_EndFrame(context, frame_graph);
			failed |= frame_graph == NULL || zest_GetFrameGraphResult(frame_graph) != zest_fgs_success;
		}
		failed |= capture.run_count != 1 || capture.offset == 0 || capture.size < 760;
		failed |= capture.stride_remainder != 0 || capture.view_remainder != 0;
		failed |= zest_GetValidationErrorCount(device) != 0;
		zest_DestroyContext(context);
	}
	failed |= finish_device(device);
	close_test_window(&window);
	return failed;
}

#define D3D12_CONCURRENT_THREAD_COUNT 4
#define D3D12_CONCURRENT_ITERATIONS 200
//More than a CPU descriptor block, so the heap grows while other threads allocate
#define D3D12_CONCURRENT_HELD_HANDLES 1100
#define D3D12_HANDLE_TABLE_SIZE 16384
#define D3D12_CHUNK_COUNT (ZEST_DX_TRANSIENT_DESCRIPTOR_COUNT / ZEST_DX_TRANSIENT_DESCRIPTOR_CHUNK)

//Every handle seen gets a slot for good, and a slot's owner is the thread holding that handle, or 0
struct D3D12ConcurrentShared {
	volatile LONG64 handle_keys[D3D12_HANDLE_TABLE_SIZE];
	volatile LONG handle_owners[D3D12_HANDLE_TABLE_SIZE];
	volatile LONG chunk_owners[D3D12_CHUNK_COUNT];
	volatile LONG duplicates;
	volatile LONG failures;
	zest_device device;
	zest_dx_cpu_descriptor_heap_t *heap;
	HANDLE start_event;
};

struct D3D12ConcurrentWorker {
	D3D12ConcurrentShared *shared;
	LONG owner;
	D3D12_CPU_DESCRIPTOR_HANDLE handles[D3D12_CONCURRENT_HELD_HANDLES];
};

static volatile LONG *handle_owner_slot(D3D12ConcurrentShared *shared, SIZE_T handle) {
	zest_uint slot = (zest_uint)(((zest_u64)handle * 0x9E3779B97F4A7C15ull) >> 50);
	for (zest_uint probe = 0; probe != D3D12_HANDLE_TABLE_SIZE; ++probe) {
		LONG64 key = InterlockedCompareExchange64(&shared->handle_keys[slot], (LONG64)handle, 0);
		if (key == 0 || key == (LONG64)handle) return &shared->handle_owners[slot];
		slot = (slot + 1) & (D3D12_HANDLE_TABLE_SIZE - 1);
	}
	return NULL;
}

static void claim_owner(D3D12ConcurrentShared *shared, volatile LONG *owner_slot, LONG owner) {
	if (!owner_slot) {
		InterlockedIncrement(&shared->failures);
	} else if (InterlockedCompareExchange(owner_slot, owner, 0) != 0) {
		InterlockedIncrement(&shared->duplicates);
	}
}

static DWORD WINAPI concurrent_descriptor_worker(void *parameter) {
	D3D12ConcurrentWorker *worker = (D3D12ConcurrentWorker *)parameter;
	D3D12ConcurrentShared *shared = worker->shared;
	zest_dx_transient_descriptors_t descriptors = {};
	D3D12_CPU_DESCRIPTOR_HANDLE cpu = {};
	D3D12_GPU_DESCRIPTOR_HANDLE gpu = {};
	WaitForSingleObject(shared->start_event, INFINITE);
	for (int iteration = 0; iteration != D3D12_CONCURRENT_ITERATIONS; ++iteration) {
		for (int index = 0; index != D3D12_CONCURRENT_HELD_HANDLES; ++index) {
			worker->handles[index] = zest__dx_allocate_cpu_descriptor(shared->device, shared->heap);
			claim_owner(shared, worker->handles[index].ptr ? handle_owner_slot(shared, worker->handles[index].ptr) : NULL, worker->owner);
		}
		for (int slot = 0; slot != 2 * ZEST_DX_TRANSIENT_DESCRIPTOR_CHUNK; ++slot) {
			if (!zest__dx_allocate_transient_descriptor(shared->device, &descriptors, &cpu, &gpu)) InterlockedIncrement(&shared->failures);
		}
		zest_vec_foreach(chunk_index, descriptors.chunks) {
			zest_uint chunk = descriptors.chunks[chunk_index];
			claim_owner(shared, chunk < D3D12_CHUNK_COUNT ? &shared->chunk_owners[chunk] : NULL, worker->owner);
		}
		zest_vec_foreach(chunk_index, descriptors.chunks) {
			zest_uint chunk = descriptors.chunks[chunk_index];
			if (chunk < D3D12_CHUNK_COUNT) InterlockedCompareExchange(&shared->chunk_owners[chunk], 0, worker->owner);
		}
		zest__dx_release_transient_descriptors(shared->device, &descriptors);
		for (int index = 0; index != D3D12_CONCURRENT_HELD_HANDLES; ++index) {
			if (!worker->handles[index].ptr) continue;
			volatile LONG *owner_slot = handle_owner_slot(shared, worker->handles[index].ptr);
			if (owner_slot) InterlockedCompareExchange(owner_slot, 0, worker->owner);
			zest__dx_free_cpu_descriptor(shared->device, shared->heap, worker->handles[index]);
		}
	}
	return 0;
}

static zest_uint live_cpu_descriptors(const zest_dx_cpu_descriptor_heap_t *heap) {
	zest_uint block_count = zest_vec_size(heap->blocks);
	if (!block_count) return 0;
	return (block_count - 1) * ZEST_DX_CPU_DESCRIPTOR_BLOCK_SIZE + heap->next_in_block - zest_vec_size(heap->free_handles);
}

//Threads that grow and share one CPU descriptor heap and the transient chunk free list never get the same handle or chunk while it's live
int test_d3d12_concurrent_descriptor_allocation(void) {
	zest_device device = zest_EndDeviceBuilder(BeginD3D12TestBuilder());
	if (!device) return 1;
	zest_dx_descriptor_pool_backend_t *pool = ZEST_DX_BACKEND(descriptor_set, device->bindless_set)->pool;
	D3D12ConcurrentShared *shared = (D3D12ConcurrentShared *)calloc(1, sizeof(D3D12ConcurrentShared));
	D3D12ConcurrentWorker *workers = (D3D12ConcurrentWorker *)calloc(D3D12_CONCURRENT_THREAD_COUNT, sizeof(D3D12ConcurrentWorker));
	int failed = shared == NULL || workers == NULL;
	if (!failed) {
		shared->device = device;
		shared->heap = &ZEST_DX_BACKEND(device, device)->cpu_rtv_heap;
		shared->start_event = CreateEventA(NULL, TRUE, FALSE, NULL);
		zest_uint blocks_before = zest_vec_size(shared->heap->blocks);
		zest_uint live_before = live_cpu_descriptors(shared->heap);
		zest_uint free_chunks_before = zest_vec_size(pool->free_transient_chunks);
		HANDLE threads[D3D12_CONCURRENT_THREAD_COUNT] = {};
		DWORD thread_count = 0;
		failed |= shared->start_event == NULL;
		for (int index = 0; index != D3D12_CONCURRENT_THREAD_COUNT && !failed; ++index) {
			workers[index].shared = shared;
			workers[index].owner = index + 1;
			threads[thread_count] = CreateThread(NULL, 0, concurrent_descriptor_worker, &workers[index], 0, NULL);
			failed |= threads[thread_count] == NULL;
			thread_count += threads[thread_count] != NULL;
		}
		if (shared->start_event) SetEvent(shared->start_event);
		if (thread_count) WaitForMultipleObjects(thread_count, threads, TRUE, INFINITE);
		for (DWORD index = 0; index != thread_count; ++index) CloseHandle(threads[index]);
		if (shared->start_event) CloseHandle(shared->start_event);
		failed |= shared->duplicates != 0 || shared->failures != 0;
		failed |= zest_vec_size(shared->heap->blocks) <= blocks_before;
		failed |= live_cpu_descriptors(shared->heap) != live_before;
		failed |= zest_vec_size(pool->free_transient_chunks) != free_chunks_before;
		//Every owner is back to 0, so a free list entry that's there twice can't claim its slot the second time
		zest_vec_foreach(index, shared->heap->free_handles) {
			claim_owner(shared, handle_owner_slot(shared, shared->heap->free_handles[index].ptr), 1);
		}
		zest_vec_foreach(index, pool->free_transient_chunks) {
			zest_uint chunk = pool->free_transient_chunks[index];
			claim_owner(shared, chunk < D3D12_CHUNK_COUNT ? &shared->chunk_owners[chunk] : NULL, 1);
		}
		failed |= shared->duplicates != 0 || shared->failures != 0;
	}
	free(workers);
	free(shared);
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
	{ "D3D12 Stride Aligned Allocation", test_d3d12_stride_aligned_allocation },
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
	{ "D3D12 Image Barrier Builder", test_d3d12_image_barrier_builder },
	{ "D3D12 Attachment Views", test_d3d12_attachment_views },
	{ "D3D12 Depth Deny SRV", test_d3d12_depth_deny_srv },
	{ "D3D12 Format Support", test_d3d12_format_support },
	{ "D3D12 3D Upload Slice Pitch", test_d3d12_3d_upload_slice_pitch },
	{ "D3D12 Validation Device Conflict", test_d3d12_validation_device_conflict },
	{ "D3D12 Recovers From Failed Close", test_d3d12_recovers_from_failed_close },
	{ "D3D12 Begin Waits For Previous Submission", test_d3d12_begin_waits_for_previous_submission },
	{ "D3D12 Feature Level Fallback", test_d3d12_feature_level_fallback },
	{ "D3D12 Validation Without Debug Layer", test_d3d12_validation_without_debug_layer },
	{ "D3D12 Recovers From Failed Signal", test_d3d12_recovers_from_failed_signal },
	{ "D3D12 SDL Helpers", test_d3d12_sdl_helpers },
	{ "D3D12 Device Reset", test_d3d12_device_reset },
	{ "D3D12 Failed Reset Teardown", test_d3d12_failed_reset_teardown },
	{ "D3D12 Render Pass Access", test_d3d12_render_pass_access },
	{ "D3D12 Viewport Flip", test_d3d12_viewport_flip },
	{ "D3D12 Empty Command Graph", test_d3d12_empty_command_graph },
	{ "D3D12 Render Pass Clear", test_d3d12_render_pass_clear },
	{ "D3D12 Failed Batch Signal Drains", test_d3d12_failed_batch_signal_drains },
	{ "D3D12 Swapchain Formats", test_d3d12_swapchain_formats },
	{ "D3D12 Present Parameters", test_d3d12_present_parameters },
	{ "D3D12 Windowed Frames", test_d3d12_windowed_frames },
	{ "D3D12 DXGI Messages Counted", test_d3d12_dxgi_messages_counted },
	{ "D3D12 Per Mip Views", test_d3d12_per_mip_views },
	{ "D3D12 HLSL Compile", test_d3d12_hlsl_compile },
	{ "D3D12 HLSL Compile Error", test_d3d12_hlsl_compile_error },
	{ "D3D12 DXC Missing", test_d3d12_dxc_missing },
	{ "D3D12 Viewport Orientation", test_d3d12_viewport_orientation },
	{ "D3D12 Buffer Copies", test_d3d12_buffer_copies },
	{ "D3D12 Buffer Descriptors", test_d3d12_buffer_descriptors },
	{ "D3D12 Frame Graph Buffer Copies", test_d3d12_frame_graph_buffer_copies },
	{ "D3D12 Image Resource Flags", test_d3d12_image_resource_flags },
	{ "D3D12 Discard Follows Resource Flags", test_d3d12_discard_follows_resource_flags },
	{ "D3D12 Transient Descriptor Chunks", test_d3d12_transient_descriptor_chunks },
	{ "D3D12 Generate Mipmaps", test_d3d12_generate_mipmaps },
	{ "D3D12 Reset After Blit", test_d3d12_reset_after_blit },
	{ "D3D12 Blit Image", test_d3d12_blit_image },
	{ "D3D12 Image Mip Commands", test_d3d12_image_mip_commands },
	{ "D3D12 Clear Colour Image", test_d3d12_clear_colour_image },
	{ "D3D12 Clear Depth Image", test_d3d12_clear_depth_image },
	{ "D3D12 Image Clear Command", test_d3d12_image_clear_command },
	{ "D3D12 Resolve Image", test_d3d12_resolve_image },
	{ "D3D12 Transient First Use Layout", test_d3d12_transient_first_use_layout },
	{ "D3D12 Arena Backing", test_d3d12_arena_backing },
	{ "D3D12 Transient Images", test_d3d12_transient_images },
	{ "D3D12 Transient Buffer Stride", test_d3d12_transient_buffer_stride },
	{ "D3D12 Concurrent Descriptor Allocation", test_d3d12_concurrent_descriptor_allocation },
	//Must stay last, see the test
	{ "D3D12 Device Removed Mid Frame", test_d3d12_device_removed_mid_frame },
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
