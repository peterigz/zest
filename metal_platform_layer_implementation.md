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
| Timelines, queue ordering | `MTLSharedEvent`. A command buffer encodes its waits first and its signals last. CPU waits use the event's timed wait *(unverified)* | Spike B |
| Binary semaphores | Not needed. The swapchain semaphore hooks return NULL | |
| Command pools | Nothing to pool. Each batch takes a command buffer with unretained references, since Zest already defers frees past the frame in flight | M2 |
| Passes | One encoder at a time per command buffer. Render passes open a render encoder from a descriptor carrying the load and store ops. Compute and transfer passes switch lazily between a compute and a blit encoder as commands need them | M2, M4 |
| Buffer pools | One untracked `MTLBuffer` per pool, suballocated by TLSF. Host-visible pools are shared storage and always mapped. Device-local pools are private | Task 8 |
| Image pools, dedicated images, transient arenas | Private, untracked placement `MTLHeap`s. An image is a texture placed at the pool offset *(unverified)* | Spike B |
| Residency | One `MTLResidencySet` per device holding every pool buffer and heap, attached to all queues. No `useResource` anywhere *(unverified, including for GPU writes through the bindless table)* | Spike B |
| Ordering inside a queue | One `MTLFence` per queue. Each encoder waits on it when it opens and updates it when it ends, which serialises passes *(unverified)*. Refined from the barrier records only if profiling shows a cost | Spike B, M5 |
| Image layouts | None. The backend tracks the Zest layout so `get_image_raw_layout` and the layout tests work | Task 9 |
| Crossing queues | The shared event orders the handoff. No barrier on either side | M5 |
| Views | `newTextureView` objects. `MTLTextureUsagePixelFormatView` only on images that need a view of another format | Task 9 |
| The bindless set | One shared-storage `MTLBuffer` with a fixed region per slot. An image or sampler entry is its `gpuResourceID`, a buffer entry is `gpuAddress + offset`. Entry sizes and how the regions bind come from SPIRV-Cross's layout | Spike A |
| Structured buffer indexes | The same as raw ones, as on Vulkan. MSL pointers carry the type | Spike A |
| Shader creation | shaderc for GLSL (`ZEST_METAL=1` defined), then SPIRV-Cross to MSL, then `newLibraryWithSource`. Cached per shader for the device's life | Spike A, M4 |
| Clip space | SPIRV-Cross flips Y in the vertex stage, so positions land on Vulkan's pixels. Front face mapping is whatever the spike measures | Spike A |
| Push constants | `setVertexBytes`, `setFragmentBytes` and the compute `setBytes` at flush, at the index SPIRV-Cross was told to use. No upload ring | M4 |
| Compute local size | Read from the SPIR-V and passed to `dispatchThreadgroups` | Spike A |
| Pipelines | Built lazily from the rendering info, with a device-wide cache keyed by contents. Depth-stencil state is its own cached object. Cull mode, winding, depth bias and topology are encoder or draw state | M4 |
| Indirect draws | One native call per draw, looped over `draw_count`. Metal's indirect argument structs have Vulkan's field order *(unverified)* | M5 |
| Mip generation | The blit encoder's `generateMipmaps` | Task 9 |
| Scaled blits | A built-in pipeline whose shaders are MSL text in `zest_metal.h`, so it needs neither shaderc nor SPIRV-Cross | M3 |
| Clears outside a pass | A render pass with a clear load op and no draws where the image can be a target, otherwise a copy from zeroed staging | M3 |
| Swapchain | A `CAMetalLayer`. Acquire is `nextDrawable`, present is `presentDrawable` on the frame's last command buffer. Resize is detected by comparing sizes | M2 |
| Validation | `MTL_DEBUG_LAYER` and `MTL_SHADER_VALIDATION`. How messages reach `zest_GetValidationErrorCount` is open | Spike C |
| Markers and names | `pushDebugGroup` and `label`, always on | M7 |
| GPU timestamps | Counter sample buffers. Apple GPUs sample at pass stage boundaries only *(unverified)*, so sub-pass regions may be a documented limit | M7 |
| Device loss | The command buffer's error and encoder execution status | M7 |

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

