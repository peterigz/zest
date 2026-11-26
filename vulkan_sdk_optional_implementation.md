# Vulkan SDK optional

**Goal:** Build Zest and every example without the Vulkan SDK: on D3D12 on Windows, and on Metal on macOS. Vulkan stays a backend that's built whenever CMake finds it, so nothing changes for anyone who has the SDK.

**Architecture:** A new `cmake/ZestVulkan.cmake`, mirroring the D3D12 and Metal modules, makes the Vulkan backend an `AUTO`/`ON`/`OFF` option and defines `ZEST_ENABLE_VULKAN` when it's built. Every source that defines `ZEST_VULKAN_IMPLEMENTATION` does so under that define, as it already does for D3D12 and Metal, and `zest.h` guards its two Vulkan device helpers. When Vulkan doesn't supply shaderc, Metal finds shaderc and SPIRV-Cross itself: the SDK first, then Homebrew. On Windows, examples get SDL2 from the SDK as they do today, or else from a pinned download of SDL's official VC package. Nothing runs differently at runtime.

**Tech stack:** CMake, `zest.h` (two `#ifdef`s), `zest.cpp`, the examples' defines and `example_platform.h`, the test runner, CI, docs. No changes to `zest_vulkan.h`, `zest_dx12.h` or `zest_metal.h`, and no shader changes. No new dependencies: SDL2 2.32.10 is the version the SDK ships today, and Homebrew's `shaderc` and `spirv-cross` are the libraries the SDK ships.

**Spec:** Peter's answers on 2026-11-25 (Level 1, Homebrew for Metal's tools, a pinned SDL2 download, `AUTO` by default), his choice on 2026-11-26 to keep Level 1 and widen the follow-up plan after comparing other multi-backend libraries, and the findings in memory `vulkan-sdk-optional-plan-pending`.

## Global Constraints

