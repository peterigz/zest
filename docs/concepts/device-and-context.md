# Device & Context

Zest organizes rendering around two core objects: the **Device** and the **Context**. Understanding their responsibilities is fundamental to using the library.

## Device (`zest_device`)

The device is a singleton that represents your GPU and all shared resources.

### Creation

An example of creating a device with SDL2:

```cpp
// Create a window first - the device needs it to query required Vulkan extensions
zest_window_data_t window_data = zest_implsdl2_CreateWindow(50, 50, 1280, 768, 0, "My Window");
zest_device device = zest_implsdl2_CreateVulkanDevice(&window_data, false);
```

If you need more control, you can use the device builder directly:

```cpp
// Create the device using Vulkan as the platform layer
zest_device_builder device_builder = zest_BeginVulkanDeviceBuilder();
// Add your required extensions here
zest_AddDeviceBuilderExtensions(device_builder, extensions, count);
if (enable_validation) {
	zest_AddDeviceBuilderValidation(device_builder);
	zest_DeviceBuilderLogToConsole(device_builder);
}
zest_device device = zest_EndDeviceBuilder(device_builder);
```

Ensure that you include the SDL2 header before zest.h when using the SDL2 helper functions.

### What the Device Manages

| Resource | Description |
|----------|-------------|
| Platform Instance | The Platform API entry point (Currently Vulkan, DX/Metal/WebGPU(?) added in future) |
| Physical Device | GPU selection and feature queries |
| Shaders | Compiled/Loaded shaders |
| Pipeline Templates | Cached pipeline configurations |
| Compute Pipelines | Cached pipelines for compute dispatches |
| Bindless Descriptors | Global descriptor set for all resources |
| Memory Pools | GPU memory allocation (TLSF) and CPU side memory pools |
| Samplers | Texture samplers for use in shaders |
| Images | All textures stored on the GPU |

### Device Lifecycle

```cpp
int main() {
    // Create once at startup
    zest_window_data_t window_data = zest_implsdl2_CreateWindow(50, 50, 1280, 768, 0, "My Window");
zest_device device = zest_implsdl2_CreateVulkanDevice(&window_data, false);

    // ... create contexts, run application ...

    // Destroy at shutdown 
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

## Context (`zest_context`)

A context represents a render target - typically a window with its swapchain.

### Creation

```cpp
// Create window handles (platform-specific)
zest_window_data_t window = zest_implsdl2_CreateWindow(
    50, 50,        // Position
    1280, 768,     // Size
    0,             // Maximised (0 = false)
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
| Synchronization | Uses semaphores for frame pacing |
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

Each frame is bracketed by `zest_BeginFrame()` and `zest_EndFrame()`. You must build or retrieve a frame graph between these calls:

```cpp
// Create a cache key for the frame graph (typically done once per frame)
zest_frame_graph_cache_key_t cache_key = zest_InitialiseCacheKey(context, NULL, 0);

if (zest_BeginFrame(context)) {
    // Try to retrieve cached frame graph
    zest_frame_graph frame_graph = zest_GetCachedFrameGraph(context, &cache_key);
    if (!frame_graph) {
        // Build new frame graph if not cached
        if (zest_BeginFrameGraph(context, "Main Graph", &cache_key)) {
            // ... define passes ...
            frame_graph = zest_EndFrameGraph();
        }
    }

    // Execute frame graph and present
    zest_EndFrame(context, frame_graph);
}
```

`zest_BeginFrame()` returns false if the window is minimized or if the swap chain could not be acquired (window resized or other state change), allowing you to skip rendering. See [Frame Graph](frame-graph.md) for details on building and caching frame graphs.

## Multiple Contexts (Multi-Window)

One device can serve multiple contexts:

```cpp
zest_window_data_t window_data = zest_implsdl2_CreateWindow(50, 50, 1280, 768, 0, "My Window");
zest_device device = zest_implsdl2_CreateVulkanDevice(&window_data, false);

// Main window
zest_window_data_t main_window = zest_implsdl2_CreateWindow(...);
zest_context main_context = zest_CreateContext(device, &main_window, &info);

// Secondary window
zest_window_data_t debug_window = zest_implsdl2_CreateWindow(...);
zest_context debug_context = zest_CreateContext(device, &debug_window, &info);

while (running) {
    zest_UpdateDevice(device);

    // Render to main window
    if (zest_BeginFrame(main_context)) {
        zest_frame_graph main_graph = /* build or retrieve frame graph */;
        zest_EndFrame(main_context, main_graph);
    }

    // Render to debug window
    if (zest_BeginFrame(debug_context)) {
        zest_frame_graph debug_graph = /* build or retrieve frame graph */;
        zest_EndFrame(debug_context, debug_graph);
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

The device manages multiple named memory pools for different buffer types (device buffers, staging buffers, transient buffers, etc.). Default pool sizes are configured automatically, but can be customized after device creation using `zest_SetDevicePoolSize()` and `zest_SetStagingBufferPoolSize()`.

See [Memory Management](memory.md) for details on pool configuration.

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
SDL_Window* sdl_window = (SDL_Window*)zest_Window(context);
```

### Swapchain Information

```cpp
zest_swapchain swapchain = zest_GetSwapchain(context);
zest_format format = zest_GetSwapchainFormat(swapchain);
zest_extent2d_t extent = zest_GetSwapChainExtent(context);

// Set clear color
zest_SetSwapchainClearColor(context, 0.1f, 0.1f, 0.1f, 1.0f);
```

### Frame State

```cpp
// Current frame-in-flight index (0 to ZEST_MAX_FIF - 1, default is 2 frames in flight)
zest_uint fif = zest_CurrentFIF(context);

// Check if swapchain was recreated (window resize)
if (zest_SwapchainWasRecreated(context)) {
    // Rebuild resolution-dependent resources
}
```

## Best Practices

1. **Create device first** - Before any contexts or resources
2. **Call `zest_UpdateDevice()` every frame** - Even if not rendering
3. **Check `zest_BeginFrame()` return value** - Returns false when minimized or swapchain was not acquired
4. **Share resources via device** - Textures, pipelines work across contexts
5. **Destroy device last** - After all rendering is complete

## See Also

- [Architecture Overview](../getting-started/architecture.md) - High-level design
- [Frame Graph](frame-graph.md) - Execution model
- [Memory Management](memory.md) - Pool configuration
