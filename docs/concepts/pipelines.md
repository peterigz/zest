# Pipelines

Zest uses a **pipeline template** system. Instead of creating GPU pipelines directly, you configure templates that are compiled on-demand when first used. This deferred compilation approach simplifies pipeline management - you only need to specify what you want, and Zest handles the actual device pipeline creation and caching.

## Pipeline Templates vs Built Pipelines

| Pipeline Template | Built Pipeline |
|-------------------|----------------|
| Configuration only | Actual GPU pipeline |
| Stored in device | Created per command list |
| Reusable | Cached internally |
| Configured once | Used for drawing |

```cpp
// 1. Create template (once at startup)
zest_pipeline_template my_template = zest_CreatePipelineTemplate(device, "my_pipeline");
// ... configure ...

// 2. Build pipeline (in render callback)
zest_pipeline pipeline = zest_GetPipeline(my_template, cmd);
zest_cmd_BindPipeline(cmd, pipeline);
```

## Creating a Pipeline Template

```cpp
zest_pipeline_template sprite_template = zest_CreatePipelineTemplate(device, "sprite_pipeline");

// Set shaders
zest_SetPipelineShaders(sprite_template, vertex_shader, fragment_shader);

// Configure vertex input
zest_AddVertexInputBindingDescription(sprite_template, 0, sizeof(vertex_t), zest_input_rate_vertex);
zest_AddVertexAttribute(sprite_template, 0, 0, zest_format_r32g32b32_sfloat, offsetof(vertex_t, position));
zest_AddVertexAttribute(sprite_template, 0, 1, zest_format_r32g32_sfloat, offsetof(vertex_t, uv));
zest_AddVertexAttribute(sprite_template, 0, 2, zest_format_r8g8b8a8_unorm, offsetof(vertex_t, color));

// Configure state
zest_SetPipelineTopology(sprite_template, zest_topology_triangle_list);
zest_SetPipelineCullMode(sprite_template, zest_cull_mode_none);
zest_SetPipelineDepthTest(sprite_template, true, true);
zest_SetPipelineBlend(sprite_template, zest_AlphaBlendState());
```

## Shaders

### Creating Shaders

```cpp
// From file (auto-compiles if needed)
zest_shader_handle vert = zest_CreateShaderFromFile(
    device,
    "shaders/sprite.vert",    // Source file in GLSL 
    "sprite.vert.spv",        // Cache name
    zest_vertex_shader,       // Shader type
    false                     // Whether to disable caching (false = enable caching)
);

// From pre-compiled SPIR-V file
zest_shader_handle frag = zest_CreateShaderFromSPVFile(
    device,
    "shaders/sprite.frag.spv",
    zest_fragment_shader
);

// From SPIR-V in memory
zest_shader_handle comp = zest_AddShaderFromSPVMemory(
    device,
    "compute.spv",            // Name
    spirv_data,               // Buffer
    spirv_size,               // Size
    zest_compute_shader       // Shader type
);
```

### Setting Shaders

```cpp
// Graphics pipeline - both shaders at once
zest_SetPipelineShaders(my_template, vertex_shader, fragment_shader);

// Or set shaders separately
zest_SetPipelineVertShader(my_template, vertex_shader);
zest_SetPipelineFragShader(my_template, fragment_shader);

// Combined vertex+fragment shader (single file containing both)
zest_SetPipelineShader(my_template, combined_shader);
```

