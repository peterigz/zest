# Cascading Shadows Example

Cascaded shadow mapping for directional light sources. Based on [Sascha Willems' Vulkan tutorial](https://github.com/SaschaWillems/Vulkan/tree/master/examples/shadowmappingcascade).

## What It Does

- Renders a scene with multiple oak trees on terrain using cascaded shadow mapping
- Supports 4 shadow map cascades for high-quality shadows across different distances
- Includes PCF (Percentage Closer Filtering) for soft shadow edges
- Provides debug visualization to inspect individual cascade levels
- Animates the light source to show dynamic shadow updates

## Zest Features Used

- **Frame Graph** - Multi-pass rendering with automatic barrier management (`zest_BeginFrameGraph`, render/transfer passes)
- **Frame Graph Caching** - Caches compiled frame graphs with `zest_InitialiseCacheKey` based on render state
- **Transient Resources** - Shadow map and depth buffer as frame-lifetime resources (`zest_AddTransientImageResource`)
- **Pipeline Templates** - Custom pipelines for scene rendering, shadow depth, and debug display
- **Multi-view Rendering** - Single draw call renders to all 4 cascade layers (`zest_SetPipelineViewCount`)
- **Instanced Mesh Layers** - Efficient instanced rendering of multiple tree copies
- **Bindless Descriptors** - Push constants reference uniform buffers and textures by descriptor index
- **Uniform Buffers** - View/projection matrices, cascade matrices, and fragment shader data
- **GLTF Loading** - Loads oak tree and terrain models via `zest_utilities.h`
- **ImGui Integration** - Interactive controls for PCF, cascade visualization, and settings

## Controls

- **Right Mouse + WASD** - Camera movement
- **PCF Filtering** - Toggle soft shadows
- **Color Cascades** - Visualize cascade boundaries
- **Display Shadow Map** - View raw shadow map cascades
