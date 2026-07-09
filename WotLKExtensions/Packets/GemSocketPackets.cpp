#include "GemSocketPackets.h"
#include "Packet.h"
#include <CustomPacket.h>
#include <CustomLua.h>
#include <XMLExtensions.h>
#include <SharedDefines.h>

GemSocketPackets& GemSocketPackets::Instance()
{
	static GemSocketPackets instance;
	return instance;
}

void GemSocketPackets::Handler_SMSG_GEM_SOCKET_LIST(void*, uint32_t, uint32_t, CDataStore* pkt)
{
	Packet r(pkt);
	uint8_t total = r.GetUInt8();
	uint8_t count = r.GetUInt8();

	GemSocketPackets& self = Instance();
	self.m_totalSockets = total;
	self.m_socketCount = count;
	self.m_sockets.clear();
	self.m_sockets.reserve(count);

	for (uint8_t i = 0; i < count; ++i)
	{
		GemSocketEntry entry;
		entry.slot = r.GetUInt8();
		entry.itemId = r.GetUInt32();
		entry.state = r.GetUInt8();
		self.m_sockets.push_back(entry);
	}

	self.m_hasData = true;
	FrameScript::SignalEvent(FrameXMLExtensions::GetEventIdByName("HOT_GEM_SOCKET_UPDATE"), "");
}

// SMSG_GEM_SOCKET_ERROR: UInt8 errorCode.
void GemSocketPackets::Handler_SMSG_GEM_SOCKET_ERROR(void*, uint32_t, uint32_t, CDataStore* pkt)
{
	int8_t code = Packet(pkt).GetInt8();
	FrameScript::SignalEvent(FrameXMLExtensions::GetEventIdByName("HOT_GEM_SOCKET_ERROR"), "%d", (int)code);
}

int GemSocketPackets::RequestGemSocketList(lua_State*)
{
	Packet(CMSG_GEM_SOCKET_LIST_REQUEST).Send();
	return 0;
}

int GemSocketPackets::GetGemSocketCount(lua_State* L)
{
	GemSocketPackets& self = Instance();
	FrameScript::PushNumber(L, self.m_hasData ? self.m_totalSockets : 0);
	return 1;
}

int GemSocketPackets::GetGemSocketEntryCount(lua_State* L)
{
	GemSocketPackets& self = Instance();
	FrameScript::PushNumber(L, self.m_hasData ? self.m_socketCount : 0);
	return 1;
}

int GemSocketPackets::GetActiveFilledCount(lua_State* L)
{
	uint8_t count = 0;
	for (GemSocketEntry const& entry : Instance().m_sockets)
		if (entry.state == 1)
			++count;
	FrameScript::PushNumber(L, count);
	return 1;
}

int GemSocketPackets::GetGemSocketInfo(lua_State* L)
{
	GemSocketPackets& self = Instance();
	if (!self.m_hasData)
	{
		FrameScript::PushNil(L);
		return 1;
	}

	uint32_t slot = static_cast<uint32_t>(FrameScript::GetNumber(L, 1));
	for (GemSocketEntry const& entry : self.m_sockets)
	{
		if (entry.slot == slot)
		{
			FrameScript::PushNumber(L, entry.itemId);
			FrameScript::PushNumber(L, entry.state);
			return 2;
		}
	}

	FrameScript::PushNil(L);
	return 1;
}

int GemSocketPackets::InsertGem(lua_State* L)
{
	uint8_t slot = static_cast<uint8_t>(FrameScript::GetNumber(L, 1));
	uint32_t itemId = static_cast<uint32_t>(FrameScript::GetNumber(L, 2));

	Packet(CMSG_GEM_SOCKET_INSERT)
	    .PutUInt8(slot)
	    .PutUInt32(itemId)
	    .Send();
	return 0;
}

int GemSocketPackets::RemoveGem(lua_State* L)
{
	int8_t slot = static_cast<int8_t>(FrameScript::GetNumber(L, 1));

	Packet(CMSG_GEM_SOCKET_REMOVE).PutInt8(slot).Send();
	return 0;
}

void GemSocketPackets::Apply()
{
	sCustomPacket.RegisterHandler(SMSG_GEM_SOCKET_LIST, &Handler_SMSG_GEM_SOCKET_LIST);
	sCustomPacket.RegisterHandler(SMSG_GEM_SOCKET_ERROR, &Handler_SMSG_GEM_SOCKET_ERROR);

	sLua.RegisterFunction("RequestGemSocketList", &RequestGemSocketList, LuaFunctionState::FRAME);
	sLua.RegisterFunction("GetGemSocketCount", &GetGemSocketCount, LuaFunctionState::FRAME);
	sLua.RegisterFunction("GetGemSocketEntryCount", &GetGemSocketEntryCount, LuaFunctionState::FRAME);
	sLua.RegisterFunction("GetGemSocketInfo", &GetGemSocketInfo, LuaFunctionState::FRAME);
	sLua.RegisterFunction("InsertGem", &InsertGem, LuaFunctionState::FRAME);
	sLua.RegisterFunction("RemoveGem", &RemoveGem, LuaFunctionState::FRAME);
	sLua.RegisterFunction("GetActiveFilledCount", &GetActiveFilledCount, LuaFunctionState::FRAME);
}
