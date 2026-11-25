# Metal M6: Parity

**Goal:** every example runs on Metal, and on a Mac it starts there. Slang compiles for a Metal device with `ZEST_METAL` defined, so the seven Slang tests and the four Slang examples, TimelineFX among them, run on Metal. ImGui's extra viewport windows work on Metal. At the end `ZEST_ENABLE_METAL` is on by default on Apple, and the Metal suite leaves out only the four Vulkan-only tests and M7's timestamp test.

**Architecture:**
- **Slang reaches Metal as SPIR-V** and goes through the backend's existing SPIRV-Cross conversion, as GLSL does. `impl_slang.hpp` already gives a Metal device the SPIR-V target and `spirv_1_5`. Only its platform macro changes, to `ZEST_METAL`. Strides, linking and hot reload are Vulkan's SPIR-V path unchanged. [Probe_] converted and compiled all 21 entry points the examples use.
- **Examples:** each main file defines `ZEST_METAL_IMPLEMENTATION` under `ZEST_ENABLE_METAL`, as the minimal template does. CMake already compiles every example's main file as Objective-C++ when Metal is on. `example_platform.h` makes Metal the default when it's compiled in and falls back to Vulkan. ImGui's SDL backend gets `InitForMetal`.
- **Backend:** until M7's timestamps, a context that asks for GPU profiling runs without it rather than aborting (Decision 1). Names that clash with MSL address spaces are renamed before SPIRV-Cross writes MSL (Decision 3).
- **Tests:** the Slang tests leave the pending list. "Metal Windowed Frames" gets a real queue switch. Two new backend tests.
- Nothing on a per-frame, per-draw or per-dispatch path changes on any backend.

**Tech stack:**
- `zest_metal.h`, `examples/SDL2/zest-tests/zest-metal-tests.cpp`, `cmake/ZestMetal.cmake`.
- Shared: `implementations/impl_slang.hpp`, `examples/Common/example_platform.h`, the main file of 18 examples, `examples/SDL2/zest-tests/shaders/slang/common.slang`, `zest-slang-tests.cpp`, `zest-tests.cpp`.
- Docs: `docs/examples/index.md`, `docs/getting-started/installation.md`, `README.md` (the flag table), `docs/api-reference/compute.md`, two skills, the design document.
- No change to `zest.h`, `zest_vulkan.h`, `zest_dx12.h`, any submodule (`timelinefx.slang` included), or any shader except `common.slang`. No new dependency: Slang is the Vulkan SDK's, which `cmake/ZestSlang.cmake` already finds on macOS.

**Spec:**
- `metal_platform_layer_implementation.md`: the M6 row of [Roadmap_], Decision 4, and the last item of "For M2's plan" under [Built_] (an example has to create its Metal device before any Vulkan device for validation to come on).
- `metal_m5_implementation.md`: "For M6's plan" items 1 to 3, and "Since M5" for the baseline.
- D3D12's M6 plan (`docs/superpowers/plans/dx12_m6_parity_implementation.md`, git-ignored, on the Windows machine only) is the model for the Slang target, the example header, ImGui's SDL init and the default-on switch. What this plan needs from it is carried here.
- The probes in [Probe_], run on the Mac on 2026-11-25 while writing this plan.

## Contents

- [Constraints_] Global Constraints
- [Settled_] Settled
- [State_] Current state
- [Probe_] What the Mac's Slang and SPIRV-Cross do
- [Decisions_] Decisions for Peter
- [Review_] Review Focus
- [Scope_] Out of scope
- [Phase_1] Baseline and the suite
- [Phase_2] The examples
- [Phase_3] Parity
- [Cost_] Per-frame cost
- [Testing_] Testing
- [Built_] As built

## [Constraints_] Global Constraints

Everything in `metal_m5_implementation.md` [Constraints_] holds:
- never commit
- edit on Windows, and build and run on the Mac over `ssh macmini`: the clone is `~/Documents/GitHub/Zest`, `../Zest-Build-Metal` has Metal on and `../Zest-Build` has it off, both Debug with Unix Makefiles
- copy edited files to the clone with `scp` and give them LF endings there with `perl -pi -e 's/\r\n/\n/'`, retrying when the Mac's hostname drops
- restore the Mac clone at the end of every phase (`git checkout -- <files>`, `spikes/` stays untracked)
- write a script with a backslash or an apostrophe to a file with the Write tool
- break every new test on purpose once
- follow code-style

What M6 adds:

- **Never edit** `zest.h`, `zest_vulkan.h`, `zest_dx12.h` or a submodule. A problem found in any of them goes to Peter with a repro.
- **Shared edits** are only the files under Tech stack. The C, C++, CMake, Slang and docs files are CRLF, and the pipelines skill is LF. Check with `file` or perl before scripting an edit.
- **Windows builds after every phase:** in Debug, `zest-tests` and `zest-c-compile-check`, and in Phase 2 and 3 every example as well. The Windows suites are Peter's.
- **Budget.** No new per-frame, per-draw or per-dispatch work on any backend. On Metal, a shader's first conversion gains one walk over its SPIR-V ids (Decision 3). See [Cost_].
- **Runs.** The implementer runs `zest-tests` and `--frames N` example runs from the repo root. Launching an example without `--frames` is Peter's. A windowed run needs the screen unlocked and the display awake: wake it with `caffeinate -u -t 2`, run under `caffeinate -d`, and check `ioreg -n Root -d1 | grep IOConsoleLocked` before judging a windowed failure. Never overlap two runs.
- **A run only counts once it has drawn.** Example runs on Metal use the draw count patch (Task 1). An example whose count is 0 runs again with more frames, and the count that proved it goes in As built.
- **Baseline** (Mac, 9b25e45, screen unlocked, display awake): `--platform all` gives `Dynamic rendering: 132 / 132`, `Legacy render pass: 132 / 132`, `Metal suite: 127 / 127`, `Metal backend: 53 / 53`, no VUID line, and the same Metal lines with `--shader-validation`. The Metal-off build gives `132 / 132` twice. 22 targets build with Metal on and 21 with it off. Task 1 confirms these.
- **End counts:** `139 / 139` twice, `Metal suite: 134 / 134`, `Metal backend: 55 / 55`. Each task gives its own.

## [Settled_] Settled

The implementer doesn't reopen these.

- **From the design and M5, not reopened:**
  - Slang reaches Metal as SPIR-V through the same SPIRV-Cross conversion as GLSL. Slang's own Metal target was rejected in the design (invalid MSL for bindless arrays), and would need MSL as a shader input, which is M7's.
  - Examples start on Metal when it's compiled in and fall back to Vulkan if the Metal device can't be created. `--metal` and `--vulkan` name a backend with no fallback (Decision 4).
  - `ZEST_ENABLE_METAL` is on by default on Apple from M6 (Decision 4).
  - zest-cascading-shadows is the visual check of M5's multiview (M5, "For M6's plan" item 1).
  - "Metal Windowed Frames" gets a real queue switch, held with `zest__acquire_manager_queue` as "Metal Queue Switch Order" does (M5, item 3).
- **Slang on Metal defines `ZEST_METAL`, not `ZEST_VULKAN`,** as GLSL on Metal does (design, "Calls made without Peter"; `zest_metal.h:5116-5120`). The target, the default profile `spirv_1_5`, the SPIR-V flags and the "other backend" profile check stay as they are for Vulkan. A shader that wants the same code on Vulkan and Metal tests for either macro.
- **ImGui on Metal initialises with `ImGui_ImplSDL2_InitForMetal`.** D3D12's reason holds: `InitForVulkan` gives extra viewport windows `SDL_WINDOW_VULKAN` (`imgui_impl_sdl2.cpp:1149`), which a Mac without a Vulkan loader can't create. ImGui's shaders are already GLSL on Metal (`impl_imgui.cpp:36`), and "Pipeline Test ImGui Shaders" already passes in the Metal suite.
- **zest-compute-example keeps its point sprites on Metal** (`zest-compute-example.cpp:36`). `[[point_size]]` and `[[point_coord]]` convert and compile ([Probe_]). D3D12's quad path stays D3D12's.
- **The Metal suite's permanent exclusions are the four `vulkan_only_tests[]`** (`zest-tests.cpp:215-224`), which `RegisterTest` already skips on every backend but Vulkan (`:260`). After M6, `metal_pending_tests[]` holds only "GPU Profiler Timestamps", which M7's timestamps take off.

## [State_] Current state (verified 2026-11-25, Windows tree and Mac clone at 9b25e45)

