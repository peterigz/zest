# Metal M7: Polish

**Goal:** the last Metal milestone. On Metal, the debug overlay and the profile windows show GPU timings. Xcode's frame capture and Instruments show every command buffer, pass and pipeline by name. A command buffer that fails on the GPU writes a report naming the pass that faulted, and the frames after it carry on. A user can hand the Metal backend a hand-written MSL shader. The user docs, the README and a `metal-backend` skill describe the backend as it is.

**Architecture:**
- **Timestamps.** Apple GPUs sample timestamps only at an encoder's stage boundaries ([Probe_]). So each profiled context owns one `MTLCounterSampleBuffer` per frame in flight.
  - While profiling is on, every encoder a pass opens takes samples: a render encoder four (vertex and fragment start and end), a compute or blit encoder two.
  - `write_timestamp` doesn't write anything. It maps each query to the start of the next encoder, or to the end of the open or last encoder.
  - A profile region in a compute or transfer pass ends the open encoder, so the region's work starts an encoder of its own. A region in a render pass takes its pass's span (Decision 2).
  - `readback_gpu_timestamps` resolves the frame in flight's samples on the CPU and turns the mapping into Zest's begin and end ticks. Ticks are nanoseconds.
- **Names.** Each pass is a debug group on its command buffer, from its acquire barriers to its release barriers, as on D3D12. Its encoders carry its name. Command buffers carry their frame graph's name. Pipeline states and libraries carry their template's, compute's or shader's name.
- **Error reports.** Frame graph batches take their command buffers from a descriptor. With validation on, that descriptor asks for encoder execution status, as D3D12 keeps DRED to validation.
  - A failed batch writes one report: the error's name and text, the frame graph, and with validation each pass's state.
  - Its events are then moved on, so later waits don't run out their timeout (Decision 4).
  - `MTLCommandBufferErrorDeviceRemoved` flags the context's device as lost.
- **MSL input.** A shader options flag marks the source as MSL, with the compute threadgroup size beside it (Decision 3).
  - The Metal backend skips shaderc and SPIRV-Cross and compiles the text.
  - It takes the library's one function of the stage, and reads vertex inputs from the function's reflection.
  - Shared code refuses MSL on Vulkan and D3D12 before their compilers see it.
- **No `msl/` cache** (Decision 1): the probe measured what it would save at about 0.3 ms a shader, and the SPIR-V cache Zest already has skips the GLSL compile.
- **Hot path.** With profiling off, a Metal pass gains a debug group and a label, and an encoder a branch. Vulkan and D3D12 gain nothing per frame. See [Cost_].

**Tech stack:**
- `zest_metal.h`, `examples/SDL2/zest-tests/zest-metal-tests.cpp`.
- Shared: `zest.h` (the MSL option and its refusal, and one stale comment), `zest-tests.cpp` (the pending list goes), `zest-pipeline-tests.cpp` (one test).
- Docs: `README.md`, `docs/concepts/bindless.md`, `docs/concepts/rendering-conventions.md`, `docs/getting-started/installation.md`, `docs/examples/index.md`, `docs/api-reference/index.md`, `docs/api-reference/pipeline.md`, `docs/concepts/device-and-context.md`. Skills: a new `metal-backend`, plus `gpu-profiling` and `pipelines-and-shaders`. Also `CLAUDE.md`'s skill list and the design document.
- Not touched: `zest_vulkan.h`, `zest_dx12.h`, any submodule, any shader file. No new dependency.

**Spec:**
- `metal_platform_layer_implementation.md`: the M7 row of [Roadmap_], the [Mapping_] rows "Markers and names", "GPU timestamps" and "Device loss", the "MSL as a shader input" item under [Decisions_], and [Scope_] ("M7 caches MSL text only").
- D3D12's M7 plan (`docs/superpowers/plans/dx12_m7_polish_implementation.md`, git-ignored, on the Windows machine only) is the model for the profiler hooks, the pass markers, the naming and the removal report. What this plan needs from it is carried here.
- The probes in [Probe_], run on the Mac on 2026-11-25 while writing this plan.
- Peter asked for this plan on 2026-11-25, after M6's sanity check.

## Contents

- [Constraints_] Global Constraints
- [Settled_] Settled
- [State_] Current state
- [Probe_] What the M2 does
- [Decisions_] Decisions for Peter
- [Review_] Review Focus
- [Scope_] Out of scope
- [Phase_1] GPU timestamps
- [Phase_2] Names, markers and error reports
- [Phase_3] MSL as a shader input
- [Phase_4] Docs and the final check
- [Cost_] Per-frame cost
- [Testing_] Testing

## [Constraints_] Global Constraints

Everything in `metal_m6_implementation.md` [Constraints_] holds:
- never commit
- edit on Windows, and build and run on the Mac over `ssh macmini`: the clone is `~/Documents/GitHub/Zest`, `../Zest-Build-Metal` has Metal on and `../Zest-Build` has it off, both Debug with Unix Makefiles and Slang on
- copy edited files to the clone with `scp` and give them LF endings there with `perl -pi -e 's/\r\n/\n/'`, retrying when the Mac's hostname drops
- restore the Mac clone at the end of every phase (`git checkout -- <files>`, `spikes/` stays untracked)
- write a script with a backslash or an apostrophe to a file with the Write tool
- break every new test on purpose once
- a windowed run needs the screen unlocked and the display awake (`caffeinate -u -t 2`, run under `caffeinate -d`, check `IOConsoleLocked`), and two runs never overlap
- a Metal example run counts once it has drawn: apply the draw count patch, and rebuild it with `spikes/metal/m6_make_draw_count_patch.py` once `zest_metal.h` has moved
- follow code-style

What M7 changes:

- **Edits.** Only the files under Tech stack. `zest.h` gets only what Task 6 lists: the MSL option, its refusal, its cache key, and the slot comment. Never `zest_vulkan.h`, `zest_dx12.h` or a submodule. A problem found in any of them goes to Peter with a repro.
- **Line endings.** The C, C++ and docs files are CRLF. `docs/concepts/rendering-conventions.md`, the d3d12-backend skill and the plan files are LF. Check with `file` before scripting an edit.
- **Windows builds:** after Phases 1 and 2, `zest-tests` and `zest-c-compile-check` in Debug. After Phase 3, which edits `zest.h`, every project in Debug. The Windows suites are Peter's.
- **Budget.** Vulkan and D3D12: no new per-frame work. Their shader creation gains one branch. On Metal:
  - With profiling off, the only per-frame additions are a debug group and a label per pass, a label per batch, and a branch per encoder.
  - With profiling on, sample attachments per encoder and one CPU resolve per frame.
  - Nothing on the draw or dispatch path changes.

  See [Cost_].
- **Known flake.** "Resource Test Concurrent Pool Growth" can abort at `zest.h:10584`, the pool growth race in M6's "Found in shared code". Rerun the suite once, and record the abort in As built.
- **Baseline** (Mac, the commit that holds M6, screen unlocked): `--platform all` gives `139 / 139` twice, `Metal suite: 134 / 134`, `Metal backend: 55 / 55`, no VUID line. `--platform metal --shader-validation` gives the same Metal lines. The Metal-off build gives `139 / 139` twice. 26 targets build with Metal on and 25 with it off. Task 1 confirms these.
- **End counts:** `140 / 140` twice, `Metal suite: 136 / 136`, `Metal backend: 58 / 58`, and the Metal-off suites at `140 / 140` twice. Each task gives its own.

## [Settled_] Settled

The implementer doesn't reopen these.

- **From the design, M6 and D3D12's M7:**
  - Markers and labels are always on. D3D12's are (its M7 Decision 3), because captures are taken without validation. The design's [Mapping_] row says the same for Metal.
  - A pass's marker wraps it from before its acquire barriers to after its release barriers, as shared code calls the hooks (`zest.h:17650`, `zest.h:17778`). `sync_only` groups break out before the marker (`zest.h:17644`) and get none.
  - Shared code reads back the frame in flight `zest_BeginFrame` waited for (`zest.h:22363-22370`). A headless context reads back after a command graph's wait (`zest.h:17194-17201`). No backend reads queries the GPU may still be writing.
  - One timebase, as on Vulkan and D3D12.
  - Encoder execution status only with validation, as DRED is on D3D12: Apple says it costs GPU time.
  - "GPU Profiler Timestamps" comes off `metal_pending_tests[]`, which then holds nothing. M6 left it as the only entry (`zest-tests.cpp:235-238`).
- **Counter sampling is at stage boundaries only.** The M2 supports `MTLCounterSamplingPointAtStageBoundary` and none of the other four points ([Probe_]). So there's no `sampleCountersInBuffer:` path, and no code for a GPU that has one.
- **Ticks are nanoseconds.** `timestamp_period_ns` is 1. On the M2, GPU timestamps advance exactly as the CPU timestamp `sampleTimestamps:gpuTimestamp:` gives, which is nanoseconds ([Probe_]). Intel Macs get no calibration code, as they get no code anywhere in the backend (design, "Calls made without Peter").
- **One shared-storage sample buffer of 4096 samples per frame in flight.** 4096 is Metal's limit, 32 KB ([Probe_]). That is at least 1024 render encoders a frame.
  - An encoder that opens with the frame in flight's samples used up takes none, and the context's log gets one line the first time.
  - Shared storage lets `resolveCounterRange:` run on the CPU at readback, with no resolve pass on the GPU.
- **Encoders are split only for profile regions, and only with profiling on.** The fence already orders a queue's encoders (design [Mapping_]), so ending one at a region boundary costs an encoder and one fence wait and update, not overlap.
  - A render encoder is never split. Ending one and opening another with load actions would store and load every attachment on a tile GPU, which would perturb the very timing being measured.

## [State_] Current state (verified 2026-11-25, Windows tree at 9b25e45 with M6 uncommitted)

**Shared profiler**
- The hooks are `zest.h:4796-4800`. The pass markers are `zest.h:4807-4808`, and Metal leaves them unset (`zest_metal.h:1404`).
- `zest__init_gpu_profiler` (`zest.h:22310-22335`) turns profiling off when `create_gpu_profiler_backend` returns false. Otherwise it allocates the query maps, results and smoothing arrays itself. `raw_timestamps[fif]` is the backend's to allocate (D3D12: `zest_dx12.h:8064`). `zest__cleanup_gpu_profiler` (`zest.h:22337-22359`) frees `raw_timestamps`, the maps and the backend struct after the backend's cleanup.
- **Readback** (`zest.h:22361-22495`):
  - It reads back `query_count[current_fif]` queries as pairs, begin at `2i` and end at `2i + 1`.
  - Durations are `end - begin` in `zest_u64`, so an end before its begin wraps to a huge number.
  - The total spans the earliest begin to the latest end, so a zero tick anywhere ruins it.
  - It ends with `reset_gpu_query_pool(profiler, current_fif)` (`zest.h:22492`).
- **Pass timestamps:**
  - The begin is written after `acquire_barrier` and before `begin_render_pass` (`zest.h:17660-17672`).
  - The end is written after the pass's callbacks and overlays, while its render encoder is still open (`zest.h:17744-17748`).
  - Then come `end_render_pass`, `release_barrier` and `end_debug_marker` (`zest.h:17767-17779`).
  - `command_list.gpu_profiler` is set after `begin_command_buffer` (`zest.h:17617`, `zest.h:17634`).
