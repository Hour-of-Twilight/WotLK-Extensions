#pragma once

#include <cstdint>
#include <string>
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

	// SMSG_GEM_SOCKET_OPEN's one byte. Matches GemSocketOpenMode in the emulator's
	// SharedDefines.h.
	enum GemSocketOpenMode : uint8_t
	{
		GEM_SOCKET_OPEN_JEWELCRAFTING = 0,
		GEM_SOCKET_OPEN_PANEL = 1,
	};

	struct GemSocketEntry
	{
		uint8_t slot = 0;
		uint32_t itemId = 0;
		uint8_t state = 0; // 0=active empty, 1=active filled, 2=deactivated
	};

	struct GemLoadoutEntry
	{
		uint8_t id = 0;
		std::string name;
		uint8_t sockets = 0;   // capacity of that board
		uint8_t filled = 0;    // gems sitting in it
		uint8_t purchased = 0; // sockets bought for that board
	};

	static void Handler_SMSG_GEM_SOCKET_LIST(void*, uint32_t, uint32_t, CDataStore* pkt);
	static void Handler_SMSG_GEM_SOCKET_ERROR(void*, uint32_t, uint32_t, CDataStore* pkt);
	static void Handler_SMSG_GEM_SOCKET_COST(void*, uint32_t, uint32_t, CDataStore* pkt);
	static void Handler_SMSG_GEM_SOCKET_OPEN(void*, uint32_t, uint32_t, CDataStore* pkt);
	static void Handler_SMSG_GEM_LOADOUTS(void*, uint32_t, uint32_t, CDataStore* pkt);
	static void Handler_SMSG_GEM_LOADOUT_RESULT(void*, uint32_t, uint32_t, CDataStore* pkt);
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
	static int RequestGemLoadouts(lua_State* L);
	static int GetGemLoadouts(lua_State* L);
	static int GetActiveGemLoadout(lua_State* L);
	static int GetMaxGemLoadouts(lua_State* L);
	static int GetGemLoadoutUnlockCost(lua_State* L);
	static int SwitchGemLoadout(lua_State* L);
	static int UnlockGemLoadout(lua_State* L);
	static int RenameGemLoadout(lua_State* L);

	uint8_t m_totalSockets = 0;
	uint8_t m_socketCount = 0;
	std::vector<GemSocketEntry> m_sockets;
	bool m_hasData = false;

	uint32_t m_costItemId = 0;
	uint32_t m_cost = 0;
	uint8_t m_purchased = 0;
	uint8_t m_canPurchase = 0;
	bool m_hasCost = false;

	std::vector<GemLoadoutEntry> m_loadouts;
	uint8_t m_activeLoadout = 0;
	uint8_t m_pendingLoadout = 0xFF;
	uint8_t m_maxLoadouts = 0;
	uint32_t m_unlockItemId = 0;
	uint32_t m_unlockCost = 0;
	uint8_t m_canUnlock = 0;
	bool m_hasLoadouts = false;
};

#define sGemSocketPackets GemSocketPackets::Instance()
