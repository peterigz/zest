# Device API

Functions for device creation, configuration, and management. The device is the central object that manages the graphics backend, GPU selection, shader library, pipeline templates, bindless descriptor sets, and all GPU memory allocators. Typically you create one device per application, which can then serve one or more contexts (windows).

## Creation

### zest_implglfw_CreateDevice

Creates a Zest device with GLFW window support.

```cpp
zest_device zest_implglfw_CreateDevice(zest_bool enable_validation);
```

This is a convenience function that handles graphics backend initialization, GPU selection, and command queue setup for GLFW-based applications. Use validation layers during development to catch API misuse and bugs.

**Parameters:**

- `enable_validation` - Enable validation layers (`ZEST_TRUE` for development, `ZEST_FALSE` for release)

**Returns:** Device handle

**Example:**

```cpp
// Development build with validation
zest_device device = zest_implglfw_CreateDevice(true);

// Release build without validation overhead
zest_device device = zest_implglfw_CreateDevice(false);
```

---

### zest_DestroyDevice

Destroys the device and frees all associated resources.

```cpp
void zest_DestroyDevice(zest_device device);
```

Call this at application shutdown. This will free all GPU memory pools, shut down the graphics backend, and release all CPU allocations including any contexts. The function will warn if any memory leaks are detected.

**Example:**

```cpp
// At application shutdown
zest_DestroyDevice(device);
```

---

## Configuration

Most of the following configuration functions are for more advanced use when you want to fine tune your use of the API. Memory allocations and pool creation happens automatically internally.

### zest_SetDevicePoolSize

Sets the pool size for a specific type of buffer allocation.

```cpp
void zest_SetDevicePoolSize(
    zest_device device,
    const char *name,
    zest_buffer_usage_t usage,
    zest_size minimum_allocation_size,
    zest_size pool_size
);
```

Configures the memory pool for buffers matching the given usage flags. Call this after device creation but before creating contexts or resources. Pool sizes must be powers of 2.

**Note** if the device runs out of memory it will create and add a new pool of memory to use.

**Parameters:**

- `device` - The device handle
- `name` - A descriptive name for debugging/logging
- `usage` - Buffer usage and memory property flags
- `minimum_allocation_size` - Smallest allocation unit (may be overridden by alignment requirements)
- `pool_size` - Total pool size (must be power of 2)

---

### zest_SetGPUBufferPoolSize

Sets the default pool size for GPU-local buffer allocations.

```cpp
void zest_SetGPUBufferPoolSize(
    zest_device device,
    zest_size minimum_size,
    zest_size pool_size
);
```

Configures the memory pool used for GPU-only buffers (vertex buffers, index buffers, etc.). Call after device creation but before creating contexts. Useful when you know your application will need more GPU memory than the default.

**Parameters:**

- `device` - The device handle
- `minimum_size` - Minimum allocation granularity
- `pool_size` - Total pool size (must be power of 2)

**Example:**

```cpp
zest_device device = zest_implglfw_CreateDevice(ZEST_FALSE);

// Increase GPU buffer pool for a mesh-heavy application
zest_SetGPUBufferPoolSize(device, zloc__KILOBYTE(64), zloc__MEGABYTE(256));

zest_context context = zest_CreateContext(device, &window, &info);
```

---

### zest_SetStagingBufferPoolSize

Sets the pool size for staging buffer allocations.

```cpp
void zest_SetStagingBufferPoolSize(
    zest_device device,
    zest_size minimum_size,
    zest_size pool_size
);
```

Staging buffers are CPU-visible memory used to upload data to the GPU (textures, mesh data, etc.). Increase this if you're loading many large assets or streaming data frequently.

**Parameters:**

- `device` - The device handle
- `minimum_size` - Minimum allocation granularity
- `pool_size` - Total pool size (must be power of 2)

**Example:**

```cpp
zest_device device = zest_implglfw_CreateDevice(ZEST_FALSE);

// Larger staging pool for texture-heavy applications
zest_SetStagingBufferPoolSize(device, zloc__KILOBYTE(256), zloc__MEGABYTE(128));

zest_context context = zest_CreateContext(device, &window, &info);
```

---

### zest_SetGPUSmallBufferPoolSize

Sets the pool size for small GPU-local buffer allocations.

```cpp
void zest_SetGPUSmallBufferPoolSize(
    zest_device device,
    zest_size minimum_size,
    zest_size pool_size
);
```

