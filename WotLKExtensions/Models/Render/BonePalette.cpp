// M2 bone compatibility: post-fill bone-palette event, and the shadow-batch and main-draw doodad-batch
// detours the client carries exactly one owner of each for.
// Copyright (C) 2026 WarcraftXL
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program. If not, see <https://www.gnu.org/licenses/>.

#include "Models/Common/ModelHooks.h"
#include "Models/Client/GxBindings.h"
#include "Models/Render/ShadowSpace.h"
#include "Models/Compat/BoneBudget.h"

#include "Models/Format/M2Format.h"

#include "Models/Offsets/GxOffsets.h"
#include "Models/Offsets/M2Offsets.h"

#include <windows.h>

#include <algorithm>
#include <cstdint>

namespace
{
    namespace m2    = ModernM2::Offsets::M2;
    namespace gxoff = ModernM2::Offsets::Gx;
    namespace bones = ModernM2::Bones;

    m2::M2_RenderBatchShadowMapFn g_origRenderBatchShadowMap = nullptr;

    using DrawBatchDoodadFn = void (__fastcall*)(void* ctx, void* edx, void* elements, void* indices);
    DrawBatchDoodadFn g_origDrawBatchDoodad = nullptr;

    /**
     * @brief Detours the M2 ground-shadow batch draw, splitting an over-budget co-instance run into
     *        several native calls instead of drawing it as one.
     *
     * This detour is the ONLY one the client's real M2 ground-shadow draw carries, so the shadow
     * bone probe rides it from here too.
     *
     * The native function's own bone-copy loop (c31-based, 3 registers/bone) is unbounded across the
     * whole co-instance run -- boneCount * coInstanceCount can exceed the 75-bone VS-constant budget
     * even when boneCount alone is small, overflowing past c255 into the device's own vertex-stream
     * slot cache. That overflow is a confirmed, disasm-verified crash: it corrupts a slot record's
     * "count" dword with a bone-matrix float, which FUN_006844c0 later reads as an array index and
     * faults on a wild address (see corpus/re_comprehension/335/m2_instance_0x184_gx_cache.md §14 for
     * the original trace, and the register-level confirmation recorded in this session's own crash
     * triage). Mirrors DrawBatchDoodad's fix exactly, using the run-list shape
     * kShadowRunStride/kShadowRunCountField document: shrink the run's requested-count field to a
     * bone-budget-safe value per sub-call, advance drawIndex by however many co-instances were
     * actually drawn, and restore the field to the original total before returning -- the caller
     * (RenderModelBatchListShadowMap) reads that same field a second time, right after this call
     * returns, to advance its own run cursor.
     */
    void __fastcall hkRenderBatchShadowMap(
        void* instance, void*, uint32_t batchMode, void* skinBatch, void* drawList,
        uint32_t drawIndex, void* skinSection, void* previousSection)
    {
        if constexpr (ModernM2::kEnabled)
            ModernM2::Shadow::OnShadowBatch(instance, skinSection);

        uint32_t* runs          = nullptr;
        uint32_t  originalCount = 0;
        uint32_t  chunkSize     = 0;
        void*     shared        = nullptr;
        __try
        {
            if (drawList && skinSection)
            {
                auto* listData = *reinterpret_cast<uint32_t* const*>(drawList);
                if (listData)
                {
                    runs = listData;
                    originalCount = runs[drawIndex * m2::kShadowRunStride + m2::kShadowRunCountField];

                    const uint32_t boneCount =
                        static_cast<const ModernM2::Format::M2SkinSection*>(skinSection)->boneCount;
                    chunkSize = boneCount > 0
                        ? std::max<uint32_t>(1u, bones::kMaxBonesPerDraw / boneCount)
                        : originalCount;
                    shared = *reinterpret_cast<void* const*>(
                        static_cast<uint8_t*>(instance) + m2::kOffInstShared);
                }
            }
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            runs = nullptr; // fail safe: single native call below, run list left untouched
        }

        if (!runs || chunkSize >= originalCount)
        {
            g_origRenderBatchShadowMap(instance, nullptr, batchMode, skinBatch, drawList,
                                       drawIndex, skinSection, previousSection);
            return;
        }

        if (chunkSize == 1 && shared)
        {
            ModernM2::Native<m2::M2_SharedSetIndicesFn>(m2::kSharedSetIndices)(shared, nullptr);
            ModernM2::Native<m2::M2_SharedSetVerticesFn>(m2::kSharedSetVertices)(shared, nullptr, 0);
        }

        uint32_t drawn = 0;
        while (drawn < originalCount)
        {
            const uint32_t thisChunk = std::min(chunkSize, originalCount - drawn);
            const size_t   slot      = static_cast<size_t>(drawIndex + drawn) * m2::kShadowRunStride +
                                       m2::kShadowRunCountField;
            const uint32_t saved     = runs[slot];
            runs[slot] = thisChunk;

            g_origRenderBatchShadowMap(instance, nullptr, batchMode, skinBatch, drawList,
                                       drawIndex + drawn, skinSection, nullptr);
            runs[slot] = saved;
            drawn += thisChunk;
        }
    }

