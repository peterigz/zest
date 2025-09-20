#ifndef ZEST_VULKAN_H
#define ZEST_VULKAN_H

#include "zest.h"
#include <vulkan/vulkan.h>
#include "vulkan/vulkan_win32.h"

#ifdef __cplusplus
extern "C" {
#endif

ZEST_API VkInstance zest_GetVKInstance();
ZEST_API VkAllocationCallbacks *zest_GetVKAllocationCallbacks();
ZEST_API void zest_vk_SetWindowSurface(VkSurfaceKHR surface);

ZEST_GLOBAL const char* zest_validation_layers[zest__validation_layer_count] = {
	"VK_LAYER_KHRONOS_validation"
};

ZEST_GLOBAL const char* zest_required_extensions[zest__required_extension_names_count] = {
	VK_KHR_SWAPCHAIN_EXTENSION_NAME,
	#if defined(__APPLE__)
	"VK_KHR_portability_subset"
	#endif
};

//For error checking vulkan commands
#define ZEST_VK_ASSERT_RESULT(res) do {                                                                                \
ZestRenderer->backend->last_result = (res);                                                                                 \
if (ZestRenderer->backend->last_result != VK_SUCCESS) {                                                                     \
zest__log_vulkan_error(ZestRenderer->backend->last_result, __FILE__, __LINE__);                                         \
return ZestRenderer->backend->last_result;                                                                              \
}                                                                                                                  \
} while(0)

#define ZEST_RETURN_FALSE_ON_FAIL(res)                                                                                 \
ZestRenderer->backend->last_result = res;                                                                                   \
if (ZestRenderer->backend->last_result != VK_SUCCESS) {                                                                     \
return ZEST_FALSE;                                                                                             \
}

#define ZEST_CLEANUP_ON_FAIL(res) do {                                                                                 \
ZestRenderer->backend->last_result = (res);                                                                                 \
if (ZestRenderer->backend->last_result != VK_SUCCESS) {                                                                     \
goto cleanup;                                                                                                  \
}                                                                                                                  \
} while(0)

#define ZEST_VK_LOG(res) do {                                                                                          \
ZestRenderer->backend->last_result = (res);                                                                                 \
if (ZestRenderer->backend->last_result != VK_SUCCESS) {                                                                     \
zest__log_vulkan_error(ZestRenderer->backend->last_result, __FILE__, __LINE__);                                         \
}                                                                                                                  \
} while(0)

#define ZEST_VK_PRINT_RESULT(result) do {                                                                              \
ZestRenderer->backend->last_result = (result);                                                                     \
if (result != VK_SUCCESS) {                                                                                        \
zest__log_vulkan_error(result, __FILE__, __LINE__);                                                            \
}                                                                                                                  \
} while(0)

ZEST_PRIVATE void zest__vk_initialise_platform_callbacks(void);

ZEST_PRIVATE void zest__log_vulkan_error(VkResult result, const char *file, int line);
ZEST_PRIVATE const char *zest__vulkan_error_string(VkResult errorCode);

// --Frame_graph_platform_functions
ZEST_PRIVATE zest_bool zest__vk_begin_command_buffer(const zest_frame_graph_context context);
ZEST_PRIVATE zest_bool zest__vk_set_next_command_buffer(zest_frame_graph_context context, zest_queue queue);
ZEST_PRIVATE void zest__vk_acquire_barrier(zest_frame_graph_context context, zest_execution_details_t *exe_details);
ZEST_PRIVATE void zest__vk_release_barrier(zest_frame_graph_context context, zest_execution_details_t *exe_details);
ZEST_PRIVATE void* zest__vk_new_execution_backend(zloc_linear_allocator_t *allocator);
ZEST_PRIVATE void zest__vk_set_execution_fence(zest_execution_backend backend, zest_bool is_intraframe);
ZEST_PRIVATE zest_frame_graph_semaphores zest__vk_get_frame_graph_semaphores(const char *name);
ZEST_PRIVATE zest_bool zest__vk_submit_frame_graph_batch(zest_frame_graph frame_graph, zest_execution_backend backend, zest_submission_batch_t *batch, zest_map_queue_value *queues);
ZEST_PRIVATE zest_frame_graph_result zest__vk_create_fg_render_pass(zest_pass_group_t *pass, zest_execution_details_t *exe_details, zest_uint current_pass_index);
ZEST_PRIVATE zest_bool zest__vk_begin_render_pass(const zest_frame_graph_context context, zest_execution_details_t *exe_details);
ZEST_PRIVATE void zest__vk_end_render_pass(const zest_frame_graph_context context);
ZEST_PRIVATE void zest__vk_end_command_buffer(const zest_frame_graph_context context);
ZEST_PRIVATE void zest__vk_carry_over_semaphores(zest_frame_graph frame_graph, zest_wave_submission_t *wave_submission, zest_execution_backend backend);
ZEST_PRIVATE zest_bool zest__vk_frame_graph_fence_wait(zest_execution_backend backend);
ZEST_PRIVATE void zest__vk_deferr_framebuffer_destruction(void *framebuffer);
ZEST_PRIVATE void zest__vk_cleanup_deferred_framebuffers(void);
ZEST_PRIVATE zest_bool zest__vk_present_frame(zest_swapchain swapchain);
ZEST_PRIVATE zest_bool zest__vk_dummy_submit_for_present_only(void);
ZEST_PRIVATE zest_bool zest__vk_acquire_swapchain_image(zest_swapchain swapchain);
// --End_Frame_graph_platform_functions

// --Device set up
ZEST_PRIVATE VKAPI_ATTR VkBool32 VKAPI_CALL zest__vk_debug_callback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageType, const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData);
ZEST_PRIVATE VkResult zest__vk_create_debug_messenger(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pDebugMessenger);

// Helper functions to convert enums to strings 
ZEST_PRIVATE const char *zest__vk_image_layout_to_string(VkImageLayout layout);
ZEST_PRIVATE zest_text_t zest__vk_access_flags_to_string(VkAccessFlags flags);
ZEST_PRIVATE zest_text_t zest__vk_pipeline_stage_flags_to_string(VkPipelineStageFlags flags);

//Buffers and memory
ZEST_PRIVATE zest_bool zest__vk_create_buffer_memory_pool(zest_size size, zest_buffer_info_t *buffer_info, zest_device_memory_pool memory_pool, const char *name);
ZEST_PRIVATE zest_bool zest__vk_create_image_memory_pool(zest_size size_in_bytes, zest_buffer_info_t *buffer_info, zest_device_memory_pool buffer);
ZEST_PRIVATE zest_bool zest__vk_map_memory(zest_device_memory_pool memory_allocation, zest_size size, zest_size offset);
ZEST_PRIVATE void zest__vk_unmap_memory(zest_device_memory_pool memory_allocation);
ZEST_PRIVATE void zest__vk_set_buffer_backend_details(zest_buffer buffer);
ZEST_PRIVATE void zest__vk_flush_used_buffers(void);
ZEST_PRIVATE void zest__vk_cmd_copy_buffer_one_time(zest_buffer src_buffer, zest_buffer dst_buffer, zest_size size);
ZEST_PRIVATE void zest__vk_push_buffer_for_freeing(zest_buffer buffer);
ZEST_PRIVATE zest_access_flags zest__vk_get_buffer_last_access_mask(zest_buffer buffer);

//Descriptor Sets
ZEST_PRIVATE zest_bool zest__vk_create_uniform_descriptor_set(zest_uniform_buffer buffer, zest_set_layout associated_layout);

//Pipelines
ZEST_PRIVATE zest_bool zest__vk_build_pipeline(zest_pipeline pipeline, zest_render_pass render_pass);

//Set layouts
ZEST_PRIVATE zest_bool zest__vk_create_set_layout(zest_set_layout_builder_t *builder, zest_set_layout layout, zest_bool is_bindless);
ZEST_PRIVATE zest_bool zest__vk_create_set_pool(zest_set_layout layout, zest_uint max_set_count, zest_bool bindless);
ZEST_PRIVATE zest_descriptor_set zest__vk_create_bindless_set(zest_set_layout layout);
ZEST_PRIVATE void zest__vk_update_bindless_image_descriptor(zest_uint binding_number, zest_uint array_index, zest_descriptor_type type, zest_image image, zest_image_view view, zest_sampler sampler, zest_descriptor_set set);
ZEST_PRIVATE void zest__vk_update_bindless_buffer_descriptor(zest_uint binding_number, zest_uint array_index, zest_buffer buffer, zest_descriptor_set set);

//General renderer
ZEST_PRIVATE void zest__vk_set_depth_format(void);
ZEST_PRIVATE zest_bool zest__vk_initialise_renderer_backend(void);
ZEST_PRIVATE void zest__vk_wait_for_idle_device(void);
ZEST_PRIVATE zest_sample_count_flags zest__vk_get_msaa_sample_count(void);

//Allocation callbacks
ZEST_PRIVATE void *zest__vk_allocate_callback(void* pUserData, size_t size, size_t alignment, VkSystemAllocationScope allocationScope);
ZEST_PRIVATE void *zest__vk_reallocate_callback(void* pUserData, void *memory, size_t size, size_t alignment, VkSystemAllocationScope allocationScope);
ZEST_PRIVATE void zest__vk_free_callback(void* pUserData, void *memory);

//Device/OS
ZEST_PRIVATE void zest__vk_os_add_platform_extensions(void);

//Fences
ZEST_PRIVATE zest_fence_status zest__vk_wait_for_renderer_fences(void);
ZEST_PRIVATE zest_bool zest__vk_reset_renderer_fences(void);

//Command buffers/queues
ZEST_PRIVATE void zest__vk_reset_queue_command_pool(zest_queue queue);

//New backends
ZEST_PRIVATE void *zest__vk_new_device_backend(void);
ZEST_PRIVATE void *zest__vk_new_renderer_backend(void);
ZEST_PRIVATE void *zest__vk_new_frame_graph_context_backend(void);
ZEST_PRIVATE void *zest__vk_new_swapchain_backend(void);
ZEST_PRIVATE void *zest__vk_new_buffer_backend(void);
ZEST_PRIVATE void *zest__vk_new_uniform_buffer_backend(void);
ZEST_PRIVATE void zest__vk_set_uniform_buffer_backend(zest_uniform_buffer buffer);
ZEST_PRIVATE void *zest__vk_new_compute_backend(void);
ZEST_PRIVATE void *zest__vk_new_image_backend(void);
ZEST_PRIVATE void *zest__vk_new_frame_graph_image_backend(zloc_linear_allocator_t *allocator, zest_image node_image, zest_image imported_image);
ZEST_PRIVATE void *zest__vk_new_set_layout_backend(void);
ZEST_PRIVATE void *zest__vk_new_descriptor_pool_backend(void);
ZEST_PRIVATE void *zest__vk_new_sampler_backend(void);
ZEST_PRIVATE void *zest__vk_new_queue_backend(void);
ZEST_PRIVATE void *zest__vk_new_submission_batch_backend(void);
ZEST_PRIVATE void *zest__vk_new_frame_graph_semaphores_backend(void);
ZEST_PRIVATE void *zest__vk_new_deferred_destruction_backend(void);
ZEST_PRIVATE void *zest__vk_new_pipeline_backend(void);
ZEST_PRIVATE void *zest__vk_new_memory_pool_backend(void);
ZEST_PRIVATE void *zest__vk_new_shader_resources_backend(void);
ZEST_PRIVATE void *zest__vk_new_window_backend(void);

ZEST_PRIVATE zest_bool zest__vk_finish_compute(zest_compute_builder_t *builder, zest_compute compute);

//Cleanup backends
ZEST_PRIVATE void zest__vk_cleanup_swapchain_backend(zest_swapchain swapchain, zest_bool for_recreation);
ZEST_PRIVATE void zest__vk_cleanup_window_backend(zest_window window);
ZEST_PRIVATE void zest__vk_cleanup_buffer_backend(zest_buffer buffer);
ZEST_PRIVATE void zest__vk_cleanup_uniform_buffer_backend(zest_uniform_buffer buffer);
ZEST_PRIVATE void zest__vk_cleanup_compute_backend(zest_compute compute);
ZEST_PRIVATE void zest__vk_cleanup_set_layout_backend(zest_set_layout layout);
ZEST_PRIVATE void zest__vk_cleanup_pipeline_backend(zest_pipeline pipeline);
ZEST_PRIVATE void zest__vk_cleanup_image_backend(zest_image image);
ZEST_PRIVATE void zest__vk_cleanup_sampler_backend(zest_sampler sampler);
ZEST_PRIVATE void zest__vk_cleanup_queue_backend(zest_queue queue);
ZEST_PRIVATE void zest__vk_cleanup_image_view_backend(zest_image_view view);
ZEST_PRIVATE void zest__vk_cleanup_image_view_array_backend(zest_image_view_array view);
ZEST_PRIVATE void zest__vk_cleanup_descriptor_backend(zest_set_layout layout, zest_descriptor_set set);
ZEST_PRIVATE void zest__vk_cleanup_shader_resources_backend(zest_shader_resources shader_resource);
ZEST_PRIVATE void zest__vk_cleanup_deferred_destruction_backend(void);
ZEST_PRIVATE void zest__vk_cleanup_memory_pool_backend(zest_device_memory_pool memory_allocation);
ZEST_PRIVATE void zest__vk_cleanup_device_backend(void);
ZEST_PRIVATE void zest__vk_cleanup_renderer_backend(void);

ZEST_PRIVATE zest_bool zest__vk_create_window_surface(zest_window window);
ZEST_PRIVATE zest_bool zest__vk_initialise_device(void);
ZEST_PRIVATE zest_bool zest__vk_initialise_swapchain(zest_swapchain swapchain, zest_window window);
ZEST_PRIVATE zest_bool zest__vk_create_instance();
ZEST_PRIVATE zest_bool zest__vk_create_logical_device();
ZEST_PRIVATE void zest__vk_set_limit_data(void);
ZEST_PRIVATE void zest__vk_setup_validation();
ZEST_PRIVATE void zest__vk_pick_physical_device(void);
ZEST_PRIVATE zest_bool zest__vk_create_image(zest_image image, zest_uint layer_count, zest_sample_count_flags num_samples, zest_image_flags flags);
ZEST_PRIVATE zest_image_view_t *zest__vk_create_image_view(zest_image image, zest_image_view_type view_type, zest_uint mip_levels_this_view, zest_uint base_mip, zest_uint base_array_index, zest_uint layer_count, zloc_linear_allocator_t *allocator);
ZEST_PRIVATE zest_image_view_array_t *zest__vk_create_image_views_per_mip(zest_image image, zest_image_view_type view_type, zest_uint base_array_index, zest_uint layer_count, zloc_linear_allocator_t *allocator);
ZEST_PRIVATE zest_bool zest__vk_copy_buffer_regions_to_image(zest_buffer_image_copy_t *regions, zest_buffer buffer, zest_size src_offset, zest_image image);
ZEST_PRIVATE zest_bool zest__vk_transition_image_layout(zest_image image, zest_image_layout new_layout, zest_uint base_mip_index, zest_uint mip_levels, zest_uint base_array_index, zest_uint layer_count);
ZEST_PRIVATE zest_bool zest__vk_create_sampler(zest_sampler sampler);
ZEST_PRIVATE int zest__vk_get_image_raw_layout(zest_image image);
ZEST_PRIVATE zest_bool zest__vk_copy_buffer_to_image(zest_buffer buffer, zest_size src_offset, zest_image image, zest_uint width, zest_uint height);
ZEST_PRIVATE zest_bool zest__vk_generate_mipmaps(zest_image image);
ZEST_PRIVATE zest_bool zest__vk_begin_single_time_commands(void);
ZEST_PRIVATE zest_bool zest__vk_end_single_time_commands(void);
ZEST_PRIVATE zest_bool zest__vk_create_execution_timeline_backend(zest_execution_timeline timeline);
ZEST_PRIVATE void zest__vk_add_image_barrier(zest_resource_node resource, zest_execution_barriers_t *barriers, zest_bool acquire, 
				zest_access_flags src_access, zest_access_flags dst_access, zest_image_layout old_layout, zest_image_layout new_layout, 
				zest_uint src_family, zest_uint dst_family, zest_pipeline_stage_flags src_stage, zest_pipeline_stage_flags dst_stage);
ZEST_PRIVATE void zest__vk_add_memory_buffer_barrier(zest_resource_node resource, zest_execution_barriers_t *barriers, zest_bool acquire, zest_access_flags src_access, zest_access_flags dst_access, 
				zest_uint src_family, zest_uint dst_family, zest_pipeline_stage_flags src_stage, zest_pipeline_stage_flags dst_stage);
ZEST_PRIVATE void zest__vk_validate_barrier_pipeline_stages(zest_execution_barriers_t *barriers);
ZEST_PRIVATE void* zest__vk_new_execution_barriers_backend(zloc_linear_allocator_t *allocator);
ZEST_PRIVATE void zest__vk_cleanup_frame_graph_semaphore(zest_frame_graph_semaphores semaphores);
ZEST_PRIVATE void zest__vk_cleanup_render_pass(zest_render_pass render_pass);
ZEST_PRIVATE void zest__vk_print_compiled_frame_graph(zest_frame_graph frame_graph);

ZEST_API void zest_UseVulkan();

#ifdef ZEST_VULKAN_IMPLEMENTATION
/*
Zest Vulkan Implementation

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
    -- [Descriptor_sets]
    -- [General_renderer]
	-- [Device_OS]
    -- [Pipelines]
    -- [Images]
    -- [General_helpers]
	-- [Allocation_callbacks]
    -- [Internal_Frame_graph_context_functions]
    -- [Frame_graph_context_functions]
    -- [Debug_functions]
*/

// -- Backend_structs
typedef struct zest_queue_backend_t {
    VkQueue vk_queue;
    VkSemaphore semaphore[ZEST_MAX_FIF];
    VkCommandPool command_pool[ZEST_MAX_FIF];
    VkCommandBuffer *command_buffers[ZEST_MAX_FIF];
} zest_queue_backend_t;

typedef struct zest_execution_backend_t {
	VkSemaphore batch_semaphore;
	zest_size *batch_value;
    VkSemaphore *wave_wait_semaphores;
    zest_size *wave_wait_values;
    zest_uint intraframe_fence_count;
    VkFence *fence;
    zest_uint *fence_count;
    VkFence submit_fence;
    VkSemaphore *wait_semaphores;
    VkPipelineStageFlags *wait_stages;
    VkSemaphore *signal_semaphores;
    zest_u64 *wait_values;
    zest_u64 *signal_values;
} zest_execution_backend_t;

typedef struct zest_execution_barriers_backend_t {
    VkImageMemoryBarrier *acquire_image_barriers;
    VkBufferMemoryBarrier *acquire_buffer_barriers;
    VkImageMemoryBarrier *release_image_barriers;
    VkBufferMemoryBarrier *release_buffer_barriers;
    VkPipelineStageFlags overall_src_stage_mask_for_acquire_barriers;
    VkPipelineStageFlags overall_dst_stage_mask_for_acquire_barriers;
    VkPipelineStageFlags overall_src_stage_mask_for_release_barriers;
    VkPipelineStageFlags overall_dst_stage_mask_for_release_barriers;
} zest_execution_barriers_backend_t;

typedef struct zest_window_backend_t {
    VkSurfaceKHR surface;
} zest_window_backend_t;

typedef struct zest_device_memory_pool_backend_t {
    VkBufferCreateInfo buffer_info;
    VkBuffer vk_buffer;
    VkDeviceMemory memory;
    VkImageUsageFlags image_usage_flags;
    VkMemoryPropertyFlags property_flags;
} zest_device_memory_pool_backend_t;

typedef struct zest_submission_batch_backend_t {
    VkSemaphore *final_wait_semaphores;
    VkSemaphore *final_signal_semaphores;
} zest_submission_batch_backend_t;

typedef struct zest_frame_graph_semaphores_backend_t {
    VkSemaphore vk_semaphores[ZEST_MAX_FIF][ZEST_QUEUE_COUNT];
} zest_frame_graph_semaphores_backend_t;

typedef struct zest_execution_timeline_backend_t {
    VkSemaphore semaphore;
} zest_execution_timeline_backend_t;

typedef struct zest_device_backend_t {
    VkAllocationCallbacks allocation_callbacks;
    VkInstance instance;
    VkPhysicalDevice physical_device;
    VkDevice logical_device;
    VkDebugUtilsMessengerEXT debug_messenger;
    VkSampleCountFlagBits msaa_samples;
    VkPhysicalDeviceProperties properties;
    VkPhysicalDeviceFeatures features;
    VkPhysicalDeviceMemoryProperties memory_properties;
    VkQueueFamilyProperties *queue_families;
    VkCommandPool one_time_command_pool[ZEST_MAX_FIF];
    PFN_vkSetDebugUtilsObjectNameEXT pfnSetDebugUtilsObjectNameEXT;
    VkFormat color_format;
} zest_device_backend_t;

typedef struct zest_swapchain_backend_t {
    VkSwapchainKHR vk_swapchain;
    VkSemaphore *vk_render_finished_semaphore;
    VkSemaphore vk_image_available_semaphore[ZEST_MAX_FIF];
} zest_swapchain_backend_t;

typedef struct zest_render_pass_backend_t {
    VkRenderPass vk_render_pass;
    VkSampleCountFlags sample_count;
} zest_render_pass_backend_t;

typedef struct zest_deferred_destruction_backend_t {
	VkFramebuffer *frame_buffers[ZEST_MAX_FIF];             
} zest_deferred_destruction_backend_t;

typedef struct zest_renderer_backend_t {
    VkPipelineCache pipeline_cache;
    VkBuffer *used_buffers_ready_for_freeing;
    VkCommandBuffer utility_command_buffer[ZEST_MAX_FIF];
    VkCommandBuffer one_time_command_buffer;
    VkFence fif_fence[ZEST_MAX_FIF][ZEST_QUEUE_COUNT];
    VkFence intraframe_fence[ZEST_QUEUE_COUNT];
    VkResult last_result;
} zest_renderer_backend_t;

typedef struct zest_frame_graph_context_backend_t {
    VkCommandBuffer command_buffer;
} zest_frame_graph_context_backend_t;

typedef struct zest_buffer_backend_t {
    int magic;
    union {
        VkBuffer vk_buffer;
        VkImage vk_image;
    };
    VkPipelineStageFlags last_stage_mask;
    VkAccessFlags last_access_mask;
} zest_buffer_backend_t;

typedef struct zest_uniform_buffer_backend_t {
    VkDescriptorBufferInfo descriptor_info[ZEST_MAX_FIF];
} zest_uniform_buffer_backend_t;

typedef struct zest_shader_resources_backend_t {
    VkDescriptorSet *binding_sets;
} zest_shader_resources_backend_t;

typedef struct zest_descriptor_pool_backend_t {
    VkDescriptorPool vk_descriptor_pool;
    VkDescriptorPoolSize *vk_pool_sizes;
} zest_descriptor_pool_backend_t;

typedef struct zest_descriptor_set_backend_t {
    VkDescriptorSet vk_descriptor_set;
} zest_descriptor_set_backend_t;

typedef struct zest_set_layout_backend_t {
    VkDescriptorSetLayoutBinding *layout_bindings;
    VkDescriptorSetLayout vk_layout;
    VkDescriptorSetLayoutCreateFlags create_flags;
} zest_set_layout_backend_t;

typedef struct zest_pipeline_backend_t {
    VkPipeline pipeline;                                                         //The vulkan handle for the pipeline
    VkPipelineLayout pipeline_layout;                                            //The vulkan handle for the pipeline layout
} zest_pipeline_backend_t;

typedef struct zest_compute_backend_t {
    VkPipelineLayout pipeline_layout;                         // Layout of the compute pipeline
    VkPipeline pipeline;                                      // Compute pipeline
    VkPushConstantRange push_constant_range;
} zest_compute_backend_t;

typedef struct zest_image_backend_t {
	VkImage vk_image;
	VkImageLayout vk_current_layout; // The actual, current layout of the image on the GPU
    VkImageAspectFlags vk_aspect;
	VkFormat vk_format;
	VkExtent3D vk_extent;
} zest_image_backend_t;

typedef struct zest_image_view_backend_t {
	VkImageView vk_view;             // Default view of the entire image
} zest_image_view_backend_t;

typedef struct zest_sampler_backend_t {
	VkSampler vk_sampler;             // Default view of the entire image
} zest_sampler_backend_t;

// -- Other_setup_structs
typedef struct zest_swapchain_support_details_t {
    VkSurfaceCapabilitiesKHR capabilities;
    VkSurfaceFormatKHR *formats;
    zest_uint formats_count;
    VkPresentModeKHR *present_modes;
    zest_uint present_modes_count;
} zest_swapchain_support_details_t;

void zest__vk_initialise_platform_callbacks(void) {
    //Frame Graph Related
    ZestPlatform.begin_command_buffer                      = zest__vk_begin_command_buffer;
    ZestPlatform.end_command_buffer                        = zest__vk_end_command_buffer;
    ZestPlatform.set_next_command_buffer                   = zest__vk_set_next_command_buffer;
    ZestPlatform.set_execution_fence                       = zest__vk_set_execution_fence;
    ZestPlatform.acquire_barrier                           = zest__vk_acquire_barrier;
    ZestPlatform.release_barrier                           = zest__vk_release_barrier;
    ZestPlatform.get_frame_graph_semaphores                = zest__vk_get_frame_graph_semaphores;
    ZestPlatform.submit_frame_graph_batch                  = zest__vk_submit_frame_graph_batch;
    ZestPlatform.create_fg_render_pass                     = zest__vk_create_fg_render_pass;
    ZestPlatform.begin_render_pass                         = zest__vk_begin_render_pass;
    ZestPlatform.end_render_pass                           = zest__vk_end_render_pass;
    ZestPlatform.carry_over_semaphores                     = zest__vk_carry_over_semaphores;
    ZestPlatform.frame_graph_fence_wait                    = zest__vk_frame_graph_fence_wait;
    ZestPlatform.create_execution_timeline_backend         = zest__vk_create_execution_timeline_backend;
    ZestPlatform.new_execution_barriers_backend            = zest__vk_new_execution_barriers_backend;
    ZestPlatform.add_frame_graph_buffer_barrier            = zest__vk_add_memory_buffer_barrier;
    ZestPlatform.add_frame_graph_image_barrier             = zest__vk_add_image_barrier;
    ZestPlatform.validate_barrier_pipeline_stages          = zest__vk_validate_barrier_pipeline_stages;
    ZestPlatform.print_compiled_frame_graph                = zest__vk_print_compiled_frame_graph;
    ZestPlatform.deferr_framebuffer_destruction            = zest__vk_deferr_framebuffer_destruction;
    ZestPlatform.present_frame                             = zest__vk_present_frame;
    ZestPlatform.dummy_submit_for_present_only             = zest__vk_dummy_submit_for_present_only;
    ZestPlatform.acquire_swapchain_image                   = zest__vk_acquire_swapchain_image;
    ZestPlatform.new_frame_graph_image_backend             = zest__vk_new_frame_graph_image_backend;

    ZestPlatform.create_buffer_memory_pool                 = zest__vk_create_buffer_memory_pool;
    ZestPlatform.create_image_memory_pool                  = zest__vk_create_image_memory_pool;
    ZestPlatform.map_memory                                = zest__vk_map_memory;
    ZestPlatform.unmap_memory                              = zest__vk_unmap_memory;
    ZestPlatform.set_buffer_backend_details                = zest__vk_set_buffer_backend_details;
    ZestPlatform.flush_used_buffers                        = zest__vk_flush_used_buffers;
    ZestPlatform.cmd_copy_buffer_one_time                  = zest__vk_cmd_copy_buffer_one_time;
    ZestPlatform.push_buffer_for_freeing                   = zest__vk_push_buffer_for_freeing;
    ZestPlatform.get_buffer_last_access_mask               = zest__vk_get_buffer_last_access_mask;

	ZestPlatform.create_image 								= zest__vk_create_image;
	ZestPlatform.create_image_view         				= zest__vk_create_image_view;
	ZestPlatform.create_image_views_per_mip		 		= zest__vk_create_image_views_per_mip;
	ZestPlatform.copy_buffer_regions_to_image		 		= zest__vk_copy_buffer_regions_to_image;
	ZestPlatform.transition_image_layout			 		= zest__vk_transition_image_layout;
	ZestPlatform.create_sampler		 					= zest__vk_create_sampler;
	ZestPlatform.get_image_raw_layout		 				= zest__vk_get_image_raw_layout;
	ZestPlatform.copy_buffer_to_image		 				= zest__vk_copy_buffer_to_image;
	ZestPlatform.generate_mipmaps		 					= zest__vk_generate_mipmaps;

    ZestPlatform.create_uniform_descriptor_set             = zest__vk_create_uniform_descriptor_set;

    ZestPlatform.build_pipeline                            = zest__vk_build_pipeline;
    ZestPlatform.finish_compute                            = zest__vk_finish_compute;

	ZestPlatform.wait_for_renderer_fences 					= zest__vk_wait_for_renderer_fences;
	ZestPlatform.reset_renderer_fences 				   	= zest__vk_reset_renderer_fences;

    ZestPlatform.create_set_layout                         = zest__vk_create_set_layout;
    ZestPlatform.create_set_pool                           = zest__vk_create_set_pool;
    ZestPlatform.create_bindless_set                       = zest__vk_create_bindless_set;
    ZestPlatform.update_bindless_image_descriptor          = zest__vk_update_bindless_image_descriptor;
    ZestPlatform.update_bindless_buffer_descriptor         = zest__vk_update_bindless_buffer_descriptor;

    ZestPlatform.set_depth_format                          = zest__vk_set_depth_format;
    ZestPlatform.initialise_renderer_backend               = zest__vk_initialise_renderer_backend;
    ZestPlatform.get_msaa_sample_count						= zest__vk_get_msaa_sample_count;
    ZestPlatform.initialise_swapchain						= zest__vk_initialise_swapchain;

	ZestPlatform.wait_for_idle_device                      = zest__vk_wait_for_idle_device;
	ZestPlatform.initialise_device                      	= zest__vk_initialise_device;
	ZestPlatform.os_add_platform_extensions 			    = zest__vk_os_add_platform_extensions;
	ZestPlatform.create_window_surface 				    = zest__vk_create_window_surface;

	ZestPlatform.reset_queue_command_pool 					= zest__vk_reset_queue_command_pool;
	ZestPlatform.begin_single_time_commands 				= zest__vk_begin_single_time_commands;
	ZestPlatform.end_single_time_commands 					= zest__vk_end_single_time_commands;

    ZestPlatform.new_execution_backend                     = zest__vk_new_execution_backend;
    ZestPlatform.new_frame_graph_semaphores_backend        = zest__vk_new_frame_graph_semaphores_backend;
    ZestPlatform.new_deferred_desctruction_backend         = zest__vk_new_deferred_destruction_backend;
    ZestPlatform.new_pipeline_backend                      = zest__vk_new_pipeline_backend;
    ZestPlatform.new_memory_pool_backend                   = zest__vk_new_memory_pool_backend;
	ZestPlatform.new_device_backend                        = zest__vk_new_device_backend;
	ZestPlatform.new_renderer_backend                      = zest__vk_new_renderer_backend;
	ZestPlatform.new_frame_graph_context_backend           = zest__vk_new_frame_graph_context_backend;
	ZestPlatform.new_swapchain_backend                     = zest__vk_new_swapchain_backend;
	ZestPlatform.new_buffer_backend                        = zest__vk_new_buffer_backend;
	ZestPlatform.new_uniform_buffer_backend                = zest__vk_new_uniform_buffer_backend;
	ZestPlatform.set_uniform_buffer_backend                = zest__vk_set_uniform_buffer_backend;
	ZestPlatform.new_image_backend                         = zest__vk_new_image_backend;
	ZestPlatform.new_compute_backend                       = zest__vk_new_compute_backend;
	ZestPlatform.new_queue_backend                         = zest__vk_new_queue_backend;
	ZestPlatform.new_submission_batch_backend              = zest__vk_new_submission_batch_backend;
	ZestPlatform.new_set_layout_backend                    = zest__vk_new_set_layout_backend;
	ZestPlatform.new_descriptor_pool_backend               = zest__vk_new_descriptor_pool_backend;
	ZestPlatform.new_sampler_backend                       = zest__vk_new_sampler_backend;
	ZestPlatform.new_shader_resources_backend              = zest__vk_new_shader_resources_backend;
	ZestPlatform.new_window_backend              			= zest__vk_new_window_backend;

    ZestPlatform.cleanup_frame_graph_semaphore             = zest__vk_cleanup_frame_graph_semaphore;
    ZestPlatform.cleanup_render_pass                       = zest__vk_cleanup_render_pass;
    ZestPlatform.cleanup_image_backend                     = zest__vk_cleanup_image_backend;
    ZestPlatform.cleanup_image_view_backend                = zest__vk_cleanup_image_view_backend;
    ZestPlatform.cleanup_image_view_array_backend          = zest__vk_cleanup_image_view_array_backend;
    ZestPlatform.cleanup_deferred_framebuffers             = zest__vk_cleanup_deferred_framebuffers;
    ZestPlatform.cleanup_deferred_destruction_backend      = zest__vk_cleanup_deferred_destruction_backend;
    ZestPlatform.cleanup_memory_pool_backend               = zest__vk_cleanup_memory_pool_backend;
    ZestPlatform.cleanup_device_backend                    = zest__vk_cleanup_device_backend;
    ZestPlatform.cleanup_buffer_backend                    = zest__vk_cleanup_buffer_backend;
    ZestPlatform.cleanup_renderer_backend                  = zest__vk_cleanup_renderer_backend;
    ZestPlatform.cleanup_shader_resources_backend          = zest__vk_cleanup_shader_resources_backend;
	ZestPlatform.cleanup_swapchain_backend 				= zest__vk_cleanup_swapchain_backend;
	ZestPlatform.cleanup_window_backend 					= zest__vk_cleanup_window_backend;
	ZestPlatform.cleanup_uniform_buffer_backend 			= zest__vk_cleanup_uniform_buffer_backend;
	ZestPlatform.cleanup_compute_backend 					= zest__vk_cleanup_compute_backend;
	ZestPlatform.cleanup_set_layout_backend				= zest__vk_cleanup_set_layout_backend;
	ZestPlatform.cleanup_pipeline_backend 					= zest__vk_cleanup_pipeline_backend;
	ZestPlatform.cleanup_sampler_backend 					= zest__vk_cleanup_sampler_backend;
	ZestPlatform.cleanup_queue_backend 					= zest__vk_cleanup_queue_backend;
}

// -- Error_logging
void zest__log_vulkan_error(VkResult result, const char *file, int line) {
    // Only log actual errors, not warnings or success codes
    const char *error_str = zest__vulkan_error_string(result);
    if (result < 0) { // Vulkan errors are negative enum values
        ZEST_PRINT("Zest Vulkan Error: %s in %s at line %d", error_str, file, line);
        ZEST_APPEND_LOG(ZestDevice->log_path.str, "Zest Vulkan Error: %s in %s at line %d", error_str, file, line);
    } else {
        ZEST_PRINT("Zest Vulkan Warning: %s in %s at line %d", error_str, file, line);
        ZEST_APPEND_LOG(ZestDevice->log_path.str, "Zest Vulkan Warning: %s in %s at line %d", error_str, file, line);
    }
}

const char *zest__vulkan_error_string(VkResult errorCode) {
    switch (errorCode) {
#define ZEST_STR(r) case VK_ ##r: return #r
        ZEST_STR(SUCCESS);
        ZEST_STR(NOT_READY);
        ZEST_STR(TIMEOUT);
        ZEST_STR(EVENT_SET);
        ZEST_STR(EVENT_RESET);
        ZEST_STR(INCOMPLETE);
        ZEST_STR(ERROR_OUT_OF_HOST_MEMORY);
        ZEST_STR(ERROR_OUT_OF_DEVICE_MEMORY);
        ZEST_STR(ERROR_INITIALIZATION_FAILED);
        ZEST_STR(ERROR_DEVICE_LOST);
        ZEST_STR(ERROR_MEMORY_MAP_FAILED);
        ZEST_STR(ERROR_LAYER_NOT_PRESENT);
        ZEST_STR(ERROR_EXTENSION_NOT_PRESENT);
        ZEST_STR(ERROR_FEATURE_NOT_PRESENT);
        ZEST_STR(ERROR_INCOMPATIBLE_DRIVER);
        ZEST_STR(ERROR_TOO_MANY_OBJECTS);
        ZEST_STR(ERROR_FORMAT_NOT_SUPPORTED);
        ZEST_STR(ERROR_FRAGMENTED_POOL);
        ZEST_STR(ERROR_UNKNOWN);
        ZEST_STR(ERROR_OUT_OF_POOL_MEMORY);
        ZEST_STR(ERROR_INVALID_EXTERNAL_HANDLE);
        ZEST_STR(ERROR_FRAGMENTATION);
        ZEST_STR(ERROR_INVALID_OPAQUE_CAPTURE_ADDRESS);
        ZEST_STR(PIPELINE_COMPILE_REQUIRED);
        ZEST_STR(ERROR_SURFACE_LOST_KHR);
        ZEST_STR(ERROR_NATIVE_WINDOW_IN_USE_KHR);
        ZEST_STR(SUBOPTIMAL_KHR);
        ZEST_STR(ERROR_OUT_OF_DATE_KHR);
        ZEST_STR(ERROR_INCOMPATIBLE_DISPLAY_KHR);
        ZEST_STR(ERROR_VALIDATION_FAILED_EXT);
        ZEST_STR(ERROR_INVALID_SHADER_NV);
        ZEST_STR(ERROR_INVALID_DRM_FORMAT_MODIFIER_PLANE_LAYOUT_EXT);
        ZEST_STR(ERROR_NOT_PERMITTED_KHR);
        ZEST_STR(ERROR_FULL_SCREEN_EXCLUSIVE_MODE_LOST_EXT);
        ZEST_STR(THREAD_IDLE_KHR);
        ZEST_STR(THREAD_DONE_KHR);
        ZEST_STR(OPERATION_DEFERRED_KHR);
        ZEST_STR(OPERATION_NOT_DEFERRED_KHR);
#undef ZEST_STR
    default:
        return "UNKNOWN_ERROR";
    }
}
// -- End Error_logging

