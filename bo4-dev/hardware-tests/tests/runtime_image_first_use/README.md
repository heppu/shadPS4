# Runtime image first use

Independent OpenOrbis homebrew with original GCN assembly. One dispatch uses a
runtime descriptor table to sample 64 independently allocated 64x64 RGBA32F
textures. Each workgroup selects its own table entry and all 64 lanes sample a
known constant. The first dispatch checks 4,096 output words and 16 guard words;
there is no frame-boundary warmup.

On baseline 57cd101e, the first 48 textures pass, but the last 16 return zero
(1,024 mismatches). Removing the per-epoch 48-image resolution limit makes every
output and guard pass with core and synchronization validation and zero reports.
Runtime image update and independent-allocation identity regressions also pass.
This is emulator validation; physical PS4 execution is unavailable.

```sh
make -C tests/runtime_image_first_use
python3 scripts/run-test.py runtime_image_first_use --shadps4 /path/to/shadps4 --timeout 40
```

The ELF is `out/runtime_image_first_use/runtime_image_first_use.elf`. No video
flip is needed. GPU completion is awaited before CPU reads the results.
