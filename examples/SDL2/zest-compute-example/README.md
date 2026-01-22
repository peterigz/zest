# Compute Particles Example

Attraction-based particle system using compute shaders to update particle positions on the GPU.

## What It Does

Recreated from [Sascha Willems' Vulkan compute particles example](https://github.com/SaschaWillems/Vulkan/tree/master/examples/computeparticles). Simulates 256K particles attracted to a moving point (automated or mouse-controlled). Particles are rendered as point sprites with additive blending, colored by sampling a gradient texture.

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
