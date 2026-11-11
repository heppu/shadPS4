import gdb

class GuestFrame(gdb.Command):
    """Select the frame interrupted by the signal handler and dump guest state."""
    def __init__(self):
        super().__init__("guestframe", gdb.COMMAND_USER)

    def invoke(self, arg, from_tty):
        f = gdb.newest_frame()
        while f is not None and f.type() != gdb.SIGTRAMP_FRAME:
            f = f.older()
        if f is None or f.older() is None:
            print("no signal frame on this thread")
            return
        f.older().select()
        print("\n==== GUEST STATE AT FAULT ====")
        gdb.execute("info registers")
        rsp = int(gdb.parse_and_eval("$rsp")) & ~0xfff
        inf = gdb.selected_inferior()
        with open("/out/stack.txt", "w") as o:
            for page in range(rsp - 0x40000, rsp + 0x10000, 0x1000):
                try:
                    data = bytes(inf.read_memory(page, 0x1000))
                except gdb.MemoryError:
                    o.write("%x unmapped\n" % page)
                    continue
                for i in range(0, 0x1000, 8):
                    o.write("%x %016x\n" % (page + i, int.from_bytes(data[i:i+8], "little")))
        import os
        if not os.path.exists("/out/eboot.mem"):
            with open("/out/eboot.mem", "wb") as o:
                for page in range(0x400000, 0x400000 + 0xd494000, 0x100000):
                    try:
                        o.write(bytes(inf.read_memory(page, 0x100000)))
                    except gdb.MemoryError:
                        o.write(b"\0" * 0x100000)
        for cmd in ("x/8i $pc", "x/24gx $rsp-0x40"):
            try:
                gdb.execute(cmd)
            except gdb.error as e:
                print(cmd, "->", e)

GuestFrame()
