import gdb

SLOT = 0x7efcd3c88
BAD = 0xbfbeaaaaaa

class Arm(gdb.Breakpoint):
    def stop(self):
        if gdb.selected_thread().name != "jobqueue CORE 0":
            return False
        gdb.execute("watch *(unsigned long*)%#x if *(unsigned long*)%#x == %#x" % (SLOT, SLOT, BAD))
        self.enabled = False
        return False

Arm("Libraries::Kernel::sceKernelCreateEqueue")
