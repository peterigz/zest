# Zest Metal backend, M2: swapchain and frames

**Goal:** A Zest app clears a window to a colour on Metal, resizes and shuts down clean, with Metal's validation silent and allocated size steady. The Metal backend joins the shared test suite (`RunSuite`) with every test that needs only a device, resources and frame execution. Vulkan and D3D12 behaviour doesn't change.

**Architecture:** A windowed context presents to a `CAMetalLayer`. The swapchain shared code sees has one image, whose texture is re-pointed at each frame's drawable. Every submission batch records into one unretained `MTLCommandBuffer` taken from the device queue the context queue acquires when the batch begins. Its shared-event waits are encoded before its first encoder and its signals after its last, and the drawable's present goes on the batch the compiler marked as finishing the frame. A render pass is one render encoder that waits on the queue's fence when it opens and updates it when it ends. Barriers do no GPU work: they are records that keep the tracked Zest layouts right. Committed buffers are kept until they finish so a failure or a shader validation log is read.

**Tech stack:**
- Touched: `zest_metal.h`, `zest.h` (one device helper, one reorder of three assignments, one dead branch removed), `examples/Common/example_platform.h`, `examples/SDL2/zest-clear-screen/zest-clear-screen.cpp`, `examples/SDL2/zest-tests/zest-tests.cpp`, `zest-metal-tests.cpp`.
- Not touched: `zest_vulkan.h`, `zest_dx12.h`, every shader, the CMake files, the submodules.
- No new dependency. SDL2's Metal view functions come with the SDL2 the examples already use.

**Spec:** `metal_platform_layer_implementation.md`, the design this plan belongs to. Its [Constraints_], [Settled_], [Mapping_], [Decisions_] and [Digest_] sections apply here unchanged, and its [Built_] section records M1. The roadmap line for M2 is at `metal_platform_layer_implementation.md:260`. The D3D12 plan this one mirrors is `docs/superpowers/plans/2026-11-12-dx12-m2-swapchain-frame-execution.md` (Windows only, git-ignored).

## Contents

- [Constraints_] Global constraints
- [Settled_] Settled
- [State_] Current state
- [Probe_] Probe results
- [Decisions_] Decisions taken
- [Review_] Review focus
- [Scope_] Out of scope
- [Phase_1] Shared wiring, validation and device reset
- [Phase_2] Frame execution on headless contexts
- [Phase_3] Windows
- [Phase_4] The suite and the example
- [Cost_] Per-frame cost
- [Testing_] Testing
- [Built_] As built

## [Constraints_] Global Constraints

Everything in the design's [Constraints_] holds. What M2 adds or changes:

- Never commit or stage. Peter handles all commits.
- **Where the work really runs.** M1 was built from the Windows machine over `ssh macmini`, not by an agent on the Mac, and M2 is planned the same way:
  - Edit on Windows, where the repo and `qgrep` are.
  - Copy changed files to `~/Documents/GitHub/Zest` on the Mac with `scp`, then fix line endings there with `perl -pi -e "s/\r\n/\n/" <file>`.
  - Non-interactive SSH needs `source ~/.zprofile; source ~/.zshrc` first.
  - Build folders on the Mac: `../Zest-Build-Metal` (Metal on) and `../Zest-Build` (Metal off). Both are single-config, so configure with `-DCMAKE_BUILD_TYPE=Debug`.
  - **At the end of every phase the Mac clone is restored to a clean tree** (`git checkout -- <files>`, delete new files), so Peter's pull never conflicts. `spikes/` stays untracked.
  - A script that contains a backslash is written to a file with the Write tool, never passed as a heredoc.
- **Windows builds after every shared edit.** After any change to `zest.h`, `example_platform.h`, `zest-tests.cpp` or `zest-clear-screen.cpp`, build `zest-tests`, `zest-c-compile-check` and `zest-clear-screen` on Windows in Debug. Expected: no warning, no error. The Windows suites are Peter's to run.
- **Shared edits stay behaviour-neutral for Vulkan and D3D12.** Each phase's report lists them.
- **Runs.** The implementer runs `zest-tests` and `--frames N` example runs on the Mac. They open windows on the Mac's logged-in session. Peter does every visual check. Never overlap two test runs.
- **Budget.** Vulkan and D3D12 get no new per-frame work. On Metal, once the first frames have sized the lists, nothing per batch or per pass allocates from Zest's allocators or takes a lock. See [Cost_].
- **Every new test is broken on purpose once** and must fail, as in M1. A check that can't fail yet is named in the report with the milestone that makes it real.
- Follow code-style. A comment never names a milestone, a plan or a phase, which rules out the "reason and milestone" wording the roadmap gave the pending test list. The list in code gives the missing capability only.

## [Settled_] Settled

The implementer doesn't reopen these.

- **The swapchain has one image.** `image_count` is 1 and `current_image_frame` stays 0. Acquire points `images[0]` and `views[0]` at the drawable's texture.
  - Why: a `CAMetalLayer` hands out a drawable at a time and has no fixed set of back buffers to enumerate. Shared code reads swapchain images in four places and all four hold with one (`metal_platform_layer_implementation.md:867`).
  - A cached graph keeps the view struct, which never changes. Only the texture behind it does.
- **The layer lives in the context backend, not the swapchain backend.** `zest__recreate_swapchain` frees the swapchain struct and its backend on every resize (`zest.h:12444-12470`), and the layer has to survive that. `destroy_context_surface` releases it. D3D12 keeps its `IDXGISwapChain` the same way.
- **SDL makes the layer.** With SDL2 compiled in, `create_window_surface` calls `SDL_Metal_CreateView` on the context's window, as Vulkan's calls `SDL_Vulkan_CreateSurface` (`zest_vulkan.h:5194-5202`). Without a window library, `window_data.native_handle` is the `CAMetalLayer`, which is what `zest.h:3962-3969` already documents for macOS.
  - Why: the probe showed it works on the window that serves Vulkan, follows resizes by itself, and survives being created and destroyed repeatedly on one window. Rejected: attaching our own layer to the window's content view, which would mean handling the backing scale and the drawable size ourselves.
- **A batch's command buffer is taken when the batch begins, from the device queue acquired then.** A Metal command buffer belongs to its queue from creation, unlike a Vulkan command buffer or a D3D12 list. `zest__acquire_context_queue` returns at once when the context queue already holds a queue (`zest.h:11199-11200`), so the later call in submit is harmless.
- **Waits are encoded at begin, signals at submit.** Metal orders a wait or signal by where it sits among the buffer's encoders, and shared code only reaches submit after every pass is recorded (`zest.h:17694-17697`). This is what Decision 1 serves.
- **The present goes on the batch that carries the render-finished reference.** The compiler pushes `zest_dynamic_resource_render_finished_semaphore` onto the graphics batch of the last wave that writes the swapchain (`zest.h:16596-16609`). That marks the frame's last write to the drawable, so `presentDrawable:` is encoded on that batch before its commit. `present_frame` runs after the commit (`zest.h:9755`) and only does bookkeeping.
  - Rejected: a present-only command buffer committed from `present_frame`. One more command buffer per frame, and it would have to wait on the frame's shared event to be ordered behind work on another queue.
- **Barriers are records.** The add hooks push the node (the printer indexes the node lists) and a record holding the new layout. Acquire and release write the tracked layouts and send nothing. Ordering inside a queue is the fence chain, and across queues it's the shared events.
- **One Metal device per process, kept across a reset.** `MTLCreateSystemDefaultDevice` returns the same object every time (M1, Task 4), so `reinit_logical_device` recreates the queues and the residency set and keeps the device, its feature flags and its validation state.

## [State_] Current state (verified 2026-11-23, Windows tree at 0bc5816, clean)

**What M1 left**
- `zest_metal.h` is 2,666 lines. 88 members are still `ZEST__MTL_UNAVAILABLE` stubs, all in `Unavailable_stubs` (`zest_metal.h:2552-2660`). M2 replaces 38 of them.
- A windowed context is refused: `zest__mtl_initialise_context_backend` reports for anything but a headless context (`zest_metal.h:1346-1349`).
- `zest_mtl_context_backend_t` holds only `void *layer` (`zest_metal.h:107-109`), and `zest_mtl_context_queue_backend_t` only `command_buffer` (`zest_metal.h:111-113`).
- Each device queue has an `MTLCommandQueue` and an `MTLFence` (`zest_metal.h:99-105`). Immediate commands already wait on and update that fence (`zest_metal.h:1605-1623`).
- `zest__mtl_reset_queue_command_pool` does nothing (`zest_metal.h:1631`). `zest__mtl_wait_for_idle_device` commits an empty buffer on every queue and waits for it (`zest_metal.h:1953`).
- `zest__mtl_finish_command_buffer` reports a failed buffer and takes in its shader validation logs (`zest_metal.h:1548-1562`). Only the immediate path calls it.
- Validation is set up inside device creation: `zest__mtl_prepare_validation` sets the environment (`zest_metal.h:1029-1043`) and `zest__mtl_confirm_validation` makes one deliberate mistake to check it came on (`zest_metal.h:1046-1094`). When it didn't, the device carries on without validation and prints an alert (`zest_metal.h:1081-1086`).
- `zest__mtl_initialise_device` creates the residency set and the queues inline (`zest_metal.h:1239-1285`). `zest__mtl_cleanup_queues` undoes the queues (`zest_metal.h:1441-1462`).
- Images keep their layout in the shared `image->layout`. `zest_mtl_image_backend_t` and `zest_mtl_image_view_backend_t` hold a texture each (`zest_metal.h:133-140`).

