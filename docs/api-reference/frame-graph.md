# Frame Graph API

Functions for building and executing frame graphs. The frame graph is the core execution model in Zest - it automatically handles resource barriers, synchronization, pass culling, and memory management.

## Graph Building

### zest_BeginFrameGraph

Start building a new frame graph. This begins the recording phase where you define passes and their resource dependencies by connecting resources as inputs and outputs.

```cpp
zest_bool zest_BeginFrameGraph(
    zest_context context,
    const char *name,
    zest_frame_graph_cache_key_t *cache_key
);
```

| Parameter | Description |
|-----------|-------------|
| `context` | The context to build for |
| `name` | Debug name for the graph (shown in profilers/debuggers) |
| `cache_key` | Optional cache key (pass `NULL` or `0` to disable caching) |

Returns `ZEST_TRUE` if building should proceed, `ZEST_FALSE` if a cached graph was found.

**Typical usage:** Called at the start of each frame to begin defining your render pipeline. When using caching, check if a cached graph exists first to avoid redundant compilation.

```cpp
zest_frame_graph_cache_key_t cache_key = zest_InitialiseCacheKey(context, 0, 0);
zest_frame_graph frame_graph = zest_GetCachedFrameGraph(context, &cache_key);
if (!frame_graph) {
    if (zest_BeginFrameGraph(context, "Main Render", &cache_key)) {
        // Define passes and resources here...
        frame_graph = zest_EndFrameGraph();
    }
}
```

---

### zest_EndFrameGraph

Compile and finalize the frame graph. The compiler analyzes dependencies, culls unused passes/resources, determines execution order, and generates synchronization barriers.

```cpp
zest_frame_graph zest_EndFrameGraph();
```

Returns the compiled frame graph ready for execution.

**Typical usage:** Called after all passes and resources have been defined. The returned graph can be cached and re-executed across multiple frames.

```cpp
if (zest_BeginFrameGraph(context, "My Graph", &cache_key)) {
    // ... define passes ...
    zest_frame_graph graph = zest_EndFrameGraph();
    zest_QueueFrameGraphForExecution(context, graph);
}
```

---

### zest_EndFrameGraphAndExecute

Compile and immediately submit the frame graph for GPU execution. Combines `zest_EndFrameGraph()` and execution into a single call.

```cpp
zest_frame_graph zest_EndFrameGraphAndExecute();
```

Returns the compiled frame graph.

**Typical usage:** Useful for off-screen rendering or compute work that doesn't need to be presented to a swapchain. Often paired with `zest_WaitForSignal()` to synchronize results.

```cpp
zest_execution_timeline timeline = zest_CreateExecutionTimeline(device);
if (zest_BeginFrameGraph(context, "Compute Work", 0)) {
    // Define compute passes...
    zest_SignalTimeline(timeline);
    zest_frame_graph graph = zest_EndFrameGraphAndExecute();
    zest_WaitForSignal(timeline, ZEST_SECONDS_IN_MICROSECONDS(1));
    // Results are now available
}
```

---

## Caching

### zest_InitialiseCacheKey

Create a cache key that uniquely identifies a frame graph configuration. The key incorporates swapchain state (dimensions, format) plus optional user-defined state.

```cpp
zest_frame_graph_cache_key_t zest_InitialiseCacheKey(
    zest_context context,
    const void *user_state,
    zest_size user_state_size
);
```

| Parameter | Description |
|-----------|-------------|
| `context` | The context (incorporates swapchain state) |
| `user_state` | Optional custom data to include in key (e.g., render settings) |
| `user_state_size` | Size of custom data in bytes |

**Typical usage:** Use custom state when your frame graph structure changes based on runtime settings (e.g., enabling/disabling post-processing effects).

```cpp
// Simple key based only on swapchain state
zest_frame_graph_cache_key_t key = zest_InitialiseCacheKey(context, 0, 0);

// Key with custom state
struct RenderSettings { int quality_level; bool enable_shadows; };
RenderSettings settings = {2, true};
zest_frame_graph_cache_key_t key = zest_InitialiseCacheKey(context, &settings, sizeof(settings));
```

---

### zest_GetCachedFrameGraph

Retrieve a previously compiled frame graph from the cache. Returns the cached graph if found, allowing you to skip the compilation step.

```cpp
zest_frame_graph zest_GetCachedFrameGraph(
    zest_context context,
    zest_frame_graph_cache_key_t *cache_key
);
```

Returns the cached frame graph or `NULL` if not found.

**Typical usage:** Check for a cached graph before beginning a new one. Although most frame graphs compile very quickly this still helps reduce CPU overhead and is highly recommended.

