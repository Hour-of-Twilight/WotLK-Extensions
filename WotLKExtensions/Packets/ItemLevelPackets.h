#pragma once

#include <Defines.h>

#include <cstdint>
#include <unordered_map>

struct lua_State;
struct CDataStore;

class ItemLevelPackets
{
public:
	static ItemLevelPackets& Instance();

	ItemLevelPackets(const ItemLevelPackets&) = delete;
	ItemLevelPackets& operator=(const ItemLevelPackets&) = delete;

	void Apply();

	void Clear();

private:
	ItemLevelPackets() = default;

	static void Handler_SMSG_ITEM_LEVEL_BREAKDOWN(void*, uint32, uint32, CDataStore* pkt);
	static int RequestItemLevelBreakdown(lua_State* L);
	static int HasItemLevelBreakdown(lua_State* L);
	static int GetItemLevelSubClass(lua_State* L);
	static int GetItemLevelBreakdownSlot(lua_State* L);
	static int GetItemLevelBreakdownAverage(lua_State* L);

	uint8 m_subClass = 0;
	std::unordered_map<uint8, uint32> m_slotLevels;
	uint32 m_averageItemLevel = 0; // uncapped, unlike GetOurItemLevel
	bool m_hasData = false;
};

#define sItemLevelPackets ItemLevelPackets::Instance()
