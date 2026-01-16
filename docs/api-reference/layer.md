# Layer API

Layers are the primary mechanism for batching and drawing instanced geometry (sprites, billboards, meshes). They manage staging buffers, draw instructions, and automatic buffer growth.

You can consider layers as a convenient way to do instanced based drawing but they're not essential and you can easily roll your own method depending on what you need.

## Layer Types

Zest provides three layer types:

- **Instance Layer** - For sprites, billboards, and 2D quads. Each instance is a single struct drawn as a quad or whatever else you construct in the vertex shader.
- **Mesh Layer** - For dynamic geometry where you control vertices and indices directly.
- **Instance Mesh Layer** - For instanced 3D meshes. Upload meshes once, then draw many instances.

---

## Layer Creation

### zest_CreateInstanceLayer

Create an instance layer for drawing sprites, billboards, or other small pieces of geometry that you construct in the vertex shader. The layer manages staging buffers that auto-grow as needed.

```cpp
zest_layer_handle zest_CreateInstanceLayer(
    zest_context context,
    const char *name,
    zest_size type_size
);
```

**Parameters:**
- `context` - The Zest context
- `name` - Unique name for the layer
- `type_size` - Size of your instance struct in bytes (e.g., `sizeof(my_sprite_t)`)

**Returns:** Handle to the created layer.

**Example:**
```cpp
typedef struct {
    zest_vec4 position;  // x, y, z, scale
    zest_vec4 uv;        // u, v, width, height
    zest_uint color;     // packed RGBA
} sprite_instance_t;

zest_layer_handle sprites = zest_CreateInstanceLayer(context, "sprites", sizeof(sprite_instance_t));
```

---

### zest_CreateFIFInstanceLayer

Create an instance layer with separate buffers for each frame-in-flight. Use this when you need manual control over buffer uploads rather than using transient buffers each frame.

```cpp
zest_layer_handle zest_CreateFIFInstanceLayer(
    zest_context context,
    const char *name,
    zest_size type_size,
    zest_uint max_instances
);
```

**Parameters:**
- `context` - The Zest context
- `name` - Unique name for the layer
- `type_size` - Size of your instance struct in bytes
- `max_instances` - Initial capacity for instances

**Returns:** Handle to the created layer.

---

### zest_CreateMeshLayer

Create a mesh layer for dynamic geometry where you build vertices and indices each frame or just have a static mesh that you upload once and draw whenever you need.

```cpp
zest_layer_handle zest_CreateMeshLayer(
    zest_context context,
    const char *name,
    zest_size vertex_type_size
);
```

**Parameters:**
- `context` - The Zest context
- `name` - Unique name for the layer
- `vertex_type_size` - Size of your vertex struct in bytes

**Returns:** Handle to the created layer.

**Example:**
```cpp
typedef struct {
    zest_vec3 position;
    zest_vec2 uv;
    zest_uint color;
} mesh_vertex_t;

zest_layer_handle dynamic_mesh = zest_CreateMeshLayer(context, "dynamic_geo", sizeof(mesh_vertex_t));
```

---

### zest_CreateInstanceMeshLayer

Create an instance mesh layer for drawing many instances of pre-uploaded meshes. Add meshes to the layer with `zest_AddMeshToLayer`, then draw instances each frame using an index to the mesh in the layer that you want to draw.

You must calculate the required vertex and index capacity that you'll need ahead of time as the vertex and index capacity of the layer will not be grown as needed.

```cpp
zest_layer_handle zest_CreateInstanceMeshLayer(
    zest_context context,
    const char *name,
    zest_size instance_struct_size,
    zest_size vertex_capacity,
    zest_size index_capacity
);
```

**Parameters:**
- `context` - The Zest context
- `name` - Unique name for the layer
- `instance_struct_size` - Size of your instance struct in bytes
- `vertex_capacity` - Initial vertex buffer capacity in bytes
- `index_capacity` - Initial index buffer capacity in bytes

