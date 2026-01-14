# Dear ImGui Template

Shows how to integrate Dear ImGui with Zest, including docking and multiple viewport support.

## What It Does

Sets up a complete ImGui environment with:
- Docking workspace using `ImGui::DockSpaceOverViewport`
- Multiple viewport support for dragging ImGui windows outside the main window
- Custom font loading and texture rebuilding
- ImGui demo window for reference

## Zest Features Used

- **ImGui Integration**: `zest_imgui_Initialise`, `zest_imgui_DarkStyle`
- **Font Texture**: `zest_imgui_RebuildFontTexture` for custom fonts
- **Viewport Rendering**: `zest_imgui_RenderViewport` for main viewport
- **Multi-Viewport**: `ImGui::RenderPlatformWindowsDefault` with Zest imgui context
- **Frame Graph**: Simple ImGui-only render pass
- **Timer System**: 60 FPS update rate for ImGui
- **Window Callbacks**: `zest_implglfw_DestroyWindow` for viewport cleanup
