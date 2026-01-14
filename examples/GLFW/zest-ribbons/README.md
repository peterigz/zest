# Ribbon Rendering

Demonstrates GPU-based ribbon geometry generation using compute shaders.

## What It Does

Renders 10 animated 3D ribbons with different motion patterns (circular, figure-8, spiral, bouncing, Lissajous curves). Ribbon segments are updated on CPU, then a compute shader generates the tessellated vertex/index geometry on the GPU. Each ribbon has unique colors and width scaling.

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
