#include "QuestLogStorage.h"

#include <ClientDetours.h>
#include <Logger.h>
#include <Packets/Packet.h>
#include <Packets/QuestLogPackets.h>
#include <SharedDefines.h>
#include <Util.h>

#include <Windows.h>
#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <ctime>

using namespace ClientData::QuestLog;

QuestLogStorage& QuestLogStorage::Instance()
{
	static QuestLogStorage instance;
	return instance;
}

bool QuestLogStorage::VerifyBytes(uint32_t address, uint32_t expected) const
{
	uint32_t current = *reinterpret_cast<const uint32_t*>(address);
	if (current == expected)
		return true;

	LOG_ERROR << "QuestLog patch mismatch at " << std::hex << address << " expected " << expected << " found " << current << std::dec;
	return false;
}

void QuestLogStorage::PatchReloc(const QuestLogPatchSite& site, uint32_t oldBase, uint32_t newBase)
{
	if (!VerifyBytes(site.address, site.expected))
	{
		++m_patchFailures;
		return;
	}

	Util::OverwriteUInt32AtAddress(site.address, newBase + (site.expected - oldBase));
}

void QuestLogStorage::PatchImm8(const QuestLogPatchSite& site)
{
	uint8_t current = *reinterpret_cast<const uint8_t*>(site.address);
	if (current != static_cast<uint8_t>(site.expected))
	{
		LOG_ERROR << "QuestLog imm8 mismatch at " << std::hex << site.address << std::dec;
		++m_patchFailures;
		return;
	}

	Util::SetByteAtAddress(reinterpret_cast<void*>(site.address), static_cast<uint8_t>(site.value));
}

void QuestLogStorage::PatchImm32(const QuestLogPatchSite& site)
{
	if (!VerifyBytes(site.address, site.expected))
	{
		++m_patchFailures;
		return;
	}

	Util::OverwriteUInt32AtAddress(site.address, site.value);
}

void QuestLogStorage::PatchSlotBase(const QuestLogPatchSite& site)
{
	if (!VerifyBytes(site.address, site.expected) || *reinterpret_cast<const uint16_t*>(site.address + 4) != 0)
	{
		++m_patchFailures;
		return;
	}

	uint8_t bytes[6];
	bytes[0] = static_cast<uint8_t>(0xB8 + (site.value & 7));
	uint32_t base = reinterpret_cast<uint32_t>(m_slots) - kSlotFieldBase;
	memcpy(bytes + 1, &base, sizeof(base));
	bytes[5] = 0x90;
	Util::OverwriteBytesAtAddress(site.address, bytes, sizeof(bytes));
}

void QuestLogStorage::PatchLoopCave(const QuestLogPatchSite& site)
{
	if (!VerifyBytes(site.address, site.expected))
	{
		++m_patchFailures;
		return;
	}

	int8_t rel8 = *reinterpret_cast<const int8_t*>(site.address + 4);
	uint32_t loopTarget = site.address + 5 + rel8;
	uint32_t fallthrough = site.address + 5;

	uint8_t* cave = m_cave + m_caveUsed;
	m_caveUsed += kCaveEntrySize;

	cave[0] = 0x81;
	cave[1] = static_cast<uint8_t>(0xF8 + (site.value & 7));
	uint32_t capacity = kQuestLogEntryCapacity;
	memcpy(cave + 2, &capacity, sizeof(capacity));
	cave[6] = 0x0F;
	cave[7] = 0x82;
	int32_t jbRel = static_cast<int32_t>(loopTarget - (reinterpret_cast<uint32_t>(cave) + 12));
	memcpy(cave + 8, &jbRel, sizeof(jbRel));
	cave[12] = 0xE9;
	int32_t backRel = static_cast<int32_t>(fallthrough - (reinterpret_cast<uint32_t>(cave) + 17));
	memcpy(cave + 13, &backRel, sizeof(backRel));

	uint8_t jmp[5];
	jmp[0] = 0xE9;
	int32_t caveRel = static_cast<int32_t>(reinterpret_cast<uint32_t>(cave) - (site.address + 5));
	memcpy(jmp + 1, &caveRel, sizeof(caveRel));
	Util::OverwriteBytesAtAddress(site.address, jmp, sizeof(jmp));
}

