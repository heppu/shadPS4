# Black Ops 4 dev loop for shadPS4

Scripts used to build, run and debug shadPS4 with Call of Duty: Black Ops 4 (CUSA11100) on a host that cannot build the emulator natively. Everything runs in a docker image with a headless sway session, so nothing opens on the desktop. Tooling and emulator fixes live together on the `bo4-dev` branch.

Paths inside the scripts point at the machine they were written on (`/mnt/games/PS4/bo4-dev`, the fork checkout under `~/code`). Adjust them before use.

## Layout

- `bo4-dev/Dockerfile`, `sd`, `build`: build image, run a command in the container, incremental build.
- `bo4-dev/headless`, `sway.conf`, `run`, `play`: headless run with a time limit, and a windowed run with audio and gamepads.
- `bo4-dev/guard`: runs a command inside the container and monitors GPU memory/free RAM throughout its lifetime. It stops only its owned command group when a limit is crossed. Headless and windowed launches use it; `VRAM_MAX`, `GTT_MAX`, and `RAM_MIN` are forwarded into the container. It does not rely on a host background process or PID file.
- `bo4-dev/shot`, `key`, `step`, `zmplay.sh`, `zmslow.sh`: screenshots, key injection, scripted route into a Zombies match.
- `bo4-dev/hang`, `bprun`, `peekrun`, `gdb*`, `bp.py`, `peek.py`, `guest.py`, `watch.py`: gdb helpers for guest registers, host and guest breakpoints, memory dumps and runtime byte patches.
- `bo4-dev/zmcap.sh`, `rdrun`, `rd/*.py`, `patches/rdoc-trigger.diff`: RenderDoc frame capture of a match on X11 (the capture layer refuses Wayland) and headless replay scripts that list passes, bound textures, descriptor accesses and resource usage. The diff adds a timed capture trigger (`RDOC_AT` seconds) to the emulator and is applied only for capture builds. Replay runs without network and with RenderDoc analytics off.
- `bo4-dev/peekgrid.py`: example `PEEKPY` script for `peekrun`.
- `bo4-dev/xr.py`, `xref.py`, `dis`: cross references and disassembly over a dumped eboot image.
- `bo4-dev/patches`: emulator patch file that opens the offline mode cards when the downloaded playlist data is missing. Copy it to `user/patches/local` in the emulator user folder.

Game data, the emulator user folder, build output and logs are not part of this branch.

## Shared-GPU checks

Keep other applications running when requested. Use fresh BO4 processes and
lightweight `shot` captures before attempting full RenderDoc captures.
`VRAM_MB` sets RADV's advertised heap size, not a resident-memory limit. Check
`GPU memory budget is ... MiB` in the emulator log: an override close to memory
already used by other applications can leave almost no usable budget and force
heavy texture-cache pressure. In the November 13 shared-RPCS3 run, overrides of
3/4/6 GiB reported usable budgets of 0/487/2278 MiB respectively.

The watchdog independently checks combined GPU VRAM/GTT and available host RAM.
Its defaults are `VRAM_MAX=12000`, `GTT_MAX=4000`, and `RAM_MIN=10000`, in MiB;
a crossed limit stops BO4, not the other GPU applications. Preserve the run log,
watchdog samples and screenshots before restarting. A visually correct sampled
frame does not establish full-match stability or clear disabled Vulkan checks.

## Performance launch

Run `./play` from the live directory for the windowed performance preset.
`./play-fast` is also available directly.
It temporarily selects the GPU's `high` power policy and, on CPUs with unequal
L3 caches, places the rendering thread on a physical core in the largest cache
cluster. The helper restores the previous GPU policy when its game process exits.
Resolution, shaders, audio, gamepad support and memory guards remain configured
by the existing launcher. `BO4_GPU_MODE=off` disables the power-policy adjustment;
`BO4_RENDER_CPUS=off` disables thread placement.

`./play-fast --headless 1200` runs the same preset with a time limit. Its input
backend defaults to X11 for the diagnostic route scripts; `BO4_HEADLESS_DRIVER`
can override that choice independently of the desktop SDL environment.

`./pgo-build generate|use` supports compiler experiments with the offline
container dependencies. Generation builds a separate instrumented executable;
use requires a merged `out/performance/pgo.profdata`. A successful build or probe
suite does not establish a native gameplay speedup. See the rendering-validation
notes for measured runs, rejected candidates and remaining work.

## Runtime buffer-table reproducer

`bo4-dev/hardware-tests/tests/runtime_buffer_table` contains an OpenOrbis
reproducer with original GFX7 assembly. It checks runtime-selected buffer pairs,
different strides, out-of-range accesses, an empty source, and guard regions.
See its README for build and run instructions. It passes on the modified
emulator; physical PS4 validation remains outstanding.

## Branch history and saved progress

The first project commit above upstream contains this tooling directory and the
project workflow instructions. Subsequent commits are individual emulator fixes.
Use `bo4-dev` for ongoing work and commit and push progress to `heppu/shadPS4`
before ending a session. Previous split-branch tips are retained as archive tags.

The live runtime directory remains `/mnt/games/PS4/bo4-dev`; keep its scripts in
sync with this directory when changing them. Its user data, downloads and captures
are runtime inputs/outputs, not repository source.

## Temporary performance launch

`./play-fast` wraps the windowed launcher with temporary AMD GPU `high` policy
and places only the render thread on the largest-cache CPU cluster when the
host has unequal L3 cache sizes. On this 9950X3D it chooses CPU 7. Placement waits
60 seconds for startup workers. The policy helper watches the owned container's
host PID and start time, survives that container's shutdown, and restores the
original policy after its GPU context closes. This can increase GPU power usage
while the game runs. It leaves other applications running.

Override with `BO4_GPU_MODE=off` and/or `BO4_RENDER_CPUS=off`; an explicit CPU list
is also accepted. `./play-fast --headless 1200` uses X11 for the existing menu
automation. The standalone `run` retains Wayland by default and now accepts an
explicit `SDL_VIDEODRIVER`. Policy restoration and native launch/CPU placement
have been checked; sustained 30 FPS is not yet established.

## Profile-guided build experiment

`./pgo-build generate` builds a separate native-CPU instrumented executable in
`build-pgo-gen`, reusing the normal build's downloaded dependencies offline.
The development container must already contain LLVM and a normal build.
`BO4_BUILD_JOBS` controls parallelism (default eight). The training-only
`pgo-checkpoint.cpp` waits for `out/performance/pgo-dump.request`, writes a full
LLVM profile under `out/performance/pgo-checkpoints`, then records its return
code and path in `pgo-dump.status`. Only a zero result and a successful
`llvm-profdata merge` establish a usable checkpoint.

The initial LLVM 23.1.4 continuous-profile experiment produced truncated data
when indirect calls were present. A small reproducer confirmed the issue; the
explicit runtime checkpoint preserves readable indirect-call profiles. Invalid
continuous profiles are kept outside the checkpoint directory and must not be
merged. The training code is excluded from the optimized build.

After training with representative gameplay, merge the valid checkpoint into
`out/performance/pgo.profdata` using `llvm-profdata` in `sd`. `./pgo-build use`
then rebuilds the normal `build/shadps4` using that profile and native CPU
instructions. This is machine-specific. It still needs rendering probes and
native FPS validation before adoption; the experiment is currently unfinished.