```cpp
zest_frame_graph_cache_key_t cache_key = zest_InitialiseCacheKey(context, 0, 0);
zest_frame_graph frame_graph = zest_GetCachedFrameGraph(context, &cache_key);
if (!frame_graph) {
    // No cache hit - build the graph
    if (zest_BeginFrameGraph(context, "Render", &cache_key)) {
        // ... define passes ...
        frame_graph = zest_EndFrameGraph();	// <-- Frame graph get's cached here for the next frame(s)
    }
}
zest_QueueFrameGraphForExecution(context, frame_graph);
```

---

### zest_FlushCachedFrameGraphs

Clear all cached frame graphs for a context. Call this when major state changes require rebuilding all graphs (e.g., window resize, quality preset change etc.).

```cpp
void zest_FlushCachedFrameGraphs(zest_context context);
```

**Typical usage:** Called in response to events that invalidate all cached graphs.

---

## Passes

### zest_BeginRenderPass

Start defining a render pass for graphics operations (drawing geometry, clearing render targets). 

```cpp
zest_pass_node zest_BeginRenderPass(const char *name);
```

Returns a pass node handle that can be used for debugging or advanced queries.

**Typical usage:** Use for any pass that draws geometry or clears color/depth attachments.

```cpp
zest_BeginRenderPass("Shadow Map");
zest_ConnectOutput(shadow_depth_resource);
zest_SetPassTask(RenderShadowMap, &shadow_data);
zest_EndPass();

zest_BeginRenderPass("Main Scene");
zest_ConnectInput(shadow_depth_resource);
zest_ConnectSwapChainOutput();
zest_SetPassTask(RenderMainScene, &scene_data);
zest_EndPass();
```

---

### zest_BeginComputePass

Start defining a compute pass for GPU compute shader dispatch.

```cpp
zest_pass_node zest_BeginComputePass(zest_compute compute, const char *name);
```

| Parameter | Description |
|-----------|-------------|
| `compute` | The compute object containing the compute pipeline |
| `name` | Debug name for the pass |

**Typical usage:** Use for image processing, particle simulation, physics, or any parallel GPU work.

```cpp
// Blur pass using compute shader
zest_compute blur_compute = zest_GetCompute(blur_compute_handle);
zest_BeginComputePass(blur_compute, "Gaussian Blur");
zest_ConnectInput(input_image);
zest_ConnectOutput(blurred_image);
zest_SetPassTask(DispatchBlurShader, &blur_params);
zest_EndPass();
```

---

### zest_BeginTransferPass

Start defining a transfer pass for data uploads, downloads, and copy operations.

```cpp
zest_pass_node zest_BeginTransferPass(const char *name);
```

**Typical usage:** Use for staging buffer uploads, buffer-to-buffer copies, or readback operations.

```cpp
zest_BeginTransferPass("Upload Vertex Data");
zest_ConnectOutput(vertex_buffer_resource);
zest_SetPassTask(UploadVertexData, vertex_data);
zest_EndPass();
```

---

### zest_EndPass

Finalize the current pass definition. Must be called after each `zest_BeginRenderPass`, `zest_BeginComputePass`, or `zest_BeginTransferPass`.

```cpp
void zest_EndPass();
```

**Typical usage:** Always pair with a corresponding `zest_Begin*Pass()` call.

```cpp
zest_BeginRenderPass("My Pass");
zest_ConnectSwapChainOutput();
zest_SetPassTask(MyRenderCallback, user_data);
zest_EndPass();  // Don't forget this!
```

---

### zest_SetPassTask

Set the callback function that will be executed when this pass runs. The callback records GPU commands for the pass.

```cpp
void zest_SetPassTask(zest_fg_execution_callback callback, void *user_data);
```

| Parameter | Description |
|-----------|-------------|
| `callback` | Function pointer with signature `void (*)(const zest_command_list, void*)` |
| `user_data` | User data passed to callback during execution |

The user data must have the appropriate lifetime. Don't use a local variable or it will be out of scope by the time the callback is called.

**Typical usage:** Set a callback that records draw calls, compute dispatches, or transfer commands.

```cpp
void DrawScene(const zest_command_list cmd_list, void *user_data) {
    SceneData *scene = (SceneData*)user_data;
    zest_pipeline pipeline = zest_PipelineWithTemplate(scene->pipeline_template, cmd_list);
    zest_cmd_BindPipeline(cmd_list, pipeline);
    zest_cmd_DrawIndexed(cmd_list, scene->index_count, 1, 0, 0, 0);
}

zest_BeginRenderPass("Scene");
zest_ConnectSwapChainOutput();
zest_SetPassTask(DrawScene, &scene_data);
zest_EndPass();
```

