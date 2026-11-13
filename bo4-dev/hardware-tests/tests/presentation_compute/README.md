# Compute followed by host presentation

Original OpenOrbis homebrew derived from `dynamic_scalar_reads`. It checks the
same 64 scalar-loop sums (828), 64 ordinary buffer reads, and 16 guard words,
then stays alive for two seconds before its PASS marker. This gives shadPS4
time to reuse each host swapchain image after guest compute work.

Build: `make -C tests/presentation_compute`. Run:

```sh
python3 scripts/run-test.py presentation_compute --shadps4 PATH \
  --set GPU.direct_memory_access_enabled=true \
  --set Vulkan.vkvalidation_enabled=true \
  --set Vulkan.vkvalidation_core_enabled=true \
  --set Vulkan.vkvalidation_sync_enabled=true
```

The shader output passes on 8686f232. Both tested Vulkan validation layers
(1.4.363 and 1.4.368) report WRITE_AFTER_PRESENT after the first sparse timeline
wait. A standalone native reproduction isolates missing `vkQueueBindSparse`
timeline signal tracking in the validator; see `diagnostics/sparse-present`.
PASS means guest values are correct, not that the validation log is clean.

On PS4 the ELF can run through an OpenOrbis-compatible loader; it has no
console presentation output and only checks shader results and guards.
No physical-console result is claimed.
