#include "UnitLevelCache.h"
#include "Packet.h"
#include <CustomPacket.h>
#include <ClientData/ClientFunctions.h>
#include <Lua/XMLExtensions.h>

UnitLevelCache& UnitLevelCache::Instance()
{
	static UnitLevelCache instance;
	return instance;
}

bool UnitLevelCache::HasPlayerItemLevel(uint64_t guid) const
{
	return m_playerItemLevels.count(guid) != 0;
}

uint32_t UnitLevelCache::GetPlayerItemLevel(uint64_t guid) const
{
	auto it = m_playerItemLevels.find(guid);
	return it != m_playerItemLevels.end() ? it->second : 0;
}

uint8_t UnitLevelCache::GetPlayerSubClass(uint64_t guid) const
{
	auto it = m_playerSubClasses.find(guid);
	return it != m_playerSubClasses.end() ? it->second : 0;
}

void UnitLevelCache::SetPlayerItemLevel(uint64_t guid, uint32_t ilvl, uint8_t subClass)
{
	m_playerItemLevels[guid] = ilvl;
	m_playerSubClasses[guid] = subClass;
}

void UnitLevelCache::ClearPlayers()
{
	m_playerItemLevels.clear();
	m_playerSubClasses.clear();
}

bool UnitLevelCache::HasCreatureDungeonLevel(uint64_t guid) const
{
	return m_creatureDungeonLevels.count(guid) != 0;
}

uint32_t UnitLevelCache::GetCreatureDungeonLevel(uint64_t guid) const
{
	auto it = m_creatureDungeonLevels.find(guid);
	return it != m_creatureDungeonLevels.end() ? it->second : 0;
}

void UnitLevelCache::SetCreatureDungeonLevel(uint64_t guid, uint32_t level)
{
	m_creatureDungeonLevels[guid] = level;
}

void UnitLevelCache::ClearCreatures()
{
	m_creatureDungeonLevels.clear();
}

void UnitLevelCache::ClearAll()
{
	m_playerItemLevels.clear();
	m_playerSubClasses.clear();
	m_creatureDungeonLevels.clear();
}

bool UnitLevelCache::HasUnitItemLevelOrDungeonLevel(uint64_t guid) const
{
	return HasPlayerItemLevel(guid) || HasCreatureDungeonLevel(guid);
}

uint32_t UnitLevelCache::GetUnitItemLevelOrDungeonLevel(uint64_t guid) const
{
	auto playerIt = m_playerItemLevels.find(guid);
	if (playerIt != m_playerItemLevels.end())
		return playerIt->second;

	auto creatureIt = m_creatureDungeonLevels.find(guid);
	if (creatureIt != m_creatureDungeonLevels.end())
		return creatureIt->second;

	return 0;
}

// The reply usually lands after the unit was first drawn (it's requested when the unit comes into
// view, which is also when its nameplate appears and when it's often targeted or hovered), and
// nothing redraws those on its own, so the raw level stayed until it was drawn again
void UnitLevelCache::RefreshUnitDisplays(uint64_t guid)
{
	// Same null-checked call the client makes itself (0x72E41A)
	if (CGUnit* unit = static_cast<CGUnit*>(ClntObjMgr::ObjectPtr(guid, TYPEMASK_UNIT)))
		if (void* namePlate = *reinterpret_cast<void**>(reinterpret_cast<uint8_t*>(unit) + 0xC38))
			CGNamePlateFrame::UpdateLevelDisplay(namePlate, unit);

	static const char* const kTokens[] = { "mouseover", "target", "focus", "targettarget", "focustarget" };
	bool onScreen = false;
	for (const char* token : kTokens)
	{
		uint64_t tokenGuid = 0;
		Script_GetGUIDFromToken(token, &tokenGuid, 0);
		if (tokenGuid != guid)
			continue;

		onScreen = true;
		if (strcmp(token, "mouseover") != 0)
			FrameXMLExtensions::SignalEvent("UNIT_LEVEL", "%s", token);
	}

	// Tooltips don't listen for UNIT_LEVEL, so fill the unit's tooltip again if it's showing
	if (onScreen)
	{
		char script[192];
		sprintf_s(script, "if GameTooltip:IsShown() then local _,u=GameTooltip:GetUnit() if u and UnitGUID(u)==\"0x%016llX\" then GameTooltip:SetUnit(u) end end",
		    (unsigned long long)guid);
		FrameScript::Execute(script, "UnitLevelCache", 0);
	}
}

int __stdcall UnitLevelCache::GetTooltipUnitLevel(void* unit)
{
	CGUnit* cgUnit = static_cast<CGUnit*>(unit);
	uint64_t guid = cgUnit->objectBase.ObjectData->OBJECT_FIELD_GUID;
	uint32_t dlvl = sUnitLevelCache.GetUnitItemLevelOrDungeonLevel(guid);
	if (dlvl)
		return static_cast<int>(dlvl);
	return cgUnit->unitData ? static_cast<int>(cgUnit->unitData->level) : 0;
}

void UnitLevelCache::SendRequest(uint64_t guid)
{
	Packet(CMSG_UNIT_LEVEL_CACHE_REQUEST).PutUInt64(guid).Send();
}

static bool IsPlayerGuid(uint64_t guid)
{
	return guid != 0 && (guid >> 48) == 0;
}

void UnitLevelCache::Handler_SMSG_UNIT_LEVEL_CACHE_RESPONSE(void*, uint32_t, uint32_t, CDataStore* pkt)
{
	Packet r(pkt);
	uint64_t guid = r.GetUInt64();
	uint32_t value = r.GetUInt32();
	if (guid == 0 || value == 0) // not a scaled unit
		return;

	if (IsPlayerGuid(guid))
	{
		uint8_t subClass = r.GetUInt8();
		sUnitLevelCache.SetPlayerItemLevel(guid, value, subClass);
		Util::DebugOutput("UnitLevelCache: GUID %016llX  type=player  ilvl=%u  subClass=%u",
		    (unsigned long long)guid, value, subClass);
		if (guid == ClntObjMgr::GetActivePlayer())
			FrameXMLExtensions::SignalEvent("HOT_PLAYER_ITEM_LEVEL", "%u", value);
	}
	else
	{
		sUnitLevelCache.SetCreatureDungeonLevel(guid, value);
		Util::DebugOutput("UnitLevelCache: GUID %016llX  type=creature  dlvl=%u", (unsigned long long)guid, value);
	}

	RefreshUnitDisplays(guid);
}

void UnitLevelCache::Apply()
{
	sCustomPacket.RegisterHandler(SMSG_UNIT_LEVEL_CACHE_RESPONSE, &Handler_SMSG_UNIT_LEVEL_CACHE_RESPONSE);
}
