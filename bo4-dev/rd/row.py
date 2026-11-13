# Run inside qrenderdoc: RD_ARGS="eid|stride_bytes|instance". Prints the material row (pixel stage buffer binding 0)
# and what the image table holds at every value in it.
import os, struct, traceback
import renderdoc as rd
RDC = os.environ["RDC"]; OUT = os.environ.get("RD_OUT", "/out/rd")
EID, STRIDE, INST = [int(v) for v in os.environ["RD_ARGS"].split("|")]
log = open(os.path.join(OUT, "row.txt"), "w")
def p(*a):
    log.write(" ".join(str(x) for x in a) + "\n"); log.flush()
def main():
    cap = rd.OpenCaptureFile(); cap.OpenFile(RDC, "", None)
    r, ctl = cap.OpenCapture(rd.ReplayOptions(), None)
    bufs = {b.resourceId: b for b in ctl.GetBuffers()}
    ctl.SetFrameEvent(EID, True)
    row = None; table = None; samp = None
    for x in ctl.GetDescriptorAccess():
        if x.stage != rd.ShaderStage.Pixel or int(x.type) != 9:
            continue
        rng = rd.DescriptorRange(); rng.offset = x.byteOffset; rng.descriptorSize = x.byteSize; rng.count = 1; rng.type = x.type
        d = ctl.GetDescriptors(x.descriptorStore, [rng])[0]
        p("buf idx", x.index, "res", int(d.resource), "size", d.byteSize)
        if x.index == 0:
            data = ctl.GetBufferData(d.resource, d.byteOffset + INST * STRIDE, STRIDE)
            row = struct.unpack("<%dI" % (STRIDE // 4), data)
        if d.byteSize == 524288:
            table = ctl.GetBufferData(d.resource, d.byteOffset, d.byteSize)
        if d.byteSize in (1360, 1376):
            samp = ctl.GetBufferData(d.resource, d.byteOffset, d.byteSize)
    for x in ctl.GetDescriptorAccess():
        if x.stage == rd.ShaderStage.Pixel and int(x.type) == 9 and x.index == 0:
            rng = rd.DescriptorRange(); rng.offset = x.byteOffset; rng.descriptorSize = x.byteSize; rng.count = 1; rng.type = x.type
            d = ctl.GetDescriptors(x.descriptorStore, [rng])[0]
            p("material buffer offset", d.byteOffset, "size", d.byteSize)
            for k in (0, 1, 2, INST - 1, INST, INST + 1, 400, 2000):
                data = ctl.GetBufferData(d.resource, d.byteOffset + k * STRIDE, STRIDE)
                dw = struct.unpack("<%dI" % (STRIDE // 4), data)
                fl = struct.unpack("<%df" % (STRIDE // 4), data)
                p(" row", k, " ".join("%x" % v for v in dw), "| floats", " ".join("%.3g" % f for f in fl if 1e-6 < abs(f) < 1e6))
            whole = ctl.GetBufferData(d.resource, d.byteOffset, min(d.byteSize, 1572864))
            dws = struct.unpack("<%dI" % (len(whole) // 4), whole)
            floaty = sum(1 for v in dws if 0x38000000 <= (v & 0x7fffffff) <= 0x47000000)
            small = sum(1 for v in dws if 0 < v < 16384)
            p(" whole buffer dwords", len(dws), "float-like", floaty, "small ints", small, "zero", sum(1 for v in dws if v == 0))
            first_float = next((i for i, v in enumerate(dws) if 0x38000000 <= (v & 0x7fffffff) <= 0x47000000), None)
            p(" first float-like dword index", first_float, "=> byte", first_float * 4 if first_float is not None else None)
    p("row", ["%x" % v for v in row])
    texs = {t.resourceId: t for t in ctl.GetTextures()}
    for x in ctl.GetDescriptorAccess():
        if x.stage == rd.ShaderStage.Pixel and int(x.type) == 4:
            rng = rd.DescriptorRange(); rng.offset = x.byteOffset; rng.descriptorSize = x.byteSize; rng.count = 1; rng.type = x.type
            d = ctl.GetDescriptors(x.descriptorStore, [rng])[0]
            t = texs.get(d.resource)
            if t:
                mn, mx = ctl.GetMinMax(t.resourceId, rd.Subresource(0, 0, 0), rd.CompType.Typeless)
                p("  image elem", x.arrayElement, t.format.Name(), "%dx%d" % (t.width, t.height), "min", ["%.3f" % v for v in mn.floatValue], "max", ["%.3f" % v for v in mx.floatValue])
    for i, v in enumerate(row):
        line = ["  dw", i, "val", v]
        if table and v < 16384:
            q0, q1, q2, q3 = struct.unpack("<4Q", table[v * 32: v * 32 + 32])
            if q1 >> 60:
                line += ["tex %dx%d fmt %d nfmt %d lv %d type %d" % ((q1 & 0x3fff) + 1, ((q1 >> 14) & 0x3fff) + 1, (q0 >> 52) & 0x3f, (q0 >> 58) & 0xf, ((q1 >> 48) & 0xf) + 1, (q1 >> 60) & 0xf)]
            else:
                line += ["tex slot empty"]
        if samp and v * 16 + 16 <= len(samp):
            line += ["| sampler", ["%08x" % w for w in struct.unpack("<4I", samp[v * 16: v * 16 + 16])]]
        p(*line)
    ctl.Shutdown(); cap.Shutdown()
try:
    main()
except Exception:
    p(traceback.format_exc())
log.close(); os._exit(0)
