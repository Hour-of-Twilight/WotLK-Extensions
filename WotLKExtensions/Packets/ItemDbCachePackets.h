#pragma once

#include <Defines.h>

#include <cstdint>

struct CDataStore;

class ItemDbCachePackets
{
public:
	static void RegisterHandlers();
	static void RegisterRealmHandlers();

private:
	static void Handler_SMSG_ITEM_DB_CACHE(void*, uint32, uint32, CDataStore* pkt);
};
