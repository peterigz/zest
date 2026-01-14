# Tutorial 4: Loading Textures

Learn to load and use textures with Zest's bindless descriptor system.

**Example:** `examples/GLFW/zest-pbr-texture`

## What You'll Learn

- Loading images (PNG, KTX)
- Creating samplers
- Bindless texture access
- Texture arrays

## Loading a Texture

This example shows loading an image with stb_image but you can use any other image loader that suits you.

```cpp
// Load pixels
int width, height, channels;
unsigned char* pixels = stbi_load("texture.png", &width, &height, &channels, 4);

// Create image info
zest_image_info_t info = zest_CreateImageInfo(width, height);
info.format = zest_format_r8g8b8a8_unorm;
info.flags = zest_image_preset_texture_mipmaps;

// Create image
zest_image_handle handle = zest_CreateImageWithPixels(device, pixels, width * height * 4, &info);
stbi_image_free(pixels);
```

## Loading KTX

```cpp
#include <zest_utilities.h>

zest_image_handle texture = zest_LoadKTX(device, "my_texture", "assets/texture.ktx");
```

## Creating a Sampler

```cpp
zest_sampler_info_t sampler_info = zest_CreateSamplerInfo();
sampler_info.min_filter = zest_filter_linear;
sampler_info.mag_filter = zest_filter_linear;
sampler_info.address_mode_u = zest_sampler_address_mode_repeat;
sampler_info.address_mode_v = zest_sampler_address_mode_repeat;

zest_sampler_handle sampler = zest_CreateSampler(device, &sampler_info);
```

## Acquiring Bindless Indices

```cpp
zest_image image = zest_GetImage(texture_handle);
zest_sampler sampler_obj = zest_GetSampler(sampler);

zest_uint tex_index = zest_AcquireSampledImageIndex(device, image, zest_texture_2d_binding);
zest_uint sampler_index = zest_AcquireSamplerIndex(device, sampler_obj);
```

## Using in Shaders

```glsl
layout(set = 0, binding = 0) uniform sampler samplers[];
layout(set = 0, binding = 1) uniform texture2D textures[];

layout(push_constant) uniform Push {
    uint tex_index;
    uint sampler_index;
} push;

void main() {
    vec4 color = texture(
        sampler2D(textures[push.tex_index], samplers[push.sampler_index]),
        uv
    );
}
```

## Full Example

See `examples/GLFW/zest-pbr-texture/zest-pbr-texture.cpp` for complete implementation with PBR textures.

## Next Steps

- [Tutorial 5: Compute Shaders](05-compute.md)
- [Images Concept](../concepts/images.md) - Deep dive on image types
- [Bindless Concept](../concepts/bindless.md) - Descriptor system details