**Returns:** Handle to the created layer.

**Example:**
```cpp
typedef struct {
    zest_matrix4 transform;
    zest_vec4 color;
} mesh_instance_t;

zest_layer_handle meshes = zest_CreateInstanceMeshLayer(
    context, "3d_meshes",
    sizeof(mesh_instance_t),
    1024 * 1024,  // 1MB vertex buffer
    512 * 1024    // 512KB index buffer
);
```

---

### zest_GetLayer

Get a layer pointer from a handle. Call once per frame and reuse the pointer for better performance.

```cpp
zest_layer zest_GetLayer(zest_layer_handle handle);
```

**Parameters:**
- `handle` - Layer handle from creation

**Returns:** Layer pointer for use with other layer functions.

**Example:**
```cpp
zest_layer layer = zest_GetLayer(sprite_layer_handle);
// Use 'layer' for all subsequent operations this frame
```

---

### zest_FreeLayer

Free a layer and all its resources.

```cpp
void zest_FreeLayer(zest_layer_handle layer);
```

---

## Instance Recording

### zest_StartInstanceInstructions

Begin recording instances for a layer. This is generally used internally but can be used for more advanced things where you want manage your own instance instructions.

```cpp
void zest_StartInstanceInstructions(zest_layer layer);
```

**Usage:** Prepares the layer for receiving new instance data. Must be paired with `zest_EndInstanceInstructions`.

---

### zest_EndInstanceInstructions

End recording instances for a layer. Call after all instances are added. For advanced used if your managing your own instanct instructions. This will add the current instruction to the list of instructions in the layer ready to be processed when rendering and executing draw calls.

```cpp
void zest_EndInstanceInstructions(zest_layer layer);
```

**Usage:** Finalizes the current draw instruction. Required before the layer can be drawn.

---

### zest_MaybeEndInstanceInstructions

Conditionally end instance instructions if the frame-in-flight changed. Useful for FIF layers with manual buffer management.

```cpp
zest_bool zest_MaybeEndInstanceInstructions(zest_layer layer);
```

**Returns:** `ZEST_TRUE` if instructions were ended, `ZEST_FALSE` otherwise.

---

### zest_NextInstance

Get a pointer to the next instance slot and advance the instance pointer. You'll need to use this function when creating your "DrawSprite/DrawBillboard/DrawMeshInstance" function.

```cpp
void* zest_NextInstance(zest_layer layer);
```

**Returns:** Pointer to the next instance slot. Cast to your instance struct type.

**Example:**
```cpp
zest_SetInstanceInstructions(layer, pipeline_template);

for (int i = 0; i < sprite_count; i++) {
    sprite_instance_t *instance = (sprite_instance_t*)zest_NextInstance(layer);
    instance->position = sprites[i].position;
    instance->uv = sprites[i].uv;
    instance->color = sprites[i].color;
}
```

---

### zest_DrawInstanceBuffer

Copy a buffer of pre-prepared instances directly into the layer. Useful when instance data is computed elsewhere.

```cpp
zest_draw_buffer_result zest_DrawInstanceBuffer(
    zest_layer layer,
    void *src,
    zest_uint amount
);
```

**Parameters:**
- `layer` - The layer to draw to
- `src` - Source buffer containing instance data
- `amount` - Number of instances to copy

**Returns:** Result enum:
- `zest_draw_buffer_result_ok` - Success
- `zest_draw_buffer_result_buffer_grew` - Buffer was resized
- `zest_draw_buffer_result_failed_to_grow` - Failed to resize buffer

---

### zest_DrawInstanceInstruction

Record a draw instruction for a number of instances when writing directly to the staging buffer.

```cpp
void zest_DrawInstanceInstruction(zest_layer layer, zest_uint amount);
```

**Parameters:**
- `layer` - The layer
- `amount` - Number of instances to draw

---

## Instance Layer State

### zest_StartInstanceDrawing

