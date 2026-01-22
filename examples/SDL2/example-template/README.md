# Example Template

A starting template for creating new Zest examples with ImGui integration, 3D camera controls, and frame graph setup.

## What It Does

Provides boilerplate code with:
- ImGui window setup with docking support
- Free-look camera with WASD movement and mouse control
- Uniform buffer for view/projection matrices
- Custom mesh pipeline template with vertex/instance attributes
- Frame graph with depth buffer and ImGui pass

## Zest Features Used

- **Frame Graph**: `zest_BeginFrameGraph`, `zest_EndFrameGraph`, `zest_QueueFrameGraphForExecution`
- **Frame Graph Caching**: `zest_InitialiseCacheKey`, `zest_GetCachedFrameGraph`
- **Transient Resources**: `zest_AddTransientImageResource` for depth buffer
- **Resource Groups**: `zest_CreateResourceGroup`, `zest_AddResourceToGroup`
- **Pipeline Templates**: `zest_BeginPipelineTemplate` with custom vertex attributes
- **Uniform Buffers**: `zest_CreateUniformBuffer`
- **Samplers**: `zest_CreateSampler`, `zest_AcquireSamplerIndex`
- **ImGui Integration**: `zest_imgui_Initialise`, `zest_imgui_BeginPass`
- **Timer System**: `zest_CreateTimer`, `zest_StartTimerLoop`
- **Camera Utilities**: `zest_CreateCamera`, `zest_TurnCamera`

## Controls

- **Right Mouse + WASD**: Move camera
- **Right Mouse + Mouse Move**: Look around
