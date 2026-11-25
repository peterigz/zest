# Metal M5: Full frame graph

**Goal:** a multiview pass on Metal draws one view per layer of its attachments, as on Vulkan and D3D12, so a pipeline with a view count works unchanged. Backend tests prove the three things Metal's frame graph ordering rests on: the shared events between queues, the wait a batch makes when its queue slot moves to another device queue, and the fence every encoder waits on and updates. With Decision 1, an unwaited command graph flush stops releasing its transients early, which removes the last failure under shader validation.

**Architecture:**
- **Multiview** uses SPIRV-Cross's own scheme: each view is an instance. MoltenVK's limits on this Mac fit the same scheme ([Probe_]).
  - The shaders of a pipeline with a view mask are converted with `SPVC_COMPILER_OPTION_MSL_MULTIVIEW`. The vertex stage then takes the first view and the view count at buffer index 24, splits the instance index into a view and an instance, and writes the view to `[[render_target_array_index]]`. The fragment stage reads the view back from there ([Probe_]).
  - The backend multiplies each draw's instance count and each per-instance step rate by the view count, sets the pass's `renderTargetArrayLength`, and sends the 8 bytes at index 24 when a multiview pipeline is bound.
  - An indirect draw's instance count is in a GPU buffer, so the backend can't multiply it, and a multiview pass refuses indirect draws.
- **Ordering** keeps M2's design: shared events between queues and between device queues of one slot, one fence per queue inside it. Three new backend tests make a missing wait fail. Two hold a queue back behind a gate, a command buffer that waits on a test-owned shared event that a thread opens after a delay. The third chains passes that Metal runs out of order without fences.
- **The race (Decision 1)** is a shared fix in `zest_FlushFrameGraph`. When the flush can't prove the GPU finished, the graph's transient bindless indexes move to the context's deferred list for the frame in flight, as its arena returns already do. Its per-mip views can't move, and are documented (Phase 1 As built).

**Tech stack:**
- `zest_metal.h` and `zest-metal-tests.cpp`.
- Shared, only with Decision 1: `zest.h` (`zest_FlushFrameGraph`) and `zest-compute-tests.cpp` (one test).
- No change to `zest_vulkan.h`, `zest_dx12.h`, any shader file or CMake. No new dependency.

**Spec:**
- `metal_platform_layer_implementation.md`: the M5 row of [Roadmap_], the [Mapping_] rows "Ordering inside a queue" and "Crossing queues", Spike A's multiview note and Spike B's ordering results under [Built_] (Tasks 2 and 3).
- `metal_m4_implementation.md` [Built_], "For M5's plan".
- D3D12's M5 plan (`docs/superpowers/plans/dx12_m5_full_frame_graph_implementation.md`, git-ignored, on the Windows machine only) is the model for multiview (its Task 7). What this plan needs from it is carried here.

## Contents

- [Constraints_] Global Constraints
- [Settled_] Settled
- [State_] Current state
- [Probe_] What the SDK and SPIRV-Cross say
- [Decisions_] Decisions for Peter
- [Review_] Review Focus
- [Scope_] Out of scope
- [Phase_1] Baseline and the flush race
- [Phase_2] Ordering
- [Phase_3] Multiview
- [Phase_4] Docs and the final check
- [Cost_] Per-frame cost
- [Testing_] Testing
- [Built_] As built

## [Constraints_] Global Constraints

Everything in `metal_m4_implementation.md` [Constraints_] holds:
- never commit
- edit on Windows, and build and run on the Mac over `ssh macmini`
- restore the Mac clone at the end of every phase (`git checkout -- <files>`, `spikes/` stays untracked)
- write a script with a backslash or an apostrophe to a file with the Write tool
- break every new test on purpose once
- follow code-style

What M5 adds:

- **Never edit** `zest_vulkan.h` or `zest_dx12.h`. `zest.h` is edited only in Task 2, and only if Decision 1 is taken. A problem found anywhere else in it goes to Peter with a repro.
- **Shared edits** are only the files under Tech stack. After Task 2, build `zest-tests` and `zest-c-compile-check` on Windows in Debug. The Windows suites are Peter's. The C++ sources are CRLF: check with `file` before scripting an edit.
- **Budget.** Vulkan and D3D12 get no new per-frame work. On Metal, a single view draw gains one multiply and a pass one branch. A multiview pipeline bind sends 16 bytes. A draw still allocates nothing, takes no lock and opens no autorelease pool. See [Cost_].
- **Threads in tests.** The gate threads use pthreads, in `zest-metal-tests.cpp` only. Every test joins its thread and opens its gate on every exit path, so a failed check never leaves a queue blocked.
- **Runs.** The implementer runs `zest-tests` and `--frames N` example runs. Launching an example without `--frames` is Peter's. Before judging a windowed failure, check the Mac's screen: `ssh macmini 'ioreg -n Root -d1 | grep IOConsoleLocked'`.
- **Baseline:** commit 41ed835. `Dynamic rendering: 129 / 129`, `Legacy render pass: 129 / 129`, `Metal suite: 124 / 124`, `Metal backend: 46 / 46` with the screen unlocked (45 / 46 locked, "Metal Windowed Frames"), no VUID line. `--platform metal --shader-validation`: `Metal suite: 123 / 124`, the failure being "Compute Test Immediate Execute No Wait". Task 1 confirms these.

## [Settled_] Settled

The implementer doesn't reopen these.

- **From the design, not reopened:**
  - Three queue families, each queue its own `MTLCommandQueue` (decision 2).
  - The shared event orders a crossing between queues, with no barrier on either side.
  - One `MTLFence` per queue orders the encoders on it.
  - State with no native form is a pipeline-creation error and a documented limit.
- **Multiview draws each view as an instance, in layered rendering** ([Probe_]).
  - Why: it is SPIRV-Cross's built-in scheme, so the converted shaders need no edits. MoltenVK's limits on this Mac, 32 views and instance indexes below 2^32 / 32, point to it drawing views as instances too.
  - Rejected: vertex amplification. Metal allows an amplification count of at most 2 ([Probe_]), and zest-cascading-shadows draws 4 views.
  - Rejected: SPIRV-Cross with layered rendering off, which needs one render pass per view and so a pass callback run per view.
- **Both stages of a multiview pipeline are converted with the multiview option**, as a vertex shader that never reads `gl_ViewIndex` still has to split the instance index and write the layer ([Probe_]). The function cache key gains the flag, so one SPIR-V can have a single view and a multiview function.
- **The view mask buffer is index 24**, SPIRV-Cross's default, set explicitly through `SPVC_COMPILER_OPTION_MSL_VIEW_MASK_BUFFER_INDEX` and named `ZEST__MTL_VIEW_MASK_BUFFER_INDEX`. It holds the first view (always 0, since Zest's masks start at bit 0, `zest.h:12950`, `zest.h:17010`) and the view count. It collides with nothing: vertex buffers are 0 to 3, push constants 4 and the table 5 to 13 (`zest_metal.h:137-139`).
- **A pipeline's view mask must equal its pass's**, as on Vulkan. D3D12's message is reused: "The pipeline %s draws %u views but its render pass has %u. They must match, as on Vulkan." (`zest_dx12.h:4723-4725`).
- **Ordering tests use gates where they can.** A held queue makes a missing wait fail every run, where timing a long kernel fails some runs. D3D12's ordering test holds its queue with a fence the same way (`zest-d3d12-tests.cpp:3456-3487`). Encoders inside one command buffer can't be held from outside, so the fence test relies on Metal overlapping them, as Spike B measured.

## [State_] Current state (verified 2026-11-24, Windows tree at 41ed835)

**What the roadmap's M5 row lists, and what is already there**
- **Multi-queue and async compute.** Three families: graphics with 4 queues, compute with 4, transfer with 2 (`zest_metal.h:395-399`). Each queue is its own `MTLCommandQueue` and fence (`zest__mtl_new_queue_backend`, `zest_metal.h:2099-2114`).
  - A batch takes its command buffer from the device queue its context queue acquired (`zest__mtl_set_next_command_buffer`, `zest_metal.h:4393-4416`).
  - The compiler folds only single-queue waves onto the graphics queue (`zest.h:16256-16268`). So "Multi Queue Sync", whose first wave is a compute and a transfer pass (`zest-frame-graph-tests.cpp:923-990`), already commits on the compute and transfer queues. No test checks it, which M4 asked M5 to settle.
- **Shared events.**
  - `zest__mtl_begin_command_buffer` encodes the wave's waits before any pass (`zest_metal.h:4419-4428`). The first wave waits on the graph's wait timeline instead (`:4429-4432`).
  - A batch whose slot moved to another device queue also waits on the slot's ordering event (`:4434-4438`). So does the empty-frame present (`:2036-2039`).
  - `zest__mtl_submit_frame_graph_batch` signals the batch's event, or the graph's signal timeline on the last wave, then the slot's ordering event (`:4603-4664`).
