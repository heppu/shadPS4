# Runtime image updates

Independent OpenOrbis homebrew with original GCN assembly. Samples two 2D textures
through a runtime descriptor table across multiple submissions before SubmitDone.
After the first completion label, CPU writes change both texture values; after
the second, CPU swaps the descriptor entries. GPU cache invalidation accompanies
each dispatch through the shared barrier helper. Each phase checks all 128 values
and 16 guard words. A final phase repeats after SubmitDone. Physical PS4 execution
is unavailable. On baseline db4f1bfa, initial reads, guards and the SubmitDone control pass,
but both same-frame update phases fail all 128 output words. With ACQUIRE_MEM
invalidation of runtime image-table snapshots and image synchronization state,
all five cases pass with core and synchronization validation and zero reports.

```sh
make -C tests/runtime_image_updates
python3 scripts/run-test.py runtime_image_updates --shadps4 /path/to/shadps4 --timeout 40
```

The ELF is `out/runtime_image_updates/runtime_image_updates.elf`. No video flip
is needed. Both CPU writes and descriptor replacement occur only after the
prior GPU completion label; cache invalidation precedes each new dispatch.

The physical-alias phase maps the arena a second time before any draw, then
writes both textures through that previously unused mapping. After SubmitDone,
the unchanged descriptors still address the first mapping and must sample the
new values (0.625 and 0.875). This checks CPU-backing verification without
replacing the original same-frame CPU-update and descriptor-swap controls.

The `remap_reuse` phase unmaps both sampled ranges, maps newly allocated direct
memory at the same virtual addresses, and gives the new memory different pixels.
It also overwrites the still-mapped old physical aliases with another pattern.
Sampling must use the replacement memory, with no stale GPU backing or old-alias
contents leaking across the mapping lifetime.
