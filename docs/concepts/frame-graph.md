# Frame Graph

The frame graph is Zest's core execution model. Instead of manually managing barriers, semaphores, and resource states, you declare a graph of render passes with their dependencies, and Zest handles the rest.

## Why Frame Graphs?

Traditional Vulkan requires:

```cpp
// Manual barrier insertion
vkCmdPipelineBarrier(cmd,
    VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
    VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
    0, 0, nullptr, 0, nullptr, 1, &imageBarrier);

// Explicit semaphore management
VkSubmitInfo submit = {...};
submit.waitSemaphoreCount = 1;
submit.pWaitSemaphores = &renderFinished;
```

This is error-prone and verbose. Frame graphs solve this by:

1. **Automatic barriers** - Inserted based on resource usage
2. **Automatic synchronization** - Semaphores between queues
3. **Pass culling** - Unused passes are removed
4. **Resource aliasing** - Transient resources can share memory

## Basic Structure

```cpp
if (zest_BeginFrameGraph(context, "My Graph", &cache_key)) {
    // 1. Import external resources
    zest_ImportSwapchainResource();

    // 2. Create transient resources (optional)
    zest_resource_node depth = zest_AddTransientImageResource("Depth", &depth_info);

    // 3. Define passes
    zest_BeginRenderPass("Shadow Pass"); {
        zest_ConnectOutput(shadow_map);
        zest_SetPassTask(RenderShadows, app);
        zest_EndPass();
    }

    zest_BeginRenderPass("Main Pass"); {
        zest_ConnectInput(shadow_map);
        zest_ConnectSwapChainOutput();
        zest_SetPassTask(RenderScene, app);
        zest_EndPass();
    }

    // 4. Compile
    frame_graph = zest_EndFrameGraph();
}
```

## Pass Types

### Render Pass

For graphics pipeline operations (drawing):

```cpp
zest_BeginRenderPass("Draw Scene"); {
    zest_ConnectOutput(color_target);
    zest_ConnectOutput(depth_target);
    zest_SetPassTask(DrawCallback, user_data);
    zest_EndPass();
}
```

### Compute Pass

For compute shader dispatch:

```cpp
zest_compute compute = zest_GetCompute(compute_handle);
zest_BeginComputePass(compute, "Particle Sim"); {
    zest_ConnectInput(particle_positions);
    zest_ConnectOutput(particle_positions);  // Read-modify-write
    zest_SetPassTask(ComputeCallback, user_data);
    zest_EndPass();
}
```

### Transfer Pass

For data uploads and copies:

```cpp
zest_BeginTransferPass("Upload"); {
    zest_ConnectOutput(instance_buffer);
    zest_SetPassTask(UploadCallback, layer);
    zest_EndPass();
}
```

## Resource Connections

### Inputs and Outputs

```cpp
zest_ConnectInput(resource);   // Pass reads from this resource
zest_ConnectOutput(resource);  // Pass writes to this resource
zest_ConnectSwapChainOutput(); // Pass writes to swapchain
```

### Resource Groups

For multiple related resources:

```cpp
zest_resource_group group = zest_CreateResourceGroup();
zest_AddResourceToGroup(group, albedo);
zest_AddResourceToGroup(group, normal);
zest_AddResourceToGroup(group, depth);

zest_BeginRenderPass("G-Buffer"); {
    zest_ConnectOutputGroup(group);
    zest_SetPassTask(GBufferCallback, app);
    zest_EndPass();
}
```

## Transient Resources

Resources that only exist within the frame:

```cpp
// Image resource
zest_image_resource_info_t img_info = {
    .format = zest_format_r16g16b16a16_sfloat,
    .usage_hint = zest_resource_usage_hint_copyable,
    .width = 1920,
    .height = 1080,
    .mip_levels = 1
};
zest_resource_node hdr_target = zest_AddTransientImageResource("HDR", &img_info);

// Buffer resource
zest_buffer_resource_info_t buf_info = {
    .size = sizeof(particle_t) * MAX_PARTICLES,
    .usage_hint = zest_resource_usage_hint_vertex_storage
};
zest_resource_node particles = zest_AddTransientBufferResource("Particles", &buf_info);
```

Transient resources:

- Are allocated from a pool
- Can share memory with non-overlapping resources
- Are automatically cleaned up

## Importing Resources

### Swapchain

```cpp
zest_resource_node swapchain = zest_ImportSwapchainResource();
```

### Images

```cpp
zest_image image = zest_GetImage(image_handle);
zest_resource_node tex = zest_ImportImageResource("Texture", image, zest_texture_2d_binding);
```

### Buffers

