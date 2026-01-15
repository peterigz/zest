# Immediate API

One-off GPU commands outside frame graphs (`zest_imm_*` functions). Use these for initialization, resource uploads, and other operations that don't fit within the frame graph model.

## Command Buffer

### zest_imm_BeginCommandBuffer

Begin immediate command recording. Acquires a command buffer from the specified queue for one-time submission.

```cpp
zest_queue zest_imm_BeginCommandBuffer(zest_device device, zest_device_queue_type target_queue);
```

**Parameters:**
- `device` - The Zest device handle
- `target_queue` - Queue type: `zest_queue_graphics`, `zest_queue_transfer`, or `zest_queue_compute`

**Returns:** A queue handle to use with subsequent immediate commands.

**Usage:** Call this at the start of any immediate command sequence. Choose the appropriate queue type for your operation - transfer for copies, compute for compute dispatches, graphics for operations requiring graphics capabilities.

---

### zest_imm_EndCommandBuffer

Submit the command buffer and wait for completion (blocking). The queue is released immediately after.

```cpp
zest_bool zest_imm_EndCommandBuffer(zest_queue queue);
```

**Parameters:**
- `queue` - The queue handle from `zest_imm_BeginCommandBuffer`

**Returns:** `ZEST_TRUE` on success.

**Usage:** Always call this to finalize and execute immediate commands. This is a blocking call that waits for GPU completion before returning.

---

## Buffer Operations

### zest_imm_CopyBuffer

Copy data from one buffer to another. Typically used to upload data from a staging buffer to GPU-local memory.

```cpp
zest_bool zest_imm_CopyBuffer(
    zest_queue queue,
    zest_buffer src_buffer,
    zest_buffer dst_buffer,
    zest_size size
);
```

**Parameters:**
- `queue` - Queue handle from `zest_imm_BeginCommandBuffer`
- `src_buffer` - Source buffer (typically staging buffer)
- `dst_buffer` - Destination buffer (typically device-local buffer)
- `size` - Number of bytes to copy

**Returns:** `ZEST_TRUE` on success.

**Example:**
```cpp
// Upload vertex data to GPU
zest_buffer staging = zest_CreateStagingBuffer(device, vertex_size, vertex_data);
zest_buffer gpu_buffer = zest_CreateDeviceBuffer(device, vertex_size, zest_buffer_usage_vertex);

zest_queue queue = zest_imm_BeginCommandBuffer(device, zest_queue_transfer);
zest_imm_CopyBuffer(queue, staging, gpu_buffer, vertex_size);
zest_imm_EndCommandBuffer(queue);

zest_FreeBuffer(staging);
```

---

### zest_imm_CopyBufferRegion

Copy a region from one buffer to another with specific offsets. Useful for partial updates or appending data.

```cpp
zest_bool zest_imm_CopyBufferRegion(
    zest_queue queue,
    zest_buffer src_buffer,
    zest_size src_offset,
    zest_buffer dst_buffer,
    zest_size dst_offset,
    zest_size size
);
```

**Parameters:**
- `queue` - Queue handle from `zest_imm_BeginCommandBuffer`
- `src_buffer` - Source buffer
- `src_offset` - Byte offset in source buffer
- `dst_buffer` - Destination buffer
- `dst_offset` - Byte offset in destination buffer
- `size` - Number of bytes to copy

**Returns:** `ZEST_TRUE` on success.

**Example:**
```cpp
// Append mesh data to an existing buffer
zest_queue queue = zest_imm_BeginCommandBuffer(device, zest_queue_transfer);
zest_imm_CopyBufferRegion(queue, staging, 0, vertex_buffer, existing_size, new_data_size);
zest_imm_EndCommandBuffer(queue);
```

---

### zest_imm_FillBuffer

Fill an entire buffer with a 32-bit value. Commonly used to zero-initialize buffers or set default values.

```cpp
void zest_imm_FillBuffer(
    zest_queue queue,
    zest_buffer buffer,
    zest_uint value
);
```

**Parameters:**
- `queue` - Queue handle from `zest_imm_BeginCommandBuffer`
- `buffer` - Buffer to fill
- `value` - 32-bit value to fill with (repeated throughout buffer)

