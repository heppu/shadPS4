#define SHADTEST_NAME "image_table_reuse"
#include <gnm_sampler.h>
#include <gnm_texture.h>
#include "copy.h"
#include "shadtest_guest.h"
enum { ENTRIES = 4096, LANES = 64, WORDS = ENTRIES * LANES + 16, ROUNDS = 132 };
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
    if (!dcb->ok || sceGnmSubmitCommandBuffers(1, commands, sizes, NULL, NULL) < 0 ||
        sceGnmSubmitDone() < 0) {
        return false;
    }
    const uint64_t deadline = sceKernelGetProcessTime() + (uint64_t)ST_EOP_WAIT_MS * 1000;
    do {
        for (unsigned poll = 0; poll < 1024; ++poll) {
            if (*dcb->queue->label == value) {
                __builtin_ia32_lfence();
                st_compiler_barrier();
                return true;
            }
            __builtin_ia32_pause();
        }
    } while (sceKernelGetProcessTime() < deadline);
    return false;
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
    GnmTexture* entries = st_arena_alloc(&arena, ENTRIES * sizeof(GnmTexture), 65536);
    if (!entries) return st_fail("reason=table_memory");
    for (unsigned i = 0; i < ENTRIES; ++i) entries[i] = textures[i % 64];
    GnmBuffer table = st_raw_buffer(entries, ENTRIES * sizeof(GnmTexture));
    table.stride = sizeof(GnmTexture);
    table.numrecords = ENTRIES;
    GnmSampler sampler = {0};
    StCase cases[2] = {{.name = "repeated_values"}, {.name = "guards"}};
    volatile uint32_t* output = st_arena_alloc(&arena, WORDS * 4, 65536);
    if (!output) return st_fail("reason=output_memory");
    GnmBuffer result = st_raw_buffer(output + 8, ENTRIES * LANES * 4);
    result.stride = 4;
    result.numrecords = ENTRIES * LANES;
    uint64_t measured = 0;
    for (unsigned round = 0; round < ROUNDS; ++round) {
        st_fill_words(output, WORDS, 0xdeadbeef);
        StDcb dcb;
        st_dcb_begin(&dcb, &queue);
        sceGnmDrawCmdSetCsShader(&dcb.cmd, &cs);
        sceGnmDrawCmdSetVsharpUserData(&dcb.cmd, GNM_STAGE_CS, 0, &table);
        sceGnmDrawCmdSetSsharpUserData(&dcb.cmd, GNM_STAGE_CS, 4, &sampler);
        sceGnmDrawCmdSetVsharpUserData(&dcb.cmd, GNM_STAGE_CS, 8, &result);
        st_dcb_barrier(&dcb, 0);
        sceGnmDrawCmdDispatchDirect(&dcb.cmd, ENTRIES, 1, 1, 0);
        st_dcb_barrier(&dcb, 0);
        const uint64_t started = sceKernelGetProcessTime();
        if (!submit_and_wait(&dcb)) return st_fail("reason=submit round=%u", round);
        if (round >= 4) measured += sceKernelGetProcessTime() - started;
        for (unsigned i = 0; i < WORDS; ++i) {
            uint32_t expected = 0xdeadbeef;
            unsigned which = 1;
            if (i >= 8 && i < WORDS - 8) {
                unsigned group = (i - 8) / LANES;
                which = 0;
                float value = ((group % 64) + 1) / 128.f;
                memcpy(&expected, &value, 4);
            }
            st_case_expect(&cases[which], "word", round * WORDS + i, expected, output[i]);
        }
    }
    char detail[256];
    snprintf(detail, sizeof(detail), "textures=64 entries=4096 rounds=132 warmup=4 guards=1 measured_us=%llu",
             (unsigned long long)measured);
    return st_finish_cases(cases, 2, detail);
}
