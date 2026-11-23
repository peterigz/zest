# Metal M3: Memory and Resources

**Goal:** a Metal frame graph can use transient images and buffers, copy between buffers and between image mips, and the immediate commands that fill, update, clear, blit and resolve work. The resource-management tests then pass on Metal, apart from those that need a shader.

**Architecture:**
- A transient arena backing is one private `MTLBuffer` or one placement `MTLHeap`, as M1's pools are, kept in the device's residency set. A transient image is sized unbound, then placed in its arena's heap at the offset shared code packs it to.
- Copies in a frame graph go through one blit encoder per pass, opened by the pass's first copy and ended when the pass ends, on the queue's fence like every other encoder.
- A clear outside a pass is a render pass with a clear load action and no draws. A scaled blit is a draw with a built-in pipeline whose shaders are MSL text in `zest_metal.h`. A resolve is a render pass that loads the multisampled image and stores with a resolve.
- Nothing here compiles GLSL. shaderc and SPIRV-Cross stay with M4.

**Tech stack:** `zest_metal.h`, `zest-metal-tests.cpp`, `zest-tests.cpp` (the pending list), and two one-line edits to shared tests. No change to `zest.h`, `zest_vulkan.h` or `zest_dx12.h`. No shader files.

**Spec:** `metal_platform_layer_implementation.md`, the M3 row of [Roadmap_] and the rows of [Mapping_] it names. `metal_m2_implementation.md` [Built_], "For M3's plan", and Peter's answers of 2026-11-23 to its four questions.

## Contents

- [Constraints_] Global Constraints
- [Settled_] Settled
- [State_] Current state
- [Probe_] Probe results
- [Decisions_] Decisions for Peter
- [Review_] Review Focus
- [Scope_] Out of scope
- [Phase_1] What M2 left
- [Phase_2] Transient arenas
- [Phase_3] Copies in a frame graph
- [Phase_4] Immediate commands
- [Phase_5] The suite and the close
- [Cost_] Per-frame cost
- [Testing_] Testing
- [Built_] As built

## [Constraints_] Global Constraints

Everything in `metal_m2_implementation.md` [Constraints_] holds: never commit, edit on Windows and build on the Mac over `ssh macmini`, restore the Mac clone at the end of every phase, a script with a backslash is written to a file, every new test is broken on purpose once, follow code-style. What M3 adds:

- **Shared edits.** Only Task 1 touches shared files, and only two tests. After it, build `zest-tests` on Windows in Debug. The Windows suites are Peter's to run.
- **Budget.** Vulkan and D3D12 get no new work. On Metal, a pass that copies takes one blit encoder inside one autorelease pool. A frame graph command allocates nothing from Zest's allocators. See [Cost_].
- **Unretained references.** A frame graph batch's command buffer keeps nothing alive. A command recorded into one may only use objects Zest already holds until the frame in flight comes round: pool buffers, heaps, image textures, views. It never makes a temporary Metal object. Metal needs each of those objects to outlive the batch's completion, which a timeline wait sees to ([Settled_], "Batches stay unretained").
- **Lazily created objects take a lock on creation and none on the read path**, as the design's [Lessons_] asks. That is the blit library, its pipelines and its samplers.
- **Runs open no window** except the two windowed backend tests M2 added and the suite's own.

## [Settled_] Settled

The implementer doesn't reopen these.

- **Peter's answers to M2's questions (2026-11-23).**
  - The two pending tests that only exempt D3D12 change to expect their feature on Vulkan only. First task.
  - `zest_CopyImageToBitmap` on a depth image is fixed in M3, after checking what the other backends do. They disagree, so it is Decision 3.
  - The SDL Metal view is left alone. It is in [Scope_].
  - MoltenVK's layer gets one measurement in the baseline task, and no fix.
- **One image arena category.** `create_transient_image_unbound` reports `ZEST_ARENA_CATEGORY_IMAGE_BASE` for every image. Any texture can be placed in any of the backend's heaps, which is why M1's image pools have one category too (`zest_metal.h:2557`).
- **A transient image's descriptor is rebuilt at bind.** The unbound call keeps only the sample count. Rejected: keeping the `MTLTextureDescriptor` between the two calls, which shared code would have to release on every failure path between them.
- **Copies between image mips follow D3D12's rule, by channel layout.** Metal copies bits between formats with the same channel count and bytes per pixel, and gives wrong bytes with no message between formats that only match in size ([Probe_]). So `copy_image_mip`, and `blit_image_mip` on matching formats, require the same channels and bytes per pixel from `zest_GetFormatPixelData`, and report otherwise.
- **The blit pipeline is built on first use**, not at device creation. The first build costs about 200 ms on a machine that has never compiled it and about 2 ms after ([Probe_]), and most apps never blit.
- **Blit objects outlive a device reset.** The library, pipelines and samplers belong to the `MTLDevice`, which a reset keeps. `cleanup_device_backend` releases them.
- **A resolve keeps its source.** The store action is store and resolve, as Vulkan's resolve leaves the multisampled image intact.
- **Batches stay unretained, and a timeline wait settles them (Peter, 2026-11-23, during Phase 2: whichever is the most performant).**
  - Metal's debug layer needs an object to outlive the completion of every command buffer with unretained references that used it. A shared event signalled last in a buffer is seen before Metal marks the buffer completed, and shared code frees a frame's or a flushed graph's objects straight after the wait on that event. In Metal's default error mode the layer's complaint aborts the process.
  - Each committed batch keeps the last event and value it signals. A successful `zest__mtl_wait_for_timeline` reaps the device's batches, and so does `zest__mtl_wait_for_idle_device`. The reap waits for any batch whose signal has been seen before it reads and releases it.
  - A batch committed while its context holds a drawable also keeps the drawable's texture until it's reaped. The drawable is the one object a batch uses that Zest doesn't free itself: the layer drops it once it has been shown.
  - The device backend keeps its own list of context queues under `committed_lock`, as a wait on one thread must not walk another context's queue maps.
  - Rejected: `commandBuffer`, which retains what it names. Measured on the Apple M2, no debug layer, 20,000 draws a buffer: an indexed draw costs 85 ns unretained and 103 to 120 ns retained, and a draw with a vertex buffer and a texture bound first costs 272 ns against 290 ns.

## [State_] Current state (verified 2026-11-23, Windows tree at 325b2f5)

**What M2 left**
- `zest_metal.h` is 3,457 lines with 50 stubs, all in `Unavailable_stubs` (`zest_metal.h:3384-3450`). M3 replaces 16.
- `zest-tests --platform all` on the Mac: `Dynamic rendering: 129 / 129`, `Legacy render pass: 129 / 129`, `Metal suite: 52 / 52`, `Metal backend: 23 / 23`.
- `metal_pending_tests[]` holds 80 tests in six groups (`zest-tests.cpp:230-317`): 16 transient images, 8 transient buffers, 47 shader compiling, 7 Slang, and the two that only exempt D3D12.
- **Already done in M1, although the roadmap lists them under M3:** storage and uniform buffer entries, storage image entries (`zest_metal.h:2337-2376`) and per-mip views (`zest_metal.h:2710`). "Metal Bindless Slots" reads through every slot. Nothing at the backend level tests per-mip views.

