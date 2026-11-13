#!/usr/bin/env python3
"""usage: xr.py 0xaddr[-0xend]...  find any rip-relative disp32 reference into the given addresses/ranges (out/eboot.mem, base 0x400000)"""
import sys
from array import array
BASE, CODE_END = 0x400000, 0x1c60000
n = CODE_END - BASE
buf = open('/mnt/games/PS4/bo4-dev/out/eboot.mem', 'rb').read(n + 16)
rng = []
for a in sys.argv[1:]:
    lo, _, hi = a.partition('-')
    lo = int(lo, 16); rng.append((lo, int(hi, 16) if hi else lo))
LO = min(r[0] for r in rng) - 8; HI = max(r[1] for r in rng)
out = []
for off in range(4):
    arr = array('i'); arr.frombytes(buf[off:off + (n // 4) * 4])
    p = BASE + off + 4
    for d in arr:
        t = p + d
        if LO <= t <= HI:
            for k in (0, 1, 4):
                for lo, hi in rng:
                    if lo <= t + k <= hi:
                        i = p - 4 - BASE
                        out.append((p - 4, k, t + k, buf[i-3:i+4+k].hex()))
        p += 4
for a, k, t, h in sorted(out):
    print('disp@%#x imm%d -> %#x  bytes %s' % (a, k, t, h))
