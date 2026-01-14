# SDL2 ImGui Template

Demonstrates using SDL2 instead of GLFW, plus async image loading in a separate thread.

## What It Does

Shows how to:
- Use SDL2 for window creation and event handling
- Load images asynchronously without blocking the main thread
- Handle thread-safe texture updates with atomic flags
- Switch between windowed, borderless, and fullscreen modes
- Integrate Dear ImGui with SDL2 backend

## Zest Features Used

- **SDL2 Integration**: `zest_implsdl2_CreateDevice`, `zest_implsdl2_CreateWindow`
- **Window Modes**: `zest_implsdl2_SetWindowMode` (bordered, borderless, fullscreen)
- **Async Image Loading**: `zest_CreateImageWithPixels` in background thread
- **Image Activation**: Thread-safe staging image pattern with atomic flags
- **Frame Graph**: ImGui-only render pass with frame graph caching
- **Pipeline Templates**: `zest_CopyPipelineTemplate` for custom ImGui sprite rendering
- **Samplers**: `zest_CreateSampler`, `zest_AcquireSamplerIndex`
- **Timer System**: 60 FPS update loop for ImGui
- **VSync Control**: `zest_EnableVSync`, `zest_DisableVSync`

## Thread Safety

The example demonstrates safe async resource loading:
1. Load image in background thread using `zest_CreateImageWithPixels`
2. Set atomic flag when loading complete
3. Main thread checks flag in `zest_BeginFrame` and activates the image
4. Old image is safely freed after activation

## Controls

- **Toggle VSync button**: Enable/disable vertical sync
- **Print Render Graph button**: Output frame graph structure
- **Window mode buttons**: Switch between bordered, borderless, fullscreen modes