- Never commit. Peter commits.
- Follow code-style for C and C++. In CMake, match each file's indentation, and write messages as whole sentences that say what to do, as `ZestD3D12.cmake` does.
- Edit only these files:
  - `CMakeLists.txt`, `cmake/ZestMetal.cmake`, and the new `cmake/ZestVulkan.cmake`
  - `zest.h` (Task 4's lines only) and `zest.cpp`
  - `examples/Common/example_platform.h`, `examples/SDL2/CMakeLists.txt` and `examples/SDL2/zest-timelinefx-reload/CMakeLists.txt`
  - the 20 built examples' main files
  - in `examples/SDL2/zest-tests/`: `zest-tests.cpp`, `zest-user-tests.cpp`, `zest-c-compile-check.c` and `zest-objc-compile-check.m`
  - `.github/workflows/ci.yml`
  - the docs and skills in Task 9
- Never edit `zest_vulkan.h`, `zest_dx12.h`, `zest_metal.h` or anything under `submodules/`.
- Every file above is CRLF. Edit with Edit or perl, never `sed -i`, and check a scripted edit with `file`.
- Windows builds:
  - `E:/Projects/C++/Zest-Build` is the SDK build. Build and run commands are in CLAUDE.md.
  - New: `E:/Projects/C++/Zest-Build-NoVulkan` (Vulkan off, SDK still installed) and `E:/Projects/C++/Zest-Build-NoSDK` (no SDK in the environment).
  - Run `qgrep update zest` after the source tasks.
- Mac builds:
  - Follow the metal-backend skill's SSH workflow: `ssh macmini`, `source ~/.zprofile`, the clone at `~/Documents/GitHub/Zest`, tar sync with the CRLF strip, and restore with `git checkout -- .`. Run windowed tests under `caffeinate -d`.
  - Existing: `../Zest-Build-Metal` (Metal and Vulkan) and `../Zest-Build` (Metal off). New: `../Zest-Build-MetalOnly` (SDK libraries, Vulkan off) and `../Zest-Build-Homebrew` (no SDK).
- Task 7 installs `shaderc` and `spirv-cross` with Homebrew on the Mac mini. Approving the plan approves that.
- The implementer runs `zest-tests` and the configure probes. Peter runs the examples.
- Budget: no runtime cost at all, and a build with the SDK installed links exactly the libraries it links today.

## Settled

- **Level 1 (Peter, 2026-11-25, kept on 2026-11-26).** D3D12 and Metal users need no SDK. Building the Vulkan backend still needs it, or the distro packages on Linux. Peter kept this after a comparison with sokol, bgfx, SDL3 GPU, Diligent, The Forge, NVRHI and LLGL. Most of them build Vulkan without the SDK, using their own copy of the headers and a loader opened at runtime. Zest can't do the same yet, because its built-in shaders still need shaderc, so that work goes in the follow-up plan (see Out of scope).
- **`ZEST_ENABLE_VULKAN` defaults to `AUTO` (Peter).** It's a cache STRING with three values:
  - `AUTO` builds Vulkan when `find_package(Vulkan)` succeeds, and says which way it went.
  - `ON` makes a missing Vulkan an error.
  - `OFF` leaves Vulkan out.

  On Linux, Vulkan is the only backend, so `AUTO` without Vulkan ends in the no-backend error. That error replaces "Could not find Vulkan library!".
- **A `cmake/ZestVulkan.cmake` module, mirroring `ZestD3D12.cmake` and `ZestMetal.cmake`.**
  - It's included before `ZestMetal.cmake`, so Metal knows whether Vulkan supplies shaderc.
  - It takes `CMakeLists.txt:68-95` and `:125-228` unchanged, so builds with the SDK link what they link today. That includes:
    - the `libs/vulkan` fallback. `libs/` is git-ignored (`.gitignore:401`), so a local copy may rely on it.
    - the Linux shaderc libraries
    - the pkg-config fallback
    - `ZEST_NO_SHADERC`
  - The duplicated MSVC and non-MSVC branches for `zest-interface` and `zest` become one library list, linked to both.
  - Slang's `${SLANG_LIBRARY_LINK}` is linked only inside those Vulkan branches today. It moves to the Slang block.
- **Sources define `ZEST_VULKAN_IMPLEMENTATION` under `#ifdef ZEST_ENABLE_VULKAN`**, as they already do for D3D12 and Metal (`zest.cpp:6-11`). `example_platform.h` tests `ZEST_VULKAN_IMPLEMENTATION`, as it tests `ZEST_METAL_IMPLEMENTATION`.
- **`zest.h` only guards the definitions of its two Vulkan device helpers.** Their declarations stay, as D3D12's and Metal's do (`zest.h:5184-5191`). The probe under Current state shows that nothing else in the shared code needs Vulkan.
- **Only one copy of shaderc is linked.** When Vulkan is on, Metal uses Vulkan's shaderc and links none of its own. Two copies, or Homebrew's shared library beside a static one, risk duplicate symbols. Metal looks for `shaderc_combined` only when Vulkan is off or found no shaderc.
- **Homebrew for Metal's tools (Peter), with the SDK first.**
  - When `VULKAN_SDK` is set, its folder is searched before anything else, so an SDK build keeps the SDK's libraries.
  - Homebrew's prefix is the fallback: `HOMEBREW_PREFIX`, else `/opt/homebrew`.
  - Both formulas ship what Metal links: `libshaderc_combined.a`, and the `libspirv-cross-*.a` libraries with `spirv_cross_c.h`.
- **SDL2 on Windows comes from a pinned download when `find_package(SDL2)` fails (Peter).**
  - It's SDL2 2.32.10, the version the SDK ships today: `SDL2-devel-2.32.10-VC.zip` from SDL's GitHub release, checked against SHA256 `af347939395a58b365846aaea27391e69f9ec9d4dd650d6ac40802159b418a6e`.
  - When `VULKAN_SDK` is set, the SDK's SDL2 still wins (`CMakeLists.txt:99-101`), so Peter's builds don't change.
  - The examples are optional, so a failed download isn't fatal. They're skipped as they are today (`CMakeLists.txt:106-109`), and a warning says why.
  - Nothing puts the downloaded `SDL2.dll` on `PATH`, so it's copied beside each example, as the D3D12 runtime is.
- **The SDL2 version and hash are cache variables, as in `ZestD3D12.cmake:17-30`.** `ZEST_SDL2_VERSION` defaults to `2.32.10`, and `ZEST_SDL2_SHA256` is only filled in for that version.
- **Built examples create their device with `CreateExampleDevice`.** So only `example_platform.h` needs backend changes, and each example's main file changes only its define.
- **Unbuilt examples stay as they are.** `example-template` and `zest-timelinefx-c` stay Vulkan only, and the reload harness is skipped when Vulkan isn't built.

## Current state (verified 2026-11-25)

**CMake**

- `CMakeLists.txt:28`, `:41` and `:55` include ZestSlang, ZestD3D12 and ZestMetal. Vulkan comes after them, at `:68-91`:
  - `find_package(Vulkan COMPONENTS shaderc_combined)`
  - a `libs/vulkan` fallback on Windows (`:76`)
  - platform defines added to `CMAKE_CXX_FLAGS` (`:82`, `:84`)
  - `FATAL_ERROR "Could not find Vulkan library!"` (`:88`)
- `:94` adds `Vulkan_INCLUDE_DIRS` globally.
- `:97-109` find SDL2. On Windows they first add `$ENV{VULKAN_SDK}/cmake` to the prefix path, then call `find_package(SDL2 QUIET)`. Without SDL2 the examples are skipped (`:231-233`).
- `:125-228` link Vulkan and shaderc into `zest-interface` (`:127-194`), then again into `zest` (`:196-228`):
  - `/ignore:4099` on MSVC (`:133`, `:203`)
  - Slang's library, linked only in these branches (`:129`, `:165`, `:178`, `:187`, `:199`, `:208`, `:214`, `:222`)
  - AppKit and QuartzCore on Apple (`:136-140`)
  - the Linux shaderc libraries (`:144-160`)
  - the pkg-config fallback (`:171-183`)
  - `ZEST_NO_SHADERC` (`:191`, `:226`), which nothing reads
- `:40-52` is the D3D12 block and `:54-66` the Metal block. Each puts its compile definition on `zest-interface` INTERFACE and `zest` PRIVATE, then adds includes and links.
- `cmake/ZestMetal.cmake`:
  - `:27-44` look for `spirv_cross_c.h` and the seven libraries `c msl hlsl cpp reflect glsl core`, with `VULKAN_SDK` as the only hint, and otherwise stop with `FATAL_ERROR`.
  - `:48-51` set the deployment target to at least 15.0.
  - Metal gets shaderc only through the Vulkan link, though `zest_metal.h:49` includes `shaderc/shaderc.h`.
- `cmake/ZestD3D12.cmake`:
  - `:9-16` the option guard
  - `:17-30` the version and hash variables
  - `:75-111` the download, with `EXPECTED_HASH`, `cmake -E tar xf` and its error messages
  - `:174-189` `zest_copy_d3d12_runtime`
- `examples/SDL2/CMakeLists.txt`:
  - `:2-6` `zest_ignore_sdl2main_crt`
  - `:143-150` link `${SDL2_LIBRARIES}`
  - `:191-192` the per-target runtime functions
  - `:208-237` the example lists
  - `:250-252` `zest-c-compile-check`, and `:255-260` `zest-objc-compile-check`
  - `:267-270` the reload harness
- The reload harness:
  - `zest-timelinefx-reload/CMakeLists.txt:18-26` skips it off Windows or without its TimelineFX copy. It builds on Peter's machine.
  - It links `zest-interface` (`:93`).
  - `zest-timelinefx-reload-render.cpp:34` defines the Vulkan implementation, and `:423` calls `zest_implsdl2_CreateVulkanDevice`.

**Sources**

- `zest.cpp:5` defines `ZEST_VULKAN_IMPLEMENTATION` unconditionally. `:6-11` define D3D12 and Metal under their own defines.
- Two Vulkan device helpers in `zest.h` aren't guarded:
  - `zest_implglfw_CreateVulkanDevice` (`:23412-23427`) calls `zest_BeginVulkanDeviceBuilder`, which only `zest_vulkan.h` defines.
  - `zest_implsdl2_CreateVulkanDevice` (`:23513-23527`) calls `SDL_Vulkan_GetInstanceExtensions`, whose header is included only under `ZEST_VULKAN_IMPLEMENTATION` (`:23506-23508`).

  D3D12's and Metal's helpers are guarded (`:23488-23498`, `:23529-23543`), and so is the window code (`:23547-23611`).
- Probe (Mac, 2026-11-25): `zest.h` with only `ZEST_METAL_IMPLEMENTATION` and `<SDL.h>`, compiled as Objective-C C11, fails at `:23515` (`SDL_Vulkan_GetInstanceExtensions` undeclared). With `:23513-23527` under `#ifdef ZEST_VULKAN_IMPLEMENTATION`, `clang -fsyntax-only` is clean.
- Without Vulkan, `zest_implsdl2_CreateWindow` leaves out `SDL_WINDOW_VULKAN`, and on Apple it doesn't set `native_handle` (`zest.h:23547-23592`). Metal needs neither: it makes its layer with `SDL_Metal_CreateView` (`zest_metal.h:1946-1964`).
- `zest_dx12.h` includes neither Vulkan nor shaderc. D3D12 loads DXC at runtime.
- All 20 built examples, listed at `examples/SDL2/CMakeLists.txt:208-237`, work the same way:
  - Each defines `ZEST_VULKAN_IMPLEMENTATION` near the top of its main file, and `ZEST_METAL_IMPLEMENTATION` under `ZEST_ENABLE_METAL`.
  - Each creates its device with `CreateExampleDevice`.

  They are `zest-minimal-template`, `zest-clear-screen`, `zest-image-swizzle`, `zest-imgui-template`, `zest-ribbons`, `zest-implot`, `zest-pbr-deferred`, `zest-pbr-forward`, `zest-pbr-texture`, `zest-instancing`, `zest-indirect-draw`, `zest-shadow-mapping`, `zest-cascading-shadows`, `zest-fonts`, `zest-render-targets`, `zest-sdl2-imgui-template`, `zest-compute-example`, `zest-vaders`, `zest-timelinefx` and `zest-timelinefx-particle-techniques`.
- `examples/Common/example_platform.h`:
  - `:42-47` the default backend
  - `:53-56` `--vulkan`
  - `:70-86` the "not compiled into this build" exits for D3D12 and Metal
  - `:91-125` `CreateExampleDevice`, which falls back to Vulkan (`:97-100`, `:106-109`) and creates the Vulkan device at `:112-123`
  - `:170-175` call `ImGui_ImplSDL2_InitForVulkan`, which needs no Vulkan headers
- The test runner, `zest-tests.cpp`:
  - `:2` defines the Vulkan implementation.
  - `:197-199` register three tests that create Vulkan devices or call Vulkan backend code:
    - `zest-user-tests.cpp:779-789` and `:816-835` use `zest_BeginVulkanDeviceBuilder` and `VK_KHR_SURFACE_EXTENSION_NAME`.
    - `:791-799` uses `zest__validation_layers_with_sync_are_enabled` (`zest_vulkan.h:1368`).
  - `:215-232` is the Vulkan-only list.
  - `:317-343` `CreateSuiteDevice` falls through to Vulkan.
  - `:405-470` parse the arguments, with `run_vulkan` true by default.
  - `:474` runs Metal in a child process when Vulkan runs too.
  - `:499-517` the Vulkan run calls `SDL_Vulkan_GetInstanceExtensions`.
  - `:538-541` print its summary lines.
- `zest-c-compile-check.c:2` and `zest-objc-compile-check.m:2` define the Vulkan implementation unconditionally.

**Machines and packages**

- Windows:
  - `VULKAN_SDK=E:\VulkanSDK\1.4.335.0`, and its `cmake/` folder has SDL2 2.32.10.
  - Six SDK `Bin` folders are on `PATH`, each with an `SDL2.dll`.
  - CMake also searches `<prefix>/cmake` and `<prefix>/Lib` for each `PATH` entry, so an SDK on `PATH` is found even without `VULKAN_SDK`.
- `SDL2-devel-2.32.10-VC.zip` is 7,172,966 bytes, and GitHub's published SHA256 matches a local download. Its `SDL2-2.32.10/cmake/sdl2-config.cmake`:
  - sets `SDL2_LIBRARIES` to `SDL2::SDL2main SDL2::SDL2` (`:51`)
  - makes `SDL2::SDL2` a SHARED IMPORTED target, whose `IMPORTED_LOCATION` is the DLL (`:63-68`)
  - has builds in `lib/x86` and `lib/x64` only. Any other architecture sets `SDL2_FOUND` to FALSE (`:33-38`).
- The Mac mini runs macOS 26.2, with cmake 4.2.2 after `source ~/.zprofile`.
  - The SDK is at `~/VulkanSDK/1.4.335.1/macOS`, and is also installed system-wide in `/usr/local`: `libvulkan`, `libshaderc_combined.a`, the `libspirv-cross-*.a` libraries and their headers.
  - `VULKAN_SDK` is set only in `~/.zshrc`, so SSH builds find the SDK through `/usr/local`.
  - Homebrew's `sdl2` 2.32.10 is installed (minos 14.0).
- Homebrew bottles, fetched but not installed:
  - `shaderc` 2026.4: `include/shaderc/shaderc.h` and `lib/libshaderc_combined.a`, plus `libshaderc.a`, `libshaderc_shared.dylib` and `.pc` files
  - `spirv-cross` 1.4.363.0: `include/spirv_cross/spirv_cross_c.h`, the eight `libspirv-cross-*.a` libraries and CMake configs
  - Both bottles' objects are built for minos 26.0.
- The SDK's libraries give no linker warnings at 15.0 today: relinking `zest-minimal-template` in `../Zest-Build-Metal` printed none.
- CI, from run 45705559775:
  - Windows installs SDK 1.3.296.0. The install action rejects `optional_components: com.lunarg.vulkan.shaderc` as invalid. The SDK has no SDL2, so the examples are skipped ("SDL2 not found. Examples will be skipped.").
  - macOS uses arm64_tahoe bottles. It finds Vulkan and `shaderc_combined` in `/opt/homebrew/lib`, and builds every example with Metal off (`ci.yml:27-30`). `brew install sdl2` now installs `sdl2-compat`.
  - Linux uses apt packages.

**Wiring to mirror**

- The option guard and enabled flag: `ZestD3D12.cmake:9-16` and `ZestMetal.cmake:9-18`.
- The top-level block: `CMakeLists.txt:40-52`.
- The source defines: `zest.cpp:6-11`.
- The "not compiled in" exits: `example_platform.h:70-86`, and `zest-tests.cpp:419-447`.
- The suite device: `zest-tests.cpp:318-327`.
- The download and runtime copy: `ZestD3D12.cmake:75-111` and `:174-189`.

## Decisions for Peter

Both taken with the recommendation (Peter, 2026-11-26).

1. **The deployment target when Metal links Homebrew's libraries.** Homebrew builds its bottles for the running macOS (minos 26.0 here), and Zest targets 15.0. So the linker warns for every object it takes from them ("built for newer 'macOS' version (26.0) than being linked (15.0)"), and the binary may not run on macOS 15 anyway.
   - **Recommendation:** when Metal is on and SPIRV-Cross or shaderc (Metal's own, or Vulkan's) lies under the Homebrew prefix, raise the deployment target to the running macOS's major version. The status line says why: "deployment target 26.0, as Homebrew built SPIRV-Cross for this macOS. Use the Vulkan SDK's libraries to target macOS 15." SDK builds keep 15.0.
   - This also covers the CI runner, where Homebrew supplies SPIRV-Cross for every Metal build.
   - Other Homebrew libraries in an SDK build, such as SDL2, aren't checked.
   - On an Intel Mac, Homebrew's prefix is `/usr/local`, where the SDK's system-wide copy also goes. So an SDK build there would also be raised, which is harmless on the machine that built it.
   - **Alternative:** keep 15.0 and accept the warnings.
2. **CI jobs that prove the promise.** Today nothing would catch a change that brings an SDK dependency back.
   - **Recommendation:**
     - Add a `windows-2022` entry without the SDK step: D3D12 only, with SDL2 downloaded.
     - Add a `macos-latest` entry with only `sdl2 shaderc spirv-cross` from Homebrew: Metal only.
     - Turn Metal back on in the existing macOS entry, by adding `spirv-cross` to its `brew install`.
     - The existing Windows entry will now download SDL2, so it builds the examples and tests for the first time.
     - That's five jobs instead of three, and about twice the macOS minutes.
   - **Alternative:** add no jobs, and check the no-SDK builds by hand when this lands.

## Review Focus

1. **A build with the SDK changes.** With the SDK installed, `AUTO` must link what the build links today. Task 1 saves the link lines for `zest-tests`: the `.vcxproj`'s `AdditionalDependencies` on Windows, and `make VERBOSE=1` on the Mac. Tasks 2, 3 and 6 compare against them: they must list the same libraries, though Slang's may move.
2. **Two shaderc copies.** With Vulkan on, the Metal build must link one shaderc: Vulkan's. Check that the Mac default build's link line has one `shaderc_combined`, and that the Metal-only build's comes from the Metal module.
3. **Which wins, the SDK or Homebrew.** With both installed and `VULKAN_SDK` set, the configure must report the SDK's paths and deployment target 15.0. Task 7 configures both ways and reads the status lines.
   - Peter's Mac complicates this. SSH builds have no `VULKAN_SDK` and find the SDK in `/usr/local`.
   - Once Homebrew's libraries are installed, `PATH` order may hand a fresh SSH configure Homebrew's libraries instead.
   - So from Task 7 on, SDK builds on the Mac are configured with `VULKAN_SDK` set.
4. **A hidden SDK passes the no-SDK test.**
   - On Windows, an SDK `Bin` folder on `PATH` lets CMake find Vulkan and SDL2, and lets an example load `SDL2.dll`. The no-SDK configures and runs therefore unset `VULKAN_SDK` and remove every SDK entry from `PATH`.
   - On the Mac, `/usr/local` holds a full copy of the SDK. The no-SDK configures pass `-DCMAKE_IGNORE_PREFIX_PATH=/usr/local`.
   - The status lines must say that Vulkan wasn't found, and that SDL2 was downloaded or Metal's tools came from Homebrew.
   - Only Decision 2's CI jobs test a really clean machine.
5. **Metal without Vulkan runs differently.** Its windows lack `SDL_WINDOW_VULKAN`, and the Metal suite runs in-process rather than in a child process (`zest-tests.cpp:474`). The Metal-only builds must pass the Metal suite and backend in full, including the windowed tests and a `--shader-validation` run.

## Out of scope

- shaderc optional on every backend. Peter wants this as its own plan (2026-11-25, widened on 2026-11-26). It covers:
  - Zest's built-in shaders shipped already compiled, so they don't need shaderc. These are the debug overlay (`zest_vulkan.h:58-90`, `zest_metal.h:91-114`) and ImGui (`impl_imgui.h:91-121`), compiled today by `zest_CreateShader`.
  - Runtime GLSL compiling that loads shaderc only when it's present, as D3D12 loads DXC.
  - Shaders written in MSL, or Slang compiled straight to Metal, so Metal needs no GLSL compiler. This starts at `zest_metal.h`'s GLSL compile and `impl_slang.hpp`'s choice of target.
  - For Vulkan, a copy of the Vulkan headers in the repo and the loader opened at runtime, starting at `zest_vulkan.h:34-45`. Together with the items above, this lets the Vulkan backend build without the SDK. On its own it wouldn't, because the built-in shaders still need shaderc.
- Slang without the SDK on Windows. `ZestSlang.cmake:59-61` look only in the SDK. Homebrew's slang already works on the Mac.
- `ZEST_NO_SHADERC`, which is defined but read nowhere. It moves into the module unchanged.
- SDL2 for Windows on ARM64. The VC package has no ARM64 build, so the examples are skipped there, as they are today.
- Linux, which doesn't change. Vulkan stays its only backend, from the distro packages.
- Runtime. Built apps already need no SDK, except for the validation layers and, for Vulkan on the Mac, MoltenVK.

## Phase 1: Vulkan becomes optional

Everything in this phase can be checked with the SDK still installed, by configuring with `-DZEST_ENABLE_VULKAN=OFF`.

### Task 1: Baselines

**Files:** none.

**Steps:**

- [x] Bring the Mac clone up to Peter's latest commit with `git pull`. It was at 9b25e45, with only `spikes/` untracked. Expected: a clean tree at HEAD.
- [x] Windows: configure `E:/Projects/C++/Zest-Build`, and build Debug and Release. Save the configure output, and the Debug `AdditionalDependencies` from `zest-tests.vcxproj`. Run `zest-tests.exe --platform all` and record its four summary lines.
- [x] Mac: rebuild `../Zest-Build-Metal` and `../Zest-Build`. Save each configure output and the `zest-tests` link line from `make VERBOSE=1`. Run `--platform all` in each, under `caffeinate -d`, and record the summary lines.
- [x] Record the warnings in each build. Expected: only the existing freetype and CMake deprecation ones.

### Task 2: The ZestVulkan.cmake module

**Files:** create `cmake/ZestVulkan.cmake`. Modify `CMakeLists.txt`:
- `:27-37` the Slang link
- `:54-55` the include order
- `:68-228`, which the module replaces

**Interfaces:**

- The header comment mirrors the other modules': `# Vulkan support without add_subdirectory(zest): set ZEST_ENABLE_VULKAN, include this, link ${ZEST_VULKAN_LIBRARIES}`.
- The option, under `if(NOT DEFINED ZEST_ENABLE_VULKAN)`:
  - `set(ZEST_ENABLE_VULKAN AUTO CACHE STRING "Build the Vulkan backend: AUTO when CMake finds Vulkan, ON to require it, OFF to leave it out")`
  - `set_property(CACHE ZEST_ENABLE_VULKAN PROPERTY STRINGS AUTO ON OFF)`
- The outputs, reset on every include:
  - `ZEST_VULKAN_ENABLED`
  - `ZEST_VULKAN_LIBRARIES`: `Vulkan::Vulkan`, shaderc, the Linux libraries and the Apple frameworks
  - `ZEST_VULKAN_INCLUDE_DIRS`
  - `ZEST_VULKAN_LIBRARY_DIRS`: pkg-config's library folders
  - `ZEST_VULKAN_DEFINITIONS`: `ZEST_NO_SHADERC` when shaderc is missing
  - `ZEST_VULKAN_SHADERC_FOUND`
- The messages:
  - `AUTO` without Vulkan: `STATUS "Vulkan not found, so the Vulkan backend isn't built. Install the Vulkan SDK to build it."`
  - `ON` without Vulkan: `FATAL_ERROR "ZEST_ENABLE_VULKAN is ON but CMake couldn't find Vulkan. Install the Vulkan SDK, or configure with -DZEST_ENABLE_VULKAN=AUTO to build without it."`
  - Found: `STATUS "Vulkan support enabled: ${Vulkan_LIBRARY}"`. This replaces `:90`.
- `CMakeLists.txt` changes:
  - It includes `ZestVulkan.cmake` just before `ZestMetal.cmake`.
  - A Vulkan block in the form of `:40-52`:
    - `ZEST_ENABLE_VULKAN` goes on `zest-interface` INTERFACE and `zest` PRIVATE.
    - `ZEST_VULKAN_DEFINITIONS` goes where `ZEST_NO_SHADERC` goes today: INTERFACE and PUBLIC.
    - The includes, library folders and libraries go on both targets, with `/ignore:4099` on MSVC.
  - The global `include_directories` (`:94`) gives way to the target includes.
  - The platform defines in `CMAKE_CXX_FLAGS` (`:82`, `:84`) are added only when Vulkan is built.
  - After the three modules, the no-backend check: `if(NOT ZEST_VULKAN_ENABLED AND NOT ZEST_D3D12_ENABLED AND NOT ZEST_METAL_ENABLED)` gives `FATAL_ERROR "Zest has no backend to build. Install the Vulkan SDK (on Linux, your distribution's Vulkan and shaderc packages), or turn on D3D12 on Windows or Metal on Apple."`
  - `${SLANG_LIBRARY_LINK}` is linked to both targets in the Slang block.

**Steps:**

- [x] Write the module and replace `:68-228`. Move Slang's link.
- [x] Windows: reconfigure and build `E:/Projects/C++/Zest-Build`. Expected:
  - "Vulkan support enabled"
  - the same `AdditionalDependencies` as Task 1, though Slang's library may move
  - no new warnings
  - Task 1's test lines
- [x] Configure scratch folders. Expected:
  - with `-DZEST_ENABLE_VULKAN=ON`: the same result as `AUTO`
  - with `-DZEST_ENABLE_VULKAN=OFF -DZEST_ENABLE_D3D12=OFF`: the no-backend error

  Don't build with Vulkan off until Task 4.
- [x] Mac: sync, then reconfigure and build `../Zest-Build-Metal` and `../Zest-Build`. Expected: the link lines match Task 1's, though Slang's library may move.

### Task 3: Metal finds its own shaderc

**Files:** `cmake/ZestMetal.cmake:27-46`, and the Metal block in `CMakeLists.txt` (`:54-66` before Task 2's move).

**Interfaces:**

- When `NOT ZEST_VULKAN_SHADERC_FOUND`, look for shaderc, using the same hints as SPIRV-Cross:
  - `find_path(ZEST_METAL_SHADERC_INCLUDE_DIR NAMES shaderc/shaderc.h ...)`
  - `find_library(ZEST_METAL_SHADERC_LIBRARY NAMES shaderc_combined ...)`

  Otherwise both stay empty. A missing shaderc stops the configure with a `FATAL_ERROR` in the form of the SPIRV-Cross ones.
- The comment above the search: `# Vulkan's shaderc serves Metal too when Vulkan is built, so include ZestVulkan.cmake first`.
- The top-level Metal block adds `${ZEST_METAL_SHADERC_INCLUDE_DIR}` and `${ZEST_METAL_SHADERC_LIBRARY}` to both targets.
- The status line names where shaderc came from: `Metal support enabled: SPIRV-Cross from <dir>, shaderc from <dir, or "the Vulkan build">, deployment target <version>`.

**Steps:**

- [x] Edit the module and the Metal block.
- [x] Reconfigure `../Zest-Build-Metal`. Expected: "shaderc from the Vulkan build", and one `shaderc_combined` on the link line, as in Task 1.
- [x] Configure `../Zest-Build-MetalOnly` with `-DZEST_ENABLE_VULKAN=OFF`. Expected: Vulkan isn't built, and the Metal line names `/usr/local` for SPIRV-Cross and shaderc. Build it in Task 4.

### Task 4: Sources

**Files:**

- `zest.h:23412-23427` and `:23513-23527`: wrap each definition in `#ifdef ZEST_VULKAN_IMPLEMENTATION` / `#endif`, as `:23488-23498` is.
- Put `#define ZEST_VULKAN_IMPLEMENTATION` under `#ifdef ZEST_ENABLE_VULKAN` in:
  - `zest.cpp:5`
  - each of the 20 built examples' main files (listed under Current state)
  - `zest-c-compile-check.c:2` and `zest-objc-compile-check.m:2`
- `examples/Common/example_platform.h`:
  - `--vulkan` without `ZEST_VULKAN_IMPLEMENTATION` prints "Vulkan is not compiled into this build" and exits 1, beside `:70-75`.
  - The fallback at `:97-100` and `:106-109` happens only under `ZEST_VULKAN_IMPLEMENTATION`. Without Vulkan, a D3D12 or Metal device that fails gives NULL, and `:115-117` reports it.
  - `:112-123` go under `ZEST_VULKAN_IMPLEMENTATION`.
  - The comments at `:6` and `:90` say that the fallback needs Vulkan compiled in.
- `examples/SDL2/zest-timelinefx-reload/CMakeLists.txt`: add a third skip after `:18-26`. Under `if(NOT ZEST_VULKAN_ENABLED)`, `message(STATUS "zest-timelinefx-reload: skipped, it runs on Vulkan only and the Vulkan backend isn't built")`, then `return()`.

**Steps:**

- [x] Edit, then check the CRLF files with `file`.
- [x] Build `E:/Projects/C++/Zest-Build`. Expected: the same build and test results as Task 1.
- [x] Configure `E:/Projects/C++/Zest-Build-NoVulkan` with `-DZEST_ENABLE_VULKAN=OFF`, and build every target except `zest-tests` in Debug. Expected: no warnings, and the reload harness skipped with its new line. `zest-minimal-template.exe --vulkan` prints "Vulkan is not compiled into this build" and exits 1 before it opens a window.
- [x] Mac: build every target except `zest-tests` in `../Zest-Build-MetalOnly`, including both compile checks. Expected: no warnings.

### Task 5: The test runner

**Files:** `zest-tests.cpp`:
- `:2` the define
- `:197-199` the three registrations
- `:317-343` `CreateSuiteDevice`
- `:405-470` the argument parsing
- `:499-517` the Vulkan run

Also `zest-user-tests.cpp:779-835`, the three tests.

**Interfaces:**

- `run_vulkan` starts as `ZEST_FALSE` and is set under `#ifdef ZEST_ENABLE_VULKAN`, as `run_d3d12` is.
- `--platform vulkan` without Vulkan prints `The Vulkan backend isn't compiled in, install the Vulkan SDK and configure with -DZEST_ENABLE_VULKAN=ON`, and returns 1. `all` sets `run_vulkan` only under the define.
- `CreateSuiteDevice` builds a Vulkan device only under the define, and returns NULL otherwise. `RunSuite` already reports a NULL device.
- The block at `:499-517` goes under the define, as it calls `SDL_Vulkan_*`. The summary lines at `:538-541` are already behind `run_vulkan`.
- The three tests' definitions and registrations go under the define, together with any static helper that only they use, so nothing is left unused.

**Steps:**

- [x] Edit, then build `E:/Projects/C++/Zest-Build` and run `--platform all`. Expected: Task 1's lines.
- [x] Build and run `zest-tests` in `E:/Projects/C++/Zest-Build-NoVulkan` with no arguments. Expected: the D3D12 suite and backend lines match Task 1's, there are no Vulkan lines, and the exit code is 0. `--platform vulkan` prints the message and exits 1.
- [x] Mac: build `zest-tests` in `../Zest-Build-MetalOnly`, and run it under `caffeinate -d` with no arguments, then again with `--shader-validation`. Expected: the Metal suite and backend lines match Task 1's both times.
- [x] Mac: run `--platform all` in `../Zest-Build-Metal` and `../Zest-Build`. Expected: Task 1's lines.
- [x] Run `qgrep update zest`.

## Phase 2: Without the SDK

### Task 6: SDL2 for Windows

**Files:**
- the SDL2 section of `CMakeLists.txt` (`:97-109`)
- `examples/SDL2/CMakeLists.txt`: `:2-6`, `:191`, and `:268-270`

**Interfaces:**

- After `find_package(SDL2 QUIET)`, under `if(NOT SDL2_FOUND AND WIN32)`:
  - Download `https://github.com/libsdl-org/SDL/releases/download/release-${ZEST_SDL2_VERSION}/SDL2-devel-${ZEST_SDL2_VERSION}-VC.zip` to `${CMAKE_BINARY_DIR}/_deps/`, using `STATUS`.
  - Check the zip with `file(SHA256 ...)` against `ZEST_SDL2_SHA256`, rather than `EXPECTED_HASH`, so a mismatch skips the examples instead of failing the configure.
  - Extract it with `cmake -E tar xf`, as `ZestD3D12.cmake:96-104` does.
  - Then `find_package(SDL2 CONFIG PATHS "${CMAKE_BINARY_DIR}/_deps/SDL2-${ZEST_SDL2_VERSION}/cmake" NO_DEFAULT_PATH)`.
  - Skip the download when the extracted folder already exists.
- `ZEST_SDL2_VERSION` is a cache STRING that defaults to `2.32.10`. `ZEST_SDL2_SHA256` is set only for 2.32.10. Another version without a hash gets a warning and no download.
- A failed download, a hash mismatch, or a package with no build for the architecture deletes the zip and warns: `WARNING "Couldn't get SDL2 ${ZEST_SDL2_VERSION}: <reason>. The examples will be skipped. Install the Vulkan SDK with SDL2, or set SDL2_DIR to an SDL2 install."`
- `ZEST_SDL2_DOWNLOADED` is TRUE when SDL2 came from the download.
- The hint at `:108` mentions the download.
- `zest_copy_sdl2_runtime(TARGET_NAME)` goes after `zest_ignore_sdl2main_crt` (`examples/SDL2/CMakeLists.txt:2-6`). When `ZEST_SDL2_DOWNLOADED`, it adds a POST_BUILD copy of `$<TARGET_FILE:SDL2::SDL2>` to `$<TARGET_FILE_DIR:${TARGET_NAME}>`. Call it beside `zest_copy_d3d12_runtime` (`:191`), and for the reload render target (`:268-270`).

**Steps:**

- [x] Edit, then reconfigure `E:/Projects/C++/Zest-Build`. Expected: SDL2 from the SDK, no download, and `AdditionalDependencies` unchanged.
- [x] Configure `E:/Projects/C++/Zest-Build-NoSDK` from a shell with `VULKAN_SDK` unset and every `VulkanSDK` entry removed from `PATH`. Expected: "Vulkan not found...", the SDL2 download, and D3D12 enabled.
- [x] Build it in Debug. Expected: no warnings, and `SDL2.dll` beside every example and `zest-tests`.
- [x] Run `zest-tests` in the same environment. Expected: the D3D12 suite and backend lines match Task 1's. `where SDL2.dll` finds nothing on `PATH`.
- [x] Reconfigure `Zest-Build-NoSDK`. Expected: no second download.
- [x] Configure a scratch folder with a wrong `ZEST_SDL2_SHA256`. Expected: the warning, the examples skipped, and a configure that still succeeds.

### Task 7: Metal from Homebrew

**Files:** `cmake/ZestMetal.cmake`: the SPIRV-Cross and shaderc searches, their messages, and the deployment target.

**Interfaces:**

- The search order:
  1. When `VULKAN_SDK` is set, a pass with `HINTS $ENV{VULKAN_SDK} NO_DEFAULT_PATH`.
  2. Then the default search, with Homebrew's prefix as `PATHS`. The prefix is `$ENV{HOMEBREW_PREFIX}`, else `/opt/homebrew`.
- The error messages end: "Install the Vulkan SDK and set VULKAN_SDK to its macOS folder, or run brew install spirv-cross shaderc. Configure with -DZEST_ENABLE_METAL=OFF to build without Metal."
- Under Decision 1's recommendation:
  - When the SPIRV-Cross library, or the shaderc Metal uses, lies under Homebrew's prefix, raise `CMAKE_OSX_DEPLOYMENT_TARGET` to the major version from `sw_vers -productVersion` (`26.0` here). Only ever raise it.
  - The status line says why, in the words given in Decision 1.
  - Vulkan's shaderc path is `Vulkan_shaderc_combined_LIBRARY`, or pkg-config's library folders.

**Steps:**

- [x] Run `brew install shaderc spirv-cross` on the Mac.
- [x] Check Decision 1's premise before changing the deployment target. Configure `../Zest-Build-Homebrew` with `VULKAN_SDK` unset, `-DZEST_ENABLE_VULKAN=OFF` and `-DCMAKE_IGNORE_PREFIX_PATH=/usr/local`, then build `zest-minimal-template`. Expected: the minos warnings. Record one.
- [x] Apply Decision 1 and rebuild. Expected: no warnings, and a status line with Homebrew's paths and deployment target 26.0.
- [x] Build every target in `../Zest-Build-Homebrew`, and run `zest-tests` with no arguments and with `--shader-validation`. Expected: Task 1's Metal lines both times. `otool -L zest-tests` shows no `libvulkan` and nothing from `/usr/local`.
- [x] Delete `../Zest-Build-Metal/CMakeCache.txt`, then reconfigure with `VULKAN_SDK` set. Expected: the SDK's paths, deployment target 15.0, and the link line from Task 1.
- [x] Configure a scratch folder fresh without `VULKAN_SDK`. Record which libraries it picks, for Review Focus 3.

## Phase 3: CI and docs

### Task 8: CI

**Files:** `.github/workflows/ci.yml:22-30` and `:39-56`.

**Interfaces:**

- Per Decision 2, add matrix fields as needed: one for whether to install the SDK on Windows, and one for each macOS entry's brew packages.
- Remove `optional_components` (`:45`), which the action rejects as invalid.

**Steps:**

- [x] Edit, then check that the YAML parses.
- [ ] The implementer can't run CI without a commit, so Peter checks the first run after he commits. Expected: every job green.
  - The no-SDK Windows log shows "Vulkan not found" and the SDL2 download.
  - The Metal-only macOS log shows Homebrew's SPIRV-Cross and shaderc.
  - Both Windows jobs build the examples and `zest-tests`. This is the first time CI builds them on Windows, so new MSVC warnings there are possible.

### Task 9: Docs, skills and memory

**Files:**

- `docs/getting-started/installation.md`:
  - `:7` Prerequisites: the SDK is needed only for the Vulkan backend. D3D12 on Windows needs nothing more. Metal on the Mac needs the SDK, or `brew install shaderc spirv-cross`.
  - `:10` SDL2 isn't a submodule. It comes from the SDK or a package manager, or on Windows it's downloaded.
  - `:45-46` the submodule note drops SDL2.
  - A short "The Vulkan Backend" section in the form of `:88-110`: `ZEST_ENABLE_VULKAN` and its three values, and defining `ZEST_VULKAN_IMPLEMENTATION` under `#ifdef ZEST_ENABLE_VULKAN`.
  - `:105` and `:129` the examples fall back to Vulkan only when it's compiled in.
  - `:114-125` Metal: Homebrew, and Decision 1's deployment target.
  - `:264-269` SDL2 troubleshooting.
- `README.md:83-89`: add an `-DZEST_ENABLE_VULKAN=OFF` row, and fix the Metal row ("needs the Vulkan SDK's SPIRV-Cross").
- `CLAUDE.md`: add `ZEST_ENABLE_VULKAN` to its CMake flags.
- The tutorials, `docs/index.md`, `docs/api-reference/index.md`, `docs/examples/index.md` and `first-application.md` show app code that defines `ZEST_VULKAN_IMPLEMENTATION`. That's still right for a Vulkan app, so change only text that says the SDK is required.
- Skills:
  - `.claude/skills/metal-backend/SKILL.md:22`, the Build bullet
  - `.claude/skills/d3d12-backend/SKILL.md:197`, the fallback
- Memory: replace `vulkan-sdk-optional-plan-pending` with what landed, and link the MSL and Slang plan that follows.
- This plan: add `## As built (<date>)`.

## Per-frame cost

| Path | SDK build (Vulkan on) | Vulkan off |
|---|---|---|
| Per frame | nothing | nothing |
| Per pass, draw or dispatch | nothing | nothing |
| Device creation | nothing | nothing; examples no longer fall back to Vulkan |
| GPU | nothing | nothing |

Memory: none. A build without Vulkan is smaller, because `zest_vulkan.h` and shaderc are left out on Windows, and `zest_vulkan.h` on the Mac.

## Testing

**Automated (implementer)**, with the expected lines taken from Task 1:

- Windows `Zest-Build` (SDK): Debug and Release build with no new warnings. `--platform all` gives Task 1's dynamic, legacy, D3D12 suite and D3D12 backend lines.
- Windows `Zest-Build-NoVulkan` and `Zest-Build-NoSDK`: everything builds. `zest-tests` gives Task 1's D3D12 lines, and `--platform vulkan` exits 1 with its message. `Zest-Build-NoSDK` runs with no SDK on `PATH`.
- Mac `../Zest-Build-Metal` and `../Zest-Build`: Task 1's lines, and the same link lines.
- Mac `../Zest-Build-MetalOnly` and `../Zest-Build-Homebrew`: the Metal suite and backend in full, with and without `--shader-validation`.
- Configure checks:
  - `AUTO` without the SDK gives the status line.
  - `ON` without the SDK gives the error.
  - No backend gives the no-backend error.
  - A wrong SDL2 hash skips the examples.
  - A second configure doesn't download again.

**Peter's tests:**

- There are no new tests. The existing tests nearest the change are in the runs above: "Metal Windowed Frames" (now without `SDL_WINDOW_VULKAN`), "D3D12 Pool Failure Rejected", and the three Vulkan-only user tests, which still run in the SDK builds.
- CI's first run after the commit (Task 8).

**Visual checks for Peter:**

1. Windows, from a shell without the SDK (`VULKAN_SDK` unset, no SDK on `PATH`): run `zest-minimal-template`, `zest-imgui-template` and `zest-pbr-deferred` from `Zest-Build-NoSDK`. Each opens on D3D12 and draws, and the ImGui windows work. `--vulkan` prints "Vulkan is not compiled into this build".
2. Mac, from `../Zest-Build-Homebrew`: run the same three. Each opens on Metal and draws. Drag an ImGui window outside the main window to check that viewports work without `SDL_WINDOW_VULKAN`.
3. Both machines, SDK builds: the examples still start on D3D12 or Metal, and `--vulkan` still runs them on Vulkan.

## As built (2026-11-26)

All three phases are in the working tree, uncommitted. Results are in each phase's report; the differences from the plan are below.

**CMake**

- The Metal block links `${ZEST_METAL_FRAMEWORKS}` before the SPIRV-Cross libraries. Vulkan and Metal both link `-framework QuartzCore`, and with Vulkan's block first, CMake linked the SPIRV-Cross archives twice to satisfy the order around the two copies. `ld` then warned "ignoring duplicate libraries" on every Mac target.
- `zest_ignore_sdl2main_crt` also passes `/ignore:4099`. The SDK's `SDL2main.lib` (and the downloaded one) ships without its PDB, and the Vulkan block's `/ignore:4099` used to hide that. Without Vulkan, every example warned.
- The `VK_USE_PLATFORM_*` flags are set in the top-level Vulkan block, not in `ZestVulkan.cmake`, so including the module never changes a host's `CMAKE_CXX_FLAGS`.
- The `libs/vulkan` fallback is relative to the module (`${CMAKE_CURRENT_LIST_DIR}/../libs/vulkan`, the same folder in Zest's own build) and searches its own variable, `ZEST_VULKAN_BUNDLED_LIBRARY`, with `NO_DEFAULT_PATH`.
  - The old code reused `Vulkan_LIBRARY`, which FindVulkan leaves set when it rejects an install that has a `vulkan-1.lib` but no headers or shaderc. The fallback then declared Vulkan found, so under `AUTO` a partial Vulkan on `PATH` turned a D3D12-only build into a configure error, as there's no `Vulkan::Vulkan` target. A probe in the sanity check reproduced that, and the fix removes it.
  - A real `libs/vulkan` copy still triggers the fallback, and still can't link, for the same reason. That was already so before this work, and the fallback stays as Settled.
- The "Using module to find Vulkan" line and its CMake 3.7 check are gone; the minimum is 3.8. `OFF` prints nothing, as D3D12 and Metal print nothing when off.
- `ZEST_SDL2_VERSION` and `ZEST_SDL2_SHA256` exist only on Windows. A reconfigure finds the download through the cached `SDL2_DIR`, so `ZEST_SDL2_DOWNLOADED` is set by comparing `SDL2_DIR` with the download folder, and a "Using the downloaded SDL2" status line says so each time. The second `find_package` is `QUIET`, as the module gives its own warning.
- Decision 1's status line names what Homebrew built: "SPIRV-Cross", "shaderc" or both.

**Sources**

- `example_platform.h`: `--vulkan` is recognised as a named backend that is neither D3D12 nor Metal. The "Could not create the device" check stays outside the Vulkan guard, so a failed D3D12 or Metal device still reports. The comment at the top also names `ZEST_VULKAN_IMPLEMENTATION` under `ZEST_ENABLE_VULKAN`.
- `zest-user-tests.cpp`: `create_device_with_failing_pool` and its two helpers sit under `ZEST_ENABLE_VULKAN || ZEST_ENABLE_D3D12`, as the D3D12 tests use them too. `live_crt_heap_bytes` and `<crtdbg.h>` go under `ZEST_ENABLE_VULKAN` with the three tests.
- `CreateSuiteDevice` casts its Vulkan-only parameters to `void` when Vulkan isn't compiled in.

**CI**

- Both Windows jobs build Debug (Peter, 2026-11-26). `zest-tests` reads barrier records that exist only under `ZEST_DEBUGGING`, which Release's `NDEBUG` removes, and Windows CI now builds the examples because SDL2 is downloaded. Linux and macOS pass no build type, so they were never hit.
- The SDK job installs `optional_components: com.lunarg.vulkan.debug` for `shaderc_combinedd.lib`, which a Debug build links. The name is in the allowlist of both v1.0.5 and main; whether SDK 1.3.296's installer has that component is unverified until the first run.
- The step sets `cache: false`. v1.0.5 only caches when asked, but later versions cache by default under a key (`cache-<platform>-<arch>-vulkan-sdk-<version>`) that leaves out the components, and a cache hit skips the install. So a cached SDK without the debug component would break the Debug link after an action bump.
- Each job has a `name`, so the two Windows and two macOS jobs can be told apart.

**Testing**

- Builds without Slang run 7 fewer shared tests (the Slang ones): the D3D12 suite in `Zest-Build-NoSDK` is 129/129, and the Metal suite in `../Zest-Build-Homebrew` is 129/129. Slang needs the SDK on Windows (out of scope), and Homebrew's slang isn't installed on the Mac mini.
- The no-SDK Windows environment also drops drive roots from `PATH`. Peter's `PATH` has `E:\`, and CMake searches `<root>/SDL2*/cmake`, which finds the SDL2 package in `E:\SDL2` and skips the download.
- The Mac baseline (Task 1) ran with the screen locked, so its Metal backend line was 56/58. Every later Mac run was unlocked and gave 58/58.
- `../Zest-Build-Metal`'s old cache is in `spikes/vsdk/Zest-Build-Metal-CMakeCache.txt.bak` on the Mac, not deleted.

**Review Focus 3, recorded**

- With Homebrew's `shaderc` and `spirv-cross` installed, a fresh SSH configure without `VULKAN_SDK` takes `libvulkan` and its headers from `/usr/local`, but Vulkan's `shaderc_combined` and SPIRV-Cross from `/opt/homebrew`, at deployment target 26.0. FindVulkan searches the `PATH`-derived `/opt/homebrew` first, and Homebrew has no `libvulkan`. That pairs the SDK's `shaderc.h` with Homebrew's shaderc library. The install guide says to set `VULKAN_SDK` when both are installed.

**Docs**

- `CLAUDE.md` and `.claude/skills/` are git-ignored, so their edits are local only.
- Beyond the lines Task 9 names, the two skills' "How it plugs in" lines, the Mac skill's build folder list and its `--platform all` note were updated, as the work made them wrong.
- The other docs never said the SDK was required, so only `installation.md` and the README table changed.
- From the sanity check, all of these were already wrong or incomplete before this work:
  - `ZestMetal.cmake`'s header and the install guide now tell hosts to include `ZestVulkan.cmake` first, and to add the shaderc include folder and library.
  - The guide's Windows test command builds and runs Debug.
  - The README and the guide give CMake 3.24 as the minimum, the first FindVulkan with the shaderc component. `cmake_minimum_required` stays at 3.8.