**Example:**
```cpp
// Zero-initialize a compute buffer
zest_queue queue = zest_imm_BeginCommandBuffer(device, zest_queue_transfer);
zest_imm_FillBuffer(queue, counter_buffer, 0);
zest_imm_EndCommandBuffer(queue);
```

---

### zest_imm_UpdateBuffer

Update a buffer with data directly from CPU memory. Limited to 64KB maximum. For larger updates, use staging buffers with `zest_imm_CopyBuffer`.

```cpp
void zest_imm_UpdateBuffer(
    zest_queue queue,
    zest_buffer buffer,
    void *data,
    zest_size intended_size
);
```

**Parameters:**
- `queue` - Queue handle from `zest_imm_BeginCommandBuffer`
- `buffer` - Destination buffer
- `data` - Pointer to source data
- `intended_size` - Size in bytes (max 64KB)

**Example:**
```cpp
// Quick update of uniform data
zest_queue queue = zest_imm_BeginCommandBuffer(device, zest_queue_transfer);
zest_imm_UpdateBuffer(queue, uniform_buffer, &uniforms, sizeof(uniforms));
zest_imm_EndCommandBuffer(queue);
```

---

## Image Operations

### zest_imm_CopyBufferToImage

Copy pixel data from a buffer to an image. Used for texture uploads from staging buffers.

```cpp
zest_bool zest_imm_CopyBufferToImage(
    zest_queue queue,
    zest_buffer src_buffer,
    zest_image dst_image,
    zest_size size
);
```

**Parameters:**
- `queue` - Queue handle from `zest_imm_BeginCommandBuffer`
- `src_buffer` - Source buffer containing pixel data
- `dst_image` - Destination image
- `size` - Size in bytes to copy

**Returns:** `ZEST_TRUE` on success.

**Example:**
```cpp
// Upload texture from staging buffer
zest_buffer staging = zest_CreateStagingBuffer(device, pixel_size, pixels);

zest_queue queue = zest_imm_BeginCommandBuffer(device, zest_queue_transfer);
zest_imm_TransitionImage(queue, texture, zest_image_layout_transfer_dst_optimal, 0, 1, 0, 1);
zest_imm_CopyBufferToImage(queue, staging, texture, pixel_size);
zest_imm_TransitionImage(queue, texture, zest_image_layout_shader_read_only_optimal, 0, 1, 0, 1);
zest_imm_EndCommandBuffer(queue);

zest_FreeBuffer(staging);
```

---

### zest_imm_CopyBufferRegionsToImage

Copy multiple regions from a buffer to an image. Useful for uploading texture arrays, cube maps, or mip levels from a single buffer.

```cpp
zest_bool zest_imm_CopyBufferRegionsToImage(
    zest_queue queue,
    zest_buffer_image_copy_t *regions,
    zest_uint regions_count,
    zest_buffer buffer,
    zest_image image
);
```

**Parameters:**
- `queue` - Queue handle from `zest_imm_BeginCommandBuffer`
- `regions` - Array of copy region descriptors
- `regions_count` - Number of regions
- `buffer` - Source buffer
- `image` - Destination image

**Returns:** `ZEST_TRUE` on success.

**Example:**
```cpp
// Upload 6 faces of a cube map
zest_buffer_image_copy_t regions[6];
// ... configure regions for each face ...

zest_queue queue = zest_imm_BeginCommandBuffer(device, zest_queue_graphics);
zest_imm_TransitionImage(queue, cube_map, zest_image_layout_transfer_dst_optimal, 0, 1, 0, 6);
zest_imm_CopyBufferRegionsToImage(queue, regions, 6, staging, cube_map);
zest_imm_TransitionImage(queue, cube_map, zest_image_layout_shader_read_only_optimal, 0, 1, 0, 6);
zest_imm_EndCommandBuffer(queue);
```

---

### zest_imm_TransitionImage

Transition an image to a new layout with full control over mip levels and array layers. Required before and after many image operations. Failure to transition images correctly will result in validation errors and undefined behaviour.

```cpp
zest_bool zest_imm_TransitionImage(
    zest_queue queue,
    zest_image image,
    zest_image_layout new_layout,
    zest_uint base_mip_index,
    zest_uint mip_levels,
    zest_uint base_array_index,
    zest_uint layer_count
);
```

