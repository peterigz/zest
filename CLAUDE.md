# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

Zest is a lightweight, single-header Vulkan rendering library. It uses a single header (`zest.h`) with a platform layer (`zest_vulkan.h`). The library is written in C (C11 compatible) but must be compilable in C++.

**Current Status:** Major refactor in progress. Working on frame graph compiler, memory management separation between device/context, and test suite.

## Build Commands

```bash
# Configure (from project root)
cmake -B build -G "Visual Studio 17 2022"

# Build
cmake --build build --config Release

# Run tests
build\examples\GLFW\Release\zest-tests.exe

# Run minimal example
build\examples\GLFW\Release\zest-minimal-template.exe
```

Optional CMake flags:
- `-DZEST_ENABLE_SLANG=ON` - Enable Slang shader compiler (requires VULKAN_SDK env var)

## Architecture

### Two Core Objects

**Device (`zest_device_t`):** Manages Vulkan instance, physical devices, shader library, pipeline templates, bindless descriptor sets, and all GPU resources. One device per application.

**Context (`zest_context_t`):** Per-window/swapchain object. Owns frame resources, manages frame graph compilation/execution, and linear allocators for frame-lifetime resources. One device can serve multiple contexts.

### Frame Graph System

The frame graph is the core execution model:
```c
zest_BeginFrameGraph()
├── zest_ImportSwapchainResource()
├── zest_BeginRenderPass()
│   ├── zest_ConnectSwapChainOutput()
│   ├── zest_SetPassTask(callback, user_data)
│   └── zest_EndPass()
└── zest_EndFrameGraph()
```

The compiler automatically handles barriers, semaphores, pass culling, async queue management, and transient resource creation/destruction. Frame graphs can be cached via `zest_InitialiseCacheKey()`.

### Memory Management

- **TLSF allocator:** Manages both CPU and GPU memory with minimal fragmentation
- **Linear allocators:** For single-frame lifetime resources
- Memory pools auto-expand when exhausted
- Supports memory aliasing for transient resources

### Key Patterns

- **Bindless descriptors:** Single global descriptor layout; resources indexed into descriptor arrays
- **Dynamic rendering:** Uses VK 1.3 dynamic render passes instead of pre-baked ones
- **Layer system:** Manages instanced/mesh rendering with automatic buffer management

## Code Style

### Naming Conventions

| Type | Convention | Example |
|------|------------|---------|
| Public API functions | `zest_` + PascalCase | `zest_BeginFrameGraph` |
| Private functions | `zest__` + snake_case | `zest__allocate_aligned` |
| Platform-specific private | `zest__vk_`, `zest__dx_`, `zest__mtl_` | `zest__vk_create_swapchain` |
| Types | `zest_` + snake_case + `_t` | `zest_device_t` |
| Enums | `zest_` + snake_case | `zest_buffer_usage` |
| Defines | `ZEST_` + UPPER_SNAKE_CASE | `ZEST_IMPLEMENTATION` |

### Function Markers
- `ZEST_API` - Public API functions
- `ZEST_PRIVATE` - Private/internal functions

### Formatting
- Opening braces on same line
- Tabs for indentation
- No line length limits

## File Structure

- `zest.h` - Main API header (~16K lines)
- `zest_vulkan.h` - Vulkan platform implementation (~5K lines)
- `zest_utilities.h` - Optional helpers for GLFW/SDL, image loading, fonts (~2.5K lines)
- `implementations/` - Optional integrations (ImGui, TimelineVFX, Slang)
- `examples/GLFW/` - Example projects using GLFW
- `examples/SDL2/` - Example projects using SDL2

## Using Zest in Examples

Define implementation macros in exactly one .cpp file:
```cpp
#define ZEST_IMPLEMENTATION
#define ZEST_VULKAN_IMPLEMENTATION
#include <zest.h>
```

## Test Suite

Tests are in `examples/GLFW/zest-tests/`:
- `zest-frame-graph-tests.cpp` - Graph compilation tests
- `zest-frame-graph-stress.cpp` - Stress tests
- `zest-pipeline-tests.cpp` - Pipeline state tests
- `zest-resource-management-tests.cpp` - Resource lifecycle tests

36 tests covering frame graph compilation, memory management, pipeline states, and image formats.