**Transients in shared code**
- `zest__place_transient_resources` returns at once for a graph with no transients (`zest.h:14891-14893`).
- A transient image is created unbound for its size, alignment and category (`zest.h:14955-14965`), and recreated when its offset or backing changes (`zest.h:15113-15121`). Both call `new_frame_graph_image_backend` with a NULL allocator, then `create_transient_image_unbound` with a sample count of 1. A false return frees the backend with `ZEST__FREE(device->allocator, ...)`.
- `bind_image_arena` places it (`zest.h:15130`), then `create_image_view` makes its view (`zest.h:15134`).
- A retired transient image goes through `cleanup_image_backend`, and shared code then frees the backend itself (`zest.h:10206-10211`). `zest__mtl_cleanup_image_backend` already releases the texture and leaves a transient's backend for shared code (`zest_metal.h:2608-2622`).
- `zest__ensure_arena_backing` calls `create_arena_backing(device, context, category, size)` with the size rounded up to 1 MB (`zest.h:14838-14841`, `zest.h:7884`). The categories are 0 for GPU buffers, 1 for CPU buffers, and 2 and up for the backend's own image categories (`zest.h:7878-7880`).
- `free_arena_backing` is called for a backing that grew, a frame in flight later (`zest.h:10225`), and at context teardown (`zest.h:12404`, `zest.h:12415`).
- A transient buffer is a proxy whose `memory_pool` is the backing and whose `memory_offset` is its packed offset (`zest.h:15095-15101`). `zest__mtl_buffer_object` and `zest__mtl_buffer_address` read the pool backend's buffer (`zest_metal.h:2238-2246`), so they work on a backing as they stand.

**The commands**
- Per pass, shared code calls `acquire_barrier`, `begin_render_pass` for a render pass, the callbacks, `end_render_pass`, then `release_barrier` (`zest.h:17571-17689`). A transfer or compute pass has no begin and end hook of its own, so `release_barrier` is where a pass's blit encoder ends.
- `zest_cmd_UploadBuffer` returns false for an uploader with no copies and otherwise calls `upload_buffer` (`zest.h:23112-23117`). `zest_cmd_CopyBuffer` asserts both buffers and the size (`zest.h:23141-23148`).
- `zest_cmd_BlitImageMip` and `zest_cmd_CopyImageMip` assert both nodes are images with equal extents and mip counts (`zest.h:23222`, `zest.h:23234`). `zest_cmd_ImageClear` passes straight through (`zest.h:23265-23267`).
- The six immediate wrappers are at `zest.h:11273-11298`.
- **What each has to do, from Vulkan:**
  - `imm_fill_buffer` fills the whole buffer with a 32-bit value (`zest_vulkan.h:6774`).
  - `imm_update_buffer` copies `min(intended_size rounded up to 4, buffer size, 65536)` bytes (`zest_vulkan.h:6781`).
  - `imm_clear_color_image` clears every mip of every layer (`zest_vulkan.h:6789`). `imm_clear_depth_stencil_image` needs the graphics queue (`zest_vulkan.h:6839`).
  - `imm_blit_image` draws a rect of mip 0, layer 0 into a rect of mip 0, layer 0 with a filter, on the graphics queue (`zest_vulkan.h:6899`). `imm_resolve_image` resolves mip 0, layer 0 (`zest_vulkan.h:7006`).
  - `image_clear` clears mip 0 of layer 0 to zero (`zest_vulkan.h`, `zest__vk_image_clear`).
  - `blit_image_mip` is a nearest blit between mips of the same size, which is a copy when the formats match.
- **Nothing in the repo calls the six immediate functions or `zest_cmd_ImageClear`** outside `zest-d3d12-tests.cpp`. `zest_cmd_BlitImageMip` and `zest_cmd_CopyImageMip` are used by `zest-frame-graph-tests.cpp` and the render targets example, `zest_cmd_CopyBuffer` and `zest_cmd_UploadBuffer` by the layer and stress tests, ImGui, TimelineFX and three examples.

**Metal code M3 builds on**
- A texture gets `MTLTextureUsageRenderTarget` only from the colour or depth attachment flag (`zest_metal.h:2508-2513`).
- An immediate queue opens one blit encoder lazily on its fence (`zest__mtl_queue_blit_encoder`, `zest_metal.h:2031`) and ends it in `zest__mtl_end_queue_encoder` (`zest_metal.h:2042`). Staging buffers pushed to `pending_staging_buffers` are freed when the queue is released (`zest_metal.h:1959`).
- `zest_mtl_command_list_backend_t` has a render encoder and the batch's fence, and no blit encoder (`zest_metal.h:135-144`). `zest_mtl_image_backend_t` holds only the texture (`zest_metal.h:185-187`).
- `zest__mtl_create_image_heap` makes a private, untracked placement heap, adds it to the residency set and counts it (`zest_metal.h:2176`). `zest__mtl_add_buffer_memory_pool` does the same for a pool buffer, with options from `zest__mtl_buffer_options` (`zest_metal.h:2136-2174`).
- Each context has one `MTLRenderPassDescriptor`, which a pass fills and empties again (`zest_metal.h:3116-3176`).
- `zest__mtl_copy_image_to_bitmap` takes its row size from `zest_GetFormatPixelData` (`zest_metal.h:2933`), which has no case for any depth format and gives 0 bytes per pixel (`zest.h:20217`). Vulkan sizes its staging buffer the same way and copies the colour aspect (`zest_vulkan.h:7171-7200`), so it can't read a depth image either. D3D12 reads by placed footprint and can (`zest_dx12.h:6610`).

**Tests**
- The two exemptions: `zest-resource-management-tests.cpp:3377` (`tests->platform != zest_platform_d3d12 && device->max_memory_allocation_count == 0`) and `zest-device-reset-tests.cpp:695` (`expects_extensions = tests->platform != zest_platform_d3d12`). Metal sets the limit to 0 as D3D12 does (`zest_metal.h:1282`, `zest_dx12.h:2445`).
- Helpers in `zest-metal-tests.cpp`: `metal_allocated_size` (35), `metal_read_texture` (89), `metal_create_clear_target` (854), `run_metal_clear_graph` (891), `metal_tests[]` (1556).

**Wiring to mirror, from D3D12**
| What | Where D3D12 does it | Its test |
| --- | --- | --- |
| Arena backing and free | `zest_dx12.h:4096-4149` | `test_d3d12_arena_backing`, `zest-d3d12-tests.cpp:5208` |
| Transient image backend, unbound create, bind | `zest_dx12.h:6494-6545` | `test_d3d12_transient_images`, 5381 |
| Buffer copy and upload in a graph | `zest_dx12.h:7502-7520` | `test_d3d12_frame_graph_buffer_copies`, 4353 |
| Mip blit and copy in a graph | `zest_dx12.h:7420-7468` | `test_d3d12_image_mip_commands`, 4791 |
| Fill and update | `zest_dx12.h:3666-3698` | `test_d3d12_buffer_copies`, 4210 |
| Clears | `zest_dx12.h:3700-3729`, `zest_dx12.h:7532` | 4909, 4972, 5050 |
| Blit | `zest_dx12.h:3731` | `test_d3d12_blit_image`, 4614 |
| Resolve | `zest_dx12.h:3812` | `test_d3d12_resolve_image`, 5105 |
| Per-mip views | | `test_d3d12_per_mip_views`, 3881 |

`zest_dx12.h` is the structural model only.

## [Probe_] Probe results (2026-11-23, Apple M2, Metal API validation on)

`spikes/metal/build/probe/probe_m3.mm` on the Mac, untracked. Metal only, no Zest and no window. Every texture is private, untracked and placed in one placement heap, as Zest's are.

- **Clears by render pass.** A pass with a clear load action and no draws cleared all 4 mips of both layers of an array texture, one pass each, and every one read back right. A depth texture cleared to 0.25 with no colour attachment. A depth and stencil texture cleared both planes in one pass with both attachments set.
- **Render target usage.** A texture is created with the usage for any format, compressed ones included. Using it as an attachment is what fails: BC1, BC7, ASTC and ETC2 each raised "is not color renderable". All 18 uncompressed formats tried cleared with no message, including `R8Snorm`, `RGB9E5Float` and `B5G6R5Unorm`.
- **The blit pipeline.** A library from MSL text and a render pipeline took 207 ms the first time on this machine and 1.9 ms on the next run, with 0.4 ms for a second format.
  - A same-size nearest blit is an exact copy and keeps the top row at the top.
  - A 2:1 linear blit averages each 2x2 block.
  - A source rect drawn into a destination rect through the viewport and scissor lands in the rect and leaves the rest, with a load action of load.
  - A destination without render target usage raises a validation message.
  - An integer source through a `texture2d<float>` raises a validation message naming the data types it would accept.