Set the pipeline for the current draw instruction. Call this before adding instances that should use a specific pipeline.

```cpp
void zest_StartInstanceDrawing(zest_layer layer, zest_pipeline_template pipeline);
```

**Parameters:**
- `layer` - The layer
- `pipeline` - Pipeline template to use for drawing

**Example:**
```cpp
// Draw some sprites specifying a pipeline that you want to use for the draw call
zest_StartInstanceDrawing(layer, sprite_pipeline);
for (int i = 0; i < normal_sprites; i++) {
    sprite_instance_t *inst = (sprite_instance_t*)zest_NextInstance(layer);
    // ... fill instance data
}

// Switch pipeline for glowing sprites. This will end the previous instruction and start a new one.
zest_StartInstanceDrawing(layer, glow_pipeline);
for (int i = 0; i < glow_sprites; i++) {
    sprite_instance_t *inst = (sprite_instance_t*)zest_NextInstance(layer);
    // ... fill instance data
}
```

---

### zest_GetInstanceLayerCount

Get the current number of instances recorded in the layer.

```cpp
zest_uint zest_GetInstanceLayerCount(zest_layer layer);
```

---

### zest_GetLayerInstanceSize

Get the total size in bytes of all instances in the layer.

```cpp
zest_size zest_GetLayerInstanceSize(zest_layer layer);
```

---

## Mesh Layer Operations

### zest_AddMeshToLayer

Add a mesh to an instance mesh layer. The mesh data is uploaded to the GPU and can be instanced many times.

> Note that the meshes that you add **must** use the same vertex struct.

```cpp
zest_uint zest_AddMeshToLayer(
    zest_layer layer,
    zest_mesh src_mesh,
    zest_uint texture_index
);
```

**Parameters:**
- `layer` - Instance mesh layer
- `src_mesh` - Source mesh handle
- `texture_index` - Texture bindless index for this mesh

**Returns:** Mesh index for use with `zest_StartInstanceMeshDrawing`.

**Example:**
```cpp
zest_mesh cube_mesh = zest_CreateMesh(...);
zest_uint cube_id = zest_AddMeshToLayer(mesh_layer, cube_mesh, cube_texture_index);

// Later, when drawing:
zest_StartInstanceMeshDrawing(layer, cube_id, mesh_pipeline);
```

---

### zest_GetLayerMeshOffsets

Get offset data for a mesh in the layer. Useful for custom draw routines.

```cpp
const zest_mesh_offset_data_t* zest_GetLayerMeshOffsets(zest_layer layer, zest_uint mesh_index);
```

**Returns:** Pointer to offset data containing:
- `vertex_offset` - Offset in vertex buffer
- `index_offset` - Offset in index buffer
- `vertex_count` - Number of vertices
- `index_count` - Number of indices
- `texture_index` - Associated texture descriptor index

---

### zest_GetLayerMeshTextureIndex

Get the texture descriptor index associated with a mesh.

```cpp
zest_uint zest_GetLayerMeshTextureIndex(zest_layer layer, zest_uint mesh_index);
```

---

### zest_SetMeshDrawing

Set the pipeline for mesh layer drawing.

```cpp
void zest_SetMeshDrawing(zest_layer layer, zest_pipeline_template pipeline);
```

---

### zest_StartInstanceMeshDrawing

Set up drawing for a specific mesh with a pipeline. Call this and then add instances that you want to draw

```cpp
void zest_StartInstanceMeshDrawing(
    zest_layer layer,
    zest_uint mesh_index,
    zest_pipeline_template pipeline
);
```

**Example:**
```cpp
zest_StartInstanceMeshDrawing(mesh_layer, mesh_index, app->shadow_pipeline);
//Your own function to fill out instance data
void DrawInstancedMesh(zest_layer layer, position, rotation, scale) {
```

---

### zest_GetVertexWriteBuffer / zest_GetIndexWriteBuffer

Get the staging buffers for direct vertex/index writing.

