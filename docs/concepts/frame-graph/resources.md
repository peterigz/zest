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
zest_ImportSwapchainResource();
```

This returns the current frame's swapchain image as a resource node (otional, you don't need to do anything with the returned resource as it's registered in the frame graph internally).

### Images

Import existing images (textures, render targets):

```cpp
zest_image image = zest_GetImage(image_handle);
zest_resource_node tex = zest_ImportImageResource("Texture", image, zest_texture_2d_binding);
```
> **You only need to import images that either require a layout transition before reading from or intend on writing to the image. It's recommended that if you only want to sample from an image and it's already in layout shader read only optimal then there's no need to import. You can just simply pass on it's descriptor array index in a push constant or uniform buffer and sample from it in a shader.**

The `provider` parameter specifies a callback that get's the correct image view to use specifically when the frame graph is cached. For example the swapchain uses a provider to get the correct image frame that was acquired. In most cases you can just pass NULL or 0 as the image will just have it's one default view.

### Buffers

Import existing buffers:

```cpp
zest_buffer buffer = zest_GetBuffer(buffer_handle);
zest_resource_node buf = zest_ImportBufferResource("Instances", buffer, provider);
```

> **You only need to import buffers that you might write to and be a part of dependency chains in the graph. If your buffers will only be read from in the graph then there's no need to import, you can just access them in shaders via their descriptor array index.

The `provider` parameter specifies a callback so that a cached frame graph can get the relevent resource - for example the correct frame in flight buffer.

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
| `usage_hint` | How the image will be used to give the frame graph hint to set the correct usage flags |
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

For advanced use: the `prev_fif` parameter controls which frame-in-flight's buffer is used:
- `zest_false` - Current frame's buffer
- `zest_true` - Previous frame's buffer (for temporal effects)

## Resource Properties

It can be useful to access resource properties inside pass callback functions.

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

The info struct contains the following data:

| Field | Description |
|-------|-------------|
| `extent` | The image dimensions, width, height and depth |
| `mip_levels` | The number of mip levels in the image |
| `layer_count` | The number of image array layers if the image is a texture array |
| `format` | The image format (zest_format) |
| `aspect_flags` | The aspect flag (color, depth or stencil) |
| `sample_count` | The sample count |
| `flags` | The flags that describe how the image is used |
| `layout` | The image layout (general, readonly, attachment etc.) |


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

Dynamically resize a buffer resource. This can be called inside a resource buffer provider function before the transient buffer is created if won't know the size until frame execution.

```cpp
zest_SetResourceBufferSize(buffer_resource, new_size);
```

### User Data

Attach custom data to resources that you can retrieve inside a pass callback function.

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
zest_SetResourceImageProvider(resource, callback);

// Change buffer binding type
zest_SetResourceBufferProvider(resource, callback);
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
zest_resource_group scene_output = zest_CreateResourceGroup();

// Add resources
zest_AddResourceToGroup(gbuffer, albedo);
zest_AddResourceToGroup(gbuffer, normal);
zest_AddResourceToGroup(gbuffer, depth);

// Add swapchain to a group
zest_AddSwapchainToGroup(scene_output);
zest_AddResourceToGroup(scene_output, depth);

// Use in passese
zest_BeginRenderPass("G-Buffer"); {
    zest_ConnectOutputGroup(gbuffer);
    zest_SetPassTask(RenderGBuffer, app);
    zest_EndPass();
}

zest_BeginRenderPass("Scene"); {
    zest_ConnectInputGroup(gbuffer);
	zest_ConnectOuputGroup(scene_output);
    zest_SetPassTask(RenderScene, app);
    zest_EndPass();
}
```

## Bindless Descriptor Access

Get bindless descriptor indices for transient resources:

> **Note: transient descriptor array indexes are automatically released after use.**

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
2. **Import persistent resources only if used as output** - Textures, meshes, and long-lived data that you need to write too or transition.
3. **Group related resources** - Simplifies MRT and G-buffer setups
4. **Set clear colors explicitly** - Avoids undefined initial values
5. **Use bindless helpers** - Simplifies shader resource access

## See Also

- [Passes](passes.md) - Pass types and callbacks
- [Execution](execution.md) - Building and executing frame graphs
- [Bindless Descriptors](../bindless.md) - Global descriptor system
- [Frame Graph API](../../api-reference/frame-graph.md) - Function reference
