# Concurrent buffer readbacks

Original OpenOrbis/OpenGNM workload with four guest threads and 48 rounds. A compute shader writes separate buffer ranges spaced one MiB apart. After an end-of-pipe label, four threads simultaneously access those ranges: two read first and two overwrite the first word before reading the rest. A second GPU copy checks that each CPU edit is uploaded, and adjacent sentinel words must survive.

The oracle checks data visibility, preservation of untouched GPU-written words, CPU upload coherence, and repeated readback lifetimes. It does not require the emulator to batch any particular number of requests or assert a timing threshold. The existing buffer_mapping_lifetimes probe covers mapping retirement separately.

Build with `make -C tests/buffer_readback_threads`. Run with `python3 scripts/run-test.py buffer_readback_threads --shadps4 /path/to/shadps4 --timeout 120 --set GPU.direct_memory_access_enabled=true`. The ELF also runs on a homebrew-capable PS4; no physical-console result is available here.
