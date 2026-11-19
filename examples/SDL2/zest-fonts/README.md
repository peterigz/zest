# MSDF Fonts Example

Demonstrates rendering high-quality text using Multi-channel Signed Distance Field (MSDF) fonts.

## What It Does

Generates MSDF font data from a TTF file using the msdf.h single-header library, then renders crisp text at any scale. The font data is cached to `.msdf` files for faster subsequent loads. Includes ImGui controls for adjusting font rendering parameters like shadow, smoothness, and bias.

## Running

Run from the root of the Zest repository, as the font and shader paths are relative to it.

- D3D12 is the default in a build configured with `ZEST_ENABLE_D3D12`, falling back to Vulkan if the D3D12 device can't be created. Otherwise it runs on Vulkan.
- `--vulkan` or `--d3d12` chooses the backend, with no fallback
- `--validate` turns on the validation layers and exits with 1 on any validation error
- `--validate-sync` adds Vulkan synchronisation validation, or D3D12 GPU-based validation, to `--validate`
- `--frames N` exits after N frames

## Zest Features Used

- **MSDF Font System**: `zest_CreateMSDF`, `zest_LoadMSDF`, `zest_SaveMSDF`
- **Font Layer**: `zest_CreateFontLayer`, `zest_CreateFontResources`
- **Font Rendering**: `zest_SetMSDFFontDrawing`, `zest_DrawMSDFText`
- **Frame Graph**: Transfer pass for font layer upload, render pass for drawing
- **Frame Graph Caching**: `zest_InitialiseCacheKey`, `zest_GetCachedFrameGraph`
- **Transient Resources**: `zest_AddTransientLayerResource` for font layer
- **ImGui Integration**: Parameter tweaking UI
- **Timer System**: 60 FPS update loop for ImGui

## Controls

- **Font Size**: Drag slider to scale text
- **Unit Range**: Adjust MSDF rendering range
- **Shadow Offset/Color**: Configure drop shadow
- **Smoothness/Gamma**: Fine-tune antialiasing