// -- Enum_to_string_functions
zest_text_t zest__vk_queue_flags_to_string(VkQueueFlags flags) {
    zest_text_t string = { 0 };
    if (!flags) {
        zest_AppendTextf(&string, "%s", "NONE");
        return string;
    }
    zloc_size flags_field = flags;
    while (flags_field) {
        if (zest_TextSize(&string)) {
            zest_AppendTextf(&string, ", ");
        }
        zest_uint index = zloc__scan_forward(flags_field);
        switch (1ull << index) {
        case VK_QUEUE_GRAPHICS_BIT: zest_AppendTextf(&string, "%s", "GRAPHICS"); break;
        case VK_QUEUE_COMPUTE_BIT: zest_AppendTextf(&string, "%s", "COMPUTE"); break;
        case VK_QUEUE_TRANSFER_BIT: zest_AppendTextf(&string, "%s", "TRANSFER"); break;
        case VK_QUEUE_SPARSE_BINDING_BIT: zest_AppendTextf(&string, "%s", "SPARSE BINDING"); break;
        case VK_QUEUE_PROTECTED_BIT: zest_AppendTextf(&string, "%s", "PROTECTED"); break;
        case VK_QUEUE_VIDEO_DECODE_BIT_KHR: zest_AppendTextf(&string, "%s", "VIDEO_DECODE"); break;
        case VK_QUEUE_VIDEO_ENCODE_BIT_KHR: zest_AppendTextf(&string, "%s", "VIDEO_ENCODE"); break;
        case VK_QUEUE_OPTICAL_FLOW_BIT_NV: zest_AppendTextf(&string, "%s", "OPTICAL_FLOW"); break;
        case VK_QUEUE_FLAG_BITS_MAX_ENUM: zest_AppendTextf(&string, "%s", "MAX"); break;
        default: zest_AppendTextf(&string, "%s", "Unknown Queue Flag"); break;
        }
        flags_field &= ~(1ull << index);
    }
    return string;
}

const char *zest__vk_image_layout_to_string(VkImageLayout layout) {
    switch (layout) {
    case VK_IMAGE_LAYOUT_UNDEFINED: return "UNDEFINED"; break;
    case VK_IMAGE_LAYOUT_GENERAL: return "GENERAL"; break;
    case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL: return "COLOR_ATTACHMENT_OPTIMAL"; break;
    case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL: return "DEPTH_STENCIL_ATTACHMENT_OPTIMAL"; break;
    case VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL: return "DEPTH_STENCIL_READ_ONLY_OPTIMAL"; break;
    case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL: return "SHADER_READ_ONLY_OPTIMAL"; break;
    case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL: return "TRANSFER_SRC_OPTIMAL"; break;
    case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL: return "TRANSFER_DST_OPTIMAL"; break;
    case VK_IMAGE_LAYOUT_PREINITIALIZED: return "PREINITIALIZED"; break;
    case VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_STENCIL_ATTACHMENT_OPTIMAL: return "DEPTH_READ_ONLY_STENCIL_ATTACHMENT_OPTIMAL"; break;
    case VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_STENCIL_READ_ONLY_OPTIMAL: return "DEPTH_ATTACHMENT_STENCIL_READ_ONLY_OPTIMAL"; break;
    case VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL: return "DEPTH_ATTACHMENT_OPTIMAL"; break;
    case VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL: return "DEPTH_READ_ONLY_OPTIMAL"; break;
    case VK_IMAGE_LAYOUT_STENCIL_ATTACHMENT_OPTIMAL: return "STENCIL_ATTACHMENT_OPTIMAL"; break;
    case VK_IMAGE_LAYOUT_STENCIL_READ_ONLY_OPTIMAL: return "STENCIL_READ_ONLY_OPTIMAL"; break;
    case VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL: return "READ_ONLY_OPTIMAL"; break;
    case VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL: return "ATTACHMENT_OPTIMAL"; break;
    default: return "Unknown Layout";
    }
}

zest_text_t zest__vk_access_flags_to_string(VkAccessFlags flags) {
    zest_text_t string = { 0 };
    if (!flags) {
        zest_AppendTextf(&string, "%s", "NONE");
        return string;
    }
    zloc_size flags_field = flags;
    while (flags_field) {
        if (zest_TextSize(&string)) {
            zest_AppendTextf(&string, ", ");
        }
        zest_uint index = zloc__scan_forward(flags_field);
        switch (1ull << index) {
        case VK_ACCESS_INDIRECT_COMMAND_READ_BIT: zest_AppendTextf(&string, "%s", "INDIRECT_COMMAND_READ_BIT"); break;
        case VK_ACCESS_INDEX_READ_BIT: zest_AppendTextf(&string, "%s", "INDEX_READ_BIT"); break;
        case VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT: zest_AppendTextf(&string, "%s", "VERTEX_ATTRIBUTE_READ_BIT"); break;
        case VK_ACCESS_UNIFORM_READ_BIT: zest_AppendTextf(&string, "%s", "UNIFORM_READ_BIT"); break;
        case VK_ACCESS_INPUT_ATTACHMENT_READ_BIT: zest_AppendTextf(&string, "%s", "INPUT_ATTACHMENT_READ_BIT"); break;
        case VK_ACCESS_SHADER_READ_BIT: zest_AppendTextf(&string, "%s", "SHADER_READ_BIT"); break;
        case VK_ACCESS_SHADER_WRITE_BIT: zest_AppendTextf(&string, "%s", "SHADER_WRITE_BIT"); break;
        case VK_ACCESS_COLOR_ATTACHMENT_READ_BIT: zest_AppendTextf(&string, "%s", "COLOR_ATTACHMENT_READ_BIT"); break;
        case VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT: zest_AppendTextf(&string, "%s", "COLOR_ATTACHMENT_WRITE_BIT"); break;
        case VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT: zest_AppendTextf(&string, "%s", "DEPTH_STENCIL_ATTACHMENT_READ_BIT"); break;
        case VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT: zest_AppendTextf(&string, "%s", "DEPTH_STENCIL_ATTACHMENT_WRITE_BIT"); break;
        case VK_ACCESS_TRANSFER_READ_BIT: zest_AppendTextf(&string, "%s", "TRANSFER_READ_BIT"); break;
        case VK_ACCESS_TRANSFER_WRITE_BIT: zest_AppendTextf(&string, "%s", "TRANSFER_WRITE_BIT"); break;
        case VK_ACCESS_HOST_READ_BIT: zest_AppendTextf(&string, "%s", "HOST_READ_BIT"); break;
        case VK_ACCESS_HOST_WRITE_BIT: zest_AppendTextf(&string, "%s", "HOST_WRITE_BIT"); break;
        case VK_ACCESS_MEMORY_READ_BIT: zest_AppendTextf(&string, "%s", "MEMORY_READ_BIT"); break;
        case VK_ACCESS_MEMORY_WRITE_BIT: zest_AppendTextf(&string, "%s", "MEMORY_WRITE_BIT"); break;
        case VK_ACCESS_NONE: zest_AppendTextf(&string, "%s", "NONE"); break;
        default: zest_AppendTextf(&string, "%s", "Unknown Access Flags"); break;
        }
        flags_field &= ~(1ull << index);
    }
    return string;
}

zest_text_t zest__vk_pipeline_stage_flags_to_string(VkPipelineStageFlags flags) {
    zest_text_t string = { 0 };
    if (!flags) {
        zest_AppendTextf(&string, "%s", "NONE");
        return string;
    }
    zloc_size flags_field = flags;
    while (flags_field) {
        if (zest_TextSize(&string)) {
            zest_AppendTextf(&string, ", ");
        }
        zest_uint index = zloc__scan_forward(flags_field);
        switch (1ull << index) {
        case VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT: zest_AppendTextf(&string, "%s", "TOP_OF_PIPE_BIT"); break;
        case VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT: zest_AppendTextf(&string, "%s", "DRAW_INDIRECT_BIT"); break;
        case VK_PIPELINE_STAGE_VERTEX_INPUT_BIT: zest_AppendTextf(&string, "%s", "VERTEX_INPUT_BIT"); break;
        case VK_PIPELINE_STAGE_VERTEX_SHADER_BIT: zest_AppendTextf(&string, "%s", "VERTEX_SHADER_BIT"); break;
        case VK_PIPELINE_STAGE_TESSELLATION_CONTROL_SHADER_BIT: zest_AppendTextf(&string, "%s", "TESSELLATION_CONTROL_SHADER_BIT"); break;
        case VK_PIPELINE_STAGE_TESSELLATION_EVALUATION_SHADER_BIT: zest_AppendTextf(&string, "%s", "TESSELLATION_EVALUATION_SHADER_BIT"); break;
        case VK_PIPELINE_STAGE_GEOMETRY_SHADER_BIT: zest_AppendTextf(&string, "%s", "GEOMETRY_SHADER_BIT"); break;
        case VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT: zest_AppendTextf(&string, "%s", "FRAGMENT_SHADER_BIT"); break;
        case VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT: zest_AppendTextf(&string, "%s", "EARLY_FRAGMENT_TESTS_BIT"); break;
        case VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT: zest_AppendTextf(&string, "%s", "LATE_FRAGMENT_TESTS_BIT"); break;
        case VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT: zest_AppendTextf(&string, "%s", "COLOR_ATTACHMENT_OUTPUT_BIT"); break;
        case VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT: zest_AppendTextf(&string, "%s", "COMPUTE_SHADER_BIT"); break;
        case VK_PIPELINE_STAGE_TRANSFER_BIT: zest_AppendTextf(&string, "%s", "TRANSFER_BIT"); break;
        case VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT: zest_AppendTextf(&string, "%s", "BOTTOM_OF_PIPE_BIT"); break;
        case VK_PIPELINE_STAGE_HOST_BIT: zest_AppendTextf(&string, "%s", "HOST_BIT"); break;
        case VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT: zest_AppendTextf(&string, "%s", "ALL_GRAPHICS_BIT"); break;
        case VK_PIPELINE_STAGE_ALL_COMMANDS_BIT: zest_AppendTextf(&string, "%s", "ALL_COMMANDS_BIT"); break;
        case VK_PIPELINE_STAGE_NONE: zest_AppendTextf(&string, "%s", "NONE"); break;
        default: zest_AppendTextf(&string, "%s", "Unknown Pipeline Stage"); break;
        }
        flags_field &= ~(1ull << index);
    }
    return string;
}
// -- End Enum_to_string_functions

// -- Type_converters
ZEST_PRIVATE inline VkClearValue zest__to_vk_clear_value(zest_clear_value_t zest_value, zest_image_aspect_flags aspect) {
    VkClearValue vk_value;
    if (aspect & zest_image_aspect_color_bit) {
        memcpy(vk_value.color.float32, &zest_value.color, sizeof(float) * 4);
    }
    else if (aspect & (zest_image_aspect_depth_bit | zest_image_aspect_stencil_bit)) {
        vk_value.depthStencil.depth = zest_value.depth_stencil.depth;
        vk_value.depthStencil.stencil = zest_value.depth_stencil.stencil;
    } else {
        // Default case or error: clear to black
        memset(&vk_value, 0, sizeof(VkClearValue));
    }
    return vk_value;
}

ZEST_PRIVATE inline VkFormat zest__to_vk_format(zest_format format) {
    return (VkFormat)format;
}

ZEST_PRIVATE inline VkAttachmentLoadOp zest__to_vk_load_op(zest_load_op op) {
    return (VkAttachmentLoadOp)op;
}

ZEST_PRIVATE inline VkAttachmentStoreOp zest__to_vk_store_op(zest_store_op op) {
    return (VkAttachmentStoreOp)op;
}

ZEST_PRIVATE inline VkImageLayout zest__to_vk_image_layout(zest_image_layout layout) {
    return (VkImageLayout)layout;
}

ZEST_PRIVATE inline VkAccessFlags zest__to_vk_access_flags(zest_access_flags flags) {
    return (VkAccessFlags)flags;
}

ZEST_PRIVATE inline VkPipelineStageFlags zest__to_vk_pipeline_stage(zest_pipeline_stage_flags flags) {
    return (VkPipelineStageFlags)flags;
}

ZEST_PRIVATE inline VkShaderStageFlags zest__to_vk_shader_stage(zest_supported_shader_stage_bits flags) {
    return (VkShaderStageFlags)flags;
}

ZEST_PRIVATE inline VkImageAspectFlags zest__to_vk_image_aspect(VkImageAspectFlags flags) {
    return (VkImageAspectFlags)flags;
}

ZEST_PRIVATE inline zest_format zest__from_vk_format(VkFormat format) {
    return (zest_format)format;
}

ZEST_PRIVATE inline VkImageUsageFlags zest__to_vk_image_usage(zest_image_usage_flags flags) {
    return (VkImageUsageFlags)flags;
}

ZEST_PRIVATE inline VkBufferUsageFlags zest__to_vk_buffer_usage(zest_buffer_usage_flags flags) {
    return (VkBufferUsageFlags)flags;
}

ZEST_PRIVATE inline VkMemoryPropertyFlags zest__to_vk_memory_property(zest_memory_property_flags flags) {
    return (VkMemoryPropertyFlags)flags;
}

ZEST_PRIVATE inline VkFilter zest__to_vk_filter(zest_filter_type type) {
    return (VkFilter)type;
}

ZEST_PRIVATE inline VkSamplerMipmapMode zest__to_vk_mipmap_mode(VkSamplerMipmapMode mode) {
    return (VkSamplerMipmapMode)mode;
}

ZEST_PRIVATE inline VkSamplerAddressMode zest__to_vk_sampler_address_mode(VkSamplerAddressMode mode) {
    return (VkSamplerAddressMode)mode;
}

ZEST_PRIVATE inline VkCompareOp zest__to_vk_compare_op(VkCompareOp op) {
    return (VkCompareOp)op;
}

ZEST_PRIVATE inline VkImageTiling zest__to_vk_image_tiling(VkImageTiling tiling) {
    return (VkImageTiling)tiling;
}

ZEST_PRIVATE inline VkSampleCountFlags zest__to_vk_sample_count(VkSampleCountFlags flags) {
    return (VkSampleCountFlags)flags;
}
// -- End Type_converters

// -- Inline_helpers
ZEST_PRIVATE inline VkBufferMemoryBarrier zest__vk_create_buffer_memory_barrier( VkBuffer buffer, VkAccessFlags src_access_mask, VkAccessFlags dst_access_mask, VkDeviceSize offset, VkDeviceSize size) {
    VkBufferMemoryBarrier barrier = { 0 }; 
    barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
    barrier.srcAccessMask = src_access_mask;
    barrier.dstAccessMask = dst_access_mask;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED; 
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.buffer = buffer;
    barrier.offset = offset;
    barrier.size = size;
    return barrier;
}

ZEST_PRIVATE inline VkImageMemoryBarrier zest__vk_create_image_memory_barrier(VkImage image, VkAccessFlags from_access, VkAccessFlags to_access, VkImageLayout from_layout, VkImageLayout to_layout, VkImageAspectFlags aspect_flags, zest_uint target_mip_level, zest_uint mip_count) {
    VkImageMemoryBarrier barrier = { 0 };
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = from_layout;
    barrier.newLayout = to_layout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;
    barrier.subresourceRange.aspectMask = aspect_flags;
    barrier.subresourceRange.baseMipLevel = target_mip_level;
    barrier.subresourceRange.levelCount = mip_count;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;
    barrier.srcAccessMask = from_access;
    barrier.dstAccessMask = to_access;
    return barrier;
}

ZEST_PRIVATE inline VkSemaphore zest__vk_get_semaphore_reference(zest_frame_graph frame_graph, zest_semaphore_reference_t *reference) {
    switch (reference->type) {
    case zest_dynamic_resource_image_available_semaphore:
        ZEST_ASSERT_HANDLE(frame_graph->swapchain);
        return (VkSemaphore)frame_graph->swapchain->backend->vk_image_available_semaphore[ZEST_FIF];
        break;
    case zest_dynamic_resource_render_finished_semaphore:
        return (VkSemaphore)frame_graph->swapchain->backend->vk_render_finished_semaphore[frame_graph->swapchain->current_image_frame];
        break;
    }
    return VK_NULL_HANDLE;
}

ZEST_PRIVATE inline VkResult zest__vk_create_queue_command_pool(int queue_family_index, VkCommandPool *command_pool) {
	VkCommandPoolCreateInfo cmd_info_pool = { 0 };
	cmd_info_pool.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	cmd_info_pool.queueFamilyIndex = queue_family_index;
	cmd_info_pool.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
	ZEST_SET_MEMORY_CONTEXT(zest_platform_device, zest_command_command_pool);
    ZEST_VK_ASSERT_RESULT(vkCreateCommandPool(ZestDevice->backend->logical_device, &cmd_info_pool, &ZestDevice->backend->allocation_callbacks, command_pool));
    return VK_SUCCESS;
}

ZEST_PRIVATE inline VkWriteDescriptorSet zest__vk_create_image_descriptor_write_with_type(VkDescriptorSet descriptor_set, VkDescriptorImageInfo* view_image_info, zest_uint dst_binding, VkDescriptorType type) {
    VkWriteDescriptorSet write = { 0 };
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstSet = descriptor_set;
    write.dstBinding = dst_binding;
    write.dstArrayElement = 0;
    write.descriptorType = type;
    write.descriptorCount = 1;
    write.pImageInfo = view_image_info;
    return write;
}

ZEST_PRIVATE inline VkWriteDescriptorSet zest__vk_create_buffer_descriptor_write_with_type(VkDescriptorSet descriptor_set, VkDescriptorBufferInfo* buffer_info, zest_uint dst_binding, VkDescriptorType type) {
    VkWriteDescriptorSet write = { 0 };
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstSet = descriptor_set;
    write.dstBinding = dst_binding;
    write.dstArrayElement = 0;
    write.descriptorType = type;
    write.descriptorCount = 1;
    write.pBufferInfo = buffer_info;
    return write;
}

ZEST_PRIVATE inline VkResult zest__vk_create_shader_module(char *code, VkShaderModule *shader_module) {
    VkShaderModuleCreateInfo create_info = { 0 };
    create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    create_info.codeSize = zest_vec_size(code);
    create_info.pCode = (zest_uint*)(code);

	ZEST_SET_MEMORY_CONTEXT(zest_platform_renderer, zest_command_shader_module);
    ZEST_VK_ASSERT_RESULT(vkCreateShaderModule(ZestDevice->backend->logical_device, &create_info, &ZestDevice->backend->allocation_callbacks, shader_module));

    return VK_SUCCESS;
}

ZEST_PRIVATE inline zest_uint zest__vk_find_memory_type(zest_uint typeFilter, VkMemoryPropertyFlags properties) {
    VkPhysicalDeviceMemoryProperties memory_properties;
    vkGetPhysicalDeviceMemoryProperties(ZestDevice->backend->physical_device, &memory_properties);

    for (zest_uint i = 0; i < memory_properties.memoryTypeCount; i++) {
        if ((typeFilter & (1 << i)) && (memory_properties.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }

    return ZEST_INVALID;
}

ZEST_PRIVATE inline VkFormat zest__vk_find_supported_format(VkFormat* candidates, zest_uint candidates_count, VkImageTiling tiling, VkFormatFeatureFlags features) {
    ZEST_PRINT_FUNCTION;
    for (int i = 0; i != candidates_count; ++i) {
        VkFormatProperties props;
        vkGetPhysicalDeviceFormatProperties(ZestDevice->backend->physical_device, candidates[i], &props);
        if (tiling == VK_IMAGE_TILING_LINEAR && (props.linearTilingFeatures & features) == features) {
            return candidates[i];
        }
        else if (tiling == VK_IMAGE_TILING_OPTIMAL && (props.optimalTilingFeatures & features) == features) {
            return candidates[i];
        }
    }
    return VK_FORMAT_UNDEFINED;
}

ZEST_PRIVATE inline VkFormat zest__vk_find_depth_format() {
    VkFormat depth_formats[5] = { VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT, VK_FORMAT_D16_UNORM, VK_FORMAT_D16_UNORM_S8_UINT };
    return zest__vk_find_supported_format(depth_formats, 5, VK_IMAGE_TILING_OPTIMAL, VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);
}

ZEST_PRIVATE inline void zest__vk_insert_image_memory_barrier(VkCommandBuffer cmdbuffer, VkImage image, VkAccessFlags srcAccessMask, VkAccessFlags dstAccessMask, VkImageLayout oldImageLayout, VkImageLayout newImageLayout, VkPipelineStageFlags srcStageMask, VkPipelineStageFlags dstStageMask, VkImageSubresourceRange subresourceRange) {
    VkImageMemoryBarrier image_memory_barrier = { 0 };
    image_memory_barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    image_memory_barrier.srcQueueFamilyIndex = ZEST_QUEUE_FAMILY_IGNORED;
    image_memory_barrier.dstQueueFamilyIndex = ZEST_QUEUE_FAMILY_IGNORED;
    image_memory_barrier.srcAccessMask = srcAccessMask;
    image_memory_barrier.dstAccessMask = dstAccessMask;
    image_memory_barrier.oldLayout = oldImageLayout;
    image_memory_barrier.newLayout = newImageLayout;
    image_memory_barrier.image = image;
    image_memory_barrier.subresourceRange = subresourceRange;

    vkCmdPipelineBarrier(
        cmdbuffer,
        srcStageMask,
        dstStageMask,
        0,
        0, 0,
        0, 0,
        1, &image_memory_barrier);
}

ZEST_PRIVATE inline VkResult zest__vk_create_temporary_image(zest_uint width, zest_uint height, zest_uint mipLevels, VkSampleCountFlagBits sample_count, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkImage* image, VkDeviceMemory* memory) {
    ZEST_PRINT_FUNCTION;
    VkImageCreateInfo image_info = { 0 };
    image_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    image_info.imageType = VK_IMAGE_TYPE_2D;
    image_info.extent.width = width;
    image_info.extent.height = height;
    image_info.extent.depth = 1;
    image_info.mipLevels = mipLevels;
    image_info.arrayLayers = 1;
    image_info.format = format;
    image_info.tiling = tiling;
    image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    image_info.usage = usage;
    image_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    image_info.samples = sample_count;

	ZEST_SET_MEMORY_CONTEXT(zest_platform_renderer, zest_command_image);
    ZEST_VK_ASSERT_RESULT(vkCreateImage(ZestDevice->backend->logical_device, &image_info, &ZestDevice->backend->allocation_callbacks, image));

    VkMemoryRequirements memRequirements;
    vkGetImageMemoryRequirements(ZestDevice->backend->logical_device, *image, &memRequirements);

    VkMemoryAllocateInfo alloc_info = { 0 };
    alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    alloc_info.allocationSize = memRequirements.size;
    alloc_info.memoryTypeIndex = zest__vk_find_memory_type(memRequirements.memoryTypeBits, properties);

	ZEST_SET_MEMORY_CONTEXT(zest_platform_renderer, zest_command_allocate_memory_image);
    ZEST_VK_ASSERT_RESULT(vkAllocateMemory(ZestDevice->backend->logical_device, &alloc_info, &ZestDevice->backend->allocation_callbacks, memory));

    vkBindImageMemory(ZestDevice->backend->logical_device, *image, *memory, 0);

    return VK_SUCCESS;
}

ZEST_PRIVATE inline void zest__vk_place_image_barrier(VkCommandBuffer command_buffer, VkPipelineStageFlags src_stage, VkPipelineStageFlags dst_stage, VkImageMemoryBarrier *barrier) {
    ZEST_PRINT_FUNCTION;
	vkCmdPipelineBarrier(
		command_buffer,
		src_stage,
		dst_stage,
		0,
		0, NULL,
		0, NULL,
		1, barrier
	);
}

ZEST_PRIVATE VkFence zest__vk_create_fence() {
    ZEST_PRINT_FUNCTION;
    VkFenceCreateInfo fence_info = { 0 };
    fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fence_info.flags = 0;
    VkFence fence;
	ZEST_SET_MEMORY_CONTEXT(zest_platform_renderer, zest_command_fence);
    vkCreateFence(ZestDevice->backend->logical_device, &fence_info, &ZestDevice->backend->allocation_callbacks, &fence);
    return fence;
}

ZEST_PRIVATE inline VkDeviceMemory zest__vk_get_buffer_device_memory(zest_buffer buffer) {
    return buffer->memory_pool->backend->memory;
}

ZEST_PRIVATE inline VkBuffer* zest__vk_get_device_buffer(zest_buffer buffer) {
    return &buffer->backend->vk_buffer;
}

ZEST_PRIVATE inline VkDescriptorBufferInfo zest__vk_get_buffer_info(zest_buffer buffer) {
    VkDescriptorBufferInfo buffer_info = { 0 };
    buffer_info.buffer = buffer->backend->vk_buffer;
    buffer_info.offset = buffer->buffer_offset;
    buffer_info.range = buffer->size;
    return buffer_info;
}
// -- End Inline_helpers

// -- Swapchain_setup
zest_swapchain_support_details_t zest__vk_query_swapchain_support(VkPhysicalDevice physical_device) {
    zest_swapchain_support_details_t details;

    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physical_device, ZestRenderer->main_window->backend->surface, &details.capabilities);

    vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device, ZestRenderer->main_window->backend->surface, &details.formats_count, ZEST_NULL);

    if (details.formats_count != 0) {
        details.formats = (VkSurfaceFormatKHR*)ZEST__ALLOCATE(sizeof(VkSurfaceFormatKHR) * details.formats_count);
        vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device, ZestRenderer->main_window->backend->surface, &details.formats_count, details.formats);
    }

    vkGetPhysicalDeviceSurfacePresentModesKHR(physical_device, ZestRenderer->main_window->backend->surface, &details.present_modes_count, ZEST_NULL);

    if (details.present_modes_count != 0) {
        details.present_modes = (VkPresentModeKHR*)ZEST__ALLOCATE(sizeof(VkPresentModeKHR) * details.present_modes_count);
        vkGetPhysicalDeviceSurfacePresentModesKHR(physical_device, ZestRenderer->main_window->backend->surface, &details.present_modes_count, details.present_modes);
    }

    return details;
}

VkSurfaceFormatKHR zest__vk_choose_swapchain_format(VkSurfaceFormatKHR *available_formats) {
    size_t num_available_formats = zest_vec_size(available_formats);

    // --- 1. Handle the rare case where the surface provides no preferred formats ---
    if (num_available_formats == 1 && available_formats[0].format == VK_FORMAT_UNDEFINED) {
        ZEST_APPEND_LOG(ZestDevice->log_path.str, "Swapchain: Surface format is UNDEFINED. Choosing VK_FORMAT_B8G8R8A8_SRGB by default.");
        VkSurfaceFormatKHR chosen_format = {
            VK_FORMAT_B8G8R8A8_SRGB, // Prefer SRGB for automatic gamma correction
            VK_COLOR_SPACE_SRGB_NONLINEAR_KHR
        };
        // Note: A truly robust implementation might double-check if this default
        // is *actually* supported by querying all possible formats, but this case is rare.
        return chosen_format;
    }

    // --- 2. Determine the user's desired format ---
    VkFormat desired_format = (VkFormat)ZestApp->create_info.color_format;

    if (desired_format == VK_FORMAT_UNDEFINED) {
        desired_format = VK_FORMAT_B8G8R8A8_SRGB; // Default to SRGB if user doesn't care
        ZEST_APPEND_LOG(ZestDevice->log_path.str, "Swapchain: User preference is UNDEFINED. Defaulting preference to VK_FORMAT_B8G8R8A8_SRGB.");
    } else {
        ZEST_APPEND_LOG(ZestDevice->log_path.str, "Swapchain: User preference is Format %d.", desired_format);
    }

    // --- 3. Search for the User's Preferred Format with SRGB Color Space ---
    for (size_t i = 0; i < num_available_formats; ++i) {
        if (available_formats[i].format == desired_format &&
            available_formats[i].colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
			ZEST_APPEND_LOG(ZestDevice->log_path.str, "Swapchain: Found exact user preference: Format %d, Colorspace %d", available_formats[i].format, available_formats[i].colorSpace);
            return available_formats[i];
        }
    }
	ZEST_APPEND_LOG(ZestDevice->log_path.str, "Swapchain: User preferred format (%d) with SRGB colorspace not available.", desired_format);

    // --- 4. Fallback: Search for *any* SRGB Format with SRGB Color Space ---
    for (size_t i = 0; i < num_available_formats; ++i) {
        if (available_formats[i].format == VK_FORMAT_B8G8R8A8_SRGB &&
            available_formats[i].colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
			ZEST_APPEND_LOG(ZestDevice->log_path.str, "Swapchain: Falling back to available VK_FORMAT_B8G8R8A8_SRGB.");
            return available_formats[i];
        }
    }
    for (size_t i = 0; i < num_available_formats; ++i) {
        if (available_formats[i].format == VK_FORMAT_R8G8B8A8_SRGB &&
            available_formats[i].colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
			ZEST_APPEND_LOG(ZestDevice->log_path.str, "Swapchain: Falling back to available VK_FORMAT_R8G8B8A8_SRGB.");
            return available_formats[i];
        }
    }
	ZEST_APPEND_LOG(ZestDevice->log_path.str, "Swapchain: No SRGB formats with SRGB colorspace available.");

    // --- 5. Fallback: Search for *any* UNORM Format with SRGB Color Space ---
    // If no SRGB format works, take any UNORM format as long as the colorspace is right.
    // This means we'll have to handle gamma correction manually in the shader.
    for (size_t i = 0; i < num_available_formats; ++i) {
        if (available_formats[i].format == VK_FORMAT_B8G8R8A8_UNORM &&
            available_formats[i].colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
			ZEST_APPEND_LOG(ZestDevice->log_path.str, "Swapchain: Falling back to VK_FORMAT_B8G8R8A8_UNORM with SRGB colorspace (Manual gamma needed).");
            return available_formats[i];
        }
    }
    for (size_t i = 0; i < num_available_formats; ++i) {
        if (available_formats[i].format == VK_FORMAT_R8G8B8A8_UNORM &&
            available_formats[i].colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
			ZEST_APPEND_LOG(ZestDevice->log_path.str, "Swapchain: Falling back to VK_FORMAT_R8G8B8A8_UNORM with SRGB colorspace (Manual gamma needed).");
            return available_formats[i];
        }
    }
	ZEST_APPEND_LOG(ZestDevice->log_path.str, "Swapchain: No UNORM formats with SRGB colorspace available.");

    // --- 6. Last Resort Fallback ---
	ZEST_APPEND_LOG(ZestDevice->log_path.str, "Swapchain: Critical Fallback! Using first available format: Format %d, Colorspace %d. Check results!", available_formats[0].format, available_formats[0].colorSpace);
    return available_formats[0];
}

VkPresentModeKHR zest__vk_choose_present_mode(VkPresentModeKHR* available_present_modes, zest_bool use_vsync) {
    VkPresentModeKHR best_mode = VK_PRESENT_MODE_FIFO_KHR;

    if (use_vsync) {
        return best_mode;
    }

    zest_vec_foreach(i, available_present_modes) {
        if (available_present_modes[i] == VK_PRESENT_MODE_MAILBOX_KHR) {
            return available_present_modes[i];
        }
        else if (available_present_modes[i] == VK_PRESENT_MODE_IMMEDIATE_KHR) {
            return available_present_modes[i];
        }
    }

    return best_mode;
}

VkExtent2D zest__vk_choose_swap_extent(VkSurfaceCapabilitiesKHR* capabilities) {
    /*
    Currently forcing getting the current window size each time because if your app starts up on a different
     monitor with a different dpi then it can crash because the DPI doesn't match the initial surface capabilities
     that were found.
    if (capabilities->currentExtent.width != ZEST_U32_MAX_VALUE) {
        return capabilities->currentExtent;
    }
    else {
     */
    int fb_width = 0, fb_height = 0;
    int window_width = 0, window_height = 0;
    ZestRenderer->get_window_size_callback(ZestApp->user_data, &fb_width, &fb_height, &window_width, &window_height);

    VkExtent2D actual_extent = {
        (zest_uint)(fb_width),
        (zest_uint)(fb_height)
    };

    actual_extent.width = ZEST__MAX(capabilities->minImageExtent.width, ZEST__MIN(capabilities->maxImageExtent.width, actual_extent.width));
    actual_extent.height = ZEST__MAX(capabilities->minImageExtent.height, ZEST__MIN(capabilities->maxImageExtent.height, actual_extent.height));

    return actual_extent;
    //}
}

zest_bool zest__vk_initialise_swapchain(zest_swapchain swapchain, zest_window window) {
    ZEST_ASSERT_HANDLE(swapchain);   //Not a valid swapchain handle!
    zest_swapchain_support_details_t swapchain_support_details = zest__vk_query_swapchain_support(ZestDevice->backend->physical_device);
    swapchain->window = window;

    VkSurfaceFormatKHR surfaceFormat = zest__vk_choose_swapchain_format(swapchain_support_details.formats);
    VkPresentModeKHR presentMode = zest__vk_choose_present_mode(swapchain_support_details.present_modes, ZestRenderer->flags & zest_renderer_flag_vsync_enabled);
    VkExtent2D extent = zest__vk_choose_swap_extent(&swapchain_support_details.capabilities);
    ZestRenderer->dpi_scale = (float)extent.width / (float)ZestRenderer->main_window->window_width;
    swapchain->format = (zest_format)surfaceFormat.format;

    swapchain->format = (zest_format)surfaceFormat.format;
    swapchain->size.width = extent.width;
    swapchain->size.height = extent.width;
    ZestRenderer->window_extent.width = ZestRenderer->main_window->window_width;
    ZestRenderer->window_extent.height = ZestRenderer->main_window->window_height;

    zest_uint image_count = swapchain_support_details.capabilities.minImageCount + 1;

    if (swapchain_support_details.capabilities.maxImageCount > 0 && image_count > swapchain_support_details.capabilities.maxImageCount) {
        image_count = swapchain_support_details.capabilities.maxImageCount;
    }

    VkSwapchainCreateInfoKHR createInfo = { 0 };
    createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    createInfo.surface = ZestRenderer->main_window->backend->surface;

    createInfo.minImageCount = image_count;
    createInfo.imageFormat = surfaceFormat.format;
    createInfo.imageColorSpace = surfaceFormat.colorSpace;
    createInfo.imageExtent = extent;
    createInfo.imageArrayLayers = 1;
    createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    createInfo.preTransform = swapchain_support_details.capabilities.currentTransform;
    createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    createInfo.presentMode = presentMode;
    createInfo.clipped = VK_TRUE;
    createInfo.oldSwapchain = VK_NULL_HANDLE;

    ZEST_SET_MEMORY_CONTEXT(zest_platform_renderer, zest_command_swapchain);
    ZEST_RETURN_FALSE_ON_FAIL(vkCreateSwapchainKHR(ZestDevice->backend->logical_device, &createInfo, &ZestDevice->backend->allocation_callbacks, &swapchain->backend->vk_swapchain));

    swapchain->image_count = image_count;

    vkGetSwapchainImagesKHR(ZestDevice->backend->logical_device, swapchain->backend->vk_swapchain, &image_count, ZEST_NULL);
    VkImage *images = NULL;
    zest_vec_resize(images, image_count);
    vkGetSwapchainImagesKHR(ZestDevice->backend->logical_device, swapchain->backend->vk_swapchain, &image_count, images);

    zest_vec_resize(swapchain->images, image_count);
    zest_vec_resize(swapchain->views, image_count);
    zest_image_info_t image_info = zest_CreateImageInfo(extent.width, extent.height);
    image_info.aspect_flags = zest_image_aspect_color_bit;
    image_info.format = (zest_format)surfaceFormat.format;
    zest_vec_foreach(i, images) {
        swapchain->images[i] = zest__image_zero;
        swapchain->images[i].backend = zest__vk_new_image_backend();
        swapchain->images[i].backend->vk_image = images[i];
        swapchain->images[i].backend->vk_format = surfaceFormat.format;
        swapchain->images[i].backend->vk_aspect = VK_IMAGE_ASPECT_COLOR_BIT;
        swapchain->images[i].backend->vk_extent.width = extent.width;
        swapchain->images[i].backend->vk_extent.width = extent.width;
        swapchain->images[i].backend->vk_extent.height = extent.height;
        swapchain->images[i].backend->vk_current_layout = VK_IMAGE_LAYOUT_UNDEFINED;
        swapchain->images[i].info = image_info;
    }

    ZEST__FREE(swapchain_support_details.formats);
    ZEST__FREE(swapchain_support_details.present_modes);
    zest_vec_free(images);

    VkSemaphoreCreateInfo semaphore_info = { 0 };
    semaphore_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    zest_ForEachFrameInFlight(i) {
        ZEST_SET_MEMORY_CONTEXT(zest_platform_renderer, zest_command_semaphore);
        ZEST_RETURN_FALSE_ON_FAIL(vkCreateSemaphore(ZestDevice->backend->logical_device, &semaphore_info, &ZestDevice->backend->allocation_callbacks, &swapchain->backend->vk_image_available_semaphore[i]));
    }

    zest_vec_resize(swapchain->backend->vk_render_finished_semaphore, swapchain->image_count);

    zest_vec_foreach(i, swapchain->backend->vk_render_finished_semaphore) {
        ZEST_SET_MEMORY_CONTEXT(zest_platform_renderer, zest_command_semaphore);
        ZEST_RETURN_FALSE_ON_FAIL(vkCreateSemaphore(ZestDevice->backend->logical_device, &semaphore_info, &ZestDevice->backend->allocation_callbacks, &swapchain->backend->vk_render_finished_semaphore[i]));
    }

    zest_vec_foreach(i, swapchain->images) {
        swapchain->views[i] = zest__vk_create_image_view(&swapchain->images[i], zest_image_view_type_2d_array, 1, 0, 0, 1, 0);
    }

    return ZEST_TRUE;
}
// -- End Swapchain_setup

