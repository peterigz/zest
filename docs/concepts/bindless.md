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

- **GLSL/SPIR-V (Vulkan and Metal):** `layout(set = 0, binding = <slot>)` as shown in the examples below.
  The Metal backend converts the SPIR-V to MSL itself, so the same shader runs on both.
- **HLSL (D3D12):** each slot is an unbounded array in a register space of its own, as shown in
  [HLSL Descriptor Layout](#hlsl-descriptor-layout) below.
- **MSL (Metal):** slot N is the buffer argument at index 5 + N, an array of 8-byte entries, as shown in
  [MSL Descriptor Layout](#msl-descriptor-layout) below. Only needed for shaders you write in MSL yourself.

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

SamplerState zest_samplers[] : register(s0, space100);                  // zest_sampler_binding
Texture2D zest_textures[] : register(t0, space101);                     // zest_texture_2d_binding
TextureCube zest_cubemaps[] : register(t0, space102);                   // zest_texture_cube_binding
Texture2DArray zest_texture_arrays[] : register(t0, space103);          // zest_texture_array_binding
Texture3D zest_textures_3d[] : register(t0, space104);                  // zest_texture_3d_binding
ByteAddressBuffer zest_buffers[] : register(t0, space105);              // zest_storage_buffer_binding, read only
RWByteAddressBuffer zest_rw_buffers[] : register(u0, space105);         // zest_storage_buffer_binding, read and write
RWTexture2D<float4> zest_images[] : register(u0, space106);             // zest_storage_image_binding
RWTexture2DArray<float4> zest_image_arrays[] : register(u0, space206);  // zest_storage_image_binding, array and cube images
ConstantBuffer<camera_t> zest_uniforms[] : register(b0, space107);      // zest_uniform_buffer_binding
TextureCubeArray zest_cube_arrays[] : register(t0, space108);           // zest_texture_cube_array_binding

float4 main(float2 uv : TEXCOORD0) : SV_Target0 {
    return zest_textures[NonUniformResourceIndex(texture_index)].Sample(zest_samplers[NonUniformResourceIndex(sampler_index)], uv);
}
```

- Every resource is an unbounded array (`name[]`), one per register class per space. The only
  resource that isn't an array is the push block, a constant buffer in space 0. Anything else fails
  pipeline creation with a message naming its register and space.
- Read-only and read-write storage buffer views are different register classes (`t` and `u`), so
  both can use space 105.
- A second array of the same register class at one binding goes in space `200 + binding number`,
  and a third in `300 + binding number`, as an unbounded array claims every register above its own.
  So `RWTexture2DArray` sits in space 206 beside `RWTexture2D` in 106, and two `ConstantBuffer`
  types read from `zest_uniform_buffer_binding` use spaces 107 and 207. `RWTexture2DArray` writes
  array and cube storage images, a cube as its six faces, indexed `uint3(x, y, face)`.
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
  sampled on the graphics queue in `DIRECT_QUEUE_GENERIC_READ`. A copy queue can only start a barrier
  from `COMMON`. When an imported image's graph starts on one queue and ends on another, the graph
  hands the image back to its first queue through `COMMON` at the end of each execution, so the next
  execution starts it there. Otherwise, if the next frame's first use of an image is on a queue type
  that can't start from the layout another queue left it in, its barrier is reported and skipped. On
  the copy queue that's any layout but `COMMON`, so a transfer pass there can only be an image's first
  use in a frame when the image is fresh or was handed back by the same graph. Inside one frame, images
  hand over between queues through `COMMON` and need nothing from the app.

**Raw and structured storage buffers.** `zest_AcquireStorageBufferIndex` gives a raw view: read it as
`ByteAddressBuffer` or `RWByteAddressBuffer` with byte offsets. `zest_AcquireStructuredBufferIndex(device,
buffer, stride)` gives a structured view for `StructuredBuffer<T>` and `RWStructuredBuffer<T>`; the
buffer's offset must be a multiple of the stride, so create it with `element_alignment` set to the
stride. The stride is at most 2048 bytes (`ZEST_MAX_STRUCTURED_STRIDE`) and no larger than the buffer,
on both backends. Instance layer buffers get structured views with the instance size as the stride. On
Vulkan both functions give the same index. Structured buffers that use their hidden counter (append,
consume and `IncrementCounter`) aren't supported.

Acquire a structured index for every buffer a shader declares as `StructuredBuffer<T>` or
`RWStructuredBuffer<T>`, with `sizeof(T)` as the stride. A raw view behind a structured declaration reads the
wrong bytes on D3D12 without any debug layer error. Only GPU-based validation reports it, as a structured buffer
byte stride mismatch. Slang's `StructuredBuffer<T>` is a structured declaration on the DXIL target.

**Transient buffers.** `zest_GetTransientBufferBindlessIndex(command_list, resource)` gives a raw view of a
transient, and `zest_GetTransientStructuredBufferIndex(command_list, resource, stride)` a structured one. A
transient read as several types takes one view per stride, up to `ZEST_MAX_TRANSIENT_BUFFER_VIEWS` (4) in one
execution, and asking for the same stride again gives back the same index. Each view starts at the buffer's
first byte, so a section of the buffer read as `T` must start on a multiple of `sizeof(T)` and is indexed in
rows of `T` from the start. Create the resource with `element_alignment` in its `zest_buffer_resource_info_t`
set to a common multiple of every stride. A fifth stride, or a stride the buffer's offset isn't a multiple of,
is a validation error and gives `ZEST_INVALID`. The indexes are released with the frame.

**Strides per target.** A struct's stride in a `StructuredBuffer` follows the shader's target: std430 on
Vulkan, D3D packing on D3D12. They differ for a `float3` followed by anything but a scalar, which std430
rounds up to 16 bytes, so a struct of two `float3` is 32 bytes on Vulkan and 24 on D3D12. Pad such members
explicitly so that one C struct matches both, and check with `zest_slang_GetTypeSize`, which gives the
stride for the session's device.

**Depth bias.** Every D3D12 pipeline bakes its `rasterization.depth_bias_*` values, and a command list
uses them until it calls `zest_cmd_SetDepthBias`. That call needs a driver with dynamic depth bias
(`D3D12_FEATURE_DATA_D3D12_OPTIONS16::DynamicDepthBiasSupported`); without one it reports once per
command list and the pipeline's own values stay. On Vulkan a pipeline with `depth_bias_enable` has a
dynamic bias that ignores the template's values, so call `zest_cmd_SetDepthBias` after binding it.

### MSL Descriptor Layout

GLSL, Slang and SPIR-V all run on the Metal backend unchanged, so MSL is only for shaders you'd rather write
for Metal yourself. Mark the source as MSL with `zest_SetShaderOptionsMSL` and create the shader as usual:

```cpp
zest_shader_options options = zest_CreateShaderOptions(device);
zest_SetShaderOptionsMSL(options, 0, 0, 0);    //A compute shader passes its threads per threadgroup here instead
zest_shader_handle vertex = zest_CreateShader(device, msl_source, zest_vertex_shader, "my_vertex", options, ZEST_FALSE);
zest_shader_handle fragment = zest_CreateShader(device, msl_source, zest_fragment_shader, "my_fragment", options, ZEST_FALSE);
zest_FreeShaderOptions(options);
```

Vulkan and D3D12 refuse MSL options with a validation error and give a zero handle. On Metal the source
is compiled when a pipeline or compute first uses it.

**The table.** Each slot is a buffer argument holding one 8-byte entry per index: a texture's or
sampler's resource ID, or a buffer's GPU address. Metal won't take a pointer straight to a texture,
sampler or buffer pointer as a buffer argument, so declare each slot as a pointer to a struct holding
one entry:

| Binding | Slot | Buffer index | Entry struct |
|---------|------|--------------|--------------|
| 0 | `zest_sampler_binding` | 5 | `struct { sampler value; }` |
| 1 | `zest_texture_2d_binding` | 6 | `struct { texture2d<float> value; }` |
| 2 | `zest_texture_cube_binding` | 7 | `struct { texturecube<float> value; }` |
| 3 | `zest_texture_array_binding` | 8 | `struct { texture2d_array<float> value; }` |
| 4 | `zest_texture_3d_binding` | 9 | `struct { texture3d<float> value; }` |
| 5 | `zest_storage_buffer_binding` | 10 | `struct { device T *value; }` |
| 6 | `zest_storage_image_binding` | 11 | `struct { texture2d<float, access::read_write> value; }` |
| 7 | `zest_uniform_buffer_binding` | 12 | `struct { constant T *value; }` |
| 8 | `zest_texture_cube_array_binding` | 13 | `struct { texturecube_array<float> value; }` |

The argument is then `const device Entry *name [[buffer(5 + slot)]]`, indexed with the index you
acquired: `textures[index].value.sample(samplers[sampler_index].value, uv)`.

**The rest of the layout.**
- Push constants are a `constant T &` at buffer index 4, up to 128 bytes (`ZEST_MAX_PUSH_SIZE`).
- Vertex binding N is buffer index N, for bindings 0 to 3, and each attribute is a `[[stage_in]]`
  member marked `[[attribute(location)]]`, with the location `zest_AddVertexAttribute` gives it.
- **Y.** Zest's clip space has Y pointing down, as Vulkan's does
  ([Rendering Conventions](rendering-conventions.md)). The backend flips Y in every vertex shader it
  converts from SPIR-V, so a vertex function you write in MSL negates Y itself:
  `position.y = -position.y`.
- **Functions.** A source can hold several functions. The backend takes the one function of the
  shader's stage (`vertex`, `fragment` or `kernel`), so one source can give a vertex shader and a
  fragment shader. A source with none or more than one of the stage fails pipeline creation, naming
  the functions it found.
- **Compute.** MSL can't declare a kernel's threads per threadgroup, so they come from
  `zest_SetShaderOptionsMSL(options, x, y, z)`. A kernel with a 0 there fails compute creation.
  `zest_cmd_DispatchCompute` takes group counts, as on the other backends.
- **No preprocessor pass.** `ZEST_METAL` isn't defined for MSL, and the options' macro definitions
  aren't applied. Metal compiles the text as it is.
- **Not supported:** a multiview pipeline (use GLSL, Slang or SPIR-V for one), and function
  constants, as the backend doesn't specialise them. MSL shaders are never cached, as compiling them
  is a copy of the text. `zest_ValidateShader` checks GLSL or HLSL only.

A textured draw in MSL, with the vertex and fragment functions in one source:

```metal
#include <metal_stdlib>
using namespace metal;

struct SamplerEntry { sampler value; };
struct TextureEntry { texture2d<float> value; };
struct DrawPush { uint texture_index; uint sampler_index; };
struct DrawInput { float2 position [[attribute(0)]]; float2 uv [[attribute(1)]]; };
struct DrawVaryings { float4 position [[position]]; float2 uv; };

vertex DrawVaryings draw_vertex(DrawInput input [[stage_in]]) {
	DrawVaryings varyings;
	varyings.position = float4(input.position.x, -input.position.y, 0.0, 1.0);
	varyings.uv = input.uv;
	return varyings;
}

fragment float4 draw_fragment(DrawVaryings varyings [[stage_in]], constant DrawPush &push [[buffer(4)]],
		const device SamplerEntry *samplers [[buffer(5)]], const device TextureEntry *textures [[buffer(6)]]) {
	return textures[push.texture_index].value.sample(samplers[push.sampler_index].value, varyings.uv);
}
```

And a kernel that writes through a storage buffer, created with `zest_SetShaderOptionsMSL(options, 8, 1, 1)`:

```metal
#include <metal_stdlib>
using namespace metal;

struct BufferEntry { device uint *value; };
struct WritePush { uint buffer_index; };

kernel void write_indexes(uint index [[thread_position_in_grid]], constant WritePush &push [[buffer(4)]],
		const device BufferEntry *buffers [[buffer(10)]]) {
	buffers[push.buffer_index].value[index] = index;
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
