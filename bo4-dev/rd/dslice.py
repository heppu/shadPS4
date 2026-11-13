# Run inside qrenderdoc: RD_ARGS="texid;eid,eid,..." prints the depth attachment view per event and per-slice min/max at the last event.
import os, traceback
import renderdoc as rd
RDC = os.environ["RDC"]; OUT = os.environ.get("RD_OUT", "/out/rd")
log = open(os.path.join(OUT, "dslice.txt"), "w")
def p(*a):
    log.write(" ".join(str(x) for x in a) + "\n"); log.flush()
def main():
    cap = rd.OpenCaptureFile(); cap.OpenFile(RDC, "", None)
    r, ctl = cap.OpenCapture(rd.ReplayOptions(), None)
    tid, eids = os.environ["RD_ARGS"].split(";")
    texs = {int(t.resourceId): t for t in ctl.GetTextures()}
    t = texs[int(tid)]
    eids = [int(e) for e in eids.split(",")]
    for eid in eids:
        ctl.SetFrameEvent(eid, True)
        st = ctl.GetPipelineState()
        d = st.GetDepthTarget()
        vp = st.GetViewport(0); sc = st.GetScissor(0)
        mm = []
        for s in range(t.arraysize):
            mn, mx = ctl.GetMinMax(t.resourceId, rd.Subresource(0, s, 0), rd.CompType.Typeless)
            mm.append("%.4g..%.4g" % (mn.floatValue[0], mx.floatValue[0]))
        p("eid", eid, "depth res", int(d.resource), "firstSlice", d.firstSlice, "numSlices", d.numSlices, "vp", "%.0f,%.0f %.0fx%.0f z %.3g..%.3g" % (vp.x, vp.y, vp.width, vp.height, vp.minDepth, vp.maxDepth),
          "sc", sc.x, sc.y, sc.width, sc.height, "slices", mm)
    for s in range(t.arraysize):
        save = rd.TextureSave(); save.resourceId = t.resourceId; save.destType = rd.FileType.PNG
        save.mip = 0; save.slice.sliceIndex = s; save.alpha = rd.AlphaMapping.Discard
        ctl.SaveTexture(save, os.path.join(OUT, "dslice_%s_s%d.png" % (tid, s)))
    ctl.Shutdown(); cap.Shutdown()
try:
    main()
except Exception:
    p(traceback.format_exc())
log.close(); os._exit(0)
