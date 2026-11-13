#define SHADTEST_NAME "scalar_mask_flags"
#include <stdint.h>
#include "and.h"
#include "andn2.h"
#include "ballot.h"
#include "nand.h"
#include "nor.h"
#include "not.h"
#include "or.h"
#include "orn2.h"
#include "shadtest_guest.h"
#include "shift.h"
#include "xnor.h"
#include "xor.h"
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

static uint64_t expected_mask(unsigned op, uint64_t a, uint64_t b) {
    switch (op) {
    case 0:
        return a;
    case 1:
        return a & b;
    case 2:
        return ~(a & b);
    case 3:
        return a & ~b;
    case 4:
        return a | b;
    case 5:
        return ~(a | b);
    case 6:
        return a | ~b;
    case 7:
        return a ^ b;
    case 8:
        return ~(a ^ b);
    default:
        return ~a;
    }
}

int main(void) {
    st_begin();
    StArena arena;
    StQueue queue;
    if (!st_arena_init(&arena, 32 * 1024 * 1024) || !st_queue_init(&arena, &queue))
        return st_fail("reason=allocation");
    GnmCsStageRegisters shaders[] = {
        shader(&arena, shift_shader, sizeof(shift_shader)),
        shader(&arena, and_shader, sizeof(and_shader)),
        shader(&arena, nand_shader, sizeof(nand_shader)),
        shader(&arena, andn2_shader, sizeof(andn2_shader)),
        shader(&arena, or_shader, sizeof(or_shader)),
        shader(&arena, nor_shader, sizeof(nor_shader)),
        shader(&arena, orn2_shader, sizeof(orn2_shader)),
        shader(&arena, xor_shader, sizeof(xor_shader)),
        shader(&arena, xnor_shader, sizeof(xnor_shader)),
        shader(&arena, not_shader, sizeof(not_shader)),
        shader(&arena, ballot_shader, sizeof(ballot_shader)),
    };
    const char* names[] = {"shift", "and", "nand", "andn2", "or",    "nor",
                           "orn2",  "xor", "xnor", "not",   "ballot"};
    const uint64_t pairs[][2] = {
        {0, 0},
        {1, 1},
        {UINT64_C(1) << 63, UINT64_C(1) << 63},
        {1, 0},
        {UINT64_C(1) << 63, 0},
        {UINT64_C(0x5555555555555555), UINT64_C(0xaaaaaaaaaaaaaaaa)},
        {UINT64_MAX, UINT64_MAX},
    };
    const unsigned thresholds[] = {0, 1, 32, 63, 64};
    volatile uint32_t* output = st_arena_alloc(&arena, 272 * 4, 65536);
    GnmBuffer result = st_raw_buffer(output + 8, 256 * 4);
    result.stride = 4;
    result.numrecords = 256;
    unsigned failures = 0, cases = 0;
    for (unsigned op = 0; op < 11; ++op) {
        for (unsigned phase = 0; phase < (op == 10 ? 5 : 7); ++phase) {
            uint64_t a = op == 10 ? thresholds[phase] : pairs[phase][0];
            uint64_t b = op == 10 ? 0 : pairs[phase][1];
            uint64_t mask = op == 10 ? (a == 64 ? UINT64_MAX : (UINT64_C(1) << a) - 1)
                                     : expected_mask(op, a, b);
            uint32_t words[] = {(uint32_t)a, a >> 32, (uint32_t)b, b >> 32};
            GnmBuffer operands;
            memcpy(&operands, words, sizeof(words));
            st_fill_words(output, 272, 0xdeadbeef);
            StDcb dcb;
            st_dcb_begin(&dcb, &queue);
            sceGnmDrawCmdSetCsShader(&dcb.cmd, &shaders[op]);
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
                        expected = (uint32_t)mask;
                        break;
                    case 1:
                        expected = mask >> 32;
                        break;
                    case 2:
                        expected = mask != 0;
                        break;
                    case 3:
                        expected = mask ? (op == 10 ? (lane ^ 1) + 1 : 1) : 0;
                        break;
                    }
                }
                if (output[i] != expected) {
                    if (!mismatches)
                        st_emit_marker("CHECK", "op=%s phase=%u word=%u expected=%08x got=%08x",
                                       names[op], phase, i, expected, output[i]);
                    ++mismatches;
                }
            }
            st_emit_marker("CHECK", "op=%s phase=%u mismatches=%u", names[op], phase, mismatches);
            failures += mismatches;
            ++cases;
        }
    }
    if (failures)
        return st_fail("cases=%u mismatches=%u", cases, failures);
    st_emit_marker("PASS",
                   "cases=%u scalar_mask_ops=9 shift_controls=7 ballot_cases=5 lanes=64 guards=1",
                   cases);
    st_arena_destroy(&arena);
    return 0;
}
