# Run inside qrenderdoc: RD_ARGS="eid|stride|value". Lists rows of the pixel stage material buffer that contain the value.
import os, struct, traceback
import renderdoc as rd
RDC = os.environ["RDC"]; OUT = os.environ.get("RD_OUT", "/out/rd")
EID, STRIDE, VAL = [int(v) for v in os.environ["RD_ARGS"].split("|")]
log = open(os.path.join(OUT, "findrow.txt"), "w")
def p(*a):
    log.write(" ".join(str(x) for x in a) + "\n"); log.flush()
def isf(v): return 0x38000000 <= (v & 0x7fffffff) <= 0x47000000
def main():
    cap = rd.OpenCaptureFile(); cap.OpenFile(RDC, "", None)
    r, ctl = cap.OpenCapture(rd.ReplayOptions(), None)
    ctl.SetFrameEvent(EID, True)
    for x in ctl.GetDescriptorAccess():
        if x.stage != rd.ShaderStage.Pixel or int(x.type) != 9 or x.index != 0:
            continue
        rng = rd.DescriptorRange(); rng.offset = x.byteOffset; rng.descriptorSize = x.byteSize; rng.count = 1; rng.type = x.type
        d = ctl.GetDescriptors(x.descriptorStore, [rng])[0]
        data = ctl.GetBufferData(d.resource, d.byteOffset, d.byteSize)
        dws = struct.unpack("<%dI" % (len(data) // 4), data)
        n = STRIDE // 4
        hits = [i for i, v in enumerate(dws) if v == VAL]
        p("value", VAL, "found at dwords", hits[:40])
        for i in hits[:12]:
            r0 = i // n
            row = dws[r0 * n: r0 * n + n]
            p(" row", r0, "pos", i % n, " ".join(("%.3g" % struct.unpack("<f", struct.pack("<I", v))[0]) if isf(v) else ("%x" % v) for v in row))
        frows = [r0 for r0 in range(len(dws) // n) if any(isf(v) for v in dws[r0 * n:r0 * n + n])]
        p("rows with floats", len(frows), frows[:40])
        for r0 in frows[:10]:
            row = dws[r0 * n: r0 * n + n]
            p(" frow", r0, " ".join(("%.3g" % struct.unpack("<f", struct.pack("<I", v))[0]) if isf(v) else ("%x" % v) for v in row))
        # try other strides: where do floats sit relative to value hits (raw dword neighbourhood)
        for i in hits[:6]:
            p(" around", i, " ".join(("%.3g" % struct.unpack("<f", struct.pack("<I", v))[0]) if isf(v) else ("%x" % v) for v in dws[max(0, i - 4): i + 20]))
    ctl.Shutdown(); cap.Shutdown()
try:
    main()
except Exception:
    p(traceback.format_exc())
log.close(); os._exit(0)
