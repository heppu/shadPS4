#define SHADTEST_NAME "runtime_image_queries"
#include <gnm_texture.h>
#include "planar.h"
#include "array.h"
#include "control.h"
#include "control_array.h"
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
    cs.computepgmrsrc2 = ((direct ? 12u : 8u) << 1) | (1u << 7);
    cs.computenumthreadx = 64;
    cs.computenumthready = cs.computenumthreadz = 1;
    return cs;
}

int main(void) {
    st_begin();
    StArena arena; StQueue queue;
    if (!st_arena_init(&arena, 8 * 1024 * 1024) || !st_queue_init(&arena, &queue))
        return st_fail("reason=allocation");
    GnmCsStageRegisters shaders[] = {
        shader(&arena, control_shader, sizeof(control_shader), true),
        shader(&arena, planar_shader, sizeof(planar_shader), false),
        shader(&arena, control_array_shader, sizeof(control_array_shader), true),
        shader(&arena, array_shader, sizeof(array_shader), false),
    };
    volatile uint32_t* output = st_arena_alloc(&arena, (128 * 4 + 16) * 4, 65536);
    GnmBuffer result = st_raw_buffer(output + 8, 128 * 16);
    result.stride = 16; result.numrecords = 128;
    unsigned failures = 0;
    for (unsigned array = 0; array < 2; ++array) {
        GnmTexture* textures = st_arena_alloc(&arena, 2 * sizeof(GnmTexture), 65536);
        for (unsigned t = 0; t < 2; ++t) {
            GnmTextureCreateInfo info = {
                .format = GNM_FMT_R32G32B32A32_FLOAT,
                .texturetype = array ? GNM_TEXTURE_2D_ARRAY : t ? GNM_TEXTURE_3D : GNM_TEXTURE_2D,
                .width = t ? 32 : 64, .height = t ? 16 : 32, .depth = !array && t ? 8 : 1,
                .pitch = t ? 32 : 64, .nummiplevels = t ? 4 : 5, .numslices = array ? 3 : 1,
                .numfragments = 1, .tilemodehint = GNM_TM_THIN_1D_THIN,
                .mingpumode = GNM_GPU_BASE,
            };
            if (sceGnmCreateTexture(&textures[t], &info) != GNM_ERROR_OK)
                return st_fail("reason=texture_create");
            uint64_t bytes = 0; uint32_t alignment = 0;
            if (sceGnmTexCalcByteSize(&bytes, &alignment, &textures[t]) != GNM_ERROR_OK)
                return st_fail("reason=texture_size");
            void* data = st_arena_alloc(&arena, bytes, 65536);
            if (!data) return st_fail("reason=texture_memory bytes=%llu", (unsigned long long)bytes);
            memset(data, 0, bytes); sceGnmTexSetBaseAddress(&textures[t], data);
            textures[t].baselevel = t;
            if (array) { textures[t].basearray = 1; textures[t].lastarray = 2; }
        }
        for (unsigned dynamic = 0; dynamic < 2; ++dynamic) {
            for (unsigned phase = 0; phase < 2; ++phase) {
                if (dynamic && phase) { GnmTexture tmp = textures[0]; textures[0] = textures[1]; textures[1] = tmp; }
                st_fill_words(output, 528, 0xdeadbeef);
                GnmBuffer table = st_raw_buffer(textures, 2 * sizeof(GnmTexture));
                table.stride = sizeof(GnmTexture); table.numrecords = 2;
                StDcb dcb; st_dcb_begin(&dcb, &queue);
                sceGnmDrawCmdSetCsShader(&dcb.cmd, &shaders[array * 2 + dynamic]);
                if (dynamic) sceGnmDrawCmdSetVsharpUserData(&dcb.cmd, GNM_STAGE_CS, 0, &table);
                else sceGnmDrawCmdSetTsharpUserData(&dcb.cmd, GNM_STAGE_CS, 0, &textures[phase]);
                sceGnmDrawCmdSetVsharpUserData(&dcb.cmd, GNM_STAGE_CS, dynamic ? 4 : 8, &result);
                sceGnmDrawCmdDispatchDirect(&dcb.cmd, dynamic ? 2 : 1, 1, 1, 0);
                st_dcb_barrier(&dcb, 0);
                if (!submit_and_wait(&dcb)) return st_fail("reason=submit");
                unsigned mismatches = 0;
                for (unsigned i = 0; i < 528; ++i) {
                    uint32_t expected = 0xdeadbeef;
                    if (i >= 8 && i < 8 + (dynamic ? 128 : 64) * 4) {
                        unsigned lane = (i - 8) / 4, c = (i - 8) % 4;
                        unsigned t = dynamic ? ((lane / 64) ^ phase) : phase;
                        unsigned lod = (lane & 1) + t;
                        unsigned values[] = {(t ? 32 : 64) >> lod, (t ? 16 : 32) >> lod,
                                             array ? 2 : t ? (8 >> lod) : 0, t ? 3 : 5};
                        expected = values[c];
                    }
                    if (output[i] != expected) {
                        if (!mismatches) st_emit_marker("CHECK", "array=%u dynamic=%u phase=%u word=%u expected=%08x got=%08x", array,dynamic,phase,i,expected,output[i]);
                        ++mismatches;
                    }
                }
                st_emit_marker("CHECK", "array=%u dynamic=%u phase=%u mismatches=%u",array,dynamic,phase,mismatches);
                failures += mismatches;
            }
        }
    }
    if (failures) return st_fail("mismatches=%u",failures);
    st_emit_marker("PASS", "static_controls=4 runtime_cases=4 mixed_dimensions=1 lods=2 table_rewrite=1 guards=1");
    return 0;
}