- **Transient aliasing.** The packer's rule (`zest.h:14748-14752`) orders reuse inside a batch by barriers, which on Metal means the queue's fence, and across waves by the wave semaphores, which are the shared events. It never aliases between concurrent batches of one wave. So Metal needs nothing more, and cross-wave reuse rests on the event wait Task 3 tests.
- **Cached graphs, the device-reset tests, the stress and compute tests** all pass on Metal since M4's sweep (M4 [Built_], Phase 4).
- **Indirect draws** came into M4 (its Decision 1).
- **The command buffer count** the design's [Lessons_] gave to M5 is done: `queue->next_buffer++` (`zest_metal.h:4414`).

**Fences**
- Every encoder waits on its queue's fence when it opens and updates it when it ends:
  - the pass blit encoder (`zest_metal.h:4454-4475`)
  - the pass compute encoder (`:4478-4503`)
  - the render encoder, waited in `begin_render_pass` (`:4593`) and updated in `end_render_pass` (`:4445-4451`)
  - the immediate encoders (`:2498-2536`)
  - M3's clears and blits (`zest__mtl_record_clear`, `zest__mtl_record_blit`)
- "Metal Untracked Pass Ordering" encodes its own fences (`zest-metal-tests.cpp:3241-3366`), so it can't catch a missing call in the backend. M4 lists the pass encoders' fence calls under "Checks that can't fail yet".
- Spike B: with no fences, compute then render in one command buffer read the previous iteration's data in more than 190 of 200 runs (design [Built_], Task 3).

**Multiview**
- `max_multiview_view_count` is 1 (`zest_metal.h:1696`). `zest_SetPipelineViewCount` clamps to it (`zest.h:12946-12951`), so a template asking for 4 views gets a mask of 1 on Metal.
- `zest__mtl_pipeline_description` refuses any template view mask (`zest_metal.h:3329-3332`), and `begin_render_pass` any pass view mask (`:4552-4554`).
- **Shared code** gives a pass the mask of its layered attachments (`zest.h:17007-17015` for colour, `:17061-17069` for depth). It asserts at most 32 layers (`:17009`, `:17063`), and fails the graph with `zest_fgs_invalid_render_pass` past the device's count (`zest__check_view_count`, `:16962-16969`).
- **Conversion:** the options are set at `zest_metal.h:5164-5169`. The function cache key is the SPIR-V's bytes and the stage (`:5281`). `zest__mtl_pipeline_shader_function` wraps it (`:3229`).
- **The pipeline:**
  - The vertex description gives each binding step rate 1 (`zest_metal.h:3298-3299`).
  - The key struct is at `:431-440` and the pipeline backend at `:368-378`.
  - The flush's pipeline-change block is at `:4704-4713`.
- **Draws:** `zest__mtl_draw` and `zest__mtl_draw_indexed` (`zest_metal.h:4877-4897`), indirect at `:4904-4932`. `draw_layer_instruction` goes through `zest__mtl_draw`.
- **The render pass descriptor.** `begin_render_pass` fills the context's one descriptor and clears its textures once the encoder exists (`zest_metal.h:4580-4587`). The empty-frame present (`:2040-2045`) and the frame graph blit and clear helpers use the same descriptor.
- A layered image is a 2D array texture (`zest_metal.h:1110`, `:3596-3614`).
- **The only user** is zest-cascading-shadows (`zest-cascading-shadows.cpp:130`), an M6 example. Its `depthpass.vert` reads `gl_ViewIndex` and per-instance attributes.
- "Metal Pipeline State Translation" expects a template view mask to be refused by name (`zest-metal-tests.cpp:3871-3884`).

**The flush race (Decision 1)**
- A command graph gets its own deferred list (`zest.h:15510-15515`). Frame graphs inside a frame use the context's.
- `zest_FlushFrameGraph` (`zest.h:17096`) releases that list's transient bindless indexes, images and view arrays at `:17169-17195`, whether or not it waited. Its arena returns stay deferred unless the wait succeeded (`:17126-17141`).
- The context's lists drain in `zest__do_context_scheduled_tasks` (`zest.h:10173-10201`), which `zest_BeginFrame` runs, and which a waited flush on a headless context runs too (`:17142-17157`).
- "Compute Test Immediate Execute No Wait" (`zest-compute-tests.cpp:563`, registered at `zest-tests.cpp:116`) flushes two graphs with no wait, then a third and waits.

**Wiring to mirror**

| What | Where | Its test |
| --- | --- | --- |
| A queue held behind a gate | `run_cross_queue_ordering`, `zest-d3d12-tests.cpp:3456-3487` | "D3D12 Windowed Frames" |
| Records of a graph that crosses queues, read after the flush from a cached graph | `test_d3d12_cross_queue_barriers`, `zest-d3d12-tests.cpp:5457-5545` | itself |
| View instancing | `zest_dx12.h:4719-4726`, `:4845-4890` | "D3D12 View Instancing", `zest-d3d12-tests.cpp:5872` |
| Batches in a graph | `metal_graph_batch_count`, `zest-metal-tests.cpp:1529` | |
| Reading one layer of an image | `metal_read_texture`, `zest-metal-tests.cpp:101` | |

`zest_dx12.h` is the structural model only.

## [Probe_] What the SDK and SPIRV-Cross say (the Mac, 2026-11-24)

- **SPIRV-Cross C API 0.67.0** (Vulkan SDK 1.4.335.1) has `SPVC_COMPILER_OPTION_MSL_MULTIVIEW`, `SPVC_COMPILER_OPTION_MSL_VIEW_MASK_BUFFER_INDEX` and `SPVC_COMPILER_OPTION_MSL_MULTIVIEW_LAYERED_RENDERING` (`spirv_cross_c.h:676-677`, `:718`).
- **What the conversion gives.** Four small GLSL shaders went through `glslc -DZEST_METAL=1` and the `spirv-cross` CLI with the backend's options, with and without `--msl-multiview`:
  - **A vertex shader that never reads `gl_ViewIndex`**, with multiview on, still takes `constant uint* spvViewMask [[buffer(24)]]`, `[[instance_id]]` and `[[base_instance]]`, and computes:
    ```
    gl_ViewIndex = spvViewMask[0] + (gl_InstanceIndex - gl_BaseInstance) % spvViewMask[1];
    gl_InstanceIndex = (gl_InstanceIndex - gl_BaseInstance) / spvViewMask[1] + gl_BaseInstance;
    out.gl_Layer = gl_ViewIndex - spvViewMask[0];	//[[render_target_array_index]]
    ```
    A shader that reads `gl_ViewIndex` gets the same lines.
  - **A fragment shader** with multiview on takes `spvViewMask` at 24 and `uint gl_ViewIndex [[render_target_array_index]]`, then adds `spvViewMask[0]`. One that never reads `gl_ViewIndex` still takes `spvViewMask`.
  - **Without multiview**, `gl_ViewIndex` is the constant 0 in both stages. A vertex shader that declares `GL_EXT_multiview` also gets a `gl_Layer [[render_target_array_index]]` member in its output struct that nothing writes. Whether Metal accepts that in a single-view pipeline is Task 6's question 3.
- **Vertex amplification** takes a count of "at most 2" (`MTLRenderCommandEncoder.h:298`).
- **Layered rendering:** `MTLRenderPassDescriptor.renderTargetArrayLength` (`MTLRenderPass.h:324`) and `MTLRenderPipelineDescriptor.inputPrimitiveTopology` (`MTLRenderPipeline.h:204`).
- **MoltenVK on this Mac** reports `maxMultiviewViewCount = 32` and `maxMultiviewInstanceIndex = 134217727`, which is 2^32 / 32 - 1 (`vulkaninfo`).
- The `metal` command-line compiler isn't installed (Xcode's Metal toolchain is missing). The backend doesn't need it, since `newLibraryWithSource` compiles at run time, and Task 6 compiles that way.

## [Decisions_] Decisions for Peter

**Settled 2026-11-24: Peter took the recommendation on all three.** M5 fixes the flush race in Task 2, MSAA goes with the MSAA resolve plan, and multiview gets backend tests only.

1. **The flush race.** "Compute Test Immediate Execute No Wait" fails under Metal shader validation, because `zest_FlushFrameGraph` releases an unwaited graph's transient bindless indexes. The next graph then rewrites those table entries before the GPU reads them. Every backend has the race. It also frees the graph's transient images and view arrays straight away.
   - **Recommended:** M5 fixes it in Task 2, a shared edit of about 20 lines in `zest_FlushFrameGraph`. When the flush didn't prove the GPU finished, the three lists move to the context's lists for the frame in flight, as the arena returns already do. "Compute Test Immediate Execute No Wait" gains a check that fails today on every backend. Peter's Windows run gates it.
   - Alternative: Peter fixes it outside M5, and the shader validation run keeps that one failure until he does.
