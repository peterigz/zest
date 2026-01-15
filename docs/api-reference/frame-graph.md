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

| Parameter | Description |
|-----------|-------------|
| `context` | The context to build for |
| `name` | Debug name for the graph |
| `cache_key` | Optional cache key (pass `NULL` to disable caching) |

Returns `zest_true` if building should proceed, `zest_false` if a cached graph was found.

---

### zest_EndFrameGraph

Compile and finalize the frame graph.

```cpp
zest_frame_graph zest_EndFrameGraph(void);
```

Returns the compiled frame graph.

---

### zest_EndFrameGraphAndExecute

Compile and immediately queue for execution.

```cpp
zest_frame_graph zest_EndFrameGraphAndExecute(void);
```

Returns the compiled frame graph.

---

## Caching

### zest_InitialiseCacheKey

Create a cache key for frame graph caching.

```cpp
zest_frame_graph_cache_key_t zest_InitialiseCacheKey(
    zest_context context,
    const void *user_state,
    zest_size user_state_size
);
```

| Parameter | Description |
|-----------|-------------|
| `context` | The context (incorporates swapchain state) |
| `user_state` | Optional custom data to include in key |
| `user_state_size` | Size of custom data in bytes |

---

### zest_GetCachedFrameGraph

Retrieve a cached frame graph.

```cpp
zest_frame_graph zest_GetCachedFrameGraph(
    zest_context context,
    zest_frame_graph_cache_key_t *cache_key
);
```

Returns the cached frame graph or `NULL` if not found.

---

### zest_FlushCachedFrameGraphs

Clear all cached frame graphs.

```cpp
void zest_FlushCachedFrameGraphs(zest_context context);
```

---

## Passes

### zest_BeginRenderPass

Start a render pass for graphics operations.

```cpp
zest_pass_node zest_BeginRenderPass(const char *name);
```

---

### zest_BeginComputePass

Start a compute pass.

```cpp
zest_pass_node zest_BeginComputePass(zest_compute compute, const char *name);
```

| Parameter | Description |
|-----------|-------------|
| `compute` | The compute object containing the pipeline |
| `name` | Debug name for the pass |

---

### zest_BeginTransferPass

Start a transfer pass for data uploads/copies.

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

Set the callback function for a pass.

```cpp
void zest_SetPassTask(zest_rg_execution_callback callback, void *user_data);
```

| Parameter | Description |
|-----------|-------------|
| `callback` | Function to execute during pass |
| `user_data` | User data passed to callback |

---

### zest_EmptyRenderPass

Built-in callback that records no commands.

```cpp
void zest_EmptyRenderPass(const zest_command_list command_list, void *user_data);
```

Use for passes that only need clears.

---

## Resource Connections

### zest_ConnectInput

Declare that a pass reads from a resource.

```cpp
void zest_ConnectInput(zest_resource_node resource);
```

---

### zest_ConnectOutput

Declare that a pass writes to a resource.

```cpp
void zest_ConnectOutput(zest_resource_node resource);
```

---

### zest_ConnectSwapChainOutput

Declare that a pass writes to the swapchain.

```cpp
void zest_ConnectSwapChainOutput(void);
```

---

### zest_ConnectInputGroup

Connect a resource group as inputs.

```cpp
void zest_ConnectInputGroup(zest_resource_group group);
```

---

### zest_ConnectOutputGroup

Connect a resource group as outputs.

```cpp
void zest_ConnectOutputGroup(zest_resource_group group);
```

---

## Resource Import

### zest_ImportSwapchainResource

