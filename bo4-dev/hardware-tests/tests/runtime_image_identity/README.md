# Runtime image identity

Independent OpenOrbis homebrew with original GCN assembly. Two separate 64x64
RGBA32F texture allocations start with the same nonuniform pixel gradient.
A dynamically indexed image sample reads both. After GPU completion, the CPU
changes only the first allocation to 0.5. Sampling the second must retain the
original gradient value at the sampled coordinate (0.251953125). Further phases
swap the descriptor entries and cross a SubmitDone boundary. Sixteen guard words
are checked for every phase. Explicit guest cache invalidation precedes sampling.

Baseline 011768cb passes initial values and guards but fails the other three
phases, with 64 wrong samples each: the cache shares one backing image between
independent allocations. Preserving the allocation address in the content key
passes all five cases with core and synchronization validation and zero reports.
Physical-console execution is unavailable.

```sh
make -C tests/runtime_image_identity
python3 scripts/run-test.py runtime_image_identity --shadps4 /path/to/shadps4 --timeout 40
```

Load `out/runtime_image_identity/runtime_image_identity.elf` with an existing
PS4 homebrew ELF loader to obtain physical-console measurements.
