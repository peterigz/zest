# Instanced Mesh Rendering

Renders 8192 asteroid instances orbiting a planet using GPU instancing.

## What It Does

Recreated from [Sascha Willems' Vulkan instancing example](https://github.com/SaschaWillems/Vulkan/tree/master/examples/instancing). Displays a lava planet surrounded by two rings of procedurally placed asteroids. Each asteroid has randomized position, rotation, scale, and texture layer. Includes an animated skybox cubemap.

## Running

Run from the root of the Zest repository, as the shader, model and texture paths are relative to it.

- D3D12 is the default in a build configured with `ZEST_ENABLE_D3D12`, falling back to Vulkan if the D3D12 device can't be created. Otherwise it runs on Vulkan.
- `--vulkan` or `--d3d12` chooses the backend, with no fallback
- `--validate` turns on the validation layers and exits with 1 on any validation error
- `--validate-sync` adds Vulkan synchronisation validation, or D3D12 GPU-based validation, to `--validate`
- `--frames N` exits after N frames

## Zest Features Used

- **Instance Mesh Layer**: `zest_CreateInstanceMeshLayer`, `zest_AddMeshToLayer`
- **GLTF Loading**: Custom `LoadGLTFScene` for planet and rock meshes
- **KTX Textures**: `zest_LoadKTX` for planet, rock array, and cubemap textures
- **Bindless Descriptors**: Texture array (`zest_texture_array_binding`), cubemap (`zest_texture_cube_binding`)
- **Pipeline Templates**: Custom vertex + instance attribute layouts
- **Multiple Pipelines**: Rock, planet, and skybox with different configurations
- **Uniform Buffers**: View/projection matrices with animation parameters
- **Frame Graph**: Geometry pass with depth buffer
- **Push Constants**: Texture indices passed to shaders
- **Camera Controls**: Free-look with WASD movement

## Controls

- **Right Mouse + WASD**: Move camera
- **Right Mouse + Mouse Move**: Look around
- **Toggle VSync**: Button in ImGui window
