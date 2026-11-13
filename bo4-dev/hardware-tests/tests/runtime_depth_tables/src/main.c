#define SHADTEST_NAME "runtime_depth_tables"
#include <gnm_sampler.h>
#include <gnm_depthrendertarget.h>
#include <gnm_rendertarget.h>
#include "fullscreen.h"
#include "color.h"
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
    cs.computepgmrsrc2 = ((direct ? 16u : 12u) << 1) | (1u << 7);
    cs.computenumthreadx = 64;
    cs.computenumthready = cs.computenumthreadz = 1;
    return cs;
}

enum { WIDTH = 64, HEIGHT = 64 };
static void* code(StArena* a, const void* src, size_t size) {
    void* dst = st_arena_alloc(a, size, 256);
    if (dst)
        memcpy(dst, src, size);
    return dst;
}
static void reg(StDcb* dcb, uint32_t address, uint32_t value) {
    const uint32_t packet[] = {PKT3(PKT3_SET_CONTEXT_REG, 1, 0), (address - 0x28000) / 4, value};
    st_dcb_put(dcb, packet, 3);
}
static void draw(StDcb* dcb, GnmRenderTarget* rt, GnmDepthRenderTarget* dt, GnmVsStageRegisters* vs,
                 GnmPsStageRegisters* ps, float depth) {
    const GnmPrimitiveSetup primitive = {.cullmode = GNM_CULL_NONE,
                                         .frontface = GNM_FACE_CCW,
                                         .frontmode = GNM_FILL_SOLID,
                                         .backmode = GNM_FILL_SOLID,
                                         .provokemode = GNM_PROVOKINGVTX_FIRST};
    sceGnmDrawCmdSetPrimitiveSetup(&dcb->cmd, &primitive);
    sceGnmDrawCmdSetRenderTarget(&dcb->cmd, 0, rt);
    sceGnmDrawCmdSetRenderTargetMask(&dcb->cmd, 0);
    sceGnmDrawCmdSetDepthRenderTarget(&dcb->cmd, dt);
    reg(dcb, 0x28800,
        1u | 2u | (4u) | ((7u) << 4) |
            ((7u) << 8));
    reg(dcb, 0x2842c, 0x30u);
    reg(dcb, 0x28430, 0x00ffff00u);
    reg(dcb, 0x28434, 0x00ffff00u);
    sceGnmDrawCmdSetScreenScissor(&dcb->cmd, 0, 0, WIDTH, HEIGHT);
    const GnmSetViewportInfo viewport = {.dmin = 0,
                                         .dmax = 1,
                                         .scale = {WIDTH * .5f, -HEIGHT * .5f, 0},
                                         .offset = {WIDTH * .5f, HEIGHT * .5f, depth}};
    sceGnmDrawCmdSetViewport(&dcb->cmd, 0, &viewport);
    sceGnmDrawCmdSetVsShader(&dcb->cmd, vs, 0);
    sceGnmDrawCmdSetPsShader(&dcb->cmd, ps);
    sceGnmDrawCmdSetPrimitiveType(&dcb->cmd, GNM_PT_TRILIST);
    sceGnmDrawCmdDrawIndexAuto(&dcb->cmd, 3);
}
static unsigned gpu_depth_cases(StArena* arena, StQueue* queue,
                                const GnmCsStageRegisters* cs, volatile uint32_t* output) {
    GnmVsStageRegisters vs = {0};
    GnmPsStageRegisters ps = {0};
    sceGnmVsRegsSetAddress(&vs, code(arena, fullscreen_shader, sizeof(fullscreen_shader)));
    vs.spishaderpgmrsrc1vs = 1u | (0xc0u << 12);
    vs.spishaderposformat = 4;
    sceGnmPsRegsSetAddress(&ps, code(arena, color_shader, sizeof(color_shader)));
    ps.spishaderpgmrsrc1ps = 0xc0u << 12;
    ps.spipsinputena = ps.spipsinputaddr = 1u << 15;
    ps.spishadercolformat = 9;
    ps.cbshadermask = 15;
    GnmRenderTarget rt = {0};
    const GnmRenderTargetCreateInfo ri = {.colorfmt = GNM_FMT_R32G32B32A32_FLOAT,
                                          .width = WIDTH,
                                          .height = HEIGHT,
                                          .pitch = WIDTH,
                                          .numslices = 1,
                                          .numsamples = 1,
                                          .numfragments = 1,
                                          .colortilemodehint = GNM_TM_DISPLAY_LINEAR_ALIGNED,
                                          .mingpumode = GNM_GPU_BASE};
    if (sceGnmCreateRenderTarget(&rt, &ri) != GNM_ERROR_OK)
        return st_fail("reason=color_create");
    uint64_t bytes = 0;
    uint32_t alignment = 0;
    if (sceGnmRtCalcByteSize(&bytes, &alignment, &rt) != GNM_ERROR_OK)
        return st_fail("reason=color_size");
    volatile float* pixels = st_arena_alloc(arena, bytes, 65536);
    if (!pixels)
        return st_fail("reason=color_memory");
    sceGnmRtSetBaseAddr(&rt, (void*)pixels);
    GnmDepthRenderTarget targets[2];
    GnmTexture* textures = st_arena_alloc(arena, 2 * sizeof(GnmTexture), 65536);
    for (unsigned t = 0; t < 2; ++t) {
    GnmDepthRenderTarget dt = {0};
    const GnmDepthRenderTargetCreateInfo di = {.width = WIDTH,
                                               .height = HEIGHT,
                                               .pitch = WIDTH,
                                               .numslices = 1,
                                               .zfmt = 3,
                                               .stencilfmt = GNM_STENCIL_8,
                                               .tilemodehint = GNM_TM_DEPTH_2D_THIN_64,
                                               .mingpumode = GNM_GPU_BASE,
                                               .numfragments = 1};
    if (sceGnmCreateDepthRenderTarget(&dt, &di) != GNM_ERROR_OK)
        return st_fail("reason=depth_create");
    uint64_t stencil_offset = 0;
    if (sceGnmDrtCalcByteSize(&bytes, &alignment, &dt) != GNM_ERROR_OK ||
        sceGnmDrtCalcStencilByteOffset(&stencil_offset, &dt) != GNM_ERROR_OK)
        return st_fail("reason=depth_size");
    uint8_t* memory = st_arena_alloc(arena, bytes, 65536);
    if (!memory)
        return st_fail("reason=depth_memory");
    memset(memory, 0, bytes);
    sceGnmDrtSetZReadAddress(&dt, memory);
    sceGnmDrtSetZWriteAddress(&dt, memory);
    sceGnmDrtSetStencilReadAddress(&dt, memory + stencil_offset);
    sceGnmDrtSetStencilWriteAddress(&dt, memory + stencil_offset);
    GnmTexture texture = {0};
    const GnmTextureCreateInfo ti = {
        .format = GNM_DATA_FORMAT_INIT(GNM_IMG_DATA_FORMAT_32, GNM_IMG_NUM_FORMAT_FLOAT, GNM_CHAN_X,
                                       GNM_CHAN_CONSTANT0, GNM_CHAN_CONSTANT0, GNM_CHAN_CONSTANT1),
        .texturetype = GNM_TEXTURE_2D,
        .width = WIDTH,
        .height = HEIGHT,
        .depth = 1,
        .pitch = sceGnmDrtGetPaddedWidth(&dt),
        .nummiplevels = 1,
        .numslices = 1,
        .numfragments = 1,
        .tilemodehint = GNM_TM_DEPTH_2D_THIN_64,
        .mingpumode = GNM_GPU_BASE};
    if (sceGnmCreateTexture(&texture, &ti) != GNM_ERROR_OK)
        return st_fail("reason=texture_create");
    sceGnmTexSetBaseAddress(&texture, memory);
    targets[t] = dt;
    textures[t] = texture;
    }

    GnmBuffer table = st_raw_buffer(textures, 2 * sizeof(GnmTexture));
    table.stride = sizeof(GnmTexture);
    table.numrecords = 2;
    GnmBuffer result = st_raw_buffer(output + 8, 128 * 4);
    result.stride = 4;
    result.numrecords = 128;
    GnmSampler sampler = {.depthcomparefunc = GNM_DEPTH_COMPARE_LESS};
    unsigned failures = 0;
    for (unsigned phase = 0; phase < 3; ++phase) {
        st_fill_words(output, 144, 0xdeadbeef);
        StDcb dcb;
        st_dcb_begin(&dcb, queue);
        for (unsigned t = 0; t < 2; ++t) {
            draw(&dcb, &rt, &targets[t], &vs, &ps, (t ^ (phase & 1)) ? .75f : .25f);
        }
        st_dcb_barrier(&dcb, 0);
        sceGnmDrawCmdSetCsShader(&dcb.cmd, cs);
        sceGnmDrawCmdSetVsharpUserData(&dcb.cmd, GNM_STAGE_CS, 0, &table);
        sceGnmDrawCmdSetSsharpUserData(&dcb.cmd, GNM_STAGE_CS, 4, &sampler);
        sceGnmDrawCmdSetVsharpUserData(&dcb.cmd, GNM_STAGE_CS, 8, &result);
        sceGnmDrawCmdDispatchDirect(&dcb.cmd, 2, 1, 1, 0);
        st_dcb_barrier(&dcb, 0);
        if (!submit_and_wait(&dcb)) return st_fail("reason=gpu_depth_submit");
        unsigned mismatches = 0;
        for (unsigned i = 0; i < 144; ++i) {
            uint32_t expected = 0xdeadbeef;
            if (i >= 8 && i < 136)
                expected = (((i - 8) / 64) ^ (phase & 1)) ? 0x3f800000 : 0;
            if (output[i] != expected) {
                if (!mismatches) st_emit_marker("CHECK", "gpu_depth=1 phase=%u word=%u expected=%08x got=%08x", phase, i, expected, output[i]);
                ++mismatches;
            }
        }
        st_emit_marker("CHECK", "gpu_depth=1 phase=%u mismatches=%u", phase, mismatches);
        failures += mismatches;
    }
    return failures;
}

