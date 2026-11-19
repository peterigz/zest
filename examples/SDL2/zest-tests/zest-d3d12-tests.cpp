#include <dxcapi.h>
#include <d3d12shader.h>
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
	CHECK_INTERFACE_ID(ID3D12GraphicsCommandList9);
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
	CHECK_INTERFACE_ID(ID3D12CommandSignature);
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
	failed |= !zest__dx_compile_hlsl(device, hlsl_vertex_source, (zest_size)strlen(hlsl_vertex_source), "test_vertex", "main", "vs_6_0", NULL, 0, NULL, &vertex_dxil);
	failed |= check_dxil_container(vertex_dxil);
	zest_dxc_compiler3_t *compiler = ZEST_DX_BACKEND(device, device)->dxc_compiler;
	failed |= compiler == NULL;
	failed |= !zest__dx_compile_hlsl(device, hlsl_pixel_source, (zest_size)strlen(hlsl_pixel_source), "test_pixel", "main", "ps_6_0", NULL, 0, NULL, &pixel_dxil);
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
	zest_text_t error_text = {};
	failed |= zest__dx_compile_hlsl(device, broken_source, (zest_size)strlen(broken_source), "broken", "main", "ps_6_0", NULL, 0, &error_text, &dxil) != ZEST_FALSE;
	failed |= dxil != NULL;
	failed |= zest_TextLength(&error_text) == 0;
	zest_FreeText(device->allocator, &error_text);
	failed |= !zest__dx_compile_hlsl(device, hlsl_pixel_source, (zest_size)strlen(hlsl_pixel_source), "test_pixel", "main", "ps_6_0", NULL, 0, NULL, &dxil);
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
	zest_text_t error_text = {};
	failed |= zest__dx_compile_hlsl(device, hlsl_pixel_source, (zest_size)strlen(hlsl_pixel_source), "test_pixel", "main", "ps_6_0", NULL, 0, &error_text, &dxil) != ZEST_FALSE;
	zest__dx_forced_dxc_library = NULL;
	failed |= dxil != NULL;
	failed |= zest_TextLength(&error_text) == 0 || strcmp(error_text.str, "Shader compiler unavailable") != 0;
	zest_FreeText(device->allocator, &error_text);
	failed |= zest_GetValidationErrorCount(device) != error_count + 1;
	failed |= ZEST_DX_BACKEND(device, device)->dxc_compiler != NULL;
	failed |= !zest__dx_compile_hlsl(device, hlsl_pixel_source, (zest_size)strlen(hlsl_pixel_source), "test_pixel", "main", "ps_6_0", NULL, 0, NULL, &dxil);
	failed |= check_dxil_container(dxil);
	ZEST_DX_RELEASE(dxil);
	zest_ResetValidationErrors(device);
	return failed | finish_device(device);
}

#define D3D12_HLSL_SHADER_PATH "examples/SDL2/zest-tests/shaders/hlsl/"

struct D3D12TestShaderFile {
	const char *file_name;
	const char *profile;
};

static const D3D12TestShaderFile d3d12_test_shader_files[] = {
	{ "buffer_write.comp.hlsl", "cs_6_0" },
	{ "buffer_verify.comp.hlsl", "cs_6_0" },
	{ "image_verify.comp.hlsl", "cs_6_0" },
	{ "image_write2.comp.hlsl", "cs_6_0" },
	{ "stride_check.comp.hlsl", "cs_6_0" },
	{ "genbrdflut.comp.hlsl", "cs_6_0" },
	{ "instance.vert.hlsl", "vs_6_0" },
	{ "instance.frag.hlsl", "ps_6_0" },
	{ "vertex.vert.hlsl", "vs_6_0" },
	{ "vertex.frag.hlsl", "ps_6_0" },
};
#define D3D12_TEST_SHADER_FILE_COUNT (sizeof(d3d12_test_shader_files) / sizeof(d3d12_test_shader_files[0]))

static ID3DBlob *compile_test_hlsl(zest_device device, const char *source, const char *name, const char *profile, const char **arguments, zest_uint argument_count) {
	ID3DBlob *dxil = NULL;
	zest__dx_compile_hlsl(device, source, (zest_size)strlen(source), name, "main", profile, arguments, argument_count, NULL, &dxil);
	return dxil;
}

static ID3DBlob *compile_test_shader_file(zest_device device, const D3D12TestShaderFile *shader_file) {
	char path[256];
	snprintf(path, sizeof(path), D3D12_HLSL_SHADER_PATH "%s", shader_file->file_name);
	zest_file source = zest_ReadEntireFile(device, path, ZEST_TRUE);
	if (!source) {
		ZEST_PRINT("\tCouldn't read %s. Run the tests from the repository root.", path);
		return NULL;
	}
	ID3DBlob *dxil = compile_test_hlsl(device, source, shader_file->file_name, shader_file->profile, NULL, 0);
	zest_FreeFile(device, source);
	return dxil;
}

static zest_bool reflect_blob(ID3DBlob *dxil, zest_dx_shader_reflection_t *out_reflection, const char *name) {
	if (!dxil) return ZEST_FALSE;
	const char *error = NULL;
	if (!zest__dx_reflect_shader(ZEST_DX_CALL0(dxil, GetBufferPointer), ZEST_DX_CALL0(dxil, GetBufferSize), out_reflection, &error)) {
		ZEST_PRINT("\t%s: %s", name, error);
		return ZEST_FALSE;
	}
	return ZEST_TRUE;
}

static const char *reflection_vertex_source =
	"cbuffer zest_push : register(b0, space0) { float4 offset; };\n"
	"struct vertex_input {\n"
	"	uint instance : SV_InstanceID;\n"
	"	float3 position : POSITION0;\n"
	"	float2 uv : TEXCOORD0;\n"
	"	float4 color : COLOR1;\n"
	"};\n"
	"float4 main(vertex_input input) : SV_Position {\n"
	"	return float4(input.position + offset.xyz, 1.0) + float4(input.uv, 0.0, 0.0) * input.color + float(input.instance);\n"
	"}\n";

//One unbounded array for each row of the slot table, every one used, at space 100 + its row
static const char *reflection_slot_pixel_source =
	"SamplerState samplers[] : register(s0, space100);\n"
	"Texture2D textures_2d[] : register(t0, space101);\n"
	"TextureCube textures_cube[] : register(t0, space102);\n"
	"Texture2DArray textures_array[] : register(t0, space103);\n"
	"Texture3D textures_3d[] : register(t0, space104);\n"
	"TextureCubeArray textures_cube_array[] : register(t0, space105);\n"
	"StructuredBuffer<float4> structured_buffers[] : register(t0, space106);\n"
	"ByteAddressBuffer raw_buffers[] : register(t0, space107);\n"
	"RWStructuredBuffer<float4> read_write_structured_buffers[] : register(u0, space108);\n"
	"RWByteAddressBuffer read_write_raw_buffers[] : register(u0, space109);\n"
	"RWTexture2D<float4> storage_images[] : register(u0, space110);\n"
	"struct uniform_t { float4 value; };\n"
	"ConstantBuffer<uniform_t> uniforms[] : register(b0, space111);\n"
	"float4 main(float4 position : SV_Position, nointerpolation uint index : TEXCOORD0) : SV_Target0 {\n"
	"	SamplerState chosen_sampler = samplers[NonUniformResourceIndex(index)];\n"
	"	float4 sum = textures_2d[NonUniformResourceIndex(index)].Sample(chosen_sampler, position.xy);\n"
	"	sum += textures_cube[NonUniformResourceIndex(index)].Sample(chosen_sampler, position.xyz);\n"
	"	sum += textures_array[NonUniformResourceIndex(index)].Sample(chosen_sampler, position.xyz);\n"
	"	sum += textures_3d[NonUniformResourceIndex(index)].Sample(chosen_sampler, position.xyz);\n"
	"	sum += textures_cube_array[NonUniformResourceIndex(index)].Sample(chosen_sampler, position);\n"
	"	sum += structured_buffers[NonUniformResourceIndex(index)][0];\n"
	"	sum += asfloat(raw_buffers[NonUniformResourceIndex(index)].Load4(0));\n"
	"	sum += uniforms[NonUniformResourceIndex(index)].value;\n"
	"	read_write_structured_buffers[NonUniformResourceIndex(index)][0] = sum;\n"
	"	read_write_raw_buffers[NonUniformResourceIndex(index)].Store4(0, asuint(sum));\n"
	"	storage_images[NonUniformResourceIndex(index)][uint2(position.xy)] = sum;\n"
	"	return sum;\n"
	"}\n";

//PSV0 (ResType, ResKind) for each declaration of reflection_slot_pixel_source, in its order
static const zest_uint reflection_slot_types[12][2] = {
	{ 1, 14 }, { 3, 2 }, { 3, 5 }, { 3, 7 }, { 3, 4 }, { 3, 9 }, { 5, 12 }, { 4, 11 }, { 8, 12 }, { 7, 11 }, { 6, 2 }, { 2, 13 },
};

static const char *reflection_compute_source =
	"cbuffer zest_push : register(b0, space0) { uint buffer_index; uint value; };\n"
	"RWByteAddressBuffer zest_buffers[] : register(u0, space105);\n"
	"[numthreads(64, 1, 1)]\n"
	"void main(uint3 thread_id : SV_DispatchThreadID) {\n"
	"	zest_buffers[NonUniformResourceIndex(buffer_index)].Store(thread_id.x * 4, value);\n"
	"}\n";

int test_d3d12_shader_reflection(void) {
	zest_device device = zest_EndDeviceBuilder(BeginD3D12TestBuilder());
	if (!device) return 1;
	ID3DBlob *vertex_dxil = compile_test_hlsl(device, reflection_vertex_source, "reflection_vertex", "vs_6_0", NULL, 0);
	ID3DBlob *pixel_dxil = compile_test_hlsl(device, reflection_slot_pixel_source, "reflection_pixel", "ps_6_0", NULL, 0);
	ID3DBlob *compute_dxil = compile_test_hlsl(device, reflection_compute_source, "reflection_compute", "cs_6_0", NULL, 0);
	zest_dx_shader_reflection_t reflection;
	int failed = 0;

	if (reflect_blob(vertex_dxil, &reflection, "reflection_vertex")) {
		failed |= reflection.stage != 1 || reflection.input_count != 4;
		failed |= reflection.inputs[0].system_value != 8;
		failed |= strcmp(reflection.inputs[1].semantic_name, "POSITION") != 0 || reflection.inputs[1].semantic_index != 0 || reflection.inputs[1].register_index != 1 || reflection.inputs[1].system_value != 0;
		failed |= strcmp(reflection.inputs[3].semantic_name, "COLOR") != 0 || reflection.inputs[3].semantic_index != 1;
		failed |= reflection.resource_count != 1;
		zest_dx_shader_resource_t *push = &reflection.resources[0];
		failed |= push->type != 2 || push->kind != 13 || push->space != 0 || push->lower_bound != 0 || push->upper_bound != 0;
	} else {
		failed = 1;
	}

	if (reflect_blob(pixel_dxil, &reflection, "reflection_pixel")) {
		failed |= reflection.stage != 0 || reflection.resource_count != 12;
		for (zest_uint slot = 0; slot != 12; ++slot) {
			zest_bool found = ZEST_FALSE;
			for (zest_uint resource_index = 0; resource_index != reflection.resource_count; ++resource_index) {
				zest_dx_shader_resource_t *resource = &reflection.resources[resource_index];
				if (resource->space != 100 + slot) continue;
				found = resource->type == reflection_slot_types[slot][0] && resource->kind == reflection_slot_types[slot][1] && resource->lower_bound == 0 && resource->upper_bound == 0xFFFFFFFF;
			}
			if (!found) {
				ZEST_PRINT("\tSlot %u in space %u wasn't reflected as type %u kind %u", slot, 100 + slot, reflection_slot_types[slot][0], reflection_slot_types[slot][1]);
				failed = 1;
			}
		}
	} else {
		failed = 1;
	}

	if (reflect_blob(compute_dxil, &reflection, "reflection_compute")) {
		failed |= reflection.stage != 5 || reflection.input_count != 0 || reflection.resource_count != 2;
	} else {
		failed = 1;
	}
	ZEST_DX_RELEASE(vertex_dxil);
	ZEST_DX_RELEASE(pixel_dxil);
	ZEST_DX_RELEASE(compute_dxil);
	return failed | finish_device(device);
}

//The reflection Type a PSV0 ResType becomes, indexed by ResType
static const D3D_SHADER_INPUT_TYPE psv_type_to_input_type[10] = {
	D3D_SIT_CBUFFER, D3D_SIT_SAMPLER, D3D_SIT_CBUFFER, D3D_SIT_TEXTURE, D3D_SIT_BYTEADDRESS, D3D_SIT_STRUCTURED,
	D3D_SIT_UAV_RWTYPED, D3D_SIT_UAV_RWBYTEADDRESS, D3D_SIT_UAV_RWSTRUCTURED, D3D_SIT_UAV_RWSTRUCTURED_WITH_COUNTER,
};

//The reflection Dimension a PSV0 ResKind becomes, indexed by ResKind up to Sampler
static const D3D_SRV_DIMENSION psv_kind_to_dimension[15] = {
	D3D_SRV_DIMENSION_UNKNOWN, D3D_SRV_DIMENSION_TEXTURE1D, D3D_SRV_DIMENSION_TEXTURE2D, D3D_SRV_DIMENSION_TEXTURE2DMS, D3D_SRV_DIMENSION_TEXTURE3D,
	D3D_SRV_DIMENSION_TEXTURECUBE, D3D_SRV_DIMENSION_TEXTURE1DARRAY, D3D_SRV_DIMENSION_TEXTURE2DARRAY, D3D_SRV_DIMENSION_TEXTURE2DMSARRAY,
	D3D_SRV_DIMENSION_TEXTURECUBEARRAY, D3D_SRV_DIMENSION_BUFFER, D3D_SRV_DIMENSION_BUFFER, D3D_SRV_DIMENSION_BUFFER,
	D3D_SRV_DIMENSION_UNKNOWN, D3D_SRV_DIMENSION_UNKNOWN,
};

static zest_bool resource_matches_binding(const zest_dx_shader_resource_t *resource, const D3D12_SHADER_INPUT_BIND_DESC *binding) {
	if (resource->type == 0 || resource->type >= 10 || resource->kind >= 15) return ZEST_FALSE;
	//An unbounded CBV array reports UINT_MAX and every other unbounded array 0
	zest_bool count_matches = resource->upper_bound == 0xFFFFFFFF ? (binding->BindCount == 0 || binding->BindCount == UINT_MAX) : binding->BindCount == resource->upper_bound - resource->lower_bound + 1;
	return binding->Type == psv_type_to_input_type[resource->type] && binding->Dimension == psv_kind_to_dimension[resource->kind] && binding->Space == resource->space && binding->BindPoint == resource->lower_bound && count_matches;
}

static int cross_check_reflection(IDxcUtils *utils, const void *data, size_t size, const char *name) {
	zest_dx_shader_reflection_t parsed;
	const char *error = NULL;
	if (!zest__dx_reflect_shader(data, size, &parsed, &error)) {
		ZEST_PRINT("\t%s: %s", name, error);
		return 1;
	}
	DxcBuffer buffer = { data, size, 0 };
	ID3D12ShaderReflection *reflection = NULL;
	if (FAILED(utils->CreateReflection(&buffer, IID_PPV_ARGS(&reflection))) || !reflection) {
		ZEST_PRINT("\t%s: IDxcUtils::CreateReflection failed", name);
		return 1;
	}
	D3D12_SHADER_DESC description;
	reflection->GetDesc(&description);
	int failed = 0;
	if (description.BoundResources != parsed.resource_count) {
		ZEST_PRINT("\t%s: %u resources parsed, %u reflected", name, parsed.resource_count, description.BoundResources);
		failed = 1;
	} else {
		zest_bool used[ZEST_DX_MAX_SHADER_RESOURCES] = {};
		for (zest_uint resource_index = 0; resource_index != parsed.resource_count; ++resource_index) {
			const zest_dx_shader_resource_t *resource = &parsed.resources[resource_index];
			zest_bool found = ZEST_FALSE;
			for (zest_uint binding_index = 0; binding_index != description.BoundResources && !found; ++binding_index) {
				D3D12_SHADER_INPUT_BIND_DESC binding;
				reflection->GetResourceBindingDesc(binding_index, &binding);
				if (!used[binding_index] && resource_matches_binding(resource, &binding)) {
					used[binding_index] = ZEST_TRUE;
					found = ZEST_TRUE;
				}
			}
			if (!found) {
				ZEST_PRINT("\t%s: no reflected binding matches type %u kind %u space %u bounds %u to %u", name, resource->type, resource->kind, resource->space, resource->lower_bound, resource->upper_bound);
				failed = 1;
			}
		}
	}
	if (description.InputParameters != parsed.input_count) {
		ZEST_PRINT("\t%s: %u inputs parsed, %u reflected", name, parsed.input_count, description.InputParameters);
		failed = 1;
	} else {
		//ISG1 is in register order and reflection in declaration order, which differ where pixel inputs pack into one register
		zest_bool used[ZEST_DX_MAX_SHADER_INPUTS] = {};
		for (zest_uint input_index = 0; input_index != parsed.input_count; ++input_index) {
			const zest_dx_shader_input_t *input = &parsed.inputs[input_index];
			zest_bool found = ZEST_FALSE;
			for (zest_uint parameter_index = 0; parameter_index != description.InputParameters && !found; ++parameter_index) {
				D3D12_SIGNATURE_PARAMETER_DESC parameter;
				reflection->GetInputParameterDesc(parameter_index, &parameter);
				//Reflection upper-cases system value names, so those compare by number
				zest_bool name_matches = input->system_value != 0 || strcmp(input->semantic_name, parameter.SemanticName) == 0;
				if (used[parameter_index] || !name_matches || parameter.SemanticIndex != input->semantic_index || (zest_uint)parameter.SystemValueType != input->system_value) continue;
				used[parameter_index] = ZEST_TRUE;
				found = ZEST_TRUE;
				if (parameter.Register != input->register_index || (zest_uint)parameter.ComponentType != input->component_type || parameter.Mask != input->mask) {
					ZEST_PRINT("\t%s: input %s%u is register %u mask %u type %u, reflected as register %u mask %u type %u", name, input->semantic_name, input->semantic_index, input->register_index, input->mask, input->component_type, parameter.Register, (zest_uint)parameter.Mask, (zest_uint)parameter.ComponentType);
					failed = 1;
				}
			}
			if (!found) {
				ZEST_PRINT("\t%s: input %s%u isn't reflected", name, input->semantic_name, input->semantic_index);
				failed = 1;
			}
		}
	}
	reflection->Release();
	return failed;
}

#ifdef ZEST_ENABLE_SLANG
struct D3D12SlangEntry {
	const char *module;
	const char *entry_point;
	zest_uint stage;
};

//Every entry point of timelinefx.slang in its default configuration, then particle.slang's compute stage, as its vertex stage writes SV_PointSize
static const D3D12SlangEntry d3d12_slang_entries[] = {
	{ "timelinefx", "simulate_particles", 5 },
	{ "timelinefx", "tfx_ribbon_compute", 5 },
	{ "timelinefx", "tfx_sprite_playback", 5 },
	{ "timelinefx", "tfx_ribbon_playback", 5 },
	{ "timelinefx", "tfx_sprite_bounds", 5 },
	{ "timelinefx", "tfx_ribbon_bounds", 5 },
	{ "timelinefx", "tfx_billboard_vertex", 1 },
	{ "timelinefx", "tfx_billboard_lit_vertex", 1 },
	{ "timelinefx", "tfx_billboard_baked_vertex", 1 },
	{ "timelinefx", "tfx_billboard_baked_lit_vertex", 1 },
	{ "timelinefx", "tfx_ribbon_vertex", 1 },
	{ "timelinefx", "tfx_ribbon_lit_vertex", 1 },
	{ "timelinefx", "tfx_billboard_fragment", 0 },
	{ "timelinefx", "tfx_billboard_cutout_fragment", 0 },
	{ "timelinefx", "tfx_billboard_lit_fragment", 0 },
	{ "timelinefx", "tfx_billboard_lit_cutout_fragment", 0 },
	{ "timelinefx", "tfx_ribbon_fragment", 0 },
	{ "timelinefx", "tfx_ribbon_cutout_fragment", 0 },
	{ "timelinefx", "tfx_ribbon_lit_fragment", 0 },
	{ "timelinefx", "tfx_ribbon_lit_cutout_fragment", 0 },
	{ "particle", "computeMain", 5 },
};
#define D3D12_SLANG_ENTRY_COUNT (sizeof(d3d12_slang_entries) / sizeof(d3d12_slang_entries[0]))

static zest_shader_type d3d12_slang_entry_type(zest_uint stage) {
	switch (stage) {
	case 0: return zest_fragment_shader;
	case 1: return zest_vertex_shader;
	default: return zest_compute_shader;
	}
}

//Compiles the entries above to DXIL through impl_slang. A failed entry is left empty. Free the blobs with release_slang_dxil_set
static int compile_slang_dxil_set(zest_device device, zest_slang_blob_t *out_blobs) {
	memset(out_blobs, 0, sizeof(zest_slang_blob_t) * D3D12_SLANG_ENTRY_COUNT);
	if (!device->slang_info) {
		zest_slang_InitialiseSession(device);
	}
	const char *search_paths[2] = { "submodules/timelinefxlib/Shaders", "examples/SDL2/zest-compute-example/shaders" };
	zest_slang_session_info_t info = zest_slang_DefaultSessionInfo();
	info.search_paths = search_paths;
	info.search_path_count = 2;
	zest_slang_session session = zest_slang_CreateSession(device, &info);
	if (!session) {
		ZEST_PRINT("\tNo Slang session: %s", zest_slang_GetCreateSessionError(device));
		return 1;
	}
	int failed = 0;
	for (zest_uint entry_index = 0; entry_index != D3D12_SLANG_ENTRY_COUNT; ++entry_index) {
		const D3D12SlangEntry *entry = &d3d12_slang_entries[entry_index];
		zest_slang_entry_point_info_t entry_info = {};
		entry_info.module = entry->module;
		entry_info.entry_point = entry->entry_point;
		entry_info.type = d3d12_slang_entry_type(entry->stage);
		if (!zest_slang_CompileToBinary(session, &entry_info, &out_blobs[entry_index])) {
			ZEST_PRINT("\tSlang couldn't compile %s to DXIL: %s", entry->entry_point, zest_slang_GetLastError(session));
			failed = 1;
		}
	}
	zest_slang_FreeSession(session);
	return failed;
}