**Slang**
- `impl_slang.hpp:406-427`: every choice keys on `zest__slang_is_d3d12`. A Metal device gets the SPIR-V target (`:415-417`), `spirv_1_5` (`:410-413`), the SPIR-V flags (`:445-448`, `:729-732`) and the macro `ZEST_VULKAN` (`:425-427`). The macro goes into the legacy compile (`:449`) and every session (`:802-803`).
- The comments that name the targets: `:84-85` (profile, debug info) and `:410`. `:1091` and `:1245` stay true for Metal.
- `timelinefx.slang` reads no `ZEST_` macro. Its own `TFX_` macros (`:646-760`) choose its bindings, so the macro's name changes nothing in it.
- Metal reports no `zest_capability_scalar_block_layout` (`zest_metal.h:1690-1697`), so a session asking for scalar layout is refused on Metal (`impl_slang.hpp:814-816`), as on D3D12.
- **Tests:** the seven are registered under `ZEST_ENABLE_SLANG` (`zest-tests.cpp:177-185`), and all seven are in `metal_pending_tests[]` (`:235-246`).
  - "Slang Type Size" expects `PlatformMacro` to be 4 on any device but D3D12 (`zest-slang-tests.cpp:300`) and `PackedPair` 32 (`:295`). `common.slang`'s `PlatformMacro` has a D3D12 and a Vulkan case only. So on Metal today the test would pass on `ZEST_VULKAN`.
  - "Slang Debug Info" compiles `simulate_a` with `zest_slang_optimization_none` (`:429`) and checks only the binary (`:438-449`), so on Metal its SPIR-V never reaches SPIRV-Cross.
  - `slang_test__binary_is_compute` takes the SPIR-V path on Metal (`:59`). `slang_test__run_and_check` creates a compute and checks the particles (`:109-116`).
- **CMake:** `cmake/ZestSlang.cmake` already finds the SDK's macOS library and its companions (`:59-61`, `:101-106`, `:124`). Both Mac build folders have `ZEST_ENABLE_SLANG=OFF`, so the Slang tests and the four Slang examples (`examples/SDL2/CMakeLists.txt:229-237`) have never been built on the Mac.
- The SDK (`~/VulkanSDK/1.4.335.1/macOS`) has `libslang.dylib`, `libslang-compiler.0.2025.23.2.dylib`, `libslang-glslang-2025.23.2.dylib`, `libslang-glsl-module-2025.23.2.dylib` and `slangc`.

**Examples**
- **CMake builds 20 examples:** 16 in `EXAMPLES` and 4 in `SLANG_EXAMPLES` (`examples/SDL2/CMakeLists.txt:208-237`). All 20 already go through `example_platform.h`, stop at `--frames` and end with `FinishExample`.
- Only `zest-minimal-template` and `zest-clear-screen` define `ZEST_METAL_IMPLEMENTATION` (`zest-minimal-template.cpp:6-8`). The other 18 define `ZEST_D3D12_IMPLEMENTATION` at line 4 or 5 under `#ifdef ZEST_ENABLE_D3D12`.
- **Every example's main file already compiles as Objective-C++ without ARC** when Metal is on (`examples/SDL2/CMakeLists.txt:192`, `cmake/ZestMetal.cmake:54-70`). So defining the implementation adds only `zest_metal.h` and the Apple headers to it.
- **`example_platform.h`:**
  - D3D12 is the default when compiled in (`:42-44`), and the comment says Metal is used only when named (`:6`).
  - Metal validation is prepared while parsing, before any window exists (`:73-83`).
  - The Metal device has no fallback (`:100-104`), where D3D12 falls back to Vulkan (`:91-99`).
  - `InitImGuiSDL2ForDevice` gives everything but D3D12 `InitForVulkan` (`:162-171`).
  - `ExampleShaderPath` gives Metal the GLSL folder (`:153-160`).
- **ImGui's SDL backend:** `InitForMetal` is a plain init (`imgui_impl_sdl2.cpp:723-726`), while `InitForVulkan` sets `IsVulkan` (`:703-713`). Its framebuffer scale comes from the window's flag: Vulkan's or Metal's drawable size, or GL's for a window with neither (`:1047-1056`). The Metal backend gives any SDL window one `CAMetalLayer` through `SDL_Metal_CreateView` (`zest_metal.h:1879-1906`).
- **GPU profiling:**
  - `zest-compute-example` (`:428`) and `zest-timelinefx-particle-techniques` (`:585`) ask for it. `zest-render-targets` and `zest-vaders` have it commented out.
  - Metal's five profiler members are stubs that report (`zest_metal.h:5491-5495`), and `zest__mtl_report_unavailable` asserts outside test builds (`:734-744`). So both examples abort at context creation on Metal in a Debug build.
  - Shared code handles a false return by leaving profiling off (`zest.h:22316-22319`) and not setting `zest_context_flag_gpu_profiling_enabled` (`:11802-11806`). `zest_EnableGPUProfiling` only turns it on with a backend (`:22585-22590`), and `zest_BeginGPUProfile` returns on a disabled profiler (`:22497-22499`).
- **particle-techniques strips vertex location 7 on Vulkan only** (`:232-236`), as the baked vertex shader doesn't read it. Metal builds the vertex descriptor only from the locations a shader reads (`zest_metal.h:3264-3291`), so the template works on Metal either way.
- `zest-timelinefx-reload` is its own harness (`examples/SDL2/CMakeLists.txt:264-270`) and `zest-timelinefx-c` is in neither list.

**The windowed queue switch**
- "Metal Windowed Frames" (`zest-metal-tests.cpp:3025`) sets `preferred_queue_index` each frame in its "queue switch" phase (`:3096-3106`), which never moves the slot (M5 Phase 2).
- The empty-frame present (`zest__mtl_dummy_submit_for_present_only`, `zest_metal.h:2036`) acquires its slot's queue (`:2045`) and waits on the slot's ordering event when the slot moved (`:2053-2056`). No test catches that wait (M5, "Checks that can't fail yet").
- The gate helpers are `MetalGate`, `metal_close_gate`, `metal_open_gate_after` and `metal_finish_gate` (`zest-metal-tests.cpp:5043-5092`), defined after the windowed test.

**The SPIR-V conversion**
- `zest__mtl_convert_spirv` (`zest_metal.h:5184-5294`) sets the options (`:5207-5221`), the slot and push bindings (`:5224-5243`), checks the set 0 arrays (`:5245-5264`) and compiles at `:5290`. The uniform alias replacement is in `zest__mtl_compile_msl` (`:5301-5302`).
- SPIRV-Cross C API 0.67 has `spvc_compiler_get_current_id_bound`, `spvc_compiler_set_name` and `spvc_compiler_get_name` (`spirv_cross_c.h:794`, `:932`, `:947`).

**Build and docs**
- `cmake/ZestMetal.cmake:7-11`: `option(... OFF)`, under the comment "Off by default: the Metal backend can't create a device yet". Both fatal messages already end with `-DZEST_ENABLE_METAL=OFF` (`:29`, `:36`).
- `docs/examples/index.md:272-301` says every macOS build runs on Vulkan, and its argument table has no `--metal`.
- `docs/getting-started/installation.md:88-110` has a D3D12 section and nothing on Metal. `README.md:88` lists `-DZEST_ENABLE_D3D12=OFF` only.
- The Slang lines that name targets and macros: `docs/api-reference/compute.md:284`, and `.claude/skills/pipelines-and-shaders/SKILL.md:212` and `:216-217`. The TimelineFX skill's D3D12 section (`.claude/skills/timelinefx/SKILL.md:31-45`) says D3D12 has no GPU profiling, which D3D12's M7 made wrong.

**Wiring to mirror**

| What | Where |
| --- | --- |
| The implementation define in an example | `zest-minimal-template.cpp:3-8` |
| A default backend that falls back to Vulkan | D3D12's branches, `example_platform.h:42-44` and `:91-99` |
| A default-on CMake option | `cmake/ZestD3D12.cmake:11-15` |
| A queue held behind a gate, and a slot moved by holding its queue | `test_metal_queue_switch_order`, `zest-metal-tests.cpp:5444-5500` |
| A backend test with a GLSL compute | "Metal Multiview Rules", `zest-metal-tests.cpp:6011` |

`zest_dx12.h` is the structural model only.

## [Probe_] What the Mac's Slang and SPIRV-Cross do (2026-11-25)

Vulkan SDK 1.4.335.1, `slangc` 2025.23.2, SPIRV-Cross `vulkan-sdk-1.4.335.0`. The scripts and their output are in `spikes/metal/probe_slang/` on the Mac (untracked).

- **Method:** `slangc -target spirv -profile spirv_1_5 -matrix-layout-column-major -O2 -D ZEST_METAL=1`, then the `spirv-cross` CLI with the backend's options (`--msl-version 30100 --msl-argument-buffers --msl-argument-buffer-tier 1 --msl-discrete-descriptor-set 0 --flip-vert-y`), then `newLibraryWithSource` at MSL 3.1 after the backend's uniform alias replacement (`probe_slang_compile_msl.mm`). The CLI doesn't set the backend's buffer indexes, which don't decide whether a shader compiles.
- **All 21 entry points compile:** the 20 in `timelinefx.slang` and `particle.slang`'s `computeMain`, the set the D3D12 tests compile (`zest-d3d12-tests.cpp:930-951`).
  - The compute and vertex stages read the arrays aliased at binding 5 through `spvBufferAliasSet`, 3 to 19 of them per entry.
  - The compute stages have `packed_` members where std430 offsets need them, up to 31 in `simulate_particles`.
- **Point sprites:** `particle.slang`'s `vertexMain` writes `[[point_size]]` and `fragmentMain` reads `[[point_coord]]`. Both compile.
- **Debug info:** with `-g` the SPIR-V carries `NonSemantic.Shader.DebugInfo`. The billboard vertex and fragment stages compile at `-g -O0`, and `simulate_particles` at `-g -O2`.
- **`simulate_particles` fails at `-O0`**, with or without `-g`: `bool constant = graph.easing_type == 0;` gives "expected unqualified-id". The local `constant` (`timelinefx.slang:334`) keeps its `OpName` at `-O0`, SPIRV-Cross writes it unchanged, and `constant` is an MSL address space. At `-O1`, `-O2` and `-O2 -g` the name is gone and it compiles.
  - `zest_slang_optimization_none` is `-O0`. The TimelineFX examples ask for high (`examples/Common/timelinefx_shaders.h:21`), so they never meet it.
  - glslang keeps local names by default, so a GLSL local with that name would fail the same way. None of the repo's GLSL has one, as every one already builds on Metal. Decision 3.