- **Resolve.** A 4x multisampled texture can be placed in a heap. A pass that loads it and stores with a resolve gave the right pixels, into a destination with render target usage and into one without, with no message either way.
- **Copies between formats.** From `RGBA8Unorm`, a copy into `RGBA8Unorm_sRGB`, `BGRA8Unorm` and `RGBA8Uint` copied the bytes. Into `R32Float` and `RG16Unorm`, the same size per pixel, it raised no message and the destination did not hold the source's bytes. Into `RGBA16Float` it raised "must be compatible".
- **Buffers.** `fillBuffer` takes one byte. It filled an aligned and an unaligned range of a private buffer with no message. A copy between two ranges of one buffer worked.
- **Aliasing.** Two textures of different formats and usages at one heap offset each read back what was last cleared into it.

## [Decisions_] Decisions for Peter

All three were taken on 2026-11-23: Peter went with the recommendations. They are settled, and the implementer doesn't reopen them.

1. **Which images can be render targets.** Clears, blit destinations and resolve sources all need `MTLTextureUsageRenderTarget`, which today only the attachment flags give.
   - **Taken:** the attachment flags as now, plus `zest_image_flag_storage` on an uncompressed colour format. A storage image already carries shader write usage, and it is the other kind of image Vulkan and D3D12 can clear. Anything else given to a clear or as a blit destination reports, naming the flag to add, as D3D12 does.
   - Rejected: attachments only. A storage image then can't be cleared outside a pass on Metal until compute exists.
   - Not offered: every uploaded texture, which D3D12's review narrowed away from for the same commands.
   - Not known: whether the usage costs anything on a storage image on Apple GPUs. The probe can't measure it.
2. **Integer and depth formats in a blit.** The built-in fragment shader samples a `texture2d<float>`, and an integer texture bound to it is a validation error.
   - **Taken:** a documented limit. `zest_imm_BlitImage`, and `zest_cmd_BlitImageMip` between different formats, report for an integer or a depth format on either side. Same-format mip blits are copies and take any format.
   - Rejected: two more fragment functions and a pipeline per kind, about 30 lines, with nothing in the repo to use them.
3. **`zest_CopyImageToBitmap` on a depth image.** Vulkan can't do it and D3D12 can ([State_]).
   - **Taken:** Metal refuses with a validation error, in place of handing Metal a row size of 0.
   - Rejected: support it as D3D12 does, with the depth formats' sizes in a switch in `zest_metal.h`, about 10 lines.
   - As built: the error tells the user to draw the depth into a colour image and read that. The recommendation said "copy the image to a buffer in a pass", and Zest has no public call that does.

**Calls made without Peter (check these)**
- **A fill uses `fillBuffer` when the value's four bytes are equal**, which covers a fill with 0. Any other value copies one staging block of at most 64 KB as many times as it takes, as D3D12's fill does.
- **A clear of a 3D image reports.** A render pass targets one depth plane at a time, and nothing asks for it.
- **A blit between different formats in a frame graph takes single-layer 2D images only.** Anything else needs a temporary view of one slice, which a batch can't keep alive. The immediate blit makes that view, as its command buffer does retain what it uses.
- **A copy command inside a render pass reports** and records nothing. Metal can't open a blit encoder while a render encoder is open, and Vulkan doesn't allow the copy there either.
- **`zest_cmd_ImageClear` inside a render pass reports**, as on D3D12.
- **"Metal Per Mip Views" is added**, although the code is M1's, because the roadmap puts per-mip views in M3 and no backend test covers them.
- **The MoltenVK measurement is a local edit on the Mac**, not code: the runner prints Metal's allocated size around each Vulkan suite for one run.
- **`get_msaa_sample_count` stays a stub.** D3D12's still reports too (`zest_dx12.h:6547`), and shared code only passes a sample count of 1 to a transient.

## [Review_] Review Focus

1. **An encoder is left open.** A blit encoder still open when a render pass begins, or at the commit, makes Metal abort the process (M2: "no pass ended" ended in exit -6). Probe: "Metal Frame Graph Buffer Copies" runs a transfer pass that copies and a render pass in one batch, and a graph that ends on a copy. Break it by not ending the encoder in `release_barrier`.
2. **A transient image is placed over memory another still uses, or a backing is never freed.** Probes: "Metal Transient Images" aliases two transients in one graph and reads back what each pass wrote. "Metal Arena Backing" expects the residency set's `allocationCount`, the device's `memory_allocation_count` and Metal's allocated size back at their baselines after the context goes.
3. **A frame graph command uses a temporary object.** With unretained references it is freed before the GPU reads it. Since Phase 2 the debug layer's "destroyed while still required" record counts as a validation error, so a frame graph test fails on it when the timing shows it. Probe: the frame graph tests run once under `NSZombieEnabled=YES MallocScribble=1`, and the plan's rule is that those commands make no Metal object.
4. **The blit lands flipped, in the wrong rect, or with the wrong filter.** Probe: "Metal Blit Image" repeats the probe's four pixel checks, with a marked top row.
5. **The render target rule is wrong in either direction.** Too narrow raises Metal's "doesn't specify MTLTextureUsageRenderTarget". Too wide gives every texture the usage. Probe: "Metal Render Target Usage" checks the usage of the descriptor for each flag combination, and the clear tests cover an attachment, a storage image and an image that must report.
6. **A copy between formats silently gives wrong bytes.** Probe: "Metal Image Mip Commands" copies between two formats of one layout and expects the bytes, and between `RGBA8Unorm` and `R32Float` and expects the report and an untouched destination.
7. **The two shared test edits change a Windows result.** Probe: Peter's Windows run after Phase 1.

## [Scope_] Out of scope

- Compiling GLSL, pipelines, draws, dispatch, push constants, `insert_compute_image_barrier` and the three immediate compute functions. M4.
- Multisampled transients and `get_msaa_sample_count`.
- Graphs that cross queues, and the aliasing order between queues. M5.
- Integer, depth and 3D cases named under [Decisions_].
- Destroying the SDL Metal view with its window (Peter, 2026-11-23). It goes in M7's docs as a known limit.
- A fix for anything the MoltenVK measurement shows. `zest_vulkan.h` is not edited. The numbers go to Peter.
- GPU timestamps, markers, device loss, user docs. M7.

## [Phase_1] What M2 left

### Task 1: Baseline, the two shared tests, depth readback, MoltenVK

**Files:** `zest-resource-management-tests.cpp:3377`, `zest-device-reset-tests.cpp:695`, `zest-tests.cpp:313-316`, `zest_metal.h:2933`.

**Interfaces:**
- `zest-resource-management-tests.cpp:3377` becomes `tests->platform == zest_platform_vulkan && device->max_memory_allocation_count == 0`, and its comment says only Vulkan has an allocation count limit.
- `zest-device-reset-tests.cpp:695` becomes `expects_extensions = tests->platform == zest_platform_vulkan`, and its comment says only a Vulkan device has instance extensions.
- `zest__mtl_copy_image_to_bitmap` handles a depth format as Decision 3 settles.

**Steps:**
- [x] Mac baseline before any edit, Metal on: `zest-tests --platform all`. Expected: 129 / 129, 129 / 129, `Metal suite: 52 / 52`, `Metal backend: 23 / 23`, exit 0, no VUID line.
- [x] Make the two test edits. Build `zest-tests` on Windows in Debug. Expected: no warning, no error.
- [x] Run each of the two tests alone on Metal with the pending check off. One that passes comes off `metal_pending_tests[]`. One that fails on something else stays, under a comment naming what it needs, and the finding goes in the report.
- [x] Decision 3. Test "Metal Depth Readback": the call returns what the decision says for a `d32_sfloat` image, and a colour image still reads back.
- [x] MoltenVK measurement, by a local edit on the Mac that is not kept: print `[MTLCreateSystemDefaultDevice() currentAllocatedSize]` before the first Vulkan suite and after each, in one `--platform vulkan` run. Record the three numbers.
- [x] Build and run `--platform all`. Expected: the Vulkan lines at the baseline, the Metal suite at 52 plus whichever of the two came off, `Metal backend: 24 / 24`.
- [x] Phase report, with the shared edits listed for Peter's Windows run. Restore the Mac clone.

