#pragma once

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

private:
	ItemLevelPackets() = default;

	static void Handler_SMSG_ITEM_LEVEL_BREAKDOWN(void*, uint32_t, uint32_t, CDataStore* pkt);
	static int RequestItemLevelBreakdown(lua_State* L);
	static int HasItemLevelBreakdown(lua_State* L);
	static int GetItemLevelSubClass(lua_State* L);
	static int GetItemLevelBreakdownSlot(lua_State* L);

	uint8_t m_subClass = 0;
	std::unordered_map<uint8_t, uint32_t> m_slotLevels;
	bool m_hasData = false;
};

#define sItemLevelPackets ItemLevelPackets::Instance()
