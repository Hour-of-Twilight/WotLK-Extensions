// Bone budget: partition a submesh whose per-draw bone palette exceeds the client ceiling into
// sub-sections, and re-point batches across the resulting sub-section run.
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

#include <Defines.h>
#include "Models/Compat/BoneBudget.h"

#include "Models/Common/ModelHooks.h"

#include "Models/Common/Text.h"

#include <algorithm>
#include <cstring>
#include <unordered_map>

namespace ModernM2::Bones
{
    namespace fmt  = ModernM2::Format;
    namespace text = ModernM2::Text;
    using Skin     = ModernM2::Client::M2SkinProfile;

    namespace
    {
        constexpr uint32 kSkinU16Max         = 0xFFFF;  // u16 ceiling of the skin arrays
        constexpr uint32 kSplitMaxBoneCombos = 0x10000; // boneCombos upper bound, rejected before bulk copy
    }

    bool UsesExtendedIndexStart(std::string_view name)
    {
        return text::StartsWithCI(name, "character\\") ||
               text::StartsWithCI(name, "characterhd\\") ||
               text::StartsWithCI(name, "item\\objectcomponents\\");
    }

    uint32 FullIndexStart(const fmt::M2SkinSection& section, bool extended)
    {
        return extended ? ((static_cast<uint32>(section.level) << 16) | section.indexStart)
                        : static_cast<uint32>(section.indexStart);
    }

    /**
     * @brief Partitions over-ceiling submeshes into sub-sections, rebuilding the live skin geometry and
     *        header.boneCombos.
     * @param md          Parsed model header (boneCombos array is a raw pointer here).
     * @param skin        Live skin profile whose geometry arrays are rebuilt on success.
     * @param outSections Receives the rebuilt sub-sections.
     * @param splitMap    Receives the per-original-submesh sub-section run, indexed by original submesh.
     * @param splitCount  Receives the count of extra sub-draws produced.
     * @param name        Model path, used for logging.
     * @return true on commit; false (no commit) on any overflow, allocation failure, or missing array.
     */
    uint32 ClampBoneBudget(const fmt::M2Header* md, Skin* skin)
    {
        if (!md || !skin || !skin->submeshes)
            return 0;

        uint32 clamped = 0;
        const uint32 count = std::min<uint32>(skin->submeshCount, kMaxBatches);
        for (uint32 i = 0; i < count; ++i)
        {
            fmt::M2SkinSection& s = skin->submeshes[i];
            uint16 cap = kMaxBonesPerDraw;
            const uint16 byCombo = md->boneCombos.count > s.boneComboIndex
                ? static_cast<uint16>(md->boneCombos.count - s.boneComboIndex)
                : 1;
            if (byCombo < cap) cap = byCombo;
            if (cap < 1)       cap = 1;
            if (s.boneCount > cap) { s.boneCount = cap; ++clamped; }
            if (s.boneCount < 1)   s.boneCount = 1;
        }
        return clamped;
    }

