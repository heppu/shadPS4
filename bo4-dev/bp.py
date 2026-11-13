import gdb, os

LO, HI = 0x400000, 0x1c00000

def chain(n=10):
    inf = gdb.selected_inferior()
    rsp = int(gdb.parse_and_eval("$rsp"))
    out = []
    mem = inf.read_memory(rsp, 0x800)
    for i in range(0, 0x800, 8):
        v = int.from_bytes(bytes(mem[i:i+8]), "little")
        if LO <= v < HI:
            out.append("%#x" % v)
            if len(out) >= n:
                break
    return out

class Bp(gdb.Breakpoint):
    def __init__(self, spec, limit):
        super().__init__(spec)
        self.limit = limit
        self.n = 0
    def stop(self):
        self.n += 1
        if self.n <= self.limit:
            print("BPHIT %s #%d thr=%s rdi=%#x rsi=%#x rdx=%#x rcx=%#x chain=%s" % (
                self.location, self.n, gdb.selected_thread().name,
                int(gdb.parse_and_eval("$rdi")), int(gdb.parse_and_eval("$rsi")),
                int(gdb.parse_and_eval("$rdx")), int(gdb.parse_and_eval("$rcx")), " ".join(chain())))
        return False

for spec in os.environ.get("BPS", "").split():
    name, _, lim = spec.partition("@")
    Bp(name, int(lim or 5))

class GuestBp(gdb.Breakpoint):
    def __init__(self, addr, regs, limit, cond=None):
        super().__init__("*%#x" % addr)
        self.regs = regs; self.limit = limit; self.n = 0
        self.cond = cond
    def stop(self):
        if self.cond and not int(gdb.parse_and_eval(self.cond)):
            return False
        self.n += 1
        if self.n <= self.limit:
            out = []
            for r in self.regs:
                if r.startswith("@"):
                    expr, _, n = r[1:].partition("/")
                    a = int(gdb.parse_and_eval(expr)) & 0xFFFFFFFFFFFFFFFF
                    try:
                        out.append("%s@%#x=[%s]" % (expr, a, bytes(gdb.selected_inferior().read_memory(a, int(n or 64))).hex()))
                    except gdb.MemoryError:
                        out.append("%s@%#x=unreadable" % (expr, a))
                else:
                    out.append("%s=%#x" % (r, int(gdb.parse_and_eval("$" + r)) & 0xFFFFFFFFFFFFFFFF))
            print("GBP %s #%d thr=%s %s" % (self.location, self.n, gdb.selected_thread().name, " ".join(out)))
        return False

class ArmGuest(gdb.Breakpoint):
    def stop(self):
        for spec in os.environ.get("GBPS", "").split():
            parts = spec.split(":")
            GuestBp(int(parts[0], 16), parts[1].split(","), int(parts[2]) if len(parts) > 2 else 20, parts[3] if len(parts) > 3 else None)
        self.enabled = False
        return False

if os.environ.get("GBPS"):
    ArmGuest("Libraries::PlayGo::scePlayGoOpen")

class ArmPatch(gdb.Breakpoint):
    def stop(self):
        inf = gdb.selected_inferior()
        for spec in os.environ.get("PATCH", "").split():
            a, _, hx = spec.partition(":")
            inf.write_memory(int(a, 16), bytes.fromhex(hx))
            print("PATCHED %s %s" % (a, hx))
        self.enabled = False
        return False

if os.environ.get("PATCH"):
    ArmPatch("Libraries::PlayGo::scePlayGoGetChunkId")