static void release_slang_dxil_set(zest_device device, zest_slang_blob_t *blobs) {
	for (zest_uint entry_index = 0; entry_index != D3D12_SLANG_ENTRY_COUNT; ++entry_index) {
		zest_slang_FreeBlob(&blobs[entry_index]);
	}
	zest_slang_Shutdown(device);
}
#endif

int test_d3d12_shader_reflection_cross_check(void) {
	zest_device device = zest_EndDeviceBuilder(BeginD3D12TestBuilder());
	if (!device) return 1;
	int failed = 0;
	zest_uint checked_count = 0;
	ID3DBlob *blit_vertex = NULL;
	ID3DBlob *blit_pixel = NULL;
	zest_size blit_length = (zest_size)strlen(zest__dx_blit_shader);
	failed |= !zest__dx_compile_hlsl(device, zest__dx_blit_shader, blit_length, "zest_blit_vertex", "vertex_main", "vs_6_0", NULL, 0, NULL, &blit_vertex);
	failed |= !zest__dx_compile_hlsl(device, zest__dx_blit_shader, blit_length, "zest_blit_pixel", "pixel_main", "ps_6_0", NULL, 0, NULL, &blit_pixel);
	DxcCreateInstanceProc create_instance = (DxcCreateInstanceProc)(void *)GetProcAddress(ZEST_DX_BACKEND(device, device)->dxc_module, "DxcCreateInstance");
	IDxcUtils *utils = NULL;
	if (!create_instance || FAILED(create_instance(CLSID_DxcUtils, IID_PPV_ARGS(&utils))) || !utils) {
		ZEST_DX_RELEASE(blit_vertex);
		ZEST_DX_RELEASE(blit_pixel);
		finish_device(device);
		return 1;
	}
	for (zest_uint file_index = 0; file_index != D3D12_TEST_SHADER_FILE_COUNT; ++file_index) {
		ID3DBlob *dxil = compile_test_shader_file(device, &d3d12_test_shader_files[file_index]);
		failed |= !dxil || cross_check_reflection(utils, ZEST_DX_CALL0(dxil, GetBufferPointer), ZEST_DX_CALL0(dxil, GetBufferSize), d3d12_test_shader_files[file_index].file_name);
		checked_count += dxil != NULL;
		ZEST_DX_RELEASE(dxil);
	}
	ID3DBlob *blit_blobs[2] = { blit_vertex, blit_pixel };
	for (zest_uint blit_index = 0; blit_index != 2; ++blit_index) {
		failed |= !blit_blobs[blit_index] || cross_check_reflection(utils, ZEST_DX_CALL0(blit_blobs[blit_index], GetBufferPointer), ZEST_DX_CALL0(blit_blobs[blit_index], GetBufferSize), blit_index ? "zest_blit_pixel" : "zest_blit_vertex");
		checked_count += blit_blobs[blit_index] != NULL;
	}
	ZEST_DX_RELEASE(blit_vertex);
	ZEST_DX_RELEASE(blit_pixel);
#ifdef ZEST_ENABLE_SLANG
	zest_slang_blob_t slang_blobs[D3D12_SLANG_ENTRY_COUNT];
	failed |= compile_slang_dxil_set(device, slang_blobs);
	for (zest_uint entry_index = 0; entry_index != D3D12_SLANG_ENTRY_COUNT; ++entry_index) {
		zest_slang_blob_t *blob = &slang_blobs[entry_index];
		const D3D12SlangEntry *entry = &d3d12_slang_entries[entry_index];
		if (!blob->data) continue;
		zest_dx_shader_reflection_t reflection;
		const char *error = NULL;
		failed |= cross_check_reflection(utils, blob->data, (size_t)blob->size, entry->entry_point);
		if (zest__dx_reflect_shader(blob->data, blob->size, &reflection, &error) && reflection.stage != entry->stage) {
			ZEST_PRINT("\t%s: stage %u, expected %u", entry->entry_point, reflection.stage, entry->stage);
			failed = 1;
		}
		checked_count++;
	}
	release_slang_dxil_set(device, slang_blobs);
#else
	ZEST_PRINT("\tThe Slang set was skipped, as ZEST_ENABLE_SLANG isn't defined");
#endif
	ZEST_PRINT("\t%u shaders cross checked", checked_count);
	utils->Release();
	return failed | finish_device(device);
}

//Places the bytes so they end where a no access page begins, so any read past them faults
struct GuardedBuffer {
	unsigned char *allocation;
	unsigned char *guard;
};

