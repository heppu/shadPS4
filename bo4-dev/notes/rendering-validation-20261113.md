# BO4 Zombies rendering validation — 2026-11-13

Current checkpoint (renderer f4b2ac4c, metadata/binary cache 17/30): all eight
maps have representative opening-gameplay and settled-report checks on the same
renderer build. The sampled materials remain textured and lit; missing Voyage
geometry, black report clothing and broken hair coverage no longer reproduce.
The Voyage GPU hang is resolved by EXEC-aware first-lane selection. RPCS3 stays
running throughout the checks, with fresh BO4 processes and a memory watchdog.

This is not full-match or all-shader certification. No complete round is claimed,
sampled gameplay ranges roughly 1–10 FPS, and higher-MRT SRC1 semantics remain
unresolved. Game Vulkan validation is disabled for the visual runs; separate
homebrew/validator results are recorded below. No physical PS4 is available.
The historical entries retain the evidence and failures that led here.

| Map | Gameplay evidence | Settled report |
| --- | --- | --- |
| Voyage of Despair | [Forecastle](rendering-voyage-exec-fixed-20261113.png) | [Report](rendering-voyage-report-exec-fixed-20261113.png) |
| IX | [Arena](rendering-ix-exec-fixed-20261113.png) | [Report](rendering-ix-report-exec-fixed-20261113.png) |
| Blood of the Dead | [Laboratory](rendering-blood-budget-fixed-20261113.png) | [Report](rendering-blood-report-exec-fixed-20261113.png) |
| Classified | [Conference Room](rendering-classified-exec-fixed-20261113.png) | [Report](rendering-classified-report-exec-fixed-20261113.png) |
| Dead of the Night | [North Atrium Bridge](rendering-deadnight-exec-fixed-20261113.png) | [Report](rendering-deadnight-report-exec-fixed-20261113.png) |
| Ancient Evil | [Temple of Apollo](rendering-ancient-exec-fixed-20261113.png) | [Report](rendering-ancient-report-exec-fixed-20261113.png) |
| Alpha Omega | [Security Checkpoint](rendering-alpha-exec-fixed-20261113.png) | [Report](rendering-alpha-report-exec-fixed-20261113.png) |
| Tag der Toten | [Docks](rendering-tag-exec-fixed-20261113.png) | [Report](rendering-tag-report-exec-fixed-20261113.png) |

Earlier checkpoint: FAIL. Voyage of Despair reaches Forecastle gameplay, but the ship has missing/transparent surfaces, dark polygon patches and sometimes overbright panels. The earlier material-buffer fix restores visible wood/blood, weapon and zombie textures; it does not establish artifact-free rendering. No all-map or full-match pass is claimed.

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

## Follow-up: mipmapped subrectangle fix

The complete current-frame capture `out/ship-fix/current-20261113.rdc` identified
an incorrect sampled-image extent in GPU visibility culling. The guest requested
a 128×128 depth pyramid (pitch 512) over the same allocation as a cached 256×128
image. The host returned width 256 to `image_get_resinfo`. Correcting just that
width in a diagnostic RenderDoc shader replacement changed 765 visibility count
words and restored the missing ship wall in both albedo and normal attachments.
All 4,426 subsequent indirect commands matched an independent reconstruction
from their input records, ruling out the indirect-command generator for this case.

The production fix extends the existing cropped sampled-image backing to
compatible mip chains. It checks guest mip offsets and physical row layouts,
copies the requested extent of every mip, and distinguishes cached backings by
mip count. It contains no game or shader identifiers. Storage views retain the
existing full image backing.

`rendering-mip-subrect-20261113.png` shows live Forecastle gameplay after the fix:
ship walls, windows, rigging and deck machinery are present, with textured deck,
zombie and weapon materials. This establishes the observed missing-geometry fix;
it is not yet an all-map or full-match visual pass. Combat/viewpoint coverage and
the three previously recorded Vulkan validation categories remain in progress.

The independent OpenOrbis homebrew under `hardware-tests/tests/mip_subrect`
queries four alias views and checks every requested mip's dimensions and texel,
output guards, and refreshed contents after another GPU write. The old code fails
with width 256 instead of 128; the fix passes all assertions. Vulkan validation
reports no image/copy/view errors in that reproducer, but still reports 06409.
No physical PS4 execution is claimed. Full local evidence is in
`/mnt/games/PS4/bo4-dev/out/ship-fix/`.

## Follow-up: overlay uploads and sparse arena limits

`51038fd` maps and flushes complete ImGui upload allocations. The earlier code
flushed VK_WHOLE_SIZE after mapping only the current vertex/index bytes, leaving
the mapping end unaligned to nonCoherentAtomSize. The subsequent menu and
Forecastle run contains no 01389 reports (`out/ship-fix/imgui-fixed-gameplay.log`).

Sparse arenas now use the physical device's maxBufferSize and interval mappings.
Requests crossing preferred allocation boundaries get a bounded contiguous host
buffer without forcing two full arenas into an oversized allocation. Existing
resident backing is rebound and BDA mappings refreshed; DMA synchronization and
page-table updates split at arena boundaries. Released backing is detached from
all retained arena aliases before it is freed.

The OpenOrbis `sparse_arena_limits` workload checks narrow and spanning GPU writes,
GPU readback, guards, and unmap/reuse across 2 GiB and 4 GiB virtual boundaries.
It passes with zero VUID reports. `runtime_buffer_table` and `mip_subrect` also
pass with zero VUID reports under the new allocator. The first live menu run
likewise has no 06409 or 01389 reports. Live gameplay validation continues.
No physical PS4 execution is claimed.

A later, longer gameplay run also exposed transient invalid image views during
image-table descriptor updates (02996); those and dual-source attachment 09239
remain under investigation. The repaired ship geometry remains visible.

## Follow-up: descriptor binding order

A cold startup core showed Vulkan validation 1.4.363 retaining a 15-binding push
set while processing an 18-binding write. A standalone Vulkan reproducer isolates
the transition: push set 0, replace it with an ordinary set, bind a higher set
with another layout, then push set 0. The old order crashes in validation's
DescriptorSet::PerformWriteUpdate (exit 139), without a preceding VUID.
Binding set 0 before higher sets passes (exit 0, no VUID). This order also reaches
the BO4 title screen past the captured startup failure. The emulator now binds
sets in ascending order; this does not change guest shader semantics.
Local diagnostic source/log: out/ship-fix/push-layout-repro.cpp and .log.

## Follow-up: image-table descriptor lifetime

Image-table construction can retire views collected earlier in the same pass;
waiting to reuse a descriptor set can also finish their deferred destruction.
The assembled table is now checked against the cache's view and image identity
immediately before updating descriptors. Retired entries become null and mark
the table unresolved for a later rebuild. Existing draw-time validation remains.

The combined cold-start run reaches Voyage of Despair, turns toward the sea and
lantern, fires the weapon, and moves toward the ship's stairs. Its log contains
no 02996, 06409, or 01389 reports. The remaining Vulkan category is 09239.
The preserved muzzle-flash frame `rendering-fire-20261113.png` shows lit reflective
weapon surfaces, skin, textured zombies, deck, restored ship geometry and fog.
This is representative scene/effect coverage, not a full-match/all-map claim.
Local frames/logs: out/ascending-*.png and out/ship-fix/ascending-gameplay-final.log.


## Follow-up: writable stencil aliases

IX exposed a separate R8 stencil alias being redirected to a depth/stencil storage
view, producing 00339, 00344 and 07028. Writable stencil aliases now retain their
R8 backing; stencil bytes are copied from the associated depth image before use
and copied back after the dispatch, without replacing depth. The association is
also preserved when a placeholder first becomes a real image. The transfer path
currently handles single-sample surfaces.

Independent OpenOrbis homebrew `stencil_storage_alias` writes depth 0.5/stencil
0x35, increments one stencil pixel twice through R8_UINT storage, and checks the
result using depth/stencil tests at every pixel. The baseline passes its ordinary
draw control but fails both storage phases, with 00339/07028. The fixed build
passes all three cases with core and synchronization validation enabled and zero
VUID reports. Original shader source and build files accompany the fix. Physical
PS4 validation is unavailable; these are emulator results.

Local evidence: out/ship-fix/stencil-probe-baseline.log,
stencil-association-test.log, stencil-dispatch.rdc and stencil-dispatch2.rdc.
The captures isolated the missing first-use association. The temporary capture
hooks were removed before the passing build. The subsequent live IX run reaches gameplay with zero Vulkan validation errors.
The frame remains too dark, which was traced separately below.


## Follow-up: runtime volume texture tables

The IX capture `out/ship-fix/ix-stencil-fixed.rdc` has valid textured geometry and
normal/material buffers. Lighting dispatch 8501 (shader a1f741f9) reaches visible
pixels, but accumulated environment lighting is zero. The probe weight is 1, so
the shader suppresses fallback lighting. AO and transmission are nonzero;
removing denormal flushing does not fix it. Table entries 54–67 are absent from
the 2D table, while the corresponding cube entries 40–53 are present. Guest
descriptor logging confirms BC6 volume images were rejected as 2D table entries.

Dynamic sampled tables now include a 3D image table. The shader reads the actual
image type from descriptor word 3 and selects the correctly dimensioned sample,
preserving Z and keeping mixed 2D/3D tables valid. Table identity, hashing and
switch tracking distinguish volume images. Shader binary version is increased
so old compiled sampling paths are rebuilt. Temporary descriptor logging and
all replay shader substitutions are excluded from production source.

The independent `runtime_image_dimensions` homebrew passes 2D reads and guards
on the baseline, but returns zero for all 64 volume reads. The fixed build passes
all cases, including alternating Z slices. The stencil, mip-subrectangle and
runtime buffer-table probes also pass. All four run with core and synchronization
validation and have zero VUID reports. Build and diff checks pass. Live IX visual
verification of the volume change is in progress.

Evidence: out/ship-fix/ix-lighting-investigation.log, ix-table1.json, ix-table2.json,
volume-table-descriptors.log, volume-probe-baseline.log and volume-regression-*.log.


The subsequent live IX run reaches the arena with textured stone, wood, banners,
lit skin and reflective weapon metal visible in `rendering-ix-volume-20261113.png`.
The corridor remains appropriately darker than the sunlit arena. The downed
state also retains weapon detail and renders the blood overlay/desaturation.
The run has zero Vulkan VUID reports through gameplay and game over. This is
short representative coverage; no completed round or all-map validation is claimed.
Unresolved SRT-walker diagnostics remain in the cold shader compilation log.
The menu player portrait and Alpha Omega thumbnail still require investigation.
The user has confirmed no physical PS4 is available for the blend measurement.

Local evidence: out/volume-ix-{gameplay,gate,combat,exitfocus}.png and
out/ship-fix/volume-fixed-ix-gameplay.log. The local shared homebrew bundle
`out/bo4-rendering-probes.tar.gz` now includes runtime_image_dimensions and
runtime_buffer_table alongside the four earlier rendering probes.


## Correction: Alpha Omega preview is intentional mosaic artwork

Draw 21155 of `out/ship-fix/alpha-artifact.rdc` samples one 1920x1080 BC7 sRGB
image (resource 512845). Exporting it at full size reveals intact miniature
screenshots forming the artwork. It is not a tiling corruption: Treyarch
released this mosaic in July 2019. Reference announcement:
https://doope.jp/2019/0792262.html (links Treyarch's original announcement and
https://www.callofduty.com/cdn/images/alpha-omega.jpg). The official image
URL timed out during retrieval; the exported texture and announcement establish
the mosaic design. No emulator change is warranted for this preview.

