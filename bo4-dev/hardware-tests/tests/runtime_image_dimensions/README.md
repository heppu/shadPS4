# Runtime image dimensions

Independent OpenOrbis homebrew using original GCN assembly. One compute sampling
instruction selects a descriptor using the workgroup ID from a mixed table:
64 lanes sample a 2D texture, and 64 lanes sample a 3D texture. The 2D result is
0.125. Alternating volume coordinates Z=0.25 and Z=0.75 must return 0.5 and 1.0.
Eight output guard words on either side must remain unchanged.

The probe uses point sampling at mip zero, 64x64 linear RGBA32F textures, and a
four-slice volume. It checks the runtime image-type decision and the Z coordinate,
including the 2D path through the same instruction. No game assets are required.

```sh
make -C tests/runtime_image_dimensions
python3 scripts/run-test.py runtime_image_dimensions --shadps4 /path/to/shadps4 --timeout 40
```

Run `out/runtime_image_dimensions/runtime_image_dimensions.elf` through an existing
PS4 homebrew ELF loader and preserve its stdout. Physical-console testing is
unavailable. On the baseline emulator, 2D and guards pass, but all 64 volume reads
return zero. The fixed emulator passes all three cases with core and
synchronization validation enabled, without Vulkan validation errors.
