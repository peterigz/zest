#ifndef ZEST_DX12_H
#define ZEST_DX12_H

/*
Zest D3D12 Implementation
	-- [Shader_code]
	-- [Backend_structs]
	-- [Other_setup_structs]
	-- [Error_logging]
	-- [Enum_to_string_functions]
	-- [Type_converters]
	-- [Inline_helpers]
	-- [Initialisation_functions]
	-- [Swapchain_setup]
	-- [Swapchain_presenting]
	-- [Backend_setup_functions]
	-- [Backend_cleanup_functions]
	-- [Fences]
	-- [Command_pools]
	-- [Buffer_and_memory]
	-- [Descriptor_heaps]
	-- [General_renderer]
	-- [Device_OS]
	-- [Pipelines]
	-- [Images]
	-- [General_helpers]
	-- [Internal_Frame_graph_context_functions]
	-- [Frame_graph_context_functions]
	-- [Shader_compiling]
	-- [Debug_functions]
*/

#include <d3d12.h>
#include <d3d12sdklayers.h>
#include <dxgi1_6.h>

#ifdef __cplusplus
#define ZEST_DX_CALL(object, method, ...) (object)->method(__VA_ARGS__)
#define ZEST_DX_CALL0(object, method) (object)->method()
#define ZEST_DX_IID(type) zest__dx_iid_##type
#else
#define ZEST_DX_CALL(object, method, ...) (object)->lpVtbl->method((object), __VA_ARGS__)
#define ZEST_DX_CALL0(object, method) (object)->lpVtbl->method(object)
#define ZEST_DX_IID(type) &zest__dx_iid_##type
#endif
#define ZEST_DX_BACKEND(name, object) ((zest_dx_##name##_backend_t *)(object)->backend)
#define ZEST_DX_RELEASE(object) do { if (object) { ZEST_DX_CALL0((object), Release); (object) = NULL; } } while (0)

#define ZEST_DX_RETURN_FALSE_ON_FAIL(device_handle, expression) do { \
	ZEST_DX_BACKEND(device, (device_handle))->last_result = (expression); \
	if (FAILED(ZEST_DX_BACKEND(device, (device_handle))->last_result)) { \
		zest__dx_log_hresult((device_handle), ZEST_DX_BACKEND(device, (device_handle))->last_result, __FILE__, __LINE__); \
		return ZEST_FALSE; \
	} \
} while (0)

#if defined(ZEST_D3D12_SDK_VERSION) && !defined(ZEST_D3D12_NO_AGILITY_EXPORTS)
#ifdef __cplusplus
extern "C" {
#endif
__declspec(dllexport) extern const UINT D3D12SDKVersion = ZEST_D3D12_SDK_VERSION;
__declspec(dllexport) extern const char *D3D12SDKPath = ".\\D3D12\\";
#ifdef __cplusplus
}
#endif
#endif

#define ZEST__DX_DEFINE_IID(name, data1, data2, data3, byte0, byte1, byte2, byte3, byte4, byte5, byte6, byte7) \
	static const GUID zest__dx_iid_##name = { data1, data2, data3, { byte0, byte1, byte2, byte3, byte4, byte5, byte6, byte7 } }

// -- Interface IDs
ZEST__DX_DEFINE_IID(IDXGIFactory6, 0xc1b6694f, 0xff09, 0x44a9, 0xb0, 0x3c, 0x77, 0x90, 0x0a, 0x0a, 0x1d, 0x17);
ZEST__DX_DEFINE_IID(IDXGIAdapter4, 0x3c8d99d1, 0x4fbf, 0x4181, 0xa8, 0x2c, 0xaf, 0x66, 0xbf, 0x7b, 0xd2, 0x4e);
ZEST__DX_DEFINE_IID(ID3D12Device, 0x189819f1, 0x1db6, 0x4b57, 0xbe, 0x54, 0x18, 0x21, 0x33, 0x9b, 0x85, 0xf7);
ZEST__DX_DEFINE_IID(ID3D12Device10, 0x517f8718, 0xaa66, 0x49f9, 0xb0, 0x2b, 0xa7, 0xab, 0x89, 0xc0, 0x60, 0x31);
ZEST__DX_DEFINE_IID(ID3D12Debug3, 0x5cf4e58f, 0xf671, 0x4ff1, 0xa5, 0x42, 0x36, 0x86, 0xe3, 0xd1, 0x53, 0xd1);
ZEST__DX_DEFINE_IID(ID3D12DebugDevice, 0x3febd6dd, 0x4973, 0x4787, 0x81, 0x94, 0xe4, 0x5f, 0x9e, 0x28, 0x92, 0x3e);
ZEST__DX_DEFINE_IID(ID3D12InfoQueue1, 0x2852dd88, 0xb484, 0x4c0c, 0xb6, 0xb1, 0x67, 0x16, 0x85, 0x00, 0xe6, 0x00);
ZEST__DX_DEFINE_IID(ID3D12DeviceRemovedExtendedDataSettings1, 0xdbd5ae51, 0x3317, 0x4f0a, 0xad, 0xf9, 0x1d, 0x7c, 0xed, 0xca, 0xae, 0x0b);
ZEST__DX_DEFINE_IID(ID3D12CommandQueue, 0x0ec870a6, 0x5d7e, 0x4c22, 0x8c, 0xfc, 0x5b, 0xaa, 0xe0, 0x76, 0x16, 0xed);
ZEST__DX_DEFINE_IID(ID3D12CommandAllocator, 0x6102dee4, 0xaf59, 0x4b09, 0xb9, 0x99, 0xb4, 0x4d, 0x73, 0xf0, 0x9b, 0x24);
ZEST__DX_DEFINE_IID(ID3D12GraphicsCommandList7, 0xdd171223, 0x8b61, 0x4769, 0x90, 0xe3, 0x16, 0x0c, 0xcd, 0xe4, 0xe2, 0xc1);
ZEST__DX_DEFINE_IID(ID3D12Fence, 0x0a753dcf, 0xc4d8, 0x4b91, 0xad, 0xf6, 0xbe, 0x5a, 0x60, 0xd9, 0x5a, 0x76);
ZEST__DX_DEFINE_IID(ID3D12Resource, 0x696442be, 0xa72e, 0x4059, 0xbc, 0x79, 0x5b, 0x5c, 0x98, 0x04, 0x0f, 0xad);
ZEST__DX_DEFINE_IID(ID3D12Heap, 0x6b3b2502, 0x6e51, 0x45b3, 0x90, 0xee, 0x98, 0x84, 0x26, 0x5e, 0x8d, 0xf3);
ZEST__DX_DEFINE_IID(ID3D12DescriptorHeap, 0x8efb471d, 0x616c, 0x4f49, 0x90, 0xf7, 0x12, 0x7b, 0xb7, 0x63, 0xfa, 0x51);
// -- End Interface IDs

// -- Backend_structs
typedef struct zest_dx_feature_support_t {
	D3D_FEATURE_LEVEL feature_level;
	D3D12_RESOURCE_BINDING_TIER resource_binding_tier;
	D3D12_RESOURCE_HEAP_TIER resource_heap_tier;
	D3D_SHADER_MODEL shader_model;
	D3D12_VIEW_INSTANCING_TIER view_instancing_tier;
	zest_bool enhanced_barriers;
	zest_bool inverted_viewport_height;
	zest_bool unrestricted_copy_pitch;
	zest_bool dynamic_depth_bias;
	zest_bool gpu_upload_heap;
	zest_bool copy_queue_timestamps;
	zest_bool tearing;
	zest_bool int64_shader_ops;
} zest_dx_feature_support_t;

#define ZEST_DX_CPU_DESCRIPTOR_BLOCK_SIZE 1024

typedef struct zest_dx_cpu_descriptor_heap_t {
	D3D12_DESCRIPTOR_HEAP_TYPE type;
	zest_uint increment;
	ID3D12DescriptorHeap **blocks;
	zest_uint next_in_block;
	D3D12_CPU_DESCRIPTOR_HANDLE *free_handles;
} zest_dx_cpu_descriptor_heap_t;

typedef struct zest_dx_device_backend_t {
	IDXGIFactory6 *factory;
	IDXGIAdapter4 *adapter;
	ID3D12Device *device;
	ID3D12Device10 *device10;
	ID3D12InfoQueue1 *info_queue;
	DWORD info_queue_cookie;
	zest_dx_feature_support_t support;
	zest_dx_cpu_descriptor_heap_t cpu_resource_heap;
	zest_dx_cpu_descriptor_heap_t cpu_sampler_heap;
	HRESULT last_result;
	zest_bool counted_as_live;
} zest_dx_device_backend_t;

typedef struct zest_dx_queue_backend_t {
	ID3D12CommandQueue *queue;
	ID3D12CommandAllocator *command_allocator;
	ID3D12GraphicsCommandList7 *command_list;
	D3D12_COMMAND_LIST_TYPE type;
	zest_buffer *pending_staging_buffers;
	//The last immediate list was executed but its fence signal failed, so nothing yet says when it finishes
	zest_bool unfenced_submission;
} zest_dx_queue_backend_t;

typedef struct zest_dx_context_backend_t {
	HRESULT last_result;
} zest_dx_context_backend_t;

typedef struct zest_dx_context_queue_backend_t {
	ID3D12CommandAllocator *command_allocators[ZEST_MAX_FIF];
	ID3D12GraphicsCommandList7 **command_lists;
} zest_dx_context_queue_backend_t;

typedef struct zest_dx_execution_timeline_backend_t {
	ID3D12Fence *fence;
} zest_dx_execution_timeline_backend_t;

//Heap categories carried in backend_memory_bits, where zero is a plain buffer
#define ZEST_DX_HEAP_CATEGORY_TEXTURES 1
#define ZEST_DX_HEAP_CATEGORY_TARGETS 2
#define ZEST_DX_HEAP_CATEGORY_MSAA 3

typedef struct zest_dx_buffer_allocator_backend_t {
	D3D12_HEAP_PROPERTIES heap_properties;
	zest_uint heap_category;
} zest_dx_buffer_allocator_backend_t;

typedef struct zest_dx_device_memory_pool_backend_t {
	ID3D12Resource *buffer;
	ID3D12Heap *heap;
} zest_dx_device_memory_pool_backend_t;

typedef struct zest_dx_device_memory_backend_t {
	ID3D12Heap *heap;
} zest_dx_device_memory_backend_t;

typedef struct zest_dx_image_backend_t {
	ID3D12Resource *resource;
	DXGI_FORMAT resource_format;
	D3D12_BARRIER_LAYOUT layout;
	zest_sample_count_flags sample_count;
} zest_dx_image_backend_t;

typedef struct zest_dx_image_view_backend_t {
	D3D12_CPU_DESCRIPTOR_HANDLE srv;
} zest_dx_image_view_backend_t;

typedef struct zest_dx_sampler_backend_t {
	D3D12_CPU_DESCRIPTOR_HANDLE descriptor;
} zest_dx_sampler_backend_t;

//Offsets into the shader visible heaps, where slot 0 is the sampler heap and every other slot is in the resource heap
typedef struct zest_dx_bindless_regions_t {
	zest_uint resource_offsets[zest_max_global_binding_number];
	zest_uint storage_buffer_uav_offset;
	zest_uint resource_descriptor_count;
	zest_uint sampler_descriptor_count;
} zest_dx_bindless_regions_t;

typedef struct zest_dx_set_layout_backend_t {
	zest_uint binding_counts[zest_max_global_binding_number];
} zest_dx_set_layout_backend_t;

typedef struct zest_dx_descriptor_pool_backend_t {
	ID3D12DescriptorHeap *resource_heap;
	ID3D12DescriptorHeap *sampler_heap;
	zest_dx_bindless_regions_t regions;
	zest_uint resource_increment;
	zest_uint sampler_increment;
	D3D12_CPU_DESCRIPTOR_HANDLE resource_cpu_start;
	D3D12_CPU_DESCRIPTOR_HANDLE sampler_cpu_start;
} zest_dx_descriptor_pool_backend_t;

typedef struct zest_dx_descriptor_set_backend_t {
	zest_dx_descriptor_pool_backend_t *pool;
} zest_dx_descriptor_set_backend_t;

typedef struct zest_dx_pipeline_layout_backend_t {
	zest_uint push_constant_size;
} zest_dx_pipeline_layout_backend_t;
// -- End Backend_structs

// -- Other_setup_structs
#define ZEST__DX_QUEUE_FAMILY_COUNT 3
#define ZEST__DX_WAIT_FOREVER (~(zest_microsecs)0)
#define ZEST__DX_IMMEDIATE_TIMEOUT ZEST_SECONDS_IN_MICROSECONDS(1000)

typedef struct zest_dx_queue_family_t {
	D3D12_COMMAND_LIST_TYPE type;
	zest_device_queue_type queue_types;
	zest_uint queue_count;
	const char *name;
} zest_dx_queue_family_t;

static const zest_dx_queue_family_t zest__dx_queue_families[ZEST__DX_QUEUE_FAMILY_COUNT] = {
	{ D3D12_COMMAND_LIST_TYPE_DIRECT, zest_queue_graphics | zest_queue_compute | zest_queue_transfer, 4, "Direct" },
	{ D3D12_COMMAND_LIST_TYPE_COMPUTE, zest_queue_compute | zest_queue_transfer, 4, "Compute" },
	{ D3D12_COMMAND_LIST_TYPE_COPY, zest_queue_transfer, 2, "Copy" },
};

//What every layer of one buffer to image copy region shares
typedef struct zest_dx_region_copy_t {
	D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint;
	zest_size source_row_pitch;
	zest_size source_slice_pitch;
	zest_uint source_slice_rows;
	zest_size row_bytes;
	zest_uint row_count;
	zest_uint first_subresource;
} zest_dx_region_copy_t;

static const char *zest__dx_forced_missing_feature = NULL;
static zest_bool zest__dx_forced_restricted_copy_pitch = ZEST_FALSE;
static zest_bool zest__dx_forced_feature_level_failure = ZEST_FALSE;
static zest_bool zest__dx_forced_debug_layer_unavailable = ZEST_FALSE;
static zest_bool zest__dx_forced_signal_failure = ZEST_FALSE;
static zest_uint zest__dx_device_release_count = 0;
//Enabling the debug layer removes every device created before it, so enabling it and counting a new device share a lock
static SRWLOCK zest__dx_debug_layer_lock = SRWLOCK_INIT;
static volatile LONG zest__dx_live_device_count = 0;
static zest_bool zest__dx_debug_layer_enabled = ZEST_FALSE;
//Per thread so concurrent waiters on one fence can't take each other's wake
static __declspec(thread) HANDLE zest__dx_wait_event = NULL;
// -- End Other_setup_structs

ZEST_API ID3D12Device *zest_GetD3D12Device(zest_device device);

ZEST_PRIVATE void zest__dx_initialise_platform_callbacks(zest_platform_t *platform);
ZEST_PRIVATE void zest__dx_log_hresult(zest_device device, HRESULT result, const char *file, int line);
ZEST_PRIVATE D3D12_CPU_DESCRIPTOR_HANDLE zest__dx_cpu_heap_start(ID3D12DescriptorHeap *heap);
ZEST_PRIVATE D3D12_GPU_DESCRIPTOR_HANDLE zest__dx_gpu_heap_start(ID3D12DescriptorHeap *heap);
ZEST_PRIVATE D3D12_RESOURCE_ALLOCATION_INFO zest__dx_resource_allocation_info(ID3D12Device10 *device, const D3D12_RESOURCE_DESC1 *description);
ZEST_PRIVATE D3D12_RESOURCE_DESC1 zest__dx_resource_description(ID3D12Resource *resource);
ZEST_PRIVATE D3D12_HEAP_PROPERTIES zest__dx_heap_properties(zest_memory_property_flags property_flags, zest_bool gpu_upload_supported);
ZEST_PRIVATE D3D12_HEAP_FLAGS zest__dx_image_heap_flags(zest_uint category, D3D12_RESOURCE_HEAP_TIER tier);
ZEST_PRIVATE zest_bool zest__dx_create_image_heap(zest_device device, zest_size size_in_bytes, zest_uint category, ID3D12Heap **heap_out);

//Type converters
ZEST_PRIVATE DXGI_FORMAT zest__dx_to_dxgi_format(zest_format format);
ZEST_PRIVATE zest_format zest__dx_from_dxgi_format(DXGI_FORMAT format);
ZEST_PRIVATE DXGI_FORMAT zest__dx_typeless_format(DXGI_FORMAT format);
ZEST_PRIVATE DXGI_FORMAT zest__dx_shader_view_format(DXGI_FORMAT format);
ZEST_PRIVATE zest_uint zest__dx_image_heap_category(zest_image_flags flags, zest_sample_count_flags samples, D3D12_RESOURCE_HEAP_TIER tier);
ZEST_PRIVATE UINT zest__dx_component_mapping(zest_component_mapping_t mapping);
ZEST_PRIVATE D3D12_BARRIER_LAYOUT zest__dx_depth_read_layout(zest_access_flags access, zest_device_queue_type queue_type);
ZEST_PRIVATE D3D12_BARRIER_LAYOUT zest__dx_barrier_layout(zest_image_layout layout, zest_access_flags access, zest_device_queue_type queue_type);
ZEST_PRIVATE D3D12_BARRIER_ACCESS zest__dx_barrier_access(zest_access_flags access, zest_bool is_image, D3D12_BARRIER_LAYOUT layout);
ZEST_PRIVATE D3D12_BARRIER_SYNC zest__dx_barrier_sync(zest_pipeline_stage_flags stages, zest_bool is_before);
ZEST_PRIVATE D3D12_BARRIER_SYNC zest__dx_normalise_sync(D3D12_BARRIER_SYNC sync, D3D12_BARRIER_ACCESS access);
ZEST_PRIVATE zest_image_layout zest__dx_image_layout_from_resource_state(zest_resource_state state, zest_bool is_depth);
ZEST_PRIVATE void zest__dx_layout_access_and_stage(zest_image_layout layout, zest_device_queue_type queue_type, zest_access_flags *access_out, zest_pipeline_stage_flags *stage_out);
ZEST_PRIVATE zest_device_queue_type zest__dx_queue_type(zest_queue queue);
ZEST_PRIVATE zest_uint zest__dx_plane_count(zest_format format);

//CPU descriptor heaps
ZEST_PRIVATE zest_bool zest__dx_initialise_cpu_descriptor_heap(zest_device device, zest_dx_cpu_descriptor_heap_t *heap, D3D12_DESCRIPTOR_HEAP_TYPE type);
ZEST_PRIVATE zest_bool zest__dx_add_cpu_descriptor_block(zest_device device, zest_dx_cpu_descriptor_heap_t *heap);
ZEST_PRIVATE D3D12_CPU_DESCRIPTOR_HANDLE zest__dx_allocate_cpu_descriptor(zest_device device, zest_dx_cpu_descriptor_heap_t *heap);
ZEST_PRIVATE void zest__dx_free_cpu_descriptor(zest_device device, zest_dx_cpu_descriptor_heap_t *heap, D3D12_CPU_DESCRIPTOR_HANDLE handle);
ZEST_PRIVATE void zest__dx_cleanup_cpu_descriptor_heap(zest_device device, zest_dx_cpu_descriptor_heap_t *heap);

//Bindless descriptor heaps
ZEST_PRIVATE void zest__dx_compute_bindless_regions(const zest_uint binding_counts[zest_max_global_binding_number], zest_dx_bindless_regions_t *regions);
ZEST_PRIVATE void zest__dx_bind_bindless_heaps(zest_device device, ID3D12GraphicsCommandList7 *command_list);

//Images
ZEST_PRIVATE D3D12_FEATURE_DATA_FORMAT_SUPPORT zest__dx_format_support(ID3D12Device *native_device, DXGI_FORMAT format);
//Whether the support bits allow every use in flags, leaving a depth format's shader loads to its view format's own check
ZEST_PRIVATE zest_bool zest__dx_format_support_allows(const D3D12_FEATURE_DATA_FORMAT_SUPPORT *support, zest_image_flags flags, zest_bool check_shader_load);
ZEST_PRIVATE D3D12_SHADER_RESOURCE_VIEW_DESC zest__dx_image_srv_description(zest_image image, zest_image_view_type view_type, zest_uint mip_levels_this_view, zest_uint base_mip, zest_uint base_array_index, zest_uint layer_count);
ZEST_PRIVATE zest_bool zest__dx_transition_image_layout(zest_queue queue, zest_image image, zest_image_layout new_layout, const D3D12_BARRIER_LAYOUT *exact_layout_after, zest_uint base_mip_index, zest_uint mip_levels, zest_uint base_array_index, zest_uint layer_count);
ZEST_PRIVATE D3D12_BARRIER_ACCESS zest__dx_common_layout_access(D3D12_BARRIER_LAYOUT layout, D3D12_BARRIER_ACCESS access);
ZEST_PRIVATE zest_bool zest__dx_copy_region_is_valid(zest_device device, const D3D12_RESOURCE_DESC1 *description, const zest_buffer_image_copy_t *region);
ZEST_PRIVATE void zest__dx_prepare_region_copy(ID3D12Device10 *native_device, const D3D12_RESOURCE_DESC1 *description, zest_uint plane_count, zest_uint block_width, zest_uint block_height, const zest_buffer_image_copy_t *region, zest_dx_region_copy_t *copy);
ZEST_PRIVATE zest_bool zest__dx_copy_region_fits_buffer(zest_device device, const zest_dx_region_copy_t *copy, const zest_buffer_image_copy_t *region, zest_buffer buffer, zest_size src_offset);

//General helpers
ZEST_PRIVATE zest_bool zest__dx_copy_needs_repack(zest_size row_pitch, zest_size offset, zest_bool unrestricted_copy_pitch);
ZEST_PRIVATE zest_bool zest__dx_region_needs_repack(const zest_dx_region_copy_t *copy, zest_size offset, zest_bool unrestricted_copy_pitch);
ZEST_PRIVATE void zest__dx_repack_rows(const void *source, zest_size source_row_pitch, void *destination, zest_size destination_row_pitch, zest_size row_bytes, zest_uint row_count);

//Device setup
ZEST_PRIVATE void __stdcall zest__dx_debug_callback(D3D12_MESSAGE_CATEGORY category, D3D12_MESSAGE_SEVERITY severity, D3D12_MESSAGE_ID id, LPCSTR description, void *context);
ZEST_PRIVATE zest_bool zest__dx_meets_minimum_spec(const zest_dx_feature_support_t *support, const char **missing_out);
ZEST_PRIVATE zest_capability_flags zest__dx_translate_capabilities(const zest_dx_feature_support_t *support);
ZEST_PRIVATE void zest__dx_query_feature_support(ID3D12Device *native_device, IDXGIFactory6 *factory, zest_dx_feature_support_t *support);
ZEST_PRIVATE zest_bool zest__dx_warp_meets_minimum_spec(const char **missing_out);
ZEST_PRIVATE void zest__dx_enable_debug_layer(zest_device device, ID3D12Debug3 *debug);
ZEST_PRIVATE zest_bool zest__dx_prepare_debug_layer(zest_device device);
ZEST_PRIVATE zest_bool zest__dx_select_adapter(zest_device device);
ZEST_PRIVATE zest_bool zest__dx_device_already_exists(ID3D12Device *native_device);
ZEST_PRIVATE void zest__dx_setup_info_queue(zest_device device);
ZEST_PRIVATE zest_bool zest__dx_create_queues(zest_device device);
ZEST_PRIVATE void zest__dx_add_queue_manager_list(zest_device device, zest_device_queue_type queue_type, const zest_uint *family_order, zest_uint order_count);
ZEST_PRIVATE void zest__dx_cleanup_queues(zest_device device);
ZEST_PRIVATE void zest__dx_release_native_device(zest_device device, ID3D12Device *native_device);
ZEST_PRIVATE zest_uint zest__dx_last_device_release_count(void);
ZEST_PRIVATE DWORD zest__dx_timeout_milliseconds(zest_microsecs timeout);
ZEST_PRIVATE void zest__dx_release_immediate_queue(zest_queue queue, zest_bool gpu_finished);

