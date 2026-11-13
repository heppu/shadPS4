#define SHADTEST_NAME "sampler_shared_page"
#include <gnm_sampler.h>
#include <gnm_texture.h>
#include "sample.h"
#include "write.h"
#include "shadtest_guest.h"

static bool submit_and_wait(StDcb* dcb) {
    const uint64_t address = (uint64_t)(uintptr_t)dcb->queue->label;
    const uint64_t value = ++dcb->queue->next_value;
    const uint32_t eop[] = {
        PKT3(PKT3_EVENT_WRITE_EOP, 4, 0),
        EVENT_TYPE(GNM_CACHE_FLUSH_AND_INV_TS_EVENT) | EVENT_INDEX(5),
        (uint32_t)address, (uint32_t)(address >> 32) | (2u << 29),
        (uint32_t)value, (uint32_t)(value >> 32),
    };
    st_dcb_put(dcb, eop, sizeof(eop) / sizeof(eop[0]));
    void* commands[] = {dcb->cmd.beginptr};
    uint32_t sizes[] = {(uint32_t)((uintptr_t)dcb->cmd.cmdptr - (uintptr_t)dcb->cmd.beginptr)};
    st_store_fence();
    return dcb->ok && sceGnmSubmitCommandBuffers(1, commands, sizes, NULL, NULL) >= 0 &&
           st_wait_label(dcb->queue->label, value);
}

static GnmCsStageRegisters shader(StArena* arena, const void* data, size_t size,
                                  unsigned users) {
    void* code = st_arena_alloc(arena, size, 256);
    memcpy(code, data, size);
    GnmCsStageRegisters cs = {0};
    sceGnmCsRegsSetAddress(&cs, code);
    cs.computepgmrsrc1 = 1u | (3u << 6) | (0xc0u << 12);
    cs.computepgmrsrc2 = (users << 1) | (1u << 7);
    cs.computenumthreadx = 64;
    cs.computenumthready = cs.computenumthreadz = 1;
    return cs;
}

static GnmBuffer words(const void* data, unsigned count) {
    GnmBuffer b = st_raw_buffer(data, count * 4);
    b.stride = 4;
    b.numrecords = count;
    return b;
}

