#define SHADTEST_NAME "buffer_mapping_lifetimes"
#include "shadtest_guest.h"
#include "copy.h"

enum { REGION_BYTES = 65536, WORDS = REGION_BYTES / 4, MAPPING_BYTES = 4 * REGION_BYTES };
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

int main(void) {
    st_begin();
    StArena arena;
    StQueue queue;
    if (!st_arena_init(&arena, 2 * 1024 * 1024) || !st_queue_init(&arena, &queue))
        return st_fail("reason=allocation");
    void* code = st_arena_alloc(&arena, sizeof(copy_shader), 256);
    memcpy(code, copy_shader, sizeof(copy_shader));
    GnmCsStageRegisters regs = {0};
    sceGnmCsRegsSetAddress(&regs, code);
    regs.computepgmrsrc1 = 1u << 6;
    regs.computepgmrsrc2 = (8u << 1) | (1u << 7);
    regs.computenumthreadx = 64;
    regs.computenumthready = regs.computenumthreadz = 1;
    void* previous = NULL;
    off_t previous_physical = 0;
    StCase cases[3] = {{.name = "copies"}, {.name = "sources"}, {.name = "guards"}};
    for (unsigned life = 0; life < 12; ++life) {
        off_t physical;
        void* mapping = NULL;
        if (!st_dmem_alloc_in(0, (off_t)sceKernelGetDirectMemorySize(), MAPPING_BYTES,
                              REGION_BYTES, &physical) ||
            !st_dmem_map(&mapping, MAPPING_BYTES, physical, false))
            return st_fail("reason=mapping lifetime=%u", life);
        if (previous) {
            if (mapping == previous || sceKernelMunmap(previous, MAPPING_BYTES) != 0 ||
                sceKernelReleaseDirectMemory(previous_physical, MAPPING_BYTES) != 0)
                return st_fail("reason=retirement lifetime=%u", life);
        }
        volatile uint32_t* words = mapping;
        for (unsigned i = 0; i < 4 * WORDS; ++i)
            words[i] = i < WORDS ? (0x12345678u ^ (life * 0x9e3779b9u) ^ i)
                                 : 0xded00000u ^ (life << 16) ^ i;
        GnmBuffer source = st_raw_buffer(words, REGION_BYTES);
        source.stride = 4;
        source.numrecords = WORDS;
        GnmBuffer destination = st_raw_buffer(words + 2 * WORDS, REGION_BYTES);
        destination.stride = 4;
        destination.numrecords = WORDS;
        StDcb dcb;
        st_dcb_begin(&dcb, &queue);
        sceGnmDrawCmdSetCsShader(&dcb.cmd, &regs);
        sceGnmDrawCmdSetVsharpUserData(&dcb.cmd, GNM_STAGE_CS, 0, &source);
        sceGnmDrawCmdSetVsharpUserData(&dcb.cmd, GNM_STAGE_CS, 4, &destination);
        st_dcb_barrier(&dcb, 0);
        sceGnmDrawCmdDispatchDirect(&dcb.cmd, WORDS / 64, 1, 1, 0);
        st_dcb_barrier(&dcb, 0);
        if (!submit_and_wait(&dcb))
            return st_fail("reason=submit lifetime=%u", life);
        for (unsigned i = 0; i < 4 * WORDS; ++i) {
            const unsigned region = i / WORDS;
            const unsigned lane = i % WORDS;
            const uint32_t value = 0x12345678u ^ (life * 0x9e3779b9u) ^ lane;
            const uint32_t expected = region == 0 ? value : region == 2 ? value ^ 0x5a5a1234u
                                        : 0xded00000u ^ (life << 16) ^ i;
            st_case_expect(&cases[region == 0 ? 1 : region == 2 ? 0 : 2], "word",
                           life * 4 * WORDS + i, expected, words[i]);
        }
        previous = mapping;
        previous_physical = physical;
    }
    if (sceKernelMunmap(previous, MAPPING_BYTES) != 0 ||
        sceKernelReleaseDirectMemory(previous_physical, MAPPING_BYTES) != 0)
        return st_fail("reason=final_retirement");
    return st_finish_cases(cases, 3, "lifetimes=12 copies=1 source_preserved=1 guards=1");
}
