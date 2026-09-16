# Image API

Functions for image, image view, and sampler management.

## Image Creation

### zest_CreateImageInfo

Creates a default image configuration struct with the specified dimensions.

```cpp
zest_image_info_t zest_CreateImageInfo(zest_uint width, zest_uint height);
```

Set `info.flags` to a preset for common use cases:
- `zest_image_preset_texture` - Standard texture loaded from CPU
- `zest_image_preset_texture_mipmaps` - Texture with automatic mipmap generation
- `zest_image_preset_color_attachment` - Render target that can be sampled (post-processing)
- `zest_image_preset_depth_attachment` - Depth buffer that can be sampled (shadow mapping, SSAO)
- `zest_image_preset_storage` - Compute shader read/write with sampling support
- `zest_image_preset_storage_cubemap` - Cubemap for compute shader processing
- `zest_image_preset_storage_mipped_cubemap` - Mipped cubemap for PBR lighting preparation

**Example:**
```cpp
// Create a texture with mipmaps
zest_image_info_t info = zest_CreateImageInfo(512, 512);
info.flags = zest_image_preset_texture_mipmaps;
info.format = zest_format_r8g8b8a8_unorm;
```

---

### Component Swizzle

`zest_image_info_t` carries a `swizzle` field that remaps the channels an image view reports to the shader. It is applied when the image's views are created, so the pixel data on the GPU is untouched - only what a sample returns changes.

```cpp
typedef enum {
    zest_component_swizzle_identity = 0,
    zest_component_swizzle_zero     = 1,
    zest_component_swizzle_one      = 2,
    zest_component_swizzle_r        = 3,
    zest_component_swizzle_g        = 4,
    zest_component_swizzle_b        = 5,
    zest_component_swizzle_a        = 6,
} zest_component_swizzle;

typedef struct zest_component_mapping_t {
    zest_component_swizzle r, g, b, a;
} zest_component_mapping_t;
```

Identity is `0`, so an image info left alone by `zest_CreateImageInfo` applies no swizzle at all.

Two helpers cover the common cases:

```cpp
zest_component_mapping_t zest_SwizzleAlphaOnly(void);        // rgb = 1, a = r
zest_component_mapping_t zest_SwizzleLuminanceAlpha(void);   // rgb = r, a = g
```

`zest_SwizzleAlphaOnly` reads a single channel image (an `r8_unorm` font or mask) as an alpha mask with white rgb, so a tint colour multiplies cleanly. `zest_SwizzleLuminanceAlpha` reads a two channel image as the old luminance/alpha pair.

**Example:**
```cpp
// An r8_unorm coverage mask sampled as vec4(1, 1, 1, coverage)
zest_image_info_t info = zest_CreateImageInfo(256, 256);
info.format = zest_format_r8_unorm;
info.flags = zest_image_preset_texture;
info.swizzle = zest_SwizzleAlphaOnly();
zest_image_handle mask = zest_CreateImageWithPixels(device, pixels, 256 * 256, &info);
```

!!! warning "Sampled only"
    A non identity swizzle is legal only on sampled only images. Vulkan requires an identity component mapping for a view used as a framebuffer attachment (`VUID-VkFramebuffer-pAttachments-00884`) or as a storage image descriptor, and `zest_CreateImage` asserts if the swizzle is set alongside `zest_image_flag_color_attachment`, `zest_image_flag_depth_stencil_attachment`, `zest_image_flag_storage` or `zest_image_flag_input_attachment`. Swapchain views and the single mip views the frame graph builds for framebuffer attachments always stay identity.

    Note also that the D3D12 and Metal backends would map this straight onto SRV component mapping and `MTLTextureSwizzleChannels`, but WebGPU has no per-view swizzle - a WebGPU backend would have to expand narrow formats to RGBA at upload time instead.

---

### zest_CreateImage

Creates an empty GPU image with the specified configuration.

```cpp
zest_image_handle zest_CreateImage(zest_device device, zest_image_info_t *create_info);
```

