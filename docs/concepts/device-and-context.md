# Device & Context

Zest organizes rendering around two core objects: the **Device** and the **Context**. Understanding their responsibilities is fundamental to using the library.

## Device (`zest_device`)

The device is a singleton that represents your GPU and all shared resources.

### Creation

```cpp
// Basic creation (validation disabled)
zest_device device = zest_implglfw_CreateVulkanDevice(false);

// With validation layers (recommended for development)
zest_device device = zest_implglfw_CreateVulkanDevice(true);
```

### What the Device Manages

| Resource | Description |
|----------|-------------|
| Vulkan Instance | The Vulkan API entry point |
| Physical Device | GPU selection and feature queries |
| Logical Device | Vulkan device handle |
| Shader Library | Compiled shader modules |
| Pipeline Templates | Cached pipeline configurations |
| Bindless Descriptors | Global descriptor set for all resources |
| Memory Pools | GPU memory allocation (TLSF) |
| Samplers | Texture sampler cache |

### Device Lifecycle

```cpp
int main() {
    // Create once at startup
    zest_device device = zest_implglfw_CreateVulkanDevice(false);

    // ... create contexts, run application ...

    // Destroy at shutdown (after all contexts)
    zest_DestroyDevice(device);
}
```

### Per-Frame Device Update

```cpp
while (running) {
    zest_UpdateDevice(device);  // Must call every frame
    // ...
}
```

`zest_UpdateDevice()` handles:

- Deferred resource destruction
- Memory pool maintenance
- Shader reloading (if enabled)

## Context (`zest_context`)

A context represents a render target - typically a window with its swapchain.

### Creation

```cpp
// Create window handles (platform-specific)
zest_window_data_t window = zest_implglfw_CreateWindow(
    50, 50,        // Position
    1280, 768,     // Size
    0,             // Flags
    "My Window"    // Title
);

// Configure context
zest_create_context_info_t info = zest_CreateContextInfo();
ZEST__UNFLAG(info.flags, zest_context_init_flag_enable_vsync);  // Disable vsync

// Create context
zest_context context = zest_CreateContext(device, &window, &info);
```

### What the Context Manages

| Resource | Description |
|----------|-------------|
| Swapchain | Presentation images |
| Command Pools | Per-frame command buffer allocation |
| Synchronization | Semaphores and fences for frame pacing |
| Frame Graph Cache | Compiled frame graphs |
| Linear Allocators | Fast per-frame memory |

### Context Lifecycle

```cpp
// Create context for each window
zest_context context1 = zest_CreateContext(device, &window1, &info);
zest_context context2 = zest_CreateContext(device, &window2, &info);

// ... run application ...

// Contexts are destroyed with the device
zest_DestroyDevice(device);
```

### Frame Boundaries

```cpp
if (zest_BeginFrame(context)) {
    // Frame content here
    // Returns false if window is minimized

    zest_EndFrame(context);
}
```

## Multiple Contexts (Multi-Window)

One device can serve multiple contexts:

```cpp
zest_device device = zest_implglfw_CreateVulkanDevice(false);

// Main window
zest_window_data_t main_window = zest_implglfw_CreateWindow(...);
zest_context main_context = zest_CreateContext(device, &main_window, &info);

// Secondary window
zest_window_data_t debug_window = zest_implglfw_CreateWindow(...);
zest_context debug_context = zest_CreateContext(device, &debug_window, &info);

while (running) {
    zest_UpdateDevice(device);

    // Render to main window
    if (zest_BeginFrame(main_context)) {
        // ... main window rendering ...
        zest_EndFrame(main_context);
    }

    // Render to debug window
    if (zest_BeginFrame(debug_context)) {
        // ... debug window rendering ...
        zest_EndFrame(debug_context);
    }
}
```

## Configuration Options

### Context Flags

```cpp
zest_create_context_info_t info = zest_CreateContextInfo();

// Disable vsync
ZEST__UNFLAG(info.flags, zest_context_init_flag_enable_vsync);

// Enable vsync (default)
ZEST__FLAG(info.flags, zest_context_init_flag_enable_vsync);
```

### Memory Pool Sizes

```cpp
// Before creating the device
zest_SetDevicePoolSize(device, 128 * 1024 * 1024);      // 128 MB device memory
zest_SetStagingBufferPoolSize(device, 64 * 1024 * 1024); // 64 MB staging
```

## Utility Functions

### Window Information

```cpp
// Get screen dimensions
zest_uint width = zest_ScreenWidth(context);
zest_uint height = zest_ScreenHeight(context);

// Float versions
float w = zest_ScreenWidthf(context);
float h = zest_ScreenHeightf(context);

// DPI scale
float dpi = zest_DPIScale(context);

// Native window handle (for platform APIs)
void* native = zest_NativeWindow(context);
GLFWwindow* glfw = (GLFWwindow*)zest_Window(context);
```

### Swapchain Information

```cpp
zest_swapchain swapchain = zest_GetSwapchain(context);
VkFormat format = zest_GetSwapchainFormat(context);
VkExtent2D extent = zest_GetSwapChainExtent(context);

// Set clear color
zest_SetSwapchainClearColor(context, 0.1f, 0.1f, 0.1f, 1.0f);
```

### Frame State

```cpp
// Current frame-in-flight index (0, 1, or 2)
zest_uint fif = zest_CurrentFIF(context);

// Check if swapchain was recreated (window resize)
if (zest_SwapchainWasRecreated(context)) {
    // Rebuild resolution-dependent resources
}
```

## Best Practices

1. **Create device first** - Before any contexts or resources
2. **Call `zest_UpdateDevice()` every frame** - Even if not rendering
3. **Check `zest_BeginFrame()` return value** - Returns false when minimized
4. **Share resources via device** - Textures, pipelines work across contexts
5. **Destroy device last** - After all rendering is complete

## See Also

- [Architecture Overview](../getting-started/architecture.md) - High-level design
- [Frame Graph](frame-graph.md) - Execution model
- [Memory Management](memory.md) - Pool configuration