## [Phase_2] Transient arenas

### Task 2: Arena backings and transient buffers

**Files:** `zest_metal.h` sections `Buffer_and_memory`, `Unavailable_stubs`.

**Interfaces:**
- **`create_arena_backing(device, context, category, size)`** allocates the pool and its backend from `context->allocator`, with the magic, device and size set.
  - `ZEST_ARENA_CATEGORY_GPU_BUFFERS`: a private buffer, through `zest__mtl_buffer_options(zest_memory_property_device_local_bit)`.
  - `ZEST_ARENA_CATEGORY_CPU_BUFFERS`: a shared buffer, through the host visible and coherent flags, with `pool->mapped` set to its `contents`.
  - Both: added to the residency set, counted in `memory_allocation_count`, `pool->alignment` set to `ZEST_BUFFER_OFFSET_GRANULARITY`.
  - Any other category: `zest__mtl_create_image_heap`.
  - A failure frees what was made and returns NULL.
- **`free_arena_backing(device, context, backing)`** removes the buffer or heap from the residency set, releases it, takes it off the count, and frees the backend and the pool.

**Steps:**
- [x] Implement, replacing the two stubs.
- [x] Test "Metal Arena Backing", mirroring `zest-d3d12-tests.cpp:5208`: on a headless context, create and free a backing of each of the three kinds. While one exists the residency set's `allocationCount` and `memory_allocation_count` are one higher. A CPU backing's `mapped` takes a write and reads it back. After the context is destroyed, both counts and `metal_allocated_size()` equal their values from before.
- [x] Run each test of the pending "Transient buffers" group alone with the pending check off. One that passes with no Metal validation record comes off the list. Record what each of the others stops at.
- [x] Break it: skip the residency removal in `free_arena_backing` (the count check must fail), and never set `mapped` (the write must fail or crash inside the test).
- [x] Build and run `--platform metal`, then with `--shader-validation`.

### Task 3: Transient images

**Files:** `zest_metal.h` sections `Backend_structs`, `Images`, `Unavailable_stubs`.

**Interfaces:**
- `zest_mtl_image_backend_t` gains `zest_uint sample_count;`.
- **`new_frame_graph_image_backend(device, allocator, image, imported_image)`** allocates the backend from the linear allocator when there is one and from `device->allocator` otherwise, zeroes it, sets `image->backend` and returns it.
- **`create_transient_image_unbound`**:
  - Reports and returns false for `zest_image_flag_host_visible`, and for a format with no Metal form, with the wording `zest__mtl_create_image` uses (`zest_metal.h:2534-2541`).
  - Builds the descriptor with `zest__mtl_new_texture_descriptor`, asks `heapTextureSizeAndAlignWithDescriptor:`, releases the descriptor.
  - `info->size` is the size rounded up to the alignment, `info->alignment` the alignment, `info->category` `ZEST_ARENA_CATEGORY_IMAGE_BASE`. Keeps `num_samples` in the backend.
- **`bind_image_arena(device, image, backing, offset)`** rebuilds the descriptor from the image, its `info.layer_count`, the kept sample count and `info.flags`, places the texture in the backing's heap at the offset, and sets `image->layout` to undefined. A nil texture reports and returns false.

**Steps:**
- [x] Implement, replacing the three stubs.
- [x] Test "Metal Transient Images", mirroring `zest-d3d12-tests.cpp:5381`, on a headless context with command graphs:
  - A graph with one transient colour target cleared in a pass, and a transfer pass that copies it to an imported image with `zest_cmd_CopyImageMip`. Until Task 5 that copy reports, so this step first checks only that the graph compiles, places and flushes with 0 validation errors. Task 5 adds the readback.
  - The same graph run 6 times: the transient's texture is the same object from the second run on, and `metal_allocated_size()` after the sixth equals the fourth.
  - A graph with two transients of different sizes used in passes that don't overlap: both land in one arena, and the second run reuses both textures.
  - A transient with the CPU transfer hint: the graph reports a transient resource failure and one report is recorded.
- [x] Run each test of the pending "Transient images" group alone with the pending check off. One that passes comes off the list. Record what each of the others stops at.
- [x] Break it: place every transient at offset 0 (the reuse or the readback must fail once Task 5 lands, so this one is rerun there), and return a category that differs per format (the one-arena check must fail).
- [x] Build and run `--platform metal`, then with `--shader-validation`.
- [x] Phase report. Restore the Mac clone.

## [Phase_3] Copies in a frame graph

### Task 4: The pass's blit encoder, buffer copies and uploads

**Files:** `zest_metal.h` sections `Backend_structs`, `Internal_Frame_graph_context_functions`, `Frame_graph_context_functions`, `Unavailable_stubs`.

**Interfaces:**
- `zest_mtl_command_list_backend_t` gains `id<MTLBlitCommandEncoder> blit_encoder;`, open from a pass's first copy until the pass ends.
- `id<MTLBlitCommandEncoder> zest__mtl_pass_blit_encoder(const zest_command_list command_list);` returns the open encoder, or opens one inside an autorelease pool, retains it and waits on the batch's fence. With a render encoder open it logs a validation error naming the command's rule and returns nil.
- `void zest__mtl_end_pass_blit_encoder(const zest_command_list command_list);` does nothing without one. Otherwise it updates the fence, ends the encoder and releases it.
- `release_barrier`, `begin_render_pass` and `end_command_buffer` call `zest__mtl_end_pass_blit_encoder` first.
- **`copy_buffer(command_list, staging_buffer, device_buffer, size)`** copies from one buffer's `memory_offset` to the other's.
- **`upload_buffer(command_list, uploader)`** copies each entry of `uploader->buffer_copies`, then clears the list, the flags and both buffer pointers, and returns true, as D3D12's does (`zest_dx12.h:7506-7520`). Without an encoder it returns false and leaves the uploader as it was.

**Steps:**
- [ ] Implement, replacing the two stubs.
- [ ] Test "Metal Frame Graph Buffer Copies", mirroring `zest-d3d12-tests.cpp:4353`, on a headless context:
  - A transfer pass copies a staging buffer of known bytes into a device buffer with `zest_cmd_CopyBuffer`, and a second transfer pass copies that into a readback buffer. The bytes match.
  - The same through `zest_cmd_UploadBuffer` with three regions at different offsets. Afterwards the uploader's list is empty.
  - A graph with a transfer pass that copies followed by a render pass that clears an imported image, in one batch. Both results are right and there are 0 validation errors.
  - A copy called from a render pass callback: one validation error, which the test resets, and the destination is untouched.
- [ ] Break each once: don't end the encoder in `release_barrier` (the copy-then-render graph must abort or report), drop the fence wait, which can't fail yet and is recorded as such, and copy from offset 0 in place of `memory_offset` (the bytes must differ).
- [ ] Build and run `--platform metal`, then with `--shader-validation`.

### Task 5: Mip copies, and mip blits between matching formats

**Files:** `zest_metal.h` sections `Type_converters`, `Frame_graph_context_functions`, `Unavailable_stubs`.

**Interfaces:**
- `zest_bool zest__mtl_formats_copy_compatible(zest_format source, zest_format destination);` in `Type_converters`: true when `zest_GetFormatPixelData` gives both the same channel count and bytes per pixel, and for two equal formats of any kind.
- `void zest__mtl_record_mip_copy(const zest_command_list command_list, zest_resource_node src, zest_resource_node dst, zest_uint mip);` copies every layer of one mip between the two nodes' textures through the pass's blit encoder, at the mip's size.
- **`copy_image_mip`** reports through `zest__mtl_report_unavailable` when the formats aren't copy compatible, naming `zest_cmd_BlitImageMip` as the way to convert, and otherwise records the copy.
- **`blit_image_mip`** records the copy when the formats are copy compatible. Otherwise it reports that a blit between formats isn't available yet. Task 8 replaces that report.

