#pragma once

#include <cstdint>
#include <string>
#include <vector>

struct lua_State;
struct CDataStore;

class ItemGenPackets
{
public:
	static ItemGenPackets& Instance();

	ItemGenPackets(const ItemGenPackets&) = delete;
	ItemGenPackets& operator=(const ItemGenPackets&) = delete;

	void Apply();

private:
	ItemGenPackets() = default;

	struct StatGroupEntry
	{
		int32_t id = 0;
		std::string name;
		uint32_t mask = 0;
		uint32_t flags = 0;
		uint32_t weight = 0;
		int32_t trinketGroup = -1;
	};

	struct StatGroupMaskEntry
	{
		uint32_t bit = 0;
		std::string name;
	};

	struct LegendaryEntry
	{
		uint32_t id = 0;
		std::string name;
		std::string nameOverride;
		uint32_t flags = 0;
		int32_t minItemLevel = -1;
		int32_t maxItemLevel = -1;
		int32_t itemClass = -1;
		int32_t itemSubClass = -1;
		int32_t inventoryType = -1;
		int32_t statGroup = -1;
		uint32_t statGroupMask = 0;
		int32_t statGroupOverride = -1;
		uint32_t uniqueId = 0;
	};

	struct UniqueEntry
	{
		uint32_t id = 0;
		std::string name;
		std::string description;
		uint32_t quality = 0;
		uint32_t itemClass = 0;
		uint32_t subClass = 0;
		uint32_t inventoryType = 0;
		int32_t statGroup = -1;
		uint32_t flags = 0;
		uint32_t itemLevelMin = 0;
		uint32_t itemLevelMax = 0;
		bool spawnable = false;
	};

	static void Handler_SMSG_ITEMGEN_STAT_GROUPS(void*, uint32_t, uint32_t, CDataStore* pkt);
	static void Handler_SMSG_ITEMGEN_LEGENDARIES(void*, uint32_t, uint32_t, CDataStore* pkt);
	static void Handler_SMSG_ITEMGEN_UNIQUES(void*, uint32_t, uint32_t, CDataStore* pkt);
	static int RequestStatGroups(lua_State* L);
	static int GetStatGroupCount(lua_State* L);
	static int GetStatGroupInfo(lua_State* L);
	static int GetStatGroupMaskCount(lua_State* L);
	static int GetStatGroupMaskInfo(lua_State* L);
	static int RequestLegendaries(lua_State* L);
	static int GetLegendaryCount(lua_State* L);
	static int GetLegendaryInfo(lua_State* L);
	static int RequestUniqueItems(lua_State* L);
	static int GetUniqueItemCount(lua_State* L);
	static int GetUniqueItemInfo(lua_State* L);

	std::vector<StatGroupEntry> m_groups;
	std::vector<StatGroupMaskEntry> m_maskBits;
	std::vector<LegendaryEntry> m_legendaries;
	std::vector<UniqueEntry> m_uniques;
	bool m_hasData = false;
	bool m_hasLegendaries = false;
	bool m_hasUniques = false;
};

#define sItemGenPackets ItemGenPackets::Instance()
