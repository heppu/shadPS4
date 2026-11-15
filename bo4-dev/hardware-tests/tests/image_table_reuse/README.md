# Repeated image table sampling

Original OpenOrbis homebrew derived from the project's first-use image probe,
using original GCN assembly. A 4,096-entry table names 64 independently allocated
RGBA32F textures. Every dispatch samples each entry with 64 lanes. All 262,144
result words and 16 guards are checked in each of 132 rounds.

The first four rounds are warmup. `measured_us` sums submission-to-completion time
for the remaining 128 rounds, excluding CPU output initialization and validation.
Completion uses bounded pause polling with a load fence; no 1-ms sleep. The timer
includes queue/GPU/driver costs and is not gameplay FPS or pure lookup time.

Build with `make -C tests/image_table_reuse`, then run via
`python3 scripts/run-test.py image_table_reuse --shadps4 /path/to/shadps4 --timeout 120`.
Physical-console execution is unavailable; retain output/guard checks when comparing builds.
