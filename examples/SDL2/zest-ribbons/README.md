# Ribbon Rendering

Demonstrates GPU-based ribbon geometry generation using compute shaders.

## What It Does

Renders 10 animated 3D ribbons with different motion patterns (circular, figure-8, spiral, bouncing, Lissajous curves). Ribbon segments are updated on CPU, then a compute shader generates the tessellated vertex/index geometry on the GPU. Each ribbon has unique colors and width scaling.

## Running

Run from the root of the Zest repository, as the shader and texture paths are relative to it.

- D3D12 is the default in a build configured with `ZEST_ENABLE_D3D12`, falling back to Vulkan if the D3D12 device can't be created. Otherwise it runs on Vulkan.
- `--vulkan` or `--d3d12` chooses the backend, with no fallback
- `--validate` turns on the validation layers and exits with 1 on any validation error
- `--validate-sync` adds Vulkan synchronisation validation, or D3D12 GPU-based validation, to `--validate`
- `--frames N` exits after N frames

## Zest Features Used

- **Compute Shaders**: `zest_CreateCompute` for ribbon geometry generation
- **Frame Graph**: Transfer pass (segment data) -> Compute pass (geometry) -> Render pass
- **Transient Buffers**: `zest_AddTransientBufferResource` for segment, instance, vertex, and index buffers
- **Storage Buffers**: GPU-side geometry generation with bindless buffer access
- **Staging Buffers**: `zest_CreateStagingBuffer` for per-frame segment uploads
- **Custom Pipeline**: Ribbon vertex attributes (position, UV, color)
- **Uniform Buffers**: Camera view/projection matrices
- **Push Constants**: Tessellation settings, UV scale, width multiplier
- **Bindless Descriptors**: `zest_GetTransientBufferBindlessIndex` for compute buffer access
- **ImGui Integration**: Texture scale, offset, and width controls
- **Camera Controls**: Free-look with WASD movement

## Controls

- **Space**: Animate ribbons
- **N**: Step single frame
- **Right Mouse + WASD**: Move camera
- **Right Mouse + Mouse Move**: Look around
