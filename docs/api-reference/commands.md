# Command API

Frame graph render commands (`zest_cmd_*` functions). These functions are called within frame graph pass callbacks to record rendering commands that are executed on the GPU.

## Pipeline Binding

### zest_cmd_BindPipeline

Bind a graphics pipeline for subsequent draw calls.

```cpp
void zest_cmd_BindPipeline(
    const zest_command_list command_list,
    zest_pipeline pipeline
);
```

**Description:** Binds a pre-built graphics pipeline to the command list. All draw commands after this call will use the specified pipeline's shader programs, render states, and vertex input configuration.

**Typical Usage:** Called at the start of a render pass before issuing draw commands, or when switching between different materials/shaders.

```cpp
void my_render_callback(zest_command_list cmd, void *user_data) {
	MyApp *app = (MyApp*)user_data;
	zest_pipeline my_pipeline = zest_PipelineWithTemplate(app->my_pipeline_template, cmd);
    zest_cmd_BindPipeline(cmd, my_pipeline);
    zest_cmd_SetScreenSizedViewport(cmd, 0.0f, 1.0f);
    // ... draw commands
}
```

---

### zest_cmd_BindComputePipeline

Bind a compute pipeline for compute dispatch calls.

```cpp
void zest_cmd_BindComputePipeline(
    const zest_command_list command_list,
    zest_compute compute
);
```

**Description:** Binds a compute pipeline to the command list. All subsequent `zest_cmd_DispatchCompute` calls will use this pipeline's compute shader.

**Typical Usage:** Called before dispatching compute work, such as particle simulation or image processing.

```cpp
void my_compute_callback(zest_command_list cmd, void *user_data) {
    zest_cmd_BindComputePipeline(cmd, particle_compute);
    zest_cmd_DispatchCompute(cmd, particle_count / 256, 1, 1);
}
```

---

## Viewport and Scissor

### zest_cmd_SetScreenSizedViewport

Set viewport and scissor to match the swapchain dimensions.

```cpp
void zest_cmd_SetScreenSizedViewport(
    const zest_command_list command_list,
    float min_depth,
    float max_depth
);
```

**Description:** Convenience function that sets both the viewport and scissor rectangle to cover the entire swapchain. The depth range is specified by `min_depth` and `max_depth` (typically 0.0 to 1.0).

**Typical Usage:** Most common viewport setup for full-screen rendering passes.

```cpp
void render_scene(zest_command_list cmd, void *user_data) {
	...
    zest_cmd_BindPipeline(cmd, scene_pipeline);
    zest_cmd_SetScreenSizedViewport(cmd, 0.0f, 1.0f);
    // Draw scene geometry...
}
```

---

### zest_cmd_ViewPort

Set a custom viewport.

```cpp
void zest_cmd_ViewPort(
    const zest_command_list command_list,
    zest_viewport_t *viewport
);
```

**Description:** Sets the viewport transformation that maps normalized device coordinates to framebuffer coordinates. Use `zest_CreateViewport()` to create the viewport struct.

**Typical Usage:** Split-screen rendering, picture-in-picture views, or rendering to a sub-region of the framebuffer.

```cpp
zest_viewport_t viewport = zest_CreateViewport(0, 0, 800, 600, 0.0f, 1.0f);
zest_cmd_ViewPort(cmd, &viewport);
```

---

### zest_cmd_Scissor

Set the scissor rectangle for pixel clipping.

```cpp
void zest_cmd_Scissor(
    const zest_command_list command_list,
    zest_scissor_rect_t *scissor
);
```

**Description:** Defines the rectangular region where pixels can be written. Pixels outside the scissor rectangle are discarded. Use `zest_CreateRect2D()` to create the scissor struct.

**Typical Usage:** UI clipping, rendering within masked regions, or optimizing rendering to visible areas only.

```cpp
zest_scissor_rect_t scissor = zest_CreateRect2D(400, 300, 100, 50);
zest_cmd_Scissor(cmd, &scissor);
```

---

### zest_cmd_LayerViewport

Set viewport and scissor from a layer's configuration.

```cpp
void zest_cmd_LayerViewport(
    const zest_command_list command_list,
    zest_layer layer
);
```

**Description:** Convenience function that sets both viewport and scissor from the layer's stored viewport and scissor settings. Useful when rendering layer content with custom viewports.

**Typical Usage:** Rendering layers with specific viewport configurations, such as UI panels or sub-windows.

---

### zest_cmd_Clip

Set viewport and scissor in a single call with explicit parameters.

```cpp
void zest_cmd_Clip(
    const zest_command_list command_list,
    float x,
    float y,
    float width,
    float height,
    float minDepth,
    float maxDepth
);
```

