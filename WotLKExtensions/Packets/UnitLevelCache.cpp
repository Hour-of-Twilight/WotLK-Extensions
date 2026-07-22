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

void UnitLevelCache::ApplyPlayerItemLevel(CGUnit* unit, uint32_t ilvl)
{
	if (!unit)
		return;

	void* namePlate = *reinterpret_cast<void**>(reinterpret_cast<uint8_t*>(unit) + 0xC38);
	if (namePlate)
		CGNamePlateFrame::UpdateLevelDisplay(namePlate, unit);

	if (!unit->objectBase.ObjectData)
		return;

	uint64_t guid = unit->objectBase.ObjectData->OBJECT_FIELD_GUID;
	const char* token = Script_GetTokenFromGUID(guid);
	if (token)
		FrameScript::SignalEvent(FrameXMLExtensions::GetEventIdByName("UNIT_LEVEL"), "%s", token);
}

void UnitLevelCache::ApplyCreatureDungeonLevel(CGUnit* unit, uint32_t level)
{
	if (!unit)
		return;

	void* namePlate = *reinterpret_cast<void**>(reinterpret_cast<uint8_t*>(unit) + 0xC38);
	if (namePlate)
		CGNamePlateFrame::UpdateLevelDisplay(namePlate, unit);
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
	if (value == 0) // not a scaled unit
		return;

	if (IsPlayerGuid(guid))
	{
		uint8_t subClass = r.GetUInt8();
		sUnitLevelCache.SetPlayerItemLevel(guid, value, subClass);
		Util::DebugOutput("UnitLevelCache: GUID %016llX  type=player  ilvl=%u  subClass=%u",
		    (unsigned long long)guid, value, subClass);
		if (guid == ClntObjMgr::GetActivePlayer())
			FrameScript::SignalEvent(FrameXMLExtensions::GetEventIdByName("HOT_PLAYER_ITEM_LEVEL"), "%u", value);
		// else if (CGUnit* unit = static_cast<CGUnit*>(ClntObjMgr::ObjectPtr(guid, TYPEMASK_UNIT)))
		//	ApplyPlayerItemLevel(unit, value);
	}
	else
	{
		sUnitLevelCache.SetCreatureDungeonLevel(guid, value);
		Util::DebugOutput("UnitLevelCache: GUID %016llX  type=creature  dlvl=%u", (unsigned long long)guid, value);
		// if (CGUnit* unit = static_cast<CGUnit*>(ClntObjMgr::ObjectPtr(guid, TYPEMASK_UNIT)))
		// ApplyCreatureDungeonLevel(unit, value);
	}
}

void UnitLevelCache::Apply()
{
	sCustomPacket.RegisterHandler(SMSG_UNIT_LEVEL_CACHE_RESPONSE, &Handler_SMSG_UNIT_LEVEL_CACHE_RESPONSE);
}
