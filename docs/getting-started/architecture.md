# Architecture Overview

Zest is built around two core objects and one execution model. Understanding these is key to using the library effectively.

## The Two Core Objects

### Device (`zest_device`)

The **device** is a singleton that represents your GPU and manages global resources:

```cpp
zest_device device = zest_implglfw_CreateVulkanDevice(false);
```

**What it owns:**

- Vulkan instance and physical device
- Shader library (compiled shaders)
- Pipeline template cache
- Bindless descriptor sets
- GPU memory pools (buffers, images)
- Sampler cache

**Lifecycle:** Create once at startup, destroy at shutdown. All contexts share the same device.

### Context (`zest_context`)

The **context** represents a render target (window) and manages per-frame resources:

```cpp
zest_context context = zest_CreateContext(device, &window, &create_info);
```

**What it owns:**

- Swapchain and presentation
- Frame synchronization (semaphores, fences)
- Command buffer pools
- Frame graph cache
- Linear allocators for frame-lifetime data

**Lifecycle:** One per window. Can have multiple contexts sharing one device.

```
┌─────────────────────────────────────────────────┐
│                    Device                        │
│  - Vulkan instance                              │
│  - Shader library                               │
│  - Bindless descriptors                         │
│  - Memory pools                                 │
├─────────────────────────────────────────────────┤
│         │                       │               │
│    ┌────▼────┐             ┌────▼────┐          │
│    │ Context │             │ Context │          │
│    │ (Win 1) │             │ (Win 2) │          │
│    └─────────┘             └─────────┘          │
└─────────────────────────────────────────────────┘
```

## The Frame Graph

The **frame graph** is Zest's execution model. Instead of manually managing barriers, semaphores, and resource states, you declare what resources each pass reads and writes.

### Why Frame Graphs?

Traditional Vulkan requires:

- Manual barrier insertion
- Explicit semaphore management
- Resource state tracking
- Render pass object creation

Frame graphs handle all of this automatically by analyzing resource dependencies.

### Building a Frame Graph

```cpp
if (zest_BeginFrameGraph(context, "My Graph", &cache_key)) {
    // 1. Import external resources
    zest_ImportSwapchainResource();

    // 2. Define passes
    zest_BeginRenderPass("Pass Name"); {
        zest_ConnectInput(some_resource);    // Read from resource
        zest_ConnectOutput(other_resource);  // Write to resource
        zest_SetPassTask(MyCallback, data);  // Render callback
        zest_EndPass();
    }

    // 3. Compile
    frame_graph = zest_EndFrameGraph();
}
```

### What Happens at Compile Time

When you call `zest_EndFrameGraph()`, the compiler:

1. **Builds the dependency graph** - Analyzes which passes depend on which resources
2. **Inserts barriers** - Adds pipeline barriers for resource transitions
3. **Culls unused passes** - Removes passes that don't contribute to final output
4. **Handles synchronization** - Sets up semaphores between queues
5. **Creates transient resources** - Allocates temporary images/buffers

### Pass Types

| Pass Type | Purpose | Example |
|-----------|---------|---------|
| Render Pass | Graphics pipeline, drawing | Scene rendering, UI |
| Compute Pass | Compute shaders | Particle simulation, post-processing |
| Transfer Pass | Data uploads | Staging buffer copies |

```cpp
zest_BeginRenderPass("Draw Scene");   // Graphics
zest_BeginComputePass(compute, "Sim"); // Compute
zest_BeginTransferPass("Upload");      // Transfer
```

### Frame Graph Caching

Compiling frame graphs has CPU cost. Cache them when the structure doesn't change:

```cpp
// Generate cache key (can include custom data)
zest_frame_graph_cache_key_t key = zest_InitialiseCacheKey(context, custom_data, size);

// Try to get cached graph
zest_frame_graph graph = zest_GetCachedFrameGraph(context, &key);

// Only build if not cached
if (!graph) {
    if (zest_BeginFrameGraph(context, "Graph", &key)) {
        // ... build graph ...
        graph = zest_EndFrameGraph();
    }
}
```

## Bindless Descriptors

Zest uses a **bindless descriptor** model. Instead of creating and binding descriptor sets per object, all resources are indexed into global arrays.

```cpp
// Acquire an index when creating a texture
zest_uint tex_index = zest_AcquireSampledImageIndex(device, image, zest_texture_2d_binding);

// Pass the index to shaders via push constants
push_data.texture_index = tex_index;
zest_cmd_SendPushConstants(cmd, &push_data, sizeof(push_data));
```

In shaders:

```glsl
layout(set = 0, binding = 0) uniform sampler2D textures[];

void main() {
    vec4 color = texture(textures[push.texture_index], uv);
}
```

Benefits:

- Bind descriptor sets once per frame
- No descriptor set management per object
- Unlimited textures (within GPU limits)
- Simpler shader code

## Memory Management

Zest uses a **TLSF allocator** for both CPU and GPU memory:

- **Minimal fragmentation** - Two-level segregated fit algorithm
- **O(1) allocation** - Constant time allocate and free
- **Auto-expanding pools** - Pools grow when needed

### Pool Types

| Pool | Purpose | Typical Size |
|------|---------|--------------|
| Device Buffer Pool | GPU-only buffers | 64 MB |
| Staging Buffer Pool | CPU-visible uploads | 32 MB |
| Image Memory Pool | Textures | 256 MB |

```cpp
// Configure pool sizes before creating device
zest_SetDevicePoolSize(device, 128 * 1024 * 1024);  // 128 MB
```

## Typical Frame Structure

```cpp
while (running) {
    zest_UpdateDevice(device);          // 1. Update device state
    HandleInput();                       // 2. Process input

    if (zest_BeginFrame(context)) {      // 3. Begin frame
        // Update uniforms, instance data, etc.
        UpdateGameState();

        // Get or build frame graph
        zest_frame_graph graph = GetOrBuildFrameGraph();

        // Queue for execution
        zest_QueueFrameGraphForExecution(context, graph);
        zest_EndFrame(context);          // 4. Present
    }
}
```

## Next Steps

- [Frame Graph Concept](../concepts/frame-graph/index.md) - Deep dive into frame graphs
- [Device & Context Concept](../concepts/device-and-context.md) - Detailed API reference
- [Minimal Template Tutorial](../tutorials/01-minimal-template.md) - Annotated walkthrough
