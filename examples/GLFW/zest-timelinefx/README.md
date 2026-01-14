# TimelineFX Particle Effects

Demonstrates integrating the TimelineFX particle effects library with Zest.

## What It Does

Shows how to load and play real-time particle effects created with the [TimelineFX Editor](https://www.rigzsoft.co.uk/). Effects are updated each frame by the particle manager and rendered as billboards/sprites. Click to spawn explosion effects at the mouse position using screen-to-world ray casting.

## Zest Features Used

- **TimelineFX Integration**: `zest_tfx_InitTimelineFXRenderResources`, `zest_tfx_ShapeLoader`
- **Image Atlases**: `zest_CreateImageAtlas` for particle textures and color ramps
- **Texture Arrays**: `zest_texture_array_binding` for sprite sheets
- **Frame Graph**: Transfer pass + render pass with TimelineFX rendering
- **Uniform Buffers**: Camera view/projection for 3D effect positioning
- **Push Constants**: Texture and sampler indices
- **Screen Ray Casting**: `zest_ScreenRay` to place effects at mouse position
- **ImGui Integration**: Particle count, effect count, emitter stats

## Controls

- **Left Click**: Spawn explosion effect at mouse position
- **Print Render Graph button**: Output frame graph structure
- **Toggle VSync button**: Enable/disable vertical sync