// -- Swapchain_presenting
zest_bool zest__vk_dummy_submit_for_present_only(void) {
    ZEST_RETURN_FALSE_ON_FAIL(vkResetCommandPool(ZestDevice->backend->logical_device, ZestDevice->backend->one_time_command_pool[ZEST_FIF], 0));

    VkCommandBufferBeginInfo beginInfo = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    ZEST_RETURN_FALSE_ON_FAIL(vkBeginCommandBuffer(ZestRenderer->backend->utility_command_buffer[ZEST_FIF], &beginInfo));

    VkImageMemoryBarrier image_barrier = { 0 };
    image_barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    image_barrier.srcAccessMask = 0; 
    image_barrier.dstAccessMask = 0; 
    image_barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    image_barrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    image_barrier.srcQueueFamilyIndex = ZEST_QUEUE_FAMILY_IGNORED;
    image_barrier.dstQueueFamilyIndex = ZEST_QUEUE_FAMILY_IGNORED;
    zest_swapchain swapchain = ZestRenderer->main_swapchain;
    image_barrier.image = swapchain->images[swapchain->current_image_frame].backend->vk_image;
    image_barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    image_barrier.subresourceRange.baseMipLevel = 0;
    image_barrier.subresourceRange.levelCount = 1;
    image_barrier.subresourceRange.baseArrayLayer = 0;
    image_barrier.subresourceRange.layerCount = 1;

    vkCmdPipelineBarrier(
        ZestRenderer->backend->utility_command_buffer[ZEST_FIF],
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,      
        VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,   
        0,                                      
        0, NULL,                               
        0, NULL,                              
        1, &image_barrier                    
    );

    ZEST_RETURN_FALSE_ON_FAIL(vkEndCommandBuffer(ZestRenderer->backend->utility_command_buffer[ZEST_FIF]));

    VkPipelineStageFlags wait_stage_array[] = { VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT }; 
    VkSubmitInfo submit_info = { 0 };
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

    submit_info.waitSemaphoreCount = 1;
    submit_info.pWaitSemaphores = &ZestRenderer->main_swapchain->backend->vk_image_available_semaphore[ZEST_FIF];
    submit_info.pWaitDstStageMask = wait_stage_array;

    submit_info.signalSemaphoreCount = 1;
    submit_info.pSignalSemaphores = &ZestRenderer->main_swapchain->backend->vk_render_finished_semaphore[swapchain->current_image_frame];

    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &ZestRenderer->backend->utility_command_buffer[ZEST_FIF];

    VkFence fence = ZestRenderer->backend->fif_fence[ZEST_FIF][0];
    ZestRenderer->fence_count[ZEST_FIF] = 1;
    ZEST_RETURN_FALSE_ON_FAIL(vkQueueSubmit(ZestDevice->graphics_queue.backend->vk_queue, 1, &submit_info, fence));

    return ZEST_TRUE;
}

zest_bool zest__vk_present_frame(zest_swapchain swapchain) {
    VkPresentInfoKHR presentInfo = { 0 };
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = &swapchain->backend->vk_render_finished_semaphore[swapchain->current_image_frame];
    VkSwapchainKHR swapChains[] = { swapchain->backend->vk_swapchain };
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = swapChains;
    presentInfo.pImageIndices = &swapchain->current_image_frame;
    presentInfo.pResults = ZEST_NULL;

    VkResult result = vkQueuePresentKHR(ZestDevice->graphics_queue.backend->vk_queue, &presentInfo);
    ZestRenderer->backend->last_result = result;

	zest_bool status = ZEST_TRUE;

    if ((ZestRenderer->flags & zest_renderer_flag_schedule_change_vsync) || result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || swapchain->window->framebuffer_resized) {
        swapchain->window->framebuffer_resized = ZEST_FALSE;
        ZEST__UNFLAG(ZestRenderer->flags, zest_renderer_flag_schedule_change_vsync);

		status = ZEST_FALSE;
    }

    ZestDevice->previous_fif = ZestDevice->current_fif;
    ZestDevice->current_fif = (ZestDevice->current_fif + 1) % ZEST_MAX_FIF;

    return status;
}

zest_bool zest__vk_acquire_swapchain_image(zest_swapchain swapchain) {
    VkResult result = vkAcquireNextImageKHR(ZestDevice->backend->logical_device, swapchain->backend->vk_swapchain, UINT64_MAX, swapchain->backend->vk_image_available_semaphore[ZEST_FIF], ZEST_NULL, &swapchain->current_image_frame);
    ZestRenderer->backend->last_result = result;
    //Has the window been resized? if so rebuild the swap chain.
    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
        return ZEST_FALSE;
    }
    return ZEST_TRUE;
}
// -- End Swapchain_presenting

void zest_UseVulkan() {
	zest__register_platform(zest_platform_vulkan, zest__vk_initialise_platform_callbacks);
}

// -- Initialisation_functions
zest_bool zest__vk_initialise_device(void) {
    if (!zest__vk_create_instance()) {
        return ZEST_FALSE;
    }
	if (zest__validation_layers_are_enabled()) {
		zest__vk_setup_validation();
	}
	zest__vk_pick_physical_device();
    if (!zest__vk_create_logical_device()) {
        return ZEST_FALSE;
    }
	zest__vk_set_limit_data();
	zest__set_default_pool_sizes();
    return ZEST_TRUE;
}

static VKAPI_ATTR VkBool32 VKAPI_CALL zest__vk_debug_callback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageType, const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData, void *pUserData) {
    if (pCallbackData->messageIdNumber == 1734198062) {
        //Just ignore the best practice "Hey validation errors are for debug only".
        return VK_FALSE;
    }
    if (ZestDevice->log_path.str) {
        ZEST_APPEND_LOG(ZestDevice->log_path.str, "Validation Layer: %s", pCallbackData->pMessage);
    }
    if (ZEST__FLAGGED(ZestApp->create_info.flags, zest_init_flag_log_validation_errors_to_console)) {
        ZEST_PRINT("%s", pCallbackData->pMessageIdName);
        ZEST_PRINT("Validation Layer: %s", pCallbackData->pMessage);
        ZEST_PRINT("-------------------------------------------------------");
    }
    if (ZEST__FLAGGED(ZestApp->create_info.flags, zest_init_flag_log_validation_errors_to_memory)) {
        if (!zest_map_valid_key(ZestDevice->validation_errors, (zest_key)pCallbackData->messageIdNumber)) {
            zest_text_t error_message = { 0 };
            zest_SetText(&error_message, pCallbackData->pMessage);
            zest_map_insert_key(ZestDevice->validation_errors, (zest_key)pCallbackData->messageIdNumber, error_message);
        }
    }

    return VK_FALSE;
}

VkResult zest__vk_create_debug_messenger(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT *pCreateInfo, const VkAllocationCallbacks *pAllocator, VkDebugUtilsMessengerEXT *pDebugMessenger) {
    PFN_vkCreateDebugUtilsMessengerEXT func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
    if (func != ZEST_NULL) {
        return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
    } else {
        return VK_ERROR_EXTENSION_NOT_PRESENT;
    }
}

zest_bool zest__vk_check_validation_layer_support(void) {
    zest_uint layer_count;
    vkEnumerateInstanceLayerProperties(&layer_count, ZEST_NULL);

    ZEST__ARRAY(available_layers, VkLayerProperties, layer_count);
    vkEnumerateInstanceLayerProperties(&layer_count, available_layers);

    for (int i = 0; i != zest__validation_layer_count; ++i) {
        zest_bool layer_found = 0;
        const char *layer_name = zest_validation_layers[i];

        for (int layer = 0; layer != layer_count; ++layer) {
            if (strcmp(layer_name, available_layers[layer].layerName) == 0) {
                layer_found = 1;
                break;
            }
        }

        if (!layer_found) {
            return 0;
        }
    }

    ZEST__FREE(available_layers);

    return 1;
}

zest_bool zest__vk_check_device_extension_support(VkPhysicalDevice physical_device) {
    zest_uint extension_count;
    vkEnumerateDeviceExtensionProperties(physical_device, ZEST_NULL, &extension_count, ZEST_NULL);

    ZEST__ARRAY(available_extensions, VkExtensionProperties, extension_count);
    vkEnumerateDeviceExtensionProperties(physical_device, ZEST_NULL, &extension_count, available_extensions);

    zest_uint required_extensions_found = 0;
    for (int i = 0; i != extension_count; ++i) {
        for (int e = 0; e != zest__required_extension_names_count; ++e) {
            if (strcmp(available_extensions[i].extensionName, zest_required_extensions[e]) == 0) {
                required_extensions_found++;
            }
        }
    }

    ZEST__FREE(available_extensions);
    return required_extensions_found >= zest__required_extension_names_count;
}

zest_bool zest__vk_is_device_suitable(VkPhysicalDevice physical_device) {
    zest_bool extensions_supported = zest__vk_check_device_extension_support(physical_device);

    zest_bool swapchain_adequate = 0;
    if (extensions_supported) {
        zest_swapchain_support_details_t swapchain_support_details = zest__vk_query_swapchain_support(physical_device);
        swapchain_adequate = swapchain_support_details.formats_count && swapchain_support_details.present_modes_count;
        ZEST__FREE(swapchain_support_details.formats);
        ZEST__FREE(swapchain_support_details.present_modes);
    }

    VkPhysicalDeviceFeatures supported_features;
    vkGetPhysicalDeviceFeatures(physical_device, &supported_features);

    return extensions_supported && swapchain_adequate && supported_features.samplerAnisotropy;
}

void zest__vk_log_device_name(VkPhysicalDevice physical_device) {
    VkPhysicalDeviceProperties properties;
    vkGetPhysicalDeviceProperties(physical_device, &properties);
    ZEST_APPEND_LOG(ZestDevice->log_path.str, "\t%s", properties.deviceName);
}

zest_bool zest__vk_device_is_discrete_gpu(VkPhysicalDevice physical_device) {
    VkPhysicalDeviceProperties properties;
    vkGetPhysicalDeviceProperties(physical_device, &properties);
    return properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU;
}

VkSampleCountFlagBits zest__vk_get_max_useable_sample_count(void) {
    VkPhysicalDeviceProperties physical_device_properties;
    vkGetPhysicalDeviceProperties(ZestDevice->backend->physical_device, &physical_device_properties);

    VkSampleCountFlags counts = ZEST__MIN(physical_device_properties.limits.framebufferColorSampleCounts, physical_device_properties.limits.framebufferDepthSampleCounts);
    if (counts & VK_SAMPLE_COUNT_64_BIT) { return VK_SAMPLE_COUNT_64_BIT; }
    if (counts & VK_SAMPLE_COUNT_32_BIT) { return VK_SAMPLE_COUNT_32_BIT; }
    if (counts & VK_SAMPLE_COUNT_16_BIT) { return VK_SAMPLE_COUNT_16_BIT; }
    if (counts & VK_SAMPLE_COUNT_8_BIT) { return VK_SAMPLE_COUNT_8_BIT; }
    if (counts & VK_SAMPLE_COUNT_4_BIT) { return VK_SAMPLE_COUNT_4_BIT; }
    if (counts & VK_SAMPLE_COUNT_2_BIT) { return VK_SAMPLE_COUNT_2_BIT; }

    return VK_SAMPLE_COUNT_1_BIT;
}

void zest__vk_set_limit_data() {
    VkPhysicalDeviceProperties physicalDeviceProperties;
    vkGetPhysicalDeviceProperties(ZestDevice->backend->physical_device, &physicalDeviceProperties);
    ZestDevice->max_image_size = physicalDeviceProperties.limits.maxImageDimension2D;
}

void zest__vk_setup_validation(void) {
    ZEST_APPEND_LOG(ZestDevice->log_path.str, "Enabling validation layers\n");

    VkDebugUtilsMessengerCreateInfoEXT create_info = { 0 };
    create_info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    create_info.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    create_info.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    create_info.pfnUserCallback = zest__vk_debug_callback;

    ZEST_SET_MEMORY_CONTEXT(zest_platform_device, zest_command_debug_messenger);
    ZEST_VK_LOG(zest__vk_create_debug_messenger(ZestDevice->backend->instance, &create_info, &ZestDevice->backend->allocation_callbacks, &ZestDevice->backend->debug_messenger));

    ZestDevice->backend->pfnSetDebugUtilsObjectNameEXT = (PFN_vkSetDebugUtilsObjectNameEXT)vkGetInstanceProcAddr(ZestDevice->backend->instance, "vkSetDebugUtilsObjectNameEXT");
}

void zest__vk_pick_physical_device(void) {
    zest_uint device_count = 0;
    vkEnumeratePhysicalDevices(ZestDevice->backend->instance, &device_count, ZEST_NULL);

    ZEST_ASSERT(device_count);        //Failed to find GPUs with Vulkan support!

    ZEST__ARRAY(devices, VkPhysicalDevice, device_count);
    vkEnumeratePhysicalDevices(ZestDevice->backend->instance, &device_count, devices);

    ZEST_APPEND_LOG(ZestDevice->log_path.str, "Found %i devices", device_count);
    for (int i = 0; i != device_count; ++i) {
        zest__vk_log_device_name(devices[i]);
    }
    ZestDevice->backend->physical_device = VK_NULL_HANDLE;

    //Prioritise discrete GPUs when picking physical device
    if (device_count == 1 && zest__vk_is_device_suitable(devices[0])) {
        if (zest__vk_device_is_discrete_gpu(devices[0])) {
            ZEST_APPEND_LOG(ZestDevice->log_path.str, "The one device found is suitable and is a discrete GPU");
        } else {
            ZEST_APPEND_LOG(ZestDevice->log_path.str, "The one device found is suitable");
        }
        ZestDevice->backend->physical_device = devices[0];
    } else {
        VkPhysicalDevice discrete_device = VK_NULL_HANDLE;
        for (int i = 0; i != device_count; ++i) {
            if (zest__vk_is_device_suitable(devices[i]) && zest__vk_device_is_discrete_gpu(devices[i])) {
                discrete_device = devices[i];
                break;
            }
        }
        if (discrete_device != VK_NULL_HANDLE) {
            ZEST_APPEND_LOG(ZestDevice->log_path.str, "Found suitable device that is a discrete GPU: ");
            zest__vk_log_device_name(discrete_device);
            ZestDevice->backend->physical_device = discrete_device;
        } else {
            for (int i = 0; i != device_count; ++i) {
                if (zest__vk_is_device_suitable(devices[i])) {
                    ZEST_APPEND_LOG(ZestDevice->log_path.str, "Found suitable device:");
                    zest__vk_log_device_name(devices[i]);
                    ZestDevice->backend->physical_device = devices[i];
                    break;
                }
            }
        }
    }

    if (ZestDevice->backend->physical_device == VK_NULL_HANDLE) {
        ZEST_APPEND_LOG(ZestDevice->log_path.str, "Could not find a suitable device!");
    }
    ZEST_ASSERT(ZestDevice->backend->physical_device != VK_NULL_HANDLE);    //Unable to find suitable GPU
    ZestDevice->backend->msaa_samples = zest__vk_get_max_useable_sample_count();

    // Store Properties features, limits and properties of the physical ZestDevice for later use
    // ZestDevice properties also contain limits and sparse properties
    vkGetPhysicalDeviceProperties(ZestDevice->backend->physical_device, &ZestDevice->backend->properties);
    // Features should be checked by the examples before using them
    vkGetPhysicalDeviceFeatures(ZestDevice->backend->physical_device, &ZestDevice->backend->features);
    // Memory properties are used regularly for creating all kinds of buffers
    vkGetPhysicalDeviceMemoryProperties(ZestDevice->backend->physical_device, &ZestDevice->backend->memory_properties);

    //Print out the memory available
    ZEST_APPEND_LOG(ZestDevice->log_path.str, "Max Memory Allocation Count: %i", ZestDevice->backend->properties.limits.maxMemoryAllocationCount);
    ZEST_APPEND_LOG(ZestDevice->log_path.str, "Memory available in GPU:");
    for (int i = 0; i != ZestDevice->backend->memory_properties.memoryHeapCount; ++i) {
        ZEST_APPEND_LOG(ZestDevice->log_path.str, "    Heap flags: %i, Size: %llu", ZestDevice->backend->memory_properties.memoryHeaps[i].flags, ZestDevice->backend->memory_properties.memoryHeaps[i].size);
    }

    ZEST_APPEND_LOG(ZestDevice->log_path.str, "Memory types mapping in GPU:");
    for (int i = 0; i != ZestDevice->backend->memory_properties.memoryTypeCount; ++i) {
        ZEST_APPEND_LOG(ZestDevice->log_path.str, "    %i) Heap Index: %i, Property Flags: %i", i, ZestDevice->backend->memory_properties.memoryTypes[i].heapIndex, ZestDevice->backend->memory_properties.memoryTypes[i].propertyFlags);
    }

    ZEST__FREE(devices);
}

