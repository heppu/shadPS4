#define SHADTEST_NAME "shader_variant_reuse"
#include "shadtest_guest.h"
#include "copy.h"

enum { VARIANTS = 24, LANES = 64, WORDS = VARIANTS * LANES + 16, ROUNDS = 144 };
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
    if (!st_arena_init(&arena, 4 * 1024 * 1024) || !st_queue_init(&arena, &queue))
        return st_fail("reason=allocation");
    void* code = st_arena_alloc(&arena, sizeof(copy_shader), 256);
    memcpy(code, copy_shader, sizeof(copy_shader));
    GnmCsStageRegisters regs = {0};
    sceGnmCsRegsSetAddress(&regs, code);
    regs.computepgmrsrc1 = 1u << 6;
    regs.computepgmrsrc2 = (8u << 1) | (1u << 7);
    regs.computenumthreadx = 64;
    regs.computenumthready = regs.computenumthreadz = 1;
    uint32_t* source = st_arena_alloc(&arena, WORDS * 4, 65536);
    volatile uint32_t* output = st_arena_alloc(&arena, WORDS * 4, 65536);
    if (!source || !output)
        return st_fail("reason=buffers");
    StCase cases[3] = {{.name="variant_values"}, {.name="guards"}, {.name="source_preserved"}};
    uint64_t started = 0;
    for (unsigned round = 0; round < ROUNDS; ++round) {
        if (round == VARIANTS) started = sceKernelGetProcessTime();
        const unsigned stride = round < VARIANTS ? round + 1
            : round < VARIANTS * 3 ? VARIANTS
            : 1 + ((round * 7) % VARIANTS);
        for (unsigned i = 0; i < WORDS; ++i) {
            source[i] = 0x31415926u ^ (round * 0x9e3779b9u) ^ i;
            output[i] = 0xdeadbeefu;
        }
        GnmBuffer input = st_raw_buffer(source, LANES * stride * 4);
        GnmBuffer result = st_raw_buffer(output, LANES * stride * 4);
        input.stride = result.stride = stride * 4;
        input.numrecords = result.numrecords = LANES;
        StDcb dcb;
        st_dcb_begin(&dcb, &queue);
        sceGnmDrawCmdSetCsShader(&dcb.cmd, &regs);
        sceGnmDrawCmdSetVsharpUserData(&dcb.cmd, GNM_STAGE_CS, 0, &input);
        sceGnmDrawCmdSetVsharpUserData(&dcb.cmd, GNM_STAGE_CS, 4, &result);
        st_dcb_barrier(&dcb, 0);
        for (unsigned repeat = 0; repeat < 64; ++repeat) {
            sceGnmDrawCmdDispatchDirect(&dcb.cmd, 1, 1, 1, 0);
            st_dcb_barrier(&dcb, 0);
        }
        if (!submit_and_wait(&dcb))
            return st_fail("reason=submit round=%u", round);
        for (unsigned i = 0; i < WORDS; ++i) {
            const bool written = i % stride == 0 && i / stride < LANES;
            const uint32_t initial = 0x31415926u ^ (round * 0x9e3779b9u) ^ i;
            st_case_expect(&cases[written ? 0 : 1], "word", round * WORDS + i,
                           written ? initial ^ 0x5a5a1234u : 0xdeadbeefu, output[i]);
            st_case_expect(&cases[2], "word", round * WORDS + i, initial, source[i]);
        }
    }
    char detail[256];
    snprintf(detail, sizeof(detail), "variants=24 rounds=144 dispatches_per_round=64 repeated_last=48 alternating=72 guards=1 measured_us=%llu",
             (unsigned long long)(sceKernelGetProcessTime() - started));
    return st_finish_cases(cases, 3, detail);
}
