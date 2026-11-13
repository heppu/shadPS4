# Stencil storage alias roundtrip

Independent OpenOrbis homebrew with original GCN assembly. A fullscreen draw writes
0.5 depth and 0x35 stencil into a D32F/S8 target with separate guest depth and
stencil allocations. A control draw checks depth and stencil equality across the
whole target. Compute then reads and increments stencil at pixel (3,5) through an
R8_UINT texture alias, twice in separate submissions. After each update, a draw
using stencil equality and unchanged depth must write only that pixel.

The linear RGBA32F output is checked at every pixel and channel. Untouched pixels
must retain their initialized value. This verifies the initial stencil read,
shader writeback, preserved depth, unchanged neighboring stencil values, and a
second storage access after the depth target is used again.

```sh
make -C tests/stencil_storage_alias
python3 scripts/run-test.py stencil_storage_alias --shadps4 /path/to/shadps4 --timeout 40
```

The executable is `out/stencil_storage_alias/stencil_storage_alias.elf`. It needs
no external assets; retain stdout from a physical PS4 homebrew ELF loader.
Physical-console validation is pending.

Baseline emulator: the ordinary stencil control passes, but both compute phases
fail at output word 1292. Vulkan reports 00339 and 07028 because the depth/stencil
view was incorrectly bound as a storage image. The fixed emulator passes all three
cases with core and synchronization validation enabled and no Vulkan errors.
The fix also preserves the depth association when a stencil placeholder first
becomes a real R8 image; otherwise the first compute read misses the stencil data.
