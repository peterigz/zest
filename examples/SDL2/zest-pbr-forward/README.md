# PBR Forward Rendering

Physically-based rendering with image-based lighting using forward shading.

## What It Does

Recreated from [Sascha Willems' PBR IBL example](https://github.com/SaschaWillems/Vulkan/tree/master/examples/pbribl). Renders multiple GLTF models (teapot, torus, venus, cube, sphere) with varying metallic/roughness values in a single forward pass. Uses environment-based IBL with irradiance and prefiltered cubemaps for realistic reflections.

## Running

Run from the root of the Zest repository, as the shader and texture paths are relative to it.

- D3D12 is the default in a build configured with `ZEST_ENABLE_D3D12`, falling back to Vulkan if the D3D12 device can't be created. Otherwise it runs on Vulkan.
- `--vulkan` or `--d3d12` chooses the backend, with no fallback
- `--validate` turns on the validation layers and exits with 1 on any validation error
- `--validate-sync` adds Vulkan synchronisation validation, or D3D12 GPU-based validation, to `--validate`
- `--frames N` exits after N frames

## Zest Features Used

- **Frame Graph**: Transfer pass + render pass with depth buffer
- **IBL Generation**: BRDF LUT, irradiance cubemap, prefiltered cubemap (compute-generated)
- **Pipeline Templates**: PBR forward pipeline and skybox pipeline
- **KTX Textures**: `zest_LoadKTX` for environment cubemap
- **GLTF Loading**: Multiple mesh loading with `LoadGLTFScene`
- **Instance Mesh Layer**: `zest_CreateInstanceMeshLayer`, `zest_AddMeshToLayer`
- **Uniform Buffers**: View/projection and animated lights
- **Push Constants**: IBL texture indices, material properties
- **Bindless Descriptors**: Environment cubemap, BRDF LUT, prefiltered cubemap
- **Transient Resources**: Depth buffer via `zest_AddTransientImageResource`
- **ImGui Integration**: Debug controls

## Controls

- **Right Mouse + WASD**: Move camera
- **Right Mouse + Mouse Move**: Look around
