# Run inside qrenderdoc: RD_ARGS="x,y,texid,lasteid". Finds the draw that wrote the pixel, then for every table
# element its pixel shader accessed compares the bound image with the guest descriptor at that table index.
import os, struct, traceback
import renderdoc as rd
RDC = os.environ["RDC"]; OUT = os.environ.get("RD_OUT", "/out/rd")
X, Y, TEX, LAST = [int(v) for v in os.environ["RD_ARGS"].split(",")]
log = open(os.path.join(OUT, "slot.txt"), "w")
def p(*a):
    log.write(" ".join(str(x) for x in a) + "\n"); log.flush()
def main():
    cap = rd.OpenCaptureFile(); cap.OpenFile(RDC, "", None)
    r, ctl = cap.OpenCapture(rd.ReplayOptions(), None)
    texs = {t.resourceId: t for t in ctl.GetTextures()}
    bufs = {b.resourceId: b for b in ctl.GetBuffers()}
    names = {d.resourceId: d.name for d in ctl.GetResources()}
    tid = [t for t in texs if int(t) == TEX][0]
    ctl.SetFrameEvent(LAST, True)
    hist = ctl.PixelHistory(tid, X, Y, rd.Subresource(0, 0, 0), rd.CompType.Typeless)
    eids = [h.eventId for h in hist if h.Passed() and h.eventId <= LAST]
    p("history", [(h.eventId, h.Passed()) for h in hist])
    for eid in eids[-2:]:
        ctl.SetFrameEvent(eid, True)
        p("=== eid", eid)
        table = None; images = []
        for x in ctl.GetDescriptorAccess():
            if x.stage != rd.ShaderStage.Pixel:
                continue
            rng = rd.DescriptorRange(); rng.offset = x.byteOffset; rng.descriptorSize = x.byteSize; rng.count = 1; rng.type = x.type
            d = ctl.GetDescriptors(x.descriptorStore, [rng])[0]
            if d.resource in bufs and d.byteSize == 524288:
                table = ctl.GetBufferData(d.resource, d.byteOffset, d.byteSize)
            if d.resource in texs or (int(x.type) == 4 and x.arrayElement > 0):
                images.append((x.index, x.arrayElement, d))
        p("table bytes", len(table) if table else None)
        for x in ctl.GetDescriptorAccess():
            if x.stage == rd.ShaderStage.Pixel and int(x.type) == 2:
                rng = rd.DescriptorRange(); rng.offset = x.byteOffset; rng.descriptorSize = x.byteSize; rng.count = 1; rng.type = x.type
                sd = ctl.GetSamplerDescriptors(x.descriptorStore, [rng])[0]
                p("sampler elem", x.arrayElement, "minlod", sd.minLOD, "maxlod", sd.maxLOD, "bias", sd.mipBias, "filter", str(sd.filter.minify), str(sd.filter.magnify), str(sd.filter.mip),
                  "addr", str(sd.addressU), str(sd.addressV), "border", [round(c, 2) for c in sd.borderColorValue.floatValue], "aniso", sd.maxAnisotropy, "cmp", str(sd.compareFunction))
        st = ctl.GetPipelineState()
        refl = st.GetShaderReflection(rd.ShaderStage.Pixel)
        open(os.path.join(OUT, "slot_ps_e%d.spv" % eid), "wb").write(bytes(refl.rawBytes))
        for idx, elem, d in images:
            t = texs.get(d.resource)
            line = ["img idx", idx, "elem", elem]
            if t:
                addr = [w for w in names.get(d.resource, "").split() if w.startswith("0x")]
                line += ["bound", "%dx%d mips %d %s @%s view %d+%d" % (t.width, t.height, t.mips, t.format.Name(), addr[0] if addr else "?", d.firstMip, d.numMips)]
            else:
                line += ["bound NULL"]
            if table and elem * 32 + 32 <= len(table):
                q0, q1, q2, q3 = struct.unpack("<4Q", table[elem * 32: elem * 32 + 32])
                va = (q0 & ((1 << 38) - 1)) << 8
                line += ["| guest va %#x %dx%d fmt %d nfmt %d lv %d..%d minlod %.2f type %d" % (va, (q1 & 0x3fff) + 1, ((q1 >> 14) & 0x3fff) + 1, (q0 >> 52) & 0x3f, (q0 >> 58) & 0xf, (q1 >> 44) & 0xf, (q1 >> 48) & 0xf, ((q0 >> 40) & 0xfff) / 256.0, (q1 >> 60) & 0xf)]
            p(*line)
            if t:
                save = rd.TextureSave(); save.resourceId = d.resource; save.destType = rd.FileType.PNG
                save.mip = min(1, t.mips - 1); save.slice.sliceIndex = 0; save.alpha = rd.AlphaMapping.Discard
                ctl.SaveTexture(save, os.path.join(OUT, "slot_e%d_%d.png" % (eid, elem)))
    ctl.Shutdown(); cap.Shutdown()
try:
    main()
except Exception:
    p(traceback.format_exc())
log.close(); os._exit(0)