- **Slang in an Objective-C++ unit:** `slang.h` and `slang-com-ptr.h` compile with `-x objective-c++ -fno-objc-arc` after the Metal and QuartzCore headers, and a global session creates. `zest-tests.cpp` and the TimelineFX examples include Slang in their implementation file, which compiles that way on a Metal build.
- **Not checked by the probe:**
  - `kernel.slang`, as the CLI won't link its `extern Collider` the way the API does. The suite checks it in Task 2.
  - Strides at run time ([Review_] 1).
  - Scalar layout (Decision 2).

## [Decisions_] Decisions for Peter

**Settled 2026-11-25: Peter took the recommendation on all three.** GPU profiling turns itself off on Metal until M7 (Task 5), Metal doesn't report scalar block layout, and the backend renames names that clash with MSL (Task 3).

1. **GPU profiling on Metal until M7.** Two examples ask for it, and Metal's profiler stub asserts in a Debug build, so both abort at context creation.
   - **Recommended:** `zest__mtl_create_gpu_profiler_backend` writes one log line and returns false, and shared code leaves profiling off (`zest.h:22316-22319`). Every example runs unchanged, and an app that asks for GPU profiling gets CPU timings only, as with any context whose profiler couldn't be created. M7 replaces it with timestamps. One function body and a backend test, "Metal GPU Profiling Off" (Task 5).
   - Alternative: keep the reporting stub, and the two examples ask for GPU profiling only off Metal, as D3D12's M6 did. Two example edits that M7 takes out again, and a user's app that asks for profiling still aborts on Metal in a Debug build.
2. **Scalar block layout on Metal.** MoltenVK reports `scalarBlockLayout` on this GPU through the same SPIRV-Cross, so it likely works.
   - **Recommended:** Metal keeps not reporting it, as D3D12 doesn't. A Slang session that asks for it is refused with the existing message, and "Slang Type Size" skips its scalar check on Metal (`zest-slang-tests.cpp:315-324`).
   - Alternative: Metal reports `zest_capability_scalar_block_layout`. That's a promise, so it needs a dispatch that writes and reads `PackedPair` at 24 bytes on Metal, and the docs to say scalar layout works there. A probe and a test for a layout nothing in the repo uses outside that test.
3. **Names that clash with MSL.** SPIRV-Cross writes a variable named `constant` into the MSL unchanged, and Metal won't compile it ([Probe_]). The same is likely for MSL's other address spaces. Any GLSL local of that name, or a Slang one at `zest_slang_optimization_none`, fails on Metal only.
   - **Recommended:** the backend renames them before SPIRV-Cross compiles: one walk over the SPIR-V's ids with `spvc_compiler_get_name` against a table of the names Task 3's first step finds, and `spvc_compiler_set_name` with a trailing underscore. About 25 lines in the conversion, which runs once per shader. A backend test, "Metal Shader Keyword Names". So a shader that builds on Vulkan builds on Metal.
   - Alternative: document the limit (no variable may be named after an MSL address space on Metal) and report it upstream. `timelinefx.slang:334` then fails on Metal at `zest_slang_optimization_none`, and renaming it is a TimelineFX change.

**Calls made without Peter (check these)**
- **Slang is turned on in both Mac build folders,** so Slang code builds with and without Metal, and the two Vulkan suites run the Slang tests on MoltenVK. Task 9's fresh configure still checks the default options, where Slang is off.
- **Draw counts come from an untracked patch** (`spikes/metal/m6_draw_count.patch`) applied to the Mac clone for the example runs only and never committed. Nothing shared counts draws, and a counter in the backend would be test-only code on the draw path.
- **"Slang Debug Info" dispatches its shader on every backend,** keeping one code path. On Metal that's what sends debug info through SPIRV-Cross. It adds a dispatch to the Vulkan and D3D12 runs.
- **`PlatformMacro` is 12 bytes on Metal,** `uint values[3]`, so each backend's size differs.
- **particle-techniques' Vulkan-only vertex attribute strip stays** (`:232-236`), as Metal works either way.
- **The docs M6 touches** are the ones its changes make wrong or incomplete: the examples index ("every macOS build runs on Vulkan", the argument table), the README's flag table and the installation guide's build options. The backend's user docs, the README's platform line and a `metal-backend` skill stay in M7, as the roadmap has them.
- **The fallback check** sets `zest__mtl_forced_missing_feature` in the Mac clone's copy of the minimal template, never committed.
- **A fallback from Metal under `--validate`** keeps Metal's validation environment, which the example set before it made a window. MoltenVK then runs with Metal's debug layer and prints its own one message on standard error, which Zest's Vulkan count doesn't see. The examples index says so. Nothing can undo the environment once a Metal device has existed (Spike C).

## [Review_] Review Focus

1. **Slang on Metal reads a different layout from the one its SPIR-V declares.** SPIRV-Cross has to honour every `Offset` and `ArrayStride`, with `float3` members packed. A struct read at the wrong stride draws garbage with no error. Probes: "Slang Variants And Linking" on Metal, which dispatches and checks its particles, and "Slang TimelineFX Strides". Then the TimelineFX examples under shader validation (`--validate-sync`), and Peter's side-by-side checks against `--vulkan`.
2. **Metal as the default turns validation off, or changes Vulkan.** Probes: Task 6's runs with no backend argument and `--validate`, where `FinishExample` fails a run whose Metal validation didn't come on (`example_platform.h:126-133`), and the `--vulkan --validate` run of every example.
3. **A run proves nothing because nothing drew.** Probe: the draw count patch in every Metal run, and particle-techniques' baked mode at 5000 frames.
4. **The keyword rename changes a conversion that worked.** Only an exact name from the table is renamed. Probes: the whole Metal suite, which sends every GLSL shader the tests use through the conversion, the 21 Slang entry points through the examples, and "Metal Shader Keyword Names".
5. **A shared edit breaks Windows.** `example_platform.h`, `impl_slang.hpp`, `common.slang`, the Slang tests and every example's main file compile there. Probes: the implementer's Debug builds of every project on Windows, and Peter's suites and ImGui template runs.

## [Scope_] Out of scope

- GPU timestamps and "GPU Profiler Timestamps" (M7). Metal runs without GPU profiling until then (Decision 1).
- `zest-timelinefx-reload`, whose harness stays on Vulkan as it did for D3D12, and `zest-timelinefx-c`, which CMake doesn't build.
- MSAA and `get_msaa_sample_count`, which go with the MSAA resolve plan.
- The backend's user docs, the README's platform line, a `metal-backend` skill, MSL as a shader input and the `msl/` cache (M7).
- Scalar block layout on Metal (Decision 2).
- The shared items still open from M5's "Found in shared code": the command graph builder allocator that can't grow, the concurrent pool growth abort seen once, and the three smaller ones.
- Reporting the SPIRV-Cross naming bug upstream. The uniform alias report (design Decision 6) is still unsent too.
- Intel Macs.

## [Phase_1] Baseline and the suite

### Task 1: Baseline and the draw count patch

**Files:** `spikes/metal/m6_draw_count.patch`, new and untracked, on the Mac only.

**Interfaces:**
- The patch adds a draw count and a dispatch count to `zest_metal.h`, file-scope `zest_ull` values:
  - draws, incremented in `zest__mtl_draw` (`:4913`) and `zest__mtl_draw_indexed` (`:4922`), and once per command in `zest__mtl_draw_indirect_commands` (`:4940`)
  - dispatches, incremented in `zest__mtl_dispatch_compute` (`:4982`)
  - `zest__mtl_cleanup_device_backend` (`:2200`) prints `metal draws: N, dispatches: M` and zeroes both.
- It's applied with `git apply` after every sync that runs examples, and `git checkout -- zest_metal.h` takes it out.

**Steps:**
- [x] Sync the clone. Expected: clean at 9b25e45 apart from `spikes/`.
- [x] Wake the display and run, under `caffeinate -d`: `--platform all` and `--platform metal --shader-validation` from `../Zest-Build-Metal`, then `zest-tests` from `../Zest-Build`. Expected: the baseline in [Constraints_]. Save the logs in the scratchpad.
- [x] Write the patch, apply it, build `zest-minimal-template` and `zest-clear-screen`, and run each with `--metal --validate --frames 240`. Expected: exit 0 and `validation errors: 0` for both, and non-zero draws for the template. `zest-clear-screen` draws nothing by design: its one pass clears and has an empty task (`zest-clear-screen.cpp:68-70`), so its line reads `metal draws: 0`. Take the patch out.

### Task 2: Slang on Metal

**Files:**
- The Mac's build folders: `cmake -S . -B ../Zest-Build-Metal -DZEST_ENABLE_SLANG=ON`, and the same for `../Zest-Build`.
- `implementations/impl_slang.hpp:84-85`, `:410`, `:425-427`.
- `examples/SDL2/zest-tests/shaders/slang/common.slang`: `PlatformMacro`.
- `examples/SDL2/zest-tests/zest-slang-tests.cpp:282-305` ("Slang Type Size") and `:419-455` ("Slang Debug Info").
- `examples/SDL2/zest-tests/zest-tests.cpp:233-246`: `metal_pending_tests[]`.

