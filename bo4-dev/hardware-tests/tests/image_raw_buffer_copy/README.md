# GPU image writes followed by raw buffer reads

Original OpenOrbis / GCN homebrew, with no commercial-game shaders or assets.
It writes a 128×128 R32_UINT image through image instructions, then reads the
same allocation through ordinary buffer instructions. Both Thin2DThin and
DisplayLinearAligned layouts are covered with automatic linear-image readback
disabled so CPU backing memory cannot hide a missing GPU synchronization.

Each layout checks:

- A 64 KiB raw copy of CPU-initialized data, followed by image reads.
- Two successive GPU image writes, raw copies, and image reads of 64 lane values.
- Uniform GPU image fills followed by 64-byte raw reads at offsets 0, 4 and 4096.
- Untouched output guards after every read.

```sh
make -C tests/image_raw_buffer_copy
python3 scripts/run-test.py image_raw_buffer_copy --shadps4 PATH \
  --set GPU.readback_linear_images_enabled=false \
  --set Vulkan.vkvalidation_enabled=true \
  --set Vulkan.vkvalidation_core_enabled=true \
  --set Vulkan.vkvalidation_sync_enabled=true
```

Baseline 17345b92 passes the CPU control and fails the first GPU-written value:
expected `10213000`, observed stale `12345678`. The original ELF can be run with
an OpenOrbis homebrew loader. No physical-console result is claimed.

With raw-buffer image synchronization, every case passes on the AMD Vulkan host
with zero core/synchronization validation reports. The six neighboring probes
(subrect format aliases, stencil storage, sampled depth attachment, runtime image
updates/identity, and dynamic scalar reads) also pass without validation reports.
