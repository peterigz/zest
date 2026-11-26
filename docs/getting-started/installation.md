# Installation

This guide covers building Zest from source and running the examples.

## Prerequisites

- **Vulkan SDK** - Only for the Vulkan backend, which Zest builds whenever CMake finds the SDK. D3D12 on Windows needs nothing more, Metal on macOS needs the SDK or `brew install shaderc spirv-cross`, and on Linux your distribution's packages will do. Download from [LunarG](https://vulkan.lunarg.com/)
- **CMake 3.24+** - Build system (if you want to create projects for the examples). The Vulkan backend needs 3.24, the first version whose FindVulkan has the shaderc component
- **C++17 compiler (for the examples, C11 for the library only)** - MSVC 2019+, GCC 9+, or Clang 10+
- **SDL2** - For the examples, although your own app can use anything else (including GLFW). It comes from the Vulkan SDK or a package manager, and on Windows the configure downloads it when it can't find one

### Linux-Specific Prerequisites

On Linux, you'll need to install the following packages:

=== "Ubuntu/Debian"

    ```bash
    sudo apt install libsdl2-dev libshaderc-dev vulkan-tools
    ```

=== "Fedora"

    ```bash
    sudo dnf install SDL2-devel shaderc vulkan-tools
    ```

=== "Arch Linux"

    ```bash
    sudo pacman -S sdl2 shaderc vulkan-tools
    ```

!!! note "Vulkan SDK on Linux"
    While you can install the Vulkan SDK manually from LunarG, many Linux distributions provide sufficient Vulkan development packages. If you need Slang support, you should install the full Vulkan SDK and source the setup script:
    ```bash
    source ~/path/to/vulkansdk/setup-env.sh
    ```

## Clone the Repository

```bash
git clone --recursive https://github.com/peterigz/zest.git
cd zest
```

!!! note "Submodules"
    The `--recursive` flag is important - Zest uses submodules for ImGui, FreeType and other dependencies used by the examples.

## Build with CMake

=== "Windows (Visual Studio)"

    ```bash
    cmake -B build -G "Visual Studio 17 2022"
    cmake --build build --config Release
    ```

=== "Linux/macOS"

    ```bash
    cmake -B build
    cmake --build build --config Release
    ```

## Run the Examples

After building, you'll find the executables in the build directory:

=== "Windows"

    ```bash
    build\examples\SDL2\Release\zest-minimal-template.exe
    build\examples\SDL2\Release\zest-imgui-template.exe
    ```

=== "Linux/macOS"

    ```bash
    ./build/examples/SDL2/zest-minimal-template
    ./build/examples/SDL2/zest-imgui-template
    ./build/examples/SDL2/zest-vaders
    ```

!!! tip "Working Directory"
    Run examples from the repository root directory so they can find the `examples/assets/` folder.

## The Vulkan Backend

`ZEST_ENABLE_VULKAN` decides whether the Vulkan backend is built:

- `AUTO`, the default, builds it when CMake finds Vulkan (the Vulkan SDK, or on Linux your distribution's Vulkan and
  shaderc packages), and otherwise leaves it out and says so.
- `ON` builds it, and stops the configure with an error if CMake can't find Vulkan.
- `OFF` leaves it out.

```bash
cmake -B build -DZEST_ENABLE_VULKAN=OFF
```

On Linux Vulkan is the only backend, so a configure without it stops with an error. On Windows a build without Vulkan
has D3D12 only, and on macOS Metal only.

With it on, `ZEST_ENABLE_VULKAN` is defined for everything that links zest. Define `ZEST_VULKAN_IMPLEMENTATION` next to
`ZEST_IMPLEMENTATION` under `#ifdef ZEST_ENABLE_VULKAN`, and create the device with `zest_BeginVulkanDeviceBuilder` or
`zest_implsdl2_CreateVulkanDevice`. Projects that compile zest into their own target can include
`cmake/ZestVulkan.cmake`, which sets `ZEST_VULKAN_ENABLED`, `ZEST_VULKAN_LIBRARIES`, `ZEST_VULKAN_INCLUDE_DIRS`,
`ZEST_VULKAN_LIBRARY_DIRS` and `ZEST_VULKAN_DEFINITIONS`. Include it before `cmake/ZestMetal.cmake`, so that Metal uses
Vulkan's shaderc rather than linking a second copy.

## The D3D12 Backend

On Windows the D3D12 backend is built by default. The first configure downloads two NuGet packages into the build
folder: the D3D12 Agility SDK (`ZEST_D3D12_AGILITY_VERSION`, 1.615.1) and DXC (`ZEST_D3D12_DXC_VERSION`, 1.9.2609.5),
each checked against a pinned SHA512. Without a network connection the configure stops with an error that says how to
build without D3D12:

```bash
cmake -B build -DZEST_ENABLE_D3D12=OFF
```