---

### zest_EmptyRenderPass

Built-in callback that records no commands. Useful for passes that only need to clear attachments.

```cpp
void zest_EmptyRenderPass(const zest_command_list command_list, void *user_data);
```

**Typical usage:** Use for clear-only passes or placeholder passes during development.

```cpp
// Pass that only clears the swapchain to a color
zest_BeginRenderPass("Clear Background");
zest_ConnectSwapChainOutput();
zest_SetPassTask(zest_EmptyRenderPass, NULL);
zest_EndPass();
```

---

## Resource Connections

### zest_ConnectInput

Declare that the current pass reads from a resource. This establishes a dependency - the pass will wait for any previous writes to complete.

```cpp
void zest_ConnectInput(zest_resource_node resource);
```

**Typical usage:** Connect textures, buffers, or images that your pass will sample or read from.

```cpp
zest_resource_node shadow_map = zest_AddTransientImageResource("Shadow Map", &depth_info);

// First pass writes the shadow map
zest_BeginRenderPass("Shadow Pass");
zest_ConnectOutput(shadow_map);
zest_SetPassTask(RenderShadows, NULL);
zest_EndPass();

// Second pass reads the shadow map
zest_BeginRenderPass("Lighting Pass");
zest_ConnectInput(shadow_map);  // Creates dependency on Shadow Pass
zest_ConnectSwapChainOutput();
zest_SetPassTask(RenderLighting, NULL);
zest_EndPass();
```

---

### zest_ConnectOutput

Declare that the current pass writes to a resource. The resource will be used as a render target or storage target.

```cpp
void zest_ConnectOutput(zest_resource_node resource);
```

**Typical usage:** Connect render targets for graphics passes or output buffers for compute passes.

```cpp
zest_resource_node gbuffer_albedo = zest_AddTransientImageResource("GBuffer Albedo", &color_info);
zest_resource_node gbuffer_normal = zest_AddTransientImageResource("GBuffer Normal", &normal_info);

zest_BeginRenderPass("GBuffer Pass");
zest_ConnectOutput(gbuffer_albedo);
zest_ConnectOutput(gbuffer_normal);
zest_SetPassTask(RenderGBuffer, NULL);
zest_EndPass();
```

---

### zest_ConnectSwapChainOutput

Declare that the current pass writes to the swapchain (the window surface). This marks the pass as essential and ensures the swapchain image is acquired.

```cpp
void zest_ConnectSwapChainOutput();
```

**Typical usage:** Use for the final pass(es) in your frame graph that produce visible output.

```cpp
zest_BeginRenderPass("Final Composite");
zest_ConnectInput(post_processed_image);
zest_ConnectSwapChainOutput();  // Output goes to screen
zest_SetPassTask(BlitToScreen, NULL);
zest_EndPass();
```

---

### zest_ConnectInputGroup

Connect multiple resources as inputs using a resource group. Convenience function for passes that read from many resources.

```cpp
void zest_ConnectInputGroup(zest_resource_group group);
```

**Typical usage:** Use when a pass needs to read from a collection of related resources.

```cpp
zest_resource_group gbuffer = zest_CreateResourceGroup();
zest_AddResourceToGroup(gbuffer, albedo);
zest_AddResourceToGroup(gbuffer, normal);
zest_AddResourceToGroup(gbuffer, depth);

zest_BeginRenderPass("Deferred Lighting");
zest_ConnectInputGroup(gbuffer);  // Reads all G-buffer textures
zest_ConnectSwapChainOutput();
zest_SetPassTask(DeferredLighting, NULL);
zest_EndPass();
```

---

### zest_ConnectOutputGroup

Connect multiple resources as outputs using a resource group. Convenience function for MRT (multiple render target) rendering.

```cpp
void zest_ConnectOutputGroup(zest_resource_group group);
```

**Typical usage:** Use when a pass writes to multiple render targets simultaneously.

```cpp
zest_resource_group gbuffer = zest_CreateResourceGroup();
zest_AddResourceToGroup(gbuffer, albedo);
zest_AddResourceToGroup(gbuffer, normal);
zest_AddResourceToGroup(gbuffer, material);

zest_BeginRenderPass("GBuffer Fill");
zest_ConnectOutputGroup(gbuffer);  // Writes to all G-buffer targets
zest_SetPassTask(RenderGBuffer, NULL);
zest_EndPass();
```

---

