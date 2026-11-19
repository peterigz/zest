# Compute Particles Example

Attraction-based particle system using compute shaders to update particle positions on the GPU.

## What It Does

Recreated from [Sascha Willems' Vulkan compute particles example](https://github.com/SaschaWillems/Vulkan/tree/master/examples/computeparticles). Simulates 256K particles attracted to a moving point (automated or mouse-controlled). Particles are rendered as point sprites with additive blending, colored by sampling a gradient texture.

## Running

Run from the root of the Zest repository, as the shader and texture paths are relative to it.

- D3D12 is the default in a build configured with `ZEST_ENABLE_D3D12`, falling back to Vulkan if the D3D12 device can't be created. Otherwise it runs on Vulkan.
- `--vulkan` or `--d3d12` chooses the backend, with no fallback
- `--validate` turns on the validation layers and exits with 1 on any validation error
- `--validate-sync` adds Vulkan synchronisation validation, or D3D12 GPU-based validation, to `--validate`
- `--frames N` exits after N frames

## Zest Features Used

- **Compute Shaders**: `zest_CreateCompute`, `zest_cmd_BindComputePipeline`, `zest_cmd_DispatchCompute`
- **Frame Graph**: Compute pass connected to render pass via buffer resource
- **Storage Buffers**: `zest_CreateBuffer` with `zest_buffer_type_vertex_storage`
- **Bindless Descriptors**: `zest_AcquireStorageBufferIndex`, `zest_AcquireSampledImageIndex`
- **Pipeline Templates**: Custom point sprite pipeline with `zest_topology_point_list`
- **Uniform Buffers**: Per-frame compute parameters
- **Slang Shaders**: `zest_slang_CreateShader` for shader compilation
- **ImGui Integration**: Debug UI with particle controls
- **Staging Buffers**: `zest_CreateStagingBuffer` for initial particle data upload

## Controls

- **Repel Mouse checkbox**: Toggle between automated animation and mouse-controlled attraction point