void QuestLogStorage::ApplyPatches()
{
	uint32_t caveCount = 0;
	for (const QuestLogPatchSite& site : kQuestLogPatchSites)
		if (site.kind == QuestLogPatchKind::LoopCave)
			++caveCount;

	m_cave = static_cast<uint8_t*>(VirtualAlloc(nullptr, caveCount * kCaveEntrySize + 64, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE));
	if (!m_cave)
	{
		LOG_ERROR << "QuestLog code cave allocation failed";
		return;
	}

	for (const QuestLogPatchSite& site : kQuestLogPatchSites)
	{
		switch (site.kind)
		{
		case QuestLogPatchKind::RelocQuests:
			PatchReloc(site, kQuestLogOriginalQuestsBase, reinterpret_cast<uint32_t>(m_entries));
			break;
		case QuestLogPatchKind::RelocPoi:
			PatchReloc(site, kQuestLogOriginalPoiBase, reinterpret_cast<uint32_t>(m_poi));
			break;
		case QuestLogPatchKind::RelocSort:
			PatchReloc(site, kQuestLogOriginalSortBase, reinterpret_cast<uint32_t>(m_sortTypes));
			break;
		case QuestLogPatchKind::Imm8:
			PatchImm8(site);
			break;
		case QuestLogPatchKind::Imm32:
			PatchImm32(site);
			break;
		case QuestLogPatchKind::SlotBase:
			PatchSlotBase(site);
			break;
		case QuestLogPatchKind::LoopCave:
			PatchLoopCave(site);
			break;
		}
	}

	if (m_patchFailures)
		LOG_ERROR << "QuestLog patches failed: " << m_patchFailures;
	else
		LOG_INFO << "QuestLog patches applied, " << kQuestLogSlots << " slots";
}

void QuestLogStorage::ClearSlots()
{
	memset(m_slots, 0, sizeof(m_slots));
}

void QuestLogStorage::Reset()
{
	ClearSlots();
	ClearPartyQuests();
}

void QuestLogStorage::ClearPartyQuests()
{
	m_partyQuests.clear();
	m_partyRequestMs.clear();
}

Slot* QuestLogStorage::GetSlot(uint32_t index)
{
	if (index >= kQuestLogSlots)
		return nullptr;
	return &m_slots[index];
}

void QuestLogStorage::SetSlot(uint32_t index, const Slot& data)
{
	if (index >= kQuestLogSlots)
		return;
	m_slots[index] = data;
}

bool QuestLogStorage::IsOnQuest(uint32_t questId) const
{
	if (!questId)
		return false;
	for (const Slot& slot : m_slots)
		if (slot.questId == questId)
			return true;
	return false;
}

void QuestLogStorage::OnFullLogReceived()
{
	Update(true);
}

void QuestLogStorage::OnSlotReceived(uint32_t index, const Slot& data)
{
	if (index >= kQuestLogSlots)
		return;

	Slot old = m_slots[index];
	m_slots[index] = data;

	uint64_t playerGuid = ClntObjMgr::GetActivePlayer();
	if (!playerGuid)
		return;

	uint32_t guidLow = static_cast<uint32_t>(playerGuid);
	uint32_t guidHigh = static_cast<uint32_t>(playerGuid >> 32);
	if (!PlayerObjectPtr(guidLow, guidHigh, 0x10, "QuestLogStorage.cpp", __LINE__))
		return;

	OnUpdateQuest(guidLow, guidHigh, static_cast<int>(kSlotFieldBase + index * kSlotFieldSize), 0, &old);
}

void QuestLogStorage::PruneWatches()
{
	uint32_t guard = 0;
	while (guard++ < kMaxWatches * 2)
	{
		uint32_t numWatches = std::min<uint32_t>(*m_numWatches, kMaxWatches);
		uint32_t stale = 0;
		bool foundStale = false;
		for (uint32_t w = 0; w < numWatches && !foundStale; ++w)
		{
			uint32_t questId = m_watches[w].questId;
			bool present = false;
			for (uint32_t e = 0; e < *m_numQuest; ++e)
			{
				if (!m_entries[e].isHeader && static_cast<uint32_t>(m_entries[e].questId) == questId)
				{
					present = true;
					break;
				}
			}
			if (!present)
			{
				stale = questId;
				foundStale = true;
			}
		}

		if (!foundStale)
			return;

		uint32_t before = *m_numWatches;
		RemoveQuestWatchById(stale);
		if (*m_numWatches >= before)
			return;
	}
}

