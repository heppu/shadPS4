# Runtime image table sharing a protected page

Original OpenOrbis homebrew with handwritten gfx700 shaders. A runtime image
descriptor table starts 64 bytes beyond a GPU-written buffer on the same 4 KiB
page. Two descriptor swizzles select known red/green texture components. Further
phases swap the descriptors through GPU writes and restore them through CPU writes.
All 384 samples, output guards, neighboring GPU words and unused prefix words are
checked. No physical-console results are claimed.
