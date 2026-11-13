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
