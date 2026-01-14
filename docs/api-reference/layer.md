# Layer API

Functions for layer creation and management (~60 functions).

## Layer Creation

### zest_CreateInstanceLayer

Create instance layer for sprites/billboards.

```cpp
zest_layer_handle zest_CreateInstanceLayer(
    zest_context context,
    const char *name,
    zest_size instance_size,
    zest_uint max_instances
);
```

---

### zest_CreateMeshLayer

Create mesh layer for dynamic geometry.

```cpp
zest_layer_handle zest_CreateMeshLayer(
    zest_context context,
    const char *name,
    zest_size vertex_size,
    zest_uint max_vertices,
    zest_uint max_indices
);
```

---

### zest_CreateInstanceMeshLayer

Create instance mesh layer for 3D instancing.

```cpp
zest_layer_handle zest_CreateInstanceMeshLayer(
    zest_context context,
    const char *name,
    zest_size instance_size,
    zest_uint max_vertices,
    zest_uint max_indices
);
```

---

### zest_GetLayer

Get layer object from handle.

```cpp
zest_layer zest_GetLayer(zest_layer_handle handle);
```

---

## Instance Management

### zest_StartInstanceInstructions

Begin recording instances.

```cpp
void zest_StartInstanceInstructions(zest_layer layer);
```

---

### zest_EndInstanceInstructions

End recording instances.

```cpp
void zest_EndInstanceInstructions(zest_layer layer);
```

---

### zest_NextInstance

Get next instance index.

```cpp
zest_uint zest_NextInstance(zest_layer layer);
```

---

### zest_GetLayerInstancesData

Get instance data pointer.

```cpp
void* zest_GetLayerInstancesData(zest_layer layer);
```

---

### zest_GetInstanceLayerCount

```cpp
zest_uint zest_GetInstanceLayerCount(zest_layer layer);
```

---

## Mesh Management

### zest_AddMeshToLayer

Add mesh to instance mesh layer.

```cpp
zest_uint zest_AddMeshToLayer(zest_layer layer, void *mesh_data, zest_uint texture_index);
```

---

### zest_GetLayerMeshOffsets

Get mesh offset data for drawing.

```cpp
const zest_mesh_offset_data_t* zest_GetLayerMeshOffsets(zest_layer layer, zest_uint mesh_id);
```

---

### zest_GetVertexWriteBuffer / zest_GetIndexWriteBuffer

```cpp
void* zest_GetVertexWriteBuffer(zest_layer layer);
void* zest_GetIndexWriteBuffer(zest_layer layer);
```

---

## Drawing

### zest_DrawInstanceLayer

Draw all instances in layer.

```cpp
void zest_DrawInstanceLayer(zest_command_list cmd, zest_layer layer);
```

---

### zest_DrawInstanceMeshLayer

Draw instance mesh layer.

```cpp
void zest_DrawInstanceMeshLayer(zest_command_list cmd, zest_layer layer);
```

---

## Layer Properties

### zest_SetLayerViewPort / zest_SetLayerScissor

```cpp
void zest_SetLayerViewPort(zest_layer layer, VkViewport viewport);
void zest_SetLayerScissor(zest_layer layer, VkRect2D scissor);
```

---

### zest_SetLayerColor / zest_SetLayerColorf

```cpp
void zest_SetLayerColor(zest_layer layer, zest_byte r, zest_byte g, zest_byte b, zest_byte a);
void zest_SetLayerColorf(zest_layer layer, float r, float g, float b, float a);
```

---

### zest_SetLayerPushConstants

```cpp
void zest_SetLayerPushConstants(zest_layer layer, void *data, zest_size size);
```

---

### zest_ResetLayer

Clear layer for next frame.

```cpp
void zest_ResetLayer(zest_layer_handle handle);
```

---

## Data Upload

### zest_UploadLayerStagingData

Upload layer data to GPU.

```cpp
void zest_UploadLayerStagingData(zest_layer layer, zest_command_list cmd);
```

---

## See Also

- [Layers Concept](../concepts/layers.md)
- [Instancing Tutorial](../tutorials/03-instancing.md)