//Platform callbacks
ZEST_PRIVATE void *zest__dx_new_device_backend(zest_device device);
ZEST_PRIVATE zest_bool zest__dx_initialise_device(zest_device device);
ZEST_PRIVATE zest_bool zest__dx_query_device_capabilities(zest_device device);
ZEST_PRIVATE void zest__dx_set_depth_format(zest_device device);
ZEST_PRIVATE void zest__dx_wait_for_idle_device(zest_device device);
ZEST_PRIVATE void zest__dx_get_memory_budget(zest_device device, zest_memory_budget_t *budget);
ZEST_PRIVATE void zest__dx_cleanup_device_backend(zest_device device);
ZEST_PRIVATE void *zest__dx_new_queue_backend(zest_device device, zest_uint family_index);
ZEST_PRIVATE void zest__dx_cleanup_queue_backend(zest_device device, zest_queue queue);
ZEST_PRIVATE void *zest__dx_new_context_backend(zest_context context);
ZEST_PRIVATE zest_bool zest__dx_initialise_context_backend(zest_context context);
ZEST_PRIVATE void zest__dx_cleanup_context_backend(zest_context context);
ZEST_PRIVATE void *zest__dx_new_context_queue_backend(zest_context context);
ZEST_PRIVATE zest_bool zest__dx_initialise_context_queue_backend(zest_context context, zest_context_queue context_queue);
ZEST_PRIVATE void zest__dx_cleanup_context_queue_backend(zest_context context, zest_context_queue context_queue);
ZEST_PRIVATE void zest__dx_reset_queue_command_pool(zest_context context, zest_context_queue queue, zest_bool release_resources);
ZEST_PRIVATE void zest__dx_flush_used_buffers(zest_context context, zest_uint frame_in_flight);
ZEST_PRIVATE zest_bool zest__dx_create_execution_timeline_backend(zest_device device, zest_execution_timeline timeline);
ZEST_PRIVATE void zest__dx_cleanup_execution_timeline_backend(zest_execution_timeline timeline);
ZEST_PRIVATE zest_semaphore_status zest__dx_wait_for_timeline(zest_execution_timeline timeline, zest_microsecs timeout);
ZEST_PRIVATE zest_queue zest__dx_imm_begin_command_buffer(zest_device device, zest_device_queue_type target_queue);
ZEST_PRIVATE zest_bool zest__dx_imm_end_command_buffer(zest_queue queue);
ZEST_PRIVATE zest_bool zest__dx_report_unavailable(zest_queue queue, const char *message);
ZEST_PRIVATE void zest__dx_imm_fill_buffer(zest_queue queue, zest_buffer buffer, zest_uint value);
ZEST_PRIVATE void zest__dx_imm_update_buffer(zest_queue queue, zest_buffer buffer, void *data, zest_size intended_size);
ZEST_PRIVATE zest_bool zest__dx_imm_clear_color_image(zest_queue queue, zest_image image, zest_clear_value_t clear_value);
ZEST_PRIVATE zest_bool zest__dx_imm_clear_depth_stencil_image(zest_queue queue, zest_image image, float depth, zest_uint stencil);
ZEST_PRIVATE zest_bool zest__dx_imm_blit_image(zest_queue queue, zest_image src_image, zest_image dst_image, int src_x, int src_y, int src_width, int src_height, int dst_x, int dst_y, int dst_width, int dst_height, zest_filter_type filter);
ZEST_PRIVATE zest_bool zest__dx_imm_resolve_image(zest_queue queue, zest_image src_image, zest_image dst_image);
ZEST_PRIVATE void zest__dx_imm_send_push_constants(zest_queue queue, void *data, zest_uint size);
ZEST_PRIVATE void zest__dx_imm_bind_compute_pipeline(zest_queue queue, zest_compute compute);
ZEST_PRIVATE zest_bool zest__dx_imm_dispatch_compute(zest_queue queue, zest_uint group_count_x, zest_uint group_count_y, zest_uint group_count_z);
ZEST_PRIVATE zest_bool zest__dx_cmd_copy_buffer_one_time(zest_queue queue, zest_buffer src_buffer, zest_buffer dst_buffer, zest_size size, zest_size src_offset, zest_size dst_offset);
ZEST_PRIVATE zest_bool zest__dx_copy_buffer_to_image(zest_queue queue, zest_buffer buffer, zest_size src_offset, zest_image image, zest_uint width, zest_uint height);
ZEST_PRIVATE zest_bool zest__dx_generate_mipmaps(zest_queue queue, zest_image image);
ZEST_PRIVATE void *zest__dx_create_buffer_allocator_backend(zest_device device, zest_context context, zest_size size, zest_buffer_info_t *buffer_info, zest_uint backend_memory_bits);
ZEST_PRIVATE void *zest__dx_new_memory_pool_backend(zest_buffer_allocator buffer_allocator);
ZEST_PRIVATE void *zest__dx_new_memory_backend(zest_device device);
ZEST_PRIVATE zest_bool zest__dx_add_buffer_memory_pool(zest_device device, zest_context context, zest_size size, zest_buffer_allocator buffer_allocator, zest_device_memory_pool memory_pool);
ZEST_PRIVATE zest_bool zest__dx_create_image_memory_pool(zest_device device, zest_context context, zest_size size_in_bytes, zest_buffer_allocator buffer_allocator, zest_device_memory_pool pool);
ZEST_PRIVATE zest_bool zest__dx_create_device_memory(zest_device device, zest_size size_in_bytes, zest_buffer_info_t *buffer_info, zest_uint backend_memory_bits, zest_device_memory memory);
ZEST_PRIVATE zest_bool zest__dx_map_memory(zest_device_memory_pool memory_allocation, zest_size size, zest_size offset);
ZEST_PRIVATE void zest__dx_unmap_memory(zest_device_memory_pool memory_allocation);
ZEST_PRIVATE void zest__dx_cleanup_memory_pool_backend(zest_device_memory_pool memory_allocation);
ZEST_PRIVATE void zest__dx_cleanup_memory_backend(zest_device_memory memory);
ZEST_PRIVATE void zest__dx_cleanup_buffer_allocator_backend(zest_buffer_allocator buffer_allocator);
ZEST_PRIVATE zest_bool zest__dx_is_image_format_supported(zest_device device, zest_format format, zest_image_flags flags);
ZEST_PRIVATE void *zest__dx_new_image_backend(zest_device device);
ZEST_PRIVATE zest_bool zest__dx_create_image(zest_device device, zest_context context, zest_image image, zest_uint layer_count, zest_sample_count_flags num_samples, zest_image_flags flags);
ZEST_PRIVATE void zest__dx_cleanup_image_backend(zest_image image);
ZEST_PRIVATE zest_image_view_t *zest__dx_create_image_view(zest_device device, zest_image image, zest_image_view_type view_type, zest_uint mip_levels_this_view, zest_uint base_mip, zest_uint base_array_index, zest_uint layer_count, zloc_linear_allocator_t *linear_allocator);
ZEST_PRIVATE void zest__dx_cleanup_image_view_backend(zest_image_view view);
ZEST_PRIVATE void *zest__dx_new_sampler_backend(zest_device device);
ZEST_PRIVATE zest_bool zest__dx_create_sampler(zest_sampler sampler);
ZEST_PRIVATE void zest__dx_cleanup_sampler_backend(zest_sampler sampler);
ZEST_PRIVATE zest_bool zest__dx_transition_image(zest_queue queue, zest_image image, zest_resource_state new_state, zest_uint base_mip_index, zest_uint mip_levels, zest_uint base_array_index, zest_uint layer_count);
ZEST_PRIVATE zest_bool zest__dx_copy_buffer_regions_to_image(zest_queue queue, zest_buffer_image_copy_t *regions, zest_uint regions_count, zest_buffer buffer, zest_size src_offset, zest_image image);
ZEST_PRIVATE int zest__dx_get_image_raw_layout(zest_image image);
ZEST_PRIVATE zest_bool zest__dx_image_layout_is_valid_for_descriptor(zest_image image);
ZEST_PRIVATE zest_bool zest__dx_copy_bitmap_to_image(zest_device device, void *bitmap, zest_size image_size, zest_image dst_image, zest_uint width, zest_uint height);
ZEST_PRIVATE zest_bool zest__dx_copy_image_to_bitmap(zest_device device, zest_image src_image, void *pixels);
ZEST_PRIVATE void *zest__dx_new_set_layout_backend(zloc_allocator *allocator);
ZEST_PRIVATE zest_bool zest__dx_create_set_layout(zest_device device, zest_context context, zest_set_layout_builder_t *builder, zest_set_layout layout, zest_bool is_bindless);
ZEST_PRIVATE void zest__dx_cleanup_set_layout_backend(zest_set_layout layout);
ZEST_PRIVATE void *zest__dx_new_descriptor_pool_backend(zloc_allocator *allocator);
ZEST_PRIVATE zest_bool zest__dx_create_set_pool(zest_device device, zest_context context, zest_descriptor_pool pool, zest_set_layout layout, zest_uint max_set_count, zest_bool bindless);
ZEST_PRIVATE zest_descriptor_set zest__dx_create_bindless_set(zest_set_layout layout);
ZEST_PRIVATE void zest__dx_update_bindless_image_descriptor(zest_device device, zest_uint binding_number, zest_uint array_index, zest_descriptor_type type, zest_image image, zest_image_view view, zest_sampler sampler, zest_descriptor_set set);
ZEST_PRIVATE void *zest__dx_new_pipeline_layout_backend(zest_device device);
ZEST_PRIVATE zest_bool zest__dx_build_pipeline_layout(zest_device device, zest_pipeline_layout layout, zest_pipeline_layout_info_t *info);
ZEST_PRIVATE void zest__dx_cleanup_pipeline_layout_backend(zest_pipeline_layout layout);

// -- Error_logging
static const char *zest__dx_hresult_name(HRESULT result) {
	switch (result) {
		case E_FAIL: return "E_FAIL";
		case E_INVALIDARG: return "E_INVALIDARG";
		case E_OUTOFMEMORY: return "E_OUTOFMEMORY";
		case E_NOTIMPL: return "E_NOTIMPL";
		case E_NOINTERFACE: return "E_NOINTERFACE";
		case E_POINTER: return "E_POINTER";
		case E_ACCESSDENIED: return "E_ACCESSDENIED";
		case E_UNEXPECTED: return "E_UNEXPECTED";
		case DXGI_ERROR_DEVICE_REMOVED: return "DXGI_ERROR_DEVICE_REMOVED";
		case DXGI_ERROR_DEVICE_HUNG: return "DXGI_ERROR_DEVICE_HUNG";
		case DXGI_ERROR_DEVICE_RESET: return "DXGI_ERROR_DEVICE_RESET";
		case DXGI_ERROR_DRIVER_INTERNAL_ERROR: return "DXGI_ERROR_DRIVER_INTERNAL_ERROR";
		case DXGI_ERROR_INVALID_CALL: return "DXGI_ERROR_INVALID_CALL";
		case DXGI_ERROR_NOT_FOUND: return "DXGI_ERROR_NOT_FOUND";
		case DXGI_ERROR_UNSUPPORTED: return "DXGI_ERROR_UNSUPPORTED";
		case DXGI_ERROR_WAS_STILL_DRAWING: return "DXGI_ERROR_WAS_STILL_DRAWING";
		case DXGI_ERROR_ACCESS_LOST: return "DXGI_ERROR_ACCESS_LOST";
		case DXGI_ERROR_NOT_CURRENTLY_AVAILABLE: return "DXGI_ERROR_NOT_CURRENTLY_AVAILABLE";
		case D3D12_ERROR_ADAPTER_NOT_FOUND: return "D3D12_ERROR_ADAPTER_NOT_FOUND";
		case D3D12_ERROR_DRIVER_VERSION_MISMATCH: return "D3D12_ERROR_DRIVER_VERSION_MISMATCH";
		default: return "unknown HRESULT";
	}
}

void zest__dx_log_hresult(zest_device device, HRESULT result, const char *file, int line) {
	const char *name = zest__dx_hresult_name(result);
	ZEST_ALERT("Zest D3D12 Error 0x%08X (%s) in %s at line %d", (unsigned int)result, name, file, line);
	ZEST_APPEND_LOG(device->log_path.str, "Zest D3D12 Error: 0x%08X (%s) in %s at line %d", (unsigned int)result, name, file, line);
}

static const char *zest__dx_message_severity_name(D3D12_MESSAGE_SEVERITY severity) {
	switch (severity) {
		case D3D12_MESSAGE_SEVERITY_CORRUPTION: return "D3D12 CORRUPTION";
		case D3D12_MESSAGE_SEVERITY_ERROR: return "D3D12 ERROR";
		case D3D12_MESSAGE_SEVERITY_WARNING: return "D3D12 WARNING";
		case D3D12_MESSAGE_SEVERITY_INFO: return "D3D12 INFO";
		default: return "D3D12 MESSAGE";
	}
}

void __stdcall zest__dx_debug_callback(D3D12_MESSAGE_CATEGORY category, D3D12_MESSAGE_SEVERITY severity, D3D12_MESSAGE_ID id, LPCSTR description, void *context) {
	zest_device device = (zest_device)context;
	(void)category;
	if (device->log_path.str) {
		ZEST_APPEND_LOG(device->log_path.str, "Validation Layer: %s", description);
	}
	if (ZEST__FLAGGED(device->init_flags, zest_device_init_flag_log_validation_errors_to_console)) {
		ZEST_ALERT("%s", zest__dx_message_severity_name(severity));
		ZEST_PRINT("Validation Layer: %s / %i", description, (int)id);
		ZEST_PRINT("-------------------------------------------------------");
	}
	zest_vec_foreach(index, device->validation_debug_stops) {
		if ((int)id == device->validation_debug_stops[index]) {
			ZEST_DEBUG_STOP();
		}
	}
	//Severities run from corruption down to message, so this keeps info and message out of zest_GetValidationErrorCount
	if (ZEST__FLAGGED(device->init_flags, zest_device_init_flag_log_validation_errors_to_memory) && severity <= D3D12_MESSAGE_SEVERITY_WARNING) {
		if (!zest_map_valid_key(device->validation_errors, (zest_key)id)) {
			zest_text_t error_message = ZEST__ZERO_INIT(zest_text_t);
			zest_SetText(device->allocator, &error_message, description);
			zest_map_insert_key(device->allocator, device->validation_errors, (zest_key)id, error_message);
		}
	}
}
// -- End Error_logging

// -- Type_converters
DXGI_FORMAT zest__dx_to_dxgi_format(zest_format format) {
	switch (format) {
		case zest_format_r5g6b5_unorm_pack16: return DXGI_FORMAT_B5G6R5_UNORM;
		case zest_format_a1r5g5b5_unorm_pack16: return DXGI_FORMAT_B5G5R5A1_UNORM;
		case zest_format_r8_unorm: return DXGI_FORMAT_R8_UNORM;
		case zest_format_r8_snorm: return DXGI_FORMAT_R8_SNORM;
		case zest_format_r8_uint: return DXGI_FORMAT_R8_UINT;
		case zest_format_r8_sint: return DXGI_FORMAT_R8_SINT;
		case zest_format_r8g8_unorm: return DXGI_FORMAT_R8G8_UNORM;
		case zest_format_r8g8_snorm: return DXGI_FORMAT_R8G8_SNORM;
		case zest_format_r8g8_uint: return DXGI_FORMAT_R8G8_UINT;
		case zest_format_r8g8_sint: return DXGI_FORMAT_R8G8_SINT;
		case zest_format_r8g8b8a8_unorm: case zest_format_a8b8g8r8_unorm_pack32: return DXGI_FORMAT_R8G8B8A8_UNORM;
		case zest_format_r8g8b8a8_snorm: case zest_format_a8b8g8r8_snorm_pack32: return DXGI_FORMAT_R8G8B8A8_SNORM;
		case zest_format_r8g8b8a8_uint: case zest_format_a8b8g8r8_uint_pack32: return DXGI_FORMAT_R8G8B8A8_UINT;
		case zest_format_r8g8b8a8_sint: case zest_format_a8b8g8r8_sint_pack32: return DXGI_FORMAT_R8G8B8A8_SINT;
		case zest_format_r8g8b8a8_srgb: case zest_format_a8b8g8r8_srgb_pack32: return DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
		case zest_format_b8g8r8a8_unorm: return DXGI_FORMAT_B8G8R8A8_UNORM;
		case zest_format_b8g8r8a8_srgb: return DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;
		case zest_format_a2b10g10r10_unorm_pack32: return DXGI_FORMAT_R10G10B10A2_UNORM;
		case zest_format_a2b10g10r10_uint_pack32: return DXGI_FORMAT_R10G10B10A2_UINT;
		case zest_format_r16_unorm: return DXGI_FORMAT_R16_UNORM;
		case zest_format_r16_snorm: return DXGI_FORMAT_R16_SNORM;
		case zest_format_r16_uint: return DXGI_FORMAT_R16_UINT;
		case zest_format_r16_sint: return DXGI_FORMAT_R16_SINT;
		case zest_format_r16_sfloat: return DXGI_FORMAT_R16_FLOAT;
		case zest_format_r16g16_unorm: return DXGI_FORMAT_R16G16_UNORM;
		case zest_format_r16g16_snorm: return DXGI_FORMAT_R16G16_SNORM;
		case zest_format_r16g16_uint: return DXGI_FORMAT_R16G16_UINT;
		case zest_format_r16g16_sint: return DXGI_FORMAT_R16G16_SINT;
		case zest_format_r16g16_sfloat: return DXGI_FORMAT_R16G16_FLOAT;
		case zest_format_r16g16b16a16_unorm: return DXGI_FORMAT_R16G16B16A16_UNORM;
		case zest_format_r16g16b16a16_snorm: return DXGI_FORMAT_R16G16B16A16_SNORM;
		case zest_format_r16g16b16a16_uint: return DXGI_FORMAT_R16G16B16A16_UINT;
		case zest_format_r16g16b16a16_sint: return DXGI_FORMAT_R16G16B16A16_SINT;
		case zest_format_r16g16b16a16_sfloat: return DXGI_FORMAT_R16G16B16A16_FLOAT;
		case zest_format_r32_uint: return DXGI_FORMAT_R32_UINT;
		case zest_format_r32_sint: return DXGI_FORMAT_R32_SINT;
		case zest_format_r32_sfloat: return DXGI_FORMAT_R32_FLOAT;
		case zest_format_r32g32_uint: return DXGI_FORMAT_R32G32_UINT;
		case zest_format_r32g32_sint: return DXGI_FORMAT_R32G32_SINT;
		case zest_format_r32g32_sfloat: return DXGI_FORMAT_R32G32_FLOAT;
		case zest_format_r32g32b32_uint: return DXGI_FORMAT_R32G32B32_UINT;
		case zest_format_r32g32b32_sint: return DXGI_FORMAT_R32G32B32_SINT;
		case zest_format_r32g32b32_sfloat: return DXGI_FORMAT_R32G32B32_FLOAT;
		case zest_format_r32g32b32a32_uint: return DXGI_FORMAT_R32G32B32A32_UINT;
		case zest_format_r32g32b32a32_sint: return DXGI_FORMAT_R32G32B32A32_SINT;
		case zest_format_r32g32b32a32_sfloat: return DXGI_FORMAT_R32G32B32A32_FLOAT;
		case zest_format_b10g11r11_ufloat_pack32: return DXGI_FORMAT_R11G11B10_FLOAT;
		case zest_format_e5b9g9r9_ufloat_pack32: return DXGI_FORMAT_R9G9B9E5_SHAREDEXP;
		case zest_format_d16_unorm: return DXGI_FORMAT_D16_UNORM;
		case zest_format_x8_d24_unorm_pack32: case zest_format_d24_unorm_s8_uint: return DXGI_FORMAT_D24_UNORM_S8_UINT;
		case zest_format_d32_sfloat: return DXGI_FORMAT_D32_FLOAT;
		case zest_format_d32_sfloat_s8_uint: return DXGI_FORMAT_D32_FLOAT_S8X24_UINT;
		//DXGI has one BC1 layout, so the RGB variants share it the same way Metal's single BC1 format does
		case zest_format_bc1_rgb_unorm_block: case zest_format_bc1_rgba_unorm_block: return DXGI_FORMAT_BC1_UNORM;
		case zest_format_bc1_rgb_srgb_block: case zest_format_bc1_rgba_srgb_block: return DXGI_FORMAT_BC1_UNORM_SRGB;
		case zest_format_bc2_unorm_block: return DXGI_FORMAT_BC2_UNORM;
		case zest_format_bc2_srgb_block: return DXGI_FORMAT_BC2_UNORM_SRGB;
		case zest_format_bc3_unorm_block: return DXGI_FORMAT_BC3_UNORM;
		case zest_format_bc3_srgb_block: return DXGI_FORMAT_BC3_UNORM_SRGB;
		case zest_format_bc4_unorm_block: return DXGI_FORMAT_BC4_UNORM;
		case zest_format_bc4_snorm_block: return DXGI_FORMAT_BC4_SNORM;
		case zest_format_bc5_unorm_block: return DXGI_FORMAT_BC5_UNORM;
		case zest_format_bc5_snorm_block: return DXGI_FORMAT_BC5_SNORM;
		case zest_format_bc6h_ufloat_block: return DXGI_FORMAT_BC6H_UF16;
		case zest_format_bc6h_sfloat_block: return DXGI_FORMAT_BC6H_SF16;
		case zest_format_bc7_unorm_block: return DXGI_FORMAT_BC7_UNORM;
		case zest_format_bc7_srgb_block: return DXGI_FORMAT_BC7_UNORM_SRGB;
		default: return DXGI_FORMAT_UNKNOWN;
	}
}

zest_format zest__dx_from_dxgi_format(DXGI_FORMAT format) {
	switch (format) {
		case DXGI_FORMAT_B5G6R5_UNORM: return zest_format_r5g6b5_unorm_pack16;
		case DXGI_FORMAT_B5G5R5A1_UNORM: return zest_format_a1r5g5b5_unorm_pack16;
		case DXGI_FORMAT_R8_UNORM: return zest_format_r8_unorm;
		case DXGI_FORMAT_R8_SNORM: return zest_format_r8_snorm;
		case DXGI_FORMAT_R8_UINT: return zest_format_r8_uint;
		case DXGI_FORMAT_R8_SINT: return zest_format_r8_sint;
		case DXGI_FORMAT_R8G8_UNORM: return zest_format_r8g8_unorm;
		case DXGI_FORMAT_R8G8_SNORM: return zest_format_r8g8_snorm;
		case DXGI_FORMAT_R8G8_UINT: return zest_format_r8g8_uint;
		case DXGI_FORMAT_R8G8_SINT: return zest_format_r8g8_sint;
		case DXGI_FORMAT_R8G8B8A8_UNORM: return zest_format_r8g8b8a8_unorm;
		case DXGI_FORMAT_R8G8B8A8_SNORM: return zest_format_r8g8b8a8_snorm;
		case DXGI_FORMAT_R8G8B8A8_UINT: return zest_format_r8g8b8a8_uint;
		case DXGI_FORMAT_R8G8B8A8_SINT: return zest_format_r8g8b8a8_sint;
		case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB: return zest_format_r8g8b8a8_srgb;
		case DXGI_FORMAT_B8G8R8A8_UNORM: return zest_format_b8g8r8a8_unorm;
		case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB: return zest_format_b8g8r8a8_srgb;
		case DXGI_FORMAT_R10G10B10A2_UNORM: return zest_format_a2b10g10r10_unorm_pack32;
		case DXGI_FORMAT_R10G10B10A2_UINT: return zest_format_a2b10g10r10_uint_pack32;
		case DXGI_FORMAT_R16_UNORM: return zest_format_r16_unorm;
		case DXGI_FORMAT_R16_SNORM: return zest_format_r16_snorm;
		case DXGI_FORMAT_R16_UINT: return zest_format_r16_uint;
		case DXGI_FORMAT_R16_SINT: return zest_format_r16_sint;
		case DXGI_FORMAT_R16_FLOAT: return zest_format_r16_sfloat;
		case DXGI_FORMAT_R16G16_UNORM: return zest_format_r16g16_unorm;
		case DXGI_FORMAT_R16G16_SNORM: return zest_format_r16g16_snorm;
		case DXGI_FORMAT_R16G16_UINT: return zest_format_r16g16_uint;
		case DXGI_FORMAT_R16G16_SINT: return zest_format_r16g16_sint;
		case DXGI_FORMAT_R16G16_FLOAT: return zest_format_r16g16_sfloat;
		case DXGI_FORMAT_R16G16B16A16_UNORM: return zest_format_r16g16b16a16_unorm;
		case DXGI_FORMAT_R16G16B16A16_SNORM: return zest_format_r16g16b16a16_snorm;
		case DXGI_FORMAT_R16G16B16A16_UINT: return zest_format_r16g16b16a16_uint;
		case DXGI_FORMAT_R16G16B16A16_SINT: return zest_format_r16g16b16a16_sint;
		case DXGI_FORMAT_R16G16B16A16_FLOAT: return zest_format_r16g16b16a16_sfloat;
		case DXGI_FORMAT_R32_UINT: return zest_format_r32_uint;
		case DXGI_FORMAT_R32_SINT: return zest_format_r32_sint;
		case DXGI_FORMAT_R32_FLOAT: return zest_format_r32_sfloat;
		case DXGI_FORMAT_R32G32_UINT: return zest_format_r32g32_uint;
		case DXGI_FORMAT_R32G32_SINT: return zest_format_r32g32_sint;
		case DXGI_FORMAT_R32G32_FLOAT: return zest_format_r32g32_sfloat;
		case DXGI_FORMAT_R32G32B32_UINT: return zest_format_r32g32b32_uint;
		case DXGI_FORMAT_R32G32B32_SINT: return zest_format_r32g32b32_sint;
		case DXGI_FORMAT_R32G32B32_FLOAT: return zest_format_r32g32b32_sfloat;
		case DXGI_FORMAT_R32G32B32A32_UINT: return zest_format_r32g32b32a32_uint;
		case DXGI_FORMAT_R32G32B32A32_SINT: return zest_format_r32g32b32a32_sint;
		case DXGI_FORMAT_R32G32B32A32_FLOAT: return zest_format_r32g32b32a32_sfloat;
		case DXGI_FORMAT_R11G11B10_FLOAT: return zest_format_b10g11r11_ufloat_pack32;
		case DXGI_FORMAT_R9G9B9E5_SHAREDEXP: return zest_format_e5b9g9r9_ufloat_pack32;
		case DXGI_FORMAT_D16_UNORM: return zest_format_d16_unorm;
		case DXGI_FORMAT_D24_UNORM_S8_UINT: return zest_format_d24_unorm_s8_uint;
		case DXGI_FORMAT_D32_FLOAT: return zest_format_d32_sfloat;
		case DXGI_FORMAT_D32_FLOAT_S8X24_UINT: return zest_format_d32_sfloat_s8_uint;
		case DXGI_FORMAT_BC1_UNORM: return zest_format_bc1_rgba_unorm_block;
		case DXGI_FORMAT_BC1_UNORM_SRGB: return zest_format_bc1_rgba_srgb_block;
		case DXGI_FORMAT_BC2_UNORM: return zest_format_bc2_unorm_block;
		case DXGI_FORMAT_BC2_UNORM_SRGB: return zest_format_bc2_srgb_block;
		case DXGI_FORMAT_BC3_UNORM: return zest_format_bc3_unorm_block;
		case DXGI_FORMAT_BC3_UNORM_SRGB: return zest_format_bc3_srgb_block;
		case DXGI_FORMAT_BC4_UNORM: return zest_format_bc4_unorm_block;
		case DXGI_FORMAT_BC4_SNORM: return zest_format_bc4_snorm_block;
		case DXGI_FORMAT_BC5_UNORM: return zest_format_bc5_unorm_block;
		case DXGI_FORMAT_BC5_SNORM: return zest_format_bc5_snorm_block;
		case DXGI_FORMAT_BC6H_UF16: return zest_format_bc6h_ufloat_block;
		case DXGI_FORMAT_BC6H_SF16: return zest_format_bc6h_sfloat_block;
		case DXGI_FORMAT_BC7_UNORM: return zest_format_bc7_unorm_block;
		case DXGI_FORMAT_BC7_UNORM_SRGB: return zest_format_bc7_srgb_block;
		default: return zest_format_undefined;
	}
}

