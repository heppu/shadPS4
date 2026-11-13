# Sparse timeline presentation diagnostic

Original native Vulkan/Xlib reproduction of WRITE_AFTER_PRESENT reports after
a timeline signal in `vkQueueBindSparse`. No game files or shader binaries are
needed. Requires a Vulkan 1.4 graphics/present queue that supports sparse binding,
an X11 display, Vulkan headers/loader, Xlib and Khronos validation.

```sh
c++ -std=c++20 -O2 main.cpp -o sparse-present -lvulkan -lX11
./sparse-present counter sparse-timeline
./sparse-present counter sparse-binary
./sparse-present counter submit-timeline
./sparse-present counter missing-acquire
```

The first argument controls host timeline queries: `normal`, `counter`, or
`wait`. The second selects the synchronization case. `sparse-timeline` is the
default. Each positive case presents 40 frames; the deliberately invalid
`missing-acquire` control stops after six. The program prints diagnostic counts
and returns success when Vulkan calls complete, including expected diagnostic
cases; inspect `RESULT`, not just its exit status.

| Case | Signal operation | Expected synchronization | Observed reports |
|---|---|---|---|
| sparse-timeline | Empty bind-sparse, timeline signal | Valid | 10 WRITE_AFTER_PRESENT logs (duplicate limit) |
| sparse-binary | Empty bind-sparse, binary signal | Valid | 0 |
| submit-timeline | Empty ordinary submit, timeline signal | Valid | 0 |
| missing-acquire | Ordinary timeline submit, omit acquire wait | Deliberately invalid | 16 core/sync reports |

Results above use upstream Vulkan-ValidationLayers commit
`37ec2baf9aac8c0f55fe2b9beec1d818b70d7841` (API 1.4.368), RADV 26.2.6,
RX 9070 XT, Xwayland. Duplicate limits and timing can change report counts.
The native case contains zero sparse resources: the empty bind-sparse submission
only signals a semaphore. Frame acquisition, barriers, rendering, present waits
and host waits are otherwise identical between positive cases.

The validator has no sync-validation hook for `vkQueueBindSparse`.
`QueueBatchContext::ResolveSubmitWaits` therefore treats its timeline signal as
an unresolved future signal. `QueueSubmitCommand::Execute` then defers subsequent
submissions, but presentation validation continues against stale accesses.
Traced shadPS4 `presentation_compute` shows precisely this path: the arena-memory
semaphore at value 1 becomes unresolved, followed by 242 deferred submissions
and 118 detected presentation hazards over the two-second observation.

This isolates a validation-layer tracking defect, not a missing acquire wait in
shadPS4. It does not prove unrelated GPU synchronization correct. The emulator
retains its timeline semaphore and validation reports; no warning suppression or
rendering workaround is included. The validator's documented lack of sparse
resource support also limits synchronization coverage.

Primary references:
- [Validation implementation at the inspected revision](https://github.com/KhronosGroup/Vulkan-ValidationLayers/blob/37ec2baf9aac8c0f55fe2b9beec1d818b70d7841/layers/sync/sync_submit.cpp)
- [Sync validation limitations](https://github.com/KhronosGroup/Vulkan-ValidationLayers/blob/37ec2baf9aac8c0f55fe2b9beec1d818b70d7841/docs/syncval_usage.md)
- [Vulkan sparse resource binding](https://docs.vulkan.org/spec/latest/chapters/sparsemem.html#sparsememory-resource-binding)

Local evidence: `out/ship-fix/sparse-present-matrix.log` and
`present-compute-unresolved-trace.log`. Trace instrumentation was confined to an
isolated validator build; it only logs state and is not needed to reproduce.
