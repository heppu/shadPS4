# Runtime shadow-map dimension selection

Original OpenOrbis / GCN homebrew with no commercial assets or shaders.
One sampling instruction reads a runtime table mixing 2D and cube depth maps;
a second variant mixes 2D arrays and cubes with the array instruction flag.
Coordinates share a cube-face calculation even when the selected descriptor
is planar. The descriptor type selects planar UVs or GCN cube UVs. LESS
comparisons use reference .5 against .25/.75 depth regions. Depth alternates
across the texture, cube faces and array layers, so coordinate conversion and
layer selection affect the result. The table is also rewritten in place.

Eight direct-descriptor controls and four runtime dispatches check exact
results and output guards. The emulator baseline passes all eight controls
but reports 128 mismatches in the two mixed 2D/cube dispatches. With the fix, all 12 cases pass with zero Vulkan core or synchronization
validation errors. No physical-console result is claimed.

```sh
make -C tests/runtime_shadow_dimensions
python3 scripts/run-test.py runtime_shadow_dimensions --shadps4 PATH \
  --set GPU.readback_linear_images_enabled=false \
  --set Vulkan.vkvalidation_enabled=true \
  --set Vulkan.vkvalidation_core_enabled=true \
  --set Vulkan.vkvalidation_sync_enabled=true
```

Run outside a directory containing a portable `user/` configuration, so the
runner's isolated settings are used.