```cpp
zest_buffer zest_GetVertexWriteBuffer(zest_layer layer);
zest_buffer zest_GetIndexWriteBuffer(zest_layer layer);
```

---

### zest_PushIndex

Push an index to the index staging buffer. Automatically grows the buffer if needed.

```cpp
void zest_PushIndex(zest_layer layer, zest_uint offset);
```

---

### zest_GrowMeshVertexBuffers / zest_GrowMeshIndexBuffers

Manually trigger buffer growth when you know more space is needed.

```cpp
void zest_GrowMeshVertexBuffers(zest_layer layer);
void zest_GrowMeshIndexBuffers(zest_layer layer);
```

---

### zest_GetLayerVertexMemoryInUse / zest_GetLayerIndexMemoryInUse

Get current memory usage for mesh layers.

```cpp
zest_size zest_GetLayerVertexMemoryInUse(zest_layer layer);
zest_size zest_GetLayerIndexMemoryInUse(zest_layer layer);
```

---

## Drawing

### zest_DrawInstanceLayer

Draw all instances in a layer. Use as a frame graph task callback.

```cpp
void zest_DrawInstanceLayer(const zest_command_list command_list, void *user_data);
```

**Parameters:**
- `command_list` - Command list from the frame graph
- `user_data` - Layer pointer (pass via `zest_SetPassTask`)

**Example:**
```cpp
zest_BeginRenderPass("sprites");
    zest_SetPassTask(zest_DrawInstanceLayer, sprite_layer);
zest_EndPass();
```

---

### zest_DrawInstanceMeshLayer

Draw all instances in an instance mesh layer. Use as a frame graph task callback.

```cpp
void zest_DrawInstanceMeshLayer(const zest_command_list command_list, void *user_data);
```

**Parameters:**
- `command_list` - Command list from the frame graph
- `user_data` - Layer pointer (pass via `zest_SetPassTask`)

**Example:**
```cpp
zest_BeginRenderPass("Meshes");
    zest_SetPassTask(zest_DrawInstanceMeshLayer, mesh_layer);
zest_EndPass();
```

---

### zest_DrawInstanceMeshLayerWithPipeline

Draw an instance mesh layer with a specific pipeline override. This is useful for when you need to draw multiple passes of the same meshes. For example one pass for rendering shadow depth followed by a normal render with the shadows. See the cascading shadows example for a demonstration of this.

```cpp
void zest_DrawInstanceMeshLayerWithPipeline(
    const zest_command_list command_list,
    zest_layer layer,
    zest_pipeline_template pipeline
);
```

---

### zest_UploadLayerStagingData

Upload layer staging data to the GPU. Call in an upload callback before drawing.

```cpp
void zest_UploadLayerStagingData(zest_layer layer, const zest_command_list command_list);
```

---

### zest_UploadInstanceLayerData

Callback for uploading instance layer data. Use with frame graph upload tasks.

> Note that if you have more then one layer to upload then you can call multiple of these functions in your own callback instead rather then create multiple transfer passes.

```cpp
void zest_UploadInstanceLayerData(const zest_command_list command_list, void *user_data);
```

**Example:**
```cpp
zest_BeginTransferPass("Upload Layer");
    zest_SetPassTask(zest_UploadInstanceLayerData, sprite_layer);
zest_EndPass();
```

---

## Layer Properties

### zest_SetLayerViewPort

Set the viewport and scissor for layer drawing.

```cpp
void zest_SetLayerViewPort(
    zest_layer layer,
    int x, int y,
    zest_uint scissor_width, zest_uint scissor_height,
    float viewport_width, float viewport_height
);
```

**Parameters:**
- `layer` - The layer
- `x`, `y` - Scissor offset
- `scissor_width`, `scissor_height` - Scissor dimensions
- `viewport_width`, `viewport_height` - Viewport dimensions

---

### zest_SetLayerScissor

Set only the scissor rectangle for the current draw instruction. Call this after you start an instruction like zest_StartInstanceDrawing.

