#define SHADTEST_NAME "runtime_image_loads"
#include <gnm_texture.h>
#include "array.h"
#include "control.h"
#include "control_array.h"
#include "planar.h"
#include "shadtest_guest.h"
#include "write.h"
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

static GnmCsStageRegisters shader(StArena* arena, const void* data, size_t size, unsigned users) {
    void* code = st_arena_alloc(arena, size, 256);
    memcpy(code, data, size);
    GnmCsStageRegisters cs = {0};
    sceGnmCsRegsSetAddress(&cs, code);
    cs.computepgmrsrc1 = 2u | (3u << 6) | (0xc0u << 12);
    cs.computepgmrsrc2 = (users << 1) | (1u << 7);
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
    GnmCsStageRegisters shaders[] = {
        shader(&arena, control_shader, sizeof(control_shader), 12),
        shader(&arena, planar_shader, sizeof(planar_shader), 8),
        shader(&arena, control_array_shader, sizeof(control_array_shader), 12),
        shader(&arena, array_shader, sizeof(array_shader), 8)};
    GnmCsStageRegisters writer = shader(&arena, write_shader, sizeof(write_shader), 9);
    GnmTexture textures[9] = {0};
    const GnmDataFormat formats[] = {
        GNM_FMT_R32_FLOAT,
        GNM_DATA_FORMAT_INIT(GNM_IMG_DATA_FORMAT_32, GNM_IMG_NUM_FORMAT_UINT, GNM_CHAN_X,
                             GNM_CHAN_CONSTANT0, GNM_CHAN_CONSTANT0, GNM_CHAN_CONSTANT1),
        GNM_DATA_FORMAT_INIT(GNM_IMG_DATA_FORMAT_32, GNM_IMG_NUM_FORMAT_SINT, GNM_CHAN_X,
                             GNM_CHAN_CONSTANT0, GNM_CHAN_CONSTANT0, GNM_CHAN_CONSTANT1)};
    const uint32_t values[] = {0x3e800000, 0x89abcdef, 0xf8a432eb};
    for (unsigned t = 0; t < 9; ++t) {
        GnmTextureCreateInfo info = {
            .format = formats[t % 3],
            .texturetype = t < 3 ? GNM_TEXTURE_2D : (t < 6 ? GNM_TEXTURE_3D : GNM_TEXTURE_2D_ARRAY),
            .width = 64,
            .height = 64,
            .depth = t >= 3 && t < 6 ? 4 : 1,
            .pitch = 64,
            .nummiplevels = 1,
            .numslices = t >= 6 ? 2 : 1,
            .numfragments = 1,
            .tilemodehint = GNM_TM_THIN_1D_THIN,
            .mingpumode = GNM_GPU_BASE};
        if (sceGnmCreateTexture(&textures[t], &info) != GNM_ERROR_OK)
            return st_fail("reason=texture_create");
        uint64_t bytes = 0;
        uint32_t alignment = 0;
        if (sceGnmTexCalcByteSize(&bytes, &alignment, &textures[t]) != GNM_ERROR_OK)
            return st_fail("reason=texture_size");
        uint32_t* data = st_arena_alloc(&arena, bytes, 65536);
        if (!data)
            return st_fail("reason=texture_memory");
        st_fill_words(data, bytes / 4, values[t % 3]);
        sceGnmTexSetBaseAddress(&textures[t], data);
        if (t >= 6)
            textures[t].lastarray = 1;
    }
    GnmTexture* descriptors = st_arena_alloc(&arena, 2 * sizeof(GnmTexture), 65536);
    volatile uint32_t* output = st_arena_alloc(&arena, 144 * 4, 65536);
    GnmBuffer result = st_raw_buffer(output + 8, 128 * 4);
    result.stride = 4;
    result.numrecords = 128;
    unsigned failures = 0;
    for (unsigned kind = 0; kind < 4; ++kind)
        for (unsigned dynamic = 0; dynamic < 2; ++dynamic)
            for (unsigned phase = 0; phase < 3; ++phase) {
                if (kind == 3 && !dynamic)
                    continue;
                const unsigned base = kind == 3 ? 0 : kind * 3;
                uint32_t expected_values[3];
                memcpy(expected_values, values, sizeof(values));
                if (kind == 3) {
                    for (unsigned t = 0; t < 3; ++t) {
                        expected_values[t] = values[(t + phase + 1) % 3];
                        StDcb write;
                        st_dcb_begin(&write, &queue);
                        sceGnmDrawCmdSetCsShader(&write.cmd, &writer);
                        sceGnmDrawCmdSetTsharpUserData(&write.cmd, GNM_STAGE_CS, 0, &textures[t]);
                        GnmBuffer value = {0};
                        memcpy(&value, &expected_values[t], sizeof(uint32_t));
                        sceGnmDrawCmdSetVsharpUserData(&write.cmd, GNM_STAGE_CS, 8, &value);
                        sceGnmDrawCmdDispatchDirect(&write.cmd, 64, 1, 1, 0);
                        st_dcb_barrier(&write, 0);
                        if (!submit_and_wait(&write))
                            return st_fail("reason=gpu_write");
                    }
                }
                descriptors[0] = textures[base + phase];
                descriptors[1] = textures[base + (phase + 1) % 3];
                st_fill_words(output, 144, 0xdeadbeef);
                GnmBuffer table = st_raw_buffer(descriptors, 2 * sizeof(GnmTexture));
                table.stride = sizeof(GnmTexture);
                table.numrecords = 2;
                StDcb dcb;
                st_dcb_begin(&dcb, &queue);
                sceGnmDrawCmdSetCsShader(&dcb.cmd, &shaders[dynamic + (kind == 2 ? 2 : 0)]);
                if (dynamic)
                    sceGnmDrawCmdSetVsharpUserData(&dcb.cmd, GNM_STAGE_CS, 0, &table);
                else
                    sceGnmDrawCmdSetTsharpUserData(&dcb.cmd, GNM_STAGE_CS, 0,
                                                   &textures[base + phase]);
                sceGnmDrawCmdSetVsharpUserData(&dcb.cmd, GNM_STAGE_CS, dynamic ? 4 : 8, &result);
                sceGnmDrawCmdDispatchDirect(&dcb.cmd, dynamic ? 2 : 1, 1, 1, 0);
                st_dcb_barrier(&dcb, 0);
                if (!submit_and_wait(&dcb))
                    return st_fail("reason=submit");
                unsigned mismatches = 0;
                for (unsigned i = 0; i < 144; ++i) {
                    uint32_t expected = 0xdeadbeef;
                    if (i >= 8 && i < 8 + (dynamic ? 128 : 64))
                        expected = expected_values[(phase + (dynamic ? (i - 8) / 64 : 0)) % 3];
                    if (output[i] != expected) {
                        if (!mismatches)
                            st_emit_marker(
                                "CHECK",
                                "kind=%u dynamic=%u phase=%u word=%u expected=%08x got=%08x", kind,
                                dynamic, phase, i, expected, output[i]);
                        ++mismatches;
                    }
                }
                st_emit_marker("CHECK", "kind=%u dynamic=%u phase=%u mismatches=%u", kind, dynamic,
                               phase, mismatches);
                failures += mismatches;
            }
    if (failures)
        return st_fail("mismatches=%u", failures);
    st_emit_marker("PASS", "numeric_types=3 dimensions=3 static_controls=9 runtime_cases=12 "
                           "gpu_updates=3 table_rewrite=1 guards=1");
    return 0;
}
