# Unwaited command graph lifetimes

**Goal:** A command graph flushed without a wait keeps everything it uses alive until its GPU work is proven finished, on every backend, windowed or headless. Users no longer need `zest_FlushFrameGraphAndWait` or `zest_SignalTimeline` just because a graph uses per-mip views. A frame's resources are no longer released early because a command graph was flushed after `zest_EndFrame`.

**Architecture:** Every batch of every graph already signals its queue slot's ordering timeline (`context->queue_ordering_timelines[slot]`). All three backends do this, in order within a slot. An unwaited flush records each slot's ordering value against the frame in flight its releases are queued on. `zest__do_context_scheduled_tasks` is the one drain behind `zest_BeginFrame`, the headless waited flush and `zest_DrainContextResources`. It first waits for those recorded values, then releases. Per-mip view arrays move from the builder's linear memory to the device allocator. A deferred entry then no longer points into memory that a flush or `zest_BeginFrame` resets, and an unwaited flush can defer them like its bindless indexes. A command graph's flush also stops replacing the frame's sync timeline, so `zest_BeginFrame` always waits on the frame's own work. The hot path sees three compares per drain when nothing is recorded.

**Tech stack:** `zest.h` throughout. The value wait in Task 3 adds one platform member to `zest_vulkan.h`, `zest_dx12.h` and `zest_metal.h` (Decision 2). Tests are in `examples/SDL2/zest-tests/`. No shader changes, no new public API.

**Spec:** Peter's decision of 2026-11-24, after M5's sanity check: one shared plan of its own, outside Metal M6, "a context-owned timeline that zest_BeginFrame and the headless drain wait on, and per-mip view arrays outside frame-linear memory". Background is in `metal_m5_implementation.md`: Phase 1 As built, "After the sanity check", and "For M6's plan" item 4. The memory note is "Unwaited command graph lifetimes".

Contents:
- [Constraints_] Global Constraints
- [Settled_] Settled
- [State_] Current state
- [Decisions_] Decisions for Peter
- [Focus_] Review Focus
- [Scope_] Out of scope
- [Phase_views] Phase 1: per-mip views outside frame-linear memory
- [Phase_drain] Phase 2: the drain waits for unwaited command graphs
- [Phase_docs] Phase 3: docs and final check
- [Cost_] Per-frame cost
- [Testing_] Testing

## [Constraints_] Global Constraints

- Never commit. Peter handles all commits.
- Follow code-style. Keep CRLF on the C and C++ sources and check them with `file` after editing. Write any script that holds a backslash or apostrophe with the Write tool.
- Edit `zest.h` and the test files. Edit `zest_vulkan.h`, `zest_dx12.h` and `zest_metal.h` only for Task 3's platform member, and only if Peter takes Decision 2's recommendation.
- Windows build: `cmake --build E:/Projects/C++/Zest-Build --config Debug --target zest-tests zest-c-compile-check`, with no new warning. Run `qgrep update zest` after code changes.
- Mac runs:
  - Sync with the scratchpad's `m5_sync.sh` pattern, which needs `zest-frame-graph-tests.cpp` in its file list. Build `../Zest-Build-Metal` (Metal on) and `../Zest-Build` (Metal off).
  - Restore the clone afterwards with `git checkout --`. `spikes/` stays untracked.
  - Check `IOConsoleLocked` first, as a locked screen fails "Metal Windowed Frames".
  - Never overlap test runs.
- The implementer runs the Mac suites. Peter runs Windows `zest-tests.exe` and `--gbv`, which cover D3D12 and Windows Vulkan. Launching an example without `--frames` is Peter's.
- Performance budget:
  - A frame with no unwaited command graph pays three compares in the drain.
  - A graph that never asks for per-mip indexes pays nothing.
  - No new GPU signals or waits on any queue.

## [Settled_] Settled

- **Fixed in shared code, for every backend.** Peter (2026-11-24): nothing users must do on one backend but not another, and a doc note is a stopgap. The API comment above `zest_FlushFrameGraph` that asks users to wait for graphs with per-mip views goes once this lands.
- **An unwaited flush still doesn't wait.** The wait moves to the drain, so `zest_FlushFrameGraph` keeps its asynchronous behaviour.
- **The drain waits for recorded values, never for a timeline's current value.** A current value includes work submitted after the graph, such as the frame in flight, so waiting for it would stall `zest_BeginFrame` on the previous frame's GPU work and lose the CPU and GPU overlap.
- **The drain's timeout is `create_info.max_semaphore_timeout_ms`** (10 s by default, `zest.h:15257`), the limit the frame wait already gives up at. On a timeout or error it reports `zest_report_submission_failure` and keeps that frame in flight's lists for the next drain. That leaks nothing and frees nothing early. It never asserts, as a device lost reaches here.
- **Per-mip view arrays come from the device allocator.** That is the backends' existing path when no linear allocator is passed (`zest_vulkan.h:4989-4992`, `zest_dx12.h:5994-5996`, `zest_metal.h:4104-4106`). `ZLOC_THREAD_SAFE` is on (`zest.h:124`), so contexts on different threads can share it. Rejected: a per-frame-in-flight linear allocator for views, which would need its own growth and consolidation code for a path few graphs take.
- **A waited flush keeps releasing its own entries straight away** (`zest.h:17187-17213`). A loading loop of waited flushes on a windowed context must not pile up bindless indexes until the next frame.
- **A command graph's flush leaves `frame_sync_timeline` alone.** The backends set it from any graph's signal timeline (`zest_vulkan.h:6123`, `zest_dx12.h:7160`, `zest_metal.h:4655`). Its wait, or the unwaited record, already covers a command graph. The flush saves the frame in flight's entry before executing and puts it back after.
- **The memory note is updated when the work lands.** It is a lead, not a fact, and its claims were checked against the code below.

