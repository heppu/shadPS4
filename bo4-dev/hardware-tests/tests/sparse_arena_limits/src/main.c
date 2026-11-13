#define SHADTEST_NAME "sparse_arena_limits"
#include "copy.h"
#include "shadtest_guest.h"

enum { WORDS = 64, REGION_BYTES = 256 * 1024 };
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

static GnmBuffer view(const volatile void* ptr, uint32_t stride) {
    GnmBuffer b = st_raw_buffer(ptr, 0);
    b.stride = stride;
    b.numrecords = WORDS;
    return b;
}

static bool copy(StQueue* queue, const GnmCsStageRegisters* regs, GnmBuffer src, GnmBuffer dst) {
    StDcb dcb;
    st_dcb_begin(&dcb, queue);
    sceGnmDrawCmdSetCsShader(&dcb.cmd, regs);
    sceGnmDrawCmdSetVsharpUserData(&dcb.cmd, GNM_STAGE_CS, 0, &src);
    sceGnmDrawCmdSetVsharpUserData(&dcb.cmd, GNM_STAGE_CS, 4, &dst);
    sceGnmDrawCmdDispatchDirect(&dcb.cmd, 1, 1, 1, 0);
    st_dcb_barrier(&dcb, 0);
    return submit_and_wait(&dcb);
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
    regs.computepgmrsrc2 = 8u << 1;
    regs.computenumthreadx = WORDS;
    regs.computenumthready = regs.computenumthreadz = 1;
    volatile uint32_t* source = st_arena_alloc(&arena, 65536, 65536);
    volatile uint32_t* result = st_arena_alloc(&arena, 65536, 65536);
    uint32_t* expected = malloc(REGION_BYTES);
    if (!expected)
        return st_fail("reason=oracle_allocation");
    const uintptr_t bases[] = {0x27ffe0000ull, 0x2fffe0000ull};
    const uint32_t offsets[] = {0, 0x30000, 0, 0, 0x30000, 0x20000 - 128};
    const uint32_t strides[] = {4, 4, 4096, 4, 4, 4};
    for (uint32_t test = 0; test < 2; ++test) {
        for (uint32_t phase = 0; phase < 2; ++phase) {
            off_t phys;
            void* mapping = (void*)bases[test];
            if (!st_dmem_alloc_in(0, sceKernelGetDirectMemorySize(), REGION_BYTES, 65536, &phys) ||
                !st_dmem_map(&mapping, REGION_BYTES, phys, true))
                return st_fail("reason=fixed_mapping");
            volatile uint32_t* region = mapping;
            st_fill_words(region, REGION_BYTES / 4, poison);
            for (uint32_t i = 0; i < REGION_BYTES / 4; ++i)
                expected[i] = poison;
            for (uint32_t step = 0; step < 6; ++step) {
                for (uint32_t i = 0; i < WORDS; ++i) {
                    source[i] = 0x12340000u + phase * 10000 + step * 1000 + i;
                    expected[(offsets[step] + i * strides[step]) / 4] = source[i];
                }
                if (!copy(&queue, &regs, view(source, 4),
                          view((volatile uint8_t*)region + offsets[step], strides[step])))
                    return st_fail("reason=write_submit step=%u", step);
            }
            st_fill_words(result, 65536 / 4, poison);
            if (!copy(&queue, &regs, view(region, 4096), view(result, 4)))
                return st_fail("reason=read_submit");
            for (uint32_t i = 0; i < 65536 / 4; ++i) {
                uint32_t want = i < WORDS ? expected[i * 1024] : poison;
                if (result[i] != want)
                    return st_fail(
                        "reason=gpu_read case=%u phase=%u word=%u expected=%08x got=%08x", test,
                        phase, i, want, result[i]);
            }
            for (uint32_t i = 0; i < REGION_BYTES / 4; ++i)
                if (region[i] != expected[i])
                    return st_fail(
                        "reason=contents case=%u phase=%u word=%u expected=%08x got=%08x", test,
                        phase, i, expected[i], region[i]);
            sceKernelMunmap(mapping, REGION_BYTES);
            sceKernelReleaseDirectMemory(phys, REGION_BYTES);
        }
    }
    free(expected);
    st_emit_marker("PASS", "boundaries=2 phases=2 migration=1 reuse=1 guards=1");
    st_arena_destroy(&arena);
    return 0;
}