DXGI_FORMAT zest__dx_typeless_format(DXGI_FORMAT format) {
	switch (format) {
		case DXGI_FORMAT_D16_UNORM: return DXGI_FORMAT_R16_TYPELESS;
		case DXGI_FORMAT_D32_FLOAT: return DXGI_FORMAT_R32_TYPELESS;
		case DXGI_FORMAT_D24_UNORM_S8_UINT: return DXGI_FORMAT_R24G8_TYPELESS;
		case DXGI_FORMAT_D32_FLOAT_S8X24_UINT: return DXGI_FORMAT_R32G8X24_TYPELESS;
		default: return format;
	}
}

DXGI_FORMAT zest__dx_shader_view_format(DXGI_FORMAT format) {
	switch (format) {
		case DXGI_FORMAT_D16_UNORM: return DXGI_FORMAT_R16_UNORM;
		case DXGI_FORMAT_D32_FLOAT: return DXGI_FORMAT_R32_FLOAT;
		case DXGI_FORMAT_D24_UNORM_S8_UINT: return DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
		case DXGI_FORMAT_D32_FLOAT_S8X24_UINT: return DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS;
		default: return format;
	}
}

zest_uint zest__dx_image_heap_category(zest_image_flags flags, zest_sample_count_flags samples, D3D12_RESOURCE_HEAP_TIER tier) {
	if (samples > 1) {
		return ZEST_DX_HEAP_CATEGORY_MSAA;
	}
	if (tier < D3D12_RESOURCE_HEAP_TIER_2 && ZEST__FLAGGED(flags, zest_image_flag_color_attachment | zest_image_flag_depth_stencil_attachment)) {
		return ZEST_DX_HEAP_CATEGORY_TARGETS;
	}
	return ZEST_DX_HEAP_CATEGORY_TEXTURES;
}

static UINT zest__dx_component_swizzle(zest_component_swizzle swizzle, UINT identity) {
	switch (swizzle) {
		case zest_component_swizzle_zero: return D3D12_SHADER_COMPONENT_MAPPING_FORCE_VALUE_0;
		case zest_component_swizzle_one: return D3D12_SHADER_COMPONENT_MAPPING_FORCE_VALUE_1;
		case zest_component_swizzle_r: return D3D12_SHADER_COMPONENT_MAPPING_FROM_MEMORY_COMPONENT_0;
		case zest_component_swizzle_g: return D3D12_SHADER_COMPONENT_MAPPING_FROM_MEMORY_COMPONENT_1;
		case zest_component_swizzle_b: return D3D12_SHADER_COMPONENT_MAPPING_FROM_MEMORY_COMPONENT_2;
		case zest_component_swizzle_a: return D3D12_SHADER_COMPONENT_MAPPING_FROM_MEMORY_COMPONENT_3;
		default: return identity;
	}
}

UINT zest__dx_component_mapping(zest_component_mapping_t mapping) {
	return D3D12_ENCODE_SHADER_4_COMPONENT_MAPPING(zest__dx_component_swizzle(mapping.r, 0), zest__dx_component_swizzle(mapping.g, 1),
		zest__dx_component_swizzle(mapping.b, 2), zest__dx_component_swizzle(mapping.a, 3));
}

//Indexed by zest_sampler_address_mode
static const D3D12_TEXTURE_ADDRESS_MODE zest__dx_address_modes[] = {
	D3D12_TEXTURE_ADDRESS_MODE_WRAP, D3D12_TEXTURE_ADDRESS_MODE_MIRROR, D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
	D3D12_TEXTURE_ADDRESS_MODE_BORDER, D3D12_TEXTURE_ADDRESS_MODE_MIRROR_ONCE,
};

//Indexed by zest_compare_op
static const D3D12_COMPARISON_FUNC zest__dx_comparison_funcs[] = {
	D3D12_COMPARISON_FUNC_NEVER, D3D12_COMPARISON_FUNC_LESS, D3D12_COMPARISON_FUNC_EQUAL, D3D12_COMPARISON_FUNC_LESS_EQUAL,
	D3D12_COMPARISON_FUNC_GREATER, D3D12_COMPARISON_FUNC_NOT_EQUAL, D3D12_COMPARISON_FUNC_GREATER_EQUAL, D3D12_COMPARISON_FUNC_ALWAYS,
};

//Indexed by zest_border_color / 2, as the float and int variants of each colour share a value
static const FLOAT zest__dx_border_colors[3][4] = {
	{ 0.f, 0.f, 0.f, 0.f },
	{ 0.f, 0.f, 0.f, 1.f },
	{ 1.f, 1.f, 1.f, 1.f },
};

//Depth testing and sampling at once is only allowed in the direct queue's generic read layout
D3D12_BARRIER_LAYOUT zest__dx_depth_read_layout(zest_access_flags access, zest_device_queue_type queue_type) {
	if (ZEST__NOT_FLAGGED(access, zest_access_shader_read_bit | zest_access_input_attachment_read_bit)) {
		return D3D12_BARRIER_LAYOUT_DEPTH_STENCIL_READ;
	}
	if (queue_type == zest_queue_graphics && ZEST__FLAGGED(access, zest_access_depth_stencil_attachment_read_bit)) {
		return D3D12_BARRIER_LAYOUT_DIRECT_QUEUE_GENERIC_READ;
	}
	return D3D12_BARRIER_LAYOUT_SHADER_RESOURCE;
}

D3D12_BARRIER_LAYOUT zest__dx_barrier_layout(zest_image_layout layout, zest_access_flags access, zest_device_queue_type queue_type) {
	if (layout == zest_image_layout_undefined || layout == zest_image_layout_preinitialized) {
		return D3D12_BARRIER_LAYOUT_UNDEFINED;
	}
	//A copy queue can't move a texture into any other layout
	if (queue_type == zest_queue_transfer) {
		return D3D12_BARRIER_LAYOUT_COMMON;
	}
	switch (layout) {
		case zest_image_layout_general:
			if (ZEST__FLAGGED(access, zest_access_shader_write_bit)) {
				return D3D12_BARRIER_LAYOUT_UNORDERED_ACCESS;
			}
			return queue_type == zest_queue_compute ? D3D12_BARRIER_LAYOUT_COMPUTE_QUEUE_COMMON : D3D12_BARRIER_LAYOUT_DIRECT_QUEUE_COMMON;
		case zest_image_layout_color_attachment_optimal: return D3D12_BARRIER_LAYOUT_RENDER_TARGET;
		case zest_image_layout_depth_stencil_attachment_optimal:
		case zest_image_layout_depth_attachment_optimal:
		case zest_image_layout_stencil_attachment_optimal:
		case zest_image_layout_depth_read_only_stencil_attachment_optimal:
		case zest_image_layout_depth_attachment_stencil_read_only_optimal: return D3D12_BARRIER_LAYOUT_DEPTH_STENCIL_WRITE;
		case zest_image_layout_depth_stencil_read_only_optimal:
		case zest_image_layout_depth_read_only_optimal:
		case zest_image_layout_stencil_read_only_optimal: return zest__dx_depth_read_layout(access, queue_type);
		case zest_image_layout_shader_read_only_optimal: return D3D12_BARRIER_LAYOUT_SHADER_RESOURCE;
		//The combined layouts carry no format, so depth access is what marks a depth image
		case zest_image_layout_attachment_optimal:
			return ZEST__FLAGGED(access, zest_access_depth_stencil_attachment_read_bit | zest_access_depth_stencil_attachment_write_bit) ? D3D12_BARRIER_LAYOUT_DEPTH_STENCIL_WRITE : D3D12_BARRIER_LAYOUT_RENDER_TARGET;
		case zest_image_layout_read_only_optimal:
			return ZEST__FLAGGED(access, zest_access_depth_stencil_attachment_read_bit) ? zest__dx_depth_read_layout(access, queue_type) : D3D12_BARRIER_LAYOUT_SHADER_RESOURCE;
		case zest_image_layout_transfer_src_optimal: return D3D12_BARRIER_LAYOUT_COPY_SOURCE;
		case zest_image_layout_transfer_dst_optimal: return D3D12_BARRIER_LAYOUT_COPY_DEST;
		case zest_image_layout_present: return D3D12_BARRIER_LAYOUT_PRESENT;
		default: return D3D12_BARRIER_LAYOUT_COMMON;
	}
}

D3D12_BARRIER_ACCESS zest__dx_barrier_access(zest_access_flags access, zest_bool is_image, D3D12_BARRIER_LAYOUT layout) {
	//Common means every access the layout allows
	if (ZEST__FLAGGED(access, zest_access_memory_read_bit | zest_access_memory_write_bit)) {
		return D3D12_BARRIER_ACCESS_COMMON;
	}
	UINT result = 0;
	if (ZEST__FLAGGED(access, zest_access_indirect_command_read_bit)) result |= D3D12_BARRIER_ACCESS_INDIRECT_ARGUMENT;
	if (ZEST__FLAGGED(access, zest_access_index_read_bit)) result |= D3D12_BARRIER_ACCESS_INDEX_BUFFER;
	if (ZEST__FLAGGED(access, zest_access_vertex_attribute_read_bit)) result |= D3D12_BARRIER_ACCESS_VERTEX_BUFFER;
	if (ZEST__FLAGGED(access, zest_access_uniform_read_bit)) result |= D3D12_BARRIER_ACCESS_CONSTANT_BUFFER;
	if (ZEST__FLAGGED(access, zest_access_input_attachment_read_bit)) result |= D3D12_BARRIER_ACCESS_SHADER_RESOURCE;
	if (ZEST__FLAGGED(access, zest_access_shader_read_bit)) {
		if (!is_image) {
			//Both views exist at every storage buffer index
			result |= D3D12_BARRIER_ACCESS_SHADER_RESOURCE | D3D12_BARRIER_ACCESS_UNORDERED_ACCESS;
		} else {
			result |= layout == D3D12_BARRIER_LAYOUT_UNORDERED_ACCESS ? D3D12_BARRIER_ACCESS_UNORDERED_ACCESS : D3D12_BARRIER_ACCESS_SHADER_RESOURCE;
		}
	}
	if (ZEST__FLAGGED(access, zest_access_shader_write_bit)) result |= D3D12_BARRIER_ACCESS_UNORDERED_ACCESS;
	if (ZEST__FLAGGED(access, zest_access_color_attachment_read_bit | zest_access_color_attachment_write_bit)) result |= D3D12_BARRIER_ACCESS_RENDER_TARGET;
	//Depth stencil write already includes the read, and the write layout rejects the read bit
	if (ZEST__FLAGGED(access, zest_access_depth_stencil_attachment_write_bit)) {
		result |= D3D12_BARRIER_ACCESS_DEPTH_STENCIL_WRITE;
	} else if (ZEST__FLAGGED(access, zest_access_depth_stencil_attachment_read_bit)) {
		result |= D3D12_BARRIER_ACCESS_DEPTH_STENCIL_READ;
	}
	if (ZEST__FLAGGED(access, zest_access_transfer_read_bit)) result |= D3D12_BARRIER_ACCESS_COPY_SOURCE;
	if (ZEST__FLAGGED(access, zest_access_transfer_write_bit)) result |= D3D12_BARRIER_ACCESS_COPY_DEST;
	//Host access is dropped because fence completion already makes GPU work visible to the host
	return result ? (D3D12_BARRIER_ACCESS)result : D3D12_BARRIER_ACCESS_NO_ACCESS;
}

D3D12_BARRIER_SYNC zest__dx_barrier_sync(zest_pipeline_stage_flags stages, zest_bool is_before) {
	UINT result = 0;
	if (ZEST__FLAGGED(stages, zest_pipeline_stage_top_of_pipe_bit) && !is_before) result |= D3D12_BARRIER_SYNC_ALL;
	if (ZEST__FLAGGED(stages, zest_pipeline_stage_bottom_of_pipe_bit) && is_before) result |= D3D12_BARRIER_SYNC_ALL;
	if (ZEST__FLAGGED(stages, zest_pipeline_stage_draw_indirect_bit)) result |= D3D12_BARRIER_SYNC_EXECUTE_INDIRECT;
	if (ZEST__FLAGGED(stages, zest_pipeline_stage_vertex_input_bit)) result |= D3D12_BARRIER_SYNC_INDEX_INPUT | D3D12_BARRIER_SYNC_VERTEX_SHADING;
	if (ZEST__FLAGGED(stages, zest_pipeline_stage_index_input_bit)) result |= D3D12_BARRIER_SYNC_INDEX_INPUT;
	if (ZEST__FLAGGED(stages, zest_pipeline_stage_vertex_shader_bit | zest_pipeline_stage_tessellation_control_shader_bit |
		zest_pipeline_stage_tessellation_evaluation_shader_bit | zest_pipeline_stage_geometry_shader_bit)) result |= D3D12_BARRIER_SYNC_VERTEX_SHADING;
	if (ZEST__FLAGGED(stages, zest_pipeline_stage_fragment_shader_bit)) result |= D3D12_BARRIER_SYNC_PIXEL_SHADING;
	if (ZEST__FLAGGED(stages, zest_pipeline_stage_early_fragment_tests_bit | zest_pipeline_stage_late_fragment_tests_bit)) result |= D3D12_BARRIER_SYNC_DEPTH_STENCIL;
	if (ZEST__FLAGGED(stages, zest_pipeline_stage_color_attachment_output_bit)) result |= D3D12_BARRIER_SYNC_RENDER_TARGET;
	if (ZEST__FLAGGED(stages, zest_pipeline_stage_compute_shader_bit)) result |= D3D12_BARRIER_SYNC_COMPUTE_SHADING;
	if (ZEST__FLAGGED(stages, zest_pipeline_stage_transfer_bit)) result |= D3D12_BARRIER_SYNC_COPY | D3D12_BARRIER_SYNC_RESOLVE;
	if (ZEST__FLAGGED(stages, zest_pipeline_stage_all_graphics_bit)) result |= D3D12_BARRIER_SYNC_DRAW;
	if (ZEST__FLAGGED(stages, zest_pipeline_stage_all_commands_bit)) result |= D3D12_BARRIER_SYNC_ALL;
	//All already covers every narrower scope
	return (result & D3D12_BARRIER_SYNC_ALL) ? D3D12_BARRIER_SYNC_ALL : (D3D12_BARRIER_SYNC)result;
}

//An empty access needs no sync, and an access with no sync scope has to wait on everything
D3D12_BARRIER_SYNC zest__dx_normalise_sync(D3D12_BARRIER_SYNC sync, D3D12_BARRIER_ACCESS access) {
	if (access == D3D12_BARRIER_ACCESS_NO_ACCESS) {
		return D3D12_BARRIER_SYNC_NONE;
	}
	//The runtime rejects the resolve scope alongside an access list that has no resolve in it
	UINT trimmed = sync;
	if (access != D3D12_BARRIER_ACCESS_COMMON && !(access & (D3D12_BARRIER_ACCESS_RESOLVE_SOURCE | D3D12_BARRIER_ACCESS_RESOLVE_DEST))) {
		trimmed &= ~(UINT)D3D12_BARRIER_SYNC_RESOLVE;
	}
	return trimmed ? (D3D12_BARRIER_SYNC)trimmed : D3D12_BARRIER_SYNC_ALL;
}

zest_image_layout zest__dx_image_layout_from_resource_state(zest_resource_state state, zest_bool is_depth) {
	switch (state) {
		case zest_resource_state_undefined: return zest_image_layout_undefined;
		case zest_resource_state_shader_read: return is_depth ? zest_image_layout_depth_stencil_read_only_optimal : zest_image_layout_shader_read_only_optimal;
		case zest_resource_state_unordered_access: return zest_image_layout_general;
		case zest_resource_state_copy_src: return zest_image_layout_transfer_src_optimal;
		case zest_resource_state_copy_dst: return zest_image_layout_transfer_dst_optimal;
		case zest_resource_state_render_target: return zest_image_layout_color_attachment_optimal;
		case zest_resource_state_depth_stencil_write: return zest_image_layout_depth_stencil_attachment_optimal;
		case zest_resource_state_depth_stencil_read: return zest_image_layout_depth_stencil_read_only_optimal;
		case zest_resource_state_present: return zest_image_layout_present;
		default: break;
	}
	ZEST_ASSERT(0, "This resource state is not valid for an image transition (buffer states cannot be applied to images).");
	return zest_image_layout_undefined;
}

void zest__dx_layout_access_and_stage(zest_image_layout layout, zest_device_queue_type queue_type, zest_access_flags *access_out, zest_pipeline_stage_flags *stage_out) {
	zest_bool graphics = ZEST__FLAGGED(queue_type, zest_queue_graphics);
	zest_bool shaders = ZEST__FLAGGED(queue_type, zest_queue_graphics | zest_queue_compute);
	zest_access_flags access = 0;
	zest_pipeline_stage_flags stage = zest_pipeline_stage_bottom_of_pipe_bit;
	switch (layout) {
		case zest_image_layout_undefined:
			stage = zest_pipeline_stage_top_of_pipe_bit;
			break;
		case zest_image_layout_general:
			access = zest_access_shader_read_bit | zest_access_shader_write_bit;
			if (shaders) stage = zest_pipeline_stage_compute_shader_bit;
			break;
		case zest_image_layout_transfer_src_optimal:
			access = zest_access_transfer_read_bit;
			stage = zest_pipeline_stage_transfer_bit;
			break;
		case zest_image_layout_transfer_dst_optimal:
			access = zest_access_transfer_write_bit;
			stage = zest_pipeline_stage_transfer_bit;
			break;
		case zest_image_layout_shader_read_only_optimal:
		case zest_image_layout_depth_stencil_read_only_optimal: {
			access = layout == zest_image_layout_depth_stencil_read_only_optimal ? zest_access_depth_stencil_attachment_read_bit | zest_access_shader_read_bit : zest_access_shader_read_bit;
			zest_pipeline_stage_flags shader_stages = graphics ? zest_pipeline_stage_vertex_shader_bit | zest_pipeline_stage_fragment_shader_bit : 0;
			shader_stages |= shaders ? zest_pipeline_stage_compute_shader_bit : 0;
			stage = shader_stages ? shader_stages : zest_pipeline_stage_bottom_of_pipe_bit;
			break;
		}
		case zest_image_layout_color_attachment_optimal:
			access = zest_access_color_attachment_write_bit | zest_access_color_attachment_read_bit;
			if (graphics) stage = zest_pipeline_stage_color_attachment_output_bit;
			break;
		case zest_image_layout_depth_stencil_attachment_optimal:
			access = zest_access_depth_stencil_attachment_write_bit | zest_access_depth_stencil_attachment_read_bit;
			if (graphics) stage = zest_pipeline_stage_early_fragment_tests_bit | zest_pipeline_stage_late_fragment_tests_bit;
			break;
		case zest_image_layout_present:
			access = zest_access_memory_read_bit;
			if (graphics) stage = zest_pipeline_stage_color_attachment_output_bit;
			break;
		default:
			access = zest_access_memory_read_bit | zest_access_memory_write_bit;
			break;
	}
	//Each queue type can only make the accesses its command lists support
	if (queue_type == zest_queue_transfer) {
		access &= zest_access_transfer_read_bit | zest_access_transfer_write_bit;
	} else if (!graphics) {
		access &= ~(zest_access_color_attachment_read_bit | zest_access_color_attachment_write_bit | zest_access_depth_stencil_attachment_read_bit |
			zest_access_depth_stencil_attachment_write_bit | zest_access_input_attachment_read_bit | zest_access_index_read_bit | zest_access_vertex_attribute_read_bit);
	}
	*access_out = access;
	*stage_out = stage;
}

//Families 0, 1 and 2 are direct, compute and copy, which line up with the graphics, compute and transfer bits
zest_device_queue_type zest__dx_queue_type(zest_queue queue) {
	return (zest_device_queue_type)(1u << queue->family_index);
}

zest_uint zest__dx_plane_count(zest_format format) {
	switch (format) {
		//x8_d24 maps to D24_UNORM_S8_UINT, so it carries a stencil plane too
		case zest_format_x8_d24_unorm_pack32:
		case zest_format_d24_unorm_s8_uint:
		case zest_format_d32_sfloat_s8_uint: return 2;
		default: return 1;
	}
}
// -- End Type_converters

// -- Inline_helpers
D3D12_CPU_DESCRIPTOR_HANDLE zest__dx_cpu_heap_start(ID3D12DescriptorHeap *heap) {
#ifdef __cplusplus
	return ZEST_DX_CALL0(heap, GetCPUDescriptorHandleForHeapStart);
#else
	D3D12_CPU_DESCRIPTOR_HANDLE handle;
	ZEST_DX_CALL(heap, GetCPUDescriptorHandleForHeapStart, &handle);
	return handle;
#endif
}

D3D12_GPU_DESCRIPTOR_HANDLE zest__dx_gpu_heap_start(ID3D12DescriptorHeap *heap) {
#ifdef __cplusplus
	return ZEST_DX_CALL0(heap, GetGPUDescriptorHandleForHeapStart);
#else
	D3D12_GPU_DESCRIPTOR_HANDLE handle;
	ZEST_DX_CALL(heap, GetGPUDescriptorHandleForHeapStart, &handle);
	return handle;
#endif
}

D3D12_RESOURCE_ALLOCATION_INFO zest__dx_resource_allocation_info(ID3D12Device10 *device, const D3D12_RESOURCE_DESC1 *description) {
#ifdef __cplusplus
	return ZEST_DX_CALL(device, GetResourceAllocationInfo2, 0, 1, description, NULL);
#else
	D3D12_RESOURCE_ALLOCATION_INFO info;
	ZEST_DX_CALL(device, GetResourceAllocationInfo2, &info, 0, 1, description, NULL);
	return info;
#endif
}

D3D12_RESOURCE_DESC1 zest__dx_resource_description(ID3D12Resource *resource) {
#ifdef __cplusplus
	D3D12_RESOURCE_DESC description = ZEST_DX_CALL0(resource, GetDesc);
#else
	D3D12_RESOURCE_DESC description;
	ZEST_DX_CALL(resource, GetDesc, &description);
#endif
	//Copied field by field so the padding never reaches SamplerFeedbackMipRegion
	D3D12_RESOURCE_DESC1 description1 = ZEST__ZERO_INIT(D3D12_RESOURCE_DESC1);
	description1.Dimension = description.Dimension;
	description1.Alignment = description.Alignment;
	description1.Width = description.Width;
	description1.Height = description.Height;
	description1.DepthOrArraySize = description.DepthOrArraySize;
	description1.MipLevels = description.MipLevels;
	description1.Format = description.Format;
	description1.SampleDesc = description.SampleDesc;
	description1.Layout = description.Layout;
	description1.Flags = description.Flags;
	return description1;
}
// -- End Inline_helpers

// -- Initialisation_functions
zest_device_builder zest_BeginD3D12DeviceBuilder(zest_device_init_flags flags) {
	zest__register_platform(zest_platform_d3d12, zest__dx_initialise_platform_callbacks);
	zest_device_builder builder = zest__begin_device_builder();
	builder->graphics_queue_count = -1;
	builder->compute_queue_count = -1;
	builder->transfer_queue_count = -1;
	builder->platform = zest_platform_d3d12;
	builder->cached_shader_path = "./dxil/";
	builder->flags = flags;
	return builder;
}

zest_bool zest__dx_meets_minimum_spec(const zest_dx_feature_support_t *support, const char **missing_out) {
	const char *missing = NULL;
	if (support->feature_level < D3D_FEATURE_LEVEL_12_0) {
		missing = "feature level 12_0";
	} else if (support->resource_binding_tier < D3D12_RESOURCE_BINDING_TIER_3) {
		missing = "resource binding tier 3";
	} else if (!support->enhanced_barriers) {
		missing = "enhanced barriers";
	} else if (!support->inverted_viewport_height) {
		missing = "inverted viewport height";
	}
	if (!missing) {
		return ZEST_TRUE;
	}
	if (missing_out) {
		*missing_out = missing;
	}
	return ZEST_FALSE;
}

zest_capability_flags zest__dx_translate_capabilities(const zest_dx_feature_support_t *support) {
	zest_capability_flags supported = zest_capability_multi_draw_indirect |
		zest_capability_nonuniform_storage_buffer_indexing | zest_capability_nonuniform_uniform_buffer_indexing |
		zest_capability_nonuniform_storage_image_indexing | zest_capability_nonuniform_sampled_image_indexing |
		zest_capability_anisotropic_filtering | zest_capability_wireframe | zest_capability_image_cube_array |
		zest_capability_tessellation | zest_capability_geometry_shader |
		zest_capability_fragment_stores_and_atomics | zest_capability_texture_compression_bc;
	if (support->int64_shader_ops) {
		supported |= zest_capability_shader_int64;
	}
	return supported;
}