Note: Compute shaders are handled separately through `zest_CreateCompute()` rather than pipeline templates. See the [Compute Pipelines](#compute-pipelines) section.

## Vertex Input

Vertex input describes how vertex data is organized in memory and how it maps to shader inputs. A **binding** represents a buffer of vertex data, while **attributes** describe individual data fields within that buffer.

### Binding Descriptions

```cpp
// Per-vertex data (binding 0)
zest_AddVertexInputBindingDescription(
    my_template,
    0,                        // Binding index
    sizeof(vertex_t),         // Stride
    zest_input_rate_vertex    // Per vertex
);

// Per-instance data (binding 1)
zest_AddVertexInputBindingDescription(
    my_template,
    1,
    sizeof(instance_t),
    zest_input_rate_instance  // Per instance
);
```

### Attribute Descriptions

```cpp
// Position at location 0
zest_AddVertexAttribute(
    my_template,
    0,                              // Binding
    0,                              // Location
    zest_format_r32g32b32_sfloat,   // Format
    offsetof(vertex_t, position)    // Offset
);

// UV at location 1
zest_AddVertexAttribute(my_template, 0, 1, zest_format_r32g32_sfloat, offsetof(vertex_t, uv));

// Color at location 2 (packed)
zest_AddVertexAttribute(my_template, 0, 2, zest_format_r8g8b8a8_unorm, offsetof(vertex_t, color));
```

### Clearing Vertex Input

```cpp
zest_ClearVertexInputBindingDescriptions(my_template);
```

## Pipeline State

### Topology

```cpp
zest_SetPipelineTopology(my_template, zest_topology_triangle_list);    // Default
zest_SetPipelineTopology(my_template, zest_topology_triangle_strip);
zest_SetPipelineTopology(my_template, zest_topology_line_list);
zest_SetPipelineTopology(my_template, zest_topology_point_list);
```

### Culling

```cpp
zest_SetPipelineCullMode(my_template, zest_cull_mode_back);   // Cull back faces
zest_SetPipelineCullMode(my_template, zest_cull_mode_front);  // Cull front faces
zest_SetPipelineCullMode(my_template, zest_cull_mode_none);   // No culling (Default)
```

### Front Face

```cpp
zest_SetPipelineFrontFace(my_template, zest_front_face_counter_clockwise);  // Default
zest_SetPipelineFrontFace(my_template, zest_front_face_clockwise);
```

### Polygon Mode

```cpp
zest_SetPipelinePolygonFillMode(my_template, zest_polygon_mode_fill);   // Solid (Default)
zest_SetPipelinePolygonFillMode(my_template, zest_polygon_mode_line);   // Wireframe
zest_SetPipelinePolygonFillMode(my_template, zest_polygon_mode_point);  // Points
```

### Depth Testing

```cpp
// Enable depth test and write
zest_SetPipelineDepthTest(my_template, true, true);

// Depth test only (no write)
zest_SetPipelineDepthTest(my_template, true, false);

// Disable depth
zest_SetPipelineDepthTest(my_template, false, false);
```

## Blend States

### Built-in Blend States

```cpp
zest_SetPipelineBlend(my_template, zest_BlendStateNone());       // No blending
zest_SetPipelineBlend(my_template, zest_AlphaBlendState());      // Standard alpha
zest_SetPipelineBlend(my_template, zest_AdditiveBlendState());   // Additive
zest_SetPipelineBlend(my_template, zest_PreMultiplyBlendState()); // Pre-multiplied
zest_SetPipelineBlend(my_template, zest_ImGuiBlendState());      // ImGui style
```

### Custom Blend State

```cpp
zest_color_blend_attachment_t blend = {
    .blend_enable = ZEST_TRUE,
    .src_color_blend_factor = zest_blend_factor_src_alpha,
    .dst_color_blend_factor = zest_blend_factor_one_minus_src_alpha,
    .color_blend_op = zest_blend_op_add,
    .src_alpha_blend_factor = zest_blend_factor_one,
    .dst_alpha_blend_factor = zest_blend_factor_one_minus_src_alpha,
    .alpha_blend_op = zest_blend_op_add,
    .color_write_mask = zest_color_component_r_bit | zest_color_component_g_bit |
                        zest_color_component_b_bit | zest_color_component_a_bit
};
zest_SetPipelineBlend(my_template, blend);
```

## Copying Templates

Create variants from existing templates:

```cpp
// Copy and modify
zest_pipeline_template wireframe = zest_CopyPipelineTemplate("wireframe", solid_template);
zest_SetPipelinePolygonFillMode(wireframe, zest_polygon_mode_line);

// Copy with different shaders
zest_pipeline_template shadow = zest_CopyPipelineTemplate("shadow", main_template);
zest_SetPipelineShaders(shadow, shadow_vert, shadow_frag);
```

## Pipeline Layouts

Pipeline layouts define the interface between shaders and descriptor sets (resources like buffers and textures). Zest provides a default bindless layout that works for most use cases. Custom layouts are not currently fully supported, although most of the API is there some of the internals need updating to allow it.

```cpp
zest_pipeline_layout_info_t layout_info = zest_NewPipelineLayoutInfo(device);
zest_AddPipelineLayoutDescriptorLayout(&layout_info, my_descriptor_layout);
zest_pipeline_layout layout = zest_CreatePipelineLayout(&layout_info);
```

## Using Pipelines

```cpp
void RenderCallback(const zest_command_list cmd, void* user_data) {
    app_t* app = (app_t*)user_data;

    // Build pipeline for this command list, once built the pipeline is cached and subsequent
	// calls will retrieve the pipeline from the cache instead.
    zest_pipeline pipeline = zest_GetPipeline(app->pipeline_template, cmd);

    // Bind pipeline
    zest_cmd_BindPipeline(cmd, pipeline);

    // Set viewport/scissor (min_depth, max_depth)
    zest_cmd_SetScreenSizedViewport(cmd, 0.0f, 1.0f);

    // Draw...
    zest_cmd_Draw(cmd, vertex_count, instance_count, 0, 0);
}
```

## Compute Pipelines

```cpp
// Create compute shader
zest_shader_handle comp = zest_CreateShaderFromFile(
    device, "compute.comp", "compute.spv", zest_compute_shader, false);

// Create compute handle
zest_compute_handle compute = zest_CreateCompute(device, "particle_sim", comp);

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
