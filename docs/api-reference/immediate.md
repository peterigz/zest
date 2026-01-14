# Immediate API

One-off GPU commands outside frame graphs (`zest_imm_*` functions).

## Command Buffer

### zest_imm_BeginCommandBuffer

Begin immediate command recording.

```cpp
zest_queue zest_imm_BeginCommandBuffer(zest_device device, zest_queue_type type);
```

Types: `zest_queue_graphics`, `zest_queue_transfer`, `zest_queue_compute`

---

### zest_imm_EndCommandBuffer

Submit and wait for completion.

```cpp
void zest_imm_EndCommandBuffer(zest_queue queue);
```

---

## Buffer Operations

### zest_imm_CopyBuffer

```cpp
void zest_imm_CopyBuffer(
    zest_queue queue,
    zest_buffer src,
    zest_buffer dst,
    zest_size size
);
```

---

### zest_imm_CopyBufferRegion

```cpp
void zest_imm_CopyBufferRegion(
    zest_queue queue,
    zest_buffer src,
    zest_buffer dst,
    zest_size src_offset,
    zest_size dst_offset,
    zest_size size
);
```

---

### zest_imm_FillBuffer

```cpp
void zest_imm_FillBuffer(
    zest_queue queue,
    zest_buffer buffer,
    zest_uint data,
    zest_size offset,
    zest_size size
);
```

---

## Image Operations

### zest_imm_CopyBufferToImage

```cpp
void zest_imm_CopyBufferToImage(
    zest_queue queue,
    zest_buffer buffer,
    zest_image image
);
```

---

### zest_imm_CopyImageToImage

```cpp
void zest_imm_CopyImageToImage(
    zest_queue queue,
    zest_image src,
    zest_image dst,
    zest_uint width,
    zest_uint height
);
```

---

### zest_imm_GenerateMipMaps

```cpp
void zest_imm_GenerateMipMaps(zest_queue queue, zest_image image);
```

---

### zest_imm_TransitionImage

```cpp
void zest_imm_TransitionImage(
    zest_queue queue,
    zest_image image,
    VkImageLayout new_layout
);
```

---

### zest_imm_ClearColorImage

```cpp
void zest_imm_ClearColorImage(
    zest_queue queue,
    zest_image image,
    float r, float g, float b, float a
);
```

---

### zest_imm_BlitImage

```cpp
void zest_imm_BlitImage(
    zest_queue queue,
    zest_image src,
    zest_image dst,
    zest_filter filter
);
```

---

## Example Usage

```cpp
// Upload texture data
zest_buffer staging = zest_CreateStagingBuffer(device, size, pixels);

zest_queue queue = zest_imm_BeginCommandBuffer(device, zest_queue_transfer);
zest_imm_CopyBufferToImage(queue, staging, image);
zest_imm_EndCommandBuffer(queue);

zest_FreeBuffer(staging);

// Generate mipmaps
queue = zest_imm_BeginCommandBuffer(device, zest_queue_graphics);
zest_imm_GenerateMipMaps(queue, image);
zest_imm_EndCommandBuffer(queue);
```

---

## See Also

- [Command API](commands.md) - Frame graph commands
- [Buffers Concept](../concepts/buffers.md)