**Interfaces:**
- `zest__slang_platform_macro` returns `"ZEST_METAL"` on a Metal device, `"ZEST_D3D12"` on D3D12 and `"ZEST_VULKAN"` otherwise, with a `switch` on `zest_GetDevicePlatform`. Nothing else in `impl_slang.hpp` changes behaviour.
- Comments:
  - `profile`: `//NULL for the platform's default, spirv_1_5 on Vulkan and Metal and sm_6_0 on D3D12`
  - `debug_info`: `//SPIR-V source-level debug info on Vulkan and Metal, embedded ILDB debug info on D3D12. Best with zest_slang_optimization_none.`
  - `:410`: `//spirv_1_5 on Vulkan and Metal, sm_6_0 on D3D12`
- `common.slang`: `PlatformMacro` takes the cases in the order `ZEST_D3D12` (`uint values[2]`), `ZEST_METAL` (`uint values[3]`), `ZEST_VULKAN` (`uint values[1]`), under the comment `//4 bytes on Vulkan, 8 on D3D12 and 12 on Metal, so a type size shows which platform macro the session defined`.
- "Slang Type Size" expects `PlatformMacro` to be 12 on Metal. `PackedPair` stays 32 there, as SPIR-V lays it out. The scalar check skips on Metal (Decision 2).
- "Slang Debug Info", after its binary check, runs the shader through `slang_test__run_and_check` with the arguments "Slang Variants And Linking" gives `simulate_a` with the default collider, on every backend. The comment: `//On Metal this sends the debug info through SPIRV-Cross`.
- `metal_pending_tests[]` loses the seven Slang names and their group comment, leaving "GPU Profiler Timestamps".

**Steps:**
- [x] Reconfigure both Mac build folders with Slang on and build every target. Expected: "Found Slang library" naming the SDK's `libslang.dylib`, 26 targets with Metal on and 25 with it off, and no warning from Zest's or the examples' files.
- [x] Make the test changes, `common.slang` and the pending list, but not the macro. Build and run `--platform all`. Expected: `139 / 139` twice, so every Slang test passes on MoltenVK, and `Metal suite: 133 / 134`, with "Slang Type Size" seeing `PlatformMacro` at 4. Note in As built any other Slang test that fails on Metal.
- [x] Change the macro and the comments. Build and run `--platform all`, then `--platform metal --shader-validation`. Expected: `139 / 139`, `139 / 139`, `Metal suite: 134 / 134`, `Metal backend: 53 / 53`, no VUID line, and the same Metal lines under shader validation.
- [x] Breaks, each on the clone and put back: `"ZEST_VULKAN"` on Metal fails "Slang Type Size" on Metal only. A wrong marker in the debug info dispatch fails "Slang Debug Info" on all three suites.
- [x] Run the Metal-off build's `zest-tests`. Expected: `139 / 139` twice.
- [x] On Windows, build `zest-tests` and `zest-c-compile-check` in Debug. Expected: no warning.

### Task 3: Names that clash with MSL (Decision 3)

**Files:**
- `zest_metal.h`: the prototype beside `zest__mtl_convert_spirv`'s (`:703`), the function before `zest__mtl_convert_spirv` (`:5184`), and its call just before `spvc_compiler_compile` (`:5290`).
- `zest-metal-tests.cpp`: "Metal Shader Keyword Names", registered after "Metal Multiview Rules" at the end of `metal_tests[]`.

**Interfaces:**
- `ZEST_PRIVATE void zest__mtl_rename_reserved_names(spvc_compiler compiler);` with the comment `//SPIRV-Cross writes a variable named after an MSL address space, such as constant, unchanged, and Metal won't compile it`.
- It walks ids 1 to `spvc_compiler_get_current_id_bound(compiler)`. A name from `spvc_compiler_get_name` that matches the table gets the same name with a trailing underscore through `spvc_compiler_set_name`. If the first step finds struct members break too, member names are walked the same way through the struct types' member counts.
- The table holds exactly the names the first step finds, with a one-line comment saying they are MSL's address spaces that SPIRV-Cross leaves alone.
- The test: a GLSL compute shader declares each name in the table as a local, a function parameter and a struct member, as far as the first step found each one breaks, and writes a known value through each into a storage buffer. The test creates the compute, dispatches once and reads the values back. Expected: the compute is created, every value is right, and no validation error.

**Steps:**
- [x] Probe in `spikes/metal/probe_slang/`: a GLSL compute with each of `constant`, `device`, `thread`, `threadgroup`, `threadgroup_imageblock`, `ray_data` and `object_data` as a local, a parameter and a struct member, through `glslc -DZEST_METAL=1 --target-env=vulkan1.2`, the backend's `spirv-cross` options and `compile_msl`. Expected: at least `constant` as a local breaks. Record which break, and where, in As built.
- [x] Write the test and register it. Build and run `--platform metal`. Expected: it fails at the compute's creation, with Metal's compile error in the log.
- [x] Implement the rename. Build and run `--platform all` and `--platform metal --shader-validation`. Expected: `139 / 139` twice, `Metal suite: 134 / 134`, `Metal backend: 54 / 54`.
- [x] A scratch test, never registered: a Slang session at `zest_slang_optimization_none` on `submodules/timelinefxlib/Shaders`, `simulate_particles` through `zest_slang_CreateShaderFromSession`, and `zest_CreateCompute` from it on a Metal device. Expected: the compute is created, where without the rename Metal's compiler fails on `constant`. Delete the scratch test.
- [x] Break: an empty table fails the new test.

### Task 4: "Metal Windowed Frames" switches queue

**Files:**
- `zest-metal-tests.cpp:3096-3106`, the "queue switch" phase of `test_metal_windowed_frames`.
- `zest-metal-tests.cpp:5043-5092`: `MetalGate` and its helpers move above `test_metal_windowed_frames` (`:3025`), unchanged.

**Interfaces:**
- The phase replaces the preferred index loop. The comment above it: `//A frame with no work on another device queue than the slot's last batch still waits for that batch`.
  - Frame A, with work, runs on the graphics family's queue 0 (`preferred_queue_index` 0 with `has_preferred_queue`) behind a closed gate on that queue's `MTLCommandQueue`.
  - The test then holds queue 0 with `zest__acquire_manager_queue(graphics_family, 0, ZEST_FALSE)`, so frame B, a frame with no work (`zest_EndFrame(context, 0)`), takes queue 1 through `zest__mtl_dummy_submit_for_present_only`, which must wait on the slot's ordering event.
  - The gate watches frame B's frame timeline, `context->frame_timeline[<B's frame in flight>]` at its value plus 1, which must not be reached while the gate holds A. The gate opens after `METAL_GATE_DELAY`.
- Checks: B's present ran on queue 1, `watched_reached` is false, both frames presented, the slot's ordering timeline is reached within a second, and the held queue is released and the gate finished on every exit path.

**Steps:**
- [x] Write the phase. Build and run `--platform metal` with the screen unlocked and the display awake. Expected: `Metal backend: 54 / 54`.
- [x] Break: take out the wait at `zest_metal.h:2053-2056` on the clone. Expected: "Metal Windowed Frames" fails with frame B's signal reached while the gate held A, in 3 runs of 3. Put it back.

### Task 5: GPU profiling off on Metal until M7 (Decision 1)

**Files:**
- `zest_metal.h:5491`: `zest__mtl_create_gpu_profiler_backend`.
- `zest-metal-tests.cpp`: "Metal GPU Profiling Off", registered after "Metal Shader Keyword Names".

**Interfaces:**
- `zest__mtl_create_gpu_profiler_backend` writes `ZEST_APPEND_LOG(context->device->log_path.str, "GPU timestamps aren't available on the Metal backend yet, so this context runs without GPU profiling.")` and returns `ZEST_FALSE`. No validation error and no alert. Its comment: `//Until GPU timestamps exist, a context that asks for GPU profiling runs without it`. The other four profiler stubs keep reporting, as nothing reaches them with profiling off.
- The test, on a headless context created with `zest_context_init_flag_gpu_profiling | zest_context_init_flag_cpu_profiling`:
  - the context is created, and `zest_context_flag_gpu_profiling_enabled` isn't set
  - `zest_EnableGPUProfiling(context, ZEST_TRUE)` leaves `context->gpu_profiler.enabled` false
  - a command graph whose compute pass calls `zest_BeginGPUProfile` and `zest_EndGPUProfile` flushes and waits
  - `zest_GetGPUProfileSmoothedResults` gives a count of 0, and the validation count stays 0

**Steps:**
- [x] Write the test and register it. Build and run `--platform metal`. Expected: it fails, with the stub's validation error.
- [x] Change the stub. Build and run `--platform all` and `--platform metal --shader-validation`. Expected: `139 / 139` twice, `Metal suite: 134 / 134`, `Metal backend: 55 / 55`, no VUID line.
- [x] The phase's end: one `NSZombieEnabled=YES MallocScribble=1` run of `--platform metal`, the Metal-off suites, and every target with Metal on and off. Expected: no zombie report, `139 / 139` twice. On Windows, `zest-tests` and `zest-c-compile-check` build in Debug with no warning.
- [x] Phase report, with the shared edits listed for Peter's Windows run: `impl_slang.hpp`, `common.slang`, `zest-slang-tests.cpp`, and `zest-tests.cpp` inside its Metal block. Restore the Mac clone.

