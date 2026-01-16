# Pipelines

Zest uses a **pipeline template** system. Instead of creating Vulkan pipelines directly, you configure templates that are compiled on-demand when first used.

## Pipeline Templates vs Built Pipelines

| Pipeline Template | Built Pipeline |
|-------------------|----------------|
| Configuration only | Actual Vulkan pipeline |
| Stored in device | Created per command list |
| Reusable | Cached internally |
| Configured once | Used for drawing |

```cpp
// 1. Create template (once at startup)
zest_pipeline_template_handle template = zest_BeginPipelineTemplate(device, "my_pipeline");
// ... configure ...

// 2. Build pipeline (in render callback)
zest_pipeline pipeline = zest_GetPipeline(template, cmd);
zest_cmd_BindPipeline(cmd, pipeline);
```

## Creating a Pipeline Template

```cpp
zest_pipeline_template_handle template = zest_BeginPipelineTemplate(device, "sprite_pipeline");

// Set shaders
zest_SetPipelineShaders(template, vertex_shader, fragment_shader);

// Configure vertex input
zest_AddVertexInputBindingDescription(template, 0, sizeof(vertex_t), zest_input_rate_vertex);
zest_AddVertexAttribute(template, 0, 0, zest_format_r32g32b32_sfloat, offsetof(vertex_t, position));
zest_AddVertexAttribute(template, 0, 1, zest_format_r32g32_sfloat, offsetof(vertex_t, uv));
zest_AddVertexAttribute(template, 0, 2, zest_format_r8g8b8a8_unorm, offsetof(vertex_t, color));

// Configure state
zest_SetPipelineTopology(template, zest_topology_triangle_list);
zest_SetPipelineCullMode(template, zest_cull_mode_none);
zest_SetPipelineDepthTest(template, true, true);
zest_SetPipelineBlend(template, zest_AlphaBlendState());
```

## Shaders

### Creating Shaders

```cpp
// From file (auto-compiles if needed)
zest_shader_handle vert = zest_CreateShaderFromFile(
    device,
    "shaders/sprite.vert",    // Source file
    "sprite.vert.spv",        // Cache name
    zest_vertex_shader,       // Stage
    true                      // Reload on change
);

// From memory (pre-compiled SPIR-V)
zest_shader_handle frag = zest_CreateShader(
    device,
    spirv_data,
    spirv_size,
    "sprite.frag.spv",
    zest_fragment_shader
);
```

### Setting Shaders

```cpp
// Graphics pipeline
zest_SetPipelineShaders(template, vertex_shader, fragment_shader);

// Or separately
zest_SetPipelineVertShader(template, vertex_shader);
zest_SetPipelineFragShader(template, fragment_shader);

// Compute pipeline
zest_SetPipelineComputeShader(template, compute_shader);
```

## Vertex Input

### Binding Descriptions

```cpp
// Per-vertex data (binding 0)
zest_AddVertexInputBindingDescription(
    template,
    0,                        // Binding index
    sizeof(vertex_t),         // Stride
    zest_input_rate_vertex    // Per vertex
);

// Per-instance data (binding 1)
zest_AddVertexInputBindingDescription(
    template,
    1,
    sizeof(instance_t),
    zest_input_rate_instance  // Per instance
);
```

### Attribute Descriptions

```cpp
// Position at location 0
zest_AddVertexAttribute(
    template,
    0,                              // Binding
    0,                              // Location
    zest_format_r32g32b32_sfloat,   // Format
    offsetof(vertex_t, position)    // Offset
);

// UV at location 1
zest_AddVertexAttribute(template, 0, 1, zest_format_r32g32_sfloat, offsetof(vertex_t, uv));

// Color at location 2 (packed)
zest_AddVertexAttribute(template, 0, 2, zest_format_r8g8b8a8_unorm, offsetof(vertex_t, color));
```

### Clearing Vertex Input

```cpp
zest_ClearVertexInputBindingDescriptions(template);
```

## Pipeline State

### Topology

```cpp
zest_SetPipelineTopology(template, zest_topology_triangle_list);    // Default
zest_SetPipelineTopology(template, zest_topology_triangle_strip);
zest_SetPipelineTopology(template, zest_topology_line_list);
zest_SetPipelineTopology(template, zest_topology_point_list);
```

### Culling