int main(void) {
    st_begin();
    StArena arena;
    StQueue queue;
    if (!st_arena_init(&arena, 4 * 1024 * 1024) || !st_queue_init(&arena, &queue))
        return st_fail("reason=allocation");
    GnmCsStageRegisters reader = shader(&arena, sample_shader, sizeof(sample_shader), 16);
    GnmCsStageRegisters writer = shader(&arena, write_shader, sizeof(write_shader), 8);
    GnmTexture texture = {0};
    GnmTextureCreateInfo info = {
        .format = GNM_FMT_R32G32B32A32_FLOAT, .texturetype = GNM_TEXTURE_2D,
        .width = 64, .height = 64, .depth = 1, .pitch = 64, .nummiplevels = 1,
        .numslices = 1, .numfragments = 1, .tilemodehint = GNM_TM_DISPLAY_LINEAR_ALIGNED,
        .mingpumode = GNM_GPU_BASE,
    };
    if (sceGnmCreateTexture(&texture, &info) != GNM_ERROR_OK)
        return st_fail("reason=texture_create");
    uint64_t bytes = 0;
    uint32_t alignment = 0;
    if (sceGnmTexCalcByteSize(&bytes, &alignment, &texture) != GNM_ERROR_OK)
        return st_fail("reason=texture_size");
    float* pixels = st_arena_alloc(&arena, bytes, 65536);
    uint32_t* shared = st_arena_alloc(&arena, 4096, 65536);
    uint32_t* source = st_arena_alloc(&arena, 256, 65536);
    GnmSampler* replacement = st_arena_alloc(&arena, 32, 65536);
    volatile uint32_t* output = st_arena_alloc(&arena, 144 * 4, 65536);
    if (!pixels || !shared || !source || !replacement || !output)
        return st_fail("reason=memory");
    for (unsigned i = 0; i < bytes / 4; ++i)
        pixels[i] = (i / 4) % 64 < 32 ? .25f : .75f;
    sceGnmTexSetBaseAddress(&texture, pixels);
    st_fill_words(shared, 1024, 0xdeadbeef);
    for (unsigned i = 0; i < 64; ++i)
        source[i] = 0x12340000u + i;
    GnmSampler* samplers = (GnmSampler*)((uint8_t*)shared + 1088);
    memset(samplers, 0, 64 * sizeof(GnmSampler));
    samplers[1].clampx = GNM_TEX_CLAMP_CLAMP_LAST_TEXEL;
    replacement[0] = samplers[1];
    replacement[1] = samplers[0];
    GnmBuffer sampler_table = st_raw_buffer(samplers, 64 * sizeof(GnmSampler));
    sampler_table.stride = sizeof(GnmSampler);
    sampler_table.numrecords = 64;
    StCase cases[4] = {{.name = "gpu_neighbor"}, {.name = "gpu_sampler_update"},
                       {.name = "cpu_sampler_update"}, {.name = "guards_and_neighbor"}};
    for (unsigned phase = 0; phase < 3; ++phase) {
        if (phase == 2) {
            samplers[0] = replacement[1];
            samplers[1] = replacement[0];
        }
        st_fill_words(output, 144, 0xdeadbeef);
        GnmBuffer from = words(source, 64);
        GnmBuffer neighbor = words(shared, 256);
        GnmBuffer result = words((const void*)(output + 8), 128);
        StDcb dcb;
        st_dcb_begin(&dcb, &queue);
        writer.computenumthreadx = 64;
        sceGnmDrawCmdSetCsShader(&dcb.cmd, &writer);
        sceGnmDrawCmdSetVsharpUserData(&dcb.cmd, GNM_STAGE_CS, 0, &from);
        sceGnmDrawCmdSetVsharpUserData(&dcb.cmd, GNM_STAGE_CS, 4, &neighbor);
        sceGnmDrawCmdDispatchDirect(&dcb.cmd, 1, 1, 1, 0);
        st_dcb_barrier(&dcb, 0);
        if (phase == 1) {
            from = words(replacement, 8);
            GnmBuffer to = words(samplers, 8);
            writer.computenumthreadx = 8;
            sceGnmDrawCmdSetCsShader(&dcb.cmd, &writer);
            sceGnmDrawCmdSetVsharpUserData(&dcb.cmd, GNM_STAGE_CS, 0, &from);
            sceGnmDrawCmdSetVsharpUserData(&dcb.cmd, GNM_STAGE_CS, 4, &to);
            sceGnmDrawCmdDispatchDirect(&dcb.cmd, 1, 1, 1, 0);
            st_dcb_barrier(&dcb, 0);
        }
        sceGnmDrawCmdSetCsShader(&dcb.cmd, &reader);
        sceGnmDrawCmdSetTsharpUserData(&dcb.cmd, GNM_STAGE_CS, 0, &texture);
        sceGnmDrawCmdSetVsharpUserData(&dcb.cmd, GNM_STAGE_CS, 8, &sampler_table);
        sceGnmDrawCmdSetVsharpUserData(&dcb.cmd, GNM_STAGE_CS, 12, &result);
        sceGnmDrawCmdDispatchDirect(&dcb.cmd, 2, 1, 1, 0);
        st_dcb_barrier(&dcb, 0);
        if (!submit_and_wait(&dcb))
            return st_fail("reason=submit phase=%u", phase);
        for (unsigned i = 0; i < 144; ++i) {
            uint32_t expected = 0xdeadbeef;
            unsigned which = 3;
            if (i >= 8 && i < 136) {
                which = phase;
                bool clamp = ((i - 8) >= 64) ^ (phase == 1);
                expected = clamp ? 0x3f400000 : 0x3e800000;
            }
            st_case_expect(&cases[which], "sample", phase * 144 + i, expected, output[i]);
        }
        for (unsigned i = 0; i < 272; ++i)
            st_case_expect(&cases[3], "neighbor", phase * 272 + i,
                           i < 64 ? source[i] : 0xdeadbeef, shared[i]);
    }
    sceGnmSubmitDone();
    return st_finish_cases(cases, 4, "shared_page=1 gpu_sampler_write=1 cpu_sampler_write=1 guards=1");
}
