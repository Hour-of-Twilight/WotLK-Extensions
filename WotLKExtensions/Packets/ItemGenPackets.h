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

	static void Handler_SMSG_ITEMGEN_STAT_GROUPS(void*, uint32_t, uint32_t, CDataStore* pkt);
	static int RequestStatGroups(lua_State* L);
	static int GetStatGroupCount(lua_State* L);
	static int GetStatGroupInfo(lua_State* L);
	static int GetStatGroupMaskCount(lua_State* L);
	static int GetStatGroupMaskInfo(lua_State* L);

	std::vector<StatGroupEntry> m_groups;
	std::vector<StatGroupMaskEntry> m_maskBits;
	bool m_hasData = false;
};

#define sItemGenPackets ItemGenPackets::Instance()
