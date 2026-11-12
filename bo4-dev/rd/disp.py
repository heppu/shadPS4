# Run inside qrenderdoc: list compute dispatches with their buffer/image bindings; RD_ARGS="lo,hi" (hex offsets) flags overlaps in big buffers.
import os, traceback
import renderdoc as rd
RDC = os.environ["RDC"]; OUT = os.environ.get("RD_OUT", "/out/rd")
log = open(os.path.join(OUT, "disp.txt"), "w")
def p(*a):
    log.write(" ".join(str(x) for x in a) + "\n"); log.flush()
def main():
    lo, hi = [int(v, 16) for v in os.environ["RD_ARGS"].split(",")]
    cap = rd.OpenCaptureFile(); cap.OpenFile(RDC, "", None)
    r, ctl = cap.OpenCapture(rd.ReplayOptions(), None)
    texs = {t.resourceId: t for t in ctl.GetTextures()}
    bufs = {b.resourceId: b for b in ctl.GetBuffers()}
    names = {d.resourceId: d.name for d in ctl.GetResources()}
    flat = []
    def walk(a):
        flat.append(a)
        for c in a.children:
            walk(c)
    for a in ctl.GetRootActions():
        walk(a)
    for a in flat:
        if not (a.flags & rd.ActionFlags.Dispatch):
            continue
        ctl.SetFrameEvent(a.eventId, True)
        st = ctl.GetPipelineState()
        line = ["eid", a.eventId, "dim", list(a.dispatchDimension), "cs", names.get(st.GetShader(rd.ShaderStage.Compute), "?").replace("Shader Module ", "sm")]
        for x in ctl.GetDescriptorAccess():
            if x.stage != rd.ShaderStage.Compute:
                continue
            rng = rd.DescriptorRange(); rng.offset = x.byteOffset; rng.descriptorSize = x.byteSize; rng.count = 1; rng.type = x.type
            d = ctl.GetDescriptors(x.descriptorStore, [rng])[0]
            t = texs.get(d.resource); b = bufs.get(d.resource)
            if t:
                addr = [w for w in names.get(d.resource, "").split() if w.startswith("0x")]
                line.append("T%d[%d]:%s:%dx%dx%d/%s" % (int(x.type), x.index, addr[0].split(":")[0] if addr else int(d.resource), t.width, t.height, max(t.depth, t.arraysize), t.format.Name()))
            elif b:
                flag = "*HIT*" if (b.length > (1 << 30) and d.byteOffset < hi and d.byteOffset + d.byteSize > lo) else ""
                line.append("B%d[%d]:%d+%x:%x%s" % (int(x.type), x.index, int(d.resource), d.byteOffset, d.byteSize, flag))
        p(*line)
    ctl.Shutdown(); cap.Shutdown()
try:
    main()
except Exception:
    p(traceback.format_exc())
log.close(); os._exit(0)