**Steps:**
- [ ] Implement, replacing the two stubs.
- [ ] Pure checks added to "Metal Format Table": `r8g8b8a8_unorm` is copy compatible with its sRGB form, with `b8g8r8a8_unorm` and with `r8g8b8a8_uint`, and not with `r32_sfloat`, `r16g16_unorm` or `r16g16b16a16_sfloat`.
- [ ] Test "Metal Image Mip Commands", mirroring `zest-d3d12-tests.cpp:4791`: two 16x16 images with 3 mips, each mip of the source cleared to its own colour by render passes.
  - A transfer pass copies mip 1 with `zest_cmd_CopyImageMip`. Mip 1 of the destination reads back as the source's and mips 0 and 2 are untouched.
  - The same with `zest_cmd_BlitImageMip` on mip 2.
  - A copy from `r8g8b8a8_unorm` into `r8g8b8a8_srgb`: the bytes match.
  - A copy from `r8g8b8a8_unorm` into `r32_sfloat`: one validation error, and the destination is untouched.
- [ ] Finish "Metal Transient Images": the copied transient reads back as the colour its pass cleared it to, on every one of the 6 runs. Rerun Task 3's "offset 0" breakage, which must now fail.
- [ ] Break it: copy mip 0 whatever mip was asked for, and skip the compatibility check (the `r32_sfloat` case must fail).
- [ ] Build and run `--platform metal`, with `--shader-validation`, and once with `NSZombieEnabled=YES MallocScribble=1`. Expected: no zombie report.
- [ ] Phase report. Restore the Mac clone.

## [Phase_4] Immediate commands

### Task 6: Fill and update

**Files:** `zest_metal.h` sections `Backend_structs`, `Command_pools`, `Unavailable_stubs`.

**Interfaces:**
- `#define ZEST__MTL_FILL_BLOCK_SIZE 65536` beside the other backend defines, with its unit in a trailing comment.
- **`imm_fill_buffer(queue, buffer, value)`** does nothing for a buffer of size 0. When the value's four bytes are equal it is one `fillBuffer:range:value:` over the buffer's range. Otherwise it fills a staging block of `min(buffer size, ZEST__MTL_FILL_BLOCK_SIZE)` with the value, pushes it to the queue's `pending_staging_buffers`, and copies it over the buffer block by block.
- **`imm_update_buffer(queue, buffer, data, intended_size)`** copies `min(intended_size rounded up to 4, buffer size, 65536)` bytes through a staging buffer, with the bytes past `intended_size` zeroed, as D3D12's does (`zest_dx12.h:3685-3698`).

**Steps:**
- [ ] Implement, replacing the two stubs.
- [ ] Test "Metal Buffer Fills", mirroring the fill and update parts of `zest-d3d12-tests.cpp:4210`: a device buffer of 200,000 bytes.
  - Filled with 0, then with 0xABABABAB, then with 0x11223344. Each time a copy to a readback buffer holds the value in every word, including the last block, which is shorter than 64 KB.
  - A buffer whose size isn't a multiple of 4 filled with 0x11223344: the tail bytes are the value's first bytes.
  - An update of 10 bytes: the first 10 match, bytes 10 and 11 are 0, and byte 12 is what the fill left.
  - An update with an `intended_size` above 65,536: only 65,536 bytes change.
- [ ] Break it: always take the `fillBuffer` path (the 0x11223344 fill must fail), and drop the round up to 4 (bytes 10 and 11 must fail).
- [ ] Build and run `--platform metal`, then with `--shader-validation`.

### Task 7: Render target usage and clears

**Files:** `zest_metal.h` sections `Images`, `Command_pools`, `Frame_graph_context_functions`, `Unavailable_stubs`.

**Interfaces:**
- `zest__mtl_new_texture_descriptor` gives the usage Decision 1 settles.
- `zest_bool zest__mtl_image_can_be_target(zest_image image);` is true for an image whose texture carries the render target usage and whose format isn't block compressed.
- `zest_bool zest__mtl_record_clear(zest_device device, id<MTLCommandBuffer> command_buffer, id<MTLFence> fence, MTLRenderPassDescriptor *descriptor, zest_image image, zest_clear_value_t clear_value, zest_uint mip_count, zest_uint layer_count);`
  - One render pass per mip per layer, with a clear load action, a store action of store and no draws. Each encoder waits on the fence and updates it.
  - A depth format sets the depth attachment, and the stencil attachment too when the format has stencil.
  - It empties every attachment it filled before it returns.
  - It reports and returns false for an image that can't be a target, naming the flag to add, and for a 3D image.
- **`imm_clear_color_image`** and **`imm_clear_depth_stencil_image`** end the queue's blit encoder, then call it for every mip and layer with a descriptor made in their own autorelease pool. The depth one reports on a queue that isn't a graphics queue, as Vulkan's does.
- **`image_clear(command_list, image)`** reports with a render encoder open. Otherwise it ends the pass's blit encoder and clears mip 0 of layer 0 to zero with the context's descriptor.

**Steps:**
- [ ] Implement, replacing the three stubs.
- [ ] Pure test "Metal Render Target Usage": the descriptor's usage for a sampled texture, a colour attachment, a depth attachment, a storage image and a compressed texture, as Decision 1 gives them.
- [ ] Test "Metal Clear Colour Image", mirroring `zest-d3d12-tests.cpp:4909`: a 16x16 colour attachment with 3 mips and 2 layers cleared to (0.2, 0.4, 0.6, 1). Every mip of both layers reads back (51, 102, 153, 255). Then a storage image, if Decision 1 takes it. Then a plain sampled texture: false, one validation error, and the report names the flag.
- [ ] Test "Metal Clear Depth Image", mirroring 4972: a `d32_sfloat` attachment cleared to 0.25 reads back 0.25 everywhere. On a compute queue: false, one validation error, and the image still holds 0.25. A `d32_sfloat_s8_uint` attachment cleared to 0.5 and 7 reads back both.
- [ ] Test "Metal Image Clear Command", mirroring 5050: an image cleared to a colour, then `zest_cmd_ImageClear` in a transfer pass after a copy in the same pass. Mip 0 reads back zero, mip 1 keeps its colour, and the copy's result is right. The same call from a render pass callback: false and one validation error.
- [ ] Break each once: clear mip 0 only (the mip check must fail), leave the descriptor's texture set (the freed target's owner count from M2's "Metal Render Pass Clear" pattern must fail), skip the target check (Metal's own validation message must fail the plain texture case).
- [ ] Build and run `--platform metal`, then with `--shader-validation`.

### Task 8: The blit pipeline

**Files:** `zest_metal.h` sections `Shader_code`, `Backend_structs`, `Backend_cleanup_functions`, `Images`, `Frame_graph_context_functions`, `Unavailable_stubs`.

**Interfaces:**
- `Shader_code` holds the MSL as one string, `zest__mtl_blit_shader_source`: a vertex function that makes a full-target triangle from the vertex id and maps it to a source rect, and a fragment function that samples a `texture2d<float>` at a mip level. Both the rect and the level come from one 32-byte block set with `setVertexBytes` and `setFragmentBytes`.
- `#define ZEST__MTL_MAX_BLIT_PIPELINES 16`
- `typedef struct zest_mtl_blit_pipeline_t { MTLPixelFormat pixel_format; id<MTLRenderPipelineState> pipeline; } zest_mtl_blit_pipeline_t;`
- `zest_mtl_device_backend_t` gains `id<MTLLibrary> blit_library;`, `zest_mtl_blit_pipeline_t blit_pipelines[ZEST__MTL_MAX_BLIT_PIPELINES];`, `volatile int blit_pipeline_count;`, `id<MTLSamplerState> blit_samplers[2];` (nearest, linear) and `pthread_mutex_t blit_lock;`.
- `id<MTLRenderPipelineState> zest__mtl_blit_pipeline(zest_device device, MTLPixelFormat pixel_format);`
  - Looks through the first `blit_pipeline_count` entries with no lock.
  - On a miss it takes `blit_lock`, looks again, builds the library and the samplers if they're missing and then the pipeline, writes the entry, and raises the count last.
  - A compile or pipeline error goes to the log and a report with Metal's text, and returns nil. So does a seventeenth format.
