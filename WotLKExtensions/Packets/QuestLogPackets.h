#pragma once

#include <SharedDefines.h>

class QuestLogPackets
{
public:
	static QuestLogPackets& Instance();

	void Apply();

	static void SendPartyRequest(uint64_t guid);

	QuestLogPackets(const QuestLogPackets&) = delete;
	QuestLogPackets& operator=(const QuestLogPackets&) = delete;

private:
	QuestLogPackets() = default;

	static void Handler_SMSG_QUEST_LOG_FULL(void* param, uint32_t opcode, uint32_t a2, CDataStore* pkt);
	static void Handler_SMSG_QUEST_LOG_SLOTS(void* param, uint32_t opcode, uint32_t a2, CDataStore* pkt);
	static void Handler_SMSG_QUEST_LOG_PARTY(void* param, uint32_t opcode, uint32_t a2, CDataStore* pkt);
};

#define sQuestLogPackets QuestLogPackets::Instance()
