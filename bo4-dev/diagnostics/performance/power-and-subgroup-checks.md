# Power and subgroup diagnostics

sample-live.py records GPU power policy every second. Compare runs only when the
policy and gameplay state match. Inspect the endpoint and movement captures:
downed, dead and menu samples are not normal gameplay benchmarks.

The temporary gpu-profile-test.py watchdog accepts high or profile_peak and
restores the original policy after 600 seconds or its mode-specific stop file.
Run it in a read-only, network-disabled shad-dev container with all capabilities
dropped, mounting only the GPU performance-level sysfs file as
/gpu-performance-level and this project's out directory as /out. Precreate the
mode-specific JSON status file writable by the container before launch. Remove
only that mode's old stop file before reuse.

Stop the benchmark application before requesting restoration, then verify sysfs
again after its GPU context is closed. An earlier profile_peak run reported auto
immediately on restore but peak reappeared after later application shutdown.
A second profile_exit/auto write after all benchmark containers closed restored
auto through subsequent probe and native runs. The immediate JSON restored flag
alone does not establish that the policy remains restored.

GPU busy reports 100% in profile_peak and should not be used to estimate GPU time.
No voltage, overclock or persistent system configuration changes are needed.

The vertex wave32 experiment was rejected. The retained implementation keeps
vertex stages at 64 lanes. The vertex-varying GDB scripts require the saved
experimental shadps4-vertex-varying binary and its matching source. With that
experimental build and RADV_PERFTEST=gewave32, the negative script disables the
guard and causes 128 mismatched words in the lane-sensitive point case. These
scripts do not establish a negative control on the retained implementation.

sample-steady-combat.sh waits for the previous movement sequence to finish, but
cannot prevent the player dying. Its first measured run ended at the after-action
menu and must not be reported as gameplay performance.


sample-live.py now captures each ten-second endpoint as well as the final screen.
BENCH_RENDER_CPUSET restricts only the render thread and records its original,
requested and actual affinity. It does not pin the entire container. The trial
with 8-15 ended downed and does not establish a gameplay performance improvement.
The experimental circling route got stuck against the central podium and is not
a reliable benchmark route. Use the earlier movement sequence and inspect all
sample captures before accepting a measurement.

readback-queue.gdb samples actual buffer waits; verify its source line against the
matching binary before use. A 48-wait diagnostic found no queued command in 41
cases, limiting the opportunity for simple adjacent-request batching.
image-lifetimes-debug.gdb verifies the original mip-expansion probe changes image
generation while reusing the unaffected table. It records evidence only; it
does not alter program behavior.
