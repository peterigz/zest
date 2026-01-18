# Tutorial 3: GPU 3D Instancing

Learn to render thousands of objects efficiently using Zest's layer system.

**Example:** `examples/GLFW/zest-instancing`

## What You'll Learn

- Instance mesh layers
- Loading 3D models (GLTF)
- Per-instance data
- Efficient batched drawing

## Overview

GPU instancing renders multiple copies of geometry in a single draw call. Each instance can have unique properties (position, rotation, color, texture).

```
Traditional: 1000 objects = 1000 draw calls
Instancing:  1000 objects = 1 draw call
```

## Instance Data Structure

```cpp
struct mesh_instance_t {
    zest_vec3 pos;
    zest_vec3 rotation;
    zest_vec3 scale;
    zest_uint texture_layer_index;
};
```

## Creating an Instance Mesh Layer

```cpp
zest_layer_handle layer = zest_CreateInstanceMeshLayer(
    context,
    "3d_objects",
    sizeof(mesh_instance_t),
    MAX_VERTICES,
    MAX_INDICES
);
```

## Loading Meshes

```cpp
// Load GLTF using the loader in zest_utilities.h (requires ZEST_ALL_UTILITIES_IMPLEMENTATION)
zest_mesh mesh = LoadGLTFScene(context, "model.gltf", 1.0f);

// Add to layer
zest_layer layer_obj = zest_GetLayer(layer);
zest_uint mesh_id = zest_AddMeshToLayer(layer_obj, mesh, 0);

// Free mesh CPU data after adding to layer
zest_FreeMesh(mesh);
```

## Preparing Instance Data

Instance data is uploaded to a GPU buffer separately from the mesh layer:

```cpp
// Allocate and fill instance data on CPU
zest_size instance_data_size = INSTANCE_COUNT * sizeof(mesh_instance_t);
mesh_instance_t* instance_data = (mesh_instance_t*)malloc(instance_data_size);

for (int i = 0; i < INSTANCE_COUNT; i++) {
    instance_data[i].pos = objects[i].pos;
    instance_data[i].rotation = objects[i].rot;
    instance_data[i].scale = {1, 1, 1};
    instance_data[i].texture_layer_index = objects[i].tex_id;
}

// Create GPU buffer for instances
zest_buffer_info_t buffer_info = zest_CreateBufferInfo(zest_buffer_type_vertex, zest_memory_usage_gpu_only);
zest_buffer instance_buffer = zest_CreateBuffer(device, instance_data_size, &buffer_info);

// Upload via staging buffer
zest_buffer staging = zest_CreateStagingBuffer(device, instance_data_size, instance_data);
zest_queue queue = zest_imm_BeginCommandBuffer(device, zest_queue_transfer);
zest_imm_CopyBuffer(queue, staging, instance_buffer, instance_data_size);
zest_imm_EndCommandBuffer(queue);

// Cleanup
zest_FreeBuffer(staging);
free(instance_data);
```

## Pipeline Setup

```cpp
// Vertex input for per-vertex data (binding 0) using zest_vertex_t
zest_AddVertexInputBindingDescription(pipeline, 0, sizeof(zest_vertex_t), zest_input_rate_vertex);
zest_AddVertexAttribute(pipeline, 0, 0, zest_format_r32g32b32_sfloat, 0);                                   // Position
zest_AddVertexAttribute(pipeline, 0, 1, zest_format_r8g8b8a8_unorm, offsetof(zest_vertex_t, color));        // Color
zest_AddVertexAttribute(pipeline, 0, 2, zest_format_r32g32b32_sfloat, offsetof(zest_vertex_t, normal));     // Normal
zest_AddVertexAttribute(pipeline, 0, 3, zest_format_r32g32_sfloat, offsetof(zest_vertex_t, uv));            // UV

// Instance data (binding 1)
zest_AddVertexInputBindingDescription(pipeline, 1, sizeof(mesh_instance_t), zest_input_rate_instance);
zest_AddVertexAttribute(pipeline, 1, 4, zest_format_r32g32b32_sfloat, 0);                                   // Position
zest_AddVertexAttribute(pipeline, 1, 5, zest_format_r32g32b32_sfloat, offsetof(mesh_instance_t, rotation)); // Rotation
zest_AddVertexAttribute(pipeline, 1, 6, zest_format_r32g32b32_sfloat, offsetof(mesh_instance_t, scale));    // Scale
zest_AddVertexAttribute(pipeline, 1, 7, zest_format_r32_uint, offsetof(mesh_instance_t, texture_layer_index)); // Texture
```

## Drawing

```cpp
void RenderCallback(zest_command_list command_list, void* user_data) {
    app_t* app = (app_t*)user_data;
    zest_layer layer = zest_GetLayer(app->instance_layer);

    // Bind mesh vertex and index buffers from layer
    zest_cmd_BindMeshVertexBuffer(command_list, layer);
    zest_cmd_BindMeshIndexBuffer(command_list, layer);

    // Bind instance buffer to binding slot 1
    zest_cmd_BindVertexBuffer(command_list, 1, 1, app->instance_buffer);

    // Bind pipeline
    zest_pipeline pipeline = zest_GetPipeline(app->pipeline_template, command_list);
    if (pipeline) {
        zest_cmd_BindPipeline(command_list, pipeline);

        // Get mesh offsets for indexed drawing
        const zest_mesh_offset_data_t* offsets = zest_GetLayerMeshOffsets(layer, app->mesh_id);

        // Set viewport
        zest_cmd_SetScreenSizedViewport(command_list, 0.f, 1.f);

        // Draw all instances
        zest_cmd_DrawIndexed(command_list,
            offsets->index_count,
            INSTANCE_COUNT,
            offsets->index_offset,
            offsets->vertex_offset,
            0
        );
    }
}
```

## Full Example

See `examples/GLFW/zest-instancing/zest-instancing.cpp` for the complete implementation.

## Next Steps

- [Tutorial 4: Loading Textures](04-textures.md)
- [Layers Concept](../concepts/layers.md) - Deep dive on layer types
