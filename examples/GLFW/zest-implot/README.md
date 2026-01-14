# ImPlot Example

Demonstrates integrating the ImPlot library with Zest for data visualization.

## What It Does

Shows how to use [ImPlot](https://github.com/epezent/implot), a plotting library for Dear ImGui, within a Zest application. Displays the ImPlot demo window with various chart types and interactive plots.

## Zest Features Used

- **ImGui Integration**: `zest_imgui_Initialise`, `zest_imgui_DarkStyle`
- **Font Texture**: `zest_imgui_RebuildFontTexture` for custom fonts
- **Frame Graph**: `zest_BeginFrameGraph`, `zest_imgui_BeginPass`
- **Swapchain**: `zest_ImportSwapchainResource`, `zest_ConnectSwapChainOutput`
- **Timer System**: 60 FPS update loop
- **Context Management**: Separate ImPlot context creation/destruction
