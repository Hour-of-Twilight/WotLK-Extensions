#include "ItemDbCachePackets.h"
#include "Packet.h"
#include <CustomPacket.h>
#include <ClientData/ClientFunctions.h>
#include <SharedDefines.h>
#include <Player.h>
#include <Item.h>

struct RefreshItemContext
{
	uint32_t targetItemEntry;
};

static constexpr uintptr_t PLAYER_ITEM_BASE_OFFSET = 0x1008;
static constexpr uintptr_t PLAYER_COMP_OFFSET = 0xB4C;
static constexpr uintptr_t ITEM_ENTRY_OFFSET = 0x21C;
static constexpr uint32 ITEM_SLOT_STRIDE = 8;

static int __cdecl RefreshItemCallback(uint32 guidLow, uint32 guidHigh, void* userdata)
{
	auto* ctx = static_cast<RefreshItemContext*>(userdata);
	uint64 guid = (static_cast<uint64>(guidHigh) << 32) | guidLow;

	auto* player = static_cast<CGPlayer*>(ClntObjMgr::ObjectPtr(guid, TYPEMASK_PLAYER));
	if (!player)
		return 1;

	uintptr_t playerBase = reinterpret_cast<uintptr_t>(player);

	uint8* itemBase = *reinterpret_cast<uint8**>(playerBase + PLAYER_ITEM_BASE_OFFSET);
	if (!itemBase)
		return 1;

	void* comp = *reinterpret_cast<void**>(playerBase + PLAYER_COMP_OFFSET);
	if (!comp)
		return 1;

	for (uint32 slot = 0; slot <= 0x12; slot++)
	{
		uint32* itemEntry = reinterpret_cast<uint32*>(itemBase + slot * ITEM_SLOT_STRIDE + ITEM_ENTRY_OFFSET);
		if (*itemEntry != ctx->targetItemEntry)
			continue;

		CCharacterComponent::RemoveItemBySlot(comp, slot);
		CGPlayer_C::EquipVisibleItem(player, reinterpret_cast<int*>(itemEntry), slot);
	}

	return 1;
}

static void RefreshItemForAllPlayers(uint32_t targetItemEntry)
{
	RefreshItemContext ctx;
	ctx.targetItemEntry = targetItemEntry;
	ClntObjMgr::EnumVisibleObjects(RefreshItemCallback, &ctx);
}

void ItemDbCachePackets::Handler_SMSG_ITEM_DB_CACHE(void*, uint32_t, uint32_t, CDataStore* pkt)
{
	Packet r(pkt);
	uint32_t itemId = r.GetUInt32();
	int32_t itemClass = r.GetInt32();
	int32_t subClass = r.GetInt32();
	int32_t displayInfo = r.GetInt32();
	int32_t invType = r.GetInt32();
	int32_t material = r.GetInt32();
	int32_t soundOverride = r.GetInt32();
	int32_t sheath = r.GetInt32();

	if (itemId == 0)
		return;

	Item::AddItemRecordFromFields(static_cast<int>(itemId), itemClass, subClass,
	    soundOverride, material, displayInfo, invType, sheath);

	if (ClntObjMgr::GetActivePlayer())
		RefreshItemForAllPlayers(itemId);
}

void ItemDbCachePackets::RegisterHandlers()
{
	sCustomPacket.RegisterHandler(SMSG_ITEM_DB_CACHE, &Handler_SMSG_ITEM_DB_CACHE);
}

void ItemDbCachePackets::RegisterRealmHandlers()
{
	sCustomPacket.RegisterRealmHandler(SMSG_ITEM_DB_CACHE, &Handler_SMSG_ITEM_DB_CACHE);
}