**Parameters:**
- `queue` - Queue handle from `zest_imm_BeginCommandBuffer`
- `image` - Image to transition
- `new_layout` - Target layout (e.g., `zest_image_layout_shader_read_only_optimal`)
- `base_mip_index` - First mip level to transition
- `mip_levels` - Number of mip levels (use `ZEST__ALL_MIPS` for all)
- `base_array_index` - First array layer to transition
- `layer_count` - Number of layers (use `ZEST__ALL_LAYERS` for all)

**Returns:** `ZEST_TRUE` on success.

**Common layouts:**
- `zest_image_layout_undefined` - Initial/don't care
- `zest_image_layout_transfer_dst_optimal` - Destination for copies
- `zest_image_layout_transfer_src_optimal` - Source for copies
- `zest_image_layout_shader_read_only_optimal` - Sampling in shaders
- `zest_image_layout_general` - Storage images
- `zest_image_layout_color_attachment_optimal` - Render target

---

### zest_imm_GenerateMipMaps

Generate mip maps for an image using hardware filtering. The image must have been created with multiple mip levels.

```cpp
zest_bool zest_imm_GenerateMipMaps(zest_queue queue, zest_image image);
```

**Parameters:**
- `queue` - Queue handle (must be graphics queue for blit operations)
- `image` - Image to generate mips for

**Returns:** `ZEST_TRUE` on success.

**Example:**
```cpp
// Generate mips after uploading base level
zest_queue queue = zest_imm_BeginCommandBuffer(device, zest_queue_graphics);
zest_imm_GenerateMipMaps(queue, texture);
zest_imm_EndCommandBuffer(queue);
```

---

### zest_imm_ClearColorImage

Clear a color image to a specified value.

```cpp
zest_bool zest_imm_ClearColorImage(
    zest_queue queue,
    zest_image image,
    zest_clear_value_t clear_value
);
```

**Parameters:**
- `queue` - Queue handle from `zest_imm_BeginCommandBuffer`
- `image` - Color image to clear
- `clear_value` - Clear color (use `.color` field of the union)

**Returns:** `ZEST_TRUE` on success.

**Example:**
```cpp
zest_clear_value_t clear = { .color = { 0.0f, 0.0f, 0.0f, 1.0f } };

zest_queue queue = zest_imm_BeginCommandBuffer(device, zest_queue_graphics);
zest_imm_TransitionImage(queue, image, zest_image_layout_transfer_dst_optimal, 0, 1, 0, 1);
zest_imm_ClearColorImage(queue, image, clear);
zest_imm_TransitionImage(queue, image, zest_image_layout_shader_read_only_optimal, 0, 1, 0, 1);
zest_imm_EndCommandBuffer(queue);
```

---

### zest_imm_ClearDepthStencilImage

Clear a depth/stencil image to specified depth and stencil values.

```cpp
zest_bool zest_imm_ClearDepthStencilImage(
    zest_queue queue,
    zest_image image,
    float depth,
    zest_uint stencil
);
```

**Parameters:**
- `queue` - Queue handle from `zest_imm_BeginCommandBuffer`
- `image` - Depth/stencil image to clear
- `depth` - Depth clear value (typically 1.0 for far plane)
- `stencil` - Stencil clear value (typically 0)

**Returns:** `ZEST_TRUE` on success.

**Example:**
```cpp
zest_queue queue = zest_imm_BeginCommandBuffer(device, zest_queue_graphics);
zest_imm_ClearDepthStencilImage(queue, depth_buffer, 1.0f, 0);
zest_imm_EndCommandBuffer(queue);
```

---

### zest_imm_BlitImage

Blit (scaled copy) from one image to another with filtering. Useful for resizing images, format conversion, or copying between images of different dimensions.

```cpp
zest_bool zest_imm_BlitImage(
    zest_queue queue,
    zest_image src_image,
    zest_image dst_image,
    int src_x, int src_y,
    int src_width, int src_height,
    int dst_x, int dst_y,
    int dst_width, int dst_height,
    zest_filter_type filter
);
```

