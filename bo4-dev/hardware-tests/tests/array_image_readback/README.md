# Array image readback

Original OpenOrbis homebrew writes 4,096-layer R32_UINT and R32_FLOAT images in
linear and microtiled layouts. Two completed submissions write distinct bit
patterns; the CPU verifies every valid texel and allocation guards. A 33-layer
5x7 image additionally checks partial compute workgroups and padded row pitches.
Microtiled layers use a uniform value per layer, allowing exact checks without
reproducing the emulator's tiling implementation. Linear images vary each texel.
Physical-console validation is pending; no PS4 is available.

The fixture sizes one layer and multiplies by the array allocation count because
the pinned OpenGnm size helper multiplies array depth twice. The descriptor's
last array index is explicitly inclusive (layers minus one).

A third format, RGBA8_UINT, checks that the R32_UINT host view preserves packed
channel bytes. Integer patterns exercise high bits as well as individual texels.

Packed 10_11_11 floating-point storage writes 1.0 and 2.0 in separate submissions,
checking their exact packed bit patterns in a microtiled array.
