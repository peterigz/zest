# TimelineFX Particle Effects

Demonstrates integrating the TimelineFX particle effects library with Zest.

## What It Does

Shows how to load and play real-time particle effects created with the [TimelineFX Editor](https://www.rigzsoft.co.uk/). Effects are updated each frame by the particle manager and rendered as billboards/sprites. Click to spawn explosion effects at the mouse position using screen-to-world ray casting.

## Running

Run from the root of the Zest repository, as the library and shader paths are relative to it.

- `--d3d12` uses the D3D12 backend, in a build configured with `ZEST_ENABLE_D3D12`
- `--validate` turns on the validation layers and exits with 1 on any validation error
- `--validate-sync` adds Vulkan synchronisation validation, or D3D12 GPU-based validation, to `--validate`
- `--frames N` exits after N frames

## Zest Features Used

- **TimelineFX Integration**: `zest_tfx_InitTimelineFXRenderResources`, `zest_tfx_ShapeLoader`
- **Per Shape Images**: one layered image and bindless index per particle shape, animation frames as array layers
- **Image Atlases**: `zest_CreateImageAtlas` for the color ramps
- **Texture Arrays**: `zest_texture_array_binding` for the shape images
- **Frame Graph**: Transfer pass + render pass with TimelineFX rendering
- **Uniform Buffers**: Camera view/projection for 3D effect positioning
- **Push Constants**: Texture and sampler indices
- **Screen Ray Casting**: `zest_ScreenRay` to place effects at mouse position
- **ImGui Integration**: Particle count, effect count, emitter stats

## Controls

- **Left Click**: Spawn explosion effect at mouse position
- **Print Render Graph button**: Output frame graph structure
- **Toggle VSync button**: Enable/disable vertical sync
