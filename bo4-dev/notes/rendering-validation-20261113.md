# BO4 Zombies rendering validation — 2026-11-13

Status: FAIL. Voyage of Despair reaches Forecastle gameplay, but the ship has missing/transparent surfaces, dark polygon patches and sometimes overbright panels. The earlier material-buffer fix restores visible wood/blood, weapon and zombie textures; it does not establish artifact-free rendering. No all-map or full-match pass is claimed.

## Scope and evidence

Headless Vulkan runs on the existing local AMD setup, including warm and cold pipeline caches, Vulkan core validation and runs without validation. The same world corruption appears with and without RenderDoc. Images and complete logs are kept locally under `/mnt/games/PS4/bo4-dev/out/validation-20261113/`.

- `baseline-gameplay2.png`: cold-cache baseline, broken ship surfaces.
- `bindings-fixed-gameplay2.png`: corruption persists after descriptor/cache/upload corrections.
- `capture-gameplay2.png`: corruption with Vulkan validation disabled.
- `aux-fixed-gameplay2.png`: detailed zombie skin/clothing alongside corrupted world surfaces.
- `interface-final-gameplay2.png`: final build, textured deck and weapon with broken ship surfaces.
- `interface-final-side.png`: final build, textured zombie clothing and overbright/missing ship surfaces. A copy is checked in as `rendering-20261113.png`.

## Renderer corrections

- `62a9cad9`: initialize null-image descriptor type on every binding. Previously a stale storage/texture type triggered `VUID-VkWriteDescriptorSet-descriptorType-00319`.
- `24ac7f8a`: reject cached pipeline stages with incompatible starting bindings; clear stage state before preload; count mip-array/table bindings correctly. Previously mixed cache generations triggered `VUID-VkGraphicsPipelineCreateInfo-layout-07988`.
- `eab837e5`: align image staging uploads to at least 16 bytes. Previously RGBA32F uploads triggered `VUID-vkCmdCopyBufferToImage-dstImage-07975`.
- `b9d4cd5`: omit unused fragment and auxiliary tessellation inputs, including AMD per-vertex interpolation setup; retain input usage in serialized shader metadata; invalidate older shader/pipeline caches. Eliminates the observed `VUID-RuntimeSpirv-OpEntryPoint-08743` reports through Forecastle gameplay.

These are host Vulkan/compiler correctness changes with no game/shader special cases. They do not change a guest instruction's intended semantics. No new emulation behavior is inferred from these checks; physical PS4 validation of the earlier runtime-buffer-table reproducer remains outstanding.

## Checks and limits

The baseline emitted descriptor type, pipeline layout, image alignment and stage-interface errors. After the first three fixes, the first three categories disappeared through gameplay (`bindings-upload-fixed.log`); interface errors remained.

SPIR-V validation passed for all 826 modules dumped during the initial gameplay run (`shader-validation-gameplay.json`) and all 807 fresh modules selected from the final build run (`shader-validation-final.json`), using `spirv-val --target-env vulkan1.3`. This checks module validity, not visual equivalence or complete shader coverage.

The final build passed compilation and `git diff --check`. Its gameplay run (`out/runs/1113-100506`, preserved as `interface-final-gameplay.log`) contains zero reports for the four corrected VUID categories. Remaining counts are 10 buffer-size, 3 flush-range and 14 dual-source blending reports; duplicate suppression means these are log counts, not total events. A restart with the populated cache reached the Zombies lobby without any of those four VUID categories (`interface-final-warm.log`); the three remaining categories still occur. The temporary capture instrumentation is absent from the final source. Original runtime settings and the original capture filename were restored after stopping the emulator.

Known remaining Vulkan errors include sparse buffers larger than the reported maxBufferSize (`06409`), mapped-memory flush ranges (`01389`), and dual-source blend factors on multiple color attachments (`09239`). Their causal relationship to the ship corruption is not established.

Two full-scene RenderDoc capture attempts exceeded the existing GPU/host memory guard thresholds; their partial captures are not usable evidence. A later capture setup exposed a null input in the preliminary shader-interface patch, fixed by applying the same input-usage filter to AMD interpolation setup. Temporary capture options were removed. The original capture is preserved.

## Remaining work

Investigate a corrupt world draw with a complete GPU capture, then implement a general fix with appropriate hardware evidence if guest semantics change. Repeat representative scene/material/effect and warm-cache checks after that fix. Full matches and other Zombies maps remain unvalidated.
