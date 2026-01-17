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
│                   TLSF Pool                     │
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
| Small Buffer Pool | 4 MB | GPU-only vertex, index, storage buffers for small buffers |
| Staging Buffer Pool | 32 MB | CPU-visible upload buffers |
| Small Staging Buffer Pool | 4 MB | Small CPU-visible upload buffers |
| Transient Image Memory Pool | 64 MB | Transient textures and render targets used in frame graphs |
| Transient Buffer Memory Pool | 32 MB | Transient buffers used in frame graphs |
| Small Transient Buffer Memory Pool | 4 MB | Small Transient buffers used in frame graphs |

### Configuring Pool Sizes

Configure after creating the device but before creating buffers:

```cpp
// Set pool sizes (minimum allocation size, pool size in bytes)
zest_SetGPUBufferPoolSize(device, 1024, 64 * 1024 * 1024);      // 64 MB GPU buffer pool for large pools
zest_SetStagingBufferPoolSize(device, 1024, 32 * 1024 * 1024);  // 32 MB staging pool for large pools

### Auto-Expansion

Pools automatically expand when exhausted by adding additional pools. If the requested allocation is larger then the pool size then the pool allocation size will be the next power of 2 from the requested allocaiton size.

```cpp
zest_buffer_info_t info = zest_CreateBufferInfo(zest_buffer_type_storage, zest_memory_usage_gpu_only);

// Initial allocation succeeds
zest_buffer buf1 = zest_CreateBuffer(device, 32 * 1024 * 1024, &info);

// Pool expands if needed
zest_buffer buf2 = zest_CreateBuffer(device, 64 * 1024 * 1024, &info);
```

## Linear Allocators

For frame-lifetime allocations, contexts use **linear allocators** internally. These are used automatically by the frame graph system for building the frame graph and means that the frame graph can be cached instantly by simply promoting the used linear memory to persistant memory.  


Benefits:

- **Extremely fast** - Just bump a pointer
- **Zero fragmentation** - Entire pool reset at once
- **Cache-friendly** - Sequential allocations
- **Can be cached instantly** - The linear memory that was used is simply promoted to persistant memory.

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

### CPU-to-GPU Memory

CPU can write, GPU can read. Used for uploads:

```cpp
zest_buffer_info_t info = zest_CreateBufferInfo(
    zest_buffer_type_uniform,
    zest_memory_usage_cpu_to_gpu
);
```

Use for:

- Uniform buffers (updated every frame)
- Staging buffers for uploads

### GPU-to-CPU Memory

GPU can write, CPU can read. Used for readback:

```cpp
zest_buffer_info_t info = zest_CreateBufferInfo(
    zest_buffer_type_storage,
    zest_memory_usage_gpu_to_cpu
);
```

Use for:

- Readback buffers
- GPU compute results that need CPU access

## Transient Resources

Resources that only exist within a frame graph execution:

```cpp
zest_image_resource_info_t info = {
    .format = zest_format_r16g16b16a16_sfloat,
    .usage_hints = zest_resource_usage_hint_none,  // Set as needed
    .width = 1920,
    .height = 1080,
    .mip_levels = 1,
    .layer_count = 1
};
zest_resource_node target = zest_AddTransientImageResource("HDR", &info);
```

### Memory Aliasing

Transient resources can share memory when their lifetimes don't overlap:

```
Pass 1: Write → Target A
Pass 2: Read Target A, Write → Target B
Pass 3: Read Target B, Write → Target C
Pass 3: Read Target C

Target A and Target C can share the same memory!
```

The frame graph compiler automatically handles aliasing.

## Deferred Destruction

Resources are destroyed when safe (after GPU finishes):

```cpp
// Mark for destruction (safe, deferred until GPU is done)
zest_FreeBuffer(buffer);
zest_FreeImage(image_handle);
```

For immediate destruction (ensure GPU is idle first):

```cpp
zest_WaitForIdleDevice(device);
zest_FreeImageNow(image_handle);
zest_FreeImageViewNow(view_handle);
```

Note: Buffer destruction is always deferred through `zest_FreeBuffer()`. 

## Memory Statistics

Query memory usage:

```cpp
// Host memory stats (from TLSF allocator)
// This is memory stored CPU side for everything the API allocates
zloc_allocation_stats_t device_stats = zest_GetDeviceMemoryStats(device);
printf("Capacity: %zu bytes\n", device_stats.capacity);
printf("Free: %zu bytes\n", device_stats.free);
printf("Used: %zu bytes\n", device_stats.capacity - device_stats.free);
printf("Blocks in use: %d\n", device_stats.blocks_in_use);

// Context memory stats
zloc_allocation_stats_t context_stats = zest_GetContextMemoryStats(context);

// Print detailed memory report
zest_OutputMemoryUsage(context);
```

## Memory Debugging

### Enable Memory Tracking

```cpp
// Set error log path for memory warnings
zest_SetErrorLogPath(device, "memory_log.txt");

// Print reports for a context. Can be useful to use after zest_EndFrameGraph to print any errors.
// Reports are also printed when the device is destroyed.
zest_PrintReports(context);
```

### Common Issues

**Leaks:**

```
Symptom: Memory usage grows over time
Solution: Ensure all resources are freed, check zest_OutputMemoryUsage(). When the device is destroyed it will check for any blocks of memory that have not been freed and alert you that there maybe a potential memory leak.
```

## Best Practices

1. **Size pools for your use case** - Larger pools = fewer reallocations
2. **Use transient resources** - For intermediate render targets
3. **Prefer GPU-only memory** - For static resources
4. **Batch uploads** - Minimize staging buffer churn
5. **Monitor memory usage** - Use `zest_OutputMemoryUsage()` during development or check for leak reports when the device is destroyed.

## Example: Memory-Efficient Rendering

```cpp
// Configure pools based on expected usage (minimum size, pool size)
zest_SetGPUBufferPoolSize(device, 1024, 256 * 1024 * 1024);    // 256 MB for large scenes
zest_SetStagingBufferPoolSize(device, 1024, 64 * 1024 * 1024); // 64 MB for streaming

// Use transient resources for render targets
zest_key key;
if (zest_BeginFrameGraph(context, "Renderer", &key)) {
    // These share memory when possible
    zest_resource_node gbuffer_albedo = zest_AddTransientImageResource("Albedo", &rt_info);
    zest_resource_node gbuffer_normal = zest_AddTransientImageResource("Normal", &rt_info);
    zest_resource_node gbuffer_depth = zest_AddTransientImageResource("Depth", &depth_info);

    // ... passes ...

    frame_graph = zest_EndFrameGraph();
}

```

## See Also

- [Buffers](buffers.md) - Buffer creation and management
- [Images](images.md) - Image memory
- [Frame Graph](frame-graph.md) - Transient resources