**Description:** Combined viewport and scissor command using explicit float parameters. Sets both the viewport transformation and scissor clipping to the same rectangular region.

**Typical Usage:** Quick setup for rendering to a specific screen region without creating separate viewport/scissor structs.

```cpp
// Render to the bottom-right quadrant
zest_cmd_Clip(cmd, 400, 300, 400, 300, 0.0f, 1.0f);
```

---

## Descriptor Sets

### zest_cmd_BindDescriptorSets

Bind descriptor sets for shader resource access.

```cpp
void zest_cmd_BindDescriptorSets(
    const zest_command_list command_list,
    zest_pipeline_bind_point bind_point,
    zest_pipeline_layout layout,
    zest_descriptor_set *sets,
    zest_uint set_count,
    zest_uint first_set
);
```

**Description:** Binds descriptor sets containing shader resources (textures, buffers, samplers). The global bindless descriptor set is bound automatically at the start of each command buffer and so you don't need to use this command; at some point there maybe the feature for custom descriptor sets but the global set should cover most things.

**Parameters:**
- `bind_point`: `zest_bind_point_graphics` or `zest_bind_point_compute`
- `layout`: The pipeline layout that matches the descriptor set layout
- `sets`: Array of descriptor sets to bind
- `set_count`: Number of sets in the array
- `first_set`: Starting set index in the pipeline layout

**Typical Usage:** Binding uniform buffer descriptor sets for per-object or per-frame data.

---

## Buffer Binding

### zest_cmd_BindVertexBuffer

Bind a vertex buffer for drawing.

```cpp
void zest_cmd_BindVertexBuffer(
    const zest_command_list command_list,
    zest_uint first_binding,
    zest_uint binding_count,
    zest_buffer buffer
);
```

**Description:** Binds a vertex buffer to one or more binding points for subsequent draw calls. The vertex data format is determined by the bound pipeline's vertex input description.

**Parameters:**
- `first_binding`: The first vertex input binding to update
- `binding_count`: Number of consecutive bindings to update
- `buffer`: The vertex buffer containing vertex data

**Typical Usage:** Binding custom vertex data for mesh rendering.

```cpp
zest_cmd_BindPipeline(cmd, my_pipeline);
zest_cmd_BindVertexBuffer(cmd, 0, 1, my_vertex_buffer);
zest_cmd_Draw(cmd, vertex_count, 1, 0, 0);
```

---

### zest_cmd_BindIndexBuffer

Bind an index buffer for indexed drawing.

```cpp
void zest_cmd_BindIndexBuffer(
    const zest_command_list command_list,
    zest_buffer buffer
);
```

**Description:** Binds an index buffer for subsequent indexed draw calls. The buffer should contain 32-bit unsigned integer indices.

**Typical Usage:** Binding custom index data for indexed mesh rendering.

```cpp
zest_cmd_BindVertexBuffer(cmd, 0, 1, my_vertex_buffer);
zest_cmd_BindIndexBuffer(cmd, my_index_buffer);
zest_cmd_DrawIndexed(cmd, index_count, 1, 0, 0, 0);
```

---

### zest_cmd_BindMeshVertexBuffer

Bind a layer's vertex buffer for mesh drawing.

```cpp
void zest_cmd_BindMeshVertexBuffer(
    const zest_command_list command_list,
    zest_layer layer
);
```

**Description:** Binds the vertex buffer associated with a layer. Used internally by the layer system and for custom layer rendering.

---

### zest_cmd_BindMeshIndexBuffer

Bind a layer's index buffer for mesh drawing.

```cpp
void zest_cmd_BindMeshIndexBuffer(
    const zest_command_list command_list,
    zest_layer layer
);
```

**Description:** Binds the index buffer associated with a layer. Used internally by the layer system and for custom layer rendering.

---

## Drawing

### zest_cmd_Draw

Issue a non-indexed draw call.

```cpp
void zest_cmd_Draw(
    const zest_command_list command_list,
    zest_uint vertex_count,
    zest_uint instance_count,
    zest_uint first_vertex,
    zest_uint first_instance
);
```

**Description:** Records a non-indexed draw command. Draws `vertex_count` vertices starting from `first_vertex`, repeated `instance_count` times for instanced rendering.

**Parameters:**
- `vertex_count`: Number of vertices to draw
- `instance_count`: Number of instances to draw (1 for non-instanced)
- `first_vertex`: Offset to the first vertex in the vertex buffer
- `first_instance`: Instance ID of the first instance (for `gl_InstanceIndex`)

**Typical Usage:** Drawing simple geometry without index buffers, such as full-screen triangles or particle quads.

```cpp
// Draw a full-screen triangle (3 vertices, generated in shader)
zest_cmd_BindPipeline(cmd, fullscreen_pipeline);
zest_cmd_Draw(cmd, 3, 1, 0, 0);
```