## Resource Import

### zest_ImportSwapchainResource

Import the swapchain as a resource in the frame graph. Required for any graph that presents to the screen.

```cpp
zest_resource_node zest_ImportSwapchainResource();
```

Returns a resource node representing the swapchain.

**Typical usage:** Call once at the start of frame graph definition, before any passes that output to the swapchain.

```cpp
if (zest_BeginFrameGraph(context, "Main", &cache_key)) {
    zest_ImportSwapchainResource();  // Always import first

    zest_BeginRenderPass("Draw");
    zest_ConnectSwapChainOutput();
    zest_SetPassTask(DrawCallback, NULL);
    zest_EndPass();

    frame_graph = zest_EndFrameGraph();
}
```

---

### zest_ImportImageResource

Import an existing image (texture, render target) into the frame graph. Use this for persistent resources that outlive a single frame.

**Important Note** You don't have to import all the images that you want to use in a frame graph. You only need to import images that you're going to write and read from and therefore require synconization and layout changes that the frame will take care of. Otherwise it's a waste of CPU time, you can just simply pass around the descriptor indexes inside the render/compute pass callbacks to use them in your shaders.


```cpp
zest_resource_node zest_ImportImageResource(
    const char *name,
    zest_image image,
    zest_resource_image_provider provider
);
```

| Parameter | Description |
|-----------|-------------|
| `name` | Debug name for the resource |
| `image` | The image to import |
| `provider` | A zest_resource_image_provider callback function (optional) |

**Typical usage:** Import persistent textures, pre-loaded assets, or images from previous frames.

The image provider callback is used to fetch the correct image view of the image in some circumstances. For example the swapchain has a built in provider to get the correct view of the currently acquired swapchain image. This is needed for when the swapchain is cached and the original image that was imported is now stale.

```cpp
// Import a persistent texture for sampling
zest_image diffuse_tex = zest_GetImage(my_texture_handle);
zest_resource_node diffuse = zest_ImportImageResource("Diffuse", diffuse_tex, zest_texture_2d_binding);

zest_BeginRenderPass("Draw Textured");
zest_ConnectInput(diffuse);
zest_ConnectSwapChainOutput();
zest_SetPassTask(DrawTextured, NULL);
zest_EndPass();
```

---

### zest_ImportBufferResource

Import an existing buffer into the frame graph. Use for persistent buffers or CPU-accessible readback buffers.

**Important Note** You don't have to import all the buffers that you want to use in a frame graph. You only need to import buffers that you're going to write and read from and therefore require synconization that the frame will take care of. Otherwise it's a waste of CPU time, you can just simply pass around the descriptor indexes for the buffers inside the render/compute pass callbacks to use them in your shaders.

```cpp
zest_resource_node zest_ImportBufferResource(
    const char *name,
    zest_buffer buffer,
    zest_resource_buffer_provider provider
);
```

| Parameter | Description |
|-----------|-------------|
| `name` | Debug name for the resource |
| `buffer` | The buffer to import |
| `provider` | A zest_resource_buffer_provider callback (optional) |

**Typical usage:** Import vertex buffers, index buffers, or GPU-to-CPU readback buffers.

```cpp
// Import a buffer for compute results that can be read back
zest_buffer results_buffer = zest_CreateBuffer(device, size, &readback_info);
zest_resource_node results = zest_ImportBufferResource("Results", results_buffer, 0);

zest_BeginComputePass(compute, "Process");
zest_ConnectOutput(results);
zest_SetPassTask(ProcessData, NULL);
zest_EndPass();

// After execution, read from results_buffer
```

---

## Transient Resources

### zest_AddTransientImageResource

Create an image resource with frame-lifetime. The memory is automatically allocated when needed and can be aliased with other transient resources to reduce memory usage.

```cpp
zest_resource_node zest_AddTransientImageResource(
    const char *name,
    zest_image_resource_info_t *info
);
```

**Typical usage:** Use for intermediate render targets, G-buffer textures, post-processing buffers, and any image that doesn't need to persist across frames.

```cpp
zest_image_resource_info_t color_info = {zest_format_r8g8b8a8_unorm};
zest_image_resource_info_t depth_info = {zest_format_depth};

zest_resource_node color_buffer = zest_AddTransientImageResource("Color", &color_info);
zest_resource_node depth_buffer = zest_AddTransientImageResource("Depth", &depth_info);

zest_BeginRenderPass("Offscreen Render");
zest_ConnectOutput(color_buffer);
zest_ConnectOutput(depth_buffer);
zest_SetPassTask(RenderScene, NULL);
zest_EndPass();
```