- **Regions:** `zest_BeginGPUProfile` (`zest.h:22497-22524`) writes the begin before it records the region's metadata. `zest_EndGPUProfile` (`zest.h:22526-22539`) writes `query_index + 1`. Regions nest up to `ZEST_MAX_GPU_PROFILE_STACK_DEPTH` (8). `ZEST_MAX_GPU_PROFILE_QUERIES` is 512 (`zest.h:4165`).
- **Who uses regions:** the compute example (`zest-compute-example.cpp:223-225`), render-targets' downsampler and upsampler (`zest-render-targets.cpp:118-135`, `:172-188`) and "GPU Profiler Timestamps" (`zest-frame-graph-tests.cpp:3054-3055`). All of them are in compute passes.
- **"GPU Profiler Timestamps"** (`zest-frame-graph-tests.cpp:3038-3167`, registered at `zest-tests.cpp:169`):
  - Its passes are a transfer "Profile Upload", a compute "Profile Compute" whose region wraps nothing, and a render "Profile Draw" to the swapchain with `zest_EmptyRenderPass`.
  - Every result must be 0 to 1,000,000 us, and the total above 0.

**Metal today**
- `zest_mtl_feature_support_t` (`zest_metal.h:154-165`) already holds `counter_sampling_at_stage` and `counter_sampling_at_draw`. They are read at `:1670-1671` and logged at `:1733`.
- The five profiler members are M6's (`zest_metal.h:5524-5533`): creation logs one line and returns false, and the other four report.
- **Encoders:**
  - `begin_render_pass` opens the render encoder from the context's shared `render_pass_descriptor` (`zest_metal.h:4568-4633`, encoder at `:4610`). It clears the descriptor's textures after.
  - `zest__mtl_pass_blit_encoder` (`:4484-4497`) and `zest__mtl_pass_compute_encoder` (`:4508-4524`) open lazily, each ending the other.
  - `release_barrier` ends both (`:4562-4566`), and `end_render_pass` the render encoder (`:4475-4481`).
  - `zest__mtl_image_clear` (`:5089-5099`) and `zest__mtl_blit_image_mip` (`:5053-5087`) end both pass encoders and record one render encoder each, through `zest__mtl_record_clear` (`:3799`) and `zest__mtl_record_blit` (`:3958`) on the context's descriptor.
  - `end_command_buffer` (`:4535-4539`) closes whatever a failed execution left open.
- **The context backend** (`zest_metal.h:241-246`) holds `render_pass_descriptor`, made in `zest__mtl_initialise_context_backend` (`:2144-2145`) and released in `zest__mtl_cleanup_context_backend` (`:2312`).
- **The command list backend** (`zest_metal.h:275-287`) holds the command buffer, the three encoders, the fence and the bind state. `begin_command_buffer` (`:4449-4472`) zeroes the bind state.
- **Batches:**
  - `set_next_command_buffer` (`:4423-4447`) takes `commandBufferWithUnretainedReferences` with no label.
  - `submit_frame_graph_batch` (`:4635-4696`) signals the frame or wave event, then the slot's ordering event, and commits.
  - `zest__mtl_push_committed_batch` (`:2751-2757`) keeps only the ordering event and its value (`zest_mtl_committed_batch_t`, `:253-258`).
- **Failures:**
  - `zest__mtl_finish_command_buffer` (`:2462-2476`) logs "A Metal command buffer failed: <description>" for a status other than completed, and reads shader validation logs.
  - The reaper (`:2760-2786`) calls it for every finished batch. It runs after every successful timeline wait (`:2414-2433`) and in `set_next_command_buffer`.
  - A batch whose signals never fire leaves its frame's wait running out its timeout. That's 10 s for a command graph in a debug build (`zest.h:17163`), and `fence_wait_timeout_ns` for a frame.
  - The immediate path already moves its timeline on after a failure (`:2510-2513`).
  - Nothing on Metal sets `zest_context_flag_device_lost` (`zest.h:2136`).
- **Labels today:** the queues (`:2133`), the immediate command buffer (`:2486`), the residency set (`:1807`), the bindless table (`:3074`) and the blit pipelines (`:3918`). Also whatever the app names through the three naming hooks (`:5434-5464`). Render pipeline states come from `zest__mtl_render_pipeline_state` (`:3420`), and compute ones from `newComputePipelineStateWithFunction` (`:3550`).
- **Shaders:**
  - `zest__mtl_compile_shader` (`:5152-5180`) runs shaderc with `ZEST_METAL` defined.
  - `zest__mtl_shader_function` (`:5360-5404`) keys the function cache by the binary's bytes, the stage and multiview. It converts with `zest__mtl_convert_spirv` (`:5217-5328`), which reflects the vertex input mask and the compute local size (`:5301-5320`). Then `zest__mtl_compile_msl` (`:5331-5357`) builds the library and takes the function by its entry point name.

**Shared shader code**
- `zest_shader_options_t` (`zest.h:7766-7770`) holds only the macro definitions. `zest_shader_t` (`:7772-7792`) keeps a copy of its options. `zest__copy_shader_options` is at `:13281`.
- `zest__shader_cache_key` (`:13362-13393`) hashes the options' macros, and Vulkan's salt 0 isn't hashed "so its existing cache files keep their keys".
- **`zest_CreateShader`** (`:13425-13460`):
  - It reads the cache when caching is on.
  - A cache file in another format is compiled again.
  - It asserts when `compile_shader` fails.
- `zest_CreateShaderFromFile` (`:13339-13349`) dereferences the handle `zest_CreateShader` returns with no check.
- `zest_CompileShader` (`:13324-13337`) passes its options to `compile_shader`. `zest_ValidateShader` takes no options.
- The binary check (`:13395-13405`) wants the SPIR-V magic on Metal. `zest__report_shader_binary_mismatch` (`:13409-13413`) is the pattern for a validation error over a shader's platform.
- The slot comment above `zest_binding_number_type` (`zest.h:2690-2702`) still says "HLSL (D3D12, future)" and "MSL (Metal, future): mapped to argument buffer ids ([[id(<slot>)]])". `docs/concepts/bindless.md:70-74` says the same.

**Tests**
- `metal_pending_tests[]` and `IsMetalPendingTest` (`zest-tests.cpp:233-246`) hold only "GPU Profiler Timestamps". `RegisterTest` checks them at `:255-257`.
- "Metal GPU Profiling Off" (`zest-metal-tests.cpp:6280-6352`, registered at `:6409`) checks M6's stub, which this milestone replaces.
- The D3D12 removal report test (`zest-d3d12-tests.cpp:4112`) is the model for reading a device's log after a failure: `log_file_size` and `zest_ReadEntireFile`.
- Test devices have validation on (`zest-metal-tests.cpp:17-25`).

**Docs**
- `README.md` says Metal is future, or that macOS runs through MoltenVK: lines 13, 21, 36 and 201.
- `docs/api-reference/index.md:3` and `:217`, and `docs/concepts/device-and-context.md:39`, say the same.
- `docs/examples/index.md:317-320` says Metal has no GPU timestamps. So does the gpu-profiling skill (`.claude/skills/gpu-profiling/SKILL.md:115`).

**Wiring to mirror**

| What | Where |
| --- | --- |
| A profiler backend | `zest__dx_create_gpu_profiler_backend` and its four siblings, `zest_dx12.h:8003-8115` |
| Pass markers | `zest__dx_begin_debug_marker`, `zest_dx12.h:8125-8136` |
| A report written once per device | `zest__dx_report_device_removal`, `zest_dx12.h:1395-1419` |
| A forced-failure hook | `zest__mtl_forced_missing_feature`, `zest_metal.h:423` |
| A shared validation error over a shader's platform | `zest__report_shader_binary_mismatch`, `zest.h:13409` |
| A backend test with a compute and a command graph | "Metal GPU Profiling Off", `zest-metal-tests.cpp:6295` |

## [Probe_] What the M2 does (2026-11-25)

Apple M2, macOS 26.2, Vulkan SDK 1.4.335.1. The sources are in `spikes/metal/probe_m7/` on the Mac (untracked): `probe_counters.mm`, `probe_errors.mm` and `probe_conversion_times.mm`, with `run_probes.sh` and `run_conversion_times.sh`.

**Timestamps** (`probe_counters.mm`)
- `supportsCounterSampling`: at stage boundaries yes. At draw, dispatch, tile dispatch and blit boundaries no. The one counter set is `timestamp`, with `GPUTimestamp`.
- A sample buffer takes 8 to 32768 bytes, shared or private: 4096 samples at most.
- Over 200 ms, `sampleTimestamps:gpuTimestamp:` gave identical CPU and GPU deltas, equal to `mach_absolute_time` in nanoseconds. So one tick is 1 ns.
- **Samples that come back 0, not `MTLCounterErrorValue`:**
  - an empty compute or blit encoder's start and end
  - a render encoder with no draws: its fragment samples are 0, but its vertex samples are written, about 18 us apart
  - any index never attached
- A render encoder with draws writes all four. Vertex start to fragment end covers the encoder.
- Asking for only the fragment samples (the vertex ones `MTLCounterDontSample`) works. So does a start with no end.
- Two encoders with no fence between them overlapped: a render encoder's vertex stage started during the compute encoder before it. Zest's queue fence prevents that.
- Under `MTL_DEBUG_LAYER=1`, sample attachments raise no message.
- 64 MB fill: about 0.9 to 1.1 ms. A long dispatch: about 17 ms.

**Command buffer errors** (`probe_errors.mm`, with `MTLCommandBufferErrorOptionEncoderExecutionStatus`)
- Three ways to make a command buffer fail on purpose all completed with no error, with or without the debug layer and shader validation, and their signals fired. They were:
  - a write to a tracked buffer emptied with `MTLPurgeableStateEmpty`
  - an untracked resident buffer, emptied, reached through an address in a table
  - an address no allocation has

  The `MTLPurgeableStateEmpty` call left the buffer reading as non-volatile.
- This matches the memory note from M2's work: an endless kernel wasn't stopped in 60 s either. So no test can fail a command buffer on this Mac. Task 5 uses a forced hook.

