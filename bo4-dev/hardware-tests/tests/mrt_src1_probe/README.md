# MRT secondary-source blend probe

Independent OpenOrbis homebrew with original GCN assembly. It measures SRC1_ALPHA
on MRT1–3, and ONE_MINUS_SRC1_ALPHA on MRT2, while MRT0 uses ordinary output.
These cases are deliberately measurements, not assertions of undocumented PS4
behavior. The current emulator emits Vulkan 09239 for them. Do not treat emulator
measurements as a physical-console oracle.

Three controls verify every pixel: unblended four-target exports, ordinary source
alpha blending on MRT2, and genuine dual-source blending on MRT0 with two exports.
All controls use exact binary fractions and linear 64×128 RGBA32F targets initialized
to (0.25, 0.25, 0.25, 0.25). The four shader colors are:

| Export | RGBA |
| --- | --- |
| MRT0 | 0.125, 0.25, 0.375, 0.25 |
| MRT1 | 0.25, 0.375, 0.5, 0.75 |
| MRT2 | 0.375, 0.5, 0.625, 0.5 |
| MRT3 | 0.5, 0.625, 0.75, 0.125 |

Blending uses source ONE and the measured destination factor; a separate alpha
operation preserves destination alpha. `MEASURE` lines report center-pixel floats
and bit patterns. `SHADTEST PASS` means the controls passed and measurements were
collected; it does not certify the unknown blend cases.

Build with the surrounding hardware-tests toolchain:

```sh
make -C tests/mrt_src1_probe
python3 scripts/run-test.py mrt_src1_probe --shadps4 /path/to/shadps4 --timeout 40
```

Keep the runner's precise readbacks and linear-image readback enabled for the
emulator run. Disabling linear-image readback makes this CPU pixel oracle read
the initialized targets, including the three known controls.

Run `out/mrt_src1_probe/mrt_src1_probe.elf` using an existing homebrew ELF loader
on a PS4 and retain its stdout, including all MEASURE and SHADTEST lines. It needs
no game files or external shader assets. Physical-console results are pending.

The blend-control packet is written directly because the bundled OpenGNM helper
adds a byte offset instead of a four-byte register stride for MRT indices above
zero. The completion packet uses a non-interrupting cache-flush EOP.

Local emulator validation: controls pass. The four unknown cases leave their
selected target at the initialized value on RADV GFX1201 and trigger 09239;
that invalid Vulkan execution cannot establish guest hardware behavior.
