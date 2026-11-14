#define SHADTEST_NAME "early_depth_dma"
#include <gnm_depthrendertarget.h>
#include <gnm_rendertarget.h>
#include <gnm_texture.h>
#include "color.h"
#include "fullscreen.h"
#include "read.h"
#include "write.h"
#include "kill.h"
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

enum { WIDTH = 64, HEIGHT = 128 };
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
                 GnmPsStageRegisters* ps, bool initialize, unsigned compare, bool write_depth) {
    const GnmPrimitiveSetup primitive = {.cullmode = GNM_CULL_NONE,
                                         .frontface = GNM_FACE_CCW,
                                         .frontmode = GNM_FILL_SOLID,
                                         .backmode = GNM_FILL_SOLID,
                                         .provokemode = GNM_PROVOKINGVTX_FIRST};
    sceGnmDrawCmdSetPrimitiveSetup(&dcb->cmd, &primitive);
    sceGnmDrawCmdSetRenderTarget(&dcb->cmd, 0, rt);
    sceGnmDrawCmdSetRenderTargetMask(&dcb->cmd, initialize ? 0 : 15);
    sceGnmDrawCmdSetDepthRenderTarget(&dcb->cmd, dt);
    reg(dcb, 0x28800, 2u | ((initialize || write_depth) ? 4u : 0) | ((initialize ? 7u : compare) << 4));
    reg(dcb, 0x2842c, 0);
    sceGnmDrawCmdSetScreenScissor(&dcb->cmd, 0, 0, WIDTH, HEIGHT);
    const GnmSetViewportInfo viewport = {.dmin = 0,
                                         .dmax = 1,
                                         .scale = {WIDTH * .5f, -HEIGHT * .5f, 1},
                                         .offset = {WIDTH * .5f, HEIGHT * .5f, 0}};
    sceGnmDrawCmdSetViewport(&dcb->cmd, 0, &viewport);
    sceGnmDrawCmdSetVsShader(&dcb->cmd, vs, 0);
    sceGnmDrawCmdSetPsShader(&dcb->cmd, ps);
    sceGnmDrawCmdSetPrimitiveType(&dcb->cmd, GNM_PT_TRILIST);
    sceGnmDrawCmdDrawIndexAuto(&dcb->cmd, 3);
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
    sceGnmVsRegsSetAddress(&vs, code(&arena, fullscreen_shader, sizeof(fullscreen_shader)));
    vs.spishaderpgmrsrc1vs = 1u | (0xc0u << 12);
    vs.spishaderposformat = 4;
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
    uint8_t* memory = st_arena_alloc(&arena, bytes, 65536);
    if (!memory)
        return st_fail("reason=depth_memory");
    memset(memory, 0, bytes);
    sceGnmDrtSetZReadAddress(&dt, memory);
    sceGnmDrtSetZWriteAddress(&dt, memory);
    sceGnmDrtSetStencilReadAddress(&dt, memory + stencil_offset);
    sceGnmDrtSetStencilWriteAddress(&dt, memory + stencil_offset);
    uint32_t* source = st_arena_alloc(&arena, 256, 65536);
    volatile uint32_t* result = st_arena_alloc(&arena, 16 * 4, 65536);
    if (!source || !result)
        return st_fail("reason=buffer_memory");
    st_fill_words(source, 64, 0);
    source[0] = 0x3e800000;
    source[8] = 0x3f000000;
    GnmBuffer input = st_raw_buffer(source, 256);
    input.stride = 4;
    input.numrecords = 64;
    GnmBuffer output = st_raw_buffer(result + 4, 4);
    output.stride = 4;
    output.numrecords = 1;
    GnmBuffer pointer = {0};
    memcpy(&pointer, &source, sizeof(source));
    StDcb dcb;
    st_dcb_begin(&dcb, &queue);
    draw(&dcb, &rt, &dt, &vs, &ps, true, 7, false);
    if (!submit_and_wait(&dcb))
        return st_fail("reason=initial_draw");
    GnmPsStageRegisters readers[4] = {ps, ps, ps, ps};
    sceGnmPsRegsSetAddress(&readers[0], code(&arena, read_shader, sizeof(read_shader)));
    sceGnmPsRegsSetAddress(&readers[1], code(&arena, write_shader, sizeof(write_shader)));
    sceGnmPsRegsSetAddress(&readers[2], code(&arena, kill_shader, sizeof(kill_shader)));
    sceGnmPsRegsSetAddress(&readers[3], code(&arena, read_shader, sizeof(read_shader)));
    for (unsigned i = 0; i < 4; ++i) {
        readers[i].spishaderpgmrsrc1ps = 1u | (1u << 6) | (0xc0u << 12);
        readers[i].spishaderpgmrsrc2ps = 12u << 1;
        readers[i].dbshadercontrol = i == 1 ? (1u << 9) | (1u << 10) : i == 3 ? 0 : 1u << 4;
    }
    StCase cases[7] = {{.name = "read_pass"}, {.name = "read_occluded"},
                       {.name = "write_pass"}, {.name = "write_occluded"},
                       {.name = "depth_write_control"}, {.name = "discard_control"}, {.name = "late_read_control"}};
    for (unsigned phase = 0; phase < 7; ++phase) {
        for (unsigned i = 0; i < pitch * HEIGHT * 4; ++i) pixels[i] = .125f;
        st_fill_words(result, 16, 0xdeadbeef);
        st_dcb_begin(&dcb, &queue);
        sceGnmDrawCmdSetVsharpUserData(&dcb.cmd, GNM_STAGE_PS, 0, &pointer);
        sceGnmDrawCmdSetVsharpUserData(&dcb.cmd, GNM_STAGE_PS, 4, &input);
        sceGnmDrawCmdSetVsharpUserData(&dcb.cmd, GNM_STAGE_PS, 8, &output);
        unsigned shader_index = phase < 4 ? phase / 2 : phase == 4 ? 0 : phase == 5 ? 2 : 3;
        draw(&dcb, &rt, &dt, &vs, &readers[shader_index], false,
             phase == 1 || phase == 3 ? 1 : 2, phase == 4);
        if (!submit_and_wait(&dcb))
            return st_fail("reason=verify_draw phase=%u", phase);
        for (unsigned y = 0; y < HEIGHT; ++y)
            for (unsigned x = 0; x < WIDTH; ++x)
                for (unsigned c = 0; c < 4; ++c) {
                    const float color[] = {.25f, .5f, .75f, 1.f};
                    unsigned i = (y * pitch + x) * 4 + c;
                    const float expected = phase % 2 ? .125f : color[c];
                    st_case_expect(&cases[phase], "pixel", i, bits(expected), bits(pixels[i]));
                }
        for (unsigned i = 0; i < 16; ++i)
            st_case_expect(&cases[phase], "side_effect", i,
                           phase >= 2 && phase < 4 && i == 4 ? 0x3e800000 : 0xdeadbeef, result[i]);
    }
    return st_finish_cases(cases, 7, "dma_reads=1 depth_pass_fail=1 late_guest_writes=1 depth_write_control=1 discard=1 late_read=1 guards=1");
}
