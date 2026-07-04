# Debugging

This page covers tools for debugging and optimizing frame graphs.

## Printing Compiled Graphs

Visualize the compiled frame graph structure:

```cpp
zest_frame_graph graph = zest_EndFrameGraph();
zest_PrintCompiledFrameGraph(graph);
```

Example output:

```
Swapchain Info
Image Available Wait Semaphores:
000002181325E9D0 *
000002181325EA68

Render Finished Signal Semaphores:
000002181325EB00
000002181325EB98 *
000002181325EC30

--- Frame graph Execution Plan, Current FIF: 0 ---
Resource List: Total Resources: 5

Buffer: read particle buffer (0000021801EF0140) - Size: 8388608, Offset: 0
Swapchain Image: Zest Window (000002181325D178)
Image: Imgui Font (0000021801EDB578) - Size: 512 x 64
Buffer: Viewport Vertex Buffer (0000000000000000) - Size: 5840, Offset: 0
Buffer: Viewport Index Buffer (0000000000000000) - Size: 5840, Offset: 0

Transient Placement Schedule: 0 transient resource(s)
  (none - every resource in this graph is imported or persistent)

Graph Wave Layout ([G]raphics, [C]ompute, [T]ransfer)
Wave Index      G       C       T       Pass Count
0                       X       X       2
1               X                       1

Number of Submission Batches: 2

Wave Submission Index 0:
  Target Queue Family: Compute Queue - index: 1
  Waits on the following Semaphores:
     Timeline Semaphore: 0000021801EDB190, Value: 15307, Stages: COMPUTE_SHADER_BIT
  Passes in this batch:
    Pass [0] (QueueType: 2)
       Compute Particles
        Release Buffers:
            read particle buffer (0000021801EF0140) |
            Access: SHADER_WRITE_BIT -> VERTEX_ATTRIBUTE_READ_BIT,
            Pipeline Stage: COMPUTE_SHADER_BIT -> BOTTOM_OF_PIPE_BIT,
            QFI: 1 -> 0, Size: 8388608
  Signal Semaphores:
  Timeline Semaphore: 00000218134146E0, Stage: BOTTOM_OF_PIPE_BIT, Value: 7654

  Target Queue Family: Transfer Queue - index: 2
  Waits on the following Semaphores:
     Timeline Semaphore: 0000021801EDB190, Value: 15307, Stages: TRANSFER_BIT
  Passes in this batch:
    Pass [2] (QueueType: 4)
       Upload ImGui Viewport
        Release Buffers:
            Viewport Vertex Buffer (0000000000000000) |
            Access: TRANSFER_READ_BIT, TRANSFER_WRITE_BIT -> VERTEX_ATTRIBUTE_READ_BIT,
            Pipeline Stage: TRANSFER_BIT -> BOTTOM_OF_PIPE_BIT,
            QFI: 2 -> 0, Size: 5840
            Viewport Index Buffer (0000000000000000) |
            Access: TRANSFER_READ_BIT, TRANSFER_WRITE_BIT -> INDEX_READ_BIT, VERTEX_ATTRIBUTE_READ_BIT,
            Pipeline Stage: TRANSFER_BIT -> BOTTOM_OF_PIPE_BIT,
            QFI: 2 -> 0, Size: 5840
  Signal Semaphores:
  Timeline Semaphore: 0000021813414778, Stage: BOTTOM_OF_PIPE_BIT, Value: 7654

Wave Submission Index 1:
  Target Queue Family: Graphics Queue - index: 0
  Waits on the following Semaphores:
     Timeline Semaphore: 00000218134146E0, Value: 7654, Stages: VERTEX_INPUT_BIT, COMPUTE_SHADER_BIT, TRANSFER_BIT
     Timeline Semaphore: 0000021813414778, Value: 7654, Stages: VERTEX_INPUT_BIT, COMPUTE_SHADER_BIT, TRANSFER_BIT
     Binary Semaphore:   000002181325EA68, Stages: TOP_OF_PIPE_BIT
  Passes in this batch:
    Pass [1] (QueueType: 1)
       Graphics Pass
       Dear ImGui Viewport Pass
        Acquire Images:
            Zest Window (000002181325D178), Layout: UNDEFINED -> COLOR_ATTACHMENT_OPTIMAL,
            Access: NONE -> COLOR_ATTACHMENT_WRITE_BIT,
            Pipeline Stage: TOP_OF_PIPE_BIT -> COLOR_ATTACHMENT_OUTPUT_BIT,
            QFI: 4294967295 -> 4294967295
        Acquire Buffers:
            read particle buffer (0000021801EF0140) |
            Access: SHADER_WRITE_BIT -> VERTEX_ATTRIBUTE_READ_BIT,
            Pipeline Stage: COMPUTE_SHADER_BIT -> VERTEX_INPUT_BIT,
            QFI: 1 -> 0, Size: 8388608
            Viewport Vertex Buffer (0000000000000000) |
            Access: TRANSFER_READ_BIT, TRANSFER_WRITE_BIT -> VERTEX_ATTRIBUTE_READ_BIT,
            Pipeline Stage: TRANSFER_BIT -> VERTEX_INPUT_BIT,
            QFI: 2 -> 0, Size: 5840
            Viewport Index Buffer (0000000000000000) |
            Access: TRANSFER_READ_BIT, TRANSFER_WRITE_BIT -> INDEX_READ_BIT, VERTEX_ATTRIBUTE_READ_BIT,
            Pipeline Stage: TRANSFER_BIT -> VERTEX_INPUT_BIT, INDEX_INPUT_BIT,
            QFI: 2 -> 0, Size: 5840
      RenderArea: (0,0)-(1280x768)
  Signal Semaphores:
  Binary Semaphore: 000002181325EB00 Stage: BOTTOM_OF_PIPE_BIT,
  Timeline Semaphore: 0000021801EDB190, Stage: BOTTOM_OF_PIPE_BIT, Value: 15308

--- End of Report ---```

