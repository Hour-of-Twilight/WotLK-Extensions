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
#include "Models/Db2/Db2Decode.h"

#include <cstring>
#include <unordered_map>

namespace
{
    constexpr uint32 kMagicWDC1 = 0x31434457; // 'WDC1'
    constexpr uint32 kMagicWDC2 = 0x32434457; // 'WDC2'
    constexpr uint32 kMagic1SLC = 0x434C5331; // '1SLC'
    constexpr uint32 kMagicWDC3 = 0x33434457; // 'WDC3'

#pragma pack(push, 1)
    struct FieldStructure
    {
        int16  size;       // byteSize = (32 - size) / 8; negative size => field larger than 32 bits
        uint16 position;   // byte offset of the field within the record
    };

    struct FieldStorageInfo
    {
        uint16 fieldOffsetBits;
        uint16 fieldSizeBits;      // total bits of the field (arrays summed)
        uint32 additionalDataSize; // bytes this field occupies in pallet_data or common_data
        uint32 storageType;        // field_compression enum
        uint32 p1, p2, p3;         // type-specific (default_value for common, array_count for type 4, ...)
    };

    struct Wdc1Header
    {
        uint32 magic, recordCount, fieldCount, recordSize, stringTableSize;
        uint32 tableHash, layoutHash, minId, maxId, locale, copyTableSize;
        uint16 flags, idIndex;
        uint32 totalFieldCount, bitpackedDataOffset, lookupColumnCount, offsetMapOffset;
        uint32 idListSize, fieldStorageInfoSize, commonDataSize, palletDataSize, relationshipDataSize;
    };

    struct Wdc23Header
    {
        uint32 magic, recordCount, fieldCount, recordSize, stringTableSize;
        uint32 tableHash, layoutHash, minId, maxId, locale;
        uint16 flags, idIndex;
        uint32 totalFieldCount, bitpackedDataOffset, lookupColumnCount;
        uint32 fieldStorageInfoSize, commonDataSize, palletDataSize, sectionCount;
    };

    struct Wdc2Section
    {
        uint64 tactKeyHash;
        uint32 fileOffset, recordCount, stringTableSize, copyTableSize;
        uint32 offsetMapOffset, idListSize, relationshipDataSize;
    };

    struct Wdc3Section
    {
        uint64 tactKeyHash;
        uint32 fileOffset, recordCount, stringTableSize, offsetRecordsEnd;
        uint32 idListSize, relationshipDataSize, offsetMapIdCount, copyTableCount;
    };
#pragma pack(pop)

    // Little-endian bitfield read of up to ~57 useful bits (enough for every DB2 bitpacked field).
    uint64 ReadBitsU(const uint8* p, uint32 bitOffset, uint32 bitCount)
    {
        if (bitCount == 0) return 0;
        uint32 byteIdx = bitOffset >> 3;
        uint32 shift   = bitOffset & 7;
        uint32 need    = (bitCount + shift + 7) >> 3;
        uint64 v = 0;
        for (uint32 i = 0; i < need && i < 8; ++i)
            v |= static_cast<uint64>(p[byteIdx + i]) << (8 * i);
        v >>= shift;
        if (bitCount < 64) v &= (static_cast<uint64>(1) << bitCount) - 1;
        return v;
    }

    int32 SignExtend(uint64 v, uint32 bits)
    {
        if (bits == 0 || bits >= 32) return static_cast<int32>(v);
        uint64 m = static_cast<uint64>(1) << (bits - 1);
        return static_cast<int32>((v ^ m) - m);
    }

    // One section's assembled blocks (pointers into the file buffer).
    struct Section
    {
        const uint8* records = nullptr;
        uint32 recordCount = 0;
        uint32 recordsFileOffset = 0;
        uint32 stringFileOffset = 0;
        uint32 stringSize = 0;
        const uint32* idList = nullptr;
        uint32 idListCount = 0;
        std::vector<std::pair<uint32, uint32>> copyTable;          // (newId, srcId)
        std::vector<std::pair<uint32, uint16>> offsetMap;          // (fileOffset, size) for flags&1
        std::vector<uint32> offsetMapIds;                            // explicit ids (WDC3); empty => implicit
        std::unordered_map<uint32, uint32> relationship;           // recordIndex -> foreignId
        uint32 stringBaseInBlob = 0;                                 // base of this section's strings in out.strings
    };

