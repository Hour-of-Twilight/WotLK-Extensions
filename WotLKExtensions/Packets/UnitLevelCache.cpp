#include <Defines.h>
#include "UnitLevelCache.h"
#include "Packet.h"
#include <CustomPacket.h>
#include <ClientData/ClientFunctions.h>
#include <ClientData/WowObjects.h>
#include <Lua/XMLExtensions.h>

UnitLevelCache& UnitLevelCache::Instance()
{
	static UnitLevelCache instance;
	return instance;
}

bool UnitLevelCache::HasPlayerItemLevel(uint64 guid) const
{
	return m_playerItemLevels.count(guid) != 0;
}

uint32 UnitLevelCache::GetPlayerItemLevel(uint64 guid) const
{
	auto it = m_playerItemLevels.find(guid);
	return it != m_playerItemLevels.end() ? it->second : 0;
}

uint8 UnitLevelCache::GetPlayerSubClass(uint64 guid) const
{
	auto it = m_playerSubClasses.find(guid);
	return it != m_playerSubClasses.end() ? it->second : 0;
}

void UnitLevelCache::SetPlayerItemLevel(uint64 guid, uint32 ilvl, uint8 subClass)
{
	m_playerItemLevels[guid] = ilvl;
	m_playerSubClasses[guid] = subClass;
	ForgetRequest(guid);
}

void UnitLevelCache::ClearPlayers()
{
	m_playerItemLevels.clear();
	m_playerSubClasses.clear();
	ForgetRequests(true);
}

bool UnitLevelCache::HasCreatureDungeonLevel(uint64 guid) const
{
	return m_creatureDungeonLevels.count(guid) != 0;
}

uint32 UnitLevelCache::GetCreatureDungeonLevel(uint64 guid) const
{
	auto it = m_creatureDungeonLevels.find(guid);
	return it != m_creatureDungeonLevels.end() ? it->second : 0;
}

void UnitLevelCache::SetCreatureDungeonLevel(uint64 guid, uint32 level)
{
	m_creatureDungeonLevels[guid] = level;
	ForgetRequest(guid);
}

void UnitLevelCache::ClearCreatures()
{
	m_creatureDungeonLevels.clear();
	ForgetRequests(false);
}

void UnitLevelCache::ClearAll()
{
	m_playerItemLevels.clear();
	m_playerSubClasses.clear();
	m_creatureDungeonLevels.clear();
	m_requests.clear();
}

bool UnitLevelCache::HasUnitItemLevelOrDungeonLevel(uint64 guid) const
{
	return HasPlayerItemLevel(guid) || HasCreatureDungeonLevel(guid);
}

uint32 UnitLevelCache::GetUnitItemLevelOrDungeonLevel(uint64 guid) const
{
	auto playerIt = m_playerItemLevels.find(guid);
	if (playerIt != m_playerItemLevels.end())
		return playerIt->second;

	auto creatureIt = m_creatureDungeonLevels.find(guid);
	if (creatureIt != m_creatureDungeonLevels.end())
		return creatureIt->second;

	return 0;
}

static bool IsPlayerGuid(uint64 guid)
{
	return guid != 0 && (guid >> 48) == 0;
}

bool UnitLevelCache::WantsLevelCache(uint64 guid)
{
	if (guid == 0)
		return false;
	uint16 high = static_cast<uint16>(guid >> 48);
	return high == 0x0000 || high == 0xF130 || high == 0xF140 || high == 0xF150;
}

void UnitLevelCache::StampRequest(uint64 guid)
{
	Request& req = m_requests[guid];
	req.lastMs = static_cast<uint32>(OsGetAsyncTimeMs());
	if (req.attempts < kMaxAttempts)
		++req.attempts;
}

bool UnitLevelCache::ShouldRequest(uint64 guid) const
{
	auto it = m_requests.find(guid);
	if (it == m_requests.end())
		return true;
	if (it->second.attempts >= kMaxAttempts)
		return false;

	uint32 nowMs = static_cast<uint32>(OsGetAsyncTimeMs());
	return (nowMs - it->second.lastMs) >= kRetryIntervalMs;
}

void UnitLevelCache::ForgetRequest(uint64 guid)
{
	m_requests.erase(guid);
}

void UnitLevelCache::ForgetRequests(bool players)
{
	for (auto it = m_requests.begin(); it != m_requests.end();)
	{
		if (IsPlayerGuid(it->first) == players)
			it = m_requests.erase(it);
		else
			++it;
	}
}

void UnitLevelCache::SendRequest(uint64 guid)
{
	if (!WantsLevelCache(guid))
		return;

	sUnitLevelCache.ForgetRequest(guid);
	sUnitLevelCache.StampRequest(guid);
	Packet(CMSG_UNIT_LEVEL_CACHE_REQUEST).PutUInt64(guid).Send();
}

void UnitLevelCache::EnsureRequested(uint64 guid)
{
	UnitLevelCache& self = sUnitLevelCache;
	if (!WantsLevelCache(guid) || self.HasUnitItemLevelOrDungeonLevel(guid) || !self.ShouldRequest(guid))
		return;

	self.StampRequest(guid);
	Packet(CMSG_UNIT_LEVEL_CACHE_REQUEST).PutUInt64(guid).Send();
}

void UnitLevelCache::RefreshUnitDisplays(uint64 guid)
{
	if (ClientData::WowUnit unit = ClientData::WowUnit::FromGuid(guid))
		if (void* namePlate = unit.NamePlate())
			CGNamePlateFrame::UpdateLevelDisplay(namePlate, unit.Raw());

	int tokenCount = 0;
	if (const char** tokens = Script_GetTokensFromGUID(&guid, &tokenCount))
		for (int i = 0; i < tokenCount; ++i)
			FrameXMLExtensions::SignalEvent("UNIT_LEVEL", "%s", tokens[i]);

	static const char* const kViewTokens[] = { "mouseover", "target", "focus", "targettarget", "focustarget" };
	bool onScreen = false;
	for (const char* token : kViewTokens)
	{
		uint64 tokenGuid = 0;
		Script_GetGUIDFromToken(token, &tokenGuid, 0);
		if (tokenGuid != guid)
			continue;

		onScreen = true;
		if (strcmp(token, "mouseover") != 0)
			FrameXMLExtensions::SignalEvent("UNIT_LEVEL", "%s", token);
	}

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
	ClientData::WowUnit cgUnit(unit);
	uint64 guid = cgUnit.Guid();
	uint32 dlvl = sUnitLevelCache.GetUnitItemLevelOrDungeonLevel(guid);
	if (dlvl)
		return static_cast<int>(dlvl);

	EnsureRequested(guid);
	return static_cast<int>(cgUnit.Level());
}

void UnitLevelCache::Handler_SMSG_UNIT_LEVEL_CACHE_RESPONSE(void*, uint32, uint32, CDataStore* pkt)
{
	Packet r(pkt);
	uint64 guid = r.GetUInt64();
	uint32 value = r.GetUInt32();
	if (guid == 0 || value == 0) // not a scaled unit
		return;

	if (IsPlayerGuid(guid))
	{
		uint8 subClass = r.GetUInt8();
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
