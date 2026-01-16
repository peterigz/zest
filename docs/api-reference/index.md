# API Reference

Complete reference for Zest's public functions organized by category. Zest provides a lightweight C API for GPU rendering with automatic resource management, frame graph compilation, and bindless descriptor support. The core API is graphics-API agnostic, with platform layers providing the backend implementation (currently Vulkan, with DirectX/Metal/WebGPU possible in the future).

## API Conventions

### Naming Patterns

Zest uses consistent naming conventions to make the API predictable and discoverable:

| Pattern | Meaning | Example |
|---------|---------|---------|
| `zest_Create*` | Create and return a new resource | `zest_CreateBuffer`, `zest_CreateImage` |
| `zest_Get*` | Retrieve an existing resource or property | `zest_GetImage`, `zest_GetShader` |
| `zest_Set*` | Configure a property on a resource | `zest_SetPipelineBlend`, `zest_SetPipelineTopology` |
| `zest_Free*` | Release a resource and its memory | `zest_FreeBuffer`, `zest_FreeShader` |
| `zest_Begin*` | Start a scope, pass, or builder pattern | `zest_BeginFrameGraph`, `zest_BeginRenderPass` |
| `zest_End*` | End a scope, pass, or builder pattern | `zest_EndFrameGraph`, `zest_EndPass` |
| `zest_cmd_*` | Command recorded into frame graph | `zest_cmd_Draw`, `zest_cmd_BindPipeline` |
| `zest_imm_*` | Immediate/one-off command (outside frame graph) | `zest_imm_CopyBuffer` |
| `zest_impl*_` | Platform-specific implementation | `zest_implglfw_CreateVulkanDevice` |

### Function Markers

Functions in the header are marked with visibility indicators:

- `ZEST_API` - Public API function, safe to use in your application
- `ZEST_PRIVATE` - Internal function, may change without notice (prefixed with `zest__`)

### Handle vs Object

Many resources use a two-tier system for safe resource management:

```cpp
// Handles are lightweight identifiers (safe to copy, store, pass around)
zest_image_handle handle = zest_CreateImage(device, &info);

// Objects are pointers to the actual data which you can use to pass in to various functions that use the pointers.
zest_image image = zest_GetImage(handle);
```

The reason why you have to get the object pointer to pass in to functions is just to reduce the overhead of having to fetch the resource from the store. Whilst it's still quick to do so there is still some overhead of verifying the validity of the resource, so instead you can get the point and then use that. Typically you want the pointer lifetime to be just the current scope of where you're using it.

Handles provide an extra layer of indirection that allows Zest to manage resource lifetimes, detect stale references, and support hot-reloading.

The object pointers are opaque and just designed to work with functions to make specific changes in a safe way. It also means that changes can be made to the underlying struct and not break code that may have accessed struct members directly.

### Type Conventions

| Suffix | Meaning | Example |
|--------|---------|---------|
| `_t` | Struct type | `zest_timer_t`, `zest_buffer_info_t` |
| `_handle` | Opaque handle type | `zest_image_handle`, `zest_shader_handle` |
| (none) | Pointer to struct | `zest_image`, `zest_buffer` |

---

## Categories

### Core

These APIs form the foundation of every Zest application:

| Category | Description |
|----------|-------------|
| [Device API](device.md) | Device creation, configuration, and lifecycle. The device manages GPU resources, shader library, and pipeline templates. |
| [Context API](context.md) | Window/swapchain management. Each context represents a render target (window or offscreen) with its own frame resources. |
| [Frame Graph API](frame-graph.md) | Declarative render pass building and automatic execution. The frame graph compiler handles barriers, synchronization, and resource transitions. |

### Resources

APIs for creating and managing GPU resources:

| Category | Description |
|----------|-------------|
| [Buffer API](buffer.md) | Vertex buffers, index buffers, uniform buffers, and storage buffers. Includes staging and upload utilities. |
| [Image API](image.md) | Textures, render targets, depth buffers, and samplers. Supports mipmapping, cubemaps, and array textures. |
| [Pipeline API](pipeline.md) | Pipeline templates for graphics and compute. Configure shaders, vertex input, rasterization, depth testing, and blending. |

### Rendering

APIs for recording draw commands and managing render state:

| Category | Description |
|----------|-------------|
| [Layer API](layer.md) | High-level rendering abstraction for instanced sprites, meshes, and text. Automatic batching and buffer management. |
| [Compute API](compute.md) | Compute shader dispatch and synchronization. |
| [Command API](commands.md) | Low-level `zest_cmd_*` functions for recording draw calls, binding resources, and setting state within frame graph passes. |
| [Immediate API](immediate.md) | `zest_imm_*` functions for one-off operations outside the frame graph (uploads, copies, etc.). |

### Utilities

Helper APIs for common tasks that you can take or leave, these are added to help with example writing but you can make use of them as well.

| Category | Description |
|----------|-------------|
| [Math API](math.md) | Vector and matrix operations, interpolation, packing functions, and angle conversion. |
| [Camera API](camera.md) | First-person and orbit camera utilities with view/projection matrix generation. |
| [Timer API](timer.md) | Fixed timestep game loop utilities with accumulator-based timing and interpolation support. |

---

## Quick Reference

### Application Setup

