# Execution

This page covers building, caching, and executing frame graphs.

## Building Frame Graphs

### Basic Flow

```cpp
//1. Acquire a swapchain image
if(zest_BeginFrame(context) {
	// 2. Begin building
	if (zest_BeginFrameGraph(context, "My Graph", &cache_key)) {
		// 3. Import/create resources
		zest_ImportSwapchainResource();

		// 4. Define passes
		zest_BeginRenderPass("Main"); {
			zest_ConnectSwapChainOutput();
			zest_SetPassTask(RenderCallback, app);
			zest_EndPass();
		}

		// 5. Compile
		frame_graph = zest_EndFrameGraph();
	}

	// 6. Execute the frame graph and present to the screen
	zest_EndFrame(context, frame_graph);
}
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

Returns `zest_true` if the frame graph successfully initialised. If there's any problems then it will assert here.

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

You can use this to perform a compute dispatch or render something to an image as part of an initialisation process. An alternative would be to use zest_imm_* commands which do something similar, but the advantage of using a frame graph is that it automatically take care of barriers and resource transitions if required.

## Frame Graph Caching

Compiling frame graphs has CPU overhead. Cache them whenever possible to increase performance.

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

// Graph is automatically executed during zest_EndFrame()
```

### Cache Invalidation

Clear all cached graphs when the render configuration changes:

```cpp
// On window resize or major state change
zest_FlushCachedFrameGraphs(context);
```

## Execution

Frame graphs are automatically executed when you call `zest_EndFrame()`. The compiled graph from `zest_EndFrameGraph()` is submitted to the GPU, and its final output is presented to the swapchain.

### Synchronization

Wait for a frame graph timeline signal:

```cpp
zest_semaphore_status status = zest_WaitForSignal(timeline, timeout_microseconds);
```

Returns:
- `zest_semaphore_status_success` - Signal received
- `zest_semaphore_status_timeout` - Timeout elapsed
- `zest_semaphore_status_error` - Error occurred

You can use this to signal a timeline in a frame graph and then wait on the signal outside to ensure that it's finished.

```cpp
zest_BeginFrameGraph(...);
...
zest_SignalTimeline(timeline)
zest_EndFrameGraphAndExecute();

zest_semaphore_status status = zest_WaitForSignal(timeline, timeout_microseconds);
```

## Execution Flow

The typical frame loop:

```cpp
while (running) {
    zest_UpdateDevice(device);

    // Start frame
    zest_BeginFrame(context);

    // Build or retrieve cached graph
    zest_frame_graph_cache_key_t key = zest_InitialiseCacheKey(context, NULL, 0);
    zest_frame_graph graph = zest_GetCachedFrameGraph(context, &key);

    if (!graph) {
        if (zest_BeginFrameGraph(context, "Main", &key)) {
            // Setup resources and define passes...
            graph = zest_EndFrameGraph();
        }
    }

    // End frame (executes the graph, presents to swapchain)
    zest_EndFrame(context);
}
```

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

// Graph executes automatically during zest_EndFrame()
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

### State changes

For any major state changes in your application you can flush the whole cache with

```cpp
zest_FlushCachedFrameGraphs(context);
// Graphs will be rebuilt on next frame
}
```

## Best Practices

1. **Cache whenever possible** - Static graphs should always be cached
2. **Use cache keys for variants** - Different render modes get different cache keys
3. **Flush on major changes** - Window resize, render mode switch
4. **Check compilation results** - During development, verify graph builds correctly

## See Also

- [Passes](passes.md) - Pass types and callbacks
- [Resources](resources.md) - Resource management
- [Debugging](debugging.md) - Debug output and optimization
- [Frame Graph API](../../api-reference/frame-graph.md) - Function reference
