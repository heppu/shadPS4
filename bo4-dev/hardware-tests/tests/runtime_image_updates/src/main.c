#define SHADTEST_NAME "runtime_image_updates"
#include <gnm_sampler.h>
#include <gnm_texture.h>
#include "copy.h"
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
    if (!dcb->ok || sceGnmSubmitCommandBuffers(1, commands, sizes, NULL, NULL) < 0) {
        return false;
    }
    return st_wait_label(dcb->queue->label, value);
}

int main(void) {
    st_begin();
    StArena arena;
    StQueue queue;
    if (!st_arena_init(&arena, 4 * 1024 * 1024) || !st_queue_init(&arena, &queue))
        return st_fail("reason=allocation");
    void* code = st_arena_alloc(&arena, sizeof(copy_shader), 256);
    memcpy(code, copy_shader, sizeof(copy_shader));
    GnmCsStageRegisters cs = {0};
    sceGnmCsRegsSetAddress(&cs, code);
    cs.computepgmrsrc1 = 2u | (3u << 6) | (0xc0u << 12);
    cs.computepgmrsrc2 = (12u << 1) | (1u << 7);
    cs.computenumthreadx = 64;
    cs.computenumthready = cs.computenumthreadz = 1;
    GnmTexture* textures = st_arena_alloc(&arena, 2 * sizeof(GnmTexture), 65536);
    float* pixels[2];
    uint64_t sizes[2];
    for (unsigned t = 0; t < 2; ++t) {
        GnmTextureCreateInfo info = {.format = GNM_FMT_R32G32B32A32_FLOAT,
                                     .texturetype = GNM_TEXTURE_2D,
                                     .width = 64,
                                     .height = 64,
                                     .depth = 1,
                                     .pitch = 64,
                                     .nummiplevels = 1,
                                     .numslices = 1,
                                     .numfragments = 1,
                                     .tilemodehint = GNM_TM_DISPLAY_LINEAR_ALIGNED,
                                     .mingpumode = GNM_GPU_BASE};
        if (sceGnmCreateTexture(&textures[t], &info) != GNM_ERROR_OK)
            return st_fail("reason=texture_create");
        uint64_t bytes = 0;
        uint32_t alignment = 0;
        if (sceGnmTexCalcByteSize(&bytes, &alignment, &textures[t]) != GNM_ERROR_OK)
            return st_fail("reason=texture_size");
        float* data = st_arena_alloc(&arena, bytes, 65536);
        if (!data)
            return st_fail("reason=texture_memory");
        pixels[t] = data;
        sizes[t] = bytes;
        for (unsigned i = 0; i < bytes / sizeof(float); ++i)
            data[i] = t ? .25f : .125f;
        sceGnmTexSetBaseAddress(&textures[t], data);
    }
    void* alias = NULL;
    if (!st_dmem_map(&alias, arena.size, arena.direct_memory, false))
        return st_fail("reason=alias_mapping");
    GnmBuffer table = st_raw_buffer(textures, 2 * sizeof(GnmTexture));
    table.stride = sizeof(GnmTexture);
    table.numrecords = 2;
    GnmSampler sampler = {0};
    StCase cases[10] = {{.name = "initial"},
                       {.name = "cpu_update_one"},
                       {.name = "cpu_update"},
                       {.name = "descriptor_swap"},
                       {.name = "submit_boundary"},
                       {.name = "physical_alias"},
                       {.name = "remap_reuse"},
                       {.name = "unrelated_remap"},
                       {.name = "descriptor_remap"},
                       {.name = "guards"}};
    for (unsigned phase = 0; phase < 9; ++phase) {
        if (phase == 1) {
            for (unsigned i = 0; i < sizes[0] / 4; ++i)
                pixels[0][i] = .5f;
            st_store_fence();
        }
        if (phase == 2) {
            for (unsigned t = 0; t < 2; ++t)
                for (unsigned i = 0; i < sizes[t] / 4; ++i)
                    pixels[t][i] = t ? .75f : .5f;
            st_store_fence();
        }
        if (phase == 3) {
            GnmTexture tmp = textures[0];
            textures[0] = textures[1];
            textures[1] = tmp;
            st_store_fence();
        }
        if (phase == 5) {
            for (unsigned t = 0; t < 2; ++t) {
                float* alias_pixels = (float*)((uint8_t*)alias +
                                               ((uint8_t*)pixels[t] - arena.base));
                for (unsigned i = 0; i < sizes[t] / 4; ++i)
                    alias_pixels[i] = t ? .875f : .625f;
            }
            st_store_fence();
        }
        if (phase == 6) {
            for (unsigned t = 0; t < 2; ++t) {
                off_t replacement;
                void* address = pixels[t];
                if (sceKernelMunmap(address, sizes[t]) != 0 ||
                    !st_dmem_alloc_in(0, (off_t)sceKernelGetDirectMemorySize(), sizes[t],
                                      ST_DMEM_PAGE_BYTES, &replacement) ||
                    !st_dmem_map(&address, sizes[t], replacement, true))
                    return st_fail("reason=remap_reuse");
                float* old_pixels = (float*)((uint8_t*)alias +
                                             ((uint8_t*)pixels[t] - arena.base));
                for (unsigned i = 0; i < sizes[t] / 4; ++i) {
                    pixels[t][i] = t ? .4375f : .3125f;
                    old_pixels[i] = t ? .1875f : .0625f;
                }
            }
            st_store_fence();
        }
        if (phase == 7) {
            off_t unrelated_physical;
            void* unrelated = NULL;
            if (!st_dmem_alloc_in(0, (off_t)sceKernelGetDirectMemorySize(), 65536,
                                  ST_DMEM_PAGE_BYTES, &unrelated_physical) ||
                !st_dmem_map(&unrelated, 65536, unrelated_physical, false) ||
                sceKernelMunmap(unrelated, 65536) != 0 ||
                sceKernelReleaseDirectMemory(unrelated_physical, 65536) != 0)
                return st_fail("reason=unrelated_remap");
        }
        if (phase == 8) {
            GnmTexture replacement_descriptors[2] = {textures[1], textures[0]};
            off_t replacement;
            void* address = textures;
            if (sceKernelMunmap(address, 65536) != 0 ||
                !st_dmem_alloc_in(0, (off_t)sceKernelGetDirectMemorySize(), 65536,
                                  ST_DMEM_PAGE_BYTES, &replacement) ||
                !st_dmem_map(&address, 65536, replacement, true))
                return st_fail("reason=descriptor_remap");
            memcpy(textures, replacement_descriptors, sizeof(replacement_descriptors));
            st_store_fence();
        }
        if (phase >= 4 && phase < 7 && sceGnmSubmitDone() < 0)
            return st_fail("reason=submit_done");
        volatile uint32_t* output = st_arena_alloc(&arena, 144 * 4, 65536);
        st_fill_words(output, 144, 0xdeadbeef);
        GnmBuffer result = st_raw_buffer(output + 8, 128 * 4);
        result.stride = 4;
        result.numrecords = 128;
        StDcb dcb;
        st_dcb_begin(&dcb, &queue);
        sceGnmDrawCmdSetCsShader(&dcb.cmd, &cs);
        sceGnmDrawCmdSetVsharpUserData(&dcb.cmd, GNM_STAGE_CS, 0, &table);
        sceGnmDrawCmdSetSsharpUserData(&dcb.cmd, GNM_STAGE_CS, 4, &sampler);
        sceGnmDrawCmdSetVsharpUserData(&dcb.cmd, GNM_STAGE_CS, 8, &result);
        st_dcb_barrier(&dcb, 0);
        sceGnmDrawCmdDispatchDirect(&dcb.cmd, 2, 1, 1, 0);
        st_dcb_barrier(&dcb, 0);
        if (!submit_and_wait(&dcb))
            return st_fail("reason=submit");
        for (unsigned i = 0; i < 144; ++i) {
            uint32_t expected = 0xdeadbeef;
            unsigned which = 9;
            if (i >= 8 && i < 136) {
                unsigned group = (i - 8) / 64;
                which = phase;
                float value = phase == 0   ? (group ? .25f : .125f)
                              : phase == 1 ? (group ? .25f : .5f)
                              : phase == 2 ? (group ? .75f : .5f)
                              : phase == 5 ? (group ? .625f : .875f)
                              : phase == 6 || phase == 7 ? (group ? .3125f : .4375f)
                              : phase == 8 ? (group ? .4375f : .3125f)
                                           : (group ? .5f : .75f);
                memcpy(&expected, &value, 4);
            }
            st_case_expect(&cases[which], "word", i, expected, output[i]);
        }
    }
    sceGnmSubmitDone();
    return st_finish_cases(cases, 10,
                           "cpu_update_one=1 cpu_update=1 descriptor_swap=1 submit_boundary=1 physical_alias=1 remap_reuse=1 unrelated_remap=1 descriptor_remap=1 guards=1");
}
