# Run inside qrenderdoc: for draw RD_ARGS=eid, dump pixel-stage buffer bindings and post-VS attributes.
import os, struct, traceback
import renderdoc as rd

RDC = os.environ["RDC"]
OUT = os.environ.get("RD_OUT", "/out/rd")
EIDS = [int(v) for v in os.environ["RD_ARGS"].split(",")]
log = open(os.path.join(OUT, "mat.txt"), "w")

def p(*a):
    log.write(" ".join(str(x) for x in a) + "\n"); log.flush()

def main():
    cap = rd.OpenCaptureFile()
    cap.OpenFile(RDC, "", None)
    r, ctl = cap.OpenCapture(rd.ReplayOptions(), None)
    bufs = {b.resourceId: b for b in ctl.GetBuffers()}
    for eid in EIDS:
        ctl.SetFrameEvent(eid, True)
        st = ctl.GetPipelineState()
        p("=== eid", eid)
        ssbo = {}
        for x in ctl.GetDescriptorAccess():
            if x.stage != rd.ShaderStage.Pixel:
                continue
            rng = rd.DescriptorRange()
            rng.offset = x.byteOffset; rng.descriptorSize = x.byteSize; rng.count = 1; rng.type = x.type
            d = ctl.GetDescriptors(x.descriptorStore, [rng])[0]
            b = bufs.get(d.resource)
            n = min(d.byteSize, 1 << 22) if d.byteSize < (1 << 60) else 1 << 22
            data = ctl.GetBufferData(d.resource, d.byteOffset, n) if b else b""
            dw = struct.unpack("<%dI" % (len(data) // 4), data[: len(data) // 4 * 4])
            nz = sum(1 for v in dw if v)
            p(" buf type", int(x.type), "idx", x.index, "res", int(d.resource), "off", d.byteOffset, "size", d.byteSize, "buflen", b.length if b else None,
              "read", len(data), "nonzero_dw", nz, "first", ["%x" % v for v in dw[:12]])
            ssbo[(int(x.type), x.index)] = dw
            if int(x.type) == 4:
                p("   image elem", x.arrayElement, "res", int(d.resource))
            if int(x.type) == 9 and x.index in (0, 1):
                for inst in (0x3c, 0x2e, 0, 1):
                    a = (inst * 156 + 56) // 4 + 1; b = (inst * 156 + 64) // 4
                    p("   inst", inst, "img_idx", dw[a] if a < len(dw) else "OOB", "samp_idx", dw[b] if b < len(dw) else "OOB",
                      "row", ["%x" % v for v in dw[inst * 39: inst * 39 + 39]] if inst * 39 + 39 <= len(dw) else "OOB")
        vrefl = st.GetShaderReflection(rd.ShaderStage.Vertex)
        open(os.path.join(OUT, "vs_e%d.spv" % eid), "wb").write(bytes(vrefl.rawBytes))
        for vb in st.GetVBuffers():
            p(" vbuf res", int(vb.resourceId), "off", vb.byteOffset, "stride", vb.byteStride, "size", vb.byteSize)
        for a in st.GetVertexInputs():
            p(" vin", a.name, "buf", a.vertexBuffer, "off", a.byteOffset, "fmt", a.format.Name(), "perInstance", a.perInstance, "used", a.used)
        for x in ctl.GetDescriptorAccess():
            if x.stage != rd.ShaderStage.Vertex:
                continue
            rng = rd.DescriptorRange(); rng.offset = x.byteOffset; rng.descriptorSize = x.byteSize; rng.count = 1; rng.type = x.type
            d = ctl.GetDescriptors(x.descriptorStore, [rng])[0]
            b = bufs.get(d.resource)
            n = min(d.byteSize, 256) if d.byteSize < (1 << 60) else 256
            data = ctl.GetBufferData(d.resource, d.byteOffset, n) if b else b""
            dw = struct.unpack("<%dI" % (len(data) // 4), data[: len(data) // 4 * 4])
            p(" vs buf type", int(x.type), "idx", x.index, "res", int(d.resource), "off", d.byteOffset, "size", d.byteSize, "first", ["%x" % v for v in dw[:10]])
        post = ctl.GetPostVSData(0, 0, rd.MeshDataStage.VSOut)
        vsr = st.GetShaderReflection(rd.ShaderStage.Vertex)
        p(" postvs stride", post.vertexByteStride, "num", post.numIndices, "outs", [(s.varName, s.compCount, s.regIndex) for s in vsr.outputSignature])
        if post.vertexResourceId != rd.ResourceId.Null() and post.vertexByteStride:
            data = ctl.GetBufferData(post.vertexResourceId, post.vertexByteOffset, post.vertexByteStride * 4)
            off = 0
            for s in vsr.outputSignature:
                sz = s.compCount * 4
                vals = []
                for v in range(min(3, len(data) // post.vertexByteStride)):
                    chunk = data[v * post.vertexByteStride + off: v * post.vertexByteStride + off + sz]
                    vals.append((["%.4g" % f for f in struct.unpack("<%df" % s.compCount, chunk)], ["%x" % u for u in struct.unpack("<%dI" % s.compCount, chunk)]))
                p("  out", s.varName, "reg", s.regIndex, vals)
                off += sz
        log.flush()
    ctl.Shutdown(); cap.Shutdown()

try:
    main()
except Exception:
    p(traceback.format_exc())
log.close()
os._exit(0)
