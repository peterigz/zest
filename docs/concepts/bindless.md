# Bindless Descriptors

Zest uses a **bindless descriptor** model where all resources are indexed into global arrays. This eliminates per-object descriptor set management and enables flexible resource access.

## How Bindless Works

### Traditional Model

```cpp
// Per-object descriptor sets (Vulkan traditional)
VkDescriptorSet object1_set = CreateDescriptorSet(texture1, sampler);
VkDescriptorSet object2_set = CreateDescriptorSet(texture2, sampler);

// Must bind different sets per object
vkCmdBindDescriptorSets(..., object1_set);
vkCmdDraw(...);
vkCmdBindDescriptorSets(..., object2_set);
vkCmdDraw(...);
```

### Bindless Model

```cpp
// Single global set with all resources
// Bind once, use indices to select resources
zest_cmd_BindDescriptorSets(cmd, device->bindless_set);

// Pass indices via push constants
push.tex_index = object1_texture_index;
zest_cmd_SendPushConstants(cmd, &push, sizeof(push));
zest_cmd_Draw(...);

push.tex_index = object2_texture_index;
zest_cmd_SendPushConstants(cmd, &push, sizeof(push));
zest_cmd_Draw(...);
```

## Resource Types

Zest's bindless system supports:

| Binding | Resource Type | Array |
|---------|---------------|-------|
| 0 | Samplers | `sampler[]` |
| 1 | Sampled Images (2D) | `texture2D[]` |
| 2 | Storage Images | `image2D[]` |
| 3 | Storage Buffers | `buffer[]` |
| 4 | Uniform Buffers | `uniform[]` |
| 5+ | Additional bindings | Custom |

## Acquiring Indices

### Sampled Images (Textures)

```cpp
zest_image image = zest_GetImage(image_handle);

// Acquire index for 2D texture
zest_uint tex_index = zest_AcquireSampledImageIndex(
    device,
    image,
    zest_texture_2d_binding  // Binding type
);
```

### Samplers

```cpp
zest_sampler sampler = zest_GetSampler(sampler_handle);
zest_uint sampler_index = zest_AcquireSamplerIndex(device, sampler);
```

### Storage Images

```cpp
zest_uint storage_index = zest_AcquireStorageImageIndex(device, image);
```

### Storage Buffers

```cpp
zest_uint buffer_index = zest_AcquireStorageBufferIndex(device, buffer);
```

### Uniform Buffers

```cpp
zest_uniform_buffer ubo = zest_CreateUniformBuffer(context, "camera", sizeof(camera_t));
zest_uint ubo_index = zest_GetUniformBufferDescriptorIndex(ubo);
```

## Shader Setup

### GLSL Descriptor Layout

```glsl
#version 450
#extension GL_EXT_nonuniform_qualifier : enable

// Bindless arrays at set 0
layout(set = 0, binding = 0) uniform sampler samplers[];
layout(set = 0, binding = 1) uniform texture2D textures[];
layout(set = 0, binding = 2, rgba16f) uniform image2D storage_images[];
layout(set = 0, binding = 3) buffer StorageBuffers {
    float data[];
} storage_buffers[];
layout(set = 0, binding = 4) uniform UniformBuffers {
    mat4 view;
    mat4 projection;
} uniforms[];

// Push constants for indices
layout(push_constant) uniform PushConstants {
    uint texture_index;
    uint sampler_index;
    uint ubo_index;
} push;

void main() {
    // Sample texture using indices
    vec4 color = texture(
        sampler2D(textures[push.texture_index], samplers[push.sampler_index]),
        uv
    );

    // Access uniform buffer
    mat4 vp = uniforms[push.ubo_index].view * uniforms[push.ubo_index].projection;
}
```

### Slang Descriptor Layout

```slang
// Bindless resources
Sampler2D textures[];
SamplerState samplers[];

struct PushConstants {
    uint textureIndex;
    uint samplerIndex;
};
[[vk::push_constant]] PushConstants push;

float4 main() : SV_Target {
    return textures[push.textureIndex].Sample(samplers[push.samplerIndex], uv);
}
```

## Push Constants

Push constants are the primary way to pass indices to shaders.

### Defining Push Constants

```cpp
struct push_constants_t {
    zest_matrix4 transform;
    zest_uint texture_index;
    zest_uint sampler_index;
    zest_uint ubo_index;
    float time;
};
```

### Sending Push Constants

```cpp
void RenderCallback(zest_command_list cmd, void* data) {
    push_constants_t push = {};
    push.transform = model_matrix;
    push.texture_index = my_texture_index;
    push.sampler_index = my_sampler_index;
    push.ubo_index = my_ubo_index;
    push.time = current_time;

    zest_cmd_SendPushConstants(cmd, &push, sizeof(push));
    zest_cmd_Draw(cmd, ...);
}
```

## Releasing Indices

When resources are freed, release their indices:

```cpp
// Release texture index
zest_ReleaseSampledImageIndex(device, tex_index);

// Release sampler index
zest_ReleaseSamplerIndex(device, sampler_index);

// Release storage image index
zest_ReleaseStorageImageIndex(device, storage_index);

// Release storage buffer index
zest_ReleaseStorageBufferIndex(device, buffer_index);
```

## Bindless Layout Access

Access the device's bindless descriptor layout:

```cpp
VkDescriptorSetLayout layout = zest_GetBindlessLayout(device);
```

For custom descriptor sets:

```cpp
zest_descriptor_set_layout custom_layout = zest_CreateBindlessSet(device, &layout_info);
```

## Per-Instance Indices

For instanced rendering, store indices in instance data:

```cpp
struct instance_t {
    zest_vec3 position;
    zest_uint texture_index;  // Each instance can have different texture
    zest_uint material_index;
};
```

In shader:

```glsl
layout(location = 3) in uint in_texture_index;

void main() {
    vec4 color = texture(
        sampler2D(textures[in_texture_index], samplers[0]),
        uv
    );
}
```

## Best Practices

1. **Acquire indices at load time** - Not every frame
2. **Release indices when done** - Prevents descriptor pool exhaustion
3. **Use push constants for dynamic indices** - Fast to update
4. **Store static indices in instance data** - For per-object textures
5. **Keep index ranges contiguous** - Helps GPU cache efficiency

## Limitations

- Maximum resources depend on GPU limits (usually 500K+ descriptors)
- Some older GPUs have lower limits
- `GL_EXT_nonuniform_qualifier` required for non-uniform indexing

## Example: Multi-Textured Scene

```cpp
// At load time
struct object_t {
    zest_uint texture_index;
    zest_uint normal_index;
    zest_uint material_index;
};

object_t objects[MAX_OBJECTS];
for (int i = 0; i < object_count; i++) {
    objects[i].texture_index = zest_AcquireSampledImageIndex(device,
        zest_GetImage(textures[i]), zest_texture_2d_binding);
    objects[i].normal_index = zest_AcquireSampledImageIndex(device,
        zest_GetImage(normals[i]), zest_texture_2d_binding);
}

// At render time
void RenderCallback(zest_command_list cmd, void* data) {
    for (int i = 0; i < object_count; i++) {
        push.texture_index = objects[i].texture_index;
        push.normal_index = objects[i].normal_index;
        zest_cmd_SendPushConstants(cmd, &push, sizeof(push));
        zest_cmd_DrawIndexed(cmd, ...);
    }
}
```

## See Also

- [Images](images.md) - Image creation and bindless indices
- [Pipelines](pipelines.md) - Push constant configuration
- [API Reference](../api-reference/index.md) - Bindless functions