**The shared frame path**
- `zest__initialise_context` creates the surface and the swapchain before `initialise_context_backend` and before any context queue exists (`zest.h:11670-11683`).
- `zest__create_swapchain` calls `new_swapchain_backend` then `initialise_swapchain` (`zest.h:11828-11844`). `zest__cleanup_swapchain` calls `cleanup_swapchain_backend` then `destroy_context_surface`, then frees `images` and `views` (`zest.h:11846-11853`).
- `zest_BeginFrame` waits on the frame's timeline, then calls `acquire_swapchain_image`. A false return recreates the swapchain and fails the frame (`zest.h:9703-9707`).
- `zest_EndFrame` executes the graph, then calls `present_frame` when the graph is flagged to present. A false return recreates the swapchain and flushes cached graphs (`zest.h:9753-9760`). A frame that submitted nothing calls `dummy_submit_for_present_only`, then `present_frame` (`zest.h:9765-9775`).
- `zest__execute_frame_graph`, per batch: `reset_queue_command_pool` once per device frame (`zest.h:17498-17502`), `set_next_command_buffer` and `begin_command_buffer` (`zest.h:17506-17524`), `bind_descriptor_sets` for the bindless set on every graphics and compute batch (`zest.h:17528-17537`), then per pass `acquire_barrier`, `begin_render_pass`, the callbacks, `end_render_pass`, `release_barrier` (`zest.h:17566-17685`), then `end_command_buffer` and `submit_frame_graph_batch` (`zest.h:17694-17697`). `carry_over_semaphores` runs after every wave (`zest.h:17713`).
- **`command_list.submission_index` and `queue_index` are set inside the pass loop** (`zest.h:17549-17551`), and `command_list.frame_graph` inside the callback loop (`zest.h:17629`). So none of the three is set when `set_next_command_buffer` and `begin_command_buffer` run for a graph's first batch. Vulkan and D3D12 read them only at submit and at end (`zest_vulkan.h:6029-6030`, `zest_dx12.h:6872`, `zest_dx12.h:7099-7100`).
- A failed execution calls `end_command_buffer` on an open buffer and never submits it (`zest.h:17770-17778`).
- Shared code calls `end_render_pass` even when `begin_render_pass` returned false (`zest.h:17593`, `zest.h:17679`).
- A render pass's attachments are `zest_rendering_attachment_info_t`: a pointer to the resource's view pointer, the load and store ops, the clear value, and `is_swapchain_image` (`zest.h:4400-4407`). The render area offset is always 0 (`zest.h:16998-16999`, `zest.h:17053-17054`).
- The swapchain's view reaches a pass through `zest__swapchain_resource_provider`, which returns `views[current_image_frame]` on every execution (`zest.h:15410-15412`).
- A graph with no transients returns from `zest__place_transient_resources` before any arena hook (`zest.h:14889-14892`), so M2's graphs never reach M3's stubs.
- `zest_EnableVSync` and `zest_DisableVSync` both set `zest_context_flag_schedule_change_vsync` (`zest.h:13820-13826`). The backends toggle `zest_context_flag_vsync_enabled` in `present_frame` and return false to force a recreate (`zest_dx12.h:2986-3006`).
- `zest_ResetDevice` destroys every context, frees every device resource, then calls `reinit_logical_device` and asserts on a false return (`zest.h:9812-9924`). `RunTests` calls it between every test (`zest-tests.cpp:272`), which is why it's in M2.

**Windows and helpers**
- `zest_implsdl2_CreateWindow` asks for `SDL_WINDOW_VULKAN` and retries without it (`zest.h:23417-23438`). It still holds the dead `#elif defined(ZEST_IMPLEMENT_METAL)` placeholder (`zest.h:23421-23422`).
- `zest_implsdl2_GetWindowSizeCallback` uses `SDL_Vulkan_GetDrawableSize` on a window with the Vulkan flag and `SDL_GetWindowSizeInPixels` otherwise (`zest.h:23479-23504`). On macOS the first returns the Metal layer's drawable size once the window has a Metal view (probe).
- The D3D12 device helpers are declared at `zest.h:5182-5184` and defined at `zest.h:23368-23378` and `zest.h:23409-23415`.
- `zest_metal.h` is included after the SDL2 helpers (`zest.h:23553-23555`), so `SDL_MAJOR_VERSION` is visible to it when the app included SDL first.
- The repo has no GLFW target. Every example is under `examples/SDL2`.

**Tests and examples**
- `zest-tests.cpp`: `vulkan_only_tests[]` and `RegisterTest` filter the D3D12 suite (`zest-tests.cpp:201-225`). `CreateSuiteDevice` picks the builder (`zest-tests.cpp:298-318`). `RunSuite` creates the device and the first context (`zest-tests.cpp:323-356`).
- `main` runs the Metal list in a child process when the Vulkan suites also run (`zest-tests.cpp:428-440`), then the Vulkan suites, then the D3D12 suite in a window of its own and the D3D12 list (`zest-tests.cpp:470-483`).
- `zest-metal-tests.cpp`: `MetalTestFlags` (16), `BeginMetalTestBuilder` (24), `finish_metal_device` (28), `metal_allocated_size` (35), `metal_read_texture` (89), `metal_tests[]` (919), `RunMetalSuite` (937), `RunMetalSuiteInChildProcess` (958). The child reports `passed total` through a pipe.
- `zest-clear-screen` already exists and goes through `example_platform.h`, which knows `--vulkan` and `--d3d12` (`example_platform.h:17-55`). No example reads `options.use_d3d12` itself.
- The Mac's display is 2560 x 1440 at a scale of 1.0. Nothing on it exercises a backing scale of 2.

**Wiring to mirror, from D3D12**
| What | Where D3D12 does it |
| --- | --- |
| Device reset | `zest__dx_reinit_logical_device`, `zest_dx12.h:2567-2600` |
| Surface, swapchain, images and views | `zest_dx12.h:2769-2935` |
| Acquire, present, dummy submit | `zest_dx12.h:2940-3090` |
| Context and frame graph backends | `zest_dx12.h:3165-3245` |
| Swapchain and semaphore cleanup | `zest_dx12.h:3390-3445` |
| Semaphores per graph name | `zest__dx_get_frame_graph_semaphores`, `zest_dx12.h:6670-6694` |
| Barrier records and the node pushes | `zest_dx12.h:6696-6749` |
| Command list lifecycle | `zest_dx12.h:6813-6885` |
| Render passes | `zest_dx12.h:7024-7090` |
| Submission and carried waits | `zest_dx12.h:7092-7211` |
| Viewport, scissor, clip | `zest_dx12.h:7349-7374` |
| Windowed test helpers | `zest-d3d12-tests.cpp:3326-3394`, "D3D12 Windowed Frames" at 3757 |
| Headless frame tests | `zest-d3d12-tests.cpp:3167` and 3228 |

`zest_dx12.h` is the structural model only. It's agent-written, so it isn't a style model.

## [Probe_] Probe results (2026-11-23, Apple M2, SDL 2.32.10)

`spikes/metal/build/probe/probe_drawable.mm` on the Mac, untracked. One file, SDL2 and Metal, no Zest. These are facts the tasks rely on.

**Validation and windows**
- **`SDL_CreateWindow` creates the process's first Metal device.** With `MTL_DEBUG_LAYER=1` set before `SDL_Init` or between `SDL_Init` and `SDL_CreateWindow`, Metal prints "Metal API Validation Enabled" during window creation and both deliberate errors are reported. Set after the window, nothing is reported and nothing warns. A hidden window and a window without the Vulkan flag behave the same.
- So M1's rule, "before the process's first Metal device", means **before the first window** for any windowed app. Device creation is too late, and every example creates its window first.

**Drawables**
- `nextDrawable` is where a frame waits for the display: 16.7 ms per frame with display sync on, 2.6 ms with it off. `displaySyncEnabled` switches at runtime with no other change.
- A frame built the way the backend will build it runs clean under the debug layer: `commandBufferWithUnretainedReferences`, a render encoder using the queue's fence, `presentDrawable:` on the last buffer, the drawable released straight after the commit. So does a frame with two command buffers that both write the drawable and present on the second.
- **A drawable that is held and never presented stalls the layer.** With two held, the third `nextDrawable` took 1,009 ms. A drawable has to be presented or released in the frame that took it.
- After `SDL_SetWindowSize` the very next drawable has the new size, with no event pump in between. The layer's `drawableSize` and `SDL_Vulkan_GetDrawableSize` agree.
- A minimised window and a hidden window both keep handing out drawables, at about 7 ms each. No nil drawable was seen in any phase.
- The layer accepted all seven formats tried: BGRA8Unorm, BGRA8Unorm_sRGB, RGBA16Float, RGB10A2Unorm, BGR10A2Unorm, RGBA8Unorm, RGBA8Unorm_sRGB.
- `SDL_Metal_CreateView` and `SDL_Metal_DestroyView` ran four times on one window, each view presenting frames.
- `maximumDrawableCount` is 3, and `allowsNextDrawableTimeout` is on by default.

**Not probed, confirmed by the task named**
- A depth attachment whose format also has stencil, with no stencil attachment set *(unverified, Task 5)*.
- Whether Metal accepts a zero-area scissor rect *(unverified, Task 5)*.
- A backing scale of 2 *(not testable on this Mac, see [Testing_])*.

## [Decisions_] Decisions taken (Peter, 2026-11-23)

All five were taken as recommended. The implementer does not reopen them.

1. **Move three assignments earlier in `zest__execute_frame_graph`.** Metal has to encode a batch's waits when the batch begins, and needs the wave index, the queue slot and the graph for that. They are set later today (`zest.h:17549-17551`, `zest.h:17629`).
   - Taken: set `command_list.submission_index`, `command_list.queue_index` and `command_list.frame_graph` just before the `switch` at `zest.h:17507`, and leave `timeline_wait_stage` where it is. All three are constant for the batch, and Vulkan and D3D12 only read them later, so nothing changes for them. Four lines of shared code.
   - Rejected: no shared edit. Metal commits a second command buffer in front of each waiting batch that holds only the waits. One more command buffer per waiting batch, and it depends on a queue never starting a buffer while an earlier one is still blocked, which nothing has measured.
2. **GLFW windows on Metal.**
   - Taken: not in M2. A GLFW app passes its own `CAMetalLayer` in `window_data.native_handle`, which the fallback path takes. The repo has no GLFW target, so a GLFW helper couldn't be run on the Mac. The limit goes in M7's docs.
   - Rejected: add a Cocoa branch to `zest_implglfw_CreateWindow` and a `zest_implglfw_CreateMetalDevice` now, about 20 lines, untested.
3. **A vsync change doesn't recreate the swapchain on Metal.**
   - Taken: `present_frame` sets `layer.displaySyncEnabled` and returns true. No pipelines or cached graphs are flushed. `zest_SwapchainWasRecreated` then stays false after `zest_EnableVSync` on Metal, where it turns true on Vulkan and D3D12.
   - Rejected: return false as the others do, for identical behaviour, at the cost of a full recreate that Metal doesn't need.
4. **A public call that turns validation on before the window exists.**
   - Taken: `ZEST_API void zest_PrepareMetalValidation(zest_device_init_flags flags);` in `zest_metal.h`. It sets the environment and starts the reader, exactly what device creation does today, and device creation calls it too. `example_platform.h` calls it from `ParseExampleOptions` for `--metal --validate`, and `zest-tests` from the top of `main`. An app that doesn't call it gets today's alert, reworded to name the call.
   - Rejected: no API. Document that Metal validation needs `MTL_DEBUG_LAYER=1` set by whatever launches the app. `--validate` would then do nothing on Metal for every example.
5. **A frame with no work clears the drawable.**
   - Taken: `dummy_submit_for_present_only` clears the drawable to the swapchain's clear colour in an empty render pass and presents it. A new drawable's contents are undefined, and the other backends present whatever the back buffer held.
   - Rejected: present it uncleared, which may flash stale pixels. Rejected: not presenting at all, which leaves the loop spinning with nothing to wait on.

