# Run inside qrenderdoc: RD_ARGS=eid,eid lists every descriptor the pixel shader accesses at those draws, saves textures.
import os, struct, traceback
import renderdoc as rd
RDC = os.environ["RDC"]; OUT = os.environ.get("RD_OUT", "/out/rd")
log = open(os.path.join(OUT, "acc.txt"), "w")
def p(*a):
    log.write(" ".join(str(x) for x in a) + "\n"); log.flush()
def main():
    cap = rd.OpenCaptureFile(); cap.OpenFile(RDC, "", None)
    r, ctl = cap.OpenCapture(rd.ReplayOptions(), None)
    texs = {t.resourceId: t for t in ctl.GetTextures()}
    bufs = {b.resourceId: b for b in ctl.GetBuffers()}
    names = {d.resourceId: d.name for d in ctl.GetResources()}
    for eid in [int(v) for v in os.environ["RD_ARGS"].split(",")]:
        ctl.SetFrameEvent(eid, True)
        st = ctl.GetPipelineState()
        refl = st.GetShaderReflection(rd.ShaderStage.Pixel)
        p("=== eid", eid, "ps", names.get(st.GetShader(rd.ShaderStage.Pixel)))
        if refl:
            open(os.path.join(OUT, "ps_e%d.spv" % eid), "wb").write(bytes(refl.rawBytes))
            for i, r in enumerate(refl.readOnlyResources):
                p("  refl ro", i, r.name, "set", r.fixedBindSetOrSpace, "bind", r.fixedBindNumber, "n", r.bindArraySize, str(r.textureType), "istex", r.isTexture)
        for o in st.GetOutputTargets():
            if o.resource != rd.ResourceId.Null():
                p("  out", int(o.resource), texs[o.resource].format.Name())
        cb = st.GetColorBlends()
        p("  blend", [(b.enabled, str(b.colorBlend.source), str(b.colorBlend.destination)) for b in cb[:3]])
        for x in ctl.GetDescriptorAccess():
            if x.stage != rd.ShaderStage.Pixel:
                continue
            rng = rd.DescriptorRange(); rng.offset = x.byteOffset; rng.descriptorSize = x.byteSize; rng.count = 1; rng.type = x.type
            d = ctl.GetDescriptors(x.descriptorStore, [rng])[0]
            t = texs.get(d.resource); b = bufs.get(d.resource)
            if t:
                sub = rd.Subresource(d.firstMip, d.firstSlice, 0)
                mn, mx = ctl.GetMinMax(t.resourceId, sub, rd.CompType.Typeless)
                p("  tex type", int(x.type), "idx", x.index, "elem", x.arrayElement, "res", int(d.resource), "%dx%dx%d arr %d mips %d %s" % (t.width, t.height, t.depth, t.arraysize, t.mips, t.format.Name()),
                  "view mip", d.firstMip, d.numMips, "slice", d.firstSlice, d.numSlices, "min", ["%.3g" % v for v in mn.floatValue], "max", ["%.3g" % v for v in mx.floatValue])
            elif b:
                n = min(d.byteSize, 4096)
                data = ctl.GetBufferData(d.resource, d.byteOffset, n)
                dw = struct.unpack("<%dI" % (len(data) // 4), data[: len(data) // 4 * 4])
                p("  buf type", int(x.type), "idx", x.index, "res", int(d.resource), "size", d.byteSize, "first", ["%x" % v for v in dw[:16]])
            else:
                p("  null type", int(x.type), "idx", x.index, "elem", x.arrayElement, "res", int(d.resource))
    ctl.Shutdown(); cap.Shutdown()
try:
    main()
except Exception:
    p(traceback.format_exc())
log.close(); os._exit(0)