**Calls made without Peter (check these)**
- The Metal GLSL compile is its own shaderc call in `zest_metal.h`, about 60 lines that mirror `zest__vk_compile_shader`. Hoisting the Vulkan one into `zest.h` would edit `zest_vulkan.h`, and a Metal-only build has no `zest_vulkan.h`.
- Metal compiles GLSL with `ZEST_METAL=1` and not `ZEST_VULKAN`, with a non-zero cache salt, and caches SPIR-V in `./spv/` beside Vulkan's. Precompiled Vulkan SPIR-V still loads.
- Intel Macs get no code and no gate. The backend uses only shared buffers and private textures, which are legal there, and the docs call Intel Macs untested.
- Spike sources go in an untracked `spikes/metal/` folder. Their results go under As built in this file, and their checks move into backend tests.
- Only `zest_GetMetalDevice` is added as an interop getter in M1. Queue and command buffer getters wait for an integration that needs one.

## [Review_] Review Focus

1. **The bindless table layout is wrong by one entry size.** A texture entry of 8 bytes against a buffer entry of 8 or 16 shifts every index after it, and the shader reads another resource with no error. Probe: the "Metal Bindless Slots" test writes a distinct resource at the first, a middle and the last index of every slot and reads each back through a shader. It must fail when any region's stride is changed by 8.
2. **Residency or ordering is assumed, not proven.** A pass reads a buffer the previous pass wrote through the table, with no `useResource`. If residency sets don't cover writes, or untracked passes overlap, it works on the day and fails under load. Probe: Spike B under `MTL_SHADER_VALIDATION=1`, then "Metal Untracked Pass Ordering" in the backend list, run 200 times in a loop.
3. **Leaked or over-released Metal objects.** Manual reference counting with `@autoreleasepool` in some hooks and not others. Probe: every M1 test ends by destroying its device and comparing `currentAllocatedSize` with the value before creation, and the suite runs once under `MallocScribble` and zombies (`NSZombieEnabled=YES`).
4. **A shared-code edit breaks Windows.** The Mac can't see it. Probe: Peter's Windows run in [Testing_], which must match the counts he records before M1 starts.
5. **Validation is silent because it isn't on.** If `setenv` after process start doesn't enable the debug layer, every "no validation errors" line means nothing. Probe: Spike C's deliberate error must be seen, and "Metal Validation Reports" stays in the backend list to make one error per run and expect a count of one.

## [Scope_] Out of scope

- iOS, tvOS and visionOS. The API use is the same, but there's no window helper, no test target and no device to run on. It would start in the window helpers and `cmake/ZestMetal.cmake`.
- Metal 4. A later backend revision could move to `MTL4` command allocators and argument tables. It would start at the command recording and residency sections.
- Intel Mac support beyond "may work".
- MSL as a shader input, and MSL twins of anything. The built-in blit shaders are the only MSL in the repo.
- MSAA resolve, which doesn't work in shared code or on Vulkan dynamic rendering and has its own plan pending.
- Custom descriptor sets, which shared code still can't allocate or write on any backend.
- Tessellation and geometry shaders, which Metal doesn't have in Vulkan's form. The capabilities report false.
- A serialised pipeline cache (`MTLBinaryArchive`). M7 caches MSL text only.
- The TimelineFX hot reload harness, left on Vulkan as it was for D3D12.

## [Roadmap_] Milestones

Each ends with something runnable. M2 to M7 get task-level plans when they start.