6. **One Metal view per SDL window (Peter, 2026-11-23, during Phase 3).** macOS keeps a `CAMetalLayer` for the rest of the process once `displaySyncEnabled` has been set to NO on it, with its drawables, even after its view is destroyed (As built, Phase 3). Vsync off is Zest's default, so a view per context leaves a window's worth of drawables behind for every context destroyed, and `zest_ResetDevice` destroys the context between every suite test.
   - Taken: `create_window_surface` makes the SDL Metal view once per window, keeps it on the window with `SDL_SetWindowData` under `ZEST__MTL_SDL_VIEW_NAME`, and every later context on that window reuses it. `destroy_context_surface` leaves it in place. This replaces "`destroy_context_surface` destroys the SDL view" in [Settled_] and in Task 6, and the context backend has no `sdl_metal_view`.
   - Rejected: a view per context as first planned, with the leak written up as a macOS limit.

**Calls made without Peter (seen by Peter on 2026-11-23 with the plan, none changed)**
- **`--test <text>` on the test runner.** `RegisterTest` skips any test whose name doesn't contain the text. Task 7 tries each pending suite test on its own, and a test that crashes then costs one process, not the run. About 10 lines in `zest-tests.cpp`, on every platform.
- **In the Metal process the suite runs before the backend list.** The suite's window then exists before any Metal device, which is the order a real app has, so a missing `zest_PrepareMetalValidation` fails the run.
- **A command buffer that fails is reported but its signals aren't forced.** A later wait then times out through `fence_wait_timeout_ns`, and teardown after that can hang. Forcing the signals belongs with M7's device-loss work.
- **The pending list in code names what a test needs** ("pipelines", "transient images"), never a milestone. The As built section maps the groups to milestones.
- **No interop getters.** The tests reach backend structs directly, as the M1 tests do. A `zest_GetMetalLayer` or `zest_GetMetalCommandBuffer` waits for an integration that needs one.
- **An unsupported swapchain format falls back to BGRA8Unorm with one log line**, as D3D12 falls back to B8G8R8A8_UNORM.

## [Review_] Review Focus

1. **Validation is silently off because a window came first.** Proven by the probe, and it makes every "no validation errors" line worthless. Probe: the suite's device must have `api_validation` set or the suite reports 0 passed. Break it by removing the `zest_PrepareMetalValidation` call from `main`: the run must fail.
2. **A drawable is held, leaked or released twice.** Manual retain and release across acquire, submit and present, on unretained command buffers. A held drawable costs a one-second stall, and an over-release is a crash that may come frames later. Probes: every `zest_BeginFrame` in "Metal Windowed Frames" returns inside 500 ms, including the frames after a missing `zest_EndFrame`. "Metal Windowed Context" compares allocated size across context lifecycles. The windowed tests run once under zombies and `MallocScribble`.
3. **The present is encoded on the wrong batch, twice, or not at all.** It shows only as flicker or a frozen window. Probe: "Metal Windowed Frames" checks after every frame that `present_count` is exactly 1 and that the context no longer holds a drawable. Peter's visual check covers what a count can't.
4. **A signal is never encoded, or a wait is encoded after an encoder.** A missing signal hangs the next wait until its timeout. Probe: "Metal Render Pass Clear" uses `zest_FlushFrameGraphAndWait`, which fails on a timeout. Breaking the signal must fail it. **Waits between waves can't fail yet:** no graph with two waves on different queues passes before M4 and M5, so the report says so and M5's plan takes the check.
5. **The drawable and the swapchain disagree on size.** A pass sized for the old extent on a new drawable is a Metal validation error, the same class MoltenVK raises in "User Test Swapchain Resize". Probe: the resize phases of "Metal Windowed Frames" and the suite's "User Test Swapchain Resize" end with 0 validation errors.
6. **The shared reorder breaks Windows.** The Mac can't see it. Probe: Peter's Windows run after Phase 1.

## [Scope_] Out of scope

- Transient images and arenas, `image_clear`, blits and the remaining immediate functions. M3.
- Pipelines, draws, dispatch, push constants, binding the bindless table to an encoder, the debug overlay. M4. `bind_descriptor_sets` only checks that the bindless set is what's bound.
- A frame-graph test that fails without the fence chain. It needs a pass that reads what another wrote, which needs a draw. M4's plan takes it. M1's "Metal Untracked Pass Ordering" covers the fences at the Metal level until then.
- Graphs that cross queues, multiview, and the device-reset suite tests. M5.
- MSAA resolve. A resolve attachment is reported and the pass doesn't begin.
- GLFW windows (Decision 2), iOS, HDR and colour space settings on the layer.
- Reading the swapchain back. The layer stays `framebufferOnly`.
- Forcing a failed command buffer's signals, device loss, debug markers, GPU timestamps. M7.
- User docs and the `metal-backend` skill. M7.

## [Phase_1] Shared wiring, validation and device reset

Every `zest.h` edit M2 needs for the backend is in this phase, so one Windows run covers them.

### Task 1: Baseline and the shared edits

**Files:** `zest.h:17506-17551`, `zest.h:17629`, `zest.h:5182-5184`, `zest.h:23409-23423`.

**Interfaces:**
- `zest__execute_frame_graph` sets `command_list.submission_index`, `command_list.queue_index` and `command_list.frame_graph` before the `switch (batch->queue_type)`. The three old assignments go. `timeline_wait_stage` stays in the pass loop.
- `ZEST_API zest_device zest_implsdl2_CreateMetalDevice(zest_window_data_t *window_data, zest_device_init_flags flags);` declared beside the D3D12 helper with no guard, defined inside `#ifdef ZEST_METAL_IMPLEMENTATION` in the SDL2 section. It ignores `window_data` and returns `zest_EndDeviceBuilder(zest_BeginMetalDeviceBuilder(flags))`. One comment line above the declaration, as the D3D12 one has.
- The dead `#elif defined(ZEST_IMPLEMENT_METAL)` branch and its comment are removed from `zest_implsdl2_CreateWindow`.

**Steps:**
- [x] Capture the Mac baseline at the current commit, Metal on: `zest-tests --platform all`. Expected: `Dynamic rendering: 129 / 129`, `Legacy render pass: 129 / 129`, `Metal backend: 15 / 15`, exit 0. Record the numbers; every later "expected" is relative to them.
- [x] Make the three edits.
- [x] Build on Windows: `zest-tests`, `zest-c-compile-check`, `zest-clear-screen`. Expected: clean.
- [x] Build on the Mac with Metal on and off, and run `zest-tests --platform all` with Metal on. Expected: the baseline lines.

### Task 2: Validation before the window

**Files:** `zest_metal.h` sections `Other_setup_structs`, `Initialisation_functions`. `zest-tests.cpp` `main`. `zest-metal-tests.cpp`.

**Interfaces:**
- `ZEST_API void zest_PrepareMetalValidation(zest_device_init_flags flags);` declared beside `zest_GetMetalDevice` (`zest_metal.h:210`). Three `//` lines above it say: Metal reads its validation settings when the process makes its first Metal device, creating a window makes one, so call this before creating a window when the device will ask for validation.
  - With `zest_device_init_flag_enable_validation_layers` in `flags` it sets `MTL_DEBUG_LAYER`, sets `MTL_DEBUG_LAYER_ERROR_MODE` to `nslog` unless the launcher chose a mode, sets `MTL_SHADER_VALIDATION` for the sync flag, and starts the reader. Without the flag it does nothing. Calling it twice is harmless.
  - `zest__mtl_forced_validation_unavailable` still skips the environment, as it does now.
- `zest__mtl_prepare_validation(device)` keeps reading `launcher_shader_validation` and then calls `zest_PrepareMetalValidation(device->setup_info.flags)`.
- The alert and log line at `zest_metal.h:1082-1083` are reworded: validation can't be turned on because a window or another Metal device already existed, so call `zest_PrepareMetalValidation` before creating the window, or launch with `MTL_DEBUG_LAYER=1`.
- `zest-tests.cpp`: when the process runs Metal and not Vulkan, `main` calls `zest_PrepareMetalValidation(MetalTestFlags())` straight after the arguments are parsed, before any window or device.

**Steps:**
- [x] Implement.
- [x] Add "Metal SDL Helpers" to `metal_tests[]`. It makes a 320x200 window with `zest_implsdl2_CreateWindow`, then a device with `zest_implsdl2_CreateMetalDevice`. It checks: the platform is `zest_platform_metal`, the device backend's `api_validation` is set, the size callback gives a framebuffer of at least 320x200 and a window of 320x200. Then `finish_metal_device` returns 0 and the window is destroyed with `SDL_DestroyWindow`.
- [x] Break it: remove the call from `main` and move "Metal SDL Helpers" to the front of the list. Expected: it fails on `api_validation`. Restore both.
- [x] Build and run `--platform metal` and `--platform metal --shader-validation`. Expected: 16 / 16 in both.

### Task 3: Device reset

**Files:** `zest_metal.h` sections `Initialisation_functions`, `Backend_cleanup_functions`.

**Interfaces:**
- `zest_bool zest__mtl_create_device_objects(zest_device device);` creates the residency set and calls `zest__mtl_create_queues`. `zest__mtl_initialise_device` calls it in place of its inline code.
- `zest_bool zest__mtl_reinit_logical_device(zest_device device);` waits for the device to idle, collects validation messages, calls `zest__mtl_cleanup_queues`, releases the residency set and calls `zest__mtl_create_device_objects`. It keeps the `MTLDevice`, `support` and the validation fields. A failure returns false with a log line and leaves a device that `zest_DestroyDevice` can still finish.
- Device teardown doesn't change: shared code frees the queues (`zest.h:11923`) and `zest__mtl_cleanup_device_backend` releases the set.

**Steps:**
- [x] Implement, and register the hook.
- [x] Test "Metal Device Reset": create a device, run `run_metal_image_round_trip`, call `zest_ResetDevice`, expect 0 validation errors and a residency set that holds only what a fresh device holds. Do the round trip and the reset three times, comparing `metal_allocated_size()` after the third reset with the size after the second.
- [x] Break it twice: skip releasing the old residency set (the size check must fail), and skip recreating the queues (the round trip must fail or report).
- [x] Build and run `--platform metal`, then with `--shader-validation`. Expected: 17 / 17.
- [x] Phase report, with the shared edits listed for Peter's Windows run. Restore the Mac clone.

## [Phase_2] Frame execution on headless contexts

Nothing here needs a window. Both tasks are proven on headless contexts with `zest_BeginCommandGraph`.

### Task 4: Command buffers, shared events and submission

**Files:** `zest_metal.h` sections `Backend_structs`, `Backend_setup_functions`, `Backend_cleanup_functions`, `Command_pools`, `Internal_Frame_graph_context_functions`, `Debug_functions`.

