#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace ClientData
{
	struct WoWClientDB;
}

// In-memory native client DBC patching: adds/replaces records in a WowClientDB storage without a
// reload, and hands our own rows back uncompressed via the DecompressRow hook. Fed by
// Streaming/DbcFromMpq.cpp when the background downloader mounts a patched MPQ.
class DBCPatch
{
public:
	// idsAreComplete: ids is the whole table, so rows the client had under other ids are dropped.
	static bool ApplyRecords(const char* dbcName, uint32_t recordSize,
	    const std::vector<uint16_t>& strOffsets,
	    const std::vector<uint32_t>& ids, const uint8_t* images,
	    const char* strBlock, uint32_t strBlockSize, bool idsAreComplete = false);

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

	// The client loads FirstRow in file order, which is not always id order.
	static std::vector<int> SnapshotDenseOrder(ClientData::WoWClientDB* db, uint32_t recordSize,
	    bool inlineStorage);

	static void RebuildDenseArray(ClientData::WoWClientDB* db, uint32_t recordSize, bool inlineStorage,
	    const std::vector<int>& order, const uint16_t* sortKey);
	static void RebuildInlineDense(ClientData::WoWClientDB* db, void** byId, const std::vector<int>& order, uint32_t recordSize);
	static void RebuildPointerDense(ClientData::WoWClientDB* db, void** byId, const std::vector<int>& order);
	static void** SlotFor(ClientData::WoWClientDB* db, uint32_t id);
	static void FixupStrings(void* record, const std::vector<uint16_t>& strOffsets, const char* internedBlob, uint32_t blobSize);
	static void WriteRecord(void* dst, const uint8_t* image, uint32_t recordSize, const std::vector<uint16_t>& strOffsets, const char* blob, uint32_t blobSize);

	// Storages the client post-processes after load: patched rows need the same treatment.
	static void FixupRowForClient(const char* dbcName, void* record);
	static void RefreshDerivedState(const char* dbcName, ClientData::WoWClientDB* db);
};
