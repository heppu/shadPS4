# Runtime depth-comparison texture tables

Original OpenOrbis / GCN homebrew with no commercial assets or shaders.
A LESS comparison samples 64x64 R32_FLOAT textures containing .25 and .75
with reference .5. Four direct-descriptor controls cover both expected results
and alternating array layers. Four runtime-table cases select descriptors by
workgroup ID, including an in-place table swap. Three additional cases render
successive .25/.75 depths on the GPU and sample them through the same table.
All cases check output guards.

```sh
make -C tests/runtime_depth_tables
python3 scripts/run-test.py runtime_depth_tables --shadps4 PATH \
  --set GPU.readback_linear_images_enabled=false \
  --set Vulkan.vkvalidation_enabled=true \
  --set Vulkan.vkvalidation_core_enabled=true \
  --set Vulkan.vkvalidation_sync_enabled=true
```

Before depth-table support, the four direct controls pass while the four
runtime cases produce 256 mismatches in total. With the fix, all 11 cases pass.
No physical-console result is claimed. An earlier control using a constant
S_BUFFER descriptor load encountered a separate SRT-walker fault and was
replaced by direct user-data descriptors; that earlier run is not the baseline.