2. **MSAA.** `get_msaa_sample_count` still reports, and no milestone owns it. Vulkan builds every pipeline with one sample (`zest_vulkan.h:4020`), and the MSAA resolve plan is pending (design [Scope_]).
   - **Recommended:** it stays out of M5 and goes with the MSAA resolve plan. Metal's part is then the sample count hook and a resolve store action at `begin_render_pass`, which reports today (`zest_metal.h:4549-4551`).
   - Alternative: M5 does Metal's part now. Metal would then build multisampled pipelines that Vulkan can't match.
3. **Where the multiview test lives.** No shared test covers multiview, so Vulkan has none.
   - **Recommended:** backend tests only, "Metal Multiview" and "Metal Multiview Rules", mirroring "D3D12 View Instancing". No shared edit and no HLSL twin.
   - Alternative: a shared test in `zest-pipeline-tests.cpp`, with GLSL and an HLSL twin. It gives Vulkan its first multiview test and runs on all three backends, for one more shared edit and a Windows run.

**Calls made without Peter (check these)**
- **The view limit is 32**, what MoltenVK reports on the same GPU. Zest's mask is 32 bits, and shared code asserts at most 32 layers. D3D12 reports 4.
- **An indirect draw in a multiview pass is refused** with a validation error, and the docs list the limit. The instance count sits in a GPU buffer the backend can't multiply. Supporting it would take a compute pass that rewrites the commands before the render pass, as MoltenVK does, which is code for a case no Zest user has.
- **Instance indexes in a multiview pass** must stay below 2^32 divided by the view count, as on MoltenVK. The docs say so, and nothing checks it.
- **Both stages get the 8 bytes** at every multiview pipeline bind, though only the stages that read the view need them. It costs two calls and keeps the flush simple.
- **The fence is not refined from the barrier records.** The design's [Mapping_] row allows that only if profiling shows a cost, and GPU timestamps come in M7.
- **The immediate encoders' fence calls** and M3's clears and blits keep the tests they have. Task 5 covers the frame graph's pass encoders, which M4 listed as uncovered.

## [Review_] Review Focus

1. **Instance data is wrong under multiview.** The step rate isn't multiplied, the base instance is lost, or `gl_InstanceIndex` reads the raw instance ID. One view draws right and the other draws another instance's data. Probes: Task 6's question 1, then the instance case of "Metal Multiview", which draws from base instance 1.
2. **Layered state leaks out of a multiview pass.** `renderTargetArrayLength` stays on the context's shared descriptor, so a later single layer pass, clear, blit or empty-frame present renders wrong or raises a debug layer error. Probe: "Metal Multiview Rules" runs a single layer pass and an image clear after a multiview pass on one context.
3. **A conversion cached for one view count serves the other.** A single view function in a multiview pipeline draws one layer, and the reverse makes a single view pipeline read buffer index 24. Probe: "Metal Multiview Rules" builds one shader pair into a single view and a two view template, in both orders, with `zest_ResetDevice` between.
4. **An ordering test passes with its wait removed.** Probe: each of Tasks 3 to 5 is broken once for every call it guards. A call no test catches goes under the checks that can't fail yet, as M4 did.
5. **The flush fix changes what Vulkan and D3D12 free and when** (Decision 1). Probes: Peter's Windows suites, the "Device Reset Memory Stability" and headless flush tests, and the new check in "Compute Test Immediate Execute No Wait".

## [Scope_] Out of scope

- Indirect draws in a multiview pass (Calls above).
- zest-cascading-shadows with `--metal`, which is M6's with the other examples. It is the first example that draws with multiview, so M6's run of it is the visual check of Phase 3.
- MSAA, per Decision 2.
- Deferring an unwaited command graph's per-mip views, which live in frame-linear memory. Documented for now (Phase 1 As built).
- A fence refined from the barrier records (Calls above).
- GPU timestamps, markers and labels, and the five profiler stubs (M7).
- Slang on Metal (M6). The Slang tests aren't built on the Mac.
- User docs and the README line (M7). Phase 4 updates the skills only.

## [Phase_1] Baseline and the flush race

### Task 1: Baseline

**Files:** none.

**Steps:**
- [x] Check the Mac's screen with `IOConsoleLocked`. Sync the tree at 41ed835 and build every target with Metal on.
- [x] Run `zest-tests --platform all` and `--platform metal --shader-validation`. Expected: the numbers under [Constraints_]. With the screen locked, "Metal Windowed Frames" fails and the rest match. Record the run, and stop and report if anything else differs.

### Task 2: An unwaited flush keeps its transients (only if Decision 1 is taken)

**Files:**
- `zest.h:17169-17195`, in `zest_FlushFrameGraph`.
- `zest-compute-tests.cpp:563`: "Compute Test Immediate Execute No Wait".

**Interfaces:**
- No new function. `zest_FlushFrameGraph` keeps a flag that is false only when the graph was executed and its wait didn't return `zest_semaphore_status_success`: it had no signal timeline, or the wait timed out. A graph that never ran, had no work, or failed part way (`zest.h:17162-17168`, which drains in-flight work first) counts as finished.
- When the flag is false, each of the graph's three lists for `context->current_fif` is pushed record by record onto the matching list of `context->deferred_resource_freeing_list`, and then freed. The images are copied by value, and keep their `buffer` and `default_view` pointers.
- When it is true, the lists are released as today.
- The comment: `//Without a wait the GPU may still be using these, so they go with the context's other deferred releases for this frame in flight`.

**Steps:**
- [x] Add the check to "Compute Test Immediate Execute No Wait": the first two graphs' transient storage buffers must get different bindless indexes. The test-local task does what `zest_WriteBufferCompute` does (`zest-frame-graph-tests.cpp:288`) and records `push.index1`. Build on Windows in Debug and on the Mac. Expected: the check fails on the Mac's three suites, since both graphs get the index the first released.
- [x] Implement.
- [x] Find where the deferred indexes drain on a headless context: after the test's waited flush, read `context->deferred_resource_freeing_list.transient_binding_indexes` for every frame in flight. Record where they go, and stop and report if they never drain before the context is destroyed.
- [x] Break it: release the indexes even when the flag is false. The new check must fail.
- [x] Build `zest-tests` and `zest-c-compile-check` on Windows in Debug, and every Mac target. Run `--platform all` and `--platform metal --shader-validation` on the Mac. Expected: the Vulkan lines at the baseline, and `Metal suite: 124 / 124` in both runs.
- [x] Phase report, with the shared edit listed for Peter's Windows run. Restore the Mac clone.

## [Phase_2] Ordering

All three tasks add backend tests. A test that finds a real ordering bug gets the fix in `zest_metal.h`, in the task that found it. Stop and report only if the fix needs a Settled decision changed.

### Task 3: Crossing queues

**Files:** `zest-metal-tests.cpp`.

**Interfaces:**
- A gate for the tests:
  ```cpp
  struct MetalGate {
  	id<MTLSharedEvent> event;			//The held queue waits for value 1
  	pthread_t thread;
  	zest_bool thread_started;
  	zest_microsecs delay;				//How long the thread waits before it opens the gate
  	id<MTLSharedEvent> watched_event;	//Read just before the gate opens, when set
  	zest_u64 watched_value;
  	zest_bool watched_reached;
  };
  ```
- `static zest_bool metal_close_gate(zest_device device, MetalGate *gate, id<MTLCommandQueue> queue);` commits a command buffer on the queue that waits for the event to reach 1. Everything committed to that queue after it waits too.
- `static zest_bool metal_open_gate_after(MetalGate *gate, zest_microsecs delay);` starts the thread. It sleeps, reads the watched event if one is set, then sets the event to 1.
- `static void metal_finish_gate(MetalGate *gate);` opens the gate if it is still closed, joins the thread and releases the event. Every exit path of a test that closed a gate calls it.

**Steps:**
- [x] Test **"Metal Cross Queue Order"**, mirroring `test_d3d12_cross_queue_barriers`. It uses a headless context and a cached command graph, since only a cached graph's records outlive the flush.
  - Wave 1 has a compute pass that writes an imported storage image with `image_write2.comp`, and a transfer pass that copies a staging buffer into an imported buffer. Wave 2 is a render pass on the graphics queue that reads both and writes an imported target.
  - Each pass task records `[command_buffer commandQueue]` from its command list backend.
  - The context's compute slot prefers device queue 0 (`preferred_queue_index`, `has_preferred_queue`), and a gate holds that queue. The thread opens it after 200 ms, and first reads the transfer batch's wave event against the value it signals.
  - The graph is flushed with `zest_FlushFrameGraphAndWait`.
  - **Checks:**
    - The first wave holds a compute and a transfer batch.
    - The three passes ran on three different `MTLCommandQueue`s, from families 1, 2 and 0.
    - The transfer batch had finished while the compute batch was held.
    - The flush returned only after the gate opened, and took at least 200 ms.
    - The image's records cross queues on both sides, and its layout after the graph is the render pass's read layout.
    - The buffer reads back the staging words.
    - 0 validation errors.
- [x] Run it alone, in the backend list, and with `--shader-validation`.
- [x] Break it: leave the wave waits out of `zest__mtl_begin_command_buffer` (`zest_metal.h:4424-4427`). The gate check must fail. Leaving out the signals is not tried, as the waiting queue would hang.