## [Phase_2] The examples

### Task 6: `example_platform.h` starts on Metal

**Files:** `examples/Common/example_platform.h:3-8`, `:42-44`, `:100-104`, `:162-171`.

**Interfaces:**
- The header comment: D3D12 or Metal is the default when compiled in, falling back to Vulkan if its device can't be created. Otherwise the default is Vulkan.
- `ParseExampleOptions` sets `options.use_metal = ZEST_TRUE` under `#ifdef ZEST_METAL_IMPLEMENTATION`, beside the D3D12 default. The existing `zest_PrepareMetalValidation` call then runs for the default too, before the example makes its window.
- `CreateExampleDevice` falls back from Metal as from D3D12: when the Metal device isn't created and no backend was named, it prints `Could not create a Metal device, falling back to Vulkan` and clears `use_metal`.
- `InitImGuiSDL2ForDevice` switches on `zest_GetDevicePlatform`: `InitForD3D` on D3D12, `InitForMetal` on Metal, `InitForVulkan` otherwise. Its comment stays.

**Steps:**
- [x] Make the changes. Sync, apply the draw count patch, build `zest-minimal-template` and `zest-clear-screen`.
- [x] Run each with no backend argument and `--validate --frames 240`. Expected: exit 0, `validation errors: 0`, and a `metal draws` line, which only a Metal device prints, so the run was on Metal.
- [x] Run each with `--vulkan --validate --frames 240`. Expected: exit 0, `validation errors: 0`, no `metal draws` line, and no Metal debug layer message.
- [x] The fallback, on the clone only: set `zest__mtl_forced_missing_feature` in `zest-minimal-template.cpp` before `CreateExampleDevice`, build, and run with `--frames 240` and no backend argument. Expected: "Could not create a Metal device, falling back to Vulkan", exit 0. With `--metal`: the device isn't created and the run exits 1. Put the file back.
- [x] On Windows, build every example in Debug. Expected: no warning.

### Task 7: The examples without Slang

**Files:** the main file of the 14 examples below, after their D3D12 define (line 4 or 5): `zest-image-swizzle`, `zest-imgui-template`, `zest-ribbons`, `zest-implot`, `zest-pbr-deferred`, `zest-pbr-forward`, `zest-pbr-texture`, `zest-instancing`, `zest-indirect-draw`, `zest-shadow-mapping`, `zest-cascading-shadows`, `zest-fonts`, `zest-render-targets`, `zest-sdl2-imgui-template`.

**Interfaces:** each gains, as `zest-minimal-template.cpp:6-8`:

```c
#ifdef ZEST_ENABLE_METAL
#define ZEST_METAL_IMPLEMENTATION
#endif
```

**Steps:**
- [x] Add the define to the 14. Sync and build every target with Metal on. Expected: 26 targets, no warning from Zest's or the examples' files. A clash between Foundation's macros or Objective-C++'s names and an example's own code is fixed in the example, and goes in As built.
- [x] With the draw count patch applied, run each of the 16 examples without Slang (these 14, the minimal template and clear-screen) from the repo root:
  - with no backend argument and `--validate --frames 600`
  - with `--metal --validate-sync --frames 300`, which adds shader validation

  Expected: exit 0, `validation errors: 0` and non-zero draws in every run, apart from `zest-clear-screen`, which has no draws (Task 1). A run with 0 draws runs again with more frames.
- [x] Run each of the 16 with `--vulkan --validate --frames 300`. Expected: exit 0, `validation errors: 0`.
- [x] Build every target with Metal off. Expected: 25 targets, no warning. Take the patch out.
- [x] On Windows, build every example in Debug. Expected: no warning.

### Task 8: The Slang examples

**Files:** the main file of `zest-compute-example`, `zest-vaders`, `zest-timelinefx` and `zest-timelinefx-particle-techniques`, at line 4.

**Interfaces:** the same define as Task 7. The two examples that ask for GPU profiling (`zest-compute-example.cpp:428`, `zest-timelinefx-particle-techniques.cpp:585`) are left as they are, as Task 5 makes Metal run them without it.

**Steps:**
- [x] Add the define. Sync, apply the patch, and build the four.
- [x] Run from the repo root:
  - `zest-compute-example`, `zest-vaders` and `zest-timelinefx` with no backend argument and `--validate --frames 600`, then with `--metal --validate-sync --frames 300`
  - `zest-timelinefx-particle-techniques --effect-mode <mode>` for each of `baked`, `cpu` and `gpu`, with `--validate --frames <frames>` and `--metal --validate-sync --frames <sync frames>`. `<frames>` is 600 for `cpu` and `gpu` and 5000 for `baked`, and `<sync frames>` 300 and 1500, as baked mode's first draw comes after about 600 uncapped frames.

  Expected: exit 0, `validation errors: 0`, and non-zero draws in every run, with dispatches in the compute example and in particle-techniques' `gpu` mode. The compute example's draws are its point sprites. The compute example and particle-techniques log Task 5's profiling line once and run.
- [x] Run the same set with `--vulkan --validate` at the `<frames>` counts. Expected: exit 0, `validation errors: 0`.
- [x] Take the patch out. On Windows, build the four in Debug. Expected: no warning.
- [x] Phase report, with the shared edits for Peter's Windows run: `example_platform.h` and the 18 examples' main files. Restore the Mac clone.

## [Phase_3] Parity

### Task 9: The default, the docs and the final check

**Files:**
- `cmake/ZestMetal.cmake:7-11`.
- `docs/examples/index.md:272-301`, `docs/getting-started/installation.md:88-110`, `README.md:88`, `docs/api-reference/compute.md:284`.
- `.claude/skills/pipelines-and-shaders/SKILL.md:212`, `:216-217`, and `.claude/skills/timelinefx/SKILL.md:31-45`.
- `metal_platform_layer_implementation.md`: [Roadmap_], [Mapping_] and [Built_].
- This file's As built.

**Interfaces:**
- `ZestMetal.cmake`, as `ZestD3D12.cmake:11-15`: `_zest_metal_default` is `ON` when `APPLE` and `OFF` otherwise, and `option(ZEST_ENABLE_METAL "Enable the Metal backend (Apple only)" ${_zest_metal_default})`, still inside `if(NOT DEFINED ZEST_ENABLE_METAL)`. The comment becomes `# On by default on Apple`.
- **Docs:**
  - The examples index: "Running on D3D12" becomes "Running on D3D12 and Metal". Every example runs on Metal in a macOS build, which has Metal on by default and starts there, falling back to Vulkan. `--metal` joins the argument table, and `--validate-sync` names Metal's shader validation. One line says a fallback from Metal under `--validate` prints one Metal debug layer message from MoltenVK.
  - The installation guide: a short "The Metal Backend" section after D3D12's. Metal is on by default on Apple, needs the Vulkan SDK's SPIRV-Cross, and `-DZEST_ENABLE_METAL=OFF` builds without it. `zest_metal_implementation_source` compiles the implementation file as Objective-C++ without ARC.
  - The README's flag table gains `-DZEST_ENABLE_METAL=OFF`.
  - `compute.md` and the pipelines skill: Slang on Metal is SPIR-V with `ZEST_METAL` defined, and `zest_slang_CompileToBinary` gives SPIR-V on a Metal device.
  - The TimelineFX skill: a Metal line beside the D3D12 section. Its D3D12 profiling line goes, as D3D12 has timestamps now. Metal runs particle-techniques without GPU profiling until M7.
  - The pipelines skill: a context that asks for GPU profiling on Metal runs without it until M7, and Metal renames variables named after MSL address spaces before converting.
- **The design document:**
  - The roadmap's sentence names this file.
  - The "Shader creation" row of [Mapping_] says Slang arrives as SPIR-V with `ZEST_METAL` defined, and that names clashing with MSL address spaces are renamed. The "GPU timestamps" row says a context that asks for GPU profiling runs without it until M7.
  - M6's part goes in [Built_].

**Steps:**
- [x] Change the CMake default. On the Mac, configure an empty folder, `../Zest-Build-M6-Fresh`, with no options and build Debug. Expected: "Metal support enabled", "Slang support disabled", every target builds with no warning, and `zest-minimal-template --validate --frames 240` from it exits 0 with `validation errors: 0`. Delete the folder.
- [x] Check `metal_pending_tests[]`. Expected: only "GPU Profiler Timestamps".
- [x] Write the docs, the skills and the design document's parts. Run `qgrep update zest`.
- [x] **The final check on the Mac,** with the screen unlocked and the display awake:
  - every target with Metal on (26) and off (25), with no warning
  - `--platform all`, `--platform metal --shader-validation`, one `NSZombieEnabled=YES MallocScribble=1` run of `--platform metal`, and the Metal-off suites
  - `zest-minimal-template` and `zest-timelinefx` with no backend argument and `--validate --frames 600`, with the patch
  - Expected: `139 / 139` twice, `Metal suite: 134 / 134`, `Metal backend: 55 / 55`, the same Metal lines under shader validation, no VUID line, no zombie report, the Metal-off suites at `139 / 139` twice, and both examples at exit 0 with `validation errors: 0` and non-zero draws.