**Shader steps** (`probe_conversion_times.mm`, all 67 GLSL files in the repo, the backend's SPIRV-Cross options, MSL 3.1)

| Step | Total for 67 shaders | Typical shader |
| --- | --- | --- |
| shaderc | 97 to 102 ms, 60 ms of it the first compile in the process | 0.3 to 1.1 ms |
| SPIRV-Cross | 19 to 20 ms | 0.05 to 0.9 ms |
| `newLibraryWithSource`, first time on the machine | 1671 ms | |
| `newLibraryWithSource`, after that (Metal's own cache) | 1.1 to 4.4 ms | 0.01 to 0.05 ms |

- 324 KB of SPIR-V became 200 KB of MSL.
- So a warm start, with Zest's SPIR-V cache on (`zest_DeviceBuilderCacheShaders`), spends about 0.3 ms a shader in SPIRV-Cross and almost nothing in Metal. Without the cache, shaderc dominates.

**What SPIRV-Cross writes for the table** (the `spirv-cross` CLI with the backend's discrete set 0)
- Each slot is `device const void* spvDescriptorSet0Binding<slot> [[buffer(...)]]`, read through `spvDescriptorArray<T>`. That is an array of `spvDescriptor<T>`, a struct holding one `T`: `texture2d_array<float>`, `sampler`, or `device TestBuffer*`.
- The backend puts slot N at buffer index 5 + N (`zest_metal.h:5256-5267`). Push constants are `constant T& pc [[buffer(4)]]`.

## [Decisions_] Decisions for Peter

**Peter took the recommended option on all four (2026-11-25).**

1. **The `msl/` text cache** (roadmap M7: "an `msl/` text cache so a warm start skips shaderc and SPIRV-Cross").
   - **Recommended: drop it.** The GLSL compile is what a warm start can skip, and `zest_DeviceBuilderCacheShaders` already does that on Metal: the SPIR-V lands in `./spv/` with Metal's salt, and "Shader Cache Invalidation" passes on Metal. What's left is SPIRV-Cross, about 0.3 ms a shader, 20 ms for all 67 shaders in the repo. Metal's own cache already makes its compile 0.01 to 0.05 ms after the first run ([Probe_]).
     - The docs say how to get a warm start.
     - The done line becomes "a second launch with shader caching on compiles no GLSL", which holds today.
     - The design's [Scope_] line changes to match.
   - Alternative: build it.
     - Converted MSL, with the vertex input mask and local size, would go in `msl/` under a key of the SPIR-V's hash, the stage, multiview and a converter version.
     - It would be read and written only with caching on.
     - About 100 lines and a backend test, for about 0.3 ms a shader per launch.
     - It would be a task of its own after Task 7.
2. **A profile region inside a render pass.** Metal can't sample inside a render encoder ([Probe_]), and splitting one is ruled out (Settled).
   - **Recommended:** the region takes its render pass's start and end, so it shows the pass's time. The docs say a region inside a render pass measures the whole pass on Metal. A region never reads as free work, and no shared code changes. Every region in the repo is in a compute pass ([State_]), where regions are measured exactly.
   - Alternative: the region reads 0 us at the pass's start. Easy to mistake for free work.
3. **How MSL is marked.**
   - **Recommended: a shader options setter.** `zest_SetShaderOptionsMSL(options, local_size_x, local_size_y, local_size_z)` marks the options' source as MSL. The local size is the threads per threadgroup of a compute shader, which MSL can't declare, and is ignored for the other stages.
     - It works with `zest_CreateShader`, `zest_CreateShaderFromFile`, `zest_CompileShader` and hot reload as they are.
     - Two fields on `zest_shader_options_t` and one function.
   - Alternative: `zest_CreateShaderFromMSL(device, name, source, type, local_size_x, local_size_y, local_size_z)`. One call, but no file or hot reload path unless more functions are added.
4. **How far error reports go.**
   - **Recommended:**
     - The report: the error's name and text, the frame graph's name, and with validation each pass's execution state.
     - A failed batch's events are moved on, so the next wait doesn't run out its timeout. Timeline waits wait in slices of at most 100 ms and reap between slices, so a failure is found within one slice.
     - `MTLCommandBufferErrorDeviceRemoved` flags device loss.
     - A forced hook tests all of it, as no failure can be made on this Mac ([Probe_]).
     - About 150 lines and "Metal Command Buffer Errors".
   - Alternative: the report and the device-loss flag only. A failed frame graph batch then still costs each later wait its whole timeout, and the docs say so. About 60 lines, and the test checks only the report. Task 5 then loses the event move, the sliced wait and their checks.

**Calls made without Peter (check these)**
- **Labels go beyond the roadmap's "markers"** to render and compute pipeline states (the template's or compute's name) and to libraries (the shader's name). It's a creation-time cost only. A compute pipeline state is then built from an `MTLComputePipelineDescriptor`, which has the label.
- **The forced failure hook** is two file-scope values beside `zest__mtl_forced_missing_feature`, read only on the submit and finish paths (Task 5).
- **Encoder execution status is asked for only on frame graph batches.** Immediate command buffers keep `[queue commandBuffer]` and get the same report without pass states. They have one or two encoders and no pass names.
- **"Metal GPU Profiling Off" is deleted**, as it tests M6's stub. "Metal GPU Timestamps" takes its place in the list.
- **`metal_pending_tests[]` and `IsMetalPendingTest` go** rather than stay empty. A C++ array can't be empty, and the D3D12 suite has no pending list either.
- **The slot comment** in `zest.h` also loses "(D3D12, future)", which was stale before M7.
- **The README lines that name the backends** name D3D12 too, as they were stale for both. Peter may want his own wording (Task 8).
- **A capture for Peter:** an untracked patch in the clone writes one frame of `zest-pbr-deferred` to a `.gputrace` with `MTLCaptureManager`, for Peter to open in Xcode on the Mac.

## [Review_] Review Focus

1. **A query lands on the wrong encoder's sample.**
   - The numbers stay plausible but are wrong.
   - Probe: "Metal GPU Timestamps" checks that each region sits inside its pass, that two regions in one compute pass don't overlap, that a region around a dispatch is above 0 and one around nothing is 0, and that a render pass's region equals the pass.
   - Breaks: map ends to start samples, and skip the split.
2. **Samples read before the GPU wrote them, or left over from an earlier frame.**
   - Probe: "GPU Profiler Timestamps" names each region after the frame that recorded it and checks it per frame in flight. Its passes with work must be above 0. Run it 50 times in a loop on the Metal suite.
3. **A sample attachment stays on a shared descriptor.**
   - The context's render pass descriptor, and the new compute and blit pass descriptors, are reused by every pass, clear and blit.
   - An attachment left on one makes the next encoder write samples it doesn't own, or into a sample buffer that has been freed.
   - Probe: "Metal GPU Timestamps" checks that every descriptor's sample buffer is nil after a profiled graph, and again after profiling is turned off. The zombie run catches a freed sample buffer.
4. **Unbalanced debug groups or a leaked pass label.**
   - Probes:
     - "Metal Pass Labels" checks the group depth is 1 inside a pass, and 0 with the label nil after the batch.
     - The zombie and `MallocScribble` run catches an over-released label.
     - Every backend test runs with the debug layer on.
5. **The MSL option changes Vulkan or D3D12 shader creation.**
   - `zest.h`'s shader path runs on every backend, and the new cache key must leave existing keys alone.
   - Probes: "Shader Cache Invalidation" and "Shader MSL Source Platform" on all three suites, and Peter's Windows run.
6. **Moving a failed batch's events on lets work run before its inputs are ready.**
   - Probe: after the forced failure, "Metal Command Buffer Errors" runs another graph that writes and reads back a buffer, and expects the right values.

## [Scope_] Out of scope

- The `msl/` cache, if Peter takes Decision 1.
- Timestamps between draws or dispatches, which the M2 can't sample. A GPU that can would start at `zest__mtl_write_timestamp`, with `sampleCountersInBuffer:`.
- `MTLBinaryArchive` pipeline caching (design [Scope_]).
- MSL in a multiview pipeline. The backend refuses it with a message. Supporting it would mean documenting the view index and layer conventions the backend's conversion uses (design [Mapping_], "Multiview").
- `zest_ValidateShader` on MSL. It takes no options, so it stays GLSL and HLSL. MSL errors come from pipeline or compute creation.
- Debug groups for profile regions, as on D3D12 (its M7 [Excluded]).
- A test that fails a real command buffer. The M2 won't ([Probe_]).
- What a failed batch's drawable does. A batch that failed after `presentDrawable` may not present, and the layer's next `nextDrawable` then waits its own timeout.
- The pool growth race and the other shared items from M5 and M6, which are Peter's.
- Reporting SPIRV-Cross's naming and uniform alias bugs upstream.
- Building without the Vulkan SDK. Metal still needs the SDK's shaderc and SPIRV-Cross, and CMake still needs Vulkan for every build. It gets its own plan after M7 (Peter, 2026-11-25).

## [Phase_1] GPU timestamps

### Task 1: Baseline

**Steps:**
- [x] Sync the clone to the commit that holds M6. Expected: clean apart from `spikes/`.
- [x] Wake the display and run, under `caffeinate -d`, `--platform all` and `--platform metal --shader-validation` from `../Zest-Build-Metal`, then `zest-tests` from `../Zest-Build`. Expected: the baseline in [Constraints_]. Save the logs in the scratchpad.
- [x] Rebuild the draw count patch if it no longer applies, and check it on `zest-minimal-template --metal --validate --frames 240`. Expected: exit 0, `validation errors: 0`, `metal draws: 240`.

### Task 2: Pass timestamps

**Files:**
- `zest_metal.h`:
  - the structs (`:154-287`) and a `#define` beside `ZEST__MTL_MAX_BLIT_PIPELINES`
  - the GPU profiling prototypes (`:580-585`)
  - the five profiler functions, out of `Unavailable_stubs` (`:5524-5533`) into the empty `GPU_Profiling_backend` section (`:5429-5430`)
  - the encoder sites: `begin_render_pass` (`:4610`), `zest__mtl_pass_blit_encoder` (`:4493`), `zest__mtl_pass_compute_encoder` (`:4518`), `zest__mtl_image_clear` (`:5098`), `zest__mtl_blit_image_mip` (`:5085`), and the three encoder ends (`:4475`, `:4499`, `:4526`)
  - `begin_command_buffer` (`:4449`) and `end_command_buffer` (`:4535`)
  - context backend creation and cleanup (`:2144`, `:2312`)
- `zest-tests.cpp:233-257`: the pending list and its check go.
- `zest-metal-tests.cpp`: "Metal GPU Profiling Off" and its helpers go (`:6280-6352`, `:6409`).

**Interfaces:**
```c
#define ZEST__MTL_PROFILE_SAMPLE_COUNT 4096			//Samples in each frame in flight's sample buffer, the most one buffer takes (32 KB)

//Where an encoder's samples sit in its frame in flight's sample buffer: two for a compute or blit encoder, four for a render encoder
typedef struct zest_mtl_profile_encoder_t {
	zest_uint first_sample;
	zest_uint sample_count;
} zest_mtl_profile_encoder_t;

//Metal samples timestamps at encoder boundaries only, so each query is mapped to an encoder's start or end and resolved at readback
typedef struct zest_mtl_gpu_profiler_backend_t {
	id<MTLCounterSampleBuffer> sample_buffers[ZEST_MAX_FIF];
	zest_mtl_profile_encoder_t *encoders[ZEST_MAX_FIF];		//ZEST__MTL_PROFILE_SAMPLE_COUNT / 2 each, in the order they opened
	zest_uint *query_encoders[ZEST_MAX_FIF];				//max_queries each: the encoder's index times 2, plus 1 for its end, or ZEST_INVALID
	zest_uint encoder_count[ZEST_MAX_FIF];
	zest_uint sample_count[ZEST_MAX_FIF];					//Samples handed out this frame
	zest_bool reported_full;
} zest_mtl_gpu_profiler_backend_t;
```
- `zest_mtl_command_list_backend_t` gains:
  ```c
	zest_uint profile_pending[ZEST_MAX_GPU_PROFILE_STACK_DEPTH + 1];	//Queries waiting for the next encoder's start
	zest_uint profile_pending_count;
	zest_uint profile_open_encoder;					//Index of the open encoder in the frame in flight's list, ZEST_INVALID for none
	zest_uint profile_last_encoder;					//The encoder this batch ended last, ZEST_INVALID for none
  ```
- `zest_mtl_context_backend_t` gains `MTLComputePassDescriptor *compute_pass_descriptor;` and `MTLBlitPassDescriptor *blit_pass_descriptor;`, made and released beside `render_pass_descriptor`, with its member comment.
- **Encoder helpers:**
  - `ZEST_PRIVATE zest_uint zest__mtl_profile_encoder_samples(const zest_command_list command_list, zest_uint sample_count);`, commented `//The first of the samples a pass encoder about to open takes, or ZEST_INVALID when the command list isn't profiling or the frame's samples are used up`.
    - Profiling means `command_list->gpu_profiler` is set and enabled, with a backend.
    - It records the encoder, maps every pending query to its start, empties the pending list and makes it the open encoder.
    - With the samples used up, it logs `GPU timestamps ran out of samples this frame, so some passes read 0.` once per profiler, and leaves pending queries pending.
  - `ZEST_PRIVATE void zest__mtl_profile_encoder_ended(const zest_command_list command_list);`: the open encoder becomes the last one.
- **Each encoder site** asks for samples (4 for a render encoder, 2 for the others).
  - When it gets them, it sets them on its descriptor's `sampleBufferAttachments[0]`:
    - render: vertex start and end, then fragment start and end
    - compute and blit: start and end of the encoder
  - It opens through the descriptor: `renderCommandEncoderWithDescriptor:`, `computeCommandEncoderWithDescriptor:` or `blitCommandEncoderWithDescriptor:`, then clears the attachment's sample buffer.
  - Without samples, it opens as it does today. That's one branch with profiling off.
  - `zest__mtl_image_clear` and `zest__mtl_blit_image_mip` attach to the context's render pass descriptor around their one encoder and call the ended helper after it.
- **`zest__mtl_write_timestamp`.** It first sets the query's entry to `ZEST_INVALID`.
  - A begin:
    - With a compute or blit encoder open, it ends that encoder and pushes the query to the pending list (a region in a compute or transfer pass).
    - With a render encoder open, the query takes the encoder's start (Decision 2).
    - Otherwise it pushes the query to the pending list.
  - An end:
    - If its begin (`query_index - 1`) is still the top of the pending list, it pops it and both stay `ZEST_INVALID`: a pair with no encoder.
    - With a render encoder open, the query takes that encoder's end.
    - With a compute or blit encoder open, the query takes its end and the encoder is ended.
    - Otherwise it takes the last ended encoder's end.
  - `is_end` is what tells a begin from an end.
- `begin_command_buffer` empties the pending list and sets both encoder indexes to `ZEST_INVALID`. `end_command_buffer` calls the ended helper when an encoder was open.
- **Creation:**
  - Without `support.counter_sampling_at_stage`, or without the `MTLCommonCounterSetTimestamp` set, it logs `This GPU can't sample timestamps at pass boundaries, so this context runs without GPU profiling.` and returns false.
  - Otherwise it makes the sample buffers (shared storage, `ZEST__MTL_PROFILE_SAMPLE_COUNT`, label `Zest GPU Timestamps`) and the encoder and query lists, on `context->allocator`.
  - It allocates `raw_timestamps[fif]` as D3D12 does, and sets `timestamp_period_ns = 1.0f`.
  - It logs `Metal GPU timestamps: sampled at encoder boundaries, 4096 samples per frame in flight.`
  - On any failure it frees what it made and returns false.
- `zest__mtl_reset_gpu_query_pool` zeroes the frame in flight's encoder and sample counts. `zest__mtl_cleanup_gpu_profiler_backend` releases the sample buffers and frees the lists. Shared cleanup frees the struct and `raw_timestamps`.
- **`zest__mtl_readback_gpu_timestamps`** resolves `[0, sample_count[fif])` with `resolveCounterRange:` inside `@autoreleasepool`. Then:
  1. Each query takes its encoder's tick, and 0 when it has none.
     - A render encoder starts at its vertex start, or its fragment start when that is 0, and ends at its fragment end, or its vertex end when that is 0.
     - A compute or blit encoder uses its two samples.
  2. In each pair, a 0 side takes the other side.
  3. A pair still 0 on both sides takes the previous pair's end. Leading pairs that are still 0 take the first non-zero begin after them.
  4. An end before its begin takes the begin.

  It returns true. A nil resolve logs once and leaves zeros.
- Comments: on the struct as above, and on `write_timestamp`: `//Metal samples only where encoders start and end, so a query is mapped to the encoder that starts after it or the one that ended before it`.

**Steps:**
- [x] Write the structs and helpers. Move the five functions, with their `//GPU Profiling` line (`zest_metal.h:5524`), out of `Unavailable_stubs` into `GPU_Profiling_backend`, and write them. Build. Expected: no warning.
- [x] Take "GPU Profiler Timestamps" off the pending list by deleting `metal_pending_tests[]`, `IsMetalPendingTest` and the `#ifdef ZEST_ENABLE_METAL` check in `RegisterTest`. Delete "Metal GPU Profiling Off". Build and run `--platform all` and `--platform metal --shader-validation`. Expected: `139 / 139` twice, `Metal suite: 135 / 135`, `Metal backend: 54 / 54`, no VUID line.
- [x] Run "GPU Profiler Timestamps" 50 times on Metal (`--platform metal --test "GPU Profiler"`). Expected: 50 passes. Record its three pass timings from one run in As built.
- [x] Break: no encoder takes samples. Expected: "GPU Profiler Timestamps" fails on a total of 0. The shared test accepts 0 for any one pass, so the breaks that need a pass to be above 0 are Task 3's.

### Task 3: Profile regions and the backend test

**Files:** `zest-metal-tests.cpp`: "Metal GPU Timestamps", registered where "Metal GPU Profiling Off" was.

**Interfaces:**
- The test, on a headless context created with `zest_context_init_flag_gpu_profiling`:
  - The flag is set, `gpu_profiler.backend` isn't NULL, and `timestamp_period_ns` is 1.
  - **One command graph, flushed with `zest_FlushFrameGraphAndWait`.** Each pass depends on the one before:
    - "Timed Copy": a transfer pass copying 16 MB between two buffers.
    - "Timed Compute": a compute pass with three regions: "Region A" around one dispatch of `buffer_write.comp`, "Region Empty" around nothing, and "Region B" around a second dispatch that writes another range.
    - "Timed Draw": a render pass to a 256 x 256 transient colour image, with "Region Draw" around a clear-only task.
  - **Checks on `zest_GetGPUProfileResults` and the raw ticks** (`context->gpu_profiler.raw_timestamps[context->current_fif]`, pair `i` at `2i` and `2i + 1`):
    - every pass and region is there at its depth
    - the three passes are above 0 us
    - Region A and Region B are above 0 and inside the compute pass's ticks, and Region A ends no later than Region B begins
    - Region Empty is 0
    - Region Draw's ticks equal Timed Draw's (Decision 2)
    - `zest_GetGPUProfileTotalTime` equals Timed Copy's begin to Timed Draw's end, which the dependent passes keep in order on one queue
    - both dispatches' values read back right, so the split encoders re-bound their pipeline and push constants
    - the validation count is 0
  - **A second graph** with profiling turned off by `zest_EnableGPUProfiling(context, ZEST_FALSE)`: a transfer pass that calls `zest_cmd_ImageClear` on the transient image. After it, the sample buffer of each of the context's three pass descriptors is nil.
- The test's comment: `//Passes and the regions inside them get their own GPU timings, which Metal can only sample where encoders start and end`.

**Steps:**
- [x] Write the test and register it. Build and run `--platform metal`. Expected: `Metal backend: 55 / 55`.
- [x] Breaks, each on the clone and put back:
  - every end mapped to its encoder's start: the passes read 0
  - readback skipping step 3: Region Empty's 0 ticks make the total wrong
  - no encoder split at a region's begin: Region B reads 0
  - Decision 2's mapping removed, so a begin with a render encoder open goes pending: Region Draw reads 0
  - the sample buffer not cleared off the compute pass descriptor: the descriptor check fails
- [x] **The phase's end:**
  - `--platform all`, `--platform metal --shader-validation`, one `NSZombieEnabled=YES MallocScribble=1` run of `--platform metal`, the Metal-off suites, and every target with Metal on and off. Expected: `139 / 139` twice, `Metal suite: 135 / 135`, `Metal backend: 55 / 55`, no zombie report, Metal off `139 / 139` twice, no warning.
  - With the draw count patch, `zest-compute-example --metal --validate --frames 600` and `zest-render-targets --metal --validate --frames 600`. Expected: exit 0, `validation errors: 0`, draws, and no "runs without GPU profiling" line in `zest_log.txt`.
  - On Windows, `zest-tests` and `zest-c-compile-check` in Debug. Expected: no warning.
- [x] Phase report, with the shared edit for Peter's Windows run: `zest-tests.cpp`. Restore the clone.

## [Phase_2] Names, markers and error reports

### Task 4: Markers and labels

**Files:**
- `zest_metal.h`:
  - two new functions in `Debug_naming` (`:5432`), and their table entries, removing "and the debug markers" from the comment at `:1404`
  - `set_next_command_buffer` (`:4423`), the encoder sites of Task 2, `end_command_buffer`
  - `zest__mtl_render_pipeline_state`'s caller (`:3479`, which has the template)
  - `zest__mtl_finish_compute` (`:3538-3560`) and `zest__mtl_compile_msl` (`:5331`)
- `zest-metal-tests.cpp`: "Metal Pass Labels".

**Interfaces:**
- `zest_mtl_command_list_backend_t` gains `NSString *pass_label; //The pass being recorded, from its debug marker to the end of it` and `zest_uint debug_group_depth;`.
- **`zest__mtl_begin_debug_marker(const zest_command_list command_list, const char *name)`:**
  - `pass_label` becomes `[[NSString alloc] initWithUTF8String:name]`, a fallback of `@"Unnamed Pass"` for a NULL result.
  - It calls `pushDebugGroup:` on the batch's command buffer, inside `@autoreleasepool`, and raises the depth.
- **`zest__mtl_end_debug_marker`** pops the group, lowers the depth and releases the label. `end_command_buffer` pops whatever is still open and releases the label.
- Every encoder a pass opens (Task 2's sites) gets `setLabel:pass_label` when the label isn't nil.
- **Other labels:**
  - Each batch's command buffer takes the frame graph's name, from an NSString made and released in `set_next_command_buffer`.
  - A render pipeline descriptor takes its template's name before its state is built.
  - A compute's pipeline state is built from an `MTLComputePipelineDescriptor` labelled with the compute's name, through `newComputePipelineStateWithDescriptor:options:reflection:error:`.
  - A converted shader's library takes the shader's name.
- **The test**, on a headless context:
  - **A command graph "Label Graph"** has a compute pass "Label Compute" whose task dispatches `buffer_write.comp` and then reads, from the command list's backend:
    - `pass_label` equal to the pass's name
    - `[command_buffer label]` equal to the graph's name
    - the open compute encoder's `label` equal to the pass's name
    - `debug_group_depth` 1

    It records each into the task data.
  - After `zest_FlushFrameGraphAndWait`, the depth is 0, `pass_label` is nil, and the dispatch's values are right.
  - A pipeline built from a template named "Labelled Pipeline", on the test's own device, has a state with that label. A state is shared by every template with the same contents, so it keeps the name of the first one that built it. The compute's state is labelled "Label Compute Pipeline". Its library is labelled with the shader's name.
  - The validation count is 0.
- Shared code ends every pass's marker, even when `begin_render_pass` fails (`zest.h:17767-17779`). So the pop in `end_command_buffer` is only a guard, for a failure no test can force.

**Steps:**
- [x] Write the markers, the labels and the test. Build and run `--platform metal` and `--platform metal --shader-validation`. Expected: `Metal backend: 56 / 56` both times.
- [x] Breaks: no `popDebugGroup` in `end_debug_marker` fails the depth check. Record whether the debug layer also reports the unbalanced group at commit. A label released in `end_debug_marker` but not set to nil fails under `NSZombieEnabled=YES`.
- [x] **The capture for Peter**, with an untracked patch on the clone:
  - `zest-pbr-deferred` calls `MTLCaptureManager` `startCaptureWithDescriptor:` around frame 30, with `MTLCaptureDestinationGPUTraceDocument` and the output `spikes/metal/m7_pbr_deferred.gputrace`.
  - Run it with `MTL_CAPTURE_ENABLED=1 --metal --frames 60`.
  - Expected: the trace exists. Take the patch out, and name the path for Peter.

### Task 5: Command buffer error reports (Decision 4)

**Files:**
- `zest_metal.h`:
  - the device backend (`:185-210`) and `zest_mtl_committed_batch_t` (`:253-258`)
  - `zest__mtl_initialise_device` (`:1820`) and the device cleanup
  - `set_next_command_buffer`, `submit_frame_graph_batch` (`:4635`), `zest__mtl_push_committed_batch` (`:2751`), the reaper (`:2760`), `zest__mtl_finish_command_buffer` (`:2462`)
  - `zest__mtl_wait_for_timeline_value` (`:2414`), `zest__mtl_reinit_logical_device`
  - the forced hooks (`:423-424`)
- `zest-metal-tests.cpp`: "Metal Command Buffer Errors".

**Interfaces:**
- The device backend gains `MTLCommandBufferDescriptor *batch_descriptor;`, commented `//Unretained references, and encoder execution status when validation is on`, and `zest_bool failure_alerted;`.
- `set_next_command_buffer` takes its buffer with `commandBufferWithDescriptor:batch_descriptor`.
- `zest_mtl_committed_batch_t` gains `zest_context context;`, `id<MTLSharedEvent> signal_event;` (retained) and `zest_u64 signal_value;`, the frame or wave event the batch signals before its ordering event. `zest__mtl_push_committed_batch` takes them.
- **`ZEST_PRIVATE void zest__mtl_report_command_buffer_error(zest_device device, id<MTLCommandBuffer> command_buffer, NSError *error);`** writes to the device's log:
  - `A Metal command buffer failed: <name> (<code>), <description>. Command buffer: <label>.` The name comes from a switch over `MTLCommandBufferError`.
  - With encoder infos in the error's `userInfo` (`MTLCommandBufferEncoderInfoErrorKey`), one line per encoder: `  <label>: <state>`, the state Completed, Affected, Faulted, Pending or Unknown, then its signposts.
  - Without them, on a device without validation: `Turn on validation to have Metal report each pass's state.` With validation on: `Metal gave no pass states for this failure.`
  - The first report on a device also raises `ZEST_ALERT("A Metal command buffer failed (<name>). The log has the report.")`. `failure_alerted` keeps it to one, and `reinit_logical_device` clears it.
- `zest__mtl_finish_command_buffer` calls it for a failed buffer, in place of its line today.
- **A failed batch in the reaper:**
  - It moves each of the batch's two events to its value with `setSignaledValue:` when it's behind, as the immediate path does.
  - It flags `zest_context_flag_device_lost` on the batch's context for `MTLCommandBufferErrorDeviceRemoved`.
- **`zest__mtl_wait_for_timeline_value`** waits in slices of at most 100 ms, and after a slice that times out reaps the device's batches before checking again. The comment says why: a failed batch's signals may never fire.
- **The forced hooks:**
  - `static NSInteger zest__mtl_forced_command_buffer_error = 0;` One-shot: the next frame graph batch encodes no signals, its buffer is kept in `static id<MTLCommandBuffer> zest__mtl_forced_failed_command_buffer`, and the hook resets.
  - `zest__mtl_finish_command_buffer` treats that buffer as failed with `[NSError errorWithDomain:MTLCommandBufferErrorDomain code:... userInfo:nil]`, then clears it.
  - Comment: `//Test hooks: the M2 can't make a command buffer fail, so the next batch skips its signals and reads as failed with this code`.
- **The test:**
  - A headless context.
  - A command graph "Failing Graph" with a compute pass "Failing Pass" that dispatches, flushed with `zest_FlushFrameGraphAndWait` under `MTLCommandBufferErrorTimeout`. Expected:
    - success within 1 s
    - the log grew by one report with "Timeout", "Failing Graph" and "Metal gave no pass states", as the forced error carries none and the test device has validation on
    - `zest_ContextDeviceWasLost` false
  - A second graph that writes and reads back a buffer with no hook. Expected: success and the right values.
  - A third graph under `MTLCommandBufferErrorDeviceRemoved`. Expected: `zest_ContextDeviceWasLost` true.
  - Validation messages and reports from the failures are expected and reset. The context is destroyed and the device destroys clean.

**Steps:**
- [x] Write the descriptor, the report, the reaper's handling, the sliced wait, the hooks and the test. Build and run `--platform metal` and `--platform metal --shader-validation`. Expected: `Metal backend: 57 / 57`. The test takes under 3 s.
- [x] Breaks, each on the clone and put back:
  - no event move: the flush waits out the 10 s debug timeout and the test fails
  - no device-lost flag: the third check fails
  - the report skipped: the log check fails
- [x] **The phase's end:**
  - `--platform all`, `--platform metal --shader-validation`, the zombie and `MallocScribble` run, the Metal-off suites, and every target with Metal on and off. Expected: `139 / 139` twice, `Metal suite: 135 / 135`, `Metal backend: 57 / 57`, no zombie report, no warning.
  - With the patch, `zest-minimal-template` and `zest-timelinefx` with no backend argument and `--validate --frames 600`. Expected: exit 0, `validation errors: 0`, draws.
  - On Windows, `zest-tests` and `zest-c-compile-check` in Debug. Expected: no warning.
- [x] Phase report: no shared edits in this phase. Name the `.gputrace` path. Restore the clone.

## [Phase_3] MSL as a shader input

### Task 6: The MSL option in shared code (Decision 3)

**Files:**
- `zest.h`:
  - `zest_shader_options_t` (`:7766-7770`) and the declarations beside `zest_AddMacroDefinition` (`:5335-5339`)
  - `zest_CreateShaderOptions` (`:13264`), `zest__copy_shader_options` (`:13281`), `zest_CompileShader` (`:13324`), `zest_CreateShaderFromFile` (`:13339`), `zest__shader_cache_key` (`:13362`), `zest_CreateShader` (`:13425`)
  - the slot comment (`:2698-2699`)
- `zest_metal.h`: `zest__mtl_compile_shader` (`:5152`), so the shared test passes on Metal.
- `zest-pipeline-tests.cpp`, `zest-tests.h` and `zest-tests.cpp`: "Shader MSL Source Platform", registered after "Shader Binary Platform Mismatch" (`zest-tests.cpp:78`).

**Interfaces:**
- `zest_shader_options_t` gains:
  ```c
	zest_bool msl;					//The source is MSL, which only the Metal backend takes
	zest_uint local_size[3];		//Threads per threadgroup of an MSL compute shader, which MSL can't declare
  ```
- **The setter:**
  ```c
  //Mark the source compiled with these options as MSL, which only the Metal backend takes. A compute shader's threads per
  //threadgroup are local_size, as MSL can't declare them; vertex and fragment shaders ignore it. See docs/concepts/bindless.md
  //for the buffer indexes the backend binds.
  ZEST_API void zest_SetShaderOptionsMSL(zest_shader_options options, zest_uint local_size_x, zest_uint local_size_y, zest_uint local_size_z);
  ```
- **The rest of the shared path:**
  - `zest__copy_shader_options` copies both fields.
  - `zest__shader_cache_key` hashes `msl` and the local size only when `msl` is set, so every other shader keeps its key.
  - `zest_CreateShader`, with `msl` set:
    - On a device whose platform isn't Metal, it logs a validation error and returns a zero handle before making a shader: `Shader <name> is MSL, which only the Metal backend takes. Give this device GLSL, HLSL or Slang.`
    - On Metal it neither reads nor writes the cache, as compiling MSL is a copy.
  - `zest_CompileShader` refuses the same way and returns false.
  - `zest_CreateShaderFromFile` frees the source and returns the zero handle when `zest_CreateShader` gives one.
  - `zest_ValidateShader`'s comment says it checks GLSL or HLSL only.
- `zest__mtl_compile_shader`, with `options->msl` set, copies the source and its terminating zero into `binary`, clears `last_error` and returns true. It runs no shaderc.
- **The slot comment:** `//  - HLSL (D3D12): one register space per slot` and `//  - MSL (Metal): slot N is the buffer argument at index 5 + N, an array of 8-byte entries`. Only those two lines change.
- **The test:**
  - On Vulkan and D3D12, `zest_CreateShader` with MSL options gives a zero handle and a validation count of 1, which the test resets.
  - On Metal it gives a shader whose `zest_GetCompiledShader` holds the MSL text, its size the text's length plus 1.
  - The source is a fragment function returning red.

**Steps:**
- [x] Make the shared edits, the `compile_shader` branch and the test. Build on the Mac with Metal on and off, and run `--platform all` and the Metal-off suites. Expected: `140 / 140` twice, `Metal suite: 136 / 136`, `Metal backend: 57 / 57`, and the Metal-off suites at `140 / 140` twice.
- [x] Break: drop the platform refusal. Expected: on Vulkan, shaderc's compile error asserts in `zest_CreateShader`. Put it back.
- [x] On Windows, build every project in Debug and `zest-c-compile-check`. Expected: no warning.

### Task 7: MSL on the Metal backend

**Files:**
- `zest_metal.h`: `zest__mtl_shader_function` (`:5360`), `zest__mtl_compile_msl` (`:5331`), and a new function beside them.
- `zest-metal-tests.cpp`: "Metal MSL Shaders".

**Interfaces:**
- **`ZEST_PRIVATE zest_bool zest__mtl_msl_function(zest_device device, zest_shader shader, zest_shader_type expected_type, zest_bool multiview, zest_mtl_shader_function_t *out_function, char *out_error, zest_size error_size);`** is commented `//A shader written in MSL: the library's only function of the stage, with its vertex inputs from Metal's reflection`. In order:
  - A multiview pipeline is refused: `Shader <name> is MSL, which a multiview pipeline on Metal can't take. Use GLSL, Slang or SPIR-V for a multiview pipeline.`
  - It compiles the library, without the uniform alias replacement.
  - It takes the one function whose `functionType` matches the stage. None, or more than one, is an error naming the functions found.
  - A vertex function's `vertexAttributes` that are active give the input mask, and an attribute index above 30 is an error.
  - A compute shader takes `shader->options.local_size`, and a 0 in it is an error naming `zest_SetShaderOptionsMSL`.
- `zest__mtl_shader_function` calls it for an MSL shader in place of the conversion, under the same cache and lock.
- `zest__mtl_compile_msl` is split so that both paths share the library compile: the uniform alias replacement stays on the SPIR-V path.
- **The test:**
  - **The declarations first.** In a scratch `.mm` on the Mac, compile slot declarations with `newLibraryWithSource`:
    - `const device texture2d<float> *textures [[buffer(6)]]`
    - a struct holding one `texture2d<float>`, read through a pointer to it
    - the same two forms for `sampler` at 5 and `device T *` at 10

    The test and the docs use the forms that compile and read correctly. Record which in As built.
  - **A draw.** A combined MSL source has a vertex function and a fragment function:
    - The vertex function reads `[[attribute(0)]]` float2 position and `[[attribute(1)]]` float2 uv from vertex binding 0, and negates y.
    - The fragment function samples a bindless 2D texture (slot 1, buffer 6) with a bindless sampler (slot 0, buffer 5), both indexed by push constants at buffer 4.
    - It draws a triangle covering the top half of a 16 x 16 target in Zest's clip space, with a texture of one colour, in a command graph.
    - Readback: row 0 holds the texture's colour and row 15 the clear colour, so the Y convention is the one the docs give.
  - **A compute.** An MSL kernel with local size 8 x 1 x 1 writes `index + 1` through a bindless storage buffer (slot 5, buffer 10). Four groups are dispatched, and the 32 values read back right.
  - **Refusals, each one validation error, which the test resets:**
    - an MSL compute with a local size of 0
    - a source with two vertex functions
    - an MSL vertex shader in a pipeline with a view mask, in a multiview pass built as "Metal Multiview Rules" builds one
  - **Hot reload**, as the hot reload phase of "Metal Immediate Compute" does it (`zest-metal-tests.cpp:4445-4456`): the kernel's file in the working folder is rewritten to write `index + 2`. After `zest_CheckShaderHotReload`, the dispatch writes the new values.

**Steps:**
- [x] Run the declaration probe and record the forms.
- [x] Write the backend path and the test. Build and run `--platform all` and `--platform metal --shader-validation`. Expected: `140 / 140` twice, `Metal suite: 136 / 136`, `Metal backend: 58 / 58`.
- [x] Breaks, each on the clone and put back:
  - the input mask from the first attribute only: the draw reads the clear colour
  - the local size ignored: the compute fails to dispatch or writes wrong values
  - no multiview refusal: the refusal check fails
- [x] **The phase's end:**
  - `--platform all`, `--platform metal --shader-validation`, the zombie and `MallocScribble` run, the Metal-off suites, and every target with Metal on and off. Expected: `140 / 140` twice, `Metal suite: 136 / 136`, `Metal backend: 58 / 58`, Metal off `140 / 140` twice, no warning.
  - On Windows, every project in Debug and `zest-c-compile-check`. Expected: no warning.
- [x] Phase report, with the shared edits for Peter's Windows run: `zest.h`, `zest-pipeline-tests.cpp`, `zest-tests.h`, `zest-tests.cpp`. Restore the clone.

## [Phase_4] Docs and the final check

### Task 8: Docs, the skill and the README

**Files:**
- The docs under Tech stack.
- `.claude/skills/metal-backend/SKILL.md` (new), `.claude/skills/gpu-profiling/SKILL.md:115`, `.claude/skills/pipelines-and-shaders/SKILL.md`, and `CLAUDE.md`'s skill list.
- `metal_platform_layer_implementation.md`: [Roadmap_], [Mapping_], [Scope_], [Built_].

**Interfaces:**
- **`docs/concepts/bindless.md`:**
  - The contract list (`:70-74`): "GLSL/SPIR-V (Vulkan and Metal)" and "MSL (Metal): slot N at buffer index 5 + N", linking a new "MSL Descriptor Layout" section after HLSL's (`:174`).
  - That section has:
    - the slot table with buffer indexes 5 to 13
    - the entry forms Task 7 recorded
    - push constants at buffer 4, and vertex binding N at buffer N (0 to 3) with `[[attribute(location)]]`
    - that the vertex function negates y
    - that the backend takes the library's only function of each stage, and that a compute shader's threads per threadgroup come from `zest_SetShaderOptionsMSL`
    - what's refused (multiview, a 0 local size)
    - that `ZEST_METAL` isn't defined for MSL, as there's no preprocessor pass
    - Task 7's draw as the example
- `docs/concepts/rendering-conventions.md`, "Clip Space / NDC": one line, that a hand-written MSL vertex shader negates y itself, as the backend does for GLSL, Slang and SPIR-V.
- `docs/api-reference/pipeline.md`: `zest_SetShaderOptionsMSL` beside the shader options.
- **`docs/getting-started/installation.md`, "The Metal Backend":**
  - A paragraph mirroring D3D12's (`:108-110`): Xcode's frame capture and Instruments show each command buffer, pass and pipeline by name, and with validation on a failed command buffer's report gives each pass's state.
  - A line on warm starts: `zest_DeviceBuilderCacheShaders` keeps the GLSL compile off a second launch, with the probe's numbers.
- **`docs/examples/index.md`, "Limits on Metal":** GPU timings are there now. A profile region inside a render pass shows the whole pass, and a region inside a compute or transfer pass runs as an encoder of its own.
- `docs/api-reference/index.md:3` and `:217`, and `docs/concepts/device-and-context.md:39`, name the three backends.
- **`README.md`.** Peter may reword any of these:
  - line 13: "drop `zest.h` and a platform layer (`zest_vulkan.h`, `zest_dx12.h` or `zest_metal.h`) into your project and go"
  - line 21: "Windows (Vulkan or D3D12), Linux (Vulkan) and macOS (Metal, or Vulkan through MoltenVK)"
  - line 36: "These are the Vulkan backend's. The D3D12 backend needs Enhanced Barriers, and the Metal backend macOS 15 and a Metal 3 GPU, tested on Apple silicon."
  - line 201: the D3D12 and Metal to-do goes, leaving WebGPU and OpenGL
- **The `metal-backend` skill** mirrors `d3d12-backend`'s sections and is written from the code, the design document and the M1 to M7 plans:
  - its description, for triggering
  - Orientation: how it plugs in, finding code by the contents keywords, names, the Objective-C rules (no ARC, which hooks take an autorelease pool), the build
  - Device requirements and the forced hooks
  - How Zest maps onto Metal: queues and shared events, command buffers, encoders and the fence, residency, the bindless table, push constants, pipelines and their caches, multiview
  - Shaders: SPIR-V to MSL, the renames, the alias fix, MSL input, the limits
  - Debugging:
    - validation and the standard error reader
    - shader validation logs
    - labels, groups and captures
    - error reports and the forced hook
    - timestamps and their limits
  - Testing: the Mac workflow, the suites and switches, the draw count patch, gates, the lock screen
  - Gotchas, from the memory notes:
    - the lock screen
    - completed status before handlers
    - GPU errors that can't be provoked
    - windowed frame rates
    - descriptors holding textures
    - views under shader validation
    - validation fixed at the first device
  - Known limits, and the design record
- `CLAUDE.md`'s skill list gains `metal-backend/`, and the gpu-profiling and pipelines skills lose their "until M7" lines.
- **The design document:**
  - The roadmap names this file.
  - The three [Mapping_] rows give what was built.
  - "Shader creation" gains MSL input.
  - [Scope_]'s cache line follows Decision 1.
  - M7 goes in [Built_].

**Steps:**
- [x] Write them. Run `qgrep update zest`.
- [x] Read each changed doc against the code it describes: every function named exists, and every buffer index matches `zest_metal.h`.

### Task 9: The final check

**Steps:**
- [x] **On the Mac,** with the screen unlocked:
  - every target with Metal on (26) and off (25)
  - `--platform all`, `--platform metal --shader-validation`, the zombie and `MallocScribble` run, and the Metal-off suites
  - with the draw count patch, `zest-minimal-template`, `zest-compute-example`, `zest-render-targets` and `zest-timelinefx-particle-techniques --effect-mode gpu`, each with no backend argument and `--validate --frames 600`

  Expected: no warning, `140 / 140` twice, `Metal suite: 136 / 136`, `Metal backend: 58 / 58`, the same Metal lines under shader validation, no VUID line, no zombie report, Metal off `140 / 140` twice, and every example at exit 0 with `validation errors: 0` and draws.
- [x] On Windows, every project in Debug, `zest-c-compile-check` included. Expected: no warning.
- [x] Add As built to this file: the counts, the timings from Task 2, the declaration forms from Task 7, every difference from the plan, and the shared edits.
- [x] Hand Peter the Windows checks, the visual checks and the `.gputrace` path. Restore the clone.
- [x] **Remind Peter** that the push trigger in `.github/workflows/ci.yml` has been commented out since 2026-11-23, for the Metal work, and that this is the last Metal milestone.

## [Cost_] Per-frame cost

| Path | Vulkan and D3D12 | Metal, profiling off | Metal, profiling on |
| --- | --- | --- | --- |
| Per draw or dispatch | Unchanged | Unchanged | Unchanged |
| Per encoder opened | Unchanged | One branch, and a label (a retain) | Plus a sample record and two or four sample indexes on a reused descriptor |
| Per pass | Unchanged | An NSString made and released, a debug group pushed and popped, in an autorelease pool | Plus two `write_timestamp` calls. In a compute or transfer pass, each profile region ends one encoder and opens one |
| Per batch | Unchanged | An NSString for the command buffer's label | The same |
| Per frame | Unchanged | Unchanged | One CPU resolve of the samples used, 8 bytes each, in an autorelease pool, and a walk over the encoders and queries |
| A timeline wait | Unchanged | Slices of at most 100 ms with a reap between them, which only a wait longer than 100 ms sees (Decision 4) | The same |
| Shader creation | One branch on the options' MSL flag | GLSL unchanged. MSL: a copy, then Metal's compile, with no shaderc or SPIRV-Cross | |
| Pipeline and library creation | Unchanged | One label each | |

**Memory:**
- A context with GPU profiling: two 32 KB shared sample buffers, which are Metal objects outside Zest's pools. On the context allocator, two encoder lists of 16 KB and two query maps of 2 KB, at `ZEST_MAX_FIF` 2. No new buffer for a context without profiling.
- `zest_mtl_command_list_backend_t` grows by about 64 bytes, `zest_mtl_context_backend_t` by 16, `zest_mtl_committed_batch_t` by 24, `zest_mtl_device_backend_t` by 16, and `zest_shader_options_t` by 16 on every backend.

## [Testing_] Testing

**Automated, on the Mac (implementer)**
- **After every task:**
  - build `zest-tests`, `zest-c-compile-check` and `zest-objc-compile-check` with Metal on
  - then `zest-tests --platform metal` and `--platform metal --shader-validation`
- **At the end of every phase:**
  - `zest-tests --platform all`
  - the Metal-off build and its suites
  - one zombie and `MallocScribble` run of `--platform metal`
  - every target with Metal on and off
- **New backend tests,** taking `metal_tests[]` from 55 to 58: GPU Timestamps (in place of GPU Profiling Off), Pass Labels, Command Buffer Errors and MSL Shaders.
- **Shared tests:** "GPU Profiler Timestamps" joins the Metal suite, and "Shader MSL Source Platform" is new. The Vulkan suites go from 139 to 140, and the Metal suite from 134 to 136.

**Peter's tests**
- On Windows after Phase 1: `zest-tests.exe` with the default `--platform all`. Expected: `139 / 139` twice, `D3D12 suite: 135 / 135`, `D3D12 backend: 103 / 103`.
- On Windows after Phase 3: the same and `--gbv`. Expected: `140 / 140` twice, `D3D12 suite: 136 / 136`, `D3D12 backend: 103 / 103`, no VUID line. "Shader MSL Source Platform" passes on Vulkan and D3D12, and "Shader Cache Invalidation" still passes, so existing cache keys didn't move.
- On the Mac at the end, with the screen unlocked: `zest-tests --platform all`. Expected: `Metal backend: 58 / 58`.
- The existing tests nearest the changes: "GPU Profiler Timestamps", "Shader Cache Invalidation", "Shader Binary Platform Mismatch", "Shader Hot Reload Keeps Macros", "Metal Pass Encoder Order", "Metal Lazy Binding Order", "Metal Unwaited Graph Timeout" and "Metal Queue Switch Order".

**Visual checks for Peter**
Run each on the Mac from the repo root with `--validate`, so that it prints `validation errors: 0` on exit. With no backend argument it runs on Metal.
1. `zest-compute-example`:
   - The debug overlay and the profile window show GPU timings for "Compute Particles", its region "Compute Dispatch", and "Graphics Pass".
   - The total GPU time is close to the same example's with `--vulkan`, which takes MoltenVK's timestamps. Compare GPU timings, not frame rates (M6's frame pacing note).
2. `zest-render-targets`:
   - The profile shows "Downsampler Mip N" and "Upsampler Mip N" each with its own time, getting smaller with the mip.
   - The bloom looks as it did in M6, so splitting encoders for regions changed nothing on screen.
3. `zest-timelinefx-particle-techniques --effect-mode gpu`: the profile window shows GPU timings next to the CPU ones.
4. Open `spikes/metal/m7_pbr_deferred.gputrace` in Xcode on the Mac:
   - Each command buffer is named after its frame graph.
   - Each pass is a group named after the pass, with its encoders labelled the same.
   - The pipeline states carry their templates' names.

## [Built_] As built

### Phase 1: GPU timestamps (2026-11-25, on top of 9b25e45 with M6 uncommitted, built and run on the Mac over SSH)

**What changed**
- `zest_metal.h`:
  - `ZEST__MTL_PROFILE_SAMPLE_COUNT`, `zest_mtl_profile_encoder_t` and `zest_mtl_gpu_profiler_backend_t`, which also holds its `context` (for the lists' allocator and the log) and a `reported_resolve_failure` flag.
  - The command list backend's four profile members, and the context backend's compute and blit pass descriptors, made and released beside the render pass descriptor.
  - The five profiler members moved out of `Unavailable_stubs` into `GPU_Profiling_backend`, with the helpers `zest__mtl_profile_encoder_samples`, `zest__mtl_profile_encoder_ended`, `zest__mtl_profile_sample_buffer`, `zest__mtl_profile_render_encoder` (asks for four samples and attaches them to a render pass descriptor) and `zest__mtl_profile_encoder_tick`.
  - The encoder sites: `begin_render_pass`, `zest__mtl_pass_blit_encoder`, `zest__mtl_pass_compute_encoder`, `zest__mtl_image_clear` and `zest__mtl_blit_image_mip` take samples while profiling. The three encoder ends, the clear and the blit call the ended helper. `begin_command_buffer` empties the pending list and sets both encoder indexes to `ZEST_INVALID`.
- `zest-tests.cpp`: `metal_pending_tests[]`, `IsMetalPendingTest` and the check in `RegisterTest` are gone.
- `zest-metal-tests.cpp`: "Metal GPU Profiling Off" and its helpers are gone. "Metal GPU Timestamps" is new, registered where it was.

**Results**

| Run | Dynamic rendering | Legacy render pass | Metal suite | Metal backend |
| --- | --- | --- | --- | --- |
| Baseline `--platform all` (Task 1) | 139 / 139 | 139 / 139 | 134 / 134 | 55 / 55 |
| `--platform all` | 139 / 139 | 139 / 139 | 135 / 135 | 55 / 55 |
| `--platform metal --shader-validation` | | | 135 / 135 | 55 / 55 |
| `NSZombieEnabled=YES MallocScribble=1` | | | 135 / 135 | 55 / 55, no zombie report |
| Metal off | 139 / 139 | 139 / 139 | | |

- No VUID line. Every target builds with Metal on (26) and off (25). The only warning is `stb_image_write.h:776`'s deprecated `sprintf`, in the baseline too.
- Task 1's draw count check: `zest-minimal-template --metal --validate --frames 240` drew 240 with `validation errors: 0`. The patch still applies after Phase 1.
- "GPU Profiler Timestamps" passed 50 runs of 50 on Metal. One run's timings: "Profile Upload" 1.3 to 2.3 us, "Profile Draw" (an empty render pass to the swapchain) 46 to 97 us, "Profile Compute" 0 us, as its region wraps nothing and so no encoder opens. Totals were 236 to 3360 us, which include the waits between the three queues.
- "Metal GPU Timestamps", first run: copy 462 us, compute 24.8 us with Region A 3.5 us and Region B 2.3 us, draw 4.8 us, total 652 us. Totals in later runs were 568 to 9608 us, the gaps between queues again.
- With the draw count patch: `zest-compute-example --metal --validate --frames 600` drew 3604 with 600 dispatches, and `zest-render-targets` 1200 draws and 7200 dispatches. Both exit 0 with `validation errors: 0`. The compute example's log has one "Metal GPU timestamps: sampled" line and no "runs without" line.
- On Windows in Debug, `zest-tests` and `zest-c-compile-check` build with no warning.

**Breaks** (each on the clone and put back)
- No encoder takes samples: "GPU Profiler Timestamps" fails on a total of 0 in every frame.
- Every end mapped to its encoder's start: the copy, the draw, both regions and the profiled clear read 0.
- Readback without step 3: Region Empty's 0 ticks make the total 393519496648 us.
- No encoder split at a region's begin (the begin goes pending with the encoder left open): Region B reads 0.
- Decision 2's mapping removed: Region Draw spans its pass's end to its end.
- The sample buffer left on the compute pass descriptor: all three descriptor checks fail.

**Differences from the plan**
1. **A render encoder's end is the later of its vertex end and fragment end**, not "its fragment end, or its vertex end when that is 0". A probe (`spikes/metal/probe_m7/probe_stale_samples.mm`) showed that an encoder that writes no sample leaves the value from the last frame that used the index, not 0. Empty compute and blit encoders keep both of theirs, and a render encoder with no draws keeps its fragment samples while writing its vertex ones. Under the plan's rule a clear-only render pass, like Timed Draw, would read the stale fragment end, which is before its begin, and so read 0. The start stays the vertex start, which is written whenever the encoder runs. Zest opens compute and blit encoders on their first command, so they're never empty.
2. **"Timed Compute" has a 4-group dispatch outside any region, before Region B.** With only the plan's three regions, each region's begin finds no encoder open, as the region before it ended its encoder, so the begin's split never runs and its break couldn't fail. The extra dispatch writes the first half of the second buffer, and Region B's 8 groups write all of it, so the values check also proves the encoder Region B opened sent the pipeline and push constants again.
3. **"Timed Target" is flagged essential.** Nothing reads it, so the compiler culled it (`zest.h:16673`), which left the draw pass with no render pass and skipped its task, so Region Draw had no result.
4. **The clear graph runs twice**, with profiling on and then off, and checks the descriptors after each. The profiled run also checks that "Clear Pass" took time, which covers the clear's sample attachment. It clears an imported 16 x 16 target through the existing `run_metal_image_clear_graph`, as a transient written only by a transfer pass has no render target usage.
5. **A begin also clears its pair's end entry**, so a region that's never ended reads 0 rather than an encoder from an earlier frame.
6. **`end_command_buffer` is unchanged.** The three encoder ends call the ended helper themselves, and it calls them.
7. **`zest-render-targets` has its profiling flags commented out** (`zest-render-targets.cpp:370-374`), so its run measured nothing. A second run with them uncommented on the clone only gave 1800 draws, 7200 dispatches, exit 0, `validation errors: 0`, one profiling line and no out-of-samples line. Visual check 2 needs them on.

### Phase 2: names, markers and error reports (2026-11-25, on top of Phase 1, built and run on the Mac over SSH)

**What changed** (`zest_metal.h` and `zest-metal-tests.cpp` only, so no shared edits)
- Markers: `zest__mtl_begin_debug_marker` and `zest__mtl_end_debug_marker` in `Debug_naming`, in the platform table. The command list backend has `pass_label` and `debug_group_depth`. `end_command_buffer` pops what's left open and releases the label.
- Labels:
  - each batch's command buffer takes its frame graph's name in `set_next_command_buffer`
  - every pass encoder takes the pass label, including the clear and blit encoders, through a new `label` parameter on `zest__mtl_record_clear` and `zest__mtl_record_blit` (nil from the immediate path)
  - render pipeline descriptors take the template's name, compute states are built from an `MTLComputePipelineDescriptor`, and converted libraries take the shader's name
- Error reports:
  - the device backend's `batch_descriptor` (unretained, with encoder execution status when `api_validation` is set) and `failure_alerted`, which a reset clears
  - `zest_mtl_committed_batch_t` keeps its context and its frame or wave event and value
  - `zest__mtl_report_command_buffer_error`, with `zest__mtl_command_buffer_error_name` and `zest__mtl_encoder_state_name`
  - `zest__mtl_finish_command_buffer` takes `out_error_code`; the reaper moves a failed batch's events on and flags device loss
  - timeline waits wait in `ZEST__MTL_WAIT_SLICE` (100 ms) slices and reap between them
  - the forced hooks, read in `submit_frame_graph_batch` and `zest__mtl_finish_command_buffer`
  - the empty-frame present passes its frame timeline event to the committed batch too
- Tests: "Metal Pass Labels" and "Metal Command Buffer Errors".

**Results**

| Run | Dynamic rendering | Legacy render pass | Metal suite | Metal backend |
| --- | --- | --- | --- | --- |
| `--platform all` | 139 / 139 | 139 / 139 | 135 / 135 | 57 / 57 |
| `--platform metal --shader-validation` | | | 135 / 135 | 57 / 57 |
| `NSZombieEnabled=YES MallocScribble=1` | | | 135 / 135 | 57 / 57, no zombie report |
| Metal off | 139 / 139 | 139 / 139 | | |

- No VUID line. Every target builds with Metal on (26) and off (25), with only the `stb_image_write.h` warning.
- "Metal Command Buffer Errors": the failing graph's flush returned success after 101 to 103 ms, one wait slice. The report it wrote:
  ```
  A Metal command buffer failed: Timeout (2), The operation couldn’t be completed. (MTLCommandBufferErrorDomain error 2.). Command buffer: Failing Graph.
  Metal gave no pass states for this failure.
  ```
  Only the first failure on the device printed the alert.
- With the draw count patch, no backend argument and `--validate --frames 600`: `zest-minimal-template` 600 draws, `zest-timelinefx` 2400 draws and 600 dispatches, both exit 0 with `validation errors: 0`.
- On Windows in Debug, `zest-tests` and `zest-c-compile-check` build with no warning.
- **The capture:** `spikes/metal/m7_pbr_deferred.gputrace` on the Mac (183 MB), frame 30 of `zest-pbr-deferred --metal --frames 60` under `MTL_CAPTURE_ENABLED=1`, made by `spikes/metal/m7_capture_patch.py`, which was taken out. Its files hold the command buffer label "Deferred PBR", the pass names ("G-Buffer Pass", "Lighting Pass", "Composite Pass", "Upload ImGui Viewport") and the library labels (`gbuffer_vert.spv` and the others). It keeps pipeline states by hash, with their labels in a form `strings` can't read, so the pipeline names are for Peter's look in Xcode.

**Breaks** (each on the clone and put back)
- No pop in `end_debug_marker`: "Label Read" sees depth 2.
- No pop anywhere, `end_command_buffer`'s included: the depth after the batch is 2 as well. **The debug layer doesn't report an unbalanced group at commit**: the other 55 backend tests passed with no validation error.
- The label released in `end_debug_marker` but not set to nil: under `NSZombieEnabled=YES` the run stops on `-[CFString retain]: message sent to deallocated instance`.
- No event move: the failing graph's flush runs out the 10 s debug timeout (status 1), and the device-removed graph's flush times out too.
- No device-lost flag: the third check fails.
- The report skipped: all three log lines are missing.

**Differences from the plan**
1. **A compute's state is labelled with its shader's name, not the compute's.** `zest_compute_t` keeps no name: `zest__new_compute` drops the one `zest_CreateCompute` is given (`zest.h:21842`), and this phase can't edit `zest.h`. So the test checks the shader's name, `label_buffer_write`. Keeping the compute's name would take a `name` member on `zest_compute_t`, which is Peter's call.
2. **"Metal Pass Labels" has two compute passes in one batch**, "Label Compute" and "Label Read", and checks they recorded into the same command buffer. With one pass, `end_command_buffer`'s pop would hide a missing pop in `end_debug_marker`, so the plan's break couldn't fail. The test reads the command list backend once after the flush, as the flush resets the graph's allocator without reusing it.
3. **`zest__mtl_record_clear` and `zest__mtl_record_blit` take a label**, so the clear and blit encoders a pass records carry its name as well.
4. **The forced hook keeps its code until the reap** rather than resetting at the submit, so `zest__mtl_finish_command_buffer` can build the error from it. A second batch committed before the reap isn't forced, as the hook only takes a batch while no forced buffer is outstanding.
5. **The report also goes through `ZEST_REPORT`**, as the old failure line did, and the test resets the context's reports.
6. **The empty-frame present's batch records its frame timeline event**, so a failure there moves the frame wait on too.

### Phase 3: MSL as a shader input (2026-11-25, on top of Phase 2, built and run on the Mac over SSH)

**What changed**
- `zest.h`:
  - `zest_shader_options_t` gains `msl` and `local_size[3]`. `zest_SetShaderOptionsMSL` is declared after `zest_FreeShaderOptions` and defined after `zest_AddMacroDefinition`.
  - `zest__copy_shader_options` copies both fields, and clears them for NULL options.
  - `zest__shader_language_refused` logs the validation error. `zest_CreateShader` calls it before making a shader, and `zest_CompileShader` before compiling.
  - `zest_CreateShader` neither reads nor writes the cache for MSL.
  - `zest_CreateShaderFromFile` frees the source and returns the zero handle when it gets one.
  - `zest__shader_cache_key` hashes `msl` and the local size only when `msl` is set.
  - `zest_ValidateShader`'s comment and the two slot comment lines.
- `zest_metal.h`:
  - `zest__mtl_compile_shader` copies MSL with its terminating zero.
  - `zest__mtl_new_library` is split out of `zest__mtl_compile_msl` and labels the library. A failed compile now reads "Metal couldn't compile the MSL of shader <name>" on both paths.
  - `zest__mtl_msl_function` is new, and `zest__mtl_shader_function` branches to it.
- `zest-pipeline-tests.cpp` and `zest-tests.cpp`: "Shader MSL Source Platform". `zest-metal-tests.cpp`: "Metal MSL Shaders".

**The declaration probe** (`spikes/metal/probe_m7/probe_msl_declarations.mm`, run plain and under `MTL_DEBUG_LAYER=1 MTL_SHADER_VALIDATION=1`)

| Declaration | Compiles | Entry 1 reads |
| --- | --- | --- |
| `const device texture2d<float> *textures [[buffer(6)]]` | no: "type 'const device texture2d<float> *' is not valid for attribute 'buffer'" | |
| `struct TextureEntry { texture2d<float> value; };` and `const device TextureEntry *textures [[buffer(6)]]` | yes | 5 6 7 8, right |
| `const device sampler *samplers [[buffer(5)]]` | no, the same error | |
| `struct SamplerEntry { sampler value; };` and `const device SamplerEntry *samplers [[buffer(5)]]` | yes | 0.5 from a linear sampler at entry 1, right |
| `device Value *const device *buffers [[buffer(10)]]` | no, the same error | |
| `struct BufferEntry { device Value *value; };` and `const device BufferEntry *buffers [[buffer(10)]]` | yes | 22, right |

So a slot is always declared as a pointer to a struct holding one entry, which is SPIRV-Cross's shape too. The test and the docs use those forms.

**Results**

| Run | Dynamic rendering | Legacy render pass | Metal suite | Metal backend |
| --- | --- | --- | --- | --- |
| `--platform all` | 140 / 140 | 140 / 140 | 136 / 136 | 58 / 58 |
| `--platform metal --shader-validation` | | | 136 / 136 | 58 / 58 |
| `NSZombieEnabled=YES MallocScribble=1` | | | 136 / 136 | 58 / 58, no zombie report |
| Metal off | 140 / 140 | 140 / 140 | | |

- No VUID line. Every target builds with Metal on (26) and off (25), with only the `stb_image_write.h` warning.
- "Shader MSL Source Platform" and "Shader Cache Invalidation" pass in all three shared suites.
- On Windows, every project in Debug (28, `zest-c-compile-check` included) builds with no warning, after Task 6 and again at the phase's end.

**Breaks** (each on the clone and put back)
- No platform refusal, on Vulkan: shaderc's error asserts in `zest_CreateShader` (`zest.h:13492`).
- The input mask from the first attribute only: the draw leaves rows 0 and 15 at the clear colour.
- The local size ignored: words 4 onwards are never written, before and after the reload.
- No multiview refusal: the refusal check fails.

**Differences from the plan**
1. **The function cache keys an MSL shader by its local size too.** The test found this. The unsized kernel's text matched the write kernel's text from before its reload, so it got that cached function, with a local size of 8, and wasn't refused. Two computes from one MSL text with different local sizes would have shared a function the same way.
2. **`zest-tests.h` is unchanged.** The test files compile into `zest-tests.cpp` as one unit, so the test needs no declaration, as "Shader Binary Platform Mismatch" has none.
3. **"Shader MSL Source Platform" checks more than the plan says.** With caching turned on for the test, it checks that Metal writes no cache file for the MSL shader. On Vulkan and D3D12 it also checks that `zest_CompileShader` refuses MSL options with one validation error.
4. **The draw is two triangles over the top half** of Zest's clip space. Rows 0 to 7 must hold the texture's colour and rows 8 to 15 the clear colour, rather than only rows 0 and 15.
5. **Hot reload uses one file kernel** in the working folder, `zest_metal_msl_write.metal`, which is also the compute check. It's removed at the end.
6. **The refusals.** The two-vertex source is refused when its pipeline is built from a backend-less command list, as "Metal Pipeline Cache" builds them. The multiview refusal uses the MSL draw's vertex shader with the GLSL multiview fragment. Each check also matches the error's text.
7. **An MSL function is taken with `newFunctionWithName:` and no constant values.** A hand-written function with function constants isn't specialised, so it would fail at pipeline creation. That's a limit for the docs.

### Phase 4: docs and the final check (2026-11-25, on top of Phase 3)

**What changed** (docs and skills only, no code)
- `docs/concepts/bindless.md`: the contract list names Metal for GLSL and SPIR-V and gives MSL's buffer indexes. A new "MSL Descriptor Layout" section has the nine slots with their buffer indexes and entry structs, push constants, vertex bindings, Y, the function rule, compute local size, no preprocessor pass, what isn't supported, and the draw and kernel from "Metal MSL Shaders".
- `docs/concepts/rendering-conventions.md`: one line under "Clip Space / NDC".
- `docs/api-reference/pipeline.md`: a `zest_SetShaderOptionsMSL` section after `zest_CreateShader`. Also Metal in the shader language, macro and binary format sentences, and `zest_ValidateShader`'s GLSL or HLSL only.
- `docs/getting-started/installation.md`: captures and error reports, and warm starts with shader caching, under "The Metal Backend".
- `docs/examples/index.md`: "Limits on Metal" now says how timings sample.
- `docs/api-reference/index.md`, `docs/concepts/device-and-context.md`, `docs/index.md`: the three backends.
- `README.md`: the four lines Task 8 gives, in the plan's wording.
- Skills (git-ignored): `metal-backend` is new. `gpu-profiling` has a "Metal Backend" section in place of the M6 stub's paragraph. `pipelines-and-shaders` has `zest_SetShaderOptionsMSL` and an MSL input paragraph. `CLAUDE.md`'s list names `metal-backend/`.
- `metal_platform_layer_implementation.md`: the roadmap names this file and M7's row follows Decision 1, the three [Mapping_] rows and "Shader creation" say what was built, [Scope_]'s cache line follows Decision 1, and [Built_] has M7.
- `qgrep update zest` run. Every function the changed docs name exists, and every buffer index matches `zest_metal.h`.

**A second slot probe** (`spikes/metal/probe_m7/probe_msl_slot_forms.mm`), for the slots Task 7's probe didn't cover, plain and under the debug layer and shader validation: the struct entry forms for `texturecube`, `texture2d_array`, `texture3d`, `texture2d<float, access::read_write>` and `texturecube_array` compile, and a uniform buffer entry reads right as a `constant T *` or a `const device T *`. The docs give `constant T *`.

**The final check** (Mac, screen unlocked)

| Run | Dynamic rendering | Legacy render pass | Metal suite | Metal backend |
| --- | --- | --- | --- | --- |
| `--platform all` | 140 / 140 | 140 / 140 | 136 / 136 | 58 / 58 |
| `--platform metal --shader-validation` | | | 136 / 136 | 58 / 58 |
| `NSZombieEnabled=YES MallocScribble=1` | | | 136 / 136 | 58 / 58, no zombie report |
| Metal off | 140 / 140 | 140 / 140 | | |

- No VUID line. Every target builds with Metal on (26) and off (25). The only warning is `stb_image_write.h:776`'s `sprintf`, in the baseline too.
- With the draw count patch, no backend argument and `--validate --frames 600`, each exits 0 with `validation errors: 0`:
  - `zest-minimal-template`: 600 draws.
  - `zest-compute-example`: 3598 draws and 600 dispatches, one "Metal GPU timestamps" line.
  - `zest-render-targets`: 1200 draws and 7200 dispatches. Its profiling flags are commented out, so it measured nothing (Phase 1, difference 7).
  - `zest-timelinefx-particle-techniques --effect-mode gpu`: 4193 draws and 1124 dispatches, one "Metal GPU timestamps" line.
- On Windows, every project in Debug (28, `zest-c-compile-check` included) builds with no warning.
- "Resource Test Concurrent Pool Growth" didn't abort in any run of M7.
- The Mac clone is back at 9b25e45 with only `spikes/` untracked.

**Shared edits for Peter's Windows run, over the whole milestone**
- `zest.h` (Phase 3): the MSL option, its setter, the refusal, the cache key, `zest_CreateShaderFromFile`'s zero handle, `zest_ValidateShader`'s comment and the two slot comment lines.
- `zest-tests.cpp`: the Metal pending list gone (Phase 1), "Shader MSL Source Platform" registered (Phase 3).
- `zest-pipeline-tests.cpp`: "Shader MSL Source Platform" (Phase 3).
- No change to `zest_vulkan.h`, `zest_dx12.h`, any submodule or any shader file.

**Differences from the plan in Phase 4**
1. **More doc lines than Task 8 lists.** `docs/index.md` still called Metal future, and `pipeline.md`'s shader language, macro and binary format sentences named only Vulkan and D3D12. They now name Metal.
2. **The slot table covers all nine slots**, from a second probe. Task 7's covered three. `bindless.md`'s own resource table, which stops at binding 7, was left as it was.
3. **The gpu-profiling skill gains a "Metal Backend" section**, beside D3D12's and Vulkan's, rather than only losing its M6 paragraph.

### Fixes from the sanity check (2026-11-25)

Peter asked for the CI push trigger back on, and then for these fixes.
- **CI.** The push trigger in `.github/workflows/ci.yml` is back, as it was before `fc6e8705`. The macOS job's runner has no SPIRV-Cross, and Metal has been on by default on Apple since M6, so its configure would have stopped at `ZestMetal.cmake:32-41`. Its matrix entry now passes `-DZEST_ENABLE_METAL=OFF` through a new `cmake_args`, so macOS CI builds Vulkan through MoltenVK as it did before M6. Not run, as nothing here pushes.
- **A template with no name** no longer throws on Metal: the render pipeline descriptor is only labelled when the template has a name. `zest_CreatePipelineTemplate` doesn't require one, and Vulkan and D3D12 accept NULL. No test covers it.
- **Device loss from another thread.** The reaper can run on any thread's timeline wait, and shared code writes `context->flags` without atomics, so a reap on another thread could lose a flag. The context backend now keeps `recording_thread` and `device_lost_pending`. `zest__mtl_report_device_lost` flags the context only on its recording thread and otherwise leaves the loss pending, and `zest__mtl_take_device_lost` folds it in at the context's next batch or frame wait. That's a `pthread_self` and an atomic load per batch and per frame wait.
  - "Metal Command Buffer Errors" has a new phase on a second headless context: an unwaited removed graph, reaped by a thread waiting on the queue-ordering timelines, reads not lost until the context's next batch, then lost.
  - Breaks: flagging from any thread makes the second context read lost straight after the reap, and never folding leaves it not lost after its next batch. Both fail the test.
- **The forced-failure hook** is reset at the end of "Metal Command Buffer Errors" after the idle wait, error code and buffer both, so a failure that left the forced batch unreaped neither hangs the wait nor marks a later buffer as failed.
- **The pending-region push** returns when the list is full, after its assert, so a release build can't write past it.
- Results: `--platform all` gave `140 / 140` twice, `Metal suite: 136 / 136`, `Metal backend: 58 / 58`, no VUID line. The same Metal lines with shader validation and under `NSZombieEnabled`, with no zombie report. All 26 Metal targets build with only the `stb_image_write.h` warning. Nothing Windows compiles changed, so Windows wasn't rebuilt.