**Interfaces:**
- `zest_mtl_context_queue_backend_t` gains `id<MTLCommandBuffer> *committed[ZEST_MAX_FIF];`, a `zest_vec` per frame in flight of the batches committed in it, each retained until it has finished.
- `zest_mtl_command_list_backend_t`, frame-linear, from `new_frame_graph_context_backend`:
  - `id<MTLCommandBuffer> command_buffer;`
  - `id<MTLRenderCommandEncoder> render_encoder;`, open between a pass's begin and end
  - `zest_context_queue context_queue;`
  - `id<MTLFence> fence;`, the acquired device queue's
  - `zest_uint target_width;` and `zest_uint target_height;`, the open pass's attachment size
  - `zest_bool switched_queue;`
  - `zest_mtl_execution_backend_t *execution;`, set by `carry_over_semaphores`
- `zest_mtl_execution_backend_t`, frame-linear, from `new_execution_backend`: `id<MTLSharedEvent> wave_wait_events[ZEST_QUEUE_COUNT]; zest_u64 wave_wait_values[ZEST_QUEUE_COUNT]; zest_uint wave_wait_count;`
- `zest_mtl_frame_graph_semaphores_backend_t`: `id<MTLSharedEvent> events[ZEST_MAX_FIF][ZEST_QUEUE_COUNT];`, allocated from the context's allocator by `new_frame_graph_semaphores_backend`. `get_frame_graph_semaphores` caches one per graph name in `context->cached_frame_graph_semaphores`, as D3D12 does. `cleanup_frame_graph_semaphore` releases the events and frees the backend.
- `zest_mtl_submission_batch_backend_t`: `id<MTLSharedEvent> signal_event;`, frame-linear. `get_final_signal_ptr` returns it and `get_final_wait_ptr` returns NULL, without reporting.
- **`set_next_command_buffer(command_list, queue)`:**
  - Reaps the queue's finished buffers (below).
  - Releases a buffer a failed batch left uncommitted.
  - Acquires the device queue with `zest__acquire_context_queue(context, queue, command_list->queue_index)`, keeps the result in `switched_queue` and the queue's fence in `fence`.
  - Takes `[native_queue commandBufferWithUnretainedReferences]` inside an autorelease pool and retains it. A nil buffer returns false with a report.
  - Increments `queue->next_buffer`, as the other backends count buffers begun since the reset.
- **`begin_command_buffer(command_list)`** encodes the batch's waits, in this order, each with `encodeWaitForEvent:value:`:
  - when `submission_index` is above 0, every event and value in `execution`
  - when `submission_index` is 0 and the graph has a `wait_on_timeline` with a value above 0, that timeline
  - when `switched_queue` is set and the slot's `queue_ordering_timelines` value is above 0, that timeline
- **`end_command_buffer(command_list)`** ends an encoder a failed pass left open. It doesn't commit.
- **`submit_frame_graph_batch`** keeps every obligation of `zest__dx_submit_frame_graph_batch` (`zest_dx12.h:7092-7197`) that has a Metal form:
  - inserts the context queue into `*queues`
  - on the last wave, when the graph has a `signal_timeline`: adds 1 to its value, encodes the signal, and sets `context->frame_sync_timeline[current_fif]`
  - on any other wave: adds 1 to `semaphores->values[fif][slot]` and encodes the signal on that slot's event
  - always adds 1 to the slot's ordering timeline and encodes its signal
  - encodes the present when one of the batch's `signal_semaphores` has the type `zest_dynamic_resource_render_finished_semaphore` (Task 6, a no-op until then)
  - commits, pushes the buffer onto `committed[current_fif]` and clears `command_buffer`
  - sets `zest_context_flag_work_was_submitted`
  - A missing command buffer returns false. Nothing after the commit can fail, so there's no rollback path.
- **`carry_over_semaphores`** fills `execution` with the event and value of every slot that had a batch in the wave, and stores the pointer in the command list backend.
- **`void zest__mtl_reap_command_buffers(zest_context context, zest_context_queue queue, zest_bool release_all);`** walks the queue's `committed` lists. A buffer that has finished goes through `zest__mtl_finish_command_buffer` and is released. One still running stays, unless `release_all` is set, when it is released unread.
  - `reset_queue_command_pool` calls it, passing its `release_resources` through, and frees the vecs when that is set.
  - `zest__mtl_collect_validation_messages` calls it for every context queue of every context on the device, so a validation count taken after a wait includes the logs of the buffers that just finished.
  - `cleanup_context_queue_backend` reaps with `release_all` and frees the vecs, as cached graph queues never get the reset call (`zest.h:12310-12317`).
- `new_execution_barriers_backend`, `add_frame_graph_image_barrier`, `add_frame_graph_buffer_barrier`, `acquire_barrier` and `release_barrier` are Task 5's. Until then the first three exist with the node pushes and the last two do nothing, so a graph with no passes compiles.
- `bind_descriptor_sets` records nothing. A set other than `device->bindless_set` is a validation error naming custom sets, as on D3D12 (`zest_dx12.h:7216-7224`).
- `get_resource_ptr` returns the resource's texture or pool buffer.

**Steps:**
- [x] Implement, replacing the stubs and their registrations.
- [x] Test "Metal Empty Command Graph", mirroring `zest-d3d12-tests.cpp:3167`: on a headless context, `zest_BeginCommandGraph`, `zest_EndFrameGraph` with no passes. The result has `zest_fgs_no_work_to_do` and `zest_FlushFrameGraph` returns. Destroy the context and the device with 0 validation errors.
- [x] Break it: skip releasing the semaphores' events in `cleanup_frame_graph_semaphore`. "Metal Headless Context" compares allocated size across contexts, so give that test one empty command graph per context and confirm it now fails. This is the check M1 recorded as unable to fail.
- [x] Build and run `--platform metal`, then with `--shader-validation`. Expected: 18 / 18.

### Task 5: Barrier records, render passes, viewports and scissors

**Files:** `zest_metal.h` sections `Backend_structs`, `Type_converters`, `Backend_setup_functions`, `Internal_Frame_graph_context_functions`, `Frame_graph_context_functions`.

**Interfaces:**
- `zest_mtl_frame_graph_image_barrier_t`: `zest_resource_node resource; zest_image_layout new_layout; zest_bool crosses_queues;`
- `zest_mtl_execution_barriers_backend_t`: `zest_mtl_frame_graph_image_barrier_t *acquire_images; zest_mtl_frame_graph_image_barrier_t *release_images;`, both frame-linear vecs.
- `add_frame_graph_image_barrier` pushes a record and the node. `crosses_queues` is true when both families are real and differ. `add_frame_graph_buffer_barrier` pushes the node only.
- `acquire_barrier` writes each record's layout to `resource->view->image->layout`, and to `*resource->linked_layout` and `resource->image_layout` when the node has a linked layout. `release_barrier` does the same but skips a record that crosses queues, as Vulkan leaves that layout to the matching acquire (`zest_vulkan.h:5572-5580`).
- Pure helpers in `Type_converters`:
  - `MTLLoadAction zest__mtl_load_action(zest_load_op load_op);` clear, load and don't care map to Clear, Load and DontCare.
  - `MTLStoreAction zest__mtl_store_action(zest_store_op store_op);` store and don't care map to Store and DontCare.
  - `MTLScissorRect zest__mtl_clamped_scissor(int x, int y, zest_uint width, zest_uint height, zest_uint target_width, zest_uint target_height);` returns the part of the rect inside the target. Metal rejects a scissor that leaves the target, and Vulkan and D3D12 accept one.
- `zest_mtl_context_backend_t` gains `MTLRenderPassDescriptor *render_pass_descriptor;`, created in `initialise_context_backend` for every context and released in `cleanup_context_backend`. A pass refills it, so no pass allocates one.
- **`begin_render_pass`:**
  - A resolve attachment or a non-zero `view_mask` is reported through `zest__mtl_report_unavailable` and returns false.
  - Fills the descriptor: each colour attachment's texture from the view backend, its load and store actions, and its clear colour, which is the swapchain's `clear_color` read now for a swapchain image and `resource->clear_color` otherwise. The depth attachment's texture, actions and clear depth. Attachments past the pass's count are cleared to nil. The stencil attachment is left unset, as Vulkan's dynamic rendering attaches only depth.
  - Opens the encoder inside an autorelease pool and retains it, then `waitForFence:beforeStages:MTLRenderStageVertex` on the batch's fence.
  - Records the target size from the first attachment's texture.
- **`end_render_pass`** does nothing without an open encoder. Otherwise `updateFence:afterStages:MTLRenderStageFragment`, `endEncoding`, release.
- `viewport`, `clip`, `scissor` and `set_screensized_viewport` act on the open render encoder and do nothing without one. A viewport is passed straight through, because the converted shaders flip Y themselves (Spike A). Every scissor goes through `zest__mtl_clamped_scissor`. The screen-sized one takes the swapchain's size, as D3D12's does (`zest_dx12.h:7349-7357`).

**Steps:**
- [x] Implement.
- [x] Pure test "Metal Render Pass Actions": the five op mappings, and `zest__mtl_clamped_scissor` for a rect inside the target, one hanging off the right and bottom edges, one with a negative origin, and one wholly outside.
- [x] Test "Metal Render Pass Clear", mirroring `zest-d3d12-tests.cpp:3228`: a 16x16 `r8g8b8a8_unorm` colour attachment with `transfer_src`. Three times on one headless context: a command graph imports it, sets a clear colour (0.2, 0.4, 0.6 on the first and third, 0.6, 0.4, 0.2 on the second), clears it in a pass with `zest_EmptyRenderPass`, and `zest_FlushFrameGraphAndWait` returns success. Read the image back: every pixel is (51, 102, 153, 255) or (153, 102, 51, 255), within 1 per channel. After the third, the context queue's `committed` lists hold no more than the buffers still in flight.
- [x] Add a depth phase: the same pass with a 16x16 depth attachment in the device's depth format, cleared to 0.25. Read it back if the format has no stencil, otherwise only expect 0 validation errors. Record whether a depth and stencil format is accepted with no stencil attachment.
- [x] Add a scissor phase: a pass whose callback sets a scissor larger than the target, one wholly outside it, and a screen-sized viewport. Expected: 0 validation errors. Record whether Metal accepts the zero-area rect the outside case gives. If it doesn't, the hook keeps the last valid rect and the finding goes to M4's plan, since only a draw can show the difference.
- [x] Break each once: swap the two clear colour sources, drop the frame signal from submit (the flush must time out), skip the clamp (the scissor phase must report), never reap (the `committed` check must fail).
- [x] Build and run `--platform metal`, then with `--shader-validation`, then once with zombies and `MallocScribble`. Expected: 20 / 20 and no zombie report.
- [x] Phase report. Restore the Mac clone.

## [Phase_3] Windows

### Task 6: Surface, swapchain, acquire and present

**Files:** `zest_metal.h` sections `Backend_structs`, `Type_converters`, `Swapchain_setup`, `Swapchain_presenting`, `Backend_setup_functions`, `Backend_cleanup_functions`, `Debug_functions`. `zest-metal-tests.cpp`.