**Parameters:**
- `queue` - Queue handle (must be graphics queue)
- `src_image` - Source image
- `dst_image` - Destination image
- `src_x`, `src_y` - Top-left corner of source region
- `src_width`, `src_height` - Source region dimensions
- `dst_x`, `dst_y` - Top-left corner of destination region
- `dst_width`, `dst_height` - Destination region dimensions
- `filter` - `zest_filter_nearest` or `zest_filter_linear`

**Returns:** `ZEST_TRUE` on success.

**Example:**
```cpp
// Downscale an image by half
zest_queue queue = zest_imm_BeginCommandBuffer(device, zest_queue_graphics);
zest_imm_BlitImage(queue, src, dst,
    0, 0, 1024, 1024,  // source region
    0, 0, 512, 512,    // destination region (half size)
    zest_filter_linear);
zest_imm_EndCommandBuffer(queue);
```

---

### zest_imm_ResolveImage

Resolve a multisampled image to a single-sampled image. Used to convert MSAA render targets to regular textures for further processing or display.

```cpp
zest_bool zest_imm_ResolveImage(
    zest_queue queue,
    zest_image src_image,
    zest_image dst_image
);
```

**Parameters:**
- `queue` - Queue handle from `zest_imm_BeginCommandBuffer`
- `src_image` - Multisampled source image
- `dst_image` - Single-sampled destination image (must match dimensions)

**Returns:** `ZEST_TRUE` on success.

---

## Compute Operations

### zest_imm_SendPushConstants

Send push constant data to shaders. Must be called inside an immediate command buffer.

```cpp
void zest_imm_SendPushConstants(
    zest_queue queue,
    void *data,
    zest_uint size
);
```

**Parameters:**
- `queue` - Queue handle from `zest_imm_BeginCommandBuffer`
- `data` - Pointer to push constant data
- `size` - Size in bytes

---

### zest_imm_BindComputePipeline

Bind a compute pipeline for subsequent dispatch calls.

```cpp
void zest_imm_BindComputePipeline(zest_queue queue, zest_compute compute);
```

**Parameters:**
- `queue` - Queue handle from `zest_imm_BeginCommandBuffer`
- `compute` - Compute pipeline handle

---

### zest_imm_DispatchCompute

Dispatch a compute workload. Must call `zest_imm_BindComputePipeline` first.

```cpp
zest_bool zest_imm_DispatchCompute(
    zest_queue queue,
    zest_uint group_count_x,
    zest_uint group_count_y,
    zest_uint group_count_z
);
```

**Parameters:**
- `queue` - Queue handle from `zest_imm_BeginCommandBuffer`
- `group_count_x/y/z` - Number of work groups in each dimension

**Returns:** `ZEST_TRUE` on success.

**Example:**
```cpp
// Run a compute shader outside frame graph
zest_queue queue = zest_imm_BeginCommandBuffer(device, zest_queue_compute);
zest_imm_BindComputePipeline(queue, blur_compute);
zest_imm_SendPushConstants(queue, &params, sizeof(params));
zest_imm_DispatchCompute(queue, (width + 15) / 16, (height + 15) / 16, 1);
zest_imm_EndCommandBuffer(queue);
```

---

## Complete Example

```cpp
// Full texture upload workflow
void upload_texture(zest_device device, zest_image texture, void *pixels, zest_size size) {
    // Create staging buffer with pixel data
    zest_buffer staging = zest_CreateStagingBuffer(device, size, pixels);

    // Upload to GPU
    zest_queue queue = zest_imm_BeginCommandBuffer(device, zest_queue_transfer);
    zest_imm_TransitionImage(queue, texture, zest_image_layout_transfer_dst_optimal, 0, 1, 0, 1);
    zest_imm_CopyBufferToImage(queue, staging, texture, size);
    zest_imm_TransitionImage(queue, texture, zest_image_layout_transfer_src_optimal, 0, 1, 0, 1);
    zest_imm_EndCommandBuffer(queue);

    // Generate mipmaps (requires graphics queue for blit)
    queue = zest_imm_BeginCommandBuffer(device, zest_queue_graphics);
    zest_imm_GenerateMipMaps(queue, texture);
    zest_imm_EndCommandBuffer(queue);

    // Clean up staging buffer
    zest_FreeBuffer(staging);
}
```

---

## See Also

- [Command API](commands.md) - Frame graph commands
- [Buffers Concept](../concepts/buffers.md)
- [Image API](image.md) - Image creation and management
