#include <Defines.h>
#include "LootWindowPackets.h"
#include "Packet.h"
#include <CustomPacket.h>
#include <SharedDefines.h>
#include <Entities/LootWindow.h>

LootWindowPackets& LootWindowPackets::Instance()
{
	static LootWindowPackets instance;
	return instance;
}

void LootWindowPackets::Handler_SMSG_PRIME_LOOT_TARGET(void*, uint32, uint32, CDataStore* pkt)
{
	sLootWindow.OnServerPushedLoot(Packet(pkt).GetUInt64());
}

void LootWindowPackets::Apply()
{
	sCustomPacket.RegisterHandler(SMSG_PRIME_LOOT_TARGET, &Handler_SMSG_PRIME_LOOT_TARGET);
}
