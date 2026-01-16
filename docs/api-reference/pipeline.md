# Pipeline API

Functions for creating and configuring graphics and compute pipelines. Zest uses pipeline templates that are compiled into actual GPU pipelines on demand, with automatic caching for efficiency.

## Pipeline Template Creation

### `zest_CreatePipelineTemplate`

Create a new pipeline template with default settings.

```cpp
zest_pipeline_template zest_CreatePipelineTemplate(zest_device device, const char *name);
```

**Usage:** Start building a new rendering pipeline. Templates define the shader programs, vertex input layout, rasterization state, and blending configuration.

```cpp
zest_pipeline_template my_pipeline = zest_CreatePipelineTemplate(device, "my_custom_pipeline");
zest_SetPipelineShaders(my_pipeline, vertex_shader, fragment_shader);
zest_SetPipelineTopology(my_pipeline, zest_topology_triangle_list);
// Configure other settings...
```

---

### `zest_CopyPipelineTemplate`

Copy an existing template to create a variant with modified settings.

```cpp
zest_pipeline_template zest_CopyPipelineTemplate(const char *name, zest_pipeline_template source);
```

**Usage:** Create pipeline variations (e.g., wireframe version of a solid pipeline) without duplicating all configuration code.

```cpp
// Create wireframe variant of existing pipeline
zest_pipeline_template wireframe = zest_CopyPipelineTemplate("wireframe_pipeline", solid_pipeline);
zest_SetPipelinePolygonFillMode(wireframe, zest_polygon_mode_line);
```

---

### `zest_FreePipelineTemplate`

Free a pipeline template and all its cached pipeline instances.

```cpp
void zest_FreePipelineTemplate(zest_pipeline_template pipeline_template);
```

**Usage:** Clean up pipelines that are no longer needed, such as during hot-reload or shutdown.

```cpp
zest_FreePipelineTemplate(old_pipeline);
```

---

### `zest_PipelineIsValid`

Check if a pipeline template is in a valid state. This will build the pipeline if it hasn't been already and report if the pipeline failed to build or had errors.

```cpp
zest_bool zest_PipelineIsValid(zest_pipeline_template pipeline);
```

**Usage:** Verify pipeline configuration before use, especially after shader changes.

```cpp
if (!zest_PipelineIsValid(pipeline)) {
    // Handle invalid pipeline - likely shader compilation error
}
```

---

## Shaders

### `zest_SetPipelineShaders`

Set both vertex and fragment shaders at once.

```cpp
void zest_SetPipelineShaders(zest_pipeline_template pipeline_template,
                              zest_shader_handle vertex_shader,
                              zest_shader_handle fragment_shader);
```

**Usage:** Configure the shader programs for a graphics pipeline.

```cpp
zest_shader_handle vert = zest_CreateShaderFromFile(device, "shaders/mesh.vert", "mesh_vert", zest_vertex_shader, 0);
zest_shader_handle frag = zest_CreateShaderFromFile(device, "shaders/mesh.frag", "mesh_frag", zest_fragment_shader, 0);
zest_SetPipelineShaders(pipeline, vert, frag);
```

---

### `zest_SetPipelineVertShader` / `zest_SetPipelineFragShader`

Set vertex or fragment shader individually.

```cpp
void zest_SetPipelineVertShader(zest_pipeline_template pipeline_template, zest_shader_handle shader);
void zest_SetPipelineFragShader(zest_pipeline_template pipeline_template, zest_shader_handle shader);
```

**Usage:** Update individual shaders, useful for shader hot-reloading or when multiple pipelines share shaders.

```cpp
// Swap out just the fragment shader
zest_SetPipelineFragShader(pipeline, new_fragment_shader);
```

---

### `zest_SetPipelineShader`

Set a combined vertex and fragment shader (single shader handle containing both stages).

```cpp
void zest_SetPipelineShader(zest_pipeline_template pipeline_template,
                            zest_shader_handle combined_vertex_and_fragment_shader);
```

**Usage:** Use with shader systems that compile vertex and fragment stages together.

---

## Vertex Input

### `zest_AddVertexInputBindingDescription`

Define a vertex buffer binding (stride and input rate).

```cpp
zest_vertex_binding_desc_t zest_AddVertexInputBindingDescription(
    zest_pipeline_template pipeline_template,
    zest_uint binding,
    zest_uint stride,
    zest_input_rate input_rate
);
```

