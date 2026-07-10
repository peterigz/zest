# Zest Test Suite

Comprehensive test suite for Zest's frame graph system, pipeline states, resource management, and compute shaders.

## What It Does

Runs 80 automated tests, executed twice — once with dynamic rendering (the default path on VK 1.3 hardware) and once with the legacy VkRenderPass fallback forced — covering:
- **Frame Graph Tests**: Empty graphs, single pass, pass culling, resource culling, chained dependencies, cyclic dependency detection, caching
- **Stress Tests**: Large numbers of passes, transient buffers/images, multi-queue synchronization
- **Pipeline Tests**: Depth states, blending, culling, topology, polygon mode, front face, vertex input, rasterization
- **Resource Tests**: Image format support, creation/destruction, views, buffers, uniform buffers, staging operations, samplers
- **User Error Tests**: Missing `UpdateDevice`, `EndFrame`, swapchain import, end pass, bad ordering, state errors
- **Compute Tests**: Frame graph execution, timeline semaphores, mipmap chains, read-modify-write patterns

## Zest Features Tested

- **Frame Graph Compilation**: Pass ordering, barrier insertion, semaphore synchronization
- **Resource Management**: Transient allocation, aliasing, lifetime tracking
- **Pipeline State Hashing**: Detecting state changes for pipeline caching
- **Validation Layer Integration**: `zest_ResetValidationErrors`, error counting
- **Multi-Queue Support**: Graphics + compute + transfer queue coordination
- **Compute Shaders**: Immediate execution, cached execution, timeline waits

## Running Tests

Run from the repository root (shader paths are relative to it):

```bash
E:/Projects/C++/Zest-Build/examples/SDL2/Debug/zest-tests.exe
```

Tests output colored pass/fail results to the console. Each test runs through multiple phases and validates expected outcomes against actual results. The process exit code is 0 only if every test passes in both render pass modes.
