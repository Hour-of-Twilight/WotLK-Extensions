#include "GemSocketPackets.h"
#include "Packet.h"
#include <CustomPacket.h>
#include <CustomLua.h>
#include <ClientData/ClientFunctions.h>
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
	FrameXMLExtensions::SignalEvent("HOT_GEM_SOCKET_UPDATE", "");
}

// SMSG_GEM_SOCKET_ERROR: UInt8 errorCode.
void GemSocketPackets::Handler_SMSG_GEM_SOCKET_ERROR(void*, uint32_t, uint32_t, CDataStore* pkt)
{
	int8_t code = Packet(pkt).GetInt8();
	FrameXMLExtensions::SignalEvent("HOT_GEM_SOCKET_ERROR", "%d", (int)code);
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
	FrameXMLExtensions::SignalEvent("HOT_GEM_SOCKET_COST_UPDATE", "");
}

// SMSG_GEM_SOCKET_OPEN: UInt8 mode.
//
// The packet used to carry nothing and always opened Jewelcrafting. A short read leaves the
// mode at zero, so a server that has not been updated still gets exactly that and only a
// server that asks for it gets the panel.
void GemSocketPackets::Handler_SMSG_GEM_SOCKET_OPEN(void*, uint32_t, uint32_t, CDataStore* pkt)
{
	Packet r(pkt);
	uint8_t mode = r.GetUInt8();

	if (mode == GEM_SOCKET_OPEN_PANEL)
	{
		// GemSocketUI.lua listens for this and shows the panel.
		FrameXMLExtensions::SignalEvent("HOT_GEM_SOCKET_OPEN", "");
		return;
	}

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

// SMSG_GEM_LOADOUTS: UInt8 active, UInt8 pending, UInt8 max, UInt32 unlockItemId,
// UInt32 unlockCost, UInt8 canUnlock, UInt8 count, then per loadout
// UInt8 id, String name, UInt8 sockets, UInt8 filled, UInt8 purchased.
void GemSocketPackets::Handler_SMSG_GEM_LOADOUTS(void*, uint32_t, uint32_t, CDataStore* pkt)
{
	Packet r(pkt);
	GemSocketPackets& self = Instance();
	self.m_activeLoadout = r.GetUInt8();
	self.m_pendingLoadout = r.GetUInt8();
	self.m_maxLoadouts = r.GetUInt8();
	self.m_unlockItemId = r.GetUInt32();
	self.m_unlockCost = r.GetUInt32();
	self.m_canUnlock = r.GetUInt8();

	uint8_t count = r.GetUInt8();
	self.m_loadouts.clear();
	self.m_loadouts.reserve(count);

	for (uint8_t i = 0; i < count; ++i)
	{
		GemLoadoutEntry entry;
		entry.id = r.GetUInt8();
		char name[128] = {};
		r.GetString(name, sizeof(name));
		entry.name = name;
		entry.sockets = r.GetUInt8();
		entry.filled = r.GetUInt8();
		entry.purchased = r.GetUInt8();
		self.m_loadouts.push_back(std::move(entry));
	}

	self.m_hasLoadouts = true;
	FrameXMLExtensions::SignalEvent("HOT_GEM_LOADOUTS", "%u", (uint32_t)self.m_activeLoadout);
}

// SMSG_GEM_LOADOUT_RESULT: UInt8 op, UInt8 result, UInt8 loadoutId.
void GemSocketPackets::Handler_SMSG_GEM_LOADOUT_RESULT(void*, uint32_t, uint32_t, CDataStore* pkt)
{
	Packet r(pkt);
	uint8_t op = r.GetUInt8();
	uint8_t result = r.GetUInt8();
	uint8_t loadoutId = r.GetUInt8();
	FrameXMLExtensions::SignalEvent("HOT_GEM_LOADOUT_RESULT", "%u%u%u",
	    (uint32_t)op, (uint32_t)result, (uint32_t)loadoutId);
}

int GemSocketPackets::RequestGemLoadouts(lua_State*)
{
	Packet(CMSG_GEM_LOADOUTS_REQUEST).Send();
	return 0;
}

// Returns an array of { id, name, sockets, filled, purchased }, ordered by id.
int GemSocketPackets::GetGemLoadouts(lua_State* L)
{
	auto const& loadouts = Instance().m_loadouts;

	FrameScript::CreateTable(L, (int)loadouts.size(), 0);
	int tbl = FrameScript::GetTop(L);

	for (int i = 0; i < (int)loadouts.size(); ++i)
	{
		GemLoadoutEntry const& entry = loadouts[i];

		FrameScript::CreateTable(L, 0, 5);
		int row = FrameScript::GetTop(L);

		FrameScript::PushNumber(L, entry.id);
		FrameScript::SetField(L, row, "id");

		FrameScript::PushString(L, entry.name.c_str());
		FrameScript::SetField(L, row, "name");

		FrameScript::PushNumber(L, entry.sockets);
		FrameScript::SetField(L, row, "sockets");

		FrameScript::PushNumber(L, entry.filled);
		FrameScript::SetField(L, row, "filled");

		FrameScript::PushNumber(L, entry.purchased);
		FrameScript::SetField(L, row, "purchased");

		FrameScript::RawSetI(L, tbl, i + 1);
	}

	FrameScript::SetTop(L, tbl);
	return 1;
}

// Returns: activeId, pendingId (nil when no switch is being cast)
int GemSocketPackets::GetActiveGemLoadout(lua_State* L)
{
	GemSocketPackets& self = Instance();
	FrameScript::PushNumber(L, self.m_activeLoadout);
	if (self.m_pendingLoadout == 0xFF)
		FrameScript::PushNil(L);
	else
		FrameScript::PushNumber(L, self.m_pendingLoadout);
	return 2;
}

int GemSocketPackets::GetMaxGemLoadouts(lua_State* L)
{
	FrameScript::PushNumber(L, Instance().m_maxLoadouts);
	return 1;
}

// Returns: itemId, cost, canUnlock(bool)
int GemSocketPackets::GetGemLoadoutUnlockCost(lua_State* L)
{
	GemSocketPackets& self = Instance();
	if (!self.m_hasLoadouts)
	{
		FrameScript::PushNil(L);
		return 1;
	}

	FrameScript::PushNumber(L, self.m_unlockItemId);
	FrameScript::PushNumber(L, self.m_unlockCost);
	FrameScript::PushBoolean(L, self.m_canUnlock != 0);
	return 3;
}

int GemSocketPackets::SwitchGemLoadout(lua_State* L)
{
	uint8_t id = (uint8_t)FrameScript::GetNumber(L, 1);
	Packet(CMSG_GEM_LOADOUT_SWITCH).PutUInt8(id).Send();
	return 0;
}

int GemSocketPackets::UnlockGemLoadout(lua_State* L)
{
	// An empty name is allowed: the server names it after its position.
	char* name = FrameScript::ToLString(L, 1, false);
	Packet(CMSG_GEM_LOADOUT_UNLOCK).PutString(name ? name : "").Send();
	return 0;
}

int GemSocketPackets::RenameGemLoadout(lua_State* L)
{
	uint8_t id = (uint8_t)FrameScript::GetNumber(L, 1);
	char* name = FrameScript::ToLString(L, 2, false);
	if (!name || name[0] == '\0')
		return 0;

	Packet(CMSG_GEM_LOADOUT_RENAME).PutUInt8(id).PutString(name).Send();
	return 0;
}

void GemSocketPackets::Apply()
{
	sCustomPacket.RegisterHandler(SMSG_GEM_SOCKET_LIST, &Handler_SMSG_GEM_SOCKET_LIST);
	sCustomPacket.RegisterHandler(SMSG_GEM_SOCKET_ERROR, &Handler_SMSG_GEM_SOCKET_ERROR);
	sCustomPacket.RegisterHandler(SMSG_GEM_SOCKET_COST, &Handler_SMSG_GEM_SOCKET_COST);
	sCustomPacket.RegisterHandler(SMSG_GEM_SOCKET_OPEN, &Handler_SMSG_GEM_SOCKET_OPEN);
	sCustomPacket.RegisterHandler(SMSG_GEM_LOADOUTS, &Handler_SMSG_GEM_LOADOUTS);
	sCustomPacket.RegisterHandler(SMSG_GEM_LOADOUT_RESULT, &Handler_SMSG_GEM_LOADOUT_RESULT);

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
	sLua.RegisterFunction("RequestGemLoadouts", &RequestGemLoadouts, LuaFunctionState::FRAME);
	sLua.RegisterFunction("GetGemLoadouts", &GetGemLoadouts, LuaFunctionState::FRAME);
	sLua.RegisterFunction("GetActiveGemLoadout", &GetActiveGemLoadout, LuaFunctionState::FRAME);
	sLua.RegisterFunction("GetMaxGemLoadouts", &GetMaxGemLoadouts, LuaFunctionState::FRAME);
	sLua.RegisterFunction("GetGemLoadoutUnlockCost", &GetGemLoadoutUnlockCost, LuaFunctionState::FRAME);
	sLua.RegisterFunction("SwitchGemLoadout", &SwitchGemLoadout, LuaFunctionState::FRAME);
	sLua.RegisterFunction("UnlockGemLoadout", &UnlockGemLoadout, LuaFunctionState::FRAME);
	sLua.RegisterFunction("RenameGemLoadout", &RenameGemLoadout, LuaFunctionState::FRAME);
}
