# Zest

**A lightweight single-header rendering library**

Zest is a C11-compatible rendering library with a clean, modern API designed for multiple graphics backends. It provides a frame graph execution model, bindless descriptors, and automatic resource management - all in a single header file. Currently Vulkan is the primary backend, with DirectX, Metal, and WebGPU planned for the future.

## Key Features

- **Single Header** - Just `#include <zest.h>` and you're ready to go
- **Frame Graph System** - Declarative rendering with automatic barrier insertion, pass culling, and resource management
- **Bindless Descriptors** - Global descriptor set with indexed access to all textures and buffers
- **TLSF Memory Allocator** - Efficient GPU memory management with minimal fragmentation
- **Dynamic Rendering** - No pre-baked render pass objects needed; render passes are configured at draw time
- **Layer System** - Built-in support for instanced sprites, meshes and static meshes

## Requirements

- GPU with bindless descriptor support
- SDL2 for windowing (or any other windowing library of your choice)
- C11 compiler (also compiles as C++)

**Vulkan backend:** Requires Vulkan 1.2+ capable GPU

## Quick Start

```c
#define ZEST_IMPLEMENTATION
#define ZEST_VULKAN_IMPLEMENTATION
#include <zest.h>

int main() {
    // Create window and device (one per application)
    zest_window_data_t window = zest_implsdl2_CreateWindow(50, 50, 1280, 768, 0, "My App");
    zest_device device = zest_implsdl2_CreateVulkanDevice(&window, false);

    // Create context
    zest_create_context_info_t info = zest_CreateContextInfo();
    zest_context context = zest_CreateContext(device, &window, &info);

    // Main loop with frame graph...
}
```

## Documentation Sections

<div class="grid cards" markdown>

-   :material-rocket-launch: **Getting Started**

    ---

    Installation, first application, and architecture overview

    [:octicons-arrow-right-24: Get started](getting-started/index.md)

-   :material-book-open-variant: **Concepts**

    ---

    Deep dives into Device, Context, Frame Graph, Pipelines, and more

    [:octicons-arrow-right-24: Learn concepts](concepts/index.md)

-   :material-school: **Tutorials**

    ---

    Step-by-step guides from basic setup to advanced rendering

    [:octicons-arrow-right-24: Follow tutorials](tutorials/index.md)

-   :material-api: **API Reference**

    ---

    Complete reference for all 500+ Zest functions

    [:octicons-arrow-right-24: Browse API](api-reference/index.md)

</div>

## Examples

Zest includes 16 working examples demonstrating various features:

| Example | Description |
|---------|-------------|
| [zest-minimal-template](examples/index.md#minimal-template) | Bare minimum Zest application |
| [zest-imgui-template](examples/index.md#imgui-template) | ImGui integration with docking |
| [zest-compute-example](examples/index.md#compute-example) | Compute shader particle simulation |
| [zest-instancing](examples/index.md#instancing) | GPU instancing with multi-mesh layers |
| [zest-pbr-forward](examples/index.md#pbr-forward) | Physical-based rendering |
| [zest-shadow-mapping](examples/index.md#shadow-mapping) | Shadow mapping techniques |

[View all examples :octicons-arrow-right-24:](examples/index.md)
