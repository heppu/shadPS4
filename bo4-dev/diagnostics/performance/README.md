# BO4 performance measurements

These scripts run in the existing `bo4-dev` Docker/headless harness. Install them
with `sh diagnostics/performance/install.sh`. The live harness is currently
`/mnt/games/PS4/bo4-dev`; the source mount is `/src`. Build the optional CPU profiler
image with `docker build -t shad-perf -f diagnostics/performance/Dockerfile.perf .`.
The image extends the existing shad-dev image and installs Arch's perf package.

From the live harness, run `sh out/performance/start-benchmark.sh LABEL`.
It uses the currently selected Zombies map; select IX in the game first. Default
input is stationary firing and reloading. An optional second argument selects a
sequence script, for example `/out/performance/sequence-arena-enter.py`, which
enters the arena immediately after detecting the HUD. These are bounded developer
workloads, not proof of sustained playable combat. Characters, enemy positions,
loading and downed states vary. Inspect screenshots when interpreting averages.

The frame-counter offset is obtained from the executable offline using GDB.
The running counter is read through `/proc/PID/mem`; no debugger attaches during
timing. The CPU cycle sampler runs concurrently. A complete 60-second result has
`complete: true` in LABEL-frames.json. Partial results are saved every ten seconds.
The final screenshot is LABEL-sample-end.png. Never attach a debugger or stop the
container before completion when collecting a usable timing result.

The guard remains 12,000 MiB aggregate VRAM, 6,000 MiB GTT and 10,000 MiB minimum
available system RAM. The default advertised heap is 6,144 MiB; the latest
comparisons use `BENCH_VRAM_MB=10240`. This changes the advertised heap without
raising the memory guards. Each game container has
a 28 GiB memory limit and no network. Do not run capture replay or GPU regression
probes concurrently. These scripts only manage their own named containers.

Decode CPU data while the game container still exists, using shad-perf with
`--pid=container:shad-run`, the same source/output mounts, and
`sh /out/performance/report-profile.sh LABEL`. Otherwise symbols from the running
container can become unavailable. Preserve the exact executable for each result.

The route recognizes title cards and the lobby from screen pixels. If navigation
fails, inspect its screenshots and log rather than accepting menu FPS as gameplay.
Raw captures, game files, executable binaries and generated logs are excluded
from Git. The emulator changes remain generic; only these input scripts know the
game's menu layout and controls.

For a fresh moving-arena sample, pass
`/out/performance/sequence-moving-on-gameplay.py` as the sequence argument. It
waits for the HUD, enters the arena and alternates strafing, shooting and reloads.
It can exhaust ammunition; screenshots and the sampled scene remain essential.
`moving-after-sample.sh LABEL` instead adds an 80-second moving sample after an
opening sample. Run it as root inside the game container; it delegates X11 input
to the display user `dev`. Wait for both samples before stopping the run.

`readback-callers.gdb` captures 128 readback requests with thread names, arguments
and call stacks. Attach only after native timing completes, while still in the
match. Its fixed output name is `readback-callers.json`; archive it per experiment.
Do not mix debugger-stalled timing with the native frame-counter measurements.

`BENCH_CPUSET` optionally restricts only the benchmark container's CPUs. Default
is unrestricted; forcing cache-core affinity has not demonstrated a benefit.