### Task 4: A slot that moves to another device queue

**Files:** `zest-metal-tests.cpp`.

**Steps:**
- [x] Test **"Metal Queue Switch Order"**, a headless version of D3D12's `run_cross_queue_ordering`:
  - The graphics slot prefers device queue 0, and a gate holds that queue.
  - Graph A clears an imported image in a render pass and is flushed with `zest_FlushFrameGraph` and no timeline, so the flush doesn't wait. Its resources are imported, so nothing in it is freed early. The flush raises the headless flush report once (`zest.h:17158-17161`), which the test expects.
  - The slot then prefers device queue 1. Graph B clears a second imported image and is flushed with `zest_FlushFrameGraphAndWait`. The thread opens the gate after 200 ms.
  - **Checks:**
    - A's pass ran on family 0's queue 0 and B's on queue 1.
    - B's flush returned only after the gate opened.
    - The slot's ordering timeline reached its value.
    - Both images hold their clear colour.
    - 0 validation errors.
- [x] Break it: leave the switched queue's wait out of `zest__mtl_begin_command_buffer` (`zest_metal.h:4434-4438`). The test must fail. The same wait in the empty-frame present (`:2036-2039`) needs a window and a gate. Record it as a check that can't fail yet.

### Task 5: Encoders in one batch

**Files:** `zest-metal-tests.cpp`.

**Steps:**
- [x] Test **"Metal Frame Graph Fence Order"**: the frame graph version of "Metal Untracked Pass Ordering".
  - One command graph repeats a chain 32 times. A compute pass writes a seed into device-local buffer B through the table. A transfer pass copies B to device-local buffer C with `zest_cmd_CopyBuffer`. A render pass reads C through the table in its fragment shader and writes the seed to word i of host-visible buffer R, also through the table, drawing into an 8x8 target.
  - Every pass depends on the one before it through a resource, including each compute pass on the previous render pass. So every wave has one queue type, and the graph is one batch on the graphics queue (`zest.h:16256-16268`). Check it with `metal_graph_batch_count`, and check that no pass was culled: `zest_GetFrameGraphFinalPassCount` is 96.
  - The shaders are GLSL, built with `zest_CreateShader`.
  - Run the graph 8 times with `zest_FlushFrameGraphAndWait` and different seeds. Every word of R holds its seed, and there are 0 validation errors.
- [x] Break it six times, once per call, and run the test 10 times for each: the wait and the update of the pass compute encoder, the pass blit encoder and the render encoder (`zest_metal.h:4490`, `:4499`, `:4465`, `:4472`, `:4593`, `:4448`). Expected: each break fails at least 9 of 10 runs. Record the rate for each. A call that never fails goes under the checks that can't fail yet, with the chain it was tried on.
- [x] Build and run `--platform metal`, `--shader-validation`, and `--platform all`. Expected: the Vulkan lines at the baseline, `Metal backend: 49 / 49` with the screen unlocked.
- [x] Phase report. Restore the Mac clone.

## [Phase_3] Multiview

### Task 6: Multiview probe

**Files:** `spikes/metal/probe_multiview.mm` and a build script beside it, both untracked, as Spike A's were.

**Question:** do SPIRV-Cross's multiview shaders draw each view into its layer on Metal, with per-instance data and base instances right?

**Steps:**
- [x] Write four GLSL shaders and convert them with `glslc -DZEST_METAL=1` and the `spirv-cross` CLI, using the backend's options plus `--msl-multiview`:
  - a vertex shader with a per-instance position and colour that passes `gl_InstanceIndex` and `gl_ViewIndex` on
  - a fragment shader that writes them as colour
  - a vertex shader that never reads `gl_ViewIndex`
  - a vertex-only shader whose depth is `0.25 + 0.5 * gl_ViewIndex`

  The probe compiles the MSL with `newLibraryWithSource` and runs with `MTL_DEBUG_LAYER=1`, then again with `MTL_SHADER_VALIDATION=1` added.
- [x] **Question 1, instances.** Two views into a two layer 8x8 target, 2 instances from base instance 1. The per-instance attributes come from a buffer of 3 elements with a step rate of 2, and `[[buffer(24)]]` holds 0 and 2.
  - **Pass:** each layer shows elements 1 and 2 at their positions and colours, and `gl_InstanceIndex` reads 1 and 2.
  - **Fail:** stop and report to Peter. There is no fallback without him.
- [x] **Question 2, `inputPrimitiveTopology`.** Build that pipeline with and without it set to the triangle class. Record whether creation fails, whether the debug layer reports, and whether layer 1 draws. Task 7 sets it on multiview pipelines if any of those goes wrong without it.
- [x] **Question 3, a single view pipeline with an unwritten layer.** Convert the first vertex shader without multiview, and draw into a single layer target with and without `inputPrimitiveTopology`. This is what `depthpass.vert` in a single view pipeline does today. If it fails without it, Task 7 sets it on every pipeline from the template's topology, and the class joins the pipeline key.
- [x] **Question 4, depth.** Two views into a two layer `MTLPixelFormatDepth32Float` target with no fragment function. **Pass:** layer 0 reads 0.25 and layer 1 reads 0.75.
- [x] Record the verdict, each answer and the logs under As built.

### Task 7: Multiview in the backend

**Files:** `zest_metal.h` sections `Backend_structs`, `Other_setup_structs`, `Initialisation_functions`, `Pipelines`, `Internal_Frame_graph_context_functions`, `Frame_graph_context_functions`, `Shader_compiling`. `zest-metal-tests.cpp`: the two new tests, case 10 of "Metal Pipeline State Translation" (`:3871-3884`), and the helpers that call the changed functions.

**Interfaces:**
- `#define ZEST__MTL_VIEW_MASK_BUFFER_INDEX 24	//The first view and the view count, 8 bytes, for a multiview pipeline's stages`
- `device->max_multiview_view_count = 32;` at `zest_metal.h:1696`, with `//Views are instances, so this is what MoltenVK reports on the same GPUs`.
- **Conversion.**
  - `zest__mtl_convert_spirv`, `zest__mtl_shader_function` and `zest__mtl_pipeline_shader_function` each gain `zest_bool multiview` after `expected_type`.
  - With it, the conversion sets `SPVC_COMPILER_OPTION_MSL_MULTIVIEW` and `SPVC_COMPILER_OPTION_MSL_VIEW_MASK_BUFFER_INDEX`. Layered rendering stays at its default, on.
  - The function cache key hashes the flag with the stage.
- **`zest_mtl_pipeline_backend_t`** gains `zest_uint view_count;	//1, or the views a multiview pipeline draws as instances`.
- **`zest_mtl_pipeline_key_t`** gains `zest_uint view_count;`. It also gains `MTLPrimitiveTopologyClass input_topology;` if Task 6 needs `inputPrimitiveTopology`.
- **`zest__mtl_vertex_description`** takes the view count and sets it as the step rate of every per-instance binding.
- **`zest__mtl_pipeline_description`:**
  - It drops the `view_mask` refusal.
  - It refuses a template mask that differs from `rendering_info->view_mask`, with D3D12's message ([Settled_]).
  - It sets `out_backend->view_count` from the mask, or 1.
  - It sets `inputPrimitiveTopology` as Task 6 decides.
- **`zest__mtl_build_pipeline`** asks for both functions with `multiview` set when the template has a mask.
- **The flush.** In the pipeline-change block, a pipeline with a view count above 1 sends `{ 0, view_count }` with `setVertexBytes` and `setFragmentBytes` at index 24.
- **Draws.** `zest__mtl_draw` and `zest__mtl_draw_indexed` pass `instance_count * view_count`.
- **Indirect draws.** `zest__mtl_draw_indirect_commands` logs a validation error and draws nothing when the bound pipeline's view count is above 1: "zest_cmd_DrawIndirect can't draw in a multiview pass on the Metal backend, as each view is an instance and the instance count is in the GPU buffer. Use zest_cmd_Draw or zest_cmd_DrawIndexed." (`zest_cmd_DrawIndexedIndirect` for the indexed one.)
- **`zest__mtl_begin_render_pass`:**
  - It drops the view mask refusal.
  - With a mask it sets `renderTargetArrayLength` to the view count before the encoder is made.
  - It puts 0 back where it clears the descriptor's textures (`zest_metal.h:4581-4587`), with `//The descriptor is shared, so a layered pass leaves nothing behind for the next pass, clear or blit`.

**Steps:**
- [x] Test **"Metal Multiview"**, headless:
  - The device reports 32 views.
  - **Colour:** a full-screen triangle whose vertex shader never reads `gl_ViewIndex`, and a fragment shader that writes red for view 0 and green for view 1, into a two layer 16x16 `r8g8b8a8_unorm` target. Read each layer with `metal_read_texture`.
  - **Instances:** Task 6's instance case through Zest. A per-instance binding at binding 1, 2 instances from first instance 1, with colours from the attribute and from `gl_InstanceIndex`. Both layers hold elements 1 and 2.
  - **Depth only:** a vertex-only pipeline into a two layer `d32_sfloat` target, with depth by view. Layer 0 reads 0.25 and layer 1 reads 0.75.
  - **32 views:** a 4x4 target with 32 layers, red set to the view index times 8. Each layer holds its own value.
  - 0 validation errors.
