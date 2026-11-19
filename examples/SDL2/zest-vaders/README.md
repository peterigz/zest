# Zest Vaders

A complete Space Invaders clone demonstrating billboards, MSDF fonts, and TimelineFX particle effects.

## What It Does

A playable Space Invaders-style game featuring:
- Player ship with shooting mechanics
- Multiple enemy types (regular vaders and big vaders with laser attacks)
- Particle effects for explosions, bullets, power-ups, and lasers
- MSDF font rendering for score display
- Animated background effects
- Title screen and game over states

## Running

Run from the root of the Zest repository, as the shader, effect and font paths are relative to it.

- D3D12 is the default in a build configured with `ZEST_ENABLE_D3D12`, falling back to Vulkan if the D3D12 device can't be created. Otherwise it runs on Vulkan.
- `--vulkan` or `--d3d12` chooses the backend, with no fallback
- `--validate` turns on the validation layers and exits with 1 on any validation error
- `--validate-sync` adds Vulkan synchronisation validation, or D3D12 GPU-based validation, to `--validate`
- `--frames N` exits after N frames

## Zest Features Used

- **Billboard Rendering**: Custom billboard instance struct with UV packing, alignment, rotation
- **TimelineFX Integration**: Multiple particle managers (game, background, title effects)
- **MSDF Fonts**: `zest_CreateMSDF` for crisp score and UI text
- **Image Atlases**: `zest_CreateImageAtlas` for sprite sheets
- **Frame Graph**: Transfer pass + multiple render passes (background, sprites, particles, fonts, ImGui)
- **Frame Graph Caching**: `zest_InitialiseCacheKey` with game state flags
- **Texture Arrays**: Particle sprites and color ramps
- **Uniform Buffers**: 3D camera for effect positioning
- **Push Constants**: Texture and sampler indices for billboards
- **Layer System**: `zest_CreateInstanceLayer` for sprite batching
- **Camera Utilities**: `zest_CreateCamera` for 3D effect spawning

## Controls

- **Left/Right Arrow or A/D**: Move player ship
- **Space**: Fire
- **P**: Pause game
