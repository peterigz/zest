# Command API

Frame graph render commands (`zest_cmd_*` functions).

## Pipeline Binding

### zest_cmd_BindPipeline

Bind graphics pipeline.

```cpp
void zest_cmd_BindPipeline(zest_command_list cmd, zest_pipeline pipeline);
```

---

### zest_cmd_BindComputePipeline

Bind compute pipeline.

```cpp
void zest_cmd_BindComputePipeline(zest_command_list cmd, zest_compute compute);
```

---

## Viewport and Scissor

### zest_cmd_SetScreenSizedViewport

Set viewport to swapchain size.

```cpp
void zest_cmd_SetScreenSizedViewport(zest_command_list cmd);
```

---

### zest_cmd_ViewPort

Set custom viewport.

```cpp
void zest_cmd_ViewPort(zest_command_list cmd, VkViewport viewport);
```

---

### zest_cmd_Scissor

Set scissor rectangle.

```cpp
void zest_cmd_Scissor(zest_command_list cmd, VkRect2D scissor);
```

---

## Buffer Binding

### zest_cmd_BindVertexBuffer

```cpp
void zest_cmd_BindVertexBuffer(
    zest_command_list cmd,
    zest_buffer buffer,
    zest_size offset
);
```

---

### zest_cmd_BindIndexBuffer

```cpp
void zest_cmd_BindIndexBuffer(
    zest_command_list cmd,
    zest_buffer buffer,
    zest_size offset,
    zest_index_type type
);
```

Types: `zest_index_type_uint16`, `zest_index_type_uint32`

---

## Drawing

### zest_cmd_Draw

Non-indexed draw.

```cpp
void zest_cmd_Draw(
    zest_command_list cmd,
    zest_uint vertex_count,
    zest_uint instance_count,
    zest_uint first_vertex,
    zest_uint first_instance
);
```

---

### zest_cmd_DrawIndexed

Indexed draw.

```cpp
void zest_cmd_DrawIndexed(
    zest_command_list cmd,
    zest_uint index_count,
    zest_uint instance_count,
    zest_uint first_index,
    int vertex_offset,
    zest_uint first_instance
);
```

---

## Push Constants

### zest_cmd_SendPushConstants

```cpp
void zest_cmd_SendPushConstants(
    zest_command_list cmd,
    void *data,
    zest_size size
);
```

---

## Compute

### zest_cmd_DispatchCompute

```cpp
void zest_cmd_DispatchCompute(
    zest_command_list cmd,
    zest_uint groups_x,
    zest_uint groups_y,
    zest_uint groups_z
);
```

---

## Buffer Operations

### zest_cmd_CopyBuffer

```cpp
void zest_cmd_CopyBuffer(
    zest_command_list cmd,
    zest_buffer src,
    zest_buffer dst,
    zest_size size
);
```

---

### zest_cmd_UploadBuffer

```cpp
void zest_cmd_UploadBuffer(
    zest_command_list cmd,
    zest_buffer staging,
    zest_buffer dst,
    zest_size size
);
```

---

## Image Operations

### zest_cmd_ImageClear

```cpp
void zest_cmd_ImageClear(
    zest_command_list cmd,
    zest_image image,
    float r, float g, float b, float a
);
```

---

### zest_cmd_BlitImageMip

```cpp
void zest_cmd_BlitImageMip(
    zest_command_list cmd,
    zest_image src,
    zest_image dst,
    zest_uint src_mip,
    zest_uint dst_mip,
    zest_filter filter
);
```

---

## See Also

- [Immediate API](immediate.md) - One-off commands
- [Frame Graph Concept](../concepts/frame-graph.md)
