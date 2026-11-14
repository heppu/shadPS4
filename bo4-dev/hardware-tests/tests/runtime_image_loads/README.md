# Runtime typed image loads

Original OpenOrbis / GCN homebrew with no commercial shaders or assets.
Nine direct controls read R32_FLOAT, R32_UINT, and R32_SINT textures in 2D,
3D, and 2D-array views. Nine runtime cases read mixed numeric formats from
an in-place rewritten descriptor table. Three further cases overwrite the
2D images on the GPU between table reads, checking current contents without
a CPU image readback. Three CPU rewrites then check cache freshness after those GPU writes.
Each dispatch checks output guards and exact words.

```sh
make -C tests/runtime_image_loads
python3 scripts/run-test.py runtime_image_loads --shadps4 PATH \
  --set GPU.readback_linear_images_enabled=false \
  --set Vulkan.vkvalidation_enabled=true \
  --set Vulkan.vkvalidation_core_enabled=true \
  --set Vulkan.vkvalidation_sync_enabled=true
```

The initial corrected 2D baseline on c74e1bf3 passes the three direct readbacks
but returns zero for all runtime cases (384 mismatches). Switching the direct
shader from UINT to SINT also reports VUID-vkCmdDispatch-format-07753, because
its specialization did not distinguish signedness. An earlier exploratory
probe left descriptor bits uninitialized and is not baseline evidence.

With typed image tables and signed specializations, all 24 expanded cases pass
with no core/synchronization validation errors. The 14 emitted SPIR-V modules
also pass spirv-val. Local evidence: load-baseline-valid.log, load-fixed2.log,
and load-spirv-validation.log under out/ship-fix. No physical-console result is
claimed.

The cached-view follow-up retains per-draw updates and barriers and keeps depth,
stencil aliases, and cropped views on the full resolution path. The three CPU
rewrite cases cover the newly cached path.

The incremental table-refresh follow-up adds four reads from an unchanged two-image
table in one submission. Both images begin as CPU-uploaded textures; the last two
reads follow GPU writes to the first image while the second remains unchanged.
All 512 output words and surrounding guards are checked, including the transition
from an immutable texture into a GPU-written image.

The layout-refresh follow-up reads two immutable full-size images, then a table
with a cropped view of only the first image, and finally the original full-size
table twice. The cropped view requires a transfer from the cached full image;
subsequent loads must still return both original values. All 512 words and guards
are checked with synchronization validation enabled.