| Milestone | Covers | Done when |
| --- | --- | --- |
| **M1** Spikes, groundwork, device | The three spikes. CMake and shared wiring. `zest_metal.h` with every member stubbed. Device, queues, shared events, immediate command buffers. Buffer pools, heaps, residency. Formats, images, views, samplers, uploads with mips, readback. The bindless table. Headless contexts. The Metal backend test list | A Metal device and a headless context create and destroy with validation silent and allocated size back at its baseline, an image with pixels reads back unchanged, and the Mac's Vulkan runs match Task 1's baseline |
| **M2** Swapchain and frames | Window helpers, `CAMetalLayer`, acquire, present, resize. Per-batch command buffers, render passes with load and store ops, shared-event submission, the fence chain. `reinit_logical_device`. The Metal suite run joins `RunSuite` with its pending list | An app clears the window to a colour on Metal, resizes and shuts down clean |
| **M3** Memory and resources | Transient arenas and unbound transient images. Buffer copies and uploads. Storage and uniform buffer entries, storage images, per-mip views. The MSL blit pipeline, `image_clear`, the remaining immediate functions except push constants and compute | The resource-management tests pass on Metal, apart from those that need compute |
| **M4** Pipelines | shaderc and SPIRV-Cross inside the backend, the conversion cache, render and compute pipelines, lazy bind, draws, dispatch, push constants, the debug overlay, `--metal` on the minimal template | The pipeline and layer tests pass on Metal and the minimal template draws |
| **M5** Full frame graph | Multi-queue and async compute over shared events, transient aliasing, cached graphs, indirect draws, multiview, the device-reset tests | The frame graph, stress, compute and device-reset tests pass on Metal |
| **M6** Parity | `ZEST_METAL` in the Slang integration, ImGui, the TimelineFX examples, every example through `example_platform.h` with `--metal`, the pending list cut down to permanent exclusions | The suite passes on both Mac backends and every example runs on Metal with a non-zero draw count. `ZEST_ENABLE_METAL` then defaults to on for Apple |
| **M7** Polish | GPU timestamps, markers and labels, command buffer error reports, an `msl/` text cache so a warm start skips shaderc and SPIRV-Cross, user docs, a `metal-backend` skill, the README line | The overlay shows GPU timings, an Xcode capture shows pass names, and a second launch compiles no GLSL |

## [Phase_1] M1: baselines and spikes

The spikes are standalone programs: one `.mm` file each plus the SDK's SPIRV-Cross, no Zest. They answer what the design assumes before `zest_metal.h` exists.

### Task 1: Mac baseline

**Files:** none edited. Results go under `## As built` in this file.

**Steps:**
- [ ] Record the machine: chip, macOS version, Xcode version, Vulkan SDK version and path, and whether the SDK has `spirv_cross_c.h` and the `spirv-cross-*` static libraries. Expected: macOS 15 or newer. If the SDK lacks SPIRV-Cross, stop and tell Peter.
- [ ] Configure and build the tree as it is (no Metal option exists yet) and build `zest-tests` in Debug. Expected: it builds. If it doesn't, list the errors for Peter before fixing anything: they're Mac regressions from the D3D12 work in shared code.
- [ ] Run `zest-tests` from the repo root. Record the `Dynamic rendering` and `Legacy render pass` counts, every failing test, and the known noise lines.
- [ ] Run `zest-minimal-template` and `zest-imgui-template` with `--validate --frames 600`. Record exit codes and validation counts.
- [ ] Build `zest-c-compile-check`. Expected: clean.

### Task 2: Spike A, shaders and the bindless table

**Files:** `spikes/metal/spike_shaders.mm` (untracked).

**Question:** can SPIRV-Cross turn Zest's SPIR-V into MSL that Metal compiles, with the bindless set as plain 8-byte entries the CPU writes?

