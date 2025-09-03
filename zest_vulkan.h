/*
Zest Vulkan Implementation

    -- [Backend_structs]
    -- [Cleanup_functions]
    -- [Backend_setup_functions]
    -- [Backend_cleanup_functions]
    -- [General_helpers]
    -- [Frame_graph_context_functions]
*/

// -- Backend_structs
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

typedef struct zest_renderer_backend_t {
    VkPipelineCache pipeline_cache;
    VkBuffer *used_buffers_ready_for_freeing;
    VkCommandBuffer utility_command_buffer[ZEST_MAX_FIF];
    VkResult last_result;
    VkFence fif_fence[ZEST_MAX_FIF][ZEST_QUEUE_COUNT];
    VkFence intraframe_fence[ZEST_QUEUE_COUNT];
} zest_renderer_backend_t;

typedef struct zest_frame_graph_context_backend_t {
    VkCommandBuffer command_buffer;
} zest_frame_graph_context_backend_t;

typedef struct zest_buffer_backend_t {
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
    zest_descriptor_set *sets[ZEST_MAX_FIF];
    VkDescriptorSet *binding_sets;
} zest_shader_resources_backend_t;

typedef struct zest_descriptor_pool_backend_t {
    VkDescriptorPool vk_descriptor_pool;
    VkDescriptorPoolSize *vk_pool_sizes;
} zest_descriptor_pool_backend_t;

typedef struct zest_descriptor_set_backend_t {
    VkDescriptorSet vk_descriptor_set;
} zest_descriptor_set_backend_t;

typedef struct zest_descriptor_indices_t {
    zest_uint *free_indices;
    zest_uint next_new_index;
    zest_uint capacity;
    VkDescriptorType descriptor_type;
} zest_descriptor_indices_t;

typedef struct zest_set_layout_backend_t {
    VkDescriptorSetLayoutBinding *layout_bindings;
    VkDescriptorSetLayout vk_layout;
    VkDescriptorSetLayoutCreateFlags create_flags;
    zest_descriptor_indices_t *descriptor_indexes;
} zest_set_layout_backend_t;

typedef struct zest_pipeline_backend_t {
    VkPipeline pipeline;                                                         //The vulkan handle for the pipeline
    VkPipelineLayout pipeline_layout;                                            //The vulkan handle for the pipeline layout
    VkRenderPass render_pass;
} zest_pipeline_backend_t;

typedef struct zest_compute_backend_t {
    VkPipelineLayout pipeline_layout;                         // Layout of the compute pipeline
    VkPipeline pipeline;                                      // Compute pipeline
    VkPushConstantRange push_constant_range;
} zest_compute_backend_t;

typedef struct zest_image_backend_t {
	VkImage vk_image;
	VkImageView vk_view;             // Default view of the entire image
    VkImageView *vk_mip_views;
	VkImageLayout vk_current_layout; // The actual, current layout of the image on the GPU
    VkImageViewType vk_view_type;
	VkFormat vk_format;
	VkExtent3D vk_extent;
} zest_image_backend_t;

// -- Cleanup_functions
// -- End cleanup functions

// -- Backend_setup_functions
void *zest__vk_new_frame_graph_context_backend(void) {
    zest_frame_graph_context_backend_t *backend_context = zloc_LinearAllocation(
        ZestRenderer->frame_graph_allocator[ZEST_FIF],
        sizeof(zest_frame_graph_context_backend_t)
    );
    *backend_context = (zest_frame_graph_context_backend_t){ 0 };
    return backend_context;
}

void *zest__vk_new_buffer_backend(void) {
    zest_buffer_backend buffer_backend = ZEST__NEW(zest_buffer_backend);
    *buffer_backend = (zest_buffer_backend_t){ 0 };
    return buffer_backend;
}

void *zest__vk_new_uniform_buffer_backend(void) {
    zest_uniform_buffer_backend uniform_buffer_backend = ZEST__NEW(zest_uniform_buffer_backend);
    *uniform_buffer_backend = (zest_uniform_buffer_backend_t){ 0 };
    return uniform_buffer_backend;
}

