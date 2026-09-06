# zest-timelinefx-reload

A correctness harness for running TimelineFX inside a host DLL that gets unloaded and
reloaded while the simulation keeps running. It is not a demo — the point is to catch
bugs, so it fails loudly and asserts on continuity of particle state rather than on the
absence of a crash.

## Topology

```
platform.exe                          persistent. owns the 128MB TimelineFX pool and the
  zest-timelinefx-reload.exe          allocate/deallocate callbacks. in the rendered build
  zest-timelinefx-reload-render.exe   it also owns the window, device, swapchain and every
                                      GPU resource. never reloads.
    |  LoadLibrary / FreeLibrary
    v
engine.dll (zest-tfx-reload-engine.dll)
    reloaded on demand. statically links TimelineFX and nothing else.
    +-- TimelineFX   dies and comes back as a fresh image with a zeroed data segment,
                     while its state stays put in platform.exe memory.
```

TimelineFX is compiled into `engine.dll` and **nowhere else**. Two copies would each get
their own `tfxCurrentContext` and the test would be meaningless. zest is linked into
`zest-timelinefx-reload-render.exe` and **nowhere else** — never into `engine.dll` — so a
broken reload can never be blamed on zest.

## The two stages

| | `zest-timelinefx-reload` | `zest-timelinefx-reload-render` |
|---|---|---|
| Stage | 1, headless | 2, rendered |
| Links | no zest, no Vulkan, no SDL | zest + SDL2 + Vulkan |
| What reloads | TimelineFX only | TimelineFX only |
| Extra assertions | continuity, motion tolerance, pool drift, negative cases | uv_lookup re-registration via a GPU-shape checksum, frames kept presenting |

They share `tfx_reload_platform.{h,cpp}` (the pool, the module load/unload pair, the one
reload sequence) and the same `engine.dll`.

## Build

The reload API (`tfx_GetContext`, `tfx_SetContext`, `tfx_SuspendTimelineFX`,
`tfx_ResumeTimelineFX`) does not exist on TimelineFX `master`. `CMakeLists.txt` points
`TIMELINEFX_RELOAD` at a local working copy carrying it:

```cmake
set(TIMELINEFX_RELOAD "E:/Projects/C++/TimelineFX/TFXEditor/TimelineFXLib/" CACHE PATH ...)
```

Override with `-DTIMELINEFX_RELOAD=<path>`. The shared `TIMELINEFX` variable and
`submodules/timelinefxlib` are untouched, so every other TimelineFX example keeps building
against the submodule copy on `master`. If the path does not exist the harness is skipped
with a warning and the rest of the build proceeds.

```bash
cmake --build E:/Projects/C++/Zest-Build --config Release --target zest-timelinefx-reload
cmake --build E:/Projects/C++/Zest-Build --config Release --target zest-timelinefx-reload-render
```

## Run

Run from the repository root — the effects library path is relative to it.

```bash
BIN=E:/Projects/C++/Zest-Build/examples/SDL2/zest-timelinefx-reload/Release

# stage 1: 50 reloads headless. exit 0 on pass, 1 on any failed check.
$BIN/zest-timelinefx-reload.exe

# the three negative cases, each in its own process
$BIN/zest-timelinefx-reload.exe --negatives

# stage 2: 50 reloads while rendering, then exits
$BIN/zest-timelinefx-reload-render.exe
```

Options, both binaries: `--iterations=N` (50), `--warmup=N` (300), `--frames-between=N`
(30), `--effect=NAME` (`Background`), `--library=PATH`, `--verbose`. Headless also takes
`--case=no-suspend|no-setcontext|drop-callback` to run one negative case in-process;
rendered also takes `--windowed-forever` to keep drawing after the reload loop.

`TFX_RELOAD_TRACE=1` in the environment makes `engine.dll` narrate init and each reload.

The simulation uses a fixed 16.667 ms timestep and a fixed stage seed so runs are
comparable.

## What it asserts

1. **Continuity.** Particle positions are captured on the frame before `engine_detach`.
   After reattach one frame is run and each sampled particle is matched to its nearest
   neighbour in the new cloud — the instance buffer is rebuilt every frame and its order is
   not a stable identity, so a positional match is the only honest comparison. The tolerance
   comes from the effect's *measured* per-frame motion during warmup, not from a guess. A
   state reset moves the whole cloud back to the spawn point and every match distance blows
   up.
2. **Particles are alive across the boundary.** A zero population on either side fails the
   iteration outright — it would prove nothing.
