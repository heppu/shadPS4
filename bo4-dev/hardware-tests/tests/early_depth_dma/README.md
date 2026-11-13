# Read-only DMA fragment depth tests

Original OpenOrbis homebrew using handwritten gfx700 shaders. A scalar loop
reads eight words through a guest pointer while an ordinary bound buffer read
keeps the same small source resident. The shader emits known RGBA values.

The probe initializes depth to 0.5 and compares equal (pass) and less (reject),
with depth writes and stencil disabled. A second shader performs a real guest
buffer store under late depth testing; the store must happen even when the
fragment fails depth. All color pixels, the store and surrounding guards are
checked. No console results are claimed until it is run on real hardware.

Additional controls enable depth writing or discard every fragment. Shader
dumps allow checking that these controls and the real guest-store shader retain
late tests when the read-only DMA optimization is enabled.

The read-only and discard shaders request the guest early/late Z order. The
real-store shader and an additional read-only control explicitly request late Z.
The late-only control must retain late tests even with read-only depth.
