# Debugging

This page covers tools for debugging and optimizing frame graphs.

## Printing Compiled Graphs

Visualize the compiled frame graph structure:

```cpp
zest_frame_graph graph = zest_EndFrameGraph();
zest_PrintCompiledFrameGraph(graph);
```

Example output:

```
=== Frame Graph: Deferred Renderer ===
Pass 0: G-Buffer [RENDER]
  Outputs: Albedo, Normal, Depth
  Barriers: 3 image transitions

Pass 1: Lighting [RENDER]
  Inputs: Albedo, Normal, Depth
  Outputs: Swapchain
  Barriers: 3 image transitions, 1 swapchain acquire

Resources:
  Albedo: Transient (1920x1080, R8G8B8A8_UNORM)
  Normal: Transient (1920x1080, R8G8B8A8_UNORM)
  Depth: Transient (1920x1080, D32_SFLOAT)
  Swapchain: Imported
```

## Checking Compilation Results

Verify frame graph compiled successfully:

```cpp
zest_frame_graph graph = zest_EndFrameGraph();
zest_frame_graph_result result = zest_GetFrameGraphResult(graph);

switch (result) {
    case zest_frame_graph_result_success:
        // All good
        break;
    case zest_frame_graph_result_no_passes:
        // Graph has no passes
        break;
    case zest_frame_graph_result_cycle_detected:
        // Circular dependency found
        break;
    // ... other error cases
}
```

## Understanding Pass Culling

The frame graph compiler automatically removes unused passes. A pass is culled if:

1. It has no connection to the swapchain (directly or through other passes)
2. It produces no essential resources
3. It is not marked with `zest_DoNotCull()`

### Preventing Culling

For passes that should always execute (e.g., debug output, compute side effects):

```cpp
zest_BeginRenderPass("Debug Overlay"); {
    zest_DoNotCull();  // Keep even without swapchain connection
    zest_SetPassTask(DebugCallback, app);
    zest_EndPass();
}
```

### Essential Resources

Mark resources whose production should never be culled:

```cpp
zest_resource_node debug_buffer = zest_AddTransientBufferResource("Debug", &info);
zest_FlagResourceAsEssential(debug_buffer);

// Any pass that outputs to debug_buffer will not be culled
```

## Common Issues

### Pass Not Executing

**Symptom:** A pass callback is never called.

**Causes:**
1. Pass was culled (no path to swapchain)
2. Missing `zest_ConnectOutput()` or `zest_ConnectInput()` calls
3. Resource connection creates no dependency chain to output

**Solution:**
```cpp
// Option 1: Connect to something that reaches swapchain
zest_ConnectOutput(resource_used_by_later_pass);

// Option 2: Force no culling
zest_DoNotCull();

// Option 3: Mark output as essential
zest_FlagResourceAsEssential(output_resource);
```

### Incorrect Barrier Timing

**Symptom:** Visual artifacts, validation errors about resource states.

**Causes:**
1. Missing resource connection (input or output)
2. Resource used without declaring dependency

**Solution:** Ensure all resource accesses are declared:
```cpp
// If you read a resource, connect it as input
zest_ConnectInput(shadow_map);

// If you write a resource, connect it as output
zest_ConnectOutput(color_target);

// If you read-modify-write, connect both
zest_ConnectInput(particle_buffer);
zest_ConnectOutput(particle_buffer);
```

### Transient Resource Aliasing Issues

**Symptom:** Resources appear corrupted when aliasing is enabled.

**Causes:**
1. Resource lifetimes overlap unexpectedly
2. Missing dependency between passes

**Solution:** Verify pass order through connections or use `zest_PrintCompiledFrameGraph()` to inspect execution order.

## Performance Debugging

### Minimize Pass Count

Each pass has overhead. Combine passes when logical:

```cpp
// Instead of separate clear and draw passes:
zest_BeginRenderPass("Clear"); { ... }
zest_BeginRenderPass("Draw"); { ... }

// Combine into one:
zest_BeginRenderPass("Draw"); {
    zest_SetResourceClearColor(target, 0, 0, 0, 1);
    zest_ConnectOutput(target);
    zest_SetPassTask(DrawCallback, app);
    zest_EndPass();
}
```

### Cache Frame Graphs

Avoid recompiling every frame:

```cpp
// BAD: Rebuilds every frame
if (zest_BeginFrameGraph(context, "Graph", NULL)) {
    // ...
    graph = zest_EndFrameGraph();
}

// GOOD: Caches and reuses
zest_frame_graph_cache_key_t key = zest_InitialiseCacheKey(context, NULL, 0);
graph = zest_GetCachedFrameGraph(context, &key);
if (!graph) {
    if (zest_BeginFrameGraph(context, "Graph", &key)) {
        // ...
        graph = zest_EndFrameGraph();
    }
}
```

### Use Transient Resources

Transient resources benefit from memory aliasing:

```cpp
// Transient: memory can be shared with non-overlapping resources
zest_resource_node temp = zest_AddTransientImageResource("Temp", &info);

// vs. Imported: dedicated memory allocation
zest_resource_node persistent = zest_ImportImageResource("Texture", image, binding);
```

## Validation Layers

Enable Vulkan validation layers during development:

```cpp
zest_device_info_t device_info = zest_CreateDeviceInfo();
device_info.enable_validation = zest_true;

zest_device device = zest_CreateDevice(&device_info);
```

Validation layers catch:
- Incorrect resource states
- Missing synchronization
- Invalid API usage

## Best Practices

1. **Print during development** - Use `zest_PrintCompiledFrameGraph()` to verify structure
2. **Check compilation results** - Handle errors gracefully
3. **Enable validation** - Catch synchronization issues early
4. **Name everything** - Pass and resource names appear in debug output
5. **Understand culling** - Know why passes are removed

## See Also

- [Passes](passes.md) - Pass types and culling options
- [Resources](resources.md) - Essential resource flags
- [Execution](execution.md) - Building and caching
- [Frame Graph API](../../api-reference/frame-graph.md) - Function reference
