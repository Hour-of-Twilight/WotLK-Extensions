#include "Item.h"
#include <Util.h>
#include <ClientDetours.h>
#include <cstring>
#include "Logger.h"

// Server side names
enum ItemFlags2 : uint32
{
	ITEM_FLAG2_PARADOXICAL = 0x10000000,
	ITEM_FLAG2_HEIRLOOM = 0x20000000,
	ITEM_FLAG2_LEGENDARY = 0x40000000,
	ITEM_FLAG2_CORRUPTED = 0x80000000,
};

// Most specific combination first, so an item flagged corrupted and legendary reads as
// Time-Warped Legendary instead of picking whichever single bit came first.
static const struct
{
	uint32 mask;
	const char* key;
} sTooltipLabels[] = {
	{ ITEM_FLAG2_PARADOXICAL | ITEM_FLAG2_CORRUPTED | ITEM_FLAG2_LEGENDARY, "ITEM_PARADOXICAL_TIME_WARPED_LEGENDARY" },
	{ ITEM_FLAG2_PARADOXICAL | ITEM_FLAG2_CORRUPTED, "ITEM_PARADOXICAL_CORRUPTED" },
	{ ITEM_FLAG2_PARADOXICAL | ITEM_FLAG2_LEGENDARY, "ITEM_PARADOXICAL_LEGENDARY" },
	{ ITEM_FLAG2_PARADOXICAL | ITEM_FLAG2_HEIRLOOM, "ITEM_PARADOXICAL_HEIRLOOM" },
	{ ITEM_FLAG2_CORRUPTED | ITEM_FLAG2_LEGENDARY, "ITEM_TIME_WARPED_LEGENDARY" },
	{ ITEM_FLAG2_CORRUPTED, "ITEM_CORRUPTED" },
	{ ITEM_FLAG2_PARADOXICAL, "ITEM_PARADOXICAL" },
	{ ITEM_FLAG2_LEGENDARY, "ITEM_LEGENDARY" },
	{ ITEM_FLAG2_HEIRLOOM, "ITEM_HEIRLOOM" },
};

static const char* GetHeroicQualityLabelKey(const ItemCache* item, const char* defaultKey)
{
	if (!item)
		return defaultKey;

	const uint32 flags2 = static_cast<uint32>(item->FlagsAndFaction[1]); // +0x1C

	for (const auto& label : sTooltipLabels)
		if ((flags2 & label.mask) == label.mask)
			return label.key;

	return defaultKey;
}

static const char* __cdecl GetHeroicEpicLabelKey(const ItemCache* item)
{
	return GetHeroicQualityLabelKey(item, "ITEM_HEROIC_EPIC");
}

static const char* __cdecl GetHeroicLabelKey(const ItemCache* item)
{
	return GetHeroicQualityLabelKey(item, "ITEM_HEROIC");
}

__declspec(naked) static void CGTooltip__SetItem_HeroicEpicLabel()
{
	__asm {
        push eax
        call GetHeroicEpicLabelKey
        add  esp, 4
        push eax
        push 0x00627B4A
        ret
	}
}

__declspec(naked) static void CGTooltip__SetItem_HeroicLabel()
{
	__asm {
        push ecx
        call GetHeroicLabelKey
        add  esp, 4
        push eax
        push 0x00627BB1
        ret
	}
}

void Item::Apply()
{
	PatchHeroicQualityTooltipLabel();
}

CLIENT_DETOUR(ClientDBInitialize, 0x00634E00, __cdecl, void, (void))
{
	ClientDBInitialize();
	Item::PatchItemDBC();
}

void Item::PatchHeroicQualityTooltipLabel()
{
	PatchTooltipLabelPush(0x00627B45, &CGTooltip__SetItem_HeroicEpicLabel);
	PatchTooltipLabelPush(0x00627BAC, &CGTooltip__SetItem_HeroicLabel);
}

void Item::PatchTooltipLabelPush(uint32_t pushSite, void* stub)
{
	if (*reinterpret_cast<uint8_t*>(pushSite) != 0x68) // push imm32
	{
		LOG_DEBUG << "Unexpected bytes at tooltip label site " << pushSite << ", skipping patch.";
		return;
	}

	uint8_t patch[5];
	int32_t rel = static_cast<int32_t>(reinterpret_cast<uintptr_t>(stub) - (pushSite + 5));
	patch[0] = 0xE9; // jmp rel32
	std::memcpy(&patch[1], &rel, sizeof(rel));
	Util::OverwriteBytesAtAddress(pushSite, patch, sizeof(patch));
}

void Item::PatchItemDBC()
{
	if (!g_itemDB->b_base_01.m_loaded)
	{
		LOG_ERROR << "Item dbc is not loaded, skipping item record table expansion.";
		return;
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