- [x] On Windows, build every project in Debug, `zest-c-compile-check` included. Expected: no warning.
- [x] Add As built to this file, with the counts, every difference from the plan and the shared edits. Hand Peter the Windows checks and the visual checks. Restore the Mac clone.

## [Cost_] Per-frame cost

| Path | Vulkan and D3D12 | Metal |
| --- | --- | --- |
| Per frame, per draw, per dispatch | Unchanged | Unchanged |
| A shader's first conversion | Unchanged | One walk over the SPIR-V's ids, comparing each name against a table of a few names (Decision 3) |
| Slang session creation | Unchanged | Unchanged: one macro with another name |
| Context creation with GPU profiling | Unchanged | One log line, and no profiler buffers until M7 (Decision 1) |
| Example start-up | Unchanged | Metal is tried first. A failed Metal device costs one device creation before Vulkan's |

**Memory:** nothing new. No struct grows and no buffer is added. A context that asks for GPU profiling on Metal allocates less than before, as its profiler is never created.

## [Testing_] Testing

**Automated, on the Mac (implementer)**
- After every task: build `zest-tests`, `zest-c-compile-check` and `zest-objc-compile-check` with Metal on, then `zest-tests --platform metal` and `--platform metal --shader-validation`.
- At the end of every phase: `zest-tests --platform all`, the Metal-off build and its suites, and one zombie and `MallocScribble` run of `--platform metal`.
- Every example run on Metal with the draw count patch, as Tasks 6 to 8 give them.
- New backend tests, taking `metal_tests[]` from 53 to 55: Shader Keyword Names, GPU Profiling Off. "Metal Windowed Frames" gains its queue switch.
- The Slang tests join the Metal suite, taking it from 127 to 134, and run in both Vulkan suites on the Mac for the first time, taking each from 132 to 139.

**Peter's tests**
- On Windows after Phase 1: `zest-tests.exe` with the default `--platform all`, and `--gbv`. Expected: the counts of his last run. "Slang Debug Info" now dispatches its shader on Vulkan and D3D12, and "Slang Type Size" still expects 4 and 8.
- On Windows after Phase 2: `zest-imgui-template.exe --validate --frames 240`, which starts on D3D12, and the same with `--vulkan`. Expected: exit 0 and `validation errors: 0` both times, as `InitImGuiSDL2ForDevice` changed shape.
- On the Mac at the end, with the screen unlocked: `zest-tests --platform all`. Expected: `Metal backend: 55 / 55`, which includes "Metal Windowed Frames" with its queue switch.
- The existing tests nearest the changes: "Shader Platform Macro", "Pipeline Test ImGui Shaders", "Slang Variants And Linking", "Slang TimelineFX Strides", "Metal Queue Switch Order" and "Metal Debug Overlay".

**Visual checks for Peter**
Run each on the Mac from the repo root with `--validate`, so that on exit it prints `validation errors: 0`. With no backend argument it runs on Metal. Compare with the same example given `--vulkan`.
1. `zest-cascading-shadows`: the four cascades match the Vulkan run, and each cascade's layer in the shadow map debug view holds its own depths. This is M5's multiview check.
2. `zest-imgui-template`: the text is crisp at the Mac's display scale. Drag a window out of the main one, move it about, and drag it back. It draws while outside, and its text is as sharp as in the main window. A blurry or half-size outside window means ImGui got the wrong framebuffer scale with `InitForMetal`.
3. `zest-sdl2-imgui-template`: the sprite under "Test Window" cycles through its pictures. Press "Toggle Refresh Rate Sync" twice, then "Bordered", "Borderless", "Full Screen", "Full Screen Borderless" and "Bordered" again, then "Reset Renderer". Each mode redraws, and the reset gives back a working window.
4. `zest-timelinefx`: the title effect, the ribbons, the repeating effects and the laser that follows the mouse look as on Vulkan. Press Space to refresh the library, and nothing on screen changes.
5. `zest-timelinefx-particle-techniques`: each entry of the "Effects" combo looks as on Vulkan. The profile window shows CPU timings only on Metal.
6. `zest-compute-example`: the point sprites look as on Vulkan.
7. `zest-vaders`: play for a minute. Effects and ribbons look as on Vulkan.
8. `zest-image-swizzle`: each swizzled view shows the same channels as on Vulkan.
9. Glance at the rest against Vulkan: `zest-fonts`, `zest-implot`, `zest-pbr-forward`, `zest-pbr-deferred`, `zest-pbr-texture`, `zest-instancing`, `zest-indirect-draw`, `zest-shadow-mapping`, `zest-render-targets` and `zest-ribbons`.
10. Resize one Metal example's window and minimise it. It redraws at the new size and comes back from minimised.

## [Built_] As built

### Phase 1: baseline and the suite (2026-11-25, on top of 9b25e45, built and run on the Mac over SSH)

**What changed**
- `impl_slang.hpp`: `zest__slang_platform_macro` switches on the platform and gives `ZEST_METAL`, `ZEST_D3D12` or `ZEST_VULKAN`. The three comments name Metal.
- `common.slang`: `PlatformMacro` has a `ZEST_METAL` case, `uint values[3]`.
- `zest-slang-tests.cpp`: includes `<unistd.h>` off Windows. "Slang Type Size" expects 12 on Metal. "Slang Debug Info" dispatches `simulate_a` on Metal.
- `zest-tests.cpp`: `metal_pending_tests[]` holds only "GPU Profiler Timestamps".
- `zest_metal.h`:
  - `zest__mtl_reserved_name_replacement` and `zest__mtl_rename_reserved_names`, called just before `spvc_compiler_compile`. Ids and struct members are walked.
  - `zest__mtl_create_gpu_profiler_backend` writes one log line and returns false.
- `zest-metal-tests.cpp`: `MetalGate` and its helpers moved above `test_metal_windowed_frames`. Its queue switch phase is rewritten. "Metal Shader Keyword Names" and "Metal GPU Profiling Off" are new.
- `zest-timelinefx-particle-techniques.cpp`: two format specifiers.
- Both Mac build folders have `ZEST_ENABLE_SLANG=ON`.

**Baseline**
- The first `--platform all` aborted in the dynamic rendering suite at `zest.h:10584`, in "Resource Test Concurrent Pool Growth" (see "Found in shared code"). The rerun gave `132 / 132`, `132 / 132`, `Metal suite: 127 / 127`, `Metal backend: 53 / 53`, no VUID line.
- Shader validation gave `127 / 127` and `53 / 53`. The Metal-off build gave `132 / 132` twice. 22 targets with Metal on, 21 off.
- Draw count patch: `zest-minimal-template --metal --validate --frames 240` gave `metal draws: 240`, and `zest-clear-screen` `metal draws: 0`. Both exit 0 with `validation errors: 0`.

**Results**

| Run | Dynamic rendering | Legacy render pass | Metal suite | Metal backend |
| --- | --- | --- | --- | --- |
| `--platform all` | 139 / 139 | 139 / 139 | 134 / 134 | 55 / 55 |
| `--platform metal --shader-validation` | | | 134 / 134 | 55 / 55 |
| `NSZombieEnabled=YES MallocScribble=1` | | | 134 / 134 | 55 / 55, no zombie report |
| Metal off | 139 / 139 | 139 / 139 | | |

- No VUID line, and no Metal debug layer record in the shared suite's part of any log. The screen was unlocked throughout.
- Every target builds with Metal on (26) and off (25), with no warning from Zest's or the examples' files.
- Task 2's middle run, with the test changes but the old macro, gave `139 / 139` twice and `Metal suite: 133 / 134`. Only "Slang Type Size" failed, on `PlatformMacro` at 4. No other Slang test failed on Metal.
- On Windows in Debug, with Slang and D3D12 on: `zest-tests`, `zest-c-compile-check` and `zest-timelinefx-particle-techniques` build with no warning. The Windows suites were not run.

**Breaks**
- `"ZEST_VULKAN"` on Metal: "Slang Type Size" fails on the Metal suite only.
- A marker of 2 in the debug info dispatch: "Slang Debug Info" fails on the Metal suite only, as only Metal dispatches.
- A table with no address space in it: "Metal Shader Keyword Names" fails. Neither the GLSL compute nor the Slang one is created.
- The ordering wait out of the empty-frame present: "Metal Windowed Frames" fails in 3 runs of 3, with "The empty frame finished while the gate still held the frame before it."
- The old profiler stub: "Metal GPU Profiling Off" fails with the stub's validation error.

**What Task 3's probe found** (`spikes/metal/probe_slang/probe_msl_names.py` and `probe_msl_names_slang.py`)
- GLSL through `glslc` with shaderc's defaults, as the backend sets no target environment:
  - `constant`, `device`, `thread` and `threadgroup` compile as a local, a parameter, a local struct's member and a buffer block's member. SPIRV-Cross renames them itself (`constant0`).
  - `threadgroup_imageblock`, `ray_data` and `object_data` fail in all four places, with "expected unqualified-id", or "parameter may not be qualified with an address space" for a parameter.
  - `kernel`, `vertex` and `fragment` compile everywhere.
- Slang at `-O0`, each name a `bool` local: all seven fail. Slang names the SSA value, and SPIRV-Cross writes a named SSA value without its renaming. That is what [Probe_]'s `simulate_particles` case was, not a variable.
- So the table holds all seven, and the members are walked for the last three.
- The scratch test, `simulate_particles` at `zest_slang_optimization_none` from `submodules/timelinefxlib/Shaders`, creates its compute on Metal. Without the rename, Metal fails at MSL line 3018, `bool constant`. It was deleted.

