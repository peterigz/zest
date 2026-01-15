# Resources

Resources are the data that flows through a frame graph. This page covers importing external resources, creating transient resources, and managing resource properties.

## Resource Types

| Type | Lifetime | Memory | Use Case |
|------|----------|--------|----------|
| **Swapchain** | Per-frame | Managed by presentation engine | Final output |
| **Imported** | Application-controlled | Persistent | Textures, meshes, external buffers |
| **Transient** | Single frame | Pooled/aliased | Intermediate render targets |

## Importing External Resources

### Swapchain

The swapchain is always imported first:

```cpp
zest_resource_node swapchain = zest_ImportSwapchainResource();
```

This returns the current frame's swapchain image as a resource node.

### Images

Import existing images (textures, render targets):

```cpp
zest_image image = zest_GetImage(image_handle);
zest_resource_node tex = zest_ImportImageResource("Texture", image, zest_texture_2d_binding);
```

The `provider` parameter specifies how the image will be accessed:
- `zest_texture_2d_binding` - 2D texture sampling
- `zest_texture_cube_binding` - Cubemap sampling
- `zest_storage_image_binding` - Storage image read/write

### Buffers

Import existing buffers:

```cpp
zest_buffer buffer = zest_GetBuffer(buffer_handle);
zest_resource_node buf = zest_ImportBufferResource("Instances", buffer, zest_storage_buffer_binding);
```

The `provider` parameter specifies buffer access type:
- `zest_storage_buffer_binding` - Storage buffer
- `zest_uniform_buffer_binding` - Uniform buffer

## Transient Resources

Transient resources exist only for a single frame. They are allocated from a pool and can share memory with non-overlapping resources.

### Transient Images

```cpp
zest_image_resource_info_t info = {
    .format = zest_format_r16g16b16a16_sfloat,
    .usage_hint = zest_resource_usage_hint_copyable,
    .width = 1920,
    .height = 1080,
    .mip_levels = 1
};
zest_resource_node hdr_target = zest_AddTransientImageResource("HDR", &info);
```

**Image Resource Info Fields:**

| Field | Description |
|-------|-------------|
| `format` | Image format (e.g., `zest_format_r8g8b8a8_unorm`) |
| `usage_hint` | How the image will be used |
| `width`, `height` | Dimensions in pixels |
| `mip_levels` | Number of mip levels (1 = no mipmaps) |

### Transient Buffers

```cpp
zest_buffer_resource_info_t info = {
    .size = sizeof(particle_t) * MAX_PARTICLES,
    .usage_hint = zest_resource_usage_hint_vertex_storage
};
zest_resource_node particles = zest_AddTransientBufferResource("Particles", &info);
```

**Buffer Resource Info Fields:**

| Field | Description |
|-------|-------------|
| `size` | Buffer size in bytes |
| `usage_hint` | How the buffer will be used |

### Transient Layers

For layer-based rendering with previous frame access:

```cpp
zest_resource_node layer_node = zest_AddTransientLayerResource("Sprites", sprite_layer, zest_false);
```

The `prev_fif` parameter controls which frame-in-flight's buffer is used:
- `zest_false` - Current frame's buffer
- `zest_true` - Previous frame's buffer (for temporal effects)

## Resource Properties

### Dimensions and Format

Query resource dimensions:

```cpp
zest_uint width = zest_GetResourceWidth(resource);
zest_uint height = zest_GetResourceHeight(resource);
zest_uint mips = zest_GetResourceMipLevels(resource);
```

Get full image description:

```cpp
zest_image_info_t info = zest_GetResourceImageDescription(resource);
```

### Resource Type

```cpp
zest_resource_type type = zest_GetResourceType(resource);
// Returns: zest_resource_type_image, zest_resource_type_buffer, etc.
```

### Clear Color

Set the clear color for render target resources:

```cpp
zest_SetResourceClearColor(color_target, 0.1f, 0.1f, 0.1f, 1.0f);
```

### Buffer Size

Dynamically resize a buffer resource:

```cpp
zest_SetResourceBufferSize(buffer_resource, new_size);
```

### User Data

Attach custom data to resources:

```cpp
// Store
zest_SetResourceUserData(resource, my_data);

// Retrieve
void* data = zest_GetResourceUserData(resource);
```

### Providers

Change how a resource is accessed:

```cpp
// Change image binding type
zest_SetResourceImageProvider(resource, zest_storage_image_binding);

// Change buffer binding type
zest_SetResourceBufferProvider(resource, zest_uniform_buffer_binding);
```

### Getting the Underlying Image

