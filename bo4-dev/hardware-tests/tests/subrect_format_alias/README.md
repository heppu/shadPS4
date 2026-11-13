# Cropped views with compatible format aliases

Original OpenOrbis / GCN homebrew. A compute shader writes 64 texels into a
128×128 macro-tiled UINT image. A second shader reads the same allocation as a
128×120 UNORM texture and checks the cropped dimensions and normalized values.
The cases cover both R8 and R8G8, two successive GPU writes, and an optional
read of the zero-filled cropped alias before the first write. All untouched
output guards must remain unchanged.

The first write uses red=255, green=0; the second uses red=0, green=255.
UNORM reads must yield exact 0.0/1.0 values (R8's absent green channel is zero).
The cropped view must report 128×120, without sampling padded rows.

```sh
make -C tests/subrect_format_alias
python3 scripts/run-test.py subrect_format_alias --shadps4 PATH \
  --set Vulkan.vkvalidation_enabled=true \
  --set Vulkan.vkvalidation_core_enabled=true \
  --set Vulkan.vkvalidation_sync_enabled=true
```

Baseline 12b9493b fails the first R8 result: expected 1.0, observed zero.
The original ELF is compatible with an OpenOrbis homebrew loader.
No physical-console result is claimed.

With compatible-format crop reuse and GPU-data preference, all cases pass with
zero core/synchronization validation reports. Mip cropping, stencil storage
aliases, and mixed 2D/3D runtime-image regressions also pass with zero reports.
