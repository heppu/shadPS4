#define SHADTEST_NAME "array_image_readback"
#include <gnm_texture.h>
#include "shadtest_guest.h"
#include "write.h"
#include "write_rgba.h"
#include "write_packed.h"

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

int main(void) {
    st_begin();
    StArena arena;
    StQueue queue;
    if (!st_arena_init(&arena, 64 * 1024 * 1024) || !st_queue_init(&arena, &queue))
        return st_fail("reason=allocation");
    void* code = st_arena_alloc(&arena, sizeof(write_shader), 256);
    memcpy(code, write_shader, sizeof(write_shader));
    GnmCsStageRegisters writer = {0};
    sceGnmCsRegsSetAddress(&writer, code);
    writer.computepgmrsrc1 = (1u << 6) | 1u | (0xc0u << 12);
    writer.computepgmrsrc2 = (12u << 1) | (1u << 7);
    writer.computenumthreadx = 64;
    writer.computenumthready = writer.computenumthreadz = 1;
    void* rgba_code = st_arena_alloc(&arena, sizeof(write_rgba_shader), 256);
    memcpy(rgba_code, write_rgba_shader, sizeof(write_rgba_shader));
    GnmCsStageRegisters rgba_writer = writer;
    sceGnmCsRegsSetAddress(&rgba_writer, rgba_code);
    void* packed_code = st_arena_alloc(&arena, sizeof(write_packed_shader), 256);
    memcpy(packed_code, write_packed_shader, sizeof(write_packed_shader));
    GnmCsStageRegisters packed_writer = writer;
    sceGnmCsRegsSetAddress(&packed_writer, packed_code);
    StCase cases[8] = {{.name="linear_uint"}, {.name="tiled_uint"},
                       {.name="linear_float"}, {.name="tiled_float"},
                       {.name="partial_workgroup"}, {.name="rgba_uint"}, {.name="packed_float"}, {.name="guards"}};
    for (unsigned phase = 0; phase < 7; ++phase) {
        const bool tiled = phase == 1 || phase == 3 || phase == 6;
        const bool is_float = phase == 2 || phase == 3 || phase == 6;
        const bool rgba = phase == 5;
        const bool packed = phase == 6;
        const unsigned layers = phase >= 4 ? 33 : 4096;
        const unsigned width = phase == 4 ? 5 : 8;
        const unsigned height = phase == 4 ? 7 : 8;
        GnmTexture texture = {0};
        const GnmTextureCreateInfo info = {
            .format = GNM_DATA_FORMAT_INIT(packed ? GNM_IMG_DATA_FORMAT_10_11_11 : rgba ? GNM_IMG_DATA_FORMAT_8_8_8_8 : GNM_IMG_DATA_FORMAT_32,
                         is_float ? GNM_IMG_NUM_FORMAT_FLOAT : GNM_IMG_NUM_FORMAT_UINT,
                         GNM_CHAN_X, (rgba || packed) ? GNM_CHAN_Y : GNM_CHAN_CONSTANT0,
                         (rgba || packed) ? GNM_CHAN_Z : GNM_CHAN_CONSTANT0, (rgba || packed) ? GNM_CHAN_W : GNM_CHAN_CONSTANT1),
            .texturetype = GNM_TEXTURE_2D_ARRAY,
            .width = width, .height = height, .depth = 1, .pitch = phase == 4 ? 64 : 8,
            .nummiplevels = 1, .numslices = layers, .numfragments = 1,
            .tilemodehint = tiled ? GNM_TM_THIN_1D_THIN : GNM_TM_DISPLAY_LINEAR_ALIGNED,
            .mingpumode = GNM_GPU_BASE,
        };
        if (sceGnmCreateTexture(&texture, &info) != GNM_ERROR_OK)
            return st_fail("reason=texture phase=%u", phase);
        if (phase == 4)
            sceGnmTexSetPitch(&texture, 64);
        texture.lastarray = layers - 1;
        GnmTexture single_layer = texture;
        sceGnmTexSetDepth(&single_layer, 1);
        single_layer.lastarray = 0;
        uint64_t bytes = 0;
        uint32_t alignment = 0;
        if (sceGnmTexCalcByteSize(&bytes, &alignment, &single_layer) != GNM_ERROR_OK)
            return st_fail("reason=size phase=%u", phase);
        const unsigned layer_words = bytes / 4;
        bytes *= sceGnmTexGetNumArraySlices(&texture);
        const uint64_t allocated = (bytes + 65535) & ~65535ull;
        volatile uint32_t* pixels = st_arena_alloc(&arena, allocated + 65536, 65536);
        if (!pixels)
            return st_fail("reason=pixels phase=%u bytes=%llu pitch=%u layers=%u", phase,
                           (unsigned long long)bytes, sceGnmTexGetPitch(&texture), layers);
        st_fill_words(pixels, (allocated + 65536) / 4, 0xdeadbeef);
        sceGnmTexSetBaseAddress(&texture, (void*)pixels);
        const unsigned pitch = sceGnmTexGetPitch(&texture);
        for (unsigned iteration = 0; iteration < 2; ++iteration) {
            const uint32_t base = (is_float ? 0x3f000000u : 0x87654321u) ^ (iteration << 20);
            GnmBuffer parameters = {0};
            ((uint32_t*)&parameters)[0] = tiled ? 0 : 63;
            ((uint32_t*)&parameters)[1] = packed ? (iteration ? 0x40000000u : 0x3f800000u) : base;
            StDcb dcb;
            st_dcb_begin(&dcb, &queue);
            sceGnmDrawCmdSetCsShader(&dcb.cmd, packed ? &packed_writer : rgba ? &rgba_writer : &writer);
            sceGnmDrawCmdSetTsharpUserData(&dcb.cmd, GNM_STAGE_CS, 0, &texture);
            sceGnmDrawCmdSetVsharpUserData(&dcb.cmd, GNM_STAGE_CS, 8, &parameters);
            st_dcb_barrier(&dcb, 0);
            sceGnmDrawCmdDispatchDirect(&dcb.cmd, layers, 1, 1, 0);
            st_dcb_barrier(&dcb, 0);
            if (!submit_and_wait(&dcb))
                return st_fail("reason=submit phase=%u", phase);
            for (unsigned layer = 0; layer < layers; ++layer)
                for (unsigned y = 0; y < height; ++y)
                    for (unsigned x = 0; x < width; ++x) {
                        const unsigned index = layer * layer_words + y * (tiled ? 8 : pitch) + x;
                        const uint32_t expected = packed ? (iteration ? 0x80200400u : 0x781e03c0u) :
                            base ^ (layer << 6) ^ (tiled ? 0 : y * 8 + x);
                        st_case_expect(&cases[phase], "word", index, expected, pixels[index]);
                    }
            for (unsigned i = 0; i < 16; ++i)
                st_case_expect(&cases[7], "word", phase * 32 + iteration * 16 + i,
                               0xdeadbeef, pixels[allocated / 4 + i]);
        }
    }
    return st_finish_cases(cases, 8, "layers=4096 layouts=2 formats=4 partial=1 repeats=2 guards=1");
}
