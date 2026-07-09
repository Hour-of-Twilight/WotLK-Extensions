#pragma once

#include <cstdint>

struct CDataStore;

// Receives SMSG_DBC_RECORD_UPDATE and forwards the records to DBCPatch (CDBCMgr/DBCPatch.h).
class DBCRecordPackets
{
public:
	static void Apply();

private:
	static void Handler_SMSG_DBC_RECORD_UPDATE(void*, uint32_t, uint32_t, CDataStore* pkt);
};