    // Per-field decode plan, precomputed once. The decoder reconstructs each record into a native-width
    // layout (a uint16 stays 2 bytes, a uint8 stays 1 byte, an array stays consecutive), matching a
    // naturally-packed C struct, so a Definition row mirrors the table fields exactly.
    struct FieldMeta
    {
        int storageType = 0;
        uint32 fieldOffsetBits = 0;
        uint32 fieldSizeBits = 0;
        uint32 elementBytes = 4;      // native width of one element (1/2/4/8)
        uint32 elementCount = 1;      // array length
        uint32 palletBase = 0;        // byte base into palletData (type 3/4)
        int32  defaultValue = 0;      // type 2
        bool     signExt = false;       // type 1 signed
        uint32 outByteOffset = 0;     // byte offset of this field in the decoded record
        bool     isInlineId = false;    // this field carries the id (skip emitting; id slot holds it)
        std::unordered_map<uint32, uint32> common; // type 2 id -> value
    };

    struct Parsed
    {
        uint32 version = 0;           // 1, 2, 3
        uint32 recordSize = 0;
        uint32 fieldCount = 0;
        uint32 totalFieldCount = 0;
        uint16 flags = 0;
        uint16 idIndex = 0;
        int32  minId = 0;
        int32  maxId = 0;
        const FieldStructure*   fieldStruct = nullptr;
        const FieldStorageInfo* fieldInfo = nullptr;
        uint32 fieldInfoCount = 0;
        bool     hasIdList = false;      // true => id comes from a section id list (not inline)
        const uint8* palletData = nullptr;
        const uint8* commonData = nullptr;
        std::vector<Section> sections;
    };

    bool ParseWDC1(const uint8* d, uint32 size, Parsed& out)
    {
        if (size < sizeof(Wdc1Header)) return false;
        const auto& h = *reinterpret_cast<const Wdc1Header*>(d);

        out.version = 1;
        out.recordSize = h.recordSize;
        out.fieldCount = h.fieldCount;
        out.totalFieldCount = h.totalFieldCount;
        out.flags = h.flags;
        out.idIndex = h.idIndex;
        out.minId = static_cast<int32>(h.minId);
        out.maxId = static_cast<int32>(h.maxId);

        uint32 cur = sizeof(Wdc1Header);
        out.fieldStruct = reinterpret_cast<const FieldStructure*>(d + cur);
        cur += h.totalFieldCount * sizeof(FieldStructure);

        Section sec;
        const bool offsetMap = (h.flags & 0x01) != 0;
        if (!offsetMap)
        {
            sec.records = d + cur;
            sec.recordCount = h.recordCount;
            sec.recordsFileOffset = cur;
            cur += h.recordCount * h.recordSize;
            sec.stringFileOffset = cur;
            sec.stringSize = h.stringTableSize;
            cur += h.stringTableSize;
        }
        else
        {
            // Variable-length records pointed at by an offset map after the strings region.
            cur = h.offsetMapOffset;
            uint32 entries = (h.maxId >= h.minId) ? (h.maxId - h.minId + 1) : 0;
            for (uint32 i = 0; i < entries; ++i)
            {
                uint32 off = *reinterpret_cast<const uint32*>(d + cur); cur += 4;
                uint16 len = *reinterpret_cast<const uint16*>(d + cur); cur += 2;
                if (off != 0 && len != 0) sec.offsetMap.emplace_back(off, len);
            }
            sec.recordCount = static_cast<uint32>(sec.offsetMap.size());
        }

        if (h.idListSize)
        {
            sec.idList = reinterpret_cast<const uint32*>(d + cur);
            sec.idListCount = h.idListSize / 4;
            cur += h.idListSize;
            out.hasIdList = true;
        }
        if (h.copyTableSize)
        {
            uint32 n = h.copyTableSize / 8;
            const uint32* ct = reinterpret_cast<const uint32*>(d + cur);
            for (uint32 i = 0; i < n; ++i) sec.copyTable.emplace_back(ct[2 * i], ct[2 * i + 1]);
            cur += h.copyTableSize;
        }

        out.fieldInfo = reinterpret_cast<const FieldStorageInfo*>(d + cur);
        out.fieldInfoCount = h.fieldStorageInfoSize / sizeof(FieldStorageInfo);
        cur += h.fieldStorageInfoSize;
        out.palletData = d + cur; cur += h.palletDataSize;
        out.commonData = d + cur; cur += h.commonDataSize;

        if (h.relationshipDataSize)
        {
            uint32 numEntries = *reinterpret_cast<const uint32*>(d + cur);
            const uint32* e = reinterpret_cast<const uint32*>(d + cur + 12);
            for (uint32 i = 0; i < numEntries; ++i)
                sec.relationship[e[2 * i + 1]] = e[2 * i]; // recordIndex -> foreignId
            cur += h.relationshipDataSize;
        }

        out.sections.push_back(std::move(sec));
        return true;
    }

