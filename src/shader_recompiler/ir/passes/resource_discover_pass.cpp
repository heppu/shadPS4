// SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <algorithm>
#include <limits>
#include "common/assert.h"
#include "shader_recompiler/ir/ir_emitter.h"
#include "shader_recompiler/ir/passes/ir_passes.h"
#include "shader_recompiler/ir/passes/resource_pass.h"
#include "shader_recompiler/ir/program.h"
#include "shader_recompiler/profile.h"
#include "shader_recompiler/resource.h"

namespace Shader::Optimization {

static bool IsSharpSource(const IR::Inst* inst) {
    return inst->GetOpcode() == IR::Opcode::GetUserData ||
           inst->GetOpcode() == IR::Opcode::ReadConst ||
           inst->GetOpcode() == IR::Opcode::ReadConstBuffer;
}

struct StridePatchResult {
    IR::Value vsharp_dw1;
    u32 dw1_mask;
    bool found;
};
StridePatchResult CheckStridePatchPattern(IR::Value value) {
    // s_or_b32        s37, s101, 0x400000
    // s_mov_b32       s36, s100
    // s_mov_b32       s38, -1
    // s_mov_b32       s39, 0x2000c004
    // s_movk_i32      s0, 0x6d0
    // buffer_load_dwordx4 v[17:20], v59, s[36:39], s0 idxen

    auto* inst = value.TryInst();
    if (!inst) {
        return {value, 0, false};
    }
    if (inst->GetOpcode() != IR::Opcode::BitwiseOr32 || !inst->Arg(1).IsImmediate()) {
        return {value, 0, false};
    }
    return {inst->Arg(0), inst->Arg(1).U32(), true};
}

struct ClearStrideAndPatchRecordsResult {
    IR::Value vsharp_dw1;
    IR::Value vsharp_dw2;
    u32 records_shift;
    bool found;
};
ClearStrideAndPatchRecordsResult CheckClearStrideAndPatchRecordsPattern(IR::Value dword1,
                                                                        IR::Value dword2) {
    // s_and_b32       s21, s21, 0xffff
    // s_lshl_b32      s22, s22, <imm>
    // s_buffer_load_dwordx4 s[20:23], s[20:23], 0x0

    auto* dw1_inst = dword1.TryInst();
    if (!dw1_inst) {
        return {dword1, dword2, false};
    }
    if (dw1_inst->GetOpcode() != IR::Opcode::BitwiseAnd32 || !dw1_inst->Arg(1).IsImmediate() ||
        dw1_inst->Arg(1).U32() != 0xffffu) {
        return {dword1, dword2, false};
    }
    auto* dw2_inst = dword2.TryInst();
    if (!dw2_inst) {
        return {dword1, dword2, false};
    }
    if (dw2_inst->GetOpcode() != IR::Opcode::ShiftLeftLogical32 ||
        !dw2_inst->Arg(1).IsImmediate()) {
        return {dword1, dword2, false};
    }

    return {dw1_inst->Arg(0), dw2_inst->Arg(0), dw2_inst->Arg(1).U32(), true};
}

struct InlineCbufResult {
    IR::Value vsharp_dw0;
    bool found;
};
InlineCbufResult CheckInlineCbufPattern(IR::Value value) {
    // Assuming V# is in s[32:35]
    // The next pattern:
    //  s_getpc_b64 s[32:33]
    //  s_add_u32   s32, <const>, s32
    //  s_addc_u32  s33, 0, s33
    //  s_mov_b32   s35, <const>
    //  s_movk_i32  s34, <const>
    //  buffer_load_format_xyz v[8:10], v1, s[32:35], 0 ...
    // is used to define an inline constant buffer

    auto* inst = value.TryInst();
    if (!inst) {
        return {value, false};
    }
    if (inst->GetOpcode() != IR::Opcode::IAdd32 ||
        inst->Arg(0).IsImmediate() == inst->Arg(1).IsImmediate()) {
        return {value, false};
    }
    const u32 offset = inst->Arg(0).IsImmediate() ? inst->Arg(0).U32() : inst->Arg(1).U32();
    auto* prod = inst->Arg(0).IsImmediate() ? inst->Arg(1).Inst() : inst->Arg(0).Inst();
    if (prod->GetOpcode() != IR::Opcode::GetPcLo) {
        return {value, false};
    }
    return {IR::Value{prod->Arg(0).U32() + offset}, true};
}

struct CubeTo2DArrayResult {
    IR::Value tsharp_dw3;
    IR::Value tsharp_dw4;
    bool found;
};
CubeTo2DArrayResult CheckCubeTo2DArrayPattern(IR::Value dword3, IR::Value dword4) {
    // Assuming T# is in s[12:19]
    // The next pattern:
    //  s_bfe_u32       vcc_lo, s16, 0xd0000
    //  s_mul_i32       vcc_lo, vcc_lo, 6
    //  s_add_i32       vcc_lo, vcc_lo, 5
    //  s_and_b32       vcc_hi, 0x1fff, vcc_lo
    //  s_and_b32       vcc_lo, s16, 0xffffe000
    //  s_or_b32        s16, vcc_hi, vcc_lo
    //  s_and_b32       vcc_lo, s15, 0xfffffff
    //  s_or_b32        s15, 0xd0000000, vcc_lo
    //  image_load_mip  v[4:7], v[0:3], s[12:19] dmask:15 da
    // is used to convert the resource from cubemap to 2d-array

    // Check pattern that forces type to Color2DArray
    auto* dw3_inst = dword3.TryInst();
    if (!dw3_inst) {
        return {dword3, dword4, false};
    }

    if (dw3_inst->GetOpcode() != IR::Opcode::BitwiseOr32 || dw3_inst->Arg(1).IsImmediate() ||
        !dw3_inst->Arg(0).IsImmediate() || dw3_inst->Arg(0).U32() != 0xd0000000u) {
        return {dword3, dword4, false};
    }

    auto* dw3_and = dw3_inst->Arg(1).Inst();
    if (dw3_and->GetOpcode() != IR::Opcode::BitwiseAnd32 || !dw3_and->Arg(1).IsImmediate() ||
        dw3_and->Arg(1).U32() != 0xfffffffu) {
        return {dword3, dword4, false};
    }

    // Check pattern that translates cubemap depth to slices
    auto* dw4_inst = dword4.TryInst();
    if (!dw4_inst) {
        return {dword3, dword4, false};
    }

    if (dw4_inst->GetOpcode() != IR::Opcode::BitwiseOr32 || dw4_inst->Arg(0).IsImmediate() ||
        dw4_inst->Arg(1).IsImmediate()) {
        return {dword3, dword4, false};
    }

    auto* dw4_rhs = dw4_inst->Arg(1).Inst();
    if (dw4_rhs->GetOpcode() != IR::Opcode::BitwiseAnd32 || !dw4_rhs->Arg(1).IsImmediate() ||
        dw4_rhs->Arg(1).U32() != 0xffffe000u) {
        return {dword3, dword4, false};
    }

    return {dw3_and->Arg(0), dw4_rhs->Arg(0), true};
}

struct AnisoLod0Result {
    IR::Value ssharp_dw0;
    IR::Value tsharp_dw3;
    bool found;
};
AnisoLod0Result CheckDisableAnisoLod0Pattern(IR::Value value) {
    // Assuming S# is in s[12:15] and T# is in s[4:11]
    // The next pattern:
    //  s_bfe_u32     s0, s7,  $0x0008000c
    //  s_and_b32     s1, s12, $0xfffff1ff
    //  s_cmp_eq_u32  s0, 0
    //  s_cselect_b32 s0, s1, s12
    //  s_mov_b32     s12, s0
    // is used to disable anisotropy in the sampler if the sampled texture doesn't have mips

    auto* inst = value.TryInst();
    if (!inst) {
        return {value, {}, false};
    }

    if (inst->GetOpcode() != IR::Opcode::SelectU32) {
        return {value, {}, false};
    }

    // Select should be based on zero check
    const auto* prod0 = inst->Arg(0).Inst();
    if (prod0->GetOpcode() != IR::Opcode::IEqual32 ||
        !(prod0->Arg(1).IsImmediate() && prod0->Arg(1).U32() == 0u)) {
        return {value, {}, false};
    }

    auto* prod0_arg0 = prod0->Arg(0).Inst();
    ASSERT(prod0_arg0->GetOpcode() != IR::Opcode::Phi);

    // The bits range is for lods (note that constants are changed after constant propagation pass)
    if (prod0_arg0->GetOpcode() != IR::Opcode::BitFieldUExtract ||
        !(prod0_arg0->Arg(1).IsImmediate() && prod0_arg0->Arg(1).U32() == 12) ||
        !(prod0_arg0->Arg(2).IsImmediate() && prod0_arg0->Arg(2).U32() == 8)) {
        return {value, {}, false};
    }

    // Make sure mask is masking out anisotropy
    const auto* prod1 = inst->Arg(1).Inst();
    if (prod1->GetOpcode() != IR::Opcode::BitwiseAnd32 || prod1->Arg(1).U32() != 0xfffff1ff) {
        return {value, {}, false};
    }

    // We're working on the first dword of S#
    auto* prod2 = inst->Arg(2).Inst();
    ASSERT(prod2->GetOpcode() != IR::Opcode::Phi);
    return {inst->Arg(2), prod0_arg0->Arg(0), true};
}

struct SamplerPatchResult {
    IR::Value ssharp_dw0;
    bool found;
};
SamplerPatchResult CheckForceClampToWrapPattern(IR::Value value) {
    // s_and_b32 s12, 0xfffffe00, s12
    // is used to force clamp_x/y/z to wrap

    auto* inst = value.TryInst();
    if (!inst) {
        return {value, false};
    }

    if (inst->GetOpcode() != IR::Opcode::BitwiseAnd32 || !inst->Arg(0).IsImmediate() ||
        inst->Arg(0).U32() != 0xfffffe00u) {
        return {value, false};
    }

    return {inst->Arg(1), true};
}

SamplerPatchResult CheckForceClampToLastTexelPattern(IR::Value value) {
    // s_and_b32 s12, 0xfffffe00, s12
    // s_or_b32  0x12, s12
    // is used to force clamp_x/y to last texel

    auto* inst = value.TryInst();
    if (!inst) {
        return {value, false};
    }

    if (inst->GetOpcode() != IR::Opcode::BitwiseOr32 || inst->Arg(1).IsImmediate() ||
        !inst->Arg(0).IsImmediate() || inst->Arg(0).U32() != 0x12u) {
        return {value, false};
    }

    auto* prod = inst->Arg(1).Inst();
    if (prod->GetOpcode() != IR::Opcode::BitwiseAnd32 || !prod->Arg(0).IsImmediate() ||
        prod->Arg(0).U32() != 0xfffffe00u) {
        return {value, false};
    }

    return {prod->Arg(1), true};
}

SamplerPatchResult CheckClearAnisoRatioAndThresholdPattern(IR::Value value) {
    // s_and_b32 s12, s12, 0xfff8f1ff
    // is used to clear anisotropy ratio and threshold fields

    auto* inst = value.TryInst();
    if (!inst) {
        return {value, false};
    }

    if (inst->GetOpcode() != IR::Opcode::BitwiseAnd32 || !inst->Arg(1).IsImmediate() ||
        inst->Arg(1).U32() != 0xfff8f1ffu) {
        return {value, false};
    }

    return {inst->Arg(0), true};
}

IR::Inst* FindSharpSource(IR::Inst* handle) {
    ASSERT(IsSharpSource(handle));
    return handle;
}

void MarkReadConstBufferSharpSources(const SharpReference& sharp) {
    // In cases of bindless sharp fetches mark all producer instructions
    // so the extended userdata flattening pass will include them.
    for (size_t i = 0; i < sharp.num_dwords; i++) {
        IR::Inst* source = sharp.dwords[i].TryInst();
        if (!source) {
            continue;
        }
        ASSERT(IsSharpSource(source));
        if (source->GetOpcode() == IR::Opcode::ReadConstBuffer) {
            auto flags = source->Flags<IR::BufferInstInfo>();
            flags.sharp_source.Assign(1u);
            source->SetFlags(flags);
        }
    }
}

static void DiscoverSharpTable(const SharpReference& sharp, SharpTableReference& table);

void DiscoverBufferSharp(IR::Block& block, IR::Inst& inst, ResourceDiscoveryList& resources,
                         const Profile& profile) {
    IR::Inst* handle = inst.Arg(0).Inst();
    auto& resource = resources.emplace_back(&inst);
    auto& vsharp = resource.sharps[0];

    // Gather V# dwords
    vsharp.num_dwords = handle->NumArgs();
    for (size_t i = 0; i < handle->NumArgs(); ++i) {
        vsharp.dwords[i] = handle->Arg(i);
        if (auto* inst = vsharp.dwords[i].TryInst();
            inst && inst->GetOpcode() == IR::Opcode::ReadFirstLane) {
            vsharp.dwords[i] = inst->Arg(0);
        }
    }

    // Attempt to "see through" various V# access patterns and have binding reproduce them
    if (auto [dword0, found] = CheckInlineCbufPattern(vsharp.dwords[0]); found) {
        vsharp.post_op = SharpFetchPostOp::OffsetByProgramBase;
        vsharp.dwords[0] = dword0;
        vsharp.dwords[1] = IR::Value{0U};
    } else if (auto [dword1, mask, found] = CheckStridePatchPattern(handle->Arg(1)); found) {
        vsharp.post_op = SharpFetchPostOp::BitwiseOrDw1WithImm;
        vsharp.post_op_data.dw1_mask = mask;
        vsharp.dwords[1] = dword1;
    } else if (auto [dword1, dword2, shift, found] =
                   CheckClearStrideAndPatchRecordsPattern(handle->Arg(1), handle->Arg(2));
               found) {
        vsharp.post_op = SharpFetchPostOp::ClearStrideAndBitshiftDw2WithImm;
        vsharp.post_op_data.dw2_shift = shift;
        vsharp.dwords[1] = dword1;
        vsharp.dwords[2] = dword2;
    }

    auto& table = resource.tables[0];
    if (profile.support_bda && vsharp.post_op == SharpFetchPostOp::None) {
        switch (inst.GetOpcode()) {
        case IR::Opcode::LoadBufferU32:
        case IR::Opcode::LoadBufferU32x2:
        case IR::Opcode::LoadBufferU32x3:
        case IR::Opcode::LoadBufferU32x4:
        case IR::Opcode::LoadBufferF32:
        case IR::Opcode::LoadBufferF32x2:
        case IR::Opcode::LoadBufferF32x3:
        case IR::Opcode::LoadBufferF32x4:
        case IR::Opcode::StoreBufferU32:
        case IR::Opcode::StoreBufferU32x2:
        case IR::Opcode::StoreBufferU32x3:
        case IR::Opcode::StoreBufferU32x4:
        case IR::Opcode::StoreBufferF32:
        case IR::Opcode::StoreBufferF32x2:
        case IR::Opcode::StoreBufferF32x3:
        case IR::Opcode::StoreBufferF32x4:
            DiscoverSharpTable(vsharp, table);
            break;
        default:
            break;
        }
    }
    MarkReadConstBufferSharpSources(table.index.IsEmpty() ? vsharp : table.sharp);
}

struct TableOffset {
    IR::Value index{};
    u64 scale{1};
    u64 offset{};
};

static TableOffset DescribeTableOffset(const IR::Value& value) {
    constexpr u64 max_value = std::numeric_limits<u32>::max();
    if (value.IsImmediate()) {
        return {.scale = 0, .offset = value.U32()};
    }
    const IR::Inst* inst = value.Inst();
    const IR::Opcode opcode = inst->GetOpcode();
    if ((opcode == IR::Opcode::IAdd32 || opcode == IR::Opcode::IMul32) &&
        inst->Arg(0).IsImmediate() != inst->Arg(1).IsImmediate()) {
        const bool is_first_immediate = inst->Arg(0).IsImmediate();
        TableOffset result = DescribeTableOffset(inst->Arg(is_first_immediate ? 1 : 0));
        const u64 immediate = inst->Arg(is_first_immediate ? 0 : 1).U32();
        if (opcode == IR::Opcode::IAdd32) {
            result.offset += immediate;
        } else {
            result.scale *= immediate;
            result.offset *= immediate;
        }
        if (result.scale <= max_value && result.offset <= max_value) {
            return result;
        }
    } else if ((opcode == IR::Opcode::ShiftLeftLogical32 ||
                opcode == IR::Opcode::ShiftRightLogical32) &&
               inst->Arg(1).IsImmediate() && inst->Arg(1).U32() < 32) {
        const u32 shift = inst->Arg(1).U32();
        TableOffset result = DescribeTableOffset(inst->Arg(0));
        if (opcode == IR::Opcode::ShiftLeftLogical32) {
            result.scale <<= shift;
            result.offset <<= shift;
            if (result.scale <= max_value && result.offset <= max_value) {
                return result;
            }
        } else if (((result.scale | result.offset) & ((u64{1} << shift) - 1)) == 0) {
            result.scale >>= shift;
            result.offset >>= shift;
            return result;
        }
    }
    return {.index = value};
}

static bool HasRuntimeSource(const IR::Value& value) {
    boost::container::small_vector<const IR::Inst*, 16> visited;
    boost::container::small_vector<const IR::Inst*, 16> worklist;
    const auto visit = [&](const IR::Value& arg) {
        if (arg.IsImmediate()) {
            return;
        }
        const IR::Inst* inst = arg.Inst();
        if (std::ranges::find(visited, inst) == visited.end()) {
            visited.push_back(inst);
            worklist.push_back(inst);
        }
    };
    visit(value);
    while (!worklist.empty()) {
        const IR::Inst* inst = worklist.back();
        worklist.pop_back();
        switch (inst->GetOpcode()) {
        case IR::Opcode::GetUserData:
            break;
        case IR::Opcode::ReadConst:
        case IR::Opcode::ReadConstBuffer:
        case IR::Opcode::CompositeConstructU32x2:
        case IR::Opcode::CompositeConstructU32x4:
        case IR::Opcode::IAdd32:
        case IR::Opcode::ISub32:
        case IR::Opcode::IMul32:
        case IR::Opcode::ShiftLeftLogical32:
        case IR::Opcode::ShiftRightLogical32:
        case IR::Opcode::BitwiseAnd32:
        case IR::Opcode::BitwiseOr32:
        case IR::Opcode::BitwiseXor32:
        case IR::Opcode::BitwiseNot32:
        case IR::Opcode::UMin32:
        case IR::Opcode::UMax32:
        case IR::Opcode::BitFieldUExtract:
            for (size_t i = 0; i < inst->NumArgs(); ++i) {
                visit(inst->Arg(i));
            }
            break;
        default:
            return true;
        }
    }
    return false;
}

static void DiscoverSharpTable(const SharpReference& sharp, SharpTableReference& table) {
    const IR::Inst* first = sharp.dwords[0].TryInst();
    if (!first || first->GetOpcode() != IR::Opcode::ReadConstBuffer) {
        return;
    }
    const IR::Inst* handle = first->Arg(0).TryInst();
    if (!handle || handle->GetOpcode() != IR::Opcode::CompositeConstructU32x4) {
        return;
    }
    const TableOffset base = DescribeTableOffset(first->Arg(1));
    if (base.index.IsEmpty() || base.scale < sharp.num_dwords || !HasRuntimeSource(base.index)) {
        return;
    }
    for (u32 dw = 1; dw < sharp.num_dwords; ++dw) {
        const IR::Inst* read = sharp.dwords[dw].TryInst();
        if (!read || read->GetOpcode() != IR::Opcode::ReadConstBuffer) {
            return;
        }
        const IR::Inst* read_handle = read->Arg(0).TryInst();
        if (!read_handle || read_handle->GetOpcode() != IR::Opcode::CompositeConstructU32x4) {
            return;
        }
        for (size_t i = 0; i < handle->NumArgs(); ++i) {
            if (read_handle->Arg(i) != handle->Arg(i)) {
                return;
            }
        }
        const TableOffset offset = DescribeTableOffset(read->Arg(1));
        if (offset.index != base.index || offset.scale != base.scale ||
            offset.offset != base.offset + dw) {
            return;
        }
    }

    SharpReference vsharp{};
    vsharp.num_dwords = static_cast<u32>(handle->NumArgs());
    for (size_t i = 0; i < handle->NumArgs(); ++i) {
        vsharp.dwords[i] = handle->Arg(i);
        if (const IR::Inst* source = vsharp.dwords[i].TryInst();
            source && source->GetOpcode() == IR::Opcode::ReadFirstLane) {
            vsharp.dwords[i] = source->Arg(0);
        }
        if (const IR::Inst* source = vsharp.dwords[i].TryInst(); source && !IsSharpSource(source)) {
            return;
        }
    }
    table.sharp = vsharp;
    table.index = base.index;
    table.stride_dw = static_cast<u32>(base.scale);
    table.offset_dw = static_cast<u32>(base.offset);
}

void LowerRuntimeImageQuery(IR::Inst& inst, const SharpReference& sharp) {
    IR::IREmitter ir{*inst.GetParent(), IR::Block::InstructionList::s_iterator_to(inst)};
    const auto field = [&](u32 word, u32 offset, u32 bits) {
        return ir.BitFieldExtract(IR::U32{sharp.dwords[word]}, ir.Imm32(offset), ir.Imm32(bits));
    };
    const auto zero = ir.Imm32(0U);
    const auto one = ir.Imm32(1U);
    const auto type = field(3, 28, 4);
    const auto is_type = [&](AmdGpu::ImageType value) {
        return ir.IEqual(type, ir.Imm32(u32(value)));
    };
    const auto is_msaa = ir.LogicalOr(is_type(AmdGpu::ImageType::Color2DMsaa),
                                      is_type(AmdGpu::ImageType::Color2DMsaaArray));
    const auto is_1d =
        ir.LogicalOr(is_type(AmdGpu::ImageType::Color1D), is_type(AmdGpu::ImageType::Color1DArray));
    const auto is_volume = is_type(AmdGpu::ImageType::Color3D);
    const IR::U32 base = field(3, 12, 4);
    const IR::U32 level{ir.Select(is_msaa, zero, ir.IAdd(base, IR::U32{inst.Arg(1)}))};
    const auto minify = [&](IR::U32 value) {
        return ir.UMax(ir.ShiftRightLogical(ir.IAdd(value, one), ir.UMin(level, ir.Imm32(31U))),
                       one);
    };
    const auto width = minify(field(2, 0, 14));
    const auto height = minify(field(2, 14, 14));
    const auto depth = minify(field(4, 0, 13));
    const auto layers = ir.IAdd(ir.ISub(field(5, 13, 13), field(5, 0, 13)), one);
    const auto array = inst.Flags<IR::TextureInstInfo>().is_array;
    const auto array_type =
        ir.LogicalOr(ir.LogicalOr(is_type(AmdGpu::ImageType::Color1DArray),
                                  is_type(AmdGpu::ImageType::Color2DArray)),
                     ir.LogicalOr(is_type(AmdGpu::ImageType::Cube),
                                  is_type(AmdGpu::ImageType::Color2DMsaaArray)));
    const IR::U32 slices = array ? IR::U32{ir.Select(array_type, layers, zero)} : zero;
    const IR::U32 y{ir.Select(is_1d, slices, height)};
    const IR::U32 z{ir.Select(is_volume, depth, ir.Select(is_1d, zero, slices))};
    const IR::U32 mips =
        inst.Arg(2).U1()
            ? IR::U32{ir.Select(is_msaa, one, ir.IAdd(ir.ISub(field(3, 16, 4), base), one))}
            : zero;
    const auto valid = ir.INotEqual(IR::U32{sharp.dwords[1]}, zero);
    inst.ReplaceUsesWith(ir.CompositeConstruct(ir.Select(valid, width, zero),
                                               ir.Select(valid, y, zero), ir.Select(valid, z, zero),
                                               ir.Select(valid, mips, zero)));
}

void DiscoverImageSharp(IR::Block& block, IR::Inst& inst, ResourceDiscoveryList& resources,
                        const Profile& profile) {
    IR::Inst* image_handle = inst.Arg(0).Inst();
    ASSERT(image_handle->GetOpcode() == IR::Opcode::ImageHandle);
    auto& resource = resources.emplace_back(&inst);
    auto& tsharp = resource.sharps[0];

    // Gather T# dwords
    IR::Inst* tsharp_low = image_handle->Arg(0).Inst();
    for (size_t i = 0; i < tsharp_low->NumArgs(); ++i) {
        tsharp.dwords[tsharp.num_dwords++] = tsharp_low->Arg(i);
    }
    if (auto tsharp_high = image_handle->Arg(1).TryInst()) {
        for (size_t i = 0; i < tsharp_high->NumArgs(); ++i) {
            tsharp.dwords[tsharp.num_dwords++] = tsharp_high->Arg(i);
        }
    }

    if (auto [tsharp_dw3, tsharp_dw4, found] =
            CheckCubeTo2DArrayPattern(tsharp.dwords[3], tsharp.dwords[4]);
        found) {
        tsharp.post_op = SharpFetchPostOp::ConvertCubeTo2DArray;
        tsharp.dwords[3] = tsharp_dw3;
        tsharp.dwords[4] = tsharp_dw4;
    }

    auto& image_table = resource.tables[0];
    const auto inst_info = inst.Flags<IR::TextureInstInfo>();
    if (inst.GetOpcode() == IR::Opcode::ImageQueryDimensions && tsharp.num_dwords == 8 &&
        tsharp.post_op == SharpFetchPostOp::None) {
        DiscoverSharpTable(tsharp, image_table);
        if (!image_table.index.IsEmpty()) {
            LowerRuntimeImageQuery(inst, tsharp);
            resources.pop_back();
            return;
        }
    }
    if (profile.max_image_table_size != 0 && inst.GetOpcode() == IR::Opcode::ImageSampleRaw &&
        tsharp.post_op == SharpFetchPostOp::None && !inst_info.is_r128) {
        DiscoverSharpTable(tsharp, image_table);
    }
    MarkReadConstBufferSharpSources(image_table.index.IsEmpty() ? tsharp : image_table.sharp);

    if (inst.GetOpcode() != IR::Opcode::ImageSampleRaw) {
        return;
    }

    // Gather S# dwords
    const IR::Inst* sampler = inst.Arg(1).Inst();
    auto& ssharp = resource.sharps[1];
    for (size_t i = 0; i < sampler->NumArgs(); ++i) {
        ssharp.dwords[ssharp.num_dwords++] = sampler->Arg(i);
    }
    if (auto [ssharp_dw0, tsharp_dw3, found] = CheckDisableAnisoLod0Pattern(ssharp.dwords[0]);
        found) {
        ssharp.post_op = SharpFetchPostOp::DisableAnisoIfSingleLod;
        ssharp.post_op_data.lod_prod = tsharp_dw3;
        ssharp.dwords[0] = ssharp_dw0;
    } else if (auto [ssharp_dw0, found] = CheckForceClampToWrapPattern(ssharp.dwords[0]); found) {
        ssharp.post_op = SharpFetchPostOp::ForceRepeatXyzClamp;
        ssharp.dwords[0] = ssharp_dw0;
    } else if (auto [ssharp_dw0, found] = CheckForceClampToLastTexelPattern(ssharp.dwords[0]);
               found) {
        ssharp.post_op = SharpFetchPostOp::ForceLastTexelXyClamp;
        ssharp.dwords[0] = ssharp_dw0;
    } else if (auto [ssharp_dw0, found] = CheckClearAnisoRatioAndThresholdPattern(ssharp.dwords[0]);
               found) {
        ssharp.post_op = SharpFetchPostOp::ClearAnisoRatioAndThreshold;
        ssharp.dwords[0] = ssharp_dw0;
    }

    auto& sampler_table = resource.tables[1];
    if (profile.supports_sampled_image_array_non_uniform_indexing &&
        ssharp.post_op != SharpFetchPostOp::DisableAnisoIfSingleLod) {
        DiscoverSharpTable(ssharp, sampler_table);
    }
    MarkReadConstBufferSharpSources(sampler_table.index.IsEmpty() ? ssharp : sampler_table.sharp);
}

ResourceDiscoveryList ResourceDiscoverPass(IR::Program& program, const Profile& profile) {
    ResourceDiscoveryList sharp_usages;

    for (IR::Block* const block : program.blocks) {
        for (IR::Inst& inst : block->Instructions()) {
            if (IsBufferInstruction(inst)) {
                DiscoverBufferSharp(*block, inst, sharp_usages, profile);
            } else if (IsImageInstruction(inst)) {
                DiscoverImageSharp(*block, inst, sharp_usages, profile);
            } else if (IsDataRingInstruction(inst)) {
                sharp_usages.emplace_back(&inst);
            }
        }
    }
    return sharp_usages;
}

} // namespace Shader::Optimization