void QuestLogStorage::SendMissingPoiQuery(const uint32_t* questIds, uint32_t count)
{
	Packet packet(kOpcodeQuestPoiQuery);
	packet.PutUInt32(count);
	for (uint32_t i = 0; i < count; ++i)
		packet.PutUInt32(questIds[i]);
	packet.Send();
}

void QuestLogStorage::SendQueryTimeIfDue()
{
	if (!*s_questResetTime)
		return;
	if (static_cast<uint32_t>(time(nullptr)) < *s_questResetTime)
		return;

	Packet packet(kOpcodeQueryTime);
	packet.Send();
	*s_questResetTime = 0;
}

void QuestLogStorage::Update(bool rebuild)
{
	if (!rebuild)
	{
		SendUniqueSignal(kEventQuestLogUpdate);
		return;
	}

	if (*s_questCallbackCount != 0)
		return;

	uint64_t playerGuid = ClntObjMgr::GetActivePlayer();
	if (!PlayerObjectPtr(static_cast<uint32_t>(playerGuid), static_cast<uint32_t>(playerGuid >> 32), 0x10, "QuestLogStorage.cpp", __LINE__))
		return;

	void* collapseCvar = *s_cvQuestLogCollapseFilter;
	uint32_t collapseMask = collapseCvar ? *reinterpret_cast<uint32_t*>(static_cast<uint8_t*>(collapseCvar) + kCVarValueOffset) : 0;

	int32_t oldSortTypes[kQuestLogSortTypes];
	uint32_t oldNumSortTypes = std::min<uint32_t>(*m_numSortTypes, kQuestLogSortTypes);
	for (uint32_t i = 0; i < oldNumSortTypes; ++i)
	{
		int32_t value = m_sortTypes[i];
		if (collapseMask & (1u << i))
			value = -value;
		oldSortTypes[i] = value;
	}

	memset(m_entries, 0, sizeof(m_entries));
	memset(m_sortTypes, 0, sizeof(m_sortTypes));
	*m_numQuest = 0;
	*m_numSortTypes = 0;
	*s_questCallbackCount = 0;

	uint32_t missing[kQuestLogSlots];
	uint32_t numMissing = 0;
	int32_t timeDelta = *s_serverTimeDelta;

	for (uint32_t slotIndex = 0; slotIndex < kQuestLogSlots; ++slotIndex)
	{
		const Slot& slot = m_slots[slotIndex];
		if (static_cast<int32_t>(slot.questId) <= 0)
			continue;
		if (*m_numQuest + 2 > kQuestLogEntryCapacity)
			break;

		uint32_t callbackData[2] = { 0, 0 };
		void* record = GetQuestRecord(g_questDBCache, slot.questId, callbackData, reinterpret_cast<void*>(QuestQueryCounterCallback), 0, 0);
		if (!record)
		{
			++*s_questCallbackCount;
			continue;
		}

		if (!HasQuestPoi(slot.questId))
			missing[numMissing++] = slot.questId;

		Entry& entry = m_entries[*m_numQuest];
		entry.questId = static_cast<int32_t>(slot.questId);
		entry.slot = slotIndex;
		++*m_numQuest;

		if (slot.timer && !(slot.state & kQuestStateFailed))
		{
			int32_t remaining = static_cast<int32_t>(slot.timer + timeDelta - static_cast<uint32_t>(time(nullptr)) - 1);
			if (remaining < 0)
				SetQuestExpired(*m_numQuest - 1);
		}

		int32_t sortType = *reinterpret_cast<int32_t*>(static_cast<uint8_t*>(record) + kQuestRecordSortTypeOffset);
		uint32_t numSortTypes = *m_numSortTypes;
		uint32_t found = 0;
		while (found < numSortTypes && m_sortTypes[found] != sortType)
			++found;
		if (found != numSortTypes)
			continue;
		if (numSortTypes >= kQuestLogSortTypes)
			continue;

		m_sortTypes[numSortTypes] = sortType;
		*m_numSortTypes = numSortTypes + 1;

		Entry& header = m_entries[*m_numQuest];
		header.questId = sortType;
		header.isHeader = 1;
		++*m_numQuest;
	}

	qsort(m_sortTypes, *m_numSortTypes, sizeof(int32_t), QSortQuestSortTypes);

	for (uint32_t i = 0; i < *m_numSortTypes; ++i)
	{
		int32_t sortType = m_sortTypes[i];
		bool expanded = false;
		bool matched = false;
		for (uint32_t j = 0; j < oldNumSortTypes; ++j)
		{
			if (oldSortTypes[j] == sortType)
			{
				expanded = true;
				matched = true;
				break;
			}
			if (oldSortTypes[j] == -sortType)
			{
				matched = true;
				break;
			}
		}

		if (matched && expanded)
			collapseMask &= ~(1u << i);
		else
			collapseMask |= (1u << i);
	}

	if (collapseCvar)
		SetCollapseFilter(collapseCvar, static_cast<int>(collapseMask), 1, 0, 0, 1);

	FilterAndSortQuests();

	if (!*s_trackedQuestsLoaded && *m_numQuest > 0)
		LoadTrackedQuests();

	PruneWatches();

	SendUniqueSignal(kEventQuestLogUpdate);

	if (numMissing)
		SendMissingPoiQuery(missing, numMissing);

	SendQueryTimeIfDue();
}