---

### zest_AddTransientBufferResource

Create a buffer resource with frame-lifetime. Memory is automatically managed and can be aliased with other transient resources.

```cpp
zest_resource_node zest_AddTransientBufferResource(
    const char *name,
    const zest_buffer_resource_info_t *info
);
```

**Typical usage:** Use for temporary compute buffers, staging data, or any buffer that doesn't persist across frames.

```cpp
zest_buffer_resource_info_t buffer_info = {};
buffer_info.size = sizeof(ParticleData) * particle_count;

zest_resource_node particle_buffer = zest_AddTransientBufferResource("Particles", &buffer_info);

zest_BeginComputePass(compute, "Simulate Particles");
zest_ConnectOutput(particle_buffer);
zest_SetPassTask(SimulateParticles, NULL);
zest_EndPass();
```

---

### zest_AddTransientLayerResource

Create a transient resource from a layer's buffer. Layers manage instanced/mesh rendering and this allows their buffers to participate in the frame graph.

```cpp
zest_resource_node zest_AddTransientLayerResource(
    const char *name,
    const zest_layer layer,
    zest_bool prev_fif
);
```

| Parameter | Description |
|-----------|-------------|
| `name` | Debug name for the resource |
| `layer` | The layer to use |
| `prev_fif` | If `ZEST_TRUE`, use previous frame-in-flight's buffer (for temporal effects) |

**Typical usage:** Use when compute shaders need to process or generate layer data.

```cpp
zest_resource_node sprite_buffer = zest_AddTransientLayerResource("Sprites", sprite_layer, ZEST_FALSE);

zest_BeginComputePass(compute, "Update Sprites");
zest_ConnectOutput(sprite_buffer);
zest_SetPassTask(UpdateSpritePositions, NULL);
zest_EndPass();
```

---

## Resource Properties

### zest_GetResourceWidth

Get the width in pixels of an image resource.

```cpp
zest_uint zest_GetResourceWidth(zest_resource_node resource);
```

**Typical usage:** Query resource dimensions for dispatch calculations or viewport setup.

```cpp
void BlurCallback(const zest_command_list cmd_list, void *user_data) {
    zest_resource_node target = zest_GetPassOutputResource(cmd_list, "Target");
    zest_uint width = zest_GetResourceWidth(target);
    zest_uint height = zest_GetResourceHeight(target);
    zest_cmd_DispatchCompute(cmd_list, (width + 7) / 8, (height + 7) / 8, 1);
}
```

---

### zest_GetResourceHeight

Get the height in pixels of an image resource.

```cpp
zest_uint zest_GetResourceHeight(zest_resource_node resource);
```

**Typical usage:** Used alongside `zest_GetResourceWidth()` for computing dispatch sizes or viewports.

---

### zest_GetResourceMipLevels

Get the number of mip levels in an image resource.

```cpp
zest_uint zest_GetResourceMipLevels(zest_resource_node resource);
```

**Typical usage:** Use when generating mipmaps or accessing specific mip levels in compute shaders.

```cpp
zest_uint mip_count = zest_GetResourceMipLevels(hdr_texture);
for (zest_uint mip = 1; mip < mip_count; mip++) {
    // Process each mip level
}
```

---

### zest_GetResourceType

Get the type of a resource (image, buffer, etc.).

```cpp
zest_resource_type zest_GetResourceType(zest_resource_node resource_node);
```

**Typical usage:** Use for generic code that handles different resource types.

---

### zest_GetResourceImage

Get the underlying `zest_image` from an image resource node. Useful when you need access to the actual image pointer.

```cpp
zest_image zest_GetResourceImage(zest_resource_node resource_node);
```

**Typical usage:** Access the image for operations that require the image pointer directly.

---

### zest_GetResourceImageDescription

Get the full image description including format, extent, mip levels, and other properties.

```cpp
zest_image_info_t zest_GetResourceImageDescription(zest_resource_node resource_node);
```

**Typical usage:** Query detailed image properties for configuring compute dispatches or rendering.

```cpp
void ComputeCallback(const zest_command_list cmd_list, void *user_data) {
    zest_resource_node output = zest_GetPassOutputResource(cmd_list, "Output");
    zest_image_info_t desc = zest_GetResourceImageDescription(output);
    zest_cmd_DispatchCompute(cmd_list,
        (desc.extent.width + 7) / 8,
        (desc.extent.height + 7) / 8,
        1);
}
```

---

### zest_SetResourceBufferSize