static GuardedBuffer create_guarded_buffer(size_t capacity) {
	SYSTEM_INFO system_info;
	GetSystemInfo(&system_info);
	size_t page_size = system_info.dwPageSize;
	size_t usable_size = (capacity + page_size - 1) / page_size * page_size;
	GuardedBuffer buffer = {};
	buffer.allocation = (unsigned char *)VirtualAlloc(NULL, usable_size + page_size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
	if (!buffer.allocation) return buffer;
	DWORD old_protection = 0;
	VirtualProtect(buffer.allocation + usable_size, page_size, PAGE_NOACCESS, &old_protection);
	buffer.guard = buffer.allocation + usable_size;
	return buffer;
}

static unsigned char *guarded_copy(GuardedBuffer *buffer, const void *data, size_t size) {
	unsigned char *destination = buffer->guard - size;
	memcpy(destination, data, size);
	return destination;
}

int test_d3d12_shader_reflection_bad_containers(void) {
	zest_device device = zest_EndDeviceBuilder(BeginD3D12TestBuilder());
	if (!device) return 1;
	int failed = 0;
	zest_dx_shader_reflection_t reflection;
	const char *error = NULL;
	ID3DBlob *dxil = compile_test_hlsl(device, reflection_slot_pixel_source, "reflection_pixel", "ps_6_0", NULL, 0);
	if (!dxil) return 1 | finish_device(device);
	const unsigned char *container = (const unsigned char *)ZEST_DX_CALL0(dxil, GetBufferPointer);
	size_t container_size = ZEST_DX_CALL0(dxil, GetBufferSize);
	GuardedBuffer buffer = create_guarded_buffer(container_size);
	failed |= buffer.allocation == NULL;
	if (buffer.allocation) {
		failed |= !zest__dx_reflect_shader(guarded_copy(&buffer, container, container_size), container_size, &reflection, &error);
		zest_uint accepted_prefixes = 0;
		for (size_t length = 0; length != container_size; ++length) {
			accepted_prefixes += zest__dx_reflect_shader(guarded_copy(&buffer, container, length), length, &reflection, &error) != ZEST_FALSE;
			//The same prefix with its size field rewritten to match, so the part bounds are what has to catch it
			if (length >= 28) {
				unsigned char *copy = guarded_copy(&buffer, container, length);
				zest_uint patched_size = (zest_uint)length;
				memcpy(copy + 24, &patched_size, sizeof(patched_size));
				accepted_prefixes += zest__dx_reflect_shader(copy, length, &reflection, &error) != ZEST_FALSE;
			}
		}
		if (accepted_prefixes) {
			ZEST_PRINT("\t%u truncated containers were accepted", accepted_prefixes);
			failed = 1;
		}
		zest_uint spirv[16] = { 0x07230203 };
		failed |= zest__dx_reflect_shader(guarded_copy(&buffer, spirv, sizeof(spirv)), sizeof(spirv), &reflection, &error) != ZEST_FALSE;
		VirtualFree(buffer.allocation, 0, MEM_RELEASE);
	}
	ZEST_DX_RELEASE(dxil);

	const char *old_validator[2] = { "-validator-version", "1.5" };
	ID3DBlob *old_dxil = compile_test_hlsl(device, reflection_compute_source, "reflection_compute_validator_1_5", "cs_6_0", old_validator, 2);
	ID3DBlob *current_dxil = compile_test_hlsl(device, reflection_compute_source, "reflection_compute", "cs_6_0", NULL, 0);
	failed |= !old_dxil || !current_dxil;
	if (old_dxil) {
		error = NULL;
		failed |= zest__dx_reflect_shader(ZEST_DX_CALL0(old_dxil, GetBufferPointer), ZEST_DX_CALL0(old_dxil, GetBufferSize), &reflection, &error) != ZEST_FALSE;
		failed |= !error || !strstr(error, "1.6");
	}
	failed |= !reflect_blob(current_dxil, &reflection, "reflection_compute");
	ZEST_DX_RELEASE(old_dxil);
	ZEST_DX_RELEASE(current_dxil);
	return failed | finish_device(device);
}

#define TEST_UNBOUNDED 0xFFFFFFFF

static zest_dx_shader_reflection_t test_reflection(zest_uint stage) {
	zest_dx_shader_reflection_t reflection;
	memset(&reflection, 0, sizeof(reflection));
	reflection.stage = stage;
	return reflection;
}

static void add_test_resource(zest_dx_shader_reflection_t *reflection, zest_uint type, zest_uint kind, zest_uint space, zest_uint lower_bound, zest_uint upper_bound) {
	zest_dx_shader_resource_t *resource = &reflection->resources[reflection->resource_count++];
	resource->type = type;
	resource->kind = kind;
	resource->space = space;
	resource->lower_bound = lower_bound;
	resource->upper_bound = upper_bound;
}

static zest_dx_bindless_regions_t test_bindless_regions(void) {
	zest_uint counts[zest_max_global_binding_number] = { 64, 1024, 1024, 1024, 256, 1024, 1024, 64, 256 };
	zest_dx_bindless_regions_t regions;
	zest__dx_compute_bindless_regions(counts, &regions);
	return regions;
}

static zest_bool derive_test_layout(zest_dx_shader_reflection_t *first, zest_dx_shader_reflection_t *second, zest_uint push_constant_size, zest_dx_root_layout_t *out_layout, char *out_error, zest_size error_size) {
	zest_dx_bindless_regions_t regions = test_bindless_regions();
	const zest_dx_shader_reflection_t *stages[2] = { first, second };
	return zest__dx_derive_root_layout(stages, second ? 2 : 1, push_constant_size, first->stage == 1, &regions, out_layout, out_error, error_size);
}

int test_d3d12_root_layout_rules(void) {
	int failed = 0;
	char error[512] = {};
	zest_dx_root_layout_t layout;

	zest_dx_shader_reflection_t vertex = test_reflection(1);
	zest_dx_shader_reflection_t pixel = test_reflection(0);
	add_test_resource(&vertex, 2, 13, 0, 0, 0);
	add_test_resource(&pixel, 2, 13, 0, 0, 0);
	failed |= !derive_test_layout(&vertex, &pixel, 128, &layout, error, sizeof(error));
	failed |= layout.push_count != 1 || layout.pushes[0].visibility != D3D12_SHADER_VISIBILITY_ALL || layout.push_dword_count != 32 || layout.push_as_cbv != ZEST_FALSE;

	//The baked billboard's shape: two push registers and both tables, 66 DWORDs as root constants
	vertex = test_reflection(1);
	pixel = test_reflection(0);
	add_test_resource(&vertex, 2, 13, 0, 2, 2);
	add_test_resource(&pixel, 2, 13, 0, 1, 1);
	add_test_resource(&pixel, 1, 14, 100, 0, TEST_UNBOUNDED);
	add_test_resource(&pixel, 3, 2, 101, 0, TEST_UNBOUNDED);
	failed |= !derive_test_layout(&vertex, &pixel, 128, &layout, error, sizeof(error));
	failed |= layout.push_count != 2 || layout.pushes[0].shader_register != 2 || layout.pushes[0].visibility != D3D12_SHADER_VISIBILITY_VERTEX;
	failed |= layout.pushes[1].shader_register != 1 || layout.pushes[1].visibility != D3D12_SHADER_VISIBILITY_PIXEL || layout.push_as_cbv != ZEST_TRUE;
	zest_dx_root_parameters_t parameters = zest__dx_root_parameters(&layout);
	failed |= parameters.push_count != 2 || parameters.resource_table != 2 || parameters.sampler_table != 3 || parameters.push_as_cbv != ZEST_TRUE;
	failed |= !derive_test_layout(&vertex, &pixel, 48, &layout, error, sizeof(error));
	failed |= layout.push_as_cbv != ZEST_FALSE || layout.push_dword_count != 12;

	//Every row of the slot table, expected range type and region start written out for the counts above
	static const zest_uint expected_range_types[12] = {
		D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER, D3D12_DESCRIPTOR_RANGE_TYPE_SRV, D3D12_DESCRIPTOR_RANGE_TYPE_SRV, D3D12_DESCRIPTOR_RANGE_TYPE_SRV,
		D3D12_DESCRIPTOR_RANGE_TYPE_SRV, D3D12_DESCRIPTOR_RANGE_TYPE_SRV, D3D12_DESCRIPTOR_RANGE_TYPE_SRV, D3D12_DESCRIPTOR_RANGE_TYPE_SRV,
		D3D12_DESCRIPTOR_RANGE_TYPE_UAV, D3D12_DESCRIPTOR_RANGE_TYPE_UAV, D3D12_DESCRIPTOR_RANGE_TYPE_UAV, D3D12_DESCRIPTOR_RANGE_TYPE_CBV,
	};
	static const zest_uint expected_heap_offsets[12] = { 0, 0, 1024, 2048, 3072, 6464, 3328, 3328, 4352, 4352, 5376, 6400 };
	pixel = test_reflection(0);
	for (zest_uint slot = 0; slot != 12; ++slot) {
		add_test_resource(&pixel, reflection_slot_types[slot][0], reflection_slot_types[slot][1], 100 + slot, 0, TEST_UNBOUNDED);
	}
	failed |= !derive_test_layout(&pixel, NULL, 128, &layout, error, sizeof(error));
	failed |= layout.resource_range_count != 11 || layout.sampler_range_count != 1 || layout.push_count != 0;
	for (zest_uint slot = 0; slot != 12; ++slot) {
		const zest_dx_root_range_t *range = NULL;
		const zest_dx_root_range_t *ranges = slot == 0 ? layout.sampler_ranges : layout.resource_ranges;
		zest_uint range_count = slot == 0 ? layout.sampler_range_count : layout.resource_range_count;
		for (zest_uint range_index = 0; range_index != range_count; ++range_index) {
			if (ranges[range_index].space == 100 + slot) range = &ranges[range_index];
		}
		if (!range || range->range_type != expected_range_types[slot] || range->heap_offset != expected_heap_offsets[slot] || range->base_register != 0) {
			ZEST_PRINT("\tSlot %u in space %u didn't get range type %u at heap offset %u", slot, 100 + slot, expected_range_types[slot], expected_heap_offsets[slot]);
			failed = 1;
		}
	}
	parameters = zest__dx_root_parameters(&layout);
	failed |= parameters.resource_table != 0 || parameters.sampler_table != 1;

	//One array two stages declare at the same register is one range
	vertex = test_reflection(1);
	pixel = test_reflection(0);
	add_test_resource(&vertex, 3, 2, 101, 0, TEST_UNBOUNDED);
	add_test_resource(&pixel, 3, 2, 101, 0, TEST_UNBOUNDED);
	failed |= !derive_test_layout(&vertex, &pixel, 128, &layout, error, sizeof(error));
	failed |= layout.resource_range_count != 1;
	parameters = zest__dx_root_parameters(&layout);
	failed |= parameters.resource_table != 0 || parameters.sampler_table != ZEST_INVALID;

	//An unbounded ConstantBuffer<T> array in space 0 is a uniform array, beside the push block
	pixel = test_reflection(0);
	add_test_resource(&pixel, 2, 13, 0, 0, 0);
	add_test_resource(&pixel, 2, 13, 0, 1, TEST_UNBOUNDED);
	failed |= !derive_test_layout(&pixel, NULL, 128, &layout, error, sizeof(error));
	failed |= layout.push_count != 1 || layout.pushes[0].shader_register != 0 || layout.resource_range_count != 1;
	failed |= layout.resource_ranges[0].range_type != D3D12_DESCRIPTOR_RANGE_TYPE_CBV || layout.resource_ranges[0].base_register != 1 || layout.resource_ranges[0].heap_offset != 6400;

	//The layout's bytes are the cache key, so nothing left over from before may survive in them
	vertex = test_reflection(1);
	pixel = test_reflection(0);
	add_test_resource(&vertex, 2, 13, 0, 2, 2);
	add_test_resource(&pixel, 2, 13, 0, 1, 1);
	add_test_resource(&pixel, 1, 14, 100, 0, TEST_UNBOUNDED);
	zest_dx_root_layout_t second_layout;
	memset(&layout, 0xCD, sizeof(layout));
	memset(&second_layout, 0x5A, sizeof(second_layout));
	failed |= !derive_test_layout(&vertex, &pixel, 128, &layout, error, sizeof(error));
	failed |= !derive_test_layout(&vertex, &pixel, 128, &second_layout, error, sizeof(error));
	failed |= memcmp(&layout, &second_layout, sizeof(layout)) != 0;
	if (error[0]) ZEST_PRINT("\t%s", error);
	return failed;
}

struct RootLayoutErrorCase {
	const char *name;
	const char *fragment;
	zest_uint space;
};

int test_d3d12_root_layout_errors(void) {
	int failed = 0;
	char error[512];
	zest_dx_root_layout_t layout;
	static const RootLayoutErrorCase cases[] = {
		{ "Texture2DMS array", "is an unbounded array of a type the D3D12 backend has no bindless region for (type 3, kind 3)", 101 },
		{ "Counter buffer", "is a structured buffer that uses its hidden counter, which the D3D12 backend doesn't support", 105 },
		{ "Non-array in space 3", "is a non-array resource in space 3. Custom descriptor sets aren't available on the D3D12 backend", 3 },
		{ "Non-array texture in space 0", "is a non-array resource in space 0. Only constant buffers, which are push constants, can be declared there", 0 },
		{ "Push without a range", "reads a push constant block but the pipeline layout has no push constant range", 0 },
		{ "Different types", "has a different type from the array another stage declares at the same register", 101 },
		{ "Too many arrays", "is one bindless array more than the D3D12 backend's limit", 132 },
		{ "Too many samplers", "is one bindless array more than the D3D12 backend's limit", 104 },
		{ "Too many push registers", "is one push constant register more than the D3D12 backend's limit", 0 },
		{ "Fixed size array", "is a fixed size array", 9 },
		{ "Push range too large", "is over the D3D12 backend's limit of 128 bytes", 0 },
	};
	zest_uint case_count = sizeof(cases) / sizeof(cases[0]);
	for (zest_uint case_index = 0; case_index != case_count; ++case_index) {
		zest_dx_shader_reflection_t vertex = test_reflection(1);
		zest_dx_shader_reflection_t pixel = test_reflection(0);
		zest_dx_shader_reflection_t *first = &pixel;
		zest_dx_shader_reflection_t *second = NULL;
		zest_uint push_constant_size = 128;
		switch (case_index) {
		case 0: add_test_resource(&pixel, 3, 3, 101, 0, TEST_UNBOUNDED); break;
		case 1: add_test_resource(&pixel, 9, 12, 105, 0, TEST_UNBOUNDED); break;
		case 2: add_test_resource(&pixel, 3, 2, 3, 0, 0); break;
		case 3: add_test_resource(&pixel, 3, 2, 0, 0, 0); break;
		case 4: add_test_resource(&pixel, 2, 13, 0, 0, 0); push_constant_size = 0; break;
		case 5:
			add_test_resource(&vertex, 3, 2, 101, 0, TEST_UNBOUNDED);
			add_test_resource(&pixel, 3, 5, 101, 0, TEST_UNBOUNDED);
			first = &vertex;
			second = &pixel;
			break;
		case 6:
			for (zest_uint space = 0; space != ZEST_DX_MAX_ROOT_RANGES + 1; ++space) add_test_resource(&pixel, 3, 2, 100 + space, 0, TEST_UNBOUNDED);
			break;
		case 7:
			for (zest_uint space = 0; space != ZEST_DX_MAX_SAMPLER_RANGES + 1; ++space) add_test_resource(&pixel, 1, 14, 100 + space, 0, TEST_UNBOUNDED);
			break;
		case 8:
			for (zest_uint shader_register = 0; shader_register != ZEST_DX_MAX_PUSH_REGISTERS + 1; ++shader_register) add_test_resource(&pixel, 2, 13, 0, shader_register, shader_register);
			break;
		case 9: add_test_resource(&pixel, 3, 2, 9, 0, 3); break;
		case 10: add_test_resource(&pixel, 2, 13, 0, 0, 0); push_constant_size = 256; break;
		}
		error[0] = 0;
		zest_bool derived = derive_test_layout(first, second, push_constant_size, &layout, error, sizeof(error));
		char space_text[32];
		snprintf(space_text, sizeof(space_text), "space %u", cases[case_index].space);
		if (derived || !strstr(error, cases[case_index].fragment) || !strstr(error, space_text) || strncmp(error, "The pixel shader's ", 19) != 0) {
			ZEST_PRINT("\t%s: %s", cases[case_index].name, derived ? "derived without an error" : error);
			failed = 1;
		}
	}
	zest_dx_shader_reflection_t geometry = test_reflection(2);
	failed |= derive_test_layout(&geometry, NULL, 128, &layout, error, sizeof(error)) != ZEST_FALSE;
	return failed;
}

static int create_root_signature_from_blobs(zest_device device, const void **datas, const size_t *sizes, zest_uint stage_count, const char *name, zest_dx_root_layout_t *out_layout) {
	zest_dx_shader_reflection_t reflections[2];
	const zest_dx_shader_reflection_t *stages[2];
	zest_bool input_assembler = ZEST_FALSE;
	for (zest_uint stage_index = 0; stage_index != stage_count; ++stage_index) {
		const char *error = NULL;
		if (!zest__dx_reflect_shader(datas[stage_index], sizes[stage_index], &reflections[stage_index], &error)) {
			ZEST_PRINT("\t%s: %s", name, error);
			return 1;
		}
		stages[stage_index] = &reflections[stage_index];
		input_assembler |= reflections[stage_index].stage == 1;
	}
	char error_text[512];
	const zest_dx_bindless_regions_t *regions = &ZEST_DX_BACKEND(descriptor_set, device->bindless_set)->pool->regions;
	if (!zest__dx_derive_root_layout(stages, stage_count, 128, input_assembler, regions, out_layout, error_text, sizeof(error_text))) {
		ZEST_PRINT("\t%s: %s", name, error_text);
		return 1;
	}
	if (!zest__dx_root_signature(device, out_layout)) {
		ZEST_PRINT("\t%s: the root signature wasn't created", name);
		return 1;
	}
	return 0;
}

static int create_root_signature_from_dxil(zest_device device, ID3DBlob **blobs, zest_uint stage_count, const char *name, zest_dx_root_layout_t *out_layout) {
	const void *datas[2];
	size_t sizes[2];
	for (zest_uint stage_index = 0; stage_index != stage_count; ++stage_index) {
		if (!blobs[stage_index]) return 1;
		datas[stage_index] = ZEST_DX_CALL0(blobs[stage_index], GetBufferPointer);
		sizes[stage_index] = ZEST_DX_CALL0(blobs[stage_index], GetBufferSize);
	}
	return create_root_signature_from_blobs(device, datas, sizes, stage_count, name, out_layout);
}

int test_d3d12_root_signatures_from_shaders(void) {
	zest_device device = zest_EndDeviceBuilder(BeginD3D12TestBuilder());
	if (!device) return 1;
	int failed = 0;
	zest_uint created_count = 0;
	zest_dx_root_layout_t layout;
	ID3DBlob *files[D3D12_TEST_SHADER_FILE_COUNT];
	for (zest_uint file_index = 0; file_index != D3D12_TEST_SHADER_FILE_COUNT; ++file_index) {
		files[file_index] = compile_test_shader_file(device, &d3d12_test_shader_files[file_index]);
	}
	//The six compute files come first, then instance.vert, instance.frag, vertex.vert and vertex.frag
	for (zest_uint file_index = 0; file_index != 6; ++file_index) {
		failed |= create_root_signature_from_dxil(device, &files[file_index], 1, d3d12_test_shader_files[file_index].file_name, &layout);
		created_count++;
	}
	failed |= create_root_signature_from_dxil(device, &files[6], 2, "instance.vert + instance.frag", &layout);
	failed |= create_root_signature_from_dxil(device, &files[8], 2, "vertex.vert + vertex.frag", &layout);
	created_count += 2;
	ID3D12RootSignature *first_signature = zest__dx_root_signature(device, &layout);
	failed |= first_signature == NULL || zest__dx_root_signature(device, &layout) != first_signature;
	for (zest_uint file_index = 0; file_index != D3D12_TEST_SHADER_FILE_COUNT; ++file_index) {
		ZEST_DX_RELEASE(files[file_index]);
	}

#ifdef ZEST_ENABLE_SLANG
	zest_slang_blob_t slang_blobs[D3D12_SLANG_ENTRY_COUNT];
	failed |= compile_slang_dxil_set(device, slang_blobs);
	zest_uint baked_vertex = ZEST_INVALID;
	zest_uint billboard_fragment = ZEST_INVALID;
	for (zest_uint entry_index = 0; entry_index != D3D12_SLANG_ENTRY_COUNT; ++entry_index) {
		zest_slang_blob_t *blob = &slang_blobs[entry_index];
		if (!strcmp(d3d12_slang_entries[entry_index].entry_point, "tfx_billboard_baked_vertex")) baked_vertex = entry_index;
		if (!strcmp(d3d12_slang_entries[entry_index].entry_point, "tfx_billboard_fragment")) billboard_fragment = entry_index;
		if (!blob->data) continue;
		const void *data = blob->data;
		size_t size = (size_t)blob->size;
		failed |= create_root_signature_from_blobs(device, &data, &size, 1, d3d12_slang_entries[entry_index].entry_point, &layout);
		created_count++;
	}
	if (baked_vertex != ZEST_INVALID && billboard_fragment != ZEST_INVALID && slang_blobs[baked_vertex].data && slang_blobs[billboard_fragment].data) {
		const void *datas[2] = { slang_blobs[baked_vertex].data, slang_blobs[billboard_fragment].data };
		size_t sizes[2] = { (size_t)slang_blobs[baked_vertex].size, (size_t)slang_blobs[billboard_fragment].size };
		failed |= create_root_signature_from_blobs(device, datas, sizes, 2, "tfx_billboard_baked_vertex + tfx_billboard_fragment", &layout);
		created_count++;
		zest_bool baked_shape = layout.push_count == 2 && layout.pushes[0].shader_register == 2 && layout.pushes[0].visibility == D3D12_SHADER_VISIBILITY_VERTEX &&
			layout.pushes[1].shader_register == 1 && layout.pushes[1].visibility == D3D12_SHADER_VISIBILITY_PIXEL && layout.push_as_cbv;
		ZEST_PRINT("\tBaked billboard: %u push registers, b%u for %s and b%u for %s, %s", layout.push_count, layout.pushes[0].shader_register, layout.pushes[0].visibility == D3D12_SHADER_VISIBILITY_VERTEX ? "vertex" : "other",
			layout.pushes[1].shader_register, layout.pushes[1].visibility == D3D12_SHADER_VISIBILITY_PIXEL ? "pixel" : "other", layout.push_as_cbv ? "root CBVs" : "root constants");
		failed |= !baked_shape;
	} else {
		failed = 1;
	}
	release_slang_dxil_set(device, slang_blobs);
#else
	ZEST_PRINT("\tThe Slang set was skipped, as ZEST_ENABLE_SLANG isn't defined");
#endif
	ZEST_PRINT("\t%u root signatures created, %u distinct", created_count, zest_map_size(ZEST_DX_BACKEND(device, device)->root_signatures));
	failed |= zest_GetValidationErrorCount(device) != 0;

	zest_ResetDevice(device);
	failed |= zest__dx_last_device_release_count() != 0;
	failed |= zest_map_size(ZEST_DX_BACKEND(device, device)->root_signatures) != 0;
	ID3D12RootSignature *recreated = zest__dx_root_signature(device, &layout);
	failed |= recreated == NULL || zest_map_size(ZEST_DX_BACKEND(device, device)->root_signatures) != 1;
	return failed | finish_device(device);
}

static const char *macro_compute_source =
	"#if FIRST != 3\n"
	"#error FIRST must be 3\n"
	"#endif\n"
	"#ifndef SECOND\n"
	"#error SECOND is missing\n"
	"#endif\n"
	"[numthreads(1, 1, 1)]\n"
	"void main() {}\n";

//The part's data, or NULL when a container that already parsed has no part with this FourCC
static const unsigned char *find_container_part(const void *container, zest_uint fourcc, zest_uint *out_part_size) {
	const unsigned char *bytes = (const unsigned char *)container;
	zest_uint part_count;
	memcpy(&part_count, bytes + 28, sizeof(part_count));
	for (zest_uint part = 0; part != part_count; ++part) {
		zest_uint offset;
		zest_uint part_fourcc;
		memcpy(&offset, bytes + 32 + part * 4, sizeof(offset));
		memcpy(&part_fourcc, bytes + offset, sizeof(part_fourcc));
		if (part_fourcc == fourcc) {
			memcpy(out_part_size, bytes + offset + 4, sizeof(*out_part_size));
			return bytes + offset + 8;
		}
	}
	return NULL;
}

#define D3D12_CONCURRENT_SOURCE_COUNT 8
#define D3D12_CONCURRENT_THREAD_COUNT 4

struct D3D12CompileWorker {
	zest_device device;
	zest_file *sources;				//D3D12_CONCURRENT_SOURCE_COUNT, the first files of d3d12_test_shader_files
	zest_uint parsed_count;
};

static DWORD WINAPI concurrent_compile_worker(void *parameter) {
	D3D12CompileWorker *worker = (D3D12CompileWorker *)parameter;
	for (zest_uint source_index = 0; source_index != D3D12_CONCURRENT_SOURCE_COUNT; ++source_index) {
		const D3D12TestShaderFile *shader_file = &d3d12_test_shader_files[source_index];
		ID3DBlob *dxil = compile_test_hlsl(worker->device, worker->sources[source_index], shader_file->file_name, shader_file->profile, NULL, 0);
		zest_dx_shader_reflection_t reflection;
		if (reflect_blob(dxil, &reflection, shader_file->file_name)) worker->parsed_count++;
		ZEST_DX_RELEASE(dxil);
	}
	return 0;
}

int test_d3d12_shader_compile(void) {
	zest_device device = zest_EndDeviceBuilder(BeginD3D12TestBuilder());
	if (!device) return 1;
	int failed = 0;
	zest_dx_device_backend_t *backend = ZEST_DX_BACKEND(device, device);

	//Profile: the DXIL part's program version ends in the shader model as 0x6x
	zest_shader_handle profile_handle = zest_CreateShader(device, hlsl_empty_compute, zest_compute_shader, "zest_profile_test", NULL, ZEST_TRUE);
	zest_shader profile_shader = zest_GetShader(profile_handle);
	zest_dx_shader_reflection_t reflection;
	const char *error = NULL;
	zest_uint program_version = 0;
	if (zest__dx_reflect_shader(profile_shader->binary, profile_shader->binary_size, &reflection, &error)) {
		zest_uint part_size = 0;
		const unsigned char *program = find_container_part(profile_shader->binary, ZEST__DX_FOURCC_DXIL, &part_size);
		if (program && part_size >= sizeof(program_version)) memcpy(&program_version, program, sizeof(program_version));
	}
	ZEST_PRINT("\tShader model 0x%X, compute program version 0x%X", (unsigned int)backend->support.shader_model, program_version);
	failed |= program_version == 0 || (program_version & 0xFF) != ((zest_uint)backend->support.shader_model & 0xFF);
	zest_FreeShader(profile_handle);

	//Macros: one with a value and one without, then a compile without them fails and keeps the last binary
	zest_shader_handle macro_handle = zest_CreateShader(device, hlsl_empty_compute, zest_compute_shader, "zest_macro_test", NULL, ZEST_TRUE);
	zest_shader_options options = zest_CreateShaderOptions(device);
	zest_AddMacroDefinition(options, "FIRST", "3");
	zest_AddMacroDefinition(options, "SECOND", NULL);
	failed |= !compile_test_source(device, macro_handle, macro_compute_source, options);
	zest_FreeShaderOptions(options);
	zest_shader macro_shader = zest_GetShader(macro_handle);
	char *binary_before = macro_shader->binary;
	zest_size size_before = macro_shader->binary_size;
	zest_key hash_before = zest_Hash(macro_shader->binary, (zest_uint)size_before, ZEST_HASH_SEED);
	zest_shader_options no_macros = zest_CreateShaderOptions(device);
	ZEST_PRINT("\tThe next shader compilation error is expected");
	failed |= zest_CompileShader(macro_handle, no_macros) != ZEST_FALSE;
	zest_FreeShaderOptions(no_macros);
	failed |= macro_shader->binary != binary_before || macro_shader->binary_size != size_before || zest_Hash(macro_shader->binary, (zest_uint)size_before, ZEST_HASH_SEED) != hash_before;
	failed |= zest_GetShaderLastError(macro_handle)[0] == 0;
	zest_FreeShader(macro_handle);

	//Salt
	failed |= device->shader_cache_salt == 0;
	failed |= zest__shader_cache_key(macro_compute_source, zest_compute_shader, "main", NULL, 0) == zest__shader_cache_key(macro_compute_source, zest_compute_shader, "main", NULL, 1);

	//Concurrent: DXC compiler objects aren't thread safe, so compiles must serialise inside zest__dx_compile_hlsl
	zest_file sources[D3D12_CONCURRENT_SOURCE_COUNT] = {};
	for (zest_uint source_index = 0; source_index != D3D12_CONCURRENT_SOURCE_COUNT; ++source_index) {
		char path[256];
		snprintf(path, sizeof(path), D3D12_HLSL_SHADER_PATH "%s", d3d12_test_shader_files[source_index].file_name);
		sources[source_index] = zest_ReadEntireFile(device, path, ZEST_TRUE);
		failed |= sources[source_index] == NULL;
	}
	if (!failed) {
		D3D12CompileWorker workers[D3D12_CONCURRENT_THREAD_COUNT] = {};
		HANDLE threads[D3D12_CONCURRENT_THREAD_COUNT] = {};
		zest_uint thread_count = 0;
		for (; thread_count != D3D12_CONCURRENT_THREAD_COUNT; ++thread_count) {
			workers[thread_count].device = device;
			workers[thread_count].sources = sources;
			threads[thread_count] = CreateThread(NULL, 0, concurrent_compile_worker, &workers[thread_count], 0, NULL);
			if (!threads[thread_count]) break;
		}
		WaitForMultipleObjects(thread_count, threads, TRUE, INFINITE);
		zest_uint parsed_count = 0;
		for (zest_uint thread = 0; thread != thread_count; ++thread) {
			CloseHandle(threads[thread]);
			parsed_count += workers[thread].parsed_count;
		}
		ZEST_PRINT("\t%u of %u concurrent compiles parsed", parsed_count, D3D12_CONCURRENT_THREAD_COUNT * D3D12_CONCURRENT_SOURCE_COUNT);
		failed |= parsed_count != D3D12_CONCURRENT_THREAD_COUNT * D3D12_CONCURRENT_SOURCE_COUNT;
	}
	for (zest_uint source_index = 0; source_index != D3D12_CONCURRENT_SOURCE_COUNT; ++source_index) {
		if (sources[source_index]) zest_FreeFile(device, sources[source_index]);
	}
	failed |= zest_GetValidationErrorCount(device) != 0;
	return failed | finish_device(device);
}

static const char *translation_vertex_source =
	"struct vertex_input {\n"
	"	uint instance : SV_InstanceID;\n"
	"	float3 position : POSITION0;\n"
	"	float2 uv : TEXCOORD0;\n"
	"	float4 color : COLOR0;\n"
	"};\n"
	"float4 main(vertex_input input) : SV_Position {\n"
	"	return float4(input.position + float3(input.uv, (float)input.instance), 1.0) * input.color;\n"
	"}\n";

static zest_rendering_info_t colour_rendering_info(zest_format depth_format) {
	zest_rendering_info_t rendering_info = {};
	rendering_info.color_attachment_count = 1;
	rendering_info.color_attachment_formats[0] = zest_format_r8g8b8a8_unorm;
	rendering_info.depth_attachment_format = depth_format;
	return rendering_info;
}

//Location 3 sits on a per instance binding that the translation vertex shader never reads
static zest_pipeline_template translation_template(zest_device device, const char *name, zest_bool with_uv) {
	zest_pipeline_template pipeline_template = zest_CreatePipelineTemplate(device, name);
	zest_AddVertexInputBindingDescription(pipeline_template, 0, 36, zest_input_rate_vertex);
	zest_AddVertexInputBindingDescription(pipeline_template, 1, 16, zest_input_rate_instance);
	zest_AddVertexAttribute(pipeline_template, 0, 0, zest_format_r32g32b32_sfloat, 0);
	if (with_uv) zest_AddVertexAttribute(pipeline_template, 0, 1, zest_format_r32g32_sfloat, 12);
	zest_AddVertexAttribute(pipeline_template, 0, 2, zest_format_r8g8b8a8_unorm, 20);
	zest_AddVertexAttribute(pipeline_template, 1, 3, zest_format_r32g32b32a32_sfloat, 0);
	return pipeline_template;
}

int test_d3d12_pipeline_state_translation(void) {
	zest_device device = zest_EndDeviceBuilder(BeginD3D12TestBuilder());
	if (!device) return 1;
	zest_dx_device_backend_t *backend = ZEST_DX_BACKEND(device, device);
	ID3DBlob *translation_dxil = compile_test_hlsl(device, translation_vertex_source, "translation_vertex", "vs_6_0", NULL, 0);
	ID3DBlob *simple_dxil = compile_test_hlsl(device, hlsl_vertex_source, "simple_vertex", "vs_6_0", NULL, 0);
	zest_dx_shader_reflection_t translation_reflection;
	zest_dx_shader_reflection_t simple_reflection;
	int failed = !reflect_blob(translation_dxil, &translation_reflection, "translation_vertex") || !reflect_blob(simple_dxil, &simple_reflection, "simple_vertex");
	D3D12_GRAPHICS_PIPELINE_STATE_DESC description;
	D3D12_INPUT_ELEMENT_DESC elements[ZEST_DX_MAX_SHADER_INPUTS];
	char error[640] = {};
	zest_rendering_info_t colour_only = colour_rendering_info(zest_format_undefined);
	zest_rendering_info_t with_depth = colour_rendering_info(zest_format_d32_sfloat);
	if (!failed) {
		//Input layout
		zest_pipeline_template input_template = translation_template(device, "Translation Input", ZEST_TRUE);
		if (zest__dx_pipeline_description(device, input_template, &colour_only, &translation_reflection, &description, elements, error, sizeof(error))) {
			failed |= description.InputLayout.NumElements != 3;
			failed |= strcmp(elements[0].SemanticName, "POSITION") != 0 || elements[0].InputSlot != 0 || elements[0].InputSlotClass != D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA || elements[0].InstanceDataStepRate != 0;
			failed |= strcmp(elements[1].SemanticName, "TEXCOORD") != 0 || strcmp(elements[2].SemanticName, "COLOR") != 0;
		} else {
			ZEST_PRINT("\t%s", error);
			failed = 1;
		}
		input_template->attribute_descriptions[2].binding = 1;
		if (zest__dx_pipeline_description(device, input_template, &colour_only, &translation_reflection, &description, elements, error, sizeof(error))) {
			failed |= elements[2].InputSlot != 1 || elements[2].InputSlotClass != D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA || elements[2].InstanceDataStepRate != 1;
		} else {
			ZEST_PRINT("\t%s", error);
			failed = 1;
		}
		zest_pipeline_template missing_template = translation_template(device, "Translation Missing", ZEST_FALSE);
		failed |= zest__dx_pipeline_description(device, missing_template, &colour_only, &translation_reflection, &description, elements, error, sizeof(error)) != ZEST_FALSE;
		failed |= strstr(error, "TEXCOORD") == NULL;
		zest_FreePipelineTemplate(input_template);
		zest_FreePipelineTemplate(missing_template);

		//Blend
		zest_pipeline_template state_template = zest_CreatePipelineTemplate(device, "Translation State");
		zest_SetPipelineDisableVertexInput(state_template);
		state_template->color_blend_attachment.src_color_blend_factor = zest_blend_factor_dst_color;
		state_template->color_blend_attachment.src_alpha_blend_factor = zest_blend_factor_dst_color;
		failed |= !zest__dx_pipeline_description(device, state_template, &colour_only, &simple_reflection, &description, elements, error, sizeof(error));
		failed |= description.BlendState.RenderTarget[0].SrcBlend != D3D12_BLEND_DEST_COLOR || description.BlendState.RenderTarget[0].SrcBlendAlpha != D3D12_BLEND_DEST_ALPHA;

		//Depth, and the sample count of 0 the rendering info has
		zest_SetPipelineDepthTest(state_template, ZEST_TRUE, ZEST_TRUE);
		state_template->depth_stencil.depth_compare_op = zest_compare_op_less;
		failed |= !zest__dx_pipeline_description(device, state_template, &colour_only, &simple_reflection, &description, elements, error, sizeof(error));
		failed |= description.DepthStencilState.DepthEnable != FALSE || description.SampleDesc.Count != 1;
		failed |= !zest__dx_pipeline_description(device, state_template, &with_depth, &simple_reflection, &description, elements, error, sizeof(error));
		failed |= description.DepthStencilState.DepthEnable != TRUE || description.DepthStencilState.DepthFunc != D3D12_COMPARISON_FUNC_LESS || description.DSVFormat != DXGI_FORMAT_D32_FLOAT;

		//Front face
		zest_front_face front_faces[2] = { zest_front_face_counter_clockwise, zest_front_face_clockwise };
		for (int face_index = 0; face_index != 2; ++face_index) {
			state_template->rasterization.front_face = front_faces[face_index];
			failed |= !zest__dx_pipeline_description(device, state_template, &colour_only, &simple_reflection, &description, elements, error, sizeof(error));
			failed |= description.RasterizerState.FrontCounterClockwise != zest__dx_front_counter_clockwise(front_faces[face_index]);
		}

		//Depth bias
		state_template->rasterization.depth_bias_enable = ZEST_TRUE;
		state_template->rasterization.depth_bias_constant_factor = 1.6f;
		state_template->rasterization.depth_bias_clamp = 0.0f;
		state_template->rasterization.depth_bias_slope_factor = 1.75f;
		zest_bool saved_dynamic_depth_bias = backend->support.dynamic_depth_bias;
		backend->support.dynamic_depth_bias = ZEST_TRUE;
		failed |= !zest__dx_pipeline_description(device, state_template, &with_depth, &simple_reflection, &description, elements, error, sizeof(error));
		failed |= description.RasterizerState.DepthBias != 2 || description.RasterizerState.SlopeScaledDepthBias != 1.75f;
		failed |= (description.Flags & D3D12_PIPELINE_STATE_FLAG_DYNAMIC_DEPTH_BIAS) == 0;
		backend->support.dynamic_depth_bias = ZEST_FALSE;
		failed |= !zest__dx_pipeline_description(device, state_template, &with_depth, &simple_reflection, &description, elements, error, sizeof(error));
		failed |= (description.Flags & D3D12_PIPELINE_STATE_FLAG_DYNAMIC_DEPTH_BIAS) != 0;
		backend->support.dynamic_depth_bias = saved_dynamic_depth_bias;
		zest_FreePipelineTemplate(state_template);

		//Errors
		const char *unsupported_states[8] = { "zest_cull_mode_front_and_back", "zest_polygon_mode_point", "zest_topology_triangle_fan", "zest_topology_patch_list", "view_mask", "stencil_test_enable", "depth_bounds_test_enable", "rasterizer_discard_enable" };
		zest_bool saved_triangle_fan = backend->support.triangle_fan;
		D3D12_VIEW_INSTANCING_TIER saved_view_instancing_tier = backend->support.view_instancing_tier;
		for (int case_index = 0; case_index != 8; ++case_index) {
			zest_pipeline_template error_template = zest_CreatePipelineTemplate(device, "Translation Error");
			zest_SetPipelineDisableVertexInput(error_template);
			backend->support.triangle_fan = ZEST_TRUE;
			backend->support.view_instancing_tier = saved_view_instancing_tier;
			switch (case_index) {
				case 0: error_template->rasterization.cull_mode = zest_cull_mode_front_and_back; break;
				case 1: error_template->rasterization.polygon_mode = zest_polygon_mode_point; break;
				case 2: error_template->primitive_topology = zest_topology_triangle_fan; backend->support.triangle_fan = ZEST_FALSE; break;
				case 3: error_template->primitive_topology = zest_topology_patch_list; break;
				case 4: error_template->view_mask = 3; backend->support.view_instancing_tier = D3D12_VIEW_INSTANCING_TIER_NOT_SUPPORTED; break;
				case 5: error_template->depth_stencil.stencil_test_enable = ZEST_TRUE; break;
				case 6: error_template->depth_stencil.depth_bounds_test_enable = ZEST_TRUE; break;
				case 7: error_template->rasterization.rasterizer_discard_enable = ZEST_TRUE; break;
			}
			error[0] = 0;
			zest_bool described = zest__dx_pipeline_description(device, error_template, &colour_only, &simple_reflection, &description, elements, error, sizeof(error));
			if (described || strstr(error, unsupported_states[case_index]) == NULL) {
				ZEST_PRINT("\t%s wasn't rejected by name", unsupported_states[case_index]);
				failed = 1;
			}
			zest_FreePipelineTemplate(error_template);
		}
		backend->support.triangle_fan = saved_triangle_fan;
		backend->support.view_instancing_tier = saved_view_instancing_tier;
	}
	ZEST_DX_RELEASE(translation_dxil);
	ZEST_DX_RELEASE(simple_dxil);
	return failed | finish_device(device);
}

static const char *cache_other_pixel_source =
	"float4 main() : SV_Target {\n"
	"	return float4(0.25, 0.5, 1.0, 1.0);\n"
	"}\n";
static const char *cache_lonely_texture_source =
	"Texture2D lonely_texture : register(t0, space0);\n"
	"float4 main() : SV_Target {\n"
	"	return lonely_texture.Load(int3(0, 0, 0));\n"
	"}\n";

//A command list with no backend, which is all a pipeline build reads
static zest_command_list_t colour_command_list(zest_context context, zest_key render_pass_key) {
	zest_command_list_t command_list = ZEST__ZERO_INIT(zest_command_list_t);
	command_list.context = context;
	command_list.device = context->device;
	command_list.rendering_info = colour_rendering_info(zest_format_undefined);
	command_list.rendering_info.render_pass_key = render_pass_key;
	return command_list;
}

static zest_pipeline_template no_input_template(zest_device device, const char *name, zest_shader_handle vertex_shader, zest_shader_handle pixel_shader) {
	zest_pipeline_template pipeline_template = zest_CreatePipelineTemplate(device, name);
	zest_SetPipelineShaders(pipeline_template, vertex_shader, pixel_shader);
	zest_SetPipelineDisableVertexInput(pipeline_template);
	zest_SetPipelineBlend(pipeline_template, zest_BlendStateOpaque());
	return pipeline_template;
}

static ID3D12PipelineState *pipeline_state_of(zest_pipeline pipeline) {
	return pipeline ? ZEST_DX_BACKEND(pipeline, pipeline)->pipeline_state : NULL;
}

#define D3D12_CONCURRENT_PIPELINE_THREAD_COUNT 4

struct D3D12PipelineWorker {
	zest_device device;
	HANDLE start_event;			//Set once every thread exists, so their cache misses overlap
	const void *bytecode;
	zest_size bytecode_size;
	ID3D12RootSignature *root_signature;
	ID3D12PipelineState *pipeline_state;
};

static DWORD WINAPI concurrent_pipeline_worker(void *parameter) {
	D3D12PipelineWorker *worker = (D3D12PipelineWorker *)parameter;
	WaitForSingleObject(worker->start_event, INFINITE);
	worker->pipeline_state = zest__dx_compute_pipeline_state(worker->device, worker->bytecode, worker->bytecode_size, worker->root_signature);
	return 0;
}

//Every thread asks for the same compute pipeline state at once, and all must get the one the cache keeps
static int run_concurrent_pipeline_states(zest_device device) {
	zest_dx_device_backend_t *backend = ZEST_DX_BACKEND(device, device);
	//The layout shader's root signature has a push block and a table, which a shader that reads nothing can run under
	zest_shader_handle layout_shader = zest_CreateShader(device, reflection_compute_source, zest_compute_shader, "cache_concurrent_layout", NULL, ZEST_TRUE);
	zest_shader_handle empty_shader = zest_CreateShader(device, hlsl_empty_compute, zest_compute_shader, "cache_concurrent_empty", NULL, ZEST_TRUE);
	zest_compute_handle compute_handle = zest_CreateCompute(device, "Cache Concurrent", layout_shader);
	zest_compute compute = compute_handle.value ? zest_GetCompute(compute_handle) : NULL;
	zest_shader empty = zest_GetShader(empty_shader);
	HANDLE start_event = CreateEventA(NULL, TRUE, FALSE, NULL);
	int failed = compute == NULL || empty == NULL || start_event == NULL;
	if (!failed) {
		zest_uint state_count = zest_map_size(backend->pipeline_states);
		D3D12PipelineWorker workers[D3D12_CONCURRENT_PIPELINE_THREAD_COUNT] = {};
		HANDLE threads[D3D12_CONCURRENT_PIPELINE_THREAD_COUNT] = {};
		zest_uint thread_count = 0;
		for (; thread_count != D3D12_CONCURRENT_PIPELINE_THREAD_COUNT; ++thread_count) {
			D3D12PipelineWorker *worker = &workers[thread_count];
			worker->device = device;
			worker->start_event = start_event;
			worker->bytecode = empty->binary;
			worker->bytecode_size = empty->binary_size;
			worker->root_signature = ZEST_DX_BACKEND(compute, compute)->root_signature;
			threads[thread_count] = CreateThread(NULL, 0, concurrent_pipeline_worker, worker, 0, NULL);
			if (!threads[thread_count]) break;
		}
		SetEvent(start_event);
		WaitForMultipleObjects(thread_count, threads, TRUE, INFINITE);
		for (zest_uint thread = 0; thread != thread_count; ++thread) {
			CloseHandle(threads[thread]);
			failed |= workers[thread].pipeline_state == NULL || workers[thread].pipeline_state != workers[0].pipeline_state;
		}
		failed |= thread_count != D3D12_CONCURRENT_PIPELINE_THREAD_COUNT || zest_map_size(backend->pipeline_states) != state_count + 1;
		ZEST_PRINT("\t%u threads asked for one pipeline state and the cache grew by %u", thread_count, zest_map_size(backend->pipeline_states) - state_count);
	}
	if (start_event) CloseHandle(start_event);
	if (compute) zest_FreeCompute(compute_handle);
	zest_FreeShader(layout_shader);
	zest_FreeShader(empty_shader);
	return failed;
}

int test_d3d12_pipeline_cache(void) {
	zest_device device = zest_EndDeviceBuilder(BeginD3D12TestBuilder());
	if (!device) return 1;
	zest_dx_device_backend_t *backend = ZEST_DX_BACKEND(device, device);
	zest_create_context_info_t info = zest_CreateContextInfo();
	zest_context context = zest_CreateHeadlessContext(device, &info);
	if (!context) return 1 | finish_device(device);
	zest_shader_handle vertex_shader = zest_CreateShader(device, hlsl_vertex_source, zest_vertex_shader, "cache_vertex", NULL, ZEST_TRUE);
	zest_shader_handle pixel_shader = zest_CreateShader(device, hlsl_pixel_source, zest_fragment_shader, "cache_pixel", NULL, ZEST_TRUE);
	zest_shader_handle other_pixel_shader = zest_CreateShader(device, cache_other_pixel_source, zest_fragment_shader, "cache_other_pixel", NULL, ZEST_TRUE);
	zest_shader_handle texture_pixel_shader = zest_CreateShader(device, cache_lonely_texture_source, zest_fragment_shader, "cache_texture_pixel", NULL, ZEST_TRUE);

	//Shared object
	zest_pipeline_template base_template = no_input_template(device, "Cache Base", vertex_shader, pixel_shader);
	zest_command_list_t first_list = colour_command_list(context, 1);
	zest_command_list_t second_list = colour_command_list(context, 2);
	zest_pipeline first_pipeline = zest_GetPipeline(base_template, &first_list);
	zest_pipeline second_pipeline = zest_GetPipeline(base_template, &second_list);
	ID3D12PipelineState *first_state = pipeline_state_of(first_pipeline);
	int failed = first_state == NULL || second_pipeline == NULL || first_pipeline == second_pipeline || pipeline_state_of(second_pipeline) != first_state;

	//Different state
	zest_pipeline_template blend_template = zest_CopyPipelineTemplate("Cache Blend", base_template);
	zest_SetPipelineBlend(blend_template, zest_AlphaBlendState());
	ID3D12PipelineState *blend_state = pipeline_state_of(zest_GetPipeline(blend_template, &first_list));
	failed |= blend_state == NULL || blend_state == first_state;
	zest_pipeline_template source_template = no_input_template(device, "Cache Source", vertex_shader, other_pixel_shader);
	ID3D12PipelineState *source_state = pipeline_state_of(zest_GetPipeline(source_template, &first_list));
	failed |= source_state == NULL || source_state == first_state;

	//Recreate, as a swapchain recreate does
	zest__cleanup_pipelines(context);
	zest_map_free(context->allocator, context->cached_pipelines);
	failed |= pipeline_state_of(zest_GetPipeline(base_template, &first_list)) != first_state;
	ZEST_PRINT("\t%u pipeline states and %u root signatures cached", zest_map_size(backend->pipeline_states), zest_map_size(backend->root_signatures));

	//Unsupported state
	zest_pipeline_template culled_template = zest_CopyPipelineTemplate("Cache Cull Both", base_template);
	zest_SetPipelineCullMode(culled_template, zest_cull_mode_front_and_back);
	zest_uint error_count = zest_GetValidationErrorCount(device);
	failed |= zest_GetPipeline(culled_template, &first_list) != NULL || zest_GetValidationErrorCount(device) != error_count + 1 || zest_PipelineIsValid(culled_template);

	//Unsupported shader
	zest_pipeline_template texture_template = no_input_template(device, "Cache Texture", vertex_shader, texture_pixel_shader);
	error_count = zest_GetValidationErrorCount(device);
	failed |= zest_GetPipeline(texture_template, &first_list) != NULL || zest_GetValidationErrorCount(device) != error_count + 1;

	//Still alive
	zest_pipeline_template alive_template = zest_CopyPipelineTemplate("Cache Alive", base_template);
	zest_SetPipelineCullMode(alive_template, zest_cull_mode_back);
	failed |= zest_GetPipeline(alive_template, &first_list) == NULL;

	zest_FreePipelineTemplate(base_template);
	zest_FreePipelineTemplate(blend_template);
	zest_FreePipelineTemplate(source_template);
	zest_FreePipelineTemplate(culled_template);
	zest_FreePipelineTemplate(texture_template);
	zest_FreePipelineTemplate(alive_template);
	zest_FreeShader(vertex_shader);
	zest_FreeShader(pixel_shader);
	zest_FreeShader(other_pixel_shader);
	zest_FreeShader(texture_pixel_shader);

	//Concurrent misses on one new state. Whether a create loses the race depends on timing; when one does, the release count after the reset checks it was released
	failed |= run_concurrent_pipeline_states(device);

	//Reset, which frees shaders and templates, so the first pipeline is made again from scratch
	zest_ResetValidationErrors(device);
	zest_ResetDevice(device);
	failed |= zest__dx_last_device_release_count() != 0;
	failed |= zest_map_size(backend->pipeline_states) != 0 || zest_map_size(backend->root_signatures) != 0;
	context = zest_CreateHeadlessContext(device, &info);
	failed |= context == NULL;
	if (context) {
		vertex_shader = zest_CreateShader(device, hlsl_vertex_source, zest_vertex_shader, "cache_vertex", NULL, ZEST_TRUE);
		pixel_shader = zest_CreateShader(device, hlsl_pixel_source, zest_fragment_shader, "cache_pixel", NULL, ZEST_TRUE);
		base_template = no_input_template(device, "Cache Base", vertex_shader, pixel_shader);
		first_list = colour_command_list(context, 1);
		failed |= zest_GetPipeline(base_template, &first_list) == NULL;
		failed |= zest_map_size(backend->pipeline_states) != 1;
		zest_FreePipelineTemplate(base_template);
		zest_FreeShader(vertex_shader);
		zest_FreeShader(pixel_shader);
		zest_DestroyContext(context);
	}
	return failed | finish_device(device);
}

int test_d3d12_precompiled_shaders(void) {
	zest_device first_device = zest_EndDeviceBuilder(BeginD3D12TestBuilder());
	if (!first_device) return 1;
	zest_shader_handle compiled_vertex = zest_CreateShaderFromFile(first_device, D3D12_HLSL_SHADER_PATH "vertex.vert.hlsl", "precompiled_vertex", zest_vertex_shader, NULL, ZEST_TRUE);
	zest_shader_handle compiled_pixel = zest_CreateShaderFromFile(first_device, D3D12_HLSL_SHADER_PATH "vertex.frag.hlsl", "precompiled_pixel", zest_fragment_shader, NULL, ZEST_TRUE);
	zest_shader vertex_source = zest_GetShader(compiled_vertex);
	zest_shader pixel_source = zest_GetShader(compiled_pixel);
	zest_uint vertex_size = (zest_uint)vertex_source->binary_size;
	zest_uint pixel_size = (zest_uint)pixel_source->binary_size;
	//The copies outlive the first device and its allocator
	void *vertex_binary = malloc(vertex_size);
	void *pixel_binary = malloc(pixel_size);
	memcpy(vertex_binary, vertex_source->binary, vertex_size);
	memcpy(pixel_binary, pixel_source->binary, pixel_size);
	int failed = finish_device(first_device);

	zest__dx_forced_dxc_library = "zest-missing-dxcompiler.dll";
	zest_device device = zest_EndDeviceBuilder(BeginD3D12TestBuilder());
	failed |= device == NULL;
	if (device) {
		zest_shader_handle vertex_shader = zest_CreateShaderFromBinary(device, "precompiled_vertex", vertex_binary, vertex_size, zest_vertex_shader);
		zest_shader_handle pixel_shader = zest_CreateShaderFromBinary(device, "precompiled_pixel", pixel_binary, pixel_size, zest_fragment_shader);
		zest_create_context_info_t info = zest_CreateContextInfo();
		zest_context context = zest_CreateHeadlessContext(device, &info);
		failed |= context == NULL;
		if (context) {
			zest_pipeline_template pipeline_template = zest_CreatePipelineTemplate(device, "Precompiled");
			zest_SetPipelineShaders(pipeline_template, vertex_shader, pixel_shader);
			zest_AddVertexInputBindingDescription(pipeline_template, 0, 48, zest_input_rate_vertex);
			zest_AddVertexAttribute(pipeline_template, 0, 0, zest_format_r32g32b32a32_sfloat, 0);
			zest_AddVertexAttribute(pipeline_template, 0, 1, zest_format_r32g32b32a32_sfloat, 16);
			zest_AddVertexAttribute(pipeline_template, 0, 2, zest_format_r8g8b8a8_unorm, 32);
			zest_command_list_t command_list = colour_command_list(context, 0);
			failed |= zest_GetPipeline(pipeline_template, &command_list) == NULL;
			zest_FreePipelineTemplate(pipeline_template);
			zest_DestroyContext(context);
		}
		failed |= ZEST_DX_BACKEND(device, device)->dxc_compiler != NULL;
		zest_FreeShader(vertex_shader);
		zest_FreeShader(pixel_shader);
		zest__dx_forced_dxc_library = NULL;
		failed |= finish_device(device);
	}
	zest__dx_forced_dxc_library = NULL;
	free(vertex_binary);
	free(pixel_binary);
	return failed;
}

struct LazyBindingDraws {
	VertexDrawResources resources;
	zest_pipeline_template wide_template;		//The vertex draw's pipeline with a stride of 24
	zest_pipeline_template no_push_template;
	zest_uint stride_before;
	zest_uint stride_after;
	zest_pipeline pipeline_after_blit;			//What the pass after the blit found bound, NULL when it started a new list
	ID3D12PipelineState *recorded_state_after_blit;
};

//Pushes, buffers, viewport and scissor all come before the pipeline they're for
static void lazy_order_task(const zest_command_list command_list, void *user_data) {
	LazyBindingDraws *draws = (LazyBindingDraws *)user_data;
	float tint[4] = { 1.f, 0.5f, 1.f, 1.f };
	zest_cmd_SendPushConstants(command_list, tint, sizeof(tint));
	zest_cmd_BindVertexBuffer(command_list, 0, 1, draws->resources.vertex_buffer);
	zest_cmd_BindIndexBuffer(command_list, draws->resources.index_buffer);
	set_full_viewport(command_list);
	zest_pipeline pipeline = zest_GetPipeline(draws->resources.pipeline_template, command_list);
	zest_pipeline wide_pipeline = zest_GetPipeline(draws->wide_template, command_list);
	if (!pipeline || !wide_pipeline) return;
	zest_cmd_BindPipeline(command_list, pipeline);
	zest_cmd_DrawIndexed(command_list, 6, 1, 0, 0, 0);
	zest_dx_bind_state_t *state = &ZEST_DX_BACKEND(command_list, command_list)->bind_state;
	draws->stride_before = state->recorded_vertex_strides[0];
	//Vertices 0, 2 and 4, a triangle inside the left quad, with no new vertex buffer bind
	zest_cmd_BindPipeline(command_list, wide_pipeline);
	zest_cmd_Draw(command_list, 3, 1, 0, 0);
	draws->stride_after = state->recorded_vertex_strides[0];
}

static void lazy_no_push_task(const zest_command_list command_list, void *user_data) {
	LazyBindingDraws *draws = (LazyBindingDraws *)user_data;
	char push_data[ZEST_MAX_PUSH_SIZE];
	memset(push_data, 0x3F, sizeof(push_data));
	zest_cmd_SendPushConstants(command_list, push_data, sizeof(push_data));
	set_full_viewport(command_list);
	zest_pipeline pipeline = zest_GetPipeline(draws->no_push_template, command_list);
	if (!pipeline) return;
	zest_cmd_BindPipeline(command_list, pipeline);
	zest_cmd_Draw(command_list, 3, 1, 0, 0);
}

static void lazy_blit_task(const zest_command_list command_list, void *user_data) {
	zest_resource_node source = zest_GetPassInputResource(command_list, "Blit Source");
	zest_resource_node destination = zest_GetPassOutputResource(command_list, "Blit Destination");
	zest_cmd_BlitImageMip(command_list, source, destination, 0, zest_shader_fragment_stage);
}

//Tinted unlike the draw before the blit, so a draw that ran on the blit's pipeline state, copying that target, shows
static void lazy_after_blit_task(const zest_command_list command_list, void *user_data) {
	LazyBindingDraws *draws = (LazyBindingDraws *)user_data;
	draws->pipeline_after_blit = ZEST_DX_BACKEND(command_list, command_list)->bind_state.pipeline;
	draws->recorded_state_after_blit = ZEST_DX_BACKEND(command_list, command_list)->bind_state.recorded_pipeline_state;
	set_full_viewport(command_list);
	zest_cmd_BindVertexBuffer(command_list, 0, 1, draws->resources.vertex_buffer);
	zest_cmd_BindIndexBuffer(command_list, draws->resources.index_buffer);
	zest_pipeline pipeline = zest_GetPipeline(draws->resources.pipeline_template, command_list);
	if (!pipeline) return;
	zest_cmd_BindPipeline(command_list, pipeline);
	float left_tint[4] = { 0.5f, 1.f, 1.f, 1.f };
	zest_cmd_SendPushConstants(command_list, left_tint, sizeof(left_tint));
	zest_cmd_DrawIndexed(command_list, 6, 1, 0, 0, 0);
	float right_tint[4] = { 1.f, 0.f, 1.f, 1.f };
	zest_cmd_SendPushConstants(command_list, right_tint, sizeof(right_tint));
	zest_cmd_DrawIndexed(command_list, 6, 1, 0, 4, 0);
}

static void lazy_no_pipeline_task(const zest_command_list command_list, void *user_data) {
	set_full_viewport(command_list);
	zest_cmd_Draw(command_list, 3, 1, 0, 0);
}

static void lazy_mip_copy_task(const zest_command_list command_list, void *user_data) {
	zest_resource_node source = zest_GetPassInputResource(command_list, "Copy Source");
	zest_resource_node target = zest_GetPassOutputResource(command_list, "Copy Target");
	zest_cmd_CopyImageMip(command_list, source, target, 0, zest_shader_fragment_stage);
}

//A draw, a blit that sets its own root signature and pipeline state, then a draw into another target. The blit is in a compute
//pass, as a transfer pass records its image outputs as reads, which would leave nothing ordering the second draw after it.
static int run_blit_between_draws(zest_context context, LazyBindingDraws *draws, zest_image first_target, zest_image converted, zest_image second_target) {
	if (!zest_BeginCommandGraph(context, "Lazy Blit", 0)) return 1;
	zest_resource_node first = zest_ImportImageResource("Blit Source", first_target, 0);
	zest_resource_node converted_node = zest_ImportImageResource("Blit Destination", converted, 0);
	zest_resource_node second = zest_ImportImageResource("Second Target", second_target, 0);
	zest_SetResourceClearColor(first, 0.f, 0.f, 0.f, 1.f);
	zest_SetResourceClearColor(second, 0.f, 0.f, 0.f, 1.f);
	zest_BeginRenderPass("Draw Before Blit");
	zest_ConnectOutput(first);
	zest_SetPassTask(vertex_draw_task, &draws->resources);
	zest_EndPass();
	zest_BeginComputePass("Blit Between Draws");
	zest_ConnectInput(first);
	zest_ConnectOutput(converted_node);
	zest_SetPassTask(lazy_blit_task, NULL);
	zest_EndPass();
	zest_BeginRenderPass("Draw After Blit");
	zest_ConnectInput(converted_node);
	zest_ConnectOutput(second);
	zest_SetPassTask(lazy_after_blit_task, draws);
	zest_EndPass();
	zest_frame_graph frame_graph = zest_EndFrameGraph();
	return frame_graph == NULL || zest_FlushFrameGraphAndWait(frame_graph) != zest_semaphore_status_success;
}

static int run_mip_copy_in_pass(zest_context context, zest_image source, zest_image target) {
	if (!zest_BeginCommandGraph(context, "Lazy Mip Copy", 0)) return 1;
	zest_resource_node source_node = zest_ImportImageResource("Copy Source", source, 0);
	zest_resource_node target_node = zest_ImportImageResource("Copy Target", target, 0);
	zest_SetResourceClearColor(target_node, 0.f, 0.f, 0.f, 1.f);
	zest_BeginRenderPass("Mip Copy In Pass");
	zest_ConnectInput(source_node);
	zest_ConnectOutput(target_node);
	zest_SetPassTask(lazy_mip_copy_task, NULL);
	zest_EndPass();
	zest_frame_graph frame_graph = zest_EndFrameGraph();
	return frame_graph == NULL || zest_FlushFrameGraphAndWait(frame_graph) != zest_semaphore_status_success;
}

int test_d3d12_lazy_binding_order(void) {
	zest_device device = zest_EndDeviceBuilder(BeginD3D12TestBuilder());
	if (!device) return 1;
	zest_create_context_info_t info = zest_CreateContextInfo();
	zest_context context = zest_CreateHeadlessContext(device, &info);
	if (!context) return 1 | finish_device(device);
	LazyBindingDraws draws = {};
	int failed = !create_vertex_draw_resources(device, &draws.resources);
	draws.wide_template = zest_CopyPipelineTemplate("Lazy Wide", draws.resources.pipeline_template);
	draws.wide_template->binding_descriptions[0].stride = 24;
	zest_shader_handle no_push_vertex = zest_CreateShader(device, hlsl_vertex_source, zest_vertex_shader, "lazy_no_push_vertex", NULL, ZEST_TRUE);
	zest_shader_handle no_push_pixel = zest_CreateShader(device, hlsl_pixel_source, zest_fragment_shader, "lazy_no_push_pixel", NULL, ZEST_TRUE);
	draws.no_push_template = no_input_template(device, "Lazy No Push", no_push_vertex, no_push_pixel);
	zest_image_handle target_handle = create_draw_target(device);
	zest_image_handle second_handle = create_draw_target(device);
	zest_image_handle source_handle = create_draw_target(device);
	zest_image_info_t copy_target_info = zest_CreateImageInfo(16, 16);
	copy_target_info.format = zest_format_r8g8b8a8_unorm;
	copy_target_info.flags = zest_image_preset_color_attachment | zest_image_flag_transfer_dst;
	zest_image_handle copy_target_handle = zest_CreateImage(device, &copy_target_info);
	zest_image_info_t converted_info = zest_CreateImageInfo(16, 16);
	converted_info.format = zest_format_r16g16b16a16_sfloat;
	converted_info.flags = zest_image_preset_storage | zest_image_flag_color_attachment;
	zest_image_handle converted_handle = zest_CreateImage(device, &converted_info);
	zest_image target = zest_GetImage(target_handle);
	zest_image second = zest_GetImage(second_handle);
	zest_image source = zest_GetImage(source_handle);
	zest_image converted = zest_GetImage(converted_handle);
	zest_image copy_target = zest_GetImage(copy_target_handle);
	failed |= !target || !second || !source || !converted || !copy_target;
	const int black[4] = { 0, 0, 0, 255 };
	const int no_push_colour[4] = { 255, 128, 64, 255 };
	zest_uint pixels[256];
	if (!failed) {
		//Order, and a stride change with no new vertex buffer bind
		failed |= render_draw_pass(context, "Lazy Order", target, ZEST_FALSE, lazy_order_task, &draws, pixels);
		failed |= !columns_hold(pixels, 0, 8, vertex_draw_left) || !columns_hold(pixels, 8, 8, black);
		ZEST_PRINT("\tVertex stride %u before the second pipeline's draw and %u after", draws.stride_before, draws.stride_after);
		failed |= draws.stride_before != 12 || draws.stride_after != 24;

		//No push block, after 128 bytes were pushed
		failed |= render_draw_pass(context, "Lazy No Push", target, ZEST_FALSE, lazy_no_push_task, &draws, pixels);
		failed |= !columns_hold(pixels, 0, 16, no_push_colour);
		failed |= zest_GetValidationErrorCount(device) != 0;

		//After a blit
		failed |= run_blit_between_draws(context, &draws, target, converted, second);
		const int after_blit_left[4] = { 128, 128, 0, 255 };
		const int after_blit_right[4] = { 255, 0, 0, 255 };
		failed |= !zest_CopyImageToBitmap(device, second, pixels) || !columns_hold(pixels, 0, 8, after_blit_left) || !columns_hold(pixels, 8, 8, after_blit_right);
		ZEST_PRINT("\tThe draw after the blit %s the blit's command list and found %s pipeline state recorded", draws.pipeline_after_blit ? "shared" : "didn't share", draws.recorded_state_after_blit ? "a" : "no");
		failed |= draws.pipeline_after_blit == NULL || draws.recorded_state_after_blit != NULL;
		failed |= zest_GetValidationErrorCount(device) != 0;

		//No pipeline
		zest_uint error_count = zest_GetValidationErrorCount(device);
		failed |= render_draw_pass(context, "Lazy No Pipeline", target, ZEST_FALSE, lazy_no_pipeline_task, NULL, pixels);
		failed |= zest_GetValidationErrorCount(device) != error_count + 1 || !columns_hold(pixels, 0, 16, black);

		//Mip copy in a render pass
		error_count = zest_GetValidationErrorCount(device);
		failed |= run_mip_copy_in_pass(context, source, copy_target);
		failed |= zest_GetValidationErrorCount(device) != error_count + 1;
		zest_ResetValidationErrors(device);
	}
	if (target) zest_FreeImageNow(target_handle);
	if (second) zest_FreeImageNow(second_handle);
	if (source) zest_FreeImageNow(source_handle);
	if (converted) zest_FreeImageNow(converted_handle);
	if (copy_target) zest_FreeImageNow(copy_target_handle);
	zest_FreePipelineTemplate(draws.wide_template);
	zest_FreePipelineTemplate(draws.no_push_template);
	zest_FreeShader(no_push_vertex);
	zest_FreeShader(no_push_pixel);
	free_vertex_draw_resources(&draws.resources);
	zest_DestroyContext(context);
	return failed | finish_device(device);
}

//The table keeps the two push registers over 64 DWORDs, and the read behind it never runs
static const char *fallback_vertex_source =
	"cbuffer placement_block : register(b0, space0) { float4 placement; };\n"
	"ByteAddressBuffer zest_buffers[] : register(t0, space105);\n"
	"static const float2 corners[6] = { float2(0, 0), float2(1, 0), float2(0, 1), float2(1, 0), float2(1, 1), float2(0, 1) };\n"
	"float4 main(uint vertex_index : SV_VertexID) : SV_Position {\n"
	"	float4 position = float4((placement.xy + corners[vertex_index]) / 8.0 - 1.0, 0.0, 1.0);\n"
	"	if (placement.w > 1000.0) position.z = asfloat(zest_buffers[NonUniformResourceIndex(uint(placement.z))].Load(0));\n"
	"	return position;\n"
	"}\n";
static const char *fallback_pixel_source =
	"cbuffer colour_block : register(b1, space0) { float4 unused_placement; float4 colour; };\n"
	"float4 main() : SV_Target0 { return colour; }\n";

#define FALLBACK_DRAW_COUNT 300

struct FallbackPush {
	float placement[4];		//The pixel's x and y
	float colour[4];
};

struct FallbackDraws {
	zest_pipeline_template pipeline_template;
	zest_bool push_as_cbv;
	zest_uint block_count;
	zest_uint fif_block_counts[ZEST_MAX_FIF];		//The chain of each frame in flight after its last frame
	zest_uint fif_frame_counts[ZEST_MAX_FIF];
};

static void fallback_task(const zest_command_list command_list, void *user_data) {
	FallbackDraws *draws = (FallbackDraws *)user_data;
	set_full_viewport(command_list);
	zest_pipeline pipeline = zest_GetPipeline(draws->pipeline_template, command_list);
	if (!pipeline) return;
	zest_cmd_BindPipeline(command_list, pipeline);
	for (zest_uint draw = 0; draw != FALLBACK_DRAW_COUNT; ++draw) {
		zest_uint pixel = draw % 256;
		FallbackPush push = {};
		push.placement[0] = (float)(pixel % 16);
		push.placement[1] = (float)(pixel / 16);
		push.colour[0] = (float)(draw % 256) / 255.f;
		push.colour[1] = (float)(draw / 256 * 100) / 255.f;
		push.colour[2] = 7.f / 255.f;
		push.colour[3] = 1.f;
		zest_cmd_SendPushConstants(command_list, &push, sizeof(push));
		zest_cmd_Draw(command_list, 6, 1, 0, 0);
	}
	draws->push_as_cbv = ZEST_DX_BACKEND(pipeline, pipeline)->root_parameters.push_as_cbv;
	zest_dx_context_queue_backend_t *queue_backend = ZEST_DX_BACKEND(context_queue, ZEST_DX_BACKEND(command_list, command_list)->context_queue);
	zest_uint fif = command_list->context->current_fif;
	draws->block_count = zest_vec_size(queue_backend->push_uploads[fif].blocks);
	draws->fif_block_counts[fif] = draws->block_count;
	draws->fif_frame_counts[fif]++;
}

//Each pixel holds the colour of the last draw placed on it
static zest_uint count_fallback_wrong_pixels(const zest_uint *pixels) {
	zest_uint wrong_pixels = 0;
	for (zest_uint pixel = 0; pixel != 256; ++pixel) {
		zest_uint last_draw = pixel + 256 < FALLBACK_DRAW_COUNT ? pixel + 256 : pixel;
		zest_uint expected = (last_draw % 256) | ((last_draw / 256 * 100) << 8) | (7u << 16) | (255u << 24);
		if (pixels[pixel] != expected) {
			if (!wrong_pixels) ZEST_PRINT("\tPixel %u is 0x%08X, expected 0x%08X", pixel, pixels[pixel], expected);
			wrong_pixels++;
		}
	}
	return wrong_pixels;
}

int test_d3d12_push_constant_fallback(void) {
	zest_device device = zest_EndDeviceBuilder(BeginD3D12TestBuilder());
	if (!device) return 1;
	zest_create_context_info_t info = zest_CreateContextInfo();
	zest_context context = zest_CreateHeadlessContext(device, &info);
	if (!context) return 1 | finish_device(device);
	zest_shader_handle vertex_shader = zest_CreateShader(device, fallback_vertex_source, zest_vertex_shader, "fallback_vertex", NULL, ZEST_TRUE);
	zest_shader_handle pixel_shader = zest_CreateShader(device, fallback_pixel_source, zest_fragment_shader, "fallback_pixel", NULL, ZEST_TRUE);
	FallbackDraws draws = {};
	draws.pipeline_template = no_input_template(device, "Push Fallback", vertex_shader, pixel_shader);
	zest_image_handle image_handle = create_draw_target(device);
	zest_image image = zest_GetImage(image_handle);
	int failed = image == NULL;
	if (!failed) {
		zest_uint pixels[256];
		failed |= render_draw_pass(context, "Push Fallback", image, ZEST_FALSE, fallback_task, &draws, pixels);
		zest_uint wrong_pixels = count_fallback_wrong_pixels(pixels);
		ZEST_PRINT("\tRoot CBV pushes: %s, %u wrong pixels, %u upload blocks", draws.push_as_cbv ? "yes" : "no", wrong_pixels, draws.block_count);
		failed |= !draws.push_as_cbv || wrong_pixels != 0 || draws.block_count != 2;
		zest_FreeImageNow(image_handle);
	}
	zest_FreePipelineTemplate(draws.pipeline_template);
	zest_FreeShader(vertex_shader);
	zest_FreeShader(pixel_shader);
	zest_DestroyContext(context);
	return failed | finish_device(device);
}

#define IMMEDIATE_COMPUTE_ELEMENTS 64

//buffer_write stores (x + added, 0, 0, 0) at element x for the 64 threads across of an 8x1x1 dispatch
static int run_immediate_write(zest_device device, zest_device_queue_type queue_type, zest_compute compute, zest_buffer buffer, zest_uint buffer_index, float added) {
	memset(zest_BufferData(buffer), 0xFF, IMMEDIATE_COMPUTE_ELEMENTS * 16);
	zest_queue queue = zest_imm_BeginCommandBuffer(device, queue_type);
	if (!queue) return 1;
	TestPushConstants push = {};
	push.index1 = (int)buffer_index;
	zest_imm_SendPushConstants(queue, &push, sizeof(push));
	zest_imm_BindComputePipeline(queue, compute);
	int failed = !zest_imm_DispatchCompute(queue, 8, 1, 1);
	failed |= !zest_imm_EndCommandBuffer(queue);
	const float *values = (const float *)zest_BufferData(buffer);
	for (zest_uint element = 0; element != IMMEDIATE_COMPUTE_ELEMENTS; ++element) {
		const float *value = &values[element * 4];
		if (value[0] != (float)element + added || value[1] != 0.f || value[2] != 0.f || value[3] != 0.f) {
			ZEST_PRINT("\tElement %u holds (%f, %f, %f, %f), expected %f", element, value[0], value[1], value[2], value[3], (float)element + added);
			return 1;
		}
	}
	return failed;
}

int test_d3d12_immediate_compute(void) {
	zest_device device = zest_EndDeviceBuilder(BeginD3D12TestBuilder());
	if (!device) return 1;
	zest_shader_handle shader = zest_CreateShaderFromFile(device, D3D12_HLSL_SHADER_PATH "buffer_write.comp.hlsl", "immediate_buffer_write", zest_compute_shader, NULL, ZEST_TRUE);
	zest_compute_handle compute_handle = zest_CreateCompute(device, "Immediate Buffer Write", shader);
	zest_compute compute = compute_handle.value ? zest_GetCompute(compute_handle) : NULL;
	zest_buffer_info_t buffer_info = zest_CreateBufferInfo(zest_buffer_type_storage, zest_memory_usage_gpu_to_cpu);
	zest_buffer buffer = zest_CreateBuffer(device, IMMEDIATE_COMPUTE_ELEMENTS * 16, &buffer_info);
	int failed = compute == NULL || buffer == NULL;
	zest_uint buffer_index = buffer ? zest_AcquireStorageBufferIndex(device, buffer) : ZEST_INVALID;
	if (!failed) {
		//Dispatch, on the graphics queue and then the compute queue
		failed |= run_immediate_write(device, zest_queue_graphics, compute, buffer, buffer_index, 0.f);
		failed |= run_immediate_write(device, zest_queue_compute, compute, buffer, buffer_index, 0.f);
		failed |= zest_GetValidationErrorCount(device) != 0;

		//Nothing bound
		zest_uint error_count = zest_GetValidationErrorCount(device);
		zest_queue queue = zest_imm_BeginCommandBuffer(device, zest_queue_graphics);
		failed |= queue == NULL || zest_imm_DispatchCompute(queue, 1, 1, 1) != ZEST_FALSE;
		failed |= queue == NULL || !zest_imm_EndCommandBuffer(queue);
		failed |= zest_GetValidationErrorCount(device) != error_count + 1;

		//Transfer queue, when it's a copy list
		queue = zest_imm_BeginCommandBuffer(device, zest_queue_transfer);
		failed |= queue == NULL;
		if (queue && ZEST_DX_BACKEND(queue, queue)->type == D3D12_COMMAND_LIST_TYPE_COPY) {
			error_count = zest_GetValidationErrorCount(device);
			zest_imm_BindComputePipeline(queue, compute);
			failed |= zest_imm_DispatchCompute(queue, 8, 1, 1) != ZEST_FALSE;
			failed |= zest_GetValidationErrorCount(device) < error_count + 1;
		} else {
			ZEST_PRINT("\tThe transfer queue phase was skipped, as the queue given is more capable than a copy queue");
		}
		if (queue) failed |= !zest_imm_EndCommandBuffer(queue);

		//Hot reload, through the same compute handle
		const char *hot_path = "zest_hot_reload_compute.hlsl";
		zest_file original = zest_ReadEntireFile(device, D3D12_HLSL_SHADER_PATH "buffer_write.comp.hlsl", ZEST_TRUE);
		const char *write_value = original ? strstr(original, "float(index)") : NULL;
		failed |= write_value == NULL || !macro_test__write_file(hot_path, original, 0);
		if (write_value) {
			zest_shader_handle hot_shader = zest_CreateShaderFromFile(device, hot_path, "hot_buffer_write", zest_compute_shader, NULL, ZEST_TRUE);
			zest_SetShaderHotReload(hot_shader, ZEST_TRUE);
			zest_compute_handle hot_handle = zest_CreateCompute(device, "Hot Buffer Write", hot_shader);
			zest_compute hot_compute = hot_handle.value ? zest_GetCompute(hot_handle) : NULL;
			failed |= hot_compute == NULL;
			if (hot_compute) {
				failed |= run_immediate_write(device, zest_queue_compute, hot_compute, buffer, buffer_index, 0.f);
				char edited[4096];
				snprintf(edited, sizeof(edited), "%.*sfloat(index + 1000)%s", (int)(write_value - original), original, write_value + strlen("float(index)"));
				failed |= !macro_test__write_file(hot_path, edited, zest_GetShader(hot_shader)->last_mtime);
				zest_uint reload_count = zest_CheckShaderHotReload(device);
				ZEST_PRINT("\t%u shader reloaded", reload_count);
				failed |= reload_count != 1;
				failed |= run_immediate_write(device, zest_queue_compute, hot_compute, buffer, buffer_index, 1000.f);
				zest_FreeCompute(hot_handle);
			}
			zest_FreeShader(hot_shader);
		}
		remove(hot_path);
		if (original) zest_FreeFile(device, original);
		zest_ResetValidationErrors(device);
	}
	if (buffer) zest_FreeBufferNow(buffer);
	if (compute) zest_FreeCompute(compute_handle);
	zest_FreeShader(shader);
	return failed | finish_device(device);
}

static const char *reads_descriptors_source =
	"cbuffer zest_push : register(b0, space0) {\n"
	"	uint raw_index;\n"
	"	uint structured_index;\n"
	"	uint uniform_index;\n"
	"	uint image_index;\n"
	"	uint result_index;\n"
	"	uint raw_word_count;\n"
	"	uint structured_count;\n"
	"};\n"
	"struct structured_element { uint index; uint3 padding0; float4 padding1; float4 padding2; };\n"
	"struct four_floats { float4 values; };\n"
	"ByteAddressBuffer zest_raw_buffers[] : register(t0, space105);\n"
	"StructuredBuffer<structured_element> zest_structured_buffers[] : register(t0, space109);\n"
	"ConstantBuffer<four_floats> zest_uniforms[] : register(b0, space107);\n"
	"RWTexture2D<float4> zest_images[] : register(u0, space106);\n"
	"RWByteAddressBuffer zest_results[] : register(u0, space105);\n"
	"[numthreads(8, 8, 1)]\n"
	"void main(uint3 thread_id : SV_DispatchThreadID) {\n"
	"	zest_images[NonUniformResourceIndex(image_index)][thread_id.xy] = float4(0.25, 0.5, 0.75, 1.0);\n"
	"	if (thread_id.x != 0 || thread_id.y != 0) return;\n"
	"	uint raw_sum = 0;\n"
	"	for (uint word = 0; word != raw_word_count; ++word) raw_sum += zest_raw_buffers[NonUniformResourceIndex(raw_index)].Load(word * 4);\n"
	"	uint structured_sum = 0;\n"
	"	for (uint element = 0; element != structured_count; ++element) structured_sum += zest_structured_buffers[NonUniformResourceIndex(structured_index)][element].index;\n"
	"	zest_results[NonUniformResourceIndex(result_index)].Store4(0, uint4(raw_sum, structured_sum, 0, 0));\n"
	"	zest_results[NonUniformResourceIndex(result_index)].Store4(16, asuint(zest_uniforms[NonUniformResourceIndex(uniform_index)].values));\n"
	"}\n";

#define READS_RAW_WORD_COUNT 256
#define READS_STRUCTURED_COUNT 37

struct ReadsDescriptorsPush {
	zest_uint raw_index;
	zest_uint structured_index;
	zest_uint uniform_index;
	zest_uint image_index;
	zest_uint result_index;
	zest_uint raw_word_count;
	zest_uint structured_count;
};

struct ReadsDescriptorsTask {
	zest_compute compute;
	ReadsDescriptorsPush push;
};

static void reads_descriptors_task(const zest_command_list command_list, void *user_data) {
	ReadsDescriptorsTask *task = (ReadsDescriptorsTask *)user_data;
	zest_cmd_BindComputePipeline(command_list, task->compute);
	zest_cmd_SendPushConstants(command_list, &task->push, sizeof(task->push));
	zest_cmd_DispatchCompute(command_list, 1, 1, 1);
}

static int pixels_within(const zest_uint *pixels, zest_uint pixel_count, const int expected[4], int tolerance);

int test_d3d12_compute_reads_descriptors(void) {
	zest_device device = zest_EndDeviceBuilder(BeginD3D12TestBuilder());
	if (!device) return 1;
	zest_create_context_info_t info = zest_CreateContextInfo();
	zest_context context = zest_CreateHeadlessContext(device, &info);
	if (!context) return 1 | finish_device(device);
	zest_shader_handle shader = zest_CreateShader(device, reads_descriptors_source, zest_compute_shader, "reads_descriptors", NULL, ZEST_TRUE);
	zest_compute_handle compute_handle = zest_CreateCompute(device, "Reads Descriptors", shader);
	ReadsDescriptorsTask task = {};
	task.compute = compute_handle.value ? zest_GetCompute(compute_handle) : NULL;

	zest_buffer_info_t upload_info = zest_CreateBufferInfo(zest_buffer_type_storage, zest_memory_usage_cpu_to_gpu);
	zest_buffer_info_t structured_info = upload_info;
	structured_info.element_alignment = 48;
	zest_buffer_info_t uniform_info = zest_CreateBufferInfo(zest_buffer_type_uniform, zest_memory_usage_cpu_to_gpu);
	zest_buffer_info_t result_info = zest_CreateBufferInfo(zest_buffer_type_storage, zest_memory_usage_gpu_to_cpu);
	//The blockers move the raw and structured views' first elements past 0
	zest_buffer raw_blocker = zest_CreateBuffer(device, 300, &upload_info);
	zest_buffer raw_buffer = zest_CreateBuffer(device, READS_RAW_WORD_COUNT * 4, &upload_info);
	zest_buffer structured_blocker = zest_CreateBuffer(device, 300, &upload_info);
	zest_buffer structured_buffer = zest_CreateBuffer(device, 48 * READS_STRUCTURED_COUNT, &structured_info);
	zest_buffer uniform_buffer = zest_CreateBuffer(device, 256, &uniform_info);
	zest_buffer result_buffer = zest_CreateBuffer(device, 256, &result_info);
	zest_image_info_t image_info = zest_CreateImageInfo(16, 16);
	image_info.format = zest_format_r8g8b8a8_unorm;
	image_info.flags = zest_image_preset_storage | zest_image_flag_transfer_dst | zest_image_flag_transfer_src;
	image_info.mip_levels = 2;
	zest_image_handle image_handle = zest_CreateImage(device, &image_info);
	zest_image image = zest_GetImage(image_handle);
	int failed = task.compute == NULL || !raw_blocker || !raw_buffer || !structured_blocker || !structured_buffer || !uniform_buffer || !result_buffer || !image;
	if (!failed) {
		zest_uint expected_raw_sum = 0;
		zest_uint *raw_words = (zest_uint *)zest_BufferData(raw_buffer);
		for (zest_uint word = 0; word != READS_RAW_WORD_COUNT; ++word) {
			raw_words[word] = word * 3 + 1;
			expected_raw_sum += raw_words[word];
		}
		unsigned char *structured_bytes = (unsigned char *)zest_BufferData(structured_buffer);
		memset(structured_bytes, 0xCD, 48 * READS_STRUCTURED_COUNT);
		zest_uint expected_structured_sum = 0;
		for (zest_uint element = 0; element != READS_STRUCTURED_COUNT; ++element) {
			memcpy(structured_bytes + element * 48, &element, sizeof(element));
			expected_structured_sum += element;
		}
		const float uniform_values[4] = { 1.5f, -2.25f, 3.0f, 0.125f };
		memcpy(zest_BufferData(uniform_buffer), uniform_values, sizeof(uniform_values));
		memset(zest_BufferData(result_buffer), 0, 256);
		zest_uint upload[256];
		for (zest_uint pixel = 0; pixel != 256; ++pixel) upload[pixel] = 10u | (20u << 8) | (30u << 16) | (255u << 24);
		failed |= !zest_CopyBitmapToImage(device, upload, sizeof(upload), image, 16, 16);
		zest_image_view_array_handle view_array_handle = zest_CreateImageViewsPerMip(device, image);
		zest_image_view_array view_array = view_array_handle.value ? zest_GetImageViewArray(view_array_handle) : NULL;
		zest_uint *mip_indexes = view_array ? zest_AcquireImageMipIndexes(device, image, view_array, zest_storage_image_binding, zest_descriptor_type_storage_image) : NULL;
		failed |= zest_vec_size(mip_indexes) != 2;
		task.push.raw_index = zest_AcquireStorageBufferIndex(device, raw_buffer);
		task.push.structured_index = zest_AcquireStructuredBufferIndex(device, structured_buffer, 48);
		task.push.uniform_index = zest_AcquireUniformBufferIndex(device, uniform_buffer);
		task.push.image_index = mip_indexes ? mip_indexes[1] : 0;
		task.push.result_index = zest_AcquireStorageBufferIndex(device, result_buffer);
		task.push.raw_word_count = READS_RAW_WORD_COUNT;
		task.push.structured_count = READS_STRUCTURED_COUNT;
		if (!failed && zest_BeginCommandGraph(context, "Reads Descriptors", 0)) {
			zest_resource_node image_node = zest_ImportImageResource("Mip Image", image, 0);
			zest_BeginComputePass("Read Descriptors");
			zest_ConnectOutput(image_node);
			zest_SetPassTask(reads_descriptors_task, &task);
			zest_EndPass();
			zest_frame_graph frame_graph = zest_EndFrameGraph();
			failed |= frame_graph == NULL || zest_FlushFrameGraphAndWait(frame_graph) != zest_semaphore_status_success;
		} else {
			failed = 1;
		}
		const zest_uint *results = (const zest_uint *)zest_BufferData(result_buffer);
		float result_floats[4];
		memcpy(result_floats, results + 4, sizeof(result_floats));
		ZEST_PRINT("\tRaw sum %u (expected %u), structured sum %u (expected %u), uniform (%g, %g, %g, %g)", results[0], expected_raw_sum, results[1], expected_structured_sum, result_floats[0], result_floats[1], result_floats[2], result_floats[3]);
		failed |= results[0] != expected_raw_sum || results[1] != expected_structured_sum || memcmp(result_floats, uniform_values, sizeof(uniform_values)) != 0;
		zest_uint mip_pixels[64];
		memset(mip_pixels, 0, sizeof(mip_pixels));
		failed |= !zest__dx_read_subresource(device, image, 1, 0, mip_pixels);
		const int written[4] = { 64, 128, 191, 255 };
		failed |= !pixels_within(mip_pixels, 64, written, 1);
		zest_uint base_pixels[256];
		memset(base_pixels, 0, sizeof(base_pixels));
		failed |= !zest__dx_read_subresource(device, image, 0, 0, base_pixels);
		const int uploaded[4] = { 10, 20, 30, 255 };
		failed |= !pixels_within(base_pixels, 256, uploaded, 0);
		zest_ReleaseStorageBufferIndex(device, task.push.raw_index);
		zest_ReleaseStorageBufferIndex(device, task.push.structured_index);
		zest_ReleaseBindlessIndex(device, task.push.uniform_index, zest_uniform_buffer_binding);
		zest_ReleaseStorageBufferIndex(device, task.push.result_index);
		if (view_array) zest_FreeImageViewArrayNow(view_array_handle);
	}
	failed |= zest_GetValidationErrorCount(device) != 0;
	if (image) zest_FreeImageNow(image_handle);
	zest_buffer buffers[6] = { raw_blocker, raw_buffer, structured_blocker, structured_buffer, uniform_buffer, result_buffer };
	for (int buffer_index = 0; buffer_index != 6; ++buffer_index) {
		if (buffers[buffer_index]) zest_FreeBufferNow(buffers[buffer_index]);
	}
	if (task.compute) zest_FreeCompute(compute_handle);
	zest_FreeShader(shader);
	zest_DestroyContext(context);
	return failed | finish_device(device);
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

static zest_dx_frame_graph_buffer_barrier_t buffer_barrier_record(zest_resource_node node, zest_access_flags src_access, zest_access_flags dst_access, zest_pipeline_stage_flags src_stage, zest_pipeline_stage_flags dst_stage) {
	zest_dx_frame_graph_buffer_barrier_t record;
	record.resource = node;
	record.src_access = src_access;
	record.dst_access = dst_access;
	record.src_stage = src_stage;
	record.dst_stage = dst_stage;
	return record;
}

int test_d3d12_buffer_barrier_translation(void) {
	//Fake pool resources, which the translation only compares and copies
	zest_dx_device_memory_pool_backend_t pool_backends[2] = {};
	pool_backends[0].buffer = (ID3D12Resource *)(void *)&pool_backends[0];
	pool_backends[1].buffer = (ID3D12Resource *)(void *)&pool_backends[1];
	zest_device_memory_pool_t pools[2] = {};
	pools[0].backend = (zest_device_memory_pool_backend)&pool_backends[0];
	pools[1].backend = (zest_device_memory_pool_backend)&pool_backends[1];
	zest_buffer_t buffers[3] = {};
	buffers[0].memory_pool = &pools[0];
	buffers[1].memory_pool = &pools[1];
	buffers[2].memory_pool = &pools[1];
	zest_resource_node_t nodes[4] = {};
	for (int node_index = 0; node_index != 3; ++node_index) nodes[node_index].storage_buffer = &buffers[node_index];
	zest_dx_frame_graph_buffer_barrier_t records[2];
	D3D12_BUFFER_BARRIER barriers[2];
	int failed = 0;

	//Write then read in compute
	records[0] = buffer_barrier_record(&nodes[0], zest_access_shader_write_bit, zest_access_shader_read_bit, zest_pipeline_stage_compute_shader_bit, zest_pipeline_stage_compute_shader_bit);
	zest_uint count = zest__dx_translate_buffer_barriers(records, 1, barriers);
	failed |= count != 1 || barriers[0].pResource != pool_backends[0].buffer || barriers[0].Offset != 0 || barriers[0].Size != UINT64_MAX;
	failed |= barriers[0].AccessBefore != D3D12_BARRIER_ACCESS_UNORDERED_ACCESS || barriers[0].AccessAfter != (D3D12_BARRIER_ACCESS_SHADER_RESOURCE | D3D12_BARRIER_ACCESS_UNORDERED_ACCESS);
	failed |= barriers[0].SyncBefore != D3D12_BARRIER_SYNC_COMPUTE_SHADING || barriers[0].SyncAfter != D3D12_BARRIER_SYNC_COMPUTE_SHADING;

	//Upload then draw
	records[0] = buffer_barrier_record(&nodes[0], zest_access_transfer_write_bit, zest_access_vertex_attribute_read_bit, zest_pipeline_stage_transfer_bit, zest_pipeline_stage_vertex_input_bit);
	count = zest__dx_translate_buffer_barriers(records, 1, barriers);
	failed |= count != 1 || barriers[0].AccessBefore != D3D12_BARRIER_ACCESS_COPY_DEST || barriers[0].AccessAfter != D3D12_BARRIER_ACCESS_VERTEX_BUFFER;
	//The vertex input stage's index input scope is dropped, as the runtime rejects it beside a vertex buffer read alone (message 1331)
	failed |= barriers[0].SyncBefore != D3D12_BARRIER_SYNC_COPY || barriers[0].SyncAfter != D3D12_BARRIER_SYNC_VERTEX_SHADING;

	//First use, with nothing before to order, needs no barrier: SYNC_NONE would be invalid once another buffer of the pool was used in the list (message 1417)
	records[0] = buffer_barrier_record(&nodes[0], 0, zest_access_shader_read_bit, 0, zest_pipeline_stage_compute_shader_bit);
	failed |= zest__dx_translate_buffer_barriers(records, 1, barriers) != 0;

	//A stage with no access is an execution dependency
	records[0] = buffer_barrier_record(&nodes[0], 0, zest_access_shader_write_bit, zest_pipeline_stage_fragment_shader_bit, zest_pipeline_stage_compute_shader_bit);
	count = zest__dx_translate_buffer_barriers(records, 1, barriers);
	failed |= count != 1 || barriers[0].AccessBefore != D3D12_BARRIER_ACCESS_NO_ACCESS || barriers[0].SyncBefore != D3D12_BARRIER_SYNC_PIXEL_SHADING;

	//No backing
	records[0] = buffer_barrier_record(&nodes[3], zest_access_shader_write_bit, zest_access_shader_read_bit, zest_pipeline_stage_compute_shader_bit, zest_pipeline_stage_compute_shader_bit);
	failed |= zest__dx_translate_buffer_barriers(records, 1, barriers) != 0;

	//Shared pool
	records[0] = buffer_barrier_record(&nodes[1], zest_access_shader_write_bit, zest_access_shader_read_bit, zest_pipeline_stage_compute_shader_bit, zest_pipeline_stage_compute_shader_bit);
	records[1] = buffer_barrier_record(&nodes[2], zest_access_transfer_write_bit, zest_access_vertex_attribute_read_bit, zest_pipeline_stage_transfer_bit, zest_pipeline_stage_vertex_input_bit);
	count = zest__dx_translate_buffer_barriers(records, 2, barriers);
	failed |= count != 1 || barriers[0].pResource != pool_backends[1].buffer;
	failed |= barriers[0].AccessBefore != (D3D12_BARRIER_ACCESS_UNORDERED_ACCESS | D3D12_BARRIER_ACCESS_COPY_DEST);
	failed |= barriers[0].AccessAfter != (D3D12_BARRIER_ACCESS_SHADER_RESOURCE | D3D12_BARRIER_ACCESS_UNORDERED_ACCESS | D3D12_BARRIER_ACCESS_VERTEX_BUFFER);
	failed |= barriers[0].SyncBefore != (D3D12_BARRIER_SYNC_COMPUTE_SHADING | D3D12_BARRIER_SYNC_COPY);
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

//Text and a rectangle before each frame, which the frame's swapchain pass draws and then resets
static int run_overlay_frames(zest_context context, int frame_count) {
	int failed = 0;
	for (int frame = 0; frame != frame_count; ++frame) {
		zest_DrawDebugText(context, 4.0f, 4.0f, 0xFFFFFFFF, "Zest %d", frame);
		zest_DrawDebugRect(context, 4.0f, 16.0f, 64.0f, 8.0f, 0xCC000000);
		D3D12FrameTimes times;
		failed |= !run_windowed_clear_frame(context, &times);
		failed |= context->db_overlay.vertex_count != 0;
	}
	return failed;
}

int test_d3d12_debug_overlay(void) {
	D3D12TestWindow window;
	if (!open_test_window(&window, "D3D12 Debug Overlay")) {
		close_test_window(&window);
		return 1;
	}
	zest_device device = create_windowed_test_device(&window);
	if (!device) {
		close_test_window(&window);
		return 1;
	}
	zest_dx_device_backend_t *backend = ZEST_DX_BACKEND(device, device);
	zest_create_context_info_t info = zest_CreateContextInfo();
	info.flags |= zest_context_init_flag_debug_overlay;
	zest_context context = zest_CreateContext(device, &window.window_data, &info);
	int failed = context == NULL;
	if (context) {
		set_test_clear_color(context);
		failed |= run_overlay_frames(context, 8);
		failed |= !zest_PipelineIsValid(context->db_overlay.pipeline);
		ID3DBlob *vertex_blob = backend->overlay_vertex_shader;
		ID3DBlob *pixel_blob = backend->overlay_pixel_shader;
		failed |= vertex_blob == NULL || pixel_blob == NULL;
		failed |= zest_GetValidationErrorCount(device) != 0;
		//The reset destroys the context, and the blobs outlive it as they don't depend on the device
		zest_ResetDevice(device);
		failed |= backend->overlay_vertex_shader != vertex_blob || backend->overlay_pixel_shader != pixel_blob;
		context = zest_CreateContext(device, &window.window_data, &info);
		failed |= context == NULL;
		if (context) {
			set_test_clear_color(context);
			failed |= run_overlay_frames(context, 2);
			failed |= zest_GetValidationErrorCount(device) != 0;
			zest_DestroyContext(context);
		}
	}
	failed |= finish_device(device);
	close_test_window(&window);
	return failed;
}

//WKPDID_D3DDebugObjectNameW from d3dcommon.h, as Zest doesn't link dxguid.lib
static const GUID d3d12_test_debug_object_name = { 0x4cca5fd8, 0x921f, 0x42c8, { 0x85, 0x66, 0x70, 0xca, 0xf2, 0xa9, 0xb7, 0x41 } };

//The name SetName gave object, as UTF-8. Empty when it has none
static void read_object_name(ID3D12Object *object, char *out_name, int name_size) {
	out_name[0] = 0;
	if (!object) return;
	wchar_t wide_name[ZEST__DX_MARKER_NAME_LENGTH + 1] = {};
	UINT size = ZEST__DX_MARKER_NAME_LENGTH * sizeof(wchar_t);
	if (FAILED(object->GetPrivateData(d3d12_test_debug_object_name, &size, wide_name))) return;
	if (!WideCharToMultiByte(CP_UTF8, 0, wide_name, -1, out_name, name_size, NULL, NULL)) out_name[0] = 0;
}

static int check_object_name(ID3D12Object *object, const char *expected_name) {
	char name[ZEST__DX_MARKER_NAME_LENGTH];
	read_object_name(object, name, (int)sizeof(name));
	int failed = strcmp(name, expected_name) != 0;
	if (failed) ZEST_PRINT("\tExpected the name \"%s\", read \"%s\"", expected_name, name);
	return failed;
}

typedef struct d3d12_object_names_task_t {
	char transient_name[ZEST__DX_MARKER_NAME_LENGTH];
	int run_count;
} d3d12_object_names_task_t;

static void d3d12_object_names_pass(const zest_command_list command_list, void *user_data) {
	d3d12_object_names_task_t *task = (d3d12_object_names_task_t *)user_data;
	zest_image image = zest_GetResourceImage(zest_GetPassOutputResource(command_list, "Named Transient"));
	read_object_name((ID3D12Object *)ZEST_DX_BACKEND(image, image)->resource, task->transient_name, (int)sizeof(task->transient_name));
	task->run_count++;
}

//One frame that renders to a transient image named "Named Transient" and clears the swapchain
static void run_object_names_frame(zest_context context, d3d12_object_names_task_t *task) {
	SDL_Event event;
	while (SDL_PollEvent(&event)) {}
	zest_UpdateDevice(context->device);
	if (!zest_BeginFrame(context)) return;
	zest_frame_graph frame_graph = NULL;
	if (zest_BeginFrameGraph(context, "Object Names Frame", NULL)) {
		zest_ImportSwapchainResource();
		zest_image_resource_info_t image_info = { zest_format_r8g8b8a8_unorm };
		image_info.width = 16;
		image_info.height = 16;
		zest_resource_node transient = zest_AddTransientImageResource("Named Transient", &image_info);
		zest_FlagResourceAsEssential(transient);
		zest_SetResourceClearColor(transient, 0.f, 0.f, 0.f, 1.f);
		zest_BeginRenderPass("Named Transient Pass");
		zest_ConnectOutput(transient);
		zest_SetPassTask(d3d12_object_names_pass, task);
		zest_EndPass();
		zest_BeginRenderPass("Clear Swapchain");
		zest_ConnectSwapChainOutput();
		zest_SetPassTask(zest_EmptyRenderPass, 0);
		zest_EndPass();
		frame_graph = zest_EndFrameGraph();
	}
	zest_EndFrame(context, frame_graph);
}

//Names given through the naming functions and the frame graph, and the backend's own, read back from the D3D12 objects
int test_d3d12_object_names(void) {
	D3D12TestWindow window;
	if (!open_test_window(&window, "D3D12 Object Names")) {
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
		d3d12_object_names_task_t task = {};
		for (int frame = 0; frame != 2; ++frame) {
			run_object_names_frame(context, &task);
		}
		failed |= task.run_count != 2;
		if (strcmp(task.transient_name, "Named Transient") != 0) {
			ZEST_PRINT("\tExpected the transient image to be named \"Named Transient\", read \"%s\"", task.transient_name);
			failed = 1;
		}
		failed |= check_object_name((ID3D12Object *)zest_GetD3D12CommandQueue(context), "Zest Direct Queue");
		zest_image_handle image_handle = create_draw_target(device);
		zest_image image = image_handle.value ? zest_GetImage(image_handle) : NULL;
		zest_buffer_info_t buffer_info = zest_CreateBufferInfo(zest_buffer_type_storage, zest_memory_usage_gpu_only);
		zest_buffer buffer = zest_CreateBuffer(device, 256, &buffer_info);
		failed |= image == NULL || buffer == NULL;
		if (image && buffer) {
			zest_SetImageName(device, image, "Named Image");
			zest_SetBufferPoolName(device, buffer->memory_pool, "Named Buffer Pool");
			failed |= check_object_name((ID3D12Object *)ZEST_DX_BACKEND(image, image)->resource, "Named Image");
			failed |= check_object_name((ID3D12Object *)ZEST_DX_BACKEND(device_memory_pool, buffer->memory_pool)->buffer, "Named Buffer Pool");
		}
		if (buffer) zest_FreeBufferNow(buffer);
		if (image_handle.value) zest_FreeImageNow(image_handle);
		failed |= zest_GetValidationErrorCount(device) != 0;
		zest_DestroyContext(context);
	}
	failed |= finish_device(device);
	close_test_window(&window);
	return failed;
}

//One frame that draws the fallback pushes into target and clears the swapchain. fallback_task counts the frames that ran
static void run_fallback_frame(zest_context context, FallbackDraws *draws, zest_image target) {
	SDL_Event event;
	while (SDL_PollEvent(&event)) {}
	zest_UpdateDevice(context->device);
	if (!zest_BeginFrame(context)) return;
	zest_frame_graph frame_graph = NULL;
	if (zest_BeginFrameGraph(context, "Push Fallback Frame", NULL)) {
		zest_ImportSwapchainResource();
		zest_resource_node target_node = zest_ImportImageResource("Fallback Target", target, 0);
		zest_SetResourceClearColor(target_node, 0.f, 0.f, 0.f, 1.f);
		zest_BeginRenderPass("Fallback Draws");
		zest_ConnectOutput(target_node);
		zest_SetPassTask(fallback_task, draws);
		zest_EndPass();
		zest_BeginRenderPass("Clear Swapchain");
		zest_ConnectSwapChainOutput();
		zest_SetPassTask(zest_EmptyRenderPass, 0);
		zest_EndPass();
		frame_graph = zest_EndFrameGraph();
	}
	zest_EndFrame(context, frame_graph);
}

//Root CBV pushes over several frames, so each frame in flight's chain is reset and reused when its frame comes round again rather than grown
int test_d3d12_push_constant_fallback_frames(void) {
	D3D12TestWindow window;
	if (!open_test_window(&window, "D3D12 Push Constant Fallback Frames")) {
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
		zest_shader_handle vertex_shader = zest_CreateShader(device, fallback_vertex_source, zest_vertex_shader, "fallback_frames_vertex", NULL, ZEST_TRUE);
		zest_shader_handle pixel_shader = zest_CreateShader(device, fallback_pixel_source, zest_fragment_shader, "fallback_frames_pixel", NULL, ZEST_TRUE);
		FallbackDraws draws = {};
		draws.pipeline_template = no_input_template(device, "Push Fallback Frames", vertex_shader, pixel_shader);
		zest_image_handle image_handle = create_draw_target(device);
		zest_image image = zest_GetImage(image_handle);
		failed |= image == NULL;
		if (image) {
			for (int frame = 0; frame != ZEST_MAX_FIF * 3; ++frame) {
				run_fallback_frame(context, &draws, image);
			}
			zest_WaitForIdleDevice(device);
			zest_uint pixels[256];
			memset(pixels, 0, sizeof(pixels));
			failed |= !zest_CopyImageToBitmap(device, image, pixels);
			zest_uint wrong_pixels = count_fallback_wrong_pixels(pixels);
			ZEST_PRINT("\tRoot CBV pushes: %s, %u wrong pixels after the last frame", draws.push_as_cbv ? "yes" : "no", wrong_pixels);
			failed |= !draws.push_as_cbv || wrong_pixels != 0;
			for (zest_uint fif = 0; fif != ZEST_MAX_FIF; ++fif) {
				ZEST_PRINT("\tFrame in flight %u drew %u frames and ended with %u upload blocks", fif, draws.fif_frame_counts[fif], draws.fif_block_counts[fif]);
				failed |= draws.fif_frame_counts[fif] < 2 || draws.fif_block_counts[fif] != 2;
			}
			zest_FreeImageNow(image_handle);
		}
		failed |= zest_GetValidationErrorCount(device) != 0;
		zest_FreePipelineTemplate(draws.pipeline_template);
		zest_FreeShader(vertex_shader);
		zest_FreeShader(pixel_shader);
		zest_DestroyContext(context);
	}
	failed |= finish_device(device);
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
	int failed = !zest__dx_compile_hlsl(device, orientation_vertex_source, (zest_size)strlen(orientation_vertex_source), "orientation_vertex", "main", "vs_6_0", NULL, 0, NULL, &vertex_dxil);
	failed |= !zest__dx_compile_hlsl(device, orientation_pixel_source, (zest_size)strlen(orientation_pixel_source), "orientation_pixel", "main", "ps_6_0", NULL, 0, NULL, &pixel_dxil);
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

//One frame whose render pass, named "Removal Pass", clears the swapchain
static void run_removal_report_frame(zest_context context) {
	SDL_Event event;
	while (SDL_PollEvent(&event)) {}
	zest_UpdateDevice(context->device);
	if (!zest_BeginFrame(context)) return;
	zest_frame_graph frame_graph = NULL;
	if (zest_BeginFrameGraph(context, "Removal Report Frame", NULL)) {
		zest_ImportSwapchainResource();
		zest_BeginRenderPass("Removal Pass");
		zest_ConnectSwapChainOutput();
		zest_SetPassTask(zest_EmptyRenderPass, 0);
		zest_EndPass();
		frame_graph = zest_EndFrameGraph();
	}
	zest_EndFrame(context, frame_graph);
}

static zest_uint log_file_size(zest_device device) {
	zest_file log = zest_ReadEntireFile(device, device->log_path.str, ZEST_FALSE);
	zest_uint size = log ? zest_vec_size(log) : 0;
	if (log) zest_FreeFile(device, log);
	return size;
}

static int count_report_lines(const char *report, const char *line) {
	int count = 0;
	for (const char *found = strstr(report, line); found; found = strstr(found + 1, line)) count++;
	return count;
}

//Every line of the report, whose DRED part is empty as ID3D12Device5::RemoveDevice leaves no breadcrumbs or page fault
static int check_removal_report(const char *report) {
	const char *lines[] = { "D3D12 device removed: DXGI_ERROR_DEVICE_REMOVED (0x887A0005)", "DRED breadcrumbs: none recorded.", "DRED: no page fault." };
	int failed = 0;
	for (int line = 0; line != 3; ++line) {
		int count = count_report_lines(report, lines[line]);
		if (count != 1) {
			ZEST_PRINT("\tThe log has \"%s\" %d times after the removal, expected once", lines[line], count);
			failed = 1;
		}
	}
	return failed;
}

//The first place to see a removal writes one report to the log, and a new device on the adapter then runs clean
int test_d3d12_device_removal_report(void) {
	D3D12TestWindow window;
	if (!open_test_window(&window, "D3D12 Device Removal Report")) {
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
		for (int frame = 0; frame != 2; ++frame) run_removal_report_frame(context);
		zest_WaitForIdleDevice(device);
		failed |= zest_GetValidationErrorCount(device) != 0;
		zest_uint log_size = log_file_size(device);
		remove_test_device(device);
		for (int frame = 0; frame != 2; ++frame) run_removal_report_frame(context);
		failed |= !zest_ContextDeviceWasLost(context);
		//The removal itself raises no validation messages, on the AMD GPU or on WARP
		failed |= zest_GetValidationErrorCount(device) != 0;
		zest_file log = zest_ReadEntireFile(device, device->log_path.str, ZEST_TRUE);
		failed |= log == NULL || zest_vec_size(log) <= log_size;
		if (log && zest_vec_size(log) > log_size) {
			failed |= check_removal_report(log + log_size);
		}
		if (log) zest_FreeFile(device, log);
		zest_DestroyContext(context);
	}
	zest_ResetValidationErrors(device);
	zest_DestroyDevice(device);
	failed |= zest__dx_last_device_release_count() != 0;

	device = create_windowed_test_device(&window);
	failed |= device == NULL;
	if (device) {
		context = zest_CreateContext(device, &window.window_data, &info);
		failed |= context == NULL;
		if (context) {
			set_test_clear_color(context);
			for (int frame = 0; frame != 3; ++frame) run_removal_report_frame(context);
			zest_WaitForIdleDevice(device);
			failed |= zest_ContextDeviceWasLost(context);
			zest_DestroyContext(context);
		}
		failed |= finish_device(device);
	}
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

int test_d3d12_structured_buffer_descriptors(void) {
	zest_device device = zest_EndDeviceBuilder(BeginD3D12TestBuilder());
	if (!device) return 1;
	zest_buffer_info_t storage_info = zest_CreateBufferInfo(zest_buffer_type_storage, zest_memory_usage_gpu_only);
	zest_buffer_info_t stride_info = storage_info;
	stride_info.element_alignment = 48;
	//The blocker puts the structured view's first element past 0
	zest_buffer blocker = zest_CreateBuffer(device, 300, &storage_info);
	zest_buffer stride_buffer = zest_CreateBuffer(device, 48 * 37, &stride_info);
	int failed = !blocker || !stride_buffer;
	if (!failed) {
		ZEST_PRINT("\tThe stride 48 buffer is at offset %llu", (unsigned long long)stride_buffer->memory_offset);
		failed |= stride_buffer->memory_offset == 0 || stride_buffer->memory_offset % 48 != 0;
		zest_uint raw_index = zest_AcquireStorageBufferIndex(device, stride_buffer);
		zest_uint structured_index = zest_AcquireStructuredBufferIndex(device, stride_buffer, 48);
		failed |= raw_index == ZEST_INVALID || structured_index == ZEST_INVALID || raw_index == structured_index;
		failed |= zest_GetValidationErrorCount(device) != 0;
		zest_ReleaseStorageBufferIndex(device, raw_index);
		zest_ReleaseStorageBufferIndex(device, structured_index);
	}
	if (blocker) zest_FreeBufferNow(blocker);
	if (stride_buffer) zest_FreeBufferNow(stride_buffer);
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
	zest_resource_node source = zest_GetPassInputResource(command_list, "Mip Source");
	zest_resource_node destination = zest_GetPassOutputResource(command_list, "Mip Destination");
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
	//A first use with no previous occupant waits on nothing
	failed |= barrier.SyncBefore != D3D12_BARRIER_SYNC_NONE;
	barrier = zest__dx_subresource_barrier(&node.image, layout_before, D3D12_BARRIER_LAYOUT_RENDER_TARGET, record.src_access, record.dst_access, 0, record.dst_stage, 0, 1, 0, 1);
	failed |= barrier.SyncBefore != D3D12_BARRIER_SYNC_NONE;
	//An aliased transient's record carries the previous occupant's last write: UNDEFINED drops the access, and the sync waits for its stages
	record.src_access = zest_access_color_attachment_write_bit;
	record.src_stage = zest_pipeline_stage_color_attachment_output_bit;
	barrier = zest__dx_subresource_barrier(&node.image, zest__dx_record_layout_before(&record, &node.image), D3D12_BARRIER_LAYOUT_RENDER_TARGET, record.src_access, record.dst_access, record.src_stage, record.dst_stage, 0, 1, 0, 1);
	failed |= barrier.LayoutBefore != D3D12_BARRIER_LAYOUT_UNDEFINED;
	failed |= barrier.AccessBefore != D3D12_BARRIER_ACCESS_NO_ACCESS || barrier.SyncBefore != D3D12_BARRIER_SYNC_RENDER_TARGET;
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

//The third target's first barrier was compiled with the depth target's last write, and must have been sent with its stages but none of its access
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
			found &= barrier->AccessBefore == D3D12_BARRIER_ACCESS_NO_ACCESS && (barrier->SyncBefore & D3D12_BARRIER_SYNC_DEPTH_STENCIL) != 0;
			found &= barrier->SyncBefore == zest__dx_normalise_dependency_sync(zest__dx_barrier_sync(record->src_stage, ZEST_TRUE), D3D12_BARRIER_ACCESS_NO_ACCESS);
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

typedef struct d3d12_cross_queue_task_t {
	zest_compute compute;
	zest_buffer staging_buffer;
} d3d12_cross_queue_task_t;

static void d3d12_cross_queue_write_image(const zest_command_list command_list, void *user_data) {
	d3d12_cross_queue_task_t *task = (d3d12_cross_queue_task_t *)user_data;
	zest_resource_node image = zest_GetPassOutputResource(command_list, "Cross Image");
	TestPushConstants push = {};
	push.index1 = (int)zest_GetTransientSampledImageBindlessIndex(command_list, image, zest_storage_image_binding);
	zest_cmd_BindComputePipeline(command_list, task->compute);
	zest_cmd_SendPushConstants(command_list, &push, sizeof(push));
	zest_cmd_DispatchCompute(command_list, 2, 2, 1);
}

static void d3d12_cross_queue_copy_buffer(const zest_command_list command_list, void *user_data) {
	d3d12_cross_queue_task_t *task = (d3d12_cross_queue_task_t *)user_data;
	zest_resource_node buffer = zest_GetPassOutputResource(command_list, "Cross Buffer");
	zest_cmd_CopyBuffer(command_list, task->staging_buffer, buffer->storage_buffer, task->staging_buffer->size);
}

//The translated barrier of the record that hands the named image between queues, from the release or the acquire side
static const D3D12_TEXTURE_BARRIER *crossing_image_barrier(zest_frame_graph frame_graph, const char *name, zest_bool acquire) {
	for (zest_uint pass_index = 0; pass_index != zest_GetFrameGraphFinalPassCount(frame_graph); ++pass_index) {
		const zest_pass_group_t *pass_group = zest_GetFrameGraphFinalPass(frame_graph, pass_index);
		zest_dx_execution_barriers_backend_t *backend = ZEST_DX_BACKEND(execution_barriers, &pass_group->execution_details.barriers);
		zest_dx_frame_graph_image_barrier_t *records = acquire ? backend->acquire_images : backend->release_images;
		for (zest_uint index = 0; index != zest_vec_size(records); ++index) {
			if (strcmp(records[index].resource->name, name) != 0 || !zest__dx_crosses_queues(records[index].src_family, records[index].dst_family)) continue;
			return acquire ? &backend->acquire_translated[index] : &backend->release_translated[index];
		}
	}
	return NULL;
}

//The pass group on the queue type that holds a record on the named buffer, or NULL
static const zest_pass_group_t *buffer_record_group(zest_frame_graph frame_graph, const char *name, zest_bool acquire, zest_device_queue_type queue_type) {
	for (zest_uint pass_index = 0; pass_index != zest_GetFrameGraphFinalPassCount(frame_graph); ++pass_index) {
		const zest_pass_group_t *pass_group = zest_GetFrameGraphFinalPass(frame_graph, pass_index);
		if (pass_group->queue_info.queue_type != queue_type) continue;
		zest_dx_execution_barriers_backend_t *backend = ZEST_DX_BACKEND(execution_barriers, &pass_group->execution_details.barriers);
		zest_dx_frame_graph_buffer_barrier_t *records = acquire ? backend->acquire_buffers : backend->release_buffers;
		for (zest_uint index = 0; index != zest_vec_size(records); ++index) {
			if (strcmp(records[index].resource->name, name) == 0) return pass_group;
		}
	}
	return NULL;
}

//A compute pass and a copy pass share the first wave, so they run on the compute and copy queues, and a render pass on the direct queue reads both
int test_d3d12_cross_queue_barriers(void) {
	zest_device device = zest_EndDeviceBuilder(BeginD3D12TestBuilder());
	if (!device) return 1;
	zest_create_context_info_t info = zest_CreateContextInfo();
	zest_context context = zest_CreateHeadlessContext(device, &info);
	zest_shader_handle shader = zest_CreateShaderFromFile(device, D3D12_HLSL_SHADER_PATH "image_write2.comp.hlsl", "cross_queue_image_write", zest_compute_shader, NULL, ZEST_TRUE);
	zest_compute_handle compute_handle = zest_CreateCompute(device, "Cross Queue Image Write", shader);
	zest_uint words[256];
	for (zest_uint index = 0; index != 256; ++index) words[index] = index * 7u + 3u;
	d3d12_cross_queue_task_t task = {};
	task.compute = compute_handle.value ? zest_GetCompute(compute_handle) : NULL;
	task.staging_buffer = zest_CreateStagingBuffer(device, sizeof(words), words);
	zest_image_info_t target_info = zest_CreateImageInfo(16, 16);
	target_info.format = zest_format_r8g8b8a8_unorm;
	target_info.flags = zest_image_preset_color_attachment;
	zest_image_handle target_handle = zest_CreateImage(device, &target_info);
	int failed = context == NULL || task.compute == NULL || task.staging_buffer == NULL || !target_handle.value;
	if (!failed) {
		//The records outlive the flush only in a cached graph
		zest_uint cache_state = 1;
		zest_frame_graph_cache_key_t cache_key = zest_InitialiseCacheKey(context, &cache_state, sizeof(cache_state));
		failed |= !zest_BeginCommandGraph(context, "Cross Queue Barriers", &cache_key);
		zest_image_resource_info_t image_info = { zest_format_r8g8b8a8_unorm };
		image_info.width = 16;
		image_info.height = 16;
		zest_buffer_resource_info_t buffer_info = {};
		buffer_info.size = sizeof(words);
		zest_resource_node image = zest_AddTransientImageResource("Cross Image", &image_info);
		zest_resource_node buffer = zest_AddTransientBufferResource("Cross Buffer", &buffer_info);
		zest_resource_node target = zest_ImportImageResource("Cross Target", zest_GetImage(target_handle), 0);

		zest_BeginComputePass("Cross Compute");
		zest_ConnectOutput(image);
		zest_SetPassTask(d3d12_cross_queue_write_image, &task);
		zest_EndPass();

		zest_BeginTransferPass("Cross Transfer");
		zest_ConnectOutput(buffer);
		zest_SetPassTask(d3d12_cross_queue_copy_buffer, &task);
		zest_EndPass();

		zest_BeginRenderPass("Cross Render");
		zest_ConnectInput(image);
		zest_ConnectInput(buffer);
		zest_ConnectOutput(target);
		zest_SetPassTask(zest_EmptyRenderPass, 0);
		zest_EndPass();

		zest_frame_graph frame_graph = zest_EndFrameGraph();
		failed |= frame_graph == NULL || zest_GetFrameGraphResult(frame_graph) != zest_fgs_success;
		failed |= frame_graph == NULL || zest_FlushFrameGraphAndWait(frame_graph) != zest_semaphore_status_success;
		if (frame_graph) {
			const D3D12_TEXTURE_BARRIER *release = crossing_image_barrier(frame_graph, "Cross Image", ZEST_FALSE);
			failed |= release == NULL;
			if (release) {
				failed |= release->LayoutAfter != D3D12_BARRIER_LAYOUT_COMMON;
				failed |= release->AccessAfter != D3D12_BARRIER_ACCESS_NO_ACCESS || release->SyncAfter != D3D12_BARRIER_SYNC_NONE;
			}
			const D3D12_TEXTURE_BARRIER *acquire = crossing_image_barrier(frame_graph, "Cross Image", ZEST_TRUE);
			failed |= acquire == NULL;
			if (acquire) {
				failed |= acquire->LayoutBefore != D3D12_BARRIER_LAYOUT_COMMON || acquire->LayoutAfter != D3D12_BARRIER_LAYOUT_SHADER_RESOURCE;
				failed |= acquire->AccessBefore != D3D12_BARRIER_ACCESS_NO_ACCESS || acquire->SyncBefore != D3D12_BARRIER_SYNC_NONE;
			}
			zest_bool has_copy_group = ZEST_FALSE;
			for (zest_uint pass_index = 0; pass_index != zest_GetFrameGraphFinalPassCount(frame_graph); ++pass_index) {
				has_copy_group |= zest_GetFrameGraphFinalPass(frame_graph, pass_index)->queue_info.queue_type == zest_queue_transfer;
			}
			failed |= !has_copy_group;
			//The wave fence orders the buffer's handoff and ExecuteCommandLists flushes the copy list, so neither side recorded a barrier
			failed |= buffer_record_group(frame_graph, "Cross Buffer", ZEST_FALSE, zest_queue_transfer) != NULL;
			failed |= buffer_record_group(frame_graph, "Cross Buffer", ZEST_TRUE, zest_queue_graphics) != NULL;
			//zest_PrintCompiledFrameGraph reads the node lists beside shared code's debug records, crossings included
			for (zest_uint pass_index = 0; pass_index != zest_GetFrameGraphFinalPassCount(frame_graph); ++pass_index) {
				const zest_execution_barriers_t *barriers = &zest_GetFrameGraphFinalPass(frame_graph, pass_index)->execution_details.barriers;
				failed |= zest_vec_size(barriers->acquire_buffer_barrier_nodes) != zest_vec_size(barriers->acquire_buffer_barriers);
				failed |= zest_vec_size(barriers->release_buffer_barrier_nodes) != zest_vec_size(barriers->release_buffer_barriers);
				failed |= zest_vec_size(barriers->acquire_image_barrier_nodes) != zest_vec_size(barriers->acquire_image_barriers);
				failed |= zest_vec_size(barriers->release_image_barrier_nodes) != zest_vec_size(barriers->release_image_barriers);
			}
		}
		failed |= zest_GetValidationErrorCount(device) != 0;
	}
	failed |= zest__dx_layout_fits_queue(D3D12_BARRIER_LAYOUT_COMPUTE_QUEUE_COMMON, zest_queue_graphics) != ZEST_FALSE;
	failed |= zest__dx_layout_fits_queue(D3D12_BARRIER_LAYOUT_DIRECT_QUEUE_GENERIC_READ, zest_queue_compute) != ZEST_FALSE;
	failed |= zest__dx_layout_fits_queue(D3D12_BARRIER_LAYOUT_COMMON, zest_queue_transfer) != ZEST_TRUE;
	//A copy list can only start a barrier from COMMON, or UNDEFINED for a fresh image
	failed |= zest__dx_layout_fits_queue(D3D12_BARRIER_LAYOUT_UNDEFINED, zest_queue_transfer) != ZEST_TRUE;
	failed |= zest__dx_layout_fits_queue(D3D12_BARRIER_LAYOUT_SHADER_RESOURCE, zest_queue_transfer) != ZEST_FALSE;
	failed |= zest__dx_crosses_queues(ZEST_QUEUE_FAMILY_IGNORED, ZEST_QUEUE_FAMILY_IGNORED) || zest__dx_crosses_queues(1, 1) || !zest__dx_crosses_queues(1, 0);
	if (task.staging_buffer) zest_FreeBufferNow(task.staging_buffer);
	if (target_handle.value) zest_FreeImageNow(target_handle);
	if (context) zest_DestroyContext(context);
	return failed | finish_device(device);
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

typedef struct d3d12_indirect_draws_t {
	VertexDrawResources resources;
	zest_buffer commands;
	zest_uint stride;
} d3d12_indirect_draws_t;

static void d3d12_indirect_stride_task(const zest_command_list command_list, void *user_data) {
	d3d12_indirect_draws_t *draws = (d3d12_indirect_draws_t *)user_data;
	set_full_viewport(command_list);
	zest_cmd_BindVertexBuffer(command_list, 0, 1, draws->resources.vertex_buffer);
	zest_cmd_BindIndexBuffer(command_list, draws->resources.index_buffer);
	zest_pipeline pipeline = zest_GetPipeline(draws->resources.pipeline_template, command_list);
	if (!pipeline) return;
	zest_cmd_BindPipeline(command_list, pipeline);
	float tint[4] = { 1.f, 0.5f, 1.f, 1.f };
	zest_cmd_SendPushConstants(command_list, tint, sizeof(tint));
	zest_cmd_DrawIndexedIndirect(command_list, draws->commands, 0, 2, draws->stride);
}

static zest_uint d3d12_indirect_signature_count(zest_device device, zest_uint table) {
	zest_dx_device_backend_t *backend = ZEST_DX_BACKEND(device, device);
	zest_uint count = 0;
	for (zest_uint slot = 0; slot != ZEST_DX_MAX_INDIRECT_STRIDES; ++slot) {
		count += backend->indirect_signatures[table][slot].signature != NULL || backend->indirect_signatures[table][slot].stride != 0;
	}
	return count;
}

//One command signature per stride, a stride shorter than the command is refused, and a reset releases them all
int test_d3d12_indirect_signatures(void) {
	zest_device device = zest_EndDeviceBuilder(BeginD3D12TestBuilder());
	if (!device) return 1;
	zest_create_context_info_t info = zest_CreateContextInfo();
	zest_context context = zest_CreateHeadlessContext(device, &info);
	if (!context) return 1 | finish_device(device);
	d3d12_indirect_draws_t draws = {};
	int failed = !create_vertex_draw_resources(device, &draws.resources);
	zest_buffer_info_t indirect_info = zest_CreateBufferInfo(zest_buffer_type_indirect, zest_memory_usage_cpu_to_gpu);
	draws.commands = zest_CreateBuffer(device, 64, &indirect_info);
	zest_image_handle target_handle = create_draw_target(device);
	zest_image target = zest_GetImage(target_handle);
	failed |= !draws.commands || !target;
	if (!failed) {
		const zest_draw_indexed_indirect_command_t left = { 6, 1, 0, 0, 0 };
		const zest_draw_indexed_indirect_command_t right = { 6, 1, 0, 4, 0 };
		//Packed, then padded to 32 bytes
		const zest_uint strides[2] = { (zest_uint)sizeof(zest_draw_indexed_indirect_command_t), 32 };
		zest_uint pixels[256];
		for (int stride_index = 0; stride_index != 2; ++stride_index) {
			char *commands = (char *)zest_BufferData(draws.commands);
			memset(commands, 0, 64);
			memcpy(commands, &left, sizeof(left));
			memcpy(commands + strides[stride_index], &right, sizeof(right));
			draws.stride = strides[stride_index];
			failed |= render_draw_pass(context, "Indirect Stride", target, ZEST_FALSE, d3d12_indirect_stride_task, &draws, pixels);
			failed |= !columns_hold(pixels, 0, 16, vertex_draw_left);
		}
		failed |= d3d12_indirect_signature_count(device, 1) != 2 || d3d12_indirect_signature_count(device, 0) != 0;
		failed |= zest_GetValidationErrorCount(device) != 0;
		zest_uint error_count = zest_GetValidationErrorCount(device);
		draws.stride = 12;
		failed |= render_draw_pass(context, "Indirect Short Stride", target, ZEST_FALSE, d3d12_indirect_stride_task, &draws, pixels);
		const int black[4] = { 0, 0, 0, 255 };
		failed |= zest_GetValidationErrorCount(device) != error_count + 1 || !columns_hold(pixels, 0, 16, black);
		failed |= d3d12_indirect_signature_count(device, 1) != 2;
	}
	if (target) zest_FreeImageNow(target_handle);
	if (draws.commands) zest_FreeBufferNow(draws.commands);
	free_vertex_draw_resources(&draws.resources);
	zest_ResetValidationErrors(device);
	zest_ResetDevice(device);
	failed |= zest__dx_last_device_release_count() != 0;
	failed |= d3d12_indirect_signature_count(device, 0) != 0 || d3d12_indirect_signature_count(device, 1) != 0;
	return failed | finish_device(device);
}

//View 0 draws red and view 1 green
static const char *view_instancing_pixel_source =
	"float4 main(uint view : SV_ViewID) : SV_Target0 { return view == 0 ? float4(1.0, 0.0, 0.0, 1.0) : float4(0.0, 1.0, 0.0, 1.0); }\n";

static void d3d12_view_instancing_task(const zest_command_list command_list, void *user_data) {
	zest_pipeline_template pipeline_template = (zest_pipeline_template)user_data;
	set_full_viewport(command_list);
	zest_pipeline pipeline = zest_GetPipeline(pipeline_template, command_list);
	if (!pipeline) return;
	zest_cmd_BindPipeline(command_list, pipeline);
	zest_cmd_Draw(command_list, 3, 1, 0, 0);
}

static zest_image_handle create_layered_target(zest_device device, zest_uint layer_count) {
	zest_image_info_t image_info = zest_CreateImageInfo(16, 16);
	image_info.format = zest_format_r8g8b8a8_unorm;
	image_info.layer_count = layer_count;
	image_info.flags = zest_image_preset_color_attachment | zest_image_flag_transfer_src;
	return zest_CreateImage(device, &image_info);
}

//Draws into every layer of image in one render pass, and returns the graph's result
static zest_frame_graph_result run_view_instancing_pass(zest_context context, zest_image image, zest_pipeline_template pipeline_template) {
	if (!zest_BeginCommandGraph(context, "View Instancing", 0)) return zest_fgs_critical_error;
	zest_resource_node target = zest_ImportImageResource("Layered Target", image, 0);
	zest_SetResourceClearColor(target, 0.f, 0.f, 0.f, 1.f);
	zest_BeginRenderPass("View Instancing");
	zest_ConnectOutput(target);
	zest_SetPassTask(d3d12_view_instancing_task, pipeline_template);
	zest_EndPass();
	zest_frame_graph frame_graph = zest_EndFrameGraph();
	if (!frame_graph) return zest_fgs_critical_error;
	zest_FlushFrameGraphAndWait(frame_graph);
	return zest_GetFrameGraphResult(frame_graph);
}

//Two views render to the two layers of one target, a pipeline whose view count differs from its pass is refused, and a pass with more views than the device allows fails its graph
int test_d3d12_view_instancing(void) {
	zest_device device = zest_EndDeviceBuilder(BeginD3D12TestBuilder());
	if (!device) return 1;
	zest_create_context_info_t info = zest_CreateContextInfo();
	zest_context context = zest_CreateHeadlessContext(device, &info);
	if (!context) return 1 | finish_device(device);
	int failed = ZEST_DX_BACKEND(device, device)->support.view_instancing_tier == D3D12_VIEW_INSTANCING_TIER_NOT_SUPPORTED;
	zest_shader_handle vertex_shader = zest_CreateShader(device, depth_bias_vertex_hlsl, zest_vertex_shader, "view_instancing_vertex", NULL, ZEST_TRUE);
	zest_shader_handle pixel_shader = zest_CreateShader(device, view_instancing_pixel_source, zest_fragment_shader, "view_instancing_pixel", NULL, ZEST_TRUE);
	zest_pipeline_template pipeline_template = zest_CreatePipelineTemplate(device, "View Instancing");
	zest_SetPipelineShaders(pipeline_template, vertex_shader, pixel_shader);
	zest_SetPipelineDisableVertexInput(pipeline_template);
	zest_SetPipelineBlend(pipeline_template, zest_BlendStateOpaque());
	zest_SetPipelineViewCount(pipeline_template, 2);
	failed |= pipeline_template->view_mask != 3;
	zest_image_handle two_layer_handle = create_layered_target(device, 2);
	zest_image_handle one_layer_handle = create_layered_target(device, 1);
	zest_image_handle six_layer_handle = create_layered_target(device, 6);
	zest_image two_layer = zest_GetImage(two_layer_handle);
	zest_image one_layer = zest_GetImage(one_layer_handle);
	zest_image six_layer = zest_GetImage(six_layer_handle);
	failed |= !two_layer || !one_layer || !six_layer;
	if (!failed) {
		const int red[4] = { 255, 0, 0, 255 };
		const int green[4] = { 0, 255, 0, 255 };
		zest_uint pixels[256];
		failed |= run_view_instancing_pass(context, two_layer, pipeline_template) != zest_fgs_success;
		memset(pixels, 0, sizeof(pixels));
		failed |= !zest__dx_read_subresource(device, two_layer, 0, 0, pixels) || !columns_hold(pixels, 0, 16, red);
		memset(pixels, 0, sizeof(pixels));
		failed |= !zest__dx_read_subresource(device, two_layer, 0, 1, pixels) || !columns_hold(pixels, 0, 16, green);
		failed |= zest_GetValidationErrorCount(device) != 0;

		zest_uint error_count = zest_GetValidationErrorCount(device);
		failed |= run_view_instancing_pass(context, one_layer, pipeline_template) != zest_fgs_success;
		failed |= zest_GetValidationErrorCount(device) != error_count + 1;

		error_count = zest_GetValidationErrorCount(device);
		failed |= !(run_view_instancing_pass(context, six_layer, pipeline_template) & zest_fgs_invalid_render_pass);
		failed |= zest_GetValidationErrorCount(device) != error_count + 1;
	}
	if (two_layer) zest_FreeImageNow(two_layer_handle);
	if (one_layer) zest_FreeImageNow(one_layer_handle);
	if (six_layer) zest_FreeImageNow(six_layer_handle);
	zest_FreePipelineTemplate(pipeline_template);
	zest_FreeShader(vertex_shader);
	zest_FreeShader(pixel_shader);
	zest_ResetValidationErrors(device);
	return failed | finish_device(device);
}

//Finds the named image's first-use record in the graph. out_barrier is the translated barrier sent for it, or NULL when the send dropped it
static zest_bool imported_first_barrier(zest_frame_graph frame_graph, const char *name, const D3D12_TEXTURE_BARRIER **out_barrier) {
	*out_barrier = NULL;
	for (zest_uint pass_index = 0; pass_index != zest_GetFrameGraphFinalPassCount(frame_graph); ++pass_index) {
		const zest_pass_group_t *pass_group = zest_GetFrameGraphFinalPass(frame_graph, pass_index);
		zest_dx_execution_barriers_backend_t *backend = ZEST_DX_BACKEND(execution_barriers, &pass_group->execution_details.barriers);
		for (zest_uint index = 0; index != zest_vec_size(backend->acquire_images); ++index) {
			const zest_dx_frame_graph_image_barrier_t *record = &backend->acquire_images[index];
			if (!record->first_use || strcmp(record->resource->name, name) != 0) continue;
			//A dropped barrier is cleared and the kept ones are compacted over it, so none is left for its image
			ID3D12Resource *resource = ZEST_DX_BACKEND(image, record->resource->view->image)->resource;
			for (zest_uint translated = 0; translated != zest_vec_size(backend->acquire_translated); ++translated) {
				if (backend->acquire_translated[translated].pResource == resource) {
					*out_barrier = &backend->acquire_translated[translated];
					break;
				}
			}
			return ZEST_TRUE;
		}
	}
	return ZEST_FALSE;
}

static zest_frame_graph build_imported_first_use_graph(zest_context context, zest_frame_graph_cache_key_t *cache_key, zest_image storage_image, zest_image texture) {
	if (!zest_BeginFrameGraph(context, "Imported First Use", cache_key)) return NULL;
	zest_ImportSwapchainResource();
	zest_resource_node storage_node = zest_ImportImageResource("First Use Storage", storage_image, 0);
	zest_resource_node texture_node = zest_ImportImageResource("First Use Texture", texture, 0);

	zest_BeginComputePass("First Use Compute");
	zest_ConnectOutput(storage_node);
	zest_SetPassTask(zest_EmptyRenderPass, 0);
	zest_EndPass();

	zest_BeginRenderPass("First Use Render");
	zest_ConnectInput(storage_node);
	zest_ConnectInput(texture_node);
	zest_ConnectSwapChainOutput();
	zest_SetPassTask(zest_EmptyRenderPass, 0);
	zest_EndPass();
	return zest_EndFrameGraph();
}

//A cached graph's first barrier for an imported image starts from the state the previous execution left: refreshed when the layout changed, dropped when nothing did
int test_d3d12_imported_first_use(void) {
	D3D12TestWindow window;
	if (!open_test_window(&window, "D3D12 Imported First Use")) {
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
	zest_image_info_t storage_info = zest_CreateImageInfo(16, 16);
	storage_info.format = zest_format_r8g8b8a8_unorm;
	storage_info.flags = zest_image_preset_storage;
	zest_image_handle storage_handle = zest_CreateImage(device, &storage_info);
	zest_image_info_t texture_info = zest_CreateImageInfo(16, 16);
	texture_info.format = zest_format_r8g8b8a8_unorm;
	texture_info.flags = zest_image_preset_texture;
	zest_image_handle texture_handle = zest_CreateImage(device, &texture_info);
	zest_image storage_image = zest_GetImage(storage_handle);
	zest_image texture = zest_GetImage(texture_handle);
	int failed = context == NULL || !storage_image || !texture;
	if (!failed) {
		const int frame_count = ZEST_MAX_FIF + 2;
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
			if (!frame_graph) frame_graph = build_imported_first_use_graph(context, &cache_key, storage_image, texture);
			zest_EndFrame(context, frame_graph);
			failed |= frame_graph == NULL || zest_GetFrameGraphResult(frame_graph) != zest_fgs_success;
			if (failed || frame == 0) continue;
			//The previous execution's render pass left the storage image in SHADER_RESOURCE, so its read is the source
			const D3D12_TEXTURE_BARRIER *storage_barrier = NULL;
			failed |= !imported_first_barrier(frame_graph, "First Use Storage", &storage_barrier) || storage_barrier == NULL;
			if (storage_barrier) {
				failed |= storage_barrier->LayoutBefore != D3D12_BARRIER_LAYOUT_SHADER_RESOURCE || storage_barrier->LayoutAfter != D3D12_BARRIER_LAYOUT_UNORDERED_ACCESS;
				failed |= !(storage_barrier->AccessBefore & D3D12_BARRIER_ACCESS_SHADER_RESOURCE) || !(storage_barrier->SyncBefore & D3D12_BARRIER_SYNC_PIXEL_SHADING);
			}
			//The texture is only sampled, so once the first execution has moved it out of UNDEFINED there's nothing to change
			const D3D12_TEXTURE_BARRIER *texture_barrier = NULL;
			failed |= !imported_first_barrier(frame_graph, "First Use Texture", &texture_barrier) || texture_barrier != NULL;
		}
		failed |= cached_count != frame_count - 1;
		failed |= zest_GetValidationErrorCount(device) != 0;
	}
	if (context) zest_DestroyContext(context);
	if (storage_image) zest_FreeImageNow(storage_handle);
	if (texture) zest_FreeImageNow(texture_handle);
	failed |= finish_device(device);
	close_test_window(&window);
	return failed;
}

//An imported image left in SHADER_RESOURCE can't be its graph's first use on the copy queue, so that barrier is reported and skipped
int test_d3d12_copy_queue_first_use_report(void) {
	zest_device device = zest_EndDeviceBuilder(BeginD3D12TestBuilder());
	if (!device) return 1;
	zest_create_context_info_t info = zest_CreateContextInfo();
	zest_context context = zest_CreateHeadlessContext(device, &info);
	if (!context) return 1 | finish_device(device);
	zest_image_info_t image_info = zest_CreateImageInfo(16, 16);
	image_info.format = zest_format_r8g8b8a8_unorm;
	image_info.flags = zest_image_preset_texture;
	zest_image_handle image_handle = zest_CreateImage(device, &image_info);
	zest_image image = zest_GetImage(image_handle);
	int failed = image == NULL;
	if (image) {
		zest_uint pixels[16 * 16];
		memset(pixels, 0xFF, sizeof(pixels));
		failed |= !zest_CopyBitmapToImage(device, pixels, sizeof(pixels), image, 16, 16);
		failed |= ZEST_DX_BACKEND(image, image)->layout != D3D12_BARRIER_LAYOUT_SHADER_RESOURCE;
		zest_frame_graph frame_graph = NULL;
		if (zest_BeginCommandGraph(context, "Copy Queue First Use Report", 0)) {
			zest_image_resource_info_t target_info = { zest_format_r8g8b8a8_unorm };
			target_info.width = 16;
			target_info.height = 16;
			zest_resource_node target = zest_AddTransientImageResource("Copy Queue Target", &target_info);
			zest_FlagResourceAsEssential(target);
			zest_resource_node imported = zest_ImportImageResource("Copy Queue Image", image, 0);

			zest_BeginRenderPass("Copy Queue Render");
			zest_ConnectOutput(target);
			zest_SetPassTask(zest_EmptyRenderPass, 0);
			zest_EndPass();

			//It shares the render pass's wave, so it runs on the copy queue. It records no copy, as the image stays in SHADER_RESOURCE
			zest_BeginTransferPass("Copy Queue Transfer");
			zest_ConnectOutput(imported);
			zest_SetPassTask(zest_EmptyRenderPass, 0);
			zest_EndPass();
			frame_graph = zest_EndFrameGraph();
		}
		failed |= frame_graph == NULL || zest_FlushFrameGraphAndWait(frame_graph) != zest_semaphore_status_success;
		failed |= frame_graph == NULL || zest_GetFrameGraphResult(frame_graph) != zest_fgs_success;
		zest_bool transfer_on_copy_queue = ZEST_FALSE;
		for (zest_uint pass_index = 0; frame_graph && pass_index != zest_GetFrameGraphFinalPassCount(frame_graph); ++pass_index) {
			const zest_pass_group_t *pass_group = zest_GetFrameGraphFinalPass(frame_graph, pass_index);
			if (strcmp(pass_group->passes[0]->name, "Copy Queue Transfer") == 0) transfer_on_copy_queue = pass_group->compiled_queue_info.queue_type == zest_queue_transfer;
		}
		failed |= !transfer_on_copy_queue;
		failed |= zest_GetValidationErrorCount(device) != 1;
		failed |= !validation_error_contains(device, "The image Copy Queue Image was left in a layout the copy queue can't start from");
		//The skipped barrier left the image where the upload put it
		failed |= ZEST_DX_BACKEND(image, image)->layout != D3D12_BARRIER_LAYOUT_SHADER_RESOURCE;
	}
	zest_ResetValidationErrors(device);
	if (image) zest_FreeImageNow(image_handle);
	zest_DestroyContext(context);
	return failed | finish_device(device);
}

//Fails the signal of the batch it's recorded in when the flag it's given is set
static void rescue_read_task(const zest_command_list command_list, void *user_data) {
	zest__dx_forced_signal_failure = *(zest_bool *)user_data;
}

//The imported image's first use is on the copy queue and its last on the graphics queue, in a later submission, so it's handed back
static zest_semaphore_status run_rescue_graph(zest_context context, zest_image image, zest_bool *fail_second_submission, zest_bool *out_read_in_second_submission) {
	*out_read_in_second_submission = ZEST_FALSE;
	if (!zest_BeginCommandGraph(context, "Rescue", 0)) return zest_semaphore_status_error;
	zest_image_resource_info_t target_info = { zest_format_r8g8b8a8_unorm };
	target_info.width = 16;
	target_info.height = 16;
	zest_resource_node side_target = zest_AddTransientImageResource("Rescue Side", &target_info);
	zest_resource_node read_target = zest_AddTransientImageResource("Rescue Target", &target_info);
	zest_FlagResourceAsEssential(side_target);
	zest_FlagResourceAsEssential(read_target);
	zest_resource_node imported = zest_ImportImageResource("Rescue Image", image, 0);

	zest_BeginTransferPass("Rescue Transfer");
	zest_ConnectOutput(imported);
	zest_SetPassTask(zest_EmptyRenderPass, 0);
	zest_EndPass();

	zest_BeginRenderPass("Rescue Side Pass");
	zest_ConnectOutput(side_target);
	zest_SetPassTask(zest_EmptyRenderPass, 0);
	zest_EndPass();

	zest_BeginRenderPass("Rescue Read");
	zest_ConnectInput(imported);
	zest_ConnectOutput(read_target);
	zest_SetPassTask(rescue_read_task, fail_second_submission);
	zest_EndPass();
	zest_frame_graph frame_graph = zest_EndFrameGraph();
	zest_semaphore_status status = zest_FlushFrameGraphAndWait(frame_graph);
	zest__dx_forced_signal_failure = ZEST_FALSE;
	//The failure only drains the device when a batch went out before it, so the pass that fails has to be in the second submission
	for (zest_uint pass_index = 0; pass_index != zest_GetFrameGraphFinalPassCount(frame_graph); ++pass_index) {
		const zest_pass_group_t *pass_group = zest_GetFrameGraphFinalPass(frame_graph, pass_index);
		if (strcmp(pass_group->passes[0]->name, "Rescue Read") == 0) *out_read_in_second_submission = ZEST__SUBMISSION_INDEX(pass_group->submission_id) == 1;
	}
	return status;
}

//An execution that fails after a submission went out drains the device, so it clears the imported image's tracked state rather than leave a handback the next execution would acquire
int test_d3d12_rescue_clears_tracked_state(void) {
	zest_device device = zest_EndDeviceBuilder(BeginD3D12TestBuilder());
	if (!device) return 1;
	zest_create_context_info_t info = zest_CreateContextInfo();
	zest_context context = zest_CreateHeadlessContext(device, &info);
	if (!context) return 1 | finish_device(device);
	zest_image_info_t image_info = zest_CreateImageInfo(16, 16);
	image_info.format = zest_format_r8g8b8a8_unorm;
	image_info.flags = zest_image_preset_texture;
	zest_image_handle image_handle = zest_CreateImage(device, &image_info);
	zest_image image = zest_GetImage(image_handle);
	int failed = image == NULL;
	if (image) {
		zest_bool fail_second_submission = ZEST_FALSE;
		zest_bool read_in_second_submission = ZEST_FALSE;
		failed |= run_rescue_graph(context, image, &fail_second_submission, &read_in_second_submission) != zest_semaphore_status_success;
		failed |= !read_in_second_submission;
		failed |= image->last_access_mask == 0 || image->last_stage_mask == 0;
		failed |= image->queue_family_index != context->graphics_family_index || image->released_to_family != context->transfer_family_index;

		fail_second_submission = ZEST_TRUE;
		failed |= run_rescue_graph(context, image, &fail_second_submission, &read_in_second_submission) != zest_semaphore_status_error;
		failed |= image->last_access_mask != 0 || image->last_stage_mask != 0;
		failed |= image->queue_family_index != ZEST_QUEUE_FAMILY_IGNORED || image->released_to_family != ZEST_QUEUE_FAMILY_IGNORED;

		//The next execution starts from the cleared state and hands the image back again
		fail_second_submission = ZEST_FALSE;
		failed |= run_rescue_graph(context, image, &fail_second_submission, &read_in_second_submission) != zest_semaphore_status_success;
		failed |= image->queue_family_index != context->graphics_family_index || image->released_to_family != context->transfer_family_index;
		failed |= zest_GetValidationErrorCount(device) != 0;
	}
	if (image) zest_FreeImageNow(image_handle);
	zest_DestroyContext(context);
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
	{ "D3D12 Immediate Compute", test_d3d12_immediate_compute },
	{ "D3D12 Pool Failure Rejected", test_d3d12_pool_failure_rejected },
	{ "D3D12 SRV Description", test_d3d12_srv_description },
	{ "D3D12 Image Barrier Builder", test_d3d12_image_barrier_builder },
	{ "D3D12 Buffer Barrier Translation", test_d3d12_buffer_barrier_translation },
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
	{ "D3D12 Debug Overlay", test_d3d12_debug_overlay },
	{ "D3D12 Object Names", test_d3d12_object_names },
	{ "D3D12 Push Constant Fallback Frames", test_d3d12_push_constant_fallback_frames },
	{ "D3D12 DXGI Messages Counted", test_d3d12_dxgi_messages_counted },
	{ "D3D12 Per Mip Views", test_d3d12_per_mip_views },
	{ "D3D12 HLSL Compile", test_d3d12_hlsl_compile },
	{ "D3D12 HLSL Compile Error", test_d3d12_hlsl_compile_error },
	{ "D3D12 DXC Missing", test_d3d12_dxc_missing },
	{ "D3D12 Shader Reflection", test_d3d12_shader_reflection },
	{ "D3D12 Shader Reflection Cross Check", test_d3d12_shader_reflection_cross_check },
	{ "D3D12 Shader Reflection Bad Containers", test_d3d12_shader_reflection_bad_containers },
	{ "D3D12 Root Layout Rules", test_d3d12_root_layout_rules },
	{ "D3D12 Root Layout Errors", test_d3d12_root_layout_errors },
	{ "D3D12 Root Signatures From Shaders", test_d3d12_root_signatures_from_shaders },
	{ "D3D12 Shader Compile", test_d3d12_shader_compile },
	{ "D3D12 Pipeline State Translation", test_d3d12_pipeline_state_translation },
	{ "D3D12 Pipeline Cache", test_d3d12_pipeline_cache },
	{ "D3D12 Precompiled Shaders", test_d3d12_precompiled_shaders },
	{ "D3D12 Lazy Binding Order", test_d3d12_lazy_binding_order },
	{ "D3D12 Push Constant Fallback", test_d3d12_push_constant_fallback },
	{ "D3D12 Compute Reads Descriptors", test_d3d12_compute_reads_descriptors },
	{ "D3D12 Viewport Orientation", test_d3d12_viewport_orientation },
	{ "D3D12 Buffer Copies", test_d3d12_buffer_copies },
	{ "D3D12 Buffer Descriptors", test_d3d12_buffer_descriptors },
	{ "D3D12 Structured Buffer Descriptors", test_d3d12_structured_buffer_descriptors },
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
	{ "D3D12 Cross Queue Barriers", test_d3d12_cross_queue_barriers },
	{ "D3D12 Indirect Signatures", test_d3d12_indirect_signatures },
	{ "D3D12 View Instancing", test_d3d12_view_instancing },
	{ "D3D12 Imported First Use", test_d3d12_imported_first_use },
	{ "D3D12 Copy Queue First Use Report", test_d3d12_copy_queue_first_use_report },
	{ "D3D12 Rescue Clears Tracked State", test_d3d12_rescue_clears_tracked_state },
	{ "D3D12 Device Removal Report", test_d3d12_device_removal_report },
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
