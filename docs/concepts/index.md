# Concepts

This section provides deep dives into Zest's core systems. Each concept page explains the design, API, and best practices.

## Core Architecture

| Concept | Description |
|---------|-------------|
| [Device & Context](device-and-context.md) | The two fundamental objects in Zest |
| [Frame Graph](frame-graph.md) | The declarative execution model |
| [Memory Management](memory.md) | TLSF allocator and memory pools |
| [Bindless Descriptors](bindless.md) | Global descriptor set design |

## Resource Types

| Concept | Description |
|---------|-------------|
| [Pipelines](pipelines.md) | Graphics and compute pipeline templates |
| [Buffers](buffers.md) | Vertex, index, uniform, and storage buffers |
| [Images](images.md) | Textures, render targets, and samplers |
| [Layers](layers.md) | Instance, mesh, and font rendering systems |

## Recommended Reading Order

1. **[Device & Context](device-and-context.md)** - Start here to understand the basic structure
2. **[Frame Graph](frame-graph.md)** - The most important concept in Zest
3. **[Pipelines](pipelines.md)** - How to set up rendering state
4. **[Buffers](buffers.md)** and **[Images](images.md)** - Resource management
5. **[Layers](layers.md)** - High-level rendering abstractions
6. **[Bindless Descriptors](bindless.md)** - Advanced descriptor management