## [State_] Current state (verified 2026-11-25)

**The flush** (`zest_FlushFrameGraph`, `zest.h:17103-17218`)
- `gpu_finished` (`:17109`) is false once the graph has run (`:17122`) and true again only after a successful wait on its signal timeline (`:17136`).
- When it's false, the graph's transient bindless indexes move to the context's list for the frame in flight (`:17179-17186`, M5 Phase 1).
- The graph's own per-mip view arrays are destroyed at once either way (`:17207-17213`). Its `transient_images` list is freed at `:17195-17205`, but nothing ever fills that list: no push to it exists in any header.
- The flush ends with `zest__cleanup_frame_graph_builder` and a reset of the context's frame-in-flight allocator (`:17214-17215`).
- A waited flush on a headless context drains the context's lists (`:17152-17166`). An unwaited flush on a headless context reports once (`:17168-17171`).
- `zest_FlushFrameGraphAndWait` attaches the utility timeline and flushes (`:17220-17228`).

**Deferred lists**
- `zest_context_destruction_queue_t` (`zest.h:4099-4107`) has per-frame-in-flight lists. `transient_view_arrays` holds view array structs by value.
- Frame graphs queue on the context's lists (`zest.h:15518`). Command graphs get their own copy in their builder memory (`:15520-15521`).
- The drain is `zest__do_context_scheduled_tasks` (`zest.h:10176-10255`), with view arrays at `:10198-10204`. It runs from:
  - `zest_BeginFrame` (`:9678`), before the frame-in-flight allocator is reset or consolidated (`:9680-9696`), which the comment at `:9677` explains
  - the headless waited flush (`:17158`)
  - `zest_DrainContextResources` (`:10257-10260`)
- Context teardown waits for an idle device (`zest.h:9803`), then frees each list directly (`:12333-12367`), with view arrays at `:12356-12362`.
- `zest_GetContextPendingReleaseCount` counts every list (`zest.h:14023-14036`).
- Arena returns go to the context's list at the end of every execution (`zest__return_frame_graph_arenas`, `zest.h:15220-15227`). A waited flush pulls its own out early (`:17142-17151`). Retired transient images go to `arena_images` (`zest.h:14865`).
- `zest__warn_headless_deferred_growth` (`zest.h:10262-10267`) is the rate-limited report for a headless list that keeps growing.

**The frame wait**
- `zest__main_loop_semaphore_wait` (`zest.h:10269-10299`) waits only on `frame_sync_timeline[fif]` (`zest.h:7464`), then clears it.
- Each backend sets that entry from its present-only submit (`zest_vulkan.h:1770`, `zest_dx12.h:3084`, `zest_metal.h:2074`). It also sets it from the last wave of any graph with a signal timeline (`zest_vulkan.h:6123`, `zest_dx12.h:7160`, `zest_metal.h:4655`).
- **So a command graph with a signal timeline replaces the frame's entry.** This includes `zest_FlushFrameGraphAndWait`'s utility timeline. Flushed between `zest_EndFrame` and `zest_BeginFrame`, that graph's timeline becomes what the frame in flight's next `zest_BeginFrame` waits on, and it is already reached. If the graph ran on the compute or transfer slot, nothing proves the frame's own work finished before its lists drain. Found by reading, not reproduced.

**Queue ordering timelines**
- `context->queue_ordering_timelines[ZEST_QUEUE_COUNT]` (`zest.h:7444-7445`) are created with the context (`:11746-11748`) and cleaned up at teardown (`:12371-12373`).
- Every batch signals its slot's timeline with the next value:
  - Vulkan: `zest__vk_add_queue_ordering_semaphores` (`zest_vulkan.h:6007-6023`), called for every batch at `:6104`, rolled back when a submit fails (`:6157`)
  - D3D12: `zest_dx12.h:7178-7184`, rolled back on failure
  - Metal: `zest_metal.h:4663-4666`. The command buffer is committed straight after, with no failure path to roll back (`:4681-4683`).
  - The present-only submits signal them too (`zest_vulkan.h:1779`, `zest_dx12.h:3086`, `zest_metal.h:2075-2077`).