Set or update the size of a buffer resource. Typically you set the size of a transient resource buffer using the info struct you pass into zest_AddTransientBufferResource but you could use this inside a resource provider callback function if you don't know the size of the buffer until execution time.

```cpp
void zest_SetResourceBufferSize(zest_resource_node resource, zest_size size);
```

**Typical usage:** Dynamically size buffers based on runtime requirements.

```cpp
zest_SetResourceBufferSize(particles, sizeof(Particle) * active_particle_count);
```

---

### zest_SetResourceClearColor

Set the clear color for a render target resource. This color is used when the attachment is cleared at pass start.

```cpp
void zest_SetResourceClearColor(
    zest_resource_node resource,
    float red, float green, float blue, float alpha
);
```

**Typical usage:** Set clear colors for intermediate render targets.

```cpp
zest_resource_node hdr_buffer = zest_AddTransientImageResource("HDR", &hdr_info);
zest_SetResourceClearColor(hdr_buffer, 0.0f, 0.0f, 0.0f, 1.0f);

zest_BeginRenderPass("HDR Scene");
zest_ConnectOutput(hdr_buffer);
zest_SetPassTask(RenderHDR, NULL);  // Starts with black clear
zest_EndPass();
```

---

### zest_GetResourceUserData

Get custom user data attached to a resource.

```cpp
void *zest_GetResourceUserData(zest_resource_node resource_node);
```

**Typical usage:** Retrieve context-specific data associated with resources.

---

### zest_SetResourceUserData

Attach custom user data to a resource. Useful for associating application-specific metadata with resources.

```cpp
void zest_SetResourceUserData(zest_resource_node resource_node, void *user_data);
```

**Typical usage:** Store per-resource metadata like debug info or application context.

---

### zest_SetResourceBufferProvider

Allows you to set a resource provider callback to a transient resource if you need to make changes to a buffer at execution time.

```cpp
void zest_SetResourceBufferProvider(
    zest_resource_node resource_node,
    zest_resource_buffer_provider buffer_provider
);
```

**Typical usage:** Change buffer size when the graph is executed but before the transient resource is created.

---

### zest_SetResourceImageProvider

Allows you to set a resource provider callback to a transient resource if you need to make changes to an image at execution time.

```cpp
void zest_SetResourceImageProvider(
    zest_resource_node resource_node,
    zest_resource_image_provider image_provider
);
```

**Typical usage:** Change image size when the graph is executed but before the transient resource is created.

---

## Pass Resource Access

### zest_GetPassInputResource

Get an input resource by name within a pass callback. Use this to access resources you connected with `zest_ConnectInput()`.

```cpp
zest_resource_node zest_GetPassInputResource(
    const zest_command_list command_list,
    const char *name
);
```

**Typical usage:** Retrieve resources in your pass callback to get their bindless indices or properties.

```cpp
void LightingPass(const zest_command_list cmd_list, void *user_data) {
    zest_resource_node shadow_map = zest_GetPassInputResource(cmd_list, "Shadow Map");
    zest_uint shadow_index = zest_GetTransientSampledImageBindlessIndex(
        cmd_list, shadow_map, zest_texture_2d_binding);
    // Use shadow_index in push constants
}
```

---

### zest_GetPassOutputResource

Get an output resource by name within a pass callback. Use this to access resources you connected with `zest_ConnectOutput()`.

```cpp
zest_resource_node zest_GetPassOutputResource(
    const zest_command_list command_list,
    const char *name
);
```

**Typical usage:** Access output resources to get their bindless indices for compute shader writes.

```cpp
void ComputePass(const zest_command_list cmd_list, void *user_data) {
    zest_resource_node output = zest_GetPassOutputResource(cmd_list, "Output Buffer");
    zest_uint output_index = zest_GetTransientBufferBindlessIndex(cmd_list, output);
    // Use output_index in push constants for compute shader
}
```

---

### zest_GetPassInputBuffer

Get an input buffer by name within a pass callback. Convenience function that returns the buffer directly.

```cpp
zest_buffer zest_GetPassInputBuffer(
    const zest_command_list command_list,
    const char *name
);
```

**Typical usage:** Access input buffers when you need the buffer handle directly.

---

### zest_GetPassOutputBuffer

Get an output buffer by name within a pass callback. Convenience function that returns the buffer directly.

```cpp
zest_buffer zest_GetPassOutputBuffer(
    const zest_command_list command_list,
    const char *name
);
```

**Typical usage:** Access output buffers for copy operations or direct access.

---

## Bindless Descriptor Helpers

These functions will either acquire an index or get the index that was already acquired for the transient resource which might be the case if you're using the resource again in another pass.

