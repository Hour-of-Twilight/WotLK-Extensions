#pragma once

#include <array>
#include <cstdint>

struct lua_State;
struct CDataStore;

enum PlayerSettingId : uint8_t
{
	PLAYER_SETTING_AOE_LOOT = 0,
	PLAYER_SETTING_AUTO_ITEM_LEVEL = 1,

	PLAYER_SETTING_MAX
};

class PlayerSettingsPackets
{
public:
	static PlayerSettingsPackets& Instance();

	PlayerSettingsPackets(const PlayerSettingsPackets&) = delete;
	PlayerSettingsPackets& operator=(const PlayerSettingsPackets&) = delete;

	void Apply();

private:
	PlayerSettingsPackets() = default;

	static void Handler_SMSG_PLAYER_SETTINGS(void*, uint32_t, uint32_t, CDataStore* pkt);
	static int GetPlayerSetting(lua_State* L);
	static int SetPlayerSetting(lua_State* L);

	std::array<uint32_t, PLAYER_SETTING_MAX> m_values{};
	bool m_hasData = false;
};

#define sPlayerSettingsPackets PlayerSettingsPackets::Instance()
