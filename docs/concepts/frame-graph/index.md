# Frame Graph

The frame graph is Zest's core execution model. Instead of manually managing barriers, semaphores, and resource states, you declare a graph of render passes with their dependencies, and Zest handles the rest.

## Why Frame Graphs?

Traditional Vulkan requires explicit synchronization:

```cpp
// Manual barrier insertion
vkCmdPipelineBarrier(cmd,
    VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
    VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
    0, 0, nullptr, 0, nullptr, 1, &imageBarrier);

// Explicit semaphore management
VkSubmitInfo submit = {...};
submit.waitSemaphoreCount = 1;
submit.pWaitSemaphores = &renderFinished;
```

This is error-prone and verbose. Frame graphs solve this by:

1. **Automatic barriers** - Inserted based on resource usage
2. **Automatic synchronization** - Semaphores between queues
3. **Pass culling** - Unused passes are removed
4. **Resource aliasing** - Transient resources can share memory

## Basic Structure

A frame graph is built by declaring resources and passes:

```cpp
if (zest_BeginFrameGraph(context, "My Graph", &cache_key)) {
    // 1. Import external resources
    zest_ImportSwapchainResource();

    // 2. Create transient resources (optional)
    zest_resource_node depth = zest_AddTransientImageResource("Depth", &depth_info);
    zest_resource_node shadow_map = zest_AddTransientImageResource("Shadows", &shadow_info);

    // 3. Define passes
    zest_BeginRenderPass("Shadow Pass"); {
        zest_ConnectOutput(shadow_map);
        zest_SetPassTask(RenderShadows, app);
        zest_EndPass();
    }

    zest_BeginRenderPass("Main Pass"); {
        zest_ConnectInput(shadow_map);
        zest_ConnectSwapChainOutput();
        zest_ConnectOutput(depth);
        zest_SetPassTask(RenderScene, app);
        zest_EndPass();
    }

    // 4. Compile
    frame_graph = zest_EndFrameGraph();
}
```

## Threading

Notice that the functions between `zest_BeginFrameGraph` and `zest_EndFrameGraph`
don't take a context: the begin call sets a *thread-local* current builder, and
every builder function (`zest_BeginRenderPass`, `zest_ConnectOutput`,
`zest_SetPassTask` and so on) operates on it implicitly.

This has two practical consequences:

- **Build a graph from a single OS thread.** All calls from `zest_BeginFrameGraph`
  to `zest_EndFrameGraph` must happen on the same thread. Different threads can
  build graphs concurrently as long as each is working with a different context —
  the intended pattern for running a command graph on another thread is to give it
  its own (often headless) context. Two threads building graphs on the *same*
  context is not supported.
- **Language bindings:** runtimes that migrate lightweight threads between OS
  threads (e.g. Go's goroutines) must pin the OS thread for the duration of the
  build (`runtime.LockOSThread()` in Go) so that all builder calls see the same
  thread-local state.

## Pass Types

Zest supports three types of passes:

| Pass Type | Function | Use Case |
|-----------|----------|----------|
| Render | `zest_BeginRenderPass` | Drawing with graphics pipelines |
| Compute | `zest_BeginComputePass` | Compute shader dispatch |
| Transfer | `zest_BeginTransferPass` | Data uploads and copies |

See [Passes](passes.md) for detailed documentation.

## Resource Types

Resources flow through the graph as inputs and outputs:

| Resource Type | Description |
|--------------|-------------|
| **Swapchain** | The window's presentation surface |
| **Imported** | Existing images/buffers from outside the graph (only required to import if they require writing to or in an initial state that requires them to be transitioned with a barrier) |
| **Transient** | Frame-lifetime resources allocated by the graph |

See [Resources](resources.md) for detailed documentation.

## Topics

| Page | Description |
|------|-------------|
| [Passes](passes.md) | Render, compute, and transfer passes |
| [Resources](resources.md) | Importing, transient resources, and resource groups |
| [Execution](execution.md) | Building, caching, and executing frame graphs |
| [Debugging](debugging.md) | Debug output and optimization flags |

## Example: Deferred Renderer

```cpp
if (zest_BeginFrameGraph(context, "Deferred Renderer", &cache_key)) {
    zest_ImportSwapchainResource();

    // Transient G-buffer
    zest_resource_node albedo = zest_AddTransientImageResource("Albedo", &rgba_info);
    zest_resource_node normal = zest_AddTransientImageResource("Normal", &rgba_info);
    zest_resource_node depth = zest_AddTransientImageResource("Depth", &depth_info);

    // Pass 1: G-Buffer
    zest_BeginRenderPass("G-Buffer"); {
        zest_ConnectOutput(albedo);
        zest_ConnectOutput(normal);
        zest_ConnectOutput(depth);
        zest_SetPassTask(RenderGBuffer, app);
        zest_EndPass();
    }

    // Pass 2: Lighting
    zest_BeginRenderPass("Lighting"); {
        zest_ConnectInput(albedo);
        zest_ConnectInput(normal);
        zest_ConnectInput(depth);
        zest_ConnectSwapChainOutput();
        zest_SetPassTask(RenderLighting, app);
        zest_EndPass();
    }

    frame_graph = zest_EndFrameGraph();
}
```

## See Also

- [First Application](../getting-started/first-application.md) - Basic frame graph usage
- [Multi-Pass Tutorial](../tutorials/06-multipass.md) - Advanced frame graph patterns
- [Frame Graph API](../api-reference/frame-graph.md) - Complete function reference
