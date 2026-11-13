#define SHADTEST_NAME "runtime_compact_images"
#include <gnm_sampler.h>
#include <gnm_texture.h>
#include "control.h"
#include "padded.h"
#include "planar.h"
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
    cs.computepgmrsrc2 = (16u << 1) | (1u << 7);
    cs.computenumthreadx = 64;
    cs.computenumthready = cs.computenumthreadz = 1;
    return cs;
}

int main(void) {
    st_begin();
    StArena arena;
    StQueue queue;
    if (!st_arena_init(&arena, 4 * 1024 * 1024) || !st_queue_init(&arena, &queue))
        return st_fail("reason=allocation");
    GnmCsStageRegisters shaders[] = {shader(&arena, control_shader, sizeof(control_shader)),
                                     shader(&arena, planar_shader, sizeof(planar_shader)),
                                     shader(&arena, padded_shader, sizeof(padded_shader))};
    GnmTexture textures[2];
    for (unsigned t = 0; t < 2; ++t) {
        GnmTextureCreateInfo info = {.format = GNM_FMT_R32G32B32A32_FLOAT,
                                     .texturetype = GNM_TEXTURE_2D,
                                     .width = 64,
                                     .height = 64,
                                     .depth = 1,
                                     .pitch = 64,
                                     .nummiplevels = 1,
                                     .numslices = 1,
                                     .numfragments = 1,
                                     .tilemodehint = GNM_TM_THIN_1D_THIN,
                                     .mingpumode = GNM_GPU_BASE};
        if (sceGnmCreateTexture(&textures[t], &info) != GNM_ERROR_OK)
            return st_fail("reason=texture_create");
        uint64_t bytes = 0;
        uint32_t alignment = 0;
        if (sceGnmTexCalcByteSize(&bytes, &alignment, &textures[t]) != GNM_ERROR_OK)
            return st_fail("reason=texture_size");
        float* data = st_arena_alloc(&arena, bytes, 65536);
        if (!data)
            return st_fail("reason=texture_memory");
        for (unsigned i = 0; i < bytes / 4; ++i)
            data[i] = t ? .75f : .25f;
        sceGnmTexSetBaseAddress(&textures[t], data);
    }
    uint32_t* descriptors = st_arena_alloc(&arena, 64, 65536);
    volatile uint32_t* output = st_arena_alloc(&arena, 144 * 4, 65536);
    GnmBuffer result = st_raw_buffer(output + 8, 128 * 4);
    result.stride = 4;
    result.numrecords = 128;
    GnmSampler sampler = {0};
    unsigned failures = 0;
    for (unsigned stride = 16; stride <= 32; stride += 16) {
        for (unsigned dynamic = 0; dynamic < 2; ++dynamic) {
            for (unsigned phase = 0; phase < 2; ++phase) {
                st_fill_words(descriptors, 16, 0xffffffff);
                for (unsigned t = 0; t < 2; ++t)
                    memcpy((uint8_t*)descriptors + t * stride, &textures[t ^ phase], 16);
                st_fill_words(output, 144, 0xdeadbeef);
                GnmBuffer table = st_raw_buffer(descriptors, 2 * stride);
                table.stride = stride;
                table.numrecords = 2;
                StDcb dcb;
                st_dcb_begin(&dcb, &queue);
                sceGnmDrawCmdSetCsShader(&dcb.cmd, &shaders[dynamic ? stride / 16 : 0]);
                if (dynamic) {
                    sceGnmDrawCmdSetVsharpUserData(&dcb.cmd, GNM_STAGE_CS, 0, &table);
                } else
                    sceGnmDrawCmdSetTsharpUserData(&dcb.cmd, GNM_STAGE_CS, 0, &textures[phase]);
                sceGnmDrawCmdSetSsharpUserData(&dcb.cmd, GNM_STAGE_CS, dynamic ? 4 : 8, &sampler);
                sceGnmDrawCmdSetVsharpUserData(&dcb.cmd, GNM_STAGE_CS, dynamic ? 8 : 12, &result);
                sceGnmDrawCmdDispatchDirect(&dcb.cmd, dynamic ? 2 : 1, 1, 1, 0);
                st_dcb_barrier(&dcb, 0);
                if (!submit_and_wait(&dcb))
                    return st_fail("reason=submit");
                unsigned mismatches = 0;
                for (unsigned i = 0; i < 144; ++i) {
                    uint32_t expected = 0xdeadbeef;
                    if (i >= 8 && i < 8 + (dynamic ? 128 : 64))
                        expected =
                            (dynamic ? (((i - 8) / 64) ^ phase) : phase) ? 0x3f400000 : 0x3e800000;
                    if (output[i] != expected) {
                        if (!mismatches)
                            st_emit_marker("CHECK",
                                           "stride=%u dynamic=%u phase=%u word=%u "
                                           "expected=%08x got=%08x",
                                           stride, dynamic, phase, i, expected, output[i]);
                        ++mismatches;
                    }
                }
                st_emit_marker("CHECK", "stride=%u dynamic=%u phase=%u mismatches=%u", stride,
                               dynamic, phase, mismatches);
                failures += mismatches;
            }
        }
    }
    if (failures)
        return st_fail("mismatches=%u", failures);
    st_emit_marker("PASS", "strides=2 direct_controls=4 runtime_cases=4 "
                           "poisoned_padding=1 table_rewrite=1 guards=1");
    return 0;
}
