# Run inside qrenderdoc: RD_ARGS="hexaddr,hexaddr,...". For images whose name carries that guest address, print usage counts.
import os, traceback, collections
import renderdoc as rd
RDC = os.environ["RDC"]; OUT = os.environ.get("RD_OUT", "/out/rd")
ADDRS = os.environ["RD_ARGS"].split(",")
log = open(os.path.join(OUT, "texuse.txt"), "w")
def p(*a):
    log.write(" ".join(str(x) for x in a) + "\n"); log.flush()
def main():
    cap = rd.OpenCaptureFile(); cap.OpenFile(RDC, "", None)
    r, ctl = cap.OpenCapture(rd.ReplayOptions(), None)
    res = ctl.GetResources()
    for a in ADDRS:
        found = [d for d in res if ("0x%s:" % a) in d.name]
        if not found:
            p(a, "no image with this address in the capture"); continue
        for d in found:
            c = collections.Counter(str(u.usage) for u in ctl.GetUsage(d.resourceId))
            evs = [u.eventId for u in ctl.GetUsage(d.resourceId) if str(u.usage) == "17"][:6]
            p(a, d.name[:60], dict(c), "ps events", evs)
    ctl.Shutdown(); cap.Shutdown()
try:
    main()
except Exception:
    p(traceback.format_exc())
log.close(); os._exit(0)
