#include <Defines.h>
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

bool QuestLogStorage::VerifyBytes(uint32 address, uint32 expected) const
{
	uint32 current = *reinterpret_cast<const uint32*>(address);
	if (current == expected)
		return true;

	LOG_ERROR << "QuestLog patch mismatch at " << std::hex << address << " expected " << expected << " found " << current << std::dec;
	return false;
}

void QuestLogStorage::PatchReloc(const QuestLogPatchSite& site, uint32 oldBase, uint32 newBase)
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
	uint8 current = *reinterpret_cast<const uint8*>(site.address);
	if (current != static_cast<uint8>(site.expected))
	{
		LOG_ERROR << "QuestLog imm8 mismatch at " << std::hex << site.address << std::dec;
		++m_patchFailures;
		return;
	}

	Util::SetByteAtAddress(reinterpret_cast<void*>(site.address), static_cast<uint8>(site.value));
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
	if (!VerifyBytes(site.address, site.expected) || *reinterpret_cast<const uint16*>(site.address + 4) != 0)
	{
		++m_patchFailures;
		return;
	}

	uint8 bytes[6];
	bytes[0] = static_cast<uint8>(0xB8 + (site.value & 7));
	uint32 base = reinterpret_cast<uint32>(m_slots) - kSlotFieldBase;
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

	int8 rel8 = *reinterpret_cast<const int8*>(site.address + 4);
	uint32 loopTarget = site.address + 5 + rel8;
	uint32 fallthrough = site.address + 5;

	uint8* cave = m_cave + m_caveUsed;
	m_caveUsed += kCaveEntrySize;

	cave[0] = 0x81;
	cave[1] = static_cast<uint8>(0xF8 + (site.value & 7));
	uint32 capacity = kQuestLogEntryCapacity;
	memcpy(cave + 2, &capacity, sizeof(capacity));
	cave[6] = 0x0F;
	cave[7] = 0x82;
	int32 jbRel = static_cast<int32>(loopTarget - (reinterpret_cast<uint32>(cave) + 12));
	memcpy(cave + 8, &jbRel, sizeof(jbRel));
	cave[12] = 0xE9;
	int32 backRel = static_cast<int32>(fallthrough - (reinterpret_cast<uint32>(cave) + 17));
	memcpy(cave + 13, &backRel, sizeof(backRel));

	uint8 jmp[5];
	jmp[0] = 0xE9;
	int32 caveRel = static_cast<int32>(reinterpret_cast<uint32>(cave) - (site.address + 5));
	memcpy(jmp + 1, &caveRel, sizeof(caveRel));
	Util::OverwriteBytesAtAddress(site.address, jmp, sizeof(jmp));
}

