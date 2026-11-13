# Idle presentation control

Original OpenOrbis homebrew that starts, sleeps two seconds, and emits PASS.
It performs no guest GPU work. shadPS4 continues presenting its host UI during
the delay, covering swapchain reuse without sparse buffer bindings.

Build: `make -C tests/presentation_idle`. Run through `scripts/run-test.py` with
core and synchronization validation enabled. The inspected run contains 121
host presents and zero Vulkan validation errors. The delay is a duration, not
a guarantee of an exact frame count. No physical-console result is claimed.
