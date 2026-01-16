# Tutorial 2: Adding ImGui

This tutorial shows how to integrate Dear ImGui with Zest for immediate-mode UI rendering.

**Example:** `examples/GLFW/zest-imgui-template`

## What You'll Learn

- Setting up ImGui with Zest
- Custom fonts
- ImGui render pass integration
- Docking support

## Prerequisites

- Completed [Tutorial 1](01-minimal-template.md)
- ImGui submodule (included with Zest)

## Additional Includes

```cpp
#define ZEST_IMPLEMENTATION
#define ZEST_VULKAN_IMPLEMENTATION
#include <GLFW/glfw3.h>
#include <zest.h>

// ImGui implementation
#include <imgui.h>
#include <impl_imgui.h>
#include <imgui_impl_glfw.h>
```

## Application Structure

```cpp
struct app_t {
    zest_device device;
    zest_context context;
    zest_imgui_t imgui;         // ImGui state
    zest_timer_t timer;         // For fixed timestep
};
```

## Step 1: Initialize ImGui

```cpp
void InitImGui(app_t *app) {
    // Initialize Zest's ImGui integration
    zest_imgui_Initialise(app->context, &app->imgui, zest_implglfw_DestroyWindow);

    // Initialize ImGui for GLFW
    ImGui_ImplGlfw_InitForVulkan((GLFWwindow*)zest_Window(app->context), true);

    // Apply dark style, you can copy this function and setup your own colors
    zest_imgui_DarkStyle(&app->imgui);
}
```

## Step 2: Custom Fonts (Optional)

```cpp
void SetupFonts(app_t *app) {
    ImGuiIO& io = ImGui::GetIO();
    io.Fonts->Clear();

    // Load custom font
    io.Fonts->AddFontFromFileTTF("assets/fonts/Roboto-Regular.ttf", 16.0f);

    // Build font atlas
    unsigned char* font_data;
    int tex_width, tex_height;
    io.Fonts->GetTexDataAsRGBA32(&font_data, &tex_width, &tex_height);

    // Upload to GPU
    zest_imgui_RebuildFontTexture(&app->imgui, tex_width, tex_height, font_data);
}
```

## Step 3: ImGui in the Main Loop

```cpp
void MainLoop(app_t *app) {
    while (!glfwWindowShouldClose(...)) {
        zest_UpdateDevice(app->device);
        glfwPollEvents();

        // Fixed timestep loop for game logic
	// This is optional and just shows that you can use a timer to only update imgui a maximum number of times per second
        zest_StartTimerLoop(app->timer) {
            // Start ImGui frame
            ImGui_ImplGlfw_NewFrame();
            ImGui::NewFrame();

            // Your UI code
            DrawUI(app);

            // Finalize ImGui frame
            ImGui::Render();
        } zest_EndTimerLoop(app->timer);

        // Render
        if (zest_BeginFrame(app->context)) {
            // ... render frame graph ...
            zest_EndFrame(app->context);
        }
    }
}
```

## Step 4: Building the Frame Graph with ImGui

```cpp
zest_frame_graph BuildFrameGraph(app_t *app, zest_frame_graph_cache_key_t *cache_key) {
    if (zest_BeginFrameGraph(app->context, "ImGui Graph", cache_key)) {
        zest_ImportSwapchainResource();

        // Your render passes first...
        zest_BeginRenderPass("Scene"); {
            zest_ConnectSwapChainOutput();
            zest_SetPassTask(RenderScene, app);
            zest_EndPass();
        }

        // ImGui pass last (renders on top)
        zest_pass_node imgui_pass = zest_imgui_BeginPass(&app->imgui, app->imgui.main_viewport);
        if (imgui_pass) {
            zest_ConnectSwapChainOutput();
            zest_EndPass();
        }

        return zest_EndFrameGraph();
    }
    return 0;
}
```

!!! note
    `zest_imgui_BeginPass` handles pass setup and callback assignment internally.

## Step 5: Drawing UI

```cpp
void DrawUI(app_t *app) {
    // Main menu bar
    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("Exit")) {
                // Handle exit
            }
            ImGui::EndMenu();
        }
        ImGui::EndMainMenuBar();
    }

    // Debug window
    ImGui::Begin("Debug");
    ImGui::Text("Frame time: %.3f ms", zest_TimerDeltaTime(&app->timer) * 1000.0f);
    ImGui::Text("FPS: %.1f", 1.0f / zest_TimerDeltaTime(&app->timer));

    if (ImGui::Button("Reset")) {
        // Handle reset
    }

    ImGui::End();
}
```