void QuestLogStorage::ApplyPatches()
{
	uint32 caveCount = 0;
	for (const QuestLogPatchSite& site : kQuestLogPatchSites)
		if (site.kind == QuestLogPatchKind::LoopCave)
			++caveCount;

	m_cave = static_cast<uint8*>(VirtualAlloc(nullptr, caveCount * kCaveEntrySize + 64, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE));
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
			PatchReloc(site, kQuestLogOriginalQuestsBase, reinterpret_cast<uint32>(m_entries));
			break;
		case QuestLogPatchKind::RelocPoi:
			PatchReloc(site, kQuestLogOriginalPoiBase, reinterpret_cast<uint32>(m_poi));
			break;
		case QuestLogPatchKind::RelocSort:
			PatchReloc(site, kQuestLogOriginalSortBase, reinterpret_cast<uint32>(m_sortTypes));
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

Slot* QuestLogStorage::GetSlot(uint32 index)
{
	if (index >= kQuestLogSlots)
		return nullptr;
	return &m_slots[index];
}

void QuestLogStorage::SetSlot(uint32 index, const Slot& data)
{
	if (index >= kQuestLogSlots)
		return;
	m_slots[index] = data;
}

bool QuestLogStorage::IsOnQuest(uint32 questId) const
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

void QuestLogStorage::OnSlotReceived(uint32 index, const Slot& data)
{
	if (index >= kQuestLogSlots)
		return;

	Slot old = m_slots[index];
	m_slots[index] = data;

	uint64 playerGuid = ClntObjMgr::GetActivePlayer();
	if (!playerGuid)
		return;

	uint32 guidLow = static_cast<uint32>(playerGuid);
	uint32 guidHigh = static_cast<uint32>(playerGuid >> 32);
	if (!PlayerObjectPtr(guidLow, guidHigh, 0x10, "QuestLogStorage.cpp", __LINE__))
		return;

	OnUpdateQuest(guidLow, guidHigh, static_cast<int>(kSlotFieldBase + index * kSlotFieldSize), 0, &old);
}

void QuestLogStorage::PruneWatches()
{
	uint32 guard = 0;
	while (guard++ < kMaxWatches * 2)
	{
		uint32 numWatches = std::min<uint32>(*m_numWatches, kMaxWatches);
		uint32 stale = 0;
		bool foundStale = false;
		for (uint32 w = 0; w < numWatches && !foundStale; ++w)
		{
			uint32 questId = m_watches[w].questId;
			bool present = false;
			for (uint32 e = 0; e < *m_numQuest; ++e)
			{
				if (!m_entries[e].isHeader && static_cast<uint32>(m_entries[e].questId) == questId)
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

		uint32 before = *m_numWatches;
		RemoveQuestWatchById(stale);
		if (*m_numWatches >= before)
			return;
	}
}

void QuestLogStorage::SendMissingPoiQuery(const uint32* questIds, uint32 count)
{
	Packet packet(kOpcodeQuestPoiQuery);
	packet.PutUInt32(count);
	for (uint32 i = 0; i < count; ++i)
		packet.PutUInt32(questIds[i]);
	packet.Send();
}

void QuestLogStorage::SendQueryTimeIfDue()
{
	if (!*s_questResetTime)
		return;
	if (static_cast<uint32>(time(nullptr)) < *s_questResetTime)
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

	uint64 playerGuid = ClntObjMgr::GetActivePlayer();
	if (!PlayerObjectPtr(static_cast<uint32>(playerGuid), static_cast<uint32>(playerGuid >> 32), 0x10, "QuestLogStorage.cpp", __LINE__))
		return;

	void* collapseCvar = *s_cvQuestLogCollapseFilter;
	uint32 collapseMask = collapseCvar ? *reinterpret_cast<uint32*>(static_cast<uint8*>(collapseCvar) + kCVarValueOffset) : 0;

	int32 oldSortTypes[kQuestLogSortTypes];
	uint32 oldNumSortTypes = std::min<uint32>(*m_numSortTypes, kQuestLogSortTypes);
	for (uint32 i = 0; i < oldNumSortTypes; ++i)
	{
		int32 value = m_sortTypes[i];
		if (collapseMask & (1u << i))
			value = -value;
		oldSortTypes[i] = value;
	}

	memset(m_entries, 0, sizeof(m_entries));
	memset(m_sortTypes, 0, sizeof(m_sortTypes));
	*m_numQuest = 0;
	*m_numSortTypes = 0;
	*s_questCallbackCount = 0;

	uint32 missing[kQuestLogSlots];
	uint32 numMissing = 0;
	int32 timeDelta = *s_serverTimeDelta;

	for (uint32 slotIndex = 0; slotIndex < kQuestLogSlots; ++slotIndex)
	{
		const Slot& slot = m_slots[slotIndex];
		if (static_cast<int32>(slot.questId) <= 0)
			continue;
		if (*m_numQuest + 2 > kQuestLogEntryCapacity)
			break;

		uint32 callbackData[2] = { 0, 0 };
		void* record = GetQuestRecord(g_questDBCache, slot.questId, callbackData, reinterpret_cast<void*>(QuestQueryCounterCallback), 0, 0);
		if (!record)
		{
			++*s_questCallbackCount;
			continue;
		}

		if (!HasQuestPoi(slot.questId))
			missing[numMissing++] = slot.questId;

		Entry& entry = m_entries[*m_numQuest];
		entry.questId = static_cast<int32>(slot.questId);
		entry.slot = slotIndex;
		++*m_numQuest;

		if (slot.timer && !(slot.state & kQuestStateFailed))
		{
			int32 remaining = static_cast<int32>(slot.timer + timeDelta - static_cast<uint32>(time(nullptr)) - 1);
			if (remaining < 0)
				SetQuestExpired(*m_numQuest - 1);
		}

		int32 sortType = *reinterpret_cast<int32*>(static_cast<uint8*>(record) + kQuestRecordSortTypeOffset);
		uint32 numSortTypes = *m_numSortTypes;
		uint32 found = 0;
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

	qsort(m_sortTypes, *m_numSortTypes, sizeof(int32), QSortQuestSortTypes);

	for (uint32 i = 0; i < *m_numSortTypes; ++i)
	{
		int32 sortType = m_sortTypes[i];
		bool expanded = false;
		bool matched = false;
		for (uint32 j = 0; j < oldNumSortTypes; ++j)
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

void QuestLogStorage::SetPartyQuests(uint64 guid, std::vector<uint32> questIds)
{
	m_partyQuests[guid] = std::move(questIds);
	m_partyRequestMs.erase(guid);
	SendUnitSignal(&guid, kEventUnitQuestLogChanged);
}

bool QuestLogStorage::ShouldRequestParty(uint64 guid)
{
	uint32 now = GetTickCount();
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

	int32 index = static_cast<int32>(LuaToNumber(L, 1)) - 1;
	if (index < 0 || static_cast<uint32>(index) >= *m_numQuest || m_entries[index].isHeader || m_entries[index].questId <= 0)
	{
		LuaPushNil(L);
		return 1;
	}

	uint32 questId = static_cast<uint32>(m_entries[index].questId);
	uint64 guid = GetGUIDFromName(LuaToLString(L, 2, nullptr));
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

CLIENT_DETOUR_THISCALL(CGQuestLog__GetQuestIdFromIndex, 0x5E0870, Slot*, (uint32 index))
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
