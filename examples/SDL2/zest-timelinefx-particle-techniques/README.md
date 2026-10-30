# TimelineFX Particle Techniques

Plays the same particle effects three different ways, so the cost of each technique can be compared in the profiler.

## What It Does

Pick a technique from the **Effects** dropdown:

- **Baked**: plays pre-recorded sprite data from `prerecorded_effects.tfxsd`, exported from the TimelineFX Editor. All the sprite data for every frame is stored up front, and a compute shader builds the renderable sprite buffer each frame from the animation instances that are playing and their current playback time.
- **Stage simulated on the CPU**: simulates the effects live from `instanced_effects.tfx` on a TimelineFX stage, with every emitter updated on the CPU.
- **Stage simulated on the GPU**: the same stage, but emitters set to Run on GPU are simulated by the `simulate_particles` compute shader, which writes its instances into the layer's instance buffer after the CPU ones. That part is handled by `tfx_gpu_particle_renderer_t` in `implementations/impl_timelinefx.h`.

The GPU stats show particle slots rather than live particles. A dead GPU particle keeps its slot until the longest-lived particle spawned on the same tick dies, so the slot count runs higher than the CPU particle count for the same effects.

## Zest Features Used

- **Compute Shaders**: `zest_CreateCompute` for sprite data playback and GPU particle simulation
- **Storage Buffers**: Pre-recorded sprite data, emitter properties, GPU image data
- **Frame Graph**: Compute pass -> Render pass with transient sprite buffer, and a cached graph per technique
- **Transient Buffers**: `zest_AddTransientBufferResource` for per-frame sprite output and GPU particle uploads
- **Staging Buffers**: `zest_CreateStagingBuffer` for initial data upload
- **Immediate Commands**: `zest_BeginImmediateCommandBuffer` for one-time buffer copies
- **Bindless Descriptors**: Buffer indices passed to compute shaders
- **Animation Manager**: `tfx_animation_manager` for playback state tracking
- **GPU Particle Simulation**: A persistent imported pool buffer, a transient upload buffer and `zest_ReserveInstanceLayerDeviceCapacity` so a compute pass can append instances to a layer
- **GPU Profiling**: `zest_MarkProfileIdle` on frames where the upload and simulate passes have nothing to do

## When To Use Each

Baked effects are useful for:
- Complex effects that are expensive to simulate in real-time
- Consistent playback across different hardware
- Effects that need to be synchronized with game events

Stage simulation is useful when effects need to react at run time, and running emitters on the GPU takes most of the particle cost off the CPU.
