#ifndef ZEST_METAL_H
#define ZEST_METAL_H

/*
Zest Metal Implementation
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
	-- [Shared_events]
	-- [Command_pools]
	-- [Buffer_and_memory]
	-- [Bindless_table]
	-- [General_renderer]
	-- [Device_OS]
	-- [Pipelines]
	-- [Images]
	-- [General_helpers]
	-- [Internal_Frame_graph_context_functions]
	-- [Frame_graph_context_functions]
	-- [Shader_compiling]
	-- [GPU_Profiling_backend]
	-- [Debug_naming]
	-- [Debug_functions]
	-- [Unavailable_stubs]
*/

#if !defined(__OBJC__)
#error "zest_metal.h is Objective-C. Compile the file that defines ZEST_METAL_IMPLEMENTATION as Objective-C or Objective-C++ (-x objective-c++). Zest's CMake does this through zest_metal_implementation_source."
#endif
#if __has_feature(objc_arc)
#error "zest_metal.h uses manual reference counting. Compile the file that defines ZEST_METAL_IMPLEMENTATION with -fno-objc-arc."
#endif

#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>
#if defined(SDL_MAJOR_VERSION)
#include <SDL_metal.h>
#endif
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
#include <time.h>
#include <unistd.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ZEST_MTL_BACKEND(name, object) ((zest_mtl_##name##_backend_t *)(object)->backend)
#define ZEST_MTL_RELEASE(object) do { if (object) { [(object) release]; (object) = nil; } } while (0)
#define ZEST__MTL_UNAVAILABLE(member) "The Metal backend doesn't implement " member " yet. Use zest_BeginVulkanDeviceBuilder on macOS until it does."

// -- Shader_code
// -- End Shader_code

// -- Backend_structs
#define ZEST__MTL_QUEUE_FAMILY_COUNT 3
//Buffer argument indices every converted shader is given. Vertex buffer binding N is at the first plus N and bindless slot N at the last plus N.
#define ZEST__MTL_VERTEX_BUFFER_INDEX 0
#define ZEST__MTL_PUSH_CONSTANT_BUFFER_INDEX 4
#define ZEST__MTL_BINDLESS_BUFFER_INDEX 5
#define ZEST__MTL_BINDLESS_ENTRY_SIZE 8				//Bytes: a texture's or sampler's resource ID, or a buffer's GPU address
#define ZEST__MTL_WAIT_FOREVER (~(zest_microsecs)0)
#define ZEST__MTL_VALIDATION_MESSAGE_SIZE 768		//Bytes kept of one validation record
#define ZEST__MTL_VALIDATION_PENDING_COUNT 64		//Records the reader holds until a device collects them
#define ZEST__MTL_VALIDATION_SYNC_LINE "\x1d" "Zest Metal validation sync"
#define ZEST__MTL_SDL_VIEW_NAME "zest_metal_view"	//The name an SDL window keeps its Metal view under

//What the rest of the backend switches on, read once from the device
typedef struct zest_mtl_feature_support_t {
	zest_bool macos_15;							//Residency sets
	zest_bool metal3;
	zest_bool argument_buffers_tier2;
	zest_bool bc_compression;
	zest_bool apple_gpu;						//The Apple GPU families' formats: ASTC, ETC2, 16-bit packed colour and 8-bit sRGB
	zest_bool depth24_stencil8;
	zest_bool counter_sampling_at_stage;		//GPU timestamps can be sampled at pass stage boundaries
	zest_bool counter_sampling_at_draw;			//and between draws
	zest_uint max_argument_buffer_samplers;
	zest_u64 max_buffer_length;					//Bytes
} zest_mtl_feature_support_t;

typedef struct zest_mtl_device_backend_t {
	id<MTLDevice> device;
	id<MTLResidencySet> residency_set;			//Every pool buffer and heap, attached to every queue so no pass declares what it uses
	pthread_mutex_t residency_lock;				//Pools are added and freed from any thread
	zest_mtl_feature_support_t support;
	zest_bool api_validation;					//The debug layer answered the probe when this device was created
	zest_bool shader_validation;
	zest_bool launcher_shader_validation;		//MTL_SHADER_VALIDATION was already set when this device was asked for
} zest_mtl_device_backend_t;

typedef struct zest_mtl_queue_backend_t {
	id<MTLCommandQueue> queue;
	id<MTLFence> fence;							//Every encoder on this queue waits on it when it opens and updates it when it ends
	id<MTLCommandBuffer> command_buffer;		//The immediate command buffer being recorded, nil between them
	id<MTLBlitCommandEncoder> blit_encoder;		//Opened by the first copy of an immediate command buffer
	zest_buffer *pending_staging_buffers;
} zest_mtl_queue_backend_t;

typedef struct zest_mtl_context_backend_t {
	CAMetalLayer *layer;								//What a windowed context presents to, nil for a headless one. It outlives every swapchain of the context.
	id<CAMetalDrawable> drawable;						//This frame's, from acquire until its present is committed
	zest_uint present_count;							//Presents encoded for this frame's drawable, which must end at one
	MTLRenderPassDescriptor *render_pass_descriptor;	//Refilled by every pass, so none allocates its own
} zest_mtl_context_backend_t;

typedef struct zest_mtl_swapchain_backend_t {
	MTLPixelFormat pixel_format;
} zest_mtl_swapchain_backend_t;

typedef struct zest_mtl_context_queue_backend_t {
	id<MTLCommandBuffer> command_buffer;			//The batch being recorded, nil between batches
	id<MTLCommandBuffer> *committed[ZEST_MAX_FIF];	//The batches committed in each frame in flight, each held until it has finished and been read
} zest_mtl_context_queue_backend_t;

//The events and values the next wave's batches wait on
typedef struct zest_mtl_execution_backend_t {
	id<MTLSharedEvent> wave_wait_events[ZEST_QUEUE_COUNT];
	zest_u64 wave_wait_values[ZEST_QUEUE_COUNT];
	zest_uint wave_wait_count;
} zest_mtl_execution_backend_t;

typedef struct zest_mtl_command_list_backend_t {
	id<MTLCommandBuffer> command_buffer;			//The batch being recorded, which its context queue owns
	id<MTLRenderCommandEncoder> render_encoder;		//Open between a pass's begin and end
	zest_context_queue context_queue;
	id<MTLFence> fence;								//The fence of the device queue the batch runs on
	zest_uint target_width;							//Size of the open pass's attachments, in pixels
	zest_uint target_height;
	zest_bool switched_queue;						//The batch's slot last ran on a different device queue
	zest_mtl_execution_backend_t *execution;		//Set once a wave has been submitted
} zest_mtl_command_list_backend_t;

//A wave's batch signals its slot's event, and the next wave's batches wait on it
typedef struct zest_mtl_frame_graph_semaphores_backend_t {
	id<MTLSharedEvent> events[ZEST_MAX_FIF][ZEST_QUEUE_COUNT];
} zest_mtl_frame_graph_semaphores_backend_t;

typedef struct zest_mtl_submission_batch_backend_t {
	id<MTLSharedEvent> signal_event;				//What the batch signalled, for zest_PrintCompiledFrameGraph
} zest_mtl_submission_batch_backend_t;

//Metal tracks no layouts, so a barrier is only the layout Zest records for the image
typedef struct zest_mtl_frame_graph_image_barrier_t {
	zest_resource_node resource;
	zest_image_layout new_layout;
	zest_bool crosses_queues;
} zest_mtl_frame_graph_image_barrier_t;

typedef struct zest_mtl_execution_barriers_backend_t {
	zest_mtl_frame_graph_image_barrier_t *acquire_images;
	zest_mtl_frame_graph_image_barrier_t *release_images;
} zest_mtl_execution_barriers_backend_t;

typedef struct zest_mtl_execution_timeline_backend_t {
	id<MTLSharedEvent> event;
} zest_mtl_execution_timeline_backend_t;

typedef struct zest_mtl_buffer_allocator_backend_t {
	MTLResourceOptions buffer_options;			//Storage, cache and tracking modes of the allocator's pool buffers
} zest_mtl_buffer_allocator_backend_t;

//A buffer pool is one buffer that Zest suballocates, and an image pool is one heap that textures are placed in
typedef struct zest_mtl_device_memory_pool_backend_t {
	id<MTLBuffer> buffer;
	id<MTLHeap> heap;
} zest_mtl_device_memory_pool_backend_t;

typedef struct zest_mtl_device_memory_backend_t {
	id<MTLHeap> heap;
} zest_mtl_device_memory_backend_t;

typedef struct zest_mtl_image_backend_t {
	id<MTLTexture> texture;
} zest_mtl_image_backend_t;

//A view of the whole image in its own type holds the image's texture, anything else a texture view
typedef struct zest_mtl_image_view_backend_t {
	id<MTLTexture> texture;
} zest_mtl_image_view_backend_t;

typedef struct zest_mtl_sampler_backend_t {
	id<MTLSamplerState> sampler;
} zest_mtl_sampler_backend_t;

typedef struct zest_mtl_set_layout_backend_t {
	zest_uint binding_counts[zest_max_global_binding_number];
} zest_mtl_set_layout_backend_t;

//Where each binding slot's entries sit in the bindless table
typedef struct zest_mtl_bindless_regions_t {
	zest_size offsets[zest_max_global_binding_number];		//Bytes from the start of the table
	zest_uint counts[zest_max_global_binding_number];
	zest_size table_size;									//Bytes
} zest_mtl_bindless_regions_t;

//The bindless set is one buffer the CPU writes entries into and every stage binds a region of per slot
typedef struct zest_mtl_descriptor_pool_backend_t {
	id<MTLBuffer> table;
	zest_mtl_bindless_regions_t regions;
} zest_mtl_descriptor_pool_backend_t;

typedef struct zest_mtl_descriptor_set_backend_t {
	zest_mtl_descriptor_pool_backend_t *pool;
} zest_mtl_descriptor_set_backend_t;

typedef struct zest_mtl_pipeline_layout_backend_t {
	zest_uint push_constant_size;
} zest_mtl_pipeline_layout_backend_t;
// -- End Backend_structs

// -- Other_setup_structs
typedef struct zest_mtl_queue_family_t {
	zest_device_queue_type queue_types;
	zest_uint queue_count;
	const char *name;
	const char *queue_label;
} zest_mtl_queue_family_t;

//Metal has one kind of queue, so the families only exist to give shared code the three it expects
static const zest_mtl_queue_family_t zest__mtl_queue_families[ZEST__MTL_QUEUE_FAMILY_COUNT] = {
	{ zest_queue_graphics | zest_queue_compute | zest_queue_transfer, 4, "Graphics", "Zest Graphics Queue" },
	{ zest_queue_compute | zest_queue_transfer, 4, "Compute", "Zest Compute Queue" },
	{ zest_queue_transfer, 2, "Transfer", "Zest Transfer Queue" },
};

//Metal's debug layer has no callback, so its NSLog records are read back from standard error. One reader serves the process.
typedef struct zest_mtl_validation_reader_t {
	int read_end;								//The pipe standard error points at
	int real_stderr;							//The original standard error, which every byte is passed on to
	zest_bool started;
	zest_bool lost_stderr;						//Something else redirected standard error, so sync lines no longer arrive
	zest_bool saw_api_enabled;					//Metal announced the debug layer after the reader started
	zest_bool saw_shader_enabled;
	zest_bool record_open;						//The last pending record is still taking detail lines
	zest_bool hiding_probe;						//A device is making its deliberate mistake, which isn't passed on to standard error
	zest_u64 sync_requested;
	zest_u64 sync_seen;
	zest_uint pending_count;
	zest_uint dropped_count;					//Records that arrived with the pending list full
	char pending[ZEST__MTL_VALIDATION_PENDING_COUNT][ZEST__MTL_VALIDATION_MESSAGE_SIZE];
} zest_mtl_validation_reader_t;

static zest_mtl_validation_reader_t zest__mtl_validation_reader;
static pthread_mutex_t zest__mtl_validation_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t zest__mtl_validation_caught_up = PTHREAD_COND_INITIALIZER;
static const char *zest__mtl_forced_missing_feature = NULL;
static zest_bool zest__mtl_forced_validation_unavailable = ZEST_FALSE;

ZEST_API id<MTLDevice> zest_GetMetalDevice(zest_device device);
//Metal reads its validation settings when the process makes its first Metal device, and creating a window makes one.
//So call this before creating a window when the device will be created with validation flags, passing those flags.
//It does nothing without zest_device_init_flag_enable_validation_layers, and a launcher that sets MTL_DEBUG_LAYER=1 doesn't need it.
ZEST_API void zest_PrepareMetalValidation(zest_device_init_flags flags);
// -- End Other_setup_structs

ZEST_PRIVATE void zest__mtl_initialise_platform_callbacks(zest_platform_t *platform);
ZEST_PRIVATE zest_bool zest__mtl_report_unavailable(zest_device device, const char *message);

//Frame Graph Platform Commands
ZEST_PRIVATE zest_bool zest__mtl_begin_command_buffer(const zest_command_list command_list);
ZEST_PRIVATE void zest__mtl_end_command_buffer(const zest_command_list command_list);
ZEST_PRIVATE zest_bool zest__mtl_set_next_command_buffer(const zest_command_list command_list, zest_context_queue queue);
ZEST_PRIVATE void zest__mtl_acquire_barrier(const zest_command_list command_list, zest_execution_details_t *exe_details);
ZEST_PRIVATE void zest__mtl_release_barrier(const zest_command_list command_list, zest_execution_details_t *exe_details);
ZEST_PRIVATE void *zest__mtl_new_execution_backend(zloc_linear_allocator_t *allocator);
ZEST_PRIVATE zest_frame_graph_semaphores zest__mtl_get_frame_graph_semaphores(zest_context context, const char *name);
ZEST_PRIVATE zest_bool zest__mtl_submit_frame_graph_batch(zest_frame_graph frame_graph, zest_execution_backend backend, zest_submission_batch_t *batch, zest_map_queue_value *queues);
ZEST_PRIVATE zest_bool zest__mtl_begin_render_pass(const zest_command_list command_list, zest_execution_details_t *exe_details);
ZEST_PRIVATE void zest__mtl_end_render_pass(const zest_command_list command_list);
ZEST_PRIVATE void zest__mtl_carry_over_semaphores(zest_frame_graph frame_graph, zest_wave_submission_t *wave_submission, zest_execution_backend backend);
ZEST_PRIVATE zest_bool zest__mtl_create_execution_timeline_backend(zest_device device, zest_execution_timeline timeline);
ZEST_PRIVATE void zest__mtl_cleanup_execution_timeline_backend(zest_execution_timeline timeline);
ZEST_PRIVATE void zest__mtl_add_frame_graph_buffer_barrier(zest_resource_node resource, zest_execution_barriers_t *barriers, zest_bool acquire, zest_access_flags src_access, zest_access_flags dst_access, zest_uint src_family, zest_uint dst_family, zest_pipeline_stage_flags src_stage, zest_pipeline_stage_flags dst_stage);
ZEST_PRIVATE void zest__mtl_add_frame_graph_image_barrier(zest_resource_node resource, zest_execution_barriers_t *barriers, zest_bool acquire, zest_bool first_use, zest_access_flags src_access, zest_access_flags dst_access, zest_image_layout old_layout, zest_image_layout new_layout, zest_uint src_family, zest_uint dst_family, zest_pipeline_stage_flags src_stage, zest_pipeline_stage_flags dst_stage);
ZEST_PRIVATE zest_bool zest__mtl_present_frame(zest_context context, zest_context_queue present_queue);
ZEST_PRIVATE zest_bool zest__mtl_dummy_submit_for_present_only(zest_context context);
ZEST_PRIVATE zest_bool zest__mtl_acquire_swapchain_image(zest_swapchain swapchain);
ZEST_PRIVATE void *zest__mtl_new_frame_graph_image_backend(zest_device device, zloc_linear_allocator_t *allocator, zest_image image, zest_image imported_image);
//Buffer and memory
ZEST_PRIVATE void *zest__mtl_create_buffer_allocator_backend(zest_device device, zest_context context, zest_size size, zest_buffer_info_t *buffer_info, zest_uint backend_memory_bits);
ZEST_PRIVATE zest_bool zest__mtl_add_buffer_memory_pool(zest_device device, zest_context context, zest_size size, zest_buffer_allocator buffer_allocator, zest_device_memory_pool memory_pool);
ZEST_PRIVATE zest_bool zest__mtl_create_image_memory_pool(zest_device device, zest_context context, zest_size size_in_bytes, zest_buffer_allocator buffer_allocator, zest_device_memory_pool buffer);
ZEST_PRIVATE zest_bool zest__mtl_create_device_memory(zest_device device, zest_size size_in_bytes, zest_buffer_info_t *buffer_info, zest_uint backend_memory_bits, zest_device_memory memory);
ZEST_PRIVATE void zest__mtl_get_memory_budget(zest_device device, zest_memory_budget_t *budget);
ZEST_PRIVATE zest_bool zest__mtl_map_memory(zest_device_memory_pool memory_allocation, zest_size size, zest_size offset);
ZEST_PRIVATE void zest__mtl_unmap_memory(zest_device_memory_pool memory_allocation);
ZEST_PRIVATE void zest__mtl_flush_used_buffers(zest_context context, zest_uint fif);
ZEST_PRIVATE zest_bool zest__mtl_cmd_copy_buffer_one_time(zest_queue queue, zest_buffer src_buffer, zest_buffer dst_buffer, zest_size size, zest_size src_offset, zest_size dst_offset);
//Images
ZEST_PRIVATE zest_bool zest__mtl_is_image_format_supported(zest_device device, zest_format format, zest_image_flags flags);
ZEST_PRIVATE zest_bool zest__mtl_create_image(zest_device device, zest_context context, zest_image image, zest_uint layer_count, zest_sample_count_flags num_samples, zest_image_flags flags);
ZEST_PRIVATE zest_bool zest__mtl_create_transient_image_unbound(zest_device device, zest_context context, zest_image image, zest_uint layer_count, zest_sample_count_flags num_samples, zest_image_flags flags, zest_transient_memory_info_t *info);
ZEST_PRIVATE zest_bool zest__mtl_bind_image_arena(zest_device device, zest_image image, zest_device_memory_pool backing, zest_size offset);
ZEST_PRIVATE zest_device_memory_pool zest__mtl_create_arena_backing(zest_device device, zest_context context, zest_uint category, zest_size size);
ZEST_PRIVATE void zest__mtl_free_arena_backing(zest_device device, zest_context context, zest_device_memory_pool backing);
ZEST_PRIVATE zest_image_view zest__mtl_create_image_view(zest_device device, zest_image image, zest_image_view_type view_type, zest_uint mip_levels_this_view, zest_uint base_mip, zest_uint base_array_index, zest_uint layer_count, zloc_linear_allocator_t *linear_allocator);
ZEST_PRIVATE zest_image_view_array zest__mtl_create_image_views_per_mip(zest_device device, zest_image image, zest_image_view_type view_type, zest_uint base_array_index, zest_uint layer_count, zloc_linear_allocator_t *linear_allocator);
ZEST_PRIVATE zest_bool zest__mtl_copy_buffer_regions_to_image(zest_queue queue, zest_buffer_image_copy_t *regions, zest_uint regions_count, zest_buffer buffer, zest_size src_offset, zest_image image);
ZEST_PRIVATE zest_bool zest__mtl_transition_image(zest_queue queue, zest_image image, zest_resource_state new_state, zest_uint base_mip_index, zest_uint mip_levels, zest_uint base_array_index, zest_uint layer_count);
ZEST_PRIVATE zest_bool zest__mtl_create_sampler(zest_sampler sampler);
ZEST_PRIVATE int zest__mtl_get_image_raw_layout(zest_image image);
ZEST_PRIVATE zest_bool zest__mtl_image_layout_is_valid_for_descriptor(zest_image image);
ZEST_PRIVATE zest_bool zest__mtl_copy_buffer_to_image(zest_queue queue, zest_buffer buffer, zest_size src_offset, zest_image image, zest_uint width, zest_uint height);
ZEST_PRIVATE zest_bool zest__mtl_generate_mipmaps(zest_queue queue, zest_image image);
//Pipelines
ZEST_PRIVATE zest_bool zest__mtl_build_pipeline(zest_pipeline pipeline, zest_command_list command_list);
ZEST_PRIVATE zest_bool zest__mtl_build_pipeline_layout(zest_device device, zest_pipeline_layout layout, zest_pipeline_layout_info_t *info);
ZEST_PRIVATE zest_bool zest__mtl_finish_compute(zest_device device, zest_compute compute);
//Semaphores
ZEST_PRIVATE zest_semaphore_status zest__mtl_wait_for_renderer_semaphore(zest_context context);
ZEST_PRIVATE void zest__mtl_wait_for_fif_semaphore(zest_context context, zest_uint fif);
ZEST_PRIVATE void zest__mtl_queue_wait_idle(zest_context context, zest_context_queue queue);
ZEST_PRIVATE zest_semaphore_status zest__mtl_wait_for_timeline(zest_execution_timeline timeline, zest_microsecs timeout);
//Set layouts
ZEST_PRIVATE zest_bool zest__mtl_create_set_layout(zest_device device, zest_context context, zest_set_layout_builder_t *builder, zest_set_layout layout, zest_bool is_bindless);
ZEST_PRIVATE zest_bool zest__mtl_create_set_pool(zest_device device, zest_context context, zest_descriptor_pool pool, zest_set_layout layout, zest_uint max_set_count, zest_bool bindless);
ZEST_PRIVATE zest_descriptor_set zest__mtl_create_bindless_set(zest_set_layout layout);
ZEST_PRIVATE void zest__mtl_update_bindless_image_descriptor(zest_device device, zest_uint binding_number, zest_uint array_index, zest_descriptor_type type, zest_image image, zest_image_view view, zest_sampler sampler, zest_descriptor_set set);
ZEST_PRIVATE void zest__mtl_update_bindless_storage_buffer_descriptor(zest_device device, zest_uint binding_number, zest_uint array_index, zest_buffer buffer, zest_uint stride, zest_descriptor_set set);
ZEST_PRIVATE void zest__mtl_update_bindless_uniform_buffer_descriptor(zest_device device, zest_uint binding_number, zest_uint array_index, zest_buffer buffer, zest_descriptor_set set);
//Command buffers/queues
ZEST_PRIVATE void zest__mtl_reset_queue_command_pool(zest_context context, zest_context_queue queue, zest_bool release_resources);
//General Context
ZEST_PRIVATE void zest__mtl_set_depth_format(zest_device device);
ZEST_PRIVATE zest_bool zest__mtl_initialise_context_backend(zest_context context);
ZEST_PRIVATE zest_sample_count_flags zest__mtl_get_msaa_sample_count(zest_context context);
ZEST_PRIVATE zest_bool zest__mtl_initialise_swapchain(zest_context context);
ZEST_PRIVATE zest_bool zest__mtl_initialise_context_queue_backend(zest_context context, zest_context_queue context_queue);
ZEST_PRIVATE zest_shader_handle zest__mtl_get_db_overlay_vertex_shader(zest_device device);
ZEST_PRIVATE zest_shader_handle zest__mtl_get_db_overlay_fragment_shader(zest_device device);
//Device/OS
ZEST_PRIVATE void zest__mtl_wait_for_idle_device(zest_device device);
ZEST_PRIVATE zest_bool zest__mtl_initialise_device(zest_device device);
ZEST_PRIVATE zest_bool zest__mtl_query_device_capabilities(zest_device device);
ZEST_PRIVATE zest_bool zest__mtl_create_window_surface(zest_context context);
//Shader Compiling
ZEST_PRIVATE zest_bool zest__mtl_validate_shader(zest_device device, const char *shader_code, zest_shader_type type, const char *name);
ZEST_PRIVATE zest_bool zest__mtl_compile_shader(zest_shader shader, const char *code, zest_uint code_length, zest_shader_type type, const char *name, const char *entry_point, zest_shader_options options);
//Create backends
ZEST_PRIVATE void *zest__mtl_new_frame_graph_semaphores_backend(zest_context context);
ZEST_PRIVATE void *zest__mtl_new_execution_barriers_backend(zloc_linear_allocator_t *allocator);
ZEST_PRIVATE void *zest__mtl_new_pipeline_backend(zest_context context);
ZEST_PRIVATE void *zest__mtl_new_pipeline_layout_backend(zest_device device);
ZEST_PRIVATE void *zest__mtl_new_memory_pool_backend(zest_buffer_allocator buffer_allocator);
ZEST_PRIVATE void *zest__mtl_new_memory_backend(zest_device device);
ZEST_PRIVATE void *zest__mtl_new_device_backend(zest_device device);
ZEST_PRIVATE void *zest__mtl_new_context_backend(zest_context context);
ZEST_PRIVATE void *zest__mtl_new_frame_graph_context_backend(zest_context context);
ZEST_PRIVATE void *zest__mtl_new_swapchain_backend(zest_context context);
ZEST_PRIVATE void *zest__mtl_new_image_backend(zest_device device);
ZEST_PRIVATE void *zest__mtl_new_compute_backend(zest_device device);
ZEST_PRIVATE void *zest__mtl_new_queue_backend(zest_device device, zest_uint family_index);
ZEST_PRIVATE void *zest__mtl_new_submission_batch_backend(zest_context context);
ZEST_PRIVATE void *zest__mtl_new_set_layout_backend(zloc_allocator *allocator);
ZEST_PRIVATE void *zest__mtl_new_descriptor_pool_backend(zloc_allocator *allocator);
ZEST_PRIVATE void *zest__mtl_new_sampler_backend(zest_device device);
ZEST_PRIVATE void *zest__mtl_new_context_queue_backend(zest_context context);
//Cleanup backends
ZEST_PRIVATE void zest__mtl_cleanup_frame_graph_semaphore(zest_context context, zest_frame_graph_semaphores semaphores);
ZEST_PRIVATE void zest__mtl_cleanup_image_backend(zest_image image);
ZEST_PRIVATE void zest__mtl_cleanup_image_view_backend(zest_image_view image_view);
ZEST_PRIVATE void zest__mtl_cleanup_image_view_array_backend(zest_image_view_array image_view);
ZEST_PRIVATE void zest__mtl_cleanup_buffer_allocator_backend(zest_buffer_allocator buffer_allocator);
ZEST_PRIVATE void zest__mtl_cleanup_memory_pool_backend(zest_device_memory_pool memory_allocation);
ZEST_PRIVATE void zest__mtl_cleanup_memory_backend(zest_device_memory memory);
ZEST_PRIVATE void zest__mtl_cleanup_device_backend(zest_device device);
ZEST_PRIVATE zest_bool zest__mtl_reinit_logical_device(zest_device device);
ZEST_PRIVATE void zest__mtl_cleanup_context_backend(zest_context context);
ZEST_PRIVATE void zest__mtl_destroy_context_surface(zest_context context);
ZEST_PRIVATE void zest__mtl_cleanup_swapchain_backend(zest_swapchain swapchain);
ZEST_PRIVATE void zest__mtl_cleanup_compute_backend(zest_compute compute);
ZEST_PRIVATE void zest__mtl_cleanup_pipeline_backend(zest_pipeline pipeline);
ZEST_PRIVATE void zest__mtl_cleanup_pipeline_layout_backend(zest_pipeline_layout layout);
ZEST_PRIVATE void zest__mtl_cleanup_sampler_backend(zest_sampler sampler);
ZEST_PRIVATE void zest__mtl_cleanup_queue_backend(zest_device device, zest_queue queue);
ZEST_PRIVATE void zest__mtl_cleanup_context_queue_backend(zest_context context, zest_context_queue context_queue);
ZEST_PRIVATE void zest__mtl_cleanup_set_layout_backend(zest_set_layout layout);
//GPU Profiling
ZEST_PRIVATE zest_bool zest__mtl_create_gpu_profiler_backend(zest_context context, zest_gpu_profiler_t *profiler);
ZEST_PRIVATE void zest__mtl_cleanup_gpu_profiler_backend(zest_gpu_profiler_t *profiler);
ZEST_PRIVATE void zest__mtl_reset_gpu_query_pool(zest_gpu_profiler_t *profiler, zest_uint fif);
ZEST_PRIVATE void zest__mtl_write_timestamp(const zest_command_list command_list, zest_gpu_profiler_t *profiler, zest_uint fif, zest_uint query_index, zest_bool is_end);
ZEST_PRIVATE zest_bool zest__mtl_readback_gpu_timestamps(zest_gpu_profiler_t *profiler, zest_uint fif, zest_uint query_count);
//Debugging
ZEST_PRIVATE void zest__mtl_set_object_name(zest_device device, zest_uint object_type, zest_u64 object_handle, const char *name);
ZEST_PRIVATE void zest__mtl_set_image_name(zest_device device, zest_image image, const char *name);
ZEST_PRIVATE void zest__mtl_set_buffer_pool_name(zest_device device, zest_device_memory_pool pool, const char *name);
ZEST_PRIVATE void *zest__mtl_get_final_signal_ptr(zest_submission_batch_t *batch, zest_uint semaphore_index);
ZEST_PRIVATE void *zest__mtl_get_final_wait_ptr(zest_submission_batch_t *batch, zest_uint semaphore_index);
ZEST_PRIVATE void *zest__mtl_get_resource_ptr(zest_resource_node resource);
ZEST_PRIVATE void *zest__mtl_get_buffer_ptr(zest_buffer buffer);
ZEST_PRIVATE void *zest__mtl_get_swapchain_wait_semaphore(zest_swapchain swapchain, zest_uint index);
ZEST_PRIVATE void *zest__mtl_get_swapchain_signal_semaphore(zest_swapchain swapchain, zest_uint index);
ZEST_PRIVATE void *zest__mtl_get_swapchain_identifier(zest_swapchain swapchain);
ZEST_PRIVATE void zest__mtl_create_test_render_pass(zest_device device, const zest_command_list_t *command_list);
//Command recording
ZEST_PRIVATE void zest__mtl_blit_image_mip(const zest_command_list command_list, zest_resource_node src, zest_resource_node dst, zest_uint mip_to_blit, zest_supported_shader_stages read_by_stages);
ZEST_PRIVATE void zest__mtl_copy_image_mip(const zest_command_list command_list, zest_resource_node src, zest_resource_node dst, zest_uint mip_to_blit, zest_supported_shader_stages read_by_stages);
ZEST_PRIVATE void zest__mtl_insert_compute_image_barrier(const zest_command_list command_list, zest_resource_node resource, zest_uint base_mip);
ZEST_PRIVATE void zest__mtl_set_screensized_viewport(const zest_command_list command_list, float min_depth, float max_depth);
ZEST_PRIVATE void zest__mtl_scissor(const zest_command_list command_list, zest_scissor_rect_t *scissor);
ZEST_PRIVATE void zest__mtl_viewport(const zest_command_list command_list, zest_viewport_t *viewport);
ZEST_PRIVATE void zest__mtl_clip(const zest_command_list command_list, float x, float y, float width, float height, float min_depth, float max_depth);
ZEST_PRIVATE void zest__mtl_bind_descriptor_sets(const zest_command_list command_list, zest_pipeline_bind_point bind_point, zest_pipeline_layout layout, zest_descriptor_set *descriptor_sets, zest_uint set_count, zest_uint first_set);
ZEST_PRIVATE void zest__mtl_bind_pipeline(const zest_command_list command_list, zest_pipeline pipeline);
ZEST_PRIVATE void zest__mtl_bind_compute_pipeline(const zest_command_list command_list, zest_compute compute);
ZEST_PRIVATE void zest__mtl_copy_buffer(const zest_command_list command_list, zest_buffer staging_buffer, zest_buffer device_buffer, zest_size size);
ZEST_PRIVATE zest_bool zest__mtl_upload_buffer(const zest_command_list command_list, zest_buffer_uploader_t *uploader);
ZEST_PRIVATE void zest__mtl_bind_vertex_buffer(const zest_command_list command_list, zest_uint first_binding, zest_uint binding_count, zest_buffer buffer);
ZEST_PRIVATE void zest__mtl_bind_index_buffer(const zest_command_list command_list, zest_buffer buffer);
ZEST_PRIVATE zest_bool zest__mtl_image_clear(const zest_command_list command_list, zest_image image);
ZEST_PRIVATE void zest__mtl_bind_mesh_vertex_buffer(const zest_command_list command_list, zest_layer layer);
ZEST_PRIVATE void zest__mtl_bind_mesh_index_buffer(const zest_command_list command_list, zest_layer layer);
ZEST_PRIVATE void zest__mtl_dispatch_compute(const zest_command_list command_list, zest_uint group_count_x, zest_uint group_count_y, zest_uint group_count_z);
ZEST_PRIVATE void zest__mtl_send_push_constants(const zest_command_list command_list, zest_pipeline_layout layout, void *data, zest_uint size);
ZEST_PRIVATE void zest__mtl_draw(const zest_command_list command_list, zest_uint vertex_count, zest_uint instance_count, zest_uint first_vertex, zest_uint first_instance);
ZEST_PRIVATE void zest__mtl_draw_layer_instruction(const zest_command_list command_list, zest_uint vertex_count, zest_layer_instruction_t *instruction);
ZEST_PRIVATE void zest__mtl_draw_indexed(const zest_command_list command_list, zest_uint index_count, zest_uint instance_count, zest_uint first_index, int32_t vertex_offset, zest_uint first_instance);
ZEST_PRIVATE void zest__mtl_draw_indexed_indirect(const zest_command_list command_list, zest_buffer buffer, zest_size offset, zest_uint draw_count, zest_uint stride);
ZEST_PRIVATE void zest__mtl_draw_indirect(const zest_command_list command_list, zest_buffer buffer, zest_size offset, zest_uint draw_count, zest_uint stride);
ZEST_PRIVATE void zest__mtl_set_depth_bias(const zest_command_list command_list, float factor, float clamp, float slope);
//Immediate commands
ZEST_PRIVATE zest_queue zest__mtl_imm_begin_command_buffer(zest_device device, zest_device_queue_type target_queue);
ZEST_PRIVATE zest_bool zest__mtl_imm_end_command_buffer(zest_queue queue);
ZEST_PRIVATE void zest__mtl_imm_fill_buffer(zest_queue queue, zest_buffer buffer, zest_uint value);
ZEST_PRIVATE void zest__mtl_imm_update_buffer(zest_queue queue, zest_buffer buffer, void *data, zest_size intended_size);
ZEST_PRIVATE zest_bool zest__mtl_imm_clear_color_image(zest_queue queue, zest_image image, zest_clear_value_t clear_value);
ZEST_PRIVATE zest_bool zest__mtl_imm_clear_depth_stencil_image(zest_queue queue, zest_image image, float depth, zest_uint stencil);
ZEST_PRIVATE zest_bool zest__mtl_imm_blit_image(zest_queue queue, zest_image src_image, zest_image dst_image, int src_x, int src_y, int src_width, int src_height, int dst_x, int dst_y, int dst_width, int dst_height, zest_filter_type filter);
ZEST_PRIVATE zest_bool zest__mtl_imm_resolve_image(zest_queue queue, zest_image src_image, zest_image dst_image);
ZEST_PRIVATE void zest__mtl_imm_send_push_constants(zest_queue queue, void *data, zest_uint size);
ZEST_PRIVATE void zest__mtl_imm_bind_compute_pipeline(zest_queue queue, zest_compute compute);
ZEST_PRIVATE zest_bool zest__mtl_imm_dispatch_compute(zest_queue queue, zest_uint group_count_x, zest_uint group_count_y, zest_uint group_count_z);
ZEST_PRIVATE zest_bool zest__mtl_copy_bitmap_to_image(zest_device device, void *bitmap, zest_size image_size, zest_image dst_image, zest_uint width, zest_uint height);
ZEST_PRIVATE zest_bool zest__mtl_copy_image_to_bitmap(zest_device device, zest_image src_image, void *pixels);
ZEST_PRIVATE zest_bool zest__mtl_start_validation_reader(void);
ZEST_PRIVATE void zest__mtl_sync_validation_reader(void);
ZEST_PRIVATE void zest__mtl_record_validation_message(zest_device device, const char *description, zest_bool print);
ZEST_PRIVATE void zest__mtl_collect_validation_messages(zest_device device);
ZEST_PRIVATE void zest__mtl_prepare_validation(zest_device device);
ZEST_PRIVATE void zest__mtl_confirm_validation(zest_device device);
ZEST_PRIVATE void zest__mtl_query_feature_support(id<MTLDevice> native_device, zest_mtl_feature_support_t *support);
ZEST_PRIVATE zest_bool zest__mtl_meets_minimum_spec(const zest_mtl_feature_support_t *support, const char **out_missing);
ZEST_PRIVATE zest_capability_flags zest__mtl_translate_capabilities(const zest_mtl_feature_support_t *support);
ZEST_PRIVATE zest_bool zest__mtl_create_queues(zest_device device);
ZEST_PRIVATE zest_bool zest__mtl_create_device_objects(zest_device device);
ZEST_PRIVATE void zest__mtl_cleanup_queues(zest_device device);
ZEST_PRIVATE void zest__mtl_add_queue_manager_list(zest_device device, zest_device_queue_type queue_type, const zest_uint *family_order, zest_uint order_count);
ZEST_PRIVATE void zest__mtl_release_immediate_queue(zest_queue queue, zest_bool gpu_finished);
ZEST_PRIVATE zest_bool zest__mtl_finish_command_buffer(zest_device device, id<MTLCommandBuffer> command_buffer);
ZEST_PRIVATE void zest__mtl_reap_command_buffers(zest_context context, zest_context_queue queue, zest_bool release_all);
ZEST_PRIVATE void zest__mtl_reap_device_command_buffers(zest_device device);
ZEST_PRIVATE void zest__mtl_add_to_residency_set(zest_device device, id<MTLAllocation> allocation);
ZEST_PRIVATE void zest__mtl_remove_from_residency_set(zest_device device, id<MTLAllocation> allocation);
ZEST_PRIVATE MTLResourceOptions zest__mtl_buffer_options(zest_memory_property_flags property_flags);
ZEST_PRIVATE zest_bool zest__mtl_create_image_heap(zest_device device, zest_size size, id<MTLHeap> *out_heap);
ZEST_PRIVATE id<MTLBuffer> zest__mtl_buffer_object(zest_buffer buffer);
ZEST_PRIVATE zest_u64 zest__mtl_buffer_address(zest_buffer buffer);
ZEST_PRIVATE MTLPixelFormat zest__mtl_pixel_format(zest_format format);
ZEST_PRIVATE zest_bool zest__mtl_device_has_pixel_format(const zest_mtl_feature_support_t *support, MTLPixelFormat pixel_format);
ZEST_PRIVATE zest_image_layout zest__mtl_image_layout_from_resource_state(zest_resource_state state, zest_bool is_depth);
ZEST_PRIVATE MTLTextureType zest__mtl_image_texture_type(zest_image image, zest_uint layer_count, zest_uint sample_count, zest_image_flags flags);
ZEST_PRIVATE MTLTextureType zest__mtl_view_texture_type(zest_image_view_type view_type, zest_bool multisampled);
ZEST_PRIVATE MTLTextureDescriptor *zest__mtl_new_texture_descriptor(zest_device device, zest_image image, zest_uint layer_count, zest_uint sample_count, zest_image_flags flags);
ZEST_PRIVATE zest_bool zest__mtl_swizzle_is_identity(zest_component_mapping_t swizzle);
ZEST_PRIVATE zest_component_mapping_t zest__mtl_image_swizzle(zest_image image);
ZEST_PRIVATE id<MTLTexture> zest__mtl_new_view_texture(zest_image image, zest_image_view_type view_type, zest_uint mip_levels_this_view, zest_uint base_mip, zest_uint base_array_index, zest_uint layer_count);
ZEST_PRIVATE id<MTLBlitCommandEncoder> zest__mtl_queue_blit_encoder(zest_queue queue);
ZEST_PRIVATE void zest__mtl_end_queue_encoder(zest_queue queue);
ZEST_PRIVATE MTLBlitOption zest__mtl_blit_option(zest_format format, zest_image_aspect_flags aspect);
ZEST_PRIVATE zest_bool zest__mtl_swapchain_format(zest_format requested, MTLPixelFormat *out_pixel_format, zest_format *out_reported);
ZEST_PRIVATE zest_extent2d_t zest__mtl_layer_extent(CAMetalLayer *layer);
ZEST_PRIVATE MTLLoadAction zest__mtl_load_action(zest_load_op load_op);
ZEST_PRIVATE MTLStoreAction zest__mtl_store_action(zest_store_op store_op);
ZEST_PRIVATE MTLScissorRect zest__mtl_clamped_scissor(int x, int y, zest_uint width, zest_uint height, zest_uint target_width, zest_uint target_height);
ZEST_PRIVATE void zest__mtl_apply_image_barriers(const zest_mtl_frame_graph_image_barrier_t *records, zest_bool release);
ZEST_PRIVATE void zest__mtl_compute_bindless_regions(const zest_uint *binding_counts, zest_mtl_bindless_regions_t *regions);
ZEST_PRIVATE zest_u64 *zest__mtl_bindless_entry(zest_device device, zest_descriptor_set set, zest_uint binding_number, zest_uint array_index);