Configures the memory pool for small GPU-only buffers. This is a separate pool from the main GPU buffer pool, optimized for many small allocations with less fragmentation. Useful when your application creates numerous small buffers (e.g., per-object uniform buffers).

**Parameters:**

- `device` - The device handle
- `minimum_size` - Minimum allocation granularity
- `pool_size` - Total pool size (must be power of 2)

---

### zest_SetGPUTransientBufferPoolSize

Sets the pool size for transient GPU buffer allocations.

```cpp
void zest_SetGPUTransientBufferPoolSize(
    zest_device device,
    zest_size minimum_size,
    zest_size pool_size
);
```

Configures the memory pool for transient GPU buffers—temporary buffers that are allocated and freed frequently within a frame or across frames. These are GPU-local buffers optimized for short-lived allocations.

**Parameters:**

- `device` - The device handle
- `minimum_size` - Minimum allocation granularity
- `pool_size` - Total pool size (must be power of 2)

---

### zest_SetGPUSmallTransientBufferPoolSize

Sets the pool size for small transient GPU buffer allocations.

```cpp
void zest_SetGPUSmallTransientBufferPoolSize(
    zest_device device,
    zest_size minimum_size,
    zest_size pool_size
);
```

Configures the memory pool for small transient GPU buffers, combining the characteristics of both small and transient pools. Ideal for applications that frequently allocate and free many small temporary buffers.

**Parameters:**

- `device` - The device handle
- `minimum_size` - Minimum allocation granularity
- `pool_size` - Total pool size (must be power of 2)

---

### zest_SetSmallHostBufferPoolSize

Sets the pool size for small host-visible buffer allocations.

```cpp
void zest_SetSmallHostBufferPoolSize(
    zest_device device,
    zest_size minimum_size,
    zest_size pool_size
);
```

Configures the memory pool for small CPU-accessible buffers. These buffers are host-visible and coherent, meaning the CPU can read/write them directly without explicit flushing. Useful for many small uniform buffers or other frequently-updated data.

**Parameters:**

- `device` - The device handle
- `minimum_size` - Minimum allocation granularity
- `pool_size` - Total pool size (must be power of 2)

---

## Per-Frame

### zest_UpdateDevice

Updates device state each frame. Must be called once per frame before any `zest_BeginFrame` calls.

```cpp
int zest_UpdateDevice(zest_device device);
```

Performs maintenance tasks including:
- Frees deferred resources that are no longer in use by the GPU
- Releases staging buffers that have finished uploading
- Increments the internal frame counter for the device

**Returns:** Number of resources freed this frame (useful for debugging memory issues)

**Example:**

```cpp
while (!glfwWindowShouldClose(window)) {
    zest_UpdateDevice(device);  // Must come before BeginFrame
    glfwPollEvents();

    if (zest_BeginFrame(context)) {
        // ... render ...
        zest_EndFrame(context);
    }
}
```

---

### zest_WaitForIdleDevice

Blocks until all GPU work is complete.

```cpp
void zest_WaitForIdleDevice(zest_device device);
```

Waits for all queues to drain and the GPU to become idle. Use this before destroying resources that might still be in use, or when you need to guarantee all pending work is finished (e.g., before taking a screenshot of completed rendering).

This is a heavy function that should not be used frequently, only when you absolutely must ensure that the GPU has finished all work.

**Example:**

```cpp
// Ensure GPU is done before destroying resources
zest_WaitForIdleDevice(device);
zest_FreeTextureNow(my_texture);
```

---

## Queries

### zest_GetDevicePoolSize

Gets the configured pool size for a specific memory type.

```cpp
zest_buffer_pool_size_t zest_GetDevicePoolSize(
    zest_device device,
    zest_memory_property_flags property_flags
);
```

Returns information about the pool configuration for buffers with the given memory properties.

**Returns:** A struct containing the pool name, size, and minimum allocation size

---

### zest_GetDeviceMemoryStats

Gets current memory allocation statistics for the device.

```cpp
zloc_allocation_stats_t zest_GetDeviceMemoryStats(zest_device device);
```

Returns statistics from the TLSF allocator managing device memory, useful for debugging memory usage and detecting leaks.

**Returns:** Statistics including capacity, free space, blocks in use, and free block count

**Example:**

```cpp
zloc_allocation_stats_t stats = zest_GetDeviceMemoryStats(device);
printf("Device memory: %zu / %zu bytes used (%d blocks)\n",
    stats.capacity - stats.free, stats.capacity, stats.blocks_in_use);
```

---

## See Also

- [Context API](context.md)
- [Device Concept](../concepts/device-and-context.md)