---

### zest_cmd_DrawIndexed

Issue an indexed draw call.

```cpp
void zest_cmd_DrawIndexed(
    const zest_command_list command_list,
    zest_uint index_count,
    zest_uint instance_count,
    zest_uint first_index,
    int32_t vertex_offset,
    zest_uint first_instance
);
```

**Description:** Records an indexed draw command. Reads `index_count` indices from the bound index buffer, using them to fetch vertices from the vertex buffer. Supports instancing and vertex offset.

**Parameters:**
- `index_count`: Number of indices to read
- `instance_count`: Number of instances to draw
- `first_index`: Offset into the index buffer
- `vertex_offset`: Value added to each index before fetching the vertex
- `first_instance`: Instance ID of the first instance

**Typical Usage:** Drawing meshes with shared vertices, such as 3D models or UI quads.

```cpp
zest_cmd_BindVertexBuffer(cmd, 0, 1, mesh_vertices);
zest_cmd_BindIndexBuffer(cmd, mesh_indices);
zest_cmd_DrawIndexed(cmd, mesh->index_count, 1, 0, 0, 0);
```

---

### zest_cmd_DrawLayerInstruction

Draw using a layer instruction struct.

```cpp
void zest_cmd_DrawLayerInstruction(
    const zest_command_list command_list,
    zest_uint vertex_count,
    zest_layer_instruction_t *instruction
);
```

**Description:** Specialized draw command that uses a `zest_layer_instruction_t` to configure the draw call. Used internally by the layer system for batched rendering.

**Typical Usage:** Used by Zest's internal layer rendering system; rarely called directly.

---

## Push Constants

### zest_cmd_SendPushConstants

Send push constant data to shaders.

```cpp
void zest_cmd_SendPushConstants(
    const zest_command_list command_list,
    void *data,
    zest_uint size
);
```

**Description:** Uploads push constant data to the currently bound pipeline. Push constants provide fast, small data updates without descriptor sets—ideal for per-draw data like bindless descriptor array indexes.

