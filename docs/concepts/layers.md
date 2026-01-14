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

// Get instance data pointer
sprite_instance_t* instances = (sprite_instance_t*)zest_GetLayerInstancesData(layer);

// Add instances
for (int i = 0; i < sprite_count; i++) {
    zest_uint index = zest_NextInstance(layer);
    instances[index].position = sprites[i].pos;
    instances[index].scale = sprites[i].size;
    instances[index].rotation = sprites[i].angle;
    instances[index].texture_index = sprites[i].tex;
    instances[index].color = sprites[i].color;
}

// End recording
zest_EndInstanceInstructions(layer);
```

### Drawing Instances

```cpp
void RenderCallback(zest_command_list cmd, void* data) {
    app_t* app = (app_t*)data;
    zest_layer layer = zest_GetLayer(app->sprite_layer);

    // Get instance count
    zest_uint count = zest_GetInstanceLayerCount(layer);
    if (count == 0) return;

    // Bind pipeline
    zest_pipeline pipeline = zest_PipelineWithTemplate(app->sprite_pipeline, cmd);
    zest_cmd_BindPipeline(cmd, pipeline);

    // Draw instanced
    zest_DrawInstanceLayer(cmd, layer);
}
```

## Mesh Layers

For dynamic geometry with arbitrary vertex/index data (UI, procedural meshes).

### Creating a Mesh Layer

```cpp
zest_layer_handle layer = zest_CreateMeshLayer(
    context,
    "ui_mesh",
    sizeof(ui_vertex_t),  // Vertex size
    MAX_VERTICES,
    MAX_INDICES
);
```

### Adding Mesh Data

```cpp
zest_layer layer = zest_GetLayer(layer_handle);

// Get write buffers
ui_vertex_t* vertices = (ui_vertex_t*)zest_GetVertexWriteBuffer(layer);
zest_uint* indices = (zest_uint*)zest_GetIndexWriteBuffer(layer);

// Write vertex data
vertices[0] = {.pos = {0, 0}, .uv = {0, 0}, .color = WHITE};
vertices[1] = {.pos = {100, 0}, .uv = {1, 0}, .color = WHITE};
// ...

// Write index data
indices[0] = 0; indices[1] = 1; indices[2] = 2;
// ...

// Set mesh drawing mode
zest_SetMeshDrawing(layer, vertex_count, index_count, instance_count);
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
// Create a mesh
zest_mesh_handle mesh_handle = zest_NewMesh();
zest_mesh mesh = zest_GetMesh(mesh_handle);

// Reserve space
zest_ReserveMeshVertices(mesh, vertex_count);

// Add vertices
for (int i = 0; i < vertex_count; i++) {
    vertex_t* v = (vertex_t*)zest_NextMeshVertex(mesh);
    v->position = positions[i];
    v->normal = normals[i];
    v->uv = uvs[i];
}

// Copy index data
zest_CopyMeshIndexData(mesh, indices, index_count);

// Add to layer
zest_uint mesh_id = zest_AddMeshToLayer(layer, mesh, 0);

// Free mesh handle (data is now in layer)
zest_FreeMesh(mesh_handle);
```

### Drawing Instance Mesh Layer

```cpp
void RenderCallback(zest_command_list cmd, void* data) {
    app_t* app = (app_t*)data;
    zest_layer layer = zest_GetLayer(app->mesh_layer);

    // Bind mesh vertex/index buffers
    zest_cmd_BindMeshVertexBuffer(cmd, layer);
    zest_cmd_BindMeshIndexBuffer(cmd, layer);

    // Bind pipeline
    zest_pipeline pipeline = zest_PipelineWithTemplate(app->mesh_pipeline, cmd);
    zest_cmd_BindPipeline(cmd, pipeline);

    // Draw each mesh type with its instances
    for (int mesh_id = 0; mesh_id < mesh_count; mesh_id++) {
        const zest_mesh_offset_data_t* offsets = zest_GetLayerMeshOffsets(layer, mesh_id);

        // Bind instance buffer at correct offset
        zest_cmd_BindVertexBuffer(cmd, 1, 1, instance_buffer, offsets->instance_offset);

        // Draw indexed instances
        zest_cmd_DrawIndexed(
            cmd,
            offsets->index_count,
            instance_counts[mesh_id],
            offsets->index_offset,
            offsets->vertex_offset,
            0
        );
    }
}
```

## Layer Properties

### Viewport and Scissor

```cpp
zest_SetLayerViewPort(layer, viewport);
zest_SetLayerScissor(layer, scissor);
zest_SetLayerSizeToSwapchain(layer);  // Match swapchain size
zest_SetLayerSize(layer, width, height);
```

### Colors

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
void* data = zest_GetLayerUserData(layer);
```

### Change Tracking

```cpp
// Mark layer as changed (triggers re-upload)
zest_SetLayerChanged(layer);

// Check if layer has changed
if (zest_LayerHasChanged(layer)) {
    // Handle change
}
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
// In transfer pass
zest_BeginTransferPass("Upload Layers"); {
    zest_ConnectOutput(layer_resources);
    zest_SetPassTask(zest_UploadInstanceLayerData, layer);  // Built-in helper
    zest_EndPass();
}
```

Or manually:

```cpp
void UploadCallback(zest_command_list cmd, void* data) {
    zest_layer layer = (zest_layer)data;
    zest_UploadLayerStagingData(layer, cmd);
}
```

## Resetting Layers

Clear instance data for next frame:

```cpp
zest_ResetLayer(layer_handle);
zest_ResetInstanceLayer(layer);
```

## Best Practices

1. **Choose the right layer type** - Instance for sprites, Mesh for UI, Instance Mesh for 3D
2. **Batch by texture** - Minimize texture switches within a layer
3. **Reset between frames** - Call reset before adding new instances
4. **Upload before rendering** - Use transfer pass to upload layer data
5. **Size appropriately** - Set max instances/vertices based on expected usage

## See Also

- [Instancing Tutorial](../tutorials/03-instancing.md) - Complete instancing example
- [Layer API](../api-reference/layer.md) - Complete function reference
- [Pipelines](pipelines.md) - Pipeline configuration for layers