- A batch that moves its slot to another device queue first waits on the slot's last value, so within a slot the values complete in order.
- A graph's last wave is always one batch. The compiler adds a sync wave when the last wave spans queues (`zest.h:16423-16450`). That batch waits on every earlier wave, so the slot value it signals proves the whole graph finished.

**Timeline waits**
- `platform->wait_for_timeline(timeline, timeout)` (`zest.h:4728`) always waits for `timeline->current_value`: Vulkan `zest_vulkan.h:3281-3296` (`:3287`), D3D12 `zest_dx12.h:3460-3462`, Metal `zest_metal.h:2406-2408`. No member waits for a given value.

**Per-mip views**
- `zest_GetTransientSampledMipBindlessIndexes` (`zest.h:19197-19231`) creates the view array from `zest__frame_graph_builder->allocator` (`:19209-19211`) and points its handle at the device's view array store (`:19216`). The struct and the `views` block, which holds the backend array after the public one, both sit in that linear memory.
- At the end of every execution, `zest__execute_frame_graph` retires each transient's view array (`zest.h:17762-17774`) through `zest__deferr_view_array_destruction` (`:17388-17390`). That copies the struct by value into the graph's list for the frame in flight.
- The backends' cleanup reads only `views[]` and the store's origin, never `views[i].image`: Vulkan `zest_vulkan.h:3142-3151`, D3D12 `zest_dx12.h:5971-5980`, Metal `zest_metal.h:4081-4090`.
- `zest__cleanup_image_view_array` (`zest.h:12778-12785`) is for user arrays: it removes the handle from the store, which a transient array never joined, so the transient path mustn't use it.
- **A second hazard in the same memory, found by reading.** Every backend's compile allocates batch backends from `context->frame_graph_allocator[current_fif]`, for command graphs too (`zest_vulkan.h:2881`, `zest_dx12.h:3241`, `zest_metal.h:2191`). Vulkan also puts its barrier vectors there (`zest_vulkan.h:6196-6243`).
  - Between `zest_EndFrame` and `zest_BeginFrame`, the first command graph's flush resets that allocator (`zest.h:17215`).
  - A second command graph then writes from offset 0, over the ended frame graph's memory. That memory holds the `views` blocks its deferred view arrays still point to until `zest_BeginFrame` drains them.
  - Moving the views to the device allocator removes this hazard.

**Tests that exist near the change**
- "Compute Test Immediate Execute No Wait" (`zest-compute-tests.cpp:580`): two unwaited graphs, whose transient buffers must get different indexes.
- "Transient Mip Views Across Allocator Growth" (`zest-frame-graph-tests.cpp:1706`): per-mip views in a frame graph, with its helper `tst__acquire_transient_mip_indexes` (`:1698`). It is the only test that asks for transient per-mip indexes.
- "Intraframe Two Graphs" (`zest-frame-graph-tests.cpp:2673`), "Compute Test Headless Flush And Wait" (`zest-compute-tests.cpp:753`), "Compute Test Immediate Execute Cached" (`zest-compute-tests.cpp:179`).
- Metal's gate helpers hold a device queue behind a shared event until a thread opens it: `METAL_GATE_DELAY` (`zest-metal-tests.cpp:5043`), `metal_close_gate` (`:5067`), `metal_open_gate_after` (`:5078`), `metal_finish_gate` (`:5085`). "Metal Cross Queue Order" shows how to hold the queue a slot will use.

