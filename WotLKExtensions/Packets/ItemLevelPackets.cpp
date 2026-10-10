#include <Defines.h>
#include "ItemLevelPackets.h"
#include "Packet.h"
#include <CustomPacket.h>
#include <CustomLua.h>
#include <XMLExtensions.h>
#include <SharedDefines.h>

ItemLevelPackets& ItemLevelPackets::Instance()
{
	static ItemLevelPackets instance;
	return instance;
}

void ItemLevelPackets::Handler_SMSG_ITEM_LEVEL_BREAKDOWN(void*, uint32, uint32, CDataStore* pkt)
{
	Packet r(pkt);
	uint8 subClass = r.GetUInt8();
	uint8 count = r.GetUInt8();

	ItemLevelPackets& self = Instance();
	self.m_subClass = subClass;
	self.m_slotLevels.clear();
	self.m_slotLevels.reserve(count);

	for (uint8 i = 0; i < count; ++i)
	{
		uint8 slot = r.GetUInt8();
		uint32 level = r.GetUInt32();
		self.m_slotLevels[slot] = level;
	}
	self.m_averageItemLevel = r.GetUInt32();

	self.m_hasData = true;
	FrameXMLExtensions::SignalEvent("HOT_ITEM_LEVEL_UPDATE", "");
}

int ItemLevelPackets::RequestItemLevelBreakdown(lua_State*)
{
	Packet(CMSG_ITEM_LEVEL_BREAKDOWN_REQUEST).Send();
	return 0;
}

int ItemLevelPackets::HasItemLevelBreakdown(lua_State* L)
{
	FrameScript::PushBoolean(L, Instance().m_hasData);
	return 1;
}

int ItemLevelPackets::GetItemLevelSubClass(lua_State* L)
{
	ItemLevelPackets& self = Instance();
	FrameScript::PushNumber(L, self.m_hasData ? self.m_subClass : 0);
	return 1;
}

int ItemLevelPackets::GetItemLevelBreakdownSlot(lua_State* L)
{
	ItemLevelPackets& self = Instance();
	if (!self.m_hasData)
	{
		FrameScript::PushNil(L);
		return 1;
	}

	uint8 slot = static_cast<uint8>(FrameScript::GetNumber(L, 1));
	auto it = self.m_slotLevels.find(slot);
	if (it == self.m_slotLevels.end())
	{
		FrameScript::PushNil(L);
		return 1;
	}

	FrameScript::PushNumber(L, it->second);
	return 1;
}

int ItemLevelPackets::GetItemLevelBreakdownAverage(lua_State* L)
{
	ItemLevelPackets& self = Instance();
	if (!self.m_hasData)
	{
		FrameScript::PushNil(L);
		return 1;
	}

	FrameScript::PushNumber(L, self.m_averageItemLevel);
	return 1;
}

void ItemLevelPackets::Clear()
{
	m_slotLevels.clear();
	m_averageItemLevel = 0;
	m_subClass = 0;
	m_hasData = false;
}

void ItemLevelPackets::Apply()
{
	sCustomPacket.RegisterHandler(SMSG_ITEM_LEVEL_BREAKDOWN, &Handler_SMSG_ITEM_LEVEL_BREAKDOWN);

	sLua.RegisterFunction("RequestItemLevelBreakdown", &RequestItemLevelBreakdown, LuaFunctionState::FRAME);
	sLua.RegisterFunction("HasItemLevelBreakdown", &HasItemLevelBreakdown, LuaFunctionState::FRAME);
	sLua.RegisterFunction("GetItemLevelSubClass", &GetItemLevelSubClass, LuaFunctionState::FRAME);
	sLua.RegisterFunction("GetItemLevelBreakdownSlot", &GetItemLevelBreakdownSlot, LuaFunctionState::FRAME);
	sLua.RegisterFunction("GetItemLevelBreakdownAverage", &GetItemLevelBreakdownAverage, LuaFunctionState::FRAME);
}
