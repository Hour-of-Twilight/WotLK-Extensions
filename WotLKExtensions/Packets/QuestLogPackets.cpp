#include "QuestLogPackets.h"
#include "Packet.h"
#include <CustomPacket.h>
#include <Quests/QuestLogStorage.h>

#include <vector>

QuestLogPackets& QuestLogPackets::Instance()
{
	static QuestLogPackets instance;
	return instance;
}

void QuestLogPackets::Apply()
{
	sCustomPacket.RegisterHandler(SMSG_QUEST_LOG_FULL, &Handler_SMSG_QUEST_LOG_FULL);
	sCustomPacket.RegisterHandler(SMSG_QUEST_LOG_SLOTS, &Handler_SMSG_QUEST_LOG_SLOTS);
	sCustomPacket.RegisterHandler(SMSG_QUEST_LOG_PARTY, &Handler_SMSG_QUEST_LOG_PARTY);
}

void QuestLogPackets::SendPartyRequest(uint64_t guid)
{
	Packet packet(CMSG_QUEST_LOG_PARTY_REQUEST);
	packet.PutUInt64(guid);
	packet.Send();
}

static ClientData::QuestLog::Slot ReadSlot(Packet& r, uint32_t& index)
{
	ClientData::QuestLog::Slot slot;
	index = r.GetUInt8();
	slot.questId = r.GetUInt32();
	slot.state = r.GetUInt32();
	slot.counters[0] = r.GetUInt32();
	slot.counters[1] = r.GetUInt32();
	slot.timer = r.GetUInt32();
	return slot;
}

void QuestLogPackets::Handler_SMSG_QUEST_LOG_FULL(void*, uint32_t, uint32_t, CDataStore* pkt)
{
	Packet r(pkt);
	uint32_t count = r.GetUInt8();

	sQuestLog.ClearSlots();
	for (uint32_t i = 0; i < count; ++i)
	{
		uint32_t index;
		ClientData::QuestLog::Slot slot = ReadSlot(r, index);
		sQuestLog.SetSlot(index, slot);
	}

	sQuestLog.OnFullLogReceived();
}

void QuestLogPackets::Handler_SMSG_QUEST_LOG_SLOTS(void*, uint32_t, uint32_t, CDataStore* pkt)
{
	Packet r(pkt);
	uint32_t count = r.GetUInt8();

	for (uint32_t i = 0; i < count; ++i)
	{
		uint32_t index;
		ClientData::QuestLog::Slot slot = ReadSlot(r, index);
		sQuestLog.OnSlotReceived(index, slot);
	}
}

void QuestLogPackets::Handler_SMSG_QUEST_LOG_PARTY(void*, uint32_t, uint32_t, CDataStore* pkt)
{
	Packet r(pkt);
	uint64_t guid = r.GetUInt64();
	uint32_t count = r.GetUInt8();

	std::vector<uint32_t> questIds;
	questIds.reserve(count);
	for (uint32_t i = 0; i < count; ++i)
		questIds.push_back(r.GetUInt32());

	sQuestLog.SetPartyQuests(guid, std::move(questIds));
}
