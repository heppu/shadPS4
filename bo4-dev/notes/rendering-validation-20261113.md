# BO4 Zombies rendering validation — 2026-11-13

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
