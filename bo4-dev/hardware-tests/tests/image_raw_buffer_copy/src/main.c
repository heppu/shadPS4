#define SHADTEST_NAME "image_raw_buffer_copy"
#include <gnm_texture.h>
#include "copy.h"
#include "read.h"
#include "fill.h"
#include "shadtest_guest.h"
#include "write.h"

enum { LANES = 64, IMAGE_BYTES = 65536, OUTPUT_WORDS = LANES + 16 };
static const uint32_t poison = 0xdeadbeefu;

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

static GnmCsStageRegisters shader(StArena* arena, const void* bytes, size_t size,
                                  uint32_t threads) {
    void* code = st_arena_alloc(arena, size, 256);
    memcpy(code, bytes, size);
    GnmCsStageRegisters regs = {0};
    sceGnmCsRegsSetAddress(&regs, code);
    regs.computepgmrsrc1 = (1u << 6) | 2u;
    regs.computepgmrsrc2 = 12u << 1;
    regs.computenumthreadx = threads;
    regs.computenumthready = 1;
    regs.computenumthreadz = 1;
    return regs;
}

int main(void) {
    st_begin();
    StArena arena;
    StQueue queue;
    if (!st_arena_init(&arena, 4 * 1024 * 1024) || !st_queue_init(&arena, &queue))
        return st_fail("reason=allocation");
    GnmCsStageRegisters writer = shader(&arena, write_shader, sizeof(write_shader), LANES);
    GnmCsStageRegisters copier = shader(&arena, copy_shader, sizeof(copy_shader), LANES);
    GnmCsStageRegisters filler = shader(&arena, fill_shader, sizeof(fill_shader), LANES);
    GnmCsStageRegisters reader = shader(&arena, read_shader, sizeof(read_shader), LANES);
    volatile uint32_t* output = st_arena_alloc(&arena, OUTPUT_WORDS * 4, 65536);
    GnmBuffer result = st_raw_buffer(output, LANES * 4);
    result.stride = 4;
    result.numrecords = LANES;
    for (unsigned layout = 0; layout < 2; ++layout) {
        volatile uint32_t* source = st_arena_alloc(&arena, IMAGE_BYTES, 65536);
        volatile uint32_t* destination = st_arena_alloc(&arena, IMAGE_BYTES, 65536);
        st_fill_words(source, IMAGE_BYTES / 4, 0x12345678u);
        st_fill_words(destination, IMAGE_BYTES / 4, 0);
        GnmTexture src = {0};
        const GnmTextureCreateInfo info = {
            .format = GNM_DATA_FORMAT_INIT(GNM_IMG_DATA_FORMAT_32, GNM_IMG_NUM_FORMAT_UINT,
                                          GNM_CHAN_X, GNM_CHAN_CONSTANT0, GNM_CHAN_CONSTANT0,
                                          GNM_CHAN_CONSTANT1),
            .texturetype = GNM_TEXTURE_2D,
            .width = 128, .height = 128, .depth = 1, .pitch = 128,
            .nummiplevels = 1, .numslices = 1, .numfragments = 1,
            .tilemodehint = layout ? GNM_TM_DISPLAY_LINEAR_ALIGNED : GNM_TM_THIN_2D_THIN,
            .mingpumode = GNM_GPU_BASE,
        };
        if (sceGnmCreateTexture(&src, &info) != GNM_ERROR_OK)
            return st_fail("reason=texture_creation layout=%u", layout);
        sceGnmTexSetBaseAddress(&src, (void*)source);
        GnmTexture dst = src;
        sceGnmTexSetBaseAddress(&dst, (void*)destination);
        GnmBuffer input = st_raw_buffer(source, IMAGE_BYTES);
        GnmBuffer target = st_raw_buffer(destination, IMAGE_BYTES);
        input.stride = target.stride = 4;
        input.numrecords = target.numrecords = IMAGE_BYTES / 4;
        for (unsigned phase = 0; phase < 3; ++phase) {
            const uint32_t base = 0x10203000u + phase * 0x10000u;
            st_fill_words(output, OUTPUT_WORDS, poison);
            StDcb dcb;
            st_dcb_begin(&dcb, &queue);
            if (phase) {
                GnmBuffer value = {0};
                ((uint32_t*)&value)[0] = base;
                sceGnmDrawCmdSetCsShader(&dcb.cmd, &writer);
                sceGnmDrawCmdSetTsharpUserData(&dcb.cmd, GNM_STAGE_CS, 0, &src);
                sceGnmDrawCmdSetVsharpUserData(&dcb.cmd, GNM_STAGE_CS, 8, &value);
                sceGnmDrawCmdDispatchDirect(&dcb.cmd, 1, 1, 1, 0);
                st_dcb_barrier(&dcb, 0);
            }
            sceGnmDrawCmdSetCsShader(&dcb.cmd, &copier);
            sceGnmDrawCmdSetVsharpUserData(&dcb.cmd, GNM_STAGE_CS, 0, &input);
            sceGnmDrawCmdSetVsharpUserData(&dcb.cmd, GNM_STAGE_CS, 4, &target);
            sceGnmDrawCmdDispatchDirect(&dcb.cmd, 1, 1, 1, 0);
            st_dcb_barrier(&dcb, 0);
            sceGnmDrawCmdSetCsShader(&dcb.cmd, &reader);
            sceGnmDrawCmdSetTsharpUserData(&dcb.cmd, GNM_STAGE_CS, 0, &dst);
            sceGnmDrawCmdSetVsharpUserData(&dcb.cmd, GNM_STAGE_CS, 8, &result);
            sceGnmDrawCmdDispatchDirect(&dcb.cmd, 1, 1, 1, 0);
            st_dcb_barrier(&dcb, 0);
            if (!submit_and_wait(&dcb))
                return st_fail("reason=submit layout=%u phase=%u", layout, phase);
            for (unsigned word = 0; word < OUTPUT_WORDS; ++word) {
                const uint32_t want = word >= LANES ? poison : phase ? base + word : 0x12345678u;
                if (output[word] != want)
                    return st_fail("layout=%u phase=%u word=%u expected=%08x got=%08x",
                                   layout, phase, word, want, output[word]);
            }
            st_emit_marker("CHECK", "layout=%u phase=%u values=%u guards=16", layout, phase, LANES);
        }
        for (unsigned offset_case = 0; offset_case < 3; ++offset_case) {
            const uint32_t offsets[] = {0, 4, 4096};
            const uint32_t base = 0x31415920u + offset_case;
            GnmBuffer value = {0};
            ((uint32_t*)&value)[0] = base;
            GnmBuffer small_input = st_raw_buffer((const uint8_t*)source + offsets[offset_case], 64);
            GnmBuffer small_output = st_raw_buffer(output, 64);
            small_input.stride = small_output.stride = 4;
            small_input.numrecords = small_output.numrecords = 16;
            st_fill_words(output, OUTPUT_WORDS, poison);
            StDcb dcb;
            st_dcb_begin(&dcb, &queue);
            sceGnmDrawCmdSetCsShader(&dcb.cmd, &filler);
            sceGnmDrawCmdSetTsharpUserData(&dcb.cmd, GNM_STAGE_CS, 0, &src);
            sceGnmDrawCmdSetVsharpUserData(&dcb.cmd, GNM_STAGE_CS, 8, &value);
            sceGnmDrawCmdDispatchDirect(&dcb.cmd, 1, 1, 1, 0);
            st_dcb_barrier(&dcb, 0);
            sceGnmDrawCmdSetCsShader(&dcb.cmd, &copier);
            sceGnmDrawCmdSetVsharpUserData(&dcb.cmd, GNM_STAGE_CS, 0, &small_input);
            sceGnmDrawCmdSetVsharpUserData(&dcb.cmd, GNM_STAGE_CS, 4, &small_output);
            sceGnmDrawCmdDispatchDirect(&dcb.cmd, 1, 1, 1, 0);
            st_dcb_barrier(&dcb, 0);
            if (!submit_and_wait(&dcb))
                return st_fail("reason=small_submit layout=%u offset=%u", layout, offsets[offset_case]);
            for (unsigned word = 0; word < OUTPUT_WORDS; ++word) {
                const uint32_t want = word < 16 ? base : poison;
                if (output[word] != want)
                    return st_fail("layout=%u offset=%u word=%u expected=%08x got=%08x",
                                   layout, offsets[offset_case], word, want, output[word]);
            }
            st_emit_marker("CHECK", "layout=%u offset=%u raw_bytes=64 guards=64", layout, offsets[offset_case]);
        }
        const uint32_t cpu_value = 0x55667788u;
        const uint32_t gpu_value = 0x88776655u;
        st_fill_words(source, IMAGE_BYTES / 4, cpu_value);
        GnmBuffer small_input = st_raw_buffer(source, 64);
        GnmBuffer small_output = st_raw_buffer(output, 64);
        small_input.stride = small_output.stride = 4;
        small_input.numrecords = small_output.numrecords = 16;
        for (unsigned phase = 0; phase < 2; ++phase) {
            st_fill_words(output, OUTPUT_WORDS, poison);
            StDcb dcb;
            st_dcb_begin(&dcb, &queue);
            if (phase) {
                GnmBuffer value = {0};
                ((uint32_t*)&value)[0] = gpu_value;
                sceGnmDrawCmdSetCsShader(&dcb.cmd, &filler);
                sceGnmDrawCmdSetTsharpUserData(&dcb.cmd, GNM_STAGE_CS, 0, &src);
                sceGnmDrawCmdSetVsharpUserData(&dcb.cmd, GNM_STAGE_CS, 8, &value);
                sceGnmDrawCmdDispatchDirect(&dcb.cmd, 1, 1, 1, 0);
                st_dcb_barrier(&dcb, 0);
            }
            sceGnmDrawCmdSetCsShader(&dcb.cmd, &copier);
            sceGnmDrawCmdSetVsharpUserData(&dcb.cmd, GNM_STAGE_CS, 0, &small_input);
            sceGnmDrawCmdSetVsharpUserData(&dcb.cmd, GNM_STAGE_CS, 4, &small_output);
            sceGnmDrawCmdDispatchDirect(&dcb.cmd, 1, 1, 1, 0);
            st_dcb_barrier(&dcb, 0);
            if (!submit_and_wait(&dcb))
                return st_fail("reason=cpu_then_gpu_submit layout=%u phase=%u", layout, phase);
            for (unsigned word = 0; word < OUTPUT_WORDS; ++word) {
                const uint32_t want = word < 16 ? (phase ? gpu_value : cpu_value) : poison;
                if (output[word] != want)
                    return st_fail("cpu_then_gpu layout=%u phase=%u word=%u expected=%08x got=%08x",
                                   layout, phase, word, want, output[word]);
            }
        }
        st_emit_marker("CHECK", "layout=%u cpu_then_gpu=1 raw_bytes=64 guards=64", layout);
    }
    st_emit_marker("PASS", "layouts=2 gpu_writes=2 raw_copy_bytes=65536 partial_offsets=3 cpu_then_gpu=1 guards=1");
    st_arena_destroy(&arena);
    return 0;
}
