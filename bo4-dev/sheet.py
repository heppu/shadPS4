#!/usr/bin/env python3
# usage: sheet.py out.png in1.png in2.png ...   half-size contact sheet, 4 per row (pure python, no deps)
import sys, zlib, struct

def read_png(path):
    d = open(path, "rb").read()
    pos = 8; idat = b""
    while pos < len(d):
        n, typ = struct.unpack(">I4s", d[pos:pos + 8]); body = d[pos + 8:pos + 8 + n]; pos += 12 + n
        if typ == b"IHDR":
            w, h, depth, ctype = struct.unpack(">IIBB", body[:10])
        elif typ == b"IDAT":
            idat += body
    bpp = {2: 3, 6: 4}[ctype]
    raw = zlib.decompress(idat); stride = w * bpp
    rows = []; prev = bytearray(stride); p = 0
    for _ in range(h):
        f = raw[p]; line = bytearray(raw[p + 1:p + 1 + stride]); p += 1 + stride
        if f == 1:
            for i in range(bpp, stride): line[i] = (line[i] + line[i - bpp]) & 255
        elif f == 2:
            for i in range(stride): line[i] = (line[i] + prev[i]) & 255
        elif f == 3:
            for i in range(stride): line[i] = (line[i] + (((line[i - bpp] if i >= bpp else 0) + prev[i]) >> 1)) & 255
        elif f == 4:
            for i in range(stride):
                a = line[i - bpp] if i >= bpp else 0; b = prev[i]; c = prev[i - bpp] if i >= bpp else 0
                pa = abs(b - c); pb = abs(a - c); pc = abs(a + b - 2 * c)
                line[i] = (line[i] + (a if pa <= pb and pa <= pc else b if pb <= pc else c)) & 255
        rows.append(line); prev = line
    return w, h, bpp, rows

def main():
    out = sys.argv[1]; tiles = []
    for path in sys.argv[2:]:
        w, h, bpp, rows = read_png(path)
        tiles.append((w // 2, h // 2, [bytes(b for x in range(0, w - 1, 2) for b in rows[y][x * bpp:x * bpp + 3]) for y in range(0, h - 1, 2)]))
    tw, th = tiles[0][0], tiles[0][1]; cols = 4; nrows = (len(tiles) + cols - 1) // cols
    data = bytearray()
    for r in range(nrows):
        for y in range(th):
            data.append(0)
            for c in range(cols):
                i = r * cols + c
                data += tiles[i][2][y] if i < len(tiles) else bytes(tw * 3)
    def chunk(t, b): return struct.pack(">I", len(b)) + t + b + struct.pack(">I", zlib.crc32(t + b) & 0xffffffff)
    open(out, "wb").write(b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", tw * cols, th * nrows, 8, 2, 0, 0, 0)) + chunk(b"IDAT", zlib.compress(bytes(data), 6)) + chunk(b"IEND", b""))
main()