#define ZEST__DX_CHECK_FEATURE(native_device, feature, data) SUCCEEDED(ZEST_DX_CALL((native_device), CheckFeatureSupport, (feature), &(data), (UINT)sizeof(data)))
void zest__dx_query_feature_support(ID3D12Device *native_device, IDXGIFactory6 *factory, zest_dx_feature_support_t *support) {
	*support = ZEST__ZERO_INIT(zest_dx_feature_support_t);

	static const D3D_FEATURE_LEVEL requested_levels[] = { D3D_FEATURE_LEVEL_12_2, D3D_FEATURE_LEVEL_12_1, D3D_FEATURE_LEVEL_12_0, D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0 };
	D3D12_FEATURE_DATA_FEATURE_LEVELS feature_levels = ZEST__ZERO_INIT(D3D12_FEATURE_DATA_FEATURE_LEVELS);
	feature_levels.NumFeatureLevels = (UINT)(sizeof(requested_levels) / sizeof(requested_levels[0]));
	feature_levels.pFeatureLevelsRequested = requested_levels;
	//The device was created at 12_0, so a failed query still means at least that
	support->feature_level = D3D_FEATURE_LEVEL_12_0;
	if (!zest__dx_forced_feature_level_failure && ZEST__DX_CHECK_FEATURE(native_device, D3D12_FEATURE_FEATURE_LEVELS, feature_levels)) {
		support->feature_level = feature_levels.MaxSupportedFeatureLevel;
	}

	D3D12_FEATURE_DATA_D3D12_OPTIONS options = ZEST__ZERO_INIT(D3D12_FEATURE_DATA_D3D12_OPTIONS);
	if (ZEST__DX_CHECK_FEATURE(native_device, D3D12_FEATURE_D3D12_OPTIONS, options)) {
		support->resource_binding_tier = options.ResourceBindingTier;
		support->resource_heap_tier = options.ResourceHeapTier;
	}

	D3D12_FEATURE_DATA_D3D12_OPTIONS1 options1 = ZEST__ZERO_INIT(D3D12_FEATURE_DATA_D3D12_OPTIONS1);
	if (ZEST__DX_CHECK_FEATURE(native_device, D3D12_FEATURE_D3D12_OPTIONS1, options1)) {
		support->int64_shader_ops = options1.Int64ShaderOps ? ZEST_TRUE : ZEST_FALSE;
	}

	D3D12_FEATURE_DATA_D3D12_OPTIONS3 options3 = ZEST__ZERO_INIT(D3D12_FEATURE_DATA_D3D12_OPTIONS3);
	if (ZEST__DX_CHECK_FEATURE(native_device, D3D12_FEATURE_D3D12_OPTIONS3, options3)) {
		support->view_instancing_tier = options3.ViewInstancingTier;
		support->copy_queue_timestamps = options3.CopyQueueTimestampQueriesSupported ? ZEST_TRUE : ZEST_FALSE;
	}

	D3D12_FEATURE_DATA_D3D12_OPTIONS12 options12 = ZEST__ZERO_INIT(D3D12_FEATURE_DATA_D3D12_OPTIONS12);
	if (ZEST__DX_CHECK_FEATURE(native_device, D3D12_FEATURE_D3D12_OPTIONS12, options12)) {
		support->enhanced_barriers = options12.EnhancedBarriersSupported ? ZEST_TRUE : ZEST_FALSE;
	}

	D3D12_FEATURE_DATA_D3D12_OPTIONS13 options13 = ZEST__ZERO_INIT(D3D12_FEATURE_DATA_D3D12_OPTIONS13);
	if (ZEST__DX_CHECK_FEATURE(native_device, D3D12_FEATURE_D3D12_OPTIONS13, options13)) {
		support->unrestricted_copy_pitch = options13.UnrestrictedBufferTextureCopyPitchSupported ? ZEST_TRUE : ZEST_FALSE;
		support->inverted_viewport_height = options13.InvertedViewportHeightFlipsYSupported ? ZEST_TRUE : ZEST_FALSE;
	}

	D3D12_FEATURE_DATA_D3D12_OPTIONS16 options16 = ZEST__ZERO_INIT(D3D12_FEATURE_DATA_D3D12_OPTIONS16);
	if (ZEST__DX_CHECK_FEATURE(native_device, D3D12_FEATURE_D3D12_OPTIONS16, options16)) {
		support->dynamic_depth_bias = options16.DynamicDepthBiasSupported ? ZEST_TRUE : ZEST_FALSE;
		support->gpu_upload_heap = options16.GPUUploadHeapSupported ? ZEST_TRUE : ZEST_FALSE;
	}

	//A runtime that doesn't know the requested model rejects the query, so step down until one is accepted
	static const D3D_SHADER_MODEL shader_models[] = { D3D_SHADER_MODEL_6_6, D3D_SHADER_MODEL_6_5, D3D_SHADER_MODEL_6_4, D3D_SHADER_MODEL_6_3, D3D_SHADER_MODEL_6_2, D3D_SHADER_MODEL_6_1, D3D_SHADER_MODEL_6_0, D3D_SHADER_MODEL_5_1 };
	for (zest_uint model_index = 0; model_index != sizeof(shader_models) / sizeof(shader_models[0]); ++model_index) {
		D3D12_FEATURE_DATA_SHADER_MODEL shader_model = ZEST__ZERO_INIT(D3D12_FEATURE_DATA_SHADER_MODEL);
		shader_model.HighestShaderModel = shader_models[model_index];
		if (ZEST__DX_CHECK_FEATURE(native_device, D3D12_FEATURE_SHADER_MODEL, shader_model)) {
			support->shader_model = shader_model.HighestShaderModel;
			break;
		}
	}

	BOOL allow_tearing = FALSE;
	if (SUCCEEDED(ZEST_DX_CALL(factory, CheckFeatureSupport, DXGI_FEATURE_PRESENT_ALLOW_TEARING, &allow_tearing, (UINT)sizeof(allow_tearing)))) {
		support->tearing = allow_tearing ? ZEST_TRUE : ZEST_FALSE;
	}
}
#undef ZEST__DX_CHECK_FEATURE

//Releases everything it creates, so it touches neither the release count nor a later existing device check
zest_bool zest__dx_warp_meets_minimum_spec(const char **missing_out) {
	IDXGIFactory6 *factory = NULL;
	IDXGIAdapter4 *adapter = NULL;
	ID3D12Device *native_device = NULL;
	const char *missing = "a WARP adapter";
	if (FAILED(CreateDXGIFactory2(0, ZEST_DX_IID(IDXGIFactory6), (void **)&factory))) {
		factory = NULL;
	} else if (FAILED(ZEST_DX_CALL(factory, EnumWarpAdapter, ZEST_DX_IID(IDXGIAdapter4), (void **)&adapter))) {
		adapter = NULL;
	} else if (FAILED(D3D12CreateDevice((IUnknown *)adapter, D3D_FEATURE_LEVEL_12_0, ZEST_DX_IID(ID3D12Device), (void **)&native_device))) {
		native_device = NULL;
		missing = "feature level 12_0";
	} else {
		zest_dx_feature_support_t support;
		zest__dx_query_feature_support(native_device, factory, &support);
		missing = zest__dx_forced_missing_feature;
		if (!missing) {
			zest__dx_meets_minimum_spec(&support, &missing);
		}
	}
	ZEST_DX_RELEASE(native_device);
	ZEST_DX_RELEASE(adapter);
	ZEST_DX_RELEASE(factory);
	if (missing_out) {
		*missing_out = missing;
	}
	return missing == NULL;
}

//Runs under zest__dx_debug_layer_lock, and releases the debug interface it's given
void zest__dx_enable_debug_layer(zest_device device, ID3D12Debug3 *debug) {
	const char *log_path = device->log_path.str;
	ZEST_DX_CALL0(debug, EnableDebugLayer);
	zest__dx_debug_layer_enabled = ZEST_TRUE;
	//Matched exactly because the sync flag also carries the plain validation bit
	zest_uint gpu_validation_flags = zest_device_init_flag_enable_validation_layers_with_sync;
	if ((device->setup_info.flags & gpu_validation_flags) == gpu_validation_flags) {
		ZEST_DX_CALL(debug, SetEnableGPUBasedValidation, TRUE);
		ZEST_APPEND_LOG(log_path, "D3D12 GPU-based validation enabled");
	}
	ZEST_DX_RELEASE(debug);

	ID3D12DeviceRemovedExtendedDataSettings1 *dred_settings = NULL;
	HRESULT result = D3D12GetDebugInterface(ZEST_DX_IID(ID3D12DeviceRemovedExtendedDataSettings1), (void **)&dred_settings);
	if (SUCCEEDED(result)) {
		ZEST_DX_CALL(dred_settings, SetAutoBreadcrumbsEnablement, D3D12_DRED_ENABLEMENT_FORCED_ON);
		ZEST_DX_CALL(dred_settings, SetPageFaultEnablement, D3D12_DRED_ENABLEMENT_FORCED_ON);
		ZEST_DX_RELEASE(dred_settings);
	} else {
		ZEST_APPEND_LOG(log_path, "D3D12 DRED settings aren't available (0x%08X), so device removal reports won't include breadcrumbs or page faults.", (unsigned int)result);
	}
	ZEST_APPEND_LOG(log_path, "D3D12 debug layer enabled");
}

//Enables the debug layer when asked and counts the device as live, refusing only when enabling would remove a live device
zest_bool zest__dx_prepare_debug_layer(zest_device device) {
	const char *log_path = device->log_path.str;
	zest_dx_device_backend_t *backend = ZEST_DX_BACKEND(device, device);
	AcquireSRWLockExclusive(&zest__dx_debug_layer_lock);
	if (ZEST__FLAGGED(device->setup_info.flags, zest_device_init_flag_enable_validation_layers)) {
		ID3D12Debug3 *debug = NULL;
		HRESULT result = zest__dx_forced_debug_layer_unavailable ? E_NOINTERFACE : D3D12GetDebugInterface(ZEST_DX_IID(ID3D12Debug3), (void **)&debug);
		if (FAILED(result)) {
			ZEST_APPEND_LOG(log_path, "The D3D12 debug layer isn't available (0x%08X). Disabling validation.", (unsigned int)result);
			ZEST__UNFLAG(device->setup_info.flags, zest_device_init_flag_enable_validation_layers);
		} else if (!zest__dx_debug_layer_enabled && InterlockedCompareExchange(&zest__dx_live_device_count, 0, 0) > 0) {
			ZEST_DX_RELEASE(debug);
			ReleaseSRWLockExclusive(&zest__dx_debug_layer_lock);
			ZEST_APPEND_LOG(log_path, "A D3D12 device with validation can't be created while a device without validation exists, because enabling the debug layer removes the existing device. Create the validation device first, or destroy the other device.");
			ZEST_ALERT("A D3D12 device with validation can't be created while a device without validation exists, because enabling the debug layer removes the existing device. Create the validation device first, or destroy the other device.");
			return ZEST_FALSE;
		} else {
			zest__dx_enable_debug_layer(device, debug);
		}
	} else if (zest__dx_debug_layer_enabled) {
		ZEST_APPEND_LOG(log_path, "The D3D12 debug layer was enabled earlier in this process, so it stays on for this device.");
	}
	//Counted before its native device exists, so a validation device can't enable the layer between the two
	InterlockedIncrement(&zest__dx_live_device_count);
	backend->counted_as_live = ZEST_TRUE;
	ReleaseSRWLockExclusive(&zest__dx_debug_layer_lock);
	return ZEST_TRUE;
}

zest_bool zest__dx_select_adapter(zest_device device) {
	zest_dx_device_backend_t *backend = ZEST_DX_BACKEND(device, device);
	const char *log_path = device->log_path.str;
	zest_bool use_warp = ZEST__FLAGGED(device->init_flags, zest_device_init_flag_use_warp);
	const char *last_missing = NULL;
	zest_uint adapters_tried = 0;
	zest_uint adapters_in_use = 0;
	ZEST_APPEND_LOG(log_path, "=== Device Compatibility Report ===");
	for (zest_uint adapter_index = 0; !use_warp || adapter_index == 0; ++adapter_index) {
		IDXGIAdapter4 *adapter = NULL;
		HRESULT result = use_warp
			? ZEST_DX_CALL(backend->factory, EnumWarpAdapter, ZEST_DX_IID(IDXGIAdapter4), (void **)&adapter)
			: ZEST_DX_CALL(backend->factory, EnumAdapterByGpuPreference, adapter_index, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, ZEST_DX_IID(IDXGIAdapter4), (void **)&adapter);
		if (result == DXGI_ERROR_NOT_FOUND) {
			break;
		}
		if (FAILED(result)) {
			ZEST_APPEND_LOG(log_path, "Adapter %u: couldn't be enumerated (0x%08X, %s)", adapter_index, (unsigned int)result, zest__dx_hresult_name(result));
			if (result == E_NOINTERFACE) {
				continue;
			}
			break;
		}

		DXGI_ADAPTER_DESC3 description = ZEST__ZERO_INIT(DXGI_ADAPTER_DESC3);
		ZEST_DX_CALL(adapter, GetDesc3, &description);
		char adapter_name[512];
		if (!WideCharToMultiByte(CP_UTF8, 0, description.Description, -1, adapter_name, (int)sizeof(adapter_name), NULL, NULL)) {
			adapter_name[0] = '\0';
		}
		if (!use_warp && (description.Flags & DXGI_ADAPTER_FLAG3_SOFTWARE)) {
			ZEST_APPEND_LOG(log_path, "Adapter %u: %s skipped, software adapter", adapter_index, adapter_name);
			ZEST_DX_RELEASE(adapter);
			continue;
		}
		adapters_tried++;
		ZEST_APPEND_LOG(log_path, "Adapter %u: %s, Vendor: 0x%04X, Device ID: 0x%04X, Dedicated video memory: %llu MB", adapter_index, adapter_name,
			description.VendorId, description.DeviceId, (zest_ull)(description.DedicatedVideoMemory / (1024 * 1024)));

		ID3D12Device *native_device = NULL;
		const char *missing = NULL;
		zest_dx_feature_support_t support = ZEST__ZERO_INIT(zest_dx_feature_support_t);
		result = D3D12CreateDevice((IUnknown *)adapter, D3D_FEATURE_LEVEL_12_0, ZEST_DX_IID(ID3D12Device), (void **)&native_device);
		if (FAILED(result)) {
			ZEST_APPEND_LOG(log_path, "  D3D12CreateDevice at feature level 12_0 failed (0x%08X, %s)", (unsigned int)result, zest__dx_hresult_name(result));
			native_device = NULL;
			missing = "feature level 12_0";
		} else if (zest__dx_device_already_exists(native_device)) {
			//The other Zest device still holds it, so this isn't a final release and the release count stays as it is
			ZEST_DX_CALL0(native_device, Release);
			native_device = NULL;
			ZEST_APPEND_LOG(log_path, "  REJECTED: a D3D12 device already exists on this adapter in this process, from another Zest device or from other code, and D3D12 shares one device per adapter.");
			adapters_in_use++;
			ZEST_DX_RELEASE(adapter);
			continue;
		} else {
			zest__dx_query_feature_support(native_device, backend->factory, &support);
			if (zest__dx_forced_restricted_copy_pitch) {
				support.unrestricted_copy_pitch = ZEST_FALSE;
			}
			ZEST_APPEND_LOG(log_path, "  Feature level: %u_%u, Resource binding tier: %i, Enhanced barriers: %s, Inverted viewport height: %s",
				(unsigned int)(support.feature_level >> 12), (unsigned int)((support.feature_level >> 8) & 0xF), (int)support.resource_binding_tier,
				support.enhanced_barriers ? "yes" : "no", support.inverted_viewport_height ? "yes" : "no");
			if (zest__dx_forced_missing_feature) {
				missing = zest__dx_forced_missing_feature;
			} else {
				zest__dx_meets_minimum_spec(&support, &missing);
			}
		}

		if (!missing) {
			backend->adapter = adapter;
			backend->device = native_device;
			backend->support = support;
			ZEST_APPEND_LOG(log_path, "Selected adapter %u: %s", adapter_index, adapter_name);
			return ZEST_TRUE;
		}
		ZEST_APPEND_LOG(log_path, "  MISSING (required): %s", missing);
		last_missing = missing;
		if (native_device) {
			zest_uint release_count = (zest_uint)ZEST_DX_CALL0(native_device, Release);
			zest__dx_device_release_count = ZEST__MAX(zest__dx_device_release_count, release_count);
		}
		ZEST_DX_RELEASE(adapter);
	}

	if (!adapters_tried) {
		ZEST_APPEND_LOG(log_path, "Fatal: no D3D12 adapters were found.");
		ZEST_ALERT("No D3D12 adapters were found.");
		return ZEST_FALSE;
	}
	if (adapters_in_use) {
		ZEST_APPEND_LOG(log_path, "Fatal: a D3D12 device already exists on %u adapter(s) in this process, from another Zest device or from other code, and D3D12 shares one device per adapter. No other adapter meets the Zest minimum spec (see REJECTED and MISSING lines above).", adapters_in_use);
		ZEST_ALERT("A D3D12 device already exists on this adapter in this process, from another Zest device or from other code, and D3D12 shares one device per adapter.");
		return ZEST_FALSE;
	}
	ZEST_APPEND_LOG(log_path, "Fatal: no adapter meets the Zest minimum spec (see MISSING lines above).");
	ZEST_ALERT("GPU does not meet the Zest minimum spec, missing %s. See the log for each adapter tried.", last_missing);
	return ZEST_FALSE;
}

//D3D12CreateDevice hands back the live device when its adapter already has one, which then holds more than the new reference
zest_bool zest__dx_device_already_exists(ID3D12Device *native_device) {
	ZEST_DX_CALL0(native_device, AddRef);
	return ZEST_DX_CALL0(native_device, Release) > 1;
}

void zest__dx_setup_info_queue(zest_device device) {
	zest_dx_device_backend_t *backend = ZEST_DX_BACKEND(device, device);
	const char *log_path = device->log_path.str;
	HRESULT result = ZEST_DX_CALL(backend->device, QueryInterface, ZEST_DX_IID(ID3D12InfoQueue1), (void **)&backend->info_queue);
	if (SUCCEEDED(result)) {
		D3D12_MESSAGE_ID denied_messages[] = {
			D3D12_MESSAGE_ID_CLEARRENDERTARGETVIEW_MISMATCHINGCLEARVALUE,	//Zest sets clear values per pass, not per image
			D3D12_MESSAGE_ID_CLEARDEPTHSTENCILVIEW_MISMATCHINGCLEARVALUE,	//Zest sets clear values per pass, not per image
			D3D12_MESSAGE_ID_NON_OPTIMAL_BARRIER_ONLY_EXECUTE_COMMAND_LISTS,	//Immediate transitions legitimately submit barrier-only lists
		};
		//The debug layer reports every object create and destroy at info level, so only warnings and above get through
		D3D12_MESSAGE_SEVERITY denied_severities[] = {
			D3D12_MESSAGE_SEVERITY_INFO,
			D3D12_MESSAGE_SEVERITY_MESSAGE,
		};
		D3D12_INFO_QUEUE_FILTER filter = ZEST__ZERO_INIT(D3D12_INFO_QUEUE_FILTER);
		filter.DenyList.NumIDs = (UINT)(sizeof(denied_messages) / sizeof(denied_messages[0]));
		filter.DenyList.pIDList = denied_messages;
		filter.DenyList.NumSeverities = (UINT)(sizeof(denied_severities) / sizeof(denied_severities[0]));
		filter.DenyList.pSeverityList = denied_severities;
		result = ZEST_DX_CALL(backend->info_queue, PushStorageFilter, &filter);
		if (FAILED(result)) {
			ZEST_APPEND_LOG(log_path, "The D3D12 info queue deny list couldn't be pushed (0x%08X)", (unsigned int)result);
		}
		result = ZEST_DX_CALL(backend->info_queue, RegisterMessageCallback, zest__dx_debug_callback, D3D12_MESSAGE_CALLBACK_FLAG_NONE, device, &backend->info_queue_cookie);
		if (SUCCEEDED(result)) {
			ZEST_APPEND_LOG(log_path, "D3D12 validation messages are routed through the info queue callback");
			return;
		}
		//Holding the info queue means a callback is registered, which the cleanup relies on
		ZEST_DX_RELEASE(backend->info_queue);
	}
	backend->info_queue = NULL;
	ZEST_APPEND_LOG(log_path, "The D3D12 info queue callback isn't available (0x%08X), so validation messages only reach the debugger output.", (unsigned int)result);
	ZEST_ALERT("The D3D12 info queue callback isn't available, so validation messages only reach the debugger output.");
}

zest_bool zest__dx_query_device_capabilities(zest_device device) {
	zest_dx_device_backend_t *backend = ZEST_DX_BACKEND(device, device);
	const zest_dx_feature_support_t *support = &backend->support;
	const char *log_path = device->log_path.str;

	zest_capability_flags supported = zest__dx_translate_capabilities(support);
	device->capabilities.supported = supported;
	device->capabilities.meets_minimum_spec = ZEST_TRUE;
	zest__resolve_enabled_capabilities(device, supported);

	device->max_image_size = D3D12_REQ_TEXTURE2D_U_OR_V_DIMENSION;
	device->max_multiview_view_count = support->view_instancing_tier != D3D12_VIEW_INSTANCING_TIER_NOT_SUPPORTED ? D3D12_MAX_VIEW_INSTANCE_COUNT : 1;
	device->min_uniform_buffer_offset_alignment = D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT;
	device->max_uniform_buffer_size = 65536;
	device->max_storage_buffer_size = (1u << 27) * 4;
	device->buffer_offset_granularity = ZEST_BUFFER_OFFSET_GRANULARITY;
	//No allocation limit, so the shared code's limit warnings stay quiet
	device->max_memory_allocation_count = 0;
	device->max_bindless_samplers = D3D12_MAX_SHADER_VISIBLE_SAMPLER_HEAP_SIZE;
	//Nine resource regions of 100000 stay under the 1,000,000 descriptor shader visible heap limit
	device->max_bindless_sampled_images = 100000;
	device->max_bindless_storage_buffers = 100000;
	device->max_bindless_storage_images = 100000;
	device->max_bindless_uniform_buffers = 100000;
	ZEST_APPEND_LOG(log_path, "Bindless descriptor ceilings: samplers %u, sampled images %u, storage buffers %u, storage images %u, uniform buffers %u",
		device->max_bindless_samplers, device->max_bindless_sampled_images, device->max_bindless_storage_buffers,
		device->max_bindless_storage_images, device->max_bindless_uniform_buffers);

	ZEST_APPEND_LOG(log_path, "D3D12 features: shader model %u.%u, resource heap tier %i, view instancing tier %i, unrestricted copy pitch: %s, dynamic depth bias: %s, GPU upload heap: %s, copy queue timestamps: %s, tearing: %s",
		(unsigned int)(support->shader_model >> 4), (unsigned int)(support->shader_model & 0xF), (int)support->resource_heap_tier, (int)support->view_instancing_tier,
		support->unrestricted_copy_pitch ? "yes" : "no", support->dynamic_depth_bias ? "yes" : "no", support->gpu_upload_heap ? "yes" : "no",
		support->copy_queue_timestamps ? "yes" : "no", support->tearing ? "yes" : "no");
	zest__log_supported_capabilities(device, supported);
	return ZEST_TRUE;
}

zest_bool zest__dx_create_queues(zest_device device) {
	const char *log_path = device->log_path.str;
	zest_vec_resize(device->allocator, device->queue_families, ZEST__DX_QUEUE_FAMILY_COUNT);
	ZEST_APPEND_LOG(log_path, "Iterate available queues:");
	for (zest_uint family_index = 0; family_index != ZEST__DX_QUEUE_FAMILY_COUNT; ++family_index) {
		const zest_dx_queue_family_t *family = &zest__dx_queue_families[family_index];
		zest_queue_manager manager = (zest_queue_manager)ZEST__NEW(device->allocator, zest_queue_manager);
		*manager = ZEST__ZERO_INIT(zest_queue_manager_t);
		manager->family_index = family_index;
		manager->queue_count = family->queue_count;
		manager->type = family->queue_types;
		manager->free_queues = (int)family->queue_count;
		device->queue_families[family_index] = manager;
		ZEST_APPEND_LOG(log_path, "Index: %u) %s, Queue count: %u", family_index, family->name, family->queue_count);
	}

	//Each request falls back only to a more capable family
	static const zest_uint graphics_order[] = { 0 };
	static const zest_uint compute_order[] = { 1, 0 };
	static const zest_uint transfer_order[] = { 2, 1, 0 };
	zest__dx_add_queue_manager_list(device, zest_queue_graphics, graphics_order, 1);
	zest__dx_add_queue_manager_list(device, zest_queue_compute, compute_order, 2);
	zest__dx_add_queue_manager_list(device, zest_queue_transfer, transfer_order, 3);
	zest_map_insert_key(device->allocator, device->queue_names, ZEST_QUEUE_FAMILY_IGNORED, "Ignored");

	zest_bool queues_created = ZEST_TRUE;
	for (zest_uint family_index = 0; family_index != ZEST__DX_QUEUE_FAMILY_COUNT; ++family_index) {
		zest_queue_manager manager = device->queue_families[family_index];
		zest_vec_resize(device->allocator, manager->queues, manager->queue_count);
		for (zest_uint queue_index = 0; queue_index != manager->queue_count; ++queue_index) {
			zest_queue queue = &manager->queues[queue_index];
			*queue = ZEST__ZERO_INIT(zest_queue_t);
			queue->magic = zest_INIT_MAGIC(zest_struct_type_queue);
			queue->index = queue_index;
			queue->manager = manager;
			queue->family_index = family_index;
			queue->device = device;
			queue->backend = (zest_queue_backend)zest__dx_new_queue_backend(device, family_index);
			queues_created &= queue->backend != NULL;
			queues_created &= zest__initialise_timeline(device, &queue->timeline);
		}
	}

	if (!queues_created) {
		ZEST_APPEND_LOG(log_path, "Fatal Error: could not create the D3D12 queues, their command lists or their fences.");
		zest__dx_cleanup_queues(device);
		return ZEST_FALSE;
	}
	return ZEST_TRUE;
}

void zest__dx_add_queue_manager_list(zest_device device, zest_device_queue_type queue_type, const zest_uint *family_order, zest_uint order_count) {
	zest_queue_manager_list_t *manager_list = (zest_queue_manager_list_t *)ZEST__ALLOCATE(device->allocator, sizeof(zest_queue_manager_list_t));
	manager_list->managers = 0;
	for (zest_uint order_index = 0; order_index != order_count; ++order_index) {
		zest_vec_push(device->allocator, manager_list->managers, device->queue_families[family_order[order_index]]);
	}
	zest_vec_push(device->allocator, device->queue_managers, manager_list);
	device->queue_pool[queue_type] = manager_list;
}