Import the swapchain as a resource.

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
    zest_resource_image_provider provider
);
```

| Parameter | Description |
|-----------|-------------|
| `name` | Debug name for the resource |
| `image` | The image to import |
| `provider` | How the image will be accessed (e.g., `zest_texture_2d_binding`) |

---

### zest_ImportBufferResource

Import an existing buffer.

```cpp
zest_resource_node zest_ImportBufferResource(
    const char *name,
    zest_buffer buffer,
    zest_resource_buffer_provider provider
);
```

| Parameter | Description |
|-----------|-------------|
| `name` | Debug name for the resource |
| `buffer` | The buffer to import |
| `provider` | How the buffer will be accessed |

---

## Transient Resources

### zest_AddTransientImageResource

Create a frame-lifetime image resource.

```cpp
zest_resource_node zest_AddTransientImageResource(
    const char *name,
    zest_image_resource_info_t *info
);
```

---

### zest_AddTransientBufferResource

Create a frame-lifetime buffer resource.

```cpp
zest_resource_node zest_AddTransientBufferResource(
    const char *name,
    const zest_buffer_resource_info_t *info
);
```

---

### zest_AddTransientLayerResource

Create a transient resource from a layer.

```cpp
zest_resource_node zest_AddTransientLayerResource(
    const char *name,
    const zest_layer layer,
    zest_bool prev_fif
);
```

| Parameter | Description |
|-----------|-------------|
| `name` | Debug name for the resource |
| `layer` | The layer to use |
| `prev_fif` | If `zest_true`, use previous frame-in-flight's buffer |

---

## Resource Properties

### zest_GetResourceWidth

Get resource width in pixels.

```cpp
zest_uint zest_GetResourceWidth(zest_resource_node resource);
```

---

### zest_GetResourceHeight

Get resource height in pixels.

```cpp
zest_uint zest_GetResourceHeight(zest_resource_node resource);
```

---

### zest_GetResourceMipLevels

Get number of mip levels.

```cpp
zest_uint zest_GetResourceMipLevels(zest_resource_node resource);
```

---

### zest_GetResourceType

Get the resource type.

```cpp
zest_resource_type zest_GetResourceType(zest_resource_node resource_node);
```

---

### zest_GetResourceImage

Get the underlying image from a resource.

```cpp
zest_image zest_GetResourceImage(zest_resource_node resource_node);
```

---

### zest_GetResourceImageDescription

Get full image description.

```cpp
zest_image_info_t zest_GetResourceImageDescription(zest_resource_node resource_node);
```

---

### zest_SetResourceBufferSize

Set buffer resource size.

```cpp
void zest_SetResourceBufferSize(zest_resource_node resource, zest_size size);
```

---

### zest_SetResourceClearColor

Set clear color for a render target.

```cpp
void zest_SetResourceClearColor(
    zest_resource_node resource,
    float red, float green, float blue, float alpha
);
```

---

### zest_GetResourceUserData

Get user data attached to resource.

```cpp
void *zest_GetResourceUserData(zest_resource_node resource_node);
```

---

### zest_SetResourceUserData

Attach user data to resource.

```cpp
void zest_SetResourceUserData(zest_resource_node resource_node, void *user_data);
```

---

### zest_SetResourceBufferProvider

Change buffer access type.

```cpp
void zest_SetResourceBufferProvider(
    zest_resource_node resource_node,
    zest_resource_buffer_provider buffer_provider
);
```

---

### zest_SetResourceImageProvider

Change image access type.

```cpp
void zest_SetResourceImageProvider(
    zest_resource_node resource_node,
    zest_resource_image_provider image_provider
);
```

---

## Pass Resource Access

### zest_GetPassInputResource

Get an input resource by name within a pass callback.

```cpp
zest_resource_node zest_GetPassInputResource(
    const zest_command_list command_list,
    const char *name
);
```

---

### zest_GetPassOutputResource

Get an output resource by name within a pass callback.

```cpp
zest_resource_node zest_GetPassOutputResource(
    const zest_command_list command_list,
    const char *name
);
```

---

### zest_GetPassInputBuffer

Get an input buffer by name within a pass callback.

```cpp
zest_buffer zest_GetPassInputBuffer(
    const zest_command_list command_list,
    const char *name
);
```

---

### zest_GetPassOutputBuffer

Get an output buffer by name within a pass callback.

```cpp
zest_buffer zest_GetPassOutputBuffer(
    const zest_command_list command_list,
    const char *name
);
```

---

## Bindless Descriptor Helpers

### zest_GetTransientSampledImageBindlessIndex

Get bindless index for a transient sampled image.

```cpp
zest_uint zest_GetTransientSampledImageBindlessIndex(
    const zest_command_list command_list,
    zest_resource_node resource,
    zest_binding_number_type binding_number
);
```

---

### zest_GetTransientSampledMipBindlessIndexes

Get bindless indices for all mip levels.

```cpp
zest_uint *zest_GetTransientSampledMipBindlessIndexes(
    const zest_command_list command_list,
    zest_resource_node resource,
    zest_binding_number_type binding_number
);
```

Returns array of indices, one per mip level.

---

### zest_GetTransientBufferBindlessIndex

Get bindless index for a transient buffer.

```cpp
zest_uint zest_GetTransientBufferBindlessIndex(
    const zest_command_list command_list,
    zest_resource_node resource
);
```

---

## Resource Groups

### zest_CreateResourceGroup

Create an empty resource group.

```cpp
zest_resource_group zest_CreateResourceGroup(void);
```

---

### zest_AddResourceToGroup

Add a resource to a group.

```cpp
void zest_AddResourceToGroup(zest_resource_group group, zest_resource_node image);
```

---

### zest_AddSwapchainToGroup

Add the swapchain to a group.

```cpp
void zest_AddSwapchainToGroup(zest_resource_group group);
```

---

## Special Flags

### zest_FlagResourceAsEssential

Mark a resource to prevent pass culling.

```cpp
void zest_FlagResourceAsEssential(zest_resource_node resource);
```

---

### zest_DoNotCull

Prevent the current pass from being culled.

```cpp
void zest_DoNotCull(void);
```

---

## Execution

### zest_QueueFrameGraphForExecution

Queue a frame graph to execute during EndFrame.

```cpp
void zest_QueueFrameGraphForExecution(
    zest_context context,
    zest_frame_graph frame_graph
);
```

---

### zest_WaitForSignal

Wait for a timeline semaphore signal.

```cpp
zest_semaphore_status zest_WaitForSignal(
    zest_execution_timeline timeline,
    zest_microsecs timeout
);
```

| Parameter | Description |
|-----------|-------------|
| `timeline` | The execution timeline to wait on |
| `timeout` | Timeout in microseconds |

Returns `zest_semaphore_status_success`, `zest_semaphore_status_timeout`, or `zest_semaphore_status_error`.

---

## Debugging

### zest_PrintCompiledFrameGraph

Print frame graph structure to console.

```cpp
void zest_PrintCompiledFrameGraph(zest_frame_graph frame_graph);
```

---

### zest_GetFrameGraphResult

Get compilation result status.

```cpp
zest_frame_graph_result zest_GetFrameGraphResult(zest_frame_graph frame_graph);
```

---

## See Also

- [Frame Graph Concepts](../concepts/frame-graph/index.md)
- [Passes](../concepts/frame-graph/passes.md)
- [Resources](../concepts/frame-graph/resources.md)
- [Execution](../concepts/frame-graph/execution.md)
- [Debugging](../concepts/frame-graph/debugging.md)
- [Multi-Pass Tutorial](../tutorials/06-multipass.md)
