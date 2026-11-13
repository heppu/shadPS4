# EOP image ordering

Original image and raw-buffer workloads followed by EOP, WAIT_REG_MEM on its label, then another EOP in the same command buffer. Checks forward progress, final label and image/buffer contents with repeated command-buffer reuse. Run with linear image readbacks enabled. This is an emulator regression oracle; no physical PS4 result is available.
