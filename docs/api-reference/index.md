# API Reference

Complete reference for Zest's ~500+ public functions organized by category.

## API Conventions

### Naming Patterns

| Pattern | Meaning | Example |
|---------|---------|---------|
| `zest_Create*` | Create a new resource | `zest_CreateBuffer` |
| `zest_Get*` | Retrieve existing resource | `zest_GetImage` |
| `zest_Set*` | Configure a property | `zest_SetPipelineBlend` |
| `zest_Free*` | Release a resource | `zest_FreeBuffer` |
| `zest_Begin*` | Start a scope/builder | `zest_BeginFrameGraph` |
| `zest_End*` | End a scope/builder | `zest_EndFrameGraph` |
| `zest_cmd_*` | Frame graph command | `zest_cmd_Draw` |
| `zest_imm_*` | Immediate command | `zest_imm_CopyBuffer` |

### Function Markers

- `ZEST_API` - Public API function
- `ZEST_PRIVATE` - Internal function (don't use directly)

### Handle vs Object

Many resources have both a handle and object type:

```cpp
zest_image_handle handle = zest_CreateImage(...);  // Opaque handle
zest_image image = zest_GetImage(handle);          // Actual object pointer
```

## Categories

### Core

| Category | Functions | Description |
|----------|-----------|-------------|
| [Device API](device.md) | ~30 | Device creation and management |
| [Context API](context.md) | ~20 | Window/swapchain management |
| [Frame Graph API](frame-graph.md) | ~60 | Pass building and execution |

### Resources

| Category | Functions | Description |
|----------|-----------|-------------|
| [Buffer API](buffer.md) | ~40 | Vertex, index, uniform buffers |
| [Image API](image.md) | ~50 | Textures, render targets, samplers |
| [Pipeline API](pipeline.md) | ~50 | Pipeline templates and state |

### Rendering

| Category | Functions | Description |
|----------|-----------|-------------|
| [Layer API](layer.md) | ~60 | Instance, mesh, font layers |
| [Compute API](compute.md) | ~10 | Compute shader support |
| [Command API](commands.md) | ~30 | `zest_cmd_*` render commands |
| [Immediate API](immediate.md) | ~20 | `zest_imm_*` one-off commands |

### Utilities

| Category | Functions | Description |
|----------|-----------|-------------|
| [Math API](math.md) | ~80 | Vector, matrix operations |
| [Camera API](camera.md) | ~20 | Camera utilities |
| [Timer API](timer.md) | ~15 | Timing and fixed timestep |

## Quick Reference

### Essential Functions

```cpp
// Setup
zest_device device = zest_implglfw_CreateDevice(false);
zest_context context = zest_CreateContext(device, &window, &info);

// Frame loop
zest_UpdateDevice(device);
if (zest_BeginFrame(context)) {
    // Build/get frame graph
    zest_QueueFrameGraphForExecution(context, graph);
    zest_EndFrame(context);
}

// Cleanup
zest_DestroyDevice(device);
```

### Common Render Commands

```cpp
zest_cmd_BindPipeline(cmd, pipeline);
zest_cmd_SetScreenSizedViewport(cmd);
zest_cmd_BindVertexBuffer(cmd, buffer, offset);
zest_cmd_BindIndexBuffer(cmd, buffer, offset, type);
zest_cmd_SendPushConstants(cmd, data, size);
zest_cmd_Draw(cmd, vertex_count, instance_count, first_vertex, first_instance);
zest_cmd_DrawIndexed(cmd, index_count, instance_count, first_index, vertex_offset, first_instance);
```

### Resource Creation

```cpp
// Buffer
zest_buffer_info_t buf_info = zest_CreateBufferInfo(type, memory_usage);
zest_buffer buffer = zest_CreateBuffer(device, size, &buf_info);

// Image
zest_image_info_t img_info = zest_CreateImageInfo(width, height);
img_info.flags = zest_image_preset_texture_mipmaps;
zest_image_handle image = zest_CreateImage(device, &img_info);

// Pipeline
zest_pipeline_template_handle pipeline = zest_BeginPipelineTemplate(device, "name");
zest_SetPipelineShaders(pipeline, vert, frag);
// ... configure ...
```

## Source Files

The API is defined in these header files:

| File | Contents |
|------|----------|
| `zest.h` | Main API (~535 functions) |
| `zest_vulkan.h` | Vulkan platform layer |
| `zest_utilities.h` | Optional helpers (~81 functions) |