    bool ParseWDC23(const uint8* d, uint32 size, Parsed& out, uint32 version)
    {
        if (size < sizeof(Wdc23Header)) return false;
        const auto& h = *reinterpret_cast<const Wdc23Header*>(d);

        out.version = version;
        out.recordSize = h.recordSize;
        out.fieldCount = h.fieldCount;
        out.totalFieldCount = h.totalFieldCount;
        out.flags = h.flags;
        out.idIndex = h.idIndex;
        out.minId = static_cast<int32>(h.minId);
        out.maxId = static_cast<int32>(h.maxId);

        uint32 cur = sizeof(Wdc23Header);
        const uint8* sectionHeaders = d + cur;
        cur += h.sectionCount * (version == 3 ? sizeof(Wdc3Section) : sizeof(Wdc2Section));

        out.fieldStruct = reinterpret_cast<const FieldStructure*>(d + cur);
        cur += h.totalFieldCount * sizeof(FieldStructure);
        out.fieldInfo = reinterpret_cast<const FieldStorageInfo*>(d + cur);
        out.fieldInfoCount = h.fieldStorageInfoSize / sizeof(FieldStorageInfo);
        cur += h.fieldStorageInfoSize;
        out.palletData = d + cur; cur += h.palletDataSize;
        out.commonData = d + cur; cur += h.commonDataSize;

        const bool offsetMap = (h.flags & 0x01) != 0;

        for (uint32 s = 0; s < h.sectionCount; ++s)
        {
            Section sec;
            if (version == 3)
            {
                const auto& sh = reinterpret_cast<const Wdc3Section*>(sectionHeaders)[s];
                uint32 p = sh.fileOffset;
                if (!offsetMap)
                {
                    sec.records = d + p;
                    sec.recordCount = sh.recordCount;
                    sec.recordsFileOffset = p;
                    p += sh.recordCount * h.recordSize;
                    sec.stringFileOffset = p;
                    sec.stringSize = sh.stringTableSize;
                    p += sh.stringTableSize;
                }
                else
                {
                    sec.recordsFileOffset = p;
                    p = sh.offsetRecordsEnd;
                }
                if (sh.idListSize) { sec.idList = reinterpret_cast<const uint32*>(d + p); sec.idListCount = sh.idListSize / 4; p += sh.idListSize; out.hasIdList = true; }
                if (sh.copyTableCount)
                {
                    const uint32* ct = reinterpret_cast<const uint32*>(d + p);
                    for (uint32 i = 0; i < sh.copyTableCount; ++i) sec.copyTable.emplace_back(ct[2 * i], ct[2 * i + 1]);
                    p += sh.copyTableCount * 8;
                }
                if (offsetMap)
                {
                    const uint8* om = d + p;
                    const uint32* ids = reinterpret_cast<const uint32*>(om + sh.offsetMapIdCount * 6);
                    for (uint32 i = 0; i < sh.offsetMapIdCount; ++i)
                    {
                        uint32 off = *reinterpret_cast<const uint32*>(om + i * 6);
                        uint16 len = *reinterpret_cast<const uint16*>(om + i * 6 + 4);
                        sec.offsetMap.emplace_back(off, len);
                        sec.offsetMapIds.push_back(ids[i]);
                    }
                    sec.recordCount = sh.offsetMapIdCount;
                    p += sh.offsetMapIdCount * 6 + sh.offsetMapIdCount * 4;
                }
                if (sh.relationshipDataSize)
                {
                    uint32 numEntries = *reinterpret_cast<const uint32*>(d + p);
                    const uint32* e = reinterpret_cast<const uint32*>(d + p + 12);
                    for (uint32 i = 0; i < numEntries; ++i) sec.relationship[e[2 * i + 1]] = e[2 * i];
                    p += sh.relationshipDataSize;
                }
            }
            else // WDC2
            {
                const auto& sh = reinterpret_cast<const Wdc2Section*>(sectionHeaders)[s];
                uint32 p = sh.fileOffset;
                if (!offsetMap)
                {
                    sec.records = d + p;
                    sec.recordCount = sh.recordCount;
                    sec.recordsFileOffset = p;
                    p += sh.recordCount * h.recordSize;
                    sec.stringFileOffset = p;
                    sec.stringSize = sh.stringTableSize;
                    p += sh.stringTableSize;
                }
                else
                {
                    sec.recordsFileOffset = p;
                    uint32 entries = (h.maxId >= h.minId) ? (h.maxId - h.minId + 1) : 0;
                    uint32 om = sh.offsetMapOffset;
                    for (uint32 i = 0; i < entries; ++i)
                    {
                        uint32 off = *reinterpret_cast<const uint32*>(d + om); om += 4;
                        uint16 len = *reinterpret_cast<const uint16*>(d + om); om += 2;
                        if (off != 0 && len != 0) sec.offsetMap.emplace_back(off, len);
                    }
                    sec.recordCount = static_cast<uint32>(sec.offsetMap.size());
                    p = om;
                }
                if (sh.idListSize) { sec.idList = reinterpret_cast<const uint32*>(d + p); sec.idListCount = sh.idListSize / 4; p += sh.idListSize; out.hasIdList = true; }
                if (sh.copyTableSize)
                {
                    uint32 n = sh.copyTableSize / 8;
                    const uint32* ct = reinterpret_cast<const uint32*>(d + p);
                    for (uint32 i = 0; i < n; ++i) sec.copyTable.emplace_back(ct[2 * i], ct[2 * i + 1]);
                    p += sh.copyTableSize;
                }
                if (sh.relationshipDataSize)
                {
                    uint32 numEntries = *reinterpret_cast<const uint32*>(d + p);
                    const uint32* e = reinterpret_cast<const uint32*>(d + p + 12);
                    for (uint32 i = 0; i < numEntries; ++i) sec.relationship[e[2 * i + 1]] = e[2 * i];
                    p += sh.relationshipDataSize;
                }
            }
            out.sections.push_back(std::move(sec));
        }
        return true;
    }