Bindless indexes that are acquired for transient resources are automatically released in the next frame after they're done with.

### zest_GetTransientSampledImageBindlessIndex

Get the bindless descriptor array index for a transient sampled image. Use this index in shaders to access the texture.

```cpp
zest_uint zest_GetTransientSampledImageBindlessIndex(
    const zest_command_list command_list,
    zest_resource_node resource,
    zest_binding_number_type binding_number
);
```

**Typical usage:** Get the bindless index for passing to shaders via push constants.

```cpp
void RenderPass(const zest_command_list cmd_list, void *user_data) {
    zest_resource_node albedo = zest_GetPassInputResource(cmd_list, "Albedo");

    MyPushConstants push;
    push.albedo_index = zest_GetTransientSampledImageBindlessIndex(
        cmd_list, albedo, zest_texture_2d_binding);
    zest_cmd_SendPushConstants(cmd_list, &push, sizeof(push));
}
```

---

### zest_GetTransientSampledMipBindlessIndexes

Get bindless indices for all mip levels of a transient image. Returns an array with one index per mip level.

```cpp
zest_uint *zest_GetTransientSampledMipBindlessIndexes(
    const zest_command_list command_list,
    zest_resource_node resource,
    zest_binding_number_type binding_number
);
```

Returns array of indices, one per mip level.

**Typical usage:** Access specific mip levels in compute shaders for downsampling or mipmap generation.

```cpp
void GenerateMipmaps(const zest_command_list cmd_list, void *user_data) {
    zest_resource_node image = zest_GetPassOutputResource(cmd_list, "Image");
    zest_uint *mip_indices = zest_GetTransientSampledMipBindlessIndexes(
        cmd_list, image, zest_storage_image_binding);
    zest_uint mip_count = zest_GetResourceMipLevels(image);

    for (zest_uint i = 1; i < mip_count; i++) {
        // Dispatch downsample from mip_indices[i-1] to mip_indices[i]
    }
}
```

---

### zest_GetTransientBufferBindlessIndex

Get the bindless descriptor array index for a transient buffer.

```cpp
zest_uint zest_GetTransientBufferBindlessIndex(
    const zest_command_list command_list,
    zest_resource_node resource
);
```

**Typical usage:** Get the bindless index for buffers accessed in compute shaders.

```cpp
void ComputePass(const zest_command_list cmd_list, void *user_data) {
    zest_resource_node input = zest_GetPassInputResource(cmd_list, "Input Buffer");
    zest_resource_node output = zest_GetPassOutputResource(cmd_list, "Output Buffer");

    MyPushConstants push;
    push.input_buffer = zest_GetTransientBufferBindlessIndex(cmd_list, input);
    push.output_buffer = zest_GetTransientBufferBindlessIndex(cmd_list, output);
    zest_cmd_SendPushConstants(cmd_list, &push, sizeof(push));
}
```

---

## Resource Groups

### zest_CreateResourceGroup

Create an empty resource group for batching multiple resources together.

```cpp
zest_resource_group zest_CreateResourceGroup();
```

**Typical usage:** Create groups for G-buffers, multiple render targets, or related texture sets.

```cpp
zest_resource_group gbuffer = zest_CreateResourceGroup();
zest_AddResourceToGroup(gbuffer, albedo);
zest_AddResourceToGroup(gbuffer, normal);
zest_AddResourceToGroup(gbuffer, material);
```

---

### zest_AddResourceToGroup

Add a resource to a resource group.

```cpp
void zest_AddResourceToGroup(zest_resource_group group, zest_resource_node image);
```

**Typical usage:** Build up a group of related resources for batch connection.

```cpp
zest_resource_group attachments = zest_CreateResourceGroup();
zest_AddResourceToGroup(attachments, color_target);
zest_AddResourceToGroup(attachments, depth_target);

zest_BeginRenderPass("Main Render");
zest_ConnectOutputGroup(attachments);
zest_SetPassTask(RenderScene, NULL);
zest_EndPass();
```

---

### zest_AddSwapchainToGroup

Add the swapchain to a resource group.

```cpp
void zest_AddSwapchainToGroup(zest_resource_group group);
```

**Typical usage:** Include the swapchain in a group when combining it with other outputs.

```cpp
zest_resource_group outputs = zest_CreateResourceGroup();
zest_AddSwapchainToGroup(outputs);
zest_AddResourceToGroup(outputs, ui_overlay);
```

---

## Special Flags

### zest_FlagResourceAsEssential

Mark a resource to prevent it and its dependent passes from being culled, even if the resource isn't used as a final output.

