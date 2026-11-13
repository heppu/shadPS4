# WAIT_REG_MEM coherence

Original OpenOrbis/OpenGNM workload, 512 rounds. A compute dispatch writes 256 bytes on a page containing a separate CPU synchronization word. A masked WAIT_REG_MEM checks that CPU word, then a second dispatch consumes the GPU-written bytes. Another WAIT_REG_MEM checks an actual GPU-written word. The oracle verifies both GPU outputs and the untouched label/guard values.

This covers precise synchronization-word reads without requiring unrelated bytes on the protected page to be downloaded. A GPU-written poll target must still become current before its comparison succeeds. It does not assert an emulator-specific wait count or speed threshold.

Build: `make -C tests/wait_reg_mem_coherence`. Run: `python3 scripts/run-test.py wait_reg_mem_coherence --shadps4 /path/to/shadps4 --timeout 120 --set GPU.direct_memory_access_enabled=true`. The original ELF is shareable for homebrew PS4 validation; no physical-console result is available.

The result includes workload elapsed microseconds as diagnostic data; it is not a pass threshold.
