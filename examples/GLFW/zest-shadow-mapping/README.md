# Shadow Mapping

Basic shadow mapping for directional light sources with PCF (Percentage Closer Filtering).

## What It Does

Recreated from [Sascha Willems' shadow mapping example](https://github.com/SaschaWillems/Vulkan/tree/master/examples/shadowmapping). Renders a Vulkan scene with dynamic shadows cast from an animated directional light. Uses a two-pass approach: first renders depth from light's perspective, then renders the scene sampling the shadow map.

## Zest Features Used

- **Frame Graph**: Multi-pass (depth pass + scene pass)
- **Transient Resources**: `zest_AddTransientImageResource` for shadow map depth buffer
- **Pipeline Templates**: Scene pipeline and shadow (depth-only) pipeline
- **Depth Bias**: `zest_SetPipelineDepthBias` to reduce shadow acne
- **GLTF Loading**: `LoadGLTFScene` for the Vulkan scene model
- **Instance Mesh Layer**: `zest_CreateInstanceMeshLayer`
- **Uniform Buffers**: View/projection and light space matrices
- **Push Constants**: Shadow map index, sampler index, PCF enable flag
- **Transient Image Access**: `zest_GetTransientSampledImageBindlessIndex`
- **ImGui Integration**: Light FOV and PCF toggle controls

## Controls

- **Right Mouse + WASD**: Move camera
- **Right Mouse + Mouse Move**: Look around
- **Light FOV slider**: Adjust shadow map coverage
- **Enable PCF checkbox**: Toggle soft shadows