```cpp
void zest_SetLayerScissor(
    zest_layer layer,
    int offset_x, int offset_y,
    zest_uint scissor_width, zest_uint scissor_height
);
```

---

### zest_SetLayerDrawingViewport

Set viewport/scissor for the current draw instruction. Call this after you start an instruction like zest_StartInstanceDrawing.

```cpp
void zest_SetLayerDrawingViewport(
    zest_layer layer,
    int x, int y,
    zest_uint scissor_width, zest_uint scissor_height,
    float viewport_width, float viewport_height
);
```

---

### zest_SetLayerSizeToSwapchain

Update layer viewport to match the current swapchain size.

```cpp
void zest_SetLayerSizeToSwapchain(zest_layer layer);
```

---

### zest_SetLayerSize

Set the layer size explicitly.

```cpp
void zest_SetLayerSize(zest_layer layer, float width, float height);
```

---

### zest_GetLayerScissor / zest_GetLayerViewport

Get the current scissor/viewport settings.

```cpp
zest_scissor_rect_t zest_GetLayerScissor(zest_layer layer);
zest_viewport_t zest_GetLayerViewport(zest_layer layer);
```

---

### zest_SetLayerColor / zest_SetLayerColorf

Set the layer color. The alpha value controls the blend between additive (0) and alpha blending (1).

> **These functions will be removed**

```cpp
void zest_SetLayerColor(zest_layer layer, zest_byte r, zest_byte g, zest_byte b, zest_byte a);
void zest_SetLayerColorf(zest_layer layer, float r, float g, float b, float a);
```

**Example:**
```cpp
// Full opacity alpha blending
zest_SetLayerColorf(layer, 1.0f, 1.0f, 1.0f, 1.0f);

// Semi-additive blending
zest_SetLayerColorf(layer, 1.0f, 1.0f, 1.0f, 0.5f);
```

---

### zest_SetLayerIntensity

Set the intensity (brightness) of the layer. Values above 1.0 make sprites brighter due to pre-multiplied blending.

> **Will be removed**

```cpp
void zest_SetLayerIntensity(zest_layer layer, float value);
```

---

### zest_SetLayerPushConstants / zest_GetLayerPushConstants

Set or get push constants for the current draw instruction. These will get uploaded to the GPU for each instruction in the layer.

```cpp
void zest_SetLayerPushConstants(zest_layer layer, void *push_constants, zest_size size);
void* zest_GetLayerPushConstants(zest_layer layer);
```

---

### zest_SetLayerUserData / zest_GetLayerUserData

Store custom data with a layer.

```cpp
void zest_SetLayerUserData(zest_layer layer, void *data);
#define zest_GetLayerUserData(type, layer) ((type *)layer->user_data)
```

---

## Layer Reset

### zest_ResetLayer

Set the layer frame in flight to the next layer. Use this if you're manually setting the current fif for the layer so that you can avoid uploading the staging buffers every frame and only do so when it's neccessary.

```cpp
void zest_ResetLayer(zest_layer layer);
```

---

### zest_ResetInstanceLayer

Same as ResetLayer but specifically for an instance layer

```cpp
void zest_ResetInstanceLayer(zest_layer layer);
```

---

### zest_ResetInstanceLayerDrawing

Reset the drawing for an instance layer. This is called after all drawing is done and dispatched to the gpu.

```cpp
void zest_ResetInstanceLayerDrawing(zest_layer layer);
```

---

## Draw Instructions

### zest_NextLayerInstruction

Get the next draw instruction slot.

```cpp
zest_layer_instruction_t* zest_NextLayerInstruction(zest_layer layer);
```
**Example**
```cpp
zest_layer_instruction_t *current = zest_NextLayerInstruction(layer);
while(current) {
	...
	zest_cmd_DrawLayerInstruction(command_list, 6, current);
	current = zest_NextLayerInstruction(layer);
}
```

