# Compute API

Functions for compute shader support.

## Creation

### zest_CreateCompute

Create a compute pipeline.

```cpp
zest_compute_handle zest_CreateCompute(
    zest_device device,
    const char *name,
    zest_shader_handle shader,
    void *user_data
);
```

---

### zest_GetCompute

Get compute object from handle.

```cpp
zest_compute zest_GetCompute(zest_compute_handle handle);
```

---

### zest_FreeCompute

Free compute pipeline.

```cpp
void zest_FreeCompute(zest_compute_handle handle);
```

---

## Usage in Frame Graph

```cpp
zest_compute compute = zest_GetCompute(compute_handle);
zest_BeginComputePass(compute, "My Compute Pass"); {
    zest_ConnectInput(input_resource);
    zest_ConnectOutput(output_resource);
    zest_SetPassTask(ComputeCallback, user_data);
    zest_EndPass();
}
```

---

## Compute Commands

### zest_cmd_BindComputePipeline

```cpp
void zest_cmd_BindComputePipeline(zest_command_list cmd, zest_compute compute);
```

---

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

## Example Callback

```cpp
void ComputeCallback(zest_command_list cmd, void* data) {
    app_t* app = (app_t*)data;

    zest_compute compute = zest_GetCompute(app->compute_handle);
    zest_cmd_BindComputePipeline(cmd, compute);

    push_t push = { .delta_time = app->dt };
    zest_cmd_SendPushConstants(cmd, &push, sizeof(push));

    zest_cmd_DispatchCompute(cmd, (COUNT + 255) / 256, 1, 1);
}
```

---

## See Also

- [Compute Tutorial](../tutorials/05-compute.md)
- [Frame Graph Concept](../concepts/frame-graph.md)
