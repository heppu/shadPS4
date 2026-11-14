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
    uint32_t* backing[9];
    uint64_t backing_bytes[9];
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
        backing[t] = data;
        backing_bytes[t] = bytes;
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
    for (unsigned kind = 0; kind < 5; ++kind)
        for (unsigned dynamic = 0; dynamic < 2; ++dynamic)
            for (unsigned phase = 0; phase < 3; ++phase) {
                if (kind >= 3 && !dynamic)
                    continue;
                const unsigned base = kind >= 3 ? 0 : kind * 3;
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
                if (kind == 4) {
                    for (unsigned t = 0; t < 3; ++t) {
                        expected_values[t] = values[(t + phase + 2) % 3];
                        st_fill_words(backing[t], backing_bytes[t] / 4, expected_values[t]);
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
    GnmTexture mixed[2] = {textures[0], textures[0]};
    for (unsigned t = 0; t < 2; ++t) {
        uint32_t* data = st_arena_alloc(&arena, backing_bytes[0], 65536);
        if (!data)
            return st_fail("reason=mixed_memory");
        st_fill_words(data, backing_bytes[0] / 4, t ? 0x3f000000 : 0x3e800000);
        sceGnmTexSetBaseAddress(&mixed[t], data);
        descriptors[t] = mixed[t];
    }
    GnmBuffer mixed_table = st_raw_buffer(descriptors, 2 * sizeof(GnmTexture));
    mixed_table.stride = sizeof(GnmTexture);
    mixed_table.numrecords = 2;
    volatile uint32_t* mixed_output = st_arena_alloc(&arena, (8 + 4 * 128 + 8) * 4, 65536);
    if (!mixed_output)
        return st_fail("reason=mixed_output");
    st_fill_words(mixed_output, 8 + 4 * 128 + 8, 0xdeadbeef);
    StDcb repeated;
    st_dcb_begin(&repeated, &queue);
    for (unsigned pass = 0; pass < 4; ++pass) {
        if (pass >= 2) {
            sceGnmDrawCmdSetCsShader(&repeated.cmd, &writer);
            sceGnmDrawCmdSetTsharpUserData(&repeated.cmd, GNM_STAGE_CS, 0, &mixed[0]);
            GnmBuffer value = {0};
            uint32_t bits = pass == 2 ? 0x3f400000 : 0x3f800000;
            memcpy(&value, &bits, sizeof(bits));
            sceGnmDrawCmdSetVsharpUserData(&repeated.cmd, GNM_STAGE_CS, 8, &value);
            sceGnmDrawCmdDispatchDirect(&repeated.cmd, 64, 1, 1, 0);
            st_dcb_barrier(&repeated, 0);
        }
        GnmBuffer destination = st_raw_buffer(mixed_output + 8 + pass * 128, 128 * 4);
        destination.stride = 4;
        destination.numrecords = 128;
        sceGnmDrawCmdSetCsShader(&repeated.cmd, &shaders[1]);
        sceGnmDrawCmdSetVsharpUserData(&repeated.cmd, GNM_STAGE_CS, 0, &mixed_table);
        sceGnmDrawCmdSetVsharpUserData(&repeated.cmd, GNM_STAGE_CS, 4, &destination);
        sceGnmDrawCmdDispatchDirect(&repeated.cmd, 2, 1, 1, 0);
        st_dcb_barrier(&repeated, 0);
    }
    if (!submit_and_wait(&repeated))
        return st_fail("reason=mixed_submit");
    unsigned mixed_mismatches = 0;
    for (unsigned i = 0; i < 8 + 4 * 128 + 8; ++i) {
        uint32_t expected = 0xdeadbeef;
        if (i >= 8 && i < 8 + 4 * 128) {
            unsigned pass = (i - 8) / 128;
            expected = (i - 8) % 128 >= 64 ? 0x3f000000
                       : pass < 2 ? 0x3e800000
                       : pass == 2 ? 0x3f400000 : 0x3f800000;
        }
        if (mixed_output[i] != expected) {
            if (!mixed_mismatches)
                st_emit_marker("CHECK", "mixed word=%u expected=%08x got=%08x", i,
                               expected, mixed_output[i]);
            ++mixed_mismatches;
        }
    }
    st_emit_marker("CHECK", "mixed_repeated_reads=4 mismatches=%u", mixed_mismatches);
    failures += mixed_mismatches;
    GnmTexture* full_views = st_arena_alloc(&arena, 2 * sizeof(GnmTexture), 65536);
    GnmTexture* crop_views = st_arena_alloc(&arena, 2 * sizeof(GnmTexture), 65536);
    if (!full_views || !crop_views)
        return st_fail("reason=layout_descriptors");
    for (unsigned t = 0; t < 2; ++t) {
        uint32_t* data = st_arena_alloc(&arena, backing_bytes[0], 65536);
        if (!data)
            return st_fail("reason=layout_memory");
        st_fill_words(data, backing_bytes[0] / 4, t ? 0x3f000000 : 0x3e800000);
        full_views[t] = textures[0];
        sceGnmTexSetBaseAddress(&full_views[t], data);
        crop_views[t] = full_views[t];
    }
    sceGnmTexSetWidth(&crop_views[0], 32);
    sceGnmTexSetHeight(&crop_views[0], 32);
    volatile uint32_t* layout_output = st_arena_alloc(&arena, (8 + 4 * 128 + 8) * 4, 65536);
    if (!layout_output)
        return st_fail("reason=layout_output");
    st_fill_words(layout_output, 8 + 4 * 128 + 8, 0xdeadbeef);
    StDcb layout_reads;
    st_dcb_begin(&layout_reads, &queue);
    for (unsigned pass = 0; pass < 4; ++pass) {
        GnmBuffer table = st_raw_buffer(pass == 1 ? crop_views : full_views,
                                        2 * sizeof(GnmTexture));
        table.stride = sizeof(GnmTexture);
        table.numrecords = 2;
        GnmBuffer destination = st_raw_buffer(layout_output + 8 + pass * 128, 128 * 4);
        destination.stride = 4;
        destination.numrecords = 128;
        sceGnmDrawCmdSetCsShader(&layout_reads.cmd, &shaders[1]);
        sceGnmDrawCmdSetVsharpUserData(&layout_reads.cmd, GNM_STAGE_CS, 0, &table);
        sceGnmDrawCmdSetVsharpUserData(&layout_reads.cmd, GNM_STAGE_CS, 4, &destination);
        sceGnmDrawCmdDispatchDirect(&layout_reads.cmd, 2, 1, 1, 0);
        st_dcb_barrier(&layout_reads, 0);
    }
    if (!submit_and_wait(&layout_reads))
        return st_fail("reason=layout_submit");
    unsigned layout_mismatches = 0;
    for (unsigned i = 0; i < 8 + 4 * 128 + 8; ++i) {
        uint32_t expected = 0xdeadbeef;
        if (i >= 8 && i < 8 + 4 * 128)
            expected = (i - 8) % 128 >= 64 ? 0x3f000000 : 0x3e800000;
        if (layout_output[i] != expected) {
            if (!layout_mismatches)
                st_emit_marker("CHECK", "layout word=%u expected=%08x got=%08x", i,
                               expected, layout_output[i]);
            ++layout_mismatches;
        }
    }
    st_emit_marker("CHECK", "layout_reads=4 cropped_sources=1 mismatches=%u", layout_mismatches);
    failures += layout_mismatches;
    enum { LARGE_ENTRIES = 8192, LARGE_READS = 256, LARGE_WORDS = 8 + LARGE_READS * 128 + 8 };
    GnmTexture* large_views = st_arena_alloc(&arena, LARGE_ENTRIES * sizeof(GnmTexture), 65536);
    volatile uint32_t* large_output = st_arena_alloc(&arena, LARGE_WORDS * 4, 65536);
    if (!large_views || !large_output)
        return st_fail("reason=large_table_memory");
    for (unsigned i = 0; i < LARGE_ENTRIES; ++i)
        large_views[i] = mixed[i % 2];
    GnmBuffer large_table = st_raw_buffer(large_views, LARGE_ENTRIES * sizeof(GnmTexture));
    large_table.stride = sizeof(GnmTexture);
    large_table.numrecords = LARGE_ENTRIES;
    for (unsigned phase = 0; phase < 2; ++phase) {
        large_views[0] = mixed[phase];
        large_views[1] = mixed[1 - phase];
        st_fill_words(large_output, LARGE_WORDS, 0xdeadbeef);
        StDcb large_reads;
        st_dcb_begin(&large_reads, &queue);
        sceGnmDrawCmdSetCsShader(&large_reads.cmd, &shaders[1]);
        sceGnmDrawCmdSetVsharpUserData(&large_reads.cmd, GNM_STAGE_CS, 0, &large_table);
        for (unsigned pass = 0; pass < LARGE_READS; ++pass) {
            GnmBuffer destination = st_raw_buffer(large_output + 8 + pass * 128, 128 * 4);
            destination.stride = 4;
            destination.numrecords = 128;
            sceGnmDrawCmdSetVsharpUserData(&large_reads.cmd, GNM_STAGE_CS, 4, &destination);
            sceGnmDrawCmdDispatchDirect(&large_reads.cmd, 2, 1, 1, 0);
            st_dcb_barrier(&large_reads, 0);
        }
        if (!submit_and_wait(&large_reads))
            return st_fail("reason=large_table_submit phase=%u", phase);
        unsigned mismatches = 0;
        for (unsigned i = 0; i < LARGE_WORDS; ++i) {
            uint32_t expected = 0xdeadbeef;
            if (i >= 8 && i < 8 + LARGE_READS * 128) {
                unsigned image = ((i - 8) % 128 >= 64) ^ phase;
                expected = image ? 0x3f000000 : 0x3f800000;
            }
            if (large_output[i] != expected) {
                if (!mismatches)
                    st_emit_marker("CHECK", "large phase=%u word=%u expected=%08x got=%08x",
                                   phase, i, expected, large_output[i]);
                ++mismatches;
            }
        }
        st_emit_marker("CHECK", "large_entries=%u reads=%u phase=%u mismatches=%u",
                       LARGE_ENTRIES, LARGE_READS, phase, mismatches);
        failures += mismatches;
    }
    if (failures)
        return st_fail("mismatches=%u", failures);
    st_emit_marker("PASS", "numeric_types=3 dimensions=3 static_controls=9 runtime_cases=15 "
                           "gpu_updates=3 cpu_updates=3 mixed_reads=4 layout_reads=4 large_entries=8192 large_reads=512 table_rewrite=1 guards=1");
    return 0;
}
