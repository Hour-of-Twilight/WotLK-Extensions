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

// SMSG_GEM_SOCKET_COST: UInt32 itemId, UInt32 cost, UInt8 purchased, UInt8 canPurchase.
void GemSocketPackets::Handler_SMSG_GEM_SOCKET_COST(void*, uint32_t, uint32_t, CDataStore* pkt)
{
	Packet r(pkt);
	GemSocketPackets& self = Instance();
	self.m_costItemId = r.GetUInt32();
	self.m_cost = r.GetUInt32();
	self.m_purchased = r.GetUInt8();
	self.m_canPurchase = r.GetUInt8();
	self.m_hasCost = true;
	FrameScript::SignalEvent(FrameXMLExtensions::GetEventIdByName("HOT_GEM_SOCKET_COST_UPDATE"), "");
}

// SMSG_GEM_SOCKET_OPEN: no payload. Open the Jewelcrafting
void GemSocketPackets::Handler_SMSG_GEM_SOCKET_OPEN(void*, uint32_t, uint32_t, CDataStore*)
{
	uint64_t guid = ClntObjMgr::GetActivePlayer();
	if (!guid)
		return;

	CGTradeSkillInfo::SetTradeSkill(25229, &guid, nullptr, 0, 0);
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
	uint8_t shatter = FrameScript::ToBoolean(L, 2) ? 1 : 0;

	Packet(CMSG_GEM_SOCKET_REMOVE).PutInt8(slot).PutUInt8(shatter).Send();
	return 0;
}

int GemSocketPackets::PurchaseGemSocket(lua_State*)
{
	Packet(CMSG_GEM_SOCKET_PURCHASE).Send();
	return 0;
}

int GemSocketPackets::RequestGemSocketCost(lua_State*)
{
	Packet(CMSG_GEM_SOCKET_COST_REQUEST).Send();
	return 0;
}

// Returns: itemId, cost, purchasedCount, canPurchase(bool)
int GemSocketPackets::GetGemSocketCost(lua_State* L)
{
	GemSocketPackets& self = Instance();
	if (!self.m_hasCost)
	{
		FrameScript::PushNil(L);
		return 1;
	}

	FrameScript::PushNumber(L, self.m_costItemId);
	FrameScript::PushNumber(L, self.m_cost);
	FrameScript::PushNumber(L, self.m_purchased);
	FrameScript::PushBoolean(L, self.m_canPurchase != 0);
	return 4;
}

void GemSocketPackets::Apply()
{
	sCustomPacket.RegisterHandler(SMSG_GEM_SOCKET_LIST, &Handler_SMSG_GEM_SOCKET_LIST);
	sCustomPacket.RegisterHandler(SMSG_GEM_SOCKET_ERROR, &Handler_SMSG_GEM_SOCKET_ERROR);
	sCustomPacket.RegisterHandler(SMSG_GEM_SOCKET_COST, &Handler_SMSG_GEM_SOCKET_COST);
	sCustomPacket.RegisterHandler(SMSG_GEM_SOCKET_OPEN, &Handler_SMSG_GEM_SOCKET_OPEN);

	sLua.RegisterFunction("RequestGemSocketList", &RequestGemSocketList, LuaFunctionState::FRAME);
	sLua.RegisterFunction("GetGemSocketCount", &GetGemSocketCount, LuaFunctionState::FRAME);
	sLua.RegisterFunction("GetGemSocketEntryCount", &GetGemSocketEntryCount, LuaFunctionState::FRAME);
	sLua.RegisterFunction("GetGemSocketInfo", &GetGemSocketInfo, LuaFunctionState::FRAME);
	sLua.RegisterFunction("InsertGem", &InsertGem, LuaFunctionState::FRAME);
	sLua.RegisterFunction("RemoveGem", &RemoveGem, LuaFunctionState::FRAME);
	sLua.RegisterFunction("GetActiveFilledCount", &GetActiveFilledCount, LuaFunctionState::FRAME);
	sLua.RegisterFunction("PurchaseGemSocket", &PurchaseGemSocket, LuaFunctionState::FRAME);
	sLua.RegisterFunction("RequestGemSocketCost", &RequestGemSocketCost, LuaFunctionState::FRAME);
	sLua.RegisterFunction("GetGemSocketCost", &GetGemSocketCost, LuaFunctionState::FRAME);
}
