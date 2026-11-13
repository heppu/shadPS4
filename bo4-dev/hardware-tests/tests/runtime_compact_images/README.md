# Runtime compact image descriptors

Original OpenOrbis / GCN homebrew without commercial shaders or assets.
Four direct R128 descriptor controls sample .25/.75 float textures. Four runtime
cases read compact descriptors from tables with 16-byte and 32-byte strides.
The padded entries contain 0xffffffff after their first four words, to catch
accidental use of a full eight-word descriptor. Both tables are rewritten in
place and all output guards are checked.

```sh
make -C tests/runtime_compact_images
python3 scripts/run-test.py runtime_compact_images --shadps4 PATH \
  --set Vulkan.vkvalidation_enabled=true \
  --set Vulkan.vkvalidation_core_enabled=true \
  --set Vulkan.vkvalidation_sync_enabled=true
```

On b81714a5 the direct controls pass and runtime samples return zero (512
mismatches). With compact-table support all eight cases pass with zero core or
synchronization validation reports. No physical-console result is claimed.
Earlier exploratory versions had an incorrect stride expression and a workgroup
register overlap; only r128-baseline-final.log is the baseline evidence.
