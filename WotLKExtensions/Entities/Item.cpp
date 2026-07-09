#include "Item.h"
#include <Util.h>
#include <ClientDetours.h>
#include "Logger.h"

void Item::Apply()
{
	PatchItemDBC();
}

void Item::PatchItemDBC()
{
	while (!g_itemDB->b_base_01.m_loaded)
	{
		LOG_DEBUG << "Waiting for item dbc to be loaded...";
		Sleep(100);
	}

	int oldMinID = g_itemDB->b_base_01.m_minID;
	int oldMaxID = g_itemDB->b_base_01.m_maxID;
	int oldSize = (oldMaxID - oldMinID + 1);

	int newMinID = 0;
	int newMaxID = ITEM_ALLOCATE_AMOUNT;
	int newSize = (newMaxID - newMinID + 1);

	ItemRecord** newArray = (ItemRecord**)calloc(newSize, sizeof(ItemRecord*));

	if (!newArray)
	{
		LOG_DEBUG << "Failed to allocate memory for new item record array.";
		return;
	}

	ItemRecord** oldArray = g_itemDB->b_base_02.m_recordsById;
	for (int i = 0; i < oldSize; i++)
	{
		int itemID = oldMinID + i;
		newArray[itemID] = oldArray[i];
	}

	g_itemDB->b_base_02.m_recordsById = newArray;
	g_itemDB->b_base_01.m_minID = newMinID;
	g_itemDB->b_base_01.m_maxID = newMaxID;
	// Util::OverwriteBytesAtAddress((void*)0x0062C0E9, 0x90, 2);
}

void Item::AddItemToDBC(int itemID, int itemClass, int itemSubClass, int displayInfo, int inventorySlot, int materialID, int soundOverride, int sheathID)
{

	if (itemID < 0 || itemID > ITEM_ALLOCATE_AMOUNT)
	{
		return;
	}

	if (g_itemDB->b_base_02.m_recordsById[itemID])
	{
		free(g_itemDB->b_base_02.m_recordsById[itemID]);
		g_itemDB->b_base_01.m_numRecords--;
	}

	ItemRecord* record = (ItemRecord*)calloc(1, sizeof(ItemRecord));
	record->itemID = itemID;
	record->itemClass = itemClass;
	record->itemSubClass = itemSubClass;
	record->sound_override_subclassid = soundOverride;
	record->itemID = materialID;
	record->itemDisplayInfo = displayInfo;
	record->inventorySlotID = inventorySlot;
	record->sheathID = sheathID;

	g_itemDB->b_base_02.m_recordsById[itemID] = record;
	g_itemDB->b_base_01.m_numRecords++;
}

// Remove a custom item
void Item::RemoveItem(int itemID)
{
	if (itemID < 0 || itemID > ITEM_ALLOCATE_AMOUNT)
		return;

	if (g_itemDB->b_base_02.m_recordsById[itemID])
	{
		free(g_itemDB->b_base_02.m_recordsById[itemID]);
		g_itemDB->b_base_02.m_recordsById[itemID] = nullptr;
		g_itemDB->b_base_01.m_numRecords--;
	}
}

bool Item::ItemExists(int itemID)
{
	if (itemID < 0 || itemID > ITEM_ALLOCATE_AMOUNT)
		return false;

	return g_itemDB->b_base_02.m_recordsById[itemID] != nullptr;
}

ItemRecord* Item::GetItemRecord(int itemID)
{
	if (itemID < 0 || itemID > ITEM_ALLOCATE_AMOUNT)
		return nullptr;

	return g_itemDB->b_base_02.m_recordsById[itemID];
}

void Item::AddItemToMemoryFromCache(const ItemCache* cache)
{
	if (!cache)
		return;

	AddItemRecordFromFields(cache->Id, cache->Class, cache->SubClass, cache->UnkInt,
	    cache->Material, cache->DisplayId, cache->InvType, cache->Sheath);
}

void Item::AddItemRecordFromFields(int itemID, int itemClass, int itemSubClass,
    int soundOverride, int material, int displayInfo,
    int invType, int sheath)
{
	if (itemID < 0 || itemID > ITEM_ALLOCATE_AMOUNT)
		return;

	if (g_itemDB->b_base_02.m_recordsById[itemID])
	{
		free(g_itemDB->b_base_02.m_recordsById[itemID]);
		g_itemDB->b_base_01.m_numRecords--;
	}

	ItemRecord* record = (ItemRecord*)calloc(1, sizeof(ItemRecord));
	record->itemID = itemID;
	record->itemClass = itemClass;
	record->itemSubClass = itemSubClass;
	record->sound_override_subclassid = soundOverride;
	record->materialID = material;
	record->itemDisplayInfo = displayInfo;
	record->inventorySlotID = invType;
	record->sheathID = sheath;

	g_itemDB->b_base_02.m_recordsById[itemID] = record;
	g_itemDB->b_base_01.m_numRecords++;
}