**Steps:**
- [ ] Compile all 67 GLSL files to SPIR-V with the SDK's `glslc`, defining `ZEST_METAL=1`. If `slangc` is present, add `timelinefx.slang`'s entry points as SPIR-V.
- [ ] Convert each with the SPIRV-Cross C API: MSL 3.1 or newer, Metal 3 argument buffers at tier 2, runtime arrays as descriptor arrays, `flip_vert_y` on. Try set 0 both as one argument buffer and as discrete bindings, and keep whichever gives every runtime array a region the CPU can fill with consecutive 8-byte values. Record the exact option set.
- [ ] Compile every MSL result with `newLibraryWithSource`. Expected: all compile. List any that don't, with the error.
- [ ] From the generated MSL and Metal's reflection, record for each of the nine slots: the entry size in bytes, the buffer index it binds at, and whether one binding call or several are needed per stage.
- [ ] Build one render pipeline and one compute pipeline from converted shaders, and run them with a hand-built table: sample a texture through slot 1 with a sampler from slot 0, read a storage buffer through slot 5, write one through slot 5, write a storage image through slot 6, read a uniform buffer through slot 7. Read the results back.
- [ ] Assign fixed buffer indices for the table, the push constant block and vertex buffer bindings 0 to 3 with no collision in any of the 67 shaders. Record them as the proposed `ZEST__MTL_*_BUFFER_INDEX` values.
- [ ] Read a compute shader's local size from SPIR-V through the C API and dispatch with it.
- [ ] Viewport and facing: draw one triangle covering the top-left half of a small target, with culling off as the control, then with back-face culling for each winding. Compare the covered pixels with what the same SPIR-V covers on MoltenVK.
- [ ] Draw with a non-zero base instance and check `gl_InstanceIndex` includes it.

**Pass:** every shader converts and compiles, the table works with CPU-written entries, the indices don't collide, the covered pixels equal Vulkan's, and exactly one winding draws.
**Fail:** runtime arrays can't be filled as plain entries, or a class of the repo's shaders doesn't convert. Stop and tell Peter: the fallback is MSL twins, which changes M4 and M6.
**Takes over the check:** "Metal Bindless Slots" (Task 10) and, in M4, a facing test that stays in the suite.

### Task 3: Spike B, memory, residency and ordering

**Files:** `spikes/metal/spike_memory.mm` (untracked).

**Question:** do placement heaps, untracked resources, one residency set and one fence per queue give Zest's memory model with nothing declared per pass?

