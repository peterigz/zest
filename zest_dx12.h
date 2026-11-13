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
#include <dxgidebug.h>

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
#define ZEST_DXC_CALL(object, method, ...) (object)->lpVtbl->method((object), __VA_ARGS__)
#define ZEST_DXC_CALL0(object, method) (object)->lpVtbl->method(object)
#define ZEST_DXC_RELEASE(object) do { if (object) { ZEST_DXC_CALL0((object), Release); (object) = NULL; } } while (0)

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

#define ZEST__DX_DEFINE_CLSID(name, data1, data2, data3, byte0, byte1, byte2, byte3, byte4, byte5, byte6, byte7) \
	static const GUID zest__dx_clsid_##name = { data1, data2, data3, { byte0, byte1, byte2, byte3, byte4, byte5, byte6, byte7 } }

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
ZEST__DX_DEFINE_IID(IDXGISwapChain1, 0x790a45f7, 0x0d42, 0x4876, 0x98, 0x3a, 0x0a, 0x55, 0xcf, 0xe6, 0xf4, 0xaa);
ZEST__DX_DEFINE_IID(IDXGISwapChain3, 0x94d99bdb, 0xf1f8, 0x4ab0, 0xb2, 0x36, 0x7d, 0xa0, 0x17, 0x0e, 0xda, 0xb1);
ZEST__DX_DEFINE_IID(IDXGIInfoQueue, 0xd67441c7, 0x672a, 0x476f, 0x9e, 0x82, 0xcd, 0x55, 0xb4, 0x49, 0x49, 0xce);
ZEST__DX_DEFINE_IID(ID3D12Device5, 0x8b4f173b, 0x2fea, 0x4b80, 0x8f, 0x58, 0x43, 0x07, 0x19, 0x1a, 0xb9, 0x5d);
ZEST__DX_DEFINE_IID(ID3D12RootSignature, 0xc54a6b66, 0x72df, 0x4ee8, 0x8b, 0xe5, 0xa9, 0x46, 0xa1, 0x42, 0x92, 0x14);
ZEST__DX_DEFINE_IID(ID3D12PipelineState, 0x765a30f3, 0xf624, 0x4c6f, 0xa8, 0x28, 0xac, 0xe9, 0x48, 0x62, 0x24, 0x45);
ZEST__DX_DEFINE_IID(IDxcCompiler3, 0x228b4687, 0x5a6a, 0x4730, 0x90, 0x0c, 0x97, 0x02, 0xb2, 0x20, 0x3f, 0x54);
ZEST__DX_DEFINE_IID(IDxcResult, 0x58346cda, 0xdde7, 0x4497, 0x94, 0x61, 0x6f, 0x87, 0xaf, 0x5e, 0x06, 0x59);
ZEST__DX_DEFINE_CLSID(DxcCompiler, 0x73e22d93, 0xe6ce, 0x47f3, 0xb5, 0xbf, 0xf0, 0x66, 0x4f, 0x39, 0xc1, 0xb0);
//DXGI_DEBUG_DXGI, which the headers only declare
static const GUID zest__dx_debug_dxgi = { 0x25cddaa4, 0xb1c6, 0x47e1, { 0xac, 0x3e, 0x98, 0x87, 0x5b, 0x5a, 0x2e, 0x2a } };
// -- End Interface IDs

// -- Shader_code
//Variadic so that commas outside brackets stay in the shader. New lines are lost, so no preprocessor lines or line comments.
#define ZEST_HLSL(...) #__VA_ARGS__

//A full screen triangle that samples one slice of the source, with the point sampler when filter is 0
static const char *zest__dx_blit_shader = ZEST_HLSL(
cbuffer zest_blit_constants : register(b0) {
	float4 source_rect;
	uint filter;
	uint3 padding;
};
Texture2DArray source : register(t0);
SamplerState linear_sampler : register(s0);
SamplerState point_sampler : register(s1);

struct zest_blit_varyings {
	float4 position : SV_Position;
	float2 uv : TEXCOORD0;
};

zest_blit_varyings vertex_main(uint vertex_id : SV_VertexID) {
	zest_blit_varyings output;
	float2 corner = float2((vertex_id << 1) & 2, vertex_id & 2);
	output.position = float4(corner * 2.0 - 1.0, 0.0, 1.0);
	output.uv = corner;
	return output;
}

float4 pixel_main(zest_blit_varyings input) : SV_Target {
	float3 location = float3(source_rect.xy + input.uv * source_rect.zw, 0.0);
	if (filter == 0) {
		return source.SampleLevel(point_sampler, location, 0.0);
	}
	return source.SampleLevel(linear_sampler, location, 0.0);
}
);
// -- End Shader_code

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
	//Zeroed with the device backend, which is SRWLOCK_INIT, and never reinitialised so a reset can't clear it while it's held
	SRWLOCK lock;
} zest_dx_cpu_descriptor_heap_t;

typedef struct zest_dxc_compiler3_t zest_dxc_compiler3_t;

//Indexed by DXGI_FORMAT
#define ZEST_DX_BLIT_FORMAT_COUNT 192
#define ZEST_DX_TRANSIENT_DESCRIPTOR_COUNT 8192
#define ZEST_DX_TRANSIENT_DESCRIPTOR_CHUNK 64

//Chunks are indexes into the transient region of the bindless resource heap
typedef struct zest_dx_transient_descriptors_t {
	zest_uint *chunks;
	zest_uint chunk_cursor;
	zest_uint slot_cursor;
} zest_dx_transient_descriptors_t;

typedef struct zest_dx_blit_constants_t {
	float source_rect[4];
	zest_uint filter;
	zest_uint padding[3];
} zest_dx_blit_constants_t;

typedef struct zest_dx_device_backend_t {
	IDXGIFactory6 *factory;
	IDXGIAdapter4 *adapter;
	ID3D12Device *device;
	ID3D12Device10 *device10;
	ID3D12InfoQueue1 *info_queue;
	DWORD info_queue_cookie;
	//Process wide and with no callback, so its messages are drained after swapchain calls
	IDXGIInfoQueue *dxgi_info_queue;
	zest_dx_feature_support_t support;
	zest_dx_cpu_descriptor_heap_t cpu_resource_heap;
	zest_dx_cpu_descriptor_heap_t cpu_sampler_heap;
	zest_dx_cpu_descriptor_heap_t cpu_rtv_heap;
	zest_dx_cpu_descriptor_heap_t cpu_dsv_heap;
	HMODULE dxc_module;
	HMODULE dxil_module;
	zest_dxc_compiler3_t *dxc_compiler;
	ID3D12RootSignature *utility_root_signature;
	//DXIL is independent of the ID3D12Device, so the blobs outlive a device reset while the pipelines don't
	ID3DBlob *blit_vertex_shader;
	ID3DBlob *blit_pixel_shader;
	ID3D12PipelineState *blit_pipelines[ZEST_DX_BLIT_FORMAT_COUNT];
	//Guard the lazily created objects above, and blit_lock is held while taking either of the others but never the reverse
	SRWLOCK dxc_lock;
	SRWLOCK root_signature_lock;
	SRWLOCK blit_lock;
	HRESULT last_result;
	zest_bool counted_as_live;
} zest_dx_device_backend_t;

typedef struct zest_dx_queue_backend_t {
	ID3D12CommandQueue *queue;
	ID3D12CommandAllocator *command_allocator;
	ID3D12GraphicsCommandList7 *command_list;
	D3D12_COMMAND_LIST_TYPE type;
	zest_buffer *pending_staging_buffers;
	zest_dx_transient_descriptors_t transient_descriptors;
	//The last immediate list was executed but its fence signal failed, so nothing yet says when it finishes
	zest_bool unfenced_submission;
} zest_dx_queue_backend_t;

//The swapchain lives here because shared code frees the swapchain struct and its backend on every resize
typedef struct zest_dx_context_backend_t {
	HWND window;
	IDXGISwapChain3 *swapchain;
	HANDLE frame_latency_waitable;
	ID3D12CommandQueue *present_queue;
	UINT swapchain_flags;
	ID3D12CommandAllocator *utility_allocators[ZEST_MAX_FIF];
	ID3D12GraphicsCommandList7 *utility_command_list;
	//The D3D12 queue that last signalled each slot's ordering timeline, as back buffer work runs on the swapchain's queue whatever the slot holds
	ID3D12CommandQueue *ordering_queues[ZEST_QUEUE_COUNT];
	HRESULT last_result;
} zest_dx_context_backend_t;

typedef struct zest_dx_swapchain_backend_t {
	DXGI_FORMAT buffer_format;
	DXGI_FORMAT view_format;
} zest_dx_swapchain_backend_t;

//Batches of a context queue are recorded one after another, and a list can be reset once it's executed, so one list serves them all
typedef struct zest_dx_context_queue_backend_t {
	ID3D12CommandAllocator *command_allocators[ZEST_MAX_FIF];
	ID3D12GraphicsCommandList7 *command_list;
	zest_dx_transient_descriptors_t transient_descriptors[ZEST_MAX_FIF];
} zest_dx_context_queue_backend_t;

typedef struct zest_dx_execution_timeline_backend_t {
	ID3D12Fence *fence;
} zest_dx_execution_timeline_backend_t;

typedef struct zest_dx_command_list_backend_t {
	ID3D12GraphicsCommandList7 *command_list;
	zest_context_queue context_queue;
	zest_device_queue_type queue_type;
	zest_descriptor_set *bound_sets;
	zest_uint bound_set_count;
	zest_pipeline_bind_point bound_point;
	zest_bool render_pass_open;
	HRESULT close_result;
} zest_dx_command_list_backend_t;

//Recorded when the graph is compiled, before the queue type or the image's current D3D12 layout is known
typedef struct zest_dx_frame_graph_image_barrier_t {
	zest_resource_node resource;
	zest_access_flags src_access;
	zest_access_flags dst_access;
	zest_image_layout old_layout;
	zest_image_layout new_layout;
	zest_pipeline_stage_flags src_stage;
	zest_pipeline_stage_flags dst_stage;
	zest_uint src_family;
	zest_uint dst_family;
} zest_dx_frame_graph_image_barrier_t;

//The translated arrays always hold one entry per record, so executing a cached graph only fills them in
typedef struct zest_dx_execution_barriers_backend_t {
	zest_dx_frame_graph_image_barrier_t *acquire_images;
	zest_dx_frame_graph_image_barrier_t *release_images;
	D3D12_TEXTURE_BARRIER *acquire_translated;
	D3D12_TEXTURE_BARRIER *release_translated;
} zest_dx_execution_barriers_backend_t;

//The fence values are in the shared values[fif][queue]
typedef struct zest_dx_frame_graph_semaphores_backend_t {
	ID3D12Fence *fences[ZEST_MAX_FIF][ZEST_QUEUE_COUNT];
} zest_dx_frame_graph_semaphores_backend_t;

//The fences and values the next wave's batches wait on
typedef struct zest_dx_execution_backend_t {
	ID3D12Fence *wave_wait_fences[ZEST_QUEUE_COUNT];
	zest_u64 wave_wait_values[ZEST_QUEUE_COUNT];
	zest_uint wave_wait_count;
} zest_dx_execution_backend_t;

typedef struct zest_dx_submission_batch_backend_t {
	ID3D12Fence *signal_fence;
} zest_dx_submission_batch_backend_t;

//Heap categories carried in backend_memory_bits, where zero is a plain buffer
#define ZEST_DX_HEAP_CATEGORY_TEXTURES 1
#define ZEST_DX_HEAP_CATEGORY_TARGETS 2
#define ZEST_DX_HEAP_CATEGORY_MSAA 3

#define ZEST_DX_FILL_BLOCK_SIZE 65536

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
	D3D12_RESOURCE_FLAGS resource_flags;
} zest_dx_image_backend_t;

typedef struct zest_dx_image_view_backend_t {
	D3D12_CPU_DESCRIPTOR_HANDLE srv;
	D3D12_CPU_DESCRIPTOR_HANDLE uav;
	D3D12_CPU_DESCRIPTOR_HANDLE rtv;
	D3D12_CPU_DESCRIPTOR_HANDLE dsv;
} zest_dx_image_view_backend_t;

typedef struct zest_dx_sampler_backend_t {
	D3D12_CPU_DESCRIPTOR_HANDLE descriptor;
} zest_dx_sampler_backend_t;

