#!/usr/bin/env python3
"""usage: xref.py <string-or-0xaddr>...  find rip-relative references in out/eboot.mem (base 0x400000)"""
import sys, re, mmap, struct
BASE = 0x400000
f = open('/mnt/games/PS4/bo4-dev/out/eboot.mem', 'rb')
m = mmap.mmap(f.fileno(), 0, access=mmap.ACCESS_READ)
targets = {}
for a in sys.argv[1:]:
    if a.startswith('0x'):
        targets[int(a, 16)] = a
    else:
        for mt in re.finditer(re.escape(a.encode()) + b'\0', m):
            s = mt.start()
            while s > 0 and m[s-1] != 0: s -= 1
            targets[BASE + s] = a
for t, n in targets.items(): print('target %#x %s' % (t, n))
CODE_END = 0x1c00000 - BASE
pat = re.compile(rb'[\x48\x4c]\x8d[\x05\x0d\x15\x1d\x25\x2d\x35\x3d]|[\x48\x4c]\x8b[\x05\x0d\x15\x1d\x25\x2d\x35\x3d]')
for mt in pat.finditer(m, 0, CODE_END):
    p = mt.start()
    disp = struct.unpack_from('<i', m, p + 3)[0]
    tgt = BASE + p + 7 + disp
    if tgt in targets:
        print('ref %#x -> %#x %s' % (BASE + p, tgt, targets[tgt]))
