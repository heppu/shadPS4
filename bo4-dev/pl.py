import struct
def q(a): return struct.unpack('<Q', rd(a, 8))[0]
def d(a): return struct.unpack('<i', rd(a, 4))[0]
def s(a, n=64):
    try:
        b = rd(a, n); return b.split(b'\0')[0].decode('latin1')
    except Exception as e: return '?'
A = 0x23ce6b0370
try:
    print("PL hdr", rd(A, 0x60).hex())
    cats, n = q(A + 0x40), d(A + 0x48)
    print("PL cats %#x n=%d" % (cats, n))
    for i in range(min(n, 8)):
        c = cats + i * 0x50
        ents, m = q(c + 0x40), d(c + 0x48)
        print("PL cat %d raw=%s" % (i, rd(c, 0x50).hex()))
        for j in range(min(m, 40)):
            e = ents + j * 0x138
            print("PL  ent %d id=%d raw=%s" % (j, d(e + 0x10c), rd(e, 0x138).hex()))
except Exception as ex:
    print("PL error", ex)
