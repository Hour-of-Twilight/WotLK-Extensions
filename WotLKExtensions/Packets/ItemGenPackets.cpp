#include "ItemGenPackets.h"
#include "Packet.h"
#include <CustomPacket.h>
#include <CustomLua.h>
#include <XMLExtensions.h>
#include <SharedDefines.h>

ItemGenPackets& ItemGenPackets::Instance()
{
	static ItemGenPackets instance;
	return instance;
}

// SMSG_ITEMGEN_STAT_GROUPS: UInt32 count, then per group Int32 id, CString name, UInt32 mask,
// UInt32 flags, UInt32 weight, Int32 trinketGroup. Then UInt32 maskCount and per bit
// UInt32 bit, CString name.
void ItemGenPackets::Handler_SMSG_ITEMGEN_STAT_GROUPS(void*, uint32_t, uint32_t, CDataStore* pkt)
{
	Packet r(pkt);
	char buffer[128];

	ItemGenPackets& self = Instance();
	self.m_groups.clear();
	self.m_maskBits.clear();

	uint32_t count = r.GetUInt32();
	self.m_groups.reserve(count);
	for (uint32_t i = 0; i < count; ++i)
	{
		StatGroupEntry entry;
		entry.id = r.GetInt32();
		r.GetString(buffer, sizeof(buffer));
		entry.name = buffer;
		entry.mask = r.GetUInt32();
		entry.flags = r.GetUInt32();
		entry.weight = r.GetUInt32();
		entry.trinketGroup = r.GetInt32();
		self.m_groups.push_back(entry);
	}

	uint32_t maskCount = r.GetUInt32();
	self.m_maskBits.reserve(maskCount);
	for (uint32_t i = 0; i < maskCount; ++i)
	{
		StatGroupMaskEntry entry;
		entry.bit = r.GetUInt32();
		r.GetString(buffer, sizeof(buffer));
		entry.name = buffer;
		self.m_maskBits.push_back(entry);
	}

	self.m_hasData = true;
	FrameXMLExtensions::SignalEvent("HOT_STAT_GROUPS", "");
}

void ItemGenPackets::Handler_SMSG_ITEMGEN_LEGENDARIES(void*, uint32_t, uint32_t, CDataStore* pkt)
{
	Packet r(pkt);
	char buffer[256];

	ItemGenPackets& self = Instance();
	self.m_legendaries.clear();

	uint32_t count = r.GetUInt32();
	self.m_legendaries.reserve(count);
	for (uint32_t i = 0; i < count; ++i)
	{
		LegendaryEntry entry;
		entry.id = r.GetUInt32();
		r.GetString(buffer, sizeof(buffer));
		entry.name = buffer;
		r.GetString(buffer, sizeof(buffer));
		entry.nameOverride = buffer;
		entry.flags = r.GetUInt32();
		entry.minItemLevel = r.GetInt32();
		entry.maxItemLevel = r.GetInt32();
		entry.itemClass = r.GetInt32();
		entry.itemSubClass = r.GetInt32();
		entry.inventoryType = r.GetInt32();
		entry.statGroup = r.GetInt32();
		entry.statGroupMask = r.GetUInt32();
		entry.statGroupOverride = r.GetInt32();
		entry.uniqueId = r.GetUInt32();
		self.m_legendaries.push_back(entry);
	}

	self.m_hasLegendaries = true;
	FrameXMLExtensions::SignalEvent("HOT_LEGENDARIES", "");
}

