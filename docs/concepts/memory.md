# Memory Management

Zest uses a **TLSF (Two-Level Segregated Fit)** allocator for both CPU and GPU memory. This provides fast, fragmentation-resistant allocation.

## TLSF Allocator

### Properties

- **O(1) allocation and free** - Constant time operations
- **Low fragmentation** - Two-level segregation minimizes waste
- **Bounded overhead** - Predictable memory usage
- **Thread-safe** - Safe for multi-threaded use

### How It Works

```
┌─────────────────────────────────────────────────┐
│                   TLSF Pool                      │
├─────────────────────────────────────────────────┤
│  First Level: Size classes (powers of 2)        │
│  ├── 64 KB                                      │
│  ├── 128 KB                                     │
│  ├── 256 KB                                     │
│  └── ...                                        │
├─────────────────────────────────────────────────┤
│  Second Level: Subdivisions within each class   │
│  ├── 64-80 KB                                   │
│  ├── 80-96 KB                                   │
│  ├── 96-112 KB                                  │
│  └── ...                                        │
└─────────────────────────────────────────────────┘
```

## Memory Pools

Zest organizes GPU memory into pools:

| Pool | Default Size | Contents |
|------|--------------|----------|
| Device Buffer Pool | 64 MB | GPU-only vertex, index, storage buffers |
| Staging Buffer Pool | 32 MB | CPU-visible upload buffers |
| Image Memory Pool | 256 MB | Textures and render targets |

### Configuring Pool Sizes

Configure before creating the device:

```cpp
// Set pool sizes (in bytes)
zest_SetDevicePoolSize(device, 128 * 1024 * 1024);      // 128 MB
zest_SetGPUBufferPoolSize(device, 64 * 1024 * 1024);    // 64 MB
zest_SetStagingBufferPoolSize(device, 32 * 1024 * 1024); // 32 MB

// Query current sizes
zest_size device_pool = zest_GetDevicePoolSize(device);
zest_size gpu_pool = zest_GetGPUBufferPoolSize(device);
zest_size staging_pool = zest_GetStagingBufferPoolSize(device);
```

### Auto-Expansion

Pools automatically expand when exhausted:

```cpp
// Initial allocation succeeds
zest_buffer buf1 = zest_CreateBuffer(device, 32 * 1024 * 1024, &info);

// Pool expands if needed
zest_buffer buf2 = zest_CreateBuffer(device, 64 * 1024 * 1024, &info);
```

## Linear Allocators

For frame-lifetime allocations, contexts use **linear allocators**:

```cpp
// Allocate from context's linear pool (reset each frame)
void* temp = zest_AllocateFromContext(context, size, alignment);

// Automatically freed at end of frame
```

Benefits:

- **Extremely fast** - Just bump a pointer
- **Zero fragmentation** - Entire pool reset at once
- **Cache-friendly** - Sequential allocations

## Memory Types

### GPU-Only Memory

Fastest for GPU access, no CPU access:

```cpp
zest_buffer_info_t info = zest_CreateBufferInfo(
    zest_buffer_type_vertex,
    zest_memory_usage_gpu_only
);
```

Use for:

- Vertex buffers
- Index buffers
- Textures
- Render targets

### CPU-Visible Memory

CPU can write, slower GPU access:

```cpp
zest_buffer_info_t info = zest_CreateBufferInfo(
    zest_buffer_type_uniform,
    zest_memory_usage_cpu_visible
);
```

Use for:

- Uniform buffers (updated every frame)
- Staging buffers
- Readback buffers

### Auto Memory

Let Zest choose optimal placement:

```cpp
zest_buffer_info_t info = zest_CreateBufferInfo(
    zest_buffer_type_storage,
    zest_memory_usage_auto
);
```

## Transient Resources

Resources that only exist within a frame graph execution:

```cpp
zest_image_resource_info_t info = {
    .format = zest_format_r16g16b16a16_sfloat,
    .usage_hint = zest_resource_usage_hint_render_target,
    .width = 1920,
    .height = 1080
};
zest_resource_node target = zest_AddTransientImageResource("HDR", &info);
```

### Memory Aliasing

Transient resources can share memory when their lifetimes don't overlap:

```
Pass 1: Write → Target A
Pass 2: Read Target A, Write → Target B
Pass 3: Read Target B

Target A and Target B can share the same memory!
```

The frame graph compiler automatically handles aliasing.

## Deferred Destruction

Resources are destroyed when safe (after GPU finishes):

```cpp
// Mark for destruction (safe)
zest_FreeBuffer(buffer);
zest_FreeImage(image);

// Actual destruction happens later in zest_UpdateDevice()
```

For immediate destruction (ensure GPU is idle first):

```cpp
zest_WaitForIdleDevice(device);
zest_FreeBufferNow(buffer);
zest_FreeImageNow(image);
```

## Memory Statistics

Query memory usage:

```cpp
// Device memory stats
zest_memory_stats_t device_stats = zest_GetDeviceMemoryStats(device);
printf("Used: %zu / %zu bytes\n", device_stats.used, device_stats.total);

// Context memory stats
zest_memory_stats_t context_stats = zest_GetContextMemoryStats(context);

// Print detailed memory report
zest_OutputMemoryUsage(device);

// Print memory block details (debug)
zest_PrintMemoryBlocks(device);
```

## Memory Debugging

### Enable Memory Tracking

```cpp
// Set error log path for memory warnings
zest_SetErrorLogPath("memory_log.txt");

// Print reports after running
zest_PrintReports();
```

### Common Issues

**Fragmentation:**

```
Symptom: Allocation fails despite enough free memory
Solution: Increase pool size or use transient resources
```

**Leaks:**

```
Symptom: Memory usage grows over time
Solution: Ensure all resources are freed, check zest_OutputMemoryUsage()
```

**Staging exhaustion:**

```
Symptom: Upload operations fail
Solution: Increase staging pool size or batch uploads
```

## Best Practices

1. **Size pools for your use case** - Larger pools = fewer reallocations
2. **Use transient resources** - For intermediate render targets
3. **Prefer GPU-only memory** - For static resources
4. **Batch uploads** - Minimize staging buffer churn
5. **Monitor memory usage** - Use `zest_OutputMemoryUsage()` during development

## Example: Memory-Efficient Rendering

```cpp
// Configure pools based on expected usage
zest_SetDevicePoolSize(device, 256 * 1024 * 1024);  // 256 MB for large scenes
zest_SetStagingBufferPoolSize(device, 64 * 1024 * 1024);  // 64 MB for streaming

// Use transient resources for render targets
if (zest_BeginFrameGraph(context, "Renderer", &key)) {
    // These share memory when possible
    zest_resource_node gbuffer_albedo = zest_AddTransientImageResource("Albedo", &rt_info);
    zest_resource_node gbuffer_normal = zest_AddTransientImageResource("Normal", &rt_info);
    zest_resource_node gbuffer_depth = zest_AddTransientImageResource("Depth", &depth_info);

    // ... passes ...

    frame_graph = zest_EndFrameGraph();
}

// Monitor memory periodically
if (frame_count % 1000 == 0) {
    zest_OutputMemoryUsage(device);
}
```

## See Also

- [Buffers](buffers.md) - Buffer creation and management
- [Images](images.md) - Image memory
- [Frame Graph](frame-graph.md) - Transient resources
