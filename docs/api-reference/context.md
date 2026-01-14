# Context API

Functions for context (window/swapchain) management.

## Creation

### zest_CreateContextInfo

Create default context configuration.

```cpp
zest_create_context_info_t zest_CreateContextInfo(void);
```

---

### zest_CreateContext

Create a context for a window.

```cpp
zest_context zest_CreateContext(
    zest_device device,
    zest_window_data_t *window_data,
    zest_create_context_info_t *create_info
);
```

---

## Frame Management

### zest_BeginFrame

Begin a new frame. Returns false if window is minimized.

```cpp
zest_bool zest_BeginFrame(zest_context context);
```

---

### zest_EndFrame

End frame, submit commands, and present.

```cpp
void zest_EndFrame(zest_context context);
```

---

## Window Queries

### zest_ScreenWidth / zest_ScreenHeight

```cpp
zest_uint zest_ScreenWidth(zest_context context);
zest_uint zest_ScreenHeight(zest_context context);
float zest_ScreenWidthf(zest_context context);
float zest_ScreenHeightf(zest_context context);
```

---

### zest_Window

Get native window handle.

```cpp
void* zest_Window(zest_context context);
```

---

### zest_DPIScale

```cpp
float zest_DPIScale(zest_context context);
```

---

## Swapchain

### zest_GetSwapchain

```cpp
zest_swapchain zest_GetSwapchain(zest_context context);
```

---

### zest_SetSwapchainClearColor

```cpp
void zest_SetSwapchainClearColor(zest_context context, float r, float g, float b, float a);
```

---

### zest_SwapchainWasRecreated

Check if swapchain was recreated (window resize).

```cpp
zest_bool zest_SwapchainWasRecreated(zest_context context);
```

---

## VSync

### zest_EnableVSync / zest_DisableVSync

```cpp
void zest_EnableVSync(zest_context context);
void zest_DisableVSync(zest_context context);
```

---

## See Also

- [Device API](device.md)
- [Device & Context Concept](../concepts/device-and-context.md)