    inline uint32 AlignUp(uint32 o, uint32 a) { return (o + a - 1) & ~(a - 1); }

    // Round a raw on-disk field width to a width a C struct member can have (1/2/4/8).
    inline uint32 StdWidth(uint32 bytes)
    {
        if (bytes <= 1) return 1;
        if (bytes == 2) return 2;
        if (bytes <= 4) return 4;
        return 8;
    }

    // Native decoded-record layout, computed once. The record is [id u32][fields in file order at native
    // widths][relationship u32], laid out with natural C alignment so a Definition struct matches it.
    struct Layout
    {
        uint32 rowSize = 0;
        uint32 idOffset = 0;
        uint32 relOffset = 0;
        bool     hasRel = false;
    };

    Layout BuildLayout(const Parsed& p, std::vector<FieldMeta>& meta, bool hasRel)
    {
        meta.resize(p.fieldInfoCount);
        const bool inlineId = !p.hasIdList;

        uint32 palletCursor = 0, commonCursor = 0;
        Layout lay;
        lay.hasRel = hasRel;

        uint32 o = 0, maxAlign = 4;
        lay.idOffset = o; o += 4; // id is always the first u32

        for (uint32 i = 0; i < p.fieldInfoCount; ++i)
        {
            const FieldStorageInfo& fi = p.fieldInfo[i];
            FieldMeta& m = meta[i];
            m.storageType = static_cast<int>(fi.storageType);
            m.fieldOffsetBits = fi.fieldOffsetBits;
            m.fieldSizeBits = fi.fieldSizeBits;

            // On-disk element width from the field-structure block (logical width of one element).
            uint32 diskBytes = 4;
            if (i < p.totalFieldCount && p.fieldStruct)
            {
                int16 sz = p.fieldStruct[i].size;
                diskBytes = static_cast<uint32>((32 - sz) / 8);
                if (diskBytes == 0) diskBytes = 4;
            }
            m.elementBytes = StdWidth(diskBytes);

            switch (fi.storageType)
            {
                case 0: // none (inline): element width = diskBytes, count from total bits
                {
                    uint32 elemBits = diskBytes * 8;
                    uint32 totalBits = fi.fieldSizeBits ? fi.fieldSizeBits : elemBits;
                    m.elementCount = elemBits ? (totalBits / elemBits) : 1;
                    if (m.elementCount == 0) m.elementCount = 1;
                    break;
                }
                case 2: // common data
                {
                    m.elementCount = 1;
                    m.defaultValue = static_cast<int32>(fi.p1);
                    uint32 n = fi.additionalDataSize / 8;
                    const uint32* cd = reinterpret_cast<const uint32*>(p.commonData + commonCursor);
                    for (uint32 k = 0; k < n; ++k) m.common[cd[2 * k]] = cd[2 * k + 1];
                    commonCursor += fi.additionalDataSize;
                    break;
                }
                case 1: // bitpacked
                    m.elementCount = 1;
                    m.signExt = (fi.p3 & 0x01) != 0;
                    break;
                case 3: // bitpacked indexed
                    m.elementCount = 1;
                    m.palletBase = palletCursor;
                    palletCursor += fi.additionalDataSize;
                    break;
                case 4: // bitpacked indexed array
                    m.elementCount = fi.p3 ? fi.p3 : 1;
                    m.palletBase = palletCursor;
                    palletCursor += fi.additionalDataSize;
                    break;
                default:
                    m.elementCount = 1;
                    break;
            }

            // The inline id field is represented by the leading id slot, not emitted again.
            if (inlineId && i == p.idIndex)
            {
                m.isInlineId = true;
                continue;
            }

            if (m.elementBytes > maxAlign) maxAlign = m.elementBytes;
            o = AlignUp(o, m.elementBytes);
            m.outByteOffset = o;
            o += m.elementBytes * m.elementCount;
        }

        if (hasRel)
        {
            o = AlignUp(o, 4);
            lay.relOffset = o;
            o += 4;
        }
        lay.rowSize = AlignUp(o, maxAlign);
        return lay;
    }