// -- Error_logging
//Reports a call the backend can't serve. A member with no device to report on only prints.
zest_bool zest__mtl_report_unavailable(zest_device device, const char *message) {
#ifdef ZEST_TEST_MODE
	if (device) {
		zest__log_validation_error(device, message);
		return ZEST_FALSE;
	}
#endif
	ZEST_ALERT("Zest Metal Error: %s", message);
	ZEST_ASSERT(!device, "The Metal backend was asked for something it can't do. See the Zest Metal Error printed above.");
	return ZEST_FALSE;
}
//The text after the date, process and thread that NSLog puts before a message, or NULL for any other line
ZEST_PRIVATE const char *zest__mtl_nslog_message(const char *line, size_t length) {
	if (length < 26 || line[4] != '-' || line[7] != '-' || line[10] != ' ' || line[13] != ':' || line[16] != ':' || line[19] != '.') return NULL;
	const char *prefix_end = strstr(line + 20, "] ");
	return prefix_end ? prefix_end + 2 : NULL;
}

//Passes one line of standard error on and keeps it if it belongs to a debug layer record
ZEST_PRIVATE void zest__mtl_read_validation_line(const char *line, size_t length, zest_bool complete) {
	zest_mtl_validation_reader_t *reader = &zest__mtl_validation_reader;
	if (complete && strcmp(line, ZEST__MTL_VALIDATION_SYNC_LINE) == 0) {
		pthread_mutex_lock(&zest__mtl_validation_lock);
		reader->record_open = ZEST_FALSE;
		reader->sync_seen++;
		pthread_cond_broadcast(&zest__mtl_validation_caught_up);
		pthread_mutex_unlock(&zest__mtl_validation_lock);
		return;
	}
	const char *message = zest__mtl_nslog_message(line, length);
	pthread_mutex_lock(&zest__mtl_validation_lock);
	zest_bool probe_line = reader->hiding_probe && (message ? strcmp(message, "Buffer Validation") == 0 : reader->record_open);
	if (!probe_line) {
		(void)write(reader->real_stderr, line, length);
		if (complete) (void)write(reader->real_stderr, "\n", 1);
	}
	if (message) {
		size_t message_length = strlen(message);
		reader->record_open = ZEST_FALSE;
		if (strcmp(message, "Metal API Validation Enabled") == 0) {
			reader->saw_api_enabled = ZEST_TRUE;
		} else if (strcmp(message, "Metal GPU Validation Enabled") == 0) {
			reader->saw_shader_enabled = ZEST_TRUE;
		} else if (message_length >= 10 && strcmp(message + message_length - 10, "Validation") == 0) {
			//Every debug layer record opens with a title ending in Validation, and its detail lines follow without a prefix
			if (reader->pending_count == ZEST__MTL_VALIDATION_PENDING_COUNT) {
				reader->dropped_count++;
			} else {
				snprintf(reader->pending[reader->pending_count++], ZEST__MTL_VALIDATION_MESSAGE_SIZE, "%s", message);
				reader->record_open = ZEST_TRUE;
			}
		}
	} else if (reader->record_open && length) {
		char *record = reader->pending[reader->pending_count - 1];
		size_t record_length = strlen(record);
		snprintf(record + record_length, ZEST__MTL_VALIDATION_MESSAGE_SIZE - record_length, " %s", line);
	}
	pthread_mutex_unlock(&zest__mtl_validation_lock);
}

ZEST_PRIVATE void *zest__mtl_validation_reader_thread(void *unused) {
	(void)unused;
	char chunk[4096];
	char line[1024];
	size_t line_length = 0;
	for (;;) {
		ssize_t chunk_size = read(zest__mtl_validation_reader.read_end, chunk, sizeof(chunk));
		if (chunk_size < 0 && errno == EINTR) continue;
		if (chunk_size <= 0) break;
		for (ssize_t chunk_index = 0; chunk_index != chunk_size; ++chunk_index) {
			char character = chunk[chunk_index];
			if (character == '\n') {
				line[line_length] = 0;
				zest__mtl_read_validation_line(line, line_length, ZEST_TRUE);
				line_length = 0;
				continue;
			}
			//A line longer than the buffer is passed on in pieces
			if (line_length == sizeof(line) - 1) {
				line[line_length] = 0;
				zest__mtl_read_validation_line(line, line_length, ZEST_FALSE);
				line_length = 0;
			}
			line[line_length++] = character;
		}
	}
	return NULL;
}

//An abort leaves what was last written to standard error in the pipe, which is where the reason for the abort is
ZEST_PRIVATE void zest__mtl_flush_stderr_on_abort(int signal_number) {
	zest_mtl_validation_reader_t *reader = &zest__mtl_validation_reader;
	char chunk[1024];
	ssize_t chunk_size = 0;
	fcntl(reader->read_end, F_SETFL, fcntl(reader->read_end, F_GETFL) | O_NONBLOCK);
	while ((chunk_size = read(reader->read_end, chunk, sizeof(chunk))) > 0) {
		(void)write(reader->real_stderr, chunk, (size_t)chunk_size);
	}
	signal(signal_number, SIG_DFL);
	raise(signal_number);
}

//A normal exit must not leave standard error text unread in the pipe
ZEST_PRIVATE void zest__mtl_sync_validation_reader_at_exit(void) {
	zest__mtl_sync_validation_reader();
}

zest_bool zest__mtl_start_validation_reader(void) {
	zest_mtl_validation_reader_t *reader = &zest__mtl_validation_reader;
	zest_bool started = ZEST_FALSE;
	pthread_mutex_lock(&zest__mtl_validation_lock);
	if (reader->started) {
		pthread_mutex_unlock(&zest__mtl_validation_lock);
		return ZEST_TRUE;
	}
	int pipe_ends[2];
	if (pipe(pipe_ends) == 0) {
		fflush(stderr);
		reader->real_stderr = dup(STDERR_FILENO);
		if (reader->real_stderr >= 0 && dup2(pipe_ends[1], STDERR_FILENO) >= 0) {
			reader->read_end = pipe_ends[0];
			pthread_t thread;
			if (pthread_create(&thread, NULL, zest__mtl_validation_reader_thread, NULL) == 0) {
				pthread_detach(thread);
				signal(SIGABRT, zest__mtl_flush_stderr_on_abort);
				atexit(zest__mtl_sync_validation_reader_at_exit);
				started = ZEST_TRUE;
			} else {
				dup2(reader->real_stderr, STDERR_FILENO);
			}
		}
		close(pipe_ends[1]);
		if (!started) {
			close(pipe_ends[0]);
			if (reader->real_stderr >= 0) close(reader->real_stderr);
		}
	}
	reader->started = started;
	pthread_mutex_unlock(&zest__mtl_validation_lock);
	return started;
}

//Returns once the reader has taken in everything written to standard error before the call
void zest__mtl_sync_validation_reader(void) {
	zest_mtl_validation_reader_t *reader = &zest__mtl_validation_reader;
	if (!reader->started || reader->lost_stderr) return;
	pthread_mutex_lock(&zest__mtl_validation_lock);
	zest_u64 wanted = ++reader->sync_requested;
	pthread_mutex_unlock(&zest__mtl_validation_lock);
	fflush(stderr);
	(void)write(STDERR_FILENO, ZEST__MTL_VALIDATION_SYNC_LINE "\n", sizeof(ZEST__MTL_VALIDATION_SYNC_LINE));
	struct timespec deadline;
	clock_gettime(CLOCK_REALTIME, &deadline);
	deadline.tv_sec += 1;
	pthread_mutex_lock(&zest__mtl_validation_lock);
	while (reader->sync_seen < wanted) {
		if (pthread_cond_timedwait(&zest__mtl_validation_caught_up, &zest__mtl_validation_lock, &deadline) != 0) {
			//The line never came back, so standard error no longer leads to the reader
			reader->lost_stderr = ZEST_TRUE;
			break;
		}
	}
	pthread_mutex_unlock(&zest__mtl_validation_lock);
}

void zest__mtl_record_validation_message(zest_device device, const char *description, zest_bool print) {
	ZEST_APPEND_LOG(device->log_path.str, "Validation Layer: %s", description);
	if (print && ZEST__FLAGGED(device->init_flags, zest_device_init_flag_log_validation_errors_to_console)) {
		ZEST_ALERT("Metal Validation: %s", description);
	}
	if (ZEST__FLAGGED(device->init_flags, zest_device_init_flag_log_validation_errors_to_memory)) {
		zest_key key = zest_Hash(description, (zest_uint)strlen(description), ZEST_HASH_SEED);
		if (!zest_map_valid_key(device->validation_errors, key)) {
			zest_text_t error_message = ZEST__ZERO_INIT(zest_text_t);
			zest_SetText(device->allocator, &error_message, description);
			zest_map_insert_key(device->allocator, device->validation_errors, key, error_message);
		}
	}
}

//Moves the debug layer records read so far into the device. The layer is process wide, so they go to whichever device asks first.
void zest__mtl_collect_validation_messages(zest_device device) {
	zest_mtl_validation_reader_t *reader = &zest__mtl_validation_reader;
	if (!device->backend) return;
	//Batches that have finished since the last frame hold their shader validation logs and any failure
	zest__mtl_reap_device_command_buffers(device);
	if (!reader->started) return;
	zest__mtl_sync_validation_reader();
	pthread_mutex_lock(&zest__mtl_validation_lock);
	for (zest_uint record_index = 0; record_index != reader->pending_count; ++record_index) {
		//The record is already on standard error, so it isn't printed again
		zest__mtl_record_validation_message(device, reader->pending[record_index], ZEST_FALSE);
	}
	if (reader->dropped_count) {
		char dropped_message[160];
		snprintf(dropped_message, sizeof(dropped_message), "%u more Metal validation messages arrived than Zest holds between collections. They are on standard error.", reader->dropped_count);
		zest__mtl_record_validation_message(device, dropped_message, ZEST_TRUE);
	}
	reader->pending_count = 0;
	reader->dropped_count = 0;
	reader->record_open = ZEST_FALSE;
	pthread_mutex_unlock(&zest__mtl_validation_lock);
}
// -- End Error_logging

// -- Enum_to_string_functions
// -- End Enum_to_string_functions

