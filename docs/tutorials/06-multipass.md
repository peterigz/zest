# Tutorial 6: Multi-Pass Rendering

Learn to create complex rendering pipelines with multiple passes and transient resources.

**Example:** `examples/SDL2/zest-render-targets`

## What You'll Learn

- Transient render targets
- Multi-pass frame graphs
- Bloom effect implementation
- Compute-based post-processing

## Transient Resources

Resources that exist only during frame graph execution:

```cpp
zest_image_resource_info_t hdr_info = {
    .format = zest_format_r16g16b16a16_sfloat,
    .usage_hint = zest_resource_usage_hint_render_target,
    .width = screen_width,
    .height = screen_height,
    .mip_levels = 1
};
zest_resource_node hdr_target = zest_AddTransientImageResource("HDR", &hdr_info);
```

## Multi-Pass Frame Graph

```cpp
if (zest_BeginFrameGraph(context, "Bloom Pipeline", &cache_key)) {
    zest_ImportSwapchainResource();

    // Transient targets
    zest_resource_node scene = zest_AddTransientImageResource("Scene", &hdr_info);
    zest_resource_node bright = zest_AddTransientImageResource("Bright", &hdr_info);
    zest_resource_node blur_h = zest_AddTransientImageResource("BlurH", &hdr_info);
    zest_resource_node blur_v = zest_AddTransientImageResource("BlurV", &hdr_info);

    // Pass 1: Render scene to HDR
    zest_BeginRenderPass("Scene"); {
        zest_ConnectOutput(scene);
        zest_SetPassTask(RenderScene, app);
        zest_EndPass();
    }

    // Pass 2: Extract bright pixels
    zest_BeginComputePass(extract_compute, "Extract Bright"); {
        zest_ConnectInput(scene);
        zest_ConnectOutput(bright);
        zest_SetPassTask(ExtractBright, app);
        zest_EndPass();
    }

    // Pass 3: Horizontal blur
    zest_BeginComputePass(blur_compute, "Blur H"); {
        zest_ConnectInput(bright);
        zest_ConnectOutput(blur_h);
        zest_SetPassTask(BlurHorizontal, app);
        zest_EndPass();
    }

    // Pass 4: Vertical blur
    zest_BeginComputePass(blur_compute, "Blur V"); {
        zest_ConnectInput(blur_h);
        zest_ConnectOutput(blur_v);
        zest_SetPassTask(BlurVertical, app);
        zest_EndPass();
    }

    // Pass 5: Composite
    zest_BeginRenderPass("Composite"); {
        zest_ConnectInput(scene);
        zest_ConnectInput(blur_v);
        zest_ConnectSwapChainOutput();
        zest_SetPassTask(Composite, app);
        zest_EndPass();
    }

    return zest_EndFrameGraph();
}
```

## Benefits of Transient Resources

- **Memory aliasing** - Non-overlapping resources share memory
- **Automatic barriers** - Frame graph handles transitions
- **No manual cleanup** - Freed after execution

## Full Example

See `examples/SDL2/zest-render-targets/zest-render-targets.cpp` for complete bloom implementation.

## Next Steps

- [Tutorial 7: Shadow Mapping](07-shadows.md)
- [Memory Concept](../concepts/memory.md) - Transient resource memory
