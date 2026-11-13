# Vertex subgroup size

Original OpenOrbis homebrew; no game assets or proprietary SDK code.

Draws 64 one-pixel points into a linear RGBA32F target. The independent vertex
shader places each point using its vertex index. The lane-sensitive shader uses
mbcnt lane identity for the horizontal position, requiring all 64 columns to
remain reachable. Both phases verify exact color and untouched surrounding rows.
A 32-lane replacement for the lane-sensitive shader leaves the upper 32 columns
unwritten. Physical-console validation is pending; no PS4 is available.

A third phase renders 64 adjacent one-pixel quads. The quad tessellation path must
remain coherent with its vertex stage: allowing only that vertex stage to vary
caused 768 mismatched color words on the candidate, while the saved pre-change
build passed. The corrected pipeline retains 64 lanes for linked pre-raster stages.

The vertex wave32 experiment was rejected after native performance regressed.
The retained emulator path uses the existing 64-lane vertex behavior. The probe
also caught an invalid Flat decoration on the vertex SubgroupLtMask input, which
is fixed independently of subgroup-size selection.
