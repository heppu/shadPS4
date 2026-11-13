# Runtime texture dimension queries

Original OpenOrbis / GCN homebrew without commercial shaders or assets.
Four direct-descriptor controls and four runtime cases query 2D, 3D, and 2D-array
textures. The cases include two LODs, a nonzero base mip, an explicit array view,
mip counts, mixed 2D/3D descriptors, in-place table swaps, and output guards.

```sh
make -C tests/runtime_image_queries
python3 scripts/run-test.py runtime_image_queries --shadps4 PATH \
  --set GPU.readback_linear_images_enabled=false \
  --set Vulkan.vkvalidation_enabled=true \
  --set Vulkan.vkvalidation_core_enabled=true \
  --set Vulkan.vkvalidation_sync_enabled=true
```

Baseline f1e7f9ec passes the four direct controls but returns zero for dynamic
queries (1,920 mismatches). Descriptor-based lowering passes all eight cases
with no core or synchronization validation reports. No console result is claimed.

The expected view dimensions agree with the static controls and AMD's descriptor
query implementation in [Mesa](https://chromium.googlesource.com/external/gitlab.freedesktop.org/mesa/mesa/+/c393734520d9a2b5ad1dfb1f2e5d6afe4550bce8/src/amd/common/nir/ac_nir_lower_resinfo.c).
The probe sets the array range explicitly: OpenGNM initializes lastarray to
numslices, so relying on its default added an unintended layer to the first draft.
