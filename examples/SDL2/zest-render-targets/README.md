# Render Targets / Bloom Effect

Demonstrates bloom/blur effects using compute shader down/up sampling with multiple render targets.

## What It Does

Implements a bloom post-processing effect:
1. Render MSDF text to a transient render target
2. Downsample through mip chain using compute shader
3. Upsample back up, blending with downsampled mips
4. Composite the bloom result with the original

The effect is controlled by mouse position (X = threshold, Y = knee).

## Running

Run from the root of the Zest repository, as the font and shader paths are relative to it.

- D3D12 is the default in a build configured with `ZEST_ENABLE_D3D12`, falling back to Vulkan if the D3D12 device can't be created. Otherwise it runs on Vulkan.
- `--vulkan` or `--d3d12` chooses the backend, with no fallback
- `--validate` turns on the validation layers and exits with 1 on any validation error
- `--validate-sync` adds Vulkan synchronisation validation, or D3D12 GPU-based validation, to `--validate`
- `--frames N` exits after N frames

## Zest Features Used

- **Frame Graph**: Multi-pass with transfer, render, compute (downsample), compute (upsample), composite
- **Compute Shaders**: `zest_CreateCompute` for downsampler and upsampler
- **Transient Resources**: `zest_AddTransientImageResource` with mip levels for down/up targets
- **Mip-Level Access**: `zest_GetTransientSampledMipBindlessIndexes` for per-mip bindless indices
- **Storage Images**: Writing to specific mip levels via compute shaders
- **MSDF Fonts**: `zest_CreateFontLayer`, `zest_DrawMSDFText`
- **Fullscreen Pipeline**: `zest_SetPipelineDisableVertexInput` for composite pass
- **Image Barriers**: `zest_cmd_InsertComputeImageBarrier` between mip dispatches
- **Mip Copy**: `zest_cmd_CopyImageMip` to initialize upsampler from downsampler

## Controls

- **Mouse X**: Adjust bloom threshold
- **Mouse Y**: Adjust bloom knee
