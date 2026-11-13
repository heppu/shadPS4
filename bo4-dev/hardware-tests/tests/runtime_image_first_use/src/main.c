#define SHADTEST_NAME "runtime_image_first_use"
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
    if (!st_arena_init(&arena, 16 * 1024 * 1024) || !st_queue_init(&arena, &queue))
        return st_fail("reason=allocation");
    void* code = st_arena_alloc(&arena, sizeof(copy_shader), 256);
    memcpy(code, copy_shader, sizeof(copy_shader));
    GnmCsStageRegisters cs = {0};
    sceGnmCsRegsSetAddress(&cs, code);
    cs.computepgmrsrc1 = 2u | (3u << 6) | (0xc0u << 12);
    cs.computepgmrsrc2 = (12u << 1) | (1u << 7);
    cs.computenumthreadx = 64;
    cs.computenumthready = cs.computenumthreadz = 1;
    GnmTexture* textures = st_arena_alloc(&arena, 64 * sizeof(GnmTexture), 65536);
    for (unsigned t = 0; t < 64; ++t) {
        GnmTextureCreateInfo info = {.format = GNM_FMT_R32G32B32A32_FLOAT,
                                     .texturetype = GNM_TEXTURE_2D,
                                     .width = 64,
                                     .height = 64,
                                     .depth = 1,
                                     .pitch = 64,
                                     .nummiplevels = 1,
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
        for (unsigned i = 0; i < bytes / sizeof(float); ++i)
            data[i] = (t + 1) / 128.f;
        sceGnmTexSetBaseAddress(&textures[t], data);
    }
    GnmBuffer table = st_raw_buffer(textures, 64 * sizeof(GnmTexture));
    table.stride = sizeof(GnmTexture);
    table.numrecords = 64;
    GnmSampler sampler = {0};
    StCase cases[2] = {{.name = "first_dispatch"}, {.name = "guards"}};
    volatile uint32_t* output = st_arena_alloc(&arena, (64 * 64 + 16) * 4, 65536);
    st_fill_words(output, 64 * 64 + 16, 0xdeadbeef);
    GnmBuffer result = st_raw_buffer(output + 8, 64 * 64 * 4);
    result.stride = 4;
    result.numrecords = 64 * 64;
    StDcb dcb;
    st_dcb_begin(&dcb, &queue);
    sceGnmDrawCmdSetCsShader(&dcb.cmd, &cs);
    sceGnmDrawCmdSetVsharpUserData(&dcb.cmd, GNM_STAGE_CS, 0, &table);
    sceGnmDrawCmdSetSsharpUserData(&dcb.cmd, GNM_STAGE_CS, 4, &sampler);
    sceGnmDrawCmdSetVsharpUserData(&dcb.cmd, GNM_STAGE_CS, 8, &result);
    st_dcb_barrier(&dcb, 0);
    sceGnmDrawCmdDispatchDirect(&dcb.cmd, 64, 1, 1, 0);
    st_dcb_barrier(&dcb, 0);
    if (!submit_and_wait(&dcb)) return st_fail("reason=submit");
    for (unsigned i = 0; i < 64 * 64 + 16; ++i) {
        uint32_t expected = 0xdeadbeef;
        unsigned which = 1;
        if (i >= 8 && i < 64 * 64 + 8) {
            unsigned group = (i - 8) / 64;
            which = 0;
            float value = (group + 1) / 128.f;
            memcpy(&expected, &value, 4);
        }
        st_case_expect(&cases[which], "word", i, expected, output[i]);
    }
    sceGnmSubmitDone();
    return st_finish_cases(cases, 2, "textures=64 first_use=1 guards=1");
}