With it on, `ZEST_ENABLE_D3D12` is defined for everything that links zest. Define `ZEST_D3D12_IMPLEMENTATION` next to
`ZEST_IMPLEMENTATION` under `#ifdef ZEST_ENABLE_D3D12`, and create the device with `zest_BeginD3D12DeviceBuilder` or
`zest_implsdl2_CreateD3D12Device`. `zest_copy_d3d12_runtime(<target>)` copies the Agility SDK runtime into a `D3D12`
folder next to the target, and `dxcompiler.dll` and `dxil.dll` beside it, which the target needs to run. Projects that
compile zest into their own target can include `cmake/ZestD3D12.cmake` for the same function.

With D3D12 compiled in, the examples start on D3D12 and, when Vulkan is compiled in too, fall back to Vulkan if the
D3D12 device can't be created. `--vulkan` or `--d3d12` chooses a backend. See [Examples](../examples/index.md).

PIX and RenderDoc show each frame graph pass by name, as the D3D12 backend always wraps passes in event markers and
names its queues and command lists. With validation on, a device removal writes DRED breadcrumbs and any page fault
to the log.

## The Metal Backend

On Apple the Metal backend is built by default. It compiles GLSL to SPIR-V with shaderc and converts SPIR-V to MSL with
SPIRV-Cross. Both come from the Vulkan SDK when `VULKAN_SDK` is set to the SDK's `macOS` folder, and otherwise from
Homebrew:

```bash
brew install shaderc spirv-cross
```

When the Vulkan backend is built, Metal uses its shaderc. With both the SDK and Homebrew's packages installed, set
`VULKAN_SDK`, or CMake can take Vulkan from the SDK and shaderc from Homebrew. If CMake can't find SPIRV-Cross or
shaderc the configure stops with an error that says how to build without Metal:

```bash
cmake -B build -DZEST_ENABLE_METAL=OFF
```

With it on, `ZEST_ENABLE_METAL` is defined for everything that links zest, and the deployment target is at least
macOS 15. Homebrew builds its libraries for the macOS it runs on, so when SPIRV-Cross or shaderc comes from Homebrew the
deployment target is raised to that version, and the configure says so. Use the Vulkan SDK's libraries to target
macOS 15. Define `ZEST_METAL_IMPLEMENTATION` next to `ZEST_IMPLEMENTATION` under `#ifdef ZEST_ENABLE_METAL`, and create
the device with `zest_BeginMetalDeviceBuilder` or `zest_implsdl2_CreateMetalDevice`. The file that defines the
implementation has to compile as Objective-C++ without ARC, or as Objective-C for a `.c` file:
`zest_metal_implementation_source(<target> <file>)` sets that. Projects that compile zest into their own target can
include `cmake/ZestMetal.cmake` for the same function, after `cmake/ZestVulkan.cmake` when Vulkan is built. They then
add `ZEST_METAL_SPIRV_CROSS_INCLUDE_DIR` and `ZEST_METAL_SHADERC_INCLUDE_DIR` as include folders, and link
`ZEST_METAL_FRAMEWORKS`, `ZEST_METAL_SPIRV_CROSS_LIBRARIES` and `ZEST_METAL_SHADERC_LIBRARY`. The shaderc ones are empty
when Vulkan's shaderc serves Metal.

With Metal compiled in, the examples start on Metal and, when Vulkan is compiled in too, fall back to Vulkan if the
Metal device can't be created. `--vulkan` or `--metal` chooses a backend. See [Examples](../examples/index.md).

Xcode's frame capture and Instruments show each command buffer, pass and pipeline by name, as the Metal backend always
makes each frame graph pass a debug group, labels the pass's encoders with its name and each command buffer with its
frame graph's, and names pipeline states after their templates and libraries after their shaders. A command buffer that
fails writes a report to the log with the error and the frame graph, and with validation on, each pass's state.

