#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace ClientData
{
	struct WoWClientDB;
}

// In-memory native client DBC patching: adds/replaces records in a WowClientDB storage without a
// reload, and hands our own rows back uncompressed via the DecompressRow hook. See the
// SMSG_DBC_RECORD_UPDATE handler in Packets/DBCRecordPackets.cpp and Streaming/DbcFromMpq.cpp.
class DBCPatch
{
public:
	static bool ApplyRecords(const char* dbcName, uint32_t recordSize,
	    const std::vector<uint16_t>& strOffsets,
	    const std::vector<uint32_t>& ids, const uint8_t* images,
	    const char* strBlock, uint32_t strBlockSize);

	// Called by the DecompressRow client hook: true if p is one of our patched-row buffers.
	static bool IsPatchedRow(const void* p);

private:
	struct PatchRange
	{
		uintptr_t begin, end;
	};

	static std::vector<PatchRange>& PatchedRanges();
	static void AddPatchedRange(const void* begin, const void* end);

	static ClientData::WoWClientDB* FindStorage(const char* name);
	static bool EnsureCapacity(ClientData::WoWClientDB* db, int wantMin, int wantMax);

	static void RebuildDenseArray(ClientData::WoWClientDB* db, uint32_t recordSize, bool inlineStorage);
	static void RebuildInlineDense(ClientData::WoWClientDB* db, void** byId, int idCount, size_t count, uint32_t recordSize);
	static void RebuildPointerDense(ClientData::WoWClientDB* db, void** byId, int idCount, size_t count);
	static void** SlotFor(ClientData::WoWClientDB* db, uint32_t id);
	static void FixupStrings(void* record, const std::vector<uint16_t>& strOffsets, const char* internedBlob, uint32_t blobSize);
	static void WriteRecord(void* dst, const uint8_t* image, uint32_t recordSize, const std::vector<uint16_t>& strOffsets, const char* blob, uint32_t blobSize);
};
