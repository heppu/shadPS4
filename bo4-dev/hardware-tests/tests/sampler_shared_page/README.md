# Runtime sampler table sharing a protected page

Original OpenOrbis homebrew with handwritten gfx700 shaders. A runtime sampler
table starts 64 bytes after a GPU-written buffer, within the same 4 KiB page.
Wrap and clamp samplers select different known halves of a texture. Further
phases replace both samplers through a GPU buffer copy and through CPU writes.
Every sample, output guard, neighboring GPU word and unused prefix word is checked.
This distinguishes avoiding false page-sharing readbacks from skipping genuine
GPU-generated sampler descriptors. No physical-console results are claimed.