void ItemGenPackets::Handler_SMSG_ITEMGEN_UNIQUES(void*, uint32_t, uint32_t, CDataStore* pkt)
{
	Packet r(pkt);
	char buffer[1024];

	ItemGenPackets& self = Instance();
	self.m_uniques.clear();

	uint32_t count = r.GetUInt32();
	self.m_uniques.reserve(count);
	for (uint32_t i = 0; i < count; ++i)
	{
		UniqueEntry entry;
		entry.id = r.GetUInt32();
		r.GetString(buffer, sizeof(buffer));
		entry.name = buffer;
		r.GetString(buffer, sizeof(buffer));
		entry.description = buffer;
		entry.quality = r.GetUInt32();
		entry.itemClass = r.GetUInt32();
		entry.subClass = r.GetUInt32();
		entry.inventoryType = r.GetUInt32();
		entry.statGroup = r.GetInt32();
		entry.flags = r.GetUInt32();
		entry.itemLevelMin = r.GetUInt32();
		entry.itemLevelMax = r.GetUInt32();
		entry.spawnable = r.GetUInt8() != 0;
		self.m_uniques.push_back(entry);
	}

	self.m_hasUniques = true;
	FrameXMLExtensions::SignalEvent("HOT_UNIQUE_ITEMS", "");
}

int ItemGenPackets::RequestStatGroups(lua_State*)
{
	Packet(CMSG_ITEMGEN_STAT_GROUPS_REQUEST).Send();
	return 0;
}

int ItemGenPackets::GetStatGroupCount(lua_State* L)
{
	ItemGenPackets& self = Instance();
	FrameScript::PushNumber(L, self.m_hasData ? self.m_groups.size() : 0);
	return 1;
}

// Takes a 1 based index. Returns: id, name, mask, flags, weight, trinketGroup
int ItemGenPackets::GetStatGroupInfo(lua_State* L)
{
	ItemGenPackets& self = Instance();
	uint32_t index = static_cast<uint32_t>(FrameScript::GetNumber(L, 1));
	if (!self.m_hasData || index < 1 || index > self.m_groups.size())
	{
		FrameScript::PushNil(L);
		return 1;
	}

	StatGroupEntry const& entry = self.m_groups[index - 1];
	FrameScript::PushNumber(L, entry.id);
	FrameScript::PushString(L, entry.name.c_str());
	FrameScript::PushNumber(L, entry.mask);
	FrameScript::PushNumber(L, entry.flags);
	FrameScript::PushNumber(L, entry.weight);
	FrameScript::PushNumber(L, entry.trinketGroup);
	return 6;
}

int ItemGenPackets::GetStatGroupMaskCount(lua_State* L)
{
	ItemGenPackets& self = Instance();
	FrameScript::PushNumber(L, self.m_hasData ? self.m_maskBits.size() : 0);
	return 1;
}

// Takes a 1 based index. Returns: bit, name
int ItemGenPackets::GetStatGroupMaskInfo(lua_State* L)
{
	ItemGenPackets& self = Instance();
	uint32_t index = static_cast<uint32_t>(FrameScript::GetNumber(L, 1));
	if (!self.m_hasData || index < 1 || index > self.m_maskBits.size())
	{
		FrameScript::PushNil(L);
		return 1;
	}

	StatGroupMaskEntry const& entry = self.m_maskBits[index - 1];
	FrameScript::PushNumber(L, entry.bit);
	FrameScript::PushString(L, entry.name.c_str());
	return 2;
}

int ItemGenPackets::RequestLegendaries(lua_State*)
{
	Packet(CMSG_ITEMGEN_LEGENDARIES_REQUEST).Send();
	return 0;
}

int ItemGenPackets::GetLegendaryCount(lua_State* L)
{
	ItemGenPackets& self = Instance();
	FrameScript::PushNumber(L, self.m_hasLegendaries ? self.m_legendaries.size() : 0);
	return 1;
}

int ItemGenPackets::GetLegendaryInfo(lua_State* L)
{
	ItemGenPackets& self = Instance();
	uint32_t index = static_cast<uint32_t>(FrameScript::GetNumber(L, 1));
	if (!self.m_hasLegendaries || index < 1 || index > self.m_legendaries.size())
	{
		FrameScript::PushNil(L);
		return 1;
	}

	LegendaryEntry const& entry = self.m_legendaries[index - 1];
	FrameScript::PushNumber(L, entry.id);
	FrameScript::PushString(L, entry.name.c_str());
	FrameScript::PushString(L, entry.nameOverride.c_str());
	FrameScript::PushNumber(L, entry.flags);
	FrameScript::PushNumber(L, entry.minItemLevel);
	FrameScript::PushNumber(L, entry.maxItemLevel);
	FrameScript::PushNumber(L, entry.itemClass);
	FrameScript::PushNumber(L, entry.itemSubClass);
	FrameScript::PushNumber(L, entry.inventoryType);
	FrameScript::PushNumber(L, entry.statGroup);
	FrameScript::PushNumber(L, entry.statGroupMask);
	FrameScript::PushNumber(L, entry.statGroupOverride);
	FrameScript::PushNumber(L, entry.uniqueId);
	return 13;
}

