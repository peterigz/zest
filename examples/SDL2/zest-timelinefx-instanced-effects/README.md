# TimelineFX Instanced Effects

Pre-recorded particle effect playback using GPU compute shaders for maximum performance.

## What It Does

Instead of simulating particles in real-time, this example plays back pre-recorded effect animations. The sprite data is pre-baked and uploaded once, then a compute shader builds the per-frame sprite buffer from animation instance data. This allows rendering hundreds of thousands of particles with minimal CPU overhead. Includes frustum culling in the compute shader.

## Zest Features Used

- **Compute Shaders**: `zest_CreateCompute` for sprite buffer construction
- **Storage Buffers**: Sprite data, emitter properties, image data, animation instances, output sprite buffer
- **Frame Graph**: Transfer pass -> Compute pass -> Render pass
- **Bindless Buffers**: Storage buffer indices passed via push constants
- **Staging Buffers**: One-time upload of pre-recorded animation data
- **Texture Arrays**: Particle sprite sheets
- **Frustum Culling**: `zest_CalculateFrustumPlanes` with GPU-based visibility testing
- **Custom Pipeline**: Billboard vertex shader for sprite rendering

## Performance

This approach is ideal for games that need many simultaneous effects with predictable performance. The compute shader handles:
- Sprite interpolation between keyframes
- Billboard transformation
- Texture coordinate lookup
- Visibility culling
