// SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <unordered_map>
#include "common/recursive_lock.h"
#include "common/shared_first_mutex.h"
#include "video_core/buffer_cache/buffer_cache.h"
#include "video_core/page_manager.h"
#include "video_core/renderer_vulkan/vk_pipeline_cache.h"
#include "video_core/renderer_vulkan/vk_scheduler.h"
#include "video_core/texture_cache/texture_cache.h"

namespace AmdGpu {
struct Liverpool;
}

namespace Core {
class MemoryManager;
}

namespace Vulkan {

class GraphicsPipeline;
class Runtime;

class Rasterizer {
public:
    explicit Rasterizer(const Instance& instance, Scheduler& scheduler, Runtime& runtime,
                        AmdGpu::Liverpool* liverpool);
    ~Rasterizer();

    [[nodiscard]] Scheduler& GetScheduler() noexcept {
        return scheduler;
    }

    [[nodiscard]] Runtime& GetRuntime() noexcept {
        return runtime;
    }

    [[nodiscard]] VideoCore::BufferCache& GetBufferCache() noexcept {
        return buffer_cache;
    }

    [[nodiscard]] VideoCore::TextureCache& GetTextureCache() noexcept {
        return texture_cache;
    }

    void Draw(bool is_indexed, u32 index_offset = 0);
    void DrawIndirect(bool is_indexed, VAddr arg_address, u32 offset, u32 size, u32 max_count,
                      VAddr count_address, u16 vertex_sgpr_offset, u16 instance_sgpr_offset);

    void DispatchDirect();
    void DispatchIndirect(VAddr address, u32 offset, u32 size);

    void ScopeMarker(fmt::string_view fmt, fmt::format_args args, auto&& func) {
        if (host_markers_enabled) {
            ScopeMarkerBegin(fmt::vformat(fmt, args));
            func();
            ScopeMarkerEnd();
        } else {
            func();
        }
    }

    void ScopeMarkerBegin(const std::string_view& str, bool from_guest = false);
    void ScopeMarkerEnd(bool from_guest = false);
    void ScopedMarkerInsert(const std::string_view& str, bool from_guest = false);
    void ScopedMarkerInsertColor(const std::string_view& str, const u32 color,
                                 bool from_guest = false);

    void FillBuffer(VAddr address, u32 num_bytes, u32 value, bool is_gds);
    void CopyBuffer(VAddr dst, VAddr src, u32 num_bytes, bool dst_gds, bool src_gds);
    u32 ReadDataFromGds(u32 gsd_offset);
    bool InvalidateMemory(VAddr addr, u64 size, bool assume_locks = false);
    bool ReadMemory(VAddr addr, u64 size, bool assume_locks = false);
    bool IsMapped(VAddr addr, u64 size);
    void MapMemory(VAddr addr, u64 size);
    void RegisterMemory(VAddr addr, u64 size);
    void UnmapMemory(VAddr addr, u64 size);

    u64 Flush();
    void Finish();
    void OnSubmit();
    void OnFence();
    void InvalidateImageTables();

    PipelineCache& GetPipelineCache() {
        return pipeline_cache;
    }

    template <typename Func>
    void ForEachMappedRangeInRange(VAddr addr, u64 size, Func&& func) {
        const auto range = decltype(mapped_ranges)::interval_type::right_open(addr, addr + size);
        Common::RecursiveSharedLock lock{mapped_ranges_mutex};
        for (const auto& mapped_range : (mapped_ranges & range)) {
            func(mapped_range);
        }
    }

    std::thread::id GetGpuCommandProcessorThread();
#ifdef __linux__
    u32 GetGpuCommandProcessorThreadId();
#endif

private:
    void PrepareRenderState(const GraphicsPipeline* pipeline);
    RenderState BeginRendering(const GraphicsPipeline* pipeline);
    void Resolve();
    void DepthStencilCopy(bool is_depth, bool is_stencil);
    void EliminateFastClear();

    void UpdateDynamicState(const GraphicsPipeline* pipeline, bool is_indexed) const;
    void UpdateViewportScissorState() const;
    void UpdateDepthStencilState() const;
    void UpdatePrimitiveState(bool is_indexed) const;
    void UpdateRasterizationState() const;
    void UpdateColorBlendingState(const GraphicsPipeline* pipeline) const;

    bool FilterDraw();

    void BindBuffers(const Shader::Info& stage, Shader::Backend::Bindings& binding,
                     Shader::PushData& push_data);
    void BindTextures(const Shader::Info& stage, Shader::Backend::Bindings& binding);
    vk::Sampler ResolveSampler(AmdGpu::Sampler ssharp, bool is_depth);
    void ResolveSamplerTable(const Shader::SamplerResource& sampler, const AmdGpu::Buffer& table,
                             const Shader::Info& stage);
    vk::DescriptorSet GetSamplerTableSet(const Shader::SamplerResource& sampler,
                                         const Shader::Info& stage);
    u64 TableEpoch() const;
    void DropRemappedTableKeys();
    u64 TableImageKey(const AmdGpu::Image& tsharp, AmdGpu::ImageType table_type,
                      bool is_depth = false);
    u64 TableImageFullHash(const AmdGpu::Image& tsharp, AmdGpu::ImageType table_type);
    u64 TableImageContentHash(const AmdGpu::Image& tsharp, AmdGpu::ImageType table_type) const;
    vk::ImageView ResolveDepthTableImage(const AmdGpu::Image& tsharp, u64 key,
                                         const Shader::ImageResource& image_desc);
    vk::ImageView ResolveTableImage(const AmdGpu::Image& tsharp, u64& key,
                                    const Shader::ImageResource* image_desc, bool& is_pending);
    u32 AcquireImageTableSet(u32 num_entries);
    void BuildImageTableSet(u32 table_index, const Shader::ImageResource* image_desc);
    vk::DescriptorSet GetImageTableSet(const Shader::ImageResource& image_desc,
                                       const Shader::Info& stage);
    void ValidateImageTableSets();
    bool BindResources(const Pipeline* pipeline);