int ItemGenPackets::RequestUniqueItems(lua_State*)
{
	Packet(CMSG_ITEMGEN_UNIQUES_REQUEST).Send();
	return 0;
}

int ItemGenPackets::GetUniqueItemCount(lua_State* L)
{
	ItemGenPackets& self = Instance();
	FrameScript::PushNumber(L, self.m_hasUniques ? self.m_uniques.size() : 0);
	return 1;
}

int ItemGenPackets::GetUniqueItemInfo(lua_State* L)
{
	ItemGenPackets& self = Instance();
	uint32_t index = static_cast<uint32_t>(FrameScript::GetNumber(L, 1));
	if (!self.m_hasUniques || index < 1 || index > self.m_uniques.size())
	{
		FrameScript::PushNil(L);
		return 1;
	}

	UniqueEntry const& entry = self.m_uniques[index - 1];
	FrameScript::PushNumber(L, entry.id);
	FrameScript::PushString(L, entry.name.c_str());
	FrameScript::PushString(L, entry.description.c_str());
	FrameScript::PushNumber(L, entry.quality);
	FrameScript::PushNumber(L, entry.itemClass);
	FrameScript::PushNumber(L, entry.subClass);
	FrameScript::PushNumber(L, entry.inventoryType);
	FrameScript::PushNumber(L, entry.statGroup);
	FrameScript::PushNumber(L, entry.flags);
	FrameScript::PushNumber(L, entry.itemLevelMin);
	FrameScript::PushNumber(L, entry.itemLevelMax);
	FrameScript::PushBoolean(L, entry.spawnable ? 1 : 0);
	return 12;
}

void ItemGenPackets::Apply()
{
	sCustomPacket.RegisterHandler(SMSG_ITEMGEN_STAT_GROUPS, &Handler_SMSG_ITEMGEN_STAT_GROUPS);
	sCustomPacket.RegisterHandler(SMSG_ITEMGEN_LEGENDARIES, &Handler_SMSG_ITEMGEN_LEGENDARIES);
	sCustomPacket.RegisterHandler(SMSG_ITEMGEN_UNIQUES, &Handler_SMSG_ITEMGEN_UNIQUES);

	sLua.RegisterFunction("RequestStatGroups", &RequestStatGroups, LuaFunctionState::FRAME);
	sLua.RegisterFunction("GetStatGroupCount", &GetStatGroupCount, LuaFunctionState::FRAME);
	sLua.RegisterFunction("GetStatGroupInfo", &GetStatGroupInfo, LuaFunctionState::FRAME);
	sLua.RegisterFunction("GetStatGroupMaskCount", &GetStatGroupMaskCount, LuaFunctionState::FRAME);
	sLua.RegisterFunction("GetStatGroupMaskInfo", &GetStatGroupMaskInfo, LuaFunctionState::FRAME);
	sLua.RegisterFunction("RequestLegendaries", &RequestLegendaries, LuaFunctionState::FRAME);
	sLua.RegisterFunction("GetLegendaryCount", &GetLegendaryCount, LuaFunctionState::FRAME);
	sLua.RegisterFunction("GetLegendaryInfo", &GetLegendaryInfo, LuaFunctionState::FRAME);
	sLua.RegisterFunction("RequestUniqueItems", &RequestUniqueItems, LuaFunctionState::FRAME);
	sLua.RegisterFunction("GetUniqueItemCount", &GetUniqueItemCount, LuaFunctionState::FRAME);
	sLua.RegisterFunction("GetUniqueItemInfo", &GetUniqueItemInfo, LuaFunctionState::FRAME);
}