// -- Type_converters
//Formats with no Metal form give MTLPixelFormatInvalid. The numbers the Apple GPU families add are in the SDK on every Mac.
MTLPixelFormat zest__mtl_pixel_format(zest_format format) {
	switch (format) {
		case zest_format_r4g4b4a4_unorm_pack16:			return MTLPixelFormatABGR4Unorm;
		case zest_format_r5g6b5_unorm_pack16:			return MTLPixelFormatB5G6R5Unorm;
		case zest_format_a1r5g5b5_unorm_pack16:			return MTLPixelFormatBGR5A1Unorm;
		case zest_format_r8_unorm:						return MTLPixelFormatR8Unorm;
		case zest_format_r8_snorm:						return MTLPixelFormatR8Snorm;
		case zest_format_r8_uint:						return MTLPixelFormatR8Uint;
		case zest_format_r8_sint:						return MTLPixelFormatR8Sint;
		case zest_format_r8_srgb:						return MTLPixelFormatR8Unorm_sRGB;
		case zest_format_r8g8_unorm:					return MTLPixelFormatRG8Unorm;
		case zest_format_r8g8_snorm:					return MTLPixelFormatRG8Snorm;
		case zest_format_r8g8_uint:						return MTLPixelFormatRG8Uint;
		case zest_format_r8g8_sint:						return MTLPixelFormatRG8Sint;
		case zest_format_r8g8_srgb:						return MTLPixelFormatRG8Unorm_sRGB;
		case zest_format_r8g8b8a8_unorm:				return MTLPixelFormatRGBA8Unorm;
		case zest_format_r8g8b8a8_snorm:				return MTLPixelFormatRGBA8Snorm;
		case zest_format_r8g8b8a8_uint:					return MTLPixelFormatRGBA8Uint;
		case zest_format_r8g8b8a8_sint:					return MTLPixelFormatRGBA8Sint;
		case zest_format_r8g8b8a8_srgb:					return MTLPixelFormatRGBA8Unorm_sRGB;
		case zest_format_b8g8r8a8_unorm:				return MTLPixelFormatBGRA8Unorm;
		case zest_format_b8g8r8a8_srgb:					return MTLPixelFormatBGRA8Unorm_sRGB;
		//Packed into 32 bits with red lowest, which on a little-endian machine is the byte order of RGBA8
		case zest_format_a8b8g8r8_unorm_pack32:			return MTLPixelFormatRGBA8Unorm;
		case zest_format_a8b8g8r8_snorm_pack32:			return MTLPixelFormatRGBA8Snorm;
		case zest_format_a8b8g8r8_uint_pack32:			return MTLPixelFormatRGBA8Uint;
		case zest_format_a8b8g8r8_sint_pack32:			return MTLPixelFormatRGBA8Sint;
		case zest_format_a8b8g8r8_srgb_pack32:			return MTLPixelFormatRGBA8Unorm_sRGB;
		case zest_format_a2r10g10b10_unorm_pack32:		return MTLPixelFormatBGR10A2Unorm;
		case zest_format_a2b10g10r10_unorm_pack32:		return MTLPixelFormatRGB10A2Unorm;
		case zest_format_a2b10g10r10_uint_pack32:		return MTLPixelFormatRGB10A2Uint;
		case zest_format_r16_unorm:						return MTLPixelFormatR16Unorm;
		case zest_format_r16_snorm:						return MTLPixelFormatR16Snorm;
		case zest_format_r16_uint:						return MTLPixelFormatR16Uint;
		case zest_format_r16_sint:						return MTLPixelFormatR16Sint;
		case zest_format_r16_sfloat:					return MTLPixelFormatR16Float;
		case zest_format_r16g16_unorm:					return MTLPixelFormatRG16Unorm;
		case zest_format_r16g16_snorm:					return MTLPixelFormatRG16Snorm;
		case zest_format_r16g16_uint:					return MTLPixelFormatRG16Uint;
		case zest_format_r16g16_sint:					return MTLPixelFormatRG16Sint;
		case zest_format_r16g16_sfloat:					return MTLPixelFormatRG16Float;
		case zest_format_r16g16b16a16_unorm:			return MTLPixelFormatRGBA16Unorm;
		case zest_format_r16g16b16a16_snorm:			return MTLPixelFormatRGBA16Snorm;
		case zest_format_r16g16b16a16_uint:				return MTLPixelFormatRGBA16Uint;
		case zest_format_r16g16b16a16_sint:				return MTLPixelFormatRGBA16Sint;
		case zest_format_r16g16b16a16_sfloat:			return MTLPixelFormatRGBA16Float;
		case zest_format_r32_uint:						return MTLPixelFormatR32Uint;
		case zest_format_r32_sint:						return MTLPixelFormatR32Sint;
		case zest_format_r32_sfloat:					return MTLPixelFormatR32Float;
		case zest_format_r32g32_uint:					return MTLPixelFormatRG32Uint;
		case zest_format_r32g32_sint:					return MTLPixelFormatRG32Sint;
		case zest_format_r32g32_sfloat:					return MTLPixelFormatRG32Float;
		case zest_format_r32g32b32a32_uint:				return MTLPixelFormatRGBA32Uint;
		case zest_format_r32g32b32a32_sint:				return MTLPixelFormatRGBA32Sint;
		case zest_format_r32g32b32a32_sfloat:			return MTLPixelFormatRGBA32Float;
		case zest_format_b10g11r11_ufloat_pack32:		return MTLPixelFormatRG11B10Float;
		case zest_format_e5b9g9r9_ufloat_pack32:		return MTLPixelFormatRGB9E5Float;
		case zest_format_d16_unorm:						return MTLPixelFormatDepth16Unorm;
		//Metal has no 24-bit depth without stencil, so the stencil byte rides along unused
		case zest_format_x8_d24_unorm_pack32:			return MTLPixelFormatDepth24Unorm_Stencil8;
		case zest_format_d32_sfloat:					return MTLPixelFormatDepth32Float;
		case zest_format_s8_uint:						return MTLPixelFormatStencil8;
		case zest_format_d24_unorm_s8_uint:				return MTLPixelFormatDepth24Unorm_Stencil8;
		case zest_format_d32_sfloat_s8_uint:			return MTLPixelFormatDepth32Float_Stencil8;
		//Metal has one BC1 format, and views of the RGB ones read alpha as one
		case zest_format_bc1_rgb_unorm_block:			return MTLPixelFormatBC1_RGBA;
		case zest_format_bc1_rgb_srgb_block:			return MTLPixelFormatBC1_RGBA_sRGB;
		case zest_format_bc1_rgba_unorm_block:			return MTLPixelFormatBC1_RGBA;
		case zest_format_bc1_rgba_srgb_block:			return MTLPixelFormatBC1_RGBA_sRGB;
		case zest_format_bc2_unorm_block:				return MTLPixelFormatBC2_RGBA;
		case zest_format_bc2_srgb_block:				return MTLPixelFormatBC2_RGBA_sRGB;
		case zest_format_bc3_unorm_block:				return MTLPixelFormatBC3_RGBA;
		case zest_format_bc3_srgb_block:				return MTLPixelFormatBC3_RGBA_sRGB;
		case zest_format_bc4_unorm_block:				return MTLPixelFormatBC4_RUnorm;
		case zest_format_bc4_snorm_block:				return MTLPixelFormatBC4_RSnorm;
		case zest_format_bc5_unorm_block:				return MTLPixelFormatBC5_RGUnorm;
		case zest_format_bc5_snorm_block:				return MTLPixelFormatBC5_RGSnorm;
		case zest_format_bc6h_ufloat_block:				return MTLPixelFormatBC6H_RGBUfloat;
		case zest_format_bc6h_sfloat_block:				return MTLPixelFormatBC6H_RGBFloat;
		case zest_format_bc7_unorm_block:				return MTLPixelFormatBC7_RGBAUnorm;
		case zest_format_bc7_srgb_block:				return MTLPixelFormatBC7_RGBAUnorm_sRGB;
		case zest_format_etc2_r8g8b8_unorm_block:		return MTLPixelFormatETC2_RGB8;
		case zest_format_etc2_r8g8b8_srgb_block:		return MTLPixelFormatETC2_RGB8_sRGB;
		case zest_format_etc2_r8g8b8a1_unorm_block:		return MTLPixelFormatETC2_RGB8A1;
		case zest_format_etc2_r8g8b8a1_srgb_block:		return MTLPixelFormatETC2_RGB8A1_sRGB;
		case zest_format_etc2_r8g8b8a8_unorm_block:		return MTLPixelFormatEAC_RGBA8;
		case zest_format_etc2_r8g8b8a8_srgb_block:		return MTLPixelFormatEAC_RGBA8_sRGB;
		case zest_format_eac_r11_unorm_block:			return MTLPixelFormatEAC_R11Unorm;
		case zest_format_eac_r11_snorm_block:			return MTLPixelFormatEAC_R11Snorm;
		case zest_format_eac_r11g11_unorm_block:		return MTLPixelFormatEAC_RG11Unorm;
		case zest_format_eac_r11g11_snorm_block:		return MTLPixelFormatEAC_RG11Snorm;
		case zest_format_astc_4X4_unorm_block:			return MTLPixelFormatASTC_4x4_LDR;
		case zest_format_astc_4X4_srgb_block:			return MTLPixelFormatASTC_4x4_sRGB;
		case zest_format_astc_5X4_unorm_block:			return MTLPixelFormatASTC_5x4_LDR;
		case zest_format_astc_5X4_srgb_block:			return MTLPixelFormatASTC_5x4_sRGB;
		case zest_format_astc_5X5_unorm_block:			return MTLPixelFormatASTC_5x5_LDR;
		case zest_format_astc_5X5_srgb_block:			return MTLPixelFormatASTC_5x5_sRGB;
		case zest_format_astc_6X5_unorm_block:			return MTLPixelFormatASTC_6x5_LDR;
		case zest_format_astc_6X5_srgb_block:			return MTLPixelFormatASTC_6x5_sRGB;
		case zest_format_astc_6X6_unorm_block:			return MTLPixelFormatASTC_6x6_LDR;
		case zest_format_astc_6X6_srgb_block:			return MTLPixelFormatASTC_6x6_sRGB;
		case zest_format_astc_8X5_unorm_block:			return MTLPixelFormatASTC_8x5_LDR;
		case zest_format_astc_8X5_srgb_block:			return MTLPixelFormatASTC_8x5_sRGB;
		case zest_format_astc_8X6_unorm_block:			return MTLPixelFormatASTC_8x6_LDR;
		case zest_format_astc_8X6_srgb_block:			return MTLPixelFormatASTC_8x6_sRGB;
		case zest_format_astc_8X8_unorm_block:			return MTLPixelFormatASTC_8x8_LDR;
		case zest_format_astc_8X8_srgb_block:			return MTLPixelFormatASTC_8x8_sRGB;
		case zest_format_astc_10X5_unorm_block:			return MTLPixelFormatASTC_10x5_LDR;
		case zest_format_astc_10X5_srgb_block:			return MTLPixelFormatASTC_10x5_sRGB;
		case zest_format_astc_10X6_unorm_block:			return MTLPixelFormatASTC_10x6_LDR;
		case zest_format_astc_10X6_srgb_block:			return MTLPixelFormatASTC_10x6_sRGB;
		case zest_format_astc_10X8_unorm_block:			return MTLPixelFormatASTC_10x8_LDR;
		case zest_format_astc_10X8_srgb_block:			return MTLPixelFormatASTC_10x8_sRGB;
		case zest_format_astc_10X10_unorm_block:		return MTLPixelFormatASTC_10x10_LDR;
		case zest_format_astc_10X10_srgb_block:			return MTLPixelFormatASTC_10x10_sRGB;
		case zest_format_astc_12X10_unorm_block:		return MTLPixelFormatASTC_12x10_LDR;
		case zest_format_astc_12X10_srgb_block:			return MTLPixelFormatASTC_12x10_sRGB;
		case zest_format_astc_12X12_unorm_block:		return MTLPixelFormatASTC_12x12_LDR;
		case zest_format_astc_12X12_srgb_block:			return MTLPixelFormatASTC_12x12_sRGB;
		default:										return MTLPixelFormatInvalid;
	}
}

//Metal has no query for a single format, only what each GPU family brings
zest_bool zest__mtl_device_has_pixel_format(const zest_mtl_feature_support_t *support, MTLPixelFormat pixel_format) {
	if (pixel_format >= MTLPixelFormatASTC_4x4_sRGB && pixel_format <= MTLPixelFormatASTC_12x12_LDR) {
		return support->apple_gpu;
	}
	switch (pixel_format) {
		case MTLPixelFormatInvalid:
			return ZEST_FALSE;
		case MTLPixelFormatDepth24Unorm_Stencil8:
			return support->depth24_stencil8;
		case MTLPixelFormatBC1_RGBA: case MTLPixelFormatBC1_RGBA_sRGB: case MTLPixelFormatBC2_RGBA: case MTLPixelFormatBC2_RGBA_sRGB:
		case MTLPixelFormatBC3_RGBA: case MTLPixelFormatBC3_RGBA_sRGB: case MTLPixelFormatBC4_RUnorm: case MTLPixelFormatBC4_RSnorm:
		case MTLPixelFormatBC5_RGUnorm: case MTLPixelFormatBC5_RGSnorm: case MTLPixelFormatBC6H_RGBUfloat: case MTLPixelFormatBC6H_RGBFloat:
		case MTLPixelFormatBC7_RGBAUnorm: case MTLPixelFormatBC7_RGBAUnorm_sRGB:
			return support->bc_compression;
		case MTLPixelFormatABGR4Unorm: case MTLPixelFormatB5G6R5Unorm: case MTLPixelFormatBGR5A1Unorm:
		case MTLPixelFormatR8Unorm_sRGB: case MTLPixelFormatRG8Unorm_sRGB:
		case MTLPixelFormatETC2_RGB8: case MTLPixelFormatETC2_RGB8_sRGB: case MTLPixelFormatETC2_RGB8A1: case MTLPixelFormatETC2_RGB8A1_sRGB:
		case MTLPixelFormatEAC_RGBA8: case MTLPixelFormatEAC_RGBA8_sRGB: case MTLPixelFormatEAC_R11Unorm: case MTLPixelFormatEAC_R11Snorm:
		case MTLPixelFormatEAC_RG11Unorm: case MTLPixelFormatEAC_RG11Snorm:
			return support->apple_gpu;
		default:
			return ZEST_TRUE;
	}
}

