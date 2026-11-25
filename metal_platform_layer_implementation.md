# Zest Metal platform layer (`zest_metal.h`): implementation plan

**Goal:** A native Metal backend that does what `zest_vulkan.h` and `zest_dx12.h` do, so a Zest app on macOS runs without MoltenVK. The same application code and the same shader sources (GLSL, Slang, precompiled SPIR-V) run on it unchanged. It is a first-class equal of the other two backends, not an experiment.

**Architecture:** `zest_metal.h` fills `zest_platform_t` the way the other two backends do, and is compiled as Objective-C or Objective-C++ in the file that defines `ZEST_IMPLEMENTATION`. A shader's binary on Metal is the same SPIR-V Vulkan uses. The backend turns it into MSL with SPIRV-Cross and into an `MTLLibrary` with Metal's own compiler, once per shader, when the first pipeline needs it. The bindless set is one `MTLBuffer` of 8-byte resource IDs and GPU addresses with a fixed region per binding slot, so a bindless write is one store and nothing is declared per pass. Memory keeps Zest's TLSF model: one `MTLBuffer` per buffer pool, placement `MTLHeap`s for image pools and transient arenas, all untracked and all in one `MTLResidencySet` attached to the queues. Timelines are `MTLSharedEvent`s. The hot path sees lazy bind state flushed at each draw or dispatch, as on D3D12.

**Tech stack:**
- New: `zest_metal.h`, `cmake/ZestMetal.cmake`, `examples/SDL2/zest-tests/zest-metal-tests.cpp`, an Objective-C compile check.
- Touched: `zest.h` (builder, platform switch, shader magic, window helpers), `zest.cpp`, `CMakeLists.txt`, `examples/SDL2/CMakeLists.txt`, `examples/Common/example_platform.h`, `implementations/impl_slang.hpp` (one macro name), the test runner.
- Not touched: `zest_vulkan.h`, `zest_dx12.h`, every shader file, the submodules. No MSL twins.
- New dependency, approved 2026-11-22: SPIRV-Cross through its C API, from the macOS Vulkan SDK.

**Spec:** the 2026-11-22 discussion, where Peter chose SPIRV-Cross, Metal 3 on macOS 15+, an agent on an Apple silicon Mac, and this document shape. The D3D12 design it mirrors is `docs/superpowers/specs/2026-11-09-dx12-platform-layer-design.md`. Its milestone plans, with their As built sections, are in `docs/superpowers/plans/`. Both folders are git-ignored and exist only on the Windows machine, so this plan carries what it needs from them.

## Contents

- [Constraints_] Global constraints
- [Settled_] Settled
- [State_] Current state
- [Mapping_] How Zest maps onto Metal
- [Lessons_] Lessons carried from D3D12
- [Decisions_] Decisions taken
- [Review_] Review focus
- [Scope_] Out of scope
- [Roadmap_] Milestones
- [Phase_1] M1: baselines and spikes
- [Phase_2] M1: build and shared wiring
- [Phase_3] M1: device bring-up
- [Cost_] Per-frame cost
- [Testing_] Testing
- [Digest_] Code style digest for the Mac
- [Built_] As built

## [Constraints_] Global Constraints

- Never commit or stage. Peter handles all commits.
- **Where the work runs.** An agent on an Apple silicon Mac builds and tests. Nothing in this plan has been compiled: it was written on Windows. Every Metal API statement marked *(unverified)* is from memory and is confirmed by the spike or task named beside it, against the SDK headers on the Mac. Where the SDK disagrees with the plan, the SDK wins: record the difference under As built and carry on, unless it breaks a Settled decision, in which case stop and tell Peter.
- **What the Mac clone lacks.** `.claude/`, `CLAUDE.md`, `Notes/` and `docs/superpowers` are git-ignored (`.gitignore:422-431`). Before Task 1, Peter copies to the Mac: the repo's `CLAUDE.md` and `.claude/skills/`, and his `code-style`, `implementation-plan` and `execute-plan` skills. [Digest_] is the fallback if a skill is missing.
- **What may be edited:** the files under Tech stack. Never `zest_vulkan.h` or `zest_dx12.h`. A problem found in either goes to Peter with a repro. Never a submodule.
- **Shared code is Windows code too.** The Mac can't build D3D12 or run the Windows suites. Every task that edits `zest.h` ends its report with the list of shared edits, and Peter runs the Windows checks in [Testing_] before the next milestone starts. Keep shared edits small and additive: no existing Vulkan or D3D12 path changes behaviour in M1.
- Follow code-style. Backend naming: private functions `zest__mtl_*` (`CLAUDE.md:102`), backend structs `zest_mtl_<name>_backend_t`, `ZEST_MTL_BACKEND(name, object)`, macros `ZEST_MTL_*` and `ZEST__MTL_*`. `zest_dx12.h` is the structural model for the file but not a style model: it's agent-written.
- **Line endings.** The tracked sources are CRLF (`zest.h`, `zest.cpp`, both CMake files, `example_platform.h`). Keep each file's endings. Check with `file` before scripting any edit, and never run `sed -i` on them. New files match their neighbours.
- **Build** (Mac, from the repo root, build folder outside the tree):
  - Configure: `cmake -S . -B ../Zest-Build -DZEST_ENABLE_METAL=ON`
  - Tests: `cmake --build ../Zest-Build --config Debug --target zest-tests`
  - Compile checks: `--target zest-c-compile-check` and `--target zest-objc-compile-check`
  - Run from the repo root, because shader paths are relative to it.
- **Runs.** The implementer runs `zest-tests` and headless or `--frames N` example runs. Peter does every visual check. Never overlap two test runs.
- **Budget.** Vulkan and D3D12 get no new per-frame work. On Metal, nothing on a per-draw path allocates, locks or takes an autorelease pool. See [Cost_].
- **Spikes stop the session only on a failed verdict.** A passed spike records its numbers under As built and work continues.

## [Settled_] Settled

