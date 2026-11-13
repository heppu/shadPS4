# Run inside qrenderdoc: for each event id in RD_ARGS, list the textures and samplers the pixel shader uses and save them.
import os, traceback
import renderdoc as rd

RDC = os.environ["RDC"]
OUT = os.environ.get("RD_OUT", "/out/rd")
EIDS = [int(x) for x in os.environ["RD_ARGS"].split(",")]
log = open(os.path.join(OUT, "draw.txt"), "w")

def p(*a):
    log.write(" ".join(str(x) for x in a) + "\n"); log.flush()

def main():
    cap = rd.OpenCaptureFile()
    r = cap.OpenFile(RDC, "", None)
    r, ctl = cap.OpenCapture(rd.ReplayOptions(), None)
    texs = {t.resourceId: t for t in ctl.GetTextures()}
    names = {d.resourceId: d.name for d in ctl.GetResources()}
    for eid in EIDS:
        ctl.SetFrameEvent(eid, True)
        st = ctl.GetPipelineState()
        p("=== eid", eid, "ps", names.get(st.GetShader(rd.ShaderStage.Pixel)), "vs", names.get(st.GetShader(rd.ShaderStage.Vertex)))
        for o in st.GetOutputTargets():
            if o.resource != rd.ResourceId.Null():
                t = texs.get(o.resource)
                p("  out", int(o.resource), t.format.Name() if t else "?")
        saved = 0
        acts = {}
        def walk(a):
            acts[a.eventId] = a
            for c in a.children:
                walk(c)
        for a in ctl.GetRootActions():
            walk(a)
        a = acts.get(eid)
        p("  action", a.customName if a else None, "flags", str(a.flags) if a else None, "idx", a.numIndices if a else None)
        vk = ctl.GetVulkanPipelineState()
        for si, dset in enumerate(vk.graphics.descriptorSets):
            p("  set", si, "store", int(dset.descriptorSetResourceId), names.get(dset.descriptorSetResourceId), "layout", int(dset.layoutResourceId), "push", dset.pushDescriptor)
            if si == 0 or dset.descriptorSetResourceId == rd.ResourceId.Null():
                continue
            locs = ctl.GetDescriptorLocations(dset.descriptorSetResourceId, [rd.DescriptorRange()]) if False else None
            rng = rd.DescriptorRange()
            rng.offset = 0; rng.count = 16384 if si == 1 else 85
            rng.descriptorSize = 1
            rng.type = rd.DescriptorType.Image if si == 1 else rd.DescriptorType.Sampler
            try:
                ds = ctl.GetDescriptors(dset.descriptorSetResourceId, [rng])
                nn = [(i, d) for i, d in enumerate(ds) if d.resource != rd.ResourceId.Null()]
                p("   descriptors", len(ds), "nonnull", len(nn), "types", sorted(set(str(d.type) for d in ds))[:4])
                import collections
                hist = collections.Counter()
                for i, d in nn:
                    t = texs.get(d.resource)
                    hist[(t.width, t.height, t.mips) if t else None] += 1
                p("   size hist", sorted(hist.items(), key=lambda kv: -kv[1])[:14])
                for i, d in nn[:400]:
                    t = texs.get(d.resource)
                    if si == 1 and t and t.width >= 512 and saved < 10:
                        save = rd.TextureSave()
                        save.resourceId = d.resource
                        save.destType = rd.FileType.PNG
                        save.mip = 1; save.slice.sliceIndex = 0
                        save.alpha = rd.AlphaMapping.Discard
                        ctl.SaveTexture(save, os.path.join(OUT, "tbl_%d_r%d.png" % (i, int(d.resource))))
                        saved += 1
                for i, d in nn[:12]:
                    t = texs.get(d.resource)
                    p("    [%d]" % i, int(d.resource), ("%dx%d mips %d arr %d %s view mip %d+%d" % (t.width, t.height, t.mips, t.arraysize, t.format.Name(), d.firstMip, d.numMips)) if t else "notex")
                if si == 2:
                    ss = ctl.GetSamplerDescriptors(dset.descriptorSetResourceId, [rng])
                    for i, d in enumerate(ss[:6]):
                        p("    samp[%d]" % i, "minlod", d.minLOD, "maxlod", d.maxLOD, "bias", d.mipBias, str(d.filter.minify), str(d.addressU), "aniso", d.maxAnisotropy)
            except Exception as e:
                p("   err", e)
        acc = ctl.GetDescriptorAccess()
        p("  acc types", sorted(set((str(x.type), str(x.stage)) for x in acc)))
        p("  accesses", len(acc))
        for x in acc[:60]:
            if x.stage != rd.ShaderStage.Pixel:
                continue
            rng = rd.DescriptorRange()
            rng.offset = x.byteOffset; rng.descriptorSize = x.byteSize; rng.count = 1; rng.type = x.type
            ds = ctl.GetDescriptors(x.descriptorStore, [rng])
            d = ds[0] if ds else None
            t = texs.get(d.resource) if d else None
            p("   acc", str(x.type), "idx", x.index, "elem", x.arrayElement, "static_unused", x.staticallyUnused, "res", int(d.resource) if d else None,
              ("%dx%d mips %d %s view %d+%d" % (t.width, t.height, t.mips, t.format.Name(), d.firstMip, d.numMips)) if t else "")
            if t is not None and saved < 16:
                save = rd.TextureSave()
                save.resourceId = d.resource
                save.destType = rd.FileType.PNG
                save.mip = 0; save.slice.sliceIndex = 0
                save.alpha = rd.AlphaMapping.Discard
                ctl.SaveTexture(save, os.path.join(OUT, "e%d_i%d_%d_r%d.png" % (eid, x.index, x.arrayElement, int(d.resource))))
                saved += 1
        for stage in (rd.ShaderStage.Pixel,):
            refl = st.GetShaderReflection(stage)
            if refl:
                for i, r in enumerate(refl.readOnlyResources):
                    p("   refl ro", i, r.name, "set", r.fixedBindSetOrSpace, "bind", r.fixedBindNumber, "n", r.bindArraySize, str(r.textureType))
                for i, r in enumerate(refl.samplers):
                    p("   refl samp", i, r.name, "set", r.fixedBindSetOrSpace, "bind", r.fixedBindNumber, "n", r.bindArraySize)
            used = st.GetReadOnlyResources(stage, True)
            allr = st.GetReadOnlyResources(stage, False)
            p("  ro used", len(used), "all", len(allr), "nonnull", sum(1 for u in allr if u.descriptor.resource != rd.ResourceId.Null()))
            if not used:
                used = [u for u in allr if u.descriptor.resource != rd.ResourceId.Null()][:40]
            for u in used:
                d = u.descriptor
                t = texs.get(d.resource)
                if t is None:
                    p("   bind", u.access.index, "elem", u.access.arrayElement, "res", int(d.resource), "(not a texture or null)")
                    continue
                p("   bind", u.access.index, "elem", u.access.arrayElement, "res", int(d.resource), names.get(d.resource),
                  "%dx%dx%d" % (t.width, t.height, t.depth), "arr", t.arraysize, "mips", t.mips, t.format.Name(),
                  "view mip", d.firstMip, d.numMips, "slice", d.firstSlice, d.numSlices, "minlod", d.minLODClamp, "type", str(d.textureType))
                if saved < 24:
                    save = rd.TextureSave()
                    save.resourceId = d.resource
                    save.destType = rd.FileType.PNG
                    save.mip = 0; save.slice.sliceIndex = 0
                    save.alpha = rd.AlphaMapping.Discard
                    ctl.SaveTexture(save, os.path.join(OUT, "e%d_b%d_%d_r%d.png" % (eid, u.access.index, u.access.arrayElement, int(d.resource))))
                    saved += 1
            for u in st.GetSamplers(stage, True):
                d = u.descriptor
                p("   samp", u.access.index, "elem", u.access.arrayElement, "minlod", d.minLOD, "maxlod", d.maxLOD, "bias", d.mipBias,
                  "filter", str(d.filter.minify), str(d.filter.magnify), str(d.filter.mip), "addr", str(d.addressU), str(d.addressV), "aniso", d.maxAnisotropy)
    ctl.Shutdown(); cap.Shutdown()

try:
    main()
except Exception:
    p(traceback.format_exc())
log.close()
os._exit(0)
