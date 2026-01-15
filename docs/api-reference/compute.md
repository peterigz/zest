# Compute API

Functions for creating, managing, and executing compute shaders. Compute shaders run on the GPU and are ideal for parallel data processing tasks like particle simulations, image processing, physics calculations, and general-purpose GPU (GPGPU) workloads.

## Creation

### zest_CreateCompute

Creates a compute pipeline from a compiled compute shader. The returned handle is used to reference the compute object when building frame graphs and executing dispatches.

```cpp
zest_compute_handle zest_CreateCompute(
    zest_device device,
    const char *name,
    zest_shader_handle shader
);
```

**Parameters:**
- `device` - The Zest device that will own the compute pipeline
- `name` - A debug name for identifying the compute object
- `shader` - Handle to a compiled compute shader (created via shader compilation)

**Returns:** A handle to the created compute pipeline.

**Example:**
```cpp
// Load and create a compute shader
zest_shader_handle comp_shader = zest_slang_CreateShader(
    device, "shaders/particles.slang", "particles.spv",
    "computeMain", zest_compute_shader, true
);

// Create the compute pipeline
zest_compute_handle compute = zest_CreateCompute(
    device, "Particle Simulation", comp_shader
);
```

---

### zest_GetCompute

Retrieves the compute pointer from a handle. Use this to pass the compute object into command recording functions. The returned pointer is only valid while the compute resource has not been freed.

```cpp
zest_compute zest_GetCompute(zest_compute_handle compute_handle);
```

**Parameters:**
- `compute_handle` - Handle obtained from `zest_CreateCompute`

**Returns:** Pointer to the compute object.

**Example:**
```cpp
// Get the compute pointer for binding during command recording
zest_compute compute = zest_GetCompute(app->compute_handle);
zest_cmd_BindComputePipeline(command_list, compute);
```

---

### zest_FreeCompute

Frees a compute pipeline and all associated resources. The free operation is deferred until the GPU is no longer using the resource.

```cpp
void zest_FreeCompute(zest_compute_handle compute_handle);
```

**Parameters:**
- `compute_handle` - Handle to the compute pipeline to free

**Example:**
```cpp
// Clean up compute resources on shutdown
zest_FreeCompute(app->compute_handle);
```

---

## Usage in Frame Graph

Compute passes are added to a frame graph using `zest_BeginComputePass`. The frame graph compiler automatically handles synchronization barriers between compute and graphics passes.

```cpp
zest_compute compute = zest_GetCompute(compute_handle);
zest_BeginComputePass(compute, "Particle Update"); {
    // Connect buffer as both input and output (read-modify-write)
    zest_ConnectInput(particle_buffer);
    zest_ConnectOutput(particle_buffer);
    zest_SetPassTask(ComputeCallback, user_data);
    zest_EndPass();
}
```

**Frame graph functions:**
- `zest_BeginComputePass(compute, name)` - Starts a compute pass definition
- `zest_ConnectInput(resource)` - Declares a resource as read input
- `zest_ConnectOutput(resource)` - Declares a resource as write output
- `zest_SetPassTask(callback, user_data)` - Sets the execution callback
- `zest_EndPass()` - Ends the pass definition

---

## Compute Commands

### zest_cmd_BindComputePipeline

Binds a compute pipeline for subsequent dispatch commands. Must be called within a compute pass callback before dispatching.

```cpp
void zest_cmd_BindComputePipeline(
    const zest_command_list command_list,
    zest_compute compute
);
```

**Parameters:**
- `command_list` - The command list from the execution callback
- `compute` - Compute object obtained from `zest_GetCompute`

**Example:**
```cpp
void MyComputeCallback(zest_command_list command_list, void *user_data) {
    app_t *app = (app_t*)user_data;
    zest_compute compute = zest_GetCompute(app->compute_handle);
    zest_cmd_BindComputePipeline(command_list, compute);
    // ... dispatch commands follow
}
```

---

### zest_cmd_DispatchCompute

Dispatches compute work groups for execution. The total number of shader invocations equals `group_count_x * group_count_y * group_count_z * local_size` (where local_size is defined in the shader).

```cpp
void zest_cmd_DispatchCompute(
    const zest_command_list command_list,
    zest_uint group_count_x,
    zest_uint group_count_y,
    zest_uint group_count_z
);
```

**Parameters:**
- `command_list` - The command list from the execution callback
- `group_count_x` - Number of work groups in the X dimension
- `group_count_y` - Number of work groups in the Y dimension
- `group_count_z` - Number of work groups in the Z dimension

**Example:**
```cpp
// Dispatch enough groups to cover all particles
// Shader uses local_size of 256, so divide particle count by 256
zest_uint particle_count = 256 * 1024;
zest_cmd_DispatchCompute(command_list, particle_count / 256, 1, 1);

// For a 2D image processing shader with 16x16 local size
zest_uint image_width = 1920;
zest_uint image_height = 1080;
zest_cmd_DispatchCompute(
    command_list,
    (image_width + 15) / 16,   // Round up to cover all pixels
    (image_height + 15) / 16,
    1
);
```

---

## Complete Example

A typical compute shader workflow for a particle simulation:

```cpp
// 1. Create the compute pipeline (during initialization)
zest_shader_handle shader = zest_slang_CreateShader(
    device, "shaders/particle.slang", "particle_comp.spv",
    "computeMain", zest_compute_shader, true
);
app->compute = zest_CreateCompute(device, "Particles", shader, app);

// 2. Create a storage buffer for particle data
zest_buffer_info_t buffer_info = zest_CreateBufferInfo(
    zest_buffer_type_vertex_storage, zest_memory_usage_gpu_only
);
app->particle_buffer = zest_CreateBuffer(device, buffer_size, &buffer_info);
app->particle_buffer_index = zest_AcquireStorageBufferIndex(
    device, app->particle_buffer
);

// 3. Define the frame graph with compute and render passes
if (zest_BeginFrameGraph(context, "Particle Demo", &cache_key)) {
    zest_resource_node particles = zest_ImportBufferResource(
        "particles", app->particle_buffer, 0
    );
    zest_resource_node swapchain = zest_ImportSwapchainResource();

    // Compute pass updates particle positions
    zest_compute compute = zest_GetCompute(app->compute);
    zest_BeginComputePass(compute, "Update Particles"); {
        zest_ConnectInput(particles);
        zest_ConnectOutput(particles);
        zest_SetPassTask(UpdateParticlesCallback, app);
        zest_EndPass();
    }

    // Render pass draws particles as point sprites
    zest_BeginRenderPass("Draw Particles"); {
        zest_ConnectInput(particles);
        zest_ConnectSwapChainOutput();
        zest_SetPassTask(DrawParticlesCallback, app);
        zest_EndPass();
    }

    frame_graph = zest_EndFrameGraph();
}

// 4. Compute callback records dispatch commands
void UpdateParticlesCallback(zest_command_list command_list, void *user_data) {
    app_t *app = (app_t*)user_data;

    zest_compute compute = zest_GetCompute(app->compute);
    zest_cmd_BindComputePipeline(command_list, compute);

    // Send buffer index and simulation parameters via push constants
    push_constants_t push = {
        .buffer_index = app->particle_buffer_index,
        .delta_time = app->frame_time
    };
    zest_cmd_SendPushConstants(command_list, &push, sizeof(push));

    // Dispatch: 256K particles with 256 threads per group
    zest_cmd_DispatchCompute(command_list, (256 * 1024) / 256, 1, 1);
}
```

---

## See Also

- [Compute Tutorial](../tutorials/05-compute.md)
- [Frame Graph Concept](../concepts/frame-graph.md)
