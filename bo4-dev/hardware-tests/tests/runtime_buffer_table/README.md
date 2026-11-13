# Runtime buffer descriptor table reproducer

An OpenOrbis homebrew that selects a source/destination pair from a buffer
descriptor table using the compute workgroup ID. Its original GFX7 assembly
is in `assets/copy.s`; it contains no commercial-game code or assets.

Three workgroups exercise different source and destination strides, an empty
source, reads beyond the source range, and writes beyond the destination range.
The CPU checks every source/destination word, including untouched padding and
guards. Buffers occupy separate 64 KiB cache pages to isolate descriptor behavior
from shared-page CPU/GPU coherence.

Expected result:

```text
SHADTEST name=runtime_buffer_table status=PASS groups=3 runtime_stride=1 bounds=1 guards=1 empty_source=1
```

Built with OpenOrbis v0.5.4, LLVM's GFX7 assembler and OpenGNM commit
`fbb16e2e948536ec77806d7f46e140b806cff5af`. The common guest helpers come from
[shadps4-open-test](https://github.com/kaaburgh/shadps4-open-test), under its MIT license.
The reproducer passed on the modified shadPS4 `bo4-tables` worktree
on 2026-11-13. **It has not been run on a physical PS4.**

To build in that harness checkout:

```sh
make -C tests/runtime_buffer_table \
  TOOLCHAIN=/path/to/OpenOrbis/PS4Toolchain \
  OPENGNM=/path/to/built/opengnm
```

The shader uses `llvm-mc` and `llvm-objcopy`; no GLSL or proprietary shader
compiler is required. Output is `out/runtime_buffer_table/runtime_buffer_table.elf`.
Run the ELF with a PS4 homebrew loader and inspect stdout/kernel debug output
for the marker above. Enable GPU readbacks when running under shadPS4.

The test emits an end-of-pipe 64-bit label with interrupt selection zero.
OpenGNM's default selection three is not supported by this emulator branch.
This branch also traps during guest `exit(0)`, after reporting PASS; the test
result is the marker, not the host process's eventual shutdown status.
