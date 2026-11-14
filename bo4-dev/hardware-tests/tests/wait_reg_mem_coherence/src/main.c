#define SHADTEST_NAME "wait_reg_mem_coherence"
#include "shadtest_guest.h"
#include "copy.h"

enum { ROUNDS = 512, WORDS = 64 };
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
    sceGnmDrawCmdDispatchDirect(&dcb->cmd, 1, 1, 1, 0);
}

static void wait_equal(StDcb* dcb, const void* address, uint32_t value, uint32_t mask) {
    const uint64_t addr = (uint64_t)(uintptr_t)address;
    const uint32_t packet[] = {
        PKT3(0x3c, 5, 0), 3u | (1u << 4), (uint32_t)addr, (uint32_t)(addr >> 32),
        value, mask, 10,
    };
    st_dcb_put(dcb, packet, sizeof(packet) / sizeof(packet[0]));
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
    uint32_t* source = st_arena_alloc(&arena, 4096, 65536);
    uint32_t* shared = st_arena_alloc(&arena, 4096, 65536);
    volatile uint32_t* returned = st_arena_alloc(&arena, 4096, 65536);
    if (!source || !shared || !returned)
        return st_fail("reason=buffers");
    StCase cases[3] = {{.name="gpu_poll"}, {.name="gpu_reuse"}, {.name="neighbor_guard"}};
    const uint64_t started = sceKernelGetProcessTime();
    for (unsigned round = 0; round < ROUNDS; ++round) {
        for (unsigned i = 0; i < WORDS; ++i)
            source[i] = 0x12345678u ^ (round * 0x9e3779b9u) ^ i;
        shared[128] = 0xface0000u | round;
        shared[64] = 0xdead0000u | round;
        StDcb dcb;
        st_dcb_begin(&dcb, &queue);
        st_dcb_barrier(&dcb, 0);
        copy(&dcb, &regs, source, shared);
        st_dcb_barrier(&dcb, 0);
        wait_equal(&dcb, shared + 128, round, 0xffff);
        copy(&dcb, &regs, shared, (const void*)returned);
        st_dcb_barrier(&dcb, 0);
        wait_equal(&dcb, shared, source[0] ^ 0x5a5a1234u, 0xffffffffu);
        if (!submit_and_wait(&dcb))
            return st_fail("reason=submit round=%u", round);
        for (unsigned i = 0; i < WORDS; ++i) {
            st_case_expect(&cases[0], "written", round * WORDS + i,
                           source[i] ^ 0x5a5a1234u, ((volatile uint32_t*)shared)[i]);
            st_case_expect(&cases[1], "reused", round * WORDS + i, source[i], returned[i]);
        }
        st_case_expect(&cases[2], "label", round, 0xface0000u | round, shared[128]);
        st_case_expect(&cases[2], "guard", round, 0xdead0000u | round, shared[64]);
    }
    char detail[192];
    snprintf(detail, sizeof(detail),
        "rounds=512 cpu_poll_shared_page=1 gpu_poll=1 masked_compare=1 gpu_reuse=1 elapsed_us=%llu",
        (unsigned long long)(sceKernelGetProcessTime() - started));
    return st_finish_cases(cases, 3, detail);
}