void zest__vk_get_required_extensions() {
    ZestRenderer->add_platform_extensions_callback();

    //If you're compiling on Mac and hitting this assert then it could be because you need to allow 3rd party libraries when signing the app.
    //Check "Disable Library Validation" under Signing and Capabilities
    ZEST_ASSERT(ZestDevice->extensions); //Vulkan not available

    if (zest__validation_layers_are_enabled()) {
        zest_AddInstanceExtension(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }

#ifdef ZEST_PORTABILITY_ENUMERATION
    ZEST_APPEND_LOG(ZestDevice->log_path.str, "Adding enumerate portability extension");
    zest_AddInstanceExtension(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);
    zest_AddInstanceExtension("VK_KHR_get_physical_device_properties2");
#endif
}


zest_bool zest__vk_create_instance() {
    if (zest__validation_layers_are_enabled()) {
        zest_bool validation_support = zest__vk_check_validation_layer_support();
        ZEST_APPEND_LOG(ZestDevice->log_path.str, "Checking for validation support");
        if (!validation_support) {
            ZEST_APPEND_LOG(ZestDevice->log_path.str, "Validation layers not supported. Disabling.");
            ZEST__UNFLAG(ZestDevice->setup_info.flags, zest_init_flag_enable_validation_layers);;
        }
    }

    zest_uint instance_api_version_supported;
    if (vkEnumerateInstanceVersion(&instance_api_version_supported) == VK_SUCCESS) {
        ZEST_APPEND_LOG(ZestDevice->log_path.str, "System supports Vulkan API Version: %d.%d.%d",
            VK_API_VERSION_MAJOR(instance_api_version_supported),
            VK_API_VERSION_MINOR(instance_api_version_supported),
            VK_API_VERSION_PATCH(instance_api_version_supported));
        if (instance_api_version_supported < VK_API_VERSION_1_2) {
            ZEST_APPEND_LOG(ZestDevice->log_path.str, "Zest requires minumum Vulkan version: 1.2+. Version found: %u", instance_api_version_supported);
            return VK_ERROR_INCOMPATIBLE_DRIVER;
        }
    } else {
        ZEST_PRINT_WARNING("Vulkan 1.0 detected (vkEnumerateInstanceVersion not found). Zest requires Vulkan 1.2+.");
        ZEST_APPEND_LOG(ZestDevice->log_path.str, "Vulkan 1.0 detected (vkEnumerateInstanceVersion not found). Zest requiresVulkan 1.2+.")
            return VK_ERROR_INCOMPATIBLE_DRIVER;
    }

    VkApplicationInfo app_info = { 0 };
    app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app_info.pApplicationName = "Zest";
    app_info.applicationVersion = VK_MAKE_VERSION(0, 0, 1);
    app_info.pEngineName = "No Engine";
    app_info.engineVersion = VK_MAKE_VERSION(0, 0, 1);
    app_info.apiVersion = VK_API_VERSION_1_2;
    ZestDevice->api_version = app_info.apiVersion;

    VkInstanceCreateInfo create_info = { 0 };
    create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    create_info.pApplicationInfo = &app_info;
#ifdef ZEST_PORTABILITY_ENUMERATION
    ZEST_APPEND_LOG(ZestDevice->log_path.str, "Flagging for enumerate portability on MACOS");
    create_info.flags = VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
#endif

    zest__vk_get_required_extensions();
    zest_uint extension_count = zest_vec_size(ZestDevice->extensions);
    create_info.enabledExtensionCount = extension_count;
    create_info.ppEnabledExtensionNames = (const char **)ZestDevice->extensions;

    VkDebugUtilsMessengerCreateInfoEXT debug_create_info = { 0 };
    VkValidationFeatureEnableEXT *enabled_validation_features = 0;
    if (zest__validation_layers_are_enabled()) {
        create_info.enabledLayerCount = (zest_uint)zest__validation_layer_count;
        create_info.ppEnabledLayerNames = zest_validation_layers;
        debug_create_info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
        debug_create_info.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
        debug_create_info.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
        debug_create_info.pfnUserCallback = zest__vk_debug_callback;

        if (zest__validation_layers_with_sync_are_enabled()) {
            zest_vec_push(enabled_validation_features, VK_VALIDATION_FEATURE_ENABLE_SYNCHRONIZATION_VALIDATION_EXT);
        }
        if (zest__validation_layers_with_best_practices_are_enabled()) {
            zest_vec_push(enabled_validation_features, VK_VALIDATION_FEATURE_ENABLE_BEST_PRACTICES_EXT);
        }
        if (zest_vec_size(enabled_validation_features)) {
            // Potentially add others like VK_VALIDATION_FEATURE_ENABLE_BEST_PRACTICES_EXT for more advice
            VkValidationFeaturesEXT validation_features_ext = { 0 };
            validation_features_ext.sType = VK_STRUCTURE_TYPE_VALIDATION_FEATURES_EXT;
            validation_features_ext.enabledValidationFeatureCount = zest_vec_size(enabled_validation_features);
            validation_features_ext.pEnabledValidationFeatures = enabled_validation_features;
            debug_create_info.pNext = &validation_features_ext;
        }

        create_info.pNext = &debug_create_info;
    } else {
        create_info.enabledLayerCount = 0;
    }

    zest_uint extension_property_count = 0;
    vkEnumerateInstanceExtensionProperties(ZEST_NULL, &extension_property_count, ZEST_NULL);

    ZEST__ARRAY(available_extensions, VkExtensionProperties, extension_property_count);

    vkEnumerateInstanceExtensionProperties(ZEST_NULL, &extension_property_count, available_extensions);

    zest_vec_foreach(i, ZestDevice->extensions) {
        ZEST_APPEND_LOG(ZestDevice->log_path.str, "Extension: %s\n", ZestDevice->extensions[i]);
    }

    ZEST_SET_MEMORY_CONTEXT(zest_platform_device, zest_command_instance);
    ZEST_RETURN_FALSE_ON_FAIL(vkCreateInstance(&create_info, &ZestDevice->backend->allocation_callbacks, &ZestDevice->backend->instance));

    ZEST_APPEND_LOG(ZestDevice->log_path.str, "Validating Vulkan Instance");

    ZEST__FREE(available_extensions);
    ZestRenderer->create_window_surface_callback(ZestRenderer->main_window);

    zest_vec_free(enabled_validation_features);

    return ZEST_TRUE;
}

VkResult zest__vk_create_command_buffer_pools() {
    VkCommandBufferAllocateInfo alloc_info = { 0 };
    alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    alloc_info.commandBufferCount = 10;
    zest_ForEachFrameInFlight(fif) {
        zest_vec_foreach(i, ZestDevice->queues) {
            zest_queue queue = ZestDevice->queues[i];
            ZEST_VK_ASSERT_RESULT(zest__vk_create_queue_command_pool(queue->family_index, &queue->backend->command_pool[fif]));
			alloc_info.commandPool = queue->backend->command_pool[fif];
			zest_vec_resize(queue->backend->command_buffers[fif], alloc_info.commandBufferCount);
			ZEST_SET_MEMORY_CONTEXT(zest_platform_renderer, zest_command_command_buffer);
			ZEST_VK_ASSERT_RESULT(vkAllocateCommandBuffers(ZestDevice->backend->logical_device, &alloc_info, queue->backend->command_buffers[fif]));
        }
    }
    return VK_SUCCESS;
}

zest_bool zest__vk_create_logical_device() {
    zest_queue_family_indices indices = { 0 };

    zest_uint queue_family_count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(ZestDevice->backend->physical_device, &queue_family_count, ZEST_NULL);

    zest_vec_resize(ZestDevice->backend->queue_families, queue_family_count);
    //VkQueueFamilyProperties queue_families[10];
    vkGetPhysicalDeviceQueueFamilyProperties(ZestDevice->backend->physical_device, &queue_family_count, ZestDevice->backend->queue_families);

    zest_uint graphics_candidate = ZEST_INVALID;
    zest_uint compute_candidate = ZEST_INVALID;
    zest_uint transfer_candidate = ZEST_INVALID;

	ZEST_APPEND_LOG(ZestDevice->log_path.str, "Iterate available queues:");
    zest_vec_foreach(i, ZestDevice->backend->queue_families) {
        VkQueueFamilyProperties properties = ZestDevice->backend->queue_families[i];

        zest_text_t queue_flags = zest__vk_queue_flags_to_string(properties.queueFlags);
		ZEST_APPEND_LOG(ZestDevice->log_path.str, "Index: %i) %s, Queue count: %i", i, queue_flags.str, properties.queueCount);
        zest_FreeText(&queue_flags);

        // Is it a dedicated transfer queue?
        if ((properties.queueFlags & VK_QUEUE_TRANSFER_BIT) &&
            !(properties.queueFlags & VK_QUEUE_GRAPHICS_BIT) &&
            !(properties.queueFlags & VK_QUEUE_COMPUTE_BIT)) {
			ZEST_APPEND_LOG(ZestDevice->log_path.str, "Found a dedicated transfer queue on index %i", i);
            transfer_candidate = i;
        }

        // Is it a dedicated compute queue?
        if ((properties.queueFlags & VK_QUEUE_COMPUTE_BIT) &&
            !(properties.queueFlags & VK_QUEUE_GRAPHICS_BIT)) {
			ZEST_APPEND_LOG(ZestDevice->log_path.str, "Found a dedicated compute queue on index %i", i);
            compute_candidate = i;
        }
    }

    zest_vec_foreach(i, ZestDevice->backend->queue_families) {
        VkQueueFamilyProperties properties = ZestDevice->backend->queue_families[i];
        // Find the primary graphics queue (must support presentation!)
        VkBool32 present_support = 0;
        vkGetPhysicalDeviceSurfaceSupportKHR(ZestDevice->backend->physical_device, i, ZestRenderer->main_window->backend->surface, &present_support);

        if ((properties.queueFlags & VK_QUEUE_GRAPHICS_BIT) && present_support) {
            if (graphics_candidate == ZEST_INVALID) {
                graphics_candidate = i;
            }
        }

        if (compute_candidate == ZEST_INVALID) {
            if (properties.queueFlags & VK_QUEUE_COMPUTE_BIT) {
				ZEST_APPEND_LOG(ZestDevice->log_path.str, "Set compute queue to index %i", i);
                compute_candidate = i;
            }
        }

        if (transfer_candidate == ZEST_INVALID) {
            if (properties.queueFlags & VK_QUEUE_TRANSFER_BIT) {
				ZEST_APPEND_LOG(ZestDevice->log_path.str, "Set transfer queue to index %i", i);
                transfer_candidate = i;
            }
        }
    }

	if (graphics_candidate == ZEST_INVALID) {
		ZEST_APPEND_LOG(ZestDevice->log_path.str, "Fatal Error: Unable to find graphics queue/present support!");
		return 0;
	}

    float queue_priority = 0.0f;
    VkDeviceQueueCreateInfo queue_create_infos[3];

    int queue_create_count = 0;
    // Graphics queue
    {
        indices.graphics_family_index = graphics_candidate;
        VkDeviceQueueCreateInfo queue_info = { 0 };
        queue_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queue_info.queueFamilyIndex = indices.graphics_family_index;
        queue_info.queueCount = 1;
        queue_info.pQueuePriorities = &queue_priority;
        queue_create_infos[0] = queue_info;
        queue_create_count++;
        ZEST_APPEND_LOG(ZestDevice->log_path.str, "Graphics queue index set to: %i", indices.graphics_family_index);
		zest_map_insert_key(ZestDevice->queue_names, indices.graphics_family_index, "Graphics Queue");
    }

    // Dedicated compute queue
    {
        indices.compute_family_index = compute_candidate;
        if (indices.compute_family_index != indices.graphics_family_index)
        {
            // If compute family index differs, we need an additional queue create info for the compute queue
            VkDeviceQueueCreateInfo queue_info = { 0 };
            queue_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
            queue_info.queueFamilyIndex = indices.compute_family_index;
            queue_info.queueCount = 1;
            queue_info.pQueuePriorities = &queue_priority;
            queue_create_infos[1] = queue_info;
            queue_create_count++;
			zest_map_insert_key(ZestDevice->queue_names, indices.compute_family_index, "Compute Queue");
        }
        ZEST_APPEND_LOG(ZestDevice->log_path.str, "Compute queue index set to: %i", indices.compute_family_index);
    }

    //Dedicated transfer queue
    {
        indices.transfer_family_index = transfer_candidate;
        if (indices.transfer_family_index != indices.graphics_family_index && indices.transfer_family_index != indices.compute_family_index)
        {
            // If transfer_index family index differs, we need an additional queue create info for the transfer queue
            VkDeviceQueueCreateInfo queue_info = { 0 };
            queue_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
            queue_info.queueFamilyIndex = indices.transfer_family_index;
            queue_info.queueCount = 1;
            queue_info.pQueuePriorities = &queue_priority;
            queue_create_infos[2] = queue_info;
            queue_create_count++;
			zest_map_insert_key(ZestDevice->queue_names, indices.transfer_family_index, "Transfer Queue");
        }
        ZEST_APPEND_LOG(ZestDevice->log_path.str, "Transfer queue index set to: %i", indices.transfer_family_index);
    }
    
	zest_map_insert_key(ZestDevice->queue_names, VK_QUEUE_FAMILY_IGNORED, "Ignored");

	// Check for bindless support
    VkPhysicalDeviceDescriptorIndexingFeaturesEXT descriptor_indexing_features = { 0 };
    descriptor_indexing_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES_EXT;
    // No pNext needed for querying this specific struct usually, unless querying other chained features too
    VkPhysicalDeviceFeatures2 physical_device_features2 = { 0 };
    physical_device_features2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
    physical_device_features2.pNext = &descriptor_indexing_features; // Chain it
    vkGetPhysicalDeviceFeatures2(ZestDevice->backend->physical_device, &physical_device_features2);
    if (!descriptor_indexing_features.shaderSampledImageArrayNonUniformIndexing ||
        !descriptor_indexing_features.descriptorBindingPartiallyBound ||
        !descriptor_indexing_features.runtimeDescriptorArray) {
        ZEST_APPEND_LOG(ZestDevice->log_path.str, "Fatal Error: Required descriptor indexing features not supported by this GPU!");
        return 0;
    }

    VkPhysicalDeviceFeatures device_features = { 0 };
    device_features.samplerAnisotropy = VK_TRUE;
    device_features.multiDrawIndirect = VK_TRUE;
    device_features.shaderInt64 = VK_TRUE;
    //device_features.wideLines = VK_TRUE;
    //device_features.dualSrcBlend = VK_TRUE;
    //device_features.vertexPipelineStoresAndAtomics = VK_TRUE;
    if (ZEST__FLAGGED(ZestDevice->setup_info.flags, zest_init_flag_enable_fragment_stores_and_atomics)) device_features.fragmentStoresAndAtomics = VK_TRUE;
    VkPhysicalDeviceVulkan12Features device_features_12 = { 0 };
    device_features_12.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
    //For bindless descriptor sets:
	device_features_12.shaderSampledImageArrayNonUniformIndexing = VK_TRUE;
    device_features_12.shaderStorageImageArrayNonUniformIndexing = VK_TRUE; 
    device_features_12.shaderStorageBufferArrayNonUniformIndexing = VK_TRUE; 
    device_features_12.shaderUniformBufferArrayNonUniformIndexing = VK_TRUE; 
    device_features_12.descriptorBindingPartiallyBound = VK_TRUE; 
    device_features_12.descriptorBindingVariableDescriptorCount = VK_TRUE; 
    device_features_12.runtimeDescriptorArray = VK_TRUE; 
    device_features_12.descriptorBindingUniformBufferUpdateAfterBind = VK_TRUE;
    device_features_12.descriptorBindingSampledImageUpdateAfterBind = VK_TRUE;
    device_features_12.descriptorBindingStorageImageUpdateAfterBind = VK_TRUE;
    device_features_12.descriptorBindingStorageBufferUpdateAfterBind = VK_TRUE;
    device_features_12.descriptorBindingUniformTexelBufferUpdateAfterBind = VK_TRUE;
    device_features_12.descriptorBindingStorageTexelBufferUpdateAfterBind = VK_TRUE;
    device_features_12.bufferDeviceAddress = VK_TRUE;
    device_features_12.timelineSemaphore = VK_TRUE;

    VkDeviceCreateInfo create_info = { 0 };
    create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    create_info.pEnabledFeatures = &device_features;
    create_info.queueCreateInfoCount = queue_create_count;
    create_info.pQueueCreateInfos = queue_create_infos;
    create_info.enabledExtensionCount = zest__required_extension_names_count;
    create_info.ppEnabledExtensionNames = zest_required_extensions;
	create_info.pNext = &device_features_12;

    if (ZEST__FLAGGED(ZestDevice->setup_info.flags, zest_init_flag_enable_validation_layers)) {
        create_info.enabledLayerCount = zest__validation_layer_count;
        create_info.ppEnabledLayerNames = zest_validation_layers;
    }
    else {
        create_info.enabledLayerCount = 0;
    }

    ZEST_APPEND_LOG(ZestDevice->log_path.str, "Creating logical device");
    ZEST_SET_MEMORY_CONTEXT(zest_platform_device, zest_command_logical_device);
    ZEST_VK_LOG(vkCreateDevice(ZestDevice->backend->physical_device, &create_info, &ZestDevice->backend->allocation_callbacks, &ZestDevice->backend->logical_device));

    if (ZestDevice->backend->logical_device == VK_NULL_HANDLE) {
        return ZEST_FALSE;
    }

    ZestDevice->graphics_queue.backend = zest__vk_new_queue_backend();
    ZestDevice->compute_queue.backend = zest__vk_new_queue_backend();
    ZestDevice->transfer_queue.backend = zest__vk_new_queue_backend();

    vkGetDeviceQueue(ZestDevice->backend->logical_device, indices.graphics_family_index, 0, &ZestDevice->graphics_queue.backend->vk_queue);
    vkGetDeviceQueue(ZestDevice->backend->logical_device, indices.compute_family_index, 0, &ZestDevice->compute_queue.backend->vk_queue);
    vkGetDeviceQueue(ZestDevice->backend->logical_device, indices.transfer_family_index, 0, &ZestDevice->transfer_queue.backend->vk_queue);

    ZestDevice->graphics_queue_family_index = indices.graphics_family_index;
    ZestDevice->transfer_queue_family_index = indices.transfer_family_index;
    ZestDevice->compute_queue_family_index = indices.compute_family_index;

    ZestDevice->graphics_queue_family_index = indices.graphics_family_index;
    ZestDevice->transfer_queue_family_index = indices.transfer_family_index;
    ZestDevice->compute_queue_family_index = indices.compute_family_index;

    zest_vec_push(ZestDevice->queues, &ZestDevice->graphics_queue);
    ZestDevice->graphics_queue.family_index = indices.graphics_family_index;
    ZestDevice->graphics_queue.type = zest_queue_graphics;
    if (ZestDevice->graphics_queue.backend->vk_queue != ZestDevice->compute_queue.backend->vk_queue) {
		zest_vec_push(ZestDevice->queues, &ZestDevice->compute_queue);
		ZestDevice->compute_queue.family_index = indices.compute_family_index;
		ZestDevice->compute_queue.type = zest_queue_compute;
    }
    if (ZestDevice->graphics_queue.backend->vk_queue != ZestDevice->transfer_queue.backend->vk_queue) {
		zest_vec_push(ZestDevice->queues, &ZestDevice->transfer_queue);
		ZestDevice->transfer_queue.family_index = indices.transfer_family_index;
		ZestDevice->transfer_queue.type = zest_queue_transfer;
    }

    zest_ForEachFrameInFlight(fif) {
        zest_vec_foreach(i, ZestDevice->queues) {
            zest_queue queue = ZestDevice->queues[i];
            //Create the timeline semaphore pool
            VkSemaphoreTypeCreateInfo timeline_create_info = { 0 };
            timeline_create_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO;
            timeline_create_info.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE;
            timeline_create_info.initialValue = 0;
            VkSemaphoreCreateInfo semaphore_info = { 0 };
            semaphore_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
            semaphore_info.pNext = &timeline_create_info;

			ZEST_SET_MEMORY_CONTEXT(zest_platform_device, zest_command_semaphore);
            vkCreateSemaphore(ZestDevice->backend->logical_device, &semaphore_info, &ZestDevice->backend->allocation_callbacks, &queue->backend->semaphore[fif]);

            queue->current_count[fif] = 0;
        }
    }

    VkCommandPoolCreateInfo cmd_info_pool = { 0 };
    cmd_info_pool.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    cmd_info_pool.queueFamilyIndex = ZestDevice->graphics_queue_family_index;
    cmd_info_pool.flags = 0;    //Maybe needs transient bit?
    ZEST_APPEND_LOG(ZestDevice->log_path.str, "Creating one time command pools");
	ZEST_SET_MEMORY_CONTEXT(zest_platform_device, zest_command_command_pool);
    zest_ForEachFrameInFlight(fif) {
        ZEST_RETURN_FALSE_ON_FAIL(vkCreateCommandPool(ZestDevice->backend->logical_device, &cmd_info_pool, &ZestDevice->backend->allocation_callbacks, &ZestDevice->backend->one_time_command_pool[fif]));
    }

    ZEST_APPEND_LOG(ZestDevice->log_path.str, "Create queue command buffer pools");
    ZEST_RETURN_FALSE_ON_FAIL(zest__vk_create_command_buffer_pools());

    return ZEST_TRUE;
}
// -- End initialisation functions

// -- General_create_functions
zest_bool zest__vk_create_execution_timeline_backend(zest_execution_timeline timeline) {
    VkSemaphoreTypeCreateInfo timeline_create_info = { 0 };
    timeline_create_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO;
    timeline_create_info.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE;
    timeline_create_info.initialValue = 0;
    VkSemaphoreCreateInfo semaphore_info = { 0 };
    semaphore_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    semaphore_info.pNext = &timeline_create_info;
	ZEST_SET_MEMORY_CONTEXT(zest_platform_renderer, zest_command_semaphore);
    timeline->backend = ZEST__NEW(zest_execution_timeline_backend);
    ZEST_RETURN_FALSE_ON_FAIL(vkCreateSemaphore(ZestDevice->backend->logical_device, &semaphore_info, &ZestDevice->backend->allocation_callbacks, &timeline->backend->semaphore));
    return ZEST_TRUE;
}
// -- End General_create_functions

// -- Backend_setup_functions
void *zest__vk_new_device_backend(void) {
    zest_device_backend backend = zloc_Allocate(ZestDevice->allocator, sizeof(zest_device_backend_t));
    *backend = (zest_device_backend_t){ 0 };
    backend->allocation_callbacks.pUserData = &ZestDevice->platform_memory_info;
    backend->allocation_callbacks.pfnAllocation = zest__vk_allocate_callback;
    backend->allocation_callbacks.pfnReallocation = zest__vk_reallocate_callback;
    backend->allocation_callbacks.pfnFree = zest__vk_free_callback;
    return backend;
}

void *zest__vk_new_queue_backend(void) {
    zest_queue_backend backend = ZEST__NEW(zest_queue_backend);
    *backend = (zest_queue_backend_t){ 0 };
    return backend;
}

void *zest__vk_new_submission_batch_backend(void) {
    zest_submission_batch_backend backend = zloc_LinearAllocation(ZestRenderer->frame_graph_allocator[ZEST_FIF], sizeof(zest_submission_batch_backend_t));
    *backend = (zest_submission_batch_backend_t){ 0 };
    return backend;
}

void *zest__vk_new_renderer_backend(void) {
    zest_renderer_backend backend = ZEST__NEW(zest_renderer_backend);
    *backend = (zest_renderer_backend_t){ 0 };
    return backend;
}

void *zest__vk_new_frame_graph_context_backend(void) {
    zest_frame_graph_context_backend_t *backend_context = zloc_LinearAllocation( ZestRenderer->frame_graph_allocator[ZEST_FIF], sizeof(zest_frame_graph_context_backend_t) );
    *backend_context = (zest_frame_graph_context_backend_t){ 0 };
    return backend_context;
}

void *zest__vk_new_swapchain_backend(void) {
    zest_swapchain_backend swapchain_backend = ZEST__NEW(zest_swapchain_backend);
    *swapchain_backend = (zest_swapchain_backend_t){ 0 };
    return swapchain_backend;
}

void *zest__vk_new_buffer_backend(void) {
    zest_buffer_backend buffer_backend = ZEST__NEW(zest_buffer_backend);
    *buffer_backend = (zest_buffer_backend_t){ 0 };
    buffer_backend->magic = zest_INIT_MAGIC(zest_struct_type_buffer_backend);
    return buffer_backend;
}

void *zest__vk_new_uniform_buffer_backend(void) {
    zest_uniform_buffer_backend uniform_buffer_backend = ZEST__NEW(zest_uniform_buffer_backend);
    *uniform_buffer_backend = (zest_uniform_buffer_backend_t){ 0 };
    return uniform_buffer_backend;
}

void zest__vk_set_uniform_buffer_backend(zest_uniform_buffer uniform_buffer) {
    zest_ForEachFrameInFlight(fif) {
        uniform_buffer->backend->descriptor_info[fif].buffer = *zest__vk_get_device_buffer(uniform_buffer->buffer[fif]);
        uniform_buffer->backend->descriptor_info[fif].offset = 0;
        uniform_buffer->backend->descriptor_info[fif].range = uniform_buffer->buffer[fif]->size;
    }
}

void *zest__vk_new_image_backend(void) {
    zest_image_backend image_backend = ZEST__NEW(zest_image_backend);
    *image_backend = (zest_image_backend_t){ 0 };
    return image_backend;
}

void *zest__vk_new_frame_graph_image_backend(zloc_linear_allocator_t *allocator, zest_image node_image, zest_image imported_image) {
    node_image->backend = zloc_LinearAllocation(allocator, sizeof(zest_image_backend_t));
    *node_image->backend = (zest_image_backend_t){ 0 };
	if (imported_image) {
		node_image->backend->vk_format = (VkFormat)imported_image->info.format;
		node_image->backend->vk_image = imported_image->backend->vk_image;
	}
    return node_image->backend;
}

void *zest__vk_new_compute_backend(void) {
    zest_compute_backend compute_backend = ZEST__NEW(zest_compute_backend);
    *compute_backend = (zest_compute_backend_t){ 0 };
    return compute_backend;
}

void *zest__vk_new_set_layout_backend(void) {
    zest_set_layout_backend layout_backend = ZEST__NEW(zest_set_layout_backend);
    *layout_backend = (zest_set_layout_backend_t){ 0 };
    return layout_backend;
}

void *zest__vk_new_descriptor_pool_backend(void) {
    zest_descriptor_pool_backend backend = ZEST__NEW(zest_descriptor_pool_backend);
    *backend = (zest_descriptor_pool_backend_t){ 0 };
    return backend;
}

void *zest__vk_new_sampler_backend(void) {
    zest_sampler_backend backend = ZEST__NEW(zest_sampler_backend);
    *backend = (zest_sampler_backend_t){ 0 };
    return backend;
}

void *zest__vk_new_frame_graph_semaphores_backend(void) {
    zest_frame_graph_semaphores_backend backend = ZEST__NEW(zest_frame_graph_semaphores_backend);
    *backend = (zest_frame_graph_semaphores_backend_t){ 0 };
    return backend;
}

void *zest__vk_new_deferred_destruction_backend(void) {
    zest_deferred_destruction_backend backend = ZEST__NEW(zest_deferred_destruction_backend);
    *backend = (zest_deferred_destruction_backend_t){ 0 };
    return backend;
}

zest_bool zest__vk_finish_compute(zest_compute_builder_t *builder, zest_compute compute) {
    VkPipelineLayoutCreateInfo pipeline_layout_create_info = { 0 };
    if (builder->push_constant_size) {
        compute->backend->push_constant_range.offset = 0;
        compute->backend->push_constant_range.size = builder->push_constant_size;
        compute->backend->push_constant_range.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
        pipeline_layout_create_info.pPushConstantRanges = &compute->backend->push_constant_range;
        pipeline_layout_create_info.pushConstantRangeCount = 1;
    }
    pipeline_layout_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    VkDescriptorSetLayout *set_layouts = 0;
    zest_set_layout bindless_layout = (zest_set_layout)zest__get_store_resource_checked(&ZestRenderer->set_layouts, compute->bindless_layout.value);
    if (bindless_layout) {
        zest_vec_push(set_layouts, bindless_layout->backend->vk_layout);
    }
    zest_vec_foreach(i, builder->non_bindless_layouts) {
        zest_set_layout non_bindless_layout = (zest_set_layout)zest__get_store_resource_checked(&ZestRenderer->set_layouts, builder->non_bindless_layouts[i].value);
        zest_vec_push(set_layouts, non_bindless_layout->backend->vk_layout);
    }
    pipeline_layout_create_info.setLayoutCount = zest_vec_size(set_layouts);
    pipeline_layout_create_info.pSetLayouts = set_layouts;

    ZEST_SET_MEMORY_CONTEXT(zest_platform_renderer, zest_command_pipeline_layout);
    ZEST_CLEANUP_ON_FAIL(vkCreatePipelineLayout(ZestDevice->backend->logical_device, &pipeline_layout_create_info, &ZestDevice->backend->allocation_callbacks, &compute->backend->pipeline_layout));

    // Create compute shader pipeline_templates
    VkComputePipelineCreateInfo compute_pipeline_create_info = { 0 };
    compute_pipeline_create_info.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    compute_pipeline_create_info.layout = compute->backend->pipeline_layout;
    compute_pipeline_create_info.flags = 0;

    // One pipeline for each effect
    ZEST_ASSERT(zest_vec_size(builder->shaders));
    compute->shaders = builder->shaders;
    VkShaderModule shader_module = { 0 };
    zest_vec_foreach(i, compute->shaders) {
        zest_shader_handle shader_handle = compute->shaders[i];
        zest_shader shader = (zest_shader)zest__get_store_resource_checked(&ZestRenderer->shaders, shader_handle.value);

        ZEST_ASSERT(shader->spv);   //Compile the shader first before making the compute pipeline
        VkShaderModule shader_module = 0;
        ZEST_CLEANUP_ON_FAIL(zest__vk_create_shader_module(shader->spv, &shader_module));

        VkPipelineShaderStageCreateInfo compute_shader_stage_info = { 0 };
        compute_shader_stage_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        compute_shader_stage_info.stage = VK_SHADER_STAGE_COMPUTE_BIT;
        compute_shader_stage_info.module = shader_module;
        compute_shader_stage_info.pName = "main";
        compute_pipeline_create_info.stage = compute_shader_stage_info;

        VkPipeline pipeline;
        //If you get validation errors here or even a crash then make sure that you're shader has the correct inputs that match the
        //compute builder set up. For example if the shader expects a push constant range then use zest_SetComputePushConstantSize before
        //calling this function.
        ZEST_SET_MEMORY_CONTEXT(zest_platform_renderer, zest_command_compute_pipeline);
        ZestRenderer->backend->last_result = vkCreateComputePipelines(ZestDevice->backend->logical_device, VK_NULL_HANDLE, 1, &compute_pipeline_create_info, &ZestDevice->backend->allocation_callbacks, &pipeline);
        vkDestroyShaderModule(ZestDevice->backend->logical_device, shader_module, &ZestDevice->backend->allocation_callbacks);
        if (ZestRenderer->backend->last_result != VK_SUCCESS) {
            goto cleanup;
        }
        compute->backend->pipeline = pipeline;
    }
    zest_vec_free(set_layouts);
    return ZEST_TRUE;
cleanup:
    zest_vec_free(set_layouts);
    return ZEST_FALSE;
}

void *zest__vk_new_execution_barriers_backend(zloc_linear_allocator_t *allocator) {
    zest_execution_barriers_backend backend = zloc_LinearAllocation(allocator, sizeof(zest_execution_barriers_backend_t));
    *backend = (zest_execution_barriers_backend_t){ 0 };
    return backend;
}

void *zest__vk_new_shader_resources_backend(void) {
	zest_shader_resources_backend backend = ZEST__NEW(zest_shader_resources_backend);
	*backend = (zest_shader_resources_backend_t) { 0 };
	return backend;
}

void *zest__vk_new_window_backend(void) {
	zest_window_backend backend = ZEST__NEW(zest_window_backend);
	*backend = (zest_window_backend_t) { 0 };
	return backend;
}
// -- End Backend_setup_functions

// -- Backend_cleanup_functions
void zest__vk_cleanup_compute_backend(zest_compute compute) {
	if(compute->backend->pipeline) vkDestroyPipeline(ZestDevice->backend->logical_device, compute->backend->pipeline, &ZestDevice->backend->allocation_callbacks);
    if(compute->backend->pipeline_layout) vkDestroyPipelineLayout(ZestDevice->backend->logical_device, compute->backend->pipeline_layout, &ZestDevice->backend->allocation_callbacks);
    ZEST__FREE(compute->backend);
}

void zest__vk_cleanup_queue_backend(zest_queue queue) {
    zest_ForEachFrameInFlight(fif) {
        vkDestroySemaphore(ZestDevice->backend->logical_device, queue->backend->semaphore[fif], &ZestDevice->backend->allocation_callbacks);
        vkDestroyCommandPool(ZestDevice->backend->logical_device, queue->backend->command_pool[fif], &ZestDevice->backend->allocation_callbacks);
        zest_vec_free(queue->backend->command_buffers[fif]);
    }
    ZEST__FREE(queue->backend);
    queue->backend = 0;
}

void zest__vk_cleanup_swapchain_backend(zest_swapchain swapchain, zest_bool for_recreation) {
    zest_vec_foreach(i, swapchain->views) {
        zest__cleanup_image_view(swapchain->views[i]);
        ZEST__FREE(swapchain->images[i].backend);
    }
    vkDestroySwapchainKHR(ZestDevice->backend->logical_device, swapchain->backend->vk_swapchain, &ZestDevice->backend->allocation_callbacks);
	zest_ForEachFrameInFlight(fif) {
		vkDestroySemaphore(ZestDevice->backend->logical_device, swapchain->backend->vk_image_available_semaphore[fif], &ZestDevice->backend->allocation_callbacks);
	}
	zest_vec_foreach(i, swapchain->backend->vk_render_finished_semaphore) {
		vkDestroySemaphore(ZestDevice->backend->logical_device, swapchain->backend->vk_render_finished_semaphore[i], &ZestDevice->backend->allocation_callbacks);
	}
	zest_vec_free(swapchain->backend->vk_render_finished_semaphore);

    if (!for_recreation) {
        ZEST__FREE(swapchain->backend);
        swapchain->backend = 0;
    }
}

void zest__vk_cleanup_window_backend(zest_window window) {
    vkDestroySurfaceKHR(ZestDevice->backend->instance, window->backend->surface, &ZestDevice->backend->allocation_callbacks);
    ZEST__FREE(window->backend);
    window->backend = 0;
}

void zest__vk_cleanup_buffer_backend(zest_buffer buffer) {
	if (buffer->backend->vk_buffer != VK_NULL_HANDLE && ZEST__NOT_FLAGGED(buffer->memory_pool->flags, zest_memory_pool_flag_single_buffer)) {
		vkDestroyBuffer(ZestDevice->backend->logical_device, buffer->backend->vk_buffer, &ZestDevice->backend->allocation_callbacks);
        buffer->backend->vk_buffer = 0;
	}
    ZEST__FREE(buffer->backend);
    buffer->backend = 0;
}

void zest__vk_cleanup_uniform_buffer_backend(zest_uniform_buffer buffer) {
    ZEST__FREE(buffer->backend);
    buffer->backend = 0;
}

void zest__vk_cleanup_pipeline_backend(zest_pipeline pipeline) {
    vkDestroyPipeline(ZestDevice->backend->logical_device, pipeline->backend->pipeline, &ZestDevice->backend->allocation_callbacks);
    vkDestroyPipelineLayout(ZestDevice->backend->logical_device, pipeline->backend->pipeline_layout, &ZestDevice->backend->allocation_callbacks);
    ZEST__FREE(pipeline->backend);
    pipeline->backend = 0;
}

void zest__vk_cleanup_set_layout_backend(zest_set_layout layout) {
    if (layout->backend) {
        zest_vec_free(layout->backend->layout_bindings);
        if (layout->backend->vk_layout) {
            vkDestroyDescriptorSetLayout(ZestDevice->backend->logical_device, layout->backend->vk_layout, &ZestDevice->backend->allocation_callbacks);
        }
		ZEST__FREE(layout->backend);
		layout->backend = 0;
    }
    if (layout->pool && layout->pool->backend) {
		zest_vec_free(layout->pool->backend->vk_pool_sizes);
        vkDestroyDescriptorPool(ZestDevice->backend->logical_device, layout->pool->backend->vk_descriptor_pool, &ZestDevice->backend->allocation_callbacks);
        ZEST__FREE(layout->pool->backend);
        layout->pool->backend = 0;
    }
}

void zest__vk_cleanup_image_backend(zest_image image) {
    if (!image->backend) {
        return;
    }
	if(image->backend->vk_image) vkDestroyImage(ZestDevice->backend->logical_device, image->backend->vk_image, &ZestDevice->backend->allocation_callbacks);
	image->backend->vk_image = VK_NULL_HANDLE;
    if (ZEST__NOT_FLAGGED(image->info.flags, zest_image_flag_transient)) {
		ZEST__FREE(image->backend);
    }
    image->backend = 0;
}

void zest__vk_cleanup_sampler_backend(zest_sampler sampler) {
    if (!sampler->backend) {
        return;
    }
	if(sampler->backend->vk_sampler) vkDestroySampler(ZestDevice->backend->logical_device, sampler->backend->vk_sampler, &ZestDevice->backend->allocation_callbacks);
	sampler->backend->vk_sampler = VK_NULL_HANDLE;
    ZEST__FREE(sampler->backend);
    sampler->backend = 0;
}

void zest__vk_cleanup_image_view_backend(zest_image_view view) {
    if (!view->backend) {
        return;
    }
    if (view->backend->vk_view) {
        vkDestroyImageView(ZestDevice->backend->logical_device, view->backend->vk_view, &ZestDevice->backend->allocation_callbacks);
    }
}

void zest__vk_cleanup_image_view_array_backend(zest_image_view_array view_array) {
    for (int i = 0; i != view_array->count; ++i) {
        zest_image_view view = &view_array->views[i];
        if (view->backend) {
			vkDestroyImageView(ZestDevice->backend->logical_device, view->backend->vk_view, &ZestDevice->backend->allocation_callbacks);
        }
    }
    view_array->magic = 0;
}

void zest__vk_cleanup_descriptor_backend(zest_set_layout layout, zest_descriptor_set set) {
    vkFreeDescriptorSets(ZestDevice->backend->logical_device, layout->pool->backend->vk_descriptor_pool, 1, &set->backend->vk_descriptor_set);
	ZEST__FREE(set->backend);
    set->backend = 0;
}

void zest__vk_cleanup_shader_resources_backend(zest_shader_resources shader_resource) {
	zest_ForEachFrameInFlight(fif) {
		zest_vec_free(shader_resource->sets[fif]);
	}
	zest_vec_free(shader_resource->backend->binding_sets);
    ZEST__FREE(shader_resource->backend);
}

void zest__vk_cleanup_frame_graph_semaphore(zest_frame_graph_semaphores semaphores) {
	zest_ForEachFrameInFlight(fif) {
		for (int queue_index = 0; queue_index != ZEST_QUEUE_COUNT; ++queue_index) {
			vkDestroySemaphore(ZestDevice->backend->logical_device, semaphores->backend->vk_semaphores[fif][queue_index], &ZestDevice->backend->allocation_callbacks);
		}
	}
    ZEST__FREE(semaphores->backend);
}

void zest__vk_cleanup_render_pass(zest_render_pass render_pass) {
	vkDestroyRenderPass(ZestDevice->backend->logical_device, render_pass->backend->vk_render_pass, &ZestDevice->backend->allocation_callbacks);
    ZEST__FREE(render_pass->backend);
}

void zest__vk_cleanup_device_backend(void) {
    if (ZestDevice->backend->debug_messenger != VK_NULL_HANDLE) {
        PFN_vkDestroyDebugUtilsMessengerEXT destroyDebugUtilsMessenger =
            (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(ZestDevice->backend->instance, "vkDestroyDebugUtilsMessengerEXT");
        if (destroyDebugUtilsMessenger) {
            destroyDebugUtilsMessenger(ZestDevice->backend->instance, ZestDevice->backend->debug_messenger, &ZestDevice->backend->allocation_callbacks);
        }
        ZestDevice->backend->debug_messenger = VK_NULL_HANDLE;
    }
    zest_ForEachFrameInFlight(fif) {
        vkDestroyCommandPool(ZestDevice->backend->logical_device, ZestDevice->backend->one_time_command_pool[fif], &ZestDevice->backend->allocation_callbacks);
    }
    vkDestroyDevice(ZestDevice->backend->logical_device, &ZestDevice->backend->allocation_callbacks);
    vkDestroyInstance(ZestDevice->backend->instance, &ZestDevice->backend->allocation_callbacks);
    zest_vec_free(ZestDevice->backend->queue_families);
    ZEST__FREE(ZestDevice->backend);
}

void zest__vk_cleanup_renderer_backend(void) {
    vkDestroyPipelineCache(ZestDevice->backend->logical_device, ZestRenderer->backend->pipeline_cache, &ZestDevice->backend->allocation_callbacks);

	for (zest_uint queue_index = 0; queue_index != ZEST_QUEUE_COUNT; ++queue_index) {
		zest_ForEachFrameInFlight(i) {
            vkDestroyFence(ZestDevice->backend->logical_device, ZestRenderer->backend->fif_fence[i][queue_index], &ZestDevice->backend->allocation_callbacks);
        }
		vkDestroyFence(ZestDevice->backend->logical_device, ZestRenderer->backend->intraframe_fence[queue_index], &ZestDevice->backend->allocation_callbacks);
    }

    zest_vec_foreach(i, ZestRenderer->timeline_semaphores) {
        zest_execution_timeline timeline = ZestRenderer->timeline_semaphores[i];
        vkDestroySemaphore(ZestDevice->backend->logical_device, timeline->backend->semaphore, &ZestDevice->backend->allocation_callbacks);
        ZEST__FREE(timeline);
    }
    zest_vec_free(ZestRenderer->timeline_semaphores);
    zest_vec_free(ZestRenderer->backend->used_buffers_ready_for_freeing);
    ZEST__FREE(ZestRenderer->backend);
}
// -- End Backend_cleanup_functions

// -- Fences
ZEST_PRIVATE zest_fence_status zest__vk_wait_for_renderer_fences(void) {
	zest_uint count = ZestRenderer->fence_count[ZEST_FIF];
	if (count == 0) {
		return zest_fence_status_success;
	}

	VkFence *fences = ZestRenderer->backend->fif_fence[ZEST_FIF];
	VkResult result = vkWaitForFences( ZestDevice->backend->logical_device, count, fences, VK_TRUE, ZestApp->fence_wait_timeout_ns);

	if (result == VK_SUCCESS) {
		return zest_fence_status_success;
	} else if (result == VK_TIMEOUT) {
		return zest_fence_status_timeout;
	} 
	return zest_fence_status_error;
}

ZEST_PRIVATE zest_bool zest__vk_reset_renderer_fences(void) {
	zest_uint count = ZestRenderer->fence_count[ZEST_FIF];
	VkFence *fences = ZestRenderer->backend->fif_fence[ZEST_FIF];
	ZEST_RETURN_FALSE_ON_FAIL(vkResetFences(ZestDevice->backend->logical_device, count, fences));
	return ZEST_TRUE;
}
// -- End Fences

// -- Command_pools
void zest__vk_reset_queue_command_pool(zest_queue queue) {
    vkResetCommandPool(ZestDevice->backend->logical_device, queue->backend->command_pool[ZEST_FIF], 0);
}
// -- End Command_pools

// -- Buffer_and_memory
void *zest__vk_new_memory_pool_backend(void) {
    zest_device_memory_pool_backend backend = ZEST__NEW(zest_device_memory_pool_backend);
    *backend = (zest_device_memory_pool_backend_t){ 0 };
    return backend;
}

zest_bool zest__vk_map_memory(zest_device_memory_pool memory_allocation, zest_size size, zest_size offset) {
    return vkMapMemory(ZestDevice->backend->logical_device, memory_allocation->backend->memory, offset, size, 0, &memory_allocation->mapped);
}

void zest__vk_unmap_memory(zest_device_memory_pool memory_allocation) {
	vkUnmapMemory(ZestDevice->backend->logical_device, memory_allocation->backend->memory);
}

zest_bool zest__vk_create_buffer_memory_pool(zest_size size, zest_buffer_info_t *buffer_info, zest_device_memory_pool memory_pool, const char* name) {
    VkBufferCreateInfo create_buffer_info = { 0 };
    create_buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    create_buffer_info.size = size;
    create_buffer_info.usage = buffer_info->buffer_usage_flags;
    create_buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    create_buffer_info.flags = 0;

    VkBuffer temp_buffer;
	ZEST_SET_MEMORY_CONTEXT(zest_platform_renderer, zest_command_buffer);
    ZEST_VK_ASSERT_RESULT(vkCreateBuffer(ZestDevice->backend->logical_device, &create_buffer_info, &ZestDevice->backend->allocation_callbacks, &temp_buffer));

    VkMemoryRequirements memory_requirements;
    vkGetBufferMemoryRequirements(ZestDevice->backend->logical_device, temp_buffer, &memory_requirements);

    VkMemoryAllocateFlagsInfo flags;
    flags.deviceMask = 0;
    flags.flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT;
    flags.pNext = NULL;
    flags.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO;

    VkMemoryAllocateInfo alloc_info = { 0 };
    alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    alloc_info.allocationSize = memory_requirements.size;
    alloc_info.memoryTypeIndex = zest__vk_find_memory_type(memory_requirements.memoryTypeBits, buffer_info->property_flags);
    ZEST_ASSERT(alloc_info.memoryTypeIndex != ZEST_INVALID);
    if (zest__validation_layers_are_enabled() && ZestDevice->api_version == VK_API_VERSION_1_2) {
        alloc_info.pNext = &flags;
    }
    ZEST_APPEND_LOG(ZestDevice->log_path.str, "Allocating buffer memory pool, size: %llu type: %i, alignment: %llu, type bits: %i", alloc_info.allocationSize, alloc_info.memoryTypeIndex, memory_requirements.alignment, memory_requirements.memoryTypeBits);
	ZEST_SET_MEMORY_CONTEXT(zest_platform_renderer, zest_command_allocate_memory_pool);
    ZEST_VK_ASSERT_RESULT(vkAllocateMemory(ZestDevice->backend->logical_device, &alloc_info, &ZestDevice->backend->allocation_callbacks, &memory_pool->backend->memory));

    memory_pool->size = memory_requirements.size;
    memory_pool->alignment = memory_requirements.alignment;
    memory_pool->minimum_allocation_size = ZEST__MAX(memory_pool->alignment, memory_pool->minimum_allocation_size);
    memory_pool->memory_type_index = alloc_info.memoryTypeIndex;
    memory_pool->backend->property_flags = buffer_info->property_flags;
    memory_pool->backend->image_usage_flags = (VkImageUsageFlags)buffer_info->image_usage_flags;
    memory_pool->backend->buffer_info = create_buffer_info;

    if (ZEST__FLAGGED(create_buffer_info.flags, zest_memory_pool_flag_single_buffer)) {
        vkDestroyBuffer(ZestDevice->backend->logical_device, temp_buffer, &ZestDevice->backend->allocation_callbacks);
    } else {
        memory_pool->backend->vk_buffer = temp_buffer;
        vkBindBufferMemory(ZestDevice->backend->logical_device, memory_pool->backend->vk_buffer, memory_pool->backend->memory, 0);
    }
    return ZEST_TRUE;
}

zest_bool zest__vk_create_image_memory_pool(zest_size size_in_bytes, zest_buffer_info_t *buffer_info, zest_device_memory_pool buffer) {
    VkMemoryAllocateInfo alloc_info = { 0 };
    alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    alloc_info.allocationSize = size_in_bytes;
    alloc_info.memoryTypeIndex = zest__vk_find_memory_type(buffer_info->memory_type_bits, buffer_info->property_flags);
    ZEST_ASSERT(alloc_info.memoryTypeIndex != ZEST_INVALID);

    buffer->size = size_in_bytes;
    buffer->alignment = buffer_info->alignment;
    buffer->minimum_allocation_size = ZEST__MAX(buffer->alignment, buffer->minimum_allocation_size);
    buffer->memory_type_index = alloc_info.memoryTypeIndex;
    buffer->backend->property_flags = buffer_info->property_flags;
    buffer->backend->image_usage_flags = 0;

    ZEST_APPEND_LOG(ZestDevice->log_path.str, "Allocating image memory pool, size: %llu type: %i, alignment: %llu, type bits: %i", alloc_info.allocationSize, alloc_info.memoryTypeIndex, buffer_info->alignment, buffer_info->memory_type_bits);
	ZEST_SET_MEMORY_CONTEXT(zest_platform_renderer, zest_command_allocate_memory_pool);
    ZEST_RETURN_FALSE_ON_FAIL(vkAllocateMemory(ZestDevice->backend->logical_device, &alloc_info, &ZestDevice->backend->allocation_callbacks, &buffer->backend->memory));

    return ZEST_TRUE;
}

void zest__vk_cleanup_memory_pool_backend(zest_device_memory_pool memory_allocation) {
    if (memory_allocation->backend->vk_buffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(ZestDevice->backend->logical_device, memory_allocation->backend->vk_buffer, &ZestDevice->backend->allocation_callbacks);
    }
    if (memory_allocation->backend->memory) {
        vkFreeMemory(ZestDevice->backend->logical_device, memory_allocation->backend->memory, &ZestDevice->backend->allocation_callbacks);
    }
    ZEST__FREE(memory_allocation->backend);
}

void zest__vk_set_buffer_backend_details(zest_buffer buffer) {
    if (ZEST__NOT_FLAGGED(buffer->memory_pool->flags, zest_memory_pool_flag_single_buffer) && buffer->memory_pool->backend->buffer_info.size > 0) {
        VkBufferCreateInfo create_buffer_info = buffer->memory_pool->backend->buffer_info;
        create_buffer_info.size = buffer->size;
		ZEST_SET_MEMORY_CONTEXT(zest_platform_renderer, zest_command_buffer);
		vkCreateBuffer(ZestDevice->backend->logical_device, &create_buffer_info, &ZestDevice->backend->allocation_callbacks, &buffer->backend->vk_buffer);
		vkBindBufferMemory(ZestDevice->backend->logical_device, buffer->backend->vk_buffer, buffer->memory_pool->backend->memory, buffer->memory_offset);
        //As this buffer has it's own VkBuffer we set the buffer_offset to 0. Otherwise the offset needs to be the sub allocation
        //within the pool.
        buffer->buffer_offset = 0;
    } else {
		buffer->backend->vk_buffer = buffer->memory_pool->backend->vk_buffer;
        buffer->buffer_offset = buffer->memory_offset;
    }
    buffer->backend->last_stage_mask = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    buffer->backend->last_access_mask = VK_ACCESS_NONE;
}

void zest__vk_flush_used_buffers(void) {
    zest_vec_foreach(i, ZestRenderer->backend->used_buffers_ready_for_freeing) {
        VkBuffer buffer = ZestRenderer->backend->used_buffers_ready_for_freeing[i];
        vkDestroyBuffer(ZestDevice->backend->logical_device, buffer, &ZestDevice->backend->allocation_callbacks);
    }
}

void zest__vk_cmd_copy_buffer_one_time(zest_buffer src_buffer, zest_buffer dst_buffer, zest_size size) {
    VkBufferCopy copyInfo = { 0 };
    copyInfo.srcOffset = src_buffer->buffer_offset;
    copyInfo.dstOffset = dst_buffer->buffer_offset;
    copyInfo.size = size;
    vkCmdCopyBuffer(ZestRenderer->backend->one_time_command_buffer, src_buffer->backend->vk_buffer, dst_buffer->backend->vk_buffer, 1, &copyInfo);
}

void zest__vk_push_buffer_for_freeing(zest_buffer buffer) {
	if ((buffer->magic & 0xFFFF) == ZEST_STRUCT_IDENTIFIER && ZEST__NOT_FLAGGED(buffer->memory_pool->flags, zest_memory_pool_flag_single_buffer) && buffer->backend->vk_buffer) {
		zest_vec_push(ZestRenderer->backend->used_buffers_ready_for_freeing, buffer->backend->vk_buffer);
		buffer->backend->vk_buffer = VK_NULL_HANDLE;
	}
}

zest_access_flags zest__vk_get_buffer_last_access_mask(zest_buffer buffer) {
	return (zest_access_flags)buffer->backend->last_access_mask;
}
// -- End Buffer_and_memory

// -- Descriptor_sets
inline VkDescriptorType zest__vk_get_descriptor_type(zest_descriptor_type type) {
    switch (type) {
    case zest_descriptor_type_sampler: return VK_DESCRIPTOR_TYPE_SAMPLER;
    case zest_descriptor_type_sampled_image: return VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    case zest_descriptor_type_storage_image: return VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    case zest_descriptor_type_uniform_buffer: return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    case zest_descriptor_type_storage_buffer: return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    default:
        ZEST_ASSERT(ZEST_FALSE);  //Unknown descriptor type
        return VK_DESCRIPTOR_TYPE_MAX_ENUM; 
    }
}

zest_bool zest__vk_create_set_layout(zest_set_layout_builder_t *builder, zest_set_layout layout, zest_bool is_bindless) {
    VkDescriptorSetLayoutCreateInfo layoutInfo = { 0 };
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = zest_vec_size(builder->bindings);
    VkDescriptorSetLayoutBinding *bindings = 0;
    VkDescriptorBindingFlags *binding_flag_list = 0;
    zest_vec_resize(bindings, layoutInfo.bindingCount);
    zest_vec_resize(binding_flag_list, layoutInfo.bindingCount);

    layoutInfo.pBindings = bindings;

    zest_vec_foreach(i, builder->bindings) {
        zest_descriptor_binding_desc_t *desc = &builder->bindings[i];
        VkDescriptorSetLayoutBinding binding = { 0 };
        binding.binding = desc->binding;
        binding.stageFlags = zest__to_vk_shader_stage(desc->stages);
        binding.descriptorType = zest__vk_get_descriptor_type(desc->type);
        binding.descriptorCount = desc->count;
        bindings[i] = binding;
		VkDescriptorBindingFlags binding_flags = VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT | VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT;
        binding_flag_list[i] = binding_flags;
    }

    if (is_bindless) {
        VkDescriptorSetLayoutBindingFlagsCreateInfo binding_flags_create_info = { 0 };
        binding_flags_create_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO;
        binding_flags_create_info.bindingCount = layoutInfo.bindingCount;
        binding_flags_create_info.pBindingFlags = binding_flag_list;

        layoutInfo.flags = is_bindless ? VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT : 0;
        layoutInfo.pNext = &binding_flags_create_info;
    }

	ZEST_SET_MEMORY_CONTEXT(zest_platform_renderer, zest_command_descriptor_layout);
    VkResult result = vkCreateDescriptorSetLayout(ZestDevice->backend->logical_device, &layoutInfo, &ZestDevice->backend->allocation_callbacks, &layout->backend->vk_layout);
	zest_vec_free(binding_flag_list);
	ZestRenderer->backend->last_result = result;
    if (result != VK_SUCCESS) {
        zest_vec_free(bindings);
        return ZEST_FALSE;
    }

    zest_uint count = zest_vec_size(layout->backend->layout_bindings);
    zest_uint size_of_binding = sizeof(VkDescriptorSetLayoutBinding);
    zest_uint size_in_bytes = zest_vec_size_in_bytes(builder->bindings);
	layout->backend->layout_bindings = bindings;
    return ZEST_TRUE;
}

zest_bool zest__vk_create_set_pool(zest_set_layout layout, zest_uint max_set_count, zest_bool bindless) {
    zest_hash_map(zest_uint) type_counts_map;
    type_counts_map type_counts = { 0 };
    zest_vec_foreach(i, layout->backend->layout_bindings) {
        VkDescriptorSetLayoutBinding *binding = &layout->backend->layout_bindings[i];
        bool is_purely_immutable_sampler = (binding->descriptorType == VK_DESCRIPTOR_TYPE_SAMPLER && binding->pImmutableSamplers != NULL);

        if (!is_purely_immutable_sampler) {
            if (!zest_map_valid_key(type_counts, (zest_key)binding->descriptorType)) {
                zest_map_insert_key(type_counts, (zest_key)binding->descriptorType, binding->descriptorCount);
            } else {
                zest_uint *count = zest_map_at_key(type_counts, (zest_key)binding->descriptorType);
                *count += binding->descriptorCount;
            }
        }
    }

    VkDescriptorPoolSize *pool_sizes = NULL;
    zest_map_foreach(i, type_counts) {
        VkDescriptorPoolSize new_pool_size = { 0 };
        new_pool_size.type = (VkDescriptorType)type_counts.map[i].key;
        if (bindless) {
            new_pool_size.descriptorCount = type_counts.data[i];
        } else {
            new_pool_size.descriptorCount = type_counts.data[i] * max_set_count;
        }
        zest_vec_push(pool_sizes, new_pool_size);
    }
    zest_map_free(type_counts); // Free the map now that we're done with it

    if (zest_vec_empty(pool_sizes)) {
        ZEST_ASSERT(0); // Layout resulted in no descriptors for the pool
        return ZEST_FALSE;
    }

    VkDescriptorPoolCreateInfo pool_create_info = { 0 };
    pool_create_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;

    if (bindless) {
        pool_create_info.flags = VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT;
    } else {
        pool_create_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    }
    pool_create_info.maxSets = bindless ? 1 : max_set_count; // Correct for a single global bindless set
    pool_create_info.poolSizeCount = zest_vec_size(pool_sizes);
    pool_create_info.pPoolSizes = pool_sizes;

    zest_descriptor_pool pool = zest__create_descriptor_pool(pool_create_info.maxSets);
    ZEST_SET_MEMORY_CONTEXT(zest_platform_renderer, zest_command_descriptor_pool);
    VkResult result = vkCreateDescriptorPool(ZestDevice->backend->logical_device, &pool_create_info, &ZestDevice->backend->allocation_callbacks,
        &pool->backend->vk_descriptor_pool);

    zest_vec_free(pool_sizes); // Free the temporary pool_sizes vector

    if (result != VK_SUCCESS) {
        pool->backend->vk_descriptor_pool = VK_NULL_HANDLE;
        ZEST__FREE(pool->backend);
        ZEST__FREE(pool);
        ZEST_VK_PRINT_RESULT(result);
        return ZEST_FALSE; // 4. Add return on failure
    }

    layout->pool = pool;
    return ZEST_TRUE; // 4. Add return on success
}

zest_descriptor_set zest__vk_create_bindless_set(zest_set_layout layout) {
    ZEST_ASSERT(layout->backend->vk_layout != VK_NULL_HANDLE && "VkDescriptorSetLayout in wrapper is null.");
    ZEST_ASSERT(layout->pool->backend->vk_descriptor_pool != VK_NULL_HANDLE && "VkDescriptorPool in wrapper is null.");
    VkDescriptorSetAllocateInfo alloc_info = { 0 };
    alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    alloc_info.descriptorPool = layout->pool->backend->vk_descriptor_pool;
    alloc_info.descriptorSetCount = 1; // Maybe add this as a parameter at some point if it's needed
    alloc_info.pSetLayouts = &layout->backend->vk_layout;

    zest_descriptor_set set = ZEST__NEW(zest_descriptor_set);
    *set = (zest_descriptor_set_t){ 0 };
    set->backend = ZEST__NEW(zest_descriptor_set_backend);
    *set->backend = (zest_descriptor_set_backend_t){ 0 };
    set->magic = zest_INIT_MAGIC(zest_struct_type_descriptor_set);
    VkResult result = vkAllocateDescriptorSets(ZestDevice->backend->logical_device, &alloc_info, &set->backend->vk_descriptor_set);

    if (result != VK_SUCCESS) {
        ZEST_VK_PRINT_RESULT(result);
        ZEST__FREE(set);
        return 0;
    }
    return set;
}

void zest__vk_update_bindless_image_descriptor(zest_uint binding_number, zest_uint array_index, zest_descriptor_type type, zest_image image, zest_image_view view, zest_sampler sampler, zest_descriptor_set set) {
    VkWriteDescriptorSet write = { 0 };
    VkDescriptorImageInfo image_info = { 0 };
    image_info.imageLayout = image ? image->backend->vk_current_layout : VK_IMAGE_LAYOUT_UNDEFINED;
    image_info.imageView = view ? view->backend->vk_view : VK_NULL_HANDLE;
    image_info.sampler = sampler ? sampler->backend->vk_sampler : VK_NULL_HANDLE;
    VkDescriptorType descriptor_type = zest__vk_get_descriptor_type(type);
    write = zest__vk_create_image_descriptor_write_with_type(set->backend->vk_descriptor_set, &image_info, binding_number, descriptor_type);
    write.dstArrayElement = array_index;
    vkUpdateDescriptorSets(ZestDevice->backend->logical_device, 1, &write, 0, 0);
}

void zest__vk_update_bindless_buffer_descriptor(zest_uint binding_number, zest_uint array_index, zest_buffer buffer, zest_descriptor_set set) {
    VkDescriptorBufferInfo buffer_info = zest__vk_get_buffer_info(buffer);

    VkWriteDescriptorSet write = zest__vk_create_buffer_descriptor_write_with_type(set->backend->vk_descriptor_set, &buffer_info, binding_number, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
    write.dstArrayElement = array_index;
    vkUpdateDescriptorSets(ZestDevice->backend->logical_device, 1, &write, 0, 0);
}

zest_bool zest__vk_create_uniform_descriptor_set(zest_uniform_buffer buffer, zest_set_layout associated_layout) {
    VkDescriptorSet target_sets[ZEST_MAX_FIF];
    VkDescriptorSetLayout layouts[ZEST_MAX_FIF];
    VkWriteDescriptorSet writes[ZEST_MAX_FIF] = { 0 };
    zest_ForEachFrameInFlight(fif) {
        if (ZEST_VALID_HANDLE(buffer->descriptor_set[fif])) {
            ZEST_ASSERT(0);     //descriptor sets for uniform buffer already exist! This function should only be called
                                //when the uniform buffer is created.
        }
		buffer->descriptor_set[fif] = ZEST__NEW(zest_descriptor_set);
		*buffer->descriptor_set[fif] = (zest_descriptor_set_t){0};
        buffer->descriptor_set[fif]->magic = zest_INIT_MAGIC(zest_struct_type_descriptor_set);
		buffer->descriptor_set[fif]->backend = ZEST__NEW(zest_descriptor_set_backend);
		*buffer->descriptor_set[fif]->backend = (zest_descriptor_set_backend_t){ 0 };
        target_sets[fif] = VK_NULL_HANDLE;
        layouts[fif] = associated_layout->backend->vk_layout;
    }

    zest_descriptor_pool pool = associated_layout->pool;

	VkDescriptorSetAllocateInfo allocation_info = { 0 };
	allocation_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	allocation_info.descriptorPool = pool->backend->vk_descriptor_pool;
	allocation_info.descriptorSetCount = ZEST_MAX_FIF;
	allocation_info.pSetLayouts = layouts; 

	VkResult result = vkAllocateDescriptorSets(ZestDevice->backend->logical_device, &allocation_info, target_sets);
	if (result != VK_SUCCESS) {
		ZEST_VK_PRINT_RESULT(result);
		return ZEST_FALSE;
	}

    zest_ForEachFrameInFlight(fif) {
        writes[fif].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[fif].dstBinding = 0;
        writes[fif].dstArrayElement = 0;
        writes[fif].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        writes[fif].descriptorCount = 1;
        writes[fif].dstSet = target_sets[fif];
        writes[fif].pBufferInfo = &buffer->backend->descriptor_info[fif];
    }

	vkUpdateDescriptorSets( ZestDevice->backend->logical_device, ZEST_MAX_FIF, writes, 0, NULL);

    zest_ForEachFrameInFlight(fif) {
        buffer->descriptor_set[fif]->backend->vk_descriptor_set = target_sets[fif];
    }

    return ZEST_TRUE;
}
// -- End Descriptor_sets

// -- General_renderer
void zest__vk_set_depth_format(void) {
    VkFormat depth_format = zest__vk_find_depth_format();
    ZestRenderer->depth_format = zest__from_vk_format(depth_format);
}

zest_bool zest__vk_initialise_renderer_backend(void) {
    VkFenceCreateInfo fence_info = { 0 };
    fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fence_info.flags = 0;
	ZEST_SET_MEMORY_CONTEXT(zest_platform_renderer, zest_command_fence);
	for (zest_uint queue_index = 0; queue_index != ZEST_QUEUE_COUNT; ++queue_index) {
		zest_ForEachFrameInFlight(i) {
            ZEST_RETURN_FALSE_ON_FAIL(vkCreateFence(ZestDevice->backend->logical_device, &fence_info, &ZestDevice->backend->allocation_callbacks, &ZestRenderer->backend->fif_fence[i][queue_index]));
			ZestRenderer->fence_count[i] = 0;
        }
		ZEST_RETURN_FALSE_ON_FAIL(vkCreateFence(ZestDevice->backend->logical_device, &fence_info, &ZestDevice->backend->allocation_callbacks, &ZestRenderer->backend->intraframe_fence[queue_index]));
    }

    VkCommandBufferAllocateInfo alloc_info = { 0 };
    alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    alloc_info.commandBufferCount = 1;
	ZEST_SET_MEMORY_CONTEXT(zest_platform_renderer, zest_command_command_buffer);
    zest_ForEachFrameInFlight(fif) {
		alloc_info.commandPool = ZestDevice->backend->one_time_command_pool[fif];
        ZEST_RETURN_FALSE_ON_FAIL(vkAllocateCommandBuffers(ZestDevice->backend->logical_device, &alloc_info, &ZestRenderer->backend->utility_command_buffer[fif]));
    }

	VkPipelineCacheCreateInfo pipeline_cache_create_info = { 0 };
	pipeline_cache_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
	ZEST_SET_MEMORY_CONTEXT(zest_platform_renderer, zest_command_pipeline_cache);
	ZEST_RETURN_FALSE_ON_FAIL(vkCreatePipelineCache(ZestDevice->backend->logical_device, &pipeline_cache_create_info, &ZestDevice->backend->allocation_callbacks, &ZestRenderer->backend->pipeline_cache));

    return ZEST_TRUE;
}

void zest__vk_wait_for_idle_device(void) {
	vkDeviceWaitIdle(ZestDevice->backend->logical_device); 
}

zest_sample_count_flags zest__vk_get_msaa_sample_count(void) {
	return (zest_sample_count_flags)ZestDevice->backend->msaa_samples;
}
// -- End General_renderer

// -- Device_OS
void zest__vk_os_add_platform_extensions(void) {
    zest_AddInstanceExtension(VK_KHR_SURFACE_EXTENSION_NAME);
    zest_AddInstanceExtension(VK_KHR_WIN32_SURFACE_EXTENSION_NAME);
}
// -- End Device_OS

// -- Pipelines
void *zest__vk_new_pipeline_backend(void) {
    zest_pipeline_backend backend = ZEST__NEW(zest_pipeline_backend);
    *backend = (zest_pipeline_backend_t){ 0 };
    return backend;
}

zest_bool zest__vk_build_pipeline(zest_pipeline pipeline, zest_render_pass render_pass) {
    ZEST_SET_MEMORY_CONTEXT(zest_platform_renderer, zest_command_pipeline_layout);

    zloc_linear_allocator_t *scratch = ZestDevice->scratch_arena;

    zest_pipeline_template template = pipeline->pipeline_template;

    VkPushConstantRange push_constant_range = {
        (VkShaderStageFlags)template->push_constant_range.stage_flags,
        template->push_constant_range.offset,
        template->push_constant_range.size,
    };

    VkDescriptorSetLayout *layouts = 0;

    zest_vec_foreach(i, template->set_layouts) {
        zest_set_layout layout = template->set_layouts[i];
        zest_vec_linear_push(scratch, layouts, layout->backend->vk_layout);
    }

    VkPipelineLayoutCreateInfo pipeline_layout_info = { 0 };
	pipeline_layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipeline_layout_info.pushConstantRangeCount = 1;
    pipeline_layout_info.pPushConstantRanges = &push_constant_range;
    pipeline_layout_info.setLayoutCount = zest_vec_size(layouts);
    pipeline_layout_info.pSetLayouts = layouts;

    VkResult result = vkCreatePipelineLayout(ZestDevice->backend->logical_device, &pipeline_layout_info, &ZestDevice->backend->allocation_callbacks, &pipeline->backend->pipeline_layout);
    if (result != VK_SUCCESS) {
        ZEST_VK_PRINT_RESULT(result);
        return ZEST_FALSE;
    }

    VkShaderModule vert_shader_module = { 0 };
    VkShaderModule frag_shader_module = { 0 };
    zest_shader vert_shader = (zest_shader)zest__get_store_resource_checked(&ZestRenderer->shaders, template->vertex_shader.value);
    zest_shader frag_shader = (zest_shader)zest__get_store_resource_checked(&ZestRenderer->shaders, template->fragment_shader.value);
    VkPipelineShaderStageCreateInfo vert_shader_stage_info = { 0 };
    VkPipelineShaderStageCreateInfo frag_shader_stage_info = { 0 };
    if (ZEST_VALID_HANDLE(vert_shader)) {
        if (!vert_shader->spv) {
            ZEST_APPEND_LOG(ZestDevice->log_path.str, "Vertex shader [%s] in pipeline [%s] did not have any spv data, make sure it's compiled.", vert_shader->name.str, template->name);
            result = VK_ERROR_UNKNOWN;
            goto cleanup;
        }
        result = zest__vk_create_shader_module(vert_shader->spv, &vert_shader_module);
        vert_shader_stage_info.module = vert_shader_module;
    }

    if (ZEST_VALID_HANDLE(frag_shader)) {
        if (!frag_shader->spv) {
            ZEST_APPEND_LOG(ZestDevice->log_path.str, "Vertex shader [%s] in pipeline [%s] did not have any spv data, make sure it's compiled.", frag_shader->name.str, template->name);
            result = VK_ERROR_UNKNOWN;
            goto cleanup;
        }
        result = zest__vk_create_shader_module(frag_shader->spv, &frag_shader_module);
        frag_shader_stage_info.module = frag_shader_module;
    }

    if (result != VK_SUCCESS) {
        ZEST_VK_PRINT_RESULT(result);
        goto cleanup;
    }

    vert_shader_stage_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vert_shader_stage_info.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vert_shader_stage_info.pName = template->vertShaderFunctionName;

    frag_shader_stage_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    frag_shader_stage_info.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    frag_shader_stage_info.pName = template->fragShaderFunctionName;

    VkPipelineShaderStageCreateInfo shaderStages[2] = { vert_shader_stage_info, frag_shader_stage_info };

    VkPipelineVertexInputStateCreateInfo vertex_input_info = { 0 };
    vertex_input_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    VkVertexInputBindingDescription *binding_descriptions = 0;
    VkVertexInputAttributeDescription *attribute_descriptions = 0;
    if (!template->no_vertex_input) {
        //If the pipeline is set to have vertex input, then you must add bindingDescriptions. 
        //You can use zest_AddVertexInputBindingDescription for this
        ZEST_ASSERT(zest_vec_size(template->binding_descriptions));
        zest_vec_foreach(i, template->binding_descriptions) {
            VkVertexInputBindingDescription description = {
                template->binding_descriptions[i].binding,
                template->binding_descriptions[i].stride,
                (VkVertexInputRate)template->binding_descriptions[i].input_rate,
            };
            zest_vec_linear_push(scratch, binding_descriptions, description);
        }
        zest_vec_foreach(i, template->attribute_descriptions) {
            VkVertexInputAttributeDescription description = {
                template->attribute_descriptions[i].location,
                template->attribute_descriptions[i].binding,
                (VkFormat)template->attribute_descriptions[i].format,
                template->attribute_descriptions[i].offset,
            };
            zest_vec_linear_push(scratch, attribute_descriptions, description);
        }
        vertex_input_info.vertexBindingDescriptionCount = (zest_uint)zest_vec_size(template->binding_descriptions);
        vertex_input_info.pVertexBindingDescriptions = binding_descriptions;
        vertex_input_info.vertexAttributeDescriptionCount = (zest_uint)zest_vec_size(template->attribute_descriptions);
        vertex_input_info.pVertexAttributeDescriptions = attribute_descriptions;
    }

    VkPipelineInputAssemblyStateCreateInfo input_assembly = { 0 };
    input_assembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    input_assembly.primitiveRestartEnable = VK_FALSE;
    input_assembly.topology = (VkPrimitiveTopology)template->primitive_topology;
    input_assembly.flags = 0;

    VkPipelineViewportStateCreateInfo viewport_state = { 0 };
    viewport_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewport_state.viewportCount = 1;
    viewport_state.pViewports = NULL;
    viewport_state.scissorCount = 1;
    viewport_state.pScissors = NULL;

    VkDynamicState *dynamic_states = 0;
    zest_vec_linear_push(scratch, dynamic_states, VK_DYNAMIC_STATE_VIEWPORT);
    zest_vec_linear_push(scratch, dynamic_states, VK_DYNAMIC_STATE_SCISSOR);

    VkPipelineDynamicStateCreateInfo dynamic_state = { 0 };
    dynamic_state.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamic_state.dynamicStateCount = (zest_uint)(zest_vec_size(dynamic_states));
    dynamic_state.pDynamicStates = dynamic_states;

    VkPipelineMultisampleStateCreateInfo multisampling = { 0 };
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.sampleShadingEnable = VK_FALSE;
    multisampling.rasterizationSamples = render_pass->backend->sample_count;

    VkPipelineRasterizationStateCreateInfo rasterizer = { 0 };
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.cullMode = (VkCullModeFlags)template->rasterization.cull_mode;
    rasterizer.polygonMode = (VkPolygonMode)template->rasterization.polygon_mode;
    rasterizer.frontFace = (VkFrontFace)template->rasterization.front_face;
    rasterizer.lineWidth = template->rasterization.line_width;
    rasterizer.rasterizerDiscardEnable = template->rasterization.rasterizer_discard_enable;
    rasterizer.depthClampEnable = template->rasterization.depth_clamp_enable;
    rasterizer.depthBiasEnable = template->rasterization.depth_bias_enable;
    rasterizer.depthBiasClamp = template->rasterization.depth_bias_clamp;
    rasterizer.depthBiasConstantFactor = template->rasterization.depth_bias_constant_factor;
    rasterizer.depthBiasSlopeFactor = template->rasterization.depth_bias_slope_factor;

    VkPipelineDepthStencilStateCreateInfo depth_stencil = { 0 };
    depth_stencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depth_stencil.depthBoundsTestEnable = template->depth_stencil.depth_bounds_test_enable;
    depth_stencil.depthWriteEnable = template->depth_stencil.depth_write_enable;
    depth_stencil.depthTestEnable = template->depth_stencil.depth_test_enable;
    depth_stencil.stencilTestEnable = template->depth_stencil.stencil_test_enable;
    depth_stencil.depthCompareOp = (VkCompareOp)template->depth_stencil.depth_compare_op;

    VkPipelineColorBlendAttachmentState color_attachment = { 0 };
    color_attachment.blendEnable = template->color_blend_attachment.blend_enable;
    color_attachment.srcColorBlendFactor = (VkBlendFactor)template->color_blend_attachment.src_color_blend_factor;
    color_attachment.dstColorBlendFactor = (VkBlendFactor)template->color_blend_attachment.dst_color_blend_factor;
    color_attachment.colorBlendOp = (VkBlendOp)template->color_blend_attachment.color_blend_op;
    color_attachment.srcAlphaBlendFactor = (VkBlendFactor)template->color_blend_attachment.src_alpha_blend_factor;
    color_attachment.dstAlphaBlendFactor = (VkBlendFactor)template->color_blend_attachment.dst_alpha_blend_factor;
    color_attachment.alphaBlendOp = (VkBlendOp)template->color_blend_attachment.alpha_blend_op;
    color_attachment.colorWriteMask = (VkColorComponentFlags)template->color_blend_attachment.color_write_mask;

    VkPipelineColorBlendStateCreateInfo color_blending = { 0 };
    color_blending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    color_blending.logicOpEnable = VK_FALSE;
    color_blending.logicOp = VK_LOGIC_OP_COPY;
    color_blending.attachmentCount = 1;
    color_blending.pAttachments = &color_attachment;
    color_blending.blendConstants[0] = 0.0f;
    color_blending.blendConstants[1] = 0.0f;
    color_blending.blendConstants[2] = 0.0f;
    color_blending.blendConstants[3] = 0.0f;
    
    VkGraphicsPipelineCreateInfo pipeline_info = { 0 };
    pipeline_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipeline_info.stageCount = 2;
    pipeline_info.pStages = shaderStages;
    pipeline_info.pVertexInputState = &vertex_input_info;
    pipeline_info.pInputAssemblyState = &input_assembly;
    pipeline_info.pViewportState = &viewport_state;
    pipeline_info.pRasterizationState = &rasterizer;
    pipeline_info.pMultisampleState = &multisampling;
    pipeline_info.pColorBlendState = &color_blending;
    pipeline_info.pDepthStencilState = &depth_stencil;
    pipeline_info.layout = pipeline->backend->pipeline_layout;
    pipeline_info.renderPass = render_pass->backend->vk_render_pass;
    pipeline_info.subpass = 0;
    pipeline_info.basePipelineHandle = VK_NULL_HANDLE;
	pipeline_info.pDynamicState = &dynamic_state;

	ZEST_SET_MEMORY_CONTEXT(zest_platform_renderer, zest_command_pipelines);
    result = vkCreateGraphicsPipelines(ZestDevice->backend->logical_device, ZestRenderer->backend->pipeline_cache, 1, &pipeline_info, &ZestDevice->backend->allocation_callbacks, &pipeline->backend->pipeline);
    if (result != VK_SUCCESS) {
        ZEST_VK_PRINT_RESULT(result);
    } else {
        ZEST_APPEND_LOG(ZestDevice->log_path.str, "Built pipeline %s", template->name);
    }

	cleanup:
	vkDestroyShaderModule(ZestDevice->backend->logical_device, frag_shader_module, &ZestDevice->backend->allocation_callbacks);
	vkDestroyShaderModule(ZestDevice->backend->logical_device, vert_shader_module, &ZestDevice->backend->allocation_callbacks);
    zloc_ResetLinearAllocator(scratch);
    return result == VK_SUCCESS ? ZEST_TRUE : ZEST_FALSE;
}

// -- End Pipelines

// -- Images
zest_bool zest__vk_create_image(zest_image image, zest_uint layer_count, zest_sample_count_flags num_samples, zest_image_flags flags) {

    VkImageUsageFlags usage = ZEST__FLAGGED(flags, zest_image_flag_sampled) ? VK_IMAGE_USAGE_SAMPLED_BIT : 0;
    usage |= ZEST__FLAGGED(flags, zest_image_flag_storage) ? VK_IMAGE_USAGE_STORAGE_BIT : 0;
    usage |= ZEST__FLAGGED(flags, zest_image_flag_color_attachment) ? VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT : 0;
    usage |= ZEST__FLAGGED(flags, zest_image_flag_depth_stencil_attachment) ? VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT : 0;
    usage |= ZEST__FLAGGED(flags, zest_image_flag_transfer_src) ? VK_IMAGE_USAGE_TRANSFER_SRC_BIT : 0;
    usage |= ZEST__FLAGGED(flags, zest_image_flag_transfer_dst) ? VK_IMAGE_USAGE_TRANSFER_DST_BIT : 0;
    usage |= ZEST__FLAGGED(flags, zest_image_flag_input_attachment) ? VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT : 0;

    VkMemoryPropertyFlags memory_properties = ZEST__FLAGGED(flags, zest_image_flag_device_local) ? VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT : 0;
    memory_properties |= ZEST__FLAGGED(flags, zest_image_flag_host_visible) ? VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT : 0;
    memory_properties |= ZEST__FLAGGED(flags, zest_image_flag_host_coherent) ? VK_MEMORY_PROPERTY_HOST_COHERENT_BIT : 0;

    VkImageTiling tiling = ZEST__FLAGGED(flags, zest_image_flag_host_visible) ? VK_IMAGE_TILING_LINEAR : VK_IMAGE_TILING_OPTIMAL;

    VkImageCreateFlags create_flags = ZEST__FLAGGED(flags, zest_image_flag_mutable_format) ? VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT : 0;
    create_flags |= ZEST__FLAGGED(flags, zest_image_flag_cubemap) ? VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT : 0;
    create_flags |= ZEST__FLAGGED(flags, zest_image_flag_3d_as_2d_array) ? VK_IMAGE_CREATE_2D_ARRAY_COMPATIBLE_BIT : 0;
    create_flags |= ZEST__FLAGGED(flags, zest_image_flag_disjoint_planes) ? VK_IMAGE_CREATE_DISJOINT_BIT : 0;

    VkImageCreateInfo image_info = { 0 };
    image_info.flags = create_flags;
    image_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    image_info.imageType = VK_IMAGE_TYPE_2D;
    image_info.extent.width = image->info.extent.width;
    image_info.extent.height = image->info.extent.height;
    image_info.extent.depth = 1;
    image_info.mipLevels = image->info.mip_levels;
    image_info.arrayLayers = layer_count;
    image_info.format = (VkFormat)image->info.format;
    image_info.tiling = tiling;
    image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    image_info.usage = usage;
    image_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    image_info.samples = num_samples;

    image->backend->vk_format = image_info.format;
    image->backend->vk_aspect = (VkImageAspectFlags)zest__determine_aspect_flag(image->info.format);
    image->backend->vk_current_layout = VK_IMAGE_LAYOUT_UNDEFINED;
    image->backend->vk_extent = image_info.extent;

    ZEST_SET_MEMORY_CONTEXT(zest_platform_renderer, zest_command_image);
    ZEST_RETURN_FALSE_ON_FAIL(vkCreateImage(ZestDevice->backend->logical_device, &image_info, &ZestDevice->backend->allocation_callbacks, &image->backend->vk_image));

    VkMemoryRequirements memory_requirements;
    vkGetImageMemoryRequirements(ZestDevice->backend->logical_device, image->backend->vk_image, &memory_requirements);

    zest_buffer_info_t buffer_info = { 0 };
    buffer_info.image_usage_flags = usage;
    buffer_info.property_flags = memory_properties;
    buffer_info.memory_type_bits = memory_requirements.memoryTypeBits;
    buffer_info.alignment = memory_requirements.alignment;
    image->buffer = zest_CreateBuffer(memory_requirements.size, &buffer_info);

    if (image->buffer) {
        vkBindImageMemory(ZestDevice->backend->logical_device, image->backend->vk_image, zest__vk_get_buffer_device_memory(image->buffer), image->buffer->memory_offset);
    } else {
        // Destroy the image handle before returning to prevent a leak
        vkDestroyImage(ZestDevice->backend->logical_device, image->backend->vk_image, &ZestDevice->backend->allocation_callbacks);
        return ZEST_FALSE;
    }

    return ZEST_TRUE;
}

int zest__vk_get_image_raw_layout(zest_image image) {
    return (int)image->backend->vk_current_layout;
}

zest_bool inline zest__vk_has_stencil_format(VkFormat format) {
    return format == VK_FORMAT_D32_SFLOAT_S8_UINT
        || format == VK_FORMAT_D24_UNORM_S8_UINT
        || format == VK_FORMAT_D16_UNORM_S8_UINT
        || format == VK_FORMAT_S8_UINT;
}

zest_bool inline zest__vk_has_depth_format(VkFormat format) {
    return format == VK_FORMAT_D32_SFLOAT
        || format == VK_FORMAT_D32_SFLOAT_S8_UINT
        || format == VK_FORMAT_D24_UNORM_S8_UINT
        || format == VK_FORMAT_D16_UNORM
        || format == VK_FORMAT_D16_UNORM_S8_UINT;
}

VkAccessFlags zest__vk_get_access_mask_for_layout(VkImageLayout layout) {
    switch (layout) {
    case VK_IMAGE_LAYOUT_UNDEFINED:
        return 0;
    case VK_IMAGE_LAYOUT_GENERAL:
        return VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
    case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
        return VK_ACCESS_TRANSFER_READ_BIT;
    case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
        return VK_ACCESS_TRANSFER_WRITE_BIT;
    case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
        return VK_ACCESS_SHADER_READ_BIT;
    case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
        return VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT;
    case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
        return VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
    case VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL:
        return VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_SHADER_READ_BIT;
    case VK_IMAGE_LAYOUT_PRESENT_SRC_KHR:
        return VK_ACCESS_MEMORY_READ_BIT;
    default:
        return VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
    }
}

VkPipelineStageFlags zest__vk_get_stage_for_layout(VkImageLayout layout) {
    switch (layout) {
    case VK_IMAGE_LAYOUT_UNDEFINED:
        return VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    case VK_IMAGE_LAYOUT_GENERAL:
        return VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
    case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
    case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
        return VK_PIPELINE_STAGE_TRANSFER_BIT;
    case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
    case VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL:
        return VK_PIPELINE_STAGE_VERTEX_SHADER_BIT
            | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT
            | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
    case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
        return VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
        return VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
    case VK_IMAGE_LAYOUT_PRESENT_SRC_KHR:
        return VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    default:
        return VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    }
}

zest_bool zest__vk_transition_image_layout(zest_image image, zest_image_layout new_layout, zest_uint base_mip_index, zest_uint mip_levels, zest_uint base_array_index, zest_uint layer_count) {
    ZEST_ASSERT(ZestRenderer->backend->one_time_command_buffer);    //You must call begin_single_time_command_buffer before calling this fuction

    VkImageMemoryBarrier barrier = { 0 };
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = image->backend->vk_current_layout;
    barrier.newLayout = (VkImageLayout)new_layout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image->backend->vk_image;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = base_mip_index;
    barrier.subresourceRange.levelCount = mip_levels;
    barrier.subresourceRange.baseArrayLayer = base_array_index;
    barrier.subresourceRange.layerCount = layer_count;
    barrier.srcAccessMask = 0;

    if (zest__vk_has_depth_format(image->info.format)) { 
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
        if (zest__vk_has_stencil_format(image->info.format)) {
            barrier.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
        }
    } else {
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    }

    barrier.srcAccessMask = zest__vk_get_access_mask_for_layout(image->backend->vk_current_layout);
    barrier.dstAccessMask = zest__vk_get_access_mask_for_layout(new_layout);
    VkPipelineStageFlags source_stage = zest__vk_get_stage_for_layout(image->backend->vk_current_layout);
    VkPipelineStageFlags destination_stage = zest__vk_get_stage_for_layout(new_layout);

    vkCmdPipelineBarrier(ZestRenderer->backend->one_time_command_buffer, source_stage, destination_stage, 0, 0, ZEST_NULL, 0, ZEST_NULL, 1, &barrier);

    image->backend->vk_current_layout = (VkImageLayout)new_layout;

    return ZEST_TRUE;
}

zest_bool zest__vk_copy_buffer_regions_to_image(zest_buffer_image_copy_t *regions, zest_buffer buffer, zest_size src_offset, zest_image image) {
	ZEST_ASSERT(ZestRenderer->backend->one_time_command_buffer);	//You must call begin_single_time_command_buffer before calling this fuction

    VkBufferImageCopy *copy_regions = 0;
    zest_vec_reserve(copy_regions, zest_vec_size(regions));
    zest_vec_foreach(i, regions) {
		VkBufferImageCopy copy_region = { 0 };
        copy_region.bufferImageHeight = regions[i].buffer_image_height;
        copy_region.bufferOffset = regions[i].buffer_offset + src_offset;
        copy_region.bufferRowLength = regions[i].buffer_row_length;
        copy_region.imageExtent = (VkExtent3D){ regions[i].image_extent.width, regions[i].image_extent.height, regions[i].image_extent.depth  };
        copy_region.imageOffset = (VkOffset3D){ regions[i].image_offset.x, regions[i].image_offset.y, regions[i].image_offset.z };
        copy_region.imageSubresource.aspectMask = regions[i].image_aspect;
        copy_region.imageSubresource.baseArrayLayer = regions[i].base_array_layer;
        copy_region.imageSubresource.layerCount = regions[i].layer_count;
        copy_region.imageSubresource.mipLevel = regions[i].mip_level;
        zest_vec_push(copy_regions, copy_region);
    }

    vkCmdCopyBufferToImage(ZestRenderer->backend->one_time_command_buffer, buffer->backend->vk_buffer, image->backend->vk_image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, zest_vec_size(copy_regions), copy_regions);

    zest_vec_free(copy_regions);

    return ZEST_TRUE;
}

zest_image_view_t *zest__vk_create_image_view(zest_image image, zest_image_view_type view_type, zest_uint mip_levels_this_view, zest_uint base_mip, zest_uint base_array_index, zest_uint layer_count, zloc_linear_allocator_t *linear_allocator) {
    VkImageViewCreateInfo viewInfo = { 0 };
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = image->backend->vk_image;
    viewInfo.viewType = (VkImageViewType)view_type;
    viewInfo.format = image->backend->vk_format;
    viewInfo.subresourceRange.aspectMask = image->backend->vk_aspect;
    viewInfo.subresourceRange.baseMipLevel = base_mip;
    viewInfo.subresourceRange.levelCount = mip_levels_this_view;
    viewInfo.subresourceRange.baseArrayLayer = base_array_index;
    viewInfo.subresourceRange.layerCount = layer_count;

    zest_size public_array_size = sizeof(zest_image_view_t);
    zest_size backend_array_size = sizeof(zest_image_view_backend_t);
    zest_size total_size = public_array_size + backend_array_size;
    
    void *memory = 0;
    if (!linear_allocator) {
        memory = zest_AllocateMemory(total_size);
    } else {
        memory = zloc_LinearAllocation(linear_allocator, total_size);
    }

    zest_image_view_t *image_view = (zest_image_view_t *)memory;
    zest_image_view_backend_t *backend_array = (zest_image_view_backend_t *)((char *)memory + public_array_size);

    image_view->handle = (zest_image_view_handle){ 0 };
    image_view->image = image;
    image_view->backend = &backend_array[0];

	ZEST_SET_MEMORY_CONTEXT(zest_platform_renderer, zest_command_image_view);
	ZestRenderer->backend->last_result = vkCreateImageView(ZestDevice->backend->logical_device, &viewInfo, &ZestDevice->backend->allocation_callbacks, &image_view->backend->vk_view);
	if (ZestRenderer->backend->last_result != VK_SUCCESS) {
        ZEST__FREE(memory);
        return NULL;
    }

    return image_view;
}

zest_image_view_array zest__vk_create_image_views_per_mip(zest_image image, zest_image_view_type view_type, zest_uint base_array_index, zest_uint layer_count, zloc_linear_allocator_t *linear_allocator) {
    VkImageViewCreateInfo viewInfo = { 0 };
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = image->backend->vk_image;
    viewInfo.viewType = (VkImageViewType)view_type;
    viewInfo.format = image->backend->vk_format;
    viewInfo.subresourceRange.aspectMask = image->backend->vk_aspect;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = base_array_index;
    viewInfo.subresourceRange.layerCount = layer_count;

    zest_size public_array_size = sizeof(zest_image_view_t) * image->info.mip_levels;
    zest_size backend_array_size = sizeof(zest_image_view_backend_t) * image->info.mip_levels;
    zest_size total_size = public_array_size + backend_array_size;

    void *memory = 0;
    zest_image_view_array view_array = 0;
    if (!linear_allocator) {
        memory = zest_AllocateMemory(total_size);
        view_array = ZEST__NEW(zest_image_view_array);
        *view_array = (zest_image_view_array_t){ 0 };
    } else {
        memory = zloc_LinearAllocation(linear_allocator, total_size);
        view_array = zloc_LinearAllocation(linear_allocator, sizeof(zest_image_view_array_t));
        *view_array = (zest_image_view_array_t){ 0 };
    }

    view_array->magic = zest_INIT_MAGIC(zest_struct_type_view_array);
    view_array->views = (zest_image_view_t *)memory;
    view_array->handle = (zest_image_view_array_handle){ 0 };
    view_array->count = image->info.mip_levels;
    zest_image_view_backend_t *backend_array = (zest_image_view_backend_t *)((char *)memory + public_array_size);

    ZEST_SET_MEMORY_CONTEXT(zest_platform_renderer, zest_command_image_view);
    for (int mip_index = 0; mip_index != image->info.mip_levels; ++mip_index) {
		view_array->views[mip_index].image = image;
		view_array->views[mip_index].backend = &backend_array[mip_index];
		viewInfo.subresourceRange.baseMipLevel = mip_index;
        ZestRenderer->backend->last_result = vkCreateImageView(ZestDevice->backend->logical_device, &viewInfo, &ZestDevice->backend->allocation_callbacks, &view_array->views[mip_index].backend->vk_view);
        if (ZestRenderer->backend->last_result != VK_SUCCESS) {
            for (int i = 0; i < mip_index; ++i) {
                vkDestroyImageView(ZestDevice->backend->logical_device, view_array->views[i].backend->vk_view, &ZestDevice->backend->allocation_callbacks);
            }
            ZEST__FREE(memory);
            return NULL;
        }
    }
    return view_array;
}

zest_bool zest__vk_create_sampler(zest_sampler sampler) {
    VkSamplerCreateInfo info = { 0 };
    info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    info.addressModeU     = (VkSamplerAddressMode)sampler->create_info.address_mode_u;
    info.addressModeV     = (VkSamplerAddressMode)sampler->create_info.address_mode_v;
    info.addressModeW     = (VkSamplerAddressMode)sampler->create_info.address_mode_w;
    info.anisotropyEnable = sampler->create_info.anisotropy_enable;
    info.borderColor      = (VkBorderColor)sampler->create_info.border_color;
    info.compareEnable    = sampler->create_info.compare_enable;
    info.compareOp        = (VkCompareOp)sampler->create_info.compare_op;
    info.flags            = 0;
    info.magFilter        = (VkFilter)sampler->create_info.mag_filter;
    info.minFilter        = (VkFilter)sampler->create_info.min_filter;
    info.maxAnisotropy    = sampler->create_info.max_lod;
    info.maxLod           = sampler->create_info.max_lod;
    info.mipLodBias       = sampler->create_info.mip_lod_bias;
    info.mipmapMode       = (VkSamplerMipmapMode)sampler->create_info.mipmap_mode;
	ZEST_SET_MEMORY_CONTEXT(zest_platform_renderer, zest_command_sampler);
    ZEST_RETURN_FALSE_ON_FAIL(vkCreateSampler(ZestDevice->backend->logical_device, &info, &ZestDevice->backend->allocation_callbacks, &sampler->backend->vk_sampler));

    return ZEST_TRUE;
}

zest_bool zest__vk_generate_mipmaps(zest_image image) {
    VkFormatProperties format_properties;
    vkGetPhysicalDeviceFormatProperties(ZestDevice->backend->physical_device,
        image->backend->vk_format, &format_properties);

    if (ZEST__NOT_FLAGGED(format_properties.optimalTilingFeatures,
        VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT)) {
        // Texture image format does not support linear blitting!
        return ZEST_FALSE;
    }

    VkImageMemoryBarrier barrier = { 0 };
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.image = image->backend->vk_image; // Use the correct VkImage handle
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = image->info.layer_count;
    barrier.subresourceRange.levelCount = 1;

    int32_t mip_width = image->info.extent.width;
    int32_t mip_height = image->info.extent.height;

    for (zest_uint i = 1; i < image->info.mip_levels; i++) {
        barrier.subresourceRange.baseMipLevel = i - 1;
        barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

        vkCmdPipelineBarrier(ZestRenderer->backend->one_time_command_buffer,
            VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0,
            0, ZEST_NULL, 0, ZEST_NULL, 1, &barrier);

        VkImageBlit blit = { 0 };
        blit.srcOffsets[0] = (VkOffset3D){ 0, 0, 0 };
        blit.srcOffsets[1] = (VkOffset3D){ mip_width, mip_height, 1 };
        blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        blit.srcSubresource.mipLevel = i - 1;
        blit.srcSubresource.baseArrayLayer = 0;
        blit.srcSubresource.layerCount = image->info.layer_count;
        blit.dstOffsets[0] = (VkOffset3D){ 0, 0, 0 };
        blit.dstOffsets[1] = (VkOffset3D){ mip_width > 1 ? mip_width / 2 : 1, mip_height > 1 ?  mip_height / 2 : 1, 1 };
        blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        blit.dstSubresource.mipLevel = i;
        blit.dstSubresource.baseArrayLayer = 0;
        blit.dstSubresource.layerCount = image->info.layer_count;

        vkCmdBlitImage(ZestRenderer->backend->one_time_command_buffer,
            image->backend->vk_image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            image->backend->vk_image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            1, &blit, VK_FILTER_LINEAR);

        // Transition the mip level that was just used as a source to shader read optimal
        barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

        vkCmdPipelineBarrier(ZestRenderer->backend->one_time_command_buffer,
            VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0,
            0, ZEST_NULL, 0, ZEST_NULL, 1, &barrier);


        if (mip_width > 1) mip_width /= 2;
        if (mip_height > 1) mip_height /= 2;
    }

    // Transition the last mip level from DST_OPTIMAL to SHADER_READ_ONLY_OPTIMAL
    barrier.subresourceRange.baseMipLevel = image->info.mip_levels - 1;
    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    vkCmdPipelineBarrier(ZestRenderer->backend->one_time_command_buffer,
        VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0,
        0, ZEST_NULL, 0, ZEST_NULL, 1, &barrier);

    // Update the image's current layout
    image->backend->vk_current_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    return ZEST_TRUE;
}
// -- End images

// -- General_helpers
zest_bool zest__vk_has_blit_support(VkFormat src_format) {
    VkFormatProperties format_properties;
    vkGetPhysicalDeviceFormatProperties(ZestDevice->backend->physical_device, src_format, &format_properties);
    if (!(format_properties.optimalTilingFeatures & VK_FORMAT_FEATURE_BLIT_SRC_BIT)) {
        return 0;
    }

    // Check if the device supports blitting to linear images
    vkGetPhysicalDeviceFormatProperties(ZestDevice->backend->physical_device, VK_FORMAT_R8G8B8A8_UNORM, &format_properties);
    if (!(format_properties.linearTilingFeatures & VK_FORMAT_FEATURE_BLIT_DST_BIT)) {
        return 0;
    }
    return ZEST_TRUE;
}

VkInstance zest_GetVKInstance() {
    return ZestDevice->backend->instance;
}

void zest_vk_SetWindowSurface(VkSurfaceKHR surface) {
     ZestRenderer->main_window->backend->surface = surface;
}

VkAllocationCallbacks *zest_GetVKAllocationCallbacks() {
    return &ZestDevice->backend->allocation_callbacks;
}

zest_bool zest__vk_create_window_surface(zest_window window) {
    VkWin32SurfaceCreateInfoKHR surface_create_info;
    surface_create_info.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
    surface_create_info.pNext = NULL;
    surface_create_info.flags = 0;
    surface_create_info.hinstance = zest_window_instance;
    surface_create_info.hwnd = window->window_handle;
    ZEST_SET_MEMORY_CONTEXT(zest_platform_renderer, zest_command_surface);
    ZEST_RETURN_FALSE_ON_FAIL(vkCreateWin32SurfaceKHR(ZestDevice->backend->instance, &surface_create_info, &ZestDevice->backend->allocation_callbacks, &window->backend->surface));
    return ZEST_TRUE;
}

zest_bool zest__vk_begin_single_time_commands(void) {
    ZEST_ASSERT(ZestRenderer->backend->one_time_command_buffer == VK_NULL_HANDLE);  //The command buffer must be null. Call zest__vk_end_single_time_commands before calling begin again.
    VkCommandBufferAllocateInfo alloc_info = { 0 };
    alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    alloc_info.commandPool = ZestDevice->backend->one_time_command_pool[ZEST_FIF];
    alloc_info.commandBufferCount = 1;

    ZEST_SET_MEMORY_CONTEXT(zest_platform_renderer, zest_command_command_buffer);
    ZEST_RETURN_FALSE_ON_FAIL(vkAllocateCommandBuffers(ZestDevice->backend->logical_device, &alloc_info, &ZestRenderer->backend->one_time_command_buffer));

    VkCommandBufferBeginInfo begin_info = { 0 };
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    ZEST_RETURN_FALSE_ON_FAIL(vkBeginCommandBuffer(ZestRenderer->backend->one_time_command_buffer, &begin_info));

    return ZEST_TRUE;
}

zest_bool zest__vk_end_single_time_commands(void) {
    ZEST_VK_ASSERT_RESULT(vkEndCommandBuffer(ZestRenderer->backend->one_time_command_buffer));

    VkSubmitInfo submit_info = { 0 };
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &ZestRenderer->backend->one_time_command_buffer;

    VkFence fence = zest__vk_create_fence();
    ZEST_RETURN_FALSE_ON_FAIL(vkQueueSubmit(ZestDevice->graphics_queue.backend->vk_queue, 1, &submit_info, fence));
    vkWaitForFences(ZestDevice->backend->logical_device, 1, &fence, VK_TRUE, UINT64_MAX);
    vkDestroyFence(ZestDevice->backend->logical_device, fence, &ZestDevice->backend->allocation_callbacks);

    vkFreeCommandBuffers(ZestDevice->backend->logical_device, ZestDevice->backend->one_time_command_pool[ZEST_FIF], 1, &ZestRenderer->backend->one_time_command_buffer);
    ZestRenderer->backend->one_time_command_buffer = VK_NULL_HANDLE;

    return ZEST_TRUE;
}
// -- End General_helpers
 
void *zest__vk_allocate_callback(void *pUserData, size_t size, size_t alignment, VkSystemAllocationScope allocationScope) {
    zest_platform_memory_info_t *pInfo = (zest_platform_memory_info_t *)pUserData;
    if (pInfo->timestamp == 37) {
        int d = 0;
    }
    void *pAllocation = ZEST__ALLOCATE(size + sizeof(zest_platform_memory_info_t));
    *(zest_platform_memory_info_t *)pAllocation = *pInfo;
    return (void *)((zest_platform_memory_info_t *)pAllocation + 1);
}

void *zest__vk_reallocate_callback(void *pUserData, void *pOriginal, size_t size, size_t alignment, VkSystemAllocationScope allocationScope) {
    zest_platform_memory_info_t *pHeader = ((zest_platform_memory_info_t *)pOriginal) - 1;
    void *pNewAllocation = ZEST__REALLOCATE(pHeader, size + sizeof(zest_platform_memory_info_t));
    zest_platform_memory_info_t *pInfo = (zest_platform_memory_info_t *)pUserData;
    *(zest_platform_memory_info_t *)pNewAllocation = *pInfo;
    return (void *)((zest_platform_memory_info_t *)pNewAllocation + 1);
}

void zest__vk_free_callback(void *pUserData, void *memory) {
    if (!memory) return;
    zest_platform_memory_info_t *original_allocation = (zest_platform_memory_info_t *)memory - 1;
    ZEST__FREE(original_allocation);
}
// -- Internal_Frame_graph_context_functions

zest_bool zest__vk_begin_command_buffer(const zest_frame_graph_context context) {
	VkCommandBufferBeginInfo begin_info = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
	begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    ZEST_RETURN_FALSE_ON_FAIL(vkBeginCommandBuffer(context->backend->command_buffer, &begin_info));
    return ZEST_TRUE;
}

zest_bool zest__vk_set_next_command_buffer(zest_frame_graph_context context, zest_queue queue) {
    if (zest_vec_size(queue->backend->command_buffers[ZEST_FIF]) > queue->next_buffer) {
        VkCommandBuffer command_buffer = queue->backend->command_buffers[ZEST_FIF][queue->next_buffer];
        queue->next_buffer++;
        context->backend->command_buffer = command_buffer;
        return ZEST_TRUE;
    } else {
        // Ran out of command buffers, create a new one
		VkCommandBufferAllocateInfo alloc_info = { 0 };
		alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		alloc_info.commandBufferCount = 1;
		alloc_info.commandPool = queue->backend->command_pool[ZEST_FIF];
        VkCommandBuffer new_command_buffer;
		ZEST_SET_MEMORY_CONTEXT(zest_platform_renderer, zest_command_command_buffer);
        VkResult result = vkAllocateCommandBuffers(ZestDevice->backend->logical_device, &alloc_info, &new_command_buffer);
        if (result != VK_SUCCESS) {
            ZEST_VK_PRINT_RESULT(result);
			return ZEST_FALSE;
        }
        zest_vec_push(queue->backend->command_buffers[ZEST_FIF], new_command_buffer);
        queue->next_buffer++;
        context->backend->command_buffer = new_command_buffer;
        return ZEST_TRUE;
    }
	return ZEST_FALSE;
}

void *zest__vk_new_execution_backend(zloc_linear_allocator_t *allocator) {
    zest_execution_backend backend = zloc_LinearAllocation(allocator, sizeof(zest_execution_backend_t));
    *backend = (zest_execution_backend_t){ 0 };
    return backend;
}

void zest__vk_set_execution_fence(zest_execution_backend backend, zest_bool is_intraframe) {
    backend->fence = !is_intraframe ? ZestRenderer->backend->fif_fence[ZEST_FIF] : ZestRenderer->backend->intraframe_fence;
    backend->fence_count = !is_intraframe ? &ZestRenderer->fence_count[ZEST_FIF] : &backend->intraframe_fence_count;
}

void zest__vk_acquire_barrier(zest_frame_graph_context context, zest_execution_details_t *exe_details) {
	zest_uint buffer_count = zest_vec_size(exe_details->barriers.backend->acquire_buffer_barriers);
	zest_uint image_count = zest_vec_size(exe_details->barriers.backend->acquire_image_barriers);
	if (buffer_count > 0 || image_count > 0) {
		zest_vec_foreach(resource_index, exe_details->barriers.backend->acquire_buffer_barriers) {
			VkBufferMemoryBarrier *barrier = &exe_details->barriers.backend->acquire_buffer_barriers[resource_index];
			zest_resource_node resource = exe_details->barriers.acquire_buffer_barrier_nodes[resource_index];
			zest_buffer buffer = resource->storage_buffer;
			barrier->buffer = buffer->backend->vk_buffer;
			barrier->size = buffer->size;
			barrier->offset = buffer->buffer_offset;
		}
		zest_vec_foreach(resource_index, exe_details->barriers.backend->acquire_image_barriers) {
			VkImageMemoryBarrier *barrier = &exe_details->barriers.backend->acquire_image_barriers[resource_index];
			zest_resource_node resource = exe_details->barriers.acquire_image_barrier_nodes[resource_index];
			barrier->image = resource->view->image->backend->vk_image;
			ZEST_ASSERT(barrier->image);    //The image handle in the resource is null, if the resource is not
											//transient then can resource provider callback must be set in the resource.
			barrier->subresourceRange.levelCount = resource->image.info.mip_levels;
			if (resource->linked_layout) {
				//Update the layout in the texture
				*resource->linked_layout = (zest_image_layout)barrier->newLayout;
                resource->image.backend->vk_current_layout = barrier->newLayout;
                resource->image_layout = (zest_image_layout)barrier->newLayout;
			}
		}
		vkCmdPipelineBarrier(
			context->backend->command_buffer,
			exe_details->barriers.backend->overall_src_stage_mask_for_acquire_barriers, // Single mask for all barriers in this batch
			exe_details->barriers.backend->overall_dst_stage_mask_for_acquire_barriers, // Single mask for all barriers in this batch
			0,
			0, NULL,
			buffer_count,
			exe_details->barriers.backend->acquire_buffer_barriers,
			image_count,
			exe_details->barriers.backend->acquire_image_barriers
		);
	}
}

void zest__vk_release_barrier(zest_frame_graph_context context, zest_execution_details_t *exe_details) {
	zest_uint buffer_count = zest_vec_size(exe_details->barriers.backend->release_buffer_barriers);
	zest_uint image_count = zest_vec_size(exe_details->barriers.backend->release_image_barriers);
	if (buffer_count > 0 || image_count > 0) {
		zest_vec_foreach(resource_index, exe_details->barriers.backend->release_buffer_barriers) {
			VkBufferMemoryBarrier *barrier = &exe_details->barriers.backend->release_buffer_barriers[resource_index];
			zest_resource_node resource = exe_details->barriers.release_buffer_barrier_nodes[resource_index];
			zest_buffer buffer = resource->storage_buffer;
			barrier->buffer = buffer->backend->vk_buffer;
			barrier->size = buffer->size;
			barrier->offset = buffer->buffer_offset;
		}
		zest_vec_foreach(resource_index, exe_details->barriers.backend->release_image_barriers) {
			VkImageMemoryBarrier *barrier = &exe_details->barriers.backend->release_image_barriers[resource_index];
			zest_resource_node resource = exe_details->barriers.release_image_barrier_nodes[resource_index];
			barrier->image = resource->image.backend->vk_image;
			barrier->subresourceRange.levelCount = resource->image.info.mip_levels;
			if (resource->linked_layout) {
				//Update the layout in the texture
				*resource->linked_layout = (zest_image_layout)barrier->newLayout;
                resource->image.backend->vk_current_layout = barrier->newLayout;
                resource->image_layout = (zest_image_layout)barrier->newLayout;
			}
		}
		vkCmdPipelineBarrier(
			context->backend->command_buffer,
			exe_details->barriers.backend->overall_src_stage_mask_for_release_barriers, // Single mask for all barriers in this batch
			exe_details->barriers.backend->overall_dst_stage_mask_for_release_barriers, // Single mask for all barriers in this batch
			0,
			0, NULL,
			buffer_count,
			exe_details->barriers.backend->release_buffer_barriers,
			image_count,
			exe_details->barriers.backend->release_image_barriers
		);
	}
}

zest_frame_graph_semaphores zest__vk_get_frame_graph_semaphores(const char *name) {
    if (!zest_map_valid_name(ZestRenderer->cached_frame_graph_semaphores, name)) {

        VkSemaphoreCreateInfo semaphore_info = { 0 };
        semaphore_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
        VkSemaphoreTypeCreateInfo timeline_create_info = { 0 };
        timeline_create_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO;
        timeline_create_info.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE;
        timeline_create_info.initialValue = 0;
        semaphore_info.pNext = &timeline_create_info;

        zest_frame_graph_semaphores semaphores = ZEST__NEW(zest_frame_graph_semaphores);
        *semaphores = (zest_frame_graph_semaphores_t){ 0 };
        semaphores->magic = zest_INIT_MAGIC(zest_struct_type_frame_graph_semaphores);
        semaphores->backend = ZestPlatform.new_frame_graph_semaphores_backend();

        zest_ForEachFrameInFlight(fif) {
            for (int queue_index = 0; queue_index != ZEST_QUEUE_COUNT; ++queue_index) {
                VkSemaphore semaphore;
				ZEST_SET_MEMORY_CONTEXT(zest_platform_renderer, zest_command_semaphore);
                vkCreateSemaphore(ZestDevice->backend->logical_device, &semaphore_info, &ZestDevice->backend->allocation_callbacks, &semaphore);
                semaphores->backend->vk_semaphores[fif][queue_index] = semaphore;
            }
        }

        zest_map_insert(ZestRenderer->cached_frame_graph_semaphores, name, semaphores);
        return semaphores;
    }
	return *zest_map_at(ZestRenderer->cached_frame_graph_semaphores, name);
}

zest_frame_graph_result zest__vk_create_fg_render_pass(zest_pass_group_t *pass, zest_execution_details_t *exe_details, zest_uint current_pass_index) {
    zloc_linear_allocator_t *allocator = ZestRenderer->frame_graph_allocator[ZEST_FIF];

    if (exe_details->requires_dynamic_render_pass) {
        zest_uint color_attachment_index = 0;
        //Determine attachments for color and depth (resolve can come later), first for outputs
        zest_map_foreach(o, pass->outputs) {
            zest_resource_usage_t *output_usage = &pass->outputs.data[o];
            zest_resource_node resource = pass->outputs.data[o].resource_node;
            if (resource->type & zest_resource_type_is_image) {
				if (resource->type != zest_resource_type_depth && ZEST__FLAGGED(pass->flags, zest_pass_flag_output_resolve) && resource->image.info.sample_count == 1) {
                    output_usage->purpose = zest_purpose_color_attachment_resolve;
				}
                if (output_usage->purpose == zest_purpose_color_attachment_write) {
                    zest_temp_attachment_info_t color = { 0 };
                    color.resource_node = resource;
                    color.usage_info = output_usage;
                    color.attachment_slot = color_attachment_index++;
                    zest_vec_linear_push(allocator, exe_details->color_attachment_info, color);
                } else if (output_usage->purpose == zest_purpose_color_attachment_resolve) {
                    zest_temp_attachment_info_t color = { 0 };
                    color.resource_node = resource;
                    color.usage_info = output_usage;
                    color.attachment_slot = color_attachment_index++;
                    zest_vec_linear_push(allocator, exe_details->resolve_attachment_info, color);
                } else if (output_usage->purpose == zest_purpose_depth_stencil_attachment_write) {
                    exe_details->depth_attachment_info.resource_node = resource;
                    exe_details->depth_attachment_info.usage_info = output_usage;
                    exe_details->depth_attachment_info.attachment_slot = 0;
                    ZEST__FLAG(resource->flags, zest_resource_node_flag_used_in_output);
                }
            }
        }

        zest_uint input_attachment_index = 0;
        //Do the same for inputs
        zest_map_foreach(i, pass->inputs) {
            zest_resource_usage_t *input_usage = &pass->inputs.data[i];
            zest_resource_node input_resource = pass->inputs.data[i].resource_node;
            if (input_resource->type & zest_resource_type_is_image) {
                if (ZEST__FLAGGED(input_usage->access_mask, VK_ACCESS_INPUT_ATTACHMENT_READ_BIT)) {
                    zest_temp_attachment_info_t color = { 0 };
                    color.resource_node = input_resource;
                    color.usage_info = input_usage;
                    color.attachment_slot = input_attachment_index++;
                    zest_vec_linear_push(allocator, exe_details->color_attachment_info, color);
                } else if (ZEST__FLAGGED(input_usage->access_mask, VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT) && ZEST__NOT_FLAGGED(input_resource->flags, zest_resource_node_flag_used_in_output)) {
                    zest_temp_attachment_info_t depth = { 0 };
                    depth.resource_node = input_resource;
                    depth.usage_info = input_usage;
                    depth.attachment_slot = 0;
                }
            }
        }

        VkAttachmentDescription *attachments = 0;
        //Now that we've parsed the inputs and outputs, make the attachment descriptions
        zest_key render_pass_hash = 0;
        zest_uint expected_samples = 0;
        if (zest_vec_size(exe_details->color_attachment_info) > 0) {
           expected_samples = exe_details->color_attachment_info[0].resource_node->image.info.sample_count;
        }        
        bool is_resolved = (zest_vec_size(exe_details->resolve_attachment_info) > 0);
        zest_vec_foreach(c, exe_details->color_attachment_info) {
            zest_resource_node node = exe_details->color_attachment_info[c].resource_node;
            zest_resource_usage_t *usage_info = exe_details->color_attachment_info[c].usage_info;
            //All attachments must have the same number of sample counts
            if (expected_samples != node->image.info.sample_count) {
                ZEST__REPORT(zest_report_invalid_render_pass, "Render pass is invalid. When processing the color attachments the number of samples for resource [%s] was not equal to the expected sample counts. Make sure they're the same for all attachments.", node->name);
                return zest_fgs_invalid_render_pass;
            }
            if (!zest_map_valid_key(exe_details->attachment_indexes, (zest_key)node)) {
                VkAttachmentDescription attachment = { 0 };
                attachment.format = zest__to_vk_format(node->image.info.format);
                attachment.samples = node->image.info.sample_count;
                attachment.loadOp = zest__to_vk_load_op(usage_info->load_op);
                attachment.storeOp = is_resolved ? VK_ATTACHMENT_STORE_OP_DONT_CARE : zest__to_vk_store_op(usage_info->store_op);
                attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
                attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
                attachment.initialLayout = zest__to_vk_image_layout(node->image_layout);
				//We'll handle the transition using barriers
				attachment.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
                if (node->type == zest_resource_type_swap_chain_image) {
                    attachment.finalLayout = zest__to_vk_image_layout(zest__determine_final_layout(current_pass_index, node, usage_info));
                } else {
                    //We'll handle the transition using barriers
                    attachment.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
                }
                if (attachment.finalLayout == VK_IMAGE_LAYOUT_PRESENT_SRC_KHR) {
                    ZEST__FLAG(ZestRenderer->current_frame_graph->flags, zest_frame_graph_present_after_execute);
                }
                zest_vec_linear_push(allocator, attachments, attachment);
                zest_map_insert_linear_key(allocator, exe_details->attachment_indexes, (zest_key)node, (zest_vec_size(attachments) - 1));
                zest_vec_linear_push(allocator, exe_details->attachment_resource_nodes, node);
                zest_vec_linear_push(allocator, exe_details->clear_values, usage_info->clear_value);
                render_pass_hash += zest_Hash(&attachment, sizeof(VkAttachmentDescription), ZEST_HASH_SEED);
            }
        }

        if (exe_details->depth_attachment_info.resource_node) {
            zest_resource_node node = exe_details->depth_attachment_info.resource_node;
            zest_resource_usage_t *usage_info = exe_details->depth_attachment_info.usage_info;
			zest_key attachment_key = node->swapchain ? (zest_key)node->swapchain : (zest_key)node;
			//Depth attachment must have the same sample count as the color attachments
            if (expected_samples != node->image.info.sample_count) {
                ZEST__REPORT(zest_report_invalid_render_pass, "Render pass is invalid. When processing the depth attachment the number of samples for resource [%s] was not equal to the expected sample counts. Make sure they're the same for all color and depth attachments.", node->name);
                return zest_fgs_invalid_render_pass;
            }
            if (!zest_map_valid_key(exe_details->attachment_indexes, attachment_key)) {
                VkAttachmentDescription attachment = { 0 };
                attachment.format = zest__to_vk_format(node->image.info.format);
                attachment.samples = node->image.info.sample_count;
                attachment.loadOp = zest__to_vk_load_op(usage_info->load_op);
                attachment.storeOp = zest__to_vk_store_op(usage_info->store_op);
                attachment.stencilLoadOp = zest__to_vk_load_op(usage_info->stencil_load_op);
                attachment.stencilStoreOp = zest__to_vk_store_op(usage_info->stencil_store_op);
                attachment.initialLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
                if (node->type == zest_resource_type_swap_chain_image) {
                    attachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
                } else {
                    //We'll handle the transition using barriers
                    attachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
                }
                zest_vec_linear_push(allocator, attachments, attachment);
                zest_map_insert_linear_key(allocator, exe_details->attachment_indexes, attachment_key, (zest_vec_size(attachments) - 1));
                zest_vec_linear_push(allocator, exe_details->attachment_resource_nodes, node);
                zest_vec_linear_push(allocator, exe_details->clear_values, usage_info->clear_value);
                render_pass_hash += zest_Hash(&attachment, sizeof(VkAttachmentDescription), ZEST_HASH_SEED);
            }
        }

        zest_uint resolve_attachment_num_samples = 0;
        zest_vec_foreach(c, exe_details->resolve_attachment_info) {
            zest_resource_node node = exe_details->resolve_attachment_info[c].resource_node;
            zest_resource_usage_t *usage_info = exe_details->resolve_attachment_info[c].usage_info;
            if (!zest_map_valid_key(exe_details->attachment_indexes, (zest_key)node)) {
                VkAttachmentDescription attachment = { 0 };
                attachment.format = zest__to_vk_format(node->image.info.format);
                attachment.samples = node->image.info.sample_count;
                attachment.loadOp = zest__to_vk_load_op(usage_info->load_op);
                attachment.storeOp = zest__to_vk_store_op(usage_info->store_op);
                attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
                attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
                attachment.initialLayout = zest__to_vk_image_layout(node->image_layout);
                //We'll handle the transition using barriers
                attachment.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
                if (node->type == zest_resource_type_swap_chain_image) {
                    attachment.finalLayout = zest__to_vk_image_layout(zest__determine_final_layout(current_pass_index, node, usage_info));
                } else {
                    //We'll handle the transition using barriers
                    attachment.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
                }
                if (attachment.finalLayout == VK_IMAGE_LAYOUT_PRESENT_SRC_KHR) {
                    ZEST__FLAG(ZestRenderer->current_frame_graph->flags, zest_frame_graph_present_after_execute);
                }
                zest_vec_linear_push(allocator, attachments, attachment);
                zest_map_insert_linear_key(allocator, exe_details->attachment_indexes, (zest_key)node, (zest_vec_size(attachments) - 1));
                zest_vec_linear_push(allocator, exe_details->attachment_resource_nodes, node);
                zest_vec_linear_push(allocator, exe_details->clear_values, usage_info->clear_value);
                render_pass_hash += zest_Hash(&attachment, sizeof(VkAttachmentDescription), ZEST_HASH_SEED);
            }
        }

        if (zest_vec_size(exe_details->resolve_attachment_info) > 0) {
            if (zest_vec_size(exe_details->resolve_attachment_info) != zest_vec_size(exe_details->color_attachment_info)) {
                ZEST__REPORT(zest_report_invalid_render_pass, "Invalid render pass. If resolving, the number of resolve attachments must equal the number of color attachments");
                return zest_fgs_invalid_render_pass;
            }
        }

        VkAttachmentReference *vk_color_refs = 0;
        VkAttachmentReference vk_depth_ref = { VK_ATTACHMENT_UNUSED, VK_IMAGE_LAYOUT_UNDEFINED };
        VkAttachmentReference *vk_input_refs = 0;       //Not used yet
        VkAttachmentReference *vk_resolve_refs = 0;	
        zest_vec_foreach(c, exe_details->color_attachment_info) {
            zest_resource_node node = exe_details->color_attachment_info[c].resource_node;
            zest_resource_usage_t *usage_info = exe_details->color_attachment_info[c].usage_info;
            VkAttachmentReference reference = { 0 };
            reference.attachment = *zest_map_at_key(exe_details->attachment_indexes, (zest_key)node);
            reference.layout = zest__to_vk_image_layout(usage_info->image_layout);
            zest_vec_linear_push(allocator, vk_color_refs, reference);
        }
        if (exe_details->depth_attachment_info.resource_node) {
            zest_resource_node node = exe_details->depth_attachment_info.resource_node;
            zest_resource_usage_t *usage_info = exe_details->depth_attachment_info.usage_info;
            if (node->swapchain) {
                vk_depth_ref.attachment = *zest_map_at_key(exe_details->attachment_indexes, (zest_key)node->swapchain);
				vk_depth_ref.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
            } else {
                vk_depth_ref.attachment = *zest_map_at_key(exe_details->attachment_indexes, (zest_key)node);
				vk_depth_ref.layout = zest__to_vk_image_layout(usage_info->image_layout);
            }
        }
        zest_vec_foreach(c, exe_details->resolve_attachment_info) {
            zest_resource_node node = exe_details->resolve_attachment_info[c].resource_node;
            zest_resource_usage_t *usage_info = exe_details->resolve_attachment_info[c].usage_info;
            VkAttachmentReference reference = { 0 };
            reference.attachment = *zest_map_at_key(exe_details->attachment_indexes, (zest_key)node);
            reference.layout = zest__to_vk_image_layout(usage_info->image_layout);
            zest_vec_linear_push(allocator, vk_resolve_refs, reference);
            //The resolve attachment format must match the color attachment format that it's resolving from.
            if (node->image.info.format != exe_details->color_attachment_info[c].resource_node->image.info.format) {
                ZEST__REPORT(zest_report_invalid_render_pass, "Invalid render pass. The resolve attachment for resource [%s] does not match the same color attachment.", node->name);
                return zest_fgs_invalid_render_pass;
            }
        }

        //Not worried about input and resolve attachments just yet, will leave that for later

        //Construct the subpass:
        VkSubpassDescription subpass_desc = { 0 };
        subpass_desc.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass_desc.colorAttachmentCount = zest_vec_size(vk_color_refs);
        subpass_desc.inputAttachmentCount = zest_vec_size(vk_input_refs);

        render_pass_hash += zest_Hash(&subpass_desc, sizeof(VkSubpassDescription), ZEST_HASH_SEED);

        subpass_desc.pColorAttachments = vk_color_refs;
        subpass_desc.pDepthStencilAttachment = (vk_depth_ref.attachment != VK_ATTACHMENT_UNUSED) ? &vk_depth_ref : VK_NULL_HANDLE;
        subpass_desc.pInputAttachments = vk_input_refs;
        subpass_desc.pResolveAttachments = vk_resolve_refs;

        //Handle sub pass dependencies
        VkPipelineStageFlags combined_attachment_stage_mask = 0;
        VkAccessFlags combined_attachment_access_mask = 0;

        // Collect stage masks from color attachments
        zest_vec_foreach(c, exe_details->color_attachment_info) {
            zest_resource_usage_t *usage_info = exe_details->color_attachment_info[c].usage_info;
            combined_attachment_stage_mask |= usage_info->stage_mask;
            combined_attachment_access_mask |= usage_info->access_mask;
        }

        // Collect stage masks from depth attachment, if present
        if (exe_details->depth_attachment_info.resource_node) {
            zest_resource_usage_t *usage_info = exe_details->depth_attachment_info.usage_info;
            combined_attachment_stage_mask |= usage_info->stage_mask;
            combined_attachment_access_mask |= usage_info->access_mask;
        }

        //Handle sub pass dependencies
        VkSubpassDependency initial_dependency = { 0 };
        initial_dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
        initial_dependency.dstSubpass = 0;
        initial_dependency.srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT; // A safe bet if barriers handle specifics
        initial_dependency.dstStageMask = combined_attachment_stage_mask;   // Stages where attachments are first used
        initial_dependency.srcAccessMask = 0;
        initial_dependency.dstAccessMask = combined_attachment_access_mask; // A broad mask - can be refined later?
        initial_dependency.dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;
        render_pass_hash += zest_Hash(&initial_dependency, sizeof(VkSubpassDependency), ZEST_HASH_SEED);

        VkSubpassDependency final_dependency = { 0 };
        final_dependency.srcSubpass = 0;
        final_dependency.dstSubpass = VK_SUBPASS_EXTERNAL;
        final_dependency.srcStageMask = combined_attachment_stage_mask;		// Stages where attachments were last used
        final_dependency.dstStageMask = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;	// Stages of subsequent operations
        final_dependency.srcAccessMask = combined_attachment_access_mask;	// Accesses this subpass performed
        final_dependency.dstAccessMask = 0; // Subsequent access will be handled by next pass's barriers/RP
        final_dependency.dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;
        render_pass_hash += zest_Hash(&final_dependency, sizeof(VkSubpassDependency), ZEST_HASH_SEED);

        VkRenderPassCreateInfo render_pass_info = { 0 };
        render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        render_pass_info.attachmentCount = zest_vec_size(attachments);
        render_pass_info.subpassCount = 1;
        render_pass_info.dependencyCount = 2;
        render_pass_hash += zest_Hash(&render_pass_info, sizeof(VkRenderPassCreateInfo), ZEST_HASH_SEED);

        render_pass_info.pAttachments = attachments;
        render_pass_info.pSubpasses = &subpass_desc;
        render_pass_info.pDependencies = (VkSubpassDependency[]){ initial_dependency, final_dependency };
        if (!zest_map_valid_key(ZestRenderer->cached_render_passes, render_pass_hash)) {
            zest_render_pass render_pass = zest__create_render_pass();
            render_pass->backend = ZEST__NEW(zest_render_pass_backend);
            *render_pass->backend = (zest_render_pass_backend_t){ 0 };
			ZEST_SET_MEMORY_CONTEXT(zest_platform_renderer, zest_command_render_pass);
            ZestRenderer->backend->last_result = vkCreateRenderPass(ZestDevice->backend->logical_device, &render_pass_info, &ZestDevice->backend->allocation_callbacks, &render_pass->backend->vk_render_pass);
            if (ZestRenderer->backend->last_result != VK_SUCCESS) {
                ZEST__REPORT(zest_report_invalid_render_pass, "There was an error when calling vkCreateRenderPass with code: %i", ZestRenderer->backend->last_result);
                return zest_fgs_invalid_render_pass;
            }
            zest_map_insert_key(ZestRenderer->cached_render_passes, render_pass_hash, render_pass);
            render_pass->backend->sample_count = expected_samples;
            exe_details->render_pass = render_pass;
        } else {
            exe_details->render_pass = *zest_map_at_key(ZestRenderer->cached_render_passes, render_pass_hash);
        }
    }
    return zest_fgs_success;
}

zest_bool zest__vk_begin_render_pass(const zest_frame_graph_context context, zest_execution_details_t *exe_details) {
    zloc_linear_allocator_t *allocator = ZestRenderer->frame_graph_allocator[ZEST_FIF];
    //Create_frame_buffers
    VkImageView *image_views = 0;
    zest_map_foreach(i, exe_details->attachment_indexes) {
        zest_resource_node node = exe_details->attachment_resource_nodes[i];
        zest_vec_linear_push(allocator, image_views, node->view->backend->vk_view);
    }

    VkFramebufferCreateInfo fb_create_info = { 0 };
    fb_create_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    fb_create_info.renderPass = exe_details->render_pass->backend->vk_render_pass;
    fb_create_info.attachmentCount = zest_vec_size(image_views);
    fb_create_info.pAttachments = image_views;
    if (zest_vec_size(exe_details->color_attachment_info)) {
        fb_create_info.width = exe_details->color_attachment_info[0].resource_node->image.info.extent.width;
        fb_create_info.height = exe_details->color_attachment_info[0].resource_node->image.info.extent.height;
    } else {
        fb_create_info.width = exe_details->depth_attachment_info.resource_node->image.info.extent.width;
        fb_create_info.height = exe_details->depth_attachment_info.resource_node->image.info.extent.height;
    }
    exe_details->render_area.extent.width = fb_create_info.width;
    exe_details->render_area.extent.height = fb_create_info.height;
    exe_details->render_area.offset.x = 0;
    exe_details->render_area.offset.y = 0;
    fb_create_info.layers = 1;

    VkFramebuffer frame_buffer;
    ZEST_SET_MEMORY_CONTEXT(zest_platform_renderer, zest_command_frame_buffer);
    ZEST_RETURN_FALSE_ON_FAIL(vkCreateFramebuffer(ZestDevice->backend->logical_device, &fb_create_info, &ZestDevice->backend->allocation_callbacks, &frame_buffer));
    ZestPlatform.deferr_framebuffer_destruction(&frame_buffer);

    VkRenderPassBeginInfo render_pass_info = { 0 };
    render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    render_pass_info.renderPass = exe_details->render_pass->backend->vk_render_pass;
    render_pass_info.framebuffer = frame_buffer;
    render_pass_info.renderArea.extent.width = exe_details->render_area.extent.width;
    render_pass_info.renderArea.extent.height = exe_details->render_area.extent.height;
    render_pass_info.renderArea.offset.x = exe_details->render_area.offset.x;
    render_pass_info.renderArea.offset.y = exe_details->render_area.offset.y;

    zest_uint clear_size = zest_vec_size(exe_details->clear_values);
    render_pass_info.clearValueCount = clear_size;

    VkClearValue *clear_values = 0;
    zest_vec_foreach(i, exe_details->clear_values) {
        zest_clear_value_t *cv = &exe_details->clear_values[i];
        VkClearValue clear_value = {
            .color = {cv->color.r, cv->color.g, cv->color.b, cv->color.a},
            .depthStencil = {cv->depth_stencil.depth, cv->depth_stencil.stencil}
        };
        zest_vec_linear_push(allocator, clear_values, clear_value);
    }
    render_pass_info.pClearValues = clear_values;
    vkCmdBeginRenderPass(context->backend->command_buffer, &render_pass_info, VK_SUBPASS_CONTENTS_INLINE);
    return ZEST_TRUE;
}

void zest__vk_deferr_framebuffer_destruction(void *framebuffer) {
    VkFramebuffer fb = *((VkFramebuffer*)framebuffer);
    zest_vec_push(ZestRenderer->deferred_resource_freeing_list.backend->frame_buffers[ZEST_FIF], fb);
}

void zest__vk_cleanup_deferred_destruction_backend(void) {
    zest_ForEachFrameInFlight(fif) {
		zest_vec_foreach(i, ZestRenderer->deferred_resource_freeing_list.backend->frame_buffers[fif]) {
			VkFramebuffer frame_buffer = ZestRenderer->deferred_resource_freeing_list.backend->frame_buffers[fif][i];
			vkDestroyFramebuffer(ZestDevice->backend->logical_device, frame_buffer, &ZestDevice->backend->allocation_callbacks);
		}
        zest_vec_free(ZestRenderer->deferred_resource_freeing_list.backend->frame_buffers[fif])
    }
    ZEST__FREE(ZestRenderer->deferred_resource_freeing_list.backend);
}

void zest__vk_cleanup_deferred_framebuffers(void) {
    zest_vec_foreach(i, ZestRenderer->deferred_resource_freeing_list.backend->frame_buffers[ZEST_FIF]) {
        VkFramebuffer frame_buffer = ZestRenderer->deferred_resource_freeing_list.backend->frame_buffers[ZEST_FIF][i];
		vkDestroyFramebuffer(ZestDevice->backend->logical_device, frame_buffer, &ZestDevice->backend->allocation_callbacks);
    }
    zest_vec_clear(ZestRenderer->deferred_resource_freeing_list.backend->frame_buffers[ZEST_FIF]);
}

void zest__vk_end_render_pass(const zest_frame_graph_context context) {
	vkCmdEndRenderPass(context->backend->command_buffer);
}

void zest__vk_end_command_buffer(const zest_frame_graph_context context) {
	vkEndCommandBuffer(context->backend->command_buffer);
}

void zest__vk_carry_over_semaphores(zest_frame_graph frame_graph, zest_wave_submission_t *wave_submission, zest_execution_backend backend) {
	zest_vec_clear(backend->wave_wait_semaphores);
	zest_vec_clear(backend->wave_wait_values);
    zloc_linear_allocator_t *allocator = ZestRenderer->frame_graph_allocator[ZEST_FIF];
	for (zest_uint queue_index = 0; queue_index != ZEST_QUEUE_COUNT; ++queue_index) {
		if (wave_submission->batches[queue_index].magic) {
			zest_vec_linear_push(allocator, backend->wave_wait_semaphores, frame_graph->semaphores->backend->vk_semaphores[ZEST_FIF][queue_index]);
			zest_vec_linear_push(allocator, backend->wave_wait_values, frame_graph->semaphores->values[ZEST_FIF][queue_index]);
		}
	}
}

zest_bool zest__vk_frame_graph_fence_wait(zest_execution_backend backend) {
	if (*backend->fence_count > 0) {
		ZEST_RETURN_FALSE_ON_FAIL(vkWaitForFences(ZestDevice->backend->logical_device, *backend->fence_count, backend->fence, VK_TRUE, UINT64_MAX));
		ZEST_RETURN_FALSE_ON_FAIL(vkResetFences(ZestDevice->backend->logical_device, *backend->fence_count, backend->fence));
	}
    return ZEST_TRUE;
}

zest_bool zest__vk_submit_frame_graph_batch(zest_frame_graph frame_graph, zest_execution_backend backend, zest_submission_batch_t *batch, zest_map_queue_value *queues) {
    zest_uint queue_index = frame_graph->context.queue_index;
    zest_uint submission_index = frame_graph->context.submission_index;
    zest_pipeline_stage_flags timeline_wait_stage = frame_graph->context.timeline_wait_stage;

    VkSemaphore batch_semaphore = frame_graph->semaphores->backend->vk_semaphores[ZEST_FIF][queue_index];
    zest_size *batch_value = &frame_graph->semaphores->values[ZEST_FIF][queue_index];

    VkCommandBuffer command_buffer = frame_graph->context.backend->command_buffer;

    VkSubmitInfo submit_info = { VK_STRUCTURE_TYPE_SUBMIT_INFO };
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &command_buffer; // Use the CB we just recorded

    // Set signal semaphores for this batch
    zest_queue queue = batch->queue;

    zest_uint queue_fif = queue->fif;
    VkSemaphore timeline_semaphore_for_this_fif = queue->backend->semaphore[queue_fif];

    //Increment the queue count for the timeline semaphores if the queue hasn't been used yet this frame graph
    zest_u64 wait_value = 0;
    zest_uint wait_index = queue->fif;
    if (zest_map_valid_key((*queues), (zest_key)queue)) {
        //Intraframe timeline semaphore required. This will happen if there are more than one batch for a queue family
        wait_value = *zest_map_at_key((*queues), (zest_key)queue)->signal_value;
    } else {
        //Interframe timeline semaphore required. Has to connect to the semaphore value in the previous frame that this
        //queue was ran.
        wait_index = (queue_fif + 1) % ZEST_MAX_FIF;
        wait_value = queue->current_count[wait_index];
    }

    zest_u64 signal_value = wait_value + 1;
    queue->signal_value = signal_value;
    queue->current_count[queue_fif] = signal_value;

    zloc_linear_allocator_t *allocator = ZestRenderer->frame_graph_allocator[ZEST_FIF];

    zest_map_insert_linear_key(allocator, (*queues), (zest_key)queue, queue);

    //We need to mix the binary semaphores in the batches with the timeline semaphores in the queue,
    //so use these arrays for that. Because they're mixed we also need wait values for the binary values
    //even if they're not used (they're set to 0)
    VkSemaphore *wait_semaphores = 0;
    VkPipelineStageFlags *wait_stages = 0;
    VkSemaphore *signal_semaphores = 0;
    zest_u64 *wait_values = 0;
    zest_u64 *signal_values = 0;

    zest_vec_linear_push(allocator, signal_semaphores, timeline_semaphore_for_this_fif);
    zest_vec_linear_push(allocator, signal_values, signal_value);

    VkTimelineSemaphoreSubmitInfo timeline_info = { 0 };
    timeline_info.sType = VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO;

    //If there's been enough frames in flight processed then add a timeline wait semphore that waits on the
    //previous frame in flight from max frames in flight ago. But only if this is the first batch of this queue
    //type being processed
    if (wait_value > 0 && batch->need_timeline_wait) {
        zest_vec_linear_push(allocator, wait_semaphores, queue->backend->semaphore[wait_index]);
        zest_vec_linear_push(allocator, wait_values, wait_value);
        zest_vec_linear_push(allocator, wait_stages, zest__to_vk_pipeline_stage(batch->timeline_wait_stage));
    }

    //Wait on the semaphores from the previous wave
    zest_vec_foreach(semaphore_index, backend->wave_wait_semaphores) {
        zest_vec_linear_push(allocator, wait_semaphores, backend->wave_wait_semaphores[semaphore_index]);
        zest_vec_linear_push(allocator, wait_stages, zest__to_vk_pipeline_stage(batch->queue_wait_stages));
        zest_vec_linear_push(allocator, wait_values, backend->wave_wait_values[semaphore_index]);
    }

    //Wait for any extra semaphores such as the acquire image semaphore
    zest_vec_foreach(semaphore_index, batch->wait_semaphores) {
        VkSemaphore dynamic_semaphore = zest__vk_get_semaphore_reference(frame_graph, &batch->wait_semaphores[semaphore_index]);
        zest_vec_linear_push(allocator, wait_semaphores, dynamic_semaphore);
        zest_vec_linear_push(allocator, wait_stages, zest__to_vk_pipeline_stage(batch->wait_dst_stage_masks[semaphore_index]));
        zest_vec_linear_push(allocator, wait_values, 0);
    }

    if (submission_index == 0 && zest_vec_size(frame_graph->wait_on_timelines)) {
        zest_vec_foreach(timeline_index, frame_graph->wait_on_timelines) {
            zest_execution_timeline timeline = frame_graph->wait_on_timelines[timeline_index];
            if (timeline->current_value > 0) {
                zest_vec_linear_push(allocator, wait_semaphores, timeline->backend->semaphore);
                zest_vec_linear_push(allocator, wait_stages, timeline_wait_stage);
                zest_vec_linear_push(allocator, wait_values, timeline->current_value);
            }
        }
    }

    timeline_info.waitSemaphoreValueCount = zest_vec_size(wait_values);
    timeline_info.pWaitSemaphoreValues = wait_values;

    // Set wait semaphores for this batch
    if (wait_semaphores) {
        submit_info.waitSemaphoreCount = zest_vec_size(wait_semaphores);
        submit_info.pWaitSemaphores = wait_semaphores;
        submit_info.pWaitDstStageMask = wait_stages; // Needs to be correctly set
    }

    //push any additional binary semaphores in the batch
    zest_vec_foreach(semaphore_index, batch->signal_semaphores) {
        VkSemaphore dynamic_semaphore = zest__vk_get_semaphore_reference(frame_graph, &batch->signal_semaphores[semaphore_index]);
        zest_vec_linear_push(allocator, signal_semaphores, dynamic_semaphore);
        zest_vec_linear_push(allocator, signal_values, 0);
    }

    //If this is the last batch then add the fence that tells the cpu to wait each frame
    VkFence submit_fence = VK_NULL_HANDLE;
    if (submission_index == zest_vec_size(frame_graph->submissions) - 1) {
        submit_fence = backend->fence[*backend->fence_count];
        (*backend->fence_count)++;
        ZEST_ASSERT(*backend->fence_count < ZEST_QUEUE_COUNT);

        if (zest_vec_size(frame_graph->signal_timelines)) {
            zest_vec_foreach(timeline_index, frame_graph->signal_timelines) {
                zest_execution_timeline timeline = frame_graph->signal_timelines[timeline_index];
                timeline->current_value += 1;
                zest_vec_linear_push(allocator, signal_semaphores, timeline->backend->semaphore);
                zest_vec_linear_push(allocator, signal_values, timeline->current_value);
            }
        }
    } else {
        //Make sure the submission includes the queue semaphores to chain together the dependencies
        //between each wave but only if this is not the last wave as we only signal intraframe
        (*batch_value)++;
        zest_vec_linear_push(allocator, signal_semaphores, batch_semaphore);
        zest_vec_linear_push(allocator, signal_values, *batch_value);
    }

    //Finish the rest of the queue submit info and submit the queue
    timeline_info.signalSemaphoreValueCount = zest_vec_size(signal_values);
    timeline_info.pSignalSemaphoreValues = signal_values;
    submit_info.signalSemaphoreCount = zest_vec_size(signal_semaphores);
    submit_info.pSignalSemaphores = signal_semaphores;
    submit_info.pNext = &timeline_info;

    ZEST_RETURN_FALSE_ON_FAIL(vkQueueSubmit(batch->queue->backend->vk_queue, 1, &submit_info, submit_fence));
    ZEST__FLAG(ZestRenderer->flags, zest_renderer_flag_work_was_submitted);

    batch->backend->final_wait_semaphores = wait_semaphores;
    batch->backend->final_signal_semaphores = signal_semaphores;
    batch->wait_stages = wait_stages;
    batch->wait_values = wait_values;
    batch->signal_values = signal_values;

    return ZEST_TRUE;
}

void zest__vk_add_image_barrier(zest_resource_node resource, zest_execution_barriers_t *barriers, zest_bool acquire, 
        zest_access_flags src_access, zest_access_flags dst_access, zest_image_layout old_layout, zest_image_layout new_layout, 
        zest_uint src_family, zest_uint dst_family, zest_pipeline_stage_flags src_stage, zest_pipeline_stage_flags dst_stage) {
    VkImageMemoryBarrier image_barrier = zest__vk_create_image_memory_barrier(
        VK_NULL_HANDLE,
        zest__to_vk_access_flags(src_access),
        zest__to_vk_access_flags(dst_access),
        zest__to_vk_image_layout(old_layout),
        zest__to_vk_image_layout(new_layout),
        zest__to_vk_image_aspect(resource->image.info.aspect_flags),
        0, resource->image.info.mip_levels);
    image_barrier.srcQueueFamilyIndex = src_family;
    image_barrier.dstQueueFamilyIndex = dst_family;
    if (acquire) {
        zest_vec_linear_push(ZestRenderer->frame_graph_allocator[ZEST_FIF], barriers->backend->acquire_image_barriers, image_barrier);
        zest_vec_linear_push(ZestRenderer->frame_graph_allocator[ZEST_FIF], barriers->acquire_image_barrier_nodes, resource);
		barriers->backend->overall_src_stage_mask_for_acquire_barriers |= zest__to_vk_pipeline_stage(src_stage);
		barriers->backend->overall_dst_stage_mask_for_acquire_barriers |= zest__to_vk_pipeline_stage(dst_stage);
    } else {
        zest_vec_linear_push(ZestRenderer->frame_graph_allocator[ZEST_FIF], barriers->backend->release_image_barriers, image_barrier);
        zest_vec_linear_push(ZestRenderer->frame_graph_allocator[ZEST_FIF], barriers->release_image_barrier_nodes, resource);
		barriers->backend->overall_src_stage_mask_for_release_barriers |= zest__to_vk_pipeline_stage(src_stage);
		barriers->backend->overall_dst_stage_mask_for_release_barriers |= zest__to_vk_pipeline_stage(dst_stage);
    }
}

void zest__vk_add_memory_buffer_barrier(zest_resource_node resource, zest_execution_barriers_t *barriers, zest_bool acquire, zest_access_flags src_access, zest_access_flags dst_access,
    zest_uint src_family, zest_uint dst_family, zest_pipeline_stage_flags src_stage, zest_pipeline_stage_flags dst_stage) {
    VkBufferMemoryBarrier buffer_barrier = zest__vk_create_buffer_memory_barrier(
        VK_NULL_HANDLE,
        zest__to_vk_access_flags(src_access),
        zest__to_vk_access_flags(dst_access),
        0, 0);
    buffer_barrier.srcQueueFamilyIndex = src_family;
    buffer_barrier.dstQueueFamilyIndex = dst_family;
    if (acquire) {
        zest_vec_linear_push(ZestRenderer->frame_graph_allocator[ZEST_FIF], barriers->backend->acquire_buffer_barriers, buffer_barrier);
        zest_vec_linear_push(ZestRenderer->frame_graph_allocator[ZEST_FIF], barriers->acquire_buffer_barrier_nodes, resource);
        barriers->backend->overall_src_stage_mask_for_acquire_barriers |= zest__to_vk_pipeline_stage(src_stage);
        barriers->backend->overall_dst_stage_mask_for_acquire_barriers |= zest__to_vk_pipeline_stage(dst_stage);
    } else {
        zest_vec_linear_push(ZestRenderer->frame_graph_allocator[ZEST_FIF], barriers->backend->release_buffer_barriers, buffer_barrier);
        zest_vec_linear_push(ZestRenderer->frame_graph_allocator[ZEST_FIF], barriers->release_buffer_barrier_nodes, resource);
        barriers->backend->overall_src_stage_mask_for_release_barriers |= zest__to_vk_pipeline_stage(src_stage);
        barriers->backend->overall_dst_stage_mask_for_release_barriers |= zest__to_vk_pipeline_stage(dst_stage);
    }
}

void zest__vk_validate_barrier_pipeline_stages(zest_execution_barriers_t *barriers) {
	// Handle TOP_OF_PIPE if no actual prior stages generated by resource usage
	if (barriers->backend->overall_src_stage_mask_for_acquire_barriers == 0) {
		barriers->backend->overall_src_stage_mask_for_acquire_barriers = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
	}
	// Handle BOTTOM_OF_PIPE if no actual subsequent stages (though less common for pre-pass barriers)
	if (barriers->backend->overall_dst_stage_mask_for_acquire_barriers == 0) {
		barriers->backend->overall_dst_stage_mask_for_acquire_barriers = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
	}

	// Handle TOP_OF_PIPE if no actual prior stages generated by resource usage
	if (barriers->backend->overall_src_stage_mask_for_release_barriers == 0) {
		barriers->backend->overall_src_stage_mask_for_release_barriers = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
	}
	// Handle BOTTOM_OF_PIPE if no actual subsequent stages (though less common for pre-pass barriers)
	if (barriers->backend->overall_dst_stage_mask_for_release_barriers == 0) {
		barriers->backend->overall_dst_stage_mask_for_release_barriers = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
	}
}
// -- End Internal_Frame_graph_context_functions


// -- Frame_graph_context_functions
zest_bool zest_cmd_UploadBuffer(const zest_frame_graph_context context, zest_buffer_uploader_t *uploader) {
    if (!zest_vec_size(uploader->buffer_copies)) {
        return ZEST_FALSE;
    }

    VkBufferCopy *buffer_copies = 0;
    zest_vec_foreach(i, uploader->buffer_copies) {
        zest_buffer_copy_t *copy = &uploader->buffer_copies[i];
        zest_vec_linear_push(ZestRenderer->frame_graph_allocator[ZEST_FIF], buffer_copies, 
            ((VkBufferCopy){ copy->src_offset, copy->dst_offset, copy->size }));
    }

    vkCmdCopyBuffer(context->backend->command_buffer, *zest__vk_get_device_buffer(uploader->source_buffer), *zest__vk_get_device_buffer(uploader->target_buffer), zest_vec_size(buffer_copies), buffer_copies);

    zest_vec_clear(uploader->buffer_copies);
    uploader->flags = 0;
    uploader->source_buffer = 0;
    uploader->target_buffer = 0;

    return ZEST_TRUE;
}

void zest_cmd_DrawIndexed(const zest_frame_graph_context context, zest_uint index_count, zest_uint instance_count, zest_uint first_index, int32_t vertex_offset, zest_uint first_instance) {
    ZEST_ASSERT_HANDLE(context);        //Not valid context, this command must be called within a frame graph execution callback
    vkCmdDrawIndexed(context->backend->command_buffer, index_count, instance_count, first_index, vertex_offset, first_instance);
}

void zest_cmd_CopyBuffer(const zest_frame_graph_context context, zest_buffer src_buffer, zest_buffer dst_buffer, VkDeviceSize size) {
    ZEST_ASSERT(size <= src_buffer->size);        //size must be less than or equal to the staging buffer size and the device buffer size
    ZEST_ASSERT(size <= dst_buffer->size);
    ZEST_ASSERT_HANDLE(context);                  //Not valid context, this command must be called within a frame graph execution callback
    VkBufferCopy copyInfo = { 0 };
    copyInfo.srcOffset = src_buffer->buffer_offset;
    copyInfo.dstOffset = dst_buffer->buffer_offset;
    copyInfo.size = size;
    vkCmdCopyBuffer(context->backend->command_buffer, src_buffer->backend->vk_buffer, dst_buffer->backend->vk_buffer, 1, &copyInfo);
}

void zest_cmd_BindPipelineShaderResource(const zest_frame_graph_context context, zest_pipeline pipeline, zest_shader_resources_handle handle) {
    zest_shader_resources shader_resources = (zest_shader_resources)zest__get_store_resource_checked(&ZestRenderer->shader_resources, handle.value);
    ZEST_ASSERT_HANDLE(context);        //Not valid context, this command must be called within a frame graph execution callback
    zest_vec_foreach(set_index, shader_resources->sets[ZEST_FIF]) {
        zest_descriptor_set set = shader_resources->sets[ZEST_FIF][set_index];
        ZEST_ASSERT_HANDLE(set);     //Not a valid desriptor set in the shader resource. Did you set all frames in flight?
		zest_vec_push(shader_resources->backend->binding_sets, set->backend->vk_descriptor_set);
	}
    vkCmdBindPipeline(context->backend->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->backend->pipeline);
    vkCmdBindDescriptorSets(context->backend->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->backend->pipeline_layout, 0, zest_vec_size(shader_resources->backend->binding_sets), shader_resources->backend->binding_sets, 0, 0);
    zest_vec_clear(shader_resources->backend->binding_sets);
}

void zest_cmd_BindPipeline(const zest_frame_graph_context context, zest_pipeline pipeline, zest_descriptor_set *descriptor_sets, zest_uint set_count) {
    ZEST_ASSERT_HANDLE(context);        //Not valid context, this command must be called within a frame graph execution callback
    ZEST_ASSERT(set_count && descriptor_sets);    //No descriptor sets. Must bind the pipeline with a valid desriptor set
    zloc_linear_allocator_t *allocator = ZestRenderer->frame_graph_allocator[ZEST_FIF];
    VkDescriptorSet *vk_sets = 0;
    zest_vec_linear_resize(allocator, vk_sets, set_count);
    for (zest_uint i = 0; i < set_count; ++i) {
        vk_sets[i] = descriptor_sets[i]->backend->vk_descriptor_set;
    }
    vkCmdBindPipeline(context->backend->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->backend->pipeline);
    vkCmdBindDescriptorSets(context->backend->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->backend->pipeline_layout, 0, set_count, vk_sets, 0, 0);
}

void zest_cmd_BindComputePipeline(const zest_frame_graph_context context, zest_compute_handle compute_handle, zest_descriptor_set *descriptor_sets, zest_uint set_count) {
    ZEST_ASSERT_HANDLE(context);        //Not valid context, this command must be called within a frame graph execution callback
    zest_compute compute = (zest_compute)zest__get_store_resource_checked(&ZestRenderer->compute_pipelines, compute_handle.value);
    ZEST_ASSERT(set_count && descriptor_sets);   //No descriptor sets. Must bind the pipeline with a valid desriptor set
    zloc_linear_allocator_t *allocator = ZestRenderer->frame_graph_allocator[ZEST_FIF];
    VkDescriptorSet *vk_sets = 0;
    zest_vec_linear_resize(allocator, vk_sets, set_count);
    for (zest_uint i = 0; i < set_count; ++i) {
        vk_sets[i] = descriptor_sets[i]->backend->vk_descriptor_set;
    }
    vkCmdBindPipeline(context->backend->command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, compute->backend->pipeline);
    vkCmdBindDescriptorSets(context->backend->command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, compute->backend->pipeline_layout, 0, set_count, vk_sets, 0, 0);
}

void zest_cmd_BindVertexBuffer(const zest_frame_graph_context context, zest_buffer buffer) {
    ZEST_ASSERT_HANDLE(context);        //Not valid context, this command must be called within a frame graph execution callback
    VkDeviceSize offsets[] = { buffer->buffer_offset };
    vkCmdBindVertexBuffers(context->backend->command_buffer, 0, 1, zest__vk_get_device_buffer(buffer), offsets);
}

void zest_cmd_BindIndexBuffer(const zest_frame_graph_context context, zest_buffer buffer) {
    ZEST_ASSERT_HANDLE(context);        //Not valid context, this command must be called within a frame graph execution callback
    vkCmdBindIndexBuffer(context->backend->command_buffer, *zest__vk_get_device_buffer(buffer), buffer->buffer_offset, VK_INDEX_TYPE_UINT32);
}

void zest_cmd_SendPushConstants(const zest_frame_graph_context context, zest_pipeline pipeline, void *data) {
    ZEST_ASSERT_HANDLE(context);        //Not valid context, this command must be called within a frame graph execution callback
    vkCmdPushConstants(context->backend->command_buffer, pipeline->backend->pipeline_layout, zest__to_vk_shader_stage(pipeline->pipeline_template->push_constant_range.stage_flags), pipeline->pipeline_template->push_constant_range.offset, pipeline->pipeline_template->push_constant_range.size, data);
}

void zest_cmd_SendCustomComputePushConstants(const zest_frame_graph_context context, zest_compute_handle compute_handle, const void *push_constant) {
    ZEST_ASSERT_HANDLE(context);        //Not valid context, this command must be called within a frame graph execution callback
    zest_compute compute = (zest_compute)zest__get_store_resource_checked(&ZestRenderer->compute_pipelines, compute_handle.value);
    vkCmdPushConstants(context->backend->command_buffer, compute->backend->pipeline_layout, compute->backend->push_constant_range.stageFlags, 0, compute->backend->push_constant_range.size, push_constant);
}

void zest_cmd_SendStandardPushConstants(const zest_frame_graph_context context, zest_pipeline_t* pipeline, void* data) {
    ZEST_ASSERT_HANDLE(context);        //Not valid context, this command must be called within a frame graph execution callback
    vkCmdPushConstants(context->backend->command_buffer, pipeline->backend->pipeline_layout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(zest_push_constants_t), data);
}

void zest_cmd_Draw(const zest_frame_graph_context context, zest_uint vertex_count, zest_uint instance_count, zest_uint first_vertex, zest_uint first_instance) {
    ZEST_ASSERT_HANDLE(context);        //Not valid context, this command must be called within a frame graph execution callback
    vkCmdDraw(context->backend->command_buffer, vertex_count, instance_count, first_vertex, first_instance);
}

void zest_cmd_DrawLayerInstruction(const zest_frame_graph_context context, zest_uint vertex_count, zest_layer_instruction_t *instruction) {
    ZEST_ASSERT_HANDLE(context);        //Not valid context, this command must be called within a frame graph execution callback
    vkCmdDraw(context->backend->command_buffer, vertex_count, instruction->total_indexes, 0, instruction->start_index);
}

void zest_cmd_DispatchCompute(const zest_frame_graph_context context, zest_compute_handle compute_handle, zest_uint group_count_x, zest_uint group_count_y, zest_uint group_count_z) {
    ZEST_ASSERT_HANDLE(context);        //Not valid context, this command must be called within a frame graph execution callback
    vkCmdDispatch(context->backend->command_buffer, group_count_x, group_count_y, group_count_z);
}

void zest_cmd_SetScreenSizedViewport(const zest_frame_graph_context context, float min_depth, float max_depth) {
    //This function must be called within a frame graph execution pass callback
    ZEST_ASSERT(context);    //Must be a valid command buffer
    ZEST_ASSERT(context->backend->command_buffer);    //Must be a valid command buffer
    ZEST_ASSERT_HANDLE(context->frame_graph);
    ZEST_ASSERT_HANDLE(context->frame_graph->swapchain);    //frame graph must be set up with a swapchain to use this function
    zest_swapchain swapchain = context->frame_graph->swapchain;

    VkViewport viewport = {
        .width = (float)swapchain->window->window_width,
        .height = (float)swapchain->window->window_height,
        .minDepth = min_depth,
        .maxDepth = max_depth,
    };
    VkRect2D scissor = {
		.extent.width = swapchain->window->window_width,
		.extent.height = swapchain->window->window_height,
		.offset.x = 0,
		.offset.y = 0,
    };
    vkCmdSetViewport(context->backend->command_buffer, 0, 1, &viewport);
    vkCmdSetScissor(context->backend->command_buffer, 0, 1, &scissor);
}

void zest_cmd_Scissor(const zest_frame_graph_context context, zest_scissor_rect_t *scissor) {
    ZEST_ASSERT_HANDLE(context);        //Not valid context, this command must be called within a frame graph execution callback
    VkRect2D rect = { scissor->offset.x, scissor->offset.y, scissor->extent.width, scissor->extent.height };
	vkCmdSetScissor(context->backend->command_buffer, 0, 1, &rect);
}

void zest_cmd_BlitImageMip(const zest_frame_graph_context context, zest_resource_node src, zest_resource_node dst, zest_uint mip_to_blit, zest_supported_pipeline_stages pipeline_stage) {
    ZEST_ASSERT_HANDLE(context);        //Not valid context, this command must be called within a frame graph execution callback
    ZEST_ASSERT_HANDLE(src);
    ZEST_ASSERT_HANDLE(dst);
    ZEST_ASSERT(src->type == zest_resource_type_image && dst->type == zest_resource_type_image);
    //Source and destination images must be the same width/height and have the same number of mip levels
    ZEST_ASSERT(src->image.backend->vk_image);
    ZEST_ASSERT(dst->image.backend->vk_image);
    ZEST_ASSERT(src->image.info.extent.width == dst->image.info.extent.width);
    ZEST_ASSERT(src->image.info.extent.height == dst->image.info.extent.height);
    ZEST_ASSERT(src->image.info.mip_levels == dst->image.info.mip_levels);

    VkImage src_image = src->image.backend->vk_image;
    VkImage dst_image = dst->image.backend->vk_image;

    zest_uint mip_width = ZEST__MAX(1u, src->image.info.extent.width >> mip_to_blit);
    zest_uint mip_height = ZEST__MAX(1u, src->image.info.extent.height >> mip_to_blit);

    VkImageLayout src_current_layout = (VkImageLayout)(src->journey[src->current_state_index].usage.image_layout);
    VkImageLayout dst_current_layout = (VkImageLayout)(dst->journey[dst->current_state_index].usage.image_layout);

    //Blit the smallest mip level from the downsampled render target first
    VkImageMemoryBarrier blit_src_barrier = zest__vk_create_image_memory_barrier(src_image,
        0,
        VK_ACCESS_TRANSFER_READ_BIT,
        src_current_layout,
        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        src->image.info.aspect_flags,
        mip_to_blit, 1);
    zest__vk_place_image_barrier(context->backend->command_buffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, &blit_src_barrier);

    VkImageMemoryBarrier blit_dst_barrier = zest__vk_create_image_memory_barrier(dst_image,
        0,
        VK_ACCESS_TRANSFER_WRITE_BIT,
        dst_current_layout,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        dst->image.info.aspect_flags,
        mip_to_blit, 1);
    zest__vk_place_image_barrier(context->backend->command_buffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, &blit_dst_barrier);

    VkOffset3D base_offset = { 0 };
    VkImageBlit blit = { 0 };
    blit.srcSubresource.mipLevel = mip_to_blit;
    blit.srcSubresource.layerCount = 1;
    blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    blit.dstSubresource.mipLevel = mip_to_blit;
    blit.dstSubresource.layerCount = 1;
    blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    blit.dstOffsets[0] = base_offset;
    blit.srcOffsets[0] = base_offset;
    blit.dstOffsets[1].x = mip_width;
    blit.dstOffsets[1].y = mip_height;
    blit.dstOffsets[1].z = 1;
    blit.srcOffsets[1].x = mip_width;
    blit.srcOffsets[1].y = mip_height;
    blit.srcOffsets[1].z = 1;

    bool same_size = (blit.srcOffsets[1].x == blit.dstOffsets[1].x && blit.srcOffsets[1].y == blit.dstOffsets[1].y);

    vkCmdBlitImage(
        context->backend->command_buffer,
        src_image,
        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        dst_image,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blit, same_size ? VK_FILTER_NEAREST : VK_FILTER_LINEAR);

    blit_src_barrier = zest__vk_create_image_memory_barrier(src_image,
        VK_ACCESS_TRANSFER_READ_BIT,
        VK_ACCESS_SHADER_READ_BIT,
        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        src_current_layout,
        src->image.info.aspect_flags,
        mip_to_blit, 1);
    zest__vk_place_image_barrier(context->backend->command_buffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, &blit_src_barrier);

    blit_dst_barrier = zest__vk_create_image_memory_barrier(dst_image,
        VK_ACCESS_TRANSFER_WRITE_BIT,
        VK_ACCESS_SHADER_READ_BIT,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        dst_current_layout,
        dst->image.info.aspect_flags,
        mip_to_blit, 1);
    zest__vk_place_image_barrier(context->backend->command_buffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, &blit_dst_barrier);
}

void zest_cmd_CopyImageMip(const zest_frame_graph_context context, zest_resource_node src, zest_resource_node dst, zest_uint mip_to_copy, zest_supported_pipeline_stages pipeline_stage) {
    ZEST_ASSERT_HANDLE(context);        //Not valid context, this command must be called within a frame graph execution callback
    ZEST_ASSERT_HANDLE(src);
    ZEST_ASSERT_HANDLE(dst);
    ZEST_ASSERT(src->type == zest_resource_type_image && dst->type == zest_resource_type_image);
    //Source and destination images must be the same width/height and have the same number of mip levels
    ZEST_ASSERT(src->image.backend->vk_image);
    ZEST_ASSERT(dst->image.backend->vk_image);
    ZEST_ASSERT(src->image.info.extent.width == dst->image.info.extent.width);
    ZEST_ASSERT(src->image.info.extent.height == dst->image.info.extent.height);
    ZEST_ASSERT(src->image.info.mip_levels == dst->image.info.mip_levels);

    //You must ensure that when creating the images that you use usage hints to indicate that you intend to copy
    //the images. When creating a transient image resource you can set the usage hints in the zest_image_resource_info_t
    //type that you pass into zest_CreateTransientImageResource. Trying to copy images that don't have the appropriate
    //usage flags set will result in validation errors.
    ZEST_ASSERT(src->image.info.flags & zest_image_flag_transfer_src);
    ZEST_ASSERT(dst->image.info.flags & zest_image_flag_transfer_dst);

    VkImage src_image = src->image.backend->vk_image;
    VkImage dst_image = dst->image.backend->vk_image;

    zest_uint mip_width = ZEST__MAX(1u, src->image.info.extent.width >> mip_to_copy);
    zest_uint mip_height = ZEST__MAX(1u, src->image.info.extent.height >> mip_to_copy);

    VkImageLayout src_current_layout = (VkImageLayout)(src->journey[src->current_state_index].usage.image_layout);
    VkImageLayout dst_current_layout = (VkImageLayout)(dst->journey[dst->current_state_index].usage.image_layout);

    //Blit the smallest mip level from the downsampled render target first
    VkImageMemoryBarrier blit_src_barrier = zest__vk_create_image_memory_barrier(src_image,
        0,
        VK_ACCESS_TRANSFER_READ_BIT,
        src_current_layout,
        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        src->image.info.aspect_flags,
        mip_to_copy, 1);
    zest__vk_place_image_barrier(context->backend->command_buffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, &blit_src_barrier);

    VkImageMemoryBarrier blit_dst_barrier = zest__vk_create_image_memory_barrier(dst_image,
        0,
        VK_ACCESS_TRANSFER_WRITE_BIT,
        dst_current_layout,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        dst->image.info.aspect_flags,
        mip_to_copy, 1);
    zest__vk_place_image_barrier(context->backend->command_buffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, &blit_dst_barrier);

    VkOffset3D base_offset = { 0 };
    VkImageCopy image_copy = { 0 };
    image_copy.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    image_copy.srcSubresource.layerCount = 1;
    image_copy.srcSubresource.mipLevel = mip_to_copy;
    image_copy.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    image_copy.dstSubresource.layerCount = 1;
    image_copy.dstSubresource.mipLevel = mip_to_copy;
    image_copy.srcOffset = base_offset;
    image_copy.dstOffset = base_offset;
    image_copy.extent.width = mip_width;
    image_copy.extent.height = mip_height;
    image_copy.extent.depth = 1;

    vkCmdCopyImage(
        context->backend->command_buffer,
        src_image,
        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        dst_image,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &image_copy);

    blit_src_barrier = zest__vk_create_image_memory_barrier(src_image,
        VK_ACCESS_TRANSFER_READ_BIT,
        VK_ACCESS_SHADER_READ_BIT,
        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        src_current_layout,
        src->image.info.aspect_flags,
        mip_to_copy, 1);
    zest__vk_place_image_barrier(context->backend->command_buffer, VK_PIPELINE_STAGE_TRANSFER_BIT, pipeline_stage, &blit_src_barrier);

    blit_dst_barrier = zest__vk_create_image_memory_barrier(dst_image,
        VK_ACCESS_TRANSFER_WRITE_BIT,
        VK_ACCESS_SHADER_READ_BIT,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        dst_current_layout,
        dst->image.info.aspect_flags,
        mip_to_copy, 1);
    zest__vk_place_image_barrier(context->backend->command_buffer, VK_PIPELINE_STAGE_TRANSFER_BIT, pipeline_stage, &blit_dst_barrier);
}

void zest_cmd_Clip(const zest_frame_graph_context context, float x, float y, float width, float height, float minDepth, float maxDepth) {
    ZEST_ASSERT_HANDLE(context);        //Not valid context, this command must be called within a frame graph execution callback
    VkViewport view = { x, y, width, height, minDepth, maxDepth };
	VkRect2D scissor = {(zest_uint)width, (zest_uint)height, (zest_uint)x, (zest_uint)y};
	vkCmdSetViewport(context->backend->command_buffer, 0, 1, &view);
	vkCmdSetScissor(context->backend->command_buffer, 0, 1, &scissor);
}

void zest_cmd_InsertComputeImageBarrier(const zest_frame_graph_context context, zest_resource_node resource, zest_uint base_mip) {
    ZEST_ASSERT_HANDLE(context);        //Not valid context, this command must be called within a frame graph execution callback
    ZEST_ASSERT_HANDLE(resource);    //Not a valid resource handle!
    ZEST_ASSERT(resource->type == zest_resource_type_image);    //resource type must be an image
	VkImageMemoryBarrier barrier = {
		.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
		.pNext = NULL,
		.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT,
		.dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
		.oldLayout = VK_IMAGE_LAYOUT_GENERAL,
		.newLayout = VK_IMAGE_LAYOUT_GENERAL,
		.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
		.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
		.image = resource->image.backend->vk_image,
		.subresourceRange = {
			.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
			.baseMipLevel = base_mip,
			.levelCount = 1,
			.baseArrayLayer = 0,
			.layerCount = 1
		}
	};

	vkCmdPipelineBarrier(
		context->backend->command_buffer,
		VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
		VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
		0,
		0, NULL,
		0, NULL,
		1, &barrier
	);
}

zest_bool zest_cmd_ImageClear(zest_image_handle handle, const zest_frame_graph_context context) {
    zest_image image = (zest_image)zest__get_store_resource_checked(&ZestRenderer->images, handle.value);
    VkCommandBuffer command_buffer = context ? context->backend->command_buffer : ZestRenderer->backend->one_time_command_buffer;
	ZEST_ASSERT(command_buffer);	//You must call begin_single_time_command_buffer before calling this fuction

    VkImageSubresourceRange image_sub_resource_range;
    image_sub_resource_range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    image_sub_resource_range.baseMipLevel = 0;
    image_sub_resource_range.levelCount = 1;
    image_sub_resource_range.baseArrayLayer = 0;
    image_sub_resource_range.layerCount = 1;

    VkClearColorValue ClearColorValue = { 0.0, 0.0, 0.0, 0.0 };

    zest__vk_insert_image_memory_barrier(
        command_buffer,
        image->backend->vk_image,
        0,
        VK_ACCESS_TRANSFER_WRITE_BIT,
        image->backend->vk_current_layout,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        image_sub_resource_range);

    vkCmdClearColorImage(command_buffer, image->backend->vk_image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &ClearColorValue, 1, &image_sub_resource_range);

    /*
    zest__vk_insert_image_memory_barrier(
        command_buffer,
        texture->image.image,
        0,
        VK_ACCESS_TRANSFER_WRITE_BIT,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        image_sub_resource_range);
	*/

    return ZEST_TRUE;
}

zest_bool zest_cmd_CopyImageToImage(zest_image_handle src_handle, zest_image_handle dst_handle, int src_x, int src_y, int dst_x, int dst_y, int width, int height) {
    zest_image src_image = (zest_image)zest__get_store_resource_checked(&ZestRenderer->images, src_handle.value);
    ZEST_ASSERT_HANDLE(src_image);    //Not a valid texture resource
    zest_image dst_image = (zest_image)zest__get_store_resource_checked(&ZestRenderer->images, dst_handle.value);
    ZEST_ASSERT_HANDLE(dst_image);	    //Not a valid handle!
    VkCommandBuffer copy_command = 0; 
    ZEST_ASSERT(ZestRenderer->backend->one_time_command_buffer);    //You must call begin_single_time_command_buffer before calling this fuction

    VkImageLayout target_layout = dst_image->backend->vk_current_layout;

    VkImageSubresourceRange image_range = { 0 };
    image_range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    image_range.baseMipLevel = 0;
    image_range.levelCount = 1;
    image_range.baseArrayLayer = 0;
    image_range.layerCount = 1;

    // Transition destination image to transfer destination layout
    zest__vk_insert_image_memory_barrier(
        copy_command,
        dst_image->backend->vk_image,
        0,
        VK_ACCESS_TRANSFER_WRITE_BIT,
        dst_image->backend->vk_current_layout,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        image_range);

    // Transition swapchain image from present to transfer source layout
    zest__vk_insert_image_memory_barrier(
        copy_command,
        src_image->backend->vk_image,
        VK_ACCESS_MEMORY_READ_BIT,
        VK_ACCESS_TRANSFER_READ_BIT,
        src_image->backend->vk_current_layout,
        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        image_range);

    VkOffset3D src_blit_offset;
    src_blit_offset.x = src_x;
    src_blit_offset.y = src_y;
    src_blit_offset.z = 0;

    VkOffset3D dst_blit_offset;
    dst_blit_offset.x = dst_x;
    dst_blit_offset.y = dst_y;
    dst_blit_offset.z = 0;

    VkOffset3D blit_size_src;
    blit_size_src.x = src_x + width;
    blit_size_src.y = src_y + height;
    blit_size_src.z = 1;

    VkOffset3D blit_size_dst;
    blit_size_dst.x = dst_x + width;
    blit_size_dst.y = dst_y + height;
    blit_size_dst.z = 1;

    VkImageBlit image_blit_region = { 0 };
    image_blit_region.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    image_blit_region.srcSubresource.layerCount = 1;
    image_blit_region.srcOffsets[0] = src_blit_offset;
    image_blit_region.srcOffsets[1] = blit_size_src;
    image_blit_region.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    image_blit_region.dstSubresource.layerCount = 1;
    image_blit_region.dstOffsets[0] = dst_blit_offset;
    image_blit_region.dstOffsets[1] = blit_size_dst;

    // Issue the blit command
    vkCmdBlitImage(
        copy_command,
        src_image->backend->vk_image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        dst_image->backend->vk_image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        1,
        &image_blit_region,
        VK_FILTER_NEAREST);

    // Transition destination image to general layout, which is the required layout for mapping the image memory later on
    zest__vk_insert_image_memory_barrier(
        copy_command,
        dst_image->backend->vk_image,
        VK_ACCESS_TRANSFER_WRITE_BIT,
        VK_ACCESS_MEMORY_READ_BIT,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        target_layout,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        image_range);

    zest__vk_insert_image_memory_barrier(
        copy_command,
        src_image->backend->vk_image,
        VK_ACCESS_TRANSFER_READ_BIT,
        VK_ACCESS_MEMORY_READ_BIT,
        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        src_image->backend->vk_current_layout,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        image_range);

    return ZEST_FALSE;
}

zest_bool zest_CopyTextureToBitmap(zest_image_handle src_handle, zest_bitmap dst_bitmap, int src_x, int src_y, int dst_x, int dst_y, int width, int height) {
    zest_image src_image = (zest_image)zest__get_store_resource_checked(&ZestRenderer->images, src_handle.value);

    // Ensure the destination bitmap can hold the copied region
    ZEST_ASSERT(dst_x + width <= dst_bitmap->meta.width);
    ZEST_ASSERT(dst_y + height <= dst_bitmap->meta.height);

    VkCommandBuffer copy_command = 0;
    ZEST_ASSERT(ZestRenderer->backend->one_time_command_buffer);    //You must call begin_single_time_command_buffer before calling this fuction

    VkImage temp_image;
    VkDeviceMemory temp_memory;
    // Create a temporary image that is host-visible
    ZEST_RETURN_FALSE_ON_FAIL(zest__vk_create_temporary_image(width, height, 1, VK_SAMPLE_COUNT_1_BIT, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_TILING_LINEAR, VK_IMAGE_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &temp_image, &temp_memory));

    VkImageSubresourceRange image_range = { 0 };
    image_range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    image_range.baseMipLevel = 0;
    image_range.levelCount = 1;
    image_range.baseArrayLayer = 0;
    image_range.layerCount = 1;

    // Transition temporary image to be ready as a transfer destination
    zest__vk_insert_image_memory_barrier(
        copy_command, temp_image, 0, VK_ACCESS_TRANSFER_WRITE_BIT,
        VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, image_range);

    // Transition source image to be ready as a transfer source
    zest__vk_insert_image_memory_barrier(
        copy_command, src_image->backend->vk_image, VK_ACCESS_MEMORY_READ_BIT, VK_ACCESS_TRANSFER_READ_BIT,
        src_image->backend->vk_current_layout, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, image_range);

    zest_bool supports_blit = zest__vk_has_blit_support(src_image->backend->vk_format);

    if (supports_blit) {
        VkImageBlit image_blit_region = { 0 };
        image_blit_region.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        image_blit_region.srcSubresource.layerCount = 1;
        image_blit_region.srcOffsets[0] = (VkOffset3D){ src_x, src_y, 0 };
        image_blit_region.srcOffsets[1] = (VkOffset3D){ src_x + width, src_y + height, 1 };
        image_blit_region.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        image_blit_region.dstSubresource.layerCount = 1;
        image_blit_region.dstOffsets[0] = (VkOffset3D){ 0, 0, 0 }; // Corrected: Copy to origin of temp image
        image_blit_region.dstOffsets[1] = (VkOffset3D){ width, height, 1 };

        vkCmdBlitImage(copy_command,
            src_image->backend->vk_image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            temp_image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            1, &image_blit_region, VK_FILTER_NEAREST);
    } else {
        VkImageCopy image_copy_region = { 0 };
        image_copy_region.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        image_copy_region.srcSubresource.layerCount = 1;
        image_copy_region.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        image_copy_region.dstSubresource.layerCount = 1;
        image_copy_region.srcOffset = (VkOffset3D){ src_x, src_y, 0 };
        image_copy_region.dstOffset = (VkOffset3D){ 0, 0, 0 }; // Corrected: Copy to origin of temp image
        image_copy_region.extent = (VkExtent3D){ width, height, 1 };

        vkCmdCopyImage(copy_command,
            src_image->backend->vk_image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            temp_image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            1, &image_copy_region);
    }

    // Transition temporary image to be readable by the host
    zest__vk_insert_image_memory_barrier(
        copy_command, temp_image, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL,
        VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, image_range);

    // Transition source image back to its original layout
    zest__vk_insert_image_memory_barrier(
        copy_command, src_image->backend->vk_image, VK_ACCESS_TRANSFER_READ_BIT, VK_ACCESS_MEMORY_READ_BIT,
        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, src_image->backend->vk_current_layout,
        VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, image_range);

    // Get layout of the temporary image to get the row pitch for correct memory copy
    VkImageSubresource sub_resource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0 };
    VkSubresourceLayout sub_resource_layout;
    vkGetImageSubresourceLayout(ZestDevice->backend->logical_device, temp_image, &sub_resource, &sub_resource_layout);

    // Manually swizzle if the source is BGR and we couldn't use blit (which handles conversion automatically)
    zest_bool color_swizzle = !supports_blit && 
        (src_image->backend->vk_format == VK_FORMAT_B8G8R8A8_SRGB || 
         src_image->backend->vk_format == VK_FORMAT_B8G8R8A8_UNORM || 
         src_image->backend->vk_format == VK_FORMAT_B8G8R8A8_SNORM);

    void* mapped_data;
    vkMapMemory(ZestDevice->backend->logical_device, temp_memory, 0, VK_WHOLE_SIZE, 0, &mapped_data);

    const zest_byte* src_ptr = (const zest_byte*)mapped_data;
    zest_byte* dst_ptr = dst_bitmap->data + (dst_y * dst_bitmap->meta.stride) + (dst_x * dst_bitmap->meta.channels);
    
    // Copy row by row, respecting the source row pitch and destination stride
    for (int y = 0; y < height; ++y) {
        if (color_swizzle) {
            // Perform copy and BGR->RGB swizzle in one pass
            for (int x = 0; x < width; ++x) {
                const zest_byte* src_pixel = src_ptr + (x * 4);
                zest_byte* dst_pixel = dst_ptr + (x * dst_bitmap->meta.channels);
                dst_pixel[0] = src_pixel[2]; // R
                dst_pixel[1] = src_pixel[1]; // G
                dst_pixel[2] = src_pixel[0]; // B
                dst_pixel[3] = src_pixel[3]; // A
            }
        } else {
            // Simple memcpy for the row
            memcpy(dst_ptr, src_ptr, width * 4); // Temp image is always 4 channels (RGBA)
        }
        src_ptr += sub_resource_layout.rowPitch;
        dst_ptr += dst_bitmap->meta.stride;
    }

    vkUnmapMemory(ZestDevice->backend->logical_device, temp_memory);
    vkFreeMemory(ZestDevice->backend->logical_device, temp_memory, &ZestDevice->backend->allocation_callbacks);
    vkDestroyImage(ZestDevice->backend->logical_device, temp_image, &ZestDevice->backend->allocation_callbacks);
    
    return ZEST_TRUE;
}

zest_bool zest__vk_copy_buffer_to_image(zest_buffer buffer, zest_size src_offset, zest_image image, zest_uint width, zest_uint height) {
    VkBufferImageCopy region = { 0 };
    region.bufferOffset = src_offset;
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;

    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = 1;

    region.imageOffset.x = 0;
    region.imageOffset.y = 0;
    region.imageOffset.z = 0;
    region.imageExtent.width = width;
    region.imageExtent.height = height;
    region.imageExtent.depth = 1;

    vkCmdCopyBufferToImage(ZestRenderer->backend->one_time_command_buffer, buffer->backend->vk_buffer, image->backend->vk_image, image->backend->vk_current_layout, 1, &region);

    return ZEST_TRUE;
}

zest_bool zest_cmd_CopyBitmapToImage(zest_bitmap bitmap, zest_image_handle dst_handle, int src_x, int src_y, int dst_x, int dst_y, int width, int height) {
    zest_image dst_image = (zest_image)zest__get_store_resource_checked(&ZestRenderer->images, dst_handle.value);
    zest_uint channels = bitmap->meta.channels;
    ZEST_ASSERT(channels == zest__get_format_channel_count(dst_image->info.format));    //Incompatible destination image format
    VkDeviceSize image_size = bitmap->meta.width * bitmap->meta.height * channels;

    zest_buffer staging_buffer = 0;
	zest_buffer_info_t buffer_info = zest_CreateStagingBufferInfo();
	staging_buffer = zest_CreateBuffer(image_size, &buffer_info);
	if (!staging_buffer) {
		return ZEST_FALSE;
	}
	memcpy(staging_buffer->data, bitmap->data, (zest_size)image_size);

    ZEST_CLEANUP_ON_FALSE(zest__vk_begin_single_time_commands());

	ZEST_CLEANUP_ON_FALSE(zest__vk_transition_image_layout(dst_image, zest_image_layout_transfer_dst_optimal, 0, dst_image->info.mip_levels, 0, dst_image->info.layer_count));

	ZEST_CLEANUP_ON_FALSE(zest__vk_copy_buffer_to_image(staging_buffer, staging_buffer->buffer_offset, dst_image, (zest_uint)bitmap->meta.width, (zest_uint)bitmap->meta.height));
	zest_FreeBuffer(staging_buffer);
    if (dst_image->info.mip_levels > 1) {
        ZEST_CLEANUP_ON_FALSE(zest__vk_generate_mipmaps(dst_image));
    }
	ZEST_CLEANUP_ON_FALSE(zest__vk_transition_image_layout(dst_image, zest_image_layout_shader_read_only_optimal, 0, dst_image->info.mip_levels, 0, dst_image->info.layer_count));

    ZEST_CLEANUP_ON_FALSE(zest__vk_end_single_time_commands());

    return ZEST_TRUE;

cleanup:
    if (staging_buffer) {
        zest_FreeBuffer(staging_buffer);
    }
    return ZEST_TRUE;
}

void zest_cmd_DrawInstanceLayer(const zest_frame_graph_context context, void *user_data) {
    zest_layer_handle layer_handle = *(zest_layer_handle*)user_data;
    zest_layer layer = (zest_layer)zest__get_store_resource_checked(&ZestRenderer->layers, layer_handle.value);

	VkCommandBuffer command_buffer = context->backend->command_buffer;
    if (!layer->vertex_buffer_node) return; //It could be that the frame graph culled the pass because it was unreferenced or disabled
    if (!layer->vertex_buffer_node->storage_buffer) return;
	zest_buffer device_buffer = layer->vertex_buffer_node->storage_buffer;
	VkDeviceSize instance_data_offsets[] = { device_buffer->buffer_offset };
	vkCmdBindVertexBuffers(command_buffer, 0, 1, &device_buffer->backend->vk_buffer, instance_data_offsets);

    bool has_instruction_view_port = false;
    zest_vec_foreach(i, layer->draw_instructions[layer->fif]) {
        zest_layer_instruction_t* current = &layer->draw_instructions[layer->fif][i];

        if (current->draw_mode == zest_draw_mode_viewport) {
			VkViewport viewport = { current->viewport.x, current->viewport.y, current->viewport.width, current->viewport.height, current->viewport.minDepth, current->viewport.maxDepth };
			VkRect2D scissor = { current->scissor.offset.x, current->scissor.offset.y, current->scissor.extent.width, current->scissor.extent.height };
            vkCmdSetViewport(command_buffer, 0, 1, &viewport);
            vkCmdSetScissor(command_buffer, 0, 1, &scissor);
            has_instruction_view_port = true;
            continue;
        } else if(!has_instruction_view_port) {
			VkViewport viewport = { layer->viewport.x, layer->viewport.y, layer->viewport.width, layer->viewport.height, layer->viewport.minDepth, layer->viewport.maxDepth };
			VkRect2D scissor = { layer->scissor.offset.x, layer->scissor.offset.y, layer->scissor.extent.width, layer->scissor.extent.height };
            vkCmdSetViewport(command_buffer, 0, 1, &viewport);
            vkCmdSetScissor(command_buffer, 0, 1, &scissor);
        }

        ZEST_ASSERT(current->shader_resources.value); //Shader resources handle must be set in the instruction

        zest_pipeline pipeline = zest_PipelineWithTemplate(current->pipeline_template, context);
        if (pipeline) {
			zest_cmd_BindPipelineShaderResource(context, pipeline, current->shader_resources);
        } else {
            continue;
        }

		vkCmdPushConstants(
			command_buffer,
			pipeline->backend->pipeline_layout,
			zest__to_vk_shader_stage(pipeline->pipeline_template->push_constant_range.stage_flags),
			pipeline->pipeline_template->push_constant_range.offset,
			pipeline->pipeline_template->push_constant_range.offset,
			&current->push_constant);

        vkCmdDraw(command_buffer, 6, current->total_instances, 0, current->start_index);
    }
    if (ZEST__NOT_FLAGGED(layer->flags, zest_layer_flag_manual_fif)) {
		zest__reset_instance_layer_drawing(layer);
    }
}

void zest_cmd_DrawInstanceMeshLayer(const zest_frame_graph_context context, void *user_data) {
    zest_layer_handle layer_handle = *(zest_layer_handle*)user_data;
    zest_layer layer = (zest_layer)zest__get_store_resource_checked(&ZestRenderer->layers, layer_handle.value);

    VkCommandBuffer command_buffer = context->backend->command_buffer;

    if (layer->vertex_data && layer->index_data) {
        zest_cmd_BindMeshVertexBuffer(context, layer_handle);
        zest_cmd_BindMeshIndexBuffer(context, layer_handle);
    } else {
        ZEST_PRINT("No Vertex/Index data found in mesh layer [%s]!", layer->name);
        return;
    }

    ZEST_ASSERT_HANDLE(layer->vertex_buffer_node);  //No resource node in the layer, check that you added this layer as a 
                                                    //resource to the frame graph

	zest_buffer device_buffer = layer->vertex_buffer_node->storage_buffer;
	VkDeviceSize instance_data_offsets[] = { device_buffer->buffer_offset };
	vkCmdBindVertexBuffers(command_buffer, 1, 1, &device_buffer->backend->vk_buffer, instance_data_offsets);

    bool has_instruction_view_port = false;
    zest_vec_foreach(i, layer->draw_instructions[layer->fif]) {
        zest_layer_instruction_t *current = &layer->draw_instructions[layer->fif][i];

        if (current->draw_mode == zest_draw_mode_viewport) {
			VkViewport viewport = { current->viewport.x, current->viewport.y, current->viewport.width, current->viewport.height, current->viewport.minDepth, current->viewport.maxDepth };
			VkRect2D scissor = { current->scissor.offset.x, current->scissor.offset.y, current->scissor.extent.width, current->scissor.extent.height };
            vkCmdSetViewport(command_buffer, 0, 1, &viewport);
            vkCmdSetScissor(command_buffer, 0, 1, &scissor);
            has_instruction_view_port = true;
            continue;
        } else if(!has_instruction_view_port) {
			VkViewport viewport = { layer->viewport.x, layer->viewport.y, layer->viewport.width, layer->viewport.height, layer->viewport.minDepth, layer->viewport.maxDepth };
			VkRect2D scissor = { layer->scissor.offset.x, layer->scissor.offset.y, layer->scissor.extent.width, layer->scissor.extent.height };
            vkCmdSetViewport(command_buffer, 0, 1, &viewport);
            vkCmdSetScissor(command_buffer, 0, 1, &scissor);
        }

        ZEST_ASSERT(current->shader_resources.value);

        zest_pipeline pipeline = zest_PipelineWithTemplate(current->pipeline_template, context);
        if (pipeline) {
			zest_cmd_BindPipelineShaderResource(context, pipeline, current->shader_resources);
        } else {
            continue;
        }

        vkCmdPushConstants(
            command_buffer,
            pipeline->backend->pipeline_layout,
			zest__to_vk_shader_stage(pipeline->pipeline_template->push_constant_range.stage_flags),
			pipeline->pipeline_template->push_constant_range.offset,
			pipeline->pipeline_template->push_constant_range.size,
            current->push_constant);

        vkCmdDrawIndexed(command_buffer, layer->index_count, current->total_instances, 0, 0, current->start_index);
    }
    if (ZEST__NOT_FLAGGED(layer->flags, zest_layer_flag_manual_fif)) {
		zest__reset_instance_layer_drawing(layer);
    }
}

void zest_cmd_BindMeshVertexBuffer(const zest_frame_graph_context context, zest_layer_handle layer_handle) {
    ZEST_ASSERT_HANDLE(context);        //Not valid context, this command must be called within a frame graph execution callback
    zest_layer layer = (zest_layer)zest__get_store_resource_checked(&ZestRenderer->layers, layer_handle.value);
    ZEST_ASSERT(layer->vertex_data);    //There's no vertex data in the buffer. Did you call zest_AddMeshToLayer?
    zest_buffer_t *buffer = layer->vertex_data;
    VkDeviceSize offsets[] = { buffer->buffer_offset };
    vkCmdBindVertexBuffers(context->backend->command_buffer, 0, 1, zest__vk_get_device_buffer(buffer), offsets);
}

void zest_cmd_BindMeshIndexBuffer(const zest_frame_graph_context context, zest_layer_handle layer_handle) {
    ZEST_ASSERT_HANDLE(context);        //Not valid context, this command must be called within a frame graph execution callback
    zest_layer layer = (zest_layer)zest__get_store_resource_checked(&ZestRenderer->layers, layer_handle.value);
    ZEST_ASSERT(layer->index_data);    //There's no index data in the buffer. Did you call zest_AddMeshToLayer?
    zest_buffer_t *buffer = layer->index_data;
    vkCmdBindIndexBuffer(context->backend->command_buffer, *zest__vk_get_device_buffer(buffer), buffer->buffer_offset, VK_INDEX_TYPE_UINT32);
}

// -- End Frame graph context functions

// -- Debug_functions
void zest__vk_print_compiled_frame_graph(zest_frame_graph frame_graph) {
    ZEST_ASSERT_HANDLE(frame_graph);        //Not a valid frame graph! Make sure you called BeginRenderGraph or BeginRenderToScreen
    ZEST_PRINT("--- frame graph Execution Plan, Current FIF: %i ---", ZEST_FIF);
    if (!ZEST_VALID_HANDLE(frame_graph)) {
        ZEST_PRINT("frame graph handle is NULL.");
        return;
    }

    if (!ZEST__FLAGGED(frame_graph->flags, zest_frame_graph_is_compiled)) {
        ZEST_PRINT("frame graph is not in a compiled state");
        return;
    }

    ZEST_PRINT("Resource List: Total Resources: %u\n", zest_bucket_array_size(&frame_graph->resources));

    zest_bucket_array_foreach(resource_index, frame_graph->resources) {
		zest_resource_node resource = zest_bucket_array_get(&frame_graph->resources, zest_resource_node_t, resource_index);
        if (resource->type == zest_resource_type_buffer) {
			ZEST_PRINT("Buffer: %s - Size: %zu", resource->name, resource->buffer_desc.size);
        } else if (resource->type & zest_resource_type_image) {
            ZEST_PRINT("Image: %s - VkImage: %p, Size: %u x %u", 
                resource->name, resource->image.backend->vk_image,  
                resource->image.info.extent.width, resource->image.info.extent.height);
        } else if (resource->type == zest_resource_type_swap_chain_image) {
            ZEST_PRINT("Image: %s - VkImage: %p", 
                resource->name, resource->image.backend->vk_image);
        }
    }

    ZEST_PRINT("");
    ZEST_PRINT("Number of Submission Batches: %u\n", zest_vec_size(frame_graph->submissions));

    zest_vec_foreach(submission_index, frame_graph->submissions) {
        zest_wave_submission_t *wave_submission = &frame_graph->submissions[submission_index];
		ZEST_PRINT("Wave Submission Index %i:", submission_index);
        for (zest_uint queue_index = 0; queue_index != ZEST_QUEUE_COUNT; ++queue_index) {
            zest_submission_batch_t *batch = &wave_submission->batches[queue_index];
            if (!batch->magic) continue;
            if (zest_map_valid_key(ZestDevice->queue_names, batch->queue_family_index)) {
                ZEST_PRINT("  Target Queue Family: %s - index: %u (VkQueue: %p)", *zest_map_at_key(ZestDevice->queue_names, batch->queue_family_index), batch->queue_family_index, (void *)batch->queue->backend->vk_queue);
            } else {
                ZEST_PRINT("  Target Queue Family: %s - index: %u (VkQueue: %p)", "Ignored", batch->queue_family_index, (void *)batch->queue->backend->vk_queue);
            }

            // --- Print Wait Semaphores for the Batch ---
            // (Your batch struct needs to store enough info for this, e.g., an array of wait semaphores and stages)
            // For simplicity, assuming single wait_on_batch_semaphore for now, and you'd identify if it's external
            if (batch->backend->final_wait_semaphores) {
                // This stage should ideally be stored with the batch submission info by EndRenderGraph
                ZEST_PRINT("  Waits on the following Semaphores:");
                zest_vec_foreach(semaphore_index, batch->backend->final_wait_semaphores) {
                    zest_text_t pipeline_stages = zest__vk_pipeline_stage_flags_to_string(zest__to_vk_pipeline_stage(batch->wait_stages[semaphore_index]));
                    if (zest_vec_size(batch->wait_values) && batch->wait_values[semaphore_index] > 0) {
                        ZEST_PRINT("     Timeline Semaphore: %p, Value: %zu at Stage: %s", (void *)batch->backend->final_wait_semaphores[semaphore_index], batch->wait_values[semaphore_index], pipeline_stages.str);
                    } else {
                        ZEST_PRINT("     Binary Semaphore:   %p at Stage: %s", (void *)batch->backend->final_wait_semaphores[semaphore_index], pipeline_stages.str);
                    }
                    zest_FreeText(&pipeline_stages);
                }
            } else {
                ZEST_PRINT("  Does not wait on any semaphores.");
            }

            ZEST_PRINT("  Passes in this batch:");
            zest_vec_foreach(batch_pass_index, batch->pass_indices) {
                int pass_index = batch->pass_indices[batch_pass_index];
                zest_pass_group_t *pass_node = &frame_graph->final_passes.data[pass_index];
                zest_execution_details_t *exe_details = &pass_node->execution_details;

                ZEST_PRINT("    Pass [%d] (QueueType: %d)",
                    pass_index, pass_node->queue_info.queue_type);
                zest_vec_foreach(pass_index, pass_node->passes) {
                    ZEST_PRINT("       %s", pass_node->passes[pass_index]->name);
                }

                if (zest_vec_size(exe_details->barriers.backend->acquire_buffer_barriers) > 0 ||
                    zest_vec_size(exe_details->barriers.backend->acquire_image_barriers) > 0) {
                    zest_text_t overal_src_pipeline_stages = zest__vk_pipeline_stage_flags_to_string(exe_details->barriers.backend->overall_src_stage_mask_for_acquire_barriers);
                    zest_text_t overal_dst_pipeline_stages = zest__vk_pipeline_stage_flags_to_string(exe_details->barriers.backend->overall_dst_stage_mask_for_acquire_barriers);
                    ZEST_PRINT("      Acquire Barriers (Overall Pipeline Src Stages: %s, Dst Stages: %s):",
                        overal_src_pipeline_stages.str,
                        overal_dst_pipeline_stages.str);
                    zest_FreeText(&overal_src_pipeline_stages);
                    zest_FreeText(&overal_dst_pipeline_stages);

                    ZEST_PRINT("        Images:");
                    zest_vec_foreach(barrier_index, exe_details->barriers.backend->acquire_image_barriers) {
                        VkImageMemoryBarrier *imb = &exe_details->barriers.backend->acquire_image_barriers[barrier_index];
                        zest_resource_node image_resource = exe_details->barriers.acquire_image_barrier_nodes[barrier_index];
                        zest_text_t src_access_mask = zest__vk_access_flags_to_string(imb->srcAccessMask);
                        zest_text_t dst_access_mask = zest__vk_access_flags_to_string(imb->dstAccessMask);
                        ZEST_PRINT("            %s, Layout: %s -> %s, Access: %s -> %s, QFI: %u -> %u",
                            image_resource->name,
                            zest__vk_image_layout_to_string(imb->oldLayout), zest__vk_image_layout_to_string(imb->newLayout),
                            src_access_mask.str, dst_access_mask.str,
                            imb->srcQueueFamilyIndex, imb->dstQueueFamilyIndex);
                        zest_FreeText(&src_access_mask);
                        zest_FreeText(&dst_access_mask);
                    }

                    ZEST_PRINT("        Buffers:");
                    zest_vec_foreach(barrier_index, exe_details->barriers.backend->acquire_buffer_barriers) {
                        VkBufferMemoryBarrier *bmb = &exe_details->barriers.backend->acquire_buffer_barriers[barrier_index];
                        zest_resource_node buffer_resource = exe_details->barriers.acquire_buffer_barrier_nodes[barrier_index];
                        // You need a robust way to get resource_name from bmb->image
                        zest_text_t src_access_mask = zest__vk_access_flags_to_string(bmb->srcAccessMask);
                        zest_text_t dst_access_mask = zest__vk_access_flags_to_string(bmb->dstAccessMask);
                        ZEST_PRINT("            %s | Access: %s -> %s, QFI: %u -> %u, Size: %zu",
                            buffer_resource->name,
                            src_access_mask.str, dst_access_mask.str,
                            bmb->srcQueueFamilyIndex, bmb->dstQueueFamilyIndex,
                            buffer_resource->buffer_desc.size);
                        zest_FreeText(&src_access_mask);
                        zest_FreeText(&dst_access_mask);
                    }
                }

                // Print Inputs and Outputs (simplified)
                // ...

                if (ZEST_VALID_HANDLE(exe_details->render_pass)) {
                    ZEST_PRINT("      VkRenderPass: %p, RenderArea: (%d,%d)-(%ux%u)",
                        (void *)exe_details->render_pass->backend->vk_render_pass, 
                        exe_details->render_area.offset.x, exe_details->render_area.offset.y,
                        exe_details->render_area.extent.width, exe_details->render_area.extent.height);
                    // Further detail: iterate VkRenderPassCreateInfo's attachments (if stored or re-derived)
                    // and print each VkAttachmentDescription's load/store/layouts and clear values.
                }

                if (zest_vec_size(exe_details->barriers.backend->release_buffer_barriers) > 0 ||
                    zest_vec_size(exe_details->barriers.backend->release_image_barriers) > 0) {
                    zest_text_t overal_src_pipeline_stages = zest__vk_pipeline_stage_flags_to_string(exe_details->barriers.backend->overall_src_stage_mask_for_release_barriers);
                    zest_text_t overal_dst_pipeline_stages = zest__vk_pipeline_stage_flags_to_string(exe_details->barriers.backend->overall_dst_stage_mask_for_release_barriers);
                    ZEST_PRINT("      Release Barriers (Overall Pipeline Src Stages: %s, Dst Stages: %s):",
                        overal_src_pipeline_stages.str,
                        overal_dst_pipeline_stages.str);
                    zest_FreeText(&overal_src_pipeline_stages);
                    zest_FreeText(&overal_dst_pipeline_stages);

                    ZEST_PRINT("        Images:");
                    zest_vec_foreach(barrier_index, exe_details->barriers.backend->release_image_barriers) {
                        VkImageMemoryBarrier *imb = &exe_details->barriers.backend->release_image_barriers[barrier_index];
                        zest_resource_node image_resource = exe_details->barriers.release_image_barrier_nodes[barrier_index];
                        zest_text_t src_access_mask = zest__vk_access_flags_to_string(imb->srcAccessMask);
                        zest_text_t dst_access_mask = zest__vk_access_flags_to_string(imb->dstAccessMask);
                        ZEST_PRINT("            %s, Layout: %s -> %s, Access: %s -> %s, QFI: %u -> %u",
                            image_resource->name,
                            zest__vk_image_layout_to_string(imb->oldLayout), zest__vk_image_layout_to_string(imb->newLayout),
                            src_access_mask.str, dst_access_mask.str,
                            imb->srcQueueFamilyIndex, imb->dstQueueFamilyIndex);
                        zest_FreeText(&src_access_mask);
                        zest_FreeText(&dst_access_mask);
                    }

                    ZEST_PRINT("        Buffers:");
                    zest_vec_foreach(barrier_index, exe_details->barriers.backend->release_buffer_barriers) {
                        VkBufferMemoryBarrier *bmb = &exe_details->barriers.backend->release_buffer_barriers[barrier_index];
                        zest_resource_node buffer_resource = exe_details->barriers.release_buffer_barrier_nodes[barrier_index];
                        // You need a robust way to get resource_name from bmb->image
                        zest_text_t src_access_mask = zest__vk_access_flags_to_string(bmb->srcAccessMask);
                        zest_text_t dst_access_mask = zest__vk_access_flags_to_string(bmb->dstAccessMask);
                        ZEST_PRINT("            %s, Access: %s -> %s, QFI: %u -> %u, Size: %zu",
                            buffer_resource->name,
                            src_access_mask.str, dst_access_mask.str,
                            bmb->srcQueueFamilyIndex, bmb->dstQueueFamilyIndex,
                            buffer_resource->buffer_desc.size);
                        zest_FreeText(&src_access_mask);
                        zest_FreeText(&dst_access_mask);
                    }
                }
            }

            // --- Print Signal Semaphores for the Batch ---
            if (batch->backend->final_signal_semaphores != 0) {
                ZEST_PRINT("  Signal Semaphores:");
                zest_vec_foreach(signal_index, batch->backend->final_signal_semaphores) {
                    if (batch->signal_values[signal_index] > 0) {
                        ZEST_PRINT("  Timeline Semaphore: %p, Value: %zu", (void *)batch->backend->final_signal_semaphores[signal_index], batch->signal_values[signal_index]);
                    } else {
                        ZEST_PRINT("  Binary Semaphore: %p", (void *)batch->backend->final_signal_semaphores[signal_index]);
                    }
                }
            }
            ZEST_PRINT(""); // End of batch
        }
    }
	ZEST_PRINT("--- End of Report ---");
}
#endif
// -- End Debug_functions

#ifdef __cplusplus
}
#endif

#endif	//ZEST_VULKAN_H