Use this for images that will be written to by the GPU (render targets, compute output).

**Example:**
```cpp
// Create a render target
zest_image_info_t info = zest_CreateImageInfo(1920, 1080);
info.flags = zest_image_preset_color_attachment;
info.format = zest_format_r8g8b8a8_unorm;
zest_image_handle render_target = zest_CreateImage(device, &info);
```

---

### zest_IsImageFormatSupported

Checks whether the device can create an image with a given format and usage flags.

```cpp
zest_bool zest_IsImageFormatSupported(zest_device device, zest_format format, zest_image_flags flags);
```

`zest_CreateImage` runs the same check and fails validation if it doesn't pass. In debug builds that failure is an assert. If you have a fallback format, call this first and pick the format before creating the image.

The check covers every usage in `flags`. For example, a format that can be sampled but not rendered to returns `ZEST_FALSE` when `flags` includes `zest_image_flag_color_attachment`. `zest_image_flag_host_visible` checks linear tiling support. Otherwise the check uses optimal tiling.

A block compressed format also needs its device capability enabled. Zest enables each of these automatically when the GPU supports it:

| Formats | Capability |
|---------|------------|
| `zest_format_bc1_*` to `zest_format_bc7_*` | `zest_capability_texture_compression_bc` |
| `zest_format_etc2_*`, `zest_format_eac_*` | `zest_capability_texture_compression_etc2` |
| `zest_format_astc_*` | `zest_capability_texture_compression_astc_ldr` |

If the capability isn't enabled, the function returns `ZEST_FALSE`, even when the driver reports that the format can be sampled.

**Example:**
```cpp
// Upload BC4 blocks when the device can sample them, otherwise decode to R8 on the CPU
zest_image_flags flags = zest_image_preset_texture | zest_image_flag_force_image_array;
if (zest_IsImageFormatSupported(device, zest_format_bc4_unorm_block, flags)) {
    info.format = zest_format_bc4_unorm_block;
    info.mip_levels = stored_level_count;
} else {
    info.format = zest_format_r8_unorm;
    flags = zest_image_preset_texture_mipmaps | zest_image_flag_force_image_array;
}
info.flags = flags;
info.swizzle = zest_SwizzleAlphaOnly();
```

---

### zest_CreateImageWithPixels

Creates a GPU image and uploads pixel data from CPU memory.

```cpp
zest_image_handle zest_CreateImageWithPixels(
    zest_device device,
    void *pixels,
    zest_size size,
    zest_image_info_t *create_info
);
```

The most common way to create textures from loaded image data.

**Example:**
```cpp
// Load and create a texture
int width, height, channels;
stbi_uc *pixels = stbi_load("texture.png", &width, &height, &channels, 4);
zest_size size = width * height * 4;

zest_image_info_t info = zest_CreateImageInfo(width, height);
info.flags = zest_image_preset_texture;
info.format = zest_format_r8g8b8a8_unorm;

zest_image_handle texture = zest_CreateImageWithPixels(device, pixels, size, &info);
stbi_image_free(pixels);
```

---

### zest_GetImage

Retrieves the image object from a handle for usage in various image functions.

```cpp
zest_image zest_GetImage(zest_image_handle handle);
```

**Example:**
```cpp
zest_image image = zest_GetImage(texture_handle);
zest_uint mip_count = image->mip_levels;
```

---

### zest_FreeImage

Queues an image for deferred destruction at the end of the current frame.

```cpp
void zest_FreeImage(zest_image_handle image_handle);
```

Safe to call while the image may still be in use by in-flight GPU commands.

---

### zest_FreeImageNow

Immediately destroys an image. Only call when certain the image is not in use.

```cpp
void zest_FreeImageNow(zest_image_handle image_handle);
```

---

## Image Views

Image views provide a way to access subsets of an image (specific mip levels, array layers).

### zest_CreateViewImageInfo

Creates a default image view configuration for an image.

```cpp
zest_image_view_create_info_t zest_CreateViewImageInfo(zest_image image);
```

