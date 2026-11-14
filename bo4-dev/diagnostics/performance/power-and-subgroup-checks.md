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

BENCH_RADV_PERFTEST=gewave32 passes the Mesa geometry-wave preference to the
benchmark container. Use only with subgroup eligibility guards and validated
hardware probes. vertex-varying-stages.gdb inspects pipeline subgroup structures.
vertex-varying-negative.gdb deliberately disables the vertex eligibility guard
in process memory: vertex_wave_size must fail its upper 32 lane columns when run
with RADV_PERFTEST=gewave32. This is a negative control, never a game setting.

sample-steady-combat.sh waits for the previous movement sequence to finish, but
cannot prevent the player dying. Its first measured run ended at the after-action
menu and must not be reported as gameplay performance.
