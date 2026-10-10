#pragma once

#include <Defines.h>

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
	ClientData::QuestLog::Slot* GetSlot(uint32 index);
	void SetSlot(uint32 index, const ClientData::QuestLog::Slot& data);
	bool IsOnQuest(uint32 questId) const;

	void Update(bool rebuild);
	void OnFullLogReceived();
	void OnSlotReceived(uint32 index, const ClientData::QuestLog::Slot& data);

	void SetPartyQuests(uint64 guid, std::vector<uint32> questIds);
	void ClearPartyQuests();
	int ScriptIsUnitOnQuest(lua_State* L);

	QuestLogStorage(const QuestLogStorage&) = delete;
	QuestLogStorage& operator=(const QuestLogStorage&) = delete;

private:
	QuestLogStorage() = default;

	static const uint32 kPartyRequestIntervalMs = 3000;
	static const uint32 kCaveEntrySize = 32;

	bool VerifyBytes(uint32 address, uint32 expected) const;
	void PatchReloc(const QuestLogPatchSite& site, uint32 oldBase, uint32 newBase);
	void PatchImm8(const QuestLogPatchSite& site);
	void PatchImm32(const QuestLogPatchSite& site);
	void PatchSlotBase(const QuestLogPatchSite& site);
	void PatchLoopCave(const QuestLogPatchSite& site);
	bool ShouldRequestParty(uint64 guid);
	void PruneWatches();
	void SendMissingPoiQuery(const uint32* questIds, uint32 count);
	void SendQueryTimeIfDue();

	alignas(16) ClientData::QuestLog::Slot m_slots[kQuestLogSlots] = {};
	alignas(16) ClientData::QuestLog::Entry m_entries[kQuestLogEntryCapacity] = {};
	alignas(16) int32 m_sortTypes[kQuestLogSortTypes] = {};
	alignas(16) ClientData::QuestLog::PoiEntry m_poi[kQuestLogPoiSlots] = {};

	std::unordered_map<uint64, std::vector<uint32>> m_partyQuests;
	std::unordered_map<uint64, uint32> m_partyRequestMs;

	uint8* m_cave = nullptr;
	uint32 m_caveUsed = 0;
	uint32 m_patchFailures = 0;
};

#define sQuestLog QuestLogStorage::Instance()