Returns a configuration with sensible defaults based on the image type. Modify the returned struct to customize which mip levels or array layers are accessible.

**Fields in `zest_image_view_create_info_t`:**
- `view_type` - The type of view (2D, cube, array, etc.)
- `base_mip_level` - First mip level accessible through this view
- `mip_level_count` - Number of mip levels accessible
- `base_array_layer` - First array layer accessible
- `layer_count` - Number of array layers accessible

---

### zest_CreateImageView

Creates an image view with the specified configuration.

```cpp
zest_image_view_handle zest_CreateImageView(
    zest_device device,
    zest_image image,
    zest_image_view_create_info_t *create_info
);
```

**Example:**
```cpp
// Create a view for a specific mip level
zest_image_view_create_info_t view_info = zest_CreateViewImageInfo(image);
view_info.base_mip_level = 2;
view_info.mip_level_count = 1;
zest_image_view_handle mip2_view = zest_CreateImageView(device, image, &view_info);
```

> Note that when you create an image it automatically creates an default image view that is used when acquiring descriptor indexes. You can use this though if you need something more specific like targetting a specific mip level as in the example above.


---

### zest_CreateImageViewsPerMip

Creates an array of image views, one for each mip level.

```cpp
zest_image_view_array_handle zest_CreateImageViewsPerMip(zest_device device, zest_image image);
```

Useful for compute shaders that need to write to individual mip levels (e.g., mipmap generation, cubemap filtering).

---

### zest_GetImageView

Retrieves the image view object from a handle.

```cpp
zest_image_view zest_GetImageView(zest_image_view_handle handle);
```

---

### zest_GetImageViewArray

Retrieves an array of image views from a handle.

```cpp
zest_image_view_array zest_GetImageViewArray(zest_image_view_array_handle handle);
```

---

### zest_FreeImageView / zest_FreeImageViewNow

Destroys an image view (deferred or immediate).

```cpp
void zest_FreeImageView(zest_image_view_handle view_handle);
void zest_FreeImageViewNow(zest_image_view_handle view_handle);
```

---

### zest_FreeImageViewArray / zest_FreeImageViewArrayNow

Destroys an array of image views (deferred or immediate).

```cpp
void zest_FreeImageViewArray(zest_image_view_array_handle view_handle);
void zest_FreeImageViewArrayNow(zest_image_view_array_handle view_handle);
```

---

## Samplers

Samplers define how textures are filtered and addressed when sampled in shaders.

### zest_CreateSamplerInfo

Creates default sampler configurations.

```cpp
zest_sampler_info_t zest_CreateSamplerInfo();           // Clamp-to-edge addressing
zest_sampler_info_t zest_CreateSamplerInfoRepeat();     // Repeat/wrap addressing
zest_sampler_info_t zest_CreateSamplerInfoMirrorRepeat(); // Mirror-repeat addressing
```

All variants default to linear filtering with linear mipmap interpolation.

---

### zest_CreateSampler

Creates a sampler with the specified configuration.

```cpp
zest_sampler_handle zest_CreateSampler(zest_device device, zest_sampler_info_t *info);
```

**Example:**
```cpp
zest_sampler_info_t info = zest_CreateSamplerInfoRepeat();
zest_sampler_handle sampler_handle = zest_CreateSampler(device, &info);
zest_sampler sampler = zest_GetSampler(sampler_handle);
```

---

### zest_GetSampler

Retrieves the sampler object from a handle.

```cpp
zest_sampler zest_GetSampler(zest_sampler_handle handle);
```

---

## Bindless Indices

Zest uses bindless descriptors where images and samplers are indexed into global descriptor arrays. These functions manage the allocation and release of those indices.

### zest_AcquireSampledImageIndex

Registers an image in the bindless descriptor array and returns its index for shader access.

```cpp
zest_uint zest_AcquireSampledImageIndex(
    zest_device device,
    zest_image image,
    zest_binding_number_type binding_number
);
```

