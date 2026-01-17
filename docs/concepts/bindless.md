# Bindless Descriptors

Zest uses a **bindless descriptor** model where all resources are indexed into global arrays. This eliminates per-object descriptor set management and enables flexible resource access.

## Why Bindless?

In the case of Vulkan (not so much with Direct X and Metal), it requires creating and binding separate descriptor sets for each object's resources. This creates overhead from:
- Allocating descriptor sets from pools
- Updating descriptors when resources change
- Binding different sets between draw calls

Bindless descriptors solve this by putting all resources into large arrays indexed by integers. You bind the global descriptor set once, then pass indices via push constants or uniform buffers to select which resources each draw call uses.

## How Bindless Works

The following pseudocode illustrates the conceptual difference:

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
// Single global set with all resources - bound once at frame start
// (Zest binds this automatically during frame graph execution)

// Pass indices via push constants to select resources
push.tex_index = object1_texture_index;
zest_cmd_SendPushConstants(cmd, &push, sizeof(push));
zest_cmd_Draw(cmd, vertex_count, 1, 0, 0);

push.tex_index = object2_texture_index;
zest_cmd_SendPushConstants(cmd, &push, sizeof(push));
zest_cmd_Draw(cmd, vertex_count, 1, 0, 0);
```

## Resource Types

Zest's bindless system supports:

| Binding | Constant | Resource Type | Array |
|---------|----------|---------------|-------|
| 0 | `zest_sampler_binding` | Samplers | `sampler[]` |
| 1 | `zest_texture_2d_binding` | 2D Textures | `texture2D[]` |
| 2 | `zest_texture_cube_binding` | Cube Textures | `textureCube[]` |
| 3 | `zest_texture_array_binding` | Texture Arrays | `texture2DArray[]` |
| 4 | `zest_texture_3d_binding` | 3D Textures | `texture3D[]` |
| 5 | `zest_storage_buffer_binding` | Storage Buffers | `buffer[]` |
| 6 | `zest_storage_image_binding` | Storage Images | `image2D[]` |
| 7 | `zest_uniform_buffer_binding` | Uniform Buffers | `uniform[]` |

Take note of the binding numbers as that's what you need to use to correctly set up your shaders.

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
zest_uint storage_index = zest_AcquireStorageImageIndex(device, image, zest_storage_image_binding);
```

### Storage Buffers

```cpp
zest_uint buffer_index = zest_AcquireStorageBufferIndex(device, buffer);
```

### Uniform Buffers

```cpp
//(Indexes are acquired automatically when the uniform buffer is created)
zest_uniform_buffer_handle ubo_handle = zest_CreateUniformBuffer(context, "camera", sizeof(camera_t));
zest_uniform_buffer ubo = zest_GetUniformBuffer(ubo_handle);
zest_uint ubo_index = zest_GetUniformBufferDescriptorIndex(ubo);
```

## Shader Setup

### GLSL Descriptor Layout

```glsl
#version 450
#extension GL_EXT_nonuniform_qualifier : enable

// Bindless arrays at set 0 (binding numbers match zest_binding_number_type)
layout(set = 0, binding = 0) uniform sampler samplers[];           // zest_sampler_binding
layout(set = 0, binding = 1) uniform texture2D textures[];         // zest_texture_2d_binding
layout(set = 0, binding = 2) uniform textureCube cubemaps[];       // zest_texture_cube_binding
layout(set = 0, binding = 3) uniform texture2D texture_arrays[];   // zest_texture_array_binding
layout(set = 0, binding = 5) buffer StorageBuffers {               // zest_storage_buffer_binding
    float data[];
} storage_buffers[];
layout(set = 0, binding = 6, rgba16f) uniform image2D storage_images[];  // zest_storage_image_binding
layout(set = 0, binding = 7) uniform UniformBuffers {              // zest_uniform_buffer_binding
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
    app_t* app = (app_t*)data;

    push_constants_t push = {};
    push.transform = app->model_matrix;
    push.texture_index = app->texture_index;
    push.sampler_index = app->sampler_index;
    push.ubo_index = app->ubo_index;
    push.time = app->current_time;

    zest_cmd_SendPushConstants(cmd, &push, sizeof(push));
    zest_cmd_Draw(cmd, 6, 1, 0, 0);  // vertex_count, instance_count, first_vertex, first_instance
}
```

## Releasing Indices

```cpp
// Release image indices (sampled or storage images)
// Pass the image and the binding type used when acquiring
zest_ReleaseImageIndex(device, image, zest_texture_2d_binding);
zest_ReleaseImageIndex(device, image, zest_storage_image_binding);

// Release storage buffer index (uses array index directly)
zest_ReleaseStorageBufferIndex(device, buffer_index);
```

Note: Image indices, Sampler indices and uniform buffer indices are managed automatically when the resource is freed.

## Bindless Layout Access

Access the device's bindless descriptor set layout:

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

## Limitations

- Maximum resources depend on GPU limits (usually 500K+ descriptors)
- Some older GPUs have lower limits
- `GL_EXT_nonuniform_qualifier` required for non-uniform indexing in GLSL shaders

## Example: Multi-Textured Scene

```cpp
// At load time
struct object_t {
    zest_uint texture_index;
    zest_uint normal_index;
    zest_uint index_count;
    zest_uint index_offset;
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
    scene_t* scene = (scene_t*)data;
    push_constants_t push = {};

    for (int i = 0; i < scene->object_count; i++) {
        push.texture_index = scene->objects[i].texture_index;
        push.normal_index = scene->objects[i].normal_index;
        zest_cmd_SendPushConstants(cmd, &push, sizeof(push));
        zest_cmd_DrawIndexed(cmd,
            scene->objects[i].index_count,  // index_count
            1,                               // instance_count
            scene->objects[i].index_offset, // first_index
            0,                               // vertex_offset
            0);                              // first_instance
    }
}
```

## See Also

- [Images](images.md) - Image creation and bindless indices
- [Pipelines](pipelines.md) - Push constant configuration
- [API Reference](../api-reference/index.md) - Bindless functions
