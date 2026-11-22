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

#ifdef __cplusplus
extern "C" {
#endif

#define ZEST_MTL_BACKEND(name, object) ((zest_mtl_##name##_backend_t *)(object)->backend)
#define ZEST_MTL_RELEASE(object) do { if (object) { [(object) release]; (object) = nil; } } while (0)
#define ZEST__MTL_UNAVAILABLE(member) "The Metal backend doesn't implement " member " yet. Use zest_BeginVulkanDeviceBuilder on macOS until it does."

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
ZEST_PRIVATE void zest__mtl_clip(const zest_command_list command_list, float x, float y, float width, float height, float minDepth, float maxDepth);
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

// -- Shader_code
// -- End Shader_code

// -- Backend_structs
// -- End Backend_structs

// -- Other_setup_structs
// -- End Other_setup_structs

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
// -- End Error_logging

// -- Enum_to_string_functions
// -- End Enum_to_string_functions

// -- Type_converters
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
// -- End Initialisation_functions

// -- Swapchain_setup
// -- End Swapchain_setup

// -- Swapchain_presenting
// -- End Swapchain_presenting

// -- Backend_setup_functions
// -- End Backend_setup_functions

// -- Backend_cleanup_functions
// -- End Backend_cleanup_functions

// -- Shared_events
// -- End Shared_events

// -- Command_pools
// -- End Command_pools

// -- Buffer_and_memory
// -- End Buffer_and_memory

// -- Bindless_table
// -- End Bindless_table

// -- General_renderer
// -- End General_renderer

// -- Device_OS
// -- End Device_OS

// -- Pipelines
// -- End Pipelines

// -- Images
// -- End Images

// -- General_helpers
// -- End General_helpers

// -- Internal_Frame_graph_context_functions
// -- End Internal_Frame_graph_context_functions

// -- Frame_graph_context_functions
// -- End Frame_graph_context_functions

// -- Shader_compiling
// -- End Shader_compiling

// -- GPU_Profiling_backend
// -- End GPU_Profiling_backend

// -- Debug_naming
// -- End Debug_naming

// -- Debug_functions
// -- End Debug_functions

// -- Unavailable_stubs
//Every member the backend doesn't serve yet. A member leaves this section when its own section implements it.
//Frame Graph Platform Commands
zest_bool zest__mtl_begin_command_buffer(const zest_command_list command_list) { return zest__mtl_report_unavailable(command_list->context->device, ZEST__MTL_UNAVAILABLE("begin_command_buffer")); }
void zest__mtl_end_command_buffer(const zest_command_list command_list) { zest__mtl_report_unavailable(command_list->context->device, ZEST__MTL_UNAVAILABLE("end_command_buffer")); }
zest_bool zest__mtl_set_next_command_buffer(const zest_command_list command_list, zest_context_queue queue) { return zest__mtl_report_unavailable(command_list->context->device, ZEST__MTL_UNAVAILABLE("set_next_command_buffer")); }
void zest__mtl_acquire_barrier(const zest_command_list command_list, zest_execution_details_t *exe_details) { zest__mtl_report_unavailable(command_list->context->device, ZEST__MTL_UNAVAILABLE("acquire_barrier")); }
void zest__mtl_release_barrier(const zest_command_list command_list, zest_execution_details_t *exe_details) { zest__mtl_report_unavailable(command_list->context->device, ZEST__MTL_UNAVAILABLE("release_barrier")); }
void *zest__mtl_new_execution_backend(zloc_linear_allocator_t *allocator) { zest__mtl_report_unavailable(NULL, ZEST__MTL_UNAVAILABLE("new_execution_backend")); return NULL; }
zest_frame_graph_semaphores zest__mtl_get_frame_graph_semaphores(zest_context context, const char *name) { zest__mtl_report_unavailable(context->device, ZEST__MTL_UNAVAILABLE("get_frame_graph_semaphores")); return NULL; }
zest_bool zest__mtl_submit_frame_graph_batch(zest_frame_graph frame_graph, zest_execution_backend backend, zest_submission_batch_t *batch, zest_map_queue_value *queues) { return zest__mtl_report_unavailable(frame_graph->command_list.context->device, ZEST__MTL_UNAVAILABLE("submit_frame_graph_batch")); }
zest_bool zest__mtl_begin_render_pass(const zest_command_list command_list, zest_execution_details_t *exe_details) { return zest__mtl_report_unavailable(command_list->context->device, ZEST__MTL_UNAVAILABLE("begin_render_pass")); }
void zest__mtl_end_render_pass(const zest_command_list command_list) { zest__mtl_report_unavailable(command_list->context->device, ZEST__MTL_UNAVAILABLE("end_render_pass")); }
void zest__mtl_carry_over_semaphores(zest_frame_graph frame_graph, zest_wave_submission_t *wave_submission, zest_execution_backend backend) { zest__mtl_report_unavailable(frame_graph->command_list.context->device, ZEST__MTL_UNAVAILABLE("carry_over_semaphores")); }
zest_bool zest__mtl_create_execution_timeline_backend(zest_device device, zest_execution_timeline timeline) { return zest__mtl_report_unavailable(device, ZEST__MTL_UNAVAILABLE("create_execution_timeline_backend")); }
void zest__mtl_cleanup_execution_timeline_backend(zest_execution_timeline timeline) { zest__mtl_report_unavailable(timeline->device, ZEST__MTL_UNAVAILABLE("cleanup_execution_timeline_backend")); }
void zest__mtl_add_frame_graph_buffer_barrier(zest_resource_node resource, zest_execution_barriers_t *barriers, zest_bool acquire, zest_access_flags src_access, zest_access_flags dst_access, zest_uint src_family, zest_uint dst_family, zest_pipeline_stage_flags src_stage, zest_pipeline_stage_flags dst_stage) { zest__mtl_report_unavailable(resource->frame_graph->command_list.context->device, ZEST__MTL_UNAVAILABLE("add_frame_graph_buffer_barrier")); }
void zest__mtl_add_frame_graph_image_barrier(zest_resource_node resource, zest_execution_barriers_t *barriers, zest_bool acquire, zest_bool first_use, zest_access_flags src_access, zest_access_flags dst_access, zest_image_layout old_layout, zest_image_layout new_layout, zest_uint src_family, zest_uint dst_family, zest_pipeline_stage_flags src_stage, zest_pipeline_stage_flags dst_stage) { zest__mtl_report_unavailable(resource->frame_graph->command_list.context->device, ZEST__MTL_UNAVAILABLE("add_frame_graph_image_barrier")); }
zest_bool zest__mtl_present_frame(zest_context context, zest_context_queue present_queue) { return zest__mtl_report_unavailable(context->device, ZEST__MTL_UNAVAILABLE("present_frame")); }
zest_bool zest__mtl_dummy_submit_for_present_only(zest_context context) { return zest__mtl_report_unavailable(context->device, ZEST__MTL_UNAVAILABLE("dummy_submit_for_present_only")); }
zest_bool zest__mtl_acquire_swapchain_image(zest_swapchain swapchain) { return zest__mtl_report_unavailable(swapchain->context->device, ZEST__MTL_UNAVAILABLE("acquire_swapchain_image")); }
void *zest__mtl_new_frame_graph_image_backend(zest_device device, zloc_linear_allocator_t *allocator, zest_image image, zest_image imported_image) { zest__mtl_report_unavailable(device, ZEST__MTL_UNAVAILABLE("new_frame_graph_image_backend")); return NULL; }
//Buffer and memory
void *zest__mtl_create_buffer_allocator_backend(zest_device device, zest_context context, zest_size size, zest_buffer_info_t *buffer_info, zest_uint backend_memory_bits) { zest__mtl_report_unavailable(device, ZEST__MTL_UNAVAILABLE("create_buffer_allocator_backend")); return NULL; }
zest_bool zest__mtl_add_buffer_memory_pool(zest_device device, zest_context context, zest_size size, zest_buffer_allocator buffer_allocator, zest_device_memory_pool memory_pool) { return zest__mtl_report_unavailable(device, ZEST__MTL_UNAVAILABLE("add_buffer_memory_pool")); }
zest_bool zest__mtl_create_image_memory_pool(zest_device device, zest_context context, zest_size size_in_bytes, zest_buffer_allocator buffer_allocator, zest_device_memory_pool buffer) { return zest__mtl_report_unavailable(device, ZEST__MTL_UNAVAILABLE("create_image_memory_pool")); }
zest_bool zest__mtl_create_device_memory(zest_device device, zest_size size_in_bytes, zest_buffer_info_t *buffer_info, zest_uint backend_memory_bits, zest_device_memory memory) { return zest__mtl_report_unavailable(device, ZEST__MTL_UNAVAILABLE("create_device_memory")); }
void zest__mtl_get_memory_budget(zest_device device, zest_memory_budget_t *budget) { zest__mtl_report_unavailable(device, ZEST__MTL_UNAVAILABLE("get_memory_budget")); }
zest_bool zest__mtl_map_memory(zest_device_memory_pool memory_allocation, zest_size size, zest_size offset) { return zest__mtl_report_unavailable(memory_allocation->device, ZEST__MTL_UNAVAILABLE("map_memory")); }
void zest__mtl_unmap_memory(zest_device_memory_pool memory_allocation) { zest__mtl_report_unavailable(memory_allocation->device, ZEST__MTL_UNAVAILABLE("unmap_memory")); }
void zest__mtl_flush_used_buffers(zest_context context, zest_uint fif) { zest__mtl_report_unavailable(context->device, ZEST__MTL_UNAVAILABLE("flush_used_buffers")); }
zest_bool zest__mtl_cmd_copy_buffer_one_time(zest_queue queue, zest_buffer src_buffer, zest_buffer dst_buffer, zest_size size, zest_size src_offset, zest_size dst_offset) { return zest__mtl_report_unavailable(queue->device, ZEST__MTL_UNAVAILABLE("cmd_copy_buffer_one_time")); }
//Images
zest_bool zest__mtl_is_image_format_supported(zest_device device, zest_format format, zest_image_flags flags) { return zest__mtl_report_unavailable(device, ZEST__MTL_UNAVAILABLE("is_image_format_supported")); }
zest_bool zest__mtl_create_image(zest_device device, zest_context context, zest_image image, zest_uint layer_count, zest_sample_count_flags num_samples, zest_image_flags flags) { return zest__mtl_report_unavailable(device, ZEST__MTL_UNAVAILABLE("create_image")); }
zest_bool zest__mtl_create_transient_image_unbound(zest_device device, zest_context context, zest_image image, zest_uint layer_count, zest_sample_count_flags num_samples, zest_image_flags flags, zest_transient_memory_info_t *info) { return zest__mtl_report_unavailable(device, ZEST__MTL_UNAVAILABLE("create_transient_image_unbound")); }
zest_bool zest__mtl_bind_image_arena(zest_device device, zest_image image, zest_device_memory_pool backing, zest_size offset) { return zest__mtl_report_unavailable(device, ZEST__MTL_UNAVAILABLE("bind_image_arena")); }
zest_device_memory_pool zest__mtl_create_arena_backing(zest_device device, zest_context context, zest_uint category, zest_size size) { zest__mtl_report_unavailable(device, ZEST__MTL_UNAVAILABLE("create_arena_backing")); return NULL; }
void zest__mtl_free_arena_backing(zest_device device, zest_context context, zest_device_memory_pool backing) { zest__mtl_report_unavailable(device, ZEST__MTL_UNAVAILABLE("free_arena_backing")); }
zest_image_view zest__mtl_create_image_view(zest_device device, zest_image image, zest_image_view_type view_type, zest_uint mip_levels_this_view, zest_uint base_mip, zest_uint base_array_index, zest_uint layer_count, zloc_linear_allocator_t *linear_allocator) { zest__mtl_report_unavailable(device, ZEST__MTL_UNAVAILABLE("create_image_view")); return NULL; }
zest_image_view_array zest__mtl_create_image_views_per_mip(zest_device device, zest_image image, zest_image_view_type view_type, zest_uint base_array_index, zest_uint layer_count, zloc_linear_allocator_t *linear_allocator) { zest__mtl_report_unavailable(device, ZEST__MTL_UNAVAILABLE("create_image_views_per_mip")); return NULL; }
zest_bool zest__mtl_copy_buffer_regions_to_image(zest_queue queue, zest_buffer_image_copy_t *regions, zest_uint regions_count, zest_buffer buffer, zest_size src_offset, zest_image image) { return zest__mtl_report_unavailable(queue->device, ZEST__MTL_UNAVAILABLE("copy_buffer_regions_to_image")); }
zest_bool zest__mtl_transition_image(zest_queue queue, zest_image image, zest_resource_state new_state, zest_uint base_mip_index, zest_uint mip_levels, zest_uint base_array_index, zest_uint layer_count) { return zest__mtl_report_unavailable(queue->device, ZEST__MTL_UNAVAILABLE("transition_image")); }
zest_bool zest__mtl_create_sampler(zest_sampler sampler) { return zest__mtl_report_unavailable((zest_device)sampler->handle.store->origin, ZEST__MTL_UNAVAILABLE("create_sampler")); }
int zest__mtl_get_image_raw_layout(zest_image image) { zest__mtl_report_unavailable((zest_device)image->handle.store->origin, ZEST__MTL_UNAVAILABLE("get_image_raw_layout")); return 0; }
zest_bool zest__mtl_image_layout_is_valid_for_descriptor(zest_image image) { return zest__mtl_report_unavailable((zest_device)image->handle.store->origin, ZEST__MTL_UNAVAILABLE("image_layout_is_valid_for_descriptor")); }
zest_bool zest__mtl_copy_buffer_to_image(zest_queue queue, zest_buffer buffer, zest_size src_offset, zest_image image, zest_uint width, zest_uint height) { return zest__mtl_report_unavailable(queue->device, ZEST__MTL_UNAVAILABLE("copy_buffer_to_image")); }
zest_bool zest__mtl_generate_mipmaps(zest_queue queue, zest_image image) { return zest__mtl_report_unavailable(queue->device, ZEST__MTL_UNAVAILABLE("generate_mipmaps")); }
//Pipelines
zest_bool zest__mtl_build_pipeline(zest_pipeline pipeline, zest_command_list command_list) { return zest__mtl_report_unavailable(command_list->context->device, ZEST__MTL_UNAVAILABLE("build_pipeline")); }
zest_bool zest__mtl_build_pipeline_layout(zest_device device, zest_pipeline_layout layout, zest_pipeline_layout_info_t *info) { return zest__mtl_report_unavailable(device, ZEST__MTL_UNAVAILABLE("build_pipeline_layout")); }
zest_bool zest__mtl_finish_compute(zest_device device, zest_compute compute) { return zest__mtl_report_unavailable(device, ZEST__MTL_UNAVAILABLE("finish_compute")); }
//Semaphores
zest_semaphore_status zest__mtl_wait_for_renderer_semaphore(zest_context context) { zest__mtl_report_unavailable(context->device, ZEST__MTL_UNAVAILABLE("wait_for_renderer_semaphore")); return zest_semaphore_status_error; }
void zest__mtl_wait_for_fif_semaphore(zest_context context, zest_uint fif) { zest__mtl_report_unavailable(context->device, ZEST__MTL_UNAVAILABLE("wait_for_fif_semaphore")); }
void zest__mtl_queue_wait_idle(zest_context context, zest_context_queue queue) { zest__mtl_report_unavailable(context->device, ZEST__MTL_UNAVAILABLE("queue_wait_idle")); }
zest_semaphore_status zest__mtl_wait_for_timeline(zest_execution_timeline timeline, zest_microsecs timeout) { zest__mtl_report_unavailable(timeline->device, ZEST__MTL_UNAVAILABLE("wait_for_timeline")); return zest_semaphore_status_error; }
//Set layouts
zest_bool zest__mtl_create_set_layout(zest_device device, zest_context context, zest_set_layout_builder_t *builder, zest_set_layout layout, zest_bool is_bindless) { return zest__mtl_report_unavailable(device, ZEST__MTL_UNAVAILABLE("create_set_layout")); }
zest_bool zest__mtl_create_set_pool(zest_device device, zest_context context, zest_descriptor_pool pool, zest_set_layout layout, zest_uint max_set_count, zest_bool bindless) { return zest__mtl_report_unavailable(device, ZEST__MTL_UNAVAILABLE("create_set_pool")); }
zest_descriptor_set zest__mtl_create_bindless_set(zest_set_layout layout) { zest__mtl_report_unavailable(layout->device, ZEST__MTL_UNAVAILABLE("create_bindless_set")); return NULL; }
void zest__mtl_update_bindless_image_descriptor(zest_device device, zest_uint binding_number, zest_uint array_index, zest_descriptor_type type, zest_image image, zest_image_view view, zest_sampler sampler, zest_descriptor_set set) { zest__mtl_report_unavailable(device, ZEST__MTL_UNAVAILABLE("update_bindless_image_descriptor")); }
void zest__mtl_update_bindless_storage_buffer_descriptor(zest_device device, zest_uint binding_number, zest_uint array_index, zest_buffer buffer, zest_uint stride, zest_descriptor_set set) { zest__mtl_report_unavailable(device, ZEST__MTL_UNAVAILABLE("update_bindless_storage_buffer_descriptor")); }
void zest__mtl_update_bindless_uniform_buffer_descriptor(zest_device device, zest_uint binding_number, zest_uint array_index, zest_buffer buffer, zest_descriptor_set set) { zest__mtl_report_unavailable(device, ZEST__MTL_UNAVAILABLE("update_bindless_uniform_buffer_descriptor")); }
//Command buffers/queues
void zest__mtl_reset_queue_command_pool(zest_context context, zest_context_queue queue, zest_bool release_resources) { zest__mtl_report_unavailable(context->device, ZEST__MTL_UNAVAILABLE("reset_queue_command_pool")); }
//General Context
void zest__mtl_set_depth_format(zest_device device) { zest__mtl_report_unavailable(device, ZEST__MTL_UNAVAILABLE("set_depth_format")); }
zest_bool zest__mtl_initialise_context_backend(zest_context context) { return zest__mtl_report_unavailable(context->device, ZEST__MTL_UNAVAILABLE("initialise_context_backend")); }
zest_sample_count_flags zest__mtl_get_msaa_sample_count(zest_context context) { zest__mtl_report_unavailable(context->device, ZEST__MTL_UNAVAILABLE("get_msaa_sample_count")); return zest_sample_count_1_bit; }
zest_bool zest__mtl_initialise_swapchain(zest_context context) { return zest__mtl_report_unavailable(context->device, ZEST__MTL_UNAVAILABLE("initialise_swapchain")); }
zest_bool zest__mtl_initialise_context_queue_backend(zest_context context, zest_context_queue context_queue) { return zest__mtl_report_unavailable(context->device, ZEST__MTL_UNAVAILABLE("initialise_context_queue_backend")); }
zest_shader_handle zest__mtl_get_db_overlay_vertex_shader(zest_device device) { zest__mtl_report_unavailable(device, ZEST__MTL_UNAVAILABLE("get_db_overlay_vertex_shader")); return ZEST__ZERO_INIT(zest_shader_handle); }
zest_shader_handle zest__mtl_get_db_overlay_fragment_shader(zest_device device) { zest__mtl_report_unavailable(device, ZEST__MTL_UNAVAILABLE("get_db_overlay_fragment_shader")); return ZEST__ZERO_INIT(zest_shader_handle); }
//Device/OS
void zest__mtl_wait_for_idle_device(zest_device device) { zest__mtl_report_unavailable(device, ZEST__MTL_UNAVAILABLE("wait_for_idle_device")); }
zest_bool zest__mtl_initialise_device(zest_device device) { return zest__mtl_report_unavailable(device, ZEST__MTL_UNAVAILABLE("initialise_device")); }
zest_bool zest__mtl_query_device_capabilities(zest_device device) { return zest__mtl_report_unavailable(device, ZEST__MTL_UNAVAILABLE("query_device_capabilities")); }
zest_bool zest__mtl_create_window_surface(zest_context context) { return zest__mtl_report_unavailable(context->device, ZEST__MTL_UNAVAILABLE("create_window_surface")); }
//Shader Compiling
zest_bool zest__mtl_validate_shader(zest_device device, const char *shader_code, zest_shader_type type, const char *name) { return zest__mtl_report_unavailable(device, ZEST__MTL_UNAVAILABLE("validate_shader")); }
zest_bool zest__mtl_compile_shader(zest_shader shader, const char *code, zest_uint code_length, zest_shader_type type, const char *name, const char *entry_point, zest_shader_options options) { return zest__mtl_report_unavailable((zest_device)shader->handle.store->origin, ZEST__MTL_UNAVAILABLE("compile_shader")); }
//Create backends
void *zest__mtl_new_frame_graph_semaphores_backend(zest_context context) { zest__mtl_report_unavailable(context->device, ZEST__MTL_UNAVAILABLE("new_frame_graph_semaphores_backend")); return NULL; }
void *zest__mtl_new_execution_barriers_backend(zloc_linear_allocator_t *allocator) { zest__mtl_report_unavailable(NULL, ZEST__MTL_UNAVAILABLE("new_execution_barriers_backend")); return NULL; }
void *zest__mtl_new_pipeline_backend(zest_context context) { zest__mtl_report_unavailable(context->device, ZEST__MTL_UNAVAILABLE("new_pipeline_backend")); return NULL; }
void *zest__mtl_new_pipeline_layout_backend(zest_device device) { zest__mtl_report_unavailable(device, ZEST__MTL_UNAVAILABLE("new_pipeline_layout_backend")); return NULL; }
void *zest__mtl_new_memory_pool_backend(zest_buffer_allocator buffer_allocator) { zest__mtl_report_unavailable(buffer_allocator->device, ZEST__MTL_UNAVAILABLE("new_memory_pool_backend")); return NULL; }
void *zest__mtl_new_memory_backend(zest_device device) { zest__mtl_report_unavailable(device, ZEST__MTL_UNAVAILABLE("new_memory_backend")); return NULL; }
void *zest__mtl_new_device_backend(zest_device device) { zest__mtl_report_unavailable(device, ZEST__MTL_UNAVAILABLE("new_device_backend")); return NULL; }
void *zest__mtl_new_context_backend(zest_context context) { zest__mtl_report_unavailable(context->device, ZEST__MTL_UNAVAILABLE("new_context_backend")); return NULL; }
void *zest__mtl_new_frame_graph_context_backend(zest_context context) { zest__mtl_report_unavailable(context->device, ZEST__MTL_UNAVAILABLE("new_frame_graph_context_backend")); return NULL; }
void *zest__mtl_new_swapchain_backend(zest_context context) { zest__mtl_report_unavailable(context->device, ZEST__MTL_UNAVAILABLE("new_swapchain_backend")); return NULL; }
void *zest__mtl_new_image_backend(zest_device device) { zest__mtl_report_unavailable(device, ZEST__MTL_UNAVAILABLE("new_image_backend")); return NULL; }
void *zest__mtl_new_compute_backend(zest_device device) { zest__mtl_report_unavailable(device, ZEST__MTL_UNAVAILABLE("new_compute_backend")); return NULL; }
void *zest__mtl_new_queue_backend(zest_device device, zest_uint family_index) { zest__mtl_report_unavailable(device, ZEST__MTL_UNAVAILABLE("new_queue_backend")); return NULL; }
void *zest__mtl_new_submission_batch_backend(zest_context context) { zest__mtl_report_unavailable(context->device, ZEST__MTL_UNAVAILABLE("new_submission_batch_backend")); return NULL; }
void *zest__mtl_new_set_layout_backend(zloc_allocator *allocator) { zest__mtl_report_unavailable(NULL, ZEST__MTL_UNAVAILABLE("new_set_layout_backend")); return NULL; }
void *zest__mtl_new_descriptor_pool_backend(zloc_allocator *allocator) { zest__mtl_report_unavailable(NULL, ZEST__MTL_UNAVAILABLE("new_descriptor_pool_backend")); return NULL; }
void *zest__mtl_new_sampler_backend(zest_device device) { zest__mtl_report_unavailable(device, ZEST__MTL_UNAVAILABLE("new_sampler_backend")); return NULL; }
void *zest__mtl_new_context_queue_backend(zest_context context) { zest__mtl_report_unavailable(context->device, ZEST__MTL_UNAVAILABLE("new_context_queue_backend")); return NULL; }
//Cleanup backends
void zest__mtl_cleanup_frame_graph_semaphore(zest_context context, zest_frame_graph_semaphores semaphores) { zest__mtl_report_unavailable(context->device, ZEST__MTL_UNAVAILABLE("cleanup_frame_graph_semaphore")); }
void zest__mtl_cleanup_image_backend(zest_image image) { zest__mtl_report_unavailable((zest_device)image->handle.store->origin, ZEST__MTL_UNAVAILABLE("cleanup_image_backend")); }
void zest__mtl_cleanup_image_view_backend(zest_image_view image_view) { zest__mtl_report_unavailable((zest_device)image_view->handle.store->origin, ZEST__MTL_UNAVAILABLE("cleanup_image_view_backend")); }
void zest__mtl_cleanup_image_view_array_backend(zest_image_view_array image_view) { zest__mtl_report_unavailable((zest_device)image_view->handle.store->origin, ZEST__MTL_UNAVAILABLE("cleanup_image_view_array_backend")); }
void zest__mtl_cleanup_buffer_allocator_backend(zest_buffer_allocator buffer_allocator) { zest__mtl_report_unavailable(buffer_allocator->device, ZEST__MTL_UNAVAILABLE("cleanup_buffer_allocator_backend")); }
void zest__mtl_cleanup_memory_pool_backend(zest_device_memory_pool memory_allocation) { zest__mtl_report_unavailable(memory_allocation->device, ZEST__MTL_UNAVAILABLE("cleanup_memory_pool_backend")); }
void zest__mtl_cleanup_memory_backend(zest_device_memory memory) { zest__mtl_report_unavailable(memory->device, ZEST__MTL_UNAVAILABLE("cleanup_memory_backend")); }
void zest__mtl_cleanup_device_backend(zest_device device) { zest__mtl_report_unavailable(device, ZEST__MTL_UNAVAILABLE("cleanup_device_backend")); }
zest_bool zest__mtl_reinit_logical_device(zest_device device) { return zest__mtl_report_unavailable(device, ZEST__MTL_UNAVAILABLE("reinit_logical_device")); }
void zest__mtl_cleanup_context_backend(zest_context context) { zest__mtl_report_unavailable(context->device, ZEST__MTL_UNAVAILABLE("cleanup_context_backend")); }
void zest__mtl_destroy_context_surface(zest_context context) { zest__mtl_report_unavailable(context->device, ZEST__MTL_UNAVAILABLE("destroy_context_surface")); }
void zest__mtl_cleanup_swapchain_backend(zest_swapchain swapchain) { zest__mtl_report_unavailable(swapchain->context->device, ZEST__MTL_UNAVAILABLE("cleanup_swapchain_backend")); }
void zest__mtl_cleanup_compute_backend(zest_compute compute) { zest__mtl_report_unavailable((zest_device)compute->handle.store->origin, ZEST__MTL_UNAVAILABLE("cleanup_compute_backend")); }
void zest__mtl_cleanup_pipeline_backend(zest_pipeline pipeline) { zest__mtl_report_unavailable(pipeline->context->device, ZEST__MTL_UNAVAILABLE("cleanup_pipeline_backend")); }
void zest__mtl_cleanup_pipeline_layout_backend(zest_pipeline_layout layout) { zest__mtl_report_unavailable(layout->device, ZEST__MTL_UNAVAILABLE("cleanup_pipeline_layout_backend")); }
void zest__mtl_cleanup_sampler_backend(zest_sampler sampler) { zest__mtl_report_unavailable((zest_device)sampler->handle.store->origin, ZEST__MTL_UNAVAILABLE("cleanup_sampler_backend")); }
void zest__mtl_cleanup_queue_backend(zest_device device, zest_queue queue) { zest__mtl_report_unavailable(device, ZEST__MTL_UNAVAILABLE("cleanup_queue_backend")); }
void zest__mtl_cleanup_context_queue_backend(zest_context context, zest_context_queue context_queue) { zest__mtl_report_unavailable(context->device, ZEST__MTL_UNAVAILABLE("cleanup_context_queue_backend")); }
void zest__mtl_cleanup_set_layout_backend(zest_set_layout layout) { zest__mtl_report_unavailable(layout->device, ZEST__MTL_UNAVAILABLE("cleanup_set_layout_backend")); }
//GPU Profiling
zest_bool zest__mtl_create_gpu_profiler_backend(zest_context context, zest_gpu_profiler_t *profiler) { return zest__mtl_report_unavailable(context->device, ZEST__MTL_UNAVAILABLE("create_gpu_profiler_backend")); }
void zest__mtl_cleanup_gpu_profiler_backend(zest_gpu_profiler_t *profiler) { zest__mtl_report_unavailable(NULL, ZEST__MTL_UNAVAILABLE("cleanup_gpu_profiler_backend")); }
void zest__mtl_reset_gpu_query_pool(zest_gpu_profiler_t *profiler, zest_uint fif) { zest__mtl_report_unavailable(NULL, ZEST__MTL_UNAVAILABLE("reset_gpu_query_pool")); }
void zest__mtl_write_timestamp(const zest_command_list command_list, zest_gpu_profiler_t *profiler, zest_uint fif, zest_uint query_index, zest_bool is_end) { zest__mtl_report_unavailable(command_list->context->device, ZEST__MTL_UNAVAILABLE("write_timestamp")); }
zest_bool zest__mtl_readback_gpu_timestamps(zest_gpu_profiler_t *profiler, zest_uint fif, zest_uint query_count) { return zest__mtl_report_unavailable(NULL, ZEST__MTL_UNAVAILABLE("readback_gpu_timestamps")); }
//Debugging
void zest__mtl_set_object_name(zest_device device, zest_uint object_type, zest_u64 object_handle, const char *name) { zest__mtl_report_unavailable(device, ZEST__MTL_UNAVAILABLE("set_object_name")); }
void zest__mtl_set_image_name(zest_device device, zest_image image, const char *name) { zest__mtl_report_unavailable(device, ZEST__MTL_UNAVAILABLE("set_image_name")); }
void zest__mtl_set_buffer_pool_name(zest_device device, zest_device_memory_pool pool, const char *name) { zest__mtl_report_unavailable(device, ZEST__MTL_UNAVAILABLE("set_buffer_pool_name")); }
void *zest__mtl_get_final_signal_ptr(zest_submission_batch_t *batch, zest_uint semaphore_index) { zest__mtl_report_unavailable(NULL, ZEST__MTL_UNAVAILABLE("get_final_signal_ptr")); return NULL; }
void *zest__mtl_get_final_wait_ptr(zest_submission_batch_t *batch, zest_uint semaphore_index) { zest__mtl_report_unavailable(NULL, ZEST__MTL_UNAVAILABLE("get_final_wait_ptr")); return NULL; }
void *zest__mtl_get_resource_ptr(zest_resource_node resource) { zest__mtl_report_unavailable(resource->frame_graph->command_list.context->device, ZEST__MTL_UNAVAILABLE("get_resource_ptr")); return NULL; }
void *zest__mtl_get_buffer_ptr(zest_buffer buffer) { zest__mtl_report_unavailable(buffer->memory_pool->device, ZEST__MTL_UNAVAILABLE("get_buffer_ptr")); return NULL; }
void *zest__mtl_get_swapchain_wait_semaphore(zest_swapchain swapchain, zest_uint index) { zest__mtl_report_unavailable(swapchain->context->device, ZEST__MTL_UNAVAILABLE("get_swapchain_wait_semaphore")); return NULL; }
void *zest__mtl_get_swapchain_signal_semaphore(zest_swapchain swapchain, zest_uint index) { zest__mtl_report_unavailable(swapchain->context->device, ZEST__MTL_UNAVAILABLE("get_swapchain_signal_semaphore")); return NULL; }
void *zest__mtl_get_swapchain_identifier(zest_swapchain swapchain) { zest__mtl_report_unavailable(swapchain->context->device, ZEST__MTL_UNAVAILABLE("get_swapchain_identifier")); return NULL; }
void zest__mtl_create_test_render_pass(zest_device device, const zest_command_list_t *command_list) {
}
//Command recording
void zest__mtl_blit_image_mip(const zest_command_list command_list, zest_resource_node src, zest_resource_node dst, zest_uint mip_to_blit, zest_supported_shader_stages read_by_stages) { zest__mtl_report_unavailable(command_list->context->device, ZEST__MTL_UNAVAILABLE("blit_image_mip")); }
void zest__mtl_copy_image_mip(const zest_command_list command_list, zest_resource_node src, zest_resource_node dst, zest_uint mip_to_blit, zest_supported_shader_stages read_by_stages) { zest__mtl_report_unavailable(command_list->context->device, ZEST__MTL_UNAVAILABLE("copy_image_mip")); }
void zest__mtl_insert_compute_image_barrier(const zest_command_list command_list, zest_resource_node resource, zest_uint base_mip) { zest__mtl_report_unavailable(command_list->context->device, ZEST__MTL_UNAVAILABLE("insert_compute_image_barrier")); }
void zest__mtl_set_screensized_viewport(const zest_command_list command_list, float min_depth, float max_depth) { zest__mtl_report_unavailable(command_list->context->device, ZEST__MTL_UNAVAILABLE("set_screensized_viewport")); }
void zest__mtl_scissor(const zest_command_list command_list, zest_scissor_rect_t *scissor) { zest__mtl_report_unavailable(command_list->context->device, ZEST__MTL_UNAVAILABLE("scissor")); }
void zest__mtl_viewport(const zest_command_list command_list, zest_viewport_t *viewport) { zest__mtl_report_unavailable(command_list->context->device, ZEST__MTL_UNAVAILABLE("viewport")); }
void zest__mtl_clip(const zest_command_list command_list, float x, float y, float width, float height, float minDepth, float maxDepth) { zest__mtl_report_unavailable(command_list->context->device, ZEST__MTL_UNAVAILABLE("clip")); }
void zest__mtl_bind_descriptor_sets(const zest_command_list command_list, zest_pipeline_bind_point bind_point, zest_pipeline_layout layout, zest_descriptor_set *descriptor_sets, zest_uint set_count, zest_uint first_set) { zest__mtl_report_unavailable(command_list->context->device, ZEST__MTL_UNAVAILABLE("bind_descriptor_sets")); }
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
zest_queue zest__mtl_imm_begin_command_buffer(zest_device device, zest_device_queue_type target_queue) { zest__mtl_report_unavailable(device, ZEST__MTL_UNAVAILABLE("imm_begin_command_buffer")); return NULL; }
zest_bool zest__mtl_imm_end_command_buffer(zest_queue queue) { return zest__mtl_report_unavailable(queue->device, ZEST__MTL_UNAVAILABLE("imm_end_command_buffer")); }
void zest__mtl_imm_fill_buffer(zest_queue queue, zest_buffer buffer, zest_uint value) { zest__mtl_report_unavailable(queue->device, ZEST__MTL_UNAVAILABLE("imm_fill_buffer")); }
void zest__mtl_imm_update_buffer(zest_queue queue, zest_buffer buffer, void *data, zest_size intended_size) { zest__mtl_report_unavailable(queue->device, ZEST__MTL_UNAVAILABLE("imm_update_buffer")); }
zest_bool zest__mtl_imm_clear_color_image(zest_queue queue, zest_image image, zest_clear_value_t clear_value) { return zest__mtl_report_unavailable(queue->device, ZEST__MTL_UNAVAILABLE("imm_clear_color_image")); }
zest_bool zest__mtl_imm_clear_depth_stencil_image(zest_queue queue, zest_image image, float depth, zest_uint stencil) { return zest__mtl_report_unavailable(queue->device, ZEST__MTL_UNAVAILABLE("imm_clear_depth_stencil_image")); }
zest_bool zest__mtl_imm_blit_image(zest_queue queue, zest_image src_image, zest_image dst_image, int src_x, int src_y, int src_width, int src_height, int dst_x, int dst_y, int dst_width, int dst_height, zest_filter_type filter) { return zest__mtl_report_unavailable(queue->device, ZEST__MTL_UNAVAILABLE("imm_blit_image")); }
zest_bool zest__mtl_imm_resolve_image(zest_queue queue, zest_image src_image, zest_image dst_image) { return zest__mtl_report_unavailable(queue->device, ZEST__MTL_UNAVAILABLE("imm_resolve_image")); }
void zest__mtl_imm_send_push_constants(zest_queue queue, void *data, zest_uint size) { zest__mtl_report_unavailable(queue->device, ZEST__MTL_UNAVAILABLE("imm_send_push_constants")); }
void zest__mtl_imm_bind_compute_pipeline(zest_queue queue, zest_compute compute) { zest__mtl_report_unavailable(queue->device, ZEST__MTL_UNAVAILABLE("imm_bind_compute_pipeline")); }
zest_bool zest__mtl_imm_dispatch_compute(zest_queue queue, zest_uint group_count_x, zest_uint group_count_y, zest_uint group_count_z) { return zest__mtl_report_unavailable(queue->device, ZEST__MTL_UNAVAILABLE("imm_dispatch_compute")); }
zest_bool zest__mtl_copy_bitmap_to_image(zest_device device, void *bitmap, zest_size image_size, zest_image dst_image, zest_uint width, zest_uint height) { return zest__mtl_report_unavailable(device, ZEST__MTL_UNAVAILABLE("copy_bitmap_to_image")); }
zest_bool zest__mtl_copy_image_to_bitmap(zest_device device, zest_image src_image, void *pixels) { return zest__mtl_report_unavailable(device, ZEST__MTL_UNAVAILABLE("copy_image_to_bitmap")); }
// -- End Unavailable_stubs

#ifdef __cplusplus
}
#endif

#endif // ! ZEST_METAL_H
