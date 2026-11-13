# Runtime buffer descriptor tables

Verified on 2026-11-13. BO4 material initialization uses compute shader
`0x5544b5a7` to copy static material blocks through runtime-selected source and
destination V# descriptors. The previous compiler reduced both accesses to a
null buffer at offset zero. The later `0x7e653ee4` sparse patch pass worked, leaving
texture indices present while static material constants were missing.

The fix recognizes table-fetched raw 32-bit integer/float scalar/vector accesses,
preserves runtime index/stride/addressing, and resolves bounded Vulkan device
addresses with normal buffer-cache synchronization and write tracking. It has no
game or shader hash special cases. Formatted, atomic and 8/16/64-bit table accesses
remain outside this change.

Validation: clean build, SPIR-V validation of the game and synthetic shaders,
and PASS from the OpenOrbis reproducer in `../hardware-tests/tests/runtime_buffer_table`.
It checks three pairs, varying strides, range checks, guards and an empty source.
Physical PS4 execution remains outstanding. The emulator traps during guest
`exit(0)` after reporting PASS; use the guest marker as the test result.

Forecastle gameplay on the clean build shows wood/blood deck texture, ship-panel
detail and colored zombie skin/clothing. Extended validation later reproduced
missing/transparent ship surfaces, dark patches and overbright panels. Overall
rendering validation fails; see [the follow-up report](rendering-validation-20261113.md).
Local evidence for this earlier run is under
`/mnt/games/PS4/bo4-dev/out/buffer-table-fix/`; the final run is
`/mnt/games/PS4/bo4-dev/out/runs/1113-085406`.

Earlier active-list probes found no changes between submission and dispatch
through 11,776 dispatches. Older late-write counters compared normally reused
lists, and older shadow probes compared different generations. The timing theory
was unsupported. Temporary probes are archived locally and removed from the fix.

For headless menu recovery, 60 ms confirm presses can be dropped. Use 200–250 ms,
check the screenshot after entering Zombies, then move right to Play and confirm.
Holding confirm for three seconds skips the intro after level loading begins.
The emulator process name for `pkill -x` is `shadPS4:Main`.