```cpp
zest_SetPipelineCullMode(template, zest_cull_mode_back);   // Cull back faces
zest_SetPipelineCullMode(template, zest_cull_mode_front);  // Cull front faces
zest_SetPipelineCullMode(template, zest_cull_mode_none);   // No culling
```

### Front Face

```cpp
zest_SetPipelineFrontFace(template, zest_front_face_counter_clockwise);  // Default
zest_SetPipelineFrontFace(template, zest_front_face_clockwise);
```

### Polygon Mode

```cpp
zest_SetPipelinePolygonFillMode(template, zest_fill_mode_fill);       // Solid
zest_SetPipelinePolygonFillMode(template, zest_fill_mode_line);       // Wireframe
zest_SetPipelinePolygonFillMode(template, zest_fill_mode_point);      // Points
```

### Depth Testing

```cpp
// Enable depth test and write
zest_SetPipelineDepthTest(template, true, true);

// Depth test only (no write)
zest_SetPipelineDepthTest(template, true, false);

// Disable depth
zest_SetPipelineDepthTest(template, false, false);
```

## Blend States

### Built-in Blend States

```cpp
zest_SetPipelineBlend(template, zest_BlendStateNone());       // No blending
zest_SetPipelineBlend(template, zest_AlphaBlendState());      // Standard alpha
zest_SetPipelineBlend(template, zest_AdditiveBlendState());   // Additive
zest_SetPipelineBlend(template, zest_PreMultipliedBlendState()); // Pre-multiplied
zest_SetPipelineBlend(template, zest_ImGuiBlendState());      // ImGui style
```

### Custom Blend State

```cpp
VkPipelineColorBlendAttachmentState blend = {
    .blendEnable = VK_TRUE,
    .srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
    .dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
    .colorBlendOp = VK_BLEND_OP_ADD,
    .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
    .dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
    .alphaBlendOp = VK_BLEND_OP_ADD,
    .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                      VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT
};
zest_SetPipelineBlend(template, blend);
```

## Copying Templates

Create variants from existing templates:

```cpp
// Copy and modify
zest_pipeline_template_handle wireframe = zest_CopyPipelineTemplate("wireframe", solid_template);
zest_SetPipelinePolygonFillMode(wireframe, zest_fill_mode_line);

// Copy with different shaders
zest_pipeline_template_handle shadow = zest_CopyPipelineTemplate("shadow", main_template);
zest_SetPipelineShaders(shadow, shadow_vert, shadow_frag);
```

## Pipeline Layouts

For custom descriptor set layouts:

```cpp
zest_pipeline_layout_info_t layout_info = zest_NewPipelineLayoutInfo();
zest_AddPipelineLayoutDescriptorLayout(&layout_info, my_descriptor_layout);
zest_pipeline_layout layout = zest_CreatePipelineLayout(device, &layout_info);
```

## Using Pipelines

```cpp
void RenderCallback(const zest_command_list cmd, void* user_data) {
    app_t* app = (app_t*)user_data;

    // Build pipeline for this command list
    zest_pipeline pipeline = zest_GetPipeline(app->template, cmd);

    // Bind pipeline
    zest_cmd_BindPipeline(cmd, pipeline);

    // Set viewport/scissor
    zest_cmd_SetScreenSizedViewport(cmd);

    // Draw...
    zest_cmd_Draw(cmd, vertex_count, instance_count, 0, 0);
}
```

## Compute Pipelines

```cpp
// Create compute shader
zest_shader_handle comp = zest_CreateShaderFromFile(
    device, "compute.comp", "compute.spv", zest_compute_shader, true);

// Create compute handle
zest_compute_handle compute = zest_CreateCompute(device, "particle_sim", comp, user_data);

// Use in compute pass
zest_compute compute_obj = zest_GetCompute(compute);
zest_BeginComputePass(compute_obj, "Simulate"); {
    zest_SetPassTask(ComputeCallback, app);
    zest_EndPass();
}
```

## Best Practices

1. **Create templates at startup** - Not during rendering
2. **Use `zest_CopyPipelineTemplate`** - For variants with minor differences
3. **Cache pipelines are automatic** - `zest_GetPipeline` handles caching
4. **Match vertex input to shaders** - Locations must align

## See Also

- [Shaders Guide](../guides/custom-shaders.md) - Shader compilation details
- [Pipeline API](../api-reference/pipeline.md) - Complete function reference
- [Instancing Tutorial](../tutorials/03-instancing.md) - Pipeline with instancing
