# Passes

Passes are the building blocks of a frame graph. Each pass represents a unit of GPU work with defined inputs and outputs.

## Pass Types

### Render Pass

For graphics pipeline operations (drawing geometry):

```cpp
zest_BeginRenderPass("Draw Scene"); {
    zest_ConnectOutput(color_target);
    zest_ConnectOutput(depth_target);
    zest_SetPassTask(DrawCallback, user_data);
    zest_EndPass();
}
```

Render passes use the graphics queue and support:
- Vertex/index buffer binding
- Graphics pipeline binding
- Draw commands

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

Compute passes:
- Require a `zest_compute` object (contains the compute pipeline)
- Can run on compute or graphics queue
- Support storage buffer/image operations

### Transfer Pass

For data uploads and copies:

```cpp
zest_BeginTransferPass("Upload"); {
    zest_ConnectOutput(instance_buffer);
    zest_SetPassTask(UploadCallback, layer);
    zest_EndPass();
}
```

Transfer passes:
- Handle CPU-to-GPU data transfers
- Can run on dedicated transfer queue if available
- Used for staging buffer uploads

## Resource Connections

### Inputs and Outputs

```cpp
zest_ConnectInput(resource);   // Pass reads from this resource
zest_ConnectOutput(resource);  // Pass writes to this resource
```

The frame graph compiler uses these connections to:
- Determine execution order
- Insert memory barriers
- Track resource state transitions

### Swapchain Output

To write to the swapchain (final presentation):

```cpp
zest_ConnectSwapChainOutput();
```

This marks the pass as contributing to the final image. Passes without a path to the swapchain may be culled.

### Input/Output Groups

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

## Pass Tasks (Callbacks)

Each pass requires a callback function that executes the actual GPU commands:

```cpp
void MyRenderCallback(const zest_command_list cmd, void* user_data) {
    app_t* app = (app_t*)user_data;

    // Bind pipeline
    zest_pipeline pipeline = zest_GetPipeline(app->pipeline, cmd);
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

### Callback Parameters

| Parameter | Type | Description |
|-----------|------|-------------|
| `cmd` | `zest_command_list` | Command buffer for recording commands |
| `user_data` | `void*` | User data passed to `zest_SetPassTask` |

### Utility Callbacks

For passes that need no commands (e.g., just clears):

```cpp
zest_BeginRenderPass("Clear Pass"); {
    zest_ConnectOutput(color_target);
    zest_SetResourceClearColor(color_target, 0.0f, 0.0f, 0.0f, 1.0f);
    zest_SetPassTask(zest_EmptyRenderPass, NULL);
    zest_EndPass();
}
```

`zest_EmptyRenderPass` is a built-in callback that records no commands.

## Pass Options

### Preventing Pass Culling

By default, passes without connections to the swapchain are culled:

```cpp
zest_BeginRenderPass("Debug Pass"); {
    zest_DoNotCull();  // Keep this pass even if unused
    zest_SetPassTask(DebugCallback, app);
    zest_EndPass();
}
```

### Custom Descriptor Sets

Pass additional descriptor sets to a pass:

```cpp
VkDescriptorSet sets[] = {my_custom_set};
zest_SetDescriptorSets(1, sets);
```

This is useful when you need descriptor sets beyond the global bindless set.

## Accessing Resources in Callbacks

Within a pass callback, retrieve connected resources:

```cpp
void MyCallback(const zest_command_list cmd, void* user_data) {
    // Get input resource by name
    zest_resource_node input = zest_GetPassInputResource(cmd, "ShadowMap");

    // Get output resource by name
    zest_resource_node output = zest_GetPassOutputResource(cmd, "ColorTarget");

    // Get buffer directly
    zest_buffer buf = zest_GetPassInputBuffer(cmd, "InstanceData");
    zest_buffer out_buf = zest_GetPassOutputBuffer(cmd, "Results");

    // Use resources...
}
```

## Complete Example

```cpp
void RenderScene(const zest_command_list cmd, void* user_data) {
    app_t* app = (app_t*)user_data;

    // Get pipeline variant for current render state
    zest_pipeline pipeline = zest_GetPipeline(app->mesh_pipeline, cmd);
    zest_cmd_BindPipeline(cmd, pipeline);

    // Set viewport
    zest_cmd_SetScreenSizedViewport(cmd);

    // Bind vertex/index buffers
    zest_cmd_BindVertexBuffer(cmd, app->vertex_buffer, 0);
    zest_cmd_BindIndexBuffer(cmd, app->index_buffer, VK_INDEX_TYPE_UINT32);

    // Push constants
    push_constants_t pc = {
        .mvp = app->camera_mvp,
        .texture_index = app->texture_index
    };
    zest_cmd_PushConstants(cmd, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(pc), &pc);

    // Draw indexed
    zest_cmd_DrawIndexed(cmd, app->index_count, 1, 0, 0, 0);
}

// In frame graph setup
zest_BeginRenderPass("Scene"); {
    zest_ConnectOutput(color_target);
    zest_ConnectOutput(depth_target);
    zest_SetPassTask(RenderScene, app);
    zest_EndPass();
}
```

## Best Practices

1. **Name passes descriptively** - Names appear in debug output and profilers
2. **Minimize pass count** - Combine passes when logical to reduce overhead
3. **Declare all resource dependencies** - Missing connections cause undefined behavior
4. **Use groups for MRT** - Resource groups simplify multiple render target setups
5. **Let culling work** - Avoid `zest_DoNotCull` unless necessary

## See Also

- [Resources](resources.md) - Resource types and management
- [Execution](execution.md) - Building and executing frame graphs
- [Frame Graph API](../../api-reference/frame-graph.md) - Function reference