Common binding numbers:
- `zest_texture_2d_binding` - Standard 2D textures
- `zest_texture_array_binding` - Standard 2D texture arrays
- `zest_storage_image_binding` - Storage images for use in compute shaders
- `zest_texture_cube_binding` - Cubemap textures

**Example:**
```cpp
zest_image image = zest_GetImage(texture_handle);
zest_uint texture_index = zest_AcquireSampledImageIndex(device, image, zest_texture_2d_binding);
// Pass texture_index to shader via push constants or uniform buffer
```

---

### zest_AcquireSamplerIndex

Registers a sampler in the bindless descriptor array.

```cpp
zest_uint zest_AcquireSamplerIndex(zest_device device, zest_sampler sampler);
```

**Example:**
```cpp
zest_sampler sampler = zest_GetSampler(sampler_handle);
zest_uint sampler_index = zest_AcquireSamplerIndex(device, sampler);
```

---

### zest_AcquireStorageImageIndex

Registers an image for storage (read/write) access in compute shaders.

```cpp
zest_uint zest_AcquireStorageImageIndex(
    zest_device device,
    zest_image image,
    zest_binding_number_type binding_number
);
```

Use `zest_storage_image_binding` for the binding number.

---

### zest_AcquireImageMipIndexes

Registers each mip level of an image separately for individual mip access in shaders.

```cpp
zest_uint *zest_AcquireImageMipIndexes(
    zest_device device,
    zest_image image,
    zest_image_view_array image_view_array,
    zest_binding_number_type binding_number,
    zest_descriptor_type descriptor_type
);
```

Returns an array of indices, one per mip level. Useful for compute-based mipmap generation.

---

### zest_ReleaseImageIndex

Releases a sampled or storage image index back to the pool.

```cpp
void zest_ReleaseImageIndex(
    zest_device device,
    zest_image image,
    zest_binding_number_type binding_number
);
```

---

### zest_ReleaseImageMipIndexes

Releases all mip-level indices for an image.

```cpp
void zest_ReleaseImageMipIndexes(
    zest_device device,
    zest_image image,
    zest_binding_number_type binding_number
);
```

---

### zest_ReleaseAllImageIndexes

Releases all bindless indices associated with an image across all binding numbers.

> **Note:** This will happen automatically when the image is freed.

```cpp
void zest_ReleaseAllImageIndexes(zest_device device, zest_image image);
```

---

### zest_ReleaseBindlessIndex

Releases a specific bindless index by value (for samplers or when you only have the index).

```cpp
void zest_ReleaseBindlessIndex(
    zest_device device,
    zest_uint index,
    zest_binding_number_type binding_number
);
```

---

## Complete Example

Loading a texture and making it available to shaders:

```cpp
// Load image from file
int width, height, channels;
stbi_uc *pixels = stbi_load("sprite.png", &width, &height, &channels, 4);

// Create GPU image
zest_image_info_t info = zest_CreateImageInfo(width, height);
info.flags = zest_image_preset_texture;
info.format = zest_format_r8g8b8a8_unorm;
zest_image_handle image_handle = zest_CreateImageWithPixels(device, pixels, width * height * 4, &info);
stbi_image_free(pixels);

// Get bindless index for shader access
zest_image image = zest_GetImage(image_handle);
zest_uint texture_index = zest_AcquireSampledImageIndex(device, image, zest_texture_2d_binding);

// Create a sampler
zest_sampler_info_t sampler_info = zest_CreateSamplerInfo();
zest_sampler_handle sampler_handle = zest_CreateSampler(device, &sampler_info);
zest_sampler sampler = zest_GetSampler(sampler_handle);
zest_uint sampler_index = zest_AcquireSamplerIndex(device, sampler);

// Use texture_index and sampler_index in your shader push constants
```

---

## See Also

- `examples/SDL2/zest-image-swizzle` - identity vs alpha only swizzle side by side
- [Images Concept](../concepts/images.md)
- [Bindless Concept](../concepts/bindless.md)
