#pragma once

#include <cstdint>

struct CDataStore;

// Handles SMSG_PRIME_LOOT_TARGET. The 3.3.5 client only opens the corpse loot
// window if it initiated the loot itself (CGPlayer_C sends CMSG_LOOT and stores
// the target GUID at +0x18E0). CGPlayer_C__OnLootResponse rejects a LOOT_CORPSE
// response whose GUID does not match that field and fires CMSG_LOOT_RELEASE
// instead. This module writes the pending-loot-target GUID directly so a
// server-driven loot response (fetcher pets, etc.) is accepted and the window
// opens. The server sends this immediately before its SMSG_LOOT_RESPONSE.
class LootWindowPackets
{
public:
	static LootWindowPackets& Instance();

	LootWindowPackets(const LootWindowPackets&) = delete;
	LootWindowPackets& operator=(const LootWindowPackets&) = delete;

	void Apply();

private:
	LootWindowPackets() = default;

	static void Handler_SMSG_PRIME_LOOT_TARGET(void*, uint32_t, uint32_t, CDataStore* pkt);
};

#define sLootWindowPackets LootWindowPackets::Instance()
