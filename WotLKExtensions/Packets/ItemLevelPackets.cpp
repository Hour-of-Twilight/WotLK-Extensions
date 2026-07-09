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

void ItemLevelPackets::Handler_SMSG_ITEM_LEVEL_BREAKDOWN(void*, uint32_t, uint32_t, CDataStore* pkt)
{
	Packet r(pkt);
	uint8_t subClass = r.GetUInt8();
	uint8_t count = r.GetUInt8();

	ItemLevelPackets& self = Instance();
	self.m_subClass = subClass;
	self.m_slotLevels.clear();
	self.m_slotLevels.reserve(count);

	for (uint8_t i = 0; i < count; ++i)
	{
		uint8_t slot = r.GetUInt8();
		uint32_t level = r.GetUInt32();
		self.m_slotLevels[slot] = level;
	}

	self.m_hasData = true;
	FrameScript::SignalEvent(FrameXMLExtensions::GetEventIdByName("HOT_ITEM_LEVEL_UPDATE"), "");
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

	uint8_t slot = static_cast<uint8_t>(FrameScript::GetNumber(L, 1));
	auto it = self.m_slotLevels.find(slot);
	if (it == self.m_slotLevels.end())
	{
		FrameScript::PushNil(L);
		return 1;
	}

	FrameScript::PushNumber(L, it->second);
	return 1;
}

void ItemLevelPackets::Apply()
{
	sCustomPacket.RegisterHandler(SMSG_ITEM_LEVEL_BREAKDOWN, &Handler_SMSG_ITEM_LEVEL_BREAKDOWN);

	sLua.RegisterFunction("RequestItemLevelBreakdown", &RequestItemLevelBreakdown, LuaFunctionState::FRAME);
	sLua.RegisterFunction("HasItemLevelBreakdown", &HasItemLevelBreakdown, LuaFunctionState::FRAME);
	sLua.RegisterFunction("GetItemLevelSubClass", &GetItemLevelSubClass, LuaFunctionState::FRAME);
	sLua.RegisterFunction("GetItemLevelBreakdownSlot", &GetItemLevelBreakdownSlot, LuaFunctionState::FRAME);
}
