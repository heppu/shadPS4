#define SHADTEST_NAME "subrect_format_alias"
#include <gnm_texture.h>
#include "read.h"
#include "shadtest_guest.h"
#include "write.h"

enum { LANES = 64, OUTPUT_WORDS = LANES * 4 + 16 };
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
    if (!st_arena_init(&arena, 4 * 1024 * 1024) || !st_queue_init(&arena, &queue))
        return st_fail("reason=allocation");
    GnmCsStageRegisters writer = shader(&arena, write_shader, sizeof(write_shader), LANES);
    GnmCsStageRegisters reader = shader(&arena, read_shader, sizeof(read_shader), LANES);
    volatile uint32_t* output = st_arena_alloc(&arena, OUTPUT_WORDS * 4, 65536);
    GnmBuffer result = st_raw_buffer(output, LANES * 16);
    result.stride = 16;
    result.numrecords = LANES;
    for (unsigned format = 0; format < 2; ++format) {
        for (unsigned warmup = 0; warmup < 2; ++warmup) {
            void* pixels = st_arena_alloc(&arena, 65536, 65536);
            memset(pixels, 0, 65536);
            GnmTexture parent = {0};
            const GnmTextureCreateInfo info = {
                .format = GNM_DATA_FORMAT_INIT(
                    format ? GNM_IMG_DATA_FORMAT_8_8 : GNM_IMG_DATA_FORMAT_8,
                    GNM_IMG_NUM_FORMAT_UINT, GNM_CHAN_X, format ? GNM_CHAN_Y : GNM_CHAN_CONSTANT0,
                    GNM_CHAN_CONSTANT0, GNM_CHAN_CONSTANT1),
                .texturetype = GNM_TEXTURE_2D,
                .width = 128,
                .height = 128,
                .depth = 1,
                .pitch = 128,
                .nummiplevels = 1,
                .numslices = 1,
                .numfragments = 1,
                .tilemodehint = GNM_TM_THIN_2D_THIN,
                .mingpumode = GNM_GPU_BASE,
            };
            if (sceGnmCreateTexture(&parent, &info) != GNM_ERROR_OK)
                return st_fail("reason=texture_creation");
            sceGnmTexSetBaseAddress(&parent, pixels);
            GnmTexture cropped = parent;
            cropped.height = 119;
            cropped.numformat = GNM_IMG_NUM_FORMAT_UNORM;
            for (unsigned phase = 0; phase < 3; ++phase) {
                if (phase == 0 && !warmup)
                    continue;
                if (phase) {
                    GnmBuffer value = {0};
                    ((uint32_t*)&value)[0] = phase == 1 ? 255 : 0;
                    ((uint32_t*)&value)[1] = phase == 2 ? 255 : 0;
                    StDcb dcb;
                    st_dcb_begin(&dcb, &queue);
                    sceGnmDrawCmdSetCsShader(&dcb.cmd, &writer);
                    sceGnmDrawCmdSetTsharpUserData(&dcb.cmd, GNM_STAGE_CS, 0, &parent);
                    sceGnmDrawCmdSetVsharpUserData(&dcb.cmd, GNM_STAGE_CS, 8, &value);
                    sceGnmDrawCmdDispatchDirect(&dcb.cmd, 1, 1, 1, 0);
                    st_dcb_barrier(&dcb, 0);
                    if (!submit_and_wait(&dcb))
                        return st_fail("reason=write_submit");
                }
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
                for (unsigned word = 0; word < OUTPUT_WORDS; ++word) {
                    const uint32_t expected[] = {128, 120, phase == 1 ? 0x3f800000 : 0,
                                                 format && phase == 2 ? 0x3f800000 : 0};
                    const uint32_t want = word < LANES * 4 ? expected[word % 4] : poison;
                    if (output[word] != want)
                        return st_fail(
                            "format=%u warmup=%u phase=%u word=%u expected=%08x got=%08x", format,
                            warmup, phase, word, want, output[word]);
                }
            }
        }
    }
    st_emit_marker("PASS", "formats=2 warmup_cases=2 phases=2 cropped_dimensions=1 guards=1");
    st_arena_destroy(&arena);
    return 0;
}