On Metal a shader's GLSL is compiled to SPIR-V with shaderc, then converted to MSL with SPIRV-Cross and compiled by
Metal when a pipeline first needs it. With shader caching on (`zest_DeviceBuilderCacheShaders`) a second launch skips
the GLSL compile, as on Vulkan: the SPIR-V is read from `./spv/`. What's left is the conversion, about 0.3 ms a shader
on an M2, and Metal's own compile, which Metal caches itself after the first run on the machine. Shaders can also be
written in MSL, see [MSL Descriptor Layout](../concepts/bindless.md#msl-descriptor-layout).

## Optional: Enable Slang Shader Compiler

If you want to use the [Slang](https://shader-slang.com/) shader compiler for runtime shader compilation:

```bash
cmake -B build -DZEST_ENABLE_SLANG=ON
```

This enables the `zest-compute-example` which demonstrates Slang integration with compute shaders.

To compile Slang only in some configurations, so that release builds have no Slang dependency, list them in
`ZEST_SLANG_CONFIGS`. `ZEST_ENABLE_SLANG` is then only defined in those configurations, so guard Slang code with
`#ifdef ZEST_ENABLE_SLANG`:

```bash
cmake -B build -DZEST_ENABLE_SLANG=ON -DZEST_SLANG_CONFIGS=Debug
```

`zest_copy_slang_runtime(<target>)` copies the Slang runtime libraries (`slang.dll`, `slang-compiler.dll`,
`slang-glslang.dll` and `slang-glsl-module.dll` on Windows) next to a target after it builds, so it runs without the
Vulkan SDK on the `PATH`. Without `slang-glslang.dll` Slang can't run its SPIR-V optimiser and outputs unoptimised code.

Projects that compile zest into their own target instead of using `add_subdirectory` can include the same logic from
`cmake/ZestSlang.cmake`. Set the options first; `zest_target_use_slang` then defines `ZEST_ENABLE_SLANG`, adds the Slang
headers, links Slang and copies its runtime, all only in the configurations Slang is enabled for:

```cmake
set(ZEST_ENABLE_SLANG ON)
set(ZEST_SLANG_CONFIGS Debug)
include(${ZEST}/cmake/ZestSlang.cmake)
zest_target_use_slang(${NAME})
```

The module also sets `ZEST_SLANG_ENABLED_GENEX`, `SLANG_INCLUDE_DIR`, `SLANG_LIBRARY`, `SLANG_LIBRARY_LINK` and
`ZEST_SLANG_RUNTIME_FILES`, and can be included more than once, for example by the host and by zest as a subdirectory.
`zest_target_use_slang` links with the plain `target_link_libraries` signature, so call it on targets that don't use the
`PUBLIC`/`PRIVATE` keyword form.

!!! warning "Slang Requirement"
    The `VULKAN_SDK` environment variable must be set when using Slang. On Linux, source the Vulkan SDK setup script before running CMake:
    ```bash
    source ~/path/to/vulkansdk/setup-env.sh
    cmake -B build -DZEST_ENABLE_SLANG=ON
    ```

## Run the Test Suite

The tests read frame graph records that only exist without `NDEBUG`, so on Windows build them in Debug:

=== "Windows"

    ```bash
    cmake --build build --config Debug --target zest-tests
    build\examples\SDL2\Debug\zest-tests.exe
    ```

=== "Linux/macOS"

    ```bash
    ./build/examples/SDL2/zest-tests
    ```

The shared suite has 140 tests on Vulkan, covering frame graph compilation, memory management, pipeline states, image
formats, compute and device resets. 7 of them need Slang (`-DZEST_ENABLE_SLANG=ON`). D3D12 and Metal run all but the 4
Vulkan-only ones, then their own backend tests.

## Using Zest in Your Project

### Option 1: Copy Headers

Copy these files to your project:

- `zest.h` - Main API header
- `zest_vulkan.h` - Vulkan implementation
- `zest_utilities.h` (optional) - image loading, mesh primitives, basic gltf loading, fonts

### Option 2: Add as Subdirectory

```cmake
add_subdirectory(path/to/zest)
target_link_libraries(your_app zest-interface)
```

### Implementation Defines

In exactly **one** `.cpp` file, define the implementation macros before including:

```cpp
#define ZEST_IMPLEMENTATION
#define ZEST_VULKAN_IMPLEMENTATION
#include <zest.h>
```

All other files just include the header normally:

```cpp
#include <zest.h>
```

## Troubleshooting

### Vulkan Validation Errors

Enable validation layers during development:

```cpp
zest_window_data_t window_data = zest_implsdl2_CreateWindow(50, 50, 1280, 768, 0, "My App");
zest_device device = zest_implsdl2_CreateVulkanDevice(&window_data, true);  // true = enable validation
```

### Missing Submodules

If you cloned without `--recursive`:

```bash
git submodule update --init --recursive
```

### GPU Not Supported

Zest requires Vulkan 1.2+ with bindless descriptor support. Most discrete GPUs from 2016+ are supported. Some integrated GPUs may not have full bindless support.

### Linux: shaderc Not Found

If you get a warning about shaderc not being found, install the shaderc development package for your distribution (see Linux-Specific Prerequisites above). shaderc is required for runtime shader compilation.

### SDL2 Not Found

If CMake reports SDL2 not found, the examples are skipped. To get them:

1. On Linux, install the SDL2 development package (see Linux-Specific Prerequisites above). On macOS, `brew install sdl2`.
2. On Windows, the configure downloads SDL2 2.32.10's VC package when it can't find SDL2, and a warning above says why
   that failed. Set `SDL2_DIR` to another SDL2 install, or set `ZEST_SDL2_VERSION` and `ZEST_SDL2_SHA256` for another
   version. The examples copy the downloaded `SDL2.dll` next to themselves.
3. If using the Vulkan SDK's SDL2, ensure `VULKAN_SDK` is set before running CMake

### Linux: Slang Library Not Found

If Slang is enabled but not found:

1. Ensure the Vulkan SDK is installed (not just system packages)
2. Source the SDK setup script: `source ~/vulkansdk/x.x.x.x/setup-env.sh`
3. Verify `VULKAN_SDK` is set: `echo $VULKAN_SDK`
4. Re-run CMake after sourcing the setup script
