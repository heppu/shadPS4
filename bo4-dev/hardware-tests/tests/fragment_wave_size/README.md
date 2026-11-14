# Fragment wave size

Original OpenOrbis homebrew with handwritten gfx700 shaders. One fragment shader
writes known constant RGBA values without observing wave state. Another exports
the lane index produced by both halves of v_mbcnt. Every color channel is checked;
lane values must be integral in 0..63 and the rendered image must include lanes
32..63. The checks do not assume a hardware-specific screen-to-lane mapping.
No physical-console measurements are claimed.
