# Read-first-lane execution masks

Original OpenOrbis/OpenGNM compute workload, with no extracted shaders or game data.
Ten direct cases select the first lane in explicit EXEC masks: empty/full waves,
low bits, bits 31/32/63, odd lanes, the upper half, and both endpoints. Empty EXEC
reads lane zero. All 64 lanes check the scalar result after EXEC is restored.

Forty bounded loop cases group lanes by one, two, four or 64 keys. Each iteration
reads the first remaining lane, processes matching keys, removes those lanes
from EXEC, and continues while the scalar 64-bit mask is nonzero. Every output,
iteration count, final remaining mask and guard word is checked. A hard limit of
65 iterations converts a stalled emulated loop into a reported failure instead
of a GPU hang. The guest arena is 32 MiB.

The first-lane and zero-mask behavior is specified in AMD's
[Southern Islands ISA reference](https://www.amd.com/content/dam/amd/en/documents/radeon-tech-docs/instruction-set-architectures/southern-islands-instruction-set-architecture.pdf).
No physical-console result is claimed.

```sh
bash scripts/build-test.sh readfirstlane_exec
python3 scripts/run-test.py readfirstlane_exec --shadps4 /path/to/shadps4 \
  --set GPU.readback_linear_images_enabled=false \
  --set Vulkan.vkvalidation_enabled=true \
  --set Vulkan.vkvalidation_core_enabled=true \
  --set Vulkan.vkvalidation_sync_enabled=true
```

Run the host runner outside a directory containing a portable `user/` folder.
