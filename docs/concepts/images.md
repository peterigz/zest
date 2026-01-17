# Images

Images in Zest represent GPU memory that stores pixel data. This includes textures (sampled in shaders), color attachments (render targets), depth/stencil attachments, and storage images (read/write in compute shaders).

Zest uses a handle-based system where `zest_image_handle` is a lightweight reference to the underlying `zest_image` resource. This separation allows safe deferred destruction while maintaining fast access during rendering.

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

//Image is now ready to be sampled in a texture.

stbi_image_free(pixels);
```

## Image Presets

Common configurations via flags:

```cpp
// Texture with mipmaps (most common)
info.flags = zest_image_preset_texture_mipmaps;

// Texture without mipmaps
info.flags = zest_image_preset_texture;

// Color attachment (render target)
info.flags = zest_image_preset_color_attachment;

// Depth attachment
info.flags = zest_image_preset_depth_attachment;

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
zest_sampler_handle sampler = zest_CreateSampler(context, &info);
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

// Anisotropic filtering (reduces blur on surfaces viewed at angles)
info.anisotropy_enable = ZEST_TRUE;
info.max_anisotropy = 16.0f;
```

## Bindless Access

Zest uses bindless descriptors to access images in shaders. Instead of binding individual textures to specific slots, all textures are stored in global descriptor arrays and accessed by index. This allows shaders to use any texture without rebinding, enabling efficient batching and dynamic material systems.

The following functions apply to persistant images that were created outside of a frame graph. Transient images acquire indexes with different functions as they will get automatically released at the end of each frame.

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
// When image is no longer needed, release its bindless index
zest_ReleaseImageIndex(device, image, zest_texture_2d_binding);

// To release all bindless indices for an image at once
zest_ReleaseAllImageIndexes(device, image);
```

> **Note:** When you call zest_FreeImage or zest_FreeImageNow any indexes that the image acquired will be released so you only need to call the functions above if you want to keep the image but no longer need to sample it in the shader.

## Image Views

Image views provide a way to access a subset of an image's data. While an image might contain multiple mip levels or array layers, a view can expose just a portion of that data to shaders or as an attachment. This is useful for mipmap generation, cubemap face access, or array texture slicing.

When an image is created it is automatically set up with a default view, but you can create other views if you have more specific needs.

```cpp
// Create view info (defaults to all mips and layers)
zest_image_view_create_info_t view_info = zest_CreateViewImageInfo(image);

// Customize for specific mip range
view_info.base_mip_level = 0;
view_info.level_count = 1;  // Single mip

// Create the view
zest_image_view_handle view_handle = zest_CreateImageView(device, image, &view_info);
zest_image_view view = zest_GetImageView(view_handle);

// Create views for all mips (one view per mip level)
zest_image_view_array_handle views_handle = zest_CreateImageViewsPerMip(device, image);
zest_image_view_array views = zest_GetImageViewArray(views_handle);
zest_uint mip_count = image->info.mip_levels;

// Free views
zest_FreeImageView(view_handle);
zest_FreeImageViewArray(views_handle);
```

## Render Targets

### Creating Render Targets

```cpp
zest_image_info_t info = zest_CreateImageInfo(width, height);
info.format = zest_format_r16g16b16a16_sfloat;
info.flags = zest_image_preset_color_attachment;

zest_image_handle render_target = zest_CreateImage(device, &info);
```

### Transient Render Targets

For intermediate targets that only exist within a single frame, use the frame graph's transient resource system. These resources are automatically created and destroyed as needed:

```cpp
zest_image_resource_info_t info = {
    .format = zest_format_r16g16b16a16_sfloat,
    .usage_hints = zest_resource_usage_hint_none,  // Frame graph infers usage
    .width = screen_width,   // 0 defaults to screen width
    .height = screen_height, // 0 defaults to screen height
    .mip_levels = 1
};
zest_resource_node hdr_target = zest_AddTransientImageResource("HDR", &info);
```

## Depth Buffers

```cpp
zest_image_info_t info = zest_CreateImageInfo(width, height);
info.format = zest_format_d32_sfloat;
info.flags = zest_image_preset_depth_attachment;

zest_image_handle depth = zest_CreateImage(device, &info);
```

## Loading KTX Textures

Using the optional zest_utilities.h:

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

Zest provides two ways to perform image operations:
- **Immediate mode** (`zest_imm_*`): One-off operations that block until complete. Useful for initialization and loading.
- **Frame graph commands** (`zest_cmd_*`): Operations within render/compute passes. Automatically synchronized by the frame graph.

### Blit Between Images

Use immediate mode commands for one-off operations outside the frame graph:

```cpp
zest_queue queue = zest_imm_BeginCommandBuffer(device, zest_queue_graphics);
zest_imm_BlitImage(queue, src_image, dst_image,
    0, 0, src_width, src_height,    // Source region
    0, 0, dst_width, dst_height,    // Destination region
    zest_filter_linear);
zest_imm_EndCommandBuffer(queue);
```

### Generate Mipmaps

```cpp
zest_queue queue = zest_imm_BeginCommandBuffer(device, zest_queue_graphics);
zest_imm_GenerateMipMaps(queue, image);
zest_imm_EndCommandBuffer(queue);
```

### Clear Image

Within a frame graph callback:

```cpp
void ClearCallback(zest_command_list cmd, void* data) {
    zest_cmd_ImageClear(cmd, image);
}
```

For immediate mode clearing with specific values:

```cpp
zest_queue queue = zest_imm_BeginCommandBuffer(device, zest_queue_graphics);
zest_clear_value_t clear = { .color = {0.0f, 0.0f, 0.0f, 1.0f} };
zest_imm_ClearColorImage(queue, image, clear);
zest_imm_EndCommandBuffer(queue);
```

### Blit Mip Level (Frame Graph)

Within a frame graph, blit from one mip level to another:

```cpp
void BlitCallback(zest_command_list cmd, void* data) {
    zest_cmd_BlitImageMip(cmd, src_resource, dst_resource, mip_to_blit,
        zest_pipeline_stage_fragment_shader_bit);
}
```

## Freeing Images

Images should be freed when no longer needed. Use deferred destruction to ensure the GPU has finished using the resource:

```cpp
// Deferred free (safe - waits for GPU to finish)
zest_FreeImage(image_handle);

// Immediate free (only use when you've ensured GPU is done)
zest_FreeImageNow(image_handle);
```

## Best Practices

1. **Use appropriate formats** - sRGB for color textures, linear for data
2. **Generate mipmaps** - For textures viewed at varying distances
3. **Use compressed formats** - BC7 for quality, BC1 for small size, not essential but if you're using a lot of texture memory then well worth it.
4. **Prefer transient images** - For intermediate render targets

## See Also

- [Bindless Descriptors](bindless.md) - Descriptor system details
- [Image API](../api-reference/image.md) - Complete function reference
- [Textures Tutorial](../tutorials/04-textures.md) - Loading and using textures