- [x] Test **"Metal Multiview Rules"**, headless:
  - **Mismatch:** a two view template in a single layer pass, and a single view template in a two layer pass. Each gives no pipeline and exactly one validation error, and the graph still runs.
  - **Cache:** one shader pair in a single view template and in a two view template, drawn in that order and then, after `zest_ResetDevice`, in the other order. Each draws its own layers right.
  - **Shared descriptor:** after a multiview pass, a single layer pass and a pass that calls `zest_cmd_ImageClear` on another image in the same graph. Both are right, with 0 validation errors.
  - **Indirect:** an indirect draw in a multiview pass gives one validation error and leaves the layers at their clear colour.
- [x] Build and run them. Expected: both fail at the refusals.
- [x] Implement. Case 10 of "Metal Pipeline State Translation" now expects the mismatch message.
- [x] Break it, each once. Each must fail a test:
  - a step rate of 1
  - an instance count that isn't multiplied
  - no `renderTargetArrayLength`
  - `renderTargetArrayLength` not put back
  - a function cache key without the multiview flag
  - `inputPrimitiveTopology` left out, if Task 6 found it needed
  - Then send the 8 bytes to the vertex stage only, and send a first view of 1, and record which tests catch each.
- [x] Build and run `--platform metal`, `--shader-validation`, `--platform all`, and once with `NSZombieEnabled=YES MallocScribble=1`. Expected: the Vulkan lines at the baseline, `Metal suite: 124 / 124`, `Metal backend: 51 / 51` with the screen unlocked, and no zombie report.
- [x] Phase report. Restore the Mac clone.

## [Phase_4] Docs and the final check

### Task 8: Docs, the roadmap and the final check

**Files:** `.claude/skills/pipelines-and-shaders/SKILL.md:260`, `.claude/skills/frame-graph/SKILL.md:328`, `metal_m5_implementation.md`, `metal_platform_layer_implementation.md`.

**Steps:**
- [x] The `zest_SetPipelineViewCount` row of the pipelines skill: Metal draws at most 32 views as instances, and refuses an indirect draw in a multiview pass. The frame-graph skill's `zest_fgs_invalid_render_pass` row needs no change, but check it.
- [x] Sweep `metal_pending_tests[]`. Nothing is expected to come off: it holds "GPU Profiler Timestamps" (M7) and the Slang tests (M6).
- [x] The final check on the Mac:
  - every target with Metal on and with Metal off
  - `--platform all`, `--platform metal --shader-validation`, and one zombie and `MallocScribble` run
  - `zest-clear-screen --metal --validate --frames 240` and `zest-minimal-template --metal --validate --frames 240`
  - Expected: the Vulkan lines at the baseline, both Metal lines full, `validation errors: 0`.
- [x] Build `zest-tests`, `zest-c-compile-check` and `zest-minimal-template` on Windows in Debug. Expected: clean.
- [x] Add the As built part to this file: the counts, Task 6's answers, the fence break rates, every difference from the plan, the checks that can't fail yet and the shared edits.
- [x] In the design document:
  - Add M5's part to [Built_].
  - Mark "Ordering inside a queue" and "Crossing queues" confirmed in [Mapping_], and add a "Multiview" row.
  - In the M5 row of [Roadmap_], indirect draws move to M4's row, and the roadmap's sentence names this file.
- [x] Hand Peter the Windows check and the questions M6's plan needs answered. Restore the Mac clone.

## [Cost_] Per-frame cost

| Path | Vulkan and D3D12 | Metal, single view | Metal, multiview pass |
| --- | --- | --- | --- |
| Per draw | Unchanged | One multiply of the instance count by 1 | The same multiply. The GPU runs the vertex stage once per view |
| Per indirect draw call | Unchanged | One compare of the view count | Refused, with a validation error |
| Per pipeline change in the flush | Unchanged | One branch | Two `setBytes` of 8 bytes |
| Per render pass | Unchanged | One branch | `renderTargetArrayLength` set and put back: two calls |
| Per vertex on the GPU | Unchanged | Unchanged | One modulo, one divide and the layer write in the converted shader |
| An unwaited command graph flush (Decision 1) | A push per transient bindless index onto the context's list, in place of releasing it | The same | The same |
| A waited flush | Unchanged | Unchanged | Unchanged |
| Pipeline build | Unchanged | Unchanged | A second conversion of each shader the first time it is used in a multiview pipeline |

**Memory:** the pipeline backend and the pipeline key gain 4 bytes each, or 8 with the topology class. The function cache holds a second entry for each shader stage used in a multiview pipeline. No new buffer. With Decision 1, an unwaited command graph's transient records live on the context's lists until the frame in flight comes round, rather than being released at the flush. No struct in `zest.h` grows.

The ordering tests add nothing to the backend.

## [Testing_] Testing

**Automated, on the Mac (implementer)**
- After every task: build `zest-tests`, `zest-c-compile-check` and `zest-objc-compile-check` with Metal on, then `zest-tests --platform metal` and `--platform metal --shader-validation`.
- At the end of every phase: `zest-tests --platform all`, and a Metal off build with `zest-tests`. Expected: the Vulkan lines at the baseline.
- At the end of Phases 3 and 4: `NSZombieEnabled=YES MallocScribble=1 zest-tests --platform metal`.
- New backend tests, taking `metal_tests[]` from 46 to 51: Cross Queue Order, Queue Switch Order, Frame Graph Fence Order, Multiview, Multiview Rules.
- The Metal suite stays at 124. With Decision 1 the shader validation run gives `124 / 124`, without it `123 / 124`.

**Peter's tests**
- On Windows after Task 2 (Decision 1): `zest-tests.exe` with the default `--platform all`, and `--gbv`. Expected: the counts of his last run. "Compute Test Immediate Execute No Wait" passes on every backend, with its new check.
- On the Mac, with the screen unlocked, at the end: `zest-tests --platform all`. Expected: `Metal backend: 51 / 51`, which includes "Metal Windowed Frames" with M4's reap change.

**Visual checks for Peter**
- None in M5. Multiview's first visual check is zest-cascading-shadows with `--metal` in M6. Its four cascades should match the Vulkan run.
- An optional check: capture "Metal Multiview" in Xcode with `zest-tests --platform metal --test "Metal Multiview"`. Each draw's instance count is the app's count times the view count, and the attachments are two layer arrays.

## [Built_] As built

### Phase 1: baseline and the flush race (2026-11-24, on top of commit 41ed835, built and run on the Mac over SSH)

**What changed**
- `zest.h`, `zest_FlushFrameGraph`: a `gpu_finished` flag, false once the graph has run and true again only when the timeline wait succeeds. When it is false, the graph's transient bindless indexes are pushed onto `context->deferred_resource_freeing_list.transient_binding_indexes` for the frame in flight, and the graph's list is freed, before the existing release code runs.
- `zest-compute-tests.cpp`, "Compute Test Immediate Execute No Wait": the first two graphs run `no_wait_write_buffer`, which does what `zest_WriteBufferCompute` does and keeps the index the transient buffer was given. The test fails when both graphs got the same index.

**Results**
- Task 1, with the screen unlocked: `Dynamic rendering: 129 / 129`, `Legacy render pass: 129 / 129`, `Metal suite: 124 / 124`, `Metal backend: 46 / 46`, no VUID line. `--shader-validation`: `Metal suite: 123 / 124` ("Compute Test Immediate Execute No Wait") and `46 / 46`. So "Metal Windowed Frames" passes with M4's reap change.
- The new check before the fix: it failed in all three Mac suites, with both graphs given index 0.
- After the fix: `--platform all` gave 129 / 129, 129 / 129, `Metal suite: 124 / 124` and `Metal backend: 46 / 46`, exit 0, no VUID line. `--platform metal --shader-validation` gave 124 / 124 and 46 / 46.
- Where the indexes drain: a probe printed the context's lists around the test's flushes, on Metal and on both Vulkan suites, alone and in the suite. After the two unwaited flushes, frame in flight 0 holds 2 indexes. After the waited flush it holds none: `zest__do_context_scheduled_tasks` released them, as the headless path runs it once the wait succeeds (`zest.h:17145-17160`).
- Break: with the move skipped, the new check fails, and under shader validation the "resident:No" stores come back.
- Builds: every Mac target with Metal on (22) and off (21), no warning from Zest's files. The Metal-off suites gave 129 / 129 twice. `zest-tests` and `zest-c-compile-check` build on Windows in Debug with no warning. The Windows suites were not run.

