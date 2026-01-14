# Tutorial 7: Shadow Mapping

Learn to implement shadow mapping with depth render targets.

**Example:** `examples/GLFW/zest-shadow-mapping`

## What You'll Learn

- Depth-only render passes
- Shadow map generation
- Sampling depth textures
- PCF filtering basics

## Creating a Shadow Map

```cpp
zest_image_info_t shadow_info = zest_CreateImageInfo(SHADOW_MAP_SIZE, SHADOW_MAP_SIZE);
shadow_info.format = zest_format_d32_sfloat;
shadow_info.flags = zest_image_preset_depth;

zest_image_handle shadow_map = zest_CreateImage(device, &shadow_info);
```

## Frame Graph with Shadow Pass

```cpp
if (zest_BeginFrameGraph(context, "Shadow Mapping", &cache_key)) {
    zest_ImportSwapchainResource();
    zest_resource_node shadow = zest_ImportImageResource("Shadow", shadow_image, zest_texture_2d_binding);

    // Pass 1: Render shadow map
    zest_BeginRenderPass("Shadow Pass"); {
        zest_ConnectOutput(shadow);
        zest_SetPassTask(RenderShadowMap, app);
        zest_EndPass();
    }

    // Pass 2: Render scene with shadows
    zest_BeginRenderPass("Scene"); {
        zest_ConnectInput(shadow);
        zest_ConnectSwapChainOutput();
        zest_SetPassTask(RenderScene, app);
        zest_EndPass();
    }

    return zest_EndFrameGraph();
}
```

## Shadow Pass Pipeline

```cpp
// Depth-only pipeline (no fragment shader output)
zest_pipeline_template_handle shadow_pipeline = zest_BeginPipelineTemplate(device, "shadow");
zest_SetPipelineVertShader(shadow_pipeline, shadow_vert);
// No fragment shader needed for depth-only
zest_SetPipelineDepthTest(shadow_pipeline, true, true);
zest_SetPipelineCullMode(shadow_pipeline, zest_cull_mode_front);  // Reduce peter-panning
```

## Shadow Map Shader

```glsl
// Vertex shader - output light-space position
layout(push_constant) uniform Push {
    mat4 light_space_matrix;
} push;

void main() {
    gl_Position = push.light_space_matrix * model * vec4(position, 1.0);
}
```

## Sampling Shadows

```glsl
float ShadowCalculation(vec4 light_space_pos) {
    vec3 proj = light_space_pos.xyz / light_space_pos.w;
    proj = proj * 0.5 + 0.5;  // Transform to [0,1]

    float current_depth = proj.z;
    float shadow_depth = texture(sampler2D(shadow_map, shadow_sampler), proj.xy).r;

    float bias = 0.005;
    return current_depth - bias > shadow_depth ? 0.5 : 1.0;
}
```

## Full Example

See `examples/GLFW/zest-shadow-mapping/zest-shadow-mapping.cpp` for complete implementation.

Also check `examples/GLFW/zest-cascading-shadows/` for cascaded shadow maps.

## Next Steps

- [Examples Gallery](../examples/index.md) - All working examples
- [Frame Graph Concept](../concepts/frame-graph.md) - Multi-pass details
