import struct, collections
for name, a in (("A_34", 0x243a34a100), ("B_28", 0x243a28a100), ("C_40", 0x243a40a100)):
    try:
        data = rd(a, 0xc0000)
    except Exception as e:
        print("PEEKGRID", name, "err", e); continue
    dw = struct.unpack("<%dI" % (len(data) // 4), data)
    nz = [(i, v) for i, v in enumerate(dw) if v]
    c = collections.Counter(v for _, v in nz)
    print("PEEKGRID", name, "dwords", len(dw), "nonzero", len(nz), "first", [(i, hex(v)) for i, v in nz[:8]], "top", [(hex(k), n) for k, n in c.most_common(6)])
