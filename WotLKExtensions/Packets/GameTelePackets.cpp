#include "GameTelePackets.h"
#include "Packet.h"
#include <CustomPacket.h>
#include <CustomLua.h>
#include <XMLExtensions.h>
#include <SharedDefines.h>

GameTelePackets& GameTelePackets::Instance()
{
	static GameTelePackets instance;
	return instance;
}

void GameTelePackets::Handler_SMSG_GAME_TELE_LIST(void*, uint32_t, uint32_t, CDataStore* pkt)
{
	Packet r(pkt);
	char buffer[512];

	GameTelePackets& self = Instance();
	self.m_teles.clear();

	uint32_t count = r.GetUInt32();
	self.m_teles.reserve(count);
	for (uint32_t i = 0; i < count; ++i)
	{
		TeleEntry entry;
		entry.id = r.GetUInt32();
		r.GetString(buffer, sizeof(buffer));
		entry.name = buffer;
		entry.mapId = r.GetUInt32();
		self.m_teles.push_back(entry);
	}

	self.m_hasData = true;
	FrameXMLExtensions::SignalEvent("HOT_GAME_TELES", "");
}

int GameTelePackets::RequestGameTeles(lua_State*)
{
	Packet(CMSG_GAME_TELE_LIST_REQUEST).Send();
	return 0;
}

int GameTelePackets::GetGameTeleCount(lua_State* L)
{
	GameTelePackets& self = Instance();
	FrameScript::PushNumber(L, self.m_hasData ? self.m_teles.size() : 0);
	return 1;
}

int GameTelePackets::GetGameTeleInfo(lua_State* L)
{
	GameTelePackets& self = Instance();
	uint32_t index = static_cast<uint32_t>(FrameScript::GetNumber(L, 1));
	if (!self.m_hasData || index < 1 || index > self.m_teles.size())
	{
		FrameScript::PushNil(L);
		return 1;
	}

	TeleEntry const& entry = self.m_teles[index - 1];
	FrameScript::PushNumber(L, entry.id);
	FrameScript::PushString(L, entry.name.c_str());
	FrameScript::PushNumber(L, entry.mapId);
	return 3;
}

void GameTelePackets::Apply()
{
	sCustomPacket.RegisterHandler(SMSG_GAME_TELE_LIST, &Handler_SMSG_GAME_TELE_LIST);

	sLua.RegisterFunction("RequestGameTeles", &RequestGameTeles, LuaFunctionState::FRAME);
	sLua.RegisterFunction("GetGameTeleCount", &GetGameTeleCount, LuaFunctionState::FRAME);
	sLua.RegisterFunction("GetGameTeleInfo", &GetGameTeleInfo, LuaFunctionState::FRAME);
}