- `zest_bool zest__mtl_blit_formats_are_valid(zest_device device, zest_image source, zest_image destination);` reports and returns false for an integer or depth format on either side (Decision 2) and for a destination that can't be a target.
- `void zest__mtl_record_blit(id<MTLCommandBuffer> command_buffer, id<MTLFence> fence, MTLRenderPassDescriptor *descriptor, id<MTLRenderPipelineState> pipeline, id<MTLSamplerState> sampler, id<MTLTexture> source, zest_uint source_mip, const float source_rect[4], id<MTLTexture> destination, zest_uint destination_mip, MTLViewport viewport, MTLScissorRect scissor);` is one render pass with a load action of load and one draw of 3 vertices, on the fence.
- **`imm_blit_image`** reports off a graphics queue. It ends the queue's blit encoder, turns the source rect into 0 to 1 coordinates of mip 0 and the destination rect into a viewport and a clamped scissor, and records the blit. A source that isn't a plain 2D texture is sampled through a 2D view of mip 0, slice 0 made for the call.
- **`blit_image_mip`** between formats that aren't copy compatible: reports with a render encoder open, for a source or destination with more than one layer, and for formats `zest__mtl_blit_formats_are_valid` refuses. Otherwise it ends the pass's blit encoder and records a blit of the whole mip with the nearest sampler and the context's descriptor.
- `cleanup_device_backend` releases the pipelines, the samplers and the library, and destroys the lock.

**Steps:**
- [ ] Implement, replacing `imm_blit_image` and the report Task 5 left in `blit_image_mip`.
- [ ] Test "Metal Blit Image", mirroring `zest-d3d12-tests.cpp:4614`, with a 16x16 source whose left half is red, right half blue and top row green:
  - Whole to whole, nearest: an exact copy.
  - Whole into a 16x16 destination's 8x8 region, linear: the top row is half green.
  - The right half into a 4x4 rect at (6, 2), nearest, over a destination cleared to green: blue inside the rect, green outside.
  - A destination rect that hangs off the destination: only the part inside is written, with 0 validation errors.
  - Into a plain sampled texture, and from an `r8g8b8a8_uint` source: false and one validation error each.
  - Into a second destination format: `blit_pipeline_count` is 2. The first format again: still 2.
  - A device reset between two blits: the second still works and the count is unchanged.