**Input Rates:**
- `zest_input_rate_vertex` - Advance per vertex for general mesh drawing
- `zest_input_rate_instance` - Advance per instance for instanced drawing

**Usage:** Configure how vertex data is read from buffers.

```cpp
// Per-vertex position/normal/uv data
zest_AddVertexInputBindingDescription(pipeline, 0, sizeof(Vertex_t), zest_input_rate_vertex);

// Per-instance transform data
zest_AddVertexInputBindingDescription(pipeline, 1, sizeof(InstanceData_t), zest_input_rate_instance);
```

---

### `zest_AddVertexAttribute`

Define a vertex attribute within a binding.

```cpp
void zest_AddVertexAttribute(
    zest_pipeline_template pipeline_template,
    zest_uint binding,
    zest_uint location,
    zest_format format,
    zest_uint offset
);
```

**Common Formats:**
- `zest_format_r32g32b32_sfloat` - vec3 (position, normal)
- `zest_format_r32g32_sfloat` - vec2 (UV coordinates)
- `zest_format_r8g8b8a8_unorm` - Color (4 bytes normalized)
- `zest_format_r32g32b32a32_sfloat` - vec4

**Usage:** Map vertex struct fields to shader input locations.

```cpp
typedef struct {
    float position[3];  // location 0
    float normal[3];    // location 1
    float uv[2];        // location 2
} Vertex;

zest_AddVertexInputBindingDescription(pipeline, 0, sizeof(Vertex), zest_input_rate_vertex);
zest_AddVertexAttribute(pipeline, 0, 0, zest_format_r32g32b32_sfloat, offsetof(Vertex, position));
zest_AddVertexAttribute(pipeline, 0, 1, zest_format_r32g32b32_sfloat, offsetof(Vertex, normal));
zest_AddVertexAttribute(pipeline, 0, 2, zest_format_r32g32_sfloat, offsetof(Vertex, uv));
```

---

### `zest_ClearVertexInputBindingDescriptions`

Remove all vertex binding descriptions from a pipeline template.

```cpp
void zest_ClearVertexInputBindingDescriptions(zest_pipeline_template pipeline_template);
```

**Usage:** Reset vertex vertex input binding descriptions when copying pipelines that need different requirements.

---

### `zest_ClearVertexAttributeDescriptions`

Remove all vertex attribute descriptions from a pipeline template.

```cpp
void zest_ClearVertexAttributeDescriptions(zest_pipeline_template pipeline_template);
```

**Usage:** Reset vertex input configuration when copying pipelines that need different layouts.

```cpp
zest_pipeline_template fullscreen = zest_CopyPipelineTemplate("fullscreen", base_pipeline);
zest_ClearVertexInputBindingDescriptions(fullscreen);
zest_ClearVertexAttributeDescriptions(fullscreen);
zest_SetPipelineDisableVertexInput(fullscreen);  // Fullscreen pass uses vertex ID
```

---

### `zest_SetPipelineEnableVertexInput` / `zest_SetPipelineDisableVertexInput`

Enable or disable vertex input for the pipeline.

```cpp
void zest_SetPipelineEnableVertexInput(zest_pipeline_template pipeline_template);
void zest_SetPipelineDisableVertexInput(zest_pipeline_template pipeline_template);
```

**Usage:** Disable vertex input for fullscreen passes that generate vertices in the shader.

```cpp
// Fullscreen quad using gl_VertexIndex
zest_SetPipelineDisableVertexInput(fullscreen_pipeline);
```

---

## State Configuration

### `zest_SetPipelineTopology`

Set the primitive topology (how vertices form primitives).

```cpp
void zest_SetPipelineTopology(zest_pipeline_template pipeline_template, zest_topology topology);
```

**Topology Values:**
| Value | Description |
|-------|-------------|
| `zest_topology_point_list` | Individual points |
| `zest_topology_line_list` | Pairs of vertices form lines |
| `zest_topology_line_strip` | Connected line segments |
| `zest_topology_triangle_list` | Every 3 vertices form a triangle (most common) |
| `zest_topology_triangle_strip` | Connected triangles sharing edges |
| `zest_topology_triangle_fan` | Triangles sharing a central vertex |