**Differences from the plan**
- **Only the bindless indexes move.** The plan moved the transient images and view arrays too, and neither can:
  - A command graph's `transient_images` list is never filled. Nothing in `zest.h` pushes to it, so it is always empty.
  - A transient view array, its views and their backends are allocated from the frame graph's linear allocator (`zest.h:19205-19207`, `zest_vulkan.h:4983-4996`, `zest__mtl_create_image_views_per_mip`). The flush resets that allocator as it ends (`zest.h:17211`), so a deferred view array would point into memory the next graph reuses.
  - So an unwaited flush still destroys a graph's per-mip views straight away, which the GPU may still be reading. That is the same race for another resource, on every backend.
  - **Peter's call (2026-11-24): document it for now.** The API comment above `zest_FlushFrameGraph` and its row in the frame-graph skill say a command graph that uses `zest_GetTransientSampledMipBindlessIndexes` must be flushed with `zest_FlushFrameGraphAndWait` or signal a timeline. He wants it fixed later, so that no backend needs anything of the user that another doesn't. The fix would start at the allocator `zest_GetTransientSampledMipBindlessIndexes` passes to `create_image_views_per_mip` (`zest.h:19207`) for a command graph.

### Phase 2: ordering (2026-11-24, on top of commit 41ed835 with Phase 1 uncommitted, built and run on the Mac over SSH)

**What changed.** Only `zest-metal-tests.cpp`, and no backend code: no test found an ordering bug.
- Gate helpers: `MetalGate`, `metal_gate_thread`, `metal_close_gate`, `metal_open_gate_after` and `metal_finish_gate`, as the plan's Interfaces give them. Also `metal_queue_family`, `metal_task_queue` (the `MTLCommandQueue` a pass's batch goes to) and `metal_crossing_record`.
- "Metal Cross Queue Order", "Metal Queue Switch Order" and "Metal Frame Graph Fence Order", at the end of `metal_tests[]`.

**Results**
- `--platform all`: `Dynamic rendering: 129 / 129`, `Legacy render pass: 129 / 129`, `Metal suite: 124 / 124`, `Metal backend: 49 / 49`, no VUID line. `--platform metal --shader-validation`: 124 / 124 and 49 / 49. The screen was unlocked.
- Every Mac target builds with Metal on (22) and off (21), with no warning from Zest's files, and the Metal-off suites gave 129 / 129 twice. `zest-tests` and `zest-c-compile-check` build on Windows in Debug with no warning.
- "Metal Frame Graph Fence Order" takes about half a second.

**Breaks.** Each call was taken out on the Mac clone and put back. The fence breaks ran only the fence test, through a temporary name filter in the clone's `RunMetalSuite`, 10 times each and 30 for the two weakest.

| Call taken out | Fails | How |
| --- | --- | --- |
| The wave waits (`zest_metal.h:4426`) | "Metal Cross Queue Order", and "Metal Transient Images" | The flush returned after 1 to 4 ms with the gate still closed |
| The switched queue's wait (`zest_metal.h:4434-4438`) | "Metal Queue Switch Order" | "The second clear finished while the gate still held the first." |
| The pass compute encoder's wait (`:4490`) | Fence Order, 10 of 10 runs | 248 of 256 chains wrong |
| Its update (`:4499`) | 30 of 30 | 2 of 256 |
| The pass blit encoder's wait (`:4465`) | 10 of 10 | 216 of 256 |
| Its update (`:4472`) | 30 of 30 | 6 to 8 of 256 |
| The render encoder's wait (`:4593`) | 10 of 10 | 255 of 256 |
| Its update (`:4448`) | 10 of 10 | 245 of 256 |

**What Phase 2 learnt**
- **Metal runs other queues while one is held.** A command buffer on one queue completed while another queue's head waited on a shared event, for every pair of render, compute, blit and empty work, with and without the debug layer (`spikes/metal/probe_gate_queues.mm`).
- **The reap hides a missing GPU wait between a slot's batches from the CPU.** Every batch of a slot signals the slot's ordering event. When a later batch runs first, its value marks the earlier batch finished, and the reap then blocks in `waitUntilCompleted` until that batch really ends (`zest_metal.h:2732-2738`). That is safe, but a flush's timing then looks right with the wait missing. So "Metal Queue Switch Order" checks that the second batch's own signal isn't reached while the gate is closed.
- **A slot changes device queue only when the queue it used last is busy.** `zest__acquire_context_queue` reports a switch by comparing with the preferred index it was given (`zest.h:11204-11216`), so setting `preferred_queue_index` asks for nothing new. The test holds queue 0 with `zest__acquire_manager_queue`. M2's "Metal Windowed Frames" sets `preferred_queue_index` each frame (`zest-metal-tests.cpp:3100-3106`) and so never switches; its "queue switch" phase doesn't reach the ordering wait. Left alone.
- **Graphics passes with the same outputs merge into one render pass** (`zest.h:15944-15956`), while compute and transfer passes get unique keys. So each iteration of the fence test draws to a transient target of its own, which the next compute pass reads.
- **A command graph's builder allocator doesn't grow.** It is set up without user data (`zest.h:15497-15501`), and `zest__linear_allocate` grows only an allocator that has some (`zest.h:10611`). The first fence graph, 96 passes, segfaulted while adding its 25th iteration, after 72 passes, with the default 256 KB, where an in-frame graph would grow. The test asks for 2 MB through `frame_graph_allocator_size`. This affects every backend, and goes to Peter.
- **What makes a fence call testable.** The first fence design (no spins, and two result words read from the copy) caught the compute wait, the blit update and the render wait, but not the compute update or the render update (0 of 10), and the blit wait in 4 of 10. Spinning the kernel before it stores, and having the render pass read the written buffer after a spin, so the next compute pass can overwrite it underneath, made all six fail. A runtime mask of 0 keeps the compiler from moving either access ahead of its spin.

**Differences from the plan**
- **Task 3:**
  - The crossing records are read after `zest_EndFrameGraph` and before the flush, with no cache key. Metal's records are made at compile time, in the frame in flight's allocator, which the flush resets.
  - The records are picked by the queue their pass runs on. The imported image's first use already crosses from the graphics family to the compute queue, with the layout general.
- **Task 4:**
  - The test holds queue 0 rather than preferring queue 1, for the reason above.
  - It checks the second clear's signal rather than the flush's timing.
  - It expects the headless flush report once.
- **Task 5:**
  - Each iteration is compute, then copy, then render, and the render pass draws to a transient target of its own.
  - The results are three words per iteration: the first and last copied word, then the first written word read after a spin.
  - The buffers are 65,536 words, and the context's `frame_graph_allocator_size` is 2 MB.
- **Running the tests:** the backend list has no `--test` filter, so the new tests were judged in full runs.
- **Seen once, in shared code:** one full Metal run aborted on `ZEST_ASSERT(allocation)` in `zest__allocate` (`zest.h:10549`) during "Resource Test Concurrent Pool Growth". It didn't repeat in 30 runs of that test alone on Metal, 30 on Vulkan, or any later full run. Likely, though not proven, another thread takes the pool the grower has just added before the grower allocates from it, and `zest__allocate` asserts after one retry (`zest.h:10540-10549`). Nothing M5 changed is on that path.

**Checks that can't fail yet**
- The switched queue's wait in the empty-frame present (`zest_metal.h:2036-2039`), which needs a window and a gate.
- The immediate encoders' fence calls, and M3's clears and blits, keep the tests they had ([Decisions_], Calls).

### Phase 3: multiview (2026-11-24, on top of commit 41ed835 with Phases 1 and 2 uncommitted, built and run on the Mac over SSH)

**Task 6, the probe. Verdict: pass.** `spikes/metal/probe_multiview/` (untracked): four GLSL shaders, `build_probe_multiview.sh` and `probe_multiview.mm`. The MSL comes from the `spirv-cross` CLI with the backend's options. The CLI has no flag for the view mask index, and its default is already 24. The logs are in `build/`.
- **Question 1, instances: pass.** Two views into a two layer 8x8 target, 2 instances from base instance 1 with a step rate of 2. Both layers show elements 1 and 2 at their positions and colours, `gl_InstanceIndex` reads 1 and 2, and the blue byte holds the layer.
- **Question 2, `inputPrimitiveTopology`: needed.** Without it the pipeline isn't created: "Vertex shader writes render_target_array_index but inputPrimitiveTopology is not specified."
- **Question 3, a single view conversion of a shader that reads `gl_ViewIndex`.** Without the topology, creation fails the same way. With it, the draw is right, but the debug layer reports "the renderPipelineState has a vertex shader that writes to render_target_array_index but renderTargetArrayLength is set to zero".
  - The output struct carries a `gl_Layer [[render_target_array_index]]` that nothing writes. With SPIRV-Cross's layered rendering option off, that member is gone, and the pipeline builds and draws with no topology and no message.
  - So a single view pipeline built from `depthpass.vert` fails on Metal before this phase.
- **Question 4, depth: pass.** Layer 0 reads 0.25 and layer 1 reads 0.75 in every pixel, with no fragment function.
- A vertex shader that never reads `gl_ViewIndex` draws into both layers.
- Every case gave the same result under shader validation.

