# Run inside qrenderdoc: RD_ARGS="eid|lo|hi". Dumps the (offset, value) patch list of a patch dispatch and the dest range lo..hi before/after.
import os, struct, traceback, collections
import renderdoc as rd
RDC = os.environ["RDC"]; OUT = os.environ.get("RD_OUT", "/out/rd")
EID, LO, HI = [int(v) for v in os.environ["RD_ARGS"].split("|")]
log = open(os.path.join(OUT, "pairs.txt"), "w")
def p(*a):
    log.write(" ".join(str(x) for x in a) + "\n"); log.flush()
def main():
    cap = rd.OpenCaptureFile(); cap.OpenFile(RDC, "", None)
    r, ctl = cap.OpenCapture(rd.ReplayOptions(), None)
    ctl.SetFrameEvent(EID, True)
    acc = [x for x in ctl.GetDescriptorAccess() if x.stage == rd.ShaderStage.Compute and int(x.type) == 9]
    ds = {}
    for x in acc:
        rng = rd.DescriptorRange(); rng.offset = x.byteOffset; rng.descriptorSize = x.byteSize; rng.count = 1; rng.type = x.type
        ds[x.index] = ctl.GetDescriptors(x.descriptorStore, [rng])[0]
    def rd32(i, when):
        ctl.SetFrameEvent(EID - 1 if when == "before" else EID, True)
        d = ds[i]; data = ctl.GetBufferData(d.resource, d.byteOffset, d.byteSize)
        return struct.unpack("<%dI" % (len(data) // 4), data)
    count = rd32(0, "before")[0]
    src = rd32(1, "before"); b0 = rd32(2, "before"); b1 = rd32(2, "after")
    pairs = [(src[2 * i], src[2 * i + 1]) for i in range(count)]
    p("count", count, "dest dwords", len(b0), "changed by dispatch", sum(1 for a, b in zip(b0, b1) if a != b))
    offs = collections.Counter(o for o, v in pairs)
    p("distinct offsets", len(offs), "dup offsets", sum(1 for o, c in offs.items() if c > 1), "oob", sum(1 for o, v in pairs if o >= len(b0)))
    p("pairs already equal in dest before", sum(1 for o, v in pairs if o < len(b0) and b0[o] == v), "equal after", sum(1 for o, v in pairs if o < len(b1) and b1[o] == v))
    p("float vals", sum(1 for o, v in pairs if 0x38000000 <= (v & 0x7fffffff) <= 0x47000000), "max val", max(v for o, v in pairs), "zero vals", sum(1 for o, v in pairs if v == 0))
    tail = [(i, src[2 * i], src[2 * i + 1]) for i in range(count, min(count + 12, len(src) // 2))]
    p("after count:", " ".join("%x:%x" % (o, v) for i, o, v in tail))
    p("nonzero pairs beyond count", sum(1 for i in range(count, len(src) // 2) if src[2 * i] or src[2 * i + 1]))
    for i, (o, v) in enumerate(pairs):
        if LO <= o < HI:
            p(" pair", i, "off", o, "(+%d)" % (o - LO), "val %x" % v, "ctx", " ".join("%x:%x" % q for q in pairs[max(0, i - 4): i + 5]))
    p("dest before", " ".join("%x" % v for v in b0[LO:HI])); p("dest after ", " ".join("%x" % v for v in b1[LO:HI]))
    written = set(offs)
    p("nonzero dest dwords", sum(1 for v in b1 if v), "of which in this patch list", sum(1 for i, v in enumerate(b1) if v and i in written))
    p("first 60 pairs", " ".join("%x:%x" % q for q in pairs[:60]))
    p("last 60 pairs", " ".join("%x:%x" % q for q in pairs[-60:]))
    ctl.Shutdown(); cap.Shutdown()
try:
    main()
except Exception:
    p(traceback.format_exc())
log.close(); os._exit(0)