**Wiring to mirror** (M5's index deferral and the arena returns)
- Deferring: the `!gpu_finished` block in `zest_FlushFrameGraph` (`zest.h:17179-17186`).
- Releasing: `zest__do_context_scheduled_tasks`, the context teardown loop and the waited flush's own release, all three.
- Counting: `zest_GetContextPendingReleaseCount`.
- Headless growth: `zest__warn_headless_deferred_growth`, as `zest__return_frame_graph_arenas` calls it (`zest.h:15225`).

## [Decisions_] Decisions for Peter

Peter took all three recommendations on 2026-11-25: the slot ordering timelines, a `wait_for_timeline_value` platform member, and Decision 3 kept out of this plan.

1. **Which timeline the drains wait on.**
   - **Recommended: the queue slots' existing ordering timelines.** They are context-owned, already signalled by every batch on all three backends, and complete in order within a slot. No new signal goes on the GPU.
   - **Alternative: one new context timeline,** attached as each unwaited graph's signal timeline, as the decision of 2026-11-24 worded it. That has three costs:
     - Attaching it makes the backends replace `frame_sync_timeline`, which this plan stops command graphs doing.
     - The flush's `if (signal_timeline)` wait needs a separate flag.
     - Graphs whose last batch is on different slots signal one timeline out of order. A value going backwards is invalid on Vulkan and wrong on D3D12 fences.
2. **How to wait for a given value.**
   - **Recommended: a platform member** `zest_semaphore_status (*wait_for_timeline_value)(zest_execution_timeline timeline, zest_u64 value, zest_microsecs timeout)`, placed after `wait_for_timeline`. Each backend's existing wait becomes this with `timeline->current_value`. That is a small edit to `zest_vulkan.h`, `zest_dx12.h` and `zest_metal.h`.
   - **Alternative, with no backend edit:** shared code passes the existing wait a stack copy of the timeline with `current_value` set to the recorded value. It works because all three waits read only `device`, `backend` and `current_value`, but it depends on that staying true.
3. **Cached command graphs keep backend data in the frame-in-flight allocator.**
   - **What it is.** This was found by reading and not reproduced. A command graph's builder memory is its own, and caching promotes only that (`zest.h:15394-15399`). Its batch backends and Vulkan barrier vectors are written to the context's frame-in-flight allocator at compile (see Current state). A cached command graph replayed after another graph compiled into that allocator reads overwritten data. "Compute Test Immediate Execute Cached" never compiles anything in between, so it can't see this.
   - **Recommended: keep it out of this plan.** Write a repro first: a headless context, a cached command graph with a barrier, a second graph compiled after its flush, then the cached one flushed again. Then plan the fix on its own: the backends allocate compile-time data from `zest__frame_graph_builder->allocator`.
   - **Alternative:** fold it into this plan as a Phase 4.

## [Focus_] Review Focus

1. **A wait for a value that never comes.**
   - **Risk:** a submit that failed after its ordering value was counted would hang the drain.
   - **What guards it:** the backends roll the value back (`zest_vulkan.h:6157`, `zest_dx12.h:7181`), and the flush records only after `zest__execute_frame_graph` succeeded. The drain also has a timeout and keeps its lists on failure.
   - **Check:** Metal commits straight after counting the value, so its only risk is a command buffer that errors on the GPU. Check whether Metal still signals the event after such an error. If it doesn't, report it and don't fix it in this plan. The D3D12 forced signal failure tests on Peter's Windows run must not hang.
2. **Over-waiting.**
   - **Risk:** waiting for a slot's current value instead of the recorded one stalls `zest_BeginFrame` on the previous frame.
   - **Check:** "Metal Unwaited Graph Drain" drains the other frame in flight while the gate holds a later graph on the same slot, and that drain must return in less than `METAL_GATE_DELAY`.
3. **View array memory.**
   - **Risk:** each heap array must be freed exactly once on every path: the drain, teardown, the waited flush, and a failed execution (`gpu_finished` stays true and the arrays are released at the flush).
   - **Check:** the device's leak report at shutdown, with no validation errors in any suite. Run "Transient Mip Views Across Allocator Growth" on its own too (`--test`).
4. **Frame sync.**
   - **Risk:** with the restore wrong, `zest_BeginFrame` stops waiting on the frame's own work, or on Metal and D3D12 a present waits on the wrong fence (`zest_dx12.h:2964-2967`).
   - **Check:** "Command Graphs After End Frame" checks `frame_sync_timeline[fif] == &frame_timeline[fif]` after a waited flush, and the windowed suites must stay clean.
5. **Headless contexts keep one frame in flight.**
   - **Risk:** every record and drain lands on the same slot, so a waited flush now also waits for every earlier unwaited flush.
   - **Check:** this is intended and is what makes the headless drain safe. "Unwaited Graph Mip Views" relies on it.
6. **Behaviour changes users can see.** Each is correct, but the As built must name them:
   - **A long graph now holds up `zest_BeginFrame`.** An unwaited graph still running when its frame in flight comes round makes `zest_BeginFrame` wait for it. It has to: that frame in flight's command pool is reset by the first submission after it (`zest.h:17518-17522`).
   - **A wait on a later signal stalls until the timeout.** If an unwaited graph's GPU work waits on a timeline that something submitted only after that `zest_BeginFrame` will signal, the drain waits the full timeout, reports, and goes on.
   - **The frame wait's timeout callback isn't consulted.** The drain uses `max_semaphore_timeout_ms` as one wait, not `frame_wait_timeout_callback`.

## [Scope_] Out of scope

- Decision 3's cached command graph data, unless Peter folds it in.
- The `transient_images` lists in `zest_context_destruction_queue_t` are never filled. Their release loops (`zest.h:10186-10196`, `:12344-12354`, `:17195-17205`) could go in a later tidy-up.
- A command graph's builder allocator still can't grow, and asserts since M5 (`zest.h:10624`).
- A host-signalled gate for the Vulkan and D3D12 suites. Only Metal's tests can hold a queue, so the race itself is proven on Metal, and the shared tests prove the bookkeeping.
- Two graphs in one frame that share an imported image on different queues (the frame-graph skill's last imported image note).

## [Phase_views] Phase 1: per-mip views outside frame-linear memory

### Task 1: View arrays from the device allocator, deferred by unwaited flushes

**Files:** `zest.h`: `zest_GetTransientSampledMipBindlessIndexes` (`:19209-19211`), `zest__deferr_view_array_destruction` (`:17388-17390`), `zest__do_context_scheduled_tasks` (`:10198-10204`), the teardown loop (`:12356-12362`), `zest_FlushFrameGraph` (`:17179-17186`, `:17207-17213`), the comment at `:9677`, and the private prototypes with the other `zest__deferr_` ones.

**Interfaces:**
- `ZEST_PRIVATE void zest__free_transient_view_array(zest_device device, zest_image_view_array view_array);` destroys the views through `cleanup_image_view_array_backend`, then frees `view_array->views` with `device->allocator`. Comment: `//A transient view array was never in the store, so only its views are destroyed and freed`.
- `zest__deferr_view_array_destruction` keeps copying the struct into the list, then frees the struct with `device->allocator`. The copy owns the `views` block.

**Steps:**
- [x] Capture baselines on the Mac (screen unlocked): `--platform all` and `--platform metal --shader-validation`. Expected: `130 / 130`, `130 / 130`, `Metal suite: 125 / 125`, `Metal backend: 51 / 51`, no VUID line.
- [x] Pass `NULL` instead of the builder's allocator to `create_image_views_per_mip`. The store line stays.
- [x] Free the struct in `zest__deferr_view_array_destruction` after the copy.
- [x] Use `zest__free_transient_view_array` in the three release loops.
- [x] In the `!gpu_finished` block, move the graph's view arrays to the context's list for the frame in flight, as the indexes are, and free the graph's vector. Call `zest__warn_headless_deferred_growth(context, size, "transient view array")`.
- [x] Delete the comment at `zest.h:9677`, as no deferred entry points into frame memory any more. The drain still runs first.
- [x] Remove the per-mip sentence from the API comment above `zest_FlushFrameGraph` (`zest.h:5631-5633`). What's left says that without a signal timeline the flush can't wait, and its resources are released once the frame in flight comes round.
- [x] Build on Windows. Expected: no new warning.

### Task 2: Tests for the views

**Files:** `zest-compute-tests.cpp` (after `test__immediate_execute_no_wait`), `zest-frame-graph-tests.cpp` (after `test__cached_graph_allocator_growth`), `zest-tests.cpp` (registration).

**Interfaces:**
- **"Unwaited Graph Mip Views"** (headless, 1 frame, registered after "Compute Test Immediate Execute No Wait"):
  - A command graph has a compute pass with a 4-mip transient image, flagged essential, as its output. The task asks for the per-mip indexes, as `tst__acquire_transient_mip_indexes` does.
  - It is flushed with `zest_FlushFrameGraph`.
  - Right after the flush, the context's `transient_view_arrays[fif]` holds 1 entry and `transient_binding_indexes[fif]` at least 4.
  - Then `zest_FlushFrameGraphAndWait` on a small graph empties both, and the validation error count is 0.
- **"Command Graphs After End Frame"** (windowed, `ZEST_MAX_FIF * 3` frames, registered after "Cached Graph Allocator Growth"):
  - Each frame's graph asks for per-mip indexes, as "Transient Mip Views Across Allocator Growth" does.
  - After `zest_EndFrame` come two unwaited command graphs and one `zest_FlushFrameGraphAndWait`, each with a transient buffer written by a compute pass.
  - The test checks no validation error, every pass ran, and the context's view array lists empty at each slot's next `zest_BeginFrame`.
  - Phase 2 adds its frame sync check.

**Steps:**
- [x] Write both tests and register them.
- [x] Mac, every target with Metal on and off. Expected: `132 / 132` twice, `Metal suite: 127 / 127`, `Metal backend: 51 / 51`, the same with shader validation, and no VUID line.
- [x] Break: take out the view array move in the `!gpu_finished` block. Expected: "Unwaited Graph Mip Views" fails on all three suites.
- [x] Break: pass the builder's allocator again. Record what "Command Graphs After End Frame" does on each suite. The overwrite depends on layout, so this break is a probe, not a must-fail.
- [x] Restore the clone.

## [Phase_drain] Phase 2: the drain waits for unwaited command graphs

### Task 3: A value wait on every backend

Depends on Decisions 1 and 2. If Peter takes Decision 2's alternative, this task is one private shared function instead, and touches no backend.

**Files:** `zest.h:4728` (the platform member). `zest_vulkan.h:3281-3296` and `:729`. `zest_dx12.h:3460-3500` and `:2624`. `zest_metal.h:2406-2424` and `:1457`. The prototypes in each backend's list.

**Interfaces:**
- `zest_semaphore_status (*wait_for_timeline_value)(zest_execution_timeline timeline, zest_u64 value, zest_microsecs timeout);`
- `zest__vk_wait_for_timeline_value`, `zest__dx_wait_for_timeline_value` and `zest__mtl_wait_for_timeline_value` hold the existing bodies with `value` in place of `timeline->current_value`. Each `wait_for_timeline` calls its own with `timeline->current_value`.
- Comment on the member: `//Wait until the timeline reaches value, which may be below its current value`.

**Steps:**
- [x] Add the member and the three implementations, and assign them in each platform table.
- [x] Build on Windows (Vulkan and D3D12) and on the Mac (Metal on and off). Expected: no new warning, suites unchanged from Task 2.

### Task 4: Record, wait, and leave the frame's sync alone

**Files:** `zest.h`: the context struct after `queue_ordering_timelines` (`:7445`), `zest_FlushFrameGraph` (`:17119-17122`, `:17179`), `zest__do_context_scheduled_tasks` (`:10176`), the private prototypes.

**Interfaces:**
- Context field: `zest_u64 unwaited_ordering_values[ZEST_MAX_FIF][ZEST_QUEUE_COUNT];	//Each slot's ordering value the unwaited command graphs on a frame in flight reach, 0 when none waits`
- `ZEST_PRIVATE zest_bool zest__wait_for_unwaited_graphs(zest_context context);`
  - For each slot with a non-zero value on `context->current_fif`, it waits with `wait_for_timeline_value(&context->queue_ordering_timelines[slot], value, timeout)`. The timeout is `create_info.max_semaphore_timeout_ms` in microseconds.
  - It clears each value that was reached.
  - On a timeout or error it reports `zest_report_submission_failure` and returns `ZEST_FALSE`.
- `zest__do_context_scheduled_tasks` calls it first, and returns at once when it fails, keeping every list.

**Steps:**
- [x] In the `!gpu_finished` block, set each slot's `unwaited_ordering_values[current_fif][slot]` to `queue_ordering_timelines[slot].current_value`. Comment: `//The drain for this frame in flight waits for these, as nothing waited for the graph`.
- [x] Before `zest__execute_frame_graph` in `zest_FlushFrameGraph`, save `frame_sync_timeline[current_fif]`. Put it back after, on success and failure alike. Comment: `//A command graph is covered by its own wait or the unwaited record, so the frame keeps waiting on its own work`.
- [x] Add `zest__wait_for_unwaited_graphs` and the call at the top of the drain.
- [x] Add the frame sync check to "Command Graphs After End Frame": after the waited flush, `context->frame_sync_timeline[fif] == &context->frame_timeline[fif]`.
- [x] Add to "Unwaited Graph Mip Views": after the unwaited flush, at least one slot's recorded value equals that slot's current value. After the waited flush, all are 0.
- [x] Build on Windows and the Mac. Expected: suites as in Task 2, plus the new checks passing.

### Task 5: Prove the wait on Metal

**Files:** `zest-metal-tests.cpp`, after "Metal Cross Queue Order", registered in the backend list.

**Interfaces:**
- **"Metal Unwaited Graph Drain"** (headless):
  1. Flush unwaited graph G1 on frame in flight 0, and let it finish.
  2. Close a gate on the device queue the compute slot will use, as "Metal Cross Queue Order" does.
  3. With `zest_SetFrameInFlight(context, 1)`, flush unwaited graph G2 behind the gate. G2 has a transient buffer and per-mip views.
  4. `zest_DrainContextResources` for frame in flight 0 must return in under `METAL_GATE_DELAY`.
  5. Open the gate with `metal_open_gate_after`. `zest_DrainContextResources` for frame in flight 1 must take at least `METAL_GATE_DELAY` and leave its lists empty.
  6. Restore the frame in flight, and finish the gate on every exit path.
- If the thread fails to start, open the gate first, as the cross queue test does.

**Steps:**
- [x] Write and register the test.
- [x] Mac, with the screen unlocked: `--platform all` and `--platform metal --shader-validation`. Expected: `132 / 132` twice, `Metal suite: 127 / 127`, `Metal backend: 52 / 52`, no VUID line.
- [x] Break: return `ZEST_TRUE` at the top of `zest__wait_for_unwaited_graphs`. Expected: "Metal Unwaited Graph Drain" fails on its second drain, and "Unwaited Graph Mip Views" still passes, as its waited flush waits anyway.
- [x] Break: wait for each slot's current value instead. Expected: the first drain fails, as it waits for the gate.
- [x] Break: drop the restore of `frame_sync_timeline`. Expected: "Command Graphs After End Frame" fails on all three suites.
- [x] Every Mac target with Metal on (22) and off (21). Expected: no warning from Zest's files, and the Metal-off suites `132 / 132` twice.
- [x] Probe: take out the `zest_WaitForIdleDevice` that "Intraframe Two Graphs" needs only because nothing ever waited on its command graph's command buffer. Its comment names VUID-vkResetCommandPool-commandPool-00040. Run it alone on the Mac's two Vulkan suites. Report whether the drain's wait now retires that command buffer for the validation layer. Change the test only if Peter agrees.
- [x] Restore the clone.

## [Phase_docs] Phase 3: docs and final check

### Task 6: Docs, memory and the final run

**Files:**
- `zest.h:5250-5255`: the comment above `zest_DrainContextResources`, which says it is only safe once the GPU is done with the current frame in flight's resources.
- `.claude/skills/frame-graph/SKILL.md`: row `:107` and the headless paragraph `:196`.
- `.claude/skills/device-and-context/SKILL.md`: row `:120` and the paragraph `:172`.
- `.claude/skills/images-and-textures/SKILL.md`: its per-mip views notes, if any.
- The memory note `unwaited-flush-releases-transient-bindless-indexes.md` and its MEMORY.md line.
- This plan's As built.

**Steps:**
- [x] The `zest_FlushFrameGraph` row:
  - An unwaited graph's resources wait on the context's lists until the drain proves the graph finished.
  - Per-mip views included.
  - No user action needed.
- [x] The headless paragraph and the `zest_DrainContextResources` row: the drain now waits for unwaited command graphs itself, so it is safe for their resources without an idle device. A windowed context's frame resources still need the frame wait or an idle device.
- [x] The memory note: what is fixed, what Decision 3 left, and that `frame_sync_timeline` is no longer replaced by command graphs.
- [x] Final Mac run as in Task 5, and the Windows build. Run `qgrep update zest`.
- [x] Add `## As built (<date>)`, then tell Peter it is ready for his Windows run and pre-commit check.

## [Cost_] Per-frame cost

| Path | No unwaited graph, no per-mip views | Unwaited command graph flushed | Graph asks for per-mip indexes |
|---|---|---|---|
| `zest_FlushFrameGraph` | Save and restore one pointer | Plus 3 `zest_u64` copies, and a vector push per index and view array (indexes already today) | No change |
| Drain (`zest_BeginFrame`, headless, `zest_DrainContextResources`) | 3 compares | Up to 3 waits for values that have usually been reached. Vulkan, D3D12 and Metal all return without blocking then. | No change |
| Execution | Nothing | Nothing | Per resource per execution: 2 device allocations (struct and views block) instead of 2 linear bumps, 1 free at deferral, 1 free at drain |
| GPU | Nothing | Nothing: no new signal or wait | Nothing |

**Memory:** the context grows by 48 bytes (`ZEST_MAX_FIF * ZEST_QUEUE_COUNT * 8`). A deferred view array holds `count * (sizeof(zest_image_view_t) + backend view)` bytes of device memory until its drain, where before it sat in frame-linear memory. No new buffers.

## [Testing_] Testing

**Automated (the implementer, on the Mac):**
- `--platform all` and `--platform metal --shader-validation` with the screen unlocked. Expected after Task 5: `132 / 132`, `132 / 132`, `Metal suite: 127 / 127`, `Metal backend: 52 / 52`, no VUID line, and no leak report at shutdown.
- Metal off: `132 / 132` twice. Every target builds with Metal on (22) and off (21) with no warning from Zest's files.
- Each task's breaks, as listed.
- Windows: `zest-tests` and `zest-c-compile-check` build in Debug with no warning.

**Peter's tests:**
- Windows `zest-tests.exe` and `--gbv` cover D3D12 and Windows Vulkan. Expected: two more tests than his last run, both passing on every backend, with no validation error and no leak report.
- The new tests: "Unwaited Graph Mip Views", "Command Graphs After End Frame", "Metal Unwaited Graph Drain".
- Existing tests near the change:
  - "Compute Test Immediate Execute No Wait"
  - "Transient Mip Views Across Allocator Growth"
  - "Intraframe Two Graphs"
  - "Compute Test Headless Flush And Wait"
  - "Compute Test Immediate Execute Cached"
  - "Metal Per Mip Views"
  - "D3D12 Per Mip Views"
  - the D3D12 forced signal failure tests

**Visual checks for Peter:** none. Nothing drawn changes. Optionally, run an example that builds mip chains with `--frames`, such as zest-cascading-shadows or a bloom example, and check it looks as before with no validation output.

## As built (2026-11-25)

Built on top of commit afad7b4. Nothing is committed. Peter took all three decisions as recommended.

**Where the code is**
- `zest.h`:
  - `wait_for_timeline_value` is a platform member (`:4730`).
  - `unwaited_ordering_values` is a context field (`:7450`).
  - `zest__wait_for_unwaited_graphs` (`:10180`) is called at the top of `zest__do_context_scheduled_tasks`.
  - `zest_FlushFrameGraph` saves the frame's sync timeline (`:17134`) and puts it back (`:17205`).
  - `zest__free_transient_view_array` is at `:17431`, and `zest__retire_transient_view_arrays` at `:17437`.
  - Per-mip views are created with no linear allocator (`:19257`).
- Backends: `zest__vk_wait_for_timeline_value` (`zest_vulkan.h:3287`), `zest__dx_wait_for_timeline_value` (`zest_dx12.h:3466`), `zest__mtl_wait_for_timeline_value` (`zest_metal.h:2412`). Each `wait_for_timeline` calls its own with the timeline's current value.
- Tests:
  - "Unwaited Graph Mip Views" (`zest-compute-tests.cpp:698`)
  - "Command Graphs After End Frame" (`zest-frame-graph-tests.cpp:1853`)
  - "Metal Unwaited Graph Drain" (`zest-metal-tests.cpp:5277`)

**Where it differs from the plan**
- **The execution failure path also retires transient view arrays.** It never did, which was harmless while they sat in reset frame memory. On the heap they would have leaked, and a cached graph would have reused a stale array. Phase 1 moved the loop into `zest__retire_transient_view_arrays` and calls it on both paths.
- **"Metal Unwaited Graph Drain" gates the queue the first graph actually used.** The plan said the compute queue, but a graph whose only pass is compute runs on the graphics slot. The test finds the slot whose ordering value moved and gates that slot's preferred queue.
- **"Unwaited Graph Mip Views" also fails the break that removes the drain's wait.** The plan expected it to pass, but Task 4's check that the drain clears the recorded values catches that break too.
- **The Intraframe probe was inconclusive.** Without its `zest_WaitForIdleDevice`, "Intraframe Two Graphs" passes with no VUID on the Mac's Vulkan suites, both on this work and on the committed tree. MoltenVK never reported the command pool reset error, so the test keeps its idle. Peter's Windows Vulkan run is where to try it.
- **Mac runs now keep the display awake.** They wake it with `caffeinate -u -t 2` and run the suites under `caffeinate -d`. A sleeping display fails "Metal Windowed Frames" exactly as a locked screen does.

**Final results (Mac, screen unlocked)**
- `--platform all`: `132 / 132`, `132 / 132`, `Metal suite: 127 / 127`, `Metal backend: 52 / 52`, with no VUID line and no leak report.
- Shader validation: `127 / 127` and `52 / 52`.
- Metal off: `132 / 132` twice.
- Every target builds with Metal on (22) and off (21) with no warning from Zest's files.
- Windows: `zest-tests` and `zest-c-compile-check` build in Debug with no warning. The Windows suites were not run.

**Breaks, each failing as it should**
- Phase 1:
  - Without the view array move, "Unwaited Graph Mip Views" fails on all three suites.
  - With views back in linear memory, "Command Graphs After End Frame" fails every frame on all three suites ("in frame memory"). There was no crash or VUID: the overwrite didn't land on the views in this layout.
- Phase 2:
  - With no drain wait, "Metal Unwaited Graph Drain"'s second drain returns in 19 us with the gate closed.
  - Waiting for current values makes its first drain sit out the 2 s timeout.
  - Without the frame sync restore, "Command Graphs After End Frame" fails on all three suites.

**Docs**
- `zest.h`: the comments above `zest_FlushFrameGraph` and `zest_DrainContextResources`.
- The frame-graph skill's `zest_FlushFrameGraph` row and headless paragraph.
- The device-and-context skill's `zest_DrainContextResources` row and deferred freeing paragraph.
- The memory note "Unwaited command graph lifetimes".
- The images-and-textures skill needed nothing, as it makes no lifetime claim.

**After the sanity check (Peter, 2026-11-25)**
- **A hung unwaited graph now stalls one drain, not every one.**
  - `zest__wait_for_unwaited_graphs` flags `zest_context_flag_critical_error` on a timeout or error and reports once.
  - Later drains on a context in critical error only check (timeout 0).
  - `zest__do_context_scheduled_tasks` returns whether it drained. `zest_BeginFrame` returns false when it didn't, as on a fence timeout. A headless waited flush then leaves `device_frame_counter` alone, so no command pool is reset under a running graph.
- **New Metal test "Metal Unwaited Graph Timeout".** A gate holds an unwaited graph past a 50 ms timeout:
  - The first drain takes about 50 ms, flags the error, reports once and keeps the views.
  - The second only checks.
  - Once the gate opens, a drain releases everything.
  - Break: always waiting the full timeout makes the second drain take 51 ms and the test fail.
- **Results:** `--platform all` gave `132 / 132`, `132 / 132`, `Metal suite: 127 / 127`, `Metal backend: 53 / 53`, with no VUID line and no leak report. Shader validation gave the same Metal lines, and Metal off `132 / 132` twice. Every target builds with Metal on (22) and off (21) with no warning, and Windows builds with no warning.
- **Review Focus 1's Metal check (does a failed command buffer still signal its event) is unanswered.** Probes on the Mac's M2 (macOS 26.2) couldn't make a command buffer fail on the GPU:
  - Writes and reads through an unmapped address were dropped, and the command buffers completed and signalled.
  - A write to a buffer purged to empty completed and signalled.
  - A single-thread endless kernel ran 60 s with no watchdog abort, and its event never signalled until the process was killed.
  - Either way the drain is now bounded: a missing signal costs one timeout and a critical error. The probes are `probe_gpu_error_signal.mm` and `probe_gpu_error_signal2.mm` in the session scratchpad.

**Decision 3, settled later the same day:** a repro showed that a later graph's compile overwrote a cached command graph's barrier lists, but no API could replay a cached command graph. Peter chose to stop caching command graphs, and `zest_BeginCommandGraph` no longer takes a key. See `command_graph_no_cache_implementation.md`.
