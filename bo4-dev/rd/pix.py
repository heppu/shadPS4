# Run inside qrenderdoc: pixel history of RD_ARGS="x,y,texid,lasteid" then shader debug trace of the last draw writing it.
import os, traceback
import renderdoc as rd

RDC = os.environ["RDC"]
OUT = os.environ.get("RD_OUT", "/out/rd")
X, Y, TEX, LAST = [int(v) for v in os.environ["RD_ARGS"].split(",")]
log = open(os.path.join(OUT, "pix.txt"), "w")

def p(*a):
    log.write(" ".join(str(x) for x in a) + "\n"); log.flush()

def val(v):
    try:
        n = max(1, v.columns)
        if v.type in (rd.VarType.Float, rd.VarType.Half, rd.VarType.Double):
            return ["%.6g" % v.value.f32v[i] for i in range(n)]
        return [v.value.u32v[i] for i in range(n)]
    except Exception as e:
        return "?%s" % e

def main():
    cap = rd.OpenCaptureFile()
    cap.OpenFile(RDC, "", None)
    r, ctl = cap.OpenCapture(rd.ReplayOptions(), None)
    texid = None
    for t in ctl.GetTextures():
        if int(t.resourceId) == TEX:
            texid = t.resourceId
    ctl.SetFrameEvent(LAST, True)
    sub = rd.Subresource(0, 0, 0)
    hist = ctl.PixelHistory(texid, X, Y, sub, rd.CompType.Typeless)
    p("history", len(hist))
    target = None
    for h in hist:
        p(" eid", h.eventId, "passed", h.Passed(), "prim", h.primitiveID, "shaderOut", ["%.4f" % c for c in h.shaderOut.col.floatValue], "post", ["%.4f" % c for c in h.postMod.col.floatValue])
        if h.Passed() and h.eventId <= LAST:
            target = h
    if target is None:
        return
    ctl.SetFrameEvent(target.eventId, True)
    st = ctl.GetPipelineState()
    refl = st.GetShaderReflection(rd.ShaderStage.Pixel)
    open(os.path.join(OUT, "pix_ps.spv"), "wb").write(bytes(refl.rawBytes))
    inputs = rd.DebugPixelInputs()
    inputs.sample = 0; inputs.primitive = target.primitiveID
    trace = ctl.DebugPixel(X, Y, inputs)
    if trace is None or trace.debugger is None:
        p("no trace"); return
    p("debug eid", target.eventId, "insts", len(trace.instInfo), "inputs", len(trace.inputs))
    for v in trace.inputs:
        p(" in", v.name, val(v))
    steps = 0
    while steps < 200000:
        states = ctl.ContinueDebug(trace.debugger)
        if not states:
            break
        for s in states:
            steps += 1
            for c in s.changes:
                a = c.after
                if a.name:
                    p(" s", s.stepIndex, "i", s.nextInstruction, a.name, val(a))
    p("steps", steps)
    ctl.FreeTrace(trace)
    ctl.Shutdown(); cap.Shutdown()

try:
    main()
except Exception:
    p(traceback.format_exc())
log.close()
os._exit(0)
