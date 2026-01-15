# Execution

This page covers building, caching, and executing frame graphs.

## Building Frame Graphs

### Basic Flow

```cpp
// 1. Begin building
if (zest_BeginFrameGraph(context, "My Graph", &cache_key)) {
    // 2. Import/create resources
    zest_ImportSwapchainResource();

    // 3. Define passes
    zest_BeginRenderPass("Main"); {
        zest_ConnectSwapChainOutput();
        zest_SetPassTask(RenderCallback, app);
        zest_EndPass();
    }

    // 4. Compile
    frame_graph = zest_EndFrameGraph();
}

// 5. Execute
zest_QueueFrameGraphForExecution(context, frame_graph);
```

### zest_BeginFrameGraph

Starts frame graph construction:

```cpp
zest_bool zest_BeginFrameGraph(
    zest_context context,      // The context to build for
    const char *name,          // Debug name for the graph
    zest_frame_graph_cache_key_t *cache_key  // Optional cache key
);
```

Returns `zest_true` if building should proceed, `zest_false` if a cached graph was found.

### zest_EndFrameGraph

Compiles the frame graph:

```cpp
zest_frame_graph zest_EndFrameGraph();
```

The compiler:
- Determines pass execution order
- Inserts memory barriers
- Allocates transient resources
- Culls unused passes

### zest_EndFrameGraphAndExecute

Shorthand for compile and immediate execution:

```cpp
zest_EndFrameGraphAndExecute();
```

Equivalent to:
```cpp
zest_frame_graph graph = zest_EndFrameGraph();
zest_QueueFrameGraphForExecution(context, graph);
```

## Frame Graph Caching

Compiling frame graphs has CPU overhead. Cache them when the structure is static.

### Creating Cache Keys

```cpp
// Simple cache key (context + swapchain state)
zest_frame_graph_cache_key_t key = zest_InitialiseCacheKey(context, NULL, 0);

// Cache key with custom data (for multiple configurations)
struct { int render_mode; zest_bool shadows_enabled; } config = {1, zest_true};
zest_frame_graph_cache_key_t key = zest_InitialiseCacheKey(
    context, &config, sizeof(config));
```

The cache key incorporates:
- Context state (swapchain format, size)
- Optional user-provided data

### Using Cached Graphs

```cpp
zest_frame_graph_cache_key_t key = zest_InitialiseCacheKey(context, NULL, 0);

// Try to get cached graph
zest_frame_graph graph = zest_GetCachedFrameGraph(context, &key);

if (!graph) {
    // Build new graph (only happens once)
    if (zest_BeginFrameGraph(context, "Graph", &key)) {
        // ... define passes ...
        graph = zest_EndFrameGraph();
    }
}

// Execute cached or newly built graph
zest_QueueFrameGraphForExecution(context, graph);
```

### Cache Invalidation

Clear all cached graphs when the render configuration changes:

```cpp
// On window resize or major state change
zest_FlushCachedFrameGraphs(context);
```

## Execution

### Queuing for Execution

```cpp
zest_QueueFrameGraphForExecution(context, frame_graph);
```

This queues the frame graph to execute during the next `zest_EndFrame()` call. Multiple frame graphs can be queued.

### Synchronization

Wait for a frame graph timeline signal:

```cpp
zest_semaphore_status status = zest_WaitForSignal(timeline, timeout_microseconds);
```

Returns:
- `zest_semaphore_status_success` - Signal received
- `zest_semaphore_status_timeout` - Timeout elapsed
- `zest_semaphore_status_error` - Error occurred

## Execution Flow

The typical frame loop:

```cpp
while (running) {
    // Start frame
    zest_StartFrame(device);
    zest_BeginFrame(context);

    // Build or retrieve cached graph
    zest_frame_graph_cache_key_t key = zest_InitialiseCacheKey(context, NULL, 0);
    zest_frame_graph graph = zest_GetCachedFrameGraph(context, &key);

    if (!graph) {
        if (zest_BeginFrameGraph(context, "Main", &key)) {
            // Define passes...
            graph = zest_EndFrameGraph();
        }
    }

    // Queue execution
    zest_QueueFrameGraphForExecution(context, graph);

    // End frame (executes queued graphs, presents)
    zest_EndFrame(context);
}
```

## Multiple Frame Graphs

You can execute multiple frame graphs per frame:

```cpp
// Shadow pass graph
zest_QueueFrameGraphForExecution(context, shadow_graph);

// Main render graph
zest_QueueFrameGraphForExecution(context, main_graph);

// Post-process graph
zest_QueueFrameGraphForExecution(context, post_graph);
```

Graphs execute in queue order with proper synchronization.

## Caching Patterns

### Static Graph

For applications with fixed rendering structure:

```cpp
// Initialize once
static zest_frame_graph cached_graph = NULL;
static zest_frame_graph_cache_key_t cache_key;

if (!cached_graph) {
    cache_key = zest_InitialiseCacheKey(context, NULL, 0);
    if (zest_BeginFrameGraph(context, "Static", &cache_key)) {
        // Define passes...
        cached_graph = zest_EndFrameGraph();
    }
}

zest_QueueFrameGraphForExecution(context, cached_graph);
```

### Dynamic Configuration

For applications with multiple render modes:

```cpp
typedef enum {
    RENDER_MODE_FORWARD,
    RENDER_MODE_DEFERRED,
    RENDER_MODE_COUNT
} render_mode;

// Cache key includes render mode
struct { render_mode mode; } config = { current_mode };
zest_frame_graph_cache_key_t key = zest_InitialiseCacheKey(context, &config, sizeof(config));

zest_frame_graph graph = zest_GetCachedFrameGraph(context, &key);
if (!graph) {
    if (zest_BeginFrameGraph(context, "Dynamic", &key)) {
        if (current_mode == RENDER_MODE_FORWARD) {
            // Forward rendering passes...
        } else {
            // Deferred rendering passes...
        }
        graph = zest_EndFrameGraph();
    }
}
```

### Window Resize Handling

```cpp
void OnWindowResize(int width, int height) {
    // Invalidate cached graphs
    zest_FlushCachedFrameGraphs(context);

    // Resize swapchain
    zest_RecreateSwapchain(context);

    // Graphs will be rebuilt on next frame
}
```

## Best Practices

1. **Cache whenever possible** - Static graphs should always be cached
2. **Use cache keys for variants** - Different render modes get different cache keys
3. **Flush on major changes** - Window resize, render mode switch
4. **Queue multiple graphs** - Separate concerns (shadows, main, post)
5. **Check compilation results** - During development, verify graph builds correctly

## See Also

- [Passes](passes.md) - Pass types and callbacks
- [Resources](resources.md) - Resource management
- [Debugging](debugging.md) - Debug output and optimization
- [Frame Graph API](../../api-reference/frame-graph.md) - Function reference
