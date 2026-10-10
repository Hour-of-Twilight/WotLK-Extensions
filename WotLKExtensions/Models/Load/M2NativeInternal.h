// Native modern-M2 reader: internal contract shared across the reader's translation units.
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

#pragma once

#include <Defines.h>

// The reader is split by the load phases named in NativeLoad.cpp's MECHANISM banner -- demux (M2Demux),
// in-place field deltas + post-fixup injections (M2Fixups), the per-era record normalization
// (M2Normalize + the per-record rewrites, driven by the table in M2NormalizeLayout), then the
// offset->pointer walk that resolves the body itself (M2WalkOwned, driven by the record map in
// M2WalkLayout) -- all orchestrated by NativeLoad.cpp. The POD records the phases hand across (Scan from
// the demux, Outcome back to the caller) and the shared byte helpers live here; there is no shared
// MUTABLE state (the session counters are private to the orchestrator, fed from Outcome).

#include "Models/Load/M2NormalizeLayout.h"
#include "Models/Format/M2Format.h"

#include <cstdint>
#include <cstring>

namespace ModernM2::Reader::Detail
{
    inline uint32 Rd32(const void* p) { uint32 v; std::memcpy(&v, p, 4); return v; }
    inline uint16 Rd16(const void* p) { uint16 v; std::memcpy(&v, p, 2); return v; }
    inline void     Wr32(void* p, uint32 v) { std::memcpy(p, &v, 4); }
    inline void     Wr16(void* p, uint16 v) { std::memcpy(p, &v, 2); }

    /// True when count records of stride bytes starting at offset all lie inside a body of size bytes.
    /// The subtraction form is deliberate: count * stride is computed in 64 bits so a hostile count
    /// cannot wrap past the bound.
    inline bool FitsInBody(uint32 size, uint32 offset, uint32 count, uint32 stride)
    {
        return offset <= size &&
               static_cast<uint64>(count) * stride <= static_cast<uint64>(size) - offset;
    }

    // skipMask bits reported in the OnM2NativeLoad event / logs.
    enum : uint32
    {
        kSkipTxac      = 0x01,
        kSkipLdv1      = 0x02,
        kSkipAfid      = 0x04,
        kSkipSkid      = 0x08,
        kSkipPhysBone  = 0x10, // PFID / BFID (no home in the client's own format, permanently dropped)
        kSkipOther     = 0x80, // any other auxiliary chunk (EXP2, PFDC, ...)
    };

    constexpr uint32 kMaxTxid = 128; // observed max is 7 textures; hard cap for the POD copy

    /// Everything harvested from the container walk, POD so it lives inside the SEH frame.
    struct Scan
    {
        uint32 bodyOff;             // MD20 body offset within the container
        uint32 bodySize;
        uint32 txid[kMaxTxid];
        uint32 txidCount;
        uint32 sfidFirst;
        uint32 sfidCount;
        uint32 skipMask;
    };

    /// POD outcome of the guarded core, consumed by NativeLoad for stats/event/logging.
    struct Outcome
    {
        int      ok;
        uint32 version;
        uint32 texResolved;
        uint32 texUnresolved;
        uint32 skipMask;
        uint32 extSeqPending;
        uint32 shadowGateForced; // 1 when the shared runtime's animate-gate count had to be lifted off zero
        uint32 shadowGateAfter;  // that same animate-gate count, read back immediately after the write
        uint32 combinerSlotArray; // header offset of the array that covered the undeclared combiner slot, 0 when none did
        uint32 combinerSlotBytes; // byte size of the owned copy that array was moved to
        NormalizeReport normalized; // records rewritten to the target shape, per kRecordNormalizers entry
        const char* fail; // static failure reason when ok == 0
    };

    // ---------------------------------------------------------------- phase entry points
    /// M2Demux: walks the MD21 container, harvesting the body location and the auxiliary chunks. Returns
    /// true when an MD20 body large enough for a full header was found.
    bool ScanContainer(const uint8* buf, uint32 size, Scan& s);

    /// M2Fixups: sequence deltas in place (split u16 blendTime mask, source-id remap + lookup patch),
    /// counting the .anim-streamed sequences into extSeqPending.
    void FixSequencesRaw(uint8* base, uint32 size, ModernM2::Format::M2Header* h, uint32& extSeqPending);
    /// M2Fixups: material deltas in place (blend-mode clamp into the client's 7-entry table, flag mask).
    void FixMaterialsRaw(uint8* base, uint32 size, ModernM2::Format::M2Header* h);
    /// M2Fixups: points each hardcoded texture with no inline name at its TXID-resolved client path.
    void InjectTxidNames(ModernM2::Format::M2Header* h, const Scan& s, Outcome& out);
    /// M2Fixups: clamps each ribbon's texture/material reference values into the header tables.
    void ClampRibbonRefs(ModernM2::Format::M2Header* h);
    /// M2Fixups: drops each camera reference that points past the camera array (the source exporter
    /// emits the unreferenced sentinel for those, and consumers already read it as "no camera").
    void ClampCameraRefs(ModernM2::Format::M2Header* h);

    /// M2Normalize: rewrites every array listed in kRecordNormalizers whose records are wider in this
    /// model's source era down to the target record shape, in place. Returns false when a record (or an
    /// array nested in one) does not fit the body, which must fail the load.
    bool NormalizeRecords(uint8* base, uint32 size, ModernM2::Format::M2Header* h,
                          NormalizeReport& report);

    /// M2WalkOwned: resolves every header array (and the arrays nested inside the records they point
    /// at) from body-relative offsets into real pointers. Records are already target-shaped by then, so
    /// one record map serves every source era. Returns true when every array passed its bounds check.
    bool WalkHeaderArrays(uint8* base, uint32 size, ModernM2::Format::M2Header* h);

    /// M2WalkOwned: a header without the texture combiner flag is 0x130 bytes and the body data starts
    /// right behind it, yet the skin finalize claims the combiner slot at 0x130 when it synthesizes a
    /// combo table. Any header array whose bytes overlap that slot is moved to an owned copy first so
    /// the claim cannot overwrite model data (retail puts the vertex array there on skeleton file
    /// models and the global sequence table on the rest). Returns false only when the copy fails.
    bool FreeCombinerSlot(uint8* base, uint32 size, ModernM2::Format::M2Header* h, Outcome& out);

    // Skin profiles still arrive through the stock sibling loader and are reshaped later, on the live
    // parsed profile. Owning that parse plugs in here as its own record map plus normalizer table over
    // the profile body -- the same two tables this reader already runs over the model body.
}
