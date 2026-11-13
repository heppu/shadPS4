# Run inside qrenderdoc: RD_ARGS="x,y;x,y|texid|lasteid". For each pixel: the last draw that wrote it, the
# table elements its pixel shader used compared with the guest descriptors, and its samplers.
import os, struct, traceback
import renderdoc as rd
RDC = os.environ["RDC"]; OUT = os.environ.get("RD_OUT", "/out/rd")
PTS, TEX, LAST = os.environ["RD_ARGS"].split("|"); TEX = int(TEX); LAST = int(LAST)
PTS = [tuple(int(v) for v in pt.split(",")) for pt in PTS.split(";")]
log = open(os.path.join(OUT, "slot.txt"), "w")
def p(*a):
    log.write(" ".join(str(x) for x in a) + "\n"); log.flush()

def draw(ctl, texs, bufs, names, eid):
    ctl.SetFrameEvent(eid, True)
    st = ctl.GetPipelineState()
    refl = st.GetShaderReflection(rd.ShaderStage.Pixel)
    open(os.path.join(OUT, "slot_ps_e%d.spv" % eid), "wb").write(bytes(refl.rawBytes))
    p("=== eid", eid, "ps", names.get(st.GetShader(rd.ShaderStage.Pixel)), "tables", [(r.name, r.bindArraySize, str(r.textureType)) for r in refl.readOnlyResources if r.bindArraySize > 1])
    table = None; images = []
    for x in ctl.GetDescriptorAccess():
        if x.stage != rd.ShaderStage.Pixel:
            continue
        rng = rd.DescriptorRange(); rng.offset = x.byteOffset; rng.descriptorSize = x.byteSize; rng.count = 1; rng.type = x.type
        if int(x.type) == 2:
            sd = ctl.GetSamplerDescriptors(x.descriptorStore, [rng])[0]
            p("  sampler elem", x.arrayElement, "filter", str(sd.filter.minify), str(sd.filter.mip), "addr", str(sd.addressU), str(sd.addressV), "maxlod", sd.maxLOD, "aniso", sd.maxAnisotropy)
            continue
        d = ctl.GetDescriptors(x.descriptorStore, [rng])[0]
        if d.resource in bufs and d.byteSize == 524288:
            table = ctl.GetBufferData(d.resource, d.byteOffset, d.byteSize)
        if int(x.type) == 4:
            images.append((x.index, x.arrayElement, d))
    for idx, elem, d in images:
        t = texs.get(d.resource)
        line = ["  img bind", idx, "elem", elem]
        if t:
            sub = rd.Subresource(d.firstMip, d.firstSlice, 0)
            mn, mx = ctl.GetMinMax(t.resourceId, sub, rd.CompType.Typeless)
            line += ["bound %dx%dx%d arr %d mips %d %s view %d+%d" % (t.width, t.height, t.depth, t.arraysize, t.mips, t.format.Name(), d.firstMip, d.numMips),
                     "min", ["%.2f" % v for v in mn.floatValue[:3]], "max", ["%.2f" % v for v in mx.floatValue[:3]]]
        else:
            line += ["bound NULL"]
        if table and elem * 32 + 32 <= len(table) and idx == 0:
            q0, q1, q2, q3 = struct.unpack("<4Q", table[elem * 32: elem * 32 + 32])
            line += ["| guest %dx%d fmt %d nfmt %d lv %d..%d type %d arr %d..%d" % ((q1 & 0x3fff) + 1, ((q1 >> 14) & 0x3fff) + 1, (q0 >> 52) & 0x3f, (q0 >> 58) & 0xf, (q1 >> 44) & 0xf, (q1 >> 48) & 0xf, (q1 >> 60) & 0xf, (q2 >> 32) & 0x1fff, (q2 >> 45) & 0x1fff)]
        p(*line)

def main():
    cap = rd.OpenCaptureFile(); cap.OpenFile(RDC, "", None)
    r, ctl = cap.OpenCapture(rd.ReplayOptions(), None)
    texs = {t.resourceId: t for t in ctl.GetTextures()}
    bufs = {b.resourceId: b for b in ctl.GetBuffers()}
    names = {d.resourceId: d.name for d in ctl.GetResources()}
    tid = [t for t in texs if int(t) == TEX][0]
    for (X, Y) in PTS:
        ctl.SetFrameEvent(LAST, True)
        v = ctl.PickPixel(tid, X, Y, rd.Subresource(0, 0, 0), rd.CompType.Typeless)
        hist = ctl.PixelHistory(tid, X, Y, rd.Subresource(0, 0, 0), rd.CompType.Typeless)
        eids = [h.eventId for h in hist if h.Passed() and h.eventId <= LAST]
        p("######## pixel", X, Y, "rt0", ["%.3f" % c for c in v.floatValue], "history", [(h.eventId, h.Passed()) for h in hist])
        if eids:
            draw(ctl, texs, bufs, names, eids[-1])
    ctl.Shutdown(); cap.Shutdown()
try:
    main()
except Exception:
    p(traceback.format_exc())
log.close(); os._exit(0)
