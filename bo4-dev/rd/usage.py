# Run inside qrenderdoc: RD_ARGS=texid,texid prints which events use each resource and how.
import os, traceback, collections
import renderdoc as rd
RDC = os.environ["RDC"]; OUT = os.environ.get("RD_OUT", "/out/rd")
log = open(os.path.join(OUT, "usage.txt"), "w")
def p(*a):
    log.write(" ".join(str(x) for x in a) + "\n"); log.flush()
def main():
    cap = rd.OpenCaptureFile(); cap.OpenFile(RDC, "", None)
    r, ctl = cap.OpenCapture(rd.ReplayOptions(), None)
    ids = {int(d.resourceId): d for d in ctl.GetResources()}
    texs = {int(t.resourceId): t for t in ctl.GetTextures()}
    for tid in [int(v) for v in os.environ["RD_ARGS"].split(",")]:
        d = ids[tid]; t = texs.get(tid)
        p("=== res", tid, d.name, ("%dx%dx%d arr %d %s" % (t.width, t.height, t.depth, t.arraysize, t.format.Name())) if t else "")
        us = ctl.GetUsage(d.resourceId)
        c = collections.OrderedDict()
        for u in us:
            k = str(u.usage)
            c.setdefault(k, []).append(u.eventId)
        for k, v in c.items():
            p("  ", k, "n", len(v), "events", v[:14], "..." if len(v) > 14 else "")
    ctl.Shutdown(); cap.Shutdown()
try:
    main()
except Exception:
    p(traceback.format_exc())
log.close(); os._exit(0)
