#define SHADTEST_NAME "runtime_buffer_table"
#include "shadtest_guest.h"
#include "copy.h"

enum { GROUPS = 3, WORDS = 256, GUARD = 16 };
static const uint32_t poison = 0xdeadbeefu;

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
    if (!st_arena_init(&arena, 2 * 1024 * 1024) || !st_queue_init(&arena, &queue)) {
        return st_fail("reason=allocation");
    }
    void* code = st_arena_alloc(&arena, sizeof(copy_shader), 256);
    memcpy(code, copy_shader, sizeof(copy_shader));
    GnmCsStageRegisters regs = {0};
    sceGnmCsRegsSetAddress(&regs, code);
    regs.computepgmrsrc1 = 1u << 6;
    regs.computepgmrsrc2 = (4u << 1) | (1u << 7);
    regs.computenumthreadx = 64;
    regs.computenumthready = 1;
    regs.computenumthreadz = 1;

    const uint32_t src_stride[GROUPS] = {1, 2, 3};
    const uint32_t dst_stride[GROUPS] = {3, 1, 2};
    const uint32_t src_count[GROUPS] = {17, 64, 0};
    const uint32_t dst_count[GROUPS] = {64, 23, 64};
    volatile uint32_t* source[GROUPS];
    volatile uint32_t* destination[GROUPS];
    GnmBuffer* descriptors = st_arena_alloc(&arena, GROUPS * 2 * sizeof(GnmBuffer), 65536);
    for (uint32_t group = 0; group < GROUPS; ++group) {
        source[group] = st_arena_alloc(&arena, WORDS * sizeof(uint32_t), 65536);
        destination[group] = st_arena_alloc(&arena, WORDS * sizeof(uint32_t), 65536);
        for (uint32_t i = 0; i < WORDS; ++i) {
            source[group][i] = 0x12340000u + group * 1024 + i;
            destination[group][i] = poison;
        }
        descriptors[group * 2] = st_raw_buffer(source[group] + GUARD, 0);
        descriptors[group * 2].stride = src_stride[group] * 4;
        descriptors[group * 2].numrecords = src_count[group];
        descriptors[group * 2 + 1] = st_raw_buffer(destination[group] + GUARD, 0);
        descriptors[group * 2 + 1].stride = dst_stride[group] * 4;
        descriptors[group * 2 + 1].numrecords = dst_count[group];
    }
    GnmBuffer table = st_raw_buffer(descriptors, GROUPS * 2 * sizeof(GnmBuffer));
    table.stride = 2 * sizeof(GnmBuffer);
    table.numrecords = GROUPS;

    StDcb dcb;
    st_dcb_begin(&dcb, &queue);
    sceGnmDrawCmdSetCsShader(&dcb.cmd, &regs);
    sceGnmDrawCmdSetVsharpUserData(&dcb.cmd, GNM_STAGE_CS, 0, &table);
    sceGnmDrawCmdDispatchDirect(&dcb.cmd, GROUPS, 1, 1, 0);
    st_dcb_barrier(&dcb, 0);
    if (!submit_and_wait(&dcb)) {
        return st_fail("reason=submit_or_timeout");
    }
    for (uint32_t group = 0; group < GROUPS; ++group) {
        for (uint32_t i = 0; i < WORDS; ++i) {
            uint32_t expected = poison;
            if (i >= GUARD && (i - GUARD) % dst_stride[group] == 0) {
                const uint32_t lane = (i - GUARD) / dst_stride[group];
                if (lane < 64 && lane < dst_count[group]) {
                    expected = lane < src_count[group]
                        ? 0x12340000u + group * 1024 + GUARD + lane * src_stride[group] : 0;
                }
            }
            if (destination[group][i] != expected) {
                return st_fail("reason=destination group=%u word=%u expected=%08x got=%08x",
                               group, i, expected, destination[group][i]);
            }
            if (source[group][i] != 0x12340000u + group * 1024 + i) {
                return st_fail("reason=source_modified group=%u word=%u", group, i);
            }
        }
    }
    st_emit_marker("PASS", "groups=3 runtime_stride=1 bounds=1 guards=1 empty_source=1");
    st_arena_destroy(&arena);
    return 0;
}
