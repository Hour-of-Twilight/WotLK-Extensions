#pragma once

#include <Defines.h>

#include <SharedDefines.h>
static constexpr int32 ITEM_ALLOCATE_AMOUNT = 0xFFFFFF;
class Item
{
public:
	static void Apply();
	static void PatchItemDBC();
	static void AddItemToDBC(int itemID, int itemClass, int itemSubClass, int displayInfo, int inventorySlot, int materialID = 1, int soundOverride = -1, int sheathID = 0);
	static void RemoveItem(int itemID);
	static bool ItemExists(int itemID);
	static ItemRecord* GetItemRecord(int itemID);
	static void AddItemToMemoryFromCache(const ItemCache* cache);
	static void AddItemRecordFromFields(int itemID, int itemClass, int itemSubClass,
	    int soundOverride, int material, int displayInfo,
	    int invType, int sheath);

private:
	static void PatchHeroicQualityTooltipLabel();
	static void PatchOnSocketSpellTriggerLabel();
	static void PatchTooltipLabelPush(uint32 pushSite, void* stub);
	static void StoreRecord(int itemID, int itemClass, int itemSubClass, int soundOverride,
	    int materialID, int displayInfo, int inventorySlot, int sheathID);
};