### Reading the Transient Placement Schedule

The **Transient Placement Schedule** block reports how the compiler placed
transient buffers and images into memory. Transients no longer allocate from a
record-time pool; instead the compiler builds a size-independent *schedule*
(order, lifetimes, and batch/wave hazard info) once, and a per-execution packer
assigns byte offsets into per-category, per-frame-in-flight **arenas** after the
resource providers have set that frame's sizes. Everything printed is for the
current frame in flight, so it reflects the offsets and sizes the GPU is
actually running with.

The graph above has no transients, so the block collapses to a single line. A
post-process chain with a few transient targets looks like this instead:

```
Transient Placement Schedule: 4 transient resource(s)
  Arena usage by category (FIF 1):
    Category               Backing        This Graph     High-Water     Gen
    Image[mem type 0]      8388608        7864320        7864320        0
      Aliasing reclaimed 3932160 bytes (33%): 11796480 bytes of resources packed into 7864320 peak.
    GPU Buffers            2097152        4096           4096           0

  Placement (first-use order, FIF 1):
    #   Resource             Kind Category             Size         Offset       Lifetime    Wave      Batch
    0   Scene Colour         Img  Image[mem type 0]    3932160      0x00000000   [ 0.. 1]  0..0      0..0
    1   Bloom Scratch        Img  Image[mem type 0]    3932160      0x003c0000   [ 1.. 2]  0..1      0..256
    2   Reduce Buffer        Buf  GPU Buffers          4096         0x00000000   [ 1.. 2]  0..1      0..256
          buffer proxy: suballocation of arena backing, usage 0xa3
    3   Blur Target          Img  Image[mem type 0]    3932160      0x00000000   [ 2.. 3]  1..1      256..256
```