```cpp
// Create device using GLFW as a basis for window creation. (initializes graphics backend, creates logical device)
zest_device device = zest_implglfw_CreateVulkanDevice(ZEST_FALSE);  // ZEST_TRUE for validation

// Create context (window and swapchain)
zest_create_context_info_t context_info = zest_CreateContextInfo();
zest_context context = zest_CreateContext(device, &window_data, &context_info);
```

### Main Loop

```cpp
while (running) {
    // Poll events, update input, etc.

    // Update device (handles window resize, resource cleanup)
    zest_UpdateDevice(device);

    // Begin frame (acquires swapchain image)
    if (zest_BeginFrame(context)) {
        // Build or retrieve frame graph
        zest_BeginFrameGraph(context, "main", NULL);
        // ... define passes ...
        zest_frame_graph graph = zest_EndFrameGraph();

        // Queue for execution
        zest_QueueFrameGraphForExecution(context, graph);

        // End frame (submits commands, presents)
        zest_EndFrame(context);
    }
}

// Cleanup
zest_DestroyDevice(device);
```

### Common Render Commands

These commands are used inside frame graph pass callbacks:

```cpp
void my_render_callback(zest_command_list cmd, void *user_data) {
    // Bind pipeline
    zest_pipeline pipeline = zest_GetPipeline(my_pipeline_template, cmd);
    zest_cmd_BindPipeline(cmd, pipeline);

    // Set viewport (screen-sized with default depth range)
    zest_cmd_SetScreenSizedViewport(cmd, 0.0f, 1.0f);

    // Bind vertex/index buffers
    zest_cmd_BindVertexBuffer(cmd, 0, 1, vertex_buffer);  // binding 0, count 1
    zest_cmd_BindIndexBuffer(cmd, index_buffer);

    // Send push constants
    zest_cmd_SendPushConstants(cmd, &push_data, sizeof(push_data));

    // Draw
    zest_cmd_Draw(cmd, vertex_count, instance_count, first_vertex, first_instance);
    zest_cmd_DrawIndexed(cmd, index_count, instance_count, first_index, vertex_offset, first_instance);
}
```

### Resource Creation

```cpp
// Buffer
zest_buffer_info_t buf_info = zest_CreateBufferInfo(zest_buffer_type_vertex, zest_memory_usage_gpu);
zest_buffer buffer = zest_CreateBuffer(device, size, &buf_info);

// Image (texture with mipmaps)
zest_image_info_t img_info = zest_CreateImageInfo(width, height);
img_info.flags = zest_image_preset_texture_mipmaps;
zest_image_handle image = zest_CreateImage(device, &img_info);

// Pipeline
zest_pipeline_template pipeline = zest_CreatePipelineTemplate(device, "my_pipeline");
zest_SetPipelineShaders(pipeline, vert_shader, frag_shader);
zest_SetPipelineTopology(pipeline, zest_topology_triangle_list);
zest_SetPipelineCullMode(pipeline, zest_cull_mode_back);
zest_SetPipelineDepthTest(pipeline, ZEST_TRUE, ZEST_TRUE);
zest_SetPipelineBlend(pipeline, zest_BlendStateNone());
```

### Image Presets

Common image flag combinations for typical use cases:

| Preset | Use Case |
|--------|----------|
| `zest_image_preset_texture` | Basic sampled texture |
| `zest_image_preset_texture_mipmaps` | Texture with auto-generated mipmaps |
| `zest_image_preset_color_attachment` | Render target (color) |
| `zest_image_preset_depth_attachment` | Render target (depth/stencil) |
| `zest_image_preset_storage` | Compute shader read/write |
| `zest_image_preset_storage_cubemap` | Cubemap for compute |

---

## Source Files

The API is defined across these header files:

| File | Contents |
|------|----------|
| `zest.h` | Main API - device, context, frame graph, resources, commands, math, timer |
| `zest_vulkan.h` | Vulkan platform layer (current default backend) |
| `zest_utilities.h` | Optional helpers - image loading (stb, ktx), font loading with msdf generator, model loading with gltf |

Future platform layers (e.g., `zest_dx12.h`, `zest_metal.h`) would follow the same pattern.

### Include Pattern

Copy zest.h and zest_vulkan.h (in future other platforms you want to support), and if you want to use it zest_utilities.h into your project include folder.

```cpp
// In exactly ONE .cpp file:
#define ZEST_IMPLEMENTATION
#define ZEST_VULKAN_IMPLEMENTATION
#include <zest.h>

// In other files, just include the header:
#include <zest.h>
```

---

## Error Handling

Zest uses a combination of return values and assertions:

- Functions that can fail return `zest_bool` or a handle (check for `handle.value == 0` for null handle return)
- Debug builds include assertions for invalid parameters
- Enable validation/debug layers during development: `zest_implglfw_CreateVulkanDevice(ZEST_TRUE)`

```cpp
zest_image_handle image = zest_CreateImage(device, &info);
if (image.value == 0) {
    // Handle creation failure
}
```

---

## Thread Safety

Still largely untested but:

- **Device creation/destruction**: Main thread only
- **Resource creation**: Generally main thread, some resources support background loading
- **Frame graph building**: Single thread per context
- **Command recording**: Thread-safe within a pass (different command lists)

For multi-threaded rendering, use separate command lists and synchronize at pass boundaries.
