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
	FrameScript::SignalEvent(FrameXMLExtensions::GetEventIdByName("HOT_STAT_GROUPS"), "");
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

void ItemGenPackets::Apply()
{
	sCustomPacket.RegisterHandler(SMSG_ITEMGEN_STAT_GROUPS, &Handler_SMSG_ITEMGEN_STAT_GROUPS);

	sLua.RegisterFunction("RequestStatGroups", &RequestStatGroups, LuaFunctionState::FRAME);
	sLua.RegisterFunction("GetStatGroupCount", &GetStatGroupCount, LuaFunctionState::FRAME);
	sLua.RegisterFunction("GetStatGroupInfo", &GetStatGroupInfo, LuaFunctionState::FRAME);
	sLua.RegisterFunction("GetStatGroupMaskCount", &GetStatGroupMaskCount, LuaFunctionState::FRAME);
	sLua.RegisterFunction("GetStatGroupMaskInfo", &GetStatGroupMaskInfo, LuaFunctionState::FRAME);
}