zest_bool zest__dx_initialise_device(zest_device device) {
	zest_dx_device_backend_t *backend = ZEST_DX_BACKEND(device, device);
	const char *log_path = device->log_path.str;
	if (!zest__dx_prepare_debug_layer(device)) {
		return ZEST_FALSE;
	}
	zest_bool validation = ZEST__FLAGGED(device->setup_info.flags, zest_device_init_flag_enable_validation_layers);

	UINT factory_flags = validation ? DXGI_CREATE_FACTORY_DEBUG : 0;
	backend->last_result = CreateDXGIFactory2(factory_flags, ZEST_DX_IID(IDXGIFactory6), (void **)&backend->factory);
	if (FAILED(backend->last_result) && factory_flags) {
		ZEST_APPEND_LOG(log_path, "The DXGI debug factory isn't available (0x%08X), creating the factory without DXGI validation", (unsigned int)backend->last_result);
		backend->last_result = CreateDXGIFactory2(0, ZEST_DX_IID(IDXGIFactory6), (void **)&backend->factory);
	}
	if (FAILED(backend->last_result)) {
		backend->factory = NULL;
		zest__dx_log_hresult(device, backend->last_result, __FILE__, __LINE__);
		return ZEST_FALSE;
	}

	if (!zest__dx_select_adapter(device)) {
		return ZEST_FALSE;
	}
	if (validation) {
		zest__dx_setup_info_queue(device);
	}
	ZEST_DX_RETURN_FALSE_ON_FAIL(device, ZEST_DX_CALL(backend->device, QueryInterface, ZEST_DX_IID(ID3D12Device10), (void **)&backend->device10));
	if (!zest__dx_initialise_cpu_descriptor_heap(device, &backend->cpu_resource_heap, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV) ||
		!zest__dx_initialise_cpu_descriptor_heap(device, &backend->cpu_sampler_heap, D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER)) {
		return ZEST_FALSE;
	}
	if (!zest__dx_query_device_capabilities(device)) {
		return ZEST_FALSE;
	}
	if (!zest__dx_create_queues(device)) {
		return ZEST_FALSE;
	}
	zest__set_default_pool_sizes(device);
	return ZEST_TRUE;
}

void zest__dx_initialise_platform_callbacks(zest_platform_t *platform) {
	platform->new_device_backend = zest__dx_new_device_backend;
	platform->initialise_device = zest__dx_initialise_device;
	platform->query_device_capabilities = zest__dx_query_device_capabilities;
	platform->set_depth_format = zest__dx_set_depth_format;
	platform->wait_for_idle_device = zest__dx_wait_for_idle_device;
	platform->get_memory_budget = zest__dx_get_memory_budget;
	platform->cleanup_device_backend = zest__dx_cleanup_device_backend;
	platform->new_queue_backend = zest__dx_new_queue_backend;
	platform->cleanup_queue_backend = zest__dx_cleanup_queue_backend;
	platform->new_context_backend = zest__dx_new_context_backend;
	platform->initialise_context_backend = zest__dx_initialise_context_backend;
	platform->cleanup_context_backend = zest__dx_cleanup_context_backend;
	platform->new_context_queue_backend = zest__dx_new_context_queue_backend;
	platform->initialise_context_queue_backend = zest__dx_initialise_context_queue_backend;
	platform->cleanup_context_queue_backend = zest__dx_cleanup_context_queue_backend;
	platform->reset_queue_command_pool = zest__dx_reset_queue_command_pool;
	platform->flush_used_buffers = zest__dx_flush_used_buffers;
	platform->create_execution_timeline_backend = zest__dx_create_execution_timeline_backend;
	platform->cleanup_execution_timeline_backend = zest__dx_cleanup_execution_timeline_backend;
	platform->wait_for_timeline = zest__dx_wait_for_timeline;
	platform->imm_begin_command_buffer = zest__dx_imm_begin_command_buffer;
	platform->imm_end_command_buffer = zest__dx_imm_end_command_buffer;
	platform->imm_fill_buffer = zest__dx_imm_fill_buffer;
	platform->imm_update_buffer = zest__dx_imm_update_buffer;
	platform->imm_clear_color_image = zest__dx_imm_clear_color_image;
	platform->imm_clear_depth_stencil_image = zest__dx_imm_clear_depth_stencil_image;
	platform->imm_blit_image = zest__dx_imm_blit_image;
	platform->imm_resolve_image = zest__dx_imm_resolve_image;
	platform->imm_send_push_constants = zest__dx_imm_send_push_constants;
	platform->imm_bind_compute_pipeline = zest__dx_imm_bind_compute_pipeline;
	platform->imm_dispatch_compute = zest__dx_imm_dispatch_compute;
	platform->cmd_copy_buffer_one_time = zest__dx_cmd_copy_buffer_one_time;
	platform->copy_buffer_to_image = zest__dx_copy_buffer_to_image;
	platform->generate_mipmaps = zest__dx_generate_mipmaps;
	platform->create_buffer_allocator_backend = zest__dx_create_buffer_allocator_backend;
	platform->new_memory_pool_backend = zest__dx_new_memory_pool_backend;
	platform->add_buffer_memory_pool = zest__dx_add_buffer_memory_pool;
	platform->create_image_memory_pool = zest__dx_create_image_memory_pool;
	platform->map_memory = zest__dx_map_memory;
	platform->unmap_memory = zest__dx_unmap_memory;
	platform->create_device_memory = zest__dx_create_device_memory;
	platform->new_memory_backend = zest__dx_new_memory_backend;
	platform->cleanup_memory_backend = zest__dx_cleanup_memory_backend;
	platform->cleanup_memory_pool_backend = zest__dx_cleanup_memory_pool_backend;
	platform->cleanup_buffer_allocator_backend = zest__dx_cleanup_buffer_allocator_backend;
	platform->is_image_format_supported = zest__dx_is_image_format_supported;
	platform->new_image_backend = zest__dx_new_image_backend;
	platform->create_image = zest__dx_create_image;
	platform->cleanup_image_backend = zest__dx_cleanup_image_backend;
	platform->create_image_view = zest__dx_create_image_view;
	platform->cleanup_image_view_backend = zest__dx_cleanup_image_view_backend;
	platform->create_sampler = zest__dx_create_sampler;
	platform->new_sampler_backend = zest__dx_new_sampler_backend;
	platform->cleanup_sampler_backend = zest__dx_cleanup_sampler_backend;
	platform->transition_image = zest__dx_transition_image;
	platform->copy_buffer_regions_to_image = zest__dx_copy_buffer_regions_to_image;
	platform->get_image_raw_layout = zest__dx_get_image_raw_layout;
	platform->image_layout_is_valid_for_descriptor = zest__dx_image_layout_is_valid_for_descriptor;
	platform->copy_bitmap_to_image = zest__dx_copy_bitmap_to_image;
	platform->copy_image_to_bitmap = zest__dx_copy_image_to_bitmap;
	platform->new_set_layout_backend = zest__dx_new_set_layout_backend;
	platform->create_set_layout = zest__dx_create_set_layout;
	platform->cleanup_set_layout_backend = zest__dx_cleanup_set_layout_backend;
	platform->new_descriptor_pool_backend = zest__dx_new_descriptor_pool_backend;
	platform->create_set_pool = zest__dx_create_set_pool;
	platform->create_bindless_set = zest__dx_create_bindless_set;
	platform->update_bindless_image_descriptor = zest__dx_update_bindless_image_descriptor;
	platform->new_pipeline_layout_backend = zest__dx_new_pipeline_layout_backend;
	platform->build_pipeline_layout = zest__dx_build_pipeline_layout;
	platform->cleanup_pipeline_layout_backend = zest__dx_cleanup_pipeline_layout_backend;
}
// -- End Initialisation_functions

// -- Backend_setup_functions
void *zest__dx_new_device_backend(zest_device device) {
	zest__dx_device_release_count = 0;
	zest_dx_device_backend_t *backend = (zest_dx_device_backend_t *)ZEST__ALLOCATE(device->allocator, sizeof(zest_dx_device_backend_t));
	*backend = ZEST__ZERO_INIT(zest_dx_device_backend_t);
	return backend;
}

void *zest__dx_new_queue_backend(zest_device device, zest_uint family_index) {
	ZEST_ASSERT(family_index < ZEST__DX_QUEUE_FAMILY_COUNT, "The D3D12 backend only has three queue families.");
	zest_dx_device_backend_t *device_backend = ZEST_DX_BACKEND(device, device);
	zest_dx_queue_backend_t *backend = (zest_dx_queue_backend_t *)ZEST__ALLOCATE(device->allocator, sizeof(zest_dx_queue_backend_t));
	*backend = ZEST__ZERO_INIT(zest_dx_queue_backend_t);
	backend->type = zest__dx_queue_families[family_index].type;

	D3D12_COMMAND_QUEUE_DESC queue_description = ZEST__ZERO_INIT(D3D12_COMMAND_QUEUE_DESC);
	queue_description.Type = backend->type;
	queue_description.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
	queue_description.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	HRESULT result = ZEST_DX_CALL(device_backend->device, CreateCommandQueue, &queue_description, ZEST_DX_IID(ID3D12CommandQueue), (void **)&backend->queue);
	if (SUCCEEDED(result)) {
		result = ZEST_DX_CALL(device_backend->device, CreateCommandAllocator, backend->type, ZEST_DX_IID(ID3D12CommandAllocator), (void **)&backend->command_allocator);
	}
	if (SUCCEEDED(result)) {
		result = ZEST_DX_CALL(device_backend->device, CreateCommandList, 0, backend->type, backend->command_allocator, NULL, ZEST_DX_IID(ID3D12GraphicsCommandList7), (void **)&backend->command_list);
	}
	if (SUCCEEDED(result)) {
		result = ZEST_DX_CALL0(backend->command_list, Close);
	}
	device_backend->last_result = result;
	if (FAILED(result)) {
		zest__dx_log_hresult(device, result, __FILE__, __LINE__);
		ZEST_DX_RELEASE(backend->command_list);
		ZEST_DX_RELEASE(backend->command_allocator);
		ZEST_DX_RELEASE(backend->queue);
		ZEST__FREE(device->allocator, backend);
		return NULL;
	}
	return backend;
}

zest_bool zest__dx_create_execution_timeline_backend(zest_device device, zest_execution_timeline timeline) {
	zest_dx_device_backend_t *device_backend = ZEST_DX_BACKEND(device, device);
	zest_dx_execution_timeline_backend_t *backend = (zest_dx_execution_timeline_backend_t *)ZEST__ALLOCATE(device->allocator, sizeof(zest_dx_execution_timeline_backend_t));
	*backend = ZEST__ZERO_INIT(zest_dx_execution_timeline_backend_t);
	timeline->backend = (zest_execution_timeline_backend)backend;
	ZEST_DX_RETURN_FALSE_ON_FAIL(device, ZEST_DX_CALL(device_backend->device, CreateFence, 0, D3D12_FENCE_FLAG_NONE, ZEST_DX_IID(ID3D12Fence), (void **)&backend->fence));
	return ZEST_TRUE;
}

void *zest__dx_new_context_backend(zest_context context) {
	zest_dx_context_backend_t *backend = (zest_dx_context_backend_t *)ZEST__ALLOCATE(context->allocator, sizeof(zest_dx_context_backend_t));
	*backend = ZEST__ZERO_INIT(zest_dx_context_backend_t);
	return backend;
}

zest_bool zest__dx_initialise_context_backend(zest_context context) {
	return ZEST_TRUE;
}

void *zest__dx_new_context_queue_backend(zest_context context) {
	zest_dx_context_queue_backend_t *backend = (zest_dx_context_queue_backend_t *)ZEST__ALLOCATE(context->allocator, sizeof(zest_dx_context_queue_backend_t));
	*backend = ZEST__ZERO_INIT(zest_dx_context_queue_backend_t);
	return backend;
}

zest_bool zest__dx_initialise_context_queue_backend(zest_context context, zest_context_queue context_queue) {
	zest_device device = context->device;
	ID3D12Device *native_device = ZEST_DX_BACKEND(device, device)->device;
	zest_dx_context_backend_t *context_backend = ZEST_DX_BACKEND(context, context);
	zest_dx_context_queue_backend_t *backend = ZEST_DX_BACKEND(context_queue, context_queue);
	D3D12_COMMAND_LIST_TYPE type = zest__dx_queue_families[context_queue->queue_manager->family_index].type;
	zest_ForEachFrameInFlight(frame_in_flight) {
		context_queue->current_count[frame_in_flight] = 0;
		context_backend->last_result = ZEST_DX_CALL(native_device, CreateCommandAllocator, type, ZEST_DX_IID(ID3D12CommandAllocator), (void **)&backend->command_allocators[frame_in_flight]);
		if (FAILED(context_backend->last_result)) {
			//Allocators created so far are released by the context queue cleanup
			backend->command_allocators[frame_in_flight] = NULL;
			zest__dx_log_hresult(device, context_backend->last_result, __FILE__, __LINE__);
			return ZEST_FALSE;
		}
	}
	ZEST_APPEND_LOG(device->log_path.str, "Create context queue command allocators");
	return ZEST_TRUE;
}
// -- End Backend_setup_functions

// -- Backend_cleanup_functions
void zest__dx_cleanup_device_backend(zest_device device) {
	zest_dx_device_backend_t *backend = ZEST_DX_BACKEND(device, device);
	if (!backend) return;
	zest__dx_cleanup_cpu_descriptor_heap(device, &backend->cpu_resource_heap);
	zest__dx_cleanup_cpu_descriptor_heap(device, &backend->cpu_sampler_heap);
	if (backend->info_queue) {
		ZEST_DX_CALL(backend->info_queue, UnregisterMessageCallback, backend->info_queue_cookie);
	}
	ZEST_DX_RELEASE(backend->info_queue);
	ZEST_DX_RELEASE(backend->device10);
	ZEST_DX_RELEASE(backend->adapter);
	ZEST_DX_RELEASE(backend->factory);
	zest__dx_release_native_device(device, backend->device);
	backend->device = NULL;
	if (backend->counted_as_live) {
		InterlockedDecrement(&zest__dx_live_device_count);
	}
	ZEST__FREE(device->allocator, backend);
	device->backend = NULL;
}

void zest__dx_release_native_device(zest_device device, ID3D12Device *native_device) {
	if (!native_device) return;
	if (ZEST__FLAGGED(device->setup_info.flags, zest_device_init_flag_enable_validation_layers)) {
		ZEST_DX_CALL0(native_device, AddRef);
		ULONG reference_count = ZEST_DX_CALL0(native_device, Release);
		if (reference_count > 1) {
			ID3D12DebugDevice *debug_device = NULL;
			if (SUCCEEDED(ZEST_DX_CALL(native_device, QueryInterface, ZEST_DX_IID(ID3D12DebugDevice), (void **)&debug_device))) {
				ZEST_DX_CALL(debug_device, ReportLiveDeviceObjects, D3D12_RLDO_DETAIL | D3D12_RLDO_IGNORE_INTERNAL);
				ZEST_DX_RELEASE(debug_device);
			}
			ZEST_ALERT("The D3D12 device still holds %u references besides Zest's own at cleanup. The live objects were reported to the debugger output.", (unsigned int)(reference_count - 1));
			ZEST_APPEND_LOG(device->log_path.str, "The D3D12 device still holds %u references besides Zest's own at cleanup.", (unsigned int)(reference_count - 1));
		}
	}
	zest_uint release_count = (zest_uint)ZEST_DX_CALL0(native_device, Release);
	zest__dx_device_release_count = ZEST__MAX(zest__dx_device_release_count, release_count);
}

//The worst final release of an ID3D12Device since the device backend was created
zest_uint zest__dx_last_device_release_count(void) {
	return zest__dx_device_release_count;
}

void zest__dx_cleanup_queue_backend(zest_device device, zest_queue queue) {
	zest_dx_queue_backend_t *backend = ZEST_DX_BACKEND(queue, queue);
	if (!backend) return;
	ZEST_DX_RELEASE(backend->command_list);
	ZEST_DX_RELEASE(backend->command_allocator);
	ZEST_DX_RELEASE(backend->queue);
	zest_vec_free(device->allocator, backend->pending_staging_buffers);
	ZEST__FREE(device->allocator, backend);
	queue->backend = NULL;
}

void zest__dx_cleanup_execution_timeline_backend(zest_execution_timeline timeline) {
	zest_dx_execution_timeline_backend_t *backend = ZEST_DX_BACKEND(execution_timeline, timeline);
	if (!backend) return;
	ZEST_DX_RELEASE(backend->fence);
	ZEST__FREE(timeline->device->allocator, backend);
	timeline->backend = NULL;
}

//Releases every queue object so a failed initialisation leaves no child holding the device
void zest__dx_cleanup_queues(zest_device device) {
	zest_vec_foreach(family_index, device->queue_families) {
		zest_queue_manager manager = device->queue_families[family_index];
		if (!manager) continue;
		zest_vec_foreach(queue_index, manager->queues) {
			zest_queue queue = &manager->queues[queue_index];
			zest__dx_cleanup_execution_timeline_backend(&queue->timeline);
			zest__dx_cleanup_queue_backend(device, queue);
		}
		zest_vec_free(device->allocator, manager->queues);
		ZEST__FREE(device->allocator, manager);
	}
	zest_vec_foreach(list_index, device->queue_managers) {
		zest_queue_manager_list_t *manager_list = device->queue_managers[list_index];
		zest_vec_free(device->allocator, manager_list->managers);
		ZEST__FREE(device->allocator, manager_list);
	}
	zest_vec_free(device->allocator, device->queue_families);
	zest_vec_free(device->allocator, device->queue_managers);
	memset(device->queue_pool, 0, sizeof(device->queue_pool));
}

void zest__dx_cleanup_context_queue_backend(zest_context context, zest_context_queue context_queue) {
	zest_dx_context_queue_backend_t *backend = ZEST_DX_BACKEND(context_queue, context_queue);
	if (!backend) return;
	zest_vec_foreach(list_index, backend->command_lists) {
		ZEST_DX_RELEASE(backend->command_lists[list_index]);
	}
	zest_vec_free(context->allocator, backend->command_lists);
	zest_ForEachFrameInFlight(frame_in_flight) {
		ZEST_DX_RELEASE(backend->command_allocators[frame_in_flight]);
	}
	ZEST__FREE(context->allocator, backend);
	context_queue->backend = NULL;
}

void zest__dx_cleanup_context_backend(zest_context context) {
	if (!context->backend) return;
	ZEST__FREE(context->allocator, context->backend);
	context->backend = NULL;
}

//Buffers are sub-allocations of their memory pools, so the context holds no buffer objects of its own to destroy
void zest__dx_flush_used_buffers(zest_context context, zest_uint frame_in_flight) {
}
// -- End Backend_cleanup_functions

// -- Fences
DWORD zest__dx_timeout_milliseconds(zest_microsecs timeout) {
	zest_microsecs milliseconds = timeout / 1000 + (timeout % 1000 != 0);
	return milliseconds < INFINITE ? (DWORD)milliseconds : INFINITE - 1;
}

zest_semaphore_status zest__dx_wait_for_timeline(zest_execution_timeline timeline, zest_microsecs timeout) {
	zest_dx_execution_timeline_backend_t *backend = ZEST_DX_BACKEND(execution_timeline, timeline);
	zest_u64 target_value = timeline->current_value;
	zest_u64 completed_value = ZEST_DX_CALL0(backend->fence, GetCompletedValue);
	//A removed device reports every fence as UINT64_MAX
	if (completed_value == UINT64_MAX) return zest_semaphore_status_error;
	if (completed_value >= target_value) return zest_semaphore_status_success;
	if (timeout == 0) return zest_semaphore_status_timeout;
	if (!zest__dx_wait_event) {
		zest__dx_wait_event = CreateEventW(NULL, FALSE, FALSE, NULL);
		if (!zest__dx_wait_event) {
			zest__dx_log_hresult(timeline->device, HRESULT_FROM_WIN32(GetLastError()), __FILE__, __LINE__);
			return zest_semaphore_status_error;
		}
	}

	zest_microsecs wait_start = zest_Microsecs();
	zest_microsecs remaining = timeout;
	for (;;) {
		HRESULT result = ZEST_DX_CALL(backend->fence, SetEventOnCompletion, target_value, zest__dx_wait_event);
		if (FAILED(result)) {
			zest__dx_log_hresult(timeline->device, result, __FILE__, __LINE__);
			return zest_semaphore_status_error;
		}
		DWORD wait_result = WaitForSingleObject(zest__dx_wait_event, zest__dx_timeout_milliseconds(remaining));
		if (wait_result == WAIT_FAILED) return zest_semaphore_status_error;
		completed_value = ZEST_DX_CALL0(backend->fence, GetCompletedValue);
		if (completed_value == UINT64_MAX) return zest_semaphore_status_error;
		if (completed_value >= target_value) return zest_semaphore_status_success;
		if (wait_result == WAIT_TIMEOUT) return zest_semaphore_status_timeout;
		//A registration left by an earlier timed-out wait on this thread can wake it early
		zest_microsecs elapsed = zest_Microsecs() - wait_start;
		if (elapsed >= timeout) return zest_semaphore_status_timeout;
		remaining = timeout - elapsed;
	}
}
// -- End Fences

// -- Command_pools
//Staging buffers are freed before the queue is released, as the next holder pushes to the same list
void zest__dx_release_immediate_queue(zest_queue queue, zest_bool gpu_finished) {
	zest_dx_queue_backend_t *backend = ZEST_DX_BACKEND(queue, queue);
	zest_vec_foreach(buffer_index, backend->pending_staging_buffers) {
		if (gpu_finished) {
			zest_FreeBufferNow(backend->pending_staging_buffers[buffer_index]);
		} else {
			zest_FreeBuffer(backend->pending_staging_buffers[buffer_index]);
		}
	}
	zest_vec_clear(backend->pending_staging_buffers);
	queue->last_bound_compute = NULL;
	zest__release_queue(queue);
}

zest_queue zest__dx_imm_begin_command_buffer(zest_device device, zest_device_queue_type target_queue) {
	zest_queue queue = zest__acquire_immediate_queue(device, target_queue);
	if (!queue) {
		return NULL;
	}

	zest_dx_device_backend_t *device_backend = ZEST_DX_BACKEND(device, device);
	zest_dx_queue_backend_t *queue_backend = ZEST_DX_BACKEND(queue, queue);
	//The queue runs in order, so a signal queued now tells the wait below when an unfenced list has finished
	if (queue_backend->unfenced_submission) {
		zest_dx_execution_timeline_backend_t *timeline_backend = ZEST_DX_BACKEND(execution_timeline, &queue->timeline);
		queue->timeline.current_value++;
		device_backend->last_result = ZEST_DX_CALL(queue_backend->queue, Signal, timeline_backend->fence, queue->timeline.current_value);
		if (FAILED(device_backend->last_result)) {
			queue->timeline.current_value--;
			zest__dx_log_hresult(device, device_backend->last_result, __FILE__, __LINE__);
			ZEST_REPORT(device, zest_report_submission_failure, "The last immediate submission on this queue couldn't be fenced, so its command allocator can't be reset for a new immediate command buffer.");
			zest__dx_release_immediate_queue(queue, ZEST_FALSE);
			return NULL;
		}
		queue_backend->unfenced_submission = ZEST_FALSE;
	}
	//An end that timed out or failed can leave the last submission still using the allocator
	if (zest_WaitForSignal(&queue->timeline, ZEST__DX_IMMEDIATE_TIMEOUT) != zest_semaphore_status_success) {
		ZEST_REPORT(device, zest_report_submission_failure, "The previous immediate submission on this queue didn't finish in time or the device was lost, so its command allocator can't be reset for a new immediate command buffer.");
		zest__dx_release_immediate_queue(queue, ZEST_FALSE);
		return NULL;
	}
	device_backend->last_result = ZEST_DX_CALL0(queue_backend->command_allocator, Reset);
	if (SUCCEEDED(device_backend->last_result) && queue_backend->command_list) {
		device_backend->last_result = ZEST_DX_CALL(queue_backend->command_list, Reset, queue_backend->command_allocator, NULL);
	} else if (SUCCEEDED(device_backend->last_result)) {
		//A list that failed to close was released by the end, so a new one is created already recording
		device_backend->last_result = ZEST_DX_CALL(device_backend->device, CreateCommandList, 0, queue_backend->type, queue_backend->command_allocator, NULL, ZEST_DX_IID(ID3D12GraphicsCommandList7), (void **)&queue_backend->command_list);
		if (FAILED(device_backend->last_result)) {
			queue_backend->command_list = NULL;
		}
	}
	if (FAILED(device_backend->last_result)) {
		zest__dx_log_hresult(device, device_backend->last_result, __FILE__, __LINE__);
		zest__dx_release_immediate_queue(queue, ZEST_TRUE);
		return NULL;
	}
	//The acquired queue can be more capable than the one requested, so its own type decides
	if (queue_backend->type != D3D12_COMMAND_LIST_TYPE_COPY && device->bindless_set) {
		zest__dx_bind_bindless_heaps(device, queue_backend->command_list);
	}
	return queue;
}

zest_bool zest__dx_imm_end_command_buffer(zest_queue queue) {
	if (!queue) return ZEST_FALSE;
	zest_device device = queue->device;
	zest_dx_device_backend_t *device_backend = ZEST_DX_BACKEND(device, device);
	zest_dx_queue_backend_t *queue_backend = ZEST_DX_BACKEND(queue, queue);
	zest_dx_execution_timeline_backend_t *timeline_backend = ZEST_DX_BACKEND(execution_timeline, &queue->timeline);
	device_backend->last_result = ZEST_DX_CALL0(queue_backend->command_list, Close);
	if (FAILED(device_backend->last_result)) {
		zest__dx_log_hresult(device, device_backend->last_result, __FILE__, __LINE__);
		//A list that failed to close can't be reset and keeps its allocator from resetting, so the next begin replaces it
		ZEST_DX_RELEASE(queue_backend->command_list);
		zest__dx_release_immediate_queue(queue, ZEST_TRUE);
		return ZEST_FALSE;
	}

	ID3D12CommandList *command_lists[1] = { (ID3D12CommandList *)queue_backend->command_list };
	ZEST_DX_CALL(queue_backend->queue, ExecuteCommandLists, 1, command_lists);
	queue->timeline.current_value++;
	device_backend->last_result = zest__dx_forced_signal_failure ? E_OUTOFMEMORY : ZEST_DX_CALL(queue_backend->queue, Signal, timeline_backend->fence, queue->timeline.current_value);
	if (FAILED(device_backend->last_result)) {
		//The signal was never queued, so roll the value back to one a later wait can actually reach
		queue->timeline.current_value--;
		//The list is already executing, so the next begin fences it before resetting the allocator
		queue_backend->unfenced_submission = ZEST_TRUE;
		zest__dx_log_hresult(device, device_backend->last_result, __FILE__, __LINE__);
		zest__dx_release_immediate_queue(queue, ZEST_FALSE);
		return ZEST_FALSE;
	}

	zest_semaphore_status semaphore_status = zest_WaitForSignal(&queue->timeline, ZEST__DX_IMMEDIATE_TIMEOUT);
	zest__dx_release_immediate_queue(queue, semaphore_status == zest_semaphore_status_success);
	return semaphore_status == zest_semaphore_status_success;
}

