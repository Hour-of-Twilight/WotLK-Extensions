#include <Defines.h>
#include "ItemDbCachePackets.h"
#include "Packet.h"
#include <CustomPacket.h>
#include <ClientData/ClientFunctions.h>
#include <ClientData/WowObjects.h>
#include <SharedDefines.h>
#include <Player.h>
#include <Item.h>

struct RefreshItemContext
{
	uint32 targetItemEntry;
};

static constexpr uintptr_t COMP_DISPLAY_ID_OFFSET = 0x428;

static int __cdecl RefreshItemCallback(uint32 guidLow, uint32 guidHigh, void* userdata)
{
	auto* ctx = static_cast<RefreshItemContext*>(userdata);
	uint64 guid = (static_cast<uint64>(guidHigh) << 32) | guidLow;

	ClientData::WowPlayer player = ClientData::WowPlayer::FromGuid(guid);
	if (!player)
		return 1;

	void* comp = player.CharComponent();
	if (!comp)
		return 1;

	uintptr_t compBase = reinterpret_cast<uintptr_t>(comp);

	for (uint32 slot = 0; slot <= 0x12; slot++)
	{
		uint32* itemEntry = player.VisibleItemEntrySlot(slot);
		uint32 displayId = *reinterpret_cast<uint32*>(compBase + COMP_DISPLAY_ID_OFFSET + slot * sizeof(uint32));

		if (*itemEntry != ctx->targetItemEntry)
			continue;

		CCharacterComponent::RemoveItemBySlot(comp, slot);
		CGPlayer_C::EquipVisibleItem(player.Player(), reinterpret_cast<int*>(itemEntry), slot);

		uint8 displayRecord[0x64] = {};
		if (displayId && ItemDisplayInfoDB::GetRecord(g_itemDisplayInfoDB, displayId, displayRecord))
			CCharacterComponent::AddItem(comp, slot, displayRecord, 0);
	}

	return 1;
}

static void RefreshItemForAllPlayers(uint32 targetItemEntry)
{
	RefreshItemContext ctx;
	ctx.targetItemEntry = targetItemEntry;
	ClntObjMgr::EnumVisibleObjects(RefreshItemCallback, &ctx);
}

void ItemDbCachePackets::Handler_SMSG_ITEM_DB_CACHE(void*, uint32, uint32, CDataStore* pkt)
{
	Packet r(pkt);
	uint32 itemId = r.GetUInt32();
	int32 itemClass = r.GetInt32();
	int32 subClass = r.GetInt32();
	int32 displayInfo = r.GetInt32();
	int32 invType = r.GetInt32();
	int32 material = r.GetInt32();
	int32 soundOverride = r.GetInt32();
	int32 sheath = r.GetInt32();

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
