# Tutorial 2: Adding ImGui

This tutorial shows how to integrate Dear ImGui with Zest for immediate-mode UI rendering.

**Example:** `examples/SDL2/zest-imgui-template`

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
#include <SDL.h>
#include <zest.h>

// ImGui implementation
#include <imgui.h>
#include <impl_imgui.h>
#include <imgui_impl_sdl2.h>
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
    zest_imgui_Initialise(app->context, &app->imgui, zest_implsdl2_DestroyWindow);

    // Initialize ImGui for SDL2
    ImGui_ImplSDL2_InitForVulkan((SDL_Window*)zest_Window(app->context));

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
    int running = 1;
    SDL_Event event;

    while (running) {
        while (SDL_PollEvent(&event)) {
            ImGui_ImplSDL2_ProcessEvent(&event);
            if (event.type == SDL_QUIT) running = 0;
        }

        zest_UpdateDevice(app->device);

        // Fixed timestep loop for game logic
	// This is optional and just shows that you can use a timer to only update imgui a maximum number of times per second
        zest_StartTimerLoop(app->timer) {
            // Start ImGui frame
            ImGui_ImplSDL2_NewFrame();
            ImGui::NewFrame();

            // Your UI code
            DrawUI(app);

            // Finalize ImGui frame
            ImGui::Render();
        } zest_EndTimerLoop(app->timer);

        // Render
        if (zest_BeginFrame(app->context)) {
            // ... build/get frame graph ...
            zest_EndFrame(app->context, frame_graph);
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
    zest_imgui_Initialise(app->context, &app->imgui, zest_implsdl2_DestroyWindow);
    ImGui_ImplSDL2_InitForVulkan((SDL_Window*)zest_Window(app->context));

    // Enable docking
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
}

void DrawUI(app_t *app) {
    // Create dockspace over entire window
    ImGui::DockSpaceOverViewport(ImGui::GetMainViewport()->ID, ImGui::GetMainViewport(), ImGuiDockNodeFlags_PassthruCentralNode);

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

ImGui automatically captures input via `ImGui_ImplSDL2_InitForVulkan`. Check if ImGui wants input:

```cpp
void HandleInput(app_t *app) {
    ImGuiIO& io = ImGui::GetIO();

    // Don't process game input if ImGui wants it
    if (io.WantCaptureMouse) {
        return;
    }

    // Your game input handling using SDL2
    int mouse_x, mouse_y;
    Uint32 buttons = SDL_GetMouseState(&mouse_x, &mouse_y);
    if (buttons & SDL_BUTTON(SDL_BUTTON_LEFT)) {
        // Handle click
    }
}
```

## Complete Example

See the full source at `examples/SDL2/zest-imgui-template/zest-imgui-template.cpp`.

## Next Steps

- [Tutorial 3: GPU Instancing](03-instancing.md) - Render many objects efficiently
- [ImGui Documentation](https://github.com/ocornut/imgui) - Official ImGui docs
