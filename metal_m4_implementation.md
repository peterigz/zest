# Metal M4: Pipelines

**Goal:** the Metal backend compiles GLSL to SPIR-V, converts SPIR-V to MSL when a pipeline first needs it, builds render and compute pipelines, and draws and dispatches through lazily flushed bind state. The pipeline and layer tests pass on Metal, the debug overlay draws, and the minimal template runs with `--metal`.

**Architecture:**
- **Shaders.** `compile_shader` is shaderc with `ZEST_METAL=1`, as Vulkan's is with `ZEST_VULKAN=1`, so a shader's binary is SPIR-V on both. Conversion happens when a pipeline or compute is built: SPIRV-Cross with Spike A's option set, then `newLibraryWithSource`. The function and its reflection (vertex input locations, compute local size) go into a device cache keyed by the SPIR-V's bytes. A binary from the disk cache, from `zest_CreateShaderFromBinary` or from a Slang reload callback then converts the same way, and a hot reload converts again because its bytes changed.
- **Render pipelines.** `build_pipeline` reads the template and the command list's rendering info, as D3D12's does. Render pipeline states and depth-stencil states live in two device caches keyed by content, so a resize, which throws away every context pipeline, compiles nothing. Cull mode, winding, fill mode, depth clip and depth bias are encoder state that the pipeline backend carries.
- **Lazy bind state.** Bind calls record what the app asked for. One flush before each draw or dispatch sends what differs from what the open encoder holds. A Metal encoder starts with nothing set, so opening one clears the recorded side. The bindless table is bound when a render or compute encoder opens, with one ranged call per stage.
- **Compute passes** open a compute encoder at their first dispatch, and switch with M3's blit encoder on the queue's fence. Dispatches in one compute encoder run in order, so `insert_compute_image_barrier` records nothing.
- **The debug overlay** compiles its own copy of Vulkan's overlay GLSL. **The minimal template** defines the Metal implementation.

**Tech stack:**
- `zest_metal.h`, most of the work, and `zest-metal-tests.cpp`.
- Shared: `zest-pipeline-tests.cpp` (two tests), `zest-tests.cpp` (the pending list, and the runner if Decision 3 is taken), `zest-minimal-template.cpp`.
- shaderc from the Vulkan SDK, which every Mac build already links (`CMakeLists.txt:162-167`), and SPIRV-Cross's C API, which `cmake/ZestMetal.cmake:27-32` already finds and links. No new dependency.
- No change to `zest.h`, `zest_vulkan.h` or `zest_dx12.h`. No shader file changes.

**Spec:** `metal_platform_layer_implementation.md`: the M4 row of [Roadmap_], the [Mapping_] rows Shader creation, Clip space, Push constants, Compute local size, Pipelines, Indirect draws and Passes, Spike A under [Built_] (Task 2) and decisions 6 to 8. `metal_m3_implementation.md` [Built_], "For M4's plan". D3D12's M4 plan (`docs/superpowers/plans/dx12_m4_pipelines_implementation.md`, git-ignored, on the Windows machine only) is the structural model, and what this plan needs from it is carried here.

## Contents

- [Constraints_] Global Constraints
- [Settled_] Settled
- [State_] Current state
- [Probe_] What the SDK headers say
- [Decisions_] Decisions for Peter
- [Review_] Review Focus
- [Scope_] Out of scope
- [Phase_1] Shaders
- [Phase_2] Render pipelines and draws
- [Phase_3] Compute
- [Phase_4] Overlay, template and the suite
- [Cost_] Per-frame cost
- [Testing_] Testing
- [Built_] As built

## [Constraints_] Global Constraints

Everything in `metal_m3_implementation.md` [Constraints_] holds: never commit, edit on Windows and build and run on the Mac over `ssh macmini`, restore the Mac clone at the end of every phase (`git checkout -- <files>`, `spikes/` stays untracked), write a script with a backslash to a file, break every new test on purpose once, follow code-style. What M4 adds:

- **Never edit** `zest.h`, `zest_vulkan.h` or `zest_dx12.h`. A problem found in one goes to Peter with a repro.
- **Shared edits** are only the files under Tech stack. After each, build `zest-tests` and `zest-c-compile-check` on Windows in Debug, and `zest-minimal-template` after Task 7. The Windows suites are Peter's. The C++ sources are CRLF: check with `file` before scripting an edit.
- **Budget.** Vulkan and D3D12 get no new work. On Metal a draw or a dispatch allocates nothing, takes no lock and opens no autorelease pool. A pipeline or compute build may take the cache lock and compile, once per template and rendering info per context, never per draw. See [Cost_].
- **Unretained references** (M3 [Settled_]). A frame graph command makes no temporary Metal object. Render pipeline and depth-stencil states belong to the device's caches until a reset, which waits for the device to go idle first. A compute's pipeline state belongs to the compute and goes with Zest's deferred free.
- **Locks.** Lazily created objects take a lock on creation and none on the draw path (design [Lessons_]). The caches are read only when a pipeline is built.
- **Errors.** A pipeline the backend can't build is logged through `zest__log_validation_error` (`zest.h:14616`) with the pipeline's name and the state, and the build returns false. It never asserts, so a test can count it, as on D3D12. A wrong call reports through `ZEST_ASSERT_OR_VALIDATE` and a limit of the backend through `zest__mtl_report_unavailable` (M3). Shader compile errors go to the log and `last_error`, never to the validation count, as Vulkan's do: "Shader Platform Macro" counts validation errors and expects none.
- **Runs.** The implementer runs `zest-tests` and `--frames N` example runs. Launching an example without `--frames` is Peter's. Windowed tests need the Mac's screen unlocked: check `ssh macmini 'ioreg -n Root -d1 | grep IOConsoleLocked'` before judging a windowed failure.
- **Baseline:** M3's last commit, f6ddb18. `Dynamic rendering: 129 / 129`, `Legacy render pass: 129 / 129`, `Metal suite: 77 / 77`, `Metal backend: 36 / 36`, exit 0, no VUID line. Task 1's first step confirms it.

## [Settled_] Settled

The implementer doesn't reopen these.

