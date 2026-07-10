#include "LootWindowPackets.h"
#include "Packet.h"
#include <CustomPacket.h>
#include <SharedDefines.h>

static constexpr uint32_t kLootTargetGuidOffset = 0x18E0;
static constexpr uint32_t kAutoLootFlagOffset = 0x18E8;

LootWindowPackets& LootWindowPackets::Instance()
{
	static LootWindowPackets instance;
	return instance;
}

void LootWindowPackets::Handler_SMSG_PRIME_LOOT_TARGET(void*, uint32_t, uint32_t, CDataStore* pkt)
{
	uint64_t guid = Packet(pkt).GetUInt64();

	CGPlayer* player = ClntObjMgr::GetActivePlayerObj();
	if (!player)
		return;

	uint8_t* base = reinterpret_cast<uint8_t*>(player);
	*reinterpret_cast<uint64_t*>(base + kLootTargetGuidOffset) = guid;

	base[kAutoLootFlagOffset] = CGGameUI::IsAutoLooting() ? 1 : 0;
}

void LootWindowPackets::Apply()
{
	sCustomPacket.RegisterHandler(SMSG_PRIME_LOOT_TARGET, &Handler_SMSG_PRIME_LOOT_TARGET);
}