    bool SplitSubmeshes(fmt::M2Header* md, Skin* skin, std::vector<SplitSection>& outSections, std::vector<SplitRun>& splitMap, uint32& splitCount, const char* name)
    {
        if (!md->boneCombos.count || !md->boneCombos.offset) return false;
        if (!skin->vertexLookup || !skin->indices || !skin->bones) return false;

        uint32 boneComboCount = md->boneCombos.count;
        auto* boneCombos = reinterpret_cast<uint16*>(static_cast<uintptr_t>(md->boneCombos.offset));
        if (boneComboCount > kSplitMaxBoneCombos || !boneCombos)
        {
            WLOG_WARN("modern-assets: '%s' boneCombos count=%u out of range, skipping bone split", name, boneComboCount);
            return false;
        }

        bool needsSplit = false;
        const bool extendedIndexStart = UsesExtendedIndexStart(name);
        for (uint32 si = 0; si < skin->submeshCount; ++si)
        {
            const fmt::M2SkinSection& s = skin->submeshes[si];
            if ((extendedIndexStart || s.level == 0) && s.boneCount > kMaxBonesPerDraw)
                { needsSplit = true; break; }
        }
        if (!needsSplit) return false;

        std::vector<uint16> newVtxLookup;
        std::vector<uint8>  newBones;
        std::vector<uint16> newIndices;
        std::vector<uint16> newBoneCombos(boneCombos, boneCombos + boneComboCount);
        newVtxLookup.reserve(skin->vertexCount);
        newBones.reserve(skin->vertexCount * 4);
        newIndices.reserve(skin->indexCount);

        splitCount = 0;

        for (uint32 si = 0; si < skin->submeshCount; ++si)
        {
            fmt::M2SkinSection s = skin->submeshes[si];

            // A level>0 submesh is a sub-batch the engine cannot draw. Pass it through as a single zeroed
            // placeholder so the batch re-point stays 1:1; its batch is later skipped.
            if (s.level > 0 && !extendedIndexStart)
            {
                s.level = 0; s.vertexStart = 0; s.vertexCount = 0; s.indexStart = 0;
                s.indexCount = 0; s.boneComboIndex = 0; s.centerBoneIndex = 0; s.boneCount = 1;
                reinterpret_cast<uint8*>(&s)[0x11] = 0;
                outSections.push_back({ s, static_cast<uint16>(si) });
                continue;
            }

            const uint32 sourceIndexStart = FullIndexStart(s, extendedIndexStart);
            if (sourceIndexStart > skin->indexCount || s.indexCount > skin->indexCount - sourceIndexStart)
            {
                WLOG_WARN("modern-assets: '%s' submesh %u index window past skin indexCount, skipping bone split", name, si);
                return false;
            }

            uint32 triCount = s.indexCount / 3;
            uint32 comboBase = s.boneComboIndex;

            std::vector<uint16> curGlobals;
            uint32 curTriStart = 0;
            uint32 emittedSections = 0;

            // Emit triangles [triFrom, triTo) as one sub-section: a sorted boneCombos slice, a deduplicated
            // vertex block with bones[] remapped to the slice, and a global-indexed triangle block.
            auto emit = [&](uint32 triFrom, uint32 triTo, std::vector<uint16>& globals) -> bool
            {
                if (triFrom >= triTo) return true;
                std::sort(globals.begin(), globals.end());
                uint32 comboIndex = static_cast<uint32>(newBoneCombos.size());
                if (comboIndex > kSkinU16Max) return false;
                for (uint16 g : globals) newBoneCombos.push_back(g);

                uint32 secVertStart  = static_cast<uint32>(newVtxLookup.size());
                uint32 secIndexStart = static_cast<uint32>(newIndices.size());
                if (secVertStart > kSkinU16Max) return false;
                if (!extendedIndexStart && secIndexStart > kSkinU16Max) return false;
                if (extendedIndexStart && (secIndexStart >> 16) > kSkinU16Max) return false;

                std::unordered_map<uint16, uint16> vmap;
                for (uint32 t = triFrom; t < triTo; ++t)
                {
                    for (uint32 k = 0; k < 3; ++k)
                    {
                        uint16 lv = skin->indices[sourceIndexStart + t * 3 + k];
                        if (lv >= skin->vertexCount) return false;
                        auto it = vmap.find(lv);
                        uint16 nv;
                        if (it == vmap.end())
                        {
                            uint32 idx = static_cast<uint32>(newVtxLookup.size());
                            if (idx > kSkinU16Max) return false;
                            nv = static_cast<uint16>(idx);
                            vmap.emplace(lv, nv);
                            newVtxLookup.push_back(skin->vertexLookup[lv]);
                            const uint8* infl = skin->bones + lv * 4;
                            for (uint32 j = 0; j < 4; ++j)
                            {
                                uint32 comboIdx = comboBase + infl[j];
                                uint16 g = comboIdx < boneComboCount ? boneCombos[comboIdx] : globals[0];
                                auto lo = std::lower_bound(globals.begin(), globals.end(), g);
                                uint16 local = (lo != globals.end() && *lo == g)
                                               ? static_cast<uint16>(lo - globals.begin()) : 0;
                                newBones.push_back(static_cast<uint8>(local));
                            }
                        }
                        else nv = it->second;
                        newIndices.push_back(nv);
                    }
                }

                uint32 secVertCount  = static_cast<uint32>(newVtxLookup.size()) - secVertStart;
                uint32 secIndexCount = static_cast<uint32>(newIndices.size()) - secIndexStart;
                if (secVertCount > kSkinU16Max || secIndexCount > kSkinU16Max) return false;

                fmt::M2SkinSection sec = s;
                sec.vertexStart    = static_cast<uint16>(secVertStart);
                sec.vertexCount    = static_cast<uint16>(secVertCount);
                sec.level          = extendedIndexStart ? static_cast<uint16>(secIndexStart >> 16) : 0;
                sec.indexStart     = static_cast<uint16>(secIndexStart & kSkinU16Max);
                sec.indexCount     = static_cast<uint16>(secIndexCount);
                sec.boneCount      = static_cast<uint16>(globals.size());
                sec.boneComboIndex = static_cast<uint16>(comboIndex);
                outSections.push_back({ sec, static_cast<uint16>(si) });
                ++emittedSections;
                return true;
            };

            for (uint32 t = 0; t < triCount; ++t)
            {
                uint16 g[12]; int gn = 0;
                for (uint32 k = 0; k < 3; ++k)
                {
                    uint16 lv = skin->indices[sourceIndexStart + t * 3 + k];
                    if (lv >= skin->vertexCount) return false;
                    const uint8* infl = skin->bones + lv * 4;
                    for (uint32 j = 0; j < 4; ++j)
                    {
                        uint32 comboIdx = comboBase + infl[j];
                        uint16 gg = comboIdx < boneComboCount ? boneCombos[comboIdx] : 0;
                        bool seen = false;
                        for (int e = 0; e < gn; ++e) if (g[e] == gg) { seen = true; break; }
                        if (!seen && gn < 12) g[gn++] = gg;
                    }
                }
                size_t unionSize = curGlobals.size();
                for (int e = 0; e < gn; ++e)
                    if (std::find(curGlobals.begin(), curGlobals.end(), g[e]) == curGlobals.end())
                        ++unionSize;

                if (unionSize > kMaxBonesPerDraw && t > curTriStart)
                {
                    if (!emit(curTriStart, t, curGlobals)) return false;
                    curGlobals.clear();
                    curTriStart = t;
                }
                for (int e = 0; e < gn; ++e)
                    if (std::find(curGlobals.begin(), curGlobals.end(), g[e]) == curGlobals.end())
                        curGlobals.push_back(g[e]);
            }
            if (!emit(curTriStart, triCount, curGlobals)) return false;
            if (emittedSections == 0)
            {
                fmt::M2SkinSection sec = s;
                sec.vertexCount = 0; sec.indexCount = 0; sec.boneCount = 1;
                outSections.push_back({ sec, static_cast<uint16>(si) });
            }
            else if (emittedSections > 1)
            {
                splitCount += emittedSections - 1;
            }
        }

        if (newVtxLookup.size() > kSkinU16Max || outSections.size() > kMaxBatches) return false;

        splitMap.assign(skin->submeshCount, SplitRun{ 0, 0 });
        for (uint16 i = 0; i < outSections.size(); ++i)
        {
            uint16 orig = outSections[i].origSubmesh;
            if (orig >= splitMap.size()) continue;
            if (splitMap[orig].count == 0) splitMap[orig].first = i;
            ++splitMap[orig].count;
        }

        // Commit the rebuilt geometry into owned buffers (leaked for the model's lifetime; the engine never
        // per-array frees the file-mapped skin arrays).
        auto* vl = static_cast<uint16*>(std::malloc(newVtxLookup.size() * sizeof(uint16)));
        auto* bn = static_cast<uint8*>(std::malloc(newBones.size()));
        auto* ix = static_cast<uint16*>(std::malloc(newIndices.size() * sizeof(uint16)));
        auto* bc = static_cast<uint16*>(std::malloc(newBoneCombos.size() * sizeof(uint16)));
        auto* sm = static_cast<fmt::M2SkinSection*>(std::malloc(outSections.size() * sizeof(fmt::M2SkinSection)));
        if (!vl || !bn || !ix || !bc || !sm)
        {
            std::free(vl); std::free(bn); std::free(ix); std::free(bc); std::free(sm);
            return false;
        }
        std::memcpy(vl, newVtxLookup.data(), newVtxLookup.size() * sizeof(uint16));
        std::memcpy(bn, newBones.data(), newBones.size());
        std::memcpy(ix, newIndices.data(), newIndices.size() * sizeof(uint16));
        std::memcpy(bc, newBoneCombos.data(), newBoneCombos.size() * sizeof(uint16));
        for (size_t i = 0; i < outSections.size(); ++i) sm[i] = outSections[i].section;

        skin->vertexLookup = vl;
        skin->vertexCount  = static_cast<uint32>(newVtxLookup.size());
        skin->bones        = bn;
        skin->boneCount    = static_cast<uint32>(newVtxLookup.size());
        skin->indices      = ix;
        skin->indexCount   = static_cast<uint32>(newIndices.size());
        skin->submeshes    = sm;
        skin->submeshCount = static_cast<uint32>(outSections.size());

        md->boneCombos.count  = static_cast<uint32>(newBoneCombos.size());
        md->boneCombos.offset = static_cast<uint32>(reinterpret_cast<uintptr_t>(bc));
        return true;
    }

