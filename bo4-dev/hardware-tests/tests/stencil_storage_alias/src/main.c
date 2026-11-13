#define SHADTEST_NAME "stencil_storage_alias"
#include <gnm_depthrendertarget.h>
#include <gnm_rendertarget.h>
#include <gnm_texture.h>
#include "color.h"
#include "fullscreen.h"
#include "increment.h"
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
                 GnmPsStageRegisters* ps, bool initialize, unsigned ref) {
    const GnmPrimitiveSetup primitive = {.cullmode = GNM_CULL_NONE,
                                         .frontface = GNM_FACE_CCW,
                                         .frontmode = GNM_FILL_SOLID,
                                         .backmode = GNM_FILL_SOLID,
                                         .provokemode = GNM_PROVOKINGVTX_FIRST};
    sceGnmDrawCmdSetPrimitiveSetup(&dcb->cmd, &primitive);
    sceGnmDrawCmdSetRenderTarget(&dcb->cmd, 0, rt);
    sceGnmDrawCmdSetRenderTargetMask(&dcb->cmd, initialize ? 0 : 15);
    sceGnmDrawCmdSetDepthRenderTarget(&dcb->cmd, dt);
    reg(dcb, 0x28800,
        1u | 2u | (initialize ? 4u : 0) | ((initialize ? 7u : 2u) << 4) |
            ((initialize ? 7u : 2u) << 8));
    reg(dcb, 0x2842c, initialize ? 0x30u : 0);
    reg(dcb, 0x28430, ref | 0x00ffff00u);
    reg(dcb, 0x28434, ref | 0x00ffff00u);
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
    GnmCsStageRegisters cs = {0};
    sceGnmVsRegsSetAddress(&vs, code(&arena, fullscreen_shader, sizeof(fullscreen_shader)));
    vs.spishaderpgmrsrc1vs = 1u | (0xc0u << 12);
    vs.spishaderposformat = 4;
    sceGnmPsRegsSetAddress(&ps, code(&arena, color_shader, sizeof(color_shader)));
    ps.spishaderpgmrsrc1ps = 0xc0u << 12;
    ps.spipsinputena = ps.spipsinputaddr = 1u << 15;
    ps.spishadercolformat = 9;
    ps.cbshadermask = 15;
    sceGnmCsRegsSetAddress(&cs, code(&arena, increment_shader, sizeof(increment_shader)));
    cs.computepgmrsrc1 = 1u | (0xc0u << 12);
    cs.computepgmrsrc2 = 8u << 1;
    cs.computenumthreadx = cs.computenumthready = cs.computenumthreadz = 1;
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
    GnmTexture texture = {0};
    const GnmTextureCreateInfo ti = {
        .format = GNM_DATA_FORMAT_INIT(GNM_IMG_DATA_FORMAT_8, GNM_IMG_NUM_FORMAT_UINT, GNM_CHAN_X,
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
    sceGnmTexSetBaseAddress(&texture, memory + stencil_offset);
    StDcb dcb;
    st_dcb_begin(&dcb, &queue);
    draw(&dcb, &rt, &dt, &vs, &ps, true, 0x35);
    if (!submit_and_wait(&dcb))
        return st_fail("reason=initial_draw");
    StCase cases[3] = {{.name = "stencil_control"},
                       {.name = "storage_increment"},
                       {.name = "storage_increment_again"}};
    for (unsigned phase = 0; phase < 3; ++phase) {
        for (unsigned j = 0; j < pitch * HEIGHT * 4; ++j)
            pixels[j] = .25f;
        if (phase) {
            st_dcb_begin(&dcb, &queue);
            sceGnmDrawCmdSetCsShader(&dcb.cmd, &cs);
            sceGnmDrawCmdSetTsharpUserData(&dcb.cmd, GNM_STAGE_CS, 0, &texture);
            sceGnmDrawCmdDispatchDirect(&dcb.cmd, 1, 1, 1, 0);
            st_dcb_barrier(&dcb, 0);
            if (!submit_and_wait(&dcb))
                return st_fail("reason=compute");
        }
        st_dcb_begin(&dcb, &queue);
        draw(&dcb, &rt, &dt, &vs, &ps, false, 0x35 + phase);
        if (!submit_and_wait(&dcb))
            return st_fail("reason=verify_draw");
        for (unsigned y = 0; y < HEIGHT; ++y)
            for (unsigned x = 0; x < WIDTH; ++x)
                for (unsigned c = 0; c < 4; ++c) {
                    const float output[] = {.125f, .25f, .375f, .25f};
                    float expected = phase == 0 || (x == 3 && y == 5) ? output[c] : .25f;
                    unsigned i = (y * pitch + x) * 4 + c;
                    st_case_expect(&cases[phase], "pixel", i, bits(expected), bits(pixels[i]));
                }
    }
    return st_finish_cases(cases, 3, "depth_preserved=1 stencil_storage_roundtrip=1");
}
