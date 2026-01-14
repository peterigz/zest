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
