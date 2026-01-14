# Device API

Functions for device creation, configuration, and management.

## Creation

### zest_implglfw_CreateDevice

Creates a Zest device with GLFW window support.

```cpp
zest_device zest_implglfw_CreateDevice(zest_bool enable_validation);
```

**Parameters:**

- `enable_validation` - Enable Vulkan validation layers (use `true` for development)

**Returns:** Device handle

**Example:**

```cpp
zest_device device = zest_implglfw_CreateDevice(true);  // With validation
```

---

### zest_DestroyDevice

Destroys the device and all associated resources.

```cpp
void zest_DestroyDevice(zest_device device);
```

---

## Configuration

### zest_SetDevicePoolSize

Set the main device memory pool size.

```cpp
void zest_SetDevicePoolSize(zest_device device, zest_size size);
```

---

### zest_SetGPUBufferPoolSize

Set the GPU buffer pool size.

```cpp
void zest_SetGPUBufferPoolSize(zest_device device, zest_size size);
```

---

### zest_SetStagingBufferPoolSize

Set the staging buffer pool size.

```cpp
void zest_SetStagingBufferPoolSize(zest_device device, zest_size size);
```

---

## Per-Frame

### zest_UpdateDevice

Update device state. Must be called every frame.

```cpp
void zest_UpdateDevice(zest_device device);
```

Handles:
- Deferred resource destruction
- Memory pool maintenance
- Shader hot-reloading

---

### zest_WaitForIdleDevice

Block until GPU is idle. Use before immediate destruction.

```cpp
void zest_WaitForIdleDevice(zest_device device);
```

---

## Queries

### zest_GetDevicePoolSize

```cpp
zest_size zest_GetDevicePoolSize(zest_device device);
```

---

### zest_GetDeviceMemoryStats

```cpp
zest_memory_stats_t zest_GetDeviceMemoryStats(zest_device device);
```

---

## See Also

- [Context API](context.md)
- [Device Concept](../concepts/device-and-context.md)
