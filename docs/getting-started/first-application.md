# First Application

Let's build your first Zest application step by step. We'll create a minimal app that displays a blank screen - the simplest possible frame graph.

## Complete Code

Here's the full example (from `zest-minimal-template`):

```cpp
#define ZEST_IMPLEMENTATION
#define ZEST_VULKAN_IMPLEMENTATION
#include <GLFW/glfw3.h>
#include <zest.h>

struct minimal_app_t {
    zest_device device;
    zest_context context;
};

void BlankScreen(const zest_command_list command_list, void *user_data) {
    // Render commands go here
}

void MainLoop(minimal_app_t *app) {
    while (!glfwWindowShouldClose((GLFWwindow*)zest_Window(app->context))) {
        zest_UpdateDevice(app->device);
        glfwPollEvents();

        zest_frame_graph_cache_key_t cache_key = zest_InitialiseCacheKey(app->context, 0, 0);
        if (zest_BeginFrame(app->context)) {
            zest_frame_graph frame_graph = zest_GetCachedFrameGraph(app->context, &cache_key);
            if (!frame_graph) {
                if (zest_BeginFrameGraph(app->context, "Render Graph", &cache_key)) {
                    zest_ImportSwapchainResource();
                    zest_BeginRenderPass("Draw Nothing"); {
                        zest_ConnectSwapChainOutput();
                        zest_SetPassTask(BlankScreen, 0);
                        zest_EndPass();
                    }
                    frame_graph = zest_EndFrameGraph();
                }
            }
            zest_EndFrame(app->context, frame_graph);
        }
    }
}

int main(void) {
    zest_create_context_info_t create_info = zest_CreateContextInfo();

    if (!glfwInit()) return 0;

    minimal_app_t app = {};
    app.device = zest_implglfw_CreateVulkanDevice(false);

    zest_window_data_t window = zest_implglfw_CreateWindow(50, 50, 1280, 768, 0, "Minimal Example");
    app.context = zest_CreateContext(app.device, &window, &create_info);

    MainLoop(&app);
    zest_DestroyDevice(app.device);

    return 0;
}
```

## Step-by-Step Breakdown

### 1. Include Headers and Define Implementation

```cpp
#define ZEST_IMPLEMENTATION
#define ZEST_VULKAN_IMPLEMENTATION
#include <GLFW/glfw3.h>
#include <zest.h>
```

The `ZEST_IMPLEMENTATION` macros tell Zest to include the actual implementation, not just declarations. Do this in **one file only**.

### 2. Create the Device

```cpp
app.device = zest_implglfw_CreateVulkanDevice(false);
```

The **device** is a singleton that manages:

- Vulkan instance and physical device selection
- Shader library
- Bindless descriptor sets
- Memory pools

The `false` parameter disables validation layers (use `true` during development).

### 3. Create Window and Context

```cpp
zest_window_data_t window = zest_implglfw_CreateWindow(50, 50, 1280, 768, 0, "Minimal Example");
app.context = zest_CreateContext(app.device, &window, &create_info);
```

The **context** is tied to a window/swapchain and manages:

- Frame resources (command buffers, synchronization)
- Frame graph compilation and execution
- Linear allocators for temporary data

One device can serve multiple contexts (multiple windows).

### 4. The Main Loop

```cpp
while (!glfwWindowShouldClose(...)) {
    zest_UpdateDevice(app.device);   // Update device state
    glfwPollEvents();                // Handle window events

    if (zest_BeginFrame(app.context)) {
        // Build or get cached frame graph...
        zest_EndFrame(app.context, frame_graph);  // Execute and present
    }
}
```

Every frame:

1. `zest_UpdateDevice()` - Updates device-level state
2. `zest_BeginFrame()` - Acquires swapchain image, returns false if window minimized
3. Build or get cached frame graph
4. `zest_EndFrame(context, frame_graph)` - Executes the graph and presents the frame

### 5. Frame Graph Caching

```cpp
zest_frame_graph_cache_key_t cache_key = zest_InitialiseCacheKey(app->context, 0, 0);
zest_frame_graph frame_graph = zest_GetCachedFrameGraph(app->context, &cache_key);
```

Frame graphs can be cached to avoid recompilation every frame. The cache key identifies a specific frame graph configuration.

### 6. Building the Frame Graph

```cpp
if (zest_BeginFrameGraph(app->context, "Render Graph", &cache_key)) {
    zest_ImportSwapchainResource();
    zest_BeginRenderPass("Draw Nothing"); {
        zest_ConnectSwapChainOutput();
        zest_SetPassTask(BlankScreen, 0);
        zest_EndPass();
    }
    frame_graph = zest_EndFrameGraph();
}
```

When building a frame graph:

1. **Import resources** - `zest_ImportSwapchainResource()` makes the swapchain available
2. **Define passes** - Each pass has inputs, outputs, and a task callback
3. **Connect resources** - `zest_ConnectSwapChainOutput()` declares this pass writes to the swapchain
4. **Set the task** - The callback function that records GPU commands
5. **End and compile** - `zest_EndFrameGraph()` compiles barriers and returns the graph

### 7. Execute the Frame Graph

```cpp
zest_EndFrame(app->context, frame_graph);
```

This executes the frame graph and presents the result to the window. The frame graph is passed directly to `zest_EndFrame()` which handles execution and presentation.

### 8. The Render Callback

```cpp
void BlankScreen(const zest_command_list command_list, void *user_data) {
    // Usually you'd have commands like:
    // zest_cmd_BindPipeline(command_list, pipeline);
    // zest_cmd_Draw(command_list, vertex_count, 1, 0, 0);
}
```

This callback receives a command list for recording GPU commands. The `user_data` parameter lets you pass your application state.

## What's Next?

Now that you understand the basic structure:

- [Architecture Overview](architecture.md) - Deeper explanation of Device, Context, and Frame Graph
- [Adding ImGui Tutorial](../tutorials/02-imgui.md) - Add a UI to your application
- [Frame Graph Concept](../concepts/frame-graph/index.md) - Full details on the frame graph system