```cpp
void zest_FlagResourceAsEssential(zest_resource_node resource);
```

**Typical usage:** Keep side-effect resources like depth buffers that don't directly connect to final output.

```cpp
zest_resource_node depth = zest_AddTransientImageResource("Depth", &depth_info);
zest_FlagResourceAsEssential(depth);  // Prevent culling

zest_BeginRenderPass("3D Scene");
zest_ConnectSwapChainOutput();
zest_ConnectOutput(depth);  // Depth needed for rendering but not as input elsewhere
zest_SetPassTask(Render3D, NULL);
zest_EndPass();
```

---

### zest_DoNotCull

Prevent the current pass from being culled during compilation, regardless of whether its outputs are used.

```cpp
void zest_DoNotCull();
```

**Typical usage:** Keep passes with important side effects that don't produce visible resource outputs.

```cpp
zest_BeginComputePass(compute, "Update Simulation");
zest_DoNotCull();  // Keep even if outputs aren't connected
zest_SetPassTask(UpdateSimulation, NULL);
zest_EndPass();
```

---

## Execution

### zest_QueueFrameGraphForExecution

Queue a compiled frame graph for execution during `zest_EndFrame()`. The graph will be executed and its final output presented to the swapchain.

```cpp
void zest_QueueFrameGraphForExecution(
    zest_context context,
    zest_frame_graph frame_graph
);
```

**Typical usage:** Queue the frame graph after building (or retrieving from cache) before calling `zest_EndFrame()`.

```cpp
if (zest_BeginFrame(context)) {
    zest_frame_graph graph = GetOrBuildFrameGraph(context);
    zest_QueueFrameGraphForExecution(context, graph);
    zest_EndFrame(context);  // Executes and presents
}
```

---

### zest_WaitForSignal

Wait for a timeline semaphore signal, blocking until the GPU work completes or timeout occurs.

```cpp
zest_semaphore_status zest_WaitForSignal(
    zest_execution_timeline timeline,
    zest_microsecs timeout
);
```

| Parameter | Description |
|-----------|-------------|
| `timeline` | The execution timeline to wait on |
| `timeout` | Timeout in microseconds |

Returns `zest_semaphore_status_success`, `zest_semaphore_status_timeout`, or `zest_semaphore_status_error`.

**Typical usage:** Synchronize with GPU work for readback or when results are needed immediately.

```cpp
zest_execution_timeline timeline = zest_CreateExecutionTimeline(device);

if (zest_BeginFrameGraph(context, "Compute Work", 0)) {
    // ... define compute passes ...
    zest_SignalTimeline(timeline);
    zest_EndFrameGraphAndExecute();
}

zest_semaphore_status status = zest_WaitForSignal(timeline, ZEST_SECONDS_IN_MICROSECONDS(1));
if (status == zest_semaphore_status_success) {
    // GPU work complete, safe to read results
    void *data = zest_BufferData(result_buffer);
}
```

---

## Debugging

### zest_PrintCompiledFrameGraph

Print the compiled frame graph structure to the console. Shows passes, resources, dependencies, and barriers for debugging.

**Note** You must `#define ZEST_TEST_MODE` before including `zest.h` in order to print frame graphs.

```cpp
void zest_PrintCompiledFrameGraph(zest_frame_graph frame_graph);
```

**Typical usage:** Debug frame graph compilation issues or understand the execution order.

```cpp
zest_frame_graph graph = zest_EndFrameGraph();
#ifdef _DEBUG
zest_PrintCompiledFrameGraph(graph);  // Print in debug builds
#endif
zest_QueueFrameGraphForExecution(context, graph);
```

---

### zest_GetFrameGraphResult

Get the compilation result status of a frame graph. Use to check for errors after compilation.

```cpp
zest_frame_graph_result zest_GetFrameGraphResult(zest_frame_graph frame_graph);
```

**Typical usage:** Validate that frame graph compilation succeeded.

```cpp
zest_frame_graph graph = zest_EndFrameGraph();
zest_frame_graph_result result = zest_GetFrameGraphResult(graph);
if (result != 0) {
    // Handle compilation error
    zest_PrintCompiledFrameGraph(graph);
}
```

---

## See Also

- [Frame Graph Concepts](../concepts/frame-graph/index.md)
- [Passes](../concepts/frame-graph/passes.md)
- [Resources](../concepts/frame-graph/resources.md)
- [Execution](../concepts/frame-graph/execution.md)
- [Debugging](../concepts/frame-graph/debugging.md)
- [Multi-Pass Tutorial](../tutorials/06-multipass.md)