A separate bright purple ground reflection appeared in the lobby after returning
from IX, then disappeared after capture. `out/ship-fix/lobby-reflection.rdc` and
`out/volume-mutation-lobby.png` preserve that investigation; cause is not yet
established. The player portrait also remains to be checked against its artwork.


## Follow-up: guest invalidation of runtime image tables

The independent runtime_image_updates homebrew isolates a same-frame coherency
bug. It samples two dynamically indexed 2D textures, waits for GPU completion,
then changes texture contents from the CPU and swaps descriptor entries between
submissions. Each dispatch has an explicit cache invalidation. Before SubmitDone,
the baseline returns the old values for all 128 samples in both update phases.
The initial read, guard words and the control after SubmitDone pass.

ACQUIRE_MEM texture/L1/scalar cache actions now invalidate the cached table
snapshot and its per-image synchronization state on both graphics and compute
queues. Guest invalidation no longer waits for the frame epoch to change. The
fixed build passes all five cases with core and synchronization validation and
zero reports. The four related probes pass their data checks; one mip_subrect
run reports a separate WRITE_AFTER_PRESENT swapchain warning during presentation.
That warning is retained in image-updates-regression-mip_subrect.log.

The preceding unmodified warm-cache game run reaches IX, game over and the lobby
without Vulkan or synchronization reports, and the transient purple reflection
does not reproduce. This does not prove its cause. Live verification of the new
invalidation fix is in progress. No physical PS4 execution is claimed.

Evidence: out/ship-fix/image-updates-{baseline,fixed,fix-build}.log,
image-updates-regression-*.log, warm-sync-gameplay.log and out/warm-sync-return.png.


## Follow-up: independent allocations with equal texture contents

The runtime_image_identity homebrew exposes a second cache bug: two allocations
with identical nonuniform contents share a cached image because the content key
clears the base address. Updating only one then changes the sampled result of
the other. Baseline 011768cb passes initial values and guards but fails the CPU
update, descriptor swap and SubmitDone phases with 64 wrong words each.

Image keys now retain the descriptor's allocation address. The identity probe
and all five related probes pass with core and synchronization validation and
zero reports. Build and diff checks pass. The earlier isolated mip_subrect
repeat also passes without the intermittent presentation warning; its initial
warning remains documented above. Live game validation of allocation identity
is pending. The data checks are emulator results, not physical-console results.

Evidence: out/ship-fix/image-identity-baseline.log, image-identity-fixed-*.log
and image-identity-fix-build.log.


The combined cache fixes reach IX gameplay with core and synchronization
validation enabled and no reports (image-identity-ix-gameplay.log). The normal
run with RenderDoc and validation disabled reaches the arena and preserves
textured stone/wood, colored banners, lit skin and reflective weapon metal from
several angles. `rendering-ix-cache-fixes-20261113.png` preserves that build.
The weapon fires and its ammo decreases; consecutive live frames retain material
detail through damage and the downed-state overlay. One frame grabbed immediately
after an emulator pause/resume had black surfaces, which cleared after settling;
the subsequent 16-frame unpaused sample does not reproduce them. This is not
proof that every temporal artifact is resolved.