**Differences from the plan**
1. **"Slang Debug Info" dispatches on Metal only**, not on every backend. Its comment and the Slang memory: Slang's debug SPIR-V crashes `vkCreateComputePipelines` on the AMD RX 6600 driver 32.0.21045.5002, which Peter's Windows machine runs. So its break fails the Metal suite only.
2. **"Metal Shader Keyword Names" has a Slang part** under `ZEST_ENABLE_SLANG`: a module written to the working folder with each name as a `bool` local, compiled at `zest_slang_optimization_none`, dispatched and checked for 127. GLSL can't name an SSA value, so a GLSL test alone can't catch the four names SPIRV-Cross already renames on variables. The GLSL part declares all seven as a local, a parameter and a member of a struct that is also the buffer's element.
3. The rename's comment says what the probe found, not "a variable named after an MSL address space".
4. `zest-slang-tests.cpp` includes `<unistd.h>` off Windows. The Metal-off Mac build failed on `rmdir`, which the Apple headers declare in the Metal-on build. This was the Slang tests' first Unix build.
5. particle-techniques' two `-Wformat` warnings are fixed (`%zi` given `zest_millisecs`, `%i` given `size_t`). This was its first clang build.
6. The draw count patch also counts the immediate dispatch (`zest__mtl_imm_dispatch_compute`).
7. The wiring table names "Metal Multiview Rules" as the model for a GLSL compute test, but it has no compute. The new tests follow "Metal Immediate Compute".

**For Phase 2**
- The profiling line goes to the device's log file through `ZEST_APPEND_LOG`, not to standard output, so Task 8 finds it in an example's log file, if it sets one.
- `spikes/metal/m6_run_example.sh <build folder> <example> <arguments>` runs one example and prints its exit code, validation count and draw line. `spikes/metal/m6_make_draw_count_patch.py` rewrites the patch if `zest_metal.h` moves.

