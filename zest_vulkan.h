/*
Zest Vulkan Implementation

    -- [Backend_structs]
    -- [Cleanup_functions]
    -- [Backend_setup_functions]
    -- [Backend_cleanup_functions]
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

// -- Cleanup_functions
void zest__cleanup_shader_resource_store() {
    zest_shader_resources_t *shader_resources = (zest_shader_resources)ZestRenderer->shader_resources.data;
    for (int i = 0; i != ZestRenderer->shader_resources.current_size; ++i) {
        if (ZEST_VALID_HANDLE(&shader_resources[i])) {
            zest_shader_resources resource = &shader_resources[i];
			zest_ForEachFrameInFlight(fif) {
				zest_vec_free(resource->backend->sets[fif]);
			}
			zest_vec_free(resource->backend->binding_sets);
        }
    }
    zest__free_store(&ZestRenderer->shader_resources);
}
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
}

void zest__vk_cleanup_set_layout(zest_set_layout layout) {
    zest_vec_free(layout->backend->layout_bindings);
    vkDestroyDescriptorSetLayout(ZestDevice->backend->logical_device, layout->backend->vk_layout, &ZestDevice->backend->allocation_callbacks);
    zest_vec_foreach(i, layout->backend->descriptor_indexes) {
        zest_vec_free(layout->backend->descriptor_indexes[i].free_indices);
    }
    zest_vec_free(layout->backend->descriptor_indexes);
    zest_vec_free(layout->pool->backend->vk_pool_sizes);
    if (layout->pool) {
        vkDestroyDescriptorPool(ZestDevice->backend->logical_device, layout->pool->backend->vk_descriptor_pool, &ZestDevice->backend->allocation_callbacks);
        ZEST__FREE(layout->pool->backend);
    }
    ZEST__FREE(layout->backend);
}

// -- End Backend cleanup functions

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
// -- End Frame graph context functions