    inline void WriteVal(char* dst, uint32 width, uint64 v)
    {
        for (uint32 b = 0; b < width && b < 8; ++b) dst[b] = static_cast<char>((v >> (8 * b)) & 0xFF);
    }

    // Decode one field of one record and write its native-width value(s) into recOut.
    void DecodeField(const FieldMeta& m, const Parsed& p, const uint8* rec, int32 id, char* recOut)
    {
        if (m.isInlineId) return;
        char* dst = recOut + m.outByteOffset;
        switch (m.storageType)
        {
            case 0:
            {
                uint32 diskBytes = (m.fieldSizeBits && m.elementCount) ? (m.fieldSizeBits / 8 / m.elementCount) : m.elementBytes;
                if (diskBytes == 0) diskBytes = m.elementBytes;
                uint32 baseByte = m.fieldOffsetBits / 8;
                for (uint32 e = 0; e < m.elementCount; ++e)
                {
                    uint64 v = 0;
                    const uint8* src = rec + baseByte + e * diskBytes;
                    for (uint32 b = 0; b < diskBytes && b < 8; ++b) v |= static_cast<uint64>(src[b]) << (8 * b);
                    WriteVal(dst + e * m.elementBytes, m.elementBytes, v);
                }
                break;
            }
            case 1:
            {
                uint64 v = ReadBitsU(rec, m.fieldOffsetBits, m.fieldSizeBits);
                int32 iv = m.signExt ? SignExtend(v, m.fieldSizeBits) : static_cast<int32>(v);
                WriteVal(dst, m.elementBytes, static_cast<uint32>(iv));
                break;
            }
            case 2:
            {
                auto it = m.common.find(static_cast<uint32>(id));
                int32 iv = (it != m.common.end()) ? static_cast<int32>(it->second) : m.defaultValue;
                WriteVal(dst, m.elementBytes, static_cast<uint32>(iv));
                break;
            }
            case 3:
            {
                uint64 idx = ReadBitsU(rec, m.fieldOffsetBits, m.fieldSizeBits);
                const uint32* pal = reinterpret_cast<const uint32*>(p.palletData + m.palletBase);
                WriteVal(dst, m.elementBytes, pal[idx]);
                break;
            }
            case 4:
            {
                uint64 idx = ReadBitsU(rec, m.fieldOffsetBits, m.fieldSizeBits);
                const uint32* pal = reinterpret_cast<const uint32*>(p.palletData + m.palletBase);
                for (uint32 a = 0; a < m.elementCount; ++a)
                    WriteVal(dst + a * m.elementBytes, m.elementBytes, pal[idx * m.elementCount + a]);
                break;
            }
            default:
                break;
        }
    }

