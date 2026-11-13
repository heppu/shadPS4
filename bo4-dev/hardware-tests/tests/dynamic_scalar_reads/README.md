# Dynamic scalar reads

Original OpenOrbis / GCN homebrew. A scalar loop loads eight words through a
pointer with a loop-carried offset and sums 100 through 107 (828). All 64 lanes
store that sum. Ordinary buffer reads of the same 256-byte allocation provide
64 control values; sixteen output guard words must remain unchanged.

Build with `make -C tests/dynamic_scalar_reads` from hardware-tests. Run with
`scripts/run-test.py dynamic_scalar_reads --shadps4 PATH --set GPU.direct_memory_access_enabled=true`.
The ELF can also be run by an OpenOrbis-compatible homebrew loader on PS4.
No physical-console result is available.

Baseline: fallback disabled gives a wrong sum; enabled still gives zero because
the small read-only source is streamed without a guest-address mapping. Enlarging
the source to 64 KiB makes the existing enabled path pass, isolating residency.
The fix keeps bound buffers resident when their shader uses DMA. The original
256-byte case then passes on its first dispatch with core and synchronization
validation and no reports. Runtime buffer-table regression also passes.

This probe covers a pointer alias of an explicitly bound buffer. It does not
establish correctness for arbitrary unbound pointers or cold page faults.