    void BindVertexBuffers(const GraphicsPipeline* pipeline);
    void BindIndexBuffer(u32 index_offset = 0, bool is_indirect = false);

    void ResetBindings(bool is_compute);

    void FlushPeriodically();

    bool IsComputeMetaClear(const Pipeline* pipeline);
    bool IsComputeImageCopy(const Pipeline* pipeline);
    bool IsComputeImageClear(const Pipeline* pipeline);

private:
    friend class VideoCore::BufferCache;

    const Instance& instance;
    Scheduler& scheduler;
    Runtime& runtime;
    VideoCore::PageManager page_manager;
    VideoCore::BufferCache buffer_cache;
    VideoCore::TextureCache texture_cache;
    AmdGpu::Liverpool* liverpool;
    Core::MemoryManager* memory;
    boost::icl::interval_set<VAddr> mapped_ranges;
    Common::SharedFirstMutex mapped_ranges_mutex;
    PipelineCache pipeline_cache;
    const bool host_markers_enabled;
    const bool guest_markers_enabled;
    const u32 submit_draw_interval;
    u64 last_submit_tick{};
    u32 draws_since_submit{};

    struct ImageBinding {
        VideoCore::ImageId image_id;
        VideoCore::TextureCache::ImageDesc desc;
    };
    std::array<ImageBinding, Shader::NUM_IMAGES> image_bindings;
    std::array<ImageBinding, AmdGpu::NUM_COLOR_BUFFERS> cb_descs;
    ImageBinding db_desc;

    std::vector<vk::DescriptorImageInfo> image_infos;

    struct SamplerTableKey {
        u64 content_hash;
        u64 border_color_base;
        u32 num_entries;
        u32 stride_dw;
        u32 offset_dw;
        Shader::SharpFetchPostOp post_op;
        bool is_depth;

        bool operator==(const SamplerTableKey&) const = default;
    };
    struct SamplerTableSet {
        SamplerTableKey key;
        vk::DescriptorSet set;
        u64 touch_tick;
    };
    std::vector<SamplerTableSet> sampler_table_sets;
    std::vector<vk::DescriptorImageInfo> sampler_table_infos;
    boost::container::small_vector<vk::DescriptorSet, 4> table_sets;
    u64 sampler_table_generation{};
    u64 sampler_table_binds{};

    /// A texture resolved from an image table entry, shared by every index that names it.
    struct TableImage {
        VideoCore::ImageId image_id{};
        u64 image_uid{};
        vk::ImageView view{};
        u64 used_tick{};
        u64 synced_tick{};
        u64 synced_generation{};
        VAddr resolved_address{};
        u64 full_hash{};
        u64 verify_tick{};
        u32 verify_interval{};
    };
    /// One immutable version of a table's descriptors.
    struct ImageTableSet {
        vk::DescriptorSet set;
        u32 num_entries{};
        u64 bound_tick{};
        u64 generation{};
        bool is_current{};
        std::vector<u64> keys;
        std::vector<vk::ImageView> views;
    };
    struct ImageTable {
        VAddr address{};
        u32 bytes{};
        u32 stride_dw{};
        u32 offset_dw{};
        u32 num_entries{};
        AmdGpu::ImageType type{};
        std::vector<u8> shadow;
        u64 checked_tick{};
        u64 checked_generation{};
        s32 current_set{-1};
        bool has_unresolved{};
        bool is_depth{};
        bool is_r128{};
    };
    struct BoundImageTable {
        u32 table;
        u32 slot;
    };
    std::unordered_map<u64, TableImage> table_images;
    std::unordered_map<u32, u64> table_image_owners;
    struct TableKey {
        u64 key;
        u64 epoch;
        VAddr begin;
        VAddr end;
    };
    std::unordered_map<u64, TableKey> table_keys;
    std::vector<std::pair<VAddr, VAddr>> table_remapped_ranges;
    std::array<u32, 64> last_image_tables{};
    std::vector<ImageTableSet> image_table_sets;
    std::vector<ImageTable> image_tables;
    boost::container::small_vector<BoundImageTable, 2> bound_image_tables;
    std::vector<u64> image_table_keys;
    std::vector<vk::ImageView> image_table_views;
    std::vector<vk::DescriptorImageInfo> image_table_infos;
    u64 table_cache_generation{};
    u64 table_evict_tick{};
    u32 table_steals{};
    u64 table_verify_tick{};
    u64 table_verify_bytes{};
    std::vector<u8> table_verify_data;
    boost::container::static_vector<vk::DescriptorBufferInfo, Shader::NUM_BUFFERS> buffer_infos;

    struct BoundBuffer {
        const VideoCore::Buffer* buffer;
        u64 offset;
        u32 size;
        vk::AccessFlags2 src_access;
    };
    std::vector<BoundBuffer> bound_buffers;
    std::vector<VideoCore::ImageId> bound_images;

    u32 set_write_index{};
    Pipeline::DescriptorWrites set_writes;
    Shader::PushData push_data;

    bool attachment_feedback_loop{};
    bool needs_barrier{};
};

} // namespace Vulkan
