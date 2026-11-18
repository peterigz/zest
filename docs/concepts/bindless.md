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

### Cross-backend binding contract

The slot numbers above are part of Zest's API contract and will not change. Your application code only
deals in slot numbers (`zest_binding_number_type`) and the indexes returned by the `zest_Acquire*Index`
functions, so it is identical on every backend. Only the shader-side declaration is per-language:

- **GLSL/SPIR-V (Vulkan, today):** `layout(set = 0, binding = <slot>)` as shown in the examples below.
- **HLSL (D3D12):** each slot is an unbounded array in a register space of its own, as shown in
  [HLSL Descriptor Layout](#hlsl-descriptor-layout) below.
- **MSL (Metal, future):** the backend maps each slot to an argument buffer id (`[[id(<slot>)]]`).

Writing shaders in [Slang](../../implementations/impl_slang.hpp) lets you declare bindings once and target
every backend.

Note also that samplers and images are always separate bindings — there is no combined image sampler
descriptor type (D3D12 and Metal have no equivalent). Combine them at sample time in the shader, e.g.
`texture(sampler2D(textures[i], samplers[j]), uv)`.

Descriptor update-after-bind behaviour is handled by the backend automatically: you can acquire new
indexes mid-frame for everything except uniform buffers (see [Uniform Buffers](#uniform-buffers) below).

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

Create uniform buffers during setup, not mid-frame. Their descriptors are written once at creation and
must not be updated while the set is in flight (the global layout deliberately supports older/mobile GPUs
here). All other resource types can be acquired mid-frame.

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

### HLSL Descriptor Layout

The D3D12 backend reads each pipeline's bindings from its compiled shader, so a bindless array can sit
in any free register space: its type decides which slot it reads. The recommended layout puts each
array at register 0 of space `100 + binding number`, and the push constants in a constant buffer at
`b0` in space 0:

```hlsl
cbuffer zest_push : register(b0, space0) {
    uint texture_index;
    uint sampler_index;
    uint buffer_index;
};

SamplerState zest_samplers[] : register(s0, space100);              // zest_sampler_binding
Texture2D zest_textures[] : register(t0, space101);                 // zest_texture_2d_binding
TextureCube zest_cubemaps[] : register(t0, space102);               // zest_texture_cube_binding
Texture2DArray zest_texture_arrays[] : register(t0, space103);      // zest_texture_array_binding
Texture3D zest_textures_3d[] : register(t0, space104);              // zest_texture_3d_binding
ByteAddressBuffer zest_buffers[] : register(t0, space105);          // zest_storage_buffer_binding, read only
RWByteAddressBuffer zest_rw_buffers[] : register(u0, space105);     // zest_storage_buffer_binding, read and write
RWTexture2D<float4> zest_images[] : register(u0, space106);         // zest_storage_image_binding
ConstantBuffer<camera_t> zest_uniforms[] : register(b0, space107);  // zest_uniform_buffer_binding
TextureCubeArray zest_cube_arrays[] : register(t0, space108);       // zest_texture_cube_array_binding

float4 main(float2 uv : TEXCOORD0) : SV_Target0 {
    return zest_textures[NonUniformResourceIndex(texture_index)].Sample(zest_samplers[NonUniformResourceIndex(sampler_index)], uv);
}
```

- Every resource is an unbounded array (`name[]`), one per register class per space. The only
  resource that isn't an array is the push block, a constant buffer in space 0. Anything else fails
  pipeline creation with a message naming its register and space.
- Read-only and read-write storage buffer views are different register classes (`t` and `u`), so
  both can use space 105.
- Spaces below 100 are left for custom descriptor sets, which the D3D12 backend doesn't support yet.
- `NonUniformResourceIndex` costs nothing on a uniform index and is required on one that varies
  within a draw.
- The push block holds up to 128 bytes (`ZEST_MAX_PUSH_SIZE`). A pipeline layout with a larger push
  constant range fails pipeline creation.
- `SV_InstanceID` starts at 0 whatever `first_instance` the draw was given, and in an indexed draw
  `SV_VertexID` doesn't include `vertex_offset`. GLSL's `gl_InstanceIndex` and `gl_VertexIndex`
  include both. A shader that indexes a buffer by instance, such as one reading an instance layer
  drawn with `zest_cmd_DrawLayerInstruction` (which passes the instruction's `start_index` as
  `first_instance`), needs the first instance in its push constants. Per-instance vertex attributes
  are offset by `first_instance` on both backends.
- **Multiview.** A pipeline whose template has a view count (`zest_SetPipelineViewCount`) reads the view
  in `SV_ViewID`, which needs shader model 6.1. View i renders to layer i of the pass's layered
  attachments. D3D12 draws at most 4 views (`D3D12_MAX_VIEW_INSTANCE_COUNT`), and a render pass whose
  attachments have more layers than the device's view count fails its graph on both backends. The
  template's view count must equal the pass's layer count, as Vulkan requires, and on D3D12 a pipeline
  that doesn't match isn't built.
- **Indirect draws.** The `offset` of `zest_cmd_DrawIndirect` and `zest_cmd_DrawIndexedIndirect` is a draw
  index on both backends, so the commands are read from `offset * stride` bytes into the buffer. On
  D3D12 the stride is a multiple of 4 and at least the size of the command, and each draw type takes at
  most 8 different strides (`ZEST_DX_MAX_INDIRECT_STRIDES`). A draw outside those limits is reported and
  skipped.
- **Images used on another queue type in the next frame.** D3D12 has layouts that only one queue type
  can use. A frame graph leaves an image read in `zest_image_layout_general` on the queue that read it
  in `DIRECT_QUEUE_COMMON` or `COMPUTE_QUEUE_COMMON`, and a depth image that was both depth tested and
  sampled on the graphics queue in `DIRECT_QUEUE_GENERIC_READ`. If the next frame's first use of that
  image is on another queue type, its barrier is reported and skipped. A copy queue can only start a
  barrier from `COMMON`, so a transfer pass that runs on the copy queue can't be the first use in a
  frame of an image another queue left in a different layout: the debug layer rejects the barrier
  (message 1334), and nothing reports it. Inside one frame, images hand over between queues through
  `COMMON` and need nothing from the app.

**Raw and structured storage buffers.** `zest_AcquireStorageBufferIndex` gives a raw view: read it as
`ByteAddressBuffer` or `RWByteAddressBuffer` with byte offsets. `zest_AcquireStructuredBufferIndex(device,
buffer, stride)` gives a structured view for `StructuredBuffer<T>` and `RWStructuredBuffer<T>`; the
buffer's offset must be a multiple of the stride, so create it with `element_alignment` set to the
stride. The stride is at most 2048 bytes (`ZEST_MAX_STRUCTURED_STRIDE`) and no larger than the buffer,
on both backends. Instance layer buffers get structured views with the instance size as the stride. On
Vulkan both functions give the same index. Structured buffers that use their hidden counter (append,
consume and `IncrementCounter`) aren't supported.

**Depth bias.** Every D3D12 pipeline bakes its `rasterization.depth_bias_*` values, and a command list
uses them until it calls `zest_cmd_SetDepthBias`. That call needs a driver with dynamic depth bias
(`D3D12_FEATURE_DATA_D3D12_OPTIONS16::DynamicDepthBiasSupported`); without one it reports once per
command list and the pipeline's own values stay. On Vulkan a pipeline with `depth_bias_enable` has a
dynamic bias that ignores the template's values, so call `zest_cmd_SetDepthBias` after binding it.

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
#extension GL_EXT_nonuniform_qualifier : require

layout(location = 3) in flat uint in_texture_index;

void main() {
    // Varies per instance within the draw, so this index is NOT dynamically uniform
    // and must be wrapped. Requires zest_capability_nonuniform_sampled_image_indexing.
    vec4 color = texture(
        sampler2D(textures[nonuniformEXT(in_texture_index)], samplers[0]),
        uv
    );
}
```

If you need to support hardware without that capability, use a texture array and vary the *layer*
per instance instead — the layer index is an ordinary integer, not a descriptor index, so it carries
no such requirement. That is what Zest's own billboard and font shaders do.

## Best Practices

1. **Acquire indices at load time** - Not every frame
2. **Release indices when done** - Prevents descriptor pool exhaustion
3. **Use push constants for dynamic indices** - Fast to update
4. **Store static indices in instance data** - For per-object textures

## Limitations

- Maximum resources depend on GPU limits (usually 500K+ descriptors)
- Some older GPUs have lower limits
- `GL_EXT_nonuniform_qualifier` must be enabled in GLSL to declare unsized bindless arrays
  (`textures[]`), but see the note below before using `nonuniformEXT()` itself

## Uniform vs non-uniform indexing

Zest only requires **dynamically uniform** indexing of the bindless arrays — the core Vulkan 1.0
`shader*ArrayDynamicIndexing` features, which are near-universally supported. An index is dynamically
uniform when every invocation in a draw or dispatch reads the same value; a push constant always
qualifies, which is why every index in Zest's own shaders is delivered by push constant.

Indexing with a value that varies *within* a draw — a per-instance vertex attribute, or a value read
from a storage buffer per invocation — is **non-uniform**, and needs `nonuniformEXT()` in the shader
plus hardware support. That support is optional, so check for it before relying on it:

```cpp
if (zest_DeviceFeatureEnabled(device, zest_capability_nonuniform_sampled_image_indexing)) {
    // safe to use a shader variant with nonuniformEXT() on sampled-image indices
}
```

The matching capabilities are `zest_capability_nonuniform_sampled_image_indexing`,
`..._storage_buffer_indexing`, `..._uniform_buffer_indexing` and `..._storage_image_indexing`. All are
auto-enabled when the hardware supports them.

Do not add `nonuniformEXT()` "just in case": it emits the SPIR-V `ShaderNonUniform` capability, which
makes the shader fail to load on hardware that would otherwise have run it fine. The
[Per-Instance Indices](#per-instance-indices) pattern below is one of the cases that genuinely needs it.

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
