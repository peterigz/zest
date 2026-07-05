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

**Types:**

| Buffer Type | Usage |
|-------------|-------|
| zest_buffer_type_staging | Used to upload data from CPU side to GPU |
| zest_buffer_type_vertex | Any kind of data for storing vertices used in vertex shaders |
| zest_buffer_type_index | Index data for use in vertex shaders |
| zest_buffer_type_uniform | Small buffers for uploading data to the GPU every frame |
| zest_buffer_type_storage | General purpose storage buffers mainly for compute but any other shader type can access too |
| zest_buffer_type_vertex_storage | Vertex data that can also be accessed/written to by the GPU |
| zest_buffer_type_index_storage | Index data that can also be accessed/written to by the GPU |

**Memory:** `zest_memory_usage_gpu_only`, `zest_memory_usage_cpu_to_gpu`, `zest_memory_usage_gpu_to_cpu`

| Memory Usage | Usage |
|--------------|-------|
| zest_memory_usage_gpu_only | Any memory that is stored locally on the GPU only |
| zest_memory_usage_cpu_to_gpu | Used for staging and uniform buffers that exist in host memory and can be transferred to GPU only memory |
| zest_memory_usage_gpu_to_cpu | GPU local memory that can be used to transfer data back to the host for debugging or other purposes |

---

### zest_CreateBuffer

Buffers can be used for wide variety of data storage depending on what you need. 

```cpp
zest_buffer zest_CreateBuffer(
    zest_device device,
    zest_size size,
    zest_buffer_info_t *info
);
```

Some typical examples:

**For particle data processed in a compute shader**
```cpp
zest_buffer_info_t particle_vertex_buffer_info = zest_CreateBufferInfo(zest_buffer_type_vertex_storage, zest_memory_usage_gpu_only);
app->particle_buffer = zest_CreateBuffer(app->device, storage_buffer_size, &particle_vertex_buffer_info);
```

**For mesh data processed in a vertex shader**
```cpp
zest_buffer_info_t index_info = zest_CreateBufferInfo(zest_buffer_type_index, zest_memory_usage_gpu_only);
zest_buffer_info_t vertex_info = zest_CreateBufferInfo(zest_buffer_type_vertex, zest_memory_usage_gpu_only);
app->planet_mesh.index_buffer = zest_CreateBuffer(app->device, planet_index_capacity, &index_info);
app->planet_mesh.vertex_buffer = zest_CreateBuffer(app->device, planet_vertex_capacity, &vertex_info);
```

---

### zest_CreateStagingBuffer

Create staging buffer with initial data. This will prepare the appropriate buffer info for a staging buffer configuration and you can pass in a pointer to any data that you want copied to the staging buffer so that it can then be immediately used to upload the data to the GPU.

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

A convenience function you can use to grow a buffer. It will grow the buffer to at least `minimum_bytes`, growing by `unit_size` increments. Returns true if the buffer grew.
You could use this function if you are incrementally writing data to a staging buffer and planning to upload to a device buffer at some point. If the buffer runs out of space you can call this to grow the memory.

**Contract (applies to `zest_ResizeBuffer` too):** growing can relocate the buffer to a different memory block, so `memory_offset` can change and anything caching it (descriptors, recorded copies) must be refreshed after a successful grow.

- **Host visible buffers:** contents are preserved (copied CPU-side when the buffer relocates).
- **Device local buffers:** contents are **not** preserved — the buffer always relocates and you must fully re-upload it after growing. The old block is freed deferred (per frame in flight), so in-flight GPU reads of the old block are safe.

Safe usage patterns: per frame-in-flight host visible buffers, and device local buffers that are fully re-uploaded after growth.

```cpp
zest_bool zest_GrowBuffer(zest_buffer *buffer, zest_size unit_size, zest_size minimum_bytes);
```

---

### zest_ResizeBuffer

Resize buffer (may reallocate). Resizes to at least the size that you pass in to the function (sizes are rounded up to the pool's offset granularity) and never shrinks a buffer. Returns true if the buffer was resized successfully. See the contract on `zest_GrowBuffer`: contents are preserved for host visible buffers only; device local buffers relocate with a deferred free of the old block and must be fully re-uploaded.

```cpp
zest_bool zest_ResizeBuffer(zest_buffer *buffer, zest_size new_size);
```

---

### zest_GetBufferSize

```cpp
zest_size zest_GetBufferSize(zest_buffer buffer);
```

---

### zest_FreeBuffer

Frees the buffer. The free is deferred per frame in flight, so the memory is not reused while the GPU may still be reading it.

```cpp
void zest_FreeBuffer(zest_buffer buffer);
```

---

## Uniform Buffers

### zest_CreateUniformBuffer

Create per-frame uniform buffer. Internally this creates a buffer for each frame in flight (typically 2).

Having a buffer for each frame in flight means you can safely write to each buffer whilst the other from the previous frame may still be accessed on the GPU.

```cpp
zest_uniform_buffer_handle zest_CreateUniformBuffer(
    zest_context context,
    const char *name,
    zest_size size
);
```

---

### zest_GetUniformBufferData

Get pointer to the mapped memory for the uniform buffer for the current frame in flight.

```cpp
void* zest_GetUniformBufferData(zest_uniform_buffer ubo);
```

---

### zest_GetUniformBufferDescriptorIndex

Get the bindless index for the uniform buffer that you can pass onto your shaders for access there. It's important that you do this each frame as each frame in flight uniform buffer will have its own index that was acquired when the buffer was created.

```cpp
zest_uint zest_GetUniformBufferDescriptorIndex(zest_uniform_buffer ubo);
```

---

## Data Access

### zest_BufferData / zest_BufferDataEnd

Map CPU-visible buffer. Gets the pointer to a mapped memory range in a CPU-visible buffer.

```cpp
void* zest_BufferData(zest_buffer buffer);
void* zest_BufferDataEnd(zest_buffer buffer);
```

---

## See Also

- [Buffers Concept](../concepts/buffers.md)
- [Memory Concept](../concepts/memory.md)