**Steps:**
- [ ] Create a private placement heap, query a texture's size and alignment, and place two textures at chosen offsets. Then place a third over the first's range, as an aliased transient would be.
- [ ] Create one untracked shared buffer as a pool and form two bindless entries from `gpuAddress` plus different offsets.
- [ ] Put the heap and the buffer in one residency set, commit it and attach it to three command queues. Call `useResource` and `useHeap` nowhere.
- [ ] In one command buffer: a compute encoder writes the buffer and a heap texture through the table, then a render encoder samples that texture and reads the buffer. Each encoder waits on one shared fence when it opens and updates it when it ends. Read back and check. Run it 200 times.
- [ ] Repeat with the render pass rendering into a heap texture and a later pass sampling it.
- [ ] Run everything under `MTL_DEBUG_LAYER=1` and `MTL_SHADER_VALIDATION=1`. Expected: no message.
- [ ] Shared events: queue A's command buffer signals value 1, queue B's waits for it before reading what A wrote. Then a CPU wait with a timeout, once satisfied and once timing out. Record the API used for the timed wait.
- [ ] Record `currentAllocatedSize` before the first allocation and after the last release.

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
- [ ] Call `setenv` for `MTL_DEBUG_LAYER` after `main` starts and before the device is created. Make one deliberate API error (a scissor rect outside the target). Record whether it's reported.
- [ ] If it isn't, record the alternatives that do work: variables set by the launcher, or re-executing the process with them set.
- [ ] With `MTL_DEBUG_LAYER_ERROR_MODE=nslog`, find out where the message goes and whether an in-process reader on standard error sees it. Record the line's prefix.
- [ ] Make one deliberate shader validation error (an out-of-bounds buffer read) under `MTL_SHADER_VALIDATION=1` and record what the command buffer's `logs` and `error` hold.
- [ ] Record what a second device created in the same process sees.

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
- [ ] Write `ZestMetal.cmake` mirroring `ZestD3D12.cmake`'s shape: option, find, enabled flag. Link Metal, QuartzCore and Foundation, and the SPIRV-Cross libraries. Set the deployment target to 15.0 when the option is on.
- [ ] Call `zest_metal_implementation_source` for `zest.cpp`, for each example's main file in `buildExample`, and for `zest-tests.cpp`.
- [ ] Configure with the option OFF. Expected: the build is unchanged from Task 1 and the tests give Task 1's counts.
- [ ] Configure with it ON. Expected: the configure finds SPIRV-Cross. The build fails only because `zest_metal.h` doesn't exist yet, which Task 6 fixes.

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
- [ ] Add the shared edits. They're additive: no Vulkan or D3D12 line changes meaning.
- [ ] Write the skeleton with the callbacks function and one stub per member, grouped as `zest_platform_t` groups them.
- [ ] Build `zest-tests`, `zest-c-compile-check` and `zest-objc-compile-check` with Metal ON. Expected: all build.
- [ ] Run `zest-tests`. Expected: Task 1's counts, since nothing creates a Metal device yet.
- [ ] List the shared edits for Peter's Windows run.

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
- [ ] Device selection and the requirement check, with validation turned on the way Spike C found.
- [ ] Queues and the residency set, attached to every queue at creation.
- [ ] Timelines as shared events, with the CPU wait Spike B recorded, honouring `fence_wait_timeout_ns` and the timeout callbacks.
- [ ] Immediate command buffers: begin takes a command buffer on the family's queue, end commits and waits.
- [ ] Tests: "Metal Device Lifecycle" (create and destroy twice, allocated size back at baseline), "Metal Minimum Spec Rejected" (the forced hook makes creation fail and the log name the feature), "Metal Timeline Wait" (signal from a command buffer, wait satisfied, wait timed out).
- [ ] Break each on purpose once (skip the release, skip the requirement check, never signal) and confirm its test fails.
- [ ] Build all three targets and run.

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
- [ ] Implement the hooks. `map_memory` returns the shared buffer's contents pointer plus the offset, and `flush_used_buffers` does nothing.
- [ ] Test "Metal Buffer Pool": allocate host-visible buffers at two offsets in one pool, write through the mapping, copy to a device-local buffer and back with an immediate copy stubbed in for now by a blit encoder in the test's own helper, and compare. State in the test what is read back.
- [ ] Test "Metal Residency Tracks Pools": the set's allocation count rises with a new pool and falls when it's freed.
- [ ] Build and run.

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
- [ ] The format table and `is_image_format_supported`, checked against the device's capabilities for the usage asked.
- [ ] Image creation as a placed texture, views, samplers.
- [ ] Uploads, mip generation and readback through blit encoders on immediate command buffers.
- [ ] Tests: "Metal Image Round Trip" (pixels in, read back, compared byte for byte, for an RGBA8 image, an image 8 pixels wide like the debug font, and a 1x1 image), "Metal Mip Chain" (upload a two-colour checker, generate mips, read back the last mip and check it's the average), "Metal Format Table" (every `zest_format` either maps and is creatable as a 4x4 texture, or reports unsupported, with no third outcome).
- [ ] Break each once and confirm it fails.
- [ ] Build and run.

### Task 10: The bindless table

**Files:** `zest_metal.h` section `Bindless_table`.

**Interfaces:**
- `zest_mtl_bindless_regions_t`: per slot, the byte offset, entry size and count, from Spike A's numbers and the device's `bindless_*_count` settings.
- Hooks: `create_set_layout`, `new_set_layout_backend`, `cleanup_set_layout_backend`, `create_set_pool`, `new_descriptor_pool_backend`, `create_bindless_set`, `update_bindless_image_descriptor`, `update_bindless_storage_buffer_descriptor`, `update_bindless_uniform_buffer_descriptor`, `build_pipeline_layout`, `new_pipeline_layout_backend`, `cleanup_pipeline_layout_backend`.
- A write is a store of the resource ID or of `gpuAddress + buffer->memory_offset`. The stride parameter of the storage buffer hook is accepted and ignored.
- A non-bindless set layout reports through `zest__mtl_report_unavailable`, as custom sets do on D3D12.

**Steps:**
- [ ] Create the table buffer, add it to the residency set, lay out the regions.
- [ ] Implement the three write hooks.
- [ ] Test "Metal Bindless Slots": for each slot, write a distinct resource at the first, a middle and the last index, then run the spike's converted shader through a pipeline built in the test's own helper and read each back. Change one region's entry size by 8 and confirm the test fails.
- [ ] With Tasks 7 to 10 in place `zest_EndDeviceBuilder` should complete. Test "Metal Device With Defaults": the default images and the debug font exist and the font's first glyph row reads back as uploaded.
- [ ] Build and run.

### Task 11: Headless contexts, the test list and the runner

**Files:** `zest_metal.h` sections `Backend_setup_functions`, `Backend_cleanup_functions`. `zest-tests.cpp:352-448`, `zest-tests.h`, `zest-metal-tests.cpp`.

**Interfaces:**
- Hooks: `new_context_backend`, `initialise_context_backend`, `cleanup_context_backend`, `new_context_queue_backend`, `initialise_context_queue_backend`, `cleanup_context_queue_backend`, `reset_queue_command_pool`, `queue_wait_idle`, `wait_for_fif_semaphore`, `wait_for_renderer_semaphore`, for a headless context only. The windowed paths stay stubs until M2.
- `zest_device_builder BeginMetalTestBuilder(void);` and `int RunMetalSuite(int *total_out);` in `zest-metal-tests.cpp`, mirroring `zest-d3d12-tests.cpp:21` and `RunD3D12Suite`. The table is `metal_tests[]`.
- `--platform` accepts `metal`. `all` includes it when it's compiled in. A new `--shader-validation` switch adds `MTL_SHADER_VALIDATION`, the counterpart of `--gbv`.
- A summary line `Metal backend: N / M`, included in the exit code.

**Steps:**
- [ ] Headless context creation and teardown.
- [ ] Move the tests from Tasks 7 to 10 into `metal_tests[]` and add "Metal Headless Context" (create, destroy, allocated size at baseline), "Metal Untracked Pass Ordering" (Spike B's compute-then-read pair, 200 iterations) and "Metal Validation Reports" (one deliberate error, expect a count of one).
- [ ] Remove the temporary call in `main` and wire the runner.
- [ ] Run `zest-tests --platform all`, then `--platform metal --shader-validation`. Expected: the Vulkan lines match Task 1, and every Metal test passes in both.
- [ ] Run `--platform metal` once with zombies and `MallocScribble` on. Expected: no report.

### Task 12: Milestone close

**Steps:**
- [ ] Build `zest-tests`, `zest-c-compile-check` and `zest-objc-compile-check` with Metal ON, then `zest-tests` with it OFF. Expected: all clean, and OFF gives Task 1's counts.
- [ ] Write `## As built (<date>)`: the spike numbers, every *(unverified)* row that turned out different, the buffer index values, the test counts, the full list of shared edits.
- [ ] Hand Peter the Windows checks in [Testing_] and the questions M2's plan needs answered. Known ones: whether `SDL_Metal_CreateView` works on a window made without `SDL_WINDOW_METAL`, and whether a swapchain with `image_count` 1 whose image is re-pointed at each drawable satisfies every shared use of `views[current_image_frame]`.

## [Cost_] Per-frame cost

| Path | Vulkan and D3D12 | Metal, design target |
| --- | --- | --- |
| Per frame | Unchanged | One drawable acquire, one present, one autorelease pool per hook that creates an object |
| Per batch | Unchanged | One command buffer, its event waits and signals |
| Per pass | Unchanged | One encoder, one fence wait and one update, the bindless table bound once per stage, viewport and scissor |
| Per draw or dispatch | Unchanged | The flush compares recorded state and sets only what changed. Push constants cost one copy of up to 128 bytes per stage that reads them. No allocation, no lock, no pool |
| Per bindless write | Unchanged | One 8-byte store, or two if Spike A finds 16-byte buffer entries |
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
- Before M1 starts: record the four summary lines from `zest-tests.exe` as the Windows baseline.
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