void QuestLogStorage::SetPartyQuests(uint64_t guid, std::vector<uint32_t> questIds)
{
	m_partyQuests[guid] = std::move(questIds);
	m_partyRequestMs.erase(guid);
	SendUnitSignal(&guid, kEventUnitQuestLogChanged);
}

bool QuestLogStorage::ShouldRequestParty(uint64_t guid)
{
	uint32_t now = GetTickCount();
	auto it = m_partyRequestMs.find(guid);
	if (it != m_partyRequestMs.end() && now - it->second < kPartyRequestIntervalMs)
		return false;
	m_partyRequestMs[guid] = now;
	return true;
}

int QuestLogStorage::ScriptIsUnitOnQuest(lua_State* L)
{
	if (!LuaIsNumber(L, 1) || !LuaIsString(L, 2))
	{
		LuaError(L, "Usage: IsUnitOnQuest(index, \"unit\")");
		return 0;
	}

	int32_t index = static_cast<int32_t>(LuaToNumber(L, 1)) - 1;
	if (index < 0 || static_cast<uint32_t>(index) >= *m_numQuest || m_entries[index].isHeader || m_entries[index].questId <= 0)
	{
		LuaPushNil(L);
		return 1;
	}

	uint32_t questId = static_cast<uint32_t>(m_entries[index].questId);
	uint64_t guid = GetGUIDFromName(LuaToLString(L, 2, nullptr));
	if (!guid)
	{
		LuaPushNil(L);
		return 1;
	}

	if (guid == ClntObjMgr::GetActivePlayer())
	{
		if (IsOnQuest(questId))
			LuaPushNumber(L, 1.0);
		else
			LuaPushNil(L);
		return 1;
	}

	if (!IsPartyMember(&guid))
	{
		LuaPushNil(L);
		return 1;
	}

	auto it = m_partyQuests.find(guid);
	if (it == m_partyQuests.end())
	{
		if (ShouldRequestParty(guid))
			QuestLogPackets::SendPartyRequest(guid);
		LuaPushNil(L);
		return 1;
	}

	if (std::find(it->second.begin(), it->second.end(), questId) != it->second.end())
		LuaPushNumber(L, 1.0);
	else
		LuaPushNil(L);
	return 1;
}

CLIENT_DETOUR(CGQuestLog__Update, 0x5E6940, __cdecl, void, (int rebuild))
{
	sQuestLog.Update(rebuild != 0);
}

CLIENT_DETOUR_THISCALL(CGQuestLog__GetQuestIdFromIndex, 0x5E0870, Slot*, (uint32_t index))
{
	return sQuestLog.GetSlot(index);
}

CLIENT_DETOUR(Script_IsUnitOnQuest, 0x5E4070, __cdecl, int, (lua_State * L))
{
	return sQuestLog.ScriptIsUnitOnQuest(L);
}

CLIENT_DETOUR(CGQuestLog__InitializeGame, 0x5E71A0, __cdecl, int, ())
{
	int result = CGQuestLog__InitializeGame();
	sQuestLog.Reset();
	return result;
}

CLIENT_DETOUR(CGQuestLog__ShutdownGame, 0x5E6F30, __cdecl, int, ())
{
	int result = CGQuestLog__ShutdownGame();
	sQuestLog.Reset();
	return result;
}

CLIENT_DETOUR(CGPartyInfo__RemoveAll, 0x52D6E0, __cdecl, void*, (int notify))
{
	void* result = CGPartyInfo__RemoveAll(notify);
	sQuestLog.ClearPartyQuests();
	return result;
}
