# Tutorial 1: Minimal Template

This tutorial walks through the simplest possible Zest application - displaying a blank screen using a frame graph.

**Example:** `examples/GLFW/zest-minimal-template`

## What You'll Learn

- Creating a device and context
- Building a frame graph
- Defining a render pass
- Frame graph caching

## The Application Structure

Every Zest application follows this pattern:

```cpp
struct app_t {
    zest_device device;    // GPU resources (one per app)
    zest_context context;  // Window/swapchain (one per window)
};
```

## Step 1: Include Headers

```cpp
#define ZEST_IMPLEMENTATION
#define ZEST_VULKAN_IMPLEMENTATION
#include <GLFW/glfw3.h>
#include <zest.h>
```

!!! important
    The `ZEST_IMPLEMENTATION` defines must be in exactly **one** `.cpp` file.

## Step 2: Initialize Device and Context

```cpp
int main(void) {
    // Initialize GLFW
    if (!glfwInit()) return 0;

    app_t app = {};

    // Create device (Vulkan instance, GPU selection, resource pools)
    app.device = zest_implglfw_CreateDevice(false);

    // Create window
    zest_window_data_t window = zest_implglfw_CreateWindow(
        50, 50,       // Position
        1280, 768,    // Size
        0,            // Flags
        "Minimal Example"
    );

    // Configure context options
    zest_create_context_info_t create_info = zest_CreateContextInfo();
    // Create context (swapchain, command pools, frame resources)
    app.context = zest_CreateContext(app.device, &window, &create_info);

    // Run main loop
    MainLoop(&app);

    // Cleanup
    zest_DestroyDevice(app.device);
    return 0;
}
```

## Step 3: The Main Loop

```cpp
void MainLoop(app_t *app) {
    while (!glfwWindowShouldClose((GLFWwindow*)zest_Window(app->context))) {
        // Update device state (deferred cleanup, etc.)
        zest_UpdateDevice(app->device);

        // Process window events
        glfwPollEvents();

        // Generate cache key for frame graph
        zest_frame_graph_cache_key_t cache_key = zest_InitialiseCacheKey(app->context, 0, 0);

        // Begin frame (acquires swapchain image)
        if (zest_BeginFrame(app->context)) {
            // Try to get cached frame graph
            zest_frame_graph frame_graph = zest_GetCachedFrameGraph(app->context, &cache_key);

            if (!frame_graph) {
                // Build new frame graph
                frame_graph = BuildFrameGraph(app, &cache_key);
            }

            // Queue for execution
            zest_QueueFrameGraphForExecution(app->context, frame_graph);

            // End frame (submits commands, presents)
            zest_EndFrame(app->context);
        }
    }
}
```

### Key Points

- `zest_UpdateDevice()` must be called every frame
- `zest_BeginFrame()` returns `false` if window is minimized or the window changes size which results in the swapchain being recreated.
- Frame graphs are cached to avoid recompilation

## Step 4: Building the Frame Graph

```cpp
zest_frame_graph BuildFrameGraph(app_t *app, zest_frame_graph_cache_key_t *cache_key) {
    if (zest_BeginFrameGraph(app->context, "Render Graph", cache_key)) {
        // Import the swapchain as a resource
        zest_ImportSwapchainResource();

        // Create a render pass
        zest_BeginRenderPass("Draw Nothing"); {
            // Declare output to swapchain
            zest_ConnectSwapChainOutput();

            // Set the render callback
            zest_SetPassTask(BlankScreen, 0);

            // End pass definition
            zest_EndPass();
        }

        // Compile and return
        return zest_EndFrameGraph();
    }
    return 0;
}
```

### What Happens Here

1. **Import swapchain** - Makes it available for output
2. **Begin render pass** - Starts pass definition
3. **Connect output** - Declares this pass writes to swapchain
4. **Set task** - Assigns the render callback
5. **End frame graph** - Compiles barriers and synchronization

## Step 5: The Render Callback

```cpp
void BlankScreen(const zest_command_list command_list, void *user_data) {
    // This is where rendering commands go
    // With nothing here, we get a blank screen (cleared to the clear color)
}
```

The callback receives:

- `command_list` - For recording GPU commands
- `user_data` - Whatever you passed to `zest_SetPassTask()`

## Understanding Frame Graph Caching

```cpp
// Cache key includes context state + optional custom data
zest_frame_graph_cache_key_t key = zest_InitialiseCacheKey(context, custom_data, data_size);

// First frame: cache miss, build graph
zest_frame_graph graph = zest_GetCachedFrameGraph(context, &key);
// graph == NULL

// Build and cache
graph = BuildFrameGraph(...);

// Next frame: cache hit, skip building
graph = zest_GetCachedFrameGraph(context, &key);
// graph == valid cached graph
```

Use custom data in the cache key for different configurations:

```cpp
struct config_t { int render_mode; };
config_t config = { .render_mode = 1 };
zest_frame_graph_cache_key_t key = zest_InitialiseCacheKey(context, &config, sizeof(config));
```

## Complete Code

See the full source at `examples/GLFW/zest-minimal-template/zest-minimal-template.cpp`.


## Next Steps

- [Tutorial 2: Adding ImGui](02-imgui.md) - Add a user interface
- [Frame Graph Concept](../concepts/frame-graph.md) - Deep dive on frame graphs
