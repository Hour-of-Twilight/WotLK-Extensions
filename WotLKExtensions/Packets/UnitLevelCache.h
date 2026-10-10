#pragma once

#include <Defines.h>
#include <SharedDefines.h>
#include <unordered_map>

class UnitLevelCache
{
public:
	static UnitLevelCache& Instance();

	bool HasPlayerItemLevel(uint64 guid) const;
	uint32 GetPlayerItemLevel(uint64 guid) const;
	uint8 GetPlayerSubClass(uint64 guid) const;
	void SetPlayerItemLevel(uint64 guid, uint32 ilvl, uint8 subClass);
	void ClearPlayers();

	bool HasCreatureDungeonLevel(uint64 guid) const;
	uint32 GetCreatureDungeonLevel(uint64 guid) const;
	void SetCreatureDungeonLevel(uint64 guid, uint32 level);
	void ClearCreatures();

	bool HasUnitItemLevelOrDungeonLevel(uint64 guid) const;
	uint32 GetUnitItemLevelOrDungeonLevel(uint64 guid) const;
	void ClearAll();

	static bool WantsLevelCache(uint64 guid);

	static void SendRequest(uint64 guid);

	static void EnsureRequested(uint64 guid);

	static void RefreshUnitDisplays(uint64 guid);
	void Apply();

	static int __stdcall GetTooltipUnitLevel(void* unit);

	UnitLevelCache(const UnitLevelCache&) = delete;
	UnitLevelCache& operator=(const UnitLevelCache&) = delete;

private:
	UnitLevelCache() = default;

	static const uint32 kRetryIntervalMs = 2000;
	static const uint8 kMaxAttempts = 4;

	struct Request
	{
		uint32 lastMs = 0;
		uint8 attempts = 0;
	};

	void StampRequest(uint64 guid);
	bool ShouldRequest(uint64 guid) const;
	void ForgetRequest(uint64 guid);
	void ForgetRequests(bool players);

	std::unordered_map<uint64, uint32> m_playerItemLevels;
	std::unordered_map<uint64, uint8> m_playerSubClasses;
	std::unordered_map<uint64, uint32> m_creatureDungeonLevels;
	std::unordered_map<uint64, Request> m_requests;

	static void Handler_SMSG_UNIT_LEVEL_CACHE_RESPONSE(void* param, uint32 opcode, uint32 a2, CDataStore* pkt);
};

#define sUnitLevelCache UnitLevelCache::Instance()