**Task 7, what changed**
- `zest_metal.h`:
  - `ZEST__MTL_VIEW_MASK_BUFFER_INDEX` (24), and `max_multiview_view_count` is 32.
  - `zest__mtl_convert_spirv`, `zest__mtl_shader_function` and `zest__mtl_pipeline_shader_function` take `multiview` after the stage. A multiview conversion sets `SPVC_COMPILER_OPTION_MSL_MULTIVIEW` and the view mask index. A single view conversion turns `SPVC_COMPILER_OPTION_MSL_MULTIVIEW_LAYERED_RENDERING` off. The function cache key hashes the flag with the stage.
  - `zest__mtl_vertex_description` takes the view count as every per-instance binding's step rate.
  - `zest__mtl_pipeline_description` refuses a template mask that differs from the pass's, with D3D12's message, and gives the backend its view count. A multiview pipeline sets `inputPrimitiveTopology` from the template's topology (`zest__mtl_primitive_topology_class`, new), and the key holds both the view count and the topology class.
  - The flush sends `{ 0, view_count }` to both stages at index 24 when a multiview pipeline is bound. `zest__mtl_draw` and `zest__mtl_draw_indexed` multiply the instance count. An indirect draw with a multiview pipeline is refused with a validation error.
  - `begin_render_pass` sets `renderTargetArrayLength` for a layered pass and puts 0 back once the encoder exists.
- `zest-metal-tests.cpp`:
  - "Metal Multiview" and "Metal Multiview Rules".
  - Case 10 of "Metal Pipeline State Translation" expects the mismatch message.
  - The seven test calls of `zest__mtl_shader_function` pass `ZEST_FALSE`.

**Results**
- Before the backend change, both new tests failed: "Metal Multiview" at the refusals, and "Metal Multiview Rules" with every draw wrong, the single view ones included.
- After it, with the screen locked throughout, so "Metal Windowed Frames" fails in every Metal run:

| Run | Dynamic rendering | Legacy render pass | Metal suite | Metal backend |
| --- | --- | --- | --- | --- |
| `--platform all` | 129 / 129 | 129 / 129 | 124 / 124 | 50 / 51 |
| `--shader-validation` | | | 124 / 124 | 50 / 51 |
| `NSZombieEnabled=YES MallocScribble=1` | | | 124 / 124 | 50 / 51, no zombie report |
| Metal off | 129 / 129 | 129 / 129 | | |

- No VUID line. Every Mac target builds with Metal on (22) and off (21), with no warning from Zest's files. Phase 3 changed nothing that Windows compiles.

**Breaks.** Each change was made on the Mac clone and only the two multiview tests were run, through the temporary name filter.

| Change | Fails | How |
| --- | --- | --- |
| Step rate 1 | Both | Element 0's data in element 2's column |
| Instance count not multiplied | Both | Layer 1 empty |
| No `renderTargetArrayLength` | Both | Every view drawn into layer 0, and a debug layer message |
| `renderTargetArrayLength` not put back | Rules | "slice + renderTargetArrayLength is 2, but the texture only has 1 slices" on the next single layer pass |
| Function cache key without the flag | Rules | The second template got the first's function: `gl_InstanceIndex` doubled, or the build refused |
| No `inputPrimitiveTopology` | Both | The multiview pipelines aren't created |
| Layered rendering left on for single view | Rules | The single view template isn't created |
| The 8 bytes sent to the vertex stage only | Both | Debug layer draw errors |
| A first view of 1 | Both | Views land one layer off, and the depths are wrong |
| No mismatch check | Rules | Both mismatched templates build |
| No indirect refusal | Rules | No validation error |

In the first batch, the "not put back" break showed the symptoms of the break before it, which looks like a stale build. Run alone it gave the line above.

**Differences from the plan**
- **Single view conversions turn layered rendering off.** The plan's fallback for question 3 was `inputPrimitiveTopology` on every pipeline, with the class in the key. The probe found a fix that changes nothing for single view pipelines, and it also fixes single view pipelines built from shaders that read `gl_ViewIndex`. Only multiview pipelines set the topology.
- **The 32 view case sets the template's mask itself.** `zest_SetPipelineViewCount` computes `(1u << view_count) - 1` (`zest.h:12953`), which for 32 shifts by the type's width. That is undefined, and it gives a mask of 0 on x86 and ARM64. So no pipeline can ask for 32 views through the API on any backend, though the pass gives 32 layers `0xFFFFFFFF` (`zest.h:17013`). This goes to Peter.
- **Headless graphs have no swapchain**, so `zest_cmd_SetScreenSizedViewport` asserts (`zest.h:23217`). The tests size the viewport from the open pass's attachments.
- **`zest_cmd_ImageClear` clears to zero**, not to the resource's clear colour. The rules test fills that target with a colour first.
- **A refused build marks its template invalid for good** (`zest.h:13167`, `zest_GetPipeline`), so the mismatch cases have templates of their own.

### Phase 4: docs and the final check (2026-11-24, on top of commit 41ed835 with Phases 1 to 3 uncommitted, built and run on the Mac over SSH)

**What changed**
- `.claude/skills/pipelines-and-shaders/SKILL.md`, the `zest_SetPipelineViewCount` row: D3D12 and Metal refuse a mismatch, GLSL reads `gl_ViewIndex`, Metal draws at most 32 views as instances with instance indexes below 2^32 / view count, and refuses an indirect draw in a multiview pass. The frame-graph skill's `zest_fgs_invalid_render_pass` row is still right. Both skills are git-ignored.
- `metal_pending_tests[]`: nothing came off. It holds "GPU Profiler Timestamps" (M7) and the 7 Slang tests (M6).
- `metal_platform_layer_implementation.md`:
  - M5's part in [Built_], and M4's part names its commit.
  - "Ordering inside a queue" and "Crossing queues" are confirmed in [Mapping_], and a "Multiview" row follows "Indirect draws".
  - Indirect draws moved from M5's [Roadmap_] row to M4's, and the roadmap's sentence names this file.

**The final check, with the screen unlocked**

| Run | Dynamic rendering | Legacy render pass | Metal suite | Metal backend |
| --- | --- | --- | --- | --- |
| `--platform all` | 129 / 129 | 129 / 129 | 124 / 124 | 51 / 51 |
| `--platform metal --shader-validation` | | | 124 / 124 | 51 / 51 |
| `NSZombieEnabled=YES MallocScribble=1` | | | 124 / 124 | 51 / 51, no zombie report |
| Metal off | 129 / 129 | 129 / 129 | | |

- Every run exits 0, with no VUID line.
- `zest-clear-screen --metal --validate --frames 240` and `zest-minimal-template --metal --validate --frames 240` exit 0 with `validation errors: 0`, and the template's log shows its pipeline built.
- Every Mac target builds with Metal on (22) and off (21), with no warning from Zest's files.
- On Windows in Debug, `zest-tests`, `zest-c-compile-check` and `zest-minimal-template` build with no warning. The first two were already up to date with the last `zest.h` edit. The Windows suites were not run.
- Against the baseline: `Metal backend` went from 46 to 51, and the shader validation run's `Metal suite` from 123 / 124 to 124 / 124.

**The shared edits, for Peter's Windows run**
- `zest.h`: `zest_FlushFrameGraph` defers an unwaited graph's transient bindless indexes, and the API comment above its prototype gives the per-mip views limit.
- `zest-compute-tests.cpp`: the check in "Compute Test Immediate Execute No Wait".
- Expected: `zest-tests.exe` and `--gbv` give the counts of his last run, and that test passes on every backend.

**Checks that can't fail yet**
- The switched queue's wait in the empty-frame present (`zest_metal.h:2036-2039`), which needs a window and a gate.
- The immediate encoders' fence calls, and M3's clears and blits, keep the tests they had.
- "Metal Windowed Frames" sets `preferred_queue_index` each frame, so its queue switch phase never switches and never reaches the ordering wait.

**Found in shared code, for Peter**
1. `zest_SetPipelineViewCount(template, 32)` gives a view mask of 0, as `(1u << 32) - 1` is undefined (`zest.h:12953`). The render pass side already special-cases 32 (`zest.h:17013`).
2. A command graph's builder allocator doesn't grow (`zest.h:15497-15501`, `:10611`), so a large command graph crashes.
3. An unwaited command graph's per-mip views are destroyed at the flush. Documented in Phase 1, to be fixed so no backend needs anything of the user that another doesn't.
4. "Resource Test Concurrent Pool Growth" aborted once in `zest__allocate` (`zest.h:10549`), and not again in 30 runs of that test alone on Metal, 30 on Vulkan, or any later full run.
5. Smaller: `zest_cmd_SetScreenSizedViewport` asserts in a headless graph, `zest_cmd_ImageClear` clears to zero rather than to the clear colour, and a refused pipeline build marks its template invalid for good.