```cpp
zest_buffer buffer = zest_GetBuffer(buffer_handle);
zest_resource_node buf = zest_ImportBufferResource("Instances", buffer);
```

## Frame Graph Caching

Compiling frame graphs has CPU overhead. Cache them when the structure is static:

```cpp
// Create a cache key
zest_frame_graph_cache_key_t key = zest_InitialiseCacheKey(context, 0, 0);

// Or with custom data (for multiple configurations)
struct { int render_mode; } config = {1};
zest_frame_graph_cache_key_t key = zest_InitialiseCacheKey(
    context, &config, sizeof(config));

// Try to get cached graph
zest_frame_graph graph = zest_GetCachedFrameGraph(context, &key);

if (!graph) {
    // Build new graph (only happens once per configuration)
    if (zest_BeginFrameGraph(context, "Graph", &key)) {
        // ... define passes ...
        graph = zest_EndFrameGraph();
    }
}

// Execute
zest_QueueFrameGraphForExecution(context, graph);
```

### Cache Invalidation

```cpp
// Clear all cached graphs (e.g., on window resize)
zest_FlushCachedFrameGraphs(context);
```

## Pass Tasks (Callbacks)

```cpp
void MyRenderCallback(const zest_command_list cmd, void* user_data) {
    app_t* app = (app_t*)user_data;

    // Bind pipeline
    zest_pipeline pipeline = zest_PipelineWithTemplate(app->pipeline, cmd);
    zest_cmd_BindPipeline(cmd, pipeline);

    // Set viewport/scissor
    zest_cmd_SetScreenSizedViewport(cmd);

    // Bind vertex buffer
    zest_cmd_BindVertexBuffer(cmd, buffer, 0);

    // Draw
    zest_cmd_Draw(cmd, vertex_count, 1, 0, 0);
}

// In frame graph
zest_SetPassTask(MyRenderCallback, app);
```

## Pass Options

### Prevent Culling

By default, passes without connections to the swapchain are culled:

```cpp
zest_BeginRenderPass("Debug Pass"); {
    zest_DoNotCull();  // Keep this pass even if unused
    zest_SetPassTask(DebugCallback, app);
    zest_EndPass();
}
```

### Essential Resources

Mark resources that should prevent pass culling:

```cpp
zest_FlagResourceAsEssential(important_buffer);
```

### Descriptor Sets

Pass custom descriptor sets to a pass:

```cpp
VkDescriptorSet sets[] = {my_set};
zest_SetDescriptorSets(1, sets);
```

## Debugging Frame Graphs

### Print Compiled Graph

```cpp
zest_frame_graph graph = zest_EndFrameGraph();
zest_PrintCompiledFrameGraph(graph);
```

Output shows passes, barriers, and resource states.

### Check Compilation Result

```cpp
zest_frame_graph_result result = zest_GetFrameGraphResult(graph);
if (result != zest_frame_graph_result_success) {
    // Handle error
}
```

## Example: Multi-Pass Rendering

```cpp
if (zest_BeginFrameGraph(context, "Deferred Renderer", &cache_key)) {
    zest_ImportSwapchainResource();

    // Transient G-buffer
    zest_resource_node albedo = zest_AddTransientImageResource("Albedo", &rgba_info);
    zest_resource_node normal = zest_AddTransientImageResource("Normal", &rgba_info);
    zest_resource_node depth = zest_AddTransientImageResource("Depth", &depth_info);

    // Pass 1: G-Buffer
    zest_BeginRenderPass("G-Buffer"); {
        zest_ConnectOutput(albedo);
        zest_ConnectOutput(normal);
        zest_ConnectOutput(depth);
        zest_SetPassTask(RenderGBuffer, app);
        zest_EndPass();
    }

    // Pass 2: Lighting
    zest_BeginRenderPass("Lighting"); {
        zest_ConnectInput(albedo);
        zest_ConnectInput(normal);
        zest_ConnectInput(depth);
        zest_ConnectSwapChainOutput();
        zest_SetPassTask(RenderLighting, app);
        zest_EndPass();
    }

    frame_graph = zest_EndFrameGraph();
}
```

## Best Practices

1. **Cache when possible** - Static graphs should be cached
2. **Minimize pass count** - Combine passes when logical
3. **Use transient resources** - For intermediate render targets
4. **Group related resources** - Use resource groups for G-buffers
5. **Check compilation results** - During development

## See Also

- [First Application](../getting-started/first-application.md) - Basic frame graph usage
- [Multi-Pass Tutorial](../tutorials/06-multipass.md) - Advanced frame graph patterns
- [Frame Graph API](../api-reference/frame-graph.md) - Complete function reference
