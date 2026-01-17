# Layers

Layers are Zest's high-level rendering abstraction for batched and instanced drawing. They handle buffer management, instance data, and draw call organization.

## Layer Types

| Layer Type | Use Case | Key Feature |
|------------|----------|-------------|
| Instance Layer | 2D sprites, billboards | Per-instance data array |
| Mesh Layer | ImGui, custom meshes | Dynamic vertex/index buffers |
| Instance Mesh Layer | 3D models, instancing | Meshes + per-instance data |
| Font Layer | Text rendering | MSDF font integration |

## Instance Layers

For rendering many similar objects with per-instance data (sprites, particles, billboards).

### Creating an Instance Layer

```cpp
zest_layer_handle layer = zest_CreateInstanceLayer(
    context,
    "sprites",
    sizeof(sprite_instance_t),  // Per-instance data size
    MAX_INSTANCES
);
```

### Instance Data Structure

```cpp
struct sprite_instance_t {
    zest_vec3 position;
    zest_vec2 scale;
    float rotation;
    zest_uint texture_index;
    zest_color_t color;
};
```

### Adding Instances

```cpp
zest_layer layer = zest_GetLayer(layer_handle);

// Start recording instances
zest_StartInstanceInstructions(layer);

// Add instances - zest_NextInstance returns pointer to next slot
for (int i = 0; i < sprite_count; i++) {
    sprite_instance_t* inst = (sprite_instance_t*)zest_NextInstance(layer);
    inst->position = sprites[i].pos;
    inst->scale = sprites[i].size;
    inst->rotation = sprites[i].angle;
    inst->texture_index = sprites[i].tex;
    inst->color = sprites[i].color;
}

// End recording (optional: is also ended in the next call to zest_StartInstanceInstructions
zest_EndInstanceInstructions(layer);
```

### Drawing Instances

Use `zest_DrawInstanceLayer` as a pass task callback:

```cpp
// Set up the render pass with the built-in draw function
zest_BeginRenderPass("Draw Sprites"); {
    zest_ConnectSwapChainOutput();
    zest_SetPassTask(zest_DrawInstanceLayer, layer);  // Pass layer as user_data
    zest_EndPass();
}
```

Or implement a custom draw callback:

```cpp
void RenderCallback(zest_command_list cmd, void* data) {
    zest_layer layer = (zest_layer)data;

    // Get instance count
    zest_uint count = zest_GetInstanceLayerCount(layer);
    if (count == 0) return;

    // Bind pipeline and draw
    zest_pipeline pipeline = zest_GetPipeline(sprite_pipeline, cmd);
    zest_cmd_BindPipeline(cmd, pipeline);

    // Custom drawing logic here...
}
```

## Mesh Layers

For dynamic geometry with arbitrary vertex/index data (UI, procedural meshes).

### Creating a Mesh Layer

```cpp
zest_layer_handle layer = zest_CreateMeshLayer(
    context,
    "ui_mesh",
    sizeof(ui_vertex_t)   // Vertex size
);
```

### Adding Mesh Data

```cpp
zest_layer layer = zest_GetLayer(layer_handle);

// Get write buffers (returns zest_buffer, use zest_BufferData to get pointer)
zest_buffer vertex_buffer = zest_GetVertexWriteBuffer(layer);
zest_buffer index_buffer = zest_GetIndexWriteBuffer(layer);

ui_vertex_t* vertices = (ui_vertex_t*)zest_BufferData(vertex_buffer);
zest_uint* indices = (zest_uint*)zest_BufferData(index_buffer);

// Write vertex data
vertices[0] = (ui_vertex_t){.pos = {0, 0}, .uv = {0, 0}, .color = WHITE};
vertices[1] = (ui_vertex_t){.pos = {100, 0}, .uv = {1, 0}, .color = WHITE};
// ...

// Write index data
indices[0] = 0; indices[1] = 1; indices[2] = 2;
// ...

// Set mesh drawing mode with pipeline
zest_SetMeshDrawing(layer, mesh_pipeline);
```

## Instance Mesh Layers

Combines pre-loaded meshes with per-instance data. Ideal for 3D instanced rendering.

### Creating an Instance Mesh Layer

```cpp
zest_layer_handle layer = zest_CreateInstanceMeshLayer(
    context,
    "3d_objects",
    sizeof(mesh_instance_t),  // Per-instance data size
    MAX_VERTICES,             // Total vertex capacity
    MAX_INDICES               // Total index capacity
);
```

### Loading Meshes

```cpp
zest_layer layer = zest_GetLayer(layer_handle);

// Load mesh data (from file, GLTF, etc.)
mesh_data_t mesh = load_mesh("model.gltf");

// Add mesh to layer
zest_uint mesh_index = zest_AddMeshToLayer(layer, &mesh, texture_index);
```

### Using Zest Mesh API

```cpp
// Create a mesh (returns zest_mesh directly, not a handle)
zest_mesh mesh = zest_NewMesh(context, sizeof(vertex_t));

// Reserve space
zest_ReserveMeshVertices(mesh, vertex_count);

// Add vertices - zest_NextMeshVertex takes current pointer and returns the next
void* current = NULL;
for (int i = 0; i < vertex_count; i++) {
    vertex_t* v = (vertex_t*)zest_NextMeshVertex(mesh, current);
    v->position = positions[i];
    v->normal = normals[i];
    v->uv = uvs[i];
    current = v;
}

// Copy index data
zest_CopyMeshIndexData(mesh, indices, index_count);

// Add to layer
zest_uint mesh_id = zest_AddMeshToLayer(layer, mesh, texture_index);

// Free mesh (data is now copied to layer)
zest_FreeMesh(mesh);
```