**After the sanity check**
- "Metal Cross Queue Order" opens its gate itself when the thread doesn't start, as the ended graph still has to be flushed.
- The 32 view case of "Metal Multiview" stopped calling `zest_SetPipelineViewCount` until its fix below.
- The layers skill's two indirect draw rows give Metal's multiview refusal.
- `--platform metal`, with and without shader validation: `Metal suite: 124 / 124`, `Metal backend: 51 / 51`, with the screen unlocked.
- Left for Peter: an unwaited command graph's deferred records, the bindless indexes and the arena returns, drain when the context's frame in flight next comes round. `zest_BeginFrame` waits only on the frame's own last submission there, so a graph flushed between `zest_EndFrame` and `zest_BeginFrame`, or whose batches run on the compute or transfer slot, may still be running (`zest.h:15220`, `zest_vulkan.h:1770`, `:6123`).

**For M6's plan (settled 2026-11-24: Peter took the recommendation on all four)**
1. **zest-cascading-shadows is Phase 3's visual check.** It is the first example that draws with multiview, 4 views, and its `depthpass.vert` reads `gl_ViewIndex` and per-instance attributes. Its four cascades should match the Vulkan run.
2. **Slang on Metal goes through SPIR-V** and the same SPIRV-Cross conversion as GLSL. The table layout, push constant index, vertex buffer indices, local size and vertex inputs all come from that conversion, and Slang's Metal target would need MSL as a shader input (M7). M6's first probe covers:
   - whether SPIRV-Cross takes Slang's SPIR-V, its descriptor arrays and debug info included
   - "Slang TimelineFX Strides" under MSL's packing of three-float vectors
   - whether Metal reports `scalar_block_layout`, which a Slang session can ask for
   - the Mac's library: Vulkan SDK 1.4.335.1 ships `slangc` and `libslang-compiler.0.2025.23.2.dylib`, a different name from Windows'
3. **"Metal Windowed Frames" gets a real queue switch.** Its queue switch phase holds queue 0 with `zest__acquire_manager_queue`, as "Metal Queue Switch Order" does, and a gate makes the empty-frame present's ordering wait (`zest_metal.h:2036-2039`) fail when it is taken out. It needs the screen unlocked.
4. **Unwaited command graph lifetimes get a plan of their own**, outside M6: a context-owned timeline that `zest_BeginFrame` and the headless drain wait on, and per-mip view arrays outside frame-linear memory. It covers the deferral left for Peter above and item 3 of "Found in shared code".

**The small shared fixes (Peter, 2026-11-24, before M6)**
- `zest_SetPipelineViewCount` gives 32 views a mask of `0xFFFFFFFF`, as the render pass code does (`zest.h:12954`). The 32 view case of "Metal Multiview" sets its views through it again.
- `zest__linear_allocate` asserts when an allocator that can't grow runs out, and names `frame_graph_allocator_size` (`zest.h:10624`). A large command graph wrote through NULL before. Command graphs still don't grow, so the fence test keeps its 2 MB.
- Results, with the screen unlocked: `--platform all` gave `129 / 129`, `129 / 129`, `Metal suite: 124 / 124` and `Metal backend: 51 / 51`, with no VUID line, and shader validation `124 / 124` and `51 / 51`. Every Mac target builds with Metal on (22) and off (21) with no warning from Zest's files, and the Metal-off suites gave `129 / 129` twice. `zest-tests` and `zest-c-compile-check` build on Windows in Debug with no warning. The Windows suites were not run.
- Breaks: with the old mask, "Metal Multiview" fails with "0 of 32 layers hold their view's index." With the fence test's 2 MB taken out, the run stops at the new assert with its message, where it segfaulted before.
- Found while adding the assert, by reading, not run: caching a graph re-initialises its builder's allocator (`zest.h:15402`), and `zloc_InitialiseLinearAllocator` clears its user data (`zest.h:7110`). For an in-frame graph that is the frame in flight's own allocator, which then never grows again: `zest_BeginFrame` sets the user data back only when it consolidates a chain (`zest.h:9686-9693`), and an allocator that can't grow never has one. So after the first cached graph, a later graph on that frame in flight that needs more than `frame_graph_allocator_size` stops at the new assert.

**The cached graph allocator fix (Peter, 2026-11-25)**
- `zest__cache_frame_graph` keeps the allocator's user data across the re-initialise (`zest.h:15402`, `:15406`). A command graph's allocator had none and still has none.
- New shared test "Cached Graph Allocator Growth" (`zest-frame-graph-tests.cpp`, registered after "Transient Mip Views Across Allocator Growth"). Its context has 64 KB of builder memory. Frame 0 caches a one-pass graph, and the test checks that it was cached and that the slot's allocator still holds the context. The slot's next frame builds an uncached 64-pass graph, which must grow the allocator. The other slot runs the cached graph. 5 frames.
- Probe on the Mac: the cached graph used 31984 to 34280 bytes and the large graph 140464 to 142760, so both sides have about twice the margin.
- Results, with the Mac screen locked: `--platform all` gave `130 / 130`, `130 / 130`, `Metal suite: 125 / 125` and `Metal backend: 50 / 51`, with no VUID line. The one failure was "Metal Windowed Frames" with a 1012814 microsecond `zest_BeginFrame`, the locked screen's failure. Shader validation gave `125 / 125` and `50 / 51`, the same way. Every Mac target builds with Metal on (22) and off (21) with no warning from Zest's files, and the Metal-off suites gave `130 / 130` twice. `zest-tests` and `zest-c-compile-check` build on Windows in Debug with no warning. The Windows suites were not run.
- Breaks: without the fix, the new test fails on all three suites. With its user data check also taken out, the run stops at the builder's assert (`zest.h:10624`).
- Found with it: `zest__cache_frame_graph` checked for a grown allocator before it checked for a cache key, so a graph that grew printed "Cannot cache the frame graph" even when it never asked to be cached. The message named `frame_graph_allocation_size`, which doesn't exist.

**The cache alert fix (Peter, 2026-11-25)**
- `zest__cache_frame_graph` returns for a graph without a cache key, or with errors, before it checks for growth (`zest.h:15368-15376`). The alert names `frame_graph_allocator_size`.
- Results, with the Mac screen locked: "Cached Graph Allocator Growth" and "Transient Mip Views Across Allocator Growth" pass on all three suites with no cache alert, where the first printed one per suite before. A probe that gave the large graph the cache key printed the alert once per suite, naming `frame_graph_allocator_size`. `--platform all` gave `130 / 130`, `130 / 130`, `Metal suite: 125 / 125` and `Metal backend: 50 / 51`, with no VUID line and no cache alert; "Metal Windowed Frames" failed on the locked screen again, with a 1012998 microsecond `zest_BeginFrame`. Every Mac target builds with Metal on (22) and off (21) with no warning from Zest's files, and the Metal-off suites gave `130 / 130` twice. `zest-tests` and `zest-c-compile-check` build on Windows in Debug with no warning.
- Rerun with the screen unlocked: `--platform metal` gave `Metal suite: 125 / 125` and `Metal backend: 51 / 51` with no cache alert, and shader validation gave the same.

## Since M5 (2026-11-25)

Two shared changes landed after M5 was committed (afad7b4). They settle four items above, and M6's plan starts from here.

- **Unwaited command graph lifetimes (c4ed23d):**
  - The drain waits for the queue slot values each unwaited flush recorded before it releases anything. It runs from `zest_BeginFrame`, a headless waited flush or `zest_DrainContextResources`.
  - Per-mip views of transients come from the device allocator, and are deferred like the bindless indexes.
  - A command graph's flush no longer replaces the frame's `frame_sync_timeline`.
  - A hung graph costs one timeout, then a critical error.
  - Metal implements the new platform member `wait_for_timeline_value` (`zest_metal.h:2412`).
  - This settles four items above: Out of scope's per-mip deferral, item 3 of "Found in shared code", the drain timing "Left for Peter" under "After the sanity check", and item 4 of "For M6's plan".
- **Command graphs take no cache key (8d75c16):** `zest_BeginCommandGraph(context, name)` never caches. A test that reads a command graph's compiled records does so between `zest_EndFrameGraph` and the flush, as "Metal Cross Queue Order" does.
- **Baseline for M6** (Mac, screen unlocked, display awake):
  - `--platform all` gives `132 / 132`, `132 / 132`, `Metal suite: 127 / 127` and `Metal backend: 53 / 53`, with the same Metal lines under shader validation.
  - New since M5: "Unwaited Graph Mip Views", "Command Graphs After End Frame", "Metal Unwaited Graph Drain" and "Metal Unwaited Graph Timeout".
  - "Compute Test Immediate Execute Cached" is now "Compute Test Immediate Execute Repeated".
- **Windowed Metal runs need the display awake as well as the screen unlocked.** Wake it with `caffeinate -u -t 2` and run the suites under `caffeinate -d`. A sleeping display fails "Metal Windowed Frames" exactly as a locked screen does. Item 3 of "For M6's plan" needs this.
- **Still open from "Found in shared code":**
  - Item 2: a command graph's builder allocator can't grow, so the fence test sets 2 MB.
  - Item 4: the concurrent pool growth abort, seen once.
  - Item 5's three smaller ones.
