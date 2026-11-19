# ImPlot Example

Demonstrates integrating the ImPlot library with Zest for data visualization.

## What It Does

Shows how to use [ImPlot](https://github.com/epezent/implot), a plotting library for Dear ImGui, within a Zest application. Displays the ImPlot demo window with various chart types and interactive plots.

## Running

Run from the root of the Zest repository, as the font paths are relative to it.

- D3D12 is the default in a build configured with `ZEST_ENABLE_D3D12`, falling back to Vulkan if the D3D12 device can't be created. Otherwise it runs on Vulkan.
- `--vulkan` or `--d3d12` chooses the backend, with no fallback
- `--validate` turns on the validation layers and exits with 1 on any validation error
- `--validate-sync` adds Vulkan synchronisation validation, or D3D12 GPU-based validation, to `--validate`
- `--frames N` exits after N frames

## Zest Features Used

- **ImGui Integration**: `zest_imgui_Initialise`, `zest_imgui_DarkStyle`
- **Font Texture**: `zest_imgui_RebuildFontTexture` for custom fonts
- **Frame Graph**: `zest_BeginFrameGraph`, `zest_imgui_BeginPass`
- **Swapchain**: `zest_ImportSwapchainResource`, `zest_ConnectSwapChainOutput`
- **Timer System**: 60 FPS update loop
- **Context Management**: Separate ImPlot context creation/destruction