**Interfaces:**
- `zest_mtl_context_backend_t`: `layer` becomes `CAMetalLayer *`, with `#import <QuartzCore/CAMetalLayer.h>` beside the Metal import. It gains:
  - `void *sdl_metal_view;`, the `SDL_MetalView` that owns the layer when SDL made it
  - `id<CAMetalDrawable> drawable;`, this frame's, from acquire until its present is committed
  - `zest_uint present_count;`, the presents encoded for this frame's drawable, which must end at one
- `zest_mtl_swapchain_backend_t`: `MTLPixelFormat pixel_format;`, allocated by `new_swapchain_backend`.
- `zest_bool zest__mtl_swapchain_format(zest_format requested, MTLPixelFormat *out_pixel_format, zest_format *out_reported);` in `Type_converters`. The seven formats the probe saw accepted pass through. Anything else gives BGRA8Unorm, reports `zest_format_b8g8r8a8_unorm` and returns false.
- **`create_window_surface`:**
  - With `SDL_MAJOR_VERSION` defined: `SDL_Metal_CreateView` on `zest_Window(context)`, then `SDL_Metal_GetLayer`. `zest_metal.h` includes `<SDL_metal.h>` under that define, as `zest.h:23386-23388` includes `SDL_vulkan.h`.
  - Otherwise: `window_data.native_handle` must be a `CAMetalLayer`, checked with `isKindOfClass:`, and is retained.
  - A missing view or layer returns false with a log line saying which.
- **`initialise_swapchain`:**
  - Sets the layer's `device`, `pixelFormat`, `framebufferOnly` to YES and `displaySyncEnabled` from `zest_context_flag_vsync_enabled`.
  - `swapchain->size` and `context->window_extent` are the layer's `drawableSize`. The window size and `dpi_scale` come from `window_sizes_callback`, as D3D12 takes them (`zest_dx12.h:2798-2803`, `zest_dx12.h:2877`).
  - `image_count` is 1. `images[0]` gets the extent, the reported format, the colour aspect and an image backend with no texture yet. `views[0]` is one allocation holding the view and its backend, as D3D12's are (`zest_dx12.h:2920-2927`).
- **`cleanup_swapchain_backend`** releases a drawable still held, frees the image backend, the view and the swapchain backend, and leaves the layer.
- **`destroy_context_surface`** destroys the SDL view, or releases the layer it retained, and clears both pointers.
- **`initialise_context_backend`** no longer refuses a windowed context.
- **`acquire_swapchain_image`:**
  - Releases a drawable still held from a frame that never presented.
  - Returns false when the layer's `drawableSize` isn't `swapchain->size`.
  - Takes `nextDrawable` inside an autorelease pool and retains it. A nil drawable returns false with a report.
  - Returns false, releasing the drawable, when its texture isn't the swapchain's size.
  - Points the image backend and the view backend at the drawable's texture, with no retain, and zeroes `present_count`.
- **In `submit_frame_graph_batch`:** when the batch carries the render-finished reference and the context holds a drawable, `presentDrawable:` is encoded after the signals and `present_count` goes up by one.
- **`present_frame`:**
  - Releases the drawable and clears the two texture pointers.
  - If `present_count` is 0, reports that the graph asked to present and no batch carried it.
  - A scheduled vsync change toggles `zest_context_flag_vsync_enabled`, clears the schedule flag and sets `displaySyncEnabled` (Decision 3).
  - Returns false when `swapchain->framebuffer_resized` is set or the layer's `drawableSize` isn't `swapchain->size`, clearing `framebuffer_resized`.
- **`dummy_submit_for_present_only`**, following `zest__dx_dummy_submit_for_present_only` (`zest_dx12.h:3010-3090`):
  - Acquires the graphics context queue and takes a command buffer on it.
  - Waits on the slot's ordering timeline if the queue switched.
  - Clears the drawable in a render pass with no draws, using the context's descriptor and the queue's fence (Decision 5).
  - Adds 1 to `frame_timeline[current_fif]`, encodes its signal and stores it in `frame_sync_timeline[current_fif]`. Signals the ordering timeline.
  - Encodes the present, adds one to `present_count`, commits, and hands the buffer to the graphics context queue's `committed` list.
- `get_swapchain_identifier` returns the layer. `get_swapchain_wait_semaphore` and `get_swapchain_signal_semaphore` return NULL without reporting.

**Steps:**
- [x] Implement.
- [x] Pure test "Metal Swapchain Formats": `b8g8r8a8_unorm` and `b8g8r8a8_srgb` pass through and return true, `r16g16b16a16_sfloat` passes through, `r8_unorm` gives BGRA8Unorm, reports `b8g8r8a8_unorm` and returns false.
- [x] Add test helpers to `zest-metal-tests.cpp`, mirroring `zest-d3d12-tests.cpp:3326-3394`: open and close a test window, and run one windowed frame (`zest_UpdateDevice`, `zest_BeginFrame`, a cached graph that imports the swapchain and clears it with `zest_EmptyRenderPass`, `zest_EndFrame`) that returns how long `zest_BeginFrame` took.
- [x] Test "Metal Windowed Frames", on its own 320x200 window, with the clear colour set to (0.1, 0.2, 0.3, 1). After every frame: `present_count` is 1 and the context holds no drawable. After every phase: 0 validation errors. Every `zest_BeginFrame` returns inside 500 ms.
  - **10 frames.**
  - **Resize:** `SDL_SetWindowSize` to 400x300, frames until `zest_SwapchainWasRecreated` (at most 10), the extent equals the callback's framebuffer size. Re-apply the clear colour, 5 frames, back to 320x200 and repeat.
  - **Vsync:** both calls schedule a toggle (`zest.h:13820-13826`). Read the layer's `displaySyncEnabled`, call `zest_EnableVSync`, run 5 frames: the layer's value has flipped, it matches `zest_context_flag_vsync_enabled`, and no frame recreated. Call `zest_DisableVSync`, run 5 frames: it has flipped back.
  - **Minimise:** `SDL_MinimizeWindow`, 5 frames, `SDL_RestoreWindow`, 5 frames.
  - **Hidden:** `SDL_HideWindow`, 5 frames, `SDL_ShowWindow`, 5 frames.
  - **Screen-sized viewport:** 3 frames whose pass callback calls `zest_cmd_SetScreenSizedViewport(command_list, 0.f, 1.f)`. It needs a swapchain, so Task 5's headless test couldn't cover it.
  - **Queue switch:** 6 frames alternating `context->preferred_queue_index[graphics slot]` between 0 and 1 with `has_preferred_queue` set, as the D3D12 test does.
  - **No work:** 3 frames of `zest_BeginFrame` then `zest_EndFrame(context, 0)`.
  - **No end frame:** `zest_BeginFrame` twice with no `zest_EndFrame` between. The second fails with one validation error, which the test resets. `zest_EndFrame(context, 0)` then presents the held drawable, and 3 normal frames follow.
  - **Teardown:** destroy the context straight after the last `zest_EndFrame`, then the device, then the window.
- [x] Test "Metal Windowed Context": on one window and one device, create a windowed context, run 4 frames and destroy it, five times. Allocated size after the fifth equals the size after the fourth, and the same for the third.
- [x] Break each once: never release the drawable in `present_frame` (the 500 ms bound must fail), skip the size check in acquire (the resize phase must report), encode the present twice on the presenting batch (the count check must fail), leak the SDL view (the size check must fail).
- [x] Build and run `--platform metal`, with `--shader-validation`, and with zombies and `MallocScribble`. Expected: 23 / 23 and no zombie report.
- [x] Phase report. Restore the Mac clone.

## [Phase_4] The suite and the example

### Task 7: The Metal run joins `RunSuite`

**Files:** `zest-tests.cpp:201-225`, `298-356`, `358-502`. `zest-metal-tests.cpp:937-1004`.

**Interfaces:**
- `CreateSuiteDevice` gains a Metal branch returning `zest_EndDeviceBuilder(BeginMetalTestBuilder())`.
- `RunSuite` on Metal fails the whole suite, with a red line, when the device's `api_validation` isn't set.
- `static const char *metal_pending_tests[]` in `zest-tests.cpp`, inside `#ifdef ZEST_ENABLE_METAL`, grouped under one comment per missing capability: transient images and buffers, pipelines and draws, compute, more than one queue, shader compiling, and so on. `RegisterTest` skips a listed test on Metal. It also skips `vulkan_only_tests[]` on every platform that isn't Vulkan, where today it names D3D12.
- `--test <text>`: `RegisterTest` skips a test whose name doesn't contain the text. A suite left with no test prints a line and is skipped, as `RunTests` reads the first test unasked.
- **The Metal process** (`--platform metal`, or the child of `--platform all`) runs the suite first, in a 1280x768 window from `zest_implsdl2_CreateWindow` that it destroys afterwards, then the backend list.
- The child writes four numbers, `suite_passed suite_total backend_passed backend_total`, and `RunMetalSuiteInChildProcess` reads four.
- Summary lines: `Metal suite: N / N` and `Metal backend: N / N`. Both count toward the exit code.

**Steps:**
- [x] Implement the runner changes with `metal_pending_tests[]` holding every registered test except this starting set, which is what D3D12 ran at the same point:
  - "Empty Graph", "Single Pass", "Blank Screen", "Pass Culling", "Resource Culling", "Chained Pass Culling"
  - "Import Image", "Pass Grouping", "Cyclic Dependency", "Simple Graph Cache"
  - "Resource Test Image Format Support", "Resource Test Image Creation Destruction", "Resource Test Buffer Creation Destruction", "Resource Test Buffer Edge Cases", "Resource Test Sampler Creation"
  - "User Test No Update Device", "User Test No End Frame", "User Test No Swapchain Import", "User Test Missing End Pass", "User Test Bad Frame Graph Ordering", "User Test Frame Graph State Errors", "User Test Pass Without Task", "User Test Unused Imported Resource", "User Test Multiple Swapchain Imports", "User Test Resources Named the Same", "User Test Add the same resource twice"
  - "Compute Test Timeline Timeout"
  - "Resource Test Buffer Offset Alignment", "Resource Test Dedicated Buffer", "Resource Test Buffer Grow Contract"
  - "User Test Device Platform Query", "User Test Descriptor Pool Failure", "User Test Swapchain Resize"
- [x] Run `--platform metal`. A starting test that fails because it needs a later capability goes onto the pending list with that capability named. A failure in code this plan owns gets fixed.
- [x] Sweep the pending list: run each pending test alone with `--test`, in its own process. One that passes with 0 validation errors comes off the list. Record the final suite count.
- [x] Run `--platform all`. Expected: the two Vulkan lines at the Task 1 baseline, `Metal suite` full at the recorded count, `Metal backend: 23 / 23`, exit 0, no VUID line.
- [x] Run `--platform metal --shader-validation`. Expected: both Metal lines full.
- [x] Build `zest-tests` and `zest-c-compile-check` on Windows. Expected: clean.

### Task 8: The clear-screen example, and the close

