# MSDF Fonts Example

Demonstrates rendering high-quality text using Multi-channel Signed Distance Field (MSDF) fonts.

## What It Does

Generates MSDF font data from a TTF file using the msdf.h single-header library, then renders crisp text at any scale. The font data is cached to `.msdf` files for faster subsequent loads. Includes ImGui controls for adjusting font rendering parameters like shadow, smoothness, and bias.

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
