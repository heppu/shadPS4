# Shader variant reuse

Original OpenOrbis compute probe using one handwritten gfx700 program with 24 distinct source/destination strides. It creates variants, repeats the latest 48 times, then switches in a nonsequential order for 72 rounds. Every output, untouched word, trailing guard and source word is checked. Run both cold and warm shader caches. No physical PS4 result is claimed.

Each round submits 64 dispatches. The elapsed marker excludes the first 24 warmup rounds. Completion uses bounded pause polling with the same load fence as the shared helper. The original helper sleeps for 1 ms, which masked sub-millisecond savings: its approximately 135 ms results must not be used to conclude that shader lookup costs are identical.

Three corrected alternating pairs measured 35.293 ms control and 35.317 ms with last-variant lookup. No speedup was established, and all values and guards passed. These are emulator workload times, not gameplay FPS or physical-console results.
