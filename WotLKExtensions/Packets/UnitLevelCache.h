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

	static void SendRequest(uint64_t guid);
	static void ApplyCreatureDungeonLevel(CGUnit* unit, uint32_t level);
	static void ApplyPlayerItemLevel(CGUnit* unit, uint32_t ilvl);
	void Apply();

	// Called from CGTooltip__SetUnit via raw patch (push ebx; call).
	// Returns cached dungeon level if present, fires a request if not, then falls back to unitData->level.
	static int __stdcall GetTooltipUnitLevel(void* unit);

	UnitLevelCache(const UnitLevelCache&) = delete;
	UnitLevelCache& operator=(const UnitLevelCache&) = delete;

private:
	UnitLevelCache() = default;

	std::unordered_map<uint64_t, uint32_t> m_playerItemLevels;
	std::unordered_map<uint64_t, uint8_t> m_playerSubClasses;
	std::unordered_map<uint64_t, uint32_t> m_creatureDungeonLevels;

	// SMSG_UNIT_LEVEL_CACHE_RESPONSE: Int64 guid, UInt32 value, UInt8 subClass (player only).
	// Player vs creature is derived from the GUID, not a wire flag.
	static void Handler_SMSG_UNIT_LEVEL_CACHE_RESPONSE(void* param, uint32_t opcode, uint32_t a2, CDataStore* pkt);
};

#define sUnitLevelCache UnitLevelCache::Instance()
