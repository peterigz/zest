# TimelineFX Pre-recorded Effects

Load and play pre-recorded particle effect animations from `.tfxsd` files.

## What It Does

Loads pre-baked particle animation data exported from the TimelineFX Editor. Unlike real-time simulation, pre-recorded effects store all sprite data for every frame of an animation. A compute shader builds the renderable sprite buffer each frame based on which animation instances are playing and their current playback time.

## Zest Features Used

- **Compute Shaders**: `zest_CreateCompute` for sprite data playback
- **Storage Buffers**: Pre-recorded sprite data, emitter properties, GPU image data
- **Frame Graph**: Compute pass -> Render pass with transient sprite buffer
- **Transient Buffers**: `zest_AddTransientBufferResource` for per-frame sprite output
- **Staging Buffers**: `zest_CreateStagingBuffer` for initial data upload
- **Immediate Commands**: `zest_BeginImmediateCommandBuffer` for one-time buffer copies
- **Bindless Descriptors**: Buffer indices passed to compute shader
- **Animation Manager**: `tfx_animation_manager` for playback state tracking

## Use Cases

Pre-recorded effects are useful for:
- Complex effects that are expensive to simulate in real-time
- Consistent playback across different hardware
- Effects that need to be synchronized with game events
- Reducing CPU load in particle-heavy scenes