**Arena usage by category** — one row per arena category the graph touched.
Buffers and images never share an arena; on Vulkan each distinct memory type
index gets its own image arena (shown as `Image[mem type N]`), and buffers split
into `GPU Buffers` (device-local) and `CPU Buffers` (host-visible).

| Column | Meaning |
|--------|---------|
| `Backing` | Size of the device-memory allocation the context holds for this category, in bytes. Grows to cover the high-water mark; never shrinks (yet). |
| `This Graph` | Peak bytes this graph's transients occupy this execution (the highest end offset). |
| `High-Water` | Largest watermark this arena has ever needed across all graphs that used it. |
| `Gen` | Backing generation. Bumps when the backing is reallocated (grown), which forces bound images to be recreated. |

The **Aliasing reclaimed** line appears when interval packing overlapped
non-conflicting lifetimes onto the same bytes. In the example, three 3.75 MB
images sum to 11.25 MB of requests but pack into a 7.5 MB peak because
`Blur Target` reuses the memory `Scene Colour` vacated — 33% saved. That reuse
is exactly what the compile-time aliasing barriers make safe.

**Placement (first-use order)** — one row per transient, in the order the
packer walks them (first use):

| Column | Meaning |
|--------|---------|
| `#` | Index into the schedule. |
| `Resource` / `Kind` | Name and whether it is an image (`Img`) or buffer (`Buf`). |
| `Category` | Which arena it was placed in. |
| `Size` / `Offset` | This execution's size (bytes) and the assigned offset within the arena (hex). Two resources sharing an offset range but with disjoint lifetimes are aliasing the same memory. |
| `Lifetime` | The `[first..last]` pass-group interval the resource is live over. |
| `Wave` | The execution waves of the first and last use. |
| `Batch` | An opaque identifier (derived from the pass group's `submission_id`, combining wave and queue) for the submission batch the first and last use run in. The packer uses it to apply the hazard rule: same-batch reuse needs an aliasing barrier, cross-wave reuse is already covered by wave semaphores. |

A resource marked `(not placed)` was skipped this execution because a provider
supplied its own buffer or a zero size — it occupies no arena memory that frame.

Where present, the indented detail line under an entry shows the backing:

- **Buffers** are a suballocation of the arena's single fat buffer, so an offset
  that differs from last frame costs nothing. The `usage` value is the buffer
  usage flags.
- **Images** print their persistent per-FIF slot: extent, format, mips, layers,
  the offset they are bound at, and the arena generation they were bound
  against. For a **cached** graph with stable sizes this same image `backend`
  survives across executions (visible via `zest_PrintCachedFrameGraph`) — that
  persistence is the CPU win the arena design buys, since the per-frame image
  create → bindless-write → destroy cycle disappears. For a non-cached graph the
  slot is retired right after execution, so this line only appears while the
  image is still live.

## Checking Compilation Results

Verify frame graph compiled successfully:

```cpp
zest_frame_graph graph = zest_EndFrameGraph();
zest_frame_graph_result result = zest_GetFrameGraphResult(graph);

switch (result) {
    case zest_frame_graph_result_success:
        // All good
        break;
    case zest_frame_graph_result_no_passes:
        // Graph has no passes
        break;
    case zest_frame_graph_result_cycle_detected:
        // Circular dependency found
        break;
    // ... other error cases
}
```

## Understanding Pass Culling

The frame graph compiler automatically removes unused passes. A pass is culled if:

1. It produces no output, so in other words it does not use zest_ConnectOutput.
2. It produces output but that ouput is not consumed and output to an essential target (like the swapchain or any resource you manually set as essential)
3. It is not marked with `zest_DoNotCull()`. 

### Preventing Culling

For passes that should always execute (e.g., debug output, compute side effects):

```cpp
zest_BeginRenderPass("Debug Overlay"); {
    zest_DoNotCull();  // Keep even without swapchain connection
    zest_SetPassTask(DebugCallback, app);
    zest_EndPass();
}
```

### Essential Resources

Mark resources whose production should never be culled:

```cpp
zest_resource_node debug_buffer = zest_AddTransientBufferResource("Debug", &info);
zest_FlagResourceAsEssential(debug_buffer);

// Any pass that outputs to debug_buffer will not be culled
```

## Common Issues

### Pass Not Executing

**Symptom:** A pass callback is never called.

**Causes:**
1. Pass was culled (no path to swapchain)
2. Missing `zest_ConnectOutput()` or `zest_ConnectInput()` calls
3. Resource connection creates no dependency chain to output

**Solution:**
```cpp
// Option 1: Connect to something that reaches swapchain or other essential resource.
zest_ConnectOutput(resource_used_by_later_pass);

// Option 2: Force no culling
zest_DoNotCull();

// Option 3: Mark output as essential
zest_FlagResourceAsEssential(output_resource);
```

### Incorrect Barrier Timing

**Symptom:** Visual artifacts, validation errors about resource states.

**Causes:**
1. Missing resource connection (input or output)
2. Resource used without declaring dependency

**Solution:** Ensure all resource accesses are declared:
```cpp
// If you read a resource, connect it as input
zest_ConnectInput(shadow_map);

// If you write a resource, connect it as output
zest_ConnectOutput(color_target);

// If you read-modify-write, connect both
zest_ConnectInput(particle_buffer);
zest_ConnectOutput(particle_buffer);
```

### Cache Frame Graphs

Avoid recompiling every frame:

```cpp
// BAD: Rebuilds every frame
if (zest_BeginFrameGraph(context, "Graph", NULL)) {
    // ...
    graph = zest_EndFrameGraph();
}

// GOOD: Caches and reuses
zest_frame_graph_cache_key_t key = zest_InitialiseCacheKey(context, NULL, 0);
graph = zest_GetCachedFrameGraph(context, &key);
if (!graph) {
    if (zest_BeginFrameGraph(context, "Graph", &key)) {
        // ...
        graph = zest_EndFrameGraph();
    }
}
```

### Use Transient Resources

Transient resources benefit from memory aliasing:

```cpp
// Transient: memory can be shared with non-overlapping resources
zest_resource_node temp = zest_AddTransientImageResource("Temp", &info);

// vs. Imported: dedicated memory allocation
zest_resource_node persistent = zest_ImportImageResource("Depth", image, provider);
```

## Validation Layers

Enable Vulkan validation layers during development:

```cpp
zest_device_builder device_builder = zest_BeginVulkanDeviceBuilder();
// Add your required Vulkan instance extensions
zest_AddDeviceBuilderExtensions(device_builder, extensions, count);
zest_AddDeviceBuilderValidation(device_builder);
zest_DeviceBuilderLogToConsole(device_builder);
zest_device device = zest_EndDeviceBuilder(device_builder);
```

Or using the helper functions:

```cpp
zest_window_data_t window_data = zest_implsdl2_CreateWindow(50, 50, 1280, 768, 0, "My App");
zest_device device = zest_implsdl2_CreateVulkanDevice(&window_data, true);
```


Validation layers catch:
- Incorrect resource states
- Missing synchronization
- Invalid API usage

## Best Practices

1. **Print during development** - Use `zest_PrintCompiledFrameGraph()` to verify structure
2. **Check compilation results** - Handle errors gracefully
3. **Enable validation** - Catch synchronization issues early
4. **Name everything** - Pass and resource names appear in debug output
5. **Understand culling** - Know why passes are removed

If anything seems amiss please raise an issue, it could be a bug or missing feature in Zest!

## See Also

- [Passes](passes.md) - Pass types and culling options
- [Resources](resources.md) - Essential resource flags
- [Execution](execution.md) - Building and caching
- [Frame Graph API](../../api-reference/frame-graph.md) - Function reference
