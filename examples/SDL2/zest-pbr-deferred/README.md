# PBR Deferred Rendering

Physically-based rendering with image-based lighting using deferred shading.

## What It Does

Based on [Sascha Willems' PBR IBL example](https://github.com/SaschaWillems/Vulkan/tree/master/examples/pbribl), but implements deferred rendering instead of forward. Renders multiple GLTF models (teapot, torus, venus, cube, sphere) with varying metallic/roughness values. Uses a G-buffer pass to output position, normal, albedo, and material properties, followed by a fullscreen lighting pass.

## Zest Features Used

- **Frame Graph**: Multi-pass rendering with G-buffer, lighting, and composite passes
- **Transient Resources**: `zest_AddTransientImageResource` for G-buffer attachments
- **Multiple Render Targets**: G-buffer with position, normal, albedo, material outputs
- **Pipeline Templates**: G-buffer, lighting (fullscreen), skybox, and composite pipelines
- **IBL Textures**: BRDF LUT, irradiance cubemap, prefiltered cubemap generation
- **KTX Textures**: `zest_LoadKTX` for environment cubemap
- **GLTF Loading**: Multiple mesh loading with `LoadGLTFScene`
- **Instance Mesh Layer**: `zest_CreateInstanceMeshLayer` for instanced rendering
- **Uniform Buffers**: View/projection and lights data
- **Push Constants**: Material properties and texture indices
- **Bindless Descriptors**: Cubemap and 2D texture indices

## Controls

- **Right Mouse + WASD**: Move camera
- **Right Mouse + Mouse Move**: Look around
