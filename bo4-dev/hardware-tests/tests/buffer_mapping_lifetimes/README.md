# Buffer mapping lifetimes

Original OpenOrbis homebrew with original GCN assembly. Across forty-four direct-memory
lifetimes, the GPU copies and transforms a source into a separate destination.
The first twelve use 64 KiB regions, then two cycles use sizes from 1 to 16 MiB. The CPU checks every source/output word and both equally sized guard ranges.
Each lifetime uses a different pattern. The next mapping is created before the
previous one is unmapped and released, so it cannot immediately reuse the same
live virtual range. Completed GPU backing can be recycled across later mappings
without leaking bytes or dirty state from an earlier lifetime.

Build with `make -C tests/buffer_mapping_lifetimes`. Run with
`python3 scripts/run-test.py buffer_mapping_lifetimes --shadps4 /path/to/shadps4 --timeout 120`.
The ELF is `out/buffer_mapping_lifetimes/buffer_mapping_lifetimes.elf`. Physical PS4
execution has not been performed.

The varied sizes exceed the emulator's 128 MiB retired-backing pool budget,
exercising eviction and later reuse while checking every byte represented by a word.