3. **50 reloads**, because slow drift and monotonic growth do not show up in one.
4. **Pool usage is flat.** `tfx_CreateMemorySnapshot` over every pool after each reload; the
   mean of the late half must not exceed the early half. The platform's own bump allocator
   cursor must also not move after init, which catches a whole leaked pool.
5. **Clean shutdown.** `tfx_EndTimelineFX`'s stdout is captured and must contain "Successful
   shutdown of TimelineFX." and no leak report, and every block the platform handed out must
   have come back.

Two of the checks exist to stop the harness passing for the wrong reason:

- **the module really was unmapped.** `FreeLibrary` reports success whether or not the
  unload happened, so `module_unload` follows it with a `VirtualQuery` on the old base.
- **the data segment really came back zeroed.** `engine_reattach` reads its own globals
  before restoring anything; if they still hold the previous generation's values, no reload
  took place.

Supporting checks: the context pointer must land inside the platform's `VirtualAlloc` block
(proving the state is host-owned), TimelineFX must actually be multithreaded (otherwise the
suspend requirement is untested), and the re-registered Tier 2 update callback must keep
firing after every reload.

The rendered harness adds one the headless one cannot make. `uv_lookup` is stored on the
library and nulled by `tfx_SetContext`; it belongs to `platform.exe` here, but it still has
to be handed back through `engine_saved_state_t` and re-registered. So every iteration
rebuilds the GPU shape data through it and checksums the result against a reference. A
missed re-registration shows up as a mismatch on the next reload instead of as silently
wrong uvs on screen much later.

## The negative cases

`--negatives` spawns each as a child process and asserts on its exit code.

| Case | What it skips | Observed |
|---|---|---|
| `no-suspend` | `tfx_SuspendTimelineFX` before `FreeLibrary` | **no crash** — the unload is silently refused. See below. |
| `no-setcontext` | `tfx_SetContext` after the reload | access violation on the first TimelineFX touch |
| `drop-callback` | re-registering the update callback `tfx_SetContext` nulled | callback stops firing, no crash |

`no-suspend` does not behave the way the roadmap predicted, and the real behaviour is worse.
MSVC's `_beginthreadex` holds a module reference for the lifetime of every thread whose
start routine lives in that module. TimelineFX creates all its threads that way, so with a
live instance `FreeLibrary` returns success and unloads nothing at all: the old image stays
mapped, its globals keep their values, and the "reload" goes on running the previous
generation's code while leaking a module and 23 threads per iteration. `tfx_SuspendTimelineFX`
is what makes the unload possible, not merely what makes it safe. The harness asserts on the
refused unload, which is directly observable, rather than on a crash that never comes.

## The reload contract

The whole sequence lives in `harness_reload()` in `tfx_reload_platform.cpp` and nothing else
does the unload/load pair:

```c
engine_detach(&saved, flags);      // internally: tfx_SuspendTimelineFX(); saved.context = tfx_GetContext();
FreeLibrary(engine);
engine = LoadLibrary(next_generation_copy);
engine_reattach(&saved, &callbacks, flags);
                                   // internally: tfx_SetContext(...); re-register callbacks; tfx_ResumeTimelineFX();
```

Each generation is loaded from its own copy of the DLL so the module comes back as a fresh
image, as it would in a real rebuild-and-reload loop.

`engine_saved_state_t` carries more than the context: `engine.dll`'s own globals are zeroed
by the reload, so the library, stage, effect template, effect id and the caller's
`uv_lookup` are stashed alongside it. All of them are pointers into the platform-owned pool
or into `platform.exe` itself, so they stay valid. A real engine.dll would do exactly this.

## Notes

- The allocate/deallocate callbacks live in `platform.exe`. If they lived in `engine.dll`
  their addresses would dangle across the reload like every other pointer.
- Windows only for now. The module layer is isolated in `module_load`/`module_unload` so a
  `dlopen` path can be added without touching the test logic.
- Tracy is off by default and **reload does not survive with it on** — see below.

## Tracy

The baseline deliberately builds with Tracy disabled: it keeps its own globals and thread
registrations inside the reloading module. To re-test with it:

```bash
cmake -S . -B <build> -DZEST_TFX_RELOAD_ENABLE_TRACY=ON
```

Result: **it does not work.** With `tfxTRACY` defined, the harness hangs inside the very
first `tfx_SuspendTimelineFX` — before any `FreeLibrary` — so the reload path is
unreachable, not merely unreliable. Everything else about the run is normal up to that
point (300 warmup frames, 118 particles, 22 workers, all preconditions pass). Profile with
Tracy or reload, not both, until the suspend/Tracy interaction is fixed.
