#define SHADTEST_NAME "presentation_compute"
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
    regs.computepgmrsrc2 = 12u << 1;
    regs.computenumthreadx = 64;
    regs.computenumthready = regs.computenumthreadz = 1;
    uint32_t* source = st_arena_alloc(&arena, 256, 65536);
    volatile uint32_t* destination = st_arena_alloc(&arena, 144 * 4, 65536);
    for (unsigned i = 0; i < 64; ++i)
        source[i] = 100 + i;
    st_fill_words(destination, 144, 0xdeadbeef);
    GnmBuffer input = st_raw_buffer(source, 256);
    GnmBuffer output = st_raw_buffer(destination, 128 * 4);
    input.stride = output.stride = 4;
    input.numrecords = 64;
    output.numrecords = 128;
    StDcb dcb;
    st_dcb_begin(&dcb, &queue);
    sceGnmDrawCmdSetCsShader(&dcb.cmd, &regs);
    sceGnmDrawCmdSetPointerUserData(&dcb.cmd, GNM_STAGE_CS, 0, source);
    sceGnmDrawCmdSetVsharpUserData(&dcb.cmd, GNM_STAGE_CS, 4, &input);
    sceGnmDrawCmdSetVsharpUserData(&dcb.cmd, GNM_STAGE_CS, 8, &output);
    sceGnmDrawCmdDispatchDirect(&dcb.cmd, 1, 1, 1, 0);
    st_dcb_barrier(&dcb, 0);
    if (!submit_and_wait(&dcb))
        return st_fail("reason=submit_or_timeout");
    unsigned errors = 0;
    for (unsigned i = 0; i < 144; ++i) {
        uint32_t expected = i < 64 ? 828 : i < 128 ? 100 + i - 64 : 0xdeadbeef;
        if (destination[i] != expected) {
            if (!errors)
                st_emit_marker("DETAIL", "word=%u expected=%08x got=%08x", i, expected,
                               destination[i]);
            ++errors;
        }
    }
    if (errors)
        return st_fail("reason=values errors=%u", errors);
    sceKernelUsleep(2000000);
    st_emit_marker("PASS", "loop_sum=828 buffer_control=1 guards=1");
    st_arena_destroy(&arena);
    return 0;
}