- **From the design, not reopened:** shaders reach Metal as SPIR-V through SPIRV-Cross. Bind state is lazy. State with no native form is a pipeline-creation error and a documented limit. A member with no implementation is a stub that reports. Decisions 6 (the uniform alias workaround), 7 (an array indexed only by constants is a pipeline-creation error) and 8 (`.length()` on a bindless storage buffer passes SPIRV-Cross's error on).
- **Spike A's conversion** (design [Built_], Task 2):
  - `SPVC_COMPILER_OPTION_MSL_VERSION` 3.1.0, `MSL_PLATFORM` macOS, `MSL_ARGUMENT_BUFFERS` true, `MSL_ARGUMENT_BUFFERS_TIER` 1 (which is tier 2), `FLIP_VERTEX_Y` true.
  - `spvc_compiler_msl_add_discrete_descriptor_set(compiler, 0)`.
  - One `spvc_msl_resource_binding_2` per slot for the shader's stage: `desc_set` 0, `binding` the slot, `count` 0, and `msl_buffer`, `msl_texture` and `msl_sampler` all `ZEST__MTL_BINDLESS_BUFFER_INDEX` + slot. One more for push constants: `SPVC_MSL_PUSH_CONSTANT_DESC_SET`, `SPVC_MSL_PUSH_CONSTANT_BINDING`, `msl_buffer` `ZEST__MTL_PUSH_CONSTANT_BUFFER_INDEX`.
  - The MSL entry name from `spvc_compiler_get_cleansed_entry_point_name` (`main` becomes `main0`). `MTLCompileOptions.languageVersion = MTLLanguageVersion3_1`, everything else default. The function is taken with `newFunctionWithName:constantValues:error:` and an empty `MTLFunctionConstantValues`, which a shader with specialisation constants needs.
  - The alias workaround replaces `constant void* spvBufferAliasSet` with `device const void* spvBufferAliasSet` in the MSL before compiling.
  - Buffer indices: vertex buffer binding N at N for 0 to 3, push constants at 4, slot N of the table at 5 + N (`zest_metal.h:92-94`).
- **Metal compiles GLSL with `ZEST_METAL=1`**, caches SPIR-V in `./spv/` beside Vulkan's with the salt `0x4D450001` (design [Decisions_], "Calls made without Peter"; `zest_metal.h:1060`, `zest_metal.h:1503`). Whether it also defines `ZEST_VULKAN` is Decision 4.
- **Clip space and winding** (Spike A): SPIRV-Cross flips Y in the vertex stage, the viewport passes straight through (`zest_metal.h:3904`), and the front face maps straight across: clockwise is `MTLWindingClockwise`.
- **Carried from D3D12's M4:**
  - Every colour attachment takes the template's one blend state, as on Vulkan.
  - A depth test with no depth attachment is turned off: Vulkan passes every fragment there.
  - Push constants are one 128-byte block per command list, serving graphics and compute. Shared code always pushes through the device's layout (`zest.h:23181`).
  - Pipeline objects live in device caches keyed by content, and a context's `zest_pipeline` borrows them. The build reads only `rendering_info`, `context` and `device` from the command list, because the pipeline tests pass one with no backend (`zest-pipeline-tests.cpp:12-20`). A sample count of 0 reads as 1.
  - `line_width` is ignored.
- **Metal's indirect argument structs have Vulkan's field order** ([Probe_]), so a Zest indirect buffer is read as it is.

## [State_] Current state (verified 2026-11-23, Windows tree at f6ddb18)

**What M3 left in `zest_metal.h`** (4,163 lines)
- 34 stubs in `Unavailable_stubs` (`zest_metal.h:4103-4157`). M4 replaces 28, or 26 without Decision 1. `get_msaa_sample_count` stays (Decision 2), and the five GPU profiler members stay for M7.
- Neither `shaderc/shaderc.h` nor `spirv_cross/spirv_cross_c.h` is included (`zest_metal.h:43-54`). `zest_vulkan.h` includes shaderc unconditionally (`zest_vulkan.h:45`).
- `zest_mtl_pipeline_layout_backend_t` holds only `push_constant_size` (`zest_metal.h:271-273`). There's no shader, pipeline or compute backend struct.
- `zest_mtl_command_list_backend_t` has a render encoder, a blit encoder and the batch's fence, and no compute encoder or bind state (`zest_metal.h:185-195`).
- **Nothing binds the bindless table.** `bind_descriptor_sets` checks its sets and records nothing (`zest_metal.h:3890-3902`). The backend tests bind it themselves (`metal_bind_table`, `zest-metal-tests.cpp:77-85`).
- **Encoders in a frame graph batch:**
  - `begin_render_pass` opens the render encoder and waits on the fence (`zest_metal.h:3748-3807`). `end_render_pass` updates the fence and ends it (`zest_metal.h:3685-3691`).
  - `zest__mtl_pass_blit_encoder` refuses inside a render pass (`zest_metal.h:3694-3706`). `release_barrier` and `end_command_buffer` end it (`zest_metal.h:3717-3746`).
  - `zest__mtl_blit_image_mip` and `zest__mtl_image_clear` open render encoders of their own on the context's descriptor (`zest_metal.h:3972`, `zest_metal.h:4007`).
  - A compute pass has no begin or end hook. Its callbacks run between `acquire_barrier` and `release_barrier`.
- **Immediate queues:** one blit encoder, opened lazily on the queue's fence (`zest_metal.h:2163-2172`) and ended by `zest__mtl_end_queue_encoder` (`zest_metal.h:2174-2181`). `last_bound_compute` is cleared at release (`zest_metal.h:2101`). Immediate command buffers are retained (`zest_metal.h:2129`) and frame graph batches are not (`zest_metal.h:3646`).
- **A reset** keeps the `MTLDevice` and remakes the residency set and the queues (`zest__mtl_reinit_logical_device`, `zest_metal.h:1517-1528`). `zest_ResetDevice` destroys every context first (`zest.h:9822-9824`) and cleans up every compute and shader (`zest.h:11877-11881`) before it calls that hook (`zest.h:9917`).
- Helpers to reuse: `zest__mtl_pixel_format` (`zest_metal.h:772`), `zest__mtl_clamped_scissor` (`zest_metal.h:997`), `zest__mtl_buffer_object` (M1), and M3's blit pipeline table as the lock pattern (`zest__mtl_blit_pipeline`, `zest_metal.h:3139`).

**Shared shader code** (`zest.h`; Vulkan for what each hook does)
- `compile_shader` (`zest.h:4753`) is called by `zest_CompileShader` (`zest.h:13291`), `zest_CreateShader` (`zest.h:13410`) and hot reload (`zest.h:13606`).
- Vulkan's (`zest_vulkan.h:7255-7299`):
  - It ignores its `code`, `code_length`, `type` and `entry_point` arguments. It compiles `shader->shader_code` as `shader->type` with entry point `main`.
  - It defines `ZEST_VULKAN=1` (`zest_vulkan.h:7271`) and then each of `options->macro_definitions`.
  - On success it resizes the `binary` zest_vec, copies the SPIR-V, sets `binary_size` and frees `last_error`. On failure it sets `last_error` and keeps the old binary, which hot reload relies on.
  - The shaderc compiler is created on the first compile or validate (`zest_vulkan.h:7234`, `zest_vulkan.h:7259`) and released at cleanup (`zest_vulkan.h:3174`). The stage mapping is `zest__to_shaderc_shader_kind` (`zest_vulkan.h:1087`).
- `validate_shader` (`zest_vulkan.h:7231`) compiles and discards, with `ZEST_VULKAN=1` and no user macros (`zest_vulkan.h:7242`).
- **Binaries that never pass through `compile_shader`:** a cache file read by `zest_CreateShader` (`zest.h:13395`), `zest_CreateShaderFromBinaryFile` and `zest_CreateShaderFromBinary`. `zest__shader_binary_matches_platform` expects the SPIR-V magic on Metal (`zest.h:13363`). A Slang reload callback writes `binary` itself.
- The cache key (`zest__shader_cache_key`, `zest.h:13323`) includes the salt when it isn't 0.
- **Hot reload** (`zest_CheckShaderHotReload`, `zest.h:13615`) waits for the device to go idle, throws away the dependent templates' cached pipelines, and rebuilds dependent computes with `cleanup_compute_backend`, `new_compute_backend` and `finish_compute` (`zest.h:13579-13584`).

**Shared pipeline code**
- `build_pipeline` (`zest.h:4721`) is called only by `zest__cache_pipeline` (`zest.h:13159`), on a miss in `zest_GetPipeline`, so inside pass callbacks. `new_pipeline_backend` is called just before (`zest.h:12811`). A failed build flags the template `zest_pipeline_invalid` (`zest.h:13164`) and calls `cleanup_pipeline_backend` on the half-built pipeline (`zest.h:13165`).
- A context's cache key is the template's address and the whole rendering info, including `render_pass_key` (`zest.h:13728-13729`). A swapchain recreate throws away every cached pipeline (`zest.h:12466`).
- **The template** (`zest.h:4537`):
  - one `color_blend_attachment` (`zest.h:4519`), rasterization (`zest.h:4498`), depth-stencil (`zest.h:4511`), vertex bindings (`zest.h:4485`) and attributes (`zest.h:4478`), `no_vertex_input`, `view_mask`
  - no blend constants, logic op, primitive restart or stencil ops
  - defaults (`zest.h:13676-13700`): alpha blend, fill, triangle list, cull none, front face clockwise, depth test and write off, compare `less_or_equal`
- **The rendering info** (`zest.h:4409`): colour count and formats, depth format, a stencil format shared code never sets, `sample_count`, `view_mask`, `render_pass_key`.
- **Vulkan:** every pipeline has one sample, whatever the rendering info says (`zest_vulkan.h:4020`). Primitive restart is off (`zest_vulkan.h:3998`). It logs "Built pipeline %s" (`zest_vulkan.h:4102`).

**Recording**
- `zest_cmd_SendPushConstants` always passes the device's layout (`zest.h:23181`). `ZEST_MAX_PUSH_SIZE` is 128 (`zest.h:1979`). Layers push all 128 bytes, whatever the shader declares. Examples push before they bind (`examples/Common/pbr_functions.cpp:87-89`).
- `bind_vertex_buffer` binds one buffer at `first_binding` (Vulkan, `zest_vulkan.h:6304`). Instance layers bind binding 0 and mesh layers put the instances at binding 1 (`zest.h:19950`). Indices are always 32-bit.
- `draw_layer_instruction` draws `vertex_count`, `instruction->total_indexes`, 0, `instruction->start_index` (Vulkan). Only TimelineFX calls it.
- **Indirect draws:** `offset` is a command index. The byte offset is `memory_offset + offset * stride` (`zest_vulkan.h:6260`). The command structs are at `zest.h:3882-3895`.
- `insert_compute_image_barrier` is called only from user code between per-mip dispatches (`zest-compute-tests.cpp:271`).

**The debug overlay**
- `zest_context_init_flag_debug_overlay` makes `zest__initialise_context` call the two overlay hooks (`zest.h:11779-11780`) and build "Debug Overlay Pipeline" (`zest.h:11781-11794`): four attributes, the device layout, ImGui blend, no cull, no depth.
- **On Metal today the stubs return a zero handle and `zest_SetPipelineShaders` asserts (`zest.h:12879`), so an overlay context can't be created.**
- Vulkan's overlay GLSL is `static` in `zest_vulkan.h` (`zest_vulkan.h:58`, `zest_vulkan.h:90`), wrapped by `ZEST_GLSL` (`zest_vulkan.h:53`) and created with `zest_CreateShader` (`zest_vulkan.h:1696`).

**The minimal template**
- It never defines `ZEST_METAL_IMPLEMENTATION` (`zest-minimal-template.cpp:1-5`; compare `zest-clear-screen.cpp:6-8`). So `--metal` reaches `example_platform.h:78-82`, which prints that Metal isn't compiled in.
- `example_platform.h` already parses `--metal` (`:54-57`), prepares validation (`:73-77`) and creates the device (`:100-104`). CMake already marks the main file (`examples/SDL2/CMakeLists.txt:192`).
- Its draw is unconditional every frame (`zest-minimal-template.cpp:50-63`). Its comments name only `--vulkan` and `--d3d12` (`:18`, `:124`).

**Tests**
- `metal_pending_tests[]` holds 55 (`zest-tests.cpp:230-289`). M4's done line names the 18 in `zest-pipeline-tests.cpp` and the two "Layer Test" ones.
- **"Shader Platform Macro" requires `ZEST_VULKAN`** on every platform that isn't D3D12 (`zest-pipeline-tests.cpp:1279-1293`, `:1309`).
- **"Pipeline Test State Culling" skips `zest_cull_mode_front_and_back` on D3D12 only** (`zest-pipeline-tests.cpp:164-168`).
- `__APPLE__` already drops line strip, triangle fan and triangle strip from "State Topology" (`zest-pipeline-tests.cpp:194-218`), and point mode from "State Polygon Mode" and "State Rasterization" (`:267-270`, `:503-505`).
- "State Vertex Input" uses `r16g16b16_sfloat`, which D3D12 skips (`:340-341`).
- "Depth Bias" bakes -16 and calls `zest_cmd_SetDepthBias(-16, 0, 0)` after binding (`:899-903`, `:871`).
- "Indirect Draw" draws with offset 1, count 2 and stride 20, then offset 1, count 1 and stride 16 (`:969-1010`).
- "Layer Test Instance Draw" dispatches `image_verify.comp` in a compute pass (`zest-layer-tests.cpp:265-345`). "Resource Test Stride Aligned Buffers" dispatches five times in one pass with pushes in between (`zest-resource-management-tests.cpp:3077-3275`).
- `RunSuite` creates its first context with the default create info (`zest-tests.cpp:447`) before `InitialiseTests` registers anything (`:449`), and the first test runs on that context.

**Wiring to mirror, from D3D12**

| What | Where D3D12 does it | Its test |
| --- | --- | --- |
| Compile and validate | `zest_dx12.h:7677-7736` | "D3D12 Shader Compile", `zest-d3d12-tests.cpp:1475` |
| Pipeline backend, build, cleanup | `zest_dx12.h:4670-4674`, `:4971-5046` | "D3D12 Pipeline State Translation" `:1584`, "D3D12 Pipeline Cache" `:1789` |
| A cache under a lock, built outside it | `zest__dx_cached_pipeline_state`, `zest_dx12.h:4892-4935`, lock at `:352` | |
| Bind state, flush, invalidate | `zest_dx12.h:260-280`, `:7280-7347`, `:7226-7237` | "D3D12 Lazy Binding Order" `:2046` |
| Compute and the immediate compute hooks | `zest_dx12.h:5047-5095`, `:7251-7278`, `:7494-7579`, `:3870-3899` | "D3D12 Immediate Compute" `:2297`, "D3D12 Compute Reads Descriptors" `:2423` |
| Indirect draws | `zest_dx12.h:7600-7658` | "D3D12 Indirect Signatures" `:5788` |
| Overlay | `zest_dx12.h:149-200`, `:5096-5119` | "D3D12 Debug Overlay" `:3531` |

`zest_dx12.h` is the structural model only.

## [Probe_] What the SDK headers say (macOS SDK 26.2 on the Mac, 2026-11-23)

- **Indirect arguments:** `MTLDrawPrimitivesIndirectArguments` is `vertexCount, instanceCount, vertexStart, baseInstance`. `MTLDrawIndexedPrimitivesIndirectArguments` is `indexCount, instanceCount, indexStart, baseVertex` (int32), `baseInstance` (`MTLRenderCommandEncoder.h:73-86`). That is Vulkan's order, which settles the design's one unverified row about draws.
- **Primitive types:** point, line, line strip, triangle, triangle strip (`MTLRenderCommandEncoder.h:31-35`). No fan, no adjacency, no patch list.
- **Fill modes:** fill and lines (`MTLRenderCommandEncoder.h:69-70`). No points.
- **Encoder state:** `setFrontFacingWinding:`, `setCullMode:`, `setDepthClipMode:`, `setDepthBias:slopeScale:clamp:`, `setTriangleFillMode:`, `setBlendColorRed:green:blue:alpha:`, `setStencilReferenceValue:` (`MTLRenderCommandEncoder.h:290-462`).
- **Compute:** an encoder's dispatches are serial by default, "executed in dispatched order". `memoryBarrierWithScope:` "on a serial encoder is allowed, but ignored" (`MTLCommandBuffer.h:224-237`, `MTLComputeCommandEncoder.h:315-319`).
- **Vertex formats:** 3-component 8 and 16-bit formats exist (`Char3`, `UChar3Normalized`, `UShort3`, `Short3Normalized`, `Half3`; `MTLVertexDescriptor.h:27-55`), which D3D12 lacks. There are no scaled formats.
- **Math mode:** `mathMode` replaces `fastMathEnabled` from macOS 15, and fast is the default (`MTLLibrary.h:262-314`).
- **Not in the headers:** whether a vertex buffer's stride must be a multiple of 4, and whether Metal restarts a strip at an all-ones index. Task 3 finds out the first. The second is a documented limit (Calls below).

## [Decisions_] Decisions for Peter

**Settled 2026-11-23: Peter took the recommendation on all four.** M4 takes indirect draws, the MSAA hook keeps reporting, `RunSuite` makes its first context from the first test's create info, and Metal defines `ZEST_METAL` only, with a Metal case in "Shader Platform Macro".

1. **Indirect draws.** M4's done line names the pipeline tests, which include "Pipeline Test Indirect Draw", and the roadmap puts indirect draws in M5.
   - **Recommended:** M4 takes them (Task 5). One native call per command in a loop, as the design's [Mapping_] row says, with D3D12's stride rule (`zest_dx12.h:7640-7653`). About 40 lines, and the argument structs already match ([Probe_]).
   - Alternative: both hooks keep reporting. The test stays pending until M5, and M4's done line leaves it out.
2. **MSAA.** M3 recommended M4 take `get_msaa_sample_count`. Reading the code reverses that.
   - Vulkan builds every pipeline with one sample whatever the rendering info says (`zest_vulkan.h:4020`), D3D12's hook reports (`zest_dx12.h:6547`), MSAA resolve has its own plan pending (design [Scope_]), and no M4 test uses MSAA.
   - **Recommended:** the hook keeps reporting. Pipelines still take the rendering info's sample count, so the MSAA plan only has to change the hook.
   - Alternative: return the device's highest supported count now. A graph with the MSAA hint then builds pipelines Vulkan can't match, and its resolve still reports at `begin_render_pass` (`zest_metal.h:3759-3761`).
3. **`--test` and a test's own create info.** `--test` runs the test on the suite's first context, which is made with the default create info before any test registers (`zest-tests.cpp:447-449`). So a test with its own create info can only be judged in the whole suite.
   - **Recommended:** `RunSuite` creates its first context after `InitialiseTests`, from the first registered test's create info. About 3 lines in Task 1. It's a shared edit, and it changes what "Empty Graph" runs on in every suite run: it asks for a headless context (`zest-tests.cpp:39`) and gets a windowed one today.
   - Alternative: leave it. The sweep judges those tests in the whole suite, as M3's did.
4. **The platform macro.** The design has Metal define `ZEST_METAL=1` and not `ZEST_VULKAN`. "Shader Platform Macro" requires `ZEST_VULKAN` on every platform that isn't D3D12.
   - **Recommended:** keep the design and give the test a Metal case: `ZEST_METAL` required, `ZEST_VULKAN` and `ZEST_D3D12` rejected. A shader can then tell the two Mac backends apart. Slang picks `ZEST_VULKAN` for any device that isn't D3D12 (`impl_slang.hpp:426`), which M6 changes to match.
   - Alternative: Metal defines both. No test edit, but no shader can tell Metal from Vulkan through `ZEST_VULKAN`.

**Calls made without Peter (check these)**
- **The function cache is keyed by the SPIR-V's bytes**, not by the shader, as binaries reach the backend without `compile_shader`. A hot reload adds an entry per edit until the next reset.
- **A reset empties the function, render pipeline and depth-stencil caches**, where M3's blit objects survive one. Those three grow with the app's content, a reset frees every shader anyway, and D3D12 empties its caches too. The shaderc compiler isn't a Metal object and survives.
- **A compute's pipeline state belongs to the compute**, not to a cache. Computes aren't thrown away on a resize.
- **The overlay's GLSL is copied into `zest_metal.h`.** The strings are `static` in `zest_vulkan.h`, which can't be edited to move them, and a Metal translation unit shouldn't depend on another backend's internals. D3D12 keeps its own HLSL copy the same way.
- **Depth bias.** The template's three values apply while a pipeline with `depth_bias_enable` is bound, until `zest_cmd_SetDepthBias` replaces them in that command list. A pipeline without it draws with no bias. That matches Vulkan for an app that sets the bias after binding (`zest-pipeline-tests.cpp:871`), and D3D12's baked bias for one that doesn't.
- **`insert_compute_image_barrier` records nothing** ([Probe_]): dispatches in one encoder run in order, and the fence orders encoders.
- **A dispatch on a transfer queue reports**, as on Vulkan and D3D12, although a Metal queue could run it.
- **`rasterizer_discard_enable` maps to `rasterizationEnabled`.** Metal has it, D3D12 refuses it, and no setter turns it on.
- **Fast math stays at Metal's default**, as Spike A ran. If a pixel check fails by more than 1 because of it, report rather than change it.
- **Primitive restart is a documented limit.** Metal restarts a strip at an all-ones index *(unverified)*, while Vulkan's pipelines turn restart off (`zest_vulkan.h:3998`). Zest never asks for a restart.

## [Review_] Review Focus

1. **A converted shader reads a resource from somewhere the backend didn't put it:** a region offset, the push constant index or a vertex buffer index. M1's "Metal Bindless Slots" binds the table from the same numbers the backend writes with, so it can't catch a wrong region offset (design [Built_], Tasks 7 to 12). Probe: "Metal Compute Reads Descriptors" reads all nine slots through a converted GLSL kernel, with decoys either side and the table bound by the backend, and must fail with one region's offset moved by 8. "Pipeline Test Vertex Draw" covers vertex buffers and pushes.
2. **Bind state at encoder boundaries.** State recorded before an encoder opens, carried over from the previous one, or left behind by a blit or clear that opened its own. Probes: "Metal Lazy Binding Order" and "Metal Pass Encoder Order".
3. **Cache lifetime and threads.** A resize throws away every context pipeline, a reset frees every shader, a hot reload changes a shader's bytes, and two threads can build at once. Probes: "Metal Pipeline Cache", "Metal Shader Conversion", "Shader Hot Reload Keeps Macros" and the hot reload phase of "Metal Immediate Compute".
4. **An object a batch uses is released before the batch completes.** Probes: the zombie and `MallocScribble` run, and the debug layer's "destroyed while still required" record, which the reader counts (M3 Phase 2).
5. **The Y flip or the winding is wrong one way only.** Probe: "Pipeline Test Winding Coverage" expects exactly 120 pixels for clockwise and none for counter-clockwise, and Spike A compared the same SPIR-V on MoltenVK.

## [Scope_] Out of scope

- GPU timestamps and the five profiler hooks (M7). "GPU Profiler Timestamps" stays pending.
- Multiview (M5). A template with a view mask is a pipeline-creation error, as a pass with one already reports.
- Graphs that cross queues and the aliasing order between them (M5).
- MSAA, per Decision 2.
- Slang on Metal (M6). The Slang tests aren't built on the Mac.
- MSL as a shader input, the MSL text cache, markers and labels (M7).
- User docs and the README line (M7).
- Tessellation and geometry shaders, stencil ops, logic ops and blend constants. The template carries none of the last three.

## [Phase_1] Shaders

### Task 1: Baseline, compiling and the platform macro

**Files:** `zest_metal.h` sections `Backend_structs`, `Type_converters`, `Backend_cleanup_functions`, `Shader_compiling`, `Unavailable_stubs`. `zest-pipeline-tests.cpp:1279-1312` (Decision 4). `zest-tests.cpp:415-449` (Decision 3). `zest-metal-tests.cpp`.

**Interfaces:**
- `#include <shaderc/shaderc.h>` beside the other includes.
- `zest_mtl_device_backend_t` gains `shaderc_compiler_t shaderc_compiler;` and `pthread_mutex_t shader_compiler_lock; //Held only to create the compiler`.
- `ZEST_PRIVATE shaderc_compiler_t zest__mtl_shader_compiler(zest_device device);`
  - It creates the compiler on first use, double-checked with an acquiring load as `zest__mtl_blit_pipeline` reads its count. shaderc compiles on one compiler from several threads.
  - The compiler survives a reset and is released in `zest__mtl_cleanup_device_backend` (`zest_metal.h:1855`).
- `ZEST_PRIVATE shaderc_shader_kind zest__mtl_shaderc_kind(zest_shader_type type);` in `Type_converters`.
- **`compile_shader`** does what `zest_vulkan.h:7255-7299` does, with `ZEST_METAL=1` in place of `ZEST_VULKAN=1`. With Decision 4's alternative it defines both. A failure goes to the log and `ZEST_ALERT`, never to the validation count.
- **`validate_shader`** compiles and discards with the platform macro and no user macros.
- **Decision 4 (recommended):** on Metal `glsl_needs_own` requires `ZEST_METAL`, and `glsl_rejects_other` rejects `ZEST_VULKAN` and `ZEST_D3D12`. The Vulkan and D3D12 sources don't change.
- **Decision 3, if taken:** `RunSuite` creates its first context after `InitialiseTests`, from `tests.tests[0].create_info`.

**Steps:**
- [x] Mac baseline before any edit, Metal on: `zest-tests --platform all`. Expected: the baseline in [Constraints_].
- [x] Implement, replacing `compile_shader` and `validate_shader`. Make the shared edits the decisions settle.
- [x] Test **"Metal Shader Compile"**, mirroring `zest-d3d12-tests.cpp:1475`:
  - A GLSL vertex, fragment and compute shader each compile to a binary that starts with the SPIR-V magic.
  - A macro with a value and one without both reach the source: a shader that `#error`s without them compiles.
  - A broken source returns false, keeps the previous binary, sets `last_error` and leaves the validation count unchanged. The next good compile succeeds and clears `last_error`.
  - Four threads compile 8 shaders each at once, on a device that hasn't compiled yet. All 32 succeed.
- [x] Run each of "Shader Cache Invalidation", "Shader Binary Platform Mismatch", "Shader Platform Macro" and "Shader Hot Reload Keeps Macros" with the pending check off. One that passes with no Metal validation record comes off `metal_pending_tests[]`.
- [x] Break it: leave out `ZEST_METAL` ("Shader Platform Macro" must fail), copy the binary on a failed compile (the keeps-the-binary check), drop the user macros ("Shader Hot Reload Keeps Macros"). Creating the compiler without the lock can't fail reliably; record it.
- [x] Build `zest-tests` and `zest-c-compile-check` on Windows in Debug. Expected: no warning, no error.
- [x] Build and run `--platform metal`, then with `--shader-validation`. Expected: `Metal suite: 81 / 81`, `Metal backend: 37 / 37`.

### Task 2: Conversion and the function cache

**Files:** `zest_metal.h` sections `Backend_structs`, `Other_setup_structs`, `Initialisation_functions` (the reset), `Backend_cleanup_functions`, `Shader_compiling`. `zest-metal-tests.cpp`.

**Interfaces:**
- `#include <spirv_cross/spirv_cross_c.h>`.
- The cache entry:
  ```c
  //A converted shader, which the device keeps until a reset
  typedef struct zest_mtl_shader_function_t {
  	id<MTLLibrary> library;
  	id<MTLFunction> function;
  	zest_uint vertex_input_mask;	//Bit N: the vertex stage reads location N
  	zest_uint local_size[3];		//Threads per threadgroup, compute only
  } zest_mtl_shader_function_t;
  ```
- `zest_mtl_device_backend_t` gains a `zest_map` of entries keyed by `zest_key`, and `pthread_mutex_t pipeline_lock; //Guards the three pipeline caches, never held while converting, compiling or building`.
- `ZEST_PRIVATE zest_bool zest__mtl_shader_function(zest_device device, zest_shader shader, zest_shader_type expected_type, zest_mtl_shader_function_t *out_function, char *out_error, zest_size error_size);`
  - The key is `zest_Hash` of the binary's bytes, seeded with the type. The entry is copied out; its objects are borrowed until a reset.
  - It looks up under the lock. On a miss it converts and compiles outside the lock, then inserts under it. A thread that finds an entry already there releases its own objects and takes the cached ones, as D3D12 does.
  - **Conversion:** Spike A's options exactly ([Settled_]), then the alias replacement. A binary SPIRV-Cross rejects, or one with no entry point of the expected stage, is an error carrying SPIRV-Cross's text.
  - **Decision 7:** a set 0 resource whose array size is a literal is an error that names the array and says to index it with a variable.
  - **Reflection:** the vertex stage's stage inputs give `vertex_input_mask` from their `Location` decorations. The compute stage's `LocalSize` execution mode gives `local_size`, as Spike A read it.
  - **Compile:** `newLibraryWithSource:options:error:` and the function, inside one `@autoreleasepool`, both retained. Metal's error text goes into `out_error`.
- `zest__mtl_reinit_logical_device` and `zest__mtl_cleanup_device_backend` release every entry and empty the map.

**Steps:**
- [x] Implement. Nothing calls the function until Task 3, so the test calls it directly.
- [x] Test **"Metal Shader Conversion"**:
  - **Spike A's check moves into the suite.** All 67 `.vert`, `.frag` and `.comp` files under `examples/` compile through `compile_shader` and convert. That includes the two alias shaders, `scene.frag` in `zest-cascading-shadows` and `deferred_lighting.frag` in `zest-pbr-deferred`.
  - `buffer_write.comp` reflects a local size of 8 8 1 and `stride_check.comp` 64 1 1.
  - A vertex shader with inputs at locations 0, 1 and 3 gives a mask of `0b1011`.
  - The same SPIR-V twice gives one function object, and another binary another. The cache grows by one per distinct binary.
  - A sampler array indexed only by a constant fails with an error naming the array. `.length()` on a bindless storage buffer fails with SPIRV-Cross's text.
  - 16 bytes that start with the SPIR-V magic, as "Shader Binary Platform Mismatch" makes, fail without a crash.
  - `zest_ResetDevice` empties the cache.
  - It prints the time of a first and a second conversion of one shader, for [Cost_].
- [x] Break it: drop the alias replacement (the two alias shaders must fail), skip the constant-index check, reflect no locations, and leave the cache full after a reset. Each must fail the test.
- [x] Build and run `--platform metal`, then with `--shader-validation`. Expected: `Metal suite: 81 / 81`, `Metal backend: 38 / 38`.
- [x] Phase report, with the shared edits listed for Peter's Windows run. Restore the Mac clone.

## [Phase_2] Render pipelines and draws

### Task 3: Pipeline states

**Files:** `zest_metal.h` sections `Backend_structs`, `Type_converters`, `Pipelines`, `Initialisation_functions`, `Backend_cleanup_functions`, `Unavailable_stubs`. `zest-pipeline-tests.cpp:164-168`. `zest-metal-tests.cpp`.

**Interfaces:**
- `#define ZEST__MTL_MAX_VERTEX_BINDINGS 4 //Vertex buffer binding N is buffer index N, and index 4 is the push constants`
- The pipeline backend:
  ```c
  //The pipeline and depth-stencil states are borrowed from the device's caches. The rest is encoder state set when the pipeline is bound.
  typedef struct zest_mtl_pipeline_backend_t {
  	id<MTLRenderPipelineState> pipeline_state;
  	id<MTLDepthStencilState> depth_stencil_state;
  	MTLPrimitiveType primitive_type;
  	MTLCullMode cull_mode;
  	MTLWinding winding;
  	MTLTriangleFillMode fill_mode;
  	MTLDepthClipMode depth_clip_mode;
  	zest_bool depth_bias_enable;
  	float depth_bias[3];			//Constant factor, clamp and slope, from the template
  } zest_mtl_pipeline_backend_t;
  ```
- `zest_mtl_device_backend_t` gains a map of render pipeline states and a map of depth-stencil states, both under `pipeline_lock`, and both released and emptied at a reset and at cleanup.
- `ZEST_PRIVATE zest_bool zest__mtl_pipeline_description(zest_device device, zest_pipeline_template pipeline_template, const zest_rendering_info_t *rendering_info, const zest_mtl_shader_function_t *vertex_function, const zest_mtl_shader_function_t *fragment_function, MTLRenderPipelineDescriptor *descriptor, zest_mtl_pipeline_backend_t *out_backend, zest_key *out_key, char *out_error, zest_size error_size);`
  - It fills the descriptor, the encoder state and the cache key, and makes no Metal object, so the translation test reads its output directly.
  - The key is `zest_Hash` of a zeroed key struct: the two function keys, the colour count and formats, the blend attachment, the depth format, the sample count and rasterization. The attributes the vertex shader reads are hashed after it, in location order.
- `ZEST_PRIVATE id<MTLRenderPipelineState> zest__mtl_render_pipeline_state(zest_device device, zest_key key, MTLRenderPipelineDescriptor *descriptor, char *out_error, zest_size error_size);` and `ZEST_PRIVATE id<MTLDepthStencilState> zest__mtl_depth_stencil_state(zest_device device, MTLCompareFunction compare_function, zest_bool write_enabled);`. Both are thread-safe and borrowed until a reset, and both build outside the lock as Task 2's function does.
- **The description:**
  - **Functions:** the vertex function, and the fragment function when the template has one.
  - **Vertex input.** For each location in the vertex function's mask, the template attribute at that location gives the format (`zest__mtl_vertex_format` in `Type_converters`), offset and buffer index (its binding). The binding gives the stride and the step function: per vertex, or per instance with a step rate of 1.
    - An input with no attribute is an error naming the location. An attribute the shader doesn't read is left out.
    - A binding of 4 or above is an error. With `no_vertex_input` there's no vertex descriptor, and a vertex shader with inputs is an error.
    - A format with no Metal vertex format is an error naming it.
    - *(unverified)* This task finds out whether Metal refuses a stride that isn't a multiple of 4. If it does, the description refuses one first, with its own message.
  - **Colour.** The count and formats come from the rendering info through `zest__mtl_pixel_format`. Each attachment takes the one blend attachment: enable, factors and ops by name, and the write mask from `color_write_mask`.
  - **Depth.** `depthAttachmentPixelFormat` is the rendering info's depth format. The stencil format is left invalid, because `begin_render_pass` attaches only the depth plane (`zest_metal.h:3779`).
  - `rasterSampleCount` is the rendering info's sample count, with 0 read as 1. `rasterizationEnabled` is the opposite of `rasterizer_discard_enable`.
  - **Depth-stencil state:** with the test on and a depth attachment, the compare function from `depth_compare_op` and writes from `depth_write_enable`. Otherwise always, with no writes.
  - **Encoder state:**
    - the primitive type from the topology
    - cull none, front or back
    - `MTLWindingClockwise` for `zest_front_face_clockwise`
    - fill or lines
    - depth clip clamp when `depth_clamp_enable` is set
    - `depth_bias_enable` and the template's three values
  - **Errors**, each naming the pipeline and the state: `zest_cull_mode_front_and_back`, `zest_polygon_mode_point`, `zest_topology_triangle_fan`, the four adjacency topologies, `zest_topology_patch_list`, `stencil_test_enable`, `depth_bounds_test_enable`, and a non-zero `view_mask`.
- **`new_pipeline_backend`** allocates the struct zeroed from `context->allocator`, as `zest__vk_new_pipeline_backend` does. **`cleanup_pipeline_backend`** frees it and sets `pipeline->backend` to NULL. It releases nothing, and it accepts a NULL or half-built backend.
- **`build_pipeline`** gets the functions, fills the description, gets both states and fills the backend. On success it clears `zest_pipeline_invalid` and logs "Built pipeline %s", as Vulkan does.
- **"Pipeline Test State Culling"** skips `zest_cull_mode_front_and_back` on Metal as it does on D3D12, and its comment says so for both.

**Steps:**
- [x] Test **"Metal Pipeline State Translation"** (a device, no drawing), mirroring `zest-d3d12-tests.cpp:1584`. It calls `zest__mtl_pipeline_description` directly:
  - **Vertex input:** locations, buffer indices and the per-instance step function come out as the template gives them. A missing attribute fails with an error naming its location. Binding 4 fails. An unread attribute is left out. `r16g16b16_sfloat` gives `MTLVertexFormatHalf3`, and a scaled format fails.
  - **Blend:** `dst_color`, `one_minus_src_color`, `reverse_subtract` and `min` map by name.
  - **Depth:** a depth test with no depth format gives always and no writes. With `zest_format_d32_sfloat` and `less`, it gives less with writes.
  - A sample count of 0 gives 1. Both front faces map straight across. Depth clamp gives `MTLDepthClipModeClamp`.
  - Each state in the error list fails, and its message contains the state's name.
  - A stride of 6: record what Metal does.
- [x] Test **"Metal Pipeline Cache"** (a headless context and a command list from `create_test_command_list`), mirroring `zest-d3d12-tests.cpp:1789`:
  - Two rendering infos that differ only in `render_pass_key` give two `zest_pipeline` objects with one pipeline state. Another blend state gives another.
  - After what a swapchain recreate does (`zest__cleanup_pipelines`, then freeing `cached_pipelines`), the first pipeline state comes back.
  - An unsupported state gives NULL, raises the validation count by exactly 1, and leaves the template invalid.
  - Four threads asking for one new pipeline grow the cache by exactly 1.
  - Two templates with the same depth settings share one depth-stencil state.
  - `zest_ResetDevice` empties both caches.
- [x] Implement, replacing `new_pipeline_backend`, `build_pipeline` and `cleanup_pipeline_backend`. Edit "Pipeline Test State Culling" and build on Windows.
- [x] The nine "Pipeline Test State" tests come off the list if they pass.
- [x] Break it: invert the winding, keep the depth test with no depth attachment, pass a sample count of 0 through, skip the triangle fan check, leave the blend out of the key, and leave the caches full after a reset. Each must fail a test.
- [x] Build and run `--platform metal`, then with `--shader-validation`. Expected: `Metal suite: 90 / 90`, `Metal backend: 40 / 40`.

### Task 4: Lazy binding and draws

**Files:** `zest_metal.h` sections `Backend_structs`, `Bindless_table`, `Internal_Frame_graph_context_functions`, `Frame_graph_context_functions`, `Unavailable_stubs`. `zest-metal-tests.cpp`.

**Interfaces:**
- The bind state:
  ```c
  //The plain fields are what the app asked for, the recorded_ fields what the open encoder holds
  typedef struct zest_mtl_bind_state_t {
  	zest_pipeline pipeline;
  	zest_compute compute;
  	id<MTLRenderPipelineState> recorded_pipeline_state;
  	id<MTLComputePipelineState> recorded_compute_state;
  	zest_buffer vertex_buffers[ZEST__MTL_MAX_VERTEX_BINDINGS];
  	zest_buffer recorded_vertex_buffers[ZEST__MTL_MAX_VERTEX_BINDINGS];
  	zest_buffer index_buffer;
  	char push_data[ZEST_MAX_PUSH_SIZE];
  	zest_bool render_push_dirty;
  	zest_bool compute_push_dirty;
  	float depth_bias[3];			//Constant factor, clamp and slope from zest_cmd_SetDepthBias
  	zest_bool depth_bias_set;		//zest_cmd_SetDepthBias was called in this command list
  	zest_bool depth_bias_dirty;
  	zest_bool reported_missing_pipeline;
  } zest_mtl_bind_state_t;
  ```
- `ZEST_PRIVATE void zest__mtl_invalidate_bind_state(zest_mtl_bind_state_t *state);` clears the recorded fields and marks both pushes and the depth bias dirty. It keeps what the app asked for.
- `zest_mtl_command_list_backend_t` gains `zest_mtl_bind_state_t bind_state;`. `zest__mtl_begin_command_buffer` (`zest_metal.h:3661`) zeroes it, and every encoder that opens invalidates it.
- `ZEST_PRIVATE void zest__mtl_bind_table_to_render_encoder(zest_device device, id<MTLRenderCommandEncoder> encoder);` binds each slot's region at index 5 + slot, with one ranged call per stage, as the test helper `metal_bind_table` does for a compute encoder (`zest-metal-tests.cpp:77-85`). `begin_render_pass` calls it once the encoder is open.
- `ZEST_PRIVATE zest_bool zest__mtl_flush_render_state(const zest_command_list command_list);` runs before every draw:
  1. With no render encoder, or no pipeline bound, it reports through `ZEST_ASSERT_OR_VALIDATE` once per command list ("A draw was recorded with no pipeline bound.") and returns false. The draw is skipped.
  2. When the pipeline state differs from the recorded one, it sets the pipeline state, the depth-stencil state, the cull mode, the winding, the fill mode and the depth clip mode, and marks the depth bias dirty.
  3. When the render push is dirty, `setVertexBytes` and `setFragmentBytes` of the whole 128-byte block at index 4.
  4. For each binding whose buffer differs from the recorded one, `setVertexBuffer:offset:atIndex:` with the pool's buffer and `memory_offset`.
  5. When the bias is dirty, `setDepthBias`: zero when the pipeline has no depth bias, otherwise the values from `zest_cmd_SetDepthBias` if it was called in this command list, or the template's.
- **The hooks:**
  - `bind_pipeline` records the pipeline. A NULL pipeline, or one with no backend, reports.
  - `send_push_constants` copies `min(size, ZEST_MAX_PUSH_SIZE)` bytes to the start of `push_data` and marks both pushes dirty. The layout argument isn't used.
  - `bind_vertex_buffer` records the buffer at `first_binding`. A binding of 4 or above logs a validation error. `bind_index_buffer` records the buffer. `bind_mesh_vertex_buffer` and `bind_mesh_index_buffer` record `layer->vertex_data` at binding 0 and `layer->index_data`.
  - `set_depth_bias` records the three values and marks them set and dirty.
  - `draw` flushes, then draws the pipeline's primitive type with `vertexStart:vertexCount:instanceCount:baseInstance:`.
  - `draw_indexed` flushes, then draws 32-bit indices from the recorded index buffer at `memory_offset + first_index * 4`, with `baseVertex` and `baseInstance`. With no index buffer it reports.
  - `draw_layer_instruction` flushes, then draws `vertex_count` vertices of `instruction->total_indexes` instances from `instruction->start_index`, as Vulkan does.

**Steps:**
- [x] Test **"Metal Lazy Binding Order"** (a headless context), mirroring `zest-d3d12-tests.cpp:2046`:
  - **Order.** One task pushes, binds the vertex and index buffers, sets the viewport and scissor, then binds the pipeline and draws the "Pipeline Test Vertex Draw" quad. The left half reads the tinted colour.
  - **Stride change.** A second pipeline with a stride of 24 draws from the same vertex buffer with no new bind, and reads right.
  - **No push block.** A pipeline whose shaders declare no push block draws after 128 bytes were pushed, with no validation error.
  - **Two draws, two pushes** in one encoder: each draw gets its own colour.
  - **After a blit.** In one batch, a transfer pass runs `zest_cmd_BlitImageMip` between `r8g8b8a8_unorm` and `r16g16b16a16_sfloat`, which opens its own render encoder, and then a render pass draws. The draw's pixels are right.
  - **No pipeline.** A task that only calls `zest_cmd_Draw` raises the validation count by exactly 1, and the target keeps its clear colour.
- [x] Test **"Metal Pass Encoder Order"**, which answers M3's fourth question. One batch, so one command buffer:
  - a transfer pass uploads a 16x16 pattern into image A
  - a render pass draws a full-screen triangle that samples A into image B
  - a transfer pass copies B into a readback buffer
  - The readback holds the pattern.
- [x] Implement, replacing the ten hooks.
- [x] These come off the list if they pass: "Pipeline Test Winding Coverage", "Pipeline Test Vertex Draw", "Pipeline Test Depth Bias", "Pipeline Test ImGui Shaders" and "Layer Test Frame In Flight".
- [x] Break it, each once:
  - don't mark the push dirty on a send
  - keep the recorded fields when an encoder opens
  - don't bind the table at the render encoder's open
  - drop the base vertex
  - never apply the depth bias
  - don't set the winding
  - Each of these must fail a test.
  - Then take out, one at a time, the pass blit encoder's fence wait, its fence update, and the render encoder's wait and update. Record which of the four "Metal Pass Encoder Order" catches.
- [x] Build and run `--platform metal`, then with `--shader-validation`. Expected: `Metal suite: 95 / 95`, `Metal backend: 42 / 42`.

### Task 5: Indirect draws (only if Decision 1 takes them into M4)

**Files:** `zest_metal.h` sections `Frame_graph_context_functions`, `Unavailable_stubs`. `zest-metal-tests.cpp`.

**Interfaces:**
- `draw_indirect` and `draw_indexed_indirect`:
  - The stride must be a multiple of 4 and at least the command's size (`zest.h:3882-3895`). Otherwise they log a validation error and draw nothing, as D3D12 does (`zest_dx12.h:7640-7653`).
  - Then they flush once and make one native call per command:
    - `drawPrimitives:indirectBuffer:indirectBufferOffset:` for the plain form
    - `drawIndexedPrimitives:indexType:indexBuffer:indexBufferOffset:indirectBuffer:indirectBufferOffset:` for the indexed form, with the bound index buffer at its `memory_offset`
    - Command `draw` reads from `memory_offset + (offset + draw) * stride`.

**Steps:**
- [x] Test **"Metal Indirect Draws"**, mirroring `zest-d3d12-tests.cpp:5788`:
  - packed commands and 32-byte strides both draw right
  - a 12-byte stride raises the count by exactly 1 and draws nothing
  - a `draw_count` of 0 draws nothing
- [x] Implement, replacing the two hooks. "Pipeline Test Indirect Draw" comes off the list if it passes.
- [x] Break it: read `offset` as bytes ("Pipeline Test Indirect Draw" must fail), draw only the first command, skip the stride check.
- [x] Build and run `--platform metal`, then with `--shader-validation`. Expected: `Metal suite: 96 / 96`, `Metal backend: 43 / 43`.
- [x] Phase report, with the shared edits listed. Restore the Mac clone.

## [Phase_3] Compute

### Task 6: Compute pipelines, compute passes and immediate compute

**Files:** `zest_metal.h` sections `Backend_structs`, `Command_pools`, `Pipelines`, `Internal_Frame_graph_context_functions`, `Frame_graph_context_functions`, `Unavailable_stubs`. `zest-metal-tests.cpp`.

**Interfaces:**
- The compute backend:
  ```c
  typedef struct zest_mtl_compute_backend_t {
  	id<MTLComputePipelineState> pipeline_state;		//Owned, and released with the compute
  	MTLSize threads_per_threadgroup;				//The shader's local size
  } zest_mtl_compute_backend_t;
  ```
- **`new_compute_backend`** allocates the struct zeroed from `device->allocator`. **`cleanup_compute_backend`** releases the pipeline state, frees the struct and sets `compute->backend` to NULL. It accepts a NULL backend.
- **`finish_compute`** gets the compute stage's function (Task 2), creates the pipeline state in an autorelease pool, retains it, and keeps the local size. Errors are pipeline-creation errors.
- `zest_mtl_command_list_backend_t` gains `id<MTLComputeCommandEncoder> compute_encoder; //Open from a pass's first dispatch until the pass ends or a copy needs the blit encoder`.
- `ZEST_PRIVATE id<MTLComputeCommandEncoder> zest__mtl_pass_compute_encoder(const zest_command_list command_list);`
  - It returns the open one. Otherwise it ends an open blit encoder, opens one in an autorelease pool, retains it, waits on the fence, binds the table and invalidates the bind state.
- `ZEST_PRIVATE void zest__mtl_bind_table_to_compute_encoder(zest_device device, id<MTLComputeCommandEncoder> encoder);` is Task 4's binder for a compute encoder. The test helper `metal_bind_table` then calls it.
  - With a render encoder open it reports and returns nil.
- `ZEST_PRIVATE void zest__mtl_end_pass_compute_encoder(const zest_command_list command_list);` updates the fence, ends the encoder and releases it.
- **Who ends it:**
  - `zest__mtl_pass_blit_encoder` ends an open compute encoder before it opens its own.
  - `release_barrier` and `end_command_buffer` end both encoders.
  - `zest__mtl_blit_image_mip` and `zest__mtl_image_clear` end both before their render encoder.
- `ZEST_PRIVATE zest_bool zest__mtl_flush_compute_state(id<MTLComputeCommandEncoder> encoder, zest_mtl_bind_state_t *state);`
  - With no compute bound it reports and returns false.
  - It sets the pipeline state when it changed, and `setBytes` of the 128-byte block at index 4 when the compute push is dirty.
- **`bind_compute_pipeline`** records the compute. **`dispatch_compute`** reports on a transfer queue list, then flushes and calls `dispatchThreadgroups:threadsPerThreadgroup:` with the compute's local size.
- **`insert_compute_image_barrier`** records nothing, with a one-line comment saying why.
- **Immediate queues:**
  - `zest_mtl_queue_backend_t` gains `zest_mtl_bind_state_t bind_state;` and `id<MTLComputeCommandEncoder> compute_encoder;`. `zest__mtl_imm_begin_command_buffer` (`zest_metal.h:2122`) zeroes the bind state.
  - `zest__mtl_queue_compute_encoder` mirrors `zest__mtl_queue_blit_encoder`, and each ends the other's encoder first. `zest__mtl_end_queue_encoder` ends both, so M3's immediate clears, blits and resolves already end a compute encoder.
  - `imm_send_push_constants` copies as `send_push_constants` does.
  - `imm_bind_compute_pipeline` records the compute and sets `queue->last_bound_compute`.
  - `imm_dispatch_compute` returns false through `ZEST_ASSERT_OR_VALIDATE` with Vulkan's message when nothing is bound, and reports on a transfer queue. Otherwise it flushes, dispatches and returns true.

**Steps:**
- [x] Test **"Metal Immediate Compute"**, mirroring `zest-d3d12-tests.cpp:2297`:
  - `buffer_write.comp` on the graphics queue and then the compute queue, with the push sent before the bind as `pbr_functions.cpp` does. The first 64 elements hold their index.
  - A dispatch with nothing bound returns false and raises the count by exactly 1. A dispatch on the transfer queue is refused.
  - **Hot reload:** the shader loads from a temporary copy of `buffer_write.comp` with hot reload on. The file is rewritten to store `index + 1000` and removed at the end. `zest_CheckShaderHotReload` returns 1, and the next dispatch through the same compute handle writes the new values.
- [x] Test **"Metal Compute Reads Descriptors"**, mirroring `zest-d3d12-tests.cpp:2423`, for Review Focus 1. One GLSL kernel reads:
  - a sampler, and 2D, cube, array, 3D and cube array textures (slots 0 to 4 and 8)
  - a raw and a structured storage buffer (slot 5)
  - a uniform buffer (slot 7)
  - It writes one mip of a storage image through slot 6, from `zest_AcquireImageMipIndexes`.
  - Each resource sits at an index with a decoy of its kind on either side. The results are exact, and the storage image's other mip keeps its upload.
- [x] "Metal Pass Encoder Order" gains a compute pass that writes a buffer, followed by a transfer pass that copies it out.
- [x] Implement, replacing the nine hooks.
- [x] These come off the list if they pass: "Resource Test Compute Shader Resources", "Resource Test Stride Aligned Buffers" and "Layer Test Instance Draw".
- [x] Break it, each once. These must fail a test:
  - dispatch with a local size of 1
  - don't bind the table when a compute encoder opens
  - move one region's offset by 8
  - don't end the compute encoder in `release_barrier`
  - don't end it when the blit encoder opens
  - release nothing in `cleanup_compute_backend`
  - Then take out the compute encoder's fence wait, and its update, and record whether "Metal Pass Encoder Order" catches each.
- [x] Build and run `--platform metal`, with `--shader-validation`, and once with `NSZombieEnabled=YES MallocScribble=1`. Expected: `Metal suite: 99 / 99`, or 98 without Decision 1, `Metal backend: 45 / 45`, or 44, and no zombie report. (The last runs gave 44 / 45 only because the Mac's screen was locked. See As built.)
- [x] Phase report. Restore the Mac clone.

## [Phase_4] Overlay, template and the suite

### Task 7: The debug overlay and the minimal template

**Files:** `zest_metal.h` sections `Shader_code`, `Unavailable_stubs`. `zest-minimal-template.cpp:1-5`, `:18`, `:124`. `zest-metal-tests.cpp`.

**Interfaces:**
- `Shader_code` gains `zest__mtl_shader_db_overlay_vert` and `zest__mtl_shader_db_overlay_frag`: copies of the GLSL at `zest_vulkan.h:58` and `:90`, with the version and extension lines `ZEST_GLSL` adds (`zest_vulkan.h:53`) written out.
- `get_db_overlay_vertex_shader` and `get_db_overlay_fragment_shader` call `zest_CreateShader` with them, named and flagged as `zest_vulkan.h:1696-1704` does.
- The minimal template defines `ZEST_METAL_IMPLEMENTATION` under `ZEST_ENABLE_METAL`, as `zest-clear-screen.cpp:6-8` does. Its two comments name `--metal`.

**Steps:**
- [x] Test **"Metal Debug Overlay"** (windowed, as "Metal Windowed Frames"), mirroring `zest-d3d12-tests.cpp:3531`:
  - The context is created with `zest_context_init_flag_debug_overlay`.
  - 8 frames each call `zest_DrawDebugText` and `zest_DrawDebugRect` before a frame graph that clears the swapchain. After each frame `context->db_overlay.vertex_count` is 0.
  - The overlay pipeline is valid at the end.
  - After `zest_ResetDevice` a new overlay context draws 2 more frames. 0 validation errors.
- [x] Implement, replacing the two hooks. Edit the template, and build it on the Mac and on Windows in Debug.
- [x] Break it: return a zero handle from either hook. The test must fail.
- [x] Run `zest-minimal-template --metal --validate --frames 240` and `--vulkan --validate --frames 240` from the repo root. Expected: `validation errors: 0`, exit 0, and "Built pipeline" for the triangle in the log.

### Task 8: The pending list and the final check

**Files:** `zest-tests.cpp:230-289`. `metal_m4_implementation.md`, `metal_platform_layer_implementation.md`.

**Steps:**
- [x] Sweep every test still pending (`spikes/metal/sweep_pending.py`), each alone with the pending check off. One that passes with no Metal validation record comes off. Without Decision 3, a test with its own create info is judged in the whole suite. Regroup what stays under what it stops at.
- [x] Check the done line: every test in `zest-pipeline-tests.cpp` and `zest-layer-tests.cpp` runs on Metal. List any that doesn't, with the reason.
- [x] The final check on the Mac:
  - every target with Metal on and with Metal off
  - `--platform all`, `--platform metal --shader-validation`, and one zombie and `MallocScribble` run
  - `zest-clear-screen --metal --validate --frames 240` and `zest-minimal-template --metal --validate --frames 240`
  - Expected: the Vulkan lines at the baseline, both Metal lines full, `validation errors: 0`.
- [x] Build `zest-tests`, `zest-c-compile-check` and `zest-minimal-template` on Windows in Debug. Expected: clean.
- [x] Add the As built part to this file: the suite count, the pending groups by milestone, every difference from the plan, the checks that can't fail yet, the shared edits, and the [Cost_] numbers Task 2 measured. Add M4's part to [Built_] in the design document, and mark the [Mapping_] rows this milestone confirms.
- [x] Hand Peter the Windows check, the visual checks and the questions M5's plan needs answered. Restore the Mac clone.

## [Cost_] Per-frame cost

| Path | Vulkan and D3D12 | Metal after M4 |
| --- | --- | --- |
| Per push | Unchanged | A copy of up to 128 bytes into the bind state |
| Per draw | Unchanged | The flush: one pipeline compare, up to 4 vertex buffer compares, and the push and bias flags. Native calls only for what changed: 6 on a pipeline change, two 128-byte `setBytes` on a push, one per changed vertex buffer. No allocation, no lock, no pool. Then one branch on `declares_texture_views`, and under shader validation an atomic load and compare of the view version |
| Per indirect draw call | Unchanged | The flush, then `draw_count` native calls |
| Per dispatch | Unchanged | The same, with the compute state and one 128-byte `setBytes` on a push, and the same branch on `declares_texture_views` |
| Per render encoder | Unchanged | The table: two ranged calls over 9 buffer indices. Under shader validation only, a lock and one `useResources:count:usage:stages:` over the device's texture views at the first draw |
| Per compute encoder | Unchanged | One autorelease pool, one ranged call, one fence wait and one update. Switching between the compute and the blit encoder in one pass costs an encoder each time. Under shader validation only, a lock and one `useResources:count:usage:` at the first dispatch |
| A texture view made or freed | Unchanged | A branch. Under shader validation only, a lock and a push, or a linear search and a swap |
| Pipeline build, per template and rendering info per context | Unchanged | A description, two or three hashes and lookups under `pipeline_lock`. A render pipeline state is created once per distinct description per device |
| A resize | Unchanged | Every context pipeline is rebuilt from the caches: lookups and no compile |
| First use of a shader per device | Unchanged | SPIRV-Cross and Metal's compiler. Measured in Task 2: 2.3 ms for `pbr_texture.frag` once Metal has compiled its MSL before, 309 ms the first time on the machine. A shader already in the cache costs 5 to 6 us |
| Per reaped batch | Unchanged | One `waitUntilCompleted`, which returns at once when the buffer's completed handlers have run. Without the debug layer Zest adds no handlers |
| Shader creation from GLSL | Unchanged | shaderc, as on Vulkan |

**Memory:** the bind state is about 260 bytes, in each command list backend and each immediate queue backend. A pipeline backend is about 56 bytes per pipeline per context, and a compute backend one object and 24 bytes. The device gains three maps, a mutex, the shaderc compiler and its lock, and one library and function per distinct SPIR-V until a reset. The device also gains the texture view list (8 bytes per listed view, and empty without shader validation), its version, its mutex and a flag. Each view backend gains 8 bytes, and the bind state 8. No struct in `zest.h` grows.

## [Testing_] Testing

**Automated, on the Mac (implementer)**
- After every task: build `zest-tests`, `zest-c-compile-check` and `zest-objc-compile-check` with Metal on, then `zest-tests --platform metal` and `--platform metal --shader-validation`.
- At the end of every phase: `zest-tests --platform all`, and a Metal off build with `zest-tests`. Expected: the Vulkan lines at the baseline.
- At the end of Phases 3 and 4: `NSZombieEnabled=YES MallocScribble=1 zest-tests --platform metal`.
- New backend tests, taking `metal_tests[]` from 36 to 46, or 45 without Decision 1: Shader Compile, Shader Conversion, Pipeline State Translation, Pipeline Cache, Lazy Binding Order, Pass Encoder Order, Indirect Draws, Immediate Compute, Compute Reads Descriptors, Debug Overlay.
- The Metal suite: 77, plus what each task and the final sweep take off the list. The final count is recorded, not predicted.

**Peter's test, on Windows, after Phases 1 and 2** (the shared test edits)
- `zest-tests.exe` with the default `--platform all`. Expected: the counts of his last run. M3's plan expected `Dynamic rendering: 136 / 136`, `Legacy render pass: 136 / 136`, `D3D12 suite: 132 / 132`, `D3D12 backend: 103 / 103`, exit 0. The edited tests are "Shader Platform Macro" and "Pipeline Test State Culling", and with Decision 3 the runner.
- After Phase 4: `zest-minimal-template.exe --validate --frames 240` on each backend.

**Visual checks for Peter, on the Mac**
1. Run `zest-minimal-template --metal` from the repo root without `--frames`. The triangle matches the Vulkan one: red at the top, green at the bottom right, blue at the bottom left.
2. Resize the window a few times. Rendering carries on with no hitch on the first frame after each resize, as the pipeline states come from the cache, and no validation output appears.
3. Run it with `--vulkan` and compare. The two windows look the same.

## [Built_] As built

### Phase 1: shaders (2026-11-23)

**What changed**
- `zest_metal.h`:
  - includes shaderc and SPIRV-Cross
  - the device backend gains the shaderc compiler and its lock, the function cache and `pipeline_lock`
  - `zest_mtl_shader_function_t` and its map
  - `compile_shader` and `validate_shader`, and the conversion and its cache
  - the reset and the cleanup release the cache
- `zest-metal-tests.cpp`: "Metal Shader Compile" and "Metal Shader Conversion".
- Shared edits:
  - `zest-pipeline-tests.cpp`: "Shader Platform Macro" has a Metal case (Decision 4).
  - `zest-tests.cpp`: `RunSuite` makes its first context from the first test's create info (Decision 3). Four shader tests came off `metal_pending_tests[]`, and the heading over the rest now says what they wait for.

**Results**
- **The phase-end run on the Mac:**
  - All 22 targets build with Metal on, with no warning from Zest's files.
  - `--platform all`: `Dynamic rendering: 129 / 129`, `Legacy render pass: 129 / 129`, `Metal suite: 81 / 81`, `Metal backend: 38 / 38`, exit 0, no VUID line, no Metal record in the suite's part of the log.
  - `--platform metal --shader-validation`: 81 / 81 and 38 / 38, no record.
  - Metal off: `zest-tests` builds with no warning and runs 129 / 129 on both Vulkan paths.
- Windows: `zest-tests` and `zest-c-compile-check` build in Debug with no warning.
- "Metal Shader Compile": 32 of 32 concurrent compiles gave SPIR-V on a device with no compiler yet.
- "Metal Shader Conversion": 67 of 67 example shaders compile and convert, both alias shaders among them, giving 60 distinct cache entries.
- The four shared shader tests pass on Metal with no record: "Shader Cache Invalidation", "Shader Binary Platform Mismatch", "Shader Platform Macro" and "Shader Hot Reload Keeps Macros".
- Breaking it on purpose:
  - Leaving out `ZEST_METAL` fails "Shader Platform Macro".
  - Copying the binary on a failed compile fails "Metal Shader Compile".
  - Dropping the user macros stops the run in "Shader Hot Reload Keeps Macros": its first `zest_CreateShader` doesn't compile, and the assert there aborts. That left `zest_macro_reload_test.comp` in the Mac's repo root, which was removed.
  - Dropping the alias replacement, skipping the constant index check, reflecting no locations and leaving the cache full after a reset each fail "Metal Shader Conversion".
  - Creating the compiler without its lock can't be made to fail reliably, so no test catches it.
- **[Cost_] numbers:**
  - The first conversion of `pbr_texture.frag` took 309 ms the first time its MSL was compiled on the Mac, and 2.3 ms on the next run.
  - A conversion found in the cache takes 5 to 6 us.
  - The 67 example shaders average 1.9 ms each, from GLSL through shaderc, SPIRV-Cross and Metal's compiler.

**Differences from the plan**
- **The set 0 check reads only the resources the entry point uses.** Reading every declared resource refused `depthpass.frag`, which declares a sized `textureArrays` and never uses it. That matches Spike A's finding that SPIRV-Cross leaves such an array out of the MSL. The check also refuses a set 0 resource that isn't an array at all, which the table can't serve either. Its message names the resource.
- **The vertex input mask covers every location an input takes**: a location per matrix column and per array element, as on Vulkan. A location above 30 is refused, as Metal has 31 vertex attributes. No shader in the repo has a matrix or array input.
- **Helpers the plan didn't name:**
  - `zest__mtl_compile_glsl`, shared by compile and validate
  - `zest__mtl_convert_spirv`, `zest__mtl_compile_msl` and `zest__mtl_conversion_error`, which split the conversion
  - `zest__mtl_execution_model` in `Type_converters`
  - `zest__mtl_release_pipeline_caches`, which the reset and the cleanup call, and which Task 3 extends to the other two caches
- `zest_mtl_shader_function_t` is in `Backend_structs`, not `Other_setup_structs`, as the device struct holds its map.
- Compile failures that leave shaderc with no message say "the compiler gave no error message" where Vulkan says "Unknown compile error".

### Phase 2: render pipelines and draws (2026-11-23)

**What changed**
- `zest_metal.h`:
  - the pipeline backend, the render pipeline and depth-stencil caches, the description, and the build and cleanup hooks
  - converters for vertex formats, blend factors and operations, compare functions, the write mask and primitive types
  - the bind state, the bindless table bound whenever a render encoder opens, the flush, and the ten bind and draw hooks
  - indirect draws
- `zest-metal-tests.cpp`: "Metal Pipeline State Translation", "Metal Pipeline Cache", "Metal Lazy Binding Order", "Metal Pass Encoder Order" and "Metal Indirect Draws".
- Shared edits:
  - `zest-pipeline-tests.cpp`: "Pipeline Test State Culling" skips `zest_cull_mode_front_and_back` on Metal as on D3D12.
  - `zest-tests.cpp`: 15 tests came off `metal_pending_tests[]`, the nine State tests, the five Task 4 names and "Pipeline Test Indirect Draw".

**Results**
- **The phase-end run on the Mac:**
  - All 22 targets build with Metal on, with no warning from Zest's files.
  - `--platform all`: `Dynamic rendering: 129 / 129`, `Legacy render pass: 129 / 129`, `Metal suite: 96 / 96`, `Metal backend: 43 / 43`, exit 0, no VUID line, no Metal record in the suite's part of the log.
  - `--platform metal --shader-validation`: 96 / 96 and 43 / 43, no record, nothing destroyed while required.
  - Metal off: `zest-tests` builds with no warning and runs 129 / 129 on both Vulkan paths.
- Windows: `zest-tests` and `zest-c-compile-check` build in Debug with no warning.
- After each task: 90 / 90 and 40 / 40, then 95 / 95 and 42 / 42, then 96 / 96 and 43 / 43, as planned. Task 4's shader validation run was folded into the phase-end run.
- "Metal Pipeline Cache": four threads asking for one new pipeline state grew the cache by 1.
- "Metal Lazy Binding Order": the shared-state case drew 92 red pixels in the left half and none in the culled right half.
- "Metal Pass Encoder Order": 1024 of 1024 bytes came back as the pattern, through a copy, a sampling render pass and a copy in one batch.
- **A stride of 6:** Metal builds the pipeline and raises no validation error, so the description doesn't refuse one. "Metal Pipeline State Translation" checks that Metal still builds it.
- **Breaking it on purpose.** Each of these fails a test:
  - Task 3: inverting the winding, keeping the depth test with no depth attachment, skipping the triangle fan check, and leaving the caches full after a reset each fail "Metal Pipeline State Translation" or "Metal Pipeline Cache". Leaving the blend out of the key fails "Metal Pipeline Cache". Passing a sample count of 0 through fails all nine State tests and both new ones.
  - Task 4:
    - not marking the push dirty fails "Vertex Draw", "Depth Bias" and "Metal Lazy Binding Order"
    - keeping the recorded fields when an encoder opens crashes "Metal Lazy Binding Order", as Metal then draws with no pipeline state set
    - not binding the table fails "ImGui Shaders" and "Metal Pass Encoder Order"
    - dropping the base vertex fails "Vertex Draw" and "Metal Lazy Binding Order"
    - never applying the depth bias fails "Depth Bias"
    - not setting the winding fails "Winding Coverage" and "Metal Lazy Binding Order"
  - Task 5: reading the offset as bytes fails "Pipeline Test Indirect Draw", drawing only the first command fails it and "Metal Indirect Draws", and skipping the stride check fails "Metal Indirect Draws".
- **The four fences, which no test catches yet.** Taking out the pass blit encoder's fence wait, its update, the render encoder's wait or its update, one at a time, fails nothing, "Metal Pass Encoder Order" included. One run each. The test's copies and draw are small enough to finish in order without the fences. So M3's fourth question is still open: no test is known for a missing update in a frame graph batch.

**Differences from the plan**
- **The flush compares the bound `zest_pipeline`, not its pipeline state.** Cull mode, winding, fill mode and depth clip aren't part of a render pipeline state, so two templates that differ only in them share one. Comparing states would skip a bind that only changes the cull mode. "Metal Lazy Binding Order" has a case for it: two pipelines that share a state, one culling the triangle.
- **Vertex buffers are sent when bound, not when their pointer differs.** Each bind sets a bit in `vertex_buffer_dirty_mask`, and an encoder opening sets them all. A `zest_buffer` can change its offset or pool in place, which a pointer compare would miss. It costs one native call per bind, as on Vulkan. Stride changes need nothing at bind time on Metal, as the stride is in the pipeline state.
- **The depth-stencil cache is a 16-entry array** indexed by compare function and writes, as that is every state it can hold. `zest__mtl_depth_stencil_state` keeps the plan's signature.
- **`zest__mtl_pipeline_description` also fills an `MTLDepthStencilDescriptor`**, as the depth state isn't part of the render pipeline descriptor and the translation test reads it.
- **The key holds the two function objects**, not the function cache's keys. The function cache owns them until the reset that empties both caches.
- **Draws with a count of 0 return before reaching Metal**, as Vulkan draws nothing. Taking the check out raises a Metal validation error, which fails "Metal Lazy Binding Order".
- A draw outside a render pass reports its own message, apart from a draw with no pipeline bound.
- **Helpers the plan didn't name:** `zest__mtl_pipeline_shader_function`, `zest__mtl_vertex_description` and `zest__mtl_draw_indirect_commands`.
- "Metal Pass Encoder Order" uploads zeros into the image the first pass copies into, as an image needs a valid layout before it has a sampled index.

### Phase 3: compute (2026-11-23, finished 2026-11-24)

**What changed**
- `zest_metal.h`:
  - the compute backend, with `new_compute_backend`, `cleanup_compute_backend` and `finish_compute`
  - a pass compute encoder that takes turns with the pass blit encoder, ended by `release_barrier`, `end_command_buffer`, the mip blit and the image clear
  - a queue compute encoder that takes turns with the queue blit encoder, both ended by `zest__mtl_end_queue_encoder`
  - the bindless table bound whenever a compute encoder opens, the compute flush, and the nine compute hooks
  - **The device's texture view list,** kept only under shader validation:
    - the fields `texture_views`, `texture_view_version`, `texture_view_lock` and `declares_texture_views`
    - `zest__mtl_list_texture_view` adds a view when it's made, and `zest__mtl_unlist_texture_view` removes it when it's freed
    - the compute flush and the render flush hand the list to `useResources` through `zest__mtl_declare_texture_views_to_compute_encoder` and `zest__mtl_declare_texture_views_to_render_encoder`
    - See "Shader validation and texture views" below.
- `zest-metal-tests.cpp`: "Metal Immediate Compute", "Metal Compute Reads Descriptors", and "Metal Pass Encoder Order" rebuilt around six passes. `metal_bind_table` calls the backend's binder.
  - **"Metal Compute Reads Descriptors"** also reads texel (2, 5) of a plain texture's mip 1 through that mip's view, from an index `zest_AcquireImageMipIndexes` gave.
    - It checks that the list gains the five mip views under shader validation (none without it), and is back where it started once they're freed.
    - Its pass dispatches to the decoy storage image, copies, then dispatches through the mip 1 view, so the view's write runs on a second compute encoder.
  - **"Metal Pass Encoder Order"'s render pass** samples the copy through its mip 0 view. The pattern and the copy have 2 mips, as `zest_cmd_CopyImageMip` needs the mip counts to match.
- `zest-tests.cpp`: "Resource Test Compute Shader Resources", "Resource Test Stride Aligned Buffers" and "Layer Test Instance Draw" came off `metal_pending_tests[]`.

**Results**
- **The first phase-end run on the Mac (2026-11-23, before the storage view list):**
  - All 22 targets build with Metal on, with no warning from Zest's files.
  - `--platform all`: `Dynamic rendering: 129 / 129`, `Legacy render pass: 129 / 129`, `Metal suite: 99 / 99`, `Metal backend: 45 / 45`, exit 0, no VUID line, no Metal record in the suite's part of the log.
  - `NSZombieEnabled=YES MallocScribble=1`: 99 / 99 and 45 / 45, no zombie report.
  - `--platform metal --shader-validation`: 99 / 99 and 44 / 45, with "Metal Compute Reads Descriptors" failing. See below.
  - Metal off: `zest-tests` builds with no warning and runs 129 / 129 on both Vulkan paths.
- **The phase-end run with option 2's list (2026-11-24):**
  - Every run gave 99 / 99 and 44 / 45, and "Metal Compute Reads Descriptors" passed in all of them.
  - The one failure was "Metal Windowed Frames": the Mac's screen was locked (`IOConsoleLocked = Yes`), so `zest_BeginFrame` got no drawable.
- **The final phase-end run (2026-11-24, option 3, screen unlocked):**
  - All 22 targets build with Metal on, with no warning from Zest's files. One sprintf warning from `examples/libs/stb_image_write.h:776` came out interleaved with another compile's output, which hid its path from the filter.
  - `--platform all`: `Dynamic rendering: 129 / 129`, `Legacy render pass: 129 / 129`, `Metal suite: 99 / 99`, `Metal backend: 45 / 45`, exit 0, no VUID line, no Metal record in the suite's part of the log.
  - `--platform metal --shader-validation`: 99 / 99 and 45 / 45, exit 0, and no "MTLResourceUsage" message.
  - `NSZombieEnabled=YES MallocScribble=1`: 99 / 99 and 45 / 45, no zombie report.
  - Metal off: builds with no warning and runs 129 / 129 on both Vulkan paths.
  - In all of these, "Metal Pass Encoder Order" reads 1024 of 1024 bytes through the copy's mip 0 view. "Metal Compute Reads Descriptors" reads texel (2, 5) as 75 78 81 84, as expected, and writes all 64 mip 1 pixels.
  - The Mac's files hashed the same as Windows' with LF endings.
- **Option 3's breaks.** Each fails a test:
  - No declaration at draw: under shader validation, "Metal Pass Encoder Order" reads 4 of 1024 bytes.
  - No declaration at dispatch: under shader validation, "Metal Compute Reads Descriptors" reads the texel as 0 0 0 0 and gets 64 of 64 mip 1 pixels wrong.
  - Views listed without shader validation: "Metal Compute Reads Descriptors" fails its list check in a normal run.
  - The flag never set: under shader validation both tests fail.
  - The view array's cleanup leaving its views listed: under shader validation, "Resource Test Stride Aligned Buffers" aborts on the freed textures.
  - Per-mip views never listed: under shader validation both tests fail.
- Windows: `zest-tests` and `zest-c-compile-check` build in Debug with no warning.
- Five more runs of `--platform metal` on the same build: 99 / 99 and 45 / 45 each time.
- "Metal Compute Reads Descriptors", without shader validation: every slot reads back exactly, the structured sum is 666, mip 1 is written, mip 0 keeps its upload, and no decoy is touched.
- "Metal Immediate Compute": the hot reload rebuilds the compute, and the old pipeline state is released.
- "Metal Pass Encoder Order": six passes in one batch, with 1024 of 1024 bytes and both buffer readbacks right.
- **Breaking it on purpose.** Each of these fails a test:
  - dispatching with a local size of 1, not binding the table when a compute encoder opens, and moving the storage buffer region's offset by 8
  - not ending the compute encoder in `release_barrier`, and not ending it when the blit encoder opens: each aborts "Metal Pass Encoder Order" on Metal's encoding-in-progress assertion. Neither was caught until that test gained a render pass straight after a compute pass and a copy straight after a dispatch in one pass.
  - `cleanup_compute_backend` releasing nothing fails "Metal Immediate Compute", whose hot reload checks the old pipeline state has no owner left. One earlier run of that break aborted after "Resource Test Buffer Grow Contract" instead. It didn't happen again in a rerun or in five unmutated runs, and I found no cause.
- **What no test catches:**
  - Taking out the compute encoder's fence wait or its update fails nothing, as with the blit and render encoders' fences in Phase 2.
  - Without the check that returns early for a dispatch of 0 groups, Metal raises no error. Unlike a draw of nothing, the check only matches Vulkan and saves opening an encoder.

**Shader validation and texture views**
- **What failed.** Under `--shader-validation` the kernel's `imageStore` to mip 1 writes nothing, and Metal records "MTLResourceUsage flags mismatch or missing for texture executing kernel function: main0", at the store's line in the converted MSL.
- **Probes**, each a single run with the store's result read back:
  - **Fail:** a write through any view of a heap texture fails: mip 1's view, mip 0's view, and a view made of a plain storage image. M1's hand-written kernel fails the same way through the mip view. The kernel, the dispatch path, `newFunctionWithName:constantValues:`, the push size, and frame graph against immediate command buffers all make no difference.
  - **Pass:** writes to a heap texture itself, to a standalone texture, and to a view of a standalone texture.
  - **Fixes:** adding the view itself to the residency set fixes it, as does declaring it with `useResource:usage:`.
  - Every Zest image texture comes from a heap, so every per-mip view is affected. Without shader validation the write works.
- **The residency set was tried first.** Adding every view that isn't its image's own texture to the residency set fixed the write. But "Metal Device Lifecycle" and "Metal Device Reset" then failed: Metal's allocated size grew by exactly 2 MB per device lifecycle. A probe showed the bookkeeping balanced. One view per device, the default cube image's view, went in and came out, and the set held 0 allocations at cleanup. So the growth is unexplained, and the change was reverted.
- **Sampled views are affected too.** A probe sampled mip 1's view of a plain texture image in the same kernel:
  - Under shader validation it read 0, 0, 0, with the same message.
  - Without shader validation it read 0.55 0.56 0.58, mip 1's pattern.
  - So shader validation drops reads and writes through any view of a heap texture that the encoder hasn't declared. Without shader validation they work.
- **Peter first chose a list of storage views declared by compute encoders (option 2, 2026-11-24).** It fixed the failing write. The sampled probe then showed that list was too narrow, so Peter chose option 3 the same day. Option 2's list, test changes and breaks carried over into option 3.
- **Option 3: every view, every encoder, only under shader validation.**
  - **When it's on:** `declares_texture_views` is set once, as the device is made, from `shader_validation || launcher_shader_validation`. So an `MTL_SHADER_VALIDATION` from the launcher counts even when the device didn't ask for validation.
  - **What's listed:** with the flag set, every view whose texture isn't its image's own goes on the device's list, and comes off when it's freed. That covers per-mip arrays, single views, swizzled default views, and transient per-mip views from the frame graph's allocator. Without the flag nothing is listed.
  - **When it's declared:** the render flush and the compute flush each test the flag, then compare the bind state's declared version with the device's. A new encoder resets the bind state's version to 0, so its first draw or dispatch declares the whole list with Read and Write usage. Render encoders declare it for the vertex and fragment stages. It's declared again within an encoder only if a view is made or freed after the encoder's last declaration, as when a task asks for a transient image's mip views between draws or dispatches.
  - **What it costs without shader validation:** one branch on the flag per draw and per dispatch, and one per view made or freed. There's no lock, no call and no list.
  - Declaring Write on textures without `MTLTextureUsageShaderWrite` raises nothing from API validation.
- **Breaks from option 2's runs:**
  - No declaration at dispatch: under shader validation, "Metal Compute Reads Descriptors" failed with 64 of 64 mip 1 pixels wrong.
  - The view array's cleanup leaving its views listed: the next dispatch in the suite handed freed textures to `useResources`, and "Resource Test Stride Aligned Buffers" aborted with an `NSInvalidArgumentException`. So the suite makes and frees per-mip views of its own.
  - **A new encoder keeping the old declaration wasn't caught,** even with the view's write on a second encoder. A trace showed the second encoder dispatching with no declaration of its own, and the write still landed under shader validation with no message. So shader validation accepts a declaration from an earlier encoder in the same command buffer. The reset stays, as `useResource` is documented to cover one pass.

**Differences from the plan**
- `zest__mtl_flush_compute_state` takes the device first, as its report needs one.
- `zest__mtl_table_regions` gives both table binders the slot regions.
- `zest__mtl_end_queue_encoder` ends whichever encoder is open, and each queue encoder getter calls it before opening its own.
- The transfer queue check reads the queue family's types, so a transfer request that lands on the compute or graphics family can dispatch, as on D3D12.
- **"Metal Pass Encoder Order" is six passes in one chain.** A compute pass with no dependency on the image passes put a transfer pass and a compute pass in one wave. That made the wave async and split all five passes into five batches. Now each pass reads what the one before it made, which keeps each wave to one queue type. The plan's compute pass and transfer copy are passes 2 and 6. Pass 5 dispatches and copies in one callback, and the render pass follows a compute pass.
- "Metal Immediate Compute" fails rather than skips when the transfer queue it's given can run compute, as Metal always has a transfer family.

### Phase 4: overlay, template and the suite (2026-11-24, on top of commit f6ddb18 with Phases 1 to 3 uncommitted, built and run on the Mac over SSH)

**What changed**
- `zest_metal.h`:
  - `Shader_code` has the overlay's GLSL as `zest__mtl_shader_db_overlay_vert` and `zest__mtl_shader_db_overlay_frag`. `get_db_overlay_vertex_shader` and `get_db_overlay_fragment_shader` moved from `Unavailable_stubs` to the end of `Shader_compiling`, and call `zest_CreateShader` as Vulkan's do.
  - `zest__mtl_reap_queue_batches` counts a batch finished when its status reads completed or its last event has been seen, and calls `waitUntilCompleted` on every batch it releases. See "A drawable texture freed while Metal's debug layer still uses it".
- `zest-metal-tests.cpp`:
  - "Metal Debug Overlay". Beyond D3D12's checks, it finds the context's pipeline built from the overlay's template and checks it has a Metal pipeline state, before and after `zest_ResetDevice`.
  - "Metal Transient Images" checks Metal's allocated size across a second context, not against the size before the first. See "256 KB that Metal keeps".
- Shared edits:
  - `zest-minimal-template.cpp` defines `ZEST_METAL_IMPLEMENTATION` under `ZEST_ENABLE_METAL`, and its two comments name `--metal`. The comment on `ExampleShaderPath` says Metal reads `shaders/glsl` too.
  - `zest-tests.cpp`: 25 tests came off `metal_pending_tests[]`, which keeps 8 in two groups.

**The sweep** (`spikes/metal/sweep_pending.py`, each test alone with the pending check off)
- These 25 passed with no Metal validation record and came off: "Buffer Read/Write", "Image Write/Read", "Multi Queue Sync", the 4 "Stress Test" tests, the 10 pending "Compute Test" tests, "Frame Graph Transfer Image Output", "Frame Graph Transfer Queue Mip Copy", "Multi Queue Cached Handoff", "Cached Imported Image", "Cached Imported Image Async", "Multi Queue Imported Handback", "Frame Graph Transient Structured Buffer" and "Device Reset Memory Stability".
- "GPU Profiler Timestamps" fails on `create_gpu_profiler_backend`. Its context no longer stops at the overlay's hooks, so its group's comment now names only GPU timestamps.
- The 7 Slang tests aren't built on the Mac.

**The done line**
- All 23 tests in `zest-pipeline-tests.cpp` and `zest-layer-tests.cpp` passed in the Metal suite of the final `--platform all` run and of the `--shader-validation` run: the 14 "Pipeline Test" tests, the 4 "Shader" tests and the 5 "Layer Test" tests. None is left out.
- `zest-minimal-template --metal --validate --frames 240` draws its triangle: exit 0, `validation errors: 0`, "Built pipeline Triangle Pipeline" in the log.

**Results** (the Mac's screen was locked throughout, `IOConsoleLocked = Yes`)
- Mac, Metal on: all 22 targets build with no warning from Zest's files.
  - `zest-tests --platform all`: `Dynamic rendering: 129 / 129`, `Legacy render pass: 129 / 129`, `Metal suite: 124 / 124`, `Metal backend: 45 / 46`, no VUID line, no Metal record in the suite's part of the log. The one failure is "Metal Windowed Frames", on the locked screen.
  - `--platform metal --shader-validation`: `Metal suite: 123 / 124`, `Metal backend: 45 / 46`, nothing destroyed while required. The suite's failure is "Compute Test Immediate Execute No Wait", from a race in `zest.h` (below).
  - `NSZombieEnabled=YES MallocScribble=1`: 124 / 124 and 45 / 46, no zombie report.
  - `zest-clear-screen --metal --validate --frames 240` and `zest-minimal-template --metal --validate --frames 240`: exit 0, `validation errors: 0`, nothing destroyed while required. The template with `--vulkan`: exit 0, `validation errors: 0`.
- Mac, Metal off: all 21 targets build with no warning from Zest's files. `zest-tests` gives 129 / 129 and 129 / 129, no VUID line.
- Windows: `zest-tests`, `zest-c-compile-check` and `zest-minimal-template` build in Debug with no warning. The suites were not run.
- The Mac's files hashed the same as Windows' with LF endings.
- "Metal Debug Overlay" passed in every run, alone and in the suites. Breaks:
  - A zero handle from either hook aborts the run in "Metal Debug Overlay", on `zest_SetPipelineShaders`'s assert that both shaders share a store (`zest.h:12876`).
  - The vertex hook returning the fragment shader fails the test without aborting: the build reports "Shader DB Overlay Vertex has no vertex entry point", the template is flagged invalid, and the context holds no overlay pipeline.

**A drawable texture freed while Metal's debug layer still uses it**
- **What fails.** The template under `--metal --validate` gave the debug layer's "being destroyed while still required to be alive by the command buffer" record for a `CAMetalLayer Display Drawable` texture in 7 of 18 runs. In 70 later runs it crashed 3 times instead, with SIGSEGV, SIGABRT or SIGTRAP. `zest-clear-screen` gave nothing in 8 runs. Without `--validate`, 40 runs of 1000 frames all exited cleanly. The screen was locked throughout.
- **Where.** With `MTL_DEBUG_LAYER_ERROR_MODE=assert`, the crash report shows the texture dealloc inside `-[CAMetalLayer nextDrawable]`: the layer's cleanup releases an old drawable, which releases its texture for the last time. Each crash report shows the same race from the other side: on Metal's completion queue, `-[MTLDebugCommandBuffer preCompletionHandlers]` calls `removeUsedRenderTarget:` on the texture, which is freed or being freed.
- **Root cause, in M3's `zest__mtl_reap_queue_batches`.** It treated a batch whose `status` read Completed as finished, and released the drawable texture it held without calling `waitUntilCompleted`. Only a batch whose event had been seen but whose status wasn't Completed was waited for.
  - `probe_wait_after_handlers.mm`, 20 repeats with and without the debug layer: a command buffer reads Completed before its completed handlers have run (20 of 20), and `waitUntilCompleted` returns only after they have (0 of 20 early).
  - The debug layer's per-buffer bookkeeping on render targets runs in those handlers. When the reap releases the texture between the status flip and the handlers, the drawable holds its last reference, and the layer's next `nextDrawable` can free it while the handlers still use it.
  - This matches M3's rule: an object freed once `waitUntilCompleted` has returned raises nothing. The reap skipped the wait in one of its two paths.
- **The fix, which Peter took (2026-11-24):** the reap counts a batch finished when its status reads completed or its event has been seen, and always calls `waitUntilCompleted` before releasing it. As a probe, the template ran 70 times with it, 30 runs of 240 frames and 40 of 1000, with no failure, where the unchanged code failed 3 of 70. Built in, the template ran 30 runs of 240 frames and 30 of 1000, and `zest-clear-screen` 10, all with no failure and nothing destroyed while required.

**A race in `zest.h`**
- Under shader validation "Compute Test Immediate Execute No Wait" fails every time, 15 of 15 alone: "Invalid device store ... length:1048576, resident:No". Without shader validation it passes, as its three graphs write the same values.
- A probe labelled every buffer the residency set takes and logged the table writes:
  - The three command graphs each wrote storage buffer index 0, pointing it at a new 1 MB arena of their own, before the first had run.
  - The bad stores were in the logs of the first two graphs' command buffers, into the third graph's arena, which joined the residency set after those two were committed.
- So the first two kernels wrote into the third graph's arena. `zest_FlushFrameGraph` releases a command graph's transient bindless indexes straight after the flush (`zest.h:17169-17175`), whether or not it waited. These graphs flush with no signal timeline, so it can't wait. Its own warning there says arenas and deferred resources can't be recycled then, but the indexes are.
- Every backend has the race, and only Metal's shader validation reports it. `zest.h` isn't this plan's to edit, so nothing changed. A fix would move those indexes to the context's deferred list for the frame in flight when the flush couldn't wait, as the arenas already are.

**256 KB that Metal keeps**
- With the 25 tests in the suite, "Metal Transient Images" failed in every run of the backend list after the suite: Metal's allocated size ended 262,144 bytes above where it was before the context. The residency set and the device's allocation count came back exactly.
- A probe printed the size at each step:
  - The 256 KB appears during the first aliasing graph's flush and stays after the context goes.
  - It stays after the Zest device is destroyed too: 4,784,128 bytes before the device and 5,046,272 after.
  - Three more contexts that run the aliasing graph each come back to exactly the same size.
  - With only the backend list run, the aliasing graph adds nothing.
- So the 256 KB is Metal's own, made once per process depending on what ran before. A check against the size before the first context can't tell it from a leak.
- The test still checks the residency set and the allocation count against the baseline once the first context has gone. Then a second context runs the transient and aliasing graphs, and Metal's size must come back to where the first context left it. "Metal Windowed Context" treats Metal's size the same way.
- Breaks: an image arena heap taken out of the residency set and the count but never released fails the second context's check, 1 MB up. A heap left in the residency set fails the count check after the first context.

**Differences from the plan**
- The reap fix is in M3's code, and the "Metal Transient Images" change is to an M3 test. Neither was in the plan.
- The overlay's fragment shader is named "DB Overlay Fragment", as D3D12's is. Vulkan's says "fragment".
- The template's comment on `ExampleShaderPath`, which the plan didn't name, changed with the two it did.
- The final check also ran the template with `--vulkan --validate`, and ran both examples on Metal more than once.
- "Metal Windowed Frames" couldn't pass in any Phase 4 run, as the Mac's screen stayed locked.

### M4 as a whole

**Counts**

| | Before M4 | After M4 |
| --- | --- | --- |
| `Metal suite` | 77 / 77 | 124 / 124 |
| `Metal backend` | 36 / 36 | 46 tests, 45 / 46 with the screen locked |
| `metal_pending_tests[]` | 55 | 8 |
| Stubs in `zest_metal.h` | 34 | 6 |

The Vulkan lines stayed at 129 / 129 each. 47 tests came off the list: 4 in Phase 1, 15 in Phase 2, 3 in Phase 3 and 25 in Phase 4. The 10 new backend tests are Shader Compile, Shader Conversion, Pipeline State Translation, Pipeline Cache, Lazy Binding Order, Pass Encoder Order, Indirect Draws, Immediate Compute, Compute Reads Descriptors and Debug Overlay. The stubs left are `get_msaa_sample_count` (Decision 2) and the five GPU profiler members (M7).

**The pending list, by the milestone expected to take each test off**

| Tests | File | Stops at | Milestone |
| --- | --- | --- | --- |
| "GPU Profiler Timestamps" | `zest-frame-graph-tests.cpp` | `create_gpu_profiler_backend` | M7 |
| The 7 Slang tests | `zest-slang-tests.cpp` | Not built on the Mac | M6 |

**Shared edits**
- Phase 1: "Shader Platform Macro" has a Metal case (Decision 4), and `RunSuite` makes its first context from the first registered test's create info (Decision 3).
- Phase 2: "Pipeline Test State Culling" skips `zest_cull_mode_front_and_back` on Metal, as on D3D12.
- Phases 1 to 4: `metal_pending_tests[]` in `zest-tests.cpp`, inside `#ifdef ZEST_ENABLE_METAL`.
- Phase 4: `zest-minimal-template.cpp`.
- No change to `zest.h`, `zest_vulkan.h` or `zest_dx12.h`.

**Differences from the plan that later milestones build on.** Each phase's part has the full list.
- The flush compares the bound `zest_pipeline`, not its pipeline state, and sends vertex buffers when they're bound (Phase 2).
- Under shader validation only, every render and compute encoder declares the device's texture views with `useResources` (Phase 3).
- The reap waits for every batch it releases, as a buffer reads as completed before its completed handlers run (Phase 4).

**Checks that can't fail yet**
- Phase 1: creating the shaderc compiler without its lock.
- Phase 2: the pass blit encoder's and the render encoder's fence waits and updates.
- Phase 3: the compute encoder's fence wait and update, the early return for a dispatch of 0 groups, and a new encoder keeping the last one's texture view declaration, which shader validation accepts within one command buffer.
- Phase 4: a zero handle from either overlay hook, which aborts the run on a shared assert rather than failing the test.

**For M5's plan**
1. **The race in `zest.h`.** "Compute Test Immediate Execute No Wait" fails under shader validation until `zest_FlushFrameGraph` stops releasing an unwaited graph's transient bindless indexes. Once `zest.h` changes, the test needs a shader validation run.
2. **M5's done line is already met.** The frame graph, stress, compute and device-reset tests pass on Metal. So M5's plan should start from what the roadmap lists that no test covers yet: multiview, and the aliasing order between queues. It should also check whether the multi-queue tests put work on more than one Metal queue, as they pass without M5's work.
3. **Indirect draws** came into M4 (Decision 1), but the roadmap's M5 row still lists them.
4. **The fences.** No test catches a missing fence wait or update on any encoder. M5's queue work leans on them, so a test that makes a reorder visible would let those breaks fail.
5. **MSAA** has no milestone yet. `get_msaa_sample_count` still reports, and pipelines already take the rendering info's sample count.
