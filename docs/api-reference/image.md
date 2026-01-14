# Image API

Functions for image and sampler management.

## Image Creation

### zest_CreateImageInfo

Create image configuration.

```cpp
zest_image_info_t zest_CreateImageInfo(zest_uint width, zest_uint height);
```

Set `info.flags` to a preset: `zest_image_preset_texture_mipmaps`, `zest_image_preset_render_target`, `zest_image_preset_depth`, etc.

---

### zest_CreateImage

```cpp
zest_image_handle zest_CreateImage(zest_device device, zest_image_info_t *info);
```

---

### zest_CreateImageWithPixels

```cpp
zest_image_handle zest_CreateImageWithPixels(
    zest_device device,
    void *pixels,
    zest_size pixel_size,
    zest_image_info_t *info
);
```

---

### zest_GetImage

Get image object from handle.

```cpp
zest_image zest_GetImage(zest_image_handle handle);
```

---

### zest_FreeImage

Deferred image destruction.

```cpp
void zest_FreeImage(zest_image_handle handle);
```

---

## Samplers

### zest_CreateSamplerInfo

Create default sampler configuration.

```cpp
zest_sampler_info_t zest_CreateSamplerInfo(void);
zest_sampler_info_t zest_CreateSamplerInfoRepeat(void);
```

---

### zest_CreateSampler

```cpp
zest_sampler_handle zest_CreateSampler(zest_device device, zest_sampler_info_t *info);
```

---

### zest_GetSampler

```cpp
zest_sampler zest_GetSampler(zest_sampler_handle handle);
```

---

## Bindless Indices

### zest_AcquireSampledImageIndex

```cpp
zest_uint zest_AcquireSampledImageIndex(
    zest_device device,
    zest_image image,
    zest_texture_binding binding
);
```

---

### zest_AcquireSamplerIndex

```cpp
zest_uint zest_AcquireSamplerIndex(zest_device device, zest_sampler sampler);
```

---

### zest_AcquireStorageImageIndex

```cpp
zest_uint zest_AcquireStorageImageIndex(zest_device device, zest_image image);
```

---

### zest_ReleaseSampledImageIndex / zest_ReleaseSamplerIndex

Release bindless indices.

```cpp
void zest_ReleaseSampledImageIndex(zest_device device, zest_uint index);
void zest_ReleaseSamplerIndex(zest_device device, zest_uint index);
```

---

## Image Views

### zest_CreateImageView

```cpp
zest_image_view zest_CreateImageView(zest_image image, zest_uint mip, zest_uint layer);
```

---

### zest_CreateImageViewsPerMip

```cpp
zest_image_view* zest_CreateImageViewsPerMip(zest_image image);
```

---

## See Also

- [Images Concept](../concepts/images.md)
- [Bindless Concept](../concepts/bindless.md)
