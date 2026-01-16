# Context API

Functions for context (window/swapchain) management. A context represents a single window and its associated swapchain, frame resources, and linear allocators. One device can serve multiple contexts for multi-window applications.

Generally speaking, if you're creating a game or app that opens a single window then you'll only ever use a single context, but there is scope for having multiple contexts such as using Dear ImGui and being able to drag imgui windows outside of the main window. This will create a new context to draw just that window.

## Creation

### zest_CreateContextInfo

Creates a default context configuration struct with sensible defaults.

```cpp
zest_create_context_info_t zest_CreateContextInfo();
```

Use this to get a pre-configured struct, then modify specific fields as needed before passing to `zest_CreateContext`. The struct contains settings for window title, dimensions, vsync, frame allocator sizes, and more.

**Example:**
```cpp
zest_create_context_info_t create_info = zest_CreateContextInfo();
ZEST__FLAG(create_info.flags, zest_context_init_flag_enable_vsync);  // Enable vsync
create_info.screen_width = 1920;
create_info.screen_height = 1080;
```

---

### zest_CreateContext

Creates a context for a window, initializing the swapchain and frame resources.

```cpp
zest_context zest_CreateContext(
    zest_device device,
    zest_window_data_t *window_data,
    zest_create_context_info_t *info
);
```

Call this after creating a device and window. The window data is typically obtained from helper functions like `zest_implglfw_CreateWindow` or `zest_implsdl_CreateWindow` but you can also create your own window data if you're using any other library or native OS commands to open a window.

**Example:**
```cpp
zest_device device = zest_implglfw_CreateVulkanDevice(false);
zest_window_data_t window = zest_implglfw_CreateWindow(50, 50, 1280, 768, 0, "My App");
zest_create_context_info_t info = zest_CreateContextInfo();
zest_context context = zest_CreateContext(device, &window, &info);
```

---

## Frame Management

### zest_BeginFrame

Begins a new frame by acquiring a swapchain image.

```cpp
zest_bool zest_BeginFrame(zest_context context);
```

Returns `ZEST_FALSE` if the window is minimized or the swapchain image couldn't be acquired (in which case you should skip rendering). Always pair with `zest_EndFrame` when this returns true.

This function handles frame synchronization by waiting on the semaphore for the oldest frame in flight (based on `ZEST_MAX_FIF`) before proceeding. It also advances the current frame-in-flight index, which determines which set of per-frame resources (uniform buffers, command buffers, etc.) are used.

**Important:** Any operations that depend on the current frame-in-flight index must be called between `zest_BeginFrame` and `zest_EndFrame`. This includes:
- Updating uniform buffers
- Updating layers and mesh layers
- Recording command buffers
- Any per-frame resource updates

Calling these functions outside of a begin/end frame pair will use an incorrect frame index and lead to synchronization issues or other strange behaviour like shaders reading from and incorrect buffer.

**Example:**
```cpp
if (zest_BeginFrame(context)) {
    // Safe to update per-frame resources here
    UpdateUniformBuffer(my_uniform_buffer);
    zest_UpdateLayer(my_layer);

    // Build and execute frame graph
    zest_QueueFrameGraphForExecution(context, frame_graph);
    zest_EndFrame(context);
}
```

---

### zest_EndFrame

Ends the frame, submits queued command buffers, and presents to the window.

```cpp
void zest_EndFrame(zest_context context);
```

Executes any queued frame graphs and presents the rendered image to the screen. Must be called after `zest_BeginFrame` returns true.

**Example:**
```cpp
if (zest_BeginFrame(context)) {
    zest_QueueFrameGraphForExecution(context, frame_graph);
    zest_EndFrame(context);
}
```

---

## Window Queries

### zest_ScreenWidth / zest_ScreenHeight

Gets the current window/swapchain dimensions in pixels.

```cpp
zest_uint zest_ScreenWidth(zest_context context);
zest_uint zest_ScreenHeight(zest_context context);
float zest_ScreenWidthf(zest_context context);
float zest_ScreenHeightf(zest_context context);
```

The `f` variants return floats, useful for viewport calculations and shader uniforms where float arithmetic is needed.

**Example:**
```cpp
// Set up orthographic projection for 2D rendering
float proj_x = 2.0f / zest_ScreenWidthf(context);
float proj_y = 2.0f / zest_ScreenHeightf(context);

// Calculate aspect ratio
float aspect = zest_ScreenWidthf(context) / zest_ScreenHeightf(context);
```

---

### zest_Window

Gets the native window handle (GLFW, SDL, etc.).

```cpp
void* zest_Window(zest_context context);
```

Cast the result to your windowing library's handle type. Useful for input handling, window state queries, or platform-specific operations.

**Example:**
```cpp
// With GLFW
GLFWwindow *window = (GLFWwindow*)zest_Window(context);
while (!glfwWindowShouldClose(window)) {
    glfwPollEvents();
    // render...
}

// With SDL
SDL_Window *window = (SDL_Window*)zest_Window(context);
```

---

### zest_DPIScale

Gets the DPI scale factor for the context's window.

```cpp
float zest_DPIScale(zest_context context);
```

Returns the ratio of physical pixels to logical pixels. Use this to scale UI elements appropriately on high-DPI displays (Retina, 4K monitors, etc.).

**Example:**
```cpp
float scale = zest_DPIScale(context);
float font_size = 16.0f * scale;  // Scale font for high-DPI
```

---

## Swapchain

### zest_GetSwapchain

Gets the swapchain handle for advanced operations.

```cpp
zest_swapchain zest_GetSwapchain(zest_context context);
```

Returns the underlying swapchain object. Useful when you need to query swapchain properties like format or create compatible render targets.

**Example:**
```cpp
zest_swapchain swapchain = zest_GetSwapchain(context);
zest_format format = zest_GetSwapchainFormat(swapchain);
```

---

### zest_SetSwapchainClearColor

Sets the clear color used when the swapchain is cleared.

```cpp
void zest_SetSwapchainClearColor(zest_context context, float red, float green, float blue, float alpha);
```

The color is used as the background when rendering to the swapchain. Components are in the range 0.0 to 1.0.

**Example:**
```cpp
// Dark gray background
zest_SetSwapchainClearColor(context, 0.1f, 0.1f, 0.1f, 1.0f);

// Cornflower blue (classic game background)
zest_SetSwapchainClearColor(context, 0.39f, 0.58f, 0.93f, 1.0f);
```

---

### zest_SwapchainWasRecreated

Checks if the swapchain was recreated this frame (due to window resize).

```cpp
zest_bool zest_SwapchainWasRecreated(zest_context context);
```

Returns `ZEST_TRUE` if the swapchain was recreated. Use this to invalidate cached frame graphs or recreate screen-sized resources.

**Example:**
```cpp
if (zest_SwapchainWasRecreated(context)) {
    // Flush cached frame graphs that may reference old screen-size resources
    zest_FlushCachedFrameGraphs(context);
    RecreateScreenSizeBuffers();
}
```

---

## VSync

### zest_EnableVSync / zest_DisableVSync

Controls vertical synchronization for the context.

```cpp
void zest_EnableVSync(zest_context context);
void zest_DisableVSync(zest_context context);
```

VSync limits the frame rate to the display's refresh rate, eliminating screen tearing but potentially adding input latency. These functions trigger a swapchain recreation.

**Example:**
```cpp
// Toggle vsync based on user preference
if (user_wants_vsync) {
    zest_EnableVSync(context);
} else {
    zest_DisableVSync(context);
}
```

---

## See Also

- [Device API](device.md)
- [Device & Context Concept](../concepts/device-and-context.md)