---

### zest_GetLayerInstruction / zest_GetLayerInstructionCount

Access recorded draw instructions.

```cpp
const zest_layer_instruction_t* zest_GetLayerInstruction(zest_layer layer, zest_uint index);
zest_uint zest_GetLayerInstructionCount(zest_layer layer);
```

**Example**
```cpp
zest_uint count = zest_GetLayerInstructionCount(layer);
for(zest_uint i = 0; i != count; i++) {
	const zest_layer_instruction_t* zest_GetLayerInstruction(layer, i);
	...
	zest_cmd_DrawLayerInstruction(command_list, 6, current);
}
```

---

## Buffer Access

### zest_GetLayerVertexBuffer / zest_GetLayerResourceBuffer

Get device-side buffers.

```cpp
zest_buffer zest_GetLayerVertexBuffer(zest_layer layer);
zest_buffer zest_GetLayerResourceBuffer(zest_layer layer);
```

---

### zest_GetLayerStagingVertexBuffer / zest_GetLayerStagingIndexBuffer

Get staging buffers for CPU-side access.

```cpp
zest_buffer zest_GetLayerStagingVertexBuffer(zest_layer layer);
zest_buffer zest_GetLayerStagingIndexBuffer(zest_layer layer);
```

---

### zest_GetLayerVertexDescriptorIndex

Get the bindless descriptor index for the layer's vertex buffer.

```cpp
zest_uint zest_GetLayerVertexDescriptorIndex(zest_layer layer, zest_bool last_frame);
```

---

### zest_GetLayerFrameInFlight

Get the current frame-in-flight index for the layer.

```cpp
int zest_GetLayerFrameInFlight(zest_layer layer);
```

---

## Frame Graph Integration

### zest_AddTransientLayerResource

Add a layer's buffer as a transient resource in the frame graph. This is an essential function when using layers to draw instanced data.

```cpp
zest_resource_node zest_AddTransientLayerResource(
    const char *name,
    const zest_layer layer,
    zest_bool prev_fif
);
```

**Example**

```cpp
zest_resource_node billboard_layer_resource = zest_AddTransientLayerResource("Billboards", billboard_layer, false);
...
zest_BeginRenderPass("Particles Pass");
zest_ConnectInput(billboard_layer_resource);
zest_SetPassTask(zest_DrawInstanceLayer, this);
zest_ConnectSwapChainOutput();
zest_EndPass();
```

---

## Complete Example

```cpp
// Create a sprite layer
zest_layer_handle sprite_handle = zest_CreateInstanceLayer(context, "sprites", sizeof(sprite_t));

// In your update function:
void update_sprites(zest_layer layer, sprite_data_t *sprites, int count) {
    zest_StartInstanceDrawing(layer, sprite_pipeline);

    for (int i = 0; i < count; i++) {
        sprite_t *inst = (sprite_t*)zest_NextInstance(layer);
        inst->position = sprites[i].position;
        inst->uv = sprites[i].uv;
        inst->color = sprites[i].color;
    }

    zest_EndInstanceInstructions(layer);  
}

// In your frame graph setup:
zest_frame_graph setup_frame_graph() {
	zest_layer sprite_layer = zest_GetLayer(sprite_layer_handle);
    zest_BeginFrameGraph();
	zest_resource_node sprite_layer_resource = zest_AddTransientLayerResource("Sprites", sprite_layer, false);

    zest_BeginRenderPass("main");
		zest_ConnectInput(sprite_layer_resource);
        zest_ConnectSwapChainOutput();
        zest_SetPassTask(zest_DrawInstanceLayer, sprite_layer);
    zest_EndPass();

    return zest_EndFrameGraph();
}

```

---

## See Also

- [Layers Concept](../concepts/layers.md) - Conceptual overview
- [Instancing Tutorial](../tutorials/03-instancing.md) - Step-by-step guide
- [Pipeline API](pipeline.md) - Pipeline template configuration
