# Run inside qrenderdoc: RD_ARGS="eid:texid,texid;eid:texid" saves textures at events and prints min/max and a few pixels.
import os, traceback
import renderdoc as rd
RDC = os.environ["RDC"]; OUT = os.environ.get("RD_OUT", "/out/rd")
log = open(os.path.join(OUT, "tex.txt"), "w")
def p(*a):
    log.write(" ".join(str(x) for x in a) + "\n"); log.flush()
def main():
    cap = rd.OpenCaptureFile(); cap.OpenFile(RDC, "", None)
    r, ctl = cap.OpenCapture(rd.ReplayOptions(), None)
    texs = {int(t.resourceId): t for t in ctl.GetTextures()}
    for part in os.environ["RD_ARGS"].split(";"):
        eid, ids = part.split(":")
        ctl.SetFrameEvent(int(eid), True)
        for tid in ids.split(","):
            t = texs[int(tid)]
            sub = rd.Subresource(0, 0, 0)
            mn, mx = ctl.GetMinMax(t.resourceId, sub, rd.CompType.Typeless)
            p("eid", eid, "tex", tid, t.format.Name(), "%dx%d" % (t.width, t.height), "min", ["%.4g" % v for v in mn.floatValue], "max", ["%.4g" % v for v in mx.floatValue])
            for (x, y) in ((480, 540), (200, 500), (700, 300), (500, 900), (100, 100)):
                if x < t.width and y < t.height:
                    v = ctl.PickPixel(t.resourceId, x, y, sub, rd.CompType.Typeless)
                    p("   px", x, y, ["%.4g" % c for c in v.floatValue])
            save = rd.TextureSave(); save.resourceId = t.resourceId; save.destType = rd.FileType.PNG
            save.mip = 0; save.slice.sliceIndex = 0; save.alpha = rd.AlphaMapping.Discard
            save.channelExtract = -1
            save.comp.blackPoint = 0.0; save.comp.whitePoint = float(os.environ.get("RD_WHITE", "1.0"))
            ctl.SaveTexture(save, os.path.join(OUT, "t_e%s_%s.png" % (eid, tid)))
            try:
                import zlib, struct as st
                data = ctl.GetTextureData(t.resourceId, sub)
                bpp = len(data) // (t.width * t.height)
                if bpp == 4 and "R8G8B8A8" in t.format.Name():
                    rows = b"".join(b"\x00" + bytes(b for x in range(t.width) for b in data[(y * t.width + x) * 4:(y * t.width + x) * 4 + 3]) for y in range(t.height))
                    def ch(ty, b): return st.pack(">I", len(b)) + ty + b + st.pack(">I", zlib.crc32(ty + b) & 0xffffffff)
                    open(os.path.join(OUT, "raw_e%s_%s.png" % (eid, tid)), "wb").write(b"\x89PNG\r\n\x1a\n" + ch(b"IHDR", st.pack(">IIBBBBB", t.width, t.height, 8, 2, 0, 0, 0)) + ch(b"IDAT", zlib.compress(rows, 6)) + ch(b"IEND", b""))
            except Exception as e:
                p("raw dump failed", e)
            save.destType = rd.FileType.HDR if False else rd.FileType.PNG
    ctl.Shutdown(); cap.Shutdown()
try:
    main()
except Exception:
    p(traceback.format_exc())
log.close(); os._exit(0)