**Parameters:**
- `data`: Pointer to the push constant data (must match pipeline's push constant layout)
- `size`: Size of the data in bytes

**Typical Usage:** Sending per-object transform matrices or material IDs.

```cpp
struct PushConstants {
    float model_matrix[16];
    uint32_t texture_index;
};

PushConstants pc = { /* ... */ };
zest_cmd_SendPushConstants(cmd, &pc, sizeof(pc));
zest_cmd_Draw(cmd, vertex_count, 1, 0, 0);
```

---

## Depth Bias

### zest_cmd_SetDepthBias

Set depth bias parameters for subsequent draw calls.

```cpp
void zest_cmd_SetDepthBias(
    const zest_command_list command_list,
    float factor,
    float clamp,
    float slope
);
```

**Description:** Sets dynamic depth bias parameters when depth bias is enabled in the pipeline. Depth bias offsets the depth value to prevent z-fighting in techniques like shadow mapping or decals.

**Parameters:**
- `factor`: Constant depth offset value
- `clamp`: Maximum (or minimum) depth bias value
- `slope`: Slope-scaled depth bias factor

**Typical Usage:** Shadow map rendering to prevent shadow acne.

```cpp
zest_cmd_BindPipeline(cmd, shadow_pipeline);
zest_cmd_SetDepthBias(cmd, 1.0f, 0.0f, 1.5f);
// Draw shadow casters...
```

---

## Compute

### zest_cmd_DispatchCompute

Dispatch compute shader work groups.

```cpp
void zest_cmd_DispatchCompute(
    const zest_command_list command_list,
    zest_uint group_count_x,
    zest_uint group_count_y,
    zest_uint group_count_z
);
```

**Description:** Dispatches compute work with the specified number of work groups in each dimension. The total number of shader invocations is `group_count * local_size` (where `local_size` is defined in the compute shader).

**Typical Usage:** GPU-based particle simulation, image processing, or physics calculations.

```cpp
void compute_particles(zest_command_list cmd, void *user_data) {
    zest_cmd_BindComputePipeline(cmd, particle_update);
    // Dispatch one group per 256 particles (assuming local_size_x = 256)
    zest_uint groups = (particle_count + 255) / 256;
    zest_cmd_DispatchCompute(cmd, groups, 1, 1);
}
```

---

## Buffer Operations

### zest_cmd_CopyBuffer

Copy data between buffers.

```cpp
void zest_cmd_CopyBuffer(
    const zest_command_list command_list,
    zest_buffer src_buffer,
    zest_buffer dst_buffer,
    zest_size size
);
```

**Description:** Records a GPU-side buffer copy command. Copies `size` bytes from the source buffer to the destination buffer. Both buffers must have appropriate usage flags for transfer operations.

**Typical Usage:** Copying staging buffer data to device-local buffers, or copying between GPU buffers. Generally done during a frame graph transfer pass.

```cpp
zest_cmd_CopyBuffer(cmd, staging_buffer, device_buffer, data_size);
```

---

### zest_cmd_UploadBuffer

Upload multiple buffer regions using an uploader struct.

```cpp
zest_bool zest_cmd_UploadBuffer(
    const zest_command_list command_list,
    zest_buffer_uploader_t *uploader
);
```

**Description:** Records buffer copy commands for all regions registered with the uploader. Returns `ZEST_TRUE` if copies were recorded, `ZEST_FALSE` if the uploader had no pending copies. Use `zest_AddCopyCommand()` to add copy regions to the uploader.

**Typical Usage:** Batching multiple buffer uploads in a single command, used internally by the layer system for vertex/index data uploads.

```cpp
zest_buffer_uploader_t uploader = {0};
zest_AddCopyCommand(context, &uploader, staging, device, size1);
zest_AddCopyCommand(context, &uploader, staging, device, size2);
zest_cmd_UploadBuffer(cmd, &uploader);
```

---

## Image Operations

### zest_cmd_ImageClear

Clear an image to its default clear color.

```cpp
zest_bool zest_cmd_ImageClear(
    const zest_command_list command_list,
    zest_image image
);
```

**Description:** Clears the specified image using the clear color stored in the image's configuration. Returns `ZEST_TRUE` on success.

**Typical Usage:** Clearing render targets or storage images before use.

---

### zest_cmd_BlitImageMip

Blit (copy with scaling/filtering) between mip levels of frame graph images.

```cpp
void zest_cmd_BlitImageMip(
    const zest_command_list command_list,
    zest_resource_node src,
    zest_resource_node dst,
    zest_uint mip_to_blit,
    zest_pipeline_stage_flags pipeline_stage
);
```

**Description:** Blits a mip level from the source image resource to the same mip level in the destination image. Both images must have the same dimensions and mip count. The source image is sampled with linear filtering to produce the destination.

**Parameters:**
- `src`: Source image resource node from the frame graph
- `dst`: Destination image resource node from the frame graph
- `mip_to_blit`: The mip level index to blit
- `pipeline_stage`: Pipeline stage for synchronization

**Typical Usage:** Generating mipmaps, downsampling for bloom, or copying between render targets.

---

### zest_cmd_CopyImageMip

Copy a mip level between frame graph images (no filtering).

```cpp
void zest_cmd_CopyImageMip(
    const zest_command_list command_list,
    zest_resource_node src,
    zest_resource_node dst,
    zest_uint mip_to_copy,
    zest_pipeline_stage_flags pipeline_stage
);
```

**Description:** Copies a mip level from the source image to the destination image without any filtering or scaling. Both images must have identical dimensions, formats, and mip counts. Source must have `zest_image_flag_transfer_src`, destination must have `zest_image_flag_transfer_dst`.

**Parameters:**
- `src`: Source image resource node
- `dst`: Destination image resource node
- `mip_to_copy`: The mip level index to copy
- `pipeline_stage`: Pipeline stage for synchronization

**Typical Usage:** Copying render results between images, or backing up image contents.

---

## Barriers

### zest_cmd_InsertComputeImageBarrier

Insert a memory barrier for compute shader image access.

```cpp
void zest_cmd_InsertComputeImageBarrier(
    const zest_command_list command_list,
    zest_resource_node resource,
    zest_uint base_mip
);
```

**Description:** Inserts a pipeline barrier to synchronize compute shader writes to an image with subsequent reads. Use this when a compute shader writes to an image that will be read later in the same pass.

**Parameters:**
- `resource`: The image resource node requiring synchronization
- `base_mip`: The base mip level for the barrier

**Typical Usage:** Synchronizing between compute passes that read/write the same image.

```cpp
// First compute pass writes to image
zest_cmd_BindComputePipeline(cmd, write_shader);
zest_cmd_DispatchCompute(cmd, groups_x, groups_y, 1);

// Barrier to ensure writes complete before reads
zest_cmd_InsertComputeImageBarrier(cmd, my_image_resource, 0);

// Second compute pass reads from image
zest_cmd_BindComputePipeline(cmd, read_shader);
zest_cmd_DispatchCompute(cmd, groups_x, groups_y, 1);
```

---

## See Also

- [Immediate API](immediate.md) - One-off commands
- [Frame Graph Concept](../concepts/frame-graph/index.md)
- [Buffer Management](buffer.md) - Creating and managing buffers
- [Pipeline API](pipeline.md) - Creating graphics and compute pipelines