**Usage:** Define how the GPU assembles primitives from vertex data.

```cpp
zest_SetPipelineTopology(mesh_pipeline, zest_topology_triangle_list);
zest_SetPipelineTopology(particle_pipeline, zest_topology_point_list);
zest_SetPipelineTopology(line_pipeline, zest_topology_line_list);
```

---

### `zest_SetPipelineCullMode`

Set face culling mode.

```cpp
void zest_SetPipelineCullMode(zest_pipeline_template pipeline_template, zest_cull_mode cull_mode);
```

**Cull Modes:**
| Value | Description |
|-------|-------------|
| `zest_cull_mode_none` | No culling (render both sides) |
| `zest_cull_mode_front` | Cull front-facing triangles |
| `zest_cull_mode_back` | Cull back-facing triangles (default for most 3D) |
| `zest_cull_mode_front_and_back` | Cull all triangles (useful for stencil operations) |

**Usage:** Optimize rendering by not drawing hidden faces.

```cpp
zest_SetPipelineCullMode(opaque_pipeline, zest_cull_mode_back);  // Standard 3D
zest_SetPipelineCullMode(double_sided_pipeline, zest_cull_mode_none);  // Foliage, UI
```

---

### `zest_SetPipelineFrontFace`

Set which winding order is considered front-facing.

```cpp
void zest_SetPipelineFrontFace(zest_pipeline_template pipeline_template, zest_front_face front_face);
```

**Values:**
- `zest_front_face_counter_clockwise` - CCW winding is front (OpenGL default)
- `zest_front_face_clockwise` - CW winding is front (Zest default)

**Usage:** Match the winding order used by your mesh data.

```cpp
zest_SetPipelineFrontFace(pipeline, zest_front_face_clockwise);
```

---

### `zest_SetPipelinePolygonFillMode`

Set how polygons are rasterized.

```cpp
void zest_SetPipelinePolygonFillMode(zest_pipeline_template pipeline_template, zest_polygon_mode polygon_mode);
```

**Polygon Modes:**
| Value | Description |
|-------|-------------|
| `zest_polygon_mode_fill` | Fill triangles (default) |
| `zest_polygon_mode_line` | Draw triangle edges only (wireframe) |
| `zest_polygon_mode_point` | Draw triangle vertices only |

**Usage:** Debug visualization or stylized rendering.

```cpp
zest_SetPipelinePolygonFillMode(wireframe_pipeline, zest_polygon_mode_line);
```

---

### `zest_SetPipelineDepthTest`

Configure depth testing and writing.

```cpp
void zest_SetPipelineDepthTest(zest_pipeline_template pipeline_template,
                                zest_bool enable_test,
                                zest_bool write_enable);
```

**Usage:** Control depth buffer behavior for different rendering scenarios.

```cpp
// Standard opaque rendering - test and write depth
zest_SetPipelineDepthTest(opaque_pipeline, ZEST_TRUE, ZEST_TRUE);

// Transparent rendering - test but don't write
zest_SetPipelineDepthTest(transparent_pipeline, ZEST_TRUE, ZEST_FALSE);

// UI/overlay - no depth testing
zest_SetPipelineDepthTest(ui_pipeline, ZEST_FALSE, ZEST_FALSE);
```

---

### `zest_SetPipelineDepthBias`

Enable or disable depth bias (polygon offset).

```cpp
void zest_SetPipelineDepthBias(zest_pipeline_template pipeline_template, zest_bool enabled);
```

**Usage:** Prevent z-fighting for decals, shadows, or coplanar geometry.

```cpp
zest_SetPipelineDepthBias(decal_pipeline, ZEST_TRUE);
```

---

### `zest_SetPipelineBlend`

Set the color blending state.

```cpp
void zest_SetPipelineBlend(zest_pipeline_template pipeline_template,
                           zest_color_blend_attachment_t blend_attachment);
```

**Usage:** Configure how fragment colors combine with the framebuffer.

```cpp
zest_SetPipelineBlend(transparent_pipeline, zest_AlphaBlendState());
zest_SetPipelineBlend(additive_pipeline, zest_AdditiveBlendState());
zest_SetPipelineBlend(opaque_pipeline, zest_BlendStateNone());
```

---

### `zest_SetPipelineViewCount`

Set the number of views for multiview rendering.