    // Read the scalar value of one field (used to resolve an inline id).
    int32 ReadScalar(const FieldMeta& m, const Parsed& p, const uint8* rec)
    {
        switch (m.storageType)
        {
            case 0:
            {
                uint32 diskBytes = (m.fieldSizeBits) ? (m.fieldSizeBits / 8) : m.elementBytes;
                if (diskBytes == 0 || diskBytes > 8) diskBytes = 4;
                uint64 v = 0;
                const uint8* src = rec + m.fieldOffsetBits / 8;
                for (uint32 b = 0; b < diskBytes && b < 8; ++b) v |= static_cast<uint64>(src[b]) << (8 * b);
                return static_cast<int32>(v);
            }
            case 1:
                return static_cast<int32>(ReadBitsU(rec, m.fieldOffsetBits, m.fieldSizeBits));
            case 3:
            {
                uint64 idx = ReadBitsU(rec, m.fieldOffsetBits, m.fieldSizeBits);
                return static_cast<int32>(reinterpret_cast<const uint32*>(p.palletData + m.palletBase)[idx]);
            }
            default:
                return 0;
        }
    }

    // Resolve a record's id: from the section id list, else from the inline id field (idIndex).
    int32 ResolveId(const Parsed& p, const std::vector<FieldMeta>& meta, const Section& sec, const uint8* rec,
                      uint32 recordIndex)
    {
        if (sec.idList && recordIndex < sec.idListCount)
            return static_cast<int32>(sec.idList[recordIndex]);
        if (p.idIndex < meta.size())
            return ReadScalar(meta[p.idIndex], p, rec);
        return static_cast<int32>(recordIndex);
    }
}

