import gdb, os

def rd(a, n):
    return bytes(gdb.selected_inferior().read_memory(a, n))

class Peek(gdb.Command):
    def __init__(self):
        super().__init__("peek", gdb.COMMAND_USER)
    def invoke(self, arg, from_tty):
        for spec in os.environ.get("PEEK", "").split():
            a, _, n = spec.partition(":")
            deref = a.startswith("*")
            a = int(a.lstrip("*"), 16); n = int(n or 8)
            try:
                if deref:
                    p = int.from_bytes(rd(a, 8), "little")
                    print("PEEK *%#x -> %#x: %s" % (a, p, rd(p, n).hex(" ") if p else "null"))
                else:
                    print("PEEK %#x: %s" % (a, rd(a, n).hex(" ")))
            except gdb.MemoryError as e:
                print("PEEK %#x: %s" % (a, e))
Peek()

class PeekPy(gdb.Command):
    def __init__(self):
        super().__init__("peekpy", gdb.COMMAND_USER)
    def invoke(self, arg, from_tty):
        f = os.environ.get("PEEKPY")
        if f:
            exec(open(f).read(), {"rd": rd, "gdb": gdb})
PeekPy()