```cpp
void zest_SetPipelineViewCount(zest_pipeline_template pipeline_template, zest_uint view_count);
```

**Usage:** Configure pipelines for VR sterio, cubemap, cascading shadows, or other multiview rendering scenarios.

```cpp
zest_SetPipelineViewCount(vr_pipeline, 2);  // Left and right eye
```

---

### `zest_SetPipelineLayout`

Set a custom pipeline layout.

```cpp
void zest_SetPipelineLayout(zest_pipeline_template pipeline_template, zest_pipeline_layout pipeline_layout);
```

**Usage:** Use a custom descriptor layout for pipelines requiring non-default bindings.

```cpp
zest_pipeline_layout custom_layout = zest_CreatePipelineLayout(&layout_info);
zest_SetPipelineLayout(pipeline, custom_layout);
```

---

## Blend State Helpers

Pre-configured blend states for common scenarios.

```cpp
zest_color_blend_attachment_t zest_BlendStateNone(void);               // No blending (opaque)
zest_color_blend_attachment_t zest_AlphaBlendState(void);              // Standard alpha blending
zest_color_blend_attachment_t zest_AlphaOnlyBlendState(void);          // Blend alpha channel only
zest_color_blend_attachment_t zest_AdditiveBlendState(void);           // Additive blending
zest_color_blend_attachment_t zest_AdditiveBlendState2(void);          // Alternative additive
zest_color_blend_attachment_t zest_PreMultiplyBlendState(void);        // Pre-multiplied alpha
zest_color_blend_attachment_t zest_PreMultiplyBlendStateForSwap(void); // Pre-multiplied for swapchain
zest_color_blend_attachment_t zest_MaxAlphaBlendState(void);           // Max alpha blending
zest_color_blend_attachment_t zest_ImGuiBlendState(void);              // ImGui-compatible blending
```

**Usage:**

```cpp
// Opaque geometry
zest_SetPipelineBlend(opaque_pipeline, zest_BlendStateNone());

// Transparent with alpha
zest_SetPipelineBlend(transparent_pipeline, zest_AlphaBlendState());

// Particle effects (additive glow)
zest_SetPipelineBlend(particle_pipeline, zest_AdditiveBlendState());

// UI with pre-multiplied alpha textures
zest_SetPipelineBlend(ui_pipeline, zest_PreMultiplyBlendState());
```

---

## Using Pipelines

### `zest_GetPipeline`

Get or build a pipeline for use with a command list.

```cpp
zest_pipeline zest_GetPipeline(zest_pipeline_template pipeline_template,
                                        const zest_command_list command_list);
```

**Usage:** Retrieve a compiled pipeline during rendering. Pipelines are cached and reused automatically.

```cpp
void render_callback(zest_command_list cmd, void *user_data) {
    zest_pipeline pipeline = zest_GetPipeline(my_pipeline, cmd);
    zest_cmd_BindPipeline(cmd, pipeline);
    // Draw commands...
}
```

## Pipeline Layout

### `zest_NewPipelineLayoutInfo`

Create a new pipeline layout info structure.

```cpp
zest_pipeline_layout_info_t zest_NewPipelineLayoutInfo(zest_device device);
```

---

### `zest_NewPipelineLayoutInfoWithGlobalBindless`

Create a pipeline layout info that includes the global bindless descriptor set.

```cpp
zest_pipeline_layout_info_t zest_NewPipelineLayoutInfoWithGlobalBindless(zest_device device);
```

**Usage:** Most pipelines should use this to access the bindless resource system.

```cpp
zest_pipeline_layout_info_t info = zest_NewPipelineLayoutInfoWithGlobalBindless(device);
// Add additional descriptor layouts if needed
zest_pipeline_layout layout = zest_CreatePipelineLayout(&info);
```

---

### `zest_AddPipelineLayoutDescriptorLayout`

Add a descriptor set layout to the pipeline layout.

```cpp
void zest_AddPipelineLayoutDescriptorLayout(zest_pipeline_layout_info_t *info, zest_set_layout layout);
```

**Usage:** Add custom descriptor set layouts beyond the global bindless set.

---

### `zest_SetPipelineLayoutPushConstantRange`

Configure push constants for the pipeline layout. 