    /**
     * @brief Detours the main-draw batched-doodad path, splitting an over-budget co-instance batch into
     *        several native calls instead of drawing it as one.
     *
     * The native function already loops internally over groups of AllocInstances' granted capacity, but
     * that capacity is sized for GPU buffer space, not for the c31-based VS-constant budget -- a group
     * can still ask for more than kMaxBonesPerDraw total bones across its co-instances. The fix mirrors
     * that same internal loop shape from the outside: shrink the batch record's run-length field
     * (kM2ElementRunLengthField) to a bone-budget-safe count per call, advance the indices pointer by
     * what was actually drawn, and restore the field to its original value before returning -- the
     * caller (CM2SceneRender::Draw) reads that same field a second time, right after this call returns,
     * to advance its own sorted-index cursor past the whole run.
     */
    void __fastcall hkDrawBatchDoodad(void* ctx, void* edx, void* elements, void* indices)
    {
        uint32_t* countField    = nullptr;
        uint32_t  originalCount = 0;
        uint32_t  chunkSize     = 0;
        __try
        {
            auto* c = static_cast<gxoff::DrawBatchContext*>(ctx);
            if (c->element)
            {
                auto* elementBytes = static_cast<uint8_t*>(c->element);
                countField    = reinterpret_cast<uint32_t*>(elementBytes + gxoff::kM2ElementRunLengthField);
                originalCount = *countField;

                const void* section =
                    *reinterpret_cast<void* const*>(elementBytes + gxoff::kM2ElementSectionField);
                const uint32_t boneCount = section
                    ? static_cast<const ModernM2::Format::M2SkinSection*>(section)->boneCount
                    : 0;

                chunkSize = boneCount > 0
                    ? std::max<uint32_t>(1u, bones::kMaxBonesPerDraw / boneCount)
                    : originalCount;
            }
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            countField = nullptr; // fail safe: single native call below, batch record left untouched
        }

        if (!countField || chunkSize >= originalCount)
        {
            g_origDrawBatchDoodad(ctx, edx, elements, indices);
            return;
        }

        auto*    indexBytes = static_cast<uint8_t*>(indices);
        uint32_t drawn       = 0;
        while (drawn < originalCount)
        {
            const uint32_t thisChunk = std::min(chunkSize, originalCount - drawn);
            *countField = thisChunk;
            g_origDrawBatchDoodad(ctx, edx, elements, indexBytes + static_cast<size_t>(drawn) * 4);
            drawn += thisChunk;
        }
        *countField = originalCount;
    }
}

namespace ModernM2
{
    bool InstallM2CompatBones()
    {
        const bool shadowHooked = HookAttach("M2.RenderBatchShadowMap", m2::kRenderBatchShadowMap,
                                            &hkRenderBatchShadowMap, &g_origRenderBatchShadowMap);
        HookAttach("M2.DrawBatchDoodad", gxoff::kDrawBatchDoodad, &hkDrawBatchDoodad, &g_origDrawBatchDoodad);
        if constexpr (kEnabled)
            ModernM2::Shadow::Arm(shadowHooked);
        return true;
    }
}