**Files:** `examples/Common/example_platform.h`, `examples/SDL2/zest-clear-screen/zest-clear-screen.cpp:1-5`.

**Interfaces:**
- `example_options_t` gains `zest_bool use_metal;`. `--metal` sets it and `backend_named`, and clears `use_d3d12`. `--vulkan` and `--d3d12` clear it. Without `ZEST_ENABLE_METAL` it prints that Metal isn't compiled in and exits, as `--d3d12` does.
- `ParseExampleOptions` calls `zest_PrepareMetalValidation` with the flags `CreateExampleDevice` will add, when `use_metal` and `validate` are both set.
- `CreateExampleDevice` calls `zest_implsdl2_CreateMetalDevice` for `use_metal`. The Vulkan loader message reset stays Vulkan only.
- The header's opening comment gains `--metal`.
- `zest-clear-screen.cpp` defines `ZEST_METAL_IMPLEMENTATION` under `ZEST_ENABLE_METAL`. No other example changes: they keep running on Vulkan on the Mac until their milestone.
- The default backend doesn't change. Metal is by name only until M6.

**Steps:**
- [x] Implement.
- [x] On the Mac, from the repo root: `zest-clear-screen --metal --validate --frames 240` and `zest-clear-screen --vulkan --validate --frames 240`. Expected: both exit 0, the Metal run prints `validation errors: 0`, and the log shows Metal API validation on.
- [x] `zest-clear-screen --metal --validate-sync --frames 240`. Expected: exit 0, `validation errors: 0`, shader validation on in the log.
- [x] Build on Windows: every example in Debug, as they all include `example_platform.h`. Expected: clean.
- [x] The final check on the Mac: all targets with Metal on and with Metal off, `zest-objc-compile-check`, `zest-tests --platform all`, `--platform metal --shader-validation`, and one zombie and `MallocScribble` run of `--platform metal`.
- [x] Add `## As built (<date>)` to this file: the suite count and the final pending groups mapped to milestones, the two unverified answers from Task 5, every difference from the plan, the checks that still can't fail, and the full list of shared edits. Add M2's line to [Built_] in the design document, and correct its Validation row in [Mapping_] to say that creating a window makes the first Metal device.
- [x] Hand Peter the Windows checks and the visual checks in [Testing_], and the questions M3's plan needs answered. Restore the Mac clone.

## [Cost_] Per-frame cost

| Path | Vulkan and D3D12 | Metal after M2 |
| --- | --- | --- |
| Per frame | Three assignments run once per batch where they ran once per pass | One `nextDrawable`, which is where the frame waits for the display. One present. Two size compares. One autorelease pool in acquire |
| Per batch | Unchanged | One unretained command buffer, retained once. Up to four event waits and two signals encoded. One commit. One pointer pushed. One status read per buffer still in flight |
| Per pass | Unchanged | The context's descriptor refilled, one encoder inside one autorelease pool, one fence wait and one update. A barrier is one or three layout stores and no GPU work |
| Per scissor | Unchanged | Four clamps |
| Per draw | Unchanged | Nothing draws until M4 |
| A frame with no work | Unchanged | One command buffer with a clear pass |
| A validation count | Unchanged | Reaps every context queue on the device. Only when the app asks |

**Memory:** six shared events per graph name. Two vecs of pointers per context queue. One render pass descriptor and a few pointers per context. A barrier record is 16 bytes of frame-linear memory, against the full barrier struct on the other backends. No struct in `zest.h` grows and no buffer is added.

## [Testing_] Testing

**Automated, on the Mac (implementer)**
- After every task: build `zest-tests`, `zest-c-compile-check` and `zest-objc-compile-check` with Metal on, then `zest-tests --platform metal` and `--platform metal --shader-validation`.
- At the end of every phase: `zest-tests --platform all`, and a Metal off build with `zest-tests`. Expected: the Vulkan lines at the Task 1 baseline.
- At the end of Phases 2, 3 and 4: `NSZombieEnabled=YES MallocScribble=1 zest-tests --platform metal`. Expected: no zombie report.
- New backend tests, in `metal_tests[]`: SDL Helpers, Device Reset, Empty Command Graph, Render Pass Actions, Render Pass Clear, Swapchain Formats, Windowed Frames, Windowed Context. That takes the list from 15 to 23.
- The Metal suite: the 33 starting tests, plus whatever the sweep in Task 7 adds.

**Peter's tests, on Windows**
- After Phase 1 (the `zest.h` edits) and again after Phase 4 (`zest-tests.cpp`, `example_platform.h`, `zest-clear-screen.cpp`):
  - `zest-tests.exe` with the default `--platform all`. Expected: `Dynamic rendering: 136 / 136`, `Legacy render pass: 136 / 136`, `D3D12 suite: 132 / 132`, `D3D12 backend: 103 / 103`, exit 0.
  - `zest-clear-screen.exe --validate --frames 240` with `--d3d12` and with `--vulkan`. Expected: 0 validation errors.
- After Phase 4 only: `zest-tests.exe --test "Swapchain"`. Expected: each suite line shows only the tests with Swapchain in their name, all passing. The D3D12 backend list isn't filtered.

**Visual and usability checks for Peter, on the Mac, after Phase 4**
From the repo root, `../Zest-Build-Metal/examples/SDL2/zest-clear-screen --metal`:
1. The window shows a dark blue-grey that drifts slowly through its cycle, with no flicker and no frame of another colour.
2. Drag a corner to resize. The contents stretch while dragging and fill the new size on release, the same as the `--vulkan` run. No black bars and no stale region.
3. Minimise to the Dock and restore. The colour is still cycling, and it didn't jump as if frames had stalled.
4. Click the green button for full screen and again to leave it. Clean both ways.
5. Cover the window with another, then uncover it.
6. Close the window. The process exits with no message in the terminal.
7. Run it again with `--validate`, repeat steps 2 to 4, and close. Expected: `validation errors: 0`.
8. If you have a Retina display to hand, run step 1 and 2 there. Nothing on the mini's display exercises a backing scale of 2, so this is the only check of it.

## [Built_] As built (2026-11-23)

One part per phase, in the order they were built. Phase 4's part closes with what M2 leaves for M3.

### Phase 1: shared wiring, validation and device reset (2026-11-23, on top of commit 0bc5816, built and run on the Mac over SSH)

**What changed**
- `zest.h`, three edits:
  - `zest__execute_frame_graph` sets `command_list.submission_index`, `queue_index` and `frame_graph` once per batch, before the command buffer is taken. The two assignments in the pass loop and the one in the callback loop are gone. `timeline_wait_stage` is where it was.
  - `zest_implsdl2_CreateMetalDevice`, declared beside the D3D12 helper and defined inside `#ifdef ZEST_METAL_IMPLEMENTATION`.
  - The dead `ZEST_IMPLEMENT_METAL` branch is removed from `zest_implsdl2_CreateWindow`.
- `zest_metal.h`:
  - `zest_PrepareMetalValidation`, public. `zest__mtl_prepare_validation` calls it, and the alert for validation that didn't come on names it.
  - `zest__mtl_create_device_objects`, which `zest__mtl_initialise_device` now calls for the residency set and the queues.
  - `zest__mtl_reinit_logical_device`, replacing its stub. 87 stubs remain.
- `zest-tests.cpp`: `main` calls `zest_PrepareMetalValidation(MetalTestFlags())` when the process runs Metal and not Vulkan, inside `#ifdef ZEST_ENABLE_METAL`.
- `zest-metal-tests.cpp`: "Metal SDL Helpers" and "Metal Device Reset".

**Results**
- Mac baseline before any edit, Metal on: `Dynamic rendering: 129 / 129`, `Legacy render pass: 129 / 129`, `Metal backend: 15 / 15`, exit 0, no VUID line.
- Mac after Task 1 alone: the same three lines, with Metal on and Metal off builds clean.
- Mac at the end of the phase, Metal on:
  - `zest-tests`, `zest-c-compile-check` and `zest-objc-compile-check` build with no warning from Zest's files.
  - `zest-tests --platform metal`: `Metal backend: 17 / 17`.
  - `zest-tests --platform metal --shader-validation`: `Metal backend: 17 / 17`.
  - `zest-tests --platform all`: `Dynamic rendering: 129 / 129`, `Legacy render pass: 129 / 129`, `Metal backend: 17 / 17`, exit 0, no VUID line.
- Mac, Metal off: every target builds, and `zest-tests` gives 129 / 129 and 129 / 129, exit 0, no VUID line.
- Windows: `zest-tests`, `zest-c-compile-check` and `zest-clear-screen` build in Debug with no warning and no error, after Task 1 and again at the end of the phase. The suites were not run.

**Every new check was broken on purpose and failed** (`spikes/metal/mutate_m2.py` on the Mac, untracked)

| What was broken | Result |
| --- | --- |
| The `zest_PrepareMetalValidation` call removed from `main`, with "Metal SDL Helpers" run first | SDL Helpers, Builder and Validation Reports fail: the whole process has lost validation |
| The old residency set never released at reset | Device Reset fails |
| The old queues never cleaned up at reset | Device Reset fails |
| The queues and residency set not made again | The process crashes inside Device Reset |

**Where it differs from the plan**
- **"Metal Device Reset" counts owners, not bytes.** The plan expected `currentAllocatedSize` to show a residency set or queue that a reset failed to release. Neither holds GPU memory, so the size can't move. The test keeps a reference of its own to the old set and the old first graphics queue across each reset and expects to be their only owner afterwards (`retainCount` of 1). The allocated size is still compared between the second and third reset.
- **"Metal SDL Helpers" doesn't check `native_handle`**, which the D3D12 version does. On macOS the SDL helper only fills it for a Vulkan build, and Metal doesn't use it.
- **`zest__mtl_release_device_objects` wasn't needed.** The reset calls `zest__mtl_cleanup_queues` and releases the set directly, and device teardown is unchanged.

**Shared edits for Peter's Windows run**
- `zest.h`: the three edits above. Only the first is compiled on Windows with a change in what runs: three assignments now happen once per batch instead of two per pass and one per callback. Vulkan and D3D12 read all three later than either place.
- `zest-tests.cpp`: four lines inside `#ifdef ZEST_ENABLE_METAL`.
- Expected on Windows: the four baseline lines in [Testing_], and `zest-clear-screen.exe --validate --frames 240` clean on each backend.

### Phase 2: frame execution on headless contexts (2026-11-23, on top of commit 44a3242, built and run on the Mac over SSH)

