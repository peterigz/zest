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
    zest_vec3 position;
    zest_vec3 rotation;
    zest_vec3 scale;
    zest_uint texture_index;
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
// Load GLTF (either by using your own loader or making use of the example loader in zest_utilities.h)
zest_mesh mesh = zest_LoadGLTF("model.gltf");

// Add to layer
zest_layer layer_obj = zest_GetLayer(layer);
zest_uint mesh_id = zest_AddMeshToLayer(layer_obj, &mesh, 0);
```

## Adding Instances

```cpp
zest_layer layer_obj = zest_GetLayer(layer);
mesh_instance_t* instances = (mesh_instance_t*)zest_GetLayerInstancesData(layer_obj);

for (int i = 0; i < object_count; i++) {
    instances[i].position = objects[i].pos;
    instances[i].rotation = objects[i].rot;
    instances[i].scale = {1, 1, 1};
    instances[i].texture_index = objects[i].tex_id;
}
```

## Pipeline Setup

```cpp
// Vertex input for per-vertex data (binding 0)
zest_AddVertexInputBindingDescription(pipeline, 0, sizeof(vertex_t), zest_input_rate_vertex);
zest_AddVertexAttribute(pipeline, 0, 0, zest_format_r32g32b32_sfloat, offsetof(vertex_t, position));
zest_AddVertexAttribute(pipeline, 0, 1, zest_format_r32g32b32_sfloat, offsetof(vertex_t, normal));
zest_AddVertexAttribute(pipeline, 0, 2, zest_format_r32g32_sfloat, offsetof(vertex_t, uv));

// Instance data (binding 1)
zest_AddVertexInputBindingDescription(pipeline, 1, sizeof(mesh_instance_t), zest_input_rate_instance);
zest_AddVertexAttribute(pipeline, 1, 3, zest_format_r32g32b32_sfloat, offsetof(mesh_instance_t, position));
zest_AddVertexAttribute(pipeline, 1, 4, zest_format_r32g32b32_sfloat, offsetof(mesh_instance_t, rotation));
zest_AddVertexAttribute(pipeline, 1, 5, zest_format_r32g32b32_sfloat, offsetof(mesh_instance_t, scale));
zest_AddVertexAttribute(pipeline, 1, 6, zest_format_r32_uint, offsetof(mesh_instance_t, texture_index));
```

## Drawing

```cpp
void RenderCallback(zest_command_list cmd, void* data) {
    app_t* app = (app_t*)data;
    zest_layer layer = zest_GetLayer(app->instance_layer);

    // Bind mesh data
    zest_cmd_BindMeshVertexBuffer(cmd, layer);
    zest_cmd_BindMeshIndexBuffer(cmd, layer);

    // Bind pipeline
    zest_pipeline pipeline = zest_PipelineWithTemplate(app->pipeline, cmd);
    zest_cmd_BindPipeline(cmd, pipeline);

    // Get mesh offsets
    const zest_mesh_offset_data_t* offsets = zest_GetLayerMeshOffsets(layer, mesh_id);

    // Bind instance buffer
    zest_cmd_BindVertexBuffer(cmd, 1, 1, instance_buffer, 0);

    // Draw all instances
    zest_cmd_DrawIndexed(cmd,
        offsets->index_count,
        instance_count,
        offsets->index_offset,
        offsets->vertex_offset,
        0
    );
}
```

## Full Example

See `examples/GLFW/zest-instancing/zest-instancing.cpp` for the complete implementation.

## Next Steps

- [Tutorial 4: Loading Textures](04-textures.md)
- [Layers Concept](../concepts/layers.md) - Deep dive on layer types
