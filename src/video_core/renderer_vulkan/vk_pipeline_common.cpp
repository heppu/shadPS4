// SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <boost/container/static_vector.hpp>

#include "common/assert.h"

#include "shader_recompiler/resource.h"
#include "video_core/renderer_vulkan/vk_instance.h"
#include "video_core/renderer_vulkan/vk_pipeline_cache.h"
#include "video_core/renderer_vulkan/vk_pipeline_common.h"
#include "video_core/renderer_vulkan/vk_resource_pool.h"
#include "video_core/renderer_vulkan/vk_scheduler.h"

namespace Vulkan {

Pipeline::Pipeline(const Instance& instance_, Scheduler& scheduler_, DescriptorHeap& desc_heap_,
                   const Shader::Profile& profile_, vk::PipelineCache pipeline_cache,
                   bool is_compute_ /*= false*/)
    : instance{instance_}, scheduler{scheduler_}, desc_heap{desc_heap_}, profile{profile_},
      is_compute{is_compute_} {}

Pipeline::~Pipeline() = default;

void Pipeline::CreatePipelineLayout(const vk::PushConstantRange& push_constants) {
    boost::container::small_vector<vk::DescriptorSetLayout, 4> set_layouts;
    set_layouts.push_back(*desc_layout);
    for (const auto* stage : GetStages()) {
        if (!stage) {
            continue;
        }
        for (const auto& image : stage->images) {
            if (image.IsTable()) {
                set_layouts.push_back(
                    desc_heap.TableLayout(vk::DescriptorType::eSampledImage, image.table_size));
            }
        }
        for (const auto& sampler : stage->samplers) {
            if (sampler.IsTable()) {
                set_layouts.push_back(
                    desc_heap.TableLayout(vk::DescriptorType::eSampler, sampler.table_size));
            }
        }
    }
    const vk::PipelineLayoutCreateInfo layout_info = {
        .setLayoutCount = static_cast<u32>(set_layouts.size()),
        .pSetLayouts = set_layouts.data(),
        .pushConstantRangeCount = 1U,
        .pPushConstantRanges = &push_constants,
    };
    auto [layout_result, layout] = instance.GetDevice().createPipelineLayoutUnique(layout_info);
    ASSERT_MSG(layout_result == vk::Result::eSuccess, "Failed to create pipeline layout: {}",
               vk::to_string(layout_result));
    pipeline_layout = std::move(layout);
}

void Pipeline::BindResources(DescriptorWrites& set_writes, const Shader::PushData& push_data,
                             std::span<const vk::DescriptorSet> table_sets) const {
    const auto cmdbuf = scheduler.CommandBuffer();
    const auto bind_point =
        IsCompute() ? vk::PipelineBindPoint::eCompute : vk::PipelineBindPoint::eGraphics;

    const auto stage_flags = IsCompute() ? vk::ShaderStageFlagBits::eCompute : AllGraphicsStageBits;
    cmdbuf.pushConstants(*pipeline_layout, stage_flags, 0u, sizeof(push_data), &push_data);

    if (!set_writes.empty()) {
        if (uses_push_descriptors) {
            cmdbuf.pushDescriptorSetKHR(bind_point, *pipeline_layout, 0, set_writes);
        } else {
            const auto desc_set = desc_heap.Commit(*desc_layout);
            for (auto& set_write : set_writes) {
                set_write.dstSet = desc_set;
            }
            instance.GetDevice().updateDescriptorSets(set_writes, {});
            cmdbuf.bindDescriptorSets(bind_point, *pipeline_layout, 0, desc_set, {});
        }
    }

    if (!table_sets.empty()) {
        cmdbuf.bindDescriptorSets(bind_point, *pipeline_layout, 1,
                                  static_cast<u32>(table_sets.size()), table_sets.data(), 0,
                                  nullptr);
    }
}

std::string Pipeline::GetDebugString() const {
    std::string stage_desc;
    for (const auto& stage : stages) {
        if (stage) {
            const auto shader_name = PipelineCache::GetShaderName(stage->hw_stage, stage->pgm_hash);
            if (stage_desc.empty()) {
                stage_desc = shader_name;
            } else {
                stage_desc = fmt::format("{},{}", stage_desc, shader_name);
            }
        }
    }
    return stage_desc;
}

} // namespace Vulkan