**What changed**
- `zest_metal.h`, 26 stubs replaced, 61 left:
  - Structs: `zest_mtl_execution_backend_t`, `zest_mtl_command_list_backend_t`, `zest_mtl_frame_graph_semaphores_backend_t`, `zest_mtl_submission_batch_backend_t`, `zest_mtl_frame_graph_image_barrier_t`, `zest_mtl_execution_barriers_backend_t`. The context queue backend gains `committed[ZEST_MAX_FIF]` and the context backend `render_pass_descriptor`.
  - Command buffers: `set_next_command_buffer`, `begin_command_buffer`, `end_command_buffer`, `submit_frame_graph_batch`, `carry_over_semaphores`, `get_frame_graph_semaphores`, and the four `new_*_backend` members that go with them.
  - `zest__mtl_reap_command_buffers` and `zest__mtl_reap_device_command_buffers`. `reset_queue_command_pool`, `cleanup_context_queue_backend` and `zest__mtl_collect_validation_messages` call them.
  - Barriers: the two add hooks, `zest__mtl_apply_image_barriers`, `acquire_barrier` and `release_barrier`.
  - Render passes: `begin_render_pass`, `end_render_pass`, `viewport`, `scissor`, `clip`, `set_screensized_viewport`, and the pure helpers `zest__mtl_load_action`, `zest__mtl_store_action` and `zest__mtl_clamped_scissor`.
  - `bind_descriptor_sets`, `get_resource_ptr`, `get_final_signal_ptr`, `get_final_wait_ptr`, `cleanup_frame_graph_semaphore`.
- `zest-metal-tests.cpp`: "Metal Empty Command Graph", "Metal Render Pass Actions" and "Metal Render Pass Clear". "Metal Headless Context" now runs an empty command graph on each context.
- No shared file changed, so there is nothing for a Windows run in this phase.

**Results**
- Task 4 alone: `--platform metal` and `--platform metal --shader-validation` both `Metal backend: 18 / 18`.
- End of the phase, Metal on:
  - `zest-tests`, `zest-c-compile-check` and `zest-objc-compile-check` build with 0 warnings.
  - `--platform metal`: 20 / 20. With `--shader-validation`: 20 / 20. With `NSZombieEnabled=YES MallocScribble=1`: 20 / 20 and no zombie report.
  - `--platform all`: `Dynamic rendering: 129 / 129`, `Legacy render pass: 129 / 129`, `Metal backend: 20 / 20`, exit 0, no VUID line.
- Metal off: every target builds, 129 / 129 and 129 / 129, exit 0, no VUID line.

**What the two unverified probe items turned out to be**
- A depth attachment in `d32_sfloat_s8_uint` with no stencil attachment set is accepted with no validation message. "Metal Render Pass Clear" keeps the case.
- A zero-area scissor rect, which is what a rect wholly outside the target clamps to, is accepted with no validation message. Whether it draws nothing, as Vulkan does, can only be shown with a draw.

**Every new check was broken on purpose** (`spikes/metal/mutate_m2.py` on the Mac, untracked)

| What was broken | Result |
| --- | --- |
| Frame graph events never released | Headless Context fails |
| Clear colour sources swapped | The process crashes inside Render Pass Clear, on the headless context's missing swapchain |
| Clear colour never passed on | Render Pass Clear fails |
| Frame signal dropped from submit | Render Pass Clear fails, as the flush times out |
| Ordering signal dropped from submit | Render Pass Clear fails in its queue switch phase |
| Scissor not clamped | Render Pass Clear fails on Metal's validation message |
| Scissor clamp ignores the far edges | Render Pass Actions and Render Pass Clear fail |
| Finished batches never reaped | Render Pass Clear fails |
| Batches reaped only by a validation count | Render Pass Clear fails |
| A validation count doesn't reap | Render Pass Clear fails |
| Clear depth never passed on | Render Pass Clear fails |
| Acquire barrier records no layout | Render Pass Clear fails |
| Clear load action maps to load | Render Pass Actions and Render Pass Clear fail |
| Store action maps to don't care | Render Pass Actions and Render Pass Clear fail |
| Pass descriptor keeps its textures | Render Pass Clear fails |
| No pass ended by `end_render_pass` | Metal aborts the process at the commit, inside Render Pass Clear |

**Checks that can't fail yet**
- A pass that doesn't wait on the queue's fence, or doesn't update it: 20 / 20 either way. Nothing reads what an earlier pass wrote until there is a draw. M4.
- The waits `begin_command_buffer` encodes. A missing wait changes no result while every graph has one wave and one pass. The wave waits need a graph with two waves on different queues (M5), and the ordering wait a pass that reads the slot's previous batch (M4). The ordering signal is covered, by the queue switch phase.
- `release_barrier`. A one pass graph that imports its image has no release record. Phase 3's swapchain graphs have one, to the present layout.
- `set_screensized_viewport`, until Phase 3.

**Where it differs from the plan**
- **"Metal Headless Context" counts owners of a frame graph event.** The plan expected the allocated size to show leaked events. A shared event holds no GPU memory, the same thing Phase 1 found for queues. The test keeps a reference to one event and expects to be its only owner once the context is destroyed.
- **The depth phase overrides the clear depth inside the compiled graph.** `zest_ConnectOutput` always asks for a depth clear of 1.0 (`zest.h:19750`) and no public call changes it. 1.0 is also what Metal clears to when told nothing, so the test writes 0.25 into the pass's `depth_attachment.clear_value` before the flush.
- **Depth is read back with the test's own blit.** `zest_CopyImageToBitmap` gives a depth format a row size of 0 on Metal, which Metal's validation rejects. That is M1 code and was left alone.
- **The pass descriptor's textures are cleared straight after the encoder is made**, not only the attachments past the pass's count. The descriptor holds a reference to each texture, so a freed image's memory would otherwise stay until the context's next pass. "Metal Render Pass Clear" checks the freed target has no owner left.
- **`end_render_pass` came in with Task 4**, because `end_command_buffer` calls it.
- **The scissor phase uses `zest_cmd_ViewPort` and `zest_cmd_Clip` in place of the screen-sized viewport**, which asserts without a swapchain. Task 6's "Metal Windowed Frames" gained a phase for it.
- **A queue switch phase was added to "Metal Render Pass Clear".** With the context's preferred device queue held, the next batch runs on another queue and waits on the slot's ordering event. It is what makes the ordering signal able to fail.
- **`zest__mtl_collect_validation_messages` reaps even when the validation reader never started**, so a failed batch is reported without validation.
- **"Metal Render Pass Clear" expects exactly one committed buffer after each flush and none after a validation count**, with a device idle wait between flushes so the count doesn't depend on timing.

**Noticed and left alone**
- `zest_CopyImageToBitmap` on a depth image, above.
- Shared code ignores what `begin_render_pass` returns and sets `began_rendering` regardless (`zest.h:17598-17610`), so a pass's callbacks run after a failed begin. The Metal viewport, scissor and clip hooks do nothing without an open encoder for that reason.
- With the ordering signal dropped, the run ended with exit 1 and didn't hang at teardown, which [Decisions_] expected a blocked wait could do. What released the blocked batch wasn't looked into.

### Phase 3: windows (2026-11-23, on top of commit 039033c, built and run on the Mac over SSH)

**What changed**
- `zest_metal.h`, 11 stubs replaced, 50 left:
  - `#import <QuartzCore/CAMetalLayer.h>`, and `<SDL_metal.h>` when SDL is compiled in.
  - The context backend's `layer` is a `CAMetalLayer *` and it gains `drawable` and `present_count`. `zest_mtl_swapchain_backend_t` holds the pixel format.
  - `zest__mtl_swapchain_format` and `zest__mtl_layer_extent`.
  - `create_window_surface`, `initialise_swapchain`, `new_swapchain_backend`, `cleanup_swapchain_backend`, `destroy_context_surface`.
  - `acquire_swapchain_image`, `present_frame`, `dummy_submit_for_present_only`, and the present in `submit_frame_graph_batch`.
  - `get_swapchain_identifier`, `get_swapchain_wait_semaphore`, `get_swapchain_signal_semaphore`.
  - `initialise_context_backend` no longer refuses a windowed context.
- `zest-metal-tests.cpp`: "Metal Swapchain Formats", "Metal Windowed Frames", "Metal Windowed Context", and the window and frame helpers they share.
- No shared file changed, so there is nothing for a Windows run in this phase.

**Results**
- Metal on: `zest-tests`, `zest-c-compile-check` and `zest-objc-compile-check` build with 0 warnings.
- `--platform metal`: 23 / 23 on three runs in a row. With `--shader-validation`: 23 / 23. With `NSZombieEnabled=YES MallocScribble=1`: 23 / 23 and no zombie report.
- `--platform all`: `Dynamic rendering: 129 / 129`, `Legacy render pass: 129 / 129`, `Metal backend: 23 / 23`, exit 0, no VUID line.
- Metal off: every target builds, 129 / 129 and 129 / 129, exit 0, no VUID line.

**What the phase found out about macOS** (`spikes/metal/build/probe/probe_layer_owner.mm` on the Mac, untracked, SDL2 and Metal with no Zest)
- **A layer that has had display sync turned off is never freed.** After `SDL_Metal_DestroyView`, a layer whose `displaySyncEnabled` was only ever YES is released and its drawables' memory comes back. One that was set to NO at any point keeps an owner and its memory, whether or not it presented a frame that way.
  - None of these released it: setting it back to YES, a 4 second wait with the event loop pumped, presenting another frame, `removeFromSuperlayer`, toggling `presentsWithTransaction`.
  - Setting the drawable size to 1x1 first cut what stays to about one drawable.
  - Setting the layer's device to nil before destroying the view also keeps the layer.
  - One view reused by six contexts in turn, alternating display sync, kept the allocated size flat. That is Decision 6.
- **A drawable hands its texture out autoreleased.** Asked for outside an autorelease pool on a thread with no pool, every frame's texture keeps an owner, and the textures of an old size are never freed. Acquire takes the texture inside its pool.
- **Held drawables didn't stall the layer with display sync off.** Four drawables taken and never presented left every `zest_BeginFrame` inside 500 ms. With display sync on, the same four fail the test.
- **With display sync off, a presented drawable that is still retained doesn't stall the layer either.** It only shows as an owner left on the drawable.

**Every new check was broken on purpose** (`spikes/metal/mutate_m2.py` with `mutations_p3.py`)

| What was broken | Result |
| --- | --- |
| Drawable never released in `present_frame` | Windowed Frames fails |
| Acquire checks neither size | Windowed Frames fails |
| Present encoded twice on the presenting batch | Windowed Frames and Windowed Context fail |
| Present never encoded on a batch | Windowed Frames and Windowed Context fail |
| A frame with no work doesn't present | Windowed Frames fails |
| A frame with no work drops the frame signal | Windowed Frames fails |
| A vsync change never reaches the layer | Windowed Frames fails |
| Acquire keeps a drawable an abandoned frame left | Windowed Frames fails |
| Drawable texture taken outside the pool | Windowed Frames fails |
| sRGB swapchain format not passed through | Swapchain Formats fails |
| `present_frame` ignores a resize | Windowed Frames fails |
| Release barrier records no layout | Windowed Frames and Windowed Context fail |
| A new view is made for every context | Windowed Context fails |