- **Shaders reach Metal as SPIR-V through SPIRV-Cross (Peter, 2026-11-22).** The backend accepts the same binaries as Vulkan, and GLSL goes through shaderc exactly as it does there.
  - Why: the governing D3D12 rule was "same application code, and wherever possible the same shader code". This is the only route that keeps all 67 GLSL files, the Slang sources and precompiled SPIR-V working with no third copy. MoltenVK already converts these shaders with SPIRV-Cross on every Mac run today.
  - Rejected: hand-written MSL twins beside `glsl/` and `hlsl/` (a third copy of every shader, plus Zest conventions for the Y flip and for compute local size, which MSL can't declare). Rejected: Slang's Metal target (Peter's TFXEditor notes record slangc 2025.23.2 emitting invalid MSL for bindless arrays).
  - This reverses the D3D12 spec's "no SPIRV-Cross bridge" non-goal, for Metal only. D3D12 keeps HLSL twins.
- **Metal 3 API, macOS 15 or newer (Peter, 2026-11-22).** Device creation fails with a named reason below that. This is the Metal counterpart of "Enhanced Barriers only": one path, no fallback.
  - Why: Metal 3 argument buffers make a bindless entry an 8-byte value written by the CPU, and `MTLResidencySet` (macOS 15) removes per-pass `useResource` and `useHeap` calls, which would otherwise grow with the resource count.
  - Rejected: Metal 4 on macOS 26 (closest to the D3D12 shape, but Apple silicon only and a young API). Rejected: macOS 13 with per-encoder residency calls.
- **One Mac agent, a plan that travels (Peter, 2026-11-22).** Hence this file sits in the tracked repo root, not in a git-ignored plans folder.
- **Milestone plans are written one at a time (Peter, 2026-11-22).** This document holds the design, every milestone's scope and done line, and M1 at task detail. M2 onward get task detail when their turn comes, because the D3D12 spikes moved work between milestones three times.
- **Carried over from D3D12, not reopened:**
  - Zest never falls back between APIs. The app tries another builder.
  - A requirement that fails is a device-creation failure with a message naming it, never a second code path.
  - Barriers are recorded at compile time and acted on at execution, and the add hooks push a node on every call, because `zest_PrintCompiledFrameGraph` indexes the node lists.
  - Bind state is lazy: pipeline, push constants, vertex and index buffers are recorded and flushed at the next draw or dispatch.
  - State with no native form is a pipeline-creation error with a clear message, and a documented limit.
  - A platform member with no implementation yet is a stub that reports, never NULL.
  - A green run on a backend without synchronisation validation is not evidence of correct synchronisation. The Vulkan runs stay the sync check.

## [State_] Current state (verified 2026-11-22, Windows tree at 5686546)

**Platform plumbing**
- `zest_platform_metal` already exists in `zest_platform_type` (`zest.h:1461`), with nothing behind it.
- `zest_platform_t` is the hook table (`zest.h:4656-4857`). `begin_debug_marker` is the pattern for a hook shared code NULL-checks.
- Registration: `zest__register_platform` (`zest.h:9170`) stores a setup callback, and `zest__initialise_platform_device` (`zest.h:9611`) calls it, then `new_device_backend`, then `initialise_device`.
- `zest_EndDeviceBuilder` only accepts Vulkan and D3D12: `case zest_platform_vulkan: case zest_platform_d3d12:` at `zest.h:9390-9391`. Metal falls to "Platform not implemented yet".
- After the platform device, `zest_EndDeviceBuilder` builds the bindless layout, set and pipeline layout (`zest.h:9441-9475`), the default images, the depth format and the debug font. So a device can't exist until buffer pools, image pools, views, samplers, bindless writes, transitions and buffer-to-image copies exist. M1 includes them, as D3D12's M1 did.
- The nine bindless slots are `zest_binding_number_type` (`zest.h:2704-2715`): samplers, 2D, cube, array and 3D textures, storage buffers, storage images, uniform buffers, cube arrays.
- `zest.h:23542` has an empty `#ifdef ZEST_METAL_IMPLEMENTATION` block after the D3D12 include.
- `ZEST_PRIVATE` is `static` (`zest.h:1332`), so the backend has to share a translation unit with `ZEST_IMPLEMENTATION`. That unit must therefore be Objective-C or Objective-C++ on a Metal build.

**Builders to mirror**
- `zest_BeginVulkanDeviceBuilder` (`zest_vulkan.h:1873`) and `zest_BeginD3D12DeviceBuilder` (`zest_dx12.h:2056`): register the platform, set the three queue counts to -1, the platform, the shader cache path (`./spv/`, `./dxil/`) and the flags.
- Declarations sit at `zest.h:5177-5181`. SDL2 and GLFW device helpers are at `zest.h:23382` and `zest.h:23399`.

**Shaders**
- `compile_shader` and `validate_shader` are platform hooks (`zest.h:4752-4753`). Vulkan's call shaderc (`zest_vulkan.h:7231`, `zest_vulkan.h:7255`), define `ZEST_VULKAN=1`, and keep the compiler in the Vulkan device backend. `zest__to_shaderc_shader_kind` is in `zest_vulkan.h:1087`.
- `zest_shader_t.binary` is the backend-defined blob (`zest.h:7761`). `zest_CreateShaderFromBinary` (`zest.h:13476`) has no platform hook, so a backend first sees a binary when a pipeline is built.
- `zest__shader_binary_matches_platform` (`zest.h:13350`) checks SPIR-V magic for Vulkan and DXBC for D3D12, and passes anything else. The mismatch message names only those two (`zest.h:13365`).
- The cache key takes a per-backend salt, 0 on Vulkan (`zest.h:7387`, `zest.h:13344`).
- Slang picks its target and platform macro by `zest__slang_is_d3d12` (`impl_slang.hpp:406-426`): anything that isn't D3D12 gets SPIR-V and `ZEST_VULKAN`.
- 67 GLSL files in 14 folders, 66 HLSL twins, 5 Slang files. `ExampleShaderPath` (`example_platform.h:114`) and `TestShaderPath` (`zest-tests.h:89`) already return the GLSL path for any platform that isn't D3D12. ImGui does the same (`impl_imgui.cpp:36`).

**Windows and swapchain**
- `zest_implsdl2_CreateWindow` (`zest.h:23406`) asks for `SDL_WINDOW_VULKAN` and retries without it. It holds a dead placeholder, `#elif defined(ZEST_IMPLEMENT_METAL)` with a commented-out flag (`zest.h:23410-23411`).
- On Apple the SDL helper only fills `native_handle` under `ZEST_VULKAN_IMPLEMENTATION` (`zest.h:23449-23454`).
- `zest_swapchain_t` holds an `images` array, a `views` array, `current_image_frame` and `image_count` (`zest.h:7823-7838`). Shared code reads `views[current_image_frame]` (`zest.h:15406`, `zest.h:18933`).

**Apple today**
- macOS runs through MoltenVK: `VK_USE_PLATFORM_METAL_EXT` (`zest_vulkan.h:40`), `VK_KHR_portability_subset` (`zest_vulkan.h:129`), AppKit and QuartzCore linked on non-MSVC Apple builds (`CMakeLists.txt:123`).
- The tests already carry Apple branches: no strips or fans (`zest-pipeline-tests.cpp:214`), no `mip_lod_bias` (`zest-resource-management-tests.cpp:1548`).
- Nobody has built the Mac tree since the D3D12 work landed. Task 1 finds out whether it still builds.

**Tests and examples**
- `zest-tests.cpp` is a unity build: it defines the implementation (`zest-tests.cpp:1-4`) and includes every test file (`zest-tests.cpp:10-20`). So one file needs the Objective-C++ flag.
- `main` parses `--platform vulkan|d3d12|all`, `--warp`, `--gbv` (`zest-tests.cpp:352-395`) and prints one summary line per run (`zest-tests.cpp:440-448`).
- `vulkan_only_tests[]` and `RegisterTest` filter the D3D12 suite (`zest-tests.cpp:195-219`). `CreateSuiteDevice` picks the builder (`zest-tests.cpp:292`).
- `zest-c-compile-check` compiles `zest.h` with the compiled-in backends as C11 (`examples/SDL2/CMakeLists.txt:249-251`, `zest-tests/zest-c-compile-check.c`).
- Every example defines the implementation in its own main file and goes through `example_platform.h`, which knows only `--vulkan` and `--d3d12`.

**Wiring to mirror, from D3D12**
| What | Where D3D12 does it |
| --- | --- |
| CMake option, package finding, `ZEST_*_ENABLED` | `cmake/ZestD3D12.cmake`, `CMakeLists.txt:40-52` |
| Implementation define for the static library | `zest.cpp:5-9` |
| Backend include | `zest.h:23539-23541` |
| Builder | `zest_dx12.h:2056` |
| Callback table | `zest__dx_initialise_platform_callbacks`, `zest_dx12.h:2603` |
| Contents list and section markers | `zest_dx12.h:4-34` |
| Report stub | `zest__dx_report_unavailable`, `zest_dx12.h:3635` |
| Forced-failure test hooks | `zest__dx_forced_*`, `zest_dx12.h:739-745` |
| Interop getters | `zest_dx12.h:755-758` |
| Backend-only test list | `zest-d3d12-tests.cpp`, `BeginD3D12TestBuilder` at line 21 |
| Suite run and summary lines | `zest-tests.cpp:420-448` |
| Example backend choice | `example_platform.h:26-88` |

## [Mapping_] How Zest maps onto Metal

This is the design. Rows marked *(unverified)* are confirmed by the named spike or milestone.

| Zest | Metal | Confirmed by |
| --- | --- | --- |
| Device | The system default `MTLDevice`. Requires macOS 15, `MTLGPUFamilyMetal3` and argument buffers tier 2 | Task 7 |
| Queue families | Three `MTLCommandQueue`s reported as families 0 graphics, 1 compute, 2 transfer, with D3D12's explicit acquire lists (Decision 2) | Task 7 |
| Timelines, queue ordering | `MTLSharedEvent`. A command buffer encodes its waits first (`encodeWaitForEvent:value:`) and its signals last (`encodeSignalEvent:value:`). CPU waits use `waitUntilSignaledValue:timeoutMS:` | Spike B, confirmed |
| Binary semaphores | Not needed. The swapchain semaphore hooks return NULL | |
| Command pools | Nothing to pool. Each batch takes a command buffer with unretained references, since Zest already defers frees past the frame in flight. Metal's debug layer needs an object to outlive its buffer's completed handlers. Those run after the buffer reads as completed, which itself comes a little after the event a wait sees. So a successful timeline wait, and the device's idle wait, call `waitUntilCompleted` on every batch they release, whatever its status says. A batch also holds its frame's drawable texture until then, as the layer drops a drawable once it has been shown | M2, M3, M4 |
| Passes | One encoder at a time per command buffer. Render passes open a render encoder from a descriptor carrying the load and store ops. Compute and transfer passes switch lazily between a compute and a blit encoder as commands need them | M2, M4, confirmed |
| Buffer pools | One untracked `MTLBuffer` per pool, suballocated by TLSF. Host-visible pools are shared storage and always mapped. Device-local pools are private | Task 8 |
| Image pools, dedicated images, transient arenas | Private, untracked placement `MTLHeap`s. An image is a texture placed at the pool offset with `newTextureWithDescriptor:offset:`, and two textures may share a range | Spike B, confirmed |
| Residency | One `MTLResidencySet` per device holding every pool buffer and heap, attached to all queues. No `useResource` for any pool buffer or heap, including for GPU writes through the bindless table. A resource missing from the set reads and writes wrong, silently unless shader validation is on, when the command buffer's `logs` name the access as not resident. Shader validation also drops reads and writes through a view of a heap texture that the encoder hasn't declared, so under shader validation only, every render and compute encoder declares the device's texture views with `useResources` | Spike B, confirmed, M4 |
| Ordering inside a queue | One `MTLFence` per queue. Each encoder waits on it when it opens and updates it when it ends, which serialises passes inside a command buffer and across command buffers on that queue. Without it untracked passes do run out of order. Refined from the barrier records only if profiling shows a cost | Spike B, M5, confirmed |
| Image layouts | None. The backend tracks the Zest layout so `get_image_raw_layout` and the layout tests work | Task 9 |
| Crossing queues | The shared event orders the handoff. No barrier on either side. A batch whose queue slot moved to another device queue first waits on the slot's ordering event | M5, confirmed |
| Views | `newTextureView` objects. `MTLTextureUsagePixelFormatView` only on images that need a view of another format | Task 9 |
| The bindless set | One shared-storage `MTLBuffer` with a fixed region per slot. Every entry is 8 bytes: an image or sampler is its `gpuResourceID`, a buffer is `gpuAddress + offset`. Set 0 is converted as discrete bindings, so slot N's region binds at buffer index 5 + N, all nine with one ranged call per stage | Spike A, confirmed |
| Structured buffer indexes | The same as raw ones, as on Vulkan. MSL pointers carry the type | Spike A, confirmed |
| Shader creation | shaderc for GLSL (`ZEST_METAL=1` defined), then SPIRV-Cross to MSL, then `newLibraryWithSource`. The function is always taken with `newFunctionWithName:constantValues:error:`, which a shader with specialisation constants needs. Converted when a pipeline or compute first needs it, and cached by the SPIR-V's bytes until a device reset, as a binary can arrive without `compile_shader`. Slang arrives as SPIR-V too, compiled with `ZEST_METAL` defined, and goes through the same conversion. Before it, ids and struct members named after an MSL address space get a trailing underscore, as SPIRV-Cross leaves some of them alone. MSL input: options marked with `zest_SetShaderOptionsMSL` keep the text as the binary, never cached, and the backend takes the library's one function of the stage, with vertex inputs from Metal's reflection and the threads per threadgroup from the options. Vulkan and D3D12 refuse MSL | Spike A, M4, M6, M7, confirmed |
| Clip space | SPIRV-Cross flips Y in the vertex stage, so positions land on Vulkan's pixels with a plain viewport. Front face maps straight across: clockwise is `MTLWindingClockwise` | Spike A, confirmed |
| Push constants | `setVertexBytes`, `setFragmentBytes` and the compute `setBytes` at flush, at buffer index 4. No upload ring | Spike A, M4, confirmed |
| Compute local size | Read from the SPIR-V and passed to `dispatchThreadgroups` | Spike A, confirmed |
| Pipelines | Built lazily from the rendering info, with a device-wide cache keyed by contents that a device reset empties. Depth-stencil state is its own cached object, one of 16. Cull mode, winding, fill mode, depth clip, depth bias and topology are encoder or draw state | M4, confirmed |
| Indirect draws | One native call per draw, looped over `draw_count`. Metal's indirect argument structs have Vulkan's field order | M4, confirmed |
| Multiview | Each view is an instance in layered rendering, from SPIRV-Cross's multiview conversion. The backend multiplies the instance count and every per-instance step rate by the view count, sends the first view and the count at buffer index 24, sets `renderTargetArrayLength`, and gives a multiview pipeline its `inputPrimitiveTopology`. Up to 32 views. An indirect draw in a multiview pass is refused, as its instance count is in a GPU buffer. Vertex amplification takes at most 2 views | M5, confirmed |
| Mip generation | The blit encoder's `generateMipmaps` | Task 9 |
| Scaled blits | A built-in pipeline whose shaders are MSL text in `zest_metal.h`, so it needs neither shaderc nor SPIRV-Cross | M3 |
| Clears outside a pass | A render pass with a clear load op and no draws where the image can be a target, otherwise a copy from zeroed staging | M3 |
| Swapchain | A `CAMetalLayer`. Acquire is `nextDrawable`, present is `presentDrawable` on the frame's last command buffer. Resize is detected by comparing sizes | M2 |
| Validation | `MTL_DEBUG_LAYER` and `MTL_SHADER_VALIDATION`, set with `setenv` before the process's first Metal device. Creating a window makes that device, so for a windowed app it means before the first window, which is what `zest_PrepareMetalValidation` is for. It is process wide and can't be changed afterwards. Shader validation messages are counted from each command buffer's `logs`. API messages are logged through `NSLog` and counted by a reader on standard error (decision 9) | Spike C |
| Markers and names | Each pass is a debug group on its batch's command buffer, from its acquire barriers to its release barriers, and its encoders carry its name. Command buffers carry their frame graph's name, pipeline states their template's, compute states and libraries their shader's. Always on | M7, confirmed |
| GPU timestamps | One shared counter sample buffer of 4096 samples per frame in flight. Apple GPUs sample only at encoder stage boundaries, so a query is mapped to the start of the next encoder or the end of the open or last one, and resolved on the CPU at readback. A region in a compute or transfer pass ends its encoder and gets its own, and a region in a render pass takes the pass. Ticks are nanoseconds | M7, confirmed |
| Device loss | A failed frame graph batch writes one report: the error, the frame graph, and with validation each encoder's state, from encoder execution status asked for only then. Its events are moved on so no wait runs out its timeout, timeline waits reap every 100 ms, and `MTLCommandBufferErrorDeviceRemoved` flags the context's device as lost. The M2 can't make a command buffer fail, so a forced hook tests it | M7, confirmed |

**Objective-C rules for the file**
- It compiles as Objective-C and as Objective-C++, the way the other headers compile as C and C++. `zest-objc-compile-check` guards the C side.
- Manual retain and release (Decision 1). Backend structs hold `id` members, which are plain pointers without ARC, so `ZEST__ZERO_INIT` and the allocator work as they do everywhere else.
- A hook that calls anything returning an autoreleased object wraps its body in `@autoreleasepool` and retains what it keeps. That means device and context creation, command buffer and encoder creation, drawable acquire, and pipeline creation. Per-draw hooks call no such method and take no pool.

## [Lessons_] Lessons carried from D3D12

From the M1 to M3 ledgers, the review log and the later plans. Each one is already applied somewhere in this plan.

- **Spike what the plan can't know.** Four D3D12 facts were planned as fact and cost ruling chains in the middle of tasks: barrier rows, heap flags, back-buffer writes from another queue, and whether a discard flag initialises a target. So this plan front-loads three standalone spikes before any backend code.
- **A spike needs a control.** The D3D12 viewport spike gained a culling-off draw so a broken pipeline couldn't read as a facing result. Spike A has the same.
- **Spike findings must outlive the spike.** D3D12 lost coverage that lived only in a deleted harness. Each spike names the backend test that takes over its check.
- **Baselines are captured, not quoted.** The M1 plan's test count was already wrong on the day. Task 1 records the Mac's own numbers.
- **Tests that can't fail.** Reviews found tests that passed with the fix removed, and descriptor tests that never read through the descriptor. Every M1 test states what it reads back, and the task confirms it fails when the code under test is broken.
- **Stubs report.** Unregistered members were called as NULL, and a stub returned NULL that shared code dereferenced. Task 6 gives every member a real function or a report stub from the first day.
- **Runs that prove nothing.** A `--frames` run exited 0 before anything drew, and an unconditional validation reset hid creation errors. Example runs print a draw count, and only known loader noise is reset.
- **Shared code regresses the other backends silently.** Parity gaps turned up where a new shared wrapper behaved differently from Vulkan's old body. M1's shared edits are additive, and Peter's Windows run gates each milestone.
- **Milestone dependencies hide in the test runner.** `RunTests` resets the device between tests, so `reinit_logical_device` had to move from M5 to M2. The roadmap has it in M2. Blits needed a shader compiler early, so here the built-in blit shaders are plain MSL and nothing moves.
- **Grow the suite with an exclusion list.** D3D12 used an inclusion list until M6 and then flipped. Here the Metal suite registers everything except a pending list, each entry with its reason and milestone, so a new shared test lands on Metal by default.
- **Hardware the developer doesn't have.** Paths the dev GPU never takes went untested until forced-failure hooks existed. Metal gets `zest__mtl_forced_*` hooks for the requirement checks in Task 7.
- **Threads.** Shared descriptor heaps and lazily created objects crashed two runs out of two without locks. Anything the Metal backend creates lazily (libraries, pipelines, the blit pipeline) takes a lock on creation and none on the read path.
- **Shared tests carry backend expectations.** "Compute Test Headless Flush And Wait" reads `next_buffer`, so D3D12 had to count the lists begun since the pool reset as Vulkan does. Metal counts the command buffers begun per context queue the same way (M5).
- **A chained graph never leaves the graphics queue.** The compiler moves a wave that uses one queue type onto the graphics queue, so a multi-queue test needs independent passes in one wave. The M5 Metal tests are built that way.

## [Decisions_] Decisions taken (Peter, 2026-11-22)

All five were taken as recommended. The implementer does not reopen them.

1. **Language and reference counting.** Taken: the implementation file compiles as Objective-C or Objective-C++ without ARC, and `zest_metal.h` raises `#error` under ARC with a message naming `-fno-objc-arc`. Zest's CMake sets the flags, so `zest.cpp` and each example's main file stay `.cpp`. One memory model, explicit releases that match Zest's Create and Free pairs, and plain-data backend structs.
   - Rejected: also support ARC through bridging macros, as sokol does. Costs four macros and `void *` members, and helps users who drop the header into an Xcode target with default settings.
   - Rejected: plain C through `objc_msgSend`. Keeps every file `.c` or `.cpp`, but 8,000 lines of casted message sends that no compiler checks.
2. **Queue model.** Taken: three command queues as three families, as on D3D12. Shared multi-queue code then runs the same paths on all three backends and the stress tests cover Metal. Rejected: one family. Simpler, no crossings, but async compute collapses onto the graphics queue and Metal exercises a path the dev GPUs never take.
3. **Both backends in one Mac binary.** Taken: yes, Vulkan through MoltenVK and Metal together, chosen at runtime, as on Windows. One `zest-tests` run covers both, and the MoltenVK run keeps Vulkan's synchronisation validation on the Mac. The Mac build needs the Vulkan SDK either way, for shaderc and SPIRV-Cross. Rejected: Metal-only builds on Apple, with `ZEST_VULKAN_IMPLEMENTATION` off there.
4. **Definition of done.** Taken: D3D12's end state. The whole suite passes on Metal apart from a list of permanent exclusions with reasons, every example runs with `--metal`, and from M6 `ZEST_ENABLE_METAL` is on by default on Apple and the examples start on Metal, falling back to Vulkan. Rejected: stop at core parity (suite, minimal template, ImGui, TimelineFX) and port the rest later. With no shader twins to write, the full set is mostly wiring.
5. **How SPIRV-Cross links.** Taken: the SDK's static libraries, so nothing extra ships beside an executable, as with `shaderc_combined`. Rejected: `libspirv-cross-c-shared.dylib`, one library to find but a file to copy next to every target.

**Decisions taken after Spike A (Peter, 2026-11-22)**

All three were taken as recommended. The details are under [Built_], Task 2.

6. **The uniform alias workaround.** Two uniform block types at binding 7 in one shader make SPIRV-Cross emit MSL that Metal rejects. Two of the 67 GLSL files do it, and SPIRV-Cross `main` still has the bug. Taken: the backend replaces one string in SPIRV-Cross's output before compiling, about 20 lines, a no-op for every other shader and for a SPIRV-Cross that fixes itself. The spike ran with it and reads through both block types correctly.
   - Rejected: a documented limit of one uniform block type per shader on Metal, and the two example shaders change. That breaks "same shader on both backends" for a pattern Zest's own examples use.
   - Rejected: build SPIRV-Cross from patched source, which undoes Decision 5.
   - Still open: whether to report it upstream. Nothing has been sent.
7. **Arrays indexed only by constants.** `textures[2]` with no variable index anywhere becomes a sized array and a discrete Metal binding the table can't serve. No shader in the repo does it with a resource it uses. Taken: pipeline creation fails with a message naming the array and saying to index it with a variable, and the docs list the limit. Rejected: rewrite the array type in the SPIR-V before conversion, about 60 lines.
8. **`.length()` on a bindless storage buffer.** SPIRV-Cross refuses it without a 16-byte entry format the C API doesn't expose. No shader in the repo uses it. Taken: a documented limit, with SPIRV-Cross's error passed to the user.

**Decision taken after Spike C (Peter, 2026-11-22)**

9. **How API validation errors are handled.** Metal has no callback for its debug layer, only three modes set before the first device: abort, log through `NSLog`, or ignore. Shader validation is not in question: its messages are counted from the command buffer's `logs` either way. The details are under [Built_], Task 4.
   - Taken: **log and count.** The backend sets the log mode, points standard error at a pipe while a validated device exists, and a reader thread passes every byte through to the real standard error and counts the debug layer's records. `zest_GetValidationErrorCount` then means the same on all three backends, a suite run survives an error and reports it against its test, and the Vulkan suites can share the process. Cost: about 120 lines, one thread while validation is on, the count rests on recognising the layer's lines by their text, and a handler has to flush the pipe when the process aborts so an assert's message isn't lost.
   - Rejected: **abort.** Metal's default, and no code. An API error stops the process at the faulting call with the message, which is the best place to be in a debugger. But `zest_GetValidationErrorCount` then counts shader validation only, a suite run ends at the first API error, and the Vulkan suites can't share a process with Metal validation, because MoltenVK itself raises one debug layer error (below). `--platform all` would have to run the Metal suite in a child process.
   - The backend also has to know validation really came on. Taken: one deliberate error at device creation (a zero length buffer, one call) that the reader must see. Rejected: walking the device's wrappers through the private `baseObject` selector.

**Added to the roadmap (Peter, 2026-11-22)**
- **MSL as a shader input, in M7.** A user can hand the Metal backend MSL text and skip shaderc and SPIRV-Cross, since the backend ends in `newLibraryWithSource` either way. Slang needs nothing new: it reaches Metal as SPIR-V. M7's plan settles the API. What it has to cover:
  - a way to mark a shader's source as MSL, which is a small addition to `zest.h`
  - the conventions the user's MSL must follow, documented: slot regions as descriptor pointers at buffer indices 5 to 13, push constants at 4, vertex buffers at 0 to 3
  - the Y flip, which the user's vertex shader does itself
  - the compute threadgroup size, which MSL can't declare, so it comes through the API

**Calls made without Peter (check these)**
- The Metal GLSL compile is its own shaderc call in `zest_metal.h`, about 60 lines that mirror `zest__vk_compile_shader`. Hoisting the Vulkan one into `zest.h` would edit `zest_vulkan.h`, and a Metal-only build has no `zest_vulkan.h`.
- Metal compiles GLSL with `ZEST_METAL=1` and not `ZEST_VULKAN`, with a non-zero cache salt, and caches SPIR-V in `./spv/` beside Vulkan's. Precompiled Vulkan SPIR-V still loads.
- Intel Macs get no code and no gate. The backend uses only shared buffers and private textures, which are legal there, and the docs call Intel Macs untested.
- Spike sources go in an untracked `spikes/metal/` folder. Their results go under As built in this file, and their checks move into backend tests.
- Only `zest_GetMetalDevice` is added as an interop getter in M1. Queue and command buffer getters wait for an integration that needs one.

## [Review_] Review Focus

1. **The bindless table layout is wrong by one entry size.** A texture entry of 8 bytes against a buffer entry of 8 or 16 shifts every index after it, and the shader reads another resource with no error. Probe: the "Metal Bindless Slots" test writes a distinct resource at the first, a middle and the last index of every slot and reads each back through a shader. It must fail when any region's stride is changed by 8.
2. **Residency or ordering is assumed, not proven.** A pass reads a buffer the previous pass wrote through the table, with no `useResource`. If residency sets don't cover writes, or untracked passes overlap, it works on the day and fails under load. Probe: Spike B under `MTL_SHADER_VALIDATION=1`, then "Metal Untracked Pass Ordering" in the backend list, run 200 times in a loop.
3. **Leaked or over-released Metal objects.** Manual reference counting with `@autoreleasepool` in some hooks and not others. Probe: every M1 test ends by destroying its device and comparing `currentAllocatedSize` with a baseline taken after earlier lifecycles, never before the first one (Spike B: the device keeps first-use storage and frees the first lifecycle's late). The suite also runs once under `MallocScribble` and zombies (`NSZombieEnabled=YES`).
4. **A shared-code edit breaks Windows.** The Mac can't see it. Probe: Peter's Windows run in [Testing_], which must match the counts he records before M1 starts.
5. **Validation is silent because it isn't on.** If `setenv` after process start doesn't enable the debug layer, every "no validation errors" line means nothing. Spike C confirmed the danger: `setenv` after any Metal device exists does nothing, with no warning. Probe: device creation checks that validation came on (decision 9) and reports when it didn't, and "Metal Validation Reports" stays in the backend list to make one error per run and expect a count of one.

## [Scope_] Out of scope

- iOS, tvOS and visionOS. The API use is the same, but there's no window helper, no test target and no device to run on. It would start in the window helpers and `cmake/ZestMetal.cmake`.
- Metal 4. A later backend revision could move to `MTL4` command allocators and argument tables. It would start at the command recording and residency sections.
- Intel Mac support beyond "may work".
- MSL twins of the repo's shaders. The built-in blit shaders are the only MSL in the repo. MSL as an input for a user's own shaders is an M7 item.
- MSAA resolve, which doesn't work in shared code or on Vulkan dynamic rendering and has its own plan pending.
- Custom descriptor sets, which shared code still can't allocate or write on any backend.
- Tessellation and geometry shaders, which Metal doesn't have in Vulkan's form. The capabilities report false.
- A serialised pipeline cache (`MTLBinaryArchive`), and an `msl/` text cache. M7 dropped the text cache (its Decision 1): with `zest_DeviceBuilderCacheShaders` a warm start already skips shaderc, and SPIRV-Cross costs about 0.3 ms a shader.
- The TimelineFX hot reload harness, left on Vulkan as it was for D3D12.

## [Roadmap_] Milestones

Each ends with something runnable. M2 to M7 get task-level plans when they start. M2's is `metal_m2_implementation.md`, M3's `metal_m3_implementation.md`, M4's `metal_m4_implementation.md`, M5's `metal_m5_implementation.md`, M6's `metal_m6_implementation.md` and M7's `metal_m7_implementation.md`.

| Milestone | Covers | Done when |
| --- | --- | --- |
| **M1** Spikes, groundwork, device | The three spikes. CMake and shared wiring. `zest_metal.h` with every member stubbed. Device, queues, shared events, immediate command buffers. Buffer pools, heaps, residency. Formats, images, views, samplers, uploads with mips, readback. The bindless table. Headless contexts. The Metal backend test list | A Metal device and a headless context create and destroy with validation silent and allocated size back at its baseline, an image with pixels reads back unchanged, and the Mac's Vulkan runs match Task 1's baseline |
| **M2** Swapchain and frames | Window helpers, `CAMetalLayer`, acquire, present, resize. Per-batch command buffers, render passes with load and store ops, shared-event submission, the fence chain. `reinit_logical_device`. The Metal suite run joins `RunSuite` with its pending list | An app clears the window to a colour on Metal, resizes and shuts down clean |
| **M3** Memory and resources | Transient arenas and unbound transient images. Buffer copies and uploads. Storage and uniform buffer entries, storage images, per-mip views. The MSL blit pipeline, `image_clear`, the remaining immediate functions except push constants and compute | The resource-management tests pass on Metal, apart from those that need compute |
| **M4** Pipelines | shaderc and SPIRV-Cross inside the backend, the conversion cache, render and compute pipelines, lazy bind, draws, indirect draws, dispatch, push constants, the debug overlay, `--metal` on the minimal template | The pipeline and layer tests pass on Metal and the minimal template draws |
| **M5** Full frame graph | Multi-queue and async compute over shared events, transient aliasing, cached graphs, multiview, the device-reset tests | The frame graph, stress, compute and device-reset tests pass on Metal |
| **M6** Parity | `ZEST_METAL` in the Slang integration, ImGui, the TimelineFX examples, every example through `example_platform.h` with `--metal`, the pending list cut down to permanent exclusions | The suite passes on both Mac backends and every example runs on Metal with a non-zero draw count. `ZEST_ENABLE_METAL` then defaults to on for Apple |
| **M7** Polish | GPU timestamps, markers and labels, command buffer error reports, MSL as a shader input, user docs, a `metal-backend` skill, the README line. The `msl/` text cache was dropped (M7 Decision 1) | The overlay shows GPU timings, an Xcode capture shows pass names, a second launch with shader caching on compiles no GLSL, and a test draws with a hand-written MSL shader |

## [Phase_1] M1: baselines and spikes

The spikes are standalone programs: one `.mm` file each plus the SDK's SPIRV-Cross, no Zest. They answer what the design assumes before `zest_metal.h` exists.

### Task 1: Mac baseline

**Files:** none edited. Results go under `## As built` in this file.

**Steps:**
- [x] Record the machine: chip, macOS version, Xcode version, Vulkan SDK version and path, and whether the SDK has `spirv_cross_c.h` and the `spirv-cross-*` static libraries. Expected: macOS 15 or newer. If the SDK lacks SPIRV-Cross, stop and tell Peter.
- [x] Configure and build the tree as it is (no Metal option exists yet) and build `zest-tests` in Debug. Expected: it builds. If it doesn't, list the errors for Peter before fixing anything: they're Mac regressions from the D3D12 work in shared code.
- [x] Run `zest-tests` from the repo root. Record the `Dynamic rendering` and `Legacy render pass` counts, every failing test, and the known noise lines.
- [x] Run `zest-minimal-template` and `zest-imgui-template` with `--validate --frames 600`. Record exit codes and validation counts.
- [x] Build `zest-c-compile-check`. Expected: clean.

### Task 2: Spike A, shaders and the bindless table

**Files:** `spikes/metal/spike_shaders.mm` (untracked).

**Question:** can SPIRV-Cross turn Zest's SPIR-V into MSL that Metal compiles, with the bindless set as plain 8-byte entries the CPU writes?

**Steps:**
- [x] Compile all 67 GLSL files to SPIR-V with the SDK's `glslc`, defining `ZEST_METAL=1`. If `slangc` is present, add `timelinefx.slang`'s entry points as SPIR-V.
- [x] Convert each with the SPIRV-Cross C API: MSL 3.1 or newer, Metal 3 argument buffers at tier 2, runtime arrays as descriptor arrays, `flip_vert_y` on. Try set 0 both as one argument buffer and as discrete bindings, and keep whichever gives every runtime array a region the CPU can fill with consecutive 8-byte values. Record the exact option set.
- [x] Compile every MSL result with `newLibraryWithSource`. Expected: all compile. List any that don't, with the error.
- [x] From the generated MSL and Metal's reflection, record for each of the nine slots: the entry size in bytes, the buffer index it binds at, and whether one binding call or several are needed per stage.
- [x] Build one render pipeline and one compute pipeline from converted shaders, and run them with a hand-built table: sample a texture through slot 1 with a sampler from slot 0, read a storage buffer through slot 5, write one through slot 5, write a storage image through slot 6, read a uniform buffer through slot 7. Read the results back.
- [x] Assign fixed buffer indices for the table, the push constant block and vertex buffer bindings 0 to 3 with no collision in any of the 67 shaders. Record them as the proposed `ZEST__MTL_*_BUFFER_INDEX` values.
- [x] Read a compute shader's local size from SPIR-V through the C API and dispatch with it.
- [x] Viewport and facing: draw one triangle covering the top-left half of a small target, with culling off as the control, then with back-face culling for each winding. Compare the covered pixels with what the same SPIR-V covers on MoltenVK.
- [x] Draw with a non-zero base instance and check `gl_InstanceIndex` includes it.

**Pass:** every shader converts and compiles, the table works with CPU-written entries, the indices don't collide, the covered pixels equal Vulkan's, and exactly one winding draws.
**Fail:** runtime arrays can't be filled as plain entries, or a class of the repo's shaders doesn't convert. Stop and tell Peter: the fallback is MSL twins, which changes M4 and M6.
**Takes over the check:** "Metal Bindless Slots" (Task 10) and, in M4, a facing test that stays in the suite.

### Task 3: Spike B, memory, residency and ordering

**Files:** `spikes/metal/spike_memory.mm` (untracked).

**Question:** do placement heaps, untracked resources, one residency set and one fence per queue give Zest's memory model with nothing declared per pass?

**Steps:**
- [x] Create a private placement heap, query a texture's size and alignment, and place two textures at chosen offsets. Then place a third over the first's range, as an aliased transient would be.
- [x] Create one untracked shared buffer as a pool and form two bindless entries from `gpuAddress` plus different offsets.
- [x] Put the heap and the buffer in one residency set, commit it and attach it to three command queues. Call `useResource` and `useHeap` nowhere.
- [x] In one command buffer: a compute encoder writes the buffer and a heap texture through the table, then a render encoder samples that texture and reads the buffer. Each encoder waits on one shared fence when it opens and updates it when it ends. Read back and check. Run it 200 times.
- [x] Repeat with the render pass rendering into a heap texture and a later pass sampling it.
- [x] Run everything under `MTL_DEBUG_LAYER=1` and `MTL_SHADER_VALIDATION=1`. Expected: no message.
- [x] Shared events: queue A's command buffer signals value 1, queue B's waits for it before reading what A wrote. Then a CPU wait with a timeout, once satisfied and once timing out. Record the API used for the timed wait.
- [x] Record `currentAllocatedSize` before the first allocation and after the last release.

**Pass:** correct readback on every run, validation silent, wait-then-update on one fence accepted, the timed CPU wait returns both ways, and allocated size returns to its start.
**Fail, by part:**
- Residency sets don't cover GPU writes: the backend calls `useResource` for writable images per encoder, which costs a list walk per pass. Tell Peter before continuing.
- One fence can't chain: one fence per encoder, waited on by the next.
- No timed CPU wait: a listener block signalling a semaphore.
**Takes over the check:** "Metal Untracked Pass Ordering" and "Metal Timeline Wait" in Task 11's list.

### Task 4: Spike C, validation and leaks

**Files:** `spikes/metal/spike_validation.mm` (untracked).

**Question:** how does a Zest device turn Metal validation on, and how do its messages reach `zest_GetValidationErrorCount`?

**Steps:**
- [x] Call `setenv` for `MTL_DEBUG_LAYER` after `main` starts and before the device is created. Make one deliberate API error (a scissor rect outside the target). Record whether it's reported.
- [x] If it isn't, record the alternatives that do work: variables set by the launcher, or re-executing the process with them set.
- [x] With `MTL_DEBUG_LAYER_ERROR_MODE=nslog`, find out where the message goes and whether an in-process reader on standard error sees it. Record the line's prefix.
- [x] Make one deliberate shader validation error (an out-of-bounds buffer read) under `MTL_SHADER_VALIDATION=1` and record what the command buffer's `logs` and `error` hold.
- [x] Record what a second device created in the same process sees.

**Pass:** there is a way to turn validation on from inside the process, and a way to count API and shader validation messages without scraping the console from outside.
**Fail:** validation can only be set by the launcher, or API errors can only abort. Then `zest-tests` re-executes itself with the variables set, an API validation error ends the run at the failing test, and the docs say so. Tell Peter which.
**Takes over the check:** "Metal Validation Reports" in Task 11's list.

## [Phase_2] M1: build and shared wiring

### Task 5: CMake

**Files:**
- Create `cmake/ZestMetal.cmake`.
- Modify `CMakeLists.txt:40-52` (add the Metal block after D3D12's), `zest.cpp:5-9`, `examples/SDL2/CMakeLists.txt:191` and `:249-251`.
- Create `examples/SDL2/zest-tests/zest-objc-compile-check.m`.

**Interfaces:**
- Option `ZEST_ENABLE_METAL`: Apple only, default OFF until M6. Result variable `ZEST_METAL_ENABLED`.
- `ZEST_METAL_SPIRV_CROSS_INCLUDE_DIR`, `ZEST_METAL_SPIRV_CROSS_LIBRARIES`, found in the Vulkan SDK. A failed find stops the configure with a message ending "Configure with -DZEST_ENABLE_METAL=OFF to build without Metal."
- Compile definition `ZEST_ENABLE_METAL` on `zest` and `zest-interface`.
- Function `zest_metal_implementation_source(<target> <file>)`: marks the file that defines the implementation as Objective-C++ without ARC. It's a no-op when Metal is off.
- `zest.cpp` gains `#ifdef ZEST_ENABLE_METAL` / `#define ZEST_METAL_IMPLEMENTATION` / `#endif`, as it has for D3D12.
- Target `zest-objc-compile-check`: an object library compiling `zest.h` with the Metal implementation as Objective-C, C11. Never linked.

**Steps:**
- [x] Write `ZestMetal.cmake` mirroring `ZestD3D12.cmake`'s shape: option, find, enabled flag. Link Metal, QuartzCore and Foundation, and the SPIRV-Cross libraries in the order Spike A recorded: `spirv-cross-c`, `-msl`, `-hlsl`, `-cpp`, `-reflect`, `-glsl`, `-core`. Set the deployment target to 15.0 when the option is on.
- [x] Call `zest_metal_implementation_source` for `zest.cpp`, for each example's main file in `buildExample`, and for `zest-tests.cpp`.
- [x] Configure with the option OFF. Expected: the build is unchanged from Task 1 and the tests give Task 1's counts.
- [x] Configure with it ON. Expected: the configure finds SPIRV-Cross. The build fails only because `zest_metal.h` doesn't exist yet, which Task 6 fixes.

### Task 6: Shared wiring and the backend skeleton

**Files:**
- Create `zest_metal.h`.
- Modify `zest.h:5177-5181` (declarations), `zest.h:9390-9391` (platform switch), `zest.h:13350-13365` (shader magic and message), `zest.h:23542` (include).

**Interfaces:**
- `ZEST_API zest_device_builder zest_BeginMetalDeviceBuilder(zest_device_init_flags flags);` defined in `zest_metal.h`, mirroring `zest_dx12.h:2056`, with `cached_shader_path = "./spv/"`.
- `zest_EndDeviceBuilder` accepts `zest_platform_metal` in the same case as the other two.
- `zest__shader_binary_matches_platform` treats Metal as SPIR-V. The mismatch message names SPIR-V for every platform that isn't D3D12.
- `zest_metal.h`:
  - A contents list mirroring `zest_dx12.h:4-34`, with `Fences` renamed `Shared_events`, `Descriptor_heaps` renamed `Bindless_table`, and no `Device_removal` until M7.
  - `#error` under ARC (Decision 1).
  - `ZEST_MTL_BACKEND(name, object)`, `ZEST_MTL_RELEASE(object)` (release and set to nil).
  - `ZEST_PRIVATE void zest__mtl_initialise_platform_callbacks(zest_platform_t *platform);` assigning every member of `zest_platform_t`.
  - `ZEST_PRIVATE zest_bool zest__mtl_report_unavailable(zest_device device, const char *message);` mirroring `zest_dx12.h:3635`. Every member without an implementation points at a stub that calls it and returns the type's failure value. No stub returns a pointer shared code will dereference: check each call site.
  - `flush_legacy_framebuffers` stays NULL and `create_test_render_pass` returns at once, as on D3D12.

**Steps:**
- [x] Add the shared edits. They're additive: no Vulkan or D3D12 line changes meaning.
- [x] Write the skeleton with the callbacks function and one stub per member, grouped as `zest_platform_t` groups them.
- [x] Build `zest-tests`, `zest-c-compile-check` and `zest-objc-compile-check` with Metal ON. Expected: all build.
- [x] Run `zest-tests`. Expected: Task 1's counts, since nothing creates a Metal device yet.
- [x] List the shared edits for Peter's Windows run.

## [Phase_3] M1: device bring-up

Each task adds its tests to `zest-metal-tests.cpp` as it goes. Task 11 wires the list into the runner, so until then build and run them through a temporary call in `main` that Task 11 replaces.

### Task 7: Device, queues, shared events, immediate commands

**Files:** `zest_metal.h` sections `Backend_structs`, `Initialisation_functions`, `Shared_events`, `Command_pools`, `Device_OS`. Create `examples/SDL2/zest-tests/zest-metal-tests.cpp`.

**Interfaces:**
- `zest_mtl_device_backend_t`: the device, the residency set, the support flags, the validation state.
- `zest_mtl_feature_support_t`: what the rest of the backend switches on. At least BC and ASTC support, the argument buffer sampler limit, and the counter sampling points.
- `zest__mtl_meets_minimum_spec(const zest_mtl_feature_support_t *support, const char **out_missing)`: macOS 15, Metal 3 family, argument buffers tier 2. A failure writes a MISSING line to the compatibility log, as D3D12 does.
- `zest__mtl_forced_missing_feature` and `zest__mtl_forced_validation_unavailable`: test hooks, mirroring `zest_dx12.h:739-745`.
- `zest__mtl_queue_families[3]`: graphics, compute, transfer, each with its queue label, and the explicit acquire lists (graphics; compute then graphics; transfer then compute then graphics).
- Hooks: `new_device_backend`, `initialise_device`, `query_device_capabilities`, `cleanup_device_backend`, `new_queue_backend`, `cleanup_queue_backend`, `create_execution_timeline_backend`, `cleanup_execution_timeline_backend`, `wait_for_timeline`, `wait_for_idle_device`, `imm_begin_command_buffer`, `imm_end_command_buffer`, `set_object_name`.
- `ZEST_API id<MTLDevice> zest_GetMetalDevice(zest_device device);` declared in `zest_metal.h`.
- Capabilities: tessellation and geometry false, buffer device address false, non-uniform indexing true. `max_bindless_samplers` is the device's argument buffer sampler limit.

**Steps:**
- [x] Device selection and the requirement check, with validation turned on the way Spike C found.
- [x] Queues and the residency set, attached to every queue at creation.
- [x] Timelines as shared events, with the CPU wait Spike B recorded, honouring `fence_wait_timeout_ns` and the timeout callbacks.
- [x] Immediate command buffers: begin takes a command buffer on the family's queue, end commits and waits.
- [x] Tests: "Metal Device Lifecycle" (create and destroy four times, allocated size after the fourth equal to the size after the third), "Metal Minimum Spec Rejected" (the forced hook makes creation fail and the log name the feature), "Metal Timeline Wait" (signal from a command buffer, wait satisfied, wait timed out).
- [x] Break each on purpose once (skip the release, skip the requirement check, never signal) and confirm its test fails.
- [x] Build all three targets and run.

### Task 8: Buffer pools, heaps and residency

**Files:** `zest_metal.h` section `Buffer_and_memory`.

**Interfaces:**
- Hooks: `create_buffer_allocator_backend`, `add_buffer_memory_pool`, `create_image_memory_pool`, `create_device_memory`, `get_memory_budget`, `map_memory`, `unmap_memory`, `flush_used_buffers`, `new_memory_pool_backend`, `new_memory_backend`, `cleanup_buffer_allocator_backend`, `cleanup_memory_pool_backend`, `cleanup_memory_backend`, `get_buffer_ptr`, `set_buffer_pool_name`.
- Storage by property flags:

  | Property flags | Metal buffer |
  | --- | --- |
  | device-local | private |
  | host-visible | shared, write-combined |
  | host-visible + host-cached | shared, default cache |
  | device-local + host-visible | shared |

- Every pool buffer and heap is untracked, and is added to the residency set when created and removed when freed, with one commit per change.
- Reported alignments: 256 for uniform offsets and 16 for storage offsets, the values D3D12 reports, so `element_alignment` padding behaves the same on all three.
- `backend_memory_bits` carries one heap category for now. If Task 9 finds images that can't share a heap, the categories go here.
- Dedicated image memory is a dedicated heap with the image at offset 0, which keeps the shared path and the allocation count the same as on the other backends.
- Budget: the device's recommended working set and current allocated size.

**Steps:**
- [x] Implement the hooks. `map_memory` returns the shared buffer's contents pointer plus the offset, and `flush_used_buffers` does nothing.
- [x] Test "Metal Buffer Pool": allocate host-visible buffers at two offsets in one pool, write through the mapping, copy to a device-local buffer and back with an immediate copy stubbed in for now by a blit encoder in the test's own helper, and compare. State in the test what is read back.
- [x] Test "Metal Residency Tracks Pools": the set's allocation count rises with a new pool and falls when it's freed.
- [x] Build and run.

### Task 9: Formats, images, views, samplers, uploads, readback

**Files:** `zest_metal.h` sections `Type_converters`, `Images`.

**Interfaces:**
- `zest__mtl_pixel_format(zest_format format)`: a table from `zest_format` to `MTLPixelFormat`. Formats with no Metal form (24-bit RGB, and D24 where the device lacks it) return invalid and report as unsupported.
- Hooks: `is_image_format_supported`, `set_depth_format`, `create_image`, `new_image_backend`, `cleanup_image_backend`, `create_image_view`, `create_image_views_per_mip`, `cleanup_image_view_backend`, `cleanup_image_view_array_backend`, `create_sampler`, `new_sampler_backend`, `cleanup_sampler_backend`, `transition_image`, `get_image_raw_layout`, `image_layout_is_valid_for_descriptor`, `copy_buffer_regions_to_image`, `copy_buffer_to_image`, `generate_mipmaps`, `copy_bitmap_to_image`, `copy_image_to_bitmap`, `cmd_copy_buffer_one_time`, `set_image_name`.
- Texture usage follows the Zest image flags: sampled, storage, attachment. The pixel-format-view usage is set only when a view of another format is asked for at creation time.
- Samplers are created for argument buffers.
- `transition_image` records the Zest layout in `zest_mtl_image_backend_t` and does no GPU work.
- Mip generation is native here, so unlike D3D12's M1 an image with mips is complete in M1.

**Steps:**
- [x] The format table and `is_image_format_supported`, checked against the device's capabilities for the usage asked.
- [x] Image creation as a placed texture, views, samplers.
- [x] Uploads, mip generation and readback through blit encoders on immediate command buffers.
- [x] Tests: "Metal Image Round Trip" (pixels in, read back, compared byte for byte, for an RGBA8 image, an image 8 pixels wide like the debug font, and a 1x1 image), "Metal Mip Chain" (upload a two-colour checker, generate mips, read back the last mip and check it's the average), "Metal Format Table" (every `zest_format` either maps and is creatable as a 4x4 texture, or reports unsupported, with no third outcome).
- [x] Break each once and confirm it fails.
- [x] Build and run.

### Task 10: The bindless table

**Files:** `zest_metal.h` section `Bindless_table`.

**Interfaces:**
- `zest_mtl_bindless_regions_t`: per slot, the byte offset and count, from the device's `bindless_*_count` settings. Every entry is 8 bytes (Spike A), so there is no per-slot entry size.
- `ZEST__MTL_VERTEX_BUFFER_INDEX 0`, `ZEST__MTL_PUSH_CONSTANT_BUFFER_INDEX 4`, `ZEST__MTL_BINDLESS_BUFFER_INDEX 5`, as Spike A proposed.
- Hooks: `create_set_layout`, `new_set_layout_backend`, `cleanup_set_layout_backend`, `create_set_pool`, `new_descriptor_pool_backend`, `create_bindless_set`, `update_bindless_image_descriptor`, `update_bindless_storage_buffer_descriptor`, `update_bindless_uniform_buffer_descriptor`, `build_pipeline_layout`, `new_pipeline_layout_backend`, `cleanup_pipeline_layout_backend`.
- A write is a store of the resource ID or of `gpuAddress + buffer->memory_offset`. The stride parameter of the storage buffer hook is accepted and ignored.
- A non-bindless set layout reports through `zest__mtl_report_unavailable`, as custom sets do on D3D12.

**Steps:**
- [x] Create the table buffer, add it to the residency set, lay out the regions.
- [x] Implement the three write hooks.
- [x] Test "Metal Bindless Slots": for each slot, write a distinct resource at the first, a middle and the last index, then run the spike's converted shader through a pipeline built in the test's own helper and read each back. Write one region's entries at twice the stride and confirm the test fails, as the spike's control did.
- [x] With Tasks 7 to 10 in place `zest_EndDeviceBuilder` should complete. Test "Metal Device With Defaults": the default images and the debug font exist and the font's first glyph row reads back as uploaded.
- [x] Build and run.

### Task 11: Headless contexts, the test list and the runner

**Files:** `zest_metal.h` sections `Backend_setup_functions`, `Backend_cleanup_functions`. `zest-tests.cpp:352-448`, `zest-tests.h`, `zest-metal-tests.cpp`.

**Interfaces:**
- Hooks: `new_context_backend`, `initialise_context_backend`, `cleanup_context_backend`, `new_context_queue_backend`, `initialise_context_queue_backend`, `cleanup_context_queue_backend`, `reset_queue_command_pool`, `queue_wait_idle`, `wait_for_fif_semaphore`, `wait_for_renderer_semaphore`, for a headless context only. The windowed paths stay stubs until M2.
- `zest_device_builder BeginMetalTestBuilder(void);` and `int RunMetalSuite(int *total_out);` in `zest-metal-tests.cpp`, mirroring `zest-d3d12-tests.cpp:21` and `RunD3D12Suite`. The table is `metal_tests[]`.
- `--platform` accepts `metal`. `all` includes it when it's compiled in. A new `--shader-validation` switch adds `MTL_SHADER_VALIDATION`, the counterpart of `--gbv`. It is applied at the top of `main`, before any suite creates a device, because Metal reads it once per process (Spike C).
- A summary line `Metal backend: N / M`, included in the exit code.

**Steps:**
- [x] Headless context creation and teardown.
- [x] Move the tests from Tasks 7 to 10 into `metal_tests[]` and add "Metal Headless Context" (create and destroy three times on one device, allocated size after the third equal to the size after the second), "Metal Untracked Pass Ordering" (Spike B's compute-then-read pair, 200 iterations) and "Metal Validation Reports" (one deliberate error, expect a count of one).
- [x] Remove the temporary call in `main` and wire the runner.
- [x] Run `zest-tests --platform all`, then `--platform metal --shader-validation`. Expected: the Vulkan lines match Task 1, and every Metal test passes in both.
- [x] Run `--platform metal` once with zombies and `MallocScribble` on. Expected: no report.

### Task 12: Milestone close

**Steps:**
- [x] Build `zest-tests`, `zest-c-compile-check` and `zest-objc-compile-check` with Metal ON, then `zest-tests` with it OFF. Expected: all clean, and OFF gives Task 1's counts.
- [x] Write `## As built (<date>)`: the spike numbers, every *(unverified)* row that turned out different, the buffer index values, the test counts, the full list of shared edits.
- [x] Hand Peter the Windows checks in [Testing_] and the questions M2's plan needs answered. Known ones: whether `SDL_Metal_CreateView` works on a window made without `SDL_WINDOW_METAL`, and whether a swapchain with `image_count` 1 whose image is re-pointed at each drawable satisfies every shared use of `views[current_image_frame]`.

## [Cost_] Per-frame cost

| Path | Vulkan and D3D12 | Metal, design target |
| --- | --- | --- |
| Per frame | Unchanged | One drawable acquire, one present, one autorelease pool per hook that creates an object |
| Per batch | Unchanged | One command buffer, its event waits and signals |
| Per pass | Unchanged | One encoder, one fence wait and one update, the bindless table bound with one ranged call per stage, viewport and scissor |
| Per draw or dispatch | Unchanged | The flush compares recorded state and sets only what changed. Push constants cost one copy of up to 128 bytes per stage that reads them. No allocation, no lock, no pool |
| Per bindless write | Unchanged | One 8-byte store |
| Per shader, first use | Unchanged | shaderc if from GLSL, then SPIRV-Cross, then Metal's compiler. Once per shader per run until M7's cache |
| GPU | Unchanged | Same shaders after conversion. Y is flipped by one negate in each vertex stage |

**Memory:** the bindless table is the sum of the nine regions, which at the default counts is tens of kilobytes. One residency set, three queues and one fence per queue for the device. A converted shader keeps its `MTLLibrary` and a few words of reflection for the device's life. No new buffers in shared code, and no struct in `zest.h` grows in M1.

## [Testing_] Testing

**Automated, on the Mac (implementer)**
- After every task: build `zest-tests`, `zest-c-compile-check` and `zest-objc-compile-check`, then `zest-tests --platform all`. Expected: `Dynamic rendering` and `Legacy render pass` equal Task 1's baseline, and every registered Metal test passes.
- After Tasks 7 to 11: `zest-tests --platform metal --shader-validation`.
- At Task 11 and Task 12: one run with zombies and `MallocScribble`.
- New M1 tests, all in `metal_tests[]`: Device Lifecycle, Minimum Spec Rejected, Timeline Wait, Buffer Pool, Residency Tracks Pools, Image Round Trip, Mip Chain, Format Table, Bindless Slots, Device With Defaults, Headless Context, Untracked Pass Ordering, Validation Reports.

**Peter's tests, on Windows, after M1's shared edits**
- **Windows baseline (2026-11-22, commit 60a0fec, AMD RX 6600):**
  - `--platform all`: `Dynamic rendering: 136 / 136`, `Legacy render pass: 136 / 136`, `D3D12 suite: 132 / 132`, `D3D12 backend: 103 / 103`, exit 0.
  - `--platform d3d12 --warp` and `--platform d3d12 --gbv`: `D3D12 suite: 132 / 132`, `D3D12 backend: 103 / 103`, exit 0.
  - Known noise in a passing run: 5 "Zest D3D12 Error" lines from the injected failures, 5 "Shader compilation failed" lines from the broken-shader tests, 2 Vulkan loader lines about a Twitch layer JSON file, and no VUID lines.
  - `zest-minimal-template.exe --validate --frames 600`: 0 validation errors and exit 0 on `--d3d12` (device release count 0) and on `--vulkan` (1 loader message ignored at creation).
  - `zest-c-compile-check` builds clean.
- After: `cmake --build E:/Projects/C++/Zest-Build --config Debug --target zest-tests`, then `zest-c-compile-check`, then `zest-tests.exe` with the default `--platform all`. Expected: the same four lines.
- One D3D12 run with `--gbv` and one with `--warp`, not overlapped.
- `zest-minimal-template.exe --validate --frames 600` on each backend.

**Visual and usability checks for Peter**
- None in M1: nothing draws to a window until M2. M2's plan lists them.

## [Digest_] Code style digest for the Mac

For use only if the `code-style` skill isn't on the Mac. The skill wins where they differ.

- C that also compiles as C++: C-style casts, `NULL`, `ZEST__ZERO_INIT`, no `auto`, nothing from `std::`, no classes, lambdas or namespaces. Objective-C syntax only for Metal calls.
- Full names everywhere (`layer_index`, not `idx`). Loop counters are named for what they count. Existing short forms (`fif`, `ctx`) stay.
- `for (int layer = 0; layer != count; ++layer)`. Tabs. K&R braces. `Type *name`. No line wrapping.
- Comments are `//Text` with no space, one line, only for a reason or a hazard. Never mention plans, phases, to-dos or history. ASCII only.
- Private functions use `ZEST_PRIVATE`, never plain `static`. Public functions assert the handle first with a trailing comment saying why it fires.
- Zest's own containers (`zest_vec`, `zest_hash_map`), with the allocator passed on growth, freed on every exit path. No `malloc`.
- Flag enums: `_none = 0`, then `1 << n`, column-aligned.
- Log messages are whole sentences that tell the user what to do.
- No new dependency and no code for a niche case without asking Peter. Document the limit instead.
- Find the closest existing code and mirror it, but don't copy `zest_dx12.h`'s style: it's agent-written.

## [Built_] As built

### Task 1: Mac baseline (2026-11-22, commit 60a0fec, run over SSH from the Windows machine)

**Machine**
- Mac mini, Apple M2, 8 GB. macOS 26.2 (25C56). Xcode 26.2 (17C52), Apple clang 17.0.0. CMake 4.2.2, SDL2 2.32.10 from Homebrew. No ninja and no pkg-config.
- Vulkan SDK 1.4.335.1 at `~/VulkanSDK/1.4.335.1`, with `VULKAN_SDK` pointing at its `macOS` folder. It holds `include/spirv_cross/spirv_cross_c.h`, `lib/libspirv-cross-c.a` with the `core`, `glsl`, `msl`, `reflect`, `util`, `cpp` and `hlsl` static libraries, `libspirv-cross-c-shared.dylib` (0.67.0), `libshaderc_combined.a`, `libMoltenVK.dylib`, Slang 2025.23.2, and the `glslc`, `spirv-cross`, `slangc` and `dxc` tools. So Task 2 has everything it needs.
- The repo is `~/Documents/GitHub/zest`. An old in-tree `build/` folder from a ninja configure is still there and isn't used.

**Differences from the plan's commands**
- The default generator is Unix Makefiles, which is single-config. Configure with `cmake -S . -B ../Zest-Build -DCMAKE_BUILD_TYPE=Debug`, and build with `cmake --build ../Zest-Build --target <target> -j 8`, with no `--config`. Executables are in `../Zest-Build/examples/SDL2/`, with no `Debug` folder.
- `VULKAN_SDK` and the Homebrew path come from `~/.zshrc` and `~/.zprofile`. A non-interactive SSH command has neither until it sources both.
- A test or example launched from an SSH session opens its window and runs, as long as the user is logged in at the console.
- `git pull --ff-only` in the Mac clone fails with "Cannot fast-forward to multiple branches". The clone is at the same commit as Windows, so nothing was pulled. The clone's pull configuration needs a look.
- Slang is off in a default configure ("Slang support disabled"), so the Slang tests aren't registered. That's why the Mac registers 129 tests where Windows registers 136. `-DZEST_ENABLE_SLANG=ON` is needed for M6 and for Task 2's optional Slang step.

**Results**
- The tree builds as it is: `zest-tests`, `zest-minimal-template`, `zest-imgui-template` and `zest-c-compile-check`, with no errors and no warnings. The D3D12 work left no Mac build regression.
- `zest-tests`, exit 1: `Dynamic rendering: 129 / 129`, `Legacy render pass: 127 / 129`.
- The two failures, in the legacy run only:
  - "Pipeline Test State Polygon Mode"
  - "Pipeline Test State Rasterization"

  Both build a pipeline with the point polygon mode, and MoltenVK's portability subset rejects it (VUID-VkPipelineRasterizationStateCreateInfo-pointPolygons-04458). The same two tests pass in the dynamic rendering run with no VUID line. Whether they failed before the D3D12 work isn't known. Nothing was changed: the Metal baseline is 129 and 127 until Peter rules on them.
- Known noise in the Mac log: Vulkan loader lines for the ICD and validation layer search, 2 "Shader compilation failed" lines from the broken-shader test, 2 VUID blocks from the failures above, and repeated "Cannot cache the frame graph this frame" lines in the stress tests.
- `zest-minimal-template --validate --frames 600` and `zest-imgui-template --validate --frames 600`: exit 0, 0 validation errors, 0 messages ignored at creation.
- **Fixed the same day (Peter's ruling):** both tests skip the point polygon mode on Apple (`zest-pipeline-tests.cpp`, in `test__pipeline_state_polygon_mode` and `test__pipeline_state_rasterization`). With the skip the Mac gives `Dynamic rendering: 129 / 129`, `Legacy render pass: 129 / 129`, exit 0, with no VUID line. That is the baseline for M1. Windows is unaffected: the skip is inside `#if defined(__APPLE__)`.

### Task 2: Spike A, shaders and the bindless table (2026-11-22, commit e4cbf7c, run over SSH from the Windows machine)

**Verdict: pass.** It depends on the alias workaround, which Peter approved the same day (decision 6 under [Decisions_]).

**What ran**
- `spikes/metal/spike_shaders.mm`, `spikes/metal/build_spike_shaders.sh` and five GLSL shaders in `spikes/metal/shaders/`. All untracked, and on the Mac only. The script compiles every shader, builds the spike and runs it. From the repo root: `./spikes/metal/build_spike_shaders.sh`.
- Logs are in `spikes/metal/build/`: `run_final.log`, `run_debug_layer.log`, `run_shader_validation.log` and `run_no_alias_patch.log`. The generated MSL is in `build/msl/`.
- 97 SPIR-V modules: the 67 GLSL files through `glslc -DZEST_METAL=1`, 25 Slang entry points through `slangc -target spirv -profile spirv_1_5` (20 from `timelinefx.slang`, 5 from `particle.slang`) and the spike's 5. By stage: 37 vertex, 39 fragment, 21 compute.
- `kernel.slang` was left out. Its entry points only link through a Slang session that supplies the collider module, which `slangc` alone can't do. M6 covers it.

**The option set (SPIRV-Cross C API 0.67.0)**
- `SPVC_COMPILER_OPTION_MSL_VERSION` 3.1.0, `MSL_PLATFORM` macOS, `MSL_ARGUMENT_BUFFERS` true, `MSL_ARGUMENT_BUFFERS_TIER` 1 (which is tier 2), `FLIP_VERTEX_Y` true.
- `spvc_compiler_msl_add_discrete_descriptor_set(compiler, 0)`.
- One `spvc_msl_resource_binding_2` per slot, for the shader's stage: `desc_set` 0, `binding` the slot, `count` 0 (which keeps the array runtime sized), and `msl_buffer`, `msl_texture` and `msl_sampler` all 5 + slot.
- One more for push constants: `desc_set` `SPVC_MSL_PUSH_CONSTANT_DESC_SET`, `binding` `SPVC_MSL_PUSH_CONSTANT_BINDING`, `msl_buffer` 4.
- The entry point's MSL name comes from `spvc_compiler_get_cleansed_entry_point_name` (`main` becomes `main0`).
- Metal side: `MTLCompileOptions.languageVersion = MTLLanguageVersion3_1`, everything else default.
- Link order: `spirv-cross-c`, `-msl`, `-hlsl`, `-cpp`, `-reflect`, `-glsl`, `-core`. The C wrapper references every backend, so the link fails without hlsl, cpp and reflect. `-util` isn't needed.

**Set 0 is discrete bindings, not one argument buffer**
- Discrete: each slot's runtime array becomes `device const void* spvDescriptorSet0Binding<slot> [[buffer(5 + slot)]]`, cast in the shader to an array of 8-byte descriptors. This is the mode that works.
- One argument buffer (set 0 in device address space, ids at slot x 64): all 97 convert and compile, but the struct is 72 bytes, one 8-byte member per array in declaration order whatever the ids say, so a slot can't have a region. The table test reads zeros. Rejected.

**The table**
- Every entry is 8 bytes in all nine slots. 16-byte buffer entries exist only in SPIRV-Cross's rich descriptor mode, which the C API doesn't expose.
- One shared `MTLBuffer` holds the table, each slot's region at its own offset. It binds with one ranged call per stage over indices 5 to 13: `setVertexBuffers:offsets:withRange:`, `setFragmentBuffers:offsets:withRange:` and the compute encoder's `setBuffers:offsets:withRange:`.
- Samplers need `supportArgumentBuffers = YES`.
- Two block types at one storage binding (as `buffer_verify.comp` has) share one pointer, so structured and raw indexes are the same thing, as on Vulkan.

| Slot | Kind | MSL descriptor type | Entry | Buffer index |
| --- | --- | --- | --- | --- |
| 0 | Samplers | `sampler` | 8 bytes, `gpuResourceID` | 5 |
| 1 | 2D textures | `texture2d<float>` | 8 bytes, `gpuResourceID` | 6 |
| 2 | Cube textures | `texturecube<float>` | 8 bytes, `gpuResourceID` | 7 |
| 3 | Array textures | `texture2d_array<float>` | 8 bytes, `gpuResourceID` | 8 |
| 4 | 3D textures | `texture3d<float>` | 8 bytes, `gpuResourceID` | 9 |
| 5 | Storage buffers | `device T *` | 8 bytes, `gpuAddress + offset` | 10 |
| 6 | Storage images | `texture2d<float, access::write>` and the other access forms | 8 bytes, `gpuResourceID` | 11 |
| 7 | Uniform buffers | `constant T *` | 8 bytes, `gpuAddress + offset` | 12 |
| 8 | Cube array textures | `texturecube_array<float>` | 8 bytes, `gpuResourceID` | 13 |

**Proposed indices**
- `ZEST__MTL_VERTEX_BUFFER_INDEX 0`: vertex buffer binding N at index N, for bindings 0 to 3.
- `ZEST__MTL_PUSH_CONSTANT_BUFFER_INDEX 4`.
- `ZEST__MTL_BINDLESS_BUFFER_INDEX 5`: slot N at index 5 + N, so 5 to 13.
- SPIRV-Cross's own buffers keep its defaults, all above 13: the view mask at 24, buffer sizes at 25, swizzle at 30. None of the 97 needs the buffer size or swizzle buffer.
- No collision in any of the 97. The spike checks every `[[buffer(N)]]` of every entry point against this plan, and no shader declares a discrete `[[texture]]` or `[[sampler]]`.
- Slots in use: vertex shaders 5 and 7, fragment shaders 0, 1, 2, 3, 5 and 7, compute shaders all nine.

**Results**
- Converted: 97 / 97. Compiled with `newLibraryWithSource`: 97 / 97 with the alias workaround below and no warnings, 94 / 97 without it (the two repo shaders and the spike's own copy of the pattern). All 21 compute shaders also build a compute pipeline.
- Table, compute (`spike_slots.comp`, all nine slots in one kernel): passes with the resources at the first index, at ten different middle indices and at the last index of a 64-entry region. Every other entry of every region held a decoy of the same kind, and the decoy image and decoy buffer stayed untouched. Control: with entries written as if they were 16 bytes the checks fail, as they must.
- Table, render (`spike_table.vert` and `.frag`): a uniform buffer read in the vertex stage, a texture, sampler, storage buffer and two uniform block types read in the fragment stage, push constants in both, and vertex buffers at indices 0 and 1 suballocated from one buffer. Four probe pixels match within 1.
- Local size: `spvc_compiler_get_execution_mode_argument_by_index(compiler, SpvExecutionModeLocalSize, axis)` gives 8 8 1, and the dispatch uses it.
- Viewport and facing against MoltenVK, same SPIR-V, 8x8 target, a triangle over the top-left half: bytes identical in all three cases. Culling off covers 28 pixels on both. Cull back with front face clockwise draws on both, and with counter-clockwise draws on neither. So exactly one winding draws, and the Zest front face maps straight to the Metal winding.
- Base instance 5: `gl_InstanceIndex` reads 5 and a per-instance attribute reads element 5, on both.
- With `MTL_DEBUG_LAYER=1`, and with `MTL_SHADER_VALIDATION=1` added, both set by the launching shell: no message, same results, exit 0.

**Found on the way**
- **Specialisation constants.** `genbrdflut.comp` has one, and Metal won't build a pipeline from the plain function. `newFunctionWithName:constantValues:error:` with an empty `MTLFunctionConstantValues` keeps the SPIR-V default and works for every shader.
- **The uniform alias bug (decision 6).** Two uniform block types sharing binding 7 in one shader make SPIRV-Cross declare the shared pointer as `constant void* spvBufferAliasSet0Binding7`, and its own `spvDescriptorArray` only constructs from a `device` pointer. Metal reports "no matching constructor". The two shaders are `scene.frag` in zest-cascading-shadows and `deferred_lighting.frag` in zest-pbr-deferred. The same pattern at a storage binding works, because that pointer is `device`. SPIRV-Cross `main` still emits it (`spirv_msl.cpp`, the line building `get_variable_address_space(var) + " void* "`), so a newer SDK won't help. Replacing `constant void* spvBufferAliasSet` with `device const void* spvBufferAliasSet` in the MSL fixes all three.
- **Arrays indexed only by constants (decision 7).** glslang sizes such an array, and SPIRV-Cross then emits `array<texture2d<float>, 3> textures [[texture(0)]]`. `depthpass.frag` in zest-cascading-shadows declares one array it never uses, which is dropped, so nothing in the repo is affected.
- **`.length()` on a bindless storage buffer (decision 8).** SPIRV-Cross throws "OpArrayLength requires rich descriptor format".
- **Multiview.** Without `SPVC_COMPILER_OPTION_MSL_MULTIVIEW`, `depthpass.vert` gets `gl_ViewIndex = 0`. With it the shader takes a view mask buffer at index 24 and draws the views as instances into layers. That's M5's to wire.
- zsh doesn't split an unquoted variable into words, so a flag list held in a variable reaches a tool as one argument. The build script is bash for that reason.

**Not covered here**
- The spike calls `useResources` for everything reached through the table. Whether a residency set replaces that is Spike B's question.
- Fast math stays at Metal's default, which is on. Nothing here measures what it does to shader results. M4.
- Only the spike's two shader pairs were built into render pipelines. The repo's vertex and fragment shaders were compiled to libraries. M4.
- The GLSL embedded in `zest.h` and the implementations (the debug font, ImGui) wasn't extracted. It uses the same declarations.

### Task 3: Spike B, memory, residency and ordering (2026-11-22, commit e4cbf7c, run over SSH from the Windows machine)

**Verdict: pass.** One pass line changed: allocated size doesn't return to its value before the first allocation, it returns to a steady value from the second lifecycle on. See "Allocated size" below.

**What ran**
- `spikes/metal/spike_memory.mm` and `spikes/metal/build_spike_memory.sh`, untracked, on the Mac only. The shaders are MSL text inside the file, with table arguments in the shape Spike A's conversion gives. From the repo root: `./spikes/metal/build_spike_memory.sh`, which builds it and runs the three real variants and three controls.
- Logs in `spikes/metal/build/`: `memory_plain.log`, `memory_debug_layer.log`, `memory_shader_validation.log`, `memory_control_no_fence.log`, `memory_control_no_residency.log`, `memory_control_no_residency_validation.log`.
- The whole lifecycle (create, run, release) runs five times in one process. Each lifecycle runs six chains of 200 command buffers and the shared event test.

**Results, the same with no validation, with `MTL_DEBUG_LAYER=1`, and with `MTL_SHADER_VALIDATION=1` added: exit 0, no message**
- **Placement heap.** `MTLHeapTypePlacement`, private storage, `MTLHazardTrackingModeUntracked`. `heapTextureSizeAndAlignWithDescriptor:` gives the size and alignment, and `newTextureWithDescriptor:offset:` places the texture. A third texture placed over the first's range while the first is alive is accepted, and the two are used on alternate iterations with no fault. A placed texture reports the heap's untracked mode without setting it on the descriptor.
- **Sizes the device reports** (Apple M2):

  | Texture | Size | Alignment |
  | --- | --- | --- |
  | RGBA8 16x16, read, write and target | 1,024 | 128 |
  | RGBA8 1024x1024 with 11 mips, read | 5,636,608 | 128 |
  | RGBA16F 1920x1920, read and target | 29,622,272 | 128 |
  | Depth32F 1024x1024, read and target | 4,227,072 | 128 |
  | A 64 KB private buffer in a heap | 65,536 | 256 |

- **Pool buffer.** One shared buffer with `MTLResourceHazardTrackingModeUntracked`, and two table entries at `gpuAddress + 4096` and `gpuAddress + 8192`, used on alternate iterations.
- **Residency.** One set holding the heap, the pool, the table and the readback buffer, committed once and attached to three queues with `addResidencySet:`. `useResource` and `useHeap` are called nowhere. A compute pass writes the pool buffer and a heap texture through the table, and a render pass samples that texture and reads the buffer through the table: correct.
- **Ordering.** Each encoder waits on its queue's fence when it opens and updates it when it ends: compute and blit encoders with `waitForFence:` and `updateFence:`, render encoders with `waitForFence:beforeStages:MTLRenderStageVertex` and `updateFence:afterStages:MTLRenderStageFragment`. Wait then update on one fence in one encoder is accepted. Every chain reads back right, 200 of 200, in each of the five lifecycles:
  - compute writes, render reads, copy out: on queue 0 waiting for each command buffer, then on queues 0, 1 and 2 with all 200 committed back to back
  - a render pass draws into a heap texture and the next samples it: waited for each, then back to back
- **Shared events.** Queue 0's command buffer encodes `encodeWaitForEvent:value:1` and is committed first. 200 ms later its status is still committed (2), not completed. Queue 1's command buffer then writes and encodes `encodeSignalEvent:value:1`, and queue 0 reads what it wrote.
- **Timed CPU wait.** `-[MTLSharedEvent waitUntilSignaledValue:timeoutMS:]`. With nothing signalling it returns NO after 252 ms of a 250 ms timeout. After a command buffer signals, it returns YES in 0.1 ms. A command buffer holding only a signal and no encoder is valid.

**Controls**
- **No fences** (`--no-fence`): the chains fail. Compute then render in one command buffer reads the previous iteration's data: fewer than 10 of 200 runs are right, whether each command buffer is waited for or not. Render then render mostly survives when each is waited for (199 or 200 of 200) and fails back to back (3 to 5 of 200). So untracked passes really do overlap on this GPU, and the fence is what orders them.
- **Residency set not attached** (`--no-residency`): each compute chain gets 1 to 3 runs of 200 wrong and the shared event read fails, while the render chains pass. With shader validation on, every run of every chain fails. Neither prints anything to the console: without shader validation a resource missing from the set is a silent and intermittent wrong result, which is why "Metal Residency Tracks Pools" has to stay in the suite. With shader validation the command buffer's `logs` do report it, which Spike B didn't read and Spike C did.

**Allocated size**
- `currentAllocatedSize` is 65,536 before the first allocation and 819,200 with everything alive.
- After the last release of each lifecycle: 737,280, then 393,216 four times. Under shader validation: 28,917,760, then 28,655,616 four times.
- A separate probe (`spikes/metal/build/probe/probe_allocated.mm`) shows why. The first placed texture and the first private texture each leave storage the device keeps (the size goes from 65,536 to 393,216 and stays there), and a second round of the same objects returns exactly to 393,216. Buffers, heaps, queues, fences and events all return what they took.
- The first lifecycle's remainder is freed late, by the second or third lifecycle. In one control run the size settled from the third lifecycle, not the second.
- So a leak test can't use the size before the first allocation as its baseline, and can't trust the reading straight after the first release. The M1 tests compare two later lifecycles with each other. Review Focus 3, "Metal Device Lifecycle" and "Metal Headless Context" now say so.

**Carried into the backend**
- Fences are per queue. Nothing here used one fence from two queues, and the design doesn't need it: the shared event orders a crossing.
- The spike used retained command buffers (`commandBuffer`). The backend's unretained ones are M2's to confirm.
- Aliased textures were only ever written whole before being read. A load action of load on an aliased texture is undefined, as on the other backends.

### Task 4: Spike C, validation and leaks (2026-11-22, commit e4cbf7c, run over SSH from the Windows machine)

**Verdict: pass.** Validation can be turned on from inside the process, and both kinds of message can be counted inside it. API errors are logged and counted, which Peter chose the same day (decision 9 under [Decisions_]).

**What ran**
- `spikes/metal/spike_validation.mm` and `spikes/metal/build_spike_validation.sh`, untracked, on the Mac only. The script runs 16 scenarios, each its own process, because validation is decided once per process. Logs are `spikes/metal/build/validation_NN_*.log`.
- One run of the existing `zest-tests` under the debug layer: `spikes/metal/build/zest_tests_debug_layer_nslog.log`.
- The rules are in `man MetalValidation` on the Mac, which is worth reading before Task 7.

**Turning it on**
- `setenv("MTL_DEBUG_LAYER", "1", 1)` at the start of `main`, before the first device, works exactly like a launcher setting it. Metal prints "Metal API Validation Enabled" through `NSLog` when the device is created, and the scissor error is caught.
- `setenv` after a device has been created does nothing, even when that device has been released. No message, no warning, and the scissor error passes unreported.
- `MTL_SHADER_VALIDATION=1` follows the same rule, prints "Metal GPU Validation Enabled", and works with or without the debug layer.
- `MTLCreateSystemDefaultDevice` returns the same object every time, and `MTLCopyAllDevices` lists that one. So there is one Metal device per process, every Zest device shares it, and validation is on or off for all of them from the first creation. A Zest device created without the validation flag after one created with it is still validated.
- Re-executing the process wasn't needed and wasn't tested.

**API errors (the debug layer)**
- Without validation, a scissor rect of {4, 4, 100, 100} on an 8x8 target is accepted and the command buffer completes with no error.
- Default mode: the process aborts at the call (exit 134), having written `-[MTLDebugRenderCommandEncoder setScissorRect:]:4129: failed assertion` and then "Set Scissor Rect Validation" with the two failed conditions.
- `MTL_DEBUG_LAYER_ERROR_MODE=nslog`: one `NSLog` record is written to standard error and the call returns. The command buffer still completes with no error. The record is the usual `NSLog` prefix (`2026-11-22 17:50:06.183 spike_validation[76456:1841250] `), then a title line ending in "Validation", then the detail lines with no prefix.
- A pipe placed over standard error inside the process reads those records back whole. So an in-process reader can count them.
- The unified log is no use for this: `OSLogStore` with the current process scope does return the entries, but the validation text reads `<private>`.

**Shader errors (shader validation)**
- An out-of-bounds read of a bound buffer, and a read and write of a buffer reached only through a pointer with nothing making it resident: in both cases the command buffer's status is completed and its `error` is nil. Reads give 0 and writes are dropped.
- `commandBuffer.logs` holds one `MTLFunctionLog` per fault: type validation, the encoder's label, the function, and a debug location with line and column. Its description reads "Invalid device load at offset 400000, executing kernel function: ..." with the buffer's length and `resident:Read Write`, or `resident:No` for the buffer that wasn't resident.
- Nothing reaches standard error unless `MTL_SHADER_VALIDATION_REPORT_TO_STDERR=1` is set, when the same text is written there.
- So shader validation messages are counted from `logs` when a command buffer completes, with no scraping at all. Labels on encoders and pipelines make the text useful, which M7 adds.

**Knowing it's on**
- The device's class is no guide: it is `CaptureMTLDevice` in every scenario, with or without validation.
- Walking the wrappers through the private `baseObject` selector does tell them apart: `CaptureMTLDevice AGXG14GDevice` with nothing on, `MTLDebugDevice` in the chain with the debug layer, `MTLGPUDebugDevice` with shader validation.
- A public way, given the reader on standard error: `newBufferWithLength:0` returns nil either way, and with the debug layer in log mode it writes one "Buffer Validation" record. No pass or command buffer is needed.

**MoltenVK under the debug layer**
- `MTL_DEBUG_LAYER=1 MTL_DEBUG_LAYER_ERROR_MODE=nslog zest-tests` from the repo root: `Dynamic rendering: 129 / 129`, `Legacy render pass: 129 / 129`, exit 0.
- Each of the two runs raises one debug layer error, in "User Test Swapchain Resize": "RenderPass Descriptor Validation, renderTargetWidth (1280) must be <= minimum attachment width (640), renderTargetHeight (768) must be <= minimum attachment height (400)". In the default abort mode that would end the Vulkan run there.
- Nothing was changed. Whether it is MoltenVK's own slip after a resize or a render area Zest's Vulkan path leaves stale is not known: the Vulkan validation layer reports nothing for the same test. It goes to Peter as found, with that command as the repro.

**What this means for the tasks ahead**
- Task 7: the device applies the variables before creating the Metal device and reports when validation was asked for and didn't come on, which is the case `zest__mtl_forced_validation_unavailable` forces.
- Task 11: `zest-tests` has to apply `--shader-validation`, and Metal validation generally, at the top of `main`. With `--platform all` the Vulkan suites then run under the debug layer too, and the "RenderPass Descriptor Validation" record is known noise in that run.
- A completed command buffer has to have its `logs` read when shader validation is on. That costs nothing when it's off.

### Tasks 5 and 6: build and shared wiring (2026-11-22, on top of commit e4cbf7c, built on the Mac over SSH and on Windows)

**What changed**
- `cmake/ZestMetal.cmake` (new): the `ZEST_ENABLE_METAL` option, off by default and Apple only. It finds `spirv_cross/spirv_cross_c.h` and the seven SPIRV-Cross libraries under `VULKAN_SDK`, sets `ZEST_METAL_ENABLED`, `ZEST_METAL_SPIRV_CROSS_INCLUDE_DIR`, `ZEST_METAL_SPIRV_CROSS_LIBRARIES` and `ZEST_METAL_FRAMEWORKS`, raises the deployment target to 15.0, and defines `zest_metal_implementation_source(<target> <file>)`.
- `CMakeLists.txt`: a Metal block after D3D12's, which adds the `ZEST_ENABLE_METAL` definition, the include folder, the libraries and frameworks to `zest` and `zest-interface`, and marks `zest.cpp`.
- `examples/SDL2/CMakeLists.txt`: `buildExample` marks each example's main file, and a `zest-objc-compile-check` target exists when Metal is on.
- `examples/SDL2/zest-tests/zest-objc-compile-check.m` (new): `zest.h` with the Vulkan and Metal implementations, compiled as Objective-C with `-std=c11`.
- `zest.cpp` and `examples/SDL2/zest-tests/zest-tests.cpp`: define `ZEST_METAL_IMPLEMENTATION` under `ZEST_ENABLE_METAL`.
- `zest.h`, four edits: the `zest_BeginMetalDeviceBuilder` declaration, `zest_platform_metal` in `zest_EndDeviceBuilder`'s case list, a SPIR-V magic case for Metal in `zest__shader_binary_matches_platform`, and the `zest_metal.h` include.
- `zest_metal.h` (new, 707 lines): the contents list with a marker for every section, the two `#error` guards, `ZEST_MTL_BACKEND`, `ZEST_MTL_RELEASE`, `zest_BeginMetalDeviceBuilder`, `zest__mtl_report_unavailable`, `zest__mtl_initialise_platform_callbacks`, and a prototype and a body for 161 of the 164 members.

**Results**
- Mac, Metal off (`../Zest-Build`): every target builds, `zest-tests` gives `Dynamic rendering: 129 / 129` and `Legacy render pass: 129 / 129`, exit 0, no VUID line. That is Task 1's baseline.
- Mac, Metal on (`../Zest-Build-Metal`, configured with `-DZEST_ENABLE_METAL=ON -DCMAKE_BUILD_TYPE=Debug`): the configure prints "Metal support enabled". All 22 targets build, including `zest-c-compile-check` and `zest-objc-compile-check`, with no warning from Zest's own files. `zest-tests` gives 129 / 129 and 129 / 129, exit 0.
- The compile commands show 18 files compiled with `-x objective-c++ -fno-objc-arc` (`zest.cpp` and the 17 example main files), the check compiled with `-x objective-c -fno-objc-arc -std=c11`, and `-mmacosx-version-min=15.0` on every command.
- A throwaway program (`spikes/metal/metal_device_smoke.mm`, untracked) calls `zest_BeginMetalDeviceBuilder` and `zest_EndDeviceBuilder` in test mode. It gets NULL back and three validation errors, for `new_device_backend`, `initialise_device` and `cleanup_device_backend`, with no crash.
- Windows: the configure runs with Metal off, and `zest-tests`, `zest-c-compile-check` and `zest` build in Debug with no warning and no error. The suites were not run: that is Peter's check below.
- The 9 `sprintf` deprecation warnings from `examples/libs/stb_image_write.h` appear with Metal off as well. They were there before this work.

**Where it differs from the plan**
- **Three members are left unset, not one.** `begin_debug_marker` and `end_debug_marker` join `flush_legacy_framebuffers`. Shared code checks all three before calling (`zest.h:17556`, `zest.h:17685`, `zest.h:10244`), and a reporting stub for the markers would raise an error on every pass until M7 implements them.
- **`zest__mtl_report_unavailable` takes a NULL device.** Nine members have no device to report on: four backend allocators that only get an allocator, three profiler hooks and two debug pointer getters. For those it prints a "Zest Metal Error" line and returns. With a device it logs a validation error in test mode and asserts otherwise, as D3D12's does.
- **It doesn't use `ZEST_ASSERT_OR_VALIDATE`.** Outside test mode that macro passes the message to `fprintf` as the format, which clang warns about. The function prints the message with `ZEST_ALERT` and then asserts with a fixed string.
- **`zest-tests.cpp` defines the Metal implementation now**, which the plan left to Task 7. Without it `zest_metal.h` isn't compiled into the test runner and "build `zest-tests` with Metal on" checks nothing.
- **The mismatch message needed no edit.** It already says SPIR-V for every platform that isn't D3D12.
- **Task 5's last step wasn't observed.** Tasks 5 and 6 were both in place before the first configure with Metal on, so the "fails only because `zest_metal.h` doesn't exist" build never ran.
- **The stubs were generated.** A script read `zest_platform_t` out of `zest.h` and wrote a prototype, a stub and an assignment per member, so the signatures match the table exactly. The script isn't in the repo, and the file is now hand-edited like any other.
- **The implementation file is marked with compile flags, not a language.** `zest_metal_implementation_source` appends `-x objective-c++ -fno-objc-arc` (or `-x objective-c` for a `.c` file) to the one source file, which needs no `enable_language(OBJCXX)` and works with the project's minimum CMake.
- **Two build folders on the Mac.** `../Zest-Build` stays Metal off for the baseline and `../Zest-Build-Metal` is Metal on. The generator is single-config, so each needs `-DCMAKE_BUILD_TYPE=Debug`.

**Shared edits for Peter's Windows run**
- `zest.h`: one declaration, one case label in `zest_EndDeviceBuilder`, one case in `zest__shader_binary_matches_platform`, one include inside `#ifdef ZEST_METAL_IMPLEMENTATION`.
- `zest.cpp`, `zest-tests.cpp`: three lines each inside `#ifdef ZEST_ENABLE_METAL`, which Windows never defines.
- `CMakeLists.txt`, `examples/SDL2/CMakeLists.txt`: the Metal block and the two calls, all no-ops when `ZEST_METAL_ENABLED` is false.
- No existing Vulkan or D3D12 line changes meaning. Expected on Windows: the four baseline lines in [Testing_].

### Tasks 7 to 12: device bring-up and M1 close (2026-11-22, on top of commit df5c4da, built and run on the Mac over SSH)

**M1 is done.** A Metal device and a headless context create and destroy with validation silent and allocated size steady, an image with pixels reads back unchanged, and the Mac's Vulkan runs match Task 1's baseline.

**What changed**
- `zest_metal.h`, now 2,666 lines. 72 of the 160 stubs are replaced and 88 remain for M2 onward.
  - Task 7: the validation reader, device creation with the minimum spec check, capabilities, three queue families, shared-event timelines, immediate command buffers, `zest_GetMetalDevice`.
  - Task 8: buffer pools, image heaps, dedicated image memory, the residency set, the memory budget.
  - Task 9: the format table, images placed in heaps, views, samplers, uploads, mip generation, readback.
  - Task 10: the bindless table and its three write hooks, the pipeline layout.
  - Task 11: headless contexts and context queues.
- `examples/SDL2/zest-tests/zest-metal-tests.cpp` (new, 1,004 lines): 15 backend tests, `BeginMetalTestBuilder`, `RunMetalSuite`, `RunMetalSuiteInChildProcess`.
- `examples/SDL2/zest-tests/zest-tests.cpp`: `--platform metal`, `--shader-validation`, the `Metal backend: N / M` line and its place in the exit code.
- `zest.h`: one new platform member, `collect_validation_messages`, called from `zest_GetValidationErrorCount` and `zest_ResetValidationErrors` when a backend sets it.

**Results**
- Mac, Metal on (`../Zest-Build-Metal`):
  - All 22 targets build, with `zest-c-compile-check` and `zest-objc-compile-check`, and no warning from Zest's files.
  - `zest-tests --platform all`: `Dynamic rendering: 129 / 129`, `Legacy render pass: 129 / 129`, `Metal backend: 15 / 15`, exit 0, no VUID line.
  - `zest-tests --platform metal`: 15 / 15, with Metal API validation confirmed on by the first test.
  - `zest-tests --platform metal --shader-validation`: 15 / 15, with the deliberate out-of-bounds read counted from the command buffer's logs.
  - `NSZombieEnabled=YES MallocScribble=1 zest-tests --platform metal`: 15 / 15, no zombie report.
- Mac, Metal off (`../Zest-Build`): every target builds, and `zest-tests` gives 129 / 129 and 129 / 129, exit 0, no VUID line. That is Task 1's baseline.
- Windows: `zest-tests`, `zest-c-compile-check` and `zest` build in Debug with no warning and no error. The suites were not run.

**Every test was broken on purpose and failed**
A script on the Mac (`spikes/metal/mutate.py`, untracked) made one change at a time, rebuilt and ran the Metal list.

| What was broken | Tests that failed |
| --- | --- |
| A pool buffer is never released | Device Lifecycle |
| An image heap is never released | Device Lifecycle |
| A texture is never released | Device Lifecycle |
| The minimum spec check is skipped | Minimum Spec Rejected |
| A timeline poll always succeeds | Timeline Wait |
| A buffer copy ignores its destination offset | Buffer Pool |
| Nothing is added to the residency set | Residency Tracks Pools, Bindless Slots, Untracked Pass Ordering |
| An image upload reads 4 bytes late | Nearly all, through the validation error it raises at device creation |
| Mipmaps are never generated | Mip Chain |
| 24-bit RGB is mapped to RGBA8 | Format Table |
| Table entries are written at twice the stride | Bindless Slots, Untracked Pass Ordering |
| Every layer is uploaded to slice 0 | Bindless Slots, Device With Defaults |
| Validation messages are never collected | Validation Reports |
| The ordering test's passes use no fence | Untracked Pass Ordering |

Two tests were weak when first written and were changed because of this:
- **Device Lifecycle** didn't notice a leaked pool buffer. Metal serves small buffers from storage of its own, so a few kilobytes leaked per lifecycle never moved `currentAllocatedSize`. Each lifecycle now also allocates a 256 KB staging buffer and uploads a 256x256 image.
- **Untracked Pass Ordering** passed with its fences removed, because it chained two compute passes and those don't overlap. It is now compute writes, render reads, copy out, which is the pair Spike B measured overlapping.

One check still can't fail: "Metal Headless Context" compares allocated size across contexts, and a headless context owns no Metal object yet. It will mean something from M2.

**Where it differs from the plan**
- **The tests ran together, not task by task.** A device can't finish creating until the bindless table exists, so nothing that needs a device could run before Task 10. Each task was built on its own, a throwaway program showed how far device creation got, and the 15 tests first ran once Task 11's code was in.
- **A fourth shared edit: `collect_validation_messages`.** Metal's debug layer has no callback, so its messages have to be fetched when the count is asked for. The member is unset on Vulkan and D3D12 and checked before the call.
- **The Metal list runs in a process of its own under `--platform all`.** Metal's validation is decided once per process, and MoltenVK is not clean under it (see below), so the runner starts itself with `--platform metal --result-descriptor <n>` and reads the counts back through a pipe. `--platform metal` on its own still runs in process.
- **15 tests, not 13.** "Metal Builder" and "Metal Validation Unavailable" were added. The second uses `zest__mtl_forced_validation_unavailable`.
- **Each family has several queues, as on D3D12** (4 graphics, 4 compute, 2 transfer), not one. A context holds a queue for a frame, and an immediate command buffer needs another.
- **"Metal Bindless Slots" uses a hand-written kernel** in the shape SPIRV-Cross gives, because SPIRV-Cross isn't in the backend until M4. It catches a wrong entry size. It can't catch a wrong region offset, because the test binds the regions from the same numbers the backend writes with. M4's converted shaders are the check for that.
- **Validation, beyond decision 9:**
  - `zest_device_init_flag_enable_validation_layers_with_sync` is what asks for shader validation, as it asks for GPU-based validation on D3D12.
  - The deliberate mistake at device creation is kept off standard error, so a device with validation doesn't open with an error message.
  - A launcher that set `MTL_DEBUG_LAYER_ERROR_MODE` itself keeps its choice. In abort mode the mistake isn't made, and API errors stop the process and aren't counted.
  - Standard error is flushed through the reader when the process exits or aborts, so an assert's message isn't lost.
- **No Metal query says whether one format is supported**, only what each GPU family brings. `zest__mtl_device_has_pixel_format` encodes that: BC needs the device's BC flag, and ASTC, ETC2, 16-bit packed colour and 8-bit sRGB need an Apple GPU.
- **Limits found:**
  - Metal samplers have no LOD bias, so `mip_lod_bias` is ignored.
  - A storage image can't be sRGB, 16-bit packed, depth or block compressed.
  - Mipmaps can't be generated for depth or block compressed formats.
  - `max_multiview_view_count` reports 1 until M5.
- **A view of a whole image in its own type holds the image's texture** and creates no texture view.

**Found and left alone, for Peter**
- **MoltenVK under Metal's debug layer.** With `MTL_DEBUG_LAYER=1 MTL_DEBUG_LAYER_ERROR_MODE=nslog` the Vulkan suites raise two kinds of debug layer error:
  - "RenderPass Descriptor Validation" in "User Test Swapchain Resize", as recorded under Task 4.
  - "The following Metal object is being destroyed while still required to be alive by the command buffer (label: vkQueueSubmit MTLCommandBuffer on Queue 0-0)", for a 6,144 byte shared buffer. That is the size of the debug font's staging buffer. In one run the process then crashed with a segmentation fault just after "Compute Test Only Graph".
  - Repro, from the repo root on the Mac: `MTL_DEBUG_LAYER=1 MTL_DEBUG_LAYER_ERROR_MODE=nslog ../Zest-Build/examples/SDL2/zest-tests --platform vulkan`. Whether Zest's Vulkan path frees that staging buffer too early or MoltenVK holds it loosely is not known.

**Shared edits for Peter's Windows run**
- `zest.h`: the `collect_validation_messages` member of `zest_platform_t`, and a NULL-checked call to it at the top of `zest_GetValidationErrorCount` and `zest_ResetValidationErrors`. Vulkan and D3D12 leave it unset.
- `zest-tests.cpp`: a `run_metal` flag that is always false on Windows, `metal` as a platform name (which prints that the backend isn't compiled in), a reworded unknown-platform message, and the rest inside `#ifdef ZEST_ENABLE_METAL`.
- Expected on Windows: the four baseline lines in [Testing_], then the `--gbv` and `--warp` runs and the minimal template on each backend.

**For M2's plan**
- **Answered: `SDL_Metal_CreateView` works on any SDL window.** A probe (`spikes/metal/build/probe/probe_sdl_metal.mm`, SDL 2.32.10) made windows with no API flag, with `SDL_WINDOW_VULKAN` and with `SDL_WINDOW_METAL`. On all three the view's layer is a `CAMetalLayer`, three frames clear and present with the command buffer completed, and after `SDL_SetWindowSize` the layer's `drawableSize` and the next drawable follow the new size with nothing set by hand. So `zest_implsdl2_CreateWindow` needs no new flag, and the window that serves Vulkan can serve Metal. The layer hands out up to 3 drawables and has display sync on by default.
- **Answered: a swapchain with an `image_count` of 1 satisfies shared code.** Shared code touches a swapchain's images in four places: `views[current_image_frame]` at `zest.h:15411` and `zest.h:18938`, `images[0].info` at `zest.h:18937`, and a debug print that loops to `image_count` at `zest.h:18050`. With one image, `current_image_frame` fixed at 0 and the image's and view's textures re-pointed at each drawable, all four hold. The view struct a cached graph keeps then never changes, only the texture behind it.
- New: the frame graph's command buffers have to have their `logs` read on completion when shader validation is on, as the immediate ones do now through `zest__mtl_finish_command_buffer`.
- New: a windowed context's Metal objects are what "Metal Headless Context"'s size check is waiting for, so M2 should add the windowed counterpart.
- New: an example run with `--validate` and both backends compiled in must create its Metal device before any Vulkan device, or validation can't come on. `example_platform.h` should try Metal first, which is also what decision 4 asks for from M6.

### M2: swapchain and frames (2026-11-23, commits 44a3242 to 971d558 and the Phase 4 commit after it)

Planned and recorded in `metal_m2_implementation.md`. Its [Built_] has every result, breakage and difference from its plan.

- A windowed Metal context presents to a `CAMetalLayer`, resizes, toggles vsync in place and survives a device reset. `zest-clear-screen --metal` runs with validation clean.
- `zest-tests --platform all` on the Mac: `Dynamic rendering: 129 / 129`, `Legacy render pass: 129 / 129`, `Metal suite: 52 / 52`, `Metal backend: 23 / 23`. 80 shared tests are pending on Metal, grouped in `metal_pending_tests[]` by what each needs.
- `zest_metal.h` has 50 stubs left of M1's 88.
- What M2 learnt that later milestones rely on:
  - Creating a window makes the process's first Metal device, so validation has to be prepared before the window. `zest_PrepareMetalValidation` does it.
  - macOS never frees a `CAMetalLayer` that has had display sync turned off, so an SDL window keeps one Metal view that every context on it reuses.
  - A drawable hands its texture out autoreleased, and a thread with no autorelease pool keeps every one.
  - A depth and stencil format is accepted as a depth attachment with no stencil attachment, and a zero-area scissor rect is accepted.

### M3: memory and resources (2026-11-23, commits fc6e870 to 30cbe18 and the Phase 5 commit after it)

Planned and recorded in `metal_m3_implementation.md`. Its [Built_] has every result, breakage and difference from its plan, the pending list mapped to milestones, and the questions for M4's plan.

- A Metal frame graph places transient buffers and images in arena backings, copies buffers and image mips through one blit encoder per pass, and clears an image or blits between two formats with a render pass. The immediate fill, update, clear, blit and resolve work. The blit is a built-in pipeline from MSL text, built on first use.
- `zest-tests --platform all` on the Mac: `Dynamic rendering: 129 / 129`, `Legacy render pass: 129 / 129`, `Metal suite: 77 / 77`, `Metal backend: 36 / 36`. 55 shared tests are pending: 47 stop at `compile_shader`, the 7 Slang tests aren't built on the Mac, and "GPU Profiler Timestamps" needs GPU timestamps and the debug overlay's shaders.
- The resource management tests pass on Metal apart from the two that make a compute shader.
- `zest_metal.h` has 34 stubs left of M1's 88.
- What M3 learnt that later milestones rely on:
  - Metal's debug layer needs an object to outlive the completion of every unretained command buffer that used it, and completion comes a little after the event a timeline wait sees. A successful timeline wait, and the device's idle wait, settle the device's batches before shared code frees anything.
  - With GPU validation on, a commit outside an autorelease pool keeps what the batch used until a pool drains, which a thread with no pool never does.
  - With no fence between them, Metal ran a copy and a render pass in one command buffer out of the order they were recorded in. Every encoder waits on its queue's fence when it opens and updates it when it ends.
  - A copy between two formats with the same bytes per pixel and a different channel layout gives wrong bytes with no message. The copy rule compares channel counts, bytes per pixel and block sizes.
  - Any format takes render target usage at creation. Using a compressed texture as an attachment is what fails.
  - A sampler with Metal's default mip filter reads mip 0 whatever level the shader asks for.
  - With the Mac's screen locked a `CAMetalLayer` hands out no drawable, so the windowed tests fail on a one second `zest_BeginFrame`.
- Each MoltenVK suite leaves about 247 MB allocated on the Metal device after its Zest device is destroyed. What holds it is not known.

### M4: pipelines (2026-11-24, commit 41ed835)

Planned and recorded in `metal_m4_implementation.md`. Its [Built_] has every result, breakage and difference from its plan, the pending list mapped to milestones, and the questions for M5's plan.

- The backend compiles GLSL with shaderc and `ZEST_METAL=1`, and converts the SPIR-V to MSL with SPIRV-Cross when a pipeline or compute first needs it, cached by the SPIR-V's bytes. It builds render and compute pipelines from device caches keyed by content, and draws, draws indirectly and dispatches through bind state flushed before each draw or dispatch. The debug overlay draws, and `zest-minimal-template --metal` draws its triangle.
- `zest-tests --platform all` on the Mac: `Dynamic rendering: 129 / 129`, `Legacy render pass: 129 / 129`, `Metal suite: 124 / 124`, and 46 Metal backend tests, of which "Metal Windowed Frames" needs the screen unlocked. 8 shared tests are pending: "GPU Profiler Timestamps" needs GPU timestamps, and the 7 Slang tests aren't built on the Mac.
- The pipeline, layer, stress, compute, frame graph and device-reset tests all pass on Metal, so M5's done line is met before M5 starts.
- `zest_metal.h` has 6 stubs left of M1's 88: `get_msaa_sample_count` and the five GPU profiler members.
- What M4 learnt that later milestones rely on:
  - A command buffer reads as completed before its completed handlers run, and Metal's debug layer still uses the buffer's render targets in them. Only once `waitUntilCompleted` has returned can what the buffer used be freed.
  - Shader validation drops reads and writes through a view of a heap texture that the encoder hasn't declared with `useResource`, though the heap is in the residency set. Without shader validation they work. It accepts a declaration from an earlier encoder in the same command buffer.
  - Adding texture views to the residency set grew Metal's allocated size by 2 MB per device lifecycle, for no reason found.
  - Metal can keep 256 KB of its own from the first aliasing graph in a process, even past the device, depending on what ran before. A test of Metal's allocated size compares two cycles, not one against the size before it.
  - No test yet catches a missing fence wait or update on any encoder.
  - `zest_FlushFrameGraph` releases an unwaited command graph's transient bindless indexes, so the next graph can rewrite them before the GPU reads them. Every backend has the race, and Metal's shader validation is the first thing to report it.

### M5: full frame graph (2026-11-24, commit afad7b4)

Planned and recorded in `metal_m5_implementation.md`. Its [Built_] has every result, breakage and difference from its plan, and the questions for M6's plan.

- A multiview pass draws each view as an instance into the layer of its index, up to 32 views, with per-instance data and first instances right. A pipeline whose view count differs from its pass's is refused, as on D3D12, and so is an indirect draw in a multiview pass. A single view pipeline built from a shader that reads `gl_ViewIndex`, such as `depthpass.vert`, builds now, which it didn't before.
- Three backend tests make a missing GPU wait fail: between the queues of a wave, when a queue slot moves to another device queue, and every fence wait and update of the frame graph's pass encoders. None found an ordering bug.
- Shared changes in `zest.h`, every backend:
  - An unwaited command graph flush defers its transient bindless indexes to the context's lists for the frame in flight, so shader validation now passes the whole Metal suite. Its per-mip views were still destroyed at the flush, until the shared work below.
  - `zest_SetPipelineViewCount` gives 32 views a mask of `0xFFFFFFFF`, where it gave 0.
  - A builder allocator that can't grow, such as a command graph's, asserts when it runs out and names `frame_graph_allocator_size`.
  - Caching a graph keeps its frame in flight allocator able to grow. New shared test: "Cached Graph Allocator Growth".
  - The "Cannot cache the frame graph" alert shows only for a graph with a cache key, and names `frame_graph_allocator_size`.
- `zest-tests --platform all` on the Mac at M5's commit: `Dynamic rendering: 130 / 130`, `Legacy render pass: 130 / 130`, `Metal suite: 125 / 125`, `Metal backend: 51 / 51`. The same Metal lines with shader validation. The pending list is unchanged: "GPU Profiler Timestamps" and the 7 Slang tests.
- `zest_metal.h` still has M4's 6 stubs: `get_msaa_sample_count` and the five GPU profiler members.
- What M5 learnt that later milestones rely on:
  - Metal runs the other queues while one waits on a shared event, so a test can hold one queue without stalling the rest.
  - The reap's `waitUntilCompleted` blocks until a slot's earlier batch really ends, so a missing GPU wait between a slot's batches doesn't show in CPU timing. A test has to check the GPU's own signals.
  - A queue slot moves to another device queue only when the one it used last is busy. Setting `preferred_queue_index` doesn't move it.
  - Metal won't build a vertex function that writes `[[render_target_array_index]]` without `inputPrimitiveTopology`. With SPIRV-Cross's layered rendering on, a single view conversion of a shader that reads `gl_ViewIndex` declares that output and never writes it.
  - A fence call is only testable when the passes it orders overlap: a spin before the write and a read late in the next pass.
  - A command graph's builder allocator doesn't grow. The fence test's 96 pass graph overflowed the default 256 KB after 72 passes, so it sets 2 MB. Running out now stops at an assert naming `frame_graph_allocator_size`, where it crashed before.

### Shared work between M5 and M6 (2026-11-25, commits c4ed23d and 8d75c16)

- **Unwaited command graph lifetimes (c4ed23d):**
  - An unwaited flush records each queue slot's ordering value. The drain waits for those values before releasing the graph's bindless indexes, per-mip views and arenas.
  - Per-mip views of transients live in the device allocator.
  - Command graphs no longer replace `frame_sync_timeline`.
  - The new platform member `wait_for_timeline_value` is implemented on all three backends.
- **Command graphs take no cache key (8d75c16):** `zest_BeginCommandGraph(context, name)` never caches.
- **M6's baseline on the Mac:** `Dynamic rendering: 132 / 132`, `Legacy render pass: 132 / 132`, `Metal suite: 127 / 127`, `Metal backend: 53 / 53`, with the same Metal lines under shader validation. `metal_m5_implementation.md`'s "Since M5" section has the details.

### M6: parity (2026-11-25, uncommitted on top of 9b25e45)

Planned and recorded in `metal_m6_implementation.md`. Its [Built_] has every result, breakage and difference from its plan.

- Slang compiles for a Metal device with `ZEST_METAL` defined, to SPIR-V that goes through the same SPIRV-Cross conversion as GLSL. The seven Slang tests and the four Slang examples run on Metal, TimelineFX's strides included. Metal reports no scalar block layout, so a session that asks for it is refused, as on D3D12.
- Before converting, the backend gives ids and struct members named after an MSL address space a trailing underscore. SPIRV-Cross leaves `constant`, `device`, `thread` and `threadgroup` alone on a named SSA value, which Slang gives at `zest_slang_optimization_none`, and `threadgroup_imageblock`, `ray_data` and `object_data` anywhere.
- A context that asks for GPU profiling logs one line and runs without it until M7.
- Every example defines `ZEST_METAL_IMPLEMENTATION` under `ZEST_ENABLE_METAL`, starts on Metal and falls back to Vulkan if the Metal device can't be created. ImGui initialises with `ImGui_ImplSDL2_InitForMetal`. `ZEST_ENABLE_METAL` is on by default on Apple.
- "Metal Windowed Frames" switches device queue behind a gate, so the empty-frame present's ordering wait is tested. New backend tests: "Metal Shader Keyword Names" and "Metal GPU Profiling Off".
- `metal_pending_tests[]` holds only "GPU Profiler Timestamps". Apart from it the Metal suite leaves out only the four `vulkan_only_tests[]`.
- `zest-tests --platform all` on the Mac: `Dynamic rendering: 139 / 139`, `Legacy render pass: 139 / 139`, `Metal suite: 134 / 134`, `Metal backend: 55 / 55`. The same Metal lines with shader validation and under `NSZombieEnabled`. Every example runs on Metal with no validation error, with and without shader validation, and every one that draws drew.
- `zest_metal.h` still has 5 stubs: `get_msaa_sample_count` and the four GPU profiler members that nothing reaches without a profiler.
- What M6 learnt that later milestones rely on:
  - MoltenVK's portability subset reports no image view swizzles, so `zest-image-swizzle` and `zest-timelinefx` fail `--vulkan --validate` on a Mac. Documented in the examples index.
  - With vsync off, a window's frame rate on macOS depends on how fast the window server hands drawables back, on Metal and MoltenVK alike, and changes between launches with the same GPU time per frame. Compare GPU timings, which M7's timestamps give Metal.
  - An example's `zest_log.txt` holds one device's log, as each device rewrites it.
  - The pool growth race in `zest__allocate` (M5's "Found in shared code" item 4) aborted three full runs during M6. Still open.

### M7: polish (2026-11-25, uncommitted on top of 9b25e45 with M6)

Planned and recorded in `metal_m7_implementation.md`. Its [Built_] has every result, breakage and difference from its plan. Peter took the recommended option on all four of its decisions, so there's no `msl/` text cache.

- **GPU timestamps.** One shared counter sample buffer of 4096 samples per frame in flight, sampled at encoder stage boundaries, the only point the M2 supports. Every query maps to an encoder's start or end and is resolved on the CPU at readback. A profile region in a compute or transfer pass ends its encoder and gets one of its own, and a region in a render pass takes the pass. "GPU Profiler Timestamps" joins the Metal suite, and `metal_pending_tests[]` is gone.
- **Names.** Each pass is a debug group with its encoders labelled after it, command buffers carry their frame graph's name, pipeline states their template's, compute states and libraries their shader's. An Xcode capture of `zest-pbr-deferred` (`spikes/metal/m7_pbr_deferred.gputrace` on the Mac) shows them.
- **Error reports.** Frame graph batches take their command buffers from a descriptor that asks for encoder execution status with validation. A failed batch writes one report, moves its events on, and DeviceRemoved flags the context lost. Timeline waits wait in 100 ms slices and reap between them. A forced hook tests it, as the M2 can't fail a command buffer.
- **MSL input.** `zest_SetShaderOptionsMSL` in `zest.h` marks the options, Vulkan and D3D12 refuse MSL with a validation error, and Metal compiles the text, taking the library's one function of the stage. A slot is a pointer to a struct holding one entry, as Metal won't take a pointer straight to a texture, sampler or buffer pointer as a buffer argument.
- New tests: "Metal GPU Timestamps" (in place of "Metal GPU Profiling Off"), "Metal Pass Labels", "Metal Command Buffer Errors", "Metal MSL Shaders", and the shared "Shader MSL Source Platform".
- `zest-tests --platform all` on the Mac: `Dynamic rendering: 140 / 140`, `Legacy render pass: 140 / 140`, `Metal suite: 136 / 136`, `Metal backend: 58 / 58`. The same Metal lines with shader validation and under `NSZombieEnabled`.
- `zest_metal.h` has 2 stubs left: `get_msaa_sample_count` and `create_test_render_pass`.
- What M7 learnt:
  - An Apple GPU encoder that writes no counter sample leaves the value an earlier frame wrote, not 0. A render encoder with no draws writes only its vertex samples.
  - Metal's debug layer doesn't report a debug group left open at commit.
  - The converted function cache must key an MSL kernel by its threads per threadgroup, which come from its options, not its text.
