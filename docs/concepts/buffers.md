# Buffers

Buffers are GPU memory regions for vertex data, indices, uniforms, and general storage. Zest provides flexible buffer creation and management.

## Buffer Types

| Type | Usage | Memory |
|------|-------|--------|
| Vertex | Vertex attributes | GPU only |
| Index | Index data | GPU only |
| Uniform | Shader uniforms | CPU visible |
| Storage | Shader read/write | GPU only |
| Staging | CPU to GPU transfer | CPU visible |

## Creating Buffers

### Basic Buffer

```cpp
zest_buffer_info_t info = zest_CreateBufferInfo(
    zest_buffer_type_vertex,       // Type
    zest_memory_usage_gpu_only     // Memory location
);

zest_buffer buffer = zest_CreateBuffer(device, size_in_bytes, &info);
```

### Buffer Types

```cpp
// Vertex buffer (for vertex attributes)
zest_buffer_info_t info = zest_CreateBufferInfo(zest_buffer_type_vertex, zest_memory_usage_gpu_only);

// Index buffer
zest_buffer_info_t info = zest_CreateBufferInfo(zest_buffer_type_index, zest_memory_usage_gpu_only);

// Storage buffer (for compute shaders)
zest_buffer_info_t info = zest_CreateBufferInfo(zest_buffer_type_storage, zest_memory_usage_gpu_only);

// Vertex + storage (readable in shaders)
zest_buffer_info_t info = zest_CreateBufferInfo(zest_buffer_type_vertex_storage, zest_memory_usage_gpu_only);
```

### Memory Usage

```cpp
zest_memory_usage_gpu_only     // Fast GPU access, no CPU access
zest_memory_usage_cpu_visible  // CPU can write, slower GPU access
zest_memory_usage_auto         // Let Zest decide
```

## Uploading Data

### Staging Buffer Approach

For large uploads, use a staging buffer:

```cpp
// Create staging buffer with data
zest_buffer staging = zest_CreateStagingBuffer(device, data_size, cpu_data);

// Copy to GPU buffer
zest_queue queue = zest_imm_BeginCommandBuffer(device, zest_queue_transfer);
zest_imm_CopyBuffer(queue, staging, gpu_buffer, data_size);
zest_imm_EndCommandBuffer(queue);

// Free staging buffer
zest_FreeBuffer(staging);
```

### In Frame Graph

Copy operations in a transfer pass:

```cpp
zest_BeginTransferPass("Upload"); {
    zest_ConnectOutput(buffer_resource);
    zest_SetPassTask(UploadCallback, app);
    zest_EndPass();
}

void UploadCallback(zest_command_list cmd, void* user_data) {
    zest_cmd_CopyBuffer(cmd, staging, dest, size);
}
```

## Uniform Buffers

Per-frame uniform data with automatic multi-buffering:

```cpp
// Create uniform buffer
zest_uniform_buffer ubo = zest_CreateUniformBuffer(context, "camera_ubo", sizeof(camera_t));

// Get pointer to current frame's data
camera_t* camera = (camera_t*)zest_GetUniformBufferData(ubo);
camera->view = view_matrix;
camera->projection = proj_matrix;

// Get descriptor index for shader
zest_uint ubo_index = zest_GetUniformBufferDescriptorIndex(ubo);
```

### Using Uniform Buffers in Shaders

```glsl
layout(set = 0, binding = 1) uniform CameraUBO {
    mat4 view;
    mat4 projection;
} camera[];

void main() {
    mat4 view = camera[push.ubo_index].view;
}
```

## Buffer Operations

### Resize and Grow

```cpp
// Resize buffer (may reallocate)
zest_ResizeBuffer(buffer, new_size);

// Grow buffer (only if needed)
zest_GrowBuffer(buffer, new_size);

// Get current size
zest_size size = zest_GetBufferSize(buffer);
```

### Map and Unmap

For CPU-visible buffers:

```cpp
void* data = zest_BufferData(buffer);
memcpy(data, src, size);
zest_BufferDataEnd(buffer);
```

### Free Buffer

```cpp
// Deferred free (safe, waits for GPU)
zest_FreeBuffer(buffer);
```

## Memory Pools

Buffers are allocated from pools. Configure pool sizes before device creation:

```cpp
// GPU buffer pool (default 64 MB)
zest_SetGPUBufferPoolSize(device, 128 * 1024 * 1024);

// Staging buffer pool (default 32 MB)
zest_SetStagingBufferPoolSize(device, 64 * 1024 * 1024);

// Get current sizes
zest_size gpu_size = zest_GetDevicePoolSize(device);
```

Pools auto-expand when exhausted.

## Command Buffer Operations

### Bind Buffers

```cpp
// Bind vertex buffer
zest_cmd_BindVertexBuffer(cmd, buffer, offset);

// Bind index buffer
zest_cmd_BindIndexBuffer(cmd, buffer, offset, zest_index_type_uint32);
```

### Copy Buffers

```cpp
// Copy entire buffer
zest_cmd_CopyBuffer(cmd, src, dst, size);

// Upload from staging
zest_cmd_UploadBuffer(cmd, staging_buffer, dst_buffer, size);
```

## Transient Buffers

Temporary buffers within a frame graph:

```cpp
zest_buffer_resource_info_t info = {
    .size = particle_count * sizeof(particle_t),
    .usage_hint = zest_resource_usage_hint_vertex_storage
};
zest_resource_node particles = zest_AddTransientBufferResource("Particles", &info);
```

Transient buffers:

- Are allocated from a shared pool
- Can share memory with non-overlapping resources
- Are automatically freed after frame graph execution

## Best Practices

1. **Use staging buffers** - For large GPU-only uploads
2. **Prefer GPU-only memory** - Faster for rendering
3. **Use uniform buffers** - For frequently updated small data
4. **Size pools appropriately** - Based on your application's needs
5. **Use transient buffers** - For intermediate data in multi-pass rendering

## Example: Particle Buffer

```cpp
// Create particle buffer
zest_buffer_info_t info = zest_CreateBufferInfo(
    zest_buffer_type_vertex_storage,
    zest_memory_usage_gpu_only
);
zest_buffer particles = zest_CreateBuffer(device, MAX_PARTICLES * sizeof(particle_t), &info);

// Initial upload
zest_buffer staging = zest_CreateStagingBuffer(device, initial_size, initial_data);
zest_queue queue = zest_imm_BeginCommandBuffer(device, zest_queue_transfer);
zest_imm_CopyBuffer(queue, staging, particles, initial_size);
zest_imm_EndCommandBuffer(queue);
zest_FreeBuffer(staging);

// Use in render callback
void RenderParticles(zest_command_list cmd, void* user_data) {
    zest_cmd_BindVertexBuffer(cmd, particles, 0);
    zest_cmd_Draw(cmd, particle_count, 1, 0, 0);
}
```

## See Also

- [Memory Management](memory.md) - Pool configuration
- [Buffer API](../api-reference/buffer.md) - Complete function reference
- [Compute Tutorial](../tutorials/05-compute.md) - Storage buffers with compute