```cpp
zest_image image = zest_GetResourceImage(resource);
```

## Accessing Resources in Passes

Within a pass callback, retrieve resources by name:

```cpp
void MyCallback(const zest_command_list cmd, void* user_data) {
    // Get resource nodes
    zest_resource_node input = zest_GetPassInputResource(cmd, "ShadowMap");
    zest_resource_node output = zest_GetPassOutputResource(cmd, "HDR");

    // Get buffers directly
    zest_buffer in_buf = zest_GetPassInputBuffer(cmd, "Particles");
    zest_buffer out_buf = zest_GetPassOutputBuffer(cmd, "Results");
}
```

## Resource Groups

Group multiple resources for convenience:

```cpp
// Create group
zest_resource_group gbuffer = zest_CreateResourceGroup();

// Add resources
zest_AddResourceToGroup(gbuffer, albedo);
zest_AddResourceToGroup(gbuffer, normal);
zest_AddResourceToGroup(gbuffer, depth);

// Add swapchain to a group
zest_AddSwapchainToGroup(output_group);

// Use in pass
zest_BeginRenderPass("G-Buffer"); {
    zest_ConnectOutputGroup(gbuffer);
    zest_SetPassTask(RenderGBuffer, app);
    zest_EndPass();
}
```

## Bindless Descriptor Access

Get bindless descriptor indices for transient resources:

### Sampled Images

```cpp
void MyCallback(const zest_command_list cmd, void* user_data) {
    // Get single bindless index
    zest_uint index = zest_GetTransientSampledImageBindlessIndex(
        cmd, resource, binding_number);

    // Get indices for all mip levels
    zest_uint* mip_indices = zest_GetTransientSampledMipBindlessIndexes(
        cmd, resource, binding_number);
}
```

### Storage Buffers

```cpp
void MyCallback(const zest_command_list cmd, void* user_data) {
    zest_uint index = zest_GetTransientBufferBindlessIndex(cmd, resource);

    // Use index in push constants for shader access
    push_constants.buffer_index = index;
}
```

## Essential Resources

Mark resources that should prevent pass culling:

```cpp
zest_FlagResourceAsEssential(debug_output);
```

Essential resources ensure their producing passes are not culled, even without a path to the swapchain.

## Complete Example: G-Buffer Setup

```cpp
if (zest_BeginFrameGraph(context, "Deferred", &cache_key)) {
    zest_ImportSwapchainResource();

    // Transient G-buffer images
    zest_image_resource_info_t color_info = {
        .format = zest_format_r8g8b8a8_unorm,
        .width = width, .height = height, .mip_levels = 1
    };
    zest_image_resource_info_t depth_info = {
        .format = zest_format_d32_sfloat,
        .width = width, .height = height, .mip_levels = 1
    };

    zest_resource_node albedo = zest_AddTransientImageResource("Albedo", &color_info);
    zest_resource_node normal = zest_AddTransientImageResource("Normal", &color_info);
    zest_resource_node depth = zest_AddTransientImageResource("Depth", &depth_info);

    // Group for convenience
    zest_resource_group gbuffer = zest_CreateResourceGroup();
    zest_AddResourceToGroup(gbuffer, albedo);
    zest_AddResourceToGroup(gbuffer, normal);
    zest_AddResourceToGroup(gbuffer, depth);

    // G-buffer pass
    zest_BeginRenderPass("G-Buffer"); {
        zest_ConnectOutputGroup(gbuffer);
        zest_SetPassTask(RenderGBuffer, app);
        zest_EndPass();
    }

    // Lighting pass reads G-buffer
    zest_BeginRenderPass("Lighting"); {
        zest_ConnectInput(albedo);
        zest_ConnectInput(normal);
        zest_ConnectInput(depth);
        zest_ConnectSwapChainOutput();
        zest_SetPassTask(DeferredLighting, app);
        zest_EndPass();
    }

    frame_graph = zest_EndFrameGraph();
}
```

## Best Practices

1. **Use transient resources for intermediates** - They benefit from memory aliasing
2. **Import persistent resources** - Textures, meshes, and long-lived data
3. **Group related resources** - Simplifies MRT and G-buffer setups
4. **Set clear colors explicitly** - Avoids undefined initial values
5. **Use bindless helpers** - Simplifies shader resource access

## See Also

- [Passes](passes.md) - Pass types and callbacks
- [Execution](execution.md) - Building and executing frame graphs
- [Bindless Descriptors](../bindless.md) - Global descriptor system
- [Frame Graph API](../../api-reference/frame-graph.md) - Function reference