**Checks that can't fail yet**
- **Either size check in acquire alone.** With only the layer check or only the drawable check, 23 / 23. Each covers the other in every case the test can make; the drawable check is there for a resize that lands between the two.
- **`set_screensized_viewport`.** Made to do nothing, 23 / 23. The phase proves it raises no validation message. What it sets needs a draw. M4.
- **The swapchain's image backend never freed** on a resize: 23 / 23. It is a block in the context's own allocator, which nothing in the test measures.
- The fence and wave wait items from Phase 2 are unchanged.

**Where it differs from the plan**
- **Decision 6, one view per window**, above. The plan's "leak the SDL view" breakage became "a new view is made for every context".
- **The 500 ms bound doesn't show a held drawable with vsync off**, so two checks were added in its place:
  - "Metal Windowed Frames" keeps the first frame's drawable and expects to be its only owner ten frames later.
  - An abandoned frames phase runs with vsync on: four frames that take a drawable and never present it, where the bound does apply.
- **The plan's "No end frame" phase doesn't reach the release in acquire.** The second `zest_BeginFrame` is refused before it acquires, and `zest_EndFrame(context, 0)` then presents the first one's drawable. The phase is kept as written. The abandoned frames phase is what covers the release.
- **Three checks were added to "Metal Windowed Frames":**
  - A resize must fail the next `zest_BeginFrame`, so no pass runs at the old size.
  - A resize between `zest_BeginFrame` and `zest_EndFrame` must recreate at the present.
  - The allocated size after the third round of resizes must equal the second.
- **Every frame is also checked for the swapchain image being back in the present layout**, which is what makes `release_barrier` able to fail.
- **The queue switch phase doesn't make a batch wait on the ordering event.** Setting `preferred_queue_index` by hand gives the slot the queue asked for, so the acquire reports no switch. The phase shows frames run on either device queue. Phase 2's headless queue switch is what covers the ordering signal.
- **A nil drawable and a present with no batch to carry it are reports, not validation errors**, under `zest_report_swapchain_not_acquired` and `zest_report_submission_failure`.

**Noticed and left alone**
- The SDL Metal view is never destroyed. It stays a subview of its window, and the handle SDL gave for it is not released when the window is destroyed. One small view per window for the life of the process.
- Without SDL, the app's own layer has the same macOS behaviour, and what it does with the layer is the app's.
- MoltenVK's layer was not checked for the same thing.

### Phase 4: the suite and the example (2026-11-23, on top of commit 971d558, built and run on the Mac over SSH and built on Windows)

**What changed**
- `zest-tests.cpp`:
  - `CreateSuiteDevice` has a Metal branch, and `RunSuite` fails a Metal suite whose device has no validation.
  - `metal_pending_tests[]` and `IsMetalPendingTest`, inside `#ifdef ZEST_ENABLE_METAL`.
  - `RegisterTest` skips `vulkan_only_tests[]` on every platform that isn't Vulkan, a pending test on Metal, and any test `--test` doesn't name.
  - A suite left with no test prints a line and is skipped.
  - The Metal process runs the suite in a 1280x768 window, then the backend list, and writes four counts.
  - `Metal suite: N / N` joins the summary and the exit code.
- `zest-metal-tests.cpp`: `RunMetalTestsInChildProcess` reads four counts and passes `--test` on. `WriteMetalRunCounts`.
- `example_platform.h`: `use_metal`, `--metal`, `ExampleValidationFlags`, the early `zest_PrepareMetalValidation` call, the Metal device in `CreateExampleDevice`, and a check in `FinishExample`.
- `zest-clear-screen.cpp`: defines `ZEST_METAL_IMPLEMENTATION` under `ZEST_ENABLE_METAL`.
- `zest_metal.h` is unchanged in this phase. 50 stubs remain, where M2 started with 88.

**Results**
- Mac, Metal on. Every target builds. The only warnings are 9 from `examples/libs/stb_image_write.h`, which a Metal off build gives too.
  - `--platform metal`, three runs: `Metal suite: 52 / 52`, `Metal backend: 23 / 23`, and no Metal validation record in the suite's part of the log.
  - `--platform metal --shader-validation`: 52 / 52 and 23 / 23.
  - `NSZombieEnabled=YES MallocScribble=1`: 52 / 52 and 23 / 23, no zombie report.
  - `--platform all`: `Dynamic rendering: 129 / 129`, `Legacy render pass: 129 / 129`, `Metal suite: 52 / 52`, `Metal backend: 23 / 23`, exit 0, no VUID line.
  - `zest-clear-screen --metal --validate --frames 240`: exit 0, `validation errors: 0`, API validation on. With `--validate-sync`: the same, with shader validation on too. `--vulkan --validate --frames 240`: exit 0, `validation errors: 0`.
- Mac, Metal off. Every target builds. 129 / 129 and 129 / 129, no VUID line. `zest-clear-screen --metal` prints that Metal isn't compiled into the example and exits 1.
- Windows. Every target builds in Debug, 28 projects, no warning and no error. The suites were not run.

**The Metal suite: 52 tests**
- The 33 starting tests all passed as they were.
- The sweep ran each of the 99 pending tests alone, in its own process, with the pending check switched off by a local edit. 19 passed and came off the list:
  - "Stress Test Simple", "Stress Test Pass Dependencies", "Stress Test Pass Dependency Chain"
  - "Resource Test Image Format Edge Cases", "Resource Test Image View Creation", "Resource Test Uniform Buffer Descriptor Management", "Resource Test Uniform Buffer Edge Cases", "Resource Test Staging Buffer Operations", "Resource Test Image With Pixels Creation", "Resource Test Image Array Descriptor Indexes"
  - "User Test Unused Swapchain", "User Test Acquire/Release Indexes", "Essential Unused Transient"
  - "Resource Test Image Allocator Memory Type Keying", "Resource Test Large Readback Buffer", "Resource Test Concurrent Pool Growth", "Resource Test Structured Buffer Index"
  - "Device Reset Deferred Frees", "Device Reset Bindless Indexes"
- The 80 still pending, by what the sweep saw each stop at:

| Group in `metal_pending_tests[]` | Tests | What stopped them | Milestone |
| --- | --- | --- | --- |
| Transient images | 16 | `create_transient_image_unbound`, `new_frame_graph_image_backend` | M3 |
| Transient buffers | 8 | `create_arena_backing` | M3, then M4 for the three layer tests and M7 for "GPU Profiler Timestamps" |
| Shader compiling | 47 | `compile_shader`, at an assert in shared code that ends the process | M4, then M5 for the multi queue and device reset ones |
| Shader compiling from Slang | 7 | Not built on the Mac | M6 |
| A memory allocation count limit | 1 | "Resource Test Pooled Image Allocations" expects one unless the platform is D3D12, and Metal reports 0 as D3D12 does | A one word test edit |
| Instance extensions | 1 | "Device Reset Pool Sizing" expects some unless the platform is D3D12 | A one word test edit |

- The 4 tests in `vulkan_only_tests[]` never run on Metal.

**Checks made on the new runner and example code**
- With the `zest_PrepareMetalValidation` call removed from `main`: the suite prints its red line and counts 0 / 1, the backend list gives 20 / 23, exit 1.
- With the call removed from `ParseExampleOptions`: `zest-clear-screen --metal --validate` prints "Metal validation was not active" and exits 1.
- `--test Swapchain`: 4 tests in each suite on Vulkan and on Metal, all passing, with the Metal backend list unfiltered. `--platform all --test Swapchain` passes the text on to the Metal process.
- `--test zzz`: the suite prints that no test has the text in its name and counts 0 / 0, exit 0.

**Where it differs from the plan**
- **`FinishExample` fails a Metal run whose validation never came on.** Without it, an example that lost its early call still printed `validation errors: 0` and exited 0, with only Zest's alert to show for it. It reads the device backend's `api_validation`, as the D3D12 branch beside it reads an internal count.
- **The example helper keys on `ZEST_METAL_IMPLEMENTATION`, not `ZEST_ENABLE_METAL`.** CMake defines the second for every example on a Metal build, and only `zest-clear-screen` compiles the backend in. Any other example given `--metal` prints "Metal is not compiled into this example" and exits 1.
- **`ExampleValidationFlags`** is new, so the early call and `CreateExampleDevice` can't disagree on the flags.
- **Two device reset tests run on Metal.** [Scope_] left the device reset tests to M5. Two of the four pass now, so the sweep's rule took them off the list.
- **`MetalRunCounts` is declared in `zest-tests.cpp` on every platform**, as `main`'s exit code reads it, and `zest-metal-tests.cpp` is included after it.
- **The sweep needed no code.** The pending check was switched off by an edit on the Mac for the length of the sweep.

**One failure that didn't come back**
- "Metal Windowed Context" failed once, in the first Metal run after the sweep, which had just ended about 50 processes at an assert. The line it prints with the reason wasn't kept. The same test then passed more than 50 runs in a row, 12 of them with every core loaded. Its two timing sensitive checks are the 500 ms bound on `zest_BeginFrame` and the allocated size after each context.

**Checks that still can't fail, for all of M2**
- A pass that doesn't wait on, or doesn't update, its queue's fence. M4.
- The waits `begin_command_buffer` encodes: the wave waits (M5) and the ordering wait (M4).
- `set_screensized_viewport`, and whether a zero-area scissor draws nothing. M4.
- Either size check in `acquire_swapchain_image` alone. Removing both fails.
- The swapchain's image backend not being freed on a resize.
- A backing scale of 2. Nothing on the mini's display exercises it.

**Shared edits for Peter's Windows run (Phase 4)**
- `zest-tests.cpp`: `--test`, the empty suite line, `RegisterTest` skipping the Vulkan only tests on any platform that isn't Vulkan where it named D3D12, the `MetalRunCounts` struct and its part of the exit code. The rest is inside `#ifdef ZEST_ENABLE_METAL`.
- `example_platform.h`: `use_metal` and `--metal`, `ExampleValidationFlags` in place of the same lines in `CreateExampleDevice`, `!options->use_metal` beside the two `!options->use_d3d12` tests, and `validation_inactive` in `FinishExample`, which is always false without the Metal backend.
- `zest-clear-screen.cpp`: three lines under `ZEST_ENABLE_METAL` and two comments.
- Expected on Windows: what [Testing_] lists.

**For M3's plan**
- The two pending tests that only exempt D3D12. Changing each exemption to "only Vulkan has this" is a one word edit to a shared test.
- `zest_CopyImageToBitmap` gives a depth format a row size of 0 on Metal, which Metal's validation rejects. It belongs with the remaining immediate functions.
- The SDL Metal view is never destroyed (Decision 6). `zest_implsdl2_DestroyWindow` could destroy it, which is a shared edit.
- MoltenVK's layer wasn't checked for the display sync behaviour Phase 3 found.
- The intermittent "Metal Windowed Context" failure, if it shows again: the test prints which check failed.