    /**
     * @brief Re-points a skin's existing batches across the sub-section run their original submesh
     *        became, duplicating a batch per extra sub-section without touching any other field.
     * @param skin      Live skin profile whose batches are rebuilt in place.
     * @param splitMap  Per-original-submesh sub-section run, as produced by SplitSubmeshes.
     */
    void RepointBatchesAfterSplit(Skin* skin, const std::vector<SplitRun>& splitMap)
    {
        if (splitMap.empty() || !skin->batches || skin->batchCount == 0) return;

        std::vector<fmt::M2Batch> out;
        out.reserve(skin->batchCount);
        for (uint32 i = 0; i < skin->batchCount; ++i)
        {
            const fmt::M2Batch& b = skin->batches[i];
            SplitRun run{ b.skinSectionIndex, 1 };
            if (b.skinSectionIndex < splitMap.size()) run = splitMap[b.skinSectionIndex];
            for (uint16 s = 0; s < run.count; ++s)
            {
                fmt::M2Batch nb = b;
                nb.skinSectionIndex = static_cast<uint16>(run.first + s);
                out.push_back(nb);
            }
        }
        if (out.empty() || out.size() > kMaxBatches) return;

        // Leaked for the model's lifetime, same pattern as SplitSubmeshes' committed arrays above.
        auto* buf = static_cast<fmt::M2Batch*>(std::malloc(out.size() * sizeof(fmt::M2Batch)));
        if (!buf) return;
        std::memcpy(buf, out.data(), out.size() * sizeof(fmt::M2Batch));
        skin->batches    = buf;
        skin->batchCount = static_cast<uint32>(out.size());
    }
}
