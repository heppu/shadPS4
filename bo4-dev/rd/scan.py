# Run inside qrenderdoc: lists render passes of a capture and saves each pass's first colour target.
import os, sys, traceback
import renderdoc as rd

RDC = os.environ["RDC"]
OUT = os.environ.get("RD_OUT", "/out/rd")
os.makedirs(OUT, exist_ok=True)
log = open(os.path.join(OUT, "scan.txt"), "w")

def p(*a):
    log.write(" ".join(str(x) for x in a) + "\n"); log.flush()

def main():
    cap = rd.OpenCaptureFile()
    r = cap.OpenFile(RDC, "", None)
    if r != rd.ResultCode.Succeeded:
        p("open failed", r); return
    r, ctl = cap.OpenCapture(rd.ReplayOptions(), None)
    if r != rd.ResultCode.Succeeded:
        p("replay failed", r); return
    texs = {t.resourceId: t for t in ctl.GetTextures()}
    flat = []
    def walk(a):
        flat.append(a)
        for c in a.children:
            walk(c)
    for a in ctl.GetRootActions():
        walk(a)
    passes = []
    for a in flat:
        if not (a.flags & (rd.ActionFlags.Drawcall | rd.ActionFlags.Dispatch | rd.ActionFlags.Clear | rd.ActionFlags.Copy | rd.ActionFlags.Resolve)):
            continue
        key = (tuple(int(o) for o in a.outputs if o != rd.ResourceId.Null()), int(a.depthOut),
               bool(a.flags & rd.ActionFlags.Dispatch))
        if passes and passes[-1]["key"] == key:
            passes[-1]["last"] = a.eventId; passes[-1]["n"] += 1
        else:
            passes.append({"key": key, "first": a.eventId, "last": a.eventId, "n": 1, "outs": list(a.outputs), "depth": a.depthOut})
    p("actions", len(flat), "passes", len(passes))
    names = {d.resourceId: d.name for d in ctl.GetResources()}
    def desc(rid):
        t = texs.get(rid)
        if t is None:
            return "none"
        addr = [w for w in names.get(rid, "").split() if w.startswith("0x")]
        return "%s:%dx%dx%d/%s@%s" % (int(rid), t.width, t.height, t.arraysize, t.format.Name(), addr[0] if addr else "?")
    for i, ps in enumerate(passes):
        outs = [desc(o) for o in ps["outs"] if o != rd.ResourceId.Null()]
        p(i, "eid", ps["first"], ps["last"], "n", ps["n"], "disp" if ps["key"][2] else "draw", "out", outs, "depth", desc(ps["depth"]))
        if outs and ps["n"] >= 1 and os.environ.get("RD_SAVE", "1") == "1":
            ctl.SetFrameEvent(ps["last"], True)
            save = rd.TextureSave()
            save.resourceId = [o for o in ps["outs"] if o != rd.ResourceId.Null()][0]
            save.destType = rd.FileType.PNG
            save.mip = 0; save.slice.sliceIndex = 0
            save.alpha = rd.AlphaMapping.Discard
            ctl.SaveTexture(save, os.path.join(OUT, "pass%03d_e%d.png" % (i, ps["last"])))
    ctl.Shutdown(); cap.Shutdown()

try:
    main()
except Exception:
    p(traceback.format_exc())
log.close()
os._exit(0)
