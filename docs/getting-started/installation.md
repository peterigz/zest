# Installation

This guide covers building Zest from source and running the examples.

## Prerequisites

- **Vulkan SDK** - Although Zest has a separate platform layer, only Vulkan is implemented currently. Download from [LunarG](https://vulkan.lunarg.com/)
- **CMake 3.8+** - Build system (if you want to create projects for the examples)
- **C++17 compiler (for the examples, C11 for the library only)** - MSVC 2019+, GCC 9+, or Clang 10+
- **SDL2** - For convenience although you can use anything else if you want (including GLFW). Windowing library (included as submodule)

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
    The `--recursive` flag is important - Zest uses submodules for SDL2, ImGui, and other dependencies used by the examples.

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

## Optional: Enable Slang Shader Compiler

If you want to use the [Slang](https://shader-slang.com/) shader compiler for runtime shader compilation:

```bash
cmake -B build -DZEST_ENABLE_SLANG=ON
```

This enables the `zest-compute-example` which demonstrates Slang integration with compute shaders.

!!! warning "Slang Requirement"
    The `VULKAN_SDK` environment variable must be set when using Slang. On Linux, source the Vulkan SDK setup script before running CMake:
    ```bash
    source ~/path/to/vulkansdk/setup-env.sh
    cmake -B build -DZEST_ENABLE_SLANG=ON
    ```

## Run the Test Suite

=== "Windows"

    ```bash
    build\examples\SDL2\Release\zest-tests.exe
    ```

=== "Linux/macOS"

    ```bash
    ./build/examples/SDL2/zest-tests
    ```

The test suite includes 68 tests covering frame graph compilation, memory management, pipeline states, and image formats.

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

### Linux: SDL2 Not Found

If CMake reports SDL2 not found:

1. Install the SDL2 development package (see Linux-Specific Prerequisites above)
2. If using the Vulkan SDK's SDL2, ensure `VULKAN_SDK` is set before running CMake

### Linux: Slang Library Not Found

If Slang is enabled but not found:

1. Ensure the Vulkan SDK is installed (not just system packages)
2. Source the SDK setup script: `source ~/vulkansdk/x.x.x.x/setup-env.sh`
3. Verify `VULKAN_SDK` is set: `echo $VULKAN_SDK`
4. Re-run CMake after sourcing the setup script
