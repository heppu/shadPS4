# Sampled depth with an active stencil attachment

Original OpenOrbis / GCN homebrew. Initialize D32/S8 to depth 0.5 and stencil
0x35, then sample depth while depth/stencil equality tests use the same image.
Repeated draws must return RGBA (0.5, 0.25, 0.375, 1) for every pixel. Two phases
check all pixels after CPU color-buffer resets. No physical-console result is
available.

Build: `make -C tests/depth_sample_attachment` from hardware-tests.
Run: `python3 scripts/run-test.py depth_sample_attachment --shadps4 PATH
--set Vulkan.vkvalidation_enabled=true --set Vulkan.vkvalidation_core_enabled=true
--set Vulkan.vkvalidation_sync_enabled=true` (one command).

Baseline c73ade90 produces correct pixels but reports
SYNC-HAZARD-WRITE-AFTER-WRITE on a depth image layout transition. The renderer
queued dependent transitions for the same image into one barrier command.
Flushing the earlier transitions before queuing another transition for the same
image removes the hazard; both pixel cases still pass. The version with stencil
disabled was a clean control. Related stencil-storage, mip/subrectangle and
mixed-volume probes pass with zero core/synchronization validation reports.
