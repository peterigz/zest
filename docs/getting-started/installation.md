# Installation

This guide covers building Zest from source and running the examples.

## Prerequisites

- **Vulkan SDK** - Download from [LunarG](https://vulkan.lunarg.com/)
- **CMake 3.15+** - Build system
- **C++17 compiler** - MSVC 2019+, GCC 9+, or Clang 10+
- **GLFW** or **SDL2** - Windowing library (included as submodule)

## Clone the Repository

```bash
git clone --recursive https://github.com/peterigz/zest.git
cd zest
```

!!! note "Submodules"
    The `--recursive` flag is important - Zest uses submodules for GLFW, ImGui, and other dependencies.

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
    build\examples\GLFW\Release\zest-minimal-template.exe
    build\examples\GLFW\Release\zest-imgui-template.exe
    ```

=== "Linux/macOS"

    ```bash
    ./build/examples/GLFW/zest-minimal-template
    ./build/examples/GLFW/zest-imgui-template
    ```

## Optional: Enable Slang Shader Compiler

To use Slang shaders instead of GLSL:

```bash
cmake -B build -DZEST_ENABLE_SLANG=ON
```

!!! warning "Slang Requirement"
    The `VULKAN_SDK` environment variable must be set when using Slang.

## Run the Test Suite

```bash
build\examples\GLFW\Release\zest-tests.exe
```

The test suite includes 36 tests covering frame graph compilation, memory management, pipeline states, and image formats.

## Using Zest in Your Project

### Option 1: Copy Headers

Copy these files to your project:

- `zest.h` - Main API header
- `zest_vulkan.h` - Vulkan implementation
- `zest_utilities.h` (optional) - GLFW/SDL helpers, image loading, fonts

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
zest_device device = zest_implglfw_CreateDevice(true);  // true = enable validation
```

### Missing Submodules

If you cloned without `--recursive`:

```bash
git submodule update --init --recursive
```

### GPU Not Supported

Zest requires Vulkan 1.2+ with bindless descriptor support. Most discrete GPUs from 2016+ are supported. Some integrated GPUs may not have full bindless support.
