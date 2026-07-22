#include "DBCRecordPackets.h"
#include "Packet.h"

#include <ClientData/Achievements.h>
#include <CustomPacket.h>
#include <DBCPatch.h>
#include <SharedDefines.h>

#include <cstdint>
#include <cstring>
#include <vector>

void DBCRecordPackets::Handler_SMSG_DBC_RECORD_UPDATE(void*, uint32_t, uint32_t, CDataStore* pkt)
{
	Packet r(pkt);
	char dbcName[64] = {};
	r.GetString(dbcName, sizeof(dbcName));

	r.GetUInt8(); // mode (unused)
	uint32_t recordSize = r.GetUInt16();

	uint8_t strCount = r.GetUInt8();
	std::vector<uint16_t> strOffsets(strCount);
	for (uint8_t i = 0; i < strCount; ++i)
		strOffsets[i] = r.GetUInt16();

	uint32_t recordCount = r.GetUInt32();

	std::vector<uint32_t> ids(recordCount);
	std::vector<uint8_t> images(static_cast<size_t>(recordCount) * recordSize);
	for (uint32_t i = 0; i < recordCount; ++i)
	{
		ids[i] = r.GetUInt32();
		r.GetBytes(images.data() + static_cast<size_t>(i) * recordSize, recordSize);
	}

	uint32_t blobSize = r.GetUInt32();
	std::vector<char> blob(blobSize);
	if (blobSize)
		r.GetBytes(blob.data(), blobSize);

	DBCPatch::ApplyRecords(dbcName, recordSize, strOffsets, ids, images.data(),
	    blob.empty() ? nullptr : blob.data(), blobSize);

	// Achievement rows also live in CGAchievementInfo's own index, which DBC patching doesn't touch.
	if (_strnicmp(dbcName, "achievement", 11) == 0)
		ClientData::Achievements::RequestRebuild();

	r.Release();
}

void DBCRecordPackets::Apply()
{
	sCustomPacket.RegisterHandler(SMSG_DBC_RECORD_UPDATE, &DBCRecordPackets::Handler_SMSG_DBC_RECORD_UPDATE);
}