```cpp
void zest_SetPipelineLayoutPushConstantRange(zest_pipeline_layout_info_t *info,
                                              zest_uint size,
                                              zest_supported_shader_stages stage_flags);
```

**Usage:** Define push constant range accessible to shaders.

```cpp
typedef struct {
    float mvp[16];
    float time;
} PushConstants;

zest_pipeline_layout_info_t info = zest_NewPipelineLayoutInfoWithGlobalBindless(device);
zest_SetPipelineLayoutPushConstantRange(&info, sizeof(PushConstants),
    zest_pipeline_stage_vertex_shader_bit | zest_pipeline_stage_fragment_shader_bit);
```

---

### `zest_CreatePipelineLayout`

Create a pipeline layout from the info structure.

```cpp
zest_pipeline_layout zest_CreatePipelineLayout(zest_pipeline_layout_info_t *info);
```

---

### `zest_GetPipelineLayout`

Get the layout from an existing pipeline.

```cpp
zest_pipeline_layout zest_GetPipelineLayout(zest_pipeline pipeline);
```

---

### `zest_GetDefaultPipelineLayout`

Get the device's default pipeline layout (includes global bindless).

```cpp
zest_pipeline_layout zest_GetDefaultPipelineLayout(zest_device device);
```

---

## Shader Creation

### `zest_CreateShaderFromFile`

Create a shader from a source file. As Zest current only has a platform layer for Vulkan this will need a glsl shader which gets compiled using shaderc.

```cpp
zest_shader_handle zest_CreateShaderFromFile(zest_device device,
                                              const char *file,
                                              const char *name,
                                              zest_shader_type type,
                                              zest_bool disable_caching);
```

**Shader Types:**
- `zest_vertex_shader`
- `zest_fragment_shader`
- `zest_compute_shader`

**Usage:** Load and compile shaders from source files.

```cpp
zest_shader_handle vert = zest_CreateShaderFromFile(
    device,
    "shaders/mesh.vert",
    "mesh_vertex.spv",  //Will be cached to disk with this name
    zest_vertex_shader,
    ZEST_FALSE  // Enable caching
);

zest_shader_handle frag = zest_CreateShaderFromFile(
    device,
    "shaders/mesh.frag",
    "mesh_fragment.spv",
    zest_fragment_shader,
    ZEST_FALSE
);
```

---

### `zest_CreateShader`

Create a shader from source code string. As Zest current only has a platform layer for Vulkan this will need a glsl shader which gets compiled using shaderc. 

```cpp
zest_shader_handle zest_CreateShader(zest_device device,
                                      const char *shader_code,
                                      zest_shader_type type,
                                      const char *name,
                                      zest_bool disable_caching);
```

**Usage:** Compile shaders from runtime-generated or embedded source code.

```cpp
const char *shader_source = "...";
zest_shader_handle shader = zest_CreateShader(device, shader_source, zest_fragment_shader, "dynamic_frag", ZEST_TRUE);
```

---

### `zest_CreateShaderSPVMemory`

Create a shader from pre-compiled SPIR-V bytecode in memory.

```cpp
zest_shader_handle zest_CreateShaderSPVMemory(zest_device device,
                                               const unsigned char *shader_code,
                                               zest_uint spv_length,
                                               const char *name,
                                               zest_shader_type type);
```

**Usage:** Load embedded SPIR-V shaders compiled at build time.

```cpp
// shader_spv.h generated by build system

zest_shader_handle shader = zest_CreateShaderSPVMemory(
    device,
    mesh_vert_spv,
    sizeof(mesh_vert_spv),
    "mesh_vert.spv",
    zest_vertex_shader
);
```

---

### `zest_CreateShaderFromSPVFile`

Load a pre-compiled SPIR-V shader from a file.

```cpp
zest_shader_handle zest_CreateShaderFromSPVFile(zest_device device,
                                              const char *filename,
                                              zest_shader_type type);
```

**Usage:** Load shaders compiled offline with glslc or similar tools.

```cpp
zest_shader_handle vert = zest_CreateShaderFromSPVFile(device, "shaders/mesh.vert.spv", zest_vertex_shader);
```

---

### `zest_AddShaderFromSPVMemory`

Load a pre-compiled SPIR-V shader from a memory buffer.

```cpp
zest_shader_handle zest_AddShaderFromSPVMemory(zest_device device,
                                                const char *name,
                                                const void *buffer,
                                                zest_uint size,
                                                zest_shader_type type);
```

