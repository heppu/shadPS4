#define SHADTEST_NAME "runtime_image_lifetimes"
#include <gnm_sampler.h>
#include <gnm_texture.h>
#include "copy.h"
#include "shadtest_guest.h"
static bool submit_and_wait(StDcb* dcb) {
    const uint64_t address = (uint64_t)(uintptr_t)dcb->queue->label;
    const uint64_t value = ++dcb->queue->next_value;
    const uint32_t eop[] = {
        PKT3(PKT3_EVENT_WRITE_EOP, 4, 0),
        EVENT_TYPE(GNM_CACHE_FLUSH_AND_INV_TS_EVENT) | EVENT_INDEX(5),
        (uint32_t)address,
        (uint32_t)(address >> 32) | (2u << 29),
        (uint32_t)value,
        (uint32_t)(value >> 32),
    };
    st_dcb_put(dcb, eop, sizeof(eop) / sizeof(eop[0]));
    void* commands[] = {dcb->cmd.beginptr};
    uint32_t sizes[] = {(uint32_t)((uintptr_t)dcb->cmd.cmdptr - (uintptr_t)dcb->cmd.beginptr)};
    st_store_fence();
    if (!dcb->ok || sceGnmSubmitCommandBuffers(1, commands, sizes, NULL, NULL) < 0) {
        return false;
    }
    return st_wait_label(dcb->queue->label, value);
}

int main(void) {
    st_begin();
    StArena arena;
    StQueue queue;
    if (!st_arena_init(&arena, 4 * 1024 * 1024) || !st_queue_init(&arena, &queue))
        return st_fail("reason=allocation");
    void* code = st_arena_alloc(&arena, sizeof(copy_shader), 256);
    memcpy(code, copy_shader, sizeof(copy_shader));
    GnmCsStageRegisters cs = {0};
    sceGnmCsRegsSetAddress(&cs, code);
    cs.computepgmrsrc1 = 2u | (3u << 6) | (0xc0u << 12);
    cs.computepgmrsrc2 = (12u << 1) | (1u << 7);
    cs.computenumthreadx = 64;
    cs.computenumthready = cs.computenumthreadz = 1;
    GnmTexture* textures = st_arena_alloc(&arena, 2 * sizeof(GnmTexture), 65536);
    float* pixels[2];
    uint64_t sizes[2];
    for (unsigned t = 0; t < 2; ++t) {
        GnmTextureCreateInfo info = {.format = GNM_FMT_R32G32B32A32_FLOAT,
                                     .texturetype = GNM_TEXTURE_2D,
                                     .width = 64,
                                     .height = 64,
                                     .depth = 1,
                                     .pitch = 64,
                                     .nummiplevels = t ? 2 : 1,
                                     .numslices = 1,
                                     .numfragments = 1,
                                     .tilemodehint = GNM_TM_DISPLAY_LINEAR_ALIGNED,
                                     .mingpumode = GNM_GPU_BASE};
        if (sceGnmCreateTexture(&textures[t], &info) != GNM_ERROR_OK)
            return st_fail("reason=texture_create");
        uint64_t bytes = 0;
        uint32_t alignment = 0;
        if (sceGnmTexCalcByteSize(&bytes, &alignment, &textures[t]) != GNM_ERROR_OK)
            return st_fail("reason=texture_size");
        float* data = st_arena_alloc(&arena, bytes, 65536);
        if (!data)
            return st_fail("reason=texture_memory");
        pixels[t] = data;
        sizes[t] = bytes;
        for (unsigned i = 0; i < bytes / sizeof(float); ++i)
            data[i] = .125f + (float)(i / 4) / 16384.f;
        sceGnmTexSetBaseAddress(&textures[t], data);
    }
    GnmTexture* tables[3];
    for (unsigned t = 0; t < 3; ++t) {
        tables[t] = st_arena_alloc(&arena, 2 * sizeof(GnmTexture), 65536);
        tables[t][0] = textures[t != 0];
        if (t == 1)
            tables[t][0].lastlevel = 0;
        tables[t][1] = tables[t][0];
    }
    GnmSampler sampler = {0};
    StCase cases[7] = {{.name = "initial"},
                       {.name = "foreign_small"},
                       {.name = "foreign_expanded"},
                       {.name = "surviving_table"},
                       {.name = "cpu_update"},
                       {.name = "one_descriptor_update"},
                       {.name = "guards"}};
    for (unsigned phase = 0; phase < 6; ++phase) {
        if (phase == 4) {
            for (unsigned i = 0; i < sizes[0] / 4; ++i)
                pixels[0][i] = .5f;
            st_store_fence();
        }
        if (phase == 5) {
            tables[0][1] = textures[1];
            st_store_fence();
        }
        GnmBuffer table = st_raw_buffer(tables[phase == 1 ? 1 : phase == 2 ? 2 : 0],
                                       2 * sizeof(GnmTexture));
        table.stride = sizeof(GnmTexture);
        table.numrecords = 2;
        volatile uint32_t* output = st_arena_alloc(&arena, 144 * 4, 65536);
        st_fill_words(output, 144, 0xdeadbeef);
        GnmBuffer result = st_raw_buffer(output + 8, 128 * 4);
        result.stride = 4;
        result.numrecords = 128;
        StDcb dcb;
        st_dcb_begin(&dcb, &queue);
        sceGnmDrawCmdSetCsShader(&dcb.cmd, &cs);
        sceGnmDrawCmdSetVsharpUserData(&dcb.cmd, GNM_STAGE_CS, 0, &table);
        sceGnmDrawCmdSetSsharpUserData(&dcb.cmd, GNM_STAGE_CS, 4, &sampler);
        sceGnmDrawCmdSetVsharpUserData(&dcb.cmd, GNM_STAGE_CS, 8, &result);
        st_dcb_barrier(&dcb, 0);
        sceGnmDrawCmdDispatchDirect(&dcb.cmd, 2, 1, 1, 0);
        st_dcb_barrier(&dcb, 0);
        if (!submit_and_wait(&dcb))
            return st_fail("reason=submit");
        for (unsigned i = 0; i < 144; ++i) {
            uint32_t expected = 0xdeadbeef;
            unsigned which = 6;
            if (i >= 8 && i < 136) {
                which = phase;
                const float original = .125f + (float)(32 * 64 + 32) / 16384.f;
                float value = phase >= 4 && (phase == 4 || i < 72) ? .5f : original;
                memcpy(&expected, &value, 4);
            }
            st_case_expect(&cases[which], "word", i, expected, output[i]);
        }
    }
    sceGnmSubmitDone();
    return st_finish_cases(cases, 7, "foreign_mip_expansion=1 surviving_table=1 cpu_update=1 one_descriptor_update=1 guards=1");
}
