# Minimal Template

The absolute minimal Zest application - a blank screen in ~80 lines of code.

## What It Does

Demonstrates the simplest possible Zest setup:
- Create device and context
- Build a frame graph that renders nothing
- Cache the frame graph for reuse
- Main loop with frame begin/end

This is the best starting point to understand Zest's core architecture.

## Zest Features Used

- **Device/Context**: `zest_implglfw_CreateDevice`, `zest_CreateContext`
- **Frame Graph**: `zest_BeginFrameGraph`, `zest_EndFrameGraph`
- **Frame Graph Caching**: `zest_InitialiseCacheKey`, `zest_GetCachedFrameGraph`
- **Swapchain**: `zest_ImportSwapchainResource`, `zest_ConnectSwapChainOutput`
- **Render Pass**: `zest_BeginRenderPass`, `zest_SetPassTask`, `zest_EndPass`
- **Execution**: `zest_QueueFrameGraphForExecution`
- **Frame Lifecycle**: `zest_BeginFrame`, `zest_EndFrame`
