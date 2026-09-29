#pragma once

#include <ClientData/QuestLog.h>
#include <Quests/QuestLogPatchTable.h>

#include <cstdint>
#include <unordered_map>
#include <vector>

struct lua_State;

class QuestLogStorage
{
public:
	static QuestLogStorage& Instance();

	void ApplyPatches();

	void ClearSlots();
	void Reset();
	ClientData::QuestLog::Slot* GetSlot(uint32_t index);
	void SetSlot(uint32_t index, const ClientData::QuestLog::Slot& data);
	bool IsOnQuest(uint32_t questId) const;

	void Update(bool rebuild);
	void OnFullLogReceived();
	void OnSlotReceived(uint32_t index, const ClientData::QuestLog::Slot& data);

	void SetPartyQuests(uint64_t guid, std::vector<uint32_t> questIds);
	void ClearPartyQuests();
	int ScriptIsUnitOnQuest(lua_State* L);

	QuestLogStorage(const QuestLogStorage&) = delete;
	QuestLogStorage& operator=(const QuestLogStorage&) = delete;

private:
	QuestLogStorage() = default;

	static const uint32_t kPartyRequestIntervalMs = 3000;
	static const uint32_t kCaveEntrySize = 32;

	bool VerifyBytes(uint32_t address, uint32_t expected) const;
	void PatchReloc(const QuestLogPatchSite& site, uint32_t oldBase, uint32_t newBase);
	void PatchImm8(const QuestLogPatchSite& site);
	void PatchImm32(const QuestLogPatchSite& site);
	void PatchSlotBase(const QuestLogPatchSite& site);
	void PatchLoopCave(const QuestLogPatchSite& site);
	bool ShouldRequestParty(uint64_t guid);
	void PruneWatches();
	void SendMissingPoiQuery(const uint32_t* questIds, uint32_t count);
	void SendQueryTimeIfDue();

	alignas(16) ClientData::QuestLog::Slot m_slots[kQuestLogSlots] = {};
	alignas(16) ClientData::QuestLog::Entry m_entries[kQuestLogEntryCapacity] = {};
	alignas(16) int32_t m_sortTypes[kQuestLogSortTypes] = {};
	alignas(16) ClientData::QuestLog::PoiEntry m_poi[kQuestLogPoiSlots] = {};

	std::unordered_map<uint64_t, std::vector<uint32_t>> m_partyQuests;
	std::unordered_map<uint64_t, uint32_t> m_partyRequestMs;

	uint8_t* m_cave = nullptr;
	uint32_t m_caveUsed = 0;
	uint32_t m_patchFailures = 0;
};

#define sQuestLog QuestLogStorage::Instance()