## Docking Support

Enable docking for advanced layouts:

```cpp
void InitImGui(app_t *app) {
    zest_imgui_Initialise(app->context, &app->imgui, zest_implglfw_DestroyWindow);
    ImGui_ImplGlfw_InitForVulkan((GLFWwindow*)zest_Window(app->context), true);

    // Enable docking
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
}

void DrawUI(app_t *app) {
    // Create dockspace over entire window
    ImGui::DockSpaceOverViewport(ImGui::GetMainViewport());

    // Now windows can be docked
    ImGui::Begin("Panel 1");
    ImGui::Text("Dockable panel");
    ImGui::End();

    ImGui::Begin("Panel 2");
    ImGui::Text("Another panel");
    ImGui::End();
}
```

## Handling Input

ImGui automatically captures input via `ImGui_ImplGlfw_InitForVulkan`. Check if ImGui wants input:

```cpp
void HandleInput(app_t *app) {
    ImGuiIO& io = ImGui::GetIO();

    // Don't process game input if ImGui wants it
    if (io.WantCaptureMouse) {
        return;
    }

    // Your game input handling
    if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS) {
        // Handle click
    }
}
```

## Complete Example

```cpp
#define ZEST_IMPLEMENTATION
#define ZEST_VULKAN_IMPLEMENTATION
#include <GLFW/glfw3.h>
#include <zest.h>
#include <imgui.h>
#include <impl_imgui.h>
#include <imgui_impl_glfw.h>

struct app_t {
    zest_device device;
    zest_context context;
    zest_imgui_t imgui;
    zest_timer_t timer;
	float clear_color[3];
    float value = 0.5f;
};

void DrawUI(app_t *app) {
    ImGui::Begin("Controls");
    ImGui::SliderFloat("Value", &app->value, 0.0f, 1.0f);
    ImGui::ColorEdit3("Clear Color", app->clear_color);
    ImGui::End();
}

void MainLoop(app_t *app) {
    while (!glfwWindowShouldClose((GLFWwindow*)zest_Window(app->context))) {
        zest_UpdateDevice(app->device);
        glfwPollEvents();

        zest_StartTimerLoop(app->timer) {
            ImGui_ImplGlfw_NewFrame();
            ImGui::NewFrame();
            DrawUI(app);
            ImGui::Render();
        } zest_EndTimerLoop(app->timer);

        if (zest_BeginFrame(app->context)) {
            zest_frame_graph_cache_key_t key = zest_InitialiseCacheKey(app->context, 0, 0);
            zest_frame_graph graph = zest_GetCachedFrameGraph(app->context, &key);

			zest_SetSwapchainClearColor(app->context, app->clear_color[0], app->clear_color[1], app->clear_color[2], 1.f);

            if (!graph) {
                if (zest_BeginFrameGraph(app->context, "Graph", &key)) {
                    zest_ImportSwapchainResource();

                    zest_pass_node imgui_pass = zest_imgui_BeginPass(&app->imgui, app->imgui.main_viewport);
                    if (imgui_pass) {
                        zest_ConnectSwapChainOutput();
                        zest_EndPass();
                    }

                    graph = zest_EndFrameGraph();
                }
            }

            zest_QueueFrameGraphForExecution(app->context, graph);
            zest_EndFrame(app->context);
        }
    }
}

int main() {
    glfwInit();

    app_t app = {};
    app.device = zest_implglfw_CreateVulkanDevice(false);

    zest_window_data_t window = zest_implglfw_CreateWindow(50, 50, 1280, 768, 0, "ImGui Example");
    zest_create_context_info_t info = zest_CreateContextInfo();
    app.context = zest_CreateContext(app.device, &window, &info);

    zest_imgui_Initialise(app.context, &app.imgui, zest_implglfw_DestroyWindow);
    ImGui_ImplGlfw_InitForVulkan((GLFWwindow*)zest_Window(app.context), true);
    zest_imgui_DarkStyle(&app.imgui);

    app.timer = zest_CreateTimer(60.0);

    MainLoop(&app);
	ImGui_ImplGlfw_Shutdown();
	zest_imgui_Destroy(&app.imgui);
    zest_DestroyDevice(app.device);

    return 0;
}
```

## Next Steps

- [Tutorial 3: GPU Instancing](03-instancing.md) - Render many objects efficiently
- [ImGui Documentation](https://github.com/ocornut/imgui) - Official ImGui docs