zest_bool zest__dx_report_unavailable(zest_queue queue, const char *message) {
	ZEST_ASSERT_OR_VALIDATE(ZEST_FALSE, queue->device, message, ZEST_FALSE);
	return ZEST_FALSE;
}

void zest__dx_imm_fill_buffer(zest_queue queue, zest_buffer buffer, zest_uint value) { zest__dx_report_unavailable(queue, "zest_imm_FillBuffer is not available on the D3D12 backend yet."); }
void zest__dx_imm_update_buffer(zest_queue queue, zest_buffer buffer, void *data, zest_size intended_size) { zest__dx_report_unavailable(queue, "zest_imm_UpdateBuffer is not available on the D3D12 backend yet."); }
zest_bool zest__dx_imm_clear_color_image(zest_queue queue, zest_image image, zest_clear_value_t clear_value) { return zest__dx_report_unavailable(queue, "zest_imm_ClearColorImage is not available on the D3D12 backend yet."); }
zest_bool zest__dx_imm_clear_depth_stencil_image(zest_queue queue, zest_image image, float depth, zest_uint stencil) { return zest__dx_report_unavailable(queue, "zest_imm_ClearDepthStencilImage is not available on the D3D12 backend yet."); }
zest_bool zest__dx_imm_blit_image(zest_queue queue, zest_image src_image, zest_image dst_image, int src_x, int src_y, int src_width, int src_height, int dst_x, int dst_y, int dst_width, int dst_height, zest_filter_type filter) { return zest__dx_report_unavailable(queue, "zest_imm_BlitImage is not available on the D3D12 backend yet."); }
zest_bool zest__dx_imm_resolve_image(zest_queue queue, zest_image src_image, zest_image dst_image) { return zest__dx_report_unavailable(queue, "zest_imm_ResolveImage is not available on the D3D12 backend yet."); }
void zest__dx_imm_send_push_constants(zest_queue queue, void *data, zest_uint size) { zest__dx_report_unavailable(queue, "zest_imm_SendPushConstants is not available on the D3D12 backend yet."); }
void zest__dx_imm_bind_compute_pipeline(zest_queue queue, zest_compute compute) { zest__dx_report_unavailable(queue, "zest_imm_BindComputePipeline is not available on the D3D12 backend yet."); }
zest_bool zest__dx_imm_dispatch_compute(zest_queue queue, zest_uint group_count_x, zest_uint group_count_y, zest_uint group_count_z) { return zest__dx_report_unavailable(queue, "zest_imm_DispatchCompute is not available on the D3D12 backend yet."); }
zest_bool zest__dx_cmd_copy_buffer_one_time(zest_queue queue, zest_buffer src_buffer, zest_buffer dst_buffer, zest_size size, zest_size src_offset, zest_size dst_offset) { return zest__dx_report_unavailable(queue, "zest_imm_CopyBuffer and zest_imm_CopyBufferRegion are not available on the D3D12 backend yet."); }
zest_bool zest__dx_copy_buffer_to_image(zest_queue queue, zest_buffer buffer, zest_size src_offset, zest_image image, zest_uint width, zest_uint height) { return zest__dx_report_unavailable(queue, "zest_imm_CopyBufferToImage is not available on the D3D12 backend yet."); }
zest_bool zest__dx_generate_mipmaps(zest_queue queue, zest_image image) { return zest__dx_report_unavailable(queue, "zest_imm_GenerateMipMaps is not available on the D3D12 backend yet."); }

//A D3D12 allocator keeps its memory across every reset, so release_resources has nothing to map to
void zest__dx_reset_queue_command_pool(zest_context context, zest_context_queue queue, zest_bool release_resources) {
	ID3D12CommandAllocator *command_allocator = ZEST_DX_BACKEND(context_queue, queue)->command_allocators[context->current_fif];
	if (!command_allocator) return;
	zest_dx_context_backend_t *context_backend = ZEST_DX_BACKEND(context, context);
	context_backend->last_result = ZEST_DX_CALL0(command_allocator, Reset);
	if (FAILED(context_backend->last_result)) {
		zest__dx_log_hresult(context->device, context_backend->last_result, __FILE__, __LINE__);
	}
}
// -- End Command_pools

// -- Buffer_and_memory
D3D12_HEAP_PROPERTIES zest__dx_heap_properties(zest_memory_property_flags property_flags, zest_bool gpu_upload_supported) {
	D3D12_HEAP_PROPERTIES properties = ZEST__ZERO_INIT(D3D12_HEAP_PROPERTIES);
	properties.Type = D3D12_HEAP_TYPE_DEFAULT;
	if (ZEST__NOT_FLAGGED(property_flags, zest_memory_property_host_visible_bit)) {
		return properties;
	}
	zest_bool host_cached = ZEST__FLAGGED(property_flags, zest_memory_property_host_cached_bit);
	if (!host_cached && gpu_upload_supported && ZEST__FLAGGED(property_flags, zest_memory_property_device_local_bit)) {
		properties.Type = D3D12_HEAP_TYPE_GPU_UPLOAD;
		return properties;
	}
	//CUSTOM because the UPLOAD and READBACK heap types forbid GPU writes
	properties.Type = D3D12_HEAP_TYPE_CUSTOM;
	properties.CPUPageProperty = host_cached ? D3D12_CPU_PAGE_PROPERTY_WRITE_BACK : D3D12_CPU_PAGE_PROPERTY_WRITE_COMBINE;
	properties.MemoryPoolPreference = D3D12_MEMORY_POOL_L0;
	return properties;
}

D3D12_HEAP_FLAGS zest__dx_image_heap_flags(zest_uint category, D3D12_RESOURCE_HEAP_TIER tier) {
	zest_bool tier_1 = tier < D3D12_RESOURCE_HEAP_TIER_2;
	if (category == ZEST_DX_HEAP_CATEGORY_TARGETS || (category == ZEST_DX_HEAP_CATEGORY_MSAA && tier_1)) {
		return D3D12_HEAP_FLAG_ALLOW_ONLY_RT_DS_TEXTURES;
	}
	return tier_1 ? D3D12_HEAP_FLAG_ALLOW_ONLY_NON_RT_DS_TEXTURES : D3D12_HEAP_FLAG_ALLOW_ALL_BUFFERS_AND_TEXTURES;
}

zest_bool zest__dx_create_image_heap(zest_device device, zest_size size_in_bytes, zest_uint category, ID3D12Heap **heap_out) {
	if (category < ZEST_DX_HEAP_CATEGORY_TEXTURES || category > ZEST_DX_HEAP_CATEGORY_MSAA) {
		ZEST_ASSERT_OR_VALIDATE(ZEST_FALSE, device, "Image memory needs a heap category in backend_memory_bits.", ZEST_FALSE);
		return ZEST_FALSE;
	}
	zest_dx_device_backend_t *device_backend = ZEST_DX_BACKEND(device, device);
	D3D12_HEAP_DESC description = ZEST__ZERO_INIT(D3D12_HEAP_DESC);
	description.SizeInBytes = size_in_bytes;
	description.Properties.Type = D3D12_HEAP_TYPE_DEFAULT;
	description.Alignment = category == ZEST_DX_HEAP_CATEGORY_MSAA ? D3D12_DEFAULT_MSAA_RESOURCE_PLACEMENT_ALIGNMENT : D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT;
	description.Flags = zest__dx_image_heap_flags(category, device_backend->support.resource_heap_tier);
	ID3D12Heap *heap = NULL;
	ZEST_DX_RETURN_FALSE_ON_FAIL(device, ZEST_DX_CALL(device_backend->device, CreateHeap, &description, ZEST_DX_IID(ID3D12Heap), (void **)&heap));
	zest__atomic_fetch_add(&device->memory_allocation_count, 1);
	*heap_out = heap;
	return ZEST_TRUE;
}

void *zest__dx_create_buffer_allocator_backend(zest_device device, zest_context context, zest_size size, zest_buffer_info_t *buffer_info, zest_uint backend_memory_bits) {
	zloc_allocator *allocator = context ? context->allocator : device->allocator;
	zest_dx_buffer_allocator_backend_t *backend = (zest_dx_buffer_allocator_backend_t *)ZEST__ALLOCATE(allocator, sizeof(zest_dx_buffer_allocator_backend_t));
	*backend = ZEST__ZERO_INIT(zest_dx_buffer_allocator_backend_t);
	backend->heap_properties = zest__dx_heap_properties(buffer_info->property_flags, ZEST_DX_BACKEND(device, device)->support.gpu_upload_heap);
	backend->heap_category = backend_memory_bits;
	return backend;
}

void *zest__dx_new_memory_pool_backend(zest_buffer_allocator buffer_allocator) {
	zloc_allocator *allocator = buffer_allocator->context ? buffer_allocator->context->allocator : buffer_allocator->device->allocator;
	zest_dx_device_memory_pool_backend_t *backend = (zest_dx_device_memory_pool_backend_t *)ZEST__ALLOCATE(allocator, sizeof(zest_dx_device_memory_pool_backend_t));
	*backend = ZEST__ZERO_INIT(zest_dx_device_memory_pool_backend_t);
	return backend;
}

void *zest__dx_new_memory_backend(zest_device device) {
	zest_dx_device_memory_backend_t *backend = (zest_dx_device_memory_backend_t *)ZEST__ALLOCATE(device->allocator, sizeof(zest_dx_device_memory_backend_t));
	*backend = ZEST__ZERO_INIT(zest_dx_device_memory_backend_t);
	return backend;
}

zest_bool zest__dx_add_buffer_memory_pool(zest_device device, zest_context context, zest_size size, zest_buffer_allocator buffer_allocator, zest_device_memory_pool memory_pool) {
	zest_dx_device_backend_t *device_backend = ZEST_DX_BACKEND(device, device);
	zest_dx_buffer_allocator_backend_t *allocator_backend = ZEST_DX_BACKEND(buffer_allocator, buffer_allocator);
	D3D12_RESOURCE_DESC1 description = ZEST__ZERO_INIT(D3D12_RESOURCE_DESC1);
	description.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	description.Alignment = D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT;
	description.Width = size;
	description.Height = 1;
	description.DepthOrArraySize = 1;
	description.MipLevels = 1;
	description.Format = DXGI_FORMAT_UNKNOWN;
	description.SampleDesc.Count = 1;
	description.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	description.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
	ZEST_APPEND_LOG(device->log_path.str, "Allocating buffer memory pool, size: %llu, heap type: %i, CPU page property: %i, memory pool: %i", (zest_ull)size,
		(int)allocator_backend->heap_properties.Type, (int)allocator_backend->heap_properties.CPUPageProperty, (int)allocator_backend->heap_properties.MemoryPoolPreference);
	ID3D12Resource *buffer = NULL;
	ZEST_DX_RETURN_FALSE_ON_FAIL(device, ZEST_DX_CALL(device_backend->device10, CreateCommittedResource3, &allocator_backend->heap_properties, D3D12_HEAP_FLAG_NONE, &description,
		D3D12_BARRIER_LAYOUT_UNDEFINED, NULL, NULL, 0, NULL, ZEST_DX_IID(ID3D12Resource), (void **)&buffer));
	zest__atomic_fetch_add(&device->memory_allocation_count, 1);
	ZEST_DX_BACKEND(device_memory_pool, memory_pool)->buffer = buffer;
	memory_pool->size = size;
	memory_pool->alignment = D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT;
	return ZEST_TRUE;
}

zest_bool zest__dx_create_image_memory_pool(zest_device device, zest_context context, zest_size size_in_bytes, zest_buffer_allocator buffer_allocator, zest_device_memory_pool pool) {
	zest_uint category = ZEST_DX_BACKEND(buffer_allocator, buffer_allocator)->heap_category;
	pool->size = size_in_bytes;
	pool->alignment = buffer_allocator->buffer_info.alignment;
	pool->minimum_allocation_size = ZEST__MAX(pool->alignment, pool->minimum_allocation_size);
	ZEST_APPEND_LOG(device->log_path.str, "Allocating image memory pool, size: %llu, heap category: %u, alignment: %llu", (zest_ull)size_in_bytes, category, (zest_ull)pool->alignment);
	return zest__dx_create_image_heap(device, size_in_bytes, category, &ZEST_DX_BACKEND(device_memory_pool, pool)->heap);
}

zest_bool zest__dx_create_device_memory(zest_device device, zest_size size_in_bytes, zest_buffer_info_t *buffer_info, zest_uint backend_memory_bits, zest_device_memory memory) {
	memory->size = size_in_bytes;
	return zest__dx_create_image_heap(device, size_in_bytes, backend_memory_bits, &ZEST_DX_BACKEND(device_memory, memory)->heap);
}

zest_bool zest__dx_map_memory(zest_device_memory_pool memory_allocation, zest_size size, zest_size offset) {
	zest_device device = memory_allocation->device;
	ID3D12Resource *buffer = ZEST_DX_BACKEND(device_memory_pool, memory_allocation)->buffer;
	//Image pools are heaps, which have no CPU mapping
	if (!buffer) return ZEST_FALSE;
	void *data = NULL;
	ZEST_DX_RETURN_FALSE_ON_FAIL(device, ZEST_DX_CALL(buffer, Map, 0, NULL, &data));
	memory_allocation->mapped = (char *)data + offset;
	return ZEST_TRUE;
}

void zest__dx_unmap_memory(zest_device_memory_pool memory_allocation) {
	ID3D12Resource *buffer = ZEST_DX_BACKEND(device_memory_pool, memory_allocation)->buffer;
	if (buffer) {
		ZEST_DX_CALL(buffer, Unmap, 0, NULL);
	}
}

void zest__dx_cleanup_memory_pool_backend(zest_device_memory_pool memory_allocation) {
	zest_device device = memory_allocation->device;
	zest_buffer_allocator buffer_allocator = memory_allocation->allocator;
	zloc_allocator *allocator = buffer_allocator->context ? buffer_allocator->context->allocator : device->allocator;
	zest_dx_device_memory_pool_backend_t *backend = ZEST_DX_BACKEND(device_memory_pool, memory_allocation);
	if (!backend) return;
	if (backend->buffer) {
		ZEST_DX_RELEASE(backend->buffer);
		zest__atomic_fetch_add(&device->memory_allocation_count, -1);
	}
	if (backend->heap) {
		ZEST_DX_RELEASE(backend->heap);
		zest__atomic_fetch_add(&device->memory_allocation_count, -1);
	}
	ZEST__FREE(allocator, backend);
	memory_allocation->backend = NULL;
}

void zest__dx_cleanup_memory_backend(zest_device_memory memory) {
	zest_device device = memory->device;
	zest_dx_device_memory_backend_t *backend = ZEST_DX_BACKEND(device_memory, memory);
	if (!backend) return;
	if (backend->heap) {
		ZEST_DX_RELEASE(backend->heap);
		zest__atomic_fetch_add(&device->memory_allocation_count, -1);
	}
	ZEST__FREE(device->allocator, backend);
	memory->backend = NULL;
}

void zest__dx_cleanup_buffer_allocator_backend(zest_buffer_allocator buffer_allocator) {
	zloc_allocator *allocator = buffer_allocator->context ? buffer_allocator->context->allocator : buffer_allocator->device->allocator;
	ZEST__FREE(allocator, buffer_allocator->backend);
	buffer_allocator->backend = NULL;
}
// -- End Buffer_and_memory

// -- Descriptor_heaps
zest_bool zest__dx_initialise_cpu_descriptor_heap(zest_device device, zest_dx_cpu_descriptor_heap_t *heap, D3D12_DESCRIPTOR_HEAP_TYPE type) {
	heap->type = type;
	heap->increment = ZEST_DX_CALL(ZEST_DX_BACKEND(device, device)->device, GetDescriptorHandleIncrementSize, type);
	return zest__dx_add_cpu_descriptor_block(device, heap);
}

zest_bool zest__dx_add_cpu_descriptor_block(zest_device device, zest_dx_cpu_descriptor_heap_t *heap) {
	D3D12_DESCRIPTOR_HEAP_DESC description = ZEST__ZERO_INIT(D3D12_DESCRIPTOR_HEAP_DESC);
	description.Type = heap->type;
	description.NumDescriptors = ZEST_DX_CPU_DESCRIPTOR_BLOCK_SIZE;
	description.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	ID3D12DescriptorHeap *block = NULL;
	ZEST_DX_RETURN_FALSE_ON_FAIL(device, ZEST_DX_CALL(ZEST_DX_BACKEND(device, device)->device, CreateDescriptorHeap, &description, ZEST_DX_IID(ID3D12DescriptorHeap), (void **)&block));
	zest_vec_push(device->allocator, heap->blocks, block);
	heap->next_in_block = 0;
	return ZEST_TRUE;
}

//Returns a zero handle when a new block can't be created
D3D12_CPU_DESCRIPTOR_HANDLE zest__dx_allocate_cpu_descriptor(zest_device device, zest_dx_cpu_descriptor_heap_t *heap) {
	if (!zest_vec_empty(heap->free_handles)) {
		return zest_vec_pop(heap->free_handles);
	}
	D3D12_CPU_DESCRIPTOR_HANDLE handle = ZEST__ZERO_INIT(D3D12_CPU_DESCRIPTOR_HANDLE);
	if (zest_vec_empty(heap->blocks) || heap->next_in_block == ZEST_DX_CPU_DESCRIPTOR_BLOCK_SIZE) {
		if (!zest__dx_add_cpu_descriptor_block(device, heap)) {
			return handle;
		}
	}
	handle = zest__dx_cpu_heap_start(zest_vec_back(heap->blocks));
	handle.ptr += (SIZE_T)heap->next_in_block * heap->increment;
	heap->next_in_block++;
	return handle;
}

void zest__dx_free_cpu_descriptor(zest_device device, zest_dx_cpu_descriptor_heap_t *heap, D3D12_CPU_DESCRIPTOR_HANDLE handle) {
	if (!handle.ptr) return;
	zest_vec_push(device->allocator, heap->free_handles, handle);
}

void zest__dx_cleanup_cpu_descriptor_heap(zest_device device, zest_dx_cpu_descriptor_heap_t *heap) {
	zest_vec_foreach(block_index, heap->blocks) {
		ZEST_DX_RELEASE(heap->blocks[block_index]);
	}
	zest_vec_free(device->allocator, heap->blocks);
	zest_vec_free(device->allocator, heap->free_handles);
	heap->next_in_block = 0;
}

void zest__dx_compute_bindless_regions(const zest_uint binding_counts[zest_max_global_binding_number], zest_dx_bindless_regions_t *regions) {
	zest_uint offset = 0;
	regions->resource_offsets[zest_sampler_binding] = 0;
	for (zest_uint binding_number = zest_texture_2d_binding; binding_number != zest_max_global_binding_number; ++binding_number) {
		regions->resource_offsets[binding_number] = offset;
		offset += binding_counts[binding_number];
		//One storage buffer index serves both views, so its UAVs sit in a parallel region straight after the SRVs
		if (binding_number == zest_storage_buffer_binding) {
			regions->storage_buffer_uav_offset = offset;
			offset += binding_counts[binding_number];
		}
	}
	regions->resource_descriptor_count = offset;
	regions->sampler_descriptor_count = binding_counts[zest_sampler_binding];
}

void zest__dx_bind_bindless_heaps(zest_device device, ID3D12GraphicsCommandList7 *command_list) {
	zest_dx_descriptor_pool_backend_t *pool_backend = ZEST_DX_BACKEND(descriptor_set, device->bindless_set)->pool;
	ID3D12DescriptorHeap *heaps[2] = { pool_backend->resource_heap, pool_backend->sampler_heap };
	ZEST_DX_CALL(command_list, SetDescriptorHeaps, 2, heaps);
}

void *zest__dx_new_set_layout_backend(zloc_allocator *allocator) {
	zest_dx_set_layout_backend_t *backend = (zest_dx_set_layout_backend_t *)ZEST__ALLOCATE(allocator, sizeof(zest_dx_set_layout_backend_t));
	*backend = ZEST__ZERO_INIT(zest_dx_set_layout_backend_t);
	return backend;
}

zest_bool zest__dx_create_set_layout(zest_device device, zest_context context, zest_set_layout_builder_t *builder, zest_set_layout layout, zest_bool is_bindless) {
	ZEST_ASSERT(is_bindless, "Custom descriptor set layouts are not supported by the D3D12 backend.");
	if (!is_bindless) {
		return ZEST_FALSE;
	}
	zest_dx_set_layout_backend_t *backend = ZEST_DX_BACKEND(set_layout, layout);
	zest_vec_foreach(binding_index, builder->bindings) {
		const zest_descriptor_binding_desc_t *description = &builder->bindings[binding_index];
		if (description->binding >= zest_max_global_binding_number) {
			ZEST_ASSERT(0, "A D3D12 bindless set layout can only use the global binding numbers.");
			return ZEST_FALSE;
		}
		backend->binding_counts[description->binding] = description->count;
	}
	return ZEST_TRUE;
}

void *zest__dx_new_descriptor_pool_backend(zloc_allocator *allocator) {
	zest_dx_descriptor_pool_backend_t *backend = (zest_dx_descriptor_pool_backend_t *)ZEST__ALLOCATE(allocator, sizeof(zest_dx_descriptor_pool_backend_t));
	*backend = ZEST__ZERO_INIT(zest_dx_descriptor_pool_backend_t);
	return backend;
}

