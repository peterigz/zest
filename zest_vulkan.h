/*
Zest Vulkan Implementation

    -- [Backend_structs]
    -- [Other_setup_structs]
    -- [Enum_to_string_functions]
    -- [Initialisation_functions]
    -- [Swapchain_setup]
    -- [Backend_setup_functions]
    -- [Backend_cleanup_functions]
    -- [Images]
    -- [General_helpers]
    -- [Frame_graph_context_functions]
*/

// -- Backend_structs
typedef struct zest_window_backend_t {
    VkSurfaceKHR surface;
} zest_window_backend_t;

typedef struct zest_device_memory_pool_backend_t {
    VkBufferCreateInfo buffer_info;
    VkBuffer vk_buffer;
    VkDeviceMemory memory;
    VkImageUsageFlags usage_flags;
    VkMemoryPropertyFlags property_flags;
} zest_device_memory_pool_backend_t;

typedef struct zest_resource_backend_t {
    VkAccessFlags current_access_mask;
    VkPipelineStageFlags last_stage_mask;
    VkImageLayout current_layout;                   // The current layout of the image in the resource
    VkImageLayout final_layout;                     // Layout resource should be in after last use in this graph
    VkImageLayout *linked_layout;                   // A link to the layout in the texture so that the layout in the texture can be updated as it's transitioned in the render graph
    VkFormat format;
} zest_resource_backend_t;

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

typedef struct zest_renderer_backend_t {
    VkPipelineCache pipeline_cache;
    VkBuffer *used_buffers_ready_for_freeing;
    VkCommandBuffer utility_command_buffer[ZEST_MAX_FIF];
    VkCommandBuffer one_time_command_buffer;
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
// -- Enum to string functions

// -- Swapchain_setup
zest_swapchain_support_details_t zest__vk_query_swapchain_support(VkPhysicalDevice physical_device) {
    zest_swapchain_support_details_t details;

    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physical_device, ZestRenderer->main_window->backend->surface, &details.capabilities);

    vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device, ZestRenderer->main_window->backend->surface, &details.formats_count, ZEST_NULL);

    if (details.formats_count != 0) {
        details.formats = ZEST__ALLOCATE(sizeof(VkSurfaceFormatKHR) * details.formats_count);
        vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device, ZestRenderer->main_window->backend->surface, &details.formats_count, details.formats);
    }

    vkGetPhysicalDeviceSurfacePresentModesKHR(physical_device, ZestRenderer->main_window->backend->surface, &details.present_modes_count, ZEST_NULL);

    if (details.present_modes_count != 0) {
        details.present_modes = ZEST__ALLOCATE(sizeof(VkPresentModeKHR) * details.present_modes_count);
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
            .format = VK_FORMAT_B8G8R8A8_SRGB, // Prefer SRGB for automatic gamma correction
            .colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR
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
        .width = (zest_uint)(fb_width),
        .height = (zest_uint)(fb_height)
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
    swapchain->format = (zest_texture_format)surfaceFormat.format;

    swapchain->format = (zest_texture_format)surfaceFormat.format;
    swapchain->size = (zest_extent2d_t){ extent.width, extent.height };
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

    ZEST_SET_MEMORY_CONTEXT(zest_vk_renderer, zest_vk_swapchain);
    ZEST_RETURN_FALSE_ON_FAIL(vkCreateSwapchainKHR(ZestDevice->backend->logical_device, &createInfo, &ZestDevice->backend->allocation_callbacks, &swapchain->backend->vk_swapchain));

    swapchain->image_count = image_count;

    vkGetSwapchainImagesKHR(ZestDevice->backend->logical_device, swapchain->backend->vk_swapchain, &image_count, ZEST_NULL);
    VkImage *images = 0;
    zest_vec_resize(images, image_count);
    vkGetSwapchainImagesKHR(ZestDevice->backend->logical_device, swapchain->backend->vk_swapchain, &image_count, images);

    zest_vec_resize(swapchain->images, image_count);
    zest_vec_resize(swapchain->views, image_count);
    zest_image_info_t image_info = zest_CreateImageInfo(extent.width, extent.height);
    image_info.aspect_flags = zest_image_aspect_color_bit;
    image_info.format = (zest_texture_format)surfaceFormat.format;
    zest_vec_foreach(i, images) {
        swapchain->images[i] = (zest_image_t){ 0 };
        swapchain->images[i].backend = zest__new_image_backend();
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
        ZEST_SET_MEMORY_CONTEXT(zest_vk_renderer, zest_vk_semaphore);
        ZEST_RETURN_FALSE_ON_FAIL(vkCreateSemaphore(ZestDevice->backend->logical_device, &semaphore_info, &ZestDevice->backend->allocation_callbacks, &swapchain->backend->vk_image_available_semaphore[i]));
    }

    zest_vec_resize(swapchain->backend->vk_render_finished_semaphore, swapchain->image_count);

    zest_vec_foreach(i, swapchain->backend->vk_render_finished_semaphore) {
        ZEST_SET_MEMORY_CONTEXT(zest_vk_renderer, zest_vk_semaphore);
        ZEST_RETURN_FALSE_ON_FAIL(vkCreateSemaphore(ZestDevice->backend->logical_device, &semaphore_info, &ZestDevice->backend->allocation_callbacks, &swapchain->backend->vk_render_finished_semaphore[i]));
    }

    zest_vec_foreach(i, swapchain->images) {
        swapchain->views[i] = zest__create_image_view_backend(&swapchain->images[i], zest_image_view_type_2d_array, 1, 0, 0, 1, 0);
    }

    return ZEST_TRUE;
}
// -- End Swapchain setup