//Offsets into the shader visible heaps, where slot 0 is the sampler heap and every other slot is in the resource heap
typedef struct zest_dx_bindless_regions_t {
	zest_uint resource_offsets[zest_max_global_binding_number];
	zest_uint storage_buffer_uav_offset;
	zest_uint transient_offset;
	zest_uint transient_count;
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
	D3D12_GPU_DESCRIPTOR_HANDLE resource_gpu_start;
	D3D12_CPU_DESCRIPTOR_HANDLE sampler_cpu_start;
	zest_uint *free_transient_chunks;
	SRWLOCK chunk_lock;
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
#define ZEST__DX_SWAPCHAIN_BUFFER_COUNT (ZEST_MAX_FIF + 1)

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

//DXC is declared here because dxcapi.h is C++ only
typedef struct zest_dxc_buffer_t {
	const void *Ptr;
	SIZE_T Size;
	UINT Encoding;
} zest_dxc_buffer_t;

typedef struct zest_dxc_compiler3_vtable_t {
	HRESULT (STDMETHODCALLTYPE *QueryInterface)(zest_dxc_compiler3_t *self, const IID *riid, void **object);
	ULONG (STDMETHODCALLTYPE *AddRef)(zest_dxc_compiler3_t *self);
	ULONG (STDMETHODCALLTYPE *Release)(zest_dxc_compiler3_t *self);
	HRESULT (STDMETHODCALLTYPE *Compile)(zest_dxc_compiler3_t *self, const zest_dxc_buffer_t *source, LPCWSTR *arguments, UINT32 argument_count, void *include_handler, const IID *riid, void **result);
	HRESULT (STDMETHODCALLTYPE *Disassemble)(zest_dxc_compiler3_t *self, const zest_dxc_buffer_t *object, const IID *riid, void **result);
} zest_dxc_compiler3_vtable_t;

struct zest_dxc_compiler3_t {
	const zest_dxc_compiler3_vtable_t *lpVtbl;
};

typedef struct zest_dxc_result_t zest_dxc_result_t;

typedef struct zest_dxc_result_vtable_t {
	HRESULT (STDMETHODCALLTYPE *QueryInterface)(zest_dxc_result_t *self, const IID *riid, void **object);
	ULONG (STDMETHODCALLTYPE *AddRef)(zest_dxc_result_t *self);
	ULONG (STDMETHODCALLTYPE *Release)(zest_dxc_result_t *self);
	HRESULT (STDMETHODCALLTYPE *GetStatus)(zest_dxc_result_t *self, HRESULT *status);
	HRESULT (STDMETHODCALLTYPE *GetResult)(zest_dxc_result_t *self, ID3DBlob **result);
	HRESULT (STDMETHODCALLTYPE *GetErrorBuffer)(zest_dxc_result_t *self, ID3DBlob **errors);
} zest_dxc_result_vtable_t;

struct zest_dxc_result_t {
	const zest_dxc_result_vtable_t *lpVtbl;
};

typedef HRESULT (STDMETHODCALLTYPE *zest_dxc_create_instance_function)(const GUID *class_id, const IID *riid, void **object);

#define ZEST__DX_DXC_UTF8 65001
#define ZEST__DX_DXC_ARGUMENT_LENGTH 32

static const char *zest__dx_forced_dxc_library = NULL;
static const char *zest__dx_forced_missing_feature = NULL;
static zest_bool zest__dx_forced_restricted_copy_pitch = ZEST_FALSE;
static zest_bool zest__dx_forced_feature_level_failure = ZEST_FALSE;
static zest_bool zest__dx_forced_debug_layer_unavailable = ZEST_FALSE;
static zest_bool zest__dx_forced_signal_failure = ZEST_FALSE;
static zest_bool zest__dx_forced_uav_clear = ZEST_FALSE;
static zest_uint zest__dx_device_release_count = 0;
//Enabling the debug layer removes every device created before it, so enabling it and counting a new device share a lock
static SRWLOCK zest__dx_debug_layer_lock = SRWLOCK_INIT;
static volatile LONG zest__dx_live_device_count = 0;
static zest_bool zest__dx_debug_layer_enabled = ZEST_FALSE;
//Per thread so concurrent waiters on one fence can't take each other's wake
static __declspec(thread) HANDLE zest__dx_wait_event = NULL;
// -- End Other_setup_structs

ZEST_API ID3D12Device *zest_GetD3D12Device(zest_device device);
ZEST_API ID3D12GraphicsCommandList7 *zest_GetD3D12CommandList(zest_command_list command_list);
//The direct queue the context's swapchain presents on, or NULL for a headless context
ZEST_API ID3D12CommandQueue *zest_GetD3D12CommandQueue(zest_context context);

ZEST_PRIVATE void zest__dx_initialise_platform_callbacks(zest_platform_t *platform);
ZEST_PRIVATE void zest__dx_log_hresult(zest_device device, HRESULT result, const char *file, int line);
ZEST_PRIVATE void zest__dx_record_validation_message(zest_device device, const char *severity_name, zest_bool counted, zest_key key, int id, const char *description);
ZEST_PRIVATE void zest__dx_drain_dxgi_messages(zest_device device);
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
ZEST_PRIVATE zest_bool zest__dx_is_integer_format(DXGI_FORMAT format);
ZEST_PRIVATE zest_bool zest__dx_is_signed_integer_format(DXGI_FORMAT format);
ZEST_PRIVATE zest_uint zest__dx_image_heap_category(D3D12_RESOURCE_FLAGS resource_flags, zest_uint sample_count, D3D12_RESOURCE_HEAP_TIER tier);
ZEST_PRIVATE UINT zest__dx_component_mapping(zest_component_mapping_t mapping);
ZEST_PRIVATE D3D12_BARRIER_LAYOUT zest__dx_depth_read_layout(zest_access_flags access, zest_device_queue_type queue_type);
ZEST_PRIVATE D3D12_BARRIER_LAYOUT zest__dx_barrier_layout(zest_image_layout layout, zest_access_flags access, zest_device_queue_type queue_type);
ZEST_PRIVATE D3D12_BARRIER_ACCESS zest__dx_barrier_access(zest_access_flags access, zest_bool is_image, D3D12_BARRIER_LAYOUT layout);
ZEST_PRIVATE D3D12_BARRIER_SYNC zest__dx_barrier_sync(zest_pipeline_stage_flags stages, zest_bool is_before);
ZEST_PRIVATE D3D12_BARRIER_SYNC zest__dx_normalise_sync(D3D12_BARRIER_SYNC sync, D3D12_BARRIER_ACCESS access);
ZEST_PRIVATE zest_image_layout zest__dx_image_layout_from_resource_state(zest_resource_state state, zest_bool is_depth);
ZEST_PRIVATE void zest__dx_layout_access_and_stage(zest_image_layout layout, zest_device_queue_type queue_type, zest_access_flags *access_out, zest_pipeline_stage_flags *stage_out);
ZEST_PRIVATE zest_device_queue_type zest__dx_queue_type(zest_queue queue);
ZEST_PRIVATE zest_device_queue_type zest__dx_family_queue_type(zest_uint family_index);
ZEST_PRIVATE zest_uint zest__dx_plane_count(zest_format format);
ZEST_PRIVATE D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE zest__dx_beginning_access(zest_load_op load_op);
ZEST_PRIVATE D3D12_RENDER_PASS_ENDING_ACCESS_TYPE zest__dx_ending_access(zest_store_op store_op);
ZEST_PRIVATE D3D12_VIEWPORT zest__dx_viewport(float x, float y, float width, float height, float min_depth, float max_depth);
ZEST_PRIVATE zest_bool zest__dx_swapchain_formats(zest_format requested, DXGI_FORMAT *buffer_format, DXGI_FORMAT *view_format, zest_format *reported);
ZEST_PRIVATE void zest__dx_present_parameters(zest_bool vsync, zest_bool tearing_supported, UINT *sync_interval, UINT *flags);

//Swapchain
ZEST_PRIVATE void zest__dx_framebuffer_size(zest_context context, zest_extent2d_t *framebuffer, int *window_width, int *window_height);
ZEST_PRIVATE zest_bool zest__dx_create_swapchain_images(zest_context context);
ZEST_PRIVATE zest_bool zest__dx_create_window_surface(zest_context context);
ZEST_PRIVATE void *zest__dx_new_swapchain_backend(zest_context context);
ZEST_PRIVATE zest_bool zest__dx_initialise_swapchain(zest_context context);
ZEST_PRIVATE void zest__dx_cleanup_swapchain_backend(zest_swapchain swapchain);
ZEST_PRIVATE void zest__dx_destroy_context_surface(zest_context context);
ZEST_PRIVATE zest_bool zest__dx_acquire_swapchain_image(zest_swapchain swapchain);
ZEST_PRIVATE zest_bool zest__dx_present_frame(zest_context context, zest_context_queue present_queue);
ZEST_PRIVATE zest_bool zest__dx_dummy_submit_for_present_only(zest_context context);
ZEST_PRIVATE zest_semaphore_status zest__dx_wait_for_renderer_semaphore(zest_context context);
ZEST_PRIVATE void zest__dx_wait_for_fif_semaphore(zest_context context, zest_uint fif);

//CPU descriptor heaps
ZEST_PRIVATE zest_bool zest__dx_initialise_cpu_descriptor_heap(zest_device device, zest_dx_cpu_descriptor_heap_t *heap, D3D12_DESCRIPTOR_HEAP_TYPE type);
ZEST_PRIVATE zest_bool zest__dx_add_cpu_descriptor_block(zest_device device, zest_dx_cpu_descriptor_heap_t *heap);
ZEST_PRIVATE D3D12_CPU_DESCRIPTOR_HANDLE zest__dx_allocate_cpu_descriptor(zest_device device, zest_dx_cpu_descriptor_heap_t *heap);
ZEST_PRIVATE void zest__dx_free_cpu_descriptor(zest_device device, zest_dx_cpu_descriptor_heap_t *heap, D3D12_CPU_DESCRIPTOR_HANDLE handle);
ZEST_PRIVATE void zest__dx_cleanup_cpu_descriptor_heap(zest_device device, zest_dx_cpu_descriptor_heap_t *heap);

//Bindless descriptor heaps
ZEST_PRIVATE void zest__dx_compute_bindless_regions(const zest_uint binding_counts[zest_max_global_binding_number], zest_dx_bindless_regions_t *regions);
ZEST_PRIVATE void zest__dx_bind_bindless_heaps(zest_device device, ID3D12GraphicsCommandList7 *command_list);
ZEST_PRIVATE zest_bool zest__dx_allocate_transient_descriptor(zest_device device, zest_dx_transient_descriptors_t *descriptors, D3D12_CPU_DESCRIPTOR_HANDLE *cpu, D3D12_GPU_DESCRIPTOR_HANDLE *gpu);
ZEST_PRIVATE void zest__dx_reset_transient_descriptors(zest_dx_transient_descriptors_t *descriptors);
ZEST_PRIVATE void zest__dx_release_transient_descriptors(zest_device device, zest_dx_transient_descriptors_t *descriptors);
ZEST_PRIVATE void zest__dx_free_transient_descriptors(zest_device device, zest_dx_transient_descriptors_t *descriptors);

//Images
ZEST_PRIVATE D3D12_FEATURE_DATA_FORMAT_SUPPORT zest__dx_format_support(ID3D12Device *native_device, DXGI_FORMAT format);
//Whether the support bits allow every use in flags, leaving a depth format's shader loads to its view format's own check
ZEST_PRIVATE zest_bool zest__dx_format_support_allows(const D3D12_FEATURE_DATA_FORMAT_SUPPORT *support, zest_image_flags flags, zest_bool check_shader_load);
ZEST_PRIVATE D3D12_RESOURCE_FLAGS zest__dx_image_resource_flags(zest_device device, zest_image_flags flags, DXGI_FORMAT format, zest_uint mip_levels);
ZEST_PRIVATE zest_bool zest__dx_image_description(zest_device device, zest_image image, zest_uint layer_count, zest_uint sample_count, zest_image_flags flags, D3D12_RESOURCE_DESC1 *description);
ZEST_PRIVATE void zest__dx_texture_description(zest_image image, zest_uint layer_count, zest_uint sample_count, DXGI_FORMAT resource_format, D3D12_RESOURCE_FLAGS resource_flags, D3D12_RESOURCE_DESC1 *description);
ZEST_PRIVATE D3D12_SHADER_RESOURCE_VIEW_DESC zest__dx_image_srv_description(zest_image image, zest_image_view_type view_type, zest_uint mip_levels_this_view, zest_uint base_mip, zest_uint base_array_index, zest_uint layer_count);
ZEST_PRIVATE D3D12_UNORDERED_ACCESS_VIEW_DESC zest__dx_image_uav_description(zest_image image, zest_image_view_type view_type, zest_uint base_mip, zest_uint base_array_index, zest_uint layer_count);
ZEST_PRIVATE D3D12_RENDER_TARGET_VIEW_DESC zest__dx_image_rtv_description(zest_image image, zest_image_view_type view_type, zest_uint base_mip, zest_uint base_array_index, zest_uint layer_count);
ZEST_PRIVATE D3D12_DEPTH_STENCIL_VIEW_DESC zest__dx_image_dsv_description(zest_image image, zest_image_view_type view_type, zest_uint base_mip, zest_uint base_array_index, zest_uint layer_count);
ZEST_PRIVATE zest_bool zest__dx_create_view_descriptors(zest_device device, zest_image image, zest_image_view_type view_type, zest_uint mip_levels_this_view, zest_uint base_mip, zest_uint base_array_index, zest_uint layer_count, zest_dx_image_view_backend_t *backend);
ZEST_PRIVATE void zest__dx_free_view_descriptors(zest_device device, zest_dx_image_view_backend_t *backend);
ZEST_PRIVATE zest_bool zest__dx_transition_image_layout(zest_queue queue, zest_image image, zest_image_layout new_layout, const D3D12_BARRIER_LAYOUT *exact_layout_after, zest_uint base_mip_index, zest_uint mip_levels, zest_uint base_array_index, zest_uint layer_count);
ZEST_PRIVATE D3D12_BARRIER_ACCESS zest__dx_common_layout_access(D3D12_BARRIER_LAYOUT layout, D3D12_BARRIER_ACCESS access);
ZEST_PRIVATE D3D12_TEXTURE_BARRIER zest__dx_image_barrier(zest_image image, D3D12_BARRIER_LAYOUT layout_after, zest_access_flags src_access, zest_access_flags dst_access, zest_pipeline_stage_flags src_stage, zest_pipeline_stage_flags dst_stage, zest_uint base_mip, zest_uint mip_count, zest_uint base_layer, zest_uint layer_count);
ZEST_PRIVATE D3D12_TEXTURE_BARRIER zest__dx_subresource_barrier(zest_image image, D3D12_BARRIER_LAYOUT layout_before, D3D12_BARRIER_LAYOUT layout_after, zest_access_flags src_access, zest_access_flags dst_access, zest_pipeline_stage_flags src_stage, zest_pipeline_stage_flags dst_stage, zest_uint base_mip, zest_uint mip_count, zest_uint base_layer, zest_uint layer_count);
ZEST_PRIVATE void zest__dx_send_texture_barriers(ID3D12GraphicsCommandList7 *list, D3D12_TEXTURE_BARRIER *barriers, zest_uint barrier_count);
ZEST_PRIVATE void zest__dx_send_changing_barriers(ID3D12GraphicsCommandList7 *list, D3D12_TEXTURE_BARRIER *barriers, zest_uint barrier_count);
ZEST_PRIVATE void zest__dx_discard_subresource(ID3D12GraphicsCommandList7 *list, ID3D12Resource *resource, zest_uint subresource);
ZEST_PRIVATE D3D12_TEXTURE_BARRIER zest__dx_command_barrier(zest_image image, zest_access_flags tracked_access, zest_pipeline_stage_flags tracked_stage, D3D12_BARRIER_LAYOUT command_layout, D3D12_BARRIER_ACCESS command_access, D3D12_BARRIER_SYNC command_sync, zest_bool to_command, zest_uint mip_count, zest_uint layer_count);
ZEST_PRIVATE D3D12_BARRIER_LAYOUT zest__dx_color_clear_layout(zest_device device, zest_image image, zest_device_queue_type queue_type);
ZEST_PRIVATE zest_bool zest__dx_depth_clear_is_valid(zest_device device, zest_image image, zest_device_queue_type queue_type);
ZEST_PRIVATE zest_bool zest__dx_record_clear(zest_device device, ID3D12GraphicsCommandList7 *list, zest_dx_transient_descriptors_t *descriptors, zest_image image, D3D12_BARRIER_LAYOUT clear_layout, zest_access_flags tracked_access, zest_pipeline_stage_flags tracked_stage, zest_clear_value_t clear_value, zest_uint mip_count, zest_uint layer_count);
ZEST_PRIVATE zest_bool zest__dx_read_subresource(zest_device device, zest_image image, zest_uint mip, zest_uint layer, void *pixels);
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
ZEST_PRIVATE void zest__dx_setup_dxgi_info_queue(zest_device device);
ZEST_PRIVATE zest_bool zest__dx_create_device_objects(zest_device device);
ZEST_PRIVATE void zest__dx_release_device_objects(zest_device device);
ZEST_PRIVATE zest_bool zest__dx_create_queues(zest_device device);
ZEST_PRIVATE void zest__dx_add_queue_manager_list(zest_device device, zest_device_queue_type queue_type, const zest_uint *family_order, zest_uint order_count);
ZEST_PRIVATE void zest__dx_cleanup_queues(zest_device device);
ZEST_PRIVATE zest_uint zest__dx_release_native_device(zest_device device, ID3D12Device *native_device);
ZEST_PRIVATE zest_uint zest__dx_last_device_release_count(void);
ZEST_PRIVATE DWORD zest__dx_timeout_milliseconds(zest_microsecs timeout);
ZEST_PRIVATE void zest__dx_release_immediate_queue(zest_queue queue, zest_bool gpu_finished);
ZEST_PRIVATE void zest__dx_flag_device_lost(zest_context context, HRESULT result);

//Platform callbacks
ZEST_PRIVATE void *zest__dx_new_device_backend(zest_device device);
ZEST_PRIVATE zest_bool zest__dx_initialise_device(zest_device device);
ZEST_PRIVATE zest_bool zest__dx_reinit_logical_device(zest_device device);
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
ZEST_PRIVATE void *zest__dx_new_frame_graph_semaphores_backend(zest_context context);
ZEST_PRIVATE void zest__dx_cleanup_frame_graph_semaphore(zest_context context, zest_frame_graph_semaphores semaphores);
ZEST_PRIVATE void *zest__dx_new_frame_graph_context_backend(zest_context context);
ZEST_PRIVATE void *zest__dx_new_execution_barriers_backend(zloc_linear_allocator_t *allocator);
ZEST_PRIVATE void *zest__dx_new_submission_batch_backend(zest_context context);
ZEST_PRIVATE void *zest__dx_new_execution_backend(zloc_linear_allocator_t *allocator);
ZEST_PRIVATE zest_frame_graph_semaphores zest__dx_get_frame_graph_semaphores(zest_context context, const char *name);
ZEST_PRIVATE void zest__dx_add_frame_graph_image_barrier(zest_resource_node resource, zest_execution_barriers_t *barriers, zest_bool acquire, zest_access_flags src_access, zest_access_flags dst_access, zest_image_layout old_layout, zest_image_layout new_layout, zest_uint src_family, zest_uint dst_family, zest_pipeline_stage_flags src_stage, zest_pipeline_stage_flags dst_stage);
ZEST_PRIVATE void zest__dx_add_frame_graph_buffer_barrier(zest_resource_node resource, zest_execution_barriers_t *barriers, zest_bool acquire, zest_access_flags src_access, zest_access_flags dst_access, zest_uint src_family, zest_uint dst_family, zest_pipeline_stage_flags src_stage, zest_pipeline_stage_flags dst_stage);
ZEST_PRIVATE zest_bool zest__dx_set_next_command_buffer(const zest_command_list command_list, zest_context_queue queue);
ZEST_PRIVATE zest_bool zest__dx_begin_command_buffer(const zest_command_list command_list);
ZEST_PRIVATE void zest__dx_end_command_buffer(const zest_command_list command_list);
ZEST_PRIVATE D3D12_BARRIER_LAYOUT zest__dx_record_layout_before(const zest_dx_frame_graph_image_barrier_t *record, zest_image image);
ZEST_PRIVATE void zest__dx_send_image_barriers(const zest_command_list command_list, const zest_dx_frame_graph_image_barrier_t *records, D3D12_TEXTURE_BARRIER *translated);
ZEST_PRIVATE void zest__dx_acquire_barrier(const zest_command_list command_list, zest_execution_details_t *exe_details);
ZEST_PRIVATE void zest__dx_release_barrier(const zest_command_list command_list, zest_execution_details_t *exe_details);
ZEST_PRIVATE zest_bool zest__dx_begin_render_pass(const zest_command_list command_list, zest_execution_details_t *exe_details);
ZEST_PRIVATE void zest__dx_end_render_pass(const zest_command_list command_list);
ZEST_PRIVATE zest_bool zest__dx_submit_frame_graph_batch(zest_frame_graph frame_graph, zest_execution_backend backend, zest_submission_batch_t *batch, zest_map_queue_value *queues);
ZEST_PRIVATE void zest__dx_carry_over_semaphores(zest_frame_graph frame_graph, zest_wave_submission_t *wave_submission, zest_execution_backend backend);
ZEST_PRIVATE void zest__dx_bind_descriptor_sets(const zest_command_list command_list, zest_pipeline_bind_point bind_point, zest_pipeline_layout layout, zest_descriptor_set *descriptor_sets, zest_uint set_count, zest_uint first_set);
ZEST_PRIVATE void zest__dx_set_screensized_viewport(const zest_command_list command_list, float min_depth, float max_depth);
ZEST_PRIVATE void zest__dx_scissor(const zest_command_list command_list, zest_scissor_rect_t *scissor);
ZEST_PRIVATE void zest__dx_view_port(const zest_command_list command_list, zest_viewport_t *viewport);
ZEST_PRIVATE void zest__dx_clip(const zest_command_list command_list, float x, float y, float width, float height, float min_depth, float max_depth);
ZEST_PRIVATE void *zest__dx_get_resource_ptr(zest_resource_node resource);
ZEST_PRIVATE zest_queue zest__dx_imm_begin_command_buffer(zest_device device, zest_device_queue_type target_queue);
ZEST_PRIVATE zest_bool zest__dx_imm_end_command_buffer(zest_queue queue);
ZEST_PRIVATE zest_bool zest__dx_report_unavailable(zest_device device, const char *message);
ZEST_PRIVATE ID3D12Resource *zest__dx_buffer_resource(zest_buffer buffer);
ZEST_PRIVATE D3D12_GPU_VIRTUAL_ADDRESS zest__dx_buffer_address(zest_buffer buffer);
ZEST_PRIVATE void zest__dx_record_buffer_copy(ID3D12GraphicsCommandList7 *command_list, zest_buffer source, zest_size source_offset, zest_buffer destination, zest_size destination_offset, zest_size size);
ZEST_PRIVATE void zest__dx_order_immediate_copy(ID3D12GraphicsCommandList7 *command_list);
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
ZEST_PRIVATE zest_bool zest__dx_create_pool_buffer(zest_device device, const D3D12_HEAP_PROPERTIES *heap_properties, zest_size size, ID3D12Resource **buffer_out);
ZEST_PRIVATE zest_bool zest__dx_add_buffer_memory_pool(zest_device device, zest_context context, zest_size size, zest_buffer_allocator buffer_allocator, zest_device_memory_pool memory_pool);
ZEST_PRIVATE zest_device_memory_pool zest__dx_create_arena_backing(zest_device device, zest_context context, zest_uint category, zest_size size);
ZEST_PRIVATE void zest__dx_free_arena_backing(zest_device device, zest_context context, zest_device_memory_pool backing);
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
ZEST_PRIVATE void zest__dx_cleanup_image_view_array_backend(zest_image_view_array view_array);
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
ZEST_PRIVATE void zest__dx_update_bindless_storage_buffer_descriptor(zest_device device, zest_uint binding_number, zest_uint array_index, zest_buffer buffer, zest_descriptor_set set);
ZEST_PRIVATE void zest__dx_update_bindless_uniform_buffer_descriptor(zest_device device, zest_uint binding_number, zest_uint array_index, zest_buffer buffer, zest_descriptor_set set);
ZEST_PRIVATE void *zest__dx_new_pipeline_backend(zest_context context);
ZEST_PRIVATE zest_bool zest__dx_build_pipeline(zest_pipeline pipeline, zest_command_list command_list);
ZEST_PRIVATE void zest__dx_cleanup_pipeline_backend(zest_pipeline pipeline);
ZEST_PRIVATE ID3D12RootSignature *zest__dx_utility_root_signature(zest_device device);
ZEST_PRIVATE ID3D12RootSignature *zest__dx_create_utility_root_signature(zest_device device);
ZEST_PRIVATE ID3D12PipelineState *zest__dx_create_utility_pipeline(zest_device device, ID3DBlob *vertex_shader, ID3DBlob *pixel_shader, DXGI_FORMAT render_target_format, D3D12_CULL_MODE cull_mode, BOOL front_counter_clockwise);
ZEST_PRIVATE BOOL zest__dx_front_counter_clockwise(zest_front_face front_face);
ZEST_PRIVATE ID3D12PipelineState *zest__dx_blit_pipeline(zest_device device, DXGI_FORMAT format);
ZEST_PRIVATE zest_bool zest__dx_blit_images_are_valid(zest_device device, zest_image source, zest_image destination);
ZEST_PRIVATE zest_bool zest__dx_record_blit(zest_device device, ID3D12GraphicsCommandList7 *list, zest_dx_transient_descriptors_t *descriptors, zest_image source, zest_uint source_mip, zest_uint source_layer, const float source_rect[4], zest_image destination, zest_uint destination_mip, zest_uint destination_layer, const D3D12_VIEWPORT *viewport, const D3D12_RECT *scissor, zest_uint filter);
ZEST_PRIVATE void *zest__dx_new_compute_backend(zest_device device);
ZEST_PRIVATE zest_bool zest__dx_finish_compute(zest_device device, zest_compute compute);
ZEST_PRIVATE void zest__dx_cleanup_compute_backend(zest_compute compute);
ZEST_PRIVATE zest_shader_handle zest__dx_get_db_overlay_vertex_shader(zest_device device);
ZEST_PRIVATE zest_shader_handle zest__dx_get_db_overlay_fragment_shader(zest_device device);
ZEST_PRIVATE void zest__dx_create_test_render_pass(zest_device device, const zest_command_list_t *command_list);
ZEST_PRIVATE zest_image_view_array zest__dx_create_image_views_per_mip(zest_device device, zest_image image, zest_image_view_type view_type, zest_uint base_array_index, zest_uint layer_count, zloc_linear_allocator_t *linear_allocator);
ZEST_PRIVATE void *zest__dx_new_frame_graph_image_backend(zest_device device, zloc_linear_allocator_t *allocator, zest_image image, zest_image imported_image);
ZEST_PRIVATE zest_bool zest__dx_create_transient_image_unbound(zest_device device, zest_context context, zest_image image, zest_uint layer_count, zest_sample_count_flags num_samples, zest_image_flags flags, zest_transient_memory_info_t *info);
ZEST_PRIVATE zest_bool zest__dx_bind_image_arena(zest_device device, zest_image image, zest_device_memory_pool backing, zest_size offset);
ZEST_PRIVATE zest_sample_count_flags zest__dx_get_msaa_sample_count(zest_context context);
ZEST_PRIVATE D3D12_TEXTURE_BARRIER zest__dx_node_mip_barrier(zest_resource_node node, zest_uint mip, D3D12_BARRIER_LAYOUT command_layout, zest_access_flags command_access, zest_pipeline_stage_flags command_stage, zest_bool to_command);
ZEST_PRIVATE void zest__dx_record_mip_copy(const zest_command_list command_list, zest_resource_node src, zest_resource_node dst, zest_uint mip);
ZEST_PRIVATE void zest__dx_blit_image_mip(const zest_command_list command_list, zest_resource_node src, zest_resource_node dst, zest_uint mip_to_blit, zest_supported_shader_stages read_by_stages);
ZEST_PRIVATE void zest__dx_copy_image_mip(const zest_command_list command_list, zest_resource_node src, zest_resource_node dst, zest_uint mip_to_copy, zest_supported_shader_stages read_by_stages);
ZEST_PRIVATE void zest__dx_insert_compute_image_barrier(const zest_command_list command_list, zest_resource_node resource, zest_uint base_mip);
ZEST_PRIVATE void zest__dx_bind_pipeline(const zest_command_list command_list, zest_pipeline pipeline);
ZEST_PRIVATE void zest__dx_bind_compute_pipeline(const zest_command_list command_list, zest_compute compute);
ZEST_PRIVATE void zest__dx_copy_buffer(const zest_command_list command_list, zest_buffer staging_buffer, zest_buffer device_buffer, zest_size size);
ZEST_PRIVATE zest_bool zest__dx_upload_buffer(const zest_command_list command_list, zest_buffer_uploader_t *uploader);
ZEST_PRIVATE void zest__dx_bind_vertex_buffer(const zest_command_list command_list, zest_uint first_binding, zest_uint binding_count, zest_buffer buffer);
ZEST_PRIVATE void zest__dx_bind_index_buffer(const zest_command_list command_list, zest_buffer buffer);
ZEST_PRIVATE zest_bool zest__dx_image_clear(const zest_command_list command_list, zest_image image);
ZEST_PRIVATE void zest__dx_bind_mesh_vertex_buffer(const zest_command_list command_list, zest_layer layer);
ZEST_PRIVATE void zest__dx_bind_mesh_index_buffer(const zest_command_list command_list, zest_layer layer);
ZEST_PRIVATE void zest__dx_dispatch_compute(const zest_command_list command_list, zest_uint group_count_x, zest_uint group_count_y, zest_uint group_count_z);
ZEST_PRIVATE void zest__dx_send_push_constants(const zest_command_list command_list, zest_pipeline_layout layout, void *data, zest_uint size);
ZEST_PRIVATE void zest__dx_draw(const zest_command_list command_list, zest_uint vertex_count, zest_uint instance_count, zest_uint first_vertex, zest_uint first_instance);
ZEST_PRIVATE void zest__dx_draw_layer_instruction(const zest_command_list command_list, zest_uint vertex_count, zest_layer_instruction_t *instruction);
ZEST_PRIVATE void zest__dx_draw_indexed(const zest_command_list command_list, zest_uint index_count, zest_uint instance_count, zest_uint first_index, int32_t vertex_offset, zest_uint first_instance);
ZEST_PRIVATE void zest__dx_draw_indexed_indirect(const zest_command_list command_list, zest_buffer buffer, zest_size offset, zest_uint draw_count, zest_uint stride);
ZEST_PRIVATE void zest__dx_draw_indirect(const zest_command_list command_list, zest_buffer buffer, zest_size offset, zest_uint draw_count, zest_uint stride);
ZEST_PRIVATE void zest__dx_set_depth_bias(const zest_command_list command_list, float factor, float clamp, float slope);
ZEST_PRIVATE zest_bool zest__dx_validate_shader(zest_device device, const char *shader_code, zest_shader_type type, const char *name);
ZEST_PRIVATE zest_bool zest__dx_compile_shader(zest_shader shader, const char *code, zest_uint code_length, zest_shader_type type, const char *name, const char *entry_point, zest_shader_options options);
ZEST_PRIVATE zest_bool zest__dx_load_dxc(zest_device device);
ZEST_PRIVATE zest_bool zest__dx_compile_hlsl(zest_device device, const char *source, zest_size source_length, const char *name, const char *entry_point, const char *profile, ID3DBlob **dxil);
ZEST_PRIVATE zest_bool zest__dx_create_gpu_profiler_backend(zest_context context, zest_gpu_profiler_t *profiler);
ZEST_PRIVATE void *zest__dx_get_final_signal_ptr(zest_submission_batch_t *batch, zest_uint semaphore_index);
ZEST_PRIVATE void *zest__dx_get_final_wait_ptr(zest_submission_batch_t *batch, zest_uint semaphore_index);
ZEST_PRIVATE void *zest__dx_get_swapchain_wait_semaphore(zest_swapchain swapchain, zest_uint index);
ZEST_PRIVATE void *zest__dx_get_swapchain_signal_semaphore(zest_swapchain swapchain, zest_uint index);
ZEST_PRIVATE void *zest__dx_get_swapchain_identifier(zest_swapchain swapchain);

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

static const char *zest__dx_dxgi_severity_name(DXGI_INFO_QUEUE_MESSAGE_SEVERITY severity) {
	switch (severity) {
		case DXGI_INFO_QUEUE_MESSAGE_SEVERITY_CORRUPTION: return "DXGI CORRUPTION";
		case DXGI_INFO_QUEUE_MESSAGE_SEVERITY_ERROR: return "DXGI ERROR";
		case DXGI_INFO_QUEUE_MESSAGE_SEVERITY_WARNING: return "DXGI WARNING";
		case DXGI_INFO_QUEUE_MESSAGE_SEVERITY_INFO: return "DXGI INFO";
		default: return "DXGI MESSAGE";
	}
}

//Shared by the D3D12 callback and the DXGI drain, and a counted message goes into zest_GetValidationErrorCount once per key
void zest__dx_record_validation_message(zest_device device, const char *severity_name, zest_bool counted, zest_key key, int id, const char *description) {
	if (device->log_path.str) {
		ZEST_APPEND_LOG(device->log_path.str, "Validation Layer: %s", description);
	}
	if (ZEST__FLAGGED(device->init_flags, zest_device_init_flag_log_validation_errors_to_console)) {
		ZEST_ALERT("%s", severity_name);
		ZEST_PRINT("Validation Layer: %s / %i", description, id);
		ZEST_PRINT("-------------------------------------------------------");
	}
	zest_vec_foreach(index, device->validation_debug_stops) {
		if (id == device->validation_debug_stops[index]) {
			ZEST_DEBUG_STOP();
		}
	}
	if (counted && ZEST__FLAGGED(device->init_flags, zest_device_init_flag_log_validation_errors_to_memory)) {
		if (!zest_map_valid_key(device->validation_errors, key)) {
			zest_text_t error_message = ZEST__ZERO_INIT(zest_text_t);
			zest_SetText(device->allocator, &error_message, description);
			zest_map_insert_key(device->allocator, device->validation_errors, key, error_message);
		}
	}
}

void __stdcall zest__dx_debug_callback(D3D12_MESSAGE_CATEGORY category, D3D12_MESSAGE_SEVERITY severity, D3D12_MESSAGE_ID id, LPCSTR description, void *context) {
	(void)category;
	//Severities run from corruption down to message, so this keeps info and message out of zest_GetValidationErrorCount
	zest__dx_record_validation_message((zest_device)context, zest__dx_message_severity_name(severity), severity <= D3D12_MESSAGE_SEVERITY_WARNING, (zest_key)id, (int)id, description);
}

//Runs after every Present, so it returns at once when nothing is stored
void zest__dx_drain_dxgi_messages(zest_device device) {
	IDXGIInfoQueue *info_queue = ZEST_DX_BACKEND(device, device)->dxgi_info_queue;
	if (!info_queue) return;
	UINT64 message_count = ZEST_DX_CALL(info_queue, GetNumStoredMessagesAllowedByRetrievalFilters, zest__dx_debug_dxgi);
	if (!message_count) return;
	zloc_linear_allocator_t *allocator = zest__get_scratch_arena(device);
	for (UINT64 message_index = 0; message_index != message_count; ++message_index) {
		SIZE_T message_size = 0;
		if (FAILED(ZEST_DX_CALL(info_queue, GetMessage, zest__dx_debug_dxgi, message_index, NULL, &message_size))) continue;
		DXGI_INFO_QUEUE_MESSAGE *message = (DXGI_INFO_QUEUE_MESSAGE *)zloc_LinearAllocation(allocator, message_size);
		//Severities run from corruption down to message, so this drops info and message entries
		if (message && SUCCEEDED(ZEST_DX_CALL(info_queue, GetMessage, zest__dx_debug_dxgi, message_index, message, &message_size)) && message->Severity <= DXGI_INFO_QUEUE_MESSAGE_SEVERITY_WARNING) {
			//DXGI message IDs reuse the numbers of D3D12 ones, so they're counted in a key range of their own
			zest_key key = ((zest_key)1 << 32) | (zest_uint)message->ID;
			zest__dx_record_validation_message(device, zest__dx_dxgi_severity_name(message->Severity), ZEST_TRUE, key, (int)message->ID, message->pDescription);
		}
		zloc_ResetLinearAllocator(allocator);
	}
	ZEST_DX_CALL(info_queue, ClearStoredMessages, zest__dx_debug_dxgi);
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

//Formats with the same typeless family can be copied into each other, and formats with no family are their own
DXGI_FORMAT zest__dx_typeless_format(DXGI_FORMAT format) {
	switch (format) {
		case DXGI_FORMAT_R32G32B32A32_FLOAT: case DXGI_FORMAT_R32G32B32A32_UINT: case DXGI_FORMAT_R32G32B32A32_SINT: return DXGI_FORMAT_R32G32B32A32_TYPELESS;
		case DXGI_FORMAT_R32G32B32_FLOAT: case DXGI_FORMAT_R32G32B32_UINT: case DXGI_FORMAT_R32G32B32_SINT: return DXGI_FORMAT_R32G32B32_TYPELESS;
		case DXGI_FORMAT_R16G16B16A16_FLOAT: case DXGI_FORMAT_R16G16B16A16_UNORM: case DXGI_FORMAT_R16G16B16A16_UINT:
		case DXGI_FORMAT_R16G16B16A16_SNORM: case DXGI_FORMAT_R16G16B16A16_SINT: return DXGI_FORMAT_R16G16B16A16_TYPELESS;
		case DXGI_FORMAT_R32G32_FLOAT: case DXGI_FORMAT_R32G32_UINT: case DXGI_FORMAT_R32G32_SINT: return DXGI_FORMAT_R32G32_TYPELESS;
		case DXGI_FORMAT_D32_FLOAT_S8X24_UINT: return DXGI_FORMAT_R32G8X24_TYPELESS;
		case DXGI_FORMAT_R10G10B10A2_UNORM: case DXGI_FORMAT_R10G10B10A2_UINT: return DXGI_FORMAT_R10G10B10A2_TYPELESS;
		case DXGI_FORMAT_R8G8B8A8_UNORM: case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB: case DXGI_FORMAT_R8G8B8A8_UINT:
		case DXGI_FORMAT_R8G8B8A8_SNORM: case DXGI_FORMAT_R8G8B8A8_SINT: return DXGI_FORMAT_R8G8B8A8_TYPELESS;
		case DXGI_FORMAT_R16G16_FLOAT: case DXGI_FORMAT_R16G16_UNORM: case DXGI_FORMAT_R16G16_UINT:
		case DXGI_FORMAT_R16G16_SNORM: case DXGI_FORMAT_R16G16_SINT: return DXGI_FORMAT_R16G16_TYPELESS;
		case DXGI_FORMAT_D32_FLOAT: case DXGI_FORMAT_R32_FLOAT: case DXGI_FORMAT_R32_UINT: case DXGI_FORMAT_R32_SINT: return DXGI_FORMAT_R32_TYPELESS;
		case DXGI_FORMAT_D24_UNORM_S8_UINT: return DXGI_FORMAT_R24G8_TYPELESS;
		case DXGI_FORMAT_R8G8_UNORM: case DXGI_FORMAT_R8G8_UINT: case DXGI_FORMAT_R8G8_SNORM: case DXGI_FORMAT_R8G8_SINT: return DXGI_FORMAT_R8G8_TYPELESS;
		case DXGI_FORMAT_R16_FLOAT: case DXGI_FORMAT_D16_UNORM: case DXGI_FORMAT_R16_UNORM: case DXGI_FORMAT_R16_UINT:
		case DXGI_FORMAT_R16_SNORM: case DXGI_FORMAT_R16_SINT: return DXGI_FORMAT_R16_TYPELESS;
		case DXGI_FORMAT_R8_UNORM: case DXGI_FORMAT_R8_UINT: case DXGI_FORMAT_R8_SNORM: case DXGI_FORMAT_R8_SINT: return DXGI_FORMAT_R8_TYPELESS;
		case DXGI_FORMAT_BC1_UNORM: case DXGI_FORMAT_BC1_UNORM_SRGB: return DXGI_FORMAT_BC1_TYPELESS;
		case DXGI_FORMAT_BC2_UNORM: case DXGI_FORMAT_BC2_UNORM_SRGB: return DXGI_FORMAT_BC2_TYPELESS;
		case DXGI_FORMAT_BC3_UNORM: case DXGI_FORMAT_BC3_UNORM_SRGB: return DXGI_FORMAT_BC3_TYPELESS;
		case DXGI_FORMAT_BC4_UNORM: case DXGI_FORMAT_BC4_SNORM: return DXGI_FORMAT_BC4_TYPELESS;
		case DXGI_FORMAT_BC5_UNORM: case DXGI_FORMAT_BC5_SNORM: return DXGI_FORMAT_BC5_TYPELESS;
		case DXGI_FORMAT_B8G8R8A8_UNORM: case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB: return DXGI_FORMAT_B8G8R8A8_TYPELESS;
		case DXGI_FORMAT_BC6H_UF16: case DXGI_FORMAT_BC6H_SF16: return DXGI_FORMAT_BC6H_TYPELESS;
		case DXGI_FORMAT_BC7_UNORM: case DXGI_FORMAT_BC7_UNORM_SRGB: return DXGI_FORMAT_BC7_TYPELESS;
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

//Integer formats can't be filtered or written by a float pixel shader
zest_bool zest__dx_is_integer_format(DXGI_FORMAT format) {
	switch (format) {
		case DXGI_FORMAT_R8_UINT: case DXGI_FORMAT_R8_SINT:
		case DXGI_FORMAT_R8G8_UINT: case DXGI_FORMAT_R8G8_SINT:
		case DXGI_FORMAT_R8G8B8A8_UINT: case DXGI_FORMAT_R8G8B8A8_SINT:
		case DXGI_FORMAT_R10G10B10A2_UINT:
		case DXGI_FORMAT_R16_UINT: case DXGI_FORMAT_R16_SINT:
		case DXGI_FORMAT_R16G16_UINT: case DXGI_FORMAT_R16G16_SINT:
		case DXGI_FORMAT_R16G16B16A16_UINT: case DXGI_FORMAT_R16G16B16A16_SINT:
		case DXGI_FORMAT_R32_UINT: case DXGI_FORMAT_R32_SINT:
		case DXGI_FORMAT_R32G32_UINT: case DXGI_FORMAT_R32G32_SINT:
		case DXGI_FORMAT_R32G32B32_UINT: case DXGI_FORMAT_R32G32B32_SINT:
		case DXGI_FORMAT_R32G32B32A32_UINT: case DXGI_FORMAT_R32G32B32A32_SINT:
			return ZEST_TRUE;
		default:
			return ZEST_FALSE;
	}
}

zest_bool zest__dx_is_signed_integer_format(DXGI_FORMAT format) {
	switch (format) {
		case DXGI_FORMAT_R8_SINT: case DXGI_FORMAT_R8G8_SINT: case DXGI_FORMAT_R8G8B8A8_SINT:
		case DXGI_FORMAT_R16_SINT: case DXGI_FORMAT_R16G16_SINT: case DXGI_FORMAT_R16G16B16A16_SINT:
		case DXGI_FORMAT_R32_SINT: case DXGI_FORMAT_R32G32_SINT: case DXGI_FORMAT_R32G32B32_SINT: case DXGI_FORMAT_R32G32B32A32_SINT:
			return ZEST_TRUE;
		default:
			return ZEST_FALSE;
	}
}

zest_uint zest__dx_image_heap_category(D3D12_RESOURCE_FLAGS resource_flags, zest_uint sample_count, D3D12_RESOURCE_HEAP_TIER tier) {
	if (sample_count > 1) {
		return ZEST_DX_HEAP_CATEGORY_MSAA;
	}
	if (tier < D3D12_RESOURCE_HEAP_TIER_2 && (resource_flags & (D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET | D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL))) {
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

zest_device_queue_type zest__dx_queue_type(zest_queue queue) {
	return zest__dx_family_queue_type(queue->family_index);
}

//Families 0, 1 and 2 are direct, compute and copy, which line up with the graphics, compute and transfer bits
zest_device_queue_type zest__dx_family_queue_type(zest_uint family_index) {
	return (zest_device_queue_type)(1u << family_index);
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

D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE zest__dx_beginning_access(zest_load_op load_op) {
	switch (load_op) {
		case zest_load_op_clear: return D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_CLEAR;
		case zest_load_op_load: return D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_PRESERVE;
		default: return D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_DISCARD;
	}
}

D3D12_RENDER_PASS_ENDING_ACCESS_TYPE zest__dx_ending_access(zest_store_op store_op) {
	return store_op == zest_store_op_store ? D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_PRESERVE : D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_DISCARD;
}

//A negative height moves the origin to the bottom left, so clip space lands on the same pixels as it does on Vulkan
D3D12_VIEWPORT zest__dx_viewport(float x, float y, float width, float height, float min_depth, float max_depth) {
	D3D12_VIEWPORT viewport;
	viewport.TopLeftX = x;
	viewport.TopLeftY = y + height;
	viewport.Width = width;
	viewport.Height = -height;
	viewport.MinDepth = min_depth;
	viewport.MaxDepth = max_depth;
	return viewport;
}

//Flip model buffers can't be sRGB, so an sRGB request gets UNORM buffers with sRGB render target views
zest_bool zest__dx_swapchain_formats(zest_format requested, DXGI_FORMAT *buffer_format, DXGI_FORMAT *view_format, zest_format *reported) {
	switch (requested) {
		case zest_format_b8g8r8a8_unorm: *buffer_format = DXGI_FORMAT_B8G8R8A8_UNORM; *view_format = DXGI_FORMAT_B8G8R8A8_UNORM; break;
		case zest_format_b8g8r8a8_srgb: *buffer_format = DXGI_FORMAT_B8G8R8A8_UNORM; *view_format = DXGI_FORMAT_B8G8R8A8_UNORM_SRGB; break;
		case zest_format_r8g8b8a8_unorm: *buffer_format = DXGI_FORMAT_R8G8B8A8_UNORM; *view_format = DXGI_FORMAT_R8G8B8A8_UNORM; break;
		case zest_format_r8g8b8a8_srgb: *buffer_format = DXGI_FORMAT_R8G8B8A8_UNORM; *view_format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB; break;
		case zest_format_a2b10g10r10_unorm_pack32: *buffer_format = DXGI_FORMAT_R10G10B10A2_UNORM; *view_format = DXGI_FORMAT_R10G10B10A2_UNORM; break;
		case zest_format_r16g16b16a16_sfloat: *buffer_format = DXGI_FORMAT_R16G16B16A16_FLOAT; *view_format = DXGI_FORMAT_R16G16B16A16_FLOAT; break;
		default:
			*buffer_format = DXGI_FORMAT_B8G8R8A8_UNORM;
			*view_format = DXGI_FORMAT_B8G8R8A8_UNORM;
			*reported = zest_format_b8g8r8a8_unorm;
			return ZEST_FALSE;
	}
	*reported = requested;
	return ZEST_TRUE;
}

//Tearing needs a sync interval of 0, so it's only asked for with vsync off
void zest__dx_present_parameters(zest_bool vsync, zest_bool tearing_supported, UINT *sync_interval, UINT *flags) {
	*sync_interval = vsync ? 1 : 0;
	*flags = !vsync && tearing_supported ? DXGI_PRESENT_ALLOW_TEARING : 0;
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

//The queue outlives any one device, so what an earlier device left in it is cleared rather than counted against this one
void zest__dx_setup_dxgi_info_queue(zest_device device) {
	zest_dx_device_backend_t *backend = ZEST_DX_BACKEND(device, device);
	HRESULT result = DXGIGetDebugInterface1(0, ZEST_DX_IID(IDXGIInfoQueue), (void **)&backend->dxgi_info_queue);
	if (FAILED(result)) {
		backend->dxgi_info_queue = NULL;
		ZEST_APPEND_LOG(device->log_path.str, "The DXGI info queue isn't available (0x%08X), so DXGI validation messages only reach the debugger output.", (unsigned int)result);
		return;
	}
	ZEST_DX_CALL(backend->dxgi_info_queue, ClearStoredMessages, zest__dx_debug_dxgi);
}

//The backend's own objects made from its ID3D12Device, apart from the queues
zest_bool zest__dx_create_device_objects(zest_device device) {
	zest_dx_device_backend_t *backend = ZEST_DX_BACKEND(device, device);
	if (ZEST__FLAGGED(device->setup_info.flags, zest_device_init_flag_enable_validation_layers)) {
		zest__dx_setup_info_queue(device);
	}
	ZEST_DX_RETURN_FALSE_ON_FAIL(device, ZEST_DX_CALL(backend->device, QueryInterface, ZEST_DX_IID(ID3D12Device10), (void **)&backend->device10));
	return zest__dx_initialise_cpu_descriptor_heap(device, &backend->cpu_resource_heap, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV) &&
		zest__dx_initialise_cpu_descriptor_heap(device, &backend->cpu_sampler_heap, D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER) &&
		zest__dx_initialise_cpu_descriptor_heap(device, &backend->cpu_rtv_heap, D3D12_DESCRIPTOR_HEAP_TYPE_RTV) &&
		zest__dx_initialise_cpu_descriptor_heap(device, &backend->cpu_dsv_heap, D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
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
	device->min_storage_buffer_offset_alignment = D3D12_RAW_UAV_SRV_BYTE_ALIGNMENT;
	device->max_uniform_buffer_size = 65536;
	device->max_storage_buffer_size = (1u << 27) * 4;
	device->buffer_offset_granularity = ZEST_BUFFER_OFFSET_GRANULARITY;
	//No allocation limit, so the shared code's limit warnings stay quiet
	device->max_memory_allocation_count = 0;
	device->max_bindless_samplers = D3D12_MAX_SHADER_VISIBLE_SAMPLER_HEAP_SIZE;
	//Nine resource regions of 100000 and the transient region stay under the 1,000,000 descriptor shader visible heap limit
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
	if (validation) {
		zest__dx_setup_dxgi_info_queue(device);
	}

	if (!zest__dx_select_adapter(device)) {
		return ZEST_FALSE;
	}
	if (!zest__dx_create_device_objects(device)) {
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

//Recreates the ID3D12Device on the same adapter so the runtime frees what it kept for the old one, leaving none on failure
zest_bool zest__dx_reinit_logical_device(zest_device device) {
	zest_dx_device_backend_t *backend = ZEST_DX_BACKEND(device, device);
	const char *log_path = device->log_path.str;
	zest_WaitForIdleDevice(device);
	zest__dx_cleanup_queues(device);
	zest__dx_release_device_objects(device);
	zest_uint release_count = zest__dx_release_native_device(device, backend->device);
	backend->device = NULL;
	if (release_count != 0) {
		ZEST_APPEND_LOG(log_path, "The D3D12 device can't be recreated because something still holds %u reference(s) to the old device.", release_count);
		return ZEST_FALSE;
	}

	ID3D12Device *native_device = NULL;
	backend->last_result = D3D12CreateDevice((IUnknown *)backend->adapter, D3D_FEATURE_LEVEL_12_0, ZEST_DX_IID(ID3D12Device), (void **)&native_device);
	if (FAILED(backend->last_result)) {
		zest__dx_log_hresult(device, backend->last_result, __FILE__, __LINE__);
		return ZEST_FALSE;
	}
	if (zest__dx_device_already_exists(native_device)) {
		//The other holder keeps it alive, so this isn't a final release and the release count stays as it is
		ZEST_DX_CALL0(native_device, Release);
		ZEST_APPEND_LOG(log_path, "The D3D12 device can't be recreated because a D3D12 device already exists on this adapter in this process, and D3D12 shares one device per adapter.");
		return ZEST_FALSE;
	}
	backend->device = native_device;
	if (!zest__dx_create_device_objects(device) || !zest__dx_create_queues(device)) {
		zest__dx_release_device_objects(device);
		zest__dx_release_native_device(device, backend->device);
		backend->device = NULL;
		return ZEST_FALSE;
	}
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
	platform->reinit_logical_device = zest__dx_reinit_logical_device;
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
	platform->cleanup_image_view_array_backend = zest__dx_cleanup_image_view_array_backend;
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
	platform->new_frame_graph_semaphores_backend = zest__dx_new_frame_graph_semaphores_backend;
	platform->get_frame_graph_semaphores = zest__dx_get_frame_graph_semaphores;
	platform->cleanup_frame_graph_semaphore = zest__dx_cleanup_frame_graph_semaphore;
	platform->new_frame_graph_context_backend = zest__dx_new_frame_graph_context_backend;
	platform->new_execution_barriers_backend = zest__dx_new_execution_barriers_backend;
	platform->new_submission_batch_backend = zest__dx_new_submission_batch_backend;
	platform->new_execution_backend = zest__dx_new_execution_backend;
	platform->add_frame_graph_image_barrier = zest__dx_add_frame_graph_image_barrier;
	platform->add_frame_graph_buffer_barrier = zest__dx_add_frame_graph_buffer_barrier;
	platform->set_next_command_buffer = zest__dx_set_next_command_buffer;
	platform->begin_command_buffer = zest__dx_begin_command_buffer;
	platform->end_command_buffer = zest__dx_end_command_buffer;
	platform->acquire_barrier = zest__dx_acquire_barrier;
	platform->release_barrier = zest__dx_release_barrier;
	platform->begin_render_pass = zest__dx_begin_render_pass;
	platform->end_render_pass = zest__dx_end_render_pass;
	platform->submit_frame_graph_batch = zest__dx_submit_frame_graph_batch;
	platform->carry_over_semaphores = zest__dx_carry_over_semaphores;
	platform->bind_descriptor_sets = zest__dx_bind_descriptor_sets;
	platform->set_screensized_viewport = zest__dx_set_screensized_viewport;
	platform->scissor = zest__dx_scissor;
	platform->viewport = zest__dx_view_port;
	platform->clip = zest__dx_clip;
	platform->get_resource_ptr = zest__dx_get_resource_ptr;
	platform->create_window_surface = zest__dx_create_window_surface;
	platform->new_swapchain_backend = zest__dx_new_swapchain_backend;
	platform->initialise_swapchain = zest__dx_initialise_swapchain;
	platform->cleanup_swapchain_backend = zest__dx_cleanup_swapchain_backend;
	platform->destroy_context_surface = zest__dx_destroy_context_surface;
	platform->acquire_swapchain_image = zest__dx_acquire_swapchain_image;
	platform->present_frame = zest__dx_present_frame;
	platform->dummy_submit_for_present_only = zest__dx_dummy_submit_for_present_only;
	platform->wait_for_renderer_semaphore = zest__dx_wait_for_renderer_semaphore;
	platform->wait_for_fif_semaphore = zest__dx_wait_for_fif_semaphore;
	platform->update_bindless_storage_buffer_descriptor = zest__dx_update_bindless_storage_buffer_descriptor;
	platform->update_bindless_uniform_buffer_descriptor = zest__dx_update_bindless_uniform_buffer_descriptor;
	platform->new_pipeline_backend = zest__dx_new_pipeline_backend;
	platform->build_pipeline = zest__dx_build_pipeline;
	platform->cleanup_pipeline_backend = zest__dx_cleanup_pipeline_backend;
	platform->new_compute_backend = zest__dx_new_compute_backend;
	platform->finish_compute = zest__dx_finish_compute;
	platform->cleanup_compute_backend = zest__dx_cleanup_compute_backend;
	platform->get_db_overlay_vertex_shader = zest__dx_get_db_overlay_vertex_shader;
	platform->get_db_overlay_fragment_shader = zest__dx_get_db_overlay_fragment_shader;
	platform->create_test_render_pass = zest__dx_create_test_render_pass;
	platform->create_image_views_per_mip = zest__dx_create_image_views_per_mip;
	platform->new_frame_graph_image_backend = zest__dx_new_frame_graph_image_backend;
	platform->create_transient_image_unbound = zest__dx_create_transient_image_unbound;
	platform->bind_image_arena = zest__dx_bind_image_arena;
	platform->create_arena_backing = zest__dx_create_arena_backing;
	platform->free_arena_backing = zest__dx_free_arena_backing;
	platform->get_msaa_sample_count = zest__dx_get_msaa_sample_count;
	platform->blit_image_mip = zest__dx_blit_image_mip;
	platform->copy_image_mip = zest__dx_copy_image_mip;
	platform->insert_compute_image_barrier = zest__dx_insert_compute_image_barrier;
	platform->bind_pipeline = zest__dx_bind_pipeline;
	platform->bind_compute_pipeline = zest__dx_bind_compute_pipeline;
	platform->copy_buffer = zest__dx_copy_buffer;
	platform->upload_buffer = zest__dx_upload_buffer;
	platform->bind_vertex_buffer = zest__dx_bind_vertex_buffer;
	platform->bind_index_buffer = zest__dx_bind_index_buffer;
	platform->image_clear = zest__dx_image_clear;
	platform->bind_mesh_vertex_buffer = zest__dx_bind_mesh_vertex_buffer;
	platform->bind_mesh_index_buffer = zest__dx_bind_mesh_index_buffer;
	platform->dispatch_compute = zest__dx_dispatch_compute;
	platform->send_push_constants = zest__dx_send_push_constants;
	platform->draw = zest__dx_draw;
	platform->draw_layer_instruction = zest__dx_draw_layer_instruction;
	platform->draw_indexed = zest__dx_draw_indexed;
	platform->draw_indexed_indirect = zest__dx_draw_indexed_indirect;
	platform->draw_indirect = zest__dx_draw_indirect;
	platform->set_depth_bias = zest__dx_set_depth_bias;
	platform->validate_shader = zest__dx_validate_shader;
	platform->compile_shader = zest__dx_compile_shader;
	platform->create_gpu_profiler_backend = zest__dx_create_gpu_profiler_backend;
	platform->get_final_signal_ptr = zest__dx_get_final_signal_ptr;
	platform->get_final_wait_ptr = zest__dx_get_final_wait_ptr;
	platform->get_swapchain_wait_semaphore = zest__dx_get_swapchain_wait_semaphore;
	platform->get_swapchain_signal_semaphore = zest__dx_get_swapchain_signal_semaphore;
	platform->get_swapchain_identifier = zest__dx_get_swapchain_identifier;
}
// -- End Initialisation_functions

// -- Swapchain_setup
zest_bool zest__dx_create_window_surface(zest_context context) {
	zest_dx_context_backend_t *backend = ZEST_DX_BACKEND(context, context);
	backend->window = (HWND)context->window_data.native_handle;
	if (!backend->window) {
		ZEST_APPEND_LOG(context->device->log_path.str, "The window data has no native window handle, so there's no window for a D3D12 swapchain.");
		return ZEST_FALSE;
	}
	return ZEST_TRUE;
}

//Flip model never reports a swapchain as out of date, so the window's framebuffer size is what shows a resize
void zest__dx_framebuffer_size(zest_context context, zest_extent2d_t *framebuffer, int *window_width, int *window_height) {
	ZEST_ASSERT(context->window_data.window_sizes_callback, "You must set a window size callback in the window data struct.");
	int framebuffer_width = 0;
	int framebuffer_height = 0;
	context->window_data.window_sizes_callback(&context->window_data, &framebuffer_width, &framebuffer_height, window_width, window_height);
	//DXGI can't make a zero sized buffer
	framebuffer->width = (zest_uint)ZEST__MAX(framebuffer_width, 1);
	framebuffer->height = (zest_uint)ZEST__MAX(framebuffer_height, 1);
}

zest_bool zest__dx_initialise_swapchain(zest_context context) {
	zest_swapchain swapchain = context->swapchain;
	zest_device device = context->device;
	zest_dx_device_backend_t *device_backend = ZEST_DX_BACKEND(device, device);
	zest_dx_context_backend_t *backend = ZEST_DX_BACKEND(context, context);
	zest_dx_swapchain_backend_t *swapchain_backend = ZEST_DX_BACKEND(swapchain, swapchain);
	zest_format reported_format = zest_format_undefined;
	zest_bool format_supported = zest__dx_swapchain_formats(context->create_info.color_format, &swapchain_backend->buffer_format, &swapchain_backend->view_format, &reported_format);
	int window_width = 0;
	int window_height = 0;
	zest_extent2d_t extent;
	zest__dx_framebuffer_size(context, &extent, &window_width, &window_height);
	context->window_data.width = window_width;
	context->window_data.height = window_height;

	HRESULT result = S_OK;
	if (!backend->swapchain) {
		if (!format_supported) {
			ZEST_APPEND_LOG(device->log_path.str, "Format %i can't be a flip model swapchain format, so the swapchain uses B8G8R8A8_UNORM instead.", (int)context->create_info.color_format);
		}
		//Context queues don't exist yet, and the swapchain keeps the queue it's created on for its whole life
		ID3D12CommandQueue *native_queue = ZEST_DX_BACKEND(queue, &device->queue_families[0]->queues[0])->queue;
		//Set whatever the vsync setting is, because ResizeBuffers rejects a change of flags
		backend->swapchain_flags = DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT;
		if (device_backend->support.tearing) {
			backend->swapchain_flags |= DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING;
		}
		DXGI_SWAP_CHAIN_DESC1 description = ZEST__ZERO_INIT(DXGI_SWAP_CHAIN_DESC1);
		description.Width = extent.width;
		description.Height = extent.height;
		description.Format = swapchain_backend->buffer_format;
		description.SampleDesc.Count = 1;
		description.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
		description.BufferCount = ZEST__DX_SWAPCHAIN_BUFFER_COUNT;
		description.Scaling = DXGI_SCALING_STRETCH;
		description.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
		description.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;
		description.Flags = backend->swapchain_flags;
		IDXGISwapChain1 *swapchain1 = NULL;
		result = ZEST_DX_CALL(device_backend->factory, CreateSwapChainForHwnd, (IUnknown *)native_queue, backend->window, &description, NULL, NULL, &swapchain1);
		if (SUCCEEDED(result)) {
			result = ZEST_DX_CALL(swapchain1, QueryInterface, ZEST_DX_IID(IDXGISwapChain3), (void **)&backend->swapchain);
			ZEST_DX_RELEASE(swapchain1);
		}
		zest__dx_drain_dxgi_messages(device);
		if (FAILED(result)) {
			backend->swapchain = NULL;
			backend->last_result = result;
			zest__dx_log_hresult(device, result, __FILE__, __LINE__);
			return ZEST_FALSE;
		}
		ZEST_DX_CALL0(native_queue, AddRef);
		backend->present_queue = native_queue;
		result = ZEST_DX_CALL(backend->swapchain, SetMaximumFrameLatency, ZEST_MAX_FIF);
		if (FAILED(result)) {
			zest__dx_log_hresult(device, result, __FILE__, __LINE__);
		}
		backend->frame_latency_waitable = ZEST_DX_CALL0(backend->swapchain, GetFrameLatencyWaitableObject);
		result = ZEST_DX_CALL(device_backend->factory, MakeWindowAssociation, backend->window, DXGI_MWA_NO_ALT_ENTER);
		if (FAILED(result)) {
			zest__dx_log_hresult(device, result, __FILE__, __LINE__);
		}
	} else if (ZEST__NOT_FLAGGED(context->flags, zest_context_flag_device_lost)) {
		result = ZEST_DX_CALL(backend->swapchain, ResizeBuffers, 0, extent.width, extent.height, swapchain_backend->buffer_format, backend->swapchain_flags);
		zest__dx_drain_dxgi_messages(device);
		if (FAILED(result)) {
			backend->last_result = result;
			zest__dx_log_hresult(device, result, __FILE__, __LINE__);
			zest__dx_flag_device_lost(context, result);
			if (ZEST__NOT_FLAGGED(context->flags, zest_context_flag_device_lost)) {
				return ZEST_FALSE;
			}
		}
	}
	//A removed device can't resize, so it keeps the buffers it has until the context is destroyed
	if (ZEST__FLAGGED(context->flags, zest_context_flag_device_lost)) {
		DXGI_SWAP_CHAIN_DESC1 current = ZEST__ZERO_INIT(DXGI_SWAP_CHAIN_DESC1);
		if (SUCCEEDED(ZEST_DX_CALL(backend->swapchain, GetDesc1, &current))) {
			extent.width = current.Width;
			extent.height = current.Height;
		}
	}

	swapchain->format = reported_format;
	swapchain->size = extent;
	swapchain->image_count = ZEST__DX_SWAPCHAIN_BUFFER_COUNT;
	context->window_extent = extent;
	context->dpi_scale = window_width > 0 ? (float)extent.width / (float)window_width : 1.f;
	return zest__dx_create_swapchain_images(context);
}

//Each back buffer gets an image whose backend holds the buffer, and a view whose RTV uses the view format
zest_bool zest__dx_create_swapchain_images(zest_context context) {
	zest_swapchain swapchain = context->swapchain;
	zest_device device = context->device;
	zest_dx_device_backend_t *device_backend = ZEST_DX_BACKEND(device, device);
	zest_dx_context_backend_t *backend = ZEST_DX_BACKEND(context, context);
	zest_dx_swapchain_backend_t *swapchain_backend = ZEST_DX_BACKEND(swapchain, swapchain);
	zest_vec_resize(context->allocator, swapchain->images, swapchain->image_count);
	zest_vec_resize(context->allocator, swapchain->views, swapchain->image_count);
	//Cleared first, so a failure part way leaves cleanup_swapchain_backend only what was created
	zest_vec_foreach(index, swapchain->images) {
		swapchain->images[index] = zest__image_zero;
		swapchain->views[index] = NULL;
	}
	zest_image_info_t image_info = zest_CreateImageInfo(swapchain->size.width, swapchain->size.height);
	image_info.aspect_flags = zest_image_aspect_color_bit;
	image_info.format = swapchain->format;
	D3D12_RENDER_TARGET_VIEW_DESC view_description = ZEST__ZERO_INIT(D3D12_RENDER_TARGET_VIEW_DESC);
	view_description.Format = swapchain_backend->view_format;
	view_description.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
	zest_vec_foreach(index, swapchain->images) {
		zest_image image = &swapchain->images[index];
		image->info = image_info;
		image->layout = zest_image_layout_present;
		zest_dx_image_backend_t *image_backend = (zest_dx_image_backend_t *)ZEST__ALLOCATE(context->allocator, sizeof(zest_dx_image_backend_t));
		*image_backend = ZEST__ZERO_INIT(zest_dx_image_backend_t);
		image->backend = (zest_image_backend)image_backend;
		backend->last_result = ZEST_DX_CALL(backend->swapchain, GetBuffer, index, ZEST_DX_IID(ID3D12Resource), (void **)&image_backend->resource);
		if (FAILED(backend->last_result)) {
			image_backend->resource = NULL;
			zest__dx_log_hresult(device, backend->last_result, __FILE__, __LINE__);
			return ZEST_FALSE;
		}
		image_backend->resource_format = swapchain_backend->buffer_format;
		//Back buffers start in the common layout, which is the present layout
		image_backend->layout = D3D12_BARRIER_LAYOUT_PRESENT;
		image_backend->sample_count = 1;
		image_backend->resource_flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

		void *memory = ZEST__ALLOCATE(context->allocator, sizeof(zest_image_view_t) + sizeof(zest_dx_image_view_backend_t));
		zest_image_view_t *view = (zest_image_view_t *)memory;
		zest_dx_image_view_backend_t *view_backend = (zest_dx_image_view_backend_t *)((char *)memory + sizeof(zest_image_view_t));
		*view = ZEST__ZERO_INIT(zest_image_view_t);
		*view_backend = ZEST__ZERO_INIT(zest_dx_image_view_backend_t);
		view->image = image;
		view->backend = (zest_image_view_backend_t *)view_backend;
		swapchain->views[index] = view;
		view_backend->rtv = zest__dx_allocate_cpu_descriptor(device, &device_backend->cpu_rtv_heap);
		if (!view_backend->rtv.ptr) {
			return ZEST_FALSE;
		}
		ZEST_DX_CALL(device_backend->device, CreateRenderTargetView, image_backend->resource, &view_description, view_backend->rtv);
	}
	return ZEST_TRUE;
}
// -- End Swapchain_setup

// -- Swapchain_presenting
//A timed out wait isn't an error, because a minimised window can stop signalling the waitable
zest_bool zest__dx_acquire_swapchain_image(zest_swapchain swapchain) {
	zest_context context = swapchain->context;
	zest_dx_context_backend_t *backend = ZEST_DX_BACKEND(context, context);
	zest_extent2d_t framebuffer;
	int window_width = 0;
	int window_height = 0;
	zest__dx_framebuffer_size(context, &framebuffer, &window_width, &window_height);
	if (framebuffer.width != swapchain->size.width || framebuffer.height != swapchain->size.height) {
		return ZEST_FALSE;
	}
	if (backend->frame_latency_waitable) {
		WaitForSingleObject(backend->frame_latency_waitable, zest__dx_timeout_milliseconds((zest_microsecs)(context->fence_wait_timeout_ns / 1000)));
	}
	swapchain->current_image_frame = ZEST_DX_CALL0(backend->swapchain, GetCurrentBackBufferIndex);
	return ZEST_TRUE;
}

zest_bool zest__dx_present_frame(zest_context context, zest_context_queue present_queue) {
	zest_swapchain swapchain = context->swapchain;
	zest_device device = context->device;
	zest_dx_context_backend_t *backend = ZEST_DX_BACKEND(context, context);
	ZEST_ASSERT(present_queue->queue, "Trying to present but the frame graph did not acquire a graphics queue!");
	//Compared directly, as zest__acquire_context_queue only reports a switch when the preferred queue was busy
	ID3D12CommandQueue *frame_queue = ZEST_DX_BACKEND(queue, present_queue->queue)->queue;
	zest_execution_timeline frame_timeline = context->frame_sync_timeline[context->current_fif];
	HRESULT result = S_OK;
	if (frame_queue != backend->present_queue && frame_timeline) {
		result = ZEST_DX_CALL(backend->present_queue, Wait, ZEST_DX_BACKEND(execution_timeline, frame_timeline)->fence, frame_timeline->current_value);
	}
	if (SUCCEEDED(result)) {
		UINT sync_interval = 0;
		UINT present_flags = 0;
		zest__dx_present_parameters(ZEST__FLAGGED(context->flags, zest_context_flag_vsync_enabled), ZEST_DX_BACKEND(device, device)->support.tearing, &sync_interval, &present_flags);
		//DXGI_STATUS_OCCLUDED from a minimised window is a success code
		result = ZEST_DX_CALL(backend->swapchain, Present, sync_interval, present_flags);
		zest__dx_drain_dxgi_messages(device);
	}
	backend->last_result = result;
	if (FAILED(result)) {
		zest__dx_log_hresult(device, result, __FILE__, __LINE__);
		zest__dx_flag_device_lost(context, result);
		if (ZEST__FLAGGED(context->flags, zest_context_flag_device_lost)) {
			return ZEST_FALSE;
		}
	}

	zest_bool needs_recreate = ZEST__FLAGGED(context->flags, zest_context_flag_schedule_change_vsync) || swapchain->framebuffer_resized;
	if (!needs_recreate) {
		zest_extent2d_t framebuffer;
		int window_width = 0;
		int window_height = 0;
		zest__dx_framebuffer_size(context, &framebuffer, &window_width, &window_height);
		needs_recreate = framebuffer.width != swapchain->size.width || framebuffer.height != swapchain->size.height;
	}
	if (!needs_recreate) {
		return ZEST_TRUE;
	}
	swapchain->framebuffer_resized = ZEST_FALSE;
	if (ZEST__FLAGGED(context->flags, zest_context_flag_schedule_change_vsync)) {
		if (ZEST__FLAGGED(context->flags, zest_context_flag_vsync_enabled)) {
			ZEST__UNFLAG(context->flags, zest_context_flag_vsync_enabled);
		} else {
			ZEST__FLAG(context->flags, zest_context_flag_vsync_enabled);
		}
		ZEST__UNFLAG(context->flags, zest_context_flag_schedule_change_vsync);
	}
	return ZEST_FALSE;
}

//Runs when the frame submitted nothing, so the back buffer still reaches the present layout and the frame timeline still advances
zest_bool zest__dx_dummy_submit_for_present_only(zest_context context) {
	zest_device device = context->device;
	zest_dx_context_backend_t *backend = ZEST_DX_BACKEND(context, context);
	ID3D12CommandAllocator *command_allocator = backend->utility_allocators[context->current_fif];
	//Missing when initialise_context_backend failed, which was logged then
	if (!command_allocator || !backend->utility_command_list) return ZEST_FALSE;
	HRESULT result = ZEST_DX_CALL0(command_allocator, Reset);
	if (SUCCEEDED(result)) {
		result = ZEST_DX_CALL(backend->utility_command_list, Reset, command_allocator, NULL);
	}
	if (FAILED(result)) {
		backend->last_result = result;
		zest__dx_log_hresult(device, result, __FILE__, __LINE__);
		zest__dx_flag_device_lost(context, result);
		return ZEST_FALSE;
	}

	zest_uint slot = context->graphics_queue_index;
	zest_context_queue graphics_queue = context->queues[slot];
	zest__acquire_context_queue(context, graphics_queue, slot);
	ID3D12CommandQueue *native_queue = ZEST_DX_BACKEND(queue, graphics_queue->queue)->queue;
	zest_swapchain swapchain = context->swapchain;
	zest_image image = &swapchain->images[swapchain->current_image_frame];
	zest_dx_image_backend_t *image_backend = ZEST_DX_BACKEND(image, image);
	if (image_backend->layout != D3D12_BARRIER_LAYOUT_PRESENT) {
		//D3D12 only lets a list that writes a back buffer run on the swapchain's own queue
		native_queue = backend->present_queue;
		D3D12_TEXTURE_BARRIER barrier = zest__dx_image_barrier(image, D3D12_BARRIER_LAYOUT_PRESENT, zest_access_none, zest_access_none, zest_pipeline_stage_top_of_pipe_bit, zest_pipeline_stage_bottom_of_pipe_bit, 0, 1, 0, 1);
		D3D12_BARRIER_GROUP group = ZEST__ZERO_INIT(D3D12_BARRIER_GROUP);
		group.Type = D3D12_BARRIER_TYPE_TEXTURE;
		group.NumBarriers = 1;
		group.pTextureBarriers = &barrier;
		ZEST_DX_CALL(backend->utility_command_list, Barrier, 1, &group);
		image_backend->layout = D3D12_BARRIER_LAYOUT_PRESENT;
		image->layout = zest_image_layout_present;
	}
	result = ZEST_DX_CALL0(backend->utility_command_list, Close);
	if (FAILED(result)) {
		//A list that failed to close can't be reset, so it's replaced
		backend->last_result = result;
		zest__dx_log_hresult(device, result, __FILE__, __LINE__);
		zest__dx_flag_device_lost(context, result);
		ZEST_DX_RELEASE(backend->utility_command_list);
		if (FAILED(ZEST_DX_CALL(ZEST_DX_BACKEND(device, device)->device10, CreateCommandList1, 0, D3D12_COMMAND_LIST_TYPE_DIRECT, D3D12_COMMAND_LIST_FLAG_NONE, ZEST_DX_IID(ID3D12GraphicsCommandList7), (void **)&backend->utility_command_list))) {
			backend->utility_command_list = NULL;
		}
		return ZEST_FALSE;
	}

	zest_execution_timeline ordering_timeline = &context->queue_ordering_timelines[slot];
	ID3D12Fence *ordering_fence = ZEST_DX_BACKEND(execution_timeline, ordering_timeline)->fence;
	//Orders this submission after the slot's last one when that ran on a different D3D12 queue
	ID3D12CommandQueue *ordering_queue = backend->ordering_queues[slot];
	if (ordering_queue && ordering_queue != native_queue && ordering_timeline->current_value > 0) {
		result = ZEST_DX_CALL(native_queue, Wait, ordering_fence, ordering_timeline->current_value);
		if (FAILED(result)) {
			backend->last_result = result;
			zest__dx_log_hresult(device, result, __FILE__, __LINE__);
			zest__dx_flag_device_lost(context, result);
			return ZEST_FALSE;
		}
	}
	ID3D12CommandList *command_lists[1] = { (ID3D12CommandList *)backend->utility_command_list };
	ZEST_DX_CALL(native_queue, ExecuteCommandLists, 1, command_lists);

	zest_execution_timeline frame_timeline = &context->frame_timeline[context->current_fif];
	frame_timeline->current_value += 1;
	result = ZEST_DX_CALL(native_queue, Signal, ZEST_DX_BACKEND(execution_timeline, frame_timeline)->fence, frame_timeline->current_value);
	if (FAILED(result)) {
		//Rolled back so the next wait targets a value that will actually be signalled
		frame_timeline->current_value -= 1;
	} else {
		context->frame_sync_timeline[context->current_fif] = frame_timeline;
	}
	ordering_timeline->current_value += 1;
	HRESULT ordering_result = ZEST_DX_CALL(native_queue, Signal, ordering_fence, ordering_timeline->current_value);
	if (FAILED(ordering_result)) {
		ordering_timeline->current_value -= 1;
	} else {
		backend->ordering_queues[slot] = native_queue;
	}
	result = FAILED(result) ? result : ordering_result;
	if (FAILED(result)) {
		backend->last_result = result;
		zest__dx_log_hresult(device, result, __FILE__, __LINE__);
		zest__dx_flag_device_lost(context, result);
		//The list is already executing with nothing left to say when it finishes, and its allocator is reset next time round
		zest_WaitForIdleDevice(device);
		return ZEST_FALSE;
	}
	return ZEST_TRUE;
}
// -- End Swapchain_presenting

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

//Windowed contexts get the direct allocators and list the dummy present submit records on
zest_bool zest__dx_initialise_context_backend(zest_context context) {
	if (ZEST__FLAGGED(context->flags, zest_context_flag_headless)) return ZEST_TRUE;
	zest_device device = context->device;
	zest_dx_device_backend_t *device_backend = ZEST_DX_BACKEND(device, device);
	zest_dx_context_backend_t *backend = ZEST_DX_BACKEND(context, context);
	zest_ForEachFrameInFlight(frame_in_flight) {
		backend->last_result = ZEST_DX_CALL(device_backend->device, CreateCommandAllocator, D3D12_COMMAND_LIST_TYPE_DIRECT, ZEST_DX_IID(ID3D12CommandAllocator), (void **)&backend->utility_allocators[frame_in_flight]);
		if (FAILED(backend->last_result)) {
			//Allocators created so far are released by the context backend cleanup
			backend->utility_allocators[frame_in_flight] = NULL;
			zest__dx_log_hresult(device, backend->last_result, __FILE__, __LINE__);
			return ZEST_FALSE;
		}
	}
	//Created closed, so every dummy submit resets it the same way
	backend->last_result = ZEST_DX_CALL(device_backend->device10, CreateCommandList1, 0, D3D12_COMMAND_LIST_TYPE_DIRECT, D3D12_COMMAND_LIST_FLAG_NONE, ZEST_DX_IID(ID3D12GraphicsCommandList7), (void **)&backend->utility_command_list);
	if (FAILED(backend->last_result)) {
		backend->utility_command_list = NULL;
		zest__dx_log_hresult(device, backend->last_result, __FILE__, __LINE__);
		return ZEST_FALSE;
	}
	return ZEST_TRUE;
}

void *zest__dx_new_swapchain_backend(zest_context context) {
	zest_dx_swapchain_backend_t *backend = (zest_dx_swapchain_backend_t *)ZEST__ALLOCATE(context->allocator, sizeof(zest_dx_swapchain_backend_t));
	*backend = ZEST__ZERO_INIT(zest_dx_swapchain_backend_t);
	return backend;
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

void *zest__dx_new_frame_graph_semaphores_backend(zest_context context) {
	zest_dx_frame_graph_semaphores_backend_t *backend = (zest_dx_frame_graph_semaphores_backend_t *)ZEST__ALLOCATE(context->allocator, sizeof(zest_dx_frame_graph_semaphores_backend_t));
	*backend = ZEST__ZERO_INIT(zest_dx_frame_graph_semaphores_backend_t);
	return backend;
}

void *zest__dx_new_frame_graph_context_backend(zest_context context) {
	zest_dx_command_list_backend_t *backend = (zest_dx_command_list_backend_t *)zest__linear_allocate(&context->frame_graph_allocator[context->current_fif], sizeof(zest_dx_command_list_backend_t));
	*backend = ZEST__ZERO_INIT(zest_dx_command_list_backend_t);
	return backend;
}

void *zest__dx_new_execution_barriers_backend(zloc_linear_allocator_t *allocator) {
	zest_dx_execution_barriers_backend_t *backend = (zest_dx_execution_barriers_backend_t *)zest__linear_allocate(allocator, sizeof(zest_dx_execution_barriers_backend_t));
	*backend = ZEST__ZERO_INIT(zest_dx_execution_barriers_backend_t);
	return backend;
}

void *zest__dx_new_submission_batch_backend(zest_context context) {
	zest_dx_submission_batch_backend_t *backend = (zest_dx_submission_batch_backend_t *)zest__linear_allocate(&context->frame_graph_allocator[context->current_fif], sizeof(zest_dx_submission_batch_backend_t));
	*backend = ZEST__ZERO_INIT(zest_dx_submission_batch_backend_t);
	return backend;
}
// -- End Backend_setup_functions

// -- Backend_cleanup_functions
void zest__dx_cleanup_device_backend(zest_device device) {
	zest_dx_device_backend_t *backend = ZEST_DX_BACKEND(device, device);
	if (!backend) return;
	zest__dx_release_device_objects(device);
	//The blobs' code lives in dxcompiler.dll, so they're released before it unloads
	ZEST_DX_RELEASE(backend->blit_vertex_shader);
	ZEST_DX_RELEASE(backend->blit_pixel_shader);
	ZEST_DXC_RELEASE(backend->dxc_compiler);
	if (backend->dxc_module) FreeLibrary(backend->dxc_module);
	if (backend->dxil_module) FreeLibrary(backend->dxil_module);
	ZEST_DX_RELEASE(backend->dxgi_info_queue);
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

//The queues are left to the shared device cleanup
void zest__dx_release_device_objects(zest_device device) {
	zest_dx_device_backend_t *backend = ZEST_DX_BACKEND(device, device);
	for (zest_uint format_index = 0; format_index != ZEST_DX_BLIT_FORMAT_COUNT; ++format_index) {
		ZEST_DX_RELEASE(backend->blit_pipelines[format_index]);
	}
	ZEST_DX_RELEASE(backend->utility_root_signature);
	zest__dx_cleanup_cpu_descriptor_heap(device, &backend->cpu_resource_heap);
	zest__dx_cleanup_cpu_descriptor_heap(device, &backend->cpu_sampler_heap);
	zest__dx_cleanup_cpu_descriptor_heap(device, &backend->cpu_rtv_heap);
	zest__dx_cleanup_cpu_descriptor_heap(device, &backend->cpu_dsv_heap);
	if (backend->info_queue) {
		ZEST_DX_CALL(backend->info_queue, UnregisterMessageCallback, backend->info_queue_cookie);
	}
	ZEST_DX_RELEASE(backend->info_queue);
	ZEST_DX_RELEASE(backend->device10);
}

//Returns the device's reference count after the release
zest_uint zest__dx_release_native_device(zest_device device, ID3D12Device *native_device) {
	if (!native_device) return 0;
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
	return release_count;
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
	zest__dx_free_transient_descriptors(device, &backend->transient_descriptors);
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

//Undoes zest__dx_create_queues, so no queue object is left holding the device
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
	zest_map_free(device->allocator, device->queue_names);
}

void zest__dx_cleanup_context_queue_backend(zest_context context, zest_context_queue context_queue) {
	zest_dx_context_queue_backend_t *backend = ZEST_DX_BACKEND(context_queue, context_queue);
	if (!backend) return;
	ZEST_DX_RELEASE(backend->command_list);
	zest_ForEachFrameInFlight(frame_in_flight) {
		ZEST_DX_RELEASE(backend->command_allocators[frame_in_flight]);
		zest__dx_release_transient_descriptors(context->device, &backend->transient_descriptors[frame_in_flight]);
	}
	ZEST__FREE(context->allocator, backend);
	context_queue->backend = NULL;
}

//Shared code frees the semaphores struct itself
void zest__dx_cleanup_frame_graph_semaphore(zest_context context, zest_frame_graph_semaphores semaphores) {
	zest_dx_frame_graph_semaphores_backend_t *backend = ZEST_DX_BACKEND(frame_graph_semaphores, semaphores);
	if (!backend) return;
	zest_ForEachFrameInFlight(frame_in_flight) {
		for (zest_uint queue_index = 0; queue_index != ZEST_QUEUE_COUNT; ++queue_index) {
			ZEST_DX_RELEASE(backend->fences[frame_in_flight][queue_index]);
		}
	}
	ZEST__FREE(context->allocator, backend);
	semaphores->backend = NULL;
}

void zest__dx_cleanup_context_backend(zest_context context) {
	zest_dx_context_backend_t *backend = ZEST_DX_BACKEND(context, context);
	if (!backend) return;
	ZEST_DX_RELEASE(backend->utility_command_list);
	zest_ForEachFrameInFlight(frame_in_flight) {
		ZEST_DX_RELEASE(backend->utility_allocators[frame_in_flight]);
	}
	ZEST__FREE(context->allocator, backend);
	context->backend = NULL;
}

//The swapchain itself is kept for ResizeBuffers, so this only drops what refers to its buffers
void zest__dx_cleanup_swapchain_backend(zest_swapchain swapchain) {
	zest_context context = swapchain->context;
	zest_device device = context->device;
	zest_dx_device_backend_t *device_backend = ZEST_DX_BACKEND(device, device);
	zest_vec_foreach(index, swapchain->images) {
		zest_dx_image_backend_t *image_backend = ZEST_DX_BACKEND(image, &swapchain->images[index]);
		if (!image_backend) continue;
		ZEST_DX_RELEASE(image_backend->resource);
		ZEST__FREE(context->allocator, image_backend);
		swapchain->images[index].backend = NULL;
	}
	//Swapchain views have no handle store, so the shared view cleanup can't free them
	zest_vec_foreach(index, swapchain->views) {
		zest_image_view view = swapchain->views[index];
		if (!view) continue;
		zest__dx_free_cpu_descriptor(device, &device_backend->cpu_rtv_heap, ZEST_DX_BACKEND(image_view, view)->rtv);
		ZEST__FREE(context->allocator, view);
		swapchain->views[index] = NULL;
	}
	if (swapchain->backend) {
		ZEST__FREE(context->allocator, swapchain->backend);
		swapchain->backend = NULL;
	}
}

void zest__dx_destroy_context_surface(zest_context context) {
	zest_dx_context_backend_t *backend = ZEST_DX_BACKEND(context, context);
	ZEST_DX_RELEASE(backend->swapchain);
	if (backend->frame_latency_waitable) {
		CloseHandle(backend->frame_latency_waitable);
		backend->frame_latency_waitable = NULL;
	}
	ZEST_DX_RELEASE(backend->present_queue);
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

void zest__dx_flag_device_lost(zest_context context, HRESULT result) {
	if (result == DXGI_ERROR_DEVICE_REMOVED || result == DXGI_ERROR_DEVICE_RESET) {
		ZEST__FLAG(context->flags, zest_context_flag_device_lost);
	}
}

//After a removal this is the first thing a frame reaches, and zest_BeginFrame returns before anything else could see it
zest_semaphore_status zest__dx_wait_for_renderer_semaphore(zest_context context) {
	zest_execution_timeline timeline = context->frame_sync_timeline[context->current_fif];
	zest_microsecs timeout = (zest_microsecs)((context->fence_wait_timeout_ns + 999) / 1000);
	zest_semaphore_status status = zest__dx_wait_for_timeline(timeline, timeout);
	if (status == zest_semaphore_status_error && ZEST_DX_CALL0(ZEST_DX_BACKEND(execution_timeline, timeline)->fence, GetCompletedValue) == UINT64_MAX) {
		ZEST__FLAG(context->flags, zest_context_flag_device_lost);
	}
	return status;
}

//A removed device reads every fence as UINT64_MAX, which the wait returns on at once
void zest__dx_wait_for_fif_semaphore(zest_context context, zest_uint fif) {
	zest_execution_timeline timeline = context->frame_sync_timeline[fif];
	if (!timeline) return;
	zest__dx_wait_for_timeline(timeline, ZEST__DX_WAIT_FOREVER);
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
	zest__dx_reset_transient_descriptors(&queue_backend->transient_descriptors);
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

zest_bool zest__dx_report_unavailable(zest_device device, const char *message) {
	ZEST_ASSERT_OR_VALIDATE(ZEST_FALSE, device, message, ZEST_FALSE);
	return ZEST_FALSE;
}

ID3D12Resource *zest__dx_buffer_resource(zest_buffer buffer) {
	return ZEST_DX_BACKEND(device_memory_pool, buffer->memory_pool)->buffer;
}

D3D12_GPU_VIRTUAL_ADDRESS zest__dx_buffer_address(zest_buffer buffer) {
	return ZEST_DX_CALL0(zest__dx_buffer_resource(buffer), GetGPUVirtualAddress) + buffer->memory_offset;
}

void zest__dx_record_buffer_copy(ID3D12GraphicsCommandList7 *command_list, zest_buffer source, zest_size source_offset, zest_buffer destination, zest_size destination_offset, zest_size size) {
	ZEST_DX_CALL(command_list, CopyBufferRegion, zest__dx_buffer_resource(destination), destination_offset, zest__dx_buffer_resource(source), source_offset, size);
}

//Copies in one list can overlap, so an immediate copy is ordered before whatever the list records next
void zest__dx_order_immediate_copy(ID3D12GraphicsCommandList7 *command_list) {
	D3D12_GLOBAL_BARRIER barrier = ZEST__ZERO_INIT(D3D12_GLOBAL_BARRIER);
	barrier.SyncBefore = D3D12_BARRIER_SYNC_COPY;
	barrier.SyncAfter = D3D12_BARRIER_SYNC_COPY;
	barrier.AccessBefore = D3D12_BARRIER_ACCESS_COPY_DEST;
	barrier.AccessAfter = D3D12_BARRIER_ACCESS_COPY_SOURCE | D3D12_BARRIER_ACCESS_COPY_DEST;
	D3D12_BARRIER_GROUP group = ZEST__ZERO_INIT(D3D12_BARRIER_GROUP);
	group.Type = D3D12_BARRIER_TYPE_GLOBAL;
	group.NumBarriers = 1;
	group.pGlobalBarriers = &barrier;
	ZEST_DX_CALL(command_list, Barrier, 1, &group);
}

void zest__dx_imm_fill_buffer(zest_queue queue, zest_buffer buffer, zest_uint value) {
	if (!buffer->size) return;
	zest_dx_queue_backend_t *queue_backend = ZEST_DX_BACKEND(queue, queue);
	zest_size block_size = ZEST__MIN(buffer->size, ZEST_DX_FILL_BLOCK_SIZE);
	zest_buffer staging_buffer = zest_CreateStagingBuffer(queue->device, block_size, NULL);
	if (!staging_buffer) return;
	zest_vec_push(queue->device->allocator, queue_backend->pending_staging_buffers, staging_buffer);
	zest_uint *words = (zest_uint *)zest_BufferData(staging_buffer);
	zest_size word_count = block_size / 4;
	for (zest_size word_index = 0; word_index != word_count; ++word_index) {
		words[word_index] = value;
	}
	memcpy(words + word_count, &value, (size_t)(block_size - word_count * 4));
	for (zest_size offset = 0; offset < buffer->size; offset += block_size) {
		zest__dx_record_buffer_copy(queue_backend->command_list, staging_buffer, staging_buffer->memory_offset, buffer, buffer->memory_offset + offset, ZEST__MIN(block_size, buffer->size - offset));
	}
	zest__dx_order_immediate_copy(queue_backend->command_list);
}

void zest__dx_imm_update_buffer(zest_queue queue, zest_buffer buffer, void *data, zest_size intended_size) {
	zest_size copy_size = ZEST__MIN(zloc__align_size_up(intended_size, 4), ZEST__MIN(buffer->size, 65536));
	if (!copy_size) return;
	zest_dx_queue_backend_t *queue_backend = ZEST_DX_BACKEND(queue, queue);
	zest_buffer staging_buffer = zest_CreateStagingBuffer(queue->device, copy_size, NULL);
	if (!staging_buffer) return;
	zest_vec_push(queue->device->allocator, queue_backend->pending_staging_buffers, staging_buffer);
	zest_size data_size = ZEST__MIN(intended_size, copy_size);
	char *staged = (char *)zest_BufferData(staging_buffer);
	memcpy(staged, data, (size_t)data_size);
	memset(staged + data_size, 0, (size_t)(copy_size - data_size));
	zest__dx_record_buffer_copy(queue_backend->command_list, staging_buffer, staging_buffer->memory_offset, buffer, buffer->memory_offset, copy_size);
	zest__dx_order_immediate_copy(queue_backend->command_list);
}

zest_bool zest__dx_imm_clear_color_image(zest_queue queue, zest_image image, zest_clear_value_t clear_value) {
	ZEST_ASSERT_HANDLE(image);	//Not a valid image handle
	zest_device_queue_type queue_type = zest__dx_queue_type(queue);
	D3D12_BARRIER_LAYOUT clear_layout = zest__dx_color_clear_layout(queue->device, image, queue_type);
	if (clear_layout == D3D12_BARRIER_LAYOUT_UNDEFINED) {
		return ZEST_FALSE;
	}
	zest_dx_queue_backend_t *queue_backend = ZEST_DX_BACKEND(queue, queue);
	zest_access_flags tracked_access;
	zest_pipeline_stage_flags tracked_stage;
	zest__dx_layout_access_and_stage(image->layout, queue_type, &tracked_access, &tracked_stage);
	return zest__dx_record_clear(queue->device, queue_backend->command_list, &queue_backend->transient_descriptors, image, clear_layout, tracked_access, tracked_stage, clear_value, image->info.mip_levels, image->info.layer_count);
}

zest_bool zest__dx_imm_clear_depth_stencil_image(zest_queue queue, zest_image image, float depth, zest_uint stencil) {
	ZEST_ASSERT_HANDLE(image);	//Not a valid image handle
	zest_device_queue_type queue_type = zest__dx_queue_type(queue);
	if (!zest__dx_depth_clear_is_valid(queue->device, image, queue_type)) {
		return ZEST_FALSE;
	}
	zest_dx_queue_backend_t *queue_backend = ZEST_DX_BACKEND(queue, queue);
	zest_access_flags tracked_access;
	zest_pipeline_stage_flags tracked_stage;
	zest__dx_layout_access_and_stage(image->layout, queue_type, &tracked_access, &tracked_stage);
	zest_clear_value_t clear_value = ZEST__ZERO_INIT(zest_clear_value_t);
	clear_value.depth_stencil.depth = depth;
	clear_value.depth_stencil.stencil = stencil;
	return zest__dx_record_clear(queue->device, queue_backend->command_list, &queue_backend->transient_descriptors, image, D3D12_BARRIER_LAYOUT_DEPTH_STENCIL_WRITE, tracked_access, tracked_stage, clear_value, image->info.mip_levels, image->info.layer_count);
}

//Draws mip 0 of layer 0 across, and both images go back to the layouts they're tracked in unless the destination was undefined
zest_bool zest__dx_imm_blit_image(zest_queue queue, zest_image src_image, zest_image dst_image, int src_x, int src_y, int src_width, int src_height, int dst_x, int dst_y, int dst_width, int dst_height, zest_filter_type filter) {
	ZEST_ASSERT_HANDLE(src_image);	//Not a valid source image handle
	ZEST_ASSERT_HANDLE(dst_image);	//Not a valid destination image handle
	zest_device device = queue->device;
	zest_device_queue_type queue_type = zest__dx_queue_type(queue);
	//Checked before anything is recorded, as record_blit doesn't know what kind of list it's recording into
	if (queue_type != zest_queue_graphics) {
		ZEST_ASSERT_OR_VALIDATE(ZEST_FALSE, device, "zest_imm_BlitImage draws with a shader on the D3D12 backend, so it needs a graphics queue.", ZEST_FALSE);
		return ZEST_FALSE;
	}
	if (!zest__dx_blit_images_are_valid(device, src_image, dst_image) || !zest__dx_blit_pipeline(device, zest__dx_to_dxgi_format(dst_image->info.format))) {
		return ZEST_FALSE;
	}
	if (!dst_width || !dst_height) {
		return ZEST_TRUE;
	}
	//As in vkCmdBlitImage a negative extent flips, and a viewport can't be negative, so the flip moves onto the source rect
	float source_x = (float)src_x;
	float source_y = (float)src_y;
	float source_width = (float)src_width;
	float source_height = (float)src_height;
	if (dst_width < 0) {
		dst_x += dst_width;
		dst_width = -dst_width;
		source_x += source_width;
		source_width = -source_width;
	}
	if (dst_height < 0) {
		dst_y += dst_height;
		dst_height = -dst_height;
		source_y += source_height;
		source_height = -source_height;
	}
	float source_extent_width = (float)src_image->info.extent.width;
	float source_extent_height = (float)src_image->info.extent.height;
	float source_rect[4];
	source_rect[0] = source_x / source_extent_width;
	source_rect[1] = source_y / source_extent_height;
	source_rect[2] = source_width / source_extent_width;
	source_rect[3] = source_height / source_extent_height;
	D3D12_VIEWPORT viewport = zest__dx_viewport((float)dst_x, (float)dst_y, (float)dst_width, (float)dst_height, 0.f, 1.f);
	D3D12_RECT scissor = { (LONG)dst_x, (LONG)dst_y, (LONG)(dst_x + dst_width), (LONG)(dst_y + dst_height) };

	zest_dx_queue_backend_t *queue_backend = ZEST_DX_BACKEND(queue, queue);
	zest_dx_image_backend_t *destination_backend = ZEST_DX_BACKEND(image, dst_image);
	D3D12_BARRIER_LAYOUT source_layout = ZEST_DX_BACKEND(image, src_image)->layout;
	D3D12_BARRIER_LAYOUT destination_layout = destination_backend->layout;
	//An undefined destination is barriered whole, so that afterwards the one layout it's tracked in is true for every subresource
	zest_bool destination_undefined = destination_layout == D3D12_BARRIER_LAYOUT_UNDEFINED;
	zest_uint destination_mip_count = destination_undefined ? dst_image->info.mip_levels : 1;
	zest_uint destination_layer_count = destination_undefined ? dst_image->info.layer_count : 1;
	zest_access_flags source_access, destination_access, read_access, target_access;
	zest_pipeline_stage_flags source_stage, destination_stage, read_stage, target_stage;
	zest__dx_layout_access_and_stage(src_image->layout, queue_type, &source_access, &source_stage);
	zest__dx_layout_access_and_stage(dst_image->layout, queue_type, &destination_access, &destination_stage);
	zest__dx_layout_access_and_stage(zest_image_layout_shader_read_only_optimal, queue_type, &read_access, &read_stage);
	zest__dx_layout_access_and_stage(zest_image_layout_color_attachment_optimal, queue_type, &target_access, &target_stage);
	D3D12_TEXTURE_BARRIER barriers[2];
	barriers[0] = zest__dx_subresource_barrier(src_image, source_layout, D3D12_BARRIER_LAYOUT_SHADER_RESOURCE, source_access, read_access, source_stage, read_stage, 0, 1, 0, 1);
	barriers[1] = zest__dx_subresource_barrier(dst_image, destination_layout, D3D12_BARRIER_LAYOUT_RENDER_TARGET, destination_access, target_access, destination_stage, target_stage, 0, destination_mip_count, 0, destination_layer_count);
	zest__dx_send_changing_barriers(queue_backend->command_list, barriers, 2);
	if (destination_undefined) {
		zest__dx_discard_subresource(queue_backend->command_list, destination_backend->resource, 0);
	}
	zest_bool drawn = zest__dx_record_blit(device, queue_backend->command_list, &queue_backend->transient_descriptors, src_image, 0, 0, source_rect, dst_image, 0, 0, &viewport, &scissor, filter == zest_filter_nearest ? 0 : 1);
	barriers[0] = zest__dx_subresource_barrier(src_image, D3D12_BARRIER_LAYOUT_SHADER_RESOURCE, source_layout, read_access, source_access, read_stage, source_stage, 0, 1, 0, 1);
	//Going back to UNDEFINED would throw the blit away, so an undefined destination stays a render target
	if (destination_undefined) {
		zest__dx_send_changing_barriers(queue_backend->command_list, barriers, 1);
		destination_backend->layout = D3D12_BARRIER_LAYOUT_RENDER_TARGET;
		dst_image->layout = zest_image_layout_color_attachment_optimal;
		return drawn;
	}
	barriers[1] = zest__dx_subresource_barrier(dst_image, D3D12_BARRIER_LAYOUT_RENDER_TARGET, destination_layout, target_access, destination_access, target_stage, destination_stage, 0, 1, 0, 1);
	zest__dx_send_changing_barriers(queue_backend->command_list, barriers, 2);
	return drawn;
}

//Averages mip 0 of layer 0 over the area both images cover, and both images go back to the layouts they're tracked in unless the destination was undefined
zest_bool zest__dx_imm_resolve_image(zest_queue queue, zest_image src_image, zest_image dst_image) {
	ZEST_ASSERT_HANDLE(src_image);	//Not a valid source image handle
	ZEST_ASSERT_HANDLE(dst_image);	//Not a valid destination image handle
	zest_device device = queue->device;
	zest_device_queue_type queue_type = zest__dx_queue_type(queue);
	zest_dx_image_backend_t *source_backend = ZEST_DX_BACKEND(image, src_image);
	zest_dx_image_backend_t *destination_backend = ZEST_DX_BACKEND(image, dst_image);
	if (queue_type != zest_queue_graphics) {
		ZEST_ASSERT_OR_VALIDATE(ZEST_FALSE, device, "zest_imm_ResolveImage needs a graphics queue on the D3D12 backend.", ZEST_FALSE);
		return ZEST_FALSE;
	}
	if (source_backend->sample_count <= 1) {
		ZEST_ASSERT_OR_VALIDATE(ZEST_FALSE, device, "The source image of zest_imm_ResolveImage must be multisampled.", ZEST_FALSE);
		return ZEST_FALSE;
	}
	if (destination_backend->sample_count > 1) {
		ZEST_ASSERT_OR_VALIDATE(ZEST_FALSE, device, "The destination image of zest_imm_ResolveImage must be single sampled.", ZEST_FALSE);
		return ZEST_FALSE;
	}
	if (zest__dx_typeless_format(source_backend->resource_format) != zest__dx_typeless_format(destination_backend->resource_format)) {
		ZEST_ASSERT_OR_VALIDATE(ZEST_FALSE, device, "zest_imm_ResolveImage needs both formats in the same typeless family on the D3D12 backend.", ZEST_FALSE);
		return ZEST_FALSE;
	}
	ID3D12GraphicsCommandList7 *list = ZEST_DX_BACKEND(queue, queue)->command_list;
	zest_access_flags source_access, destination_access;
	zest_pipeline_stage_flags source_stage, destination_stage;
	zest__dx_layout_access_and_stage(src_image->layout, queue_type, &source_access, &source_stage);
	zest__dx_layout_access_and_stage(dst_image->layout, queue_type, &destination_access, &destination_stage);
	//An undefined destination is barriered whole, so that afterwards the one layout it's tracked in is true for every subresource
	zest_bool destination_undefined = destination_backend->layout == D3D12_BARRIER_LAYOUT_UNDEFINED;
	zest_uint destination_mip_count = destination_undefined ? dst_image->info.mip_levels : 1;
	zest_uint destination_layer_count = destination_undefined ? dst_image->info.layer_count : 1;
	if (destination_undefined && (destination_backend->resource_flags & D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET)) {
		//A placed render target must be discarded before a resolve writes it, and a discard needs the render target layout
		zest__dx_layout_access_and_stage(zest_image_layout_color_attachment_optimal, queue_type, &destination_access, &destination_stage);
		D3D12_TEXTURE_BARRIER discard_barrier = zest__dx_subresource_barrier(dst_image, D3D12_BARRIER_LAYOUT_UNDEFINED, D3D12_BARRIER_LAYOUT_RENDER_TARGET, 0, destination_access, 0, destination_stage, 0, destination_mip_count, 0, destination_layer_count);
		zest__dx_send_texture_barriers(list, &discard_barrier, 1);
		zest__dx_discard_subresource(list, destination_backend->resource, 0);
		destination_backend->layout = D3D12_BARRIER_LAYOUT_RENDER_TARGET;
	}
	D3D12_TEXTURE_BARRIER barriers[2];
	barriers[0] = zest__dx_command_barrier(src_image, source_access, source_stage, D3D12_BARRIER_LAYOUT_RESOLVE_SOURCE, D3D12_BARRIER_ACCESS_RESOLVE_SOURCE, D3D12_BARRIER_SYNC_RESOLVE, ZEST_TRUE, 1, 1);
	barriers[1] = zest__dx_command_barrier(dst_image, destination_access, destination_stage, D3D12_BARRIER_LAYOUT_RESOLVE_DEST, D3D12_BARRIER_ACCESS_RESOLVE_DEST, D3D12_BARRIER_SYNC_RESOLVE, ZEST_TRUE, destination_mip_count, destination_layer_count);
	zest__dx_send_texture_barriers(list, barriers, 2);
	D3D12_RECT source_rect = { 0, 0, (LONG)ZEST__MIN(src_image->info.extent.width, dst_image->info.extent.width), (LONG)ZEST__MIN(src_image->info.extent.height, dst_image->info.extent.height) };
	ZEST_DX_CALL(list, ResolveSubresourceRegion, destination_backend->resource, 0, 0, 0, source_backend->resource, 0, &source_rect, zest__dx_to_dxgi_format(dst_image->info.format), D3D12_RESOLVE_MODE_AVERAGE);
	//Going back to UNDEFINED would throw the resolve away, so an undefined destination is left where Vulkan's resolve leaves it
	if (destination_undefined) {
		destination_backend->layout = zest__dx_barrier_layout(zest_image_layout_transfer_dst_optimal, zest_access_transfer_write_bit, queue_type);
		dst_image->layout = zest_image_layout_transfer_dst_optimal;
		zest__dx_layout_access_and_stage(dst_image->layout, queue_type, &destination_access, &destination_stage);
	}
	barriers[0] = zest__dx_command_barrier(src_image, source_access, source_stage, D3D12_BARRIER_LAYOUT_RESOLVE_SOURCE, D3D12_BARRIER_ACCESS_RESOLVE_SOURCE, D3D12_BARRIER_SYNC_RESOLVE, ZEST_FALSE, 1, 1);
	barriers[1] = zest__dx_command_barrier(dst_image, destination_access, destination_stage, D3D12_BARRIER_LAYOUT_RESOLVE_DEST, D3D12_BARRIER_ACCESS_RESOLVE_DEST, D3D12_BARRIER_SYNC_RESOLVE, ZEST_FALSE, destination_mip_count, destination_layer_count);
	zest__dx_send_texture_barriers(list, barriers, 2);
	return ZEST_TRUE;
}

void zest__dx_imm_send_push_constants(zest_queue queue, void *data, zest_uint size) { zest__dx_report_unavailable(queue->device, "zest_imm_SendPushConstants is not available on the D3D12 backend yet."); }
void zest__dx_imm_bind_compute_pipeline(zest_queue queue, zest_compute compute) { zest__dx_report_unavailable(queue->device, "zest_imm_BindComputePipeline is not available on the D3D12 backend yet."); }
zest_bool zest__dx_imm_dispatch_compute(zest_queue queue, zest_uint group_count_x, zest_uint group_count_y, zest_uint group_count_z) { return zest__dx_report_unavailable(queue->device, "zest_imm_DispatchCompute is not available on the D3D12 backend yet."); }
zest_bool zest__dx_cmd_copy_buffer_one_time(zest_queue queue, zest_buffer src_buffer, zest_buffer dst_buffer, zest_size size, zest_size src_offset, zest_size dst_offset) {
	ID3D12GraphicsCommandList7 *command_list = ZEST_DX_BACKEND(queue, queue)->command_list;
	zest__dx_record_buffer_copy(command_list, src_buffer, src_buffer->memory_offset + src_offset, dst_buffer, dst_buffer->memory_offset + dst_offset, size);
	zest__dx_order_immediate_copy(command_list);
	return ZEST_TRUE;
}

zest_bool zest__dx_copy_buffer_to_image(zest_queue queue, zest_buffer buffer, zest_size src_offset, zest_image image, zest_uint width, zest_uint height) {
	zest_buffer_image_copy_t region = ZEST__ZERO_INIT(zest_buffer_image_copy_t);
	region.image_aspect = zest_image_aspect_color_bit;
	region.layer_count = 1;
	region.image_extent.width = width;
	region.image_extent.height = height;
	region.image_extent.depth = image->info.extent.depth > 1 ? image->info.extent.depth : 1;
	return zest__dx_copy_buffer_regions_to_image(queue, &region, 1, buffer, src_offset, image);
}

//Each mip is drawn from the one above it, so the mips sit in different layouts until the last barrier
zest_bool zest__dx_generate_mipmaps(zest_queue queue, zest_image image) {
	zest_device device = queue->device;
	zest_dx_image_backend_t *backend = ZEST_DX_BACKEND(image, image);
	zest_dx_queue_backend_t *queue_backend = ZEST_DX_BACKEND(queue, queue);
	zest_uint mip_count = image->info.mip_levels;
	zest_uint layer_count = image->info.layer_count;
	//Checked and compiled before anything is recorded, so a failure leaves the image as it was
	if (mip_count > 1 && (!zest__dx_blit_images_are_valid(device, image, image) || !zest__dx_blit_pipeline(device, zest__dx_to_dxgi_format(image->info.format)))) {
		return ZEST_FALSE;
	}
	zest_device_queue_type queue_type = zest__dx_queue_type(queue);
	zest_access_flags tracked_access, read_access, target_access;
	zest_pipeline_stage_flags tracked_stage, read_stage, target_stage;
	zest__dx_layout_access_and_stage(image->layout, queue_type, &tracked_access, &tracked_stage);
	zest__dx_layout_access_and_stage(zest_image_layout_shader_read_only_optimal, queue_type, &read_access, &read_stage);
	zest__dx_layout_access_and_stage(zest_image_layout_color_attachment_optimal, queue_type, &target_access, &target_stage);
	D3D12_BARRIER_LAYOUT tracked_layout = backend->layout;
	D3D12_TEXTURE_BARRIER barriers[2];
	D3D12_BARRIER_GROUP group = ZEST__ZERO_INIT(D3D12_BARRIER_GROUP);
	group.Type = D3D12_BARRIER_TYPE_TEXTURE;
	group.pTextureBarriers = barriers;
	const float full_rect[4] = { 0.f, 0.f, 1.f, 1.f };
	zest_bool drawn = ZEST_TRUE;
	for (zest_uint mip = 1; mip < mip_count; ++mip) {
		zest_uint source_mip = mip - 1;
		if (source_mip == 0) {
			barriers[0] = zest__dx_subresource_barrier(image, tracked_layout, D3D12_BARRIER_LAYOUT_SHADER_RESOURCE, tracked_access, read_access, tracked_stage, read_stage, source_mip, 1, 0, layer_count);
		} else {
			barriers[0] = zest__dx_subresource_barrier(image, D3D12_BARRIER_LAYOUT_RENDER_TARGET, D3D12_BARRIER_LAYOUT_SHADER_RESOURCE, target_access, read_access, target_stage, read_stage, source_mip, 1, 0, layer_count);
		}
		barriers[1] = zest__dx_subresource_barrier(image, tracked_layout, D3D12_BARRIER_LAYOUT_RENDER_TARGET, tracked_access, target_access, tracked_stage, target_stage, mip, 1, 0, layer_count);
		group.NumBarriers = 2;
		ZEST_DX_CALL(queue_backend->command_list, Barrier, 1, &group);
		zest_uint mip_width = ZEST__MAX(image->info.extent.width >> mip, 1);
		zest_uint mip_height = ZEST__MAX(image->info.extent.height >> mip, 1);
		D3D12_VIEWPORT viewport = zest__dx_viewport(0.f, 0.f, (float)mip_width, (float)mip_height, 0.f, 1.f);
		D3D12_RECT scissor = { 0, 0, (LONG)mip_width, (LONG)mip_height };
		//A failed draw still lets the barriers finish, so the image ends in the one layout it's tracked as
		for (zest_uint layer = 0; layer != layer_count && drawn; ++layer) {
			//An upload only copies mip 0, so the other mips are still uninitialised
			zest__dx_discard_subresource(queue_backend->command_list, backend->resource, mip + layer * mip_count);
			drawn =zest__dx_record_blit(device, queue_backend->command_list, &queue_backend->transient_descriptors, image, source_mip, layer, full_rect, image, mip, layer, &viewport, &scissor, 1);
		}
	}
	zest_uint last_mip = mip_count - 1;
	if (mip_count > 1) {
		barriers[0] = zest__dx_subresource_barrier(image, D3D12_BARRIER_LAYOUT_RENDER_TARGET, D3D12_BARRIER_LAYOUT_SHADER_RESOURCE, target_access, read_access, target_stage, read_stage, last_mip, 1, 0, layer_count);
	} else {
		barriers[0] = zest__dx_subresource_barrier(image, tracked_layout, D3D12_BARRIER_LAYOUT_SHADER_RESOURCE, tracked_access, read_access, tracked_stage, read_stage, last_mip, 1, 0, layer_count);
	}
	group.NumBarriers = 1;
	ZEST_DX_CALL(queue_backend->command_list, Barrier, 1, &group);
	backend->layout = D3D12_BARRIER_LAYOUT_SHADER_RESOURCE;
	image->layout = zest_image_layout_shader_read_only_optimal;
	return drawn;
}

//A D3D12 allocator keeps its memory across every reset, so release_resources has nothing to map to
void zest__dx_reset_queue_command_pool(zest_context context, zest_context_queue queue, zest_bool release_resources) {
	zest_dx_context_queue_backend_t *queue_backend = ZEST_DX_BACKEND(context_queue, queue);
	ID3D12CommandAllocator *command_allocator = queue_backend->command_allocators[context->current_fif];
	if (!command_allocator) return;
	zest_dx_context_backend_t *context_backend = ZEST_DX_BACKEND(context, context);
	context_backend->last_result = ZEST_DX_CALL0(command_allocator, Reset);
	if (FAILED(context_backend->last_result)) {
		zest__dx_log_hresult(context->device, context_backend->last_result, __FILE__, __LINE__);
		return;
	}
	//An allocator is only reset once the lists recorded from it have finished, so their descriptors are free too
	zest__dx_reset_transient_descriptors(&queue_backend->transient_descriptors[context->current_fif]);
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

//One committed buffer that any kind of buffer can be suballocated from
zest_bool zest__dx_create_pool_buffer(zest_device device, const D3D12_HEAP_PROPERTIES *heap_properties, zest_size size, ID3D12Resource **buffer_out) {
	zest_dx_device_backend_t *device_backend = ZEST_DX_BACKEND(device, device);
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
	ID3D12Resource *buffer = NULL;
	ZEST_DX_RETURN_FALSE_ON_FAIL(device, ZEST_DX_CALL(device_backend->device10, CreateCommittedResource3, heap_properties, D3D12_HEAP_FLAG_NONE, &description,
		D3D12_BARRIER_LAYOUT_UNDEFINED, NULL, NULL, 0, NULL, ZEST_DX_IID(ID3D12Resource), (void **)&buffer));
	zest__atomic_fetch_add(&device->memory_allocation_count, 1);
	*buffer_out = buffer;
	return ZEST_TRUE;
}

zest_bool zest__dx_add_buffer_memory_pool(zest_device device, zest_context context, zest_size size, zest_buffer_allocator buffer_allocator, zest_device_memory_pool memory_pool) {
	zest_dx_buffer_allocator_backend_t *allocator_backend = ZEST_DX_BACKEND(buffer_allocator, buffer_allocator);
	ZEST_APPEND_LOG(device->log_path.str, "Allocating buffer memory pool, size: %llu, heap type: %i, CPU page property: %i, memory pool: %i", (zest_ull)size,
		(int)allocator_backend->heap_properties.Type, (int)allocator_backend->heap_properties.CPUPageProperty, (int)allocator_backend->heap_properties.MemoryPoolPreference);
	if (!zest__dx_create_pool_buffer(device, &allocator_backend->heap_properties, size, &ZEST_DX_BACKEND(device_memory_pool, memory_pool)->buffer)) {
		return ZEST_FALSE;
	}
	memory_pool->size = size;
	memory_pool->alignment = D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT;
	return ZEST_TRUE;
}

//Buffer categories are one buffer that transient buffers are offsets into, and image categories are one heap that transient images are placed in
zest_device_memory_pool zest__dx_create_arena_backing(zest_device device, zest_context context, zest_uint category, zest_size size) {
	zest_dx_device_backend_t *device_backend = ZEST_DX_BACKEND(device, device);
	zest_device_memory_pool pool = (zest_device_memory_pool)ZEST__NEW(context->allocator, zest_device_memory_pool);
	zest_dx_device_memory_pool_backend_t *backend = (zest_dx_device_memory_pool_backend_t *)ZEST__ALLOCATE(context->allocator, sizeof(zest_dx_device_memory_pool_backend_t));
	*pool = ZEST__ZERO_INIT(zest_device_memory_pool_t);
	*backend = ZEST__ZERO_INIT(zest_dx_device_memory_pool_backend_t);
	pool->magic = zest_INIT_MAGIC(zest_struct_type_device_memory_pool);
	pool->device = device;
	pool->backend = (zest_device_memory_pool_backend)backend;
	pool->size = size;
	zest_bool created = ZEST_FALSE;
	if (category == ZEST_ARENA_CATEGORY_GPU_BUFFERS || category == ZEST_ARENA_CATEGORY_CPU_BUFFERS) {
		zest_bool host_visible = category == ZEST_ARENA_CATEGORY_CPU_BUFFERS;
		zest_memory_property_flags property_flags = host_visible ? zest_memory_property_host_visible_bit | zest_memory_property_host_coherent_bit : zest_memory_property_device_local_bit;
		D3D12_HEAP_PROPERTIES heap_properties = zest__dx_heap_properties(property_flags, device_backend->support.gpu_upload_heap);
		ZEST_APPEND_LOG(device->log_path.str, "Allocating transient buffer arena, size: %llu, category: %u", (zest_ull)size, category);
		created = zest__dx_create_pool_buffer(device, &heap_properties, size, &backend->buffer);
		pool->alignment = D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT;
		if (created && host_visible) {
			device_backend->last_result = ZEST_DX_CALL(backend->buffer, Map, 0, NULL, &pool->mapped);
			if (FAILED(device_backend->last_result)) {
				zest__dx_log_hresult(device, device_backend->last_result, __FILE__, __LINE__);
				pool->mapped = NULL;
				created = ZEST_FALSE;
			}
		}
	} else {
		ZEST_APPEND_LOG(device->log_path.str, "Allocating transient image arena, size: %llu, category: %u", (zest_ull)size, category);
		created = zest__dx_create_image_heap(device, size, category - ZEST_ARENA_CATEGORY_IMAGE_BASE, &backend->heap);
	}
	if (!created) {
		zest__dx_free_arena_backing(device, context, pool);
		return NULL;
	}
	return pool;
}

//Arena backings have no buffer allocator, so they can't go through zest__dx_cleanup_memory_pool_backend
void zest__dx_free_arena_backing(zest_device device, zest_context context, zest_device_memory_pool backing) {
	zest_dx_device_memory_pool_backend_t *backend = ZEST_DX_BACKEND(device_memory_pool, backing);
	if (backend->buffer) {
		if (backing->mapped) {
			ZEST_DX_CALL(backend->buffer, Unmap, 0, NULL);
			backing->mapped = NULL;
		}
		ZEST_DX_RELEASE(backend->buffer);
		zest__atomic_fetch_add(&device->memory_allocation_count, -1);
	}
	if (backend->heap) {
		ZEST_DX_RELEASE(backend->heap);
		zest__atomic_fetch_add(&device->memory_allocation_count, -1);
	}
	ZEST__FREE(context->allocator, backend);
	ZEST__FREE(context->allocator, backing);
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
	D3D12_CPU_DESCRIPTOR_HANDLE handle = ZEST__ZERO_INIT(D3D12_CPU_DESCRIPTOR_HANDLE);
	AcquireSRWLockExclusive(&heap->lock);
	if (!zest_vec_empty(heap->free_handles)) {
		handle = zest_vec_pop(heap->free_handles);
	} else {
		zest_bool block_full = zest_vec_empty(heap->blocks) || heap->next_in_block == ZEST_DX_CPU_DESCRIPTOR_BLOCK_SIZE;
		if (!block_full || zest__dx_add_cpu_descriptor_block(device, heap)) {
			handle = zest__dx_cpu_heap_start(zest_vec_back(heap->blocks));
			handle.ptr += (SIZE_T)heap->next_in_block * heap->increment;
			heap->next_in_block++;
		}
	}
	ReleaseSRWLockExclusive(&heap->lock);
	return handle;
}

void zest__dx_free_cpu_descriptor(zest_device device, zest_dx_cpu_descriptor_heap_t *heap, D3D12_CPU_DESCRIPTOR_HANDLE handle) {
	if (!handle.ptr) return;
	AcquireSRWLockExclusive(&heap->lock);
	zest_vec_push(device->allocator, heap->free_handles, handle);
	ReleaseSRWLockExclusive(&heap->lock);
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
	regions->transient_offset = offset;
	regions->transient_count = ZEST_DX_TRANSIENT_DESCRIPTOR_COUNT;
	offset += ZEST_DX_TRANSIENT_DESCRIPTOR_COUNT;
	regions->resource_descriptor_count = offset;
	regions->sampler_descriptor_count = binding_counts[zest_sampler_binding];
}

void zest__dx_bind_bindless_heaps(zest_device device, ID3D12GraphicsCommandList7 *command_list) {
	zest_dx_descriptor_pool_backend_t *pool_backend = ZEST_DX_BACKEND(descriptor_set, device->bindless_set)->pool;
	ID3D12DescriptorHeap *heaps[2] = { pool_backend->resource_heap, pool_backend->sampler_heap };
	ZEST_DX_CALL(command_list, SetDescriptorHeaps, 2, heaps);
}

zest_bool zest__dx_allocate_transient_descriptor(zest_device device, zest_dx_transient_descriptors_t *descriptors, D3D12_CPU_DESCRIPTOR_HANDLE *cpu, D3D12_GPU_DESCRIPTOR_HANDLE *gpu) {
	zest_dx_descriptor_pool_backend_t *pool_backend = ZEST_DX_BACKEND(descriptor_set, device->bindless_set)->pool;
	if (descriptors->chunk_cursor == zest_vec_size(descriptors->chunks)) {
		zest_uint chunk = 0;
		zest_bool popped = ZEST_FALSE;
		AcquireSRWLockExclusive(&pool_backend->chunk_lock);
		if (!zest_vec_empty(pool_backend->free_transient_chunks)) {
			chunk = zest_vec_pop(pool_backend->free_transient_chunks);
			popped = ZEST_TRUE;
		}
		ReleaseSRWLockExclusive(&pool_backend->chunk_lock);
		if (!popped) {
			ZEST_ASSERT_OR_VALIDATE(ZEST_FALSE, device, "The transient descriptor region is full. Blits and clears recorded since their command lists were last reset hold every transient descriptor.", ZEST_FALSE);
			return ZEST_FALSE;
		}
		zest_vec_push(device->allocator, descriptors->chunks, chunk);
	}
	zest_uint index = pool_backend->regions.transient_offset + descriptors->chunks[descriptors->chunk_cursor] * ZEST_DX_TRANSIENT_DESCRIPTOR_CHUNK + descriptors->slot_cursor;
	cpu->ptr = pool_backend->resource_cpu_start.ptr + (SIZE_T)index * pool_backend->resource_increment;
	gpu->ptr = pool_backend->resource_gpu_start.ptr + (UINT64)index * pool_backend->resource_increment;
	if (++descriptors->slot_cursor == ZEST_DX_TRANSIENT_DESCRIPTOR_CHUNK) {
		descriptors->slot_cursor = 0;
		descriptors->chunk_cursor++;
	}
	return ZEST_TRUE;
}

void zest__dx_reset_transient_descriptors(zest_dx_transient_descriptors_t *descriptors) {
	descriptors->chunk_cursor = 0;
	descriptors->slot_cursor = 0;
}

//Only for context queues, which are always destroyed while the bindless pool is alive
void zest__dx_release_transient_descriptors(zest_device device, zest_dx_transient_descriptors_t *descriptors) {
	if (device->bindless_set && device->bindless_set->backend) {
		zest_dx_descriptor_pool_backend_t *pool_backend = ZEST_DX_BACKEND(descriptor_set, device->bindless_set)->pool;
		AcquireSRWLockExclusive(&pool_backend->chunk_lock);
		zest_vec_foreach(chunk_index, descriptors->chunks) {
			zest_vec_push(device->allocator, pool_backend->free_transient_chunks, descriptors->chunks[chunk_index]);
		}
		ReleaseSRWLockExclusive(&pool_backend->chunk_lock);
	}
	zest__dx_free_transient_descriptors(device, descriptors);
}

//A device reset frees the pool before the immediate queues, and the new pool starts with every chunk free
void zest__dx_free_transient_descriptors(zest_device device, zest_dx_transient_descriptors_t *descriptors) {
	zest_vec_free(device->allocator, descriptors->chunks);
	zest__dx_reset_transient_descriptors(descriptors);
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
	backend->resource_gpu_start = zest__dx_gpu_heap_start(backend->resource_heap);
	backend->sampler_cpu_start = zest__dx_cpu_heap_start(backend->sampler_heap);
	//Pushed in reverse so chunks are handed out from the start of the region
	zest_uint chunk_count = backend->regions.transient_count / ZEST_DX_TRANSIENT_DESCRIPTOR_CHUNK;
	zest_vec_reserve(allocator, backend->free_transient_chunks, chunk_count);
	for (zest_uint chunk_index = chunk_count; chunk_index-- > 0;) {
		zest_vec_push(allocator, backend->free_transient_chunks, chunk_index);
	}
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
		case zest_descriptor_type_storage_image: {
			D3D12_CPU_DESCRIPTOR_HANDLE source = ZEST_DX_BACKEND(image_view, view)->uav;
			ZEST_ASSERT_OR_VALIDATE(source.ptr, device, "The view has no UAV, so its image wasn't created with zest_image_flag_storage.", );
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
		zest_vec_free(allocator, pool_backend->free_transient_chunks);
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

void zest__dx_update_bindless_storage_buffer_descriptor(zest_device device, zest_uint binding_number, zest_uint array_index, zest_buffer buffer, zest_descriptor_set set) {
	ZEST_ASSERT_OR_VALIDATE(buffer->memory_offset % D3D12_RAW_UAV_SRV_BYTE_ALIGNMENT == 0, device, "A storage buffer descriptor's offset must be a multiple of 16 bytes on the D3D12 backend.", );
	zest_dx_descriptor_pool_backend_t *pool_backend = ZEST_DX_BACKEND(descriptor_set, set)->pool;
	ID3D12Device *native_device = ZEST_DX_BACKEND(device, device)->device;
	ID3D12Resource *resource = zest__dx_buffer_resource(buffer);
	zest_uint element_count = (zest_uint)(buffer->size / 4);
	UINT64 first_element = buffer->memory_offset / 4;

	D3D12_SHADER_RESOURCE_VIEW_DESC shader_view = ZEST__ZERO_INIT(D3D12_SHADER_RESOURCE_VIEW_DESC);
	shader_view.Format = DXGI_FORMAT_R32_TYPELESS;
	shader_view.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
	shader_view.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	shader_view.Buffer.FirstElement = first_element;
	shader_view.Buffer.NumElements = element_count;
	shader_view.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_RAW;
	D3D12_CPU_DESCRIPTOR_HANDLE destination = pool_backend->resource_cpu_start;
	destination.ptr += (SIZE_T)(pool_backend->regions.resource_offsets[binding_number] + array_index) * pool_backend->resource_increment;
	ZEST_DX_CALL(native_device, CreateShaderResourceView, resource, &shader_view, destination);

	D3D12_UNORDERED_ACCESS_VIEW_DESC unordered_view = ZEST__ZERO_INIT(D3D12_UNORDERED_ACCESS_VIEW_DESC);
	unordered_view.Format = DXGI_FORMAT_R32_TYPELESS;
	unordered_view.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
	unordered_view.Buffer.FirstElement = first_element;
	unordered_view.Buffer.NumElements = element_count;
	unordered_view.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_RAW;
	destination = pool_backend->resource_cpu_start;
	destination.ptr += (SIZE_T)(pool_backend->regions.storage_buffer_uav_offset + array_index) * pool_backend->resource_increment;
	ZEST_DX_CALL(native_device, CreateUnorderedAccessView, resource, NULL, &unordered_view, destination);
}

void zest__dx_update_bindless_uniform_buffer_descriptor(zest_device device, zest_uint binding_number, zest_uint array_index, zest_buffer buffer, zest_descriptor_set set) {
	zest_dx_descriptor_pool_backend_t *pool_backend = ZEST_DX_BACKEND(descriptor_set, set)->pool;
	if (buffer->size > device->max_uniform_buffer_size) {
		zest__log_validation_error(device, "A uniform buffer descriptor can view at most 65536 bytes on the D3D12 backend, so only the first 65536 bytes are visible.");
	}
	D3D12_CONSTANT_BUFFER_VIEW_DESC constant_view = ZEST__ZERO_INIT(D3D12_CONSTANT_BUFFER_VIEW_DESC);
	constant_view.BufferLocation = zest__dx_buffer_address(buffer);
	constant_view.SizeInBytes = (UINT)ZEST__MIN(zloc__align_size_up(buffer->size, D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT), device->max_uniform_buffer_size);
	D3D12_CPU_DESCRIPTOR_HANDLE destination = pool_backend->resource_cpu_start;
	destination.ptr += (SIZE_T)(pool_backend->regions.resource_offsets[binding_number] + array_index) * pool_backend->resource_increment;
	ZEST_DX_CALL(ZEST_DX_BACKEND(device, device)->device, CreateConstantBufferView, &constant_view, destination);
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

ID3D12GraphicsCommandList7 *zest_GetD3D12CommandList(zest_command_list command_list) {
	ZEST_ASSERT_HANDLE(command_list);	//Not a valid command list handle
	return ZEST_DX_BACKEND(command_list, command_list)->command_list;
}

ID3D12CommandQueue *zest_GetD3D12CommandQueue(zest_context context) {
	ZEST_ASSERT_HANDLE(context);	//Not a valid context handle
	return ZEST_DX_BACKEND(context, context)->present_queue;
}
// -- End Device_OS

// -- Pipelines
void *zest__dx_new_pipeline_backend(zest_context context) { zest__dx_report_unavailable(context->device, "Graphics pipelines are not available on the D3D12 backend yet."); return NULL; }
zest_bool zest__dx_build_pipeline(zest_pipeline pipeline, zest_command_list command_list) { return zest__dx_report_unavailable(pipeline->context->device, "Graphics pipelines are not available on the D3D12 backend yet."); }
void zest__dx_cleanup_pipeline_backend(zest_pipeline pipeline) { zest__dx_report_unavailable(pipeline->context->device, "Graphics pipelines are not available on the D3D12 backend yet."); }
void *zest__dx_new_compute_backend(zest_device device) { zest__dx_report_unavailable(device, "Compute pipelines are not available on the D3D12 backend yet."); return NULL; }
zest_bool zest__dx_finish_compute(zest_device device, zest_compute compute) { return zest__dx_report_unavailable(device, "Compute pipelines are not available on the D3D12 backend yet."); }
void zest__dx_cleanup_compute_backend(zest_compute compute) { zest__dx_report_unavailable((zest_device)compute->handle.store->origin, "Compute pipelines are not available on the D3D12 backend yet."); }
zest_shader_handle zest__dx_get_db_overlay_vertex_shader(zest_device device) { zest__dx_report_unavailable(device, "The debug overlay is not available on the D3D12 backend yet."); return ZEST__ZERO_INIT(zest_shader_handle); }
zest_shader_handle zest__dx_get_db_overlay_fragment_shader(zest_device device) { zest__dx_report_unavailable(device, "The debug overlay is not available on the D3D12 backend yet."); return ZEST__ZERO_INIT(zest_shader_handle); }

//Pipelines are built from rendering info, so there's no render pass to make
void zest__dx_create_test_render_pass(zest_device device, const zest_command_list_t *command_list) {
}

ID3D12RootSignature *zest__dx_utility_root_signature(zest_device device) {
	zest_dx_device_backend_t *backend = ZEST_DX_BACKEND(device, device);
	ID3D12RootSignature *root_signature = (ID3D12RootSignature *)ReadPointerAcquire((PVOID const volatile *)&backend->utility_root_signature);
	if (root_signature) return root_signature;
	AcquireSRWLockExclusive(&backend->root_signature_lock);
	root_signature = backend->utility_root_signature;
	if (!root_signature) {
		root_signature = zest__dx_create_utility_root_signature(device);
		if (root_signature) WritePointerRelease((PVOID volatile *)&backend->utility_root_signature, root_signature);
	}
	ReleaseSRWLockExclusive(&backend->root_signature_lock);
	return root_signature;
}

ID3D12RootSignature *zest__dx_create_utility_root_signature(zest_device device) {
	zest_dx_device_backend_t *backend = ZEST_DX_BACKEND(device, device);
	D3D12_DESCRIPTOR_RANGE1 source_range = ZEST__ZERO_INIT(D3D12_DESCRIPTOR_RANGE1);
	source_range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	source_range.NumDescriptors = 1;
	source_range.Flags = D3D12_DESCRIPTOR_RANGE_FLAG_NONE;

	D3D12_ROOT_PARAMETER1 parameters[2];
	memset(parameters, 0, sizeof(parameters));
	parameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
	parameters[0].Constants.Num32BitValues = 8;
	parameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	parameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	parameters[1].DescriptorTable.NumDescriptorRanges = 1;
	parameters[1].DescriptorTable.pDescriptorRanges = &source_range;
	parameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

	D3D12_STATIC_SAMPLER_DESC samplers[2];
	memset(samplers, 0, sizeof(samplers));
	for (zest_uint sampler_index = 0; sampler_index != 2; ++sampler_index) {
		D3D12_STATIC_SAMPLER_DESC *sampler = &samplers[sampler_index];
		sampler->Filter = sampler_index == 0 ? D3D12_FILTER_MIN_MAG_MIP_LINEAR : D3D12_FILTER_MIN_MAG_MIP_POINT;
		sampler->AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
		sampler->AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
		sampler->AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
		sampler->MaxAnisotropy = 1;
		sampler->ComparisonFunc = D3D12_COMPARISON_FUNC_NONE;
		sampler->BorderColor = D3D12_STATIC_BORDER_COLOR_OPAQUE_BLACK;
		sampler->MaxLOD = D3D12_FLOAT32_MAX;
		sampler->ShaderRegister = sampler_index;
		sampler->ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	}

	D3D12_VERSIONED_ROOT_SIGNATURE_DESC description = ZEST__ZERO_INIT(D3D12_VERSIONED_ROOT_SIGNATURE_DESC);
	description.Version = D3D_ROOT_SIGNATURE_VERSION_1_1;
	description.Desc_1_1.NumParameters = 2;
	description.Desc_1_1.pParameters = parameters;
	description.Desc_1_1.NumStaticSamplers = 2;
	description.Desc_1_1.pStaticSamplers = samplers;
	description.Desc_1_1.Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE;

	ID3DBlob *serialized = NULL;
	ID3DBlob *errors = NULL;
	backend->last_result = D3D12SerializeVersionedRootSignature(&description, &serialized, &errors);
	if (FAILED(backend->last_result)) {
		if (errors) {
			ZEST_APPEND_LOG(device->log_path.str, "The utility root signature couldn't be serialized: %.*s", (int)ZEST_DX_CALL0(errors, GetBufferSize), (const char *)ZEST_DX_CALL0(errors, GetBufferPointer));
		}
		zest__dx_log_hresult(device, backend->last_result, __FILE__, __LINE__);
		ZEST_DX_RELEASE(errors);
		ZEST_DX_RELEASE(serialized);
		return NULL;
	}
	ZEST_DX_RELEASE(errors);
	ID3D12RootSignature *root_signature = NULL;
	backend->last_result = ZEST_DX_CALL(backend->device, CreateRootSignature, 0, ZEST_DX_CALL0(serialized, GetBufferPointer), ZEST_DX_CALL0(serialized, GetBufferSize), ZEST_DX_IID(ID3D12RootSignature), (void **)&root_signature);
	ZEST_DX_RELEASE(serialized);
	if (FAILED(backend->last_result)) {
		zest__dx_log_hresult(device, backend->last_result, __FILE__, __LINE__);
		return NULL;
	}
	return root_signature;
}

ID3D12PipelineState *zest__dx_create_utility_pipeline(zest_device device, ID3DBlob *vertex_shader, ID3DBlob *pixel_shader, DXGI_FORMAT render_target_format, D3D12_CULL_MODE cull_mode, BOOL front_counter_clockwise) {
	zest_dx_device_backend_t *backend = ZEST_DX_BACKEND(device, device);
	ID3D12RootSignature *root_signature = zest__dx_utility_root_signature(device);
	if (!root_signature) return NULL;

	D3D12_GRAPHICS_PIPELINE_STATE_DESC description = ZEST__ZERO_INIT(D3D12_GRAPHICS_PIPELINE_STATE_DESC);
	description.pRootSignature = root_signature;
	description.VS.pShaderBytecode = ZEST_DX_CALL0(vertex_shader, GetBufferPointer);
	description.VS.BytecodeLength = ZEST_DX_CALL0(vertex_shader, GetBufferSize);
	description.PS.pShaderBytecode = ZEST_DX_CALL0(pixel_shader, GetBufferPointer);
	description.PS.BytecodeLength = ZEST_DX_CALL0(pixel_shader, GetBufferSize);

	D3D12_RENDER_TARGET_BLEND_DESC *blend = &description.BlendState.RenderTarget[0];
	blend->SrcBlend = D3D12_BLEND_ONE;
	blend->DestBlend = D3D12_BLEND_ZERO;
	blend->BlendOp = D3D12_BLEND_OP_ADD;
	blend->SrcBlendAlpha = D3D12_BLEND_ONE;
	blend->DestBlendAlpha = D3D12_BLEND_ZERO;
	blend->BlendOpAlpha = D3D12_BLEND_OP_ADD;
	blend->LogicOp = D3D12_LOGIC_OP_NOOP;
	blend->RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
	//A zeroed sample mask draws nothing
	description.SampleMask = UINT_MAX;

	description.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
	description.RasterizerState.CullMode = cull_mode;
	description.RasterizerState.FrontCounterClockwise = front_counter_clockwise;
	description.RasterizerState.DepthClipEnable = TRUE;

	D3D12_DEPTH_STENCILOP_DESC stencil_op = ZEST__ZERO_INIT(D3D12_DEPTH_STENCILOP_DESC);
	stencil_op.StencilFailOp = D3D12_STENCIL_OP_KEEP;
	stencil_op.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
	stencil_op.StencilPassOp = D3D12_STENCIL_OP_KEEP;
	stencil_op.StencilFunc = D3D12_COMPARISON_FUNC_ALWAYS;
	description.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
	description.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_ALWAYS;
	description.DepthStencilState.FrontFace = stencil_op;
	description.DepthStencilState.BackFace = stencil_op;

	description.IBStripCutValue = D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_DISABLED;
	description.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	description.NumRenderTargets = 1;
	description.RTVFormats[0] = render_target_format;
	description.DSVFormat = DXGI_FORMAT_UNKNOWN;
	description.SampleDesc.Count = 1;

	ID3D12PipelineState *pipeline = NULL;
	backend->last_result = ZEST_DX_CALL(backend->device, CreateGraphicsPipelineState, &description, ZEST_DX_IID(ID3D12PipelineState), (void **)&pipeline);
	if (FAILED(backend->last_result)) {
		zest__dx_log_hresult(device, backend->last_result, __FILE__, __LINE__);
		return NULL;
	}
	return pipeline;
}

//D3D12 measures winding after the flipped viewport, in the same framebuffer space as Zest's convention
BOOL zest__dx_front_counter_clockwise(zest_front_face front_face) {
	return front_face == zest_front_face_counter_clockwise ? TRUE : FALSE;
}

ID3D12PipelineState *zest__dx_blit_pipeline(zest_device device, DXGI_FORMAT format) {
	zest_dx_device_backend_t *backend = ZEST_DX_BACKEND(device, device);
	ZEST_ASSERT((zest_uint)format < ZEST_DX_BLIT_FORMAT_COUNT, "The blit pipeline cache has no slot for this DXGI format.");
	ID3D12PipelineState *pipeline = (ID3D12PipelineState *)ReadPointerAcquire((PVOID const volatile *)&backend->blit_pipelines[format]);
	if (pipeline) return pipeline;
	AcquireSRWLockExclusive(&backend->blit_lock);
	pipeline = backend->blit_pipelines[format];
	if (!pipeline) {
		zest_size source_length = (zest_size)strlen(zest__dx_blit_shader);
		zest_bool compiled = (backend->blit_vertex_shader || zest__dx_compile_hlsl(device, zest__dx_blit_shader, source_length, "zest_blit_vertex", "vertex_main", "vs_6_0", &backend->blit_vertex_shader)) &&
			(backend->blit_pixel_shader || zest__dx_compile_hlsl(device, zest__dx_blit_shader, source_length, "zest_blit_pixel", "pixel_main", "ps_6_0", &backend->blit_pixel_shader));
		pipeline = compiled ? zest__dx_create_utility_pipeline(device, backend->blit_vertex_shader, backend->blit_pixel_shader, format, D3D12_CULL_MODE_NONE, zest__dx_front_counter_clockwise(zest_front_face_counter_clockwise)) : NULL;
		if (pipeline) WritePointerRelease((PVOID volatile *)&backend->blit_pipelines[format], pipeline);
	}
	ReleaseSRWLockExclusive(&backend->blit_lock);
	return pipeline;
}

//A blit samples the source in a float pixel shader and draws into the destination as a render target
zest_bool zest__dx_blit_images_are_valid(zest_device device, zest_image source, zest_image destination) {
	zest_image images[2] = { source, destination };
	for (zest_uint image_index = 0; image_index != 2; ++image_index) {
		zest_image image = images[image_index];
		if (zest__is_compressed_format(image->info.format) || zest__is_depth_stencil_format(image->info.format) || image->info.extent.depth > 1 ||
			ZEST_DX_BACKEND(image, image)->sample_count > 1 || zest__dx_is_integer_format(zest__dx_to_dxgi_format(image->info.format))) {
			ZEST_ASSERT_OR_VALIDATE(ZEST_FALSE, device, "Blits and mip generation draw with a shader on the D3D12 backend, so neither image can be 3D, multisampled, or have an integer, depth or block compressed format.", ZEST_FALSE);
			return ZEST_FALSE;
		}
	}
	if (!(ZEST_DX_BACKEND(image, destination)->resource_flags & D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET)) {
		ZEST_ASSERT_OR_VALIDATE(ZEST_FALSE, device, "A blit destination must be a render target on the D3D12 backend. Create it with zest_image_flag_color_attachment.", ZEST_FALSE);
		return ZEST_FALSE;
	}
	return ZEST_TRUE;
}

//The caller records the barriers that put the source in SHADER_RESOURCE and the destination in RENDER_TARGET
zest_bool zest__dx_record_blit(zest_device device, ID3D12GraphicsCommandList7 *list, zest_dx_transient_descriptors_t *descriptors, zest_image source, zest_uint source_mip, zest_uint source_layer, const float source_rect[4], zest_image destination, zest_uint destination_mip, zest_uint destination_layer, const D3D12_VIEWPORT *viewport, const D3D12_RECT *scissor, zest_uint filter) {
	if (!zest__dx_blit_images_are_valid(device, source, destination)) {
		return ZEST_FALSE;
	}
	zest_dx_device_backend_t *device_backend = ZEST_DX_BACKEND(device, device);
	DXGI_FORMAT target_format = zest__dx_to_dxgi_format(destination->info.format);
	ID3D12PipelineState *pipeline = zest__dx_blit_pipeline(device, target_format);
	if (!pipeline) {
		return ZEST_FALSE;
	}
	D3D12_CPU_DESCRIPTOR_HANDLE source_cpu;
	D3D12_GPU_DESCRIPTOR_HANDLE source_gpu;
	if (!zest__dx_allocate_transient_descriptor(device, descriptors, &source_cpu, &source_gpu)) {
		return ZEST_FALSE;
	}
	D3D12_CPU_DESCRIPTOR_HANDLE render_target = zest__dx_allocate_cpu_descriptor(device, &device_backend->cpu_rtv_heap);
	if (!render_target.ptr) {
		return ZEST_FALSE;
	}

	//vkCmdBlitImage reads texels without the view swizzle
	D3D12_SHADER_RESOURCE_VIEW_DESC source_view = zest__dx_image_srv_description(source, zest_image_view_type_2d_array, 1, source_mip, source_layer, 1);
	source_view.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	ZEST_DX_CALL(device_backend->device, CreateShaderResourceView, ZEST_DX_BACKEND(image, source)->resource, &source_view, source_cpu);

	D3D12_RENDER_TARGET_VIEW_DESC target_view = zest__dx_image_rtv_description(destination, zest_image_view_type_2d_array, destination_mip, destination_layer, 1);
	ZEST_DX_CALL(device_backend->device, CreateRenderTargetView, ZEST_DX_BACKEND(image, destination)->resource, &target_view, render_target);
	//Render target views are read when they're set, so the descriptor can be reused straight away
	ZEST_DX_CALL(list, OMSetRenderTargets, 1, &render_target, FALSE, NULL);
	zest__dx_free_cpu_descriptor(device, &device_backend->cpu_rtv_heap, render_target);

	zest_dx_blit_constants_t constants = ZEST__ZERO_INIT(zest_dx_blit_constants_t);
	memcpy(constants.source_rect, source_rect, sizeof(constants.source_rect));
	constants.filter = filter;
	ZEST_DX_CALL(list, SetGraphicsRootSignature, device_backend->utility_root_signature);
	ZEST_DX_CALL(list, SetPipelineState, pipeline);
	ZEST_DX_CALL(list, SetGraphicsRoot32BitConstants, 0, (UINT)(sizeof(constants) / 4), &constants, 0);
	ZEST_DX_CALL(list, SetGraphicsRootDescriptorTable, 1, source_gpu);
	ZEST_DX_CALL(list, RSSetViewports, 1, viewport);
	ZEST_DX_CALL(list, RSSetScissorRects, 1, scissor);
	ZEST_DX_CALL(list, IASetPrimitiveTopology, D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	ZEST_DX_CALL(list, DrawInstanced, 3, 1, 0, 0);
	return ZEST_TRUE;
}
// -- End Pipelines

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

D3D12_RESOURCE_FLAGS zest__dx_image_resource_flags(zest_device device, zest_image_flags flags, DXGI_FORMAT format, zest_uint mip_levels) {
	D3D12_RESOURCE_FLAGS resource_flags = D3D12_RESOURCE_FLAG_NONE;
	if (ZEST__FLAGGED(flags, zest_image_flag_color_attachment)) resource_flags |= D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
	if (ZEST__FLAGGED(flags, zest_image_flag_depth_stencil_attachment)) resource_flags |= D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
	if (ZEST__FLAGGED(flags, zest_image_flag_storage)) resource_flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
	//Lets drivers keep depth compression on depth images that are never sampled
	if (ZEST__FLAGGED(flags, zest_image_flag_depth_stencil_attachment) && ZEST__NOT_FLAGGED(flags, zest_image_flag_sampled)) resource_flags |= D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE;
	//Mip generation draws into each level, which depth and block compressed formats never report support for
	zest_bool can_generate_mips = ZEST__FLAGGED(flags, zest_image_flag_generate_mipmaps) || (ZEST__FLAGGED(flags, zest_image_flag_transfer_dst) && mip_levels > 1);
	if (can_generate_mips && !(resource_flags & D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET)) {
		D3D12_FEATURE_DATA_FORMAT_SUPPORT support = zest__dx_format_support(ZEST_DX_BACKEND(device, device)->device, format);
		if (support.Support1 & D3D12_FORMAT_SUPPORT1_RENDER_TARGET) resource_flags |= D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
	}
	return resource_flags;
}

//Fails only for a format with no DXGI equivalent, and reports nothing so callers can word their own report
zest_bool zest__dx_image_description(zest_device device, zest_image image, zest_uint layer_count, zest_uint sample_count, zest_image_flags flags, D3D12_RESOURCE_DESC1 *description) {
	DXGI_FORMAT format = zest__dx_to_dxgi_format(image->info.format);
	if (format == DXGI_FORMAT_UNKNOWN) {
		return ZEST_FALSE;
	}
	zest_bool sampled_depth = zest__is_depth_stencil_format(image->info.format) && ZEST__FLAGGED(flags, zest_image_flag_sampled);
	DXGI_FORMAT resource_format = sampled_depth ? zest__dx_typeless_format(format) : format;
	zest__dx_texture_description(image, layer_count, sample_count, resource_format, zest__dx_image_resource_flags(device, flags, format, image->info.mip_levels), description);
	return ZEST_TRUE;
}

//Takes the format and flags already chosen for the resource, so rebuilding a description queries nothing
void zest__dx_texture_description(zest_image image, zest_uint layer_count, zest_uint sample_count, DXGI_FORMAT resource_format, D3D12_RESOURCE_FLAGS resource_flags, D3D12_RESOURCE_DESC1 *description) {
	zest_bool is_3d = image->info.extent.depth > 1;
	*description = ZEST__ZERO_INIT(D3D12_RESOURCE_DESC1);
	description->Dimension = is_3d ? D3D12_RESOURCE_DIMENSION_TEXTURE3D : D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	description->Width = image->info.extent.width;
	description->Height = image->info.extent.height;
	description->DepthOrArraySize = (UINT16)(is_3d ? image->info.extent.depth : layer_count);
	description->MipLevels = (UINT16)image->info.mip_levels;
	description->Format = resource_format;
	description->SampleDesc.Count = sample_count;
	description->Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	description->Flags = resource_flags;
}

zest_bool zest__dx_create_image(zest_device device, zest_context context, zest_image image, zest_uint layer_count, zest_sample_count_flags num_samples, zest_image_flags flags) {
	//Transient images are created unbound and placed in a frame graph arena instead
	ZEST_ASSERT(ZEST__NOT_FLAGGED(flags, zest_image_flag_transient));
	if (ZEST__FLAGGED(flags, zest_image_flag_host_visible)) {
		ZEST_REPORT(device, zest_report_invalid_resource, "D3D12 images must be device local, so zest_image_flag_host_visible isn't supported. Upload through a staging buffer instead.");
		return ZEST_FALSE;
	}
	D3D12_RESOURCE_DESC1 description;
	if (!zest__dx_image_description(device, image, layer_count, num_samples, flags, &description)) {
		ZEST_REPORT(device, zest_report_invalid_resource, "Image format %i has no DXGI equivalent.", (int)image->info.format);
		return ZEST_FALSE;
	}
	zest_dx_device_backend_t *device_backend = ZEST_DX_BACKEND(device, device);
	zest_dx_image_backend_t *backend = ZEST_DX_BACKEND(image, image);

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
	buffer_info.backend_memory_bits = zest__dx_image_heap_category(description.Flags, num_samples, device_backend->support.resource_heap_tier);

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
	backend->resource_flags = description.Flags;
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

//A UAV sees one mip, and has no cube dimension, so cubes are written as their array of faces
D3D12_UNORDERED_ACCESS_VIEW_DESC zest__dx_image_uav_description(zest_image image, zest_image_view_type view_type, zest_uint base_mip, zest_uint base_array_index, zest_uint layer_count) {
	D3D12_UNORDERED_ACCESS_VIEW_DESC description = ZEST__ZERO_INIT(D3D12_UNORDERED_ACCESS_VIEW_DESC);
	description.Format = zest__dx_to_dxgi_format(image->info.format);
	if (view_type == zest_image_view_type_3d) {
		description.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE3D;
		description.Texture3D.MipSlice = base_mip;
		description.Texture3D.FirstWSlice = 0;
		description.Texture3D.WSize = (UINT)-1;
	} else if (view_type == zest_image_view_type_2d_array || view_type == zest_image_view_type_1d_array || view_type == zest_image_view_type_cube || view_type == zest_image_view_type_cube_array || base_array_index != 0) {
		description.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2DARRAY;
		description.Texture2DArray.MipSlice = base_mip;
		description.Texture2DArray.FirstArraySlice = base_array_index;
		description.Texture2DArray.ArraySize = layer_count;
	} else {
		description.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
		description.Texture2D.MipSlice = base_mip;
	}
	return description;
}

//For a 3D view the layers are depth slices
D3D12_RENDER_TARGET_VIEW_DESC zest__dx_image_rtv_description(zest_image image, zest_image_view_type view_type, zest_uint base_mip, zest_uint base_array_index, zest_uint layer_count) {
	D3D12_RENDER_TARGET_VIEW_DESC description = ZEST__ZERO_INIT(D3D12_RENDER_TARGET_VIEW_DESC);
	description.Format = zest__dx_to_dxgi_format(image->info.format);
	zest_bool multisampled = ZEST_DX_BACKEND(image, image)->sample_count > 1;
	zest_bool use_array = view_type == zest_image_view_type_2d_array || view_type == zest_image_view_type_1d_array || view_type == zest_image_view_type_cube || view_type == zest_image_view_type_cube_array || base_array_index != 0;
	if (view_type == zest_image_view_type_3d) {
		description.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE3D;
		description.Texture3D.MipSlice = base_mip;
		description.Texture3D.FirstWSlice = base_array_index;
		description.Texture3D.WSize = layer_count;
	} else if (use_array) {
		if (multisampled) {
			description.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DMSARRAY;
			description.Texture2DMSArray.FirstArraySlice = base_array_index;
			description.Texture2DMSArray.ArraySize = layer_count;
		} else {
			description.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DARRAY;
			description.Texture2DArray.MipSlice = base_mip;
			description.Texture2DArray.FirstArraySlice = base_array_index;
			description.Texture2DArray.ArraySize = layer_count;
		}
	} else if (multisampled) {
		description.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DMS;
	} else {
		description.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
		description.Texture2D.MipSlice = base_mip;
	}
	return description;
}

D3D12_DEPTH_STENCIL_VIEW_DESC zest__dx_image_dsv_description(zest_image image, zest_image_view_type view_type, zest_uint base_mip, zest_uint base_array_index, zest_uint layer_count) {
	D3D12_DEPTH_STENCIL_VIEW_DESC description = ZEST__ZERO_INIT(D3D12_DEPTH_STENCIL_VIEW_DESC);
	description.Format = zest__dx_to_dxgi_format(image->info.format);
	zest_bool multisampled = ZEST_DX_BACKEND(image, image)->sample_count > 1;
	zest_bool use_array = view_type == zest_image_view_type_2d_array || view_type == zest_image_view_type_1d_array || view_type == zest_image_view_type_cube || view_type == zest_image_view_type_cube_array || base_array_index != 0;
	if (use_array) {
		if (multisampled) {
			description.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2DMSARRAY;
			description.Texture2DMSArray.FirstArraySlice = base_array_index;
			description.Texture2DMSArray.ArraySize = layer_count;
		} else {
			description.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2DARRAY;
			description.Texture2DArray.MipSlice = base_mip;
			description.Texture2DArray.FirstArraySlice = base_array_index;
			description.Texture2DArray.ArraySize = layer_count;
		}
	} else if (multisampled) {
		description.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2DMS;
	} else {
		description.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
		description.Texture2D.MipSlice = base_mip;
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
	if (!zest__dx_create_view_descriptors(device, image, view_type, mip_levels_this_view, base_mip, base_array_index, layer_count, backend)) {
		if (linear_allocator) {
			zloc_ResetToMarker(linear_allocator, marker);
		} else {
			ZEST__FREE(device->allocator, memory);
		}
		return NULL;
	}
	return image_view;
}

//Frees whatever it allocated when it fails
zest_bool zest__dx_create_view_descriptors(zest_device device, zest_image image, zest_image_view_type view_type, zest_uint mip_levels_this_view, zest_uint base_mip, zest_uint base_array_index, zest_uint layer_count, zest_dx_image_view_backend_t *backend) {
	zest_dx_device_backend_t *device_backend = ZEST_DX_BACKEND(device, device);
	zest_dx_image_backend_t *image_backend = ZEST_DX_BACKEND(image, image);
	zest_bool is_sampled = ZEST__FLAGGED(image->info.flags, zest_image_flag_sampled);
	zest_bool is_storage = (image_backend->resource_flags & D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS) != 0;
	zest_bool is_color_target = ZEST__FLAGGED(image->info.flags, zest_image_flag_color_attachment);
	zest_bool is_depth_target = ZEST__FLAGGED(image->info.flags, zest_image_flag_depth_stencil_attachment);
	//Only sampled images get an SRV, as a depth image is only created typeless when it's sampled
	if (is_sampled) {
		backend->srv = zest__dx_allocate_cpu_descriptor(device, &device_backend->cpu_resource_heap);
	}
	if (is_storage) {
		backend->uav = zest__dx_allocate_cpu_descriptor(device, &device_backend->cpu_resource_heap);
	}
	if (is_color_target) {
		backend->rtv = zest__dx_allocate_cpu_descriptor(device, &device_backend->cpu_rtv_heap);
	}
	if (is_depth_target) {
		backend->dsv = zest__dx_allocate_cpu_descriptor(device, &device_backend->cpu_dsv_heap);
	}
	if ((is_sampled && !backend->srv.ptr) || (is_storage && !backend->uav.ptr) || (is_color_target && !backend->rtv.ptr) || (is_depth_target && !backend->dsv.ptr)) {
		zest__dx_free_view_descriptors(device, backend);
		return ZEST_FALSE;
	}
	if (backend->srv.ptr) {
		D3D12_SHADER_RESOURCE_VIEW_DESC description = zest__dx_image_srv_description(image, view_type, mip_levels_this_view, base_mip, base_array_index, layer_count);
		ZEST_DX_CALL(device_backend->device, CreateShaderResourceView, image_backend->resource, &description, backend->srv);
	}
	if (backend->uav.ptr) {
		D3D12_UNORDERED_ACCESS_VIEW_DESC description = zest__dx_image_uav_description(image, view_type, base_mip, base_array_index, layer_count);
		ZEST_DX_CALL(device_backend->device, CreateUnorderedAccessView, image_backend->resource, NULL, &description, backend->uav);
	}
	if (backend->rtv.ptr) {
		D3D12_RENDER_TARGET_VIEW_DESC description = zest__dx_image_rtv_description(image, view_type, base_mip, base_array_index, layer_count);
		ZEST_DX_CALL(device_backend->device, CreateRenderTargetView, image_backend->resource, &description, backend->rtv);
	}
	if (backend->dsv.ptr) {
		D3D12_DEPTH_STENCIL_VIEW_DESC description = zest__dx_image_dsv_description(image, view_type, base_mip, base_array_index, layer_count);
		ZEST_DX_CALL(device_backend->device, CreateDepthStencilView, image_backend->resource, &description, backend->dsv);
	}
	return ZEST_TRUE;
}

void zest__dx_free_view_descriptors(zest_device device, zest_dx_image_view_backend_t *backend) {
	zest_dx_device_backend_t *device_backend = ZEST_DX_BACKEND(device, device);
	zest__dx_free_cpu_descriptor(device, &device_backend->cpu_resource_heap, backend->srv);
	zest__dx_free_cpu_descriptor(device, &device_backend->cpu_resource_heap, backend->uav);
	zest__dx_free_cpu_descriptor(device, &device_backend->cpu_rtv_heap, backend->rtv);
	zest__dx_free_cpu_descriptor(device, &device_backend->cpu_dsv_heap, backend->dsv);
	*backend = ZEST__ZERO_INIT(zest_dx_image_view_backend_t);
}

void zest__dx_cleanup_image_view_backend(zest_image_view view) {
	zest_dx_image_view_backend_t *backend = ZEST_DX_BACKEND(image_view, view);
	if (!backend) return;
	zest__dx_free_view_descriptors((zest_device)view->handle.store->origin, backend);
}

//The views and their backends are one block that shared code frees, so only the descriptors are released here
void zest__dx_cleanup_image_view_array_backend(zest_image_view_array view_array) {
	zest_device device = (zest_device)view_array->handle.store->origin;
	for (zest_uint mip_index = 0; mip_index != view_array->count; ++mip_index) {
		zest_dx_image_view_backend_t *backend = ZEST_DX_BACKEND(image_view, &view_array->views[mip_index]);
		if (backend) {
			zest__dx_free_view_descriptors(device, backend);
		}
	}
	view_array->magic = 0;
}

zest_image_view_array zest__dx_create_image_views_per_mip(zest_device device, zest_image image, zest_image_view_type view_type, zest_uint base_array_index, zest_uint layer_count, zloc_linear_allocator_t *linear_allocator) {
	zest_uint mip_count = image->info.mip_levels;
	zest_size public_array_size = sizeof(zest_image_view_t) * mip_count;
	zest_size total_size = public_array_size + sizeof(zest_dx_image_view_backend_t) * mip_count;
	void *memory = NULL;
	zest_image_view_array view_array = NULL;
	zest_size marker = 0;
	if (linear_allocator) {
		marker = zloc_GetMarker(linear_allocator);
		memory = zest__linear_allocate(linear_allocator, total_size);
		view_array = (zest_image_view_array)zest__linear_allocate(linear_allocator, sizeof(zest_image_view_array_t));
	} else {
		memory = ZEST__ALLOCATE(device->allocator, total_size);
		view_array = ZEST__NEW(device->allocator, zest_image_view_array);
	}
	*view_array = ZEST__ZERO_INIT(zest_image_view_array_t);
	view_array->magic = zest_INIT_MAGIC(zest_struct_type_view_array);
	view_array->image = image;
	view_array->count = mip_count;
	view_array->views = (zest_image_view_t *)memory;
	zest_dx_image_view_backend_t *backend_array = (zest_dx_image_view_backend_t *)((char *)memory + public_array_size);
	for (zest_uint mip_index = 0; mip_index != mip_count; ++mip_index) {
		zest_image_view view = &view_array->views[mip_index];
		*view = ZEST__ZERO_INIT(zest_image_view_t);
		backend_array[mip_index] = ZEST__ZERO_INIT(zest_dx_image_view_backend_t);
		view->image = image;
		view->backend = (zest_image_view_backend_t *)&backend_array[mip_index];
		if (!zest__dx_create_view_descriptors(device, image, view_type, 1, mip_index, base_array_index, layer_count, &backend_array[mip_index])) {
			for (zest_uint created_index = 0; created_index != mip_index; ++created_index) {
				zest__dx_free_view_descriptors(device, &backend_array[created_index]);
			}
			if (linear_allocator) {
				zloc_ResetToMarker(linear_allocator, marker);
			} else {
				ZEST__FREE(device->allocator, view_array);
				ZEST__FREE(device->allocator, memory);
			}
			return NULL;
		}
	}
	return view_array;
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

D3D12_TEXTURE_BARRIER zest__dx_image_barrier(zest_image image, D3D12_BARRIER_LAYOUT layout_after, zest_access_flags src_access, zest_access_flags dst_access, zest_pipeline_stage_flags src_stage, zest_pipeline_stage_flags dst_stage, zest_uint base_mip, zest_uint mip_count, zest_uint base_layer, zest_uint layer_count) {
	return zest__dx_subresource_barrier(image, ZEST_DX_BACKEND(image, image)->layout, layout_after, src_access, dst_access, src_stage, dst_stage, base_mip, mip_count, base_layer, layer_count);
}

//Takes LayoutBefore from the caller, for subresources that have left the image's one tracked layout
D3D12_TEXTURE_BARRIER zest__dx_subresource_barrier(zest_image image, D3D12_BARRIER_LAYOUT layout_before, D3D12_BARRIER_LAYOUT layout_after, zest_access_flags src_access, zest_access_flags dst_access, zest_pipeline_stage_flags src_stage, zest_pipeline_stage_flags dst_stage, zest_uint base_mip, zest_uint mip_count, zest_uint base_layer, zest_uint layer_count) {
	zest_dx_image_backend_t *backend = ZEST_DX_BACKEND(image, image);
	D3D12_TEXTURE_BARRIER barrier = ZEST__ZERO_INIT(D3D12_TEXTURE_BARRIER);
	barrier.LayoutBefore = layout_before;
	barrier.LayoutAfter = layout_after;
	//An UNDEFINED layout only pairs with no access, which normalises the sync to NONE, whatever an aliased transient's previous occupant did
	barrier.AccessBefore = layout_before == D3D12_BARRIER_LAYOUT_UNDEFINED ? D3D12_BARRIER_ACCESS_NO_ACCESS
		: zest__dx_common_layout_access(barrier.LayoutBefore, zest__dx_barrier_access(src_access, ZEST_TRUE, barrier.LayoutBefore));
	barrier.AccessAfter = zest__dx_common_layout_access(barrier.LayoutAfter, zest__dx_barrier_access(dst_access, ZEST_TRUE, barrier.LayoutAfter));
	barrier.SyncBefore = zest__dx_normalise_sync(zest__dx_barrier_sync(src_stage, ZEST_TRUE), barrier.AccessBefore);
	barrier.SyncAfter = zest__dx_normalise_sync(zest__dx_barrier_sync(dst_stage, ZEST_FALSE), barrier.AccessAfter);
	barrier.pResource = backend->resource;
	barrier.Subresources.IndexOrFirstMipLevel = base_mip;
	barrier.Subresources.NumMipLevels = mip_count;
	barrier.Subresources.FirstArraySlice = base_layer;
	barrier.Subresources.NumArraySlices = layer_count;
	barrier.Subresources.FirstPlane = 0;
	barrier.Subresources.NumPlanes = zest__dx_plane_count(image->info.format);
	//Placed render and depth targets have to be initialised by a clear, discard or copy before first use
	if (barrier.LayoutBefore == D3D12_BARRIER_LAYOUT_UNDEFINED && (backend->resource_flags & (D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET | D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL))) {
		barrier.Flags = D3D12_TEXTURE_BARRIER_FLAG_DISCARD;
	}
	return barrier;
}

//Leaves out barriers that change neither the layout nor the access, as they'd only repeat what's in place
void zest__dx_send_changing_barriers(ID3D12GraphicsCommandList7 *list, D3D12_TEXTURE_BARRIER *barriers, zest_uint barrier_count) {
	zest_uint changing_count = 0;
	for (zest_uint index = 0; index != barrier_count; ++index) {
		if (barriers[index].LayoutBefore == barriers[index].LayoutAfter && barriers[index].AccessBefore == barriers[index].AccessAfter) continue;
		barriers[changing_count++] = barriers[index];
	}
	if (!changing_count) return;
	zest__dx_send_texture_barriers(list, barriers, changing_count);
}

void zest__dx_send_texture_barriers(ID3D12GraphicsCommandList7 *list, D3D12_TEXTURE_BARRIER *barriers, zest_uint barrier_count) {
	D3D12_BARRIER_GROUP group = ZEST__ZERO_INIT(D3D12_BARRIER_GROUP);
	group.Type = D3D12_BARRIER_TYPE_TEXTURE;
	group.NumBarriers = barrier_count;
	group.pTextureBarriers = barriers;
	ZEST_DX_CALL(list, Barrier, 1, &group);
}

//A placed render target must be discarded, cleared or copied to before it's drawn into, and a discard flag on a barrier doesn't count
void zest__dx_discard_subresource(ID3D12GraphicsCommandList7 *list, ID3D12Resource *resource, zest_uint subresource) {
	D3D12_DISCARD_REGION discard_region = ZEST__ZERO_INIT(D3D12_DISCARD_REGION);
	discard_region.FirstSubresource = subresource;
	discard_region.NumSubresources = 1;
	ZEST_DX_CALL(list, DiscardResource, resource, &discard_region);
}

//Moves the first mips and layers between the image's tracked layout and a command's state, which is given in D3D12's terms as Zest's flags can't name it
D3D12_TEXTURE_BARRIER zest__dx_command_barrier(zest_image image, zest_access_flags tracked_access, zest_pipeline_stage_flags tracked_stage, D3D12_BARRIER_LAYOUT command_layout, D3D12_BARRIER_ACCESS command_access, D3D12_BARRIER_SYNC command_sync, zest_bool to_command, zest_uint mip_count, zest_uint layer_count) {
	D3D12_BARRIER_LAYOUT tracked_layout = ZEST_DX_BACKEND(image, image)->layout;
	D3D12_TEXTURE_BARRIER barrier;
	if (to_command) {
		barrier = zest__dx_subresource_barrier(image, tracked_layout, command_layout, tracked_access, 0, tracked_stage, 0, 0, mip_count, 0, layer_count);
		barrier.AccessAfter = command_access;
		barrier.SyncAfter = command_sync;
	} else {
		barrier = zest__dx_subresource_barrier(image, command_layout, tracked_layout, 0, tracked_access, 0, tracked_stage, 0, mip_count, 0, layer_count);
		barrier.AccessBefore = command_access;
		barrier.SyncBefore = command_sync;
	}
	return barrier;
}

//Returns the layout a colour clear writes the image in on this queue type, or reports why it can't and returns UNDEFINED
D3D12_BARRIER_LAYOUT zest__dx_color_clear_layout(zest_device device, zest_image image, zest_device_queue_type queue_type) {
	if (queue_type == zest_queue_transfer) {
		ZEST_ASSERT_OR_VALIDATE(ZEST_FALSE, device, "Images can't be cleared on a copy queue on the D3D12 backend. Clear them on a graphics or compute queue.", D3D12_BARRIER_LAYOUT_UNDEFINED);
		return D3D12_BARRIER_LAYOUT_UNDEFINED;
	}
	D3D12_RESOURCE_FLAGS resource_flags = ZEST_DX_BACKEND(image, image)->resource_flags;
	zest_bool can_render = queue_type == zest_queue_graphics && (resource_flags & D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET);
	if ((resource_flags & D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS) && (zest__dx_forced_uav_clear || !can_render)) {
		return D3D12_BARRIER_LAYOUT_UNORDERED_ACCESS;
	}
	if (can_render) {
		return D3D12_BARRIER_LAYOUT_RENDER_TARGET;
	}
	if (resource_flags & D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET) {
		ZEST_ASSERT_OR_VALIDATE(ZEST_FALSE, device, "A compute queue can only clear storage images on the D3D12 backend. Clear this image on a graphics queue, or create it with zest_image_flag_storage.", D3D12_BARRIER_LAYOUT_UNDEFINED);
		return D3D12_BARRIER_LAYOUT_UNDEFINED;
	}
	ZEST_ASSERT_OR_VALIDATE(ZEST_FALSE, device, "A colour image must be a render target or a storage image to be cleared on the D3D12 backend. Create it with zest_image_flag_color_attachment or zest_image_flag_storage.", D3D12_BARRIER_LAYOUT_UNDEFINED);
	return D3D12_BARRIER_LAYOUT_UNDEFINED;
}

zest_bool zest__dx_depth_clear_is_valid(zest_device device, zest_image image, zest_device_queue_type queue_type) {
	if (queue_type != zest_queue_graphics) {
		ZEST_ASSERT_OR_VALIDATE(ZEST_FALSE, device, "Depth images can only be cleared on a graphics queue on the D3D12 backend.", ZEST_FALSE);
		return ZEST_FALSE;
	}
	if (!(ZEST_DX_BACKEND(image, image)->resource_flags & D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL)) {
		ZEST_ASSERT_OR_VALIDATE(ZEST_FALSE, device, "A depth image must be a depth stencil attachment to be cleared on the D3D12 backend. Create it with zest_image_flag_depth_stencil_attachment.", ZEST_FALSE);
		return ZEST_FALSE;
	}
	return ZEST_TRUE;
}

//Clears every layer of the first mip_count mips through one temporary view per mip, then returns the image to its tracked layout
zest_bool zest__dx_record_clear(zest_device device, ID3D12GraphicsCommandList7 *list, zest_dx_transient_descriptors_t *descriptors, zest_image image, D3D12_BARRIER_LAYOUT clear_layout, zest_access_flags tracked_access, zest_pipeline_stage_flags tracked_stage, zest_clear_value_t clear_value, zest_uint mip_count, zest_uint layer_count) {
	zest_dx_device_backend_t *device_backend = ZEST_DX_BACKEND(device, device);
	zest_dx_image_backend_t *backend = ZEST_DX_BACKEND(image, image);
	D3D12_BARRIER_ACCESS clear_access = D3D12_BARRIER_ACCESS_UNORDERED_ACCESS;
	D3D12_BARRIER_SYNC clear_sync = D3D12_BARRIER_SYNC_CLEAR_UNORDERED_ACCESS_VIEW;
	zest_image_layout cleared_layout = zest_image_layout_general;
	zest_dx_cpu_descriptor_heap_t *view_heap = &device_backend->cpu_resource_heap;
	if (clear_layout == D3D12_BARRIER_LAYOUT_RENDER_TARGET) {
		clear_access = D3D12_BARRIER_ACCESS_RENDER_TARGET;
		clear_sync = D3D12_BARRIER_SYNC_RENDER_TARGET;
		cleared_layout = zest_image_layout_color_attachment_optimal;
		view_heap = &device_backend->cpu_rtv_heap;
	} else if (clear_layout == D3D12_BARRIER_LAYOUT_DEPTH_STENCIL_WRITE) {
		clear_access = D3D12_BARRIER_ACCESS_DEPTH_STENCIL_WRITE;
		clear_sync = D3D12_BARRIER_SYNC_DEPTH_STENCIL;
		cleared_layout = zest_image_layout_depth_stencil_attachment_optimal;
		view_heap = &device_backend->cpu_dsv_heap;
	}
	//Views are read when a clear is recorded, so one serves every mip
	D3D12_CPU_DESCRIPTOR_HANDLE view = zest__dx_allocate_cpu_descriptor(device, view_heap);
	if (!view.ptr) {
		return ZEST_FALSE;
	}
	//Always sent, as a barrier that keeps the layout and access still orders earlier writes before the clear
	D3D12_TEXTURE_BARRIER barrier = zest__dx_command_barrier(image, tracked_access, tracked_stage, clear_layout, clear_access, clear_sync, ZEST_TRUE, mip_count, layer_count);
	zest__dx_send_texture_barriers(list, &barrier, 1);

	zest_bool is_3d = image->info.extent.depth > 1;
	zest_image_view_type view_type = is_3d ? zest_image_view_type_3d : zest_image_view_type_2d_array;
	DXGI_FORMAT format = zest__dx_to_dxgi_format(image->info.format);
	zest_bool is_integer = zest__dx_is_integer_format(format);
	//Vulkan reads the same union as integers for integer formats
	UINT bits[4];
	memcpy(bits, &clear_value.color, sizeof(bits));
	float color[4] = { clear_value.color.x, clear_value.color.y, clear_value.color.z, clear_value.color.w };
	if (is_integer && clear_layout == D3D12_BARRIER_LAYOUT_RENDER_TARGET) {
		//A render target view takes integers as float values, which hold them exactly below 2^24
		zest_bool is_signed = zest__dx_is_signed_integer_format(format);
		for (zest_uint channel = 0; channel != 4; ++channel) {
			color[channel] = is_signed ? (float)(INT)bits[channel] : (float)bits[channel];
		}
	}
	zest_bool cleared = ZEST_TRUE;
	for (zest_uint mip = 0; mip != mip_count; ++mip) {
		zest_uint view_layers = is_3d ? ZEST__MAX(image->info.extent.depth >> mip, 1) : layer_count;
		if (clear_layout == D3D12_BARRIER_LAYOUT_RENDER_TARGET) {
			D3D12_RENDER_TARGET_VIEW_DESC description = zest__dx_image_rtv_description(image, view_type, mip, 0, view_layers);
			ZEST_DX_CALL(device_backend->device, CreateRenderTargetView, backend->resource, &description, view);
			ZEST_DX_CALL(list, ClearRenderTargetView, view, color, 0, NULL);
		} else if (clear_layout == D3D12_BARRIER_LAYOUT_DEPTH_STENCIL_WRITE) {
			D3D12_DEPTH_STENCIL_VIEW_DESC description = zest__dx_image_dsv_description(image, view_type, mip, 0, view_layers);
			ZEST_DX_CALL(device_backend->device, CreateDepthStencilView, backend->resource, &description, view);
			D3D12_CLEAR_FLAGS clear_flags = zest__dx_plane_count(image->info.format) > 1 ? D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL : D3D12_CLEAR_FLAG_DEPTH;
			ZEST_DX_CALL(list, ClearDepthStencilView, view, clear_flags, clear_value.depth_stencil.depth, (UINT8)clear_value.depth_stencil.stencil, 0, NULL);
		} else {
			//A UAV clear takes the view twice, from the bound shader visible heap and from a CPU only heap
			D3D12_CPU_DESCRIPTOR_HANDLE shader_visible_cpu;
			D3D12_GPU_DESCRIPTOR_HANDLE shader_visible_gpu;
			cleared = zest__dx_allocate_transient_descriptor(device, descriptors, &shader_visible_cpu, &shader_visible_gpu);
			if (!cleared) break;
			D3D12_UNORDERED_ACCESS_VIEW_DESC description = zest__dx_image_uav_description(image, view_type, mip, 0, view_layers);
			ZEST_DX_CALL(device_backend->device, CreateUnorderedAccessView, backend->resource, NULL, &description, view);
			ZEST_DX_CALL(device_backend->device, CopyDescriptorsSimple, 1, shader_visible_cpu, view, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
			if (is_integer) {
				ZEST_DX_CALL(list, ClearUnorderedAccessViewUint, shader_visible_gpu, view, backend->resource, bits, 0, NULL);
			} else {
				ZEST_DX_CALL(list, ClearUnorderedAccessViewFloat, shader_visible_gpu, view, backend->resource, color, 0, NULL);
			}
		}
	}
	zest__dx_free_cpu_descriptor(device, view_heap, view);

	//Going back to UNDEFINED would throw the clear away, so a whole image that was undefined stays in the layout it was cleared in
	if (backend->layout == D3D12_BARRIER_LAYOUT_UNDEFINED && mip_count == image->info.mip_levels && layer_count == image->info.layer_count) {
		backend->layout = clear_layout;
		image->layout = cleared_layout;
		return cleared;
	}
	barrier = zest__dx_command_barrier(image, tracked_access, tracked_stage, clear_layout, clear_access, clear_sync, ZEST_FALSE, mip_count, layer_count);
	zest__dx_send_texture_barriers(list, &barrier, 1);
	return cleared;
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

	D3D12_BARRIER_LAYOUT layout_after = exact_layout_after ? *exact_layout_after : zest__dx_barrier_layout(new_layout, access_after, queue_type);
	D3D12_TEXTURE_BARRIER barrier = zest__dx_image_barrier(image, layout_after, access_before, access_after, stage_before, stage_after, base_mip_index, mip_levels, base_array_index, layer_count);

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

//Shared code frees a backend made without a linear allocator with device->allocator
void *zest__dx_new_frame_graph_image_backend(zest_device device, zloc_linear_allocator_t *allocator, zest_image image, zest_image imported_image) {
	zest_dx_image_backend_t *backend = allocator
		? (zest_dx_image_backend_t *)zest__linear_allocate(allocator, sizeof(zest_dx_image_backend_t))
		: (zest_dx_image_backend_t *)ZEST__ALLOCATE(device->allocator, sizeof(zest_dx_image_backend_t));
	*backend = ZEST__ZERO_INIT(zest_dx_image_backend_t);
	backend->layout = D3D12_BARRIER_LAYOUT_UNDEFINED;
	image->backend = (zest_image_backend)backend;
	return backend;
}

//D3D12 has no unbound resource, so this only sizes the image and keeps what bind_image_arena needs to create it
zest_bool zest__dx_create_transient_image_unbound(zest_device device, zest_context context, zest_image image, zest_uint layer_count, zest_sample_count_flags num_samples, zest_image_flags flags, zest_transient_memory_info_t *info) {
	if (ZEST__FLAGGED(flags, zest_image_flag_host_visible)) {
		ZEST_REPORT(device, zest_report_invalid_resource, "D3D12 images must be device local, so transient images can't take zest_resource_usage_hint_cpu_transfer. Copy them to a buffer instead.");
		return ZEST_FALSE;
	}
	D3D12_RESOURCE_DESC1 description;
	if (!zest__dx_image_description(device, image, layer_count, num_samples, flags, &description)) {
		ZEST_REPORT(device, zest_report_invalid_resource, "Transient image format %i has no DXGI equivalent.", (int)image->info.format);
		return ZEST_FALSE;
	}
	zest_dx_device_backend_t *device_backend = ZEST_DX_BACKEND(device, device);
	D3D12_RESOURCE_ALLOCATION_INFO allocation_info = zest__dx_resource_allocation_info(device_backend->device10, &description);
	if (allocation_info.SizeInBytes == UINT64_MAX) {
		ZEST_REPORT(device, zest_report_invalid_resource, "D3D12 rejected the transient image description (format %i, %ux%ux%u, %u layers, %u mips, %u samples).",
			(int)image->info.format, image->info.extent.width, image->info.extent.height, image->info.extent.depth, layer_count, image->info.mip_levels, num_samples);
		return ZEST_FALSE;
	}
	zest_dx_image_backend_t *backend = ZEST_DX_BACKEND(image, image);
	backend->resource_format = description.Format;
	backend->layout = D3D12_BARRIER_LAYOUT_UNDEFINED;
	backend->sample_count = num_samples;
	backend->resource_flags = description.Flags;
	//Rounded up to the alignment so packed offsets stay aligned
	info->size = (allocation_info.SizeInBytes + allocation_info.Alignment - 1) & ~(allocation_info.Alignment - 1);
	info->alignment = allocation_info.Alignment;
	info->category = ZEST_ARENA_CATEGORY_IMAGE_BASE + zest__dx_image_heap_category(description.Flags, num_samples, device_backend->support.resource_heap_tier);
	return ZEST_TRUE;
}

zest_bool zest__dx_bind_image_arena(zest_device device, zest_image image, zest_device_memory_pool backing, zest_size offset) {
	zest_dx_device_backend_t *device_backend = ZEST_DX_BACKEND(device, device);
	zest_dx_image_backend_t *backend = ZEST_DX_BACKEND(image, image);
	D3D12_RESOURCE_DESC1 description;
	zest__dx_texture_description(image, image->info.layer_count, backend->sample_count, backend->resource_format, backend->resource_flags, &description);
	ID3D12Resource *resource = NULL;
	ZEST_DX_RETURN_FALSE_ON_FAIL(device, ZEST_DX_CALL(device_backend->device10, CreatePlacedResource2, ZEST_DX_BACKEND(device_memory_pool, backing)->heap, offset, &description,
		D3D12_BARRIER_LAYOUT_UNDEFINED, NULL, 0, NULL, ZEST_DX_IID(ID3D12Resource), (void **)&resource));
	backend->resource = resource;
	backend->layout = D3D12_BARRIER_LAYOUT_UNDEFINED;
	return ZEST_TRUE;
}

zest_sample_count_flags zest__dx_get_msaa_sample_count(zest_context context) { zest__dx_report_unavailable(context->device, "MSAA is not available on the D3D12 backend yet."); return zest_sample_count_1_bit; }
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
		zest_imm_CopyBufferRegionsToImage(queue, &region, 1, staging_buffer, dst_image);
	if (recorded && dst_image->info.mip_levels > 1) {
		recorded = zest__dx_generate_mipmaps(queue, dst_image);
	}
	//Mip generation already leaves the image in shader read, so after it this barrier only syncs, as on Vulkan
	recorded = recorded && zest_imm_TransitionImage(queue, dst_image, zest_resource_state_shader_read, 0, dst_image->info.mip_levels, 0, dst_image->info.layer_count);
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
	return zest__dx_read_subresource(device, src_image, 0, 0, pixels);
}

//Reads the first depth slice of one mip of one layer, tightly packed
zest_bool zest__dx_read_subresource(zest_device device, zest_image image, zest_uint mip, zest_uint layer, void *pixels) {
	zest_dx_device_backend_t *device_backend = ZEST_DX_BACKEND(device, device);
	ID3D12Resource *texture = ZEST_DX_BACKEND(image, image)->resource;
	D3D12_RESOURCE_DESC1 description = zest__dx_resource_description(texture);
	UINT subresource = mip + layer * description.MipLevels;
	D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint = ZEST__ZERO_INIT(D3D12_PLACED_SUBRESOURCE_FOOTPRINT);
	UINT row_count = 0;
	UINT64 row_size = 0;
	UINT64 total_size = 0;
	ZEST_DX_CALL(device_backend->device10, GetCopyableFootprints1, &description, subresource, 1, 0, &footprint, &row_count, &row_size, &total_size);

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
	zest_image_layout original_layout = image->layout;
	D3D12_BARRIER_LAYOUT original_backend_layout = ZEST_DX_BACKEND(image, image)->layout;
	zest__dx_transition_image_layout(queue, image, zest_image_layout_transfer_src_optimal, NULL, mip, 1, layer, 1);
	D3D12_TEXTURE_COPY_LOCATION source = ZEST__ZERO_INIT(D3D12_TEXTURE_COPY_LOCATION);
	source.pResource = texture;
	source.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
	source.SubresourceIndex = subresource;
	D3D12_TEXTURE_COPY_LOCATION destination = ZEST__ZERO_INIT(D3D12_TEXTURE_COPY_LOCATION);
	destination.pResource = ZEST_DX_BACKEND(device_memory_pool, readback_buffer->memory_pool)->buffer;
	destination.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
	destination.PlacedFootprint = footprint;
	ZEST_DX_CALL(ZEST_DX_BACKEND(queue, queue)->command_list, CopyTextureRegion, &destination, 0, 0, 0, &source, NULL);
	zest__dx_transition_image_layout(queue, image, original_layout, &original_backend_layout, mip, 1, layer, 1);
	if (!zest_imm_EndCommandBuffer(queue)) {
		zest_FreeBuffer(readback_buffer);
		return ZEST_FALSE;
	}

	zest__dx_repack_rows((const char *)readback_buffer->memory_pool->mapped + footprint.Offset, footprint.Footprint.RowPitch, pixels, (zest_size)row_size, (zest_size)row_size, row_count);
	zest_FreeBufferNow(readback_buffer);
	return ZEST_TRUE;
}
// -- End General_helpers

// -- Internal_Frame_graph_context_functions
void *zest__dx_new_execution_backend(zloc_linear_allocator_t *allocator) {
	zest_dx_execution_backend_t *backend = (zest_dx_execution_backend_t *)zest__linear_allocate(allocator, sizeof(zest_dx_execution_backend_t));
	*backend = ZEST__ZERO_INIT(zest_dx_execution_backend_t);
	return backend;
}

zest_frame_graph_semaphores zest__dx_get_frame_graph_semaphores(zest_context context, const char *name) {
	if (zest_map_valid_name(context->cached_frame_graph_semaphores, name)) {
		return *zest_map_at(context->cached_frame_graph_semaphores, name);
	}
	zest_device device = context->device;
	ID3D12Device *native_device = ZEST_DX_BACKEND(device, device)->device;
	zest_frame_graph_semaphores semaphores = (zest_frame_graph_semaphores)ZEST__NEW(context->allocator, zest_frame_graph_semaphores);
	*semaphores = ZEST__ZERO_INIT(zest_frame_graph_semaphores_t);
	semaphores->magic = zest_INIT_MAGIC(zest_struct_type_frame_graph_semaphores);
	zest_dx_frame_graph_semaphores_backend_t *backend = (zest_dx_frame_graph_semaphores_backend_t *)zest__dx_new_frame_graph_semaphores_backend(context);
	semaphores->backend = (zest_frame_graph_semaphores_backend)backend;
	zest_ForEachFrameInFlight(frame_in_flight) {
		for (zest_uint queue_index = 0; queue_index != ZEST_QUEUE_COUNT; ++queue_index) {
			HRESULT result = ZEST_DX_CALL(native_device, CreateFence, 0, D3D12_FENCE_FLAG_NONE, ZEST_DX_IID(ID3D12Fence), (void **)&backend->fences[frame_in_flight][queue_index]);
			if (FAILED(result)) {
				//A batch that would signal a missing fence fails its submission instead
				backend->fences[frame_in_flight][queue_index] = NULL;
				ZEST_DX_BACKEND(context, context)->last_result = result;
				zest__dx_log_hresult(device, result, __FILE__, __LINE__);
			}
		}
	}
	zest_map_insert(context->allocator, context->cached_frame_graph_semaphores, name, semaphores);
	return semaphores;
}

void zest__dx_add_frame_graph_image_barrier(zest_resource_node resource, zest_execution_barriers_t *barriers, zest_bool acquire, zest_access_flags src_access, zest_access_flags dst_access, zest_image_layout old_layout, zest_image_layout new_layout, zest_uint src_family, zest_uint dst_family, zest_pipeline_stage_flags src_stage, zest_pipeline_stage_flags dst_stage) {
	zest_context context = resource->frame_graph->command_list.context;
	zloc_linear_allocator_t *allocator = &context->frame_graph_allocator[context->current_fif];
	zest_dx_execution_barriers_backend_t *backend = ZEST_DX_BACKEND(execution_barriers, barriers);
	zest_dx_frame_graph_image_barrier_t record;
	record.resource = resource;
	record.src_access = src_access;
	record.dst_access = dst_access;
	record.old_layout = old_layout;
	record.new_layout = new_layout;
	record.src_stage = src_stage;
	record.dst_stage = dst_stage;
	record.src_family = src_family;
	record.dst_family = dst_family;
	D3D12_TEXTURE_BARRIER translated = ZEST__ZERO_INIT(D3D12_TEXTURE_BARRIER);
	if (acquire) {
		zest_vec_linear_push(allocator, backend->acquire_images, record);
		zest_vec_linear_push(allocator, backend->acquire_translated, translated);
		zest_vec_linear_push(allocator, barriers->acquire_image_barrier_nodes, resource);
	} else {
		zest_vec_linear_push(allocator, backend->release_images, record);
		zest_vec_linear_push(allocator, backend->release_translated, translated);
		zest_vec_linear_push(allocator, barriers->release_image_barrier_nodes, resource);
	}
}

void zest__dx_add_frame_graph_buffer_barrier(zest_resource_node resource, zest_execution_barriers_t *barriers, zest_bool acquire, zest_access_flags src_access, zest_access_flags dst_access, zest_uint src_family, zest_uint dst_family, zest_pipeline_stage_flags src_stage, zest_pipeline_stage_flags dst_stage) {
	zest__dx_report_unavailable(resource->frame_graph->command_list.context->device, "Frame graph buffer barriers are not available on the D3D12 backend yet.");
}

zest_bool zest__dx_set_next_command_buffer(const zest_command_list command_list, zest_context_queue queue) {
	zest_context context = command_list->context;
	zest_dx_context_queue_backend_t *queue_backend = ZEST_DX_BACKEND(context_queue, queue);
	zest_dx_command_list_backend_t *backend = ZEST_DX_BACKEND(command_list, command_list);
	zest_uint family_index = queue->queue_manager->family_index;
	if (!queue_backend->command_list) {
		zest_dx_context_backend_t *context_backend = ZEST_DX_BACKEND(context, context);
		//Created closed, so the first begin resets it the same as every later one
		context_backend->last_result = ZEST_DX_CALL(ZEST_DX_BACKEND(device, context->device)->device10, CreateCommandList1, 0, zest__dx_queue_families[family_index].type,
			D3D12_COMMAND_LIST_FLAG_NONE, ZEST_DX_IID(ID3D12GraphicsCommandList7), (void **)&queue_backend->command_list);
		if (FAILED(context_backend->last_result)) {
			queue_backend->command_list = NULL;
			zest__dx_log_hresult(context->device, context_backend->last_result, __FILE__, __LINE__);
			zest__dx_flag_device_lost(context, context_backend->last_result);
			return ZEST_FALSE;
		}
	}
	backend->command_list = queue_backend->command_list;
	backend->context_queue = queue;
	backend->queue_type = zest__dx_family_queue_type(family_index);
	return ZEST_TRUE;
}

zest_bool zest__dx_begin_command_buffer(const zest_command_list command_list) {
	zest_context context = command_list->context;
	zest_device device = context->device;
	zest_dx_context_backend_t *context_backend = ZEST_DX_BACKEND(context, context);
	zest_dx_command_list_backend_t *backend = ZEST_DX_BACKEND(command_list, command_list);
	ID3D12CommandAllocator *command_allocator = ZEST_DX_BACKEND(context_queue, backend->context_queue)->command_allocators[context->current_fif];
	//Missing when the context queue's initialisation failed, which was logged then
	if (!command_allocator) return ZEST_FALSE;
	context_backend->last_result = ZEST_DX_CALL(backend->command_list, Reset, command_allocator, NULL);
	if (FAILED(context_backend->last_result)) {
		zest__dx_log_hresult(device, context_backend->last_result, __FILE__, __LINE__);
		zest__dx_flag_device_lost(context, context_backend->last_result);
		return ZEST_FALSE;
	}
	backend->close_result = S_OK;
	backend->render_pass_open = ZEST_FALSE;
	backend->bound_sets = NULL;
	backend->bound_set_count = 0;
	if (backend->queue_type != zest_queue_transfer && device->bindless_set) {
		zest__dx_bind_bindless_heaps(device, backend->command_list);
	}
	return ZEST_TRUE;
}

void zest__dx_end_command_buffer(const zest_command_list command_list) {
	zest_dx_command_list_backend_t *backend = ZEST_DX_BACKEND(command_list, command_list);
	backend->close_result = ZEST_DX_CALL0(backend->command_list, Close);
	if (FAILED(backend->close_result)) {
		zest__dx_log_hresult(command_list->context->device, backend->close_result, __FILE__, __LINE__);
	}
}

//A transient's first use starts over, as a cached graph's reused image may hold bytes another transient wrote since its tracked layout was set
D3D12_BARRIER_LAYOUT zest__dx_record_layout_before(const zest_dx_frame_graph_image_barrier_t *record, zest_image image) {
	if (record->old_layout == zest_image_layout_undefined && ZEST__FLAGGED(record->resource->flags, zest_resource_node_flag_transient)) {
		return D3D12_BARRIER_LAYOUT_UNDEFINED;
	}
	return ZEST_DX_BACKEND(image, image)->layout;
}

//Translates a pass group's records into the arrays sized when the graph was compiled, then sends them in one group
void zest__dx_send_image_barriers(const zest_command_list command_list, const zest_dx_frame_graph_image_barrier_t *records, D3D12_TEXTURE_BARRIER *translated) {
	zest_uint barrier_count = zest_vec_size(records);
	if (!barrier_count) return;
	zest_dx_command_list_backend_t *backend = ZEST_DX_BACKEND(command_list, command_list);
	for (zest_uint index = 0; index != barrier_count; ++index) {
		const zest_dx_frame_graph_image_barrier_t *record = &records[index];
		zest_resource_node resource = record->resource;
		zest_image image = resource->view->image;
		ZEST_ASSERT(image && image->backend, "The resource has no image to transition. If it's not transient then set a resource provider callback for it.");
		D3D12_BARRIER_LAYOUT layout_after = zest__dx_barrier_layout(record->new_layout, record->dst_access, backend->queue_type);
		translated[index] = zest__dx_subresource_barrier(image, zest__dx_record_layout_before(record, image), layout_after, record->src_access, record->dst_access, record->src_stage, record->dst_stage,
			0, resource->image.info.mip_levels, 0, resource->image.info.layer_count);
	}
	D3D12_BARRIER_GROUP group = ZEST__ZERO_INIT(D3D12_BARRIER_GROUP);
	group.Type = D3D12_BARRIER_TYPE_TEXTURE;
	group.NumBarriers = barrier_count;
	group.pTextureBarriers = translated;
	ZEST_DX_CALL(backend->command_list, Barrier, 1, &group);
	for (zest_uint index = 0; index != barrier_count; ++index) {
		const zest_dx_frame_graph_image_barrier_t *record = &records[index];
		zest_resource_node resource = record->resource;
		zest_image image = resource->view->image;
		ZEST_DX_BACKEND(image, image)->layout = translated[index].LayoutAfter;
		image->layout = record->new_layout;
		if (resource->linked_layout) {
			*resource->linked_layout = record->new_layout;
			resource->image_layout = record->new_layout;
		}
	}
}

void zest__dx_acquire_barrier(const zest_command_list command_list, zest_execution_details_t *exe_details) {
	zest_dx_execution_barriers_backend_t *backend = ZEST_DX_BACKEND(execution_barriers, &exe_details->barriers);
	zest__dx_send_image_barriers(command_list, backend->acquire_images, backend->acquire_translated);
}

void zest__dx_release_barrier(const zest_command_list command_list, zest_execution_details_t *exe_details) {
	zest_dx_execution_barriers_backend_t *backend = ZEST_DX_BACKEND(execution_barriers, &exe_details->barriers);
	zest__dx_send_image_barriers(command_list, backend->release_images, backend->release_translated);
}

zest_bool zest__dx_begin_render_pass(const zest_command_list command_list, zest_execution_details_t *exe_details) {
	zest_context context = command_list->context;
	zest_dx_command_list_backend_t *backend = ZEST_DX_BACKEND(command_list, command_list);
	zest_uint color_attachment_count = zest_vec_size(exe_details->color_attachments);
	ZEST_ASSERT(color_attachment_count <= ZEST_MAX_ATTACHMENTS, "A render pass has more color attachments than ZEST_MAX_ATTACHMENTS.");
	zest_bool has_resolve = exe_details->depth_attachment.resolve_image_view != NULL;
	for (zest_uint attachment_index = 0; attachment_index != color_attachment_count; ++attachment_index) {
		has_resolve |= exe_details->color_attachments[attachment_index].resolve_image_view != NULL;
	}
	if (has_resolve) {
		return zest__dx_report_unavailable(context->device, "Resolve attachments are not available on the D3D12 backend yet.");
	}

	D3D12_RENDER_PASS_RENDER_TARGET_DESC render_targets[ZEST_MAX_ATTACHMENTS];
	for (zest_uint attachment_index = 0; attachment_index != color_attachment_count; ++attachment_index) {
		zest_rendering_attachment_info_t *info = &exe_details->color_attachments[attachment_index];
		zest_image_view view = *info->image_view;
		D3D12_RENDER_PASS_RENDER_TARGET_DESC *render_target = &render_targets[attachment_index];
		*render_target = ZEST__ZERO_INIT(D3D12_RENDER_PASS_RENDER_TARGET_DESC);
		render_target->cpuDescriptor = ZEST_DX_BACKEND(image_view, view)->rtv;
		ZEST_ASSERT(render_target->cpuDescriptor.ptr, "A color attachment's view has no render target view, so its image wasn't created with zest_image_flag_color_attachment.");
		render_target->BeginningAccess.Type = zest__dx_beginning_access(info->usage.load_op);
		render_target->EndingAccess.Type = zest__dx_ending_access(info->usage.store_op);
		if (render_target->BeginningAccess.Type == D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_CLEAR) {
			D3D12_CLEAR_VALUE *clear_value = &render_target->BeginningAccess.Clear.ClearValue;
			clear_value->Format = zest__dx_to_dxgi_format(view->image->info.format);
			//The swapchain's colour is read now, so a change made after the graph was cached still applies
			zest_vec4 color = info->is_swapchain_image ? context->swapchain->clear_color.color : info->resource->clear_color;
			clear_value->Color[0] = color.r;
			clear_value->Color[1] = color.g;
			clear_value->Color[2] = color.b;
			clear_value->Color[3] = color.a;
		}
	}

	zest_bool has_depth = exe_details->depth_attachment.image_view != NULL;
	D3D12_RENDER_PASS_DEPTH_STENCIL_DESC depth_stencil = ZEST__ZERO_INIT(D3D12_RENDER_PASS_DEPTH_STENCIL_DESC);
	if (has_depth) {
		zest_rendering_attachment_info_t *info = &exe_details->depth_attachment;
		zest_image_view view = *info->image_view;
		depth_stencil.cpuDescriptor = ZEST_DX_BACKEND(image_view, view)->dsv;
		ZEST_ASSERT(depth_stencil.cpuDescriptor.ptr, "The depth attachment's view has no depth stencil view, so its image wasn't created with zest_image_flag_depth_stencil_attachment.");
		depth_stencil.DepthBeginningAccess.Type = zest__dx_beginning_access(info->usage.load_op);
		depth_stencil.DepthEndingAccess.Type = zest__dx_ending_access(info->usage.store_op);
		if (depth_stencil.DepthBeginningAccess.Type == D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_CLEAR) {
			D3D12_CLEAR_VALUE *clear_value = &depth_stencil.DepthBeginningAccess.Clear.ClearValue;
			clear_value->Format = zest__dx_to_dxgi_format(view->image->info.format);
			clear_value->DepthStencil.Depth = info->clear_value.depth_stencil.depth;
			clear_value->DepthStencil.Stencil = (UINT8)info->clear_value.depth_stencil.stencil;
		}
		//Vulkan's dynamic rendering only attaches the depth plane, so the stencil plane is left untouched
		depth_stencil.StencilBeginningAccess.Type = D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_NO_ACCESS;
		depth_stencil.StencilEndingAccess.Type = D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_NO_ACCESS;
	}

	ZEST_DX_CALL(backend->command_list, BeginRenderPass, color_attachment_count, color_attachment_count ? render_targets : NULL, has_depth ? &depth_stencil : NULL, D3D12_RENDER_PASS_FLAG_NONE);
	backend->render_pass_open = ZEST_TRUE;
	return ZEST_TRUE;
}

//Shared code calls this even when begin_render_pass failed, so only a pass that began is ended
void zest__dx_end_render_pass(const zest_command_list command_list) {
	zest_dx_command_list_backend_t *backend = ZEST_DX_BACKEND(command_list, command_list);
	if (!backend->render_pass_open) return;
	ZEST_DX_CALL0(backend->command_list, EndRenderPass);
	backend->render_pass_open = ZEST_FALSE;
}

zest_bool zest__dx_submit_frame_graph_batch(zest_frame_graph frame_graph, zest_execution_backend backend, zest_submission_batch_t *batch, zest_map_queue_value *queues) {
	zest_context context = frame_graph->command_list.context;
	zest_device device = context->device;
	zest_dx_context_backend_t *context_backend = ZEST_DX_BACKEND(context, context);
	zest_dx_execution_backend_t *execution_backend = (zest_dx_execution_backend_t *)backend;
	zest_dx_command_list_backend_t *command_list_backend = ZEST_DX_BACKEND(command_list, &frame_graph->command_list);
	zest_dx_frame_graph_semaphores_backend_t *semaphores_backend = ZEST_DX_BACKEND(frame_graph_semaphores, frame_graph->semaphores);
	zest_uint queue_index = frame_graph->command_list.queue_index;
	zest_uint submission_index = frame_graph->command_list.submission_index;
	zest_bool last_wave = submission_index == zest_vec_size(frame_graph->submissions) - 1;
	zest_context_queue context_queue = batch->queue;
	ID3D12Fence *batch_fence = semaphores_backend->fences[context->current_fif][queue_index];

	//A list that failed to close can't be reset, so it's released and the next set_next_command_buffer creates a new one
	if (FAILED(command_list_backend->close_result)) {
		zest__dx_flag_device_lost(context, command_list_backend->close_result);
		ZEST_DX_RELEASE(ZEST_DX_BACKEND(context_queue, context_queue)->command_list);
		command_list_backend->command_list = NULL;
		return ZEST_FALSE;
	}
	//get_frame_graph_semaphores logged why the fence is missing
	if (!last_wave && !batch_fence) {
		return ZEST_FALSE;
	}

	//Records which queues this graph used so their frame in flight indexes cycle at the end of execution
	zest_map_insert_linear_key(&context->frame_graph_allocator[context->current_fif], (*queues), (zest_key)context_queue, context_queue);
	zest__acquire_context_queue(context, context_queue, queue_index);
	ID3D12CommandQueue *native_queue = ZEST_DX_BACKEND(queue, context_queue->queue)->queue;
	//D3D12 only lets a list that writes a back buffer run on the swapchain's own queue
	if (batch->outputs_to_swapchain && context_backend->present_queue) {
		native_queue = context_backend->present_queue;
	}
	zest_execution_timeline ordering_timeline = &context->queue_ordering_timelines[queue_index];
	ID3D12Fence *ordering_fence = ZEST_DX_BACKEND(execution_timeline, ordering_timeline)->fence;
	//Taken from the queues themselves, as the acquire only reports a switch when the preferred queue was busy and knows nothing of the reroute
	ID3D12CommandQueue *ordering_queue = context_backend->ordering_queues[queue_index];
	zest_bool switched_queue = ordering_queue && ordering_queue != native_queue;

	//A D3D12 queue wait covers all of the queue's work, so the Vulkan wait stages have nothing to map to
	HRESULT result = S_OK;
	for (zest_uint wait_index = 0; wait_index != execution_backend->wave_wait_count && SUCCEEDED(result); ++wait_index) {
		result = ZEST_DX_CALL(native_queue, Wait, execution_backend->wave_wait_fences[wait_index], execution_backend->wave_wait_values[wait_index]);
	}
	zest_execution_timeline wait_timeline = frame_graph->wait_on_timeline;
	if (SUCCEEDED(result) && submission_index == 0 && wait_timeline && wait_timeline->current_value > 0) {
		result = ZEST_DX_CALL(native_queue, Wait, ZEST_DX_BACKEND(execution_timeline, wait_timeline)->fence, wait_timeline->current_value);
	}
	//Orders this submission after the slot's last one when that ran on a different D3D12 queue
	if (SUCCEEDED(result) && switched_queue && ordering_timeline->current_value > 0) {
		result = ZEST_DX_CALL(native_queue, Wait, ordering_fence, ordering_timeline->current_value);
	}
	if (FAILED(result)) {
		context_backend->last_result = result;
		zest__dx_log_hresult(device, result, __FILE__, __LINE__);
		zest__dx_flag_device_lost(context, result);
		return ZEST_FALSE;
	}

	ID3D12CommandList *command_lists[1] = { (ID3D12CommandList *)command_list_backend->command_list };
	ZEST_DX_CALL(native_queue, ExecuteCommandLists, 1, command_lists);

	//The last wave signals the timeline the CPU waits on, every earlier wave the fence the next wave waits on
	ID3D12Fence *signal_fence = NULL;
	if (last_wave) {
		zest_execution_timeline signal_timeline = frame_graph->signal_timeline;
		if (signal_timeline) {
			signal_fence = ZEST_DX_BACKEND(execution_timeline, signal_timeline)->fence;
			context->frame_sync_timeline[context->current_fif] = signal_timeline;
			signal_timeline->current_value += 1;
			result = zest__dx_forced_signal_failure ? E_OUTOFMEMORY : ZEST_DX_CALL(native_queue, Signal, signal_fence, signal_timeline->current_value);
			if (FAILED(result)) {
				//Rolled back so later waits target a value that will actually be signalled
				signal_timeline->current_value -= 1;
			}
		}
	} else {
		signal_fence = batch_fence;
		zest_size *batch_value = &frame_graph->semaphores->values[context->current_fif][queue_index];
		*batch_value += 1;
		result = zest__dx_forced_signal_failure ? E_OUTOFMEMORY : ZEST_DX_CALL(native_queue, Signal, batch_fence, *batch_value);
		if (FAILED(result)) {
			*batch_value -= 1;
		}
	}
	//The list ran on this queue either way, so the slot's ordering is signalled even when the signal above failed
	ordering_timeline->current_value += 1;
	HRESULT ordering_result = ZEST_DX_CALL(native_queue, Signal, ordering_fence, ordering_timeline->current_value);
	if (FAILED(ordering_result)) {
		ordering_timeline->current_value -= 1;
	} else {
		context_backend->ordering_queues[queue_index] = native_queue;
	}
	result = FAILED(result) ? result : ordering_result;
	if (FAILED(result)) {
		context_backend->last_result = result;
		zest__dx_log_hresult(device, result, __FILE__, __LINE__);
		zest__dx_flag_device_lost(context, result);
		//The list is already executing with no signal left to cover it, and shared cleanup only drains when an earlier batch succeeded
		zest_WaitForIdleDevice(device);
		return ZEST_FALSE;
	}
	ZEST__FLAG(context->flags, zest_context_flag_work_was_submitted);
	ZEST_DX_BACKEND(submission_batch, batch)->signal_fence = signal_fence;
	return ZEST_TRUE;
}

void zest__dx_carry_over_semaphores(zest_frame_graph frame_graph, zest_wave_submission_t *wave_submission, zest_execution_backend backend) {
	zest_context context = frame_graph->command_list.context;
	zest_dx_execution_backend_t *execution_backend = (zest_dx_execution_backend_t *)backend;
	zest_dx_frame_graph_semaphores_backend_t *semaphores_backend = ZEST_DX_BACKEND(frame_graph_semaphores, frame_graph->semaphores);
	execution_backend->wave_wait_count = 0;
	for (zest_uint queue_index = 0; queue_index != ZEST_QUEUE_COUNT; ++queue_index) {
		ID3D12Fence *fence = semaphores_backend->fences[context->current_fif][queue_index];
		if (!wave_submission->batches[queue_index].magic || !fence) continue;
		execution_backend->wave_wait_fences[execution_backend->wave_wait_count] = fence;
		execution_backend->wave_wait_values[execution_backend->wave_wait_count] = frame_graph->semaphores->values[context->current_fif][queue_index];
		execution_backend->wave_wait_count++;
	}
}
// -- End Internal_Frame_graph_context_functions

// -- Frame_graph_context_functions
//Kept until the next draw or dispatch, which binds them once the pipeline's root signature is known
void zest__dx_bind_descriptor_sets(const zest_command_list command_list, zest_pipeline_bind_point bind_point, zest_pipeline_layout layout, zest_descriptor_set *descriptor_sets, zest_uint set_count, zest_uint first_set) {
	zest_dx_command_list_backend_t *backend = ZEST_DX_BACKEND(command_list, command_list);
	backend->bound_sets = descriptor_sets;
	backend->bound_set_count = set_count;
	backend->bound_point = bind_point;
}

void zest__dx_set_screensized_viewport(const zest_command_list command_list, float min_depth, float max_depth) {
	//Must be called from a pass callback of a graph that renders to the swapchain
	zest_swapchain swapchain = command_list->frame_graph->swapchain;
	ID3D12GraphicsCommandList7 *native_list = ZEST_DX_BACKEND(command_list, command_list)->command_list;
	D3D12_VIEWPORT viewport = zest__dx_viewport(0.f, 0.f, (float)swapchain->size.width, (float)swapchain->size.height, min_depth, max_depth);
	D3D12_RECT scissor = { 0, 0, (LONG)swapchain->size.width, (LONG)swapchain->size.height };
	ZEST_DX_CALL(native_list, RSSetViewports, 1, &viewport);
	ZEST_DX_CALL(native_list, RSSetScissorRects, 1, &scissor);
}

void zest__dx_scissor(const zest_command_list command_list, zest_scissor_rect_t *scissor) {
	D3D12_RECT rect = { (LONG)scissor->offset.x, (LONG)scissor->offset.y, (LONG)scissor->offset.x + (LONG)scissor->extent.width, (LONG)scissor->offset.y + (LONG)scissor->extent.height };
	ZEST_DX_CALL(ZEST_DX_BACKEND(command_list, command_list)->command_list, RSSetScissorRects, 1, &rect);
}

void zest__dx_view_port(const zest_command_list command_list, zest_viewport_t *viewport) {
	D3D12_VIEWPORT native_viewport = zest__dx_viewport(viewport->x, viewport->y, viewport->width, viewport->height, viewport->minDepth, viewport->maxDepth);
	ZEST_DX_CALL(ZEST_DX_BACKEND(command_list, command_list)->command_list, RSSetViewports, 1, &native_viewport);
}

void zest__dx_clip(const zest_command_list command_list, float x, float y, float width, float height, float min_depth, float max_depth) {
	ID3D12GraphicsCommandList7 *native_list = ZEST_DX_BACKEND(command_list, command_list)->command_list;
	D3D12_VIEWPORT viewport = zest__dx_viewport(x, y, width, height, min_depth, max_depth);
	D3D12_RECT scissor = { (LONG)x, (LONG)y, (LONG)x + (LONG)width, (LONG)y + (LONG)height };
	ZEST_DX_CALL(native_list, RSSetViewports, 1, &viewport);
	ZEST_DX_CALL(native_list, RSSetScissorRects, 1, &scissor);
}

//Moves one mip of layer 0 between the layout the frame graph left it in, with the access and stages its pass declared, and a command's
D3D12_TEXTURE_BARRIER zest__dx_node_mip_barrier(zest_resource_node node, zest_uint mip, D3D12_BARRIER_LAYOUT command_layout, zest_access_flags command_access, zest_pipeline_stage_flags command_stage, zest_bool to_command) {
	zest_image image = &node->image;
	zest_resource_usage_t *usage = &node->journey[node->current_state_index].usage;
	D3D12_BARRIER_LAYOUT tracked_layout = ZEST_DX_BACKEND(image, image)->layout;
	//An UNDEFINED layout only pairs with no access and no sync
	zest_bool undefined = tracked_layout == D3D12_BARRIER_LAYOUT_UNDEFINED;
	zest_access_flags tracked_access = undefined ? 0 : usage->access_mask;
	zest_pipeline_stage_flags tracked_stage = undefined ? 0 : usage->stage_mask;
	if (to_command) {
		return zest__dx_subresource_barrier(image, tracked_layout, command_layout, tracked_access, command_access, tracked_stage, command_stage, mip, 1, 0, 1);
	}
	return zest__dx_subresource_barrier(image, command_layout, tracked_layout, command_access, tracked_access, command_stage, tracked_stage, mip, 1, 0, 1);
}

//CopyTextureRegion works on every queue type, and on the copy queue COMMON stands in for both copy layouts
void zest__dx_record_mip_copy(const zest_command_list command_list, zest_resource_node src, zest_resource_node dst, zest_uint mip) {
	zest_dx_command_list_backend_t *backend = ZEST_DX_BACKEND(command_list, command_list);
	D3D12_BARRIER_LAYOUT source_layout = zest__dx_barrier_layout(zest_image_layout_transfer_src_optimal, zest_access_transfer_read_bit, backend->queue_type);
	D3D12_BARRIER_LAYOUT destination_layout = zest__dx_barrier_layout(zest_image_layout_transfer_dst_optimal, zest_access_transfer_write_bit, backend->queue_type);
	D3D12_TEXTURE_BARRIER barriers[2];
	barriers[0] = zest__dx_node_mip_barrier(src, mip, source_layout, zest_access_transfer_read_bit, zest_pipeline_stage_transfer_bit, ZEST_TRUE);
	barriers[1] = zest__dx_node_mip_barrier(dst, mip, destination_layout, zest_access_transfer_write_bit, zest_pipeline_stage_transfer_bit, ZEST_TRUE);
	zest__dx_send_changing_barriers(backend->command_list, barriers, 2);
	D3D12_TEXTURE_COPY_LOCATION source = ZEST__ZERO_INIT(D3D12_TEXTURE_COPY_LOCATION);
	source.pResource = ZEST_DX_BACKEND(image, &src->image)->resource;
	source.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
	source.SubresourceIndex = mip;
	D3D12_TEXTURE_COPY_LOCATION destination = ZEST__ZERO_INIT(D3D12_TEXTURE_COPY_LOCATION);
	destination.pResource = ZEST_DX_BACKEND(image, &dst->image)->resource;
	destination.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
	destination.SubresourceIndex = mip;
	ZEST_DX_CALL(backend->command_list, CopyTextureRegion, &destination, 0, 0, 0, &source, NULL);
	barriers[0] = zest__dx_node_mip_barrier(src, mip, source_layout, zest_access_transfer_read_bit, zest_pipeline_stage_transfer_bit, ZEST_FALSE);
	barriers[1] = zest__dx_node_mip_barrier(dst, mip, destination_layout, zest_access_transfer_write_bit, zest_pipeline_stage_transfer_bit, ZEST_FALSE);
	zest__dx_send_changing_barriers(backend->command_list, barriers, 2);
}

//read_by_stages isn't used, as the return barriers go back to the pass's declared stages, which are the ones its queue can sync
void zest__dx_blit_image_mip(const zest_command_list command_list, zest_resource_node src, zest_resource_node dst, zest_uint mip_to_blit, zest_supported_shader_stages read_by_stages) {
	zest_image source = &src->image;
	zest_image destination = &dst->image;
	//Vulkan's version is a nearest filtered blit between mips of the same size, which is a copy when the formats match
	if (ZEST_DX_BACKEND(image, source)->resource_format == ZEST_DX_BACKEND(image, destination)->resource_format) {
		zest__dx_record_mip_copy(command_list, src, dst, mip_to_blit);
		return;
	}
	zest_context context = command_list->context;
	zest_device device = context->device;
	zest_dx_command_list_backend_t *backend = ZEST_DX_BACKEND(command_list, command_list);
	if (backend->queue_type != zest_queue_graphics || backend->render_pass_open) {
		zest__dx_report_unavailable(device, "zest_cmd_BlitImageMip between different formats needs the graphics queue outside a render pass on D3D12.");
		return;
	}
	if (!zest__dx_blit_images_are_valid(device, source, destination) || !zest__dx_blit_pipeline(device, zest__dx_to_dxgi_format(destination->info.format))) {
		return;
	}
	zest_access_flags read_access, target_access;
	zest_pipeline_stage_flags read_stage, target_stage;
	zest__dx_layout_access_and_stage(zest_image_layout_shader_read_only_optimal, zest_queue_graphics, &read_access, &read_stage);
	zest__dx_layout_access_and_stage(zest_image_layout_color_attachment_optimal, zest_queue_graphics, &target_access, &target_stage);
	D3D12_TEXTURE_BARRIER barriers[2];
	barriers[0] = zest__dx_node_mip_barrier(src, mip_to_blit, D3D12_BARRIER_LAYOUT_SHADER_RESOURCE, read_access, read_stage, ZEST_TRUE);
	barriers[1] = zest__dx_node_mip_barrier(dst, mip_to_blit, D3D12_BARRIER_LAYOUT_RENDER_TARGET, target_access, target_stage, ZEST_TRUE);
	zest__dx_send_changing_barriers(backend->command_list, barriers, 2);
	//The draw covers the whole mip, and the debug layer doesn't count a first use's barrier discard as initialising a render target
	zest__dx_discard_subresource(backend->command_list, ZEST_DX_BACKEND(image, destination)->resource, mip_to_blit);
	zest_uint mip_width = ZEST__MAX(source->info.extent.width >> mip_to_blit, 1);
	zest_uint mip_height = ZEST__MAX(source->info.extent.height >> mip_to_blit, 1);
	D3D12_VIEWPORT viewport = zest__dx_viewport(0.f, 0.f, (float)mip_width, (float)mip_height, 0.f, 1.f);
	D3D12_RECT scissor = { 0, 0, (LONG)mip_width, (LONG)mip_height };
	const float full_rect[4] = { 0.f, 0.f, 1.f, 1.f };
	zest_dx_transient_descriptors_t *descriptors = &ZEST_DX_BACKEND(context_queue, backend->context_queue)->transient_descriptors[context->current_fif];
	zest__dx_record_blit(device, backend->command_list, descriptors, source, mip_to_blit, 0, full_rect, destination, mip_to_blit, 0, &viewport, &scissor, 0);
	barriers[0] = zest__dx_node_mip_barrier(src, mip_to_blit, D3D12_BARRIER_LAYOUT_SHADER_RESOURCE, read_access, read_stage, ZEST_FALSE);
	barriers[1] = zest__dx_node_mip_barrier(dst, mip_to_blit, D3D12_BARRIER_LAYOUT_RENDER_TARGET, target_access, target_stage, ZEST_FALSE);
	zest__dx_send_changing_barriers(backend->command_list, barriers, 2);
}

void zest__dx_copy_image_mip(const zest_command_list command_list, zest_resource_node src, zest_resource_node dst, zest_uint mip_to_copy, zest_supported_shader_stages read_by_stages) {
	if (zest__dx_typeless_format(ZEST_DX_BACKEND(image, &src->image)->resource_format) != zest__dx_typeless_format(ZEST_DX_BACKEND(image, &dst->image)->resource_format)) {
		zest__dx_report_unavailable(command_list->context->device, "zest_cmd_CopyImageMip needs both formats in the same typeless family on D3D12. Use zest_cmd_BlitImageMip to convert between formats.");
		return;
	}
	zest__dx_record_mip_copy(command_list, src, dst, mip_to_copy);
}

void zest__dx_insert_compute_image_barrier(const zest_command_list command_list, zest_resource_node resource, zest_uint base_mip) { zest__dx_report_unavailable(command_list->context->device, "zest_cmd_InsertComputeImageBarrier is not available on the D3D12 backend yet."); }
void zest__dx_bind_pipeline(const zest_command_list command_list, zest_pipeline pipeline) { zest__dx_report_unavailable(command_list->context->device, "zest_cmd_BindPipeline is not available on the D3D12 backend yet."); }
void zest__dx_bind_compute_pipeline(const zest_command_list command_list, zest_compute compute) { zest__dx_report_unavailable(command_list->context->device, "zest_cmd_BindComputePipeline is not available on the D3D12 backend yet."); }
void zest__dx_copy_buffer(const zest_command_list command_list, zest_buffer staging_buffer, zest_buffer device_buffer, zest_size size) {
	zest__dx_record_buffer_copy(ZEST_DX_BACKEND(command_list, command_list)->command_list, staging_buffer, staging_buffer->memory_offset, device_buffer, device_buffer->memory_offset, size);
}

zest_bool zest__dx_upload_buffer(const zest_command_list command_list, zest_buffer_uploader_t *uploader) {
	ID3D12GraphicsCommandList7 *native_list = ZEST_DX_BACKEND(command_list, command_list)->command_list;
	ID3D12Resource *source_resource = zest__dx_buffer_resource(uploader->source_buffer);
	ID3D12Resource *target_resource = zest__dx_buffer_resource(uploader->target_buffer);
	zest_vec_foreach(copy_index, uploader->buffer_copies) {
		const zest_buffer_copy_t *copy = &uploader->buffer_copies[copy_index];
		ZEST_DX_CALL(native_list, CopyBufferRegion, target_resource, copy->dst_offset, source_resource, copy->src_offset, copy->size);
	}
	zest_vec_clear(uploader->buffer_copies);
	uploader->flags = 0;
	uploader->source_buffer = 0;
	uploader->target_buffer = 0;
	return ZEST_TRUE;
}
void zest__dx_bind_vertex_buffer(const zest_command_list command_list, zest_uint first_binding, zest_uint binding_count, zest_buffer buffer) { zest__dx_report_unavailable(command_list->context->device, "zest_cmd_BindVertexBuffer is not available on the D3D12 backend yet."); }
void zest__dx_bind_index_buffer(const zest_command_list command_list, zest_buffer buffer) { zest__dx_report_unavailable(command_list->context->device, "zest_cmd_BindIndexBuffer is not available on the D3D12 backend yet."); }

//Clears mip 0 of layer 0 to zero
zest_bool zest__dx_image_clear(const zest_command_list command_list, zest_image image) {
	ZEST_ASSERT_HANDLE(image);	//Not a valid image handle
	zest_context context = command_list->context;
	zest_device device = context->device;
	zest_dx_command_list_backend_t *backend = ZEST_DX_BACKEND(command_list, command_list);
	if (backend->render_pass_open) {
		ZEST_ASSERT_OR_VALIDATE(ZEST_FALSE, device, "zest_cmd_ImageClear can't be called inside a render pass on the D3D12 backend. Clear the image with the pass's clear load op, or in a compute pass.", ZEST_FALSE);
		return ZEST_FALSE;
	}
	D3D12_BARRIER_LAYOUT clear_layout = D3D12_BARRIER_LAYOUT_DEPTH_STENCIL_WRITE;
	if (zest__is_depth_stencil_format(image->info.format)) {
		if (!zest__dx_depth_clear_is_valid(device, image, backend->queue_type)) {
			return ZEST_FALSE;
		}
	} else {
		clear_layout = zest__dx_color_clear_layout(device, image, backend->queue_type);
		if (clear_layout == D3D12_BARRIER_LAYOUT_UNDEFINED) {
			return ZEST_FALSE;
		}
	}
	//A frame graph node's image is a copy whose zest layout isn't kept current, so the clear waits on every earlier access instead
	zest_bool undefined = ZEST_DX_BACKEND(image, image)->layout == D3D12_BARRIER_LAYOUT_UNDEFINED;
	zest_access_flags tracked_access = undefined ? 0 : zest_access_memory_read_bit | zest_access_memory_write_bit;
	zest_pipeline_stage_flags tracked_stage = undefined ? 0 : zest_pipeline_stage_all_commands_bit;
	zest_clear_value_t zero_value = ZEST__ZERO_INIT(zest_clear_value_t);
	zest_dx_transient_descriptors_t *descriptors = &ZEST_DX_BACKEND(context_queue, backend->context_queue)->transient_descriptors[context->current_fif];
	return zest__dx_record_clear(device, backend->command_list, descriptors, image, clear_layout, tracked_access, tracked_stage, zero_value, 1, 1);
}

void zest__dx_bind_mesh_vertex_buffer(const zest_command_list command_list, zest_layer layer) { zest__dx_report_unavailable(command_list->context->device, "zest_cmd_BindMeshVertexBuffer is not available on the D3D12 backend yet."); }
void zest__dx_bind_mesh_index_buffer(const zest_command_list command_list, zest_layer layer) { zest__dx_report_unavailable(command_list->context->device, "zest_cmd_BindMeshIndexBuffer is not available on the D3D12 backend yet."); }
void zest__dx_dispatch_compute(const zest_command_list command_list, zest_uint group_count_x, zest_uint group_count_y, zest_uint group_count_z) { zest__dx_report_unavailable(command_list->context->device, "zest_cmd_DispatchCompute is not available on the D3D12 backend yet."); }
void zest__dx_send_push_constants(const zest_command_list command_list, zest_pipeline_layout layout, void *data, zest_uint size) { zest__dx_report_unavailable(command_list->context->device, "zest_cmd_SendPushConstants is not available on the D3D12 backend yet."); }
void zest__dx_draw(const zest_command_list command_list, zest_uint vertex_count, zest_uint instance_count, zest_uint first_vertex, zest_uint first_instance) { zest__dx_report_unavailable(command_list->context->device, "zest_cmd_Draw is not available on the D3D12 backend yet."); }
void zest__dx_draw_layer_instruction(const zest_command_list command_list, zest_uint vertex_count, zest_layer_instruction_t *instruction) { zest__dx_report_unavailable(command_list->context->device, "zest_cmd_DrawLayerInstruction is not available on the D3D12 backend yet."); }
void zest__dx_draw_indexed(const zest_command_list command_list, zest_uint index_count, zest_uint instance_count, zest_uint first_index, int32_t vertex_offset, zest_uint first_instance) { zest__dx_report_unavailable(command_list->context->device, "zest_cmd_DrawIndexed is not available on the D3D12 backend yet."); }
void zest__dx_draw_indexed_indirect(const zest_command_list command_list, zest_buffer buffer, zest_size offset, zest_uint draw_count, zest_uint stride) { zest__dx_report_unavailable(command_list->context->device, "zest_cmd_DrawIndexedIndirect is not available on the D3D12 backend yet."); }
void zest__dx_draw_indirect(const zest_command_list command_list, zest_buffer buffer, zest_size offset, zest_uint draw_count, zest_uint stride) { zest__dx_report_unavailable(command_list->context->device, "zest_cmd_DrawIndirect is not available on the D3D12 backend yet."); }
void zest__dx_set_depth_bias(const zest_command_list command_list, float factor, float clamp, float slope) { zest__dx_report_unavailable(command_list->context->device, "zest_cmd_SetDepthBias is not available on the D3D12 backend yet."); }
// -- End Frame_graph_context_functions

// -- Shader_compiling
zest_bool zest__dx_validate_shader(zest_device device, const char *shader_code, zest_shader_type type, const char *name) { return zest__dx_report_unavailable(device, "Shader validation is not available on the D3D12 backend yet."); }
zest_bool zest__dx_compile_shader(zest_shader shader, const char *code, zest_uint code_length, zest_shader_type type, const char *name, const char *entry_point, zest_shader_options options) { return zest__dx_report_unavailable((zest_device)shader->handle.store->origin, "Shader compilation is not available on the D3D12 backend yet."); }
zest_bool zest__dx_load_dxc(zest_device device) {
	zest_dx_device_backend_t *backend = ZEST_DX_BACKEND(device, device);
	if (ReadPointerAcquire((PVOID const volatile *)&backend->dxc_compiler)) return ZEST_TRUE;
	const char *compiler_library = zest__dx_forced_dxc_library ? zest__dx_forced_dxc_library : "dxcompiler.dll";
	const char *failed_library = NULL;
	AcquireSRWLockExclusive(&backend->dxc_lock);
	if (!backend->dxc_compiler) {
		//dxcompiler only signs its DXIL when dxil.dll loads first, and D3D12 rejects unsigned DXIL
		if (!backend->dxil_module) {
			backend->dxil_module = LoadLibraryA("dxil.dll");
		}
		if (!backend->dxil_module) {
			failed_library = "dxil.dll";
		} else if (!backend->dxc_module) {
			backend->dxc_module = LoadLibraryA(compiler_library);
			if (!backend->dxc_module) {
				failed_library = compiler_library;
			}
		}
		if (!failed_library) {
			zest_dxc_create_instance_function create_instance = (zest_dxc_create_instance_function)(void *)GetProcAddress(backend->dxc_module, "DxcCreateInstance");
			zest_dxc_compiler3_t *compiler = NULL;
			if (!create_instance || FAILED(create_instance(&zest__dx_clsid_DxcCompiler, &zest__dx_iid_IDxcCompiler3, (void **)&compiler))) {
				failed_library = compiler_library;
			} else {
				WritePointerRelease((PVOID volatile *)&backend->dxc_compiler, compiler);
			}
		}
		if (failed_library && backend->dxc_module) {
			FreeLibrary(backend->dxc_module);
			backend->dxc_module = NULL;
		}
	}
	ReleaseSRWLockExclusive(&backend->dxc_lock);
	if (failed_library) {
		char message[256];
		snprintf(message, sizeof(message), "Couldn't load %s, which compiling HLSL for D3D12 needs. Place it next to the executable.", failed_library);
		zest__log_validation_error(device, message);
		return ZEST_FALSE;
	}
	return ZEST_TRUE;
}

static zest_bool zest__dx_widen_ascii(const char *text, wchar_t *output, zest_uint capacity) {
	zest_uint length = 0;
	while (text[length]) {
		if (length + 1 >= capacity) return ZEST_FALSE;
		output[length] = (wchar_t)(unsigned char)text[length];
		length++;
	}
	output[length] = 0;
	return ZEST_TRUE;
}

zest_bool zest__dx_compile_hlsl(zest_device device, const char *source, zest_size source_length, const char *name, const char *entry_point, const char *profile, ID3DBlob **dxil) {
	*dxil = NULL;
	if (!zest__dx_load_dxc(device)) return ZEST_FALSE;
	zest_dx_device_backend_t *backend = ZEST_DX_BACKEND(device, device);
	wchar_t wide_entry_point[ZEST__DX_DXC_ARGUMENT_LENGTH];
	wchar_t wide_profile[ZEST__DX_DXC_ARGUMENT_LENGTH];
	if (!zest__dx_widen_ascii(entry_point, wide_entry_point, ZEST__DX_DXC_ARGUMENT_LENGTH) || !zest__dx_widen_ascii(profile, wide_profile, ZEST__DX_DXC_ARGUMENT_LENGTH)) {
		ZEST_APPEND_LOG(device->log_path.str, "Shader compilation failed: %s, the entry point or profile is too long", name);
		ZEST_ALERT("Shader compilation failed: %s, the entry point or profile is too long", name);
		return ZEST_FALSE;
	}
	LPCWSTR arguments[6] = { L"-E", wide_entry_point, L"-T", wide_profile };
	UINT32 argument_count = 4;
	if (ZEST__FLAGGED(device->setup_info.flags, zest_device_init_flag_enable_validation_layers)) {
		arguments[argument_count++] = L"-Zi";
		arguments[argument_count++] = L"-Qembed_debug";
	}
	zest_dxc_buffer_t source_buffer;
	source_buffer.Ptr = source;
	source_buffer.Size = (SIZE_T)source_length;
	source_buffer.Encoding = ZEST__DX_DXC_UTF8;
	zest_dxc_result_t *result = NULL;
	HRESULT status = ZEST_DXC_CALL(backend->dxc_compiler, Compile, &source_buffer, arguments, argument_count, NULL, &zest__dx_iid_IDxcResult, (void **)&result);
	if (SUCCEEDED(status) && result) {
		status = E_FAIL;
		HRESULT compile_status = E_FAIL;
		if (SUCCEEDED(ZEST_DXC_CALL(result, GetStatus, &compile_status)) && SUCCEEDED(compile_status)) {
			ID3DBlob *blob = NULL;
			if (SUCCEEDED(ZEST_DXC_CALL(result, GetResult, &blob)) && blob) {
				*dxil = blob;
				status = S_OK;
			}
		}
	}
	if (FAILED(status)) {
		ID3DBlob *errors = NULL;
		const char *error_text = "no error message from DXC";
		int error_length = (int)strlen(error_text);
		if (result && SUCCEEDED(ZEST_DXC_CALL(result, GetErrorBuffer, &errors)) && errors && ZEST_DX_CALL0(errors, GetBufferSize) > 0) {
			error_text = (const char *)ZEST_DX_CALL0(errors, GetBufferPointer);
			error_length = (int)ZEST_DX_CALL0(errors, GetBufferSize);
		}
		ZEST_APPEND_LOG(device->log_path.str, "Shader compilation failed: %s, %.*s", name, error_length, error_text);
		ZEST_ALERT("Shader compilation failed: %s, %.*s", name, error_length, error_text);
		ZEST_DX_RELEASE(errors);
	}
	ZEST_DXC_RELEASE(result);
	return SUCCEEDED(status);
}
// -- End Shader_compiling

// -- Debug_functions
void *zest__dx_get_resource_ptr(zest_resource_node resource) {
	if (ZEST__FLAGGED(resource->type, zest_resource_type_is_image) && resource->image.backend) {
		return (void *)ZEST_DX_BACKEND(image, &resource->image)->resource;
	} else if (resource->storage_buffer) {
		return (void *)ZEST_DX_BACKEND(device_memory_pool, resource->storage_buffer->memory_pool)->buffer;
	}
	return NULL;
}

zest_bool zest__dx_create_gpu_profiler_backend(zest_context context, zest_gpu_profiler_t *profiler) { return zest__dx_report_unavailable(context->device, "GPU profiling is not available on the D3D12 backend yet."); }

//D3D12 has no binary semaphores and its batches keep no semaphore lists, so the print helpers get NULL
void *zest__dx_get_final_signal_ptr(zest_submission_batch_t *batch, zest_uint semaphore_index) { return NULL; }
void *zest__dx_get_final_wait_ptr(zest_submission_batch_t *batch, zest_uint semaphore_index) { return NULL; }
void *zest__dx_get_swapchain_wait_semaphore(zest_swapchain swapchain, zest_uint index) { return NULL; }
void *zest__dx_get_swapchain_signal_semaphore(zest_swapchain swapchain, zest_uint index) { return NULL; }

void *zest__dx_get_swapchain_identifier(zest_swapchain swapchain) {
	return ZEST_DX_BACKEND(context, swapchain->context)->swapchain;
}
// -- End Debug_functions

#endif // ZEST_DX12_H
