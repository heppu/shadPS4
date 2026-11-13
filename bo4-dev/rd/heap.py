# Run inside qrenderdoc: RD_ARGS="eid|idx,idx,...". Prints raw 32-byte image table entries (pixel stage 512 KB buffer).
import os, struct, traceback
import renderdoc as rd
RDC = os.environ["RDC"]; OUT = os.environ.get("RD_OUT", "/out/rd")
EID, IDX = os.environ["RD_ARGS"].split("|"); EID = int(EID); IDX = [int(v) for v in IDX.split(",")]
log = open(os.path.join(OUT, "heap.txt"), "w")
def p(*a):
    log.write(" ".join(str(x) for x in a) + "\n"); log.flush()
def main():
    cap = rd.OpenCaptureFile(); cap.OpenFile(RDC, "", None)
    r, ctl = cap.OpenCapture(rd.ReplayOptions(), None)
    ctl.SetFrameEvent(EID, True)
    for x in ctl.GetDescriptorAccess():
        if x.stage != rd.ShaderStage.Pixel or int(x.type) != 9:
            continue
        rng = rd.DescriptorRange(); rng.offset = x.byteOffset; rng.descriptorSize = x.byteSize; rng.count = 1; rng.type = x.type
        d = ctl.GetDescriptors(x.descriptorStore, [rng])[0]
        if d.byteSize != 524288:
            continue
        data = ctl.GetBufferData(d.resource, d.byteOffset, d.byteSize)
        for i in IDX:
            e = struct.unpack("<8I", data[i * 32: i * 32 + 32])
            f = struct.unpack("<8f", data[i * 32: i * 32 + 32])
            p("entry", i, " ".join("%08x" % v for v in e), "| floats", " ".join("%.3g" % v for v in f if 1e-4 < abs(v) < 1e5))
    ctl.Shutdown(); cap.Shutdown()
try:
    main()
except Exception:
    p(traceback.format_exc())
log.close(); os._exit(0)
