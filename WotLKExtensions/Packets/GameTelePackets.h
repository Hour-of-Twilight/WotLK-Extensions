#pragma once

#include <cstdint>
#include <string>
#include <vector>

struct lua_State;
struct CDataStore;

class GameTelePackets
{
public:
	static GameTelePackets& Instance();

	GameTelePackets(const GameTelePackets&) = delete;
	GameTelePackets& operator=(const GameTelePackets&) = delete;

	void Apply();

private:
	GameTelePackets() = default;

	struct TeleEntry
	{
		uint32_t id = 0;
		std::string name;
		uint32_t mapId = 0;
	};

	static void Handler_SMSG_GAME_TELE_LIST(void*, uint32_t, uint32_t, CDataStore* pkt);
	static int RequestGameTeles(lua_State* L);
	static int GetGameTeleCount(lua_State* L);
	static int GetGameTeleInfo(lua_State* L);

	std::vector<TeleEntry> m_teles;
	bool m_hasData = false;
};

#define sGameTelePackets GameTelePackets::Instance()
