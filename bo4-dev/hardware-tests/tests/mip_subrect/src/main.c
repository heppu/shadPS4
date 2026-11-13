#define SHADTEST_NAME "mip_subrect"
#include <gnm_texture.h>
#include "read.h"
#include "shadtest_guest.h"
#include "write.h"

enum { LEVELS = 9, ROW_WORDS = 8, OUTPUT_WORDS = LEVELS * ROW_WORDS };
static const uint32_t poison = 0xdeadbeefu;

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
    if (!dcb->ok || sceGnmSubmitCommandBuffers(1, commands, sizes, NULL, NULL) < 0 ||
        sceGnmSubmitDone() < 0) {
        return false;
    }
    return st_wait_label(dcb->queue->label, value);
}

static GnmCsStageRegisters shader(StArena* arena, const void* bytes, size_t size,
                                  uint32_t threads) {
    void* code = st_arena_alloc(arena, size, 256);
    memcpy(code, bytes, size);
    GnmCsStageRegisters regs = {0};
    sceGnmCsRegsSetAddress(&regs, code);
    regs.computepgmrsrc1 = (1u << 6) | 2u;
    regs.computepgmrsrc2 = 12u << 1;
    regs.computenumthreadx = threads;
    regs.computenumthready = 1;
    regs.computenumthreadz = 1;
    return regs;
}

int main(void) {
    st_begin();
    StArena arena;
    StQueue queue;
    if (!st_arena_init(&arena, 4 * 1024 * 1024) || !st_queue_init(&arena, &queue)) {
        return st_fail("reason=allocation");
    }
    GnmCsStageRegisters writer = shader(&arena, write_shader, sizeof(write_shader), 1);
    GnmCsStageRegisters reader = shader(&arena, read_shader, sizeof(read_shader), 8);
    void* pixels = st_arena_alloc(&arena, 1024 * 1024, 65536);
    memset(pixels, 0, 1024 * 1024);
    GnmTexture parent = {0};
    const GnmTextureCreateInfo info = {
        .format = GNM_FMT_R32_FLOAT,
        .texturetype = GNM_TEXTURE_2D,
        .width = 256,
        .height = 128,
        .depth = 1,
        .pitch = 512,
        .nummiplevels = LEVELS,
        .numslices = 1,
        .numfragments = 1,
        .tilemodehint = GNM_TM_DISPLAY_LINEAR_ALIGNED,
        .mingpumode = GNM_GPU_BASE,
    };
    if (sceGnmCreateTexture(&parent, &info) != GNM_ERROR_OK) {
        return st_fail("reason=texture_creation");
    }
    sceGnmTexSetBaseAddress(&parent, pixels);
    volatile uint32_t* output = st_arena_alloc(&arena, OUTPUT_WORDS * 4, 65536);
    GnmBuffer result = st_raw_buffer(output, OUTPUT_WORDS * 4);
    result.stride = ROW_WORDS * 4;
    result.numrecords = LEVELS;
    const uint32_t widths[] = {128, 128, 64, 256};
    const uint32_t levels[] = {4, 8, 8, 9};
    for (uint32_t phase = 0; phase < 2; ++phase) {
        for (uint32_t mip = 0; mip < LEVELS; ++mip) {
            GnmTexture target = parent;
            target.baselevel = mip;
            GnmBuffer value = {0};
            ((uint32_t*)&value)[0] = 64 + phase * 64 + mip;
            StDcb dcb;
            st_dcb_begin(&dcb, &queue);
            sceGnmDrawCmdSetCsShader(&dcb.cmd, &writer);
            sceGnmDrawCmdSetTsharpUserData(&dcb.cmd, GNM_STAGE_CS, 0, &target);
            sceGnmDrawCmdSetVsharpUserData(&dcb.cmd, GNM_STAGE_CS, 8, &value);
            sceGnmDrawCmdDispatchDirect(&dcb.cmd, 1, 1, 1, 0);
            st_dcb_barrier(&dcb, 0);
            if (!submit_and_wait(&dcb))
                return st_fail("reason=write_submit mip=%u", mip);
        }
        for (uint32_t test = 0; test < 4; ++test) {
            GnmTexture cropped = parent;
            cropped.width = widths[test] - 1;
            cropped.lastlevel = levels[test] - 1;
            reader.computenumthreadx = levels[test];
            st_fill_words(output, OUTPUT_WORDS, poison);
            StDcb dcb;
            st_dcb_begin(&dcb, &queue);
            sceGnmDrawCmdSetCsShader(&dcb.cmd, &reader);
            sceGnmDrawCmdSetTsharpUserData(&dcb.cmd, GNM_STAGE_CS, 0, &cropped);
            sceGnmDrawCmdSetVsharpUserData(&dcb.cmd, GNM_STAGE_CS, 8, &result);
            sceGnmDrawCmdDispatchDirect(&dcb.cmd, 1, 1, 1, 0);
            st_dcb_barrier(&dcb, 0);
            if (!submit_and_wait(&dcb))
                return st_fail("reason=read_submit");
            for (uint32_t mip = 0; mip < LEVELS; ++mip) {
                uint32_t expected[ROW_WORDS];
                for (uint32_t word = 0; word < ROW_WORDS; ++word)
                    expected[word] = poison;
                if (mip < levels[test]) {
                    expected[0] = widths[test] >> mip;
                    if (!expected[0])
                        expected[0] = 1;
                    expected[1] = 128 >> mip;
                    if (!expected[1])
                        expected[1] = 1;
                    const float value = (float)(64 + phase * 64 + mip);
                    memcpy(&expected[2], &value, 4);
                }
                for (uint32_t word = 0; word < ROW_WORDS; ++word) {
                    uint32_t got = output[mip * ROW_WORDS + word];
                    if (got != expected[word])
                        return st_fail("phase=%u case=%u mip=%u word=%u expected=%08x got=%08x",
                                       phase, test, mip, word, expected[word], got);
                }
            }
        }
    }
    if (st_gnm_errors)
        return st_fail("reason=gnm_errors count=%u", st_gnm_errors);
    st_emit_marker("PASS", "views=4 phases=2 mip_contents=1 dimensions=1 guards=1");
    st_arena_destroy(&arena);
    return 0;
}
