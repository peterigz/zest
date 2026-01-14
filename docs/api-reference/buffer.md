# Buffer API

Functions for buffer creation and management.

## Creation

### zest_CreateBufferInfo

Create buffer configuration.

```cpp
zest_buffer_info_t zest_CreateBufferInfo(
    zest_buffer_type type,
    zest_memory_usage memory_usage
);
```

**Types:** `zest_buffer_type_vertex`, `zest_buffer_type_index`, `zest_buffer_type_storage`, `zest_buffer_type_vertex_storage`

**Memory:** `zest_memory_usage_gpu_only`, `zest_memory_usage_cpu_visible`, `zest_memory_usage_auto`

---

### zest_CreateBuffer

```cpp
zest_buffer zest_CreateBuffer(
    zest_device device,
    zest_size size,
    zest_buffer_info_t *info
);
```

---

### zest_CreateStagingBuffer

Create staging buffer with initial data.

```cpp
zest_buffer zest_CreateStagingBuffer(
    zest_device device,
    zest_size size,
    void *data
);
```

---

## Management

### zest_GrowBuffer

Grow buffer if needed.

```cpp
void zest_GrowBuffer(zest_buffer buffer, zest_size new_size);
```

---

### zest_ResizeBuffer

Resize buffer (may reallocate).

```cpp
void zest_ResizeBuffer(zest_buffer buffer, zest_size new_size);
```

---

### zest_GetBufferSize

```cpp
zest_size zest_GetBufferSize(zest_buffer buffer);
```

---

### zest_FreeBuffer

Deferred buffer destruction.

```cpp
void zest_FreeBuffer(zest_buffer buffer);
```

---

## Uniform Buffers

### zest_CreateUniformBuffer

Create per-frame uniform buffer.

```cpp
zest_uniform_buffer zest_CreateUniformBuffer(
    zest_context context,
    const char *name,
    zest_size size
);
```

---

### zest_GetUniformBufferData

Get pointer to current frame's data.

```cpp
void* zest_GetUniformBufferData(zest_uniform_buffer ubo);
```

---

### zest_GetUniformBufferDescriptorIndex

```cpp
zest_uint zest_GetUniformBufferDescriptorIndex(zest_uniform_buffer ubo);
```

---

## Data Access

### zest_BufferData / zest_BufferDataEnd

Map CPU-visible buffer.

```cpp
void* zest_BufferData(zest_buffer buffer);
void zest_BufferDataEnd(zest_buffer buffer);
```

---

## See Also

- [Buffers Concept](../concepts/buffers.md)
- [Memory Concept](../concepts/memory.md)
