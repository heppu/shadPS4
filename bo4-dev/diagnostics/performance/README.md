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


## Shader lookup and refreshed compiler profile (2026-11-15)

`variant-cache.patch` is a rejected last-successful shader variant shortcut,
not applied to the playable core. It retains the compatibility check, but three
corrected alternating homebrew workloads measured 35.293 ms control versus
35.317 ms candidate, establishing no gain. The original 135 ms samples used a
1 ms completion sleep and masked small savings; the probe now uses bounded
pause polling and retains the required load fence. The 21.63-FPS native control stays
alive; the 20.42-FPS candidate contains a revive and different scene. Both use
automatic GPU policy and unrestricted CPU scheduling; keep them separate from
play-fast runs with high policy and render-thread affinity. Raw data are in
`variant-comparison.json`. All 27 rendering/coherence probes pass.

`variant-lookup.gdb` is an unsuccessful inspection attempt: the optimized build
eliminates the requested local variables. Its output cannot establish variant
counts or hit rates. The focused original `shader_variant_reuse` ELF does compile
24 variants and verifies switching, repetition, output values and guard words.

`build-background.sh LABEL [JOBS]` starts an owned detached build container and
writes `LABEL-build.log` with a `BUILD_EXIT` marker. Wait for that marker before
using the executable or changing build inputs. It avoids tying a long build to
the command session lifetime. `checkpoint-on-gameplay.py LABEL` waits for the
HUD marker, collects 60 seconds, saves a screenshot, requests the existing LLVM
profile writer, and checks its return status. Use only with a GENERATE build;
training FPS is not a performance result. Preserve the old profile and executable
before replacing the canonical profile. The refreshed USE build is checked with
`pgo-refreshed-regressions.sh` (27 original probes).

`pgo-comparison.json` records the refreshed compiler profile: 22.68 FPS versus
21.63 FPS initial control, but 23.57 FPS on the control repeat. This establishes
no reliable gain. All 27 probes pass; the normal USE build is noninstrumented.

`interval-append.patch` is another rejected experiment, not applied to core.
It passes 27 probes, but six alternating bounded-spin workloads average
34.358 ms control versus 34.926 ms candidate. Its native run slows sharply,
averages 5.23 FPS, and includes downed gameplay. A later loading-screen profile
points to readbacks, sparse binds and backing release. The cause is not proven
to be the interval change. `sequence-idle-on-gameplay.py` supplies a bounded
control reproduction workload for damage/downed states. `gpu-client-load.py`
reads two seconds of per-client AMD engine counters; it never changes workloads.

## Post-link layout experiment (2026-11-15)

`Dockerfile.bolt` builds an isolated LLVM BOLT tool image; the measured version
was 23.1.5. This is an optional rejected experiment, not the default executable.
Enable `SHAD_BOLT_RELOCS=ON` in the existing PGO CMake build to retain relocations.
`start-benchmark.sh` accepts an optional third argument for a profiler script;
`perf-bolt-on-gameplay.sh` captures 20 seconds of 99-Hz hardware branch samples.
Save the exact executable before profiling it.

Run `prepare-bolt-input.sh ORIGINAL PREPARED` in the tool image, then
`optimize-bolt.sh LABEL PREPARED OUTPUT`, with paths visible in that container.
The preparer removes optional `.sframe` metadata unsupported by this BOLT build,
compares the `.text` bytes, and checks that standard `.eh_frame` unwinding remains.
It preserves the input. The optimizer uses strict profile aggregation, ext-tsp
block ordering, hfsort function ordering, and updated debug sections. It does
not enable function splitting. Do not suppress unknown-relocation errors.

The input had three unclaimed relocations in SFrame. With that optional section
removed, strict aggregation and optimization completed. The sample is sparse:
486 functions received usable profiles, two could not be optimized, and 303
relocations were not analyzed. The tool requests denser profiling. All 27 probes
pass, including existing presentation warnings. Six alternating fixed shader
workloads measured 34.351 ms control versus 34.327 ms BOLT: essentially unchanged.
Native runs measured 19.68 versus 19.95 FPS, but both contain downed states and
different scenes, with CPU time increasing from 25.87 to 27.26 ms/frame. No gain
is established, so BOLT is not retained. See `bolt-comparison.json`.

References: [LLVM BOLT guide](https://github.com/llvm/llvm-project/blob/main/bolt/docs/GettingStarted.md),
[profile documentation](https://github.com/llvm/llvm-project/blob/main/bolt/docs/profiles.md).

## Image-table entry lookup (2026-11-15)

`table-entry.patch` preserves the rejected coarse-generation prototype. A native
repeat stayed alive but averaged 20.73 FPS / 23.79 render-CPU ms, with no gain over the
22.68 FPS / 22.94 ms refreshed-profile control. A post-timing debugger sample at 16
actual function entries found only 3 of 5,522 static entries with matching cached
generations. The initial attempt stopped in inline functions and had 63 errors;
its counts are invalid. `table-entry-cache-counts.gdb` uses the exact x86-64
function entry and ABI registers, after native timing only.

The refined candidate tracks storage changes separately. New keys can rehash
the map; erasures retire references; both invalidate cached pointers. Replacing
an existing value leaves its address stable. The bundled Abseil implementation
was checked for that property. Existing image UID/version, content, alias and
layout checks remain. A separate 16-entry diagnostic found 4,941 of 4,941 static
entries with matching generations. These are validity observations at function
entry, not a measured executed-path hit rate.

The original `image_table_reuse` homebrew checks repeated 4,096-entry sampling.
Six alternating pairs measured 9423.33 us control versus 9134.50 us candidate; six
reversed-order pairs measured 9563.33 versus 9056.17 us. Combined submission/wait
time is about 4.2% lower. Initialization and output checking are excluded from
that timer; it still includes GPU/queue costs and does not imply a gameplay FPS
speedup. `compare-image-table-probe.sh` and `compare-image-table-confirm.sh`
reproduce these runs. `retained-cache-regressions.sh` includes all 28 probes.

A native candidate run also reproduced a severe slowdown to below 1 FPS, while
all memory guards remained within limits. A later profile attributed about 48%
of sampled render cycles to scheduler finishes/sparse binding and image
readbacks. This is a separate unresolved issue. Increasing the advertised heap
from 10 to 11 GiB made the unchanged control stall in its intro with only 4.4 GiB
VRAM in use; its boot profile was about 92% under queueBindSparse. Returning to
10 GiB restored normal startup. The larger heap is not retained, and no device
reset or unrelated process shutdown was performed.