namespace ModernM2::Db2
{
    bool DecodeDB2(const uint8* data, uint32 size, DB2Decoded& out, const uint32* stringColumns,
                   uint32 stringColumnCount)
    {
        out = DB2Decoded{};
        if (size < 4) return false;

        uint32 magic = *reinterpret_cast<const uint32*>(data);
        Parsed p;
        bool ok = false;
        if (magic == kMagicWDC1) ok = ParseWDC1(data, size, p);
        else if (magic == kMagicWDC2 || magic == kMagic1SLC) ok = ParseWDC23(data, size, p, 2);
        else if (magic == kMagicWDC3) ok = ParseWDC23(data, size, p, 3);
        else return false;
        if (!ok) return false;

        std::vector<FieldMeta> meta;
        bool hasRel = false;
        for (const auto& s : p.sections) if (!s.relationship.empty()) hasRel = true;
        out.hasRelationship = hasRel;

        Layout lay = BuildLayout(p, meta, hasRel);
        out.rowSize = lay.rowSize;

        // Reconstruct one contiguous string table from every section.
        size_t totalStringBytes = 0;
        for (const auto& s : p.sections) totalStringBytes += s.stringSize;
        out.strings.reserve(totalStringBytes + 1);
        for (auto& s : p.sections)
        {
            s.stringBaseInBlob = static_cast<uint32>(out.strings.size());
            if (s.stringSize)
            {
                const char* st = reinterpret_cast<const char*>(data + s.stringFileOffset);
                out.strings.insert(out.strings.end(), st, st + s.stringSize);
            }
        }
        if (out.strings.empty()) out.strings.push_back('\0');

        const bool multiSection = p.sections.size() > 1;

        size_t sectionRows = 0, copyRows = 0;
        for (const auto& s : p.sections)
        {
            sectionRows += s.recordCount;
            copyRows += s.copyTable.size();
        }
        const size_t plannedRows = sectionRows + copyRows;
        if (!plannedRows || !out.rowSize) return false;
        if (plannedRows > (0x40000000u / out.rowSize)) return false;
        out.records.resize(plannedRows * out.rowSize);
        out.ids.reserve(plannedRows);
        size_t rowCursor = 0;

        // Only a copy table ever reads this, and filling it costs a hash node per record.
        std::unordered_map<int32, uint32> idToRow;
        if (copyRows) idToRow.reserve(sectionRows);

        // Decode every section's records.
        for (const auto& sec : p.sections)
        {
            const bool offsetMap = !sec.offsetMap.empty();
            for (uint32 i = 0; i < sec.recordCount; ++i)
            {
                const uint8* rec = offsetMap ? (data + sec.offsetMap[i].first)
                                               : (sec.records + static_cast<size_t>(i) * p.recordSize);

                int32 id;
                if (offsetMap)
                    id = !sec.offsetMapIds.empty() ? static_cast<int32>(sec.offsetMapIds[i])
                                                   : (p.minId + static_cast<int32>(i));
                else
                    id = ResolveId(p, meta, sec, rec, i);

                char* recOut = out.records.data() + rowCursor;
                rowCursor += out.rowSize;

                WriteVal(recOut + lay.idOffset, 4, static_cast<uint32>(id)); // id slot first

                for (uint32 f = 0; f < meta.size(); ++f)
                    DecodeField(meta[f], p, rec, id, recOut);

                if (hasRel)
                {
                    auto it = sec.relationship.find(i);
                    WriteVal(recOut + lay.relOffset, 4, (it != sec.relationship.end()) ? it->second : 0u);
                }

                // Fix string fields when the raw-offset assumption does not hold (WDC2+ or multi-section).
                // stringColumns lists FIELD indices that hold a string offset.
                if (stringColumns && stringColumnCount && (p.version >= 2 || multiSection) && !offsetMap)
                {
                    for (uint32 sc = 0; sc < stringColumnCount; ++sc)
                    {
                        uint32 f = stringColumns[sc];
                        if (f >= meta.size() || meta[f].isInlineId) continue;
                        char* dst = recOut + meta[f].outByteOffset;
                        uint32 raw; std::memcpy(&raw, dst, 4);
                        if (raw == 0) continue;
                        uint32 fixed;
                        if (p.version >= 2)
                        {
                            // WDC2+ stores the offset relative to the field's own position in the file blob.
                            uint32 fieldFilePos = sec.recordsFileOffset + i * p.recordSize + meta[f].fieldOffsetBits / 8;
                            fixed = raw + fieldFilePos - sec.stringFileOffset + sec.stringBaseInBlob;
                        }
                        else
                        {
                            fixed = raw + sec.stringBaseInBlob;
                        }
                        std::memcpy(dst, &fixed, 4);
                    }
                }

                if (copyRows) idToRow[id] = static_cast<uint32>(out.ids.size());
                out.ids.push_back(id);
            }
        }

        // Copy table: each entry duplicates an existing record under a new id.
        for (const auto& sec : p.sections)
        {
            for (const auto& cp : sec.copyTable)
            {
                auto it = idToRow.find(static_cast<int32>(cp.second));
                if (it == idToRow.end()) continue;
                const size_t srcStart = static_cast<size_t>(it->second) * out.rowSize;
                std::memcpy(out.records.data() + rowCursor, out.records.data() + srcStart, out.rowSize);
                rowCursor += out.rowSize;
                out.ids.push_back(static_cast<int32>(cp.first));
            }
        }

        if (out.ids.size() != plannedRows)
            out.records.resize(out.ids.size() * out.rowSize);

        return !out.ids.empty();
    }
}
