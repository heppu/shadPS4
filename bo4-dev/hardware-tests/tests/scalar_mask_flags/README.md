# Scalar 64-bit logical flags

Original OpenOrbis/OpenGNM compute homebrew; no extracted shaders or game data.
Checks the result and SCC of AND, NAND, ANDN2, OR, NOR, ORN2, XOR, XNOR and NOT
with zero, low-bit, high-bit, alternating and all-one operands. Seven LSHL-by-zero
cases provide a control. Both S_CSELECT and S_CBRANCH_SCC0 consume each flag.
Every lane must observe SCC=1 exactly when the entire 64-bit result is nonzero.

Five additional cases create VCC from lane comparisons, apply S_AND_B64, branch
on SCC, and exchange neighboring lane values through DS_SWIZZLE_B32. Thresholds
0, 1, 32, 63 and 64 distinguish an empty wave from a partly populated mask; a
nonempty mask must keep all 64 lanes in the scalar branch. Output guards are
checked for every dispatch. Total: 75 cases, 64 lanes each, 32 MiB guest arena.

This follows AMD's [Sea Islands ISA reference, scalar logical operations](https://www.amd.com/content/dam/amd/en/documents/radeon-tech-docs/instruction-set-architectures/sea-islands-instruction-set-architecture_0.pdf).
The executable is ready for console testing, but no physical PS4 result is claimed.

```sh
bash scripts/build-test.sh scalar_mask_flags
python3 scripts/run-test.py scalar_mask_flags --shadps4 /path/to/shadps4 \
  --set GPU.readback_linear_images_enabled=false \
  --set Vulkan.vkvalidation_enabled=true \
  --set Vulkan.vkvalidation_core_enabled=true \
  --set Vulkan.vkvalidation_sync_enabled=true
```

Run the host runner from outside a directory containing a portable `user/` folder,
so its isolated configuration takes effect.