---

### `zest_ValidateShader`

Validate shader source code without creating a shader object.

```cpp
zest_bool zest_ValidateShader(zest_device device,
                               const char *shader_code,
                               zest_shader_type type,
                               const char *name);
```

**Usage:** Check shader validity before committing to compilation.

---

### `zest_ReloadShader`

Reload a shader from its original source.

```cpp
zest_bool zest_ReloadShader(zest_shader_handle shader);
```

**Usage:** Hot-reload shaders during development.

```cpp
if (file_changed) {
    if (zest_ReloadShader(my_shader)) {
        // Shader reloaded successfully
    }
}
```

---

### `zest_CompileShader`

Compile or recompile a shader.

```cpp
zest_bool zest_CompileShader(zest_shader_handle shader);
```

---

### `zest_GetShader`

Get the shader object pointer from a handle.

```cpp
zest_shader zest_GetShader(zest_shader_handle shader_handle);
```

---

### `zest_FreeShader`

Free a shader and its resources.

```cpp
void zest_FreeShader(zest_shader_handle shader);
```

---

## Compute Pipelines

### `zest_CreateCompute`

Create a compute pipeline.

```cpp
zest_compute_handle zest_CreateCompute(zest_device device,
                                        const char *name,
                                        zest_shader_handle shader);
```

**Usage:** Set up a compute shader for GPU computation.

```cpp
zest_shader_handle compute_shader = zest_CreateShaderFromFile(
    device, "shaders/particle_sim.comp", "particle_sim", zest_compute_shader, ZEST_FALSE);

zest_compute_handle compute = zest_CreateCompute(device, "particle_simulation", compute_shader);
```

---

### `zest_GetCompute`

Get the compute object pointer from a handle.

```cpp
zest_compute zest_GetCompute(zest_compute_handle compute_handle);
```

---

### `zest_FreeCompute`

Free a compute pipeline.

```cpp
void zest_FreeCompute(zest_compute_handle compute);
```

---

## Complete Example

```cpp
// Create and configure a complete rendering pipeline
zest_pipeline_template create_mesh_pipeline(zest_device device) {
    // Create shaders
    zest_shader_handle vert = zest_CreateShaderFromFile(
        device, "shaders/mesh.vert", "mesh_vert", zest_vertex_shader, ZEST_FALSE);
    zest_shader_handle frag = zest_CreateShaderFromFile(
        device, "shaders/mesh.frag", "mesh_frag", zest_fragment_shader, ZEST_FALSE);

    // Create pipeline template
    zest_pipeline_template pipeline = zest_CreatePipelineTemplate(device, "mesh_pipeline");

    // Set shaders
    zest_SetPipelineShaders(pipeline, vert, frag);

    // Configure vertex input
    typedef struct {
        float position[3];
        float normal[3];
        float uv[2];
    } MeshVertex;

    zest_AddVertexInputBindingDescription(pipeline, 0, sizeof(MeshVertex), zest_input_rate_vertex);
    zest_AddVertexAttribute(pipeline, 0, 0, zest_format_r32g32b32_sfloat, offsetof(MeshVertex, position));
    zest_AddVertexAttribute(pipeline, 0, 1, zest_format_r32g32b32_sfloat, offsetof(MeshVertex, normal));
    zest_AddVertexAttribute(pipeline, 0, 2, zest_format_r32g32_sfloat, offsetof(MeshVertex, uv));

    // Configure rasterization
    zest_SetPipelineTopology(pipeline, zest_topology_triangle_list);
    zest_SetPipelineCullMode(pipeline, zest_cull_mode_back);
    zest_SetPipelineFrontFace(pipeline, zest_front_face_clockwise);
    zest_SetPipelinePolygonFillMode(pipeline, zest_polygon_mode_fill);

    // Configure depth testing
    zest_SetPipelineDepthTest(pipeline, ZEST_TRUE, ZEST_TRUE);

    // Configure blending (opaque)
    zest_SetPipelineBlend(pipeline, zest_BlendStateNone());

    return pipeline;
}
```

---

## See Also

- [Pipelines Concept](../concepts/pipelines.md)
- [Instancing Tutorial](../tutorials/03-instancing.md)
- [Frame Graph API](frame-graph.md)
