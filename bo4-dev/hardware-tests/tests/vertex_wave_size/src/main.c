#define SHADTEST_NAME "vertex_wave_size"
#include <gnm_depthrendertarget.h>
#include <gnm_rendertarget.h>
#include <gnm_texture.h>
#include "color.h"
#include "points.h"
#include "quads.h"
#include "lanes.h"
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

enum { WIDTH = 64, HEIGHT = 4 };
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
static void draw(StDcb* dcb, GnmRenderTarget* rt, GnmVsStageRegisters* vs,
                 GnmPsStageRegisters* ps, bool quads) {
    const GnmPrimitiveSetup primitive = {.cullmode = GNM_CULL_NONE,
        .frontface = GNM_FACE_CCW, .frontmode = GNM_FILL_SOLID,
        .backmode = GNM_FILL_SOLID, .provokemode = GNM_PROVOKINGVTX_FIRST};
    sceGnmDrawCmdSetPrimitiveSetup(&dcb->cmd, &primitive);
    sceGnmDrawCmdSetRenderTarget(&dcb->cmd, 0, rt);
    sceGnmDrawCmdSetRenderTargetMask(&dcb->cmd, 15);
    reg(dcb, 0x28800, 0);
    reg(dcb, 0x2842c, 0);
    sceGnmDrawCmdSetScreenScissor(&dcb->cmd, 0, 0, WIDTH, HEIGHT);
    const GnmSetViewportInfo viewport = {.dmin = 0, .dmax = 1,
        .scale = {WIDTH * .5f, -HEIGHT * .5f, 1},
        .offset = {WIDTH * .5f, HEIGHT * .5f, 0}};
    sceGnmDrawCmdSetViewport(&dcb->cmd, 0, &viewport);
    sceGnmDrawCmdSetVsShader(&dcb->cmd, vs, 0);
    sceGnmDrawCmdSetPsShader(&dcb->cmd, ps);
    sceGnmDrawCmdSetPrimitiveType(&dcb->cmd, quads ? GNM_PT_QUADLIST : GNM_PT_POINTLIST);
    sceGnmDrawCmdDrawIndexAuto(&dcb->cmd, quads ? 256 : 64);
}
static uint32_t bits(float f) {
    uint32_t b;
    memcpy(&b, &f, 4);
    return b;
}
int main(void) {
    st_begin();
    StArena arena;
    StQueue queue;
    if (!st_arena_init(&arena, 8 * 1024 * 1024) || !st_queue_init(&arena, &queue))
        return st_fail("reason=allocation");
    GnmVsStageRegisters vs = {0};
    GnmPsStageRegisters ps = {0};
    sceGnmVsRegsSetAddress(&vs, code(&arena, points_shader, sizeof(points_shader)));
    vs.spishaderpgmrsrc1vs = 1u | (0xc0u << 12);
    vs.spishaderposformat = 0x14;
    vs.paclvsoutcntl = (1u << 16) | (1u << 21);
    sceGnmPsRegsSetAddress(&ps, code(&arena, color_shader, sizeof(color_shader)));
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
    volatile float* pixels = st_arena_alloc(&arena, bytes, 65536);
    if (!pixels)
        return st_fail("reason=color_memory");
    sceGnmRtSetBaseAddr(&rt, (void*)pixels);
    unsigned pitch = sceGnmRtGetPitch(&rt);
    GnmVsStageRegisters lane_vs = vs;
    sceGnmVsRegsSetAddress(&lane_vs, code(&arena, lanes_shader, sizeof(lanes_shader)));
    GnmVsStageRegisters quad_vs = vs;
    sceGnmVsRegsSetAddress(&quad_vs, code(&arena, quads_shader, sizeof(quads_shader)));
    quad_vs.paclvsoutcntl = 0;
    quad_vs.spishaderposformat = 4;
    StCase cases[3] = {{.name = "wave_independent_points"}, {.name = "wave64_vertex_lanes"},
                       {.name = "wave_independent_quads"}};
    for (unsigned phase = 0; phase < 3; ++phase) {
        for (unsigned i = 0; i < pitch * HEIGHT * 4; ++i) pixels[i] = -1.f;
        StDcb dcb;
        st_dcb_begin(&dcb, &queue);
        draw(&dcb, &rt, phase == 2 ? &quad_vs : phase ? &lane_vs : &vs, &ps, phase == 2);
        if (!submit_and_wait(&dcb)) return st_fail("reason=draw phase=%u", phase);
        for (unsigned y = 0; y < HEIGHT; ++y)
            for (unsigned x = 0; x < WIDTH; ++x)
                for (unsigned c = 0; c < 4; ++c) {
                    const float color[] = {.125f, .25f, .375f, .25f};
                    const unsigned i = (y * pitch + x) * 4 + c;
                    const float expected = y == 2 ? color[c] : -1.f;
                    st_case_expect(&cases[phase], "pixel", i, bits(expected), bits(pixels[i]));
                }
    }
    return st_finish_cases(cases, 3, "vertex_points=64 lane_columns=64 outside_unchanged=1");
}
