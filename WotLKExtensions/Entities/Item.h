#pragma once

#include <SharedDefines.h>
static constexpr int32 ITEM_ALLOCATE_AMOUNT = 0x3FFFFFF;
class Item
{
public:
	static void Apply();
	static void AddItemToDBC(int itemID, int itemClass, int itemSubClass, int displayInfo, int inventorySlot, int materialID = 1, int soundOverride = -1, int sheathID = 0);
	static void RemoveItem(int itemID);
	static bool ItemExists(int itemID);
	static ItemRecord* GetItemRecord(int itemID);
	static void AddItemToMemoryFromCache(const ItemCache* cache);
	static void AddItemRecordFromFields(int itemID, int itemClass, int itemSubClass,
	    int soundOverride, int material, int displayInfo,
	    int invType, int sheath);

private:
	static void PatchItemDBC();
};