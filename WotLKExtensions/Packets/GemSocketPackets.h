#pragma once

#include <cstdint>
#include <vector>

struct lua_State;
struct CDataStore;

class GemSocketPackets
{
public:
	static GemSocketPackets& Instance();

	GemSocketPackets(const GemSocketPackets&) = delete;
	GemSocketPackets& operator=(const GemSocketPackets&) = delete;

	void Apply();

private:
	GemSocketPackets() = default;

	struct GemSocketEntry
	{
		uint8_t slot = 0;
		uint32_t itemId = 0;
		uint8_t state = 0; // 0=active empty, 1=active filled, 2=deactivated
	};

	static void Handler_SMSG_GEM_SOCKET_LIST(void*, uint32_t, uint32_t, CDataStore* pkt);
	static void Handler_SMSG_GEM_SOCKET_ERROR(void*, uint32_t, uint32_t, CDataStore* pkt);
	static void Handler_SMSG_GEM_SOCKET_COST(void*, uint32_t, uint32_t, CDataStore* pkt);
	static void Handler_SMSG_GEM_SOCKET_OPEN(void*, uint32_t, uint32_t, CDataStore* pkt);
	static int RequestGemSocketList(lua_State* L);
	static int GetGemSocketCount(lua_State* L);
	static int GetGemSocketEntryCount(lua_State* L);
	static int GetActiveFilledCount(lua_State* L);
	static int GetGemSocketInfo(lua_State* L);
	static int InsertGem(lua_State* L);
	static int RemoveGem(lua_State* L);
	static int PurchaseGemSocket(lua_State* L);
	static int RequestGemSocketCost(lua_State* L);
	static int GetGemSocketCost(lua_State* L);

	uint8_t m_totalSockets = 0;
	uint8_t m_socketCount = 0;
	std::vector<GemSocketEntry> m_sockets;
	bool m_hasData = false;

	uint32_t m_costItemId = 0;
	uint32_t m_cost = 0;
	uint8_t m_purchased = 0;
	uint8_t m_canPurchase = 0;
	bool m_hasCost = false;
};

#define sGemSocketPackets GemSocketPackets::Instance()
