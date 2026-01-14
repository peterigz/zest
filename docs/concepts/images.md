# Images

Images in Zest include textures, render targets, and depth buffers. The library provides streamlined creation and bindless access.

## Creating Images

### Basic Image

```cpp
zest_image_info_t info = zest_CreateImageInfo(width, height);
info.format = zest_format_r8g8b8a8_unorm;
info.flags = zest_image_preset_texture_mipmaps;

zest_image_handle handle = zest_CreateImage(device, &info);
zest_image image = zest_GetImage(handle);
```

### With Pixel Data

```cpp
// Load pixels (using stb_image or similar)
int width, height, channels;
unsigned char* pixels = stbi_load("texture.png", &width, &height, &channels, 4);

// Create image info
zest_image_info_t info = zest_CreateImageInfo(width, height);
info.format = zest_format_r8g8b8a8_unorm;
info.flags = zest_image_preset_texture_mipmaps;

// Create with pixels
zest_size pixel_size = width * height * 4;
zest_image_handle handle = zest_CreateImageWithPixels(device, pixels, pixel_size, &info);

stbi_image_free(pixels);
```

## Image Presets

Common configurations via flags:

```cpp
// Texture with mipmaps (most common)
info.flags = zest_image_preset_texture_mipmaps;

// Texture without mipmaps
info.flags = zest_image_preset_texture;

// Render target (color attachment)
info.flags = zest_image_preset_render_target;

// Depth buffer
info.flags = zest_image_preset_depth;

// Storage image (compute shader access)
info.flags = zest_image_preset_storage;
```

## Image Formats

Common formats:

```cpp
// Color formats
zest_format_r8g8b8a8_unorm      // 8-bit RGBA, normalized
zest_format_r8g8b8a8_srgb       // 8-bit RGBA, sRGB
zest_format_r16g16b16a16_sfloat // 16-bit float RGBA (HDR)
zest_format_r32g32b32a32_sfloat // 32-bit float RGBA

// Depth formats
zest_format_d32_sfloat          // 32-bit depth
zest_format_d24_unorm_s8_uint   // 24-bit depth + 8-bit stencil

// Compressed formats
zest_format_bc1_rgb_unorm       // BC1 (DXT1)
zest_format_bc3_unorm           // BC3 (DXT5)
zest_format_bc7_unorm           // BC7 (high quality)
```

## Samplers

Samplers define how textures are filtered and addressed.

### Creating Samplers

```cpp
zest_sampler_info_t info = zest_CreateSamplerInfo();
zest_sampler_handle sampler = zest_CreateSampler(device, &info);
```

### Sampler Options

```cpp
zest_sampler_info_t info = zest_CreateSamplerInfo();

// Filtering
info.min_filter = zest_filter_linear;
info.mag_filter = zest_filter_linear;
info.mipmap_mode = zest_mipmap_mode_linear;

// Address mode
info.address_mode_u = zest_sampler_address_mode_repeat;
info.address_mode_v = zest_sampler_address_mode_repeat;
info.address_mode_w = zest_sampler_address_mode_clamp_to_edge;

// Anisotropy
info.max_anisotropy = 16.0f;
```

### Preset Samplers

```cpp
// Repeat mode
zest_sampler_info_t info = zest_CreateSamplerInfoRepeat();
```

## Bindless Access

Images are accessed through global descriptor arrays.

### Acquiring Indices

```cpp
// Get image and sampler
zest_image image = zest_GetImage(image_handle);
zest_sampler sampler = zest_GetSampler(sampler_handle);

// Acquire bindless indices
zest_uint tex_index = zest_AcquireSampledImageIndex(device, image, zest_texture_2d_binding);
zest_uint sampler_index = zest_AcquireSamplerIndex(device, sampler);
```

### Using in Shaders

```glsl
// Descriptor arrays
layout(set = 0, binding = 0) uniform sampler samplers[];
layout(set = 0, binding = 1) uniform texture2D textures[];

// Sample using indices from push constants
void main() {
    vec4 color = texture(
        sampler2D(textures[push.tex_index], samplers[push.sampler_index]),
        uv
    );
}
```

### Releasing Indices

```cpp
// When image is no longer needed
zest_ReleaseSampledImageIndex(device, tex_index);
zest_ReleaseSamplerIndex(device, sampler_index);
```

## Image Views

For accessing specific mip levels or array layers:

```cpp
// Create view for specific mip
zest_image_view view = zest_CreateImageView(image, mip_level, array_layer);

// Create views for all mips
zest_image_view* views = zest_CreateImageViewsPerMip(image);
zest_uint mip_count = zest_GetImageMipCount(image);

// Free views
zest_FreeImageView(view);
zest_FreeImageViewArray(views, mip_count);
```

## Render Targets

### Creating Render Targets

```cpp
zest_image_info_t info = zest_CreateImageInfo(width, height);
info.format = zest_format_r16g16b16a16_sfloat;
info.flags = zest_image_preset_render_target;

zest_image_handle render_target = zest_CreateImage(device, &info);
```

### Transient Render Targets

For intermediate targets in a frame graph:

```cpp
zest_image_resource_info_t info = {
    .format = zest_format_r16g16b16a16_sfloat,
    .usage_hint = zest_resource_usage_hint_render_target,
    .width = screen_width,
    .height = screen_height,
    .mip_levels = 1
};
zest_resource_node hdr_target = zest_AddTransientImageResource("HDR", &info);
```

## Depth Buffers

```cpp
zest_image_info_t info = zest_CreateImageInfo(width, height);
info.format = zest_format_d32_sfloat;
info.flags = zest_image_preset_depth;

zest_image_handle depth = zest_CreateImage(device, &info);
```

## Loading KTX Textures

Using zest_utilities.h:

```cpp
#include <zest_utilities.h>

zest_image_handle texture = zest_LoadKTX(device, "my_texture", "path/to/texture.ktx");
```

KTX format supports:

- Mipmaps
- Cubemaps
- Compressed formats
- Array textures

## Image Operations

### Copy Between Images

```cpp
zest_queue queue = zest_imm_BeginCommandBuffer(device, zest_queue_transfer);
zest_imm_CopyImageToImage(queue, src, dst, width, height);
zest_imm_EndCommandBuffer(queue);
```

### Generate Mipmaps

```cpp
zest_queue queue = zest_imm_BeginCommandBuffer(device, zest_queue_graphics);
zest_imm_GenerateMipMaps(queue, image);
zest_imm_EndCommandBuffer(queue);
```

### Clear Image

```cpp
void ClearCallback(zest_command_list cmd, void* data) {
    zest_cmd_ImageClear(cmd, image, 0.0f, 0.0f, 0.0f, 1.0f);
}
```

### Blit (Scaled Copy)

```cpp
zest_cmd_BlitImageMip(cmd, src, dst, src_mip, dst_mip, filter);
```

## Freeing Images

```cpp
// Deferred free (safe)
zest_FreeImage(image_handle);

// Immediate free (ensure GPU is done)
zest_FreeImageNow(image_handle);
```

## Best Practices

1. **Use appropriate formats** - sRGB for color textures, linear for data
2. **Generate mipmaps** - For textures viewed at varying distances
3. **Use compressed formats** - BC7 for quality, BC1 for small size
4. **Prefer transient images** - For intermediate render targets
5. **Release bindless indices** - When images are freed

## See Also

- [Bindless Descriptors](bindless.md) - Descriptor system details
- [Image API](../api-reference/image.md) - Complete function reference
- [Textures Tutorial](../tutorials/04-textures.md) - Loading and using textures