int main(void) {
    st_begin();
    StArena arena;
    StQueue queue;
    if (!st_arena_init(&arena, 4 * 1024 * 1024) || !st_queue_init(&arena, &queue))
        return st_fail("reason=allocation");
    GnmCsStageRegisters shaders[] = {
        shader(&arena, control_shader, sizeof(control_shader), true),
        shader(&arena, planar_shader, sizeof(planar_shader), false),
        shader(&arena, control_array_shader, sizeof(control_array_shader), true),
        shader(&arena, array_shader, sizeof(array_shader), false),
    };
    volatile uint32_t* output = st_arena_alloc(&arena, 144 * 4, 65536);
    GnmBuffer result = st_raw_buffer(output + 8, 128 * 4);
    result.stride = 4;
    result.numrecords = 128;
    GnmSampler sampler = {0};
    sampler.depthcomparefunc = GNM_DEPTH_COMPARE_LESS;
    unsigned failures = 0;
    for (unsigned array = 0; array < 2; ++array) {
        GnmTexture* textures = st_arena_alloc(&arena, 2 * sizeof(GnmTexture), 65536);
        for (unsigned t = 0; t < 2; ++t) {
            GnmTextureCreateInfo info = {
                .format = GNM_FMT_R32_FLOAT,
                .texturetype = array ? GNM_TEXTURE_2D_ARRAY : GNM_TEXTURE_2D,
                .width = 64, .height = 64, .depth = 1, .pitch = 64,
                .nummiplevels = 1, .numslices = array ? 2 : 1, .numfragments = 1,
                .tilemodehint = GNM_TM_DISPLAY_LINEAR_ALIGNED, .mingpumode = GNM_GPU_BASE,
            };
            if (sceGnmCreateTexture(&textures[t], &info) != GNM_ERROR_OK)
                return st_fail("reason=texture_create");
            uint64_t bytes = 0;
            uint32_t alignment = 0;
            if (sceGnmTexCalcByteSize(&bytes, &alignment, &textures[t]) != GNM_ERROR_OK)
                return st_fail("reason=texture_size");
            float* data = st_arena_alloc(&arena, bytes, 65536);
            if (!data) return st_fail("reason=texture_memory");
            unsigned pitch = sceGnmTexGetPitch(&textures[t]);
            for (unsigned i = 0; i < bytes / sizeof(float); ++i) {
                unsigned slice = i / (pitch * 64);
                data[i] = (t ^ (array && slice)) ? .75f : .25f;
            }
            sceGnmTexSetBaseAddress(&textures[t], data);
        }
        for (unsigned dynamic = 0; dynamic < 2; ++dynamic) {
            for (unsigned phase = 0; phase < 2; ++phase) {
                if (dynamic && phase) {
                    GnmTexture tmp = textures[0]; textures[0] = textures[1]; textures[1] = tmp;
                }
                st_fill_words(output, 144, 0xdeadbeef);
                GnmBuffer table = st_raw_buffer(textures + (dynamic ? 0 : phase),
                                               (dynamic ? 2 : 1) * sizeof(GnmTexture));
                table.stride = sizeof(GnmTexture);
                table.numrecords = dynamic ? 2 : 1;
                StDcb dcb;
                st_dcb_begin(&dcb, &queue);
                sceGnmDrawCmdSetCsShader(&dcb.cmd, &shaders[array * 2 + dynamic]);
                if (dynamic) {
                    sceGnmDrawCmdSetVsharpUserData(&dcb.cmd, GNM_STAGE_CS, 0, &table);
                } else {
                    sceGnmDrawCmdSetTsharpUserData(&dcb.cmd, GNM_STAGE_CS, 0, &textures[phase]);
                }
                sceGnmDrawCmdSetSsharpUserData(&dcb.cmd, GNM_STAGE_CS, dynamic ? 4 : 8, &sampler);
                sceGnmDrawCmdSetVsharpUserData(&dcb.cmd, GNM_STAGE_CS, dynamic ? 8 : 12, &result);
                sceGnmDrawCmdDispatchDirect(&dcb.cmd, dynamic ? 2 : 1, 1, 1, 0);
                st_dcb_barrier(&dcb, 0);
                if (!submit_and_wait(&dcb)) return st_fail("reason=submit");
                unsigned mismatches = 0;
                for (unsigned i = 0; i < 144; ++i) {
                    uint32_t expected = 0xdeadbeef;
                    if (i >= 8 && i < 8 + (dynamic ? 128 : 64)) {
                        unsigned t = dynamic ? (((i - 8) / 64) ^ phase) : phase;
                        bool pass = (t ^ (array && ((i - 8) & 1))) != 0;
                        expected = pass ? 0x3f800000 : 0;
                    }
                    if (output[i] != expected) {
                        if (!mismatches) st_emit_marker("CHECK", "array=%u dynamic=%u phase=%u word=%u expected=%08x got=%08x", array, dynamic, phase, i, expected, output[i]);
                        ++mismatches;
                    }
                }
                st_emit_marker("CHECK", "array=%u dynamic=%u phase=%u mismatches=%u", array, dynamic, phase, mismatches);
                failures += mismatches;
            }
        }
    }
    failures += gpu_depth_cases(&arena, &queue, &shaders[1], output);
    if (failures) return st_fail("mismatches=%u", failures);
    st_emit_marker("PASS", "layouts=2 static_controls=4 dynamic_cases=4 table_rewrite=1 gpu_depth_writes=3 guards=1");
    st_arena_destroy(&arena);
    return 0;
}