void zest__vk_set_uniform_buffer_backend(zest_uniform_buffer uniform_buffer) {
    zest_ForEachFrameInFlight(fif) {
        uniform_buffer->backend->descriptor_info[fif].buffer = *zest_GetBufferDeviceBuffer(uniform_buffer->buffer[fif]);
        uniform_buffer->backend->descriptor_info[fif].offset = 0;
        uniform_buffer->backend->descriptor_info[fif].range = uniform_buffer->buffer[fif]->size;
    }
}

void *zest__vk_new_image_backend(void) {
    zest_image_backend image_backend = ZEST__NEW(zest_image_backend);
    *image_backend = (zest_image_backend_t){ 0 };
    return image_backend;
}

void *zest__vk_new_frame_graph_image_backend(zloc_linear_allocator_t *allocator) {
    zest_image_backend image_backend = zloc_LinearAllocation(allocator, sizeof(zest_image_backend_t));
    *image_backend = (zest_image_backend_t){ 0 };
    return image_backend;
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
    zest_descriptor_pool_backend pool = ZEST__NEW(zest_descriptor_pool_backend);
    *pool = (zest_descriptor_pool_backend_t){ 0 };
    return pool;
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

    ZEST_SET_MEMORY_CONTEXT(zest_vk_renderer, zest_vk_pipeline_layout);
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
        ZEST_CLEANUP_ON_FAIL(zest__create_shader_module(shader->spv, &shader_module));

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
        ZEST_SET_MEMORY_CONTEXT(zest_vk_renderer, zest_vk_compute_pipeline);
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
// -- End Backend setup functions

// -- Backend_cleanup_functions
void zest__vk_cleanup_compute(zest_compute compute) {
	if(compute->backend->pipeline) vkDestroyPipeline(ZestDevice->backend->logical_device, compute->backend->pipeline, &ZestDevice->backend->allocation_callbacks);
    if(compute->backend->pipeline_layout) vkDestroyPipelineLayout(ZestDevice->backend->logical_device, compute->backend->pipeline_layout, &ZestDevice->backend->allocation_callbacks);
    ZEST__FREE(compute->backend);
}

void zest__vk_cleanup_buffer(zest_buffer buffer) {
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

void zest__vk_cleanup_set_layout(zest_set_layout layout) {
    if (layout->backend) {
        zest_vec_free(layout->backend->layout_bindings);
        vkDestroyDescriptorSetLayout(ZestDevice->backend->logical_device, layout->backend->vk_layout, &ZestDevice->backend->allocation_callbacks);
        zest_vec_foreach(i, layout->backend->descriptor_indexes) {
            zest_vec_free(layout->backend->descriptor_indexes[i].free_indices);
        }
        zest_vec_free(layout->backend->descriptor_indexes);
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
	if(image->backend->vk_view) vkDestroyImageView(ZestDevice->backend->logical_device, image->backend->vk_view, &ZestDevice->backend->allocation_callbacks);
	if(image->backend->vk_image) vkDestroyImage(ZestDevice->backend->logical_device, image->backend->vk_image, &ZestDevice->backend->allocation_callbacks);
	image->backend->vk_view = VK_NULL_HANDLE;
	image->backend->vk_image = VK_NULL_HANDLE;
	zest_vec_foreach(i, image->backend->vk_mip_views) {
		VkImageView image_view = image->backend->vk_mip_views[i];
		if (image_view) {
			vkDestroyImageView(ZestDevice->backend->logical_device, image_view, &ZestDevice->backend->allocation_callbacks);
		}
	}
	zest_vec_free(image->backend->vk_mip_views);
    ZEST__FREE(image->backend);
    image->backend = 0;
}

void zest__vk_cleanup_texture_backend(zest_texture texture) {
    ZEST_PRINT_FUNCTION;
    zest__vk_cleanup_image_backend(&texture->image);
    zest_set_layout debug_layout = (zest_set_layout)zest__get_store_resource(&ZestRenderer->set_layouts, ZestRenderer->texture_debug_layout.value);
    if (debug_layout && texture->debug_set) {
        zest__vk_cleanup_descriptor_backend(debug_layout, texture->debug_set);
    }
    if(ZEST_VALID_HANDLE(texture->debug_set)) ZEST__FREE(texture->debug_set);
}

void zest__vk_cleanup_descriptor_backend(zest_set_layout layout, zest_descriptor_set set) {
    vkFreeDescriptorSets(ZestDevice->backend->logical_device, layout->pool->backend->vk_descriptor_pool, 1, &set->backend->vk_descriptor_set);
	ZEST__FREE(set->backend);
    set->backend = 0;
}

void zest__vk_cleanup_shader_resources_backend(zest_shader_resources shader_resource) {
	zest_ForEachFrameInFlight(fif) {
		zest_vec_free(shader_resource->backend->sets[fif]);
	}
	zest_vec_free(shader_resource->backend->binding_sets);
    ZEST__FREE(shader_resource->backend);
}

// -- End Backend cleanup functions

// -- General_helpers
zest_bool zest__has_blit_support(VkFormat src_format) {
    ZEST_PRINT_FUNCTION;
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
// -- End General helpers

// -- Frame_graph_context_functions
void zest_cmd_DrawIndexed(const zest_frame_graph_context context, zest_uint index_count, zest_uint instance_count, zest_uint first_index, int32_t vertex_offset, zest_uint first_instance) {
    ZEST_ASSERT_HANDLE(context);        //Not valid context, this command must be called within a frame graph execution callback
    vkCmdDrawIndexed(context->backend->command_buffer, index_count, instance_count, first_index, vertex_offset, first_instance);
}

void zest_cmd_CopyBuffer(const zest_frame_graph_context context, zest_buffer src_buffer, zest_buffer dst_buffer, VkDeviceSize size) {
    ZEST_PRINT_FUNCTION;
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
    zest_vec_foreach(set_index, shader_resources->backend->sets[ZEST_FIF]) {
        zest_descriptor_set set = shader_resources->backend->sets[ZEST_FIF][set_index];
        ZEST_ASSERT_HANDLE(set);     //Not a valid desriptor set in the shaer resource. Did you set all frames in flight?
		zest_vec_push(shader_resources->backend->binding_sets, set->backend->vk_descriptor_set);
	}
    vkCmdBindPipeline(context->backend->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->backend->pipeline);
    vkCmdBindDescriptorSets(context->backend->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->backend->pipeline_layout, 0, zest_vec_size(shader_resources->backend->binding_sets), shader_resources->backend->binding_sets, 0, 0);
    zest_vec_clear(shader_resources->backend->binding_sets);
}

void zest_cmd_BindPipeline(const zest_frame_graph_context context, zest_pipeline pipeline, VkDescriptorSet *descriptor_set, zest_uint set_count) {
    ZEST_ASSERT_HANDLE(context);        //Not valid context, this command must be called within a frame graph execution callback
    ZEST_ASSERT(set_count && descriptor_set);    //No descriptor sets. Must bind the pipeline with a valid desriptor set
    vkCmdBindPipeline(context->backend->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->backend->pipeline);
    vkCmdBindDescriptorSets(context->backend->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->backend->pipeline_layout, 0, set_count, descriptor_set, 0, 0);
}

void zest_cmd_BindComputePipeline(const zest_frame_graph_context context, zest_compute_handle compute_handle, VkDescriptorSet *descriptor_set, zest_uint set_count) {
    ZEST_ASSERT_HANDLE(context);        //Not valid context, this command must be called within a frame graph execution callback
    zest_compute compute = (zest_compute)zest__get_store_resource_checked(&ZestRenderer->compute_pipelines, compute_handle.value);
    ZEST_ASSERT(set_count && descriptor_set);   //No descriptor sets. Must bind the pipeline with a valid desriptor set
    vkCmdBindPipeline(context->backend->command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, compute->backend->pipeline);
    vkCmdBindDescriptorSets(context->backend->command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, compute->backend->pipeline_layout, 0, set_count, descriptor_set, 0, 0);
}

void zest_cmd_BindVertexBuffer(const zest_frame_graph_context context, zest_buffer buffer) {
    ZEST_ASSERT_HANDLE(context);        //Not valid context, this command must be called within a frame graph execution callback
    VkDeviceSize offsets[] = { buffer->buffer_offset };
    vkCmdBindVertexBuffers(context->backend->command_buffer, 0, 1, zest_GetBufferDeviceBuffer(buffer), offsets);
}

void zest_cmd_BindIndexBuffer(const zest_frame_graph_context context, zest_buffer buffer) {
    ZEST_ASSERT_HANDLE(context);        //Not valid context, this command must be called within a frame graph execution callback
    vkCmdBindIndexBuffer(context->backend->command_buffer, *zest_GetBufferDeviceBuffer(buffer), buffer->buffer_offset, VK_INDEX_TYPE_UINT32);
}

void zest_cmd_SendPushConstants(const zest_frame_graph_context context, zest_pipeline pipeline, void *data) {
    ZEST_ASSERT_HANDLE(context);        //Not valid context, this command must be called within a frame graph execution callback
    vkCmdPushConstants(context->backend->command_buffer, pipeline->backend->pipeline_layout, pipeline->pipeline_template->pushConstantRange.stageFlags, pipeline->pipeline_template->pushConstantRange.offset, pipeline->pipeline_template->pushConstantRange.size, data);
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
    ZEST_ASSERT(src->image_desc.width == dst->image_desc.width);
    ZEST_ASSERT(src->image_desc.height == dst->image_desc.height);
    ZEST_ASSERT(src->image_desc.mip_levels == dst->image_desc.mip_levels);

    VkImage src_image = src->image.backend->vk_image;
    VkImage dst_image = dst->image.backend->vk_image;

    zest_uint mip_width = ZEST__MAX(1u, src->image_desc.width >> mip_to_blit);
    zest_uint mip_height = ZEST__MAX(1u, src->image_desc.height >> mip_to_blit);

    VkImageLayout src_current_layout = zest__to_vk_image_layout(src->journey[src->current_state_index].usage.image_layout);
    VkImageLayout dst_current_layout = zest__to_vk_image_layout(dst->journey[dst->current_state_index].usage.image_layout);

    //Blit the smallest mip level from the downsampled render target first
    VkImageMemoryBarrier blit_src_barrier = zest__create_image_memory_barrier(src_image,
        0,
        VK_ACCESS_TRANSFER_READ_BIT,
        src_current_layout,
        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        src->image_desc.aspect_flags,
        mip_to_blit, 1);
    zest__place_image_barrier(context->backend->command_buffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, &blit_src_barrier);

    VkImageMemoryBarrier blit_dst_barrier = zest__create_image_memory_barrier(dst_image,
        0,
        VK_ACCESS_TRANSFER_WRITE_BIT,
        dst_current_layout,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        dst->image_desc.aspect_flags,
        mip_to_blit, 1);
    zest__place_image_barrier(context->backend->command_buffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, &blit_dst_barrier);

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

    blit_src_barrier = zest__create_image_memory_barrier(src_image,
        VK_ACCESS_TRANSFER_READ_BIT,
        VK_ACCESS_SHADER_READ_BIT,
        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        src_current_layout,
        src->image_desc.aspect_flags,
        mip_to_blit, 1);
    zest__place_image_barrier(context->backend->command_buffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, &blit_src_barrier);

    blit_dst_barrier = zest__create_image_memory_barrier(dst_image,
        VK_ACCESS_TRANSFER_WRITE_BIT,
        VK_ACCESS_SHADER_READ_BIT,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        dst_current_layout,
        dst->image_desc.aspect_flags,
        mip_to_blit, 1);
    zest__place_image_barrier(context->backend->command_buffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, &blit_dst_barrier);
}

void zest_cmd_CopyImageMip(const zest_frame_graph_context context, zest_resource_node src, zest_resource_node dst, zest_uint mip_to_copy, zest_supported_pipeline_stages pipeline_stage) {
    ZEST_ASSERT_HANDLE(context);        //Not valid context, this command must be called within a frame graph execution callback
    ZEST_ASSERT_HANDLE(src);
    ZEST_ASSERT_HANDLE(dst);
    ZEST_ASSERT(src->type == zest_resource_type_image && dst->type == zest_resource_type_image);
    //Source and destination images must be the same width/height and have the same number of mip levels
    ZEST_ASSERT(src->image.backend->vk_image);
    ZEST_ASSERT(dst->image.backend->vk_image);
    ZEST_ASSERT(src->image_desc.width == dst->image_desc.width);
    ZEST_ASSERT(src->image_desc.height == dst->image_desc.height);
    ZEST_ASSERT(src->image_desc.mip_levels == dst->image_desc.mip_levels);

    //You must ensure that when creating the images that you use usage hints to indicate that you intend to copy
    //the images. When creating a transient image resource you can set the usage hints in the zest_image_resource_info_t
    //type that you pass into zest_CreateTransientImageResource. Trying to copy images that don't have the appropriate
    //usage flags set will result in validation errors.
    ZEST_ASSERT(src->image_desc.usage & VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
    ZEST_ASSERT(dst->image_desc.usage & VK_IMAGE_USAGE_TRANSFER_DST_BIT);

    VkImage src_image = src->image.backend->vk_image;
    VkImage dst_image = dst->image.backend->vk_image;

    zest_uint mip_width = ZEST__MAX(1u, src->image_desc.width >> mip_to_copy);
    zest_uint mip_height = ZEST__MAX(1u, src->image_desc.height >> mip_to_copy);

    VkImageLayout src_current_layout = zest__to_vk_image_layout(src->journey[src->current_state_index].usage.image_layout);
    VkImageLayout dst_current_layout = zest__to_vk_image_layout(dst->journey[dst->current_state_index].usage.image_layout);

    //Blit the smallest mip level from the downsampled render target first
    VkImageMemoryBarrier blit_src_barrier = zest__create_image_memory_barrier(src_image,
        0,
        VK_ACCESS_TRANSFER_READ_BIT,
        src_current_layout,
        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        src->image_desc.aspect_flags,
        mip_to_copy, 1);
    zest__place_image_barrier(context->backend->command_buffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, &blit_src_barrier);

    VkImageMemoryBarrier blit_dst_barrier = zest__create_image_memory_barrier(dst_image,
        0,
        VK_ACCESS_TRANSFER_WRITE_BIT,
        dst_current_layout,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        dst->image_desc.aspect_flags,
        mip_to_copy, 1);
    zest__place_image_barrier(context->backend->command_buffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, &blit_dst_barrier);

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

    blit_src_barrier = zest__create_image_memory_barrier(src_image,
        VK_ACCESS_TRANSFER_READ_BIT,
        VK_ACCESS_SHADER_READ_BIT,
        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        src_current_layout,
        src->image_desc.aspect_flags,
        mip_to_copy, 1);
    zest__place_image_barrier(context->backend->command_buffer, VK_PIPELINE_STAGE_TRANSFER_BIT, pipeline_stage, &blit_src_barrier);

    blit_dst_barrier = zest__create_image_memory_barrier(dst_image,
        VK_ACCESS_TRANSFER_WRITE_BIT,
        VK_ACCESS_SHADER_READ_BIT,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        dst_current_layout,
        dst->image_desc.aspect_flags,
        mip_to_copy, 1);
    zest__place_image_barrier(context->backend->command_buffer, VK_PIPELINE_STAGE_TRANSFER_BIT, pipeline_stage, &blit_dst_barrier);
}

void zest_cmd_Clip(const zest_frame_graph_context context, float x, float y, float width, float height, float minDepth, float maxDepth) {
    ZEST_ASSERT_HANDLE(context);        //Not valid context, this command must be called within a frame graph execution callback
	VkViewport view = zest_CreateViewport(x, y, width, height, minDepth, maxDepth);
	VkRect2D scissor = zest_CreateRect2D((zest_uint)width, (zest_uint)height, (zest_uint)x, (zest_uint)y);
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

zest_bool zest_cmd_TextureClear(zest_texture_handle handle, const zest_frame_graph_context context) {
    zest_texture texture = (zest_texture)zest__get_store_resource_checked(&ZestRenderer->textures, handle.value);
    VkCommandBuffer command_buffer = context ? context->backend->command_buffer : 0;
    if (!context) {
        ZEST_RETURN_FALSE_ON_FAIL(zest__begin_single_time_commands(&command_buffer));
    }

    VkImageSubresourceRange image_sub_resource_range;
    image_sub_resource_range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    image_sub_resource_range.baseMipLevel = 0;
    image_sub_resource_range.levelCount = 1;
    image_sub_resource_range.baseArrayLayer = 0;
    image_sub_resource_range.layerCount = 1;

    VkClearColorValue ClearColorValue = { 0.0, 0.0, 0.0, 0.0 };

    zest__insert_image_memory_barrier(
        command_buffer,
        texture->image.backend->vk_image,
        0,
        VK_ACCESS_TRANSFER_WRITE_BIT,
        texture->image.backend->vk_current_layout,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        image_sub_resource_range);

    vkCmdClearColorImage(command_buffer, texture->image.backend->vk_image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &ClearColorValue, 1, &image_sub_resource_range);

    /*
    zest__insert_image_memory_barrier(
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

    if (!context) {
        ZEST_RETURN_FALSE_ON_FAIL(zest__end_single_time_commands(command_buffer));
    }
    return ZEST_TRUE;
}

zest_bool zest_cmd_CopyTextureToTexture(zest_texture_handle src_handle, zest_texture_handle dst_handle, int src_x, int src_y, int dst_x, int dst_y, int width, int height) {
    zest_texture src_image = (zest_texture)zest__get_store_resource_checked(&ZestRenderer->textures, src_handle.value);
    ZEST_ASSERT_HANDLE(src_image);    //Not a valid texture resource
    zest_texture dst_image = (zest_texture)zest__get_store_resource_checked(&ZestRenderer->textures, dst_handle.value);
    ZEST_ASSERT_HANDLE(dst_image);	    //Not a valid handle!
    VkCommandBuffer copy_command = 0; 
    ZEST_RETURN_FALSE_ON_FAIL(zest__begin_single_time_commands(&copy_command));

    VkImageLayout target_layout = dst_image->descriptor_image_info.imageLayout;

    VkImageSubresourceRange image_range = { 0 };
    image_range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    image_range.baseMipLevel = 0;
    image_range.levelCount = 1;
    image_range.baseArrayLayer = 0;
    image_range.layerCount = 1;

    // Transition destination image to transfer destination layout
    zest__insert_image_memory_barrier(
        copy_command,
        dst_image->image.backend->vk_image,
        0,
        VK_ACCESS_TRANSFER_WRITE_BIT,
        dst_image->descriptor_image_info.imageLayout,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        image_range);

    // Transition swapchain image from present to transfer source layout
    zest__insert_image_memory_barrier(
        copy_command,
        src_image->image.backend->vk_image,
        VK_ACCESS_MEMORY_READ_BIT,
        VK_ACCESS_TRANSFER_READ_BIT,
        src_image->descriptor_image_info.imageLayout,
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
        src_image->image.backend->vk_image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        dst_image->image.backend->vk_image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        1,
        &image_blit_region,
        VK_FILTER_NEAREST);

    // Transition destination image to general layout, which is the required layout for mapping the image memory later on
    zest__insert_image_memory_barrier(
        copy_command,
        dst_image->image.backend->vk_image,
        VK_ACCESS_TRANSFER_WRITE_BIT,
        VK_ACCESS_MEMORY_READ_BIT,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        target_layout,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        image_range);

    zest__insert_image_memory_barrier(
        copy_command,
        src_image->image.backend->vk_image,
        VK_ACCESS_TRANSFER_READ_BIT,
        VK_ACCESS_MEMORY_READ_BIT,
        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        src_image->descriptor_image_info.imageLayout,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        image_range);

    ZEST_RETURN_FALSE_ON_FAIL(zest__end_single_time_commands(copy_command));
    return ZEST_FALSE;
}

zest_bool zest_cmd_CopyTextureToBitmap(zest_texture_handle src_handle, zest_bitmap image, int src_x, int src_y, int dst_x, int dst_y, int width, int height, zest_bool swap_channel) {
    zest_texture src_image = (zest_texture)zest__get_store_resource_checked(&ZestRenderer->textures, src_handle.value);
    ZEST_ASSERT_HANDLE(src_image);    //Not a valid texture resource
    VkCommandBuffer copy_command = 0;
    ZEST_RETURN_FALSE_ON_FAIL(zest__begin_single_time_commands(&copy_command));
    VkImage temp_image;
    VkDeviceMemory temp_memory;
    ZEST_RETURN_FALSE_ON_FAIL(zest__create_temporary_image(width, height, 1, VK_SAMPLE_COUNT_1_BIT, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_TILING_LINEAR, VK_IMAGE_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &temp_image, &temp_memory));

    zest_bool supports_blit = zest__has_blit_support(src_image->image.backend->vk_format);

    VkImageSubresourceRange image_range = { 0 };
    image_range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    image_range.baseMipLevel = 0;
    image_range.levelCount = 1;
    image_range.baseArrayLayer = 0;
    image_range.layerCount = 1;

    // Transition temporary image to transfer destination layout
    zest__insert_image_memory_barrier(
        copy_command,
        temp_image,
        0,
        VK_ACCESS_TRANSFER_WRITE_BIT,
        VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        image_range);

    // Transition source image from present to transfer source layout
    zest__insert_image_memory_barrier(
        copy_command,
        src_image->image.backend->vk_image,
        VK_ACCESS_MEMORY_READ_BIT,
        VK_ACCESS_TRANSFER_READ_BIT,
        src_image->image.backend->vk_current_layout,
        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        image_range);

    if (supports_blit) {
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
            src_image->image.backend->vk_image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            temp_image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            1,
            &image_blit_region,
            VK_FILTER_NEAREST);
    }
    else
    {
        // Otherwise use image copy (requires us to manually flip components)
        VkImageCopy image_copy_region = { 0 };
        image_copy_region.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        image_copy_region.srcSubresource.layerCount = 1;
        image_copy_region.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        image_copy_region.dstSubresource.layerCount = 1;
        image_copy_region.srcOffset.x = src_x;
        image_copy_region.srcOffset.y = src_y;
        image_copy_region.dstOffset.x = dst_x;
        image_copy_region.dstOffset.y = dst_y;
        image_copy_region.extent.width = width;
        image_copy_region.extent.height = height;
        image_copy_region.extent.depth = 1;

        // Issue the copy command
        vkCmdCopyImage(
            copy_command,
            src_image->image.backend->vk_image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            temp_image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            1,
            &image_copy_region);
    }

    // Transition destination image to general layout
    zest__insert_image_memory_barrier(
        copy_command,
        temp_image,
        VK_ACCESS_TRANSFER_WRITE_BIT,
        VK_ACCESS_MEMORY_READ_BIT,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        VK_IMAGE_LAYOUT_GENERAL,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        image_range);

    // Transition back the source image
    zest__insert_image_memory_barrier(
        copy_command,
        src_image->image.backend->vk_image,
        VK_ACCESS_TRANSFER_READ_BIT,
        VK_ACCESS_MEMORY_READ_BIT,
        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        src_image->image.backend->vk_current_layout,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        image_range);

    ZEST_RETURN_FALSE_ON_FAIL(zest__end_single_time_commands(copy_command));

    // Get layout of the image (including row pitch)
    VkImageSubresource sub_resource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0 };
    VkSubresourceLayout sub_resource_layout;
    vkGetImageSubresourceLayout(ZestDevice->backend->logical_device, temp_image, &sub_resource, &sub_resource_layout);

    // If source is BGR (destination is always RGB) and we can't use blit (which does automatic conversion), we'll have to manually swizzle color components
    zest_bool color_swizzle = 0;
    // Check if source is BGR
    // Note: Not complete, only contains most common and basic BGR surface formats for demonstation purposes
    if (!supports_blit)
    {
        if (src_image->image.backend->vk_format == VK_FORMAT_B8G8R8A8_SRGB || src_image->image.backend->vk_format == VK_FORMAT_B8G8R8A8_UNORM || src_image->image.backend->vk_format == VK_FORMAT_B8G8R8A8_SNORM) {
            color_swizzle = 1;
        }
    }

    // Map image memory so we can start copying from it
    void* data = 0;
    vkMapMemory(ZestDevice->backend->logical_device, temp_memory, 0, VK_WHOLE_SIZE, 0, &data);
    zloc_SafeCopy(image->data, data, image->meta.size);

    if (color_swizzle) {
        zest_ConvertBGRAToRGBA(image);
    }

    vkUnmapMemory(ZestDevice->backend->logical_device, temp_memory);
    vkFreeMemory(ZestDevice->backend->logical_device, temp_memory, &ZestDevice->backend->allocation_callbacks);
    vkDestroyImage(ZestDevice->backend->logical_device, temp_image, &ZestDevice->backend->allocation_callbacks);
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
            vkCmdSetViewport(command_buffer, 0, 1, &current->viewport);
            vkCmdSetScissor(command_buffer, 0, 1, &current->scissor);
            has_instruction_view_port = true;
            continue;
        } else if(!has_instruction_view_port) {
            vkCmdSetViewport(command_buffer, 0, 1, &layer->viewport);
            vkCmdSetScissor(command_buffer, 0, 1, &layer->scissor);
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
			zest_PipelinePushConstantStageFlags(pipeline, 0),
			zest_PipelinePushConstantOffset(pipeline, 0),
			zest_PipelinePushConstantSize(pipeline, 0),
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
            vkCmdSetViewport(command_buffer, 0, 1, &current->viewport);
            vkCmdSetScissor(command_buffer, 0, 1, &current->scissor);
            has_instruction_view_port = true;
            continue;
        } else if(!has_instruction_view_port) {
            vkCmdSetViewport(command_buffer, 0, 1, &layer->viewport);
            vkCmdSetScissor(command_buffer, 0, 1, &layer->scissor);
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
            zest_PipelinePushConstantStageFlags(pipeline, 0),
            zest_PipelinePushConstantOffset(pipeline, 0),
            zest_PipelinePushConstantSize(pipeline, 0),
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
    vkCmdBindVertexBuffers(context->backend->command_buffer, 0, 1, zest_GetBufferDeviceBuffer(buffer), offsets);
}

void zest_cmd_BindMeshIndexBuffer(const zest_frame_graph_context context, zest_layer_handle layer_handle) {
    ZEST_ASSERT_HANDLE(context);        //Not valid context, this command must be called within a frame graph execution callback
    zest_layer layer = (zest_layer)zest__get_store_resource_checked(&ZestRenderer->layers, layer_handle.value);
    ZEST_ASSERT(layer->index_data);    //There's no index data in the buffer. Did you call zest_AddMeshToLayer?
    zest_buffer_t *buffer = layer->index_data;
    vkCmdBindIndexBuffer(context->backend->command_buffer, *zest_GetBufferDeviceBuffer(buffer), buffer->buffer_offset, VK_INDEX_TYPE_UINT32);
}

// -- End Frame graph context functions
