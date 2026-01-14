# Pipeline API

Functions for creating and configuring pipeline templates.

## Template Creation

### zest_BeginPipelineTemplate

Start creating a pipeline template.

```cpp
zest_pipeline_template_handle zest_BeginPipelineTemplate(zest_device device, const char *name);
```

---

### zest_CopyPipelineTemplate

Copy an existing template.

```cpp
zest_pipeline_template_handle zest_CopyPipelineTemplate(
    const char *new_name,
    zest_pipeline_template_handle source
);
```

---

### zest_FreePipelineTemplate

Free a pipeline template.

```cpp
void zest_FreePipelineTemplate(zest_pipeline_template_handle handle);
```

---

## Shaders

### zest_SetPipelineShaders

Set vertex and fragment shaders.

```cpp
void zest_SetPipelineShaders(
    zest_pipeline_template_handle pipeline,
    zest_shader_handle vertex,
    zest_shader_handle fragment
);
```

---

### zest_SetPipelineVertShader / zest_SetPipelineFragShader

```cpp
void zest_SetPipelineVertShader(zest_pipeline_template_handle pipeline, zest_shader_handle shader);
void zest_SetPipelineFragShader(zest_pipeline_template_handle pipeline, zest_shader_handle shader);
```

---

## Vertex Input

### zest_AddVertexInputBindingDescription

```cpp
void zest_AddVertexInputBindingDescription(
    zest_pipeline_template_handle pipeline,
    zest_uint binding,
    zest_uint stride,
    zest_input_rate input_rate
);
```

---

### zest_AddVertexAttribute

```cpp
void zest_AddVertexAttribute(
    zest_pipeline_template_handle pipeline,
    zest_uint binding,
    zest_uint location,
    zest_format format,
    zest_uint offset
);
```

---

## State Configuration

### zest_SetPipelineTopology

```cpp
void zest_SetPipelineTopology(zest_pipeline_template_handle pipeline, zest_topology topology);
```

Values: `zest_topology_triangle_list`, `zest_topology_triangle_strip`, `zest_topology_line_list`, `zest_topology_point_list`

---

### zest_SetPipelineCullMode

```cpp
void zest_SetPipelineCullMode(zest_pipeline_template_handle pipeline, zest_cull_mode mode);
```

Values: `zest_cull_mode_none`, `zest_cull_mode_front`, `zest_cull_mode_back`

---

### zest_SetPipelinePolygonFillMode

```cpp
void zest_SetPipelinePolygonFillMode(zest_pipeline_template_handle pipeline, zest_fill_mode mode);
```

Values: `zest_fill_mode_fill`, `zest_fill_mode_line`, `zest_fill_mode_point`

---

### zest_SetPipelineDepthTest

```cpp
void zest_SetPipelineDepthTest(
    zest_pipeline_template_handle pipeline,
    zest_bool depth_test,
    zest_bool depth_write
);
```

---

### zest_SetPipelineBlend

```cpp
void zest_SetPipelineBlend(
    zest_pipeline_template_handle pipeline,
    VkPipelineColorBlendAttachmentState blend_state
);
```

---

## Blend State Helpers

```cpp
VkPipelineColorBlendAttachmentState zest_BlendStateNone(void);
VkPipelineColorBlendAttachmentState zest_AlphaBlendState(void);
VkPipelineColorBlendAttachmentState zest_AdditiveBlendState(void);
VkPipelineColorBlendAttachmentState zest_PreMultipliedBlendState(void);
```

---

## Using Pipelines

### zest_PipelineWithTemplate

Build/get pipeline for current command list.

```cpp
zest_pipeline zest_PipelineWithTemplate(
    zest_pipeline_template_handle handle,
    zest_command_list cmd
);
```

---

## Shader Creation

### zest_CreateShaderFromFile

```cpp
zest_shader_handle zest_CreateShaderFromFile(
    zest_device device,
    const char *source_path,
    const char *cache_name,
    zest_shader_type type,
    zest_bool reload_on_change
);
```

Types: `zest_vertex_shader`, `zest_fragment_shader`, `zest_compute_shader`

---

### zest_CreateShader

Create shader from SPIR-V data.

```cpp
zest_shader_handle zest_CreateShader(
    zest_device device,
    void *spirv_data,
    zest_size spirv_size,
    const char *name,
    zest_shader_type type
);
```

---

## See Also

- [Pipelines Concept](../concepts/pipelines.md)
- [Instancing Tutorial](../tutorials/03-instancing.md)