### Drawing Instance Mesh Layer

```cpp
void RenderCallback(zest_command_list cmd, void* data) {
    app_t* app = (app_t*)data;
    zest_layer layer = zest_GetLayer(app->mesh_layer);

    // Bind mesh vertex/index buffers

	zest_cmd_BindMeshVertexBuffer(command_list, layer);
	zest_cmd_BindMeshIndexBuffer(command_list, layer);

	zest_buffer device_buffer = zest_GetLayerResourceBuffer(layer);
	zest_cmd_BindVertexBuffer(command_list, 1, 1, device_buffer);

	zest_pipeline pipeline = zest_GetPipeline(app->mesh_pipeline, command_list);

	zest_uint instruction_count = zest_GetLayerInstructionCount(layer);
	for(int i = 0; i != instruction_count; i++) {
        const zest_layer_instruction_t *current = zest_GetLayerInstruction(layer, i);
		const zest_mesh_offset_data_t *mesh_offsets = zest_GetLayerMeshOffsets(layer, current->mesh_index);

		zest_cmd_LayerViewport(command_list, layer);
		zest_cmd_BindPipeline(command_list, pipeline);

		scene_push_constants_t *push = (scene_push_constants_t*)current->push_constant;
		push->shadow_index = shadow_index;
		zest_cmd_SendPushConstants(command_list, (void*)push, sizeof(scene_push_constants_t));

		zest_cmd_DrawIndexed(command_list, mesh_offsets->index_count, current->total_instances, mesh_offsets->index_offset, mesh_offsets->vertex_offset, current->start_index);
    }
}
```

## Layer Properties

### Viewport and Scissor

```cpp
// Set viewport with full control
zest_SetLayerViewPort(
    layer,
    0, 0,                    // x, y offset
    scissor_width, scissor_height,
    viewport_width, viewport_height
);

// Set scissor rect
zest_SetLayerScissor(layer, offset_x, offset_y, width, height);

// Match swapchain size (convenience)
zest_SetLayerSizeToSwapchain(layer);

// Set specific size
zest_SetLayerSize(layer, width, height);
```

### Colors

These properties are a way that you can store a color in a layer and use it to tint or color instances.

```cpp
// Set RGBA (0-255)
zest_SetLayerColor(layer, 255, 255, 255, 255);

// Set RGBA (0.0-1.0)
zest_SetLayerColorf(layer, 1.0f, 1.0f, 1.0f, 1.0f);

// Intensity multiplier
zest_SetLayerIntensity(layer, 1.5f);
```

### User Data

```cpp
zest_SetLayerUserData(layer, my_data);
AppData *data = (AppData*)zest_GetLayerUserData(layer);
```

## Push Constants

Pass data to shaders via push constants:

```cpp
// Define push constant struct
struct push_constants_t {
    zest_matrix4 view_projection;
    zest_uint texture_index;
};

// Set push constants for layer
push_constants_t push = {...};
zest_SetLayerPushConstants(layer, &push, sizeof(push));

// In render callback
void RenderCallback(zest_command_list cmd, void* data) {
    zest_layer layer = zest_GetLayer(layer_handle);
    push_constants_t* push = (push_constants_t*)zest_GetLayerPushConstants(layer);

    // Modify if needed
    push->view_projection = camera.vp;

    // Send to shader
    zest_cmd_SendPushConstants(cmd, push, sizeof(*push));
}
```

## Uploading Layer Data

Layer data must be uploaded to GPU before rendering:

```cpp
// In transfer pass - use built-in upload function as callback
zest_BeginTransferPass("Upload Layers"); {
    zest_SetPassTask(zest_UploadInstanceLayerData, layer);  // Pass layer as user_data
    zest_EndPass();
}
```

Or implement a custom upload callback:

```cpp
void UploadCallback(zest_command_list cmd, void* data) {
    zest_layer layer = (zest_layer)data;
    zest_UploadLayerStagingData(layer, cmd);
}
```

## Resetting Layers

Clear instance data for next frame:

```cpp
zest_layer layer = zest_GetLayer(layer_handle);

// Reset layer to the next frame in flight
zest_ResetLayer(layer);

// Same as ResetLayer but specifically for an instance layer with manual frame in flight control
zest_ResetInstanceLayer(layer);

// Reset instance drawing state only. This is called automatically when you call 
// zest_StartInstanceDrawing or zest_StartInstanceMeshDrawing in a new frame
zest_ResetInstanceLayerDrawing(layer);
```

## Best Practices

1. **Choose the right layer type** - Instance for sprites, Mesh for UI or static meshes, Instance Mesh for 3D
2. **Batch by texture** - Minimize texture switches within a layer
4. **Upload before rendering** - Use transfer pass to upload layer data
5. **Size appropriately** - Set max instances/vertices based on expected usage

## See Also

- [Instancing Tutorial](../tutorials/03-instancing.md) - Complete instancing example
- [Layer API](../api-reference/layer.md) - Complete function reference
- [Pipelines](pipelines.md) - Pipeline configuration for layers
