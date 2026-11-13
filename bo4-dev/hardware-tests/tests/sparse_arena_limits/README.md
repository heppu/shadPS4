# Sparse arena boundary and reuse reproducer

An OpenOrbis homebrew using original GFX7 buffer-copy assembly. It maps small
regions across 2 GiB and 4 GiB virtual-address boundaries, then issues narrow
writes on both sides, a strided write spanning the whole region, further narrow
writes, and a tightly packed write crossing the boundary. GPU reads and CPU
checks verify every affected word and untouched guard. Each mapping is released
and recreated for a second phase.

Expected output:

```text
SHADTEST name=sparse_arena_limits status=PASS boundaries=2 phases=2 migration=1 reuse=1 guards=1
```

This checks that limiting host sparse-buffer sizes preserves data when a request
crosses the cache's preferred arena boundaries. Releasing memory also exercises
retired arena aliases. On 2026-11-13 it passes with Vulkan validation enabled and
no VUID reports after the limit-aware arena and complete-alias-unbind changes.
The runtime-buffer-table and mip-subrectangle reproducers also pass with the
arena changes. **No physical PS4 execution is claimed.**

Build with the dependencies pinned in `../../deps.lock`:

```sh
make -C tests/sparse_arena_limits \
  TOOLCHAIN=/path/to/OpenOrbis/PS4Toolchain \
  OPENGNM=/path/to/built/opengnm
```

Requires LLVM `clang`, `ld.lld`, `llvm-mc`, `llvm-objcopy`, and Python 3. Load
`out/sparse_arena_limits/sparse_arena_limits.elf` with a PS4 homebrew loader and
inspect stdout/kernel debug output. Enable GPU readbacks under shadPS4 and use
the SHADTEST marker as the result; guest shutdown can trap after that marker.

The common guest helpers are from
[shadps4-open-test](https://github.com/kaaburgh/shadps4-open-test), under its MIT
license. No commercial-game code or assets are included.
