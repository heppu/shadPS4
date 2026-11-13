# Image table lifetimes

Original OpenOrbis homebrew with original GCN assembly. A two-entry sampled-image
table is bound, then an independent allocation is sampled through another table
first with one mip and then with two. Returning to the unchanged original table
must retain the original gradient. A subsequent CPU overwrite must become visible.
All phases verify exact sampled values and sixteen untouched guard words.

The growing foreign image exercises replacement of a cached image while another
image table remains valid. No remapping or alias change is needed between these
bindings. Physical-console validation is pending; no PS4 is available.

The final phase changes only the second descriptor to the foreign image; the
first must retain its CPU-updated value while the changed entry resolves anew.

The saved pre-change subgroup-safe build and the retained-entry candidate both
pass all seven cases with Vulkan core/synchronization validation. GDB confirms
that foreign mip expansion advances image generation from 0 to 1 and that the
surviving table is rebuilt with full=False/static=True on the candidate.

The cache-growth phase inserts 32 distinct, constant-valued textures through
separate tables, then reuses the original table. Exact samples and guards check
that references survive map growth while the changed original descriptor and
CPU-updated texture remain correct.

The expanded eight-case fixture passes on both the saved layered-copy build and
the experimental reference-cache build. The latter optimization was removed
after native profiling failed to demonstrate a gain.
