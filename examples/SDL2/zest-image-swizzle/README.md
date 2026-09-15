# Image Component Swizzle

Renders the same single channel mask twice, once with an identity component mapping and once with an alpha only swizzle.

## What It Does

Builds a 256x256 `zest_format_r8_unorm` soft edged disc on the CPU and uploads it as two images with identical pixels. The only difference between them is `zest_image_info_t::swizzle`. Both quads are drawn with the same pipeline and the same fragment shader, so the difference on screen comes entirely from the component mapping baked into the image view.

- **Left (identity)**: red reads as red and alpha reads as 1, giving an opaque red disc inside a hard edged square.
- **Right (`zest_SwizzleAlphaOnly`)**: rgb reads as 1 and alpha reads from red, so the push constant tint comes through and the disc blends softly into the background.

A non identity swizzle is only legal on sampled only images. Vulkan requires identity component mapping for views used as a framebuffer attachment or as a storage image, so `zest_CreateImage` asserts if the swizzle is set alongside a colour attachment, depth/stencil attachment, storage, or input attachment flag.

## Zest Features Used

- **Component Swizzle**: `zest_image_info_t::swizzle`, `zest_SwizzleAlphaOnly`
- **Images From Pixels**: `zest_CreateImageWithPixels` with `zest_image_preset_texture`
- **Bindless Descriptors**: `zest_AcquireSampledImageIndex`, `zest_AcquireSamplerIndex`
- **Pipeline Templates**: No vertex input, triangle strip quad built from `gl_VertexIndex`
- **Push Constants**: Quad rect, tint, and bindless indices
- **Frame Graph**: Single cached render pass writing to the swapchain

## Running

```bash
cmake --build E:/Projects/C++/Zest-Build --config Release --target zest-image-swizzle
E:/Projects/C++/Zest-Build/examples/SDL2/Release/zest-image-swizzle.exe
```
