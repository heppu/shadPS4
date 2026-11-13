#define SHADTEST_NAME "buffer_readback_threads"
#include "shadtest_guest.h"
#include "copy.h"
#include <stdatomic.h>

enum { THREADS = 4, ROUNDS = 48, WORDS = 4096, STRIDE = 1024 * 1024 };
static atomic_uint epoch, completed;
static volatile uint32_t* destinations[THREADS];
static StCase cases[THREADS + 2];

static uint32_t pattern(unsigned round, unsigned thread, unsigned word) {
    return 0x12345678u ^ (round * 0x9e3779b9u) ^ (thread << 24) ^ word;
}

static void* reader(void* opaque) {
    const unsigned thread = (unsigned)(uintptr_t)opaque;
    for (unsigned round = 0; round < ROUNDS; ++round) {
        while (atomic_load_explicit(&epoch, memory_order_acquire) != round + 1)
            __asm__ volatile("pause");
        volatile uint32_t* words = destinations[thread];
        const uint32_t changed = pattern(round, thread, 0) ^ 0x5a5a1234u ^ 0x00ff00ffu;
        if (thread & 1)
            words[0] = changed;
        for (unsigned i = 0; i < WORDS; ++i) {
            const uint32_t expected = (thread & 1) && i == 0 ? changed
                : pattern(round, thread, i) ^ 0x5a5a1234u;
            st_case_expect(&cases[thread], "read", round * WORDS + i, expected, words[i]);
        }
        words[0] = changed;
        atomic_fetch_add_explicit(&completed, 1, memory_order_release);
    }
    return NULL;
}

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
    return st_wait_label(dcb->queue->label, value);
}

static void copy(StDcb* dcb, const GnmCsStageRegisters* regs, const void* src, const void* dst) {
    GnmBuffer source = st_raw_buffer(src, WORDS * 4);
    GnmBuffer destination = st_raw_buffer(dst, WORDS * 4);
    source.stride = destination.stride = 4;
    source.numrecords = destination.numrecords = WORDS;
    sceGnmDrawCmdSetCsShader(&dcb->cmd, regs);
    sceGnmDrawCmdSetVsharpUserData(&dcb->cmd, GNM_STAGE_CS, 0, &source);
    sceGnmDrawCmdSetVsharpUserData(&dcb->cmd, GNM_STAGE_CS, 4, &destination);
    sceGnmDrawCmdDispatchDirect(&dcb->cmd, WORDS / 64, 1, 1, 0);
}

int main(void) {
    st_begin();
    StArena arena;
    StQueue queue;
    if (!st_arena_init(&arena, 16 * 1024 * 1024) || !st_queue_init(&arena, &queue))
        return st_fail("reason=allocation");
    void* code = st_arena_alloc(&arena, sizeof(copy_shader), 256);
    memcpy(code, copy_shader, sizeof(copy_shader));
    GnmCsStageRegisters regs = {0};
    sceGnmCsRegsSetAddress(&regs, code);
    regs.computepgmrsrc1 = 1u << 6;
    regs.computepgmrsrc2 = (8u << 1) | (1u << 7);
    regs.computenumthreadx = 64;
    regs.computenumthready = regs.computenumthreadz = 1;
    uint32_t* sources[THREADS];
    uint32_t* returned[THREADS];
    OrbisPthread threads[THREADS];
    const char* names[] = {"reader0", "writer1", "reader2", "writer3", "roundtrip", "guards"};
    for (unsigned i = 0; i < THREADS + 2; ++i)
        cases[i].name = names[i];
    for (unsigned t = 0; t < THREADS; ++t) {
        sources[t] = st_arena_alloc(&arena, STRIDE, STRIDE);
        destinations[t] = st_arena_alloc(&arena, STRIDE, STRIDE);
        returned[t] = st_arena_alloc(&arena, STRIDE, STRIDE);
        if (!sources[t] || !destinations[t] || !returned[t])
            return st_fail("reason=buffers");
        destinations[t][WORDS] = 0xdead0000u | t;
        if (scePthreadCreate(&threads[t], NULL, reader, (void*)(uintptr_t)t, names[t]) != 0)
            return st_fail("reason=thread");
    }
    for (unsigned round = 0; round < ROUNDS; ++round) {
        StDcb dcb;
        st_dcb_begin(&dcb, &queue);
        st_dcb_barrier(&dcb, 0);
        for (unsigned t = 0; t < THREADS; ++t) {
            for (unsigned i = 0; i < WORDS; ++i)
                sources[t][i] = pattern(round, t, i);
            copy(&dcb, &regs, sources[t], (const void*)destinations[t]);
        }
        st_dcb_barrier(&dcb, 0);
        if (!submit_and_wait(&dcb))
            return st_fail("reason=download_submit round=%u", round);
        atomic_store_explicit(&epoch, round + 1, memory_order_release);
        while (atomic_load_explicit(&completed, memory_order_acquire) != (round + 1) * THREADS)
            __asm__ volatile("pause");
        st_dcb_begin(&dcb, &queue);
        st_dcb_barrier(&dcb, 0);
        for (unsigned t = 0; t < THREADS; ++t)
            copy(&dcb, &regs, (const void*)destinations[t], returned[t]);
        st_dcb_barrier(&dcb, 0);
        if (!submit_and_wait(&dcb))
            return st_fail("reason=upload_submit round=%u", round);
        for (unsigned t = 0; t < THREADS; ++t) {
            for (unsigned i = 0; i < WORDS; ++i) {
                const uint32_t expected = pattern(round, t, i) ^ (i == 0 ? 0x00ff00ffu : 0);
                st_case_expect(&cases[THREADS], "returned", (round * THREADS + t) * WORDS + i,
                               expected, returned[t][i]);
            }
            st_case_expect(&cases[THREADS + 1], "neighbor", round * THREADS + t,
                           0xdead0000u | t, destinations[t][WORDS]);
        }
    }
    for (unsigned t = 0; t < THREADS; ++t)
        scePthreadJoin(threads[t], NULL);
    return st_finish_cases(cases, THREADS + 2,
        "threads=4 rounds=48 read_first=2 write_first=2 roundtrip=1 guards=1");
}