zest_bool zest__dx_create_set_pool(zest_device device, zest_context context, zest_descriptor_pool pool, zest_set_layout layout, zest_uint max_set_count, zest_bool bindless) {
	ZEST_ASSERT(bindless, "Custom descriptor set layouts are not supported by the D3D12 backend.");
	zloc_allocator *allocator = context ? context->allocator : device->allocator;
	zest_dx_device_backend_t *device_backend = ZEST_DX_BACKEND(device, device);
	zest_dx_descriptor_pool_backend_t *backend = ZEST_DX_BACKEND(descriptor_pool, pool);
	zest__dx_compute_bindless_regions(ZEST_DX_BACKEND(set_layout, layout)->binding_counts, &backend->regions);
	ZEST_ASSERT(backend->regions.resource_descriptor_count <= D3D12_MAX_SHADER_VISIBLE_DESCRIPTOR_HEAP_SIZE_TIER_2, "The bindless counts need more descriptors than a shader visible D3D12 heap can hold.");
	ZEST_ASSERT(backend->regions.sampler_descriptor_count <= D3D12_MAX_SHADER_VISIBLE_SAMPLER_HEAP_SIZE, "The bindless sampler count is over the shader visible D3D12 sampler heap limit.");

	//An empty heap can't be created, and both are always bound together
	D3D12_DESCRIPTOR_HEAP_DESC description = ZEST__ZERO_INIT(D3D12_DESCRIPTOR_HEAP_DESC);
	description.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	description.NumDescriptors = ZEST__MAX(backend->regions.resource_descriptor_count, 1);
	description.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	device_backend->last_result = ZEST_DX_CALL(device_backend->device, CreateDescriptorHeap, &description, ZEST_DX_IID(ID3D12DescriptorHeap), (void **)&backend->resource_heap);
	if (SUCCEEDED(device_backend->last_result)) {
		description.Type = D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER;
		description.NumDescriptors = ZEST__MAX(backend->regions.sampler_descriptor_count, 1);
		device_backend->last_result = ZEST_DX_CALL(device_backend->device, CreateDescriptorHeap, &description, ZEST_DX_IID(ID3D12DescriptorHeap), (void **)&backend->sampler_heap);
	}
	if (FAILED(device_backend->last_result)) {
		zest__dx_log_hresult(device, device_backend->last_result, __FILE__, __LINE__);
		ZEST_DX_RELEASE(backend->resource_heap);
		ZEST_DX_RELEASE(backend->sampler_heap);
		//The caller frees the pool itself
		ZEST__FREE(allocator, backend);
		pool->backend = NULL;
		return ZEST_FALSE;
	}
	backend->resource_increment = ZEST_DX_CALL(device_backend->device, GetDescriptorHandleIncrementSize, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	backend->sampler_increment = ZEST_DX_CALL(device_backend->device, GetDescriptorHandleIncrementSize, D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);
	backend->resource_cpu_start = zest__dx_cpu_heap_start(backend->resource_heap);
	backend->sampler_cpu_start = zest__dx_cpu_heap_start(backend->sampler_heap);
	layout->pool = pool;
	return ZEST_TRUE;
}

zest_descriptor_set zest__dx_create_bindless_set(zest_set_layout layout) {
	ZEST_ASSERT(layout->pool && layout->pool->backend, "The bindless set layout has no descriptor pool.");
	zest_device device = layout->device;
	zest_descriptor_set set = (zest_descriptor_set)ZEST__NEW(device->allocator, zest_descriptor_set);
	*set = ZEST__ZERO_INIT(zest_descriptor_set_t);
	zest_dx_descriptor_set_backend_t *backend = (zest_dx_descriptor_set_backend_t *)ZEST__ALLOCATE(device->allocator, sizeof(zest_dx_descriptor_set_backend_t));
	*backend = ZEST__ZERO_INIT(zest_dx_descriptor_set_backend_t);
	backend->pool = ZEST_DX_BACKEND(descriptor_pool, layout->pool);
	set->backend = (zest_descriptor_set_backend)backend;
	set->magic = zest_INIT_MAGIC(zest_struct_type_descriptor_set);
	return set;
}

void zest__dx_update_bindless_image_descriptor(zest_device device, zest_uint binding_number, zest_uint array_index, zest_descriptor_type type, zest_image image, zest_image_view view, zest_sampler sampler, zest_descriptor_set set) {
	zest_dx_descriptor_pool_backend_t *pool_backend = ZEST_DX_BACKEND(descriptor_set, set)->pool;
	ID3D12Device *native_device = ZEST_DX_BACKEND(device, device)->device;
	switch (type) {
		case zest_descriptor_type_sampler: {
			ZEST_ASSERT(sampler, "A sampler descriptor write needs a sampler.");
			D3D12_CPU_DESCRIPTOR_HANDLE destination = pool_backend->sampler_cpu_start;
			destination.ptr += (SIZE_T)array_index * pool_backend->sampler_increment;
			ZEST_DX_CALL(native_device, CopyDescriptorsSimple, 1, destination, ZEST_DX_BACKEND(sampler, sampler)->descriptor, D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);
			break;
		}
		case zest_descriptor_type_sampled_image: {
			D3D12_CPU_DESCRIPTOR_HANDLE source = ZEST_DX_BACKEND(image_view, view)->srv;
			ZEST_ASSERT(source.ptr, "The view has no SRV, so its image wasn't created with zest_image_flag_sampled.");
			D3D12_CPU_DESCRIPTOR_HANDLE destination = pool_backend->resource_cpu_start;
			destination.ptr += (SIZE_T)(pool_backend->regions.resource_offsets[binding_number] + array_index) * pool_backend->resource_increment;
			ZEST_DX_CALL(native_device, CopyDescriptorsSimple, 1, destination, source, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
			break;
		}
		default:
			ZEST_ASSERT(0, "This descriptor type can't be written to the D3D12 bindless image bindings.");
			break;
	}
}

void zest__dx_cleanup_set_layout_backend(zest_set_layout layout) {
	zloc_allocator *allocator = layout->context ? layout->context->allocator : layout->device->allocator;
	if (layout->backend) {
		ZEST__FREE(allocator, layout->backend);
		layout->backend = NULL;
	}
	//Pools have no cleanup callback of their own, so the layout releases its pool's heaps
	if (layout->pool && layout->pool->backend) {
		zest_dx_descriptor_pool_backend_t *pool_backend = ZEST_DX_BACKEND(descriptor_pool, layout->pool);
		ZEST_DX_RELEASE(pool_backend->resource_heap);
		ZEST_DX_RELEASE(pool_backend->sampler_heap);
		ZEST__FREE(allocator, pool_backend);
		layout->pool->backend = NULL;
	}
}

void *zest__dx_new_pipeline_layout_backend(zest_device device) {
	zest_dx_pipeline_layout_backend_t *backend = (zest_dx_pipeline_layout_backend_t *)ZEST__ALLOCATE(device->allocator, sizeof(zest_dx_pipeline_layout_backend_t));
	*backend = ZEST__ZERO_INIT(zest_dx_pipeline_layout_backend_t);
	return backend;
}

//Root signatures come from each pipeline's shaders, so the layout only carries the root constant size
zest_bool zest__dx_build_pipeline_layout(zest_device device, zest_pipeline_layout layout, zest_pipeline_layout_info_t *info) {
	ZEST_DX_BACKEND(pipeline_layout, layout)->push_constant_size = info->push_constant_range.size;
	return ZEST_TRUE;
}

void zest__dx_cleanup_pipeline_layout_backend(zest_pipeline_layout layout) {
	if (!layout->backend) return;
	ZEST__FREE(layout->device->allocator, layout->backend);
	layout->backend = NULL;
}
// -- End Descriptor_heaps

// -- Device_OS
void zest__dx_set_depth_format(zest_device device) {
	device->depth_format = zest_format_d32_sfloat;
}

void zest__dx_wait_for_idle_device(zest_device device) {
	zest_vec_foreach(family_index, device->queue_families) {
		zest_queue_manager manager = device->queue_families[family_index];
		if (!manager) continue;
		zest_vec_foreach(queue_index, manager->queues) {
			zest_queue queue = &manager->queues[queue_index];
			zest_dx_queue_backend_t *queue_backend = ZEST_DX_BACKEND(queue, queue);
			zest_dx_execution_timeline_backend_t *timeline_backend = ZEST_DX_BACKEND(execution_timeline, &queue->timeline);
			if (!queue_backend || !timeline_backend) continue;
			queue->timeline.current_value++;
			HRESULT result = ZEST_DX_CALL(queue_backend->queue, Signal, timeline_backend->fence, queue->timeline.current_value);
			if (FAILED(result)) {
				queue->timeline.current_value--;
				zest__dx_log_hresult(device, result, __FILE__, __LINE__);
				continue;
			}
			zest_WaitForSignal(&queue->timeline, ZEST__DX_WAIT_FOREVER);
		}
	}
}

void zest__dx_get_memory_budget(zest_device device, zest_memory_budget_t *budget) {
	zest_dx_device_backend_t *backend = ZEST_DX_BACKEND(device, device);
	DXGI_QUERY_VIDEO_MEMORY_INFO local_info = ZEST__ZERO_INIT(DXGI_QUERY_VIDEO_MEMORY_INFO);
	if (FAILED(ZEST_DX_CALL(backend->adapter, QueryVideoMemoryInfo, 0, DXGI_MEMORY_SEGMENT_GROUP_LOCAL, &local_info))) {
		return;
	}
	DXGI_QUERY_VIDEO_MEMORY_INFO non_local_info = ZEST__ZERO_INIT(DXGI_QUERY_VIDEO_MEMORY_INFO);
	if (FAILED(ZEST_DX_CALL(backend->adapter, QueryVideoMemoryInfo, 0, DXGI_MEMORY_SEGMENT_GROUP_NON_LOCAL, &non_local_info))) {
		non_local_info = ZEST__ZERO_INIT(DXGI_QUERY_VIDEO_MEMORY_INFO);
	}
	DXGI_ADAPTER_DESC3 description = ZEST__ZERO_INIT(DXGI_ADAPTER_DESC3);
	ZEST_DX_CALL(backend->adapter, GetDesc3, &description);

	//Unified memory adapters report everything in the local segment group and a zero non-local budget
	zest_bool unified_memory = non_local_info.Budget == 0;
	budget->supported = ZEST_TRUE;
	budget->heap_count = unified_memory ? 1 : 2;
	budget->heaps[0].heap_size = (zest_size)description.DedicatedVideoMemory + (unified_memory ? (zest_size)description.SharedSystemMemory : 0);
	budget->heaps[0].budget = (zest_size)local_info.Budget;
	budget->heaps[0].usage = (zest_size)local_info.CurrentUsage;
	budget->heaps[0].device_local = ZEST_TRUE;
	if (!unified_memory) {
		budget->heaps[1].heap_size = (zest_size)description.SharedSystemMemory;
		budget->heaps[1].budget = (zest_size)non_local_info.Budget;
		budget->heaps[1].usage = (zest_size)non_local_info.CurrentUsage;
		budget->heaps[1].device_local = ZEST_FALSE;
	}
	budget->device_local_size = budget->heaps[0].heap_size;
	budget->device_local_budget = budget->heaps[0].budget;
	budget->device_local_usage = budget->heaps[0].usage;
}

ID3D12Device *zest_GetD3D12Device(zest_device device) {
	ZEST_ASSERT_HANDLE(device);	//Not a valid device handle
	return ZEST_DX_BACKEND(device, device)->device;
}
// -- End Device_OS

// -- Images
D3D12_FEATURE_DATA_FORMAT_SUPPORT zest__dx_format_support(ID3D12Device *native_device, DXGI_FORMAT format) {
	D3D12_FEATURE_DATA_FORMAT_SUPPORT support = ZEST__ZERO_INIT(D3D12_FEATURE_DATA_FORMAT_SUPPORT);
	support.Format = format;
	if (FAILED(ZEST_DX_CALL(native_device, CheckFeatureSupport, D3D12_FEATURE_FORMAT_SUPPORT, &support, (UINT)sizeof(support)))) {
		support.Support1 = D3D12_FORMAT_SUPPORT1_NONE;
		support.Support2 = D3D12_FORMAT_SUPPORT2_NONE;
	}
	return support;
}

zest_bool zest__dx_is_image_format_supported(zest_device device, zest_format format, zest_image_flags flags) {
	//zest__dx_create_image rejects host visible images
	if (ZEST__FLAGGED(flags, zest_image_flag_host_visible)) {
		return ZEST_FALSE;
	}
	zest_capability_flags required_capability = zest__compressed_format_capability(format);
	if (required_capability && !ZEST__FLAGGED(device->capabilities.enabled, required_capability)) {
		return ZEST_FALSE;
	}
	DXGI_FORMAT dxgi_format = zest__dx_to_dxgi_format(format);
	if (dxgi_format == DXGI_FORMAT_UNKNOWN) {
		return ZEST_FALSE;
	}
	ID3D12Device *native_device = ZEST_DX_BACKEND(device, device)->device;
	DXGI_FORMAT view_format = zest__dx_shader_view_format(dxgi_format);
	//Depth formats are sampled through their shader view format
	if (ZEST__FLAGGED(flags, zest_image_flag_sampled) && view_format != dxgi_format && !(zest__dx_format_support(native_device, view_format).Support1 & D3D12_FORMAT_SUPPORT1_SHADER_LOAD)) {
		return ZEST_FALSE;
	}
	D3D12_FEATURE_DATA_FORMAT_SUPPORT support = zest__dx_format_support(native_device, dxgi_format);
	return zest__dx_format_support_allows(&support, flags, view_format == dxgi_format);
}

zest_bool zest__dx_format_support_allows(const D3D12_FEATURE_DATA_FORMAT_SUPPORT *support, zest_image_flags flags, zest_bool check_shader_load) {
	UINT required = D3D12_FORMAT_SUPPORT1_TEXTURE2D;
	required |= ZEST__FLAGGED(flags, zest_image_flag_color_attachment) ? D3D12_FORMAT_SUPPORT1_RENDER_TARGET : 0;
	required |= ZEST__FLAGGED(flags, zest_image_flag_depth_stencil_attachment) ? D3D12_FORMAT_SUPPORT1_DEPTH_STENCIL : 0;
	required |= ZEST__FLAGGED(flags, zest_image_flag_storage) ? D3D12_FORMAT_SUPPORT1_TYPED_UNORDERED_ACCESS_VIEW : 0;
	required |= ZEST__FLAGGED(flags, zest_image_flag_sampled) && check_shader_load ? D3D12_FORMAT_SUPPORT1_SHADER_LOAD : 0;
	//Matches Vulkan's storage images, which shaders can both load from and store to
	UINT required2 = ZEST__FLAGGED(flags, zest_image_flag_storage) ? D3D12_FORMAT_SUPPORT2_UAV_TYPED_LOAD | D3D12_FORMAT_SUPPORT2_UAV_TYPED_STORE : 0;
	return ((UINT)support->Support1 & required) == required && ((UINT)support->Support2 & required2) == required2;
}

void *zest__dx_new_image_backend(zest_device device) {
	zest_dx_image_backend_t *backend = (zest_dx_image_backend_t *)ZEST__ALLOCATE(device->allocator, sizeof(zest_dx_image_backend_t));
	*backend = ZEST__ZERO_INIT(zest_dx_image_backend_t);
	return backend;
}

zest_bool zest__dx_create_image(zest_device device, zest_context context, zest_image image, zest_uint layer_count, zest_sample_count_flags num_samples, zest_image_flags flags) {
	//Transient images are created unbound and placed in a frame graph arena instead
	ZEST_ASSERT(ZEST__NOT_FLAGGED(flags, zest_image_flag_transient));
	if (ZEST__FLAGGED(flags, zest_image_flag_host_visible)) {
		ZEST_REPORT(device, zest_report_invalid_resource, "D3D12 images must be device local, so zest_image_flag_host_visible isn't supported. Upload through a staging buffer instead.");
		return ZEST_FALSE;
	}
	DXGI_FORMAT format = zest__dx_to_dxgi_format(image->info.format);
	if (format == DXGI_FORMAT_UNKNOWN) {
		ZEST_REPORT(device, zest_report_invalid_resource, "Image format %i has no DXGI equivalent.", (int)image->info.format);
		return ZEST_FALSE;
	}
	zest_dx_device_backend_t *device_backend = ZEST_DX_BACKEND(device, device);
	zest_dx_image_backend_t *backend = ZEST_DX_BACKEND(image, image);
	zest_bool is_3d = image->info.extent.depth > 1;
	zest_bool sampled_depth = zest__is_depth_stencil_format(image->info.format) && ZEST__FLAGGED(flags, zest_image_flag_sampled);

	D3D12_RESOURCE_DESC1 description = ZEST__ZERO_INIT(D3D12_RESOURCE_DESC1);
	description.Dimension = is_3d ? D3D12_RESOURCE_DIMENSION_TEXTURE3D : D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	description.Width = image->info.extent.width;
	description.Height = image->info.extent.height;
	description.DepthOrArraySize = (UINT16)(is_3d ? image->info.extent.depth : layer_count);
	description.MipLevels = (UINT16)image->info.mip_levels;
	description.Format = sampled_depth ? zest__dx_typeless_format(format) : format;
	description.SampleDesc.Count = num_samples;
	description.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	description.Flags = D3D12_RESOURCE_FLAG_NONE;
	if (ZEST__FLAGGED(flags, zest_image_flag_color_attachment)) description.Flags |= D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
	if (ZEST__FLAGGED(flags, zest_image_flag_depth_stencil_attachment)) description.Flags |= D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
	if (ZEST__FLAGGED(flags, zest_image_flag_storage)) description.Flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
	//Lets drivers keep depth compression on depth images that are never sampled
	if (ZEST__FLAGGED(flags, zest_image_flag_depth_stencil_attachment) && ZEST__NOT_FLAGGED(flags, zest_image_flag_sampled)) description.Flags |= D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE;

	D3D12_RESOURCE_ALLOCATION_INFO allocation_info = zest__dx_resource_allocation_info(device_backend->device10, &description);
	if (allocation_info.SizeInBytes == UINT64_MAX) {
		ZEST_REPORT(device, zest_report_invalid_resource, "D3D12 rejected the image description (format %i, %ux%ux%u, %u layers, %u mips, %u samples).",
			(int)image->info.format, image->info.extent.width, image->info.extent.height, image->info.extent.depth, layer_count, image->info.mip_levels, num_samples);
		return ZEST_FALSE;
	}

	//Every D3D12 texture can be copied, and a non zero usage is what routes the request to an image pool
	zest_buffer_info_t buffer_info = ZEST__ZERO_INIT(zest_buffer_info_t);
	buffer_info.image_usage_flags = zest_image_usage_transfer_src_bit | zest_image_usage_transfer_dst_bit;
	buffer_info.image_usage_flags |= ZEST__FLAGGED(flags, zest_image_flag_sampled) ? zest_image_usage_sampled_bit : 0;
	buffer_info.image_usage_flags |= ZEST__FLAGGED(flags, zest_image_flag_storage) ? zest_image_usage_storage_bit : 0;
	buffer_info.image_usage_flags |= ZEST__FLAGGED(flags, zest_image_flag_color_attachment) ? zest_image_usage_color_attachment_bit : 0;
	buffer_info.image_usage_flags |= ZEST__FLAGGED(flags, zest_image_flag_depth_stencil_attachment) ? zest_image_usage_depth_stencil_attachment_bit : 0;
	buffer_info.image_usage_flags |= ZEST__FLAGGED(flags, zest_image_flag_input_attachment) ? zest_image_usage_input_attachment_bit : 0;
	buffer_info.property_flags = zest_memory_property_device_local_bit;
	buffer_info.alignment = allocation_info.Alignment;
	buffer_info.backend_memory_bits = zest__dx_image_heap_category(flags, num_samples, device_backend->support.resource_heap_tier);

	zest_device_memory memory = NULL;
	zest_buffer buffer = NULL;
	ID3D12Heap *heap = NULL;
	zest_size heap_offset = 0;
	if (allocation_info.SizeInBytes >= device->setup_info.dedicated_image_memory_threshold) {
		memory = zest__create_device_memory(device, allocation_info.SizeInBytes, &buffer_info, buffer_info.backend_memory_bits);
		if (!memory) {
			return ZEST_FALSE;
		}
		heap = ZEST_DX_BACKEND(device_memory, memory)->heap;
	} else {
		buffer = zest_CreateBuffer(device, allocation_info.SizeInBytes, &buffer_info);
		if (!buffer) {
			return ZEST_FALSE;
		}
		ZEST_ASSERT(buffer->memory_offset % allocation_info.Alignment == 0, "The image pool returned an offset that isn't aligned for the image.");
		heap = ZEST_DX_BACKEND(device_memory_pool, buffer->memory_pool)->heap;
		heap_offset = buffer->memory_offset;
	}

	device_backend->last_result = ZEST_DX_CALL(device_backend->device10, CreatePlacedResource2, heap, heap_offset, &description, D3D12_BARRIER_LAYOUT_UNDEFINED,
		NULL, 0, NULL, ZEST_DX_IID(ID3D12Resource), (void **)&backend->resource);
	if (FAILED(device_backend->last_result)) {
		zest__dx_log_hresult(device, device_backend->last_result, __FILE__, __LINE__);
		backend->resource = NULL;
		if (memory) {
			zest__dx_cleanup_memory_backend(memory);
			ZEST__FREE(device->allocator, memory);
		}
		zest_FreeBufferNow(buffer);
		return ZEST_FALSE;
	}
	backend->resource_format = description.Format;
	backend->layout = D3D12_BARRIER_LAYOUT_UNDEFINED;
	backend->sample_count = num_samples;
	image->layout = zest_image_layout_undefined;
	if (memory) {
		image->buffer = (void *)memory;
		//Cleanup reads this flag to know which kind of backing to free
		image->info.flags |= zest_image_flag_dedicated_memory;
	} else {
		image->buffer = (void *)buffer;
	}
	#ifdef ZEST_DEBUGGING
	image->buffer_identifier = (void *)backend->resource;
	#endif
	return ZEST_TRUE;
}

void zest__dx_cleanup_image_backend(zest_image image) {
	zest_dx_image_backend_t *backend = ZEST_DX_BACKEND(image, image);
	if (!backend) return;
	zest_device device = (zest_device)image->handle.store->origin;
	ZEST_ASSERT(device);	//The store was not set in the image handle!
	ZEST_DX_RELEASE(backend->resource);
	if (ZEST__NOT_FLAGGED(image->info.flags, zest_image_flag_transient)) {
		if (ZEST__FLAGGED(image->info.flags, zest_image_flag_dedicated_memory)) {
			//The zest_device_memory struct itself is freed by zest__cleanup_image
			zest__dx_cleanup_memory_backend((zest_device_memory)image->buffer);
		}
		ZEST__FREE(device->allocator, backend);
	}
	image->backend = NULL;
}

D3D12_SHADER_RESOURCE_VIEW_DESC zest__dx_image_srv_description(zest_image image, zest_image_view_type view_type, zest_uint mip_levels_this_view, zest_uint base_mip, zest_uint base_array_index, zest_uint layer_count) {
	D3D12_SHADER_RESOURCE_VIEW_DESC description = ZEST__ZERO_INIT(D3D12_SHADER_RESOURCE_VIEW_DESC);
	description.Format = zest__dx_shader_view_format(zest__dx_to_dxgi_format(image->info.format));
	zest_component_mapping_t swizzle = image->info.swizzle;
	//Vulkan's BC1 RGB formats have no alpha, so anything that would read the shared BC1 layout's alpha reads one instead
	if (image->info.format == zest_format_bc1_rgb_unorm_block || image->info.format == zest_format_bc1_rgb_srgb_block) {
		if (swizzle.r == zest_component_swizzle_a) swizzle.r = zest_component_swizzle_one;
		if (swizzle.g == zest_component_swizzle_a) swizzle.g = zest_component_swizzle_one;
		if (swizzle.b == zest_component_swizzle_a) swizzle.b = zest_component_swizzle_one;
		if (swizzle.a == zest_component_swizzle_a || swizzle.a == zest_component_swizzle_identity) swizzle.a = zest_component_swizzle_one;
	}
	description.Shader4ComponentMapping = zest__dx_component_mapping(swizzle);
	zest_bool multisampled = ZEST_DX_BACKEND(image, image)->sample_count > 1;
	switch (view_type) {
		case zest_image_view_type_3d: {
			description.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE3D;
			description.Texture3D.MostDetailedMip = base_mip;
			description.Texture3D.MipLevels = mip_levels_this_view;
			break;
		}
		case zest_image_view_type_cube:
		case zest_image_view_type_cube_array: {
			//A plain cube view always starts at layer 0, so a cube further into the array needs the array form
			if (view_type == zest_image_view_type_cube && base_array_index == 0) {
				description.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
				description.TextureCube.MostDetailedMip = base_mip;
				description.TextureCube.MipLevels = mip_levels_this_view;
			} else {
				description.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBEARRAY;
				description.TextureCubeArray.MostDetailedMip = base_mip;
				description.TextureCubeArray.MipLevels = mip_levels_this_view;
				description.TextureCubeArray.First2DArrayFace = base_array_index;
				description.TextureCubeArray.NumCubes = layer_count / 6;
			}
			break;
		}
		default: {
			//A 2D view can only see layer 0, so a view of any other layer needs the array form
			if (view_type == zest_image_view_type_2d_array || view_type == zest_image_view_type_1d_array || base_array_index != 0) {
				if (multisampled) {
					description.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DMSARRAY;
					description.Texture2DMSArray.FirstArraySlice = base_array_index;
					description.Texture2DMSArray.ArraySize = layer_count;
				} else {
					description.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
					description.Texture2DArray.MostDetailedMip = base_mip;
					description.Texture2DArray.MipLevels = mip_levels_this_view;
					description.Texture2DArray.FirstArraySlice = base_array_index;
					description.Texture2DArray.ArraySize = layer_count;
				}
			} else if (multisampled) {
				description.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DMS;
			} else {
				description.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
				description.Texture2D.MostDetailedMip = base_mip;
				description.Texture2D.MipLevels = mip_levels_this_view;
			}
			break;
		}
	}
	return description;
}

zest_image_view_t *zest__dx_create_image_view(zest_device device, zest_image image, zest_image_view_type view_type, zest_uint mip_levels_this_view, zest_uint base_mip, zest_uint base_array_index, zest_uint layer_count, zloc_linear_allocator_t *linear_allocator) {
	zest_size total_size = sizeof(zest_image_view_t) + sizeof(zest_dx_image_view_backend_t);
	void *memory = NULL;
	zest_size marker = 0;
	if (linear_allocator) {
		marker = zloc_GetMarker(linear_allocator);
		memory = zest__linear_allocate(linear_allocator, total_size);
	} else {
		memory = ZEST__ALLOCATE(device->allocator, total_size);
	}
	zest_image_view_t *image_view = (zest_image_view_t *)memory;
	zest_dx_image_view_backend_t *backend = (zest_dx_image_view_backend_t *)((char *)memory + sizeof(zest_image_view_t));
	*image_view = ZEST__ZERO_INIT(zest_image_view_t);
	*backend = ZEST__ZERO_INIT(zest_dx_image_view_backend_t);
	image_view->image = image;
	image_view->backend = (zest_image_view_backend_t *)backend;

	//Only sampled images get an SRV, as a depth image is only created typeless when it's sampled
	if (ZEST__NOT_FLAGGED(image->info.flags, zest_image_flag_sampled)) {
		return image_view;
	}
	zest_dx_device_backend_t *device_backend = ZEST_DX_BACKEND(device, device);
	backend->srv = zest__dx_allocate_cpu_descriptor(device, &device_backend->cpu_resource_heap);
	if (!backend->srv.ptr) {
		if (linear_allocator) {
			zloc_ResetToMarker(linear_allocator, marker);
		} else {
			ZEST__FREE(device->allocator, memory);
		}
		return NULL;
	}
	D3D12_SHADER_RESOURCE_VIEW_DESC description = zest__dx_image_srv_description(image, view_type, mip_levels_this_view, base_mip, base_array_index, layer_count);
	ZEST_DX_CALL(device_backend->device, CreateShaderResourceView, ZEST_DX_BACKEND(image, image)->resource, &description, backend->srv);
	return image_view;
}

void zest__dx_cleanup_image_view_backend(zest_image_view view) {
	zest_dx_image_view_backend_t *backend = ZEST_DX_BACKEND(image_view, view);
	if (!backend) return;
	zest_device device = (zest_device)view->handle.store->origin;
	zest__dx_free_cpu_descriptor(device, &ZEST_DX_BACKEND(device, device)->cpu_resource_heap, backend->srv);
	backend->srv.ptr = 0;
}

void *zest__dx_new_sampler_backend(zest_device device) {
	zest_dx_sampler_backend_t *backend = (zest_dx_sampler_backend_t *)ZEST__ALLOCATE(device->allocator, sizeof(zest_dx_sampler_backend_t));
	*backend = ZEST__ZERO_INIT(zest_dx_sampler_backend_t);
	return backend;
}

zest_bool zest__dx_create_sampler(zest_sampler sampler) {
	zest_device device = (zest_device)sampler->handle.store->origin;
	zest_dx_device_backend_t *device_backend = ZEST_DX_BACKEND(device, device);
	zest_dx_sampler_backend_t *backend = ZEST_DX_BACKEND(sampler, sampler);
	const zest_sampler_info_t *info = &sampler->create_info;
	D3D12_FILTER_REDUCTION_TYPE reduction = info->compare_enable ? D3D12_FILTER_REDUCTION_TYPE_COMPARISON : D3D12_FILTER_REDUCTION_TYPE_STANDARD;

	D3D12_SAMPLER_DESC description = ZEST__ZERO_INIT(D3D12_SAMPLER_DESC);
	if (info->anisotropy_enable) {
		description.Filter = D3D12_ENCODE_ANISOTROPIC_FILTER(reduction);
		description.MaxAnisotropy = (UINT)ZEST__CLAMP(info->max_anisotropy, 1.f, (float)D3D12_REQ_MAXANISOTROPY);
	} else {
		D3D12_FILTER_TYPE min_filter = info->min_filter == zest_filter_linear ? D3D12_FILTER_TYPE_LINEAR : D3D12_FILTER_TYPE_POINT;
		D3D12_FILTER_TYPE mag_filter = info->mag_filter == zest_filter_linear ? D3D12_FILTER_TYPE_LINEAR : D3D12_FILTER_TYPE_POINT;
		D3D12_FILTER_TYPE mip_filter = info->mipmap_mode == zest_mipmap_mode_linear ? D3D12_FILTER_TYPE_LINEAR : D3D12_FILTER_TYPE_POINT;
		description.Filter = D3D12_ENCODE_BASIC_FILTER(min_filter, mag_filter, mip_filter, reduction);
		description.MaxAnisotropy = 1;
	}
	description.AddressU = zest__dx_address_modes[info->address_mode_u];
	description.AddressV = zest__dx_address_modes[info->address_mode_v];
	description.AddressW = zest__dx_address_modes[info->address_mode_w];
	description.MipLODBias = info->mip_lod_bias;
	//The debug layer warns about a comparison function on a non comparison filter
	description.ComparisonFunc = info->compare_enable ? zest__dx_comparison_funcs[info->compare_op] : D3D12_COMPARISON_FUNC_NONE;
	const FLOAT *border_color = zest__dx_border_colors[info->border_color / 2];
	description.BorderColor[0] = border_color[0];
	description.BorderColor[1] = border_color[1];
	description.BorderColor[2] = border_color[2];
	description.BorderColor[3] = border_color[3];
	description.MinLOD = info->min_lod;
	description.MaxLOD = info->max_lod;

	backend->descriptor = zest__dx_allocate_cpu_descriptor(device, &device_backend->cpu_sampler_heap);
	if (!backend->descriptor.ptr) {
		return ZEST_FALSE;
	}
	ZEST_DX_CALL(device_backend->device, CreateSampler, &description, backend->descriptor);
	return ZEST_TRUE;
}

void zest__dx_cleanup_sampler_backend(zest_sampler sampler) {
	zest_dx_sampler_backend_t *backend = ZEST_DX_BACKEND(sampler, sampler);
	if (!backend) return;
	zest_device device = (zest_device)sampler->handle.store->origin;
	zest__dx_free_cpu_descriptor(device, &ZEST_DX_BACKEND(device, device)->cpu_sampler_heap, backend->descriptor);
	ZEST__FREE(device->allocator, backend);
	sampler->backend = NULL;
}

int zest__dx_get_image_raw_layout(zest_image image) {
	return (int)image->layout;
}

zest_bool zest__dx_image_layout_is_valid_for_descriptor(zest_image image) {
	switch (image->layout) {
		case zest_image_layout_depth_stencil_read_only_optimal:
		case zest_image_layout_shader_read_only_optimal:
		case zest_image_layout_general:
		case zest_image_layout_depth_read_only_stencil_attachment_optimal:
		case zest_image_layout_depth_attachment_stencil_read_only_optimal:
		case zest_image_layout_read_only_optimal:
		case zest_image_layout_depth_read_only_optimal:
		case zest_image_layout_stencil_read_only_optimal:
			return ZEST_TRUE;
		default:
			return ZEST_FALSE;
	}
}

//The common layout, which a copy queue leaves textures in, only allows shader reads and copies
D3D12_BARRIER_ACCESS zest__dx_common_layout_access(D3D12_BARRIER_LAYOUT layout, D3D12_BARRIER_ACCESS access) {
	if (layout != D3D12_BARRIER_LAYOUT_COMMON || access == D3D12_BARRIER_ACCESS_COMMON) {
		return access;
	}
	UINT common_access = access & (D3D12_BARRIER_ACCESS_SHADER_RESOURCE | D3D12_BARRIER_ACCESS_COPY_SOURCE | D3D12_BARRIER_ACCESS_COPY_DEST);
	return common_access ? (D3D12_BARRIER_ACCESS)common_access : D3D12_BARRIER_ACCESS_NO_ACCESS;
}

//A non NULL exact_layout_after restores a recorded D3D12 layout instead of deriving one from new_layout
zest_bool zest__dx_transition_image_layout(zest_queue queue, zest_image image, zest_image_layout new_layout, const D3D12_BARRIER_LAYOUT *exact_layout_after, zest_uint base_mip_index, zest_uint mip_levels, zest_uint base_array_index, zest_uint layer_count) {
	ZEST_ASSERT(queue);	//No queue was acquired
	zest_dx_image_backend_t *backend = ZEST_DX_BACKEND(image, image);
	zest_device_queue_type queue_type = zest__dx_queue_type(queue);
	zest_access_flags access_before = 0;
	zest_access_flags access_after = 0;
	zest_pipeline_stage_flags stage_before = 0;
	zest_pipeline_stage_flags stage_after = 0;
	zest__dx_layout_access_and_stage(image->layout, queue_type, &access_before, &stage_before);
	zest__dx_layout_access_and_stage(new_layout, queue_type, &access_after, &stage_after);

	D3D12_TEXTURE_BARRIER barrier = ZEST__ZERO_INIT(D3D12_TEXTURE_BARRIER);
	barrier.LayoutBefore = backend->layout;
	barrier.LayoutAfter = exact_layout_after ? *exact_layout_after : zest__dx_barrier_layout(new_layout, access_after, queue_type);
	barrier.AccessBefore = zest__dx_common_layout_access(barrier.LayoutBefore, zest__dx_barrier_access(access_before, ZEST_TRUE, barrier.LayoutBefore));
	barrier.AccessAfter = zest__dx_common_layout_access(barrier.LayoutAfter, zest__dx_barrier_access(access_after, ZEST_TRUE, barrier.LayoutAfter));
	barrier.SyncBefore = zest__dx_normalise_sync(zest__dx_barrier_sync(stage_before, ZEST_TRUE), barrier.AccessBefore);
	barrier.SyncAfter = zest__dx_normalise_sync(zest__dx_barrier_sync(stage_after, ZEST_FALSE), barrier.AccessAfter);
	barrier.pResource = backend->resource;
	barrier.Subresources.IndexOrFirstMipLevel = base_mip_index;
	barrier.Subresources.NumMipLevels = mip_levels;
	barrier.Subresources.FirstArraySlice = base_array_index;
	barrier.Subresources.NumArraySlices = layer_count;
	barrier.Subresources.FirstPlane = 0;
	barrier.Subresources.NumPlanes = zest__dx_plane_count(image->info.format);
	//Placed render and depth targets have to be initialised by a clear, discard or copy before first use
	if (barrier.LayoutBefore == D3D12_BARRIER_LAYOUT_UNDEFINED && ZEST__FLAGGED(image->info.flags, zest_image_flag_color_attachment | zest_image_flag_depth_stencil_attachment)) {
		barrier.Flags = D3D12_TEXTURE_BARRIER_FLAG_DISCARD;
	}

	D3D12_BARRIER_GROUP group = ZEST__ZERO_INIT(D3D12_BARRIER_GROUP);
	group.Type = D3D12_BARRIER_TYPE_TEXTURE;
	group.NumBarriers = 1;
	group.pTextureBarriers = &barrier;
	ZEST_DX_CALL(ZEST_DX_BACKEND(queue, queue)->command_list, Barrier, 1, &group);

	backend->layout = barrier.LayoutAfter;
	image->layout = new_layout;
	return ZEST_TRUE;
}

zest_bool zest__dx_transition_image(zest_queue queue, zest_image image, zest_resource_state new_state, zest_uint base_mip_index, zest_uint mip_levels, zest_uint base_array_index, zest_uint layer_count) {
	zest_image_layout new_layout = zest__dx_image_layout_from_resource_state(new_state, zest__is_depth_stencil_format(image->info.format));
	return zest__dx_transition_image_layout(queue, image, new_layout, NULL, base_mip_index, mip_levels, base_array_index, layer_count);
}

//Runs before any footprint maths, where a mip or layer outside the image gives a zero footprint width
zest_bool zest__dx_copy_region_is_valid(zest_device device, const D3D12_RESOURCE_DESC1 *description, const zest_buffer_image_copy_t *region) {
	zest_bool is_3d = description->Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE3D;
	zest_uint array_size = is_3d ? 1 : description->DepthOrArraySize;
	if (region->mip_level >= description->MipLevels) {
		ZEST_ASSERT_OR_VALIDATE(ZEST_FALSE, device, "A buffer to image copy region's mip level is outside the image.", ZEST_FALSE);
		return ZEST_FALSE;
	}
	if (!region->layer_count || region->layer_count > array_size || region->base_array_layer > array_size - region->layer_count) {
		ZEST_ASSERT_OR_VALIDATE(ZEST_FALSE, device, "A buffer to image copy region's array layers are empty or outside the image.", ZEST_FALSE);
		return ZEST_FALSE;
	}
	zest_size mip_width = ZEST__MAX(description->Width >> region->mip_level, 1);
	zest_size mip_height = ZEST__MAX(description->Height >> region->mip_level, 1);
	zest_size mip_depth = is_3d ? ZEST__MAX((zest_size)description->DepthOrArraySize >> region->mip_level, 1) : 1;
	zest_size depth = region->image_extent.depth ? region->image_extent.depth : 1;
	if (region->image_offset.x < 0 || region->image_offset.y < 0 || region->image_offset.z < 0 || !region->image_extent.width || !region->image_extent.height ||
		(zest_size)region->image_offset.x + region->image_extent.width > mip_width || (zest_size)region->image_offset.y + region->image_extent.height > mip_height ||
		(zest_size)region->image_offset.z + depth > mip_depth) {
		ZEST_ASSERT_OR_VALIDATE(ZEST_FALSE, device, "A buffer to image copy region's offset and extent don't fit inside its mip level.", ZEST_FALSE);
		return ZEST_FALSE;
	}
	return ZEST_TRUE;
}

void zest__dx_prepare_region_copy(ID3D12Device10 *native_device, const D3D12_RESOURCE_DESC1 *description, zest_uint plane_count, zest_uint block_width, zest_uint block_height, const zest_buffer_image_copy_t *region, zest_dx_region_copy_t *copy) {
	zest_uint array_size = description->Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE3D ? 1 : description->DepthOrArraySize;
	zest_uint plane = plane_count > 1 && region->image_aspect == zest_image_aspect_stencil_bit ? 1 : 0;
	copy->first_subresource = region->mip_level + region->base_array_layer * description->MipLevels + plane * description->MipLevels * array_size;
	//Every layer of a mip shares one footprint, so a region only needs it once
	D3D12_PLACED_SUBRESOURCE_FOOTPRINT subresource_footprint = ZEST__ZERO_INIT(D3D12_PLACED_SUBRESOURCE_FOOTPRINT);
	UINT64 subresource_row_size = 0;
	ZEST_DX_CALL(native_device, GetCopyableFootprints1, description, copy->first_subresource, 1, 0, &subresource_footprint, NULL, &subresource_row_size, NULL);
	//Taken from the footprint so that each plane of a depth stencil format gets its own block size
	zest_uint subresource_blocks_wide = (subresource_footprint.Footprint.Width + block_width - 1) / block_width;
	zest_size block_bytes = (zest_size)subresource_row_size / subresource_blocks_wide;

	zest_uint blocks_wide = (region->image_extent.width + block_width - 1) / block_width;
	zest_uint blocks_high = (region->image_extent.height + block_height - 1) / block_height;
	zest_uint row_length = region->buffer_row_length ? region->buffer_row_length : region->image_extent.width;
	zest_uint image_height = region->buffer_image_height ? region->buffer_image_height : region->image_extent.height;
	copy->source_row_pitch = (((zest_size)row_length + block_width - 1) / block_width) * block_bytes;
	copy->source_slice_rows = (zest_uint)(((zest_size)image_height + block_height - 1) / block_height);
	copy->source_slice_pitch = copy->source_row_pitch * copy->source_slice_rows;
	copy->row_bytes = blocks_wide * block_bytes;
	copy->row_count = blocks_high;
	copy->footprint = subresource_footprint;
	//Block compressed extents round up to whole blocks but stop at the edge of the subresource
	copy->footprint.Footprint.Width = ZEST__MIN(blocks_wide * block_width, subresource_footprint.Footprint.Width - (zest_uint)region->image_offset.x);
	copy->footprint.Footprint.Height = ZEST__MIN(blocks_high * block_height, subresource_footprint.Footprint.Height - (zest_uint)region->image_offset.y);
	copy->footprint.Footprint.Depth = region->image_extent.depth ? region->image_extent.depth : 1;
}

//Guards both the repack memcpy and the GPU copy, which read up to the last row of the last slice of the last layer
zest_bool zest__dx_copy_region_fits_buffer(zest_device device, const zest_dx_region_copy_t *copy, const zest_buffer_image_copy_t *region, zest_buffer buffer, zest_size src_offset) {
	//Measured against what's left of the buffer instead of adding to the offset, so a huge buffer_offset can't wrap past the check
	zest_size remaining = buffer->size - (src_offset - buffer->memory_offset);
	//A 3D region has one layer and an array region has one slice, so this stays far inside 64 bits
	zest_size rows_after_first = ((zest_size)region->layer_count * copy->footprint.Footprint.Depth - 1) * copy->source_slice_rows + copy->row_count - 1;
	zest_bool fits = src_offset >= buffer->memory_offset && src_offset - buffer->memory_offset <= buffer->size;
	fits = fits && region->buffer_offset <= remaining && copy->row_bytes <= remaining - region->buffer_offset;
	fits = fits && (rows_after_first == 0 || copy->source_row_pitch <= (remaining - region->buffer_offset - copy->row_bytes) / rows_after_first);
	if (!fits) {
		ZEST_ASSERT_OR_VALIDATE(ZEST_FALSE, device, "A buffer to image copy region reads past the end of its source buffer.", ZEST_FALSE);
		return ZEST_FALSE;
	}
	return ZEST_TRUE;
}

zest_bool zest__dx_copy_buffer_regions_to_image(zest_queue queue, zest_buffer_image_copy_t *regions, zest_uint regions_count, zest_buffer buffer, zest_size src_offset, zest_image image) {
	ZEST_ASSERT(queue);	//No queue was acquired
	zest_device device = queue->device;
	zest_dx_device_backend_t *device_backend = ZEST_DX_BACKEND(device, device);
	zest_dx_queue_backend_t *queue_backend = ZEST_DX_BACKEND(queue, queue);
	ID3D12Resource *texture = ZEST_DX_BACKEND(image, image)->resource;
	D3D12_RESOURCE_DESC1 description = zest__dx_resource_description(texture);
	zest_uint plane_count = zest__dx_plane_count(image->info.format);
	int channels, bytes_per_pixel, block_width, block_height, bytes_per_block;
	zest_GetFormatPixelData(image->info.format, &channels, &bytes_per_pixel, &block_width, &block_height, &bytes_per_block);
	zest_bool unrestricted_copy_pitch = device_backend->support.unrestricted_copy_pitch;
	zest_dx_region_copy_t copy;

	//Every region is checked and every repacked layer is sized into one shared staging buffer up front, so that recording can't fail part way
	zest_size repack_size = 0;
	for (zest_uint region_index = 0; region_index != regions_count; ++region_index) {
		const zest_buffer_image_copy_t *region = &regions[region_index];
		if (!zest__dx_copy_region_is_valid(device, &description, region)) {
			return ZEST_FALSE;
		}
		zest__dx_prepare_region_copy(device_backend->device10, &description, plane_count, (zest_uint)block_width, (zest_uint)block_height, region, &copy);
		if (!zest__dx_copy_region_fits_buffer(device, &copy, region, buffer, src_offset)) {
			return ZEST_FALSE;
		}
		zest_size layer_pitch = copy.source_slice_pitch * copy.footprint.Footprint.Depth;
		zest_size repack_layer_size = zloc__align_size_up(copy.row_bytes, D3D12_TEXTURE_DATA_PITCH_ALIGNMENT) * copy.row_count * copy.footprint.Footprint.Depth;
		for (zest_uint layer = 0; layer != region->layer_count; ++layer) {
			if (zest__dx_region_needs_repack(&copy, src_offset + region->buffer_offset + layer * layer_pitch, unrestricted_copy_pitch)) {
				repack_size = zloc__align_size_up(repack_size, D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT) + repack_layer_size;
			}
		}
	}

	const char *source_data = (const char *)buffer->memory_pool->mapped;
	ID3D12Resource *repack_resource = NULL;
	char *repack_data = NULL;
	zest_size repack_base = 0;
	if (repack_size) {
		if (!source_data) {
			ZEST_REPORT(device, zest_report_submission_failure, "An image copy needs its rows repacked, either for a 256 byte row pitch and a 512 byte offset on this device or for padding rows between depth slices, and the source buffer isn't host visible. Upload from a staging buffer instead.");
			return ZEST_FALSE;
		}
		zest_buffer repack_buffer = zest_CreateStagingBuffer(device, repack_size + D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT, NULL);
		if (!repack_buffer) {
			return ZEST_FALSE;
		}
		zest_vec_push(device->allocator, queue_backend->pending_staging_buffers, repack_buffer);
		repack_resource = ZEST_DX_BACKEND(device_memory_pool, repack_buffer->memory_pool)->buffer;
		repack_data = (char *)repack_buffer->memory_pool->mapped;
		repack_base = zloc__align_size_up(repack_buffer->memory_offset, D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT);
	}

	//Nothing below can fail, so each copy is recorded as soon as its footprint is known
	ID3D12Resource *source_resource = ZEST_DX_BACKEND(device_memory_pool, buffer->memory_pool)->buffer;
	D3D12_TEXTURE_COPY_LOCATION destination = ZEST__ZERO_INIT(D3D12_TEXTURE_COPY_LOCATION);
	destination.pResource = texture;
	destination.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
	D3D12_TEXTURE_COPY_LOCATION source = ZEST__ZERO_INIT(D3D12_TEXTURE_COPY_LOCATION);
	source.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
	zest_size repack_offset = 0;
	for (zest_uint region_index = 0; region_index != regions_count; ++region_index) {
		const zest_buffer_image_copy_t *region = &regions[region_index];
		zest__dx_prepare_region_copy(device_backend->device10, &description, plane_count, (zest_uint)block_width, (zest_uint)block_height, region, &copy);
		zest_size layer_pitch = copy.source_slice_pitch * copy.footprint.Footprint.Depth;
		UINT repack_row_pitch = (UINT)zloc__align_size_up(copy.row_bytes, D3D12_TEXTURE_DATA_PITCH_ALIGNMENT);
		zest_size repack_slice_pitch = (zest_size)repack_row_pitch * copy.row_count;
		for (zest_uint layer = 0; layer != region->layer_count; ++layer) {
			zest_size source_offset = src_offset + region->buffer_offset + layer * layer_pitch;
			source.PlacedFootprint = copy.footprint;
			if (zest__dx_region_needs_repack(&copy, source_offset, unrestricted_copy_pitch)) {
				repack_offset = zloc__align_size_up(repack_offset, D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT);
				source.pResource = repack_resource;
				source.PlacedFootprint.Offset = repack_base + repack_offset;
				source.PlacedFootprint.Footprint.RowPitch = repack_row_pitch;
				for (zest_uint slice = 0; slice != copy.footprint.Footprint.Depth; ++slice) {
					zest__dx_repack_rows(source_data + source_offset + slice * copy.source_slice_pitch, copy.source_row_pitch,
						repack_data + source.PlacedFootprint.Offset + slice * repack_slice_pitch, repack_row_pitch, copy.row_bytes, copy.row_count);
				}
				repack_offset += repack_slice_pitch * copy.footprint.Footprint.Depth;
			} else {
				source.pResource = source_resource;
				source.PlacedFootprint.Offset = source_offset;
				source.PlacedFootprint.Footprint.RowPitch = (UINT)copy.source_row_pitch;
			}
			destination.SubresourceIndex = copy.first_subresource + layer * description.MipLevels;
			ZEST_DX_CALL(queue_backend->command_list, CopyTextureRegion, &destination, (UINT)region->image_offset.x, (UINT)region->image_offset.y, (UINT)region->image_offset.z, &source, NULL);
		}
	}
	return ZEST_TRUE;
}
// -- End Images

// -- General_helpers
zest_bool zest__dx_copy_needs_repack(zest_size row_pitch, zest_size offset, zest_bool unrestricted_copy_pitch) {
	if (unrestricted_copy_pitch) {
		return ZEST_FALSE;
	}
	return (row_pitch % D3D12_TEXTURE_DATA_PITCH_ALIGNMENT) != 0 || (offset % D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT) != 0;
}

//A placed footprint's slice pitch is always its row pitch times its rows, so padding rows between depth slices need a repack
zest_bool zest__dx_region_needs_repack(const zest_dx_region_copy_t *copy, zest_size offset, zest_bool unrestricted_copy_pitch) {
	if (copy->footprint.Footprint.Depth > 1 && copy->source_slice_pitch != copy->source_row_pitch * copy->row_count) {
		return ZEST_TRUE;
	}
	return zest__dx_copy_needs_repack(copy->source_row_pitch, offset, unrestricted_copy_pitch);
}

void zest__dx_repack_rows(const void *source, zest_size source_row_pitch, void *destination, zest_size destination_row_pitch, zest_size row_bytes, zest_uint row_count) {
	const char *source_row = (const char *)source;
	char *destination_row = (char *)destination;
	for (zest_uint row = 0; row != row_count; ++row) {
		memcpy(destination_row, source_row, row_bytes);
		source_row += source_row_pitch;
		destination_row += destination_row_pitch;
	}
}

zest_bool zest__dx_copy_bitmap_to_image(zest_device device, void *bitmap, zest_size image_size, zest_image dst_image, zest_uint width, zest_uint height) {
	if (dst_image->info.mip_levels > 1) {
		ZEST_ASSERT_OR_VALIDATE(ZEST_FALSE, device, "Mipmap generation is not available on the D3D12 backend yet.", ZEST_FALSE);
		return ZEST_FALSE;
	}
	zest_buffer staging_buffer = zest_CreateDedicatedStagingBuffer(device, image_size, bitmap);
	if (!staging_buffer) {
		return ZEST_FALSE;
	}
	zest_queue queue = zest_imm_BeginCommandBuffer(device, zest_queue_graphics);
	if (!queue) {
		zest_FreeBufferNow(staging_buffer);
		return ZEST_FALSE;
	}

	zest_buffer_image_copy_t region = ZEST__ZERO_INIT(zest_buffer_image_copy_t);
	region.image_aspect = zest_image_aspect_color_bit;
	region.layer_count = 1;
	region.image_extent.width = width;
	region.image_extent.height = height;
	region.image_extent.depth = dst_image->info.extent.depth > 1 ? dst_image->info.extent.depth : 1;
	zest_bool recorded = zest_imm_TransitionImage(queue, dst_image, zest_resource_state_copy_dst, 0, dst_image->info.mip_levels, 0, dst_image->info.layer_count) &&
		zest_imm_CopyBufferRegionsToImage(queue, &region, 1, staging_buffer, dst_image) &&
		zest_imm_TransitionImage(queue, dst_image, zest_resource_state_shader_read, 0, dst_image->info.mip_levels, 0, dst_image->info.layer_count);
	//Ending also submits a partly recorded list, which releases the queue either way
	zest_bool finished = zest_imm_EndCommandBuffer(queue);
	if (finished) {
		zest_FreeBufferNow(staging_buffer);
	} else {
		zest_FreeBuffer(staging_buffer);
	}
	return recorded && finished;
}

zest_bool zest__dx_copy_image_to_bitmap(zest_device device, zest_image src_image, void *pixels) {
	zest_dx_device_backend_t *device_backend = ZEST_DX_BACKEND(device, device);
	ID3D12Resource *texture = ZEST_DX_BACKEND(image, src_image)->resource;
	D3D12_RESOURCE_DESC1 description = zest__dx_resource_description(texture);
	D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint = ZEST__ZERO_INIT(D3D12_PLACED_SUBRESOURCE_FOOTPRINT);
	UINT row_count = 0;
	UINT64 row_size = 0;
	UINT64 total_size = 0;
	ZEST_DX_CALL(device_backend->device10, GetCopyableFootprints1, &description, 0, 1, 0, &footprint, &row_count, &row_size, &total_size);

	zest_buffer_info_t buffer_info = zest_CreateBufferInfo(zest_buffer_type_staging, zest_memory_usage_gpu_to_cpu);
	zest_buffer readback_buffer = zest_CreateBuffer(device, (zest_size)total_size + D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT, &buffer_info);
	if (!readback_buffer) {
		return ZEST_FALSE;
	}
	footprint.Offset = zloc__align_size_up(readback_buffer->memory_offset, D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT);

	zest_queue queue = zest_imm_BeginCommandBuffer(device, zest_queue_graphics);
	if (!queue) {
		zest_FreeBufferNow(readback_buffer);
		return ZEST_FALSE;
	}
	//Restored exactly, as an image uploaded on the copy queue sits in a D3D12 layout the zest layout doesn't map back to
	zest_image_layout original_layout = src_image->layout;
	D3D12_BARRIER_LAYOUT original_backend_layout = ZEST_DX_BACKEND(image, src_image)->layout;
	zest__dx_transition_image_layout(queue, src_image, zest_image_layout_transfer_src_optimal, NULL, 0, 1, 0, 1);
	D3D12_TEXTURE_COPY_LOCATION source = ZEST__ZERO_INIT(D3D12_TEXTURE_COPY_LOCATION);
	source.pResource = texture;
	source.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
	source.SubresourceIndex = 0;
	D3D12_TEXTURE_COPY_LOCATION destination = ZEST__ZERO_INIT(D3D12_TEXTURE_COPY_LOCATION);
	destination.pResource = ZEST_DX_BACKEND(device_memory_pool, readback_buffer->memory_pool)->buffer;
	destination.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
	destination.PlacedFootprint = footprint;
	ZEST_DX_CALL(ZEST_DX_BACKEND(queue, queue)->command_list, CopyTextureRegion, &destination, 0, 0, 0, &source, NULL);
	zest__dx_transition_image_layout(queue, src_image, original_layout, &original_backend_layout, 0, 1, 0, 1);
	if (!zest_imm_EndCommandBuffer(queue)) {
		zest_FreeBuffer(readback_buffer);
		return ZEST_FALSE;
	}

	zest__dx_repack_rows((const char *)readback_buffer->memory_pool->mapped + footprint.Offset, footprint.Footprint.RowPitch, pixels, (zest_size)row_size, (zest_size)row_size, row_count);
	zest_FreeBufferNow(readback_buffer);
	return ZEST_TRUE;
}
// -- End General_helpers

#endif // ZEST_DX12_H
