#include "PlayerSettingsPackets.h"
#include "Packet.h"
#include <CustomPacket.h>
#include <CustomLua.h>
#include <XMLExtensions.h>
#include <SharedDefines.h>

PlayerSettingsPackets& PlayerSettingsPackets::Instance()
{
	static PlayerSettingsPackets instance;
	return instance;
}

void PlayerSettingsPackets::Handler_SMSG_PLAYER_SETTINGS(void*, uint32_t, uint32_t, CDataStore* pkt)
{
	Packet r(pkt);
	uint8_t count = r.GetUInt8();

	PlayerSettingsPackets& self = Instance();
	for (uint8_t i = 0; i < count; ++i)
	{
		uint8_t setting = r.GetUInt8();
		uint32_t value = r.GetUInt32();
		if (setting < PLAYER_SETTING_MAX)
			self.m_values[setting] = value;
	}

	self.m_hasData = true;
	FrameXMLExtensions::SignalEvent("HOT_PLAYER_SETTINGS_UPDATE", "");
}

int PlayerSettingsPackets::GetPlayerSetting(lua_State* L)
{
	PlayerSettingsPackets& self = Instance();
	uint32_t setting = static_cast<uint32_t>(FrameScript::GetNumber(L, 1));

	if (!self.m_hasData || setting >= PLAYER_SETTING_MAX)
	{
		FrameScript::PushNil(L);
		return 1;
	}

	FrameScript::PushNumber(L, self.m_values[setting]);
	return 1;
}

int PlayerSettingsPackets::SetPlayerSetting(lua_State* L)
{
	PlayerSettingsPackets& self = Instance();
	uint32_t setting = static_cast<uint32_t>(FrameScript::GetNumber(L, 1));
	uint32_t value = static_cast<uint32_t>(FrameScript::GetNumber(L, 2));

	if (setting >= PLAYER_SETTING_MAX)
		return 0;

	self.m_values[setting] = value;
	self.m_hasData = true;

	Packet pkt(CMSG_PLAYER_SETTING_UPDATE);
	pkt.PutUInt8(static_cast<uint8_t>(setting));
	pkt.PutUInt32(value);
	pkt.Send();
	return 0;
}

void PlayerSettingsPackets::Apply()
{
	sCustomPacket.RegisterHandler(SMSG_PLAYER_SETTINGS, &Handler_SMSG_PLAYER_SETTINGS);

	sLua.RegisterFunction("GetPlayerSetting", &GetPlayerSetting, LuaFunctionState::FRAME);
	sLua.RegisterFunction("SetPlayerSetting", &SetPlayerSetting, LuaFunctionState::FRAME);
}
