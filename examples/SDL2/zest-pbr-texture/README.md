# PBR Textured Model

Physically-based rendering with texture maps using metal/roughness workflow.

## What It Does

Recreated from [Sascha Willems' PBR texture example](https://github.com/SaschaWillems/Vulkan/tree/master/examples/pbrtexture). Renders the Cerberus gun model with full PBR texture set (albedo, normal, ambient occlusion, metallic, roughness). Demonstrates texture-based material definition rather than per-vertex/instance properties.

## Zest Features Used

- **Frame Graph**: Transfer pass + render pass with depth buffer
- **KTX Textures**: `zest_LoadKTX` for all PBR texture maps
- **IBL Generation**: BRDF LUT, irradiance cubemap, prefiltered cubemap
- **Pipeline Templates**: PBR texture pipeline and skybox pipeline
- **GLTF Loading**: `LoadGLTFScene` for Cerberus gun model
- **Instance Mesh Layer**: `zest_CreateInstanceMeshLayer`
- **Push Constants**: All texture indices (albedo, normal, AO, metallic, roughness, IBL maps)
- **Bindless Descriptors**: Multiple 2D textures and cubemaps
- **Uniform Buffers**: View/projection and lighting parameters
- **ImGui Integration**: Gamma and exposure controls

## Controls

- **Right Mouse + WASD**: Move camera
- **Right Mouse + Mouse Move**: Look around
- **Exposure/Gamma sliders**: Adjust tone mapping
