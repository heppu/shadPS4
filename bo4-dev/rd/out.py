# Run inside qrenderdoc: RD_ARGS="eid|x,y;x,y". Prints colour blend and write mask state of the draw and the
# value of each bound colour target at the pixels before and after it.
import os, traceback
import renderdoc as rd
RDC = os.environ["RDC"]; OUT = os.environ.get("RD_OUT", "/out/rd")
EID, PTS = os.environ["RD_ARGS"].split("|"); EID = int(EID)
PTS = [tuple(int(v) for v in pt.split(",")) for pt in PTS.split(";")]
log = open(os.path.join(OUT, "out.txt"), "w")
def p(*a):
    log.write(" ".join(str(x) for x in a) + "\n"); log.flush()
def main():
    cap = rd.OpenCaptureFile(); cap.OpenFile(RDC, "", None)
    r, ctl = cap.OpenCapture(rd.ReplayOptions(), None)
    texs = {t.resourceId: t for t in ctl.GetTextures()}
    ctl.SetFrameEvent(EID, True)
    st = ctl.GetPipelineState()
    outs = [o for o in st.GetOutputTargets()]
    for i, b in enumerate(st.GetColorBlends()):
        if i < len(outs):
            t = texs.get(outs[i].resource)
            p("rt", i, int(outs[i].resource), t.format.Name() if t else None, "blend", b.enabled, "mask", b.writeMask,
              "color", str(b.colorBlend.source), str(b.colorBlend.destination), str(b.colorBlend.operation))
    vk = ctl.GetVulkanPipelineState()
    p("rasterizer discard", vk.rasterizer.rasterizerDiscardEnable, "cull", str(vk.rasterizer.cullMode), "depthTest", vk.depthStencil.depthTestEnable, "depthWrite", vk.depthStencil.depthWriteEnable, "func", str(vk.depthStencil.depthFunction), "stencil", vk.depthStencil.stencilTestEnable)
    rp = vk.currentPass
    p("renderpass color attachments", [int(a.resource) for a in rp.framebuffer.attachments][:8])
    for when, eid in (("before", EID - 1), ("after", EID)):
        ctl.SetFrameEvent(eid, True)
        for (x, y) in PTS:
            vals = []
            for o in outs:
                if o.resource == rd.ResourceId.Null():
                    continue
                v = ctl.PickPixel(o.resource, x, y, rd.Subresource(0, 0, 0), rd.CompType.Typeless)
                vals.append("%d:(%s)" % (int(o.resource), ",".join("%.3f" % c for c in v.floatValue)))
            p(when, x, y, " ".join(vals))
    ctl.Shutdown(); cap.Shutdown()
try:
    main()
except Exception:
    p(traceback.format_exc())
log.close(); os._exit(0)
