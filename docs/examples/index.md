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