**Found in shared code, for Peter**
- **The pool growth race** (M5's "Found in shared code" item 4, seen twice now). `zest__allocate` (`zest.h:10565-10585`) adds a pool under the growth lock and tries `zloc_Allocate` once more. Other threads allocate without the lock, and both growth reports allocate from the same allocator under `zest_device_init_flag_output_memory_pool_info`. So the new 256 KB pool, about 31 of the test's 8 KB blocks, can be used up before the retry, which then asserts. 1 abort in 100 runs of the test alone on the Mac. Adding pools in a loop until the thread's own allocation succeeds would close it.
- **"D3D12 Transient Images" and "D3D12 Cross Queue Barriers" fail on Windows** (Peter's run after Phase 1, `D3D12 backend: 101 / 103`, reproduced with the D3D12 backend suite alone). Neither file is touched by M6. 8d75c16 moved both tests' barrier checks to between `zest_EndFrameGraph` and the flush, as a command graph's records no longer outlive it. But D3D12 pushes each translated barrier zeroed at compile time (`zest_dx12.h:6716-6723`) and writes it only when the graph executes (`zest__dx_send_frame_graph_barriers`, `:6925`), so before the flush every `D3D12_TEXTURE_BARRIER` they read is zero.
  - **Fixed in `zest-d3d12-tests.cpp`** (Peter, 2026-11-25): each test checks the translated barriers in a pass task that runs after they were sent. `build_transient_image_graph` takes `out_third_barrier_clean`, and with it "Third Pass" runs `third_target_barrier_is_clean`, moved above it. "Cross Render" copies the image's release and acquire into the task data, which the test checks after the flush, with a message when they're wrong. The compiled records (the copy group, the buffer records and the node lists) are still read before the flush.
  - Result on Windows in Debug: `zest-tests` builds with no warning, and the D3D12 backend suite alone gives `103 / 103`, where it gave `101 / 103` with these two failing before the fix.

### Phase 2: the examples (2026-11-25, on top of 9b25e45 with Phase 1 uncommitted, built and run on the Mac over SSH)

**What changed**
- `example_platform.h`: Metal is the default under `ZEST_METAL_IMPLEMENTATION`, and `CreateExampleDevice` falls back from it to Vulkan when no backend was named. `InitImGuiSDL2ForDevice` switches on the platform and gives Metal `ImGui_ImplSDL2_InitForMetal`. The header comment, the `use_metal` member comment and `CreateExampleDevice`'s comment say so.
- `zest-minimal-template.cpp` and `zest-clear-screen.cpp`: their argument comment says D3D12 or Metal is the default. Not in the plan, but Task 6 made it wrong.
- The main file of the 14 examples in Task 7, and of the 4 in Task 8, defines `ZEST_METAL_IMPLEMENTATION` under `ZEST_ENABLE_METAL` after the D3D12 define.
- `docs/examples/index.md`: a "Limits on Vulkan on macOS" list after the D3D12 limits, for `zest-image-swizzle` and `zest-timelinefx` (below).

**Task 6**
- `zest-minimal-template` and `zest-clear-screen` with no backend argument and `--validate --frames 240`: exit 0, `validation errors: 0`, "Metal API Validation Enabled", and `metal draws: 240` and `metal draws: 0` (no draws by design).
- With `--vulkan --validate --frames 240`: exit 0, `validation errors: 0`, no draw line and no Metal debug layer line.
- The fallback, with `zest__mtl_forced_missing_feature` set in the clone's template: "Could not create a Metal device, falling back to Vulkan" and exit 0. With `--metal`, "Could not create the device" and exit 1. The refused Metal device still prints `metal draws: 0` from its cleanup, so a draw line alone doesn't show a run stayed on Metal. Draws above 0, or no fallback line, do.

**Task 7** (with the draw count patch; every Metal run printed "Metal API Validation Enabled", and every `--validate-sync` run "Metal GPU Validation Enabled" too)

| Example | Default, `--validate --frames 600` | `--metal --validate-sync --frames 300` | `--vulkan --validate --frames 300` |
| --- | --- | --- | --- |
| zest-minimal-template | 600 draws | 300 draws | 0 errors |
| zest-clear-screen | 0 draws (none by design) | 0 draws | 0 errors |
| zest-image-swizzle | 1200 draws | 600 draws | **exit 1, 2 errors** |
| zest-imgui-template | 1782 draws | 885 draws | 0 errors |
| zest-ribbons | 1798 draws, 600 dispatches | 898 draws, 300 dispatches | 0 errors |
| zest-implot | 1782 draws | 891 draws | 0 errors |
| zest-pbr-deferred | 6000 draws, 12 dispatches | 3000 draws, 12 dispatches | 0 errors |
| zest-pbr-forward | 4800 draws, 12 dispatches | 2400 draws, 12 dispatches | 0 errors |
| zest-pbr-texture | 3600 draws, 12 dispatches | 1800 draws, 12 dispatches | 0 errors |
| zest-instancing | 3000 draws | 1500 draws | 0 errors |
| zest-indirect-draw | 3000 draws, 1200 dispatches | 1500 draws, 600 dispatches | 0 errors |
| zest-shadow-mapping | 2400 draws | 1200 draws | 0 errors |
| zest-cascading-shadows | 4800 draws | 2400 draws | 0 errors |
| zest-fonts | 1800 draws | 900 draws | 0 errors |
| zest-render-targets | 1200 draws, 7200 dispatches | 600 draws, 3600 dispatches | 0 errors |
| zest-sdl2-imgui-template | 4792 draws | 2392 draws | 0 errors |

- Every Metal run exits 0 with `validation errors: 0`.
- Every target builds with Metal on (26) and off (25) with no warning, so no clash between the Apple headers and any example's code. On Windows in Debug every target builds with no warning.
- **The one mismatch: `zest-image-swizzle --vulkan --validate` exits 1 with 2 validation errors**, both MoltenVK's:
  - `VUID-VkImageViewCreateInfo-imageViewFormatSwizzle-04465`: MoltenVK's portability subset reports `imageViewFormatSwizzle` false, and the example's swizzled view is the point of the example.
  - `mvk-warn`: "Metal does not support disabling primitive restart."
  - The Metal-off build's `zest-image-swizzle --validate --frames 300`, where the new define and Metal's paths compile out, gives the same two. So it's not from M6, and it was there whenever this example ran on a Mac.
  - The primitive restart message comes from the example's `zest_topology_triangle_strip` pipeline, as Zest's Vulkan pipelines set `primitiveRestartEnable` false (`zest_vulkan.h:4004`).
  - **Peter, 2026-11-25: documented, not fixed.** `docs/examples/index.md` has a "Limits on Vulkan on macOS" list after the D3D12 limits, which Task 9 keeps.
- **Peter's checks after Task 7:** on Windows (run for him), `zest-tests.exe` and `--gbv` both gave `139 / 139`, `139 / 139`, `D3D12 suite: 135 / 135` and `D3D12 backend: 103 / 103` with no VUID line, and `zest-imgui-template.exe --validate --frames 240` exited 0 with `validation errors: 0` on D3D12 and with `--vulkan`. On the Mac, Peter saw the examples without Slang draw correctly on Metal.

**Task 8** (with the draw count patch; the profiling count is the lines of Task 5's message in that run's `zest_log.txt`, which each device rewrites in the working folder)

| Run | Default, `--validate` | `--metal --validate-sync` | `--vulkan --validate` |
| --- | --- | --- | --- |
| zest-compute-example, 600 / 300 / 600 frames | 3598 draws, 600 dispatches, 1 profiling line | 1794 draws, 300 dispatches, 1 profiling line | 0 errors |
| zest-vaders, 600 / 300 / 600 | 3600 draws, 600 dispatches | 1800 draws, 300 dispatches | 0 errors |
| zest-timelinefx, 600 / 300 / 600 | 2400 draws, 600 dispatches | 1200 draws, 300 dispatches | **exit 1, 1 error** |
| particle-techniques `cpu`, 600 / 300 / 600 | 3475 draws, 1 profiling line | 1787 draws, 1 profiling line | 0 errors |
| particle-techniques `gpu`, 600 / 300 / 600 | 4184 draws, 1035 dispatches, 1 profiling line | 2019 draws, 1943 dispatches, 1 profiling line | 0 errors |
| particle-techniques `baked`, 5000 / 1500 / 5000 | 29994 draws, 4999 dispatches, 1 profiling line | 8989 draws, 1499 dispatches, 1 profiling line | 0 errors |

- Every Metal run exits 0 with `validation errors: 0` and draws. No Vulkan run has the profiling line, and no Metal run of vaders or timelinefx, which don't ask for GPU profiling.
- The four build with no warning on the Mac and on Windows in Debug.
- **`zest-timelinefx --vulkan --validate` exits 1** with the same `VUID-VkImageViewCreateInfo-imageViewFormatSwizzle-04465`, from 9 image views counted as one error. `impl_timelinefx.c:64-92` gives single channel textures luminance and alpha-only swizzles. The Metal-off build gives the same. Under Peter's call for `zest-image-swizzle`, it joined that note in `docs/examples/index.md`.

**The phase's end** (the patch out, the screen unlocked)
- `--platform all`: `139 / 139`, `139 / 139`, `Metal suite: 134 / 134`, `Metal backend: 55 / 55`, no VUID line.
- `NSZombieEnabled=YES MallocScribble=1 --platform metal`: `134 / 134` and `55 / 55`, no zombie report.
- Metal off: `139 / 139` twice.
- The Mac clone is back at 9b25e45 with only `spikes/` untracked.

**Found after the phase: windowed frame rates on the Mac** (Peter saw `zest-compute-example` start at 60 fps, unlock to ~124 or ~380, and unlock when he moved the window, on both backends)
- A scratch probe on the Mac clone timed each frame's waits. Back-to-back launches of the same build ran at ~125 or ~380 fps. GPU execution was 2.2 ms per frame in both, and the CPU side the same. At ~125 a frame waited ~2.6 ms in `nextDrawable` and its command buffer ~2.3 ms more before it ran, about two frames per display refresh.
- The app's active, key window and visible states were the same in both. With vsync off, Zest's default, the window server decides how fast drawables come back. MoltenVK draws through a `CAMetalLayer` too, so Vulkan behaves the same.
- Not a Zest problem, and nothing changed. `docs/examples/index.md` says a windowed frame rate on macOS isn't a measure of performance. The probe was taken out.

### Phase 3: parity (2026-11-25, on top of 9b25e45 with Phases 1 and 2 uncommitted, built and run on the Mac over SSH)

**What changed**
- `cmake/ZestMetal.cmake`: `ZEST_ENABLE_METAL` defaults to `ON` on Apple and `OFF` elsewhere, as `ZestD3D12.cmake` does for Windows, still inside `if(NOT DEFINED ZEST_ENABLE_METAL)`.
- `docs/examples/index.md`: "Running on D3D12 and Metal". Covers Metal as the macOS default with its fallback, `--metal`, Metal's part in `--validate` and `--validate-sync`, and validation fixed at the first window with what a fallback under `--validate` prints. Also a paragraph on windowed frame rates on macOS, `ExampleShaderPath` giving Metal the GLSL, and a "Limits on Metal" list (no GPU profiling yet), beside Phase 2's "Limits on Vulkan on macOS".
- `docs/getting-started/installation.md`: "The Metal Backend" after D3D12's section: on by default on Apple, SPIRV-Cross from the Vulkan SDK, `-DZEST_ENABLE_METAL=OFF`, the macOS 15 deployment target, the implementation define, the device functions and `zest_metal_implementation_source`.
- `README.md`: `-DZEST_ENABLE_METAL=OFF` in the flag table.
- `docs/api-reference/compute.md`: Slang reaches Metal as SPIR-V that the backend converts. The default profile and the profile check name Metal, every session defines `ZEST_METAL` on Metal, `zest_slang_CompileToBinary` gives SPIR-V there, and Metal, like D3D12, refuses a scalar layout session.
- Skills, all git-ignored:
  - `pipelines-and-shaders`: the Slang rows and paragraph name Metal, and the shader section has the address space rename.
  - `gpu-profiling`: Metal runs without GPU profiling until M7.
  - `timelinefx`: a Metal section, MoltenVK's swizzle message, and the stale "GPU profiling isn't available on D3D12" line is gone.
- `metal_platform_layer_implementation.md`: the roadmap names this file, the "Shader creation" and "GPU timestamps" rows of [Mapping_] have M6's part, and [Built_] has M6.
- `qgrep update zest` run.

**Results**
- Fresh configure of an empty folder with no options: "Metal support enabled" (deployment target 15.0), "Slang support disabled", `ZEST_ENABLE_METAL=ON`. 22 targets build with no warning (the Slang examples need Slang). Its `zest-minimal-template --validate --frames 240` ran on Metal, exit 0, `validation errors: 0`. The folder is deleted.
- `metal_pending_tests[]` holds only "GPU Profiler Timestamps".
- The final check, with the screen unlocked:

| Run | Dynamic rendering | Legacy render pass | Metal suite | Metal backend |
| --- | --- | --- | --- | --- |
| `--platform all` | 139 / 139 | 139 / 139 | 134 / 134 | 55 / 55 |
| `--platform metal --shader-validation` | | | 134 / 134 | 55 / 55 |
| `NSZombieEnabled=YES MallocScribble=1` | | | 134 / 134 | 55 / 55, no zombie report |
| Metal off | 139 / 139 | 139 / 139 | | |

- No VUID line, and no Metal debug layer record in the shared suite's part of any log.
- The first `--platform all` aborted in the Metal process at `zest.h:10584`, in "Resource Test Concurrent Pool Growth": the pool growth race in Phase 1's "Found in shared code", its third sighting. The rerun above is clean.
- Every target builds with Metal on (26) and off (25), with no warning from Zest's or the examples' files.
- With the draw count patch, no backend argument and `--validate --frames 600`: `zest-minimal-template` 600 draws and `zest-timelinefx` 2400 draws and 600 dispatches, both exit 0 with `validation errors: 0`.
- The fallback under `--validate` (the template with `zest__mtl_forced_missing_feature` set, in the clone only): "Could not create a Metal device, falling back to Vulkan", exit 0, `validation errors: 0`. The only Metal debug layer line is "Metal API Validation Enabled", from the environment the example set before its window.
- On Windows in Debug every project builds, `zest-tests` and `zest-c-compile-check` included, with no compile warning. The only "warning" in the log is CMake's existing `cmake_minimum_required` deprecation, from the reconfigure `ZestMetal.cmake`'s edit caused.
- The Mac clone is back at 9b25e45 with only `spikes/` untracked.

**Differences from the plan**
1. The "GPU profiling off on Metal" note is in the `gpu-profiling` skill, not the pipelines skill, as that's where a reader of profiling looks.
2. The examples index also has the windowed frame rate paragraph, from the probe after Phase 2.
3. The examples index's fallback line says MoltenVK runs with Metal's validation on and that what it prints doesn't count, rather than naming one message. The final check's fallback printed only "Metal API Validation Enabled".

**After the sanity check**
- `zest_metal.h`, in `zest__mtl_rename_reserved_names`: the comment before the member walk says a struct type names its members from the first, which is what the check on member 0 relies on. Comment only.
- `docs/getting-started/installation.md`'s prerequisites no longer say only Vulkan is implemented. Its Vulkan SDK line names the three backends and Metal's use of SPIRV-Cross.

### Differences from the plan, all phases (2026-11-25)

- "Slang Debug Info" dispatches on Metal only, as Slang's debug SPIR-V crashes Vulkan pipeline creation on Peter's AMD driver (Phase 1, 1).
- The MSL name table holds all seven address spaces, and struct members are walked too. GLSL variables named `constant`, `device`, `thread` or `threadgroup` already worked; what failed was those names on Slang's named SSA values, and the newer three anywhere. "Metal Shader Keyword Names" has a Slang part for that (Phase 1, 2).
- Two fixes for the Slang tests' and particle-techniques' first clang builds: `<unistd.h>` and two format specifiers (Phase 1, 4 and 5).
- "D3D12 Transient Images" and "D3D12 Cross Queue Barriers", broken by 8d75c16 before M6, read the translated barriers from a pass task now (Phase 1, "Found in shared code").
- The minimal template's and clear-screen's argument comments name Metal as a default (Phase 2).
- `zest-image-swizzle` and `zest-timelinefx` fail `--vulkan --validate` on the Mac from MoltenVK's portability subset. Documented at Peter's call, not fixed (Phase 2).
- Still open for Peter: the pool growth race in `zest__allocate`, seen three times in full runs and once in 100 runs of the test alone.
