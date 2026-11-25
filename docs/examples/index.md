# Examples

Working examples demonstrating Zest features. All examples can be built with CMake.

## Building Examples

```bash
cmake -B build -G "Visual Studio 17 2022"
cmake --build build --config Release
```

Executables are in `build/examples/SDL2/Release/` (Windows) or `build/examples/SDL2/` (Linux/macOS).

## Examples

### Basic Setup

#### zest-minimal-template {#minimal-template}

**The simplest Zest application.** Blank screen with basic frame graph.

- Device and context creation
- Frame graph caching
- Minimal render pass

**Source:** `examples/SDL2/zest-minimal-template/`

---

#### zest-imgui-template {#imgui-template}

**ImGui integration with docking support.**

- ImGui setup and rendering
- Custom fonts
- Docking workspace

**Source:** `examples/SDL2/zest-imgui-template/`

---

### Rendering Techniques

#### zest-instancing {#instancing}

**GPU instancing with 3D models.**

- Instance mesh layers
- GLTF model loading
- Per-instance data
- Efficient batched drawing

**Source:** `examples/SDL2/zest-instancing/`

---

#### zest-pbr-forward {#pbr-forward}

**Physical-based rendering (forward shading).**

- PBR materials
- Image-based lighting (IBL)
- Environment cubemaps
- BRDF lookup tables

**Source:** `examples/SDL2/zest-pbr-forward/`

---

#### zest-pbr-deferred {#pbr-deferred}

**PBR with deferred shading.**

- G-buffer rendering
- Deferred lighting pass
- Multiple render targets

**Source:** `examples/SDL2/zest-pbr-deferred/`

---

#### zest-pbr-texture {#pbr-texture}

**PBR with texture mapping.**

- Albedo, normal, roughness, metallic maps
- KTX texture loading
- Bindless texture arrays

**Source:** `examples/SDL2/zest-pbr-texture/`

---

#### zest-shadow-mapping {#shadow-mapping}

**Basic shadow mapping.**

- Depth-only render pass
- Shadow map sampling
- Bias and PCF basics

**Source:** `examples/SDL2/zest-shadow-mapping/`

---

#### zest-cascading-shadows {#cascading-shadows}

**Cascaded shadow maps (CSM).**

- Multiple shadow cascades
- Cascade selection
- Large scene shadows

**Source:** `examples/SDL2/zest-cascading-shadows/`

---

#### zest-render-targets {#render-targets}

**Multi-pass rendering with bloom.**

- Transient render targets
- Compute-based post-processing
- HDR pipeline

**Source:** `examples/SDL2/zest-render-targets/`

---

### Compute Shaders

#### zest-compute-example {#compute-example}

**GPU compute shader particle simulation.**

- Compute pipelines
- Storage buffers
- Frame graph compute passes
- Slang shader support

**Source:** `examples/SDL2/zest-compute-example/`

---

### Special Effects

#### zest-fonts {#fonts}

**MSDF font rendering.**

- MSDF font generation
- High-quality text at any scale
- Font layers

**Source:** `examples/SDL2/zest-fonts/`

---

#### zest-ribbons {#ribbons}

**Ribbon/trail effects.**

- Dynamic geometry
- Trail rendering
- Smooth curves

**Source:** `examples/SDL2/zest-ribbons/`

---

### Integrations

#### zest-timelinefx {#timelinefx}

**TimelineFX particle system integration.**

- Particle effect loading
- Effect playback
- Shape loaders

**Source:** `examples/SDL2/zest-timelinefx/`

---

#### zest-implot {#implot}

**ImPlot data visualization.**

- Real-time plotting
- Multiple chart types
- ImGui integration

**Source:** `examples/SDL2/zest-implot/`

---

### Games

#### zest-vaders {#vaders}

**Space Invaders-style game demo.**

- Complete game loop
- Sprite rendering
- Game state management
- TimelineFX effects

**Source:** `examples/SDL2/zest-vaders/`

---

### Testing

#### zest-tests {#tests}

**Comprehensive test suite.**

- Frame graph compilation tests
- Pipeline state tests
- Resource management tests
- 36+ unit tests

**Source:** `examples/SDL2/zest-tests/`

---

## Example Structure

All examples follow this pattern:

```cpp
#define ZEST_IMPLEMENTATION
#define ZEST_VULKAN_IMPLEMENTATION
#include <zest.h>

struct app_t {
    zest_device device;
    zest_context context;
    // ... example-specific state
};

void MainLoop(app_t* app) {
    while (running) {
        zest_UpdateDevice(app->device);
        if (zest_BeginFrame(app->context)) {
            // Build/get frame graph
            // Execute
            zest_EndFrame(app->context);
        }
    }
}

int main() {
    // Setup device, window, context
    // Initialize resources
    MainLoop(&app);
    // Cleanup
}
```

## Running from Source Directory

Examples expect to be run from the repository root (for asset paths):

```bash
cd /path/to/zest
./build/examples/SDL2/Release/zest-pbr-forward.exe
```

Or set the working directory in your IDE to the repository root.

## Running on D3D12 and Metal

Every built example runs on Vulkan, and on D3D12 or Metal when that backend is compiled in. In a build with
`ZEST_ENABLE_D3D12` (on by default on Windows) the examples start on D3D12, and in a build with `ZEST_ENABLE_METAL`
(on by default on macOS) they start on Metal. If that device can't be created they print one line and run on Vulkan.
Linux builds run on Vulkan. The examples written in Slang (`zest-compute-example`, `zest-vaders`, `zest-timelinefx` and
`zest-timelinefx-particle-techniques`) also need `ZEST_ENABLE_SLANG`.

They share these arguments, from `examples/Common/example_platform.h`:

| Argument | Effect |
|----------|--------|
| `--vulkan` | Use the Vulkan backend |
| `--d3d12` | Use the D3D12 backend, with no fallback to Vulkan. A build without D3D12 exits with a message |
| `--metal` | Use the Metal backend, with no fallback to Vulkan. A build without Metal exits with a message |
| `--validate` | Turn on the validation layers, print the validation error count at exit and exit with 1 if it isn't 0. On D3D12 also print the COM references the device left behind. On Metal also exit with 1 if Metal's validation couldn't be turned on |
| `--validate-sync` | `--validate` with Vulkan synchronisation validation, D3D12 GPU-based validation or Metal shader validation |
| `--frames N` | Exit after N frames |

`zest-timelinefx-particle-techniques` also takes `--effect-mode baked|cpu|gpu` to start with that technique.

Metal's validation is fixed for the whole process when its first window is made, so an example turns it on before it
makes its window. When an example falls back from Metal to Vulkan under `--validate`, MoltenVK runs with Metal's
validation on as well, and what it prints goes to standard error without counting toward the example's validation
errors.

On macOS, with vsync off as the examples have it, a window's frame rate depends on how quickly the window server hands
back its drawables, on Metal and on Vulkan alike. It can change from one launch to the next with the same GPU work per
frame, so it isn't a measure of an example's performance.

Each shader folder (an example's `shaders/`, `examples/Common/shaders/` and `examples/assets/shaders/`) keeps its
GLSL in `glsl/` and its HLSL twins in `hlsl/`. `ExampleShaderPath` in `example_platform.h` gives
`<folder>/hlsl/<name>.hlsl` on D3D12 and `<folder>/glsl/<name>` on Vulkan and Metal, where the Metal backend converts
the GLSL to MSL. A twin keeps its GLSL's maths and the same push constant, uniform and buffer layouts, so the C++ is
the same on every backend. The Slang examples compile one source for all three.

Limits on D3D12:

- `zest-shadow-mapping` sets its depth bias with `zest_cmd_SetDepthBias`. On a driver without dynamic depth bias the
  shadows lose their bias and the run logs one validation error.
- `zest-compute-example` draws point sprites on Vulkan and an instanced quad per particle on D3D12, which has no point
  sprites.
- `zest-cascading-shadows` draws its four cascades with view instancing, so it needs a GPU that supports it.
- The TimelineFX reload harness (`zest-timelinefx-reload-render`) runs on Vulkan only.

Limits on Metal:

- GPU timings come from Metal's counter samples, which an Apple GPU takes only where a pass's encoders start and end.
  So a `zest_BeginGPUProfile` region inside a compute or transfer pass runs as an encoder of its own and gets its own
  timing, and a region inside a render pass shows the whole pass's time, as a render encoder is never split.

Limits on Vulkan on macOS, which goes through MoltenVK:

- `zest-image-swizzle --vulkan --validate` and `zest-timelinefx --vulkan --validate` exit with 1. MoltenVK's
  portability subset reports no image view swizzles (`imageViewFormatSwizzle`), which `zest-image-swizzle`'s swizzled
  view and the TimelineFX backend's single channel textures use. `zest-image-swizzle` also gets a MoltenVK warning that
  Metal can't turn primitive restart off for its triangle strip. Both examples run with no validation errors on Metal.
