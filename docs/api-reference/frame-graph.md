# Frame Graph API

Functions for building and executing frame graphs.

## Graph Building

### zest_BeginFrameGraph

Start building a frame graph.

```cpp
zest_bool zest_BeginFrameGraph(
    zest_context context,
    const char *name,
    zest_frame_graph_cache_key_t *cache_key
);
```

---

### zest_EndFrameGraph

Compile and finalize the frame graph.

```cpp
zest_frame_graph zest_EndFrameGraph(void);
```

---

### zest_EndFrameGraphAndExecute

Compile and immediately execute.

```cpp
void zest_EndFrameGraphAndExecute(void);
```

---

## Caching

### zest_InitialiseCacheKey

Create a cache key for frame graph caching.

```cpp
zest_frame_graph_cache_key_t zest_InitialiseCacheKey(
    zest_context context,
    void *custom_data,
    zest_size custom_size
);
```

---

### zest_GetCachedFrameGraph

Retrieve cached frame graph or NULL.

```cpp
zest_frame_graph zest_GetCachedFrameGraph(
    zest_context context,
    zest_frame_graph_cache_key_t *cache_key
);
```

---

### zest_FlushCachedFrameGraphs

Clear all cached frame graphs.

```cpp
void zest_FlushCachedFrameGraphs(zest_context context);
```

---

## Passes

### zest_BeginRenderPass

Start a render pass.

```cpp
zest_pass_node zest_BeginRenderPass(const char *name);
```

---

### zest_BeginComputePass

Start a compute pass.

```cpp
zest_pass_node zest_BeginComputePass(zest_compute compute, const char *name);
```

---

### zest_BeginTransferPass

Start a transfer pass.

```cpp
zest_pass_node zest_BeginTransferPass(const char *name);
```

---

### zest_EndPass

End current pass definition.

```cpp
void zest_EndPass(void);
```

---

### zest_SetPassTask

Set render callback for pass.

```cpp
void zest_SetPassTask(zest_render_pass_callback callback, void *user_data);
```

---

## Resource Connections

### zest_ConnectInput

Declare pass reads from resource.

```cpp
void zest_ConnectInput(zest_resource_node resource);
```

---

### zest_ConnectOutput

Declare pass writes to resource.

```cpp
void zest_ConnectOutput(zest_resource_node resource);
```

---

### zest_ConnectSwapChainOutput

Declare pass writes to swapchain.

```cpp
void zest_ConnectSwapChainOutput(void);
```

---

## Resource Import

### zest_ImportSwapchainResource

Import swapchain as a resource.

```cpp
zest_resource_node zest_ImportSwapchainResource(void);
```

---

### zest_ImportImageResource

Import an existing image.

```cpp
zest_resource_node zest_ImportImageResource(
    const char *name,
    zest_image image,
    zest_texture_binding binding
);
```

---

### zest_ImportBufferResource

Import an existing buffer.

```cpp
zest_resource_node zest_ImportBufferResource(const char *name, zest_buffer buffer);
```

---

## Transient Resources

### zest_AddTransientImageResource

Create a frame-lifetime image.

```cpp
zest_resource_node zest_AddTransientImageResource(
    const char *name,
    zest_image_resource_info_t *info
);
```

---

### zest_AddTransientBufferResource

Create a frame-lifetime buffer.

```cpp
zest_resource_node zest_AddTransientBufferResource(
    const char *name,
    zest_buffer_resource_info_t *info
);
```

---

## Execution

### zest_QueueFrameGraphForExecution

Queue frame graph to run during EndFrame.

```cpp
void zest_QueueFrameGraphForExecution(zest_context context, zest_frame_graph frame_graph);
```

---

## Debugging

### zest_PrintCompiledFrameGraph

Print frame graph structure.

```cpp
void zest_PrintCompiledFrameGraph(zest_frame_graph frame_graph);
```

---

## See Also

- [Frame Graph Concept](../concepts/frame-graph.md)
- [Multi-Pass Tutorial](../tutorials/06-multipass.md)
