# Runtime image views

Original OpenOrbis homebrew with original GCN assembly. Two runtime-table
entries refer to the same RGBA32F allocation. One returns its red channel and
the other swizzles green into red. Both views must remain valid in the same
dispatch. Subsequent phases change the shared pixels, swap the descriptors,
and cross a SubmitDone boundary. Every phase checks all 128 samples and 16
guard words. Guest cache invalidation precedes each dispatch.

Build with `make -C tests/runtime_image_views`. Run with
`python3 scripts/run-test.py runtime_image_views --shadps4 /path/to/shadps4 --timeout 60`.
The ELF is `out/runtime_image_views/runtime_image_views.elf`. No physical PS4
execution has been performed.
