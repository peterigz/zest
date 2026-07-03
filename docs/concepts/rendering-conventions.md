# Rendering Conventions

Zest defines a single set of coordinate and data conventions that user code — shaders,
projection matrices, winding order, serialized formats — is written against. These
conventions are part of the frozen API contract: when new backends (D3D12, Metal) are
added, the backend compensates internally so that code written against these rules renders
identically everywhere.

## Clip Space / NDC

Zest uses Vulkan's clip space conventions:

| Axis | Range | Direction on screen |
|------|-------|---------------------|
| X | -1 to +1 | Right |
| Y | -1 to +1 | **Down** (-1 is the top of the screen) |
| Z (depth) | 0 to 1 | 0 at the near plane, 1 at the far plane |

Key points:

- **+Y points down** in NDC. This matches Vulkan directly; backends whose native clip
  space is Y-up (D3D12, Metal) will compensate internally.
- **Depth is 0..1** (not OpenGL's -1..1). All of Zest's projection helpers already produce
  0..1 depth.
- Zest does not flip viewports behind your back — a viewport with a positive height has
  y = 0 at the top of the render target.

## Projection Matrix Helpers

`zest_Perspective`, `zest_Ortho` and `zest_LookAt` are standard right-handed, zero-to-one
depth (GLM style `RH_ZO`) helpers. In view space the camera looks down **-Z** with +Y up.

Because the helpers map +Y up in clip space while Zest's NDC is Y-down, the standard idiom
for a Y-up world is to flip the projection's Y scale after building it:

```cpp
ubo->proj = zest_Perspective(camera.fov, aspect, 0.1f, 10000.f);
ubo->proj.v[1].y *= -1.f;   // Flip Y for Zest's Y-down NDC
```

All of the 3D examples do this. For screen-space orthographic rendering you can instead
swap the bottom/top arguments so that y = 0 lands at the top of the screen:

```cpp
// y = 0 at the top of the screen, y = screen_height at the bottom
zest_matrix4 ortho = zest_Ortho(0.f, screen_width, 0.f, screen_height, -1.f, 1.f);
```

If you build matrices with another library (e.g. GLM), configure it for right-handed,
zero-to-one depth (`GLM_FORCE_DEPTH_ZERO_TO_ONE`) and apply the same Y flip.

## Framebuffer and Texture Origin

- **Framebuffer origin is the top-left corner**: pixel (0, 0) is the top-left of the
  render target, +Y down. Scissor rects, viewports and `gl_FragCoord` all follow this.
- **Texture coordinate (0, 0) is the top-left**: UV (0, 0) samples the first texel of the
  first row of the pixel data you upload (`zest_CreateImageWithPixels`, staging buffer
  copies). Row 0 of your CPU-side image data is the top of the texture.
- Storage image texel coordinates follow the same rule: `imageStore(img, ivec2(0, 0), ...)`
  writes the top-left texel.

## Winding and Culling

Front-face winding is measured in **framebuffer space** (origin top-left, +Y down).

- The pipeline template default is `zest_front_face_clockwise` with
  `zest_cull_mode_none` — new pipelines draw both sides until you opt into culling.
- With the standard Y-flip idiom above, geometry exported counter-clockwise in a Y-up
  world arrives clockwise in framebuffer space.
- Future backends compensate internally so the same `zest_front_face` setting selects the
  same triangles everywhere.

## Depth

- Depth range is 0..1. The pipeline template default depth compare op is
  `zest_compare_op_less_or_equal` (smaller depth = closer with a standard projection).
- Viewport min/max depth default to 0 and 1 (`zest_cmd_SetScreenSizedViewport` takes them
  explicitly).

## Formats Are Frozen

The numeric values of `zest_format` are frozen API:

- They mirror `VkFormat` values and never change meaning.
- They are the serialization contract — shader caches, cached frame graph keys and any
  user file format that stores a `zest_format` rely on the values being stable.
- New formats are only ever appended with explicit values; members are never renumbered
  or removed.
- Not every backend (or GPU) supports every format. Image creation validates format
  support and fails with a validation error if the format can't be used with the
  requested usage flags.

## See Also

- [Device & Context](device-and-context.md) - Core objects
- [Pipelines](pipelines.md) - Winding, culling and depth state configuration
- [Images](images.md) - Texture creation and formats
