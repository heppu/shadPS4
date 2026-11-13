# Mipmapped subrectangle texture reproducer

An OpenOrbis homebrew with original GFX7 assembly, containing no commercial-game
code or assets. A compute shader writes distinct values to all nine mips of a
256×128 R32_FLOAT texture with a 512-texel pitch. Another shader queries dimensions
and loads texels through aliases sharing that allocation and pitch:

- 128×128, four mip levels;
- 128×128, eight mip levels;
- 64×128, eight mip levels;
- the original 256×128, nine mip levels.

The CPU checks each requested mip's width, height and texel, plus untouched output
guards. A second round of GPU writes checks that cached cropped views refresh.
The four-level and eight-level aliases also check that backing lookup distinguishes
mip counts at identical dimensions. Writes and reads are separated by cache
barriers and end-of-pipe completion.

Expected output:

```text
SHADTEST name=mip_subrect status=PASS views=4 phases=2 mip_contents=1 dimensions=1 guards=1
```

On 2026-11-13, the unmodified subrectangle eligibility check failed the first
width assertion: expected 128, observed 256. With the mipmapped subrectangle fix,
all assertions pass. Vulkan validation reported no image/copy/view errors in this
test; it did report the pre-existing sparse-buffer size limit error 06409.
**This homebrew has not been run on a physical PS4.**

Build using the pinned dependencies in `../../deps.lock`:

```sh
make -C tests/mip_subrect \
  TOOLCHAIN=/path/to/OpenOrbis/PS4Toolchain \
  OPENGNM=/path/to/built/opengnm
```

Requires LLVM's `clang`, `ld.lld`, `llvm-mc`, `llvm-objcopy`, and Python 3. Output:
`out/mip_subrect/mip_subrect.elf`. Load that ELF with a PS4 homebrew loader and
inspect stdout/kernel debug output. Under shadPS4, enable GPU readbacks and use
the SHADTEST marker as the result; this branch can trap during guest exit after
the marker has been emitted.

The common guest helpers are from
[shadps4-open-test](https://github.com/kaaburgh/shadps4-open-test), under its MIT
license. The end-of-pipe label uses interrupt selection zero, supported by both
the hardware packet format and this emulator branch.