//Metal has no image layouts, so the Zest one is only kept for the code that asks what an image was last used as
zest_image_layout zest__mtl_image_layout_from_resource_state(zest_resource_state state, zest_bool is_depth) {
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

MTLTextureType zest__mtl_image_texture_type(zest_image image, zest_uint layer_count, zest_uint sample_count, zest_image_flags flags) {
	if (image->info.extent.depth > 1) return MTLTextureType3D;
	if (ZEST__FLAGGED(flags, zest_image_flag_cubemap)) return layer_count > 6 ? MTLTextureTypeCubeArray : MTLTextureTypeCube;
	zest_bool is_array = layer_count > 1 || ZEST__FLAGGED(flags, zest_image_flag_force_image_array);
	if (sample_count > 1) return is_array ? MTLTextureType2DMultisampleArray : MTLTextureType2DMultisample;
	return is_array ? MTLTextureType2DArray : MTLTextureType2D;
}

MTLTextureType zest__mtl_view_texture_type(zest_image_view_type view_type, zest_bool multisampled) {
	switch (view_type) {
		case zest_image_view_type_3d:			return MTLTextureType3D;
		case zest_image_view_type_cube:			return MTLTextureTypeCube;
		case zest_image_view_type_cube_array:	return MTLTextureTypeCubeArray;
		case zest_image_view_type_1d_array:
		case zest_image_view_type_2d_array:		return multisampled ? MTLTextureType2DMultisampleArray : MTLTextureType2DArray;
		default:								return multisampled ? MTLTextureType2DMultisample : MTLTextureType2D;
	}
}

//The depth or stencil half of a combined format has to be named when it's copied
MTLBlitOption zest__mtl_blit_option(zest_format format, zest_image_aspect_flags aspect) {
	if (format != zest_format_d24_unorm_s8_uint && format != zest_format_d32_sfloat_s8_uint && format != zest_format_x8_d24_unorm_pack32) return MTLBlitOptionNone;
	return ZEST__FLAGGED(aspect, zest_image_aspect_stencil_bit) ? MTLBlitOptionStencilFromDepthStencil : MTLBlitOptionDepthFromDepthStencil;
}

//The formats a CAMetalLayer takes pass through. Anything else gives BGRA8Unorm and returns false.
zest_bool zest__mtl_swapchain_format(zest_format requested, MTLPixelFormat *out_pixel_format, zest_format *out_reported) {
	switch (requested) {
		case zest_format_b8g8r8a8_unorm:
		case zest_format_b8g8r8a8_srgb:
		case zest_format_r8g8b8a8_unorm:
		case zest_format_r8g8b8a8_srgb:
		case zest_format_a2r10g10b10_unorm_pack32:
		case zest_format_a2b10g10r10_unorm_pack32:
		case zest_format_r16g16b16a16_sfloat:
			*out_pixel_format = zest__mtl_pixel_format(requested);
			*out_reported = requested;
			return ZEST_TRUE;
		default:
			*out_pixel_format = MTLPixelFormatBGRA8Unorm;
			*out_reported = zest_format_b8g8r8a8_unorm;
			return ZEST_FALSE;
	}
}

MTLLoadAction zest__mtl_load_action(zest_load_op load_op) {
	switch (load_op) {
		case zest_load_op_clear:	return MTLLoadActionClear;
		case zest_load_op_load:		return MTLLoadActionLoad;
		default:					return MTLLoadActionDontCare;
	}
}

MTLStoreAction zest__mtl_store_action(zest_store_op store_op) {
	return store_op == zest_store_op_store ? MTLStoreActionStore : MTLStoreActionDontCare;
}

//Metal rejects a scissor that leaves the render target, where Vulkan and D3D12 accept one, so only the part inside the target is kept
MTLScissorRect zest__mtl_clamped_scissor(int x, int y, zest_uint width, zest_uint height, zest_uint target_width, zest_uint target_height) {
	//64 bits, so an origin plus a size at the 32 bit limit can't wrap
	int64_t left = x > 0 ? x : 0;
	int64_t top = y > 0 ? y : 0;
	int64_t right = (int64_t)x + width;
	int64_t bottom = (int64_t)y + height;
	if (right > target_width) right = target_width;
	if (bottom > target_height) bottom = target_height;
	MTLScissorRect rect = { 0, 0, 0, 0 };
	if (right <= left || bottom <= top) return rect;
	rect.x = (NSUInteger)left;
	rect.y = (NSUInteger)top;
	rect.width = (NSUInteger)(right - left);
	rect.height = (NSUInteger)(bottom - top);
	return rect;
}
// -- End Type_converters

// -- Inline_helpers
// -- End Inline_helpers

// -- Initialisation_functions
zest_device_builder zest_BeginMetalDeviceBuilder(zest_device_init_flags flags) {
	zest__register_platform(zest_platform_metal, zest__mtl_initialise_platform_callbacks);
	zest_device_builder builder = zest__begin_device_builder();
	builder->graphics_queue_count = -1;
	builder->compute_queue_count = -1;
	builder->transfer_queue_count = -1;
	builder->platform = zest_platform_metal;
	//Metal loads the same SPIR-V as Vulkan, so the two backends share a cache folder
	builder->cached_shader_path = "./spv/";
	builder->flags = flags;
	return builder;
}

//flush_legacy_framebuffers and the debug markers stay unset: shared code checks them before calling
void zest__mtl_initialise_platform_callbacks(zest_platform_t *platform) {
	//Frame Graph Platform Commands
	platform->begin_command_buffer = zest__mtl_begin_command_buffer;
	platform->end_command_buffer = zest__mtl_end_command_buffer;
	platform->set_next_command_buffer = zest__mtl_set_next_command_buffer;
	platform->acquire_barrier = zest__mtl_acquire_barrier;
	platform->release_barrier = zest__mtl_release_barrier;
	platform->new_execution_backend = zest__mtl_new_execution_backend;
	platform->get_frame_graph_semaphores = zest__mtl_get_frame_graph_semaphores;
	platform->submit_frame_graph_batch = zest__mtl_submit_frame_graph_batch;
	platform->begin_render_pass = zest__mtl_begin_render_pass;
	platform->end_render_pass = zest__mtl_end_render_pass;
	platform->carry_over_semaphores = zest__mtl_carry_over_semaphores;
	platform->create_execution_timeline_backend = zest__mtl_create_execution_timeline_backend;
	platform->cleanup_execution_timeline_backend = zest__mtl_cleanup_execution_timeline_backend;
	platform->add_frame_graph_buffer_barrier = zest__mtl_add_frame_graph_buffer_barrier;
	platform->add_frame_graph_image_barrier = zest__mtl_add_frame_graph_image_barrier;
	platform->present_frame = zest__mtl_present_frame;
	platform->dummy_submit_for_present_only = zest__mtl_dummy_submit_for_present_only;
	platform->acquire_swapchain_image = zest__mtl_acquire_swapchain_image;
	platform->new_frame_graph_image_backend = zest__mtl_new_frame_graph_image_backend;
	//Buffer and memory
	platform->create_buffer_allocator_backend = zest__mtl_create_buffer_allocator_backend;
	platform->add_buffer_memory_pool = zest__mtl_add_buffer_memory_pool;
	platform->create_image_memory_pool = zest__mtl_create_image_memory_pool;
	platform->create_device_memory = zest__mtl_create_device_memory;
	platform->get_memory_budget = zest__mtl_get_memory_budget;
	platform->map_memory = zest__mtl_map_memory;
	platform->unmap_memory = zest__mtl_unmap_memory;
	platform->flush_used_buffers = zest__mtl_flush_used_buffers;
	platform->cmd_copy_buffer_one_time = zest__mtl_cmd_copy_buffer_one_time;
	//Images
	platform->is_image_format_supported = zest__mtl_is_image_format_supported;
	platform->create_image = zest__mtl_create_image;
	platform->create_transient_image_unbound = zest__mtl_create_transient_image_unbound;
	platform->bind_image_arena = zest__mtl_bind_image_arena;
	platform->create_arena_backing = zest__mtl_create_arena_backing;
	platform->free_arena_backing = zest__mtl_free_arena_backing;
	platform->create_image_view = zest__mtl_create_image_view;
	platform->create_image_views_per_mip = zest__mtl_create_image_views_per_mip;
	platform->copy_buffer_regions_to_image = zest__mtl_copy_buffer_regions_to_image;
	platform->transition_image = zest__mtl_transition_image;
	platform->create_sampler = zest__mtl_create_sampler;
	platform->get_image_raw_layout = zest__mtl_get_image_raw_layout;
	platform->image_layout_is_valid_for_descriptor = zest__mtl_image_layout_is_valid_for_descriptor;
	platform->copy_buffer_to_image = zest__mtl_copy_buffer_to_image;
	platform->generate_mipmaps = zest__mtl_generate_mipmaps;
	//Pipelines
	platform->build_pipeline = zest__mtl_build_pipeline;
	platform->build_pipeline_layout = zest__mtl_build_pipeline_layout;
	platform->finish_compute = zest__mtl_finish_compute;
	//Semaphores
	platform->wait_for_renderer_semaphore = zest__mtl_wait_for_renderer_semaphore;
	platform->wait_for_fif_semaphore = zest__mtl_wait_for_fif_semaphore;
	platform->queue_wait_idle = zest__mtl_queue_wait_idle;
	platform->wait_for_timeline = zest__mtl_wait_for_timeline;
	//Set layouts
	platform->create_set_layout = zest__mtl_create_set_layout;
	platform->create_set_pool = zest__mtl_create_set_pool;
	platform->create_bindless_set = zest__mtl_create_bindless_set;
	platform->update_bindless_image_descriptor = zest__mtl_update_bindless_image_descriptor;
	platform->update_bindless_storage_buffer_descriptor = zest__mtl_update_bindless_storage_buffer_descriptor;
	platform->update_bindless_uniform_buffer_descriptor = zest__mtl_update_bindless_uniform_buffer_descriptor;
	//Command buffers/queues
	platform->reset_queue_command_pool = zest__mtl_reset_queue_command_pool;
	//General Context
	platform->set_depth_format = zest__mtl_set_depth_format;
	platform->initialise_context_backend = zest__mtl_initialise_context_backend;
	platform->get_msaa_sample_count = zest__mtl_get_msaa_sample_count;
	platform->initialise_swapchain = zest__mtl_initialise_swapchain;
	platform->initialise_context_queue_backend = zest__mtl_initialise_context_queue_backend;
	platform->get_db_overlay_vertex_shader = zest__mtl_get_db_overlay_vertex_shader;
	platform->get_db_overlay_fragment_shader = zest__mtl_get_db_overlay_fragment_shader;
	//Device/OS
	platform->wait_for_idle_device = zest__mtl_wait_for_idle_device;
	platform->initialise_device = zest__mtl_initialise_device;
	platform->query_device_capabilities = zest__mtl_query_device_capabilities;
	platform->create_window_surface = zest__mtl_create_window_surface;
	//Shader Compiling
	platform->validate_shader = zest__mtl_validate_shader;
	platform->compile_shader = zest__mtl_compile_shader;
	//Create backends
	platform->new_frame_graph_semaphores_backend = zest__mtl_new_frame_graph_semaphores_backend;
	platform->new_execution_barriers_backend = zest__mtl_new_execution_barriers_backend;
	platform->new_pipeline_backend = zest__mtl_new_pipeline_backend;
	platform->new_pipeline_layout_backend = zest__mtl_new_pipeline_layout_backend;
	platform->new_memory_pool_backend = zest__mtl_new_memory_pool_backend;
	platform->new_memory_backend = zest__mtl_new_memory_backend;
	platform->new_device_backend = zest__mtl_new_device_backend;
	platform->new_context_backend = zest__mtl_new_context_backend;
	platform->new_frame_graph_context_backend = zest__mtl_new_frame_graph_context_backend;
	platform->new_swapchain_backend = zest__mtl_new_swapchain_backend;
	platform->new_image_backend = zest__mtl_new_image_backend;
	platform->new_compute_backend = zest__mtl_new_compute_backend;
	platform->new_queue_backend = zest__mtl_new_queue_backend;
	platform->new_submission_batch_backend = zest__mtl_new_submission_batch_backend;
	platform->new_set_layout_backend = zest__mtl_new_set_layout_backend;
	platform->new_descriptor_pool_backend = zest__mtl_new_descriptor_pool_backend;
	platform->new_sampler_backend = zest__mtl_new_sampler_backend;
	platform->new_context_queue_backend = zest__mtl_new_context_queue_backend;
	//Cleanup backends
	platform->cleanup_frame_graph_semaphore = zest__mtl_cleanup_frame_graph_semaphore;
	platform->cleanup_image_backend = zest__mtl_cleanup_image_backend;
	platform->cleanup_image_view_backend = zest__mtl_cleanup_image_view_backend;
	platform->cleanup_image_view_array_backend = zest__mtl_cleanup_image_view_array_backend;
	platform->cleanup_buffer_allocator_backend = zest__mtl_cleanup_buffer_allocator_backend;
	platform->cleanup_memory_pool_backend = zest__mtl_cleanup_memory_pool_backend;
	platform->cleanup_memory_backend = zest__mtl_cleanup_memory_backend;
	platform->cleanup_device_backend = zest__mtl_cleanup_device_backend;
	platform->reinit_logical_device = zest__mtl_reinit_logical_device;
	platform->cleanup_context_backend = zest__mtl_cleanup_context_backend;
	platform->destroy_context_surface = zest__mtl_destroy_context_surface;
	platform->cleanup_swapchain_backend = zest__mtl_cleanup_swapchain_backend;
	platform->cleanup_compute_backend = zest__mtl_cleanup_compute_backend;
	platform->cleanup_pipeline_backend = zest__mtl_cleanup_pipeline_backend;
	platform->cleanup_pipeline_layout_backend = zest__mtl_cleanup_pipeline_layout_backend;
	platform->cleanup_sampler_backend = zest__mtl_cleanup_sampler_backend;
	platform->cleanup_queue_backend = zest__mtl_cleanup_queue_backend;
	platform->cleanup_context_queue_backend = zest__mtl_cleanup_context_queue_backend;
	platform->cleanup_set_layout_backend = zest__mtl_cleanup_set_layout_backend;
	//GPU Profiling
	platform->create_gpu_profiler_backend = zest__mtl_create_gpu_profiler_backend;
	platform->cleanup_gpu_profiler_backend = zest__mtl_cleanup_gpu_profiler_backend;
	platform->reset_gpu_query_pool = zest__mtl_reset_gpu_query_pool;
	platform->write_timestamp = zest__mtl_write_timestamp;
	platform->readback_gpu_timestamps = zest__mtl_readback_gpu_timestamps;
	//Debugging
	platform->collect_validation_messages = zest__mtl_collect_validation_messages;
	platform->set_object_name = zest__mtl_set_object_name;
	platform->set_image_name = zest__mtl_set_image_name;
	platform->set_buffer_pool_name = zest__mtl_set_buffer_pool_name;
	platform->get_final_signal_ptr = zest__mtl_get_final_signal_ptr;
	platform->get_final_wait_ptr = zest__mtl_get_final_wait_ptr;
	platform->get_resource_ptr = zest__mtl_get_resource_ptr;
	platform->get_buffer_ptr = zest__mtl_get_buffer_ptr;
	platform->get_swapchain_wait_semaphore = zest__mtl_get_swapchain_wait_semaphore;
	platform->get_swapchain_signal_semaphore = zest__mtl_get_swapchain_signal_semaphore;
	platform->get_swapchain_identifier = zest__mtl_get_swapchain_identifier;
	platform->create_test_render_pass = zest__mtl_create_test_render_pass;
	//Command recording
	platform->blit_image_mip = zest__mtl_blit_image_mip;
	platform->copy_image_mip = zest__mtl_copy_image_mip;
	platform->insert_compute_image_barrier = zest__mtl_insert_compute_image_barrier;
	platform->set_screensized_viewport = zest__mtl_set_screensized_viewport;
	platform->scissor = zest__mtl_scissor;
	platform->viewport = zest__mtl_viewport;
	platform->clip = zest__mtl_clip;
	platform->bind_descriptor_sets = zest__mtl_bind_descriptor_sets;
	platform->bind_pipeline = zest__mtl_bind_pipeline;
	platform->bind_compute_pipeline = zest__mtl_bind_compute_pipeline;
	platform->copy_buffer = zest__mtl_copy_buffer;
	platform->upload_buffer = zest__mtl_upload_buffer;
	platform->bind_vertex_buffer = zest__mtl_bind_vertex_buffer;
	platform->bind_index_buffer = zest__mtl_bind_index_buffer;
	platform->image_clear = zest__mtl_image_clear;
	platform->bind_mesh_vertex_buffer = zest__mtl_bind_mesh_vertex_buffer;
	platform->bind_mesh_index_buffer = zest__mtl_bind_mesh_index_buffer;
	platform->dispatch_compute = zest__mtl_dispatch_compute;
	platform->send_push_constants = zest__mtl_send_push_constants;
	platform->draw = zest__mtl_draw;
	platform->draw_layer_instruction = zest__mtl_draw_layer_instruction;
	platform->draw_indexed = zest__mtl_draw_indexed;
	platform->draw_indexed_indirect = zest__mtl_draw_indexed_indirect;
	platform->draw_indirect = zest__mtl_draw_indirect;
	platform->set_depth_bias = zest__mtl_set_depth_bias;
	//Immediate commands
	platform->imm_begin_command_buffer = zest__mtl_imm_begin_command_buffer;
	platform->imm_end_command_buffer = zest__mtl_imm_end_command_buffer;
	platform->imm_fill_buffer = zest__mtl_imm_fill_buffer;
	platform->imm_update_buffer = zest__mtl_imm_update_buffer;
	platform->imm_clear_color_image = zest__mtl_imm_clear_color_image;
	platform->imm_clear_depth_stencil_image = zest__mtl_imm_clear_depth_stencil_image;
	platform->imm_blit_image = zest__mtl_imm_blit_image;
	platform->imm_resolve_image = zest__mtl_imm_resolve_image;
	platform->imm_send_push_constants = zest__mtl_imm_send_push_constants;
	platform->imm_bind_compute_pipeline = zest__mtl_imm_bind_compute_pipeline;
	platform->imm_dispatch_compute = zest__mtl_imm_dispatch_compute;
	platform->copy_bitmap_to_image = zest__mtl_copy_bitmap_to_image;
	platform->copy_image_to_bitmap = zest__mtl_copy_image_to_bitmap;
}
void zest_PrepareMetalValidation(zest_device_init_flags flags) {
	if (ZEST__NOT_FLAGGED(flags, zest_device_init_flag_enable_validation_layers)) return;
	if (!zest__mtl_forced_validation_unavailable) {
		setenv("MTL_DEBUG_LAYER", "1", 1);
		//A launcher that chose to abort on errors keeps its choice
		setenv("MTL_DEBUG_LAYER_ERROR_MODE", "nslog", 0);
		if ((flags & zest_device_init_flag_enable_validation_layers_with_sync) == zest_device_init_flag_enable_validation_layers_with_sync) {
			setenv("MTL_SHADER_VALIDATION", "1", 1);
		}
	}
	zest__mtl_start_validation_reader();
}

//Only in time for a process that has made no window and no Metal device yet. A windowed app calls zest_PrepareMetalValidation itself, earlier.
void zest__mtl_prepare_validation(zest_device device) {
	zest_mtl_device_backend_t *backend = ZEST_MTL_BACKEND(device, device);
	const char *shader_validation = getenv("MTL_SHADER_VALIDATION");
	backend->launcher_shader_validation = shader_validation && strcmp(shader_validation, "0") != 0;
	zest_PrepareMetalValidation(device->setup_info.flags);
}

//Makes one harmless mistake to find out whether the debug layer is really on, because asking too late is silent
void zest__mtl_confirm_validation(zest_device device) {
	zest_mtl_device_backend_t *backend = ZEST_MTL_BACKEND(device, device);
	zest_mtl_validation_reader_t *reader = &zest__mtl_validation_reader;
	const char *log_path = device->log_path.str;
	if (ZEST__NOT_FLAGGED(device->setup_info.flags, zest_device_init_flag_enable_validation_layers)) {
		if (reader->saw_api_enabled) {
			ZEST_APPEND_LOG(log_path, "Metal validation was turned on earlier in this process, so it stays on for this device.");
		}
		return;
	}
	const char *error_mode = getenv("MTL_DEBUG_LAYER_ERROR_MODE");
	zest_bool logged_errors = error_mode && strcmp(error_mode, "nslog") == 0;
	zest_bool confirmed = ZEST_FALSE;
	if (zest__mtl_forced_validation_unavailable || !reader->started) {
		confirmed = ZEST_FALSE;
	} else if (!logged_errors) {
		//The probe would abort the process in this mode, so the layer is taken on trust and its errors aren't counted
		confirmed = ZEST_TRUE;
		ZEST_APPEND_LOG(log_path, "MTL_DEBUG_LAYER_ERROR_MODE is %s, so Metal API validation errors aren't counted by zest_GetValidationErrorCount.", error_mode);
	} else {
		zest__mtl_sync_validation_reader();
		pthread_mutex_lock(&zest__mtl_validation_lock);
		reader->hiding_probe = ZEST_TRUE;
		pthread_mutex_unlock(&zest__mtl_validation_lock);
		[[backend->device newBufferWithLength:0 options:MTLResourceStorageModeShared] release];
		zest__mtl_sync_validation_reader();
		pthread_mutex_lock(&zest__mtl_validation_lock);
		reader->hiding_probe = ZEST_FALSE;
		if (reader->pending_count && strncmp(reader->pending[reader->pending_count - 1], "Buffer Validation", 17) == 0) {
			reader->pending_count--;
			reader->record_open = ZEST_FALSE;
			confirmed = ZEST_TRUE;
		}
		pthread_mutex_unlock(&zest__mtl_validation_lock);
	}
	if (!confirmed) {
		ZEST_APPEND_LOG(log_path, "Metal validation was asked for but can't be turned on, because a window or another Metal or MoltenVK device already existed in this process. Call zest_PrepareMetalValidation with the device's flags before creating the window, or launch with MTL_DEBUG_LAYER=1. Disabling validation.");
		ZEST_ALERT("Metal validation was asked for but can't be turned on, because a window or another Metal or MoltenVK device already existed in this process. Call zest_PrepareMetalValidation with the device's flags before creating the window, or launch with MTL_DEBUG_LAYER=1.");
		ZEST__UNFLAG(device->setup_info.flags, zest_device_init_flag_enable_validation_layers_with_sync);
		return;
	}
	backend->api_validation = ZEST_TRUE;
	backend->shader_validation = reader->saw_shader_enabled || backend->launcher_shader_validation;
	zest_bool shader_validation_wanted = (device->setup_info.flags & zest_device_init_flag_enable_validation_layers_with_sync) == zest_device_init_flag_enable_validation_layers_with_sync;
	if (shader_validation_wanted && !backend->shader_validation) {
		ZEST_APPEND_LOG(log_path, "Metal shader validation was asked for but the process's Metal device was created without it. Ask for it on the first device the process creates.");
	}
	ZEST_APPEND_LOG(log_path, "Metal API validation is on. Shader validation: %s", backend->shader_validation ? "on" : "off");
}

void zest__mtl_query_feature_support(id<MTLDevice> native_device, zest_mtl_feature_support_t *support) {
	*support = ZEST__ZERO_INIT(zest_mtl_feature_support_t);
	NSOperatingSystemVersion minimum_version;
	minimum_version.majorVersion = 15;
	minimum_version.minorVersion = 0;
	minimum_version.patchVersion = 0;
	support->macos_15 = [[NSProcessInfo processInfo] isOperatingSystemAtLeastVersion:minimum_version];
	support->metal3 = [native_device supportsFamily:MTLGPUFamilyMetal3];
	support->argument_buffers_tier2 = [native_device argumentBuffersSupport] >= MTLArgumentBuffersTier2;
	support->bc_compression = [native_device supportsBCTextureCompression];
	support->apple_gpu = [native_device supportsFamily:MTLGPUFamilyApple2];
	support->depth24_stencil8 = [native_device isDepth24Stencil8PixelFormatSupported];
	support->counter_sampling_at_stage = [native_device supportsCounterSampling:MTLCounterSamplingPointAtStageBoundary];
	support->counter_sampling_at_draw = [native_device supportsCounterSampling:MTLCounterSamplingPointAtDrawBoundary];
	support->max_argument_buffer_samplers = (zest_uint)[native_device maxArgumentBufferSamplerCount];
	support->max_buffer_length = (zest_u64)[native_device maxBufferLength];
}

zest_bool zest__mtl_meets_minimum_spec(const zest_mtl_feature_support_t *support, const char **out_missing) {
	const char *missing = NULL;
	if (!support->macos_15) {
		missing = "macOS 15";
	} else if (!support->metal3) {
		missing = "the Metal 3 GPU family";
	} else if (!support->argument_buffers_tier2) {
		missing = "argument buffers tier 2";
	}
	if (zest__mtl_forced_missing_feature) {
		missing = zest__mtl_forced_missing_feature;
	}
	*out_missing = missing;
	return missing == NULL;
}

zest_capability_flags zest__mtl_translate_capabilities(const zest_mtl_feature_support_t *support) {
	zest_capability_flags supported = zest_capability_multi_draw_indirect |
		zest_capability_nonuniform_storage_buffer_indexing | zest_capability_nonuniform_uniform_buffer_indexing |
		zest_capability_nonuniform_storage_image_indexing | zest_capability_nonuniform_sampled_image_indexing |
		zest_capability_anisotropic_filtering | zest_capability_wireframe | zest_capability_image_cube_array |
		zest_capability_shader_int64 | zest_capability_fragment_stores_and_atomics;
	if (support->bc_compression) supported |= zest_capability_texture_compression_bc;
	if (support->apple_gpu) supported |= zest_capability_texture_compression_astc_ldr | zest_capability_texture_compression_etc2;
	return supported;
}

zest_bool zest__mtl_query_device_capabilities(zest_device device) {
	zest_mtl_device_backend_t *backend = ZEST_MTL_BACKEND(device, device);
	const zest_mtl_feature_support_t *support = &backend->support;
	const char *log_path = device->log_path.str;

	zest_capability_flags supported = zest__mtl_translate_capabilities(support);
	device->capabilities.supported = supported;
	device->capabilities.meets_minimum_spec = ZEST_TRUE;
	zest__resolve_enabled_capabilities(device, supported);

	device->max_image_size = 16384;
	device->max_multiview_view_count = 1;
	//The values D3D12 reports, so offsets padded for one backend fit the others
	device->min_uniform_buffer_offset_alignment = 256;
	device->min_storage_buffer_offset_alignment = 16;
	device->max_uniform_buffer_size = 65536;
	device->max_storage_buffer_size = (zest_size)support->max_buffer_length;
	device->buffer_offset_granularity = ZEST_BUFFER_OFFSET_GRANULARITY;
	//No allocation limit, so the shared code's limit warnings stay quiet
	device->max_memory_allocation_count = 0;
	device->max_bindless_samplers = support->max_argument_buffer_samplers;
	//A table entry is 8 bytes, so these are ceilings on the table's size, not a device limit
	device->max_bindless_sampled_images = 100000;
	device->max_bindless_storage_buffers = 100000;
	device->max_bindless_storage_images = 100000;
	device->max_bindless_uniform_buffers = 100000;
	ZEST_APPEND_LOG(log_path, "Bindless descriptor ceilings: samplers %u, sampled images %u, storage buffers %u, storage images %u, uniform buffers %u",
		device->max_bindless_samplers, device->max_bindless_sampled_images, device->max_bindless_storage_buffers,
		device->max_bindless_storage_images, device->max_bindless_uniform_buffers);
	ZEST_APPEND_LOG(log_path, "Metal features: BC: %s, Apple GPU formats (ASTC, ETC2): %s, Depth24Stencil8: %s, counter sampling at stage boundaries: %s, at draw boundaries: %s, max buffer length: %llu",
		support->bc_compression ? "yes" : "no", support->apple_gpu ? "yes" : "no",
		support->depth24_stencil8 ? "yes" : "no", support->counter_sampling_at_stage ? "yes" : "no", support->counter_sampling_at_draw ? "yes" : "no",
		(unsigned long long)support->max_buffer_length);
	zest__log_supported_capabilities(device, supported);
	return ZEST_TRUE;
}

void zest__mtl_add_queue_manager_list(zest_device device, zest_device_queue_type queue_type, const zest_uint *family_order, zest_uint order_count) {
	zest_queue_manager_list_t *manager_list = (zest_queue_manager_list_t *)ZEST__ALLOCATE(device->allocator, sizeof(zest_queue_manager_list_t));
	manager_list->managers = 0;
	for (zest_uint order_index = 0; order_index != order_count; ++order_index) {
		zest_vec_push(device->allocator, manager_list->managers, device->queue_families[family_order[order_index]]);
	}
	zest_vec_push(device->allocator, device->queue_managers, manager_list);
	device->queue_pool[queue_type] = manager_list;
}

zest_bool zest__mtl_create_queues(zest_device device) {
	const char *log_path = device->log_path.str;
	zest_vec_resize(device->allocator, device->queue_families, ZEST__MTL_QUEUE_FAMILY_COUNT);
	ZEST_APPEND_LOG(log_path, "Iterate available queues:");
	for (zest_uint family_index = 0; family_index != ZEST__MTL_QUEUE_FAMILY_COUNT; ++family_index) {
		const zest_mtl_queue_family_t *family = &zest__mtl_queue_families[family_index];
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
	zest__mtl_add_queue_manager_list(device, zest_queue_graphics, graphics_order, 1);
	zest__mtl_add_queue_manager_list(device, zest_queue_compute, compute_order, 2);
	zest__mtl_add_queue_manager_list(device, zest_queue_transfer, transfer_order, 3);
	zest_map_insert_key(device->allocator, device->queue_names, ZEST_QUEUE_FAMILY_IGNORED, "Ignored");

	zest_bool queues_created = ZEST_TRUE;
	for (zest_uint family_index = 0; family_index != ZEST__MTL_QUEUE_FAMILY_COUNT; ++family_index) {
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
			queue->backend = (zest_queue_backend)zest__mtl_new_queue_backend(device, family_index);
			queues_created &= queue->backend != NULL;
			queues_created &= zest__initialise_timeline(device, &queue->timeline);
		}
	}

	if (!queues_created) {
		ZEST_APPEND_LOG(log_path, "Fatal Error: could not create the Metal command queues, their fences or their shared events.");
		zest__mtl_cleanup_queues(device);
		return ZEST_FALSE;
	}
	return ZEST_TRUE;
}

//What a device reset drops and makes again: the residency set and the queues attached to it
zest_bool zest__mtl_create_device_objects(zest_device device) {
	zest_mtl_device_backend_t *backend = ZEST_MTL_BACKEND(device, device);
	@autoreleasepool {
		MTLResidencySetDescriptor *residency_descriptor = [[MTLResidencySetDescriptor alloc] init];
		[residency_descriptor setLabel:@"Zest Residency Set"];
		[residency_descriptor setInitialCapacity:64];
		NSError *error = nil;
		backend->residency_set = [backend->device newResidencySetWithDescriptor:residency_descriptor error:&error];
		[residency_descriptor release];
		if (!backend->residency_set) {
			ZEST_APPEND_LOG(device->log_path.str, "Fatal: the Metal residency set couldn't be created: %s", error ? [[error localizedDescription] UTF8String] : "no error given");
			return ZEST_FALSE;
		}
	}
	return zest__mtl_create_queues(device);
}

zest_bool zest__mtl_initialise_device(zest_device device) {
	zest_mtl_device_backend_t *backend = ZEST_MTL_BACKEND(device, device);
	const char *log_path = device->log_path.str;
	@autoreleasepool {
		zest__mtl_prepare_validation(device);
		backend->device = MTLCreateSystemDefaultDevice();
		ZEST_APPEND_LOG(log_path, "=== Device Compatibility Report ===");
		if (!backend->device) {
			ZEST_APPEND_LOG(log_path, "Fatal: this machine has no Metal device.");
			return ZEST_FALSE;
		}
		zest__mtl_confirm_validation(device);

		zest__mtl_query_feature_support(backend->device, &backend->support);
		const char *missing = NULL;
		ZEST_APPEND_LOG(log_path, "Device: %s", [[backend->device name] UTF8String]);
		if (!zest__mtl_meets_minimum_spec(&backend->support, &missing)) {
			ZEST_APPEND_LOG(log_path, "  MISSING (required): %s", missing);
			ZEST_APPEND_LOG(log_path, "Fatal: the Metal device doesn't meet the Zest minimum spec (see MISSING lines above).");
			return ZEST_FALSE;
		}
		ZEST_APPEND_LOG(log_path, "  Meets the Zest minimum spec: macOS 15, the Metal 3 GPU family and argument buffers tier 2");

		//SPIR-V compiled with ZEST_METAL defined shares a cache folder with Vulkan's, so it needs keys of its own
		device->shader_cache_salt = 0x4D450001;

		if (!zest__mtl_query_device_capabilities(device)) {
			return ZEST_FALSE;
		}
		if (!zest__mtl_create_device_objects(device)) {
			return ZEST_FALSE;
		}
	}
	zest__set_default_pool_sizes(device);
	return ZEST_TRUE;
}

//Metal has one device per process, so a reset keeps it, with its features and validation, and remakes only what was created from it
zest_bool zest__mtl_reinit_logical_device(zest_device device) {
	zest_mtl_device_backend_t *backend = ZEST_MTL_BACKEND(device, device);
	zest_WaitForIdleDevice(device);
	zest__mtl_collect_validation_messages(device);
	zest__mtl_cleanup_queues(device);
	ZEST_MTL_RELEASE(backend->residency_set);
	if (!zest__mtl_create_device_objects(device)) {
		ZEST_APPEND_LOG(device->log_path.str, "The Metal device couldn't be reset, because its residency set or its command queues couldn't be created again.");
		return ZEST_FALSE;
	}
	return ZEST_TRUE;
}

id<MTLDevice> zest_GetMetalDevice(zest_device device) {
	ZEST_ASSERT_HANDLE(device);		//Not a valid device handle
	return ZEST_MTL_BACKEND(device, device)->device;
}
// -- End Initialisation_functions

// -- Swapchain_setup
zest_bool zest__mtl_create_window_surface(zest_context context) {
	zest_mtl_context_backend_t *backend = ZEST_MTL_BACKEND(context, context);
#if defined(SDL_MAJOR_VERSION)
	SDL_Window *window = (SDL_Window *)zest_Window(context);
	//macOS never frees a layer that has had its display sync turned off, so a window gets one view that every context on it presents to
	SDL_MetalView view = (SDL_MetalView)SDL_GetWindowData(window, ZEST__MTL_SDL_VIEW_NAME);
	if (!view) {
		//SDL's view keeps the layer's drawable size matched to the window and its backing scale
		view = SDL_Metal_CreateView(window);
		if (!view) {
			ZEST_APPEND_LOG(context->device->log_path.str, "SDL couldn't create a Metal view for the window: %s", SDL_GetError());
			return ZEST_FALSE;
		}
		SDL_SetWindowData(window, ZEST__MTL_SDL_VIEW_NAME, view);
	}
	backend->layer = (CAMetalLayer *)SDL_Metal_GetLayer(view);
#else
	id native_layer = (id)context->window_data.native_handle;
	if ([native_layer isKindOfClass:[CAMetalLayer class]]) {
		backend->layer = [(CAMetalLayer *)native_layer retain];
	}
#endif
	if (!backend->layer) {
		ZEST_APPEND_LOG(context->device->log_path.str, "The window has no CAMetalLayer to present to. Without SDL2, set the window data's native_handle to the window's CAMetalLayer.");
		return ZEST_FALSE;
	}
	return ZEST_TRUE;
}

//The size the layer's next drawable will have. A zero size is given as one pixel so the swapchain's size is never zero.
zest_extent2d_t zest__mtl_layer_extent(CAMetalLayer *layer) {
	CGSize drawable_size = [layer drawableSize];
	zest_extent2d_t extent;
	extent.width = (zest_uint)ZEST__MAX(drawable_size.width, 1.0);
	extent.height = (zest_uint)ZEST__MAX(drawable_size.height, 1.0);
	return extent;
}

//A layer hands out one drawable at a time, so the swapchain has one image, which each acquire points at that frame's drawable
zest_bool zest__mtl_initialise_swapchain(zest_context context) {
	zest_swapchain swapchain = context->swapchain;
	zest_device device = context->device;
	zest_mtl_context_backend_t *backend = ZEST_MTL_BACKEND(context, context);
	zest_mtl_swapchain_backend_t *swapchain_backend = ZEST_MTL_BACKEND(swapchain, swapchain);
	zest_format reported_format = zest_format_undefined;
	if (!zest__mtl_swapchain_format(context->create_info.color_format, &swapchain_backend->pixel_format, &reported_format)) {
		ZEST_APPEND_LOG(device->log_path.str, "Format %i can't be a CAMetalLayer format, so the swapchain uses BGRA8Unorm instead.", (int)context->create_info.color_format);
	}
	ZEST_ASSERT(context->window_data.window_sizes_callback, "You must set a window size callback in the window data struct.");
	int framebuffer_width = 0;
	int framebuffer_height = 0;
	int window_width = 0;
	int window_height = 0;
	context->window_data.window_sizes_callback(&context->window_data, &framebuffer_width, &framebuffer_height, &window_width, &window_height);
	context->window_data.width = window_width;
	context->window_data.height = window_height;

	[backend->layer setDevice:ZEST_MTL_BACKEND(device, device)->device];
	[backend->layer setPixelFormat:swapchain_backend->pixel_format];
	[backend->layer setFramebufferOnly:YES];
	[backend->layer setDisplaySyncEnabled:ZEST__FLAGGED(context->flags, zest_context_flag_vsync_enabled) ? YES : NO];
	zest_extent2d_t extent = zest__mtl_layer_extent(backend->layer);

	swapchain->format = reported_format;
	swapchain->size = extent;
	swapchain->image_count = 1;
	context->window_extent = extent;
	context->dpi_scale = window_width > 0 ? (float)extent.width / (float)window_width : 1.f;

	zest_vec_resize(context->allocator, swapchain->images, 1);
	zest_vec_resize(context->allocator, swapchain->views, 1);
	swapchain->images[0] = zest__image_zero;
	swapchain->views[0] = NULL;
	zest_image image = &swapchain->images[0];
	image->info = zest_CreateImageInfo(extent.width, extent.height);
	image->info.aspect_flags = zest_image_aspect_color_bit;
	image->info.format = reported_format;
	image->layout = zest_image_layout_present;
	zest_mtl_image_backend_t *image_backend = (zest_mtl_image_backend_t *)ZEST__ALLOCATE(context->allocator, sizeof(zest_mtl_image_backend_t));
	*image_backend = ZEST__ZERO_INIT(zest_mtl_image_backend_t);
	image->backend = (zest_image_backend)image_backend;

	void *memory = ZEST__ALLOCATE(context->allocator, sizeof(zest_image_view_t) + sizeof(zest_mtl_image_view_backend_t));
	zest_image_view_t *view = (zest_image_view_t *)memory;
	zest_mtl_image_view_backend_t *view_backend = (zest_mtl_image_view_backend_t *)((char *)memory + sizeof(zest_image_view_t));
	*view = ZEST__ZERO_INIT(zest_image_view_t);
	*view_backend = ZEST__ZERO_INIT(zest_mtl_image_view_backend_t);
	view->image = image;
	view->backend = (zest_image_view_backend_t *)view_backend;
	swapchain->views[0] = view;
	return ZEST_TRUE;
}
// -- End Swapchain_setup

// -- Swapchain_presenting
//nextDrawable is where a frame waits for the display
zest_bool zest__mtl_acquire_swapchain_image(zest_swapchain swapchain) {
	zest_context context = swapchain->context;
	zest_mtl_context_backend_t *backend = ZEST_MTL_BACKEND(context, context);
	//A frame that never presented still holds its drawable, and a layer stalls once two are held
	ZEST_MTL_RELEASE(backend->drawable);
	zest_extent2d_t extent = zest__mtl_layer_extent(backend->layer);
	if (extent.width != swapchain->size.width || extent.height != swapchain->size.height) {
		return ZEST_FALSE;
	}
	id<MTLTexture> texture = nil;
	@autoreleasepool {
		backend->drawable = [[backend->layer nextDrawable] retain];
		//Asked for inside the pool, as the drawable hands its texture out autoreleased
		texture = [backend->drawable texture];
	}
	if (!backend->drawable) {
		ZEST_REPORT(context->device, zest_report_swapchain_not_acquired, "The window's Metal layer gave no drawable for this frame.");
		return ZEST_FALSE;
	}
	//The window can change size between the check above and the drawable being taken
	if ([texture width] != swapchain->size.width || [texture height] != swapchain->size.height) {
		ZEST_MTL_RELEASE(backend->drawable);
		return ZEST_FALSE;
	}
	//The drawable owns the texture, so neither pointer holds a reference
	ZEST_MTL_BACKEND(image, &swapchain->images[0])->texture = texture;
	ZEST_MTL_BACKEND(image_view, swapchain->views[0])->texture = texture;
	backend->present_count = 0;
	return ZEST_TRUE;
}

//The present was encoded on the frame's last batch before its commit, so this lets the drawable go and looks for a reason to recreate
zest_bool zest__mtl_present_frame(zest_context context, zest_context_queue present_queue) {
	(void)present_queue;
	zest_swapchain swapchain = context->swapchain;
	zest_mtl_context_backend_t *backend = ZEST_MTL_BACKEND(context, context);
	if (backend->drawable && backend->present_count == 0) {
		ZEST_REPORT(context->device, zest_report_submission_failure, "A frame graph asked to present but none of its batches carried the present, so this frame's drawable was dropped.");
	}
	ZEST_MTL_RELEASE(backend->drawable);
	ZEST_MTL_BACKEND(image, &swapchain->images[0])->texture = nil;
	ZEST_MTL_BACKEND(image_view, swapchain->views[0])->texture = nil;
	//A layer changes its display sync in place, so a vsync change needs no new swapchain
	if (ZEST__FLAGGED(context->flags, zest_context_flag_schedule_change_vsync)) {
		if (ZEST__FLAGGED(context->flags, zest_context_flag_vsync_enabled)) {
			ZEST__UNFLAG(context->flags, zest_context_flag_vsync_enabled);
		} else {
			ZEST__FLAG(context->flags, zest_context_flag_vsync_enabled);
		}
		ZEST__UNFLAG(context->flags, zest_context_flag_schedule_change_vsync);
		[backend->layer setDisplaySyncEnabled:ZEST__FLAGGED(context->flags, zest_context_flag_vsync_enabled) ? YES : NO];
	}
	zest_extent2d_t extent = zest__mtl_layer_extent(backend->layer);
	if (!swapchain->framebuffer_resized && extent.width == swapchain->size.width && extent.height == swapchain->size.height) {
		return ZEST_TRUE;
	}
	swapchain->framebuffer_resized = ZEST_FALSE;
	return ZEST_FALSE;
}

//Runs when the frame submitted nothing. A new drawable's contents are undefined, so it's cleared before it's shown, and the frame timeline still advances.
zest_bool zest__mtl_dummy_submit_for_present_only(zest_context context) {
	zest_mtl_context_backend_t *backend = ZEST_MTL_BACKEND(context, context);
	if (!backend->drawable) return ZEST_FALSE;
	zest_uint slot = context->graphics_queue_index;
	zest_uint frame_in_flight = context->current_fif;
	zest_context_queue graphics_queue = context->queues[slot];
	zest_mtl_context_queue_backend_t *queue_backend = ZEST_MTL_BACKEND(context_queue, graphics_queue);
	//This queue only ever runs these submits, so nothing else reads back the ones that have finished
	zest__mtl_reap_command_buffers(context, graphics_queue, ZEST_FALSE);
	zest_bool switched_queue = zest__acquire_context_queue(context, graphics_queue, slot);
	zest_mtl_queue_backend_t *device_queue_backend = ZEST_MTL_BACKEND(queue, graphics_queue->queue);
	zest_execution_timeline ordering_timeline = &context->queue_ordering_timelines[slot];
	zest_execution_timeline frame_timeline = &context->frame_timeline[frame_in_flight];
	id<MTLCommandBuffer> command_buffer = nil;
	@autoreleasepool {
		command_buffer = [[device_queue_backend->queue commandBufferWithUnretainedReferences] retain];
		if (command_buffer) {
			//Orders this submit after the slot's last batch when that ran on a different device queue
			if (switched_queue && ordering_timeline->current_value > 0) {
				[command_buffer encodeWaitForEvent:ZEST_MTL_BACKEND(execution_timeline, ordering_timeline)->event value:ordering_timeline->current_value];
			}
			MTLRenderPassColorAttachmentDescriptor *attachment = [[backend->render_pass_descriptor colorAttachments] objectAtIndexedSubscript:0];
			zest_vec4 color = context->swapchain->clear_color.color;
			[attachment setTexture:[backend->drawable texture]];
			[attachment setLoadAction:MTLLoadActionClear];
			[attachment setStoreAction:MTLStoreActionStore];
			[attachment setClearColor:MTLClearColorMake(color.r, color.g, color.b, color.a)];
			id<MTLRenderCommandEncoder> encoder = [command_buffer renderCommandEncoderWithDescriptor:backend->render_pass_descriptor];
			[attachment setTexture:nil];
			[encoder waitForFence:device_queue_backend->fence beforeStages:MTLRenderStageVertex];
			[encoder updateFence:device_queue_backend->fence afterStages:MTLRenderStageFragment];
			[encoder endEncoding];
		}
	}
	if (!command_buffer) {
		ZEST_REPORT(context->device, zest_report_submission_failure, "Metal couldn't supply a command buffer to present a frame that had no work.");
		return ZEST_FALSE;
	}
	frame_timeline->current_value += 1;
	[command_buffer encodeSignalEvent:ZEST_MTL_BACKEND(execution_timeline, frame_timeline)->event value:frame_timeline->current_value];
	context->frame_sync_timeline[frame_in_flight] = frame_timeline;
	ordering_timeline->current_value += 1;
	[command_buffer encodeSignalEvent:ZEST_MTL_BACKEND(execution_timeline, ordering_timeline)->event value:ordering_timeline->current_value];
	[command_buffer presentDrawable:backend->drawable];
	backend->present_count++;
	[command_buffer commit];
	zest_vec_push(context->allocator, queue_backend->committed[frame_in_flight], command_buffer);
	return ZEST_TRUE;
}
// -- End Swapchain_presenting

// -- Backend_setup_functions
void *zest__mtl_new_device_backend(zest_device device) {
	zest_mtl_device_backend_t *backend = (zest_mtl_device_backend_t *)ZEST__ALLOCATE(device->allocator, sizeof(zest_mtl_device_backend_t));
	*backend = ZEST__ZERO_INIT(zest_mtl_device_backend_t);
	pthread_mutex_init(&backend->residency_lock, NULL);
	return backend;
}

void *zest__mtl_new_memory_pool_backend(zest_buffer_allocator buffer_allocator) {
	zloc_allocator *allocator = buffer_allocator->context ? buffer_allocator->context->allocator : buffer_allocator->device->allocator;
	zest_mtl_device_memory_pool_backend_t *backend = (zest_mtl_device_memory_pool_backend_t *)ZEST__ALLOCATE(allocator, sizeof(zest_mtl_device_memory_pool_backend_t));
	*backend = ZEST__ZERO_INIT(zest_mtl_device_memory_pool_backend_t);
	return backend;
}

void *zest__mtl_new_memory_backend(zest_device device) {
	zest_mtl_device_memory_backend_t *backend = (zest_mtl_device_memory_backend_t *)ZEST__ALLOCATE(device->allocator, sizeof(zest_mtl_device_memory_backend_t));
	*backend = ZEST__ZERO_INIT(zest_mtl_device_memory_backend_t);
	return backend;
}

void *zest__mtl_new_queue_backend(zest_device device, zest_uint family_index) {
	ZEST_ASSERT(family_index < ZEST__MTL_QUEUE_FAMILY_COUNT, "The Metal backend only has three queue families.");
	zest_mtl_device_backend_t *device_backend = ZEST_MTL_BACKEND(device, device);
	zest_mtl_queue_backend_t *backend = (zest_mtl_queue_backend_t *)ZEST__ALLOCATE(device->allocator, sizeof(zest_mtl_queue_backend_t));
	*backend = ZEST__ZERO_INIT(zest_mtl_queue_backend_t);
	@autoreleasepool {
		backend->queue = [device_backend->device newCommandQueue];
		backend->fence = [device_backend->device newFence];
		if (!backend->queue || !backend->fence) {
			ZEST_APPEND_LOG(device->log_path.str, "A Metal command queue or its fence couldn't be created.");
			ZEST_MTL_RELEASE(backend->fence);
			ZEST_MTL_RELEASE(backend->queue);
			ZEST__FREE(device->allocator, backend);
			return NULL;
		}
		[backend->queue setLabel:[NSString stringWithUTF8String:zest__mtl_queue_families[family_index].queue_label]];
		[backend->queue addResidencySet:device_backend->residency_set];
	}
	return backend;
}
void *zest__mtl_new_context_backend(zest_context context) {
	zest_mtl_context_backend_t *backend = (zest_mtl_context_backend_t *)ZEST__ALLOCATE(context->allocator, sizeof(zest_mtl_context_backend_t));
	*backend = ZEST__ZERO_INIT(zest_mtl_context_backend_t);
	return backend;
}

zest_bool zest__mtl_initialise_context_backend(zest_context context) {
	ZEST_MTL_BACKEND(context, context)->render_pass_descriptor = [[MTLRenderPassDescriptor alloc] init];
	return ZEST_TRUE;
}

void *zest__mtl_new_swapchain_backend(zest_context context) {
	zest_mtl_swapchain_backend_t *backend = (zest_mtl_swapchain_backend_t *)ZEST__ALLOCATE(context->allocator, sizeof(zest_mtl_swapchain_backend_t));
	*backend = ZEST__ZERO_INIT(zest_mtl_swapchain_backend_t);
	return backend;
}

void *zest__mtl_new_context_queue_backend(zest_context context) {
	zest_mtl_context_queue_backend_t *backend = (zest_mtl_context_queue_backend_t *)ZEST__ALLOCATE(context->allocator, sizeof(zest_mtl_context_queue_backend_t));
	*backend = ZEST__ZERO_INIT(zest_mtl_context_queue_backend_t);
	return backend;
}

zest_bool zest__mtl_initialise_context_queue_backend(zest_context context, zest_context_queue context_queue) {
	(void)context;
	zest_ForEachFrameInFlight(frame_in_flight) {
		context_queue->current_count[frame_in_flight] = 0;
	}
	return ZEST_TRUE;
}

void *zest__mtl_new_frame_graph_semaphores_backend(zest_context context) {
	zest_mtl_frame_graph_semaphores_backend_t *backend = (zest_mtl_frame_graph_semaphores_backend_t *)ZEST__ALLOCATE(context->allocator, sizeof(zest_mtl_frame_graph_semaphores_backend_t));
	*backend = ZEST__ZERO_INIT(zest_mtl_frame_graph_semaphores_backend_t);
	return backend;
}

void *zest__mtl_new_frame_graph_context_backend(zest_context context) {
	zest_mtl_command_list_backend_t *backend = (zest_mtl_command_list_backend_t *)zest__linear_allocate(&context->frame_graph_allocator[context->current_fif], sizeof(zest_mtl_command_list_backend_t));
	*backend = ZEST__ZERO_INIT(zest_mtl_command_list_backend_t);
	return backend;
}

void *zest__mtl_new_execution_barriers_backend(zloc_linear_allocator_t *allocator) {
	zest_mtl_execution_barriers_backend_t *backend = (zest_mtl_execution_barriers_backend_t *)zest__linear_allocate(allocator, sizeof(zest_mtl_execution_barriers_backend_t));
	*backend = ZEST__ZERO_INIT(zest_mtl_execution_barriers_backend_t);
	return backend;
}

void *zest__mtl_new_submission_batch_backend(zest_context context) {
	zest_mtl_submission_batch_backend_t *backend = (zest_mtl_submission_batch_backend_t *)zest__linear_allocate(&context->frame_graph_allocator[context->current_fif], sizeof(zest_mtl_submission_batch_backend_t));
	*backend = ZEST__ZERO_INIT(zest_mtl_submission_batch_backend_t);
	return backend;
}
// -- End Backend_setup_functions

// -- Backend_cleanup_functions
void zest__mtl_cleanup_device_backend(zest_device device) {
	zest_mtl_device_backend_t *backend = ZEST_MTL_BACKEND(device, device);
	if (!backend) return;
	//What the teardown raised goes to the log while the device can still take it
	zest__mtl_collect_validation_messages(device);
	ZEST_MTL_RELEASE(backend->residency_set);
	ZEST_MTL_RELEASE(backend->device);
	pthread_mutex_destroy(&backend->residency_lock);
	ZEST__FREE(device->allocator, backend);
	device->backend = NULL;
}

void zest__mtl_cleanup_memory_pool_backend(zest_device_memory_pool memory_allocation) {
	zest_device device = memory_allocation->device;
	zest_buffer_allocator buffer_allocator = memory_allocation->allocator;
	zloc_allocator *allocator = buffer_allocator->context ? buffer_allocator->context->allocator : device->allocator;
	zest_mtl_device_memory_pool_backend_t *backend = ZEST_MTL_BACKEND(device_memory_pool, memory_allocation);
	if (!backend) return;
	if (backend->buffer) {
		zest__mtl_remove_from_residency_set(device, backend->buffer);
		ZEST_MTL_RELEASE(backend->buffer);
		zest__atomic_fetch_add(&device->memory_allocation_count, -1);
	}
	if (backend->heap) {
		zest__mtl_remove_from_residency_set(device, backend->heap);
		ZEST_MTL_RELEASE(backend->heap);
		zest__atomic_fetch_add(&device->memory_allocation_count, -1);
	}
	ZEST__FREE(allocator, backend);
	memory_allocation->backend = NULL;
}

void zest__mtl_cleanup_memory_backend(zest_device_memory memory) {
	zest_device device = memory->device;
	zest_mtl_device_memory_backend_t *backend = ZEST_MTL_BACKEND(device_memory, memory);
	if (!backend) return;
	if (backend->heap) {
		zest__mtl_remove_from_residency_set(device, backend->heap);
		ZEST_MTL_RELEASE(backend->heap);
		zest__atomic_fetch_add(&device->memory_allocation_count, -1);
	}
	ZEST__FREE(device->allocator, backend);
	memory->backend = NULL;
}

void zest__mtl_cleanup_buffer_allocator_backend(zest_buffer_allocator buffer_allocator) {
	zloc_allocator *allocator = buffer_allocator->context ? buffer_allocator->context->allocator : buffer_allocator->device->allocator;
	ZEST__FREE(allocator, buffer_allocator->backend);
	buffer_allocator->backend = NULL;
}

//Shared buffers stay mapped for their whole life, so there is nothing to write back
void zest__mtl_flush_used_buffers(zest_context context, zest_uint frame_in_flight) {
	(void)context;
	(void)frame_in_flight;
}

void zest__mtl_cleanup_queue_backend(zest_device device, zest_queue queue) {
	zest_mtl_queue_backend_t *backend = ZEST_MTL_BACKEND(queue, queue);
	if (!backend) return;
	zest_mtl_device_backend_t *device_backend = ZEST_MTL_BACKEND(device, device);
	if (backend->queue && device_backend && device_backend->residency_set) {
		[backend->queue removeResidencySet:device_backend->residency_set];
	}
	ZEST_MTL_RELEASE(backend->blit_encoder);
	ZEST_MTL_RELEASE(backend->command_buffer);
	ZEST_MTL_RELEASE(backend->fence);
	ZEST_MTL_RELEASE(backend->queue);
	zest_vec_free(device->allocator, backend->pending_staging_buffers);
	ZEST__FREE(device->allocator, backend);
	queue->backend = NULL;
}

//Undoes zest__mtl_create_queues, so no queue object is left holding the device
void zest__mtl_cleanup_queues(zest_device device) {
	zest_vec_foreach(family_index, device->queue_families) {
		zest_queue_manager manager = device->queue_families[family_index];
		if (!manager) continue;
		zest_vec_foreach(queue_index, manager->queues) {
			zest_queue queue = &manager->queues[queue_index];
			zest__mtl_cleanup_execution_timeline_backend(&queue->timeline);
			zest__mtl_cleanup_queue_backend(device, queue);
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
void zest__mtl_cleanup_context_backend(zest_context context) {
	zest_mtl_context_backend_t *backend = ZEST_MTL_BACKEND(context, context);
	if (!backend) return;
	ZEST_MTL_RELEASE(backend->render_pass_descriptor);
	ZEST__FREE(context->allocator, backend);
	context->backend = NULL;
}

//Shared code frees the swapchain on every resize. The layer isn't the swapchain's, so it stays for the next one.
void zest__mtl_cleanup_swapchain_backend(zest_swapchain swapchain) {
	zest_context context = swapchain->context;
	ZEST_MTL_RELEASE(ZEST_MTL_BACKEND(context, context)->drawable);
	zest_vec_foreach(index, swapchain->images) {
		if (!swapchain->images[index].backend) continue;
		ZEST__FREE(context->allocator, swapchain->images[index].backend);
		swapchain->images[index].backend = NULL;
	}
	//Swapchain views have no handle store, so the shared view cleanup can't free them
	zest_vec_foreach(index, swapchain->views) {
		if (!swapchain->views[index]) continue;
		ZEST__FREE(context->allocator, swapchain->views[index]);
		swapchain->views[index] = NULL;
	}
	if (swapchain->backend) {
		ZEST__FREE(context->allocator, swapchain->backend);
		swapchain->backend = NULL;
	}
}

void zest__mtl_destroy_context_surface(zest_context context) {
	zest_mtl_context_backend_t *backend = ZEST_MTL_BACKEND(context, context);
#if defined(SDL_MAJOR_VERSION)
	//The view stays with its window for the next context
	backend->layer = nil;
#else
	ZEST_MTL_RELEASE(backend->layer);
#endif
}

//A cached graph's queue never gets the command pool reset, so its committed lists are emptied here
void zest__mtl_cleanup_context_queue_backend(zest_context context, zest_context_queue context_queue) {
	zest_mtl_context_queue_backend_t *backend = ZEST_MTL_BACKEND(context_queue, context_queue);
	if (!backend) return;
	zest__mtl_reap_command_buffers(context, context_queue, ZEST_TRUE);
	zest_ForEachFrameInFlight(frame_in_flight) {
		zest_vec_free(context->allocator, backend->committed[frame_in_flight]);
	}
	ZEST_MTL_RELEASE(backend->command_buffer);
	ZEST__FREE(context->allocator, backend);
	context_queue->backend = NULL;
}

//Shared code frees the semaphores struct itself
void zest__mtl_cleanup_frame_graph_semaphore(zest_context context, zest_frame_graph_semaphores semaphores) {
	zest_mtl_frame_graph_semaphores_backend_t *backend = ZEST_MTL_BACKEND(frame_graph_semaphores, semaphores);
	if (!backend) return;
	zest_ForEachFrameInFlight(frame_in_flight) {
		for (zest_uint queue_index = 0; queue_index != ZEST_QUEUE_COUNT; ++queue_index) {
			ZEST_MTL_RELEASE(backend->events[frame_in_flight][queue_index]);
		}
	}
	ZEST__FREE(context->allocator, backend);
	semaphores->backend = NULL;
}
// -- End Backend_cleanup_functions

// -- Shared_events
zest_bool zest__mtl_create_execution_timeline_backend(zest_device device, zest_execution_timeline timeline) {
	zest_mtl_device_backend_t *device_backend = ZEST_MTL_BACKEND(device, device);
	zest_mtl_execution_timeline_backend_t *backend = (zest_mtl_execution_timeline_backend_t *)ZEST__ALLOCATE(device->allocator, sizeof(zest_mtl_execution_timeline_backend_t));
	*backend = ZEST__ZERO_INIT(zest_mtl_execution_timeline_backend_t);
	timeline->backend = (zest_execution_timeline_backend)backend;
	backend->event = [device_backend->device newSharedEvent];
	if (!backend->event) {
		ZEST_APPEND_LOG(device->log_path.str, "A Metal shared event couldn't be created for a timeline.");
		return ZEST_FALSE;
	}
	return ZEST_TRUE;
}

void zest__mtl_cleanup_execution_timeline_backend(zest_execution_timeline timeline) {
	zest_mtl_execution_timeline_backend_t *backend = ZEST_MTL_BACKEND(execution_timeline, timeline);
	if (!backend) return;
	ZEST_MTL_RELEASE(backend->event);
	ZEST__FREE(timeline->device->allocator, backend);
	timeline->backend = NULL;
}

zest_semaphore_status zest__mtl_wait_for_timeline(zest_execution_timeline timeline, zest_microsecs timeout) {
	zest_mtl_execution_timeline_backend_t *backend = ZEST_MTL_BACKEND(execution_timeline, timeline);
	zest_u64 target_value = timeline->current_value;
	if ([backend->event signaledValue] >= target_value) return zest_semaphore_status_success;
	if (timeout == 0) return zest_semaphore_status_timeout;
	zest_microsecs wait_start = zest_Microsecs();
	for (;;) {
		//The event's own wait takes milliseconds, so an endless wait is a run of long ones
		zest_u64 milliseconds = 60000;
		if (timeout != ZEST__MTL_WAIT_FOREVER) {
			zest_microsecs elapsed = zest_Microsecs() - wait_start;
			if (elapsed >= timeout) return zest_semaphore_status_timeout;
			milliseconds = (timeout - elapsed + 999) / 1000;
		}
		if ([backend->event waitUntilSignaledValue:target_value timeoutMS:milliseconds]) return zest_semaphore_status_success;
	}
}
zest_semaphore_status zest__mtl_wait_for_renderer_semaphore(zest_context context) {
	zest_execution_timeline timeline = context->frame_sync_timeline[context->current_fif];
	zest_microsecs timeout = (zest_microsecs)((context->fence_wait_timeout_ns + 999) / 1000);
	return zest__mtl_wait_for_timeline(timeline, timeout);
}

void zest__mtl_wait_for_fif_semaphore(zest_context context, zest_uint fif) {
	zest_execution_timeline timeline = context->frame_sync_timeline[fif];
	if (!timeline) return;
	zest__mtl_wait_for_timeline(timeline, ZEST__MTL_WAIT_FOREVER);
}
// -- End Shared_events

// -- Command_pools
//Staging buffers are freed before the queue is released, as the next holder pushes to the same list
void zest__mtl_release_immediate_queue(zest_queue queue, zest_bool gpu_finished) {
	zest_mtl_queue_backend_t *backend = ZEST_MTL_BACKEND(queue, queue);
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

//Reports a command buffer that failed and takes in what shader validation logged against it. Call it once the buffer has finished.
zest_bool zest__mtl_finish_command_buffer(zest_device device, id<MTLCommandBuffer> command_buffer) {
	zest_bool completed = [command_buffer status] == MTLCommandBufferStatusCompleted;
	if (!completed) {
		NSError *error = [command_buffer error];
		const char *description = error ? [[error localizedDescription] UTF8String] : "no error given";
		ZEST_APPEND_LOG(device->log_path.str, "A Metal command buffer failed: %s", description);
		ZEST_REPORT(device, zest_report_submission_failure, "A Metal command buffer failed: %s", description);
	}
	if (ZEST_MTL_BACKEND(device, device)->shader_validation) {
		for (id<MTLFunctionLog> function_log in [command_buffer logs]) {
			zest__mtl_record_validation_message(device, [[function_log description] UTF8String], ZEST_TRUE);
		}
	}
	return completed;
}

zest_queue zest__mtl_imm_begin_command_buffer(zest_device device, zest_device_queue_type target_queue) {
	zest_queue queue = zest__acquire_immediate_queue(device, target_queue);
	if (!queue) {
		return NULL;
	}
	zest_mtl_queue_backend_t *backend = ZEST_MTL_BACKEND(queue, queue);
	@autoreleasepool {
		backend->command_buffer = [[backend->queue commandBuffer] retain];
		[backend->command_buffer setLabel:@"Zest Immediate Command Buffer"];
	}
	if (!backend->command_buffer) {
		ZEST_REPORT(device, zest_report_submission_failure, "Metal couldn't supply a command buffer for immediate commands.");
		zest__mtl_release_immediate_queue(queue, ZEST_TRUE);
		return NULL;
	}
	return queue;
}

zest_bool zest__mtl_imm_end_command_buffer(zest_queue queue) {
	if (!queue) return ZEST_FALSE;
	zest_device device = queue->device;
	zest_mtl_queue_backend_t *backend = ZEST_MTL_BACKEND(queue, queue);
	zest_mtl_execution_timeline_backend_t *timeline_backend = ZEST_MTL_BACKEND(execution_timeline, &queue->timeline);
	zest_bool completed = ZEST_FALSE;
	zest__mtl_end_queue_encoder(queue);
	queue->timeline.current_value++;
	@autoreleasepool {
		[backend->command_buffer encodeSignalEvent:timeline_backend->event value:queue->timeline.current_value];
		[backend->command_buffer commit];
		[backend->command_buffer waitUntilCompleted];
		completed = zest__mtl_finish_command_buffer(device, backend->command_buffer);
	}
	//A command buffer that failed never reaches its signal, which would leave the timeline behind for good
	if ([timeline_backend->event signaledValue] < queue->timeline.current_value) {
		[timeline_backend->event setSignaledValue:queue->timeline.current_value];
	}
	ZEST_MTL_RELEASE(backend->command_buffer);
	zest__mtl_release_immediate_queue(queue, ZEST_TRUE);
	return completed;
}
//Every encoder on a queue waits on the queue's fence when it opens and updates it when it ends, which keeps untracked passes in order
id<MTLBlitCommandEncoder> zest__mtl_queue_blit_encoder(zest_queue queue) {
	zest_mtl_queue_backend_t *backend = ZEST_MTL_BACKEND(queue, queue);
	if (!backend->blit_encoder) {
		@autoreleasepool {
			backend->blit_encoder = [[backend->command_buffer blitCommandEncoder] retain];
		}
		[backend->blit_encoder waitForFence:backend->fence];
	}
	return backend->blit_encoder;
}

void zest__mtl_end_queue_encoder(zest_queue queue) {
	zest_mtl_queue_backend_t *backend = ZEST_MTL_BACKEND(queue, queue);
	if (backend->blit_encoder) {
		[backend->blit_encoder updateFence:backend->fence];
		[backend->blit_encoder endEncoding];
		ZEST_MTL_RELEASE(backend->blit_encoder);
	}
}

zest_bool zest__mtl_cmd_copy_buffer_one_time(zest_queue queue, zest_buffer src_buffer, zest_buffer dst_buffer, zest_size size, zest_size src_offset, zest_size dst_offset) {
	id<MTLBlitCommandEncoder> encoder = zest__mtl_queue_blit_encoder(queue);
	[encoder copyFromBuffer:zest__mtl_buffer_object(src_buffer) sourceOffset:(NSUInteger)(src_buffer->memory_offset + src_offset) toBuffer:zest__mtl_buffer_object(dst_buffer) destinationOffset:(NSUInteger)(dst_buffer->memory_offset + dst_offset) size:(NSUInteger)size];
	return ZEST_TRUE;
}
//Reads and releases the queue's batches that have finished. With release_all, one still running is released unread.
void zest__mtl_reap_command_buffers(zest_context context, zest_context_queue queue, zest_bool release_all) {
	zest_mtl_context_queue_backend_t *backend = ZEST_MTL_BACKEND(context_queue, queue);
	zest_ForEachFrameInFlight(frame_in_flight) {
		id<MTLCommandBuffer> *committed = backend->committed[frame_in_flight];
		zest_uint committed_count = zest_vec_size(committed);
		zest_uint kept_count = 0;
		for (zest_uint buffer_index = 0; buffer_index != committed_count; ++buffer_index) {
			id<MTLCommandBuffer> command_buffer = committed[buffer_index];
			if ([command_buffer status] >= MTLCommandBufferStatusCompleted) {
				@autoreleasepool {
					zest__mtl_finish_command_buffer(context->device, command_buffer);
				}
			} else if (!release_all) {
				committed[kept_count++] = command_buffer;
				continue;
			}
			[command_buffer release];
		}
		if (kept_count != committed_count) {
			zest_vec_trim(committed, committed_count - kept_count);
		}
	}
}

void zest__mtl_reap_device_command_buffers(zest_device device) {
	zest_vec_foreach(context_index, device->contexts) {
		zest_context context = device->contexts[context_index];
		for (zest_uint queue_index = 0; queue_index != ZEST_QUEUE_COUNT; ++queue_index) {
			if (context->queues[queue_index]) {
				zest__mtl_reap_command_buffers(context, context->queues[queue_index], ZEST_FALSE);
			}
			zest_map_foreach(graph_index, context->cached_frame_graph_queues[queue_index]) {
				zest__mtl_reap_command_buffers(context, context->cached_frame_graph_queues[queue_index].data[graph_index], ZEST_FALSE);
			}
		}
	}
}

//Each batch takes a new command buffer from its device queue, so all a context queue keeps is the batches it has committed
void zest__mtl_reset_queue_command_pool(zest_context context, zest_context_queue queue, zest_bool release_resources) {
	zest_mtl_context_queue_backend_t *backend = ZEST_MTL_BACKEND(context_queue, queue);
	zest__mtl_reap_command_buffers(context, queue, release_resources);
	if (!release_resources) return;
	zest_ForEachFrameInFlight(frame_in_flight) {
		zest_vec_free(context->allocator, backend->committed[frame_in_flight]);
	}
}

//A queue runs its command buffers in order, so an empty one that has finished means everything before it has too
void zest__mtl_queue_wait_idle(zest_context context, zest_context_queue queue) {
	(void)context;
	if (!queue->queue) return;
	@autoreleasepool {
		id<MTLCommandBuffer> command_buffer = [ZEST_MTL_BACKEND(queue, queue->queue)->queue commandBuffer];
		[command_buffer commit];
		[command_buffer waitUntilCompleted];
	}
}
// -- End Command_pools

// -- Buffer_and_memory
//One commit per change, so the next command buffer committed on any queue sees the new set
void zest__mtl_add_to_residency_set(zest_device device, id<MTLAllocation> allocation) {
	zest_mtl_device_backend_t *backend = ZEST_MTL_BACKEND(device, device);
	pthread_mutex_lock(&backend->residency_lock);
	[backend->residency_set addAllocation:allocation];
	[backend->residency_set commit];
	pthread_mutex_unlock(&backend->residency_lock);
}

void zest__mtl_remove_from_residency_set(zest_device device, id<MTLAllocation> allocation) {
	zest_mtl_device_backend_t *backend = ZEST_MTL_BACKEND(device, device);
	pthread_mutex_lock(&backend->residency_lock);
	[backend->residency_set removeAllocation:allocation];
	[backend->residency_set commit];
	pthread_mutex_unlock(&backend->residency_lock);
}

//Every pool is untracked: Zest's own barriers and the queue fences order what touches it
MTLResourceOptions zest__mtl_buffer_options(zest_memory_property_flags property_flags) {
	if (ZEST__NOT_FLAGGED(property_flags, zest_memory_property_host_visible_bit)) {
		return MTLResourceStorageModePrivate | MTLResourceHazardTrackingModeUntracked;
	}
	MTLResourceOptions options = MTLResourceStorageModeShared | MTLResourceHazardTrackingModeUntracked;
	//Memory the CPU only writes skips the cache, unless it was asked for cached or the GPU works in it too
	if (ZEST__NOT_FLAGGED(property_flags, zest_memory_property_host_cached_bit) && ZEST__NOT_FLAGGED(property_flags, zest_memory_property_device_local_bit)) {
		options |= MTLResourceCPUCacheModeWriteCombined;
	}
	return options;
}

void *zest__mtl_create_buffer_allocator_backend(zest_device device, zest_context context, zest_size size, zest_buffer_info_t *buffer_info, zest_uint backend_memory_bits) {
	(void)size;
	(void)backend_memory_bits;
	zloc_allocator *allocator = context ? context->allocator : device->allocator;
	zest_mtl_buffer_allocator_backend_t *backend = (zest_mtl_buffer_allocator_backend_t *)ZEST__ALLOCATE(allocator, sizeof(zest_mtl_buffer_allocator_backend_t));
	*backend = ZEST__ZERO_INIT(zest_mtl_buffer_allocator_backend_t);
	backend->buffer_options = zest__mtl_buffer_options(buffer_info->property_flags);
	return backend;
}

zest_bool zest__mtl_add_buffer_memory_pool(zest_device device, zest_context context, zest_size size, zest_buffer_allocator buffer_allocator, zest_device_memory_pool memory_pool) {
	(void)context;
	zest_mtl_buffer_allocator_backend_t *allocator_backend = ZEST_MTL_BACKEND(buffer_allocator, buffer_allocator);
	zest_mtl_device_memory_pool_backend_t *pool_backend = ZEST_MTL_BACKEND(device_memory_pool, memory_pool);
	ZEST_APPEND_LOG(device->log_path.str, "Allocating buffer memory pool, size: %llu, resource options: 0x%llx", (zest_ull)size, (zest_ull)allocator_backend->buffer_options);
	pool_backend->buffer = [ZEST_MTL_BACKEND(device, device)->device newBufferWithLength:(NSUInteger)size options:allocator_backend->buffer_options];
	if (!pool_backend->buffer) {
		ZEST_APPEND_LOG(device->log_path.str, "Metal couldn't allocate a buffer of %llu bytes for a memory pool.", (zest_ull)size);
		return ZEST_FALSE;
	}
	zest__mtl_add_to_residency_set(device, pool_backend->buffer);
	zest__atomic_fetch_add(&device->memory_allocation_count, 1);
	memory_pool->size = size;
	memory_pool->alignment = ZEST_BUFFER_OFFSET_GRANULARITY;
	return ZEST_TRUE;
}

//A private placement heap that textures are placed in at offsets Zest chooses
zest_bool zest__mtl_create_image_heap(zest_device device, zest_size size, id<MTLHeap> *out_heap) {
	MTLHeapDescriptor *descriptor = [[MTLHeapDescriptor alloc] init];
	[descriptor setType:MTLHeapTypePlacement];
	[descriptor setStorageMode:MTLStorageModePrivate];
	[descriptor setHazardTrackingMode:MTLHazardTrackingModeUntracked];
	[descriptor setSize:(NSUInteger)size];
	id<MTLHeap> heap = [ZEST_MTL_BACKEND(device, device)->device newHeapWithDescriptor:descriptor];
	[descriptor release];
	if (!heap) {
		ZEST_APPEND_LOG(device->log_path.str, "Metal couldn't allocate a heap of %llu bytes for images.", (zest_ull)size);
		return ZEST_FALSE;
	}
	zest__mtl_add_to_residency_set(device, heap);
	zest__atomic_fetch_add(&device->memory_allocation_count, 1);
	*out_heap = heap;
	return ZEST_TRUE;
}

zest_bool zest__mtl_create_image_memory_pool(zest_device device, zest_context context, zest_size size_in_bytes, zest_buffer_allocator buffer_allocator, zest_device_memory_pool pool) {
	(void)context;
	pool->size = size_in_bytes;
	pool->alignment = buffer_allocator->buffer_info.alignment;
	pool->minimum_allocation_size = ZEST__MAX(pool->alignment, pool->minimum_allocation_size);
	ZEST_APPEND_LOG(device->log_path.str, "Allocating image memory pool, size: %llu, alignment: %llu", (zest_ull)size_in_bytes, (zest_ull)pool->alignment);
	return zest__mtl_create_image_heap(device, size_in_bytes, &ZEST_MTL_BACKEND(device_memory_pool, pool)->heap);
}

//Dedicated image memory is a heap of its own with the image at offset 0, so both kinds of image are placed the same way
zest_bool zest__mtl_create_device_memory(zest_device device, zest_size size_in_bytes, zest_buffer_info_t *buffer_info, zest_uint backend_memory_bits, zest_device_memory memory) {
	(void)buffer_info;
	(void)backend_memory_bits;
	memory->size = size_in_bytes;
	return zest__mtl_create_image_heap(device, size_in_bytes, &ZEST_MTL_BACKEND(device_memory, memory)->heap);
}

//Apple silicon has one pool of memory, so there is one heap to report
void zest__mtl_get_memory_budget(zest_device device, zest_memory_budget_t *budget) {
	id<MTLDevice> native_device = ZEST_MTL_BACKEND(device, device)->device;
	budget->supported = ZEST_TRUE;
	budget->heap_count = 1;
	budget->heaps[0].heap_size = (zest_size)[native_device recommendedMaxWorkingSetSize];
	budget->heaps[0].budget = (zest_size)[native_device recommendedMaxWorkingSetSize];
	budget->heaps[0].usage = (zest_size)[native_device currentAllocatedSize];
	budget->heaps[0].device_local = ZEST_TRUE;
	budget->device_local_size = budget->heaps[0].heap_size;
	budget->device_local_budget = budget->heaps[0].budget;
	budget->device_local_usage = budget->heaps[0].usage;
}

zest_bool zest__mtl_map_memory(zest_device_memory_pool memory_allocation, zest_size size, zest_size offset) {
	(void)size;
	id<MTLBuffer> buffer = ZEST_MTL_BACKEND(device_memory_pool, memory_allocation)->buffer;
	//Image pools are heaps and device local pools are private, and neither has a CPU mapping
	if (!buffer || [buffer storageMode] != MTLStorageModeShared) return ZEST_FALSE;
	memory_allocation->mapped = (char *)[buffer contents] + offset;
	return ZEST_TRUE;
}

void zest__mtl_unmap_memory(zest_device_memory_pool memory_allocation) {
	(void)memory_allocation;
}

id<MTLBuffer> zest__mtl_buffer_object(zest_buffer buffer) {
	return ZEST_MTL_BACKEND(device_memory_pool, buffer->memory_pool)->buffer;
}

//What a bindless entry holds for a buffer
zest_u64 zest__mtl_buffer_address(zest_buffer buffer) {
	return (zest_u64)[zest__mtl_buffer_object(buffer) gpuAddress] + buffer->memory_offset;
}
// -- End Buffer_and_memory

// -- Bindless_table
void zest__mtl_compute_bindless_regions(const zest_uint *binding_counts, zest_mtl_bindless_regions_t *regions) {
	*regions = ZEST__ZERO_INIT(zest_mtl_bindless_regions_t);
	for (zest_uint binding_number = 0; binding_number != zest_max_global_binding_number; ++binding_number) {
		regions->offsets[binding_number] = regions->table_size;
		regions->counts[binding_number] = binding_counts[binding_number];
		regions->table_size += (zest_size)binding_counts[binding_number] * ZEST__MTL_BINDLESS_ENTRY_SIZE;
	}
}

void *zest__mtl_new_set_layout_backend(zloc_allocator *allocator) {
	zest_mtl_set_layout_backend_t *backend = (zest_mtl_set_layout_backend_t *)ZEST__ALLOCATE(allocator, sizeof(zest_mtl_set_layout_backend_t));
	*backend = ZEST__ZERO_INIT(zest_mtl_set_layout_backend_t);
	return backend;
}

zest_bool zest__mtl_create_set_layout(zest_device device, zest_context context, zest_set_layout_builder_t *builder, zest_set_layout layout, zest_bool is_bindless) {
	(void)context;
	if (!is_bindless) {
		return zest__mtl_report_unavailable(device, "Custom descriptor set layouts are not available on the Metal backend. Put the resources in the global bindless set.");
	}
	zest_mtl_set_layout_backend_t *backend = ZEST_MTL_BACKEND(set_layout, layout);
	zest_vec_foreach(binding_index, builder->bindings) {
		const zest_descriptor_binding_desc_t *description = &builder->bindings[binding_index];
		if (description->binding >= zest_max_global_binding_number) {
			return zest__mtl_report_unavailable(device, "A Metal bindless set layout can only use the global binding numbers.");
		}
		backend->binding_counts[description->binding] = description->count;
	}
	return ZEST_TRUE;
}

void *zest__mtl_new_descriptor_pool_backend(zloc_allocator *allocator) {
	zest_mtl_descriptor_pool_backend_t *backend = (zest_mtl_descriptor_pool_backend_t *)ZEST__ALLOCATE(allocator, sizeof(zest_mtl_descriptor_pool_backend_t));
	*backend = ZEST__ZERO_INIT(zest_mtl_descriptor_pool_backend_t);
	return backend;
}

zest_bool zest__mtl_create_set_pool(zest_device device, zest_context context, zest_descriptor_pool pool, zest_set_layout layout, zest_uint max_set_count, zest_bool bindless) {
	(void)max_set_count;
	zloc_allocator *allocator = context ? context->allocator : device->allocator;
	zest_mtl_descriptor_pool_backend_t *backend = ZEST_MTL_BACKEND(descriptor_pool, pool);
	if (!bindless) {
		ZEST__FREE(allocator, backend);
		pool->backend = NULL;
		return zest__mtl_report_unavailable(device, "Custom descriptor set layouts are not available on the Metal backend. Put the resources in the global bindless set.");
	}
	zest__mtl_compute_bindless_regions(ZEST_MTL_BACKEND(set_layout, layout)->binding_counts, &backend->regions);
	//A buffer can't be empty
	zest_size table_size = ZEST__MAX(backend->regions.table_size, ZEST__MTL_BINDLESS_ENTRY_SIZE);
	backend->table = [ZEST_MTL_BACKEND(device, device)->device newBufferWithLength:(NSUInteger)table_size options:MTLResourceStorageModeShared | MTLResourceHazardTrackingModeUntracked];
	if (!backend->table) {
		ZEST_APPEND_LOG(device->log_path.str, "Metal couldn't allocate the %llu byte bindless table.", (zest_ull)table_size);
		//The caller frees the pool itself
		ZEST__FREE(allocator, backend);
		pool->backend = NULL;
		return ZEST_FALSE;
	}
	memset([backend->table contents], 0, (size_t)table_size);
	[backend->table setLabel:@"Zest Bindless Table"];
	zest__mtl_add_to_residency_set(device, backend->table);
	ZEST_APPEND_LOG(device->log_path.str, "Bindless table: %llu bytes", (zest_ull)table_size);
	layout->pool = pool;
	return ZEST_TRUE;
}

zest_descriptor_set zest__mtl_create_bindless_set(zest_set_layout layout) {
	ZEST_ASSERT(layout->pool && layout->pool->backend, "The bindless set layout has no descriptor pool.");
	zest_device device = layout->device;
	zest_descriptor_set set = (zest_descriptor_set)ZEST__NEW(device->allocator, zest_descriptor_set);
	*set = ZEST__ZERO_INIT(zest_descriptor_set_t);
	zest_mtl_descriptor_set_backend_t *backend = (zest_mtl_descriptor_set_backend_t *)ZEST__ALLOCATE(device->allocator, sizeof(zest_mtl_descriptor_set_backend_t));
	*backend = ZEST__ZERO_INIT(zest_mtl_descriptor_set_backend_t);
	backend->pool = ZEST_MTL_BACKEND(descriptor_pool, layout->pool);
	set->backend = (zest_descriptor_set_backend)backend;
	set->magic = zest_INIT_MAGIC(zest_struct_type_descriptor_set);
	return set;
}

//The address of one entry, or NULL with a report for an index outside the slot's region
zest_u64 *zest__mtl_bindless_entry(zest_device device, zest_descriptor_set set, zest_uint binding_number, zest_uint array_index) {
	zest_mtl_descriptor_pool_backend_t *pool_backend = ZEST_MTL_BACKEND(descriptor_set, set)->pool;
	if (binding_number >= zest_max_global_binding_number || array_index >= pool_backend->regions.counts[binding_number]) {
		zest__mtl_report_unavailable(device, "A bindless descriptor write is outside its binding's region of the Metal bindless table. Raise the binding's count on the device builder.");
		return NULL;
	}
	return (zest_u64 *)((char *)[pool_backend->table contents] + pool_backend->regions.offsets[binding_number]) + array_index;
}

void zest__mtl_update_bindless_image_descriptor(zest_device device, zest_uint binding_number, zest_uint array_index, zest_descriptor_type type, zest_image image, zest_image_view view, zest_sampler sampler, zest_descriptor_set set) {
	(void)image;
	zest_u64 *entry = zest__mtl_bindless_entry(device, set, binding_number, array_index);
	if (!entry) return;
	switch (type) {
		case zest_descriptor_type_sampler: {
			ZEST_ASSERT(sampler, "A sampler descriptor write needs a sampler.");
			*entry = [ZEST_MTL_BACKEND(sampler, sampler)->sampler gpuResourceID]._impl;
			break;
		}
		case zest_descriptor_type_sampled_image:
		case zest_descriptor_type_storage_image: {
			id<MTLTexture> texture = ZEST_MTL_BACKEND(image_view, view)->texture;
			zest_bool usable = type == zest_descriptor_type_sampled_image ? ([texture usage] & MTLTextureUsageShaderRead) != 0 : ([texture usage] & MTLTextureUsageShaderWrite) != 0;
			if (!usable) {
				zest__mtl_report_unavailable(device, type == zest_descriptor_type_sampled_image ? "The view's image wasn't created with zest_image_flag_sampled, so it can't go in a sampled image binding." : "The view's image wasn't created with zest_image_flag_storage, so it can't go in a storage image binding.");
				return;
			}
			*entry = [texture gpuResourceID]._impl;
			break;
		}
		default:
			ZEST_ASSERT(0, "This descriptor type can't be written to the Metal bindless image bindings.");
			break;
	}
}

//MSL reads a buffer entry as a typed pointer, so a structured view and a raw view are the same entry and the stride isn't needed
void zest__mtl_update_bindless_storage_buffer_descriptor(zest_device device, zest_uint binding_number, zest_uint array_index, zest_buffer buffer, zest_uint stride, zest_descriptor_set set) {
	(void)stride;
	zest_u64 *entry = zest__mtl_bindless_entry(device, set, binding_number, array_index);
	if (!entry) return;
	*entry = zest__mtl_buffer_address(buffer);
}

void zest__mtl_update_bindless_uniform_buffer_descriptor(zest_device device, zest_uint binding_number, zest_uint array_index, zest_buffer buffer, zest_descriptor_set set) {
	zest_u64 *entry = zest__mtl_bindless_entry(device, set, binding_number, array_index);
	if (!entry) return;
	*entry = zest__mtl_buffer_address(buffer);
}

void zest__mtl_cleanup_set_layout_backend(zest_set_layout layout) {
	zloc_allocator *allocator = layout->context ? layout->context->allocator : layout->device->allocator;
	if (layout->backend) {
		ZEST__FREE(allocator, layout->backend);
		layout->backend = NULL;
	}
	//Pools have no cleanup callback of their own, so the layout releases its pool's table
	if (layout->pool && layout->pool->backend) {
		zest_mtl_descriptor_pool_backend_t *pool_backend = ZEST_MTL_BACKEND(descriptor_pool, layout->pool);
		if (pool_backend->table) {
			zest__mtl_remove_from_residency_set(layout->device, pool_backend->table);
			ZEST_MTL_RELEASE(pool_backend->table);
		}
		ZEST__FREE(allocator, pool_backend);
		layout->pool->backend = NULL;
	}
}

void *zest__mtl_new_pipeline_layout_backend(zest_device device) {
	zest_mtl_pipeline_layout_backend_t *backend = (zest_mtl_pipeline_layout_backend_t *)ZEST__ALLOCATE(device->allocator, sizeof(zest_mtl_pipeline_layout_backend_t));
	*backend = ZEST__ZERO_INIT(zest_mtl_pipeline_layout_backend_t);
	return backend;
}

//Each pipeline's bindings come from its own converted shaders, so the layout only carries the push constant size
zest_bool zest__mtl_build_pipeline_layout(zest_device device, zest_pipeline_layout layout, zest_pipeline_layout_info_t *info) {
	(void)device;
	ZEST_MTL_BACKEND(pipeline_layout, layout)->push_constant_size = info->push_constant_range.size;
	return ZEST_TRUE;
}

void zest__mtl_cleanup_pipeline_layout_backend(zest_pipeline_layout layout) {
	if (!layout->backend) return;
	ZEST__FREE(layout->device->allocator, layout->backend);
	layout->backend = NULL;
}
// -- End Bindless_table

// -- General_renderer
// -- End General_renderer

// -- Device_OS
//A queue runs its command buffers in order, so an empty one that has finished means everything before it has too
void zest__mtl_wait_for_idle_device(zest_device device) {
	zest_vec_foreach(family_index, device->queue_families) {
		zest_queue_manager manager = device->queue_families[family_index];
		if (!manager) continue;
		zest_vec_foreach(queue_index, manager->queues) {
			zest_queue queue = &manager->queues[queue_index];
			zest_mtl_queue_backend_t *queue_backend = ZEST_MTL_BACKEND(queue, queue);
			zest_mtl_execution_timeline_backend_t *timeline_backend = ZEST_MTL_BACKEND(execution_timeline, &queue->timeline);
			if (!queue_backend || !timeline_backend) continue;
			@autoreleasepool {
				id<MTLCommandBuffer> command_buffer = [queue_backend->queue commandBuffer];
				queue->timeline.current_value++;
				[command_buffer encodeSignalEvent:timeline_backend->event value:queue->timeline.current_value];
				[command_buffer commit];
				[command_buffer waitUntilCompleted];
			}
		}
	}
}
// -- End Device_OS

// -- Pipelines
// -- End Pipelines

// -- Images
zest_bool zest__mtl_is_image_format_supported(zest_device device, zest_format format, zest_image_flags flags) {
	//Textures live in private heaps
	if (ZEST__FLAGGED(flags, zest_image_flag_host_visible)) {
		return ZEST_FALSE;
	}
	zest_capability_flags required_capability = zest__compressed_format_capability(format);
	if (required_capability && !ZEST__FLAGGED(device->capabilities.enabled, required_capability)) {
		return ZEST_FALSE;
	}
	MTLPixelFormat pixel_format = zest__mtl_pixel_format(format);
	if (!zest__mtl_device_has_pixel_format(&ZEST_MTL_BACKEND(device, device)->support, pixel_format)) {
		return ZEST_FALSE;
	}
	zest_bool is_depth = zest__is_depth_stencil_format(format);
	zest_bool is_compressed = zest__is_compressed_format(format);
	zest_bool is_packed16 = pixel_format == MTLPixelFormatABGR4Unorm || pixel_format == MTLPixelFormatB5G6R5Unorm || pixel_format == MTLPixelFormatBGR5A1Unorm;
	zest_bool is_srgb = pixel_format == MTLPixelFormatR8Unorm_sRGB || pixel_format == MTLPixelFormatRG8Unorm_sRGB || pixel_format == MTLPixelFormatRGBA8Unorm_sRGB || pixel_format == MTLPixelFormatBGRA8Unorm_sRGB;
	if (ZEST__FLAGGED(flags, zest_image_flag_color_attachment) && (is_depth || is_compressed)) return ZEST_FALSE;
	if (ZEST__FLAGGED(flags, zest_image_flag_depth_stencil_attachment) && !is_depth) return ZEST_FALSE;
	//A shader can't write the formats Metal stores in another colour space or packs below a byte per channel
	if (ZEST__FLAGGED(flags, zest_image_flag_storage) && (is_depth || is_compressed || is_packed16 || is_srgb)) return ZEST_FALSE;
	return ZEST_TRUE;
}

void zest__mtl_set_depth_format(zest_device device) {
	device->depth_format = zest_format_d32_sfloat;
}

zest_bool zest__mtl_swizzle_is_identity(zest_component_mapping_t swizzle) {
	return (swizzle.r == zest_component_swizzle_identity || swizzle.r == zest_component_swizzle_r) &&
		(swizzle.g == zest_component_swizzle_identity || swizzle.g == zest_component_swizzle_g) &&
		(swizzle.b == zest_component_swizzle_identity || swizzle.b == zest_component_swizzle_b) &&
		(swizzle.a == zest_component_swizzle_identity || swizzle.a == zest_component_swizzle_a);
}

//Vulkan's BC1 RGB formats have no alpha, so anything that would read the shared BC1 layout's alpha reads one instead
zest_component_mapping_t zest__mtl_image_swizzle(zest_image image) {
	zest_component_mapping_t swizzle = image->info.swizzle;
	if (image->info.format == zest_format_bc1_rgb_unorm_block || image->info.format == zest_format_bc1_rgb_srgb_block) {
		if (swizzle.r == zest_component_swizzle_a) swizzle.r = zest_component_swizzle_one;
		if (swizzle.g == zest_component_swizzle_a) swizzle.g = zest_component_swizzle_one;
		if (swizzle.b == zest_component_swizzle_a) swizzle.b = zest_component_swizzle_one;
		if (swizzle.a == zest_component_swizzle_a || swizzle.a == zest_component_swizzle_identity) swizzle.a = zest_component_swizzle_one;
	}
	return swizzle;
}

void *zest__mtl_new_image_backend(zest_device device) {
	zest_mtl_image_backend_t *backend = (zest_mtl_image_backend_t *)ZEST__ALLOCATE(device->allocator, sizeof(zest_mtl_image_backend_t));
	*backend = ZEST__ZERO_INIT(zest_mtl_image_backend_t);
	return backend;
}

//Returns a descriptor the caller releases, or nil for a format with no Metal form
MTLTextureDescriptor *zest__mtl_new_texture_descriptor(zest_device device, zest_image image, zest_uint layer_count, zest_uint sample_count, zest_image_flags flags) {
	MTLPixelFormat pixel_format = zest__mtl_pixel_format(image->info.format);
	if (!zest__mtl_device_has_pixel_format(&ZEST_MTL_BACKEND(device, device)->support, pixel_format)) {
		return nil;
	}
	MTLTextureType texture_type = zest__mtl_image_texture_type(image, layer_count, sample_count, flags);
	zest_bool is_cube = texture_type == MTLTextureTypeCube || texture_type == MTLTextureTypeCubeArray;
	zest_bool is_array = texture_type == MTLTextureType2DArray || texture_type == MTLTextureType2DMultisampleArray || texture_type == MTLTextureTypeCubeArray;
	MTLTextureUsage usage = MTLTextureUsageUnknown;
	if (ZEST__FLAGGED(flags, zest_image_flag_sampled)) usage |= MTLTextureUsageShaderRead;
	if (ZEST__FLAGGED(flags, zest_image_flag_storage)) usage |= MTLTextureUsageShaderRead | MTLTextureUsageShaderWrite;
	if (ZEST__FLAGGED(flags, zest_image_flag_color_attachment) || ZEST__FLAGGED(flags, zest_image_flag_depth_stencil_attachment)) usage |= MTLTextureUsageRenderTarget;
	//Only a view that changes the format or reorders the channels needs this, and it costs the texture its lossless compression
	if (ZEST__FLAGGED(flags, zest_image_flag_mutable_format) || !zest__mtl_swizzle_is_identity(zest__mtl_image_swizzle(image))) usage |= MTLTextureUsagePixelFormatView;

	MTLTextureDescriptor *descriptor = [[MTLTextureDescriptor alloc] init];
	[descriptor setTextureType:texture_type];
	[descriptor setPixelFormat:pixel_format];
	[descriptor setWidth:image->info.extent.width];
	[descriptor setHeight:image->info.extent.height];
	[descriptor setDepth:texture_type == MTLTextureType3D ? image->info.extent.depth : 1];
	[descriptor setMipmapLevelCount:image->info.mip_levels];
	[descriptor setArrayLength:is_array ? (is_cube ? layer_count / 6 : layer_count) : 1];
	[descriptor setSampleCount:sample_count];
	[descriptor setUsage:usage];
	[descriptor setStorageMode:MTLStorageModePrivate];
	[descriptor setHazardTrackingMode:MTLHazardTrackingModeUntracked];
	return descriptor;
}

zest_bool zest__mtl_create_image(zest_device device, zest_context context, zest_image image, zest_uint layer_count, zest_sample_count_flags num_samples, zest_image_flags flags) {
	(void)context;
	//Transient images are created unbound and placed in a frame graph arena instead
	ZEST_ASSERT(ZEST__NOT_FLAGGED(flags, zest_image_flag_transient));
	if (ZEST__FLAGGED(flags, zest_image_flag_host_visible)) {
		ZEST_REPORT(device, zest_report_invalid_resource, "Metal images must be device local, so zest_image_flag_host_visible isn't supported. Upload through a staging buffer instead.");
		return ZEST_FALSE;
	}
	MTLTextureDescriptor *descriptor = zest__mtl_new_texture_descriptor(device, image, layer_count, (zest_uint)num_samples, flags);
	if (!descriptor) {
		ZEST_REPORT(device, zest_report_invalid_resource, "Image format %i has no Metal equivalent on this device.", (int)image->info.format);
		return ZEST_FALSE;
	}
	zest_mtl_device_backend_t *device_backend = ZEST_MTL_BACKEND(device, device);
	zest_mtl_image_backend_t *backend = ZEST_MTL_BACKEND(image, image);
	MTLSizeAndAlign requirement = [device_backend->device heapTextureSizeAndAlignWithDescriptor:descriptor];

	//A non zero usage is what routes the request to an image pool
	zest_buffer_info_t buffer_info = ZEST__ZERO_INIT(zest_buffer_info_t);
	buffer_info.image_usage_flags = zest_image_usage_transfer_src_bit | zest_image_usage_transfer_dst_bit;
	buffer_info.image_usage_flags |= ZEST__FLAGGED(flags, zest_image_flag_sampled) ? zest_image_usage_sampled_bit : 0;
	buffer_info.image_usage_flags |= ZEST__FLAGGED(flags, zest_image_flag_storage) ? zest_image_usage_storage_bit : 0;
	buffer_info.image_usage_flags |= ZEST__FLAGGED(flags, zest_image_flag_color_attachment) ? zest_image_usage_color_attachment_bit : 0;
	buffer_info.image_usage_flags |= ZEST__FLAGGED(flags, zest_image_flag_depth_stencil_attachment) ? zest_image_usage_depth_stencil_attachment_bit : 0;
	buffer_info.image_usage_flags |= ZEST__FLAGGED(flags, zest_image_flag_input_attachment) ? zest_image_usage_input_attachment_bit : 0;
	buffer_info.property_flags = zest_memory_property_device_local_bit;
	buffer_info.alignment = requirement.align;
	//Any texture can be placed in any of the backend's heaps, so there is one category
	buffer_info.backend_memory_bits = 1;

	zest_device_memory memory = NULL;
	zest_buffer buffer = NULL;
	id<MTLHeap> heap = nil;
	zest_size heap_offset = 0;
	if (requirement.size >= device->setup_info.dedicated_image_memory_threshold) {
		memory = zest__create_device_memory(device, requirement.size, &buffer_info, buffer_info.backend_memory_bits);
		if (!memory) {
			[descriptor release];
			return ZEST_FALSE;
		}
		heap = ZEST_MTL_BACKEND(device_memory, memory)->heap;
	} else {
		buffer = zest_CreateBuffer(device, requirement.size, &buffer_info);
		if (!buffer) {
			[descriptor release];
			return ZEST_FALSE;
		}
		ZEST_ASSERT(buffer->memory_offset % requirement.align == 0, "The image pool returned an offset that isn't aligned for the image.");
		heap = ZEST_MTL_BACKEND(device_memory_pool, buffer->memory_pool)->heap;
		heap_offset = buffer->memory_offset;
	}

	backend->texture = [heap newTextureWithDescriptor:descriptor offset:(NSUInteger)heap_offset];
	[descriptor release];
	if (!backend->texture) {
		ZEST_REPORT(device, zest_report_invalid_resource, "Metal couldn't place the image (format %i, %ux%ux%u, %u layers, %u mips, %u samples) in its heap.",
			(int)image->info.format, image->info.extent.width, image->info.extent.height, image->info.extent.depth, layer_count, image->info.mip_levels, (zest_uint)num_samples);
		if (memory) {
			zest__mtl_cleanup_memory_backend(memory);
			ZEST__FREE(device->allocator, memory);
		}
		zest_FreeBufferNow(buffer);
		return ZEST_FALSE;
	}
	image->layout = zest_image_layout_undefined;
	if (memory) {
		image->buffer = (void *)memory;
		//Cleanup reads this flag to know which kind of backing to free
		image->info.flags |= zest_image_flag_dedicated_memory;
	} else {
		image->buffer = (void *)buffer;
	}
	#ifdef ZEST_DEBUGGING
	image->buffer_identifier = (void *)backend->texture;
	#endif
	return ZEST_TRUE;
}

void zest__mtl_cleanup_image_backend(zest_image image) {
	zest_mtl_image_backend_t *backend = ZEST_MTL_BACKEND(image, image);
	if (!backend) return;
	zest_device device = (zest_device)image->handle.store->origin;
	ZEST_ASSERT(device);	//The store was not set in the image handle!
	ZEST_MTL_RELEASE(backend->texture);
	if (ZEST__NOT_FLAGGED(image->info.flags, zest_image_flag_transient)) {
		if (ZEST__FLAGGED(image->info.flags, zest_image_flag_dedicated_memory)) {
			//The zest_device_memory struct itself is freed by zest__cleanup_image
			zest__mtl_cleanup_memory_backend((zest_device_memory)image->buffer);
		}
		ZEST__FREE(device->allocator, backend);
	}
	image->backend = NULL;
}

ZEST_PRIVATE MTLTextureSwizzle zest__mtl_swizzle_channel(zest_component_swizzle swizzle, MTLTextureSwizzle identity) {
	switch (swizzle) {
		case zest_component_swizzle_zero:	return MTLTextureSwizzleZero;
		case zest_component_swizzle_one:	return MTLTextureSwizzleOne;
		case zest_component_swizzle_r:		return MTLTextureSwizzleRed;
		case zest_component_swizzle_g:		return MTLTextureSwizzleGreen;
		case zest_component_swizzle_b:		return MTLTextureSwizzleBlue;
		case zest_component_swizzle_a:		return MTLTextureSwizzleAlpha;
		default:							return identity;
	}
}

//Returns a retained texture: the image's own when the view is the whole image in its own type, otherwise a view of it
id<MTLTexture> zest__mtl_new_view_texture(zest_image image, zest_image_view_type view_type, zest_uint mip_levels_this_view, zest_uint base_mip, zest_uint base_array_index, zest_uint layer_count) {
	id<MTLTexture> texture = ZEST_MTL_BACKEND(image, image)->texture;
	MTLTextureType texture_type = zest__mtl_view_texture_type(view_type, [texture sampleCount] > 1);
	zest_bool is_3d = [texture textureType] == MTLTextureType3D;
	zest_bool texture_is_cube = [texture textureType] == MTLTextureTypeCube || [texture textureType] == MTLTextureTypeCubeArray;
	zest_uint texture_slices = is_3d ? 1 : (zest_uint)[texture arrayLength] * (texture_is_cube ? 6 : 1);
	zest_uint slice_count = is_3d ? 1 : layer_count;
	zest_uint base_slice = is_3d ? 0 : base_array_index;
	zest_component_mapping_t swizzle = zest__mtl_image_swizzle(image);
	zest_bool identity = zest__mtl_swizzle_is_identity(swizzle);
	zest_bool whole_image = base_mip == 0 && mip_levels_this_view == (zest_uint)[texture mipmapLevelCount] && base_slice == 0 && slice_count == texture_slices;
	if (identity && whole_image && texture_type == [texture textureType]) {
		return [texture retain];
	}
	NSRange levels = NSMakeRange(base_mip, mip_levels_this_view);
	NSRange slices = NSMakeRange(base_slice, slice_count);
	if (identity) {
		return [texture newTextureViewWithPixelFormat:[texture pixelFormat] textureType:texture_type levels:levels slices:slices];
	}
	MTLTextureSwizzleChannels channels;
	channels.red = zest__mtl_swizzle_channel(swizzle.r, MTLTextureSwizzleRed);
	channels.green = zest__mtl_swizzle_channel(swizzle.g, MTLTextureSwizzleGreen);
	channels.blue = zest__mtl_swizzle_channel(swizzle.b, MTLTextureSwizzleBlue);
	channels.alpha = zest__mtl_swizzle_channel(swizzle.a, MTLTextureSwizzleAlpha);
	return [texture newTextureViewWithPixelFormat:[texture pixelFormat] textureType:texture_type levels:levels slices:slices swizzle:channels];
}

zest_image_view_t *zest__mtl_create_image_view(zest_device device, zest_image image, zest_image_view_type view_type, zest_uint mip_levels_this_view, zest_uint base_mip, zest_uint base_array_index, zest_uint layer_count, zloc_linear_allocator_t *linear_allocator) {
	zest_size total_size = sizeof(zest_image_view_t) + sizeof(zest_mtl_image_view_backend_t);
	void *memory = NULL;
	zest_size marker = 0;
	if (linear_allocator) {
		marker = zloc_GetMarker(linear_allocator);
		memory = zest__linear_allocate(linear_allocator, total_size);
	} else {
		memory = ZEST__ALLOCATE(device->allocator, total_size);
	}
	zest_image_view_t *image_view = (zest_image_view_t *)memory;
	zest_mtl_image_view_backend_t *backend = (zest_mtl_image_view_backend_t *)((char *)memory + sizeof(zest_image_view_t));
	*image_view = ZEST__ZERO_INIT(zest_image_view_t);
	*backend = ZEST__ZERO_INIT(zest_mtl_image_view_backend_t);
	image_view->image = image;
	image_view->backend = (zest_image_view_backend_t *)backend;
	backend->texture = zest__mtl_new_view_texture(image, view_type, mip_levels_this_view, base_mip, base_array_index, layer_count);
	if (!backend->texture) {
		ZEST_APPEND_LOG(device->log_path.str, "Metal couldn't create a texture view (view type %i, mips %u from %u, layers %u from %u).", (int)view_type, mip_levels_this_view, base_mip, layer_count, base_array_index);
		if (linear_allocator) {
			zloc_ResetToMarker(linear_allocator, marker);
		} else {
			ZEST__FREE(device->allocator, memory);
		}
		return NULL;
	}
	return image_view;
}

void zest__mtl_cleanup_image_view_backend(zest_image_view view) {
	zest_mtl_image_view_backend_t *backend = ZEST_MTL_BACKEND(image_view, view);
	if (!backend) return;
	ZEST_MTL_RELEASE(backend->texture);
}

//The views and their backends are one block that shared code frees, so only the textures are released here
void zest__mtl_cleanup_image_view_array_backend(zest_image_view_array view_array) {
	for (zest_uint mip_index = 0; mip_index != view_array->count; ++mip_index) {
		zest_mtl_image_view_backend_t *backend = ZEST_MTL_BACKEND(image_view, &view_array->views[mip_index]);
		if (backend) {
			ZEST_MTL_RELEASE(backend->texture);
		}
	}
	view_array->magic = 0;
}

zest_image_view_array zest__mtl_create_image_views_per_mip(zest_device device, zest_image image, zest_image_view_type view_type, zest_uint base_array_index, zest_uint layer_count, zloc_linear_allocator_t *linear_allocator) {
	zest_uint mip_count = image->info.mip_levels;
	zest_size public_array_size = sizeof(zest_image_view_t) * mip_count;
	zest_size total_size = public_array_size + sizeof(zest_mtl_image_view_backend_t) * mip_count;
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
	zest_mtl_image_view_backend_t *backend_array = (zest_mtl_image_view_backend_t *)((char *)memory + public_array_size);
	for (zest_uint mip_index = 0; mip_index != mip_count; ++mip_index) {
		zest_image_view view = &view_array->views[mip_index];
		*view = ZEST__ZERO_INIT(zest_image_view_t);
		backend_array[mip_index] = ZEST__ZERO_INIT(zest_mtl_image_view_backend_t);
		view->image = image;
		view->backend = (zest_image_view_backend_t *)&backend_array[mip_index];
		backend_array[mip_index].texture = zest__mtl_new_view_texture(image, view_type, 1, mip_index, base_array_index, layer_count);
		if (!backend_array[mip_index].texture) {
			for (zest_uint created_index = 0; created_index != mip_index; ++created_index) {
				ZEST_MTL_RELEASE(backend_array[created_index].texture);
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

void *zest__mtl_new_sampler_backend(zest_device device) {
	zest_mtl_sampler_backend_t *backend = (zest_mtl_sampler_backend_t *)ZEST__ALLOCATE(device->allocator, sizeof(zest_mtl_sampler_backend_t));
	*backend = ZEST__ZERO_INIT(zest_mtl_sampler_backend_t);
	return backend;
}

//Metal samplers have no LOD bias, so mip_lod_bias is ignored
zest_bool zest__mtl_create_sampler(zest_sampler sampler) {
	static const MTLSamplerAddressMode address_modes[] = { MTLSamplerAddressModeRepeat, MTLSamplerAddressModeMirrorRepeat, MTLSamplerAddressModeClampToEdge, MTLSamplerAddressModeClampToBorderColor, MTLSamplerAddressModeMirrorClampToEdge };
	static const MTLCompareFunction compare_functions[] = { MTLCompareFunctionNever, MTLCompareFunctionLess, MTLCompareFunctionEqual, MTLCompareFunctionLessEqual, MTLCompareFunctionGreater, MTLCompareFunctionNotEqual, MTLCompareFunctionGreaterEqual, MTLCompareFunctionAlways };
	static const MTLSamplerBorderColor border_colors[] = { MTLSamplerBorderColorTransparentBlack, MTLSamplerBorderColorOpaqueBlack, MTLSamplerBorderColorOpaqueWhite };
	zest_device device = (zest_device)sampler->handle.store->origin;
	zest_mtl_sampler_backend_t *backend = ZEST_MTL_BACKEND(sampler, sampler);
	const zest_sampler_info_t *info = &sampler->create_info;

	MTLSamplerDescriptor *descriptor = [[MTLSamplerDescriptor alloc] init];
	[descriptor setMinFilter:info->min_filter == zest_filter_linear ? MTLSamplerMinMagFilterLinear : MTLSamplerMinMagFilterNearest];
	[descriptor setMagFilter:info->mag_filter == zest_filter_linear ? MTLSamplerMinMagFilterLinear : MTLSamplerMinMagFilterNearest];
	[descriptor setMipFilter:info->mipmap_mode == zest_mipmap_mode_linear ? MTLSamplerMipFilterLinear : MTLSamplerMipFilterNearest];
	[descriptor setSAddressMode:address_modes[info->address_mode_u]];
	[descriptor setTAddressMode:address_modes[info->address_mode_v]];
	[descriptor setRAddressMode:address_modes[info->address_mode_w]];
	[descriptor setMaxAnisotropy:info->anisotropy_enable ? (NSUInteger)ZEST__CLAMP(info->max_anisotropy, 1.f, 16.f) : 1];
	[descriptor setCompareFunction:info->compare_enable ? compare_functions[info->compare_op] : MTLCompareFunctionNever];
	[descriptor setBorderColor:border_colors[info->border_color / 2]];
	[descriptor setLodMinClamp:info->min_lod];
	[descriptor setLodMaxClamp:info->max_lod];
	//The bindless table holds the sampler's resource ID, which only a sampler made for argument buffers has
	[descriptor setSupportArgumentBuffers:YES];
	backend->sampler = [ZEST_MTL_BACKEND(device, device)->device newSamplerStateWithDescriptor:descriptor];
	[descriptor release];
	if (!backend->sampler) {
		ZEST_APPEND_LOG(device->log_path.str, "Metal couldn't create a sampler.");
		return ZEST_FALSE;
	}
	return ZEST_TRUE;
}

void zest__mtl_cleanup_sampler_backend(zest_sampler sampler) {
	zest_mtl_sampler_backend_t *backend = ZEST_MTL_BACKEND(sampler, sampler);
	if (!backend) return;
	zest_device device = (zest_device)sampler->handle.store->origin;
	ZEST_MTL_RELEASE(backend->sampler);
	ZEST__FREE(device->allocator, backend);
	sampler->backend = NULL;
}

int zest__mtl_get_image_raw_layout(zest_image image) {
	return (int)image->layout;
}

zest_bool zest__mtl_image_layout_is_valid_for_descriptor(zest_image image) {
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

//Only records the layout: the queue fences order the work and Metal has no layout to change
zest_bool zest__mtl_transition_image(zest_queue queue, zest_image image, zest_resource_state new_state, zest_uint base_mip_index, zest_uint mip_levels, zest_uint base_array_index, zest_uint layer_count) {
	(void)queue;
	(void)base_mip_index;
	(void)mip_levels;
	(void)base_array_index;
	(void)layer_count;
	image->layout = zest__mtl_image_layout_from_resource_state(new_state, zest__is_depth_stencil_format(image->info.format));
	return ZEST_TRUE;
}

zest_bool zest__mtl_copy_buffer_regions_to_image(zest_queue queue, zest_buffer_image_copy_t *regions, zest_uint regions_count, zest_buffer buffer, zest_size src_offset, zest_image image) {
	zest_device device = queue->device;
	id<MTLTexture> texture = ZEST_MTL_BACKEND(image, image)->texture;
	id<MTLBuffer> source = zest__mtl_buffer_object(buffer);
	zest_bool is_3d = [texture textureType] == MTLTextureType3D;
	zest_uint layer_limit = is_3d ? 1 : image->info.layer_count;
	int channels, bytes_per_pixel, block_width, block_height, bytes_per_block;
	zest_GetFormatPixelData(image->info.format, &channels, &bytes_per_pixel, &block_width, &block_height, &bytes_per_block);
	for (zest_uint region_index = 0; region_index != regions_count; ++region_index) {
		const zest_buffer_image_copy_t *region = &regions[region_index];
		if (region->mip_level >= image->info.mip_levels) {
			ZEST_ASSERT_OR_VALIDATE(ZEST_FALSE, device, "A buffer to image copy region's mip level is outside the image.", ZEST_FALSE);
			return ZEST_FALSE;
		}
		if (!region->layer_count || region->layer_count > layer_limit || region->base_array_layer > layer_limit - region->layer_count) {
			ZEST_ASSERT_OR_VALIDATE(ZEST_FALSE, device, "A buffer to image copy region's array layers are empty or outside the image.", ZEST_FALSE);
			return ZEST_FALSE;
		}
		//A row length or image height of 0 means the buffer is packed as tightly as the region
		zest_size row_texels = region->buffer_row_length ? region->buffer_row_length : region->image_extent.width;
		zest_size image_rows = region->buffer_image_height ? region->buffer_image_height : region->image_extent.height;
		zest_size bytes_per_row = block_width > 1 ? ((row_texels + block_width - 1) / block_width) * bytes_per_block : row_texels * bytes_per_pixel;
		zest_size rows_per_image = block_height > 1 ? (image_rows + block_height - 1) / block_height : image_rows;
		zest_size bytes_per_image = bytes_per_row * rows_per_image;
		zest_size depth = region->image_extent.depth ? region->image_extent.depth : 1;
		zest_size region_size = bytes_per_image * depth * region->layer_count;
		zest_size region_start = src_offset + region->buffer_offset;
		if (region_start < buffer->memory_offset || region_start - buffer->memory_offset > buffer->size || region_size > buffer->size - (region_start - buffer->memory_offset)) {
			ZEST_ASSERT_OR_VALIDATE(ZEST_FALSE, device, "A buffer to image copy region reads past the end of its source buffer.", ZEST_FALSE);
			return ZEST_FALSE;
		}
		id<MTLBlitCommandEncoder> encoder = zest__mtl_queue_blit_encoder(queue);
		MTLBlitOption option = zest__mtl_blit_option(image->info.format, region->image_aspect);
		for (zest_uint layer = 0; layer != region->layer_count; ++layer) {
			[encoder copyFromBuffer:source sourceOffset:(NSUInteger)(region_start + layer * bytes_per_image * depth) sourceBytesPerRow:(NSUInteger)bytes_per_row sourceBytesPerImage:(NSUInteger)bytes_per_image
				sourceSize:MTLSizeMake(region->image_extent.width, region->image_extent.height, depth) toTexture:texture destinationSlice:is_3d ? 0 : region->base_array_layer + layer destinationLevel:region->mip_level
				destinationOrigin:MTLOriginMake(region->image_offset.x, region->image_offset.y, region->image_offset.z) options:option];
		}
	}
	return ZEST_TRUE;
}

zest_bool zest__mtl_copy_buffer_to_image(zest_queue queue, zest_buffer buffer, zest_size src_offset, zest_image image, zest_uint width, zest_uint height) {
	zest_buffer_image_copy_t region = ZEST__ZERO_INIT(zest_buffer_image_copy_t);
	region.image_aspect = zest_image_aspect_color_bit;
	region.layer_count = 1;
	region.image_extent.width = width;
	region.image_extent.height = height;
	region.image_extent.depth = image->info.extent.depth > 1 ? image->info.extent.depth : 1;
	return zest__mtl_copy_buffer_regions_to_image(queue, &region, 1, buffer, src_offset, image);
}

//Leaves the image in shader read, as the other backends do
zest_bool zest__mtl_generate_mipmaps(zest_queue queue, zest_image image) {
	zest_device device = queue->device;
	if (image->info.mip_levels > 1) {
		//Metal filters each level down from the one above, which it can't do for depth or block compressed formats
		if (zest__is_depth_stencil_format(image->info.format) || zest__is_compressed_format(image->info.format)) {
			ZEST_REPORT(device, zest_report_invalid_resource, "Metal can't generate mipmaps for image format %i. Upload each mip level instead.", (int)image->info.format);
			return ZEST_FALSE;
		}
		[zest__mtl_queue_blit_encoder(queue) generateMipmapsForTexture:ZEST_MTL_BACKEND(image, image)->texture];
	}
	image->layout = zest_image_layout_shader_read_only_optimal;
	return ZEST_TRUE;
}

zest_bool zest__mtl_copy_bitmap_to_image(zest_device device, void *bitmap, zest_size image_size, zest_image dst_image, zest_uint width, zest_uint height) {
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
		recorded = zest__mtl_generate_mipmaps(queue, dst_image);
	}
	recorded = recorded && zest_imm_TransitionImage(queue, dst_image, zest_resource_state_shader_read, 0, dst_image->info.mip_levels, 0, dst_image->info.layer_count);
	//Ending also submits a partly recorded command buffer, which releases the queue either way
	zest_bool finished = zest_imm_EndCommandBuffer(queue);
	if (finished) {
		zest_FreeBufferNow(staging_buffer);
	} else {
		zest_FreeBuffer(staging_buffer);
	}
	return recorded && finished;
}

//Reads the first depth slice of the first mip of the first layer, tightly packed
zest_bool zest__mtl_copy_image_to_bitmap(zest_device device, zest_image src_image, void *pixels) {
	id<MTLTexture> texture = ZEST_MTL_BACKEND(image, src_image)->texture;
	int channels, bytes_per_pixel, block_width, block_height, bytes_per_block;
	zest_GetFormatPixelData(src_image->info.format, &channels, &bytes_per_pixel, &block_width, &block_height, &bytes_per_block);
	zest_size width = src_image->info.extent.width;
	zest_size height = src_image->info.extent.height;
	zest_size bytes_per_row = block_width > 1 ? ((width + block_width - 1) / block_width) * bytes_per_block : width * bytes_per_pixel;
	zest_size total_size = bytes_per_row * (block_height > 1 ? (height + block_height - 1) / block_height : height);

	zest_buffer_info_t buffer_info = zest_CreateBufferInfo(zest_buffer_type_staging, zest_memory_usage_gpu_to_cpu);
	zest_buffer readback_buffer = zest_CreateBuffer(device, total_size, &buffer_info);
	if (!readback_buffer) {
		return ZEST_FALSE;
	}
	zest_queue queue = zest_imm_BeginCommandBuffer(device, zest_queue_graphics);
	if (!queue) {
		zest_FreeBufferNow(readback_buffer);
		return ZEST_FALSE;
	}
	[zest__mtl_queue_blit_encoder(queue) copyFromTexture:texture sourceSlice:0 sourceLevel:0 sourceOrigin:MTLOriginMake(0, 0, 0) sourceSize:MTLSizeMake(width, height, 1)
		toBuffer:zest__mtl_buffer_object(readback_buffer) destinationOffset:(NSUInteger)readback_buffer->memory_offset destinationBytesPerRow:(NSUInteger)bytes_per_row destinationBytesPerImage:(NSUInteger)total_size
		options:zest__mtl_blit_option(src_image->info.format, zest_image_aspect_depth_bit)];
	if (!zest_imm_EndCommandBuffer(queue)) {
		zest_FreeBuffer(readback_buffer);
		return ZEST_FALSE;
	}
	memcpy(pixels, (const char *)readback_buffer->memory_pool->mapped + readback_buffer->memory_offset, total_size);
	zest_FreeBufferNow(readback_buffer);
	return ZEST_TRUE;
}
// -- End Images

// -- General_helpers
// -- End General_helpers

// -- Internal_Frame_graph_context_functions
void *zest__mtl_new_execution_backend(zloc_linear_allocator_t *allocator) {
	zest_mtl_execution_backend_t *backend = (zest_mtl_execution_backend_t *)zest__linear_allocate(allocator, sizeof(zest_mtl_execution_backend_t));
	*backend = ZEST__ZERO_INIT(zest_mtl_execution_backend_t);
	return backend;
}

zest_frame_graph_semaphores zest__mtl_get_frame_graph_semaphores(zest_context context, const char *name) {
	if (zest_map_valid_name(context->cached_frame_graph_semaphores, name)) {
		return *zest_map_at(context->cached_frame_graph_semaphores, name);
	}
	zest_device device = context->device;
	id<MTLDevice> native_device = ZEST_MTL_BACKEND(device, device)->device;
	zest_frame_graph_semaphores semaphores = (zest_frame_graph_semaphores)ZEST__NEW(context->allocator, zest_frame_graph_semaphores);
	*semaphores = ZEST__ZERO_INIT(zest_frame_graph_semaphores_t);
	semaphores->magic = zest_INIT_MAGIC(zest_struct_type_frame_graph_semaphores);
	zest_mtl_frame_graph_semaphores_backend_t *backend = (zest_mtl_frame_graph_semaphores_backend_t *)zest__mtl_new_frame_graph_semaphores_backend(context);
	semaphores->backend = (zest_frame_graph_semaphores_backend)backend;
	zest_ForEachFrameInFlight(frame_in_flight) {
		for (zest_uint queue_index = 0; queue_index != ZEST_QUEUE_COUNT; ++queue_index) {
			backend->events[frame_in_flight][queue_index] = [native_device newSharedEvent];
			//A batch that would signal a missing event fails its submission instead
			if (!backend->events[frame_in_flight][queue_index]) {
				ZEST_APPEND_LOG(device->log_path.str, "A Metal shared event couldn't be created for the frame graph %s.", name);
			}
		}
	}
	zest_map_insert(context->allocator, context->cached_frame_graph_semaphores, name, semaphores);
	return semaphores;
}

//zest_PrintCompiledFrameGraph reads the node lists beside shared code's debug records, so every barrier keeps its node
void zest__mtl_add_frame_graph_image_barrier(zest_resource_node resource, zest_execution_barriers_t *barriers, zest_bool acquire, zest_bool first_use, zest_access_flags src_access, zest_access_flags dst_access, zest_image_layout old_layout, zest_image_layout new_layout, zest_uint src_family, zest_uint dst_family, zest_pipeline_stage_flags src_stage, zest_pipeline_stage_flags dst_stage) {
	zest_context context = resource->frame_graph->command_list.context;
	zloc_linear_allocator_t *allocator = &context->frame_graph_allocator[context->current_fif];
	zest_mtl_execution_barriers_backend_t *backend = ZEST_MTL_BACKEND(execution_barriers, barriers);
	zest_mtl_frame_graph_image_barrier_t record;
	record.resource = resource;
	record.new_layout = new_layout;
	record.crosses_queues = src_family != ZEST_QUEUE_FAMILY_IGNORED && dst_family != ZEST_QUEUE_FAMILY_IGNORED && src_family != dst_family;
	if (acquire) {
		zest_vec_linear_push(allocator, backend->acquire_images, record);
		zest_vec_linear_push(allocator, barriers->acquire_image_barrier_nodes, resource);
	} else {
		zest_vec_linear_push(allocator, backend->release_images, record);
		zest_vec_linear_push(allocator, barriers->release_image_barrier_nodes, resource);
	}
}

void zest__mtl_add_frame_graph_buffer_barrier(zest_resource_node resource, zest_execution_barriers_t *barriers, zest_bool acquire, zest_access_flags src_access, zest_access_flags dst_access, zest_uint src_family, zest_uint dst_family, zest_pipeline_stage_flags src_stage, zest_pipeline_stage_flags dst_stage) {
	zest_context context = resource->frame_graph->command_list.context;
	zloc_linear_allocator_t *allocator = &context->frame_graph_allocator[context->current_fif];
	if (acquire) {
		zest_vec_linear_push(allocator, barriers->acquire_buffer_barrier_nodes, resource);
	} else {
		zest_vec_linear_push(allocator, barriers->release_buffer_barrier_nodes, resource);
	}
}

//A Metal command buffer belongs to its queue from creation, so the batch's device queue is acquired here
zest_bool zest__mtl_set_next_command_buffer(const zest_command_list command_list, zest_context_queue queue) {
	zest_context context = command_list->context;
	zest_mtl_context_queue_backend_t *queue_backend = ZEST_MTL_BACKEND(context_queue, queue);
	zest_mtl_command_list_backend_t *backend = ZEST_MTL_BACKEND(command_list, command_list);
	zest__mtl_reap_command_buffers(context, queue, ZEST_FALSE);
	//A batch that failed before its commit left its buffer behind
	ZEST_MTL_RELEASE(queue_backend->command_buffer);
	backend->switched_queue = zest__acquire_context_queue(context, queue, command_list->queue_index);
	zest_mtl_queue_backend_t *device_queue_backend = ZEST_MTL_BACKEND(queue, queue->queue);
	@autoreleasepool {
		//Zest keeps every resource a batch uses alive until its frame in flight comes round again
		queue_backend->command_buffer = [[device_queue_backend->queue commandBufferWithUnretainedReferences] retain];
	}
	if (!queue_backend->command_buffer) {
		ZEST_REPORT(context->device, zest_report_submission_failure, "Metal couldn't supply a command buffer for a frame graph batch.");
		return ZEST_FALSE;
	}
	backend->command_buffer = queue_backend->command_buffer;
	backend->context_queue = queue;
	backend->fence = device_queue_backend->fence;
	//Counts buffers begun since the pool was reset, as on Vulkan
	queue->next_buffer++;
	return ZEST_TRUE;
}

//Metal orders a wait by where it sits among the buffer's encoders, so the batch's waits go in before any pass
zest_bool zest__mtl_begin_command_buffer(const zest_command_list command_list) {
	zest_context context = command_list->context;
	zest_mtl_command_list_backend_t *backend = ZEST_MTL_BACKEND(command_list, command_list);
	id<MTLCommandBuffer> command_buffer = backend->command_buffer;
	if (command_list->submission_index > 0) {
		zest_mtl_execution_backend_t *execution = backend->execution;
		for (zest_uint wait_index = 0; wait_index != execution->wave_wait_count; ++wait_index) {
			[command_buffer encodeWaitForEvent:execution->wave_wait_events[wait_index] value:execution->wave_wait_values[wait_index]];
		}
	} else {
		zest_execution_timeline wait_timeline = command_list->frame_graph->wait_on_timeline;
		if (wait_timeline && wait_timeline->current_value > 0) {
			[command_buffer encodeWaitForEvent:ZEST_MTL_BACKEND(execution_timeline, wait_timeline)->event value:wait_timeline->current_value];
		}
	}
	//Orders this batch after the slot's last one when that ran on a different device queue
	zest_execution_timeline ordering_timeline = &context->queue_ordering_timelines[command_list->queue_index];
	if (backend->switched_queue && ordering_timeline->current_value > 0) {
		[command_buffer encodeWaitForEvent:ZEST_MTL_BACKEND(execution_timeline, ordering_timeline)->event value:ordering_timeline->current_value];
	}
	return ZEST_TRUE;
}

//Shared code calls this even when begin_render_pass failed, so only a pass that began is ended
void zest__mtl_end_render_pass(const zest_command_list command_list) {
	zest_mtl_command_list_backend_t *backend = ZEST_MTL_BACKEND(command_list, command_list);
	if (!backend->render_encoder) return;
	[backend->render_encoder updateFence:backend->fence afterStages:MTLRenderStageFragment];
	[backend->render_encoder endEncoding];
	ZEST_MTL_RELEASE(backend->render_encoder);
}

//The commit is the submission's, so this only closes a pass that a failed execution left open
void zest__mtl_end_command_buffer(const zest_command_list command_list) {
	zest__mtl_end_render_pass(command_list);
}

//Metal has no layouts to change. The fences order passes on a queue and the shared events order queues, so a barrier only moves the layout Zest tracks.
void zest__mtl_apply_image_barriers(const zest_mtl_frame_graph_image_barrier_t *records, zest_bool release) {
	zest_vec_foreach(record_index, records) {
		const zest_mtl_frame_graph_image_barrier_t *record = &records[record_index];
		//A release to another queue leaves the layout to the matching acquire, as on Vulkan
		if (release && record->crosses_queues) continue;
		zest_resource_node resource = record->resource;
		resource->view->image->layout = record->new_layout;
		if (resource->linked_layout) {
			*resource->linked_layout = record->new_layout;
			resource->image_layout = record->new_layout;
		}
	}
}

void zest__mtl_acquire_barrier(const zest_command_list command_list, zest_execution_details_t *exe_details) {
	(void)command_list;
	zest__mtl_apply_image_barriers(ZEST_MTL_BACKEND(execution_barriers, &exe_details->barriers)->acquire_images, ZEST_FALSE);
}

void zest__mtl_release_barrier(const zest_command_list command_list, zest_execution_details_t *exe_details) {
	(void)command_list;
	zest__mtl_apply_image_barriers(ZEST_MTL_BACKEND(execution_barriers, &exe_details->barriers)->release_images, ZEST_TRUE);
}

zest_bool zest__mtl_begin_render_pass(const zest_command_list command_list, zest_execution_details_t *exe_details) {
	zest_context context = command_list->context;
	zest_mtl_command_list_backend_t *backend = ZEST_MTL_BACKEND(command_list, command_list);
	MTLRenderPassDescriptor *descriptor = ZEST_MTL_BACKEND(context, context)->render_pass_descriptor;
	zest_uint color_attachment_count = zest_vec_size(exe_details->color_attachments);
	ZEST_ASSERT(color_attachment_count <= ZEST_MAX_ATTACHMENTS, "A render pass has more color attachments than ZEST_MAX_ATTACHMENTS.");
	zest_bool has_depth = exe_details->depth_attachment.image_view != NULL;
	zest_bool has_resolve = exe_details->depth_attachment.resolve_image_view != NULL;
	for (zest_uint attachment_index = 0; attachment_index != color_attachment_count; ++attachment_index) {
		has_resolve |= exe_details->color_attachments[attachment_index].resolve_image_view != NULL;
	}
	if (has_resolve) {
		return zest__mtl_report_unavailable(context->device, "The Metal backend can't resolve a multisampled attachment yet. Use zest_BeginVulkanDeviceBuilder on macOS until it can.");
	}
	if (exe_details->rendering_info.view_mask) {
		return zest__mtl_report_unavailable(context->device, "The Metal backend can't render to more than one layer in a pass yet. Use zest_BeginVulkanDeviceBuilder on macOS until it can.");
	}
	id<MTLTexture> first_texture = nil;
	@autoreleasepool {
		for (zest_uint attachment_index = 0; attachment_index != color_attachment_count; ++attachment_index) {
			zest_rendering_attachment_info_t *info = &exe_details->color_attachments[attachment_index];
			MTLRenderPassColorAttachmentDescriptor *attachment = [[descriptor colorAttachments] objectAtIndexedSubscript:attachment_index];
			id<MTLTexture> texture = ZEST_MTL_BACKEND(image_view, *info->image_view)->texture;
			//The swapchain's colour is read now, so a change made after the graph was cached still applies
			zest_vec4 color = info->is_swapchain_image ? context->swapchain->clear_color.color : info->resource->clear_color;
			[attachment setTexture:texture];
			[attachment setLoadAction:zest__mtl_load_action(info->usage.load_op)];
			[attachment setStoreAction:zest__mtl_store_action(info->usage.store_op)];
			[attachment setClearColor:MTLClearColorMake(color.r, color.g, color.b, color.a)];
			if (!first_texture) first_texture = texture;
		}
		//Only the depth plane is attached, as Vulkan's dynamic rendering does, so a stencil plane is left untouched
		if (has_depth) {
			zest_rendering_attachment_info_t *info = &exe_details->depth_attachment;
			MTLRenderPassDepthAttachmentDescriptor *attachment = [descriptor depthAttachment];
			id<MTLTexture> texture = ZEST_MTL_BACKEND(image_view, *info->image_view)->texture;
			[attachment setTexture:texture];
			[attachment setLoadAction:zest__mtl_load_action(info->usage.load_op)];
			[attachment setStoreAction:zest__mtl_store_action(info->usage.store_op)];
			[attachment setClearDepth:info->clear_value.depth_stencil.depth];
			if (!first_texture) first_texture = texture;
		}
		backend->render_encoder = [[backend->command_buffer renderCommandEncoderWithDescriptor:descriptor] retain];
		//The descriptor holds a reference to each texture, which would keep a freed image's memory until the context's next pass
		for (zest_uint attachment_index = 0; attachment_index != color_attachment_count; ++attachment_index) {
			[[[descriptor colorAttachments] objectAtIndexedSubscript:attachment_index] setTexture:nil];
		}
		if (has_depth) {
			[[descriptor depthAttachment] setTexture:nil];
		}
	}
	if (!backend->render_encoder) {
		ZEST_REPORT(context->device, zest_report_render_pass_skipped, "Metal couldn't open a render pass on a frame graph batch.");
		return ZEST_FALSE;
	}
	[backend->render_encoder waitForFence:backend->fence beforeStages:MTLRenderStageVertex];
	backend->target_width = (zest_uint)[first_texture width];
	backend->target_height = (zest_uint)[first_texture height];
	return ZEST_TRUE;
}

//Signals go in after every pass, so they are reached once the batch's work has run. Nothing after the commit can fail.
zest_bool zest__mtl_submit_frame_graph_batch(zest_frame_graph frame_graph, zest_execution_backend backend, zest_submission_batch_t *batch, zest_map_queue_value *queues) {
	(void)backend;
	zest_context context = frame_graph->command_list.context;
	zest_mtl_command_list_backend_t *command_list_backend = ZEST_MTL_BACKEND(command_list, &frame_graph->command_list);
	zest_mtl_frame_graph_semaphores_backend_t *semaphores_backend = ZEST_MTL_BACKEND(frame_graph_semaphores, frame_graph->semaphores);
	zest_uint queue_index = frame_graph->command_list.queue_index;
	zest_uint frame_in_flight = context->current_fif;
	zest_bool last_wave = frame_graph->command_list.submission_index == zest_vec_size(frame_graph->submissions) - 1;
	zest_context_queue context_queue = batch->queue;
	zest_mtl_context_queue_backend_t *queue_backend = ZEST_MTL_BACKEND(context_queue, context_queue);
	id<MTLCommandBuffer> command_buffer = queue_backend->command_buffer;
	id<MTLSharedEvent> batch_event = semaphores_backend->events[frame_in_flight][queue_index];
	if (!command_buffer) return ZEST_FALSE;
	//get_frame_graph_semaphores logged why the event is missing
	if (!last_wave && !batch_event) return ZEST_FALSE;

	//Records which queues this graph used so their frame in flight indexes cycle at the end of execution
	zest_map_insert_linear_key(&context->frame_graph_allocator[frame_in_flight], (*queues), (zest_key)context_queue, context_queue);

	//The last wave signals the timeline the CPU waits on, every earlier wave the event the next wave waits on
	id<MTLSharedEvent> signal_event = nil;
	if (last_wave) {
		zest_execution_timeline signal_timeline = frame_graph->signal_timeline;
		if (signal_timeline) {
			signal_event = ZEST_MTL_BACKEND(execution_timeline, signal_timeline)->event;
			signal_timeline->current_value += 1;
			[command_buffer encodeSignalEvent:signal_event value:signal_timeline->current_value];
			context->frame_sync_timeline[frame_in_flight] = signal_timeline;
		}
	} else {
		signal_event = batch_event;
		zest_size *batch_value = &frame_graph->semaphores->values[frame_in_flight][queue_index];
		*batch_value += 1;
		[command_buffer encodeSignalEvent:batch_event value:*batch_value];
	}
	zest_execution_timeline ordering_timeline = &context->queue_ordering_timelines[queue_index];
	ordering_timeline->current_value += 1;
	[command_buffer encodeSignalEvent:ZEST_MTL_BACKEND(execution_timeline, ordering_timeline)->event value:ordering_timeline->current_value];

	//The compiler gives the render-finished reference to the batch that holds the frame's last write to the swapchain
	zest_mtl_context_backend_t *context_backend = ZEST_MTL_BACKEND(context, context);
	if (context_backend->drawable) {
		zest_vec_foreach(semaphore_index, batch->signal_semaphores) {
			if (batch->signal_semaphores[semaphore_index].type != zest_dynamic_resource_render_finished_semaphore) continue;
			[command_buffer presentDrawable:context_backend->drawable];
			context_backend->present_count++;
			break;
		}
	}

	[command_buffer commit];
	zest_vec_push(context->allocator, queue_backend->committed[frame_in_flight], command_buffer);
	queue_backend->command_buffer = nil;
	command_list_backend->command_buffer = nil;
	ZEST__FLAG(context->flags, zest_context_flag_work_was_submitted);
	ZEST_MTL_BACKEND(submission_batch, batch)->signal_event = signal_event;
	return ZEST_TRUE;
}

void zest__mtl_carry_over_semaphores(zest_frame_graph frame_graph, zest_wave_submission_t *wave_submission, zest_execution_backend backend) {
	zest_context context = frame_graph->command_list.context;
	zest_mtl_execution_backend_t *execution_backend = (zest_mtl_execution_backend_t *)backend;
	zest_mtl_frame_graph_semaphores_backend_t *semaphores_backend = ZEST_MTL_BACKEND(frame_graph_semaphores, frame_graph->semaphores);
	execution_backend->wave_wait_count = 0;
	for (zest_uint queue_index = 0; queue_index != ZEST_QUEUE_COUNT; ++queue_index) {
		id<MTLSharedEvent> event = semaphores_backend->events[context->current_fif][queue_index];
		if (!wave_submission->batches[queue_index].magic || !event) continue;
		execution_backend->wave_wait_events[execution_backend->wave_wait_count] = event;
		execution_backend->wave_wait_values[execution_backend->wave_wait_count] = frame_graph->semaphores->values[context->current_fif][queue_index];
		execution_backend->wave_wait_count++;
	}
	ZEST_MTL_BACKEND(command_list, &frame_graph->command_list)->execution = execution_backend;
}
// -- End Internal_Frame_graph_context_functions

// -- Frame_graph_context_functions
//Records nothing, as the bindless table is the only set there is and it's bound when a pipeline is
void zest__mtl_bind_descriptor_sets(const zest_command_list command_list, zest_pipeline_bind_point bind_point, zest_pipeline_layout layout, zest_descriptor_set *descriptor_sets, zest_uint set_count, zest_uint first_set) {
	(void)bind_point;
	(void)layout;
	(void)first_set;
	zest_device device = command_list->context->device;
	for (zest_uint set_index = 0; set_index != set_count; ++set_index) {
		if (descriptor_sets[set_index] != device->bindless_set) {
			zest__log_validation_error(device, "Only the bindless descriptor set can be bound on the Metal backend, as custom descriptor sets aren't available there.");
			return;
		}
	}
}

//Passed straight through, as the converted shaders flip Y themselves. Like the scissor, it needs a pass to be open.
void zest__mtl_viewport(const zest_command_list command_list, zest_viewport_t *viewport) {
	id<MTLRenderCommandEncoder> encoder = ZEST_MTL_BACKEND(command_list, command_list)->render_encoder;
	if (!encoder) return;
	MTLViewport native_viewport = { viewport->x, viewport->y, viewport->width, viewport->height, viewport->minDepth, viewport->maxDepth };
	[encoder setViewport:native_viewport];
}

void zest__mtl_scissor(const zest_command_list command_list, zest_scissor_rect_t *scissor) {
	zest_mtl_command_list_backend_t *backend = ZEST_MTL_BACKEND(command_list, command_list);
	if (!backend->render_encoder) return;
	[backend->render_encoder setScissorRect:zest__mtl_clamped_scissor(scissor->offset.x, scissor->offset.y, scissor->extent.width, scissor->extent.height, backend->target_width, backend->target_height)];
}

void zest__mtl_clip(const zest_command_list command_list, float x, float y, float width, float height, float min_depth, float max_depth) {
	zest_mtl_command_list_backend_t *backend = ZEST_MTL_BACKEND(command_list, command_list);
	if (!backend->render_encoder) return;
	MTLViewport native_viewport = { x, y, width, height, min_depth, max_depth };
	[backend->render_encoder setViewport:native_viewport];
	[backend->render_encoder setScissorRect:zest__mtl_clamped_scissor((int)x, (int)y, (zest_uint)width, (zest_uint)height, backend->target_width, backend->target_height)];
}

void zest__mtl_set_screensized_viewport(const zest_command_list command_list, float min_depth, float max_depth) {
	//Must be called from a pass callback of a graph that renders to the swapchain
	zest_swapchain swapchain = command_list->frame_graph->swapchain;
	zest__mtl_clip(command_list, 0.f, 0.f, (float)swapchain->size.width, (float)swapchain->size.height, min_depth, max_depth);
}
// -- End Frame_graph_context_functions

// -- Shader_compiling
// -- End Shader_compiling

// -- GPU_Profiling_backend
// -- End GPU_Profiling_backend

// -- Debug_naming
//The handle is any Metal object that takes a label
void zest__mtl_set_object_name(zest_device device, zest_uint object_type, zest_u64 object_handle, const char *name) {
	(void)device;
	(void)object_type;
	id object = (id)(void *)(uintptr_t)object_handle;
	if (!object || !name || ![object respondsToSelector:@selector(setLabel:)]) return;
	@autoreleasepool {
		[object setLabel:[NSString stringWithUTF8String:name]];
	}
}
void zest__mtl_set_buffer_pool_name(zest_device device, zest_device_memory_pool pool, const char *name) {
	(void)device;
	zest_mtl_device_memory_pool_backend_t *backend = ZEST_MTL_BACKEND(device_memory_pool, pool);
	if (!backend || !name) return;
	@autoreleasepool {
		//Image pools have a heap and no buffer
		if (backend->buffer) {
			[backend->buffer setLabel:[NSString stringWithUTF8String:name]];
		} else {
			[backend->heap setLabel:[NSString stringWithUTF8String:name]];
		}
	}
}
void zest__mtl_set_image_name(zest_device device, zest_image image, const char *name) {
	(void)device;
	zest_mtl_image_backend_t *backend = ZEST_MTL_BACKEND(image, image);
	if (!backend || !name) return;
	@autoreleasepool {
		[backend->texture setLabel:[NSString stringWithUTF8String:name]];
	}
}
// -- End Debug_naming

// -- Debug_functions
void *zest__mtl_get_buffer_ptr(zest_buffer buffer) {
	return (void *)zest__mtl_buffer_object(buffer);
}

void *zest__mtl_get_resource_ptr(zest_resource_node resource) {
	if (ZEST__FLAGGED(resource->type, zest_resource_type_is_image) && resource->image.backend) {
		return (void *)ZEST_MTL_BACKEND(image, &resource->image)->texture;
	} else if (resource->storage_buffer) {
		return (void *)zest__mtl_buffer_object(resource->storage_buffer);
	}
	return NULL;
}

void *zest__mtl_get_final_signal_ptr(zest_submission_batch_t *batch, zest_uint semaphore_index) {
	(void)semaphore_index;
	return (void *)ZEST_MTL_BACKEND(submission_batch, batch)->signal_event;
}

void *zest__mtl_get_swapchain_identifier(zest_swapchain swapchain) {
	return (void *)ZEST_MTL_BACKEND(context, swapchain->context)->layer;
}

//Metal has no binary semaphores, so the frame graph printer gets NULL
void *zest__mtl_get_swapchain_wait_semaphore(zest_swapchain swapchain, zest_uint index) {
	(void)swapchain;
	(void)index;
	return NULL;
}

void *zest__mtl_get_swapchain_signal_semaphore(zest_swapchain swapchain, zest_uint index) {
	(void)swapchain;
	(void)index;
	return NULL;
}

//A batch keeps no list of what it waited on
void *zest__mtl_get_final_wait_ptr(zest_submission_batch_t *batch, zest_uint semaphore_index) {
	(void)batch;
	(void)semaphore_index;
	return NULL;
}
// -- End Debug_functions

// -- Unavailable_stubs
//Every member the backend doesn't serve yet. A member leaves this section when its own section implements it.
//Frame Graph Platform Commands
void *zest__mtl_new_frame_graph_image_backend(zest_device device, zloc_linear_allocator_t *allocator, zest_image image, zest_image imported_image) { zest__mtl_report_unavailable(device, ZEST__MTL_UNAVAILABLE("new_frame_graph_image_backend")); return NULL; }
//Buffer and memory
//Images
zest_bool zest__mtl_create_transient_image_unbound(zest_device device, zest_context context, zest_image image, zest_uint layer_count, zest_sample_count_flags num_samples, zest_image_flags flags, zest_transient_memory_info_t *info) { return zest__mtl_report_unavailable(device, ZEST__MTL_UNAVAILABLE("create_transient_image_unbound")); }
zest_bool zest__mtl_bind_image_arena(zest_device device, zest_image image, zest_device_memory_pool backing, zest_size offset) { return zest__mtl_report_unavailable(device, ZEST__MTL_UNAVAILABLE("bind_image_arena")); }
zest_device_memory_pool zest__mtl_create_arena_backing(zest_device device, zest_context context, zest_uint category, zest_size size) { zest__mtl_report_unavailable(device, ZEST__MTL_UNAVAILABLE("create_arena_backing")); return NULL; }
void zest__mtl_free_arena_backing(zest_device device, zest_context context, zest_device_memory_pool backing) { zest__mtl_report_unavailable(device, ZEST__MTL_UNAVAILABLE("free_arena_backing")); }
//Pipelines
zest_bool zest__mtl_build_pipeline(zest_pipeline pipeline, zest_command_list command_list) { return zest__mtl_report_unavailable(command_list->context->device, ZEST__MTL_UNAVAILABLE("build_pipeline")); }
zest_bool zest__mtl_finish_compute(zest_device device, zest_compute compute) { return zest__mtl_report_unavailable(device, ZEST__MTL_UNAVAILABLE("finish_compute")); }
//Semaphores
//Set layouts
//Command buffers/queues
//General Context
zest_sample_count_flags zest__mtl_get_msaa_sample_count(zest_context context) { zest__mtl_report_unavailable(context->device, ZEST__MTL_UNAVAILABLE("get_msaa_sample_count")); return zest_sample_count_1_bit; }
zest_shader_handle zest__mtl_get_db_overlay_vertex_shader(zest_device device) { zest__mtl_report_unavailable(device, ZEST__MTL_UNAVAILABLE("get_db_overlay_vertex_shader")); return ZEST__ZERO_INIT(zest_shader_handle); }
zest_shader_handle zest__mtl_get_db_overlay_fragment_shader(zest_device device) { zest__mtl_report_unavailable(device, ZEST__MTL_UNAVAILABLE("get_db_overlay_fragment_shader")); return ZEST__ZERO_INIT(zest_shader_handle); }
//Device/OS
//Shader Compiling
zest_bool zest__mtl_validate_shader(zest_device device, const char *shader_code, zest_shader_type type, const char *name) { return zest__mtl_report_unavailable(device, ZEST__MTL_UNAVAILABLE("validate_shader")); }
zest_bool zest__mtl_compile_shader(zest_shader shader, const char *code, zest_uint code_length, zest_shader_type type, const char *name, const char *entry_point, zest_shader_options options) { return zest__mtl_report_unavailable((zest_device)shader->handle.store->origin, ZEST__MTL_UNAVAILABLE("compile_shader")); }
//Create backends
void *zest__mtl_new_pipeline_backend(zest_context context) { zest__mtl_report_unavailable(context->device, ZEST__MTL_UNAVAILABLE("new_pipeline_backend")); return NULL; }
void *zest__mtl_new_compute_backend(zest_device device) { zest__mtl_report_unavailable(device, ZEST__MTL_UNAVAILABLE("new_compute_backend")); return NULL; }
//Cleanup backends
void zest__mtl_cleanup_compute_backend(zest_compute compute) { zest__mtl_report_unavailable((zest_device)compute->handle.store->origin, ZEST__MTL_UNAVAILABLE("cleanup_compute_backend")); }
void zest__mtl_cleanup_pipeline_backend(zest_pipeline pipeline) { zest__mtl_report_unavailable(pipeline->context->device, ZEST__MTL_UNAVAILABLE("cleanup_pipeline_backend")); }
//GPU Profiling
zest_bool zest__mtl_create_gpu_profiler_backend(zest_context context, zest_gpu_profiler_t *profiler) { return zest__mtl_report_unavailable(context->device, ZEST__MTL_UNAVAILABLE("create_gpu_profiler_backend")); }
void zest__mtl_cleanup_gpu_profiler_backend(zest_gpu_profiler_t *profiler) { zest__mtl_report_unavailable(NULL, ZEST__MTL_UNAVAILABLE("cleanup_gpu_profiler_backend")); }
void zest__mtl_reset_gpu_query_pool(zest_gpu_profiler_t *profiler, zest_uint fif) { zest__mtl_report_unavailable(NULL, ZEST__MTL_UNAVAILABLE("reset_gpu_query_pool")); }
void zest__mtl_write_timestamp(const zest_command_list command_list, zest_gpu_profiler_t *profiler, zest_uint fif, zest_uint query_index, zest_bool is_end) { zest__mtl_report_unavailable(command_list->context->device, ZEST__MTL_UNAVAILABLE("write_timestamp")); }
zest_bool zest__mtl_readback_gpu_timestamps(zest_gpu_profiler_t *profiler, zest_uint fif, zest_uint query_count) { return zest__mtl_report_unavailable(NULL, ZEST__MTL_UNAVAILABLE("readback_gpu_timestamps")); }
//Debugging
void zest__mtl_create_test_render_pass(zest_device device, const zest_command_list_t *command_list) {
}
//Command recording
void zest__mtl_blit_image_mip(const zest_command_list command_list, zest_resource_node src, zest_resource_node dst, zest_uint mip_to_blit, zest_supported_shader_stages read_by_stages) { zest__mtl_report_unavailable(command_list->context->device, ZEST__MTL_UNAVAILABLE("blit_image_mip")); }
void zest__mtl_copy_image_mip(const zest_command_list command_list, zest_resource_node src, zest_resource_node dst, zest_uint mip_to_blit, zest_supported_shader_stages read_by_stages) { zest__mtl_report_unavailable(command_list->context->device, ZEST__MTL_UNAVAILABLE("copy_image_mip")); }
void zest__mtl_insert_compute_image_barrier(const zest_command_list command_list, zest_resource_node resource, zest_uint base_mip) { zest__mtl_report_unavailable(command_list->context->device, ZEST__MTL_UNAVAILABLE("insert_compute_image_barrier")); }
void zest__mtl_bind_pipeline(const zest_command_list command_list, zest_pipeline pipeline) { zest__mtl_report_unavailable(command_list->context->device, ZEST__MTL_UNAVAILABLE("bind_pipeline")); }
void zest__mtl_bind_compute_pipeline(const zest_command_list command_list, zest_compute compute) { zest__mtl_report_unavailable(command_list->context->device, ZEST__MTL_UNAVAILABLE("bind_compute_pipeline")); }
void zest__mtl_copy_buffer(const zest_command_list command_list, zest_buffer staging_buffer, zest_buffer device_buffer, zest_size size) { zest__mtl_report_unavailable(command_list->context->device, ZEST__MTL_UNAVAILABLE("copy_buffer")); }
zest_bool zest__mtl_upload_buffer(const zest_command_list command_list, zest_buffer_uploader_t *uploader) { return zest__mtl_report_unavailable(command_list->context->device, ZEST__MTL_UNAVAILABLE("upload_buffer")); }
void zest__mtl_bind_vertex_buffer(const zest_command_list command_list, zest_uint first_binding, zest_uint binding_count, zest_buffer buffer) { zest__mtl_report_unavailable(command_list->context->device, ZEST__MTL_UNAVAILABLE("bind_vertex_buffer")); }
void zest__mtl_bind_index_buffer(const zest_command_list command_list, zest_buffer buffer) { zest__mtl_report_unavailable(command_list->context->device, ZEST__MTL_UNAVAILABLE("bind_index_buffer")); }
zest_bool zest__mtl_image_clear(const zest_command_list command_list, zest_image image) { return zest__mtl_report_unavailable(command_list->context->device, ZEST__MTL_UNAVAILABLE("image_clear")); }
void zest__mtl_bind_mesh_vertex_buffer(const zest_command_list command_list, zest_layer layer) { zest__mtl_report_unavailable(command_list->context->device, ZEST__MTL_UNAVAILABLE("bind_mesh_vertex_buffer")); }
void zest__mtl_bind_mesh_index_buffer(const zest_command_list command_list, zest_layer layer) { zest__mtl_report_unavailable(command_list->context->device, ZEST__MTL_UNAVAILABLE("bind_mesh_index_buffer")); }
void zest__mtl_dispatch_compute(const zest_command_list command_list, zest_uint group_count_x, zest_uint group_count_y, zest_uint group_count_z) { zest__mtl_report_unavailable(command_list->context->device, ZEST__MTL_UNAVAILABLE("dispatch_compute")); }
void zest__mtl_send_push_constants(const zest_command_list command_list, zest_pipeline_layout layout, void *data, zest_uint size) { zest__mtl_report_unavailable(command_list->context->device, ZEST__MTL_UNAVAILABLE("send_push_constants")); }
void zest__mtl_draw(const zest_command_list command_list, zest_uint vertex_count, zest_uint instance_count, zest_uint first_vertex, zest_uint first_instance) { zest__mtl_report_unavailable(command_list->context->device, ZEST__MTL_UNAVAILABLE("draw")); }
void zest__mtl_draw_layer_instruction(const zest_command_list command_list, zest_uint vertex_count, zest_layer_instruction_t *instruction) { zest__mtl_report_unavailable(command_list->context->device, ZEST__MTL_UNAVAILABLE("draw_layer_instruction")); }
void zest__mtl_draw_indexed(const zest_command_list command_list, zest_uint index_count, zest_uint instance_count, zest_uint first_index, int32_t vertex_offset, zest_uint first_instance) { zest__mtl_report_unavailable(command_list->context->device, ZEST__MTL_UNAVAILABLE("draw_indexed")); }
void zest__mtl_draw_indexed_indirect(const zest_command_list command_list, zest_buffer buffer, zest_size offset, zest_uint draw_count, zest_uint stride) { zest__mtl_report_unavailable(command_list->context->device, ZEST__MTL_UNAVAILABLE("draw_indexed_indirect")); }
void zest__mtl_draw_indirect(const zest_command_list command_list, zest_buffer buffer, zest_size offset, zest_uint draw_count, zest_uint stride) { zest__mtl_report_unavailable(command_list->context->device, ZEST__MTL_UNAVAILABLE("draw_indirect")); }
void zest__mtl_set_depth_bias(const zest_command_list command_list, float factor, float clamp, float slope) { zest__mtl_report_unavailable(command_list->context->device, ZEST__MTL_UNAVAILABLE("set_depth_bias")); }
//Immediate commands
void zest__mtl_imm_fill_buffer(zest_queue queue, zest_buffer buffer, zest_uint value) { zest__mtl_report_unavailable(queue->device, ZEST__MTL_UNAVAILABLE("imm_fill_buffer")); }
void zest__mtl_imm_update_buffer(zest_queue queue, zest_buffer buffer, void *data, zest_size intended_size) { zest__mtl_report_unavailable(queue->device, ZEST__MTL_UNAVAILABLE("imm_update_buffer")); }
zest_bool zest__mtl_imm_clear_color_image(zest_queue queue, zest_image image, zest_clear_value_t clear_value) { return zest__mtl_report_unavailable(queue->device, ZEST__MTL_UNAVAILABLE("imm_clear_color_image")); }
zest_bool zest__mtl_imm_clear_depth_stencil_image(zest_queue queue, zest_image image, float depth, zest_uint stencil) { return zest__mtl_report_unavailable(queue->device, ZEST__MTL_UNAVAILABLE("imm_clear_depth_stencil_image")); }
zest_bool zest__mtl_imm_blit_image(zest_queue queue, zest_image src_image, zest_image dst_image, int src_x, int src_y, int src_width, int src_height, int dst_x, int dst_y, int dst_width, int dst_height, zest_filter_type filter) { return zest__mtl_report_unavailable(queue->device, ZEST__MTL_UNAVAILABLE("imm_blit_image")); }
zest_bool zest__mtl_imm_resolve_image(zest_queue queue, zest_image src_image, zest_image dst_image) { return zest__mtl_report_unavailable(queue->device, ZEST__MTL_UNAVAILABLE("imm_resolve_image")); }
void zest__mtl_imm_send_push_constants(zest_queue queue, void *data, zest_uint size) { zest__mtl_report_unavailable(queue->device, ZEST__MTL_UNAVAILABLE("imm_send_push_constants")); }
void zest__mtl_imm_bind_compute_pipeline(zest_queue queue, zest_compute compute) { zest__mtl_report_unavailable(queue->device, ZEST__MTL_UNAVAILABLE("imm_bind_compute_pipeline")); }
zest_bool zest__mtl_imm_dispatch_compute(zest_queue queue, zest_uint group_count_x, zest_uint group_count_y, zest_uint group_count_z) { return zest__mtl_report_unavailable(queue->device, ZEST__MTL_UNAVAILABLE("imm_dispatch_compute")); }
// -- End Unavailable_stubs

#ifdef __cplusplus
}
#endif

#endif // ! ZEST_METAL_H