Normal title/menu performance is about 27–34 FPS; the sampled IX gameplay views
are around 10–15 FPS, so no full-speed claim is made. The VRAM guard remains
untriggered at about 10.1 GB. Local frames: out/normal-ix-*.png and
out/ship-fix/live-frames/*.png. Additional map/effect validation continues.


## Follow-up: small bound buffers used by dynamic scalar reads

Remaining Phi-based SRT diagnostics led to the dynamic_scalar_reads homebrew.
With DMA disabled, 64 scalar-loop sums are wrong while ordinary buffer reads and
guards pass. With DMA enabled, the sums are zero for a 256-byte source. Increasing
the source to 64 KiB makes the existing enabled path pass: streaming small buffers
omits their guest-address mapping. Shaders that use DMA now obtain resident
buffers for both ordinary and runtime-table bindings. The original small-source
probe passes its first dispatch; runtime_buffer_table also passes. Both have
zero core/synchronization validation reports. Build succeeds.

This fixes bound-buffer aliases, not arbitrary unbound pointer residency. The
live configuration previously disabled DMA; live validation with it enabled is
pending. Evidence: out/ship-fix/dynamic-scalar-{disabled,enabled,resident-enabled,
fixed,buffer-regression}.log. Original sources accompany the fix.


## Follow-up: DMA setting and cached shader compatibility

An isolated cache seeded with DMA disabled is reused when DMA is enabled:
preloaded=1 and all 64 dynamic sums remain wrong despite the resident-buffer fix.
Shader metadata now records the DMA setting and rejects mismatches. Metadata
version increases from 15 to 16. The same-version toggle test seeds a disabled
cache, enables DMA (preloaded=0, PASS), repeats enabled (preloaded=1, PASS), then
disables DMA (preloaded=0, the expected unsupported dynamic-read failure). All
four runs have zero core/synchronization validation reports. Build/diff checks
pass. This prevents old shaders from silently defeating a configuration change.

The first live Voyage comparison had stale disabled-DMA shaders and still showed
bright rectangular effect artifacts above the otherwise textured deck. It is
not a valid enabled-DMA comparison. The cache-corrected live run is now pending.
Evidence: out/ship-fix/dma-cache-{seed,toggle-baseline,seed-fixed,toggle-fixed,
warm-fixed,reverse-fixed}.log and dma-voyage-first.png.


## Follow-up: dependent depth image transitions

The cold validation run exposes a depth-image WRITE-AFTER-WRITE hazard. The
independent depth_sample_attachment homebrew reproduces it with sampled depth
and active stencil testing on the same image. Pixel checks pass, but two dependent
layout transitions in one barrier command lack the required ordering. The
stencil-disabled control is clean. Runtime now flushes pending transitions for
an image before queuing another transition for that image.

The reproducer now passes both pixel cases with zero validation reports. The
stencil storage alias, mip/subrectangle and mixed 2D/3D probes also pass with
zero core/synchronization reports. Build/diff checks pass. Live verification
of this final ordering change remains pending.

The same cold run also reports WRITE-AFTER-PRESENT and the known higher-MRT
SRC1 blend error. Presentation remains under investigation; no suppression or
speculative synchronization workaround is applied. The installed validation
layer reports API version 1.4.363. Evidence: out/ship-fix/depth-sample-*.log,
depth-order-*-regression.log and dma-cold-validation-startup.log.


The c73ade90 visual run reaches Voyage's forecastle with textured deck boards,
ice, hull/windows and reflective weapon metal. The bright rectangles seen in
the earlier stale-cache comparison are absent in entry and subsequent live
views. Turning and firing work (ammo 8 to 7); steam/smoke remains visible. The
settled view reports about 7 FPS. Immediate resume samples briefly reported
about 1 FPS, so these are not full-speed results. No causal claim is made that
DMA alone fixed those transient rectangles; this run logs no new DMA shader
compilations and cached/scene differences remain possible. The depth-ordering
fix was built while this older process ran and is not included in these frames.

Saved image: rendering-voyage-cache-fixes-20261113.png. Local evidence:
out/dma-voyage-live{1,2,3}.png, dma-voyage-settled.png and
out/ship-fix/dma-visual-voyage.log. Next: live validation of the depth ordering
change and investigation of presentation/blend diagnostics.


## Follow-up: runtime image first-use correctness

The runtime descriptor-table resolver silently limited new image resolutions to
48 per table epoch. Entries beyond that limit returned a zero descriptor until
later frames, even when the shader needed them immediately. Remove that limit;
all valid entries needed by the table must resolve before dispatch/draw.

Original runtime_image_first_use homebrew samples 64 distinct textures in its
first dispatch. Baseline 57cd101e returns zero for textures 48 through 63 (1,024
wrong output words). The fix passes all 4,096 output words and 16 guards, with
zero core/synchronization validation reports. Runtime image updates and identity
regressions also pass with zero reports. Build succeeds. Live memory/visual
validation of this fix is in progress. No physical-console result is claimed.
Evidence: out/ship-fix/first-use-{baseline,fixed,updates-regression,
identity-regression}.log. Original homebrew sources accompany the change.

The separate 3d165abf live validation run reaches Voyage gameplay with textured
deck, ice and hull, and reflective weapon surfaces. It has no depth-image
WRITE-AFTER-WRITE report. WRITE-AFTER-PRESENT and higher-MRT SRC1 errors remain.
Evidence: out/depth-order-voyage-first.png and out/ship-fix/depth-order-voyage.log.


First-use live follow-up: IX reaches the arena with detailed stone, wood, cloth,
weapon metal and skin. A settled arena view reports 21 FPS; this is a single
scene sample, not a performance benchmark. GPU use stays below the guard at
about 9.5 GiB at gameplay entry. Saved image:
rendering-ix-first-use-20261113.png. Local evidence:
out/first-use-ix-{first,arena}.png and out/ship-fix/first-use-ix-gameplay.log.

One earlier startup stayed on the loading screen for several minutes; the repeat
with identical code reached title, lobby and gameplay normally. Preserve its
log/stacks as first-use-loading-delay.log and first-use-startup-stacks.log rather
than claiming startup stability. The startup thread sample waited on a video
buffer label; no texture-resolution stall was observed.

The presentation warning also occurs with upstream Vulkan-ValidationLayers
37ec2baf9aac8c0f55fe2b9beec1d818b70d7841 (API 1.4.368) in the mip/subrectangle
probe. Updating the validator alone does not resolve it. Investigation continues.


## Follow-up: Alpha Omega live material checkpoint

On 8686f232, Alpha Omega reaches Security Checkpoint with textured buildings,
road, fencing, foliage and skin, reflective gun metal, animated fire and smoke.
Turning and firing work (ammo 8 to 7). The settled second view reports about
13 FPS; the immediate post-pause sample reads 1 FPS and is not a steady-state
measurement. Damage/blood appears after waiting near a zombie. These are two
entry-area views, not a full-match or all-map certification.

Saved image: rendering-alpha-omega-first-use-20261113.png. Local evidence:
out/alpha-first-use-{first,side,settled}.png and
out/ship-fix/alpha-first-use-gameplay.log. The IX second angle also shows a
textured zombie close up, cloth, skin and reflective weapon metal:
out/first-use-ix-side.png. No new DMA compilation diagnostics were observed;
no causal attribution to DMA alone is made.

Presentation investigation: a new original presentation_idle homebrew performs
no guest rendering and passes 121 host presents without validation warnings.
The mip probe can reproduce WRITE-AFTER-PRESENT after its successful pixel
marker if allowed to continue until swapchain image reuse. The installed and
isolated upstream validators both report it. The traced acquire semaphore and
present wait are present, and image extent/format agree with the render area.
Reduced native Vulkan cases (binary/timeline waits, shared and separate
timelines, mailbox/FIFO, pacing, dynamic rendering, sparse-bind synchronization
and concurrent queue work) have not reproduced it. This remains unresolved;
no suppression or speculative workaround is applied. Local diagnostic sources
and logs remain under out/ship-fix/present-*. The idle and allocation controls
are still being narrowed before their sources are finalized.


## Follow-up: presentation false positive isolated

The previous native sparse control used a binary semaphore. Changing only that
semaphore to a timeline reproduces WRITE_AFTER_PRESENT without any guest/game
code or even sparse resource bindings. `diagnostics/sparse-present` contains the
original standalone reproduction, binary and ordinary-submit controls, and a
deliberately invalid missing-acquire-wait control. On upstream validator
37ec2baf (API 1.4.368), the timeline case logs 10 presentation errors (duplicate
limit); both valid controls log zero; the negative control logs 16 core/sync
errors. Each positive case presents 40 frames; the negative presents six.

The cause is missing sync-validation tracking for `vkQueueBindSparse` signals.
The first sparse timeline wait is treated as a future unsignaled value, so
`QueueSubmitCommand::Execute` defers that and all following queue submissions.
Presentation validation continues against stale access state. A traced original
`presentation_compute` homebrew records the arena-memory semaphore unresolved
at value 1, then 242 deferred submissions and 118 presentation hazards. Its
144 output/guard words still pass. `presentation_idle` stays alive for two
seconds without guest rendering and passes with zero Vulkan reports. Both
homebrew sources are shared with this checkpoint.

This identifies the observed presentation diagnostic as a validator tracking
defect; it does not establish all GPU synchronization correctness. Sparse
resource synchronization coverage remains limited by the validator. No emulator
semaphore change, validation suppression, or speculative rendering workaround
is included. The separate higher-MRT SRC1 blend mapping still needs resolution.

Evidence: out/ship-fix/{sparse-present-matrix,present-compute-unresolved-trace,
presentation-idle-final,present-system-final}.log. Native reproduction and
primary-source links are in diagnostics/sparse-present/README.md.


## Follow-up: Blood of the Dead and startup artifact

Blood of the Dead reaches Richtofen's Laboratory with detailed brickwork, pipes,
equipment, weapon geometry/materials, lamp glow, smoke and sparks. Two laboratory
angles render coherently; moving forward and firing changes ammunition 8 to 7
and leaves a visible bullet impact on a metal door. The laboratory samples
report about 13 FPS; a close-up door view reports 26 FPS. These limited scene
samples are not a benchmark or full-map certification.

Saved image: rendering-blood-first-use-20261113.png. Local evidence:
out/blood-first-use-{first,side,forward}.png and
out/ship-fix/blood-first-use-gameplay.log. The automatic entry detector missed
this map's HUD position, so gameplay was paused manually. No frozen-frame or
full-match claim is made.

The same startup displayed a uniform green frame before the title scene became
normal. This is a remaining visual defect, saved as
rendering-startup-green-20261113.png. Its cause is not established; no claim is
made that it was introduced by the first-use change. The Blood of the Dead
intro itself rendered normally. A targeted startup capture is the next step.

The installed validator (1.4.363) also reproduces the native sparse timeline
false positive, with zero errors in its binary and ordinary-submit controls.
Final presentation_compute shader values pass while its presentation diagnostic
remains; presentation_idle passes with zero core/sync reports.


## Fix: cropped views of compatible image formats

The startup green frame is caused by disconnected aliases of the decoded video
planes. A captured draw samples 1920×1080 R8_UNORM and 960×540 R8G8_UNORM images
containing only zero. At the same guest addresses, compute-written 1920×1088
R8_UINT and 960×544 R8G8_UINT images contain valid luma/chroma data. For example,
luma spans 16–229 while its sampled alias stays zero. Guest memory remains zero
because the decoder produces these planes on the GPU.

Subrectangle recognition required identical formats, so it rejected the valid
UINT/UNORM aliases. It now uses the existing Vulkan format-compatibility check
while retaining the layout, pitch, tile, bit-size and resource restrictions.
When a cropped CPU-backed image already exists, a compatible larger image with
valid GPU data takes precedence. Existing subrectangle copies preserve the
requested dimensions and reinterpretation view. No title or shader is special
cased.

Original subrect_format_alias homebrew reproduces the first R8 read returning
zero instead of 1.0 on baseline 12b9493b. The fix passes R8/R8G8, two GPU writes,
optional prior cropped reads, cropped dimensions and guards, with zero core/sync
reports. Mip subrectangles, writable stencil aliases and mixed 2D/3D runtime
image regressions also pass with zero reports. Build and diff checks pass.

The live startup sequence now shows animated characters, scenery and the logo
instead of a solid green screen, then reaches the title normally. Saved image:
rendering-startup-video-fixed-20261113.png. Local frame sequence:
out/ship-fix/startup-fixed/. The bounded observation is not an all-video or
full-match certification; higher-MRT SRC1 semantics remain unresolved.

Evidence: out/ship-fix/startup-green.rdc, green-plane-aliases.txt,
startup-green-root-cause.txt, subrect-format-{baseline,fixed,build}.log and
subrect-format-*-regression.log. The earlier capture attempt was interrupted by
GDB stopping on an emulator-handled guest fault; the corrected debugger settings
preserved normal fault handling and produced the complete capture.


## Follow-up: Classified and a post-map lobby artifact

Classified reaches the Conference Room on f7fa1e83 with detailed furniture,
curtains, flags, weapon materials, working surveillance screens and light beams.
Two inspected views show no matte-grey replacement; firing reduces ammunition
from 8 to 7. These samples report about 8–9 FPS and are not a performance or
full-map pass. Local images: out/classified-{entry,side}.png. Log:
out/ship-fix/classified-fixed-gameplay.log.

Returning to the lobby exposes a separate persistent bright-magenta fire
reflection. Screenshots before and after a five-second wait retain the artifact.
The startup is clean, so the movie fix does not certify map-unload behavior.
Local evidence: out/classified-return-magenta.png and
out/ship-fix/classified-return-magenta.log. Root cause is still under investigation.

A capture-enabled repeat starts with a clean lobby and also returns cleanly on
its first transition. The original return artifact therefore is not yet a
deterministic reproducer. A correct reference frame is saved locally as
out/ship-fix/classified-fresh-lobby.rdc. No speculative resource-cache change has
been made.

SPIR-V validation with --target-env vulkan1.3 passes all 2,001 dumped modules in
the diagnostic directory, including 32 newly dumped since this capture run
started. This validates module structure, not complete rendering semantics.
Detailed results: out/ship-fix/magenta-shader-validation.json.

### Reduced-memory reproducer comparisons

The capture-enabled process completed three Classified-to-lobby transitions
without the bright magenta patch. All three followed an initial reference
capture, so this does not yet isolate capture instrumentation from the act of
capturing a frame. The final screenshot is out/magenta-crossmap-return.png;
log: out/ship-fix/magenta-three-capture-transitions.log.

A normal run with the advertised VRAM reduced from 10 GiB to 8 GiB (application
budget approximately 3,492 MiB) reproduced the magenta patch after Classified.
RPCS3 remained running on the shared GPU, as requested. Evidence:
out/reduced-return-lobby.png and out/ship-fix/magenta-reduced-return.log.
An earlier normal run with the larger budget hit the 12,000 MiB total-VRAM
guard before reaching the menu. Reducing the budget permits this comparison;
it does not fix the reflection.

A same-budget run with direct-memory shader reads disabled reached Classified
and returned to a lobby with an unusually bright red reflection instead of the
magenta patch. This is not evidence of a correct workaround. The initial black
map-entry frames were a delayed transition, not an established rendering defect.
Evidence: out/no-dma-after-intro.png, out/no-dma-lobby-result.png and
out/ship-fix/no-dma-return-final.log. Direct-memory reads were restored for the
next capture-enabled comparison, which will not take an initial reference
capture. No emulator source change has been made for this symptom.

The normal live shader profile reports support_bda=true. Pipeline warmup already
compares the entire Shader::Profile in addition to the DMA-setting metadata;
a missing device-profile compatibility check is not supported by code inspection.
A sampled runtime sampler table contained 85 records (1,360 bytes), all with
border_color_type=0. Neither a giant sampler-table hash nor custom-border-color
caching explains that sampled table.

The reference RenderDoc capture contains two guest frame sequences. Its first
partial frame replays with invalid-looking geometry and must not be treated as
live visual evidence. The final presentation has coherent background colors and
fire, with character ghosting; it is not a pixel-identical live oracle. Evidence:
out/ship-fix/fresh-lobby-replay-present.png and fresh-lobby-scan.txt.


### Raw buffer reads missed GPU-written images

The capture-enabled run without an initial capture also reproduced the bad
reflection. Its shader profile matched the normal process. A later capture
spanning multiple submit boundaries provided a coherent second guest-frame
sequence; the partial first sequence remains unsuitable as visual evidence.
Artifact: out/ship-fix/classified-reflection-three-frames.rdc (1,964,925,142 bytes).

In that second sequence, water draw 11791 changes scene pixel (940,795) from
approximately (0.0128,0.0198,0.0391) to (108,0.03369,0.01221). It samples a
reflection image at guest 0x242f3b8000 containing extreme values and stripes,
and a zero-filled depth copy at 0x242eab8000. Raw-buffer dispatches 11732 and
11758 copy those allocations from live color/depth image addresses 0x2429320000
and 0x2426b88000. Their sources were not synchronized from the GPU images before
ordinary buffer reads. Evidence: reflection-copy-chain/chain.txt,
reflection-input-history/history.txt, and reflection-three-frames-present.png
under out/ship-fix.

Original OpenOrbis probe image_raw_buffer_copy independently reproduces the
coherence defect: baseline 17345b92 passes the CPU-initialized control but reads
stale 0x12345678 after a GPU image write whose expected first word is 0x10213000.
The fix synchronizes read-only raw buffers as well as texel buffers, prevents
small image-backed reads from bypassing the cache, and supports interior reads
by staging the full tiled image and copying only the requested byte interval.
Image lookup prefers a unique exact allocation match or a unique containing
image; it does not arbitrarily select between ambiguous aliases.

Build succeeds. The probe passes tiled and linear layouts, two GPU writes per
layout, 64-byte reads at offsets 0/4/4096, and output guards, with zero Vulkan
core/synchronization reports. Six regressions also pass with zero reports:
subrect_format_alias, stencil_storage_alias, depth_sample_attachment,
runtime_image_updates, runtime_image_identity, and dynamic_scalar_reads.
Logs: raw-image-copy-baseline.log, raw-image-copy-fixed.log,
raw-image-copy-fix-build.log, and raw-copy-regression-*.log under out/ship-fix.
The original probe source is shared under hardware-tests/tests/image_raw_buffer_copy.
No physical PS4 result is available. Normal-game validation of this fix remains
pending at this checkpoint; the separate higher-MRT SRC1 question is unresolved.


Normal-game follow-up on c031a913: with RenderDoc disabled and DMA enabled,
two consecutive Classified entries and returns show detailed conference-room
materials and ordinary fire/water reflections. The prior bright magenta/red/blue
patch is absent from both inspected lobby returns. Screenshots:
rendering-raw-copy-classified-20261113.png and
rendering-raw-copy-lobby-{first,second}-20261113.png. The second entry reached
round 1; the after-action screen reports a 20-second match. These are short
transition checks, not full-match coverage. RPCS3 remained running throughout;
BO4 used an advertised 8192 MiB VRAM budget. No renderer/shader Error or Critical
entries were logged through these checks (normal-run Vulkan validation disabled).
The local bundle out/bo4-rendering-probes.tar.gz now contains 15 original ELFs.


Dead of the Night on c031a913 reaches North Atrium Bridge. Two inspected views
show marble grain, carved stone, reflective metal, window glass, warm lights,
blue light effects, and the glowing red magical seal. Firing changes ammunition
from 8 to 7. No obvious matte-grey replacement or broken geometry appears in
these views. Observed performance is 8–10 FPS on the shared GPU. This is short
opening-area coverage, not a full round. Screenshot:
rendering-dead-of-night-20261113.png; local log: out/ship-fix/raw-copy-dead-night.log.
The Alpha Omega thumbnail remains the intentional mosaic documented above.


Ancient Evil reaches Temple of Apollo. The first late observation caught the
player downed and is unsuitable for normal-lighting assessment. A repeated run
captured a 25-frame low-memory sequence; frames 15 and 18 show normal lighting
before the downed effect: gold statue reflections, stone/cloth/skin detail,
textured zombies, fire, green smoke, and the blue wall weapon effect. No obvious
matte-grey fallback appears. Observed performance is 6–7 FPS; the unattended
character subsequently downs. No completed round is claimed. Saved frames:
rendering-ancient-evil-{temple,zombies}-20261113.png. Local evidence:
out/ship-fix/ancient-evil-sequence and raw-copy-ancient-evil.log.
The lobby also remained free of the bright reflection patch after Dead of the
Night and the first Ancient Evil run (four inspected returns in this process).


Tag der Toten opening coverage shows textured snow, ice, buildings, water, skin,
and reflective weapon surfaces. A 30-frame screenshot sequence preserves the
transition into the first round; performance ranges from about 8 FPS early to
1–2 FPS later with capture instrumentation and shader dumping enabled. This is
not a normal-run performance measurement or a full-round validation. Saved
screenshot: rendering-tag-der-toten-20261113.png. Local files:
out/ship-fix/tag-toten-sequence, raw-copy-tag-final.log, and
tag-depth-baseline.rdc (2,814,317,292 bytes). The capture spans multiple submit
boundaries; its first partial sequence must be checked before use.

### Remaining dynamic shadow-table bindings

Ancient Evil's cold shader compilation logs 204 renderer/recompiler Error lines,
including unresolved SRT offsets and Phi instructions, plus unflattened-sharp
warnings. The earlier zero-renderer-error observation applied only through the
Classified checks. The scalar-loop homebrew can pass while logging an SRT error,
so the diagnostic alone is not a sufficient failure oracle.

An offline cache audit using the current executable's debug types identifies a
concrete gap: fragment program 0x9831dde2 has invalid image fetches at bindings
21/22 in all four cached permutations; 0xdbfe971f has invalid fetches at 22/23 in
all seven cached permutations. Every dword offset is UNKNOWN_LOCATION. In each
pair, both are depth-comparison textures, one array and one non-array. Their
InfoPersistent uses_dma flag is false; these missing image descriptors are not
covered by the ordinary scalar-read DMA fallback. Runtime table discovery
explicitly excludes is_depth, and the host table path also excludes R16/R32 and
GPU render/depth targets. These exclusions must be addressed together, including
per-draw synchronization of GPU-written depth images and descriptor layouts.
Evidence: out/ship-fix/ancient-cache-audit.json and audit-cache.py. An independent
OpenOrbis runtime_depth_tables probe is being prepared; no shadow-table fix is
claimed at this checkpoint.


### Runtime depth-comparison table implementation

The original runtime_depth_tables homebrew now uses direct user-data texture
controls. All four controls pass on c1c681e0, while runtime table selection fails
with 256 output mismatches. The earlier constant S_BUFFER control encountered
an SRT-walker null-address fault and is not used as the baseline.

Depth instructions now participate in runtime table discovery. Depth/color
resources and table identities are distinct. R32_FLOAT and R16_UNORM depth
entries resolve through the ordinary image cache without CPU-content
invalidation, and are updated and transitioned for each draw. Identical views
reuse their immutable descriptor set. Shader binary version is 24.

The extended original probe passes four controls, four runtime cases including
an in-place descriptor swap, and three successive GPU-written depth cases.
The initial fixed run disables automatic linear-image readback. A separate run
with core and synchronization validation enabled also passes with zero VUID or
SYNC reports and zero renderer/recompiler Error lines. Seven adjacent probes
(depth_sample_attachment, runtime_image_dimensions, runtime_image_updates,
runtime_image_identity, image_raw_buffer_copy, subrect_format_alias, and
stencil_storage_alias) pass with zero VUID/SYNC reports as well. The actual
validation settings are confirmed in each emulator log; earlier exploratory
runs used unrecognized validation aliases and are not validation evidence.

Build: out/ship-fix/depth-table-fix-build2.log, BUILD_EXIT=0.
Baseline: depth-direct-run.log. Fixed: depth-table-fixed.log.
Validated suite: depth-validated-*.log. Post-fix game checks remain in progress;
passing synthetic cases alone does not establish correct game shadow tables.


Post-depth-fix game checks reach Ancient Evil and Tag der Toten. Ancient's
normal opening retains gold, stone, cloth, skin, fire and blue-effect detail;
89 fresh shader permutations contain valid runtime depth tables. Tag's
0xdbfe971f permutations 0–2 now have valid depth tables at bindings 22/23,
previously invalid in the version-23 cache. Program 0x9831dde2 was not exercised
in this run. Tag shows textured snow, skin, metal, water and sunset lighting.
Screenshots: rendering-{ancient,tag}-depth-fixed-20261113.png. These remain
opening-area checks, not completed rounds. The player downed while unattended.
The post-Ancient report briefly showed black clothing; the following lobby
view restored the full clothing textures. That transient screen needs a repeat
check. Local log: depth-fixed-ancient-tag.log. RPCS3 remained running throughout.

An offline structural check passed 2,275 dumped SPIR-V modules. Metadata audit
also exposed 18 compute programs with other invalid image bindings, including
runtime IMAGE_GET_RESINFO and compact R128 texture descriptors. Structural
SPIR-V validity does not establish correct resource binding.

### Runtime image-size queries

Original runtime_image_queries passes four direct controls but fails all four
runtime cases on f1e7f9ec with 1,920 mismatches. Runtime query lowering now reads
sizes and view ranges directly from the descriptor, avoiding a null image
binding. The eight cases pass with nonzero base mips, array subranges, mixed
2D/3D types, two LODs, table rewrites and guards. Core/synchronization validation
reports zero VUID/SYNC errors. Binary cache version is 25.

Depth tables, mixed image dimensions, image updates, and dynamic scalar reads
also pass with zero VUID/SYNC reports. The scalar probe requires DMA enabled;
an initial run with its default disabled fails as documented and is not a
regression. Build: query-fix-build.log, BUILD_EXIT=0. Probe logs:
query-baseline3.log, query-fixed.log, query-regression-*.log (use scalar-dma for
the scalar probe). Post-query-fix game validation remains pending.


### Runtime compact texture descriptors

Original runtime_compact_images directly samples compact four-word (R128)
descriptors as controls, then selects them through runtime tables. Packed
16-byte and padded 32-byte records, poisoned unused words, in-place rewrites,
and guards are covered. Preserved baseline b81714a5 passes controls but returns
zero in runtime cases (512 mismatches); the fixed executable passes all eight.
Core/synchronization validation reports zero VUID/SYNC errors.

Table discovery now accepts compact descriptors, table entry counts/bounds use
four words, high words are zeroed, and pitch is reconstructed from width as in
the existing static compact-descriptor path. Compact/full table identities are
separate and compact samples do not generate a spurious volume-table variant.
Shader binary version is 26. Depth tables, runtime queries, mixed dimensions,
image updates, and image identity all pass with zero VUID/SYNC reports.

Build: r128-fix-build.log, BUILD_EXIT=0. Reliable paired evidence:
r128-baseline-final.log and r128-fixed-final.log. Earlier exploratory compact
probes contained stride/register mistakes and are not baseline evidence.
Neighboring probes: r128-regression-*.log. Game checks with the combined query
and compact-table fixes remain pending.


### Runtime typed image loads

The combined query/compact-table game run reached Tag with 1,036 freshly
compiled cache records. Only fragment program 0x2f6fe911 retained an invalid
image binding: IMAGE_LOAD from a runtime table. A bounded debugger read found
the referenced descriptor at index 79: a 257x513 R32_UINT mask used for pixel
discard. This identifies the binding failure without relying on a screenshot.

Runtime image loads now use separate float/unsigned/signed descriptor tables,
with planar/volume variants and array views. GPU-backed entries resolve and
synchronize each draw; immutable sets track both image views and layouts.
Static image specializations also distinguish signed and unsigned formats.
Metadata/binary cache versions are 17/27.

Original runtime_image_loads passes all 21 expanded cases (nine direct controls,
twelve runtime cases including repeated GPU writes) with automatic linear
image readback disabled and zero core/synchronization validation errors.
Its 14 SPIR-V modules validate. The corrected pre-fix 2D baseline had 384
runtime mismatches and a UINT/SINT descriptor type VUID. Earlier uninitialized
probe descriptors are excluded from the baseline.

Nine neighboring probes pass with zero VUID/SYNC reports: depth tables, image
queries, compact images, mixed dimensions, updates, identity, raw buffer/image
coherence, subrect aliases, and stencil storage aliases. Build:
load-fix-build2.log (BUILD_EXIT=0). Evidence: load-baseline-valid.log,
load-fixed2.log, load-spirv-validation.log, load-regression-*.log.
The post-load-fix BO4 check is in progress; these synthetic passes alone do not
certify all game shaders or resolve the separate higher-MRT SRC1 question.


Post-load-fix Tag reached normal gameplay with textured snow, water, wood,
metal, skin and sunset lighting. A live host-memory inspection confirms that
table slot 79 is bound to a non-null unsigned 2D image view for the 257x513 mask;
the incompatible float/signed/volume tables correctly leave that slot null.
Evidence: bound-load-tables.json and inspect-bound-loads.log. The completed
initial run has 930 audited records (excluding seven fetch-shader records),
zero invalid resource bindings, and zero renderer errors. It remained below
the memory guard with RPCS3 running. This was an opening check, not a full round.

Repeated full image resolution reduced observed Tag performance to about 4 FPS.
A follow-up reuses registered direct image views while still calling UpdateImage
and issuing per-draw transitions. Image identity, registration generation,
rebind requests, and depth associations guard reuse. Depth/stencil aliases and
cropped images retain full resolution. The original homebrew now also checks
three CPU rewrites after GPU writes: all 24 cases pass with zero VUID/SYNC
reports and linear image readback disabled. Depth and subrect probes also pass;
stencil_storage_alias passes with its required linear-output readback enabled.
Disabling that readback fails even its direct control and is not regression
evidence. Build: load-cache-final-build.log (BUILD_EXIT=0). Probe evidence:
load-cache-final-runtime_image_loads.log, load-cache-final-runtime_depth_tables.log,
load-cache-final-subrect_format_alias.log, load-cache-final-stencil-readback.log.

The cached-view game run retains correct visible materials but remains slow,
around 4–5 FPS; this is not a general performance fix. The metadata audit now
parses fetch-shader data too: 1,010 reached records across fragment, vertex,
compute, hull and local stages have zero invalid bindings. All 2,623 dumped
SPIR-V modules validate (load-game-spirv-validation.log). Screenshot:
rendering-tag-typed-loads-20261113.png. Gameover/downed desaturation is excluded
from material comparison. Full-match coverage, the earlier transient black
post-Ancient clothing, and higher-MRT SRC1 semantics remain unverified.

## Mixed planar/cube shadow tables (follow-up)

The black after-action-report clothing is reproducible, not yet merely an
unverified transient. A bounded 1.94 GB RenderDoc capture was taken with RPCS3
left running. Clothing albedo and normals are present in the G-buffer. At
compute event 2955, a clothing pixel falls to approximately
(0.000446, 0.000519, 0.000717). Descriptor inspection shows that 2D D16 shadow
maps occupy slots accessed through a table inferred to contain cubes, so those
slots are null. Shared cube-coordinate ancestry made UsesCubeFaceMath classify
planar shadow sampling as cube sampling. A diagnostic replay correction uses
the existing 2D shadow table and planar coordinates: the same pixel becomes
(0.154297, 0.199219, 0.367188), and the coat, belt and trousers become lit again.
This replay experiment is evidence, not a shipped shader override.

The generic fix retains cube and planar variants and chooses their sampled
result from the runtime descriptor type. Array instructions retain array
coordinates in the planar variant. Binary cache version is now 28; metadata
layout remains 17. The original runtime_shadow_dimensions homebrew shares
cube-face calculations between planar and cube descriptors, varies depth
spatially and by face/layer, and swaps a runtime table in place. Its baseline
passes eight direct controls but has 128 runtime mismatches. The fixed build
passes all 12 cases with zero VUID/SYNC messages. Six neighboring probes also
pass: runtime_depth_tables, runtime_image_dimensions, runtime_compact_images,
runtime_image_loads, runtime_image_queries, runtime_image_identity. Build:
shadow-fix-build.log (BUILD_EXIT=0). Evidence: shadow-baseline.log,
shadow-fixed.log, shadow-regressions.result, black-replay/server.log.
Fresh-game visual confirmation is in progress; no physical PS4 result claimed.

Fresh-game confirmation on 3864bef0: Tag gameplay, gameover, after-action report,
and return to lobby preserve textured, lit clothing. The report no longer has
the black-clothing defect. Screenshots: rendering-report-shadow-fixed-20261113.png
and rendering-lobby-shadow-fixed-20261113.png. Tag reached round 1 and ran for
1m14s in game time; this is transition coverage, not completed-round coverage.
A 1,109-record all-stage cache audit has zero invalid resource discoveries;
2,651 dumped SPIR-V modules validate. The gameplay log has zero renderer errors
or critical failures, with Vulkan validation disabled for the game itself.
Original homebrew core/sync validation results are listed above. RPCS3 remained
running; BO4 used a 6 GiB VRAM override. Tag still runs about 4 FPS. Hair edges
remain visibly noisy and are under capture analysis; no all-artifacts-clear
claim is made. The higher-MRT SRC1 semantic question remains open.

## Wave-wide scalar logical flags

Hair uses stochastic alpha rejection, so correct temporal reconstruction matters.
At motion compute event 4466 (CS 3a42b5ee), a stencil comparison fills VCC and
S_AND_B64 VCC,VCC,VCC sets SCC before S_CBRANCH_SCC0. The translator incorrectly
used InverseBallot(result), making SCC depend on each lane's bit instead of
whether the entire scalar result is nonzero. This excluded neighboring lanes
from the DS_SWIZZLE reductions. At packed motion texel (75,70), the capture
contained 0x01530000, which the temporal resolve decodes as a 40-pixel history
shift; the diagnostic uniform-branch replay yields 0xb8207da3. The complete
fresh-frame temporal response remains to be checked; this single replay still
uses history generated by the old binary.

The generic fix changes SCC for the three 64-bit logical translation helpers
(AND-family, OR/XOR-family, NOT) to a nonzero test. EXEC-mask lowering is unchanged.
Shader binary version is 29; metadata remains 17. Original scalar_mask_flags
homebrew covers 75 cases, all 64 lanes, sparse/high-bit masks, S_CSELECT,
S_CBRANCH_SCC0, cross-lane swizzle and output guards. Baseline: 1,668 mismatched
words, all seven shift controls pass. Fixed: all 75 cases pass. Seven neighboring
probes pass: runtime_shadow_dimensions, runtime_depth_tables,
runtime_image_dimensions, runtime_compact_images, runtime_image_loads,
runtime_image_queries, runtime_image_identity. Build succeeds. No VUID messages.
The scalar baseline and fixed runs each report six WRITE_AFTER_PRESENT sync
messages; depth-tables reports one and image-loads two. These match the previously
isolated sparse timeline presentation validation issue, not a clean sync log.
No shader/resource sync hazards appear. Logs: scalar-baseline2.log,
scalar-fixed.log, scalar-fix-build.log, scalar-regressions.result. No physical PS4
result is claimed. RPCS3 PID 32412 remains running; probes used a 2 GiB override.

Fresh-game confirmation on 397e8719: Tag der Toten renders textured snow, wood,
zombie skin/clothing, reflective weapon metal and muzzle flash from several
viewpoints. One headshot kill is recorded (620 points, 1m19s in game, round 1;
no completed round claimed). The report screen now has continuous hair coverage
in consecutive settled frames, replacing the visibly broken speckled patches
of rendering-report-shadow-fixed-20261113.png. Clothing remains lit/textured.
Return to lobby and change to the Voyage character retain these results.
Screenshots: rendering-report-scalar-fixed-20261113.png,
rendering-tag-scalar-fixed-20261113.png,
rendering-tag-combat-scalar-fixed-20261113.png. The run has zero renderer errors
or critical messages; unrelated HLE stubs remain, and game Vulkan validation
is disabled. Cache audit: 1,165 records across all five encountered stages,
zero invalid discoveries, zero skipped records. All 2,663 dumped SPIR-V modules
validate, including older dumps; this is not an all-shader visual oracle.
Local evidence: scalar-tag-game.log, scalar-all-stages-audit.json,
scalar-shader-validation.json. The shared original-homebrew bundle now has
21 ELFs including scalar_mask_flags. RPCS3 remains running. Tag gameplay is
about 4–6 FPS and report/lobby about 12–13 FPS. Voyage regression is in progress.

## EXEC-aware first-lane selection

The following Voyage load exposed a second shader bug after the SCC correction.
The first run reported device loss with an innocent-context message; the kernel
attributed its ring timeout to the presentation thread. A bounded diagnostic
run with RADV_DEBUG=hang,noumr reproduced the failure and isolated compute shader
376efabf, dispatch 6x1x1, immediately after trace point 22. The dumped SPIR-V
exactly matches cs_0x00000000376efabf_0.spv (SHA256
3d7ffcf7988caeed994c6650a41a3c3fa55cdfd1a107e10e5b5c810ff64e57c6).
The second kernel timeout names GpuComm. RPCS3 PID 32412 remains alive.

A loop removes processed lanes from guest EXEC, then V_READFIRSTLANE chooses
the next key. The old host BroadcastFirst still selects the first physically
active Vulkan lane, including an already processed lane. With correct wave-wide
SCC, the remaining mask never empties and the loop cannot progress. The previous
per-lane SCC accidentally hid this read-first-lane bug. The generic fix carries
the guest EXEC ballot through ReadFirstLane IR and broadcasts from its first
set bit, with lane zero for an empty mask, as documented by AMD. No shader
identifiers or game-specific overrides are included. Binary cache version 30
invalidates the old lowering; metadata remains 17.

Original readfirstlane_exec homebrew checks ten direct masks and forty grouped
loops, including lane 63 and empty EXEC, exact iteration counts, remaining masks
and output guards. A 65-iteration bound prevents the faulty baseline from
hanging the GPU. Baseline 397e8719: 3,908 mismatched words; sparse-mask loops hit
the bound. Fixed: all 50 cases pass. Both report zero VUIDs and three instances
of the previously isolated WRITE_AFTER_PRESENT diagnostic. Build succeeds.
Live Voyage verification and neighboring regressions are in progress.
Local evidence: readfirst-{baseline,fixed,fix-build}.log,
scalar-voyage-hang{,-traced}.log, scalar-gpu-fault{,2}.log, and
driver-home/radv_dumps_35_2026.11.13_18.11.52/. Driver tracing and validation
settings were restored after the diagnostic run.

Final incremental build and repeated readfirstlane_exec pass. Ten neighboring
probes all pass: scalar_mask_flags, dynamic_scalar_reads, runtime_buffer_table,
runtime_shadow_dimensions, runtime_depth_tables, runtime_image_dimensions,
runtime_compact_images, runtime_image_loads, runtime_image_queries,
runtime_image_identity. No VUIDs are reported; intermittent
WRITE_AFTER_PRESENT diagnostics remain recorded in the individual logs.
The shared homebrew bundle now includes 22 ELFs. Game confirmation follows.


Live confirmation of f4b2ac4c: normal RADV (no hang-debug flags) loads Voyage
past the previously hanging 376efabf compute shader and reaches Forecastle.
The run exercises movement, four pistol shots, equipment use, damage/revive,
game over, report and return to lobby. The report records two kills, 680 points,
1m41s of game time and round 1; no completed round is claimed. Textured decking,
blood, ice, ship walls, lit zombie skin/clothes, weapon reflections, fog and
emissive effects remain visible across multiple viewpoints. The report character
has detailed clothing and continuous hair coverage. Screenshots:
rendering-voyage-exec-fixed-20261113.png,
rendering-voyage-effects-exec-fixed-20261113.png,
rendering-voyage-report-exec-fixed-20261113.png.

Preserved log readfirst-voyage-game.log contains zero renderer errors and zero
critical messages; unrelated HLE stubs remain. Game Vulkan validation is off,
so this does not clear the independently known higher-MRT blend VUID. The cache
audit finds 796 current-generation records, zero invalid discoveries, zero
skipped records. All 2,691 dumped SPIR-V modules pass spirv-val (older dumps are
included). Evidence: readfirst-all-stages-audit.json and
readfirst-shader-validation.json. The run uses a 6 GiB override and stays below
the memory guard while RPCS3 remains running. Ancient Evil follow-up is pending.


Ancient Evil follow-up on the same code: the initial Voyage-to-Ancient process
reached Temple of Apollo but the guard terminated BO4 at 12,016 MiB combined
VRAM usage (GTT 2,674 MiB, available RAM 31,291 MiB). That stop was not a shader
hang. RPCS3 stayed running. The repeated, fresh BO4 process uses a 4 GiB driver
VRAM override and completes gameplay, game over, report and return to lobby
below the guard. Combined VRAM observed during gameplay is about 9.5–10 GiB;
this override is an advertised budget, not a hard resident-memory cap.

The 45-frame lightweight screenshot sequence preserves normal-lit frames before
downed-state desaturation. Temple stone, painted columns, cloth, zombie skin and
hair, metallic details, fire, green smoke and blue effects remain textured/lit.
Movement, pistol fire, reload, equipment and damage/revival are exercised. The
report records one kill, 590 points, 1m17s and round 1 (no completed round).
The settled report and returned lobby both show correctly lit, detailed Scarlett
clothing and continuous hair; the earlier post-Ancient black clothing does not
recur. Gameplay samples are about 6–9 FPS, report/lobby about 15 FPS.

The final run contains zero renderer errors and zero critical messages, with
Vulkan game validation disabled and unrelated HLE stubs still present. Audit:
1,152 current-generation records, zero invalid discoveries and skipped records.
All 2,740 dumped SPIR-V modules validate, including older dumps. Evidence:
readfirst-ancient-game.log, readfirst-ancient-guard.log,
readfirst-ancient-{audit,spirv}.log, readfirst-ancient-sequence/0.png and 4.png.
Saved screenshots: rendering-ancient-exec-fixed-20261113.png,
rendering-ancient-report-exec-fixed-20261113.png and
rendering-ancient-lobby-exec-fixed-20261113.png. This closes the specific observed
post-Ancient clothing regression; higher-MRT SRC1 semantics, full-match coverage
and the small player-card artwork comparison remain unverified.


The small purple player portrait is the default Battery emblem. Opening
Identity > Emblems shows its full-size artwork and name. Its shapes, pink/purple
highlights and black regions match VGR's October 16, 2018 in-game screenshot and
Battery close-up; this is intentional artwork, not a shader defect. No emblem
selection was changed. Current screenshot: rendering-battery-emblem-20261113.png.
Reference: https://vgr.com/black-ops-4-emblems/ and
https://www.vgr.com/wp-content/uploads/2018/10/Battery-Emblem-300x300.jpg.
The post-menu audit reaches 1,231 records with zero invalid or skipped records;
all 2,779 dumped SPIR-V modules validate. Logs: readfirst-final-{audit,spirv}.log.


The host watchdog exited at 20:39:24 while the detached BO4 container remained
alive (confirmed at 20:46:33; its recorded PID no longer existed). Earlier
within-guard samples remain valid, but later menu checks were not continuously
guarded. The tooling now runs the watchdog as the command supervisor inside
the container. Headless/windowed launches use it, memory-limit environment
variables are passed through, and no host PID file or background launcher is
needed. A crossed limit or monitoring failure stops only the supervised command
process group; normal/nonzero exits are preserved and termination signals clean
up the child. Container exit then removes remaining container processes.

Functional checks pass: exit status 7 is preserved; an impossible RAM minimum
returns 137 without starting the command; SIGTERM returns 143 and reaps the
owned sleeping child. Python syntax and shell syntax checks pass. A detached IX
run confirms watchdog PID 7 remains under docker-init after the launcher exits,
with fresh memory samples. RPCS3 PID 32412 remains running. The renderer binary
is unchanged; remaining map checks continue with a 4 GiB override.


IX follow-up on the same renderer completes two gameplay/report passes under
the in-container guard. A sunlit arena frame shows textured stone, painted
banners, crowd geometry, directional shadows, wood grain and reflective weapon
metal. The initial gate close-up was movement across the arena, not missing
world rendering; sequence frame 15 preserves the open arena view. Report
Scarlett retains armor, cloth, skin and continuous hair. The second report
records 0 kills, 500 points, 1m11s and round 1; no completed round is claimed.
Gameplay samples are 8–10 FPS, report 17 FPS. Screenshots:
rendering-ix-exec-fixed-20261113.png and
rendering-ix-report-exec-fixed-20261113.png.

Preserved guarded-ix-game.log contains zero renderer errors and critical
messages (game Vulkan validation remains off). The detached watchdog remains
alive for the entire roughly 12-minute run, through repeated level/report/lobby
transitions; guarded-ix-memory.log has fresh samples up to the intentional stop
and no threshold event. RPCS3 remains running. Higher-MRT blend semantics and
full-match coverage remain unresolved.


Blood of the Dead reaches Richtofen's Laboratory on the same renderer. Normal
frames show textured walls, paper/floor debris, wooden benches, glass vessels,
metal reflections, blue blueprint and localized overhead lighting. Movement,
pistol fire, damage and revival execute. Screenshot:
rendering-blood-exec-fixed-20261113.png. Gameplay is only 2–5 FPS, falling to
about 1 FPS at the pause menu. The run does not complete a report transition: the
guard stops BO4 at 19:08:47 UTC with 12,006 MiB combined VRAM, 1,878 MiB GTT
and 21,873 MiB available RAM. RPCS3 remains running. The run's 761 memory samples
peak at 1,943 MiB GTT; the preserved game log has zero renderer errors and critical
messages, with Vulkan validation disabled. Evidence: guarded-blood-game.log,
guarded-blood-memory.log, guarded-blood-game/20.png.

The 4 GiB driver override reports only a 487 MiB usable GPU budget while RPCS3
shares the device. A 3 GiB Classified startup reports zero MiB and is stopped
intentionally before gameplay. A fresh 6 GiB override reports 2,278 MiB usable
budget, while retaining the same 12,000 MiB combined-VRAM watchdog limit. The
override is not a hard resident cap, and lowering it can increase texture-cache
pressure rather than reliably constrain memory. Further map checks use the
usable budget with fresh processes.


Classified completes Conference Room gameplay and a settled report on the same
renderer with the 6 GiB driver override. Surveillance imagery, glossy tables,
leather chairs, scattered papers, carpet, shadows and weapon highlights remain
visible. The settled Dempsey report shows complete textured clothing, skin and
hair; the first transition frame is not used as the settled comparison. The
report records 0 kills, 500 points, round 1 and 1m26s. Movement, pistol input,
damage and revival execute; no full round is claimed. Gameplay is about 1–2 FPS,
so this is not a playability pass. Screenshots:
rendering-classified-exec-fixed-20261113.png and
rendering-classified-report-exec-fixed-20261113.png.

The 633 in-container watchdog samples peak at 9,072 MiB combined VRAM and
1,951 MiB GTT, with at least 21,499 MiB available RAM. No limit event occurs.
RPCS3 remains running. guarded-classified-game.log contains zero renderer errors
and critical messages (Vulkan game validation disabled); resource samples are
in guarded-classified-memory.log.


Dead of the Night reaches North Atrium Bridge and completes a natural game-over
to the settled Godfrey report on the same renderer. Normal-color frames show
reflective statues, patterned stone, red/blue emissive effects, lamps and
localized lighting. The report suit, skin and hair remain complete and lit.
The HUD-triggered route exercises movement, turning, fire, reload/equipment
inputs and damage/revival. End Game could not be selected while downed, so the paused
run is resumed and allowed to reach its report naturally. Report: 0 kills,
500 points, round 1, 1m38s; no completed round is claimed. Gameplay samples are
5–7 FPS and report 21 FPS. Screenshots:
rendering-deadnight-exec-fixed-20261113.png and
rendering-deadnight-report-exec-fixed-20261113.png.

The 710 watchdog samples peak at 11,617 MiB combined VRAM and 2,219 MiB GTT,
with at least 19,850 MiB available RAM. No threshold event occurs; RPCS3 remains
running. guarded-deadnight-game.log has zero renderer errors and critical
messages with game Vulkan validation disabled; samples are preserved in
guarded-deadnight-memory.log.


Alpha Omega completes Security Checkpoint gameplay and a settled Takeo report
on the same renderer. Textured buildings, chain-link/barbed-wire fences,
painted signs, ground detail, vegetation, fire, smoke and moving enemies remain
visible across camera movement. The report retains detailed armor, leather,
cloth, skin and continuous hair. HUD-triggered movement, turning, pistol fire,
reload/equipment inputs and damage/revival are exercised. Report: 4 kills,
500 points, round 1, 1m24s; no completed round is claimed. Gameplay samples are
about 4 FPS, report 12 FPS. Screenshots: rendering-alpha-exec-fixed-20261113.png
and rendering-alpha-report-exec-fixed-20261113.png.

The 692 watchdog samples peak at 10,704 MiB combined VRAM and 2,453 MiB GTT,
with at least 13,087 MiB available RAM. No threshold event occurs, and RPCS3
remains running. guarded-alpha-game.log has zero renderer errors and critical
messages with game Vulkan validation disabled; samples are preserved in
guarded-alpha-memory.log.


Blood of the Dead repeated with the 6 GiB driver override completes gameplay,
a settled Richtofen report and return to lobby. A menu-input miss first launched
an extra Alpha Omega run; that run is labeled alpha-repeat and excluded from
Blood's coverage. Blood's tile and saved lobby selection were then inspected
before starting a fresh process. The verified Blood run reports a 2,278 MiB
usable GPU budget, rather than the earlier 487 MiB. A normal laboratory sample
is about 9 FPS and preserves reflective metal vessels, glass, the blue
blueprint, walls and weapon detail. The report and returned lobby retain
textured/lit clothing, skin and continuous hair. Report: 2 kills, 500 points,
round 1, 1m31s; no completed round is claimed.

The 698 watchdog samples peak at 8,874 MiB combined VRAM and 2,039 MiB GTT,
with at least 12,767 MiB available RAM. No threshold event occurs; RPCS3 remains
running. This supersedes the earlier budget-limited Blood completion result,
but does not establish long-session stability. guarded-blood-final-game.log
contains zero renderer errors and critical messages with game Vulkan validation
disabled. Evidence: guarded-blood-final-memory.log and
rendering-blood-budget-fixed-20261113.png,
rendering-blood-report-exec-fixed-20261113.png,
rendering-blood-lobby-exec-fixed-20261113.png.


The earlier Tag visual run used 397e8719, before the final ReadFirstLane fix;
its results alone cannot establish f4b2ac4c coverage. Tag is therefore repeated
on the final renderer after visually checking its selected tile and lobby.
Docks gameplay shows detailed snow/ice, wooden structures, reflective water,
sunset lighting, blood decals and reflective weapon metal across movement and
combat. The settled report retains continuous hair and textured/lit fabric,
leather, skin and metal. Report: 3 kills, 1 headshot, 500 points, round 1, 1m36s;
no completed round is claimed. Gameplay samples are 5–6 FPS, report 9 FPS.
Screenshots: rendering-tag-exec-fixed-20261113.png and
rendering-tag-report-exec-fixed-20261113.png.

The 869 watchdog samples peak at 11,141 MiB combined VRAM and 2,083 MiB GTT,
with at least 16,487 MiB available RAM. No threshold event occurs; RPCS3 remains
running. guarded-tag-final-game.log contains zero renderer errors and critical
messages with game Vulkan validation disabled. Samples are preserved in
guarded-tag-final-memory.log. BO4 is then stopped intentionally to release its
memory before the final offline metadata/SPIR-V audit and small blend probe.


Final offline audit after the same-build Tag repeat: 2,213 current-generation
metadata records, zero invalid resource discoveries and zero skipped records.
All 3,339 dumped SPIR-V modules pass spirv-val for Vulkan 1.3 (older dumps are
included). Evidence: guarded-final-audit.log, guarded-final-spirv.log,
guarded-final-all-stages-audit.json and guarded-final-shader-validation.json.
These are resource-discovery/module-validity checks, not proof of every game
shader's visual output.

The final binary also reruns the original mrt_src1_probe with precise readbacks,
linear-image readback enabled, DMA enabled and Vulkan core/synchronization
validation enabled. Unblended four-target export, ordinary MRT2 source-alpha
and true MRT0 dual-source controls all pass. The four unknown higher-target
SRC1 cases still leave their measured target at (0.25,0.25,0.25,0.25), its
initialized value, and emit VUID-vkCmdDraw-maxFragmentDualSrcAttachments-09239.
No other VUID category or critical message appears in this run. SHADTEST PASS
means the controls passed and measurements were collected; it does not certify
the unknown cases. Evidence: guarded-final-mrt.log.

The first final-probe invocation mistakenly disabled linear-image readback,
which this CPU pixel oracle requires. All controls then read initialization
values. That configuration-invalid result is preserved separately as
guarded-final-mrt-readback-disabled.log and is not treated as a renderer
regression. The corrected invocation above uses the runner's required oracle
settings. No emulation code or blend-warning suppression was added.

Remaining correctness blocker: a trusted console measurement or authoritative
hardware specification is needed to establish the higher-MRT secondary-source
factor. The user has no PS4 available; invalid host Vulkan execution cannot
supply the missing guest-hardware result. The original standalone homebrew is
ready under hardware-tests/tests/mrt_src1_probe and in the local probe archive.
The all-eight-map matrix establishes sampled opening/report rendering only,
with low FPS, no completed round and no full-match stability certification.
RPCS3 was kept running. BO4 and the final diagnostic containers have exited.

## Performance investigation (2026-11-13, in progress)

The user accepted the sampled rendering but reported roughly 8 FPS and requested
playable performance. All performance runs retain the existing 1920x1080 game
rendering, DMA, precise readbacks and linear-image readback settings. They use Tag
der Toten with the same opening control script. The script can get downed and
then reach a post-match report; only the first 60 seconds after HUD detection are
measured. Character/enemy and streaming variation means these are diagnostic
runs, not a controlled comparative benchmark or full-match certification.

Host: Ryzen 9 9950X3D, Radeon RX 9070 XT, 96 GB RAM. RPCS3 was no longer present
when this investigation checked the process list; the agent did not stop it.
Other applications remain untouched. The existing memory guard remains enabled.
The advertised VRAM budget is normally 6 GiB. A 10 GiB trial did not establish a
useful improvement. Perf recording is isolated in a diagnostic container, and
report generation now runs after the game stops to reduce memory contention.

The original CPU profile spent approximately 73% of render-thread samples under
BuildImageTableSet. Each variant traversed 16,384 descriptors, with roughly 5,000
compatible 2D entries. Changes now cache compatible descriptors, descriptor keys
and live image handles, avoid repeated mapping lookups and LRU touches, and reuse
immutable descriptor sets while their dependencies remain unchanged. These
changes are generic; no title or shader hashes select the behavior.

Observed 60-second frame-counter averages (FPS): candidate caching 8.97; contiguous
sampling 11.97; repeated handle-cache trial 9.45; larger VRAM budget 12.12; cached
keys 11.37; dependency caching 11.80; narrower GPU invalidation 7.93. These runs do
not demonstrate a reliable playable improvement. The latest profile still spent
47.2% of render-thread CPU samples in BuildImageTableSet. A separate context-switch
profile frequently found synchronous GPU readback underneath CPU texture
fingerprinting. Context-switch sample counts are not percentages of elapsed time.

The next change samples direct-memory CPU backing for texture fingerprints, using
the existing sparse-memory copy helper. It avoids read faults that unnecessarily
synchronize GPU-owned buffer pages. Mapping/readability checks remain in place;
normal image and buffer coherence paths still handle GPU-written data. Build and
all 14 selected rendering probes passed; gameplay performance is being measured.
Three existing SYNC-HAZARD-WRITE-AFTER-PRESENT reports occurred in this probe run,
with no VUID reports. This is not a clean synchronization-validation claim.

The original OpenOrbis runtime_image_updates homebrew now also writes both test
textures through a second virtual mapping of the same direct memory, leaving the
descriptors pointed at the first mapping. The preserved starting executable fails
this case (expected 0x3f600000, observed 0x3f400000); the current executable passes
all six checks. There has been no real-console measurement.

Performance evidence is local under bo4-dev/out/performance, including frame-counter
JSON, perf recordings/reports, screenshots, preserved starting/intermediate
executables and regression logs. The unfinished task is to remove the remaining
bottlenecks, measure a repeatable improvement, and validate sustained gameplay and
representative rendering before claiming playability.

The direct-backing build completed its first gameplay sample at 12.07 FPS, with
61.97% render-thread CPU occupancy. Screenshots retain textured geometry, weapon
materials, lighting, shadows and HUD in the sampled scene. This remains below
playable performance; investigation continues. The executable and logs are saved
under the `backing` label.

A later expanded runtime_image_updates binary exposes a timing-dependent failure
in the physical_alias phase on the preserved 2089a007 executable as well as on
the sparse-backing-retention experiment. Both return 0x3f400000 instead of
0x3f600000. Thus the earlier six-case passing run is not evidence of a complete
physical-alias fix. The new remap_reuse phase passes: fresh physical memory at
the original texture addresses wins over the still-mapped old aliases. The other
13 selected rendering probes pass; three existing presentation synchronization
warnings recur. Periodic texture verification remains an incomplete substitute
for tracking writes through every physical alias.

An experiment retaining recently unmapped sparse-buffer backing for up to eight
frames, capped at 128 MiB of reclaimable ranges, measured 12.42 FPS versus 12.07
FPS in the preceding run. Allocation and sparse-binding costs remained prominent,
so this does not establish a useful gain. The code was removed and its patch saved
locally as out/performance/retention-experiment.patch. Temporary table/hash counters
are now being used to identify the remaining rebuild and identity-check costs.

Bulk direct-backing reads now traverse physical mapping segments rather than
falling back to a 64-byte sampling loop when a texture spans multiple VMAs. Full
verification includes the entire byte range. All 14 probes passed with no VUID
or synchronization reports in that run. Render-thread profiling reduced
SampleDirectBacking from 10.86% to 4.92% of CPU samples, but the gameplay average
was still only 10.33 FPS; the menu route needed manual recovery in this run, so
this is not a controlled before/after result.

Temporary counters exposed approximately 26,000 image-key ownership replacements
and 10 GB of full texture hashing per 120 table epochs in settled gameplay.
Representative replacements had identical nonzero content hashes, guest addresses
and byte sizes. Different descriptors for views of one unchanged image repeatedly
evicted each other, forcing up to three table-building passes and further uploads.

The new original OpenOrbis runtime_image_views probe samples red and swizzled-green
views of one allocation in the same dispatch, then changes pixels, swaps the
views and crosses SubmitDone. The saved 2089a007 binary returns zero for all 64
samples from the first view in each of the four phases. The proposed fix compares
allocation identity, verified content range and nonzero full hash before forcing
an image reload or replacing the previous content key. Distinct views of unchanged
content coexist. All five probe cases now pass, as do the other 14 probes, with
no core or synchronization reports in this run. Gameplay measurement is in
progress. Temporary diagnostic counters are excluded from production source.

The view-preservation build averaged 16.05 FPS over the first measured gameplay
minute (57.2% render-thread CPU occupancy, 59.1% mean GPU busy). Representative
settled intervals fell from roughly 26,000 replacements and 10 GB of full hashing
to around 600 replacements and 0.8 GB per 120 epochs. Later sampled frames showed
19 FPS with textured weapons, geometry, lighting, shadows and HUD. This is still
below the playability target. The production build without diagnostic logging
also passes all five runtime_image_views cases. Further optimization continues.

A 12 GiB advertised VRAM budget measured 15.88 FPS versus 16.05 FPS at 6 GiB,
with the aggregate 12,000 MiB guard unchanged. It did not establish a benefit,
so subsequent runs returned to 6 GiB.

Sparse backing allocations now have a bounded 128 MiB reuse pool keyed by exact
allocation size. Memory enters the pool only through the existing deferred
operation after the GPU timeline completes its unbinding submission. Live backing
reference counts and original allocation sizes are tracked separately. Pool
access is protected by a mutex, and excess allocations are freed normally.
The original buffer_mapping_lifetimes homebrew checks twelve mapping lifetimes,
all transformed output/source words and two guard regions per lifetime. It passes
on both the saved pre-pool binary and the pool build. All sixteen selected probes
pass at the default submission interval with no validation messages.

The pool's first gameplay minute measured 16.23 FPS, which does not establish an
FPS gain over 16.05. In the CPU profile, driver allocation/free samples fell from
about 13.7% to 9.9%. At the end, the pool held 123.2 MiB in 98 allocations while
8,418 backings remained live. The additional memory is bounded; the existing
aggregate memory guard remains unchanged.

A stress run submitting after every draw/dispatch passes fifteen probes with no
validation messages. runtime_image_updates again fails only physical_alias,
returning 0x3f400000 instead of 0x3f600000; its other phases, including remap_reuse,
pass. This is the previously reproduced timing-dependent alias limitation.
The next gameplay experiment uses the existing submit_draw_interval setting at
32 while preserving resolution and shader settings. The original configuration
is saved locally at out/performance/config-before-submission-tuning.json.

### Incremental image-table refresh (b21043ef follow-up)

Runtime image-load tables now preserve entries previously resolved to immutable
textures within the same frame and cache generations. Mutable images still update
on every use. Descriptor bytes are checked as before; CPU update, image-state,
registration, mapping and frame changes force a complete refresh. A retry after
content ownership changes also refreshes every entry.

The same Tag der Toten opening-minute script measured 18.00 FPS versus 16.23 for
the allocation-pool run. Rendering-thread CPU was 63.4%, GPU busy averaged 45.8%.
This is not a controlled identical-scene comparison: characters and downed states
vary. The final timed screenshot remained in gameplay; the later end screenshot
shows Game Over. The post-sample GDB refresh counts were collected after Game Over
and must not be treated as gameplay frequency evidence.

Fifteen of sixteen probes passed. runtime_image_updates again reproduced its
known baseline physical-alias CPU-write failure; initial, ordinary CPU update,
descriptor swap, submission boundary, remap reuse and guard cases passed.
The already-known swapchain WRITE_AFTER_PRESENT warning occurred in depth/shadow
probes. The expanded runtime_image_loads probe checks four unchanged-table reads
in one submission, including two writes to only one of its textures. All 512
output words and guards passed; its run also reported WRITE_AFTER_PRESENT.

A separate submit_draw_interval=32 experiment averaged 15.57 FPS and later
ended in a GPUVM fault/device loss. Its timing setting was restored to zero.
The fault has not been isolated to a specific code change.

### Per-image update revisions and selective remap invalidation

Immutable table entries now record the update revision of their backing image.
Partial refreshes retain entries whose images have not changed; registration,
image-state, descriptor, and frame changes still force a full refresh. CPU dirty
state is published before the corresponding revision. Unmapping unrelated GPU
buffers no longer invalidates all texture keys. Descriptor-table overlaps still
force a reread, and texture overlaps invalidate cached keys.

The runtime_image_updates homebrew now has ten cases, adding a CPU write to only
one of two textures, an unrelated mapping retirement, and descriptor-table backing
replacement at the same virtual address. All sixteen rendering probes passed
with this change. The existing WRITE_AFTER_PRESENT diagnostic appeared in
runtime_image_loads. A passing physical_alias case does not establish that the
previously reproduced timing-dependent alias bug is fixed.

The revisions-only opening run averaged 15.65 FPS. A moving run pinned to the
9950X3D cache cores averaged 13.27 FPS but fell into water and was downed. Neither
establishes playable combat. The flat-map experiment's 13.38 FPS measurement is
invalid because a debugger attachment overlapped the final seconds; it was
reverted. A 720p configuration experiment ran around 1.12 FPS and was reverted.
Current internal resolution is 1920x1080, submission interval zero, and shader
math settings are unchanged. Dark weapon silhouettes are visible intermittently
in captures from both before and after incremental table refreshes.

Frame-counter discovery now uses offline ELF symbol offsets plus /proc/PID/mem,
avoiding debugger pauses during timed samples. CPU perf sampling remains enabled.
Old automated movement routes can fall into water and are inadequate for judging
sustained combat. Further optimization and representative gameplay checks remain.

An offline replay of current-20261113.rdc completed within existing memory guards.
The sum of per-event GPU durations was 26.42 ms, spread across thousands of draws
and copies; no single expensive shader dominated. This older captured frame and
instrumented replay are not a current native frame-time measurement.

### Remap experiment rollback

The 868235c5 executable reached a missing-character lobby and deteriorated to
0.15 FPS over 20 seconds, with approximately 100% GPU busy and 10.5% rendering
thread CPU. The saved revisions-only executable, with identical settings,
rendered the lobby character at about 38 FPS and completed a gameplay run at
14.03 FPS. This is evidence against retaining the remap narrowing; its exact
failure mechanism has not been isolated. Restore conservative remap invalidation
and retain per-image update revisions. The new remap homebrew remains useful.
The control gameplay route again entered water and ended shortly after one
minute; subsequent comparisons will use IX rather than this route.

### Cached view validation and flat texture-table maps

A generation now records insertions, replacements and removals of resolved table
views. The final table scan is skipped only when the applicable validated view
generation and image lifetime generation remain unchanged. Resolving or removing
views invalidates that shortcut. The three hot texture-table maps now use the
existing Abseil flat hash map implementation; no references survive insertions
or rehashes. Conservative remap invalidation remains enabled.

All sixteen rendering probes pass. runtime_image_views now additionally replaces
the backing allocation of two channel views at the same virtual address and
repeats their unchanged-table read; all samples and guards pass. The known
WRITE_AFTER_PRESENT diagnostic appeared in runtime_image_loads. Build evidence:
viewvalid-build.log; probe summaries: viewvalid-regressions-summary.log and
viewvalid-extra-summary.log under the live harness out/performance directory.

The IX stationary firing sequence measured 19.52 FPS versus 9.92 for the saved
revisions-only control. This is not a controlled identical-frame comparison:
characters, zombie positions and downed timing differ. Both samples include
combat and downed states; the optimized sample ends before Game Over. Rendering
CPU averaged 57.8% versus 45.3%, GPU busy 51.7% versus 66.4%. CPU cycle profiles
attribute 17.47% to BuildImageTableSet versus 29.21%, and GetImageTableSet inclusive
28.71% versus 37.61%. These are sampled proportions, not exact frame-time savings.
The result remains below the playable-performance goal. Further GPU profiling
and moving combat validation are required.

### Direct image-read table layout revisions (checkpoint)

Registered images now expose their atomic update revision to layout transitions.
Direct read/depth tables refresh affected entries when a read-only image leaves
its sampling layout, and unchanged immutable tables avoid a redundant rebuild.
Mutable GPU images still refresh on use. Content-keyed sampled tables retain
conservative full refreshes on layout changes: applying the shortcut there
exposed the existing physical-alias write-detection weakness (two failures in
four candidate runs, versus three passing saved-control runs). That broader
shortcut is not included.

All sixteen probes pass after narrowing the change (states-safe-summary.log).
The original runtime_image_loads homebrew adds full/cropped/full/full reads with
numeric output and guard verification, passing on the old and new executables.
The saved executable is shadps4-states-safe. Gameplay performance for this exact
checkpoint has not yet been measured; the playable-performance goal remains open.

A separate O3/ThinLTO build with CPU Tracy disabled measured 16.20 FPS over the
same stationary IX workload, ending downed. This does not demonstrate an
improvement over the normal build; the normal build configuration is retained.
A current bright IX RenderDoc capture was saved, but replay exceeded the 6,000
MiB GTT guard. Older replays are not evidence for current GPU frame time.

A diagnostic found an earlier 6 GiB advertised-heap run reporting about 7.56 GiB
of device usage against a 3.87 GiB critical collection threshold. A larger
advertised-budget comparison is in progress, with aggregate memory guards
unchanged. No claim of playable performance is made from these diagnostics.

### Retired sparse backing pool eviction (checkpoint)

The exact-size retired-memory pool now evicts its oldest completed allocations
when admitting a new size would exceed 128 MiB. Size lookup and retirement order
use separate ordered indexes, avoiding a repeated linear scan when many small
allocations must be retired. Reuse still happens only after the unbind submission
completes. Pool capacity and memory guards are unchanged.

The original buffer_mapping_lifetimes homebrew now checks 44 lifetimes: twelve
64 KiB cases followed by two cycles from 1 to 16 MiB, with source/output/guard
checks. It passes on both the old pool and new pool. All sixteen rendering probes
pass (pool-lru-summary.log); build: pool-lru-order-build.log. The new binary is
saved as shadps4-pool-lru.

At the same 10 GiB advertised heap, IX stationary firing measured 19.58 FPS versus
17.10 for the saved 0ef31fab control. These runs include different characters,
enemy positions and downed timing, so they are not identical-frame comparisons.
The candidate also includes the preceding direct-read table layout change.
Sampled AllocateBacking/RecycleBacking CPU proportions fell from 8.92%/8.45%
to 1.54%/1.49%. Rendering CPU was 53.47% versus 52.47%; GPU busy 53.08% versus
57.66%. Playable sustained combat remains unproven and optimization continues.

The current bright IX capture replay completed its GPU counters with a 9 GiB
GTT cap, a stricter 30 GiB available-RAM floor, and unchanged 12 GiB VRAM / 28 GiB
container caps. It used approximately 8.3 GiB VRAM and 7.1 GiB GTT with over
50 GiB RAM available. Per-event durations total 23.91 ms: indirect draws 15.38 ms,
direct indexed draws 5.48 ms, buffer copies 1.56 ms. No single event exceeded
0.11 ms. This instrumented replay is not native frame time. Optional hot-shader
extraction was stopped after the counter data was saved; its output is partial.

After native timing, a 128-call GPU-drain audit found 101 CPU-requested buffer
readbacks, nine command-processor buffer reads, and nine each EOS/EOP image
readback drains. A later readback-address sample was taken during the transition
to the after-action report, so it must not be treated as a combat workload.

### Empty image lookup cache (checkpoint)

A 256-entry address cache remembers only lookups that found no overlapping image.
Every image registration advances its generation. Existing CPU-dirty images are
never recorded as absent, and positive lookups retain the existing validation.
The cache is used by the GPU command thread's buffer binding paths.

The original image_raw_buffer_copy homebrew now checks a CPU rewrite followed
by a GPU write to the same existing image, with small buffer copies and guards.
It passes before the change with linear readback both disabled and enabled;
all sixteen probes pass after the change (empty-summary.log). Build:
empty-build.log; saved binary: shadps4-empty. Native timing is in progress.

A separate 4 MiB readback-window experiment measured 18.45 FPS, versus 19.58 for
the preceding 512 KiB run, on the variable stationary IX workload. No improvement
was demonstrated; the original 512 KiB window is restored. Its saved binary and
profile are shadps4-readback4 and readback4-arena respectively.

The empty-lookup native run completed at 21.63 FPS (render CPU 57.9%, GPU busy
48.2%); its final thirty seconds averaged 27.5 FPS. FindImageForBuffer's sampled
CPU share fell from 11.17% to 4.77%. The workload remains variable and includes
downed/revive transitions; sustained playable combat is not established. A
post-timing readback-address diagnostic captured 128 drains across frames
6606–6618, with 60,731,456 copied bytes, while still in the IX match
(empty-readbacks.json and perf-empty-diagnostic.png).

### Copy descriptor arrays only after a change (checkpoint)

Image table refreshes now compare each resolved entry with the current set before
copying its key/view/layout arrays. When nothing changes, the existing set is
retained without bulk copies or full-array comparisons. All image resolution,
layout transitions, barriers and stale-view validation remain active. Descriptor
rewrites explicitly reset the arrays so removed entries cannot survive a rebuild;
unchanged descriptors can use the lazy path even during a full image refresh.

All sixteen rendering probes pass (lazy-table-summary.log), including repeated
mixed CPU/GPU image reads, shared views, remaps and descriptor rewrites. Build:
lazy-table-build.log; exact saved binary: shadps4-lazy-table. Native timing is in
progress. This is a checkpoint, not a claim that the performance goal is met.

The descriptor-copy candidate's opening IX run completed at 22.55 FPS.
BuildImageTableSet's sampled rendering CPU share fell from 22.27% to 13.08%;
GetImageTableSet fell from 34.14% to 26.78%. An additional 80-second moving arena
run averaged only 17.37 FPS; the player remained alive with visible textured
scenery. Thus the opening tunnel average overstates arena playability. A later
80-second arena run restricted to CPUs 0–7,16–23 averaged 16.34 FPS. Enemy state,
position and ammunition differed, so this is not an identical-scene A/B, but it
provides no demonstrated benefit from forcing cache-core affinity. Default
CPU placement remains the intended setting.

The original runtime_image_loads probe now also uses 8,192 descriptors and two
batches of 256 reads, swapping the two selected descriptors between batches.
All 65,536 selected output words and the surrounding guards pass on the saved
pre-copy-optimization executable, the current executable, and the current
executable with submit_draw_interval=256 (large-table-summary.log). The existing
WRITE_AFTER_PRESENT warning is still present in these runs; numerical PASS does
not mean validation is completely clean. Native interval-256 timing is in
progress, with the original interval-zero config backed up for restoration.

### Image-page occupancy shortcut (checkpoint)

A 512 KiB atomic bitmap now records which texture-registry pages contain images.
GPU buffer lookups skip the registry lock when their starting page has no image.
Registration sets the bit after inserting the page entry; unregistration clears
it only after removing that page's last image. Nonempty pages retain the exact
address cache and existing image selection. Out-of-range addresses fall back to
the existing lookup. This does not cache GPU-valid/dirty state as absence.

All sixteen probes pass, including the expanded large-table and CPU-then-GPU
raw-copy cases (page-bits-summary.log). Build: page-bits-build.log. Exact binary:
shadps4-page-bits. Native measurement is in progress with interval zero and
default CPU placement.

The interval-256 experiment regressed to 2.73 FPS, with rendering CPU 24.43% and
GPU busy 94.95%. It is rejected and the prior interval-zero configuration was
restored. The profile and post-timing wait diagnostic are retained under
submit256-arena; periodic submission is not a recommended setting.

The page-occupancy build averaged 18.63 FPS in the opening run.
FindImageForBuffer fell below the profile's 1% reporting threshold, but this
does not establish playable moving combat.

### Read-only DMA fragment early-depth checks (checkpoint)

DMA page-table/fault bookkeeping introduces storage writes even in otherwise
read-only guest fragment shaders. Such instrumentation can inhibit implicit
early depth rejection. Emit explicit EarlyFragmentTests only when the guest
requests early-capable depth order, depth is read-only, stencil is disabled,
and neither shader side effects nor discard/coverage/depth/sample/helper
semantics require later execution. Forced execution on hierarchical failure or
no-op draws is excluded. Only emulator-owned page-table and fault buffers are
exempted from the guest storage-write check. Shader cache versions are bumped.

The new original early_depth_dma OpenOrbis probe verifies seven cases: visible
and occluded read-only DMA, visible and occluded guest writes, depth-write,
discard and explicit late-depth controls. Baseline and candidate both pass;
all sixteen existing rendering probes also pass (early-depth-final-summary.log).
Generated SPIR-V confirms EarlyFragmentTests activates only for the eligible
read-only variant; guest-write, discard, depth-write and late-depth controls
retain late execution. No physical-console result is claimed. Build:
early-depth-final-build.log; exact binary: shadps4-early-depth-final. Native
performance and visual checks are pending, with the previous shader cache saved
in cache-before-early-depth. The performance goal remains open.
