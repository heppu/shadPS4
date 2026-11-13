# Run inside qrenderdoc: RD_ARGS=eid. Dumps the compute shader of a dispatch and the head of each buffer it binds.
import os, struct, traceback
import renderdoc as rd
RDC = os.environ["RDC"]; OUT = os.environ.get("RD_OUT", "/out/rd")
EID = int(os.environ["RD_ARGS"])
log = open(os.path.join(OUT, "cs.txt"), "w")
def p(*a):
    log.write(" ".join(str(x) for x in a) + "\n"); log.flush()
def main():
    cap = rd.OpenCaptureFile(); cap.OpenFile(RDC, "", None)
    r, ctl = cap.OpenCapture(rd.ReplayOptions(), None)
    bufs = {b.resourceId: b for b in ctl.GetBuffers()}
    for when, eid in (("before", EID - 1), ("after", EID)):
        ctl.SetFrameEvent(eid, True)
        if when == "after":
            st = ctl.GetPipelineState()
            refl = st.GetShaderReflection(rd.ShaderStage.Compute)
            open(os.path.join(OUT, "cs_e%d.spv" % EID), "wb").write(bytes(refl.rawBytes))
            p("threads", list(refl.dispatchThreadsDimension))
        ctl.SetFrameEvent(EID, True) if when == "after" else None
        ctl.SetFrameEvent(EID, True)
        acc = [x for x in ctl.GetDescriptorAccess() if x.stage == rd.ShaderStage.Compute]
        ctl.SetFrameEvent(eid, True)
        for x in acc:
            rng = rd.DescriptorRange(); rng.offset = x.byteOffset; rng.descriptorSize = x.byteSize; rng.count = 1; rng.type = x.type
            ctl.SetFrameEvent(EID, True)
            d = ctl.GetDescriptors(x.descriptorStore, [rng])[0]
            ctl.SetFrameEvent(eid, True)
            if d.resource not in bufs:
                continue
            n = min(d.byteSize, 1 << 21)
            data = ctl.GetBufferData(d.resource, d.byteOffset, n)
            dw = struct.unpack("<%dI" % (len(data) // 4), data[: len(data) // 4 * 4])
            floaty = sum(1 for v in dw if 0x38000000 <= (v & 0x7fffffff) <= 0x47000000)
            p(when, "buf type", int(x.type), "idx", x.index, "off", hex(d.byteOffset), "size", hex(d.byteSize), "nonzero", sum(1 for v in dw if v), "float-like", floaty)
            for r0 in (0, 16, 32, 48, 1024, 1545, 1560):
                if r0 * 4 < len(data):
                    p("    dw%5d:" % r0, " ".join("%x" % v for v in dw[r0:r0 + 16]))
    ctl.Shutdown(); cap.Shutdown()
try:
    main()
except Exception:
    p(traceback.format_exc())
log.close(); os._exit(0)
