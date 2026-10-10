#pragma once

#include <Defines.h>

#include <SharedDefines.h>

class QuestLogPackets
{
public:
	static QuestLogPackets& Instance();

	void Apply();

	static void SendPartyRequest(uint64 guid);

	QuestLogPackets(const QuestLogPackets&) = delete;
	QuestLogPackets& operator=(const QuestLogPackets&) = delete;

private:
	QuestLogPackets() = default;

	static void Handler_SMSG_QUEST_LOG_FULL(void* param, uint32 opcode, uint32 a2, CDataStore* pkt);
	static void Handler_SMSG_QUEST_LOG_SLOTS(void* param, uint32 opcode, uint32 a2, CDataStore* pkt);
	static void Handler_SMSG_QUEST_LOG_PARTY(void* param, uint32 opcode, uint32 a2, CDataStore* pkt);
};

#define sQuestLogPackets QuestLogPackets::Instance()
