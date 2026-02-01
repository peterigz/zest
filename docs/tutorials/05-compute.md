# Tutorial 5: Compute Shaders

Learn to use compute shaders for GPU-accelerated processing.

**Example:** `examples/SDL2/zest-compute-example`

## What You'll Learn

- Creating compute pipelines
- Storage buffers
- Compute passes in frame graphs
- GPU particle simulation

## Creating a Compute Shader

```cpp
zest_shader_handle comp = zest_CreateShaderFromFile(
    device,
    "shaders/particles.comp",
    "particles.spv",
    zest_compute_shader,
    true  // Don't cache the shader, useful if you need to make changes and recompile
);
```

## Creating a Compute Pipeline

```cpp
zest_compute_handle compute = zest_CreateCompute(device, "particle_sim", comp, user_data);
```

## Storage Buffers

```cpp
zest_buffer_info_t info = zest_CreateBufferInfo(
    zest_buffer_type_storage,
    zest_memory_usage_gpu_only
);
zest_buffer particles = zest_CreateBuffer(device, sizeof(particle_t) * MAX_PARTICLES, &info);
```

## Frame Graph with Compute Pass

```cpp
if (zest_BeginFrameGraph(context, "Compute Graph", &cache_key)) {
    zest_ImportSwapchainResource();
    zest_resource_node particle_resource = zest_ImportBufferResource("Particles", particles);

    // Compute pass
    zest_compute compute_obj = zest_GetCompute(compute_handle);
    zest_BeginComputePass(compute_obj, "Simulate"); {
        zest_ConnectInput(particle_resource);
        zest_ConnectOutput(particle_resource);  // Read-modify-write
        zest_SetPassTask(ComputeCallback, app);
        zest_EndPass();
    }

    // Render pass
    zest_BeginRenderPass("Draw Particles"); {
        zest_ConnectInput(particle_resource);
        zest_ConnectSwapChainOutput();
        zest_SetPassTask(RenderCallback, app);
        zest_EndPass();
    }

    return zest_EndFrameGraph();
}
```

## Compute Callback

```cpp
void ComputeCallback(zest_command_list cmd, void* data) {
    app_t* app = (app_t*)data;

    zest_compute compute = zest_GetCompute(app->compute_handle);
    zest_cmd_BindComputePipeline(cmd, compute);

    // Push constants
    compute_push_t push = { .delta_time = app->dt };
    zest_cmd_SendPushConstants(cmd, &push, sizeof(push));

    // Dispatch
    zest_cmd_DispatchCompute(cmd, (MAX_PARTICLES + 255) / 256, 1, 1);
}
```

## Compute Shader (GLSL)

```glsl
#version 450

layout(local_size_x = 256) in;

layout(set = 0, binding = 5) buffer Particles {
    vec4 positions[];
};

layout(push_constant) uniform Push {
    float delta_time;
} push;

void main() {
    uint id = gl_GlobalInvocationID.x;
    vec4 pos = positions[id];

    // Update particle
    pos.y += pos.w * push.delta_time;  // w = velocity

    positions[id] = pos;
}
```

## Full Example

See `examples/SDL2/zest-compute-example/zest-compute-example.cpp` for complete particle simulation.

## Next Steps

- [Tutorial 6: Multi-Pass Rendering](06-multipass.md)
- [Frame Graph Concept](../concepts/frame-graph/passes.md) - Compute pass details
