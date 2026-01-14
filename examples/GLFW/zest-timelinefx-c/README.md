# TimelineFX C API

Demonstrates the TimelineFX particle effects library using the pure C API.

## What It Does

Same functionality as the C++ TimelineFX example but using TimelineFX's C API. Loads particle effect libraries, creates effect templates, and manages a particle manager for real-time effect playback. Click to spawn effects at the mouse position.

## Zest Features Used

- **TimelineFX C Integration**: `zest_tfx_InitTimelineFXRenderResources`, `tfx_LoadEffectLibrary`
- **Image Atlases**: `zest_CreateImageAtlas` for particle textures
- **Color Ramps**: `zest_CreateImageAtlasCollection` for gradient textures
- **Texture Arrays**: Bindless array indexing for sprite sheets
- **Frame Graph**: Transfer and render passes
- **Uniform Buffers**: 3D camera matrices
- **Screen Ray Casting**: `zest_ScreenRay` for effect placement

## Building

This example compiles as pure C code, demonstrating that Zest and TimelineFX can be used without C++ dependencies.

## Controls

- **Left Click**: Spawn effect at mouse position
