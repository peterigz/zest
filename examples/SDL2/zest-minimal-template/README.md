# Minimal Template

The absolute minimal Zest application - a "hello world" triangle in ~130 lines of code.

## What It Does

Demonstrates the simplest possible Zest setup:
- Create device and context
- Compile a vertex/fragment shader pair from file and build a pipeline template
- Build a frame graph with a single render pass that draws to the swapchain
- Cache the frame graph for reuse
- Main loop with frame begin/end

The triangle uses no vertex or index buffers at all. The vertex shader indexes a constant
array with `gl_VertexIndex`, so the pipeline disables vertex input and the pass just issues
`zest_cmd_Draw(command_list, 3, 1, 0, 0)`.

This is the best starting point to understand Zest's core architecture.

## Zest Features Used

- **Device/Context**: `zest_implsdl2_CreateVulkanDevice`, `zest_CreateContext`
- **Shaders**: `zest_CreateShaderFromFile` (`shaders/triangle.vert`, `shaders/triangle.frag`)
- **Pipelines**: `zest_CreatePipelineTemplate`, `zest_SetPipelineShaders`, `zest_SetPipelineDisableVertexInput`, `zest_BlendStateOpaque`, `zest_GetPipeline`
- **Frame Graph**: `zest_BeginFrameGraph`, `zest_EndFrameGraph`
- **Frame Graph Caching**: `zest_InitialiseCacheKey`, `zest_GetCachedFrameGraph`
- **Swapchain**: `zest_ImportSwapchainResource`, `zest_ConnectSwapChainOutput`
- **Render Pass**: `zest_BeginRenderPass`, `zest_SetPassTask`, `zest_EndPass`
- **Commands**: `zest_cmd_SetScreenSizedViewport`, `zest_cmd_BindPipeline`, `zest_cmd_Draw`
- **Frame Lifecycle**: `zest_BeginFrame`, `zest_EndFrame`

## Note

Shader paths are relative to the working directory, which is expected to be the root of the
Zest repository.
