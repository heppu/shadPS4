#define SHADTEST_NAME "readfirstlane_exec"
#include <stdint.h>
#include "direct.h"
#include "loop.h"
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
    if (!dcb->ok || sceGnmSubmitCommandBuffers(1, commands, sizes, NULL, NULL) < 0 ||
        sceGnmSubmitDone() < 0) {
        return false;
    }
    return st_wait_label(dcb->queue->label, value);
}

static GnmCsStageRegisters shader(StArena* arena, const void* data, size_t size) {
    void* code = st_arena_alloc(arena, size, 256);
    memcpy(code, data, size);
    GnmCsStageRegisters cs = {0};
    sceGnmCsRegsSetAddress(&cs, code);
    cs.computepgmrsrc1 = 2u | (3u << 6) | (0xc0u << 12);
    cs.computepgmrsrc2 = (8u << 1) | (1u << 7);
    cs.computenumthreadx = 64;
    cs.computenumthready = cs.computenumthreadz = 1;
    return cs;
}

int main(void) {
    st_begin();
    StArena arena;
    StQueue queue;
    if (!st_arena_init(&arena, 32 * 1024 * 1024) || !st_queue_init(&arena, &queue))
        return st_fail("reason=allocation");
    GnmCsStageRegisters shaders[] = {
        shader(&arena, direct_shader, sizeof(direct_shader)),
        shader(&arena, loop_shader, sizeof(loop_shader)),
    };
    const uint64_t masks[] = {0,
                              UINT64_MAX,
                              1,
                              UINT64_C(1) << 1,
                              UINT64_C(1) << 31,
                              UINT64_C(1) << 32,
                              UINT64_C(1) << 63,
                              UINT64_C(0xaaaaaaaaaaaaaaaa),
                              UINT64_C(0xffffffff00000000),
                              UINT64_C(0x8000000000000001)};
    const unsigned key_masks[] = {0, 1, 3, 63};
    volatile uint32_t* output = st_arena_alloc(&arena, 272 * 4, 65536);
    GnmBuffer result = st_raw_buffer(output + 8, 256 * 4);
    result.stride = 4;
    result.numrecords = 256;
    unsigned failures = 0, cases = 0;
    for (unsigned loop = 0; loop < 2; ++loop) {
        for (unsigned phase = 0; phase < 10; ++phase) {
            for (unsigned group = 0; group < (loop ? 4 : 1); ++group) {
                uint64_t mask = masks[phase], keys = 0;
                unsigned first = 0, iterations = 0;
                for (unsigned lane = 0; lane < 64; ++lane) {
                    if ((mask >> lane) & 1) {
                        if (!keys)
                            first = lane;
                        keys |= UINT64_C(1) << (lane & key_masks[group]);
                    }
                }
                for (unsigned lane = 0; lane < 64; ++lane)
                    iterations += (keys >> lane) & 1;
                uint32_t words[] = {(uint32_t)mask, mask >> 32, key_masks[group], 0};
                GnmBuffer operands;
                memcpy(&operands, words, sizeof(words));
                st_fill_words(output, 272, 0xdeadbeef);
                StDcb dcb;
                st_dcb_begin(&dcb, &queue);
                sceGnmDrawCmdSetCsShader(&dcb.cmd, &shaders[loop]);
                sceGnmDrawCmdSetVsharpUserData(&dcb.cmd, GNM_STAGE_CS, 0, &operands);
                sceGnmDrawCmdSetVsharpUserData(&dcb.cmd, GNM_STAGE_CS, 4, &result);
                sceGnmDrawCmdDispatchDirect(&dcb.cmd, 1, 1, 1, 0);
                st_dcb_barrier(&dcb, 0);
                if (!submit_and_wait(&dcb))
                    return st_fail("reason=submit");
                unsigned mismatches = 0;
                for (unsigned i = 0; i < 272; ++i) {
                    uint32_t expected = 0xdeadbeef;
                    if (i >= 8 && i < 264) {
                        unsigned lane = (i - 8) / 4;
                        switch ((i - 8) % 4) {
                        case 0:
                            expected =
                                loop ? ((mask >> lane) & 1 ? lane & key_masks[group] : 0x55555555)
                                     : 256 + first;
                            break;
                        case 1:
                            expected = loop ? iterations : 0;
                            break;
                        default:
                            expected = 0;
                            break;
                        }
                    }
                    if (output[i] != expected) {
                        if (!mismatches)
                            st_emit_marker(
                                "CHECK", "loop=%u mask=%u group=%u word=%u expected=%08x got=%08x",
                                loop, phase, group, i, expected, output[i]);
                        ++mismatches;
                    }
                }
                st_emit_marker("CHECK", "loop=%u mask=%u group=%u mismatches=%u", loop, phase,
                               group, mismatches);
                failures += mismatches;
                ++cases;
            }
        }
    }
    if (failures)
        return st_fail("cases=%u mismatches=%u", cases, failures);
    st_emit_marker("PASS", "cases=%u direct=10 loops=40 zero_exec=1 high_lane=63 limit=65 guards=1",
                   cases);
    st_arena_destroy(&arena);
    return 0;
}
