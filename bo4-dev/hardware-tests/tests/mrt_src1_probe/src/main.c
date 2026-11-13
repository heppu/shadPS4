#define SHADTEST_NAME "mrt_src1_probe"
#include <gnm_rendertarget.h>
#include "colors2.h"
#include "colors3.h"
#include "colors4.h"
#include "fullscreen.h"
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

enum { WIDTH = 64, HEIGHT = 128, TARGETS = 4 };
static const float colors[4][4] = {
    {.125f, .25f, .375f, .25f},
    {.25f, .375f, .5f, .75f},
    {.375f, .5f, .625f, .5f},
    {.5f, .625f, .75f, .125f},
};
typedef struct {
    GnmRenderTarget rt;
    volatile float* pixels;
    uint32_t pitch;
} Target;
static bool target_init(StArena* arena, Target* t) {
    memset(t, 0, sizeof(*t));
    const GnmRenderTargetCreateInfo ci = {
        .colorfmt = GNM_FMT_R32G32B32A32_FLOAT,
        .width = WIDTH,
        .height = HEIGHT,
        .pitch = WIDTH,
        .numslices = 1,
        .numsamples = 1,
        .numfragments = 1,
        .colortilemodehint = GNM_TM_DISPLAY_LINEAR_ALIGNED,
        .mingpumode = GNM_GPU_BASE,
    };
    if (sceGnmCreateRenderTarget(&t->rt, &ci) != GNM_ERROR_OK)
        return false;
    uint64_t bytes = 0;
    uint32_t alignment = 0;
    if (sceGnmRtCalcByteSize(&bytes, &alignment, &t->rt) != GNM_ERROR_OK)
        return false;
    void* memory =
        st_arena_alloc(arena, bytes, alignment > ST_PAGE_BYTES ? alignment : ST_PAGE_BYTES);
    if (!memory)
        return false;
    sceGnmRtSetBaseAddr(&t->rt, memory);
    t->pixels = memory;
    t->pitch = sceGnmRtGetPitch(&t->rt);
    return (uint64_t)t->pitch * HEIGHT * 16 <= bytes;
}
static void* shader_code(StArena* arena, const void* data, size_t bytes) {
    void* dst = st_arena_alloc(arena, bytes, 256);
    if (dst)
        memcpy(dst, data, bytes);
    return dst;
}
static uint32_t bits(float f) {
    uint32_t u;
    memcpy(&u, &f, 4);
    return u;
}
int main(void) {
    st_begin();
    StArena arena;
    StQueue queue;
    if (!st_arena_init(&arena, 8 * 1024 * 1024) || !st_queue_init(&arena, &queue))
        return st_fail("reason=allocation");
    GnmVsStageRegisters vs = {0};
    void* code = shader_code(&arena, fullscreen_shader, sizeof(fullscreen_shader));
    if (!code)
        return st_fail("reason=vs_allocation");
    sceGnmVsRegsSetAddress(&vs, code);
    vs.spishaderpgmrsrc1vs = 1u | (0xc0u << 12);
    vs.spishaderposformat = 4;
    GnmPsStageRegisters ps[3] = {0};
    const void* ps_data[] = {colors2_shader, colors3_shader, colors4_shader};
    const size_t ps_bytes[] = {sizeof(colors2_shader), sizeof(colors3_shader),
                               sizeof(colors4_shader)};
    for (unsigned i = 0; i < 3; ++i) {
        code = shader_code(&arena, ps_data[i], ps_bytes[i]);
        if (!code)
            return st_fail("reason=ps_allocation");
        sceGnmPsRegsSetAddress(&ps[i], code);
        ps[i].spishaderpgmrsrc1ps = 0xc0u << 12;
        ps[i].spipsinputena = ps[i].spipsinputaddr = 1u << 15;
        for (unsigned j = 0; j < i + 2; ++j) {
            ps[i].spishadercolformat |= 9u << (j * 4);
            ps[i].cbshadermask |= 15u << (j * 4);
        }
    }
    Target targets[TARGETS];
    for (unsigned i = 0; i < TARGETS; ++i)
        if (!target_init(&arena, &targets[i]))
            return st_fail("reason=target_allocation");
    const struct {
        const char* name;
        unsigned exports, target;
        GnmBlendOp factor;
        bool enabled, known, dual;
    } configs[] = {
        {"copy_control", 4, 2, GNM_BLEND_ZERO, false, true, false},
        {"src_alpha_control", 4, 2, GNM_BLEND_SRC_ALPHA, true, true, false},
        {"dual_src1_control", 2, 0, GNM_BLEND_SRC1_ALPHA, true, true, true},
        {"mrt1_src1_alpha", 4, 1, GNM_BLEND_SRC1_ALPHA, true, false, false},
        {"mrt2_src1_alpha", 4, 2, GNM_BLEND_SRC1_ALPHA, true, false, false},
        {"mrt3_src1_alpha", 4, 3, GNM_BLEND_SRC1_ALPHA, true, false, false},
        {"mrt2_inv_src1_alpha", 3, 2, GNM_BLEND_INVERSE_SRC1_ALPHA, true, false, false},
    };
    StCase cases[sizeof(configs) / sizeof(configs[0])] = {0};
    for (unsigned k = 0; k < sizeof(cases) / sizeof(cases[0]); ++k) {
        cases[k].name = configs[k].name;
        for (unsigned t = 0; t < TARGETS; ++t)
            for (unsigned j = 0; j < targets[t].pitch * HEIGHT * 4; ++j)
                targets[t].pixels[j] = .25f;
        StDcb dcb;
        st_dcb_begin(&dcb, &queue);
        const GnmPrimitiveSetup primitive = {.cullmode = GNM_CULL_NONE,
                                             .frontface = GNM_FACE_CCW,
                                             .frontmode = GNM_FILL_SOLID,
                                             .backmode = GNM_FILL_SOLID,
                                             .provokemode = GNM_PROVOKINGVTX_FIRST};
        sceGnmDrawCmdSetPrimitiveSetup(&dcb.cmd, &primitive);
        unsigned active = configs[k].dual ? 1 : configs[k].exports;
        for (unsigned t = 0; t < active; ++t) {
            sceGnmDrawCmdSetRenderTarget(&dcb.cmd, t, &targets[t].rt);
            const GnmBlendControl blend = {
                .blendenabled = configs[k].enabled && t == configs[k].target,
                .colorfunc = GNM_COMB_DST_PLUS_SRC,
                .colorsrcmult = GNM_BLEND_ONE,
                .colordstmult = configs[k].factor,
                .alphafunc = GNM_COMB_DST_PLUS_SRC,
                .alphasrcmult = GNM_BLEND_ZERO,
                .alphadstmult = GNM_BLEND_ONE,
                .separatealphaenable = true,
            };
            const uint32_t blend_packet[] = {
                PKT3(PKT3_SET_CONTEXT_REG, 1, 0),
                0x1e0u + t,
                (uint32_t)blend.colorsrcmult | ((uint32_t)blend.colordstmult << 8) |
                    ((uint32_t)blend.alphadstmult << 24) | (1u << 29) |
                    ((uint32_t)blend.blendenabled << 30),
            };
            st_dcb_put(&dcb, blend_packet, 3);
        }
        sceGnmDrawCmdSetRenderTargetMask(&dcb.cmd, (1u << (active * 4)) - 1);
        sceGnmDrawCmdSetScreenScissor(&dcb.cmd, 0, 0, WIDTH, HEIGHT);
        const GnmSetViewportInfo viewport = {.dmin = 0,
                                             .dmax = 1,
                                             .scale = {WIDTH * .5f, -HEIGHT * .5f, .5f},
                                             .offset = {WIDTH * .5f, HEIGHT * .5f, .5f}};
        sceGnmDrawCmdSetViewport(&dcb.cmd, 0, &viewport);
        sceGnmDrawCmdSetVsShader(&dcb.cmd, &vs, 0);
        sceGnmDrawCmdSetPsShader(&dcb.cmd, &ps[configs[k].exports - 2]);
        sceGnmDrawCmdSetPrimitiveType(&dcb.cmd, GNM_PT_TRILIST);
        sceGnmDrawCmdDrawIndexAuto(&dcb.cmd, 3);
        if (!submit_and_wait(&dcb))
            return st_fail("reason=submit case=%s", configs[k].name);
        for (unsigned t = 0; t < active; ++t) {
            const unsigned center = (HEIGHT / 2 * targets[t].pitch + WIDTH / 2) * 4;
            float got[4];
            for (unsigned c = 0; c < 4; ++c)
                got[c] = targets[t].pixels[center + c];
            printf("MEASURE case=%s mrt=%u rgba=%g,%g,%g,%g bits=%08x,%08x,%08x,%08x\n",
                   configs[k].name, t, (double)got[0], (double)got[1], (double)got[2],
                   (double)got[3], bits(got[0]), bits(got[1]), bits(got[2]), bits(got[3]));
            if (!configs[k].known && t == configs[k].target)
                continue;
            for (unsigned y = 0; y < HEIGHT; ++y)
                for (unsigned x = 0; x < WIDTH; ++x)
                    for (unsigned c = 0; c < 4; ++c) {
                        float expected = colors[t][c];
                        if (configs[k].enabled && t == configs[k].target)
                            expected = c == 3
                                           ? .25f
                                           : expected + .25f * colors[configs[k].dual ? 1 : t][3];
                        unsigned index = (y * targets[t].pitch + x) * 4 + c;
                        st_case_expect(&cases[k], "control_pixel", index, bits(expected),
                                       bits(targets[t].pixels[index]));
                    }
        }
    }
    return st_finish_cases(cases, sizeof(cases) / sizeof(cases[0]),
                           "controls_checked=1 unknown_blends=measurements_only");
}
