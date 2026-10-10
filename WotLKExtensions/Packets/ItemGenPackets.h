#pragma once

#include <Defines.h>

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
		int32 id = 0;
		std::string name;
		uint32 mask = 0;
		uint32 flags = 0;
		uint32 weight = 0;
		int32 trinketGroup = -1;
	};

	struct StatGroupMaskEntry
	{
		uint32 bit = 0;
		std::string name;
	};

	struct LegendaryEntry
	{
		uint32 id = 0;
		std::string name;
		std::string nameOverride;
		uint32 flags = 0;
		int32 minItemLevel = -1;
		int32 maxItemLevel = -1;
		int32 itemClass = -1;
		int32 itemSubClass = -1;
		int32 inventoryType = -1;
		int32 statGroup = -1;
		uint32 statGroupMask = 0;
		int32 statGroupOverride = -1;
		uint32 uniqueId = 0;
	};

	struct UniqueEntry
	{
		uint32 id = 0;
		std::string name;
		std::string description;
		uint32 quality = 0;
		uint32 itemClass = 0;
		uint32 subClass = 0;
		uint32 inventoryType = 0;
		int32 statGroup = -1;
		uint32 flags = 0;
		uint32 itemLevelMin = 0;
		uint32 itemLevelMax = 0;
		bool spawnable = false;
	};

	static void Handler_SMSG_ITEMGEN_STAT_GROUPS(void*, uint32, uint32, CDataStore* pkt);
	static void Handler_SMSG_ITEMGEN_LEGENDARIES(void*, uint32, uint32, CDataStore* pkt);
	static void Handler_SMSG_ITEMGEN_UNIQUES(void*, uint32, uint32, CDataStore* pkt);
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
