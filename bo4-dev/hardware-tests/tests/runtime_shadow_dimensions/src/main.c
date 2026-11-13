#define SHADTEST_NAME "runtime_shadow_dimensions"
#include <gnm_sampler.h>
#include <gnm_texture.h>
#include <stdint.h>
#include "array.h"
#include "control.h"
#include "control_array.h"
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

static GnmCsStageRegisters shader(StArena* arena, const void* data, size_t size, bool direct) {
    void* code = st_arena_alloc(arena, size, 256);
    memcpy(code, data, size);
    GnmCsStageRegisters cs = {0};
    sceGnmCsRegsSetAddress(&cs, code);
    cs.computepgmrsrc1 = 2u | (3u << 6) | (0xc0u << 12);
    cs.computepgmrsrc2 = ((direct ? 16u : 12u) << 1) | (1u << 7);
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
        shader(&arena, control_shader, sizeof(control_shader), true),
        shader(&arena, planar_shader, sizeof(planar_shader), false),
        shader(&arena, control_array_shader, sizeof(control_array_shader), true),
        shader(&arena, array_shader, sizeof(array_shader), false),
    };
    volatile uint32_t* output = st_arena_alloc(&arena, 272 * 4, 65536);
    GnmBuffer result = st_raw_buffer(output + 8, 256 * 4);
    result.stride = 4;
    result.numrecords = 256;
    GnmSampler sampler = {.depthcomparefunc = GNM_DEPTH_COMPARE_LESS};
    unsigned failures = 0;
    for (unsigned array = 0; array < 2; ++array) {
        GnmTexture* textures = st_arena_alloc(&arena, 4 * sizeof(GnmTexture), 65536);
        memset(textures, 0, 4 * sizeof(GnmTexture));
        for (unsigned t = 0; t < 4; ++t) {
            const bool cube = (t & 1) != 0;
            GnmTextureCreateInfo info = {
                .format = GNM_FMT_R32_FLOAT,
                .texturetype = cube    ? GNM_TEXTURE_CUBEMAP
                               : array ? GNM_TEXTURE_2D_ARRAY
                                       : GNM_TEXTURE_2D,
                .width = 64,
                .height = 64,
                .depth = 1,
                .pitch = 64,
                .nummiplevels = 1,
                .numslices = !cube && array ? 2 : 1,
                .numfragments = 1,
                .tilemodehint = GNM_TM_DISPLAY_LINEAR_ALIGNED,
                .mingpumode = GNM_GPU_BASE,
            };
            if (sceGnmCreateTexture(&textures[t], &info) != GNM_ERROR_OK)
                return st_fail("reason=texture_create");
            uint64_t bytes = 0;
            uint32_t alignment = 0;
            if (sceGnmTexCalcByteSize(&bytes, &alignment, &textures[t]) != GNM_ERROR_OK)
                return st_fail("reason=texture_size");
            float* data = st_arena_alloc(&arena, bytes, 65536);
            if (!data)
                return st_fail("reason=texture_memory");
            unsigned pitch = sceGnmTexGetPitch(&textures[t]);
            for (unsigned i = 0; i < bytes / sizeof(float); ++i) {
                unsigned slice = i / (pitch * 64);
                data[i] =
                    ((t == 1 || t == 2) ^ ((cube || array) && (slice & 1)) ^ ((i % pitch) < 32))
                        ? .75f
                        : .25f;
            }
            sceGnmTexSetBaseAddress(&textures[t], data);
        }
        for (unsigned dynamic = 0; dynamic < 2; ++dynamic) {
            for (unsigned phase = 0; phase < (dynamic ? 2 : 4); ++phase) {
                if (dynamic && phase) {
                    for (unsigned t = 0; t < 2; ++t) {
                        GnmTexture tmp = textures[t];
                        textures[t] = textures[t + 2];
                        textures[t + 2] = tmp;
                    }
                }
                st_fill_words(output, 272, 0xdeadbeef);
                GnmBuffer table = st_raw_buffer(textures, 4 * sizeof(GnmTexture));
                table.stride = sizeof(GnmTexture);
                table.numrecords = 4;
                StDcb dcb;
                st_dcb_begin(&dcb, &queue);
                sceGnmDrawCmdSetCsShader(&dcb.cmd, &shaders[array * 2 + dynamic]);
                if (dynamic)
                    sceGnmDrawCmdSetVsharpUserData(&dcb.cmd, GNM_STAGE_CS, 0, &table);
                else
                    sceGnmDrawCmdSetTsharpUserData(&dcb.cmd, GNM_STAGE_CS, 0, &textures[phase]);
                sceGnmDrawCmdSetSsharpUserData(&dcb.cmd, GNM_STAGE_CS, dynamic ? 4 : 8, &sampler);
                sceGnmDrawCmdSetVsharpUserData(&dcb.cmd, GNM_STAGE_CS, dynamic ? 8 : 12, &result);
                sceGnmDrawCmdDispatchDirect(&dcb.cmd, dynamic ? 4 : 1, 1, 1, 0);
                st_dcb_barrier(&dcb, 0);
                if (!submit_and_wait(&dcb))
                    return st_fail("reason=submit");
                unsigned mismatches = 0;
                for (unsigned i = 0; i < 272; ++i) {
                    uint32_t expected = 0xdeadbeef;
                    if (i >= 8 && i < 8 + (dynamic ? 256 : 64)) {
                        unsigned t = dynamic ? (((i - 8) / 64) ^ (phase * 2)) : phase;
                        bool pass = (t == 1 || t == 2) ^ (((t & 1) || array) && ((i - 8) & 1));
                        expected = pass ? 0x3f800000 : 0;
                    }
                    if (output[i] != expected) {
                        if (!mismatches)
                            st_emit_marker(
                                "CHECK",
                                "array=%u dynamic=%u phase=%u word=%u expected=%08x got=%08x",
                                array, dynamic, phase, i, expected, output[i]);
                        ++mismatches;
                    }
                }
                st_emit_marker("CHECK", "array=%u dynamic=%u phase=%u mismatches=%u", array,
                               dynamic, phase, mismatches);
                failures += mismatches;
            }
        }
    }
    if (failures)
        return st_fail("mismatches=%u", failures);
    st_emit_marker("PASS", "layouts=2 static_controls=8 dynamic_cases=4 table_rewrite=1 "
                           "cube_faces=2 array_layers=2 guards=1");
    st_arena_destroy(&arena);
    return 0;
}