// -- Initialisation_functions
zest_bool zest__vk_initialise_device() {
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

    ZEST_SET_MEMORY_CONTEXT(zest_vk_device, zest_vk_debug_messenger);
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

zest_uint zest_find_memory_type(zest_uint typeFilter, VkMemoryPropertyFlags properties) {
    VkPhysicalDeviceMemoryProperties memory_properties;
    vkGetPhysicalDeviceMemoryProperties(ZestDevice->backend->physical_device, &memory_properties);

    for (zest_uint i = 0; i < memory_properties.memoryTypeCount; i++) {
        if ((typeFilter & (1 << i)) && (memory_properties.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }

    return ZEST_INVALID;
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

    ZEST_SET_MEMORY_CONTEXT(zest_vk_device, zest_vk_instance);
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
            ZEST_RETURN_RESULT_ON_FAIL(zest__create_queue_command_pool(queue->family_index, &queue->command_pool[fif]));
			alloc_info.commandPool = queue->command_pool[fif];
			zest_vec_resize(queue->command_buffers[fif], alloc_info.commandBufferCount);
			ZEST_SET_MEMORY_CONTEXT(zest_vk_renderer, zest_vk_command_buffer);
			ZEST_VK_ASSERT_RESULT(vkAllocateCommandBuffers(ZestDevice->backend->logical_device, &alloc_info, queue->command_buffers[fif]));
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
    ZEST_SET_MEMORY_CONTEXT(zest_vk_device, zest_vk_logical_device);
    ZEST_VK_LOG(vkCreateDevice(ZestDevice->backend->physical_device, &create_info, &ZestDevice->backend->allocation_callbacks, &ZestDevice->backend->logical_device));

    if (ZestDevice->backend->logical_device == VK_NULL_HANDLE) {
        return ZEST_FALSE;
    }

    vkGetDeviceQueue(ZestDevice->backend->logical_device, indices.graphics_family_index, 0, &ZestDevice->graphics_queue.vk_queue);
    vkGetDeviceQueue(ZestDevice->backend->logical_device, indices.compute_family_index, 0, &ZestDevice->compute_queue.vk_queue);
    vkGetDeviceQueue(ZestDevice->backend->logical_device, indices.transfer_family_index, 0, &ZestDevice->transfer_queue.vk_queue);

    ZestDevice->graphics_queue_family_index = indices.graphics_family_index;
    ZestDevice->transfer_queue_family_index = indices.transfer_family_index;
    ZestDevice->compute_queue_family_index = indices.compute_family_index;

    zest_vec_push(ZestDevice->queues, &ZestDevice->graphics_queue);
    ZestDevice->graphics_queue.family_index = indices.graphics_family_index;
    if (ZestDevice->graphics_queue.vk_queue != ZestDevice->compute_queue.vk_queue) {
		zest_vec_push(ZestDevice->queues, &ZestDevice->compute_queue);
		ZestDevice->compute_queue.family_index = indices.compute_family_index;
    }
    if (ZestDevice->graphics_queue.vk_queue != ZestDevice->transfer_queue.vk_queue) {
		zest_vec_push(ZestDevice->queues, &ZestDevice->transfer_queue);
		ZestDevice->transfer_queue.family_index = indices.transfer_family_index;
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

			ZEST_SET_MEMORY_CONTEXT(zest_vk_device, zest_vk_semaphore);
            vkCreateSemaphore(ZestDevice->backend->logical_device, &semaphore_info, &ZestDevice->backend->allocation_callbacks, &queue->semaphore[fif]);

            queue->current_count[fif] = 0;
        }
    }

    VkCommandPoolCreateInfo cmd_info_pool = { 0 };
    cmd_info_pool.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    cmd_info_pool.queueFamilyIndex = ZestDevice->graphics_queue_family_index;
    cmd_info_pool.flags = 0;    //Maybe needs transient bit?
    ZEST_APPEND_LOG(ZestDevice->log_path.str, "Creating one time command pools");
	ZEST_SET_MEMORY_CONTEXT(zest_vk_device, zest_vk_command_pool);
    zest_ForEachFrameInFlight(fif) {
        ZEST_RETURN_FALSE_ON_FAIL(vkCreateCommandPool(ZestDevice->backend->logical_device, &cmd_info_pool, &ZestDevice->backend->allocation_callbacks, &ZestDevice->backend->one_time_command_pool[fif]));
    }

    ZEST_APPEND_LOG(ZestDevice->log_path.str, "Create queue command buffer pools");
    ZEST_RETURN_FALSE_ON_FAIL(zest__vk_create_command_buffer_pools());

    return ZEST_TRUE;
}
// -- End initialisation functions

// -- Backend_setup_functions
void *zest__vk_new_device_backend() {
    zest_device_backend backend = zloc_Allocate(ZestDevice->allocator, sizeof(zest_device_backend_t));
    *backend = (zest_device_backend_t){ 0 };
    backend->allocation_callbacks.pUserData = &ZestDevice->vulkan_memory_info;
    backend->allocation_callbacks.pfnAllocation = zest_vk_allocate_callback;
    backend->allocation_callbacks.pfnReallocation = zest_vk_reallocate_callback;
    backend->allocation_callbacks.pfnFree = zest_vk_free_callback;
    return backend;
}

void *zest__vk_new_renderer_backend() {
    zest_renderer_backend backend = ZEST__NEW(zest_renderer_backend);
    *backend = (zest_renderer_backend_t){ 0 };
    return backend;
}

void *zest__vk_new_frame_graph_context_backend() {
    zest_frame_graph_context_backend_t *backend_context = zloc_LinearAllocation(
        ZestRenderer->frame_graph_allocator[ZEST_FIF],
        sizeof(zest_frame_graph_context_backend_t)
    );
    *backend_context = (zest_frame_graph_context_backend_t){ 0 };
    return backend_context;
}

void *zest__vk_new_swapchain_backend() {
    zest_swapchain_backend swapchain_backend = ZEST__NEW(zest_swapchain_backend);
    *swapchain_backend = (zest_swapchain_backend_t){ 0 };
    return swapchain_backend;
}

void *zest__vk_new_buffer_backend() {
    zest_buffer_backend buffer_backend = ZEST__NEW(zest_buffer_backend);
    *buffer_backend = (zest_buffer_backend_t){ 0 };
    return buffer_backend;
}

void *zest__vk_new_uniform_buffer_backend() {
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

void *zest__vk_new_image_backend() {
    zest_image_backend image_backend = ZEST__NEW(zest_image_backend);
    *image_backend = (zest_image_backend_t){ 0 };
    return image_backend;
}

void *zest__vk_new_frame_graph_image_backend(zloc_linear_allocator_t *allocator) {
    zest_image_backend image_backend = zloc_LinearAllocation(allocator, sizeof(zest_image_backend_t));
    *image_backend = (zest_image_backend_t){ 0 };
    return image_backend;
}

void *zest__vk_new_compute_backend() {
    zest_compute_backend compute_backend = ZEST__NEW(zest_compute_backend);
    *compute_backend = (zest_compute_backend_t){ 0 };
    return compute_backend;
}

void *zest__vk_new_set_layout_backend() {
    zest_set_layout_backend layout_backend = ZEST__NEW(zest_set_layout_backend);
    *layout_backend = (zest_set_layout_backend_t){ 0 };
    return layout_backend;
}

void *zest__vk_new_descriptor_pool_backend() {
    zest_descriptor_pool_backend pool = ZEST__NEW(zest_descriptor_pool_backend);
    *pool = (zest_descriptor_pool_backend_t){ 0 };
    return pool;
}

void *zest__vk_new_sampler_backend() {
    zest_sampler_backend pool = ZEST__NEW(zest_sampler_backend);
    *pool = (zest_sampler_backend_t){ 0 };
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
	if(image->backend->vk_image) vkDestroyImage(ZestDevice->backend->logical_device, image->backend->vk_image, &ZestDevice->backend->allocation_callbacks);
	image->backend->vk_image = VK_NULL_HANDLE;
    ZEST__FREE(image->backend);
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
	if(view->backend->vk_view) vkDestroyImageView(ZestDevice->backend->logical_device, view->backend->vk_view, &ZestDevice->backend->allocation_callbacks);
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
		zest_vec_free(shader_resource->backend->sets[fif]);
	}
	zest_vec_free(shader_resource->backend->binding_sets);
    ZEST__FREE(shader_resource->backend);
}

// -- End Backend cleanup functions

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

    ZEST_SET_MEMORY_CONTEXT(zest_vk_renderer, zest_vk_image);
    ZEST_RETURN_FALSE_ON_FAIL(vkCreateImage(ZestDevice->backend->logical_device, &image_info, &ZestDevice->backend->allocation_callbacks, &image->backend->vk_image));

    VkMemoryRequirements memory_requirements;
    vkGetImageMemoryRequirements(ZestDevice->backend->logical_device, image->backend->vk_image, &memory_requirements);

    zest_buffer_info_t buffer_info = { 0 };
    buffer_info.image_usage_flags = usage;
    buffer_info.property_flags = memory_properties;
    buffer_info.memory_type_bits = memory_requirements.memoryTypeBits;
    buffer_info.alignment = memory_requirements.alignment;
    image->buffer = zest_CreateBuffer(memory_requirements.size, &buffer_info, image->backend->vk_image);

    if (image->buffer) {
        vkBindImageMemory(ZestDevice->backend->logical_device, image->backend->vk_image, zest_GetBufferDeviceMemory(image->buffer), image->buffer->memory_offset);
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

	ZEST_SET_MEMORY_CONTEXT(zest_vk_renderer, zest_vk_image_view);
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

    ZEST_SET_MEMORY_CONTEXT(zest_vk_renderer, zest_vk_image_view);
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
	ZEST_SET_MEMORY_CONTEXT(zest_vk_renderer, zest_vk_sampler);
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
    ZEST_SET_MEMORY_CONTEXT(zest_vk_renderer, zest_vk_surface);
    ZEST_RETURN_FALSE_ON_FAIL(vkCreateWin32SurfaceKHR(ZestDevice->backend->instance, &surface_create_info, &ZestDevice->backend->allocation_callbacks, &window->backend->surface));
    return ZEST_TRUE;
}

zest_bool zest__vk_begin_single_time_commands() {
    ZEST_ASSERT(ZestRenderer->backend->one_time_command_buffer == VK_NULL_HANDLE);  //The command buffer must be null. Call zest__vk_end_single_time_commands before calling begin again.
    VkCommandBufferAllocateInfo alloc_info = { 0 };
    alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    alloc_info.commandPool = ZestDevice->backend->one_time_command_pool[ZEST_FIF];
    alloc_info.commandBufferCount = 1;

    ZEST_SET_MEMORY_CONTEXT(zest_vk_renderer, zest_vk_command_buffer);
    ZEST_RETURN_FALSE_ON_FAIL(vkAllocateCommandBuffers(ZestDevice->backend->logical_device, &alloc_info, &ZestRenderer->backend->one_time_command_buffer));

    VkCommandBufferBeginInfo begin_info = { 0 };
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    ZEST_RETURN_FALSE_ON_FAIL(vkBeginCommandBuffer(ZestRenderer->backend->one_time_command_buffer, &begin_info));

    return ZEST_TRUE;
}

zest_bool zest__vk_end_single_time_commands() {
    ZEST_VK_ASSERT_RESULT(vkEndCommandBuffer(ZestRenderer->backend->one_time_command_buffer));

    VkSubmitInfo submit_info = { 0 };
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &ZestRenderer->backend->one_time_command_buffer;

    VkFence fence = zest_CreateFence();
    ZEST_RETURN_FALSE_ON_FAIL(vkQueueSubmit(ZestDevice->graphics_queue.vk_queue, 1, &submit_info, fence));
    zest_WaitForFence(fence);
    zest_DestroyFence(fence);

    vkFreeCommandBuffers(ZestDevice->backend->logical_device, ZestDevice->backend->one_time_command_pool[ZEST_FIF], 1, &ZestRenderer->backend->one_time_command_buffer);
    ZestRenderer->backend->one_time_command_buffer = VK_NULL_HANDLE;

    return ZEST_TRUE;
}
// -- End General helpers

// -- Frame_graph_context_functions
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
    VkImageMemoryBarrier blit_src_barrier = zest__create_image_memory_barrier(src_image,
        0,
        VK_ACCESS_TRANSFER_READ_BIT,
        src_current_layout,
        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        src->image.info.aspect_flags,
        mip_to_blit, 1);
    zest__place_image_barrier(context->backend->command_buffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, &blit_src_barrier);

    VkImageMemoryBarrier blit_dst_barrier = zest__create_image_memory_barrier(dst_image,
        0,
        VK_ACCESS_TRANSFER_WRITE_BIT,
        dst_current_layout,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        dst->image.info.aspect_flags,
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
        src->image.info.aspect_flags,
        mip_to_blit, 1);
    zest__place_image_barrier(context->backend->command_buffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, &blit_src_barrier);

    blit_dst_barrier = zest__create_image_memory_barrier(dst_image,
        VK_ACCESS_TRANSFER_WRITE_BIT,
        VK_ACCESS_SHADER_READ_BIT,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        dst_current_layout,
        dst->image.info.aspect_flags,
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
    VkImageMemoryBarrier blit_src_barrier = zest__create_image_memory_barrier(src_image,
        0,
        VK_ACCESS_TRANSFER_READ_BIT,
        src_current_layout,
        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        src->image.info.aspect_flags,
        mip_to_copy, 1);
    zest__place_image_barrier(context->backend->command_buffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, &blit_src_barrier);

    VkImageMemoryBarrier blit_dst_barrier = zest__create_image_memory_barrier(dst_image,
        0,
        VK_ACCESS_TRANSFER_WRITE_BIT,
        dst_current_layout,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        dst->image.info.aspect_flags,
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
        src->image.info.aspect_flags,
        mip_to_copy, 1);
    zest__place_image_barrier(context->backend->command_buffer, VK_PIPELINE_STAGE_TRANSFER_BIT, pipeline_stage, &blit_src_barrier);

    blit_dst_barrier = zest__create_image_memory_barrier(dst_image,
        VK_ACCESS_TRANSFER_WRITE_BIT,
        VK_ACCESS_SHADER_READ_BIT,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        dst_current_layout,
        dst->image.info.aspect_flags,
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

    zest__insert_image_memory_barrier(
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
    zest__insert_image_memory_barrier(
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
    zest__insert_image_memory_barrier(
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
    zest__insert_image_memory_barrier(
        copy_command,
        dst_image->backend->vk_image,
        VK_ACCESS_TRANSFER_WRITE_BIT,
        VK_ACCESS_MEMORY_READ_BIT,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        target_layout,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        image_range);

    zest__insert_image_memory_barrier(
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
    ZEST_RETURN_FALSE_ON_FAIL(zest__create_temporary_image(width, height, 1, VK_SAMPLE_COUNT_1_BIT, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_TILING_LINEAR, VK_IMAGE_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &temp_image, &temp_memory));

    VkImageSubresourceRange image_range = { 0 };
    image_range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    image_range.baseMipLevel = 0;
    image_range.levelCount = 1;
    image_range.baseArrayLayer = 0;
    image_range.layerCount = 1;

    // Transition temporary image to be ready as a transfer destination
    zest__insert_image_memory_barrier(
        copy_command, temp_image, 0, VK_ACCESS_TRANSFER_WRITE_BIT,
        VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, image_range);

    // Transition source image to be ready as a transfer source
    zest__insert_image_memory_barrier(
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
    zest__insert_image_memory_barrier(
        copy_command, temp_image, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL,
        VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, image_range);

    // Transition source image back to its original layout
    zest__insert_image_memory_barrier(
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
	staging_buffer = zest_CreateBuffer(image_size, &buffer_info, ZEST_NULL);
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
