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

## Synchronization experiments

`sync-timing-baseline.patch` and `sync-timing.h` are temporary Linux diagnostics
for the pre-experiment source. Check patch applicability before use and preserve
local changes. They time scheduler finishes/submissions, semaphore waits, image
drains and buffer downloads. Counters overlap; do not add their totals. The patch
is never part of the playable core build. `sync-readback-callers.gdb` records 64
breakpoint hits, including inlined lambda duplicates; they are not 64 independent
read requests. Debugger-attached runs cannot supply native FPS evidence.

`batched-readback.patch` is a rejected experiment, not a recommended patch. The
same running route measured 30.72 FPS with batching and 30.70 without it, with
essentially equal CPU time per frame. All 24 probes passed, but no gameplay gain
was established, so the core change was removed.

`compare-poll-probe.sh` compares the saved control executable with the current
build on the original 512-round WAIT_REG_MEM workload. Three alternating pairs
measured about 597 ms versus 547 ms; these are workload times, not gameplay FPS.

`tap-keys.py` uses short presses to avoid menu autorepeat. `start-ready-match.py`
requires Play to already be highlighted. Custom-health benchmark runs use +3
health bars, Short regeneration delay and Short regeneration speed. Enemy rules
remain default. Keep these results separate from default gameplay and restore
the backed-up saved settings after testing.

`image-write-alias-main.c` preserves a separate original diagnostic that fails
on the unchanged control: a WRITE_DATA through a second physical-memory alias
leaves the sampled image word unchanged. Expected 0x10293847, observed 0xabcdef11.
It is not included in the passing normal-address image-write oracle. Selective
image downloads keep the original full-drain behavior for aliased or uncertain
write destinations; that does not claim to fix the separate alias invalidation.

`configure-health-benchmark.py` is an experimental menu helper, not a reliable
state machine. Its input sequence missed transitions in native runs; the saved
`*-verified.png` screenshots, taken after manual corrections, establish the
settings used. Never start timing merely because this helper prints success.
`eop_image_ordering` adds an EOP / WAIT_REG_MEM / EOP chain to image workloads;
it checks progress and contents across repeated command-buffer reuse.

`deferred-fence.patch` is an unretained experiment against the two synchronization
fixes. It passed 26 probes, but its 22.02-FPS native sample contains downed
play and establishes no gain. Keep it out of the normal playable build.
`synchronization-comparison.json` records measured values and exclusions.
Run frame-counter tools as root **inside the owned benchmark container**;
otherwise `/proc/<pid>/mem` may reject access. Validate that access before
launching the match. Do not attach a debugger during native timing.
