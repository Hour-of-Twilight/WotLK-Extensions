#pragma once
#include <SharedDefines.h>
#include <unordered_map>

class UnitLevelCache
{
public:
	static UnitLevelCache& Instance();

	bool HasPlayerItemLevel(uint64_t guid) const;
	uint32_t GetPlayerItemLevel(uint64_t guid) const;
	uint8_t GetPlayerSubClass(uint64_t guid) const;
	void SetPlayerItemLevel(uint64_t guid, uint32_t ilvl, uint8_t subClass);
	void ClearPlayers();

	bool HasCreatureDungeonLevel(uint64_t guid) const;
	uint32_t GetCreatureDungeonLevel(uint64_t guid) const;
	void SetCreatureDungeonLevel(uint64_t guid, uint32_t level);
	void ClearCreatures();

	bool HasUnitItemLevelOrDungeonLevel(uint64_t guid) const;
	uint32_t GetUnitItemLevelOrDungeonLevel(uint64_t guid) const;
	void ClearAll();

	static bool WantsLevelCache(uint64_t guid);

	static void SendRequest(uint64_t guid);

	static void EnsureRequested(uint64_t guid);

	static void RefreshUnitDisplays(uint64_t guid);
	void Apply();

	static int __stdcall GetTooltipUnitLevel(void* unit);

	UnitLevelCache(const UnitLevelCache&) = delete;
	UnitLevelCache& operator=(const UnitLevelCache&) = delete;

private:
	UnitLevelCache() = default;

	static const uint32_t kRetryIntervalMs = 2000;
	static const uint8_t kMaxAttempts = 4;

	struct Request
	{
		uint32_t lastMs = 0;
		uint8_t attempts = 0;
	};

	void StampRequest(uint64_t guid);
	bool ShouldRequest(uint64_t guid) const;
	void ForgetRequest(uint64_t guid);
	void ForgetRequests(bool players);

	std::unordered_map<uint64_t, uint32_t> m_playerItemLevels;
	std::unordered_map<uint64_t, uint8_t> m_playerSubClasses;
	std::unordered_map<uint64_t, uint32_t> m_creatureDungeonLevels;
	std::unordered_map<uint64_t, Request> m_requests;

	static void Handler_SMSG_UNIT_LEVEL_CACHE_RESPONSE(void* param, uint32_t opcode, uint32_t a2, CDataStore* pkt);
};

#define sUnitLevelCache UnitLevelCache::Instance()
