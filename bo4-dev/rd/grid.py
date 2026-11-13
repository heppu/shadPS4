# Run inside qrenderdoc: find compute dispatches writing 30x17x256 integer grids, dump shader and per-slice stats.
import os, struct, traceback, collections
import renderdoc as rd
RDC = os.environ["RDC"]; OUT = os.environ.get("RD_OUT", "/out/rd")
log = open(os.path.join(OUT, "grid.txt"), "w")
def p(*a):
    log.write(" ".join(str(x) for x in a) + "\n"); log.flush()
def main():
    cap = rd.OpenCaptureFile(); cap.OpenFile(RDC, "", None)
    r, ctl = cap.OpenCapture(rd.ReplayOptions(), None)
    texs = {t.resourceId: t for t in ctl.GetTextures()}
    names = {d.resourceId: d.name for d in ctl.GetResources()}
    grids = [t for t in texs.values() if t.width == 30 and t.height == 17 and t.arraysize == 256]
    p("grids", [(int(t.resourceId), names.get(t.resourceId)) for t in grids])
    flat = []
    def walk(a):
        flat.append(a)
        for c in a.children:
            walk(c)
    for a in ctl.GetRootActions():
        walk(a)
    last = flat[-1].eventId
    done = set()
    for g in grids:
        for u in ctl.GetUsage(g.resourceId):
            if u.usage == rd.ResourceUsage.CS_RWResource and (int(g.resourceId), u.eventId) not in done:
                done.add((int(g.resourceId), u.eventId))
                ctl.SetFrameEvent(u.eventId, True)
                st = ctl.GetPipelineState()
                refl = st.GetShaderReflection(rd.ShaderStage.Compute)
                fn = "cs_e%d.spv" % u.eventId
                open(os.path.join(OUT, fn), "wb").write(bytes(refl.rawBytes))
                act = [a for a in flat if a.eventId == u.eventId]
                p("write grid", int(g.resourceId), "eid", u.eventId, "dim", list(act[0].dispatchDimension) if act else None, "threads", list(refl.dispatchThreadsDimension), fn)
    ctl.SetFrameEvent(last, True)
    for g in grids:
        tot = 0; per = []; vals = collections.Counter()
        for s in range(256):
            data = ctl.GetTextureData(g.resourceId, rd.Subresource(0, s, 0))
            dw = struct.unpack("<%dI" % (len(data) // 4), data)
            nz = sum(1 for v in dw if v)
            tot += nz; per.append(nz)
            for v in dw:
                if v: vals[v] += 1
        p("grid", int(g.resourceId), names.get(g.resourceId, "")[-40:], "nonzero texels", tot, "of", 30 * 17 * 256, "slices with data", sum(1 for n in per if n), "first slices", per[:24], "top values", [(hex(k), n) for k, n in vals.most_common(8)])
        # one slice as text map
        s = max(range(256), key=lambda i: per[i])
        data = ctl.GetTextureData(g.resourceId, rd.Subresource(0, s, 0))
        dw = struct.unpack("<%dI" % (len(data) // 4), data)
        p(" busiest slice", s)
        for y in range(17):
            p("  ", " ".join("%4x" % dw[y * 30 + x] for x in range(30)))
    ctl.Shutdown(); cap.Shutdown()
try:
    main()
except Exception:
    p(traceback.format_exc())
log.close(); os._exit(0)
