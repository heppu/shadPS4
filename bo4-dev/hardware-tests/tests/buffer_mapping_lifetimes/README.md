# Buffer mapping lifetimes

Original OpenOrbis homebrew with original GCN assembly. Across twelve direct-memory
lifetimes, the GPU copies and transforms a 64 KiB source into a separate 64 KiB
destination. The CPU checks every source/output word and both 64 KiB guard ranges.
Each lifetime uses a different pattern. The next mapping is created before the
previous one is unmapped and released, so it cannot immediately reuse the same
live virtual range. Completed GPU backing can be recycled across later mappings
without leaking bytes or dirty state from an earlier lifetime.

Build with `make -C tests/buffer_mapping_lifetimes`. Run with
`python3 scripts/run-test.py buffer_mapping_lifetimes --shadps4 /path/to/shadps4 --timeout 60`.
The ELF is `out/buffer_mapping_lifetimes/buffer_mapping_lifetimes.elf`. Physical PS4
execution has not been performed.