- [ ] Extend "Metal Image Mip Commands": `zest_cmd_BlitImageMip` from `r8g8b8a8_unorm` into an `r16g16b16a16_sfloat` colour attachment in a transfer pass. The destination's mip reads back as the source's colour, converted.
- [ ] Break each once: flip the V coordinate (the top row check must fail), use the linear sampler for nearest (the exact copy must fail), skip the scissor clamp (Metal's message must fail the overhang case), build a pipeline on every call (the count check must fail).
- [ ] Build and run `--platform metal`, then with `--shader-validation`.

### Task 9: Resolve, and per-mip views

**Files:** `zest_metal.h` sections `Command_pools`, `Unavailable_stubs`. `zest-metal-tests.cpp`.

**Interfaces:**
- **`imm_resolve_image(queue, src_image, dst_image)`**:
  - Reports and returns false off a graphics queue, for a source whose texture has a sample count of 1, for a destination with more, for formats that differ, and for a source that can't be a target.
  - Ends the queue's blit encoder, then records one render pass with the source as colour attachment 0, the destination as its resolve texture, a load action of load, a store action of store and resolve, and no draws.

**Steps:**
- [ ] Implement, replacing the stub.
- [ ] Test "Metal Resolve Image", mirroring `zest-d3d12-tests.cpp:5105`: a 4x multisampled 16x16 colour attachment cleared to (0.2, 0.4, 0.6, 1) and resolved into a single-sampled texture, which reads back (51, 102, 153, 255). A single-sampled source, a multisampled destination and a compute queue each return false with one validation error.
- [ ] Test "Metal Per Mip Views", mirroring `zest-d3d12-tests.cpp:3881`: a 16x16 colour attachment with 4 mips, each cleared to its own colour, and `zest_CreateImageViewsPerMip`. Each view's texture reads back its own mip's colour at that mip's size.
- [ ] Break it: store without the resolve (the readback must fail), and give every per-mip view mip 0 (the per-mip check must fail).
- [ ] Build and run `--platform metal`, with `--shader-validation`, and with zombies and `MallocScribble`.
- [ ] Phase report. Restore the Mac clone.

## [Phase_5] The suite and the close

### Task 10: The pending list, and the final check

**Files:** `zest-tests.cpp:230-317`. `metal_m3_implementation.md`, `metal_platform_layer_implementation.md`.

**Steps:**
- [ ] Sweep every test still on the pending list, each alone in its own process with the pending check off. One that passes with no Metal validation record comes off. Regroup what stays under what the sweep saw it stop at. A test with its own context create info is judged in the whole suite, as `--test` runs the first test it matches on the default context ([Built_], Phase 2).
- [ ] Check the done line: every test in `zest-resource-management-tests.cpp` runs on Metal except those the sweep shows need a shader. List them by name.
- [ ] The final check on the Mac: every target with Metal on and with Metal off, `zest-objc-compile-check`, `--platform all`, `--platform metal --shader-validation`, one zombie and `MallocScribble` run, and `zest-clear-screen --metal --validate --frames 240`. Expected: the Vulkan lines at the baseline, both Metal lines full, `validation errors: 0`.
- [ ] Build `zest-tests` and `zest-c-compile-check` on Windows in Debug. Expected: clean.
- [ ] Add `## As built (<date>)` to this file: the suite count and the pending groups mapped to milestones, the MoltenVK numbers, every difference from the plan, the checks that can't fail yet, and the shared edits. Add M3's part to [Built_] in the design document.
- [ ] Hand Peter the Windows check, and the questions M4's plan needs answered. Restore the Mac clone.

## [Cost_] Per-frame cost

| Path | Vulkan and D3D12 | Metal after M3 |
| --- | --- | --- |
| Per frame, a graph with transients | Unchanged | Nothing once placed: a cached graph's images and backings are reused. A transient that moves is one texture placed in a heap and one view |
| Per pass that copies | Unchanged | One blit encoder inside one autorelease pool, one fence wait and one update |
| Per copy command | Unchanged | One encoder call per region. No allocation, no lock |
| `zest_cmd_ImageClear`, a mip blit between formats | Unchanged | One render pass each, on the context's descriptor. The blit adds one array search of at most 16 entries with no lock |
| A graph with none of these | Unchanged | One pointer test in `release_barrier` and `end_command_buffer` |
| Per batch, from Phase 2 | Unchanged | Two takes of the device's committed lock, at the batch's start and at its commit, and one retain and release each of its ordering event and, in a frame, the drawable's texture. Nothing per draw |
| Per successful timeline wait, from Phase 2 | Unchanged | One walk of the device's context queues under the lock. A batch whose signal has been seen and isn't marked completed yet is waited for, which takes microseconds |
| Immediate commands | Unchanged | One render pass per mip per layer for a clear. A fill of a mixed value and an update each take one staging buffer |
| First blit to a format | | The library and pipeline build, under the lock: about 200 ms the first time ever on a machine, about 2 ms after |

**Memory:** `zest_mtl_image_backend_t` grows by 4 bytes, the command list backend by one pointer. The device backend gains one library, two samplers, a mutex and 16 format and pipeline pairs. An arena backing is one buffer or heap in the residency set. No struct in `zest.h` grows. From Phase 2: a committed batch is 32 bytes where it was 8, each context queue backend gains 24 bytes, and the device backend a mutex and a pointer.

## [Testing_] Testing

**Automated, on the Mac (implementer)**
- After every task: build `zest-tests`, `zest-c-compile-check` and `zest-objc-compile-check` with Metal on, then `zest-tests --platform metal` and `--platform metal --shader-validation`.
- At the end of every phase: `zest-tests --platform all`, and a Metal off build with `zest-tests`. Expected: the Vulkan lines at Task 1's baseline.
- At the end of Phases 3, 4 and 5: `NSZombieEnabled=YES MallocScribble=1 zest-tests --platform metal`.
- New backend tests, taking `metal_tests[]` from 23 to 36: Depth Readback, Arena Backing, Transient Images, Frame Graph Buffer Copies, Image Mip Commands, Buffer Fills, Render Target Usage, Clear Colour Image, Clear Depth Image, Image Clear Command, Blit Image, Resolve Image, Per Mip Views.
- The Metal suite: 52, plus the two from Task 1 if they pass, plus what each sweep takes off the list. The final count is recorded, not predicted.

**Peter's test, on Windows, after Phase 1**
- `zest-tests.exe` with the default `--platform all`. Expected: `Dynamic rendering: 136 / 136`, `Legacy render pass: 136 / 136`, `D3D12 suite: 132 / 132`, `D3D12 backend: 103 / 103`, exit 0. The two edited tests are "Resource Test Pooled Image Allocations" and "Device Reset Pool Sizing".

**Visual checks**
- None. M3 adds nothing that reaches the screen. `zest-clear-screen --metal` should look as it did after M2.

## [Built_] As built (2026-11-23)

One part per phase, in the order they were built.

### Phase 1: what M2 left (2026-11-23, on top of commit 325b2f5, built and run on the Mac over SSH)

**What changed**
- `zest-resource-management-tests.cpp`, "Resource Test Pooled Image Allocations": the allocation count limit is expected on Vulkan only.
- `zest-device-reset-tests.cpp`, "Device Reset Pool Sizing": instance extensions are expected on Vulkan only.
- `zest-tests.cpp`: both tests are off `metal_pending_tests[]`, which now holds 78 tests in four groups. The edit is inside `#ifdef ZEST_ENABLE_METAL`.
- `zest_metal.h`, `zest__mtl_copy_image_to_bitmap`: a depth or stencil format is refused through `ZEST_ASSERT_OR_VALIDATE` before anything is created. 50 stubs remain.
- `zest-metal-tests.cpp`: "Metal Depth Readback".

**Results**
- Mac baseline before any edit, Metal on: `Dynamic rendering: 129 / 129`, `Legacy render pass: 129 / 129`, `Metal suite: 52 / 52`, `Metal backend: 23 / 23`, exit 0, no VUID line.
- Each of the two tests run alone on Metal with the pending check off: passed, with no missing member and no Metal validation record.
- Mac at the end of the phase, Metal on:
  - `zest-tests`, `zest-c-compile-check` and `zest-objc-compile-check` build with no warning from Zest's files.
  - `zest-tests --platform all`: `Dynamic rendering: 129 / 129`, `Legacy render pass: 129 / 129`, `Metal suite: 54 / 54`, `Metal backend: 24 / 24`, exit 0, no VUID line.
  - `zest-tests --platform metal`, and again with `--shader-validation`: `Metal suite: 54 / 54`, `Metal backend: 24 / 24`, no Metal validation record in the suite's part of the log.
- Mac, Metal off: `zest-tests` and `zest-c-compile-check` build with no warning, and `zest-tests` gives 129 / 129 and 129 / 129, exit 0, no VUID line.
- Windows: `zest-tests` and `zest-c-compile-check` build in Debug with no warning and no error, after the two shared edits and again at the end of the phase. The suites were not run.

**The new check was broken on purpose and failed** (`spikes/metal/m3_task1_mutate.py` on the Mac, untracked)
- With the refusal taken out of `zest__mtl_copy_image_to_bitmap`, "Metal Depth Readback" failed and the run gave `Metal backend: 23 / 24`.

**The MoltenVK measurement** (`spikes/metal/m3_moltenvk_measure.py` on the Mac, a local edit that was put back)
- Metal's `currentAllocatedSize` for the process in one `--platform vulkan` run, in bytes: 65,536 before the first suite, 249,610,240 after the dynamic rendering suite, 496,304,128 after the legacy render pass suite. A second run gave 65,536, 248,578,048 and 496,304,128.
- So each Vulkan suite leaves about 247 MB allocated on the Metal device after its Zest device is destroyed. A CAMetalLayer's drawables at 1280x768 are about 12 MB, so this is far more than the layer M2 asked about.
- Not known: whether MoltenVK holds the memory or it sits in autoreleased objects, as the runner's main thread never drains an autorelease pool. Nothing was changed. The Metal backend's own tests return to their baseline once the device goes.

**Differences from the plan**
- Decision 3's error tells the user to draw the depth into a colour image and read that. The recommendation's wording, "copy the image to a buffer in a pass", names something Zest has no public call for.
- The refusal goes through `ZEST_ASSERT_OR_VALIDATE`, shared code's form for a call the user got wrong: a validation error in a test build and an assert otherwise. `zest_imm_GenerateMipMaps` on a compressed format is refused the same way in `zest.h`.
- "Metal Depth Readback" checks `d32_sfloat_s8_uint` as well as `d32_sfloat`.

**Shared edits, for Peter's Windows run**
- `zest-resource-management-tests.cpp`: one condition and its comment.
- `zest-device-reset-tests.cpp`: one condition and its comment.

### Phase 2: transient arenas (2026-11-23, on top of commit fc6e870, built and run on the Mac over SSH)

**What changed**
- `zest_metal.h`:
  - `zest__mtl_create_arena_backing` and `zest__mtl_free_arena_backing`, in `Buffer_and_memory`.
  - `zest__mtl_new_frame_graph_image_backend`, `zest__mtl_create_transient_image_unbound` and `zest__mtl_bind_image_arena`, in `Images`. `zest_mtl_image_backend_t` gains `sample_count`. 45 stubs remain.
  - `zest_mtl_committed_batch_t`, `zest__mtl_push_committed_batch` and `zest__mtl_reap_queue_batches`, and the device's `committed_lock` and list of context queues ([Settled_], "Batches stay unretained").
  - `zest__mtl_wait_for_timeline` reaps the device's batches when its wait succeeds, and `zest__mtl_wait_for_idle_device` when it's done.
  - `zest__mtl_frame_drawable_texture`, which gives each committed batch the drawable texture it holds.
  - Both commits of a batch are inside an autorelease pool.
  - The validation reader counts the debug layer's "being destroyed while still required" record, which has no title ending in Validation (`ZEST__MTL_DESTROYED_IN_USE_MESSAGE`).
- `zest-metal-tests.cpp`: "Metal Arena Backing" and "Metal Transient Images". `metal_read_native_texture`, which `metal_read_texture` now calls.
- `zest-tests.cpp`: 17 tests are off `metal_pending_tests[]`, which now holds 61 in three groups.
- `metal_platform_layer_implementation.md`: the Command pools row of [Mapping_] says what a timeline wait does.

**The lifetime problem transient images exposed**
- With only Zest holding a transient's texture, Metal's debug layer said "The following Metal object is being destroyed while still required to be alive by the command buffer" on every flushed command graph of a headless context, 8 of 8 in each of three runs.
- The rule, from `spikes/metal/build/probe/probe_unretained.mm`, 200 repeats each: an object destroyed once its buffer's `waitUntilCompleted` has returned raises nothing. One destroyed once the event the buffer signals last has been seen raises the message, and without `MTL_DEBUG_LAYER_ERROR_MODE=nslog` the process aborts.
- M2's reader didn't count the message, so no M2 test could fail on it.
- The cost of the alternative, from `probe_retained_cost.mm`, is in [Settled_].
- Peter chose between the two fixes during the phase: the most performant, which is the one built.
- **The same message for a drawable's texture.** Once the reader counted it, "Metal Windowed Context" failed on it in 2 of about 45 runs of the Metal suites. With every command buffer labelled for one run, the buffer was a frame graph batch of a context's second frame, and the object a 320 wide drawable texture.
  - Not reproduced by 600 context cycles on one window, 600 window resizes or 120 runs of the test on new windows, so what makes the layer drop that drawable early is not known. It has only been seen in the first "Metal Windowed Context" after "Metal Windowed Frames".
  - Each batch committed in a frame now holds the drawable's texture until it's reaped, so the texture can't be destroyed before the batch has completed whatever the layer does.
  - The message hasn't come back in 48 runs since. At 2 in 45 that would happen by chance about one time in nine, so the runs alone don't prove the fix. The argument does: Metal can't destroy an object a batch still holds.

**A commit outside an autorelease pool**
- With `--shader-validation`, one arena heap of 1 MB stayed allocated after the context of "Metal Transient Images" had gone, with the residency set and the allocation count back at their baselines.
- Putting a graph's execution inside a pool cleared it, and of five backend call sites tried one at a time only the commit did (`spikes/metal/m3_sv_experiment2.py` and `m3_sv_experiment3.py`). With GPU validation on, `[command_buffer commit]` leaves an autoreleased object that holds what the batch used, and a thread with no pool never lets it go.
- It is M2's code. No M2 check could show it, as no heap was freed while its device lived.

**Results**
- Mac at the end of the phase, Metal on:
  - `zest-tests`, `zest-c-compile-check` and `zest-objc-compile-check` build with no warning from Zest's files.
  - `zest-tests --platform all`: `Dynamic rendering: 129 / 129`, `Legacy render pass: 129 / 129`, `Metal suite: 71 / 71`, `Metal backend: 26 / 26`, exit 0, no VUID line.
  - `zest-tests --platform metal` 28 times, once with `--shader-validation` and once with `NSZombieEnabled=YES MallocScribble=1`: 71 / 71 and 26 / 26 every time, with no Metal validation record in the suite's part of the log, no destroyed in use message and no zombie report.
- Mac, Metal off: `zest-tests` and `zest-c-compile-check` build with no warning, and `zest-tests` gives 129 / 129 and 129 / 129, exit 0, no VUID line.
- Windows: `zest-tests` and `zest-c-compile-check` build in Debug with no warning and no error. The suites were not run.

**The pending list** (`spikes/metal/sweep_pending.py`, each test alone with the pending check off)
- "Transient buffers" group, 8 tests: "User Test Buffer Output In Render Pass" passed and came off. The other 7 stop at `copy_buffer`, three of them at `upload_buffer` as well. The group is renamed "Buffer copies in a frame graph".
- "Transient images" group, 16 tests: 15 passed alone. "Transient Mip Views Across Allocator Growth" failed alone on Metal, and fails alone on Vulkan in the same way: `--test` runs the first test it matches on the suite's first context, which is made with the default create info, so the test's 1 KB frame graph allocator is never used and never grows. In the whole suite it gets its own context and passes. All 16 came off.

**Every new check was broken on purpose** (`spikes/metal/mutate_m2.py` with `mutations_m3_p2.py`, `mutations_m3_p2b.py` and `mutations_m3_p2c.py`, untracked)

| Breakage | What failed |
| --- | --- |
| A freed backing's buffer, or its heap, stays in the residency set | "Metal Arena Backing" |
| A CPU backing is never mapped | "Metal Arena Backing" |
| A freed backing is never released | "Metal Arena Backing" |
| A GPU backing is shared memory | "Metal Arena Backing" |
| Every transient is placed at offset 0 | "Metal Transient Images" |
| A transient's category differs by format | "Metal Transient Images" |
| The sample count isn't kept for the bind | The process aborts inside "Metal Transient Images" |
| A host visible transient is accepted | "Metal Transient Images" |
| A retired transient's texture is never released | "Metal Transient Images" |
| A timeline wait doesn't reap | "Metal Render Pass Clear", "Metal Transient Images" |
| The reap doesn't wait for a batch whose signal was seen | "Metal Render Pass Clear", "Metal Transient Images" |
| A timeline wait doesn't reap, and the destroyed in use message isn't counted | "Metal Render Pass Clear" only, which is what the reader's change is for |
| A released batch keeps its ordering event | "Metal Transient Images" |
| A context queue never joins the device's list | "Metal Render Pass Clear", "Metal Transient Images" |
| A context queue never leaves the device's list | The process crashes in the suite under `MallocScribble=1`. Not run without it |
| Only the newest context queue stays in the list | "Metal Transient Images" |
| A released batch keeps the drawable's texture | "Metal Windowed Frames" |
| The device's idle wait doesn't reap | "Metal Render Pass Clear" |
| A batch holds no drawable texture | Nothing in one run. It guards the message seen in 2 of about 45 runs |
| A frame graph batch is committed outside a pool | "Metal Transient Images", with `--shader-validation` |

**Differences from the plan**
- **Command graphs don't reuse a transient's texture.** A command graph isn't run again from the cache, so each flush on a headless context places a new texture in the same arena and retires it. "Metal Transient Images" checks that instead: the texture has one owner after the flush, and Metal's allocated size and the residency set's count are the same after the sixth run as after the fourth. Reuse across frames is what the shared "Cached Transient Persistence" test checks, and it passes on Metal.
- **The test reads transients back now**, not from Task 5 on. A pass callback keeps each transient's texture, and the test reads it through its own blit once the graph has run. Task 5 still adds the copy through `zest_cmd_CopyImageMip`.
- **The aliasing graph has three transients in two formats.** The first two are alive together and must be apart, and the third must take the first's offset. The second and third read back what their passes cleared them to.
- **The host visible report names the hint the user set**, `zest_resource_usage_hint_cpu_transfer`, and points at an imported image with `zest_CopyBitmapToImage` or `zest_CopyImageToBitmap`. The plan asked for `zest__mtl_create_image`'s wording, which names a flag a transient's user never sets.
- **The settle step, its lock and list, and the reader's change** are not in the plan. See above and [Settled_].
- **Two M2 tests changed.**
  - "Metal Render Pass Clear" expects no committed batch after a waited flush, where it expected the one just committed. It also flushes one graph with no timeline, which must leave its batch committed until the device's idle wait.
  - "Metal Windowed Context" fails when Metal's allocated size grows across its last three cycles, not when it differs. It failed once in this phase with sizes of 8,912,896, 8,912,896 and 8,388,608, a fall, which is M2's one unexplained failure with its reason captured. M2's "a new view is made for every context" breakage still fails it.

**Checks that can't fail yet**
- Transient images with more than one sample: shared code only asks for one.
- `zest__mtl_bind_image_arena` returning false for a texture Metal won't place: nothing makes Metal refuse.
- A context queue left in the device's list, without `MallocScribble=1`.
- A batch that doesn't hold its frame's drawable texture, in any one run.

**Noticed and left alone**
- `zest-tests --test` runs the first test it matches on the suite's first context, which `RunSuite` makes with the default create info before any test is registered. A test with its own create info only gets it when it isn't first. It's in the shared runner, so it wasn't changed. The sweep's results for a test run alone are read with that in mind.
- What makes a `CAMetalLayer` drop a presented drawable's texture early.
- `zest__mtl_collect_validation_messages` reaped every context's batches by walking shared code's queue maps from whichever thread asked for a validation count. It now walks the backend's list under the lock like every other reap, so that exposure of M2's is closed.
